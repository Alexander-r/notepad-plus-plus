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

// Qt port: the dialog with the controls of Qt, without FTP: see DownloadDictionariesDialog.h. The methods that
// remain are upstream's.

#include "DownloadDictionariesDialog.h"

#include "ConnectionSettingsDialog.h"
#include "ProgressDialog.h"
#include "common/Utility.h"
#include "common/ProgressData.h"
#include "common/string_utils.h"
#include "common/winapi.h"
#include "common/WindowsDefs.h"
#include "network/GithubFileListProvider.h"
#include "network/UrlHelpers.h"
#include "plugin/Constants.h"
#include "plugin/Plugin.h"
#include "plugin/resource.h"
#include "plugin/Settings.h"
#include "spellers/HunspellInterface.h"
#include "spellers/SpellerContainer.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>

#include <variant>

void DownloadDictionariesDialog::do_dialog() {
  if (!isCreated())
    create(IDD_DOWNLOADDICS);
  // Qt port: shown when made too, as the dialog of the resources is WS_VISIBLE
  goToCenter();
  display();
  m_h_file_list->setFocus();
}

void DownloadDictionariesDialog::fill_file_list() {
  auto url = *current_address();
  prepare_file_list_update();
  switch (selected_url_type()) {
  case UrlType::github:
    m_github_provider->set_root_path(url);
    m_github_provider->update_file_list();
    break;
  case UrlType::unknown:
    // Qt port: an FTP address, or another that is not GitHub's, cannot be opened here
    update_status(rc_str(IDS_STATUS_URL_OPEN_FAIL).c_str(), COLOR_FAIL);
    break;
  }
}

void DownloadDictionariesDialog::on_display_action() { fill_file_list(); }

DownloadDictionariesDialog::~DownloadDictionariesDialog() = default;

DownloadDictionariesDialog::DownloadDictionariesDialog(HINSTANCE h_inst, HWND parent, const Settings &settings, const SpellerContainer &speller_container)
  : m_settings(settings), m_github_provider(std::make_unique<GitHubFileListProvider>(parent, settings)), m_speller_container(speller_container) {
  m_github_provider->file_list_received.connect([this](const std::vector<FileDescription> &list) { on_new_file_list(list); });
  m_github_provider->error_happened.connect([this](const std::string &description) {
    auto wstr = to_wstring(description);
    replace_all_inplace(wstr, L"\r\n", L" ");
    update_status(wstring_printf(rc_str(IDS_STATUS_NETWORK_ERROR_PS).c_str(), wstr.c_str()).c_str(), COLOR_FAIL);
  });
  m_github_provider->file_downloaded.connect([this](const std::optional<std::string> &error_description, bool was_cancelled) {
    m_message += prev(m_cur)->title + L'\n';
    if (was_cancelled)
      m_cur = m_to_download.end(); // the list being downloaded, which upstream mixed up with the one shown
    else if (!error_description)
      ++m_downloaded_count;
    else
      m_errors_text += "* " + *error_description + "\n";
    start_next_download();
  });
  StaticDialog::init(h_inst, parent);
  m_default_server_names[0] = L"https://github.com/LibreOffice/dictionaries";
  m_settings.settings_changed.connect([this] {
    update_controls();
    update_list_box();
  });
}

void DownloadDictionariesDialog::indicate_that_saving_might_be_needed() { m_check_if_saving_is_needed = true; }

LRESULT DownloadDictionariesDialog::ask_replacement_message(const wchar_t *dic_name) {
  auto [name,_] = apply_alias(dic_name, m_settings.data.language_name_style);
  return MessageBox(_hParent, wstring_printf(rc_str(IDS_DICT_PS_EXISTS_BODY).c_str(), name.c_str()).c_str(), rc_str(IDS_DICT_EXISTS_HEADER).c_str(), MB_YESNO);
}

