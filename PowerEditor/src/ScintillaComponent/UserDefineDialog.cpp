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


#include "UserDefineDialog.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QScrollArea>
#include <QTextDocument>
#include <QSlider>
#include <QTabWidget>
#include <QTimer>
#include <QValidator>

#include <algorithm>
#include <cstdlib>
#include <cwchar>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <SciLexer.h>

#include "ColourPicker.h"
#include "Common.h"
#include "CustomFileDialog.h"
#include "Notepad_plus_msgs.h"
#include "NppConstants.h"
#include "NppMenu.h"
#include "NppXml.h"
#include "Parameters.h"
#include "ScintillaEditView.h"
#include "StaticDialog.h"
#include "UserDefineResource.h"
#include "colors.h"
#include "localization.h"
#include "menuCmdID.h"
#include "resource.h"
#include "ui_CommentStyleDlg.h"
#include "ui_FolderStyleDlg.h"
#include "ui_GlobalUserdefineDlg.h"
#include "ui_KeywordStyleDlg.h"
#include "ui_StringDlg.h"
#include "ui_StylerPopupDlg.h"
#include "ui_SymbolStyleDlg.h"

using namespace std;

UserLangContainer * SharedParametersDialog::_pUserLang = NULL;
ScintillaEditView * SharedParametersDialog::_pScintilla = NULL;
std::wstring SharedParametersDialog::_kwlBuffer = std::wstring(max_char, L'\0');

static void convertTo(wchar_t* dest, const wchar_t* toConvert, const wchar_t* prefix)
{
	bool inGroup = false;
	size_t index = std::wcslen(dest);
	if (index > 0)
		dest[index++] = ' ';
	dest[index++] = prefix[0];
	dest[index++] = prefix[1];

	for (size_t i = 0, len = std::wcslen(toConvert); i < len && index < max_char - 7; ++i)
	{
		if (i == 0 && toConvert[i] == '(' && toConvert[i + 1] == '(')
		{
			inGroup = true;
		}
		else if (toConvert[i] == ' ' && toConvert[i + 1] == '(' && toConvert[i + 2] == '(')
		{
			inGroup = true;
			dest[index++] = ' ';
			dest[index++] = prefix[0];
			dest[index++] = prefix[1];
			++i;    // skip space
		}

		if (inGroup && toConvert[i - 1] == ')' && toConvert[i - 2] == ')')
		{
			inGroup = false;
		}

		if (toConvert[i] == ' ')
		{
			if (toConvert[i + 1] != ' ' && toConvert[i + 1] != '\0')
			{
				dest[index++] = ' ';
				if (!inGroup)
				{
					dest[index++] = prefix[0];
					dest[index++] = prefix[1];
				}
			}
		}
		else
		{
			dest[index++] = toConvert[i];
		}
	}
	dest[index] = '\0';
}

void SharedParametersDialog::createPage(HINSTANCE hInst, HWND dialog, const std::function<void(QDialog*)>& setupForm)
{
	init(hInst, dialog);

	QDialog* page = createDialogWidget(false);
	setupForm(page);
	page->setWindowFlags(Qt::Widget);

	connectControls();
}

bool SharedParametersDialog::setPropertyByCheck(WPARAM id, bool & bool2set)
{
	bool2set = isCheckedOrNot(static_cast<int>(id));

    if (_pScintilla->getCurrentBuffer()->getLangType() == L_USER)
        _pScintilla->styleChange();

    return TRUE;
}

intptr_t SharedParametersDialog::onCommand(WPARAM wParam, LPARAM /*lParam*/)
{
    if (HIWORD(wParam) == EN_CHANGE)
    {
        setKeywords2List(LOWORD(wParam));

        if (_pScintilla->getCurrentBuffer()->getLangType() == L_USER)
            _pScintilla->styleChange();

        return TRUE;
    }
    return FALSE;
}

intptr_t FolderStyleDialog::onCommand(WPARAM wParam, LPARAM lParam)
{
    switch (wParam)
    {
        case IDC_FOLDER_FOLD_COMPACT :
        {
            return setPropertyByCheck(wParam, _pUserLang->_foldCompact);
        }
        case IDC_FOLDER_IN_CODE1_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_FOLDER_IN_CODE1, SCE_USER_MASK_NESTING_NONE);
            stylerDlg.doDialog();
            return TRUE;
        }
        case IDC_FOLDER_IN_CODE2_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_FOLDER_IN_CODE2, SCE_USER_MASK_NESTING_NONE);
            stylerDlg.doDialog();
            return TRUE;
        }
        case IDC_FOLDER_IN_COMMENT_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_FOLDER_IN_COMMENT, SCE_USER_MASK_NESTING_NONE);
            stylerDlg.doDialog();
            return TRUE;
        }
        case IDC_DEFAULT_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_DEFAULT, SCE_USER_MASK_NESTING_NONE);
            stylerDlg.doDialog();
            return TRUE;
        }
        default :
            return SharedParametersDialog::onCommand(wParam, lParam);
    }
}

void FolderStyleDialog::initLink()
{
	_pageLink.init(_hInst, _hSelf);
	_pageLink.create(dlgItem(IDC_WEB_HELP_LINK), L"https://npp-user-manual.org/docs/user-defined-language-system/");
}

void FolderStyleDialog::setKeywords2List(int ctrlID)
{
	switch (ctrlID)
    {
        case IDC_FOLDER_IN_CODE1_OPEN_EDIT :     
        case IDC_FOLDER_IN_CODE1_MIDDLE_EDIT :   
        case IDC_FOLDER_IN_CODE1_CLOSE_EDIT :    
        case IDC_FOLDER_IN_CODE2_OPEN_EDIT :     
        case IDC_FOLDER_IN_CODE2_MIDDLE_EDIT :   
        case IDC_FOLDER_IN_CODE2_CLOSE_EDIT :    
        case IDC_FOLDER_IN_COMMENT_OPEN_EDIT :   
        case IDC_FOLDER_IN_COMMENT_MIDDLE_EDIT : 
        case IDC_FOLDER_IN_COMMENT_CLOSE_EDIT :  
		{
			_pUserLang->_keywordLists[globalMappper().dialogMapper[ctrlID]] = wstring2string(getItemText(ctrlID));
			break;
		}
    }
}

void FolderStyleDialog::updateDlg()
{
    setChecked(IDC_FOLDER_FOLD_COMPACT, _pUserLang->_foldCompact);

	setItemTextQuietly(IDC_FOLDER_IN_CODE1_OPEN_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_FOLDERS_IN_CODE1_OPEN]));
	setItemTextQuietly(IDC_FOLDER_IN_CODE1_MIDDLE_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_FOLDERS_IN_CODE1_MIDDLE]));
	setItemTextQuietly(IDC_FOLDER_IN_CODE1_CLOSE_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_FOLDERS_IN_CODE1_CLOSE]));
	setItemTextQuietly(IDC_FOLDER_IN_CODE2_OPEN_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_FOLDERS_IN_CODE2_OPEN]));
	setItemTextQuietly(IDC_FOLDER_IN_CODE2_MIDDLE_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_FOLDERS_IN_CODE2_MIDDLE]));
	setItemTextQuietly(IDC_FOLDER_IN_CODE2_CLOSE_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_FOLDERS_IN_CODE2_CLOSE]));
	setItemTextQuietly(IDC_FOLDER_IN_COMMENT_OPEN_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_FOLDERS_IN_COMMENT_OPEN]));
	setItemTextQuietly(IDC_FOLDER_IN_COMMENT_MIDDLE_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_FOLDERS_IN_COMMENT_MIDDLE]));
	setItemTextQuietly(IDC_FOLDER_IN_COMMENT_CLOSE_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_FOLDERS_IN_COMMENT_CLOSE]));
}

void FolderStyleDialog::retrieve(wchar_t* dest, size_t destMaxLen, const wchar_t* toRetrieve, const wchar_t* prefix)
{
    size_t j = 0;
    bool begin2Copy = false;

	for (size_t i = 0, len = std::wcslen(toRetrieve); i < len; ++i)
    {
        if ((i == 0 || (toRetrieve[i-1] == ' ')) && (toRetrieve[i] == prefix[0] && toRetrieve[i+1] == prefix[1]))
        {
            if (j > 0 && j < destMaxLen)
                dest[j++] = ' ';

            begin2Copy = true;
            ++i;
            continue;
        }
		else if (((toRetrieve[i] == ' ') && begin2Copy))
        {
            begin2Copy = false;
        }

        if (begin2Copy && j < destMaxLen)
            dest[j++] = toRetrieve[i];
    }

    if (j < destMaxLen)
        dest[j++] = '\0';
}

