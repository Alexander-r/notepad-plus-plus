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

#include "Window.h"

class QLabel;

// Turns a label of a form into a link: to a web address, or to a command for the owner of the dialog.
#include <functional>

class URLCtrl : public Window
{
public:
	void create(HWND itemHandle, const wchar_t* link);
	void create(HWND itemHandle, int cmd, HWND msgDest = nullptr);
	// Keeps the label's text, so it can be translated afterwards
	void create(HWND itemHandle, std::function<void()> onClick);
	void destroy() override {}

private:
	void applyStyle(QLabel* label) const;

	std::wstring _URL;
	int _cmdID = 0;
	HWND _msgDest = nullptr;
};
