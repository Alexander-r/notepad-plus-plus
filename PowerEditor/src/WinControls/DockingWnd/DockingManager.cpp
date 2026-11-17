// This file is part of Notepad++ project
// Copyright (c) 2021 adzm / Adam D. Walling

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

#include "DockingManager.h"

#include <QCloseEvent>
#include <QDockWidget>
#include <QHideEvent>
#include <QIcon>
#include <QMainWindow>
#include <QShowEvent>
#include <QTabBar>

#include <QTimer>

#include <algorithm>
#include <memory>

#include "NppMessages.h"

namespace
{
	constexpr int floatingContainerIndex = DOCKCONT_MAX;

	Qt::DockWidgetArea areaOf(int iCont)
	{
		switch (iCont)
		{
			case CONT_LEFT: return Qt::LeftDockWidgetArea;
			case CONT_RIGHT: return Qt::RightDockWidgetArea;
			case CONT_TOP: return Qt::TopDockWidgetArea;
			case CONT_BOTTOM: return Qt::BottomDockWidgetArea;
			default: return Qt::NoDockWidgetArea;
		}
	}

	class PanelDock final : public QDockWidget
	{
	public:
		PanelDock(DockingManager* manager, DockedPanel* panel, QWidget* parent) : QDockWidget(parent), _manager(manager), _panel(panel) {}

		// The dock outlives the manager, as it belongs to the main window, and is then on its own
		void detach()
		{
			_manager = nullptr;
			_panel = nullptr;
		}

	protected:
		void closeEvent(QCloseEvent* event) override
		{
			// The panel is asked first, as on Windows, and stays open when it answers DMN_CLOSE with nonzero
			if (_manager && _manager->panelClosed(_panel))
			{
				event->ignore();
				return;
			}
			QDockWidget::closeEvent(event);
		}

		void showEvent(QShowEvent* event) override
		{
			QDockWidget::showEvent(event);
			if (_manager)
				_manager->panelShown(_panel);
		}

		void hideEvent(QHideEvent* event) override
		{
			if (_manager)
				_manager->panelHidden(_panel);
			QDockWidget::hideEvent(event);
		}

	private:
		DockingManager* _manager;
		DockedPanel* _panel;
	};

	QString titleOf(const DockedWidgetData& data)
	{
		QString title = QString::fromWCharArray(data.pszName ? data.pszName : L"");
		if ((data.uMask & DWS_ADDINFO) && data.pszAddInfo && data.pszAddInfo[0])
			title += QStringLiteral(" - ") + QString::fromWCharArray(data.pszAddInfo);
		return title;
	}
}

std::vector<DockedWidgetData*> DockingCont::getDataOfAllTb() const
{
	std::vector<DockedWidgetData*> result;
	for (DockedPanel* panel : _panels)
		result.push_back(&panel->_data);
	return result;
}

std::vector<DockedWidgetData*> DockingCont::getDataOfVisTb() const
{
	std::vector<DockedWidgetData*> result;
	for (DockedPanel* panel : _panels)
	{
		if (panel->_dock->isVisible())
			result.push_back(&panel->_data);
	}
	return result;
}

bool DockingCont::isTbVis(const DockedWidgetData* data) const
{
	for (const DockedPanel* panel : _panels)
	{
		if (&panel->_data == data)
			return panel->_dock->isVisible();
	}
	return false;
}

DockedWidgetData* DockingCont::findDockedWidgetByWnd(HWND hClient) const
{
	for (DockedPanel* panel : _panels)
	{
		if (panel->_data.hClient == hClient)
			return &panel->_data;
	}
	return nullptr;
}

int DockingCont::getActiveTb() const
{
	// The panel on top of its tab group
	for (size_t i = 0; i < _panels.size(); ++i)
	{
		if (_panels[i]->_dock->isVisible() && !_panels[i]->_dock->visibleRegion().isEmpty())
			return static_cast<int>(i);
	}
	return _panels.empty() ? -1 : 0;
}

bool DockingCont::isVisible() const
{
	return std::any_of(_panels.begin(), _panels.end(), [](const DockedPanel* panel) { return panel->_dock->isVisible(); });
}

void DockingCont::getWindowRect(RECT& rc) const
{
	rc = {};
	if (!_panels.empty())
	{
		const QRect frame = _panels.front()->_dock->frameGeometry();
		rc.left = frame.left();
		rc.top = frame.top();
		rc.right = frame.left() + frame.width();
		rc.bottom = frame.top() + frame.height();
	}
}

void DockingCont::doDialog(bool toShow) const
{
	for (const DockedPanel* panel : _panels)
		panel->_dock->setVisible(toShow);
}

DockingManager::DockingManager() = default;

DockingManager::~DockingManager()
{
	detachPanels();
}

void DockingManager::detachPanels()
{
	for (const auto& panel : _panels)
		static_cast<PanelDock*>(panel->_dock)->detach();
}

void DockingManager::init(HINSTANCE hInst, QMainWindow* mainWindow)
{
	Window::init(hInst, mainWindow);
	_mainWindow = mainWindow;
	_hSelf = mainWindow;
	mainWindow->setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks | QMainWindow::AllowNestedDocks | QMainWindow::GroupedDragging);
	mainWindow->setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::South);
}

