// This file is part of DSpellCheck Plug-in for Notepad++
// Copyright (C)2019 Sergey Semushin <Predelnik@gmail.com>
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "Utility.h"

#include "iconv.h"
#include "common/string_utils.h"
#include "common/utf8.h"
#include "common/winapi.h"
#include "plugin/Plugin.h"
#include "plugin/resource.h"
#include "plugin/Settings.h"

#include <cassert>

// Qt port: language names from QLocale, and folders made with QDir
#include <QDir>
#include <QLocale>
#include <QString>

// Qt port: glibc's names of the encodings. wchar_t has 32 bits here, so it is WCHAR_T where Windows has UCS-2LE,
// and the encoding of the system, CHAR on Windows, is UTF-8, as for the ANSI documents of the port.
#define DSPELLCHECK_WCHAR "WCHAR_T"
#define DSPELLCHECK_ANSI "UTF-8"

static std::vector<char> convert(const char *source_enc, const char *target_enc, const void *source_data_ptr, size_t source_len, size_t max_dest_len) {
  std::vector<char> buf(max_dest_len);
  auto out_buf = buf.data();
  iconv_t converter = iconv_open(target_enc, source_enc);
  if (converter == reinterpret_cast<iconv_t>(-1)) // Qt port: an encoding that iconv does not know
    return {};
  auto char_ptr = const_cast<char *>(static_cast<const char *>(source_data_ptr)); // Qt port: glibc's iconv takes char **
  size_t res = iconv(converter, &char_ptr, &source_len, &out_buf, &max_dest_len);
  iconv_close(converter);

  if (res == static_cast<size_t>(-1))
    return {};
  return buf;
}

std::wstring to_wstring(std::string_view source) {
  auto bytes = convert(DSPELLCHECK_ANSI, DSPELLCHECK_WCHAR "//IGNORE", source.data(), source.length(), sizeof(wchar_t) * (source.length() + 1));
  if (bytes.empty())
    return {};
  return reinterpret_cast<wchar_t *>(bytes.data());
}

std::string to_string(std::wstring_view source) {
  auto bytes = convert(DSPELLCHECK_WCHAR, DSPELLCHECK_ANSI "//IGNORE", source.data(), (source.length()) * sizeof(wchar_t), sizeof(wchar_t) * (source.length() + 1));
  if (bytes.empty())
    return {};
  return bytes.data();
}

std::string to_utf8_string(std::string_view source) {
  auto bytes = convert(DSPELLCHECK_ANSI, "UTF-8//IGNORE", source.data(), source.length(), max_utf8_char_length * (source.length() + 1));
  if (bytes.empty())
    return {};
  return bytes.data();
}

std::string to_utf8_string(std::wstring_view source) {
  auto bytes = convert(DSPELLCHECK_WCHAR, "UTF-8//IGNORE", source.data(), source.length() * sizeof(wchar_t), max_utf8_char_length * (source.length() + 1));
  if (bytes.empty())
    return {};
  return bytes.data();
}

std::wstring utf8_to_wstring(const char *source) {
  auto bytes = convert("UTF-8", DSPELLCHECK_WCHAR "//IGNORE", source, strlen(source), (utf8_length(source) + 1) * sizeof(wchar_t));
  if (bytes.empty())
    return {};
  return reinterpret_cast<const wchar_t *>(bytes.data());
}

std::string utf8_to_string(const char *source) {
  auto bytes = convert("UTF-8", DSPELLCHECK_ANSI "//IGNORE", source, strlen(source), utf8_length(source) + 1);
  if (bytes.empty())
    return {};
  return bytes.data();
}

// This function is more or less transferred from gcc source
bool match_special_char(wchar_t *dest, const wchar_t *&source) {
  wchar_t c;

  bool m = true;
  auto assign = [dest](wchar_t c) {
    if (dest != nullptr)
      *dest = c;
  };

  switch (c = *(source++)) {
  case L'a':
    assign(L'\a');
    break;
  case L'b':
    assign(L'\b');
    break;
  case L't':
    assign(L'\t');
    break;
  case L'f':
    assign(L'\f');
    break;
  case L'n':
    assign(L'\n');
    break;
  case L'r':
    assign(L'\r');
    break;
  case L'v':
    assign(L'\v');
    break;
  case L'\\':
    assign(L'\\');
    break;
  case L'0':
    assign(L'\0');
    break;

  case L'x':
  case L'u':
  case L'U': {
    /* Hexadecimal form of wide characters.  */
    int len = (c == L'x' ? 2 : (c == L'u' ? 4 : 8));
    wchar_t n = 0;
#ifndef UNICODE
    len = 2;
#endif
    for (int i = 0; i < len; i++) {
      wchar_t buf[2] = {L'\0', L'\0'};

      c = *(source++);
      if (c > UCHAR_MAX || !((L'0' <= c && c <= L'9') || (L'a' <= c && c <= L'f') || (L'A' <= c && c <= L'F')))
        return false;

      buf[0] = c;
      n = n << 4;
      n += static_cast<wchar_t>(wcstol(buf, nullptr, 16));
    }
    assign(n);
    break;
  }

  default:
    /* Unknown backslash codes are simply not expanded.  */
    m = false;
    break;
  }
  return m;
}