intptr_t KeyWordsStyleDialog::onCommand(WPARAM wParam, LPARAM lParam)
{
    switch (wParam)
    {
        case IDC_KEYWORD1_PREFIX_CHECK :
            return setPropertyByCheck(wParam, _pUserLang->_isPrefix[0]);

        case IDC_KEYWORD2_PREFIX_CHECK :
            return setPropertyByCheck(wParam, _pUserLang->_isPrefix[1]);

        case IDC_KEYWORD3_PREFIX_CHECK :
            return setPropertyByCheck(wParam, _pUserLang->_isPrefix[2]);

        case IDC_KEYWORD4_PREFIX_CHECK :
            return setPropertyByCheck(wParam, _pUserLang->_isPrefix[3]);

        case IDC_KEYWORD5_PREFIX_CHECK :
            return setPropertyByCheck(wParam, _pUserLang->_isPrefix[4]);

        case IDC_KEYWORD6_PREFIX_CHECK :
            return setPropertyByCheck(wParam, _pUserLang->_isPrefix[5]);

        case IDC_KEYWORD7_PREFIX_CHECK :
            return setPropertyByCheck(wParam, _pUserLang->_isPrefix[6]);

        case IDC_KEYWORD8_PREFIX_CHECK :
            return setPropertyByCheck(wParam, _pUserLang->_isPrefix[7]);

        case IDC_KEYWORD1_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_KEYWORD1, SCE_USER_MASK_NESTING_NONE);
            stylerDlg.doDialog();
            return TRUE;
        }
        case IDC_KEYWORD2_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_KEYWORD2, SCE_USER_MASK_NESTING_NONE);
            stylerDlg.doDialog();
            return TRUE;
        }
        case IDC_KEYWORD3_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_KEYWORD3, SCE_USER_MASK_NESTING_NONE);
            stylerDlg.doDialog();
            return TRUE;
        }
        case IDC_KEYWORD4_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_KEYWORD4, SCE_USER_MASK_NESTING_NONE);
            stylerDlg.doDialog();
            return TRUE;
        }
        case IDC_KEYWORD5_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_KEYWORD5, SCE_USER_MASK_NESTING_NONE);
            stylerDlg.doDialog();
            return TRUE;
        }
        case IDC_KEYWORD6_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_KEYWORD6, SCE_USER_MASK_NESTING_NONE);
            stylerDlg.doDialog();
            return TRUE;
        }
        case IDC_KEYWORD7_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_KEYWORD7, SCE_USER_MASK_NESTING_NONE);
            stylerDlg.doDialog();
            return TRUE;
        }
        case IDC_KEYWORD8_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_KEYWORD8, SCE_USER_MASK_NESTING_NONE);
            stylerDlg.doDialog();
            return TRUE;
        }
        default :
            return SharedParametersDialog::onCommand(wParam, lParam);
    }
}

void KeyWordsStyleDialog::setKeywords2List(int id)
{
    switch (id)
    {
        case IDC_KEYWORD1_EDIT :
        case IDC_KEYWORD2_EDIT :
        case IDC_KEYWORD3_EDIT :
        case IDC_KEYWORD4_EDIT :
        case IDC_KEYWORD5_EDIT :
        case IDC_KEYWORD6_EDIT :
        case IDC_KEYWORD7_EDIT :
        case IDC_KEYWORD8_EDIT :
		{
			_pUserLang->_keywordLists[globalMappper().dialogMapper[id]] = wstring2string(getItemText(id));
			break;
		}
    }
}

void KeyWordsStyleDialog::updateDlg()
{
	setItemTextQuietly(IDC_KEYWORD1_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_KEYWORDS1]));
	setItemTextQuietly(IDC_KEYWORD2_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_KEYWORDS2]));
	setItemTextQuietly(IDC_KEYWORD3_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_KEYWORDS3]));
	setItemTextQuietly(IDC_KEYWORD4_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_KEYWORDS4]));
	setItemTextQuietly(IDC_KEYWORD5_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_KEYWORDS5]));
	setItemTextQuietly(IDC_KEYWORD6_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_KEYWORDS6]));
	setItemTextQuietly(IDC_KEYWORD7_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_KEYWORDS7]));
	setItemTextQuietly(IDC_KEYWORD8_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_KEYWORDS8]));

    setChecked(IDC_KEYWORD1_PREFIX_CHECK, _pUserLang->_isPrefix[0]);
    setChecked(IDC_KEYWORD2_PREFIX_CHECK, _pUserLang->_isPrefix[1]);
    setChecked(IDC_KEYWORD3_PREFIX_CHECK, _pUserLang->_isPrefix[2]);
    setChecked(IDC_KEYWORD4_PREFIX_CHECK, _pUserLang->_isPrefix[3]);
    setChecked(IDC_KEYWORD5_PREFIX_CHECK, _pUserLang->_isPrefix[4]);
    setChecked(IDC_KEYWORD6_PREFIX_CHECK, _pUserLang->_isPrefix[5]);
    setChecked(IDC_KEYWORD7_PREFIX_CHECK, _pUserLang->_isPrefix[6]);
    setChecked(IDC_KEYWORD8_PREFIX_CHECK, _pUserLang->_isPrefix[7]);
}

intptr_t CommentStyleDialog::onCommand(WPARAM wParam, LPARAM lParam)
{
    switch (wParam)
    {
        case IDC_FOLDING_OF_COMMENTS :
        {
            return setPropertyByCheck(wParam, _pUserLang->_allowFoldOfComments);
        }
        case IDC_ALLOW_ANYWHERE :
        case IDC_FORCE_AT_BOL :
        case IDC_ALLOW_WHITESPACE :
        {
            if (isCheckedOrNot(IDC_ALLOW_ANYWHERE))
                _pUserLang->_forcePureLC = PURE_LC_NONE;
            else if (isCheckedOrNot(IDC_FORCE_AT_BOL))
                _pUserLang->_forcePureLC = PURE_LC_BOL;
            else if (isCheckedOrNot(IDC_ALLOW_WHITESPACE))
                _pUserLang->_forcePureLC = PURE_LC_WSP;

            if (_pScintilla->getCurrentBuffer()->getLangType() == L_USER)
                _pScintilla->styleChange();

            return TRUE;
        }
        case IDC_DOT_RADIO :
        case IDC_COMMA_RADIO :
        case IDC_BOTH_RADIO :
        {
            if (isCheckedOrNot(IDC_DOT_RADIO))
                _pUserLang->_decimalSeparator = DECSEP_DOT;
            else if (isCheckedOrNot(IDC_COMMA_RADIO))
                _pUserLang->_decimalSeparator = DECSEP_COMMA;
            else if (isCheckedOrNot(IDC_BOTH_RADIO))
                _pUserLang->_decimalSeparator = DECSEP_BOTH;

            if (_pScintilla->getCurrentBuffer()->getLangType() == L_USER)
                _pScintilla->styleChange();

            return TRUE;
        }
        case IDC_COMMENTLINE_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_COMMENTLINE);
            stylerDlg.doDialog();
            return TRUE;
        }
        case IDC_COMMENT_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_COMMENT);
            stylerDlg.doDialog();
            return TRUE;
        }
        case IDC_NUMBER_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_NUMBER, SCE_USER_MASK_NESTING_NONE);
            stylerDlg.doDialog();
            return TRUE;
        }
        default :
            return SharedParametersDialog::onCommand(wParam, lParam);
    }
}

