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

// Qt port: the dialog with the controls of Qt. Each method keeps what upstream's did with the Windows controls, and
// the controls are connected to what run_dlg_proc did for them.

#include "SettingsDialog.h"

#include "aspell.h"
#include "AspellOptionsDialog.h"
#include "DownloadDictionariesDialog.h"
#include "RemoveDictionariesDialog.h"
#include "SelectMultipleLanguagesDialog.h"
#include "common/enum_range.h"
#include "common/IniWorker.h"
#include "common/winapi.h"
#include "common/WindowsDefs.h"
#include "npp/NppInterface.h"
#include "plugin/Plugin.h"
#include "plugin/resource.h"
#include "plugin/Settings.h"
#include "spellers/LanguageInfo.h"
#include "spellers/NativeSpellerInterface.h"
#include "spellers/SpellerContainer.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QTabWidget>
#include <QVBoxLayout>

#include <cassert>

namespace {
// The combo boxes of an enum, in place of WinApi::EnumComboBox
template <typename EnumType> void add_enum_item(QComboBox *combo, EnumType value) { combo->addItem(qstr(gui_string(value)), static_cast<int>(value)); }

template <typename EnumType> void add_enum_items(QComboBox *combo) {
  for (auto value : enum_range<EnumType>())
    add_enum_item(combo, value);
}

template <typename EnumType> EnumType current_enum(const QComboBox *combo) { return static_cast<EnumType>(combo->currentData().toInt()); }

template <typename EnumType> void set_current_enum(QComboBox *combo, EnumType value) {
  combo->setCurrentIndex(combo->findData(static_cast<int>(value)));
}

QColor to_qcolor(COLORREF colour) { return QColor(GetRValue(colour), GetGValue(colour), GetBValue(colour)); }

template <typename ControlType> ControlType *control_with_id(ControlType *control, int id) {
  control->setProperty("nppId", id);
  return control;
}

QPushButton *push_button(const char *text, QWidget *parent, int id) {
  auto *button = control_with_id(new QPushButton(QString::fromUtf8(text), parent), id);
  button->setAutoDefault(false);
  return button;
}
} // namespace

SimpleSettingsTab::SimpleSettingsTab(SettingsDialog &parent, const Settings &settings, NppInterface &npp)
  : m_npp(npp), m_settings(settings), m_parent(parent) {
}

void SimpleSettingsTab::disable_language_combo(bool disable) {
  m_language_cmb->clear();
  m_language_cmb->setEnabled(!disable);
  if (auto *list = get_remove_dics()->get_list_box())
    list->clear();
  m_h_remove_dics->setEnabled(!disable);
}

void SimpleSettingsTab::update_language_controls(const Settings &settings, const SpellerContainer &speller_container,
                                                 const std::wstring &selected_language_name) {

  if (m_language_cmb == nullptr)
    return;

  const QSignalBlocker blocker(m_language_cmb); // no CBN_SELCHANGE for changes made here, as on Windows
  m_language_cmb->clear();
  int selected_index = 0;

  auto langs_available = speller_container.get_available_languages();

  int i = 0;
  const auto &target_language = selected_language_name.empty() ? settings.get_active_language() : selected_language_name;

  for (auto &lang : langs_available) {
    if (target_language == lang.orig_name) {
      selected_index = i;
    }

    m_language_cmb->addItem(qstr(lang.alias_name), i);
    ++i;
  }
  if (target_language == multiple_language_alias)
    selected_index = i;

  if (!langs_available.empty())
    m_language_cmb->addItem(qstr(rc_str(IDS_MULTIPLE_LANGUAGES)), -1);

  m_language_cmb->setCurrentIndex(selected_index);
  m_language_cmb->setEnabled(!langs_available.empty());
}

SimpleSettingsTab::~SimpleSettingsTab() = default;

void SimpleSettingsTab::apply_settings(Settings &settings, const SpellerContainer &speller_container) {
  int lang_count = m_language_cmb->count();
  int cur_sel = m_language_cmb->currentIndex();

  if (m_language_cmb->isEnabled() && cur_sel >= 0) {
    std::wstring new_lang = (cur_sel == lang_count - 1 ? multiple_language_alias : speller_container.get_available_languages()[cur_sel].orig_name.c_str());
    settings.get_active_language() = new_lang;
    m_parent.m_selected_language_name = new_lang;
  }
  settings.data.suggestion_count = m_h_suggestions_num->text().toInt();
  switch (settings.data.active_speller_lib_id) {
  case SpellerId::aspell:
    settings.data.aspell_dll_path = m_h_lib_path->text().toStdWString();
    break;
  case SpellerId::hunspell:
    settings.data.hunspell_user_path = m_h_lib_path->text().toStdWString();
    settings.data.hunspell_system_path = m_h_system_path->text().toStdWString();
    break;
  case SpellerId::native:
    break;
  case SpellerId::COUNT:
    break;
  default: ;
  }

  settings.data.check_those = m_h_check_only_those->isChecked();
  settings.data.file_types = m_h_file_types->text().toStdWString();
  settings.data.suggestions_mode = current_enum<SuggestionMode>(m_suggestion_mode_cmb);
  settings.data.check_comments = m_h_check_comments->isChecked();
  settings.data.check_strings = m_h_check_strings->isChecked();
  settings.data.check_variable_functions = m_h_check_varfunc->isChecked();
  settings.data.use_unified_dictionary = m_h_one_user_dic->isChecked();
  settings.data.select_word_on_context_menu_click = m_h_select_misspelled_on_menu_cb->isChecked();
  settings.data.language_name_style = current_enum<LanguageNameStyle>(m_language_name_style_cmb);
}

// The path controls go in the Hunspell group, which Aspell has no frame for
void SimpleSettingsTab::place_path_controls(bool in_hunspell_group) {
  if (in_hunspell_group)
    m_hunspell_layout->addWidget(m_path_area, 0, 0);
  else
    m_lib_layout->addWidget(m_path_area, 2, 0, 1, 2);
  m_h_hunspell_path_group_box->setVisible(in_hunspell_group);
}

