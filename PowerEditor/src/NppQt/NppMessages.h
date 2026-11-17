// This file is part of Notepad++ project
//
// Message routing between the application's objects. The application logic and the plugin API both talk
// in (message, wParam, lParam) terms: NPPM_* to the main window, SCI_* to editors, and WM_COMMAND/WM_NOTIFY
// inside dialogs. SendMessage() and PostMessage() deliver those to the handler an object has registered
// for its widget. This is only a router: Qt widgets do not understand these messages by themselves, and
// the WM_* numbers below exist only for delivery to the original logic that is written around them.

#pragma once

#include <QEvent>

#include <functional>
#include <memory>

#include "NppPlatform.h"

using NppMessageHandler = std::function<LRESULT(UINT message, WPARAM wParam, LPARAM lParam)>;

void nppRegisterMessageTarget(HWND target, NppMessageHandler handler);
void nppUnregisterMessageTarget(HWND target);

// Synchronous. A call from a worker thread blocks until the GUI thread has handled it.
LRESULT SendMessage(HWND target, UINT message, WPARAM wParam = 0, LPARAM lParam = 0);
// Queued to the GUI thread, and dropped if the target has gone away by then.
bool PostMessage(HWND target, UINT message, WPARAM wParam = 0, LPARAM lParam = 0);

// What a widget without a registered handler gets from SendMessage and PostMessage, such as a plugin's panel or
// dialog: the message as this event, which the widget handles in event() or an event filter, setting result as its
// window procedure would give it. This is how a plugin's docked panel hears WM_NOTIFY with DMN_CLOSE, DMN_FLOAT,
// DMN_DOCK and the others, as on Windows, and answers DMN_CLOSE with nonzero to stay open.
class NppMessageEvent : public QEvent
{
public:
	static constexpr QEvent::Type eventType = static_cast<QEvent::Type>(QEvent::MaxUser - 0x4E50);

	NppMessageEvent(UINT message_, WPARAM wParam_, LPARAM lParam_)
		: QEvent(eventType), message(message_), wParam(wParam_), lParam(lParam_) {}

	UINT message;
	WPARAM wParam;
	LPARAM lParam;
	LRESULT result = 0;
};

// The main window, for code that needs a parent or a message target without knowing its class.
HWND nppMainWindow();
void nppSetMainWindow(HWND window);

constexpr WPARAM MAKEWPARAM(WORD low, WORD high)
{
	return static_cast<WPARAM>(low) | (static_cast<WPARAM>(high) << 16);
}

constexpr LPARAM MAKELPARAM(WORD low, WORD high)
{
	return static_cast<LPARAM>(static_cast<DWORD>(low) | (static_cast<DWORD>(high) << 16));
}

inline constexpr UINT WM_NULL = 0x0000;
inline constexpr UINT WM_COPYDATA = 0x004A;
inline constexpr UINT WM_SETTINGCHANGE = 0x001A;
inline constexpr UINT WM_ACTIVATEAPP = 0x001C;
inline constexpr UINT WM_QUERYENDSESSION = 0x0011;
inline constexpr UINT WM_ENDSESSION = 0x0016;
inline constexpr UINT WM_APPCOMMAND = 0x0319;
inline constexpr UINT WM_INITMENUPOPUP = 0x0117;
inline constexpr UINT WM_LBUTTONDOWN = 0x0201;
inline constexpr UINT WM_LBUTTONUP = 0x0202;
inline constexpr UINT WM_LBUTTONDBLCLK = 0x0203;
inline constexpr UINT WM_RBUTTONUP = 0x0205;
inline constexpr UINT WM_MBUTTONUP = 0x0208;
inline constexpr UINT WM_DESTROY = 0x0002;
inline constexpr UINT WM_MOVE = 0x0003;
inline constexpr UINT WM_SIZE = 0x0005;
inline constexpr UINT WM_ACTIVATE = 0x0006;
inline constexpr UINT WM_SETFOCUS = 0x0007;
inline constexpr UINT WM_KILLFOCUS = 0x0008;
inline constexpr UINT WM_CLOSE = 0x0010;
inline constexpr UINT WM_NOTIFY = 0x004E;
inline constexpr UINT WM_CONTEXTMENU = 0x007B;
inline constexpr UINT WM_INITDIALOG = 0x0110;
inline constexpr UINT WM_COMMAND = 0x0111;
inline constexpr UINT WM_TIMER = 0x0113;
inline constexpr UINT WM_HSCROLL = 0x0114;
inline constexpr UINT WM_VSCROLL = 0x0115;
inline constexpr UINT WM_MOUSEWHEEL = 0x020A;
inline constexpr UINT WM_DROPFILES = 0x0233;
inline constexpr UINT WM_CLIPBOARDUPDATE = 0x031D;
inline constexpr UINT WM_SYSCOMMAND = 0x0112;
inline constexpr WPARAM SC_MINIMIZE = 0xF020;
inline constexpr WPARAM SC_CLOSE = 0xF060;
inline constexpr WPARAM MK_RBUTTON = 0x0002;
inline constexpr LPARAM ENDSESSION_CRITICAL = 0x40000000;
inline constexpr WPARAM APPCOMMAND_BROWSER_BACKWARD = 1;
inline constexpr WPARAM APPCOMMAND_BROWSER_FORWARD = 2;

