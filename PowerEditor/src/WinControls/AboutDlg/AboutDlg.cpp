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


#include "AboutDlg.h"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QDialog>
#include <QLayout>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScreen>
#include <QStandardPaths>
#include <QSysInfo>

#include <string>

#include <pugixml.hpp>

#include "Common.h"
#include "NppConstants.h"
#include "NppControls.h"
#include "NppDarkMode.h"
#include "NppResources.h"
#include "Parameters.h"
#include "localization.h"
#include "resource.h"
#include "ui_AboutboxDlg.h"
#include "ui_CommandlineargsboxDlg.h"
#include "ui_DebuginfoboxDlg.h"
#include "ui_DosaveallboxDlg.h"
#include "ui_DosaveornotboxDlg.h"

using namespace std;

void AboutDlg::doDialog()
{
	if (!isCreated())
	{
		QDialog* dialog = createDialogWidget();
		Ui::AboutboxDlg ui;
		ui.setupUi(dialog);

		ui.label->setText(QString::fromWCharArray(NOTEPAD_PLUS_VERSION));
		ui.buildDatetime->setText(QStringLiteral("Build time: " __DATE__ " - " __TIME__));
		ui.buildDatetime->setEnabled(false);
		ui.versionBit->setText((sizeof(void*) == 4) ? QStringLiteral("(32-bit)") : QStringLiteral("(64-bit)"));
		ui.licenceEdit->setPlainText(QString::fromWCharArray(LICENCE_TXT));

		if (const QIcon* icon = nppLoadIcon(NppDarkMode::usesDarkWidgets() ? IDI_CHAMELEON_DM : IDI_CHAMELEON))
			ui.iNppaboutLogo->setPixmap(icon->pixmap(80, 80));

		_pageLink.init(_hInst, _hSelf);
		_pageLink.create(ui.homeAddr, L"https://notepad-plus-plus.org/");

		QObject::connect(ui.ok, &QAbstractButton::clicked, dialog, [this]() { display(false); });
	}

	goToCenter();
	display();
}

// adjustSize alone stays within a part of the screen, which can leave wrapped text too little height
static void fitToWrappedText(QWidget* dialog)
{
	dialog->adjustSize();
	dialog->resize(dialog->width(), dialog->layout()->totalHeightForWidth(dialog->width()));
}

