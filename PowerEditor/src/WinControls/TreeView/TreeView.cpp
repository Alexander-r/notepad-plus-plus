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



#include "TreeView.h"

#include <QColor>
#include <QDropEvent>
#include <QHeaderView>
#include <QPalette>
#include <QString>
#include <QTreeWidget>

#include <algorithm>

#include "NppResources.h"
#include "Parameters.h"

using namespace std;

namespace
{
	constexpr int paramRole = Qt::UserRole + 1;
	constexpr int imageRole = Qt::UserRole + 2;

	QString toQ(const wstring& text)
	{
		return QString::fromStdWString(text);
	}

	class NppTreeWidget : public QTreeWidget
	{
	public:
		NppTreeWidget(TreeView* owner, QWidget* parent) : QTreeWidget(parent), _owner(owner) {}

	protected:
		void dragMoveEvent(QDragMoveEvent* event) override
		{
			QTreeWidgetItem* target = itemAt(event->position().toPoint());
			if (currentItem() && target && _owner->canBeDropped(currentItem(), target))
				event->accept();
			else
				event->ignore();
		}

		void dropEvent(QDropEvent* event) override
		{
			QTreeWidgetItem* target = itemAt(event->position().toPoint());
			QTreeWidgetItem* dragged = currentItem();
			event->ignore(); // the view must not remove the row by itself
			if (dragged && target && _owner->canBeDropped(dragged, target))
			{
				_owner->moveTreeViewItem(dragged, target);
				_owner->notifyDropped();
			}
		}

		void startDrag(Qt::DropActions supportedActions) override
		{
			if (currentItem() && _owner->canDragOut(currentItem()))
				QTreeWidget::startDrag(supportedActions);
		}

	private:
		TreeView* _owner;
	};
}

void TreeView::init(HINSTANCE hInst, HWND parent, int treeViewID)
{
	Window::init(hInst, parent);

	auto* widget = new NppTreeWidget(this, parent);
	widget->setProperty("nppId", treeViewID);
	widget->setColumnCount(1);
	widget->header()->hide();
	widget->setExpandsOnDoubleClick(true);
	widget->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
	widget->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
	widget->header()->setStretchLastSection(false);
	widget->setContextMenuPolicy(Qt::CustomContextMenu);
	widget->setEditTriggers(QAbstractItemView::NoEditTriggers);
	_hSelf = widget;
	_hSelf->hide();
}

QTreeWidget* TreeView::tree() const
{
	return static_cast<QTreeWidget*>(_hSelf);
}

void TreeView::destroy()
{
	delete _hSelf;
	_hSelf = nullptr;
}

void TreeView::makeLabelEditable(bool toBeEnabled)
{
	tree()->setProperty("labelEditable", toBeEnabled);
	tree()->setEditTriggers(toBeEnabled ? QAbstractItemView::SelectedClicked : QAbstractItemView::NoEditTriggers);
}

void TreeView::editLabel(HTREEITEM item)
{
	if (!item)
		return;

	item->setFlags(item->flags() | Qt::ItemIsEditable);
	tree()->setCurrentItem(item);
	tree()->editItem(item);
}

bool TreeView::setItemParam(HTREEITEM Item2Set, LPARAM param)
{
	if (!Item2Set)
		return false;

	Item2Set->setData(0, paramRole, QVariant::fromValue<qlonglong>(param));
	return true;
}

LPARAM TreeView::getItemParam(HTREEITEM Item2Get) const
{
	return Item2Get ? static_cast<LPARAM>(Item2Get->data(0, paramRole).toLongLong()) : 0;
}

int TreeView::getItemImage(HTREEITEM item) const
{
	return item ? item->data(0, imageRole).toInt() : -1;
}

wstring TreeView::getItemDisplayName(HTREEITEM Item2Set) const
{
	return Item2Set ? Item2Set->text(0).toStdWString() : wstring{};
}

