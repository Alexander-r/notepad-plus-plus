// This file is part of Notepad++ project
//
// Data types of the tables generated from the .rc files by cmake/tools/rc2qt.py.

#pragma once

struct NppMenuEntry
{
	enum Kind { Item, Separator, Popup };
	static constexpr unsigned Checked = 1u << 0;
	static constexpr unsigned Disabled = 1u << 1;

	Kind kind;
	int id;
	const wchar_t* text;
	const NppMenuEntry* children;
	int childCount;
	unsigned flags;
};

struct NppImageResource
{
	int id;
	const char* path; // Qt resource path
};

const NppMenuEntry* nppMenuTemplate(int menuId, int& count);
const char* nppImageResourcePath(int resourceId);

class QIcon;
// A new icon from the resources, as plugin structures hold them by pointer. Null when there is no such resource.
QIcon* nppLoadIcon(int resourceId);
