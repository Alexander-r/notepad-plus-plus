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


#include "VerticalFileSwitcher.h"

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QMouseEvent>
#include <QShortcut>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <string>

#include "Buffer.h"
#include "DockingDlgInterface.h"
#include "Notepad_plus_msgs.h"
#include "NppConstants.h"
#include "Parameters.h"
#include "localization.h"
#include "menuCmdID.h"
#include "resource.h"

using namespace std;

class FileSwitcherEvents : public QObject
{
public:
	FileSwitcherEvents(VerticalFileSwitcher* owner, QObject* parent) : QObject(parent), _owner(owner) {}

	bool eventFilter(QObject* watched, QEvent* event) override
	{
		if (_owner->handleViewportEvent(event))
			return true;
		return QObject::eventFilter(watched, event);
	}

private:
	VerticalFileSwitcher* _owner;
};

VerticalFileSwitcher::VerticalFileSwitcher() : DockingDlgInterface(IDD_DOCLIST) {}
VerticalFileSwitcher::~VerticalFileSwitcher() = default;

void VerticalFileSwitcher::create(DockedWidgetData* data, bool isRTL)
{
	DockingDlgInterface::create(data, isRTL);

	_fileListView.init(_hInst, _hSelf, _icons);
	_fileListView.initList();

	auto* layout = new QVBoxLayout(_hSelf);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_fileListView.getHSelf());

	QTreeWidget* list = _fileListView.tree();
	list->viewport()->installEventFilter(new FileSwitcherEvents(this, _hSelf));

	QObject::connect(list, &QTreeWidget::itemClicked, _hSelf, [this](QTreeWidgetItem* item)
	{
		if (QApplication::keyboardModifiers() & (Qt::ControlModifier | Qt::ShiftModifier))
			return;

		BufferID bufferID = nullptr;
		int view = 0;
		if (VerticalFileSwitcherListView::infoOf(item, bufferID, view))
			activateDoc(bufferID, view);
	});

	for (const int key : { Qt::Key_Return, Qt::Key_Enter })
	{
		auto* shortcut = new QShortcut(QKeySequence(key), list);
		shortcut->setContext(Qt::WidgetShortcut);
		QObject::connect(shortcut, &QShortcut::activated, _hSelf, [this, list]()
		{
			BufferID bufferID = nullptr;
			int view = 0;
			if (VerticalFileSwitcherListView::infoOf(list->currentItem(), bufferID, view))
				activateDoc(bufferID, view);
		});
	}

	QHeaderView* header = list->header();
	QObject::connect(header, &QHeaderView::sectionClicked, _hSelf, [this](int column) { onHeaderClicked(column); });
	QObject::connect(header, &QHeaderView::sectionResized, _hSelf, [this](int column, int, int width) { storeColumnWidth(column, width); });
	QObject::connect(header, &QWidget::customContextMenuRequested, _hSelf, [this, header](const QPoint& position)
	{
		showColumnsMenu(header->mapToGlobal(position));
	});
	QObject::connect(list, &QWidget::customContextMenuRequested, _hSelf, [this](const QPoint& position) { onRightClick(position); });

	// The menu of the columns
	NativeLangSpeaker* pNativeSpeaker = NppParameters::getInstance().getNativeLangSpeaker();
	NppGUI& nppGUI = NppParameters::getInstance().getNppGUI();

	_hGlobalMenu = std::make_unique<QMenu>();
	const auto addOption = [this](const std::wstring& text, bool& isOff)
	{
		QAction* action = _hGlobalMenu->addAction(QString::fromStdWString(text));
		action->setCheckable(true);
		action->setChecked(!isOff);
		QObject::connect(action, &QAction::toggled, _hSelf, [this, &isOff](bool isOn)
		{
			isOff = !isOn;
			reload();
		});
	};
	addOption(pNativeSpeaker->getAttrNameStr(L"Ext.", FS_ROOTNODE, FS_CLMNEXT), nppGUI._fileSwitcherWithoutExtColumn);
	addOption(pNativeSpeaker->getAttrNameStr(L"Path", FS_ROOTNODE, FS_CLMNPATH), nppGUI._fileSwitcherWithoutPathColumn);
	_hGlobalMenu->addSeparator();
	addOption(pNativeSpeaker->getAttrNameStr(L"Group by View", FS_ROOTNODE, FS_LVGROUPS), nppGUI._fileSwitcherDisableListViewGroups);
}

void VerticalFileSwitcher::onDockingNotify(const NMHDR& notification)
{
	if (LOWORD(notification.code) == DMN_CLOSE)
		::SendMessage(_hParent, WM_COMMAND, IDM_VIEW_DOCLIST, 0);
	else
		DockingDlgInterface::onDockingNotify(notification);
}

