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


#include "Common.h"

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLineEdit>
#include <QStandardPaths>
#include <QLocale>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QScreen>
#include <QStorageInfo>
#include <QTextCodec>
#include <QToolTip>
#include <QUrl>
#include <QWidget>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <exception>
#include <filesystem>
#include <iterator>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <array>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "FileInterface.h"
#include "NppConstants.h"
#include "NppElevation.h"
#include "Utf8.h"
#include "WindowsCodePages.h"
#include "sha-256.h"

using namespace std;

namespace
{
	QString toQ(const wstring& s) { return QString::fromStdWString(s); }
	QString toQ(const wchar_t* s) { return s ? QString::fromWCharArray(s) : QString(); }
	wstring toW(const QString& s) { return s.toStdWString(); }

	void showMessage(const wstring& text, const wstring& title, QMessageBox::Icon icon)
	{
		QMessageBox box(icon, toQ(title), toQ(text), QMessageBox::Ok);
		box.exec();
	}
}

#ifndef _WIN32
FILE* _wfopen(const wchar_t* path, const wchar_t* mode)
{
	const std::string narrowPath = QString::fromWCharArray(path).toUtf8().toStdString();
	const std::string narrowMode = QString::fromWCharArray(mode).toStdString();
	return std::fopen(narrowPath.c_str(), narrowMode.c_str());
}
#endif

void printInt(int int2print)
{
	showMessage(to_wstring(int2print), L"", QMessageBox::NoIcon);
}


void printStr(const wchar_t *str2print)
{
	showMessage(str2print, L"", QMessageBox::NoIcon);
}

wstring commafyInt(size_t n)
{
	std::wstringstream ss;
	ss.imbue(getSysLocale());
	ss << n;
	return ss.str();
}

std::string getFileContent(const wchar_t* file2read, bool* pbFailed)
{
	if (pbFailed)
		*pbFailed = false; // reset

	if (!doesFileExist(file2read))
	{
		if (pbFailed)
			*pbFailed = true;
		return "";
	}

	FILE* fp = _wfopen(file2read, L"rb");
	if (!fp)
	{
		if (pbFailed)
			*pbFailed = true;
		return "";
	}
	
	static constexpr size_t blockSize = 1024 * 4; // 4K is optimal chunk for memory, cache, disk or network
	char data[blockSize];
	std::string wholeFileContent;
	size_t lenFile = 0;
	try {
		do
		{
			lenFile = fread(data, 1, blockSize, fp);
			if (lenFile == 0) break;
			wholeFileContent.append(data, lenFile);
		} while (lenFile > 0);
	}
	catch ([[maybe_unused]] const std::bad_alloc& ex)
	{
		if (pbFailed)
			*pbFailed = true;
		std::string().swap(wholeFileContent); // to immediately release all the allocated memory
		showMessage(L"std::bad_alloc exception caught!\n\nProbably not enough contiguous memory to complete the operation.",
			L"Notepad++ - getFileContent", QMessageBox::Warning);
	}
	catch (...)
	{
		if (pbFailed)
			*pbFailed = true;
		std::string().swap(wholeFileContent); // to immediately release all the allocated memory
	}

	fclose(fp);
	return wholeFileContent;
}

wstring relativeFilePathToFullFilePath(const wchar_t *relativeFilePath)
{
	return toW(QDir::cleanPath(QDir::current().absoluteFilePath(toQ(relativeFilePath))));
}


void writeFileContent(const wchar_t *file2write, const char *content2write)
{
	Win32_IO_File file(file2write);

	if (file.isOpened())
		file.writeStr(content2write);
}

DWORD writeFileContentEx(const wchar_t* file2write, const char* content2write, size_t length)
{
	Win32_IO_File file(file2write);

	if (file.isOpened())
		file.writeStr(content2write, length);

	return file.getLastErrorCode();
}

void writeLog(const wchar_t* logFileName, const char* log2write)
{
	QFile file(toQ(logFileName));
	if (file.open(QIODevice::WriteOnly | QIODevice::Append))
	{
		const QByteArray line = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")).toUtf8() + "  " + log2write + "\n";
		file.write(line);
		file.flush();
	}
}

void writeLog(const wchar_t* logFileName, const wchar_t* log2write)
{
	writeLog(logFileName, toQ(log2write).toUtf8().constData());
}

// outputCtrl is the QLineEdit to read the start directory from and write the result to, as the Win32
// version did through its control ID. It may be null.
wstring folderBrowser(HWND parent, const wstring & title, QLineEdit* outputCtrl, const wchar_t *defaultStr)
{
	QString directory;
	if (outputCtrl)
		directory = outputCtrl->text();
	if (directory.isEmpty() && defaultStr)
		directory = toQ(defaultStr);

	const QString folderName = QFileDialog::getExistingDirectory(parent, toQ(title), directory);
	if (!folderName.isEmpty() && outputCtrl)
		outputCtrl->setText(QDir::toNativeSeparators(folderName));
	return toW(QDir::toNativeSeparators(folderName));
}


wstring getFolderName(HWND parent, const wchar_t *defaultDir)
{
	return folderBrowser(parent, L"Select a folder", nullptr, defaultDir);
}


std::vector<wstring> tokenizeString(const wstring & tokenString, const char delim)
{
	//Vector is created on stack and copied on return
	std::vector<wstring> tokens;

    // Skip delimiters at beginning.
	wstring::size_type lastPos = tokenString.find_first_not_of(delim, 0);
    // Find first "non-delimiter".
    wstring::size_type pos     = tokenString.find_first_of(delim, lastPos);

    while (pos != std::string::npos || lastPos != std::string::npos)
    {
        // Found a token, add it to the vector.
        tokens.push_back(tokenString.substr(lastPos, pos - lastPos));
        // Skip delimiters.  Note the "not_of"
        lastPos = tokenString.find_first_not_of(delim, pos);
        // Find next "non-delimiter"
        pos = tokenString.find_first_of(delim, lastPos);
    }
	return tokens;
}

void ColorRGBToHLS(COLORREF rgb, WORD* hue, WORD* luminance, WORD* saturation)
{
	float h = 0;
	float s = 0;
	float l = 0;
	QColor(GetRValue(rgb), GetGValue(rgb), GetBValue(rgb)).getHslF(&h, &s, &l);
	*hue = static_cast<WORD>(qRound(std::max(h, 0.0f) * 240));
	*luminance = static_cast<WORD>(qRound(l * 240));
	*saturation = static_cast<WORD>(qRound(s * 240));
}

COLORREF ColorHLSToRGB(WORD hue, WORD luminance, WORD saturation)
{
	const auto unit = [](int value) { return std::clamp(value, 0, 240) / 240.0f; };
	const QColor color = QColor::fromHslF(std::fmod(static_cast<float>(hue) / 240.0f, 1.0f), unit(saturation), unit(luminance));
	return RGB(color.red(), color.green(), color.blue());
}

bool isInList(const wchar_t* token, const wchar_t* list)
{
	if ((!token) || (!list))
		return false;

	static constexpr size_t wordLen = 64;
	const size_t listLen = std::wcslen(list);

	wchar_t word[wordLen] = { L'\0' };
	size_t i = 0;
	size_t j = 0;

	for (; i <= listLen; ++i)
	{
		if ((list[i] == L' ') || (list[i] == L'\0'))
		{
			if (j != 0)
			{
				word[j] = '\0';
				j = 0;

				if (!::_wcsicmp(token, word))
					return true;
			}
		}
		else
		{
			word[j] = list[i];
			++j;

			if (j >= wordLen)
				return false;
		}
	}
	return false;
}


