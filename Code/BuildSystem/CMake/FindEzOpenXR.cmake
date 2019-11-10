
# find the folder into which the OpenXR loader has been installed

# early out, if this target has been created before
if (TARGET ezOpenXR::Loader)
	return()
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
	# UWP builds

	set (OPENXR_DYNAMIC OFF)
	find_path(EZ_OPENXR_DIR include/openxr/openxr.h)

	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(OPENXR_BIN_PREFIX "x64_uwp")
	else()
		set(OPENXR_BIN_PREFIX "Win32_uwp")
	endif()

elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	# Desktop Windows builds

	set (OPENXR_DYNAMIC ON)
	find_path(EZ_OPENXR_DIR include/openxr/openxr.h)

	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(OPENXR_BIN_PREFIX "x64")
	else()
		set(OPENXR_BIN_PREFIX "Win32")
	endif()

endif()

set (OPENXR_DIR_LOADER "${EZ_OPENXR_DIR}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(EzOpenXR DEFAULT_MSG EZ_OPENXR_DIR)

if (EZOPENXR_FOUND)

	add_library(ezOpenXR::Loader SHARED IMPORTED)
	if (OPENXR_DYNAMIC)
		set_target_properties(ezOpenXR::Loader PROPERTIES IMPORTED_LOCATION "${OPENXR_DIR_LOADER}/native/${OPENXR_BIN_PREFIX}/release/bin/openxr_loader.dll")
		set_target_properties(ezOpenXR::Loader PROPERTIES IMPORTED_LOCATION_DEBUG "${OPENXR_DIR_LOADER}/native/${OPENXR_BIN_PREFIX}/release/bin/openxr_loader.dll")
	endif()
	set_target_properties(ezOpenXR::Loader PROPERTIES IMPORTED_IMPLIB "${OPENXR_DIR_LOADER}/native/${OPENXR_BIN_PREFIX}/release/lib/openxr_loader.lib")
	set_target_properties(ezOpenXR::Loader PROPERTIES IMPORTED_IMPLIB_DEBUG "${OPENXR_DIR_LOADER}/native/${OPENXR_BIN_PREFIX}/release/lib/openxr_loader.lib")
	set_target_properties(ezOpenXR::Loader PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${OPENXR_DIR_LOADER}/include")

endif()

mark_as_advanced(FORCE EZ_OPENXR_DIR)

unset (OPENXR_DYNAMIC)
unset (OPENXR_DIR_LOADER)
unset (OPENXR_BIN_PREFIX)






