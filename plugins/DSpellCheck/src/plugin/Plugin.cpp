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

#include "Plugin.h"

//
// put the headers you need here
//

#include "Constants.h"
#include "menuCmdID.h"
#include "resource.h"
#include "Settings.h"
#include "common/raii.h"
#include "common/winapi.h"
#include "core/SpellChecker.h"
#include "core/SpellCheckerHelpers.h"
#include "npp/NppInterface.h"
#include "spellers/HunspellInterface.h"
#include "spellers/LanguageInfo.h" // Qt port: for the dialogs' lists of languages, which GCC wants complete here
#include "spellers/SpellerContainer.h"
#include "ui/AboutDialog.h"
#include "ui/AspellOptionsDialog.h"
#include "ui/ConnectionSettingsDialog.h"
#include "ui/ContextMenuHandler.h"
#include "ui/DownloadDictionariesDialog.h"
#include "ui/SelectMultipleLanguagesDialog.h"
#include "ui/MenuItem.h"
#include "ui/ProgressDialog.h"
#include "ui/RemoveDictionariesDialog.h"
#include "ui/SettingsDialog.h"
#include "ui/SuggestionMenuButton.h"

#include <chrono>

// Qt port: the clipboard, menus, files and event filters of Qt
#include "ResourceStrings.h"
#include "common/TaskWrapper.h"
#include <QAbstractScrollArea>
#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QMenu>
#include <QPointer>
#include <QTimer>
#include <QUrl>

#ifdef VLD_BUILD
#include <vld.h>
#endif // VLD_BUILD

FuncItem func_item[nb_func];
bool resources_inited = false;

FuncItem *get_func_item() {
  // Well maybe we should lock mutex here
  return func_item;
}

//
// The data of Notepad++ that you can use in your plugin commands
//

NppData npp_data;

namespace {
wchar_t ini_file_path[MAX_PATH];
DWORD custom_gui_message_ids[static_cast<int>(CustomGuiMessage::max)] = {0};
bool do_close_tag = false;
std::unique_ptr<SpellChecker> spell_checker;
std::unique_ptr<const SpellerContainer> speller_container;
std::unique_ptr<ContextMenuHandler> context_menu_handler;
std::unique_ptr<SettingsDialog> settings_dlg;
std::unique_ptr<SuggestionMenuButton> suggestions_button;
std::unique_ptr<SelectMultipleLanguagesDialog> lang_list_instance;
std::unique_ptr<RemoveDictionariesDialog> remove_dics_dlg;
std::unique_ptr<AspellOptionsDialog> aspell_options_dlg;
std::unique_ptr<ConnectionSettingsDialog> select_proxy_dlg;
std::unique_ptr<ProgressDialog> progress_dlg;
std::unique_ptr<DownloadDictionariesDialog> download_dics_dlg;
std::unique_ptr<AboutDialog> about_dlg;
std::unique_ptr<const Settings> settings;
int context_menu_id_start;
int langs_menu_id_start = 0;
bool use_allocated_ids;
std::unique_ptr<NppInterface> npp;

HANDLE h_module = nullptr;
std::unique_ptr<QObject> editor_events; // Qt port: the event filter in place of the window subclass and the mouse hook

} // namespace

//
// Initialize your plug-in data here
// It will be called while plug-in loading
HWND get_npp_window() { return npp_data.npp_handle; }

void set_context_menu_id_start(int id) { context_menu_id_start = id; }

void set_langs_menu_id_start(int id) { langs_menu_id_start = id; }

void set_use_allocated_ids(bool value) { use_allocated_ids = value; }

int get_context_menu_id_start() { return context_menu_id_start; }

int get_langs_menu_id_start() { return langs_menu_id_start; }

bool get_use_allocated_ids() { return use_allocated_ids; }

const Settings &get_settings() { return *settings; }

std::wstring get_default_hunspell_path() {
  std::wstring path = ini_file_path;
  return path.substr(0, path.rfind(L'/')) + L"/Hunspell"; // Qt port: '/'
}

