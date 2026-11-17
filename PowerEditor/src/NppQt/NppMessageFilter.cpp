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

#include "NppMessageFilter.h"

#include <QString>
#include <QtGlobal>

namespace
{
	QtMessageHandler g_nextHandler = nullptr;

	void filterMessage(QtMsgType type, const QMessageLogContext& context, const QString& message)
	{
		// Breeze's helper widget over dividers asks for the mouse on every click, which Wayland refuses and Qt reports.
		// Nothing depends on the answer.
		if (type == QtWarningMsg && message == QLatin1String("This plugin supports grabbing the mouse only for popup windows"))
			return;

		if (g_nextHandler)
			g_nextHandler(type, context, message);
	}
}

void nppInstallMessageFilter()
{
	static bool isInstalled = false;
	if (!isInstalled)
	{
		isInstalled = true;
		g_nextHandler = qInstallMessageHandler(filterMessage);
	}
}
