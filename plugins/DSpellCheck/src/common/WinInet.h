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

// Qt port: the WinInet wrapper of the plugin, with its interface, written anew with Qt's network classes. The calls
// block as WinInet's do, and are made on the plugin's threads.

#pragma once
#include "remap_rvalues_for.h"

#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>

// The access types of InternetOpen that the plugin uses
inline constexpr DWORD INTERNET_OPEN_TYPE_PRECONFIG = 0;
inline constexpr DWORD INTERNET_OPEN_TYPE_DIRECT = 1;
inline constexpr DWORD INTERNET_OPEN_TYPE_PROXY = 3;

namespace WinInet {
// The settings of the connection: the agent, the proxy as host:port, and the timeouts
class GlobalHandle {
public:
  GlobalHandle();
  GlobalHandle(const wchar_t *agent, DWORD access_type,
               const wchar_t *proxy_string,
               const wchar_t *proxy_bypass = nullptr, DWORD flags = 0);
  void set_connect_timeout(DWORD ms);
  void set_send_timeout(DWORD ms);
  void set_receive_timeout(DWORD ms);
  GlobalHandle(GlobalHandle &&) = default;
  GlobalHandle &operator=(GlobalHandle &&) = default;
  ~GlobalHandle();

  std::wstring agent;
  DWORD access_type = INTERNET_OPEN_TYPE_DIRECT;
  std::wstring proxy_string;
  DWORD timeout_ms = 0;
};

class CreateGlobalHandle {
  using Self = CreateGlobalHandle;

public:
  Self &agent(const wchar_t *agent) & {
    m_agent = agent;
    return *this;
  }
  REMAP_RVALUES_FOR(agent)

  Self &access_type(DWORD access_type) & {
    m_access_type = access_type;
    return *this;
  }
  REMAP_RVALUES_FOR(access_type)

  Self &proxy_string(const wchar_t *proxy_string) & {
    m_proxy_string = proxy_string;
    return *this;
  }
  REMAP_RVALUES_FOR(proxy_string)

  operator GlobalHandle() const;

private:
  const wchar_t *m_agent = nullptr;
  DWORD m_access_type = INTERNET_OPEN_TYPE_DIRECT;
  const wchar_t *m_proxy_string = nullptr;
};

// A URL to read, with the settings it is read with
class UrlHandle {
public:
  UrlHandle(const GlobalHandle &global, const wchar_t *url);
  UrlHandle(UrlHandle &&) = default;
  UrlHandle &operator=(UrlHandle &&) = default;
  void set_proxy_username(std::wstring_view username);
  void set_proxy_password(std::wstring_view password);
  ~UrlHandle();

  std::wstring agent;
  DWORD access_type = INTERNET_OPEN_TYPE_DIRECT;
  std::wstring proxy_string;
  std::wstring proxy_username;
  std::wstring proxy_password;
  DWORD timeout_ms = 0;
  std::wstring url;
};

class WinInetOpenUrl {
  using Self = WinInetOpenUrl;

public:
  WinInetOpenUrl(const GlobalHandle &inet_handle, const wchar_t *url)
    : m_inet_handle(inet_handle), m_url(url) {
  }

  operator UrlHandle() { return UrlHandle(m_inet_handle, m_url); }

private:
  const GlobalHandle &m_inet_handle;
  const wchar_t *m_url;
};

// Returns false if downloading was cancelled. A failure of the network throws an exception, but an HTTP error does not:
// its page is the content, as with WinInet.
bool download_file(const UrlHandle &handle, std::ostream &stream, std::function<bool(int bytes_read, int total_bytes)> callback = {});
// Returns content of target text file or throws an exception
std::string download_text_file(const UrlHandle &handle);
} // namespace WinInet