void set_hmodule(HANDLE h_module_arg) {
  h_module = h_module_arg;
  // Init it all dialog classes:
}

SelectMultipleLanguagesDialog *get_lang_list() { return lang_list_instance.get(); }

RemoveDictionariesDialog *get_remove_dics() { return remove_dics_dlg.get(); }
AspellOptionsDialog *get_aspell_options_dlg() { return aspell_options_dlg.get(); }

ConnectionSettingsDialog *get_select_proxy() { return select_proxy_dlg.get(); }

ProgressDialog *get_progress_dlg() { return progress_dlg.get(); }

DownloadDictionariesDialog *get_download_dics() { return download_dics_dlg.get(); }

HANDLE get_h_module() { return h_module; }

std::vector<MenuItem> cur_menu_list;
bool is_editor_context_menu_expected = false;

// Qt port: Qt's events of Notepad++'s widgets, which the plugin may filter as HWND is a QWidget* here, in place of
// the subclass of the main window and the mouse hook:
// - a right click in an editor, before Notepad++ makes its context menu, as WM_CONTEXTMENU came to the subclass,
// - that menu when it is polished, which comes before it is sized and shown, to add the suggestions to it as
//   WM_INITMENUPOPUP did,
// - the mouse over the text, for the suggestion button, as the mouse hook had it, and
// - the main window moving, as WM_MOVE came to messageProc.
class EditorEvents : public QObject {
public:
  EditorEvents() {
    for (HWND editor : {npp_data.scintilla_main_handle, npp_data.scintilla_second_handle}) {
      editor->installEventFilter(this);
      if (auto *scroll_area = qobject_cast<QAbstractScrollArea *>(editor))
        scroll_area->viewport()->installEventFilter(this);
    }
    npp_data.npp_handle->installEventFilter(this);
  }

protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    switch (event->type()) {
    case QEvent::ContextMenu:
      if (watched == npp_data.scintilla_main_handle || watched == npp_data.scintilla_second_handle) {
        suggestions_button->display(false);
        is_editor_context_menu_expected = true;
        // Only for the menu that this click makes, which is polished before the menu's own event loop runs
        QTimer::singleShot(0, this, []() { is_editor_context_menu_expected = false; });
      }
      break;
    case QEvent::ChildPolished:
      if (watched == npp_data.npp_handle && is_editor_context_menu_expected) {
        if (auto *menu = qobject_cast<QMenu *>(static_cast<QChildEvent *>(event)->child())) {
          is_editor_context_menu_expected = false;
          context_menu_handler->precalculate_menu();
          if (!cur_menu_list.empty())
            MenuItem::prepend_to_menu(menu, cur_menu_list);
          cur_menu_list.clear();
        }
      }
      break;
    case QEvent::MouseMove:
      if (context_menu_handler && suggestions_button)
        context_menu_handler->init_suggestions_box(*suggestions_button);
      break;
    case QEvent::Move:
    case QEvent::Resize:
      if (watched == npp_data.npp_handle && suggestions_button)
        suggestions_button->display(false);
      break;
    default:
      break;
    }
    return false;
  }
};

void create_hooks() {
  editor_events = std::make_unique<EditorEvents>();
}

void plugin_clean_up() {
  // Qt port: the plugin's Qt objects and threads go before it is unloaded, as their code is in it
  editor_events.reset();
  wait_for_background_tasks();
  {
    auto mut = speller_container->modify();
    mut->cleanup();
  }
  WinApi::delete_file(get_debug_log_path().c_str());
}

// Qt port: no window messages are needed, as background tasks call back through Qt's queued calls
void register_custom_messages() {}

void init_npp_interface() { npp = std::make_unique<NppInterface>(&npp_data); }

void notify(SCNotification *notify_code) { npp->notify(notify_code); }

NppInterface &npp_interface() { return *npp; }

void erase_misspellings() { spell_checker->erase_all_misspellings(); }

