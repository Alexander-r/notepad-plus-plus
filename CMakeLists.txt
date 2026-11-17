# Top-level build for the Qt port of Notepad++.
#
#   cmake -S . -B build
#   cmake --build build -j
#   ctest --test-dir build --output-on-failure
#   cmake --install build                       # to CMAKE_INSTALL_PREFIX, /usr/local by default
#   DESTDIR=staging cmake --install build --prefix /usr   # staged for a package

cmake_minimum_required(VERSION 3.21)
project(NotepadPlusPlus VERSION 8.9.8.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(NPP_BUILD_TESTS "Build the test suites" ON)

set(npp_qt_components Core Gui Widgets Core5Compat Network PrintSupport)
if(NPP_BUILD_TESTS)
	list(APPEND npp_qt_components Test)
endif()
find_package(Qt6 REQUIRED COMPONENTS ${npp_qt_components})

# Where installing puts the program, its data and its plugins. On Windows all of it goes in one folder, as Notepad++ for
# Windows has it. Elsewhere the data go in share/notepad++ and the plugins in lib/notepad++/plugins, which the program
# finds from its own folder, so that an installation can move.
include(GNUInstallDirs)
if(WIN32)
	set(NPP_INSTALL_BINDIR .)
	set(NPP_INSTALL_DATADIR .)
	set(NPP_INSTALL_PLUGINSDIR plugins)
	set(npp_default_system_plugins_dir "")
else()
	set(NPP_INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR})
	set(NPP_INSTALL_DATADIR ${CMAKE_INSTALL_DATADIR}/notepad++)
	set(NPP_INSTALL_PLUGINSDIR ${CMAKE_INSTALL_LIBDIR}/notepad++/plugins)
	set(npp_default_system_plugins_dir ${CMAKE_INSTALL_FULL_LIBDIR}/notepad++/plugins)
endif()
# The plugins of the system's packages, which the program loads after those beside it and in the user's folder, such as
# /usr/lib/notepad++/plugins. Empty for none, as for an AppImage, whose Qt may not be the system's.
set(NPP_SYSTEM_PLUGINS_DIR "${npp_default_system_plugins_dir}" CACHE PATH
	"The folder of plugins installed by the system's packages, which the program also loads plugins from (empty for none)")

# Desktop integration is for freedesktop.org desktops (Linux and BSDs)
if(UNIX AND NOT APPLE)
	option(NPP_INSTALL_DESKTOP_INTEGRATION "Install the desktop file and icons with the program" ON)
else()
	set(NPP_INSTALL_DESKTOP_INTEGRATION OFF)
endif()

# Installing the program with the Qt libraries, plugins and translations it uses (and the compiler's runtime), found by
# windeployqt, makes a folder that runs on another Windows system without Qt. The bundle target installs that folder in
# the build folder.
if(WIN32)
	option(NPP_DEPLOY_QT "Install the Qt libraries the program uses with it, with windeployqt" ON)
else()
	set(NPP_DEPLOY_QT OFF)
endif()

if(NPP_BUILD_TESTS)
	enable_testing()
endif()

add_subdirectory(cmake/scintilla)
add_subdirectory(cmake/lexilla)
add_subdirectory(cmake/app)

if(NPP_BUILD_TESTS)
	add_subdirectory(PowerEditor/Test/search)
	add_subdirectory(PowerEditor/Test/core)
	add_subdirectory(PowerEditor/Test/ui)
	add_subdirectory(PowerEditor/Test/plugins)
	add_subdirectory(PowerEditor/Test/app)
endif()

# The plugins that come with Notepad++, built into bin/plugins. They are kept apart, to have a repository of their own.
add_subdirectory(plugins)

if(NPP_DEPLOY_QT)
	set(NPP_BUNDLE_DIR "${CMAKE_BINARY_DIR}/bundle/Notepad++" CACHE PATH "Where the bundle target puts the program with Qt")
	add_custom_target(bundle
		COMMAND ${CMAKE_COMMAND} -E rm -rf "${NPP_BUNDLE_DIR}"
		COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --config $<CONFIG> --prefix "${NPP_BUNDLE_DIR}"
		COMMENT "Installing Notepad++ with Qt in ${NPP_BUNDLE_DIR}"
		VERBATIM)
	add_dependencies(bundle notepad++ npp_plugins)
endif()
