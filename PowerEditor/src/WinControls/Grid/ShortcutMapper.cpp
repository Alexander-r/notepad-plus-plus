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


#include "ShortcutMapper.h"

#include <QAbstractButton>
#include <QAction>
#include <QDialog>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QSignalBlocker>
#include <QTabBar>
#include <QTableWidget>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "Common.h"
#include "NppDarkMode.h"
#include "NppMenu.h"
#include "NppMessages.h"
#include "Parameters.h"
#include "PluginInterface.h"
#include "ShortcutMapper_rc.h"
#include "Window.h"
#include "localization.h"
#include "menuCmdID.h"
#include "resource.h"
#include "shortcut.h"
#include "ui_ShortcutmapperDlg.h"

using namespace std;

void ShortcutMapper::initTabs()
{
	for (size_t i = 0; i < _nbTab; ++i)
	{
		_tabNames[i] = getTabString(i);
		_tabs->addTab(QString::fromStdWString(_tabNames[i]));
	}

	_tabs->setCurrentIndex(static_cast<int>(_currentState));
}

wstring ShortcutMapper::getTabString(size_t i) const
{
	if (i >= _nbTab)
		return L"";

	NativeLangSpeaker* nativeLangSpeaker = NppParameters::getInstance().getNativeLangSpeaker();
	switch (i)
	{
		case 1:
			return nativeLangSpeaker->getShortcutMapperLangStr("MacrosTab", L"Macros");

		case 2:
			return nativeLangSpeaker->getShortcutMapperLangStr("RunCommandsTab", L"Run commands");

		case 3:
			return nativeLangSpeaker->getShortcutMapperLangStr("PluginCommandsTab", L"Plugin commands");

		case 4:
			return nativeLangSpeaker->getShortcutMapperLangStr("ScintillaCommandsTab", L"Scintilla commands");

		default: //0
			return nativeLangSpeaker->getShortcutMapperLangStr("MainMenuTab", L"Main menu");
	}
}

void ShortcutMapper::doDialog(bool isRTL)
{
	QDialog* dialog = createDialogWidget(isRTL);
	Ui::ShortcutmapperDlg ui;
	ui.setupUi(dialog);

	_grid = ui.shortcutTable;
	_tabs = ui.babygridTabbar;

	initGrid();
	initTabs();
	connectControls();

	// The shortcut dialogs ask this one about the key they are editing
	nppRegisterMessageTarget(dialog, [this](UINT message, WPARAM wParam, LPARAM lParam) { return onMessage(message, wParam, lParam); });

	fillOutGrid();
	goToCenter();

	runModal();

	nppUnregisterMessageTarget(dialog);
	_grid = nullptr;
	_tabs = nullptr;
}

void ShortcutMapper::onCancel()
{
	endDialog(-1);
}

void ShortcutMapper::initGrid()
{
	_lastHomeRow.resize(_nbTab, 0);
	_lastCursorRow.resize(_nbTab, 0);

	_grid->setSelectionBehavior(QAbstractItemView::SelectRows);
	_grid->setSelectionMode(QAbstractItemView::SingleSelection);
	_grid->setEditTriggers(QAbstractItemView::NoEditTriggers);
	_grid->setWordWrap(false);
	_grid->setContextMenuPolicy(Qt::CustomContextMenu);
	_grid->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
	_grid->verticalHeader()->setDefaultSectionSize(_grid->fontMetrics().height() + 6);
	_grid->horizontalHeader()->setSectionsClickable(false);
	_grid->horizontalHeader()->setHighlightSections(false);

	NativeLangSpeaker* nativeLangSpeaker = NppParameters::getInstance().getNativeLangSpeaker();
	nativeLangSpeaker->changeDlgLang(_hSelf, "ShortcutMapper");
	_conflictInfoOk = nativeLangSpeaker->getShortcutMapperLangStr("ConflictInfoOk", L"No shortcut conflicts for this item.");
	_conflictInfoEditing = nativeLangSpeaker->getShortcutMapperLangStr("ConflictInfoEditing", L"No conflicts . . .");
}

void ShortcutMapper::connectControls()
{
	QObject::connect(_tabs, &QTabBar::currentChanged, _hSelf, [this](int index) { onTabChanged(index); });
	QObject::connect(_grid, &QTableWidget::itemSelectionChanged, _hSelf, [this]() { onRowChanged(); });
	QObject::connect(_grid, &QTableWidget::cellDoubleClicked, _hSelf, [this]() { onModify(); });
	QObject::connect(_grid, &QWidget::customContextMenuRequested, _hSelf, [this](const QPoint& position) { showContextMenu(position); });

	QObject::connect(button(IDM_BABYGRID_MODIFY), &QAbstractButton::clicked, _hSelf, [this]() { onModify(); });
	QObject::connect(button(IDM_BABYGRID_CLEAR), &QAbstractButton::clicked, _hSelf, [this]() { onClear(); });
	QObject::connect(button(IDM_BABYGRID_DELETE), &QAbstractButton::clicked, _hSelf, [this]() { onDelete(); });
	QObject::connect(button(IDOK), &QAbstractButton::clicked, _hSelf, [this]() { endDialog(0); });

	auto* filter = qobject_cast<QLineEdit*>(dlgItem(IDC_BABYGRID_FILTER));
	QObject::connect(filter, &QLineEdit::textChanged, _hSelf, [this]() { fillOutGrid(); });
	QObject::connect(button(IDC_BABYGRID_FILTER_CLEAR), &QAbstractButton::clicked, _hSelf, [filter]()
	{
		filter->clear();
		filter->setFocus();
	});

	auto* deleteKey = new QShortcut(QKeySequence(Qt::Key_Delete), _grid);
	deleteKey->setContext(Qt::WidgetShortcut);
	QObject::connect(deleteKey, &QShortcut::activated, _hSelf, [this]()
	{
		if (_currentState == STATE_MACRO || _currentState == STATE_USER)
			onDelete();
	});
}