void CommentStyleDialog::setKeywords2List(int id)
{
    int index = 0;
    switch (id)
    {
        case IDC_NUMBER_PREFIX1_EDIT :
        case IDC_NUMBER_PREFIX2_EDIT :
        case IDC_NUMBER_EXTRAS1_EDIT :
        case IDC_NUMBER_EXTRAS2_EDIT :
        case IDC_NUMBER_SUFFIX1_EDIT :
        case IDC_NUMBER_SUFFIX2_EDIT :
        case IDC_NUMBER_RANGE_EDIT :  
		{
			_pUserLang->_keywordLists[globalMappper().dialogMapper[id]] = wstring2string(getItemText(id));
			break;
		}

        case IDC_COMMENT_OPEN_EDIT :
        case IDC_COMMENT_CLOSE_EDIT :
        case IDC_COMMENTLINE_OPEN_EDIT :
        case IDC_COMMENTLINE_CONTINUE_EDIT :
        case IDC_COMMENTLINE_CLOSE_EDIT :
            index = SCE_USER_KWLIST_COMMENTS;
            break;
        default : index = -1;
    }
    if (index != -1)
    {
		auto newList = std::wstring(max_char, L'\0');

		static constexpr int list[]{
            IDC_COMMENTLINE_OPEN_EDIT,
            IDC_COMMENTLINE_CONTINUE_EDIT,
            IDC_COMMENTLINE_CLOSE_EDIT,
            IDC_COMMENT_OPEN_EDIT,
            IDC_COMMENT_CLOSE_EDIT
        };

        wchar_t intBuffer[10] = { '0', 0 };
        for (int i = 0; static_cast<size_t>(i) < sizeof(list) / sizeof(int); ++i)
        {
            swprintf(intBuffer + 1, 9, L"%d", i);
			convertTo(newList.data(), getItemText(list[i]).substr(0, max_char - 1).c_str(), intBuffer);
        }

		_pUserLang->_keywordLists[index] = wstring2string(newList).c_str();
    }
}

void CommentStyleDialog::retrieve(wchar_t* dest, size_t destMaxLen, const wchar_t* toRetrieve, const wchar_t* prefix)
{
    size_t j = 0;
    bool begin2Copy = false;
    bool inGroup = false;

    for (size_t i = 0, len = std::wcslen(toRetrieve); i < len; ++i)
    {
        if ((i == 0 || (toRetrieve[i - 1] == ' ')) && (toRetrieve[i] == prefix[0] && toRetrieve[i + 1] == prefix[1]))
        {
            if (j > 0 && j < destMaxLen)
                dest[j++] = ' ';

            begin2Copy = true;
            ++i;
            continue;
        }

        if (toRetrieve[i] == '(' && toRetrieve[i + 1] == '(' && !inGroup && begin2Copy)
        {
            inGroup = true;
        }

        if (toRetrieve[i] != ')' && toRetrieve[i - 1] == ')' && toRetrieve[i - 2] == ')' && inGroup)
        {
            inGroup = false;
        }

        if (toRetrieve[i] == ' ' && begin2Copy)
        {
            begin2Copy = false;
        }

        if (j < destMaxLen && (begin2Copy || inGroup))
            dest[j++] = toRetrieve[i];
    }

    if (j < destMaxLen)
        dest[j++] = '\0';
}

void CommentStyleDialog::updateDlg()
{
	static constexpr int list[]{
        IDC_COMMENTLINE_OPEN_EDIT,
        IDC_COMMENTLINE_CONTINUE_EDIT,
        IDC_COMMENTLINE_CLOSE_EDIT,
        IDC_COMMENT_OPEN_EDIT,
        IDC_COMMENT_CLOSE_EDIT
    };

    wchar_t intBuffer[10] = { '0', 0 };
    for (int i = 0; static_cast<size_t>(i) < sizeof(list) / sizeof(int); ++i)
    {
        swprintf(intBuffer + 1, 9, L"%d", i);
		retrieve(_kwlBuffer.data(), _kwlBuffer.capacity(), string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_COMMENTS]).c_str(), intBuffer);
		setItemTextQuietly(list[i], _kwlBuffer.c_str());
    }

    setChecked(IDC_FOLDING_OF_COMMENTS, _pUserLang->_allowFoldOfComments);

    setChecked(IDC_ALLOW_ANYWHERE, _pUserLang->_forcePureLC == PURE_LC_NONE);
    setChecked(IDC_FORCE_AT_BOL, _pUserLang->_forcePureLC == PURE_LC_BOL);
    setChecked(IDC_ALLOW_WHITESPACE, _pUserLang->_forcePureLC == PURE_LC_WSP);

    setChecked(IDC_DOT_RADIO, _pUserLang->_decimalSeparator == DECSEP_DOT);
    setChecked(IDC_COMMA_RADIO, _pUserLang->_decimalSeparator == DECSEP_COMMA);
    setChecked(IDC_BOTH_RADIO, _pUserLang->_decimalSeparator == DECSEP_BOTH);

	setItemTextQuietly(IDC_NUMBER_PREFIX1_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_NUMBER_PREFIX1]));
	setItemTextQuietly(IDC_NUMBER_PREFIX2_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_NUMBER_PREFIX2]));
	setItemTextQuietly(IDC_NUMBER_EXTRAS1_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_NUMBER_EXTRAS1]));
	setItemTextQuietly(IDC_NUMBER_EXTRAS2_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_NUMBER_EXTRAS2]));
	setItemTextQuietly(IDC_NUMBER_SUFFIX1_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_NUMBER_SUFFIX1]));
	setItemTextQuietly(IDC_NUMBER_SUFFIX2_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_NUMBER_SUFFIX2]));
	setItemTextQuietly(IDC_NUMBER_RANGE_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_NUMBER_RANGE]));
}

void SymbolsStyleDialog::updateDlg()
{
	static constexpr int list[]{
        IDC_DELIMITER1_BOUNDARYOPEN_EDIT,
        IDC_DELIMITER1_ESCAPE_EDIT,
        IDC_DELIMITER1_BOUNDARYCLOSE_EDIT,
        IDC_DELIMITER2_BOUNDARYOPEN_EDIT,
        IDC_DELIMITER2_ESCAPE_EDIT,
        IDC_DELIMITER2_BOUNDARYCLOSE_EDIT,
        IDC_DELIMITER3_BOUNDARYOPEN_EDIT,
        IDC_DELIMITER3_ESCAPE_EDIT,
        IDC_DELIMITER3_BOUNDARYCLOSE_EDIT,
        IDC_DELIMITER4_BOUNDARYOPEN_EDIT,
        IDC_DELIMITER4_ESCAPE_EDIT,
        IDC_DELIMITER4_BOUNDARYCLOSE_EDIT,
        IDC_DELIMITER5_BOUNDARYOPEN_EDIT,
        IDC_DELIMITER5_ESCAPE_EDIT,
        IDC_DELIMITER5_BOUNDARYCLOSE_EDIT,
        IDC_DELIMITER6_BOUNDARYOPEN_EDIT,
        IDC_DELIMITER6_ESCAPE_EDIT,
        IDC_DELIMITER6_BOUNDARYCLOSE_EDIT,
        IDC_DELIMITER7_BOUNDARYOPEN_EDIT,
        IDC_DELIMITER7_ESCAPE_EDIT,
        IDC_DELIMITER7_BOUNDARYCLOSE_EDIT,
        IDC_DELIMITER8_BOUNDARYOPEN_EDIT,
        IDC_DELIMITER8_ESCAPE_EDIT,
        IDC_DELIMITER8_BOUNDARYCLOSE_EDIT
    };
    wchar_t intBuffer[10] = {'0', 0};

    for (int i = 0; static_cast<size_t>(i) < sizeof(list)/sizeof(int); ++i)
    {
        if (i < 10)
            swprintf(intBuffer + 1, 9, L"%d", i);
        else
            swprintf(intBuffer, 10, L"%d", i);

		retrieve(_kwlBuffer.data(), _kwlBuffer.capacity(), string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_DELIMITERS]).c_str(), intBuffer);
		setItemTextQuietly(list[i], _kwlBuffer.c_str());
    }

	setItemTextQuietly(IDC_OPERATOR1_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_OPERATORS1]));
	setItemTextQuietly(IDC_OPERATOR2_EDIT, string2wstring(_pUserLang->_keywordLists[SCE_USER_KWLIST_OPERATORS2]));
}