bool VerticalFileSwitcher::handleViewportEvent(QEvent* event)
{
	QTreeWidget* list = _fileListView.tree();

	if (event->type() == QEvent::MouseButtonRelease)
	{
		const auto* mouse = static_cast<QMouseEvent*>(event);
		if (mouse->button() == Qt::MiddleButton)
		{
			// Close the document under the pointer
			BufferID bufferID = nullptr;
			int view = 0;
			if (VerticalFileSwitcherListView::infoOf(list->itemAt(mouse->position().toPoint()), bufferID, view))
				closeDoc(bufferID);
			return true;
		}
	}
	else if (event->type() == QEvent::MouseButtonDblClick)
	{
		const auto* mouse = static_cast<QMouseEvent*>(event);
		if (mouse->button() == Qt::LeftButton && !list->itemAt(mouse->position().toPoint()))
		{
			::SendMessage(_hParent, WM_COMMAND, IDM_FILE_NEW, 0);
			return true;
		}
	}
	return false;
}

void VerticalFileSwitcher::onRightClick(const QPoint& position)
{
	QTreeWidget* list = _fileListView.tree();
	const int nbSelected = nbSelectedFiles();

	if (nbSelected == 1)
	{
		BufferID bufferID = nullptr;
		int view = 0;
		if (VerticalFileSwitcherListView::infoOf(list->itemAt(position), bufferID, view))
			activateDoc(bufferID, view);
	}

	if (nbSelected >= 1)
	{
		// Redirect NM_RCLICK message to Notepad_plus handle
		NMHDR nmhdr{};
		nmhdr.code = NM_RCLICK;
		nmhdr.hwndFrom = _hSelf;
		nmhdr.idFrom = 0;
		::SendMessage(_hParent, WM_NOTIFY, nmhdr.idFrom, reinterpret_cast<LPARAM>(&nmhdr));
	}
	else
	{
		showColumnsMenu(list->viewport()->mapToGlobal(position));
	}
}

void VerticalFileSwitcher::showColumnsMenu(const QPoint& globalPosition)
{
	_hGlobalMenu->exec(globalPosition);
}

void VerticalFileSwitcher::storeColumnWidth(int column, int width)
{
	if (column == 0)
		return;

	NppGUI& nppGUI = NppParameters::getInstance().getNppGUI();
	const bool isExtColumn = !nppGUI._fileSwitcherWithoutExtColumn;
	if (isExtColumn && column == 1)
		nppGUI._fileSwitcherExtWidth = width;
	else
		nppGUI._fileSwitcherPathWidth = width;
}

void VerticalFileSwitcher::onHeaderClicked(int column)
{
	// Each click goes on from ascending to descending to the order of the tabs
	if (_lastSortingColumn != column || _lastSortingDirection == SORT_DIRECTION_NONE)
		_lastSortingDirection = SORT_DIRECTION_UP;
	else if (_lastSortingDirection == SORT_DIRECTION_UP)
		_lastSortingDirection = SORT_DIRECTION_DOWN;
	else
		_lastSortingDirection = SORT_DIRECTION_NONE;

	_lastSortingColumn = column;

	if (_lastSortingDirection == SORT_DIRECTION_NONE)
		_fileListView.reload();

	startColumnSort();
}

void VerticalFileSwitcher::startColumnSort()
{
	// reset sorting if exts column was just disabled
	if (_lastSortingColumn >= _fileListView.tree()->columnCount())
	{
		_lastSortingColumn = 0;
		_lastSortingDirection = SORT_DIRECTION_NONE;
	}

	_fileListView.sortBy(_lastSortingColumn, _lastSortingDirection);
}

void VerticalFileSwitcher::display(bool toShow) const
{
	DockingDlgInterface::display(toShow);
	_fileListView.ensureVisibleCurrentItem();	// without this call the current item may stay above visible area after the panel was hidden
}

void VerticalFileSwitcher::activateDoc(BufferID bufferID, int view) const
{
	auto currentView = ::SendMessage(_hParent, NPPM_GETCURRENTVIEW, 0, 0);
	BufferID currentBufID = reinterpret_cast<BufferID>(::SendMessage(_hParent, NPPM_GETCURRENTBUFFERID, 0, 0));

	if (bufferID == currentBufID && view == currentView)
		return;

	int docPosInfo = static_cast<int32_t>(::SendMessage(_hParent, NPPM_GETPOSFROMBUFFERID, reinterpret_cast<WPARAM>(bufferID), view));
	int view2set = docPosInfo >> 30;
	int index2Switch = (docPosInfo << 2) >> 2;

	::SendMessage(_hParent, NPPM_ACTIVATEDOC, view2set, index2Switch);
}

void VerticalFileSwitcher::closeDoc(BufferID bufferID) const
{
	int docPosInfo = static_cast<int32_t>(::SendMessage(_hParent, NPPM_GETPOSFROMBUFFERID, reinterpret_cast<WPARAM>(bufferID), 0));
	int view2set = docPosInfo >> 30;
	int index2Switch = (docPosInfo << 2) >> 2;

	::SendMessage(_hParent, NPPM_INTERNAL_CLOSEDOC, view2set, index2Switch);
}
