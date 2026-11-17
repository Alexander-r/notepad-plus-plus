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


#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>

#include <memory>
#include <algorithm>
#include <array>
#include "shortcut.h"
#include "Parameters.h"
#include "ScintillaEditView.h"
#include "resource.h"
#include "Notepad_plus_Window.h"
#include "NppMenu.h"
#include "keys.h"
#include "ui_ShortcutDlg.h"
#include "ui_ShortcutscintDlg.h"

using namespace std;

struct KeyIDNAME {
	const char * name = nullptr;
	UCHAR id = 0;
};

KeyIDNAME namedKeyArray[] = {
{"None", VK_NULL},

{"Backspace", VK_BACK},
{"Tab", VK_TAB},
{"Enter", VK_RETURN},
{"Esc", VK_ESCAPE},
{"Spacebar", VK_SPACE},

{"Page up", VK_PRIOR},
{"Page down", VK_NEXT},
{"End", VK_END},
{"Home", VK_HOME},
{"Left", VK_LEFT},
{"Up", VK_UP},
{"Right", VK_RIGHT},
{"Down", VK_DOWN},

{"INS", VK_INSERT},
{"DEL", VK_DELETE},

{"0", VK_0},
{"1", VK_1},
{"2", VK_2},
{"3", VK_3},
{"4", VK_4},
{"5", VK_5},
{"6", VK_6},
{"7", VK_7},
{"8", VK_8},
{"9", VK_9},
{"A", VK_A},
{"B", VK_B},
{"C", VK_C},
{"D", VK_D},
{"E", VK_E},
{"F", VK_F},
{"G", VK_G},
{"H", VK_H},
{"I", VK_I},
{"J", VK_J},
{"K", VK_K},
{"L", VK_L},
{"M", VK_M},
{"N", VK_N},
{"O", VK_O},
{"P", VK_P},
{"Q", VK_Q},
{"R", VK_R},
{"S", VK_S},
{"T", VK_T},
{"U", VK_U},
{"V", VK_V},
{"W", VK_W},
{"X", VK_X},
{"Y", VK_Y},
{"Z", VK_Z},

{"Numpad 0", VK_NUMPAD0},
{"Numpad 1", VK_NUMPAD1},
{"Numpad 2", VK_NUMPAD2},
{"Numpad 3", VK_NUMPAD3},
{"Numpad 4", VK_NUMPAD4},
{"Numpad 5", VK_NUMPAD5},
{"Numpad 6", VK_NUMPAD6},
{"Numpad 7", VK_NUMPAD7},
{"Numpad 8", VK_NUMPAD8},
{"Numpad 9", VK_NUMPAD9},
{"Num *", VK_MULTIPLY},
{"Num +", VK_ADD},
// {"Num Enter"), VK_SEPARATOR},	//this one doesn't seem to work: see below
{"Num -", VK_SUBTRACT},
{"Num .", VK_DECIMAL},
{"Num /", VK_DIVIDE},
{"F1", VK_F1},
{"F2", VK_F2},
{"F3", VK_F3},
{"F4", VK_F4},
{"F5", VK_F5},
{"F6", VK_F6},
{"F7", VK_F7},
{"F8", VK_F8},
{"F9", VK_F9},
{"F10", VK_F10},
{"F11", VK_F11},
{"F12", VK_F12},

{"F13", VK_F13},
{"F14", VK_F14},
{"F15", VK_F15},
{"F16", VK_F16},
{"F17", VK_F17},
{"F18", VK_F18},
{"F19", VK_F19},
{"F20", VK_F20},
{"F21", VK_F21},
{"F22", VK_F22},
{"F23", VK_F23},
{"F24", VK_F24},

{"~", VK_OEM_3},
{"-", VK_OEM_MINUS},
{"=", VK_OEM_PLUS},
{"[", VK_OEM_4},
{"]", VK_OEM_6},
{";", VK_OEM_1},
{"'", VK_OEM_7},
{"\\", VK_OEM_5},
{",", VK_OEM_COMMA},
{".", VK_OEM_PERIOD},
{"/", VK_OEM_2},

{"<>", VK_OEM_102},

// add placeholders for the VirtualKeys that aren't on the standard en-US keyboard
{"", VK_OEM_8},	// VK_OEM_8 not on US keyboards, despite being named
{"", VK_SEPARATOR}, // used for the ',' in "1,234,567.89", but it's not on most keyboards, so keep it in the custom section
{"", 0x88},		// RESERVED
{"", 0x89},		// RESERVED
{"", 0x8A},		// RESERVED
{"", 0x8B},		// RESERVED
{"", 0x8C},		// RESERVED
{"", 0x8D},		// RESERVED
{"", 0x8E},		// RESERVED
{"", 0x8F},		// RESERVED
{"", 0x92},		// OEM SPECIFIC
{"", 0x93},		// OEM SPECIFIC
{"", 0x94},		// OEM SPECIFIC
{"", 0x95},		// OEM SPECIFIC
{"", 0x96},		// OEM SPECIFIC
{"", 0x97},		// UNASSIGNED
{"", 0x98},		// UNASSIGNED
{"", 0x99},		// UNASSIGNED
{"", 0x9A},		// UNASSIGNED
{"", 0x9B},		// UNASSIGNED
{"", 0x9C},		// UNASSIGNED
{"", 0x9D},		// UNASSIGNED
{"", 0x9E},		// UNASSIGNED
{"", 0x9F},		// UNASSIGNED
{"", 0xB8},		// RESERVED
{"", 0xB9},		// RESERVED
{"", 0xC1},		// RESERVED: ABNT_C1
{"", 0xC2},		// RESERVED: ABNT_C2
{"", 0xC3},		// RESERVED
{"", 0xC4},		// RESERVED
{"", 0xC5},		// RESERVED
{"", 0xC6},		// RESERVED
{"", 0xC7},		// RESERVED
{"", 0xC8},		// RESERVED
{"", 0xC9},		// RESERVED
{"", 0xCA},		// RESERVED
{"", 0xCB},		// RESERVED
{"", 0xCC},		// RESERVED
{"", 0xCD},		// RESERVED
{"", 0xCE},		// RESERVED
{"", 0xCF},		// RESERVED
{"", 0xD0},		// RESERVED
{"", 0xD1},		// RESERVED
{"", 0xD2},		// RESERVED
{"", 0xD3},		// RESERVED
{"", 0xD4},		// RESERVED
{"", 0xD5},		// RESERVED
{"", 0xD6},		// RESERVED
{"", 0xD7},		// RESERVED
{"", 0xD8},		// RESERVED
{"", 0xD9},		// RESERVED
{"", 0xDA},		// RESERVED
{"", 0xE0},		// RESERVED
{"", 0xE1},		// OEM SPECIFIC
{"", 0xE3},		// OEM SPECIFIC
{"", 0xE4},		// OEM SPECIFIC
{"", 0xE6},		// OEM SPECIFIC
{"", 0xE8},		// UNASSIGNED
{"", 0xE9},		// OEM SPECIFIC
{"", 0xEA},		// OEM SPECIFIC
{"", 0xEB},		// OEM SPECIFIC
{"", 0xEC},		// OEM SPECIFIC
{"", 0xED},		// OEM SPECIFIC
{"", 0xEE},		// OEM SPECIFIC
{"", 0xEF},		// OEM SPECIFIC
{"", 0xF0},		// OEM SPECIFIC
{"", 0xF1},		// OEM SPECIFIC
{"", 0xF2},		// OEM SPECIFIC
{"", 0xF3},		// OEM SPECIFIC
{"", 0xF4},		// OEM SPECIFIC
{"", 0xF5},		// OEM SPECIFIC
};