void SimpleSettingsTab::fill_lib_info(AspellStatus aspell_status, const Settings &settings) {

  auto is_aspell = settings.data.active_speller_lib_id == SpellerId::aspell;
  m_h_lib_link->setVisible(is_aspell);
  m_h_aspell_status->setVisible(is_aspell);
  auto is_hunspell = settings.data.active_speller_lib_id == SpellerId::hunspell;
  m_h_download_dics->setVisible(is_hunspell);
  m_h_remove_dics->setVisible(is_hunspell);
  m_h_one_user_dic->setVisible(is_hunspell);
  place_path_controls(is_hunspell);
  m_h_hunspell_path_type->setVisible(is_hunspell);
  m_h_system_path->setVisible(false);
  m_h_configure_aspell_btn->setVisible(is_aspell);
  auto not_native = settings.data.active_speller_lib_id != SpellerId::native;
  m_h_lib_group_box->setVisible(not_native);
  m_h_lib_path->setVisible(not_native);
  m_h_reset_speller_path->setVisible(not_native);
  m_h_browse_btn->setVisible(not_native);
  bool configure_enabled = false;
  COLORREF status_color = COLOR_NEUTRAL;

  switch (settings.data.active_speller_lib_id) {
  case SpellerId::aspell:

    switch (aspell_status) {
    case AspellStatus::working:
      status_color = COLOR_OK;
      configure_enabled = true;
      m_h_aspell_status->setText(qstr(rc_str(IDS_ASPELL_STATUS_OK)));
      break;
    case AspellStatus::no_dictionaries:
      status_color = COLOR_FAIL;
      configure_enabled = true;
      m_h_aspell_status->setText(qstr(rc_str(IDS_ASPELL_STATUS_NO_DICTS)));
      break;
    case AspellStatus::not_working:
      status_color = COLOR_FAIL;
      m_h_aspell_status->setText(qstr(rc_str(IDS_ASPELL_STATUS_FAIL)));
      break;
    case AspellStatus::incorrect_bitness:
      status_color = COLOR_FAIL;
      m_h_aspell_status->setText(qstr(rc_str(IDS_ASPELL_INCORRECT_BITNESS)));
      break;
    }
    m_h_configure_aspell_btn->setEnabled(configure_enabled);
    m_h_lib_path->setText(qstr(get_actual_aspell_path(settings.data.aspell_dll_path)));

    m_h_lib_group_box->setTitle(qstr(rc_str(IDS_ASPELL_LOCATION)));
    break;
  case SpellerId::hunspell: {
    auto is_lib_path = m_h_hunspell_path_type->currentIndex() == 0;
    m_h_lib_path->setVisible(is_lib_path);
    m_h_system_path->setVisible(!is_lib_path);
    m_h_lib_group_box->setTitle(qstr(rc_str(IDS_HUNSPELL_SETTINGS)));
    m_h_lib_path->setText(qstr(settings.data.hunspell_user_path));
  }
  break;
  case SpellerId::native:
    break;
  case SpellerId::COUNT:
    break;
  default: ;
  }
  // The status in its colour, which WM_CTLCOLORSTATIC gave it
  QPalette palette = m_h_aspell_status->palette();
  palette.setColor(QPalette::WindowText, status_color == COLOR_NEUTRAL ? QPalette().color(QPalette::WindowText) : to_qcolor(status_color));
  m_h_aspell_status->setPalette(palette);
  m_h_system_path->setText(qstr(settings.data.hunspell_system_path));
}

void SimpleSettingsTab::fill_sugestions_num(int suggestions_num) {
  m_h_suggestions_num->setText(QString::number(suggestions_num));
}

void SimpleSettingsTab::set_file_types(bool check_those, const wchar_t *file_types) {
  m_h_check_only_those->setChecked(check_those);
  m_h_check_not_those->setChecked(!check_those);
  m_h_file_types->setText(qstr(file_types));
}

void SimpleSettingsTab::set_sugg_type(SuggestionMode mode) { set_current_enum(m_suggestion_mode_cmb, mode); }

void SimpleSettingsTab::set_one_user_dic(bool value) {
  m_h_one_user_dic->setChecked(value);
}

void AdvancedSettingsTab::reset_settings() {
  if (MessageBox(_hParent, rc_str(IDS_RESET_SETTINGS_TEXT).c_str(), rc_str(IDS_RESET_SETTINGS_CAPTION).c_str(), MB_YESNO) == IDNO)
    return;
  auto mut = m_settings.modify();
  IniWorker worker{L"", L"", IniWorker::Action::load};
  mut->process(worker);
}

void SimpleSettingsTab::on_suggestions_num_changed() {
  // As for EN_CHANGE of IDC_SUGGESTIONS_NUM
  auto text = m_h_suggestions_num->text().toStdWString();
  if (text.empty())
    return;

  wchar_t *end_ptr = nullptr;
  int x = wcstol(text.c_str(), &end_ptr, 10);
  if (*end_ptr != L'\0')
    m_h_suggestions_num->setText(QStringLiteral("5"));
  else if (x > 20)
    m_h_suggestions_num->setText(QStringLiteral("20"));
  else if (x < 1)
    m_h_suggestions_num->setText(QStringLiteral("1"));
}

void SimpleSettingsTab::on_reset_path_clicked() {
  std::wstring path;
  switch (m_settings.data.active_speller_lib_id) {
  case SpellerId::aspell:
    path = get_default_aspell_path();
    break;
  case SpellerId::hunspell:
    path = get_default_hunspell_path();
    break;
  case SpellerId::COUNT:
    break;
  case SpellerId::native:
    break;
  }

  if (m_settings.data.active_speller_lib_id == SpellerId::aspell || m_h_hunspell_path_type->currentIndex() == 0)
    m_h_lib_path->setText(qstr(path));
  else
    m_h_system_path->setText(QStringLiteral("/usr/share/hunspell")); // where the system's dictionaries are
}

