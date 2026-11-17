// This file is part of Notepad++ project
// Copyright (C)2021 Don HO <don.h@free.fr>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.


#include "TabBar.h"
#include "resource.h"
#include "NppResources.h"
#include "NppDarkMode.h"

#include <QPalette>
#include <QWheelEvent>
#include <QToolButton>
#include <QContextMenuEvent>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QAbstractButton>
#include <QApplication>
#include <QPainter>
#include <QPaintEvent>
#include <QStyle>
#include <QStyleOptionTab>
#include <QStylePainter>

#include <algorithm>

#include "Parameters.h"

COLORREF TabBarPlus::_colours[5] = {RGB(0, 0, 0), RGB(250, 170, 60), RGB(250, 210, 150), RGB(128, 128, 128), RGB(192, 192, 192)};
std::vector<TabBarPlus*> TabBarPlus::_instances;

namespace
{
	QColor toQColor(COLORREF c)
	{
		return QColor(GetRValue(c), GetGValue(c), GetBValue(c));
	}

	constexpr char individualColourProperty[] = "nppColourId";

	// The button on a tab that pins its document. It finds out its tab when asked, as tabs move.
	class PinButton final : public QAbstractButton
	{
	public:
		PinButton(TabBarPlus* owner, QTabBar* bar) : QAbstractButton(bar), _owner(owner), _bar(bar)
		{
			setFixedSize(16, 16);
			setFocusPolicy(Qt::NoFocus);
			setCursor(Qt::ArrowCursor);
			setAttribute(Qt::WA_Hover);
			QObject::connect(this, &QAbstractButton::clicked, this, [this]()
			{
				const int tab = tabIndex();
				if (tab != -1)
					_owner->notify(TCN_TABPINNED, tab);
			});
		}

	protected:
		void paintEvent(QPaintEvent*) override
		{
			const int tab = tabIndex();
			const bool isPinned = tab != -1 && _owner->isTabPinned(tab);
			const bool isDark = NppDarkMode::usesDarkWidgets();
			// Hovering shows what a click would do
			const bool showsPinned = underMouse() ? !isPinned : isPinned;
			const int id = showsPinned ? (isDark ? IDR_PINTAB_PINNED_DM : IDR_PINTAB_PINNED) : (isDark ? IDR_PINTAB_DM : IDR_PINTAB);
			if (const QIcon* icon = nppLoadIcon(id))
			{
				QPainter painter(this);
				icon->paint(&painter, rect());
			}
		}

	private:
		TabBarPlus* _owner;
		QTabBar* _bar;

		int tabIndex() const
		{
			return _bar->tabAt(mapToParent(rect().center()));
		}
	};

	// A QTabBar reporting its events as the notifications the application logic handles
	class DocumentTabBar final : public QTabBar
	{
	public:
		DocumentTabBar(TabBarPlus* owner, QWidget* parent) : QTabBar(parent), _owner(owner)
		{
			setMouseTracking(true);
			setExpanding(false);
			setUsesScrollButtons(true);
			setDocumentMode(true);
			setElideMode(Qt::ElideNone);
		}

	protected:
		void mousePressEvent(QMouseEvent* event) override
		{
			const int tab = tabAt(event->position().toPoint());
			// As on Win32: the right click activates nothing by itself. Its menu comes with the context menu event.
			if (event->button() == Qt::RightButton)
				return;
			if (event->button() == Qt::MiddleButton)
			{
				_middlePressedTab = tab;
				return;
			}
			if (event->button() == Qt::LeftButton)
			{
				_pressedTab = tab;
				_owner->notify(NM_CLICK, tab);
			}
			QTabBar::mousePressEvent(event);
		}

		void contextMenuEvent(QContextMenuEvent* event) override
		{
			const int tab = tabAt(event->pos());
			if (tab != -1)
				_owner->notify(NM_RCLICK, tab);
			event->accept(); // this bar's, also next to the tabs
		}

		void mouseReleaseEvent(QMouseEvent* event) override
		{
			if (event->button() == Qt::MiddleButton)
			{
				const int tab = tabAt(event->position().toPoint());
				if (tab != -1 && tab == _middlePressedTab)
					_owner->notify(TCN_TABDELETE, tab);
				_middlePressedTab = -1;
				return;
			}

			QTabBar::mouseReleaseEvent(event);
			if (_movingHighlight)
				_movingHighlight->hide();

			if (event->button() == Qt::LeftButton && _isDragging)
			{
				_isDragging = false;
				_owner->setDragPoint(event->globalPosition().toPoint());
				const bool isOutside = !rect().contains(event->position().toPoint());
				_owner->notify(isOutside ? TCN_TABDROPPEDOUTSIDE : TCN_TABDROPPED, currentIndex());
			}
			_pressedTab = -1;
		}

