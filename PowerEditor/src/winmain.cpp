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


#include <QApplication>
#include <QDir>
#include <QEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QThread>

#include <chrono>
#include <cstdlib>
#include <cwchar>
#include <exception>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "NppMessageFilter.h"
#include "Common.h"
#include "FileInterface.h"
#include "Notepad_plus_Window.h"
#include "Notepad_plus_msgs.h"
#include "NppConstants.h"
#include "NppDarkMode.h"
#include "NppInstanceLink.h"
#include "Parameters.h"
#include "resource.h"

typedef std::vector<std::wstring> ParamVector;


namespace
{

// Converts /p or /P to -quickPrint if it exists as the first parameter
// This seems to mirror Notepad's behaviour
void convertParamsToNotepadStyle(ParamVector& params)
{
	for (auto it = params.begin(); it != params.end(); ++it)
	{
		if (lstrcmp(it->c_str(), L"/p") == 0 || lstrcmp(it->c_str(), L"/P") == 0)
		{
			it->assign(L"-quickPrint");
		}
	}
}

bool isInList(const wchar_t *token2Find, ParamVector& params, bool eraseArg = true)
{
	for (auto it = params.begin(); it != params.end(); ++it)
	{
		if (lstrcmp(token2Find, it->c_str()) == 0)
		{
			if (eraseArg) params.erase(it);
			return true;
		}
	}
	return false;
}

bool getParamVal(wchar_t c, ParamVector & params, std::wstring & value)
{
	value = L"";
	size_t nbItems = params.size();

	for (size_t i = 0; i < nbItems; ++i)
	{
		const wchar_t * token = params.at(i).c_str();
		if (token[0] == '-' && lstrlen(token) >= 2 && token[1] == c) //dash, and enough chars
		{
			value = (token+2);
			if (value.empty())
				return false;

			params.erase(params.begin() + i);
			return true;
		}
	}
	return false;
}

bool getParamValFromString(const wchar_t *str, ParamVector & params, std::wstring & value)
{
	value = L"";
	size_t nbItems = params.size();

	for (size_t i = 0; i < nbItems; ++i)
	{
		const wchar_t * token = params.at(i).c_str();
		std::wstring tokenStr = token;
		size_t pos = tokenStr.find(str);
		if (pos != std::wstring::npos && pos == 0)
		{
			value = (token + lstrlen(str));
			if (value.empty())
				return false;

			params.erase(params.begin() + i);
			return true;
		}
	}
	return false;
}

LangType getLangTypeFromParam(ParamVector & params)
{
	std::wstring langStr;
	if (!getParamVal('l', params, langStr))
		return L_EXTERNAL;
	return NppParameters::getLangIDFromStr(langStr.c_str());
}

std::wstring getLocalizationPathFromParam(ParamVector & params)
{
	std::wstring locStr;
	if (!getParamVal('L', params, locStr))
		return L"";
	locStr = stringToLower(stringReplace(locStr, L"_", L"-")); // convert to lowercase format with "-" as separator
	return NppParameters::getLocPathFromStr(locStr);
}

intptr_t getNumberFromParam(char paramName, ParamVector & params, bool & isParamePresent)
{
	std::wstring numStr;
	if (!getParamVal(paramName, params, numStr))
	{
		isParamePresent = false;
		return -1;
	}
	isParamePresent = true;

	long long res = -1;
	try {
		res = std::stoll(numStr);
	}
	catch (const std::invalid_argument&) // not a number
	{
		return -1;
	}
	catch (const std::out_of_range&) // number is too large
	{
		return -1;
	}
	catch (...)
	{
		return -1;
	}

	return static_cast<intptr_t>(res);
}

std::wstring getEasterEggNameFromParam(ParamVector & params, unsigned char & type)
{
	std::wstring EasterEggName;
	if (!getParamValFromString(L"-qn=", params, EasterEggName))  // get internal easter egg
	{
		if (!getParamValFromString(L"-qt=", params, EasterEggName)) // get user quote from cmdline argument
		{
			if (!getParamValFromString(L"-qf=", params, EasterEggName)) // get user quote from a content of file
				return L"";
			else
			{
				type = 2; // quote content in file
			}
		}
		else
			type = 1; // commandline quote
	}
	else
		type = 0; // easter egg

	if (EasterEggName.c_str()[0] == '"' && EasterEggName.c_str()[EasterEggName.length() - 1] == '"')
	{
		EasterEggName = EasterEggName.substr(1, EasterEggName.length() - 2);
	}

	if (type == 2)
		EasterEggName = relativeFilePathToFullFilePath(EasterEggName.c_str());

	return EasterEggName;
}

int getGhostTypingSpeedFromParam(ParamVector & params)
{
	std::wstring speedStr;
	if (!getParamValFromString(L"-qSpeed", params, speedStr))
		return -1;
	
	int speed = -1;
	try {
		speed = std::stoi(speedStr);
	}
	catch (const std::exception&) // not a number, or too large
	{
		return -1;
	}

	if (speed <= 0 || speed > 3)
		return -1;

	return speed;
}

const wchar_t FLAG_MULTI_INSTANCE[] = L"-multiInst";
const wchar_t FLAG_NO_PLUGIN[] = L"-noPlugin";
const wchar_t FLAG_READONLY[] = L"-ro"; // for current cmdline file(s) only
const wchar_t FLAG_FULL_READONLY[] = L"-fullReadOnly"; // user still can manually toggle OFF the R/O-state of N++ tabs, so saving of the tab filebuffers is possible
const wchar_t FLAG_FULL_READONLY_SAVING_FORBIDDEN[] = L"-fullReadOnlySavingForbidden"; // user cannot toggle R/O-state of N++ tabs, impossible to save opened tab filebuffers
const wchar_t FLAG_NOSESSION[] = L"-nosession";
const wchar_t FLAG_NOTABBAR[] = L"-notabbar";
const wchar_t FLAG_SYSTRAY[] = L"-systemtray";
const wchar_t FLAG_LOADINGTIME[] = L"-loadingTime";
const wchar_t FLAG_HELP[] = L"--help";
const wchar_t FLAG_ALWAYS_ON_TOP[] = L"-alwaysOnTop";
const wchar_t FLAG_OPENSESSIONFILE[] = L"-openSession";
const wchar_t FLAG_RECURSIVE[] = L"-r";
const wchar_t FLAG_FUNCLSTEXPORT[] = L"-export=functionList";
const wchar_t FLAG_PRINTANDQUIT[] = L"-quickPrint";
const wchar_t FLAG_NOTEPAD_COMPATIBILITY[] = L"-notepadStyleCmdline";
const wchar_t FLAG_OPEN_FOLDERS_AS_WORKSPACE[] = L"-openFoldersAsWorkspace";
const wchar_t FLAG_SETTINGS_DIR[] = L"-settingsDir=";
const wchar_t FLAG_TITLEBAR_ADD[] = L"-titleAdd=";
const wchar_t FLAG_APPLY_UDL[] = L"-udl=";
const wchar_t FLAG_PLUGIN_MESSAGE[] = L"-pluginMessage=";
const wchar_t FLAG_MONITOR_FILES[] = L"-monitor";
const wchar_t FLAG_MONITORING_MODE[] = L"-monitoringMode";

void doException(Notepad_plus_Window & notepad_plus_plus)
{
	QMessageBox::critical(Notepad_plus_Window::gNppHWND, QStringLiteral("Recovery initiating"), QStringLiteral("Notepad++ will attempt to save any unsaved data. However, data loss is very likely."));

	std::wstring emergencySavedDir = QDir::tempPath().toStdWString();
	pathAppend(emergencySavedDir, L"Notepad++ RECOV");

	bool res = notepad_plus_plus.emergency(emergencySavedDir);
	if (res)
	{
		std::wstring displayText = L"Notepad++ was able to successfully recover some unsaved documents, or nothing to be saved could be found.\nYou can find the results at :\n";
		displayText += emergencySavedDir;
		QMessageBox::information(Notepad_plus_Window::gNppHWND, QStringLiteral("Recovery success"), QString::fromStdWString(displayText));
	}
	else
		QMessageBox::critical(Notepad_plus_Window::gNppHWND, QStringLiteral("Recovery failure"), QStringLiteral("Unfortunately, Notepad++ was not able to save your work. We are sorry for any lost data."));
}

// Looks for -z arguments and strips command line arguments following those, if any
void stripIgnoredParams(ParamVector & params)
{
	for (auto it = params.begin(); it != params.end(); )
	{
		if (lstrcmp(it->c_str(), L"-z") == 0)
		{
			auto nextIt = std::next(it);
			if ( nextIt != params.end() )
			{
				params.erase(nextIt);
			}
			it = params.erase(it);
		}
		else
		{
			++it;
		}
	}
}

// The mouse's back and forward buttons switch documents, whichever widget is under the pointer
class MouseNavigationFilter : public QObject
{
public:
	bool eventFilter(QObject* watched, QEvent* event) override
	{
		if (event->type() == QEvent::MouseButtonPress && Notepad_plus_Window::gNppHWND)
		{
			const auto* mouseEvent = static_cast<QMouseEvent*>(event);
			if (mouseEvent->button() == Qt::BackButton)
				::SendMessage(Notepad_plus_Window::gNppHWND, WM_APPCOMMAND, APPCOMMAND_BROWSER_BACKWARD, 0);
			else if (mouseEvent->button() == Qt::ForwardButton)
				::SendMessage(Notepad_plus_Window::gNppHWND, WM_APPCOMMAND, APPCOMMAND_BROWSER_FORWARD, 0);
		}
		return QObject::eventFilter(watched, event);
	}
};

} // namespace


