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


#include "preferenceDlg.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QStyleHints>
#include <QToolButton>
#include <QToolTip>
#include <QUrl>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Scintilla.h>

#include "ColourPicker.h"
#include "Common.h"
#include "EncodingMapper.h"
#include "Notepad_plus_msgs.h"
#include "NppConstants.h"
#include "NppDarkMode.h"
#include "NppMenu.h"
#include "NppXml.h"
#include "Parameters.h"
#include "RunDlg.h"
#include "ScintillaEditView.h"
#include "StaticDialog.h"
#include "ToolBar.h"
#include "localization.h"
#include "menuCmdID.h"
#include "preference_rc.h"
#include "resource.h"
#include "shortcut.h"
#include "ui_PreferenceBoxDlg.h"
#include "ui_PreferenceSubAutocompletionDlg.h"
#include "ui_PreferenceSubBackupDlg.h"
#include "ui_PreferenceSubCloudLinkDlg.h"
#include "ui_PreferenceSubDarkmodeDlg.h"
#include "ui_PreferenceSubDefaultdirectoryDlg.h"
#include "ui_PreferenceSubDelimiterDlg.h"
#include "ui_PreferenceSubEditingDlg.h"
#include "ui_PreferenceSubEditing2Dlg.h"
#include "ui_PreferenceSubGeneralDlg.h"
#include "ui_PreferenceSubHighlightingDlg.h"
#include "ui_PreferenceSubIndentationDlg.h"
#include "ui_PreferenceSubLanguageDlg.h"
#include "ui_PreferenceSubMargingBorderEdgeDlg.h"
#include "ui_PreferenceSubMiscDlg.h"
#include "ui_PreferenceSubMultiinstanceDlg.h"
#include "ui_PreferenceSubNewdocumentDlg.h"
#include "ui_PreferenceSubPerformanceDlg.h"
#include "ui_PreferenceSubPrintDlg.h"
#include "ui_PreferenceSubRecentfileshistoryDlg.h"
#include "ui_PreferenceSubSearchengineDlg.h"
#include "ui_PreferenceSubSearchingDlg.h"
#include "ui_PreferenceSubTabbarDlg.h"
#include "ui_PreferenceSubToolbarDlg.h"

using namespace std;

static constexpr int BLINKRATE_FASTEST = 50;
static constexpr int BLINKRATE_SLOWEST = 2500;
static constexpr int BLINKRATE_INTERVAL = 50;

static constexpr int CARETLINEFRAME_SMALLEST = 1;
static constexpr int CARETLINEFRAME_LARGEST = 6;
static constexpr int CARETLINEFRAME_INTERVAL = 1;

static constexpr int BORDERWIDTH_SMALLEST = 0;
static constexpr int BORDERWIDTH_LARGEST = 30;
static constexpr int BORDERWIDTH_INTERVAL = 1;

static constexpr int PADDING_SMALLEST = 0;
static constexpr int PADDING_LARGEST = 30;
static constexpr int PADDING_INTERVAL = 1;

static constexpr int DISTRACTIONFREE_SMALLEST = 3;
static constexpr int DISTRACTIONFREE_LARGEST = 9;
static constexpr int DISTRACTIONFREE_INTERVAL = 1;

static constexpr int AUTOCOMPLETEFROMCHAR_SMALLEST = 1;
static constexpr int AUTOCOMPLETEFROMCHAR_LARGEST = 9;
static constexpr int AUTOCOMPLETEFROMCHAR_INTERVAL = 1;

// This int encoding array is built from "EncodingUnit encodings[]" (see EncodingMapper.cpp)
// And NewDocumentSubDlg will use "int encoding array" to get more info from "EncodingUnit encodings[]"
static constexpr int encodings[]{
	1250,
	1251, 
	1252, 
	1253, 
	1254, 
	1255, 
	1256, 
	1257, 
	1258, 
	28591,
	28592,
	28593,
	28594,
	28595,
	28596,
	28597,
	28598,
	28599,
	28603,
	28604,
	28605,
	437,  
	720,  
	737,  
	775,  
	850,  
	852,  
	855,  
	857,  
	858,  
	860,  
	861,  
	862,  
	863,  
	865,  
	866,  
	869,  
	950,  
	936,  
	932,  
	949,  
	51949,
	874,
	10007,
	21866,
	20866
};

namespace
{
	// The edit fields that only take a number
	constexpr int numberEdits[] = {
		IDC_BACKUPDIR_RESTORESESSION_EDIT, IDC_EDIT_CUSTOMIZELENGTHVAL, IDC_EDIT_MAXNBFILEVAL, IDC_EDIT_MB, IDC_EDIT_ML, IDC_EDIT_MR, IDC_EDIT_MT,
		IDC_EDIT_PERFORMANCE_FILESIZE, IDC_EDIT_TABCOMPACTLABELLEN, IDC_EDIT_TABSIZEVAL, IDC_FILLFINDWHAT_THRESHOLD_EDIT, IDC_INSELECTION_THRESHOLD_EDIT
	};

	COLORREF windowFaceColour(const QWidget* widget)
	{
		const QColor colour = widget->palette().color(QPalette::Window);
		return RGB(colour.red(), colour.green(), colour.blue());
	}
}

void PreferenceSubDlg::createPage(HINSTANCE hInst, HWND dialog, QStackedWidget* pages, const std::function<void(QDialog*)>& setupForm)
{
	init(hInst, dialog);

	QDialog* page = createDialogWidget(false);
	setupForm(page);
	page->setParent(pages, Qt::Widget);
	pages->addWidget(page);

	for (int id : numberEdits)
	{
		if (auto* edit = qobject_cast<QLineEdit*>(dlgItem(id)))
		{
			edit->setValidator(new QIntValidator(0, 999999999, edit));
			edit->setMaximumWidth(80);
		}
	}

	onInit();
	connectControls();
}

void PreferenceSubDlg::onCancel()
{
	_hParent->close();
}

void PreferenceDlg::doDialog(bool isRTL)
{
	if (!isCreated())
	{
		createDialog(isRTL);
		goToCenter();
	}
	display();
}

void PreferenceDlg::createDialog(bool isRTL)
{
	QDialog* dialog = createDialogWidget(isRTL);
	Ui::PreferenceBoxDlg ui;
	ui.setupUi(dialog);

	_pages = ui.pages;
	_categories = ui.listDlgtitle;

	_generalSubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubGeneralDlg form; form.setupUi(page); });
	_toolbarSubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubToolbarDlg form; form.setupUi(page); });
	_tabbarSubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubTabbarDlg form; form.setupUi(page); });
	_editingSubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubEditingDlg form; form.setupUi(page); });
	_editing2SubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubEditing2Dlg form; form.setupUi(page); });
	_darkModeSubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubDarkmodeDlg form; form.setupUi(page); });
	_marginsBorderEdgeSubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubMargingBorderEdgeDlg form; form.setupUi(page); });
	_newDocumentSubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubNewdocumentDlg form; form.setupUi(page); });
	_defaultDirectorySubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubDefaultdirectoryDlg form; form.setupUi(page); });
	_recentFilesHistorySubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubRecentfileshistoryDlg form; form.setupUi(page); });
	_languageSubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubLanguageDlg form; form.setupUi(page); });
	_indentationSubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubIndentationDlg form; form.setupUi(page); });
	_highlightingSubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubHighlightingDlg form; form.setupUi(page); });
	_printSubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubPrintDlg form; form.setupUi(page); });
	_searchingSubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubSearchingDlg form; form.setupUi(page); });
	_backupSubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubBackupDlg form; form.setupUi(page); });
	_autoCompletionSubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubAutocompletionDlg form; form.setupUi(page); });
	_multiInstanceSubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubMultiinstanceDlg form; form.setupUi(page); });
	_delimiterSubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubDelimiterDlg form; form.setupUi(page); });
	_performanceSubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubPerformanceDlg form; form.setupUi(page); });
	_cloudAndLinkSubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubCloudLinkDlg form; form.setupUi(page); });
	_searchEngineSubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubSearchengineDlg form; form.setupUi(page); });
	_miscSubDlg.createPage(_hInst, dialog, _pages, [](QDialog* page) { Ui::PreferenceSubMiscDlg form; form.setupUi(page); });

	_wVector.push_back({ &_generalSubDlg, L"General", L"Global" });
	_wVector.push_back({ &_toolbarSubDlg, L"Toolbar", L"Toolbar" });
	_wVector.push_back({ &_tabbarSubDlg, L"Tab Bar", L"Tabbar" });
	_wVector.push_back({ &_editingSubDlg, L"Editing 1", L"Scintillas" });
	_wVector.push_back({ &_editing2SubDlg, L"Editing 2", L"Scintillas2" });
	_wVector.push_back({ &_darkModeSubDlg, L"Dark Mode", L"DarkMode" });
	_wVector.push_back({ &_marginsBorderEdgeSubDlg, L"Margins/Border/Edge", L"MarginsBorderEdge" });
	_wVector.push_back({ &_newDocumentSubDlg, L"New Document", L"NewDoc" });
	_wVector.push_back({ &_defaultDirectorySubDlg, L"Default Directory", L"DefaultDir" });
	_wVector.push_back({ &_recentFilesHistorySubDlg, L"Recent Files History", L"RecentFilesHistory" });
	_wVector.push_back({ &_languageSubDlg, L"Language", L"Language" });
	_wVector.push_back({ &_indentationSubDlg, L"Indentation", L"Indentation" });
	_wVector.push_back({ &_highlightingSubDlg, L"Highlighting", L"Highlighting" });
	_wVector.push_back({ &_printSubDlg, L"Print", L"Print" });
	_wVector.push_back({ &_searchingSubDlg, L"Searching", L"Searching" });
	_wVector.push_back({ &_backupSubDlg, L"Backup", L"Backup" });
	_wVector.push_back({ &_autoCompletionSubDlg, L"Auto-Completion", L"AutoCompletion" });
	_wVector.push_back({ &_multiInstanceSubDlg, L"Multi-Instance & Date", L"MultiInstance" });
	_wVector.push_back({ &_delimiterSubDlg, L"Delimiter", L"Delimiter" });
	_wVector.push_back({ &_performanceSubDlg, L"Performance", L"Performance" });
	_wVector.push_back({ &_cloudAndLinkSubDlg, L"Cloud & Link", L"Cloud" });
	_wVector.push_back({ &_searchEngineSubDlg, L"Search Engine", L"SearchEngine" });
	_wVector.push_back({ &_miscSubDlg, L"MISC.", L"MISC" });

	makeCategoryList();

	nppRegisterMessageTarget(dialog, [this](UINT message, WPARAM wParam, LPARAM lParam) { return onMessage(message, wParam, lParam); });

	QObject::connect(_categories, &QListWidget::currentRowChanged, dialog, [this](int row)
	{
		if (row >= 0)
			showDialogByIndex(row);
	});
	QObject::connect(ui.buttonClose, &QAbstractButton::clicked, dialog, [this]() { display(false); });
}

bool PreferenceDlg::goToSection(size_t iPage, intptr_t ctrlID)
{
	setListSelection(iPage);
	showDialogByIndex(iPage);
	grabFocus();

	if (ctrlID != -1)
	{
		QWidget* control = _wVector[iPage]._dlg->dlgItem(static_cast<int>(ctrlID));
		if (!control)
			return false;

		control->setFocus();

		NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
		static wstring hereTip = pNativeSpeaker->getLocalizedStrFromID("goto-setting-tip", L"Find your setting here");
		QToolTip::showText(control->mapToGlobal(control->rect().bottomLeft()), QString::fromStdWString(hereTip), control, {}, 3000);
	}
	return true;
}

LRESULT PreferenceDlg::onMessage(UINT message, WPARAM wParam, LPARAM /*lParam*/)
{
	if (message != NPPM_INTERNAL_SETTOOLICONSSET) // Set icons set only option (checkbox) on general sub-dialog, the remaining real operations will be done in NppDarkMode::refreshDarkMode
		return 0;

	NppParameters& nppParams = NppParameters::getInstance();
	NppGUI& nppGUI = nppParams.getNppGUI();
	auto& nppGUITbInfo = nppGUI._tbIconInfo;
	nppGUITbInfo = NppDarkMode::getToolbarIconInfo(static_cast<bool>(wParam));

	const auto checkOrUncheckBtn = [this](int id, bool check = false) -> void
	{
		_toolbarSubDlg.setChecked(id, check);
	};

	checkOrUncheckBtn(IDC_RADIO_SMALLICON, nppGUITbInfo._tbIconSet == TB_SMALL);
	checkOrUncheckBtn(IDC_RADIO_BIGICON, nppGUITbInfo._tbIconSet == TB_LARGE);
	checkOrUncheckBtn(IDC_RADIO_SMALLICON2, nppGUITbInfo._tbIconSet == TB_SMALL2);
	checkOrUncheckBtn(IDC_RADIO_BIGICON2, nppGUITbInfo._tbIconSet == TB_LARGE2);
	checkOrUncheckBtn(IDC_RADIO_STANDARD, nppGUITbInfo._tbIconSet == TB_STANDARD);

	checkOrUncheckBtn(IDC_RADIO_RED, nppGUITbInfo._tbColor == FluentColor::red);
	checkOrUncheckBtn(IDC_RADIO_GREEN, nppGUITbInfo._tbColor == FluentColor::green);
	checkOrUncheckBtn(IDC_RADIO_BLUE, nppGUITbInfo._tbColor == FluentColor::blue);
	checkOrUncheckBtn(IDC_RADIO_PURPLE, nppGUITbInfo._tbColor == FluentColor::purple);
	checkOrUncheckBtn(IDC_RADIO_CYAN, nppGUITbInfo._tbColor == FluentColor::cyan);
	checkOrUncheckBtn(IDC_RADIO_OLIVE, nppGUITbInfo._tbColor == FluentColor::olive);
	checkOrUncheckBtn(IDC_RADIO_YELLOW, nppGUITbInfo._tbColor == FluentColor::yellow);
	checkOrUncheckBtn(IDC_RADIO_ACCENTCOLOR, nppGUITbInfo._tbColor == FluentColor::accent);
	checkOrUncheckBtn(IDC_RADIO_CUSTOMCOLOR, nppGUITbInfo._tbColor == FluentColor::custom);
	checkOrUncheckBtn(IDC_RADIO_DEFAULTCOLOR, nppGUITbInfo._tbColor == FluentColor::defaultColor);

	checkOrUncheckBtn(IDC_RADIO_COMPLETE, nppGUITbInfo._tbUseMono);
	checkOrUncheckBtn(IDC_RADIO_PARTIAL, !nppGUITbInfo._tbUseMono);

	_toolbarSubDlg.changeColorableState(true);

	return TRUE;
}

void PreferenceDlg::makeCategoryList()
{
	const QSignalBlocker blocker(_categories);
	for (const PageInfo& info : _wVector)
		_categories->addItem(QString::fromStdWString(info._name));

	fitCategoryList();
	setListSelection(0);
}

// All the pages in view with their whole names, whatever the style, the font and the language make of them
void PreferenceDlg::fitCategoryList()
{
	constexpr int minimumWidth = 170;
	constexpr int padding = 8;
	const int frame = 2 * _categories->frameWidth();
	_categories->setFixedWidth(std::max(minimumWidth, _categories->sizeHintForColumn(0) + frame + padding));
	_categories->setMinimumHeight(_categories->sizeHintForRow(0) * _categories->count() + frame);
}

int PreferenceDlg::getListSelectedIndex() const
{
	return _categories ? _categories->currentRow() : -1;
}

int PreferenceDlg::getIndexFromName(const wchar_t* name) const
{
	if (!name)
		return -1;

	int i = 0;
	for (auto it = _wVector.begin() ; it != _wVector.end(); ++it, ++i)
	{
		if (it->_internalName == name)
			return i;
	}
	return -1;
}

bool PreferenceDlg::setListSelection(size_t currentSel) const
{
	const QSignalBlocker blocker(_categories);
	_categories->setCurrentRow(static_cast<int>(currentSel));
	return true;
}

bool PreferenceDlg::renameDialogTitle(const wchar_t *internalName, const wchar_t *newName)
{
	for (size_t i = 0, len = _wVector.size(); i < len; ++i)
	{
		if (_wVector[i]._internalName == internalName)
		{
			_categories->item(static_cast<int>(i))->setText(QString::fromWCharArray(newName));
			fitCategoryList();
			return true;
		}
	}
	return false;
}

void PreferenceDlg::showDialogByName(const wchar_t *name) const
{
	const int i = getIndexFromName(name);
	if (i >= 0)
	{
		showDialogByIndex(i);
		setListSelection(i);
	}
}

void PreferenceDlg::showDialogByIndex(size_t index) const
{
	_pages->setCurrentWidget(_wVector[index]._dlg->getHSelf());
}

void PreferenceDlg::destroy()
{
	if (_hSelf)
		nppUnregisterMessageTarget(_hSelf);

	for (const PageInfo& info : _wVector)
		info._dlg->destroy();
	_wVector.clear();

	StaticDialog::destroy();
	_pages = nullptr;
	_categories = nullptr;
}

void TabbarSubDlg::setTabbarAlternateIcons(bool enable)
{
	NppGUI& nppGUI = NppParameters::getInstance().getNppGUI();
	if (!enable)
	{
		nppGUI._tabStatus &= ~TAB_ALTICONS;
		setChecked(IDC_CHECK_TAB_ALTICONS, false);
	}
	else
	{
		nppGUI._tabStatus |= TAB_ALTICONS;
		setChecked(IDC_CHECK_TAB_ALTICONS, true);
	}
}

UINT ToolbarSubDlg::getToolbarIconSetMsg(int* iconSetID)
{

	bool isChecked = isCheckedOrNot(IDC_RADIO_STANDARD);
	if (isChecked)
	{
		if (iconSetID)
			*iconSetID = IDC_RADIO_STANDARD;
		return NPPM_INTERNAL_TOOLBARSTANDARD;
	}

	isChecked = isCheckedOrNot(IDC_RADIO_SMALLICON);
	if (isChecked)
	{
		if (iconSetID)
			*iconSetID = IDC_RADIO_SMALLICON;
		return NPPM_INTERNAL_TOOLBARREDUCE;
	}

	isChecked = isCheckedOrNot(IDC_RADIO_BIGICON);
	if (isChecked)
	{
		if (iconSetID)
			*iconSetID = IDC_RADIO_BIGICON;
		return NPPM_INTERNAL_TOOLBARENLARGE;
	}

	isChecked = isCheckedOrNot(IDC_RADIO_SMALLICON2);
	if (isChecked)
	{
		if (iconSetID)
			*iconSetID = IDC_RADIO_SMALLICON2;
		return NPPM_INTERNAL_TOOLBARREDUCESET2;
	}

	isChecked = isCheckedOrNot(IDC_RADIO_BIGICON2);
	if (isChecked)
	{
		if (iconSetID)
			*iconSetID = IDC_RADIO_BIGICON2;
		return NPPM_INTERNAL_TOOLBARENLARGESET2;
	}

	return NPPM_INTERNAL_TOOLBARSTANDARD;
}

void ToolbarSubDlg::enableIconColorPicker(bool enable, bool useDark)
{
	_pIconColorPicker->getHSelf()->setEnabled(enable);

	if (enable)
	{
		const auto& tbIconInfo = NppDarkMode::getToolbarIconInfo(useDark);
		_pIconColorPicker->setColour(tbIconInfo._tbCustomColor);
	}
	else
	{
		COLORREF disabledColor = useDark ? NppDarkMode::getDlgBackgroundColor() : windowFaceColour(_hSelf);
		_pIconColorPicker->setColour(disabledColor);
	}

	_pIconColorPicker->redraw();
}

intptr_t GeneralSubDlg::onInit()
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = nppParam.getNppGUI();

	bool showStatus = nppGUI._statusBarShow;
	bool showMenu = nppGUI._menuBarShow;
	bool hideRightShortcutsFromMenu = nppGUI._hideMenuRightShortcuts;

	setChecked(IDC_CHECK_HIDESTATUSBAR, !showStatus);
	setChecked(IDC_CHECK_HIDEMENUBAR, !showMenu);
	setChecked(IDC_CHECK_HIDERIGHTSHORTCUTSOFMENUBAR, hideRightShortcutsFromMenu);

	LocalizationSwitcher & localizationSwitcher = nppParam.getLocalizationSwitcher();

	for (size_t i = 0, len = localizationSwitcher.size(); i < len ; ++i)
	{
		pair<wstring, wstring> localizationInfo = localizationSwitcher.getElementFromIndex(i);
		addComboItem(IDC_COMBO_LOCALIZATION, localizationInfo.first.c_str());
	}
	wstring lang = L"English"; // Set default language as English
	if (nppParam.getNativeLang()) // if nativeLang is not nullptr, then we can be sure the default language (English) is not used
	{
		string fn = localizationSwitcher.getFileName();
		wstring fnW = string2wstring(fn, CP_UTF8);
		lang = localizationSwitcher.getLangFromXmlFileName(fnW.c_str());
	}
	auto index = findComboItem(IDC_COMBO_LOCALIZATION, lang.c_str());
	if (index != -1)
		combo(IDC_COMBO_LOCALIZATION)->setCurrentIndex(static_cast<int>(index));

	static constexpr const wchar_t* fontSizes[]{ L"8", L"9", L"10", L"11", L"12", L"14", L"16", L"18", L"20", L"22", L"24" };

	const wstring systemFont = NppParameters::getInstance().getNativeLangSpeaker()->getLocalizedStrFromID("dialog-font-system", L"System default");
	addComboItem(IDC_COMBO_DIALOG_FONT_SIZE, systemFont);
	for (const auto& fontSize : fontSizes)
	{
		addComboItem(IDC_COMBO_DIALOG_FONT_SIZE, fontSize);
	}

	const WORD currentFontSize = NppParameters::getInstance().getDlgFontSize();
	const auto cbFontSzIdx = (currentFontSize == 0) ? 0 : findComboItem(IDC_COMBO_DIALOG_FONT_SIZE, std::to_wstring(currentFontSize));
	combo(IDC_COMBO_DIALOG_FONT_SIZE)->setCurrentIndex(std::max(static_cast<int>(cbFontSzIdx), 0));

	NppParameters& nppParamInst = NppParameters::getInstance();
	const std::vector<wstring>& fontlist = nppParamInst.getFontList();

	addComboItem(IDC_COMBO_DIALOG_FONT_NAME, systemFont);
	for (size_t i = 0, len = fontlist.size(); i < len; ++i)
	{
		addComboItem(IDC_COMBO_DIALOG_FONT_NAME, fontlist[i]);
	}

	int iFontName = 0;
	if (!nppParamInst.getNppGUI()._fontDlgName.empty())
	{
		iFontName = findComboItem(IDC_COMBO_DIALOG_FONT_NAME, nppParamInst.getNppGUI()._fontDlgName);
		if (iFontName == -1)
		{
			nppParamInst.getNppGUI()._fontDlgName.clear();
			iFontName = 0;
		}
	}

	combo(IDC_COMBO_DIALOG_FONT_NAME)->setCurrentIndex(iFontName);

	return TRUE;
}

intptr_t GeneralSubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = nppParam.getNppGUI();

	switch (wParam)
	{
		case IDC_CHECK_HIDESTATUSBAR:
		{
			const bool isChecked = isCheckedOrNot(IDC_CHECK_HIDESTATUSBAR);
			::SendMessage(mainWindow(), NPPM_HIDESTATUSBAR, 0, isChecked ? TRUE : FALSE);
		}
		return TRUE;

		case IDC_CHECK_HIDEMENUBAR :
		{
			bool isChecked = isCheckedOrNot(IDC_CHECK_HIDEMENUBAR);
			::SendMessage(mainWindow(), NPPM_HIDEMENU, 0, isChecked?TRUE:FALSE);
		}
		return TRUE;

		case IDC_CHECK_HIDERIGHTSHORTCUTSOFMENUBAR:
		{
			bool isChecked = isCheckedOrNot(IDC_CHECK_HIDERIGHTSHORTCUTSOFMENUBAR);
			nppGUI._hideMenuRightShortcuts = isChecked;
			static bool isFirstShow = true;
			if (isChecked)
			{
				::SendMessage(mainWindow(), NPPM_INTERNAL_HIDEMENURIGHTSHORTCUTS, 0, isChecked ? TRUE : FALSE);
			}
			else if (isFirstShow)
			{
				NativeLangSpeaker* pNativeSpeaker = nppParam.getNativeLangSpeaker();
				pNativeSpeaker->messageBox("Need2Restart2ShowMenuShortcuts",
					_hSelf,
					L"Notepad++ needs to be restarted to show right menu shortcuts.",
					L"Notepad++ need to be restarted",
					MB_OK | MB_APPLMODAL);

				isFirstShow = false;
			}
		}
		return TRUE;

		default:
			switch (HIWORD(wParam))
			{
				case CBN_SELCHANGE: // == case LBN_SELCHANGE :
				{
					switch (LOWORD(wParam))
					{
						case IDC_COMBO_LOCALIZATION:
						{
							LocalizationSwitcher& localizationSwitcher = nppParam.getLocalizationSwitcher();
							auto index = combo(IDC_COMBO_LOCALIZATION)->currentIndex();
							const wstring langName = comboText(IDC_COMBO_LOCALIZATION, index);
							if (!langName.empty())
							{
								// Make English as basic language
								if (localizationSwitcher.switchToLang(L"English"))
								{
									::SendMessage(mainWindow(), NPPM_INTERNAL_RELOADNATIVELANG, FALSE, 0);
								}

								// Change the language 
								if (localizationSwitcher.switchToLang(langName.c_str()))
								{
									::SendMessage(mainWindow(), NPPM_INTERNAL_RELOADNATIVELANG, TRUE, 0);
									_hParent->update();
								}
							}
							return TRUE;
						}

						case IDC_COMBO_DIALOG_FONT_SIZE:
						{
							// The first item is the system's size, which has no number
							NppParameters::getInstance().getNppGUI()._fontDlgSize = static_cast<WORD>(combo(IDC_COMBO_DIALOG_FONT_SIZE)->currentText().toInt());
							return TRUE;
						}

						case IDC_COMBO_DIALOG_FONT_NAME:
						{
							const int fontIndex = combo(IDC_COMBO_DIALOG_FONT_NAME)->currentIndex();
							NppParameters::getInstance().getNppGUI()._fontDlgName = (fontIndex <= 0) ? std::wstring{} : comboText(IDC_COMBO_DIALOG_FONT_NAME, fontIndex);

							return TRUE;
						}
					}
				}
				return TRUE;
			}
	}
	return FALSE;
}


intptr_t ToolbarSubDlg::onInit()
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = nppParam.getNppGUI();

	auto& nppGUITbInfo = nppGUI._tbIconInfo;

	setChecked(IDC_CHECK_HIDE, !nppGUI._toolbarShow);

	int ID2Check = 0;
	switch (nppGUITbInfo._tbIconSet)
	{
		case TB_SMALL:
			ID2Check = IDC_RADIO_SMALLICON;
			break;
		case TB_LARGE:
			ID2Check = IDC_RADIO_BIGICON;
			break;
		case TB_SMALL2:
			ID2Check = IDC_RADIO_SMALLICON2;
			break;
		case TB_LARGE2:
			ID2Check = IDC_RADIO_BIGICON2;
			break;
		case TB_STANDARD:
		default:
			ID2Check = IDC_RADIO_STANDARD;
	}
	setChecked(ID2Check, true);


	ID2Check = 0;
	switch (nppGUITbInfo._tbColor)
	{
		case FluentColor::custom:
			ID2Check = IDC_RADIO_CUSTOMCOLOR;
			break;
		case FluentColor::accent:
			ID2Check = IDC_RADIO_ACCENTCOLOR;
			break;
		case FluentColor::red:
			ID2Check = IDC_RADIO_RED;
			break;
		case FluentColor::green:
			ID2Check = IDC_RADIO_GREEN;
			break;
		case FluentColor::blue:
			ID2Check = IDC_RADIO_BLUE;
			break;
		case FluentColor::purple:
			ID2Check = IDC_RADIO_PURPLE;
			break;
		case FluentColor::cyan:
			ID2Check = IDC_RADIO_CYAN;
			break;
		case FluentColor::olive:
			ID2Check = IDC_RADIO_OLIVE;
			break;
		case FluentColor::yellow:
			ID2Check = IDC_RADIO_YELLOW;
			break;
		case FluentColor::defaultColor:
		default:
			ID2Check = IDC_RADIO_DEFAULTCOLOR;
	}
	setChecked(ID2Check, true);

	NativeLangSpeaker* pNativeSpeaker = nppParam.getNativeLangSpeaker();
	wstring findInFilesFilterTip = pNativeSpeaker->getLocalizedStrFromID("toolbar-accent-tip", L"This option makes your toolbar icons follow Windows system accent color. Accent color is the highlight color used in buttons, borders, and Start menu tiles in Windows. To change it, go to Settings > Personalization > Colors, then select your preferred accent color.");
	setTip(IDD_ACCENT_TIP_STATIC, findInFilesFilterTip);

	setChecked(nppGUITbInfo._tbUseMono ? IDC_RADIO_COMPLETE : IDC_RADIO_PARTIAL, true);

	_pIconColorPicker = std::make_unique<ColourPicker>();
	_pIconColorPicker->init(_hInst, _hSelf);
	_pIconColorPicker->disableRightClick();
	placePicker(IDC_RADIO_CUSTOMCOLOR, *_pIconColorPicker, false);
	_pIconColorPicker->display();
	_pIconColorPicker->setColour(nppGUI._tbIconInfo._tbCustomColor);

	if (nppGUITbInfo._tbIconSet == TB_STANDARD)
	{
		changeColorableState(false);
	}
	else // Eventually color picker needs to be disabled, if it's not custom choice.
	{
		if (ID2Check != IDC_RADIO_CUSTOMCOLOR)
		{
			enableIconColorPicker(false, NppDarkMode::isEnabled());
		}
	}

	return TRUE;
}

void ToolbarSubDlg::changeColorableState(bool isForDarkModeChange)
{
	bool enableColor = !isCheckedOrNot(IDC_RADIO_STANDARD);
	bool enableCustom = isCheckedOrNot(IDC_RADIO_CUSTOMCOLOR);

	enableItem(IDC_RADIO_COMPLETE, enableColor);
	enableItem(IDC_RADIO_PARTIAL, enableColor);

	enableItem(IDC_RADIO_RED, enableColor);
	enableItem(IDC_RADIO_GREEN, enableColor);
	enableItem(IDC_RADIO_BLUE, enableColor);
	enableItem(IDC_RADIO_PURPLE, enableColor);
	enableItem(IDC_RADIO_CYAN, enableColor);
	enableItem(IDC_RADIO_OLIVE, enableColor);
	enableItem(IDC_RADIO_YELLOW, enableColor);
	enableItem(IDC_RADIO_DEFAULTCOLOR, enableColor);
	enableItem(IDC_RADIO_ACCENTCOLOR, enableColor);
	enableItem(IDC_RADIO_CUSTOMCOLOR, enableColor);

	bool useDark = isForDarkModeChange ? !NppDarkMode::isEnabled() : NppDarkMode::isEnabled();
	enableIconColorPicker(enableColor && enableCustom, useDark);
}


