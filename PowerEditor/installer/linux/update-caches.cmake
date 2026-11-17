# Run at installation: update the caches of desktop files and icons for the installed files, unless they are staged
# for a package, whose installation updates them

if(NOT "$ENV{DESTDIR}" STREQUAL "")
	return()
endif()

# npp_install_datadir is set by the install rules before this
set(datadir "${CMAKE_INSTALL_PREFIX}/${npp_install_datadir}")
foreach(update IN ITEMS
		"update-desktop-database;-q;${datadir}/applications"
		"gtk-update-icon-cache;-q;-t;-f;${datadir}/icons/hicolor")
	list(POP_FRONT update program)
	# find_program() does not search again for a variable that is set
	unset(npp_update_program)
	find_program(npp_update_program ${program} NO_CACHE)
	if(npp_update_program)
		list(JOIN update " " arguments)
		message(STATUS "Updating: ${program} ${arguments}")
		execute_process(COMMAND ${npp_update_program} ${update} RESULT_VARIABLE result)
		if(NOT result EQUAL 0)
			message(WARNING "${program} failed; the menus or icons may need it run again")
		endif()
	endif()
endforeach()
