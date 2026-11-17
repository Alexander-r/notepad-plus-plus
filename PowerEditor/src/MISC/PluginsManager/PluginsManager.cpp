// This file is part of Notepad++ project
// Copyright (C)2021 Don HO <don.h@free.fr>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.


#include "PluginsManager.h"

#include <QAction>
#include <QDir>
#include <QLibrary>
#include <QMenu>

#include <cinttypes>
#include <cstdio>
#include <cwchar>
#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <ILexer.h>
#include <Lexilla.h>
#include <Scintilla.h>

#include "Common.h"
#include "Notepad_plus_msgs.h"
#include "NppConstants.h"
#include "NppDarkMode.h"
#include "NppMenu.h"
#include "NppXml.h"
#include "Parameters.h"
#include "PluginInterface.h"
#include "menuCmdID.h"
#include "resource.h"
#include "shortcut.h"

using namespace std;

const wchar_t * USERMSG = L" is not compatible with the current version of Notepad++.\n\n\
Do you want to remove this plugin from the plugins directory to prevent this message from the next launch?";

PluginInfo::~PluginInfo()
{
	delete _pluginMenu;

	if (_hLib)
		_hLib->unload();
}

PluginsManager::~PluginsManager()
{
	// The plugins stay loaded until the program ends, as their panels and dialogs belong to the main window, which is
	// not deleted before. On Windows the windows were destroyed before the DLLs were freed. A widget whose code is gone
	// crashes whatever touches it later, such as Qt's accessibility at the end. QLibrary does not unload when it goes.
	for (auto& pluginInfo : _pluginInfos)
		pluginInfo->_hLib.reset();
}