LRESULT ShortcutMapper::onMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message != NPPM_INTERNAL_FINDKEYCONFLICTS || !wParam || !lParam || _grid->currentRow() < 0)
		return 0;

	wstring conflictInfo;

	// In case of using filter will make the filtered items change index, so here we get its real index
	size_t realIndexOfSelectedItem = _shortcutIndex[_grid->currentRow()];

	const bool isConflict = findKeyConflicts(&conflictInfo, *reinterpret_cast<KeyCombo*>(wParam), realIndexOfSelectedItem);
	*reinterpret_cast<bool*>(lParam) = isConflict;

	setItemText(IDC_BABYGRID_INFO, isConflict ? conflictInfo : _conflictInfoEditing);
	return TRUE;
}

void ShortcutMapper::saveCurrentView()
{
	_lastHomeRow[_currentState] = _grid->rowAt(0);
	_lastCursorRow[_currentState] = _grid->currentRow();
}

void ShortcutMapper::onTabChanged(int index)
{
	//save the current view
	saveCurrentView();
	_currentState = static_cast<GridState>(index);
	fillOutGrid();
}

void ShortcutMapper::addRow(size_t index, const string& name, const string& shortcut, const wstring& extra, bool isConflict)
{
	const int row = static_cast<int>(_shortcutIndex.size());
	_shortcutIndex.push_back(index);

	const auto setCell = [this, row, isConflict](int column, const QString& text)
	{
		auto* item = new QTableWidgetItem(text);
		if (isConflict)
		{
			if (NppDarkMode::usesDarkWidgets())
			{
				const COLORREF colour = NppDarkMode::getErrorBackgroundColor();
				item->setBackground(QColor(GetRValue(colour), GetGValue(colour), GetBValue(colour)));
			}
			else
				item->setBackground(QColor(255, 130, 120));
		}
		_grid->setItem(row, column, item);
	};

	setCell(0, QString::fromUtf8(name));
	setCell(1, QString::fromUtf8(shortcut));
	if (_grid->columnCount() > 2)
		setCell(2, QString::fromStdWString(extra));
}

bool ShortcutMapper::isFilterValid(Shortcut sc)
{
	// all words in _shortcutFilter must be in the name or keycombo
	// For example, the shortcut with name "foo bar baz" and keycombo "Ctrl+A"
	// would be matched by the filter "foo ctrl" and the filter "bar +a"
	// but *not* by the filter "foo shift" or the filter "quz +a"
	size_t filterSize = _shortcutFilter.size();
	if (filterSize == 0)
		return true;

	wstring shortcut_name = stringToLower(string2wstring(sc.getName(), CP_UTF8));
	wstring shortcut_value = stringToLower(string2wstring(sc.toString(), CP_UTF8));

	for (size_t i = 0; i < filterSize; ++i)
	{
		wstring filterWord = _shortcutFilter.at(i);
		// every word must be matched by keycombo or name
		if (shortcut_name.find(filterWord) == std::string::npos &&
			shortcut_value.find(filterWord) == std::string::npos)
			return false;
	}
	return true;
}

bool ShortcutMapper::isFilterValid(PluginCmdShortcut sc)
{
	// all words in _shortcutFilter must be in the name or the keycombo or the plugin name
	// For example, the shortcut with name "foo bar baz" and keycombo "Ctrl+A" and plugin "BlahLint"
	// would be matched by the filter "foo ctrl" and the filter "bar +a blah"
	// but *not* by the filter "baz shift" or the filter "lint quz" 
	size_t filterSize = _shortcutFilter.size();
	if (filterSize == 0)
		return true;

	wstring shortcut_name = stringToLower(string2wstring(sc.getName(), CP_UTF8));
	wstring shortcut_value = stringToLower(string2wstring(sc.toString(), CP_UTF8));
	wstring module_name = stringToLower(string2wstring(sc.getModuleName(), CP_UTF8));
	
	for (size_t i = 0; i < filterSize; ++i)
	{
		wstring filterWord = _shortcutFilter.at(i);
		// every word must be matched by keycombo or name or plugin name
		if (shortcut_name.find(filterWord) == std::string::npos &&
			shortcut_value.find(filterWord) == std::string::npos &&
			module_name.find(filterWord) == std::string::npos)
			return false;
	}
	return true;
}