// Data from another instance of the application
struct COPYDATASTRUCT
{
	ULONG_PTR dwData = 0;
	DWORD cbData = 0;
	void* lpData = nullptr;
};

constexpr int GET_X_LPARAM(LPARAM lParam) { return static_cast<short>(LOWORD(lParam)); }
constexpr int GET_Y_LPARAM(LPARAM lParam) { return static_cast<short>(HIWORD(lParam)); }

// Header of WM_NOTIFY payloads, with the same layout as Scintilla's notification header
struct NMHDR
{
	void* hwndFrom = nullptr;
	UINT_PTR idFrom = 0;
	UINT code = 0;
};

// A mouse notification, which tells the status bar part in dwItemSpec
struct NMMOUSE
{
	NMHDR hdr;
	DWORD_PTR dwItemSpec = 0;
};

inline constexpr UINT NM_FIRST = 0;
inline constexpr UINT NM_CLICK = NM_FIRST - 2;
inline constexpr UINT NM_DBLCLK = NM_FIRST - 3;
inline constexpr UINT NM_RCLICK = NM_FIRST - 5;
inline constexpr UINT TCN_FIRST = static_cast<UINT>(-550);
inline constexpr UINT TCN_SELCHANGE = TCN_FIRST - 1;

// wParam low word of WM_ACTIVATE
inline constexpr WORD WA_INACTIVE = 0;
inline constexpr WORD WA_ACTIVE = 1;

// Common dialog results and command IDs
inline constexpr int IDOK = 1;
inline constexpr int IDCANCEL = 2;
inline constexpr int IDABORT = 3;
inline constexpr int IDRETRY = 4;
inline constexpr int IDIGNORE = 5;
inline constexpr int IDYES = 6;
inline constexpr int IDNO = 7;
inline constexpr int IDCLOSE = 8;

// Notification codes, carried in the high word of WM_COMMAND's wParam
inline constexpr WORD BN_CLICKED = 0;
inline constexpr WORD BN_DBLCLK = 5;
inline constexpr WORD STN_CLICKED = 0;
inline constexpr WORD EN_SETFOCUS = 0x0100;
inline constexpr WORD EN_KILLFOCUS = 0x0200;
inline constexpr WORD EN_CHANGE = 0x0300;
inline constexpr WORD EN_UPDATE = 0x0400;
inline constexpr WORD LBN_SELCHANGE = 1;
inline constexpr WORD LBN_DBLCLK = 2;
inline constexpr WORD CBN_SELCHANGE = 1;
inline constexpr WORD CBN_DBLCLK = 2;
inline constexpr WORD CBN_SETFOCUS = 3;
inline constexpr WORD CBN_KILLFOCUS = 4;
inline constexpr WORD CBN_EDITCHANGE = 5;
inline constexpr WORD CBN_EDITUPDATE = 6;
inline constexpr WORD CBN_DROPDOWN = 7;
inline constexpr WORD CBN_CLOSEUP = 8;
inline constexpr WORD CBN_SELENDOK = 9;

// Slider notification carried in WM_HSCROLL's wParam
inline constexpr WORD SB_THUMBTRACK = 5;
inline constexpr WORD SB_ENDSCROLL = 8;
