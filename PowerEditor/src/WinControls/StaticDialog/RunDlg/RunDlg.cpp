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


#include "RunDlg.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QCursor>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#include <string>
#include <vector>

#include "Common.h"
#include "CustomFileDialog.h"
#include "Notepad_plus_msgs.h"
#include "NppDarkMode.h"
#include "NppMenu.h"
#include "Parameters.h"
#include "RunDlg_rc.h"
#include "localization.h"
#include "menuCmdID.h"
#include "resource.h"
#include "shortcut.h"
#include "ui_RunDlg.h"

using namespace std;

namespace
{
	// Splits at the first space outside of double quotes. The quotes stay, as they matter for the arguments.
	void extractArgs(const wstring& commandLine, wstring& command, wstring& args)
	{
		size_t i = 0;
		bool quoted = false;
		for (; i < commandLine.size(); ++i)
		{
			if (commandLine[i] == L' ' && !quoted)
				break;

			if (commandLine[i] == L'"')
				quoted = !quoted;
		}
		command = commandLine.substr(0, i);

		const size_t argsBegin = commandLine.find_first_not_of(L' ', i);
		const size_t argsEnd = commandLine.find_last_not_of(L' ');
		args = (argsBegin == wstring::npos || argsBegin >= commandLine.size()) ? wstring{} : commandLine.substr(argsBegin, argsEnd - argsBegin + 1);
	}

}

// $VAR and ${VAR} from the environment. $(VAR) belongs to expandNppEnvironmentStrs().
wstring expandEnvironmentStrs(const wstring& text)
{
	static const QRegularExpression variable(QStringLiteral(R"(\$(?:\{(\w+)\}|(\w+)))"));
	const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

	const QString source = QString::fromStdWString(text);
	QString result;
	qsizetype last = 0;
	auto matches = variable.globalMatch(source);
	while (matches.hasNext())
	{
		const QRegularExpressionMatch match = matches.next();
		const QString name = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
		result += source.mid(last, match.capturedStart() - last);
		result += environment.contains(name) ? environment.value(name) : match.captured();
		last = match.capturedEnd();
	}
	result += source.mid(last);
	return result.toStdWString();
}

int whichVar(const std::wstring& str)
{
	if (str == fullCurrentPath)
		return FULL_CURRENT_PATH;
	else if (str == currentDirectory)
		return CURRENT_DIRECTORY;
	else if (str == onlyFileName)
		return FILE_NAME;
	else if (str == fileNamePart)
		return NAME_PART;
	else if (str == fileExtPart)
		return EXT_PART;
	else if (str == currentWord)
		return CURRENT_WORD;
	else if (str == nppDir)
		return NPP_DIRECTORY;
	else if (str == nppFullFilePath)
		return NPP_FULL_FILE_PATH;
	else if (str == currentLine)
		return CURRENT_LINE;
	else if (str == currentColumn)
		return CURRENT_COLUMN;
	else if (str == currentLineStr)
		return CURRENT_LINESTR;

	return VAR_NOT_RECOGNIZED;
}

wstring expandNppEnvironmentStrs(const wstring& strSrc, HWND hWnd)
{
	wstring result;
	size_t i = 0;
	while (i < strSrc.size())
	{
		if (strSrc.compare(i, 2, L"$(") == 0)
		{
			const size_t end = strSrc.find(L')', i + 2);
			if (end != wstring::npos)
			{
				const int internalVar = whichVar(strSrc.substr(i + 2, end - i - 2));
				if (internalVar != VAR_NOT_RECOGNIZED)
				{
					if (internalVar == CURRENT_LINE || internalVar == CURRENT_COLUMN)
					{
						result += std::to_wstring(::SendMessage(hWnd, RUNCOMMAND_USER + internalVar, 0, 0));
					}
					else
					{
						std::vector<wchar_t> expandedStr(CURRENTWORD_MAXLENGTH, L'\0');
						::SendMessage(hWnd, RUNCOMMAND_USER + internalVar, CURRENTWORD_MAXLENGTH, reinterpret_cast<LPARAM>(expandedStr.data()));
						result += expandedStr.data();
					}
					i = end + 1;
					continue;
				}
			}
		}
		result += strSrc[i];
		++i;
	}
	return result;
}

bool Command::run(HWND hWnd)
{
	return run(hWnd, L".");
}

