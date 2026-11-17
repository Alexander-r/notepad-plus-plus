// Tests for the parts of MISC/Common that Find in Files and file handling depend on.

#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>

#include <string>
#include <vector>

#include "Common.h"
#include "FileInterface.h"
#include "Sorters.h"
#include "WindowsCodePages.h"

using std::wstring;
using std::vector;

class CommonTest : public QObject
{
	Q_OBJECT

private slots:
	void wildcardFilters()
	{
		QVERIFY(pathMatchSpec(L"main.cpp", L"*.cpp"));
		QVERIFY(pathMatchSpec(L"MAIN.CPP", L"*.cpp"));          // case-insensitive, as PathMatchSpec
		QVERIFY(!pathMatchSpec(L"main.cpp", L"*.h"));
		QVERIFY(pathMatchSpec(L"main.cpp", L"m?in.*"));
		QVERIFY(!pathMatchSpec(L"main.cpp", L"m?n.*"));
		QVERIFY(pathMatchSpec(L"Makefile", L"*.*"));            // "*.*" accepts a name without a dot
		QVERIFY(pathMatchSpec(L"Makefile", L"*"));
		QVERIFY(pathMatchSpec(L"a.b.c", L"*.c"));
		QVERIFY(!pathMatchSpec(L"a.c.b", L"*.c"));
	}

	void matchInListHandlesExclusion()
	{
		const vector<wstring> filters {L"*.*", L"!*.exe", L"!*.obj"};
		QVERIFY(matchInList(L"a.txt", filters));
		QVERIFY(!matchInList(L"a.exe", filters));
		QVERIFY(!matchInList(L"a.OBJ", filters));
		QVERIFY(!matchInList(L"a.txt", {L"*.cpp", L"*.h"}));
		QVERIFY(allPatternsAreExclusion({L"!*.a", L"!*.b"}));
		QVERIFY(!allPatternsAreExclusion({L"*.a", L"!*.b"}));
	}

	void excludeDirectoriesFromFilter()
	{
		const vector<wstring> filters {L"*.*", L"!\\tests", L"!+\\log*", L"!/bin"};
		QVERIFY(matchInExcludeDirList(L"tests", filters, 1));   // first level only
		QVERIFY(!matchInExcludeDirList(L"tests", filters, 2));
		QVERIFY(matchInExcludeDirList(L"logs", filters, 1));    // all levels
		QVERIFY(matchInExcludeDirList(L"logs", filters, 3));
		QVERIFY(matchInExcludeDirList(L"bin", filters, 1));     // forward slash form
		QVERIFY(!matchInExcludeDirList(L"src", filters, 1));
	}

	void paths()
	{
		wstring p = L"/home/user";
		QCOMPARE(pathAppend(p, L"file.txt"), wstring(L"/home/user/file.txt"));
		p = L"/home/user/";
		QCOMPARE(pathAppend(p, L"/file.txt"), wstring(L"/home/user/file.txt"));
		p = L"/foo.txt";
		QCOMPARE(pathRemoveFileSpec(p), wstring(L"/"));
		p = L"/a/b/c.txt";
		QCOMPARE(pathRemoveFileSpec(p), wstring(L"/a/b"));
		p = L"name";
		QCOMPARE(pathRemoveFileSpec(p), wstring(L""));
	}

	void fileExistence()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const wstring folder = dir.path().toStdWString();
		wstring file = folder;
		pathAppend(file, L"x.txt");
		QVERIFY(doesDirectoryExist(folder.c_str()));
		QVERIFY(!doesFileExist(folder.c_str()));
		QVERIFY(!doesFileExist(file.c_str()));

		QFile f(QString::fromStdWString(file));
		QVERIFY(f.open(QIODevice::WriteOnly));
		f.write("hello");
		f.close();
		QVERIFY(doesFileExist(file.c_str()));
		QVERIFY(doesPathExist(file.c_str()));

		WIN32_FILE_ATTRIBUTE_DATA attributes{};
		QVERIFY(getFileAttributesExWithTimeout(file.c_str(), &attributes));
		QCOMPARE(attributes.nFileSizeLow, DWORD(5));
		QVERIFY(attributes.ftLastWriteTime.dwHighDateTime != 0);

		bool failed = true;
		QCOMPARE(getFileContent(file.c_str(), &failed), std::string("hello"));
		QVERIFY(!failed);