bool TreeView::renameItem(const HTREEITEM Item2Set, const wchar_t* newName)
{
	if (!Item2Set || !newName)
		return false;

	// prevent multiplication of the same item
	HTREEITEM hParent = getParent(Item2Set);
	HTREEITEM hItem = hParent ? getChildFrom(hParent) : getRoot(); // get current level 1st item
	while (hItem)
	{
		if (getItemDisplayName(hItem) == newName)
			return false; // a duplicity found, interrupting

		hItem = getNextSibling(hItem); // next item to check
	}

	// ok, can rename
	Item2Set->setText(0, QString::fromWCharArray(newName));
	return true;
}

HTREEITEM TreeView::addItem(const wchar_t *itemName, HTREEITEM hParentItem, int iImage, LPARAM lParam)
{
	auto* item = hParentItem ? new QTreeWidgetItem(hParentItem) : new QTreeWidgetItem(tree());
	item->setText(0, QString::fromWCharArray(itemName));
	item->setData(0, paramRole, QVariant::fromValue<qlonglong>(lParam));
	item->setData(0, imageRole, iImage);
	if (tree()->property("labelEditable").toBool())
		item->setFlags(item->flags() | Qt::ItemIsEditable);
	refreshIcon(item, iImage);
	return item;
}

void TreeView::refreshIcon(HTREEITEM item, int iImage) const
{
	if (iImage >= 0 && static_cast<size_t>(iImage) < _imageIds.size())
	{
		if (const QIcon* icon = nppLoadIcon(_imageIds[iImage]))
			item->setIcon(0, *icon);
	}
}

void TreeView::removeItem(HTREEITEM hTreeItem)
{
	delete hTreeItem;
}

void TreeView::removeAllItems()
{
	tree()->clear();
}

HTREEITEM TreeView::dupItem(const TreeView& source, HTREEITEM sourceItem, HTREEITEM hParentItem) const
{
	auto* item = hParentItem ? new QTreeWidgetItem(hParentItem) : new QTreeWidgetItem(tree());
	item->setText(0, sourceItem->text(0));
	item->setIcon(0, sourceItem->icon(0));
	item->setData(0, paramRole, sourceItem->data(0, paramRole));
	item->setData(0, imageRole, sourceItem->data(0, imageRole));
	Q_UNUSED(source);
	return item;
}

HTREEITEM TreeView::searchSubItemByName(const wchar_t *itemName, HTREEITEM hParentItem)
{
	HTREEITEM hItem = hParentItem ? getChildFrom(hParentItem) : getRoot();
	while (hItem != nullptr)
	{
		if (getItemDisplayName(hItem) == itemName)
			return hItem;

		hItem = getNextSibling(hItem);
	}
	return nullptr;
}

HTREEITEM TreeView::getChildFrom(HTREEITEM hTreeItem) const
{
	return (hTreeItem && hTreeItem->childCount() > 0) ? hTreeItem->child(0) : nullptr;
}

HTREEITEM TreeView::getSelection() const
{
	return tree()->currentItem();
}

bool TreeView::selectItem(HTREEITEM hTreeItem2Select) const
{
	if (!hTreeItem2Select)
		return false;

	tree()->setCurrentItem(hTreeItem2Select);
	return true;
}

HTREEITEM TreeView::getRoot() const
{
	return tree()->topLevelItem(0);
}

HTREEITEM TreeView::getParent(HTREEITEM hItem) const
{
	return hItem ? hItem->parent() : nullptr;
}

HTREEITEM TreeView::getNextSibling(HTREEITEM hItem) const
{
	if (!hItem)
		return nullptr;

	if (QTreeWidgetItem* parent = hItem->parent())
	{
		const int index = parent->indexOfChild(hItem);
		return (index + 1 < parent->childCount()) ? parent->child(index + 1) : nullptr;
	}

	const int index = tree()->indexOfTopLevelItem(hItem);
	return (index + 1 < tree()->topLevelItemCount()) ? tree()->topLevelItem(index + 1) : nullptr;
}

HTREEITEM TreeView::getPrevSibling(HTREEITEM hItem) const
{
	if (!hItem)
		return nullptr;

	if (QTreeWidgetItem* parent = hItem->parent())
	{
		const int index = parent->indexOfChild(hItem);
		return (index > 0) ? parent->child(index - 1) : nullptr;
	}

	const int index = tree()->indexOfTopLevelItem(hItem);
	return (index > 0) ? tree()->topLevelItem(index - 1) : nullptr;
}