void SimpleSettingsTab::on_browse_clicked() {
  switch (m_settings.data.active_speller_lib_id) {
  case SpellerId::aspell: {
    // A library of Aspell, in place of GetOpenFileName with *.dll
    const QString file = QFileDialog::getOpenFileName(_hSelf, QString(), m_h_lib_path->text(),
                                                      qstr(rc_str(IDS_ASPELL_LIBRARY)) + QStringLiteral(" (*.so *.so.*)"));
    if (!file.isEmpty())
      m_h_lib_path->setText(file);
  }
  break;
  case SpellerId::hunspell: {
    auto lib_path = m_h_lib_path->text();
    const auto final_path = QDir(qstr(m_npp.get_npp_directory())).absoluteFilePath(lib_path).toStdWString();

    auto path = WinApi::browse_for_directory(_hSelf, final_path.c_str());
    if (path)
      m_h_lib_path->setText(qstr(*path));
  }
  break;
  case SpellerId::COUNT:
    break;
  case SpellerId::native:
    break;
  default: ;
  }
}

// The form of IDD_SIMPLE
void SimpleSettingsTab::build_form(QWidget *form) {
  auto *layout = new QGridLayout(form);
  int row = 0;
  m_speller_cmb = with_id(new QComboBox(form), IDC_LIBRARY);
  layout->addWidget(new QLabel(QStringLiteral("Library:"), form), row, 0);
  layout->addWidget(m_speller_cmb, row++, 1);

  m_h_lib_group_box = with_id(new QGroupBox(QStringLiteral("Path to Aspell Library"), form), IDC_LIB_GROUPBOX);
  m_lib_layout = new QGridLayout(m_h_lib_group_box);
  // Qt port: the page of Aspell, as there is no Windows build of it to link to
  m_h_lib_link = with_id(new QLabel(QStringLiteral("<a href=\"http://aspell.net/\">Aspell Library and Dictionaries</a>"), m_h_lib_group_box), IDC_LIB_LINK);
  m_h_lib_link->setOpenExternalLinks(true);
  m_lib_layout->addWidget(m_h_lib_link, 0, 0, 1, 2);
  m_h_hunspell_path_group_box = with_id(new QGroupBox(QStringLiteral("Hunspell Dictionaries Path"), m_h_lib_group_box), IDC_HUNSPELL_PATH_GROUPBOX);
  m_hunspell_layout = new QGridLayout(m_h_hunspell_path_group_box);
  m_lib_layout->addWidget(m_h_hunspell_path_group_box, 1, 0, 1, 2);

  m_path_area = new QWidget(m_h_lib_group_box);
  auto *path_layout = new QVBoxLayout(m_path_area);
  path_layout->setContentsMargins(0, 0, 0, 0);
  m_h_lib_path = with_id(new QLineEdit(m_path_area), IDC_ASPELLPATH);
  m_h_system_path = with_id(new QLineEdit(m_path_area), IDC_SYSTEMPATH);
  m_h_browse_btn = push_button("...", m_path_area, IDC_BROWSEASPELLPATH);
  m_h_configure_aspell_btn = push_button("Configure Aspell...", m_path_area, IDC_CONFIGURE_ASPELL);
  m_h_hunspell_path_type = with_id(new QComboBox(m_path_area), IDC_HUNSPELL_PATH_TYPE);
  m_h_reset_speller_path = push_button("Reset to Default", m_path_area, IDC_RESETSPELLERPATH);
  auto *path_row = new QHBoxLayout;
  path_row->addWidget(m_h_lib_path, 1);
  path_row->addWidget(m_h_system_path, 1);
  path_row->addWidget(m_h_browse_btn);
  path_layout->addLayout(path_row);
  auto *choice_row = new QHBoxLayout;
  choice_row->addWidget(m_h_configure_aspell_btn);
  choice_row->addWidget(m_h_hunspell_path_type);
  choice_row->addStretch(1);
  choice_row->addWidget(m_h_reset_speller_path);
  path_layout->addLayout(choice_row);
  m_hunspell_layout->addWidget(m_path_area, 0, 0);

  m_h_aspell_status = with_id(new QLabel(QStringLiteral("Static"), m_h_lib_group_box), IDC_ASPELL_STATUS);
  m_h_one_user_dic = with_id(new QCheckBox(QStringLiteral("Unified User Dictionary"), m_h_lib_group_box), IDC_ONE_USER_DIC);
  m_h_download_dics = push_button("Download...", m_h_lib_group_box, IDC_DOWNLOADDICS);
  m_h_remove_dics = push_button("Remove Dictionaries...", m_h_lib_group_box, IDC_REMOVE_DICS);
  m_lib_layout->addWidget(m_h_aspell_status, 3, 0, 1, 2);
  m_lib_layout->addWidget(m_h_one_user_dic, 4, 0);
  m_lib_layout->addWidget(m_h_download_dics, 4, 1);
  m_lib_layout->addWidget(m_h_remove_dics, 5, 1);
  layout->addWidget(m_h_lib_group_box, row++, 0, 1, 2);

  m_language_name_style_cmb = with_id(new QComboBox(form), IDC_LANGUAGE_NAME_STYLE);
  layout->addWidget(new QLabel(QStringLiteral("Language Naming Style:"), form), row, 0);
  layout->addWidget(m_language_name_style_cmb, row++, 1);
  m_language_cmb = with_id(new QComboBox(form), IDC_COMBO_LANGUAGE);
  layout->addWidget(new QLabel(QStringLiteral("Language:"), form), row, 0);
  layout->addWidget(m_language_cmb, row++, 1);
  m_h_suggestions_num = with_id(new QLineEdit(form), IDC_SUGGESTIONS_NUM);
  m_h_suggestions_num->setValidator(new QIntValidator(0, 99, m_h_suggestions_num)); // ES_NUMBER
  m_h_suggestions_num->setMaximumWidth(m_h_suggestions_num->fontMetrics().averageCharWidth() * 6);
  layout->addWidget(new QLabel(QStringLiteral("Max Number of Suggestions:"), form), row, 0);
  layout->addWidget(m_h_suggestions_num, row++, 1, Qt::AlignRight);

  auto *file_types_group = new QGroupBox(QStringLiteral("File Types (Separate with semicolon ( ; ) )"), form);
  auto *file_types_layout = new QGridLayout(file_types_group);
  m_h_check_only_those = with_id(new QRadioButton(QStringLiteral("Check only those:"), file_types_group), IDC_FILETYPES_CHECKTHOSE);
  m_h_check_not_those = with_id(new QRadioButton(QStringLiteral("Check only NOT those:"), file_types_group), IDC_FILETYPES_CHECKNOTTHOSE);
  m_h_file_types = with_id(new QLineEdit(file_types_group), IDC_FILETYPES);
  file_types_layout->addWidget(m_h_check_only_those, 0, 0);
  file_types_layout->addWidget(m_h_check_not_those, 0, 1);
  file_types_layout->addWidget(m_h_file_types, 1, 0, 1, 2);
  layout->addWidget(file_types_group, row++, 0, 1, 2);

  auto *code_group = with_id(new QGroupBox(QStringLiteral("In code, check only:"), form), IDC_LIB_CODE_GB);
  auto *code_layout = new QHBoxLayout(code_group);
  m_h_check_comments = with_id(new QCheckBox(QStringLiteral("Comments"), code_group), IDC_CHECKCOMMENTS_CB);
  m_h_check_strings = with_id(new QCheckBox(QStringLiteral("Strings"), code_group), IDC_CHECKSTRINGS_CB);
  m_h_check_varfunc = with_id(new QCheckBox(QStringLiteral("Function/Variable names"), code_group), IDC_CHECKFUNCVAR_CB);
  code_layout->addWidget(m_h_check_comments);
  code_layout->addWidget(m_h_check_strings);
  code_layout->addWidget(m_h_check_varfunc);
  layout->addWidget(code_group, row++, 0, 1, 2);

  m_suggestion_mode_cmb = with_id(new QComboBox(form), IDC_SUGG_TYPE);
  layout->addWidget(new QLabel(QStringLiteral("Suggestions Control:"), form), row, 0);
  layout->addWidget(m_suggestion_mode_cmb, row++, 1);
  m_h_select_misspelled_on_menu_cb = with_id(new QCheckBox(QStringLiteral("Select the Misspelled Words when Menu is Triggered"), form), IDC_SELECT_MISSPELLED_ON_MENU);
  layout->addWidget(m_h_select_misspelled_on_menu_cb, row++, 0, 1, 2);
  layout->setRowStretch(row, 1);

  // The controls, as run_dlg_proc handled their WM_COMMAND
  QObject::connect(m_language_cmb, &QComboBox::currentIndexChanged, form, [this]() {
    m_parent.store_selected_language_name(m_language_cmb->currentData().toInt());
    if (m_language_cmb->currentData().toInt() == -1) {
      get_lang_list()->do_dialog();
    }
  });
  QObject::connect(m_language_name_style_cmb, &QComboBox::currentIndexChanged, form, [this]() {
    auto mut = m_settings.modify();
    mut->data.language_name_style = current_enum<LanguageNameStyle>(m_language_name_style_cmb);
  });
  QObject::connect(m_speller_cmb, &QComboBox::currentIndexChanged, form, [this]() { m_parent.apply_lib_change(current_enum<SpellerId>(m_speller_cmb)); });
  QObject::connect(m_h_hunspell_path_type, &QComboBox::currentIndexChanged, form, [this]() {
    const bool is_user_path = m_h_hunspell_path_type->currentIndex() == 0 || m_settings.data.active_speller_lib_id == SpellerId::aspell;
    m_h_lib_path->setVisible(is_user_path);
    m_h_system_path->setVisible(!is_user_path);
  });
  QObject::connect(m_h_suggestions_num, &QLineEdit::textChanged, form, [this]() { on_suggestions_num_changed(); });
  QObject::connect(m_h_configure_aspell_btn, &QPushButton::clicked, form, []() { get_aspell_options_dlg()->do_dialog(); });
  QObject::connect(m_h_remove_dics, &QPushButton::clicked, form, []() { get_remove_dics()->do_dialog(); });
  QObject::connect(m_h_reset_speller_path, &QPushButton::clicked, form, [this]() { on_reset_path_clicked(); });
  QObject::connect(m_h_download_dics, &QPushButton::clicked, form, [this]() { m_parent.download_dics_dlg_requested(); });
  QObject::connect(m_h_browse_btn, &QPushButton::clicked, form, [this]() { on_browse_clicked(); });
}

