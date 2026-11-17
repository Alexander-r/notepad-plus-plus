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

#include "Utility.h"
#include "common/move_only.h"

// Qt port: the part of the PPL's tasks that the plugin uses, cancellation, in place of <ppltasks.h>. A task runs on
// the plugin's thread pool, and its result comes back to the GUI thread through a queued call, where Windows posts a
// registered message to a window.
#include <atomic>

namespace concurrency {
class cancellation_token {
public:
  explicit cancellation_token(std::shared_ptr<const std::atomic<bool>> canceled) : m_canceled(std::move(canceled)) {}
  bool is_canceled() const { return m_canceled->load(); }

private:
  std::shared_ptr<const std::atomic<bool>> m_canceled;
};

class cancellation_token_source {
public:
  cancellation_token get_token() const { return cancellation_token(m_canceled); }
  void cancel() { m_canceled->store(true); }

private:
  std::shared_ptr<std::atomic<bool>> m_canceled = std::make_shared<std::atomic<bool>>(false);
};
} // namespace concurrency
namespace Concurrency = concurrency;

// Qt port: runs the task on the plugin's threads, and the callback on the GUI thread
void run_in_background(std::function<void()> task);
void run_on_gui_thread(std::function<void()> callback);
// Qt port: waits for the tasks, which have to end before the plugin is unloaded
void wait_for_background_tasks();

class CallbackData {
public:
  std::weak_ptr<void> alive_status;
  std::function<void ()> callback;
};

class TaskWrapper {
private:
  using AliveStatusType = std::shared_ptr<concurrency::cancellation_token_source>;
  using Self = TaskWrapper;

public:
  explicit TaskWrapper(HWND target_hwnd);
  TaskWrapper(Self &&) = default;
  Self &operator=(Self &&) = default;
  ~TaskWrapper();
  void reset_alive_status();
  void cancel();

  template <typename ActionType, typename GuiCallbackType>
  void do_deferred(ActionType action, GuiCallbackType gui_callback) {
    reset_alive_status();
    // Qt port: run_in_background in place of concurrency::create_task, and run_on_gui_thread in place of PostMessage
    auto task = std::make_shared<std::function<void()>>(
        [
          action = std::move(action),
          guiCallback = std::move(gui_callback),
          as = weaken(m_alive_status),
          ctoken = m_alive_status->get_token()
        ]() mutable {
          auto ret = action(ctoken);
          if (as.expired() || ctoken.is_canceled())
            return;

          auto cb_data = std::make_shared<CallbackData>();
          cb_data->callback = [guiCallback = std::move(guiCallback), ret = std::make_shared<decltype(ret)>(std::move(ret))]()mutable { guiCallback(std::move(*ret)); };
          cb_data->alive_status = as;
          run_on_gui_thread([cb_data]() {
            if (!cb_data->alive_status.expired())
              cb_data->callback();
          });
        });
    run_in_background([task]() { (*task)(); });
  }

private:
  HWND m_target_hwnd = nullptr;
  AliveStatusType m_alive_status;
  move_only<bool> m_valid;
};