bool ShortcutMapper::isFilterValid(ScintillaKeyMap sc)
{
	// all words in _shortcutFilter must be in the name or the list of keycombos
	// For example, the shortcut with name "foo bar baz" and keycombo "Ctrl+A or Alt+G"
	// would be matched by the filter "foo ctrl" and the filter "bar alt or"
	// but *not* by the filter "foo shift" or the filter "quz +a"
	size_t filterSize = _shortcutFilter.size();
	if (filterSize == 0)
		return true;

	wstring shortcut_name = stringToLower(string2wstring(sc.getName(), CP_UTF8));
	wstring shortcut_value = stringToLower(string2wstring(sc.toString(), CP_UTF8));

	for (size_t i = 0; i < filterSize; ++i)
	{
		wstring filterWord = _shortcutFilter.at(i);
		// every word must be matched by keycombo or name
		if (shortcut_name.find(filterWord) == std::string::npos &&
			shortcut_value.find(filterWord) == std::string::npos)
			return false;
	}
	return true;
}

void ShortcutMapper::fillOutGrid()
{
	NppParameters& nppParam = NppParameters::getInstance();
	NativeLangSpeaker* nativeLangSpeaker = nppParam.getNativeLangSpeaker();

	const QSignalBlocker blocker(_grid);
	_grid->setUpdatesEnabled(false);
	_grid->setRowCount(0);
	_shortcutIndex.clear();

	size_t nbItems = 0;
	wstring extraColumn;
	switch(_currentState)
	{
		case STATE_MENU:
			nbItems = nppParam.getUserShortcuts().size();
			extraColumn = nativeLangSpeaker->getShortcutMapperLangStr("ColumnCategory", L"Category");
			break;

		case STATE_MACRO:
			nbItems = nppParam.getMacroList().size();
			break;

		case STATE_USER:
			nbItems = nppParam.getUserCommandList().size();
			break;

		case STATE_PLUGIN:
			nbItems = nppParam.getPluginCommandList().size();
			extraColumn = nativeLangSpeaker->getShortcutMapperLangStr("ColumnPlugin", L"Plugin");
			break;

		case STATE_SCINTILLA:
			nbItems = nppParam.getScintillaKeyList().size();
			break;
	}

	QStringList headers{
		QString::fromStdWString(nativeLangSpeaker->getShortcutMapperLangStr("ColumnName", L"Name")),
		QString::fromStdWString(nativeLangSpeaker->getShortcutMapperLangStr("ColumnShortcut", L"Shortcut")) };
	if (!extraColumn.empty())
		headers << QString::fromStdWString(extraColumn);
	_grid->setColumnCount(static_cast<int>(headers.size()));
	_grid->setHorizontalHeaderLabels(headers);
	_grid->setRowCount(static_cast<int>(nbItems));

	// make _shortcutFilter a list of the words in IDC_BABYGRID_FILTER
	wstring shortcutFilterStr = stringToLower(getItemText(IDC_BABYGRID_FILTER));
	const wstring whitespace(L" ");
	std::vector<wstring> shortcutFilterWithEmpties;
	stringSplit(shortcutFilterStr, whitespace, shortcutFilterWithEmpties);
	// now add only the non-empty strings in the split list to _shortcutFilter
	_shortcutFilter = std::vector<wstring>();
	for (size_t i = 0; i < shortcutFilterWithEmpties.size(); ++i)
	{
		wstring filterWord = shortcutFilterWithEmpties.at(i);
		if (!filterWord.empty())
			_shortcutFilter.push_back(filterWord);
	}

	bool isModifyEnabled = true;
	bool isClearEnabled = true;
	bool isDeleteEnabled = false;

	// Rows for the shortcuts of a tab that have one key combination each
	const auto addRows = [&](auto& cshortcuts, auto getExtra)
	{
		for (size_t i = 0; i < nbItems; ++i)
		{
			if (isFilterValid(cshortcuts[i]))
			{
				//avoid empty strings for better performance
				const string keys = cshortcuts[i].isEnabled() ? cshortcuts[i].toString() : string();
				addRow(i, cshortcuts[i].getName(), keys, getExtra(cshortcuts[i]), findKeyConflicts(nullptr, cshortcuts[i].getKeyCombo(), i));
			}
		}
	};
	const auto noExtra = [](const auto&) { return wstring(); };

	switch(_currentState)
	{
		case STATE_MENU:
		{
			addRows(nppParam.getUserShortcuts(), [](const CommandShortcut& shortcut) { return wstring(shortcut.getCategory()); });
		}
		break;

		case STATE_MACRO:
		{
			addRows(nppParam.getMacroList(), noExtra);
			isModifyEnabled = isClearEnabled = isDeleteEnabled = nbItems > 0;
		}
		break;

		case STATE_USER:
		{
			addRows(nppParam.getUserCommandList(), noExtra);
			isModifyEnabled = isClearEnabled = isDeleteEnabled = nbItems > 0;
		}
		break;

		case STATE_PLUGIN:
		{
			addRows(nppParam.getPluginCommandList(), [](const PluginCmdShortcut& shortcut) { return string2wstring(shortcut.getModuleName(), CP_UTF8); });
			isModifyEnabled = isClearEnabled = nbItems > 0;
		}
		break;

		case STATE_SCINTILLA:
		{
			vector<ScintillaKeyMap> & cshortcuts = nppParam.getScintillaKeyList();
			for (size_t i = 0; i < nbItems; ++i)
			{
				if (isFilterValid(cshortcuts[i]))
				{
					bool isMarker = false;
					if (cshortcuts[i].isEnabled())
					{
						size_t sciCombos = cshortcuts[i].getSize();
						for (size_t sciIndex = 0; sciIndex < sciCombos; ++sciIndex)
						{
							if (findKeyConflicts(nullptr, cshortcuts[i].getKeyComboByIndex(sciIndex), i))
							{
								isMarker = true;
								break;
							}
						}
					}

					const string keys = cshortcuts[i].isEnabled() ? cshortcuts[i].toString() : string();
					addRow(i, cshortcuts[i].getName(), keys, {}, isMarker);
				}
			}
			isClearEnabled = false;
		}
		break;
	}

	_grid->setRowCount(static_cast<int>(_shortcutIndex.size()));

	QHeaderView* header = _grid->horizontalHeader();
	header->setStretchLastSection(false);
	header->setSectionResizeMode(0, QHeaderView::Stretch);
	for (int column = 1; column < _grid->columnCount(); ++column)
		header->setSectionResizeMode(column, QHeaderView::ResizeToContents);

	enableItem(IDM_BABYGRID_MODIFY, isModifyEnabled);
	enableItem(IDM_BABYGRID_CLEAR, isClearEnabled);
	enableItem(IDM_BABYGRID_DELETE, isDeleteEnabled);

	_grid->setUpdatesEnabled(true);

	if (_grid->rowCount() > 0) //restore the last view
	{
		const int lastRow = _grid->rowCount() - 1;
		_grid->setCurrentCell(std::clamp(_lastCursorRow[_currentState], 0, lastRow), 0);
		_grid->scrollToItem(_grid->item(std::clamp(_lastHomeRow[_currentState], 0, lastRow), 0), QAbstractItemView::PositionAtTop);
		onRowChanged();
	}
	else //clear the info area
		setItemText(IDC_BABYGRID_INFO, L"");
}

