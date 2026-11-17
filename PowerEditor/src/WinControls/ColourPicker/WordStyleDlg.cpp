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


#include "WordStyleDlg.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>

#include <cstdlib>
#include <cwchar>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <QFileInfo>

#include "ColourPicker.h"
#include "Common.h"
#include "Notepad_plus_msgs.h"
#include "NppConstants.h"
#include "NppDarkMode.h"
#include "Parameters.h"
#include "StaticDialog.h"
#include "TabBar.h"
#include "WordStyleDlgRes.h"
#include "documentMap.h"
#include "localization.h"
#include "preference_rc.h"
#include "resource.h"
#include "ui_StylerDlg.h"

// The following parameters are for apply() method which will re-initialize the followings GUI with modified styler:
// 2 Scintilla edit zones, Search result (displayed by Sintilla), Notepad++ GUI & components concerning theme
enum ApplyChange
{
	NO_VISUAL_CHANGE    = 0x00, // No need to apply visual effect - User ext.
	GENERAL_CHANGE      = 0x01, // For Sintilla zones & Notepad++ GUI (Tabbar, Find dialog, etc...)
	THEME_CHANGE        = 0x02, // For the components concerning theme, for example the background color of dockable panels 
	COLOR_CHANGE_4_MENU = 0x04, // For the color items displayed on the menu
};

static constexpr bool C_FOREGROUND = false;
static constexpr bool C_BACKGROUND = true;

using namespace std;

namespace
{
	// A colour with the value 0xFF in its top byte is not set
	bool isColourSet(COLORREF colour)
	{
		return ((colour >> 24) & 0xFF) != 0xFF;
	}

	// The first item starting with the text, after the given position and wrapping around
	int findPrefix(const QComboBox* combo, const wchar_t* text, int after)
	{
		const QString prefix = QString::fromWCharArray(text);
		const int count = combo->count();
		for (int i = 1; i <= count; ++i)
		{
			const int position = (after + i) % count;
			if (combo->itemText(position).startsWith(prefix, Qt::CaseInsensitive))
				return position;
		}
		return -1;
	}
}

void WordStyleDlg::updateGlobalOverrideCtrls()
{
	const NppGUI & nppGUI = (NppParameters::getInstance()).getNppGUI();
	setChecked(IDC_GLOBAL_FG_CHECK, nppGUI._globalOverride.enableFg);
	setChecked(IDC_GLOBAL_BG_CHECK, nppGUI._globalOverride.enableBg);
	setChecked(IDC_GLOBAL_FONT_CHECK, nppGUI._globalOverride.enableFont);
	setChecked(IDC_GLOBAL_FONTSIZE_CHECK, nppGUI._globalOverride.enableFontSize);
	setChecked(IDC_GLOBAL_BOLD_CHECK, nppGUI._globalOverride.enableBold);
	setChecked(IDC_GLOBAL_ITALIC_CHECK, nppGUI._globalOverride.enableItalic);
	setChecked(IDC_GLOBAL_UNDERLINE_CHECK, nppGUI._globalOverride.enableUnderLine);
}