int PluginsManager::loadPluginFromPath(const wchar_t *pluginFilePath)
{
	const wchar_t *pluginFileName = PathFindFileName(pluginFilePath);
	if (isInLoadedDlls(pluginFileName))
		return 0;

	NppParameters& nppParams = NppParameters::getInstance();

	auto pi = std::make_unique<PluginInfo>();
	try {
		pi->_moduleName = pluginFileName;

		// An architecture mismatch comes back as the loader's error
		pi->_hLib = std::make_unique<QLibrary>(QString::fromWCharArray(pluginFilePath));
		pi->_hLib->setLoadHints(QLibrary::ResolveAllSymbolsHint);
		if (!pi->_hLib->load())
			throw pi->_hLib->errorString().toStdWString();

		pi->_pFuncIsUnicode = reinterpret_cast<PFUNCISUNICODE>(pi->_hLib->resolve("isUnicode"));
		if (!pi->_pFuncIsUnicode || !pi->_pFuncIsUnicode())
			throw wstring(L"This ANSI plugin is not compatible with your Unicode Notepad++.");

		pi->_pFuncSetInfo = reinterpret_cast<PFUNCSETINFO>(pi->_hLib->resolve("setInfo"));

		if (!pi->_pFuncSetInfo)
			throw wstring(L"Missing \"setInfo\" function");

		pi->_pFuncGetName = reinterpret_cast<PFUNCGETNAME>(pi->_hLib->resolve("getName"));
		if (!pi->_pFuncGetName)
			throw wstring(L"Missing \"getName\" function");
		pi->_funcName = pi->_pFuncGetName();

		pi->_pBeNotified = reinterpret_cast<PBENOTIFIED>(pi->_hLib->resolve("beNotified"));
		if (!pi->_pBeNotified)
			throw wstring(L"Missing \"beNotified\" function");

		pi->_pMessageProc = reinterpret_cast<PMESSAGEPROC>(pi->_hLib->resolve("messageProc"));
		if (!pi->_pMessageProc)
			throw wstring(L"Missing \"messageProc\" function");

		pi->_pFuncSetInfo(_nppData);

		pi->_pFuncGetFuncsArray = reinterpret_cast<PFUNCGETFUNCSARRAY>(pi->_hLib->resolve("getFuncsArray"));
		if (!pi->_pFuncGetFuncsArray)
			throw wstring(L"Missing \"getFuncsArray\" function");

		pi->_funcItems = pi->_pFuncGetFuncsArray(&pi->_nbFuncItem);

		if ((!pi->_funcItems) || (pi->_nbFuncItem <= 0))
			throw wstring(L"Missing \"FuncItems\" array, or the nb of Function Item is not set correctly");

		pi->_pluginMenu = new QMenu;

		Lexilla::GetLexerCountFn GetLexerCount = reinterpret_cast<Lexilla::GetLexerCountFn>(pi->_hLib->resolve(LEXILLA_GETLEXERCOUNT));
		// it's a lexer plugin
		if (GetLexerCount)
		{
			Lexilla::GetLexerNameFn GetLexerName = reinterpret_cast<Lexilla::GetLexerNameFn>(pi->_hLib->resolve(LEXILLA_GETLEXERNAME));
			if (!GetLexerName)
				throw wstring(L"Loading GetLexerName function failed.");

			//Lexilla::GetLexerFactoryFn GetLexerFactory = (Lexilla::GetLexerFactoryFn)::GetProcAddress(pi->_hLib, LEXILLA_GETLEXERFACTORY);
			//if (!GetLexerFactory)
				//throw wstring(L"Loading GetLexerFactory function failed.");

			Lexilla::CreateLexerFn CreateLexer = reinterpret_cast<Lexilla::CreateLexerFn>(pi->_hLib->resolve(LEXILLA_CREATELEXER));
			if (!CreateLexer)
				throw wstring(L"Loading CreateLexer function failed.");

			//Lexilla::GetLibraryPropertyNamesFn GetLibraryPropertyNames = reinterpret_cast<Lexilla::GetLibraryPropertyNamesFn>(pi->_hLib->resolve(LEXILLA_GETLIBRARYPROPERTYNAMES));
			//if (!GetLibraryPropertyNames)
				//throw wstring(L"Loading GetLibraryPropertyNames function failed.");

			//Lexilla::SetLibraryPropertyFn SetLibraryProperty = reinterpret_cast<Lexilla::SetLibraryPropertyFn>(pi->_hLib->resolve(LEXILLA_SETLIBRARYPROPERTY));
			//if (!SetLibraryProperty)
				//throw wstring(L"Loading SetLibraryProperty function failed.");

			//Lexilla::GetNameSpaceFn GetNameSpace = reinterpret_cast<Lexilla::GetNameSpaceFn>(pi->_hLib->resolve(LEXILLA_GETNAMESPACE));
			//if (!GetNameSpace)
				//throw wstring(L"Loading GetNameSpace function failed.");

			// Assign a buffer for the lexer name.
			char lexName[MAX_EXTERNAL_LEXER_NAME_LEN]{};

			int numLexers = GetLexerCount();
			std::unique_ptr<ExternalLangContainer> containers[NB_MAX_EXTERNAL_LANG]{};
			numLexers = numLexers > NB_MAX_EXTERNAL_LANG ? NB_MAX_EXTERNAL_LANG : numLexers;

			for (int x = 0; x < numLexers; ++x)
			{
				GetLexerName(x, lexName, MAX_EXTERNAL_LEXER_NAME_LEN);
				if (!nppParams.isExistingExternalLangName(lexName) && nppParams.ExternalLangHasRoom())
				{
					containers[x] = std::make_unique<ExternalLangContainer>();
					containers[x]->_name = lexName;
					containers[x]->fnCL = CreateLexer;
					//containers[x]->fnGLPN = GetLibraryPropertyNames;
					//containers[x]->fnSLP = SetLibraryProperty;
				}
				else
				{
					containers[x] = nullptr;
				}
			}

			namespace fs = ::std::filesystem;

			fs::path pluginPath = L"plugins";
			pluginPath /= L"Config";
			pluginPath /= pi->_moduleName;
			pluginPath.replace_extension(".xml");

			fs::path xmlPath = fs::path(nppParams.getNppPath()) / pluginPath;

			if (!doesFileExist(xmlPath.wstring().c_str()))
			{
				xmlPath = fs::path(nppParams.getAppDataNppDir()) / pluginPath;

				if (!doesFileExist(xmlPath.wstring().c_str()))
				{
					throw std::wstring(xmlPath.wstring() + L" is missing.");
				}
			}

			NppXml::Document pXmlDoc = new NppXml::NewDocument();

			if (!NppXml::loadFile(pXmlDoc, xmlPath.wstring().c_str()))
			{
				delete pXmlDoc;
				pXmlDoc = nullptr;
				throw std::wstring(xmlPath.wstring() + L" failed to load.");
			}

			for (int x = 0; x < numLexers; ++x) // postpone adding in case the xml is missing/corrupt
			{
				if (containers[x] != nullptr)
					nppParams.addExternalLangToEnd(std::move(containers[x]));
			}

			nppParams.getExternalLexerFromXmlTree(pXmlDoc);
			nppParams.getExternalLexerDoc()->emplace_back(pXmlDoc, xmlPath.wstring());


			//const char *pDllName = wmc.wchar2char(pluginFilePath, CP_ACP);
			//::SendMessage(_nppData._scintillaMainHandle, SCI_LOADLEXERLIBRARY, 0, reinterpret_cast<LPARAM>(pDllName));


		}
		addInLoadedDlls(pluginFilePath, pluginFileName);
		_pluginInfos.push_back(std::move(pi));
		return static_cast<int>(_pluginInfos.size() - 1);
	}
	catch (std::exception& e)
	{
		pluginExceptionAlert(pluginFileName, e);
		return -1;
	}
	catch (const std::wstring& s)
	{
		std::wstring msg = s;
		msg += L"\n\n";
		msg += pluginFileName;
		msg += USERMSG;
		if (NppDarkMode::darkMessageBoxW(_nppData._nppHandle, msg.c_str(), pluginFilePath, MB_YESNO) == IDYES)
		{
			std::error_code error;
			std::filesystem::remove(pluginFilePath, error);
		}
		return -1;
	}
	catch (...)
	{
		wstring msg = L"Failed to load";
		msg += L"\n\n";
		msg += pluginFileName;
		msg += USERMSG;
		if (NppDarkMode::darkMessageBoxW(_nppData._nppHandle, msg.c_str(), pluginFilePath, MB_YESNO) == IDYES)
		{
			std::error_code error;
			std::filesystem::remove(pluginFilePath, error);
		}
		return -1;
	}
}

