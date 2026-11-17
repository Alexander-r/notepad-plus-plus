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


#pragma once

#include <QPoint>

#include <string>
#include <vector>

#include "ShortcutMapper_rc.h"
#include "StaticDialog.h"
#include "Window.h"
#include "shortcut.h"

class DynamicMenu;
class QTabBar;
class QTableWidget;

enum GridState {STATE_MENU, STATE_MACRO, STATE_USER, STATE_PLUGIN, STATE_SCINTILLA};

class ShortcutMapper : public StaticDialog {
public:
	~ShortcutMapper() override = default;

	void init(HINSTANCE hInst, HWND parent, GridState initState = STATE_MENU) {
		Window::init(hInst, parent);
		_currentState = initState;
	}

	void doDialog(bool isRTL = false);

	bool findKeyConflicts(std::wstring * const keyConflictLocation,
							const KeyCombo & itemKeyCombo, const size_t & itemIndex) const;

	bool isFilterValid(Shortcut sc);
	bool isFilterValid(PluginCmdShortcut sc);
	bool isFilterValid(ScintillaKeyMap sc);

protected:
	void onCancel() override;

private:
	QTableWidget* _grid = nullptr;
	QTabBar* _tabs = nullptr;

	GridState _currentState = STATE_MENU;

	static constexpr int _nbTab = 5;
	std::wstring _tabNames[_nbTab];
	std::vector<std::wstring> _shortcutFilter;
	std::vector<size_t> _shortcutIndex;

	//save/restore the last view
	std::vector<int> _lastHomeRow;
	std::vector<int> _lastCursorRow;

	std::wstring _conflictInfoOk;
	std::wstring _conflictInfoEditing;

	using Window::init;

	void initTabs();
	void initGrid();
	void fillOutGrid();
	void connectControls();
	void addRow(size_t index, const std::string& name, const std::string& shortcut, const std::wstring& extra, bool isConflict);
	void saveCurrentView();
	void onTabChanged(int index);
	void onRowChanged();
	void onClear();
	void onModify();
	void onDelete();
	void showContextMenu(const QPoint& position);
	LRESULT onMessage(UINT message, WPARAM wParam, LPARAM lParam);
	void deleteCommand(int menuIndex, int mapperCmd, DynamicMenu& menu, size_t shortcutIndex, size_t nbElem);
	std::wstring getTabString(size_t i) const;

	static bool isConflict(const KeyCombo& lhs, const KeyCombo& rhs)
	{
		return ( (lhs._isCtrl  == rhs._isCtrl ) &&
				 (lhs._isAlt   == rhs._isAlt  ) &&
				 (lhs._isShift == rhs._isShift) &&
				 (lhs._key	   == rhs._key	  ) );
	}
};
