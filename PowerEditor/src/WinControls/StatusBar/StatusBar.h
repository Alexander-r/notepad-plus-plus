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

#include <QLabel>
#include <QStatusBar>

#include <vector>

#include "Window.h"

class StatusBar final : public Window
{
public:
	~StatusBar() override = default;

	void init(HINSTANCE hInst, HWND hPere, int nbParts);
	bool setPartWidth(int whichPart, int width);
	void destroy() override;
	int getHeight() const override;

	bool setText(const wchar_t* str, int whichPart);
	bool setOwnerDrawText(const wchar_t* str);
	void adjustParts(int) {} // the status bar lays out its parts

private:
	std::vector<QLabel*> _parts;

	using Window::init;
};
