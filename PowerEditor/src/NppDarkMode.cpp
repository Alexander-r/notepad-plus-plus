// This file is part of Notepad++ project
// Copyright (C)2021 adzm / Adam D. Walling

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

#include "NppDarkMode.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QMessageBox>
#include <QPalette>
#include <QProxyStyle>
#include <QStyle>
#include <QStyleOption>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVariant>
#include <QStyleHints>
#include <QWidget>

#include <algorithm>
#include <cmath>

#include "Parameters.h"
#include "resource.h"

namespace NppDarkMode
{
	// Colors are written as 0xRRGGBB below
	static constexpr COLORREF HEXRGB(DWORD rrggbb)
	{
		return RGB((rrggbb >> 16) & 0xFF, (rrggbb >> 8) & 0xFF, rrggbb & 0xFF);
	}



	static constexpr Colors darkColors{
		HEXRGB(0x202020),   // background
		HEXRGB(0x383838),   // softerBackground
		HEXRGB(0x454545),   // hotBackground
		HEXRGB(0x202020),   // pureBackground
		HEXRGB(0xB00000),   // errorBackground
		HEXRGB(0xE0E0E0),   // textColor
		HEXRGB(0xC0C0C0),   // darkerTextColor
		HEXRGB(0x808080),   // disabledTextColor
		HEXRGB(0xFFFF00),   // linkTextColor
		HEXRGB(0x646464),   // edgeColor
		HEXRGB(0x9B9B9B),   // hotEdgeColor
		HEXRGB(0x484848)    // disabledEdgeColor
	};

	static constexpr int offsetEdge = HEXRGB(0x1C1C1C);

	// red tone
	static constexpr int offsetRed = HEXRGB(0x100000);
	static constexpr Colors darkRedColors{
		darkColors.background + offsetRed,
		darkColors.softerBackground + offsetRed,
		darkColors.hotBackground + offsetRed,
		darkColors.pureBackground + offsetRed,
		darkColors.errorBackground,
		darkColors.text,
		darkColors.darkerText,
		darkColors.disabledText,
		darkColors.linkText,
		darkColors.edge + offsetEdge + offsetRed,
		darkColors.hotEdge + offsetRed,
		darkColors.disabledEdge + offsetRed
	};

	// green tone
	static constexpr int offsetGreen = HEXRGB(0x001000);
	static constexpr Colors darkGreenColors{
		darkColors.background + offsetGreen,
		darkColors.softerBackground + offsetGreen,
		darkColors.hotBackground + offsetGreen,
		darkColors.pureBackground + offsetGreen,
		darkColors.errorBackground,
		darkColors.text,
		darkColors.darkerText,
		darkColors.disabledText,
		darkColors.linkText,
		darkColors.edge + offsetEdge + offsetGreen,
		darkColors.hotEdge + offsetGreen,
		darkColors.disabledEdge + offsetGreen
	};

	// blue tone
	static constexpr int offsetBlue = HEXRGB(0x000020);
	static constexpr Colors darkBlueColors{
		darkColors.background + offsetBlue,
		darkColors.softerBackground + offsetBlue,
		darkColors.hotBackground + offsetBlue,
		darkColors.pureBackground + offsetBlue,
		darkColors.errorBackground,
		darkColors.text,
		darkColors.darkerText,
		darkColors.disabledText,
		darkColors.linkText,
		darkColors.edge + offsetEdge + offsetBlue,
		darkColors.hotEdge + offsetBlue,
		darkColors.disabledEdge + offsetBlue
	};

	// purple tone
	static constexpr int offsetPurple = HEXRGB(0x100020);
	static constexpr Colors darkPurpleColors{
		darkColors.background + offsetPurple,
		darkColors.softerBackground + offsetPurple,
		darkColors.hotBackground + offsetPurple,
		darkColors.pureBackground + offsetPurple,
		darkColors.errorBackground,
		darkColors.text,
		darkColors.darkerText,
		darkColors.disabledText,
		darkColors.linkText,
		darkColors.edge + offsetEdge + offsetPurple,
		darkColors.hotEdge + offsetPurple,
		darkColors.disabledEdge + offsetPurple
	};