void SimpleSettingsTab::init_dialog() {
  // As WM_INITDIALOG: the lists of the combo boxes, which give no notifications when filled, as on Windows
  const QSignalBlocker block_style(m_language_name_style_cmb);
  const QSignalBlocker block_path_type(m_h_hunspell_path_type);
  for (auto val : enum_range<LanguageNameStyle>())
    add_enum_item(m_language_name_style_cmb, val); // QLocale knows the names of the languages
  add_enum_items<SuggestionMode>(m_suggestion_mode_cmb);
  m_h_hunspell_path_type->addItem(qstr(rc_str(IDS_FOR_CURRENT_USER)));
  m_h_hunspell_path_type->addItem(qstr(rc_str(IDS_FOR_ALL_USERS)));
  m_h_hunspell_path_type->setCurrentIndex(0);
  m_h_lib_path->setVisible(true);
  m_h_system_path->setVisible(false);
}

void AdvancedSettingsTab::set_underline_settings(int color, int style) {
  m_underline_color_btn = color;
  update_underline_color_button();
  m_h_underline_style->setCurrentIndex(style);
}

void AdvancedSettingsTab::set_conversion_opts(bool convert_yo, bool convert_single_quotes_arg, bool remove_boundary_apostrophes) {
  m_h_ignore_yo->setChecked(convert_yo);
  m_h_convert_single_quotes->setChecked(convert_single_quotes_arg);
  m_h_remove_boundary_apostrophes->setChecked(remove_boundary_apostrophes);
}

