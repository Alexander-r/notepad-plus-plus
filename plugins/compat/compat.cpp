// The functions of the compat headers that are not inline. Each plugin is built with its own copy, which stays
// inside it, so that a plugin needs nothing from Notepad++ but its messages.

#include "windows.h"
#include "shlwapi.h"

#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QSaveFile>
#include <QStringDecoder>
#include <QFileInfo>
#include <QMessageBox>
#include <QRegularExpression>

#include <dlfcn.h>

#include <map>
#include <string>
#include <vector>

DWORD GetModuleFileName(HMODULE /*module*/, wchar_t* fileName, DWORD size)
{
	Dl_info info{};
	if (!size || !dladdr(reinterpret_cast<void*>(&GetModuleFileName), &info) || !info.dli_fname)
		return 0;

	const std::wstring path = QFileInfo(QString::fromLocal8Bit(info.dli_fname)).absoluteFilePath().toStdWString();
	wcsncpy_s(fileName, size, path.c_str(), _TRUNCATE);
	return static_cast<DWORD>(std::wcslen(fileName));
}

int MessageBox(HWND parent, const wchar_t* text, const wchar_t* title, UINT type)
{
	QMessageBox box(parent);
	box.setWindowTitle(QString::fromWCharArray(title));
	box.setTextFormat(Qt::PlainText); // as on Windows, where "<name@mail>" is text
	box.setText(QString::fromWCharArray(text));

	switch (type & MB_ICONMASK)
	{
		case MB_ICONERROR: box.setIcon(QMessageBox::Critical); break;
		case MB_ICONQUESTION: box.setIcon(QMessageBox::Question); break;
		case MB_ICONWARNING: box.setIcon(QMessageBox::Warning); break;
		case MB_ICONINFORMATION: box.setIcon(QMessageBox::Information); break;
		default: box.setIcon(QMessageBox::NoIcon); break;
	}

	struct Choice
	{
		QMessageBox::StandardButton button;
		int id;
	};
	std::vector<Choice> choices;
	switch (type & MB_TYPEMASK)
	{
		case MB_OKCANCEL: choices = { { QMessageBox::Ok, IDOK }, { QMessageBox::Cancel, IDCANCEL } }; break;
		case MB_ABORTRETRYIGNORE: choices = { { QMessageBox::Abort, IDABORT }, { QMessageBox::Retry, IDRETRY }, { QMessageBox::Ignore, IDIGNORE } }; break;
		case MB_YESNOCANCEL: choices = { { QMessageBox::Yes, IDYES }, { QMessageBox::No, IDNO }, { QMessageBox::Cancel, IDCANCEL } }; break;
		case MB_YESNO: choices = { { QMessageBox::Yes, IDYES }, { QMessageBox::No, IDNO } }; break;
		case MB_RETRYCANCEL: choices = { { QMessageBox::Retry, IDRETRY }, { QMessageBox::Cancel, IDCANCEL } }; break;
		default: choices = { { QMessageBox::Ok, IDOK } }; break;
	}
	for (const Choice& choice : choices)
		box.addButton(choice.button);

	const std::size_t defaultIndex = (type & MB_DEFMASK) == MB_DEFBUTTON2 ? 1 : ((type & MB_DEFMASK) == MB_DEFBUTTON3 ? 2 : 0);
	if (defaultIndex < choices.size())
		box.setDefaultButton(choices[defaultIndex].button);

	box.exec();
	const QAbstractButton* clicked = box.clickedButton();
	for (const Choice& choice : choices)
	{
		if (clicked == box.button(choice.button))
			return choice.id;
	}
	// Closed without a button, which Windows reports as Cancel where there is one
	for (const Choice& choice : choices)
	{
		if (choice.id == IDCANCEL)
			return IDCANCEL;
	}
	return choices.size() == 1 ? choices.front().id : IDCANCEL;
}

int MessageBoxA(HWND parent, const char* text, const char* title, UINT type)
{
	// The ANSI code page of a system in UTF-8, as here
	const std::wstring wideText = QString::fromUtf8(text ? text : "").toStdWString();
	const std::wstring wideTitle = QString::fromUtf8(title ? title : "").toStdWString();
	return MessageBox(parent, wideText.c_str(), wideTitle.c_str(), type);
}

FILE* _wfopen(const wchar_t* path, const wchar_t* mode)
{
	return std::fopen(QFile::encodeName(QString::fromWCharArray(path)).constData(), QString::fromWCharArray(mode).toLatin1().constData());
}

BOOL PathFileExists(const wchar_t* path)
{
	return QFileInfo::exists(QString::fromWCharArray(path)) ? TRUE : FALSE;
}

BOOL PathIsDirectory(const wchar_t* path)
{
	return QFileInfo(QString::fromWCharArray(path)).isDir() ? TRUE : FALSE;
}

