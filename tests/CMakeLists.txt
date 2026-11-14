cmake_minimum_required(VERSION 3.19)

# This directory works both as a subproject of the main build and standalone:
#
#   cmake -S tests -B build-tests -G Ninja && cmake --build build-tests && ctest
#
# The standalone mode matters because the main build pulls in the vcpkg
# dependencies of the game libraries, while the tests are deliberately
# dependency-free and must stay buildable everywhere.
#
# The tests follow the conventions of the engine unit tests
# (xash3d-fwgs: public/tests, filesystem/tests): every test is a standalone
# program with its own main() that returns 0 on success and a non-zero value
# on failure, so that a failing test doesn't hide the result of the others.

if( NOT PROJECT_NAME STREQUAL "primext_tests" )
	project( primext_tests CXX )
endif()

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )

enable_testing()

# primext_add_test( <name> SOURCES <files...> [LIBS <libs...>] )
function( primext_add_test name )
	cmake_parse_arguments( ARG "" "" "SOURCES;LIBS" ${ARGN} )

	add_executable( ${name} ${ARG_SOURCES} )
	target_include_directories( ${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} )

	if( ARG_LIBS )
		target_link_libraries( ${name} PRIVATE ${ARG_LIBS} )
	endif()

	add_test( NAME ${name} COMMAND ${name} )
endfunction()

primext_add_test( test_smoke SOURCES test_smoke.cpp )