void show_spell_check_menu_at_cursor() {
  // Qt port: a QMenu at the caret, whose items send their commands to Notepad++, as for TPM_RETURNCMD the result went
  // to process_menu_result
  context_menu_handler->update_word_under_cursor_data();
  if (context_menu_handler->is_word_under_cursor_correct())
    return;
  QMenu menu(npp->get_view_hwnd());
  MenuItem::append_to_menu(&menu, context_menu_handler->get_suggestion_menu_items());

  ACTIVE_VIEW_BLOCK(*npp);
  auto start = npp->get_selection_start();
  auto x = npp->get_point_x_from_position(start);
  auto y = npp->get_point_y_from_position(start);
  auto *scroll_area = qobject_cast<QAbstractScrollArea *>(npp->get_view_hwnd());
  QWidget *text_area = scroll_area != nullptr ? scroll_area->viewport() : npp->get_view_hwnd();
  const QPoint point = text_area->mapToGlobal(QPoint(x, y));
  menu.exec(QPoint(point.x() - menu.sizeHint().width(), point.y())); // TPM_RIGHTALIGN
}

void replace_with_1st_suggestion() {
  SpellCheckerHelpers::replace_current_word_with_topmost_suggestion(*npp, *spell_checker, *speller_container);
}

void ignore_for_current_session() {
  TextPosition pos, length;
  if (!spell_checker->is_word_under_cursor_correct(pos, length, true)) {
    ACTIVE_VIEW_BLOCK(*npp);
    auto wstr = npp->get_mapped_wstring_range(pos, pos + length);
    {
      auto mut = speller_container->modify();
      mut->ignore_word(std::move(wstr.str));
    }
  }
}

void copy_misspellings_to_clipboard() {
  auto str = spell_checker->get_all_misspellings_as_string();
  QApplication::clipboard()->setText(qstr(str)); // Qt port: the clipboard of Qt
}

void mark_lines_with_misspelling() {
  spell_checker->mark_lines_with_misspelling();
}

void reload_hunspell_dictionaries() {
  speller_container->get_hunspell_speller().reset_spellers();
  settings->settings_changed();
}

DWORD get_custom_gui_message_id(CustomGuiMessage message_id) { return custom_gui_message_ids[static_cast<int>(message_id)]; } // Qt port: unused

void switch_auto_check_text() {
  auto mut = settings->modify(SettingsModificationStyle::ignore_file_errors);
  mut->data.auto_check_text = !settings->data.auto_check_text;
}

void switch_debug_logging() {
  auto mut = settings->modify(SettingsModificationStyle::ignore_file_errors);
  mut->data.write_debug_log = !mut->data.write_debug_log;
  if (mut->data.write_debug_log) {
    WinApi::delete_file(get_debug_log_path().c_str());
  }
}

void open_debug_log() {
  QDesktopServices::openUrl(QUrl::fromLocalFile(qstr(get_debug_log_path()))); // Qt port: in place of ShellExecute
}

void start_settings() { settings_dlg->do_dialog(); }

void start_manual() {
  QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/Predelnik/DSpellCheck/wiki/Manual"))); // Qt port: in place of ShellExecute
}

void start_about_dlg() { about_dlg->do_dialog(); }

void start_language_list() { lang_list_instance->do_dialog(); }

void recheck_visible() {
  print_to_log(L"recheck_visible ()", npp->get_editor_hwnd());
  ACTIVE_VIEW_BLOCK(npp_interface());
  spell_checker->recheck_visible();
}

void find_next_mistake() { spell_checker->find_next_mistake(); }

void find_prev_mistake() { spell_checker->find_prev_mistake(); }

void quick_lang_change_context() {
  if (QMenu *menu = get_langs_sub_menu()) // Qt port: the submenu at the cursor, in place of TrackPopupMenu
    menu->exec(QCursor::pos());
}

enum_array<Action, int> action_index = []() {
  enum_array<Action, int> val;
  val.fill(-1);
  return val;
}();

