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

#include "StatusBar.h"

void StatusBar::init(HINSTANCE hInst, HWND hPere, int nbParts)
{
	Window::init(hInst, hPere);

	auto* bar = new QStatusBar(hPere);
	bar->setSizeGripEnabled(true);
	_hSelf = bar;

	for (int i = 0; i < nbParts; ++i)
	{
		auto* part = new QLabel(bar);
		part->setFrameShape(QFrame::NoFrame);
		part->setTextFormat(Qt::PlainText);
		// The first part takes the free space, as it does in the original with its width of -1
		bar->addPermanentWidget(part, (i == 0) ? 1 : 0);
		_parts.push_back(part);
	}
}

bool StatusBar::setPartWidth(int whichPart, int width)
{
	if (whichPart < 0 || static_cast<size_t>(whichPart) >= _parts.size())
		return false;
	if (width > 0)
		_parts[whichPart]->setMinimumWidth(width);
	return true;
}

void StatusBar::destroy()
{
	delete _hSelf;
	_hSelf = nullptr;
	_parts.clear();
}

int StatusBar::getHeight() const
{
	return (_hSelf && _hSelf->isVisible()) ? _hSelf->height() : 0;
}

bool StatusBar::setText(const wchar_t* str, int whichPart)
{
	if (whichPart < 0 || static_cast<size_t>(whichPart) >= _parts.size())
		return false;
	_parts[whichPart]->setText(QString::fromWCharArray(str));
	return true;
}

bool StatusBar::setOwnerDrawText(const wchar_t* str)
{
	return setText(str, 0);
}