void WordStyleDlg::createDialog(bool isRTL)
{
	QDialog* dialog = createDialogWidget(isRTL);
	Ui::StylerDlg ui;
	ui.setupUi(dialog);

	NppParameters& nppParamInst = NppParameters::getInstance();

	_hCheckBold = ui.boldCheck;
	_hCheckItalic = ui.italicCheck;
	_hCheckUnderline = ui.underlineCheck;
	_hFontNameCombo = ui.fontCombo;
	_hFontSizeCombo = ui.fontsizeCombo;
	_hSwitch2ThemeCombo = ui.switch2themeCombo;
	_hLanguagesCombo = ui.languagesCombo;
	_hStylesList = ui.stylesList;
	_hFgColourStaticText = ui.fgStatic;
	_hBgColourStaticText = ui.bgStatic;
	_hFontNameStaticText = ui.fontnameStatic;
	_hFontSizeStaticText = ui.fontsizeStatic;
	_hStyleInfoStaticText = ui.styledescriptionStatic;

	QPalette infoColours = _hStyleInfoStaticText->palette();
	infoColours.setColor(QPalette::WindowText, infoColours.color(QPalette::Link));
	_hStyleInfoStaticText->setPalette(infoColours);

	_currentThemeIndex = -1;
	int defaultThemeIndex = 0;
	ThemeSwitcher & themeSwitcher = nppParamInst.getThemeSwitcher();
	for (size_t i = 0 ; i < themeSwitcher.size() ; ++i)
	{
		pair<wstring, wstring> & themeInfo = themeSwitcher.getElementFromIndex(i);
		_hSwitch2ThemeCombo->addItem(QString::fromStdWString(themeInfo.first));
		const int j = _hSwitch2ThemeCombo->count() - 1;
		if (! themeInfo.second.compare( nppParamInst.getNppGUI()._themeName ) )
		{
			_currentThemeIndex = j;
			_themeName.assign(themeInfo.second);
		}
		if (! themeInfo.first.compare(L"Default") )
		{
			defaultThemeIndex = j;
		}
	}
	if (_currentThemeIndex == -1)
	{
		_currentThemeIndex = defaultThemeIndex;
	}
	_hSwitch2ThemeCombo->setCurrentIndex(_currentThemeIndex);

	for (const wchar_t* fontSize : fontSizeStrs)
		_hFontSizeCombo->addItem(QString::fromWCharArray(fontSize));

	const std::vector<wstring> & fontlist = nppParamInst.getFontList();
	for (size_t i = 0, len = fontlist.size() ; i < len ; ++i)
		_hFontNameCombo->addItem(QString::fromStdWString(fontlist[i]));
	_hFontNameCombo->model()->sort(0);

	_pFgColour = std::make_unique<ColourPicker>();
	_pBgColour = std::make_unique<ColourPicker>();
	_pFgColour->init(_hInst, ui.colourgroupStatic);
	_pBgColour->init(_hInst, ui.colourgroupStatic);
	_pFgColour->getHSelf()->setObjectName(QStringLiteral("fgColour"));
	_pBgColour->getHSelf()->setObjectName(QStringLiteral("bgColour"));
	_pFgColour->setOnPicked([this]() { onColourPicked(C_FOREGROUND); });
	_pBgColour->setOnPicked([this]() { onColourPicked(C_BACKGROUND); });
	ui.colourGrid->addWidget(_pFgColour->getHSelf(), 0, 1);
	ui.colourGrid->addWidget(_pBgColour->getHSelf(), 1, 1);

	_pFgColour->display();
	_pBgColour->display();

	enableItem(IDC_SAVECLOSE_BUTTON, false);

	_goToSettings.init(_hInst, dialog);
	_goToSettings.create(ui.globalGotosettingsLink, [this]()
	{
		const std::pair<intptr_t, intptr_t> pageAndCtrlID = goToPreferencesSettings();
		if (pageAndCtrlID.first != -1)
			::SendMessage(_hParent, NPPM_INTERNAL_LAUNCHPREFERENCES, pageAndCtrlID.first, pageAndCtrlID.second);
	});

	_globalOverrideLinkTip.init(_hInst, dialog);
	_globalOverrideLinkTip.create(ui.globalWhatisglobaloverrideLink, std::function<void()>{});

	NativeLangSpeaker* pNativeSpeaker = nppParamInst.getNativeLangSpeaker();
	wstring globalOverrideTipStr = pNativeSpeaker->getLocalizedStrFromID("global-override-tip", L"Enabling \"Global override\" here will override that parameter in all language styles. What you probably really want is to use the \"Default Style\" settings instead");
	ui.globalWhatisglobaloverrideLink->setToolTip(QString::fromStdWString(globalOverrideTipStr));

	loadLangListFromNppParam();
	updateGlobalOverrideCtrls();
	setVisualFromStyleList();

	const bool isTransparentAvailable = NppParameters::isWindowOpacityAvailable();
	showItem(IDC_SC_TRANSPARENT_CHECK, isTransparentAvailable);
	showItem(IDC_SC_PERCENTAGE_SLIDER, isTransparentAvailable);
	ui.scPercentageSlider->setRange(20, 200);
	ui.scPercentageSlider->setValue(150);
	ui.scPercentageSlider->setEnabled(false);

	connectControls();
	goToCenter();
}

void WordStyleDlg::connectControls()
{
	const auto onChecked = [this](int id, auto handler)
	{
		QObject::connect(button(id), &QAbstractButton::clicked, _hSelf, handler);
	};
	onChecked(IDC_BOLD_CHECK, [this]() { onFontStyleToggled(BOLD_STATUS); });
	onChecked(IDC_ITALIC_CHECK, [this]() { onFontStyleToggled(ITALIC_STATUS); });
	onChecked(IDC_UNDERLINE_CHECK, [this]() { onFontStyleToggled(UNDERLINE_STATUS); });
	onChecked(IDCANCEL, [this]() { onCancel(); });
	onChecked(IDC_SAVECLOSE_BUTTON, [this]() { saveAndClose(); });

	auto* slider = qobject_cast<QSlider*>(dlgItem(IDC_SC_PERCENTAGE_SLIDER));
	QObject::connect(slider, &QSlider::valueChanged, _hSelf, [this](int percent) { NppParameters::setTransparent(_hSelf, percent); });
	onChecked(IDC_SC_TRANSPARENT_CHECK, [this, slider]()
	{
		const bool isChecked = isCheckedOrNot(IDC_SC_TRANSPARENT_CHECK);
		if (isChecked)
			NppParameters::setTransparent(_hSelf, slider->value());
		else
			NppParameters::removeTransparent(_hSelf);

		slider->setEnabled(isChecked);
	});

	const auto onOverride = [this, onChecked](int id, bool GlobalOverride::*setting)
	{
		onChecked(id, [this, id, setting]()
		{
			GlobalOverride & glo = (NppParameters::getInstance()).getGlobalOverrideStyle();
			glo.*setting = isCheckedOrNot(id);
			notifyDataModified();
			apply(GENERAL_CHANGE);
		});
	};
	onOverride(IDC_GLOBAL_FG_CHECK, &GlobalOverride::enableFg);
	onOverride(IDC_GLOBAL_BG_CHECK, &GlobalOverride::enableBg);
	onOverride(IDC_GLOBAL_FONT_CHECK, &GlobalOverride::enableFont);
	onOverride(IDC_GLOBAL_FONTSIZE_CHECK, &GlobalOverride::enableFontSize);
	onOverride(IDC_GLOBAL_BOLD_CHECK, &GlobalOverride::enableBold);
	onOverride(IDC_GLOBAL_ITALIC_CHECK, &GlobalOverride::enableItalic);
	onOverride(IDC_GLOBAL_UNDERLINE_CHECK, &GlobalOverride::enableUnderLine);

	QObject::connect(_hFontNameCombo, &QComboBox::activated, _hSelf, [this]()
	{
		updateFontName();
		notifyDataModified();
		apply(GENERAL_CHANGE);
	});
	QObject::connect(_hFontSizeCombo, &QComboBox::activated, _hSelf, [this]()
	{
		updateFontSize();
		notifyDataModified();
		apply(GENERAL_CHANGE);
	});
	QObject::connect(_hLanguagesCombo, &QComboBox::activated, _hSelf, [this](int i)
	{
		bool prevThemeState = _isThemeDirty;
		setStyleListFromLexer(i);
		_isThemeDirty = prevThemeState;
	});
	QObject::connect(_hStylesList, &QListWidget::currentRowChanged, _hSelf, [this]() { setVisualFromStyleList(); });
	QObject::connect(_hSwitch2ThemeCombo, &QComboBox::activated, _hSelf, [this]() { applyCurrentSelectedThemeAndUpdateUI(); });

	// Filling these from a style does not change it, so that is done with their signals blocked
	QObject::connect(qobject_cast<QPlainTextEdit*>(dlgItem(IDC_USER_KEYWORDS_EDIT)), &QPlainTextEdit::textChanged, _hSelf, [this]()
	{
		updateUserKeywords();
		notifyDataModified();
		apply(GENERAL_CHANGE);
	});
	QObject::connect(qobject_cast<QLineEdit*>(dlgItem(IDC_USER_EXT_EDIT)), &QLineEdit::textChanged, _hSelf, [this]()
	{
		updateExtension();
		notifyDataModified();
		apply(NO_VISUAL_CHANGE);
	});
}

