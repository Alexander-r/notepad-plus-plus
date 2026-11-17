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
#include "Notepad_plus.h"

constexpr int splitterSize = 8;


class QMainWindow;

// The application window. Its widget hands window events to Notepad_plus::process() as messages, the same
// way plugins talk to it, and those messages stay the one way into the application logic.
class Notepad_plus_Window : public Window
{
public:
	void init(HINSTANCE, HWND, const wchar_t *cmdLine, CmdLineParams *cmdLineParams);

	QMainWindow* mainWindow() const;

	bool emergency(const std::wstring& emergencySavedDir) {
		return _notepad_plus_plus_core.emergency(emergencySavedDir);
	}

	bool isPrelaunch() const {
		return _isPrelaunch;
	}

	void setIsPrelaunch(bool val) {
		_isPrelaunch = val;
	}

	void destroy() override;

	QIcon* getAbsentIcoHandle() {
		return _hIconAbsent;
	}

	static HWND gNppHWND;	//static handle to Notepad++ window, NULL if non-existent

private:
	Notepad_plus _notepad_plus_plus_core;
	bool _isPrelaunch = false;
	bool _disablePluginsManager = false;

	QuoteParams _quoteParams; // keep the availability of quote parameters for thread using
	std::wstring _userQuote; // keep the availability of this string for thread using

	QIcon* _hIconAbsent = nullptr;
};