wstring purgeMenuItemString(const wchar_t * menuItemStr, bool keepAmpersand)
{
	static constexpr size_t cleanedNameLen = 64;
	wchar_t cleanedName[cleanedNameLen] = L"";
	size_t j = 0;
	size_t menuNameLen = lstrlen(menuItemStr);
	if (menuNameLen >= cleanedNameLen)
		menuNameLen = cleanedNameLen - 1;

	for (size_t k = 0 ; k < menuNameLen ; ++k)
	{
		if (menuItemStr[k] == '\t')
		{
			cleanedName[k] = 0;
			break;
		}
		else
		{
			if (menuItemStr[k] == '&')
			{
				if (keepAmpersand)
					cleanedName[j++] = menuItemStr[k];
				//else skip
			}
			else
				cleanedName[j++] = menuItemStr[k];
		}
	}

	cleanedName[j] = 0;
	return cleanedName;
}

// ---- Code page conversion. Qt's codecs stand in for MultiByteToWideChar and WideCharToMultiByte.
// wchar_t is UTF-32 here, but lengths are always in wchar_t units, so callers are unaffected.
namespace
{
	// A single-byte code page with Windows' table (see WindowsCodePages.h), as Qt's differ in places and lack some. The bytes
	// Windows calls undefined have private-use characters, which convert back to the same bytes, so that saving keeps them.
	class WindowsSingleByteCodec final : public QTextCodec
	{
	public:
		explicit WindowsSingleByteCodec(const WindowsCodePage& table) : _table(table)
		{
			for (int i = 0; i < 128; ++i)
				_bytes.emplace(table.high[i], static_cast<char>(0x80 + i));
		}

		QByteArray name() const override
		{
			return "npp-windows-" + QByteArray::number(_table.codePage);
		}

		int mibEnum() const override
		{
			return -static_cast<int>(_table.codePage);
		}

	protected:
		QString convertToUnicode(const char* in, int length, ConverterState*) const override
		{
			QString out(length, Qt::Uninitialized);
			QChar* dest = out.data();
			for (int i = 0; i < length; ++i)
			{
				const auto byte = static_cast<unsigned char>(in[i]);
				dest[i] = (byte < 0x80) ? QChar(byte) : QChar(_table.high[byte - 0x80]);
			}
			return out;
		}

		// As WideCharToMultiByte with its default character: what the code page lacks becomes '?'
		QByteArray convertFromUnicode(const QChar* in, int length, ConverterState* state) const override
		{
			QByteArray out;
			out.reserve(length);
			for (int i = 0; i < length; ++i)
			{
				const char16_t c = in[i].unicode();
				if (c < 0x80)
				{
					out += static_cast<char>(c);
					continue;
				}

				const auto found = _bytes.find(c);
				if (found != _bytes.end())
				{
					out += found->second;
					continue;
				}

				out += '?';
				if (state)
					++state->invalidChars;
				if (QChar::isHighSurrogate(c) && i + 1 < length && in[i + 1].isLowSurrogate())
					++i; // one character
			}
			return out;
		}

	private:
		const WindowsCodePage& _table;
		std::unordered_map<char16_t, char> _bytes;
	};

	QTextCodec* windowsCodecFor(UINT codepage)
	{
		// Qt takes ownership of the codecs made, which live as long as the application
		static std::map<UINT, QTextCodec*> codecs;
		const auto found = codecs.find(codepage);
		if (found != codecs.end())
			return found->second;

		for (const WindowsCodePage& table : windowsCodePages)
		{
			if (table.codePage == codepage)
				return codecs[codepage] = new WindowsSingleByteCodec(table);
		}
		return codecs[codepage] = nullptr;
	}

	QTextCodec* codecFor(UINT codepage)
	{
		if (codepage == CP_ACP)
			return QTextCodec::codecForLocale();
		if (codepage == CP_UTF8)
			return QTextCodec::codecForMib(106);
		if (QTextCodec* windows = windowsCodecFor(codepage))
			return windows;

		// The Windows code pages EncodingMapper can produce, by the names Qt's codecs know them under.
		const char* name = nullptr;
		switch (codepage)
		{
			case 437: name = "IBM437"; break;
			case 720: name = "IBM720"; break;
			case 737: name = "IBM737"; break;
			case 775: name = "IBM775"; break;
			case 850: name = "IBM850"; break;
			case 852: name = "IBM852"; break;
			case 855: name = "IBM855"; break;
			case 857: name = "IBM857"; break;
			case 858: name = "IBM00858"; break;
			case 860: name = "IBM860"; break;
			case 861: name = "IBM861"; break;
			case 862: name = "IBM862"; break;
			case 863: name = "IBM863"; break;
			case 865: name = "IBM865"; break;
			case 866: name = "IBM866"; break;
			case 869: name = "IBM869"; break;
			case 874: name = "windows-874"; break;
			case 932: name = "Shift_JIS"; break;
			case 936: name = "GBK"; break;
			case 949: name = "windows-949"; break;
			case 950: name = "Big5"; break;
			case 1200: name = "UTF-16LE"; break;
			case 1201: name = "UTF-16BE"; break;
			case 1250: name = "windows-1250"; break;
			case 1251: name = "windows-1251"; break;
			case 1252: name = "windows-1252"; break;
			case 1253: name = "windows-1253"; break;
			case 1254: name = "windows-1254"; break;
			case 1255: name = "windows-1255"; break;
			case 1256: name = "windows-1256"; break;
			case 1257: name = "windows-1257"; break;
			case 1258: name = "windows-1258"; break;
			case 1361: name = "windows-1361"; break;
			case 10000: name = "macintosh"; break;
			case 20127: name = "US-ASCII"; break;
			case 20866: name = "KOI8-R"; break;
			case 21866: name = "KOI8-U"; break;
			case 28591: name = "ISO-8859-1"; break;
			case 28592: name = "ISO-8859-2"; break;
			case 28593: name = "ISO-8859-3"; break;
			case 28594: name = "ISO-8859-4"; break;
			case 28595: name = "ISO-8859-5"; break;
			case 28596: name = "ISO-8859-6"; break;
			case 28597: name = "ISO-8859-7"; break;
			case 28598: name = "ISO-8859-8"; break;
			case 28599: name = "ISO-8859-9"; break;
			case 28601: name = "ISO-8859-11"; break;
			case 28603: name = "ISO-8859-13"; break;
			case 28604: name = "ISO-8859-14"; break;
			case 28605: name = "ISO-8859-15"; break;
			case 28606: name = "ISO-8859-16"; break;
			case 50220: name = "ISO-2022-JP"; break;
			case 51932: name = "EUC-JP"; break;
			case 51949: name = "EUC-KR"; break;
			case 54936: name = "GB18030"; break;
			default: break;
		}
		QTextCodec* codec = name ? QTextCodec::codecForName(name) : nullptr;
		return codec ? codec : QTextCodec::codecForLocale();
	}

	QString decode(UINT codepage, const char* text, int len)
	{
		const int n = (len < 0) ? static_cast<int>(std::strlen(text)) : len;
		return codecFor(codepage)->toUnicode(text, n);
	}

