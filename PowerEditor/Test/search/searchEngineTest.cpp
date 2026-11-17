// Regression tests for the regex engine behind Find, Replace and Find in Files. They pin the
// behaviour the Windows build gets from Boost + w32_regex_traits, so the Linux build stays equal.

#include <QtTest>
#include <QApplication>

#include <string>
#include <utility>

#include "ScintillaEditBase.h"
#include "Scintilla.h"
#include "BoostRegexSearch.h"
#include "ILexer.h"
#include "Lexilla.h"

class SearchEngineTest : public QObject
{
	Q_OBJECT

private:
	static constexpr int rxFlags = SCFIND_REGEXP | SCFIND_POSIX | SCFIND_REGEXP_SKIPCRLFASONE;
	ScintillaEditBase* _edit = nullptr;

	void setText(const std::string& text, int codePage = SC_CP_UTF8)
	{
		_edit->send(SCI_SETCODEPAGE, codePage);
		_edit->send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(text.c_str()));
	}

	std::pair<long, long> find(const std::string& pattern, int flags, long start = 0, long end = -1)
	{
		_edit->send(SCI_SETSEARCHFLAGS, flags);
		_edit->send(SCI_SETTARGETRANGE, start, end < 0 ? _edit->send(SCI_GETLENGTH) : end);
		const long pos = static_cast<long>(_edit->send(SCI_SEARCHINTARGET, pattern.size(), reinterpret_cast<sptr_t>(pattern.c_str())));
		return {pos, pos < 0 ? pos : static_cast<long>(_edit->send(SCI_GETTARGETEND))};
	}

	std::string replaceFirst(const std::string& pattern, const std::string& replacement, int flags)
	{
		const auto match = find(pattern, flags);
		if (match.first < 0)
			return "<no match>";
		_edit->send(SCI_REPLACETARGETRE, replacement.size(), reinterpret_cast<sptr_t>(replacement.c_str()));
		std::string text(_edit->send(SCI_GETLENGTH), '\0');
		_edit->send(SCI_GETTEXT, text.size() + 1, reinterpret_cast<sptr_t>(text.data()));
		return text;
	}

	using Span = std::pair<long, long>;