BOOL PathMatchSpec(const wchar_t* path, const wchar_t* spec)
{
	QString pattern = QString::fromWCharArray(spec).trimmed();
	if (pattern == QStringLiteral("*.*"))
		pattern = QStringLiteral("*");
	const QRegularExpression wildcard(QRegularExpression::wildcardToRegularExpression(pattern, QRegularExpression::NonPathWildcardConversion),
		QRegularExpression::CaseInsensitiveOption);
	return wildcard.match(QString::fromWCharArray(path)).hasMatch() ? TRUE : FALSE;
}

std::wstring nppCompatWideFormat(const wchar_t* format)
{
	std::wstring result;
	for (const wchar_t* it = format; *it != L'\0'; ++it)
	{
		result += *it;
		if (*it != L'%')
			continue;
		++it;
		if (*it == L'%')
		{
			result += *it;
			continue;
		}
		// Flags, width, precision and length, up to the conversion. MSVC's lengths "I64", "I32" and "I" become
		// glibc's "ll", nothing and "z".
		std::wstring spec;
		while (*it != L'\0' && std::wcschr(L"-+ #0123456789.*hlLqjztI", *it) != nullptr)
		{
			if (*it == L'I')
			{
				if (it[1] == L'6' && it[2] == L'4')
				{
					spec += L"ll";
					it += 3;
				}
				else if (it[1] == L'3' && it[2] == L'2')
					it += 3;
				else
				{
					spec += L'z';
					++it;
				}
				continue;
			}
			spec += *it++;
		}
		if (*it == L'\0')
			return result + spec;
		const bool hasLength = spec.find_first_of(L"hlLqjzt") != std::wstring::npos;
		switch (*it)
		{
			case L's':
			case L'c':
				result += spec + (hasLength ? L"" : L"l") + *it;
				break;
			case L'S':
				result += spec + L's';
				break;
			case L'C':
				result += spec + L'c';
				break;
			default:
				result += spec + *it;
				break;
		}
	}
	return result;
}

namespace
{
	// An INI file as its lines, kept while the file does not change, so that many calls read it once
	struct IniFile
	{
		QDateTime modified;
		qint64 size = -1;
		QStringList lines;
	};

	QMutex g_iniLock;

	IniFile& iniFile(const QString& path)
	{
		static std::map<QString, IniFile> files;
		IniFile& file = files[path];
		const QFileInfo info(path);
		const QDateTime modified = info.exists() ? info.lastModified() : QDateTime();
		const qint64 size = info.exists() ? info.size() : -1;
		if (file.modified != modified || file.size != size)
		{
			file.lines.clear();
			QFile in(path);
			if (in.open(QIODevice::ReadOnly))
			{
				const QByteArray data = in.readAll();
				QString text;
				if (data.startsWith("\xFF\xFE"))
					text = QStringDecoder(QStringDecoder::Utf16LE)(data.mid(2));
				else if (data.startsWith("\xEF\xBB\xBF"))
					text = QString::fromUtf8(data.mid(3));
				else
					text = QString::fromUtf8(data);
				text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
				file.lines = text.split(u'\n');
				if (!file.lines.isEmpty() && file.lines.last().isEmpty())
					file.lines.removeLast();
			}
			file.modified = modified;
			file.size = size;
		}
		return file;
	}

	bool saveIniFile(const QString& path, IniFile& file)
	{
		QSaveFile out(path);
		if (!out.open(QIODevice::WriteOnly))
			return false;
		for (const QString& line : file.lines)
			out.write((line + u'\n').toUtf8());
		if (!out.commit())
			return false;
		const QFileInfo info(path);
		file.modified = info.lastModified();
		file.size = info.size();
		return true;
	}

	// The name of a "[section]" line, or a null string
	QString sectionOf(const QString& line)
	{
		const QString trimmed = line.trimmed();
		if (!trimmed.startsWith(u'[') || !trimmed.contains(u']'))
			return QString();
		return trimmed.mid(1, trimmed.indexOf(u']') - 1).trimmed();
	}

	// The key of a "key=value" line, or a null string
	QString keyOf(const QString& line)
	{
		const int equals = line.indexOf(u'=');
		if (equals < 0 || line.trimmed().startsWith(u';'))
			return QString();
		return line.left(equals).trimmed();
	}

	QString valueOf(const QString& line)
	{
		QString value = line.mid(line.indexOf(u'=') + 1).trimmed();
		if (value.size() >= 2 && (value.front() == u'"' || value.front() == u'\'') && value.back() == value.front())
			value = value.mid(1, value.size() - 2);
		return value;
	}

	// The lines of the section: from its header to the next, or -1 when there is none
	std::pair<int, int> sectionRange(const QStringList& lines, const QString& section)
	{
		for (int i = 0; i < lines.size(); ++i)
		{
			const QString name = sectionOf(lines[i]);
			if (!name.isNull() && name.compare(section, Qt::CaseInsensitive) == 0)
			{
				int end = i + 1;
				while (end < lines.size() && sectionOf(lines[end]).isNull())
					++end;
				return {i, end};
			}
		}
		return {-1, -1};
	}