	// True when the bytes convert without replacement characters (MB_ERR_INVALID_CHARS).
	bool decodesCleanly(UINT codepage, const char* text, int len, QString& out)
	{
		QTextCodec::ConverterState state;
		out = codecFor(codepage)->toUnicode(text, len, &state);
		return state.invalidChars == 0 && state.remainingChars == 0;
	}

	QByteArray encodeText(UINT codepage, const wchar_t* text, int len)
	{
		const QString s = (len < 0) ? QString::fromWCharArray(text) : QString::fromWCharArray(text, len);
		return codecFor(codepage)->fromUnicode(s);
	}

	int wideLength(const QString& s)
	{
		return static_cast<int>(s.toStdWString().size());
	}
}

const wchar_t* WcharMbcsConvertor::char2wchar(const char* mbcs2Convert, size_t codepage, int lenMbcs, int* pLenWc, int* pBytesNotProcessed)
{
	// Do not process NULL pointer
	if (!mbcs2Convert)
		return nullptr;

	// Do not process empty strings
	if (lenMbcs == 0 || (lenMbcs == -1 && mbcs2Convert[0] == 0))
	{
		_wideCharStr.empty();
		return _wideCharStr;
	}

	const UINT cp = static_cast<UINT>(codepage);
	int bytesNotProcessed = 0;
	QString decoded;

	if (lenMbcs == -1)
	{
		// If length not specified, simply convert without checking
		decoded = decode(cp, mbcs2Convert, -1);
	}
	else if (cp == CP_UTF8) // For UTF-8, we know how to test if a multi-byte character is cut at the end
	{
		const int indexOfLastChar = Utf8::characterStart(mbcs2Convert, lenMbcs - 1); // get index of last character
		if (indexOfLastChar != 0 && !Utf8::isValid(mbcs2Convert + indexOfLastChar, lenMbcs - indexOfLastChar)) // if it is not valid we do not process it right now (unless its the only character in string, to ensure that we always progress, e.g. that bytesNotProcessed < lenMbcs)
			bytesNotProcessed = lenMbcs - indexOfLastChar;
		decoded = decode(cp, mbcs2Convert, lenMbcs - bytesNotProcessed);
	}
	else // For other encodings, check for invalid characters; a cut last character is one of them
	{
		if (!decodesCleanly(cp, mbcs2Convert, lenMbcs, decoded))
		{
			QString withoutLastByte;
			if (lenMbcs > 1 && decodesCleanly(cp, mbcs2Convert, lenMbcs - 1, withoutLastByte))
			{
				// We found a valid text by removing one byte.
				bytesNotProcessed = 1;
				decoded = withoutLastByte;
			}
			// else: still load the file, invalid characters have been replaced
		}
	}

	const std::wstring wide = decoded.toStdWString();
	const int lenWc = static_cast<int>(wide.size());
	if (lenWc > 0)
	{
		_wideCharStr.sizeTo(lenWc);
		std::wmemcpy(_wideCharStr, wide.data(), wide.size());
		_wideCharStr[lenWc] = L'\0';
	}
	else
		_wideCharStr.empty();

	if (pLenWc)
		*pLenWc = lenWc;
	if (pBytesNotProcessed)
		*pBytesNotProcessed = bytesNotProcessed;

	return _wideCharStr;
}


// "mstart" and "mend" are pointers to indexes in mbcs2Convert,
// which are converted to the corresponding indexes in the returned wchar_t string.
const wchar_t* WcharMbcsConvertor::char2wchar(const char* mbcs2Convert, size_t codepage, intptr_t* mstart, intptr_t* mend, int mbcsLen)
{
	// Do not process NULL pointer
	if (!mbcs2Convert) return NULL;

	if (mbcsLen == 0 || (mbcsLen == -1 && mbcs2Convert[0] == 0))
	{
		_wideCharStr.empty();
		*mstart = 0;
		*mend = 0;
		return _wideCharStr;
	}

	const UINT cp = static_cast<UINT>(codepage);
	const intptr_t bytes = (mbcsLen > 0) ? mbcsLen : static_cast<intptr_t>(std::strlen(mbcs2Convert));
	const std::wstring wide = decode(cp, mbcs2Convert, static_cast<int>(bytes)).toStdWString();
	const int len = static_cast<int>(wide.size());

	if (len > 0)
	{
		_wideCharStr.sizeTo(len);
		std::wmemcpy(_wideCharStr, wide.data(), wide.size());
		_wideCharStr[len] = L'\0';

		if (*mstart < bytes && *mend <= bytes)
		{
			*mstart = wideLength(decode(cp, mbcs2Convert, static_cast<int>(*mstart)));
			*mend = wideLength(decode(cp, mbcs2Convert, static_cast<int>(*mend)));
			if (*mstart >= len || *mend > len)
			{
				*mstart = 0;
				*mend = 0;
			}
		}
	}
	else
	{
		_wideCharStr.empty();
		*mstart = 0;
		*mend = 0;
	}
	return _wideCharStr;
}


const char* WcharMbcsConvertor::wchar2char(const wchar_t* wcharStr2Convert, size_t codepage, int lenWc, int* pLenMbcs)
{
	if (!wcharStr2Convert)
		return nullptr;

	if (lenWc == 0 || (lenWc == -1 && wcharStr2Convert[0] == 0))
	{
		_multiByteStr.empty();
		return _multiByteStr;
	}

	const QByteArray bytes = encodeText(static_cast<UINT>(codepage), wcharStr2Convert, lenWc);
	const int lenMbcs = static_cast<int>(bytes.size());
	if (lenMbcs > 0)
	{
		_multiByteStr.sizeTo(lenMbcs);
		std::memcpy(_multiByteStr, bytes.constData(), bytes.size());
		_multiByteStr[lenMbcs] = '\0';
	}
	else
		_multiByteStr.empty();

	if (pLenMbcs)
		*pLenMbcs = lenMbcs;
	return _multiByteStr;
}


const char* WcharMbcsConvertor::wchar2char(const wchar_t* wcharStr2Convert, size_t codepage, intptr_t* mstart, intptr_t* mend, int wcharLenIn, int* lenOut)
{
	if (!wcharStr2Convert)
		return nullptr;

	if (wcharLenIn == 0 || (wcharLenIn == -1 && wcharStr2Convert[0] == 0))
	{
		_multiByteStr.empty();
		*mstart = 0;
		*mend = 0;
		return _multiByteStr;
	}

	const UINT cp = static_cast<UINT>(codepage);
	const intptr_t wcharLen = (wcharLenIn > 0) ? wcharLenIn : static_cast<intptr_t>(std::wcslen(wcharStr2Convert));
	const QByteArray bytes = encodeText(cp, wcharStr2Convert, static_cast<int>(wcharLen));
	const int len = static_cast<int>(bytes.size());

	if (len > 0)
	{
		_multiByteStr.sizeTo(len);
		std::memcpy(_multiByteStr, bytes.constData(), bytes.size());
		_multiByteStr[len] = '\0';

		if (*mstart < wcharLen && *mend < wcharLen)
		{
			*mstart = encodeText(cp, wcharStr2Convert, static_cast<int>(*mstart)).size();
			*mend = encodeText(cp, wcharStr2Convert, static_cast<int>(*mend)).size();
			if (*mstart >= len || *mend > len)
			{
				*mstart = 0;
				*mend = 0;
			}
		}
	}
	else
	{
		_multiByteStr.empty();
		*mstart = 0;
		*mend = 0;
	}

	if (lenOut)
		*lenOut = len;
	return _multiByteStr;
}