intptr_t ToolbarSubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = nppParam.getNppGUI();

	switch (wParam)
	{
		case IDC_CHECK_HIDE:
		{
			bool isChecked = isCheckedOrNot(IDC_CHECK_HIDE);
			::SendMessage(mainWindow(), NPPM_HIDETOOLBAR, 0, isChecked ? TRUE : FALSE);
			return TRUE;
		}

		case IDC_RADIO_SMALLICON:
		{
			::SendMessage(mainWindow(), NPPM_INTERNAL_TOOLBARREDUCE, 0, 0);
			NppDarkMode::setToolbarIconSet(0, NppDarkMode::isEnabled());
			changeColorableState(false);
			return TRUE;
		}

		case IDC_RADIO_BIGICON:
		{
			::SendMessage(mainWindow(), NPPM_INTERNAL_TOOLBARENLARGE, 0, 0);
			NppDarkMode::setToolbarIconSet(1, NppDarkMode::isEnabled());
			changeColorableState(false);
			return TRUE;
		}

		case IDC_RADIO_SMALLICON2:
		{
			::SendMessage(mainWindow(), NPPM_INTERNAL_TOOLBARREDUCESET2, 0, 0);
			NppDarkMode::setToolbarIconSet(2, NppDarkMode::isEnabled());
			changeColorableState(false);
			return TRUE;
		}

		case IDC_RADIO_BIGICON2:
		{
			::SendMessage(mainWindow(), NPPM_INTERNAL_TOOLBARENLARGESET2, 0, 0);
			NppDarkMode::setToolbarIconSet(3, NppDarkMode::isEnabled());
			changeColorableState(false);
			return TRUE;
		}

		case IDC_RADIO_STANDARD:
		{
			::SendMessage(mainWindow(), NPPM_INTERNAL_TOOLBARSTANDARD, 0, 0);
			NppDarkMode::setToolbarIconSet(4, NppDarkMode::isEnabled());
			changeColorableState(false);
			return TRUE;
		}



		case IDC_RADIO_CUSTOMCOLOR:
		{
			FluentColor c = FluentColor::custom;
			NppDarkMode::setToolbarFluentColor(c);
			nppGUI._tbIconInfo._tbColor = c;
			UINT msg = getToolbarIconSetMsg(nullptr);
			::SendMessage(mainWindow(), msg, TRUE, 0);
			enableIconColorPicker(true, NppDarkMode::isEnabled());
			return TRUE;
		}

		case IDC_RADIO_ACCENTCOLOR:
		{
			FluentColor c = FluentColor::accent;
			NppDarkMode::setToolbarFluentColor(c);
			nppGUI._tbIconInfo._tbColor = c;
			UINT msg = getToolbarIconSetMsg(nullptr);
			::SendMessage(mainWindow(), msg, TRUE, 0);
			enableIconColorPicker(false, NppDarkMode::isEnabled());
			return TRUE;
		}

		case IDC_RADIO_RED:
		{
			FluentColor c = FluentColor::red;
			NppDarkMode::setToolbarFluentColor(c);
			nppGUI._tbIconInfo._tbColor = c;
			UINT msg = getToolbarIconSetMsg(nullptr);
			::SendMessage(mainWindow(), msg, TRUE, 0);
			enableIconColorPicker(false, NppDarkMode::isEnabled());
			return TRUE;
		}

		case IDC_RADIO_GREEN:
		{
			FluentColor c = FluentColor::green;
			NppDarkMode::setToolbarFluentColor(c);
			nppGUI._tbIconInfo._tbColor = c;
			UINT msg = getToolbarIconSetMsg(nullptr);
			::SendMessage(mainWindow(), msg, TRUE, 0);
			enableIconColorPicker(false, NppDarkMode::isEnabled());
			return TRUE;
		}

		case IDC_RADIO_BLUE:
		{
			FluentColor c = FluentColor::blue;
			NppDarkMode::setToolbarFluentColor(c);
			nppGUI._tbIconInfo._tbColor = c;
			UINT msg = getToolbarIconSetMsg(nullptr);
			::SendMessage(mainWindow(), msg, TRUE, 0);
			enableIconColorPicker(false, NppDarkMode::isEnabled());
			return TRUE;
		}

		case IDC_RADIO_PURPLE:
		{
			FluentColor c = FluentColor::purple;
			NppDarkMode::setToolbarFluentColor(c);
			nppGUI._tbIconInfo._tbColor = c;
			UINT msg = getToolbarIconSetMsg(nullptr);
			::SendMessage(mainWindow(), msg, TRUE, 0);
			enableIconColorPicker(false, NppDarkMode::isEnabled());
			return TRUE;
		}

		case IDC_RADIO_CYAN:
		{
			FluentColor c = FluentColor::cyan;
			NppDarkMode::setToolbarFluentColor(c);
			nppGUI._tbIconInfo._tbColor = c;
			UINT msg = getToolbarIconSetMsg(nullptr);
			::SendMessage(mainWindow(), msg, TRUE, 0);
			enableIconColorPicker(false, NppDarkMode::isEnabled());
			return TRUE;
		}

		case IDC_RADIO_OLIVE:
		{
			FluentColor c = FluentColor::olive;
			NppDarkMode::setToolbarFluentColor(c);
			nppGUI._tbIconInfo._tbColor = c;
			UINT msg = getToolbarIconSetMsg(nullptr);
			::SendMessage(mainWindow(), msg, TRUE, 0);
			enableIconColorPicker(false, NppDarkMode::isEnabled());
			return TRUE;
		}

		case IDC_RADIO_YELLOW:
		{
			FluentColor c = FluentColor::yellow;
			NppDarkMode::setToolbarFluentColor(c);
			nppGUI._tbIconInfo._tbColor = c;
			UINT msg = getToolbarIconSetMsg(nullptr);
			::SendMessage(mainWindow(), msg, TRUE, 0);
			enableIconColorPicker(false, NppDarkMode::isEnabled());
			return TRUE;
		}

		case IDC_RADIO_DEFAULTCOLOR:
		{
			FluentColor c = FluentColor::defaultColor;
			NppDarkMode::setToolbarFluentColor(c);
			nppGUI._tbIconInfo._tbColor = c;
			UINT msg = getToolbarIconSetMsg(nullptr);
			::SendMessage(mainWindow(), msg, TRUE, 0);
			enableIconColorPicker(false, NppDarkMode::isEnabled());
			return TRUE;
		}



		case IDC_RADIO_COMPLETE:
		{
			NppDarkMode::setToolbarFluentMonochrome(true);
			nppGUI._tbIconInfo._tbUseMono = true;

			UINT msg = getToolbarIconSetMsg(nullptr);
			::SendMessage(mainWindow(), msg, TRUE, 0);
			return TRUE;
		}

		case IDC_RADIO_PARTIAL:
		{
			NppDarkMode::setToolbarFluentMonochrome(false);
			nppGUI._tbIconInfo._tbUseMono = false;

			UINT msg = getToolbarIconSetMsg(nullptr);
			::SendMessage(mainWindow(), msg, TRUE, 0);
			return TRUE;
		}


		default:
		{

			switch (HIWORD(wParam))
			{
				case CPN_COLOURPICKED:
				{
					if (reinterpret_cast<HWND>(lParam) == _pIconColorPicker->getHSelf())
					{
						COLORREF c = _pIconColorPicker->getColour();
						nppGUI._tbIconInfo._tbCustomColor = c;
						NppDarkMode::setToolbarFluentCustomColor(c);

						UINT msg = getToolbarIconSetMsg(nullptr);
						::SendMessage(mainWindow(), msg, TRUE, 0);

						return TRUE;
					}
					break;
				}
			}
		}
	}
	return FALSE;
}


intptr_t TabbarSubDlg::onInit()
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = nppParam.getNppGUI();

	int tabBarStatus = nppGUI._tabStatus;

	setChecked(IDC_CHECK_REDUCE, tabBarStatus & TAB_REDUCE);
	setChecked(IDC_CHECK_LOCK, !(tabBarStatus & TAB_DRAGNDROP));
	setChecked(IDC_CHECK_ORANGE, tabBarStatus & TAB_DRAWTOPBAR);
	setChecked(IDC_CHECK_DRAWINACTIVE, tabBarStatus & TAB_DRAWINACTIVETAB);

	bool showCloseButton = tabBarStatus & TAB_CLOSEBUTTON;
	bool enablePinButton = tabBarStatus & TAB_PINBUTTON;
	bool showOnlyPinnedButton = tabBarStatus & TAB_SHOWONLYPINNEDBUTTON;
	bool showButtonOnInactiveTabs = tabBarStatus & TAB_INACTIVETABSHOWBUTTON;
	bool hideTabbar = tabBarStatus & TAB_HIDE;

	setChecked(IDC_CHECK_ENABLETABCLOSE, showCloseButton);
	setChecked(IDC_CHECK_ENABLETABPIN, enablePinButton);
	setChecked(IDC_CHECK_SHOWONLYPINNEDBUTTON, showOnlyPinnedButton);
	setChecked(IDC_CHECK_INACTTABDRAWBUTTON, showButtonOnInactiveTabs);

	if (!(showCloseButton || enablePinButton))
	{
		nppGUI._tabStatus &= ~TAB_INACTIVETABSHOWBUTTON;
		setChecked(IDC_CHECK_INACTTABDRAWBUTTON, FALSE);
		enableItem(IDC_CHECK_INACTTABDRAWBUTTON, FALSE);
	}

	if (!enablePinButton)
	{
		enableItem(IDC_CHECK_SHOWONLYPINNEDBUTTON, FALSE);
	}

	setChecked(IDC_CHECK_DBCLICK2CLOSE, tabBarStatus & TAB_DBCLK2CLOSE);
	setChecked(IDC_CHECK_TAB_VERTICAL, tabBarStatus & TAB_VERTICAL);
	setChecked(IDC_CHECK_TAB_MULTILINE, tabBarStatus & TAB_MULTILINE);
	setChecked(IDC_CHECK_TAB_LAST_EXIT, tabBarStatus & TAB_QUITONEMPTY);
	setChecked(IDC_CHECK_TAB_ALTICONS, tabBarStatus & TAB_ALTICONS);

	setChecked(IDC_CHECK_TAB_HIDE, tabBarStatus & TAB_HIDE);
	if (hideTabbar)
		onCommand(IDC_CHECK_TAB_HIDE, 0);

	setItemInt(IDC_EDIT_TABCOMPACTLABELLEN, nppParam.getNbTabCompactLabelLen());

	NativeLangSpeaker* pNativeSpeaker = nppParam.getNativeLangSpeaker();
	wstring tabCompactLabelLenTip = pNativeSpeaker->getLocalizedStrFromID("tabbar-tabcompactlabellen-tip",
		L"Limits the visible length of long tab names. Enter to apply the given value. Value range: 1-257 characters (0 disables the truncation).");
	setTip(IDC_TABCOMPACTLABELLEN_TIP_STATIC, tabCompactLabelLenTip);


	return TRUE;
}


intptr_t TabbarSubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = nppParam.getNppGUI();

	switch (LOWORD(wParam))
	{
		case IDC_EDIT_TABCOMPACTLABELLEN:
		{
			switch (HIWORD(wParam))
			{
				case EN_KILLFOCUS:
				{
					const wstring str = getItemText(IDC_EDIT_TABCOMPACTLABELLEN).substr(0, 3);
					if (str.empty())
					{
						// user removed the value completely, the compacting is considered as disabled
						nppParam.setNbTabCompactLabelLen(0);
						setItemInt(IDC_EDIT_TABCOMPACTLABELLEN, 0);
						::SendMessage(mainWindow(), NPPM_INTERNAL_SETTING_TABCOMPACTLABELLEN, 0, 0);
						return FALSE;
					}

					// not using the GetDlgItemInt for obtaining the edit-ctrl value as it fails for negative numbers inside
					// (it can be inserted even into such a ES_NUMBER edit-ctrl via paste-cmd)
					int iSize = 0;
					try {
						iSize = stoi(str);
					}
					catch ([[maybe_unused]] invalid_argument const& ex)
					{
						iSize = -1;
					}
					catch ([[maybe_unused]] out_of_range const& ex)
					{
						iSize = -1;
					}

					if ((iSize >= 0) && (iSize == static_cast<int>(nppParam.getNbTabCompactLabelLen())))
						return FALSE; // nothing changed

					bool change = false;

					if (iSize < 0)
					{
						iSize = 0;
						change = true;
					}
					else if (iSize > NB_MAX_TAB_COMPACT_LABEL_LEN)
					{
						iSize = NB_MAX_TAB_COMPACT_LABEL_LEN;
						change = true;
					}

					if (change)
					{
						setItemInt(IDC_EDIT_TABCOMPACTLABELLEN, iSize);
					}

					nppParam.setNbTabCompactLabelLen(iSize);
					::SendMessage(mainWindow(), NPPM_INTERNAL_SETTING_TABCOMPACTLABELLEN, 0, 0);

					return TRUE;
				}

				default:
					break;
			}

			return FALSE;
		}

		default:
			break;
	}

	switch (wParam)
	{
		case IDC_CHECK_TAB_HIDE:
		{
			bool toBeHidden = isCheckedOrNot(IDC_CHECK_TAB_HIDE);
			if (toBeHidden)
				nppGUI._tabStatus |= TAB_HIDE;
			else
				nppGUI._tabStatus &= ~TAB_HIDE;

			enableItem(IDC_CHECK_TAB_MULTILINE, !toBeHidden);
			enableItem(IDC_CHECK_TAB_VERTICAL, !toBeHidden);
			enableItem(IDC_CHECK_REDUCE, !toBeHidden);
			enableItem(IDC_CHECK_LOCK, !toBeHidden);
			enableItem(IDC_CHECK_ORANGE, !toBeHidden);
			enableItem(IDC_CHECK_DRAWINACTIVE, !toBeHidden);
			enableItem(IDC_CHECK_ENABLETABCLOSE, !toBeHidden);
			enableItem(IDC_CHECK_ENABLETABPIN, !toBeHidden);
			enableItem(IDC_CHECK_SHOWONLYPINNEDBUTTON, !toBeHidden);
			enableItem(IDC_CHECK_INACTTABDRAWBUTTON, !toBeHidden);
			enableItem(IDC_CHECK_DBCLICK2CLOSE, !toBeHidden);
			enableItem(IDC_CHECK_TAB_LAST_EXIT, !toBeHidden);
			enableItem(IDC_CHECK_TAB_ALTICONS, !toBeHidden);

			::SendMessage(mainWindow(), WM_SIZE, 0, 0);

			// At startup, if "-notabbar" or "asNotepad.xml" is used, and tab bar was not hidden in the previous session,
			// we force tab bar visible in the next session by setting _forceTabbarVisible to true.
			// However, if "Hide tab bar" option is changed manually by user, then we don't force tab bar visible for the next session,
			// and apply user's choice instead.
			nppGUI._forceTabbarVisible = false;

			return TRUE;
		}

		case  IDC_CHECK_TAB_VERTICAL:
		{
			const bool isChecked = isCheckedOrNot(IDC_CHECK_TAB_VERTICAL);
			if (isChecked)
				nppGUI._tabStatus |= TAB_VERTICAL;
			else
				nppGUI._tabStatus &= ~TAB_VERTICAL;

			::SendMessage(mainWindow(), NPPM_INTERNAL_VERTICALTABBAR, 0, 0);
			return TRUE;
		}

		case IDC_CHECK_TAB_MULTILINE:
		{

			const bool isChecked = isCheckedOrNot(IDC_CHECK_TAB_MULTILINE);
			if (isChecked)
				nppGUI._tabStatus |= TAB_MULTILINE;
			else
				nppGUI._tabStatus &= ~TAB_MULTILINE;

			::SendMessage(mainWindow(), NPPM_INTERNAL_MULTILINETABBAR, 0, 0);
			return TRUE;
		}

		case IDC_CHECK_TAB_LAST_EXIT:
		{
			const bool isChecked = isCheckedOrNot(IDC_CHECK_TAB_LAST_EXIT);
			if (isChecked)
				nppGUI._tabStatus |= TAB_QUITONEMPTY;
			else
				nppGUI._tabStatus &= ~TAB_QUITONEMPTY;
		}
		return TRUE;

		case IDC_CHECK_TAB_ALTICONS:
		{
			const bool isChecked = isCheckedOrNot(IDC_CHECK_TAB_ALTICONS);
			if (isChecked)
				nppGUI._tabStatus |= TAB_ALTICONS;
			else
				nppGUI._tabStatus &= ~TAB_ALTICONS;

			const bool isBtnCmd = true;
			::SendMessage(mainWindow(), NPPM_INTERNAL_CHANGETABBARICONSET, static_cast<WPARAM>(isBtnCmd), isChecked ? 1 : (nppGUI._darkmode._isEnabled ? 2 : 0));
			NppDarkMode::setTabIconSet(isChecked, NppDarkMode::isEnabled());
			return TRUE;
		}

		case IDC_CHECK_REDUCE:
		{
			const bool isChecked = isCheckedOrNot(IDC_CHECK_REDUCE);
			if (isChecked)
				nppGUI._tabStatus |= TAB_REDUCE;
			else
				nppGUI._tabStatus &= ~TAB_REDUCE;

			::SendMessage(mainWindow(), NPPM_INTERNAL_REDUCETABBAR, 0, 0);
			return TRUE;
		}

		case IDC_CHECK_LOCK:
		{
			bool isLocked = isCheckedOrNot(IDC_CHECK_LOCK);
			if (isLocked)
				nppGUI._tabStatus &= ~TAB_DRAGNDROP;
			else
				nppGUI._tabStatus |= TAB_DRAGNDROP;

			return TRUE;
		}

		case IDC_CHECK_ORANGE:
		{
			const bool isChecked = isCheckedOrNot(IDC_CHECK_ORANGE);
			if (isChecked)
				nppGUI._tabStatus |= TAB_DRAWTOPBAR;
			else
				nppGUI._tabStatus &= ~TAB_DRAWTOPBAR;

			::SendMessage(mainWindow(), NPPM_INTERNAL_DRAWTABTOPBAR, 0, 0);
			return TRUE;
		}

		case IDC_CHECK_DRAWINACTIVE:
		{
			const bool isChecked = isCheckedOrNot(IDC_CHECK_DRAWINACTIVE);
			if (isChecked)
				nppGUI._tabStatus |= TAB_DRAWINACTIVETAB;
			else
				nppGUI._tabStatus &= ~TAB_DRAWINACTIVETAB;

			::SendMessage(mainWindow(), NPPM_INTERNAL_DRAWINACTIVETAB, 0, 0);
			return TRUE;
		}

		case IDC_CHECK_ENABLETABCLOSE:
		case IDC_CHECK_ENABLETABPIN:
		{
			if (wParam == IDC_CHECK_ENABLETABCLOSE)
			{
				const bool isChecked = isCheckedOrNot(IDC_CHECK_ENABLETABCLOSE);
				if (isChecked)
					nppGUI._tabStatus |= TAB_CLOSEBUTTON;
				else
					nppGUI._tabStatus &= ~TAB_CLOSEBUTTON;
			}
			else
			{
				const bool isChecked = isCheckedOrNot(IDC_CHECK_ENABLETABPIN);
				if (isChecked)
					nppGUI._tabStatus |= TAB_PINBUTTON;
				else
					nppGUI._tabStatus &= ~TAB_PINBUTTON;
			}

			::SendMessage(mainWindow(), wParam == IDC_CHECK_ENABLETABCLOSE ? NPPM_INTERNAL_DRAWTABBARCLOSEBUTTON : NPPM_INTERNAL_DRAWTABBARPINBUTTON, 0, 0);

			bool showCloseButton = isCheckedOrNot(IDC_CHECK_ENABLETABCLOSE);
			bool enablePinButton = isCheckedOrNot(IDC_CHECK_ENABLETABPIN);

			if (!(showCloseButton || enablePinButton))
			{
				nppGUI._tabStatus &= ~TAB_INACTIVETABSHOWBUTTON;
				setChecked(IDC_CHECK_INACTTABDRAWBUTTON, FALSE);
				::SendMessage(mainWindow(), NPPM_INTERNAL_REFRESHTABBAR, 0, 0);
			}
			enableItem(IDC_CHECK_INACTTABDRAWBUTTON, showCloseButton || enablePinButton);

			if (wParam == IDC_CHECK_ENABLETABPIN)
			{
				enableItem(IDC_CHECK_SHOWONLYPINNEDBUTTON, enablePinButton);
			}

			return TRUE;
		}

		case IDC_CHECK_INACTTABDRAWBUTTON:
		{
			const bool isChecked = isCheckedOrNot(IDC_CHECK_INACTTABDRAWBUTTON);
			if (isChecked)
				nppGUI._tabStatus |= TAB_INACTIVETABSHOWBUTTON;
			else
				nppGUI._tabStatus &= ~TAB_INACTIVETABSHOWBUTTON;

			::SendMessage(mainWindow(), NPPM_INTERNAL_REFRESHTABBAR, 0, 0);
			return TRUE;
		}

		case IDC_CHECK_SHOWONLYPINNEDBUTTON:
		{
			const bool isChecked = isCheckedOrNot(IDC_CHECK_SHOWONLYPINNEDBUTTON);
			if (isChecked)
				nppGUI._tabStatus |= TAB_SHOWONLYPINNEDBUTTON;
			else
				nppGUI._tabStatus &= ~TAB_SHOWONLYPINNEDBUTTON;

			::SendMessage(mainWindow(), NPPM_INTERNAL_REFRESHTABBAR, 0, 0);
			return TRUE;
		}

		case IDC_CHECK_DBCLICK2CLOSE:
		{
			const bool isChecked = isCheckedOrNot(IDC_CHECK_DBCLICK2CLOSE);
			if (isChecked)
				nppGUI._tabStatus |= TAB_DBCLK2CLOSE;
			else
				nppGUI._tabStatus &= ~TAB_DBCLK2CLOSE;
			return TRUE;
		}
	}
	return FALSE;
}


void EditingSubDlg::initScintParam()
{
	NppParameters& nppParam = NppParameters::getInstance();
	ScintillaViewParams & svp = const_cast<ScintillaViewParams &>(nppParam.getSVP());

	int id = 0;
	switch (svp._lineWrapMethod)
	{
		case LINEWRAP_ALIGNED:
			id = IDC_RADIO_LWALIGN;
			break;
		case LINEWRAP_INDENT:
			id = IDC_RADIO_LWINDENT;
			break;
		default : // LINEWRAP_DEFAULT
			id = IDC_RADIO_LWDEF;
	}
	setChecked(id, TRUE);

	setChecked(IDC_CHECK_SMOOTHFONT, svp._doSmoothFont);

	int lineHilite = 0;
	switch (svp._currentLineHiliteMode)
	{
		case LINEHILITE_NONE:
			lineHilite = IDC_RADIO_CLM_NONE;
			break;
		case LINEHILITE_FRAME:
			lineHilite = IDC_RADIO_CLM_FRAME;
			break;
		default : // LINEHILITE_HILITE
			lineHilite = IDC_RADIO_CLM_HILITE;
	}
	setChecked(lineHilite, TRUE);
	enableItem(IDC_CARETLINEFRAME_WIDTH_SLIDER, (svp._currentLineHiliteMode == LINEHILITE_FRAME));

	setChecked(IDC_CHECK_VIRTUALSPACE, svp._virtualSpace);
	setChecked(IDC_CHECK_SCROLLBEYONDLASTLINE, svp._scrollBeyondLastLine);
	setChecked(IDC_CHECK_RIGHTCLICKKEEPSSELECTION, svp._rightClickKeepsSelection);
	setChecked(IDC_CHECK_SELECTEDTEXTSINGLECOLOR, svp._selectedTextForegroundSingleColor);
	setChecked(IDC_CHECK_DISABLEADVANCEDSCROLL, svp._disableAdvancedScrolling);
	setChecked(IDC_CHECK_LINECUTCOPYWITHOUTSELECTION, svp._lineCopyCutWithoutSelection);
	setChecked(IDC_CHECK_DISABLESELECTEDTEXTDRAGDROP, svp._disableSelectedTextDragDrop);
}

void EditingSubDlg::changeLineHiliteMode(bool enableSlider)
{
	enableItem(IDC_CARETLINEFRAME_WIDTH_SLIDER, enableSlider);
	redrawDlgItem(IDC_CARETLINEFRAME_WIDTH_STATIC);
	redrawDlgItem(IDC_CARETLINEFRAME_WIDTH_DISPLAY);
	::SendMessage(mainWindow(), NPPM_INTERNAL_HILITECURRENTLINE, 0, 0);
}

intptr_t EditingSubDlg::onInit()
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI & nppGUI = nppParam.getNppGUI();
	auto& svp = const_cast<ScintillaViewParams&>(nppParam.getSVP());
	
	addComboItem(IDC_WIDTH_COMBO, L"0");
	addComboItem(IDC_WIDTH_COMBO, L"1");
	addComboItem(IDC_WIDTH_COMBO, L"2");
	addComboItem(IDC_WIDTH_COMBO, L"3");
	addComboItem(IDC_WIDTH_COMBO, L"Block");
	addComboItem(IDC_WIDTH_COMBO, L"Block After");

	combo(IDC_WIDTH_COMBO)->setCurrentIndex(nppGUI._caretWidth);
	setChecked(IDC_CHECK_FOLDINGTOGGLE, nppGUI._enableFoldCmdToggable);
	
	slider(IDC_CARETBLINKRATE_SLIDER)->setMinimum(BLINKRATE_FASTEST);
	slider(IDC_CARETBLINKRATE_SLIDER)->setMaximum(BLINKRATE_SLOWEST);
	slider(IDC_CARETBLINKRATE_SLIDER)->setPageStep(BLINKRATE_INTERVAL);
	int blinkRate = (nppGUI._caretBlinkRate == 0) ? BLINKRATE_SLOWEST : nppGUI._caretBlinkRate;
	setSliderPos(IDC_CARETBLINKRATE_SLIDER, blinkRate);

	slider(IDC_CARETLINEFRAME_WIDTH_SLIDER)->setMinimum(CARETLINEFRAME_SMALLEST);
	slider(IDC_CARETLINEFRAME_WIDTH_SLIDER)->setMaximum(CARETLINEFRAME_LARGEST);
	slider(IDC_CARETLINEFRAME_WIDTH_SLIDER)->setPageStep(CARETLINEFRAME_INTERVAL);
	setSliderPos(IDC_CARETLINEFRAME_WIDTH_SLIDER, svp._currentLineFrameWidth);
	setItemInt(IDC_CARETLINEFRAME_WIDTH_DISPLAY, svp._currentLineFrameWidth);

	initScintParam();


	return TRUE;
}

void EditingSubDlg::onScroll(HWND scale)
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = nppParam.getNppGUI();
	auto& svp = const_cast<ScintillaViewParams&>(nppParam.getSVP());

	if (scale == dlgItem(IDC_CARETBLINKRATE_SLIDER))
	{
		auto blinkRate = slider(IDC_CARETBLINKRATE_SLIDER)->value();
		if (blinkRate == BLINKRATE_SLOWEST)
			blinkRate = 0;
		nppGUI._caretBlinkRate = static_cast<int>(blinkRate);

		::SendMessage(mainWindow(), NPPM_INTERNAL_SETCARETBLINKRATE, 0, 0);
	}
	else if (scale == dlgItem(IDC_CARETLINEFRAME_WIDTH_SLIDER))
	{
		svp._currentLineFrameWidth = static_cast<unsigned char>(slider(IDC_CARETLINEFRAME_WIDTH_SLIDER)->value());
		setItemInt(IDC_CARETLINEFRAME_WIDTH_DISPLAY, svp._currentLineFrameWidth);
		::SendMessage(mainWindow(), NPPM_INTERNAL_CARETLINEFRAME, 0, svp._currentLineFrameWidth);
	}
}

intptr_t EditingSubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI & nppGUI = nppParam.getNppGUI();
	auto& svp = const_cast<ScintillaViewParams&>(nppParam.getSVP());
	
	switch (wParam)
	{
		case IDC_CHECK_SMOOTHFONT:
			svp._doSmoothFont = isCheckedOrNot(IDC_CHECK_SMOOTHFONT);
			::SendMessage(mainWindow(), NPPM_SETSMOOTHFONT, 0, svp._doSmoothFont);
			return TRUE;

		case IDC_RADIO_CLM_NONE:
			svp._currentLineHiliteMode = LINEHILITE_NONE;
			changeLineHiliteMode(false);
			return TRUE;

		case IDC_RADIO_CLM_HILITE:
			svp._currentLineHiliteMode = LINEHILITE_HILITE;
			changeLineHiliteMode(false);
			return TRUE;

		case IDC_RADIO_CLM_FRAME:
			svp._currentLineHiliteMode = LINEHILITE_FRAME;
			changeLineHiliteMode(true);
			return TRUE;

		case IDC_CHECK_VIRTUALSPACE:
			svp._virtualSpace = isCheckedOrNot(IDC_CHECK_VIRTUALSPACE);
			::SendMessage(mainWindow(), NPPM_INTERNAL_VIRTUALSPACE, 0, 0);
			return TRUE;

		case IDC_CHECK_SCROLLBEYONDLASTLINE:
			svp._scrollBeyondLastLine = isCheckedOrNot(IDC_CHECK_SCROLLBEYONDLASTLINE);
			::SendMessage(mainWindow(), NPPM_INTERNAL_SCROLLBEYONDLASTLINE, 0, 0);
			return TRUE;

		case IDC_CHECK_LINECUTCOPYWITHOUTSELECTION:
		{
			bool isChecked = isCheckedOrNot(IDC_CHECK_LINECUTCOPYWITHOUTSELECTION);
			svp._lineCopyCutWithoutSelection = isChecked;
			::SendMessage(mainWindow(), NPPM_INTERNAL_LINECUTCOPYWITHOUTSELECTION, 0, 0);
			return TRUE;
		}

		case IDC_CHECK_DISABLESELECTEDTEXTDRAGDROP:
		{
			bool isChecked = isCheckedOrNot(IDC_CHECK_DISABLESELECTEDTEXTDRAGDROP);
			svp._disableSelectedTextDragDrop = isChecked;
			::SendMessage(mainWindow(), NPPM_INTERNAL_DISABLESELECTEDTEXTDRAGDROP, 0, 0);
			return TRUE;
		}

		case IDC_CHECK_RIGHTCLICKKEEPSSELECTION:
			svp._rightClickKeepsSelection = isCheckedOrNot(IDC_CHECK_RIGHTCLICKKEEPSSELECTION);
			return TRUE;

		case IDC_CHECK_SELECTEDTEXTSINGLECOLOR:
			svp._selectedTextForegroundSingleColor = isCheckedOrNot(IDC_CHECK_SELECTEDTEXTSINGLECOLOR);
			::SendMessage(mainWindow(), NPPM_INTERNAL_CHANGESELECTTEXTFORGROUND, 0, 0);
			return TRUE;

		case IDC_CHECK_DISABLEADVANCEDSCROLL:
			svp._disableAdvancedScrolling = isCheckedOrNot(IDC_CHECK_DISABLEADVANCEDSCROLL);
			return TRUE;

		case IDC_CHECK_FOLDINGTOGGLE:
			nppGUI._enableFoldCmdToggable = isCheckedOrNot(IDC_CHECK_FOLDINGTOGGLE);
			return TRUE;

		case IDC_RADIO_LWDEF:
			svp._lineWrapMethod = LINEWRAP_DEFAULT;
			::SendMessage(mainWindow(), NPPM_INTERNAL_LWDEF, 0, 0);
			return TRUE;

		case IDC_RADIO_LWALIGN:
			svp._lineWrapMethod = LINEWRAP_ALIGNED;
			::SendMessage(mainWindow(), NPPM_INTERNAL_LWALIGN, 0, 0);
			return TRUE;

		case IDC_RADIO_LWINDENT:
			svp._lineWrapMethod = LINEWRAP_INDENT;
			::SendMessage(mainWindow(), NPPM_INTERNAL_LWINDENT, 0, 0);
			return TRUE;

		default :
			switch (HIWORD(wParam))
			{
				case CBN_SELCHANGE : // == case LBN_SELCHANGE :
				{
					if (LOWORD(wParam) == IDC_WIDTH_COMBO)
					{
						nppGUI._caretWidth = static_cast<int>(combo(IDC_WIDTH_COMBO)->currentIndex());
						::SendMessage(mainWindow(), NPPM_INTERNAL_SETCARETWIDTH, 0, 0);
						return TRUE;
					}
				}
				break;
			}
	}
	return FALSE;
}