bool Command::run(HWND hWnd, const wchar_t* cwd)
{
	wstring cmdPure;
	wstring args;
	extractArgs(_cmdLine, cmdPure, args);

	const wstring cmd2Exec = expandNppEnvironmentStrs(expandEnvironmentStrs(cmdPure), hWnd);
	const wstring args2Exec = expandNppEnvironmentStrs(expandEnvironmentStrs(args), hWnd);
	const wstring cwd2Exec = expandNppEnvironmentStrs(cwd, hWnd);

	QString program = QString::fromStdWString(cmd2Exec);
	if (program.size() >= 2 && program.startsWith(u'"') && program.endsWith(u'"'))
		program = program.mid(1, program.size() - 2);

	// As with ShellExecute's "open": a program runs, and a document or a link goes to its default application
	const QFileInfo info(program);
	const bool isProgram = (info.isFile() && info.isExecutable()) || !QStandardPaths::findExecutable(program).isEmpty();

	bool isLaunched = false;
	QString failure;
	if (isProgram)
	{
		QProcess process;
		process.setProgram(program);
		process.setArguments(QProcess::splitCommand(QString::fromStdWString(args2Exec)));
		process.setWorkingDirectory(QString::fromStdWString(cwd2Exec));
		isLaunched = process.startDetached();
		failure = process.errorString();
	}
	else if (!program.isEmpty() && (info.exists() || QUrl(program).isValid()))
	{
		const QUrl url = info.exists() ? QUrl::fromLocalFile(info.absoluteFilePath()) : QUrl(program);
		isLaunched = QDesktopServices::openUrl(url);
		failure = QStringLiteral("No application could open this.");
	}
	else
	{
		failure = QStringLiteral("The program was not found.");
	}

	if (!isLaunched)
	{
		wstring errorMsg = failure.toStdWString();
		errorMsg += L"\nAn attempt was made to execute the below command.";
		errorMsg += L"\n----------------------------------------------------------";
		errorMsg += L"\nCommand: ";
		errorMsg += cmd2Exec;
		errorMsg += L"\nArguments: ";
		errorMsg += args2Exec;
		errorMsg += L"\n----------------------------------------------------------";

		NppDarkMode::darkMessageBoxW(hWnd, errorMsg.c_str(), L"Run - ERROR", MB_ICONINFORMATION | MB_APPLMODAL);
	}

	return isLaunched;
}

void RunDlg::insertVariable(unsigned char id)
{
	wstring variable;
	switch (id)
	{
	case 0:
		variable = fullCurrentPath;
		break;
	case 1:
		variable = currentDirectory;
		break;
	case 2:
		variable = onlyFileName;
		break;
	case 3:
		variable = fileNamePart;
		break;
	case 4:
		variable = fileExtPart;
		break;
	case 5:
		variable = currentWord;
		break;
	case 6:
		variable = nppDir;
		break;
	case 7:
		variable = nppFullFilePath;
		break;
	case 8:
		variable = currentLine;
		break;
	case 9:
		variable = currentColumn;
		break;
	case 10:
		variable = currentLineStr;
		break;
	default:
		return; // Invalid id number
	}

	wstring cmdNew = getItemText(IDC_COMBO_RUN_PATH);
	cmdNew += L"$(" + variable + L")";
	setItemText(IDC_COMBO_RUN_PATH, cmdNew);
}

void RunDlg::onRun()
{
	_cmdLine = getItemText(IDC_COMBO_RUN_PATH);

	if (run(_hParent))
	{
		addTextToCombo(_cmdLine.c_str());
		display(false);
	}
	else
	{
		removeTextFromCombo(_cmdLine.c_str());
	}
}