void DebugInfoDlg::buildDebugInfo()
{
	NppParameters& nppParam = NppParameters::getInstance();
	NppGUI& nppGui = nppParam.getNppGUI();

	// Notepad++ version
	_debugInfoStr = NOTEPAD_PLUS_VERSION;
	_debugInfoStr += (sizeof(void*) == 4) ? L"   (32-bit)" : L"   (64-bit)";
	_debugInfoStr += L"\r\n";

	// Build time
	_debugInfoStr += L"Build time: ";
	wstring buildTime;
	WcharMbcsConvertor& wmc = WcharMbcsConvertor::getInstance();
	buildTime += wmc.char2wchar(__DATE__, CP_ACP);
	buildTime += L" - ";
	buildTime += wmc.char2wchar(__TIME__, CP_ACP);
	_debugInfoStr += buildTime;
	_debugInfoStr += L"\r\n";

#if defined(__clang__)
	_debugInfoStr += L"Built with: Clang ";
	_debugInfoStr += wmc.char2wchar(__clang_version__, CP_ACP);
	_debugInfoStr += L"\r\n";
#elif defined(__GNUC__)
	_debugInfoStr += L"Built with: GCC ";
	_debugInfoStr += wmc.char2wchar(__VERSION__, CP_ACP);
	_debugInfoStr += L"\r\n";
#elif !defined(_MSC_VER)
	_debugInfoStr += L"Built with: (unknown)\r\n";
#endif

	// Scintilla/Lexilla version
	_debugInfoStr += L"Scintilla/Lexilla included: ";
	{
		string strSciLexVer = NPP_SCINTILLA_VERSION;
		strSciLexVer += "/";
		strSciLexVer += NPP_LEXILLA_VERSION;
		_debugInfoStr += wmc.char2wchar(strSciLexVer.c_str(), CP_ACP);
	}
	_debugInfoStr += L"\r\n";

	// Boost Regex version
	_debugInfoStr += L"Boost Regex included: ";
	_debugInfoStr += wmc.char2wchar(NPP_BOOST_REGEX_VERSION, CP_ACP);
	_debugInfoStr += L"\r\n";

#if defined(PUGIXML_VERSION)
	// pugixml version
	_debugInfoStr += L"pugixml included: ";
	_debugInfoStr += std::to_wstring(PUGIXML_VERSION / 1000) + L"." + std::to_wstring((PUGIXML_VERSION % 1000) / 10);
	_debugInfoStr += L"\r\n";
#endif

	// Binary path
	_debugInfoStr += L"Path: ";
	_debugInfoStr += QCoreApplication::applicationFilePath().toStdWString();
	_debugInfoStr += L"\r\n";

	// Command line as specified for program launch
	// The _cmdLinePlaceHolder will be replaced later by refreshDebugInfo()
	_debugInfoStr += L"Command Line: ";
	_debugInfoStr += _cmdLinePlaceHolder;
	_debugInfoStr += L"\r\n";

	// Administrator mode
	_debugInfoStr += L"Admin mode: ";
	_debugInfoStr += _isAdmin ? L"ON" : L"OFF";
	_debugInfoStr += L"\r\n";

	// local conf
	_debugInfoStr += L"Local Conf mode: ";
	bool doLocalConf = (NppParameters::getInstance()).isLocal();
	_debugInfoStr += doLocalConf ? L"ON" : L"OFF";
	_debugInfoStr += L"\r\n";

	// Cloud config directory
	_debugInfoStr += L"Cloud Config: ";
	const wstring& cloudPath = nppParam.getNppGUI()._cloudPath;
	_debugInfoStr += cloudPath.empty() ? L"OFF" : cloudPath;
	_debugInfoStr += L"\r\n";

	// Periodic Backup
	_debugInfoStr += L"Periodic Backup: ";
	_debugInfoStr += nppGui.isSnapshotMode() ? L"ON" : L"OFF";
	_debugInfoStr += L"\r\n";

	// Placeholders
	_debugInfoStr += L"Placeholders: ";
	_debugInfoStr += nppGui._keepSessionAbsentFileEntries ? L"ON" : L"OFF";
	_debugInfoStr += L"\r\n";

	// SC_TECHNOLOGY
	// Multi-instance
	_debugInfoStr += L"Multi-instance Mode: ";
	switch (nppGui._multiInstSetting)
	{
		case monoInst:
			_debugInfoStr += L"monoInst";
			break;
		case multiInstOnSession:
			_debugInfoStr += L"multiInstOnSession";
			break;
		case multiInst:
			_debugInfoStr += L"multiInst";
			break;
		default:
			_debugInfoStr += L"unknown(" + std::to_wstring(nppGui._multiInstSetting) + L")";
	}
	_debugInfoStr += L"\r\n";

	// asNotepad
	_debugInfoStr += L"asNotepad: ";
	_debugInfoStr += nppParam.isAsNotepadStyle() ? L"ON" : L"OFF";
	_debugInfoStr += L"\r\n";

	// File Status Auto-Detection
	_debugInfoStr += L"File Status Auto-Detection: ";
	if (nppGui._fileAutoDetection == cdDisabled)
	{
		_debugInfoStr += L"cdDisabled";
	}
	else
	{
		if (nppGui._fileAutoDetection & cdEnabled4AllFiles)
			_debugInfoStr += L"cdEnabled4AllFiles";
		else if (nppGui._fileAutoDetection & cdEnabled4CurrentFile)
			_debugInfoStr += L"cdEnabled4CurrentFile";
		else
			_debugInfoStr += L"cdUnknown (?!)";

		if (nppGui._fileAutoDetection & cdAutoUpdate)
			_debugInfoStr += L" + cdAutoUpdate";
		if (nppGui._fileAutoDetection & cdGo2end)
			_debugInfoStr += L" + cdGo2end";
	}
	_debugInfoStr += L"\r\n";

	// Dark Mode
	_debugInfoStr += L"Dark Mode: ";
	_debugInfoStr += nppGui._darkmode._isEnabled ? L"ON" : L"OFF";
	_debugInfoStr += L"\r\n";

	// Display Info
	_debugInfoStr += L"Display Info:";
	if (const QScreen* screen = QGuiApplication::primaryScreen())
	{
		_debugInfoStr += L"\r\n    primary monitor: " + std::to_wstring(screen->size().width());
		_debugInfoStr += L"x" + std::to_wstring(screen->size().height());
		_debugInfoStr += L", scaling " + std::to_wstring(static_cast<int>(screen->devicePixelRatio() * 100));
		_debugInfoStr += L"%";
	}
	_debugInfoStr += L"\r\n    visible monitors count: " + std::to_wstring(QGuiApplication::screens().size());
	_debugInfoStr += L"\r\n";

	// OS information
	_debugInfoStr += L"OS Name: ";
	_debugInfoStr += QSysInfo::prettyProductName().toStdWString();
	_debugInfoStr += L" (";
	_debugInfoStr += QSysInfo::currentCpuArchitecture().toStdWString();
	_debugInfoStr += L")\r\n";

	_debugInfoStr += L"Kernel: ";
	_debugInfoStr += QSysInfo::kernelType().toStdWString() + L" " + QSysInfo::kernelVersion().toStdWString();
	_debugInfoStr += L"\r\n";

	_debugInfoStr += L"Qt: ";
	_debugInfoStr += QString::fromLatin1(qVersion()).toStdWString();
	_debugInfoStr += L", ";
	_debugInfoStr += QGuiApplication::platformName().toStdWString();
	_debugInfoStr += L"\r\n";

	if (!_loadedPlugins.empty())
	{
		_debugInfoStr += L"Plugins: ";
		_debugInfoStr += _loadedPlugins;
		_debugInfoStr += L"\r\n";
	}
	else
	{
		_debugInfoStr += L"Plugins: none\r\n";
	}
}