bool PluginsManager::loadPlugins(const wchar_t* dir)
{
	if (_isDisabled)
		return false;

	NppParameters& nppParams = NppParameters::getInstance();

	wstring pluginsFolder;
	if (dir && dir[0])
	{
		pluginsFolder = dir;
	}
	else
	{
		pluginsFolder = nppParams.getNppPath();
		pathAppend(pluginsFolder, L"plugins");
	}

	// A plugin is the library named after its folder: plugins/Name/Name.so. "Config" holds settings, not plugins,
	// and "disabled" holds deactivated plugins, so neither is scanned. As CMake names a module libName.so by default, that goes too.
	const QDir root(QString::fromStdWString(pluginsFolder));
	const QStringList folders = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
	for (const QString& folder : folders)
	{
		const wstring folderName = folder.toStdWString();
		if (_wcsicmp(folderName.c_str(), L"Config") == 0 || _wcsicmp(folderName.c_str(), L"disabled") == 0)
			continue;

		const QDir pluginDir(root.filePath(folder));
		for (const QString& name : { folder + QStringLiteral(".so"), QStringLiteral("lib") + folder + QStringLiteral(".so") })
		{
			if (pluginDir.exists(name))
			{
				loadPluginFromPath(pluginDir.filePath(name).toStdWString().c_str());
				break;
			}
		}
	}

	return true;
}

