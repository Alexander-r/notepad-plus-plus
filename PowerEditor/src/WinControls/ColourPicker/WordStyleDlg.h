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

#include <memory>
#include <string>
#include <utility>

#include "ColourPicker.h"
#include "URLCtrl.h"
#include "Parameters.h"
#include "StaticDialog.h"
#include "resource.h"

#define WM_UPDATESCINTILLAS      (WORDSTYLE_USER + 1) // WM_UPDATESCINTILLAS (BOOL doChangePanel, 0)
#define WM_UPDATEMAINMENUBITMAPS (WORDSTYLE_USER + 2)

enum fontStyleType {BOLD_STATUS, ITALIC_STATUS, UNDERLINE_STATUS};

class QAbstractButton;
class QComboBox;
class QLabel;
class QListWidget;
class QSlider;

class WordStyleDlg : public StaticDialog
{
public :
	WordStyleDlg() = default;

	void doDialog(bool isRTL = false);
	void destroy() override;
	void prepare2Cancel();
	void redraw(bool forceUpdate = false) const override;
	void restoreGlobalOverrideValues() const;
	void addLastThemeEntry() const;
	bool selectThemeByName(const wchar_t* themeName);
	void syncWithSelFgSingleColorCtrl();
	bool goToSection(const wchar_t* sectionNames); // sectionNames is formed as following: "Language name:Style name"
	                                               // ex: "Global Styles:EOL custom color" will set Language on "Global Styles", then set Style on "EOL custom color" if both are found.

private :
	std::unique_ptr<ColourPicker> _pFgColour = nullptr;
	std::unique_ptr<ColourPicker> _pBgColour = nullptr;

    int _currentLexerIndex = 0;
	int _currentThemeIndex = 0;

	QAbstractButton* _hCheckBold = nullptr;
	QAbstractButton* _hCheckItalic = nullptr;
	QAbstractButton* _hCheckUnderline = nullptr;
	QComboBox* _hFontNameCombo = nullptr;
	QComboBox* _hFontSizeCombo = nullptr;
	QComboBox* _hSwitch2ThemeCombo = nullptr;
	QComboBox* _hLanguagesCombo = nullptr;
	QListWidget* _hStylesList = nullptr;

	QLabel* _hFgColourStaticText = nullptr;
	QLabel* _hBgColourStaticText = nullptr;
	QLabel* _hFontNameStaticText = nullptr;
	QLabel* _hFontSizeStaticText = nullptr;
	QLabel* _hStyleInfoStaticText = nullptr;

	URLCtrl _goToSettings;
	URLCtrl _globalOverrideLinkTip;

	LexerStylerArray _lsArray;
    StyleArray _globalStyles;
	std::wstring _themeName;

	LexerStylerArray _styles2restored;
	StyleArray _gstyles2restored;
	GlobalOverride _gOverride2restored;
	bool _restoreInvalid = false;

	bool _isDirty = false;
	bool _isThemeDirty = false;
	bool _isShownGOCtrls = false;
	bool _isThemeChanged = false;

	std::pair<intptr_t, intptr_t> goToPreferencesSettings();

	void createDialog(bool isRTL);
	void connectControls();
	void onCancel() override;
	void saveAndClose();
	void onColourPicked(bool which);
	void onFontStyleToggled(fontStyleType whichStyle);
	void updateStaticTexts();

	Style& getCurrentStyler();

	bool getStyleName(std::wstring& styleName) const;

	int whichTabColourIndex() const;
	int whichIndividualTabColourId();
	int whichFindDlgStatusMsgColourIndex();
	void apply(int applicationInfo);
	int getApplicationInfo() const;
	bool isDocumentMapStyle();
	void updateColour(bool which);
	void updateFontStyleStatus(fontStyleType whitchStyle);
	void updateExtension();
	void updateFontName();
	void updateFontSize();
	void updateUserKeywords();
	void switchToTheme();
	static void updateThemeName(const std::wstring& themeName);
	void loadLangListFromNppParam();
	void enableFontStyle(bool isEnable) const;
	long notifyDataModified();
	void setStyleListFromLexer(int index);
    void setVisualFromStyleList();
	void updateGlobalOverrideCtrls();
	void showGlobalOverrideCtrls(bool show);
	void applyCurrentSelectedThemeAndUpdateUI();
};