intptr_t Editing2SubDlg::onInit()
{
	NppParameters& nppParam = NppParameters::getInstance();
	ScintillaViewParams& svp = const_cast<ScintillaViewParams&>(nppParam.getSVP());

	// default =>  (svp._eolMode == svp.roundedRectangleText)
	bool checkDefaultCRLF = true;
	bool checkPlainTextCRLF = false;
	bool checkWithColorCRLF = false;

	if (svp._eolMode == svp.plainText)
	{
		checkDefaultCRLF = false;
		checkPlainTextCRLF = true;
		checkWithColorCRLF = false;
	}
	else if (svp._eolMode == svp.plainTextCustomColor)
	{
		checkDefaultCRLF = false;
		checkPlainTextCRLF = true;
		checkWithColorCRLF = true;
	}
	else if (svp._eolMode == svp.roundedRectangleTextCustomColor)
	{
		checkDefaultCRLF = true;
		checkPlainTextCRLF = false;
		checkWithColorCRLF = true;
	}
	setChecked(IDC_RADIO_ROUNDCORNER_CRLF, checkDefaultCRLF);
	setChecked(IDC_RADIO_PLAINTEXT_CRLF, checkPlainTextCRLF);
	setChecked(IDC_CHECK_WITHCUSTOMCOLOR_CRLF, checkWithColorCRLF);

	
	setChecked(IDC_CHECK_MULTISELECTION, svp._multiSelection);
	setChecked(IDC_CHECK_COLUMN2MULTIEDITING, svp._columnSel2MultiEdit);
	enableItem(IDC_CHECK_COLUMN2MULTIEDITING, svp._multiSelection);

	NativeLangSpeaker* pNativeSpeaker = nppParam.getNativeLangSpeaker();
	wstring tip2show = pNativeSpeaker->getLocalizedStrFromID("eol-custom-color-tip", L"Go to Style Configurator to change the default EOL custom color (\"EOL custom color\").");

	setTip(IDC_BUTTON_LAUNCHSTYLECONF_CRLF, tip2show);

	const bool isNpcModeAbbrv = svp._npcMode == svp.abbreviation;
	setChecked(IDC_RADIO_NPC_ABBREVIATION, isNpcModeAbbrv);
	setChecked(IDC_RADIO_NPC_CODEPOINT, !isNpcModeAbbrv);

	setChecked(IDC_CHECK_NPC_COLOR, svp._npcCustomColor);
	setChecked(IDC_CHECK_NPC_INCLUDECCUNIEOL, svp._npcIncludeCcUniEol);
	setChecked(IDC_CHECK_NPC_NOINPUTC0, svp._npcNoInputC0);

	wstring tipNote2Show = pNativeSpeaker->getLocalizedStrFromID("npcNote-tip",
		L"Representation of selected \"non-ASCII\" whitespace and non-printing (control) characters.\n\n"\
		L"NOTE:\n"\
		L"Using representation will disable character effects on text.\n\n"\
		L"For the full list of selected whitespace and non-printing characters check User Manual.\n\n"\
		L"Click on this button to open website with User Manual.");

	wstring tipAb2Show = pNativeSpeaker->getLocalizedStrFromID("npcAbbreviation-tip",
		L"Abbreviation : name\n"\
		L"NBSP : no-break space\n"\
		L"ZWSP : zero-width space\n"\
		L"ZWNBSP : zero-width no-break space\n\n"\
		L"For the full list check User Manual.\n"\
		L"Click on \"?\" button on right to open website with User Manual.");

	wstring tipCp2Show = pNativeSpeaker->getLocalizedStrFromID("npcCodepoint-tip",
		L"Codepoint : name\n"\
		L"U+00A0 : no-break space\n"\
		L"U+200B : zero-width space\n"\
		L"U+FEFF : zero-width no-break space\n\n"\
		L"For the full list check User Manual.\n"\
		L"Click on \"?\" button on right to open website with User Manual.");

	wstring tipNpcCol2show = pNativeSpeaker->getLocalizedStrFromID("npcCustomColor-tip",
		L"Go to Style Configurator to change the default custom color for selected whitespace and non-printing characters (\"Non-printing characters custom color\").");

	wstring tipNpcInc2show = pNativeSpeaker->getLocalizedStrFromID("npcIncludeCcUniEol-tip",
		L"Apply non-printing characters appearance settings to C0, C1 control and Unicode EOL (next line, line separator and paragraph separator) characters.");

	setTip(IDC_BUTTON_NPC_NOTE, tipNote2Show);
	setTip(IDC_RADIO_NPC_ABBREVIATION, tipAb2Show);
	setTip(IDC_RADIO_NPC_CODEPOINT, tipCp2Show);
	setTip(IDC_BUTTON_NPC_LAUNCHSTYLECONF, tipNpcCol2show);
	setTip(IDC_CHECK_NPC_INCLUDECCUNIEOL, tipNpcInc2show);

	return TRUE;
}

intptr_t Editing2SubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppParameters& nppParam = NppParameters::getInstance();
	ScintillaViewParams& svp = const_cast<ScintillaViewParams&>(nppParam.getSVP());
	switch (wParam)
	{
		case IDC_CHECK_MULTISELECTION:
		{
			svp._multiSelection = isCheckedOrNot(IDC_CHECK_MULTISELECTION);
			if (!svp._multiSelection)
			{
				setChecked(IDC_CHECK_COLUMN2MULTIEDITING, FALSE);
				svp._columnSel2MultiEdit = false;
			}

			enableItem(IDC_CHECK_COLUMN2MULTIEDITING, svp._multiSelection);
			::SendMessage(mainWindow(), NPPM_INTERNAL_SETMULTISELECTION, 0, 0);
		}
		return TRUE;

		case IDC_CHECK_COLUMN2MULTIEDITING:
		{
			svp._columnSel2MultiEdit = isCheckedOrNot(IDC_CHECK_COLUMN2MULTIEDITING);
		}
		return TRUE;

		case IDC_RADIO_ROUNDCORNER_CRLF:
		case IDC_RADIO_PLAINTEXT_CRLF:
		case IDC_CHECK_WITHCUSTOMCOLOR_CRLF:
		{
			bool doCustomColor = isCheckedOrNot(IDC_CHECK_WITHCUSTOMCOLOR_CRLF);

			if (wParam == IDC_RADIO_ROUNDCORNER_CRLF)
			{
				svp._eolMode = doCustomColor ? svp.roundedRectangleTextCustomColor : svp.roundedRectangleText;
			}
			else if (wParam == IDC_RADIO_PLAINTEXT_CRLF)
			{
				svp._eolMode = doCustomColor ? svp.plainTextCustomColor : svp.plainText;
			}
			else // IDC_CHECK_WITHCUSTOMCOLOR_CRLF
			{
				if (isCheckedOrNot(IDC_RADIO_ROUNDCORNER_CRLF))
				{
					svp._eolMode = doCustomColor ? svp.roundedRectangleTextCustomColor : svp.roundedRectangleText;
				}
				else // IDC_RADIO_PLAINTEXT_CRLF
				{
					svp._eolMode = doCustomColor ? svp.plainTextCustomColor : svp.plainText;
				}
			}

			HWND grandParent = mainWindow();
			::SendMessage(grandParent, NPPM_INTERNAL_CRLFFORMCHANGED, 0, 0);
			return TRUE;
		}

		case IDC_BUTTON_LAUNCHSTYLECONF_CRLF:
		{
			HWND grandParent = mainWindow();
			::SendMessage(grandParent, NPPM_INTERNAL_CRLFLAUNCHSTYLECONF, 0, 0);
			return TRUE;
		}

		case IDC_RADIO_NPC_ABBREVIATION:
		case IDC_RADIO_NPC_CODEPOINT:
		{
			if (wParam == IDC_RADIO_NPC_CODEPOINT)
			{
				svp._npcMode = svp.codepoint;
			}
			else // if (wParam == IDC_RADIO_NONPRINT_ABBREVIATION)
			{
				svp._npcMode = svp.abbreviation;
			}

			HWND grandParent = mainWindow();
			::SendMessage(grandParent, NPPM_INTERNAL_SETNPC, 0, 0);
			return TRUE;
		}

		case IDC_BUTTON_NPC_NOTE:
		{
			QDesktopServices::openUrl(QUrl(QStringLiteral("https://npp-user-manual.org/docs/views/#show-symbol")));
			return TRUE;
		}

		case IDC_CHECK_NPC_COLOR:
		{
			svp._npcCustomColor = isCheckedOrNot(IDC_CHECK_NPC_COLOR);
			HWND grandParent = mainWindow();
			::SendMessage(grandParent, NPPM_INTERNAL_NPCFORMCHANGED, 0, 0);
			return TRUE;
		}

		case IDC_BUTTON_NPC_LAUNCHSTYLECONF:
		{
			HWND grandParent = mainWindow();
			::SendMessage(grandParent, NPPM_INTERNAL_NPCLAUNCHSTYLECONF, 0, 0);
			return TRUE;
		}

		case IDC_CHECK_NPC_INCLUDECCUNIEOL:
		{
			svp._npcIncludeCcUniEol = isCheckedOrNot(IDC_CHECK_NPC_INCLUDECCUNIEOL);

			const HWND grandParent = mainWindow();
			::SendMessage(grandParent, NPPM_INTERNAL_SETNPC, IDC_CHECK_NPC_INCLUDECCUNIEOL, 0);
			return TRUE;
		}

		case IDC_CHECK_NPC_NOINPUTC0:
		{
			svp._npcNoInputC0 = isCheckedOrNot(IDC_CHECK_NPC_NOINPUTC0);
			return TRUE;
		}
	}
	return TRUE;
}



void DarkModeSubDlg::enableCustomizedColorCtrls(bool doEnable)
{
	_pBackgroundColorPicker->getHSelf()->setEnabled(doEnable);
	_pCtrlBackgroundColorPicker->getHSelf()->setEnabled(doEnable);
	_pHotBackgroundColorPicker->getHSelf()->setEnabled(doEnable);
	_pDlgBackgroundColorPicker->getHSelf()->setEnabled(doEnable);
	_pErrorBackgroundColorPicker->getHSelf()->setEnabled(doEnable);
	_pTextColorPicker->getHSelf()->setEnabled(doEnable);
	_pDarkerTextColorPicker->getHSelf()->setEnabled(doEnable);
	_pDisabledTextColorPicker->getHSelf()->setEnabled(doEnable);
	_pEdgeColorPicker->getHSelf()->setEnabled(doEnable);
	_pLinkColorPicker->getHSelf()->setEnabled(doEnable);
	_pHotEdgeColorPicker->getHSelf()->setEnabled(doEnable);
	_pDisabledEdgeColorPicker->getHSelf()->setEnabled(doEnable);

	enableItem(IDD_CUSTOMIZED_RESET_BUTTON, doEnable);

	if (doEnable)
	{
		_pBackgroundColorPicker->setColour(NppDarkMode::getBackgroundColor());
		_pCtrlBackgroundColorPicker->setColour(NppDarkMode::getCtrlBackgroundColor());
		_pHotBackgroundColorPicker->setColour(NppDarkMode::getHotBackgroundColor());
		_pDlgBackgroundColorPicker->setColour(NppDarkMode::getDlgBackgroundColor());
		_pErrorBackgroundColorPicker->setColour(NppDarkMode::getErrorBackgroundColor());
		_pTextColorPicker->setColour(NppDarkMode::getTextColor());
		_pDarkerTextColorPicker->setColour(NppDarkMode::getDarkerTextColor());
		_pDisabledTextColorPicker->setColour(NppDarkMode::getDisabledTextColor());
		_pEdgeColorPicker->setColour(NppDarkMode::getEdgeColor());
		_pLinkColorPicker->setColour(NppDarkMode::getLinkTextColor());
		_pHotEdgeColorPicker->setColour(NppDarkMode::getHotEdgeColor());
		_pDisabledEdgeColorPicker->setColour(NppDarkMode::getDisabledEdgeColor());
	}
}

intptr_t DarkModeSubDlg::onInit()
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = nppParam.getNppGUI();
	enableItem(IDC_RADIO_DARKMODE_FOLLOWWINDOWS, true);
	const int topControlID = NppDarkMode::isWindowsModeEnabled() && true ? IDC_RADIO_DARKMODE_FOLLOWWINDOWS : NppDarkMode::isEnabled() ? IDC_RADIO_DARKMODE_DARKMODE : IDC_RADIO_DARKMODE_LIGHTMODE;
	setChecked(topControlID, true);

	int id = IDC_RADIO_DARKMODE_BLACK;
	switch (nppGUI._darkmode._colorTone)
	{
		case NppDarkMode::redTone:
			id = IDC_RADIO_DARKMODE_RED;
			break;
		case NppDarkMode::greenTone:
			id = IDC_RADIO_DARKMODE_GREEN;
			break;
		case NppDarkMode::blueTone:
			id = IDC_RADIO_DARKMODE_BLUE;
			break;
		case NppDarkMode::purpleTone:
			id = IDC_RADIO_DARKMODE_PURPLE;
			break;
		case NppDarkMode::cyanTone:
			id = IDC_RADIO_DARKMODE_CYAN;
			break;
		case NppDarkMode::oliveTone:
			id = IDC_RADIO_DARKMODE_OLIVE;
			break;
		case NppDarkMode::customizedTone:
			id = IDC_RADIO_DARKMODE_CUSTOMIZED;
			break;

		case NppDarkMode::blackTone:
		default:
			break;
	}
	setChecked(id, TRUE);

	_pBackgroundColorPicker = std::make_unique<ColourPicker>();
	_pCtrlBackgroundColorPicker = std::make_unique<ColourPicker>();
	_pHotBackgroundColorPicker = std::make_unique<ColourPicker>();
	_pDlgBackgroundColorPicker = std::make_unique<ColourPicker>();
	_pErrorBackgroundColorPicker = std::make_unique<ColourPicker>();
	_pTextColorPicker = std::make_unique<ColourPicker>();
	_pDarkerTextColorPicker = std::make_unique<ColourPicker>();
	_pDisabledTextColorPicker = std::make_unique<ColourPicker>();
	_pEdgeColorPicker = std::make_unique<ColourPicker>();
	_pLinkColorPicker = std::make_unique<ColourPicker>();
	_pHotEdgeColorPicker = std::make_unique<ColourPicker>();
	_pDisabledEdgeColorPicker = std::make_unique<ColourPicker>();

	_pBackgroundColorPicker->init(_hInst, _hSelf);
	_pCtrlBackgroundColorPicker->init(_hInst, _hSelf);
	_pHotBackgroundColorPicker->init(_hInst, _hSelf);
	_pDlgBackgroundColorPicker->init(_hInst, _hSelf);
	_pErrorBackgroundColorPicker->init(_hInst, _hSelf);
	_pTextColorPicker->init(_hInst, _hSelf);
	_pDarkerTextColorPicker->init(_hInst, _hSelf);
	_pDisabledTextColorPicker->init(_hInst, _hSelf);
	_pEdgeColorPicker->init(_hInst, _hSelf);
	_pLinkColorPicker->init(_hInst, _hSelf);
	_pHotEdgeColorPicker->init(_hInst, _hSelf);
	_pDisabledEdgeColorPicker->init(_hInst, _hSelf);

	_pBackgroundColorPicker->disableRightClick();
	_pCtrlBackgroundColorPicker->disableRightClick();
	_pHotBackgroundColorPicker->disableRightClick();
	_pDlgBackgroundColorPicker->disableRightClick();
	_pErrorBackgroundColorPicker->disableRightClick();
	_pTextColorPicker->disableRightClick();
	_pDarkerTextColorPicker->disableRightClick();
	_pDisabledTextColorPicker->disableRightClick();
	_pEdgeColorPicker->disableRightClick();
	_pLinkColorPicker->disableRightClick();
	_pHotEdgeColorPicker->disableRightClick();
	_pDisabledEdgeColorPicker->disableRightClick();

	placePicker(IDD_CUSTOMIZED_COLOR1_STATIC, *_pBackgroundColorPicker);
	placePicker(IDD_CUSTOMIZED_COLOR2_STATIC, *_pHotBackgroundColorPicker);
	placePicker(IDD_CUSTOMIZED_COLOR3_STATIC, *_pCtrlBackgroundColorPicker);
	placePicker(IDD_CUSTOMIZED_COLOR4_STATIC, *_pDlgBackgroundColorPicker);
	placePicker(IDD_CUSTOMIZED_COLOR5_STATIC, *_pErrorBackgroundColorPicker);
	placePicker(IDD_CUSTOMIZED_COLOR6_STATIC, *_pTextColorPicker);
	placePicker(IDD_CUSTOMIZED_COLOR7_STATIC, *_pDarkerTextColorPicker);
	placePicker(IDD_CUSTOMIZED_COLOR8_STATIC, *_pDisabledTextColorPicker);
	placePicker(IDD_CUSTOMIZED_COLOR9_STATIC, *_pEdgeColorPicker);
	placePicker(IDD_CUSTOMIZED_COLOR10_STATIC, *_pLinkColorPicker);
	placePicker(IDD_CUSTOMIZED_COLOR11_STATIC, *_pHotEdgeColorPicker);
	placePicker(IDD_CUSTOMIZED_COLOR12_STATIC, *_pDisabledEdgeColorPicker);

	_pBackgroundColorPicker->display();
	_pCtrlBackgroundColorPicker->display();
	_pHotBackgroundColorPicker->display();
	_pDlgBackgroundColorPicker->display();
	_pErrorBackgroundColorPicker->display();
	_pTextColorPicker->display();
	_pDarkerTextColorPicker->display();
	_pDisabledTextColorPicker->display();
	_pEdgeColorPicker->display();
	_pLinkColorPicker->display();
	_pHotEdgeColorPicker->display();
	_pDisabledEdgeColorPicker->display();

	enableItem(IDC_RADIO_DARKMODE_BLACK, nppGUI._darkmode._isEnabled);
	enableItem(IDC_RADIO_DARKMODE_RED, nppGUI._darkmode._isEnabled);
	enableItem(IDC_RADIO_DARKMODE_GREEN, nppGUI._darkmode._isEnabled);
	enableItem(IDC_RADIO_DARKMODE_BLUE, nppGUI._darkmode._isEnabled);
	enableItem(IDC_RADIO_DARKMODE_PURPLE, nppGUI._darkmode._isEnabled);
	enableItem(IDC_RADIO_DARKMODE_CYAN, nppGUI._darkmode._isEnabled);
	enableItem(IDC_RADIO_DARKMODE_OLIVE, nppGUI._darkmode._isEnabled);
	enableItem(IDC_RADIO_DARKMODE_CUSTOMIZED, nppGUI._darkmode._isEnabled);

	enableCustomizedColorCtrls(nppGUI._darkmode._isEnabled && id == IDC_RADIO_DARKMODE_CUSTOMIZED);

	return TRUE;
}


DarkModeSubDlg::DarkModeSubDlg() = default;
DarkModeSubDlg::~DarkModeSubDlg() = default;

void DarkModeSubDlg::showResetMenu()
{
	if (!_resetPopupMenu)
	{
		NativeLangSpeaker* pNativeSpeaker = NppParameters::getInstance().getNativeLangSpeaker();
		std::vector<const char*> nodeNames{ "Dialog", "Preference", "DarkMode" };

		_resetPopupMenu = std::make_unique<QMenu>();
		const auto addItem = [&](int cmdID, const wstring& text)
		{
			QAction* action = _resetPopupMenu->addAction(QString::fromStdWString(text));
			QObject::connect(action, &QAction::triggered, _hSelf, [this, cmdID]() { onCommand(cmdID, 0); });
		};
		addItem(IDD_CUSTOMIZED_RESET_BUTTON, pNativeSpeaker->getCmdLangStr(nodeNames, IDC_RADIO_DARKMODE_BLACK ,L"Black"));
		addItem(IDD_DROPDOWN_RESET_RED, pNativeSpeaker->getCmdLangStr(nodeNames, IDC_RADIO_DARKMODE_RED, L"Red"));
		addItem(IDD_DROPDOWN_RESET_GREEN, pNativeSpeaker->getCmdLangStr(nodeNames, IDC_RADIO_DARKMODE_GREEN, L"Green"));
		addItem(IDD_DROPDOWN_RESET_BLUE, pNativeSpeaker->getCmdLangStr(nodeNames, IDC_RADIO_DARKMODE_BLUE, L"Blue"));
		addItem(IDD_DROPDOWN_RESET_PURPLE, pNativeSpeaker->getCmdLangStr(nodeNames, IDC_RADIO_DARKMODE_PURPLE, L"Purple"));
		addItem(IDD_DROPDOWN_RESET_CYAN, pNativeSpeaker->getCmdLangStr(nodeNames, IDC_RADIO_DARKMODE_CYAN, L"Cyan"));
		addItem(IDD_DROPDOWN_RESET_OLIVE, pNativeSpeaker->getCmdLangStr(nodeNames, IDC_RADIO_DARKMODE_OLIVE, L"Olive"));

	}

	QWidget* resetButton = dlgItem(IDD_CUSTOMIZED_RESET_BUTTON);
	_resetPopupMenu->exec(resetButton->mapToGlobal(resetButton->rect().bottomLeft()));
}

intptr_t DarkModeSubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = nppParam.getNppGUI();
	bool changed = false;
	bool forceRefresh = false;
	bool doEnableCustomizedColorCtrls = false;
	switch (wParam)
	{
		case IDC_RADIO_DARKMODE_FOLLOWWINDOWS:
		{
			// The next refresh reads the desktop's preference
		}
		[[fallthrough]];
		case IDC_RADIO_DARKMODE_LIGHTMODE:
		case IDC_RADIO_DARKMODE_DARKMODE:
		{
			const bool isFollowWindows = isCheckedOrNot(IDC_RADIO_DARKMODE_FOLLOWWINDOWS);
			NppDarkMode::setWindowsMode(isFollowWindows);

			const bool enableDarkMode = isCheckedOrNot(IDC_RADIO_DARKMODE_DARKMODE) || (isFollowWindows && NppDarkMode::desktopPrefersDark());
			nppGUI._darkmode._isEnabled = enableDarkMode;

			enableItem(IDC_RADIO_DARKMODE_BLACK, enableDarkMode);
			enableItem(IDC_RADIO_DARKMODE_RED, enableDarkMode);
			enableItem(IDC_RADIO_DARKMODE_GREEN, enableDarkMode);
			enableItem(IDC_RADIO_DARKMODE_BLUE, enableDarkMode);
			enableItem(IDC_RADIO_DARKMODE_PURPLE, enableDarkMode);
			enableItem(IDC_RADIO_DARKMODE_CYAN, enableDarkMode);
			enableItem(IDC_RADIO_DARKMODE_OLIVE, enableDarkMode);
			enableItem(IDC_RADIO_DARKMODE_CUSTOMIZED, enableDarkMode);

			doEnableCustomizedColorCtrls = enableDarkMode && nppGUI._darkmode._colorTone == NppDarkMode::customizedTone;
			enableCustomizedColorCtrls(doEnableCustomizedColorCtrls);

			::SendMessage(_hParent, NPPM_INTERNAL_SETTOOLICONSSET, static_cast<WPARAM>(enableDarkMode), 0); // Set icons set only option (checkbox) on general sub-dialog, the remaining real operations will be done in NppDarkMode::refreshDarkMode

			changed = true;
		}
		break;

		case IDC_RADIO_DARKMODE_BLACK:
		case IDC_RADIO_DARKMODE_RED:
		case IDC_RADIO_DARKMODE_GREEN:
		case IDC_RADIO_DARKMODE_BLUE:
		case IDC_RADIO_DARKMODE_PURPLE:
		case IDC_RADIO_DARKMODE_CYAN:
		case IDC_RADIO_DARKMODE_OLIVE:
		case IDC_RADIO_DARKMODE_CUSTOMIZED:
		case IDD_CUSTOMIZED_RESET_BUTTON:
		case IDD_DROPDOWN_RESET_RED:
		case IDD_DROPDOWN_RESET_GREEN:
		case IDD_DROPDOWN_RESET_BLUE:
		case IDD_DROPDOWN_RESET_PURPLE:
		case IDD_DROPDOWN_RESET_CYAN:
		case IDD_DROPDOWN_RESET_OLIVE:
		{
			if (wParam == IDC_RADIO_DARKMODE_BLACK)
			{
				if (nppGUI._darkmode._colorTone == NppDarkMode::blackTone)
					return TRUE;
				nppGUI._darkmode._colorTone = NppDarkMode::blackTone;
			}
			else if (wParam == IDC_RADIO_DARKMODE_RED)
			{
				if (nppGUI._darkmode._colorTone == NppDarkMode::redTone)
					return TRUE;
				nppGUI._darkmode._colorTone = NppDarkMode::redTone;
			}
			else if (wParam == IDC_RADIO_DARKMODE_GREEN)
			{
				if (nppGUI._darkmode._colorTone == NppDarkMode::greenTone)
					return TRUE;
				nppGUI._darkmode._colorTone = NppDarkMode::greenTone;
			}
			else if (wParam == IDC_RADIO_DARKMODE_BLUE)
			{
				if (nppGUI._darkmode._colorTone == NppDarkMode::blueTone)
					return TRUE;
				nppGUI._darkmode._colorTone = NppDarkMode::blueTone;
			}
			else if (wParam == IDC_RADIO_DARKMODE_PURPLE)
			{
				if (nppGUI._darkmode._colorTone == NppDarkMode::purpleTone)
					return TRUE;
				nppGUI._darkmode._colorTone = NppDarkMode::purpleTone;
			}
			else if (wParam == IDC_RADIO_DARKMODE_CYAN)
			{
				if (nppGUI._darkmode._colorTone == NppDarkMode::cyanTone)
					return TRUE;
				nppGUI._darkmode._colorTone = NppDarkMode::cyanTone;
			}
			else if (wParam == IDC_RADIO_DARKMODE_OLIVE)
			{
				if (nppGUI._darkmode._colorTone == NppDarkMode::oliveTone)
					return TRUE;
				nppGUI._darkmode._colorTone = NppDarkMode::oliveTone;
			}
			else if (wParam == IDC_RADIO_DARKMODE_CUSTOMIZED)
			{
				if (nppGUI._darkmode._colorTone == NppDarkMode::customizedTone)
					return TRUE;
				nppGUI._darkmode._colorTone = NppDarkMode::customizedTone;
				doEnableCustomizedColorCtrls = true;
			}

			else if (wParam == IDD_CUSTOMIZED_RESET_BUTTON)
			{
				nppGUI._darkmode._customColors = NppDarkMode::getDarkModeDefaultColors();
				NppDarkMode::changeCustomTheme(nppGUI._darkmode._customColors);
				doEnableCustomizedColorCtrls = true;
			}
			else
			{
				doEnableCustomizedColorCtrls = true;
				switch (wParam)
				{
					case IDD_CUSTOMIZED_RESET_BUTTON:
					{
						nppGUI._darkmode._customColors = NppDarkMode::getDarkModeDefaultColors();
						break;
					}

					case IDD_DROPDOWN_RESET_RED:
					{
						nppGUI._darkmode._customColors = NppDarkMode::getDarkModeDefaultColors(NppDarkMode::ColorTone::redTone);
						break;
					}

					case IDD_DROPDOWN_RESET_GREEN:
					{
						nppGUI._darkmode._customColors = NppDarkMode::getDarkModeDefaultColors(NppDarkMode::ColorTone::greenTone);
						break;
					}

					case IDD_DROPDOWN_RESET_BLUE:
					{
						nppGUI._darkmode._customColors = NppDarkMode::getDarkModeDefaultColors(NppDarkMode::ColorTone::blueTone);
						break;
					}

					case IDD_DROPDOWN_RESET_PURPLE:
					{
						nppGUI._darkmode._customColors = NppDarkMode::getDarkModeDefaultColors(NppDarkMode::ColorTone::purpleTone);
						break;
					}

					case IDD_DROPDOWN_RESET_CYAN:
					{
						nppGUI._darkmode._customColors = NppDarkMode::getDarkModeDefaultColors(NppDarkMode::ColorTone::cyanTone);
						break;
					}

					case IDD_DROPDOWN_RESET_OLIVE:
					{
						nppGUI._darkmode._customColors = NppDarkMode::getDarkModeDefaultColors(NppDarkMode::ColorTone::oliveTone);
						break;
					}

					default:
						doEnableCustomizedColorCtrls = false;
						break;
				}

				if (doEnableCustomizedColorCtrls)
					NppDarkMode::changeCustomTheme(nppGUI._darkmode._customColors);
			}

			// switch to chosen dark mode
			nppGUI._darkmode._isEnabled = true;
			NppDarkMode::setDarkTone(nppGUI._darkmode._colorTone);
			changed = true;
			forceRefresh = true;

			enableCustomizedColorCtrls(doEnableCustomizedColorCtrls);
		}
		break;

		default:
			switch (HIWORD(wParam))
			{
				case CPN_COLOURPICKED:
				{
					COLORREF c = 0;
					if (reinterpret_cast<HWND>(lParam) == _pBackgroundColorPicker->getHSelf())
					{
						c = _pBackgroundColorPicker->getColour();
						NppDarkMode::setBackgroundColor(c);
						nppGUI._darkmode._customColors.background = c;
					}
					else if (reinterpret_cast<HWND>(lParam) == _pCtrlBackgroundColorPicker->getHSelf())
					{
						c = _pCtrlBackgroundColorPicker->getColour();
						NppDarkMode::setCtrlBackgroundColor(c);
						nppGUI._darkmode._customColors.softerBackground = c;
					}
					else if (reinterpret_cast<HWND>(lParam) == _pHotBackgroundColorPicker->getHSelf())
					{
						c = _pHotBackgroundColorPicker->getColour();
						NppDarkMode::setHotBackgroundColor(c);
						nppGUI._darkmode._customColors.hotBackground = c;
					}
					else if (reinterpret_cast<HWND>(lParam) == _pDlgBackgroundColorPicker->getHSelf())
					{
						c = _pDlgBackgroundColorPicker->getColour();
						NppDarkMode::setDlgBackgroundColor(c);
						nppGUI._darkmode._customColors.pureBackground = c;
					}
					else if (reinterpret_cast<HWND>(lParam) == _pErrorBackgroundColorPicker->getHSelf())
					{
						c = _pErrorBackgroundColorPicker->getColour();
						NppDarkMode::setErrorBackgroundColor(c);
						nppGUI._darkmode._customColors.errorBackground = c;
					}
					else if (reinterpret_cast<HWND>(lParam) == _pTextColorPicker->getHSelf())
					{
						c = _pTextColorPicker->getColour();
						NppDarkMode::setTextColor(c);
						nppGUI._darkmode._customColors.text = c;
					}
					else if (reinterpret_cast<HWND>(lParam) == _pDarkerTextColorPicker->getHSelf())
					{
						c = _pDarkerTextColorPicker->getColour();
						NppDarkMode::setDarkerTextColor(c);
						nppGUI._darkmode._customColors.darkerText = c;
					}
					else if (reinterpret_cast<HWND>(lParam) == _pDisabledTextColorPicker->getHSelf())
					{
						c = _pDisabledTextColorPicker->getColour();
						NppDarkMode::setDisabledTextColor(c);
						nppGUI._darkmode._customColors.disabledText = c;
					}
					else if (reinterpret_cast<HWND>(lParam) == _pEdgeColorPicker->getHSelf())
					{
						c = _pEdgeColorPicker->getColour();
						NppDarkMode::setEdgeColor(c);
						nppGUI._darkmode._customColors.edge = c;
					}
					else if (reinterpret_cast<HWND>(lParam) == _pLinkColorPicker->getHSelf())
					{
						c = _pLinkColorPicker->getColour();
						NppDarkMode::setLinkTextColor(c);
						nppGUI._darkmode._customColors.linkText = c;
					}
					else if (reinterpret_cast<HWND>(lParam) == _pHotEdgeColorPicker->getHSelf())
					{
						c = _pHotEdgeColorPicker->getColour();
						NppDarkMode::setHotEdgeColor(c);
						nppGUI._darkmode._customColors.hotEdge = c;
					}
					else if (reinterpret_cast<HWND>(lParam) == _pDisabledEdgeColorPicker->getHSelf())
					{
						c = _pDisabledEdgeColorPicker->getColour();
						NppDarkMode::setDisabledEdgeColor(c);
						nppGUI._darkmode._customColors.disabledEdge = c;
					}
					else
					{
						return FALSE;
					}

					nppGUI._darkmode._isEnabled = true;
					NppDarkMode::setDarkTone(nppGUI._darkmode._colorTone);
					changed = true;
					forceRefresh = true;
					doEnableCustomizedColorCtrls = true;
				}
				break;

				default:
				{
					return FALSE;
				}
			}
	}

	if (changed)
	{
		if (!doEnableCustomizedColorCtrls)
		{
			COLORREF disabledColor = nppGUI._darkmode._isEnabled ? NppDarkMode::getDlgBackgroundColor() : windowFaceColour(_hSelf);

			_pBackgroundColorPicker->setColour(disabledColor);
			_pCtrlBackgroundColorPicker->setColour(disabledColor);
			_pHotBackgroundColorPicker->setColour(disabledColor);
			_pDlgBackgroundColorPicker->setColour(disabledColor);
			_pErrorBackgroundColorPicker->setColour(disabledColor);
			_pTextColorPicker->setColour(disabledColor);
			_pDarkerTextColorPicker->setColour(disabledColor);
			_pDisabledTextColorPicker->setColour(disabledColor);
			_pEdgeColorPicker->setColour(disabledColor);
			_pLinkColorPicker->setColour(disabledColor);
			_pHotEdgeColorPicker->setColour(disabledColor);
			_pDisabledEdgeColorPicker->setColour(disabledColor);
		}

		NppDarkMode::refreshDarkMode(_hSelf, forceRefresh);
		grabFocus(); // to make black mode title bar appear
		return TRUE;
	}

	return FALSE;
}