intptr_t SymbolsStyleDialog::onCommand(WPARAM wParam, LPARAM lParam)
{
    switch (wParam)
    {
        case IDC_OPERATOR_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_OPERATOR, SCE_USER_MASK_NESTING_NONE);
            stylerDlg.doDialog();
            return TRUE;
        }
        case IDC_DELIMITER1_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_DELIMITER1);
            stylerDlg.doDialog();
            return TRUE;
        }
        case IDC_DELIMITER2_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_DELIMITER2);
            stylerDlg.doDialog();
            return TRUE;
        }
        case IDC_DELIMITER3_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_DELIMITER3);
            stylerDlg.doDialog();
            return TRUE;
        }
        case IDC_DELIMITER4_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_DELIMITER4);
            stylerDlg.doDialog();
            return TRUE;
        }
        case IDC_DELIMITER5_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_DELIMITER5);
            stylerDlg.doDialog();
            return TRUE;
        }
        case IDC_DELIMITER6_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_DELIMITER6);
            stylerDlg.doDialog();
            return TRUE;
        }
        case IDC_DELIMITER7_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_DELIMITER7);
            stylerDlg.doDialog();
            return TRUE;
        }
        case IDC_DELIMITER8_STYLER :
        {
            StylerDlg stylerDlg(_hInst, _hSelf, SCE_USER_STYLE_DELIMITER8);
            stylerDlg.doDialog();
            return TRUE;
        }
        default :
            return SharedParametersDialog::onCommand(wParam, lParam);
    }
}


void SymbolsStyleDialog::retrieve(wchar_t* dest, size_t destMaxLen, const wchar_t* toRetrieve, const wchar_t* prefix)
{
    size_t j = 0;
    bool begin2Copy = false;
    bool inGroup = false;

	for (size_t i = 0, len = std::wcslen(toRetrieve); i < len; ++i)
    {
        if ((i == 0 || (toRetrieve[i-1] == ' ')) && (toRetrieve[i] == prefix[0] && toRetrieve[i+1] == prefix[1]))
        {
            if (j > 0 && j < destMaxLen)
                dest[j++] = ' ';

            begin2Copy = true;
            ++i;
            continue;
        }

		if (toRetrieve[i] == '(' && toRetrieve[i + 1] == '(' && !inGroup && begin2Copy)
        {
            inGroup = true;
        }

		if (toRetrieve[i] != ')' && toRetrieve[i - 1] == ')' && toRetrieve[i - 2] == ')' && inGroup)
        {
            inGroup = false;
        }

		if (toRetrieve[i] == ' ' && begin2Copy)
        {
            begin2Copy = false;
        }

        if (j < destMaxLen && (begin2Copy || inGroup))
            dest[j++] = toRetrieve[i];
    }
    if (j < destMaxLen)
        dest[j++] = '\0';
}

void SymbolsStyleDialog::setKeywords2List(int id)
{
    switch (id)
    {
		case IDC_OPERATOR1_EDIT:
		{
			_pUserLang->_keywordLists[SCE_USER_KWLIST_OPERATORS1] = wstring2string(getItemText(id));
			break;
		}

		case IDC_OPERATOR2_EDIT:
		{
			_pUserLang->_keywordLists[SCE_USER_KWLIST_OPERATORS2] = wstring2string(getItemText(id));
			break;
		}

        case IDC_DELIMITER1_BOUNDARYOPEN_EDIT :
        case IDC_DELIMITER1_ESCAPE_EDIT :
        case IDC_DELIMITER1_BOUNDARYCLOSE_EDIT :
        case IDC_DELIMITER2_BOUNDARYOPEN_EDIT :
        case IDC_DELIMITER2_ESCAPE_EDIT :
        case IDC_DELIMITER2_BOUNDARYCLOSE_EDIT :
        case IDC_DELIMITER3_BOUNDARYOPEN_EDIT :
        case IDC_DELIMITER3_ESCAPE_EDIT :
        case IDC_DELIMITER3_BOUNDARYCLOSE_EDIT :
        case IDC_DELIMITER4_BOUNDARYOPEN_EDIT :
        case IDC_DELIMITER4_ESCAPE_EDIT :
        case IDC_DELIMITER4_BOUNDARYCLOSE_EDIT :
        case IDC_DELIMITER5_BOUNDARYOPEN_EDIT :
        case IDC_DELIMITER5_ESCAPE_EDIT :
        case IDC_DELIMITER5_BOUNDARYCLOSE_EDIT :
        case IDC_DELIMITER6_BOUNDARYOPEN_EDIT :
        case IDC_DELIMITER6_ESCAPE_EDIT :
        case IDC_DELIMITER6_BOUNDARYCLOSE_EDIT :
        case IDC_DELIMITER7_BOUNDARYOPEN_EDIT :
        case IDC_DELIMITER7_ESCAPE_EDIT :
        case IDC_DELIMITER7_BOUNDARYCLOSE_EDIT :
        case IDC_DELIMITER8_BOUNDARYOPEN_EDIT :
        case IDC_DELIMITER8_ESCAPE_EDIT :
        case IDC_DELIMITER8_BOUNDARYCLOSE_EDIT :
        {
			auto newList = std::wstring(max_char, L'\0');
            wchar_t intBuffer[10] = {'0', 0};

			static constexpr int list[]{
                IDC_DELIMITER1_BOUNDARYOPEN_EDIT,
                IDC_DELIMITER1_ESCAPE_EDIT,
                IDC_DELIMITER1_BOUNDARYCLOSE_EDIT,
                IDC_DELIMITER2_BOUNDARYOPEN_EDIT,
                IDC_DELIMITER2_ESCAPE_EDIT,
                IDC_DELIMITER2_BOUNDARYCLOSE_EDIT,
                IDC_DELIMITER3_BOUNDARYOPEN_EDIT,
                IDC_DELIMITER3_ESCAPE_EDIT,
                IDC_DELIMITER3_BOUNDARYCLOSE_EDIT,
                IDC_DELIMITER4_BOUNDARYOPEN_EDIT,
                IDC_DELIMITER4_ESCAPE_EDIT,
                IDC_DELIMITER4_BOUNDARYCLOSE_EDIT,
                IDC_DELIMITER5_BOUNDARYOPEN_EDIT,
                IDC_DELIMITER5_ESCAPE_EDIT,
                IDC_DELIMITER5_BOUNDARYCLOSE_EDIT,
                IDC_DELIMITER6_BOUNDARYOPEN_EDIT,
                IDC_DELIMITER6_ESCAPE_EDIT,
                IDC_DELIMITER6_BOUNDARYCLOSE_EDIT,
                IDC_DELIMITER7_BOUNDARYOPEN_EDIT,
                IDC_DELIMITER7_ESCAPE_EDIT,
                IDC_DELIMITER7_BOUNDARYCLOSE_EDIT,
                IDC_DELIMITER8_BOUNDARYOPEN_EDIT,
                IDC_DELIMITER8_ESCAPE_EDIT,
                IDC_DELIMITER8_BOUNDARYCLOSE_EDIT
            };

            for (int i = 0; static_cast<size_t>(i) < sizeof(list)/sizeof(int); ++i)
            {
                if (i < 10)
                    swprintf(intBuffer + 1, 9, L"%d", i);
                else
                    swprintf(intBuffer, 10, L"%d", i);

                int dd = list[i];
				convertTo(newList.data(), getItemText(dd).substr(0, max_char - 1).c_str(), intBuffer);
            }

			_pUserLang->_keywordLists[SCE_USER_KWLIST_DELIMITERS] = wstring2string(newList).c_str();
            break;
        }
        default :
            break;
    }
}

