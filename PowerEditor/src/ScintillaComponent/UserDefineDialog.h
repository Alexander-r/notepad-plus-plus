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

#include <cwchar>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <SciLexer.h>

#include <QRect>

#include "ColourPicker.h"
#include "CommandDialog.h"
#include "Parameters.h"
#include "StaticDialog.h"
#include "URLCtrl.h"
#include "UserDefineResource.h"
#include "UdlKeywordMappers.h"
#include "Window.h"

class QTabWidget;
class ScintillaEditView;
class QTabWidget;

inline constexpr bool DOCK = true;
inline constexpr bool UNDOCK = false;



class SharedParametersDialog : public CommandDialog
{
friend class StylerDlg;
public:
    SharedParametersDialog() = default;
    virtual void updateDlg() = 0;

	// As a page inside the dialog's tabs
	void createPage(HINSTANCE hInst, HWND dialog, const std::function<void(QDialog*)>& setupForm);

protected:
    //Shared data
	static std::wstring _kwlBuffer;
    static UserLangContainer *_pUserLang;
    static ScintillaEditView *_pScintilla;
    intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
	bool setPropertyByCheck(WPARAM id, bool& bool2set);
    virtual void setKeywords2List(int ctrlID) = 0;
};

class FolderStyleDialog : public SharedParametersDialog
{
public:
    FolderStyleDialog() = default;
    void updateDlg() override;
    void initLink();
protected :
    intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
    void setKeywords2List(int ctrlID) override;
private :
	static void retrieve(wchar_t* dest, size_t destMaxLen, const wchar_t* toRetrieve, const wchar_t* prefix);
    URLCtrl _pageLink;
};

class KeyWordsStyleDialog : public SharedParametersDialog
{
public:
    KeyWordsStyleDialog() = default;
    void updateDlg() override;
protected :
    intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
    void setKeywords2List(int id) override;
};

class CommentStyleDialog : public SharedParametersDialog
{
public :
    CommentStyleDialog() = default;
    void updateDlg() override;
protected :
    intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
    void setKeywords2List(int id) override;
private :
	static void retrieve(wchar_t* dest, size_t destMaxLen, const wchar_t* toRetrieve, const wchar_t* prefix);
};

class SymbolsStyleDialog : public SharedParametersDialog
{
public :
    SymbolsStyleDialog() = default;
    void updateDlg() override;
protected :
    intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
    void setKeywords2List(int id) override;
private :
	static void retrieve(wchar_t* dest, size_t destMaxLen, const wchar_t* toRetrieve, const wchar_t* prefix);
};

class UserDefineDialog : public SharedParametersDialog
{
friend class ScintillaEditView;
public :
    UserDefineDialog();
    ~UserDefineDialog() override;
    void init(HINSTANCE hInst, HWND hPere, ScintillaEditView *pSev) {
        if (!_pScintilla)
        {
            Window::init(hInst, hPere);
            _pScintilla = pSev;
        }
    }
	static void setScintilla(ScintillaEditView* pScinView) {
        _pScintilla = pScinView;
    }

    void destroy() override;

    void doDialog(bool willBeShown = true, bool isRTL = false) {
        if (!isCreated())
            createDialog(isRTL);
        display(willBeShown);
    }

    void reloadLangCombo();
    void changeStyle();
    void toggleDock() { onCommand(IDC_DOCK_BUTTON, 0); }
    bool isDocked() const { return _status == DOCK; }
    void setDockStatus(bool isDocked) { _status = isDocked; }
    HWND getFolderHandle() const {
        return _folderStyleDlg.getHSelf();
    }
    HWND getKeywordsHandle() const {
        return _keyWordsStyleDlg.getHSelf();
    }
    HWND getCommentHandle() const {
        return _commentStyleDlg.getHSelf();
    }
    HWND getSymbolHandle() const {
        return _symbolsStyleDlg.getHSelf();
    }
    void setTabName(int index, const wchar_t *name2set);
protected :
    intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
    void onScroll(HWND slider) override;
    void onCancel() override;
private :
    QTabWidget* _tabs = nullptr;
	std::unique_ptr<UserLangContainer> _pCurrentUserLang = nullptr;
    FolderStyleDialog       _folderStyleDlg;
    KeyWordsStyleDialog     _keyWordsStyleDlg;
    CommentStyleDialog      _commentStyleDlg;
    SymbolsStyleDialog      _symbolsStyleDlg;
    bool _status = UNDOCK;
    QRect _dlgPos;

	using Window::init;

    void createDialog(bool isRTL);
    void enableLangAndControlsBy(size_t index);
protected :
    void setKeywords2List(int) override {}
    void updateDlg() override;
};

class StringDlg : public StaticDialog
{
public :
    StringDlg() = default;
    void destroy() override { StaticDialog::destroy(); }
	void init(HINSTANCE hInst, HWND parent, const wchar_t *title, const wchar_t *staticName, const wchar_t *text2Set, int txtLen = 0, const wchar_t* restrictedChars = nullptr, bool bGotoCenter = false) {
		Window::init(hInst, parent);
		_title = title;
		_static = staticName;
		_textValue = text2Set;
		_txtLen = txtLen;
		_shouldGotoCenter = bGotoCenter;
		if (restrictedChars && wcslen(restrictedChars))
		{
			_restrictedChars = restrictedChars;
		}
	}

	// A pointer to the text, or 0 when canceled
	intptr_t doDialog();

private :
    std::wstring _title;
    std::wstring _textValue;
    std::wstring _static;
	std::wstring _restrictedChars;
    int _txtLen = 0;
	bool _shouldGotoCenter = false;

	using Window::init;
};

class StylerDlg : public CommandDialog
{
public:
	StylerDlg(HINSTANCE hInst, HWND parent, int stylerIndex = 0, int enabledNesters = -1)
		: _stylerIndex(stylerIndex), _enabledNesters(enabledNesters)
		, _pFgColour(std::make_unique<ColourPicker>()), _pBgColour(std::make_unique<ColourPicker>())
		, _initialStyle(SharedParametersDialog::_pUserLang->_styles.getStyler(stylerIndex)) {
		Window::init(hInst, parent);
	}

	int doDialog();

protected:
	intptr_t onCommand(WPARAM wParam, LPARAM lParam) override;
	void onCancel() override;

private:
	int _stylerIndex = 0;
	int _enabledNesters = 0;
	std::unique_ptr<ColourPicker> _pFgColour = nullptr;
	std::unique_ptr<ColourPicker> _pBgColour = nullptr;
	Style _initialStyle;
	using Window::init;

	void initControls();
};
