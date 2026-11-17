// This file is part of Notepad++ project
//
// Plain data types and string helpers that replace <windows.h> in the Qt port. Nothing here emulates
// Win32 behaviour: there are no message pumps or window functions. HWND is a QWidget pointer, and the
// other names are kept because the plugin API and the file format code are written in terms of them.

#pragma once

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <string>

#ifndef _WIN32
#include <strings.h>
#endif

class QWidget;
class QMenu;
class QIcon;
class QPixmap;

using BOOL = int;
using BYTE = unsigned char;
using UCHAR = unsigned char;
using WORD = unsigned short;
using DWORD = unsigned int;      // 32 bits, as on Windows (unsigned long is 64 bits on LP64)
using UINT = unsigned int;
using ULONG = unsigned long;
using LONG = int;                // 32 bits, as on Windows
using INT_PTR = std::intptr_t;
using UINT_PTR = std::uintptr_t;
using LONG_PTR = std::intptr_t;
using ULONG_PTR = std::uintptr_t;
using DWORD_PTR = std::uintptr_t;
using WPARAM = std::uintptr_t;
using LPARAM = std::intptr_t;
using LRESULT = std::intptr_t;
using HRESULT = int;
using COLORREF = std::uint32_t;
using WCHAR = wchar_t;
using LPCWSTR = const wchar_t*;
using LPWSTR = wchar_t*;
using LPCTSTR = const wchar_t*;
using LPTSTR = wchar_t*;
using HWND = QWidget*;
using HMENU = QMenu*;
using HICON = QIcon*;
using HBITMAP = QPixmap*;
using HANDLE = void*;
using HINSTANCE = void*;
using HMODULE = void*;
using _locale_t = void*;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef CALLBACK
#define CALLBACK
#endif
#ifndef WINAPI
#define WINAPI
#endif
#ifndef __cdecl
#define __cdecl
#endif

#ifndef _WIN32
// Scintilla and the file code use MAX_PATH for buffers: use the platform's real path limit.
#define MAX_PATH PATH_MAX
#endif

struct POINT { LONG x = 0; LONG y = 0; };
struct SIZE { LONG cx = 0; LONG cy = 0; };
struct RECT { LONG left = 0; LONG top = 0; LONG right = 0; LONG bottom = 0; };

struct FILETIME
{
	DWORD dwLowDateTime = 0;
	DWORD dwHighDateTime = 0;
};

struct SYSTEMTIME
{
	WORD wYear = 0;
	WORD wMonth = 0;
	WORD wDayOfWeek = 0;
	WORD wDay = 0;
	WORD wHour = 0;
	WORD wMinute = 0;
	WORD wSecond = 0;
	WORD wMilliseconds = 0;
};

union ULARGE_INTEGER
{
	struct { DWORD LowPart; DWORD HighPart; };
	std::uint64_t QuadPart = 0;
};