//
// Initialization of your plug-in commands
// You should fill your plug-ins commands here
void command_menu_init() {
  //
  // Firstly we get the parameters from your plugin config file (if any)
  //

  // get path of plugin configuration
  ::SendMessage(npp_data.npp_handle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, reinterpret_cast<LPARAM>(ini_file_path));

  // if config path doesn't exist, we create it
  QDir().mkpath(QString::fromWCharArray(ini_file_path)); // Qt port: QDir, in place of CreateDirectory

  // make your plugin config file full file path name
  wcscat_s(ini_file_path, L"/"); // Qt port: in place of PathAppend
  wcscat_s(ini_file_path, config_file_name);

  //--------------------------------------------//
  //-- STEP 3. CUSTOMIZE YOUR PLUGIN COMMANDS --//
  //--------------------------------------------//
  // with function :
  // setCommand(int index,                      // zero based number to indicate
  // the order of command
  //            wchar_t *commandName,             // the command name that you
  //            want to see in plugin menu
  //            PFUNCPLUGINCMD functionPointer, // the symbol of function
  //            (function pointer) associated with this command. The body should
  //            be defined below. See Step 4.
  //            ShortcutKey *shortcut,          // optional. Define a shortcut
  //            to trigger this command
  //            bool check0nInit                // optional. Make this menu item
  //            be checked visually
  //            );
  action_index[Action::toggle_auto_spell_check] = set_next_command(rc_str(IDS_AUTO_SPELL_CHECK).c_str(), switch_auto_check_text);
  action_index[Action::find_next_error] = set_next_command(rc_str(IDS_FIND_NEXT_ERROR).c_str(), find_next_mistake);
  action_index[Action::find_prev_error] = set_next_command(rc_str(IDS_FIND_PREV_ERROR).c_str(), find_prev_mistake);

  action_index[Action::quick_language_change] = set_next_command(rc_str(IDS_CHANGE_CURRENT_LANG).c_str(), quick_lang_change_context);

  set_next_command(L"---", nullptr);

  action_index[Action::settings] = set_next_command(rc_str(IDS_SETTINGS).c_str(), start_settings);
  action_index[Action::copy_all_misspellings] = set_next_command(rc_str(IDS_COPY_ALL_MISSPELLED).c_str(), copy_misspellings_to_clipboard);
  action_index[Action::erase_all_misspellings] = set_next_command(rc_str(IDS_ERASE_ALL_MISSPELLED).c_str(), erase_misspellings);
  action_index[Action::reload_user_dictionaries] = set_next_command(rc_str(IDS_RELOAD_HUNSPELL).c_str(), reload_hunspell_dictionaries);
  action_index[Action::toggle_debug_logging] = set_next_command(rc_str(IDS_SWITCH_DEBUG_LOGGING).c_str(), switch_debug_logging);
  action_index[Action::open_debug_log] = set_next_command(rc_str(IDS_OPEN_DEBUG_LOG).c_str(), open_debug_log);
  action_index[Action::online_manual] = set_next_command(rc_str(IDS_ONLINE_MANUAL).c_str(), start_manual);
  action_index[Action::about] = set_next_command(rc_str(IDS_ABOUT).c_str(), start_about_dlg);
  action_index[Action::show_spell_check_menu_at_cursor] =
      set_next_command(rc_str(IDS_SHOW_SPELL_CHECK_MENU_AT_CURSOR).c_str(), show_spell_check_menu_at_cursor);

  action_index[Action::replace_with_1st_suggestion] = set_next_command(rc_str(IDS_REPLACE_WITH_1ST_SUGGESTION).c_str(), replace_with_1st_suggestion);

  action_index[Action::ignore_for_current_session] = set_next_command(rc_str(IDS_IGNORE_WORD_AT_CURSOR).c_str(), ignore_for_current_session);
  action_index[Action::mark_lines_with_misspelling] = set_next_command(rc_str(IDS_BOOKMARK_LINES_WITH_MISSPELLING).c_str(), mark_lines_with_misspelling);
  // add further set_next_command at the bottom to avoid breaking configured hotkeys
}

