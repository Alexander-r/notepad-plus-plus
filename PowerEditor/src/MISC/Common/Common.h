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


#pragma once

#include <QString>

#include "NppPlatform.h"


#include <algorithm>
#include <locale>
#include <string>
#include <unordered_set>
#include <vector>
#include <cstdint>

class QLineEdit;
std::wstring folderBrowser(HWND parent, const std::wstring & title = L"", QLineEdit* outputCtrl = nullptr, const wchar_t *defaultStr = NULL);
std::wstring getFolderName(HWND parent, const wchar_t *defaultDir = NULL);

void printInt(int int2print);
void printStr(const wchar_t *str2print);
std::wstring commafyInt(size_t n);

void writeLog(const wchar_t* logFileName, const char* log2write);
void writeLog(const wchar_t* logFileName, const wchar_t* log2write);
std::wstring purgeMenuItemString(const wchar_t* menuItemStr, bool keepAmpersand = false);
std::vector<std::wstring> tokenizeString(const std::wstring & tokenString, const char delim);


std::wstring string2wstring(const std::string& rString, UINT codepage = CP_UTF8);
std::string wstring2string(const std::wstring& rwString, UINT codepage = CP_UTF8);
// Hue, luminance and saturation on the 0 to 240 scale of the original color settings
void ColorRGBToHLS(COLORREF rgb, WORD* hue, WORD* luminance, WORD* saturation);
COLORREF ColorHLSToRGB(WORD hue, WORD luminance, WORD saturation);

bool isInList(const wchar_t* token, const wchar_t* list);
bool pathMatchSpec(const wchar_t* fileName, const wchar_t* spec);
std::wstring BuildMenuFileName(int filenameLen, unsigned int pos, const std::wstring &filename, bool ordinalNumber = true);

std::string getFileContent(const wchar_t* file2read, bool* pbFailed = nullptr);
std::wstring relativeFilePathToFullFilePath(const wchar_t *relativeFilePath);
void writeFileContent(const wchar_t *file2write, const char *content2write);
DWORD writeFileContentEx(const wchar_t* file2write, const char* content2write, size_t length);
bool matchInList(const wchar_t *fileName, const std::vector<std::wstring> & patterns);
bool matchInExcludeDirList(const wchar_t* dirName, const std::vector<std::wstring>& patterns, size_t level);
bool allPatternsAreExclusion(const std::vector<std::wstring>& patterns);
HRESULT openInExplorerAndSelect(const wchar_t* path);
// As with ShellExecute's "open": a document, a folder or a link in its default application
bool openWithDesktop(const std::wstring& target);
// The user's terminal emulator. The "cmd" menu commands open it in the document's folder.
std::wstring findTerminal();

class WcharMbcsConvertor final
{
public:
	static WcharMbcsConvertor& getInstance() {
		static WcharMbcsConvertor instance;
		return instance;
	}

	const wchar_t* char2wchar(const char* mbcs2Convert, size_t codepage, int lenMbcs = -1, int* pLenWc = nullptr, int* pBytesNotProcessed = NULL);
	const wchar_t* char2wchar(const char* mbcs2Convert, size_t codepage, intptr_t* mstart, intptr_t* mend, int mbcsLen = 0);
	size_t getSizeW() const { return _wideCharStr.size(); }
	const char* wchar2char(const wchar_t* wcharStr2Convert, size_t codepage, int lenWc = -1, int* pLenMbcs = nullptr);
	const char* wchar2char(const wchar_t* wcharStr2Convert, size_t codepage, intptr_t* mstart, intptr_t* mend, int wcharLenIn = 0, int* lenOut = nullptr);
	size_t getSizeA() const { return _multiByteStr.size(); }

	const char* encode(UINT fromCodepage, UINT toCodepage, const char* txt2Encode, int lenIn = -1, int* pLenOut = NULL, int* pBytesNotProcessed = NULL) {
		int lenWc = 0;
		const wchar_t* strW = char2wchar(txt2Encode, fromCodepage, lenIn, &lenWc, pBytesNotProcessed);
		return wchar2char(strW, toCodepage, lenWc, pLenOut);
	}

protected:
	WcharMbcsConvertor() = default;
	~WcharMbcsConvertor() = default;

