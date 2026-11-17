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

#include "MenuItem.h"

#include "plugin/Constants.h"
#include "ui/StaticDialog.h"

#include <QAction>
#include <QMenu>

MenuItem::MenuItem(const wchar_t *text_arg, int id_arg,
                   bool separator_arg /*= false*/) {
  text = text_arg;
  id = static_cast<BYTE>(id_arg);
  separator = separator_arg;
}

// Qt port: a QAction, or for children a submenu, that sends its command to Notepad++ as a click on a Windows menu
// item does. Notepad++ hands commands of the range allocated with NPPM_ALLOCATECMDID to the plugin's messageProc.
void MenuItem::append_to_menu(HMENU menu, int insert_pos) const {
  QAction *before = (insert_pos >= 0 && insert_pos < menu->actions().size()) ? menu->actions().at(insert_pos) : nullptr;
  QAction *action = nullptr;
  if (separator) {
    action = new QAction(menu);
    action->setSeparator(true);
  } else {
    const int cmd_id = !get_use_allocated_ids() ? MAKEWORD(id, menu_id::plguin_menu) : get_context_menu_id_start() + id;
    action = new QAction(qstr(text), menu);
    action->setData(cmd_id);
    if (!children.empty()) {
      auto *submenu = new QMenu(menu);
      for (auto &child : children) {
        child.append_to_menu(submenu);
      }
      action->setMenu(submenu);
    } else {
      QObject::connect(action, &QAction::triggered, action, [cmd_id]() { ::SendMessage(get_npp_window(), WM_COMMAND, static_cast<WPARAM>(cmd_id), 0); });
    }
  }
  menu->insertAction(before, action);
}
