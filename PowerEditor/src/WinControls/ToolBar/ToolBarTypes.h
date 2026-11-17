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

#include <cstddef>
#include <string>
#include <vector>

enum toolBarStatusType {TB_SMALL, TB_LARGE, TB_SMALL2, TB_LARGE2, TB_STANDARD};

struct iconLocator {
	size_t _listIndex = 0;
	size_t _iconIndex = 0;
	std::wstring _iconLocation;

	iconLocator(size_t iList, size_t iIcon, const std::wstring& iconLoc)
		: _listIndex(iList), _iconIndex(iIcon), _iconLocation(iconLoc) {}
};

struct ToolBarButtonUnit
{
	int _cmdID;

	int _defaultIcon;
	int _grayIcon;

	int _defaultIcon2;
	int _grayIcon2;

	int _defaultDarkModeIcon;
	int _grayDarkModeIcon;

	int _defaultDarkModeIcon2;
	int _grayDarkModeIcon2;

	int _stdIcon;
};

struct ToolbarPluginButtonsConf
{
	bool _isHideAll = false;
	std::vector<bool> _showPluginButtonsArray;
};
