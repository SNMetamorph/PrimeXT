# check_physx_port.cmake - regression test for the vendored vcpkg PhysX port
#
# Usage: cmake -DREPO_ROOT=<repo root> -P check_physx_port.cmake
#
# Background: the upstream vcpkg "physx" 4.1.2#6 port downloads three patches
# from GitHub's generated "<commit>.patch" URLs and verifies their SHA512.
# GitHub changed the generated output, the hash check started failing and the
# whole CMake configure step died with misleading "Ninja/compiler not found"
# errors. The port is therefore overlaid in vcpkg_ports/physx with the patches
# stored in the repository. This test makes sure it stays that way:
#
#   1. the overlay port exists and matches the version pinned in vcpkg.json
#   2. the portfile doesn't fetch patches from the network
#   3. every patch the portfile applies is present and looks like a patch
#   4. the overlay directory is really used by the vcpkg integration

if(NOT DEFINED REPO_ROOT)
	message(FATAL_ERROR "REPO_ROOT is not set")
endif()

set(PORT_DIR "${REPO_ROOT}/vcpkg_ports/physx")
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

# 1. the port exists and matches the pinned version
check("vcpkg_ports/physx/portfile.cmake exists" EXISTS "${PORT_DIR}/portfile.cmake")
check("vcpkg_ports/physx/vcpkg.json exists" EXISTS "${PORT_DIR}/vcpkg.json")

if(NOT EXISTS "${PORT_DIR}/portfile.cmake" OR NOT EXISTS "${PORT_DIR}/vcpkg.json")
	message(FATAL_ERROR "${failures} check(s) failed: the physx overlay port is missing")
endif()

file(READ "${REPO_ROOT}/vcpkg.json" manifest)
file(READ "${PORT_DIR}/vcpkg.json" port_manifest)

string(JSON overrides_len LENGTH "${manifest}" "overrides")
set(pinned_version "")
math(EXPR last "${overrides_len} - 1")
foreach(i RANGE ${last})
	string(JSON name GET "${manifest}" "overrides" ${i} "name")
	if(name STREQUAL "physx")
		string(JSON pinned_version GET "${manifest}" "overrides" ${i} "version")
	endif()
endforeach()

string(JSON port_name GET "${port_manifest}" "name")
string(JSON port_version GET "${port_manifest}" "version")
string(JSON port_revision ERROR_VARIABLE no_revision GET "${port_manifest}" "port-version")
if(no_revision)
	set(port_revision 0)
endif()

check("overlay port is named physx" port_name STREQUAL "physx")
check("overlay version ${port_version}#${port_revision} matches the pin in vcpkg.json (${pinned_version})"
	"${port_version}#${port_revision}" STREQUAL "${pinned_version}")

# 2. no network fetched patches
file(READ "${PORT_DIR}/portfile.cmake" portfile)
string(REGEX MATCHALL "[Uu][Rr][Ll][Ss]?[ \t\r\n]+\"?https?://[^ \t\r\n\")]*\\.(patch|diff)" patch_urls "${portfile}")
check("portfile doesn't download patches from the network" NOT patch_urls)
check("portfile doesn't use vcpkg_download_distfile for patches" NOT portfile MATCHES "vcpkg_download_distfile[(][^)]*patch")

# 3. every PATCHES entry is a local file that looks like a patch
string(REGEX MATCH "PATCHES[ \t\r\n]+([^)]*)\\)" patches_block "${portfile}")
set(patch_list "${CMAKE_MATCH_1}")
string(REGEX REPLACE "[ \t\r\n]+" ";" patch_list "${patch_list}")
list(FILTER patch_list EXCLUDE REGEX "^$")
list(LENGTH patch_list patch_count)
check("portfile applies patches (found ${patch_count})" patch_count GREATER 0)

foreach(patch IN LISTS patch_list)
	check("patch '${patch}' is a literal file name, not a download variable" NOT patch MATCHES "[$]")
	check("patch '${patch}' is present in the port directory" EXISTS "${PORT_DIR}/${patch}")
	if(EXISTS "${PORT_DIR}/${patch}")
		file(READ "${PORT_DIR}/${patch}" patch_text)
		check("patch '${patch}' contains a git diff" patch_text MATCHES "(^|\n)diff --git ")
		check("patch '${patch}' contains hunks" patch_text MATCHES "(^|\n)[+][+][+] ")
	endif()
endforeach()

# the three GitHub PRs the old port downloaded must now be vendored
foreach(required physx-PR569-ada4fccf.patch physx-PR569-d590c88e.patch physx-PR569-cdbfc0f.patch)
	check("vendored ${required}" EXISTS "${PORT_DIR}/${required}")
	check("portfile applies ${required}" portfile MATCHES "${required}")
endforeach()

# 4. the overlay is really consumed by the vcpkg integration
file(READ "${REPO_ROOT}/cmake/VcpkgIntegration.cmake" integration)
check("VcpkgIntegration.cmake overlays vcpkg_ports" integration MATCHES "VCPKG_OVERLAY_PORTS[^\n]*vcpkg_ports")

if(failures GREATER 0)
	message(FATAL_ERROR "${failures} check(s) failed")
endif()
message(STATUS "physx overlay port checks passed")