void WordStyleDlg::onFontStyleToggled(fontStyleType whichStyle)
{
	updateFontStyleStatus(whichStyle);
	notifyDataModified();
	apply(GENERAL_CHANGE);
}

void WordStyleDlg::onCancel()
{
	if (_isDirty)
	{
		NppParameters& nppParamInst = NppParameters::getInstance();
		if (_restoreInvalid)
		{
			wstring str(nppParamInst.getNppGUI()._themeName);
			nppParamInst.reloadStylers(str.c_str());
			loadLangListFromNppParam();
		}

		LexerStylerArray & lsArray = nppParamInst.getLStylerArray();
		StyleArray & globalStyles = nppParamInst.getGlobalStylers();

		if (_restoreInvalid)
		{
			_lsArray = _styles2restored = lsArray;
			_globalStyles = _gstyles2restored = globalStyles;
		}
		else
		{
			globalStyles = _globalStyles = _gstyles2restored;
			lsArray = _lsArray = _styles2restored;
		}

		restoreGlobalOverrideValues();
		nppParamInst.initTabCustomColors();
		nppParamInst.initFindDlgStatusMsgCustomColors();

		_restoreInvalid = false;
		_isDirty = false;
		_isThemeDirty = false;
		setVisualFromStyleList();

		// Otherwise the next save would make the theme that was just cancelled the current one
		_themeName = nppParamInst.getNppGUI()._themeName;
		_hSwitch2ThemeCombo->setCurrentIndex(_currentThemeIndex);
		::SendMessage(_hParent, WM_UPDATESCINTILLAS, _isThemeChanged, 0);
		::SendMessage(_hParent, WM_UPDATEMAINMENUBITMAPS, 0, 0);

		_isThemeChanged = false;
	}
	enableItem(IDC_SAVECLOSE_BUTTON, false);
	display(false);
}

void WordStyleDlg::saveAndClose()
{
	if (_isDirty)
	{
		const LexerStylerArray & lsa = (NppParameters::getInstance()).getLStylerArray();
		const StyleArray & globalStyles = (NppParameters::getInstance()).getGlobalStylers();

		_lsArray = lsa;
		_globalStyles = globalStyles;
		updateThemeName(_themeName);
		_restoreInvalid = false;

		_currentThemeIndex = _hSwitch2ThemeCombo->currentIndex();
		_isDirty = false;
		_isThemeChanged = false;
	}
	_isThemeDirty = false;
	auto newSavedFilePath = (NppParameters::getInstance()).writeStyles(_lsArray, _globalStyles);
	if (!newSavedFilePath.empty())
		updateThemeName(newSavedFilePath);

	enableItem(IDC_SAVECLOSE_BUTTON, false);
	display(false);

	// With the application of each modification, the following 2 actions (applications) are not necessary
	//::SendMessage(_hParent, WM_UPDATESCINTILLAS, TRUE, 0);
	//::SendMessage(_hParent, WM_UPDATEMAINMENUBITMAPS, 0, 0);

	const wchar_t* fn = PathFindFileName(_themeName.c_str());
	NppDarkMode::setThemeName((!NppDarkMode::isEnabled() && wcscmp(fn, L"stylers.xml") == 0) ? L"" : fn);
}

void WordStyleDlg::onColourPicked(bool which)
{
	int applicationInfo = getApplicationInfo();

	int tabColourIndex = whichTabColourIndex();

	if (which == C_FOREGROUND)
	{
		updateColour(C_FOREGROUND);
		notifyDataModified();

		if (tabColourIndex != -1)
		{
			TabBarPlus::setColour(_pFgColour->getColour(), static_cast<TabBarPlus::tabColourIndex>(tabColourIndex), nullptr);
		}
		else
		{
			int findDlgStatusMsgIndex = whichFindDlgStatusMsgColourIndex();
			if (findDlgStatusMsgIndex != -1)
			{
				NppParameters& nppParamInst = NppParameters::getInstance();
				nppParamInst.setFindDlgStatusMsgIndexColor(_pFgColour->getColour(), findDlgStatusMsgIndex);
			}
			else if (isDocumentMapStyle())
			{
				ViewZoneDlg::setColour(_pFgColour->getColour(), ViewZoneDlg::ViewZoneColorIndex::focus);
			}
		}
	}
	else
	{
		updateColour(C_BACKGROUND);
		notifyDataModified();

		if (tabColourIndex != -1)
		{
			tabColourIndex = (tabColourIndex == TabBarPlus::inactiveText ? TabBarPlus::inactiveBg : tabColourIndex);
			TabBarPlus::setColour(_pBgColour->getColour(), static_cast<TabBarPlus::tabColourIndex>(tabColourIndex), nullptr);
		}
		else if (isDocumentMapStyle())
		{
			ViewZoneDlg::setColour(_pBgColour->getColour(), ViewZoneDlg::ViewZoneColorIndex::frost);
		}
		else
		{
			int colourIndex = whichIndividualTabColourId();

			if (colourIndex != -1)
			{
				if (colourIndex >= TabBarPlus::individualTabColourId::id5)
					colourIndex -= TabBarPlus::individualTabColourId::id5;

				NppParameters& nppParamInst = NppParameters::getInstance();
				nppParamInst.setIndividualTabColor(_pBgColour->getColour(), colourIndex, NppDarkMode::isEnabled());
			}
		}
	}

	apply(applicationInfo);
}

