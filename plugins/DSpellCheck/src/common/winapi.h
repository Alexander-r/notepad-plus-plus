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

#pragma once
#include "move_only.h"

#include "lsignal.h"

#include <chrono>
#include <memory>
#include <optional>
#include <string>

class QTimer;

// Qt port: the helpers that the plugin used of the Windows API, written with Qt. The control classes of
// WinApiControls.h went with the Windows dialogs, but for Timer, which is here.
namespace WinApi {
std::wstring get_edit_text(HWND edit);
bool move_file_and_reset_security_descriptor(const wchar_t *from, const wchar_t *to);

std::optional<std::wstring> browse_for_directory(HWND parent_wnd, const wchar_t *initial_path);

bool delete_file(const wchar_t *path);
std::optional<int> library_bitness(const wchar_t *path);

// A timer that ticks at its resolution until stopped, with a QTimer where Windows has SetTimer
class Timer {
public:
  Timer(HWND hwnd);
  ~Timer();
  void set_resolution(std::chrono::milliseconds resolution);
  void stop_timer();
  bool is_set() const;

  lsignal::signal<void()> on_timer_tick;

private:
  std::unique_ptr<QTimer> m_timer;
  std::optional<std::chrono::milliseconds> current_resolution;
};
} // namespace WinApi