static size_t do_parse_string(wchar_t *dest, const wchar_t *source) {
  bool dry_run = dest == nullptr;
  wchar_t *res_string = dest;
  while (*source != L'\0') {
    const wchar_t *last_pos = source;
    if (*source == '\\') {
      ++source;
      if (!match_special_char(!dry_run ? dest : nullptr, source)) {
        source = last_pos;
        if (!dry_run)
          *dest = *source;
        ++source;
        ++dest;
      } else {
        ++dest;
      }
    } else {
      if (!dry_run)
        *dest = *source;
      ++source;
      ++dest;
    }
  }
  if (!dry_run)
    *dest = L'\0';
  return dest - res_string + 1;
}

std::wstring parse_string(const wchar_t *source) {
  auto size = do_parse_string(nullptr, source);
  std::vector<wchar_t> buf(size);
  do_parse_string(buf.data(), source);
  assert(size == buf.size());
  return buf.data();
}

// These functions are mostly taken from http://research.microsoft.com

static const std::unordered_map<std::wstring_view, std::wstring> alias_map = {{L"AF-ZA", L"Afrikaans"},
                                                                              {L"AK-GH", L"Akan"},
                                                                              {L"AN-ES", L"Aragonese"},
                                                                              {L"AR", L"Arabic"},
                                                                              {L"BE-BY", L"Belarusian"},
                                                                              {L"BN-BD", L"Bengali"},
                                                                              {L"BO", L"Classical Tibetan"},
                                                                              {L"BG-BG", L"Bulgarian"},
                                                                              {L"BR-FR", L"Breton"},
                                                                              {L"BS-BA", L"Bosnian"},
                                                                              {L"CA", L"Catalan"},
                                                                              {L"CA-VALENCIA", L"Catalan (Valencia)"},
                                                                              {L"CA-ANY", L"Catalan (Any)"},
                                                                              {L"CA-ES", L"Catalan (Spain)"},
                                                                              {L"COP-EG", L"Coptic (Bohairic dialect)"},
                                                                              {L"CS-CZ", L"Czech"},
                                                                              {L"CY-GB", L"Welsh"},
                                                                              {L"DA-DK", L"Danish"},
                                                                              {L"DE-AT", L"German (Austria)"},
                                                                              {L"DE-AT-FRAMI", L"German (Austria) [frami]"},
                                                                              {L"DE-CH", L"German (Switzerland)"},
                                                                              {L"DE-CH-FRAMI", L"German (Switzerland) [frami]"},
                                                                              {L"DE-DE", L"German (Germany)"},
                                                                              {L"DE-DE-COMB", L"German (Old and New Spelling)"},
                                                                              {L"DE-DE-FRAMI", L"German (Germany) [frami]"},
                                                                              {L"DE-DE-NEU", L"German (New Spelling)"},
                                                                              {L"EL-GR", L"Greek"},
                                                                              {L"EN-AU", L"English (Australia)"},
                                                                              {L"EN-CA", L"English (Canada)"},
                                                                              {L"EN-GB", L"English (United Kingdom)"},
                                                                              {L"EN-GB-OED", L"English (United Kingdom) [OED]"},
                                                                              {L"EN-NZ", L"English (New Zealand)"},
                                                                              {L"EN-US", L"English (United States)"},
                                                                              {L"EN-ZA", L"English (South Africa)"},
                                                                              {L"EO-EO", L"Esperanto"},
                                                                              {L"ES-ANY", L"Spanish"},
                                                                              {L"ES-AR", L"Spanish (Argentina)"},
                                                                              {L"ES-BO", L"Spanish (Bolivia)"},
                                                                              {L"ES-CL", L"Spanish (Chile)"},
                                                                              {L"ES-CO", L"Spanish (Colombia)"},
                                                                              {L"ES-CR", L"Spanish (Costa Rica)"},
                                                                              {L"ES-CU", L"Spanish (Cuba)"},
                                                                              {L"ES-DO", L"Spanish (Dominican Republic)"},
                                                                              {L"ES-EC", L"Spanish (Ecuador)"},
                                                                              {L"ES-ES", L"Spanish (Spain)"},
                                                                              {L"ES-GT", L"Spanish (Guatemala)"},
                                                                              {L"ES-HN", L"Spanish (Honduras)"},
                                                                              {L"ES-MX", L"Spanish (Mexico)"},
                                                                              {L"ES-NEW", L"Spanish (New)"},
                                                                              {L"ES-NI", L"Spanish (Nicaragua)"},
                                                                              {L"ES-PA", L"Spanish (Panama)"},
                                                                              {L"ES-PE", L"Spanish (Peru)"},
                                                                              {L"ES-PR", L"Spanish (Puerto Rico)"},
                                                                              {L"ES-PY", L"Spanish (Paraguay)"},
                                                                              {L"ES-SV", L"Spanish (El Salvador)"},
                                                                              {L"ES-UY", L"Spanish (Uruguay)"},
                                                                              {L"ES-VE", L"Spanish (Bolivarian Republic of Venezuela)"},
                                                                              {L"ET-EE", L"Estonian"},
                                                                              {L"FO-FO", L"Faroese"},
                                                                              {L"FR", L"French"},
                                                                              {L"FR-FR", L"French"},
                                                                              {L"FR-FR-1990", L"French (1990)"},
                                                                              {L"FR-FR-1990-1-3-2", L"French (1990)"},
                                                                              {L"FR-FR-CLASSIQUE", L"French (Classique)"},
                                                                              {L"FR-FR-CLASSIQUE-1-3-2", L"French (Classique)"},
                                                                              {L"FR-FR-1-3-2", L"French"},
                                                                              {L"FY-NL", L"Frisian"},
                                                                              {L"GA-IE", L"Irish"},
                                                                              {L"GD-GB", L"Scottish Gaelic"},
                                                                              {L"GL-ES", L"Galician"},
                                                                              {L"GU-IN", L"Gujarati"},
                                                                              {L"GUG", L"Guarani"},
                                                                              {L"HE-IL", L"Hebrew"},
                                                                              {L"HI-IN", L"Hindi"},
                                                                              {L"HIl-PH", L"Hiligaynon"},
                                                                              {L"HR-HR", L"Croatian"},
                                                                              {L"HU-HU", L"Hungarian"},
                                                                              {L"IA", L"Interlingua"},
                                                                              {L"ID-ID", L"Indonesian"},
                                                                              {L"IS", L"Icelandic"},
                                                                              {L"IS-IS", L"Icelandic"},
                                                                              {L"IT-IT", L"Italian"},
                                                                              {L"KMR-LATN", L"Kurdish (Latin)"},
                                                                              {L"KU-TR", L"Kurdish"},
                                                                              {L"LA", L"Latin"},
                                                                              {L"LO-LA", L"Lao"},
                                                                              {L"LT", L"Lithuanian"},
                                                                              {L"LV-LV", L"Latvian"},
                                                                              {L"MG-MG", L"Malagasy"},
                                                                              {L"MI-NZ", L"Maori"},
                                                                              {L"MK-MK", L"Macedonian (FYROM)"},
                                                                              {L"MOS-BF", L"Mossi"},
                                                                              {L"MR-IN", L"Marathi"},
                                                                              {L"MS-MY", L"Malay"},
                                                                              {L"NB-NO", L"Norwegian (Bokmal)"},
                                                                              {L"NE-NP", L"Nepali"},
                                                                              {L"NL-NL", L"Dutch"},
                                                                              {L"NN-NO", L"Norwegian (Nynorsk)"},
                                                                              {L"NR-ZA", L"Ndebele"},
                                                                              {L"NS-ZA", L"Northern Sotho"},
                                                                              {L"NY-MW", L"Chichewa"},
                                                                              {L"OC-FR", L"Occitan"},
                                                                              {L"PL-PL", L"Polish"},
                                                                              {L"PT-BR", L"Portuguese (Brazil)"},
                                                                              {L"PT-PT", L"Portuguese (Portugal)"},
                                                                              {L"RO-RO", L"Romanian"},
                                                                              {L"RU-RU", L"Russian"},
                                                                              {L"RU-RU-IE", L"Russian (without io)"},
                                                                              {L"RU-RU-YE", L"Russian (without io)"},
                                                                              {L"RU-RU-YO", L"Russian (with io)"},
                                                                              {L"RW-RW", L"Kinyarwanda"},
                                                                              {L"SI-LK", L"Sinhala"},
                                                                              {L"SI-SI", L"Slovenian"},
                                                                              {L"SK-SK", L"Slovak"},
                                                                              {L"SL-SI", L"Slovenian"},
                                                                              {L"SQ-AL", L"Alban"},
                                                                              {L"SR", L"Serbian (Cyrillic)"},
                                                                              {L"SR-LATN", L"Serbian (Latin)"},
                                                                              {L"SS-ZA", L"Swazi"},
                                                                              {L"ST-ZA", L"Northern Sotho"},
                                                                              {L"SV-SE", L"Swedish"},
                                                                              {L"SV-FI", L"Swedish (Finland)"},
                                                                              {L"SW-KE", L"Kiswahili"},
                                                                              {L"SW-TZ", L"Swahili"},
                                                                              {L"TE-IN", L"Telugu"},
                                                                              {L"TET-ID", L"Tetum"},
                                                                              {L"TH-TH", L"Thai"},
                                                                              {L"TL-PH", L"Tagalog"},
                                                                              {L"TN-ZA", L"Setswana"},
                                                                              {L"TS-ZA", L"Tsonga"},
                                                                              {L"UK-UA", L"Ukrainian"},
                                                                              {L"UR-PK", L"Urdu"},
                                                                              {L"VE-ZA", L"Venda"},
                                                                              {L"VI-VN", L"Vietnamese"},
                                                                              {L"XH-ZA", L"isiXhosa"},
                                                                              {L"ZU-ZA", L"isiZulu"}};
