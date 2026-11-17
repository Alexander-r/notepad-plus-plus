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

#include "NppMessages.h"
#include "StaticDialog.h"

class ColourPicker;
class QListWidget;
class QSlider;

// A dialog with many controls whose logic is one large switch on the control's ID. Each control calls onCommand() when
// the user changes it, with the ID and what happened in wParam: BN_CLICKED, CBN_SELCHANGE, EN_CHANGE and so on.
class CommandDialog : public StaticDialog
{
protected:
	virtual intptr_t onCommand(WPARAM wParam, LPARAM lParam) = 0;
	virtual void onScroll(HWND /*slider*/) {}

	// Call this when the controls are filled, as filling them is not a change by the user
	void connectControls();

	QListWidget* list(int id) const;
	QSlider* slider(int id) const;

	int getItemInt(int id) const;
	void setItemInt(int id, long long value) const;
	void copyItemText(int id, wchar_t* buffer, size_t bufferSize) const;
	std::wstring comboText(int id, int index) const;
	int addComboItem(int id, const std::wstring& text) const;
	int findComboItem(int id, const std::wstring& text) const;
	int addListItem(int id, const std::wstring& text) const;
	void setListRow(int id, int row) const;
	void setSliderPos(int id, int position) const;
	void limitText(int id, int maxLength) const;
	void setTip(int id, const std::wstring& tip) const;
	void redrawDlgItem(int id) const;

	// Sets a field's text without it counting as a change by the user
	void setItemTextQuietly(int id, const std::wstring& text) const;

	// Puts a colour picker next to a control, in the place the control has in its layout. It calls onCommand() with CPN_COLOURPICKED.
	void placePicker(int id, ColourPicker& picker, bool isOnLeft = true);
};
