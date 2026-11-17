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

#include "VerticalFileSwitcherListView.h"

#include <QBrush>
#include <QColor>
#include <QHeaderView>
#include <QPalette>
#include <QSignalBlocker>
#include <QTreeWidget>

#include "IconList.h"
#include "Parameters.h"
#include "localization.h"

using namespace std;

namespace
{
	constexpr int bufferRole = Qt::UserRole + 1;
	constexpr int viewRole = Qt::UserRole + 2;
	constexpr int pathRole = Qt::UserRole + 3;
	constexpr int colorRole = Qt::UserRole + 4;

	int iconIndexOf(const Buffer* buf)
	{
		return buf->isMonitoringOn()?3:(buf->isReadOnly()?2:(buf->isDirty()?1:0));
	}
}

void VerticalFileSwitcherListView::init(HINSTANCE hInst, HWND parent, const IconList* icons)
{
	Window::init(hInst, parent);
	_icons = icons;

	auto* list = new QTreeWidget(parent);
	_hSelf = list;
	list->setSelectionMode(QAbstractItemView::ExtendedSelection);
	list->setRootIsDecorated(false);
	list->setIndentation(0);
	list->setUniformRowHeights(true);
	list->setAllColumnsShowFocus(true);
	list->setExpandsOnDoubleClick(false);
	list->setContextMenuPolicy(Qt::CustomContextMenu);
	list->header()->setContextMenuPolicy(Qt::CustomContextMenu);
	list->header()->setSectionsClickable(true);
	list->header()->setSectionsMovable(false);
	list->header()->setStretchLastSection(false);
	list->header()->setSortIndicatorShown(false);
}

void VerticalFileSwitcherListView::destroy()
{
	delete _hSelf;
	_hSelf = nullptr;
}

QTreeWidget* VerticalFileSwitcherListView::tree() const
{
	return static_cast<QTreeWidget*>(_hSelf);
}

bool VerticalFileSwitcherListView::infoOf(const QTreeWidgetItem* item, BufferID& bufferID, int& iView)
{
	if (!item || !item->data(0, bufferRole).isValid())
		return false;

	bufferID = reinterpret_cast<BufferID>(item->data(0, bufferRole).value<quintptr>());
	iView = item->data(0, viewRole).toInt();
	return true;
}

void VerticalFileSwitcherListView::initList()
{
	NppParameters& nppParams = NppParameters::getInstance();
	NativeLangSpeaker *pNativeSpeaker = nppParams.getNativeLangSpeaker();
	const NppGUI& nppGUI = nppParams.getNppGUI();

	const bool isExtColumn = !nppGUI._fileSwitcherWithoutExtColumn;
	const bool isPathColumn = !nppGUI._fileSwitcherWithoutPathColumn;

	//add columns
	QStringList columns;
	columns << QString::fromStdWString(pNativeSpeaker->getAttrNameStr(L"Name", FS_ROOTNODE, FS_CLMNNAME));
	if (isExtColumn)
		columns << QString::fromStdWString(pNativeSpeaker->getAttrNameStr(L"Ext.", FS_ROOTNODE, FS_CLMNEXT));
	if (isPathColumn)
		columns << QString::fromStdWString(pNativeSpeaker->getAttrNameStr(L"Path", FS_ROOTNODE, FS_CLMNPATH));

	QTreeWidget* list = tree();
	list->setColumnCount(static_cast<int>(columns.size()));
	list->setHeaderLabels(columns);

	QHeaderView* header = list->header();
	header->setSectionResizeMode(0, QHeaderView::Stretch);
	int column = 0;
	if (isExtColumn)
	{
		++column;
		header->setSectionResizeMode(column, QHeaderView::Interactive);
		header->resizeSection(column, nppGUI._fileSwitcherExtWidth);
	}
	if (isPathColumn)
	{
		++column;
		header->setSectionResizeMode(column, QHeaderView::Interactive);
		header->resizeSection(column, nppGUI._fileSwitcherPathWidth);
	}

	TaskListInfo taskListInfo;
	::SendMessage(nppMainWindow(), WM_GETTASKLISTINFO, reinterpret_cast<WPARAM>(&taskListInfo), TRUE);

	// Grouping only tells something when both views hold documents
	bool hasMain = false;
	bool hasSub = false;
	for (const TaskLstFnStatus& status : taskListInfo._tlfsLst)
	{
		hasMain = hasMain || status._iView == MAIN_VIEW;
		hasSub = hasSub || status._iView == SUB_VIEW;
	}

	if (!nppGUI._fileSwitcherDisableListViewGroups && hasMain && hasSub)
	{
		for (int view : { MAIN_VIEW, SUB_VIEW })
		{
			auto* group = new QTreeWidgetItem(list);
			group->setData(0, viewRole, view);
			group->setFlags(Qt::ItemIsEnabled);
			group->setExpanded(true);
			group->setFirstColumnSpanned(true);
		}
	}

	QTreeWidgetItem* current = nullptr;
	for (size_t i = 0, len = taskListInfo._tlfsLst.size(); i < len ; ++i)
	{
		const TaskLstFnStatus& fileNameStatus = taskListInfo._tlfsLst[i];
		auto* item = new QTreeWidgetItem;
		item->setData(0, bufferRole, QVariant::fromValue(reinterpret_cast<quintptr>(fileNameStatus._bufID)));
		item->setData(0, viewRole, fileNameStatus._iView);
		item->setData(0, colorRole, fileNameStatus._docColor);
		fillRow(item, fileNameStatus._fn, fileNameStatus._status);

		if (QTreeWidgetItem* group = groupOf(fileNameStatus._iView))
			group->addChild(item);
		else
			list->addTopLevelItem(item);

		if (static_cast<int>(i) == taskListInfo._currentIndex)
			current = item;
	}

	if (current)
		selectOnly(current);
	ensureVisibleCurrentItem();	// without this call the current item may become invisible after adding/removing columns
}

