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

#include "aspell.h"

#include "common/Utility.h"

#include <iostream>

// Qt port: the library is loaded with dlopen and dlsym, where Windows has LoadLibrary and GetProcAddress
#include <dlfcn.h>
#include <QFile>
#include <QString>

HANDLE g_h_module;

void *h_inst_lib = nullptr; // Qt port: a handle of dlopen

PFUNC_aspell_mutable_container_add aspell_mutable_container_add = nullptr;
PFUNC_aspell_mutable_container_remove aspell_mutable_container_remove = nullptr;
PFUNC_aspell_mutable_container_clear aspell_mutable_container_clear = nullptr;
PFUNC_aspell_mutable_container_to_mutable_container
aspell_mutable_container_to_mutable_container = nullptr;
PFUNC_aspell_key_info_enumeration_at_end aspell_key_info_enumeration_at_end =
    nullptr;
PFUNC_aspell_key_info_enumeration_next aspell_key_info_enumeration_next = nullptr;
PFUNC_delete_aspell_key_info_enumeration delete_aspell_key_info_enumeration =
    nullptr;
PFUNC_aspell_key_info_enumeration_clone aspell_key_info_enumeration_clone =
    nullptr;
PFUNC_aspell_key_info_enumeration_assign aspell_key_info_enumeration_assign =
    nullptr;
PFUNC_new_aspell_config new_aspell_config = nullptr;
PFUNC_delete_aspell_config delete_aspell_config = nullptr;
PFUNC_aspell_config_clone aspell_config_clone = nullptr;
PFUNC_aspell_config_assign aspell_config_assign = nullptr;
PFUNC_aspell_config_error_number aspell_config_error_number = nullptr;
PFUNC_aspell_config_error_message aspell_config_error_message = nullptr;
PFUNC_aspell_config_error aspell_config_error = nullptr;
PFUNC_aspell_config_set_extra aspell_config_set_extra = nullptr;
PFUNC_aspell_config_keyinfo aspell_config_keyinfo = nullptr;
PFUNC_aspell_config_possible_elements aspell_config_possible_elements = nullptr;
PFUNC_aspell_config_get_default aspell_config_get_default = nullptr;
PFUNC_aspell_config_elements aspell_config_elements = nullptr;
PFUNC_aspell_config_replace aspell_config_replace = nullptr;
PFUNC_aspell_config_remove aspell_config_remove = nullptr;
PFUNC_aspell_config_have aspell_config_have = nullptr;
PFUNC_aspell_config_retrieve aspell_config_retrieve = nullptr;
PFUNC_aspell_config_retrieve_list aspell_config_retrieve_list = nullptr;
PFUNC_aspell_config_retrieve_bool aspell_config_retrieve_bool = nullptr;
PFUNC_aspell_config_retrieve_int aspell_config_retrieve_int = nullptr;
PFUNC_aspell_error_number aspell_error_number = nullptr;
PFUNC_aspell_error_message aspell_error_message = nullptr;
PFUNC_aspell_error aspell_error = nullptr;
PFUNC_delete_aspell_can_have_error delete_aspell_can_have_error = nullptr;
PFUNC_new_aspell_speller new_aspell_speller = nullptr;
PFUNC_to_aspell_speller to_aspell_speller = nullptr;
PFUNC_delete_aspell_speller delete_aspell_speller = nullptr;
PFUNC_aspell_speller_error_number aspell_speller_error_number = nullptr;
PFUNC_aspell_speller_error_message aspell_speller_error_message = nullptr;
PFUNC_aspell_speller_error aspell_speller_error = nullptr;
PFUNC_aspell_speller_config aspell_speller_config = nullptr;
PFUNC_aspell_speller_check aspell_speller_check = nullptr;
PFUNC_aspell_speller_add_to_personal aspell_speller_add_to_personal = nullptr;
PFUNC_aspell_speller_add_to_session aspell_speller_add_to_session = nullptr;
PFUNC_aspell_speller_personal_word_list aspell_speller_personal_word_list =
    nullptr;
PFUNC_aspell_speller_session_word_list aspell_speller_session_word_list = nullptr;
PFUNC_aspell_speller_main_word_list aspell_speller_main_word_list = nullptr;
PFUNC_aspell_speller_save_all_word_lists aspell_speller_save_all_word_lists =
    nullptr;
