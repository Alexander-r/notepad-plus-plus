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


#include "Notepad_plus_Window.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMainWindow>
#include <QGuiApplication>
#include <QTimer>
#include <QMenuBar>
#include <QMimeData>
#include <QResizeEvent>
#include <QUrl>

#include "NppResources.h"

HWND Notepad_plus_Window::gNppHWND = nullptr;

namespace
{
	class MainWidget : public QMainWindow
	{
	public:
		explicit MainWidget(QWidget* parent) : QMainWindow(parent)
		{
			setAcceptDrops(true);
		}

	protected:
		void closeEvent(QCloseEvent* event) override
		{
			// WM_CLOSE has already closed the application when it decides to go ahead
			event->ignore();
			::SendMessage(this, WM_CLOSE, 0, 0);
		}

		void resizeEvent(QResizeEvent* event) override
		{
			QMainWindow::resizeEvent(event);
			::SendMessage(this, WM_SIZE, 0, 0);
		}

		void moveEvent(QMoveEvent* event) override
		{
			QMainWindow::moveEvent(event);
			::SendMessage(this, WM_MOVE, 0, 0);
		}

		void changeEvent(QEvent* event) override
		{
			QMainWindow::changeEvent(event);

			// The desktop's theme changed. Later, as the style has to deal with this first.
			if (event->type() == QEvent::ThemeChange)
				QTimer::singleShot(0, this, [this]() { ::SendMessage(this, WM_SETTINGCHANGE, 0, 0); });

			if (event->type() == QEvent::ActivationChange)
			{
				::SendMessage(this, WM_ACTIVATE, isActiveWindow() ? WA_ACTIVE : WA_INACTIVE, 0);
			}
			else if (event->type() == QEvent::WindowStateChange && isMinimized())
			{
				::SendMessage(this, WM_SYSCOMMAND, SC_MINIMIZE, 0);
			}
		}

		void dragEnterEvent(QDragEnterEvent* event) override
		{
			if (event->mimeData()->hasUrls())
				event->acceptProposedAction();
		}

		void dropEvent(QDropEvent* event) override
		{
			std::vector<std::wstring> paths;
			const QList<QUrl> urls = event->mimeData()->urls();
			for (const QUrl& url : urls)
			{
				if (url.isLocalFile())
					paths.push_back(QDir::toNativeSeparators(url.toLocalFile()).toStdWString());
			}
			event->acceptProposedAction();
			::SendMessage(this, WM_DROPFILES, reinterpret_cast<WPARAM>(&paths), 0);
		}
	};
}

namespace
{
	// Alt and F10 show and hide the menu bar when the settings keep it hidden
	class HiddenMenuKeys : public QObject
	{
	public:
		explicit HiddenMenuKeys(QMainWindow* window) : QObject(window), _window(window)
		{
			// Choosing from the menu puts it away again
			QObject::connect(window->menuBar(), &QMenuBar::triggered, this, [this]()
			{
				if (!NppParameters::getInstance().getNppGUI()._menuBarShow)
					_window->menuBar()->hide();
			});
		}

	protected:
		bool eventFilter(QObject* watched, QEvent* event) override
		{
			const QEvent::Type type = event->type();
			if (type == QEvent::KeyPress)
			{
				const auto* key = static_cast<QKeyEvent*>(event);
				_isAltAlone = key->key() == Qt::Key_Alt && key->modifiers() == Qt::AltModifier;
				if (key->key() == Qt::Key_F10 && key->modifiers() == Qt::NoModifier && isHidingMenu() && !key->isAutoRepeat())
				{
					toggleMenu();
					return true;
				}
			}
			else if (type == QEvent::KeyRelease)
			{
				if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Alt && _isAltAlone && isHidingMenu())
					toggleMenu();
				_isAltAlone = false;
			}
			else if (type == QEvent::MouseButtonPress)
			{
				_isAltAlone = false;
			}
			return QObject::eventFilter(watched, event);
		}

	private:
		QMainWindow* _window = nullptr;
		bool _isAltAlone = false;

		bool isHidingMenu() const
		{
			return !NppParameters::getInstance().getNppGUI()._menuBarShow && QApplication::activeModalWidget() == nullptr;
		}

		void toggleMenu() const
		{
			QMenuBar* menuBar = _window->menuBar();
			menuBar->setVisible(menuBar->isHidden());
		}
	};
}