void VerticalFileSwitcherListView::reload()
{
	tree()->clear();
	initList();
}

QTreeWidgetItem* VerticalFileSwitcherListView::groupOf(int iView) const
{
	QTreeWidget* list = tree();
	for (int i = 0; i < list->topLevelItemCount(); ++i)
	{
		QTreeWidgetItem* item = list->topLevelItem(i);
		if (!item->data(0, bufferRole).isValid() && item->data(0, viewRole).toInt() == iView)
			return item;
	}
	return nullptr;
}

std::vector<QTreeWidgetItem*> VerticalFileSwitcherListView::rows() const
{
	std::vector<QTreeWidgetItem*> result;
	QTreeWidget* list = tree();
	for (int i = 0; i < list->topLevelItemCount(); ++i)
	{
		QTreeWidgetItem* top = list->topLevelItem(i);
		if (top->data(0, bufferRole).isValid())
			result.push_back(top);
		for (int j = 0; j < top->childCount(); ++j)
			result.push_back(top->child(j));
	}
	return result;
}

void VerticalFileSwitcherListView::fillRow(QTreeWidgetItem* item, const std::wstring& fullPath, int iconIndex) const
{
	const NppGUI& nppGUI = NppParameters::getInstance().getNppGUI();
	const bool isExtColumn = !nppGUI._fileSwitcherWithoutExtColumn;
	const bool isPathColumn = !nppGUI._fileSwitcherWithoutPathColumn;

	const wchar_t* fileName = PathFindFileName(fullPath.c_str());
	const wchar_t* extension = PathFindExtension(fileName);
	const std::wstring name = isExtColumn ? std::wstring(fileName, extension) : std::wstring(fileName);

	item->setText(0, QString::fromStdWString(name));
	if (_icons && static_cast<size_t>(iconIndex) < _icons->size())
		item->setIcon(0, _icons->icon(iconIndex));

	int column = 0;
	if (isExtColumn)
		item->setText(++column, QString::fromWCharArray(extension));
	if (isPathColumn)
		item->setText(++column, QString::fromStdWString(std::wstring(fullPath.c_str(), fileName)));

	item->setData(0, pathRole, QString::fromStdWString(fullPath));
	item->setToolTip(0, QString::fromStdWString(fullPath));

	const int colorID = item->data(0, colorRole).toInt();
	const bool useColor = item->data(0, colorRole).isValid() && colorID != -1;
	const COLORREF color = useColor ? NppParameters::getInstance().getIndividualTabColor(colorID, NppDarkMode::isThemeDark(), false) : 0;
	for (int i = 0; i < tree()->columnCount(); ++i)
		item->setBackground(i, useColor ? QBrush(QColor(GetRValue(color), GetGValue(color), GetBValue(color))) : QBrush());
}