void AdvancedSettingsTab::set_ignore(bool ignore_numbers_arg, bool ignore_c_start_arg, bool ignore_c_have_arg, bool ignore_c_all_arg, bool ignore_arg,
                                     bool ignore_sa_apostrophe_arg, bool ignore_one_letter) {
  m_h_ignore_numbers->setChecked(ignore_numbers_arg);
  m_h_ignore_c_start->setChecked(ignore_c_start_arg);
  m_h_ignore_c_have->setChecked(ignore_c_have_arg);
  m_h_ignore_c_all->setChecked(ignore_c_all_arg);
  m_h_ignore_one_letter->setChecked(ignore_one_letter);
  m_h_ignore_->setChecked(ignore_arg);
  m_h_ignore_se_apostrophe->setChecked(ignore_sa_apostrophe_arg);
}

void AdvancedSettingsTab::set_sugg_box_settings(LRESULT size, LRESULT trans) {
  m_h_slider_size->setValue(static_cast<int>(size));
  m_h_slider_sugg_button_opacity->setValue(static_cast<int>(trans));
}

void AdvancedSettingsTab::update_controls(const Settings &settings) {
  m_h_edit_delimiters->setText(qstr(settings.data.delimiters));
  m_delimiter_exclusions_le->setText(qstr(m_settings.data.delimiter_exclusions));
  m_ignore_regexp_edit->setText(qstr(m_settings.data.ignore_regexp_str));
  m_split_camel_case_cb->setChecked(m_settings.data.split_camel_case);
  set_recheck_delay(settings.data.recheck_delay);
  set_conversion_opts(settings.data.ignore_yo, settings.data.convert_single_quotes, settings.data.remove_boundary_apostrophes);
  set_underline_settings(settings.data.underline_color, settings.data.underline_style);
  m_h_check_default_udl_style->setChecked(settings.data.check_default_udl_style);
  set_ignore(settings.data.ignore_containing_digit, settings.data.ignore_starting_with_capital, settings.data.ignore_having_a_capital,
             settings.data.ignore_all_capital,
             settings.data.ignore_having_underscore, settings.data.ignore_starting_or_ending_with_apostrophe, settings.data.ignore_one_letter);
  set_sugg_box_settings(settings.data.suggestion_button_size, settings.data.suggestion_button_opacity);
  set_current_enum(m_tokenization_style_cmb, settings.data.tokenization_style);
  setup_delimiter_line_edit_visiblity();
  update_regexp_error();
}

const std::vector<std::wstring> &get_indic_names() {
  static std::vector<std::wstring> indic_names = {rc_str(IDS_PLAIN), rc_str(IDS_SQUIGGLE), rc_str(IDS_TT), rc_str(IDS_DIAGONAL),
                                                  rc_str(IDS_STRIKE), rc_str(IDS_HIDDEN), rc_str(IDS_BOX), rc_str(IDS_ROUND_BOX),
                                                  rc_str(IDS_STRAIGHT_BOX), rc_str(IDS_DASH), rc_str(IDS_DOTS), rc_str(IDS_SQUIGGLE_LOW)};
  return indic_names;
}

void AdvancedSettingsTab::setup_delimiter_line_edit_visiblity() {
  m_delimiter_exclusions_le->setVisible(current_enum<TokenizationStyle>(m_tokenization_style_cmb) != TokenizationStyle::by_delimiters);
  m_h_edit_delimiters->setVisible(current_enum<TokenizationStyle>(m_tokenization_style_cmb) == TokenizationStyle::by_delimiters);
}

void AdvancedSettingsTab::on_recheck_delay_changed() {
  auto text = m_h_recheck_delay->text().toStdWString();
  if (text.empty())
    return;

  wchar_t *end_ptr = nullptr;
  auto x = wcstol(text.c_str(), &end_ptr, 10);
  if (*end_ptr != 0u)
    m_h_recheck_delay->setText(QStringLiteral("0"));
  else if (x > 30000)
    m_h_recheck_delay->setText(QStringLiteral("30000"));
  else if (x < 0)
    m_h_recheck_delay->setText(QStringLiteral("0"));
}

void AdvancedSettingsTab::on_default_delimiters_clicked() {
  switch (current_enum<TokenizationStyle>(m_tokenization_style_cmb)) {
  case TokenizationStyle::by_non_alphabetic:
  case TokenizationStyle::by_non_ansi:
    m_delimiter_exclusions_le->setText(qstr(default_delimiter_exclusions()));
    break;
  case TokenizationStyle::by_delimiters:
    m_h_edit_delimiters->setText(qstr(default_delimiters()));
    break;
  case TokenizationStyle::COUNT:
    break;
  }
}

// QColorDialog, in place of ChooseColor
void AdvancedSettingsTab::on_underline_color_button_clicked() {
  const QColor color = QColorDialog::getColor(to_qcolor(m_underline_color_btn), _hSelf);
  if (color.isValid()) {
    m_underline_color_btn = RGB(color.red(), color.green(), color.blue());
    update_underline_color_button();
  }
}

// The button shows the colour, which it was drawn in on Windows
void AdvancedSettingsTab::update_underline_color_button() {
  const int size = m_h_underline_color->fontMetrics().height();
  QPixmap swatch(size * 2, size);
  swatch.fill(to_qcolor(m_underline_color_btn));
  m_h_underline_color->setIcon(QIcon(swatch));
  m_h_underline_color->setIconSize(swatch.size());
}

