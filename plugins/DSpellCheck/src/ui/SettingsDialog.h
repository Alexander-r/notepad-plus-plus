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

#include "common/winapi.h"
#include "plugin/Settings.h"
#include "ui/StaticDialog.h"

// Qt port: the dialog and its two tabs with the controls of Qt, in place of the Windows ones and the tab bar of
// Notepad++. The methods are those of upstream.

class QCheckBox;
class QComboBox;
class QGroupBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSlider;
class QTabWidget;

enum class AspellStatus;
class HunspellInterface;
class SpellChecker;
class NppInterface;
class LanguageInfo;
class Settings;
class SettingsDialog;
class NppInteface;
class SpellerContainer;

class SimpleSettingsTab : public StaticDialog {
public:
  SimpleSettingsTab(SettingsDialog &parent, const Settings &settings, NppInterface &npp);
  ~SimpleSettingsTab() override;
  void apply_settings(Settings &settings, const SpellerContainer &speller_container);
  void update_language_controls(const Settings &settings, const SpellerContainer &speller_container, const std::wstring &selected_language_name
      );
  void fill_sugestions_num(int suggestions_num);
  void fill_lib_info(AspellStatus aspell_status, const Settings &settings);
  void disable_language_combo(bool disable);
  void set_file_types(bool check_those, const wchar_t *file_types);
  void set_sugg_type(SuggestionMode mode);
  void set_one_user_dic(bool value);
  void init_settings(HINSTANCE h_inst, HWND parent);
  void update_controls(const Settings &settings, const SpellerContainer &speller_container);
  void init_speller_id_combobox(const SpellerContainer &speller_container);

protected:
  void build_form(QWidget *form) override;
  void init_dialog() override;

private:
  void on_suggestions_num_changed();
  void on_browse_clicked();
  void on_reset_path_clicked();
  void place_path_controls(bool in_hunspell_group);

  /* NppData struct instance */
  NppInterface &m_npp;
  const Settings &m_settings;
  SettingsDialog &m_parent;

  /* handles of controls */
  QLabel *m_h_aspell_status = nullptr;
  QLineEdit *m_h_lib_path = nullptr;
  QLineEdit *m_h_suggestions_num = nullptr;
  QRadioButton *m_h_check_not_those = nullptr;
  QRadioButton *m_h_check_only_those = nullptr;
  QLineEdit *m_h_file_types = nullptr;
  QCheckBox *m_h_check_comments = nullptr;
  QCheckBox *m_h_check_strings = nullptr;
  QCheckBox *m_h_check_varfunc = nullptr;
  QLabel *m_h_lib_link = nullptr;
  QGroupBox *m_h_lib_group_box = nullptr;
  QPushButton *m_h_download_dics = nullptr;
  QPushButton *m_h_remove_dics = nullptr;
  QCheckBox *m_h_one_user_dic = nullptr;
  QGroupBox *m_h_hunspell_path_group_box = nullptr;
  QPushButton *m_h_reset_speller_path = nullptr;
  QComboBox *m_h_hunspell_path_type = nullptr;
  QLineEdit *m_h_system_path = nullptr;
  QPushButton *m_h_configure_aspell_btn = nullptr;
  QPushButton *m_h_browse_btn = nullptr;
  QCheckBox *m_h_select_misspelled_on_menu_cb = nullptr;
  QComboBox *m_speller_cmb = nullptr;
  QComboBox *m_language_cmb = nullptr;
  QComboBox *m_suggestion_mode_cmb = nullptr;
  QComboBox *m_language_name_style_cmb = nullptr;
  // The path controls, in the Hunspell group or, for Aspell, where it is
  QWidget *m_path_area = nullptr;
  QGridLayout *m_lib_layout = nullptr;
  QGridLayout *m_hunspell_layout = nullptr;
};

class AdvancedSettingsTab : public StaticDialog {
public:
  explicit AdvancedSettingsTab(const Settings &settings);
  void apply_settings(Settings &settings);
  void set_delimeters_edit(const wchar_t *delimiters);
  void set_conversion_opts(bool convert_yo, bool convert_single_quotes_arg,
                           bool remove_boundary_apostrophes);
  int get_recheck_delay();
  void set_recheck_delay(int delay);
  void set_sugg_box_settings(LRESULT size, LRESULT trans);
  void reset_settings();
  void set_underline_settings(int color, int style);
  void set_ignore(bool ignore_numbers_arg, bool ignore_c_start_arg,
                  bool ignore_c_have_arg, bool ignore_c_all_arg, bool ignore_arg,
                  bool ignore_sa_apostrophe_arg, bool ignore_one_letter);
  void update_controls(const Settings &settings);

protected:
  void setup_delimiter_line_edit_visiblity();
  void on_recheck_delay_changed();
  void on_default_delimiters_clicked();
  void on_underline_color_button_clicked();
  void update_underline_color_button();
  void update_regexp_error();
  void build_form(QWidget *form) override;
  void init_dialog() override;

private:
  const Settings &m_settings;
  QLineEdit *m_h_edit_delimiters = nullptr;
  QPushButton *m_h_default_delimiters = nullptr;
  QCheckBox *m_h_ignore_yo = nullptr;
  QCheckBox *m_h_convert_single_quotes = nullptr;
  QLineEdit *m_h_recheck_delay = nullptr;
  QPushButton *m_h_underline_color = nullptr;
  QComboBox *m_h_underline_style = nullptr;
  QCheckBox *m_h_ignore_numbers = nullptr;
  QCheckBox *m_h_ignore_c_start = nullptr;
  QCheckBox *m_h_ignore_c_have = nullptr;
  QCheckBox *m_h_ignore_c_all = nullptr;
  QCheckBox *m_h_ignore_one_letter = nullptr;
  QCheckBox *m_h_ignore_ = nullptr;
  QCheckBox *m_h_ignore_se_apostrophe = nullptr;
  QSlider *m_h_slider_size = nullptr;
  QSlider *m_h_slider_sugg_button_opacity = nullptr;
  QCheckBox *m_h_remove_boundary_apostrophes = nullptr;
  QLineEdit *m_delimiter_exclusions_le = nullptr;
  QCheckBox *m_split_camel_case_cb = nullptr;
  QCheckBox *m_h_check_default_udl_style = nullptr;
  QLineEdit *m_ignore_regexp_edit = nullptr;
  QComboBox *m_tokenization_style_cmb = nullptr;
  QPushButton *m_reset_btn = nullptr;
  COLORREF m_underline_color_btn = 0;
};

class SettingsDialog : public StaticDialog {
public:
  UINT do_dialog();
  SimpleSettingsTab *get_simple_dlg();
  AdvancedSettingsTab *get_advanced_dlg();
  SettingsDialog(HINSTANCE h_inst, HWND parent, NppInterface &npp, const Settings &settings, const SpellerContainer &speller_container);

  lsignal::signal<void ()> download_dics_dlg_requested;

private:
  void build_form(QWidget *form) override;
  void init_dialog() override;
  void destroy() override;
  void apply_settings();
  void update_controls();
  void apply_lib_change(SpellerId new_lib_id);
  void store_selected_language_name(int language_index);

private:
  NppInterface &m_npp;
  SimpleSettingsTab m_simple_dlg;
  AdvancedSettingsTab m_advanced_dlg;
  QTabWidget *m_controls_tab = nullptr;
  const Settings &m_settings;
  const SpellerContainer &m_speller_container;
  std::wstring m_selected_language_name;

  friend class SimpleSettingsTab;
};
