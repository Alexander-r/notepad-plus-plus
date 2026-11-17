// Qt port: what NppExec uses of <richedit.h>, for the console, a QPlainTextEdit here (NppExecRichEditWidget).
//
// CAnyRichEdit keeps the interface of the original over the Qt widget. The console's key and mouse events reach
// NppExec's handler as RichEdit's EN_MSGFILTER notification, with the messages and virtual key codes of Windows that it
// reads, so that its handling of the input line, history and completion stays as it is.

#pragma once

#include "NppExecWin.h"
#include "NppKeys.h"

#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCharFormat>

#include <functional>

inline constexpr UINT WM_KEYDOWN = 0x0100;
inline constexpr UINT WM_KEYUP = 0x0101;
inline constexpr UINT WM_CHAR = 0x0102;
inline constexpr UINT WM_SYSKEYDOWN = 0x0104;
inline constexpr UINT WM_SYSKEYUP = 0x0105;
inline constexpr UINT WM_SYSCHAR = 0x0106;
inline constexpr UINT WM_PASTE = 0x0302;
inline constexpr WPARAM MK_LBUTTON = 0x0001;
inline constexpr WPARAM MK_SHIFT = 0x0004;
inline constexpr WPARAM MK_CONTROL = 0x0008;

// The notification of a message that RichEdit lets the dialog see before it handles it
struct MSGFILTER
{
	NMHDR nmhdr;
	UINT msg;
	WPARAM wParam;
	LPARAM lParam;
};

struct CHARRANGE
{
	LONG cpMin;
	LONG cpMax;
};

inline constexpr DWORD CFM_BOLD = 0x00000001;
inline constexpr DWORD CFM_ITALIC = 0x00000002;
inline constexpr DWORD CFM_UNDERLINE = 0x00000004;
inline constexpr DWORD CFM_STRIKEOUT = 0x00000008;
inline constexpr DWORD CFM_PROTECTED = 0x00000010;
inline constexpr DWORD CFM_LINK = 0x00000020;
inline constexpr DWORD CFM_SIZE = 0x80000000;
inline constexpr DWORD CFM_COLOR = 0x40000000;
inline constexpr DWORD CFM_FACE = 0x20000000;
inline constexpr DWORD CFM_OFFSET = 0x10000000;
inline constexpr DWORD CFM_CHARSET = 0x08000000;
inline constexpr DWORD CFM_EFFECTS = CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_COLOR | CFM_STRIKEOUT | CFM_PROTECTED | CFM_LINK;
inline constexpr DWORD CFE_BOLD = 0x0001;
inline constexpr DWORD CFE_ITALIC = 0x0002;
inline constexpr DWORD CFE_UNDERLINE = 0x0004;
inline constexpr DWORD CFE_STRIKEOUT = 0x0008;
inline constexpr DWORD CFE_PROTECTED = 0x0010;
inline constexpr DWORD CFE_LINK = 0x0020;
inline constexpr DWORD CFE_AUTOCOLOR = 0x40000000;
inline constexpr DWORD SCF_SELECTION = 0x0001;
inline constexpr DWORD SCF_WORD = 0x0002;
inline constexpr DWORD SCF_DEFAULT = 0x0000;
inline constexpr DWORD SCF_ALL = 0x0004;

inline constexpr DWORD ENM_NONE = 0x00000000;
inline constexpr DWORD ENM_CHANGE = 0x00000001;
inline constexpr DWORD ENM_SELCHANGE = 0x00080000;
inline constexpr DWORD ENM_KEYEVENTS = 0x00010000;
inline constexpr DWORD ENM_MOUSEEVENTS = 0x00020000;
inline constexpr UINT EN_MSGFILTER = 0x0700;
inline constexpr UINT EN_SELCHANGE = 0x0702;
inline constexpr UINT EN_LINK = 0x070b;

inline constexpr BYTE DEFAULT_CHARSET = 1;

