set(SRC ${CMAKE_SOURCE_DIR}/PowerEditor/src)

qt_add_executable(dialogTest dialogTest.cpp
	${SRC}/NppQt/NppControls.cpp
	${SRC}/NppQt/NppMessages.cpp
	${SRC}/WinControls/StaticDialog/StaticDialog.cpp)
set_target_properties(dialogTest PROPERTIES AUTOMOC ON)
target_include_directories(dialogTest PRIVATE ${SRC} ${SRC}/NppQt ${SRC}/MISC/Common ${SRC}/WinControls ${SRC}/WinControls/StaticDialog)
target_link_libraries(dialogTest PRIVATE Qt6::Widgets Qt6::Test)

add_test(NAME dialogs COMMAND dialogTest)
set_tests_properties(dialogs PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

find_package(Qt6 REQUIRED COMPONENTS UiTools)
qt_add_executable(uiFormsTest uiFormsTest.cpp)
set_target_properties(uiFormsTest PROPERTIES AUTOMOC ON)
target_compile_definitions(uiFormsTest PRIVATE NPP_SOURCE_DIR="${SRC}")
target_link_libraries(uiFormsTest PRIVATE Qt6::Widgets Qt6::UiTools Qt6::Test)
add_test(NAME uiForms COMMAND uiFormsTest)
set_tests_properties(uiForms PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
