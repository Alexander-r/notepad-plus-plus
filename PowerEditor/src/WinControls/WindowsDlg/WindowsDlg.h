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

#include <QRect>

#include <vector>

#include "Common.h"
#include "NppMessages.h"
#include "StaticDialog.h"

class DocTabView;
class Buffer;
class QTableWidget;

enum WinDlgNotifyType {
	WDT_ACTIVATE = 1,
	WDT_SAVE = 2,
	WDT_CLOSE = 3,
	WDT_SORT = 4,
};

struct NMWINDLG : public NMHDR {
	BOOL processed = FALSE;
	WinDlgNotifyType type = WDT_ACTIVATE;
	UINT curSel = 0;
	UINT nItems = 0;
	UINT *Items = nullptr;
};

inline constexpr UINT WDN_NOTIFY = WM_USER + 7900;

class WindowsDlg : public StaticDialog
{
public :
	WindowsDlg() = default;
	int doDialog();
	using StaticDialog::init;
	virtual void init(HINSTANCE hInst, HWND parent, DocTabView *pTab);
	void doSortToTabs();
	void doSort();
	void sort(int columnID, bool reverseSort);
	void sortFileNameASC();
	void sortFileNameDSC();
	void sortFilePathASC();
	void sortFilePathDSC();
	void sortFileTypeASC();
	void sortFileTypeDSC();
	void sortFileSizeASC();
	void sortFileSizeDSC();
	void sortDateTimeASC();
	void sortDateTimeDSC();
	void doRefresh();

protected :
	void onCancel() override;
	void updateColumnNames() const;
	void resetSelection();
	void doSave();
	void doClose();
	void updateButtonState();
	void activateCurrent();
	void doColumnSort();
	void doCount();
	void refreshMap();
	void refreshList() const;
	void putItemsToClipboard(bool isFullPath);
	void showListMenu(const QPoint& position);
	void onColumnClicked(int column);
	void onSortTabs();
	void onCharTyped(wchar_t ch);
	Buffer* getBuffer(int index) const;
	std::wstring cellText(int row, int column) const;

	// The selected rows of the list, from the top
	std::vector<int> selectedRows() const;
	void setRowSelected(int row, bool isSelected, bool isFocused = false) const;
	void finish(int result);

	QTableWidget* _hList = nullptr;
	static QRect _lastKnownLocation;
	DocTabView* _pTab = nullptr;
	std::vector<int> _idxMap;
	int _currentColumn = -1;
	int _lastSort = -1;
	bool _reverseSort = false;
};

class WindowsMenu
{
public:
	WindowsMenu() {}
	~WindowsMenu() {}
	void init(HMENU hMainMenu, HWND commandTarget);
	void initPopupMenu(HMENU hMenu, DocTabView *pTab);

private:
	HMENU _hMenu = nullptr;
	HMENU _hMenuList = nullptr;
	HWND _commandTarget = nullptr;
	UINT _limitPrev = 0;
};
