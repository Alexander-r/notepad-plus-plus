// Qt port: helpers for NppExec's controls, which are Qt widgets with their control IDs in the "nppId" property.

#pragma once

#include "NppExecWin.h"

#include <QString>

class QWidget;
class QAction;
class QMenu;
class QBrush;
class QFont;

// The text of a control as Windows' GetWindowText gives it: an edit's or combo box's text, a button's or label's
// caption, or a window's title
QString nppExecWidgetText(const QWidget* widget);
void nppExecSetWidgetText(QWidget* widget, const QString& text);

// The control of the dialog with that ID, as GetDlgItem
HWND GetDlgItem(HWND dialog, int id);
int GetDlgCtrlID(HWND control);

using DLGPROC = INT_PTR (*)(HWND, UINT, WPARAM, LPARAM);
using TIMERPROC = void (*)(HWND, UINT, UINT_PTR, DWORD);

// Brushes and fonts of the original's custom colours, which are a palette's colours and QFonts here
using HBRUSH = QBrush*;
using HFONT = QFont*;

// Windows, as the calls of the same names, on the GUI thread
BOOL IsWindow(HWND window);
BOOL IsWindowVisible(HWND window);
BOOL IsWindowEnabled(HWND window);
HWND GetFocus();
HWND SetFocus(HWND window);
HWND GetParent(HWND window);
BOOL EnableWindow(HWND window, BOOL enable);
BOOL ShowWindow(HWND window, int command);
BOOL SetWindowText(HWND window, const wchar_t* text);
int GetWindowText(HWND window, wchar_t* text, int size);
int GetWindowTextLength(HWND window);

// A timer of the GUI thread, which calls the function with its ID, as SetTimer with a null window does
UINT_PTR SetTimer(HWND window, UINT_PTR id, UINT milliseconds, TIMERPROC function);
BOOL KillTimer(HWND window, UINT_PTR id);

// A menu item's check mark, by its command ID: HMENU is a QMenu*, and Notepad++ keeps the ID in the action's data
inline constexpr UINT MF_BYCOMMAND = 0x00000000;
inline constexpr UINT MF_BYPOSITION = 0x00000400;
inline constexpr UINT MF_UNCHECKED = 0x00000000;
inline constexpr UINT MF_CHECKED = 0x00000008;
inline constexpr UINT MF_ENABLED = 0x00000000;
inline constexpr UINT MF_GRAYED = 0x00000001;
inline constexpr UINT MF_DISABLED = 0x00000002;
DWORD CheckMenuItem(HMENU menu, UINT item, UINT flags);
// The action of the command, in the menu or its submenus, and the menu that has it
QAction* nppExecMenuAction(HMENU menu, UINT id);
QMenu* nppExecMenuOfCommand(HMENU menu, UINT id);
BOOL EnableMenuItem(HMENU menu, UINT item, UINT flags);

// Subclassing, as with SetWindowLongPtr(GWLP_WNDPROC): an event filter gives the procedure the widget's key, mouse
// and focus events as WM_KEYDOWN, WM_CHAR, WM_KEYUP, WM_LBUTTONDOWN, WM_SETFOCUS and the like. What it gives back is
// the "original procedure": CallWindowProc with it hands the event being filtered to the widget there and then, as
// the original procedure of Windows would handle the message, once for each event. An event that the procedure
// does not hand on this way is not seen by the widget.
using WNDPROC = LRESULT (*)(HWND, UINT, WPARAM, LPARAM);
inline constexpr int GWLP_WNDPROC = -4;
inline constexpr int GWLP_ID = -12;
inline constexpr int GWLP_USERDATA = -21;
inline constexpr int DWLP_MSGRESULT = 0; // a dialog's result of a notification, as WM_NOTIFY's
LONG_PTR SetWindowLongPtr(HWND window, int index, LONG_PTR value);
LONG_PTR GetWindowLongPtr(HWND window, int index);
LRESULT CallWindowProc(WNDPROC proc, HWND window, UINT message, WPARAM wParam, LPARAM lParam);

// A window's named values, as SetProp keeps them: dynamic properties of the widget here
HANDLE GetProp(HWND window, const wchar_t* name);
BOOL SetProp(HWND window, const wchar_t* name, HANDLE value);
HANDLE RemoveProp(HWND window, const wchar_t* name);
BOOL IsChild(HWND parent, HWND window);
// Whether this is a menu, and not, say, a command ID kept in the same place
BOOL IsMenu(HMENU menu);
inline HMENU GetMenu(HWND) { return nullptr; } // the menu of Notepad++ comes from NPPM_GETMENUHANDLE

// The images of NppExec.rc, from the plugin's Qt resources (NppExec.qrc), for its toolbar button and console tab
HBITMAP nppExecLoadBitmap(int id);
HICON nppExecLoadIcon(int id);