std::wstring string2wstring(const std::string& rString, UINT codepage)
{
	return decode(codepage, rString.c_str(), -1).toStdWString();
}

std::string wstring2string(const std::wstring& rwString, UINT codepage)
{
	return encodeText(codepage, rwString.c_str(), -1).toStdString();
}


// Escapes ampersands in file name to use it in menu
template <typename T>
wstring convertFileName(T beg, T end)
{
	wstring strTmp;

	for (T it = beg; it != end; ++it)
	{
		if (*it == '&') strTmp.push_back('&');
		strTmp.push_back(*it);
	}

	return strTmp;
}

namespace
{
	size_t fileNameOffset(const wstring& path)
	{
		const size_t pos = path.find_last_of(nppPathSepStr);
		return (pos == wstring::npos) ? 0 : pos + 1;
	}
}

wstring compactPath(const wstring& path, size_t maxLen)
{
	if (path.size() <= maxLen)
		return path;

	static const wstring ellipsis = L"...";
	if (maxLen <= ellipsis.size())
		return ellipsis.substr(0, maxLen);

	const size_t nameOffset = fileNameOffset(path);
	if (nameOffset == 0) // no directory part: cut the end, as PathCompactPathEx does
		return path.substr(0, maxLen - ellipsis.size()) + ellipsis;

	const wstring name = path.substr(nameOffset);
	if (name.size() + ellipsis.size() + 1 >= maxLen)
		return ellipsis + nppPathSepStr + name.substr(0, maxLen > ellipsis.size() + 1 ? maxLen - ellipsis.size() - 1 : 0);

	return path.substr(0, maxLen - name.size() - ellipsis.size() - 1) + ellipsis + nppPathSepStr + name;
}

// Build Recent File menu entries from given
wstring BuildMenuFileName(int filenameLen, unsigned int pos, const wstring &filename, bool ordinalNumber)
{
	wstring strTemp;

	if (ordinalNumber)
	{
		if (pos < 9)
		{
			strTemp.push_back('&');
			strTemp.push_back('1' + static_cast<wchar_t>(pos));
		}
		else if (pos == 9)
		{
			strTemp.append(L"1&0");
		}
		else
		{
			div_t splitDigits = div(pos + 1, 10);
			strTemp.append(std::to_wstring(splitDigits.quot));
			strTemp.push_back('&');
			strTemp.append(std::to_wstring(splitDigits.rem));
		}
		strTemp.append(L": ");
	}
	else
	{
		strTemp.push_back('&');
	}

	if (filenameLen > 0)
	{
		const wstring compacted = compactPath(filename, static_cast<size_t>(filenameLen));
		strTemp.append(convertFileName(compacted.begin(), compacted.end()));
	}
	else
	{
		// (filenameLen < 0)
		wstring::const_iterator it = filename.begin();

		if (filenameLen == 0)
			it += fileNameOffset(filename);

		// MAX_PATH is still here to keep old trimming behaviour.
		if (filename.end() - it < MAX_PATH)
		{
			strTemp.append(convertFileName(it, filename.end()));
		}
		else
		{
			strTemp.append(convertFileName(it, it + MAX_PATH / 2 - 3));
			strTemp.append(L"...");
			strTemp.append(convertFileName(filename.end() - MAX_PATH / 2, filename.end()));
		}
	}

	return strTemp;
}

wstring pathRemoveFileSpec(wstring& path)
{
	const wstring::size_type lastSep = path.find_last_of(nppPathSep);
	if (lastSep == wstring::npos)
	{
#ifdef _WIN32
		if (path.size() >= 2 && path[1] == L':')  // "C:foo.bar" becomes "C:"
		{
			path.erase(2);
			return path;
		}
#endif
		path.erase();
	}
	else
	{
#ifdef _WIN32
		if (lastSep == 2 && path[1] == L':' && path.size() >= 3)  // "C:\\foo.exe" becomes "C:\\"
			path.erase(3);
		else
#endif
		if (lastSep == 0 && path.size() > 1) // "/foo.exe" becomes "/"
			path.erase(1);
		else
			path.erase(lastSep);
	}
	return path;
}


wstring pathAppend(wstring& strDest, const wstring& str2append)
{
	if (strDest.empty() && str2append.empty()) // "" + ""
	{
		strDest = nppPathSepStr;
		return strDest;
	}

	if (strDest.empty() && !str2append.empty()) // "" + titi
	{
		strDest = str2append;
		return strDest;
	}

	if (strDest[strDest.length() - 1] == nppPathSep && (!str2append.empty() && str2append[0] == nppPathSep)) // toto/ + /titi
	{
		strDest.erase(strDest.length() - 1, 1);
		strDest += str2append;
		return strDest;
	}

	if ((strDest[strDest.length() - 1] == nppPathSep && (!str2append.empty() && str2append[0] != nppPathSep)) // toto/ + titi
		|| (strDest[strDest.length() - 1] != nppPathSep && (!str2append.empty() && str2append[0] == nppPathSep))) // toto + /titi
	{
		strDest += str2append;
		return strDest;
	}

	// toto + titi
	strDest += nppPathSepStr;
	strDest += str2append;

	return strDest;
}


std::wstring stringToUpper(std::wstring strToConvert)
{
	std::transform(strToConvert.begin(), strToConvert.end(), strToConvert.begin(),
		[](auto ch) { return std::toupper(ch, getSysLocale()); });
	return strToConvert;
}

std::wstring stringToLower(std::wstring strToConvert)
{
	std::transform(strToConvert.begin(), strToConvert.end(), strToConvert.begin(),
		[](auto ch) { return std::tolower(ch, getSysLocale()); });
	return strToConvert;
}


wstring stringReplace(wstring subject, const wstring& search, const wstring& replace)
{
	size_t pos = 0;
	while ((pos = subject.find(search, pos)) != std::string::npos)
	{
		subject.replace(pos, search.length(), replace);
		pos += replace.length();
	}
	return subject;
}


void stringSplit(const wstring& input, const wstring& delimiter, std::vector<wstring>& output)
{
	size_t start = 0U;
	size_t end = input.find(delimiter);
	const size_t delimiterLength = delimiter.length();
	while (end != std::string::npos)
	{
		output.push_back(input.substr(start, end - start));
		start = end + delimiterLength;
		end = input.find(delimiter, start);
	}
	output.push_back(input.substr(start, end));
}


bool str2numberVector(wstring str2convert, std::vector<size_t>& numVect)
{
	numVect.clear();

	for (auto i : str2convert)
	{
		switch (i)
		{
		case ' ':
		case '0': case '1':	case '2': case '3':	case '4':
		case '5': case '6':	case '7': case '8':	case '9':
		{
			// correct. do nothing
		}
		break;

		default:
			return false;
		}
	}

	std::vector<wstring> v;
	stringSplit(str2convert, L" ", v);
	for (const auto& i : v)
	{
		// Don't treat empty string and the number greater than 9999
		if (!i.empty() && i.length() < 5)
		{
			numVect.push_back(std::stoi(i));
		}
	}
	return true;
}