inline std::uint64_t fileTimeTicks(const FILETIME& ft)
{
	return (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

inline LONG CompareFileTime(const FILETIME* a, const FILETIME* b)
{
	const auto ta = fileTimeTicks(*a);
	const auto tb = fileTimeTicks(*b);
	return (ta < tb) ? -1 : ((ta > tb) ? 1 : 0);
}

// Filled by getFileAttributesExWithTimeout(), from stat() on POSIX.
inline constexpr DWORD FILE_ATTRIBUTE_READONLY = 0x00000001;
inline constexpr DWORD FILE_ATTRIBUTE_HIDDEN = 0x00000002;   // the name starts with '.'
inline constexpr DWORD FILE_ATTRIBUTE_DIRECTORY = 0x00000010;
inline constexpr DWORD FILE_ATTRIBUTE_NORMAL = 0x00000080;
inline constexpr DWORD INVALID_FILE_ATTRIBUTES = 0xFFFFFFFF;

struct WIN32_FILE_ATTRIBUTE_DATA
{
	DWORD dwFileAttributes = 0;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD nFileSizeHigh = 0;
	DWORD nFileSizeLow = 0;
};

inline std::int64_t fileSizeOf(const WIN32_FILE_ATTRIBUTE_DATA& attributes)
{
	return static_cast<std::int64_t>((static_cast<std::uint64_t>(attributes.nFileSizeHigh) << 32) | attributes.nFileSizeLow);
}

// Error codes use errno values, so the original comparisons keep their meaning.
inline constexpr DWORD NO_ERROR = 0;
inline constexpr DWORD ERROR_SUCCESS = 0;
inline constexpr DWORD ERROR_FILE_NOT_FOUND = ENOENT;
inline constexpr DWORD ERROR_ACCESS_DENIED = EACCES;
inline constexpr DWORD ERROR_INVALID_PARAMETER = EINVAL;
inline DWORD GetLastError() { return static_cast<DWORD>(errno); }
inline void SetLastError(DWORD code) { errno = static_cast<int>(code); }

// Base of the private message range. The NPPM_* and internal message numbers derive from it.
inline constexpr UINT WM_USER = 0x0400;

inline constexpr UINT CP_ACP = 0;
inline constexpr UINT CP_UTF8 = 65001;

constexpr COLORREF RGB(int r, int g, int b)
{
	return static_cast<COLORREF>(static_cast<BYTE>(r)) | (static_cast<COLORREF>(static_cast<BYTE>(g)) << 8) | (static_cast<COLORREF>(static_cast<BYTE>(b)) << 16);
}
constexpr BYTE GetRValue(COLORREF c) { return static_cast<BYTE>(c & 0xFF); }
constexpr BYTE GetGValue(COLORREF c) { return static_cast<BYTE>((c >> 8) & 0xFF); }
constexpr BYTE GetBValue(COLORREF c) { return static_cast<BYTE>((c >> 16) & 0xFF); }

constexpr BYTE HIBYTE(WORD v) { return static_cast<BYTE>((v >> 8) & 0xFF); }
constexpr WORD LOWORD(std::uintptr_t v) { return static_cast<WORD>(v & 0xFFFF); }
constexpr WORD HIWORD(std::uintptr_t v) { return static_cast<WORD>((v >> 16) & 0xFFFF); }
constexpr LONG MAKELONG(WORD low, WORD high) { return static_cast<LONG>(static_cast<DWORD>(low) | (static_cast<DWORD>(high) << 16)); }

template <typename T, std::size_t N>
constexpr std::size_t _countof(const T (&)[N]) noexcept { return N; }

// String helpers with the Microsoft CRT names the code base uses.
inline int _wcsicmp(const wchar_t* a, const wchar_t* b)
{
	for (; *a && *b; ++a, ++b)
	{
		const auto la = std::towlower(*a);
		const auto lb = std::towlower(*b);
		if (la != lb)
			return la < lb ? -1 : 1;
	}
	return (*a == *b) ? 0 : (*a ? 1 : -1);
}

inline int _wcsnicmp(const wchar_t* a, const wchar_t* b, std::size_t n)
{
	for (; n > 0; --n, ++a, ++b)
	{
		const auto la = std::towlower(*a);
		const auto lb = std::towlower(*b);
		if (la != lb)
			return la < lb ? -1 : 1;
		if (!*a)
			break;
	}
	return 0;
}

#ifndef _WIN32
inline int _stricmp(const char* a, const char* b) { return ::strcasecmp(a, b); }
inline int _strnicmp(const char* a, const char* b, std::size_t n) { return ::strncasecmp(a, b, n); }
inline int _wtoi(const wchar_t* s) { return static_cast<int>(std::wcstol(s, nullptr, 10)); }

inline int wcscpy_s(wchar_t* dest, std::size_t size, const wchar_t* src)
{
	if (!dest || !size)
		return EINVAL;
	std::wcsncpy(dest, src, size - 1);
	dest[size - 1] = L'\0';
	return 0;
}
template <std::size_t N> int wcscpy_s(wchar_t (&dest)[N], const wchar_t* src) { return wcscpy_s(dest, N, src); }

inline int wcscat_s(wchar_t* dest, std::size_t size, const wchar_t* src)
{
	const std::size_t used = std::wcslen(dest);
	return (used < size) ? wcscpy_s(dest + used, size - used, src) : ERANGE;
}
template <std::size_t N> int wcscat_s(wchar_t (&dest)[N], const wchar_t* src) { return wcscat_s(dest, N, src); }

inline int wcsncpy_s(wchar_t* dest, std::size_t size, const wchar_t* src, std::size_t count)
{
	if (!dest || !size)
		return EINVAL;
	const std::size_t n = std::min(count, size - 1);
	std::wcsncpy(dest, src, n);
	dest[n] = L'\0';
	return 0;
}
template <std::size_t N> int wcsncpy_s(wchar_t (&dest)[N], const wchar_t* src, std::size_t count) { return wcsncpy_s(dest, N, src, count); }

inline int strcpy_s(char* dest, std::size_t size, const char* src)
{
	if (!dest || !size)
		return EINVAL;
	std::strncpy(dest, src, size - 1);
	dest[size - 1] = '\0';
	return 0;
}
template <std::size_t N> int strcpy_s(char (&dest)[N], const char* src) { return strcpy_s(dest, N, src); }

inline constexpr std::size_t _TRUNCATE = static_cast<std::size_t>(-1);

inline int strncpy_s(char* dest, std::size_t size, const char* src, std::size_t count)
{
	if (!dest || !size)
		return EINVAL;
	const std::size_t n = std::min(count == _TRUNCATE ? std::strlen(src) : count, size - 1);
	std::memcpy(dest, src, std::min(n, std::strlen(src)));
	dest[std::min(n, std::strlen(src))] = '\0';
	return 0;
}

inline int memcpy_s(void* dest, std::size_t destSize, const void* src, std::size_t count)
{
	if (!dest || !src || count > destSize)
		return EINVAL;
	std::memcpy(dest, src, count);
	return 0;
}

inline wchar_t* lstrcpy(wchar_t* dest, const wchar_t* src) { return std::wcscpy(dest, src); }
inline wchar_t* lstrcpyn(wchar_t* dest, const wchar_t* src, int size)
{
	wcsncpy_s(dest, static_cast<std::size_t>(size), src, static_cast<std::size_t>(size) - 1);
	return dest;
}

inline std::size_t lstrlen(const wchar_t* s) { return std::wcslen(s); }
inline int lstrcmp(const wchar_t* a, const wchar_t* b) { return std::wcscmp(a, b); }
inline int lstrcmpi(const wchar_t* a, const wchar_t* b) { return _wcsicmp(a, b); }

// wfopen() with UTF-8 paths, the native file name encoding here.
FILE* _wfopen(const wchar_t* path, const wchar_t* mode);
#endif

// Pointers into the given path. As with the shlwapi functions of these names, the file name follows the
// last separator, and the extension starts at the last dot of the file name, or is the empty end of the string.
inline const wchar_t* PathFindFileName(const wchar_t* path)
{
	const wchar_t* slash = std::wcsrchr(path, L'/');
	return slash ? slash + 1 : path;
}

inline const wchar_t* PathFindExtension(const wchar_t* path)
{
	const wchar_t* name = PathFindFileName(path);
	const wchar_t* dot = std::wcsrchr(name, L'.');
	return dot ? dot : name + std::wcslen(name);
}

// The separator file paths are built with.
#ifdef _WIN32
inline constexpr wchar_t nppPathSep = L'\\';
inline constexpr const wchar_t* nppPathSepStr = L"\\";
#else
inline constexpr wchar_t nppPathSep = L'/';
inline constexpr const wchar_t* nppPathSepStr = L"/";
#endif

// Message box severity flags used by non-UI code, with the Win32 values SortLocale results carry.
inline constexpr UINT MB_OK = 0x00000000;
inline constexpr UINT MB_OKCANCEL = 0x00000001;
inline constexpr UINT MB_ABORTRETRYIGNORE = 0x00000002;
inline constexpr UINT MB_YESNOCANCEL = 0x00000003;
inline constexpr UINT MB_YESNO = 0x00000004;
inline constexpr UINT MB_RETRYCANCEL = 0x00000005;
inline constexpr UINT MB_CANCELTRYCONTINUE = 0x00000006;
inline constexpr UINT MB_TYPEMASK = 0x0000000F;
inline constexpr UINT MB_ICONERROR = 0x00000010;
inline constexpr UINT MB_ICONHAND = MB_ICONERROR;
inline constexpr UINT MB_ICONSTOP = MB_ICONERROR;
inline constexpr UINT MB_ICONQUESTION = 0x00000020;
inline constexpr UINT MB_ICONWARNING = 0x00000030;
inline constexpr UINT MB_ICONEXCLAMATION = MB_ICONWARNING;
inline constexpr UINT MB_ICONINFORMATION = 0x00000040;
inline constexpr UINT MB_ICONMASK = 0x000000F0;
inline constexpr UINT MB_DEFBUTTON2 = 0x00000100;
inline constexpr UINT MB_DEFBUTTON3 = 0x00000200;
inline constexpr UINT MB_DEFMASK = 0x00000F00;
inline constexpr UINT MB_APPLMODAL = 0x00000000;
inline constexpr UINT MB_SYSTEMMODAL = 0x00001000;
inline constexpr UINT MB_TASKMODAL = 0x00002000;
inline constexpr UINT MB_MODEMASK = 0x00003000;
inline constexpr UINT MB_SETFOREGROUND = 0x00010000;
inline constexpr UINT MB_TOPMOST = 0x00040000;
inline constexpr UINT MB_RIGHT = 0x00080000;
inline constexpr UINT MB_RTLREADING = 0x00100000;