#define nbKeys sizeof(namedKeyArray)/sizeof(KeyIDNAME)


string Shortcut::toString() const
{
	string sc;
	if (!isEnabled())
		return sc;

	if (_keyCombo._isCtrl)
		sc += "Ctrl+";
	if (_keyCombo._isAlt)
		sc += "Alt+";
	if (_keyCombo._isShift)
		sc += "Shift+";

	string keyString;
	getKeyStrFromVal(_keyCombo._key, keyString);
	sc += keyString;
	return sc;
}

void Shortcut::setName(const char* menuName, const char* shortcutName)
{
	_menuName = std::string(menuName).substr(0, menuItemStrLenMax - 1);
	const std::string source = shortcutName ? shortcutName : menuName;

	_name.clear();
	for (size_t j = 0; j < source.size() && _name.size() < (menuItemStrLenMax - 1); ++j)
	{
		if (source[j] != '&')
		{
			_name += source[j];
		}
		else if (j + 1 < source.size() && source[j + 1] == '&') //check if this ampersand is being escaped
		{
			_name += source[j];
			++j;	//skip escaped ampersand
		}
	}
}

string ScintillaKeyMap::toString() const
{
	string sc;
	size_t nbCombos = getSize();
	for (size_t combo = 0; combo < nbCombos; ++combo)
	{
		sc += toString(combo);
		if (combo < nbCombos - 1)
			sc += " / ";
	}
	return sc;
}

string ScintillaKeyMap::toString(size_t index) const
{
	string sc = "";
	if (!isEnabled())
		return sc;

	KeyCombo kc = _keyCombos[index];
	if (kc._isCtrl)
		sc += "Ctrl+";
	if (kc._isAlt)
		sc += "Alt+";
	if (kc._isShift)
		sc += "Shift+";

	string keyString;
	getKeyStrFromVal(kc._key, keyString);
	sc += keyString;
	return sc;
}

KeyCombo ScintillaKeyMap::getKeyComboByIndex(size_t index) const
{
	return _keyCombos[index];
}

void ScintillaKeyMap::setKeyComboByIndex(int index, KeyCombo combo)
{
	if (combo._key == 0 && (_size > 1))
	{	//remove the item if possible
		_keyCombos.erase(_keyCombos.begin() + index);
	}
	_keyCombos[index] = combo;
}

void ScintillaKeyMap::removeKeyComboByIndex(size_t index)
{
	if (_size > 1 && index < _size)
	{
		_keyCombos.erase(_keyCombos.begin() + index);
		_size--;
	}
}

int ScintillaKeyMap::addKeyCombo(KeyCombo combo)
{	//returns index where key is added, or -1 when invalid
	if (combo._key == 0)	//do not allow to add disabled keycombos
		return -1;
	if (!isEnabled())
	{	//disabled, override current combo with new enabled one
		_keyCombos[0] = combo;
		return 0;
	}

	for (size_t i = 0; i < _size; ++i)
	{	//if already in the list do not add it
		const KeyCombo& kc = _keyCombos[i];
		if (combo._key == kc._key && combo._isCtrl == kc._isCtrl && combo._isAlt == kc._isAlt && combo._isShift == kc._isShift)
			return static_cast<int32_t>(i);	//already in the list
	}
	_keyCombos.push_back(combo);
	++_size;
	return static_cast<int32_t>(_size - 1);
}

bool ScintillaKeyMap::isEnabled() const
{
	return (_keyCombos[0]._key != 0);
}

size_t ScintillaKeyMap::getSize() const
{
	return _size;
}

void getKeyStrFromVal(UCHAR keyVal, string & str)
{
	str = "";
	bool found = false;
	size_t i;
	for (i = 0; i < nbKeys; ++i)
	{
		if (keyVal == namedKeyArray[i].id)
		{
			found = true;
			break;
		}
	}
	if (found) 
	{
		// by default, assume it will just use the original name
		str = namedKeyArray[i].name;
		
		// but if that key is mapped on this keyboard, then use the mapped name (if the name is empty, it is not on the active keyboard)
	}
	else 
		str = "Unlisted";
}