	// cyan tone
	static constexpr int offsetCyan = HEXRGB(0x001020);
	static constexpr Colors darkCyanColors{
		darkColors.background + offsetCyan,
		darkColors.softerBackground + offsetCyan,
		darkColors.hotBackground + offsetCyan,
		darkColors.pureBackground + offsetCyan,
		darkColors.errorBackground,
		darkColors.text,
		darkColors.darkerText,
		darkColors.disabledText,
		darkColors.linkText,
		darkColors.edge + offsetEdge + offsetCyan,
		darkColors.hotEdge + offsetCyan,
		darkColors.disabledEdge + offsetCyan
	};

	// olive tone
	static constexpr int offsetOlive = HEXRGB(0x101000);
	static constexpr Colors darkOliveColors{
		darkColors.background + offsetOlive,
		darkColors.softerBackground + offsetOlive,
		darkColors.hotBackground + offsetOlive,
		darkColors.pureBackground + offsetOlive,
		darkColors.errorBackground,
		darkColors.text,
		darkColors.darkerText,
		darkColors.disabledText,
		darkColors.linkText,
		darkColors.edge + offsetEdge + offsetOlive,
		darkColors.hotEdge + offsetOlive,
		darkColors.disabledEdge + offsetOlive
	};

	// customized
	static Colors darkCustomizedColors{ darkColors };

	static ColorTone g_colorToneChoice = blackTone;
	static Options _options;
	static ::AdvancedOptions g_advOptions;
	static COLORREF cAccentDark = g_cDefaultSecondaryDark;
	static COLORREF cAccentLight = g_cDefaultSecondaryLight;

	void setDarkTone(ColorTone colorToneChoice)
	{
		g_colorToneChoice = colorToneChoice;
	}

	// Dark mode on a dark desktop has the desktop's colours, unless the user chose a tone in the preferences
	static bool g_followsDesktop = false;
	static Colors g_desktopColors;

	static Colors& currentColors()
	{
		if (g_followsDesktop && g_colorToneChoice == blackTone)
			return g_desktopColors;

		static Colors tones[] = {darkColors, darkRedColors, darkGreenColors, darkBlueColors, darkPurpleColors, darkCyanColors, darkOliveColors};
		switch (g_colorToneChoice)
		{
			case redTone: return tones[1];
			case greenTone: return tones[2];
			case blueTone: return tones[3];
			case purpleTone: return tones[4];
			case cyanTone: return tones[5];
			case oliveTone: return tones[6];
			case customizedTone: return darkCustomizedColors;
			default: return tones[0];
		}
	}

	static QColor toQColor(COLORREF c)
	{
		return QColor(GetRValue(c), GetGValue(c), GetBValue(c));
	}

	static COLORREF fromQColor(const QColor& c)
	{
		return RGB(c.red(), c.green(), c.blue());
	}

	// The application's one style, for both light and dark mode so that they differ only in their colours
	class NppStyle : public QProxyStyle
	{
	public:
		explicit NppStyle(const QString& key) : QProxyStyle(key) {}

