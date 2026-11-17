// A small part of <windows.h>, for the source of a Windows plugin to compile for the Qt port of Notepad++.
//
// This is only for plugins. It does not emulate the Windows API: it has the types and macros that plain plugin code
// tends to use and MessageBox. Anything that makes a window, like a dialog from the resources, has to be rewritten with
// Qt. See ../README.md.

#pragma once

#include <QMessageBox>
#include <QString>

#include <cwchar>

#include "NppMessages.h" // SendMessage, HWND and the message numbers: HWND is a QWidget* here

using TCHAR = wchar_t; // plugins are always Unicode, and wchar_t has 32 bits here
#ifndef TEXT
#define TEXT(text) L##text
#endif

// The exports of the plugin
#define __declspec(ignored) __attribute__((visibility("default")))

// For a DllMain, which nothing calls: see ../README.md
#define APIENTRY
#define CALLBACK
using LPVOID = void*;
using HANDLE = void*;
inline constexpr DWORD DLL_PROCESS_DETACH = 0;
inline constexpr DWORD DLL_PROCESS_ATTACH = 1;
inline constexpr DWORD DLL_THREAD_ATTACH = 2;
inline constexpr DWORD DLL_THREAD_DETACH = 3;

#define lstrcpy wcscpy
#define lstrlen wcslen

inline int MessageBox(HWND parent, const wchar_t* text, const wchar_t* title, UINT /*type*/)
{
	QMessageBox::warning(parent, QString::fromWCharArray(title), QString::fromWCharArray(text));
	return IDOK;
}