void getNameStrFromCmd(DWORD cmd, wstring & str)
{
	if ((cmd >= ID_MACRO) && (cmd < ID_MACRO_LIMIT))
	{
		const vector<MacroShortcut> & theMacros = (NppParameters::getInstance()).getMacroList();
		int i = cmd - ID_MACRO;
		str = string2wstring(theMacros[i].getName(), CP_UTF8);
	}
	else if ((cmd >= ID_USER_CMD) && (cmd < ID_USER_CMD_LIMIT))
	{
		const vector<UserCommand> & userCommands = (NppParameters::getInstance()).getUserCommandList();
		int i = cmd - ID_USER_CMD;
		str = string2wstring(userCommands[i].getName(), CP_UTF8);
	}
	else if ((cmd >= ID_PLUGINS_CMD) && (cmd < ID_PLUGINS_CMD_LIMIT))
	{
		const vector<PluginCmdShortcut> & pluginCmds = (NppParameters::getInstance()).getPluginCommandList();
		size_t i = 0;
		for (size_t j = 0, len = pluginCmds.size(); j < len ; ++j)
		{
			if (pluginCmds[j].getID() == cmd)
			{
				i = j;
				break;
			}
		}
		str = string2wstring(pluginCmds[i].getName(), CP_UTF8);
	}
	else
	{
		HMENU m = reinterpret_cast<HMENU>(::SendMessage(Notepad_plus_Window::gNppHWND, NPPM_INTERNAL_GETMENU, 0, 0));
		const std::wstring menuText = nppMenuCommandText(m, cmd);
		if (menuText.empty())
			return;
		const wchar_t* cmdName = menuText.c_str();
		str.clear();
		for (const wchar_t* p = cmdName; *p && *p != L'\t'; ++p)
		{
			if (*p != L'&')
				str += *p;
		}
	}
	return;
}

bool Shortcut::hasKeyConflict() const
{
	bool isConflict = false;
	::SendMessage(_hParent, NPPM_INTERNAL_FINDKEYCONFLICTS, reinterpret_cast<WPARAM>(&_keyCombo), reinterpret_cast<LPARAM>(&isConflict));
	return isConflict;
}

void Shortcut::notifyEditingEnded() const
{
	// Clean up message for detached dialogs: save Macros/RunCommands
	::SendMessage(_hParent, NPPM_INTERNAL_FINDKEYCONFLICTS, 0, 0);
}

namespace
{
	// Adds the named keys to the combo box, and returns the key code of each entry
	std::vector<UCHAR> fillKeyCombo(QComboBox* combo)
	{
		std::vector<UCHAR> keyIds;
		for (const KeyIDNAME& key : namedKeyArray)
		{
			if (key.name[0])
			{
				combo->addItem(QString::fromUtf8(key.name));
				keyIds.push_back(key.id);
			}
		}
		return keyIds;
	}

	void selectKey(QComboBox* combo, const std::vector<UCHAR>& keyIds, UCHAR key)
	{
		const auto found = std::find(keyIds.begin(), keyIds.end(), key);
		if (found != keyIds.end())
			combo->setCurrentIndex(static_cast<int>(found - keyIds.begin()));
	}

	void centerOn(QDialog& dialog, const QWidget* parent)
	{
		if (parent)
			dialog.move(parent->mapToGlobal(parent->rect().center()) - dialog.rect().center());
	}
}

int Shortcut::doDialog()
{
	QDialog dialog(_hParent);
	StaticDialog::applyDialogFont(&dialog);
	Ui::ShortcutDlg ui;
	ui.setupUi(&dialog);

	const auto hasValidName = [&]()
	{
		return !ui.nameEdit->text().isEmpty() || !_canModifyName;
	};
	const auto refreshOkButton = [&]()
	{
		ui.ok->setEnabled(isValid() && hasValidName());
	};
	const auto refreshConflict = [&]()
	{
		ui.conflictStatic->setVisible(hasKeyConflict());
	};

	ui.nameEdit->setText(QString::fromUtf8(_canModifyName ? getMenuName() : getName()));
	ui.nameEdit->setReadOnly(!_canModifyName);
	ui.ctrlCheck->setChecked(_keyCombo._isCtrl);
	ui.altCheck->setChecked(_keyCombo._isAlt);
	ui.shiftCheck->setChecked(_keyCombo._isShift);

	const std::vector<UCHAR> keyIds = fillKeyCombo(ui.keyCombo);
	selectKey(ui.keyCombo, keyIds, _keyCombo._key);

	ui.warningStatic->hide(); // Hide this warning on startup
	refreshOkButton();
	refreshConflict();

	QObject::connect(ui.ctrlCheck, &QCheckBox::clicked, &dialog, [&](bool checked)
	{
		_keyCombo._isCtrl = checked;
		refreshOkButton();
		refreshConflict();
	});
	QObject::connect(ui.altCheck, &QCheckBox::clicked, &dialog, [&](bool checked)
	{
		_keyCombo._isAlt = checked;
		refreshOkButton();
		refreshConflict();
	});
	QObject::connect(ui.shiftCheck, &QCheckBox::clicked, &dialog, [&](bool checked)
	{
		_keyCombo._isShift = checked;
		refreshConflict();
	});
	QObject::connect(ui.nameEdit, &QLineEdit::textChanged, &dialog, [&]() { refreshOkButton(); });
	QObject::connect(ui.keyCombo, &QComboBox::activated, &dialog, [&](int index)
	{
		_keyCombo._key = keyIds[index];
		refreshOkButton();
		ui.warningStatic->setVisible(!isEnabled());
		refreshConflict();
	});
	QObject::connect(ui.ok, &QPushButton::clicked, &dialog, &QDialog::accept);
	QObject::connect(ui.cancel, &QPushButton::clicked, &dialog, &QDialog::reject);

	NppParameters::getInstance().getNativeLangSpeaker()->changeDlgLang(&dialog, "ShortcutMapperSubDialg");
	centerOn(dialog, _hParent);

	const int result = dialog.exec();
	if (result == QDialog::Accepted)
	{
		if (!isEnabled())
			_keyCombo._isCtrl = _keyCombo._isAlt = _keyCombo._isShift = false;
		if (_canModifyName)
			setName(ui.nameEdit->text().toUtf8().constData());
	}
	notifyEditingEnded();
	return (result == QDialog::Accepted) ? 0 : -1;
}

