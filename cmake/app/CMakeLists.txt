# The Notepad++ application.
set(SRC ${CMAKE_SOURCE_DIR}/PowerEditor/src)

file(GLOB UCHARDET CONFIGURE_DEPENDS ${SRC}/uchardet/*.cpp)
file(GLOB_RECURSE FORMS CONFIGURE_DEPENDS ${SRC}/*.ui)

set(FORM_DIRS)
foreach(form ${FORMS})
	get_filename_component(dir ${form} DIRECTORY)
	list(APPEND FORM_DIRS ${dir})
endforeach()
list(REMOVE_DUPLICATES FORM_DIRS)

qt_add_library(nppapp OBJECT
	${UCHARDET}
	${SRC}/Notepad_plus.cpp
	${SRC}/Notepad_plus_Window.cpp
	${SRC}/NppBigSwitch.cpp
	${SRC}/NppCommands.cpp
	${SRC}/NppNotification.cpp
	${SRC}/NppIO.cpp
	${SRC}/NppDarkMode.cpp
	${SRC}/Parameters.cpp
	${SRC}/localization.cpp
	${SRC}/lastRecentFileList.cpp
	${SRC}/lesDlgs.cpp
	${SRC}/EncodingMapper.cpp
	${SRC}/Utf8_16.cpp
	${SRC}/NppQt/NppControls.cpp
	${SRC}/NppQt/NppInstanceLink.cpp
	${SRC}/NppQt/NppMenu.cpp
	${SRC}/NppQt/NppMessageFilter.cpp
	${SRC}/NppQt/NppMessages.cpp
	${SRC}/generated/NppResources_generated.cpp
	${SRC}/generated/NppResources.qrc
	${SRC}/pugixml/pugixml.cpp
	${SRC}/MISC/Common/Common.cpp
	${SRC}/MISC/Common/ConfigSettings.cpp
	${SRC}/MISC/Common/FileInterface.cpp
	${SRC}/MISC/Common/NppElevation.cpp
	${SRC}/MISC/Common/SortLocale.cpp
	${SRC}/MISC/PluginsManager/PluginsManager.cpp
	${SRC}/MISC/Process/Processus.cpp
	${SRC}/MISC/sha1/calc_sha1.cpp
	${SRC}/MISC/sha1/sha1.cpp
	${SRC}/MISC/sha2/sha-256.cpp
	${SRC}/MISC/sha512/sha512.cpp
	${SRC}/MISC/md5/md5Dlgs.cpp
	${SRC}/ScintillaComponent/AutoCompletion.cpp
	${SRC}/ScintillaComponent/Buffer.cpp
	${SRC}/ScintillaComponent/DocTabView.cpp
	${SRC}/ScintillaComponent/FindReplaceDlg.cpp
	${SRC}/ScintillaComponent/GoToLineDlg.cpp
	${SRC}/ScintillaComponent/columnEditor.cpp
	${SRC}/ScintillaComponent/FunctionCallTip.cpp
	${SRC}/ScintillaComponent/LanguageNames.cpp
	${SRC}/ScintillaComponent/Printer.cpp
	${SRC}/ScintillaComponent/ScintillaCtrls.cpp
	${SRC}/ScintillaComponent/ScintillaEditView.cpp
	${SRC}/ScintillaComponent/UserDefineDialog.cpp
	${SRC}/ScintillaComponent/SmartHighlighter.cpp
	${SRC}/ScintillaComponent/xmlMatchedTagsHighlighter.cpp
	${SRC}/WinControls/ColourPicker/ColourPicker.cpp
	${SRC}/WinControls/ColourPicker/ColourPopup.cpp
	${SRC}/WinControls/ColourPicker/WordStyleDlg.cpp
	${SRC}/WinControls/FileBrowser/fileBrowser.cpp
	${SRC}/WinControls/FunctionList/functionListPanel.cpp
	${SRC}/WinControls/Grid/ShortcutMapper.cpp
	${SRC}/WinControls/Preference/preferenceDlg.cpp
	${SRC}/WinControls/ProjectPanel/ProjectPanel.cpp
	${SRC}/WinControls/WindowsDlg/WindowsDlg.cpp
	${SRC}/WinControls/FunctionList/functionParser.cpp
	${SRC}/WinControls/TreeView/TreeView.cpp
	${SRC}/WinControls/AboutDlg/AboutDlg.cpp
	${SRC}/WinControls/AboutDlg/URLCtrl.cpp
	${SRC}/WinControls/AnsiCharPanel/ListView.cpp
	${SRC}/WinControls/AnsiCharPanel/ansiCharPanel.cpp
	${SRC}/WinControls/AnsiCharPanel/asciiListView.cpp
	${SRC}/WinControls/ClipboardHistory/clipboardHistoryPanel.cpp
	${SRC}/WinControls/ContextMenu/ContextMenu.cpp
	${SRC}/WinControls/DockingWnd/DockingManager.cpp
	${SRC}/WinControls/DocumentMap/documentMap.cpp
	${SRC}/WinControls/DocumentMap/documentSnapshot.cpp
	${SRC}/WinControls/FindCharsInRange/FindCharsInRange.cpp
	${SRC}/WinControls/ImageListSet/IconList.cpp
	${SRC}/WinControls/OpenSaveFileDialog/CustomFileDialog.cpp
	${SRC}/WinControls/SplitterContainer/SplitterContainer.cpp
	${SRC}/WinControls/StaticDialog/RunDlg/RunDlg.cpp
	${SRC}/WinControls/StaticDialog/StaticDialog.cpp
	${SRC}/WinControls/StaticDialog/CommandDialog.cpp
	${SRC}/WinControls/StatusBar/StatusBar.cpp
	${SRC}/WinControls/TabBar/TabBar.cpp
	${SRC}/WinControls/TaskList/TaskListDlg.cpp
	${SRC}/WinControls/ToolBar/ToolBar.cpp
	${SRC}/WinControls/TrayIcon/trayIconControler.cpp
	${SRC}/WinControls/VerticalFileSwitcher/VerticalFileSwitcher.cpp
	${SRC}/WinControls/VerticalFileSwitcher/VerticalFileSwitcherListView.cpp
	${SRC}/WinControls/shortcut/RunMacroDlg.cpp
	${SRC}/WinControls/shortcut/shortcut.cpp
)

set_target_properties(nppapp PROPERTIES
	AUTOMOC ON
	AUTOUIC ON
	AUTORCC ON
	AUTOUIC_SEARCH_PATHS "${FORM_DIRS}"
)

# Stand-ins for classes not ported yet come first, so that they hide the original headers
set(SRC_DIRS)
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS ${SRC}/*.h)
foreach(header ${HEADERS})
	get_filename_component(dir ${header} DIRECTORY)
	if(NOT dir MATCHES "/json")
		list(APPEND SRC_DIRS ${dir})
	endif()
endforeach()
list(REMOVE_DUPLICATES SRC_DIRS)
target_include_directories(nppapp PUBLIC ${SRC}/NppQt ${SRC} ${SRC}/pugixml ${SRC}/json ${SRC_DIRS})

target_link_libraries(nppapp PUBLIC scintilla lexilla Qt6::Widgets Qt6::Core5Compat Qt6::Network Qt6::PrintSupport)

# Library versions for the debug information dialog
file(READ ${CMAKE_SOURCE_DIR}/scintilla/version.txt SCI_VERSION)
file(READ ${CMAKE_SOURCE_DIR}/lexilla/version.txt LEX_VERSION)
string(STRIP "${SCI_VERSION}" SCI_VERSION)
string(STRIP "${LEX_VERSION}" LEX_VERSION)
string(REGEX REPLACE "^(.)(.)(.*)$" "\\1.\\2.\\3" SCI_VERSION "${SCI_VERSION}")
string(REGEX REPLACE "^(.)(.)(.*)$" "\\1.\\2.\\3" LEX_VERSION "${LEX_VERSION}")
file(STRINGS ${CMAKE_SOURCE_DIR}/boostregex/boost/version.hpp BOOST_LINE REGEX "define BOOST_LIB_VERSION")
string(REGEX REPLACE ".*\"(.*)\".*" "\\1" BOOST_VERSION "${BOOST_LINE}")
string(REPLACE "_" "." BOOST_VERSION "${BOOST_VERSION}")
target_compile_definitions(nppapp PRIVATE
	NPP_SCINTILLA_VERSION="${SCI_VERSION}"
	NPP_LEXILLA_VERSION="${LEX_VERSION}"
	NPP_BOOST_REGEX_VERSION="${BOOST_VERSION}"
)

qt_add_executable(notepad++ ${SRC}/winmain.cpp)
# Plugins are libraries that find SendMessage() and the rest of the host's API in the executable
set_target_properties(notepad++ PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
	ENABLE_EXPORTS ON
)
target_link_libraries(notepad++ PRIVATE nppapp)

# The data files beside the executable, as Parameters looks for them there when running from the build directory
set(INSTALLER ${CMAKE_SOURCE_DIR}/PowerEditor/installer)
add_custom_command(TARGET notepad++ POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SRC}/langs.model.xml ${SRC}/stylers.model.xml $<TARGET_FILE_DIR:notepad++>
	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${INSTALLER}/nativeLang $<TARGET_FILE_DIR:notepad++>/localization
	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${INSTALLER}/themes $<TARGET_FILE_DIR:notepad++>/themes
	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${INSTALLER}/functionList $<TARGET_FILE_DIR:notepad++>/functionList
	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${INSTALLER}/APIs $<TARGET_FILE_DIR:notepad++>/autoCompletion
	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_SOURCE_DIR}/PowerEditor/bin/userDefineLangs $<TARGET_FILE_DIR:notepad++>/userDefineLangs
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SRC}/tabContextMenu_example.xml ${SRC}/toolbarButtonsConf_example.xml $<TARGET_FILE_DIR:notepad++>
	VERBATIM
)

# Installing. Parameters finds the data from the program, beside it or in the data folder, and the plugins in the
# plugins folder, by these paths relative to the program's folder, so that the installation can move.
foreach(dir DATADIR PLUGINSDIR)
	cmake_path(ABSOLUTE_PATH NPP_INSTALL_${dir} BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}" NORMALIZE OUTPUT_VARIABLE npp_full_${dir})
endforeach()
cmake_path(ABSOLUTE_PATH NPP_INSTALL_BINDIR BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}" NORMALIZE OUTPUT_VARIABLE npp_full_BINDIR)
file(RELATIVE_PATH npp_data_from_bin "${npp_full_BINDIR}" "${npp_full_DATADIR}")
file(RELATIVE_PATH npp_plugins_from_bin "${npp_full_BINDIR}" "${npp_full_PLUGINSDIR}")
foreach(path npp_data_from_bin npp_plugins_from_bin)
	if(${path} STREQUAL "")
		set(${path} .)
	endif()
endforeach()
target_compile_definitions(nppapp PRIVATE
	NPP_INSTALLED_DATA_DIR="${npp_data_from_bin}"
	NPP_INSTALLED_PLUGINS_DIR="${npp_plugins_from_bin}"
	NPP_SYSTEM_PLUGINS_DIR="${NPP_SYSTEM_PLUGINS_DIR}"
)

# A Windows program, without a console
set_target_properties(notepad++ PROPERTIES WIN32_EXECUTABLE ON)

install(TARGETS notepad++ RUNTIME DESTINATION ${NPP_INSTALL_BINDIR} COMPONENT Application)
install(FILES
		${SRC}/langs.model.xml
		${SRC}/stylers.model.xml
		${SRC}/tabContextMenu_example.xml
		${SRC}/toolbarButtonsConf_example.xml
	DESTINATION ${NPP_INSTALL_DATADIR} COMPONENT Application)
install(DIRECTORY ${INSTALLER}/nativeLang/ DESTINATION ${NPP_INSTALL_DATADIR}/localization COMPONENT Application)
install(DIRECTORY ${INSTALLER}/themes/ DESTINATION ${NPP_INSTALL_DATADIR}/themes COMPONENT Application)
install(DIRECTORY ${INSTALLER}/functionList/ DESTINATION ${NPP_INSTALL_DATADIR}/functionList COMPONENT Application)
install(DIRECTORY ${INSTALLER}/APIs/ DESTINATION ${NPP_INSTALL_DATADIR}/autoCompletion COMPONENT Application)
# Copied into the user's folder on the first run, as the Windows installer does
install(DIRECTORY ${CMAKE_SOURCE_DIR}/PowerEditor/bin/userDefineLangs/ DESTINATION ${NPP_INSTALL_DATADIR}/userDefineLangs
	COMPONENT Application)

# The desktop file, so that Notepad++ is in the applications' menus and opens files, and its icons
if(NPP_INSTALL_DESKTOP_INTEGRATION)
	set(LINUX ${CMAKE_SOURCE_DIR}/PowerEditor/installer/linux)
	get_target_property(NPP_EXECUTABLE notepad++ OUTPUT_NAME)
	if(NOT NPP_EXECUTABLE)
		set(NPP_EXECUTABLE notepad++)
	endif()
	configure_file(${LINUX}/notepad-plus-plus.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/notepad-plus-plus.desktop @ONLY)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/notepad-plus-plus.desktop
		DESTINATION ${CMAKE_INSTALL_DATADIR}/applications COMPONENT Application)
	install(DIRECTORY ${LINUX}/icons/ DESTINATION ${CMAKE_INSTALL_DATADIR}/icons COMPONENT Application)

	# The caches of desktop files and icons are updated, unless the files are staged for a package, whose
	# installation updates them
	install(CODE "set(npp_install_datadir \"${CMAKE_INSTALL_DATADIR}\")" COMPONENT Application)
	install(SCRIPT ${LINUX}/update-caches.cmake COMPONENT Application)

	if(NPP_BUILD_TESTS)
		find_program(DESKTOP_FILE_VALIDATE desktop-file-validate)
		if(DESKTOP_FILE_VALIDATE)
			add_test(NAME desktopFile COMMAND ${DESKTOP_FILE_VALIDATE} ${CMAKE_CURRENT_BINARY_DIR}/notepad-plus-plus.desktop)
		endif()
	endif()
endif()

# windeployqt, run on the installed program, copies the Qt libraries, plugins and translations it needs beside it,
# with a qt.conf that finds them there. Qt's plugins go in qt/plugins, as plugins is for those of Notepad++. The qt.conf
# goes in QT_DEPLOY_BIN_DIR, which is bin unless set.
if(NPP_DEPLOY_QT)
	qt_generate_deploy_script(TARGET notepad++ OUTPUT_SCRIPT npp_deploy_script CONTENT "
set(QT_DEPLOY_BIN_DIR \"${NPP_INSTALL_BINDIR}\")
set(QT_DEPLOY_PLUGINS_DIR qt/plugins)
qt_deploy_runtime_dependencies(
	EXECUTABLE \"${NPP_INSTALL_BINDIR}/$<TARGET_FILE_NAME:notepad++>\"
	GENERATE_QT_CONF
	VERBOSE)
")
	install(SCRIPT "${npp_deploy_script}" COMPONENT Application)
endif()
