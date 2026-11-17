// A small part of <windows.h>, for the source of a Windows plugin to compile for the Qt port of Notepad++.
//
// It does not emulate the Windows API. It has the types, constants and macros that plain plugin code uses, helpers
// for strings, paths and memory that work as their Windows namesakes, and MessageBox. Anything that makes a window,
// uses the clipboard or opens a file dialog is rewritten with Qt in the plugin's source. See ../README.md.

#pragma once

#include <QString>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>

#include "NppMessages.h" // SendMessage, HWND and the message numbers: HWND is a QWidget* here

using CHAR = char;
using LPSTR = char*;
using LPCSTR = const char*;
using TCHAR = wchar_t; // plugins are always Unicode, and wchar_t has 32 bits here
using LPNMHDR = NMHDR*; // WM_NOTIFY's header, of NppMessages.h
#ifndef TEXT
#define NPP_COMPAT_WIDE(text) L##text
#define TEXT(text) NPP_COMPAT_WIDE(text) // in two steps, so that a macro given to it is expanded first
#endif

// The exports of the plugin. Everything else stays inside it, as in a DLL.
#define __declspec(ignored) __attribute__((visibility("default")))

// For a DllMain, which nothing calls: see ../README.md
#define APIENTRY
using LPVOID = void*;
inline constexpr DWORD DLL_PROCESS_DETACH = 0;
inline constexpr DWORD DLL_PROCESS_ATTACH = 1;
inline constexpr DWORD DLL_THREAD_ATTACH = 2;
inline constexpr DWORD DLL_THREAD_DETACH = 3;

constexpr WORD MAKEWORD(BYTE low, BYTE high) { return static_cast<WORD>(low | (static_cast<WORD>(high) << 8)); }
constexpr BYTE LOBYTE(WORD value) { return static_cast<BYTE>(value & 0xFF); }

inline void ZeroMemory(void* destination, std::size_t length)
{
	std::memset(destination, 0, length);
}

inline wchar_t* lstrcat(wchar_t* destination, const wchar_t* source)
{
	return std::wcscat(destination, source);
}

inline int lstrcmpiA(const char* a, const char* b)
{
	return _stricmp(a, b);
}

// The CRT's itoa, which glibc does not have
inline char* itoa(int value, char* buffer, int radix)
{
	unsigned int magnitude = (radix == 10 && value < 0) ? 0u - static_cast<unsigned int>(value) : static_cast<unsigned int>(value);
	char digits[33];
	int count = 0;
	do
	{
		const unsigned int digit = magnitude % static_cast<unsigned int>(radix);
		digits[count++] = static_cast<char>(digit < 10 ? '0' + digit : 'a' + digit - 10);
		magnitude /= static_cast<unsigned int>(radix);
	} while (magnitude);

	char* out = buffer;
	if (radix == 10 && value < 0)
		*out++ = '-';
	while (count)
		*out++ = digits[--count];
	*out = '\0';
	return buffer;
}

// Memory that a plugin hands around as on Windows. Nothing moves it, so locking gives the memory itself.
using HGLOBAL = void*;
inline constexpr UINT GMEM_MOVEABLE = 0x0002;
inline constexpr UINT GMEM_DDESHARE = 0x2000;
inline HGLOBAL GlobalAlloc(UINT /*flags*/, std::size_t size)
{
	return std::calloc(size ? size : 1, 1);
}
inline void* GlobalLock(HGLOBAL memory) { return memory; }
inline BOOL GlobalUnlock(HGLOBAL /*memory*/) { return FALSE; }
inline HGLOBAL GlobalFree(HGLOBAL memory)
{
	std::free(memory);
	return nullptr;
}

// The plain text clipboard format, for code that names it. The clipboard itself is Qt's.
inline constexpr UINT CF_TEXT = 1;

// The full path of the plugin's own library, whatever module handle is given, as only the plugin asks for it
DWORD GetModuleFileName(HMODULE module, wchar_t* fileName, DWORD size);

// A wide printf format of Windows for glibc's wide printf, where "%s" and "%c" take narrow strings and characters unless
// written "%ls" and "%lc", while on Windows they take wide ones. "%S" and "%C", Windows' narrow ones, become "%s" and "%c",
// and MSVC's lengths "%I64d", "%I32d" and "%Id" become "%lld", "%d" and "%zd".
std::wstring nppCompatWideFormat(const wchar_t* format);

// INI files, as Windows reads and writes them: sections and keys whatever their case, values without the spaces and
// quotes around them, a null section or key giving the names as strings that end with two nulls, and a null key or
// value deleting. The file is UTF-8, and one in UTF-16 with a BOM from Windows is read too.
DWORD GetPrivateProfileString(const wchar_t* section, const wchar_t* key, const wchar_t* defaultValue, wchar_t* output, DWORD size,
	const wchar_t* fileName);
UINT GetPrivateProfileInt(const wchar_t* section, const wchar_t* key, int defaultValue, const wchar_t* fileName);
BOOL WritePrivateProfileString(const wchar_t* section, const wchar_t* key, const wchar_t* value, const wchar_t* fileName);

// A message box with the buttons and icon of the type, giving back which button was pressed
int MessageBox(HWND parent, const wchar_t* text, const wchar_t* title, UINT type);
int MessageBoxA(HWND parent, const char* text, const char* title, UINT type);