void MarginsBorderEdgeSubDlg::initScintParam()
{
	NppParameters& nppParam = NppParameters::getInstance();
	ScintillaViewParams & svp = const_cast<ScintillaViewParams &>(nppParam.getSVP());
	
	setChecked(IDC_RADIO_BOX, FALSE);
	setChecked(IDC_RADIO_CIRCLE, FALSE);
	setChecked(IDC_RADIO_ARROW, FALSE);
	setChecked(IDC_RADIO_SIMPLE, FALSE);
	setChecked(IDC_RADIO_FOLDMARGENONE, FALSE);

	int id = 0;
	switch (svp._folderStyle)
	{
		case FOLDER_STYLE_NONE:
			id = IDC_RADIO_FOLDMARGENONE;
			break;
		case FOLDER_STYLE_BOX:
			id = IDC_RADIO_BOX;
			break;
		case FOLDER_STYLE_CIRCLE:
			id = IDC_RADIO_CIRCLE;
			break;
		case FOLDER_STYLE_ARROW:
			id = IDC_RADIO_ARROW;
			break;
		default : // FOLDER_STYLE_SIMPLE:
			id = IDC_RADIO_SIMPLE;
	}
	setChecked(id, TRUE);
	
	setChecked(IDC_CHECK_LINENUMBERMARGE, svp._lineNumberMarginShow);
	setChecked(IDC_RADIO_DYNAMIC, svp._lineNumberMarginDynamicWidth);
	setChecked(IDC_RADIO_CONSTANT, !svp._lineNumberMarginDynamicWidth);
	enableItem(IDC_RADIO_DYNAMIC, svp._lineNumberMarginShow);
	enableItem(IDC_RADIO_CONSTANT, svp._lineNumberMarginShow);

	setChecked(IDC_CHECK_BOOKMARKMARGE, svp._bookMarkMarginShow);
	setChecked(IDC_CHECK_CHANGHISTORYMARGIN, svp._isChangeHistoryMarginEnabled);
	setChecked(IDC_CHECK_CHANGHISTORYINDICATOR, svp._isChangeHistoryIndicatorEnabled);
	setChecked(IDC_CHECK_NOEDGE, !svp._showBorderEdge);
	
	bool canBeBg = svp._edgeMultiColumnPos.size() == 1;
	if (!canBeBg)
	{
		svp._isEdgeBgMode = false;
		setChecked(IDC_CHECK_EDGEBGMODE, FALSE);
		enableItem(IDC_CHECK_EDGEBGMODE, FALSE);
	}
	else
	{
		setChecked(IDC_CHECK_EDGEBGMODE, svp._isEdgeBgMode);
	}

	wstring edgeColumnPosStr;
	for (auto i : svp._edgeMultiColumnPos)
	{
		std::string s = std::to_string(i);
		edgeColumnPosStr += wstring(s.begin(), s.end());
		edgeColumnPosStr += L" ";
	}
	setItemText(IDC_COLUMNPOS_EDIT, edgeColumnPosStr.c_str());

	auto* columnEdit = qobject_cast<QLineEdit*>(dlgItem(IDC_COLUMNPOS_EDIT));
	columnEdit->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-9 ]*")), columnEdit));
}

intptr_t MarginsBorderEdgeSubDlg::onInit()
{
	NppParameters& nppParam = NppParameters::getInstance();

	const ScintillaViewParams & svp = nppParam.getSVP();
	slider(IDC_BORDERWIDTH_SLIDER)->setMinimum(BORDERWIDTH_SMALLEST);
	slider(IDC_BORDERWIDTH_SLIDER)->setMaximum(BORDERWIDTH_LARGEST);
	slider(IDC_BORDERWIDTH_SLIDER)->setPageStep(BORDERWIDTH_INTERVAL);
	setSliderPos(IDC_BORDERWIDTH_SLIDER, svp._borderWidth);
	setItemInt(IDC_BORDERWIDTHVAL_STATIC, svp._borderWidth);

	slider(IDC_PADDINGLEFT_SLIDER)->setMinimum(PADDING_SMALLEST);
	slider(IDC_PADDINGLEFT_SLIDER)->setMaximum(PADDING_LARGEST);
	slider(IDC_PADDINGLEFT_SLIDER)->setPageStep(PADDING_INTERVAL);
	setSliderPos(IDC_PADDINGLEFT_SLIDER, svp._paddingLeft);
	setItemInt(IDC_PADDINGLEFTVAL_STATIC, svp._paddingLeft);

	slider(IDC_PADDINGRIGHT_SLIDER)->setMinimum(PADDING_SMALLEST);
	slider(IDC_PADDINGRIGHT_SLIDER)->setMaximum(PADDING_LARGEST);
	slider(IDC_PADDINGRIGHT_SLIDER)->setPageStep(PADDING_INTERVAL);
	setSliderPos(IDC_PADDINGRIGHT_SLIDER, svp._paddingRight);
	setItemInt(IDC_PADDINGRIGHTVAL_STATIC, svp._paddingRight);

	slider(IDC_DISTRACTIONFREE_SLIDER)->setMinimum(DISTRACTIONFREE_SMALLEST);
	slider(IDC_DISTRACTIONFREE_SLIDER)->setMaximum(DISTRACTIONFREE_LARGEST);
	slider(IDC_DISTRACTIONFREE_SLIDER)->setPageStep(DISTRACTIONFREE_INTERVAL);
	setSliderPos(IDC_DISTRACTIONFREE_SLIDER, svp._distractionFreeDivPart);
	setItemInt(IDC_DISTRACTIONFREEVAL_STATIC, svp._distractionFreeDivPart);

	NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
	wstring tipNote2Show = pNativeSpeaker->getLocalizedStrFromID("verticalEdge-tip",	L"Add your column marker by indicating its position with a decimal number. You can define several column markers by using white space to separate the different numbers.");

	setTip(IDC_BUTTON_VES_TIP, tipNote2Show);

	initScintParam();


	return TRUE;
}

void MarginsBorderEdgeSubDlg::onScroll(HWND scale)
{
	NppParameters& nppParam = NppParameters::getInstance();
	auto& svp = const_cast<ScintillaViewParams&>(nppParam.getSVP());

	if (scale == dlgItem(IDC_BORDERWIDTH_SLIDER))
	{
		auto borderWidth = slider(IDC_BORDERWIDTH_SLIDER)->value();
		svp._borderWidth = static_cast<int>(borderWidth);
		setItemInt(IDC_BORDERWIDTHVAL_STATIC, static_cast<UINT>(borderWidth));
		::SendMessage(mainWindow(), WM_SIZE, 0, 0);
	}
	else if (scale == dlgItem(IDC_PADDINGLEFT_SLIDER))
	{
		svp._paddingLeft = static_cast<unsigned char>(slider(IDC_PADDINGLEFT_SLIDER)->value());
		setItemInt(IDC_PADDINGLEFTVAL_STATIC, static_cast<UINT>(svp._paddingLeft));
		::SendMessage(mainWindow(), NPPM_INTERNAL_UPDATETEXTZONEPADDING, 0, 0);
	}
	else if (scale == dlgItem(IDC_PADDINGRIGHT_SLIDER))
	{
		svp._paddingRight = static_cast<unsigned char>(slider(IDC_PADDINGRIGHT_SLIDER)->value());
		setItemInt(IDC_PADDINGRIGHTVAL_STATIC, static_cast<UINT>(svp._paddingRight));
		::SendMessage(mainWindow(), NPPM_INTERNAL_UPDATETEXTZONEPADDING, 0, 0);
	}
	else if (scale == dlgItem(IDC_DISTRACTIONFREE_SLIDER))
	{
		svp._distractionFreeDivPart = static_cast<unsigned char>(slider(IDC_DISTRACTIONFREE_SLIDER)->value());
		setItemInt(IDC_DISTRACTIONFREEVAL_STATIC, static_cast<UINT>(svp._distractionFreeDivPart));
		::SendMessage(mainWindow(), NPPM_INTERNAL_UPDATETEXTZONEPADDING, 0, 0);
	}
}

intptr_t MarginsBorderEdgeSubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppParameters& nppParam = NppParameters::getInstance();

	auto& svp = const_cast<ScintillaViewParams&>(nppParam.getSVP());
	static bool changeHistoryWarningHasBeenGiven = false;

	switch (wParam)
	{
		case IDC_CHECK_LINENUMBERMARGE:
			svp._lineNumberMarginShow = isCheckedOrNot(IDC_CHECK_LINENUMBERMARGE);
			enableItem(IDC_RADIO_DYNAMIC, svp._lineNumberMarginShow);
			enableItem(IDC_RADIO_CONSTANT, svp._lineNumberMarginShow);
			::SendMessage(mainWindow(), NPPM_INTERNAL_LINENUMBER, 0, 0);
			return TRUE;
		case IDC_RADIO_DYNAMIC:
			svp._lineNumberMarginDynamicWidth = isCheckedOrNot(IDC_RADIO_DYNAMIC);
			::SendMessage(mainWindow(), NPPM_INTERNAL_LINENUMBER, 0, 0);
			return TRUE;
		case IDC_RADIO_CONSTANT:
			svp._lineNumberMarginDynamicWidth = !isCheckedOrNot(IDC_RADIO_CONSTANT);
			::SendMessage(mainWindow(), NPPM_INTERNAL_LINENUMBER, 0, 0);
			return TRUE;
		
		case IDC_CHECK_BOOKMARKMARGE:
			svp._bookMarkMarginShow = isCheckedOrNot(IDC_CHECK_BOOKMARKMARGE);
			::SendMessage(mainWindow(), NPPM_INTERNAL_SYMBOLMARGIN, 0, 0);
			return TRUE;

		case IDC_CHECK_CHANGHISTORYMARGIN:
		{
			bool isMarginJustEnabled = isCheckedOrNot(IDC_CHECK_CHANGHISTORYMARGIN);
			bool isIndicatorAlreadyEnabled = isCheckedOrNot(IDC_CHECK_CHANGHISTORYINDICATOR);

			if (isMarginJustEnabled && !isIndicatorAlreadyEnabled) // In the case that both "in margin" & "in text" were disabled, but "in margin" is just enabled
			{
				if (!changeHistoryWarningHasBeenGiven)
				{
					NativeLangSpeaker* pNativeSpeaker = nppParam.getNativeLangSpeaker();
					pNativeSpeaker->messageBox("ChangeHistoryEnabledWarning",
						_hSelf,
						L"You have to restart Notepad++ to enable Change History.",
						L"Notepad++ needs to be relaunched",
						MB_OK | MB_APPLMODAL);
					
					changeHistoryWarningHasBeenGiven = true;
				}
				svp._isChangeHistoryMarginEnabled = true;
				svp._isChangeHistoryEnabled4NextSession = changeHistoryState::margin;
			}
			else // otherwise
			{
				svp._isChangeHistoryMarginEnabled = isMarginJustEnabled;
				svp._isChangeHistoryEnabled4NextSession = (!isMarginJustEnabled && !isIndicatorAlreadyEnabled) ? changeHistoryState::disable :
					(isMarginJustEnabled && isIndicatorAlreadyEnabled) ? changeHistoryState::marginIndicator :changeHistoryState::indicator;

				::SendMessage(mainWindow(), NPPM_INTERNAL_ENABLECHANGEHISTORY, 0, 0);
			}
			return TRUE;
		}

		case IDC_CHECK_CHANGHISTORYINDICATOR:
		{
			bool isIndicatorJustEnabled = isCheckedOrNot(IDC_CHECK_CHANGHISTORYINDICATOR);
			bool isMarginAlreadyEnabled = isCheckedOrNot(IDC_CHECK_CHANGHISTORYMARGIN);

			if (isIndicatorJustEnabled && !isMarginAlreadyEnabled) // In the case that both "in margin" & "in text" were disabled, but "in text" is just enabled
			{
				if (!changeHistoryWarningHasBeenGiven)
				{
					NativeLangSpeaker* pNativeSpeaker = nppParam.getNativeLangSpeaker();
					pNativeSpeaker->messageBox("ChangeHistoryEnabledWarning",
						_hSelf,
						L"You have to restart Notepad++ to enable Change History.",
						L"Notepad++ needs to be relaunched",
						MB_OK | MB_APPLMODAL);
					
					changeHistoryWarningHasBeenGiven = true;
				}
				svp._isChangeHistoryIndicatorEnabled = true;
				svp._isChangeHistoryEnabled4NextSession = changeHistoryState::indicator;
			}
			else
			{
				svp._isChangeHistoryIndicatorEnabled = isIndicatorJustEnabled;
				svp._isChangeHistoryEnabled4NextSession = (!isIndicatorJustEnabled && !isMarginAlreadyEnabled) ? changeHistoryState::disable :
					(isIndicatorJustEnabled && isMarginAlreadyEnabled) ? changeHistoryState::marginIndicator : changeHistoryState::margin;

				::SendMessage(mainWindow(), NPPM_INTERNAL_ENABLECHANGEHISTORY, 0, 0);
			}
			return TRUE;
		}

		case IDC_CHECK_NOEDGE:
			svp._showBorderEdge = !isCheckedOrNot(IDC_CHECK_NOEDGE);
			::SendMessage(mainWindow(), NPPM_SETEDITORBORDEREDGE, 0, svp._showBorderEdge ? TRUE : FALSE);
			return TRUE;

		case IDC_RADIO_SIMPLE:
			svp._folderStyle = FOLDER_STYLE_SIMPLE;
			::SendMessage(mainWindow(), NPPM_INTERNAL_FOLDSYMBOLSIMPLE, 0, 0);
			return TRUE;
		case IDC_RADIO_ARROW:
			svp._folderStyle = FOLDER_STYLE_ARROW;
			::SendMessage(mainWindow(), NPPM_INTERNAL_FOLDSYMBOLARROW, 0, 0);
			return TRUE;
		case IDC_RADIO_CIRCLE:
			svp._folderStyle = FOLDER_STYLE_CIRCLE;
			::SendMessage(mainWindow(), NPPM_INTERNAL_FOLDSYMBOLCIRCLE, 0, 0);
			return TRUE;
		case IDC_RADIO_BOX:
			svp._folderStyle = FOLDER_STYLE_BOX;
			::SendMessage(mainWindow(), NPPM_INTERNAL_FOLDSYMBOLBOX, 0, 0);
			return TRUE;
			
		case IDC_RADIO_FOLDMARGENONE:
			svp._folderStyle = FOLDER_STYLE_NONE;
			::SendMessage(mainWindow(), NPPM_INTERNAL_FOLDSYMBOLNONE, 0, 0);
			return TRUE;

		case IDC_CHECK_EDGEBGMODE:
			svp._isEdgeBgMode = isCheckedOrNot(IDC_CHECK_EDGEBGMODE);
			::SendMessage(mainWindow(), NPPM_INTERNAL_EDGEMULTISETSIZE, 0, 0);
			return TRUE;

		default :
			switch (HIWORD(wParam))
			{
				case EN_CHANGE :
				{
					if (LOWORD(wParam) == IDC_COLUMNPOS_EDIT)
					{
						const wstring text = getItemText(IDC_COLUMNPOS_EDIT);

						if (str2numberVector(text, svp._edgeMultiColumnPos))
						{
							bool canBeBg = svp._edgeMultiColumnPos.size() == 1;
							if (!canBeBg)
							{
								svp._isEdgeBgMode = false;
								setChecked(IDC_CHECK_EDGEBGMODE, FALSE);
							}
							enableItem(IDC_CHECK_EDGEBGMODE, canBeBg);
							::SendMessage(mainWindow(), NPPM_INTERNAL_EDGEMULTISETSIZE, 0, 0);
							return TRUE;
						}
					}
				}
				break;
			}
	}
	return FALSE;
}


const size_t fileUpdateChoiceEnable = 0;
const size_t fileUpdateChoiceEnable4All = 1;
const size_t fileUpdateChoiceDisable = 2;
intptr_t MiscSubDlg::onInit()
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI & nppGUI = nppParam.getNppGUI();

	addComboItem(IDC_COMBO_FILEUPDATECHOICE, L"Enable for current file(s)");
	addComboItem(IDC_COMBO_FILEUPDATECHOICE, L"Enable for all opened files");
	addComboItem(IDC_COMBO_FILEUPDATECHOICE, L"Disable");

	int selIndex = -1;
	
	if (nppGUI._fileAutoDetection & cdEnabled4AllFiles)
	{
		selIndex = fileUpdateChoiceEnable4All;
	}
	else if (nppGUI._fileAutoDetection & cdEnabled4CurrentFile)
	{				
		selIndex = fileUpdateChoiceEnable;
	}
	else //cdDisabled
	{
		enableItem(IDC_CHECK_UPDATESILENTLY, FALSE);
		enableItem(IDC_CHECK_UPDATEGOTOEOF, FALSE);
		
		selIndex = fileUpdateChoiceDisable;
	}
	
	combo(IDC_COMBO_FILEUPDATECHOICE)->setCurrentIndex(static_cast<int>(selIndex));

	bool bCheck = (nppGUI._fileAutoDetection & cdAutoUpdate) ? true : false;
	setChecked(IDC_CHECK_UPDATESILENTLY, bCheck);

	bCheck = (nppGUI._fileAutoDetection & cdGo2end) ? true : false;
	setChecked(IDC_CHECK_UPDATEGOTOEOF, bCheck);

	addComboItem(IDC_COMBO_SYSTRAY_ACTION_CHOICE, L"No action on");
	addComboItem(IDC_COMBO_SYSTRAY_ACTION_CHOICE, L"Minimize to");
	addComboItem(IDC_COMBO_SYSTRAY_ACTION_CHOICE, L"Close to");
	addComboItem(IDC_COMBO_SYSTRAY_ACTION_CHOICE, L"Minimize / Close to");

	if (nppGUI._isMinimizedToTray < 0 || nppGUI._isMinimizedToTray > sta_minimize_close)
		nppGUI._isMinimizedToTray = sta_none;

	combo(IDC_COMBO_SYSTRAY_ACTION_CHOICE)->setCurrentIndex(static_cast<int>(nppGUI._isMinimizedToTray));

	addComboItem(IDC_COMBO_SC_TECHNOLOGY_CHOICE, L"GDI (most compatible)");
	addComboItem(IDC_COMBO_SC_TECHNOLOGY_CHOICE, L"DirectWrite (default)");
	addComboItem(IDC_COMBO_SC_TECHNOLOGY_CHOICE, L"DirectWrite (retain frames)");
	addComboItem(IDC_COMBO_SC_TECHNOLOGY_CHOICE, L"DirectWrite (draw to GDI DC)");
	addComboItem(IDC_COMBO_SC_TECHNOLOGY_CHOICE, L"DirectWrite (DirectX 11)");

	if ((nppGUI._writeTechnologyEngine < 0) || (nppGUI._writeTechnologyEngine > directWriteTechnologyUnavailable))
		nppGUI._writeTechnologyEngine = directWriteTechnology;

	if (nppGUI._writeTechnologyEngine != directWriteTechnologyUnavailable)
	{
		combo(IDC_COMBO_SC_TECHNOLOGY_CHOICE)->setCurrentIndex(static_cast<int>(nppGUI._writeTechnologyEngine));
	}
	else
	{
		combo(IDC_COMBO_SC_TECHNOLOGY_CHOICE)->setCurrentIndex(static_cast<int>(defaultTechnology));
		enableItem(IDC_COMBO_SC_TECHNOLOGY_CHOICE, false);
	}

	NativeLangSpeaker* pNativeSpeaker = nppParam.getNativeLangSpeaker();
	wstring tipScintillaRenderingTechnology2Show = pNativeSpeaker->getLocalizedStrFromID("scintillaRenderingTechnology-tip",
		L"May improve rendering of special characters or resolve some graphics issues, restart Notepad++ to apply the changes.");
	setTip(IDC_COMBO_SC_TECHNOLOGY_CHOICE, tipScintillaRenderingTechnology2Show);

	// DirectWrite and GDI do not exist here, as Qt does the drawing
	showItem(IDC_COMBO_SC_TECHNOLOGY_CHOICE, false);
	showItem(IDC_SC_TECHNOLOGY_STATIC, false);

	setChecked(IDC_CHECK_DETECTENCODING, nppGUI._detectEncoding);
	setChecked(IDC_CHECK_SAVEALLCONFIRM, nppGUI._saveAllConfirm);
	setChecked(IDC_CHECK_ALOOWSIMLINKFAW, nppGUI._isFawSymlinkAllowed);

	addComboItem(IDC_COMBO_AUTOUPDATE, L"Disable");
	addComboItem(IDC_COMBO_AUTOUPDATE, L"Enable on Notepad++ startup");
	addComboItem(IDC_COMBO_AUTOUPDATE, L"Enable on Notepad++ exit");
	if ((nppGUI._autoUpdateOpt._doAutoUpdate < NppGUI::autoupdate_disabled) || (nppGUI._autoUpdateOpt._doAutoUpdate > NppGUI::autoupdate_on_exit))
		nppGUI._autoUpdateOpt._doAutoUpdate = NppGUI::autoupdate_on_startup;
	combo(IDC_COMBO_AUTOUPDATE)->setCurrentIndex(static_cast<int>(nppGUI._autoUpdateOpt._doAutoUpdate));
	// There is no updater on this platform
	showItem(IDC_AUTOUPDATE_STATIC, false);
	showItem(IDC_COMBO_AUTOUPDATE, false);

	setChecked(IDC_CHECK_ENABLEDOCPEEKER, nppGUI._isDocPeekOnTab);
	setChecked(IDC_CHECK_ENABLEDOCPEEKONMAP, nppGUI._isDocPeekOnMap);
	setChecked(IDC_CHECK_MUTE_SOUNDS, nppGUI._muteSounds);

	setItemText(IDC_EDIT_SESSIONFILEEXT, nppGUI._definedSessionExt.c_str());
	setItemText(IDC_EDIT_WORKSPACEFILEEXT, nppGUI._definedWorkspaceExt.c_str());
	
	setChecked(IDC_CHECK_ENABLEDOCSWITCHER, nppGUI._doTaskList);
	setChecked(IDC_CHECK_STYLEMRU, nppGUI._styleMRU);
	setChecked(IDC_CHECK_SHORTTITLE, nppGUI._shortTitlebar);

	return TRUE;
}

intptr_t MiscSubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI & nppGUI = nppParam.getNppGUI();

	if (HIWORD(wParam) == EN_CHANGE)
	{
		switch (LOWORD(wParam))
		{
			case  IDC_EDIT_SESSIONFILEEXT:
			{
				const wstring sessionExt = getItemText(IDC_EDIT_SESSIONFILEEXT);
				nppGUI._definedSessionExt = sessionExt;
				return TRUE;
			}
			case  IDC_EDIT_WORKSPACEFILEEXT:
			{
				const wstring workspaceExt = getItemText(IDC_EDIT_WORKSPACEFILEEXT);
				nppGUI._definedWorkspaceExt = workspaceExt;
				return TRUE;
			}
		}
	}
	
	switch (wParam)
	{
		case IDC_CHECK_UPDATESILENTLY:
		case IDC_CHECK_UPDATEGOTOEOF:
		{
			bool isSilent = isCheckedOrNot(IDC_CHECK_UPDATESILENTLY);
			bool isGo2End = isCheckedOrNot(IDC_CHECK_UPDATEGOTOEOF);

			auto index = combo(IDC_COMBO_FILEUPDATECHOICE)->currentIndex();

			int cd = cdDisabled;

			if (index == fileUpdateChoiceEnable || index == fileUpdateChoiceEnable4All)
			{
				if (index == fileUpdateChoiceEnable4All)
					cd |= cdEnabled4AllFiles;
				else
					cd |= cdEnabled4CurrentFile;

				if (isSilent)
					cd |= cdAutoUpdate;
				if (isGo2End)
					cd |= cdGo2end;
			}

			nppGUI._fileAutoDetection = cd;
		}
		return TRUE;

		case IDC_CHECK_DETECTENCODING:
			nppGUI._detectEncoding = isCheckedOrNot(static_cast<int>(wParam));
			return TRUE;

		case IDC_CHECK_ENABLEDOCSWITCHER :
		{
			nppGUI._doTaskList = !nppGUI._doTaskList;
			if (nppGUI._doTaskList)
			{
				enableItem(IDC_CHECK_STYLEMRU, TRUE);
			}
			else
			{
				nppGUI._styleMRU = false;
				setChecked(IDC_CHECK_STYLEMRU, false);
				enableItem(IDC_CHECK_STYLEMRU, FALSE);
			}
			return TRUE;
		}

		case IDC_CHECK_STYLEMRU :
		{
			nppGUI._styleMRU = isCheckedOrNot(IDC_CHECK_STYLEMRU);
			return TRUE;
		}

		case IDC_CHECK_SHORTTITLE:
		{
			nppGUI._shortTitlebar = isCheckedOrNot(IDC_CHECK_SHORTTITLE);
			HWND grandParent = mainWindow();
			::SendMessage(grandParent, NPPM_INTERNAL_UPDATETITLEBAR, 0, 0);
			return TRUE;
		}

		case IDC_CHECK_ENABLEDOCPEEKER:
		{
			nppGUI._isDocPeekOnTab = isCheckedOrNot(IDC_CHECK_ENABLEDOCPEEKER);
			return TRUE;
		}

		case IDC_CHECK_ENABLEDOCPEEKONMAP:
		{
			nppGUI._isDocPeekOnMap = isCheckedOrNot(IDC_CHECK_ENABLEDOCPEEKONMAP);
			return TRUE;
		}

		case IDC_CHECK_MUTE_SOUNDS:
		{
			nppGUI._muteSounds = isCheckedOrNot(IDC_CHECK_MUTE_SOUNDS);
			return TRUE;
		}
		
		case IDC_CHECK_SAVEALLCONFIRM:
		{
			nppGUI._saveAllConfirm = isCheckedOrNot(IDC_CHECK_SAVEALLCONFIRM);
			return TRUE;
		}

		case IDC_CHECK_ALOOWSIMLINKFAW:
		{
			static bool isFirstTime = true;
			nppGUI._isFawSymlinkAllowed = isCheckedOrNot(IDC_CHECK_ALOOWSIMLINKFAW);
			if (nppGUI._isFawSymlinkAllowed && isFirstTime)
			{
				NativeLangSpeaker* pNativeSpeaker = nppParam.getNativeLangSpeaker();
				pNativeSpeaker->messageBox("SymlinkLoadingWarning",
					_hSelf,
					L"Enabling symlink loading in Folder as Workspace may cause Notepad++ to enter an infinite loop and freeze.\rThis can happen when directory symlinks resolve to parent folders or form circular references.\rProceed only if you understand the risks.",
					L"Warning: Symlink Loading May Cause Freeze",
					MB_OK | MB_ICONWARNING | MB_APPLMODAL);
				isFirstTime = false;
			}
			return TRUE;
		}

		default:
		{
			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				if (LOWORD(wParam) == IDC_COMBO_FILEUPDATECHOICE)
				{
					auto index = combo(IDC_COMBO_FILEUPDATECHOICE)->currentIndex();
					
					if (index == fileUpdateChoiceEnable || index == fileUpdateChoiceEnable4All)
					{
						bool isSilent = isCheckedOrNot(IDC_CHECK_UPDATESILENTLY);
						bool isGo2End = isCheckedOrNot(IDC_CHECK_UPDATEGOTOEOF);

						int cd = cdDisabled;

						if (index == fileUpdateChoiceEnable4All)
							cd |= cdEnabled4AllFiles;
						else
							cd |= cdEnabled4CurrentFile;

						if (isSilent)
							cd |= cdAutoUpdate;
						if (isGo2End)
							cd |= cdGo2end;

						nppGUI._fileAutoDetection = cd;
						
						enableItem(IDC_CHECK_UPDATESILENTLY, TRUE);
						enableItem(IDC_CHECK_UPDATEGOTOEOF, TRUE);
					}
					else if (index == fileUpdateChoiceDisable)
					{
						enableItem(IDC_CHECK_UPDATESILENTLY, FALSE);
						enableItem(IDC_CHECK_UPDATEGOTOEOF, FALSE);
						
						nppGUI._fileAutoDetection = cdDisabled;
					}
					
					return TRUE;
				}

				else if (LOWORD(wParam) == IDC_COMBO_SYSTRAY_ACTION_CHOICE)
				{
					nppGUI._isMinimizedToTray = static_cast<int>(combo(IDC_COMBO_SYSTRAY_ACTION_CHOICE)->currentIndex());
				}

				else if (LOWORD(wParam) == IDC_COMBO_SC_TECHNOLOGY_CHOICE)
				{
					nppGUI._writeTechnologyEngine = static_cast<writeTechnologyEngine>(combo(IDC_COMBO_SC_TECHNOLOGY_CHOICE)->currentIndex());
				}

				else if (LOWORD(wParam) == IDC_COMBO_AUTOUPDATE)
				{
					nppGUI._autoUpdateOpt._doAutoUpdate = static_cast<NppGUI::AutoUpdateMode>(combo(IDC_COMBO_AUTOUPDATE)->currentIndex());
				}
			}
		}
	}
	return FALSE;
}



void NewDocumentSubDlg::makeOpenAnsiAsUtf8(bool doIt) const
{
	if (!doIt)
		setChecked(IDC_CHECK_OPENANSIASUTF8, false);
	enableItem(IDC_CHECK_OPENANSIASUTF8, doIt && !NppParameters::getInstance().isCurrentSystemCodepageUTF8());
}

