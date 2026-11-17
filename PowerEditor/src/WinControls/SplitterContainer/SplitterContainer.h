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

#include "Window.h"

class QSplitter;

enum class SplitterMode { DYNAMIC, LEFT_FIX, RIGHT_FIX };

// Two windows side by side or one above the other, with a movable divider.
class SplitterContainer : public Window
{
public:
	~SplitterContainer() override = default;

	void create(Window *pWin0, Window *pWin1, int splitterSize, SplitterMode mode = SplitterMode::DYNAMIC, int ratio = 50, bool isVertical = true);
	void destroy() override;

	void setWin0(Window* pWin);
	void setWin1(Window* pWin);

	bool isVertical() const;
	void rotate();
	void rotateTo(bool isToLeft);
	void centre();

private:
	QWidget* _pane1 = nullptr;

	QSplitter* splitter() const;
	void applyRatio(int ratio);
};