void add_icons() {
  // Qt port: the pictures of the plugin's Qt resources
  static ToolbarIconsWrapper auto_check_icon{L":/DSpellCheck/Autocheck.ico", L":/DSpellCheck/Autocheck-dark.ico", L":/DSpellCheck/Autocheck-bmp.bmp"};
  ::SendMessage(npp_data.npp_handle, NPPM_ADDTOOLBARICON_FORDARKMODE, static_cast<WPARAM>(func_item[0].cmd_id), reinterpret_cast<LPARAM>(auto_check_icon.get()));
}

// Qt port: the plugin's QMenu in the Plugins menu, and the submenu of an item, which are QMenus and QActions here
static QPointer<QMenu> dspellcheck_menu_cached;

HMENU get_this_plugin_menu() {
  if (dspellcheck_menu_cached)
    return dspellcheck_menu_cached;
  HMENU plugins_menu = npp->get_menu_handle(NPPPLUGINMENU);
  if (plugins_menu == nullptr)
    return nullptr;
  for (QAction *action : plugins_menu->actions()) {
    if (action->menu() != nullptr && action->text().remove(u'&') == qstr(getName())) {
      dspellcheck_menu_cached = action->menu();
      break;
    }
  }
  return dspellcheck_menu_cached;
}

static QAction *find_plugin_menu_action(HMENU menu, int item_id) {
  if (menu == nullptr)
    return nullptr;
  for (QAction *action : menu->actions()) {
    if (action->data().toInt() == item_id)
      return action;
    if (QAction *found = action->menu() != nullptr ? find_plugin_menu_action(action->menu(), item_id) : nullptr)
      return found;
  }
  return nullptr;
}

HMENU get_submenu(int item_id) {
  QAction *action = find_plugin_menu_action(get_this_plugin_menu(), item_id);
  return action != nullptr ? action->menu() : nullptr;
}

HMENU get_langs_sub_menu() { return get_submenu(get_func_item()[action_index[Action::quick_language_change]].cmd_id); }

void on_settings_changed() {
  npp->set_menu_item_check(get_func_item()[action_index[Action::toggle_auto_spell_check]].cmd_id, settings->data.auto_check_text);
  npp->set_menu_item_check(get_func_item()[action_index[Action::toggle_debug_logging]].cmd_id, settings->data.write_debug_log);
}

void init_classes() {
  // Qt port: no Windows controls to register
  settings = std::make_unique<Settings>(ini_file_path);
  settings->settings_changed.connect(on_settings_changed);

  speller_container = std::make_unique<SpellerContainer>(settings.get(), &npp_data);

  spell_checker = std::make_unique<SpellChecker>(settings.get(), *npp, *speller_container);

  context_menu_handler = std::make_unique<ContextMenuHandler>(*settings, *speller_container, *npp, *spell_checker);

  suggestions_button = std::make_unique<SuggestionMenuButton>(static_cast<HINSTANCE>(h_module), npp_data.npp_handle, *npp, *context_menu_handler, *settings);
  suggestions_button->do_dialog();

  settings_dlg = std::make_unique<SettingsDialog>(static_cast<HINSTANCE>(h_module), npp_data.npp_handle, *npp, *settings, *speller_container);
  download_dics_dlg = std::make_unique<DownloadDictionariesDialog>(static_cast<HINSTANCE>(h_module), npp_data.npp_handle, *settings, *speller_container);

  settings_dlg->download_dics_dlg_requested.connect([]() { download_dics_dlg->do_dialog(); });

  about_dlg = std::make_unique<AboutDialog>();
  about_dlg->init(static_cast<HINSTANCE>(h_module), npp_data.npp_handle);

  progress_dlg = std::make_unique<ProgressDialog>(*download_dics_dlg);
  progress_dlg->init(static_cast<HINSTANCE>(h_module), npp_data.npp_handle);

  lang_list_instance = std::make_unique<SelectMultipleLanguagesDialog>(static_cast<HINSTANCE>(h_module), npp_data.npp_handle, *settings, *speller_container);

  select_proxy_dlg = std::make_unique<ConnectionSettingsDialog>(*settings, *download_dics_dlg);
  select_proxy_dlg->init(static_cast<HINSTANCE>(h_module), npp_data.npp_handle);

  remove_dics_dlg = std::make_unique<RemoveDictionariesDialog>(static_cast<HINSTANCE>(h_module), npp_data.npp_handle, *settings, *speller_container);
  aspell_options_dlg = std::make_unique<AspellOptionsDialog>(static_cast<HINSTANCE>(h_module), npp_data.npp_handle, *settings, *speller_container);

  {
    auto mut = settings->modify_without_saving();
    mut->load();
  }
  resources_inited = true;
}