	// Since there's no public ctor, we need to void the default assignment operator and copy ctor.
	// Since these are marked as deleted does not matter under which access specifier are kept
	WcharMbcsConvertor(const WcharMbcsConvertor&) = delete;
	WcharMbcsConvertor& operator= (const WcharMbcsConvertor&) = delete;

	// No move ctor and assignment
	WcharMbcsConvertor(WcharMbcsConvertor&&) = delete;
	WcharMbcsConvertor& operator= (WcharMbcsConvertor&&) = delete;

	template <class T> class StringBuffer final
	{
	public:
		~StringBuffer() { if (_allocLen) delete[] _str; }

		void sizeTo(size_t size) {
			if (_allocLen < size + 1)
			{
				if (_allocLen)
					delete[] _str;
				_allocLen = std::max<size_t>(size + 1, initSize);
				_str = new T[_allocLen]{};
			}
			_dataLen = size;
		}

		void empty() {
			static T nullStr = 0; // routines may return an empty string, with null terminator, without allocating memory; a pointer to this null character will be returned in that case
			if (_allocLen == 0)
				_str = &nullStr;
			else
				_str[0] = 0;
			_dataLen = 0;
		}

		size_t size() const { return _dataLen; }
		operator T* () { return _str; }
		operator const T* () const { return _str; }

	protected:
		static constexpr int initSize = 1024;
		size_t _allocLen = 0;
		size_t _dataLen = 0;
		T* _str = nullptr;
	};

	StringBuffer<char> _multiByteStr;
	StringBuffer<wchar_t> _wideCharStr;
};

std::wstring pathRemoveFileSpec(std::wstring & path);
std::wstring pathAppend(std::wstring &strDest, const std::wstring & str2append);
std::wstring stringToUpper(std::wstring strToConvert);
std::wstring stringToLower(std::wstring strToConvert);
std::wstring stringReplace(std::wstring subject, const std::wstring& search, const std::wstring& replace);
void stringSplit(const std::wstring& input, const std::wstring& delimiter, std::vector<std::wstring>& output);
bool str2numberVector(std::wstring str2convert, std::vector<size_t>& numVect);
void stringJoin(const std::vector<std::wstring>& strings, const std::wstring& separator, std::wstring& joinedString);
std::wstring stringTakeWhileAdmissable(const std::wstring& input, const std::wstring& admissable);
double stodLocale(const std::wstring& str, _locale_t loc, size_t* idx = NULL);

const std::locale& getSysLocale();

bool str2Clipboard(const std::wstring &str2cpy, HWND hwnd);
std::wstring strFromClipboard();
class Buffer;
bool buf2Clipboard(const std::vector<Buffer*>& buffers, bool isFullPath, HWND hwnd);

std::wstring GetLastErrorAsString(DWORD errorCode = 0);


bool isAssoCommandExisting(LPCWSTR FullPathName);

bool deleteFileOrFolder(const std::wstring& f2delete);

void getFilesInFolder(std::vector<std::wstring>& files, const std::wstring& extTypeFilter, const std::wstring& inFolder);

template<typename T> size_t vecRemoveDuplicates(std::vector<T>& vec, bool isSorted = false, bool canSort = false)
{
	if (!isSorted && canSort)
	{
		std::sort(vec.begin(), vec.end());
		isSorted = true;
	}

	if (isSorted)
	{
		typename std::vector<T>::iterator it;
		it = std::unique(vec.begin(), vec.end());
		vec.resize(distance(vec.begin(), it));  // unique() does not shrink the vector
	}
	else
	{
		std::unordered_set<T> seen;
		auto newEnd = std::remove_if(vec.begin(), vec.end(), [&seen](const T& value)
			{
				return !seen.insert(value).second;
			});
		vec.erase(newEnd, vec.end());
	}
	return vec.size();
}

void trim(std::wstring& str);

int nbDigitsFromNbLines(size_t nbLines);

std::wstring getDateTimeStrFrom(const std::wstring& dateTimeFormat, const SYSTEMTIME& st);

