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
#include <QTabBar>

#include <vector>

#include "Window.h"
#include "dpiManagerV2.h"

//Notification message
inline constexpr UINT TCN_TABDROPPED = TCN_FIRST - 10;
inline constexpr UINT TCN_TABDROPPEDOUTSIDE = TCN_FIRST - 11;
inline constexpr UINT TCN_TABDELETE = TCN_FIRST - 12;
inline constexpr UINT TCN_MOUSEHOVERING = TCN_FIRST - 13;
inline constexpr UINT TCN_MOUSELEAVING = TCN_FIRST - 14;
inline constexpr UINT TCN_MOUSEHOVERSWITCHING = TCN_FIRST - 15;
inline constexpr UINT TCN_TABPINNED = TCN_FIRST - 16;

struct TBHDR
{
	NMHDR _hdr{};
	int _tabOrigin = 0;
};

class TabBar : public Window
{
public:
	TabBar() = default;
	~TabBar() override = default;
	void destroy() override;
	virtual void init(HINSTANCE hInst, HWND parent, bool isVertical, bool isMultiLine);
	int insertAtEnd(const wchar_t *subTabName);
	void activateAt(int index) const;
	std::wstring getCurrentTitle() const;

	int getCurrentTabIndex() const {
		return tabs() ? tabs()->currentIndex() : -1;
	}

	int getItemCount() const {
		return tabs() ? tabs()->count() : 0;
	}

	void deletItemAt(size_t index);

	void deletAllItem();

	size_t nbItem() const {
		return static_cast<size_t>(getItemCount());
	}

	int getNextOrPrevTabIdx(bool isNext) const;

	DPIManagerV2& dpiManager() { return _dpiManager; }

	QTabBar* tabs() const { return static_cast<QTabBar*>(_hSelf); }

protected:
	DPIManagerV2 _dpiManager;

	using Window::init;
};


// The document tab bar: closing and pinning buttons, drag and drop between views, colors per tab.
class TabBarPlus : public TabBar
{
public :
	TabBarPlus() = default;
	~TabBarPlus() override;

	enum tabColourIndex {
		activeText, activeFocusedTop, activeUnfocusedTop, inactiveText, inactiveBg
	};

	enum individualTabColourId {
		id0, id1, id2, id3, id4, id5, id6, id7, id8, id9
	};

	void init(HINSTANCE hInst, HWND parent, bool isVertical, bool isMultiLine, unsigned char buttonsStatus = 0);

	void destroy() override;

	POINT getDraggingPoint() const {
		return _draggingPoint;
	}

	void resetDraggingPoint() {
		_draggingPoint = {};
	}

	// Applies the current tab settings to all tab bars
	static void triggerOwnerDrawTabbar(DPIManagerV2* pDPIManager);
	static void doVertical();
	static void doMultiLine();
	static void setColour(COLORREF colour2Set, tabColourIndex i, DPIManagerV2* pDPIManager);

	virtual int getIndividualTabColourId(int tabIndex) = 0;
	virtual bool isTabPinned(int tabIndex) = 0;
	static COLORREF colour(tabColourIndex i) { return _colours[i]; }

	void tabToStart(int index = -1);
	void tabToEnd(int index = -1);

	void setTabPinButtonOrder(int) {}
	void setTabCloseButtonOrder(int) {}
	void setCloseBtnImageList() {}
	void setPinBtnImageList() {}

	void refresh();

	void notify(UINT notifyCode, int tabIndex);

	// Called by the widget
	void setDragPoint(const QPoint& globalPoint);

protected:
	void applySettings();
	void applyTabColours();

	POINT _draggingPoint{}; // coordinate of Screen

	static COLORREF _colours[5];
	static std::vector<TabBarPlus*> _instances;

	using TabBar::init;
};