void DebugInfoDlg::doDialog()
{
	if (!isCreated())
	{
		QDialog* dialog = createDialogWidget();
		Ui::DebuginfoboxDlg ui;
		ui.setupUi(dialog);

		buildDebugInfo();

		QObject::connect(ui.ok, &QAbstractButton::clicked, dialog, [this]() { display(false); });
		QObject::connect(ui.debuginfoCopylink, &QAbstractButton::clicked, dialog, [this, edit = ui.debuginfoEdit]()
		{
			// Visual effect
			edit->selectAll();

			// Copy to clipboard
			str2Clipboard(_debugInfoDisplay, _hSelf);

			// Set focus to edit control
			edit->setFocus();
		});
	}

	// Refresh the Debug Information.
	// For example, the command line parameters may have changed since this dialog was last opened during this session.
	refreshDebugInfo();

	goToCenter();
	display();
}

void DebugInfoDlg::refreshDebugInfo()
{
	_debugInfoDisplay = _debugInfoStr;

	size_t replacePos = _debugInfoDisplay.find(_cmdLinePlaceHolder);
	if (replacePos != std::string::npos)
	{
		_debugInfoDisplay.replace(replacePos, _cmdLinePlaceHolder.length(), NppParameters::getInstance().getCmdLineString());
	}

	// Set Debug Info text and leave the text in selected state
	auto* edit = qobject_cast<QPlainTextEdit*>(dlgItem(IDC_DEBUGINFO_EDIT));
	edit->setPlainText(QString::fromStdWString(_debugInfoDisplay));
	edit->selectAll();
	edit->setFocus();
}


static constexpr const wchar_t COMMAND_ARG_HELP[] = L"Usage:\r\n\
\r\n\
notepad++ [--help] [-multiInst] [-noPlugin] [-lLanguage] [-udl=\"My UDL Name\"]\r\n\
[-LlangCode] [-nLineNumber] [-cColumnNumber] [-pPosition] [-xLeftPos] [-yTopPos]\r\n\
[-monitor] [-monitoringMode] [-nosession] [-notabbar] [-systemtray] [-loadingTime]\r\n\
[-alwaysOnTop] [-ro] [-fullReadOnly] [-fullReadOnlySavingForbidden] [-openSession] [-r]\r\n\
[-qn=\"Easter egg name\" | -qt=\"a text to display.\" | -qf=\"/path/to/my quote.txt\"]\r\n\
[-qSpeed1|2|3] [-quickPrint] [-settingsDir=\"/path/to/your settings dir\"]\r\n\
[-openFoldersAsWorkspace]  [-titleAdd=\"additional title bar text\"]\r\n\
[filePath]\r\n\
\r\n\
--help: This help message\r\n\
-multiInst: Launch another Notepad++ instance\r\n\
-noPlugin: Launch Notepad++ without loading any plugin\r\n\
-l: Open file or Ghost type with syntax highlighting of choice\r\n\
-udl=\"My UDL Name\": Open file by applying User Defined Language\r\n\
-L: Apply indicated localization, langCode is browser language code\r\n\
-n: Scroll to indicated line on filePath\r\n\
-c: Scroll to indicated column on filePath\r\n\
-p: Scroll to indicated position on filePath\r\n\
-x: Move Notepad++ to indicated left side position on the screen\r\n\
-y: Move Notepad++ to indicated top position on the screen\r\n\
-monitor: Open files given in arguments with file monitoring enabled\r\n\
-monitoringMode: Monitoring all files opened in Notepad++\r\n\
-nosession: Launch Notepad++ without previous session\r\n\
-notabbar: Launch Notepad++ without tab bar\r\n\
-ro: Make the filePath read-only\r\n\
-fullReadOnly: Open all files read-only by default, toggling the R/O off and saving is allowed\r\n\
-fullReadOnlySavingForbidden: Open all files read-only by default, toggling the R/O off and saving is disabled\r\n\
-systemtray: Launch Notepad++ directly in system tray\r\n\
-loadingTime: Display Notepad++ loading time\r\n\
-alwaysOnTop: Make Notepad++ always on top\r\n\
-openSession: Open a session. filePath must be a session file\r\n\
-r: Open files recursively. This argument will be ignored if filePath contains no wildcard character\r\n\
-qn=\"Easter egg name\": Ghost type easter egg via its name\r\n\
-qt=\"text to display.\": Ghost type the given text\r\n\
-qf=\"/path/to/my quote.txt\": Ghost type a file content via the file path\r\n\
-qSpeed: Ghost typing speed. Value from 1 to 3 for slow, fast and fastest\r\n\
-quickPrint: Print the file given as argument then quit Notepad++\r\n\
-settingsDir=\"/path/to/your settings dir\": Override the default settings dir\r\n\
-openFoldersAsWorkspace: open filePath of folder(s) as workspace\r\n\
-titleAdd=\"string\": add string to Notepad++ title bar\r\n\
filePath: file or folder name to open (absolute or relative path name)\r\n\
";

