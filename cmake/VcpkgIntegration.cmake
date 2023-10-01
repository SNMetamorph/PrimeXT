# allows to transparently pass in custom toolchain file along with vcpkg toolchain
# include this module in main CMakeLists.txt script BEFORE project() directive
if(CMAKE_TOOLCHAIN_FILE) 
	if (NOT CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg.cmake$")
		set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_TOOLCHAIN_FILE}")
		set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/external/vcpkg/scripts/buildsystems/vcpkg.cmake")
	endif()
else()
	set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/external/vcpkg/scripts/buildsystems/vcpkg.cmake")
endif()

set(VCPKG_OVERLAY_PORTS "${CMAKE_SOURCE_DIR}/vcpkg_ports")
set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_SOURCE_DIR}/vcpkg_triplets")