bool DownloadDictionariesDialog::prepare_downloading() {
  m_downloaded_count = 0;
  m_errors_text.clear();
  m_supposed_downloaded_count = 0;
  m_failure = false;
  m_message = rc_str(IDS_DICTS_COPIED);
  m_to_download.clear();

  m_cancel_pressed = false;
  ProgressDialog *p = get_progress_dlg();
  p->get_progress_data()->set(0, L"");
  p->set_top_message(L"");
  if (!check_for_directory_existence(
      m_settings.data.download_install_dictionaries_for_all_users ? m_settings.data.hunspell_system_path : m_settings.data.hunspell_user_path,
      false,
      _hParent)) // If path doesn't exist we're gonna try to create it
    // else it's finish
  {
    MessageBox(_hParent, rc_str(IDS_CANT_CREATE_DOWNLOAD_DIR).c_str(), rc_str(IDS_NO_DICTS_DOWNLOADED).c_str(), MB_OK | MB_ICONEXCLAMATION);
    return false;
  }
  // Qt port: each item keeps the index of its dictionary, as the list may show only the recognized ones, where
  // upstream took the index of the item in the list
  for (int i = 0; i < m_h_file_list->count(); i++) {
    if (m_h_file_list->item(i)->checkState() == Qt::Checked) {
      m_to_download.push_back(m_current_list[m_h_file_list->item(i)->data(Qt::UserRole).toInt()]);
      ++m_supposed_downloaded_count;
    }
  }
  m_cur = m_to_download.begin();
  return true;
}

void DownloadDictionariesDialog::finalize_downloading() {
  get_progress_dlg()->display(false);
  if (m_failure) {
    MessageBox(_hParent, rc_str(IDS_CANT_WRITE_DICT_FILES).c_str(), rc_str(IDS_NO_DICTS_DOWNLOADED).c_str(), MB_OK | MB_ICONEXCLAMATION);
  } else if (m_downloaded_count > 0) {
    auto text = m_message;
    if (!m_errors_text.empty())
      text += rc_str(IDS_DOWNLOAD_ERRORS_ENCOUNTERED) + to_wstring(m_errors_text);
    MessageBox(_hParent, text.c_str(), rc_str(IDS_DICTS_DOWNLOADED).c_str(), MB_OK | MB_ICONINFORMATION);
  } else if (m_supposed_downloaded_count > 0) // Otherwise - silent
  {
    auto text = rc_str(IDS_ZERO_DICTS_DOWNLOAD);
    if (!m_errors_text.empty())
      text += rc_str(IDS_DOWNLOAD_ERRORS_ENCOUNTERED) + to_wstring(m_errors_text);
    MessageBox(_hParent, text.c_str(), rc_str(IDS_NO_DICTS_DOWNLOADED).c_str(), MB_OK | MB_ICONEXCLAMATION);
  }
  for (int i = 0; i < m_h_file_list->count(); i++)
    m_h_file_list->item(i)->setCheckState(Qt::Unchecked);
  m_settings.settings_changed();
}

void DownloadDictionariesDialog::start_next_download() {
  if (m_cur == m_to_download.end() || m_cancel_pressed)
    return finalize_downloading();

  get_progress_dlg()->set_top_message(wstring_printf(rc_str(IDS_DOWNLOADING_PS).c_str(), m_cur->title.c_str()).c_str());
  if (!download_file()) {
    ++m_cur;
    start_next_download();
    return;
  } else
    get_progress_dlg()->do_dialog();
  ++m_cur;
}

UrlType DownloadDictionariesDialog::selected_url_type() {
  const auto address = *current_address();
  if (UrlHelpers::is_github_url(address))
    return UrlType::github;

  return UrlType::unknown;
}

bool DownloadDictionariesDialog::download_github_file(const std::wstring &title, const std::wstring &path, std::shared_ptr<ProgressData> progress_data) {
  auto local_path = std::wstring(m_settings.get_dictionary_download_path());
  auto local_name = local_path + path.substr(path.rfind(L'/')); // Qt port: the '/' of the URL is the separator here too
  if (PathFileExists(local_name.c_str())) {
    if (ask_replacement_message(title.c_str()) == IDNO) {
      --m_supposed_downloaded_count;
      return false;
    }
    m_speller_container.get_hunspell_speller().dictionary_removed(local_name);
  }

  m_github_provider->download_dictionary(path, local_path, progress_data);
  return true;
}