QTreeWidgetItem* VerticalFileSwitcherListView::add(BufferID bufferID, int iView, int docColor)
{
	auto* item = new QTreeWidgetItem;
	item->setData(0, bufferRole, QVariant::fromValue(reinterpret_cast<quintptr>(bufferID)));
	item->setData(0, viewRole, iView);
	item->setData(0, colorRole, docColor);
	fillRow(item, bufferID->getFullPathName(), iconIndexOf(bufferID));

	if (QTreeWidgetItem* group = groupOf(iView))
		group->addChild(item);
	else
		tree()->addTopLevelItem(item);

	return item;
}

QTreeWidgetItem* VerticalFileSwitcherListView::find(BufferID bufferID, int iView) const
{
	for (QTreeWidgetItem* item : rows())
	{
		BufferID id = nullptr;
		int view = 0;
		if (infoOf(item, id, view) && id == bufferID && view == iView)
			return item;
	}
	return nullptr;
}

int VerticalFileSwitcherListView::newItem(BufferID bufferID, int iView)
{
	if (!find(bufferID, iView))
		selectOnly(add(bufferID, iView));
	return 0;
}

void VerticalFileSwitcherListView::setItemIconStatus(BufferID bufferID)
{
	for (QTreeWidgetItem* item : rows())
	{
		BufferID id = nullptr;
		int view = 0;
		if (infoOf(item, id, view) && id == bufferID)
			fillRow(item, bufferID->getFullPathName(), iconIndexOf(bufferID));
	}
}

void VerticalFileSwitcherListView::setItemColor(BufferID bufferID)
{
	for (QTreeWidgetItem* item : rows())
	{
		BufferID id = nullptr;
		int view = 0;
		if (infoOf(item, id, view) && id == bufferID)
		{
			item->setData(0, colorRole, bufferID->getDocColorId());
			fillRow(item, bufferID->getFullPathName(), iconIndexOf(bufferID));
		}
	}
}

wstring VerticalFileSwitcherListView::getFullFilePath(size_t i) const
{
	const std::vector<QTreeWidgetItem*> all = rows();
	return (i < all.size()) ? all[i]->data(0, pathRole).toString().toStdWString() : wstring{};
}

int VerticalFileSwitcherListView::closeItem(BufferID bufferID, int iView)
{
	QTreeWidgetItem* item = find(bufferID, iView);
	if (!item)
		return -1;

	delete item;
	return 0;
}

void VerticalFileSwitcherListView::selectOnly(QTreeWidgetItem* item) const
{
	const QSignalBlocker silent(tree());
	tree()->clearSelection();
	if (item)
	{
		item->setSelected(true);
		tree()->setCurrentItem(item);
	}
}

void VerticalFileSwitcherListView::activateItem(BufferID bufferID, int iView)
{
	QTreeWidgetItem* item = find(bufferID, iView);
	if (!item)
		item = add(bufferID, iView);

	selectOnly(item);
	ensureVisibleCurrentItem();
}

void VerticalFileSwitcherListView::ensureVisibleCurrentItem() const
{
	if (_hSelf && tree()->currentItem())
		tree()->scrollToItem(tree()->currentItem());
}

int VerticalFileSwitcherListView::nbSelectedFiles() const
{
	int count = 0;
	for (const QTreeWidgetItem* item : rows())
		count += item->isSelected() ? 1 : 0;
	return count;
}

std::vector<BufferViewInfo> VerticalFileSwitcherListView::getSelectedFiles(bool reverse) const
{
	std::vector<BufferViewInfo> files;
	for (const QTreeWidgetItem* item : rows())
	{
		BufferID id = nullptr;
		int view = 0;
		if (item->isSelected() != reverse && infoOf(item, id, view))
			files.push_back(BufferViewInfo(id, view));
	}
	return files;
}

void VerticalFileSwitcherListView::sortBy(int column, int direction)
{
	QHeaderView* header = tree()->header();
	const Qt::SortOrder order = (direction == SORT_DIRECTION_DOWN) ? Qt::DescendingOrder : Qt::AscendingOrder;
	header->setSortIndicatorShown(direction != SORT_DIRECTION_NONE);
	header->setSortIndicator(column, order);
	if (direction != SORT_DIRECTION_NONE)
		tree()->sortItems(column, order);
}

void VerticalFileSwitcherListView::setColours(COLORREF background, COLORREF text)
{
	QPalette palette = tree()->palette();
	palette.setColor(QPalette::Base, QColor(GetRValue(background), GetGValue(background), GetBValue(background)));
	palette.setColor(QPalette::Text, QColor(GetRValue(text), GetGValue(text), GetBValue(text)));
	tree()->setPalette(palette);
}
