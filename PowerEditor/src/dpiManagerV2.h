// This file is part of Notepad++ project
// Copyright (c) 2024 ozone10 and Notepad++ team

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
// along with this program. If not, see <https://www.gnu.org/licenses/>.


#pragma once

#include "NppPlatform.h"

// Qt lays out in device-independent pixels and scales windows between screens itself, so unlike on
// Win32 there is nothing to convert: 96 is the logical DPI of those pixels everywhere.
class DPIManagerV2
{
public:
	DPIManagerV2() = default;

	static constexpr UINT USER_DEFAULT_SCREEN_DPI = 96;

	static UINT getDpiForWindow(HWND) { return USER_DEFAULT_SCREEN_DPI; }
	static UINT getDpiForParent(HWND) { return USER_DEFAULT_SCREEN_DPI; }
	static UINT getDpiForSystem() { return USER_DEFAULT_SCREEN_DPI; }

	void setDpi(HWND) {}
	void setDpi(UINT) {}
	UINT getDpi() const { return USER_DEFAULT_SCREEN_DPI; }

	static int scale(int x, UINT, UINT) { return x; }
	static int scale(int x, UINT) { return x; }
	static int scale(int x, HWND) { return x; }
	static int unscale(int x, UINT) { return x; }
	static int unscale(int x, HWND) { return x; }
	int scale(int x) const { return x; }
	int unscale(int x) const { return x; }

	// Font sizes in the configuration are given in points, and they mean the same as a Qt point size.
	static int scaleFontForFactor(int pt) { return pt; }
	static int scaleFont(int pt, UINT) { return pt; }
	static int scaleFont(int pt, HWND) { return pt; }
	int scaleFont(int pt) const { return pt; }
};