UserDefineDialog::UserDefineDialog() : SharedParametersDialog(), _pCurrentUserLang(std::make_unique<UserLangContainer>())
{
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_DEFAULT,              string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_DEFAULT]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_COMMENT,              string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_COMMENT]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_COMMENTLINE,          string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_COMMENTLINE]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_NUMBER,               string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_NUMBER]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_KEYWORD1,             string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_KEYWORD1]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_KEYWORD2,             string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_KEYWORD2]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_KEYWORD3,             string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_KEYWORD3]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_KEYWORD4,             string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_KEYWORD4]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_KEYWORD5,             string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_KEYWORD5]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_KEYWORD6,             string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_KEYWORD6]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_KEYWORD7,             string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_KEYWORD7]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_KEYWORD8,             string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_KEYWORD8]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_OPERATOR,             string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_OPERATOR]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_FOLDER_IN_CODE1,      string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_FOLDER_IN_CODE1]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_FOLDER_IN_CODE2,      string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_FOLDER_IN_CODE2]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_FOLDER_IN_COMMENT,    string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_FOLDER_IN_COMMENT]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_DELIMITER1,           string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_DELIMITER1]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_DELIMITER2,           string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_DELIMITER2]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_DELIMITER3,           string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_DELIMITER3]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_DELIMITER4,           string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_DELIMITER4]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_DELIMITER5,           string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_DELIMITER5]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_DELIMITER6,           string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_DELIMITER6]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_DELIMITER7,           string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_DELIMITER7]).c_str());
	_pCurrentUserLang->_styles.addStyler(SCE_USER_STYLE_DELIMITER8,           string2wstring(globalMappper().styleNameMapper[SCE_USER_STYLE_DELIMITER8]).c_str());
}

UserDefineDialog::~UserDefineDialog()
{
    _pCurrentUserLang.reset(nullptr);
}

void UserDefineDialog::reloadLangCombo()
{
	NppParameters& nppParam = NppParameters::getInstance();
	combo(IDC_LANGNAME_COMBO)->clear();
	addComboItem(IDC_LANGNAME_COMBO, L"User Defined Language");
	for (int i = 0, nb = nppParam.getNbUserLang(); i < nb ; ++i)
	{
		const UserLangContainer* userLangContainer = nppParam.getULCFromIndex(i);
		addComboItem(IDC_LANGNAME_COMBO, userLangContainer->getName());
	}
}

void UserDefineDialog::setTabName(int index, const wchar_t *name2set)
{
	_tabs->setTabText(index, QString::fromWCharArray(name2set));
}

void UserDefineDialog::changeStyle()
{
	_status = !_status;
	NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
	NppXml::Element targetNode{};
	if (pNativeSpeaker->getNativeLang())
	{
		targetNode = NppXml::firstChildElement(pNativeSpeaker->getNativeLang(), "Dialog");
		if (targetNode)
			targetNode = NppXml::firstChildElement(targetNode, "UserDefine");
	}
	wstring dockButtonLabel;
	wstring defauleLabel;
	string idStr;
	if (_status == DOCK)
	{
		defauleLabel = L"Undock";
		idStr = std::to_string(IDC_UNDOCK_BUTTON);
	}
	else
	{
		defauleLabel = L"Dock";
		idStr = std::to_string(IDC_DOCK_BUTTON);
	}

	dockButtonLabel = pNativeSpeaker->getAttrNameByIdStr(defauleLabel.c_str(), targetNode, idStr.c_str());
	setItemText(IDC_DOCK_BUTTON, dockButtonLabel);

	if (_status == DOCK)
	{
		_dlgPos = _hSelf->geometry();
		_hSelf->setWindowFlags(Qt::Widget);
	}
	else
	{
		_hSelf->setParent(_hParent, Qt::Dialog);
		if (_dlgPos.isValid())
			_hSelf->setGeometry(_dlgPos);
	}
}

void UserDefineDialog::enableLangAndControlsBy(size_t index)
{
	_pUserLang = (index == 0) ? _pCurrentUserLang.get() : (NppParameters::getInstance().getULCFromIndex(index - 1));
	if (index != 0)
		setItemTextQuietly(IDC_EXT_EDIT, _pUserLang->_ext);

	showItem(IDC_EXT_STATIC, index != 0);
	showItem(IDC_EXT_EDIT, index != 0);
	showItem(IDC_RENAME_BUTTON, index != 0);
	showItem(IDC_REMOVELANG_BUTTON, index != 0);
}

void UserDefineDialog::updateDlg()
{
	const int i = combo(IDC_LANGNAME_COMBO)->currentIndex();
	if (i > 0) // the first menu item is generic UDL
	{
		NppParameters& nppParam = NppParameters::getInstance();
		nppParam.setUdlXmlDirtyFromIndex(static_cast<size_t>(i) - 1);
	}

	setChecked(IDC_LANGNAME_IGNORECASE_CHECK, _pUserLang->_isCaseIgnored);
	_folderStyleDlg.updateDlg();
	_keyWordsStyleDlg.updateDlg();
	_commentStyleDlg.updateDlg();
	_symbolsStyleDlg.updateDlg();
}

void UserDefineDialog::destroy()
{
	if (!_hSelf)
		return;

	_folderStyleDlg.destroy();
	_keyWordsStyleDlg.destroy();
	_commentStyleDlg.destroy();
	_symbolsStyleDlg.destroy();
	StaticDialog::destroy();
	_tabs = nullptr;
}

void UserDefineDialog::createDialog(bool isRTL)
{
	NppParameters& nppParam = NppParameters::getInstance();

	QDialog* dialog = createDialogWidget(isRTL);
	Ui::GlobalUserdefineDlg ui;
	ui.setupUi(dialog);

	_tabs = ui.pages;
	_pUserLang = _pCurrentUserLang.get();

	_folderStyleDlg.createPage(_hInst, dialog, [](QDialog* page) { Ui::FolderStyleDlg form; form.setupUi(page); });
	_keyWordsStyleDlg.createPage(_hInst, dialog, [](QDialog* page) { Ui::KeywordStyleDlg form; form.setupUi(page); });
	_commentStyleDlg.createPage(_hInst, dialog, [](QDialog* page) { Ui::CommentStyleDlg form; form.setupUi(page); });
	_symbolsStyleDlg.createPage(_hInst, dialog, [](QDialog* page) { Ui::SymbolStyleDlg form; form.setupUi(page); });

	const auto addTab = [this](SharedParametersDialog& page, const wchar_t* name, int linesPerEdit)
	{
		// A text box asks for far more height than these need, which only makes the page scroll
		const QList<QPlainTextEdit*> edits = page.getHSelf()->findChildren<QPlainTextEdit*>();
		for (QPlainTextEdit* edit : edits)
			edit->setFixedHeight(edit->fontMetrics().lineSpacing() * linesPerEdit + 2 * (edit->frameWidth() + static_cast<int>(edit->document()->documentMargin())));

		// When docked the dialog can be made smaller than its pages
		auto* area = new QScrollArea(_tabs);
		area->setWidgetResizable(true);
		area->setFrameShape(QFrame::NoFrame);
		area->setWidget(page.getHSelf());
		_tabs->addTab(area, QString::fromWCharArray(name));
	};
	addTab(_folderStyleDlg, L"Folder && Default", 2);
	addTab(_keyWordsStyleDlg, L"Keywords Lists", 3);
	addTab(_commentStyleDlg, L"Comment && Number", 2);
	addTab(_symbolsStyleDlg, L"Operators && Delimiters", 2);

	_folderStyleDlg.initLink();

	reloadLangCombo();
	combo(IDC_LANGNAME_COMBO)->setCurrentIndex(0);

	enableLangAndControlsBy(0);
	updateDlg();

	const bool isTransparentAvailable = NppParameters::isWindowOpacityAvailable();
	showItem(IDC_UD_TRANSPARENT_CHECK, isTransparentAvailable);
	showItem(IDC_UD_PERCENTAGE_SLIDER, isTransparentAvailable);
	slider(IDC_UD_PERCENTAGE_SLIDER)->setRange(20, 200);
	setSliderPos(IDC_UD_PERCENTAGE_SLIDER, 150);
	enableItem(IDC_UD_PERCENTAGE_SLIDER, false);

	wstring udlVersion = L"User Defined Language v.";
	udlVersion += to_wstring(SCE_UDL_VERSION_MAJOR);
	udlVersion += L".";
	udlVersion += to_wstring(SCE_UDL_VERSION_MINOR);
	udlVersion += L".";
	udlVersion += to_wstring(SCE_UDL_VERSION_BUILD);
	udlVersion += L".";
	udlVersion += to_wstring(SCE_UDL_VERSION_REVISION);

	dialog->setWindowTitle(QString::fromStdWString(udlVersion));

	connectControls();
}