// return true if one of CommandShortcuts is deleted. Otherwise false.
void Accelerator::updateShortcuts()
{
	for (QAction* action : _hiddenActions)
		action->deleteLater();
	_hiddenActions.clear();

	updateFullMenu();

	// Fixed shortcuts for switching tabs, independent of the configuration
	const auto addHidden = [this](int cmdID, const QKeySequence& keys)
	{
		auto* action = new QAction(_hMenuParent);
		action->setShortcut(keys);
		action->setShortcutContext(Qt::ApplicationShortcut);
		const HWND target = _hMenuParent;
		QObject::connect(action, &QAction::triggered, action, [target, cmdID]()
		{
			::SendMessage(target, WM_COMMAND, MAKEWPARAM(static_cast<WORD>(cmdID), 0), 0);
		});
		_hMenuParent->addAction(action);
		_hiddenActions.push_back(action);
	};
	addHidden(IDC_NEXT_TAB, QKeySequence(Qt::CTRL | Qt::Key_Tab));
	addHidden(IDC_PREV_TAB, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab));
}

void Accelerator::updateFullMenu()
{
	// The recent files are added before the window is known at the start, and init() does all of this anyway
	if (!_hMenuParent)
		return;

	NppParameters& nppParam = NppParameters::getInstance();

	for (const CommandShortcut& command : nppParam.getUserShortcuts())
		applyCommand(command);

	for (const MacroShortcut& macro : nppParam.getMacroList())
		applyCommand(macro);

	for (const UserCommand& command : nppParam.getUserCommandList())
		applyCommand(command);

	for (const PluginCmdShortcut& command : nppParam.getPluginCommandList())
		applyCommand(command);
}

void Accelerator::applyCommand(const CommandShortcut& csc)
{
	const int cmdID = csc.getID();
	const QKeySequence keys = csc.toKeySequence();

	// These keep working while the Find or Incremental Find dialog has the focus, for which the original
	// has separate accelerator tables. The others work from the main window only.
	const bool isSearchCommand = cmdID == IDM_SEARCH_FIND || cmdID == IDM_SEARCH_REPLACE || cmdID == IDM_SEARCH_FINDINFILES || cmdID == IDM_SEARCH_MARK ||
		cmdID == IDM_SEARCH_FINDNEXT || cmdID == IDM_SEARCH_FINDPREV || cmdID == IDM_SEARCH_FINDINCREMENT;
	const Qt::ShortcutContext context = isSearchCommand ? Qt::ApplicationShortcut : Qt::WindowShortcut;

	if (QAction* action = nppFindMenuAction(_hAccelMenu, cmdID))
	{
		action->setShortcut(keys);
		action->setShortcutContext(context);
		_hMenuParent->addAction(action); // a hidden menu bar would otherwise take its shortcuts along
		return;
	}

	if (keys.isEmpty())
		return;

	auto* action = new QAction(_hMenuParent);
	action->setShortcut(keys);
	action->setShortcutContext(context);
	const HWND target = _hMenuParent;
	QObject::connect(action, &QAction::triggered, action, [target, cmdID]()
	{
		::SendMessage(target, WM_COMMAND, MAKEWPARAM(static_cast<WORD>(cmdID), 0), 0);
	});
	_hMenuParent->addAction(action);
	_hiddenActions.push_back(action);
}

QKeySequence Shortcut::toKeySequence() const
{
	const UCHAR vk = _keyCombo._key;
	if (vk == 0)
		return {};

	int key = 0;
	Qt::KeyboardModifiers modifiers;
	if (_keyCombo._isCtrl)
		modifiers |= Qt::ControlModifier;
	if (_keyCombo._isAlt)
		modifiers |= Qt::AltModifier;
	if (_keyCombo._isShift)
		modifiers |= Qt::ShiftModifier;

	if ((vk >= VK_A && vk <= VK_Z) || (vk >= VK_0 && vk <= VK_9))
		key = vk; // Qt::Key_A and Qt::Key_0 use the same numbers
	else if (vk >= VK_F1 && vk <= VK_F24)
		key = Qt::Key_F1 + (vk - VK_F1);
	else if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
	{
		key = Qt::Key_0 + (vk - VK_NUMPAD0);
		modifiers |= Qt::KeypadModifier;
	}
	else
	{
		switch (vk)
		{
			case VK_BACK: key = Qt::Key_Backspace; break;
			case VK_TAB: key = Qt::Key_Tab; break;
			case VK_RETURN: key = Qt::Key_Return; break;
			case VK_ESCAPE: key = Qt::Key_Escape; break;
			case VK_SPACE: key = Qt::Key_Space; break;
			case VK_PRIOR: key = Qt::Key_PageUp; break;
			case VK_NEXT: key = Qt::Key_PageDown; break;
			case VK_END: key = Qt::Key_End; break;
			case VK_HOME: key = Qt::Key_Home; break;
			case VK_LEFT: key = Qt::Key_Left; break;
			case VK_UP: key = Qt::Key_Up; break;
			case VK_RIGHT: key = Qt::Key_Right; break;
			case VK_DOWN: key = Qt::Key_Down; break;
			case VK_INSERT: key = Qt::Key_Insert; break;
			case VK_DELETE: key = Qt::Key_Delete; break;
			case VK_MULTIPLY: key = Qt::Key_Asterisk; modifiers |= Qt::KeypadModifier; break;
			case VK_ADD: key = Qt::Key_Plus; modifiers |= Qt::KeypadModifier; break;
			case VK_SUBTRACT: key = Qt::Key_Minus; modifiers |= Qt::KeypadModifier; break;
			case VK_DECIMAL: key = Qt::Key_Period; modifiers |= Qt::KeypadModifier; break;
			case VK_DIVIDE: key = Qt::Key_Slash; modifiers |= Qt::KeypadModifier; break;
			// US layout names of the punctuation keys, as namedKeyArray shows them
			case VK_OEM_1: key = Qt::Key_Semicolon; break;
			case VK_OEM_PLUS: key = Qt::Key_Equal; break;
			case VK_OEM_COMMA: key = Qt::Key_Comma; break;
			case VK_OEM_MINUS: key = Qt::Key_Minus; break;
			case VK_OEM_PERIOD: key = Qt::Key_Period; break;
			case VK_OEM_2: key = Qt::Key_Slash; break;
			case VK_OEM_3: key = Qt::Key_QuoteLeft; break;
			case VK_OEM_4: key = Qt::Key_BracketLeft; break;
			case VK_OEM_5: key = Qt::Key_Backslash; break;
			case VK_OEM_6: key = Qt::Key_BracketRight; break;
			case VK_OEM_7: key = Qt::Key_Apostrophe; break;
			default: return {};
		}
	}
	return QKeySequence(QKeyCombination(modifiers, static_cast<Qt::Key>(key)));
}