void stringJoin(const std::vector<wstring>& strings, const wstring& separator, wstring& joinedString)
{
	size_t length = strings.size();
	for (size_t i = 0; i < length; ++i)
	{
		joinedString += strings.at(i);
		if (i != length - 1)
		{
			joinedString += separator;
		}
	}
}


wstring stringTakeWhileAdmissable(const wstring& input, const wstring& admissable)
{
	// Find first non-admissable character in "input", and remove everything after it.
	size_t idx = input.find_first_not_of(admissable);
	if (idx == std::string::npos)
	{
		return input;
	}
	else
	{
		return input.substr(0, idx);
	}
}

double stodLocale(const wstring& str, [[maybe_unused]] _locale_t loc, size_t* idx)
{
	// The callers pass the classic locale, which is what the stream is imbued with here.
	std::wistringstream stream(str);
	stream.imbue(std::locale::classic());
	double ans = 0;
	stream >> ans;
	if (stream.fail())
	{
		if (ans != 0)
			throw std::out_of_range("stod argument out of range");
		throw std::invalid_argument("invalid stod argument");
	}
	if (idx != NULL)
	{
		const auto pos = stream.tellg();
		*idx = (pos < 0) ? str.size() : static_cast<size_t>(pos);
	}
	return ans;
}

const std::locale& getSysLocale()
{
	static const auto systemLocale = []()
	{
		try {
			return std::locale("");
		}
		catch (const std::runtime_error&)
		{
			return std::locale::classic();
		}
	}();
	return systemLocale;
}

bool str2Clipboard(const wstring &str2cpy, [[maybe_unused]] HWND hwnd)
{
	QClipboard* clipboard = QGuiApplication::clipboard();
	if (!clipboard)
		return false;
	clipboard->setText(toQ(str2cpy));
	return true;
}

std::wstring strFromClipboard()
{
	std::wstring clipboardText;
	if (const QClipboard* clipboard = QGuiApplication::clipboard())
		clipboardText = toW(clipboard->text());

	// trim the EOL at the end of string if any
	// 1: X, \r, \n  => don't change
	if (clipboardText.length() < 2)
		return clipboardText;

	// 2: XX, \nX, \rX, X\r, X\n, \r\n, \r\r, \n\n, \n\r
	if (clipboardText.length() == 2)
	{
		if (clipboardText[1] != '\r' && clipboardText[1] != '\n') // XX, \nX, \rX  => don't change 
			return clipboardText;

		if (clipboardText[0] == '\r' && clipboardText[1] == '\n') // \r\n  => don't change
			return clipboardText;

		if (clipboardText[0] == '\r' && clipboardText[1] == '\r') // \r\r  => don't change
			return clipboardText;

		if (clipboardText[0] == '\n' && clipboardText[1] == '\n') // \n\n  => don't change
			return clipboardText;

		if (clipboardText[0] != '\r' && clipboardText[0] != '\n') // X\n, X\r  => remove \n or \r
		{
			wchar_t trimedResult[2]{};
			trimedResult[0] = clipboardText[0];
			trimedResult[1] = '\0';
			return trimedResult;
		}

		return clipboardText; // \n\r (unlikely happen, but if it does, keep them)
	}
	else // length >= 3    => Remove the EOL at the end of string
	{
		size_t posEol = clipboardText.length() - 2;
		if (clipboardText.substr(posEol) == L"\r\n")
			clipboardText.erase(posEol, 2);
		else if (clipboardText.substr(posEol + 1) == L"\r" || clipboardText.substr(posEol + 1) == L"\n")
			clipboardText.erase(posEol + 1, 1);
	}

	return clipboardText;
}


namespace
{
	// PathMatchSpec: '*' and '?' wildcards, case-insensitive even on case-sensitive file systems, so
	// Find in Files filters behave as in the original. "*.*" matches every name, with or without a dot.
	bool wildcardMatch(const wchar_t* name, const wchar_t* spec)
	{
		const wchar_t* star = nullptr;
		const wchar_t* afterStar = nullptr;
		while (*name)
		{
			if (*spec == L'*')
			{
				star = spec++;
				afterStar = name;
			}
			else if (*spec == L'?' || (*spec && std::towlower(*spec) == std::towlower(*name)))
			{
				++spec;
				++name;
			}
			else if (star)
			{
				spec = star + 1;
				name = ++afterStar;
			}
			else
			{
				return false;
			}
		}
		while (*spec == L'*')
			++spec;
		// "*." and "*.*" also accept a name without a dot
		if (*spec == L'.' && (spec[1] == L'\0' || (spec[1] == L'*' && spec[2] == L'\0')))
			return true;
		return *spec == L'\0';
	}
}

bool pathMatchSpec(const wchar_t* fileName, const wchar_t* spec)
{
	return wildcardMatch(fileName, spec);
}

bool matchInList(const wchar_t *fileName, const std::vector<wstring> & patterns)
{
	bool is_matched = false;
	for (size_t i = 0, len = patterns.size(); i < len; ++i)
	{
		if (patterns[i].length() > 1 && patterns[i][0] == '!')
		{
			if (pathMatchSpec(fileName, patterns[i].c_str() + 1))
				return false;

			continue;
		} 

		if (pathMatchSpec(fileName, patterns[i].c_str()))
			is_matched = true;
	}
	return is_matched;
}

bool matchInExcludeDirList(const wchar_t* dirName, const std::vector<wstring>& patterns, size_t level)
{
	// The original syntax is !\\dir and !+\\dir. Accept the forward slash as well on this platform.
	const auto isSep = [](wchar_t c) { return c == L'\\' || c == L'/'; };
	for (size_t i = 0, len = patterns.size(); i < len; ++i)
	{
		size_t patterLen = patterns[i].length();

		if (patterLen > 3 && patterns[i][0] == '!' && patterns[i][1] == '+' && isSep(patterns[i][2])) // check for !+\\folderPattern: for all levels - search this pattern recursively
		{
			if (pathMatchSpec(dirName, patterns[i].c_str() + 3))
				return true;
		}
		else if (patterLen > 2 && patterns[i][0] == '!' && isSep(patterns[i][1])) // check for !\\folderPattern: exclusive pattern for only the 1st level
		{
			if (level == 1)
				if (pathMatchSpec(dirName, patterns[i].c_str() + 2))
					return true;
		}
	}
	return false;
}

bool allPatternsAreExclusion(const std::vector<wstring>& patterns)
{
	bool oneInclusionPatternFound = false;
	for (size_t i = 0, len = patterns.size(); i < len; ++i)
	{
		if (patterns[i][0] != '!')
		{
			oneInclusionPatternFound = true;
			break;
		}
	}
	return !oneInclusionPatternFound;
}

wstring GetLastErrorAsString(DWORD errorCode)
{
	// If both error codes (passed error n GetLastError) are 0, then return empty
	if (errorCode == 0)
		errorCode = GetLastError();
	if (errorCode == 0)
		return L""; //No error message has been recorded

	return toW(QString::fromLocal8Bit(std::strerror(static_cast<int>(errorCode))));
}

bool isAssoCommandExisting(LPCWSTR FullPathName)
{
	// Whether the desktop can open this file with another application: a file manager association always
	// exists for an existing file on Linux, and it is handled by xdg-open.
	return doesFileExist(FullPathName);
}

