// This file is part of Notepad++ project
// Copyright (C)2006 Jens Lorenz <jens.plugin.npp@gmx.de>

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

#include <QDialog>
#include <QFileInfo>
#include <QString>

#include <array>
#include <cassert>
#include <string>

#include <QAbstractButton>
#include <QIcon>

#include <vector>

#include "NppDarkMode.h"
#include "NppResources.h"
#include "Docking.h"
#include "Notepad_plus_msgs.h"
#include "NppDarkMode.h"
#include "StaticDialog.h"
#include "dockingResource.h"
#include "resource.h"

// A panel shown in a dock widget. Subclasses set up their form on createPanelWidget().
class DockingDlgInterface : public StaticDialog
{
public:
	DockingDlgInterface() = default;
	explicit DockingDlgInterface(int dlgID): _dlgID(dlgID) {}

	void init(HINSTANCE hInst, HWND parent) override {
		StaticDialog::init(hInst, parent);
		_moduleName = L"notepad++";
	}

	virtual void create(DockedWidgetData* data, bool isRTL = false) {
		assert(data != nullptr);
		createPanelWidget(isRTL);

		_pluginName = _hSelf->windowTitle().toStdWString();

		// user information
		data->hClient = _hSelf;
		data->pszName = _pluginName.c_str();

		// supported features by plugin
		data->uMask = 0;

		// additional info
		data->pszAddInfo = nullptr;
	}

	virtual void create(DockedWidgetData* data, std::array<int, 3> iconIDs, bool isRTL = false) {
		create(data, isRTL);
		_iconIDs = iconIDs;
	}

	virtual void updateDockingDlg() {
		::SendMessage(_hParent, NPPM_DMMUPDATEDISPINFO, 0, reinterpret_cast<LPARAM>(_hSelf));
	}

	// Takes the icons again after the icon set or dark mode changed
	virtual void refreshIcons() {
		for (const ThemedButton& themed : _themedButtons)
			applyIcon(themed);
	}

	virtual void setBackgroundColor(COLORREF) {}
	virtual void setForegroundColor(COLORREF) {}

	void display(bool toShow = true) const override {
		::SendMessage(_hParent, toShow ? NPPM_DMMSHOW : NPPM_DMMHIDE, 0, reinterpret_cast<LPARAM>(_hSelf));
	}

	bool isClosed() const {
		return _isClosed;
	}

	void setClosed(bool toClose) {
		_isClosed = toClose;
	}

	const wchar_t * getPluginFileName() const {
		return _moduleName.c_str();
	}

	const std::array<int, 3>& getIconIDs() const {
		return _iconIDs;
	}

protected :
	int	_dlgID = -1;
	int _iDockedPos = 0;
	std::wstring _moduleName;
	std::wstring _pluginName;
	std::array<int, 3> _iconIDs{};
	bool _isFloating = true;
	bool _isClosed = false;

	// Creates the panel's widget and starts delivering the docking manager's notifications to onDockingNotify().
	QDialog* createPanelWidget(bool isRTL = false) {
		QDialog* panel = createDialogWidget(isRTL);
		panel->setWindowFlags(Qt::Widget);
		panel->setFont(QFont()); // panels keep the system font, not the Dialog Font of the preferences, as on Windows
		nppRegisterMessageTarget(panel, [this](UINT message, WPARAM wParam, LPARAM lParam) -> LRESULT
		{
			return onPanelMessage(message, wParam, lParam);
		});
		return panel;
	}

	// Messages sent to the panel, such as the commands of its popup menus and the notifications of a Scintilla in it
	virtual LRESULT onPanelMessage(UINT message, [[maybe_unused]] WPARAM wParam, LPARAM lParam) {
		if (message == WM_NOTIFY)
			onDockingNotify(*reinterpret_cast<const NMHDR*>(lParam));
		return 0;
	}

	// A button of the panel whose icon follows dark mode
	struct ThemedButton
	{
		QAbstractButton* _button = nullptr;
		int _lightIcon = 0;
		int _darkIcon = 0;
	};
	std::vector<ThemedButton> _themedButtons;

	void applyIcon(const ThemedButton& themed) const {
		if (const QIcon* icon = nppLoadIcon(NppDarkMode::usesDarkWidgets() ? themed._darkIcon : themed._lightIcon))
			themed._button->setIcon(*icon);
	}

	void addThemedButton(QAbstractButton* button, int lightIcon, int darkIcon) {
		_themedButtons.push_back({ button, lightIcon, darkIcon });
		applyIcon(_themedButtons.back());
	}

	virtual void onDockingNotify(const NMHDR& notification) {
		switch (LOWORD(notification.code))
		{
			case DMN_FLOAT:
				_isFloating = true;
				break;
			case DMN_DOCK:
				_iDockedPos = HIWORD(notification.code);
				_isFloating = false;
				break;
			default:
				break;
		}
	}
};