bool DownloadDictionariesDialog::download_file() {
  switch (selected_url_type()) {
  case UrlType::github:
    return download_github_file(m_cur->title, m_cur->path, get_progress_dlg()->get_progress_data());
  case UrlType::unknown:
    break;
  }
  return false;
}

void DownloadDictionariesDialog::download_selected() {
  if (!prepare_downloading())
    return;

  start_next_download();
}

void ftp_trim(std::wstring &ftp_address) {
  trim_inplace(ftp_address);
  for (auto &c : ftp_address) {
    if (c == L'\\')
      c = L'/';
  }
  std::wstring ftp_prefix = L"ftp://";
  if (ftp_address.find(ftp_prefix) == 0)
    ftp_address.erase(ftp_address.begin(), ftp_address.begin() + ftp_prefix.length());

  std::transform(ftp_address.begin(), std::find(ftp_address.begin(), ftp_address.end(), L'/'), ftp_address.begin(),
                 &towlower); // In dir names upper/lower case could matter
}

void DownloadDictionariesDialog::update_list_box() {
  if (m_h_file_list == nullptr || m_current_list.empty())
    return;

  m_h_file_list->clear();
  int index = 0;
  for (auto &info : m_current_list) {
    if (!(m_settings.data.download_show_only_recognized_dictionaries && !info.was_alias_applied)) {
      auto *item = new QListWidgetItem(qstr(info.title), m_h_file_list);
      item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
      item->setCheckState(Qt::Unchecked);
      item->setData(Qt::UserRole, index); // Qt port: the index of the dictionary, see prepare_downloading
    }
    ++index;
  }
}

void DownloadDictionariesDialog::set_cancel_pressed(bool value) {
  m_cancel_pressed = value;
  m_cur = m_to_download.end();
  m_github_provider->cancel_download();
  finalize_downloading();
}

std::optional<std::wstring> DownloadDictionariesDialog::current_address() const {
  if (m_h_address == nullptr)
    return std::nullopt;
  return m_h_address->currentText().toStdWString(); // Qt port: the text of the combo box, chosen or typed
}

// Qt port: the status in its colour, which WM_CTLCOLORSTATIC gave it
void DownloadDictionariesDialog::update_status(const wchar_t *text, COLORREF status_color) {
  m_h_status->setText(qstr(text));
  QPalette palette = m_h_status->palette();
  palette.setColor(QPalette::WindowText, status_color == COLOR_NEUTRAL ? QPalette().color(QPalette::WindowText)
                                                                        : QColor(GetRValue(status_color), GetGValue(status_color), GetBValue(status_color)));
  m_h_status->setPalette(palette);
}

void DownloadDictionariesDialog::on_new_file_list(std::vector<FileDescription> list) {
  int count = 0;

  for (auto &element : list) {
    count++;
    auto name = element;
  }

  if (count == 0) {
    return update_status(rc_str(IDS_STATUS_DIRECTORY_EMPTY).c_str(), COLOR_WARN);
  }

  m_current_list.clear();
  for (auto &element : list) {
    std::tie(element.title, element.was_alias_applied) = apply_alias(element.title, m_settings.data.language_name_style);
    m_current_list.push_back(element);
  }

  std::sort(m_current_list.begin(), m_current_list.end(), [](const FileDescription &lhs, const FileDescription &rhs) { return lhs.title < rhs.title; });
  update_list_box(); // Used only here and on filter change
  // If it is success when we perhaps should add this address to our list.
  if (m_check_if_saving_is_needed) {
    add_user_server(*current_address());
  }
  update_status(rc_str(IDS_STATUS_LIST_SUCCESS).c_str(), COLOR_OK);
  m_h_install_selected->setEnabled(true);
  update_download_button_availability();
}

void DownloadDictionariesDialog::preserve_current_address_index(Settings &settings) {
  auto mb_address = current_address();
  if (!mb_address)
    return;
  auto address = *mb_address;
  ftp_trim(address);
  int i = 0;
  for (auto def_server : m_default_server_names) {
    ftp_trim(def_server);
    if (address == def_server) {
      settings.data.last_used_address_index = i;
      return;
    }
    ++i;
  };
  i = 0;
  for (auto &server : m_settings.data.server_names) {
    if (address == server) {
      settings.data.last_used_address_index = USER_SERVER_CONST + i;
      return;
    }
    ++i;
  }
  settings.data.last_used_address_index = 0;
}