// return true if cmdID found and its shortcut is enable
// false otherwise
bool PluginsManager::getShortcutByCmdID(int cmdID, ShortcutKey* sk)
{
	if (cmdID == 0 || !sk)
		return false;

	const vector<PluginCmdShortcut> & pluginCmdSCList = (NppParameters::getInstance()).getPluginCommandList();

	for (size_t i = 0, len = pluginCmdSCList.size(); i < len ; ++i)
	{
		if (pluginCmdSCList[i].getID() == static_cast<unsigned long>(cmdID))
		{
			const KeyCombo & kc = pluginCmdSCList[i].getKeyCombo();
			if (kc._key == 0x00)
				return false;

			sk->_isAlt = kc._isAlt;
			sk->_isCtrl = kc._isCtrl;
			sk->_isShift = kc._isShift;
			sk->_key = kc._key;
			return true;
		}
	}
	return false;
}

// returns false if cmdID not provided, true otherwise
bool PluginsManager::removeShortcutByCmdID(int cmdID)
{
	if (cmdID == 0)
		return false;

	NppParameters& nppParam = NppParameters::getInstance();
	vector<PluginCmdShortcut> & pluginCmdSCList = nppParam.getPluginCommandList();

	for (size_t i = 0, len = pluginCmdSCList.size(); i < len; ++i)
	{
		if (pluginCmdSCList[i].getID() == static_cast<unsigned long>(cmdID))
		{
			//remove shortcut
			pluginCmdSCList[i].clear();

			// inform accelerator instance
			nppParam.getAccelerator()->updateShortcuts();

			// set dirty flag to force writing shortcuts.xml on shutdown
			nppParam.setShortcutDirty();
			break;
		}
	}
	return true;
}

void PluginsManager::addInMenuFromPMIndex(int i)
{
	vector<PluginCmdShortcut> & pluginCmdSCList = (NppParameters::getInstance()).getPluginCommandList();
	nppInsertSubMenuAt(_hPluginsMenu, i, _pluginInfos[i]->_pluginMenu, _pluginInfos[i]->_funcName);

	unsigned short j = 0;
	for ( ; j < _pluginInfos[i]->_nbFuncItem ; ++j)
	{
		if (_pluginInfos[i]->_funcItems[j]._pFunc == nullptr)
		{
			nppInsertSeparatorAt(_pluginInfos[i]->_pluginMenu, j);
			continue;
		}

		_pluginsCommands.push_back(PluginCommand(_pluginInfos[i]->_moduleName.c_str(), j, _pluginInfos[i]->_funcItems[j]._pFunc));

		const int cmdID = ID_PLUGINS_CMD + static_cast<int>(_pluginsCommands.size() - 1);
		_pluginInfos[i]->_funcItems[j]._cmdID = cmdID;
		const string itemName = wstring2string(_pluginInfos[i]->_funcItems[j]._itemName, CP_UTF8);

		if (_pluginInfos[i]->_funcItems[j]._pShKey)
		{
			ShortcutKey & sKey = *(_pluginInfos[i]->_funcItems[j]._pShKey);
			PluginCmdShortcut pcs(Shortcut(itemName.c_str(), sKey._isCtrl, sKey._isAlt, sKey._isShift, sKey._key), cmdID, wstring2string(_pluginInfos[i]->_moduleName, CP_UTF8).c_str(), j);
			pluginCmdSCList.push_back(pcs);
		}
		else
		{	//no ShortcutKey is provided, add an disabled shortcut (so it can still be mapped, Paramaters class can still index any changes and the toolbar wont be messed up)
			Shortcut sc(itemName.c_str(), false, false, false, 0x00);
			PluginCmdShortcut pcs(sc, cmdID, wstring2string(_pluginInfos[i]->_moduleName, CP_UTF8).c_str(), j);	//VK_NULL and everything disabled, the menu name is left alone
			pluginCmdSCList.push_back(pcs);
		}

		// The menu shows the shortcut by itself
		nppInsertMenuItemAt(_pluginInfos[i]->_pluginMenu, j, cmdID, _pluginInfos[i]->_funcItems[j]._itemName, _nppData._nppHandle);

		if (_pluginInfos[i]->_funcItems[j]._init2Check)
			nppCheckMenuCommand(_hPluginsMenu, cmdID, true);
	}
}

