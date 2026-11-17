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

#include "common/TaskWrapper.h"
#include "network/FileListProvider.h"
#include "ui/StaticDialog.h" // Qt port: '/' in the path

#include <array>
#include <optional>

// Qt port: the dialog with the controls of Qt, and without the FTP source of dictionaries and the zip files it gave,
// which went as Qt has no FTP. The dictionaries come from GitHub, as by default on Windows.

class QComboBox;
class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;
class QTimer;

enum class UrlType {
  github,
  unknown,
};

class LanguageInfo;
class GitHubFileListProvider;
class SpellerContainer;
class ProgressData;

void ftp_trim(std::wstring &ftp_address);

class SpellChecker;

class DownloadDictionariesDialog : public StaticDialog {
public:
  ~DownloadDictionariesDialog() override;
  DownloadDictionariesDialog(HINSTANCE h_inst, HWND parent, const Settings &settings, const SpellerContainer &speller_container);
  void do_dialog();
  void update_list_box();
  void on_new_file_list(std::vector<FileDescription> list);
  void preserve_current_address_index(Settings &settings);
  void reset_download_combobox();
  void add_user_server(std::wstring server);
  void prepare_file_list_update();
  void refresh();
  void start_next_download();
  UrlType selected_url_type();
  bool download_github_file(const std::wstring &title, const std::wstring &path, std::shared_ptr<ProgressData> progress_data);
  bool download_file();
  void download_selected();
  void fill_file_list();
  void on_display_action();
  void indicate_that_saving_might_be_needed();
  void update_controls();
  void update_settings(Settings &settings);
  void update_download_button_availability();
  void set_cancel_pressed(bool value);
  LRESULT ask_replacement_message(const wchar_t *dic_name);
  bool prepare_downloading();
  void finalize_downloading();
  std::optional<std::wstring> current_address() const;
  void update_status(const wchar_t *text, COLORREF status_color);

protected:
  void build_form(QWidget *form) override;
  void init_dialog() override;

private:
  std::vector<FileDescription> m_current_list;
  QListWidget *m_h_file_list = nullptr;
  QComboBox *m_h_address = nullptr;
  QLabel *m_h_status = nullptr;
  QPushButton *m_h_install_selected = nullptr;
  QCheckBox *m_h_show_only_known = nullptr;
  QPushButton *m_h_refresh_btn = nullptr;
  QCheckBox *m_h_install_system = nullptr;
  QTimer *m_refresh_timer = nullptr;
  bool m_cancel_pressed = false;
  bool m_check_if_saving_is_needed = false;

  bool m_failure = false;
  int m_downloaded_count = 0;
  std::string m_errors_text;
  int m_supposed_downloaded_count = 0;
  std::wstring m_message;
  std::vector<FileDescription> m_to_download;
  decltype(m_to_download)::iterator m_cur;
  std::array<std::wstring, 1> m_default_server_names;
  const Settings &m_settings;
  bool m_address_is_set = false;
  std::unique_ptr<GitHubFileListProvider> m_github_provider;
  const SpellerContainer &m_speller_container;
};
