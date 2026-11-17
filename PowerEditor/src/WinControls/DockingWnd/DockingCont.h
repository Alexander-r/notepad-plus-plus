// This file is part of Notepad++ project
// Copyright (c) 2021 adzm / Adam D. Walling

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

#include <vector>

#include "Docking.h"

class QDockWidget;

// One docked panel and the Qt dock widget showing it
struct DockedPanel
{
	DockedWidgetData _data;
	QDockWidget* _dock = nullptr;
};

// The panels of one area of the main window (CONT_LEFT, CONT_RIGHT, CONT_TOP, CONT_BOTTOM), or the floating ones.
class DockingCont
{
public:
	explicit DockingCont(int index) : _index(index) {}

	int getIndex() const { return _index; }

	std::vector<DockedWidgetData*> getDataOfAllTb() const;
	std::vector<DockedWidgetData*> getDataOfVisTb() const;
	bool isTbVis(const DockedWidgetData* data) const;

	DockedWidgetData* findDockedWidgetByWnd(HWND hClient) const;
	int getActiveTb() const;

	// Whether a panel of this area is showing
	bool isVisible() const;
	// Shows or hides all its panels, for full screen and "post it" modes
	void doDialog(bool toShow) const;
	void display(bool toShow = true) const { doDialog(toShow); }
	// The frame of its first panel, for a floating one
	void getWindowRect(RECT& rc) const;

	std::vector<DockedPanel*> _panels;

private:
	int _index;
};