// These labels show whether their setting is in use for the style
void WordStyleDlg::updateStaticTexts()
{
	const Style& style = getCurrentStyler();

	bool isFgEnabled = isColourSet(style._fgColor);

	// Selected text colour style
	if (style._styleDesc == L"Selected text colour")
		isFgEnabled = NppParameters::getInstance().getSVP()._selectedTextForegroundSingleColor;

	_hFgColourStaticText->setEnabled(isFgEnabled);
	_hBgColourStaticText->setEnabled(isColourSet(style._bgColor));
	_hFontNameStaticText->setEnabled(!style._fontName.empty());
	_hFontSizeStaticText->setEnabled(style._fontSize != STYLE_NOT_USED && style._fontSize < 100); // style._fontSize has only 2 digits
}

void WordStyleDlg::loadLangListFromNppParam()
{
	NppParameters& nppParamInst = NppParameters::getInstance();
	_lsArray = nppParamInst.getLStylerArray();
	_globalStyles = nppParamInst.getGlobalStylers();

	// Clean up Language List
	_hLanguagesCombo->clear();

	_hLanguagesCombo->addItem(QStringLiteral("Global Styles"));
	// All the lexers
	for (size_t i = 0, nb = _lsArray.getNbLexer() ; i < nb ; ++i)
	{
		const std::wstring langName = (_lsArray.getLexerDescFromIndex(i));
		_hLanguagesCombo->addItem(QString::fromStdWString(langName));
	}

	static constexpr int index2Begin = 0;
	_hLanguagesCombo->setCurrentIndex(index2Begin);
	setStyleListFromLexer(index2Begin);
}

void WordStyleDlg::updateThemeName(const wstring& themeName)
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = nppParam.getNppGUI();
	nppGUI._themeName.assign( themeName );
}

bool WordStyleDlg::getStyleName(std::wstring& styleName) const
{
	const QListWidgetItem* item = _hStylesList->currentItem();
	if (!item)
		return false;

	styleName = item->text().toStdWString();
	return true;
}

int WordStyleDlg::getApplicationInfo() const
{
	std::wstring styleName;
	if (!WordStyleDlg::getStyleName(styleName))
	{
		return NO_VISUAL_CHANGE;
	}

	if (styleName == L"Default Style")
	{
		return (GENERAL_CHANGE | THEME_CHANGE);
	}

	if ((styleName == L"Mark Style 1")
		|| (styleName == L"Mark Style 2")
		|| (styleName == L"Mark Style 3")
		|| (styleName == L"Mark Style 4")
		|| (styleName == L"Mark Style 5")
		|| (styleName == TABBAR_INDIVIDUALCOLOR_1)
		|| (styleName == TABBAR_INDIVIDUALCOLOR_2)
		|| (styleName == TABBAR_INDIVIDUALCOLOR_3)
		|| (styleName == TABBAR_INDIVIDUALCOLOR_4)
		|| (styleName == TABBAR_INDIVIDUALCOLOR_5)
		|| (styleName == TABBAR_INDIVIDUALCOLOR_DM_1)
		|| (styleName == TABBAR_INDIVIDUALCOLOR_DM_2)
		|| (styleName == TABBAR_INDIVIDUALCOLOR_DM_3)
		|| (styleName == TABBAR_INDIVIDUALCOLOR_DM_4)
		|| (styleName == TABBAR_INDIVIDUALCOLOR_DM_5))
	{
		return (GENERAL_CHANGE | COLOR_CHANGE_4_MENU);
	}
	return GENERAL_CHANGE;
}

int WordStyleDlg::whichTabColourIndex() const 
{
	std::wstring styleName;
	if (!WordStyleDlg::getStyleName(styleName))
	{
		return -1;
	}

	using enum TabBarPlus::tabColourIndex;

	if (styleName == TABBAR_ACTIVEFOCUSEDINDCATOR)
		return activeFocusedTop;

	if (styleName == TABBAR_ACTIVEUNFOCUSEDINDCATOR)
		return activeUnfocusedTop;

	if (styleName == TABBAR_ACTIVETEXT)
		return activeText;

	if (styleName == TABBAR_INACTIVETEXT)
		return inactiveText;

	return -1;
}

int WordStyleDlg::whichIndividualTabColourId()
{
	std::wstring styleName;
	if (!WordStyleDlg::getStyleName(styleName))
	{
		return -1;
	}

	using enum TabBarPlus::individualTabColourId;

	if (styleName == TABBAR_INDIVIDUALCOLOR_1)
		return id0;

	if (styleName == TABBAR_INDIVIDUALCOLOR_2)
		return id1;

	if (styleName == TABBAR_INDIVIDUALCOLOR_3)
		return id2;

	if (styleName == TABBAR_INDIVIDUALCOLOR_4)
		return id3;

	if (styleName == TABBAR_INDIVIDUALCOLOR_5)
		return id4;


	if (styleName == TABBAR_INDIVIDUALCOLOR_DM_1)
		return id5;

	if (styleName == TABBAR_INDIVIDUALCOLOR_DM_2)
		return id6;

	if (styleName == TABBAR_INDIVIDUALCOLOR_DM_3)
		return id7;

	if (styleName == TABBAR_INDIVIDUALCOLOR_DM_4)
		return id8;

	if (styleName == TABBAR_INDIVIDUALCOLOR_DM_5)
		return id9;


	return -1;
}