void DownloadDictionariesDialog::reset_download_combobox() {
  if (m_address_is_set) {
    auto mut_settings = m_settings.modify();
    preserve_current_address_index(*mut_settings);
  }
  const QSignalBlocker blocker(m_h_address); // no CBN_SELCHANGE for changes made here, as on Windows
  m_h_address->clear();
  for (auto &server_name : m_default_server_names) {
    m_h_address->addItem(qstr(server_name));
  }
  for (auto &server_name : m_settings.data.server_names) {
    if (!server_name.empty())
      m_h_address->addItem(qstr(server_name));
  }
  // Qt port: the index of the FTP address that Windows had second goes to the first
  if (m_settings.data.last_used_address_index < USER_SERVER_CONST)
    m_h_address->setCurrentIndex(m_settings.data.last_used_address_index < static_cast<int>(m_default_server_names.size()) ? m_settings.data.last_used_address_index : 0);
  else
    m_h_address->setCurrentIndex(m_settings.data.last_used_address_index - USER_SERVER_CONST + static_cast<int>(m_default_server_names.size()));
  m_address_is_set = true;
}

void DownloadDictionariesDialog::add_user_server(std::wstring server) {
  {
    ftp_trim(server);
    for (auto def_server : m_default_server_names) {
      ftp_trim(def_server);
      if (server == def_server)
        goto add_user_server_cleanup; // Nothing is done in this case
    }
    for (auto added_server : m_settings.data.server_names) {
      ftp_trim(added_server);
      if (server == added_server)
        goto add_user_server_cleanup; // Nothing is done in this case
    }
    auto settings_mut = m_settings.modify();
    // Then we're adding finally
    auto &names = settings_mut->data.server_names;
    std::move(std::next(names.rbegin()), names.rend(), names.rbegin());
    names.front() = server;
  }
add_user_server_cleanup:
  reset_download_combobox();
}

void DownloadDictionariesDialog::prepare_file_list_update() {
  m_h_install_selected->setEnabled(false);
  update_status(rc_str(IDS_STATUS_LOADING).c_str(), COLOR_NEUTRAL);
  m_h_file_list->clear();
  m_current_list.clear();
}

void DownloadDictionariesDialog::refresh() {
  indicate_that_saving_might_be_needed();
  on_display_action();
  if (m_refresh_timer != nullptr)
    m_refresh_timer->stop();
}

void DownloadDictionariesDialog::update_controls() {
  if (m_h_show_only_known == nullptr) // Qt port: before the dialog is made, there is nothing to update
    return;
  m_h_show_only_known->setChecked(m_settings.data.download_show_only_recognized_dictionaries);
  m_h_install_system->setChecked(m_settings.data.download_install_dictionaries_for_all_users);
}

void DownloadDictionariesDialog::update_settings(Settings &settings) {
  settings.data.download_show_only_recognized_dictionaries = m_h_show_only_known->isChecked();
  settings.data.download_install_dictionaries_for_all_users = m_h_install_system->isChecked();
}

void DownloadDictionariesDialog::update_download_button_availability() {
  int cnt = 0;
  for (int i = 0; i < m_h_file_list->count(); i++)
    cnt += m_h_file_list->item(i)->checkState() == Qt::Checked ? 1 : 0;
  m_h_install_selected->setEnabled(cnt > 0);
}