		void mouseMoveEvent(QMouseEvent* event) override
		{
			if ((event->buttons() & Qt::LeftButton) && _pressedTab != -1 && (NppParameters::getInstance().getNppGUI()._tabStatus & TAB_DRAGNDROP))
				_isDragging = true;

			const int hovered = tabAt(event->position().toPoint());
			if (hovered != _hoveredTab)
			{
				if (_hoveredTab != -1 && hovered != -1)
					_owner->notify(TCN_MOUSEHOVERSWITCHING, hovered);
				else if (hovered != -1)
					_owner->notify(TCN_MOUSEHOVERING, hovered);
				else
					_owner->notify(TCN_MOUSELEAVING, _hoveredTab);
				_hoveredTab = hovered;
				syncButtons();
			}
			QTabBar::mouseMoveEvent(event);
			syncMovingHighlight();
		}

		void leaveEvent(QEvent* event) override
		{
			if (_hoveredTab != -1)
			{
				_owner->notify(TCN_MOUSELEAVING, _hoveredTab);
				_hoveredTab = -1;
				syncButtons();
			}
			QTabBar::leaveEvent(event);
		}

		// The wheel scrolls the tabs. With Ctrl it goes through them endlessly, with Shift it moves the tab if they can
		// be dragged and otherwise goes through them up to the last one, and with both it goes to the first or last one.
		void wheelEvent(QWheelEvent* event) override
		{
			const int delta = event->angleDelta().y() != 0 ? event->angleDelta().y() : event->angleDelta().x();
			if (delta == 0 || count() == 0)
				return;

			const bool isForward = delta < 0;
			const int last = count() - 1;
			const bool hasControl = event->modifiers() & Qt::ControlModifier;
			const bool hasShift = event->modifiers() & Qt::ShiftModifier;
			const bool canDrag = NppParameters::getInstance().getNppGUI()._tabStatus & TAB_DRAGNDROP;
			event->accept();

			if (hasControl && hasShift)
			{
				setCurrentIndex(isForward ? last : 0);
			}
			else if (hasShift && canDrag)
			{
				const int from = currentIndex();
				int to = from + (isForward ? 1 : -1);
				if (to < 0)
					to = last;
				else if (to > last)
					to = 0;
				if (to != from)
					moveTab(from, to);
			}
			else if (hasControl || hasShift)
			{
				int to = currentIndex() + (isForward ? 1 : -1);
				if (to < 0)
				{
					if (!hasControl)
						return;
					to = last;
				}
				else if (to > last)
				{
					if (!hasControl)
						return;
					to = 0;
				}
				setCurrentIndex(to);
			}
			else if (auto* scroll = findChild<QToolButton*>(isForward ? QStringLiteral("ScrollRightButton") : QStringLiteral("ScrollLeftButton")))
			{
				if (scroll->isVisible() && scroll->isEnabled())
					scroll->click();
			}
		}

		void mouseDoubleClickEvent(QMouseEvent* event) override
		{
			if (event->button() == Qt::LeftButton)
				_owner->notify(NM_DBLCLK, tabAt(event->position().toPoint()));
			QTabBar::mouseDoubleClickEvent(event);
		}