intptr_t NewDocumentSubDlg::onInit()
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = nppParam.getNppGUI();
	auto& ndds = const_cast<NewDocDefaultSettings&>(nppGUI.getNewDocDefaultSettings());

	int ID2Check = IDC_RADIO_F_WIN;
	switch (ndds._format)
	{
		case EolType::windows:
			ID2Check = IDC_RADIO_F_WIN;
			break;
		case EolType::macos:
			ID2Check = IDC_RADIO_F_MAC;
			break;
		case EolType::unix:
			ID2Check = IDC_RADIO_F_UNIX;
			break;
		case EolType::unknown:
			assert(false);
			break;
	}
	setChecked(ID2Check, true);

	switch (ndds._unicodeMode)
	{
		case uni16BE :
			ID2Check = IDC_RADIO_UTF16BIG;
			break;
		case uni16LE :
			ID2Check = IDC_RADIO_UTF16SMALL;
			break;
		case uniUTF8 :
			ID2Check = IDC_RADIO_UTF8;
			break;
		case uniUTF8_NoBOM :
			ID2Check = IDC_RADIO_UTF8_NO_BOM;
			break;

		default : //uni8Bit
			ID2Check = IDC_RADIO_ANSI;
	}

	int selIndex = -1;
	wstring str;
	EncodingMapper& em = EncodingMapper::getInstance();
	for (size_t i = 0, encodingArraySize = sizeof(encodings)/sizeof(int) ; i < encodingArraySize ; ++i)
	{
		int cmdID = em.getIndexFromEncoding(encodings[i]);
		if (cmdID != -1)
		{
			cmdID += IDM_FORMAT_ENCODE;
			getNameStrFromCmd(cmdID, str);
			const auto index = static_cast<int>(addComboItem(IDC_COMBO_OTHERCP, str.c_str()));
			if (ndds._codepage == encodings[i])
				selIndex = index;
			combo(IDC_COMBO_OTHERCP)->setItemData(static_cast<int>(index), static_cast<qlonglong>(encodings[i]));
		}
	}

	if (ndds._codepage == -1 || selIndex == -1)
	{
		enableItem(IDC_COMBO_OTHERCP, false);
	}
	else
	{
		ID2Check = IDC_RADIO_OTHERCP;
		combo(IDC_COMBO_OTHERCP)->setCurrentIndex(static_cast<int>(selIndex));
	}

	setChecked(ID2Check, true);
	setChecked(IDC_CHECK_OPENANSIASUTF8, ID2Check == IDC_RADIO_UTF8_NO_BOM && ndds._openAnsiAsUtf8);

	bool isEnableAnsiAsUTF = ID2Check == IDC_RADIO_UTF8_NO_BOM;
	if (nppParam.isCurrentSystemCodepageUTF8())
	{
		isEnableAnsiAsUTF = false;
		enableItem(IDC_RADIO_ANSI, false);
	}
	enableItem(IDC_CHECK_OPENANSIASUTF8, isEnableAnsiAsUTF);

	// Add lexer languages to the Default Language dropdown
	for (int i = L_TEXT + 1 ; i < nppParam.L_END ; ++i) // Skip L_TEXT
	{
		LangType lt = static_cast<LangType>(i);
		str.clear();
		if (lt != L_USER && lt != L_JS_EMBEDDED)
		{
			int cmdID = nppParam.langTypeToCommandID(lt);
			if ((cmdID != -1))
			{
				getNameStrFromCmd(cmdID, str);
				if (str.length() > 0)
				{
					auto index = addComboItem(IDC_COMBO_DEFAULTLANG, str.c_str());
					combo(IDC_COMBO_DEFAULTLANG)->setItemData(static_cast<int>(index), static_cast<qlonglong>(lt));
				}
			}
		}
	}

	// Add UDL languages to the Default Language dropdown
	for (size_t u = 0; u < static_cast<size_t>(nppParam.getNbUserLang()); u++)
	{
		const UserLangContainer* ulc = nppParam.getULCFromIndex(u);
		const wchar_t* lName = ulc->getName();
		const LangType lt = L_USER;

		auto index = addComboItem(IDC_COMBO_DEFAULTLANG, lName);
		combo(IDC_COMBO_DEFAULTLANG)->setItemData(static_cast<int>(index), static_cast<qlonglong>(lt));
	}

	// Insert L_TEXT to the 1st position
	int normalTextCmdID = nppParam.langTypeToCommandID(L_TEXT);
	getNameStrFromCmd(normalTextCmdID, str);
	combo(IDC_COMBO_DEFAULTLANG)->insertItem(0, QString::fromStdWString(str));

	// Set chosen language
	LangType l = L_TEXT;
	size_t cbCount = combo(IDC_COMBO_DEFAULTLANG)->count();
	size_t j = 0;
	for (; j < cbCount; ++j)
	{
		l = static_cast<LangType>(combo(IDC_COMBO_DEFAULTLANG)->itemData(static_cast<int>(j)).toLongLong());
		if (ndds._lang == l)
		{
			if (l == L_USER)
			{
				const std::wstring wsUdlName = comboText(IDC_COMBO_DEFAULTLANG, static_cast<int>(j));
				if (ndds._udl == wsUdlName)
					break;
			}
			else
				break;
		}
	}
	combo(IDC_COMBO_DEFAULTLANG)->setCurrentIndex(static_cast<int>(j));

	setChecked(IDC_CHECK_ADDNEWDOCONSTARTUP, ndds._addNewDocumentOnStartup);

	setChecked(IDC_CHECK_USECONTENTASTABNAME, ndds._useContentAsTabName);

	return TRUE;
}

intptr_t NewDocumentSubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = nppParam.getNppGUI();
	auto& ndds = const_cast<NewDocDefaultSettings&>(nppGUI.getNewDocDefaultSettings());

	switch (wParam)
	{
		case IDC_RADIO_UTF16BIG:
			ndds._unicodeMode = uni16BE;
			ndds._openAnsiAsUtf8 = false;
			makeOpenAnsiAsUtf8(false);
			ndds._codepage = -1;
			enableItem(IDC_COMBO_OTHERCP, false);
			return TRUE;
		case IDC_RADIO_UTF16SMALL:
			ndds._unicodeMode = uni16LE;
			ndds._openAnsiAsUtf8 = false;
			makeOpenAnsiAsUtf8(false);
			ndds._codepage = -1;
			enableItem(IDC_COMBO_OTHERCP, false);
			return TRUE;
		case IDC_RADIO_UTF8:
			ndds._unicodeMode = uniUTF8;
			ndds._openAnsiAsUtf8 = false;
			makeOpenAnsiAsUtf8(false);
			ndds._codepage = -1;
			enableItem(IDC_COMBO_OTHERCP, false);
			return TRUE;
		case IDC_RADIO_UTF8_NO_BOM:
			ndds._unicodeMode = uniUTF8_NoBOM;
			makeOpenAnsiAsUtf8(true);
			ndds._codepage = -1;
			enableItem(IDC_COMBO_OTHERCP, false);
			return TRUE;
		case IDC_RADIO_ANSI:
			ndds._unicodeMode = uni8Bit;
			ndds._openAnsiAsUtf8 = false;
			makeOpenAnsiAsUtf8(false);
			ndds._codepage = -1;
			enableItem(IDC_COMBO_OTHERCP, false);
			return TRUE;

		case IDC_CHECK_OPENANSIASUTF8 :
			ndds._openAnsiAsUtf8 = isCheckedOrNot(IDC_CHECK_OPENANSIASUTF8);
			return TRUE;

		case IDC_RADIO_OTHERCP :
		{
			ndds._openAnsiAsUtf8 = false;
			makeOpenAnsiAsUtf8(false);
			enableItem(IDC_COMBO_OTHERCP, true);
			auto index = combo(IDC_COMBO_OTHERCP)->currentIndex();
			ndds._codepage = static_cast<int>(combo(IDC_COMBO_OTHERCP)->itemData(static_cast<int>(index)).toLongLong());
			return TRUE;
		}

		case IDC_RADIO_F_MAC:
		{
			ndds._format = EolType::macos;
			return TRUE;
		}
		case IDC_RADIO_F_UNIX:
		{
			ndds._format = EolType::unix;
			return TRUE;
		}
		case IDC_RADIO_F_WIN:
		{
			ndds._format = EolType::windows;
			return TRUE;
		}

		case IDC_CHECK_ADDNEWDOCONSTARTUP:
		{
			ndds._addNewDocumentOnStartup = isCheckedOrNot(IDC_CHECK_ADDNEWDOCONSTARTUP);
			return TRUE;
		}

		case IDC_CHECK_USECONTENTASTABNAME:
		{
			ndds._useContentAsTabName = isCheckedOrNot(IDC_CHECK_USECONTENTASTABNAME);
			return TRUE;
		}

		default:
		{
			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				if (LOWORD(wParam) == IDC_COMBO_DEFAULTLANG)
				{
					auto index = combo(IDC_COMBO_DEFAULTLANG)->currentIndex();
					ndds._lang = static_cast<LangType>(combo(IDC_COMBO_DEFAULTLANG)->itemData(static_cast<int>(index)).toLongLong());
					if (ndds._lang == L_USER)
					{
						const std::wstring wsUdlName = comboText(IDC_COMBO_DEFAULTLANG, static_cast<int>(index));
						ndds._udl = wsUdlName;
					}
					else
						ndds._udl = L"";
					return TRUE;
				}
				else if (LOWORD(wParam) == IDC_COMBO_OTHERCP)
				{
					auto index = combo(IDC_COMBO_OTHERCP)->currentIndex();
					ndds._codepage = static_cast<int>(combo(IDC_COMBO_OTHERCP)->itemData(static_cast<int>(index)).toLongLong());
					return TRUE;
				}
			}
			return FALSE;
		}
	}
	return FALSE;
}


intptr_t DefaultDirectorySubDlg::onInit()
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = nppParam.getNppGUI();

	int ID2Check = 0;
	bool shouldActivated;
	switch (nppGUI._openSaveDir)
	{
		case dir_last :
			ID2Check = IDC_OPENSAVEDIR_REMEMBERLAST_RADIO;
			shouldActivated = false;
			break;
		case dir_userDef :
			ID2Check = IDC_OPENSAVEDIR_ALWAYSON_RADIO;
			shouldActivated = true;
			break;

		default : 
			ID2Check = IDC_OPENSAVEDIR_FOLLOWCURRENT_RADIO;
			shouldActivated = false;
	}
	setChecked(ID2Check, true);
	enableItem(IDD_OPENSAVEDIR_ALWAYSON_BROWSE_BUTTON, shouldActivated);
	enableItem(IDC_OPENSAVEDIR_ALWAYSON_EDIT, shouldActivated); // has to precede the next WM_SETTEXT call (for fixing the consequent EN_CHANGE notification)
	setItemText(IDC_OPENSAVEDIR_ALWAYSON_EDIT, nppGUI._defaultDir);

	setChecked(IDC_OPENSAVEDIR_CHECK_DROPFOLDEROPENFILES, nppGUI._isFolderDroppedOpenFiles);

	return TRUE;
}

intptr_t DefaultDirectorySubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = nppParam.getNppGUI();

	if (HIWORD(wParam) == EN_CHANGE)
	{
		switch (LOWORD(wParam))
		{
			case  IDC_OPENSAVEDIR_ALWAYSON_EDIT:
			{
				if (dlgItem(IDC_OPENSAVEDIR_ALWAYSON_EDIT)->isEnabled()) // changed by user (and not by the WM_INITDIALOG WM_SETTEXT)
				{
					nppGUI._defaultDir = getItemText(IDC_OPENSAVEDIR_ALWAYSON_EDIT);
					nppGUI._defaultDirExp = expandEnvironmentStrs(nppGUI._defaultDir);
					nppParam.setWorkingDir(nppGUI._defaultDirExp.c_str());
				}
				return TRUE;
			}
		}
	}

	switch (wParam)
	{
		case IDC_OPENSAVEDIR_FOLLOWCURRENT_RADIO:
			nppGUI._openSaveDir = dir_followCurrent;
			::SendMessage(mainWindow(), NPPM_INTERNAL_REFRESHWORKDIR, 0, 0);
			enableItem(IDC_OPENSAVEDIR_ALWAYSON_EDIT, false);
			enableItem(IDD_OPENSAVEDIR_ALWAYSON_BROWSE_BUTTON, false);
			return TRUE;
		case IDC_OPENSAVEDIR_REMEMBERLAST_RADIO:
			nppGUI._openSaveDir = dir_last;
			enableItem(IDC_OPENSAVEDIR_ALWAYSON_EDIT, false);
			enableItem(IDD_OPENSAVEDIR_ALWAYSON_BROWSE_BUTTON, false);
			return TRUE;
		case IDC_OPENSAVEDIR_ALWAYSON_RADIO:
			nppGUI._openSaveDir = dir_userDef;
			::SendMessage(mainWindow(), NPPM_INTERNAL_REFRESHWORKDIR, 0, 0);
			enableItem(IDC_OPENSAVEDIR_ALWAYSON_EDIT, true);
			enableItem(IDD_OPENSAVEDIR_ALWAYSON_BROWSE_BUTTON, true);
			return TRUE;

		case IDD_OPENSAVEDIR_ALWAYSON_BROWSE_BUTTON :
			{
				wstring title = nppParam.getNativeLangSpeaker()->getLocalizedStrFromID("default-open-save-select-folder",
					L"Select a folder as default directory");
				folderBrowser(_hSelf, title, qobject_cast<QLineEdit*>(dlgItem(IDC_OPENSAVEDIR_ALWAYSON_EDIT)));
			}
			return TRUE;

		case IDC_OPENSAVEDIR_CHECK_DROPFOLDEROPENFILES:
			nppGUI._isFolderDroppedOpenFiles = isCheckedOrNot(IDC_OPENSAVEDIR_CHECK_DROPFOLDEROPENFILES);
			return TRUE;

		default:
			return FALSE;
	}
	return FALSE;
}


void RecentFilesHistorySubDlg::setCustomLen(int val)
{
	setItemInt(IDC_EDIT_CUSTOMIZELENGTHVAL, val);
	enableItem(IDC_EDIT_CUSTOMIZELENGTHVAL, val > 0);
}

intptr_t RecentFilesHistorySubDlg::onInit()
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = nppParam.getNppGUI();

	// Max number recent file setting
	setItemInt(IDC_EDIT_MAXNBFILEVAL, nppParam.getNbMaxRecentFile());

	// Check on launch time settings
	setChecked(IDC_CHECK_DONTCHECKHISTORY, !nppGUI._checkHistoryFiles);

	// Display in submenu setting
	setChecked(IDC_CHECK_INSUBMENU, nppParam.putRecentFileInSubMenu());

	// Recent File menu entry length setting
	int customLength = nppParam.getRecentFileCustomLength();
	int id = IDC_RADIO_CUSTOMIZELENTH;
	int length = customLength;

	if (customLength == RECENTFILES_SHOWONLYFILENAME)
	{
		id = IDC_RADIO_ONLYFILENAME;
		length = 0;
	}
	else if (customLength == RECENTFILES_SHOWFULLPATH || customLength < 0)
	{
		id = IDC_RADIO_FULLFILENAMEPATH;
		length = 0;
	}

	setChecked(id);
	setCustomLen(length);

	return TRUE;
}

intptr_t RecentFilesHistorySubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = nppParam.getNppGUI();

	switch (LOWORD(wParam))
	{
		case IDC_EDIT_MAXNBFILEVAL:
		{
			switch (HIWORD(wParam))
			{
				case EN_KILLFOCUS:
				{
					static constexpr int stringSize = 3;
					wchar_t str[stringSize]{};
					copyItemText(IDC_EDIT_MAXNBFILEVAL, str, stringSize);

					if (wcscmp(str, L"") == 0)
					{
						setItemInt(IDC_EDIT_MAXNBFILEVAL, nppParam.getNbMaxRecentFile());
						return FALSE;
					}

					UINT nbMaxFile = getItemInt(IDC_EDIT_MAXNBFILEVAL);

					if (nbMaxFile == nppParam.getNbMaxRecentFile())
					{
						return FALSE;
					}

					if (nbMaxFile > NB_MAX_LRF_FILE)
					{
						setItemInt(IDC_EDIT_MAXNBFILEVAL, NB_MAX_LRF_FILE);

						nbMaxFile = NB_MAX_LRF_FILE;
					}

					nppParam.setNbMaxRecentFile(nbMaxFile);
					::SendMessage(mainWindow(), NPPM_INTERNAL_SETTING_HISTORY_SIZE, 0, 0);

					return TRUE;
				}

				default:
				{
					break;
				}
			}

			return FALSE;
		}

		case IDC_EDIT_CUSTOMIZELENGTHVAL:
		{
			if (!isCheckedOrNot(IDC_RADIO_CUSTOMIZELENTH))
			{
				return FALSE;
			}

			switch (HIWORD(wParam))
			{
				case EN_KILLFOCUS:
				{
					static constexpr int stringSize = 4;
					wchar_t str[stringSize]{};
					copyItemText(IDC_EDIT_CUSTOMIZELENGTHVAL, str, stringSize);

					if (wcscmp(str, L"") == 0)
					{
						setItemInt(IDC_EDIT_CUSTOMIZELENGTHVAL, nppParam.getRecentFileCustomLength());
						return FALSE;
					}

					UINT size = getItemInt(IDC_EDIT_CUSTOMIZELENGTHVAL);

					if (size == static_cast<UINT>(nppParam.getRecentFileCustomLength()))
					{
						return FALSE;
					}

					bool change = false;

					if (size == 0)
					{
						size = NB_DEFAULT_LRF_CUSTOMLENGTH;
						change = true;
					}
					else if (size > NB_MAX_LRF_CUSTOMLENGTH)
					{
						size = NB_MAX_LRF_CUSTOMLENGTH;
						change = true;
					}

					if (change)
					{
						setItemInt(IDC_EDIT_CUSTOMIZELENGTHVAL, size);
					}

					nppParam.setRecentFileCustomLength(size);
					::SendMessage(mainWindow(), NPPM_INTERNAL_RECENTFILELIST_UPDATE, 0, 0);

					return TRUE;
				}

				default:
				{
					break;
				}
			}

			return FALSE;
		}

		default:
		{
			break;
		}
	}

	switch (wParam)
	{
		case IDC_CHECK_DONTCHECKHISTORY:
			nppGUI._checkHistoryFiles = !isCheckedOrNot(IDC_CHECK_DONTCHECKHISTORY);
			return TRUE;

		case IDC_CHECK_INSUBMENU:
			nppParam.setPutRecentFileInSubMenu(isCheckedOrNot(IDC_CHECK_INSUBMENU));
			::SendMessage(mainWindow(), NPPM_INTERNAL_RECENTFILELIST_SWITCH, 0, 0);
			return TRUE;

		case IDC_RADIO_ONLYFILENAME:
		{
			setCustomLen(0);
			nppParam.setRecentFileCustomLength(0);
			::SendMessage(mainWindow(), NPPM_INTERNAL_RECENTFILELIST_UPDATE, 0, 0);

			redrawDlgItem(IDC_CUSTOMIZELENGTH_RANGE_STATIC);

			return TRUE;
		}

		case IDC_RADIO_FULLFILENAMEPATH:
		{
			setCustomLen(0);
			nppParam.setRecentFileCustomLength(-1);
			::SendMessage(mainWindow(), NPPM_INTERNAL_RECENTFILELIST_UPDATE, 0, 0);

			redrawDlgItem(IDC_CUSTOMIZELENGTH_RANGE_STATIC);

			return TRUE;
		}

		case IDC_RADIO_CUSTOMIZELENTH:
		{
			int len = nppParam.getRecentFileCustomLength();
			if (len <= 0)
			{
				setCustomLen(NB_DEFAULT_LRF_CUSTOMLENGTH);
				nppParam.setRecentFileCustomLength(NB_DEFAULT_LRF_CUSTOMLENGTH);
				::SendMessage(mainWindow(), NPPM_INTERNAL_RECENTFILELIST_UPDATE, 0, 0);
			}

			redrawDlgItem(IDC_CUSTOMIZELENGTH_RANGE_STATIC);

			return TRUE;
		}

		default:
			return FALSE;
	}
	return FALSE;
}


intptr_t IndentationSubDlg::onInit()
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI & nppGUI = nppParam.getNppGUI();

	//
	// Tab settings
	//
	setItemInt(IDC_EDIT_TABSIZEVAL, nppGUI._tabSize);
	setChecked(IDC_RADIO_REPLACEBYSPACE, nppGUI._tabReplacedBySpace);
	setChecked(IDC_RADIO_USINGTAB, !nppGUI._tabReplacedBySpace);
	setChecked(IDC_CHECK_BACKSPACEUNINDENT, nppGUI._backspaceUnindent);

	addListItem(IDC_LIST_TABSETTNG, L"[Default]");
	const int nbLang = nppParam.getNbLang();
	for (int i = 0; i < nbLang; ++i)
	{
		Lang* lang = nppParam.getLangFromIndex(i);
		if (!lang) continue;

		LanguageNameInfo lni = nppParam.getLangNameInfoFromNameID(lang->_langName);
		if (!lni._shortName || lni._langID == L_JS_EMBEDDED) continue;
		
		auto j = addListItem(IDC_LIST_TABSETTNG, lni._shortName);
		if (j != -1)
			list(IDC_LIST_TABSETTNG)->item(static_cast<int>(j))->setData(Qt::UserRole, static_cast<qlonglong>(reinterpret_cast<LPARAM>(lang)));
	}
	const int index2Begin = 0;
	setListRow(IDC_LIST_TABSETTNG, static_cast<int>(index2Begin));
	showItem(IDC_CHECK_DEFAULTTABVALUE, false);

	//
	// Auto-indent settings
	//
	int choiceID = IDC_RADIO_AUTOINDENT_ADVANCED;
	if (nppGUI._maintainIndent == autoIndent_none)
		choiceID = IDC_RADIO_AUTOINDENT_NONE;
	else if (nppGUI._maintainIndent == autoIndent_basic)
		choiceID = IDC_RADIO_AUTOINDENT_BASIC;

	setChecked(choiceID, TRUE);

	NativeLangSpeaker* pNativeSpeaker = nppParam.getNativeLangSpeaker();

	wstring tipAutoIndentBasic2Show = pNativeSpeaker->getLocalizedStrFromID("autoIndentBasic-tip",
		L"Ensure that the indentation of the current line (i.e. the new line created by pressing the ENTER key) matches the indentation of the previous line.");

	wstring tipAutoIndentAdvanced2show = pNativeSpeaker->getLocalizedStrFromID("autoIndentAdvanced-tip",
		L"Enable smart indentation for \"C-like\" languages and Python. The \"C-like\" languages include:\n"\
		L"C, C++, Java, C#, Objective-C, PHP, JavaScript, JSP, CSS, Perl, Rust, PowerShell, TypeScript, Go, Swift and JSON.\n"\
		L"\n"\
		L"If you select advanced mode but do not edit files in the aforementioned languages, the indentation will remain in basic mode.");

	setTip(IDC_RADIO_AUTOINDENT_BASIC, tipAutoIndentBasic2Show);
	setTip(IDC_RADIO_AUTOINDENT_ADVANCED, tipAutoIndentAdvanced2show);

	return TRUE;
}

intptr_t IndentationSubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI & nppGUI = nppParam.getNppGUI();

	switch (HIWORD(wParam))
	{
		case LBN_SELCHANGE:
		{
			if (LOWORD(wParam) == IDC_LIST_TABSETTNG)
			{
				auto index = list(IDC_LIST_TABSETTNG)->currentRow();
				if (index == -1)
					return FALSE;
				showItem(IDC_CHECK_DEFAULTTABVALUE, index > 0);

				if (index > 0)
				{
					const Lang* lang = reinterpret_cast<Lang*>(list(IDC_LIST_TABSETTNG)->item(static_cast<int>(index))->data(Qt::UserRole).toLongLong());
					if (!lang) return FALSE;

					bool useDefaultTab = (lang->_tabSize == -1 || lang->_tabSize == 0);

					setChecked(IDC_CHECK_DEFAULTTABVALUE, useDefaultTab);

					setItemInt(IDC_EDIT_TABSIZEVAL, useDefaultTab ? nppGUI._tabSize : lang->_tabSize);
					setChecked(IDC_RADIO_REPLACEBYSPACE, useDefaultTab ? nppGUI._tabReplacedBySpace : lang->_isTabReplacedBySpace);
					setChecked(IDC_RADIO_USINGTAB, useDefaultTab ? !nppGUI._tabReplacedBySpace : !lang->_isTabReplacedBySpace);
					setChecked(IDC_CHECK_BACKSPACEUNINDENT, useDefaultTab ? nppGUI._backspaceUnindent : lang->_isBackspaceUnindent);

					enableItem(IDC_EDIT_TABSIZEVAL, !useDefaultTab);
					enableItem(IDC_RADIO_REPLACEBYSPACE, !useDefaultTab);
					enableItem(IDC_RADIO_USINGTAB, !useDefaultTab);
					enableItem(IDC_CHECK_BACKSPACEUNINDENT, !useDefaultTab);
				}
				else
				{
					setItemInt(IDC_EDIT_TABSIZEVAL, nppGUI._tabSize);
					setChecked(IDC_RADIO_REPLACEBYSPACE, nppGUI._tabReplacedBySpace);
					setChecked(IDC_RADIO_USINGTAB, !nppGUI._tabReplacedBySpace);
					setChecked(IDC_CHECK_BACKSPACEUNINDENT, nppGUI._backspaceUnindent);

					enableItem(IDC_EDIT_TABSIZEVAL, TRUE);
					enableItem(IDC_RADIO_REPLACEBYSPACE, TRUE);
					enableItem(IDC_RADIO_USINGTAB, TRUE);
					enableItem(IDC_CHECK_BACKSPACEUNINDENT, TRUE);
				}

				redrawDlgItem(IDC_TABSIZE_STATIC);
				redrawDlgItem(IDC_INDENTUSING_STATIC);

				return TRUE;
			}

			break;
		}

		case EN_CHANGE:
		{
			switch (LOWORD(wParam))
			{
				case IDC_EDIT_TABSIZEVAL:
				{
					const auto tabSize = getItemInt(IDC_EDIT_TABSIZEVAL);
					if (tabSize < 1)
					{
						return FALSE;
					}

					const bool useDefaultTab = isCheckedOrNot(IDC_CHECK_DEFAULTTABVALUE);
					const auto index = list(IDC_LIST_TABSETTNG)->currentRow();
					if (!useDefaultTab && index > 0)
					{
						Lang* lang = reinterpret_cast<Lang*>(list(IDC_LIST_TABSETTNG)->item(static_cast<int>(index))->data(Qt::UserRole).toLongLong());
						if (lang == nullptr) return FALSE;

						lang->_tabSize = tabSize;

						// write in langs.xml
						nppParam.insertTabInfo(lang->getLangName(), lang->getTabInfo(), lang->_isBackspaceUnindent);
					}
					else
					{
						nppGUI._tabSize = tabSize;
					}

					::SendMessage(mainWindow(), NPPM_INTERNAL_SET_TAB_SETTINGS, 0, 0);
					return TRUE;
				}

				default:
				{
					break;
				}
			}
			break;
		}

		case EN_KILLFOCUS:
		{
			switch (LOWORD(wParam))
			{
				case IDC_EDIT_TABSIZEVAL:
				{
					const auto tabSize = getItemInt(IDC_EDIT_TABSIZEVAL);

					if (tabSize < 1)
					{
						const bool useDefaultTab = isCheckedOrNot(IDC_CHECK_DEFAULTTABVALUE);
						const auto index = list(IDC_LIST_TABSETTNG)->currentRow();
						auto prevSize = nppGUI._tabSize;
						if (!useDefaultTab && index > 0)
						{
							const Lang* lang = reinterpret_cast<Lang*>(list(IDC_LIST_TABSETTNG)->item(static_cast<int>(index))->data(Qt::UserRole).toLongLong());
							if (lang != nullptr && lang->_tabSize > 0)
							{
								prevSize = lang->_tabSize;
							}
						}

						setItemInt(IDC_EDIT_TABSIZEVAL, prevSize);
						return TRUE;
					}
					return FALSE;
				}

				default:
				{
					break;
				}
			}
			break;
		}

		default:
		{
			break;
		}
	}


	switch (wParam)
	{
		case IDC_RADIO_REPLACEBYSPACE:
		case IDC_RADIO_USINGTAB:
		{
			bool isTabReplacedBySpace = isCheckedOrNot(IDC_RADIO_REPLACEBYSPACE);

			auto index = list(IDC_LIST_TABSETTNG)->currentRow();
			if (index == -1) return FALSE;

			if (index != 0)
			{
				Lang* lang = reinterpret_cast<Lang*>(list(IDC_LIST_TABSETTNG)->item(static_cast<int>(index))->data(Qt::UserRole).toLongLong());
				if (!lang) return FALSE;
				if (!lang->_tabSize || lang->_tabSize == -1)
					lang->_tabSize = nppGUI._tabSize;

				lang->_isTabReplacedBySpace = isTabReplacedBySpace;

				// write in langs.xml
				nppParam.insertTabInfo(lang->getLangName(), lang->getTabInfo(), lang->_isBackspaceUnindent);
			}
			else
			{
				nppGUI._tabReplacedBySpace = isTabReplacedBySpace;
			}

			::SendMessage(mainWindow(), NPPM_INTERNAL_SET_TAB_SETTINGS, 0, 0);

			return TRUE;
		}

		case IDC_CHECK_BACKSPACEUNINDENT:
		{
			bool isBackspaceUnindent = isCheckedOrNot(IDC_CHECK_BACKSPACEUNINDENT);

			auto index = list(IDC_LIST_TABSETTNG)->currentRow();
			if (index == -1) return FALSE;

			if (index != 0)
			{
				Lang* lang = reinterpret_cast<Lang*>(list(IDC_LIST_TABSETTNG)->item(static_cast<int>(index))->data(Qt::UserRole).toLongLong());
				if (!lang) return FALSE;
				if (!lang->_tabSize || lang->_tabSize == -1)
					lang->_tabSize = nppGUI._tabSize;

				lang->_isBackspaceUnindent = isBackspaceUnindent;

				// write in langs.xml
				nppParam.insertTabInfo(lang->getLangName(), lang->getTabInfo(), lang->_isBackspaceUnindent);
			}
			else
			{
				nppGUI._backspaceUnindent = isBackspaceUnindent;
			}

			::SendMessage(mainWindow(), NPPM_INTERNAL_SET_TAB_SETTINGS, 0, 0);

			return TRUE;
		}

		case IDC_CHECK_DEFAULTTABVALUE:
		{
			const bool useDefaultTab = isCheckedOrNot(IDC_CHECK_DEFAULTTABVALUE);
			const auto index = list(IDC_LIST_TABSETTNG)->currentRow();
			if (index == -1 || index == 0) // index == 0 shouldn't happen
				return FALSE;

			Lang* lang = reinterpret_cast<Lang*>(list(IDC_LIST_TABSETTNG)->item(static_cast<int>(index))->data(Qt::UserRole).toLongLong());
			if (!lang)
				return FALSE;

			//- Set tab setting in chosen language
			lang->_tabSize = useDefaultTab ? 0 : nppGUI._tabSize;
			lang->_isTabReplacedBySpace = useDefaultTab ? false : nppGUI._tabReplacedBySpace;
			lang->_isBackspaceUnindent = useDefaultTab ? false : nppGUI._backspaceUnindent;

			//- set visual effect
			setItemInt(IDC_EDIT_TABSIZEVAL, useDefaultTab ? nppGUI._tabSize : lang->_tabSize);
			setChecked(IDC_RADIO_REPLACEBYSPACE, useDefaultTab ? nppGUI._tabReplacedBySpace : lang->_isTabReplacedBySpace);
			setChecked(IDC_RADIO_USINGTAB, useDefaultTab ? !nppGUI._tabReplacedBySpace : !lang->_isTabReplacedBySpace);
			setChecked(IDC_CHECK_BACKSPACEUNINDENT, useDefaultTab ? nppGUI._backspaceUnindent : lang->_isBackspaceUnindent);
			enableItem(IDC_EDIT_TABSIZEVAL, !useDefaultTab);
			enableItem(IDC_RADIO_REPLACEBYSPACE, !useDefaultTab);
			enableItem(IDC_RADIO_USINGTAB, !useDefaultTab);
			enableItem(IDC_CHECK_BACKSPACEUNINDENT, !useDefaultTab);

			// write in langs.xml
			if (useDefaultTab)
				nppParam.insertTabInfo(lang->getLangName(), -1, false);

			redrawDlgItem(IDC_TABSIZE_STATIC);
			redrawDlgItem(IDC_INDENTUSING_STATIC);

			return TRUE;
		}

		case IDC_RADIO_AUTOINDENT_NONE:
		{
			nppGUI._maintainIndent = autoIndent_none;
			return TRUE;
		}
		case IDC_RADIO_AUTOINDENT_BASIC:
		{
			nppGUI._maintainIndent = autoIndent_basic;
			return TRUE;
		}
		case IDC_RADIO_AUTOINDENT_ADVANCED:
		{
			nppGUI._maintainIndent = autoIndent_advanced;
			return TRUE;
		}

		default:
		{
			break;
		}
	}
	return FALSE;
}



intptr_t LanguageSubDlg::onInit()
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI & nppGUI = nppParam.getNppGUI();
	NativeLangSpeaker *pNativeSpeaker = nppParam.getNativeLangSpeaker();

	//
	// Lang Menu
	//
	for (int i = L_TEXT ; i < nppParam.L_END ; ++i)
	{
		if (static_cast<LangType>(i) != L_USER && static_cast<LangType>(i) != L_JS_EMBEDDED)
		{
			int cmdID = nppParam.langTypeToCommandID(static_cast<LangType>(i));
			if ((cmdID != -1))
			{
				wstring str;
				getNameStrFromCmd(cmdID, str);
				if (str.length() > 0)
				{
					_langList.push_back(LangMenuItem(static_cast<LangType>(i), cmdID, str));
				}
			}
		}
	}

	std::sort(_langList.begin(), _langList.end());

	for (size_t i = 0, len = _langList.size(); i < len; ++i)
	{
		addListItem(IDC_LIST_ENABLEDLANG, _langList[i]._langName.c_str());
	}

	std::sort(nppGUI._excludedLangList.begin(), nppGUI._excludedLangList.end());
		
	for (size_t i = 0, len = nppGUI._excludedLangList.size(); i < len ; ++i)
	{
		addListItem(IDC_LIST_DISABLEDLANG, nppGUI._excludedLangList[i]._langName.c_str());
	}

	setChecked(IDC_CHECK_LANGMENUCOMPACT, nppGUI._isLangMenuCompact);
	enableItem(IDC_BUTTON_REMOVE, FALSE);
	enableItem(IDC_BUTTON_RESTORE, FALSE);

	setChecked(IDC_CHECK_BACKSLASHISESCAPECHARACTERFORSQL, nppGUI._backSlashIsEscapeCharacterForSql);

	return TRUE;
}

