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

#include "SelectMultipleLanguagesDialog.h"

#include "common/Utility.h"
#include "common/string_utils.h"
#include "plugin/Constants.h"
#include "plugin/Plugin.h"
#include "plugin/resource.h"
#include "plugin/Settings.h"
#include "spellers/LanguageInfo.h"
#include "spellers/SpellerContainer.h"

// Qt port: the dialog's controls, with a QListWidget of check boxes for the CheckedListBox
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

SelectMultipleLanguagesDialog::SelectMultipleLanguagesDialog(HINSTANCE h_inst, HWND parent, const Settings &settings, const SpellerContainer &speller_container)
  : m_settings(settings), m_speller_container(speller_container) {
  StaticDialog::init(h_inst, parent); // Qt port: StaticDialog has init
  m_settings.settings_changed.connect([this] { update_list(); });
  m_speller_container.speller_status_changed.connect([this] { update_list(); });
}

void SelectMultipleLanguagesDialog::do_dialog() {
  if (!isCreated()) {
    create(IDD_CHOOSE_MULTIPLE_LANGUAGES);
  }
  goToCenter();
  display();
  m_h_lang_list->setFocus(); // Qt port: setFocus
}

QListWidget *SelectMultipleLanguagesDialog::get_list_box() { return m_h_lang_list; }

void SelectMultipleLanguagesDialog::update_list() {
  if (m_h_lang_list == nullptr)
    return;

  // Qt port: the items of the QListWidget, in place of ListBox_ and CheckedListBox_ calls
  m_h_lang_list->clear();
  auto langs = m_speller_container.get_available_languages();
  for (auto &lang : langs) {
    auto *item = new QListWidgetItem(qstr(lang.alias_name), m_h_lang_list);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Unchecked);
  }
  for (auto &token : make_delimiter_tokenizer(m_settings.get_active_multi_languages(), LR"(\|)").get_all_tokens()) {
    int index = -1;
    int i = 0;
    for (auto &lang : langs) {
      if (token == lang.orig_name) {
        index = i;
        break;
      }
      ++i;
    }
    if (index != -1)
      m_h_lang_list->item(index)->setCheckState(Qt::Checked); // Qt port: the QListWidget
  }
}

void SelectMultipleLanguagesDialog::apply() {
  int count = m_h_lang_list->count(); // Qt port: the QListWidget
  std::wstring buf;
  bool first = true;
  auto langs = m_speller_container.get_available_languages();
  for (int i = 0; i < count; i++) {
    if (m_h_lang_list->item(i)->checkState() == Qt::Checked) {
      if (!first) {
        buf += L"|";
      } else
        first = false;

      buf += langs[i].orig_name;
    }
  }
  {
    auto mut = m_settings.modify();
    mut->get_active_multi_languages() = buf;
    mut->get_active_language() = multiple_language_alias;
  }
}

// Qt port: the form of IDD_CHOOSE_MULTIPLE_LANGUAGES, with the controls connected as run_dlg_proc handled them
void SelectMultipleLanguagesDialog::build_form(QWidget *form) {
  form->setWindowTitle(QStringLiteral("Choose Languages"));
  auto *layout = new QVBoxLayout(form);
  auto *warning = new QLabel(QStringLiteral("Warning: Selecting too many languages may rise CPU usage. \nAlso language guessing for suggestions is not working very "
                                            "good when multiple languages are selected."),
                             form);
  warning->setWordWrap(true);
  layout->addWidget(warning);
  m_h_lang_list = with_id(new QListWidget(form), IDC_LANGLIST);
  // About as many rows as the original shows, where Qt's default height has room for only a few
  m_h_lang_list->setMinimumHeight(m_h_lang_list->fontMetrics().lineSpacing() * 18);
  layout->addWidget(m_h_lang_list, 1);
  auto *buttons = make_buttons(form, {{IDOK, L"OK"}, {IDCANCEL, L"Cancel"}}, IDOK);
  buttons->setCenterButtons(true);
  layout->addWidget(buttons);
  form->resize(form->fontMetrics().averageCharWidth() * 45, form->fontMetrics().height() * 28);

  QObject::connect(item_as<QPushButton>(IDOK), &QPushButton::clicked, form, [this]() {
    apply();
    display(false);
  });
  const auto cancel = [this]() {
    update_list();
    display(false);
  };
  QObject::connect(item_as<QPushButton>(IDCANCEL), &QPushButton::clicked, form, cancel);
  QObject::connect(static_cast<QDialog *>(form), &QDialog::rejected, form, cancel);
}

void SelectMultipleLanguagesDialog::init_dialog() { update_list(); }
