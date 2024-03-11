
function(get_vcpkg_instance_path RESULT_VAR)
	if(DEFINED ENV{VCPKG_ROOT})
		set(${RESULT_VAR} "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" PARENT_SCOPE)
		message(STATUS "External vcpkg installation found at path: $ENV{VCPKG_ROOT}")
	else()
		set(${RESULT_VAR} "${CMAKE_SOURCE_DIR}/external/vcpkg/scripts/buildsystems/vcpkg.cmake" PARENT_SCOPE)
		message(STATUS "External vcpkg installation not found, submodule will be used")
	endif()
endfunction()

# allows to transparently pass in custom toolchain file along with vcpkg toolchain
# include this module in main CMakeLists.txt script BEFORE project() directive
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

set(VCPKG_OVERLAY_PORTS "${CMAKE_SOURCE_DIR}/vcpkg_ports")
set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_SOURCE_DIR}/vcpkg_triplets")
