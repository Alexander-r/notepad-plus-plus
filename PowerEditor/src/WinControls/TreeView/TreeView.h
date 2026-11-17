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

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Common.h"
#include "NppDarkMode.h"
#include "Window.h"

class QTreeWidget;
class QTreeWidgetItem;

using HTREEITEM = QTreeWidgetItem*;

struct TreeStateNode {
	std::wstring _label;
	std::wstring _extraData;
	bool _isExpanded = false;
	bool _isSelected = false;
	std::vector<TreeStateNode> _children;
};

// Compares two items by their parameters, as a negative, zero or positive number
using TreeCompareFunction = int (*)(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);

// A tree whose items each hold an image index and a parameter. A tree can have several top level items, all
// reached through getRoot() and getNextSibling().
class TreeView : public Window {
public:
	TreeView() = default;
	~TreeView() override = default;

	virtual void init(HINSTANCE hInst, HWND parent, int treeViewID);
	void destroy() override;
	HTREEITEM addItem(const wchar_t *itemName, HTREEITEM hParentItem, int iImage, LPARAM lParam = 0);
	bool setItemParam(HTREEITEM Item2Set, LPARAM param);
	LPARAM getItemParam(HTREEITEM Item2Get) const;
	int getItemImage(HTREEITEM item) const;
	std::wstring getItemDisplayName(HTREEITEM Item2Set) const;
	HTREEITEM searchSubItemByName(const wchar_t *itemName, HTREEITEM hParentItem);
	void removeItem(HTREEITEM hTreeItem);
	void removeAllItems();
	bool renameItem(const HTREEITEM Item2Set, const wchar_t* newName);
	void makeLabelEditable(bool toBeEnabled);
	void editLabel(HTREEITEM item);

	HTREEITEM getChildFrom(HTREEITEM hTreeItem) const;
	HTREEITEM getSelection() const;
	bool selectItem(HTREEITEM hTreeItem2Select) const;
	HTREEITEM getRoot() const;
	HTREEITEM getParent(HTREEITEM hItem) const;
	HTREEITEM getNextSibling(HTREEITEM hItem) const;
	HTREEITEM getPrevSibling(HTREEITEM hItem) const;

	void expand(HTREEITEM hItem) const;
	void fold(HTREEITEM hItem) const;
	bool isExpanded(HTREEITEM hItem) const;

	void foldExpandRecursively(HTREEITEM hItem, bool isFold) const;
	void foldExpandAll(bool isFold) const;

	void foldAll() const {
		foldExpandAll(true);
	}

	void expandAll() const {
		foldExpandAll(false);
	}

	void toggleExpandCollapse(HTREEITEM hItem) const;
	void setItemImage(HTREEITEM hTreeItem, int iImage, int iSelectedImage);

	// Drag and Drop operations: moving items within the tree with the mouse
	void enableDragAndDrop(std::function<void()> onDropped = {});
	void addCanNotDropInList(int val2set) {
		_canNotDropInList.push_back(val2set);
	}

	void addCanNotDragOutList(int val2set) {
		_canNotDragOutList.push_back(val2set);
	}

	bool moveDown(HTREEITEM itemToMove);
	bool moveUp(HTREEITEM itemToMove);
	bool swapTreeViewItem(HTREEITEM itemGoDown, HTREEITEM itemGoUp);
	bool restoreFoldingStateFrom(const TreeStateNode & treeState2Compare, HTREEITEM treeviewNode);
	bool retrieveFoldingStateTo(TreeStateNode & treeState2Construct, HTREEITEM treeviewNode);
	bool searchLeafAndBuildTree(const TreeView & tree2Build, const std::wstring & text2Search, int index2Search);
	void sort(HTREEITEM hTreeItem, bool isRecusive);
	void customSorting(HTREEITEM hTreeItem, TreeCompareFunction sortingCallbackFunc, LPARAM lParam, bool isRecursive);
	bool setImageList(const std::vector<int>& imageIds, int imgSize = 0);
	std::vector<int> getImageIds(std::vector<int> stdIds, std::vector<int>darkIds, std::vector<int> lightIds);

	// Takes the images again after the icon set or dark mode changed
	void refreshImages();

	void setColours(COLORREF background, COLORREF text);

	QTreeWidget* tree() const;

	// For the drag and drop of the tree widget
	bool canBeDropped(HTREEITEM draggedItem, HTREEITEM targetItem);
	bool canDragOut(HTREEITEM targetItem);
	void moveTreeViewItem(HTREEITEM draggedItem, HTREEITEM targetItem);
	void notifyDropped() const;

protected:
	std::vector<int> _canNotDragOutList;
	std::vector<int> _canNotDropInList;
	std::vector<int> _imageIds;
	std::vector<int> _stdImageIds;
	std::vector<int> _darkImageIds;
	std::vector<int> _lightImageIds;
	std::function<void()> _onDropped;

	void refreshIcon(HTREEITEM item, int iImage) const;
	void refreshAllIcons(HTREEITEM item) const;
	void sortChildren(HTREEITEM parent, const std::function<bool(HTREEITEM, HTREEITEM)>& isBefore) const;
	HTREEITEM dupItem(const TreeView& source, HTREEITEM sourceItem, HTREEITEM hParentItem) const;
	bool searchLeafRecusivelyAndBuildTree(const TreeView& tree2Build, HTREEITEM hBuildParent, const std::wstring & text2Search, int index2Search, HTREEITEM tree2Search);

	bool isParent(HTREEITEM targetItem, HTREEITEM draggedItem);
	bool isDescendant(HTREEITEM targetItem, HTREEITEM draggedItem);
	bool canDropIn(HTREEITEM targetItem);
};
