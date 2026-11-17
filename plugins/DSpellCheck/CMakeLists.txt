# DSpellCheck 1.5.0 by Sergey Semushin: https://github.com/Predelnik/DSpellCheck
#
# Qt port: built against the system's Hunspell and iconv in place of the copies in deps/, without the FTP client
# and the zip files it gave (so without minizip), the Windows controls and the Windows speller. The code is a static
# library, which the plugin and the unit tests of test/ share, as upstream does.

find_package(PkgConfig REQUIRED)
pkg_check_modules(HUNSPELL REQUIRED IMPORTED_TARGET hunspell)
if(NOT TARGET Qt6::Network)
	find_package(Qt6 REQUIRED COMPONENTS Network)
endif()

file(GLOB_RECURSE DSPELLCHECK_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.h)
list(REMOVE_ITEM DSPELLCHECK_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/DllMain.cpp)

add_library(DSpellCheckStatic STATIC ${DSPELLCHECK_SOURCES} ${NPP_PLUGINS_DIR}/compat/compat.cpp)
npp_plugin_settings(DSpellCheckStatic)
target_include_directories(DSpellCheckStatic PUBLIC src deps/json deps/lsignal deps/aspell)
# Its own PluginInterface.h comes before the one of Notepad++
target_include_directories(DSpellCheckStatic BEFORE PUBLIC deps/npp_extra)
target_precompile_headers(DSpellCheckStatic PRIVATE src/common/PrecompiledHeader.h)
target_link_libraries(DSpellCheckStatic PUBLIC PkgConfig::HUNSPELL Qt6::Widgets Qt6::Network ${CMAKE_DL_LIBS})

npp_add_plugin(DSpellCheck src/plugin/DllMain.cpp DSpellCheck.qrc)
set_target_properties(DSpellCheck PROPERTIES AUTORCC ON)
target_include_directories(DSpellCheck BEFORE PRIVATE deps/npp_extra)
target_precompile_headers(DSpellCheck REUSE_FROM DSpellCheckStatic)
target_link_libraries(DSpellCheck PRIVATE DSpellCheckStatic)

# Upstream's unit tests, with a mocked editor and speller
if(NPP_BUILD_TESTS)
	file(GLOB DSPELLCHECK_TESTS CONFIGURE_DEPENDS test/*.cpp test/*.h)
	add_executable(dspellcheckUnitTest ${DSPELLCHECK_TESTS})
	npp_plugin_settings(dspellcheckUnitTest)
	target_include_directories(dspellcheckUnitTest PRIVATE test deps/Catch)
	target_include_directories(dspellcheckUnitTest BEFORE PRIVATE deps/npp_extra)
	# Catch 2.13 takes SIGSTKSZ for a constant, which newer glibc has not
	target_compile_definitions(dspellcheckUnitTest PRIVATE CATCH_CONFIG_NO_POSIX_SIGNALS)
	target_precompile_headers(dspellcheckUnitTest REUSE_FROM DSpellCheckStatic)
	target_link_libraries(dspellcheckUnitTest PRIVATE DSpellCheckStatic)
	add_test(NAME dspellcheckUnit COMMAND dspellcheckUnitTest)
	set_tests_properties(dspellcheckUnit PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
endif()