		void paintEvent(QPaintEvent* event) override
		{
			QTabBar::paintEvent(event);
			syncMovingHighlight(); // Qt places the picture of a dragged tab when it paints

			const int status = NppParameters::getInstance().getNppGUI()._tabStatus;
			const bool isDark = NppDarkMode::usesDarkWidgets();
			const QWidget* focused = QApplication::focusWidget();
			const bool isFocused = focused && parentWidget() && parentWidget()->isAncestorOf(focused);
			const bool isVertical = (shape() == QTabBar::RoundedWest || shape() == QTabBar::RoundedEast);

			const bool isTabMoving = _movingHighlight && _movingHighlight->isVisible();

			QPainter painter(this);

			// The current tab normally covers the edge that the style gives the tab next to it, which shows in the empty
			// place of a dragged tab. The picture of the tab is a widget of its own, so this is only under that.
			if (isTabMoving && currentIndex() != -1)
				painter.fillRect(tabRect(currentIndex()), palette().color(QPalette::Window));

			for (int i = 0; i < count(); ++i)
			{
				const QRect area = tabRect(i);
				if (!area.intersects(event->rect()))
					continue;

				const int individual = _owner->getIndividualTabColourId(i);
				if (i == currentIndex())
				{
					if ((status & TAB_DRAWTOPBAR) && !isTabMoving) // otherwise it goes along with the tab
					{
						const COLORREF bar = (individual != -1)
							? NppParameters::getInstance().getIndividualTabColor(individual, isDark, isFocused)
							: TabBarPlus::colour(isFocused ? TabBarPlus::activeFocusedTop : TabBarPlus::activeUnfocusedTop);
						painter.fillRect(isVertical ? QRect(area.left(), area.top(), 4, area.height()) : QRect(area.left(), area.top(), area.width(), 4), toQColor(bar));
					}
				}
				else if (individual != -1 || ((status & TAB_DRAWINACTIVETAB) && !isDark)) // dark mode has its own colours for tabs
				{
					// Over what the style has drawn, so its text stays readable
					const COLORREF tint = (individual != -1)
						? NppParameters::getInstance().getIndividualTabColor(individual, isDark, false)
						: TabBarPlus::colour(TabBarPlus::inactiveBg);
					painter.setCompositionMode(isDark ? QPainter::CompositionMode_Screen : QPainter::CompositionMode_Multiply);
					painter.fillRect(area, toQColor(tint));
					painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
				}
			}
		}

		void tabInserted(int index) override
		{
			QTabBar::tabInserted(index);
			syncButtons();

			// Added while hidden, as when a session comes back, the bar does not tell the layout that it now has a size
			updateGeometry();
		}

		void tabRemoved(int index) override
		{
			QTabBar::tabRemoved(index);
			syncButtons();
		}

		void tabLayoutChange() override
		{
			QTabBar::tabLayoutChange();
			syncButtons();
		}

	private:
		// While a tab is dragged, Qt shows a picture of it in a widget of its own. The active tab's bar goes on top of that.
		void syncMovingHighlight()
		{
			QWidget* moving = nullptr;
			const QList<QWidget*> children = findChildren<QWidget*>(Qt::FindDirectChildrenOnly);
			for (QWidget* child : children)
			{
				if (child != _movingHighlight && child->metaObject() == &QWidget::staticMetaObject && child->isVisible())
					moving = child;
			}

			const int status = NppParameters::getInstance().getNppGUI()._tabStatus;
			if (!moving || !(status & TAB_DRAWTOPBAR))
			{
				if (_movingHighlight)
					_movingHighlight->hide();
				return;
			}

			if (!_movingHighlight)
			{
				_movingHighlight = new QWidget(this);
				_movingHighlight->setObjectName(QStringLiteral("movingTabHighlight"));
				_movingHighlight->setAttribute(Qt::WA_TransparentForMouseEvents);
				_movingHighlight->setAutoFillBackground(true);
			}

			const bool isDark = NppDarkMode::usesDarkWidgets();
			const int individual = _owner->getIndividualTabColourId(currentIndex());
			const COLORREF colour = (individual != -1)
				? NppParameters::getInstance().getIndividualTabColor(individual, isDark, true)
				: TabBarPlus::colour(TabBarPlus::activeFocusedTop);
			QPalette colours = _movingHighlight->palette();
			colours.setColor(QPalette::Window, toQColor(colour));
			_movingHighlight->setPalette(colours);

			const bool isVertical = (shape() == QTabBar::RoundedWest || shape() == QTabBar::RoundedEast);
			const QRect area = moving->geometry();
			_movingHighlight->setGeometry(isVertical ? QRect(area.left(), area.top(), 4, area.height()) : QRect(area.left(), area.top(), area.width(), 4));
			_movingHighlight->show();
			_movingHighlight->raise();
		}

		QWidget* _movingHighlight = nullptr;

