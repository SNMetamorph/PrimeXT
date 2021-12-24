if (MSVCRUNTIME_INCLUDED)
  return ()
endif ()
set (MSVCRUNTIME_INCLUDED true)

#!	Configures the runtime for the given target
#	Only one runtime can be provided
#
#	\arg:target_name Name of the target whose runtime to configure
#	\flag:STATIC Whether to use the static runtime
#	\flag:DYNAMIC Whether to use the dynamic runtime
#

macro (set_compiler_runtime TARGET_NAME)
	cmake_parse_arguments (ARG "STATIC;DYNAMIC" "" "" ${ARGN})
	
	if (ARG_UNPARSED_ARGUMENTS)
		message (FATAL_ERROR "set_compiler_runtime(): unrecognized arguments: ${ARG_UNPARSED_ARGUMENTS}")
	endif ()
	
	if (ARG_STATIC AND ARG_DYNAMIC)
		message (FATAL_ERROR "set_compiler_runtime(): the STATIC and DYNAMIC runtimes are mutually exclusive")
	endif ()
	
	if (NOT TARGET ${TARGET_NAME})
		message (FATAL_ERROR "set_compiler_runtime(): target ${target_name} does not exist")
	endif()

	if (MSVC)
		# Set compiler options.
		if (ARG_STATIC)
			target_compile_options(${TARGET_NAME} PRIVATE $<$<CONFIG:DEBUG>:/MTd>)
			target_compile_options(${TARGET_NAME} PRIVATE $<$<NOT:$<CONFIG:DEBUG>>:/MT>)
		elseif (ARG_DYNAMIC)
			target_compile_options(${TARGET_NAME} PRIVATE $<$<CONFIG:DEBUG>:/MDd>)
			target_compile_options(${TARGET_NAME} PRIVATE $<$<NOT:$<CONFIG:DEBUG>>:/MD>)
		else ()
			# Should never happen, but if more combinations are needed, this will cover edge cases.
			message (FATAL_ERROR "set_compiler_runtime(): unknown runtime type selected")
		endif ()
	elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		if (ARG_STATIC)
			target_compile_options(${TARGET_NAME} PRIVATE -static-libstdc++)
			target_compile_options(${TARGET_NAME} PRIVATE -static-libstdc++)
		else ()
			message (AUTHOR_WARNING "set_compiler_runtime(): desired runtime config is not STATIC, skipping for GCC")
		endif ()
	else ()
		message (FATAL_ERROR "set_compiler_runtime(): unknown compiler type")
	endif ()
endmacro ()
