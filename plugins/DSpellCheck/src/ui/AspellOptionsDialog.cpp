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

#include "AspellOptionsDialog.h"

#include "common/winapi.h" // Qt port: the file name in its case

// Qt port: the dialog's controls
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include "plugin/Settings.h"
#include "spellers/SpellerContainer.h"

AspellOptionsDialog::AspellOptionsDialog(HINSTANCE h_inst, HWND parent, const Settings &settings, const SpellerContainer &spellers)
  : m_settings(settings), m_spellers(spellers) {
  StaticDialog::init(h_inst, parent); // Qt port: StaticDialog has init
}

void AspellOptionsDialog::do_dialog() {
  if (!isCreated()) {
    create(IDD_ASPELL_OPTIONS);
  }
  update_controls();
  goToCenter();
  display();
}

void AspellOptionsDialog::apply_choice() {
  auto s = m_settings.modify();
  s->data.aspell_allow_run_together_words = m_allow_run_together_cb->isChecked(); // Qt port: the controls of Qt
  s->data.aspell_personal_dictionary_path = m_aspell_personal_path_le->text().toStdWString();
  if (s->data.aspell_personal_dictionary_path == m_spellers.get_aspell_default_personal_dictionary_path())
    s->data.aspell_personal_dictionary_path = L"";
}

void AspellOptionsDialog::update_controls() {
  // Qt port: the controls of Qt
  m_allow_run_together_cb->setChecked(m_settings.data.aspell_allow_run_together_words);
  m_aspell_personal_path_le->setText(qstr(m_settings.data.aspell_personal_dictionary_path.empty() ? m_spellers.get_aspell_default_personal_dictionary_path()
                                                                                                   : m_settings.data.aspell_personal_dictionary_path));
}

// Qt port: the form of IDD_ASPELL_OPTIONS, with the controls connected as run_dlg_proc handled them
void AspellOptionsDialog::build_form(QWidget *form) {
  form->setWindowTitle(QStringLiteral("Aspell Options"));
  auto *layout = new QGridLayout(form);
  auto *group = new QGroupBox(QStringLiteral("Personal Dictionary Path"), form);
  auto *group_layout = new QHBoxLayout(group);
  m_aspell_personal_path_le = with_id(new QLineEdit(group), IDC_ASPELL_PERSONAL_PATH_LE);
  auto *browse = with_id(new QPushButton(QStringLiteral("..."), group), IDC_BROWSEPDICTIONARYPATH);
  browse->setAutoDefault(false);
  group_layout->addWidget(m_aspell_personal_path_le, 1);
  group_layout->addWidget(browse);
  m_allow_run_together_cb = with_id(new QCheckBox(QStringLiteral("Allow Run-Together Words"), form), IDC_ASPELL_RUNTOGETHER_CB);
  layout->addWidget(group, 0, 0);
  layout->addWidget(m_allow_run_together_cb, 1, 0);
  layout->addWidget(make_buttons(form, {{IDOK, L"OK"}, {IDCANCEL, L"Cancel"}}, IDOK), 2, 0);
  form->setMinimumWidth(m_aspell_personal_path_le->fontMetrics().averageCharWidth() * 60);

  QObject::connect(browse, &QPushButton::clicked, form, [this]() {
    auto cur_path = WinApi::get_edit_text(m_aspell_personal_path_le);
    auto path = WinApi::browse_for_directory(_hSelf, cur_path.data());
    if (path)
      m_aspell_personal_path_le->setText(qstr(*path));
  });
  QObject::connect(item_as<QPushButton>(IDOK), &QPushButton::clicked, form, [this]() {
    apply_choice();
    display(false);
  });
  const auto cancel = [this]() {
    update_controls(); // Reset all settings
    display(false);
  };
  QObject::connect(item_as<QPushButton>(IDCANCEL), &QPushButton::clicked, form, cancel);
  QObject::connect(static_cast<QDialog *>(form), &QDialog::rejected, form, cancel);
}

void AspellOptionsDialog::init_dialog() {}
