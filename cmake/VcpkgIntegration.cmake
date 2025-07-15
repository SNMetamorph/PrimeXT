#
# VcpkgIntegration.cmake - module for one-step integration vcpkg into project
# Include this module in root CMakeLists.txt script BEFORE project() directive
#
# This is free and unencumbered software released into the public domain.
#
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
#
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#
# For more information, please refer to <http://unlicense.org/>

function(get_vcpkg_instance_path RESULT_VAR)
	if(DEFINED ENV{VCPKG_ROOT})
		if(VCPKG_FORCE_USE_SUBMODULE_DIST)
			set(${RESULT_VAR} "${CMAKE_SOURCE_DIR}/external/vcpkg/scripts/buildsystems/vcpkg.cmake" PARENT_SCOPE)
			message(STATUS "External vcpkg installation found, but vcpkg submodule forced to use")
		else()
			set(${RESULT_VAR} "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" PARENT_SCOPE)
			message(STATUS "External vcpkg installation found at path: $ENV{VCPKG_ROOT}")
		endif()
	else()
		set(${RESULT_VAR} "${CMAKE_SOURCE_DIR}/external/vcpkg/scripts/buildsystems/vcpkg.cmake" PARENT_SCOPE)
		message(STATUS "External vcpkg installation not found, submodule will be used")
	endif()
endfunction()

# allows to transparently pass in custom toolchain file along with vcpkg toolchain
if(NOT VCPKG_TARGET_ANDROID)
	set(VCPKG_INSTANCE_PATH "")
	get_vcpkg_instance_path(VCPKG_INSTANCE_PATH)

	if(CMAKE_TOOLCHAIN_FILE) 
		if (NOT CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg.cmake$")
			set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_TOOLCHAIN_FILE}")
			set(CMAKE_TOOLCHAIN_FILE "${VCPKG_INSTANCE_PATH}")
		endif()
	else()
		set(CMAKE_TOOLCHAIN_FILE "${VCPKG_INSTANCE_PATH}")
	endif()
endif()

# setup directories for overlay triplets and ports
set(VCPKG_OVERLAY_PORTS "${CMAKE_SOURCE_DIR}/vcpkg_ports")
set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_SOURCE_DIR}/vcpkg_triplets")
