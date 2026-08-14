# GenerateBuildInfo.cmake - generates build_info_gen.h at build time
#
# Usage:
#   cmake -DSOURCE_DIR=<dir> -DGIT_EXECUTABLE=<git> -DOUTPUT_FILE=<path> -P GenerateBuildInfo.cmake
#
# The generated header is only rewritten when its contents actually change,
# so an unchanged commit hash does not trigger recompilation of dependents.

set(_hash "notset")

if(GIT_EXECUTABLE)
	execute_process(
		COMMAND "${GIT_EXECUTABLE}" describe --always --dirty --abbrev=7
		WORKING_DIRECTORY "${SOURCE_DIR}"
		OUTPUT_VARIABLE _hash
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET
		RESULT_VARIABLE _result
	)
	if(NOT _result EQUAL 0 OR NOT _hash)
		set(_hash "notset")
	endif()
endif()

set(_content "#pragma once\n#define XASH_BUILD_COMMIT \"${_hash}\"\n")

set(_tmp "${OUTPUT_FILE}.tmp")
file(WRITE "${_tmp}" "${_content}")

if(EXISTS "${OUTPUT_FILE}")
	file(READ "${OUTPUT_FILE}" _old)
	if(_old STREQUAL _content)
		file(REMOVE "${_tmp}")
		message(STATUS "Build info up to date: ${_hash}")
		return()
	endif()
endif()

file(RENAME "${_tmp}" "${OUTPUT_FILE}")
message(STATUS "Build info updated: ${_hash}")
