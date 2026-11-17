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

// Qt port: written anew with Qt, in place of the StaticDialog and Window of Notepad++ that the plugin had. A dialog
// builds its controls in build_form(), each with its ID in the "nppId" property as the dialogs of Notepad++ have it,
// and connects them to the code that run_dlg_proc ran for them. init_dialog() is what WM_INITDIALOG did.

#pragma once

#include <QPointer>
#include <QWidget>

#ifndef IDC_STATIC
#define IDC_STATIC (-1) // as the resource compiler has it, for controls without an ID of their own
#endif

class QDialogButtonBox;
class QGridLayout;

class StaticDialog {
public:
  StaticDialog() = default;
  virtual ~StaticDialog();
  StaticDialog(const StaticDialog &) = delete;
  StaticDialog &operator=(const StaticDialog &) = delete;

  virtual void init(HINSTANCE h_inst, HWND parent) {
    _hInst = h_inst;
    _hParent = parent;
  }

  // A modeless dialog over the parent, as CreateDialogParam makes it, or with is_child a page in the parent
  virtual void create(int dialog_id, bool is_rtl = false, bool is_child = false);
  bool isCreated() const { return !_hSelf.isNull(); }
  void goToCenter();
  void display(bool to_show = true, bool activate = true) const;
  virtual void destroy();
  HWND getHSelf() const { return _hSelf.data(); }
  HWND getHParent() const { return _hParent; }
  int getHeight() const { return _hSelf ? _hSelf->height() : 0; }

  // The control with this ID
  QWidget *item(int id) const;
  template <typename ControlType> ControlType *item_as(int id) const { return qobject_cast<ControlType *>(item(id)); }

protected:
  virtual void build_form(QWidget *form) = 0;
  virtual void init_dialog() {}

  // A control with its ID, for build_form
  template <typename ControlType> static ControlType *with_id(ControlType *control, int id) {
    control->setProperty("nppId", id);
    return control;
  }
  // OK, Cancel and the like, with their IDs, and Return for the default button, as on Windows
  static QDialogButtonBox *make_buttons(QWidget *form, std::initializer_list<std::pair<int, std::wstring>> buttons, int default_id);

  HINSTANCE _hInst = nullptr;
  HWND _hParent = nullptr;
  QPointer<QWidget> _hSelf;
  int m_dialog_id = 0;
};

// Qt port: the strings of the plugin as Qt's
QString qstr(const std::wstring &str);
QString qstr(std::wstring_view str);
QString qstr(const wchar_t *str);