void command_menu_clean_up() {
}

//
// Function that initializes plug-in commands
//
static int counter = 0;

std::vector<std::unique_ptr<ShortcutKey>> shortcut_storage;

int set_next_command(const wchar_t *cmd_name, Pfuncplugincmd p_func) {
  if (counter >= nb_func) {
    assert(false); // Less actions specified in nb_func constant than added
    return -1;
  }

  if (p_func == nullptr) {
    counter++;
    return counter - 1;
  }

  lstrcpy(func_item[counter].item_name, cmd_name);
  func_item[counter].p_func = p_func;
  func_item[counter].init2_check = false;
  func_item[counter].p_sh_key = nullptr;
  counter++;

  return counter - 1;
}

std::wstring get_debug_log_path() {
  return (QDir::tempPath() + QStringLiteral("/DSpellCheck_Debug_Log.txt")).toStdWString(); // Qt port: QDir, in place of GetTempPath
}

// Qt port: the actions move into a submenu "Additional Actions" before Settings, with the connections Notepad++ gave them
void rearrange_menu() {
  auto plugin_menu = get_this_plugin_menu();
  if (plugin_menu == nullptr)
    return;
  auto submenu = new QMenu(qstr(rc_str(IDS_ADDITIONAL_ACTIONS)), plugin_menu);
  auto list = {
      Action::copy_all_misspellings, Action::erase_all_misspellings, Action::mark_lines_with_misspelling,
      Action::replace_with_1st_suggestion, Action::ignore_for_current_session,
      Action::show_spell_check_menu_at_cursor, Action::reload_user_dictionaries, Action::toggle_debug_logging, Action::open_debug_log};
  for (auto action : list) {
    if (QAction *item = find_plugin_menu_action(plugin_menu, get_func_item()[action_index[action]].cmd_id)) {
      plugin_menu->removeAction(item);
      submenu->addAction(item);
    }
  }
  plugin_menu->insertMenu(find_plugin_menu_action(plugin_menu, get_func_item()[action_index[Action::settings]].cmd_id), submenu);
}

extern FuncItem func_item[nb_func]; // NOLINT
extern NppData npp_data;            // NOLINT
extern bool do_close_tag;           // NOLINT
std::vector<std::pair<TextPosition, TextPosition>> check_queue;
std::optional<WinApi::Timer> edit_recheck_timer;
std::optional<WinApi::Timer> scroll_recheck_timer;
constexpr int scroll_recheck_timer_resolution = 100;
bool restyling_caused_recheck_was_done = false; // Hack to avoid eternal cycle in case of scintilla bug
bool first_restyle = true;                      // hack to successfully avoid checking hyperlinks
// when they appear on program start

// Qt port: the menu items for the context menu that is being made, which EditorEvents adds to it
LRESULT
show_calculated_menu(std::vector<MenuItem> &&menu_list) {
  cur_menu_list = std::move(menu_list);
  return 0;
}

void init_menu_ids() {
  bool res = npp->is_allocate_cmdid_supported();

  if (res) {
    set_use_allocated_ids(true);
    auto id = npp->allocate_cmdid(requested_menu_count);
    set_context_menu_id_start(id);
    set_langs_menu_id_start(id + 103);
  }
}

// ReSharper disable CppInconsistentNaming
extern "C" __declspec(dllexport) void setInfo(NppData notpadPlusData) {
  npp_data = notpadPlusData;
  init_npp_interface();
  init_menu_ids();
  command_menu_init();
}

extern "C" __declspec(dllexport) const wchar_t *getName() {
  static std::wstring plugin_name = rc_str(IDS_PLUGIN_NAME);
  return plugin_name.c_str();
}