		vector<wstring> files;
		getFilesInFolder(files, L"*.txt", folder);
		QCOMPARE(files.size(), size_t(1));
		QCOMPARE(files[0], file);
	}

	void fileWriterKeepsExistingFilePermissions()
	{
		QTemporaryDir dir;
		const wstring file = (dir.path() + QStringLiteral("/out.txt")).toStdWString();
		const QString qfile = QString::fromStdWString(file);

		writeFileContent(file.c_str(), "first version, longer");
		QCOMPARE(getFileContent(file.c_str()), std::string("first version, longer"));

		const auto permissions = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner;
		QVERIFY(QFile::setPermissions(qfile, permissions));
		writeFileContent(file.c_str(), "second");
		QCOMPARE(getFileContent(file.c_str()), std::string("second")); // truncated, not appended
		QCOMPARE(QFile::permissions(qfile) & (QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner), permissions);

		Win32_IO_File invalid(L"/nonexistent-dir-for-test/x.txt");
		QVERIFY(!invalid.isOpened());
		QCOMPARE(invalid.getLastErrorCode(), DWORD(ERROR_FILE_NOT_FOUND));
	}

	void codePageConversions()
	{
		WcharMbcsConvertor& conv = WcharMbcsConvertor::getInstance();

		// UTF-8 round trip, including a character outside the BMP
		const std::string utf8 = "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80";
		QCOMPARE(string2wstring(utf8, CP_UTF8), wstring(L"é€\U0001F600"));
		QCOMPARE(wstring2string(L"é€\U0001F600", CP_UTF8), utf8);

		// Windows code pages used by the encoding menu
		QCOMPARE(string2wstring("\xE9", 1252), wstring(L"é"));
		QCOMPARE(string2wstring("\x80", 1252), wstring(L"€"));
		QCOMPARE(string2wstring("\xE6", 1251), wstring(L"ж"));
		QCOMPARE(string2wstring("\xD6", 20866), wstring(L"ж"));  // KOI8-R
		QCOMPARE(wstring2string(L"ж", 1251), std::string("\xE6"));
		QCOMPARE(string2wstring("\x82\xA0", 932), wstring(L"あ")); // Shift_JIS
		QCOMPARE(string2wstring("\xB0\xA1", 936), wstring(L"啊")); // GBK

		// A multi-byte character cut at the end of the buffer is left for the next block
		int lenWc = 0;
		int notProcessed = 0;
		const char cut[] = "ab\xE2\x82";
		const wchar_t* w = conv.char2wchar(cut, CP_UTF8, 4, &lenWc, &notProcessed);
		QCOMPARE(lenWc, 2);
		QCOMPARE(notProcessed, 2);
		QCOMPARE(wstring(w), wstring(L"ab"));

		// Byte offsets map to wide character offsets
		intptr_t start = 2;
		intptr_t end = 3;
		const char mixed[] = "\xC3\xA9xy";
		conv.char2wchar(mixed, CP_UTF8, &start, &end, 4);
		QCOMPARE(start, intptr_t(1));
		QCOMPARE(end, intptr_t(2));

		int lenMbcs = 0;
		QCOMPARE(std::string(conv.wchar2char(L"éx", CP_UTF8, 2, &lenMbcs)), std::string("\xC3\xA9x"));
		QCOMPARE(lenMbcs, 3);
	}

	// Single-byte code pages read and write as on Windows, with its tables: every byte comes back on saving,
	// undefined ones included, which Windows maps to private-use characters
	void singleByteCodePages()
	{
		for (const WindowsCodePage& table : windowsCodePages)
		{
			std::string bytes;
			for (int b = 1; b < 0x100; ++b)
				bytes += static_cast<char>(b);
			const wstring text = string2wstring(bytes, table.codePage);
			QCOMPARE(text.size(), bytes.size());
			for (int b = 1; b < 0x100; ++b)
			{
				const wchar_t expected = (b < 0x80) ? static_cast<wchar_t>(b) : static_cast<wchar_t>(table.high[b - 0x80]);
				QVERIFY2(text[b - 1] == expected, qPrintable(QStringLiteral("code page %1, byte %2").arg(table.codePage).arg(b, 2, 16)));
			}
			QVERIFY2(wstring2string(text, table.codePage) == bytes, qPrintable(QStringLiteral("code page %1").arg(table.codePage)));
		}

		// As the original read them on Windows
		QCOMPARE(string2wstring("\xE6", 437), wstring(L"\u00B5"));      // micro sign, where Qt has the Greek mu
		QCOMPARE(string2wstring("\x82", 720), wstring(L"\u00E9"));      // OEM 720, which Qt lacks
		QCOMPARE(string2wstring("\x80", 737), wstring(L"\u0391"));      // OEM 737, which Qt lacks
		QCOMPARE(string2wstring("\x80", 10007), wstring(L"\u0410"));    // Mac Cyrillic, which detection picks for some Windows-1251 text
		QCOMPARE(string2wstring("\xAE", 21866), wstring(L"\u045E"));    // KOI8-U as Windows has it
		QCOMPARE(string2wstring("\xD2", 1253), wstring(L"\uF8FA"));     // undefined in Windows-1253, and kept
		QCOMPARE(wstring2string(L"\uF8FA", 1253), std::string("\xD2"));

		// What a code page lacks is written as '?', as WideCharToMultiByte does by default
		QCOMPARE(wstring2string(L"a\u4E2Db", 1252), std::string("a?b"));
	}

	// Lines that compare equal keep their order, as with the original's std::sort from MSVC, which sorts up to 32 lines
	// by insertion. Here there are more than the 16 lines below which libstdc++ does the same.
	void sortsKeepEqualLinesInOrder()
	{
		std::vector<wstring> lines;
		for (int i = 0; i < 9; ++i)
			lines.push_back(L"line " + std::to_wstring(i));
		const std::vector<wstring> ties = { L"Banana", L"banana", L"BANANA", L"bAnAnA", L"abc", L"xyz", L"123", L"10b" };
		lines.insert(lines.end(), ties.begin(), ties.end());
		QVERIFY(lines.size() > 16);

		const auto positionsOf = [](const std::vector<wstring>& sorted, std::initializer_list<const wchar_t*> wanted)
		{
			std::vector<size_t> positions;
			for (const wchar_t* w : wanted)
				positions.push_back(static_cast<size_t>(std::find(sorted.begin(), sorted.end(), w) - sorted.begin()));
			return positions;
		};
		const auto isIncreasing = [](const std::vector<size_t>& positions)
		{
			return std::is_sorted(positions.begin(), positions.end()) && std::adjacent_find(positions.begin(), positions.end()) == positions.end();
		};

		for (const bool isDescending : { false, true })
		{
			std::vector<wstring> sorted = lines;
			LexicographicCaseInsensitiveSorter(isDescending, 0, 0).sort(sorted);
			QVERIFY(isIncreasing(positionsOf(sorted, { L"Banana", L"banana", L"BANANA", L"bAnAnA" })));

			sorted = lines;
			LineLengthSorter(isDescending, 0, 0).sort(sorted);
			QVERIFY(isIncreasing(positionsOf(sorted, { L"abc", L"xyz", L"123", L"10b" })));
		}
	}

	void dateTimeFormat()
	{
		SYSTEMTIME st;
		st.wYear = 2026; st.wMonth = 3; st.wDay = 5; st.wHour = 14; st.wMinute = 7; st.wSecond = 9;
		QCOMPARE(getDateTimeStrFrom(L"yyyy-MM-dd HH:mm:ss", st), wstring(L"2026-03-05 14:07:09"));
		QVERIFY(getDateTimeStrFrom(L"h:mm tt", st).starts_with(L"2:07 "));
		QCOMPARE(getDateTimeStrFrom(L"'y' yyyy", st), wstring(L"y 2026"));
	}

	void environmentExpansion()
	{
		qputenv("NPP_TEST_VAR", "value");
		wstring s = L"%NPP_TEST_VAR%/$NPP_TEST_VAR/${NPP_TEST_VAR}/%NO_SUCH_VAR_X%";
		expandEnv(s);
		QCOMPARE(s, wstring(L"value/value/value/%NO_SUCH_VAR_X%"));
	}

	void miscellaneous()
	{
		QVERIFY(isInList(L"Cpp", L"c cpp h"));
		QVERIFY(!isInList(L"cp", L"c cpp h"));
		QCOMPARE(purgeMenuItemString(L"&Open\tCtrl+O"), wstring(L"Open"));
		QCOMPARE(BuildMenuFileName(0, 0, L"/a/b/c&d.txt", true), wstring(L"&1: c&&d.txt"));
		QVERIFY(Version(L"8.9.8") < Version(L"8.9.8.1"));
		QCOMPARE(Version(L"8.9.8.1").toString(), wstring(L"8.9.8.1"));
		QVERIFY(!isUnsupportedFileName(L"/tmp/CON"));   // only reserved on Windows
		QVERIFY(isUnsupportedFileName(L""));
		QVERIFY(isUncFileUrl(L"file://server/share"));
		QVERIFY(!isUncFileUrl(L"file:///etc/hosts"));
		QCOMPARE(stodLocale(L"1.5x", nullptr), 1.5);
	}
};

QTEST_MAIN(CommonTest)
#include "commonTest.moc"