// A wrong expression shows in the colour of an error, with the error as its tooltip, as WM_CTLCOLOREDIT and
// TTN_GETDISPINFOW did
void AdvancedSettingsTab::update_regexp_error() {
  constexpr COLORREF regex_error_color = RGB(255, 200, 200);
  auto mb_err = m_settings.get_regexp_error();
  QPalette palette = QPalette();
  if (mb_err != nullptr) {
    palette.setColor(QPalette::Base, to_qcolor(regex_error_color));
    palette.setColor(QPalette::Text, Qt::black);
  }
  m_ignore_regexp_edit->setPalette(palette);
  m_ignore_regexp_edit->setToolTip(mb_err != nullptr ? qstr(to_wstring(mb_err->what())) : QString());
}

// The form of IDD_ADVANCED
void AdvancedSettingsTab::build_form(QWidget *form) {
  auto *layout = new QGridLayout(form);
  int row = 0;
  m_tokenization_style_cmb = with_id(new QComboBox(form), IDC_TOKENIZATION_STYLE_CMB);
  layout->addWidget(m_tokenization_style_cmb, row++, 0, 1, 2);
  m_delimiter_exclusions_le = with_id(new QLineEdit(form), IDC_DELIMITER_EXCLUSIONS_LE);
  m_h_edit_delimiters = with_id(new QLineEdit(form), IDC_DELIMITERS);
  m_h_default_delimiters = push_button("Reset", form, IDC_DEFAULT_DELIMITERS);
  auto *delimiters = new QHBoxLayout;
  delimiters->addWidget(m_delimiter_exclusions_le, 1);
  delimiters->addWidget(m_h_edit_delimiters, 1);
  delimiters->addWidget(m_h_default_delimiters);
  layout->addLayout(delimiters, row++, 0, 1, 2);
  m_split_camel_case_cb = with_id(new QCheckBox(QStringLiteral("Split CamelCase Words"), form), IDC_CAMEL_CASE_SPLITTING_CB);
  layout->addWidget(m_split_camel_case_cb, row++, 0, 1, 2);
  m_h_remove_boundary_apostrophes = with_id(new QCheckBox(QStringLiteral("Treat starting/ending apostrophe as delimiter"), form), IDC_REMOVE_ENDING_APOSTROPHE);
  layout->addWidget(m_h_remove_boundary_apostrophes, row++, 0, 1, 2);
  m_h_recheck_delay = with_id(new QLineEdit(form), IDC_RECHECK_DELAY);
  m_h_recheck_delay->setValidator(new QIntValidator(0, 999999, m_h_recheck_delay)); // ES_NUMBER
  m_h_recheck_delay->setMaximumWidth(m_h_recheck_delay->fontMetrics().averageCharWidth() * 9);
  layout->addWidget(new QLabel(QStringLiteral("Typing Recheck Delay (ms):"), form), row, 0);
  layout->addWidget(m_h_recheck_delay, row++, 1, Qt::AlignRight);
  m_h_check_default_udl_style = with_id(new QCheckBox(QStringLiteral("Check default UDL style"), form), IDC_CHECK_DEFAULT_UDL_STYLE);
  layout->addWidget(m_h_check_default_udl_style, row++, 0, 1, 2);

  auto *ignore_group = new QGroupBox(QStringLiteral("Ignore Words"), form);
  auto *ignore_layout = new QGridLayout(ignore_group);
  m_h_ignore_numbers = with_id(new QCheckBox(QStringLiteral("Containing Numbers"), ignore_group), IDC_IGNORE_NUMBERS);
  m_h_ignore_c_start = with_id(new QCheckBox(QStringLiteral("Starting with Capital Letter"), ignore_group), IDC_IGNORE_CSTART);
  m_h_ignore_c_have = with_id(new QCheckBox(QStringLiteral("Having not First Capital Letter "), ignore_group), IDC_IGNORE_CHAVE);
  m_h_ignore_c_all = with_id(new QCheckBox(QStringLiteral("All Letters Capital"), ignore_group), IDC_IGNORE_CALL);
  m_h_ignore_ = with_id(new QCheckBox(QStringLiteral("Having _"), ignore_group), IDC_IGNORE_);
  m_h_ignore_se_apostrophe = with_id(new QCheckBox(QStringLiteral("That Start or End with '"), ignore_group), IDC_IGNORE_SE_APOSTROPHE);
  m_h_ignore_one_letter = with_id(new QCheckBox(QStringLiteral("One Letter"), ignore_group), IDC_IGNORE_ONE_LETTER);
  m_ignore_regexp_edit = with_id(new QLineEdit(ignore_group), IDC_IGNORE_REGEXP_EDIT);
  ignore_layout->addWidget(m_h_ignore_numbers, 0, 0);
  ignore_layout->addWidget(m_h_ignore_c_start, 0, 1, 1, 2);
  ignore_layout->addWidget(m_h_ignore_c_have, 1, 0);
  ignore_layout->addWidget(m_h_ignore_c_all, 1, 1, 1, 2);
  ignore_layout->addWidget(m_h_ignore_, 2, 0);
  ignore_layout->addWidget(m_h_ignore_se_apostrophe, 2, 1);
  ignore_layout->addWidget(m_h_ignore_one_letter, 2, 2);
  auto *regexp = new QHBoxLayout;
  regexp->addWidget(with_id(new QLabel(QStringLiteral("RegExp:"), ignore_group), IDC_IGNORE_REGEXP_STATIC));
  regexp->addWidget(m_ignore_regexp_edit, 1);
  ignore_layout->addLayout(regexp, 3, 0, 1, 3);
  layout->addWidget(ignore_group, row++, 0, 1, 2);

  auto *underline_group = new QGroupBox(QStringLiteral("Underline Style"), form);
  auto *underline_layout = new QHBoxLayout(underline_group);
  m_h_underline_color = push_button("", underline_group, IDC_UNDERLINE_COLOR);
  m_h_underline_style = with_id(new QComboBox(underline_group), IDC_UNDERLINE_STYLE);
  underline_layout->addWidget(m_h_underline_color);
  underline_layout->addWidget(m_h_underline_style, 1);
  auto *conversion_group = new QGroupBox(QStringLiteral("Conversion Settings"), form);
  auto *conversion_layout = new QVBoxLayout(conversion_group);
  m_h_ignore_yo = with_id(new QCheckBox(QStringLiteral("Cyrillic: Convert io (ё) to e"), conversion_group), IDC_COUNT_YO_AS_YE);
  m_h_convert_single_quotes = with_id(new QCheckBox(QStringLiteral("Convert ’ to '"), conversion_group), IDC_COUNT_SINGLE_QUOTES_AS_APOSTROPHE);
  conversion_layout->addWidget(m_h_ignore_yo);
  conversion_layout->addWidget(m_h_convert_single_quotes);
  auto *button_group = new QGroupBox(QStringLiteral("Suggestion Button"), form);
  auto *button_layout = new QVBoxLayout(button_group);
  m_h_slider_size = with_id(new QSlider(Qt::Horizontal, button_group), IDC_SLIDER_SIZE);
  m_h_slider_size->setRange(5, 22);
  m_h_slider_sugg_button_opacity = with_id(new QSlider(Qt::Horizontal, button_group), IDC_SLIDER_TRANSPARENCY);
  m_h_slider_sugg_button_opacity->setRange(5, 100);
  button_layout->addWidget(new QLabel(QStringLiteral("Size:"), button_group));
  button_layout->addWidget(m_h_slider_size);
  button_layout->addWidget(new QLabel(QStringLiteral("Opacity:"), button_group));
  button_layout->addWidget(m_h_slider_sugg_button_opacity);
  auto *left = new QVBoxLayout;
  left->addWidget(underline_group);
  left->addWidget(conversion_group);
  layout->addLayout(left, row, 0);
  layout->addWidget(button_group, row++, 1);
  m_reset_btn = push_button("Reset to Default...", form, IDC_RESET_SETTINGS);
  layout->addWidget(m_reset_btn, row++, 1, Qt::AlignRight);
  layout->setRowStretch(row, 1);

  // The controls, as run_dlg_proc handled their WM_COMMAND
  QObject::connect(m_reset_btn, &QPushButton::clicked, form, [this]() { reset_settings(); });
  QObject::connect(m_tokenization_style_cmb, &QComboBox::currentIndexChanged, form, [this]() { setup_delimiter_line_edit_visiblity(); });
  QObject::connect(m_h_default_delimiters, &QPushButton::clicked, form, [this]() { on_default_delimiters_clicked(); });
  QObject::connect(m_h_recheck_delay, &QLineEdit::textChanged, form, [this]() { on_recheck_delay_changed(); });
  QObject::connect(m_h_underline_color, &QPushButton::clicked, form, [this]() { on_underline_color_button_clicked(); });
}