void RunDlg::onSave()
{
	NppParameters& nppParams = NppParameters::getInstance();
	std::vector<UserCommand> & theUserCmds = nppParams.getUserCommandList();

	const auto nbCmd = static_cast<int>(theUserCmds.size());
	int cmdID = ID_USER_CMD + nbCmd;

	DynamicMenu& runMenu = nppParams.getRunMenuItems();
	int nbTopLevelItem = runMenu.getTopLevelItemNumber();

	UserCommand uc(Shortcut(), wstring2string(getItemText(IDC_COMBO_RUN_PATH), CP_UTF8).c_str(), cmdID);

	uc.init(_hInst, _hSelf);
	if (uc.doDialog() != -1)
	{
		HMENU mainMenu = reinterpret_cast<HMENU>(::SendMessage(_hParent, NPPM_INTERNAL_GETMENU, 0, 0));
		HMENU hRunMenu = nppSubMenuAt(mainMenu, MENUINDEX_RUN);
		int const posBase = runMenu.getPosBase();

		if (nbTopLevelItem == 0)
			nppInsertSeparatorAt(hRunMenu, posBase - 1);

		theUserCmds.push_back(uc);
		runMenu.push_back(MenuItemUnit(cmdID, string2wstring(uc.getName(), CP_UTF8)));
		nppInsertMenuItemAt(hRunMenu, posBase + nbTopLevelItem, cmdID, string2wstring(uc.getMenuName(), CP_UTF8), _hParent);

		if (nbTopLevelItem == 0)
		{
			// Insert the separator and modify/delete command
			nppInsertSeparatorAt(hRunMenu, posBase + nbTopLevelItem + 1);
			NativeLangSpeaker *pNativeLangSpeaker = nppParams.getNativeLangSpeaker();
			wstring nativeLangShortcutMapperMacro = pNativeLangSpeaker->getNativeLangMenuString(IDM_SETTING_SHORTCUT_MAPPER_MACRO);
			if (nativeLangShortcutMapperMacro.empty())
				nativeLangShortcutMapperMacro = runMenu.getLastCmdLabel();

			nppInsertMenuItemAt(hRunMenu, posBase + nbTopLevelItem + 2, IDM_SETTING_SHORTCUT_MAPPER_RUN, nativeLangShortcutMapperMacro, _hParent);
		}
		nppParams.getAccelerator()->updateShortcuts();
		nppParams.setShortcutDirty();
	}
}

void RunDlg::onBrowse()
{
	CustomFileDialog fd(_hSelf);
	fd.setExtFilter(L"All Files", L".*");

	wstring fn = fd.doOpenSingleFileDlg();
	if (!fn.empty())
	{
		if (fn.find(L' ') != wstring::npos)
			fn = L"\"" + fn + L"\"";
		addTextToCombo(fn.c_str());
	}
}

void RunDlg::onVariables()
{
	HMENU menu = nppCreateMenuFromTemplate(IDR_RUN_DLG_MENU_VARIABLES, nullptr, _hSelf);
	HMENU variablePopup = nppSubMenuAt(menu, 0);
	const QWidget* variablesButton = dlgItem(IDC_BUTTON_VARIABLES);
	QAction* chosen = variablePopup->exec(variablesButton->mapToGlobal(QPoint(variablesButton->width(), 0)));
	if (chosen)
	{
		const int cmd = chosen->data().toInt();
		if (cmd >= IDM_RUN_DLG_VARMENU_START && cmd <= IDM_RUN_DLG_VARMENU_END)
			insertVariable(static_cast<unsigned char>(cmd - IDM_RUN_DLG_VARMENU_START));
	}
	delete menu;
}

void RunDlg::addTextToCombo(const wchar_t *txt2Add) const
{
	QComboBox* comboBox = combo(IDC_COMBO_RUN_PATH);
	const QString text = QString::fromStdWString(txt2Add);
	int i = comboBox->findText(text, Qt::MatchExactly | Qt::MatchCaseSensitive);
	if (i < 0)
	{
		comboBox->addItem(text);
		i = comboBox->count() - 1;
	}
	comboBox->setCurrentIndex(i);
}

void RunDlg::removeTextFromCombo(const wchar_t *txt2Remove) const
{
	QComboBox* comboBox = combo(IDC_COMBO_RUN_PATH);
	const int i = comboBox->findText(QString::fromStdWString(txt2Remove), Qt::MatchExactly | Qt::MatchCaseSensitive);
	if (i >= 0)
		comboBox->removeItem(i);
}

void RunDlg::doDialog(bool isRTL)
{
	if (!isCreated())
	{
		QDialog* dialog = createDialogWidget(isRTL);
		Ui::RunDlg ui;
		ui.setupUi(dialog);

		QObject::connect(button(IDOK), &QAbstractButton::clicked, dialog, [this]() { onRun(); });
		QObject::connect(button(IDCANCEL), &QAbstractButton::clicked, dialog, [this]() { display(false); });
		QObject::connect(button(IDC_BUTTON_SAVE), &QAbstractButton::clicked, dialog, [this]() { onSave(); });
		QObject::connect(button(IDC_BUTTON_FILE_BROWSER), &QAbstractButton::clicked, dialog, [this]() { onBrowse(); });
		QObject::connect(button(IDC_BUTTON_VARIABLES), &QAbstractButton::clicked, dialog, [this]() { onVariables(); });
	}

	// Adjust the position in the center
	goToCenter();
	display();
	dlgItem(IDC_COMBO_RUN_PATH)->setFocus();
}
