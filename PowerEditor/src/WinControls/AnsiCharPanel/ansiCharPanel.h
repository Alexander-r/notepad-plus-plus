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

#include "DockingDlgInterface.h"
#include "ansiCharPanel_rc.h"
#include "asciiListView.h"

#define AI_PROJECTPANELTITLE		L"ASCII Codes Insertion Panel"

class ScintillaEditView;

class AnsiCharPanel : public DockingDlgInterface {
public:
	AnsiCharPanel(): DockingDlgInterface(IDD_ANSIASCII_PANEL) {}

	void init(HINSTANCE hInst, HWND hPere, ScintillaEditView **ppEditView) {
		DockingDlgInterface::init(hInst, hPere);
		_ppEditView = ppEditView;
	}

	void setParent(HWND parent2set) {
		_hParent = parent2set;
	}

	void switchEncoding();
	void insertChar(unsigned char char2insert) const;
	void insertString(const std::wstring& string2insert) const;

	using DockingDlgInterface::create;
	void create(DockedWidgetData* data, bool isRTL = false) override;

	void setBackgroundColor(COLORREF bgColour) override {
		_bgColour = bgColour;
		_listView.setColours(_bgColour, _fgColour);
	}

	void setForegroundColor(COLORREF fgColour) override {
		_fgColour = fgColour;
		_listView.setColours(_bgColour, _fgColour);
	}

protected:
	void onDockingNotify(const NMHDR& notification) override;

private:
	ScintillaEditView **_ppEditView = nullptr;
	AsciiListView _listView;
	COLORREF _bgColour = RGB(0xFF, 0xFF, 0xFF);
	COLORREF _fgColour = RGB(0, 0, 0);

	using DockingDlgInterface::init;
};