void AdvancedSettingsTab::init_dialog() {
  // As WM_INITDIALOG: the list of styles, the text of the Cyrillic option and the tooltips
  add_enum_items<TokenizationStyle>(m_tokenization_style_cmb);
  m_h_ignore_yo->setText(qstr(rc_str(IDS_CYRILLIC_YO_AS_YE)));
  m_h_edit_delimiters->setToolTip(qstr(rc_str(IDS_DELIMITER_TOOLTIP)));
  m_h_recheck_delay->setToolTip(qstr(rc_str(IDS_RECHECK_DELAY_TOOLTIP)));
  m_h_ignore_c_start->setToolTip(qstr(rc_str(IDS_FIRST_CAPITAL_IGNORE_TOOLTIP)));
  m_h_ignore_se_apostrophe->setToolTip(qstr(rc_str(IDS_IGNORE_BOUND_APOSTROPHE_TOOLTIP)));
  m_h_remove_boundary_apostrophes->setToolTip(qstr(rc_str(IDS_REMOVE_ENDING_APOSTROPHE_TOOLTIP)));
  m_h_underline_style->clear();
  for (const auto &name : get_indic_names())
    m_h_underline_style->addItem(qstr(name));
}

void AdvancedSettingsTab::set_delimeters_edit(const wchar_t *delimiters) {
  m_h_edit_delimiters->setText(qstr(delimiters));
}

void AdvancedSettingsTab::set_recheck_delay(int delay) {
  m_h_recheck_delay->setText(QString::number(delay));
}

int AdvancedSettingsTab::get_recheck_delay() {
  return m_h_recheck_delay->text().toInt();
}

AdvancedSettingsTab::AdvancedSettingsTab(const Settings &settings)
  : m_settings(settings) {
}

void AdvancedSettingsTab::apply_settings(Settings &settings) {
  settings.data.delimiters = m_h_edit_delimiters->text().toStdWString();
  settings.data.ignore_yo = m_h_ignore_yo->isChecked();
  settings.data.convert_single_quotes = m_h_convert_single_quotes->isChecked();
  settings.data.remove_boundary_apostrophes = m_h_remove_boundary_apostrophes->isChecked();
  settings.data.underline_color = static_cast<int>(m_underline_color_btn);
  settings.data.underline_style = m_h_underline_style->currentIndex();
  settings.data.recheck_delay = get_recheck_delay();
  settings.data.check_default_udl_style = m_h_check_default_udl_style->isChecked();
  settings.data.ignore_containing_digit = m_h_ignore_numbers->isChecked();
  settings.data.ignore_starting_with_capital = m_h_ignore_c_start->isChecked();
  settings.data.ignore_having_a_capital = m_h_ignore_c_have->isChecked();
  settings.data.ignore_all_capital = m_h_ignore_c_all->isChecked();
  settings.data.ignore_having_underscore = m_h_ignore_->isChecked();
  settings.data.ignore_starting_or_ending_with_apostrophe = m_h_ignore_se_apostrophe->isChecked();
  settings.data.ignore_one_letter = m_h_ignore_one_letter->isChecked();
  settings.data.suggestion_button_size = m_h_slider_size->value();
  settings.data.suggestion_button_opacity = m_h_slider_sugg_button_opacity->value();
  settings.data.tokenization_style = current_enum<TokenizationStyle>(m_tokenization_style_cmb);
  settings.data.delimiter_exclusions = m_delimiter_exclusions_le->text().toStdWString();
  settings.data.split_camel_case = m_split_camel_case_cb->isChecked();
  settings.data.ignore_regexp_str = m_ignore_regexp_edit->text().toStdWString();
}

