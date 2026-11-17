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

#include "NppConstants.h"
#include "Parameters.h"
#include "StaticDialog.h"
#include "Window.h"
#include "columnEditor_rc.h"

class ScintillaEditView;


class ColumnEditorDlg : public StaticDialog
{
public :
	ColumnEditorDlg() = default;
	void init(HINSTANCE hInst, HWND hPere, ScintillaEditView **ppEditView);

	void doDialog(bool isRTL = false);

	void switchTo(bool toText);
	NumBase getFormat();
	ColumnEditorParam::leadingChoice getLeading();
	NumBase getHexCase();

private :
	ScintillaEditView **_ppEditView = nullptr;

	using Window::init;

	void setupControls();
	void onOk();
	void onFormatChosen(int id);
	void onNumericFieldEdited(int id);
	void setNumericFields(const ColumnEditorParam& colEditParam);
	int getNumericFieldValueFromText(NumBase formatChoice, const std::wstring& str);
	void showValidationError(int field, NumBase formatChoice, const std::wstring& str);
};
