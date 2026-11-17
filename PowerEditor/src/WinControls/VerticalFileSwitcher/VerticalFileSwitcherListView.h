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


#pragma once

#include <string>
#include <vector>

#include "Buffer.h"
#include "TaskListDlg.h"
#include "Window.h"

class IconList;
class QTreeWidget;
class QTreeWidgetItem;

inline constexpr int SORT_DIRECTION_NONE = -1;
inline constexpr int SORT_DIRECTION_UP = 0;
inline constexpr int SORT_DIRECTION_DOWN = 1;

#define FS_ROOTNODE					"DocList"
#define FS_CLMNNAME					"ColumnName"
#define FS_CLMNEXT					"ColumnExt"
#define FS_CLMNPATH					"ColumnPath"
#define FS_LVGROUPS					"ListGroups"

// The documents of the two views, one under the other when "group by view" is on.
class VerticalFileSwitcherListView : public Window
{
public:
	VerticalFileSwitcherListView() = default;
	~VerticalFileSwitcherListView() override = default;

	void init(HINSTANCE hInst, HWND parent, const IconList* icons);
	void destroy() override;
	void initList();

	void setIcons(const IconList* icons) { _icons = icons; }

	int newItem(BufferID bufferID, int iView);
	int closeItem(BufferID bufferID, int iView);
	void activateItem(BufferID bufferID, int iView);
	void setItemIconStatus(BufferID bufferID);
	std::wstring getFullFilePath(size_t i) const;
	void setItemColor(BufferID bufferID);

	int nbSelectedFiles() const;
	std::vector<BufferViewInfo> getSelectedFiles(bool reverse = false) const;
	void reload();
	void ensureVisibleCurrentItem() const;

	void sortBy(int column, int direction);

	void setColours(COLORREF background, COLORREF text);

	QTreeWidget* tree() const;

	// The document of a row, with its view. False for a group row.
	static bool infoOf(const QTreeWidgetItem* item, BufferID& bufferID, int& iView);

private:
	const IconList* _icons = nullptr;

	QTreeWidgetItem* find(BufferID bufferID, int iView) const;
	QTreeWidgetItem* add(BufferID bufferID, int iView, int docColor = -1);
	void fillRow(QTreeWidgetItem* item, const std::wstring& fullPath, int iconIndex) const;
	QTreeWidgetItem* groupOf(int iView) const;
	std::vector<QTreeWidgetItem*> rows() const;
	void selectOnly(QTreeWidgetItem* item) const;

	using Window::init;
};
