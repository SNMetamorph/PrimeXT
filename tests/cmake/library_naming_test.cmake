#
# library_naming_test.cmake - unit test for the library name postfix
#
# Run it with: cmake -P tests/cmake/library_naming_test.cmake
#
# The postfix has to follow the naming scheme of the engine, see
# https://github.com/FWGS/xash3d-fwgs/blob/master/Documentation/extensions/library-naming.md
#
# The most important case is Android: the engine looks for
# libclient_android_arm64.so there, so the game libraries have to carry the
# "$os_$arch" suffix (issue #288).
#

get_filename_component( CMAKE_MODULE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../cmake" ABSOLUTE )
include( "${CMAKE_MODULE_DIR}/LibraryNamingPostfix.cmake" )

set( FAILURES 0 )

function( check_postfix os arch expected )
	xash_set_build_postfix( "${os}" "${arch}" postfix )

	if( NOT postfix STREQUAL expected )
		message( "FAILED: os=\"${os}\" arch=\"${arch}\": postfix is \"${postfix}\", expected \"${expected}\"" )
		math( EXPR FAILURES "${FAILURES} + 1" )
		set( FAILURES "${FAILURES}" PARENT_SCOPE )
	endif()
endfunction()

# Windows, macOS and Linux with x86 keep the plain name
check_postfix( "" "" "" )
# ... with a non-x86 architecture they get the architecture suffix
check_postfix( "" "amd64" "_amd64" )
check_postfix( "" "i386" "_i386" )
# everything else (Android, consoles, ...) gets the "$os_$arch" suffix
check_postfix( "android" "arm64" "_android_arm64" )
check_postfix( "android" "amd64" "_android_amd64" )
check_postfix( "android" "armv7l" "_android_armv7l" )
check_postfix( "freebsd" "amd64" "_freebsd_amd64" )
check_postfix( "haiku" "i386" "_haiku_i386" )

if( FAILURES GREATER 0 )
	message( "library_naming: ${FAILURES} check(s) FAILED" )
	message( FATAL_ERROR "library_naming test failed" )
endif()

message( "library_naming: all checks passed" )