bool removeReadOnlyFlagFromFileAttributes(const wchar_t* fileFullPath);
bool toggleReadOnlyFlagFromFileAttributes(const wchar_t* fileFullPath, bool& isChangedToReadOnly);

bool isWin32NamespacePrefixedFileName(const std::wstring& fileName);
bool isWin32NamespacePrefixedFileName(const wchar_t* szFileName);
bool isUnsupportedFileName(const std::wstring& fileName);
bool isUnsupportedFileName(const wchar_t* szFileName);
bool isUncPath(const std::wstring& path);
bool isUncFileUrl(const std::wstring& url);

class Version final
{
public:
	Version() = default;
	explicit Version(const std::wstring& versionStr);

	void setVersionFrom(const std::wstring& filePath);
	std::wstring toString() const;
	static bool isNumber(const std::wstring& s) {
		static const auto& loc = std::locale::classic();
		return !s.empty() &&
			find_if(s.begin(), s.end(), [](auto c) { return !std::isdigit(c, loc); }) == s.end();
	}

	int compareTo(const Version& v2c) const;

	bool operator < (const Version& v2c) const {
		return compareTo(v2c) == -1;
	}

	bool operator <= (const Version& v2c) const {
		int r = compareTo(v2c);
		return r == -1 || r == 0;
	}

	bool operator > (const Version& v2c) const {
		return compareTo(v2c) == 1;
	}

	bool operator >= (const Version& v2c) const {
		int r = compareTo(v2c);
		return r == 1 || r == 0;
	}

	bool operator == (const Version& v2c) const {
		return compareTo(v2c) == 0;
	}

	bool operator != (const Version& v2c) const {
		return compareTo(v2c) != 0;
	}

	bool empty() const {
		return _major == 0 && _minor == 0 && _patch == 0 && _build == 0;
	}

	bool isCompatibleTo(const Version& from, const Version& to) const;

private:
	unsigned long _major = 0;
	unsigned long _minor = 0;
	unsigned long _patch = 0;
	unsigned long _build = 0;
};


BOOL getDiskFreeSpaceWithTimeout(const wchar_t* dirPath, ULARGE_INTEGER* freeBytesForUser,
	DWORD milliSec2wait = 0, bool* isTimeoutReached = nullptr);
BOOL getFileAttributesExWithTimeout(const wchar_t* filePath, WIN32_FILE_ATTRIBUTE_DATA* fileAttr,
	DWORD milliSec2wait = 0, bool* isTimeoutReached = nullptr, DWORD* pdwWin32ApiError = nullptr);

bool doesFileExist(const wchar_t* filePath, DWORD milliSec2wait = 0, bool* isTimeoutReached = nullptr);
bool doesDirectoryExist(const wchar_t* dirPath, DWORD milliSec2wait = 0, bool* isTimeoutReached = nullptr);
bool doesPathExist(const wchar_t* path, DWORD milliSec2wait = 0, bool* isTimeoutReached = nullptr);


// check if the window rectangle intersects with any currently active monitor's working area
bool isWindowVisibleOnAnyMonitor(const RECT& rectWndIn);



class ControlInfoTip final
{
public:
	ControlInfoTip() = default;
	~ControlInfoTip() {
		hide();
	}

	bool init(HWND ctrl2attached, const std::wstring& tipStr, unsigned int remainTimeMillisecond = 0);

	bool isValid() const {
		return _attached != nullptr;
	}

	enum showPosition {beginning, middle, end};
	void show(showPosition pos = middle) const;

	void hide();

private:
	HWND _attached = nullptr;
	QString _tip;
	int _remainTime = 0;

	ControlInfoTip(const ControlInfoTip&) = delete;
	ControlInfoTip& operator=(const ControlInfoTip&) = delete;
};

bool fileTimeToYMD(const FILETIME& ft, int& yyyymmdd);
FILETIME currentFileTime();
// In the user's locale short date and time format
std::wstring fileTimeToLocalString(const FILETIME& ft);
// Shortens a path to at most maxLen characters with "...", keeping the file name when there is a directory part
std::wstring compactPath(const std::wstring& path, size_t maxLen);
void expandEnv(std::wstring& path2Expand);

