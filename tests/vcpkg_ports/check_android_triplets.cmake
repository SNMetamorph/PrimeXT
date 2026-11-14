# check_android_triplets.cmake - regression test for the overlay Android vcpkg triplets
#
# Usage: cmake -DREPO_ROOT=<repo root> -P check_android_triplets.cmake
#
# Background: the vcpkg builtin *-android triplets build the dependencies against
# Android API 28 (VCPKG_CMAKE_SYSTEM_VERSION 28), while the game itself is linked
# against the API level of android/app/build.gradle (minSdkVersion, but never below
# the NDK minimum of 21). libc symbols such as stderr, stdout and __fwrite_chk only
# exist from API 23/24 on, so fmt/imgui compiled against API 28 headers failed to
# link into libclient.so with "undefined symbol: stderr".
#
# The overlay triplets in vcpkg_triplets/ pin the dependencies to the same API level
# as the app. This test checks that they exist for every ABI the app builds and
# that their API level matches build.gradle.

if(NOT DEFINED REPO_ROOT)
	message(FATAL_ERROR "REPO_ROOT is not set")
endif()

set(failures 0)

# check(<message> <condition...>) - the condition is anything if() accepts
macro(check message)
	if(${ARGN})
		message(STATUS "ok: ${message}")
	else()
		message(SEND_ERROR "FAIL: ${message}")
		math(EXPR failures "${failures} + 1")
	endif()
endmacro()

# the NDK doesn't support anything below API 21 (NDK r26)
set(NDK_MIN_API 21)

file(READ "${REPO_ROOT}/android/app/build.gradle" gradle)
string(REGEX MATCH "minSdkVersion[ \t]+([0-9]+)" _ "${gradle}")
set(min_sdk "${CMAKE_MATCH_1}")
check("minSdkVersion found in android/app/build.gradle (${min_sdk})" min_sdk MATCHES "^[0-9]+$")

if(min_sdk LESS NDK_MIN_API)
	set(expected_api ${NDK_MIN_API})
else()
	set(expected_api ${min_sdk})
endif()

# ABI filter -> vcpkg triplet, see cmake/VcpkgAndroid.cmake
set(abi_triplets
	"arm64-v8a:arm64-android"
	"armeabi-v7a:arm-neon-android"
	"x86_64:x64-android"
)

foreach(entry IN LISTS abi_triplets)
	string(REPLACE ":" ";" parts "${entry}")
	list(GET parts 0 abi)
	list(GET parts 1 triplet)

	check("gradle builds ${abi}" gradle MATCHES "'${abi}'")

	set(file "${REPO_ROOT}/vcpkg_triplets/${triplet}.cmake")
	check("overlay triplet ${triplet} exists" EXISTS "${file}")
	if(NOT EXISTS "${file}")
		continue()
	endif()

	file(READ "${file}" text)
	string(REGEX MATCH "set\\(VCPKG_CMAKE_SYSTEM_VERSION[ \t]+([0-9]+)\\)" _ "${text}")
	set(api "${CMAKE_MATCH_1}")
	check("${triplet} sets VCPKG_CMAKE_SYSTEM_VERSION to ${expected_api} (found '${api}')" api STREQUAL expected_api)
	check("${triplet} targets Android" text MATCHES "VCPKG_CMAKE_SYSTEM_NAME[ \t]+Android")
	check("${triplet} keeps the static library linkage" text MATCHES "VCPKG_LIBRARY_LINKAGE[ \t]+static")
	check("${triplet} selects ANDROID_ABI=${abi}" text MATCHES "-DANDROID_ABI=${abi}")
endforeach()

check("VcpkgIntegration.cmake overlays vcpkg_triplets" EXISTS "${REPO_ROOT}/cmake/VcpkgIntegration.cmake")
file(READ "${REPO_ROOT}/cmake/VcpkgIntegration.cmake" integration)
check("VcpkgIntegration.cmake sets VCPKG_OVERLAY_TRIPLETS to vcpkg_triplets" integration MATCHES "VCPKG_OVERLAY_TRIPLETS[^\n]*vcpkg_triplets")

if(failures GREATER 0)
	message(FATAL_ERROR "${failures} check(s) failed")
endif()
message(STATUS "android triplet checks passed")