// RichEdit's search, for EM_FINDTEXT
struct FINDTEXT
{
	CHARRANGE chrg;
	const wchar_t* lpstrText;
};
inline constexpr WPARAM FR_DOWN = 0x00000001;
inline constexpr WPARAM FR_WHOLEWORD = 0x00000002;
inline constexpr WPARAM FR_MATCHCASE = 0x00000004;
inline constexpr UINT EM_FINDTEXT = 0x0438;

// The face of a font, as NppExec keeps it in its settings
inline constexpr int LF_FACESIZE = 32;
inline constexpr LONG FW_NORMAL = 400;
inline constexpr LONG FW_BOLD = 700;
struct LOGFONT
{
	LONG lfHeight;
	LONG lfWidth;
	LONG lfEscapement;
	LONG lfOrientation;
	LONG lfWeight;
	BYTE lfItalic;
	BYTE lfUnderline;
	BYTE lfStrikeOut;
	BYTE lfCharSet;
	BYTE lfOutPrecision;
	BYTE lfClipPrecision;
	BYTE lfQuality;
	BYTE lfPitchAndFamily;
	wchar_t lfFaceName[LF_FACESIZE];
};

// The console's text: what RichEdit keeps for a control besides its text
class NppExecRichEditWidget : public QPlainTextEdit
{
public:
	explicit NppExecRichEditWidget(QWidget* parent = nullptr);

	// The format for the text that the next ReplaceSelText inserts, as RichEdit's insertion point has one
	QTextCharFormat pendingFormat;
	bool hasPendingFormat = false;
	int textLimit = 0x7FFFFFFE;
	// RichEdit gives a line with its end; the edit control of Windows, which the Execute dialog's script is, without
	bool lineWithEnd = true;
	DWORD eventMask = ENM_NONE;

	int firstVisibleLine() const { return firstVisibleBlock().blockNumber(); }

	// RichEdit's EN_MSGFILTER: a message of a key or the mouse, which the handler blocks by returning nonzero or by
	// setting the message or its wParam to 0, or changes into another key or character by changing wParam
	std::function<LRESULT(UINT& message, WPARAM& wParam, LPARAM& lParam)> msgFilter;

protected:
	void keyPressEvent(QKeyEvent* event) override;
	void keyReleaseEvent(QKeyEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	void insertFromMimeData(const QMimeData* source) override;
	bool canInsertFromMimeData(const QMimeData* source) const override;

private:
	// What the handler left of the message: blocked, as it was, or changed
	enum class Filtered { blocked, unchanged, changed };
	Filtered filter(UINT message, WPARAM& wParam, LPARAM lParam);
};

// The edit messages that NppExec sends to its text widgets, handled as RichEdit and the edit control of Windows do:
// gives false when the message is not one of them
bool nppExecRichEditMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* result);

inline constexpr UINT EM_GETSEL = 0x00B0;
inline constexpr UINT EM_SETSEL = 0x00B1;
inline constexpr UINT EM_SCROLLCARET = 0x00B7;
inline constexpr UINT EM_GETLINECOUNT = 0x00BA;
inline constexpr UINT EM_LINEINDEX = 0x00BB;
inline constexpr UINT EM_LINELENGTH = 0x00C1;
inline constexpr UINT EM_REPLACESEL = 0x00C2;
inline constexpr UINT EM_GETLINE = 0x00C4;
inline constexpr UINT EM_LINEFROMCHAR = 0x00C9;
inline constexpr UINT EM_GETFIRSTVISIBLELINE = 0x00CE;
inline constexpr UINT WM_SETFONT = 0x0030;
inline constexpr UINT WM_GETFONT = 0x0031;
inline constexpr UINT EM_SETBKGNDCOLOR = 0x0443;
inline constexpr UINT EM_SETTARGETDEVICE = 0x0448;

// The virtual key code of Windows for a Qt key, or 0
UINT nppExecVirtualKey(int qtKey);
inline constexpr int VK_LBUTTON = 0x01;
inline constexpr int VK_RBUTTON = 0x02;
// As GetKeyState: negative while the key (VK_SHIFT, VK_CONTROL, VK_MENU) or mouse button is down
SHORT GetKeyState(int virtualKey);
