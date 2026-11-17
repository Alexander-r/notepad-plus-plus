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

#include "RemoveDictionariesDialog.h"

#include "common/winapi.h"
#include "core/SpellChecker.h"
#include "plugin/Plugin.h"
#include "plugin/resource.h"
#include "plugin/Settings.h"
#include "spellers/HunspellInterface.h"
#include "spellers/LanguageInfo.h"
#include "spellers/SpellerContainer.h"

// Qt port: the dialog's controls, with a QListWidget of check boxes for the CheckedListBox
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

RemoveDictionariesDialog::RemoveDictionariesDialog(HINSTANCE h_inst, HWND parent, const Settings &settings, const SpellerContainer &speller_container)
  : m_settings(settings), m_speller_container(speller_container) {
  m_settings.settings_changed.connect([this] { update_list(); });
  m_speller_container.speller_status_changed.connect([this] { update_list(); });
  StaticDialog::init(h_inst, parent); // Qt port: StaticDialog has init
  m_settings.settings_changed.connect([this] { update_controls(); });
}

void RemoveDictionariesDialog::do_dialog() {
  if (!isCreated())
    create(IDD_REMOVE_DICS);
  // Qt port: shown when made too, as the dialog of the resources is WS_VISIBLE
  goToCenter();
  display();
  m_lang_list->setFocus();
}

void RemoveDictionariesDialog::update_list() {
  if (m_lang_list == nullptr)
    return;

  // Qt port: the items of the QListWidget, in place of ListBox_ and CheckedListBox_ calls
  std::set<std::wstring> currently_checked;
  for (int i = 0; i < m_lang_list->count(); ++i)
    if (m_lang_list->item(i)->checkState() == Qt::Checked)
      currently_checked.insert(m_cur_lang_list[i].orig_name);

  m_cur_lang_list = m_speller_container.get_available_languages();
  m_lang_list->clear();
  for (auto &lang : m_cur_lang_list) {
    auto *item = new QListWidgetItem(qstr(std::wstring(lang.alias_name) + (lang.for_all_users ? L" [!For All Users]" : L"")), m_lang_list);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(currently_checked.count(lang.orig_name) > 0 ? Qt::Checked : Qt::Unchecked);
  }
}

QListWidget *RemoveDictionariesDialog::get_list_box() { return m_lang_list; }

void RemoveDictionariesDialog::remove_selected() {
  int count = 0;
  bool need_single_reset = false;
  bool need_multi_reset = false;
  bool single_temp, multi_temp;
  for (int i = 0; i < m_lang_list->count(); i++) { // Qt port: the QListWidget
    if (m_lang_list->item(i)->checkState() == Qt::Checked) {
      wchar_t file_name[MAX_PATH];
      for (int j = 0; j < 1 + (m_settings.data.remove_system_dictionaries ? 1 : 0); j++) {
        *file_name = L'\0';
        wcscat(file_name, ((j == 0) ? m_settings.data.hunspell_user_path : m_settings.data.hunspell_system_path).c_str());
        wcscat(file_name, L"/"); // Qt port: '/' 
        wcscat(file_name, m_speller_container.get_available_languages()[i].orig_name.c_str());
        wcscat(file_name, L".aff");
        bool success = WinApi::delete_file(file_name);
        wcsncpy(file_name + wcslen(file_name) - 4, L".dic", 4);
        success = success && WinApi::delete_file(file_name);
        if (m_settings.data.remove_user_dictionaries) {
          wcsncpy(file_name + wcslen(file_name) - 4, L".usr", 4);
          WinApi::delete_file(file_name);
        }
        if (success) {
          file_name[wcslen(file_name) - 4] = L'\0';
          m_speller_container.get_hunspell_speller().update_on_dic_removal(file_name, single_temp, multi_temp);
          need_single_reset |= single_temp;
          need_multi_reset |= multi_temp;
          count++;
        }
      }
    }
  }
  for (int i = 0; i < m_lang_list->count(); i++) // Qt port: the QListWidget
    m_lang_list->item(i)->setCheckState(Qt::Unchecked);
  if (count > 0) {
    update_list();
    m_settings.settings_changed();
    auto text = wstring_printf(rc_str(IDS_PD_DICTIONARIES_REMOVED).c_str(), count);
    MessageBox(_hParent, text.c_str(), L"Dictionaries were removed", MB_OK | MB_ICONINFORMATION);
  }
  if (need_single_reset) {
    auto s = m_settings.modify();
    s->reset_hunspell_lang_to_default();
  }
  if (need_multi_reset) {
    auto s = m_settings.modify();
    s->data.speller_multi_languages[SpellerId::hunspell] = L"";
  }
}

void RemoveDictionariesDialog::update_options() {
  auto mut_settings = m_settings.modify();
  mut_settings->data.remove_user_dictionaries = m_remove_user_dics->isChecked(); // Qt port: the check boxes of Qt
  mut_settings->data.remove_system_dictionaries = m_remove_system->isChecked();
}

void RemoveDictionariesDialog::update_controls() {
  if (m_remove_user_dics == nullptr) // Qt port: before the dialog is made, there is nothing to update
    return;
  m_remove_user_dics->setChecked(m_settings.data.remove_user_dictionaries);
  m_remove_system->setChecked(m_settings.data.remove_system_dictionaries);
}

// Qt port: the form of IDD_REMOVE_DICS, with the controls connected as run_dlg_proc handled them
void RemoveDictionariesDialog::build_form(QWidget *form) {
  form->setWindowTitle(QStringLiteral("Remove Dictionaries"));
  auto *layout = new QVBoxLayout(form);
  layout->addWidget(new QLabel(QStringLiteral("Select Dictionaries for Removal:"), form));
  m_lang_list = with_id(new QListWidget(form), IDC_REMOVE_LANGLIST);
  // About as many rows as the original shows, where Qt's default height has room for only a few
  m_lang_list->setMinimumHeight(m_lang_list->fontMetrics().lineSpacing() * 18);
  m_remove_user_dics = with_id(new QCheckBox(QStringLiteral("Remove Corresponding User Dictionaries"), form), IDC_REMOVE_USER_DICS);
  m_remove_system = with_id(new QCheckBox(QStringLiteral("Remove Dictionaries installed for All Users"), form), IDC_REMOVE_SYSTEM);
  layout->addWidget(m_lang_list, 1);
  layout->addWidget(m_remove_user_dics);
  layout->addWidget(m_remove_system);
  layout->addWidget(make_buttons(form, {{IDOK, L"OK"}, {IDCANCEL, L"Cancel"}}, IDOK));
  form->resize(form->fontMetrics().averageCharWidth() * 50, form->fontMetrics().height() * 30);

  QObject::connect(m_remove_user_dics, &QCheckBox::clicked, form, [this]() { update_options(); });
  QObject::connect(m_remove_system, &QCheckBox::clicked, form, [this]() { update_options(); });
  QObject::connect(item_as<QPushButton>(IDOK), &QPushButton::clicked, form, [this]() {
    display(false);
    remove_selected();
  });
  const auto cancel = [this]() {
    display(false);
    update_list(); // reset checkboxes
  };
  QObject::connect(item_as<QPushButton>(IDCANCEL), &QPushButton::clicked, form, cancel);
  QObject::connect(static_cast<QDialog *>(form), &QDialog::rejected, form, cancel);
}

void RemoveDictionariesDialog::init_dialog() {
  update_list();
  update_controls();
}