void TreeView::expand(HTREEITEM hItem) const
{
	if (hItem)
		hItem->setExpanded(true);
}

void TreeView::fold(HTREEITEM hItem) const
{
	if (hItem)
		hItem->setExpanded(false);
}

bool TreeView::isExpanded(HTREEITEM hItem) const
{
	return hItem && hItem->isExpanded();
}

void TreeView::toggleExpandCollapse(HTREEITEM hItem) const
{
	if (hItem)
		hItem->setExpanded(!hItem->isExpanded());
}

bool TreeView::setImageList(const std::vector<int>& imageIds, int /*imgSize*/)
{
	_imageIds = imageIds;
	for (HTREEITEM item = getRoot(); item; item = getNextSibling(item))
		refreshAllIcons(item);
	return true;
}

void TreeView::refreshAllIcons(HTREEITEM item) const
{
	refreshIcon(item, getItemImage(item));
	for (HTREEITEM child = getChildFrom(item); child; child = getNextSibling(child))
		refreshAllIcons(child);
}

void TreeView::refreshImages()
{
	if (!_stdImageIds.empty())
		setImageList(getImageIds(_stdImageIds, _darkImageIds, _lightImageIds));
}

std::vector<int> TreeView::getImageIds(std::vector<int> stdIds, std::vector<int> darkIds, std::vector<int> lightIds)
{
	_stdImageIds = stdIds;
	_darkImageIds = darkIds;
	_lightImageIds = lightIds;

	NppParameters& nppParam = NppParameters::getInstance();
	const bool useStdIcons = nppParam.getNppGUI()._tbIconInfo._tbIconSet == TB_STANDARD;
	if (useStdIcons)
		return stdIds;

	return NppDarkMode::usesDarkWidgets() ? darkIds : lightIds;
}

void TreeView::foldExpandRecursively(HTREEITEM hParentItem, bool isFold) const
{
	if (!hParentItem)
		return;

	for (HTREEITEM hItem = getChildFrom(hParentItem); hItem != nullptr; hItem = getNextSibling(hItem))
	{
		foldExpandRecursively(hItem, isFold);
		hItem->setExpanded(!isFold);
	}
}

void TreeView::foldExpandAll(bool isFold) const
{
	for (HTREEITEM tvProj = getRoot(); tvProj != nullptr; tvProj = getNextSibling(tvProj))
	{
		foldExpandRecursively(tvProj, isFold);
		tvProj->setExpanded(!isFold);
	}
}

void TreeView::setItemImage(HTREEITEM hTreeItem, int iImage, int /*iSelectedImage*/)
{
	if (hTreeItem)
	{
		hTreeItem->setData(0, imageRole, iImage);
		refreshIcon(hTreeItem, iImage);
	}
}

void TreeView::enableDragAndDrop(std::function<void()> onDropped)
{
	_onDropped = std::move(onDropped);
	tree()->setDragEnabled(true);
	tree()->setAcceptDrops(true);
	tree()->setDropIndicatorShown(true);
	tree()->setDragDropMode(QAbstractItemView::InternalMove);
}

void TreeView::notifyDropped() const
{
	if (_onDropped)
		_onDropped();
}

bool TreeView::canBeDropped(HTREEITEM draggedItem, HTREEITEM targetItem)
{
	if (targetItem == draggedItem)
		return false;
	if (targetItem == nullptr)
		return false;
	if (isDescendant(targetItem, draggedItem))
		return false;
	if (isParent(targetItem, draggedItem))
		return false;
	// candragItem, canBeDropInItem
	if (!canDropIn(targetItem))
		return false;
	return true;
}

bool TreeView::isDescendant(HTREEITEM targetItem, HTREEITEM draggedItem)
{
	for (HTREEITEM item = targetItem->parent(); item; item = item->parent())
	{
		if (item == draggedItem)
			return true;
	}
	return false;
}

bool TreeView::isParent(HTREEITEM targetItem, HTREEITEM draggedItem)
{
	return draggedItem->parent() == targetItem;
}

