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

#include "Common.h"

class QAction;

struct MenuItemUnit final
{
	unsigned long _cmdID = 0;
	std::wstring _itemName;
	std::wstring _parentFolderName;

	MenuItemUnit() = default;
	MenuItemUnit(unsigned long cmdID, const std::wstring& itemName, const std::wstring& parentFolderName = std::wstring())
		: _cmdID(cmdID), _itemName(itemName), _parentFolderName(parentFolderName) {}
	MenuItemUnit(unsigned long cmdID, const wchar_t* itemName, const wchar_t* parentFolderName = nullptr);
};


// Popup menu of command IDs. Choosing an item sends WM_COMMAND with its ID to the parent.
class ContextMenu final
{
public:
	~ContextMenu() {
		destroy();
	}

	void create(HWND hParent, const std::vector<MenuItemUnit> & menuItemArray, const HMENU mainMenuHandle = nullptr, bool copyLink = false);
	bool isCreated() const {return _hMenu != nullptr;}

	void display(const POINT & p) const;
	void display(HWND hwnd) const;

	void enableItem(int cmdID, bool doEnable) const;
	void checkItem(int cmdID, bool doCheck) const;

	HMENU getMenuHandle() const
	{
		return _hMenu;
	}

	void destroy();

private:
	QAction* findAction(int cmdID) const;

	HWND _hParent = nullptr;
	HMENU _hMenu = nullptr;
};
