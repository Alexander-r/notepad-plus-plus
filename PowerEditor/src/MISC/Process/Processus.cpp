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


#include <QDesktopServices>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>

#include "Processus.h"


namespace
{
	QStringList splitArgs(const std::wstring& args)
	{
		return QProcess::splitCommand(QString::fromStdWString(args));
	}
}

// Same semantic as ShellExecute "open": a program is started, and anything else (a document, a folder,
// a URL) goes to the desktop's default handler.
void Process::run([[maybe_unused]] bool isElevationRequired) const
{
	const QString command = QString::fromStdWString(_command);
	const QFileInfo info(command);
	if ((info.exists() && info.isFile() && !info.isExecutable()) || info.isDir() || command.contains(QStringLiteral("://")))
	{
		QDesktopServices::openUrl(command.contains(QStringLiteral("://")) ? QUrl(command) : QUrl::fromLocalFile(command));
		return;
	}
	QProcess::startDetached(command, splitArgs(_args), QString::fromStdWString(_curDir));
}

unsigned long Process::runSync([[maybe_unused]] bool isElevationRequired) const
{
	QProcess process;
	process.setWorkingDirectory(QString::fromStdWString(_curDir));
	process.start(QString::fromStdWString(_command), splitArgs(_args));
	if (!process.waitForStarted())
		throw QString(process.errorString()).toStdWString(); // throw exception
	process.waitForFinished(-1);
	return static_cast<unsigned long>(process.exitCode());
}