int WordStyleDlg::whichFindDlgStatusMsgColourIndex()
{
	std::wstring styleName;
	if (!WordStyleDlg::getStyleName(styleName))
	{
		return -1;
	}

	using enum TabBarPlus::individualTabColourId;

	if (styleName == FINDDLG_STAUSNOTFOUND_COLOR)
		return id0;

	if (styleName == FINDDLG_STAUSMESSAGE_COLOR)
		return id1;

	if (styleName == FINDDLG_STAUSREACHED_COLOR)
		return id2;

	return -1;
}

bool WordStyleDlg::isDocumentMapStyle()
{
	std::wstring styleName;
	return WordStyleDlg::getStyleName(styleName) && (styleName == VIEWZONE_DOCUMENTMAP);
}

void WordStyleDlg::updateColour(bool which)
{
	Style & style = getCurrentStyler();
	if (which == C_FOREGROUND)
	{
		style._fgColor = _pFgColour->getColour();
		if (_pFgColour->isEnabled())
			style._colorStyle |= COLORSTYLE_FOREGROUND;
		else
			style._colorStyle &= ~COLORSTYLE_FOREGROUND;
	}
	else //(which == C_BACKGROUND)
	{
		style._bgColor = _pBgColour->getColour();
		if (_pBgColour->isEnabled())
			style._colorStyle |= COLORSTYLE_BACKGROUND;
		else
			style._colorStyle &= ~COLORSTYLE_BACKGROUND;
	}
}

void WordStyleDlg::updateFontSize()
{
	Style & style = getCurrentStyler();

	if (_hFontSizeCombo->currentIndex() != 0)
	{
		bool isNumber = false;
		const int fontSize = _hFontSizeCombo->currentText().toInt(&isNumber);
		style._fontSize = isNumber ? fontSize : STYLE_NOT_USED;
	}
	else
		style._fontSize = 0;
}

void WordStyleDlg::updateExtension()
{
	_lsArray.getLexerFromIndex(static_cast<size_t>(_currentLexerIndex) - 1).setLexerUserExt(getItemText(IDC_USER_EXT_EDIT).c_str());
}

void WordStyleDlg::updateUserKeywords()
{
	Style & style = getCurrentStyler();
	style._keywords = wstring2string(getItemText(IDC_USER_KEYWORDS_EDIT));
}

void WordStyleDlg::updateFontName()
{
	Style & style = getCurrentStyler();
	style._fontName = _hFontNameCombo->currentText().toStdWString();
}

void WordStyleDlg::updateFontStyleStatus(fontStyleType whitchStyle)
{
	Style & style = getCurrentStyler();
	if (style._fontStyle == STYLE_NOT_USED)
		style._fontStyle = FONTSTYLE_NONE;

	int fontStyle = FONTSTYLE_UNDERLINE;
	QAbstractButton* check = _hCheckUnderline;

	if (whitchStyle == BOLD_STATUS)
	{
		fontStyle = FONTSTYLE_BOLD;
		check = _hCheckBold;
	}
	if (whitchStyle == ITALIC_STATUS)
	{
		fontStyle = FONTSTYLE_ITALIC;
		check = _hCheckItalic;
	}

	if (check->isChecked())
		style._fontStyle |= fontStyle;
	else
		style._fontStyle &= ~fontStyle;
}

void WordStyleDlg::switchToTheme()
{
	const int iSel = _hSwitch2ThemeCombo->currentIndex();

	wstring prevThemeName(_themeName);
	_themeName.clear();

	NppParameters& nppParamInst = NppParameters::getInstance();
	ThemeSwitcher & themeSwitcher = nppParamInst.getThemeSwitcher();
	pair<wstring, wstring> & themeInfo = themeSwitcher.getElementFromIndex(iSel);
	_themeName = themeInfo.second;

	if (_isThemeDirty)
	{
		const wstring themeFileName = QFileInfo(QString::fromStdWString(prevThemeName)).completeBaseName().toStdWString();
		NativeLangSpeaker *pNativeSpeaker = nppParamInst.getNativeLangSpeaker();
		int mb_response = pNativeSpeaker->messageBox("SwitchUnsavedThemeWarning",
			_hSelf,
			L"Unsaved changes are about to be discarded!\nDo you want to save your changes before switching themes?",
			L"$STR_REPLACE$",
			MB_ICONWARNING | MB_YESNO | MB_APPLMODAL | MB_SETFOREGROUND,
			0,
			themeFileName.c_str());
		if ( mb_response == IDYES )
			(NppParameters::getInstance()).writeStyles(_lsArray, _globalStyles);
	}
	nppParamInst.reloadStylers(_themeName.c_str());

	loadLangListFromNppParam();
	_restoreInvalid = true;

}

void WordStyleDlg::applyCurrentSelectedThemeAndUpdateUI()
{
	switchToTheme();
	setVisualFromStyleList();
	notifyDataModified();
	_isThemeDirty = false;
	apply(GENERAL_CHANGE | THEME_CHANGE);
}

bool WordStyleDlg::selectThemeByName(const wchar_t* themeName)
{
	const int iTheme = findPrefix(_hSwitch2ThemeCombo, themeName, 1);
	if (iTheme == -1)
		return false;

	_hSwitch2ThemeCombo->setCurrentIndex(iTheme);

	applyCurrentSelectedThemeAndUpdateUI();

	return true;
}