void DockingManager::destroy()
{
	_containerPointers.clear();
	_containers.clear();
	detachPanels();
	_panels.clear(); // the dock widgets belong to the main window
	_hSelf = nullptr;
}

DockedPanel* DockingManager::findPanel(HWND hClient) const
{
	for (const auto& panel : _panels)
	{
		if (panel->_data.hClient == hClient)
			return panel.get();
	}
	return nullptr;
}

int DockingManager::containerOf(const DockedPanel* panel) const
{
	if (panel->_dock->isFloating())
		return floatingContainerIndex;

	switch (_mainWindow->dockWidgetArea(panel->_dock))
	{
		case Qt::LeftDockWidgetArea: return CONT_LEFT;
		case Qt::RightDockWidgetArea: return CONT_RIGHT;
		case Qt::TopDockWidgetArea: return CONT_TOP;
		case Qt::BottomDockWidgetArea: return CONT_BOTTOM;
		default: return floatingContainerIndex;
	}
}

std::vector<DockingCont*>& DockingManager::getContainerInfo()
{
	_containers.clear();
	_containerPointers.clear();
	for (int i = 0; i <= floatingContainerIndex; ++i)
	{
		_containers.push_back(std::make_unique<DockingCont>(i));
		_containerPointers.push_back(_containers.back().get());
	}
	for (const auto& panel : _panels)
		_containers[containerOf(panel.get())]->_panels.push_back(panel.get());
	return _containerPointers;
}

