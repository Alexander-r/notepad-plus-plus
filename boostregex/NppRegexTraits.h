// This file is part of Notepad++ project
//
// Regex traits used by BoostRegExSearch.cxx on non-Windows targets. On Windows, Boost selects
// w32_regex_traits, whose case mapping and character classes come from LCMapStringW and
// GetStringTypeExW. Boost's default traits elsewhere depend on the C/C++ locale, and in the C
// locale they do not fold non-ASCII text, so (?i)é would not match É. This class gives the same
// class masks and lookups as w32_regex_traits using Qt's Unicode tables, with no locale state.
//
// wchar_t still holds UTF-16 code units, exactly as on Windows: UTF8DocumentIterator and
// UTF16FromUTF8 both produce surrogate pairs, so regex semantics for characters outside the
// BMP stay the same even though wchar_t is 32 bits wide here.
//
// For the 8-bit (ANSI document) instantiation, bytes are interpreted as Latin-1, because there
// is no system ANSI code page outside Windows.

#pragma once

#include <QChar>
#include <QString>

#include <boost/regex/v5/regex_traits_defaults.hpp>
#include <boost/regex/v5/primary_transform.hpp>

#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>

template <class CharT>
class NppRegexTraits
{
public:
	typedef CharT char_type;
	typedef std::size_t size_type;
	typedef std::basic_string<CharT> string_type;
	typedef std::uint32_t char_class_type;
	struct locale_type {};
	struct boost_extensions_tag {};

	// Same bit values as w32_regex_traits (the C1_* group, then Boost's extensions).
	static constexpr char_class_type maskUpper = 0x0001;
	static constexpr char_class_type maskLower = 0x0002;
	static constexpr char_class_type maskDigit = 0x0004;
	static constexpr char_class_type maskSpace = 0x0008;
	static constexpr char_class_type maskPunct = 0x0010;
	static constexpr char_class_type maskCntrl = 0x0020;
	static constexpr char_class_type maskBlank = 0x0040;
	static constexpr char_class_type maskXdigit = 0x0080;
	static constexpr char_class_type maskAlpha = 0x0100;
	static constexpr char_class_type maskWord = 0x0400;
	static constexpr char_class_type maskUnicode = 0x0800;
	static constexpr char_class_type maskHorizontal = 0x1000;
	static constexpr char_class_type maskVertical = 0x2000;
	static constexpr char_class_type maskBase = 0x03ff;

	NppRegexTraits() = default;

	static size_type length(const char_type* p)
	{
		return std::char_traits<CharT>::length(p);
	}

	boost::regex_constants::syntax_type syntax_type(char_type c) const
	{
		return (isAscii(c)) ? boost::BOOST_REGEX_DETAIL_NS::get_default_syntax_type(static_cast<char>(c)) : boost::regex_constants::syntax_char;
	}

	boost::regex_constants::escape_syntax_type escape_syntax_type(char_type c) const
	{
		if (isAscii(c))
			return boost::BOOST_REGEX_DETAIL_NS::get_default_escape_syntax_type(static_cast<char>(c));
		// w32_regex_traits treats any other lower/upper case letter as a class escape.
		const QChar qc = toQChar(c);
		if (qc.isLower())
			return boost::regex_constants::escape_type_class;
		if (qc.isUpper())
			return boost::regex_constants::escape_type_not_class;
		return boost::regex_constants::escape_type_identity;
	}

	char_type translate(char_type c) const { return c; }
	char_type translate_nocase(char_type c) const { return tolower(c); }
	char_type translate(char_type c, bool icase) const { return icase ? translate_nocase(c) : c; }

	char_type tolower(char_type c) const
	{
		if (isAscii(c))
			return (c >= 'A' && c <= 'Z') ? static_cast<char_type>(c + ('a' - 'A')) : c;
		return fromQChar(toQChar(c).toLower(), c);
	}

	char_type toupper(char_type c) const
	{
		if (isAscii(c))
			return (c >= 'a' && c <= 'z') ? static_cast<char_type>(c - ('a' - 'A')) : c;
		return fromQChar(toQChar(c).toUpper(), c);
	}

	// Only used when collate is requested, which Notepad++ never does.
	string_type transform(const char_type* p1, const char_type* p2) const
	{
		return string_type(p1, p2);
	}

	// Sort key for equivalence classes such as [[=e=]]: the base letter, case-insensitive,
	// which matches accented forms the way LCMapStringW's primary weight does.
	string_type transform_primary(const char_type* p1, const char_type* p2) const
	{
		QString decomposed;
		for (const char_type* p = p1; p != p2; ++p)
			decomposed.append(toQChar(*p));
		decomposed = decomposed.normalized(QString::NormalizationForm_D);
		string_type key;
		for (const QChar qc : decomposed)
		{
			if (!qc.isMark())
				key.push_back(fromQChar(qc.toLower(), static_cast<char_type>(qc.unicode() & 0xFF)));
		}
		if (key.empty())
			key.assign(1, char_type(0));
		return key;
	}

