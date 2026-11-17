// This file is part of Notepad++ project
//
// Helpers for the menus, which are addressed by command ID as in the original. An HMENU is a QMenu whose
// actions carry the command ID. The main menu is a QMenu too, whose top level entries the menu bar shows.

#pragma once

#include <string>

#include "NppPlatform.h"

class QAction;
class QMenuBar;
class QPixmap;
struct NppMenuEntry;

// Creates a menu from a generated .rc menu template. Choosing an item sends WM_COMMAND to commandTarget.
HMENU nppCreateMenuFromTemplate(int menuId, HWND commandTarget, QWidget* parent);

// Adds a command item. Choosing it sends WM_COMMAND to the menu's command target.
QAction* nppAddMenuItem(HMENU menu, int cmdId, const std::wstring& text, HWND commandTarget, QAction* before = nullptr);
QAction* nppAddMenuSeparator(HMENU menu, QAction* before = nullptr);
HMENU nppAddSubMenu(HMENU menu, const std::wstring& text, QAction* before = nullptr);

// By position, counting separators as the .rc menus do. Null when out of range.
int nppMenuItemCount(HMENU menu);
QAction* nppMenuActionAt(HMENU menu, int position);
HMENU nppSubMenuAt(HMENU menu, int position);

// Inserting and removing by position
QAction* nppInsertMenuItemAt(HMENU menu, int position, int cmdId, const std::wstring& text, HWND commandTarget);
QAction* nppInsertSeparatorAt(HMENU menu, int position);
void nppInsertSubMenuAt(HMENU menu, int position, HMENU subMenu, const std::wstring& text);
void nppRemoveMenuItemAt(HMENU menu, int position);

// Searches the menu and its submenus
QAction* nppFindMenuAction(HMENU menu, int cmdId);

void nppEnableMenuCommand(HMENU menu, int cmdId, bool enable);
void nppCheckMenuCommand(HMENU menu, int cmdId, bool check);
bool nppIsMenuCommandChecked(HMENU menu, int cmdId);
bool nppIsMenuCommandEnabled(HMENU menu, int cmdId);
std::wstring nppMenuCommandText(HMENU menu, int cmdId);
void nppSetMenuCommandText(HMENU menu, int cmdId, const std::wstring& text);
void nppSetMenuCommandIcon(HMENU menu, int cmdId, const QPixmap& icon);
// Checks the item with this command ID and unchecks the others in the range
void nppCheckMenuRadioRange(HMENU menu, int firstCmdId, int lastCmdId, int cmdId);
// Removes the item with this command ID from wherever it is in the menu
void nppRemoveMenuCommand(HMENU menu, int cmdId);

// Makes the bar show the top level entries of the menu. Call again after changing them.
void nppSyncMenuBar(QMenuBar* bar, HMENU menu);