PFUNC_aspell_speller_clear_session aspell_speller_clear_session = nullptr;
PFUNC_aspell_speller_suggest aspell_speller_suggest = nullptr;
PFUNC_aspell_speller_store_replacement aspell_speller_store_replacement = nullptr;
PFUNC_delete_aspell_filter delete_aspell_filter = nullptr;
PFUNC_aspell_filter_error_number aspell_filter_error_number = nullptr;
PFUNC_aspell_filter_error_message aspell_filter_error_message = nullptr;
PFUNC_aspell_filter_error aspell_filter_error = nullptr;
PFUNC_to_aspell_filter to_aspell_filter = nullptr;
PFUNC_delete_aspell_document_checker delete_aspell_document_checker = nullptr;
PFUNC_aspell_document_checker_error_number
aspell_document_checker_error_number = nullptr;
PFUNC_aspell_document_checker_error_message
aspell_document_checker_error_message = nullptr;
PFUNC_aspell_document_checker_error aspell_document_checker_error = nullptr;
PFUNC_new_aspell_document_checker new_aspell_document_checker = nullptr;
PFUNC_to_aspell_document_checker to_aspell_document_checker = nullptr;
PFUNC_aspell_document_checker_reset aspell_document_checker_reset = nullptr;
PFUNC_aspell_document_checker_process aspell_document_checker_process = nullptr;
PFUNC_aspell_document_checker_next_misspelling
aspell_document_checker_next_misspelling = nullptr;
PFUNC_aspell_document_checker_filter aspell_document_checker_filter = nullptr;
PFUNC_aspell_word_list_empty aspell_word_list_empty = nullptr;
PFUNC_aspell_word_list_size aspell_word_list_size = nullptr;
PFUNC_aspell_word_list_elements aspell_word_list_elements = nullptr;
PFUNC_delete_aspell_string_enumeration delete_aspell_string_enumeration = nullptr;
PFUNC_aspell_string_enumeration_clone aspell_string_enumeration_clone = nullptr;
PFUNC_aspell_string_enumeration_assign aspell_string_enumeration_assign = nullptr;
PFUNC_aspell_string_enumeration_at_end aspell_string_enumeration_at_end = nullptr;
PFUNC_aspell_string_enumeration_next aspell_string_enumeration_next = nullptr;
PFUNC_get_aspell_module_info_list get_aspell_module_info_list = nullptr;
PFUNC_aspell_module_info_list_empty aspell_module_info_list_empty = nullptr;
PFUNC_aspell_module_info_list_size aspell_module_info_list_size = nullptr;
PFUNC_aspell_module_info_list_elements aspell_module_info_list_elements = nullptr;
PFUNC_get_aspell_dict_info_list get_aspell_dict_info_list = nullptr;
PFUNC_aspell_dict_info_list_empty aspell_dict_info_list_empty = nullptr;
PFUNC_aspell_dict_info_list_size aspell_dict_info_list_size = nullptr;
PFUNC_aspell_dict_info_list_elements aspell_dict_info_list_elements = nullptr;
PFUNC_aspell_module_info_enumeration_at_end
aspell_module_info_enumeration_at_end = nullptr;
PFUNC_aspell_module_info_enumeration_next aspell_module_info_enumeration_next =
    nullptr;
PFUNC_delete_aspell_module_info_enumeration
delete_aspell_module_info_enumeration = nullptr;
PFUNC_aspell_module_info_enumeration_clone
aspell_module_info_enumeration_clone = nullptr;
PFUNC_aspell_module_info_enumeration_assign
aspell_module_info_enumeration_assign = nullptr;
PFUNC_aspell_dict_info_enumeration_at_end aspell_dict_info_enumeration_at_end =
    nullptr;
PFUNC_aspell_dict_info_enumeration_next aspell_dict_info_enumeration_next =
    nullptr;
PFUNC_delete_aspell_dict_info_enumeration delete_aspell_dict_info_enumeration =
    nullptr;
PFUNC_aspell_dict_info_enumeration_clone aspell_dict_info_enumeration_clone =
    nullptr;
PFUNC_aspell_dict_info_enumeration_assign aspell_dict_info_enumeration_assign =
    nullptr;
