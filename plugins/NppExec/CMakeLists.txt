# NppExec 0.8.12.1 by DV: https://github.com/d0vgan/nppexec at 7a5d279
#
# Qt port: the console and the dialogs are Qt widgets behind the interfaces of the original's window classes,
# child processes run through POSIX pipes or a pseudo terminal, and the help is the HTML manual of docs/.

file(GLOB NPPEXEC_SOURCES CONFIGURE_DEPENDS
	src/*.cpp src/*.h src/c_base/*.c src/c_base/*.h src/cpp/*.cpp src/cpp/*.h src/encodings/*.cpp src/encodings/*.h
	src/PluginCommunication/*.cpp src/PluginCommunication/*.h src/qtport/*.cpp src/qtport/*.h)
list(APPEND NPPEXEC_SOURCES src/fparser/fparser.cc)
# The C helpers use the same headers as the C++ code, as wide-character code with TCHAR
file(GLOB NPPEXEC_C_SOURCES CONFIGURE_DEPENDS src/c_base/*.c)
set_source_files_properties(${NPPEXEC_C_SOURCES} PROPERTIES LANGUAGE CXX)

# Without compat.cpp, which the plugin's module has of its own
add_library(NppExecStatic STATIC ${NPPEXEC_SOURCES})
npp_plugin_settings(NppExecStatic)
target_include_directories(NppExecStatic PUBLIC src)
# What NppExec uses of <windows.h> beyond the plugins' compat/windows.h, which every file of it has as on Windows
target_compile_options(NppExecStatic PUBLIC "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/src/qtport/NppExecWin.h")
target_link_libraries(NppExecStatic PUBLIC Qt6::Widgets ${CMAKE_DL_LIBS} util)

npp_add_plugin(NppExec src/qtport/PluginMain.cpp NppExec.qrc)
set_target_properties(NppExec PROPERTIES AUTORCC ON)
# Whole, as nothing in the module calls the exports of NppExec.cpp
target_link_libraries(NppExec PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,NppExecStatic>")

# The manual and documents, where NppExec's Help menu looks for them: doc/NppExec beside the plugin
add_custom_command(TARGET NppExec POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/docs ${NPP_PLUGINS_OUTPUT_DIR}/NppExec/doc/NppExec)

# Upstream's unit tests of the helpers and the warning masks' matcher, with the paths of Linux where those of
# Windows differ
if(NPP_BUILD_TESTS)
	file(GLOB NPPEXEC_UNIT_TESTS CONFIGURE_DEPENDS UnitTests/*.cpp)
	add_executable(nppexecUnitTest ${NPPEXEC_UNIT_TESTS} ${NPP_PLUGINS_DIR}/compat/compat.cpp)
	npp_plugin_settings(nppexecUnitTest)
	target_compile_options(nppexecUnitTest PRIVATE -UNDEBUG) # the tests are asserts
	target_link_libraries(nppexecUnitTest PRIVATE NppExecStatic)
	add_test(NAME nppexecUnit COMMAND nppexecUnitTest)
	set_tests_properties(nppexecUnit PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120)
endif()

# The headers whose constants NppExec's scripts may name, such as SCI_GETLENGTH, read at run time from NppExec/
# beside the plugin, as in the Windows package: Notepad++'s and Scintilla's of this build, and NppExec's BaseDef.h
add_custom_command(TARGET NppExec POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory ${NPP_PLUGINS_OUTPUT_DIR}/NppExec/NppExec
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${NPP_SOURCE_DIR}/PowerEditor/src/MISC/PluginsManager/Notepad_plus_msgs.h
		${NPP_SOURCE_DIR}/PowerEditor/src/menuCmdID.h
		${NPP_SOURCE_DIR}/scintilla/include/Scintilla.h
		${CMAKE_CURRENT_SOURCE_DIR}/runtime/BaseDef.h
		${NPP_PLUGINS_OUTPUT_DIR}/NppExec/NppExec)

install(DIRECTORY docs/ DESTINATION ${NPP_INSTALL_PLUGINSDIR}/NppExec/doc/NppExec COMPONENT Plugins)
install(FILES
		${NPP_SOURCE_DIR}/PowerEditor/src/MISC/PluginsManager/Notepad_plus_msgs.h
		${NPP_SOURCE_DIR}/PowerEditor/src/menuCmdID.h
		${NPP_SOURCE_DIR}/scintilla/include/Scintilla.h
		runtime/BaseDef.h
	DESTINATION ${NPP_INSTALL_PLUGINSDIR}/NppExec/NppExec COMPONENT Plugins)