void DockingManager::place(DockedPanel* panel, int iCont)
{
	const Qt::DockWidgetArea area = areaOf(iCont);
	if (area == Qt::NoDockWidgetArea)
	{
		// A floating panel, where it was last
		_mainWindow->addDockWidget(Qt::LeftDockWidgetArea, panel->_dock);
		panel->_dock->setFloating(true);
		const RECT& rc = panel->_data.rcFloat;
		if (rc.right > rc.left && rc.bottom > rc.top)
			panel->_dock->setGeometry(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
		return;
	}

	// Panels of the same area share it as tabs
	QDockWidget* sibling = nullptr;
	for (const auto& other : _panels)
	{
		if (other.get() != panel && !other->_dock->isFloating() && _mainWindow->dockWidgetArea(other->_dock) == area)
		{
			sibling = other->_dock;
			break;
		}
	}
	_mainWindow->addDockWidget(area, panel->_dock);
	if (sibling)
		_mainWindow->tabifyDockWidget(sibling, panel->_dock);
}

void DockingManager::createDockableDlg(DockedWidgetData data, int iCont, bool isVisible)
{
	auto panel = std::make_unique<DockedPanel>();
	panel->_data = data;
	auto* dock = new PanelDock(this, panel.get(), _mainWindow);
	panel->_dock = dock;

	dock->setObjectName(QString::fromWCharArray(data.pszModuleName ? data.pszModuleName : L"") + u'#' + QString::number(data.dlgID));
	dock->setWindowTitle(titleOf(data));
	if ((data.uMask & DWS_ICONTAB) && data.hIconTab)
		dock->setWindowIcon(*data.hIconTab);
	dock->setWidget(data.hClient);

	// A panel that comes into view or goes out of it hears about it, as it may have to catch up. The signal comes
	// before the layout has placed the panel, so the answer is only known afterwards.
	HWND client = data.hClient;
	auto wasVisible = std::make_shared<bool>(false);
	QObject::connect(dock, &QDockWidget::visibilityChanged, dock, [dock, client, wasVisible]()
	{
		QTimer::singleShot(0, dock, [dock, client, wasVisible]()
		{
			const bool isVisible = dock->isVisible() && !dock->visibleRegion().isEmpty();
			if (isVisible == *wasVisible)
				return;

			*wasVisible = isVisible;
			NMHDR nmhdr{};
			nmhdr.code = isVisible ? DMN_SWITCHIN : DMN_SWITCHOFF;
			nmhdr.hwndFrom = client;
			::SendMessage(client, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
		});
	});

	// The panel is told when it is torn off or docked again, as the Document Map then has to redraw its view
	QObject::connect(dock, &QDockWidget::topLevelChanged, dock, [this, dock, client](bool isFloating)
	{
		const auto notifyPanel = [client](UINT code)
		{
			NMHDR nmhdr{};
			nmhdr.code = code;
			nmhdr.hwndFrom = client;
			::SendMessage(client, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
		};

		if (isFloating)
		{
			notifyPanel(DMN_FLOAT);
			notifyPanel(DMN_FLOATDROPPED);
			return;
		}

		int position = CONT_LEFT;
		switch (_mainWindow ? _mainWindow->dockWidgetArea(dock) : Qt::LeftDockWidgetArea)
		{
			case Qt::RightDockWidgetArea: position = CONT_RIGHT; break;
			case Qt::TopDockWidgetArea: position = CONT_TOP; break;
			case Qt::BottomDockWidgetArea: position = CONT_BOTTOM; break;
			default: break;
		}
		notifyPanel(DMN_DOCK | (static_cast<UINT>(position) << 16)); // the position in the high word, as on Windows
	});

	// Without a position from the last session, the panel goes where it asks to by default
	if (iCont < 0 || iCont >= DOCKCONT_MAX)
	{
		if (iCont < 0 && !(data.uMask & DWS_DF_FLOATING))
			iCont = static_cast<int>((data.uMask & 0x30000000) >> 28);
		else
			iCont = floatingContainerIndex;
	}

	DockedPanel* placed = panel.get();
	_panels.push_back(std::move(panel));
	place(placed, iCont);
	dock->setVisible(isVisible);
}

void DockingManager::updateContainerInfo(HWND hClient)
{
	if (const DockedPanel* panel = findPanel(hClient))
	{
		panel->_dock->setWindowTitle(titleOf(panel->_data));
		if ((panel->_data.uMask & DWS_ICONTAB) && panel->_data.hIconTab)
			panel->_dock->setWindowIcon(*panel->_data.hIconTab);
	}
}

void DockingManager::setActiveTab(int iCont, int iItem)
{
	std::vector<DockingCont*>& containers = getContainerInfo();
	if (iCont >= 0 && static_cast<size_t>(iCont) < containers.size())
	{
		const auto& panels = containers[iCont]->_panels;
		if (iItem >= 0 && static_cast<size_t>(iItem) < panels.size())
			panels[iItem]->_dock->raise();
	}
}

void DockingManager::showDockableDlg(HWND hDlg, bool view)
{
	if (const DockedPanel* panel = findPanel(hDlg))
	{
		panel->_dock->setVisible(view);
		if (view)
			panel->_dock->raise();
	}
}

void DockingManager::showDockableDlg(const wchar_t* pszName, bool view)
{
	for (const auto& panel : _panels)
	{
		if (panel->_data.pszName && std::wcscmp(panel->_data.pszName, pszName) == 0)
		{
			panel->_dock->setVisible(view);
			if (view)
				panel->_dock->raise();
			return;
		}
	}
}

void DockingManager::showFloatingContainers(bool show)
{
	for (const auto& panel : _panels)
	{
		if (panel->_dock->isFloating())
			panel->_dock->setVisible(show);
	}
}

void DockingManager::getDockInfo(tDockMgr *pDockData) const
{
	*pDockData = {};
	pDockData->hWnd = _mainWindow;
}

int DockingManager::sizeIn(const DockedPanel* panel, int iCont) const
{
	return (iCont == CONT_LEFT || iCont == CONT_RIGHT) ? panel->_dock->width() : panel->_dock->height();
}

int DockingManager::getDockedContSize(int iCont) const
{
	if (iCont < 0 || iCont >= DOCKCONT_MAX)
		return 0;

	for (const auto& panel : _panels)
	{
		if (containerOf(panel.get()) == iCont && panel->_dock->isVisible())
			return sizeIn(panel.get(), iCont);
	}
	return _contSizes[iCont];
}

void DockingManager::setDockedContSize(int iCont, int iSize)
{
	if (iCont < 0 || iCont >= DOCKCONT_MAX)
		return;

	_contSizes[iCont] = iSize;
	const Qt::Orientation orientation = (iCont == CONT_LEFT || iCont == CONT_RIGHT) ? Qt::Horizontal : Qt::Vertical;
	for (const auto& panel : _panels)
	{
		if (containerOf(panel.get()) == iCont)
		{
			_mainWindow->resizeDocks({panel->_dock}, {iSize}, orientation);
			return;
		}
	}
}

void DockingManager::panelShown(const DockedPanel* panel)
{
	// A panel that comes into an area with no other panel in view takes the size that the area had
	if (!_mainWindow)
		return;
	const int iCont = containerOf(panel);
	if (iCont >= DOCKCONT_MAX || _contSizes[iCont] <= 0)
		return;

	for (const auto& other : _panels)
	{
		if (other.get() != panel && containerOf(other.get()) == iCont && other->_dock->isVisible())
			return;
	}
	const Qt::Orientation orientation = (iCont == CONT_LEFT || iCont == CONT_RIGHT) ? Qt::Horizontal : Qt::Vertical;
	_mainWindow->resizeDocks({panel->_dock}, {_contSizes[iCont]}, orientation);
}

void DockingManager::panelHidden(const DockedPanel* panel)
{
	// The area keeps the size it had, unless the panel was never laid out in it
	if (!_mainWindow || !_mainWindow->isVisible())
		return;
	const int iCont = containerOf(panel);
	if (iCont >= DOCKCONT_MAX)
		return;

	const int size = sizeIn(panel, iCont);
	const int minimum = (iCont == CONT_LEFT || iCont == CONT_RIGHT) ? panel->_dock->minimumSizeHint().width() : panel->_dock->minimumSizeHint().height();
	if (size > minimum)
		_contSizes[iCont] = size;
}

bool DockingManager::panelClosed(DockedPanel* panel)
{
	NMHDR nmhdr{};
	nmhdr.code = DMN_CLOSE;
	nmhdr.hwndFrom = _mainWindow;
	return ::SendMessage(panel->_data.hClient, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr)) != 0;
}
