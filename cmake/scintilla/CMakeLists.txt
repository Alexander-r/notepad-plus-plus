# Scintilla with its Qt platform layer and the Boost regex engine Notepad++ uses for all regex search.
set(ROOT ${CMAKE_SOURCE_DIR})
set(SCI ${ROOT}/scintilla)
set(BOOSTREGEX ${ROOT}/boostregex)

file(GLOB SCINTILLA_CORE CONFIGURE_DEPENDS ${SCI}/src/*.cxx)

add_library(scintilla STATIC
	${SCINTILLA_CORE}
	${SCI}/qt/ScintillaEditBase/PlatQt.cpp
	${SCI}/qt/ScintillaEditBase/ScintillaQt.cpp
	${SCI}/qt/ScintillaEditBase/ScintillaEditBase.cpp
	${SCI}/qt/ScintillaEditBase/ScintillaQt.h
	${SCI}/qt/ScintillaEditBase/ScintillaEditBase.h
	${BOOSTREGEX}/BoostRegExSearch.cxx
	${BOOSTREGEX}/UTF8DocumentIterator.cxx
)
set_target_properties(scintilla PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)
target_include_directories(scintilla
	PUBLIC ${SCI}/include ${SCI}/src ${SCI}/qt/ScintillaEditBase
	PRIVATE ${BOOSTREGEX})
target_compile_definitions(scintilla
	PUBLIC SCINTILLA_QT=1 SCI_OWNREGEX
	PRIVATE MAKING_LIBRARY=1 BOOST_REGEX_STANDALONE)
target_link_libraries(scintilla PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Core5Compat)