HMENU PluginsManager::initMenu(HMENU hMenu)
{
	size_t nbPlugin = _pluginInfos.size();

	if (!_hPluginsMenu)
	{
		_hPluginsMenu = nppSubMenuAt(hMenu, MENUINDEX_PLUGINS);

		if (nbPlugin > 0)
			nppInsertSeparatorAt(_hPluginsMenu, 0);
	}

	for (size_t i = 0; i < nbPlugin; ++i)
	{
		addInMenuFromPMIndex(static_cast<int>(i));
	}
	return _hPluginsMenu;
}


void PluginsManager::runPluginCommand(size_t i)
{
	if (i < _pluginsCommands.size())
	{
		if (_pluginsCommands[i]._pFunc != NULL)
		{
			try {
				_pluginsCommands[i]._pFunc();
			}
			catch (std::exception& e)
			{
				pluginExceptionAlert(_pluginsCommands[i]._pluginName.c_str(), e);
			}
			catch (...)
			{
				static constexpr size_t bufSize = 128;
				wchar_t funcInfo[bufSize] = { '\0' };
				swprintf(funcInfo, bufSize, L"runPluginCommand(size_t i : %zu)", i);
				pluginCrashAlert(_pluginsCommands[i]._pluginName.c_str(), funcInfo);
			}
		}
	}
}


void PluginsManager::runPluginCommand(const wchar_t *pluginName, int commandID)
{
	for (size_t i = 0, len = _pluginsCommands.size() ; i < len ; ++i)
	{
		if (!_wcsicmp(_pluginsCommands[i]._pluginName.c_str(), pluginName))
		{
			if (_pluginsCommands[i]._funcID == commandID)
			{
				try {
					_pluginsCommands[i]._pFunc();
				}
				catch (std::exception& e)
				{
					pluginExceptionAlert(_pluginsCommands[i]._pluginName.c_str(), e);
				}
				catch (...)
				{
					static constexpr size_t bufSize = 128;
					wchar_t funcInfo[bufSize] = { '\0' };
					swprintf(funcInfo, bufSize, L"runPluginCommand(const wchar_t *pluginName : %ls, int commandID : %d)", pluginName, commandID);
					pluginCrashAlert(_pluginsCommands[i]._pluginName.c_str(), funcInfo);
				}
			}
		}
	}
}

// send the notification to a specific plugin
void PluginsManager::notify(size_t indexPluginInfo, const SCNotification *notification)
{
	if (indexPluginInfo >= _pluginInfos.size())
		return;

	if (_pluginInfos[indexPluginInfo]->_hLib)
	{
		// To avoid the plugin change the data in SCNotification
		// Each notification to pass to a plugin is a copy of SCNotification instance
		SCNotification scNotif = *notification;
		try {
			_pluginInfos[indexPluginInfo]->_pBeNotified(&scNotif);
		}
		catch (std::exception& e)
		{
			pluginExceptionAlert(_pluginInfos[indexPluginInfo]->_moduleName.c_str(), e);
		}
		catch (...)
		{
			static constexpr size_t bufSize = 256;
			wchar_t funcInfo[bufSize] = { '\0' };
			swprintf(funcInfo, bufSize, L"notify(SCNotification *notification) : \r notification->nmhdr.code == %d\r notification->nmhdr.hwndFrom == %p\r notification->nmhdr.idFrom == %" PRIuPTR, \
				scNotif.nmhdr.code, scNotif.nmhdr.hwndFrom, scNotif.nmhdr.idFrom);
			pluginCrashAlert(_pluginInfos[indexPluginInfo]->_moduleName.c_str(), funcInfo);
		}
	}
}

