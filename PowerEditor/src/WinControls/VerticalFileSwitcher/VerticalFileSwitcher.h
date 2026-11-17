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

#include <memory>

#include "DockingDlgInterface.h"
#include "VerticalFileSwitcherListView.h"
#include "VerticalFileSwitcher_rc.h"

#define FS_PROJECTPANELTITLE		L"Document List"

class QMenu;

class VerticalFileSwitcher : public DockingDlgInterface {
public:
	VerticalFileSwitcher();
	~VerticalFileSwitcher() override;

	void init(HINSTANCE hInst, HWND hPere, const IconList* icons) {
		DockingDlgInterface::init(hInst, hPere);
		_icons = icons;
	}

	void setIcons(const IconList* icons) {
		_icons = icons;
		_fileListView.setIcons(icons);
		reload();
	}

	using DockingDlgInterface::create;
	void create(DockedWidgetData* data, bool isRTL = false) override;

	void display(bool toShow = true) const override;

	void setParent(HWND parent2set){
		_hParent = parent2set;
	}

	//Activate document in scintilla by using the internal index
	void activateDoc(BufferID bufferID, int view) const;

	void closeDoc(BufferID bufferID) const;

	int newItem(BufferID bufferID, int iView) {
		return _fileListView.newItem(bufferID, iView);
	}

	int closeItem(BufferID bufferID, int iView) {
		return _fileListView.closeItem(bufferID, iView);
	}

	void activateItem(BufferID bufferID, int iView) {
		_fileListView.activateItem(bufferID, iView);
	}

	void setItemIconStatus(BufferID bufferID) {
		_fileListView.setItemIconStatus(bufferID) ;
	}

	void setItemColor(BufferID bufferID) {
		_fileListView.setItemColor(bufferID);
	}

	std::wstring getFullFilePath(size_t i) const {
		return _fileListView.getFullFilePath(i);
	}

	int nbSelectedFiles() const {
		return _fileListView.nbSelectedFiles();
	}

	std::vector<BufferViewInfo> getSelectedFiles(bool reverse = false) const {
		return _fileListView.getSelectedFiles(reverse);
	}

	void startColumnSort();

	void reload(){
		_fileListView.reload();
		startColumnSort();
	}

	void updateTabOrder(){
		if (_lastSortingDirection == SORT_DIRECTION_NONE) {
			_fileListView.reload();
		}
	}

	void setBackgroundColor(COLORREF bgColour) override {
		_bgColour = bgColour;
		_fileListView.setColours(_bgColour, _fgColour);
	}

	void setForegroundColor(COLORREF fgColour) override {
		_fgColour = fgColour;
		_fileListView.setColours(_bgColour, _fgColour);
	}

protected:
	void onDockingNotify(const NMHDR& notification) override;

private:
	void onHeaderClicked(int column);
	void onRightClick(const QPoint& position);
	void showColumnsMenu(const QPoint& globalPosition);
	void storeColumnWidth(int column, int width);
	bool handleViewportEvent(QEvent* event);

	std::unique_ptr<QMenu> _hGlobalMenu;
	int _lastSortingColumn = 0;
	int _lastSortingDirection = SORT_DIRECTION_NONE;
	VerticalFileSwitcherListView _fileListView;
	const IconList* _icons = nullptr;
	COLORREF _bgColour = RGB(0xFF, 0xFF, 0xFF);
	COLORREF _fgColour = RGB(0, 0, 0);

	friend class FileSwitcherEvents;
};
