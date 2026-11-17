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

#include <QIcon>
#include <QToolBar>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Notepad_plus_msgs.h"
#include "NppXml.h"
#include "ToolBarTypes.h"
#include "Window.h"

inline constexpr int REBAR_BAR_TOOLBAR = 0;
inline constexpr int REBAR_BAR_SEARCH = 1;

class QMainWindow;

class QMenu;

class ToolBar : public Window
{
public :
	ToolBar() = default;
	~ToolBar() override = default;

	void initTheme(NppXml::Document toolIconsDocRoot);
	void initHideButtonsConf(NppXml::Document toolButtonsDocRoot, const ToolBarButtonUnit* buttonUnitArray, int arraySize);

	virtual bool init(HINSTANCE hInst, HWND hPere, toolBarStatusType type, const ToolBarButtonUnit* buttonUnitArray, int arraySize);

	void destroy() override;
	void enable(int cmdID, bool doEnable) const;

	void reduce() { setState(TB_SMALL); }
	void enlarge() { setState(TB_LARGE); }
	void reduceToSet2() { setState(TB_SMALL2); }
	void enlargeToSet2() { setState(TB_LARGE2); }
	void setToBmpIcons() { setState(TB_STANDARD); }

	bool getCheckState(int ID2Check) const;
	void setCheck(int ID2Check, bool willBeChecked) const;

	toolBarStatusType getState() const {
		return _state;
	}

	// Rebuilds the icons for the current dark mode state and size
	void refreshIcons();

	// A button that opens one menu from its arrow and another by a right click
	void setButtonMenus(int cmdID, QMenu* dropDown, QMenu* contextMenu);

	void registerDynBtn(UINT message, toolbarIcons* iconHandles, HICON absentIco);
	void registerDynBtnDM(UINT message, toolbarIconsWithDarkMode* iconHandles);

	void doPopup(POINT) {} // QToolBar shows hidden buttons by itself
	void resizeIconsDpi(UINT) {}

private :
	struct DynamicButton
	{
		UINT _message = 0;
		QIcon _icon;
		QIcon _iconDarkMode;
	};

	QAction* addCommand(int cmdID);
	QAction* findAction(int cmdID) const;
	void setState(toolBarStatusType state);
	QIcon withCustomIcons(const QIcon& builtIn, size_t iconIndex) const;

	const ToolBarButtonUnit* _buttonUnits = nullptr;
	int _nbButtons = 0;
	std::vector<bool> _stdButtonsVisible;
	ToolbarPluginButtonsConf _toolbarPluginButtonsConf;
	std::vector<DynamicButton> _dynamicButtons;
	std::unordered_map<int, QAction*> _actions;
	toolBarStatusType _state = TB_SMALL;

	// The files of an icon set from toolbarButtonsConf.xml, by the index of the button without the separators
	struct CustomIconFiles
	{
		QString _normal;
		QString _disabled;
	};
	std::vector<CustomIconFiles> _customIcons;

	using Window::init;
};

// A horizontal strip of widgets by ID, which can be shown or hidden one by one
class ReBar : public Window
{
public :
	void destroy() override;
	void init(HINSTANCE hInst, HWND hPere) override;

	// Places the bands above or below the docked panels. The window's tool bar area takes care of the size.
	void attachTo(QMainWindow* window, Qt::ToolBarArea area);

	void addBand(int id, QWidget* band);
	void setIDVisible(int id, bool show);
	bool getIDVisible(int id) const;
	bool hasBand(const QWidget* band) const;

private:
	QWidget* _bandArea = nullptr;
	std::unordered_map<int, QWidget*> _bands;
};
