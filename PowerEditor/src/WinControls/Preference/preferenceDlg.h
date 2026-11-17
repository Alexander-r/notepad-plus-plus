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

#include <QPoint>
#include <QWidget>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ColourPicker.h"
#include "CommandDialog.h"
#include "Common.h"
#include "Notepad_plus_msgs.h"
#include "NppMessages.h"
#include "Parameters.h"
#include "StaticDialog.h"
#include "preference_rc.h"

class QDialog;
class QListWidget;
class QMenu;
class QSlider;
class QStackedWidget;

// A page of the preferences, as a child widget of the dialog
class PreferenceSubDlg : public CommandDialog
{
public:
	void createPage(HINSTANCE hInst, HWND dialog, QStackedWidget* pages, const std::function<void(QDialog*)>& setupForm);

protected:
	virtual intptr_t onInit() = 0;
	void onCancel() override;

	HWND mainWindow() const { return _hParent->parentWidget(); }
};

class MiscSubDlg : public PreferenceSubDlg
{
friend class PreferenceDlg;
public :
	MiscSubDlg() = default;

private :
	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
};

class GeneralSubDlg : public PreferenceSubDlg
{
public :
	GeneralSubDlg() = default;

private :
	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
};

class ToolbarSubDlg : public PreferenceSubDlg
{
friend class PreferenceDlg;
public:
	ToolbarSubDlg() = default;

private:
	std::unique_ptr<ColourPicker> _pIconColorPicker = nullptr;

	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
	UINT getToolbarIconSetMsg(int* iconSetID);
	void enableIconColorPicker(bool enable, bool useDark);
	void changeColorableState(bool isForDarkModeChange);
};

class TabbarSubDlg : public PreferenceSubDlg
{
friend class PreferenceDlg;
public:
	TabbarSubDlg() = default;
	void setTabbarAlternateIcons(bool enable = false);

private:

	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
};

class EditingSubDlg : public PreferenceSubDlg
{
friend class PreferenceDlg;
public :
	EditingSubDlg() = default;
	
private :
	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
	void onScroll(HWND slider) override;
	void initScintParam();
	void changeLineHiliteMode(bool enableSlider);
};

class Editing2SubDlg : public PreferenceSubDlg
{
friend class PreferenceDlg;
public :
	Editing2SubDlg() = default;

private:

	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
};

class DarkModeSubDlg : public PreferenceSubDlg
{
public:
	DarkModeSubDlg();
	~DarkModeSubDlg() override;

	// The system asked for the dark mode
	void chooseDarkMode() { onCommand(IDC_RADIO_DARKMODE_DARKMODE, 0); }


private:
	std::unique_ptr<ColourPicker> _pBackgroundColorPicker = nullptr;
	std::unique_ptr<ColourPicker> _pCtrlBackgroundColorPicker = nullptr;
	std::unique_ptr<ColourPicker> _pHotBackgroundColorPicker = nullptr;
	std::unique_ptr<ColourPicker> _pDlgBackgroundColorPicker = nullptr;
	std::unique_ptr<ColourPicker> _pErrorBackgroundColorPicker = nullptr;
	std::unique_ptr<ColourPicker> _pTextColorPicker = nullptr;
	std::unique_ptr<ColourPicker> _pDarkerTextColorPicker = nullptr;
	std::unique_ptr<ColourPicker> _pDisabledTextColorPicker = nullptr;
	std::unique_ptr<ColourPicker> _pEdgeColorPicker = nullptr;
	std::unique_ptr<ColourPicker> _pLinkColorPicker = nullptr;
	std::unique_ptr<ColourPicker> _pHotEdgeColorPicker = nullptr;
	std::unique_ptr<ColourPicker> _pDisabledEdgeColorPicker = nullptr;

	std::unique_ptr<QMenu> _resetPopupMenu;

	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
	void enableCustomizedColorCtrls(bool doEnable);
	void showResetMenu();
};

class MarginsBorderEdgeSubDlg : public PreferenceSubDlg
{
friend class PreferenceDlg;
public :
	MarginsBorderEdgeSubDlg() = default;

private :

	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
	void onScroll(HWND slider) override;
	void initScintParam();
};

struct LangID_Name
{
	LangType _id = L_TEXT;
	std::wstring _name;
	LangID_Name(LangType id, const std::wstring& name) : _id(id), _name(name) {}
};

class NewDocumentSubDlg : public PreferenceSubDlg
{
public :
	NewDocumentSubDlg() = default;

private :
	void makeOpenAnsiAsUtf8(bool doIt) const;
	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
};

class DefaultDirectorySubDlg : public PreferenceSubDlg
{
public :
	DefaultDirectorySubDlg() = default;

private :
	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
};

class RecentFilesHistorySubDlg : public PreferenceSubDlg
{
public :
	RecentFilesHistorySubDlg() = default;
private :
	void setCustomLen(int val);
	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
};

