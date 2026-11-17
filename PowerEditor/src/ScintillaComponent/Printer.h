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

#include "NppPlatform.h"

class ScintillaEditView;

class Printer
{
public :
	Printer() = default;

	void init(HWND hwnd, ScintillaEditView *pSEView, bool showDialog, size_t startPos, size_t endPos, bool isRTL = false);

	// Returns the number of pages produced
	size_t doPrint();

private :
	HWND _hOwner = nullptr;
	ScintillaEditView *_pSEView = nullptr;
	size_t _startPos = 0;
	size_t _endPos = 0;
	bool _showDialog = true;
	bool _isRTL = false;
};