void ShortcutMapper::onRowChanged()
{
	const int row = _grid->currentRow();
	if (row < 0)
		return;

	NppParameters& nppParam = NppParameters::getInstance();

	// In case of using filter will make the filtered items change index, so here we get its real index
	size_t realIndexOfSelectedItem = _shortcutIndex[row];

	wstring conflictInfo;

	switch (_currentState)
	{
		case STATE_MENU:
		{
			vector<CommandShortcut> & vShortcuts = nppParam.getUserShortcuts();
			findKeyConflicts(&conflictInfo, vShortcuts[realIndexOfSelectedItem].getKeyCombo(), realIndexOfSelectedItem);
		}
		break;

		case STATE_MACRO:
		{
			vector<MacroShortcut> & vShortcuts = nppParam.getMacroList();
			findKeyConflicts(&conflictInfo, vShortcuts[realIndexOfSelectedItem].getKeyCombo(), realIndexOfSelectedItem);
		}
		break;

		case STATE_USER:
		{
			vector<UserCommand> & vShortcuts = nppParam.getUserCommandList();
			findKeyConflicts(&conflictInfo, vShortcuts[realIndexOfSelectedItem].getKeyCombo(), realIndexOfSelectedItem);
		}
		break;

		case STATE_PLUGIN:
		{
			vector<PluginCmdShortcut> & vShortcuts = nppParam.getPluginCommandList();
			findKeyConflicts(&conflictInfo, vShortcuts[realIndexOfSelectedItem].getKeyCombo(), realIndexOfSelectedItem);
		}
		break;

		case STATE_SCINTILLA:
		{
			vector<ScintillaKeyMap> & vShortcuts = nppParam.getScintillaKeyList();
			size_t sciCombos = vShortcuts[realIndexOfSelectedItem].getSize();
			for (size_t sciIndex = 0; sciIndex < sciCombos; ++sciIndex)
				findKeyConflicts(&conflictInfo, vShortcuts[realIndexOfSelectedItem].getKeyComboByIndex(sciIndex), realIndexOfSelectedItem);
		}
		break;
	}

	setItemText(IDC_BABYGRID_INFO, conflictInfo.empty() ? _conflictInfoOk : conflictInfo);
}

