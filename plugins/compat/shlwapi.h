// The path functions of <shlwapi.h> that plugins use, for paths with '/' as here. See windows.h next to this file.
#pragma once

#include "windows.h"

BOOL PathFileExists(const wchar_t* path);
BOOL PathIsDirectory(const wchar_t* path);

// Whether the path is the root, "/", the only one here
inline BOOL PathIsRoot(const wchar_t* path)
{
	return (path && path[0] == L'/' && path[1] == L'\0') ? TRUE : FALSE;
}

// Whether the path matches the pattern of * and ?, whatever the case. "*.*" matches any name, also one without a dot.
BOOL PathMatchSpec(const wchar_t* path, const wchar_t* spec);

// Takes the extension off the file name, as PathFindExtension finds it
inline void PathRemoveExtension(wchar_t* path)
{
	*const_cast<wchar_t*>(PathFindExtension(path)) = L'\0';
}

// Takes the file name and the separator before it off the path. The root keeps its separator.
inline BOOL PathRemoveFileSpec(wchar_t* path)
{
	wchar_t* name = const_cast<wchar_t*>(PathFindFileName(path));
	if (name == path)
	{
		const bool hadName = *path != L'\0';
		*path = L'\0';
		return hadName ? TRUE : FALSE;
	}
	if (name - 1 == path)
		*name = L'\0';
	else
		name[-1] = L'\0';
	return TRUE;
}