void CmdLineArgsDlg::doDialog()
{
	if (!isCreated())
	{
		QDialog* dialog = createDialogWidget();
		Ui::CommandlineargsboxDlg ui;
		ui.setupUi(dialog);

		ui.commandlineargsEdit->setPlainText(QString::fromWCharArray(COMMAND_ARG_HELP));
		ui.commandlineargsEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

		QObject::connect(ui.ok, &QAbstractButton::clicked, dialog, [this]() { display(false); });
	}

	goToCenter();
	display();
}

void DoSaveOrNotBox::doDialog(bool isRTL)
{
	QDialog* dialog = createDialogWidget(isRTL);
	Ui::DosaveornotboxDlg ui;
	ui.setupUi(dialog);

	changeLang();
	ui.retry->setEnabled(_isMulti);
	ui.ignore->setEnabled(_isMulti);

	const auto answer = [this](QAbstractButton* button, int id)
	{
		QObject::connect(button, &QAbstractButton::clicked, _hSelf, [this, id]()
		{
			_clickedButtonId = id;
			endDialog(id == IDCANCEL ? -1 : 0);
		});
	};
	answer(ui.cancel, IDCANCEL);
	answer(ui.yes, IDYES);
	answer(ui.no, IDNO);
	answer(ui.ignore, IDIGNORE);
	answer(ui.retry, IDRETRY);

	goToCenter();
	runModal();
}

void DoSaveOrNotBox::onCancel()
{
	_clickedButtonId = IDCANCEL;
	endDialog(-1);
}

void DoSaveOrNotBox::changeLang()
{
	wstring msg;
	wstring defaultMessage = L"Save file \"$STR_REPLACE$\" ?";
	NativeLangSpeaker* nativeLangSpeaker = NppParameters::getInstance().getNativeLangSpeaker();

	if (nativeLangSpeaker && nativeLangSpeaker->changeDlgLang(_hSelf, "DoSaveOrNot"))
		msg = getItemText(IDC_DOSAVEORNOTTEXT);

	if (msg.empty())
		msg = defaultMessage;

	msg = stringReplace(msg, L"$STR_REPLACE$", _fn);
	setItemText(IDC_DOSAVEORNOTTEXT, msg);
	fitToWrappedText(_hSelf);
}

void DoSaveAllBox::doDialog(bool isRTL)
{
	QDialog* dialog = createDialogWidget(isRTL);
	Ui::DosaveallboxDlg ui;
	ui.setupUi(dialog);

	changeLang();

	const auto answer = [this](QAbstractButton* button, int id)
	{
		QObject::connect(button, &QAbstractButton::clicked, _hSelf, [this, id]()
		{
			_clickedButtonId = id;
			endDialog(id == IDCANCEL ? -1 : 0);
		});
	};
	answer(ui.yes, IDYES);
	answer(ui.retry, IDRETRY);
	answer(ui.no, IDNO);

	goToCenter();
	runModal();
}

void DoSaveAllBox::onCancel()
{
	_clickedButtonId = IDCANCEL;
	endDialog(-1);
}

void DoSaveAllBox::changeLang()
{
	wstring msg;
	wstring defaultMessage = L"Are you sure you want to save all modified documents?\r\rChoose \"Always Yes\" if you don't want to see this dialog again.\rYou can re-activate this dialog in Preferences later.";
	NativeLangSpeaker* nativeLangSpeaker = NppParameters::getInstance().getNativeLangSpeaker();

	if (nativeLangSpeaker && nativeLangSpeaker->changeDlgLang(_hSelf, "DoSaveAll"))
		msg = getItemText(IDC_DOSAVEALLTEXT);

	if (msg.empty())
		msg = defaultMessage;

	setItemText(IDC_DOSAVEALLTEXT, msg);
	fitToWrappedText(_hSelf);
}
