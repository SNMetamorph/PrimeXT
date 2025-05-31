cmake_minimum_required(VERSION 3.19)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

cmake_policy(SET CMP0091 NEW)

include(VcpkgIntegration)
include(VcpkgAndroid)

option(BUILD_CLIENT "Build client library" ON)
option(BUILD_SERVER "Build server library" ON)
option(BUILD_UTILS "Build developer utilities (map/models compilers, etc.)" ON)
option(BUILD_GAME_LAUNCHER "Build game launcher executable" ON)
option(ENABLE_PHYSX "Enable PhysX support" ON)
option(ENABLE_VGUI_COMPATIBILITY "Enable VGUI compatibility (for better compatibility with old code)" OFF)
option(ENABLE_ASAN "Enable AddressSanitizer globally" OFF)
option(ENABLE_STATIC_CRT_LINKING "Enable static runtime linking for project targets" ON)
option(ENABLE_TRACY_PROFILER "Enable Tracy profiler integration" OFF)
set(GAMEDIR "primext" CACHE STRING "Game directory name")
set(SERVER_INSTALL_DIR "bin" CACHE STRING "Server library path relative to game directory")
set(CLIENT_INSTALL_DIR "bin" CACHE STRING "Client library path relative to game directory")
set(UTILS_INSTALL_DIR "devkit" CACHE STRING "Utilities path relative to game directory")
set(SERVER_LIBRARY_NAME "server" CACHE STRING "Library name for Linux/MacOS/Windows")
set(CMAKE_CXX_STANDARD 17)

if(BUILD_CLIENT)
	list(APPEND VCPKG_MANIFEST_FEATURES "client")
endif()

if(BUILD_SERVER)
	list(APPEND VCPKG_MANIFEST_FEATURES "server")
	if(ENABLE_PHYSX)
		list(APPEND VCPKG_MANIFEST_FEATURES "physx")
	endif()
endif()

if(BUILD_UTILS)
	list(APPEND VCPKG_MANIFEST_FEATURES "utils")
endif()

project(PrimeXT)

# Xash3D FWGS Library Naming Scheme compliance
# Documentation: https://github.com/FWGS/xash3d-fwgs/blob/master/Documentation/library-naming.md
include(LibraryNaming)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	message(STATUS "Building for 64-bit")
else()
	message(STATUS "Building for 32-bit")
endif()

# get current git commit short hash
execute_process(COMMAND "git" "describe" "--always" "--dirty" "--abbrev=7"
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	OUTPUT_VARIABLE SHORT_HASH
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(SHORT_HASH)
	message(STATUS "Current Git commit hash: ${SHORT_HASH}")
	add_definitions(-DXASH_BUILD_COMMIT="${SHORT_HASH}")
else()
	message(STATUS "Failed to set current Git commit hash")
endif()

# multithreaded build flag for MSVC
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>) 

# these should be BEFORE any add_library/add_executable calls
if(NOT MSVC)
	#add_compile_options(-Wempty-body) # GCC/Clang flag
	add_compile_options(-Wreturn-type) # GCC/Clang flag
	add_compile_options(-Wno-invalid-offsetof)
	add_compile_options(-Wno-conversion-null)
else()
	add_definitions(-D_CRT_SILENCE_NONCONFORMING_TGMATH_H)
endif()

# enable AddressSanitizer globally for all subprojects
if(ENABLE_ASAN)
	if(MSVC)
		add_compile_options(/fsanitize=address)
	else()
		add_compile_options(-fsanitize=address)
		add_link_options(-fsanitize=address)
	endif()
endif()

if(ENABLE_TRACY_PROFILER)
	add_subdirectory("${CMAKE_SOURCE_DIR}/tracy_client")
endif()

if(BUILD_CLIENT)
	add_subdirectory(client)
endif()

if(BUILD_SERVER)
	add_subdirectory(server)
endif()

if(BUILD_UTILS)
	add_subdirectory(utils)
endif()

if(BUILD_GAME_LAUNCHER)
	add_subdirectory(game_launcher)
endif()

# set build output directory
set(DIR_COMMON_OUTPUT 
	${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/${GAMEDIR}/bin/
)
message(STATUS "Compiled binaries output directory: ${DIR_COMMON_OUTPUT}")

if(BUILD_CLIENT)
	set_target_properties(client PROPERTIES DEBUG_POSTFIX "${XASH_BUILD_POSTFIX}")
	set_target_properties(client PROPERTIES RELEASE_POSTFIX "${XASH_BUILD_POSTFIX}")
	set_target_properties(client PROPERTIES RELWITHDEBINFO_POSTFIX "${XASH_BUILD_POSTFIX}")
	set_target_properties(client PROPERTIES MINSIZEREL_POSTFIX "${XASH_BUILD_POSTFIX}")

	set_target_properties(client PROPERTIES 
		ARCHIVE_OUTPUT_DIRECTORY ${DIR_COMMON_OUTPUT}
		LIBRARY_OUTPUT_DIRECTORY ${DIR_COMMON_OUTPUT}
		RUNTIME_OUTPUT_DIRECTORY ${DIR_COMMON_OUTPUT}
	)
endif()

if(BUILD_SERVER)
	set_target_properties(server PROPERTIES DEBUG_POSTFIX "${XASH_BUILD_POSTFIX}")
	set_target_properties(server PROPERTIES RELEASE_POSTFIX "${XASH_BUILD_POSTFIX}")
	set_target_properties(server PROPERTIES RELWITHDEBINFO_POSTFIX "${XASH_BUILD_POSTFIX}")
	set_target_properties(server PROPERTIES MINSIZEREL_POSTFIX "${XASH_BUILD_POSTFIX}")

	set_target_properties(server PROPERTIES 
		ARCHIVE_OUTPUT_DIRECTORY ${DIR_COMMON_OUTPUT}
		LIBRARY_OUTPUT_DIRECTORY ${DIR_COMMON_OUTPUT}
		RUNTIME_OUTPUT_DIRECTORY ${DIR_COMMON_OUTPUT}
	)
endif()