// broadcast the notification to all plugins
void PluginsManager::notify(const SCNotification *notification)
{
	if (_noMoreNotification) // this boolean should be enabled after NPPN_SHUTDOWN has been sent
		return;
	_noMoreNotification = notification->nmhdr.code == NPPN_SHUTDOWN;

	for (size_t i = 0, len = _pluginInfos.size() ; i < len ; ++i)
	{
		notify(i, notification);
	}
}


void PluginsManager::relayNppMessages(UINT Message, WPARAM wParam, LPARAM lParam)
{
	for (size_t i = 0, len = _pluginInfos.size(); i < len ; ++i)
	{
        if (_pluginInfos[i]->_hLib)
		{
			try {
				_pluginInfos[i]->_pMessageProc(Message, wParam, lParam);
			}
			catch (std::exception& e)
			{
				pluginExceptionAlert(_pluginInfos[i]->_moduleName.c_str(), e);
			}
			catch (...)
			{
				static constexpr size_t bufSize = 128;
				wchar_t funcInfo[bufSize] = { '\0' };
				swprintf(funcInfo, bufSize, L"relayNppMessages(UINT Message : %u, WPARAM wParam : %" PRIuPTR ", LPARAM lParam : %" PRIiPTR ")", Message, wParam, lParam);
				pluginCrashAlert(_pluginInfos[i]->_moduleName.c_str(), funcInfo);
			}
		}
	}
}


bool PluginsManager::relayPluginMessages(UINT Message, WPARAM wParam, LPARAM lParam)
{
	const auto* moduleName = reinterpret_cast<wchar_t*>(wParam);
	if (!moduleName || !moduleName[0] || !lParam)
		return false;

	for (size_t i = 0, len = _pluginInfos.size() ; i < len ; ++i)
	{
        if (_pluginInfos[i]->_moduleName == moduleName)
		{
            if (_pluginInfos[i]->_hLib)
			{
				try {
					_pluginInfos[i]->_pMessageProc(Message, wParam, lParam);
				}
				catch (std::exception& e)
				{
					pluginExceptionAlert(_pluginInfos[i]->_moduleName.c_str(), e);
				}
				catch (...)
				{
					static constexpr size_t bufSize = 128;
					wchar_t funcInfo[bufSize] = { '\0' };
					swprintf(funcInfo, bufSize, L"relayPluginMessages(UINT Message : %u, WPARAM wParam : %" PRIuPTR ", LPARAM lParam : %" PRIiPTR ")", Message, wParam, lParam);
					pluginCrashAlert(_pluginInfos[i]->_moduleName.c_str(), funcInfo);
				}
				return true;
            }
		}
	}
	return false;
}


bool PluginsManager::allocateCmdID(int numberRequired, int *start)
{
	bool retVal = true;

	*start = _dynamicIDAlloc.allocate(numberRequired);

	if (*start == -1)
	{
		*start = 0;
		retVal = false;
	}
	return retVal;
}

bool PluginsManager::allocateMarker(int numberRequired, int* start)
{
	bool retVal = true;
	*start = _markerAlloc.allocate(numberRequired);
	if (*start == -1)
	{
		*start = 0;
		retVal = false;
	}
	return retVal;
}

bool PluginsManager::allocateIndicator(int numberRequired, int* start)
{
	bool retVal = false;
	int possibleStart = _indicatorAlloc.allocate(numberRequired);
	if (possibleStart != -1)
	{
		*start = possibleStart;
		retVal = true;
	}
	return retVal;
}

wstring PluginsManager::getLoadedPluginNames() const
{
	wstring pluginPaths;
	for (const auto &dll : _loadedDlls)
	{
		pluginPaths += L"\r\n    ";
		pluginPaths += dll._displayName;
	}
	return pluginPaths;
}
