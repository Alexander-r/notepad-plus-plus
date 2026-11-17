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

#include "string_utils.h"

#include <algorithm>
#include <cassert>
#include <cctype>

#include <QChar> // Qt port: Unicode's cases, whatever the locale, as Windows' IsCharLower and CharUpper

// Qt port: make_lower and make_upper, as towlower and towupper only know the characters of the locale here
void to_lower_inplace(std::wstring &s) {
  std::transform(s.begin(), s.end(), s.begin(), &make_lower);
}

void to_upper_inplace(std::wstring &s) { std::transform(s.begin(), s.end(), s.begin(), &make_upper); }

void ltrim_inplace(std::wstring &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](wchar_t ch) {
    return iswspace(ch) == FALSE;
  }));
}

void rtrim_inplace(std::wstring &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(), [](wchar_t ch) {
    return iswspace(ch) == FALSE;
  }).base(), s.end());
}

void trim_inplace(std::wstring &s) {
  ltrim_inplace(s);
  rtrim_inplace(s);
}

size_t find_case_insensitive(const std::string_view &str_haystack, const std::string_view &str_needle) {
  // courtesy of https://stackoverflow.com/a/19839371/1269661
  auto it = std::search(
      str_haystack.begin(), str_haystack.end(),
      str_needle.begin(), str_needle.end(),
      [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
      );
  if (it == str_haystack.end())
    return std::string_view::npos;
  return it - str_haystack.begin();
}

string_case_type get_string_case_type(const std::wstring_view &sv) {
  if (sv.empty())
    return string_case_type::mixed;
  size_t lower_count = std::count_if(sv.begin(), sv.end(), &is_lower); // Qt port: is_lower, below
  if (lower_count == 0)
    return string_case_type::upper;
  if (lower_count == sv.length())
    return string_case_type::lower;
  if (lower_count == sv.length() - 1 && is_upper(sv.front())) // Qt port: is_upper
    return string_case_type::title;

  return string_case_type::mixed;
}

void apply_case_type(std::wstring &str, string_case_type type) {
  if (str.empty())
    return;
  std::transform(str.begin(), str.end(), str.begin(), [&](wchar_t wc) -> wchar_t {
                   switch (type) {
                   case string_case_type::lower:
                   case string_case_type::title:
                     return make_lower(wc);
                   case string_case_type::upper:
                     return make_upper(wc);
                   case string_case_type::mixed:
                     throw std::invalid_argument("apply_case_type: inapplicable for mixed case");
                     break;
                   }
                   throw std::invalid_argument("apply_case_type: corrupted enum value for type");
                 }
      );
  if (type == string_case_type::title)
    str.front() = make_upper(str.front());
}

// Qt port: QChar's, in place of CharUpper, CharLower, IsCharUpper and IsCharLower
wchar_t make_upper(wchar_t c) {
  return static_cast<wchar_t>(QChar::toUpper(static_cast<char32_t>(c)));
}

wchar_t make_lower(wchar_t c) {
  return static_cast<wchar_t>(QChar::toLower(static_cast<char32_t>(c)));
}

bool is_upper(wchar_t c) {
  return QChar::isUpper(static_cast<char32_t>(c));
}

bool is_lower(wchar_t c) {
  return QChar::isLower(static_cast<char32_t>(c));
}

// As Windows' IsCharAlpha and IsCharAlphaNumeric on a Western system, which count no character of the scripts of
// East Asia as a letter, so that their text is no word to check
static bool is_east_asian(char32_t c) {
  switch (QChar::script(c)) {
  case QChar::Script_Han:
  case QChar::Script_Hiragana:
  case QChar::Script_Katakana:
  case QChar::Script_Hangul:
  case QChar::Script_Bopomofo:
  case QChar::Script_Yi:
    return true;
  default:
    return false;
  }
}

bool is_alpha(wchar_t c) {
  return QChar::isLetter(static_cast<char32_t>(c)) && !is_east_asian(static_cast<char32_t>(c));
}

bool is_alpha_numeric(wchar_t c) {
  return QChar::isLetterOrNumber(static_cast<char32_t>(c)) && !is_east_asian(static_cast<char32_t>(c));
}
