# MIME Tools: a real plugin from Windows, as a test and as an example. See README.md.
add_library(mimeToolsPlugin MODULE
	mimeTools/b64.cpp
	mimeTools/mimeTools.cpp
	mimeTools/qp.cpp
	mimeTools/saml.cpp
	mimeTools/tinflate.c
	mimeTools/url.cpp)
set_target_properties(mimeToolsPlugin PROPERTIES
	OUTPUT_NAME mimeTools
	PREFIX ""
	LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/testPlugins/mimeTools)
target_include_directories(mimeToolsPlugin BEFORE PRIVATE compat)
target_include_directories(mimeToolsPlugin PRIVATE $<TARGET_PROPERTY:nppapp,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(mimeToolsPlugin PRIVATE SCINTILLA_QT=1 UNICODE _UNICODE)
target_link_libraries(mimeToolsPlugin PRIVATE Qt6::Widgets)
