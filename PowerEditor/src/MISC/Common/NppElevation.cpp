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

#include "NppElevation.h"

#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

#include <cerrno>

namespace
{
	constexpr int pkexecNotAuthorized = 126; // also when the user dismisses the dialog
	constexpr int pkexecNoAgent = 127;

	DWORD runElevated(const QString& tool, const QStringList& arguments, const std::string* input = nullptr)
	{
		const QString pkexec = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
		const QString toolPath = QStandardPaths::findExecutable(tool);
		if (pkexec.isEmpty() || toolPath.isEmpty())
		{
			QMessageBox::warning(nullptr, QStringLiteral("Notepad++"), QStringLiteral("Administrator rights are needed for this file, but pkexec is not available on this system."));
			return EACCES;
		}

		QProcess process;
		process.setStandardOutputFile(QProcess::nullDevice());
		process.start(pkexec, QStringList{toolPath} + arguments);
		if (!process.waitForStarted())
			return EACCES;

		if (input)
		{
			process.write(input->data(), static_cast<qint64>(input->size()));
			process.closeWriteChannel();
		}

		process.waitForFinished(-1); // the time the user takes to answer the password dialog

		if (process.exitStatus() != QProcess::NormalExit)
			return EIO;

		const int exitCode = process.exitCode();
		if (exitCode == 0)
			return NO_ERROR;
		if (exitCode == pkexecNotAuthorized)
			return EACCES;
		if (exitCode == pkexecNoAgent)
		{
			QMessageBox::warning(nullptr, QStringLiteral("Notepad++"), QStringLiteral("Administrator rights are needed for this file, but no authentication agent is running in this desktop session."));
			return EACCES;
		}
		return EIO;
	}

	QString toQ(const std::wstring& s)
	{
		return QString::fromStdWString(s);
	}
}

DWORD nppElevatedSave(const std::wstring& path, const std::string& content)
{
	return runElevated(QStringLiteral("tee"), {QStringLiteral("--"), toQ(path)}, &content);
}

DWORD nppElevatedMove(const std::wstring& from, const std::wstring& to)
{
	return runElevated(QStringLiteral("mv"), {QStringLiteral("-f"), QStringLiteral("--"), toQ(from), toQ(to)});
}

DWORD nppElevatedCreateEmptyFile(const std::wstring& path)
{
	return runElevated(QStringLiteral("touch"), {QStringLiteral("--"), toQ(path)});
}

DWORD nppElevatedSetWritable(const std::wstring& path, bool writable)
{
	return runElevated(QStringLiteral("chmod"), {writable ? QStringLiteral("u+w") : QStringLiteral("a-w"), QStringLiteral("--"), toQ(path)});
}
