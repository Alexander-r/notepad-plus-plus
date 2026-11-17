# Lexilla, including the Notepad++ lexers (User Defined Language, Find results, Objective-C).
set(LEX ${CMAKE_SOURCE_DIR}/lexilla)

file(GLOB LEXILLA_SOURCES CONFIGURE_DEPENDS ${LEX}/lexlib/*.cxx ${LEX}/lexers/*.cxx)

add_library(lexilla STATIC ${LEX}/src/Lexilla.cxx ${LEXILLA_SOURCES})
set_target_properties(lexilla PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(lexilla
	PUBLIC ${LEX}/include
	PRIVATE ${LEX}/lexlib)
target_link_libraries(lexilla PUBLIC scintilla)
