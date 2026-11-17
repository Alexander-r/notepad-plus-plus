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

#include "ColourPicker.h"

#include <QAbstractButton>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

#include "ColourPopup.h"
#include "NppConstants.h"

namespace
{
	QColor toQColor(COLORREF colour)
	{
		return QColor(GetRValue(colour), GetGValue(colour), GetBValue(colour));
	}

	class ColourButton : public QAbstractButton
	{
	public:
		ColourButton(QWidget* parent, ColourPicker* picker) : QAbstractButton(parent), _picker(picker)
		{
			setFixedSize(25, 25);
			setFocusPolicy(Qt::StrongFocus);
		}

	protected:
		void paintEvent(QPaintEvent*) override
		{
			QPainter painter(this);
			const QRect area = rect().adjusted(0, 0, -1, -1);

			if (!QWidget::isEnabled())
			{
				painter.setPen(palette().color(QPalette::Disabled, QPalette::WindowText));
				painter.setBrush(palette().color(QPalette::Window));
				painter.drawRect(area);
				return;
			}

			const COLORREF colour = _picker->getColour();
			painter.setPen(hasFocus() ? palette().color(QPalette::Highlight) : palette().color(QPalette::Dark));
			painter.setBrush(toQColor(colour));
			painter.drawRect(area);

			if (!_picker->isEnabled())
			{
				// check if the color is too dark, if so, use white strikeout
				const int brightness = GetRValue(colour) + GetGValue(colour) + GetBValue(colour);
				painter.setPen(Qt::NoPen);
				painter.setBrush(QBrush(brightness < 200 ? Qt::white : Qt::black, Qt::FDiagPattern));
				painter.drawRect(area);
			}
		}

		void mousePressEvent(QMouseEvent* event) override
		{
			if (event->button() == Qt::RightButton)
				_picker->toggleEnabled();
			else
				QAbstractButton::mousePressEvent(event);
		}

	private:
		ColourPicker* _picker = nullptr;
	};
}

ColourPicker::ColourPicker() = default;
ColourPicker::~ColourPicker() = default;

void ColourPicker::init(HINSTANCE hInst, HWND parent)
{
	Window::init(hInst, parent);

	auto* button = new ColourButton(parent, this);
	QObject::connect(button, &QAbstractButton::clicked, button, [this]() { showPopup(); });
	_hSelf = button;
}

void ColourPicker::destroy()
{
	destroyColorPopup();
	delete _hSelf;
	_hSelf = nullptr;
}

void ColourPicker::destroyColorPopup()
{
	if (_pColourPopup)
	{
		_pColourPopup->destroy();
		_pColourPopup.reset();
	}
}

void ColourPicker::showPopup()
{
	destroyColorPopup();

	_pColourPopup = std::make_unique<ColourPopup>(_currentColour);
	_pColourPopup->init(_hInst, _hSelf);
	_pColourPopup->doDialog(_hSelf->mapToGlobal(_hSelf->rect().bottomLeft()), [this](COLORREF colour) { colourChosen(colour); });
}

void ColourPicker::colourChosen(COLORREF colour)
{
	destroyColorPopup();

	if (_currentColour != colour)
	{
		_currentColour = colour;
		if (_onPicked)
			_onPicked();
		redraw();
	}
}

void ColourPicker::toggleEnabled()
{
	if (_disableRightClick)
		return;

	_isEnabled = !_isEnabled;
	if (_onPicked)
		_onPicked();
	redraw();
}