intptr_t LanguageSubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI & nppGUI = nppParam.getNppGUI();
	NativeLangSpeaker *pNativeSpeaker = nppParam.getNativeLangSpeaker();

	switch (HIWORD(wParam))
	{
		case LBN_SELCHANGE:
		{
			// Lang Menu
			if (LOWORD(wParam) == IDC_LIST_DISABLEDLANG || LOWORD(wParam) == IDC_LIST_ENABLEDLANG)
			{
				int idButton2Enable;
				int idButton2Disable;

				if (LOWORD(wParam) == IDC_LIST_ENABLEDLANG)
				{
					idButton2Enable = IDC_BUTTON_REMOVE;
					idButton2Disable = IDC_BUTTON_RESTORE;
				}
				else //IDC_LIST_DISABLEDLANG
				{
					idButton2Enable = IDC_BUTTON_RESTORE;
					idButton2Disable = IDC_BUTTON_REMOVE;
				}

				auto i = list(LOWORD(wParam))->currentRow();
				if (i != -1)
				{
					enableItem(idButton2Enable, TRUE);
					int idListbox2Disable = (LOWORD(wParam) == IDC_LIST_ENABLEDLANG) ? IDC_LIST_DISABLEDLANG : IDC_LIST_ENABLEDLANG;
					setListRow(idListbox2Disable, static_cast<int>(static_cast<WPARAM>(-1)));
					enableItem(idButton2Disable, FALSE);
				}
				return TRUE;

			}

			break;
		}

		// Check if it is double click
		case LBN_DBLCLK:
		{
			// Lang Menu
			if (LOWORD(wParam) == IDC_LIST_DISABLEDLANG || LOWORD(wParam) == IDC_LIST_ENABLEDLANG)
			{
				// On double click an item, the item should be moved
				// from one list to other list

				onCommand(dlgItem(IDC_LIST_ENABLEDLANG) == reinterpret_cast<HWND>(lParam) ? IDC_BUTTON_REMOVE : IDC_BUTTON_RESTORE, 0);
				return TRUE;
			}

			break;
		}

		default:
		{
			break;
		}
	}


	switch (wParam)
	{
		//
		// Lang Menu
		//
		case IDC_CHECK_LANGMENUCOMPACT:
		{
			nppGUI._isLangMenuCompact = isCheckedOrNot(IDC_CHECK_LANGMENUCOMPACT);
			pNativeSpeaker->messageBox("LanguageMenuCompactWarning",
				_hSelf,
				L"This option will be changed on the next launch.",
				L"Compact Language Menu",
				MB_OK);
			return TRUE;
		}
		
		case IDC_CHECK_BACKSLASHISESCAPECHARACTERFORSQL:
		{
			nppGUI._backSlashIsEscapeCharacterForSql = isCheckedOrNot(IDC_CHECK_BACKSLASHISESCAPECHARACTERFORSQL);
			HWND hwndNPP = mainWindow();
			::SendMessage(hwndNPP, NPPM_INTERNAL_SQLBACKSLASHESCAPE, 0, reinterpret_cast<LPARAM>(hwndNPP));
			return TRUE;
		}

		case IDC_BUTTON_RESTORE :
		case IDC_BUTTON_REMOVE :
		{
			int list2Remove, list2Add, idButton2Enable, idButton2Disable;
			vector<LangMenuItem> *pSrcLst, *pDestLst;

			if (LOWORD(wParam)==IDC_BUTTON_REMOVE)
			{
				list2Remove = IDC_LIST_ENABLEDLANG;
				list2Add = IDC_LIST_DISABLEDLANG;
				idButton2Enable = IDC_BUTTON_RESTORE;
				idButton2Disable = IDC_BUTTON_REMOVE;
				pSrcLst = &_langList;
				pDestLst = &nppGUI._excludedLangList;
			}
			else
			{
				list2Remove = IDC_LIST_DISABLEDLANG;
				list2Add = IDC_LIST_ENABLEDLANG;
				idButton2Enable = IDC_BUTTON_REMOVE;
				idButton2Disable = IDC_BUTTON_RESTORE;
				pSrcLst = &nppGUI._excludedLangList;
				pDestLst = &_langList;
			}
			size_t iRemove = list(list2Remove)->currentRow();
			if (static_cast<intptr_t>(iRemove) == -1)
				return TRUE;

			const wstring s = list(list2Remove)->item(static_cast<int>(iRemove))->text().toStdWString();

			LangMenuItem lmi = pSrcLst->at(iRemove);
			vector<LangMenuItem>::iterator lang2Remove = pSrcLst->begin() + iRemove;
			pSrcLst->erase(lang2Remove);

			auto iAdd = addListItem(list2Add, s);
			delete list(list2Remove)->takeItem(static_cast<int>(iRemove));
			pDestLst->push_back(lmi);

			setListRow(list2Add, static_cast<int>(iAdd));
			setListRow(list2Remove, -1);
			enableItem(idButton2Enable, TRUE);
			enableItem(idButton2Disable, FALSE);

			if ((lmi._langType >= L_EXTERNAL) && (lmi._langType < nppParam.L_END))
			{
				const std::string langName = wstring2string(lmi._langName);
				const char* excludeValue = (LOWORD(wParam) == IDC_BUTTON_REMOVE) ? "yes" : "no";
				bool found = false;
				for (const auto& extLexer : *nppParam.getExternalLexerDoc())
				{
					NppXml::Element root = NppXml::firstChildElement(extLexer._doc, "NotepadPlus");
					if (!root)
						continue;

					NppXml::Element lexersRoot = NppXml::firstChildElement(root, "LexerStyles");
					if (!lexersRoot)
						continue;

					for (NppXml::Element childNode = NppXml::firstChildElement(lexersRoot, "LexerType");
						childNode;
						childNode = NppXml::nextSiblingElement(childNode, "LexerType"))
					{
						if (NppXml::attribute(childNode, "name", "") == langName)
						{
							NppXml::setAttribute(childNode, "excluded", excludeValue);
							static_cast<void>(NppXml::saveFile(extLexer._doc, extLexer._path.c_str()));
							found = true;
							break;
						}
					}

					if (found)
						break;
				}
			}

			HWND grandParent = mainWindow();
			HMENU mainMenu = reinterpret_cast<HMENU>(::SendMessage(grandParent, NPPM_INTERNAL_GETMENU, 0, 0));
			HMENU subLangMenu = nppSubMenuAt(mainMenu, MENUINDEX_LANGUAGE);
			int nbLangItems = nppMenuItemCount(subLangMenu);

			if (LOWORD(wParam)==IDC_BUTTON_REMOVE)
			{
				nppRemoveMenuCommand(mainMenu, lmi._cmdID);

				// If using compact language menu, walk through the first letter sub-menu
				// and remove the sub-menu if it is empty after removing the language.
				if (nppGUI._isLangMenuCompact)
				{
					for (int i = nbLangItems - 1; i >= 0; --i)
					{
						HMENU hSubMenu = nppSubMenuAt(subLangMenu, i);
						if (hSubMenu && nppMenuItemCount(hSubMenu) == 0)
						{
							nppRemoveMenuItemAt(subLangMenu, i);
							break;
						}
					}
				}
			}
			else // IDC_BUTTON_RESTORE
			{
				// Find the first separator which is between IDM_LANG_TEXT and languages
				int x = 0;
				for (; x < nbLangItems; ++x)
				{
					if (nppMenuActionAt(subLangMenu, x)->isSeparator())
					{
						break;
					}
				}

				// Find the location in existing language menu to insert to. This includes submenu if using compact language menu.
				wchar_t firstLetter = lmi._langName.empty() ? L'\0' : towupper(lmi._langName[0]);

				bool enteredLetterSubMenu = false;
				HMENU subMenu2InsertIn = subLangMenu;
				for (++x; x < nbLangItems; ++x)
				{
					const QAction* action = nppMenuActionAt(subMenu2InsertIn, x);
					const wstring itemText = action->text().toStdWString();

					// Check if using compact language menu.
					if (action->menu() && !itemText.empty() && itemText[0] == firstLetter)
					{
						// Found the submenu for the language's first letter. Search in it instead.
						subMenu2InsertIn = action->menu();
						nbLangItems = nppMenuItemCount(subMenu2InsertIn);
						x = -1;
						enteredLetterSubMenu = true;
					}
					else if (action->isSeparator() || (wcscmp(lmi._langName.c_str(), itemText.c_str()) < 0))
					{
						break;
					}
				}

				if (nppGUI._isLangMenuCompact && !enteredLetterSubMenu)
				{
					// The letter submenu was removed because it became empty; recreate it at the alphabetically correct position (x, computed above),
					// then insert the language as its first (and only) entry.
					HMENU newLetterSubMenu = new QMenu(subLangMenu);
					nppInsertSubMenuAt(subLangMenu, x, newLetterSubMenu, wstring(1, firstLetter));
					nppInsertMenuItemAt(newLetterSubMenu, 0, lmi._cmdID, lmi._langName, grandParent);
				}
				else
				{
					nppInsertMenuItemAt(subMenu2InsertIn, x, lmi._cmdID, lmi._langName, grandParent);
				}
			}

			return TRUE;
		}

		default:
		{
			break;
		}
	}
	return FALSE;
}


intptr_t HighlightingSubDlg::onInit()
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = nppParam.getNppGUI();

	setChecked(IDC_CHECK_MARKALLCASESENSITIVE, nppGUI._markAllCaseSensitive);
	setChecked(IDC_CHECK_MARKALLWHOLEWORDONLY, nppGUI._markAllWordOnly);

	setChecked(IDC_CHECK_ENABLSMARTHILITE, nppGUI._enableSmartHilite);
	setChecked(IDC_CHECK_SMARTHILITECASESENSITIVE, nppGUI._smartHiliteCaseSensitive);
	setChecked(IDC_CHECK_SMARTHILITEWHOLEWORDONLY, nppGUI._smartHiliteWordOnly);
	setChecked(IDC_CHECK_SMARTHILITEUSEFINDSETTINGS, nppGUI._smartHiliteUseFindSettings);
	setChecked(IDC_CHECK_SMARTHILITEANOTHERRVIEW, nppGUI._smartHiliteOnAnotherView);

	setChecked(IDC_CHECK_ENABLTAGSMATCHHILITE, nppGUI._enableTagsMatchHilite);
	setChecked(IDC_CHECK_ENABLTAGATTRHILITE, nppGUI._enableTagAttrsHilite);
	setChecked(IDC_CHECK_HIGHLITENONEHTMLZONE, nppGUI._enableHiliteNonHTMLZone);

	enableItem(IDC_CHECK_ENABLTAGATTRHILITE, nppGUI._enableTagsMatchHilite);
	enableItem(IDC_CHECK_HIGHLITENONEHTMLZONE, nppGUI._enableTagsMatchHilite);

	enableItem(IDC_CHECK_SMARTHILITECASESENSITIVE, nppGUI._enableSmartHilite);
	enableItem(IDC_CHECK_SMARTHILITEWHOLEWORDONLY, nppGUI._enableSmartHilite);
	enableItem(IDC_CHECK_SMARTHILITEUSEFINDSETTINGS, nppGUI._enableSmartHilite);
	enableItem(IDC_CHECK_SMARTHILITEANOTHERRVIEW, nppGUI._enableSmartHilite);

	if (NppDarkMode::isEnabled())
	{
		enableItem(IDC_SMARTHILITEMATCHING_STATIC, nppGUI._enableSmartHilite);
	}

	return TRUE;
}

intptr_t HighlightingSubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = nppParam.getNppGUI();

	switch (wParam)
	{
		case IDC_CHECK_MARKALLCASESENSITIVE:
		{
			nppGUI._markAllCaseSensitive = isCheckedOrNot(IDC_CHECK_MARKALLCASESENSITIVE);
			HWND grandParent = mainWindow();
			::SendMessage(grandParent, NPPM_INTERNAL_CLEARINDICATOR, 0, 0);
			return TRUE;
		}

		case IDC_CHECK_MARKALLWHOLEWORDONLY:
		{
			nppGUI._markAllWordOnly = isCheckedOrNot(IDC_CHECK_MARKALLWHOLEWORDONLY);
			HWND grandParent = mainWindow();
			::SendMessage(grandParent, NPPM_INTERNAL_CLEARINDICATOR, 0, 0);
			return TRUE;
		}

		case IDC_CHECK_ENABLSMARTHILITE:
		{
			nppGUI._enableSmartHilite = !nppGUI._enableSmartHilite;
			if (!nppGUI._enableSmartHilite)
			{
				//HWND grandParent = mainWindow();
				//::SendMessage(grandParent, NPPM_INTERNAL_CLEARINDICATOR, 0, 0);
			}
			enableItem(IDC_CHECK_SMARTHILITECASESENSITIVE, nppGUI._enableSmartHilite);
			enableItem(IDC_CHECK_SMARTHILITEWHOLEWORDONLY, nppGUI._enableSmartHilite);
			enableItem(IDC_CHECK_SMARTHILITEUSEFINDSETTINGS, nppGUI._enableSmartHilite);
			enableItem(IDC_CHECK_SMARTHILITEANOTHERRVIEW, nppGUI._enableSmartHilite);

			if (NppDarkMode::isEnabled())
			{
				enableItem(IDC_SMARTHILITEMATCHING_STATIC, nppGUI._enableSmartHilite);
				redrawDlgItem(IDC_SMARTHILITEMATCHING_STATIC);
			}

			HWND grandParent = mainWindow();
			::SendMessage(grandParent, NPPM_INTERNAL_CLEARINDICATOR, 0, 0);
			return TRUE;
		}

		case IDC_CHECK_SMARTHILITECASESENSITIVE:
		{
			nppGUI._smartHiliteCaseSensitive = isCheckedOrNot(IDC_CHECK_SMARTHILITECASESENSITIVE);
			if (nppGUI._smartHiliteCaseSensitive)
			{
				setChecked(IDC_CHECK_SMARTHILITEUSEFINDSETTINGS, false);
				nppGUI._smartHiliteUseFindSettings = false;
			}
			HWND grandParent = mainWindow();
			::SendMessage(grandParent, NPPM_INTERNAL_CLEARINDICATOR, 0, 0);
			return TRUE;
		}

		case IDC_CHECK_SMARTHILITEWHOLEWORDONLY:
		{
			nppGUI._smartHiliteWordOnly = isCheckedOrNot(IDC_CHECK_SMARTHILITEWHOLEWORDONLY);
			if (nppGUI._smartHiliteWordOnly)
			{
				setChecked(IDC_CHECK_SMARTHILITEUSEFINDSETTINGS, false);
				nppGUI._smartHiliteUseFindSettings = false;
			}
			HWND grandParent = mainWindow();
			::SendMessage(grandParent, NPPM_INTERNAL_CLEARINDICATOR, 0, 0);
			return TRUE;
		}

		case IDC_CHECK_SMARTHILITEUSEFINDSETTINGS:
		{
			nppGUI._smartHiliteUseFindSettings = isCheckedOrNot(IDC_CHECK_SMARTHILITEUSEFINDSETTINGS);
			if (nppGUI._smartHiliteUseFindSettings)
			{
				setChecked(IDC_CHECK_SMARTHILITECASESENSITIVE, false);
				setChecked(IDC_CHECK_SMARTHILITEWHOLEWORDONLY, false);
				nppGUI._smartHiliteCaseSensitive = false;
				nppGUI._smartHiliteWordOnly = false;
			}
			HWND grandParent = mainWindow();
			::SendMessage(grandParent, NPPM_INTERNAL_CLEARINDICATOR, 0, 0);
			return TRUE;
		}

		case IDC_CHECK_SMARTHILITEANOTHERRVIEW:
		{
			nppGUI._smartHiliteOnAnotherView = isCheckedOrNot(IDC_CHECK_SMARTHILITEANOTHERRVIEW);

			HWND grandParent = mainWindow();
			::SendMessage(grandParent, NPPM_INTERNAL_CLEARINDICATOR, 0, 0);
			return TRUE;
		}

		case IDC_CHECK_ENABLTAGSMATCHHILITE:
		{
			nppGUI._enableTagsMatchHilite = !nppGUI._enableTagsMatchHilite;
			if (!nppGUI._enableTagsMatchHilite)
			{
				HWND grandParent = mainWindow();
				::SendMessage(grandParent, NPPM_INTERNAL_CLEARINDICATORTAGMATCH, 0, 0);
			}
			enableItem(IDC_CHECK_ENABLTAGATTRHILITE, nppGUI._enableTagsMatchHilite);
			enableItem(IDC_CHECK_HIGHLITENONEHTMLZONE, nppGUI._enableTagsMatchHilite);
			return TRUE;
		}

		case IDC_CHECK_ENABLTAGATTRHILITE:
		{
			nppGUI._enableTagAttrsHilite = !nppGUI._enableTagAttrsHilite;
			if (!nppGUI._enableTagAttrsHilite)
			{
				HWND grandParent = mainWindow();
				::SendMessage(grandParent, NPPM_INTERNAL_CLEARINDICATORTAGATTR, 0, 0);
			}
			return TRUE;
		}

		case IDC_CHECK_HIGHLITENONEHTMLZONE:
		{
			nppGUI._enableHiliteNonHTMLZone = !nppGUI._enableHiliteNonHTMLZone;
			return TRUE;
		}
	}
	return FALSE;
}


intptr_t PrintSubDlg::onInit()
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = nppParam.getNppGUI();

	bool printLN = nppGUI._printSettings._printLineNumber;
	setChecked(IDC_CHECK_PRINTLINENUM, printLN);

	bool printFF = nppGUI._printSettings._printFormFeedPageBreak;
	setChecked(IDC_CHECK_PRINTFORMFEED, printFF);

	int ID2Check = 0;
	switch (nppGUI._printSettings._printOption)
	{
		case SC_PRINT_NORMAL :
			ID2Check = IDC_RADIO_WYSIWYG;
			break;
		case SC_PRINT_INVERTLIGHT :
			ID2Check = IDC_RADIO_INVERT;
			break;
		case SC_PRINT_BLACKONWHITE :
			ID2Check = IDC_RADIO_BW;
			break;
		case SC_PRINT_COLOURONWHITE :
			ID2Check = IDC_RADIO_NOBG;
			break;
	}
	setChecked(ID2Check, true);

	setItemInt(IDC_EDIT_ML, nppGUI._printSettings._marge.left);
	setItemInt(IDC_EDIT_MR, nppGUI._printSettings._marge.right);
	setItemInt(IDC_EDIT_MT, nppGUI._printSettings._marge.top);
	setItemInt(IDC_EDIT_MB, nppGUI._printSettings._marge.bottom);
	setItemText(IDC_EDIT_HLEFT, nppGUI._printSettings._headerLeft.c_str());
	setItemText(IDC_EDIT_HMIDDLE, nppGUI._printSettings._headerMiddle.c_str());
	setItemText(IDC_EDIT_HRIGHT, nppGUI._printSettings._headerRight.c_str());
	setItemText(IDC_EDIT_FLEFT, nppGUI._printSettings._footerLeft.c_str());
	setItemText(IDC_EDIT_FMIDDLE, nppGUI._printSettings._footerMiddle.c_str());
	setItemText(IDC_EDIT_FRIGHT, nppGUI._printSettings._footerRight.c_str());

	for (int i = 6 ; i < 15 ; ++i)
	{
		addComboItem(IDC_COMBO_HFONTSIZE, to_wstring(i));
		addComboItem(IDC_COMBO_FFONTSIZE, to_wstring(i));
	}
	const std::vector<wstring> & fontlist = nppParam.getFontList();
	for (size_t i = 0, len = fontlist.size() ; i < len ; ++i)
	{
		addComboItem(IDC_COMBO_HFONTNAME, fontlist[i]);
		addComboItem(IDC_COMBO_FFONTNAME, fontlist[i]);
	}

	auto index = findComboItem(IDC_COMBO_HFONTNAME, nppGUI._printSettings._headerFontName.c_str());
	if (index == -1)
		index = 0;
	combo(IDC_COMBO_HFONTNAME)->setCurrentIndex(static_cast<int>(index));
	index = findComboItem(IDC_COMBO_FFONTNAME, nppGUI._printSettings._footerFontName.c_str());
	if (index == -1)
		index = 0;
	combo(IDC_COMBO_FFONTNAME)->setCurrentIndex(static_cast<int>(index));

	combo(IDC_COMBO_HFONTSIZE)->setCurrentIndex(findComboItem(IDC_COMBO_HFONTSIZE, to_wstring(nppGUI._printSettings._headerFontSize)));
	combo(IDC_COMBO_FFONTSIZE)->setCurrentIndex(findComboItem(IDC_COMBO_FFONTSIZE, to_wstring(nppGUI._printSettings._footerFontSize)));

	setChecked(IDC_CHECK_HBOLD, nppGUI._printSettings._headerFontStyle & FONTSTYLE_BOLD);
	setChecked(IDC_CHECK_HITALIC, nppGUI._printSettings._headerFontStyle & FONTSTYLE_ITALIC);
	setChecked(IDC_CHECK_FBOLD, nppGUI._printSettings._footerFontStyle & FONTSTYLE_BOLD);
	setChecked(IDC_CHECK_FITALIC, nppGUI._printSettings._footerFontStyle & FONTSTYLE_ITALIC);

	addComboItem(IDC_COMBO_VARLIST, L"Full file name path");
	addComboItem(IDC_COMBO_VARLIST, L"File name");
	addComboItem(IDC_COMBO_VARLIST, L"File directory");
	addComboItem(IDC_COMBO_VARLIST, L"Page");
	addComboItem(IDC_COMBO_VARLIST, L"Short date format");
	addComboItem(IDC_COMBO_VARLIST, L"Long date format");
	addComboItem(IDC_COMBO_VARLIST, L"Time");

	varList.push_back(L"$(FULL_CURRENT_PATH)");
	varList.push_back(L"$(FILE_NAME)");
	varList.push_back(L"$(CURRENT_DIRECTORY)");
	varList.push_back(L"$(CURRENT_PRINTING_PAGE)");
	varList.push_back(L"$(SHORT_DATE)");
	varList.push_back(L"$(LONG_DATE)");
	varList.push_back(L"$(TIME)");

	combo(IDC_COMBO_VARLIST)->setCurrentIndex(static_cast<int>(0));

	return TRUE;
}

intptr_t PrintSubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = nppParam.getNppGUI();

	if (HIWORD(wParam) == EN_CHANGE && LOWORD(wParam) != IDC_EDIT_VIEWPANEL)
	{
		switch (LOWORD(wParam))
		{
			case  IDC_EDIT_ML:
				nppGUI._printSettings._marge.left = getItemInt(IDC_EDIT_ML);
				return TRUE;

			case  IDC_EDIT_MR:
				nppGUI._printSettings._marge.right = getItemInt(IDC_EDIT_MR);
				return TRUE;

			case IDC_EDIT_MT :
				nppGUI._printSettings._marge.top = getItemInt(IDC_EDIT_MT);
				return TRUE;

			case IDC_EDIT_MB :
				nppGUI._printSettings._marge.bottom = getItemInt(IDC_EDIT_MB);
				return TRUE;

			default:
			{
				constexpr int stringSize = 256;
				wchar_t str[stringSize]{};
				_focusedEditCtrl = LOWORD(wParam);
				copyItemText(_focusedEditCtrl, str, stringSize);
				setItemText(IDC_EDIT_VIEWPANEL, str);
				switch (LOWORD(wParam))
				{
					case  IDC_EDIT_HLEFT:
						nppGUI._printSettings._headerLeft = str;
						trim(nppGUI._printSettings._headerLeft);
						return TRUE;

					case  IDC_EDIT_HMIDDLE:
						nppGUI._printSettings._headerMiddle = str;
						trim(nppGUI._printSettings._headerMiddle);
						return TRUE;

					case IDC_EDIT_HRIGHT :
						nppGUI._printSettings._headerRight = str;
						trim(nppGUI._printSettings._headerRight);
						return TRUE;

					case  IDC_EDIT_FLEFT:
						nppGUI._printSettings._footerLeft = str;
						trim(nppGUI._printSettings._footerLeft);
						return TRUE;

					case  IDC_EDIT_FMIDDLE:
						nppGUI._printSettings._footerMiddle = str;
						trim(nppGUI._printSettings._footerMiddle);
						return TRUE;

					case IDC_EDIT_FRIGHT :
						nppGUI._printSettings._footerRight = str;
						trim(nppGUI._printSettings._footerRight);
						return TRUE;

					default :
						return FALSE;
				}
			}
		}
	}
	else if (HIWORD(wParam) == EN_SETFOCUS && LOWORD(wParam) != IDC_EDIT_VIEWPANEL)
	{
		constexpr int stringSize = 256;
		wchar_t str[stringSize]{};
		_focusedEditCtrl = LOWORD(wParam);
		
		int focusedEditStatic = 0;
		int groupStatic = 0;
		switch (_focusedEditCtrl)
		{
			case IDC_EDIT_HLEFT : focusedEditStatic = IDC_HL_STATIC; groupStatic = IDC_HGB_STATIC; break;
			case IDC_EDIT_HMIDDLE : focusedEditStatic = IDC_HM_STATIC; groupStatic = IDC_HGB_STATIC; break;
			case IDC_EDIT_HRIGHT : focusedEditStatic = IDC_HR_STATIC; groupStatic = IDC_HGB_STATIC; break;
			case IDC_EDIT_FLEFT : focusedEditStatic = IDC_FL_STATIC; groupStatic = IDC_FGB_STATIC; break;
			case IDC_EDIT_FMIDDLE : focusedEditStatic = IDC_FM_STATIC; groupStatic = IDC_FGB_STATIC; break;
			case IDC_EDIT_FRIGHT : focusedEditStatic = IDC_FR_STATIC; groupStatic = IDC_FGB_STATIC; break;
			default : return TRUE;
		}
		copyItemText(_focusedEditCtrl, str, stringSize);
		setItemText(IDC_EDIT_VIEWPANEL, str);
		copyItemText(groupStatic, str, stringSize);
		wstring title = str;
		title += L" ";
		copyItemText(focusedEditStatic, str, stringSize);
		title += str;
		title = purgeMenuItemString(title.c_str()); // use purgeMenuItemString to clean '&'
		setItemText(IDC_WHICHPART_STATIC, title.c_str());
		redrawDlgItem(IDC_WHICHPART_STATIC);
		return TRUE;
	}
	else if (HIWORD(wParam) == CBN_SELCHANGE)
	{
		auto iSel = combo(LOWORD(wParam))->currentIndex();

		switch (LOWORD(wParam))
		{
			case IDC_COMBO_HFONTNAME :
			case IDC_COMBO_FFONTNAME :
			{
				const wstring fnStr = comboText(LOWORD(wParam), static_cast<int>(iSel));
				if (LOWORD(wParam) == IDC_COMBO_HFONTNAME)
					nppGUI._printSettings._headerFontName = fnStr;
				else
					nppGUI._printSettings._footerFontName = fnStr;
			}
			break;


			case IDC_COMBO_HFONTSIZE :
			case IDC_COMBO_FFONTSIZE :
			{
				int *pVal = (LOWORD(wParam) == IDC_COMBO_HFONTSIZE)?&(nppGUI._printSettings._headerFontSize):&(nppGUI._printSettings._footerFontSize);

				// The first item is empty, for no size
				*pVal = combo(LOWORD(wParam))->itemText(static_cast<int>(iSel)).toInt();
			}
			break;

			case IDC_COMBO_VARLIST:
			{
				break;
			}

			default:
			{
				break;
			}
		}
		return TRUE;
	
	}

	switch (wParam)
	{
		case IDC_CHECK_PRINTLINENUM:
			nppGUI._printSettings._printLineNumber = isCheckedOrNot(IDC_CHECK_PRINTLINENUM);
			break;

		case  IDC_CHECK_PRINTFORMFEED:
			nppGUI._printSettings._printFormFeedPageBreak = isCheckedOrNot(IDC_CHECK_PRINTFORMFEED);
			break;

		case  IDC_RADIO_WYSIWYG:
			nppGUI._printSettings._printOption = SC_PRINT_NORMAL;
			break;

		case  IDC_RADIO_INVERT:
			nppGUI._printSettings._printOption = SC_PRINT_INVERTLIGHT;
			break;

		case IDC_RADIO_BW :
			nppGUI._printSettings._printOption = SC_PRINT_BLACKONWHITE;
			break;

		case IDC_RADIO_NOBG :
			nppGUI._printSettings._printOption = SC_PRINT_COLOURONWHITE;
			break;
		case IDC_CHECK_HBOLD:
			nppGUI._printSettings._headerFontStyle ^= FONTSTYLE_BOLD;
			break;

		case  IDC_CHECK_HITALIC:
			nppGUI._printSettings._headerFontStyle ^= FONTSTYLE_ITALIC;
			break;

		case  IDC_CHECK_FBOLD:
			nppGUI._printSettings._footerFontStyle ^= FONTSTYLE_BOLD;
			break;

		case  IDC_CHECK_FITALIC:
			nppGUI._printSettings._footerFontStyle ^= FONTSTYLE_ITALIC;
			break;

		case  IDC_BUTTON_ADDVAR:
		{
			try {
				if (!_focusedEditCtrl)
					return TRUE;

				size_t iSel = combo(IDC_COMBO_VARLIST)->currentIndex();
				if (iSel >= varList.size())
					return TRUE;

				const wchar_t* varStr = varList[iSel].c_str();
				const auto* focused = qobject_cast<QLineEdit*>(dlgItem(_focusedEditCtrl));
				size_t selStart = focused->hasSelectedText() ? focused->selectionStart() : focused->cursorPosition();
				size_t selEnd = selStart + focused->selectionLength();

				wstring str2Set = getItemText(_focusedEditCtrl);
				size_t strLen = str2Set.length();
				if (selStart > strLen || selEnd > strLen)
					selStart = selEnd = strLen;

				str2Set.replace(selStart, selEnd - selStart, varStr);

				setItemText(_focusedEditCtrl, str2Set.c_str());
			}
			catch (...)
			{
				// Do nothing
			}
		}
		break;
	}
	return TRUE;
}



intptr_t BackupSubDlg::onInit()
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI & nppGUI = nppParam.getNppGUI();
	setChecked(IDC_CHECK_REMEMBERSESSION, nppGUI._rememberLastSession);
	bool snapshotCheck = nppGUI._rememberLastSession && nppGUI.isSnapshotMode();
	setChecked(IDC_BACKUPDIR_RESTORESESSION_CHECK, snapshotCheck);
	auto periodicBackupInSec = static_cast<UINT>(nppGUI._snapshotBackupTiming / 1000);
	setItemInt(IDC_BACKUPDIR_RESTORESESSION_EDIT, periodicBackupInSec);
	wstring backupFilePath = NppParameters::getInstance().getUserPath();
	pathAppend(backupFilePath, L"backup");
	backupFilePath += nppPathSepStr;
	setItemText(IDD_BACKUPDIR_RESTORESESSION_PATH_EDIT, backupFilePath.c_str());
	setChecked(IDC_CHECK_KEEPABSENTFILESINSESSION, nppGUI._keepSessionAbsentFileEntries);

	int ID2CheckBackupOnSave = 0;

	switch (nppGUI._backup)
	{
		case bak_simple :
			ID2CheckBackupOnSave = IDC_RADIO_BKSIMPLE;
			break;
		case bak_verbose :
			ID2CheckBackupOnSave = IDC_RADIO_BKVERBOSE;
			break;
		
		default : //bak_none
			ID2CheckBackupOnSave = IDC_RADIO_BKNONE;
	}
	setChecked(ID2CheckBackupOnSave, true);

	if (nppGUI._useDir)
		setChecked(IDC_BACKUPDIR_CHECK, true);

	setItemText(IDC_BACKUPDIR_EDIT, (nppGUI._backupDir.c_str()));

	updateBackupSessionGUI();
	updateBackupOnSaveGUI();

	return TRUE;
}

