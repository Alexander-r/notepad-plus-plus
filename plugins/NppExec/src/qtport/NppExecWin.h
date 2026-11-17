// Qt port: what NppExec takes from <windows.h> beyond the plugins' compat/windows.h, which only NppExec uses.
//
// Types, constants and helpers that work as their Windows namesakes, as in compat/windows.h. Windows' kernel objects
// that NppExec waits on (events, threads and child processes) are in NppExecSync.h, and its windows are Qt widgets.

#pragma once

#define NPPEXEC_QT_PORT 1

#include <windows.h>
#include <tchar.h>

#include <cstdarg>
#include <cstdint>
#include <functional>
#include <string>

using INT = int;
using SHORT = short;
using USHORT = unsigned short;
using LONGLONG = long long;
using ULONGLONG = unsigned long long;
using SIZE_T = std::size_t;
using PVOID = void*;
using LPCVOID = const void*;
using LPBYTE = unsigned char*;
using LPDWORD = DWORD*;
using LPINT = int*;
using LPLONG = LONG*;
using PBYTE = unsigned char*;
using PCHAR = char*;
using PWCHAR = wchar_t*;
using PTCHAR = wchar_t*;
using LPWCH = wchar_t*;
// MSVC's built-in type, also written as "unsigned __int64"
#define __int64 long long

inline void CopyMemory(void* destination, const void* source, std::size_t length)
{
	std::memcpy(destination, source, length);
}

// Code pages, for the conversions below, besides CP_ACP and CP_UTF8. The ANSI and OEM code pages are UTF-8, as on
// Linux it is the system's.
inline constexpr UINT CP_OEMCP = 1;
inline constexpr UINT CP_UTF7 = 65000;

// As on Windows: a length of -1 includes the terminating null, a destination size of 0 gives the size needed, and
// what a code page cannot hold or read becomes '?' or U+FFFD. Code pages other than UTF-8 go through glibc's iconv.
int MultiByteToWideChar(UINT codePage, DWORD flags, const char* source, int sourceLength, wchar_t* destination, int destinationSize);
int WideCharToMultiByte(UINT codePage, DWORD flags, const wchar_t* source, int sourceLength, char* destination, int destinationSize,
	const char* defaultChar, BOOL* usedDefaultChar);
UINT GetACP();
UINT GetOEMCP();

// User32's printf: "%s" is a wide string in the wide one, and the output stops at 1024 characters
int wvsprintf(wchar_t* output, const wchar_t* format, va_list arguments);
int wvsprintfA(char* output, const char* format, va_list arguments);
int wsprintf(wchar_t* output, const wchar_t* format, ...);
int wsprintfA(char* output, const char* format, ...);
// MSVC's swprintf without a size, for a buffer of that size, with Windows' format
int nppExecSwprintf(wchar_t* output, std::size_t size, const wchar_t* format, ...);

union LARGE_INTEGER
{
	struct
	{
		DWORD LowPart;
		LONG HighPart;
	};
	long long QuadPart;
};

inline constexpr DWORD FILE_NOTIFY_CHANGE_FILE_NAME = 0x00000001;
inline constexpr DWORD FILE_NOTIFY_CHANGE_DIR_NAME = 0x00000002;
inline constexpr DWORD FILE_NOTIFY_CHANGE_SIZE = 0x00000008;
inline constexpr DWORD FILE_NOTIFY_CHANGE_LAST_WRITE = 0x00000010;

// The modification time of a file, in a FILETIME of NppPlatform.h: 100 ns since 1601
bool nppExecGetFileWriteTime(const wchar_t* fileName, FILETIME* writeTime);
bool nppExecSetFileWriteTime(const wchar_t* fileName, const FILETIME* writeTime);

void GetLocalTime(SYSTEMTIME* time);

// Compares as CompareString with NORM_IGNORECASE: below 0, 0 or above 0. A length of -1 goes to the null.
int nppExecCompareNoCase(const wchar_t* s1, int length1, const wchar_t* s2, int length2);

// Files, as the Windows calls of the same names
BOOL DeleteFile(const wchar_t* fileName);
BOOL MoveFile(const wchar_t* existingName, const wchar_t* newName);
BOOL CopyFile(const wchar_t* existingName, const wchar_t* newName, BOOL failIfExists);
BOOL CreateDirectory(const wchar_t* path, void* securityAttributes);
DWORD GetTempPath(DWORD size, wchar_t* buffer);
BOOL SetCurrentDirectory(const wchar_t* path);

DWORD GetCurrentDirectory(DWORD size, wchar_t* buffer);
// Sets the variable in the process's environment, or removes it when value is null
BOOL SetEnvironmentVariable(const wchar_t* name, const wchar_t* value);
DWORD GetFileAttributes(const wchar_t* fileName);
// As SearchPath with a null path and extension: whether the file is there, as a path or in a folder of PATH
DWORD SearchPath(const wchar_t* path, const wchar_t* fileName, const wchar_t* extension, DWORD size, wchar_t* buffer, wchar_t** filePart);
inline int lstrlenA(const char* text) { return text ? static_cast<int>(std::strlen(text)) : 0; }
inline double _wtof(const wchar_t* text) { return std::wcstod(text, nullptr); }
inline char* lstrcpyA(char* destination, const char* source) { return std::strcpy(destination, source); }

inline constexpr DWORD ERROR_PATH_NOT_FOUND = 3;
inline constexpr DWORD ERROR_BAD_FORMAT = 11;
inline constexpr DWORD ERROR_ALREADY_EXISTS = 183;

// Case of Unicode characters, in place, as user32's CharLower and CharUpper with a string
wchar_t* CharLower(wchar_t* text);
wchar_t* CharUpper(wchar_t* text);
inline DWORD CharLowerBuff(wchar_t* text, DWORD length)
{
	for (DWORD i = 0; i < length; ++i)
	{
		wchar_t one[2] = {text[i], L'\0'};
		text[i] = CharLower(one)[0];
	}
	return length;
}

// A file name for the C library, in the system's encoding of file names
std::string nppExecLocalPath(const wchar_t* fileName);

// Runs the function on the GUI thread and waits for it, as SendMessage to a window of another thread does on Windows.
// Once the application is closing, it does nothing from other threads.
void nppExecOnGuiThread(const std::function<void()>& function);

#include "NppExecSync.h"
#include "NppExecDialog.h"