		QSize sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& size, const QWidget* widget) const override
		{
			QSize result = QProxyStyle::sizeFromContents(type, option, size, widget);

			// Breeze leaves a menu with short titles too narrow, so that the longest one runs into its submenu arrow
			const auto* item = qstyleoption_cast<const QStyleOptionMenuItem*>(option);
			if (type == CT_MenuItem && item && item->menuItemType == QStyleOptionMenuItem::SubMenu)
				result.rwidth() += pixelMetric(PM_SmallIconSize, option, widget);
			return result;
		}
	};

	// Breeze paints the menu bar and the tool bars from a KDE colour scheme and not from the palette. This is its way
	// for an application to have its own scheme. A new file each time, as KDE caches a scheme by its path.
	static void writeBreezeColourScheme(const Colors& colors, COLORREF accentColour)
	{
		static QTemporaryDir folder;
		static int generation = 0;
		if (!folder.isValid())
			return;

		const auto rgb = [](COLORREF colour)
		{
			return QStringLiteral("%1,%2,%3").arg(GetRValue(colour)).arg(GetGValue(colour)).arg(GetBValue(colour));
		};
		const QString accent = rgb(accentColour);

		QFile file(folder.filePath(QStringLiteral("dark-%1.colors").arg(++generation)));
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
			return;
		QTextStream scheme(&file);
		const auto group = [&](const char* name, COLORREF background, COLORREF alternate)
		{
			scheme << "[Colors:" << name << "]\n"
				<< "BackgroundNormal=" << rgb(background) << "\n"
				<< "BackgroundAlternate=" << rgb(alternate) << "\n"
				<< "ForegroundNormal=" << rgb(colors.text) << "\n"
				<< "ForegroundInactive=" << rgb(colors.darkerText) << "\n"
				<< "ForegroundActive=" << rgb(colors.text) << "\n"
				<< "ForegroundLink=" << rgb(colors.linkText) << "\n"
				<< "ForegroundVisited=" << rgb(colors.linkText) << "\n"
				<< "DecorationFocus=" << accent << "\n"
				<< "DecorationHover=" << accent << "\n\n";
		};
		for (const char* name : { "Window", "Header", "Complementary" })
			group(name, colors.pureBackground, colors.background);
		for (const char* name : { "Button", "View", "Tooltip" })
			group(name, colors.softerBackground, colors.background);
		group("Selection", accentColour, accentColour);
		file.close();

		qApp->setProperty("KDE_COLOR_SCHEME_PATH", file.fileName());
	}

	// Always the desktop's style, to look native. Set once, so that light and dark mode have the same layout.
	static void installStyle()
	{
		QApplication::setStyle(new NppStyle(QApplication::style() ? QApplication::style()->objectName() : QString()));
	}

	static Colors colorsOf(const QPalette& palette)
	{
		const auto toColor = [](const QColor& colour) { return fromQColor(colour); };
		Colors colors = darkColors; // no desktop has a colour for errors
		colors.pureBackground = toColor(palette.color(QPalette::Window));
		colors.background = toColor(palette.color(QPalette::AlternateBase));
		colors.softerBackground = toColor(palette.color(QPalette::Button));
		colors.hotBackground = toColor(palette.color(QPalette::Midlight));
		colors.text = toColor(palette.color(QPalette::WindowText));
		colors.darkerText = readableOn(toColor(palette.color(QPalette::Disabled, QPalette::WindowText)), colors.pureBackground, 4.5);
		colors.disabledText = toColor(palette.color(QPalette::Disabled, QPalette::WindowText));
		colors.linkText = toColor(palette.color(QPalette::Link));
		colors.edge = toColor(palette.color(QPalette::Mid));
		colors.hotEdge = toColor(palette.color(QPalette::Light));
		colors.disabledEdge = toColor(palette.color(QPalette::Mid));
		return colors;
	}

	// As the original does through the title bar, menu and control subclassing: one palette covers it here.
	// For light mode on a dark desktop, as the desktop has no light colours to take
	static Colors lightColors()
	{
		Colors colors = darkColors;
		colors.pureBackground = RGB(0xEF, 0xF0, 0xF1);
		colors.background = RGB(0xF7, 0xF7, 0xF7);
		colors.softerBackground = RGB(0xFF, 0xFF, 0xFF);
		colors.hotBackground = RGB(0xE0, 0xE1, 0xE3);
		colors.errorBackground = RGB(0xFF, 0xC0, 0xC0);
		colors.text = RGB(0x23, 0x26, 0x29);
		colors.darkerText = RGB(0x5A, 0x5E, 0x63);
		colors.disabledText = RGB(0xA0, 0xA2, 0xA5);
		colors.linkText = RGB(0x29, 0x80, 0xB9);
		colors.edge = RGB(0xB0, 0xB2, 0xB5);
		colors.hotEdge = RGB(0xFF, 0xFF, 0xFF);
		colors.disabledEdge = RGB(0xD0, 0xD1, 0xD3);
		return colors;
	}

	static void applyColours(const Colors& colors, COLORREF accent, bool isDark)
	{
		writeBreezeColourScheme(colors, accent);
		QPalette p;
		p.setColor(QPalette::Window, toQColor(colors.pureBackground));
		p.setColor(QPalette::WindowText, toQColor(colors.text));
		p.setColor(QPalette::Base, toQColor(colors.softerBackground));
		p.setColor(QPalette::AlternateBase, toQColor(colors.background));
		p.setColor(QPalette::ToolTipBase, toQColor(colors.softerBackground));
		p.setColor(QPalette::ToolTipText, toQColor(colors.text));
		p.setColor(QPalette::Text, toQColor(colors.text));
		p.setColor(QPalette::PlaceholderText, toQColor(colors.disabledText));
		p.setColor(QPalette::Button, toQColor(colors.softerBackground));
		p.setColor(QPalette::ButtonText, toQColor(colors.text));
		p.setColor(QPalette::BrightText, toQColor(isDark ? colors.text : colors.softerBackground));
		p.setColor(QPalette::Light, toQColor(colors.hotEdge));
		p.setColor(QPalette::Midlight, toQColor(colors.hotBackground));
		p.setColor(QPalette::Mid, toQColor(colors.edge));
		p.setColor(QPalette::Dark, isDark ? toQColor(colors.background) : QColor(0xA0, 0xA2, 0xA5));
		p.setColor(QPalette::Shadow, isDark ? toQColor(colors.background) : QColor(0x70, 0x72, 0x75));
		p.setColor(QPalette::Highlight, toQColor(accent));
		p.setColor(QPalette::HighlightedText, QColor(Qt::white));
		p.setColor(QPalette::Link, toQColor(colors.linkText));
		p.setColor(QPalette::LinkVisited, toQColor(colors.linkText));
		p.setColor(QPalette::Disabled, QPalette::WindowText, toQColor(colors.disabledText));
		p.setColor(QPalette::Disabled, QPalette::Text, toQColor(colors.disabledText));
		p.setColor(QPalette::Disabled, QPalette::ButtonText, toQColor(colors.disabledText));
		QApplication::setPalette(p);
	}

	static void applyPalette(bool useDark)
	{
		static const bool isStyleInstalled = (installStyle(), true);
		Q_UNUSED(isStyleInstalled);

		// Back to the desktop's palette, which an empty one means, so that it also follows a change of the desktop's theme
		qApp->setProperty("KDE_COLOR_SCHEME_PATH", QVariant());
		QApplication::setPalette(QPalette());

		const QPalette desktop = QApplication::palette();
		g_desktopColors = colorsOf(desktop);
		g_followsDesktop = desktop.color(QPalette::Window).lightness() < 128;

		// The desktop's colours when they suit the mode, which Follow system always gets. The mode that the desktop
		// has no colours for, or a tone that the user chose, has Notepad++'s own, in the desktop's style.
		if (!useDark)
		{
			if (g_followsDesktop)
				applyColours(lightColors(), getAccentColor(false), false);
			return;
		}

		if (!g_followsDesktop || g_colorToneChoice != blackTone)
			applyColours(currentColors(), getAccentColor(true), true);
	}

	static Options configuredOptions()
	{
		const NppGUI& nppGui = NppParameters::getInstance().getNppGUI();
		Options opt;
		opt.enable = nppGui._darkmode._isEnabled;
		opt.enablePlugin = nppGui._darkmode._isEnabledPlugin;

		g_colorToneChoice = nppGui._darkmode._colorTone;
		darkCustomizedColors = nppGui._darkmode._customColors;

		return opt;
	}

	// By its colours, as the desktop's dark themes do not all say so as a colour scheme. This puts back the desktop's
	// palette to see them, so a refresh has to follow when Notepad++ has its own.
	bool desktopPrefersDark()
	{
		qApp->setProperty("KDE_COLOR_SCHEME_PATH", QVariant());
		QApplication::setPalette(QPalette());
		return QApplication::palette().color(QPalette::Window).lightness() < 128;
	}

	void initDarkMode()
	{
		_options = configuredOptions();

		initAdvancedOptions();

		if (isWindowsModeEnabled()) // "follow the system" in the preferences
		{
			NppGUI& nppGUI = NppParameters::getInstance().getNppGUI();
			nppGUI._darkmode._isEnabled = desktopPrefersDark();
			_options.enable = nppGUI._darkmode._isEnabled;
		}

		// The accent color follows the desktop's highlight color
		const QColor accent = QApplication::palette().color(QPalette::Highlight);
		cAccentDark = accent.isValid() ? fromQColor(accent.lightness() < 120 ? accent.lighter(130) : accent) : g_cDefaultSecondaryDark;
		cAccentLight = accent.isValid() ? fromQColor(accent.lightness() > 150 ? accent.darker(130) : accent) : g_cDefaultSecondaryLight;

		applyPalette(_options.enable);
	}

	// attempts to apply new options from NppParameters, sends NPPM_INTERNAL_REFRESHDARKMODE to hwnd's top level
	void refreshDarkMode(HWND hwnd, bool forceRefresh)
	{
		const Options config = configuredOptions();
		const bool changed = _options.enable != config.enable;
		_options.enable = config.enable;

		if (!changed && !forceRefresh)
			return; // nothing to refresh

		applyPalette(_options.enable);

		HWND root = hwnd ? hwnd->window() : nullptr;
		if (root && root->parentWidget())
			root = root->parentWidget()->window();
		// wParam == true, will reset style and toolbar icon
		::SendMessage(root, NPPM_INTERNAL_REFRESHDARKMODE, static_cast<WPARAM>(!forceRefresh), 0);
	}

	void initAdvancedOptions()
	{
		g_advOptions = NppParameters::getInstance().getNppGUI()._darkmode._advOptions;
	}

	bool isEnabled()
	{
		return _options.enable;
	}

	bool isEnabledForPlugins()
	{
		return _options.enablePlugin;
	}

	bool isWindowsModeEnabled()
	{
		return g_advOptions._enableWindowsMode;
	}

	void setWindowsMode(bool enable)
	{
		g_advOptions._enableWindowsMode = enable;
	}

	void setThemeName(const std::wstring& newThemeName)
	{
		if (NppDarkMode::isEnabled())
			g_advOptions._darkDefaults._xmlFileName = newThemeName;
		else
			g_advOptions._lightDefaults._xmlFileName = newThemeName;
	}

	std::wstring getThemeName()
	{
		auto& theme = NppDarkMode::isEnabled() ? g_advOptions._darkDefaults._xmlFileName : g_advOptions._lightDefaults._xmlFileName;
		return (lstrcmp(theme.c_str(), L"stylers.xml") == 0) ? L"" : theme;
	}

	TbIconInfo getToolbarIconInfo(bool useDark)
	{
		auto& toolbarInfo = useDark ? g_advOptions._darkDefaults._tbIconInfo
			: g_advOptions._lightDefaults._tbIconInfo;

		if (toolbarInfo._tbCustomColor == 0)
			toolbarInfo._tbCustomColor = NppDarkMode::getAccentColor(useDark);

		return toolbarInfo;
	}

	TbIconInfo getToolbarIconInfo()
	{
		return NppDarkMode::getToolbarIconInfo(NppDarkMode::isEnabled());
	}

	void setToolbarIconSet(int state2Set, bool useDark)
	{
		if (useDark)
			g_advOptions._darkDefaults._tbIconInfo._tbIconSet = static_cast<toolBarStatusType>(state2Set);
		else
			g_advOptions._lightDefaults._tbIconInfo._tbIconSet = static_cast<toolBarStatusType>(state2Set);
	}

	void setToolbarIconSet(int state2Set)
	{
		NppDarkMode::setToolbarIconSet(state2Set, NppDarkMode::isEnabled());
	}

	void setToolbarFluentColor(FluentColor color2Set, bool useDark)
	{
		if (useDark)
			g_advOptions._darkDefaults._tbIconInfo._tbColor = color2Set;
		else
			g_advOptions._lightDefaults._tbIconInfo._tbColor = color2Set;
	}

	void setToolbarFluentColor(FluentColor color2Set)
	{
		NppDarkMode::setToolbarFluentColor(color2Set, NppDarkMode::isEnabled());
	}

	void setToolbarFluentMonochrome(bool setMonochrome, bool useDark)
	{
		if (useDark)
			g_advOptions._darkDefaults._tbIconInfo._tbUseMono = setMonochrome;
		else
			g_advOptions._lightDefaults._tbIconInfo._tbUseMono = setMonochrome;
	}

	void setToolbarFluentMonochrome(bool setMonochrome)
	{
		NppDarkMode::setToolbarFluentMonochrome(setMonochrome, NppDarkMode::isEnabled());
	}

	void setToolbarFluentCustomColor(COLORREF color, bool useDark)
	{
		if (useDark)
			g_advOptions._darkDefaults._tbIconInfo._tbCustomColor = color;
		else
			g_advOptions._lightDefaults._tbIconInfo._tbCustomColor = color;
	}

	void setToolbarFluentCustomColor(COLORREF color)
	{
		NppDarkMode::setToolbarFluentCustomColor(color, NppDarkMode::isEnabled());
	}

	void setTabIconSet(bool useAltIcons, bool useDark)
	{
		if (useDark)
			g_advOptions._darkDefaults._tabIconSet = useAltIcons ? 1 : 2;
		else
			g_advOptions._lightDefaults._tabIconSet = useAltIcons ? 1 : 0;
	}

	int getTabIconSet(bool useDark)
	{
		return useDark ? g_advOptions._darkDefaults._tabIconSet : g_advOptions._lightDefaults._tabIconSet;
	}

	bool useTabTheme()
	{
		return NppDarkMode::isEnabled() ? g_advOptions._darkDefaults._tabUseTheme : g_advOptions._lightDefaults._tabUseTheme;
	}

	void setAdvancedOptions()
	{
		NppParameters::getInstance().getNppGUI()._darkmode._advOptions = g_advOptions;
	}

	COLORREF invertLightness(COLORREF c)
	{
		int h = 0;
		int s = 0;
		int l = 0;
		toQColor(c).getHsl(&h, &s, &l);
		return fromQColor(QColor::fromHsl(h, s, 255 - l));
	}

	double calculatePerceivedLightness(COLORREF c)
	{
		auto linearValue = [](double colorChannel) -> double
		{
			colorChannel /= 255.0;
			if (colorChannel <= 0.04045)
				return colorChannel / 12.92;
			return std::pow(((colorChannel + 0.055) / 1.055), 2.4);
		};

		double r = linearValue(static_cast<double>(GetRValue(c)));
		double g = linearValue(static_cast<double>(GetGValue(c)));
		double b = linearValue(static_cast<double>(GetBValue(c)));

		double luminance = 0.2126 * r + 0.7152 * g + 0.0722 * b;

		double lightness = (luminance <= 216.0 / 24389.0) ? (luminance * 24389.0 / 27.0) : (std::pow(luminance, (1.0 / 3.0)) * 116.0 - 16.0);
		return lightness;
	}

	COLORREF getAccentColor(bool useDark)
	{
		return useDark ? cAccentDark : cAccentLight;
	}

	COLORREF getAccentColor()
	{
		return getAccentColor(isEnabled());
	}

	COLORREF getBackgroundColor() { return currentColors().background; }
	COLORREF getCtrlBackgroundColor() { return currentColors().softerBackground; }
	COLORREF getHotBackgroundColor() { return currentColors().hotBackground; }
	COLORREF getDlgBackgroundColor() { return currentColors().pureBackground; }
	COLORREF getErrorBackgroundColor() { return currentColors().errorBackground; }
	bool isDesktopDark()
	{
		return g_followsDesktop;
	}

	bool usesDarkWidgets()
	{
		return QApplication::palette().color(QPalette::Window).lightness() < 128;
	}

	double contrastRatio(COLORREF first, COLORREF second)
	{
		const auto luminance = [](COLORREF colour)
		{
			const auto channel = [](int value)
			{
				const double part = value / 255.0;
				return part <= 0.03928 ? part / 12.92 : std::pow((part + 0.055) / 1.055, 2.4);
			};
			return 0.2126 * channel(GetRValue(colour)) + 0.7152 * channel(GetGValue(colour)) + 0.0722 * channel(GetBValue(colour));
		};
		const double a = luminance(first);
		const double b = luminance(second);
		return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
	}

	COLORREF readableOn(COLORREF colour, COLORREF background, double minimumContrast)
	{
		const COLORREF target = (contrastRatio(RGB(0, 0, 0), background) >= contrastRatio(RGB(255, 255, 255), background)) ? RGB(0, 0, 0) : RGB(255, 255, 255);
		COLORREF result = colour;
		for (int percent = 10; percent <= 100 && contrastRatio(result, background) < minimumContrast; percent += 10)
		{
			const auto mix = [percent](int from, int to) { return static_cast<BYTE>(from + (to - from) * percent / 100); };
			result = RGB(mix(GetRValue(colour), GetRValue(target)), mix(GetGValue(colour), GetGValue(target)), mix(GetBValue(colour), GetBValue(target)));
		}
		return result;
	}

	COLORREF getTextColor() { return currentColors().text; }
	COLORREF getDarkerTextColor() { return currentColors().darkerText; }
	COLORREF getDisabledTextColor() { return currentColors().disabledText; }
	COLORREF getLinkTextColor() { return currentColors().linkText; }
	COLORREF getEdgeColor() { return currentColors().edge; }
	COLORREF getHotEdgeColor() { return currentColors().hotEdge; }
	COLORREF getDisabledEdgeColor() { return currentColors().disabledEdge; }

	void setBackgroundColor(COLORREF c) { currentColors().background = c; }
	void setCtrlBackgroundColor(COLORREF c) { currentColors().softerBackground = c; }
	void setHotBackgroundColor(COLORREF c) { currentColors().hotBackground = c; }
	void setDlgBackgroundColor(COLORREF c) { currentColors().pureBackground = c; }
	void setErrorBackgroundColor(COLORREF c) { currentColors().errorBackground = c; }
	void setTextColor(COLORREF c) { currentColors().text = c; }
	void setDarkerTextColor(COLORREF c) { currentColors().darkerText = c; }
	void setDisabledTextColor(COLORREF c) { currentColors().disabledText = c; }
	void setLinkTextColor(COLORREF c) { currentColors().linkText = c; }
	void setEdgeColor(COLORREF c) { currentColors().edge = c; }
	void setHotEdgeColor(COLORREF c) { currentColors().hotEdge = c; }
	void setDisabledEdgeColor(COLORREF c) { currentColors().disabledEdge = c; }

	Colors getDarkModeDefaultColors(ColorTone colorTone)
	{
		switch (colorTone)
		{
			case NppDarkMode::ColorTone::redTone:
				return darkRedColors;

			case NppDarkMode::ColorTone::greenTone:
				return darkGreenColors;

			case NppDarkMode::ColorTone::blueTone:
				return darkBlueColors;

			case NppDarkMode::ColorTone::purpleTone:
				return darkPurpleColors;

			case NppDarkMode::ColorTone::cyanTone:
				return darkCyanColors;

			case NppDarkMode::ColorTone::oliveTone:
				return darkOliveColors;

			case NppDarkMode::ColorTone::customizedTone:
			case NppDarkMode::ColorTone::blackTone:
			default:
				return darkColors;
		}
	}

	void changeCustomTheme(const Colors& colors)
	{
		darkCustomizedColors = colors;
	}

	bool isThemeDark()
	{
		constexpr double middleGrayRange = 2.0;
		return calculatePerceivedLightness(NppParameters::getInstance().getCurrentDefaultBgColor()) < (50.0 - middleGrayRange);
	}

	int darkMessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType)
	{
		QMessageBox box(hWnd);
		box.setWindowTitle(QString::fromWCharArray(lpCaption ? lpCaption : L""));
		box.setText(QString::fromWCharArray(lpText ? lpText : L""));

		switch (uType & MB_ICONMASK)
		{
			case MB_ICONERROR: box.setIcon(QMessageBox::Critical); break;
			case MB_ICONWARNING: box.setIcon(QMessageBox::Warning); break;
			case MB_ICONQUESTION: box.setIcon(QMessageBox::Question); break;
			case MB_ICONINFORMATION: box.setIcon(QMessageBox::Information); break;
			default: break;
		}

		QMessageBox::StandardButton defaultOrder[3] = {QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton};
		switch (uType & MB_TYPEMASK)
		{
			case MB_OKCANCEL: defaultOrder[0] = QMessageBox::Ok; defaultOrder[1] = QMessageBox::Cancel; break;
			case MB_ABORTRETRYIGNORE: defaultOrder[0] = QMessageBox::Abort; defaultOrder[1] = QMessageBox::Retry; defaultOrder[2] = QMessageBox::Ignore; break;
			case MB_YESNOCANCEL: defaultOrder[0] = QMessageBox::Yes; defaultOrder[1] = QMessageBox::No; defaultOrder[2] = QMessageBox::Cancel; break;
			case MB_YESNO: defaultOrder[0] = QMessageBox::Yes; defaultOrder[1] = QMessageBox::No; break;
			case MB_RETRYCANCEL: defaultOrder[0] = QMessageBox::Retry; defaultOrder[1] = QMessageBox::Cancel; break;
			case MB_CANCELTRYCONTINUE: defaultOrder[0] = QMessageBox::Cancel; defaultOrder[1] = QMessageBox::Retry; defaultOrder[2] = QMessageBox::Ignore; break;
			default: break;
		}
		QMessageBox::StandardButtons buttons;
		for (const auto b : defaultOrder)
			buttons |= b;
		box.setStandardButtons(buttons);

		const int defaultIndex = (uType & MB_DEFBUTTON3) == MB_DEFBUTTON3 ? 2 : ((uType & MB_DEFBUTTON2) ? 1 : 0);
		if (defaultOrder[defaultIndex] != QMessageBox::NoButton)
			box.setDefaultButton(defaultOrder[defaultIndex]);

		switch (box.exec())
		{
			case QMessageBox::Ok: return IDOK;
			case QMessageBox::Cancel: return IDCANCEL;
			case QMessageBox::Abort: return IDABORT;
			case QMessageBox::Retry: return IDRETRY;
			case QMessageBox::Ignore: return IDIGNORE;
			case QMessageBox::Yes: return IDYES;
			case QMessageBox::No: return IDNO;
			default: return IDCANCEL;
		}
	}
}