recordedMacroStep::recordedMacroStep(int iMessage, uptr_t wParam, uptr_t lParam)
	: _message(iMessage), _wParameter(wParam), _lParameter(lParam), _macroType(mtUseLParameter)
{ 
	if (_lParameter)
	{
		switch (_message)
		{
			case SCI_SETTEXT :
			case SCI_REPLACESEL :
			case SCI_REPLACETARGET :
			case SCI_REPLACETARGETRE :
			case SCI_SEARCHINTARGET :
			case SCI_ADDTEXT :
			case SCI_ADDSTYLEDTEXT :
			case SCI_INSERTTEXT :
			case SCI_APPENDTEXT :
			case SCI_SETWORDCHARS :
			case SCI_SETWHITESPACECHARS :
			case SCI_SETSTYLINGEX :
			case SCI_TEXTWIDTH :
			case SCI_STYLESETFONT :
			case SCI_SEARCHNEXT :
			case SCI_SEARCHPREV :
			case IDFINDWHAT:
			case IDREPLACEWITH:
			case IDD_FINDINFILES_DIR_COMBO:
			case IDD_FINDINFILES_FILTERS_COMBO:
			{
				char *ch = reinterpret_cast<char *>(_lParameter);
				_sParameter = ch;
				_macroType = mtUseSParameter;
				_lParameter = 0;
			}
			break;

				
			default : // for all other messages, use _lParameter "as is"
				break;
		}
	}
}