void ShortcutMapper::onClear()
{
	if (_grid->currentRow() < 0)
		return;

	NppParameters& nppParam = NppParameters::getInstance();
	size_t shortcutIndex = _shortcutIndex[_grid->currentRow()];

	switch(_currentState)
	{
		case STATE_MENU:
		{
			//Get CommandShortcut corresponding to row
			vector<CommandShortcut> & shortcuts = nppParam.getUserShortcuts();
			CommandShortcut csc = shortcuts[shortcutIndex];
			csc.clear();
			shortcuts[shortcutIndex] = csc;
			//shortcut was altered
			nppParam.addUserModifiedIndex(shortcutIndex);

			saveCurrentView();
			fillOutGrid();

			//Notify current Accelerator class to update everything
			nppParam.getAccelerator()->updateShortcuts();
			nppParam.setShortcutDirty();
		}
		break;

		case STATE_MACRO:
		{
			//Get MacroShortcut corresponding to row
			vector<MacroShortcut> & shortcuts = nppParam.getMacroList();
			MacroShortcut msc = shortcuts[shortcutIndex];
			msc.clear();
			shortcuts[shortcutIndex] = msc;

			saveCurrentView();
			fillOutGrid();

			//Notify current Accelerator class to update everything
			nppParam.getAccelerator()->updateShortcuts();
			nppParam.setShortcutDirty();
		}
		break;

		case STATE_USER:
		{
			//Get UserCommand corresponding to row
			vector<UserCommand> & shortcuts = nppParam.getUserCommandList();
			UserCommand ucmd = shortcuts[shortcutIndex];
			ucmd.clear();

			//shortcut was altered
			shortcuts[shortcutIndex] = ucmd;

			saveCurrentView();
			fillOutGrid();

			//Notify current Accelerator class to update everything
			nppParam.getAccelerator()->updateShortcuts();
			nppParam.setShortcutDirty();
		}
		break;

		case STATE_PLUGIN:
		{
			//Get PluginCmdShortcut corresponding to row
			vector<PluginCmdShortcut> & shortcuts = nppParam.getPluginCommandList();
			PluginCmdShortcut pcsc = shortcuts[shortcutIndex];
			pcsc.clear();
			//shortcut was altered
			nppParam.addPluginModifiedIndex(shortcutIndex);
			shortcuts[shortcutIndex] = pcsc;

			saveCurrentView();
			fillOutGrid();

			//Notify current Accelerator class to update everything
			nppParam.getAccelerator()->updateShortcuts();
			unsigned long cmdID = pcsc.getID();
			ShortcutKey shortcut;
			shortcut._isAlt = FALSE;
			shortcut._isCtrl = FALSE;
			shortcut._isShift = FALSE;
			shortcut._key = '\0';

			::SendMessage(_hParent, NPPM_INTERNAL_PLUGINSHORTCUTMODIFIED, cmdID, reinterpret_cast<LPARAM>(&shortcut));
			nppParam.setShortcutDirty();
		}
		break;

		case STATE_SCINTILLA:
		{
			// Do nothing
		}
		break;
	}
}

void ShortcutMapper::onModify()
{
	if (_grid->currentRow() < 0)
		return;

	NppParameters& nppParam = NppParameters::getInstance();
	size_t shortcutIndex = _shortcutIndex[_grid->currentRow()];
	bool isModified = false;

	switch(_currentState)
	{
		case STATE_MENU:
		{
			//Get CommandShortcut corresponding to row
			vector<CommandShortcut> & shortcuts = nppParam.getUserShortcuts();
			CommandShortcut csc = shortcuts[shortcutIndex], prevcsc = shortcuts[shortcutIndex];
			csc.init(_hInst, _hSelf);
			if (csc.doDialog() != -1 && prevcsc != csc)
			{
				//shortcut was altered
				nppParam.addUserModifiedIndex(shortcutIndex);
				shortcuts[shortcutIndex] = csc;

				saveCurrentView();
				fillOutGrid();

				isModified = true;

				//Notify current Accelerator class to update everything
				nppParam.getAccelerator()->updateShortcuts();
				nppParam.setShortcutDirty();
			}
		}
		break;

		case STATE_MACRO:
		{
			//Get MacroShortcut corresponding to row
			vector<MacroShortcut> & shortcuts = nppParam.getMacroList();
			MacroShortcut msc = shortcuts[shortcutIndex], prevmsc = shortcuts[shortcutIndex];
			msc.init(_hInst, _hSelf);
			if (msc.doDialog() != -1 && prevmsc != msc)
			{
				//shortcut was altered
				shortcuts[shortcutIndex] = msc;

				saveCurrentView();
				fillOutGrid();

				isModified = true;

				//Notify current Accelerator class to update everything
				nppParam.getAccelerator()->updateShortcuts();
				nppParam.setShortcutDirty();
			}
		}
		break;

		case STATE_USER:
		{
			//Get UserCommand corresponding to row
			vector<UserCommand> & shortcuts = nppParam.getUserCommandList();
			UserCommand ucmd = shortcuts[shortcutIndex];
			ucmd.init(_hInst, _hSelf);
			UserCommand prevucmd = ucmd;
			if (ucmd.doDialog() != -1 && prevucmd != ucmd)
			{
				//shortcut was altered
				shortcuts[shortcutIndex] = ucmd;

				saveCurrentView();
				fillOutGrid();

				isModified = true;

				//Notify current Accelerator class to update everything
				nppParam.getAccelerator()->updateShortcuts();
				nppParam.setShortcutDirty();
			}
		}
		break;

		case STATE_PLUGIN:
		{
			//Get PluginCmdShortcut corresponding to row
			vector<PluginCmdShortcut> & shortcuts = nppParam.getPluginCommandList();
			PluginCmdShortcut pcsc = shortcuts[shortcutIndex];
			pcsc.init(_hInst, _hSelf);
			PluginCmdShortcut prevpcsc = pcsc;
			if (pcsc.doDialog() != -1 && prevpcsc != pcsc)
			{
				//shortcut was altered
				nppParam.addPluginModifiedIndex(shortcutIndex);
				shortcuts[shortcutIndex] = pcsc;

				saveCurrentView();
				fillOutGrid();

				isModified = true;

				//Notify current Accelerator class to update everything
				nppParam.getAccelerator()->updateShortcuts();
				unsigned long cmdID = pcsc.getID();
				ShortcutKey shortcut;
				shortcut._isAlt = pcsc.getKeyCombo()._isAlt;
				shortcut._isCtrl = pcsc.getKeyCombo()._isCtrl;
				shortcut._isShift = pcsc.getKeyCombo()._isShift;
				shortcut._key = pcsc.getKeyCombo()._key;

				::SendMessage(_hParent, NPPM_INTERNAL_PLUGINSHORTCUTMODIFIED, cmdID, reinterpret_cast<LPARAM>(&shortcut));
				nppParam.setShortcutDirty();
			}
		}
		break;

		case STATE_SCINTILLA:
		{
			//Get ScintillaKeyMap corresponding to row
			vector<ScintillaKeyMap> & shortcuts = nppParam.getScintillaKeyList();
			ScintillaKeyMap skm = shortcuts[shortcutIndex], prevskm = shortcuts[shortcutIndex];
			skm.init(_hInst, _hSelf);
			if (skm.doDialog() != -1 && prevskm != skm)
			{
				//shortcut was altered
				nppParam.addScintillaModifiedIndex(static_cast<int>(shortcutIndex));
				shortcuts[shortcutIndex] = skm;

				saveCurrentView();
				fillOutGrid();

				isModified = true;

				//Notify current Accelerator class to update key
				nppParam.getScintillaAccelerator()->updateKeys();
				nppParam.setShortcutDirty();
			}
		}
		break;
	}

	// The dialog's editing has left its own text in the information area
	if (!isModified)
		onRowChanged();
}

