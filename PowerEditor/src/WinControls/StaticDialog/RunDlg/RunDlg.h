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

#include <string>

#include "StaticDialog.h"

inline constexpr int CURRENTWORD_MAXLENGTH = 2048;

inline constexpr wchar_t fullCurrentPath[] = L"FULL_CURRENT_PATH";
inline constexpr wchar_t currentDirectory[] = L"CURRENT_DIRECTORY";
inline constexpr wchar_t onlyFileName[] = L"FILE_NAME";
inline constexpr wchar_t fileNamePart[] = L"NAME_PART";
inline constexpr wchar_t fileExtPart[] = L"EXT_PART";
inline constexpr wchar_t currentWord[] = L"CURRENT_WORD";
inline constexpr wchar_t nppDir[] = L"NPP_DIRECTORY";
inline constexpr wchar_t nppFullFilePath[] = L"NPP_FULL_FILE_PATH";
inline constexpr wchar_t currentLine[] = L"CURRENT_LINE";
inline constexpr wchar_t currentColumn[] = L"CURRENT_COLUMN";
inline constexpr wchar_t currentLineStr[] = L"CURRENT_LINESTR";

int whichVar(const std::wstring& str);
std::wstring expandNppEnvironmentStrs(const std::wstring& strSrc, HWND hWnd);
std::wstring expandEnvironmentStrs(const std::wstring& text);

class Command {
public :
	Command() = default;
	explicit Command(const wchar_t* cmd) : _cmdLine(cmd) {}
	explicit Command(const std::wstring& cmd) : _cmdLine(cmd) {}
	// Returns whether the command could be launched. On failure the user has been told why.
	bool run(HWND hWnd);
	bool run(HWND hWnd, const wchar_t* cwd);

protected :
	std::wstring _cmdLine;
};

class RunDlg : public Command, public StaticDialog
{
public :
	RunDlg() = default;

	void doDialog(bool isRTL = false);
	void destroy() override {}

protected :
	void insertVariable(unsigned char id);

private :
	void onRun();
	void onSave();
	void onBrowse();
	void onVariables();
	void addTextToCombo(const wchar_t *txt2Add) const;
	void removeTextFromCombo(const wchar_t *txt2Remove) const;
};