// code comes from Scintilla's Editor.cxx:
// void Editor::NotifyMacroRecord(unsigned int iMessage, uptr_t wParam, sptr_t lParam)
bool recordedMacroStep::isMacroable() const
{
	// Enumerates all macroable messages
	switch (_message)
	{
		case SCI_REPLACESEL: // (<unused>, const char *text)
		case SCI_ADDTEXT:    // (int length, const char *s)
		case SCI_INSERTTEXT: // (int pos, const char *text)
		case SCI_APPENDTEXT: // (int length, const char *s)
		case SCI_SEARCHNEXT: // (int searchFlags, const char *text)
		case SCI_SEARCHPREV: // (int searchFlags, const char *text)
		{
			if (_macroType == mtUseSParameter)
				return true;
			else
				return false;
		}

		case SCI_GOTOLINE:   // (int line)
		case SCI_GOTOPOS:    // (int position)
		case SCI_SETSELECTIONMODE:  // (int mode)
		case SCI_CUT:
		case SCI_COPY:
		case SCI_PASTE:
		case SCI_CLEAR:
		case SCI_CLEARALL:
		case SCI_SELECTALL:
		case SCI_SEARCHANCHOR:
		case SCI_LINEDOWN:
		case SCI_LINEDOWNEXTEND:
		case SCI_PARADOWN:
		case SCI_PARADOWNEXTEND:
		case SCI_LINEUP:
		case SCI_LINEUPEXTEND:
		case SCI_PARAUP:
		case SCI_PARAUPEXTEND:
		case SCI_CHARLEFT:
		case SCI_CHARLEFTEXTEND:
		case SCI_CHARRIGHT:
		case SCI_CHARRIGHTEXTEND:
		case SCI_WORDLEFT:
		case SCI_WORDLEFTEXTEND:
		case SCI_WORDRIGHT:
		case SCI_WORDRIGHTEXTEND:
		case SCI_WORDPARTLEFT:
		case SCI_WORDPARTLEFTEXTEND:
		case SCI_WORDPARTRIGHT:
		case SCI_WORDPARTRIGHTEXTEND:
		case SCI_WORDLEFTEND:
		case SCI_WORDLEFTENDEXTEND:
		case SCI_WORDRIGHTEND:
		case SCI_WORDRIGHTENDEXTEND:
		case SCI_HOME:
		case SCI_HOMEEXTEND:
		case SCI_LINEEND:
		case SCI_LINEENDEXTEND:
		case SCI_HOMEWRAP:
		case SCI_HOMEWRAPEXTEND:
		case SCI_LINEENDWRAP:
		case SCI_LINEENDWRAPEXTEND:
		case SCI_DOCUMENTSTART:
		case SCI_DOCUMENTSTARTEXTEND:
		case SCI_DOCUMENTEND:
		case SCI_DOCUMENTENDEXTEND:
		case SCI_STUTTEREDPAGEUP:
		case SCI_STUTTEREDPAGEUPEXTEND:
		case SCI_STUTTEREDPAGEDOWN:
		case SCI_STUTTEREDPAGEDOWNEXTEND:
		case SCI_PAGEUP:
		case SCI_PAGEUPEXTEND:
		case SCI_PAGEDOWN:
		case SCI_PAGEDOWNEXTEND:
		case SCI_EDITTOGGLEOVERTYPE:
		case SCI_CANCEL:
		case SCI_DELETEBACK:
		case SCI_TAB:
		case SCI_BACKTAB:
		case SCI_FORMFEED:
		case SCI_VCHOME:
		case SCI_VCHOMEEXTEND:
		case SCI_VCHOMEWRAP:
		case SCI_VCHOMEWRAPEXTEND:
		case SCI_VCHOMEDISPLAY:
		case SCI_VCHOMEDISPLAYEXTEND:
		case SCI_DELWORDLEFT:
		case SCI_DELWORDRIGHT:
		case SCI_DELWORDRIGHTEND:
		case SCI_DELLINELEFT:
		case SCI_DELLINERIGHT:
		case SCI_LINECOPY:
		case SCI_LINECUT:
		case SCI_LINEDELETE:
		case SCI_LINETRANSPOSE:
		case SCI_LINEDUPLICATE:
		case SCI_LOWERCASE:
		case SCI_UPPERCASE:
		case SCI_LINESCROLLDOWN:
		case SCI_LINESCROLLUP:
		case SCI_DELETEBACKNOTLINE:
		case SCI_HOMEDISPLAY:
		case SCI_HOMEDISPLAYEXTEND:
		case SCI_LINEENDDISPLAY:
		case SCI_LINEENDDISPLAYEXTEND:
		case SCI_LINEDOWNRECTEXTEND:
		case SCI_LINEUPRECTEXTEND:
		case SCI_CHARLEFTRECTEXTEND:
		case SCI_CHARRIGHTRECTEXTEND:
		case SCI_HOMERECTEXTEND:
		case SCI_VCHOMERECTEXTEND:
		case SCI_LINEENDRECTEXTEND:
		case SCI_PAGEUPRECTEXTEND:
		case SCI_PAGEDOWNRECTEXTEND:
		case SCI_SELECTIONDUPLICATE:
		case SCI_COPYALLOWLINE:
		case SCI_VERTICALCENTRECARET:
		case SCI_MOVESELECTEDLINESUP:
		case SCI_MOVESELECTEDLINESDOWN:
		case SCI_SCROLLTOSTART:
		case SCI_SCROLLTOEND:
		case SCI_SETVIRTUALSPACEOPTIONS:
		case SCI_SETCARETLINEBACKALPHA:
		case SCI_NEWLINE:
		{
			if (_macroType == mtUseLParameter)
				return true;
			else
				return false;
		}

		// Filter out all others like display changes.
		default:
			return false;
	}
}

void recordedMacroStep::PlayBack(Window* pNotepad, ScintillaEditView *pEditView)
{
	if (_macroType == mtMenuCommand)
	{
		::SendMessage(pNotepad->getHSelf(), WM_COMMAND, _wParameter, 0);
	}
	else
	{
		// Ensure it's macroable message before send it
		if (!isMacroable())
			return;

		if (_macroType == mtUseSParameter) 
		{
			/*
			int byteBufferLength = ::WideCharToMultiByte(static_cast<UINT>(pEditView->execute(SCI_GETCODEPAGE)), 0, _sParameter.c_str(), -1, NULL, 0, NULL, NULL);
			auto byteBuffer = std::make_unique< char[] >(byteBufferLength);
			::WideCharToMultiByte(static_cast<UINT>(pEditView->execute(SCI_GETCODEPAGE)), 0, _sParameter.c_str(), -1, byteBuffer.get(), byteBufferLength, NULL, NULL);
			auto lParam = reinterpret_cast<LPARAM>(byteBuffer.get());
			pEditView->execute(_message, _wParameter, lParam);
			*/
			pEditView->execute(_message, _wParameter, (LPARAM)_sParameter.c_str());
		}
		else
		{
			pEditView->execute(_message, _wParameter, _lParameter);
		}

		// If text content has been modified in Scintilla,
		// then notify Notepad++
		if ( (_message == SCI_SETTEXT)
			|| (_message == SCI_REPLACESEL) 
			|| (_message == SCI_ADDTEXT) 
			|| (_message == SCI_ADDSTYLEDTEXT) 
			|| (_message == SCI_INSERTTEXT) 
			|| (_message == SCI_APPENDTEXT) )
		{
			SCNotification scnN{};
			scnN.nmhdr.code = SCN_CHARADDED;
			scnN.nmhdr.hwndFrom = pEditView->getHSelf();
			scnN.nmhdr.idFrom = 0;
			if (_sParameter.empty())
				scnN.ch = 0;
			else
				scnN.ch = _sParameter.at(0);

			::SendMessage(pNotepad->getHSelf(), WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&scnN));
		}
	}
}

void ScintillaAccelerator::init(vector<HWND> * vScintillas, HMENU hMenu, HWND menuParent)
{
	_hAccelMenu = hMenu;
	_hMenuParent = menuParent;
	size_t nbScintilla = vScintillas->size();
	for (size_t i = 0; i < nbScintilla; ++i)
	{
		_vScintillas.push_back(vScintillas->at(i));
	}
}

