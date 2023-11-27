cmake_minimum_required(VERSION 2.8.12)

project(server)
include(CheckIncludeFile)
include(CompilerRuntime)
check_include_file("tgmath.h" HAVE_TGMATH_H)

list(APPEND SVDLL_SOURCES
	"bmodels.cpp"
	"buttons.cpp"
	"cbase.cpp"
	"client.cpp"
	"combat.cpp"
	"dll_int.cpp"
	"doors.cpp"
	"effects.cpp"
	"explode.cpp"
	"func_break.cpp"
	"func_monitor.cpp"
	"func_tank.cpp"
	"game.cpp"
	"globals.cpp"
	"items.cpp"
	"lights.cpp"
	"mapents.cpp"
	"maprules.cpp"
	"novodex.cpp"
	"pathcorner.cpp"
	"physents.cpp"
	"physic.cpp"
	"physics/NxUserStream.cpp"
	"plats.cpp"
	"player.cpp"
	"ropes/CElectrifiedWire.cpp"
	"ropes/CRope.cpp"
	"ropes/CRopeSegment.cpp"
	"saverestore.cpp"
	"skill.cpp"
	"sound.cpp"
	"spectator.cpp"
	"stats.cpp"
	"strings.cpp"
	"subs.cpp"
	"sv_materials.cpp"
	"triggers.cpp"
	"user_messages.cpp"
	"util.cpp"
	"weapons.cpp"
	"world.cpp"
)

# shared source files
list(APPEND SVDLL_SOURCES
	"${CMAKE_SOURCE_DIR}/game_shared/bone_setup.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/common.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/ikcontext.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/mathlib.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/matrix.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/material.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/plane.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/playermove.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/stringlib.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/vector.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/virtualfs.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/trace.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/meshdesc.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/meshdesc_factory.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/filesystem_utils.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/filesystem_manager.cpp"
	"${CMAKE_SOURCE_DIR}/public/crclib.cpp"
)

file(GLOB ENTITIES_SOURCES "entities/*.cpp")
file(GLOB MONSTERS_SOURCES "monsters/*.cpp")
file(GLOB WEAPONS_SOURCES "weapons/*.cpp")
file(GLOB GAMERULES_SOURCES "gamerules/*.cpp")

# add .def file to sources
if(MSVC)
	list(APPEND SVDLL_SOURCES "server.def")
endif()

list(APPEND SVDLL_SOURCES ${ENTITIES_SOURCES})
list(APPEND SVDLL_SOURCES ${MONSTERS_SOURCES})
list(APPEND SVDLL_SOURCES ${WEAPONS_SOURCES})
list(APPEND SVDLL_SOURCES ${GAMERULES_SOURCES})
add_library(${PROJECT_NAME} SHARED ${SVDLL_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
	"."
	"monsters"
	"physics"
	"weapons"
	"entities"
	"gamerules"
	"wpn_shared"
	"${CMAKE_SOURCE_DIR}/external/novodex"
	"${CMAKE_SOURCE_DIR}/common"
	"${CMAKE_SOURCE_DIR}/engine"
	"${CMAKE_SOURCE_DIR}/game_shared"
	"${CMAKE_SOURCE_DIR}/public"
)

if(HAVE_TGMATH_H)
	target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_TGMATH_H=1)
endif()

if(ENABLE_PHYSX)
	target_compile_definitions(${PROJECT_NAME} PRIVATE USE_PHYSICS_ENGINE=1)
endif()

if(NOT MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE -fno-exceptions) # GCC/Clang flag
	target_compile_options(${PROJECT_NAME} PRIVATE -Wno-write-strings) # GCC/Clang flag
	target_compile_options(${PROJECT_NAME} PRIVATE -fvisibility=hidden) # GCC/Clang flag
	target_compile_definitions(${PROJECT_NAME} PRIVATE _LINUX=1 LINUX=1) # It seems enough for all non-Win32 systems
	target_compile_definitions(${PROJECT_NAME} PRIVATE stricmp=strcasecmp strnicmp=strncasecmp)
	if(NOT MINGW)
		target_compile_definitions(${PROJECT_NAME} PRIVATE _snprintf=snprintf _vsnprintf=vsnprintf)
	endif()
else()
	# disable annoying CRT warnings
	target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS=1 _CRT_NONSTDC_NO_DEPRECATE=1)
endif()

if(MSVC)
	# debug mode compiler flags
	target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:/Od>) # disable optimizing at all
	target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:/ZI>) # enable "Edit and Continue" MSVC feature
	target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:
		/INCREMENTAL 
		/SAFESEH:NO
	>)

	# release mode compiler flags
	target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release,RelWithDebInfo>:/O2>) # enable optimizing to maximize speed
	target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release,RelWithDebInfo>:/GL>) # enable whole program optimization
endif()

# set static compiler runtime
if(ENABLE_STATIC_CRT_LINKING)
	set_compiler_runtime(${PROJECT_NAME} STATIC)
endif()

# link dependency libraries
find_package(fmt CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)

set_target_properties (${PROJECT_NAME} PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS NO
	POSITION_INDEPENDENT_CODE 1
)

# disable library prefix for Windows/iOS/Linux
if(APPLE OR WIN32 OR ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set_target_properties(${PROJECT_NAME} PROPERTIES
		OUTPUT_NAME ${SERVER_LIBRARY_NAME}
		PREFIX "")
endif()

# copy .pdb files to install directory too
if(MSVC)
	install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> DESTINATION "${GAMEDIR}/${SERVER_INSTALL_DIR}/")
endif()

install(TARGETS ${PROJECT_NAME}
	DESTINATION "${GAMEDIR}/${SERVER_INSTALL_DIR}/"
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
	    GROUP_READ GROUP_EXECUTE
		WORLD_READ WORLD_EXECUTE
)
