// Qt port: the messages of Notepad++, which a plugin takes from the program, for the unit tests that run without
// it. The code they test talks to the mocked editor, so nothing calls these.

#include "NppMessages.h"

LRESULT SendMessage(HWND, UINT, WPARAM, LPARAM) { return 0; }
bool PostMessage(HWND, UINT, WPARAM, LPARAM) { return false; }