void ScintillaAccelerator::updateKeys() 
{
	NppParameters& nppParam = NppParameters::getInstance();
	const vector<ScintillaKeyMap> & map = nppParam.getScintillaKeyList();
	size_t mapSize = map.size();
	size_t index;
	size_t nb = nbScintillas();
	for (size_t i = 0; i < nb; ++i)
	{
		::SendMessage(_vScintillas[i], SCI_CLEARALLCMDKEYS, 0, 0);
		for (int32_t j = static_cast<int32_t>(mapSize) - 1; j >= 0; j--) //reverse order, top of the list has highest priority
		{	
			ScintillaKeyMap skm = map[j];
			if (skm.isEnabled()) 
			{		//no validating, scintilla accepts more keys
				size_t size = skm.getSize();
				for (index = 0; index < size; ++index)
					::SendMessage(_vScintillas[i], SCI_ASSIGNCMDKEY, skm.toKeyDef(index), skm.getScintillaKeyID());
			}
			if (skm.getMenuCmdID() != 0) 
			{
				updateMenuItemByID(skm, skm.getMenuCmdID());
			}
			if (j == 0)	//j is unsigned, so default method doesn't work
				break;
		}
	}
}

void ScintillaAccelerator::updateMenuItemByID(const ScintillaKeyMap& skm, int id)
{
	QAction* action = nppFindMenuAction(_hAccelMenu, id);
	if (!action)
		return;

	QKeySequence keys;
	for (size_t i = 0; i < skm.getSize() && keys.isEmpty(); ++i)
	{
		const KeyCombo combo = skm.getKeyComboByIndex(i);
		keys = Shortcut(nullptr, combo._isCtrl, combo._isAlt, combo._isShift, combo._key).toKeySequence();
	}

	// As in the original's menu text, this only shows the key. Scintilla handles it, in whichever editor has the focus,
	// which includes those of the panels, while a shortcut of the window would always act on the document.
	action->setShortcut(keys);
	action->setShortcutContext(Qt::WidgetShortcut);
}

//This procedure uses _keyCombo as a temp. variable to store current settings which can then later be applied (by pressing OK)
int ScintillaKeyMap::doDialog()
{
	QDialog dialog(_hParent);
	StaticDialog::applyDialogFont(&dialog);
	Ui::ShortcutscintDlg ui;
	ui.setupUi(&dialog);

	const auto toQ = [](const std::string& text) { return QString::fromUtf8(text.c_str()); };

	ui.nameEdit->setText(toQ(_name));
	_keyCombo = _keyCombos[0];

	const std::vector<UCHAR> keyIds = fillKeyCombo(ui.keyCombo);

	const auto validateDialog = [&]()
	{
		const bool valid = isValid();	//current combo valid?
		const bool isDisabling = _keyCombo._key == 0;	//true if this keycombo were to disable the shortcut
		const bool isDisabled = !isEnabled();	//true if this shortcut already is
		const bool isDuplicate = std::any_of(_keyCombos.begin(), _keyCombos.begin() + _size, [this](const KeyCombo& k)
		{
			return _keyCombo._key == k._key && _keyCombo._isCtrl == k._isCtrl && _keyCombo._isAlt == k._isAlt && _keyCombo._isShift == k._isShift;
		});

		ui.buttonAdd->setEnabled(valid && !isDisabling && !isDuplicate);
		ui.buttonApply->setEnabled(valid && (!isDisabling || _size == 1) && !isDuplicate);
		ui.buttonRmve->setEnabled(_size > 1);
		ui.warningStatic->setVisible(isDisabled);
		ui.conflictStatic->setVisible(hasKeyConflict());
	};

	const auto showCurrentSettings = [&]()
	{
		_keyCombo = _keyCombos[ui.listKeys->currentRow()];
		ui.ctrlCheck->setChecked(_keyCombo._isCtrl);
		ui.altCheck->setChecked(_keyCombo._isAlt);
		ui.shiftCheck->setChecked(_keyCombo._isShift);
		selectKey(ui.keyCombo, keyIds, _keyCombo._key);
	};

	const auto setRowText = [&](int index)
	{
		ui.listKeys->item(index)->setText(toQ(toString(index)));
	};

	for (size_t i = 0; i < _size; ++i)
		ui.listKeys->addItem(toQ(toString(i)));
	ui.listKeys->setCurrentRow(0);

	showCurrentSettings();
	validateDialog();
	ui.warningStatic->hide(); // Hide this warning on startup

	const auto modifierHandler = [&](bool KeyCombo::* member)
	{
		return [&, member](bool checked)
		{
			_keyCombo.*member = checked;
			validateDialog();
		};
	};
	QObject::connect(ui.ctrlCheck, &QCheckBox::clicked, &dialog, modifierHandler(&KeyCombo::_isCtrl));
	QObject::connect(ui.altCheck, &QCheckBox::clicked, &dialog, modifierHandler(&KeyCombo::_isAlt));
	QObject::connect(ui.shiftCheck, &QCheckBox::clicked, &dialog, modifierHandler(&KeyCombo::_isShift));

	QObject::connect(ui.buttonAdd, &QPushButton::clicked, &dialog, [&]()
	{
		const size_t oldSize = _size;
		const int res = addKeyCombo(_keyCombo);
		if (res > -1)
		{
			if (res == static_cast<int>(oldSize))
				ui.listKeys->addItem(toQ(toString(res)));
			else //update current string, can happen if it was disabled
				setRowText(res);
			ui.listKeys->setCurrentRow(res);
		}
		showCurrentSettings();
		validateDialog();
	});

	QObject::connect(ui.buttonRmve, &QPushButton::clicked, &dialog, [&]()
	{
		if (_size == 1)	//cannot delete last shortcut
			return;
		int row = ui.listKeys->currentRow();
		removeKeyComboByIndex(row);
		{
			const QSignalBlocker blocker(ui.listKeys);
			delete ui.listKeys->takeItem(row);
		}
		if (static_cast<size_t>(row) == _size)
			row = static_cast<int>(_size) - 1;
		ui.listKeys->setCurrentRow(row);
		showCurrentSettings();
		validateDialog();
	});

	QObject::connect(ui.buttonApply, &QPushButton::clicked, &dialog, [&]()
	{
		const int row = ui.listKeys->currentRow();
		if (row >= 0)
		{
			setKeyComboByIndex(row, _keyCombo);
			setRowText(row);
		}
		validateDialog();
	});

	QObject::connect(ui.keyCombo, &QComboBox::activated, &dialog, [&](int index)
	{
		_keyCombo._key = keyIds[index];
		ui.warningStatic->setVisible(!isEnabled());
		validateDialog();
	});

	QObject::connect(ui.listKeys, &QListWidget::currentRowChanged, &dialog, [&](int row)
	{
		if (row >= 0)
		{
			showCurrentSettings();
			validateDialog();
		}
	});

	QObject::connect(ui.ok, &QPushButton::clicked, &dialog, &QDialog::accept);
	QObject::connect(ui.cancel, &QPushButton::clicked, &dialog, &QDialog::reject);

	NppParameters::getInstance().getNativeLangSpeaker()->changeDlgLang(&dialog, "ShortcutMapperSubDialg");
	centerOn(dialog, _hParent);

	const int result = dialog.exec();

	//Cleanup
	_keyCombo._key = 0;
	_keyCombo._isCtrl = _keyCombo._isAlt = _keyCombo._isShift = false;

	return (result == QDialog::Accepted) ? 0 : -1;
}

