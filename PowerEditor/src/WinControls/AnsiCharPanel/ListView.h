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

#include "NppPlatform.h"
#include "Window.h"

class QTableWidget;

struct columnInfo {
	size_t _width;
	std::wstring _label;

	columnInfo(const std::wstring& label, size_t width) : _width(width), _label(label) {}
};

// A list of rows, with one column per addColumn(). It selects whole rows.
class ListView : public Window
{
public:
	ListView() = default;
	~ListView() override = default;

	// The control takes the given ID in its nppId property
	void init(HINSTANCE hInst, HWND parent, UINT_PTR listViewID);
	void destroy() override;

	enum SortDirection {
		sortEncrease = 0,
		sortDecrease = 1
	};
	// addColumn() should be called before init()
	void addColumn(const columnInfo & column2Add) {
		_columnInfos.push_back(column2Add);
	}

	void setColumnText(size_t i, const std::wstring& txt2Set);

	size_t findAlphabeticalOrderPos(const std::wstring& string2Cmp, SortDirection sortDir);

	void addLine(const std::vector<std::wstring>& values2Add, LPARAM lParam = 0, int pos2insert = -1);

	size_t nbItem() const;
	long getSelectedIndex() const;
	void setSelection(int itemIndex) const;
	LPARAM getLParamFromIndex(int itemIndex) const;
	bool removeFromIndex(size_t i);
	void removeAll();

	std::wstring getText(int row, int column) const;

	void setColours(COLORREF background, COLORREF text);

	QTableWidget* table() const;

protected:
	std::vector<columnInfo> _columnInfos;

private:
	using Window::init;
};