bool deleteFileOrFolder(const wstring& f2delete)
{
	// To the trash, as the original sends it to the recycle bin (FOF_ALLOWUNDO).
	return QFile::moveToTrash(toQ(f2delete));
}

// Get a vector of full file paths in a given folder. File extension type filter should be *.*, *.xml, *.so... according to the type of file you want to get.  
void getFilesInFolder(std::vector<wstring>& files, const wstring& extTypeFilter, const wstring& inFolder)
{
	const QStringList names = QDir(toQ(inFolder)).entryList(QDir::Files | QDir::Hidden | QDir::System, QDir::Name);
	for (const QString& name : names)
	{
		const wstring fileName = toW(name);
		if (pathMatchSpec(fileName.c_str(), extTypeFilter.c_str()))
		{
			wstring foundFullPath = inFolder;
			pathAppend(foundFullPath, fileName);
			files.push_back(foundFullPath);
		}
	}
}

// remove any leading or trailing spaces from str
void trim(std::wstring& str)
{
	std::wstring::size_type pos = str.find_last_not_of(' ');

	if (pos != std::wstring::npos)
	{
		str.erase(pos + 1);
		pos = str.find_first_not_of(' ');
		if (pos != std::wstring::npos) str.erase(0, pos);
	}
	else str.erase(str.begin(), str.end());
}

int nbDigitsFromNbLines(size_t nbLines)
{
	int nbDigits = 0; // minimum number of digit should be 4
	if (nbLines < 10) nbDigits = 1;
	else if (nbLines < 100) nbDigits = 2;
	else if (nbLines < 1000) nbDigits = 3;
	else if (nbLines < 10000) nbDigits = 4;
	else if (nbLines < 100000) nbDigits = 5;
	else if (nbLines < 1000000) nbDigits = 6;
	else // rare case
	{
		nbDigits = 7;
		nbLines /= 10000000;

		while (nbLines)
		{
			nbLines /= 10;
			++nbDigits;
		}
	}
	return nbDigits;
}

wstring getDateTimeStrFrom(const wstring& dateTimeFormat, const SYSTEMTIME& st)
{
	const QDateTime dateTime(QDate(st.wYear, st.wMonth, st.wDay), QTime(st.wHour, st.wMinute, st.wSecond));

	// The specifiers of GetDateFormatEx and GetTimeFormatEx are the same as QDateTime's, except:
	// tt is the AM/PM string and t its first letter, while g and gg are the era, which is dropped.
	QString format;
	const QString input = toQ(dateTimeFormat);
	bool inQuote = false;
	for (qsizetype i = 0; i < input.size(); ++i)
	{
		const QChar c = input[i];
		if (c == u'\'')
		{
			inQuote = !inQuote;
			format += c;
		}
		else if (!inQuote && c == u't')
		{
			if (i + 1 < input.size() && input[i + 1] == u't')
			{
				format += QStringLiteral("AP");
				++i;
			}
			else
			{
				format += QStringLiteral("A");
			}
		}
		else if (!inQuote && c == u'g')
		{
			// era: not supported by QDateTime
		}
		else
		{
			format += c;
		}
	}
	return toW(QLocale::system().toString(dateTime, format));
}

bool removeReadOnlyFlagFromFileAttributes(const wchar_t* fileFullPath)
{
	const QString path = toQ(fileFullPath);
	if (!QFileInfo(path).isFile())
		return false;
	return QFile::setPermissions(path, QFile::permissions(path) | QFile::WriteOwner | QFile::WriteUser);
}

// return false when failed, otherwise true and then the isChangedToReadOnly output will be set
// accordingly to the changed file R/O-state
bool toggleReadOnlyFlagFromFileAttributes(const wchar_t* fileFullPath, bool& isChangedToReadOnly)
{
	const QString path = toQ(fileFullPath);
	if (!QFileInfo(path).isFile())
		return false;

	static constexpr QFile::Permissions writeBits = QFile::WriteOwner | QFile::WriteUser | QFile::WriteGroup | QFile::WriteOther;
	QFile::Permissions permissions = QFile::permissions(path);
	const bool wasWritable = (permissions & (QFile::WriteOwner | QFile::WriteUser)) != 0;
	if (wasWritable)
		permissions &= ~writeBits;
	else
		permissions |= QFile::WriteOwner | QFile::WriteUser;

	if (QFile::setPermissions(path, permissions) || nppElevatedSetWritable(fileFullPath, !wasWritable) == NO_ERROR)
	{
		isChangedToReadOnly = wasWritable;
		return true;
	}
	return false;
}

bool isWin32NamespacePrefixedFileName(const wstring& fileName)
{
	return (fileName.starts_with(L"\\\\?\\") || fileName.starts_with(L"//?/"));
}

bool isWin32NamespacePrefixedFileName(const wchar_t* szFileName)
{
	const wstring fileName = szFileName;
	return isWin32NamespacePrefixedFileName(fileName);
}

// Only length is a limit here. The reserved device names and characters of Win32 are legal in POSIX names.
bool isUnsupportedFileName(const wstring& fileName)
{
	return fileName.empty() || fileName.size() >= MAX_PATH;
}

bool isUnsupportedFileName(const wchar_t* szFileName)
{
	const std::wstring fileName = szFileName;
	return isUnsupportedFileName(fileName);
}

// UNC paths do not exist on this platform
bool isUncPath([[maybe_unused]] const std::wstring& path)
{
	return false;
}

bool isUncFileUrl(const std::wstring& url)
{
	// A file URL naming a remote host. Scheme match must be case-insensitive
	if (url.size() < 7 || _wcsnicmp(url.c_str(), L"file://", 7) != 0)
		return false;

	const size_t hostStart = 7; // after "file://"
	const size_t hostEnd = url.find_first_of(L"/\\", hostStart);
	const std::wstring host = (hostEnd == std::wstring::npos) ? url.substr(hostStart) : url.substr(hostStart, hostEnd - hostStart);

	if (host.empty() || host == L".")
		return false;

	if (_wcsicmp(host.c_str(), L"localhost") == 0)
		return false;

	if (host == L"127.0.0.1" || host == L"::1" || host == L"[::1]")
		return false;

	return true;
}

Version::Version(const wstring& versionStr)
{
	try {
		auto ss = tokenizeString(versionStr, '.');

		if (ss.size() > 4)
		{
			std::wstring msg(L"\"");
			msg += versionStr;
			msg += L"\"";
			msg += L": Version parts are more than 4. The string to parse is not a valid version format. Let's make it default value in catch block.";
			throw msg;
		}

		int i = 0;
		std::vector<unsigned long*> v = { &_major, &_minor, &_patch, &_build };
		for (const auto& s : ss)
		{
			if (!isNumber(s))
			{
				std::wstring msg(L"\"");
				msg += versionStr;
				msg += L"\"";
				msg += L": One of version character is not number. The string to parse is not a valid version format. Let's make it default value in catch block.";
				throw msg;
			}
			*(v[i]) = std::stoi(s);

			++i;
		}
	}
#if !defined(NDEBUG) 
	catch (const std::wstring& s)
	{
		_major = 0;
		_minor = 0;
		_patch = 0;
		_build = 0;

		throw s;
	}
#endif
	catch (...)
	{
		_major = 0;
		_minor = 0;
		_patch = 0;
		_build = 0;
#if !defined(NDEBUG) 
		throw std::wstring(L"Unknown exception from \"Version::Version(const wstring& versionStr)\"");
#endif
	}
}