void UserDefineDialog::onScroll(HWND scale)
{
	if (scale == dlgItem(IDC_UD_PERCENTAGE_SLIDER))
		NppParameters::setTransparent(_hSelf, slider(IDC_UD_PERCENTAGE_SLIDER)->value());
}

void UserDefineDialog::onCancel()
{
	::SendMessage(_hParent, WM_CLOSE_USERDEFINE_DLG, 0, 0);
	display(false);
}

intptr_t UserDefineDialog::onCommand(WPARAM wParam, LPARAM /*lParam*/)
{
	NppParameters& nppParam = NppParameters::getInstance();
	NativeLangSpeaker * pNativeSpeaker = nppParam.getNativeLangSpeaker();

	if (HIWORD(wParam) == EN_CHANGE && LOWORD(wParam) == IDC_EXT_EDIT)
	{
	    _pUserLang->_ext = getItemText(IDC_EXT_EDIT);
	    return TRUE;
	}
	else if (HIWORD(wParam) == CBN_SELCHANGE) // HIWORD(wParam) for accelerators is also 1
	{
	    switch (LOWORD(wParam))
	    {
	        case IDC_LANGNAME_COMBO:
	        {
	            auto i = combo(IDC_LANGNAME_COMBO)->currentIndex();
	            enableLangAndControlsBy(i);
	            updateDlg();
	            return TRUE;
	        }

	        default:
	            return FALSE;
	    }
	}
	else
	{
	    switch (LOWORD(wParam))
	    {
	        case IDC_DOCK_BUTTON :
	        {
	            int msg = WM_UNDOCK_USERDEFINE_DLG;

	            if (_status == UNDOCK)
	            {
	                if (nppParam.isTransparentAvailable())
	                {
	                    nppParam.removeTransparent(_hSelf);
	                    showItem(IDC_UD_TRANSPARENT_CHECK, false);
	                    showItem(IDC_UD_PERCENTAGE_SLIDER, false);
	                }
	                msg = WM_DOCK_USERDEFINE_DLG;
	            }

	            changeStyle();

	            if (_status == UNDOCK)
	            {
	                if (nppParam.isTransparentAvailable())
	                {
	                    bool isChecked = (isCheckedOrNot(IDC_UD_TRANSPARENT_CHECK));
	                    if (isChecked)
	                    {
									const auto percent = slider(IDC_UD_PERCENTAGE_SLIDER)->value();
	                        nppParam.setTransparent(_hSelf, percent);
	                    }
	                    showItem(IDC_UD_TRANSPARENT_CHECK, true);
	                    showItem(IDC_UD_PERCENTAGE_SLIDER, true);
	                }
	            }
	            ::SendMessage(_hParent, msg, 0, 0);
	            return TRUE;
	        }

	        case IDC_REMOVELANG_BUTTON :
	        {
	            int result = nppParam.getNativeLangSpeaker()->messageBox("UDLRemoveCurrentLang",
							_hSelf,
							L"Are you sure?",
							L"Remove the current language",
							MB_YESNO);

	            if (result == IDYES)
	            {
	                auto i = combo(IDC_LANGNAME_COMBO)->currentIndex();
							const wstring langName = comboText(IDC_LANGNAME_COMBO, i);

	                //remove current language from combobox
	                combo(IDC_LANGNAME_COMBO)->removeItem(i);
	                combo(IDC_LANGNAME_COMBO)->setCurrentIndex(i-1);
							onCommand(MAKEWPARAM(IDC_LANGNAME_COMBO, CBN_SELCHANGE), 0);

	                //remove current language from userLangArray
	                nppParam.removeUserLang(i-1);

	                //remove current language from langMenu
	                HWND hNpp = _hParent;
							HMENU m = reinterpret_cast<HMENU>(::SendMessage(hNpp, NPPM_INTERNAL_GETMENU, 0, 0));
							HMENU subMenu = nppSubMenuAt(m, MENUINDEX_LANGUAGE);
							nppRemoveMenuCommand(subMenu, static_cast<int>(IDM_LANG_USER + i));
							::SendMessage(_hParent, WM_REMOVE_USERLANG, 0, reinterpret_cast<LPARAM>(langName.c_str()));
	            }
	            return TRUE;
	        }
	        case IDC_RENAME_BUTTON :
	        {
	            auto i = combo(IDC_LANGNAME_COMBO)->currentIndex();
						const wstring langName = comboText(IDC_LANGNAME_COMBO, i);

						wstring strName = pNativeSpeaker->getLocalizedStrFromID("common-name", L"Name");
						wstring strTitle = pNativeSpeaker->getLocalizedStrFromID("userdefined-title-rename", L"Rename Current Language Name");

	            StringDlg strDlg;
	            strDlg.init(_hInst, _hSelf, strTitle.c_str(), strName.c_str(), langName.c_str(), langNameLenMax - 1);

						auto* newName = reinterpret_cast<wchar_t*>(strDlg.doDialog());

	            if (newName)
	            {
	                if (nppParam.isExistingUserLangName(newName))
	                {
								nppParam.getNativeLangSpeaker()->messageBox("UDLNewNameError",
									_hSelf,
									L"This name is used by another language,\rplease give another one.",
									L"UDL Error",
									MB_OK);
	                    QTimer::singleShot(0, _hSelf, [this]() { onCommand(IDC_RENAME_BUTTON, 0); });
	                    return TRUE;
	                }
	                if (nppParam.getNbUserLang() >= NB_MAX_USER_LANG)
	                    return TRUE;

	                //rename current language name in combobox
	                combo(IDC_LANGNAME_COMBO)->removeItem(i);
							combo(IDC_LANGNAME_COMBO)->insertItem(i, QString::fromWCharArray(newName));
	                combo(IDC_LANGNAME_COMBO)->setCurrentIndex(i);

	                //rename current language name in userLangArray
							UserLangContainer* userLangContainer = nppParam.getULCFromIndex(i - 1);
							userLangContainer->_name = newName;

	                //rename current language name in langMenu
	                HWND hNpp = _hParent;
							HMENU hM = reinterpret_cast<HMENU>(::SendMessage(hNpp, NPPM_INTERNAL_GETMENU, 0, 0));
							HMENU hSubM = nppSubMenuAt(hM, MENUINDEX_LANGUAGE);
	                nppSetMenuCommandText(hSubM, static_cast<int>(IDM_LANG_USER + i), newName);
							::SendMessage(_hParent, WM_RENAME_USERLANG, reinterpret_cast<WPARAM>(newName), reinterpret_cast<LPARAM>(langName.c_str()));
	            }

	            return TRUE;
	        }

	        case IDC_ADDNEW_BUTTON :
	        case IDC_SAVEAS_BUTTON :
	        {
	            auto i = combo(IDC_LANGNAME_COMBO)->currentIndex();
	            if (i == 0)
	                wParam = IDC_ADDNEW_BUTTON;


						wstring strName = pNativeSpeaker->getLocalizedStrFromID("common-name", L"Name");
						wstring strTitle = (wParam == IDC_SAVEAS_BUTTON) ?
							pNativeSpeaker->getLocalizedStrFromID("userdefined-title-save", L"Save Current Language Name As...") :
							pNativeSpeaker->getLocalizedStrFromID("userdefined-title-new", L"Create New Language...");

	            StringDlg strDlg;
						strDlg.init(_hInst, _hSelf, strTitle.c_str(), strName.c_str(), L"", langNameLenMax - 1);

	            wchar_t *tmpName = reinterpret_cast<wchar_t *>(strDlg.doDialog());

	            if (tmpName && tmpName[0])
	            {
	                wstring newNameString(tmpName);
	                const wchar_t *newName = newNameString.c_str();

	                if (nppParam.isExistingUserLangName(newName))
	                {
								pNativeSpeaker->messageBox("UDLNewNameError",
									_hSelf,
									L"This name is used by another language,\rplease give another one.",
									L"UDL Error",
									MB_OK);
	                    QTimer::singleShot(0, _hSelf, [this]() { onCommand(IDC_RENAME_BUTTON, 0); });
	                    return TRUE;
	                }
	                if (nppParam.getNbUserLang() >= NB_MAX_USER_LANG)
	                    return TRUE;

	                //add current language in userLangArray at the end as a new lang
							const UserLangContainer* userLang = (wParam == IDC_SAVEAS_BUTTON) ? nppParam.getULCFromIndex(i - 1) : _pCurrentUserLang.get();
	                int newIndex = nppParam.addUserLangToEnd(userLang, newName);

	                //add new language name in combobox
							addComboItem(IDC_LANGNAME_COMBO, newName);
							combo(IDC_LANGNAME_COMBO)->setCurrentIndex(static_cast<WPARAM>(newIndex) + 1);
							onCommand(MAKEWPARAM(IDC_LANGNAME_COMBO, CBN_SELCHANGE), 0);

	                //add new language name in langMenu
	                HWND hNpp = _hParent;
							HMENU m = reinterpret_cast<HMENU>(::SendMessage(hNpp, NPPM_INTERNAL_GETMENU, 0, 0));
	                HMENU hLangMenu = nppSubMenuAt(m, MENUINDEX_LANGUAGE);
				nppAddMenuItem(hLangMenu, IDM_LANG_USER + newIndex + 1, newName, hNpp, nppFindMenuAction(hLangMenu, IDM_LANG_USER + newIndex));
	            }

	            return TRUE;
	        }

	        case IDC_IMPORT_BUTTON :
	        {
	            CustomFileDialog fDlg(_hSelf);
	            fDlg.setExtFilter(L"UDL", L".xml");
	            wstring sourceFile = fDlg.doOpenSingleFileDlg();
	            if (sourceFile.empty()) break;

	            auto udlNumbersBeforeAfter = nppParam.importUDLFromFile(sourceFile);
	            if (udlNumbersBeforeAfter.second - udlNumbersBeforeAfter.first != 0)
	            {
	                auto i = combo(IDC_LANGNAME_COMBO)->currentIndex();
	                reloadLangCombo();
	                combo(IDC_LANGNAME_COMBO)->setCurrentIndex(i);
	                
	                pNativeSpeaker->messageBox("UDL_importSuccessful",
	                    _hSelf,
	                    L"Import successful.",
	                    L"User Defined Language",
	                    MB_OK | MB_APPLMODAL,
	                    0);

	                // Update language menu
	                HWND hNpp = _hParent;
	                HMENU hLangMenu = nppSubMenuAt(reinterpret_cast<HMENU>(::SendMessage(hNpp, NPPM_INTERNAL_GETMENU, 0, 0)), MENUINDEX_LANGUAGE);

	                for (auto j = udlNumbersBeforeAfter.first + 1; j <= udlNumbersBeforeAfter.second; ++j)
	                {
	                    const wchar_t* newName = nppParam.getULCFromIndex(j - 1)->getName();
	                    nppAddMenuItem(hLangMenu, IDM_LANG_USER + j, newName, hNpp, nppFindMenuAction(hLangMenu, IDM_LANG_USER + udlNumbersBeforeAfter.first));
	                }
	            }
	            else
	            {
	                pNativeSpeaker->messageBox("UDL_importFails",
	                    _hSelf,
	                    L"Failed to import.",
	                    L"User Defined Language",
	                    MB_OK | MB_APPLMODAL | MB_ICONEXCLAMATION,
	                    0);
	            }
	            break;
	        }

	        case IDC_EXPORT_BUTTON :
	        {
						auto i2Export = combo(IDC_LANGNAME_COMBO)->currentIndex();
	            if (i2Export == 0)
	            {
	                // maybe a better option would be to simply send IDC_SAVEAS_BUTTON message, and display "Save As..." dialog?
	                pNativeSpeaker->messageBox("UDL_saveBeforeImport",
	                    _hSelf,
	                    L"Before exporting, save your language definition by clicking \"Save As...\" button",
	                    L"User Defined Language",
	                    MB_OK | MB_APPLMODAL,
	                    0);
	                break;
	            }

	            CustomFileDialog fDlg(_hSelf);
	            fDlg.setExtFilter(L"UDL", L".xml");
						fDlg.setExtIndex(0);		// 0 Default index else file will be saved without extension
						wstring fileName2save = fDlg.doSaveDlg();
	            if (fileName2save.empty()) break;

	            if (i2Export > 0)
	            {
	                bool isSuccessful = nppParam.exportUDLToFile(i2Export - 1, fileName2save);
	                if (isSuccessful)
	                {
	                    pNativeSpeaker->messageBox("UDL_exportSuccessful",
	                        _hSelf,
	                        L"Export successful.",
	                        L"User Defined Language",
	                        MB_OK | MB_APPLMODAL,
	                        0);
	                }
	                else
	                {
	                    pNativeSpeaker->messageBox("UDL_exportFails",
	                        _hSelf,
	                        L"Failed to export.",
	                        L"User Defined Language",
	                        MB_OK | MB_APPLMODAL | MB_ICONEXCLAMATION,
	                        0);
	                }
	            }
	            break;
	        }

	        case IDC_UD_TRANSPARENT_CHECK :
	        {
	            bool isChecked = (isCheckedOrNot(IDC_UD_TRANSPARENT_CHECK));
	            if (isChecked)
	            {
							const auto percent = slider(IDC_UD_PERCENTAGE_SLIDER)->value();
	                nppParam.setTransparent(_hSelf, percent);
	            }
	            else
	                nppParam.removeTransparent(_hSelf);

	            enableItem(IDC_UD_PERCENTAGE_SLIDER, isChecked);
	            return TRUE;
	        }

	        case IDC_LANGNAME_IGNORECASE_CHECK :
	            return setPropertyByCheck(wParam, _pUserLang->_isCaseIgnored);

	        default :
	            break;
	    }
	}
	return FALSE;
}