std::chrono::steady_clock::time_point g_nppStartTimePoint{};

int main(int argc, char* argv[])
{
	g_nppStartTimePoint = std::chrono::steady_clock::now();

	nppInstallMessageFilter();
	QApplication application(argc, argv);
	QCoreApplication::setApplicationName(QStringLiteral("Notepad++"));
	QGuiApplication::setDesktopFileName(QStringLiteral("notepad-plus-plus"));

	const bool TheFirstOne = !nppIsInstanceRunning();

	// Qt has taken its own options out of the list already
	ParamVector params;
	std::wstring cmdLineString;
	const QStringList arguments = QCoreApplication::arguments();
	for (qsizetype i = 1; i < arguments.size(); ++i)
	{
		const std::wstring argument = arguments[i].toStdWString();
		params.push_back(argument);

		if (!cmdLineString.empty())
			cmdLineString += L' ';
		const bool needsQuotes = argument.find(L' ') != std::wstring::npos;
		cmdLineString += needsQuotes ? L"\"" + argument + L"\"" : argument;
	}


	// Convert commandline to notepad-compatible format, if applicable
	// For treating "-notepadStyleCmdline" "/P" and "-z"
	stripIgnoredParams(params);
	if ( isInList(FLAG_NOTEPAD_COMPATIBILITY, params) )
	{
		convertParamsToNotepadStyle(params);
	}

	bool isParamePresent;
	bool isMultiInst = isInList(FLAG_MULTI_INSTANCE, params);
	bool doFunctionListExport = isInList(FLAG_FUNCLSTEXPORT, params);
	bool doPrintAndQuit = isInList(FLAG_PRINTANDQUIT, params);

	CmdLineParams cmdLineParams;
	cmdLineParams._displayCmdLineArgs = isInList(FLAG_HELP, params);
	cmdLineParams._isNoTab = isInList(FLAG_NOTABBAR, params);
	cmdLineParams._isNoPlugin = isInList(FLAG_NO_PLUGIN, params);
	cmdLineParams._isReadOnly = isInList(FLAG_READONLY, params);
	cmdLineParams._isFullReadOnly = isInList(FLAG_FULL_READONLY, params);
	cmdLineParams._isFullReadOnlySavingForbidden = isInList(FLAG_FULL_READONLY_SAVING_FORBIDDEN, params);
	cmdLineParams._isNoSession = isInList(FLAG_NOSESSION, params);
	cmdLineParams._isPreLaunch = isInList(FLAG_SYSTRAY, params);
	cmdLineParams._alwaysOnTop = isInList(FLAG_ALWAYS_ON_TOP, params);
	cmdLineParams._showLoadingTime = isInList(FLAG_LOADINGTIME, params);
	cmdLineParams._isSessionFile = isInList(FLAG_OPENSESSIONFILE, params);
	cmdLineParams._isRecursive = isInList(FLAG_RECURSIVE, params);
	cmdLineParams._openFoldersAsWorkspace = isInList(FLAG_OPEN_FOLDERS_AS_WORKSPACE, params);
	cmdLineParams._monitorFiles = isInList(FLAG_MONITOR_FILES, params);
	cmdLineParams._monitoringMode = isInList(FLAG_MONITORING_MODE, params);

	cmdLineParams._langType = getLangTypeFromParam(params);
	cmdLineParams._localizationPath = getLocalizationPathFromParam(params);
	cmdLineParams._easterEggName = getEasterEggNameFromParam(params, cmdLineParams._quoteType);
	cmdLineParams._ghostTypingSpeed = getGhostTypingSpeedFromParam(params);

	std::wstring pluginMessage;
	if (getParamValFromString(FLAG_PLUGIN_MESSAGE, params, pluginMessage))
	{
		if (pluginMessage.length() >= 2 && (pluginMessage.front() == '"' && pluginMessage.back() == '"'))
		{
			pluginMessage = pluginMessage.substr(1, pluginMessage.length() - 2);
		}
		cmdLineParams._pluginMessage = pluginMessage;
	}

	// getNumberFromParam should be run at the end, to not consuming the other params
	cmdLineParams._line2go = getNumberFromParam('n', params, isParamePresent);
    cmdLineParams._column2go = getNumberFromParam('c', params, isParamePresent);
    cmdLineParams._pos2go = getNumberFromParam('p', params, isParamePresent);
	cmdLineParams._point.x = static_cast<LONG>(getNumberFromParam('x', params, cmdLineParams._isPointXValid));
	cmdLineParams._point.y = static_cast<LONG>(getNumberFromParam('y', params, cmdLineParams._isPointYValid));

	NppParameters& nppParameters = NppParameters::getInstance();

	nppParameters.setCmdLineString(cmdLineString);

	std::wstring path;
	if (getParamValFromString(FLAG_SETTINGS_DIR, params, path))
	{
		// path could contain double quotes if path contains white space
		if (path.c_str()[0] == '"' && path.c_str()[path.length() - 1] == '"')
		{
			path = path.substr(1, path.length() - 2);
		}
		nppParameters.setCmdSettingsDir(path);
	}

	std::wstring titleBarAdditional;
	if (getParamValFromString(FLAG_TITLEBAR_ADD, params, titleBarAdditional))
	{
		if (titleBarAdditional.length() >= 2)
		{
			if (titleBarAdditional.front() == '"' && titleBarAdditional.back() == '"')
			{
				titleBarAdditional = titleBarAdditional.substr(1, titleBarAdditional.length() - 2);
			}
		}
		nppParameters.setTitleBarAdd(titleBarAdditional);
	}

	std::wstring udlName;
	if (getParamValFromString(FLAG_APPLY_UDL, params, udlName))
	{
		if (udlName.length() >= 2)
		{
			if (udlName.front() == '"' && udlName.back() == '"')
			{
				udlName = udlName.substr(1, udlName.length() - 2);
			}
		}
		cmdLineParams._udlName = udlName;
	}

	if (cmdLineParams._localizationPath != L"")
	{
		// setStartWithLocFileName() should be called before parameters are loaded
		nppParameters.setStartWithLocFileName(cmdLineParams._localizationPath);
	}

	nppParameters.load();

	NppDarkMode::initDarkMode();

	if (doFunctionListExport || doPrintAndQuit) // export functionlist feature will serialize functionlist on the disk, then exit Notepad++. So it's important to not launch into existing instance, and keep the current instance.
	{
		isMultiInst = true;
		cmdLineParams._isNoSession = true;
	}

	nppParameters.setFunctionListExportBoolean(doFunctionListExport);
	nppParameters.setPrintAndExitBoolean(doPrintAndQuit);

	// override the settings if notepad style is present
	if (nppParameters.asNotepadStyle())
	{
		isMultiInst = true;
		cmdLineParams._isNoTab = true;
		cmdLineParams._isNoSession = true;
	}

	// override the settings if multiInst is chosen by user in the preference dialog
	const NppGUI & nppGUI = nppParameters.getNppGUI();
	if (nppGUI._multiInstSetting == multiInst)
	{
		isMultiInst = true;
		// Only the first launch remembers the session
		if (!TheFirstOne)
			cmdLineParams._isNoSession = true;
	}

	std::wstring quotFileNames = L"";
    // tell the running instance the FULL path to the new files to load
	size_t nbFilesToOpen = params.size();

	for (size_t i = 0; i < nbFilesToOpen; ++i)
	{
		const wchar_t * currentFile = params.at(i).c_str();
		if (currentFile[0])
		{
			//check if relative or full path. Relative paths don't have a colon for driveletter

			quotFileNames += L"\"";
			quotFileNames += relativeFilePathToFullFilePath(currentFile);
			quotFileNames += L"\" ";
		}
	}

	//Only after loading all the file paths set the working directory
	QDir::setCurrent(QString::fromStdWString(NppParameters::getInstance().getNppPath()));	//force working directory to path of module, preventing lock

	if ((!isMultiInst) && (!TheFirstOne))
	{
		std::vector<NppInstanceMessage> messages;

		if (params.size() > 0                         // if there are files to open, hand them over
			|| !cmdLineParams._pluginMessage.empty()) // or pluginMessage is present, hand it over as well
		{
			const CmdLineParamsDTO dto = CmdLineParamsDTO::FromCmdLineParams(cmdLineParams);
			const auto wideBytes = [](const std::wstring& text)
			{
				return QByteArray(reinterpret_cast<const char*>(text.c_str()), static_cast<qsizetype>((text.length() + 1) * sizeof(wchar_t)));
			};

			messages.push_back({ COPYDATA_PARAMS, QByteArray(reinterpret_cast<const char*>(&dto), sizeof(dto)) });
			messages.push_back({ COPYDATA_FULL_CMDLINE, wideBytes(cmdLineString) });
			messages.push_back({ COPYDATA_FILENAMESW, wideBytes(quotFileNames) });
		}

		if (nppSendToRunningInstance(messages))
		{
			nppParameters.destroyInstance();
			return 0;
		}
	}

	NppInstanceServer instanceServer;
	if (TheFirstOne)
		instanceServer.listen();

	MouseNavigationFilter mouseNavigationFilter;
	application.installEventFilter(&mouseNavigationFilter);

	auto upNotepadWindow = std::make_unique<Notepad_plus_Window>();
	Notepad_plus_Window & notepad_plus_plus = *upNotepadWindow.get();

	int exitCode = 0;
	try {
		notepad_plus_plus.init(nullptr, nullptr, quotFileNames.c_str(), &cmdLineParams);
		instanceServer.setMainWindow(notepad_plus_plus.getHSelf());
		exitCode = QApplication::exec();
	}
	catch (int i)
	{
		QMessageBox::critical(Notepad_plus_Window::gNppHWND, QStringLiteral("Int Exception"), QStringLiteral("God Damned Exception:%1").arg(i));
		doException(notepad_plus_plus);
	}
	catch (std::runtime_error & ex)
	{
		QMessageBox::critical(Notepad_plus_Window::gNppHWND, QStringLiteral("Runtime Exception"), QString::fromUtf8(ex.what()));
		doException(notepad_plus_plus);
	}
	catch (std::exception & ex)
	{
		QMessageBox::critical(Notepad_plus_Window::gNppHWND, QStringLiteral("General Exception"), QString::fromUtf8(ex.what()));
		doException(notepad_plus_plus);
	}
	catch (...) // this shouldn't ever have to happen
	{
		QMessageBox::critical(Notepad_plus_Window::gNppHWND, QStringLiteral("Unknown Exception"), QStringLiteral("An exception that we did not yet found its name is just caught"));
		doException(notepad_plus_plus);
	}

	return exitCode;
}
