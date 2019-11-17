######################################
### OpenXR support
######################################

set (EZ_BUILD_OPENXR OFF CACHE BOOL "Whether support for OpenXR should be added")

######################################
### ez_requires_openxr()
######################################

macro(ez_requires_openxr)

	ez_requires_windows()
	ez_requires(EZ_BUILD_OPENXR)

endmacro()

######################################
### ez_link_target_openxr(<target>)
######################################

function(ez_link_target_openxr TARGET_NAME)

	ez_requires_openxr()

	find_package(ezOpenXR REQUIRED)

	if (EZOPENXR_FOUND)
	  target_link_libraries(${TARGET_NAME} PRIVATE ezOpenXR::Loader)

	  # TODO: How to check whether the target even has a dll, e.g. is a dynamic build?
	  add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:ezOpenXR::Loader> $<TARGET_FILE_DIR:${TARGET_NAME}>
	  )

	endif()

endfunction()

