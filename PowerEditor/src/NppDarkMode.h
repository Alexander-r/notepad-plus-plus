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

#include <string>

#include "NppMessages.h"
#include "NppPlatform.h"

enum class FluentColor;
struct TbIconInfo;
struct AdvancedOptions;

namespace NppDarkMode
{
	struct Colors
	{
		COLORREF background = 0;
		COLORREF softerBackground = 0; // ctrl background color
		COLORREF hotBackground = 0;
		COLORREF pureBackground = 0;   // dlg background color
		COLORREF errorBackground = 0;
		COLORREF text = 0;
		COLORREF darkerText = 0;
		COLORREF disabledText = 0;
		COLORREF linkText = 0;
		COLORREF edge = 0;
		COLORREF hotEdge = 0;
		COLORREF disabledEdge = 0;
	};

	struct Options
	{
		bool enable = false;
		bool enablePlugin = false;
	};

	enum ColorTone {
		blackTone  = 0,
		redTone    = 1,
		greenTone  = 2,
		blueTone   = 3,
		purpleTone = 4,
		cyanTone   = 5,
		oliveTone  = 6,
		customizedTone = 32
	};

	void initDarkMode();				// pulls options from NppParameters, and applies them to the application palette
	void refreshDarkMode(HWND hwnd, bool forceRefresh = false);	// attempts to apply new options from NppParameters

	void initAdvancedOptions();

	bool isEnabled();
	bool isEnabledForPlugins();

	bool isWindowsModeEnabled(); // whether to follow the desktop's light or dark preference
	void setWindowsMode(bool enable);
	std::wstring getThemeName();
	void setThemeName(const std::wstring& newThemeName);
	TbIconInfo getToolbarIconInfo(bool useDark);
	TbIconInfo getToolbarIconInfo();
	void setToolbarIconSet(int state2Set, bool useDark);
	void setToolbarIconSet(int state2Set);
	void setToolbarFluentColor(FluentColor color2Set, bool useDark);
	void setToolbarFluentColor(FluentColor color2Set);
	void setToolbarFluentMonochrome(bool setMonochrome, bool useDark);
	void setToolbarFluentMonochrome(bool setMonochrome);
	void setToolbarFluentCustomColor(COLORREF color, bool useDark);
	void setToolbarFluentCustomColor(COLORREF color);
	int getTabIconSet(bool useDark);
	void setTabIconSet(bool useAltIcons, bool useDark);
	bool useTabTheme();
	void setAdvancedOptions();

	COLORREF invertLightness(COLORREF c);
	double calculatePerceivedLightness(COLORREF c);

	void setDarkTone(ColorTone colorToneChoice);

	COLORREF getAccentColor(bool useDark);
	COLORREF getAccentColor();

	COLORREF getBackgroundColor();
	COLORREF getCtrlBackgroundColor();
	COLORREF getHotBackgroundColor();
	COLORREF getDlgBackgroundColor();
	COLORREF getErrorBackgroundColor();

	bool isDesktopDark();
	bool desktopPrefersDark();

	// Whether the controls are dark. What is drawn on them goes by this, which can differ from isEnabled(), the mode,
	// for a tone or a theme that does not suit it.
	bool usesDarkWidgets();

	// The WCAG contrast of two colours, from 1 to 21, and the first one moved towards black or white until it has that
	// much contrast with the second
	double contrastRatio(COLORREF first, COLORREF second);
	COLORREF readableOn(COLORREF colour, COLORREF background, double minimumContrast = 3.0);

	COLORREF getTextColor();
	COLORREF getDarkerTextColor();
	COLORREF getDisabledTextColor();
	COLORREF getLinkTextColor();

	COLORREF getEdgeColor();
	COLORREF getHotEdgeColor();
	COLORREF getDisabledEdgeColor();

	void setBackgroundColor(COLORREF c);
	void setCtrlBackgroundColor(COLORREF c);
	void setHotBackgroundColor(COLORREF c);
	void setDlgBackgroundColor(COLORREF c);
	void setErrorBackgroundColor(COLORREF c);
	void setTextColor(COLORREF c);
	void setDarkerTextColor(COLORREF c);
	void setDisabledTextColor(COLORREF c);
	void setLinkTextColor(COLORREF c);
	void setEdgeColor(COLORREF c);
	void setHotEdgeColor(COLORREF c);
	void setDisabledEdgeColor(COLORREF c);

	Colors getDarkModeDefaultColors(ColorTone colorTone = ColorTone::blackTone);
	void changeCustomTheme(const Colors& colors);

	bool isThemeDark();

	int darkMessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType);
}
