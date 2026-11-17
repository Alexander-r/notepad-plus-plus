# The plugins that Notepad++ for Windows comes with, built for the Qt port. See README.md.
#
# Inside the Notepad++ tree this is part of its build, and puts the plugins beside the program in bin/plugins.
# On its own it needs the Notepad++ source for the plugin headers:
#
#   cmake -S plugins -B build-plugins -DNPP_SOURCE_DIR=/path/to/notepad-plus-plus
#   cmake --build build-plugins -j
#   cmake --install build-plugins --prefix /usr     # to lib/notepad++/plugins, where Notepad++ also looks

cmake_minimum_required(VERSION 3.21)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR) # on its own
	project(NotepadPlusPlusPlugins LANGUAGES C CXX)
	set(CMAKE_CXX_STANDARD 20)
	set(CMAKE_CXX_STANDARD_REQUIRED ON)
	set(CMAKE_CXX_EXTENSIONS OFF)
	if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
		set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
	endif()
	find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
	set(NPP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "The Notepad++ source, for the plugin headers")
	set(NPP_PLUGINS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/plugins" CACHE PATH "Where the plugins go, each in a folder of its name")
	option(NPP_BUILD_TESTS "Build the plugins' unit tests" ON)
	if(NPP_BUILD_TESTS)
		enable_testing()
	endif()
	# Where Notepad++ looks for the plugins of the system's packages
	include(GNUInstallDirs)
	if(WIN32)
		set(NPP_INSTALL_PLUGINSDIR plugins)
	else()
		set(NPP_INSTALL_PLUGINSDIR ${CMAKE_INSTALL_LIBDIR}/notepad++/plugins)
	endif()
else()
	set(NPP_SOURCE_DIR "${CMAKE_SOURCE_DIR}")
	set(NPP_PLUGINS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/plugins")
endif()

set(NPP_PLUGINS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# What a plugin may use of Notepad++: the plugin interface, the messages, the docking data and Scintilla's API.
# These headers are all of it, and are what a plugin SDK would hold.
set(NPP_PLUGIN_INCLUDE_DIRS
	${NPP_SOURCE_DIR}/PowerEditor/src/MISC/PluginsManager # PluginInterface.h, Notepad_plus_msgs.h
	${NPP_SOURCE_DIR}/PowerEditor/src/NppQt # NppMessages.h: SendMessage and HWND
	${NPP_SOURCE_DIR}/PowerEditor/src/MISC/Common # NppPlatform.h
	${NPP_SOURCE_DIR}/PowerEditor/src/WinControls/DockingWnd # Docking.h, dockingResource.h
	${NPP_SOURCE_DIR}/PowerEditor/src # menuCmdID.h
	${NPP_SOURCE_DIR}/scintilla/include # Scintilla.h, Sci_Position.h
	${NPP_SOURCE_DIR}/lexilla/include) # SciLexer.h

# The compile settings of plugin code, for a plugin and for the libraries and tests of its code
function(npp_plugin_settings target)
	set_target_properties(${target} PROPERTIES
		C_VISIBILITY_PRESET hidden # only the exports are seen, as with a DLL
		CXX_VISIBILITY_PRESET hidden
		VISIBILITY_INLINES_HIDDEN ON
		POSITION_INDEPENDENT_CODE ON
		AUTOMOC ON)
	target_include_directories(${target} BEFORE PRIVATE ${NPP_PLUGINS_DIR}/compat)
	target_include_directories(${target} PRIVATE ${NPP_PLUGIN_INCLUDE_DIRS})
	target_compile_definitions(${target} PRIVATE SCINTILLA_QT=1 UNICODE _UNICODE)
	# On Windows the plugin headers bring <windows.h> in, which plugin code counts on
	target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-include windows.h>)
	target_link_libraries(${target} PRIVATE Qt6::Widgets ${CMAKE_DL_LIBS})
endfunction()

# All the plugins
add_custom_target(npp_plugins)

# A plugin as Notepad++ looks for it: plugins/Name/Name.so, built and installed
function(npp_add_plugin name)
	add_library(${name} MODULE ${ARGN} ${NPP_PLUGINS_DIR}/compat/compat.cpp)
	set_target_properties(${name} PROPERTIES
		PREFIX ""
		LIBRARY_OUTPUT_DIRECTORY ${NPP_PLUGINS_OUTPUT_DIR}/${name})
	npp_plugin_settings(${name})
	add_dependencies(npp_plugins ${name})
	install(TARGETS ${name} LIBRARY DESTINATION ${NPP_INSTALL_PLUGINSDIR}/${name} COMPONENT Plugins)
endfunction()

add_subdirectory(mimeTools)
add_subdirectory(NppConverter)
add_subdirectory(NppExport)
add_subdirectory(DSpellCheck)
add_subdirectory(NppExec)

# The tests run the plugins in Notepad++ itself, so they need its build
if(TARGET nppapp AND NPP_BUILD_TESTS)
	add_subdirectory(tests)
endif()