bool WordStyleDlg::goToSection(const wchar_t* sectionNames)
{
	if (!sectionNames || !sectionNames[0])
		return false;

	std::vector<std::wstring> sections = tokenizeString(sectionNames, ':');

	if (sections.size() == 0 || sections.size() >= 3)
		return false;

	const int language = findPrefix(_hLanguagesCombo, sections[0].c_str(), -1);
	if (language == -1)
		return false;
	_hLanguagesCombo->setCurrentIndex(language);
	setStyleListFromLexer(language);

	if (sections.size() == 1)
		return true;

	const QList<QListWidgetItem*> styles = _hStylesList->findItems(QString::fromStdWString(sections[1]), Qt::MatchStartsWith);
	if (styles.isEmpty())
		return false;
	{
		const QSignalBlocker blocker(_hStylesList);
		_hStylesList->setCurrentItem(styles.first());
	}
	setVisualFromStyleList();

	grabFocus();

	return true;
}

void WordStyleDlg::setStyleListFromLexer(int index)
{
	_currentLexerIndex = index;

	// Fill out Styles listbox
	// Before filling out, we clean it
	const QSignalBlocker stylesBlocker(_hStylesList);
	_hStylesList->clear();

	if (index)
	{
		const wchar_t* langName = _lsArray.getLexerNameFromIndex(static_cast<size_t>(index) - 1);
		const wchar_t *ext = NppParameters::getInstance().getLangExtFromName(langName);
		const wchar_t *userExt = (_lsArray.getLexerStylerByName(langName))->getLexerUserExt();

		if (ext)
			setItemText(IDC_DEF_EXT_EDIT, ext);

		auto* userExtEdit = qobject_cast<QLineEdit*>(dlgItem(IDC_USER_EXT_EDIT));
		const QSignalBlocker blocker(userExtEdit);
		userExtEdit->setText(QString::fromWCharArray(userExt));
		enableItem(IDC_SAVECLOSE_BUTTON, _isDirty || _isThemeDirty);
	}
	const bool isLanguage = index != 0;
	showItem(IDC_DEF_EXT_EDIT, isLanguage);
	showItem(IDC_DEF_EXT_STATIC, isLanguage);
	showItem(IDC_USER_EXT_EDIT, isLanguage);
	showItem(IDC_USER_EXT_STATIC, isLanguage);
	showItem(IDC_PLUSSYMBOL2_STATIC, isLanguage);

	const StyleArray& lexerStyler = index ? _lsArray.getLexerFromIndex(static_cast<size_t>(index) - 1) : _globalStyles;

	for (const auto& style : lexerStyler)
	{
		_hStylesList->addItem(QString::fromStdWString(style._styleDesc));
	}
	_hStylesList->setCurrentRow(0);
	setVisualFromStyleList();
}


std::pair<intptr_t, intptr_t> WordStyleDlg::goToPreferencesSettings()
{
	std::pair<intptr_t, intptr_t> result;
	result.first = -1;  // Page
	result.second = -1; // Control

	enum preferencesSectionPage {
		general = 0,
		toolbar,
		tabbar,
		edit1,
		edit2,
		darkMode,
		margins,
		newDocument,
		defaultDirectory,
		recentFileHistory,
		fileAssociation,
		language,
		indentation,
		highlighting,
		print,
		searching,
		backup,
		autoCompletion,
		multiInstance,
		delimiter,
		performance,
		cloudAndLink,
		searchEngine,
		misc
	};

	const Style& style = getCurrentStyler();

	// Check if it's one of following Global Styles:
	if (style._styleDesc == L"Current line background colour")
	{
		result.first = edit1;
		result.second = IDC_RADIO_CLM_HILITE;
	}
	else if (style._styleDesc == L"Multi-edit carets color" || style._styleDesc == L"Multi-selected text color")
	{
		result.first = edit2;
		result.second = IDC_CHECK_MULTISELECTION;
	}
	else if (style._styleDesc == L"Caret colour")
	{
		result.first = edit1;
		result.second = IDC_CARETSETTING_STATIC;
	}
	else if (style._styleDesc == L"Edge colour")
	{
		result.first = margins;
		result.second = IDC_COLUMNPOS_EDIT;
	}
	else if (style._styleDesc == L"Line number margin")
	{
		result.first = margins;
		result.second = IDC_CHECK_LINENUMBERMARGE;
	}
	else if (style._styleDesc == L"Bookmark margin")
	{
		result.first = margins;
		result.second = IDC_CHECK_BOOKMARKMARGE;
	}
	else if (style._styleDesc == L"Change History margin" || style._styleDesc == L"Change History modified"
		|| style._styleDesc == L"Change History revert modified" || style._styleDesc == L"Change History revert origin"
		|| style._styleDesc == L"Change History saved")
	{
		result.first = margins;
		result.second = IDC_GB_CHANGHISTORY;
	}
	else if (style._styleDesc == L"Fold" || style._styleDesc == L"Fold active" || style._styleDesc == L"Fold margin")
	{
		result.first = margins;
		result.second = IDC_FMS_GB_STATIC;
	}
	else if (style._styleDesc == L"Smart Highlighting")
	{
		result.first = highlighting;
		result.second = IDC_CHECK_ENABLSMARTHILITE;
	}
	else if (style._styleDesc == L"Tags match highlighting")
	{
		result.first = highlighting;
		result.second = IDC_CHECK_ENABLTAGSMATCHHILITE;
	}
	else if (style._styleDesc == L"Tags attribute")
	{
		result.first = highlighting;
		result.second = IDC_CHECK_ENABLTAGATTRHILITE;
	}
	else if (style._styleDesc == L"Mark Style 1" || style._styleDesc == L"Mark Style 2" || style._styleDesc == L"Mark Style 3"
		|| style._styleDesc == L"Mark Style 4" || style._styleDesc == L"Mark Style 5")
	{
		result.first = highlighting;
		result.second = IDC_MARKALL_STATIC;
	}
	else if (style._styleDesc == L"URL hovered")
	{
		result.first = cloudAndLink;
		result.second = IDC_CHECK_CLICKABLELINK_ENABLE;
	}
	else if (style._styleDesc == L"EOL custom color")
	{
		result.first = edit2;
		result.second = IDC_CHECK_WITHCUSTOMCOLOR_CRLF;
	}
	else if (style._styleDesc == L"Active tab focused indicator" || style._styleDesc == L"Active tab unfocused indicator")
	{
		result.first = tabbar;
		result.second = IDC_CHECK_ORANGE;
	}
	else if (style._styleDesc == L"Inactive tabs")
	{
		result.first = tabbar;
		result.second = IDC_CHECK_DRAWINACTIVE;
	}
	else if (style._styleDesc == g_npcStyleName)
	{
		result.first = edit2;
		result.second = IDC_CHECK_NPC_COLOR;
	}

	return result;
}