// The form of IDD_DOWNLOADDICS, with the controls connected as run_dlg_proc handled them
void DownloadDictionariesDialog::build_form(QWidget *form) {
  form->setWindowTitle(QStringLiteral("Download Dictionaries Dialog"));
  auto *layout = new QGridLayout(form);
  m_h_address = with_id(new QComboBox(form), IDC_ADDRESS);
  m_h_address->setEditable(true);
  m_h_status = with_id(new QLabel(QStringLiteral("Status"), form), IDC_SERVER_STATUS);
  m_h_status->setWordWrap(true);
  m_h_file_list = with_id(new QListWidget(form), IDC_FILE_LIST);
  // About as many rows as the original shows, where Qt's default height has room for only a few
  m_h_file_list->setMinimumHeight(m_h_file_list->fontMetrics().lineSpacing() * 18);
  m_h_install_system = with_id(new QCheckBox(QStringLiteral("Install for All Users"), form), IDC_INSTALL_SYSTEM);
  m_h_show_only_known = with_id(new QCheckBox(QStringLiteral("Show Only Recognized Ones"), form), IDC_SHOWONLYKNOWN);
  auto *select_proxy = with_id(new QPushButton(QStringLiteral("Connection Settings..."), form), IDC_SELECTPROXY);
  select_proxy->setAutoDefault(false);
  m_h_refresh_btn = with_id(new QPushButton(form), IDC_REFRESH);
  m_h_refresh_btn->setAutoDefault(false);
  m_h_refresh_btn->setIcon(QIcon(QStringLiteral(":/DSpellCheck/arrow_refresh.ico")));
  m_h_refresh_btn->setToolTip(qstr(rc_str(IDS_REFRESH_DICTIONARY_LIST_TOOLTIP)));
  auto *buttons = make_buttons(form, {{IDOK, L"Install Selected"}, {IDCANCEL, L"Exit"}}, IDOK);
  m_h_install_selected = item_as<QPushButton>(IDOK);
  layout->addWidget(m_h_address, 0, 0, 1, 3);
  layout->addWidget(m_h_status, 1, 0, 1, 3);
  layout->addWidget(m_h_file_list, 2, 0, 1, 3);
  layout->setRowStretch(2, 1);
  layout->addWidget(m_h_install_system, 3, 0);
  layout->addWidget(select_proxy, 3, 2, Qt::AlignRight);
  layout->addWidget(m_h_show_only_known, 4, 0);
  layout->addWidget(m_h_refresh_btn, 4, 1, Qt::AlignRight);
  layout->addWidget(buttons, 4, 2);
  form->resize(form->fontMetrics().averageCharWidth() * 60, form->fontMetrics().height() * 30);

  // The address is looked up a second after typing stops, as with SetTimer
  m_refresh_timer = new QTimer(form);
  m_refresh_timer->setSingleShot(true);
  m_refresh_timer->setInterval(1000);
  QObject::connect(m_refresh_timer, &QTimer::timeout, form, [this]() { refresh(); });

  QObject::connect(m_h_file_list, &QListWidget::itemChanged, form, [this]() { update_download_button_availability(); });
  QObject::connect(m_h_install_selected, &QPushButton::clicked, form, [this]() {
    download_selected();
    display(false);
  });
  const auto cancel = [this]() {
    for (int i = 0; i < m_h_file_list->count(); i++)
      m_h_file_list->item(i)->setCheckState(Qt::Unchecked);
    display(false);
  };
  QObject::connect(item_as<QPushButton>(IDCANCEL), &QPushButton::clicked, form, cancel);
  QObject::connect(static_cast<QDialog *>(form), &QDialog::rejected, form, cancel);
  QObject::connect(m_h_address->lineEdit(), &QLineEdit::textEdited, form, [this]() { m_refresh_timer->start(); });
  QObject::connect(m_h_address, &QComboBox::activated, form, [this]() {
    {
      auto mut_settings = m_settings.modify();
      preserve_current_address_index(*mut_settings);
    }
    refresh();
    m_check_if_saving_is_needed = false;
  });
  QObject::connect(m_h_refresh_btn, &QPushButton::clicked, form, [this]() { refresh(); });
  const auto settings_clicked = [this]() {
    auto mut_settings = m_settings.modify();
    update_settings(*mut_settings);
  };
  QObject::connect(m_h_install_system, &QCheckBox::clicked, form, settings_clicked);
  QObject::connect(m_h_show_only_known, &QCheckBox::clicked, form, settings_clicked);
  QObject::connect(select_proxy, &QPushButton::clicked, form, []() {
    get_select_proxy()->do_dialog();
    get_select_proxy()->display();
  });
}

void DownloadDictionariesDialog::init_dialog() {
  reset_download_combobox();
  fill_file_list();
  update_controls();
  update_download_button_availability();
}
