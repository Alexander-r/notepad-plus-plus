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

// Qt port: written anew with Qt's network classes, for the interface of WinInet.h

#include "WinInet.h"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>
#include <QTimer>
#include <QUrl>

#include <sstream>
#include <stdexcept>

namespace WinInet {
CreateGlobalHandle::operator GlobalHandle() const { return {m_agent, m_access_type, m_proxy_string}; }

GlobalHandle::GlobalHandle() = default;

GlobalHandle::GlobalHandle(const wchar_t *agent_arg, DWORD access_type_arg, const wchar_t *proxy_string_arg, const wchar_t *, DWORD)
  : agent(agent_arg != nullptr ? agent_arg : L""), access_type(access_type_arg), proxy_string(proxy_string_arg != nullptr ? proxy_string_arg : L"") {
}

void GlobalHandle::set_connect_timeout(DWORD ms) { timeout_ms = ms; }
void GlobalHandle::set_send_timeout(DWORD ms) { timeout_ms = ms; }
void GlobalHandle::set_receive_timeout(DWORD ms) { timeout_ms = ms; }

GlobalHandle::~GlobalHandle() = default;

UrlHandle::UrlHandle(const GlobalHandle &global, const wchar_t *url_arg)
  : agent(global.agent), access_type(global.access_type), proxy_string(global.proxy_string), timeout_ms(global.timeout_ms), url(url_arg) {
}

void UrlHandle::set_proxy_username(std::wstring_view username) { proxy_username = username; }
void UrlHandle::set_proxy_password(std::wstring_view password) { proxy_password = password; }

UrlHandle::~UrlHandle() = default;

// The tests of the plugin answer in place of the servers: with NPP_TEST_DSPELLCHECK_SERVER set to a local address,
// https://host/path is asked of that address as /host/path
static QUrl request_url(const std::wstring &url) {
  QUrl result(QString::fromStdWString(url));
  const QString test_server = qEnvironmentVariable("NPP_TEST_DSPELLCHECK_SERVER");
  if (!test_server.isEmpty())
    result = QUrl(test_server + u'/' + result.host() + result.path(QUrl::FullyEncoded) + (result.hasQuery() ? u'?' + result.query(QUrl::FullyEncoded) : QString()));
  return result;
}

bool download_file(const UrlHandle &handle, std::ostream &stream, std::function<bool(int bytes_read, int total_bytes)> callback) {
  QNetworkAccessManager manager;
  if (handle.access_type == INTERNET_OPEN_TYPE_PROXY && !handle.proxy_string.empty()) {
    const QString proxy = QString::fromStdWString(handle.proxy_string);
    const int colon = proxy.lastIndexOf(u':');
    QNetworkProxy network_proxy(QNetworkProxy::HttpProxy, colon > 0 ? proxy.left(colon) : proxy, colon > 0 ? static_cast<quint16>(proxy.mid(colon + 1).toUInt()) : 8080,
                                QString::fromStdWString(handle.proxy_username), QString::fromStdWString(handle.proxy_password));
    manager.setProxy(network_proxy);
  } else if (handle.access_type == INTERNET_OPEN_TYPE_DIRECT) {
    manager.setProxy(QNetworkProxy::NoProxy);
  }

  QNetworkRequest request(request_url(handle.url));
  if (!handle.agent.empty())
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromStdWString(handle.agent));
  if (handle.timeout_ms > 0)
    request.setTransferTimeout(static_cast<int>(handle.timeout_ms));

  QNetworkReply *reply = manager.get(request);
  QEventLoop loop;
  bool cancelled = false;
  int bytes_read_total = 0;
  const auto total_bytes = [reply]() {
    const QVariant length = reply->header(QNetworkRequest::ContentLengthHeader);
    return length.isValid() ? length.toInt() : 0;
  };
  QObject::connect(reply, &QNetworkReply::readyRead, &loop, [&]() {
    const QByteArray data = reply->readAll();
    stream.write(data.constData(), data.size());
    bytes_read_total += static_cast<int>(data.size());
    if (callback && !callback(bytes_read_total, total_bytes())) {
      cancelled = true;
      reply->abort();
    }
  });
  // As the first call of the original, before anything is read, but once the length is known from the headers
  QObject::connect(reply, &QNetworkReply::metaDataChanged, &loop, [&]() {
    if (bytes_read_total == 0 && callback && !callback(0, total_bytes())) {
      cancelled = true;
      reply->abort();
    }
  });
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  if (!reply->isFinished())
    loop.exec();

  const QByteArray rest = reply->readAll();
  stream.write(rest.constData(), rest.size());
  const bool has_http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid();
  const auto error = reply->error();
  const QString error_string = reply->errorString();
  delete reply;
  if (cancelled)
    return false;
  if (error != QNetworkReply::NoError && !has_http_status)
    throw std::runtime_error(error_string.toStdString());
  return true;
}

std::string download_text_file(const UrlHandle &handle) {
  std::stringstream ss;
  download_file(handle, ss);
  return ss.str();
}
} // namespace WinInet
