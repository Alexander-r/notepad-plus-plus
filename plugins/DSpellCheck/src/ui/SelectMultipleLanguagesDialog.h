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
#include "ui/StaticDialog.h"

class SpellChecker;
class Settings;
class SpellerContainer;

class SelectMultipleLanguagesDialog : public StaticDialog {
public:
  SelectMultipleLanguagesDialog(HINSTANCE h_inst, HWND parent, const Settings &settings,
           const SpellerContainer &speller_container);
  void do_dialog();
  class QListWidget *get_list_box(); // Qt port: a list of check boxes
  void update_list();

protected:
  void apply();
  // Qt port: the form of IDD_CHOOSE_MULTIPLE_LANGUAGES, in place of run_dlg_proc
  void build_form(QWidget *form) override;
  void init_dialog() override;

private:
  class QListWidget *m_h_lang_list = nullptr;
  const Settings &m_settings;
  const SpellerContainer &m_speller_container;
};
