set(SRC ${CMAKE_SOURCE_DIR}/PowerEditor/src)

qt_add_executable(commonTest commonTest.cpp ${SRC}/MISC/Common/Common.cpp ${SRC}/MISC/Common/FileInterface.cpp ${SRC}/MISC/Common/NppElevation.cpp ${SRC}/MISC/sha2/sha-256.cpp)
set_target_properties(commonTest PROPERTIES AUTOMOC ON)
target_include_directories(commonTest PRIVATE stub ${SRC}/MISC/Common ${SRC} ${SRC}/MISC/sha2)
target_link_libraries(commonTest PRIVATE Qt6::Widgets Qt6::Core5Compat Qt6::Test)

add_test(NAME common COMMAND commonTest)
set_tests_properties(common PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(configSettingsTest configSettingsTest.cpp ${SRC}/MISC/Common/ConfigSettings.cpp ${SRC}/pugixml/pugixml.cpp)
set_target_properties(configSettingsTest PROPERTIES AUTOMOC ON)
target_include_directories(configSettingsTest PRIVATE ${SRC}/MISC/Common ${SRC} ${SRC}/pugixml)
target_link_libraries(configSettingsTest PRIVATE Qt6::Core Qt6::Test)
add_test(NAME configSettings COMMAND configSettingsTest)
