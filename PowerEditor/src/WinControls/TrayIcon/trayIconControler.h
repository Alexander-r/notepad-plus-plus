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

#include <memory>

#include "NppPlatform.h"

class QIcon;
class QSystemTrayIcon;

inline constexpr DWORD ADD = 0;
inline constexpr DWORD REMOVE = 2;

// code d'erreur
inline constexpr int INCORRECT_OPERATION = 1;
inline constexpr int OPERATION_INCOHERENT = 2;

// The icon in the desktop's notification area. It reports clicks to the window as the message uCBMsg, with the
// WM_LBUTTONUP, WM_MBUTTONUP or WM_RBUTTONUP of the action in lParam.
class trayIconControler
{
public:
	trayIconControler(HWND hwnd, UINT uID, UINT uCBMsg, QIcon* hicon, const wchar_t *tip);
	~trayIconControler();

	int doTrayIcon(DWORD op);
	bool isInTray() const { return _isIconShown; }
	int reAddTrayIcon();

private:
	std::unique_ptr<QSystemTrayIcon> _icon;
	bool _isIconShown = false;
};
