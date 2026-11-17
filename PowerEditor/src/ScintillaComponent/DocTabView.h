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

#include <vector>

#include "Buffer.h"
#include "IconList.h"
#include "NppConstants.h"
#include "ScintillaEditView.h"
#include "TabBar.h"
#include "Window.h"


class DocTabView : public TabBarPlus
{
public:
	DocTabView() : TabBarPlus(), _pView(nullptr) {}
	~DocTabView() override {}

	void init(HINSTANCE hInst, HWND parent, ScintillaEditView * pView, unsigned char indexChoice, unsigned char buttonsStatus);

	void changeIconSet(unsigned char choice);
	const IconList* iconList(int choice) const { return (choice >= 0 && static_cast<size_t>(choice) < _iconSets.size()) ? &_iconSets[choice] : nullptr; }

	void addBuffer(BufferID buffer);
	void closeBuffer(BufferID buffer);
	void bufferUpdated(const Buffer* buffer, int mask);

	bool activateBuffer(BufferID buffer);

	BufferID activeBuffer();
	BufferID findBufferByName(const wchar_t * fullfilename);	//-1 if not found, something else otherwise

	int getIndexByBuffer(BufferID id);
	BufferID getBufferByIndex(size_t index);

	void setBuffer(size_t index, BufferID id);

	void resizeIconsDpi() {} // Qt scales icons

	// The tab bar above its editor
	HWND paneWidget() const override { return _pane; }

	// Shows or hides the whole view
	void display(bool toShow = true) const override;
	bool isVisible() const override { return _pane && _pane->isVisible(); }

	// Applies the "hide tab bar" option, which leaves the editor showing
	void syncTabBarVisibility() const;

	const ScintillaEditView* getScintillaEditView() const {
		return _pView;
	}

	static void setIndividualTabColour(BufferID bufferId, int colorId);
	int getIndividualTabColourId(int tabIndex) override;
	bool isTabPinned(int tabIndex) override;

private :
	ScintillaEditView *_pView = nullptr;
	HWND _pane = nullptr;

	std::vector<IconList> _iconSets;
	size_t _iconSetChoice = 0;

	using TabBarPlus::init;
};
