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

#include "ProgressDialog.h"

#include "DownloadDictionariesDialog.h"
#include "common/ProgressData.h"
#include "plugin/resource.h"

// Qt port: the dialog's controls
#include <QDialogButtonBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

void ProgressDialog::do_dialog() {
  if (!isCreated())
    create(IDD_DIALOGPROGRESS);
  // Qt port: shown when made too, as the dialog of the resources is WS_VISIBLE
  goToCenter();
  display();
}

void ProgressDialog::init(HINSTANCE h_inst, HWND parent) {
  return StaticDialog::init(h_inst, parent);
}

// Qt port: the form of IDD_DIALOGPROGRESS, with a timer that updates it while it shows, as WM_SHOWWINDOW and WM_TIMER did
void ProgressDialog::build_form(QWidget *form) {
  auto *layout = new QVBoxLayout(form);
  m_h_desc_top = with_id(new QLabel(QStringLiteral("What's happening now"), form), IDC_DESCTOP);
  m_h_desc_top->setAlignment(Qt::AlignCenter);
  m_h_desc_top->setWordWrap(true);
  m_h_progress_bar = with_id(new QProgressBar(form), IDC_PROGRESSBAR);
  m_h_progress_bar->setRange(0, 100);
  m_h_progress_bar->setTextVisible(false);
  m_h_desc_bottom = with_id(new QLabel(QStringLiteral("Please wait........"), form), IDC_DESCBOTTOM);
  m_h_desc_bottom->setAlignment(Qt::AlignCenter);
  auto *buttons = make_buttons(form, {{IDC_STOP, L"Cancel"}}, -1);
  buttons->setCenterButtons(true);
  layout->addWidget(m_h_desc_top);
  layout->addWidget(m_h_progress_bar);
  layout->addWidget(m_h_desc_bottom);
  layout->addWidget(buttons);
  form->setMinimumWidth(m_h_desc_top->fontMetrics().averageCharWidth() * 50);
  QObject::connect(item_as<QPushButton>(IDC_STOP), &QPushButton::clicked, form, [this]() { m_download_dics_dlg.set_cancel_pressed(true); });

  constexpr auto timer_resolution = 100;
  auto *timer = new QTimer(form);
  QObject::connect(timer, &QTimer::timeout, form, [this]() {
    if (_hSelf && _hSelf->isVisible())
      update();
  });
  timer->start(timer_resolution);
}

void ProgressDialog::set_marquee(bool animated) {
  if (m_marquee == animated)
    return;
  m_marquee = animated;
  // Qt port: a range of nothing makes the bar move by itself, as PBS_MARQUEE does
  m_h_progress_bar->setRange(0, animated ? 0 : 100);
}

void ProgressDialog::update() {
  // Qt port: the progress bar and labels of Qt
  m_h_progress_bar->setValue(m_progress_data->get_progress());
  m_h_desc_bottom->setText(qstr(m_progress_data->get_status()));
  set_marquee(m_progress_data->get_marquee());
  m_h_desc_top->setText(qstr(m_top_message));
}

void ProgressDialog::set_top_message(const wchar_t *message) {
  m_top_message = message;
}

ProgressDialog::ProgressDialog(DownloadDictionariesDialog &download_dics_dlg)
  : m_progress_data(std::make_shared<ProgressData>()),
    m_download_dics_dlg(download_dics_dlg) {
}

ProgressDialog::~ProgressDialog() = default; // Qt port: the timer goes with the dialog
