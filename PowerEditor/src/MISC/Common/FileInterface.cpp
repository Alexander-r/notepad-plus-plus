// This file is part of Notepad++ project
// Copyright (C)2021 Pavel Nedev (pg.nedev@gmail.com)

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

#include <locale>
#include "FileInterface.h"
#include <QMessageBox>
#include <QString>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Parameters.h"

using namespace std;

Win32_IO_File::Win32_IO_File(const wchar_t *fname)
{
	if (fname)
	{
		std::wstring fn = fname;
		_path = wstring2string(fn, CP_UTF8);

		// Truncating in place keeps the inode, so the owner, permissions, hard links and extended attributes
		// of an existing file survive, like the creation date and attributes the Windows version restores.
		WIN32_FILE_ATTRIBUTE_DATA attributes{};
		attributes.dwFileAttributes = INVALID_FILE_ATTRIBUTES;
		bool isTimeoutReached = false;
		const bool fileExists = getFileAttributesExWithTimeout(fname, &attributes, 0, &isTimeoutReached)
			&& !(attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);

		if (!fileExists && isTimeoutReached)
		{
			// The file doesn't exist, plus a network problem has been detected due to timeout
			_dwErrorCode = ERROR_FILE_NOT_FOUND; // store
			return;                              // In this case, we don't call open to prevent hanging
		}

		_fd = ::open(_path.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
		if (_fd == -1)
			_dwErrorCode = ::GetLastError(); // store
	}
}

void Win32_IO_File::close()
{
	_dwErrorCode = NO_ERROR; // reset

	if (isOpened())
	{
		NppParameters& nppParam = NppParameters::getInstance();

		DWORD flushError = NO_ERROR;
		if (_written)
		{
			if (::fsync(_fd) != 0)
			{
				flushError = ::GetLastError();
				std::wstring errNumberMsg = std::to_wstring(flushError) + L" - " + GetLastErrorAsString(flushError);

				if (!nppParam.isEndSessionCritical())
				{
					// because of there is not an externally forced shutdown/restart in progress,
					// we can at least alert the user that the file data could not actually be saved
					std::wstring errMsg = L"Notepad++ has encountered a serious system problem while saving:\n\n";
					errMsg += string2wstring(_path, CP_UTF8);
					errMsg += L"\n\nThat file, temporarily stored in the system cache, cannot be finally committed to the storage device selected! \
This is probably a storage driver or hardware issue, beyond the control of the Notepad++. \
Please try using another storage and also check if your saved data is not corrupted.\n\nError Code reported: ";
					errMsg += errNumberMsg;
					QMessageBox::warning(nullptr, QStringLiteral("WARNING - filebuffer flushing fail!"), QString::fromStdWString(errMsg));
				}
				else
				{
					// writing breif log here
					std::wstring nppFlushFileBuffersFailsLog = L"nppFlushFileBuffersFails.log";
					std::wstring nppIssueLog = nppParam.getUserPath();
					pathAppend(nppIssueLog, nppFlushFileBuffersFailsLog);

					std::string errNumberMsgA = wstring2string(errNumberMsg, CP_UTF8);

					writeLog(nppIssueLog.c_str(), errNumberMsgA.c_str());
				}
			}
		}

		_dwErrorCode = flushError; // store possible flushing error 1st

		if (::close(_fd) != 0)
		{
			if (!flushError)
				_dwErrorCode = ::GetLastError(); // store
		}

		_fd = -1;
	}
}


bool Win32_IO_File::write(const void *wbuf, size_t buf_size)
{
	_dwErrorCode = NO_ERROR; // reset

	if (!isOpened() || (wbuf == nullptr))
		return false;

	size_t total_bytes_written = 0;
	while (total_bytes_written < buf_size)
	{
		const ssize_t bytes_written = ::write(_fd, static_cast<const char*>(wbuf) + total_bytes_written, buf_size - total_bytes_written);
		if (bytes_written < 0)
		{
			if (errno == EINTR)
				continue;
			_dwErrorCode = ::GetLastError(); // store
			return false;
		}
		total_bytes_written += static_cast<size_t>(bytes_written);
	}

	_written = true;

	return (total_bytes_written == buf_size);
}
