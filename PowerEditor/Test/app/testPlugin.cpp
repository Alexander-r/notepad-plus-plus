// A plugin written as for the original Notepad++: the same exports and the same messages. The app test loads it.

#include <cstring>

#include <Scintilla.h>

#include "NppMessages.h"
#include "PluginInterface.h"

namespace
{
	NppData g_nppData;
	int g_readyCount = 0;
	int g_bufferActivatedCount = 0;
	long g_lastVersion = 0;

	HWND currentScintilla()
	{
		int which = -1;
		::SendMessage(g_nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<LPARAM>(&which));
		return (which == 0) ? g_nppData._scintillaMainHandle : g_nppData._scintillaSecondHandle;
	}

	void insertGreeting()
	{
		const char text[] = "greetings from a plugin";
		::SendMessage(currentScintilla(), SCI_ADDTEXT, std::strlen(text), reinterpret_cast<LPARAM>(text));
	}

	void askVersion()
	{
		g_lastVersion = static_cast<long>(::SendMessage(g_nppData._nppHandle, NPPM_GETNPPVERSION, 0, 0));
	}

	ShortcutKey g_key { true, true, true, 'G' };
	FuncItem g_funcItems[3];
}

extern "C"
{
	NPP_PLUGIN_EXPORT void setInfo(NppData data)
	{
		g_nppData = data;
		std::wcscpy(g_funcItems[0]._itemName, L"Insert greeting");
		g_funcItems[0]._pFunc = insertGreeting;
		g_funcItems[0]._pShKey = &g_key;
		// a separator
		std::wcscpy(g_funcItems[2]._itemName, L"Ask version");
		g_funcItems[2]._pFunc = askVersion;
	}

	NPP_PLUGIN_EXPORT const wchar_t* getName()
	{
		return L"Test Plugin";
	}

	NPP_PLUGIN_EXPORT FuncItem* getFuncsArray(int* nbF)
	{
		*nbF = 3;
		return g_funcItems;
	}

	NPP_PLUGIN_EXPORT void beNotified(SCNotification* notification)
	{
		if (notification->nmhdr.code == NPPN_READY)
			++g_readyCount;
		else if (notification->nmhdr.code == NPPN_BUFFERACTIVATED)
			++g_bufferActivatedCount;
	}

	NPP_PLUGIN_EXPORT LRESULT messageProc(UINT, WPARAM, LPARAM)
	{
		return TRUE;
	}

	NPP_PLUGIN_EXPORT BOOL isUnicode()
	{
		return TRUE;
	}

	// For the test to look inside
	NPP_PLUGIN_EXPORT int testPluginReadyCount() { return g_readyCount; }
	NPP_PLUGIN_EXPORT int testPluginBufferActivatedCount() { return g_bufferActivatedCount; }
	NPP_PLUGIN_EXPORT long testPluginLastVersion() { return g_lastVersion; }
	NPP_PLUGIN_EXPORT int testPluginCommandId(int index) { return g_funcItems[index]._cmdID; }
}