intptr_t BackupSubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI & nppGUI = nppParam.getNppGUI();
	if (HIWORD(wParam) == EN_CHANGE)
	{
		switch (LOWORD(wParam))
		{
			case  IDC_BACKUPDIR_EDIT:
			{
				const wstring inputDir = getItemText(IDC_BACKUPDIR_EDIT);
				nppGUI._backupDir = inputDir;
				return TRUE;
			}

			case IDC_BACKUPDIR_RESTORESESSION_EDIT:
			{
				
				constexpr int stringSize = 16;
				wchar_t str[stringSize]{};

				copyItemText(IDC_BACKUPDIR_RESTORESESSION_EDIT, str, stringSize);

				if (wcscmp(str, L"") == 0)
					return TRUE;

				nppGUI._snapshotBackupTiming = static_cast<size_t>(getItemInt(IDC_BACKUPDIR_RESTORESESSION_EDIT)) * 1000;
				if (!nppGUI._snapshotBackupTiming)
				{
					nppGUI._snapshotBackupTiming = 1000;
					setItemInt(IDC_BACKUPDIR_RESTORESESSION_EDIT, 1);
				}
				return TRUE;
			}
		}
	}
	else if (HIWORD(wParam) == EN_KILLFOCUS)
	{
		switch (LOWORD(wParam))
		{
			case  IDC_BACKUPDIR_RESTORESESSION_EDIT:
			{
				//printStr(L"");
				static constexpr int stringSize = 16;
				wchar_t str[stringSize]{};

				copyItemText(IDC_BACKUPDIR_RESTORESESSION_EDIT, str, stringSize);

				if (wcscmp(str, L"") == 0)
				{
					setItemInt(IDC_BACKUPDIR_RESTORESESSION_EDIT, static_cast<UINT>(nppGUI._snapshotBackupTiming / 1000));
				}
			}
		}
	}

	switch (wParam)
	{
		case IDC_CHECK_REMEMBERSESSION:
		{
			nppGUI._rememberLastSession = isCheckedOrNot(IDC_CHECK_REMEMBERSESSION);
			if (!nppGUI._rememberLastSession)
			{
				setChecked(IDC_BACKUPDIR_RESTORESESSION_CHECK, false);
				onCommand(IDC_BACKUPDIR_RESTORESESSION_CHECK, 0);
			}
			updateBackupSessionGUI();
			return TRUE;
		}

		case IDC_CHECK_KEEPABSENTFILESINSESSION:
		{
			nppGUI._keepSessionAbsentFileEntries = isCheckedOrNot(IDC_CHECK_KEEPABSENTFILESINSESSION);
			return TRUE;
		}

		case IDC_BACKUPDIR_RESTORESESSION_CHECK:
		{
			nppGUI._isSnapshotMode = isCheckedOrNot(IDC_BACKUPDIR_RESTORESESSION_CHECK);
			updateBackupSessionGUI();

			if (nppGUI._isSnapshotMode)
			{
				// Launch thread
				::SendMessage(mainWindow(), NPPM_INTERNAL_ENABLESNAPSHOT, 0, 0);
			}
			return TRUE;
		}

		case IDC_RADIO_BKSIMPLE:
		{
			nppGUI._backup = bak_simple;
			updateBackupOnSaveGUI();
			return TRUE;
		}

		case IDC_RADIO_BKVERBOSE:
		{
			nppGUI._backup = bak_verbose;
			updateBackupOnSaveGUI();
			return TRUE;
		}

		case IDC_RADIO_BKNONE:
		{
			nppGUI._backup = bak_none;
			updateBackupOnSaveGUI();
			return TRUE;
		}

		case IDC_BACKUPDIR_CHECK:
		{
			nppGUI._useDir = !nppGUI._useDir;
			updateBackupOnSaveGUI();
			return TRUE;
		}
		case IDD_BACKUPDIR_BROWSE_BUTTON :
		{
			wstring title = nppParam.getNativeLangSpeaker()->getLocalizedStrFromID("backup-select-folder",
				L"Select a folder as backup directory");
			folderBrowser(_hSelf, title, qobject_cast<QLineEdit*>(dlgItem(IDC_BACKUPDIR_EDIT)));
			return TRUE;
		}


		default :
			return FALSE;
	}
	
	return FALSE;
}


void BackupSubDlg::updateBackupSessionGUI()
{
	bool rememberSession = isCheckedOrNot(IDC_CHECK_REMEMBERSESSION);
	bool isSnapshot = isCheckedOrNot(IDC_BACKUPDIR_RESTORESESSION_CHECK);
	enableItem(IDC_BACKUPDIR_RESTORESESSION_CHECK, rememberSession);
	enableItem(IDC_BACKUPDIR_RESTORESESSION_EDIT, isSnapshot);
	enableItem(IDD_BACKUPDIR_RESTORESESSION_PATH_EDIT, isSnapshot);

	redrawDlgItem(IDD_BACKUPDIR_RESTORESESSION_STATIC1);
	redrawDlgItem(IDD_BACKUPDIR_RESTORESESSION_STATIC2);
	redrawDlgItem(IDD_BACKUPDIR_RESTORESESSION_PATHLABEL_STATIC);
}

void BackupSubDlg::updateBackupOnSaveGUI()
{
	bool noBackup = isCheckedOrNot(IDC_RADIO_BKNONE);
	bool isEnableGlobableCheck = false;
	bool isEnableLocalCheck = false;

	if (!noBackup)
	{
		isEnableGlobableCheck = true;
		isEnableLocalCheck = isCheckedOrNot(IDC_BACKUPDIR_CHECK);
	}
	enableItem(IDC_BACKUPDIR_CHECK, isEnableGlobableCheck);

	enableItem(IDC_BACKUPDIR_EDIT, isEnableLocalCheck);
	enableItem(IDD_BACKUPDIR_BROWSE_BUTTON, isEnableLocalCheck);

	// redrawDlgItem(IDD_BACKUPDIR_STATIC); // not needed, IDD_BACKUPDIR_STATIC rect is inside IDC_BACKUPDIR_USERCUSTOMDIR_GRPSTATIC rect

	if (NppDarkMode::isEnabled())
	{
		enableItem(IDC_BACKUPDIR_USERCUSTOMDIR_GRPSTATIC, !noBackup);
		redrawDlgItem(IDC_BACKUPDIR_USERCUSTOMDIR_GRPSTATIC);
	}
}


intptr_t AutoCompletionSubDlg::onInit()
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI & nppGUI = nppParam.getNppGUI();
	setItemInt(IDD_AUTOC_STATIC_N, nppGUI._autocFromLen);

	slider(IDC_AUTOC_CHAR_SLIDER)->setMinimum(AUTOCOMPLETEFROMCHAR_SMALLEST);
	slider(IDC_AUTOC_CHAR_SLIDER)->setMaximum(AUTOCOMPLETEFROMCHAR_LARGEST);
	slider(IDC_AUTOC_CHAR_SLIDER)->setPageStep(AUTOCOMPLETEFROMCHAR_INTERVAL);
	setSliderPos(IDC_AUTOC_CHAR_SLIDER, nppGUI._autocFromLen);

	bool isEnableAutoC = nppGUI._autocStatus != nppGUI.autoc_none;

	setChecked(IDD_AUTOC_ENABLECHECK, isEnableAutoC);
	
	int selectedID = IDD_AUTOC_BOTHRADIO;
	if (nppGUI._autocStatus == nppGUI.autoc_func)
		selectedID = IDD_AUTOC_FUNCRADIO;
	else if (nppGUI._autocStatus == nppGUI.autoc_word)
		selectedID = IDD_AUTOC_WORDRADIO;
	else if (nppGUI._autocStatus == nppGUI.autoc_both)
		selectedID = IDD_AUTOC_BOTHRADIO;
	
	setChecked(selectedID, true);

	if (nppGUI._autocStatus == nppGUI.autoc_word || nppGUI._autocStatus == nppGUI.autoc_both)
		setChecked(IDD_AUTOC_IGNORENUMBERS, nppGUI._autocIgnoreNumbers);

	setChecked(IDD_AUTOC_USEENTER, nppGUI._autocInsertSelectedUseENTER);
	setChecked(IDD_AUTOC_USETAB, nppGUI._autocInsertSelectedUseTAB);


	if (!isEnableAutoC)
	{
		enableItem(IDD_AUTOC_FUNCRADIO, FALSE);
		enableItem(IDD_AUTOC_WORDRADIO, FALSE);
		enableItem(IDD_AUTOC_BRIEF_CHECK, FALSE);
		enableItem(IDD_AUTOC_BOTHRADIO, FALSE);
		enableItem(IDD_AUTOC_USEENTER, FALSE);
		enableItem(IDD_AUTOC_USETAB, FALSE);
		enableItem(IDD_AUTOC_IGNORENUMBERS, FALSE);
		enableItem(IDC_AUTOC_CHAR_SLIDER, FALSE);

		if (NppDarkMode::isEnabled())
		{
			enableItem(IDD_AUTOC_USEKEY_GRP_STATIC, FALSE);
		}
	}

	setChecked(IDD_AUTOC_BRIEF_CHECK, nppGUI._autocBrief);
	setChecked(IDD_FUNC_CHECK, nppGUI._funcParams);

	setChecked(IDD_AUTOCPARENTHESES_CHECK, nppGUI._matchedPairConf._doParentheses);
	if (nppGUI._matchedPairConf._doParentheses)
		setItemText(IDD_AUTOCPARENTHESES_CHECK, L" (  )");

	setChecked(IDD_AUTOCBRACKET_CHECK, nppGUI._matchedPairConf._doBrackets);
	if (nppGUI._matchedPairConf._doBrackets)
		setItemText(IDD_AUTOCBRACKET_CHECK, L" [  ]");

	setChecked(IDD_AUTOCCURLYBRACKET_CHECK, nppGUI._matchedPairConf._doCurlyBrackets);
	if (nppGUI._matchedPairConf._doCurlyBrackets)
		setItemText(IDD_AUTOCCURLYBRACKET_CHECK, L" {  }");
	
	setChecked(IDD_AUTOC_QUOTESCHECK, nppGUI._matchedPairConf._doQuotes);
	if (nppGUI._matchedPairConf._doQuotes)
		setItemText(IDD_AUTOC_QUOTESCHECK, L" '  '");
	
	setChecked(IDD_AUTOC_DOUBLEQUOTESCHECK, nppGUI._matchedPairConf._doDoubleQuotes);
	if (nppGUI._matchedPairConf._doDoubleQuotes)
		setItemText(IDD_AUTOC_DOUBLEQUOTESCHECK, L" \"  \"");
	
	setChecked(IDD_AUTOCTAG_CHECK, nppGUI._matchedPairConf._doHtmlXmlTag);

	limitText(IDC_MACHEDPAIROPEN_EDIT1, static_cast<int>(1));
	limitText(IDC_MACHEDPAIRCLOSE_EDIT1, static_cast<int>(1));
	limitText(IDC_MACHEDPAIROPEN_EDIT2, static_cast<int>(1));
	limitText(IDC_MACHEDPAIRCLOSE_EDIT2, static_cast<int>(1));
	limitText(IDC_MACHEDPAIROPEN_EDIT3, static_cast<int>(1));
	limitText(IDC_MACHEDPAIRCLOSE_EDIT3, static_cast<int>(1));

	size_t nbMatchedPair = nppGUI._matchedPairConf._matchedPairs.size();
	if (nbMatchedPair > 3)
		nbMatchedPair = 3;
	for (size_t i = 0; i < nbMatchedPair; ++i)
	{
		wchar_t openChar[2]{};
		openChar[0] = nppGUI._matchedPairConf._matchedPairs[i].first;
		openChar[1] = '\0';
		wchar_t closeChar[2]{};
		closeChar[0] = nppGUI._matchedPairConf._matchedPairs[i].second;
		closeChar[1] = '\0';

		if (i == 0)
		{
			setItemText(IDC_MACHEDPAIROPEN_EDIT1, openChar);
			setItemText(IDC_MACHEDPAIRCLOSE_EDIT1, closeChar);
		}
		else if (i == 1)
		{
			setItemText(IDC_MACHEDPAIROPEN_EDIT2, openChar);
			setItemText(IDC_MACHEDPAIRCLOSE_EDIT2, closeChar);
		}
		if (i == 2)
		{
			setItemText(IDC_MACHEDPAIROPEN_EDIT3, openChar);
			setItemText(IDC_MACHEDPAIRCLOSE_EDIT3, closeChar);
		}
	}


	return TRUE;
}

void AutoCompletionSubDlg::onScroll(HWND scale)
{
	NppGUI& nppGUI = NppParameters::getInstance().getNppGUI();
	if (scale == dlgItem(IDC_AUTOC_CHAR_SLIDER))
	{
		nppGUI._autocFromLen = static_cast<UINT>(slider(IDC_AUTOC_CHAR_SLIDER)->value());
		setItemInt(IDD_AUTOC_STATIC_N, nppGUI._autocFromLen);
	}
}

intptr_t AutoCompletionSubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI & nppGUI = nppParam.getNppGUI();
	if (HIWORD(wParam) == EN_CHANGE)
	{
		switch (LOWORD(wParam))
		{
			case IDC_MACHEDPAIROPEN_EDIT1 :
			case IDC_MACHEDPAIRCLOSE_EDIT1:
			case IDC_MACHEDPAIROPEN_EDIT2 :
			case IDC_MACHEDPAIRCLOSE_EDIT2:
			case IDC_MACHEDPAIROPEN_EDIT3 :
			case IDC_MACHEDPAIRCLOSE_EDIT3:
			{
				nppGUI._matchedPairConf._matchedPairs.clear();

				wchar_t opener[2] = {'\0', '\0'};
				wchar_t closer[2] = {'\0', '\0'};

				copyItemText(IDC_MACHEDPAIROPEN_EDIT1, opener, MAX_PATH);
				copyItemText(IDC_MACHEDPAIRCLOSE_EDIT1, closer, MAX_PATH);
				if (opener[0] < 0x80 && opener[0] != '\0' && closer[0] < 0x80 && closer[0] != '\0')
					nppGUI._matchedPairConf._matchedPairs.push_back(pair<char, char>(static_cast<char>(opener[0]), static_cast<char>(closer[0])));

				copyItemText(IDC_MACHEDPAIROPEN_EDIT2, opener, MAX_PATH);
				copyItemText(IDC_MACHEDPAIRCLOSE_EDIT2, closer, MAX_PATH);
				if (opener[0] < 0x80 && opener[0] != '\0' && closer[0] < 0x80 && closer[0] != '\0')
					nppGUI._matchedPairConf._matchedPairs.push_back(pair<char, char>(static_cast<char>(opener[0]), static_cast<char>(closer[0])));

				copyItemText(IDC_MACHEDPAIROPEN_EDIT3, opener, MAX_PATH);
				copyItemText(IDC_MACHEDPAIRCLOSE_EDIT3, closer, MAX_PATH);
				if (opener[0] < 0x80 && opener[0] != '\0' && closer[0] < 0x80 && closer[0] != '\0')
					nppGUI._matchedPairConf._matchedPairs.push_back(pair<char, char>(static_cast<char>(opener[0]), static_cast<char>(closer[0])));
				 
				return TRUE;
			}
		}
	}

	switch (wParam)
	{
		case IDD_AUTOC_ENABLECHECK :
		{
			bool isEnableAutoC = isCheckedOrNot(IDD_AUTOC_ENABLECHECK);

			if (isEnableAutoC)
			{
				setChecked(IDD_AUTOC_BOTHRADIO, true);
				nppGUI._autocStatus = nppGUI.autoc_both;

				setChecked(IDD_AUTOC_IGNORENUMBERS, false);
				nppGUI._autocIgnoreNumbers = false;
			}
			else 
			{
				setChecked(IDD_AUTOC_FUNCRADIO, false);
				setChecked(IDD_AUTOC_WORDRADIO, false);
				setChecked(IDD_AUTOC_BOTHRADIO, false);
				setChecked(IDD_AUTOC_BRIEF_CHECK, false);
				nppGUI._autocBrief = false;
				nppGUI._autocStatus = nppGUI.autoc_none;

				setChecked(IDD_AUTOC_IGNORENUMBERS, false);
				nppGUI._autocIgnoreNumbers = false;
			}
			enableItem(IDD_AUTOC_FUNCRADIO, isEnableAutoC);
			enableItem(IDD_AUTOC_WORDRADIO, isEnableAutoC);
			enableItem(IDD_AUTOC_BOTHRADIO, isEnableAutoC);
			enableItem(IDD_AUTOC_BRIEF_CHECK, isEnableAutoC);
			enableItem(IDD_AUTOC_USEKEY_GRP_STATIC, isEnableAutoC);
			enableItem(IDD_AUTOC_USEENTER, isEnableAutoC);
			enableItem(IDD_AUTOC_USETAB, isEnableAutoC);
			enableItem(IDD_AUTOC_IGNORENUMBERS, isEnableAutoC);
			enableItem(IDC_AUTOC_CHAR_SLIDER, isEnableAutoC);

			redrawDlgItem(IDD_AUTOC_STATIC_FROM);
			redrawDlgItem(IDD_AUTOC_STATIC_N);
			redrawDlgItem(IDD_AUTOC_STATIC_CHAR);
			redrawDlgItem(IDD_AUTOC_SLIDER_MIN_STATIC);
			redrawDlgItem(IDD_AUTOC_SLIDER_MAX_STATIC);

			if (NppDarkMode::isEnabled())
			{
				enableItem(IDD_AUTOC_USEKEY_GRP_STATIC, isEnableAutoC);
				redrawDlgItem(IDD_AUTOC_USEKEY_GRP_STATIC);
			}

			return TRUE;
		}

		case IDD_AUTOC_FUNCRADIO :
		{
			nppGUI._autocStatus = nppGUI.autoc_func;

			setChecked(IDD_AUTOC_IGNORENUMBERS, false);
			enableItem(IDD_AUTOC_IGNORENUMBERS, FALSE);
			nppGUI._autocIgnoreNumbers = false;
			setChecked(IDD_AUTOC_BRIEF_CHECK, false);
			nppGUI._autocBrief = false;
			return TRUE;
		}

		case IDD_AUTOC_WORDRADIO :
		{
			nppGUI._autocStatus = nppGUI.autoc_word;
			enableItem(IDD_AUTOC_IGNORENUMBERS, TRUE);
			return TRUE;
		}

		case IDD_AUTOC_BOTHRADIO :
		{
			nppGUI._autocStatus = nppGUI.autoc_both;
			enableItem(IDD_AUTOC_IGNORENUMBERS, TRUE);
			return TRUE;
		}

		case IDD_AUTOC_USEENTER:
		{
			nppGUI._autocInsertSelectedUseENTER = isCheckedOrNot(static_cast<int>(wParam));
			return TRUE;
		}

		case IDD_AUTOC_USETAB:
		{
			nppGUI._autocInsertSelectedUseTAB = isCheckedOrNot(static_cast<int>(wParam));
			return TRUE;
		}

		case IDD_AUTOC_IGNORENUMBERS:
		{
			nppGUI._autocIgnoreNumbers = isCheckedOrNot(static_cast<int>(wParam));
			return TRUE;
		}

		case IDD_AUTOC_BRIEF_CHECK :
		{
			nppGUI._autocBrief = isCheckedOrNot(static_cast<int>(wParam));
			return TRUE;
		}

		case IDD_FUNC_CHECK :
		{
			nppGUI._funcParams = isCheckedOrNot(static_cast<int>(wParam));
			return TRUE;
		}

		case IDD_AUTOCPARENTHESES_CHECK :
		case IDD_AUTOCBRACKET_CHECK :
		case IDD_AUTOCCURLYBRACKET_CHECK :
		case IDD_AUTOC_DOUBLEQUOTESCHECK :
		case IDD_AUTOC_QUOTESCHECK :
		{
			const bool isChecked = isCheckedOrNot(static_cast<int>(wParam));
			const wchar_t *label = nullptr;
			if (wParam == IDD_AUTOCPARENTHESES_CHECK)
			{
				nppGUI._matchedPairConf._doParentheses = isChecked;
				label = isChecked ? L" (  )" : L" (";
			}
			else if (wParam == IDD_AUTOCBRACKET_CHECK)
			{
				nppGUI._matchedPairConf._doBrackets = isChecked;
				label = isChecked ? L" [  ]" : L" [";
			}
			else if (wParam == IDD_AUTOCCURLYBRACKET_CHECK)
			{
				nppGUI._matchedPairConf._doCurlyBrackets = isChecked;
				label = isChecked ? L" {  }" : L" {";
			}
			else if (wParam == IDD_AUTOC_DOUBLEQUOTESCHECK)
			{
				nppGUI._matchedPairConf._doDoubleQuotes = isChecked;
				label = isChecked ? L" \"  \"" : L" \"";
			}
			else // if (wParam == IDD_AUTOC_QUOTESCHECK)
			{
				nppGUI._matchedPairConf._doQuotes = isChecked;
				label = isChecked ? L" '  '" : L" '";
			}
			setItemText(static_cast<int>(wParam), label);
			return TRUE;
		}

		case IDD_AUTOCTAG_CHECK :
		{
			nppGUI._matchedPairConf._doHtmlXmlTag = isCheckedOrNot(IDD_AUTOCTAG_CHECK);
			return TRUE;
		}
		default :
			return FALSE;
	}
	
	return FALSE;
}



intptr_t MultiInstanceSubDlg::onInit()
{
	NppGUI & nppGUI = (NppParameters::getInstance()).getNppGUI();
	auto checkOrUncheckBtn = [this](int id, bool check) -> void
	{
		setChecked(id, check);
	};

	checkOrUncheckBtn(IDC_CHECK_CLIPBOARDHISTORY, nppGUI._clipboardHistoryPanelKeepState);
	checkOrUncheckBtn(IDC_CHECK_DOCLIST, nppGUI._docListKeepState);
	checkOrUncheckBtn(IDC_CHECK_CHARPANEL, nppGUI._charPanelKeepState);
	checkOrUncheckBtn(IDC_CHECK_FILEBROWSER, nppGUI._fileBrowserKeepState);
	checkOrUncheckBtn(IDC_CHECK_PROJECTPANEL, nppGUI._projectPanelKeepState);
	checkOrUncheckBtn(IDC_CHECK_DOCMAP, nppGUI._docMapKeepState);
	checkOrUncheckBtn(IDC_CHECK_FUNCLIST, nppGUI._funcListKeepState);
	checkOrUncheckBtn(IDC_CHECK_PLUGINPANEL, nppGUI._pluginPanelKeepState);

	MultiInstSetting multiInstSetting = nppGUI._multiInstSetting;

	setChecked(IDC_SESSIONININST_RADIO, multiInstSetting == multiInstOnSession);
	setChecked(IDC_MULTIINST_RADIO, multiInstSetting == multiInst);
	setChecked(IDC_MONOINST_RADIO, multiInstSetting == monoInst);

	setChecked(IDD_DATETIMEFORMAT_REVERSEORDER_CHECK, nppGUI._dateTimeReverseDefaultOrder);

	setItemText(IDC_DATETIMEFORMAT_EDIT, nppGUI._dateTimeFormat.c_str());
	wstring datetimeStr = getDateTimeStrFrom(nppGUI._dateTimeFormat, _BTTF_time);
	setItemText(IDD_DATETIMEFORMAT_RESULT_STATIC, datetimeStr.c_str());

	return TRUE;
}

intptr_t MultiInstanceSubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppGUI & nppGUI = (NppParameters::getInstance()).getNppGUI();
	if (HIWORD(wParam) == EN_CHANGE && LOWORD(wParam) == IDC_DATETIMEFORMAT_EDIT)
	{
		static constexpr size_t inputLen = 256;
		wchar_t input[inputLen]{};
		copyItemText(IDC_DATETIMEFORMAT_EDIT, input, inputLen);

		nppGUI._dateTimeFormat = input;
		wstring datetimeStr = getDateTimeStrFrom(nppGUI._dateTimeFormat, _BTTF_time);
		setItemText(IDD_DATETIMEFORMAT_RESULT_STATIC, datetimeStr.c_str());
		return TRUE;
	}

	switch (wParam)
	{
		case IDC_SESSIONININST_RADIO :
		{
			nppGUI._multiInstSetting = multiInstOnSession;
		}
		break;

		case IDC_MULTIINST_RADIO :
		{
			nppGUI._multiInstSetting = multiInst;
		}
		break;

		case IDC_MONOINST_RADIO :
		{
			nppGUI._multiInstSetting = monoInst;
		}
		break;

		case IDD_DATETIMEFORMAT_REVERSEORDER_CHECK:
		{
			nppGUI._dateTimeReverseDefaultOrder = isCheckedOrNot(IDD_DATETIMEFORMAT_REVERSEORDER_CHECK);
		}
		break;

		case IDC_CHECK_CLIPBOARDHISTORY:
		{
			nppGUI._clipboardHistoryPanelKeepState = isCheckedOrNot(IDC_CHECK_CLIPBOARDHISTORY);
		}
		break;

		case IDC_CHECK_DOCLIST:
		{
			nppGUI._docListKeepState = isCheckedOrNot(IDC_CHECK_DOCLIST);
		}
		break;

		case IDC_CHECK_CHARPANEL:
		{
			nppGUI._charPanelKeepState = isCheckedOrNot(IDC_CHECK_CHARPANEL);
		}
		break;

		case IDC_CHECK_FILEBROWSER:
		{
			nppGUI._fileBrowserKeepState = isCheckedOrNot(IDC_CHECK_FILEBROWSER);
		}
		break;

		case IDC_CHECK_PROJECTPANEL:
		{
			nppGUI._projectPanelKeepState = isCheckedOrNot(IDC_CHECK_PROJECTPANEL);
		}
		break;

		case IDC_CHECK_DOCMAP:
		{
			nppGUI._docMapKeepState = isCheckedOrNot(IDC_CHECK_DOCMAP);
		}
		break;

		case IDC_CHECK_FUNCLIST:
		{
			nppGUI._funcListKeepState = isCheckedOrNot(IDC_CHECK_FUNCLIST);
		}
		break;

		case IDC_CHECK_PLUGINPANEL:
		{
			nppGUI._pluginPanelKeepState = isCheckedOrNot(IDC_CHECK_PLUGINPANEL);
		}
		break;

		default :
			return FALSE;
	}
	return FALSE;
}


void DelimiterSubDlg::detectSpace(const char *text2Check, int & nbSp, int & nbTab) const
{
	nbSp = nbTab = 0;
	for (size_t i = 0; i < strlen(text2Check); ++i)
	{
		if (text2Check[i] == ' ')
			++nbSp;
		else if (text2Check[i] == '\t')
			++nbTab;
	}
}


wstring DelimiterSubDlg::getWarningText(size_t nbSp, size_t nbTab) const
{
	NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();

	wstring msg;
	if (nbSp && nbTab)
	{
		wstring nbSpStr = std::to_wstring(nbSp);
		wstring nbTabStr = std::to_wstring(nbTab);
		wstring warnBegin = pNativeSpeaker->getLocalizedStrFromID("word-chars-list-warning-begin", L"");
		wstring space = pNativeSpeaker->getLocalizedStrFromID("word-chars-list-space-warning", L"");
		wstring tab = pNativeSpeaker->getLocalizedStrFromID("word-chars-list-tab-warning", L"");
		wstring warnEnd = pNativeSpeaker->getLocalizedStrFromID("word-chars-list-warning-end", L"");

		// half translation is not allowed
		if (!warnBegin.empty() && !space.empty() && !tab.empty() && !warnEnd.empty())
		{
			space = stringReplace(space, L"$INT_REPLACE$", nbSpStr);
			tab = stringReplace(tab, L"$INT_REPLACE$", nbTabStr);
			msg = warnBegin;
			msg += space;
			msg += L" && ";
			msg += tab;
			msg += warnEnd;
		}
		else
		{
			msg = L"Be aware: ";
			msg += nbSpStr;
			msg += L" space(s) && ";
			msg += std::to_wstring(nbTab);
			msg += L" TAB(s) in your character list.";
		}
	}
	else if (nbSp && !nbTab)
	{
		wstring nbSpStr = std::to_wstring(nbSp);
		wstring warnBegin = pNativeSpeaker->getLocalizedStrFromID("word-chars-list-warning-begin", L"");
		wstring space = pNativeSpeaker->getLocalizedStrFromID("word-chars-list-space-warning", L"");
		wstring warnEnd = pNativeSpeaker->getLocalizedStrFromID("word-chars-list-warning-end", L"");

		// half translation is not allowed
		if (!warnBegin.empty() && !space.empty() && !warnEnd.empty())
		{
			space = stringReplace(space, L"$INT_REPLACE$", nbSpStr);
			msg = warnBegin;
			msg += space;
			msg += warnEnd;
		}
		else
		{
			msg = L"Be aware: ";
			msg += std::to_wstring(nbSp);
			msg += L" space(s) in your character list.";
		}
	}
	else if (!nbSp && nbTab)
	{
		wstring nbTabStr = std::to_wstring(nbTab);
		wstring warnBegin = pNativeSpeaker->getLocalizedStrFromID("word-chars-list-warning-begin", L"");
		wstring tab = pNativeSpeaker->getLocalizedStrFromID("word-chars-list-tab-warning", L"");
		wstring warnEnd = pNativeSpeaker->getLocalizedStrFromID("word-chars-list-warning-end", L"");

		// half translation is not allowed
		if (!warnBegin.empty() && !tab.empty() && !warnEnd.empty())
		{
			tab = stringReplace(tab, L"$INT_REPLACE$", nbTabStr);
			msg = warnBegin;
			msg += tab;
			msg += warnEnd;
		}
		else
		{
			msg = L"Be aware: ";
			msg += std::to_wstring(nbTab);
			msg += L" TAB(s) in your character list.";
		}
	}
	else // (!nbSp && !nbTab)
	{
		// do nothing
	}

	return msg;
}

void DelimiterSubDlg::setWarningIfNeed() const
{
	wstring msg;
	NppGUI & nppGUI = (NppParameters::getInstance()).getNppGUI();
	if (!nppGUI._isWordCharDefault)
	{
		int nbSp = 0;
		int nbTab = 0;
		detectSpace(nppGUI._customWordChars.c_str(), nbSp, nbTab);
		msg = getWarningText(nbSp, nbTab);
	}
	setItemText(IDD_STATIC_WORDCHAR_WARNING, msg.c_str());
}

void DelimiterSubDlg::setCtrlsPos(bool isMultiline)
{
	showItem(IDD_STATIC_BLABLA2NDLINE, isMultiline);
}

intptr_t DelimiterSubDlg::onInit()
{
	NppGUI & nppGUI = (NppParameters::getInstance()).getNppGUI();
	//
	// Delimiter
	//
	wchar_t opener[2]{};
	opener[0] = nppGUI._leftmostDelimiter;
	opener[1] = '\0';
	wchar_t closer[2]{};
	closer[0] = nppGUI._rightmostDelimiter;
	closer[1] = '\0';
	bool onSeveralLines = nppGUI._delimiterSelectionOnEntireDocument;
	
	limitText(IDC_EDIT_OPENDELIMITER, static_cast<int>(1));
	limitText(IDC_EDIT_CLOSEDELIMITER, static_cast<int>(1));
	setItemText(IDC_EDIT_OPENDELIMITER, opener);
	setItemText(IDC_EDIT_CLOSEDELIMITER, closer);
	setChecked(IDD_SEVERALLINEMODEON_CHECK, onSeveralLines);
	
	setCtrlsPos(onSeveralLines);

	//
	// Word Char List
	//
	
	setItemText(IDC_WORDCHAR_CUSTOM_EDIT, string2wstring(nppGUI._customWordChars));
	setChecked(IDC_RADIO_WORDCHAR_DEFAULT, nppGUI._isWordCharDefault);
	setChecked(IDC_RADIO_WORDCHAR_CUSTOM, !nppGUI._isWordCharDefault);
	enableItem(IDC_WORDCHAR_CUSTOM_EDIT, !nppGUI._isWordCharDefault);

	setWarningIfNeed();

	NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
	wstring tip2show = pNativeSpeaker->getLocalizedStrFromID("word-chars-list-tip", L"This allows you to include additional character into current word characters while double clicking for selection or searching with \"Match whole word only\" option checked.");

	setTip(IDD_WORDCHAR_QUESTION_BUTTON, tip2show);
	return TRUE;
}