namespace
{
	// Turns down any text that has one of the given characters
	class RestrictedCharsValidator : public QValidator
	{
	public:
		RestrictedCharsValidator(QObject* parent, const QString& restricted) : QValidator(parent), _restricted(restricted) {}

		State validate(QString& input, int& /*position*/) const override
		{
			for (const QChar ch : input)
			{
				if (_restricted.contains(ch))
					return Invalid;
			}
			return Acceptable;
		}

	private:
		QString _restricted;
	};
}

intptr_t StringDlg::doDialog()
{
	QDialog* dialog = createDialogWidget(false);
	Ui::StringDlg ui;
	ui.setupUi(dialog);

	if (!_restrictedChars.empty())
		ui.stringEdit->setValidator(new RestrictedCharsValidator(ui.stringEdit, QString::fromStdWString(_restrictedChars)));

	dialog->setWindowTitle(QString::fromStdWString(_title));
	ui.stringStatic->setTitle(QString::fromStdWString(_static));
	ui.stringEdit->setText(QString::fromStdWString(_textValue));
	ui.stringEdit->selectAll();
	if (_txtLen)
		ui.stringEdit->setMaxLength(_txtLen);

	// localization for OK and Cancel
	if (NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker())
	{
		ui.ok->setText(QString::fromStdWString(pNativeSpeaker->getLocalizedStrFromID("common-ok", L"OK")));
		ui.cancel->setText(QString::fromStdWString(pNativeSpeaker->getLocalizedStrFromID("common-cancel", L"Cancel")));
	}

	QObject::connect(ui.ok, &QAbstractButton::clicked, dialog, [this, edit = ui.stringEdit]()
	{
		_textValue = edit->text().toStdWString();
		endDialog(1);
	});
	QObject::connect(ui.cancel, &QAbstractButton::clicked, dialog, [this]() { endDialog(0); });

	if (_shouldGotoCenter)
		goToCenter();

	return (runModal() == 1) ? reinterpret_cast<intptr_t>(_textValue.c_str()) : 0;
}

int StylerDlg::doDialog()
{
	QDialog* dialog = createDialogWidget(false);
	Ui::StylerPopupDlg ui;
	ui.setupUi(dialog);

	initControls();
	connectControls();

	goToCenter();
	return runModal();
}