// Windows reads this from a PE file's version resource. Shared libraries have no equivalent.
void Version::setVersionFrom([[maybe_unused]] const wstring& filePath)
{
}

wstring Version::toString() const
{
	if (_build == 0 && _patch == 0 && _minor == 0 && _major == 0) // ""
	{
		return L"";
	}
	else if (_build == 0 && _patch == 0 && _minor == 0) // "major"
	{
		return std::to_wstring(_major);
	}
	else if (_build == 0 && _patch == 0) // "major.minor"
	{
		std::wstring v = std::to_wstring(_major);
		v += L".";
		v += std::to_wstring(_minor);
		return v;
	}
	else if (_build == 0) // "major.minor.patch"
	{
		std::wstring v = std::to_wstring(_major);
		v += L".";
		v += std::to_wstring(_minor);
		v += L".";
		v += std::to_wstring(_patch);
		return v;
	}

	// "major.minor.patch.build"
	std::wstring ver = std::to_wstring(_major);
	ver += L".";
	ver += std::to_wstring(_minor);
	ver += L".";
	ver += std::to_wstring(_patch);
	ver += L".";
	ver += std::to_wstring(_build);

	return ver;
}

int Version::compareTo(const Version& v2c) const
{
	if (_major > v2c._major)
		return 1;
	else if (_major < v2c._major)
		return -1;
	else // (_major == v2c._major)
	{
		if (_minor > v2c._minor)
			return 1;
		else if (_minor < v2c._minor)
			return -1;
		else // (_minor == v2c._minor)
		{
			if (_patch > v2c._patch)
				return 1;
			else if (_patch < v2c._patch)
				return -1;
			else // (_patch == v2c._patch)
			{
				if (_build > v2c._build)
					return 1;
				else if (_build < v2c._build)
					return -1;
				else // (_build == v2c._build)
				{
					return 0;
				}
			}
		}
	}
}

bool Version::isCompatibleTo(const Version& from, const Version& to) const
{
	// This method determines if Version object is in between "from" version and "to" version, it's useful for testing compatibility of application.
	// test in versions <from, to> example: 
	// 1. <0.0.0.0, 0.0.0.0>: both from to versions are empty, so it's 
	// 2. <6.9, 6.9>: plugin is compatible to only v6.9
	// 3. <4.2, 6.6.6>: from v4.2 (included) to v6.6.6 (included)
	// 4. <0.0.0.0, 8.2.1>: all version until v8.2.1 (included)
	// 5. <8.3, 0.0.0.0>: from v8.3 (included) to the latest version
	
	if (empty()) // if this version is empty, then no compatible to all version
		return false;

	if (from.empty() && to.empty()) // both versions "from" and "to" are empty: it's considered compatible, whatever this version is (match to 1)
	{
		return true;
	}

	if (from <= *this && to >= *this) // from_ver <= this_ver <= to_ver (match to 2, 3 and 4)
	{
		return true;
	}
		
	if (from <= *this && to.empty()) // from_ver <= this_ver (match to 5)
	{
		return true;
	}

	return false;
}

static constexpr DWORD DEFAULT_MILLISEC = 3000;

//----------------------------------------------------
// A stat() or statvfs() on a stale network mount can block for a long time, which is why the original
// runs these calls on a worker thread with a timeout. The worker cannot be killed safely, so it detaches
// and owns its state through a shared_ptr, and it simply ends whenever the system call returns.

namespace
{
	FILETIME toFileTime(const QDateTime& time)
	{
		// 100 ns ticks since 1601-01-01, the same scale as on Windows
		static constexpr std::uint64_t unixEpochAsFileTime = 116444736000000000ULL;
		FILETIME result;
		if (!time.isValid())
			return result;
		const std::uint64_t ticks = static_cast<std::uint64_t>(time.toMSecsSinceEpoch()) * 10000ULL + unixEpochAsFileTime;
		result.dwLowDateTime = static_cast<DWORD>(ticks & 0xFFFFFFFFULL);
		result.dwHighDateTime = static_cast<DWORD>(ticks >> 32);
		return result;
	}

	template <typename Result, typename Work>
	bool runWithTimeout(std::shared_ptr<Result> shared, DWORD milliSec2wait, Work work)
	{
		struct Sync
		{
			std::mutex mutex;
			std::condition_variable done;
			bool finished = false;
		};
		auto sync = std::make_shared<Sync>();

		std::thread([shared, sync, work]()
		{
			work(*shared);
			{
				std::lock_guard<std::mutex> lock(sync->mutex);
				sync->finished = true;
			}
			sync->done.notify_all();
		}).detach();

		std::unique_lock<std::mutex> lock(sync->mutex);
		return sync->done.wait_for(lock, std::chrono::milliseconds(milliSec2wait == 0 ? DEFAULT_MILLISEC : milliSec2wait), [&sync]() { return sync->finished; });
	}
}

BOOL getDiskFreeSpaceWithTimeout(const wchar_t* dirPath, ULARGE_INTEGER* freeBytesForUser, DWORD milliSec2wait, bool* isTimeoutReached)
{
	struct Result
	{
		std::uint64_t freeBytes = 0;
		bool valid = false;
	};
	auto result = std::make_shared<Result>();
	const QString path = toQ(dirPath);

	const bool completed = runWithTimeout(result, milliSec2wait, [path](Result& r)
	{
		const QStorageInfo storage(path);
		r.valid = storage.isValid();
		if (r.valid)
			r.freeBytes = static_cast<std::uint64_t>(storage.bytesAvailable());
	});

	if (isTimeoutReached != nullptr)
		*isTimeoutReached = !completed;

	if (!completed || !result->valid)
		return FALSE;

	freeBytesForUser->QuadPart = result->freeBytes;
	return TRUE;
}


BOOL getFileAttributesExWithTimeout(const wchar_t* filePath, WIN32_FILE_ATTRIBUTE_DATA* fileAttr,
	DWORD milliSec2wait, bool* isTimeoutReached, DWORD* pdwWin32ApiError)
{
	struct Result
	{
		WIN32_FILE_ATTRIBUTE_DATA attributes;
		DWORD error = NO_ERROR;
		bool found = false;
	};
	auto result = std::make_shared<Result>();
	result->attributes.dwFileAttributes = INVALID_FILE_ATTRIBUTES;
	const QString path = toQ(filePath);

	const bool completed = runWithTimeout(result, milliSec2wait, [path](Result& r)
	{
		const QFileInfo info(path);
		if (!info.exists())
		{
			r.error = ERROR_FILE_NOT_FOUND;
			return;
		}

		DWORD attributes = info.isDir() ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
		if (!info.isWritable())
			attributes |= FILE_ATTRIBUTE_READONLY;
		if (info.isHidden())
			attributes |= FILE_ATTRIBUTE_HIDDEN;

		r.attributes.dwFileAttributes = attributes;
		r.attributes.ftCreationTime = toFileTime(info.birthTime());
		r.attributes.ftLastAccessTime = toFileTime(info.lastRead());
		r.attributes.ftLastWriteTime = toFileTime(info.lastModified());
		const std::uint64_t size = static_cast<std::uint64_t>(info.size());
		r.attributes.nFileSizeHigh = static_cast<DWORD>(size >> 32);
		r.attributes.nFileSizeLow = static_cast<DWORD>(size & 0xFFFFFFFFULL);
		r.found = true;
	});

	if (isTimeoutReached != nullptr)
		*isTimeoutReached = !completed;

	if (!completed)
		return FALSE;

	*fileAttr = result->attributes;
	if (pdwWin32ApiError != nullptr)
		*pdwWin32ApiError = result->error;

	return result->found ? TRUE : FALSE;
}