// Removes a command from its dynamic menu, after its entry has gone from the list of commands
void ShortcutMapper::deleteCommand(int menuIndex, int mapperCmd, DynamicMenu& menu, size_t shortcutIndex, size_t nbElem)
{
	// clear all menu
	menu.clearMenu();

	// Erase the menu item
	menu.erase(shortcutIndex);

	for (size_t i = shortcutIndex; i < nbElem; ++i)	//lower the IDs of the remaining items so there are no gaps
	{
		// Adjust menu items
		MenuItemUnit& miu = menu.getItemFromIndex(i);
		miu._cmdID -= 1;	//shift all IDs
	}
	// create from scratch according the new menu items structure
	menu.createMenu();

	HMENU m = reinterpret_cast<HMENU>(::SendMessage(_hParent, NPPM_INTERNAL_GETMENU, 0, 0));
	HMENU hMenu = nppSubMenuAt(m, menuIndex);
	if (!hMenu)
		return;

	int32_t posBase = menu.getPosBase();
	if (nbElem == 0)
	{
		nppRemoveMenuCommand(hMenu, mapperCmd);

		//remove separator
		nppRemoveMenuItemAt(hMenu, posBase - 1);
		nppRemoveMenuItemAt(hMenu, posBase - 1);
	}
}

void ShortcutMapper::onDelete()
{
	if (_grid->currentRow() < 0)
		return;

	NppParameters& nppParam = NppParameters::getInstance();
	int res = nppParam.getNativeLangSpeaker()->messageBox("SCMapperDoDeleteOrNot",
		_hSelf,
		L"Are you sure you want to delete this shortcut?",
		L"Are you sure?",
		MB_OKCANCEL);

	if (res == IDOK)
	{
		size_t shortcutIndex = _shortcutIndex[_grid->currentRow()];

		switch(_currentState)
		{
			case STATE_MENU:
			case STATE_PLUGIN:
			case STATE_SCINTILLA:
			{
				return;
			}

			case STATE_MACRO:
			{
				vector<MacroShortcut> & theMacros = nppParam.getMacroList();
				theMacros.erase(theMacros.begin() + shortcutIndex);

				saveCurrentView();
				fillOutGrid();

				size_t nbElem = theMacros.size();
				for (size_t i = shortcutIndex; i < nbElem; ++i)	//lower the IDs of the remaining items so there are no gaps
				{
					MacroShortcut ms = theMacros[i];
					ms.setID(ms.getID() - 1);	//shift all IDs
					theMacros[i] = ms;
				}
				deleteCommand(MENUINDEX_MACRO, IDM_SETTING_SHORTCUT_MAPPER_MACRO, nppParam.getMacroMenuItems(), shortcutIndex, nbElem);
			}
			break;

			case STATE_USER:
			{
				vector<UserCommand> & theUserCmds = nppParam.getUserCommandList();
				theUserCmds.erase(theUserCmds.begin() + shortcutIndex);

				saveCurrentView();
				fillOutGrid();

				size_t nbElem = theUserCmds.size();
				for (size_t i = shortcutIndex; i < nbElem; ++i)	//lower the IDs of the remaining items so there are no gaps
				{
					UserCommand uc = theUserCmds[i];
					uc.setID(uc.getID() - 1);	//shift all IDs
					theUserCmds[i] = uc;
				}
				deleteCommand(MENUINDEX_RUN, IDM_SETTING_SHORTCUT_MAPPER_RUN, nppParam.getRunMenuItems(), shortcutIndex, nbElem);
			}
			break;
		}

		// updateShortcuts() will update all menu item - the menu items will be shifted
		nppParam.getAccelerator()->updateShortcuts();
		nppParam.setShortcutDirty();
	}
}