PFUNC_new_aspell_string_list new_aspell_string_list = nullptr;
PFUNC_aspell_string_list_empty aspell_string_list_empty = nullptr;
PFUNC_aspell_string_list_size aspell_string_list_size = nullptr;
PFUNC_aspell_string_list_elements aspell_string_list_elements = nullptr;
PFUNC_aspell_string_list_add aspell_string_list_add = nullptr;
PFUNC_aspell_string_list_remove aspell_string_list_remove = nullptr;
PFUNC_aspell_string_list_clear aspell_string_list_clear = nullptr;
PFUNC_aspell_string_list_to_mutable_container
aspell_string_list_to_mutable_container = nullptr;
PFUNC_delete_aspell_string_list delete_aspell_string_list = nullptr;
PFUNC_aspell_string_list_clone aspell_string_list_clone = nullptr;
PFUNC_aspell_string_list_assign aspell_string_list_assign = nullptr;
PFUNC_new_aspell_string_map new_aspell_string_map = nullptr;
PFUNC_aspell_string_map_add aspell_string_map_add = nullptr;
PFUNC_aspell_string_map_remove aspell_string_map_remove = nullptr;
PFUNC_aspell_string_map_clear aspell_string_map_clear = nullptr;
PFUNC_aspell_string_map_to_mutable_container
aspell_string_map_to_mutable_container = nullptr;
PFUNC_delete_aspell_string_map delete_aspell_string_map = nullptr;
PFUNC_aspell_string_map_clone aspell_string_map_clone = nullptr;
PFUNC_aspell_string_map_assign aspell_string_map_assign = nullptr;
PFUNC_aspell_string_map_empty aspell_string_map_empty = nullptr;
PFUNC_aspell_string_map_size aspell_string_map_size = nullptr;
PFUNC_aspell_string_map_elements aspell_string_map_elements = nullptr;
PFUNC_aspell_string_map_insert aspell_string_map_insert = nullptr;
PFUNC_aspell_string_map_replace aspell_string_map_replace = nullptr;
PFUNC_aspell_string_map_lookup aspell_string_map_lookup = nullptr;
PFUNC_aspell_string_pair_enumeration_at_end
aspell_string_pair_enumeration_at_end = nullptr;
PFUNC_aspell_string_pair_enumeration_next aspell_string_pair_enumeration_next =
    nullptr;
PFUNC_delete_aspell_string_pair_enumeration
delete_aspell_string_pair_enumeration = nullptr;
PFUNC_aspell_string_pair_enumeration_clone
aspell_string_pair_enumeration_clone = nullptr;
PFUNC_aspell_string_pair_enumeration_assign
aspell_string_pair_enumeration_assign = nullptr;

// Qt port: the library of the system, where Windows looks in the registry and in Program Files
std::wstring get_default_aspell_path() {
  return L"libaspell.so.15";
}

std::wstring get_actual_aspell_path(const std::wstring &supposed_path) {
  if (supposed_path.empty()) {
    return get_default_aspell_path();
  }
  return supposed_path;

}

