#include "npp/TextUtils.h"

#include "common/Utility.h"
#include "common/utf8.h"

// Qt port: the character of a UTF-8 sequence, in one wchar_t of 32 bits, where Windows converts it to UTF-16 and
// loses those beyond the first 65536. A broken sequence gives U+FFFD, as MultiByteToWideChar does.
static wchar_t decode_utf8_char(const char *begin, const char *end) {
  const auto lead = static_cast<unsigned char>(*begin);
  if (lead < 0x80)
    return lead;
  int count = 0;
  char32_t value = 0;
  if ((lead & 0xE0) == 0xC0) {
    count = 1;
    value = lead & 0x1F;
  } else if ((lead & 0xF0) == 0xE0) {
    count = 2;
    value = lead & 0x0F;
  } else if ((lead & 0xF8) == 0xF0) {
    count = 3;
    value = lead & 0x07;
  } else {
    return 0xFFFD;
  }
  if (end - begin != count + 1)
    return 0xFFFD;
  for (int i = 1; i <= count; ++i) {
    const auto c = static_cast<unsigned char>(begin[i]);
    if ((c & 0xC0) != 0x80)
      return 0xFFFD;
    value = (value << 6) | (c & 0x3F);
  }
  return static_cast<wchar_t>(value);
}

MappedWstring utf8_to_mapped_wstring(std::string_view str) {
  if (str.empty())
    return {};
  ptrdiff_t len = str.length();
  std::vector<wchar_t> buf;
  std::vector<TextPosition> mapping;
  buf.reserve(len);
  mapping.reserve(len);
  auto it = str.data();
  // sadly this garbage skipping is required due to bad find prev mistake algorithm
  while (utf8_is_cont(*it))
    ++it;
  size_t char_cnt = 1;
  while (it - str.data() < len) {
    auto next = utf8_inc(it);
    buf.resize(char_cnt);
    buf[char_cnt - 1] = decode_utf8_char(it, std::min(next, str.data() + len)); // Qt port: in place of MultiByteToWideChar
    mapping.push_back(it - str.data());
    ++char_cnt;
    it = next;
  }
  mapping.push_back(it - str.data());
  return {std::wstring{buf.begin(), buf.end()}, std::move(mapping)};
}

MappedWstring to_mapped_wstring(std::string_view str) {
  if (str.empty())
    return {};
  std::vector<TextPosition> mapping(str.length() + 1);
  std::iota(mapping.begin(), mapping.end(), 0);
  return {to_wstring(str), std::move(mapping)};
}