void ShortcutMapper::showContextMenu(const QPoint& position)
{
	const int row = _grid->rowAt(position.y());
	if (row >= 0)
		_grid->setCurrentCell(row, 0);

	NativeLangSpeaker* nativeLangSpeaker = NppParameters::getInstance().getNativeLangSpeaker();
	const bool hasRows = _grid->rowCount() > 0;

	QMenu menu;
	QAction* modify = menu.addAction(QString::fromStdWString(nativeLangSpeaker->getShortcutMapperLangStr("ModifyContextMenu", L"Modify")));
	QAction* remove = menu.addAction(QString::fromStdWString(nativeLangSpeaker->getShortcutMapperLangStr("DeleteContextMenu", L"Delete")));
	QAction* clear = menu.addAction(QString::fromStdWString(nativeLangSpeaker->getShortcutMapperLangStr("ClearContextMenu", L"Clear")));
	modify->setEnabled(hasRows);
	remove->setEnabled(hasRows && (_currentState == STATE_MACRO || _currentState == STATE_USER));
	clear->setEnabled(hasRows && _currentState != STATE_SCINTILLA);

	const QAction* chosen = menu.exec(_grid->viewport()->mapToGlobal(position));
	if (chosen == modify)
		onModify();
	else if (chosen == remove)
		onDelete();
	else if (chosen == clear)
		onClear();
}

