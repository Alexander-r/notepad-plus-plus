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

#include "IniWorker.h"

#include "Utility.h"

// Qt port: an INI file in UTF-8, read and written here as Windows' profile functions do for one section. Names
// are matched whatever their case, as on Windows. A value is kept as written, with its quotes, which is what
// process() expects of a value in quotes.
static std::wstring lower_name(std::wstring_view name) {
  std::wstring result(name);
  for (auto &c : result)
    c = static_cast<wchar_t>(std::towlower(c));
  return result;
}

IniWorker::IniWorker(std::wstring_view app_name, std::wstring_view file_name, Action action)
  : m_app_name{app_name},
    m_file_name{
        file_name
    },
    m_action{action} {
  if (m_action != Action::load)
    return;

  FILE *fp = _wfopen(m_file_name.c_str(), L"rb");
  if (fp == nullptr)
    return;
  std::string content;
  char chunk[4096];
  size_t read = 0;
  while ((read = fread(chunk, 1, sizeof(chunk), fp)) > 0)
    content.append(chunk, read);
  fclose(fp);
  if (content.size() >= 3 && content.compare(0, 3, "\xEF\xBB\xBF") == 0)
    content.erase(0, 3);

  const auto section = lower_name(m_app_name);
  bool in_section = false;
  size_t pos = 0;
  while (pos < content.size()) {
    auto end = content.find('\n', pos);
    if (end == std::string::npos)
      end = content.size();
    std::wstring line = utf8_to_wstring(content.substr(pos, end - pos).c_str());
    pos = end + 1;
    while (!line.empty() && (line.back() == L'\r' || line.back() == L' ' || line.back() == L'\t'))
      line.pop_back();
    const auto first = line.find_first_not_of(L" \t");
    if (first == std::wstring::npos || line[first] == L';')
      continue;
    if (line[first] == L'[') {
      const auto close = line.find(L']', first);
      in_section = close != std::wstring::npos && lower_name(line.substr(first + 1, close - first - 1)) == section;
      continue;
    }
    const auto equal = line.find(L'=', first);
    if (!in_section || equal == std::wstring::npos)
      continue;
    auto name = line.substr(first, equal - first);
    while (!name.empty() && (name.back() == L' ' || name.back() == L'\t'))
      name.pop_back();
    auto value = line.substr(equal + 1);
    const auto value_start = value.find_first_not_of(L" \t");
    value = value_start == std::wstring::npos ? std::wstring() : value.substr(value_start);
    m_values.emplace_back(lower_name(name), value);
  }
}

IniWorker::~IniWorker() {
  if (m_action != Action::save || m_values.empty())
    return;
  std::string content = "[" + to_utf8_string(m_app_name) + "]\n";
  for (const auto &[name, value] : m_values)
    content += to_utf8_string(name) + "=" + to_utf8_string(value) + "\n";
  FILE *fp = _wfopen(m_file_name.c_str(), L"ab");
  if (fp == nullptr)
    return;
  fwrite(content.data(), 1, content.size(), fp);
  fclose(fp);
}

void IniWorker::process(const wchar_t *name, std::wstring &value, std::wstring_view default_value,
                        bool in_quotes) const {
  switch (m_action) {
  case Action::save: {
    if (value == default_value)
      return;
    std::wstring str = value;
    if (in_quotes)
      str = L'"' + str + L'"';
    m_values.emplace_back(name, str); // Qt port: written when done
  }
    return;
  case Action::load: {
    // Qt port: the value as read from the file, or the default
    std::wstring str(default_value);
    const auto key = lower_name(name);
    for (const auto &[stored_name, stored_value] : m_values) {
      if (stored_name == key) {
        str = stored_value;
        break;
      }
    }

    if (in_quotes) {
      auto str_v = std::wstring_view(str);
      if (str_v.size() < 2 || str_v.front() != L'"' || str_v.back() != L'"') { // Qt port: also when it is too short for its quotes
        value = default_value;
        return;
      }

      if (!str_v.empty())
        str_v.remove_prefix(1);
      if (!str_v.empty())
        str_v.remove_suffix(1);
      str = str_v;
    }
    value = str;
  }
  }
}

void IniWorker::process(const wchar_t *name, int &value, int default_value) const {
  switch (m_action) {
  case Action::save: {
    auto str = std::to_wstring(value);
    return process(name, str, std::to_wstring(default_value));
  }
  case Action::load: {
    std::wstring str;
    process(name, str, std::to_wstring(default_value));
    size_t idx;
    try {
      auto new_val = std::stoi(str, &idx, 10);
      value = new_val;
    } catch (...) {
      value = default_value;
    }
    break;
  }
  }
}

void IniWorker::process_utf8(const wchar_t *name, std::string &value, const char *default_value,
                             bool in_quotes) const {
  switch (m_action) {
  case Action::save: {
    auto str = utf8_to_wstring(value.c_str());
    process(name, str, utf8_to_wstring(default_value), in_quotes);
  }
    return;
  case Action::load: {
    std::wstring str;
    process(name, str, utf8_to_wstring(default_value), in_quotes);
    value = to_utf8_string(str);
  }
  }
}

void IniWorker::process(const wchar_t *name, bool &value, bool default_value) const {
  auto int_value = value ? 1 : 0;
  process(name, int_value, default_value ? 1 : 0);
  value = int_value != 0;
}
