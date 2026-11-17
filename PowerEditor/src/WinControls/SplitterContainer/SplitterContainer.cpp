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

#include "SplitterContainer.h"

#include <QSplitter>
#include <QSplitterHandle>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QMenu>
#include <QAction>

#include "Parameters.h"
#include "localization.h"

namespace
{
	// The divider's own menu to turn the two panes, and a double click to give them the same size
	class DynamicSplitterHandle : public QSplitterHandle
	{
	public:
		DynamicSplitterHandle(Qt::Orientation orientation, QSplitter* parent, SplitterContainer* container)
			: QSplitterHandle(orientation, parent), _container(container) {}

	protected:
		void contextMenuEvent(QContextMenuEvent* event) override
		{
			NativeLangSpeaker* speaker = NppParameters::getInstance().getNativeLangSpeaker();
			QMenu menu(this);
			QAction* right = menu.addAction(QString::fromStdWString(speaker->getLocalizedStrFromID("splitter-rotate-right", L"Rotate to right")));
			QAction* left = menu.addAction(QString::fromStdWString(speaker->getLocalizedStrFromID("splitter-rotate-left", L"Rotate to left")));
			const QAction* chosen = menu.exec(event->globalPos());
			if (chosen == right || chosen == left)
				_container->rotateTo(chosen == left);
		}

		void mousePressEvent(QMouseEvent* event) override
		{
			_hasPress = true;
			QSplitterHandle::mousePressEvent(event);
		}

		void mouseDoubleClickEvent(QMouseEvent*) override
		{
			_hasPress = true;
			_container->centre();
		}

		// Breeze puts a helper widget over a thin divider once the mouse has been on it, to make it easier to grab. That
		// passes on presses and releases but not double clicks, so a double click is seen here as a release without a press.
		void mouseReleaseEvent(QMouseEvent* event) override
		{
			const bool wasDoubleClickElsewhere = !_hasPress;
			_hasPress = false;
			if (wasDoubleClickElsewhere)
				_container->centre();
			else
				QSplitterHandle::mouseReleaseEvent(event);
		}

	private:
		SplitterContainer* _container;
		bool _hasPress = false;
	};

	class DynamicSplitter : public QSplitter
	{
	public:
		DynamicSplitter(Qt::Orientation orientation, QWidget* parent, SplitterContainer* container)
			: QSplitter(orientation, parent), _container(container) {}

	protected:
		QSplitterHandle* createHandle() override
		{
			return new DynamicSplitterHandle(orientation(), this, _container);
		}

	private:
		SplitterContainer* _container;
	};
}

// Which widget of a Window goes into the splitter. A document tab bar comes with its pane.
void SplitterContainer::create(Window *pWin0, Window *pWin1, int splitterSize, SplitterMode mode, int ratio, bool isVertical)
{
	const Qt::Orientation orientation = isVertical ? Qt::Horizontal : Qt::Vertical;
	QSplitter* bar = (mode == SplitterMode::DYNAMIC) ? new DynamicSplitter(orientation, _hParent, this) : new QSplitter(orientation, _hParent);
	bar->setHandleWidth(splitterSize);
	bar->setChildrenCollapsible(false);
	_hSelf = bar;

	setWin0(pWin0);
	setWin1(pWin1);

	switch (mode)
	{
		case SplitterMode::LEFT_FIX:
			bar->setStretchFactor(0, 0);
			bar->setStretchFactor(1, 1);
			break;
		case SplitterMode::RIGHT_FIX:
			bar->setStretchFactor(0, 1);
			bar->setStretchFactor(1, 0);
			break;
		case SplitterMode::DYNAMIC:
			bar->setStretchFactor(0, 1);
			bar->setStretchFactor(1, 1);
			applyRatio(ratio);
			break;
	}
}

void SplitterContainer::destroy()
{
	delete _hSelf;
	_hSelf = nullptr;
}

QSplitter* SplitterContainer::splitter() const
{
	return static_cast<QSplitter*>(_hSelf);
}

void SplitterContainer::setWin0(Window* pWin)
{
	if (pWin && pWin->paneWidget())
	{
		// The first pane may be away while the second one stays, which then must not be replaced
		const bool isFirstPresent = splitter()->count() > 0 && splitter()->widget(0) != _pane1;
		if (isFirstPresent && splitter()->widget(0) != pWin->paneWidget())
			splitter()->replaceWidget(0, pWin->paneWidget());
		else
			splitter()->insertWidget(0, pWin->paneWidget());
	}
}

void SplitterContainer::setWin1(Window* pWin)
{
	if (pWin && pWin->paneWidget())
	{
		_pane1 = pWin->paneWidget();
		if (splitter()->count() > 1 && splitter()->widget(1) != pWin->paneWidget())
			splitter()->replaceWidget(1, pWin->paneWidget());
		else
			splitter()->addWidget(pWin->paneWidget());
	}
}

bool SplitterContainer::isVertical() const
{
	return splitter()->orientation() == Qt::Horizontal; // a vertical divider between side-by-side windows
}

void SplitterContainer::rotate()
{
	splitter()->setOrientation(isVertical() ? Qt::Vertical : Qt::Horizontal);
}

void SplitterContainer::rotateTo(bool isToLeft)
{
	// The panes change places as needed for them to turn in that direction
	const bool doSwitchPanes = isVertical() ? isToLeft : !isToLeft;
	rotate();
	if (doSwitchPanes && splitter()->count() == 2)
		splitter()->insertWidget(0, splitter()->widget(1));
}

void SplitterContainer::centre()
{
	applyRatio(50);
}

void SplitterContainer::applyRatio(int ratio)
{
	const int total = 1000;
	splitter()->setSizes({total * ratio / 100, total - (total * ratio / 100)});
}