void CommandShortcut::setCategoryFromMenu(HMENU hMenu)
{
	NativeLangSpeaker* pNativeSpeaker = NppParameters::getInstance().getNativeLangSpeaker();

	if (_id >= IDM_WINDOW_WINDOWS && _id <= IDM_WINDOW_SORT_FS_DSC)
		pNativeSpeaker->getMainMenuEntryName(_category, hMenu, "Window", L"Window");
	else if ( _id >= IDM_VIEW_GOTO_ANOTHER_VIEW && _id <= IDM_VIEW_GOTO_END)
		pNativeSpeaker->getMainMenuEntryName(_category, hMenu, "view", L"View");
	else if (_id == IDM_EDIT_LTR || _id == IDM_EDIT_RTL)
		pNativeSpeaker->getMainMenuEntryName(_category, hMenu, "view", L"View");
	else if (_id == IDC_PREV_DOC || _id == IDC_NEXT_DOC)
		pNativeSpeaker->getMainMenuEntryName(_category, hMenu, "view", L"View");
	else if (_id == IDM_FORMAT_TODOS || _id == IDM_FORMAT_TOUNIX || _id == IDM_FORMAT_TOMAC)
		pNativeSpeaker->getMainMenuEntryName(_category, hMenu, "edit", L"Edit");
	else if (_id == IDM_EDIT_AUTOCOMPLETE || _id == IDM_EDIT_AUTOCOMPLETE_CURRENTFILE || _id == IDM_EDIT_FUNCCALLTIP ||
		_id == IDM_EDIT_AUTOCOMPLETE_PATH || _id == IDM_EDIT_FUNCCALLTIP_PREVIOUS || _id == IDM_EDIT_FUNCCALLTIP_NEXT)
		pNativeSpeaker->getMainMenuEntryName(_category, hMenu, "edit", L"Edit");
	else if (_id == IDM_LANGSTYLE_CONFIG_DLG)
		pNativeSpeaker->getMainMenuEntryName(_category, hMenu, "settings", L"Settings");
	else if (_id == IDM_MACRO_STARTRECORDINGMACRO  ||_id == IDM_MACRO_STOPRECORDINGMACRO  || _id == IDM_MACRO_RUNMULTIMACRODLG ||
		_id == IDM_MACRO_PLAYBACKRECORDEDMACRO  ||_id == IDM_MACRO_SAVECURRENTMACRO || _id == IDC_EDIT_TOGGLEMACRORECORDING)
		pNativeSpeaker->getMainMenuEntryName(_category, hMenu, "macro", L"Macro");


	else if ( _id < IDM_EDIT)
		pNativeSpeaker->getMainMenuEntryName(_category, hMenu, "file", L"File");
	else if ( _id < IDM_SEARCH)
		pNativeSpeaker->getMainMenuEntryName(_category, hMenu, "edit",L"Edit");
	else if ( _id < IDM_VIEW)
		pNativeSpeaker->getMainMenuEntryName(_category, hMenu, "search", L"Search");
	else if ( _id < IDM_FORMAT)
		pNativeSpeaker->getMainMenuEntryName(_category, hMenu, "view", L"View");
	else if ( _id < IDM_LANG)
		pNativeSpeaker->getMainMenuEntryName(_category, hMenu, "encoding", L"Encoding");
	else if ( _id < IDM_ABOUT)
		pNativeSpeaker->getMainMenuEntryName(_category, hMenu, "language", L"Language");
	else if ( _id < IDM_SETTING)
		pNativeSpeaker->getMainMenuEntryName(_category, hMenu, "about", L"About");
	else if ( _id < IDM_TOOL)
		pNativeSpeaker->getMainMenuEntryName(_category, hMenu, "settings", L"Settings");
	else if ( _id < IDM_EXECUTE)
		pNativeSpeaker->getMainMenuEntryName(_category, hMenu, "tools", L"Tools");
	else // _id >= IDM_EXECUTE
		pNativeSpeaker->getMainMenuEntryName(_category, hMenu, "run", L"Run");
}

CommandShortcut& CommandShortcut::operator = (const Shortcut& sct)
{
	if (this != &sct)
	{
		this->_name = sct.getName();
		this->_keyCombo = sct.getKeyCombo();
	}
	return *this;
}