// Language Aliases
std::pair<std::wstring, bool> apply_alias(std::wstring_view str, LanguageNameStyle style) {
  if (style == LanguageNameStyle::original)
    return {std::wstring(str), true};

  std::wstring str_normalized{str};
  std::replace(str_normalized.begin(), str_normalized.end(), L'_', L'-');
  // Qt port: the name of the language from QLocale, in place of GetLocaleInfoEx
  std::wstring alias;
  {
    const QString code = QString::fromStdWString(str_normalized);
    const QLocale locale(code);
    if (locale.language() != QLocale::C && QLocale::codeToLanguage(code.section(u'-', 0, 0)) == locale.language()) {
      const bool has_territory = code.contains(u'-');
      QString name;
      switch (style) {
      case LanguageNameStyle::localized:
        name = QLocale().languageToString(locale.language());
        if (has_territory)
          name += QStringLiteral(" (") + QLocale().territoryToString(locale.territory()) + u')';
        break;
      case LanguageNameStyle::native:
        name = locale.nativeLanguageName();
        if (has_territory && !locale.nativeTerritoryName().isEmpty())
          name += QStringLiteral(" (") + locale.nativeTerritoryName() + u')';
        break;
      case LanguageNameStyle::english:
      case LanguageNameStyle::original:
      case LanguageNameStyle::COUNT:
        name = QLocale::languageToString(locale.language());
        if (has_territory)
          name += QStringLiteral(" (") + QLocale::territoryToString(locale.territory()) + u')';
        break;
      }
      alias = name.toStdWString();
    }
  }
  if (!alias.empty())
    return {alias, true};

  to_upper_inplace(str_normalized);
  auto it = alias_map.find(str_normalized);
  if (it != alias_map.end())
    return {it->second, true};

  return {std::wstring(str), false};
}