void WordStyleDlg::syncWithSelFgSingleColorCtrl()
{
	const Style& style = getCurrentStyler();

	// Selected text colour style
	if (style._styleDesc == L"Selected text colour")
	{
		// Only in case that dialog is on "Selected text colour":
		// Switch to a section then switch back for refresh current state of "Selected text colour"
		goToSection(L"Global Styles:Default Style");
		goToSection(L"Global Styles:Selected text colour");
	}
}

void WordStyleDlg::setVisualFromStyleList()
{
	showGlobalOverrideCtrls(false);

	Style & style = getCurrentStyler();

	// Global override style
	if (style._styleDesc == L"Global override")
	{
		showGlobalOverrideCtrls(true);
	}

	std::pair<intptr_t, intptr_t> pageAndCtrlID = goToPreferencesSettings();
	_goToSettings.display(pageAndCtrlID.first != -1);

	//--Warning text
	//bool showWarning = ((_currentLexerIndex == 0) && (style._styleID == STYLE_DEFAULT));//?SW_SHOW:SW_HIDE;

	if (_hLanguagesCombo->currentIndex() == -1)
		return;

	const QListWidgetItem* styleItem = _hStylesList->currentItem();
	if (!styleItem)
		return;

	_hStyleInfoStaticText->setText(_hLanguagesCombo->currentText() + QStringLiteral(": ") + styleItem->text());

	//-- 2 couleurs : fg et bg
	bool isEnable = false;
	if (isColourSet(style._fgColor))
	{
		_pFgColour->setColour(style._fgColor);
		_pFgColour->setEnabled((style._colorStyle & COLORSTYLE_FOREGROUND) != 0);
		isEnable = true;
	}

	// Selected text colour style
	if (style._styleDesc == L"Selected text colour")
	{
		isEnable = false; // disable by default for "Selected text colour" style

		if (NppParameters::getInstance().getSVP()._selectedTextForegroundSingleColor)
			isEnable = true;
	}
	_pFgColour->getHSelf()->setEnabled(isEnable);

	isEnable = false;
	if (isColourSet(style._bgColor))
	{
		_pBgColour->setColour(style._bgColor);
		_pBgColour->setEnabled((style._colorStyle & COLORSTYLE_BACKGROUND) != 0);
		isEnable = true;
	}
	_pBgColour->getHSelf()->setEnabled(isEnable);

	//-- font name
	int iFontName = 0;
	if (!style._fontName.empty())
	{
		iFontName = findPrefix(_hFontNameCombo, style._fontName.c_str(), 1);
		if (iFontName == -1)
		{
			// Not installed, as Courier New of the default themes often is here: the system substitutes a font for
			// it, and the list still says which one the style asks for
			_hFontNameCombo->addItem(QString::fromStdWString(style._fontName));
			iFontName = _hFontNameCombo->count() - 1;
		}
	}

	_hFontNameCombo->setCurrentIndex(iFontName);
	_hFontNameCombo->setEnabled(style._isFontEnabled);

	//-- font size
	int iFontSize = 0;
	if (style._fontSize != STYLE_NOT_USED && style._fontSize < 100) // style._fontSize has only 2 digits
	{
		iFontSize = _hFontSizeCombo->findText(QString::number(style._fontSize));
		if (iFontSize == -1)
			iFontSize = 0;
	}
	_hFontSizeCombo->setCurrentIndex(iFontSize);
	_hFontSizeCombo->setEnabled(style._isFontEnabled);

	updateStaticTexts();

	//-- font style : bold & italic
	if (style._fontStyle != STYLE_NOT_USED)
	{
		_hCheckBold->setChecked((style._fontStyle & FONTSTYLE_BOLD) != 0);
		_hCheckItalic->setChecked((style._fontStyle & FONTSTYLE_ITALIC) != 0);
		_hCheckUnderline->setChecked((style._fontStyle & FONTSTYLE_UNDERLINE) != 0);
	}
	else // STYLE_NOT_USED : reset them all
	{
		_hCheckBold->setChecked(false);
		_hCheckItalic->setChecked(false);
		_hCheckUnderline->setChecked(false);
	}

	enableFontStyle(style._isFontEnabled);


	//-- Default Keywords
	bool shouldBeDisplayed = style._keywordClass != STYLE_NOT_USED;
	if (shouldBeDisplayed)
	{
		LexerStyler & lexerStyler = _lsArray.getLexerFromIndex(static_cast<size_t>(_currentLexerIndex) - 1);

		NppParameters& nppParams = NppParameters::getInstance();
		LangType lType = nppParams.getLangIDFromStr(lexerStyler.getLexerName());
		if (lType == L_TEXT)
		{
			wstring lexerNameStr = lexerStyler.getLexerName();
			lexerNameStr += L" is not defined in NppParameters::getLangIDFromStr()";
				printStr(lexerNameStr.c_str());
		}
		const char* kws = nppParams.getWordList(lType, style._keywordClass);
		if (!kws)
			kws = "";
		setItemText(IDC_DEF_KEYWORDS_EDIT, string2wstring(kws));

		auto* userKeywords = qobject_cast<QPlainTextEdit*>(dlgItem(IDC_USER_KEYWORDS_EDIT));
		const QSignalBlocker blocker(userKeywords);
		userKeywords->setPlainText(QString::fromStdWString(string2wstring(style._keywords)));
	}

	showItem(IDC_DEF_KEYWORDS_EDIT, shouldBeDisplayed);
	showItem(IDC_USER_KEYWORDS_EDIT, shouldBeDisplayed);
	showItem(IDC_DEF_KEYWORDS_STATIC, shouldBeDisplayed);
	showItem(IDC_USER_KEYWORDS_STATIC, shouldBeDisplayed);
	showItem(IDC_PLUSSYMBOL_STATIC, shouldBeDisplayed);

	redraw();
}