void StylerDlg::initControls()
{
	NppParameters& nppParam = NppParameters::getInstance();

	NativeLangSpeaker *pNativeLangSpeaker = nppParam.getNativeLangSpeaker();
	pNativeLangSpeaker->changeUserDefineLangPopupDlg(_hSelf);

	Style & style = SharedParametersDialog::_pUserLang->_styles.getStyler(_stylerIndex);

	// if it's a new styler (not yet any styles set), use the theme's default fg colors and transparent background
	if (SharedParametersDialog::_pUserLang->_isUnset)
	{
		SharedParametersDialog::_pUserLang->startAtTheme();

		// also needs to update _initialStyle, which was set wrong, above
		_initialStyle = SharedParametersDialog::_pUserLang->_styles.getStyler(_stylerIndex);
	}

	int sfs = (style._fontStyle == STYLE_NOT_USED) ? FONTSTYLE_NONE : style._fontStyle; // STYLE_NOT_USED=1, so STYLE_NOT_USED & FONTSTYLE_BOLD would be wrongly true
	setChecked(IDC_STYLER_CHECK_UNDERLINE, sfs & FONTSTYLE_UNDERLINE);
	setChecked(IDC_STYLER_CHECK_ITALIC, sfs & FONTSTYLE_ITALIC);
	setChecked(IDC_STYLER_CHECK_BOLD, sfs & FONTSTYLE_BOLD);

	// for the font size combo
	for (const wchar_t* fontSize : fontSizeStrs)
		addComboItem(IDC_STYLER_COMBO_FONT_SIZE, fontSize);

	const wstring size = (style._fontSize == -1) ? wstring() : to_wstring(style._fontSize);
	int i = findComboItem(IDC_STYLER_COMBO_FONT_SIZE, size);
	if (i != -1)
		combo(IDC_STYLER_COMBO_FONT_SIZE)->setCurrentIndex(i);

	// for the font name combo
	const std::vector<wstring> & fontlist = nppParam.getFontList();
	for (size_t j = 0, len = fontlist.size() ; j < len ; ++j)
		addComboItem(IDC_STYLER_COMBO_FONT_NAME, fontlist[j]);

	i = findComboItem(IDC_STYLER_COMBO_FONT_NAME, style._fontName);
	if (i == -1)
		i = 0;
	combo(IDC_STYLER_COMBO_FONT_NAME)->setCurrentIndex(i);

	if (style._fgColor == static_cast<COLORREF>(-1))
		style._fgColor = black;

	if (style._bgColor == static_cast<COLORREF>(-1))
		style._bgColor = white;

	_pFgColour->init(_hInst, _hSelf);
	_pFgColour->setColour(style._fgColor);
	bool isFgEnabled = (style._colorStyle & COLORSTYLE_FOREGROUND) != 0;
	_pFgColour->setEnabled(isFgEnabled);
	setChecked(IDC_STYLER_CHECK_FG_TRANSPARENT, !isFgEnabled);
	_pBgColour->init(_hInst, _hSelf);
	_pBgColour->setColour(style._bgColor);
	bool isBgEnabled = (style._colorStyle & COLORSTYLE_BACKGROUND) != 0;
	_pBgColour->setEnabled(isBgEnabled);
	setChecked(IDC_STYLER_CHECK_BG_TRANSPARENT, !isBgEnabled);

	placePicker(IDC_STYLER_FG_STATIC, *_pFgColour, false);
	placePicker(IDC_STYLER_BG_STATIC, *_pBgColour, false);
	_pFgColour->getHSelf()->setObjectName(QStringLiteral("fgColour"));
	_pBgColour->getHSelf()->setObjectName(QStringLiteral("bgColour"));

	_pFgColour->display();
	_pBgColour->display();

	unordered_map<int, int>::iterator iter = globalMappper().nestingMapper.begin();
	for (; iter != globalMappper().nestingMapper.end(); ++iter)
	{
		setChecked(iter->first, style._nesting & iter->second);
		enableItem(iter->first, _enabledNesters & iter->second);
	}
}

void StylerDlg::onCancel()
{
	onCommand(IDCANCEL, 0);
}

intptr_t StylerDlg::onCommand(WPARAM wParam, LPARAM /*lParam*/)
{
	Style & style = SharedParametersDialog::_pUserLang->_styles.getStyler(_stylerIndex);
	if (HIWORD(wParam) == CBN_SELCHANGE)
	{
		if (LOWORD(wParam) == IDC_STYLER_COMBO_FONT_SIZE)
		{
			bool isNumber = false;
			const int fontSize = combo(IDC_STYLER_COMBO_FONT_SIZE)->currentText().toInt(&isNumber);
			style._fontSize = isNumber ? fontSize : -1;
		}
		else if (LOWORD(wParam) == IDC_STYLER_COMBO_FONT_NAME)
		{
			style._fontName = combo(IDC_STYLER_COMBO_FONT_NAME)->currentText().toStdWString();
		}

		// show changes to user, re-color document
		if (SharedParametersDialog::_pScintilla->getCurrentBuffer()->getLangType() == L_USER)
			SharedParametersDialog::_pScintilla->styleChange();
		return TRUE;
	}
	else if (HIWORD(wParam) == CPN_COLOURPICKED)
	{
		if (wParam == IDCANCEL)
		{
			style = _initialStyle;

			// show changes to user, re-color document
			if (SharedParametersDialog::_pScintilla->getCurrentBuffer()->getLangType() == L_USER)
				SharedParametersDialog::_pScintilla->styleChange();

			endDialog(IDCANCEL);
			return TRUE;
		}

		if (wParam == IDOK)
		{
			endDialog(IDOK);
			return TRUE;
		}

		style._fgColor = _pFgColour->getColour();
		style._bgColor = _pBgColour->getColour();

		if (wParam == IDC_STYLER_CHECK_FG_TRANSPARENT || wParam == IDC_STYLER_CHECK_BG_TRANSPARENT)
		{
			if (wParam == IDC_STYLER_CHECK_FG_TRANSPARENT)
			{
				bool isTransparent = isCheckedOrNot(IDC_STYLER_CHECK_FG_TRANSPARENT);
				_pFgColour->setEnabled(!isTransparent);
				_pFgColour->redraw();
				if (isTransparent)
					style._colorStyle &= ~COLORSTYLE_FOREGROUND;
				else
					style._colorStyle |= COLORSTYLE_FOREGROUND;
			}

			if (wParam == IDC_STYLER_CHECK_BG_TRANSPARENT)
			{
				bool isTransparent = isCheckedOrNot(IDC_STYLER_CHECK_BG_TRANSPARENT);
				_pBgColour->setEnabled(!isTransparent);
				_pBgColour->redraw();
				if (isTransparent)
					style._colorStyle &= ~COLORSTYLE_BACKGROUND;
				else
					style._colorStyle |= COLORSTYLE_BACKGROUND;
			}
		}
		else
		{
			if (_pFgColour->isEnabled())
			{
				style._colorStyle |= COLORSTYLE_FOREGROUND;
			}
			else
			{
				style._colorStyle &= ~COLORSTYLE_FOREGROUND;
			}
			setChecked(IDC_STYLER_CHECK_FG_TRANSPARENT, !_pFgColour->isEnabled());

			if (_pBgColour->isEnabled())
			{
				style._colorStyle |= COLORSTYLE_BACKGROUND;
			}
			else
			{
				style._colorStyle &= ~COLORSTYLE_BACKGROUND;
			}
			setChecked(IDC_STYLER_CHECK_BG_TRANSPARENT, !_pBgColour->isEnabled());
		}
		style._fontStyle = FONTSTYLE_NONE;
		if (isCheckedOrNot(IDC_STYLER_CHECK_BOLD))
			style._fontStyle |= FONTSTYLE_BOLD;
		if (isCheckedOrNot(IDC_STYLER_CHECK_ITALIC))
			style._fontStyle |= FONTSTYLE_ITALIC;
		if (isCheckedOrNot(IDC_STYLER_CHECK_UNDERLINE))
			style._fontStyle |= FONTSTYLE_UNDERLINE;

		style._nesting = SCE_USER_MASK_NESTING_NONE;
		unordered_map<int, int>::iterator iter = globalMappper().nestingMapper.begin();
		for (; iter != globalMappper().nestingMapper.end(); ++iter)
		{
			if (isCheckedOrNot(iter->first))
				style._nesting |= iter->second;
		}

		// show changes to user, re-color document
		if (SharedParametersDialog::_pScintilla->getCurrentBuffer()->getLangType() == L_USER)
			SharedParametersDialog::_pScintilla->styleChange();

		return TRUE;
	}
	return FALSE;
}
