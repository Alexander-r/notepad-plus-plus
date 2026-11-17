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
#include <QWidget>

#include "NppMessages.h"
#include "NppPlatform.h"

class Window
{
public:
	//! \name Constructors & Destructor
	//@{
	Window() = default;
	Window(const Window&) = delete;
	virtual ~Window() = default;
	//@}


	virtual void init(HINSTANCE hInst, HWND parent) {
		_hInst = hInst;
		_hParent = parent;
	}

	virtual void destroy() = 0;

	virtual void display(bool toShow = true) const {
		if (_hSelf)
			_hSelf->setVisible(toShow);
	}


	virtual void reSizeTo(RECT & rc) { // should NEVER be const !!!
		// rc holds x, y, width, height here
		if (_hSelf)
			_hSelf->setGeometry(rc.left, rc.top, rc.right, rc.bottom);
		redraw();
	}


	virtual void reSizeToWH(RECT& rc) { // should NEVER be const !!!
		if (_hSelf)
			_hSelf->setGeometry(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
		redraw();
	}


	virtual void redraw(bool forceUpdate = false) const {
		if (!_hSelf)
			return;
		if (forceUpdate)
			_hSelf->repaint();
		else
			_hSelf->update();
	}


	virtual void getClientRect(RECT & rc) const {
		rc = {};
		if (_hSelf)
		{
			rc.right = _hSelf->width();
			rc.bottom = _hSelf->height();
		}
	}

	virtual void getWindowRect(RECT & rc) const {
		rc = {};
		if (_hSelf)
		{
			const QRect frame = _hSelf->frameGeometry();
			const QPoint topLeft = _hSelf->isWindow() ? frame.topLeft() : _hSelf->mapToGlobal(QPoint(0, 0));
			rc.left = topLeft.x();
			rc.top = topLeft.y();
			rc.right = topLeft.x() + frame.width();
			rc.bottom = topLeft.y() + frame.height();
		}
	}

	virtual int getWidth() const {
		return _hSelf ? _hSelf->width() : 0;
	}

	virtual int getHeight() const {
		return (_hSelf && _hSelf->isVisible()) ? _hSelf->height() : 0;
	}

	virtual bool isVisible() const {
		return _hSelf && _hSelf->isVisible();
	}

	HWND getHSelf() const {
		return _hSelf;
	}

	// The widget that represents this window in a layout or a splitter
	virtual HWND paneWidget() const {
		return _hSelf;
	}

	HWND getHParent() const {
		return _hParent;
	}

	void grabFocus() const {
		if (_hSelf)
			_hSelf->setFocus();
	}

	HINSTANCE getHinst() const {
		return _hInst;
	}

	Window& operator = (const Window&) = delete;

protected:
	HINSTANCE _hInst = nullptr;
	HWND _hParent = nullptr;
	HWND _hSelf = nullptr;
};