void WordStyleDlg::doDialog(bool isRTL)
{
	if (!isCreated())
	{
		createDialog(isRTL);
		prepare2Cancel();
	}

	if (!_hSelf->isVisible())
	{
		prepare2Cancel();
	}
	display();
}

void WordStyleDlg::destroy()
{
	_pFgColour.reset();
	_pBgColour.reset();
	StaticDialog::destroy();
}

void WordStyleDlg::prepare2Cancel()
{
	_styles2restored = (NppParameters::getInstance()).getLStylerArray();
	_gstyles2restored = (NppParameters::getInstance()).getGlobalStylers();
	_gOverride2restored = (NppParameters::getInstance()).getGlobalOverrideStyle();
}

void WordStyleDlg::redraw(bool forceUpdate) const
{
	_pFgColour->redraw(forceUpdate);
	_pBgColour->redraw(forceUpdate);
}

void WordStyleDlg::restoreGlobalOverrideValues() const
{
	GlobalOverride& gOverride = (NppParameters::getInstance()).getGlobalOverrideStyle();
	gOverride = _gOverride2restored;
}

void WordStyleDlg::apply(int applicationInfo)
{
	LexerStylerArray & lsa = (NppParameters::getInstance()).getLStylerArray();
	lsa = _lsArray;

	StyleArray & globalStyles = (NppParameters::getInstance()).getGlobalStylers();
	globalStyles = _globalStyles;

	if ((applicationInfo & THEME_CHANGE) != 0)
		_isThemeChanged = true;

	if (applicationInfo & GENERAL_CHANGE || applicationInfo & THEME_CHANGE)
		::SendMessage(_hParent, WM_UPDATESCINTILLAS, (applicationInfo & THEME_CHANGE) != 0, 0);
	if (applicationInfo & COLOR_CHANGE_4_MENU)
		::SendMessage(_hParent, WM_UPDATEMAINMENUBITMAPS, 0, 0);
}

void WordStyleDlg::addLastThemeEntry() const
{
	NppParameters& nppParamInst = NppParameters::getInstance();
	ThemeSwitcher& themeSwitcher = nppParamInst.getThemeSwitcher();
	std::pair<wstring, wstring>& themeInfo = themeSwitcher.getElementFromIndex(themeSwitcher.size() - 1);
	_hSwitch2ThemeCombo->addItem(QString::fromStdWString(themeInfo.first));
}

Style& WordStyleDlg::getCurrentStyler()
{
	int styleIndex = _hStylesList->currentRow();
	if (styleIndex == -1)
		styleIndex = 0;

	try {
		if (_currentLexerIndex == 0)
		{
			return _globalStyles.getStyler(styleIndex);
		}
		else
		{
			LexerStyler& lexerStyler = _lsArray.getLexerFromIndex(static_cast<size_t>(_currentLexerIndex) - 1);
			return lexerStyler.getStyler(styleIndex);
		}
	}
	catch (...)
	{
		return _globalStyles.getStyler(0);
	}
}

void WordStyleDlg::enableFontStyle(bool isEnable) const
{
	_hCheckBold->setEnabled(isEnable);
	_hCheckItalic->setEnabled(isEnable);
	_hCheckUnderline->setEnabled(isEnable);
}

long WordStyleDlg::notifyDataModified()
{
	_isDirty = true;
	_isThemeDirty = true;
	enableItem(IDC_SAVECLOSE_BUTTON, true);
	return TRUE;
}

void WordStyleDlg::showGlobalOverrideCtrls(bool show)
{
	if (show)
	{
		updateGlobalOverrideCtrls();
	}
	showItem(IDC_GLOBAL_FG_CHECK, show);
	showItem(IDC_GLOBAL_BG_CHECK, show);
	showItem(IDC_GLOBAL_FONT_CHECK, show);
	showItem(IDC_GLOBAL_FONTSIZE_CHECK, show);
	showItem(IDC_GLOBAL_BOLD_CHECK, show);
	showItem(IDC_GLOBAL_ITALIC_CHECK, show);
	showItem(IDC_GLOBAL_UNDERLINE_CHECK, show);
	showItem(IDC_GLOBAL_WHATISGLOBALOVERRIDE_LINK, show);
	_isShownGOCtrls = show;
}