	public:
		// Which tabs show their pin and close buttons, by the settings and the state of the tab
		void syncButtons()
		{
			const int status = NppParameters::getInstance().getNppGUI()._tabStatus;
			const bool isPinShown = (status & TAB_PINBUTTON) != 0;
			const bool isOnlyPinned = (status & TAB_SHOWONLYPINNEDBUTTON) != 0;
			const bool isOnInactive = (status & TAB_INACTIVETABSHOWBUTTON) != 0;

			for (int i = 0; i < count(); ++i)
			{
				const bool isActive = (i == currentIndex()) || (i == _hoveredTab) || isOnInactive;

				if (isPinShown && !tabButton(i, QTabBar::LeftSide))
					setTabButton(i, QTabBar::LeftSide, new PinButton(_owner, this));
				else if (!isPinShown && tabButton(i, QTabBar::LeftSide))
					setTabButton(i, QTabBar::LeftSide, nullptr);

				if (QWidget* pin = tabButton(i, QTabBar::LeftSide))
				{
					const bool isPinned = _owner->isTabPinned(i);
					pin->setVisible(isOnlyPinned ? isPinned : (isPinned || isActive));
					pin->update();
				}
				if (QWidget* close = tabButton(i, QTabBar::RightSide))
					close->setVisible(isActive);
			}
		}

	private:
		TabBarPlus* _owner;
		int _pressedTab = -1;
		int _middlePressedTab = -1;
		int _hoveredTab = -1;
		bool _isDragging = false;
	};
}

void TabBar::destroy()
{
	delete _hSelf;
	_hSelf = nullptr;
}

void TabBar::init(HINSTANCE hInst, HWND parent, bool isVertical, [[maybe_unused]] bool isMultiLine)
{
	Window::init(hInst, parent);
	auto* bar = new QTabBar(parent);
	bar->setShape(isVertical ? QTabBar::RoundedWest : QTabBar::RoundedNorth);
	_hSelf = bar;
}

int TabBar::insertAtEnd(const wchar_t *subTabName)
{
	// As on Windows, only the user's action tells about a new selection
	const QSignalBlocker silent(tabs());
	return tabs()->addTab(QString::fromWCharArray(subTabName));
}

void TabBar::activateAt(int index) const
{
	if (getCurrentTabIndex() != index)
	{
		{
			// As on Windows, only the user's action tells about a new selection
			const QSignalBlocker silent(tabs());
			tabs()->setCurrentIndex(index);
		}

		// Without the signal, the active tab's close and pin buttons have to be shown here
		if (auto* bar = dynamic_cast<DocumentTabBar*>(tabs()))
			bar->syncButtons();
	}
}

std::wstring TabBar::getCurrentTitle() const
{
	return tabs()->tabText(tabs()->currentIndex()).toStdWString();
}

void TabBar::deletItemAt(size_t index)
{
	// As on Windows, only the user's action tells about a new selection
	const QSignalBlocker silent(tabs());
	tabs()->removeTab(static_cast<int>(index));
}

void TabBar::deletAllItem()
{
	// As on Windows, only the user's action tells about a new selection
	const QSignalBlocker silent(tabs());
	while (tabs()->count() > 0)
		tabs()->removeTab(0);
}

int TabBar::getNextOrPrevTabIdx(bool isNext) const
{
	const int count = getItemCount();
	if (count == 0)
		return -1;
	return (getCurrentTabIndex() + (isNext ? 1 : count - 1)) % count;
}

TabBarPlus::~TabBarPlus()
{
	_instances.erase(std::remove(_instances.begin(), _instances.end(), this), _instances.end());
}

void TabBarPlus::init(HINSTANCE hInst, HWND parent, bool isVertical, [[maybe_unused]] bool isMultiLine, [[maybe_unused]] unsigned char buttonsStatus)
{
	Window::init(hInst, parent);

	auto* bar = new DocumentTabBar(this, parent);
	bar->setShape(isVertical ? QTabBar::RoundedWest : QTabBar::RoundedNorth);
	_hSelf = bar;

	QObject::connect(bar, &QTabBar::currentChanged, bar, [this, bar](int index)
	{
		bar->syncButtons();
		if (index != -1)
			notify(TCN_SELCHANGE, index);
	});
	QObject::connect(qApp, &QApplication::focusChanged, bar, [bar]() { bar->update(); }); // the bar on the active tab shows the focus
	QObject::connect(bar, &QTabBar::tabCloseRequested, bar, [this](int index)
	{
		notify(TCN_TABDELETE, index);
	});

	_instances.push_back(this);
	applySettings();
}

void TabBarPlus::destroy()
{
	_instances.erase(std::remove(_instances.begin(), _instances.end(), this), _instances.end());
	TabBar::destroy();
}