void TreeView::moveTreeViewItem(HTREEITEM draggedItem, HTREEITEM targetItem)
{
	const bool wasExpanded = draggedItem->isExpanded();
	QTreeWidgetItem* from = draggedItem->parent();
	QTreeWidgetItem* moved = from ? from->takeChild(from->indexOfChild(draggedItem)) : tree()->takeTopLevelItem(tree()->indexOfTopLevelItem(draggedItem));

	targetItem->addChild(moved);

	targetItem->setExpanded(true);
	moved->setExpanded(wasExpanded);
	tree()->setCurrentItem(moved);
}

bool TreeView::moveDown(HTREEITEM itemToMove)
{
	HTREEITEM hItemToSwap = getNextSibling(itemToMove);
	if (hItemToSwap == nullptr)
		return false;

	return swapTreeViewItem(itemToMove, hItemToSwap);
}

bool TreeView::moveUp(HTREEITEM itemToMove)
{
	HTREEITEM hItemToSwap = getPrevSibling(itemToMove);
	if (hItemToSwap == nullptr)
		return false;

	return swapTreeViewItem(hItemToSwap, itemToMove);
}

bool TreeView::swapTreeViewItem(HTREEITEM itemGoDown, HTREEITEM itemGoUp)
{
	QTreeWidgetItem* parent = itemGoDown->parent();
	if (parent != itemGoUp->parent())
		return false;

	// Taking an item out of the tree loses its folding and the selection
	QTreeWidgetItem* const selected = tree()->currentItem();
	std::vector<QTreeWidgetItem*> expanded;
	const std::function<void(QTreeWidgetItem*)> collectExpanded = [&](QTreeWidgetItem* item)
	{
		if (item->isExpanded())
			expanded.push_back(item);
		for (int i = 0; i < item->childCount(); ++i)
			collectExpanded(item->child(i));
	};
	collectExpanded(itemGoDown);

	if (parent)
	{
		const int position = parent->indexOfChild(itemGoUp);
		QTreeWidgetItem* moved = parent->takeChild(parent->indexOfChild(itemGoDown));
		parent->insertChild(position, moved);
	}
	else
	{
		const int position = tree()->indexOfTopLevelItem(itemGoUp);
		QTreeWidgetItem* moved = tree()->takeTopLevelItem(tree()->indexOfTopLevelItem(itemGoDown));
		tree()->insertTopLevelItem(position, moved);
	}
	for (QTreeWidgetItem* item : expanded)
		item->setExpanded(true);
	tree()->setCurrentItem(selected);
	return true;
}

bool TreeView::canDropIn(HTREEITEM targetItem)
{
	const int image = getItemImage(targetItem);
	return std::find(_canNotDropInList.begin(), _canNotDropInList.end(), image) == _canNotDropInList.end();
}

bool TreeView::canDragOut(HTREEITEM targetItem)
{
	const int image = getItemImage(targetItem);
	return std::find(_canNotDragOutList.begin(), _canNotDragOutList.end(), image) == _canNotDragOutList.end();
}

bool TreeView::searchLeafAndBuildTree(const TreeView & tree2Build, const wstring & text2Search, int index2Search)
{
	return searchLeafRecusivelyAndBuildTree(tree2Build, tree2Build.getRoot(), text2Search, index2Search, getRoot());
}

bool TreeView::searchLeafRecusivelyAndBuildTree(const TreeView& tree2Build, HTREEITEM hBuildParent, const wstring & text2Search, int index2Search, HTREEITEM tree2Search)
{
	if (!tree2Search)
		return false;

	if (getItemImage(tree2Search) == index2Search)
	{
		wstring itemNameUpperCase = stringToUpper(getItemDisplayName(tree2Search));
		wstring text2SearchUpperCase = stringToUpper(text2Search);

		if (itemNameUpperCase.find(text2SearchUpperCase) != wstring::npos)
			tree2Build.dupItem(*this, tree2Search, hBuildParent);
	}

	bool isOk = true;
	for (HTREEITEM hItem = getChildFrom(tree2Search); hItem != nullptr; hItem = getNextSibling(hItem))
	{
		isOk = searchLeafRecusivelyAndBuildTree(tree2Build, hBuildParent, text2Search, index2Search, hItem);
		if (!isOk)
			break;
	}
	return isOk;
}

