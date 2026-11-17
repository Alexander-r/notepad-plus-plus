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

#include <functional>
#include <memory>

#include "Window.h"

class ColourPopup;

inline constexpr UINT CPN_COLOURPICKED = 0;

// A button showing a colour. A left click chooses another one from a popup, and a right click turns the colour on or off.
class ColourPicker : public Window
{
public:
	ColourPicker();
	~ColourPicker() override;
	void init(HINSTANCE hInst, HWND parent) override;
	void destroy() override;
	void setColour(COLORREF c) {
		_currentColour = c;
	}

	COLORREF getColour() const { return _currentColour; }
	bool isEnabled() const { return _isEnabled; }
	void setEnabled(bool enabled) { _isEnabled = enabled; }
	void disableRightClick() { _disableRightClick = true; }

	// Called when the user has changed the colour, or turned it on or off
	void setOnPicked(std::function<void()> onPicked) { _onPicked = std::move(onPicked); }

	void showPopup();
	void toggleEnabled();

private:
	void destroyColorPopup();
	void colourChosen(COLORREF colour);

	std::unique_ptr<ColourPopup> _pColourPopup;
	std::function<void()> _onPicked;
	COLORREF _currentColour = RGB(0xFF, 0x00, 0x00);
	bool _isEnabled = true;
	bool _disableRightClick = false;
};
