// This file is part of Notepad++ project
// Copyright (C)2022 Don HO <don.h@free.fr>

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

// Qt port: written anew for Qt, in place of the Windows one with its StaticDialog and Window base classes. The panel
// is a QWidget that the plugin fills in createForm(), where the Windows one was a dialog from the resources. Notepad++
// puts it in a dock of its own and owns it from then on, so it is only watched here. Its messages, such as WM_NOTIFY
// with Notepad++'s docking notifications, come as NppMessageEvent to run_dlgProc(), as the original's did.

#ifndef DOCKINGDLGINTERFACE_H
#define DOCKINGDLGINTERFACE_H

#include "dockingResource.h"
#include "Docking.h"
#include "Notepad_plus_msgs.h"
#include <shlwapi.h>

#include <QEvent>
#include <QPointer>
#include <QWidget>

#include <string>


class DockingDlgInterface
{
public:
	DockingDlgInterface() = default;
	DockingDlgInterface(int dlgID): _dlgID(dlgID) {};
	virtual ~DockingDlgInterface() = default;

	virtual void init(HINSTANCE hInst, HWND parent)
	{
		_hInst = hInst;
		_hParent = parent;
		TCHAR modulePath[MAX_PATH]{};
		::GetModuleFileName((HMODULE)hInst, modulePath, MAX_PATH);
		_moduleName = PathFindFileName(modulePath);
	}

	void create(tTbData * data, bool isRTL = false)
	{
		_hSelf = new Panel(this);
		_hSelf->setLayoutDirection(isRTL ? Qt::RightToLeft : Qt::LeftToRight);
		createForm(_hSelf);
		_pluginName = _hSelf->windowTitle().toStdWString();

		// user information
		data->hClient		= _hSelf;
		data->pszName		= _pluginName.c_str();

		// supported features by plugin
		data->uMask			= 0;

		// additional info
		data->pszAddInfo	= NULL;
		_data = data;
	};

	bool isCreated() const {
		return !_hSelf.isNull();
	}

	virtual void updateDockingDlg(void) {
		::SendMessage(_hParent, NPPM_DMMUPDATEDISPINFO, 0, (LPARAM)_hSelf.data());
	}

	virtual void destroy() {
	};

	virtual void display(bool toShow = true) const {
		::SendMessage(_hParent, toShow?NPPM_DMMSHOW:NPPM_DMMHIDE, 0, (LPARAM)_hSelf.data());
	};

	const TCHAR * getPluginFileName() const {
		return _moduleName.c_str();
	};

	HWND getHSelf() const {
		return _hSelf;
	}

protected :
	// The controls of the panel, and its title, which names its dock
	virtual void createForm(QWidget* form) = 0;

	// The messages of the panel, as the original's dialog procedure got them
	virtual INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM , LPARAM lParam)
	{
		switch (message) 
		{
			case WM_NOTIFY: 
			{
				LPNMHDR	pnmh = (LPNMHDR)lParam;

				if (pnmh->hwndFrom == _hParent)
				{
					switch (LOWORD(pnmh->code))
					{
						case DMN_CLOSE:
						{
							break;
						}
						case DMN_FLOAT:
						{
							_isFloating = true;
							break;
						}
						case DMN_DOCK:
						{
							_isFloating = false;
							break;
						}
						default:
							break;
					}
				}
				break;
			}
			default:
				break;
		}
		return FALSE;
	};

	// The panel's widget, which hands its messages to run_dlgProc()
	class Panel : public QWidget
	{
	public:
		explicit Panel(DockingDlgInterface* dlg) : _dlg(dlg) {}

	protected:
		bool event(QEvent* event) override
		{
			if (event->type() == NppMessageEvent::eventType)
			{
				auto* message = static_cast<NppMessageEvent*>(event);
				message->result = _dlg->run_dlgProc(message->message, message->wParam, message->lParam);
				return true;
			}
			return QWidget::event(event);
		}

	private:
		DockingDlgInterface* _dlg;
	};

	bool			_isFloating = true;

	HINSTANCE		_hInst = nullptr;
	HWND			_hParent = nullptr;
	QPointer<QWidget>	_hSelf;
	tTbData*		_data = nullptr;
	int				_dlgID = 0;
	std::wstring	_moduleName;
	std::wstring	_pluginName;
};

#endif // DOCKINGDLGINTERFACE_H
