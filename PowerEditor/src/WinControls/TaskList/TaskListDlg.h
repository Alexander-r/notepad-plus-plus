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

#include <string>
#include <vector>

#include "NppMessages.h"
#include "StaticDialog.h"
#include "Window.h"

class IconList;
class QListWidget;

inline constexpr UINT TASKLIST_USER = WM_USER + 8000;
inline constexpr UINT WM_GETTASKLISTINFO = TASKLIST_USER + 01;

struct TaskLstFnStatus {
	int _iView = -1;
	int _docIndex = 0;
	std::wstring _fn;
	int _status = 0;
	void *_bufID = nullptr;
	int _docColor = -1;
	TaskLstFnStatus(const std::wstring& str, int status) : _fn(str), _status(status) {}
	TaskLstFnStatus(int iView, int docIndex, std::wstring str, int status, void* bufID, int docColor)
		: _iView(iView), _docIndex(docIndex), _fn(str), _status(status), _bufID(bufID), _docColor(docColor)
	{}
};

struct TaskListInfo {
	std::vector<TaskLstFnStatus> _tlfsLst;
	int _currentIndex = -1;
};

// The document switcher of Ctrl+Tab: a list that stays while Ctrl is held, and activates its selection when Ctrl goes up.
class TaskListDlg : public StaticDialog
{
public :
	TaskListDlg() : StaticDialog() { _instanceCount++; }
	~TaskListDlg() override { _instanceCount--; }
	using Window::init;
	void init(HINSTANCE hInst, HWND parent, const IconList* icons, bool dir) {
		Window::init(hInst, parent);
		_icons = icons;
		_initDir = dir;
	}
	int doDialog(bool isRTL = false);
	void destroy() override {}

	// For the key and mouse handling of the list
	void moveSelection(int direction);
	void pickCurrent();
	void dismiss();

protected :
	void onCancel() override { dismiss(); }

private :
	QListWidget* _list = nullptr;
	TaskListInfo _taskListInfo;
	const IconList* _icons = nullptr;
	bool _initDir = false;

	void selectRow(int row);
public:
	static int _instanceCount;
};
