// Qt port: see NppExecWin.h

#include "NppExecWin.h"

#include <QChar>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QString>
#include <QThread>

#include <iconv.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <fcntl.h>
#include <ctime>
#include <chrono>

#include <cerrno>
#include <cstdio>
#include <string>

namespace
{
	std::string iconvName(UINT codePage)
	{
		if (codePage == CP_ACP || codePage == CP_OEMCP || codePage == CP_UTF8)
			return "UTF-8";
		return "CP" + std::to_string(codePage);
	}

	// Converts the whole input, putting the replacement for what cannot be converted, and gives the output
	std::string convert(const std::string& from, const std::string& to, const char* input, std::size_t inputSize, const std::string& replacement)
	{
		std::string output;
		iconv_t cd = iconv_open(to.c_str(), from.c_str());
		if (cd == reinterpret_cast<iconv_t>(-1))
			return output;

		char buffer[4096];
		char* in = const_cast<char*>(input);
		std::size_t inLeft = inputSize;
		while (inLeft > 0)
		{
			char* out = buffer;
			std::size_t outLeft = sizeof(buffer);
			const std::size_t result = iconv(cd, &in, &inLeft, &out, &outLeft);
			output.append(buffer, out - buffer);
			if (result == static_cast<std::size_t>(-1) && errno != E2BIG)
			{
				// An invalid or incomplete sequence: one unit is skipped and replaced, as Windows does
				output += replacement;
				const std::size_t unit = (from == "WCHAR_T") ? sizeof(wchar_t) : 1;
				in += unit;
				inLeft = inLeft > unit ? inLeft - unit : 0;
				iconv(cd, nullptr, nullptr, nullptr, nullptr);
			}
		}
		iconv_close(cd);
		return output;
	}
}

int MultiByteToWideChar(UINT codePage, DWORD /*flags*/, const char* source, int sourceLength, wchar_t* destination, int destinationSize)
{
	if (!source)
		return 0;
	const std::size_t length = (sourceLength < 0) ? std::strlen(source) + 1 : static_cast<std::size_t>(sourceLength);
	const wchar_t replacementChar = 0xFFFD;
	const std::string replacement(reinterpret_cast<const char*>(&replacementChar), sizeof(wchar_t));
	const std::string wide = convert(iconvName(codePage), "WCHAR_T", source, length, replacement);
	const int count = static_cast<int>(wide.size() / sizeof(wchar_t));
	if (destinationSize == 0)
		return count;
	if (count > destinationSize)
		return 0;
	std::memcpy(destination, wide.data(), count * sizeof(wchar_t));
	return count;
}

int WideCharToMultiByte(UINT codePage, DWORD /*flags*/, const wchar_t* source, int sourceLength, char* destination, int destinationSize,
	const char* defaultChar, BOOL* usedDefaultChar)
{
	if (usedDefaultChar)
		*usedDefaultChar = FALSE;
	if (!source)
		return 0;
	const std::size_t length = (sourceLength < 0) ? std::wcslen(source) + 1 : static_cast<std::size_t>(sourceLength);
	const std::string name = iconvName(codePage);
	const std::string replacement = (name == "UTF-8") ? std::string("\xEF\xBF\xBD") : std::string(defaultChar ? defaultChar : "?");
	const std::string bytes = convert("WCHAR_T", name, reinterpret_cast<const char*>(source), length * sizeof(wchar_t), replacement);
	const int count = static_cast<int>(bytes.size());
	if (destinationSize == 0)
		return count;
	if (count > destinationSize)
		return 0;
	std::memcpy(destination, bytes.data(), count);
	return count;
}

int wvsprintf(wchar_t* output, const wchar_t* format, va_list arguments)
{
	constexpr int limit = 1024;
	const int length = std::vswprintf(output, limit + 1, nppCompatWideFormat(format).c_str(), arguments);
	if (length < 0)
	{
		// Too long for the limit, where glibc gives nothing: as user32, the output is cut
		output[limit] = L'\0';
		return limit;
	}
	return length;
}

int wvsprintfA(char* output, const char* format, va_list arguments)
{
	constexpr int limit = 1024;
	const int length = std::vsnprintf(output, limit + 1, format, arguments);
	return (length > limit) ? limit : length;
}

int wsprintf(wchar_t* output, const wchar_t* format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	const int length = wvsprintf(output, format, arguments);
	va_end(arguments);
	return length;
}