void TabBarPlus::notify(UINT notifyCode, int tabIndex)
{
	TBHDR nmhdr{};
	nmhdr._hdr.hwndFrom = _hSelf;
	nmhdr._hdr.code = notifyCode;
	nmhdr._hdr.idFrom = reinterpret_cast<UINT_PTR>(this);
	nmhdr._tabOrigin = tabIndex;
	::SendMessage(_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
}

void TabBarPlus::setDragPoint(const QPoint& globalPoint)
{
	_draggingPoint.x = globalPoint.x();
	_draggingPoint.y = globalPoint.y();
}

void TabBarPlus::applySettings()
{
	if (!tabs())
		return;

	const int status = NppParameters::getInstance().getNppGUI()._tabStatus;
	tabs()->setTabsClosable((status & TAB_CLOSEBUTTON) != 0);
	tabs()->setMovable((status & TAB_DRAGNDROP) != 0);
	tabs()->setShape((status & TAB_VERTICAL) ? QTabBar::RoundedWest : QTabBar::RoundedNorth);
	applyTabColours();
	if (auto* bar = dynamic_cast<DocumentTabBar*>(tabs()))
		bar->syncButtons();
}

void TabBarPlus::applyTabColours()
{
	// The colours of the theme are for a light background, as the style draws the tabs with the palette. So dark mode
	// has its own, as in the original, and a dark desktop with Notepad++ in light mode takes those of the palette.
	const QPalette palette = QApplication::palette();
	QColor active = toQColor(_colours[activeText]);

	// An inactive tab is the style's darker shade under the tint of the theme, and a theme's grey must stay readable on it
	const QColor shade = palette.color(QPalette::Window).darker(120);
	const QColor tint = toQColor(_colours[inactiveBg]);
	const COLORREF inactiveBackground = RGB(shade.red() * tint.red() / 255, shade.green() * tint.green() / 255, shade.blue() * tint.blue() / 255);
	QColor inactive = toQColor(NppDarkMode::readableOn(_colours[inactiveText], inactiveBackground));

	if (NppDarkMode::isEnabled())
	{
		active = toQColor(NppDarkMode::getTextColor());
		inactive = toQColor(NppDarkMode::getDarkerTextColor());
	}
	else if (palette.color(QPalette::Window).lightness() < 128)
	{
		active = palette.color(QPalette::WindowText);
		const QColor faint = palette.color(QPalette::Disabled, QPalette::WindowText);
		inactive = toQColor(NppDarkMode::readableOn(RGB(faint.red(), faint.green(), faint.blue()), RGB(palette.color(QPalette::Window).red(), palette.color(QPalette::Window).green(), palette.color(QPalette::Window).blue()), 4.5));
	}

	const int count = tabs()->count();
	for (int i = 0; i < count; ++i)
		tabs()->setTabTextColor(i, (i == tabs()->currentIndex()) ? active : inactive);
}

void TabBarPlus::triggerOwnerDrawTabbar(DPIManagerV2*)
{
	for (TabBarPlus* bar : _instances)
		bar->applySettings();
}

void TabBarPlus::doVertical()
{
	triggerOwnerDrawTabbar(nullptr);
}

void TabBarPlus::doMultiLine()
{
	// QTabBar has one row and scroll buttons: there is no multi-line mode
}

void TabBarPlus::setColour(COLORREF colour2Set, tabColourIndex i, DPIManagerV2*)
{
	_colours[i] = colour2Set;
	for (TabBarPlus* bar : _instances)
		bar->applyTabColours();
}

void TabBarPlus::tabToStart(int index)
{
	const int from = (index == -1) ? getCurrentTabIndex() : index;
	if (from > 0)
	{
		{
			const QSignalBlocker silent(tabs());
			tabs()->moveTab(from, 0);
		}
		notify(TCN_TABDROPPED, 0);
	}
}

void TabBarPlus::tabToEnd(int index)
{
	const int from = (index == -1) ? getCurrentTabIndex() : index;
	const int last = getItemCount() - 1;
	if (from >= 0 && from < last)
	{
		{
			const QSignalBlocker silent(tabs());
			tabs()->moveTab(from, last);
		}
		notify(TCN_TABDROPPED, last);
	}
}

void TabBarPlus::refresh()
{
	if (tabs())
	{
		if (auto* bar = dynamic_cast<DocumentTabBar*>(tabs()))
			bar->syncButtons();
		tabs()->update();
	}
}