bool load_aspell(const wchar_t *path_arg) {
  auto path = get_actual_aspell_path(path_arg);
  h_inst_lib = dlopen(QFile::encodeName(QString::fromStdWString(path)).constData(), RTLD_NOW | RTLD_LOCAL); // Qt port: dlopen
  bool b_ret = false;

  if (h_inst_lib != nullptr) {
    aspell_mutable_container_add =
        reinterpret_cast<PFUNC_aspell_mutable_container_add>(dlsym(
            h_inst_lib, "aspell_mutable_container_add"));
    aspell_mutable_container_remove =
        reinterpret_cast<PFUNC_aspell_mutable_container_remove>(dlsym(
            h_inst_lib, "aspell_mutable_container_remove"));
    aspell_mutable_container_clear =
        reinterpret_cast<PFUNC_aspell_mutable_container_clear>(dlsym(
            h_inst_lib, "aspell_mutable_container_clear"));
    aspell_mutable_container_to_mutable_container =
        reinterpret_cast<PFUNC_aspell_mutable_container_to_mutable_container>(dlsym(
            h_inst_lib, "aspell_mutable_container_to_mutable_container"));
    aspell_key_info_enumeration_at_end =
        reinterpret_cast<PFUNC_aspell_key_info_enumeration_at_end>(dlsym(
            h_inst_lib, "aspell_key_info_enumeration_at_end"));
    aspell_key_info_enumeration_next =
        reinterpret_cast<PFUNC_aspell_key_info_enumeration_next>(dlsym(
            h_inst_lib, "aspell_key_info_enumeration_next"));
    delete_aspell_key_info_enumeration =
        reinterpret_cast<PFUNC_delete_aspell_key_info_enumeration>(dlsym(
            h_inst_lib, "delete_aspell_key_info_enumeration"));
    aspell_key_info_enumeration_clone =
        reinterpret_cast<PFUNC_aspell_key_info_enumeration_clone>(dlsym(
            h_inst_lib, "aspell_key_info_enumeration_clone"));
    aspell_key_info_enumeration_assign =
        reinterpret_cast<PFUNC_aspell_key_info_enumeration_assign>(dlsym(
            h_inst_lib, "aspell_key_info_enumeration_assign"));
    new_aspell_config =
        reinterpret_cast<PFUNC_new_aspell_config>(dlsym(h_inst_lib, "new_aspell_config"));
    delete_aspell_config = reinterpret_cast<PFUNC_delete_aspell_config>(dlsym(
        h_inst_lib, "delete_aspell_config"));
    aspell_config_clone = reinterpret_cast<PFUNC_aspell_config_clone>(dlsym(
        h_inst_lib, "aspell_config_clone"));
    aspell_config_assign = reinterpret_cast<PFUNC_aspell_config_assign>(dlsym(
        h_inst_lib, "aspell_config_assign"));
    aspell_config_error_number =
        reinterpret_cast<PFUNC_aspell_config_error_number>(dlsym(
            h_inst_lib, "aspell_config_error_number"));
    aspell_config_error_message =
        reinterpret_cast<PFUNC_aspell_config_error_message>(dlsym(
            h_inst_lib, "aspell_config_error_message"));
    aspell_config_error = reinterpret_cast<PFUNC_aspell_config_error>(dlsym(
        h_inst_lib, "aspell_config_error"));
    aspell_config_set_extra = reinterpret_cast<PFUNC_aspell_config_set_extra>(dlsym(
        h_inst_lib, "aspell_config_set_extra"));
    aspell_config_keyinfo = reinterpret_cast<PFUNC_aspell_config_keyinfo>(dlsym(
        h_inst_lib, "aspell_config_keyinfo"));
    aspell_config_possible_elements =
        reinterpret_cast<PFUNC_aspell_config_possible_elements>(dlsym(
            h_inst_lib, "aspell_config_possible_elements"));
    aspell_config_get_default = reinterpret_cast<PFUNC_aspell_config_get_default>(dlsym(
        h_inst_lib, "aspell_config_get_default"));
    aspell_config_elements = reinterpret_cast<PFUNC_aspell_config_elements>(dlsym(
        h_inst_lib, "aspell_config_elements"));
    aspell_config_replace = reinterpret_cast<PFUNC_aspell_config_replace>(dlsym(
        h_inst_lib, "aspell_config_replace"));
    aspell_config_remove = reinterpret_cast<PFUNC_aspell_config_remove>(dlsym(
        h_inst_lib, "aspell_config_remove"));
    aspell_config_have = reinterpret_cast<PFUNC_aspell_config_have>(dlsym(
        h_inst_lib, "aspell_config_have"));
    aspell_config_retrieve = reinterpret_cast<PFUNC_aspell_config_retrieve>(dlsym(
        h_inst_lib, "aspell_config_retrieve"));
    aspell_config_retrieve_list =
        reinterpret_cast<PFUNC_aspell_config_retrieve_list>(dlsym(
            h_inst_lib, "aspell_config_retrieve_list"));
    aspell_config_retrieve_bool =
        reinterpret_cast<PFUNC_aspell_config_retrieve_bool>(dlsym(
            h_inst_lib, "aspell_config_retrieve_bool"));
    aspell_config_retrieve_int =
        reinterpret_cast<PFUNC_aspell_config_retrieve_int>(dlsym(
            h_inst_lib, "aspell_config_retrieve_int"));
    aspell_error_number = reinterpret_cast<PFUNC_aspell_error_number>(dlsym(
        h_inst_lib, "aspell_error_number"));
    aspell_error_message = reinterpret_cast<PFUNC_aspell_error_message>(dlsym(
        h_inst_lib, "aspell_error_message"));
    aspell_error = reinterpret_cast<PFUNC_aspell_error>(dlsym(h_inst_lib, "aspell_error"));
    delete_aspell_can_have_error =
        reinterpret_cast<PFUNC_delete_aspell_can_have_error>(dlsym(
            h_inst_lib, "delete_aspell_can_have_error"));
    new_aspell_speller = reinterpret_cast<PFUNC_new_aspell_speller>(dlsym(
        h_inst_lib, "new_aspell_speller"));
    to_aspell_speller =
        reinterpret_cast<PFUNC_to_aspell_speller>(dlsym(h_inst_lib, "to_aspell_speller"));
    delete_aspell_speller = reinterpret_cast<PFUNC_delete_aspell_speller>(dlsym(
        h_inst_lib, "delete_aspell_speller"));
    aspell_speller_error_number =
        reinterpret_cast<PFUNC_aspell_speller_error_number>(dlsym(
            h_inst_lib, "aspell_speller_error_number"));
    aspell_speller_error_message =
        reinterpret_cast<PFUNC_aspell_speller_error_message>(dlsym(
            h_inst_lib, "aspell_speller_error_message"));
    aspell_speller_error = reinterpret_cast<PFUNC_aspell_speller_error>(dlsym(
        h_inst_lib, "aspell_speller_error"));
    aspell_speller_config = reinterpret_cast<PFUNC_aspell_speller_config>(dlsym(
        h_inst_lib, "aspell_speller_config"));
    aspell_speller_check = reinterpret_cast<PFUNC_aspell_speller_check>(dlsym(
        h_inst_lib, "aspell_speller_check"));
    aspell_speller_add_to_personal =
        reinterpret_cast<PFUNC_aspell_speller_add_to_personal>(dlsym(
            h_inst_lib, "aspell_speller_add_to_personal"));
    aspell_speller_add_to_session =
        reinterpret_cast<PFUNC_aspell_speller_add_to_session>(dlsym(
            h_inst_lib, "aspell_speller_add_to_session"));
    aspell_speller_personal_word_list =
        reinterpret_cast<PFUNC_aspell_speller_personal_word_list>(dlsym(
            h_inst_lib, "aspell_speller_personal_word_list"));
    aspell_speller_session_word_list =
        reinterpret_cast<PFUNC_aspell_speller_session_word_list>(dlsym(
            h_inst_lib, "aspell_speller_session_word_list"));
    aspell_speller_main_word_list =
        reinterpret_cast<PFUNC_aspell_speller_main_word_list>(dlsym(
            h_inst_lib, "aspell_speller_main_word_list"));
    aspell_speller_save_all_word_lists =
        reinterpret_cast<PFUNC_aspell_speller_save_all_word_lists>(dlsym(
            h_inst_lib, "aspell_speller_save_all_word_lists"));
    aspell_speller_clear_session =
        reinterpret_cast<PFUNC_aspell_speller_clear_session>(dlsym(
            h_inst_lib, "aspell_speller_clear_session"));
    aspell_speller_suggest = reinterpret_cast<PFUNC_aspell_speller_suggest>(dlsym(
        h_inst_lib, "aspell_speller_suggest"));
    aspell_speller_store_replacement =
        reinterpret_cast<PFUNC_aspell_speller_store_replacement>(dlsym(
            h_inst_lib, "aspell_speller_store_replacement"));
    delete_aspell_filter = reinterpret_cast<PFUNC_delete_aspell_filter>(dlsym(
        h_inst_lib, "delete_aspell_filter"));
    aspell_filter_error_number =
        reinterpret_cast<PFUNC_aspell_filter_error_number>(dlsym(
            h_inst_lib, "aspell_filter_error_number"));
    aspell_filter_error_message =
        reinterpret_cast<PFUNC_aspell_filter_error_message>(dlsym(
            h_inst_lib, "aspell_filter_error_message"));
    aspell_filter_error = reinterpret_cast<PFUNC_aspell_filter_error>(dlsym(
        h_inst_lib, "aspell_filter_error"));
    to_aspell_filter =
        reinterpret_cast<PFUNC_to_aspell_filter>(dlsym(h_inst_lib, "to_aspell_filter"));
    delete_aspell_document_checker =
        reinterpret_cast<PFUNC_delete_aspell_document_checker>(dlsym(
            h_inst_lib, "delete_aspell_document_checker"));
    aspell_document_checker_error_number =
        reinterpret_cast<PFUNC_aspell_document_checker_error_number>(dlsym(
            h_inst_lib, "aspell_document_checker_error_number"));
    aspell_document_checker_error_message =
        reinterpret_cast<PFUNC_aspell_document_checker_error_message>(dlsym(
            h_inst_lib, "aspell_document_checker_error_message"));
    aspell_document_checker_error =
        reinterpret_cast<PFUNC_aspell_document_checker_error>(dlsym(
            h_inst_lib, "aspell_document_checker_error"));
    new_aspell_document_checker =
        reinterpret_cast<PFUNC_new_aspell_document_checker>(dlsym(
            h_inst_lib, "new_aspell_document_checker"));
    to_aspell_document_checker =
        reinterpret_cast<PFUNC_to_aspell_document_checker>(dlsym(
            h_inst_lib, "to_aspell_document_checker"));
    aspell_document_checker_reset =
        reinterpret_cast<PFUNC_aspell_document_checker_reset>(dlsym(
            h_inst_lib, "aspell_document_checker_reset"));
    aspell_document_checker_process =
        reinterpret_cast<PFUNC_aspell_document_checker_process>(dlsym(
            h_inst_lib, "aspell_document_checker_process"));
    aspell_document_checker_next_misspelling =
        reinterpret_cast<PFUNC_aspell_document_checker_next_misspelling>(dlsym(
            h_inst_lib, "aspell_document_checker_next_misspelling"));
    aspell_document_checker_filter =
        reinterpret_cast<PFUNC_aspell_document_checker_filter>(dlsym(
            h_inst_lib, "aspell_document_checker_filter"));
    aspell_word_list_empty = reinterpret_cast<PFUNC_aspell_word_list_empty>(dlsym(
        h_inst_lib, "aspell_word_list_empty"));
    aspell_word_list_size = reinterpret_cast<PFUNC_aspell_word_list_size>(dlsym(
        h_inst_lib, "aspell_word_list_size"));
    aspell_word_list_elements = reinterpret_cast<PFUNC_aspell_word_list_elements>(dlsym(
        h_inst_lib, "aspell_word_list_elements"));
    delete_aspell_string_enumeration =
        reinterpret_cast<PFUNC_delete_aspell_string_enumeration>(dlsym(
            h_inst_lib, "delete_aspell_string_enumeration"));
    aspell_string_enumeration_clone =
        reinterpret_cast<PFUNC_aspell_string_enumeration_clone>(dlsym(
            h_inst_lib, "aspell_string_enumeration_clone"));
    aspell_string_enumeration_assign =
        reinterpret_cast<PFUNC_aspell_string_enumeration_assign>(dlsym(
            h_inst_lib, "aspell_string_enumeration_assign"));
    aspell_string_enumeration_at_end =
        reinterpret_cast<PFUNC_aspell_string_enumeration_at_end>(dlsym(
            h_inst_lib, "aspell_string_enumeration_at_end"));
    aspell_string_enumeration_next =
        reinterpret_cast<PFUNC_aspell_string_enumeration_next>(dlsym(
            h_inst_lib, "aspell_string_enumeration_next"));
    get_aspell_module_info_list =
        reinterpret_cast<PFUNC_get_aspell_module_info_list>(dlsym(
            h_inst_lib, "get_aspell_module_info_list"));
    aspell_module_info_list_empty =
        reinterpret_cast<PFUNC_aspell_module_info_list_empty>(dlsym(
            h_inst_lib, "aspell_module_info_list_empty"));
    aspell_module_info_list_size =
        reinterpret_cast<PFUNC_aspell_module_info_list_size>(dlsym(
            h_inst_lib, "aspell_module_info_list_size"));
    aspell_module_info_list_elements =
        reinterpret_cast<PFUNC_aspell_module_info_list_elements>(dlsym(
            h_inst_lib, "aspell_module_info_list_elements"));
    get_aspell_dict_info_list = reinterpret_cast<PFUNC_get_aspell_dict_info_list>(dlsym(
        h_inst_lib, "get_aspell_dict_info_list"));
    aspell_dict_info_list_empty =
        reinterpret_cast<PFUNC_aspell_dict_info_list_empty>(dlsym(
            h_inst_lib, "aspell_dict_info_list_empty"));
    aspell_dict_info_list_size =
        reinterpret_cast<PFUNC_aspell_dict_info_list_size>(dlsym(
            h_inst_lib, "aspell_dict_info_list_size"));
    aspell_dict_info_list_elements =
        reinterpret_cast<PFUNC_aspell_dict_info_list_elements>(dlsym(
            h_inst_lib, "aspell_dict_info_list_elements"));
    aspell_module_info_enumeration_at_end =
        reinterpret_cast<PFUNC_aspell_module_info_enumeration_at_end>(dlsym(
            h_inst_lib, "aspell_module_info_enumeration_at_end"));
    aspell_module_info_enumeration_next =
        reinterpret_cast<PFUNC_aspell_module_info_enumeration_next>(dlsym(
            h_inst_lib, "aspell_module_info_enumeration_next"));
    delete_aspell_module_info_enumeration =
        reinterpret_cast<PFUNC_delete_aspell_module_info_enumeration>(dlsym(
            h_inst_lib, "delete_aspell_module_info_enumeration"));
    aspell_module_info_enumeration_clone =
        reinterpret_cast<PFUNC_aspell_module_info_enumeration_clone>(dlsym(
            h_inst_lib, "aspell_module_info_enumeration_clone"));
    aspell_module_info_enumeration_assign =
        reinterpret_cast<PFUNC_aspell_module_info_enumeration_assign>(dlsym(
            h_inst_lib, "aspell_module_info_enumeration_assign"));
    aspell_dict_info_enumeration_at_end =
        reinterpret_cast<PFUNC_aspell_dict_info_enumeration_at_end>(dlsym(
            h_inst_lib, "aspell_dict_info_enumeration_at_end"));
    aspell_dict_info_enumeration_next =
        reinterpret_cast<PFUNC_aspell_dict_info_enumeration_next>(dlsym(
            h_inst_lib, "aspell_dict_info_enumeration_next"));
    delete_aspell_dict_info_enumeration =
        reinterpret_cast<PFUNC_delete_aspell_dict_info_enumeration>(dlsym(
            h_inst_lib, "delete_aspell_dict_info_enumeration"));
    aspell_dict_info_enumeration_clone =
        reinterpret_cast<PFUNC_aspell_dict_info_enumeration_clone>(dlsym(
            h_inst_lib, "aspell_dict_info_enumeration_clone"));
    aspell_dict_info_enumeration_assign =
        reinterpret_cast<PFUNC_aspell_dict_info_enumeration_assign>(dlsym(
            h_inst_lib, "aspell_dict_info_enumeration_assign"));
    new_aspell_string_list = reinterpret_cast<PFUNC_new_aspell_string_list>(dlsym(
        h_inst_lib, "new_aspell_string_list"));
    aspell_string_list_empty = reinterpret_cast<PFUNC_aspell_string_list_empty>(dlsym(
        h_inst_lib, "aspell_string_list_empty"));
    aspell_string_list_size = reinterpret_cast<PFUNC_aspell_string_list_size>(dlsym(
        h_inst_lib, "aspell_string_list_size"));
    aspell_string_list_elements =
        reinterpret_cast<PFUNC_aspell_string_list_elements>(dlsym(
            h_inst_lib, "aspell_string_list_elements"));
    aspell_string_list_add = reinterpret_cast<PFUNC_aspell_string_list_add>(dlsym(
        h_inst_lib, "aspell_string_list_add"));
    aspell_string_list_remove = reinterpret_cast<PFUNC_aspell_string_list_remove>(dlsym(
        h_inst_lib, "aspell_string_list_remove"));
    aspell_string_list_clear = reinterpret_cast<PFUNC_aspell_string_list_clear>(dlsym(
        h_inst_lib, "aspell_string_list_clear"));
    aspell_string_list_to_mutable_container =
        reinterpret_cast<PFUNC_aspell_string_list_to_mutable_container>(dlsym(
            h_inst_lib, "aspell_string_list_to_mutable_container"));
    delete_aspell_string_list = reinterpret_cast<PFUNC_delete_aspell_string_list>(dlsym(
        h_inst_lib, "delete_aspell_string_list"));
    aspell_string_list_clone = reinterpret_cast<PFUNC_aspell_string_list_clone>(dlsym(
        h_inst_lib, "aspell_string_list_clone"));
    aspell_string_list_assign = reinterpret_cast<PFUNC_aspell_string_list_assign>(dlsym(
        h_inst_lib, "aspell_string_list_assign"));
    new_aspell_string_map = reinterpret_cast<PFUNC_new_aspell_string_map>(dlsym(
        h_inst_lib, "new_aspell_string_map"));
    aspell_string_map_add = reinterpret_cast<PFUNC_aspell_string_map_add>(dlsym(
        h_inst_lib, "aspell_string_map_add"));
    aspell_string_map_remove = reinterpret_cast<PFUNC_aspell_string_map_remove>(dlsym(
        h_inst_lib, "aspell_string_map_remove"));
    aspell_string_map_clear = reinterpret_cast<PFUNC_aspell_string_map_clear>(dlsym(
        h_inst_lib, "aspell_string_map_clear"));
    aspell_string_map_to_mutable_container =
        reinterpret_cast<PFUNC_aspell_string_map_to_mutable_container>(dlsym(
            h_inst_lib, "aspell_string_map_to_mutable_container"));
    delete_aspell_string_map = reinterpret_cast<PFUNC_delete_aspell_string_map>(dlsym(
        h_inst_lib, "delete_aspell_string_map"));
    aspell_string_map_clone = reinterpret_cast<PFUNC_aspell_string_map_clone>(dlsym(
        h_inst_lib, "aspell_string_map_clone"));
    aspell_string_map_assign = reinterpret_cast<PFUNC_aspell_string_map_assign>(dlsym(
        h_inst_lib, "aspell_string_map_assign"));
    aspell_string_map_empty = reinterpret_cast<PFUNC_aspell_string_map_empty>(dlsym(
        h_inst_lib, "aspell_string_map_empty"));
    aspell_string_map_size = reinterpret_cast<PFUNC_aspell_string_map_size>(dlsym(
        h_inst_lib, "aspell_string_map_size"));
    aspell_string_map_elements =
        reinterpret_cast<PFUNC_aspell_string_map_elements>(dlsym(
            h_inst_lib, "aspell_string_map_elements"));
    aspell_string_map_insert = reinterpret_cast<PFUNC_aspell_string_map_insert>(dlsym(
        h_inst_lib, "aspell_string_map_insert"));
    aspell_string_map_replace = reinterpret_cast<PFUNC_aspell_string_map_replace>(dlsym(
        h_inst_lib, "aspell_string_map_replace"));
    aspell_string_map_lookup = reinterpret_cast<PFUNC_aspell_string_map_lookup>(dlsym(
        h_inst_lib, "aspell_string_map_lookup"));
    aspell_string_pair_enumeration_at_end =
        reinterpret_cast<PFUNC_aspell_string_pair_enumeration_at_end>(dlsym(
            h_inst_lib, "aspell_string_pair_enumeration_at_end"));
    aspell_string_pair_enumeration_next =
        reinterpret_cast<PFUNC_aspell_string_pair_enumeration_next>(dlsym(
            h_inst_lib, "aspell_string_pair_enumeration_next"));
    delete_aspell_string_pair_enumeration =
        reinterpret_cast<PFUNC_delete_aspell_string_pair_enumeration>(dlsym(
            h_inst_lib, "delete_aspell_string_pair_enumeration"));
    aspell_string_pair_enumeration_clone =
        reinterpret_cast<PFUNC_aspell_string_pair_enumeration_clone>(dlsym(
            h_inst_lib, "aspell_string_pair_enumeration_clone"));
    aspell_string_pair_enumeration_assign =
        reinterpret_cast<PFUNC_aspell_string_pair_enumeration_assign>(dlsym(
            h_inst_lib, "aspell_string_pair_enumeration_assign"));

    if (new_aspell_config == nullptr) // TODO: Add check for all used functions
      return false;

    b_ret = true;
  }

  return b_ret;
}

void unload_aspell() {
  if (h_inst_lib != nullptr)
    dlclose(h_inst_lib); // Qt port: dlclose
}

void aspell_error_msg_box(HWND h_wnd, LPCSTR sz_error_msg) {
  // Qt port: the message is in the encoding of the system, UTF-8 here
  const std::wstring sz_msg = L"Error:\n" + QString::fromUtf8(sz_error_msg).toStdWString();
  ::MessageBox(h_wnd, sz_msg.c_str(), L"GNU Aspell", MB_OK);
}
