# The plugins run in Notepad++ itself, from bin/plugins beside the program, which is where the test has to be
find_package(Qt6 REQUIRED COMPONENTS Test Network)
qt_add_executable(pluginsTest pluginsTest.cpp)
set_target_properties(pluginsTest PROPERTIES
	AUTOMOC ON
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
	ENABLE_EXPORTS ON) # SendMessage for the plugins, as the program has it
target_link_libraries(pluginsTest PRIVATE nppapp Qt6::Test Qt6::Network)
target_compile_definitions(pluginsTest PRIVATE NPP_PLUGINS_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data")
add_dependencies(pluginsTest notepad++ mimeTools NppConverter NppExport DSpellCheck)

add_test(NAME plugins COMMAND pluginsTest)
set_tests_properties(plugins PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120)

# Converter with a converter.ini of the user's own
add_test(NAME pluginsConverterSettings COMMAND pluginsTest converterSettingsFile asciiToHex)
set_tests_properties(pluginsConverterSettings PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;NPP_TEST_CONVERTER_INI=1" TIMEOUT 60)
