// The wide versions of <tchar.h>'s names, as plugins are always Unicode. See windows.h next to this file.
#pragma once
#include "windows.h"

#include <ctime>

#ifndef _T
#define _T(text) TEXT(text)
#define _TEXT(text) TEXT(text)
#endif

inline int _ttoi(const wchar_t* text)
{
	return static_cast<int>(std::wcstol(text, nullptr, 10));
}

inline const wchar_t* _tcsstr(const wchar_t* text, const wchar_t* part)
{
	return std::wcsstr(text, part);
}

inline wchar_t* _tcsstr(wchar_t* text, const wchar_t* part)
{
	return std::wcsstr(text, part);
}

inline int _tcscmp(const wchar_t* a, const wchar_t* b)
{
	return std::wcscmp(a, b);
}

inline const wchar_t* _tcschr(const wchar_t* text, wchar_t ch)
{
	return std::wcschr(text, ch);
}

inline wchar_t* _tcschr(wchar_t* text, wchar_t ch)
{
	return std::wcschr(text, ch);
}

inline std::size_t _tcsftime(wchar_t* buffer, std::size_t size, const wchar_t* format, const std::tm* time)
{
	return std::wcsftime(buffer, size, format, time);
}
