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

// Qt port: written anew with Qt, see StaticDialog.h

#include "StaticDialog.h"

#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QScreen>

StaticDialog::~StaticDialog() {
  destroy();
}

void StaticDialog::create(int dialog_id, bool is_rtl, bool is_child) {
  m_dialog_id = dialog_id;
  QWidget *form = nullptr;
  if (is_child) {
    form = new QWidget(_hParent);
  } else {
    auto *dialog = new QDialog(_hParent);
    dialog->setModal(false);
    form = dialog;
  }
  // The template's ID stays in m_dialog_id: a dialog has no control ID, and the IDs of templates repeat those of controls
  if (is_rtl)
    form->setLayoutDirection(Qt::RightToLeft);
  _hSelf = form;
  build_form(form);
  init_dialog();
}

void StaticDialog::goToCenter() {
  if (!_hSelf)
    return;
  _hSelf->adjustSize();
  const QWidget *parent = _hParent != nullptr ? _hParent->window() : nullptr;
  QRect area = parent != nullptr ? parent->frameGeometry() : _hSelf->screen()->availableGeometry();
  QRect rect = _hSelf->frameGeometry();
  rect.moveCenter(area.center());
  _hSelf->move(rect.topLeft());
}

void StaticDialog::display(bool to_show, bool activate) const {
  if (!_hSelf)
    return;
  _hSelf->setVisible(to_show);
  if (to_show && activate) {
    _hSelf->raise();
    _hSelf->activateWindow();
  }
}

void StaticDialog::destroy() {
  // The dialog goes with its parent, so it may be gone already. Once the application has gone, at the exit of the
  // process, Qt's widgets cannot be deleted any more, and are left to it.
  if (QCoreApplication::instance() != nullptr)
    delete _hSelf.data();
}

QWidget *StaticDialog::item(int id) const {
  if (!_hSelf)
    return nullptr;
  const QList<QWidget *> controls = _hSelf->findChildren<QWidget *>();
  for (QWidget *control : controls) {
    if (control->property("nppId").toInt() == id)
      return control;
  }
  return nullptr;
}

QDialogButtonBox *StaticDialog::make_buttons(QWidget *form, std::initializer_list<std::pair<int, std::wstring>> buttons, int default_id) {
  auto *box = new QDialogButtonBox(form);
  for (const auto &[id, text] : buttons) {
    auto *button = with_id(new QPushButton(qstr(text), box), id);
    button->setAutoDefault(false);
    button->setDefault(id == default_id);
    box->addButton(button, id == IDCANCEL ? QDialogButtonBox::RejectRole : QDialogButtonBox::ActionRole);
  }
  return box;
}

QString qstr(const std::wstring &str) { return QString::fromStdWString(str); }
QString qstr(std::wstring_view str) { return QString::fromWCharArray(str.data(), static_cast<qsizetype>(str.size())); }
QString qstr(const wchar_t *str) { return str != nullptr ? QString::fromWCharArray(str) : QString(); }
