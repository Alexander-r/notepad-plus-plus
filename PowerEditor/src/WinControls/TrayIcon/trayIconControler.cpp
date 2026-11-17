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


#include "trayIconControler.h"

#include <QSystemTrayIcon>

#include "NppMessages.h"

trayIconControler::trayIconControler(HWND hwnd, UINT /*uID*/, UINT uCBMsg, QIcon* hicon, const wchar_t *tip)
	: _icon(std::make_unique<QSystemTrayIcon>())
{
	if (hicon)
		_icon->setIcon(*hicon);
	_icon->setToolTip(QString::fromWCharArray(tip));

	QObject::connect(_icon.get(), &QSystemTrayIcon::activated, _icon.get(), [hwnd, uCBMsg](QSystemTrayIcon::ActivationReason reason)
	{
		switch (reason)
		{
			case QSystemTrayIcon::Trigger:
			case QSystemTrayIcon::DoubleClick:
				::SendMessage(hwnd, uCBMsg, 0, WM_LBUTTONUP);
				break;

			case QSystemTrayIcon::MiddleClick:
				::SendMessage(hwnd, uCBMsg, 0, WM_MBUTTONUP);
				break;

			case QSystemTrayIcon::Context:
				::SendMessage(hwnd, uCBMsg, 0, WM_RBUTTONUP);
				break;

			default:
				break;
		}
	});
}

trayIconControler::~trayIconControler() = default;

int trayIconControler::doTrayIcon(DWORD op)
{
	if (op != ADD && op != REMOVE)
		return INCORRECT_OPERATION;

	if ((_isIconShown && op == ADD) || (!_isIconShown && op == REMOVE))
		return OPERATION_INCOHERENT;

	_icon->setVisible(op == ADD);
	_isIconShown = !_isIconShown;

	return 0;
}

int trayIconControler::reAddTrayIcon()
{
	if (!_isIconShown)
		return -1;

	_icon->hide();
	_icon->show();
	return 0;
}
