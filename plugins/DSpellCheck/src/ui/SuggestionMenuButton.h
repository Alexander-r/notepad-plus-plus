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

#include "StaticDialog.h"

// Qt port: the button is a small widget over the text of the editor, a child of its viewport, where Windows has a
// layered popup window in screen coordinates, which Wayland would not place. Its opacity is a QGraphicsOpacityEffect.

class SpellChecker;
class NppInterface;
class Settings;
class ContextMenuHandler;

class SuggestionMenuButton : public StaticDialog {
public:
  void do_dialog();
  int get_result() const;
  bool is_pressed() const;
  SuggestionMenuButton(HINSTANCE h_inst, HWND parent, NppInterface &npp,
                    ContextMenuHandler &context_menu_handler,
                    const Settings &settings);
  void show_suggestion_menu();
  void on_settings_changed();
  void set_transparency();
  // Places the button in the editor's text area, at the point of its viewport
  void move_to(HWND editor, int x, int y, int size);

  // What the window procedure did for the mouse and painting
  void on_mouse_enter();
  void on_mouse_leave();
  void on_mouse_press();
  void on_mouse_release();
  void on_wheel(class QWheelEvent *event);
  void paint(class QPainter &painter);

protected:
  void build_form(QWidget *form) override;

private:
  bool m_state_pressed = false;
  bool m_state_hovered = false;
  bool m_state_menu = false;
  NppInterface &m_npp;
  const Settings &m_settings;
  ContextMenuHandler &m_context_menu_handler;
};
