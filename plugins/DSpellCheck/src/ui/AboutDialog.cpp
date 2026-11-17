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

#include "AboutDialog.h"

#include "common/Utility.h"
#include "plugin/Plugin.h"
#include "plugin/resource.h"

// Qt port: the dialog's controls
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

void AboutDialog::do_dialog() {
  if (!isCreated()) {
    create(IDD_ABOUT);
    goToCenter();
  }
  display();
}

// Qt port: the compiler that built the plugin, where MSVC's version was shown
void AboutDialog::update_compiler_version() {
  std::wstring compiler_name = L"Unknown Compiler";
#if defined(__clang__)
  compiler_name = wstring_printf(L"Clang %d.%d.%d", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
  compiler_name = wstring_printf(L"GCC %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif
  auto str = wstring_printf(rc_str(IDS_BUILT_WITH_PS).c_str(), compiler_name.c_str());
  if (auto *label = item_as<QLabel>(IDC_COMPILER_TEXT))
    label->setText(qstr(str));
}

// Qt port: the version of the plugin's resources, FILEVERSION 1,5,0,0, which has no file version info here to read
std::wstring get_product_and_version() {
  return wstring_printf(rc_str(IDS_VERSION_PU_PU_PU_PU).c_str(), 1u, 5u, 0u, 0u);
}

void AboutDialog::init(HINSTANCE h_inst, HWND parent) {
  return StaticDialog::init(h_inst, parent);
}

void AboutDialog::build_form(QWidget *form) {
  form->setWindowTitle(QStringLiteral("About"));
  auto *layout = new QVBoxLayout(form);
  const auto add_label = [&](int id, const QString &text, bool is_link, Qt::Alignment alignment = Qt::AlignLeft) {
    auto *label = with_id(new QLabel(text, form), id);
    label->setAlignment(alignment);
    label->setWordWrap(true);
    if (is_link) {
      label->setTextFormat(Qt::RichText);
      label->setOpenExternalLinks(true); // ShellExecute of the link's URL
    }
    layout->addWidget(label);
    return label;
  };
  add_label(IDC_STATIC, QStringLiteral("DSpellCheck"), false, Qt::AlignHCenter);
  add_label(IDC_VERSION, QStringLiteral("Version: X.X.X.X"), false, Qt::AlignHCenter);
  add_label(IDC_SYSLINK4, QStringLiteral("<a href=\"https://github.com/Predelnik/DSpellCheck/blob/master/Changes.md\">(Changelog)</a>"), true, Qt::AlignHCenter);
  add_label(IDC_STATIC, QStringLiteral("(Hopefully) Decent Spell-Checking Plug-in for Notepad++"), false);
  add_label(IDC_SYSLINK1, QStringLiteral("Created by: Sergey Semushin <a href=\"mailto:predelnik@gmail.com\">[predelnik@gmail.com]</a>"), true);
  add_label(IDC_STATIC, QStringLiteral("License: GPL"), false);
  add_label(IDC_SYSLINK3, QStringLiteral("Source Code: <a href=\"https://github.com/Predelnik/DSpellCheck\">Link</a><br>"
                                         "Report an Issue: <a href=\"https://github.com/Predelnik/DSpellCheck/issues/new\">Link</a>"), true);
  add_label(IDC_SYSLINK2, QStringLiteral("Plug-in uses <a href=\"http://aspell.net\">Aspell Library</a> and <a href=\"http://hunspell.sourceforge.net\">Hunspell Library</a>"), true);
  add_label(IDC_COMPILER_TEXT, QStringLiteral("Built with"), false);
  auto *buttons = make_buttons(form, {{IDOK, L"OK"}}, IDOK);
  buttons->setCenterButtons(true);
  layout->addWidget(buttons);
  QObject::connect(item_as<QPushButton>(IDOK), &QPushButton::clicked, form, [this]() { display(false); });
}

void AboutDialog::init_dialog() {
  if (auto *label = item_as<QLabel>(IDC_VERSION))
    label->setText(qstr(get_product_and_version()));
  update_compiler_version();
}