// Qt port: the folder and those above it are made with QDir, where Windows makes each with CreateDirectory
bool check_for_directory_existence(std::wstring path, bool silent, HWND npp_window) {
  if (QDir().mkpath(QString::fromStdWString(path)))
    return true;
  if (!silent && npp_window != nullptr) {
    auto msg = wstring_printf(rc_str(IDS_CANT_CREATE_DIR_PS).c_str(), path.c_str());
    MessageBox(npp_window, msg.c_str(), rc_str(IDS_ERROR_IN_DIR_CREATE).c_str(), MB_OK | MB_ICONERROR);
  }
  return false;
}

void write_unicode_bom(FILE *fp) {
  WORD bom = 0xFEFF;
  fwrite(&bom, sizeof(bom), 1, fp);
}

std::wstring windows_wide_format(const wchar_t *format) {
  std::wstring result;
  for (const wchar_t *it = format; *it != L'\0'; ++it) {
    result += *it;
    if (*it != L'%')
      continue;
    ++it;
    if (*it == L'%') {
      result += *it;
      continue;
    }
    // Flags, width, precision and length up to the conversion
    std::wstring spec;
    while (*it != L'\0' && std::wcschr(L"-+ #0123456789.*hlLqjzt", *it) != nullptr)
      spec += *it++;
    if (*it == L'\0')
      return result + spec;
    const bool has_length = spec.find_first_of(L"hlLqjzt") != std::wstring::npos;
    switch (*it) {
    case L's':
    case L'c':
      result += spec + (has_length ? L"" : L"l") + *it;
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
