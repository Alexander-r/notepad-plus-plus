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

#include "common/Utility.h"
#include "plugin/Settings.h"
#include "ui/StaticDialog.h"

class Settings;
class SpellChecker;
class DownloadDictionariesDialog;
enum class ProxyType;

class QCheckBox;
class QComboBox;
class QLineEdit;

class ConnectionSettingsDialog : public StaticDialog {
public:
  explicit ConnectionSettingsDialog(const Settings &settings, DownloadDictionariesDialog &download_dics_dlg);
  void do_dialog();
  void apply_choice();
  void update_controls(
      );

protected:
  // Qt port: the form of IDD_CONNECTION_SETTINGS_DIALOG, in place of run_dlg_proc
  void build_form(QWidget *form) override;
  void init_dialog() override;
  void disable_controls();

protected:
  QLineEdit *m_port = nullptr;
  QLineEdit *m_user_name = nullptr;
  QLineEdit *m_host_name = nullptr;
  QLineEdit *m_password = nullptr;
  QCheckBox *m_use_proxy = nullptr;
  QCheckBox *m_proxy_anonymous = nullptr;
  DownloadDictionariesDialog &m_download_dics_dlg;
  // Qt port: the web proxy only, as the FTP gateway and passive mode went with the FTP source of dictionaries
  QComboBox *m_proxy_type_cmb = nullptr;
  const Settings &m_settings;
};
