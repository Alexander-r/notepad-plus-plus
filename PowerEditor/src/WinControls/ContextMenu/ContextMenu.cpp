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


#include "ContextMenu.h"

#include <QAction>
#include <QCursor>
#include <QMenu>
#include <QPoint>
#include <QWidget>

#include "NppMenu.h"
#include "menuCmdID.h"
#include "Parameters.h"
#include "localization.h"

MenuItemUnit::MenuItemUnit(unsigned long cmdID, const wchar_t* itemName, const wchar_t* parentFolderName) : _cmdID(cmdID)
{
	if (!itemName)
		_itemName.clear();
	else
		_itemName = itemName;

	if (!parentFolderName)
		_parentFolderName.clear();
	else
		_parentFolderName = parentFolderName;
}

void ContextMenu::create(HWND hParent, const std::vector<MenuItemUnit> & menuItemArray, const HMENU mainMenuHandle, bool copyLink)
{
	_hParent = hParent;
	_hMenu = new QMenu(hParent);
	bool lastIsSep = false;
	HMENU hParentFolder = nullptr;
	std::wstring currentParentFolderStr;

	for (size_t i = 0, len = menuItemArray.size(); i < len; ++i)
	{
		const MenuItemUnit & item = menuItemArray[i];
		if (item._parentFolderName.empty())
		{
			currentParentFolderStr.clear();
			hParentFolder = nullptr;
		}
		else if (item._parentFolderName != currentParentFolderStr)
		{
			currentParentFolderStr = item._parentFolderName;
			hParentFolder = nppAddSubMenu(_hMenu, currentParentFolderStr);
		}

		if (hParentFolder)
		{
			if (item._cmdID == 0)
				nppAddMenuSeparator(hParentFolder);
			else
				nppAddMenuItem(hParentFolder, static_cast<int>(item._cmdID), item._itemName, _hParent);
			lastIsSep = false;
		}
		else if ((i == 0 || i == len - 1) && item._cmdID == 0)
		{
			lastIsSep = true;
		}
		else if (item._cmdID != 0)
		{
			nppAddMenuItem(_hMenu, static_cast<int>(item._cmdID), item._itemName, _hParent);
			lastIsSep = false;
		}
		else if (!lastIsSep)
		{
			nppAddMenuSeparator(_hMenu);
			lastIsSep = true;
		}
		else // last item is separator and current item is separator
		{
			lastIsSep = true;
		}

		if (mainMenuHandle && item._cmdID != 0)
		{
			// Same state and icon as the main menu's item
			if (const QAction* mainAction = nppFindMenuAction(mainMenuHandle, static_cast<int>(item._cmdID)))
			{
				if (QAction* action = findAction(static_cast<int>(item._cmdID)))
				{
					action->setIcon(mainAction->icon());
					action->setEnabled(mainAction->isEnabled());
					if (mainAction->isChecked())
						checkItem(static_cast<int>(item._cmdID), true);
				}
			}
		}

		if (copyLink && (item._cmdID == IDM_EDIT_COPY))
		{
			NativeLangSpeaker* nativeLangSpeaker = NppParameters::getInstance().getNativeLangSpeaker();
			std::wstring localized = nativeLangSpeaker->getNativeLangMenuString(IDM_EDIT_COPY_LINK);
			if (localized.empty())
				localized = L"Copy link";

			// The item just added becomes "Copy link"
			if (QAction* action = findAction(IDM_EDIT_COPY))
			{
				nppRemoveMenuCommand(_hMenu, IDM_EDIT_COPY);
				nppAddMenuItem(_hMenu, IDM_EDIT_COPY_LINK, localized, _hParent);
				static_cast<void>(action);
			}
		}
	}
}

QAction* ContextMenu::findAction(int cmdID) const
{
	return nppFindMenuAction(_hMenu, cmdID);
}

void ContextMenu::display(const POINT & p) const
{
	if (_hMenu)
		_hMenu->exec(QPoint(p.x, p.y));
}

void ContextMenu::display(HWND) const
{
	if (_hMenu)
		_hMenu->exec(QCursor::pos());
}

void ContextMenu::enableItem(int cmdID, bool doEnable) const
{
	nppEnableMenuCommand(_hMenu, cmdID, doEnable);
}

void ContextMenu::checkItem(int cmdID, bool doCheck) const
{
	nppCheckMenuCommand(_hMenu, cmdID, doCheck);
}

void ContextMenu::destroy()
{
	delete _hMenu;
	_hMenu = nullptr;
}