bool doesFileExist(const wchar_t* filePath, DWORD milliSec2wait, bool* isTimeoutReached)
{
	WIN32_FILE_ATTRIBUTE_DATA attributes{};
	attributes.dwFileAttributes = INVALID_FILE_ATTRIBUTES;
	getFileAttributesExWithTimeout(filePath, &attributes, milliSec2wait, isTimeoutReached);
	return (attributes.dwFileAttributes != INVALID_FILE_ATTRIBUTES && !(attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
}

bool doesDirectoryExist(const wchar_t* dirPath, DWORD milliSec2wait, bool* isTimeoutReached)
{
	WIN32_FILE_ATTRIBUTE_DATA attributes{};
	attributes.dwFileAttributes = INVALID_FILE_ATTRIBUTES;
	getFileAttributesExWithTimeout(dirPath, &attributes, milliSec2wait, isTimeoutReached);
	return (attributes.dwFileAttributes != INVALID_FILE_ATTRIBUTES && (attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
}

bool doesPathExist(const wchar_t* path, DWORD milliSec2wait, bool* isTimeoutReached)
{
	WIN32_FILE_ATTRIBUTE_DATA attributes{};
	attributes.dwFileAttributes = INVALID_FILE_ATTRIBUTES;
	getFileAttributesExWithTimeout(path, &attributes, milliSec2wait, isTimeoutReached);
	return (attributes.dwFileAttributes != INVALID_FILE_ATTRIBUTES);
}

// check if the window rectangle intersects with any currently active monitor's working area
bool isWindowVisibleOnAnyMonitor(const RECT& rectWndIn)
{
	const QRect window(QPoint(rectWndIn.left, rectWndIn.top), QPoint(rectWndIn.right - 1, rectWndIn.bottom - 1));
	const auto screens = QGuiApplication::screens();
	return std::any_of(screens.begin(), screens.end(), [&window](const QScreen* screen) { return screen->availableGeometry().intersects(window); });
}

bool ControlInfoTip::init(HWND ctrl2attached, const wstring& tipStr, unsigned int remainTimeMillisecond)
{
	_attached = ctrl2attached;
	_tip = toQ(tipStr);
	_remainTime = static_cast<int>(remainTimeMillisecond);
	return _attached != nullptr;
}

void ControlInfoTip::show(showPosition pos) const
{
	if (!isValid())
		return;

	const int width = _attached->width();
	int xPos = width / 2;
	if (pos == beginning)
		xPos = 15;
	else if (pos == end)
		xPos = width - 15;

	QToolTip::showText(_attached->mapToGlobal(QPoint(xPos, 25)), _tip, _attached, QRect(), _remainTime > 0 ? _remainTime : -1);
}

void ControlInfoTip::hide()
{
	if (_attached)
	{
		QToolTip::hideText();
		_attached = nullptr;
	}
}

FILETIME currentFileTime()
{
	return toFileTime(QDateTime::currentDateTimeUtc());
}

wstring fileTimeToLocalString(const FILETIME& ft)
{
	static constexpr std::uint64_t unixEpochAsFileTime = 116444736000000000ULL;
	const std::uint64_t ticks = fileTimeTicks(ft);
	if (ticks < unixEpochAsFileTime)
		return {};
	const QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>((ticks - unixEpochAsFileTime) / 10000ULL));
	return toW(QLocale::system().toString(dateTime, QLocale::ShortFormat));
}

bool fileTimeToYMD(const FILETIME& ft, int& yyyymmdd)
{
	static constexpr std::uint64_t unixEpochAsFileTime = 116444736000000000ULL;
	const std::uint64_t ticks = (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
	if (ticks < unixEpochAsFileTime)
		return false;

	const QDate date = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>((ticks - unixEpochAsFileTime) / 10000ULL)).date();
	yyyymmdd = (date.year() * 10000) + (date.month() * 100) + date.day();
	return true;
}

// Expands %VAR% as the original does, plus the native $VAR and ${VAR} forms and a leading ~/.
void expandEnv(wstring& path2Expand)
{
	QString text = toQ(path2Expand);
	const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

	static const QRegularExpression pattern(QStringLiteral(R"(%([^%/\\\\]+)%|\$\{(\w+)\}|\$(\w+))"));
	QString result;
	qsizetype last = 0;
	auto it = pattern.globalMatch(text);
	while (it.hasNext())
	{
		const QRegularExpressionMatch match = it.next();
		const QString name = !match.captured(1).isEmpty() ? match.captured(1) : (!match.captured(2).isEmpty() ? match.captured(2) : match.captured(3));
		result += text.mid(last, match.capturedStart() - last);
		// An unknown variable stays as written, as with ExpandEnvironmentStrings
		result += env.contains(name) ? env.value(name) : match.captured(0);
		last = match.capturedEnd();
	}
	result += text.mid(last);

	if (result == u'~' || result.startsWith(QStringLiteral("~/")))
		result.replace(0, 1, QDir::homePath());

	path2Expand = toW(result);
}

bool openWithDesktop(const std::wstring& target)
{
	const QString text = toQ(target);
	const QFileInfo info(text);
	return QDesktopServices::openUrl(info.exists() ? QUrl::fromLocalFile(info.absoluteFilePath()) : QUrl(text));
}

std::wstring findTerminal()
{
	const QString chosen = qEnvironmentVariable("TERMINAL");
	if (!chosen.isEmpty())
		return toW(chosen);

	static const char* const terminals[] = { "x-terminal-emulator", "konsole", "gnome-terminal", "kgx", "xfce4-terminal", "alacritty", "kitty", "foot", "lxterminal", "mate-terminal", "xterm" };
	for (const char* terminal : terminals)
	{
		const QString path = QStandardPaths::findExecutable(QString::fromLatin1(terminal));
		if (!path.isEmpty())
			return toW(path);
	}
	return L"xterm";
}

HRESULT openInExplorerAndSelect(const wchar_t* path)
{
	const QString target = toQ(path);
	const QFileInfo info(target);
	if (!info.exists())
		return static_cast<HRESULT>(ERROR_FILE_NOT_FOUND);

	// org.freedesktop.FileManager1 selects the item in file managers that implement it, and the
	// fallback opens the containing folder.
	const QString uri = QUrl::fromLocalFile(info.absoluteFilePath()).toString();
	const QStringList args
	{
		QStringLiteral("--session"), QStringLiteral("--dest=org.freedesktop.FileManager1"), QStringLiteral("--type=method_call"),
		QStringLiteral("/org/freedesktop/FileManager1"), QStringLiteral("org.freedesktop.FileManager1.ShowItems"),
		QStringLiteral("array:string:") + uri, QStringLiteral("string:")
	};
	if (QProcess::startDetached(QStringLiteral("dbus-send"), args))
		return 0;

	const QString folder = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
	return QDesktopServices::openUrl(QUrl::fromLocalFile(folder)) ? 0 : static_cast<HRESULT>(EIO);
}

