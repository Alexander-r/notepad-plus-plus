qt_add_executable(searchEngineTest searchEngineTest.cpp)
set_target_properties(searchEngineTest PROPERTIES AUTOMOC ON)
target_link_libraries(searchEngineTest PRIVATE scintilla lexilla Qt6::Test)

add_test(NAME searchEngine COMMAND searchEngineTest)
set_tests_properties(searchEngine PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