QMainWindow* Notepad_plus_Window::mainWindow() const
{
	return static_cast<QMainWindow*>(_hSelf);
}

void Notepad_plus_Window::destroy()
{
	delete _hSelf;
	_hSelf = nullptr;
}

void Notepad_plus_Window::init(HINSTANCE hInst, HWND parent, const wchar_t *cmdLine, CmdLineParams *cmdLineParams)
{
	Window::init(hInst, parent);

	_isPrelaunch = cmdLineParams->_isPreLaunch;

	NppParameters& nppParams = NppParameters::getInstance();
	NppGUI & nppGUI = nppParams.getNppGUI();

	if (cmdLineParams->_isNoPlugin)
		_notepad_plus_plus_core._pluginsManager.disable();

	nppGUI._isCmdlineNosessionActivated = cmdLineParams->_isNoSession;
	nppGUI._isFullReadOnly = cmdLineParams->_isFullReadOnly;
	nppGUI._isFullReadOnlySavingForbidden = cmdLineParams->_isFullReadOnlySavingForbidden;

	_hIconAbsent = nppLoadIcon(IDI_ICONABSENT);

	auto* window = new MainWidget(_hParent);
	_hSelf = window;

	// Coming back to the application is when it looks for files changed by others
	QObject::connect(qApp, &QGuiApplication::applicationStateChanged, window, [window](Qt::ApplicationState state)
	{
		if (state == Qt::ApplicationActive)
			::SendMessage(window, WM_ACTIVATEAPP, TRUE, 0);
	});
	window->setWindowTitle(QStringLiteral("Notepad++"));
	if (QIcon* appIcon = nppLoadIcon(IDI_M30ICON))
		window->setWindowIcon(*appIcon);
	if (_notepad_plus_plus_core._nativeLangSpeaker.isRTL())
		window->setLayoutDirection(Qt::RightToLeft);

	nppSetMainWindow(_hSelf);
	nppRegisterMessageTarget(_hSelf, [this](UINT message, WPARAM wParam, LPARAM lParam) -> LRESULT
	{
		return _notepad_plus_plus_core.process(_hSelf, message, wParam, lParam);
	});

	_notepad_plus_plus_core._pPublicInterface = this;
	_notepad_plus_plus_core.init(_hSelf);

	QObject::connect(QApplication::clipboard(), &QClipboard::dataChanged, window, [this]()
	{
		::SendMessage(_hSelf, WM_CLIPBOARDUPDATE, 0, 0);
	});

	if (!cmdLineParams->_pluginMessage.empty())
	{
		SCNotification scnN{};
		scnN.nmhdr.code = NPPN_CMDLINEPLUGINMSG;
		scnN.nmhdr.hwndFrom = _hSelf;
		scnN.nmhdr.idFrom = reinterpret_cast<uptr_t>(cmdLineParams->_pluginMessage.c_str());
		_notepad_plus_plus_core._pluginsManager.notify(&scnN);
	}

	_notepad_plus_plus_core.staticCheckMenuAndTB();

	gNppHWND = _hSelf;

	if (cmdLineParams->isPointValid())
		_hSelf->setGeometry(cmdLineParams->_point.x, cmdLineParams->_point.y, nppGUI._appPos.right, nppGUI._appPos.bottom);
	else
		_hSelf->setGeometry(nppGUI._appPos.left, nppGUI._appPos.top, nppGUI._appPos.right, nppGUI._appPos.bottom);

	if ((nppGUI._tabStatus & TAB_MULTILINE) != 0)
		::SendMessage(_hSelf, NPPM_INTERNAL_MULTILINETABBAR, 0, 0);

	if (!nppGUI._menuBarShow)
		mainWindow()->menuBar()->hide();
	qApp->installEventFilter(new HiddenMenuKeys(mainWindow()));

	if (cmdLineParams->_isNoTab || (nppGUI._tabStatus & TAB_HIDE))
	{
		const int tabStatusOld = nppGUI._tabStatus;
		::SendMessage(_hSelf, NPPM_HIDETABBAR, 0, TRUE);
		if (cmdLineParams->_isNoTab)
		{
			// Restore old settings when tab bar has been hidden from tab bar.
			if (!(tabStatusOld & TAB_HIDE))
				nppGUI._forceTabbarVisible = true;
		}
	}

	if (cmdLineParams->_monitoringMode)
		nppParams.setMonitoringModeOn();


	if (cmdLineParams->_alwaysOnTop)
		::SendMessage(_hSelf, WM_COMMAND, IDM_VIEW_ALWAYSONTOP, 0);

	std::chrono::steady_clock::duration sessionLoadingTime{};
	if (nppGUI._rememberLastSession && !nppGUI._isCmdlineNosessionActivated)
	{
		std::chrono::steady_clock::time_point sessionLoadingStartTP = std::chrono::steady_clock::now();
		_notepad_plus_plus_core.loadLastSession();
		sessionLoadingTime = std::chrono::steady_clock::now() - sessionLoadingStartTP;
	}

	if (nppParams.doFunctionListExport() || nppParams.doPrintAndExit())
	{
		_hSelf->hide();
	}
	else if (!cmdLineParams->_isPreLaunch)
	{
		if (cmdLineParams->isPointValid())
			_hSelf->show();
		else if (nppGUI._isMaximized)
			_hSelf->showMaximized();
		else
			_hSelf->show();
	}
	else
	{
		_notepad_plus_plus_core._pTrayIco = new trayIconControler(_hSelf, IDI_M30ICON, NPPM_INTERNAL_MINIMIZED_TRAY, nppLoadIcon(IDI_M30ICON), L"");
		_notepad_plus_plus_core._pTrayIco->doTrayIcon(ADD);
	}

	std::vector<std::wstring> fileNames;
	std::vector<std::wstring> patterns;
	patterns.push_back(L"*.xml");

	std::wstring nppDir = nppParams.getNppPath();

	LocalizationSwitcher & localizationSwitcher = nppParams.getLocalizationSwitcher();
	std::wstring localizationDir = nppDir;
	pathAppend(localizationDir, L"localization");
	localizationDir += nppPathSepStr;

	_notepad_plus_plus_core.getMatchedFileNames(localizationDir.c_str(), 0, patterns, fileNames, false, false);
	for (const auto& fileName : fileNames)
		localizationSwitcher.addLanguageFromXml(fileName);

	fileNames.clear();
	ThemeSwitcher & themeSwitcher = nppParams.getThemeSwitcher();

	//  Get themes from both npp install themes dir and user data themes dir (AppData, settingsDir, or cloud) with the per user
	//  overriding default themes of the same name.

	std::wstring userDataThemeDir = nppParams.getUserPath(); // getUserPath will always pick the right settingsDir/Cloud/AppData location
	if (!userDataThemeDir.empty() && userDataThemeDir != nppDir)	
	{
		// append files from userDataThemeDir, unless it matches nppDir (which means the themes are already in the internal structure)
		pathAppend(userDataThemeDir, L"themes");
		userDataThemeDir += nppPathSepStr;
		_notepad_plus_plus_core.getMatchedFileNames(userDataThemeDir.c_str(), 0, patterns, fileNames, false, false);
		for (const auto& fileName: fileNames)
		{
			themeSwitcher.addThemeFromXml(fileName);
		}
	}

	fileNames.clear();

	std::wstring nppThemeDir = nppDir; // <- should use the pointer to avoid the constructor of copy
	pathAppend(nppThemeDir, L"themes");
	nppThemeDir += nppPathSepStr;

	// Set theme directory to their installation directory
	themeSwitcher.setThemeDirPath(nppThemeDir);

	_notepad_plus_plus_core.getMatchedFileNames(nppThemeDir.c_str(), 0, patterns, fileNames, false, false);
	for (const auto& fileName : fileNames)
	{
		std::wstring themeName( themeSwitcher.getThemeFromXmlFileName(fileName.c_str()) );
		if (!themeSwitcher.themeNameExists(themeName.c_str()))
		{
			themeSwitcher.addThemeFromXml(fileName);
			
			if (!userDataThemeDir.empty() && userDataThemeDir != nppDir)
			{
				std::wstring userDataThemePath = userDataThemeDir;

				if (!doesDirectoryExist(userDataThemePath.c_str()))
				{
					QDir().mkpath(QString::fromStdWString(userDataThemePath));
				}

				const wchar_t* fn = PathFindFileName(fileName.c_str());
				pathAppend(userDataThemePath, fn);
				themeSwitcher.addThemeStylerSavePath(fileName, userDataThemePath);
			}
		}
	}

	if (NppDarkMode::isWindowsModeEnabled())
	{
		std::wstring themePath;
		std::wstring xmlFileName = NppDarkMode::getThemeName();
		if (!xmlFileName.empty())
		{
			if (!nppParams.isLocal() || nppParams.isCloud())
			{
				themePath = nppParams.getUserPath();
				pathAppend(themePath, L"themes");
				pathAppend(themePath, xmlFileName);
			}

			if (themePath.empty() || !doesFileExist(themePath.c_str()))
			{
				themePath = themeSwitcher.getThemeDirPath();
				pathAppend(themePath, xmlFileName);
			}
		}
		else
		{
			const auto& themeInfo = themeSwitcher.getElementFromIndex(0);
			themePath = themeInfo.second;
		}

		if (doesFileExist(themePath.c_str()))
		{
			nppGUI._themeName.assign(themePath);
			nppParams.reloadStylers(themePath.c_str());
			::SendMessage(_hSelf, WM_UPDATESCINTILLAS, TRUE, 0);
		}
	}

	// Restore all dockable panels from the last session
	for (size_t i = 0, len = _notepad_plus_plus_core._internalFuncIDs.size(); i < len; ++i)
	{
		int id = _notepad_plus_plus_core._internalFuncIDs[i];
		if (_notepad_plus_plus_core.isInternalPanel(id))
		{
			::SendMessage(_hSelf, WM_COMMAND, id, 0);
		}
	}

	std::chrono::steady_clock::duration cmdlineParamsLoadingTime{};
	std::vector<std::wstring> fns;
	if (cmdLine)
	{
		std::chrono::steady_clock::time_point cmdlineParamsLoadingStartTP = std::chrono::steady_clock::now();
		fns = _notepad_plus_plus_core.loadCommandlineParams(cmdLine, cmdLineParams);
		cmdlineParamsLoadingTime = std::chrono::steady_clock::now() - cmdlineParamsLoadingStartTP;
	}

	// Launch folder as workspace after all this dockable panel being restored from the last session
	// To avoid dockable panel toggle problem.
	if (cmdLineParams->_openFoldersAsWorkspace)
	{
		std::wstring emptyStr;
		_notepad_plus_plus_core.launchFileBrowser(fns, emptyStr, true);
	}
	::SendMessage(_hSelf, WM_ACTIVATE, WA_ACTIVE, 0);

	::SendMessage(_hSelf, NPPM_INTERNAL_CRLFFORMCHANGED, 0, 0);

	::SendMessage(_hSelf, NPPM_INTERNAL_NPCFORMCHANGED, 0, 0);

	::SendMessage(_hSelf, NPPM_INTERNAL_ENABLECHANGEHISTORY, 0, 0);

	::SendMessage(_hSelf, NPPM_INTERNAL_LINECUTCOPYWITHOUTSELECTION, 0, 0);

	::SendMessage(_hSelf, NPPM_INTERNAL_DISABLESELECTEDTEXTDRAGDROP, 0, 0);

	if (nppGUI._newDocDefaultSettings._addNewDocumentOnStartup && nppGUI._rememberLastSession)
	{
		::SendMessage(_hSelf, WM_COMMAND, IDM_FILE_NEW, 0);
	}

	// Notify plugins that Notepad++ is ready
	SCNotification scnN{};
	scnN.nmhdr.code = NPPN_READY;
	scnN.nmhdr.hwndFrom = _hSelf;
	scnN.nmhdr.idFrom = 0;
	_notepad_plus_plus_core._pluginsManager.notify(&scnN);

	if (!cmdLineParams->_easterEggName.empty())
	{
		if (cmdLineParams->_quoteType == 0) // Easter Egg Name
		{
			int iQuote = _notepad_plus_plus_core.getQuoteIndexFrom(cmdLineParams->_easterEggName.c_str());
			if (iQuote != -1)
			{
				_notepad_plus_plus_core.showQuoteFromIndex(iQuote);
			}
		}
		else if (cmdLineParams->_quoteType == 1) // command line quote
		{
			_userQuote = cmdLineParams->_easterEggName;
			_quoteParams.reset();
			_quoteParams._quote = _userQuote.c_str();
			_quoteParams._quoter = L"Anonymous #999";
			_quoteParams._shouldBeTrolling = false;
			_quoteParams._lang = cmdLineParams->_langType;
			if (cmdLineParams->_ghostTypingSpeed == 1)
				_quoteParams._speed = QuoteParams::slow;
			else if (cmdLineParams->_ghostTypingSpeed == 2)
				_quoteParams._speed = QuoteParams::rapid;
			else if (cmdLineParams->_ghostTypingSpeed == 3)
				_quoteParams._speed = QuoteParams::speedOfLight;

			_notepad_plus_plus_core.showQuote(&_quoteParams);
		}
		else if (cmdLineParams->_quoteType == 2) // content from file
		{
			if (doesFileExist(cmdLineParams->_easterEggName.c_str()))
			{
				bool bLoadingFailed = false;
				std::string content = getFileContent(cmdLineParams->_easterEggName.c_str(), &bLoadingFailed);
				if (!bLoadingFailed)
				{
					WcharMbcsConvertor& wmc = WcharMbcsConvertor::getInstance();
					_userQuote = wmc.char2wchar(content.c_str(), SC_CP_UTF8);
					if (!_userQuote.empty())
					{
						_quoteParams.reset();
						_quoteParams._quote = _userQuote.c_str();
						_quoteParams._quoter = L"Anonymous #999";
						_quoteParams._shouldBeTrolling = false;
						_quoteParams._lang = cmdLineParams->_langType;
						if (cmdLineParams->_ghostTypingSpeed == 1)
							_quoteParams._speed = QuoteParams::slow;
						else if (cmdLineParams->_ghostTypingSpeed == 2)
							_quoteParams._speed = QuoteParams::rapid;
						else if (cmdLineParams->_ghostTypingSpeed == 3)
							_quoteParams._speed = QuoteParams::speedOfLight;

						_notepad_plus_plus_core.showQuote(&_quoteParams);
					}
				}
			}
		}
	}

	if (cmdLineParams->_showLoadingTime)
	{
		std::chrono::steady_clock::duration nppInitTime = (std::chrono::steady_clock::now() - g_nppStartTimePoint) - g_pluginsLoadingTime - sessionLoadingTime - cmdlineParamsLoadingTime;
		std::wstringstream wss;
		wss << L"Notepad++ initialization: " << std::chrono::hh_mm_ss{ std::chrono::duration_cast<std::chrono::milliseconds>(nppInitTime) } << std::endl;
		wss << L"Plugins loading: " << std::chrono::hh_mm_ss{ std::chrono::duration_cast<std::chrono::milliseconds>(g_pluginsLoadingTime) } << std::endl;
		wss << L"Last session loading: " << std::chrono::hh_mm_ss{ std::chrono::duration_cast<std::chrono::milliseconds>(sessionLoadingTime) } << std::endl;
		wss << L"Command line params handling: " << std::chrono::hh_mm_ss{ std::chrono::duration_cast<std::chrono::milliseconds>(cmdlineParamsLoadingTime) } << std::endl;
		wss << L"Total loading time: " << std::chrono::hh_mm_ss{ std::chrono::duration_cast<std::chrono::milliseconds>(nppInitTime + g_pluginsLoadingTime + sessionLoadingTime + cmdlineParamsLoadingTime) };
		NppDarkMode::darkMessageBoxW(nullptr, wss.str().c_str(), L"Notepad++ loading time (hh:mm:ss.ms)", MB_OK);
	}

	if (cmdLineParams->_displayCmdLineArgs)
	{
		_notepad_plus_plus_core.command(IDM_CMDLINEARGUMENTS);
	}

	bool isSnapshotMode = nppGUI.isSnapshotMode();
	if (isSnapshotMode)
	{
		_notepad_plus_plus_core.checkModifiedDocument(false);
		// Launch backup task
		_notepad_plus_plus_core.launchDocumentBackupTask();
	}

	// Make this call later to take effect
	::SendMessage(_hSelf, NPPM_INTERNAL_SETWORDCHARS, 0, 0);
	::SendMessage(_hSelf, NPPM_INTERNAL_SETNPC, 0, 0);

	if (nppParams.doFunctionListExport())
		::SendMessage(_hSelf, NPPM_INTERNAL_EXPORTFUNCLISTANDQUIT, 0, 0);

	if (nppParams.doPrintAndExit())
		::SendMessage(_hSelf, NPPM_INTERNAL_PRNTANDQUIT, 0, 0);
}