extern "C" __declspec(dllexport) FuncItem *getFuncsArray(int *nbF) {
  *nbF = nb_func;
  return func_item;
}

// ReSharper restore CppInconsistentNaming

bool is_any_timer_active() {
  if (edit_recheck_timer && edit_recheck_timer->is_set())
    return true;

  if (scroll_recheck_timer && scroll_recheck_timer->is_set())
    return true;

  return false;
}

// For now doesn't look like there is such a need in check modified, but code
// stays until thorough testing
void WINAPI edit_recheck_callback() {
  edit_recheck_timer->stop_timer();

  ACTIVE_VIEW_BLOCK(npp_interface());
  spell_checker->recheck_visible();
  if (!first_restyle)
    restyling_caused_recheck_was_done = true;
  first_restyle = false;
}

void WINAPI scroll_recheck_callback() {
  scroll_recheck_timer->stop_timer();

  ACTIVE_VIEW_BLOCK(npp_interface());
  spell_checker->recheck_visible();
  if (!first_restyle)
    restyling_caused_recheck_was_done = true;
  first_restyle = false;
}

// Qt port: the strings of the plugin's string table, in place of LoadString
std::wstring_view rc_str_view(UINT string_id) {
  for (const auto &[id, text] : resource_strings) {
    if (id == string_id)
      return text;
  }
  return {};
}

std::wstring rc_str(UINT string_id) { return std::wstring{rc_str_view(string_id)}; }

void print_to_log(std::wstring_view line, HWND parent_wnd) {
  if (!settings)
    return;

  if (!settings->data.write_debug_log)
    return;

  // Qt port: in UTF-8, with _wfopen and fprintf in place of _wfopen_s and _fwprintf_p
  FILE *fp = _wfopen(get_debug_log_path().c_str(), L"a");
  if (fp == nullptr) {
    MessageBox(parent_wnd, L"Error while writing to a log file", to_wstring(strerror(errno)).c_str(), MB_OK);
    return;
  }
  const auto utf8 = to_utf8_string(line);
  fprintf(fp, "%s\n", utf8.c_str());
  fclose(fp);
}

void delete_log() {
  if (settings->data.write_debug_log)
    WinApi::delete_file(get_debug_log_path().c_str());
}

void update_on_visible_area_changed() {
  if (!is_any_timer_active() && scroll_recheck_timer) {
    scroll_recheck_timer->set_resolution(std::chrono::milliseconds(scroll_recheck_timer_resolution));
  }
}

