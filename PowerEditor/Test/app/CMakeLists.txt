qt_add_executable(appTest appTest.cpp)
set_target_properties(appTest PROPERTIES
	AUTOMOC ON
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
	ENABLE_EXPORTS ON # for the plugin
)
target_link_libraries(appTest PRIVATE nppapp Qt6::Test)
add_dependencies(appTest notepad++ testPlugin mimeToolsPlugin) # for the data files beside it
target_compile_definitions(appTest PRIVATE
	NPP_TEST_PLUGIN="$<TARGET_FILE:testPlugin>"
	NPP_TEST_MIME_TOOLS_PLUGIN="$<TARGET_FILE:mimeToolsPlugin>"
	NPP_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/PowerEditor/Test")

add_library(testPlugin MODULE testPlugin.cpp)
set_target_properties(testPlugin PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/testPlugin)
target_include_directories(testPlugin PRIVATE $<TARGET_PROPERTY:nppapp,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(testPlugin PRIVATE SCINTILLA_QT=1)
target_link_libraries(testPlugin PRIVATE Qt6::Core)

add_test(NAME app COMMAND appTest)
set_tests_properties(app PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120)

# Breeze reads the desktop's colour scheme from the user's folder, so these have their own to give the same result
# on any machine. And in dark mode, which is to differ only in its colours.
add_test(NAME appDarkMode COMMAND appTest dialogLayouts)
set_tests_properties(appDarkMode PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;NPP_TEST_DARK_MODE=1;QT_STYLE_OVERRIDE=breeze;XDG_CONFIG_HOME=${CMAKE_CURRENT_SOURCE_DIR}/lightDesktop" TIMEOUT 120)

# Coming back to documents and recent files of an earlier session
add_test(NAME appRestoredSession COMMAND appTest restoredSession)
set_tests_properties(appRestoredSession PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;NPP_TEST_RESTORED_SESSION=1" TIMEOUT 60)

# Both modes on a desktop with a dark theme, whose colours are to stay. Without the KDE platform theme these skip.
foreach(mode IN ITEMS Light Dark)
	add_test(NAME app${mode}OnDarkDesktop COMMAND appTest dialogLayouts)
	set(darkDesktop "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=kde;QT_STYLE_OVERRIDE=breeze;NPP_TEST_DARK_DESKTOP=1;XDG_CONFIG_HOME=${CMAKE_CURRENT_SOURCE_DIR}/darkDesktop")
	if(mode STREQUAL "Dark")
		list(APPEND darkDesktop "NPP_TEST_DARK_MODE=1")
	endif()
	set_tests_properties(app${mode}OnDarkDesktop PROPERTIES ENVIRONMENT "${darkDesktop}" TIMEOUT 120)
endforeach()

# And the light mode with the desktop style that dark mode has just been checked with, where available
add_test(NAME appBreeze COMMAND appTest dialogLayouts)
set_tests_properties(appBreeze PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_STYLE_OVERRIDE=breeze;XDG_CONFIG_HOME=${CMAKE_CURRENT_SOURCE_DIR}/lightDesktop" TIMEOUT 120)

# And at a fractional display scale, where the edges of lines and controls fall inside device pixels
add_test(NAME appDarkModeScaled COMMAND appTest dialogLayouts)
set_tests_properties(appDarkModeScaled PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=1.25;NPP_TEST_DARK_MODE=1;QT_STYLE_OVERRIDE=breeze;XDG_CONFIG_HOME=${CMAKE_CURRENT_SOURCE_DIR}/lightDesktop" TIMEOUT 120)
add_test(NAME appScaled COMMAND appTest dialogLayouts)
set_tests_properties(appScaled PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=1.5" TIMEOUT 120)

# The dialogs must also hold together with the largest Dialog Font of the preferences
add_test(NAME appLargeDialogFont COMMAND appTest dialogLayouts)
set_tests_properties(appLargeDialogFont PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;NPP_TEST_DIALOG_FONT_SIZE=24" TIMEOUT 120)

# The Function List parsers, each run through the built application
qt_add_executable(functionListTest functionListTest.cpp)
set_target_properties(functionListTest PROPERTIES AUTOMOC ON)
target_compile_definitions(functionListTest PRIVATE
	NPP_APPLICATION="$<TARGET_FILE:notepad++>"
	NPP_FUNCTIONLIST_DIR="${CMAKE_SOURCE_DIR}/PowerEditor/Test/FunctionList"
	NPP_FUNCTIONLIST_TESTFILES_DIR="${CMAKE_SOURCE_DIR}/PowerEditor/installer/filesForTesting")
target_link_libraries(functionListTest PRIVATE Qt6::Core Qt6::Test)
add_dependencies(functionListTest notepad++)
add_test(NAME functionList COMMAND functionListTest)
set_tests_properties(functionList PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 300)
