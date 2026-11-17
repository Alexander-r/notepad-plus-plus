// This file is part of Notepad++ project
// Copyright (c) 2021 adzm / Adam D. Walling

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

#include <array>
#include <memory>
#include <vector>

#include "DockingCont.h"
#include "Window.h"
#include "dockingResource.h"

class QDockWidget;
class QMainWindow;

// The panels docked around the document area, on Qt's dock widgets.
class DockingManager : public Window
{
public:
	DockingManager();
	~DockingManager() override;

	void init(HINSTANCE hInst, QMainWindow* mainWindow);
	void destroy() override;

	void showFloatingContainers(bool show);

	void updateContainerInfo(HWND hClient);
	void createDockableDlg(DockedWidgetData data, int iCont = CONT_LEFT, bool isVisible = false);
	void setActiveTab(int iCont, int iItem);
	void showDockableDlg(HWND hDlg, bool view);
	void showDockableDlg(const wchar_t* pszName, bool view);

	// The current panels by area. Valid until panels change.
	std::vector<DockingCont*>& getContainerInfo();

	void getDockInfo(tDockMgr *pDockData) const;

	int getDockedContSize(int iCont) const;
	void setDockedContSize(int iCont, int iSize);

	void resize() {} // Qt lays out dock widgets

	// Called when the user closes a panel: tells it with DMN_CLOSE, and gives true when it answers to stay open
	bool panelClosed(DockedPanel* panel);

	// Called when a panel is shown or hidden, for the size of its area
	void panelShown(const DockedPanel* panel);
	void panelHidden(const DockedPanel* panel);

private:
	DockedPanel* findPanel(HWND hClient) const;
	int containerOf(const DockedPanel* panel) const;
	void place(DockedPanel* panel, int iCont);
	int sizeIn(const DockedPanel* panel, int iCont) const;
	void detachPanels();

	QMainWindow* _mainWindow = nullptr;
	// The width or height of each area, which it keeps while no panel is in it, as a container does on Windows
	std::array<int, DOCKCONT_MAX> _contSizes{};
	std::vector<std::unique_ptr<DockedPanel>> _panels;
	std::vector<std::unique_ptr<DockingCont>> _containers;
	std::vector<DockingCont*> _containerPointers;
};
