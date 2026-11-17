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

#include "DockingDlgInterface.h"
#include "ListView.h"
#include "Window.h"
#include "clipboardHistoryPanel_rc.h"

#define CH_PROJECTPANELTITLE		L"Clipboard History"

class ScintillaEditView;

// Either the text on the clipboard, or the exact bytes of a binary copy
struct ClipboardDataInfo {
	std::wstring _text;
	std::vector<unsigned char> _data;
	bool _isBinaryContained = false;

	bool isEmpty() const { return _text.empty() && _data.empty(); }
	bool operator==(const ClipboardDataInfo&) const = default;
};

class ClipboardHistoryPanel : public DockingDlgInterface {
public:
	ClipboardHistoryPanel()
		: DockingDlgInterface(IDD_CLIPBOARDHISTORY_PANEL), _ppEditView(nullptr)
	{}

	void init(HINSTANCE hInst, HWND hPere, ScintillaEditView **ppEditView) {
		DockingDlgInterface::init(hInst, hPere);
		_ppEditView = ppEditView;
	}

	void setParent(HWND parent2set) {
		_hParent = parent2set;
	}

	using DockingDlgInterface::create;
	void create(DockedWidgetData* data, bool isRTL = false) override;

	ClipboardDataInfo getClipboadData();

	[[nodiscard]] std::wstring getDisplayStr(const ClipboardDataInfo& cbd);
	void addToClipboadHistory(const ClipboardDataInfo& cbd);
	int getClipboardDataIndex(const ClipboardDataInfo& cbd) const;

	void setBackgroundColor(COLORREF bgColour) override {
		_bgColour = bgColour;
		_listView.setColours(_bgColour, _fgColour);
	}
	void setForegroundColor(COLORREF fgColour) override {
		_fgColour = fgColour;
		_listView.setColours(_bgColour, _fgColour);
	}

	bool trackClipboardOps(bool bTrack) {
		bool bPreviousState = _isTrackingClipboardOps;
		_isTrackingClipboardOps = bTrack;
		return bPreviousState;
	}

protected:
	void onDockingNotify(const NMHDR& notification) override;

private:
	ScintillaEditView **_ppEditView = nullptr;
	std::vector<ClipboardDataInfo> _clipboardDataInfos;
	ListView _listView;
	bool _isTrackingClipboardOps = true; // false when we do not want to track & show some Clipboard operations
	COLORREF _bgColour = RGB(0xFF, 0xFF, 0xFF);
	COLORREF _fgColour = RGB(0, 0, 0);

	using DockingDlgInterface::init;

	void onDoubleClick(int row);
};