	char_class_type lookup_classname(const char_type* p1, const char_type* p2) const
	{
		// Index order is defined by boost's get_default_class_id, masks as in w32_regex_traits.
		static constexpr char_class_type masks[] =
		{
			0,
			maskAlpha | maskDigit,                         // alnum
			maskAlpha,                                     // alpha
			maskBlank,                                     // blank
			maskCntrl,                                     // cntrl
			maskDigit,                                     // d
			maskDigit,                                     // digit
			(~(maskCntrl | maskSpace | maskBlank) & 0x01ff) | maskWord, // graph
			maskHorizontal,                                // h
			maskLower,                                     // l
			maskLower,                                     // lower
			(~maskCntrl & 0x01ff) | maskWord,              // print
			maskPunct,                                     // punct
			maskSpace,                                     // s
			maskSpace,                                     // space
			maskUpper,                                     // u
			maskUnicode,                                   // unicode
			maskUpper,                                     // upper
			maskVertical,                                  // v
			maskAlpha | maskDigit | maskWord,              // w
			maskAlpha | maskDigit | maskWord,              // word
			maskXdigit,                                    // xdigit
		};
		int id = boost::BOOST_REGEX_DETAIL_NS::get_default_class_id(p1, p2);
		if (id < 0)
		{
			string_type lowered(p1, p2);
			for (char_type& c : lowered)
				c = tolower(c);
			id = boost::BOOST_REGEX_DETAIL_NS::get_default_class_id(lowered.data(), lowered.data() + lowered.size());
		}
		return masks[id + 1];
	}

	string_type lookup_collatename(const char_type* p1, const char_type* p2) const
	{
		std::string name;
		for (const char_type* p = p1; p != p2; ++p)
			name.push_back(static_cast<char>(*p));
		name = boost::BOOST_REGEX_DETAIL_NS::lookup_default_collate_name(name);
		if (!name.empty())
			return string_type(name.begin(), name.end());
		if (p2 - p1 == 1)
			return string_type(1, *p1);
		return string_type();
	}

	bool isctype(char_type c, char_class_type mask) const
	{
		if ((mask & maskBase) && (c1Type(c) & mask & maskBase))
			return true;
		if ((mask & maskUnicode) && isExtended(c))
			return true;
		if ((mask & maskWord) && c == '_')
			return true;
		if ((mask & maskVertical) && isVertical(c))
			return true;
		if ((mask & maskHorizontal) && (c1Type(c) & maskSpace) && !isVertical(c))
			return true;
		return false;
	}

	std::intmax_t toi(const char_type*& p1, const char_type* p2, int radix) const
	{
		return boost::BOOST_REGEX_DETAIL_NS::global_toi(p1, p2, radix, *this);
	}

	int value(char_type c, int radix) const
	{
		const int result = static_cast<int>(boost::BOOST_REGEX_DETAIL_NS::global_value(c));
		return (result < radix) ? result : -1;
	}

	locale_type imbue(locale_type l) { return l; }
	locale_type getloc() const { return locale_type(); }

	std::string error_string(boost::regex_constants::error_type e) const
	{
		return boost::BOOST_REGEX_DETAIL_NS::get_default_error_string(e);
	}

private:
	static bool isAscii(char_type c)
	{
		return static_cast<std::uint32_t>(static_cast<std::make_unsigned_t<char_type>>(c)) < 0x80;
	}

	static bool isExtended(char_type c)
	{
		return static_cast<std::uint32_t>(static_cast<std::make_unsigned_t<char_type>>(c)) > 0xFF;
	}

	static QChar toQChar(char_type c)
	{
		return QChar(static_cast<char16_t>(static_cast<std::make_unsigned_t<char_type>>(c)));
	}

	static char_type fromQChar(QChar qc, char_type original)
	{
		if constexpr (sizeof(char_type) == 1)
			return (qc.unicode() <= 0xFF) ? static_cast<char_type>(qc.unicode()) : original; // no Latin-1 counterpart, such as the upper case of 0xFF
		else
			return static_cast<char_type>(qc.unicode());
	}

	static bool isVertical(char_type c)
	{
		const std::uint32_t u = static_cast<std::make_unsigned_t<char_type>>(c);
		return u == '\n' || u == '\r' || u == '\f' || u == '\v' || u == 0x85 || u == 0x2028 || u == 0x2029;
	}

	static char_class_type c1Type(char_type c)
	{
		const QChar qc = toQChar(c);
		char_class_type t = 0;
		switch (qc.category())
		{
			case QChar::Letter_Uppercase: t = maskUpper | maskAlpha; break;
			case QChar::Letter_Lowercase: t = maskLower | maskAlpha; break;
			case QChar::Letter_Titlecase:
			case QChar::Letter_Modifier:
			case QChar::Letter_Other:
			case QChar::Number_Letter:
			case QChar::Mark_NonSpacing:
			case QChar::Mark_SpacingCombining:
			case QChar::Mark_Enclosing:
				t = maskAlpha; break;
			case QChar::Number_DecimalDigit: t = maskDigit; break;
			case QChar::Separator_Space: t = maskSpace | maskBlank; break;
			case QChar::Separator_Line:
			case QChar::Separator_Paragraph:
				t = maskSpace; break;
			case QChar::Other_Control:
				t = maskCntrl;
				if ((qc.unicode() >= 0x09 && qc.unicode() <= 0x0D) || qc.unicode() == 0x85)
					t |= maskSpace;
				if (qc.unicode() == 0x09)
					t |= maskBlank;
				break;
			case QChar::Punctuation_Connector:
			case QChar::Punctuation_Dash:
			case QChar::Punctuation_Open:
			case QChar::Punctuation_Close:
			case QChar::Punctuation_InitialQuote:
			case QChar::Punctuation_FinalQuote:
			case QChar::Punctuation_Other:
			case QChar::Symbol_Math:
			case QChar::Symbol_Currency:
			case QChar::Symbol_Modifier:
			case QChar::Symbol_Other:
				t = maskPunct; break;
			default: break;
		}
		const char16_t u = qc.unicode();
		if ((u >= '0' && u <= '9') || (u >= 'a' && u <= 'f') || (u >= 'A' && u <= 'F'))
			t |= maskXdigit;
		return t;
	}
};