	// Copies the strings as a list that ends with two nulls, cut as Windows cuts it
	DWORD copyList(const QStringList& items, wchar_t* output, DWORD size)
	{
		if (!output || size == 0)
			return 0;
		DWORD pos = 0;
		for (const QString& item : items)
		{
			const std::wstring text = item.toStdWString();
			if (pos + text.size() + 2 > size)
			{
				const DWORD room = (size >= pos + 2) ? size - pos - 2 : 0;
				std::wmemcpy(output + pos, text.data(), room);
				pos += room;
				output[pos] = L'\0';
				if (pos + 1 < size)
					output[pos + 1] = L'\0';
				return size - 2;
			}
			std::wmemcpy(output + pos, text.data(), text.size());
			pos += static_cast<DWORD>(text.size());
			output[pos++] = L'\0';
		}
		output[pos] = L'\0';
		return pos;
	}
}

DWORD GetPrivateProfileString(const wchar_t* section, const wchar_t* key, const wchar_t* defaultValue, wchar_t* output, DWORD size,
	const wchar_t* fileName)
{
	QMutexLocker lock(&g_iniLock);
	const IniFile& file = iniFile(QString::fromWCharArray(fileName));
	if (!section)
	{
		QStringList names;
		for (const QString& line : file.lines)
		{
			const QString name = sectionOf(line);
			if (!name.isNull())
				names << name;
		}
		return copyList(names, output, size);
	}

	const auto [begin, end] = sectionRange(file.lines, QString::fromWCharArray(section));
	if (!key)
	{
		QStringList keys;
		for (int i = begin + 1; begin >= 0 && i < end; ++i)
		{
			const QString name = keyOf(file.lines[i]);
			if (!name.isEmpty())
				keys << name;
		}
		return copyList(keys, output, size);
	}

	QString value = QString::fromWCharArray(defaultValue ? defaultValue : L"").trimmed();
	const QString wanted = QString::fromWCharArray(key);
	for (int i = begin + 1; begin >= 0 && i < end; ++i)
	{
		if (keyOf(file.lines[i]).compare(wanted, Qt::CaseInsensitive) == 0)
		{
			value = valueOf(file.lines[i]);
			break;
		}
	}
	if (!output || size == 0)
		return 0;
	const std::wstring text = value.toStdWString();
	const DWORD length = static_cast<DWORD>(std::min<std::size_t>(text.size(), size - 1));
	std::wmemcpy(output, text.data(), length);
	output[length] = L'\0';
	return length;
}

UINT GetPrivateProfileInt(const wchar_t* section, const wchar_t* key, int defaultValue, const wchar_t* fileName)
{
	wchar_t buffer[64];
	if (GetPrivateProfileString(section, key, L"", buffer, 64, fileName) == 0)
		return static_cast<UINT>(defaultValue);
	// The digits at the start, with a sign, as Windows reads them
	return static_cast<UINT>(std::wcstol(buffer, nullptr, 10));
}

BOOL WritePrivateProfileString(const wchar_t* section, const wchar_t* key, const wchar_t* value, const wchar_t* fileName)
{
	if (!section)
		return FALSE;
	QMutexLocker lock(&g_iniLock);
	const QString path = QString::fromWCharArray(fileName);
	IniFile& file = iniFile(path);
	const QString sectionName = QString::fromWCharArray(section);
	auto [begin, end] = sectionRange(file.lines, sectionName);
	if (!key)
	{
		if (begin >= 0)
			file.lines.erase(file.lines.begin() + begin, file.lines.begin() + end);
		return saveIniFile(path, file) ? TRUE : FALSE;
	}

	const QString keyName = QString::fromWCharArray(key);
	int found = -1;
	for (int i = begin + 1; begin >= 0 && i < end; ++i)
	{
		if (keyOf(file.lines[i]).compare(keyName, Qt::CaseInsensitive) == 0)
		{
			found = i;
			break;
		}
	}
	if (!value)
	{
		if (found >= 0)
			file.lines.removeAt(found);
		return saveIniFile(path, file) ? TRUE : FALSE;
	}

	const QString line = keyName + u'=' + QString::fromWCharArray(value);
	if (found >= 0)
		file.lines[found] = line;
	else if (begin >= 0)
	{
		// After the section's last key, before the empty lines that end it
		int at = end;
		while (at > begin + 1 && file.lines[at - 1].trimmed().isEmpty())
			--at;
		file.lines.insert(at, line);
	}
	else
	{
		file.lines << u'[' + sectionName + u']' << line;
	}
	return saveIniFile(path, file) ? TRUE : FALSE;
}