SimpleSettingsTab *SettingsDialog::get_simple_dlg() { return &m_simple_dlg; }

AdvancedSettingsTab *SettingsDialog::get_advanced_dlg() { return &m_advanced_dlg; }

SettingsDialog::SettingsDialog(HINSTANCE h_inst, HWND parent, NppInterface &npp, const Settings &settings, const SpellerContainer &speller_container)
  : m_npp(npp), m_simple_dlg(*this, settings, m_npp), m_advanced_dlg(settings), m_settings(settings), m_speller_container(speller_container) {
  StaticDialog::init(h_inst, parent);
  m_settings.settings_changed.connect([this] { update_controls(); });
  m_speller_container.speller_status_changed.connect([this] {
    if (!isCreated())
      return;
    m_simple_dlg.update_language_controls(m_settings, m_speller_container, m_selected_language_name);
    m_simple_dlg.fill_lib_info(m_speller_container.get_aspell_status(), m_settings);
  });
}

void SettingsDialog::destroy() {
  m_simple_dlg.destroy();
  m_advanced_dlg.destroy();
  StaticDialog::destroy();
};

// Send appropriate event and set some npp thread properties
void SettingsDialog::apply_settings() {
  auto mut_settings = m_settings.modify();
  m_simple_dlg.apply_settings(*mut_settings, m_speller_container);
  m_advanced_dlg.apply_settings(*mut_settings);
}

void SettingsDialog::update_controls() {
  if (!isCreated() || !m_advanced_dlg.isCreated())
    return;
  m_simple_dlg.update_controls(m_settings, m_speller_container);
  m_advanced_dlg.update_controls(m_settings);
}

void SettingsDialog::apply_lib_change(SpellerId new_lib_id) {
  auto mut_settings = m_settings.modify();
  mut_settings->data.active_speller_lib_id = new_lib_id;
}

void SettingsDialog::store_selected_language_name(int language_index) {
  m_selected_language_name = language_index != -1
                               ? m_speller_container.active_speller().get_language_list()[language_index].orig_name
                               : multiple_language_alias;
}

void SimpleSettingsTab::init_settings(HINSTANCE h_inst, HWND parent) { return StaticDialog::init(h_inst, parent); }

void SimpleSettingsTab::update_controls(const Settings &settings, const SpellerContainer &speller_container) {
  // As on Windows, setting the controls here gives no notifications
  const QSignalBlocker block_speller(m_speller_cmb);
  const QSignalBlocker block_style(m_language_name_style_cmb);
  set_current_enum(m_speller_cmb, settings.data.active_speller_lib_id);
  fill_lib_info(speller_container.get_aspell_status(), settings);
  fill_sugestions_num(settings.data.suggestion_count);
  set_file_types(settings.data.check_those, settings.data.file_types.c_str());
  m_h_check_comments->setChecked(settings.data.check_comments);
  m_h_check_strings->setChecked(settings.data.check_strings);
  m_h_check_varfunc->setChecked(settings.data.check_variable_functions);
  set_sugg_type(settings.data.suggestions_mode);
  set_one_user_dic(settings.data.use_unified_dictionary);
  set_current_enum(m_language_name_style_cmb, settings.data.language_name_style);
  m_h_select_misspelled_on_menu_cb->setChecked(settings.data.select_word_on_context_menu_click);
}

void SimpleSettingsTab::init_speller_id_combobox(const SpellerContainer &speller_container) {
  const QSignalBlocker blocker(m_speller_cmb);
  m_speller_cmb->clear();
  for (auto id : enum_range<SpellerId>()) {
    if (id == SpellerId::native && !speller_container.native_speller().is_working())
      continue;
    add_enum_item(m_speller_cmb, id);
  }
}

// The form of IDD_SETTINGS, with the two tabs in a QTabWidget in place of Notepad++'s ControlsTab
void SettingsDialog::build_form(QWidget *form) {
  form->setWindowTitle(QStringLiteral("DSpellCheck Settings"));
  auto *layout = new QVBoxLayout(form);
  m_controls_tab = new QTabWidget(form);
  layout->addWidget(m_controls_tab, 1);
  layout->addWidget(make_buttons(form, {{IDOK, L"OK"}, {IDCANCEL, L"Cancel"}, {IDAPPLY, L"Apply"}}, IDOK));

  QObject::connect(item_as<QPushButton>(IDAPPLY), &QPushButton::clicked, form, [this]() { apply_settings(); });
  QObject::connect(item_as<QPushButton>(IDOK), &QPushButton::clicked, form, [this]() {
    display(false);
    apply_settings();
  });
  QObject::connect(item_as<QPushButton>(IDCANCEL), &QPushButton::clicked, form, [this]() { display(false); });
}

void SettingsDialog::init_dialog() {
  m_simple_dlg.init_settings(_hInst, m_controls_tab);
  m_simple_dlg.create(IDD_SIMPLE, false, true);
  m_simple_dlg.init_speller_id_combobox(m_speller_container);
  m_advanced_dlg.init(_hInst, m_controls_tab);
  m_advanced_dlg.create(IDD_ADVANCED, false, true);

  m_controls_tab->addTab(m_simple_dlg.getHSelf(), qstr(rc_str(IDS_SIMPLE_TAB_TEXT)));
  m_controls_tab->addTab(m_advanced_dlg.getHSelf(), qstr(rc_str(IDS_ADVANCED_TAB_TEXT)));

  update_controls();
  m_simple_dlg.update_language_controls(m_settings, m_speller_container, m_selected_language_name);
}

UINT SettingsDialog::do_dialog() {
  if (!isCreated()) {
    create(IDD_SETTINGS);
    goToCenter();
  } else {
    goToCenter();
    update_controls();
  }

  display();
  return 1u;
}