bool ShortcutMapper::findKeyConflicts(wstring * const keyConflictLocation,
										const KeyCombo & itemKeyComboToTest, const size_t & itemIndexToTest) const
{
	if (itemKeyComboToTest._key == 0) //no key assignment
		return false;

	bool retIsConflict = false; //returns true when a conflict is found
	NppParameters& nppParam = NppParameters::getInstance();

	for (size_t gridState = STATE_MENU; gridState <= STATE_SCINTILLA; ++gridState)
	{
		switch (gridState)
		{
			case STATE_MENU:
			{
				vector<CommandShortcut> & vShortcuts = nppParam.getUserShortcuts();
				size_t nbItems = vShortcuts.size();
				for (size_t itemIndex = 0; itemIndex < nbItems; ++itemIndex)
				{
					if (!vShortcuts[itemIndex].isEnabled()) //no key assignment
						continue;

					if ((itemIndex == itemIndexToTest) && (gridState == static_cast<size_t>(_currentState))) //don't catch oneself
						continue;

					if (isConflict(vShortcuts[itemIndex].getKeyCombo(), itemKeyComboToTest))
					{
						retIsConflict = true;
						if (keyConflictLocation == nullptr)
							return retIsConflict;
						else
						{
							if (!keyConflictLocation->empty())
								*keyConflictLocation += L"\r\n";
							*keyConflictLocation += _tabNames[gridState];
							*keyConflictLocation += L"  |  ";
							*keyConflictLocation += std::to_wstring(itemIndex + 1);
							*keyConflictLocation += L"   ";
							*keyConflictLocation += string2wstring(vShortcuts[itemIndex].getName(), CP_UTF8);
							*keyConflictLocation += L"  ( ";
							*keyConflictLocation += string2wstring(vShortcuts[itemIndex].toString(), CP_UTF8);
							*keyConflictLocation += L" )";
						}
					}
				}
				break;
			} //case STATE_MENU
			case STATE_MACRO:
			{
				vector<MacroShortcut> & vShortcuts = nppParam.getMacroList();
				size_t nbItems = vShortcuts.size();
				for (size_t itemIndex = 0; itemIndex < nbItems; ++itemIndex)
				{
					if (!vShortcuts[itemIndex].isEnabled()) //no key assignment
						continue;

					if ((itemIndex == itemIndexToTest) && (gridState == static_cast<size_t>(_currentState))) //don't catch oneself
						continue;

					if (isConflict(vShortcuts[itemIndex].getKeyCombo(), itemKeyComboToTest))
					{
						retIsConflict = true;
						if (keyConflictLocation == nullptr)
							return retIsConflict;
						else
						{
							if (!keyConflictLocation->empty())
								*keyConflictLocation += L"\r\n";
							*keyConflictLocation += _tabNames[gridState];
							*keyConflictLocation += L"  |  ";
							*keyConflictLocation += std::to_wstring(itemIndex + 1);
							*keyConflictLocation += L"   ";
							*keyConflictLocation += string2wstring(vShortcuts[itemIndex].getName(), CP_UTF8);
							*keyConflictLocation += L"  ( ";
							*keyConflictLocation += string2wstring(vShortcuts[itemIndex].toString(), CP_UTF8);
							*keyConflictLocation += L" )";
						}
					}
				}
				break;
			} //case STATE_MACRO
			case STATE_USER:
			{
				vector<UserCommand> & vShortcuts = nppParam.getUserCommandList();
				size_t nbItems = vShortcuts.size();
				for (size_t itemIndex = 0; itemIndex < nbItems; ++itemIndex)
				{
					if (!vShortcuts[itemIndex].isEnabled()) //no key assignment
						continue;

					if ((itemIndex == itemIndexToTest) && (gridState == static_cast<size_t>(_currentState))) //don't catch oneself
						continue;

					if (isConflict(vShortcuts[itemIndex].getKeyCombo(), itemKeyComboToTest))
					{
						retIsConflict = true;
						if (keyConflictLocation == nullptr)
							return retIsConflict;
						else
						{
							if (!keyConflictLocation->empty())
								*keyConflictLocation += L"\r\n";
							*keyConflictLocation += _tabNames[gridState];
							*keyConflictLocation += L"  |  ";
							*keyConflictLocation += std::to_wstring(itemIndex + 1);
							*keyConflictLocation += L"   ";
							*keyConflictLocation += string2wstring(vShortcuts[itemIndex].getName(), CP_UTF8);
							*keyConflictLocation += L"  ( ";
							*keyConflictLocation += string2wstring(vShortcuts[itemIndex].toString(), CP_UTF8);
							*keyConflictLocation += L" )";
						}
					}
				}
				break;
			} //case STATE_USER
			case STATE_PLUGIN:
			{
				vector<PluginCmdShortcut> & vShortcuts = nppParam.getPluginCommandList();
				size_t nbItems = vShortcuts.size();
				for (size_t itemIndex = 0; itemIndex < nbItems; ++itemIndex)
				{
					if (!vShortcuts[itemIndex].isEnabled()) //no key assignment
						continue;

					if ((itemIndex == itemIndexToTest) && (gridState == static_cast<size_t>(_currentState))) //don't catch oneself
						continue;

					if (isConflict(vShortcuts[itemIndex].getKeyCombo(), itemKeyComboToTest))
					{
						retIsConflict = true;
						if (keyConflictLocation == nullptr)
							return retIsConflict;
						else
						{
							if (!keyConflictLocation->empty())
								*keyConflictLocation += L"\r\n";
							*keyConflictLocation += _tabNames[gridState];
							*keyConflictLocation += L"  |  ";
							*keyConflictLocation += std::to_wstring(itemIndex + 1);
							*keyConflictLocation += L"   ";
							*keyConflictLocation += string2wstring(vShortcuts[itemIndex].getName(), CP_UTF8);
							*keyConflictLocation += L"  ( ";
							*keyConflictLocation += string2wstring(vShortcuts[itemIndex].toString(), CP_UTF8);
							*keyConflictLocation += L" )";
						}
					}
				}
				break;
			} //case STATE_PLUGIN
			case STATE_SCINTILLA:
			{
				vector<ScintillaKeyMap> & vShortcuts = nppParam.getScintillaKeyList();
				size_t nbItems = vShortcuts.size();
				for (size_t itemIndex = 0; itemIndex < nbItems; ++itemIndex)
				{
					if (!vShortcuts[itemIndex].isEnabled()) //no key assignment
						continue;

					if ((itemIndex == itemIndexToTest) && (gridState == static_cast<size_t>(_currentState))) //don't catch oneself
						continue;

					size_t sciCombos = vShortcuts[itemIndex].getSize();
					for (size_t sciIndex = 0; sciIndex < sciCombos; ++sciIndex)
					{
						if (isConflict(vShortcuts[itemIndex].getKeyComboByIndex(sciIndex), itemKeyComboToTest))
						{
							retIsConflict = true;
							if (keyConflictLocation == nullptr)
								return retIsConflict;
							else
							{
								if (!keyConflictLocation->empty())
									*keyConflictLocation += L"\r\n";
								*keyConflictLocation += _tabNames[gridState];
								*keyConflictLocation += L"  |  ";
								*keyConflictLocation += std::to_wstring(itemIndex + 1);
								if (sciIndex > 0)
									*keyConflictLocation += L"*   ";
								else
									*keyConflictLocation += L"   ";
								*keyConflictLocation += string2wstring(vShortcuts[itemIndex].getName(), CP_UTF8);
								*keyConflictLocation += L"  ( ";
								*keyConflictLocation += string2wstring(vShortcuts[itemIndex].toString(sciIndex), CP_UTF8);
								*keyConflictLocation += L" )";
							}
						}
					}
				}
				break;
			}
		}
	}
	return retIsConflict;
}