int wsprintfA(char* output, const char* format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	const int length = wvsprintfA(output, format, arguments);
	va_end(arguments);
	return length;
}

int nppExecSwprintf(wchar_t* output, std::size_t size, const wchar_t* format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	const int length = std::vswprintf(output, size, nppCompatWideFormat(format).c_str(), arguments);
	va_end(arguments);
	if (length < 0 && size > 0)
		output[size - 1] = L'\0';
	return length;
}

UINT GetACP()
{
	return CP_UTF8;
}

UINT GetOEMCP()
{
	return CP_UTF8;
}

namespace
{
	// Seconds between 1601, where FILETIME starts, and 1970
	constexpr long long windowsEpochOffset = 11644473600LL;

	FILETIME toFileTime(const timespec& time)
	{
		const unsigned long long ticks = static_cast<unsigned long long>(time.tv_sec + windowsEpochOffset) * 10000000ULL + time.tv_nsec / 100;
		return FILETIME{static_cast<DWORD>(ticks & 0xFFFFFFFF), static_cast<DWORD>(ticks >> 32)};
	}

	timespec fromFileTime(const FILETIME& time)
	{
		const unsigned long long ticks = (static_cast<unsigned long long>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
		timespec result{};
		result.tv_sec = static_cast<time_t>(ticks / 10000000ULL) - windowsEpochOffset;
		result.tv_nsec = static_cast<long>(ticks % 10000000ULL) * 100;
		return result;
	}

	QByteArray localPath(const wchar_t* fileName)
	{
		return QFile::encodeName(QString::fromWCharArray(fileName));
	}
}

bool nppExecGetFileWriteTime(const wchar_t* fileName, FILETIME* writeTime)
{
	struct stat info{};
	if (::stat(localPath(fileName).constData(), &info) != 0)
		return false;
	*writeTime = toFileTime(info.st_mtim);
	return true;
}

bool nppExecSetFileWriteTime(const wchar_t* fileName, const FILETIME* writeTime)
{
	timespec times[2];
	times[0].tv_sec = 0;
	times[0].tv_nsec = UTIME_OMIT; // the access time stays
	times[1] = fromFileTime(*writeTime);
	return ::utimensat(AT_FDCWD, localPath(fileName).constData(), times, 0) == 0;
}

void GetLocalTime(SYSTEMTIME* time)
{
	const auto now = std::chrono::system_clock::now();
	const time_t seconds = std::chrono::system_clock::to_time_t(now);
	std::tm local{};
	::localtime_r(&seconds, &local);
	time->wYear = static_cast<WORD>(local.tm_year + 1900);
	time->wMonth = static_cast<WORD>(local.tm_mon + 1);
	time->wDayOfWeek = static_cast<WORD>(local.tm_wday);
	time->wDay = static_cast<WORD>(local.tm_mday);
	time->wHour = static_cast<WORD>(local.tm_hour);
	time->wMinute = static_cast<WORD>(local.tm_min);
	time->wSecond = static_cast<WORD>(local.tm_sec);
	time->wMilliseconds = static_cast<WORD>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000);
}

wchar_t* CharLower(wchar_t* text)
{
	for (wchar_t* p = text; p && *p; ++p)
		*p = static_cast<wchar_t>(QChar::toLower(static_cast<char32_t>(*p)));
	return text;
}

wchar_t* CharUpper(wchar_t* text)
{
	for (wchar_t* p = text; p && *p; ++p)
		*p = static_cast<wchar_t>(QChar::toUpper(static_cast<char32_t>(*p)));
	return text;
}

void nppExecOnGuiThread(const std::function<void()>& function)
{
	QCoreApplication* app = QCoreApplication::instance();
	if (app && QThread::currentThread() == app->thread())
		function();
	else if (!app || QCoreApplication::closingDown())
	{
		// The application is gone or going, with no GUI to work on: only the main thread may still do so, as it
		// cleans up; a script's thread has nothing to do there, as on Windows, where its windows would be gone
		if (QThread::isMainThread())
			function();
	}
	else
		QMetaObject::invokeMethod(app, function, Qt::BlockingQueuedConnection);
}

std::string nppExecLocalPath(const wchar_t* fileName)
{
	return QFile::encodeName(QString::fromWCharArray(fileName)).toStdString();
}