// ReSharper disable once CppInconsistentNaming
extern "C" __declspec(dllexport) void beNotified(SCNotification *notify_code) {
  notify(notify_code);
  switch (notify_code->nmhdr.code) {
  case NPPN_SHUTDOWN: {
    print_to_log(L"NPPN_SHUTDOWN", npp->get_editor_hwnd());
    edit_recheck_timer.reset();
    scroll_recheck_timer.reset();
    command_menu_clean_up();

    plugin_clean_up();
  }
  break;

  case NPPN_READY: {
    register_custom_messages();
    init_classes();
    print_to_log(L"NPPN_READY", npp->get_editor_hwnd());
    check_queue.clear();
    create_hooks();
    edit_recheck_timer.emplace(npp_data.npp_handle);
    edit_recheck_timer->on_timer_tick.connect(edit_recheck_callback);
    scroll_recheck_timer.emplace(npp_data.npp_handle);
    scroll_recheck_timer->on_timer_tick.connect(scroll_recheck_callback);
    spell_checker->recheck_visible_both_views();
    restyling_caused_recheck_was_done = false;
    suggestions_button->set_transparency();
    rearrange_menu();
  }
  break;

  case NPPN_BUFFERACTIVATED: {
    if (settings)
      print_to_log(L"NPPN_BUFFERACTIVATED", npp->get_editor_hwnd());
    if (!spell_checker)
      return;
    recheck_visible();
    restyling_caused_recheck_was_done = false;
  }
  break;

  case SCN_FOLDINGSTATECHANGED:
    update_on_visible_area_changed();
    break;

  case SCN_UPDATEUI:
    if (!spell_checker)
      return;
    if ((notify_code->updated & SC_UPDATE_CONTENT) != 0 && (!is_any_timer_active() || first_restyle) && !restyling_caused_recheck_was_done)
      // If restyling wasn't caused
      // by user input...
    {
      ACTIVE_VIEW_BLOCK(npp_interface());
      spell_checker->recheck_visible();
      if (!first_restyle)
        restyling_caused_recheck_was_done = true;
      first_restyle = false;
    } else if ((notify_code->updated & (SC_UPDATE_V_SCROLL | SC_UPDATE_H_SCROLL)) != 0) // If scroll wasn't caused by user input...
    {
      update_on_visible_area_changed();
    }
    if (!suggestions_button->is_pressed())
      suggestions_button->display(false);
    break;

  case SCN_ZOOM:
    update_on_visible_area_changed();
    break;

  case SCN_MODIFIED:
    if (!spell_checker)
      return;
    if (edit_recheck_timer && (notify_code->modificationType & (SC_MOD_DELETETEXT | SC_MOD_INSERTTEXT)) != 0) {
      edit_recheck_timer->set_resolution(std::chrono::milliseconds(get_settings().data.recheck_delay));
    }
    break;

  case NPPN_LANGCHANGED: {
    print_to_log(L"NPPN_LANGCHANGED", npp->get_editor_hwnd());
    if (!spell_checker)
      return;
    spell_checker->recheck_visible_on_active_view();
  }
  break;

  case NPPN_TBMODIFICATION: {
    if (settings)
      print_to_log(L"NPPN_TBMODIFICATION", npp->get_editor_hwnd());
    add_icons();
  }

  default:
    return;
  }
}

// Here you can process the Npp Messages
// I will make the messages accessible little by little, according to the need
// of plugin development.
// Please let me know if you need to access to some messages :

void init_needed_dialogs(WPARAM w_param) {
  // A little bit of code duplication here :(
  auto menu_id = w_param;
  if ((!get_use_allocated_ids() && HIBYTE(menu_id) != menu_id::plguin_menu && HIBYTE(menu_id) != menu_id::language_menu) ||
      (get_use_allocated_ids() && (static_cast<int>(menu_id) < get_context_menu_id_start() || static_cast<int>(menu_id) > get_context_menu_id_start() + 350)))
    return;
  int used_menu_id;
  if (get_use_allocated_ids()) {
    used_menu_id = (static_cast<int>(menu_id) < get_langs_menu_id_start() ? menu_id::plguin_menu : menu_id::language_menu);
  } else {
    used_menu_id = HIBYTE(menu_id);
  }
  switch (used_menu_id) {
  case menu_id::language_menu:
    WPARAM result;
    if (!get_use_allocated_ids())
      result = LOBYTE(menu_id);
    else
      result = menu_id - get_langs_menu_id_start();
    if (result == menu_id::download_dictionaries)
      download_dics_dlg->do_dialog();
    else if (result == menu_id::customize_multiple_languages) {
      get_lang_list()->do_dialog();
    } else if (result == menu_id::remove_dictionaries) {
      get_remove_dics()->do_dialog();
    }
    break;
  }
}

extern "C" __declspec(dllexport) LRESULT
// ReSharper disable once CppInconsistentNaming
messageProc(UINT message, WPARAM w_param, LPARAM /*l_param*/) {
  // NOLINT
  switch (message) {
  case WM_MOVE:
    if (suggestions_button)
      suggestions_button->display(false);
    return FALSE;
  case WM_COMMAND: {
    if (HIWORD(w_param) == 0 && get_use_allocated_ids()) {
      init_needed_dialogs(w_param);
      context_menu_handler->process_menu_result(w_param);
    }
  }
  break;
  }

  return FALSE;
}

#ifdef UNICODE
// ReSharper disable once CppInconsistentNaming
extern "C" __declspec(dllexport) BOOL isUnicode() { return TRUE; } // NOLINT
#endif                                                             // UNICODE
