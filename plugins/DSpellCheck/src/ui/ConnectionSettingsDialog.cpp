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

#include "ConnectionSettingsDialog.h"

#include "DownloadDictionariesDialog.h"
#include "common/winapi.h"
#include "plugin/resource.h"
#include "plugin/Settings.h"

// Qt port: the dialog's controls
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

ConnectionSettingsDialog::ConnectionSettingsDialog(const Settings &settings,
                                                   DownloadDictionariesDialog &download_dics_dlg)
  : m_download_dics_dlg(download_dics_dlg), m_settings(settings) {
}

void ConnectionSettingsDialog::do_dialog() {
  if (!isCreated()) {
    create(IDD_CONNECTION_SETTINGS_DIALOG);
  }
  goToCenter();
  display(); // Qt port: shown, as the dialog of the resources is when made
}

// Qt port: the controls of Qt, without the FTP options
void ConnectionSettingsDialog::apply_choice() {
  auto mut_settings = m_settings.modify();
  mut_settings->data.proxy_user_name = m_user_name->text().toStdWString();
  mut_settings->data.proxy_host_name = m_host_name->text().toStdWString();
  mut_settings->data.proxy_password = m_password->text().toStdWString();
  mut_settings->data.use_proxy = m_use_proxy->isChecked();
  mut_settings->data.proxy_is_anonymous = m_proxy_anonymous->isChecked();
  mut_settings->data.proxy_type = ProxyType::web_proxy;
  auto text = m_port->text().toStdWString();
  wchar_t *end_ptr;
  int x = wcstol(text.c_str(), &end_ptr, 10);
  mut_settings->data.proxy_port = (x);
  m_download_dics_dlg.refresh();
}

void ConnectionSettingsDialog::disable_controls() {
  bool proxy_is_used = m_use_proxy->isChecked();
  m_proxy_anonymous->setEnabled(proxy_is_used);
  m_user_name->setEnabled(proxy_is_used);
  m_host_name->setEnabled(proxy_is_used);
  m_password->setEnabled(proxy_is_used);
  m_port->setEnabled(proxy_is_used);
  m_proxy_type_cmb->setEnabled(proxy_is_used);
  if (proxy_is_used) {
    auto login_used = !m_proxy_anonymous->isChecked();
    m_user_name->setEnabled(login_used);
    m_password->setEnabled(login_used);
  }
}

void ConnectionSettingsDialog::update_controls() {
  m_use_proxy->setChecked(m_settings.data.use_proxy);
  m_proxy_anonymous->setChecked(m_settings.data.proxy_is_anonymous);
  m_user_name->setText(qstr(m_settings.data.proxy_user_name));
  m_host_name->setText(qstr(m_settings.data.proxy_host_name));
  m_password->setText(qstr(m_settings.data.proxy_password));
  m_port->setText(QString::number(m_settings.data.proxy_port));
  disable_controls();
  m_proxy_type_cmb->setCurrentIndex(0);
}

// Qt port: the form of IDD_CONNECTION_SETTINGS_DIALOG, with the controls connected as run_dlg_proc handled them
void ConnectionSettingsDialog::build_form(QWidget *form) {
  form->setWindowTitle(QStringLiteral("Connection Settings"));
  auto *layout = new QGridLayout(form);
  m_use_proxy = with_id(new QCheckBox(QStringLiteral("Use Proxy"), form), IDC_USEPROXY);
  m_proxy_type_cmb = with_id(new QComboBox(form), IDC_PROXY_TYPE);
  m_proxy_type_cmb->addItem(qstr(gui_string(ProxyType::web_proxy)));
  layout->addWidget(m_use_proxy, 0, 0);
  layout->addWidget(m_proxy_type_cmb, 0, 1);

  auto *proxy_group = new QGroupBox(QStringLiteral("Proxy Settings"), form);
  auto *proxy_layout = new QGridLayout(proxy_group);
  m_host_name = with_id(new QLineEdit(proxy_group), IDC_HOSTNAME);
  m_proxy_anonymous = with_id(new QCheckBox(QStringLiteral("Anonymous Login"), proxy_group), IDC_ANONYMOUS_LOGIN);
  m_port = with_id(new QLineEdit(proxy_group), IDC_PORT);
  m_port->setValidator(new QIntValidator(0, 65535, m_port));
  m_port->setMaximumWidth(m_port->fontMetrics().averageCharWidth() * 8);
  proxy_layout->addWidget(new QLabel(QStringLiteral("Host Name:"), proxy_group), 0, 0);
  proxy_layout->addWidget(m_host_name, 0, 1, 1, 3);
  proxy_layout->addWidget(m_proxy_anonymous, 1, 0, 1, 2);
  proxy_layout->addWidget(new QLabel(QStringLiteral("Port:"), proxy_group), 1, 2, Qt::AlignRight);
  proxy_layout->addWidget(m_port, 1, 3);
  auto *login_group = new QGroupBox(QStringLiteral("Login Info"), proxy_group);
  auto *login_layout = new QGridLayout(login_group);
  m_user_name = with_id(new QLineEdit(login_group), IDC_USERNAME);
  m_password = with_id(new QLineEdit(login_group), IDC_PASSWORD);
  m_password->setEchoMode(QLineEdit::Password);
  login_layout->addWidget(new QLabel(QStringLiteral("Username:"), login_group), 0, 0);
  login_layout->addWidget(m_user_name, 0, 1);
  login_layout->addWidget(new QLabel(QStringLiteral("Password:"), login_group), 1, 0);
  login_layout->addWidget(m_password, 1, 1);
  proxy_layout->addWidget(login_group, 2, 0, 1, 4);
  layout->addWidget(proxy_group, 1, 0, 1, 2);
  layout->addWidget(make_buttons(form, {{IDOK, L"OK"}, {IDCANCEL, L"Cancel"}}, IDOK), 2, 0, 1, 2);

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
  QObject::connect(m_use_proxy, &QCheckBox::clicked, form, [this]() { disable_controls(); });
  QObject::connect(m_proxy_anonymous, &QCheckBox::clicked, form, [this]() { disable_controls(); });
}

void ConnectionSettingsDialog::init_dialog() { update_controls(); }