int nppExecCompareNoCase(const wchar_t* s1, int length1, const wchar_t* s2, int length2)
{
	const QString q1 = QString::fromWCharArray(s1, length1 < 0 ? static_cast<int>(std::wcslen(s1)) : length1);
	const QString q2 = QString::fromWCharArray(s2, length2 < 0 ? static_cast<int>(std::wcslen(s2)) : length2);
	return q1.compare(q2, Qt::CaseInsensitive);
}

BOOL DeleteFile(const wchar_t* fileName)
{
	return ::unlink(nppExecLocalPath(fileName).c_str()) == 0 ? TRUE : FALSE;
}

BOOL MoveFile(const wchar_t* existingName, const wchar_t* newName)
{
	// As on Windows, an existing file of the new name is not replaced
	if (QFileInfo::exists(QString::fromWCharArray(newName)))
		return FALSE;
	return ::rename(nppExecLocalPath(existingName).c_str(), nppExecLocalPath(newName).c_str()) == 0 ? TRUE : FALSE;
}

BOOL CopyFile(const wchar_t* existingName, const wchar_t* newName, BOOL failIfExists)
{
	const QString target = QString::fromWCharArray(newName);
	if (QFileInfo::exists(target))
	{
		if (failIfExists)
			return FALSE;
		QFile::remove(target);
	}
	return QFile::copy(QString::fromWCharArray(existingName), target) ? TRUE : FALSE;
}

BOOL CreateDirectory(const wchar_t* path, void* /*securityAttributes*/)
{
	return ::mkdir(nppExecLocalPath(path).c_str(), 0777) == 0 ? TRUE : FALSE;
}

DWORD GetTempPath(DWORD size, wchar_t* buffer)
{
	// With the separator at the end, as Windows gives it
	const std::wstring path = (QDir::tempPath() + u'/').toStdWString();
	if (size <= path.size())
		return static_cast<DWORD>(path.size() + 1);
	std::wcscpy(buffer, path.c_str());
	return static_cast<DWORD>(path.size());
}

BOOL SetCurrentDirectory(const wchar_t* path)
{
	return QDir::setCurrent(QString::fromWCharArray(path)) ? TRUE : FALSE;
}

DWORD GetCurrentDirectory(DWORD size, wchar_t* buffer)
{
	const std::wstring path = QDir::currentPath().toStdWString();
	if (size <= path.size())
		return static_cast<DWORD>(path.size() + 1);
	std::wcscpy(buffer, path.c_str());
	return static_cast<DWORD>(path.size());
}

BOOL SetEnvironmentVariable(const wchar_t* name, const wchar_t* value)
{
	const QByteArray key = QString::fromWCharArray(name).toLocal8Bit();
	if (!value)
		return ::unsetenv(key.constData()) == 0 ? TRUE : FALSE;
	return ::setenv(key.constData(), QString::fromWCharArray(value).toLocal8Bit().constData(), 1) == 0 ? TRUE : FALSE;
}

DWORD GetFileAttributes(const wchar_t* fileName)
{
	const QFileInfo info(QString::fromWCharArray(fileName));
	if (!info.exists())
		return INVALID_FILE_ATTRIBUTES;
	DWORD attributes = info.isDir() ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
	if (!info.isWritable())
		attributes |= FILE_ATTRIBUTE_READONLY;
	if (info.fileName().startsWith(u'.'))
		attributes |= FILE_ATTRIBUTE_HIDDEN;
	return attributes;
}

DWORD SearchPath(const wchar_t* /*path*/, const wchar_t* fileName, const wchar_t* /*extension*/, DWORD size, wchar_t* buffer, wchar_t** filePart)
{
	const QString name = QString::fromWCharArray(fileName);
	QString found;
	if (name.contains(u'/'))
	{
		if (QFileInfo(name).isFile())
			found = QFileInfo(name).absoluteFilePath();
	}
	else
	{
		const QStringList dirs = QString::fromLocal8Bit(qgetenv("PATH")).split(u':', Qt::SkipEmptyParts);
		for (const QString& dir : dirs)
		{
			const QFileInfo candidate(QDir(dir).filePath(name));
			if (candidate.isFile())
			{
				found = candidate.absoluteFilePath();
				break;
			}
		}
	}
	if (found.isEmpty())
		return 0;
	const std::wstring path = found.toStdWString();
	if (buffer && size > path.size())
	{
		std::wcscpy(buffer, path.c_str());
		if (filePart)
			*filePart = buffer + path.rfind(L'/') + 1;
		return static_cast<DWORD>(path.size());
	}
	return static_cast<DWORD>(path.size() + 1);
}
