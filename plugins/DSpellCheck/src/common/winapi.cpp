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

// Qt port: written anew with Qt, for the helpers in winapi.h

#include "winapi.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QTimer>

namespace WinApi {
std::wstring get_edit_text(HWND edit) {
  if (const auto *line_edit = qobject_cast<const QLineEdit *>(edit))
    return line_edit->text().toStdWString();
  return {};
}

// The file is moved, where Windows also resets its security descriptor to that of its new folder: permissions here
// come with the file, and moving within the user's folders keeps them right
bool move_file_and_reset_security_descriptor(const wchar_t *from, const wchar_t *to) {
  const QString target = QString::fromWCharArray(to);
  QFile::remove(target);
  return QFile::rename(QString::fromWCharArray(from), target);
}

std::optional<std::wstring> browse_for_directory(HWND parent_wnd, const wchar_t *initial_path) {
  const QString dir = QFileDialog::getExistingDirectory(parent_wnd, QString::fromStdWString(rc_str(IDS_PICK_A_DIRECTORY)),
                                                        initial_path != nullptr ? QString::fromWCharArray(initial_path) : QString());
  if (dir.isEmpty())
    return {};
  return QDir::toNativeSeparators(dir).toStdWString();
}

bool delete_file(const wchar_t *path) {
  QFile file(QString::fromWCharArray(path));
  file.setPermissions(file.permissions() | QFile::WriteOwner);
  return file.remove();
}

// From the class of an ELF library. A name without a folder, which the loader looks for, counts as fitting.
std::optional<int> library_bitness(const wchar_t *path) {
  QFile file(QString::fromWCharArray(path));
  if (!QFileInfo(file).isAbsolute() || !file.exists())
    return static_cast<int>(sizeof(void *) * 8);
  if (!file.open(QIODevice::ReadOnly))
    return {};
  const QByteArray header = file.read(5);
  if (header.size() < 5 || !header.startsWith("\x7f" "ELF"))
    return {};
  return header[4] == 2 ? 64 : 32;
}

Timer::Timer(HWND hwnd)
  : m_timer(std::make_unique<QTimer>()) {
  (void)hwnd;
  QObject::connect(m_timer.get(), &QTimer::timeout, m_timer.get(), [this]() { on_timer_tick(); });
}

Timer::~Timer() = default;

void Timer::set_resolution(std::chrono::milliseconds resolution) {
  m_timer->start(resolution);
  current_resolution = resolution;
}

void Timer::stop_timer() {
  m_timer->stop();
  current_resolution = {};
}

bool Timer::is_set() const { return current_resolution.has_value(); }
} // namespace WinApi
