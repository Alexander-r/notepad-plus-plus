// Qt port: NppExec's dialogs, whose procedures of the original get their messages from Qt widgets.
//
// Each dialog template of NppExec.rc is a Qt form, built in NppExecForms.cpp with the control IDs in the "nppId"
// property. Its procedure gets WM_INITDIALOG, then WM_COMMAND from the controls' signals as Windows sends it
// (BN_CLICKED, EN_CHANGE, CBN_SELCHANGE, LBN_SELCHANGE...), IDCANCEL for Escape and WM_CLOSE for the window's close
// button. EndDialog ends a modal dialog with its result. Layouts do what the procedures' WM_SIZE code did on Windows,
// and palettes what their colour messages did, so those are not sent.
//
// SendMessage and PostMessage reach the procedure of NppExec's own dialogs, on the GUI thread, and go on to
// Notepad++ for every other window, so that the plugin imports only those two from it. What Notepad++ sends to a
// dialog, such as the docking notifications to the console, comes as its NppMessageEvent to the procedure.

#pragma once

#include "NppExecWidgets.h"

class QDialog;
class QWidget;

// The form of the template with that ID, or null when there is none: see NppExecForms.cpp
QWidget* nppExecBuildForm(int idDlg, QWidget* parent);

// As DialogBox: runs the modal dialog and gives what EndDialog was given
INT_PTR nppExecDialogBox(int idDlg, HWND parent, DLGPROC proc, LPARAM param = 0);
// As CreateDialog: a modeless dialog, or a child widget for a docked panel, shown by the caller
HWND nppExecCreateDialog(int idDlg, HWND parent, DLGPROC proc, LPARAM param = 0);
// Connects the controls of the form to the procedure: for a form made elsewhere, such as a page of a tab
void nppExecConnectControls(QWidget* form, DLGPROC proc);
// Gives a form made with its dialog, such as a page of a tab, its procedure: connects its controls, lets
// SendMessage reach it and sends it WM_INITDIALOG
void nppExecInitForm(HWND form, DLGPROC proc, LPARAM param = 0);
BOOL EndDialog(HWND dialog, INT_PTR result);
BOOL DestroyWindow(HWND window);


LRESULT nppExecSendMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
bool nppExecPostMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
// As SendDlgItemMessage, for the few messages that NppExec sends to its controls
LRESULT SendDlgItemMessage(HWND dialog, int id, UINT message, WPARAM wParam, LPARAM lParam);
UINT IsDlgButtonChecked(HWND dialog, int id);
BOOL CheckDlgButton(HWND dialog, int id, UINT check);
BOOL CheckRadioButton(HWND dialog, int firstId, int lastId, int checkId);
BOOL SetDlgItemText(HWND dialog, int id, const wchar_t* text);
UINT GetDlgItemText(HWND dialog, int id, wchar_t* text, int size);

#define SendMessage nppExecSendMessage
#define PostMessage nppExecPostMessage