class LanguageSubDlg : public PreferenceSubDlg
{
public :
	LanguageSubDlg() = default;

private :
	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
	std::vector<LangMenuItem> _langList;
};

class IndentationSubDlg : public PreferenceSubDlg
{
friend class PreferenceDlg;
public :
	IndentationSubDlg() = default;

private :
	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
};

class HighlightingSubDlg : public PreferenceSubDlg
{
public :
	HighlightingSubDlg() = default;

private :

	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
};


class SearchingSubDlg : public PreferenceSubDlg
{
friend class PreferenceDlg;
public:
	SearchingSubDlg() = default;

private:
	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
};

class PrintSubDlg : public PreferenceSubDlg
{
public :
	PrintSubDlg() = default;

private :
	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
	std::vector<std::wstring> varList;
	int _focusedEditCtrl = 0;
};

class BackupSubDlg : public PreferenceSubDlg
{
public :
	BackupSubDlg() = default;

private :
	void updateBackupSessionGUI();
	void updateBackupOnSaveGUI();
	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
};


class AutoCompletionSubDlg : public PreferenceSubDlg
{
public :
	AutoCompletionSubDlg() = default;
private :
	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
	void onScroll(HWND slider) override;
};

class MultiInstanceSubDlg : public PreferenceSubDlg
{
public :
	MultiInstanceSubDlg() = default;

private :
	const SYSTEMTIME _BTTF_time = {1985, 10, 6, 26, 16, 24, 42, 0};
	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
};

class DelimiterSubDlg : public PreferenceSubDlg
{
friend class PreferenceDlg;
public :
	DelimiterSubDlg() = default;

private :

	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
	void detectSpace(const char *text2Check, int & nbSp, int & nbTab) const;
	std::wstring getWarningText(size_t nbSp, size_t nbTab) const;
	void setWarningIfNeed() const;
	void setCtrlsPos(bool isMultiline);
};

class CloudAndLinkSubDlg : public PreferenceSubDlg
{
public :
	CloudAndLinkSubDlg() = default;

private :
	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
};

class SearchEngineSubDlg : public PreferenceSubDlg
{
public :
	SearchEngineSubDlg() = default;

private :
	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
};

class PerformanceSubDlg : public PreferenceSubDlg
{
friend class PreferenceDlg;
public :
	PerformanceSubDlg() = default;

private :
	intptr_t onInit() override;
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;

};

class PreferenceDlg : public StaticDialog
{
friend class NativeLangSpeaker;
friend class Notepad_plus;
public :
	PreferenceDlg() = default;
	void doDialog(bool isRTL = false);

	bool renameDialogTitle(const wchar_t *internalName, const wchar_t *newName);

	int getListSelectedIndex() const;
	void showDialogByName(const wchar_t *name) const;
	bool setListSelection(size_t currentSel) const;

	bool goToSection(size_t iPage, intptr_t ctrlID = -1);

	void destroy() override;

private :
	struct PageInfo
	{
		PreferenceSubDlg* _dlg = nullptr;
		std::wstring _name;
		std::wstring _internalName;
	};

	void createDialog(bool isRTL);
	LRESULT onMessage(UINT message, WPARAM wParam, LPARAM lParam);
	void makeCategoryList();
	void fitCategoryList();
	int getIndexFromName(const wchar_t* name) const;
	void showDialogByIndex(size_t index) const;

	std::vector<PageInfo> _wVector;
	QStackedWidget* _pages = nullptr;
	QListWidget* _categories = nullptr;
	GeneralSubDlg _generalSubDlg;
	ToolbarSubDlg _toolbarSubDlg;
	TabbarSubDlg _tabbarSubDlg;
	EditingSubDlg _editingSubDlg;
	Editing2SubDlg _editing2SubDlg;
	DarkModeSubDlg _darkModeSubDlg;
	MarginsBorderEdgeSubDlg _marginsBorderEdgeSubDlg;
	MiscSubDlg _miscSubDlg;
	LanguageSubDlg _languageSubDlg;
	IndentationSubDlg _indentationSubDlg;
	HighlightingSubDlg _highlightingSubDlg;
	PrintSubDlg _printSubDlg;
	NewDocumentSubDlg _newDocumentSubDlg;
	DefaultDirectorySubDlg	_defaultDirectorySubDlg;
	RecentFilesHistorySubDlg _recentFilesHistorySubDlg;
	BackupSubDlg _backupSubDlg;
	AutoCompletionSubDlg _autoCompletionSubDlg;
	MultiInstanceSubDlg _multiInstanceSubDlg;
	DelimiterSubDlg _delimiterSubDlg;
	PerformanceSubDlg _performanceSubDlg;
	CloudAndLinkSubDlg _cloudAndLinkSubDlg;
	SearchEngineSubDlg _searchEngineSubDlg;
	SearchingSubDlg _searchingSubDlg;
};