private slots:
	void initTestCase()
	{
		_edit = new ScintillaEditBase();
	}

	void cleanupTestCase()
	{
		delete _edit;
	}

	void perlSyntax()
	{
		setText("foobar bazbar\r\nfoo\r\nbar\r\n");
		const int f = rxFlags | SCFIND_MATCHCASE;
		QCOMPARE(find("(?<=foo)bar", f), Span(3, 6));
		QCOMPARE(find("\\bbaz(?=bar)", f), Span(7, 10));
		QCOMPARE(find("foo\\Rbar", f), Span(15, 23));
		QCOMPARE(find("(foo)bar\\s\\w+\\1?", f), Span(0, 13));
		QCOMPARE(find("[[:upper:]]", f).first, -1L);
	}

	void dotAndLineAnchors()
	{
		setText("one\r\ntwo\r\n");
		QCOMPARE(find("^two$", rxFlags), Span(5, 8));
		QCOMPARE(find("one.two", rxFlags).first, -1L);
		QCOMPARE(find("one.+two", rxFlags | SCFIND_REGEXP_DOTMATCHESNL), Span(0, 8));
	}

	void nonAsciiCaseFolding()
	{
		// é t é, then É T É, in UTF-8. Both must match when matching case is off.
		setText("\xC3\xA9t\xC3\xA9 \xC3\x89T\xC3\x89 \xD0\xB6\xD0\x96");
		QCOMPARE(find("\\x{C9}T\\x{C9}", rxFlags), Span(0, 5));
		QCOMPARE(find("\\x{C9}T\\x{C9}", rxFlags | SCFIND_MATCHCASE), Span(6, 11));
		QCOMPARE(find("(?i)\xC3\x89", rxFlags), Span(0, 2));
		// Cyrillic ж / Ж
		QCOMPARE(find("\xD0\x96", rxFlags), Span(12, 14));
		QCOMPARE(find("\xD0\x96", rxFlags | SCFIND_MATCHCASE), Span(14, 16));
	}

	void unicodeCharacterClasses()
	{
		setText("  \xC3\xA9t\xC3\xA9 \xD9\xA3 ");
		QCOMPARE(find("\\w+", rxFlags), Span(2, 7));          // é is a word character
		QCOMPARE(find("[[:alpha:]]+", rxFlags), Span(2, 7));
		QCOMPARE(find("\\d", rxFlags), Span(8, 10));          // Arabic-Indic digit three
		QCOMPARE(find("[[:lower:]]", rxFlags | SCFIND_MATCHCASE), Span(2, 4));
		QCOMPARE(find("[[=e=]]", rxFlags | SCFIND_MATCHCASE), Span(2, 4)); // equivalence class
	}

	void supplementaryPlaneIsUtf16()
	{
		// U+1F600. As on Windows the regex sees a surrogate pair, not one code point.
		setText("a\xF0\x9F\x98\x80z");
		const int f = rxFlags | SCFIND_MATCHCASE;
		QCOMPARE(find("\xF0\x9F\x98\x80", f), Span(1, 5));
		QCOMPARE(find("\\x{D83D}\\x{DE00}", f), Span(1, 5));
		QCOMPARE(find("a..z", f), Span(0, 6));
		QCOMPARE(find("a.z", f).first, -1L);
	}

	void replacementFormat()
	{
		setText("Hello World");
		QCOMPARE(replaceFirst("(\\w+) (\\w+)", "$2 \\1", rxFlags), std::string("World Hello"));
		setText("\xC3\xA9t\xC3\xA9");
		QCOMPARE(replaceFirst("(\\w+)", "\\U$1", rxFlags), std::string("\xC3\x89T\xC3\x89"));
		setText("ab");
		QCOMPARE(replaceFirst("(a)|(z)", "(?1X)(?2Y)", rxFlags), std::string("Xb"));
	}

	void backwardSearch()
	{
		setText("foo foo foo");
		QCOMPARE(find("foo", rxFlags, 11, 0), Span(8, 11));
		QCOMPARE(find("foo", rxFlags, 7, 0), Span(4, 7));
	}

	void emptyMatchesAndCrlf()
	{
		setText("a\r\nb");
		int f = rxFlags | SCFIND_REGEXP_EMPTYMATCH_NOTAFTERMATCH;
		QCOMPARE(find("$", f), Span(1, 1));
		QCOMPARE(find("$", f, 2), Span(4, 4)); // never lands between CR and LF
	}

	void invalidRegexReportsBoostMessage()
	{
		setText("x");
		QCOMPARE(find("(unclosed", rxFlags).first, -2L);
		const long needed = static_cast<long>(_edit->send(SCI_GETBOOSTREGEXERRMSG, 0, 0));
		QVERIFY(needed > 1);
		std::string message(needed, '\0');
		_edit->send(SCI_GETBOOSTREGEXERRMSG, message.size(), reinterpret_cast<sptr_t>(message.data()));
		QVERIFY2(message.find("parenthes") != std::string::npos, message.c_str());
		QCOMPARE(find("x", rxFlags), Span(0, 1));
		QCOMPARE(_edit->send(SCI_GETBOOSTREGEXERRMSG, 0, 0), sptr_t(1)); // cleared by the next search
	}

	void ansiDocument()
	{
		setText("caf\xE9 CAF\xC9", 0);
		QCOMPARE(find("\xC9", rxFlags), Span(3, 4));
		QCOMPARE(find("\\w+", rxFlags), Span(0, 4));
	}

	void plainSearchWithFlags()
	{
		setText("Word wordy WORD");
		QCOMPARE(find("word", 0), Span(0, 4));
		QCOMPARE(find("word", SCFIND_MATCHCASE), Span(5, 9));
		QCOMPARE(find("WORD", SCFIND_MATCHCASE | SCFIND_WHOLEWORD), Span(11, 15));
	}

	void notepadPlusPlusLexersExist()
	{
		for (const char* name : {"user", "searchResult", "objc", "cpp"})
		{
			Scintilla::ILexer5* lexer = CreateLexer(name);
			QVERIFY2(lexer != nullptr, name);
			lexer->Release();
		}
	}
};

QTEST_MAIN(SearchEngineTest)
#include "searchEngineTest.moc"