// Fonts, which NppExec keeps as LOGFONT in its settings: HFONT is a QFont* here, made with CreateFontIndirect and
// deleted with DeleteObject. lfHeight is negative for the height of the characters in pixels, as on Windows.
HFONT CreateFontIndirect(const struct LOGFONT* logFont);
int GetObject(HFONT font, int size, void* logFont);
void DeleteObject(HFONT font);
void DeleteObject(HBRUSH brush);
void DeleteObject(HBITMAP bitmap);
HBRUSH CreateSolidBrush(COLORREF color);
void DestroyIcon(HICON icon);
// The system's fixed font, for GetStockObject(ANSI_FIXED_FONT)
inline constexpr int ANSI_FIXED_FONT = 11;
HFONT GetStockObject(int object);
struct CHOOSEFONT
{
	DWORD lStructSize;
	HWND hwndOwner;
	void* hDC;
	struct LOGFONT* lpLogFont;
	INT iPointSize;
	DWORD Flags;
};
inline constexpr DWORD CF_SCREENFONTS = 0x00000001;
inline constexpr DWORD CF_INITTOLOGFONTSTRUCT = 0x00000040;
inline constexpr DWORD CF_FORCEFONTEXIST = 0x00010000;
// QFontDialog, starting with the font of lpLogFont, which it sets to the font chosen
BOOL ChooseFont(CHOOSEFONT* chooseFont);

// MessageBoxTimeout of user32: a message box that closes itself after the time, giving MB_TIMEDOUT then
inline constexpr int MB_TIMEDOUT = 32000;
int nppExecMessageBoxTimeout(HWND parent, const wchar_t* text, const wchar_t* caption, UINT type, WORD language, DWORD milliseconds);

// The window procedure's WM_CLOSE of Notepad++'s main window, for the close events of the window: the procedure
// keeps the window open by returning 0. A null procedure stops this. Notepad++ closes itself without a close event
// for File > Exit, which NppExec sees through its notifications.
void nppExecWatchClose(HWND mainWindow, WNDPROC proc);

// The desktop's colours, from the application's palette
inline constexpr int COLOR_WINDOW = 5;
inline constexpr int COLOR_WINDOWTEXT = 8;
inline constexpr int COLOR_BTNFACE = 15;
inline constexpr int COLOR_3DFACE = 15;
inline constexpr int COLOR_GRAYTEXT = 17;
inline constexpr int COLOR_HIGHLIGHT = 13;
inline constexpr int COLOR_HIGHLIGHTTEXT = 14;
COLORREF GetSysColor(int index);

// The messages to controls that NppExec sends, which its SendMessage handles for Qt's controls
inline constexpr UINT EM_LIMITTEXT = 0x00C5;
inline constexpr UINT EM_SETLIMITTEXT = 0x00C5;

// Colour messages of controls, which layouts and palettes make unneeded here, so nothing sends them
inline constexpr UINT WM_CTLCOLOREDIT = 0x0133;
inline constexpr UINT WM_CTLCOLORLISTBOX = 0x0134;
inline constexpr UINT WM_CTLCOLORBTN = 0x0135;
inline constexpr UINT WM_CTLCOLORDLG = 0x0136;
inline constexpr UINT WM_CTLCOLORSTATIC = 0x0138;

inline constexpr UINT WM_SIZING = 0x0214; // not sent, as Qt keeps a window's minimum size

inline constexpr UINT WM_SETREDRAW = 0x000B;
inline constexpr UINT WM_SETTEXT = 0x000C;
inline constexpr UINT WM_SHOWWINDOW = 0x0018;

// ShellExecute's "open": an executable starts on its own with the parameters, and anything else opens with the
// desktop's application for it. Gives more than 32 on success, or an error as ShellExecute does.
INT_PTR nppExecShellExecute(const wchar_t* file, const wchar_t* parameters);
inline constexpr int SE_ERR_FNF = 2;
inline constexpr int SE_ERR_PNF = 3;
inline constexpr int SE_ERR_ACCESSDENIED = 5;
inline constexpr int SE_ERR_OOM = 8;
inline constexpr int SE_ERR_SHARE = 26;
inline constexpr int SE_ERR_ASSOCINCOMPLETE = 27;
inline constexpr int SE_ERR_DDETIMEOUT = 28;
inline constexpr int SE_ERR_DDEFAIL = 29;
inline constexpr int SE_ERR_DDEBUSY = 30;
inline constexpr int SE_ERR_NOASSOC = 31;
inline constexpr int SE_ERR_DLLNOTFOUND = 32;

inline constexpr int SW_HIDE = 0;
inline constexpr int SW_SHOWNORMAL = 1;
inline constexpr int SW_NORMAL = 1;
inline constexpr int SW_SHOWMINIMIZED = 2;
inline constexpr int SW_SHOWMAXIMIZED = 3;
inline constexpr int SW_SHOWNOACTIVATE = 4;
inline constexpr int SW_SHOW = 5;
inline constexpr int SW_MINIMIZE = 6;
inline constexpr int SW_SHOWNA = 8;
inline constexpr int SW_RESTORE = 9;

inline constexpr int BST_UNCHECKED = 0;
inline constexpr int BST_CHECKED = 1;
inline constexpr int BST_INDETERMINATE = 2;

inline constexpr int CB_ERR = -1;
inline constexpr int LB_ERR = -1;