intptr_t DelimiterSubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppGUI & nppGUI = (NppParameters::getInstance()).getNppGUI();
	if (HIWORD(wParam) == EN_CHANGE)
	{
		switch (LOWORD(wParam))
		{
			case  IDC_EDIT_OPENDELIMITER:
			{
				wchar_t opener[2] = { '\0' };
				copyItemText(IDC_EDIT_OPENDELIMITER, opener, MAX_PATH);
				nppGUI._leftmostDelimiter =  static_cast<char>(opener[0]);
				return TRUE;
			}
			case  IDC_EDIT_CLOSEDELIMITER:
			{
				wchar_t closer[2] = { '\0' };
				copyItemText(IDC_EDIT_CLOSEDELIMITER, closer, MAX_PATH);
				nppGUI._rightmostDelimiter =  static_cast<char>(closer[0]);
				return TRUE;
			}

			case  IDC_WORDCHAR_CUSTOM_EDIT:
			{
				nppGUI._customWordChars = wstring2string(getItemText(IDC_WORDCHAR_CUSTOM_EDIT));
				setWarningIfNeed();
				::SendMessage(mainWindow(), NPPM_INTERNAL_SETWORDCHARS, 0, 0);
				return TRUE;
			}

			default:
				return FALSE;
		}
	}

	switch (wParam)
	{

		case IDD_SEVERALLINEMODEON_CHECK:
		{
			bool isChecked = isCheckedOrNot(IDD_SEVERALLINEMODEON_CHECK);
			nppGUI._delimiterSelectionOnEntireDocument = isChecked;

			setCtrlsPos(isChecked);

			return TRUE;
		}

		case IDC_RADIO_WORDCHAR_DEFAULT:
		{
			setChecked(IDC_RADIO_WORDCHAR_CUSTOM, false);
			nppGUI._isWordCharDefault = true;
			::SendMessage(mainWindow(), NPPM_INTERNAL_SETWORDCHARS, 0, 0);
			enableItem(IDC_WORDCHAR_CUSTOM_EDIT, !nppGUI._isWordCharDefault);
			setItemText(IDD_STATIC_WORDCHAR_WARNING, L"");
			return TRUE;
		}

		case IDC_RADIO_WORDCHAR_CUSTOM:
		{
			setChecked(IDC_RADIO_WORDCHAR_DEFAULT, false);
			nppGUI._isWordCharDefault = false;
			::SendMessage(mainWindow(), NPPM_INTERNAL_SETWORDCHARS, 0, 0);
			enableItem(IDC_WORDCHAR_CUSTOM_EDIT, !nppGUI._isWordCharDefault);
			setWarningIfNeed();
			return TRUE;
		}

		default :
			return FALSE;
	}
	return FALSE;
}


intptr_t CloudAndLinkSubDlg::onInit()
{
	NppParameters& nppParams = NppParameters::getInstance();
	NppGUI & nppGUI = nppParams.getNppGUI();
	static constexpr size_t uriSchemesMaxLength = 2048;

	wstring errMsg = L"";

	bool withCloud = nppGUI._cloudPath != L"";
	if (withCloud)
	{
		// detect validation of path
		if (!doesDirectoryExist(nppGUI._cloudPath.c_str()))
			errMsg = L"Invalid path";
	}
	
	setItemText(IDC_SETTINGSONCLOUD_WARNING_STATIC, errMsg.c_str());

	setChecked(IDC_NOCLOUD_RADIO, !withCloud);
	setChecked(IDC_WITHCLOUD_RADIO, withCloud);
	setItemText(IDC_CLOUDPATH_EDIT, nppGUI._cloudPath.c_str());
	enableItem(IDC_CLOUDPATH_EDIT, withCloud);
	enableItem(IDD_CLOUDPATH_BROWSE_BUTTON, withCloud);

	BOOL linkEnable = nppGUI._styleURL != urlDisable;
	BOOL dontUnderline = (nppGUI._styleURL == urlNoUnderLineFg) || (nppGUI._styleURL == urlNoUnderLineBg);
	BOOL roundBoxMode = (nppGUI._styleURL == urlNoUnderLineBg) || (nppGUI._styleURL == urlUnderLineBg);
	limitText(IDC_URISCHEMES_EDIT, static_cast<int>(uriSchemesMaxLength));
	setItemText(IDC_URISCHEMES_EDIT, nppGUI._uriSchemes);

	setChecked(IDC_CHECK_CLICKABLELINK_ENABLE, linkEnable);
	setChecked(IDC_CHECK_CLICKABLELINK_NOUNDERLINE, dontUnderline);
	setChecked(IDC_CHECK_CLICKABLELINK_FULLBOXMODE, roundBoxMode);
	enableItem(IDC_CHECK_CLICKABLELINK_NOUNDERLINE, linkEnable);
	enableItem(IDC_CHECK_CLICKABLELINK_FULLBOXMODE, linkEnable);
	enableItem(IDC_URISCHEMES_EDIT, linkEnable);

	return TRUE;
}

intptr_t CloudAndLinkSubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppParameters& nppParams = NppParameters::getInstance();
	NppGUI & nppGUI = nppParams.getNppGUI();
	static constexpr size_t uriSchemesMaxLength = 2048;

	if (HIWORD(wParam) == EN_CHANGE)
	{
		switch (LOWORD(wParam))
		{
			case  IDC_CLOUDPATH_EDIT:
			{
				const wstring inputDirExpanded = expandEnvironmentStrs(getItemText(IDC_CLOUDPATH_EDIT));
				NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
				if (doesDirectoryExist(inputDirExpanded.c_str()))
				{
					nppGUI._cloudPath = inputDirExpanded;
					nppParams.setCloudChoice(inputDirExpanded.c_str());

					wstring warningMsg;
					if (nppParams.isCloudPathChanged())
					{
						warningMsg = pNativeSpeaker->getLocalizedStrFromID("cloud-restart-warning", L"Please restart Notepad++ to take effect.");
					}
					setItemText(IDC_SETTINGSONCLOUD_WARNING_STATIC, warningMsg.c_str());
				}
				else
				{
					bool isChecked = isCheckedOrNot(IDC_WITHCLOUD_RADIO);
					if (isChecked)
					{
						wstring errMsg = pNativeSpeaker->getLocalizedStrFromID("cloud-invalid-warning", L"Invalid path.");

						setItemText(IDC_SETTINGSONCLOUD_WARNING_STATIC, errMsg.c_str());
						nppParams.removeCloudChoice();
					}
				}
				return TRUE;
			}

			case IDC_URISCHEMES_EDIT:
			{
				wchar_t uriScheme[uriSchemesMaxLength] = { '\0' };
				copyItemText(IDC_URISCHEMES_EDIT, uriScheme, uriSchemesMaxLength);
				nppGUI._uriSchemes = uriScheme;
				HWND grandParent = mainWindow();
				::SendMessage(grandParent, NPPM_INTERNAL_UPDATECLICKABLELINKS, 0, 0);
				return TRUE;
			}
		}
	}

	NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
	switch (wParam)
	{
		case IDC_NOCLOUD_RADIO:
		{
			nppGUI._cloudPath = L"";
			nppParams.removeCloudChoice();

			wstring warningMsg;
			if (nppParams.isCloudPathChanged())
			{
				warningMsg = pNativeSpeaker->getLocalizedStrFromID("cloud-restart-warning", L"Please restart Notepad++ to take effect.");
			}
			// else set empty string
			setItemText(IDC_SETTINGSONCLOUD_WARNING_STATIC, warningMsg.c_str());

			setItemText(IDC_CLOUDPATH_EDIT, nppGUI._cloudPath.c_str());
			enableItem(IDC_CLOUDPATH_EDIT, false);
			enableItem(IDD_CLOUDPATH_BROWSE_BUTTON, false);
		}
		break;

		case IDC_WITHCLOUD_RADIO:
		{
			wstring errMsg = pNativeSpeaker->getLocalizedStrFromID("cloud-invalid-warning", L"Invalid path.");
			setItemText(IDC_SETTINGSONCLOUD_WARNING_STATIC, errMsg.c_str());

			enableItem(IDC_CLOUDPATH_EDIT, true);
			enableItem(IDD_CLOUDPATH_BROWSE_BUTTON, true);
		}
		break;

		case IDD_CLOUDPATH_BROWSE_BUTTON:
		{
			wstring warningMsg = pNativeSpeaker->getLocalizedStrFromID("cloud-select-folder", L"Select a folder from/to where Notepad++ reads/writes its settings");
			folderBrowser(_hSelf, warningMsg, qobject_cast<QLineEdit*>(dlgItem(IDC_CLOUDPATH_EDIT)));
		}
		break;

		case IDC_CHECK_CLICKABLELINK_ENABLE:
		{
			bool isChecked = isCheckedOrNot(IDC_CHECK_CLICKABLELINK_ENABLE);
			if (!isChecked)
			{
				setChecked(IDC_CHECK_CLICKABLELINK_NOUNDERLINE, false);
				setChecked(IDC_CHECK_CLICKABLELINK_FULLBOXMODE, false);
			}
			enableItem(IDC_CHECK_CLICKABLELINK_NOUNDERLINE, isChecked);
			enableItem(IDC_CHECK_CLICKABLELINK_FULLBOXMODE, isChecked);
			enableItem(IDC_URISCHEMES_EDIT, isChecked);

			redrawDlgItem(IDC_CLICKABLELINK_STATIC);

			nppGUI._styleURL = isChecked ? urlUnderLineFg : urlDisable;
			HWND grandParent = mainWindow();
			::SendMessage(grandParent, NPPM_INTERNAL_UPDATECLICKABLELINKS, isChecked ? 0 : 1, 0);
		}
		return TRUE;

		case IDC_CHECK_CLICKABLELINK_NOUNDERLINE:
		case IDC_CHECK_CLICKABLELINK_FULLBOXMODE:
		{
			bool isNoUnderline = isCheckedOrNot(IDC_CHECK_CLICKABLELINK_NOUNDERLINE);
			bool isRoundBoxMode = isCheckedOrNot(IDC_CHECK_CLICKABLELINK_FULLBOXMODE);
			if (isRoundBoxMode)
				nppGUI._styleURL = isNoUnderline ? urlNoUnderLineBg : urlUnderLineBg;
			else
				nppGUI._styleURL = isNoUnderline ? urlNoUnderLineFg : urlUnderLineFg;
			HWND grandParent = mainWindow();
			::SendMessage(grandParent, NPPM_INTERNAL_UPDATECLICKABLELINKS, 0, 0);
		}
		return TRUE;

		default:
			return FALSE;

	}
	return FALSE;
}


intptr_t PerformanceSubDlg::onInit()
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = NppParameters::getInstance().getNppGUI();

	int64_t fileLenInMB = (nppGUI._largeFileRestriction._largeFileSizeDefInByte / 1024) / 1024;
	setItemInt(IDC_EDIT_PERFORMANCE_FILESIZE, static_cast<UINT>(fileLenInMB));
	setChecked(IDC_CHECK_PERFORMANCE_ENABLE, nppGUI._largeFileRestriction._isEnabled);
	setChecked(IDC_CHECK_PERFORMANCE_ALLOWBRACEMATCH, nppGUI._largeFileRestriction._allowBraceMatch);
	setChecked(IDC_CHECK_PERFORMANCE_ALLOWAUTOCOMPLETION, nppGUI._largeFileRestriction._allowAutoCompletion);
	setChecked(IDC_CHECK_PERFORMANCE_ALLOWSMARTHILITE, nppGUI._largeFileRestriction._allowSmartHilite);
	setChecked(IDC_CHECK_PERFORMANCE_ALLOWCLICKABLELINK, nppGUI._largeFileRestriction._allowClickableLink);
	setChecked(IDC_CHECK_PERFORMANCE_DEACTIVATEWORDWRAP, nppGUI._largeFileRestriction._deactivateWordWrap);
	setChecked(IDC_CHECK_PERFORMANCE_SUPPRESS2GBWARNING, nppGUI._largeFileRestriction._suppress2GBWarning);
	
	showItem(IDC_CHECK_PERFORMANCE_SUPPRESS2GBWARNING, sizeof(void*) > 4);

	bool largeFileRestrictionEnabled = isCheckedOrNot(IDC_CHECK_PERFORMANCE_ENABLE);
	enableItem(IDC_EDIT_PERFORMANCE_FILESIZE, largeFileRestrictionEnabled);
	enableItem(IDC_CHECK_PERFORMANCE_ALLOWBRACEMATCH, largeFileRestrictionEnabled);
	enableItem(IDC_CHECK_PERFORMANCE_ALLOWAUTOCOMPLETION, largeFileRestrictionEnabled);
	enableItem(IDC_CHECK_PERFORMANCE_ALLOWSMARTHILITE, largeFileRestrictionEnabled);
	enableItem(IDC_CHECK_PERFORMANCE_ALLOWCLICKABLELINK, largeFileRestrictionEnabled);
	enableItem(IDC_CHECK_PERFORMANCE_DEACTIVATEWORDWRAP, largeFileRestrictionEnabled);

	NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
	wstring enablePerfTip = pNativeSpeaker->getLocalizedStrFromID("largeFileRestriction-tip", L"Some features may slow performance in large files. These features can be auto-disabled on opening a large file. You can customize them here.\n\nNOTE:\n1. Modifying options here requires re-open currently opened large files to get proper behavior.\n\n2. If \"Deactivate Word Wrap globally\" is checked and you open a large file, \"Word Wrap\" will be disabled for all files. You can re-enable it via menu \"View->Word Wrap\"");
	setTip(IDD_PERFORMANCE_TIP_QUESTION_BUTTON, enablePerfTip);

	return TRUE;
}

intptr_t PerformanceSubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGUI = NppParameters::getInstance().getNppGUI();

	if (HIWORD(wParam) == EN_CHANGE && LOWORD(wParam) == IDC_EDIT_PERFORMANCE_FILESIZE)
	{

		static constexpr int stringSize = 16;
		wchar_t str[stringSize]{};

		copyItemText(IDC_EDIT_PERFORMANCE_FILESIZE, str, stringSize);

		if (wcscmp(str, L"") == 0)
			return TRUE;

		static constexpr int fileLenInMBMax = (INT32_MAX - 1024 * 1024) / 1024 / 1024; // -1MB ... have to to consider also the bufferSizeRequested algo in FileManager::loadFileData
		int64_t fileLenInMB = getItemInt(IDC_EDIT_PERFORMANCE_FILESIZE);
		if (fileLenInMB > fileLenInMBMax)
		{
			fileLenInMB = fileLenInMBMax;
			setItemInt(IDC_EDIT_PERFORMANCE_FILESIZE, static_cast<UINT>(fileLenInMB));
		}

		nppGUI._largeFileRestriction._largeFileSizeDefInByte = fileLenInMB * 1024 * 1024;

		return TRUE;
	}
	else if (HIWORD(wParam) == EN_KILLFOCUS && LOWORD(wParam) == IDC_EDIT_PERFORMANCE_FILESIZE)
	{
		static constexpr int stringSize = 16;
		wchar_t str[stringSize]{};
		copyItemText(IDC_EDIT_PERFORMANCE_FILESIZE, str, stringSize);

		if (wcscmp(str, L"") == 0)
		{
			setItemInt(IDC_EDIT_PERFORMANCE_FILESIZE, (NPP_STYLING_FILESIZE_LIMIT_DEFAULT / 1024) / 1024);
			return TRUE;
		}

		int64_t fileLenInMB = getItemInt(IDC_EDIT_PERFORMANCE_FILESIZE);

		if (fileLenInMB == 0)
		{
			fileLenInMB = (NPP_STYLING_FILESIZE_LIMIT_DEFAULT / 1024) / 1024;
			setItemInt(IDC_EDIT_PERFORMANCE_FILESIZE, static_cast<UINT>(fileLenInMB));
			return TRUE;
		}

		return TRUE;
	}

	switch (wParam)
	{
		case IDC_CHECK_PERFORMANCE_ENABLE:
		{
			bool largeFileRestrictionEnabled = isCheckedOrNot(IDC_CHECK_PERFORMANCE_ENABLE);
			nppGUI._largeFileRestriction._isEnabled = largeFileRestrictionEnabled;

			enableItem(IDC_EDIT_PERFORMANCE_FILESIZE, largeFileRestrictionEnabled);
			enableItem(IDC_CHECK_PERFORMANCE_ALLOWBRACEMATCH, largeFileRestrictionEnabled);
			enableItem(IDC_CHECK_PERFORMANCE_ALLOWAUTOCOMPLETION, largeFileRestrictionEnabled);
			enableItem(IDC_CHECK_PERFORMANCE_ALLOWSMARTHILITE, largeFileRestrictionEnabled);
			enableItem(IDC_CHECK_PERFORMANCE_ALLOWCLICKABLELINK, largeFileRestrictionEnabled);
			enableItem(IDC_CHECK_PERFORMANCE_DEACTIVATEWORDWRAP, largeFileRestrictionEnabled);

			redrawDlgItem(IDC_STATIC_PERFORMANCE_FILESIZE);
			redrawDlgItem(IDC_STATIC_PERFORMANCE_MB);

			if (largeFileRestrictionEnabled)
			{
				::SendMessage(mainWindow(), NPPM_INTERNAL_CLEANSMARTHILITING, 0, 0);
				::SendMessage(mainWindow(), NPPM_INTERNAL_CLEANBRACEMATCH, 0, 0);
			}
			else
			{
				::SendMessage(mainWindow(), NPPM_INTERNAL_UPDATECLICKABLELINKS, 0, 0);
			}
		}
		return TRUE;

		case IDC_CHECK_PERFORMANCE_ALLOWBRACEMATCH:
		{
			bool isAllowed = isCheckedOrNot(static_cast<int>(wParam));
			nppGUI._largeFileRestriction._allowBraceMatch = isAllowed;
			if (!isAllowed)
				::SendMessage(mainWindow(), NPPM_INTERNAL_CLEANBRACEMATCH, 0, 0);
		}
		return TRUE;

		case IDC_CHECK_PERFORMANCE_ALLOWAUTOCOMPLETION:
		{
			bool isAllowed = isCheckedOrNot(static_cast<int>(wParam));
			nppGUI._largeFileRestriction._allowAutoCompletion = isAllowed;
		}
		return TRUE;

		case IDC_CHECK_PERFORMANCE_ALLOWSMARTHILITE:
		{
			bool isAllowed = isCheckedOrNot(static_cast<int>(wParam));
			nppGUI._largeFileRestriction._allowSmartHilite = isAllowed;
			if (!isAllowed)
				::SendMessage(mainWindow(), NPPM_INTERNAL_CLEANSMARTHILITING, 0, 0);
		}
		return TRUE;

		case IDC_CHECK_PERFORMANCE_ALLOWCLICKABLELINK:
		{
			bool isAllowed = isCheckedOrNot(static_cast<int>(wParam));
			nppGUI._largeFileRestriction._allowClickableLink = isAllowed;
			::SendMessage(mainWindow(), NPPM_INTERNAL_UPDATECLICKABLELINKS, 0, 0);
		}
		return TRUE;

		case IDC_CHECK_PERFORMANCE_DEACTIVATEWORDWRAP:
		{
			bool isDeactivated = isCheckedOrNot(static_cast<int>(wParam));
			nppGUI._largeFileRestriction._deactivateWordWrap = isDeactivated;
		}
		return TRUE;

		case IDC_CHECK_PERFORMANCE_SUPPRESS2GBWARNING:
		{
			bool isDeactivated = isCheckedOrNot(static_cast<int>(wParam));
			nppGUI._largeFileRestriction._suppress2GBWarning = isDeactivated;
		}
		return TRUE;

		default:
			return FALSE;
	}
	return FALSE;
}


intptr_t SearchEngineSubDlg::onInit()
{
	NppParameters& nppParams = NppParameters::getInstance();
	NppGUI & nppGUI = nppParams.getNppGUI();

	if (nppGUI._searchEngineChoice == nppGUI.se_custom)
	{
		if (nppGUI._searchEngineCustom.empty())
		{
			nppGUI._searchEngineChoice = nppGUI.se_google;
		}
	}
	if (nppGUI._searchEngineChoice == nppGUI.se_bing)
		nppGUI._searchEngineChoice = nppGUI.se_duckDuckGo;

	setChecked(IDC_SEARCHENGINE_CUSTOM_RADIO, nppGUI._searchEngineChoice == nppGUI.se_custom);
	setChecked(IDC_SEARCHENGINE_DUCKDUCKGO_RADIO, nppGUI._searchEngineChoice == nppGUI.se_duckDuckGo);
	setChecked(IDC_SEARCHENGINE_GOOGLE_RADIO, nppGUI._searchEngineChoice == nppGUI.se_google);
	setChecked(IDC_SEARCHENGINE_YAHOO_RADIO, nppGUI._searchEngineChoice == nppGUI.se_yahoo);
	setChecked(IDC_SEARCHENGINE_STACKOVERFLOW_RADIO, nppGUI._searchEngineChoice == nppGUI.se_stackoverflow);

	setItemText(IDC_SEARCHENGINE_EDIT, nppGUI._searchEngineCustom.c_str());
	enableItem(IDC_SEARCHENGINE_EDIT, nppGUI._searchEngineChoice == nppGUI.se_custom);

	return TRUE;
}

intptr_t SearchEngineSubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppParameters& nppParams = NppParameters::getInstance();
	NppGUI & nppGUI = nppParams.getNppGUI();

	if (HIWORD(wParam) == EN_CHANGE && LOWORD(wParam) == IDC_SEARCHENGINE_EDIT)
	{
		const wstring input = getItemText(IDC_SEARCHENGINE_EDIT);
		nppGUI._searchEngineCustom = input;
		return TRUE;
	}

	switch (wParam)
	{
		case IDC_SEARCHENGINE_DUCKDUCKGO_RADIO:
		{
			nppGUI._searchEngineChoice = nppGUI.se_duckDuckGo;
			enableItem(IDC_SEARCHENGINE_EDIT, false);
		}
		break;

		case IDC_SEARCHENGINE_GOOGLE_RADIO:
		{
			nppGUI._searchEngineChoice = nppGUI.se_google;
			enableItem(IDC_SEARCHENGINE_EDIT, false);
		}
		break;

		case IDC_SEARCHENGINE_YAHOO_RADIO:
		{
			nppGUI._searchEngineChoice = nppGUI.se_yahoo;
			enableItem(IDC_SEARCHENGINE_EDIT, false);
		}
		break;

		case IDC_SEARCHENGINE_STACKOVERFLOW_RADIO:
		{
			nppGUI._searchEngineChoice = nppGUI.se_stackoverflow;
			enableItem(IDC_SEARCHENGINE_EDIT, false);
		}
		break;

		case IDC_SEARCHENGINE_CUSTOM_RADIO:
		{
			nppGUI._searchEngineChoice = nppGUI.se_custom;
			enableItem(IDC_SEARCHENGINE_EDIT, true);
		}
		break;

		default:
			return FALSE;
	}
	return FALSE;
}


intptr_t SearchingSubDlg::onInit()
{
	NppParameters& nppParams = NppParameters::getInstance();
	NppGUI& nppGUI = nppParams.getNppGUI();

	setChecked(IDC_CHECK_FILL_FIND_FIELD_WITH_SELECTED, nppGUI._fillFindFieldWithSelected);
	setChecked(IDC_CHECK_FILL_FIND_FIELD_SELECT_CARET, nppGUI._fillFindFieldSelectCaret);
	setChecked(IDC_CHECK_MONOSPACEDFONT_FINDDLG, nppGUI._monospacedFontFindDlg);
	setChecked(IDC_CHECK_FINDDLG_ALWAYS_VISIBLE, nppGUI._findDlgAlwaysVisible);
	setItemInt(IDC_FILLFINDWHAT_THRESHOLD_EDIT, nppGUI._fillFindWhatThreshold);

	enableItem(IDC_FILLFINDWHAT_THRESH_QUESTION_BUTTON, nppGUI._fillFindFieldWithSelected ? TRUE : FALSE);
	enableItem(IDC_FILLFINDWHAT_THRESHOLD_EDIT, nppGUI._fillFindFieldWithSelected ? TRUE : FALSE);
	enableItem(IDC_FILLFINDWHAT_THRESHOLD_STATIC, nppGUI._fillFindFieldWithSelected ? TRUE : FALSE);
	enableItem(IDC_CHECK_FILL_FIND_FIELD_SELECT_CARET, nppGUI._fillFindFieldWithSelected ? TRUE : FALSE);

	setChecked(IDC_CHECK_SHOWONCEPERFOUNDLINE, nppGUI._finderShowOnlyOneEntryPerFoundLine);
	setChecked(IDC_CHECK_FIF_IGNOREOPENEDFILES, nppGUI._fif_ignoreunsavedChangesInOpenedFiles);
	setItemInt(IDC_INSELECTION_THRESHOLD_EDIT, nppGUI._inSelectionAutocheckThreshold);
	setChecked(IDC_CHECK_FILL_DIR_FIELD_FROM_ACTIVE_DOC, nppGUI._fillDirFieldFromActiveDoc);

	NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
	wstring tipInSelectionText = pNativeSpeaker->getLocalizedStrFromID("searchingInSelThresh-tip", L"Minimum number of selected characters in edit zone to automatically check the \"In selection\" checkbox when the Find dialog is activated. The maximum value is $INT_REPLACE$. Set the value to 0 to disable auto-checking.");
	
	tipInSelectionText = stringReplace(tipInSelectionText, L"$INT_REPLACE$", std::to_wstring(FINDREPLACE_INSELECTION_THRESHOLD_DEFAULT));

	setTip(IDC_INSELECTION_THRESH_QUESTION_BUTTON, tipInSelectionText);

	wstring tipFillFindWhatText = pNativeSpeaker->getLocalizedStrFromID("searchingFillFindWhat-tip", L"Maximum number of selected characters in edit zone to fill automatically the \"Find what\" field when the Ctrl-F is triggered. The maximum value is $INT_REPLACE$, which is the maximum size of \"Find what\" field, limited by the system.");

	tipFillFindWhatText = stringReplace(tipFillFindWhatText, L"$INT_REPLACE$", std::to_wstring(FINDREPLACE_MAXLENGTH - 1));

	setTip(IDC_FILLFINDWHAT_THRESH_QUESTION_BUTTON, tipFillFindWhatText);
	return TRUE;
}

intptr_t SearchingSubDlg::onCommand(WPARAM wParam, LPARAM lParam)
{
	NppParameters& nppParams = NppParameters::getInstance();
	NppGUI& nppGUI = nppParams.getNppGUI();

	if (HIWORD(wParam) == EN_CHANGE)
	{
		if (LOWORD(wParam) == IDC_INSELECTION_THRESHOLD_EDIT)
		{
			static constexpr int stringSize = 5;
			wchar_t str[stringSize]{};
			copyItemText(IDC_INSELECTION_THRESHOLD_EDIT, str, stringSize);

			if (wcscmp(str, L"") == 0)
			{
				setItemInt(IDC_INSELECTION_THRESHOLD_EDIT, FINDREPLACE_INSELECTION_THRESHOLD_DEFAULT);
				return TRUE;
			}

			UINT newValue = getItemInt(IDC_INSELECTION_THRESHOLD_EDIT);

			if (static_cast<int>(newValue) == nppGUI._inSelectionAutocheckThreshold)
			{
				return TRUE;
			}

			if (newValue > FINDREPLACE_INSELECTION_THRESHOLD_DEFAULT)
			{
				setItemInt(IDC_INSELECTION_THRESHOLD_EDIT, FINDREPLACE_INSELECTION_THRESHOLD_DEFAULT);
				newValue = FINDREPLACE_INSELECTION_THRESHOLD_DEFAULT;
			}

			nppGUI._inSelectionAutocheckThreshold = newValue;

			return TRUE;
		}
		else if (LOWORD(wParam) == IDC_FILLFINDWHAT_THRESHOLD_EDIT)
		{
			static constexpr int stringSize = 6;
			wchar_t str[stringSize]{};
			copyItemText(IDC_FILLFINDWHAT_THRESHOLD_EDIT, str, stringSize);

			if (wcscmp(str, L"") == 0)
			{
				setItemInt(IDC_FILLFINDWHAT_THRESHOLD_EDIT, FILL_FINDWHAT_THRESHOLD_DEFAULT);
				return TRUE;
			}

			UINT newValue = getItemInt(IDC_FILLFINDWHAT_THRESHOLD_EDIT);

			if (static_cast<int>(newValue) == nppGUI._fillFindWhatThreshold)
			{
				return TRUE;
			}

			if (newValue >= FINDREPLACE_MAXLENGTH)
			{
				newValue = FINDREPLACE_MAXLENGTH - 1;
				setItemInt(IDC_FILLFINDWHAT_THRESHOLD_EDIT, newValue);
			}

			if (newValue == 0)
			{
				newValue = 1;
				setItemInt(IDC_FILLFINDWHAT_THRESHOLD_EDIT, newValue);
			}

			nppGUI._fillFindWhatThreshold = newValue;

			return TRUE;
		}
	}

	switch (wParam)
	{
		case IDC_CHECK_FILL_FIND_FIELD_WITH_SELECTED:
		{
			nppGUI._fillFindFieldWithSelected = isCheckedOrNot(IDC_CHECK_FILL_FIND_FIELD_WITH_SELECTED);
			enableItem(IDC_CHECK_FILL_FIND_FIELD_SELECT_CARET, nppGUI._fillFindFieldWithSelected ? TRUE : FALSE);
			enableItem(IDC_FILLFINDWHAT_THRESH_QUESTION_BUTTON, nppGUI._fillFindFieldWithSelected ? TRUE : FALSE);
			enableItem(IDC_FILLFINDWHAT_THRESHOLD_EDIT, nppGUI._fillFindFieldWithSelected ? TRUE : FALSE);
			enableItem(IDC_FILLFINDWHAT_THRESHOLD_STATIC, nppGUI._fillFindFieldWithSelected ? TRUE : FALSE);

			if (!nppGUI._fillFindFieldWithSelected)
			{
				setChecked(IDC_CHECK_FILL_FIND_FIELD_SELECT_CARET, false);
				nppGUI._fillFindFieldSelectCaret = false;
			}
			return TRUE;
		}

		case IDC_CHECK_MONOSPACEDFONT_FINDDLG:
		{
			nppGUI._monospacedFontFindDlg = isCheckedOrNot(IDC_CHECK_MONOSPACEDFONT_FINDDLG);
			return TRUE;
		}

		case IDC_CHECK_FINDDLG_ALWAYS_VISIBLE:
		{
			nppGUI._findDlgAlwaysVisible = isCheckedOrNot(IDC_CHECK_FINDDLG_ALWAYS_VISIBLE);
			return TRUE;
		}

		case IDC_CHECK_SHOWONCEPERFOUNDLINE:
		{
			nppGUI._finderShowOnlyOneEntryPerFoundLine = isCheckedOrNot(IDC_CHECK_SHOWONCEPERFOUNDLINE);
			return TRUE;
		}

		case IDC_CHECK_FIF_IGNOREOPENEDFILES:
		{
			nppGUI._fif_ignoreunsavedChangesInOpenedFiles = isCheckedOrNot(IDC_CHECK_FIF_IGNOREOPENEDFILES);
			return TRUE;
		}

		case IDC_CHECK_FILL_FIND_FIELD_SELECT_CARET:
		{
			nppGUI._fillFindFieldSelectCaret = isCheckedOrNot(IDC_CHECK_FILL_FIND_FIELD_SELECT_CARET);
			return TRUE;
		}

		case IDC_CHECK_FILL_DIR_FIELD_FROM_ACTIVE_DOC:
		{
			nppGUI._fillDirFieldFromActiveDoc = isCheckedOrNot(IDC_CHECK_FILL_DIR_FIELD_FROM_ACTIVE_DOC);
			return TRUE;
		}

		default:
			return FALSE;
	}
	return FALSE;
}