bool TreeView::retrieveFoldingStateTo(TreeStateNode & treeState2Construct, HTREEITEM treeviewNode)
{
	if (!treeviewNode)
		return false;

	treeState2Construct._label = getItemDisplayName(treeviewNode);
	treeState2Construct._isExpanded = treeviewNode->isExpanded();
	treeState2Construct._isSelected = treeviewNode->isSelected();

	if (const LPARAM param = getItemParam(treeviewNode))
		treeState2Construct._extraData = *reinterpret_cast<wstring*>(param);

	int i = 0;
	for (HTREEITEM hItem = getChildFrom(treeviewNode); hItem != nullptr; hItem = getNextSibling(hItem))
	{
		treeState2Construct._children.push_back(TreeStateNode());
		retrieveFoldingStateTo(treeState2Construct._children.at(i), hItem);
		++i;
	}
	return true;
}

bool TreeView::restoreFoldingStateFrom(const TreeStateNode & treeState2Compare, HTREEITEM treeviewNode)
{
	if (!treeviewNode)
		return false;

	if (treeState2Compare._isExpanded)
		expand(treeviewNode);
	else
		fold(treeviewNode);

	if (treeState2Compare._isSelected)
		selectItem(treeviewNode);

	size_t i = 0;
	bool isOk = true;
	for (HTREEITEM hItem = getChildFrom(treeviewNode); hItem != nullptr; hItem = getNextSibling(hItem))
	{
		if (i >= treeState2Compare._children.size())
			return false;
		isOk = restoreFoldingStateFrom(treeState2Compare._children.at(i), hItem);
		if (!isOk)
			break;
		++i;
	}
	return isOk;
}

void TreeView::sortChildren(HTREEITEM parent, const std::function<bool(HTREEITEM, HTREEITEM)>& isBefore) const
{
	if (!parent)
		return;

	// Taking children out forgets whether they are expanded
	std::vector<std::pair<HTREEITEM, bool>> children;
	const QList<QTreeWidgetItem*> taken = parent->takeChildren();
	for (QTreeWidgetItem* child : taken)
		children.emplace_back(child, child->isExpanded());

	std::stable_sort(children.begin(), children.end(), [&isBefore](const auto& a, const auto& b) { return isBefore(a.first, b.first); });

	for (const auto& child : children)
		parent->addChild(child.first);
	for (const auto& child : children)
		child.first->setExpanded(child.second);
}

void TreeView::sort(HTREEITEM hTreeItem, bool isRecusive)
{
	sortChildren(hTreeItem, [](HTREEITEM a, HTREEITEM b)
	{
		return QString::localeAwareCompare(a->text(0).toCaseFolded(), b->text(0).toCaseFolded()) < 0;
	});
	if (!isRecusive)
		return;

	for (HTREEITEM hItem = getChildFrom(hTreeItem); hItem != nullptr; hItem = getNextSibling(hItem))
		sort(hItem, isRecusive);
}

void TreeView::customSorting(HTREEITEM hTreeItem, TreeCompareFunction sortingCallbackFunc, LPARAM lParam, bool isRecusive)
{
	sortChildren(hTreeItem, [this, sortingCallbackFunc, lParam](HTREEITEM a, HTREEITEM b)
	{
		return sortingCallbackFunc(getItemParam(a), getItemParam(b), lParam) < 0;
	});
	if (!isRecusive)
		return;

	for (HTREEITEM hItem = getChildFrom(hTreeItem); hItem != nullptr; hItem = getNextSibling(hItem))
		customSorting(hItem, sortingCallbackFunc, lParam, isRecusive);
}

void TreeView::setColours(COLORREF background, COLORREF text)
{
	QPalette palette = tree()->palette();
	palette.setColor(QPalette::Base, QColor(GetRValue(background), GetGValue(background), GetBValue(background)));
	palette.setColor(QPalette::Text, QColor(GetRValue(text), GetGValue(text), GetBValue(text)));
	tree()->setPalette(palette);
}
