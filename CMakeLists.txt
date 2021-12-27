cmake_minimum_required(VERSION 2.8.12)

project(primext)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

if(NOT ${CMAKE_VERSION} VERSION_LESS "3.15.0")
	cmake_policy(SET CMP0091 NEW)
endif()

# add_compile_options for older cmake versions
if(${CMAKE_VERSION} VERSION_LESS "3.0.2")
	macro(add_compile_options)
		set(list_var "${ARGV}")
		foreach(arg IN LISTS list_var)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${arg}")
		endforeach()
	endmacro()
endif()

# Project Options
option(BUILD_CLIENT "Build client library" ON)
option(BUILD_SERVER "Build server library" ON)
option(BUILD_UTILS "Build developer utilities (map/models compilers, etc.)" ON)
option(64BIT "Build project with 64-bit support" OFF)
option(ENABLE_PHYSX "Enable PhysX support (works only on Windows at the moment)" ON)
set(GAMEDIR "primext" CACHE STRING "Game directory name")
set(SERVER_INSTALL_DIR "bin" CACHE STRING "Server library path relative to game directory")
set(CLIENT_INSTALL_DIR "bin" CACHE STRING "Client library path relative to game directory")
set(UTILS_INSTALL_DIR "utils" CACHE STRING "Utilities path relative to game directory")
set(SERVER_LIBRARY_NAME "server" CACHE STRING "Library name for Linux/MacOS/Windows")


if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND NOT 64BIT)
	message(FATAL_ERROR "You disabled 64-bit support, but compiler configured for 64-bit, check build configuration!")
endif()

if(64BIT AND CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "You enabled 64-bit support, but compiler configured for 32-bit, check build configuration!")
endif()

# Xash3D FWGS Library Naming Scheme compliance
# Documentation: https://github.com/FWGS/xash3d-fwgs/blob/master/Documentation/library-naming.md
include(LibraryNaming)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	message(STATUS "Building for 64-bit")
else()
	message(STATUS "Building for 32-bit")
endif()

if(NOT BUILD_SERVER AND NOT BUILD_CLIENT)
	message(FATAL_ERROR "Nothing to build, check build configuration!")
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

if(NOT MSVC)
	#add_compile_options(-Wempty-body) # GCC/Clang flag
	add_compile_options(-Wreturn-type) # GCC/Clang flag
else()
	add_definitions(-D_CRT_SILENCE_NONCONFORMING_TGMATH_H)
endif()

set(DIR_COMMON_OUTPUT 
	${CMAKE_SOURCE_DIR}/build/bin/${CMAKE_BUILD_TYPE}
)

# set build output directory
set_target_properties(client server PROPERTIES 
    ARCHIVE_OUTPUT_DIRECTORY ${DIR_COMMON_OUTPUT}
    LIBRARY_OUTPUT_DIRECTORY ${DIR_COMMON_OUTPUT}
    RUNTIME_OUTPUT_DIRECTORY ${DIR_COMMON_OUTPUT}
)