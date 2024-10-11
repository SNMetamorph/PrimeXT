cmake_minimum_required(VERSION 3.19)

project(server)
include(CheckIncludeFile)
include(CompilerRuntime)
check_include_file("tgmath.h" HAVE_TGMATH_H)

list(APPEND SVDLL_SOURCES
	"animating.cpp"
	"animation.cpp"
	"cbase.cpp"
	"client.cpp"
	"combat.cpp"
	"defaultai.cpp"
	"ggrenade.cpp"
	"base_tank.cpp"
	"dll_int.cpp"
	"effects.cpp"
	"ehandle.cpp"
	"game.cpp"
	"globals.cpp"
	"h_ai.cpp"
	"items.cpp"
	"lights.cpp"
	"mapents.cpp"
	"maprules.cpp"
	"monsters.cpp"
	"monsterstate.cpp"
	"nodes.cpp"
	"physic.cpp"
	"plats.cpp"
	"player.cpp"
	"ropes/CElectrifiedWire.cpp"
	"ropes/CRope.cpp"
	"ropes/CRopeSegment.cpp"
	"schedule.cpp"
	"scripted.cpp"
	"saverestore.cpp"
	"skill.cpp"
	"sound.cpp"
	"soundent.cpp"
	"spectator.cpp"
	"stats.cpp"
	"strings.cpp"
	"subs.cpp"
	"sv_materials.cpp"
	"triggers.cpp"
	"user_messages.cpp"
	"util.cpp"
	"weapons.cpp"
	"weapon_logic.cpp"
	"weapon_logic_funcs_impl.cpp"
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

if(ENABLE_PHYSX)
	include(FindPhysX)
	list(APPEND SVDLL_SOURCES
		"physx/physx_impl.cpp"
		"physx/event_handler.cpp"
		"physx/assert_handler.cpp"
		"physx/collision_filter_data.cpp"
		"physx/contact_modify_callback.cpp"
		"physx/debug_renderer.cpp"
		"physx/decomposed_shape.cpp"
		"physx/io_streams.cpp"
		"physx/error_stream.cpp"
	)
endif()

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
	"weapons"
	"entities"
	"gamerules"
	"wpn_shared"
	"${CMAKE_SOURCE_DIR}/common"
	"${CMAKE_SOURCE_DIR}/engine"
	"${CMAKE_SOURCE_DIR}/game_shared"
	"${CMAKE_SOURCE_DIR}/public"
)

if(HAVE_TGMATH_H)
	target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_TGMATH_H=1)
endif()

if(ENABLE_PHYSX)
	target_include_directories(${PROJECT_NAME} PRIVATE "physx")
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

if(ENABLE_PHYSX)
	find_package(PhysX REQUIRED)
	target_link_libraries(${PROJECT_NAME} PRIVATE
		PhysX::PhysX
	)
	find_path(PHYSX_HEADERS_DIR "PxPhysicsAPI.h")
	target_include_directories(${PROJECT_NAME} PRIVATE
		"${PHYSX_HEADERS_DIR}"
		"${PHYSX_HEADERS_DIR}/cooking"
		"${PHYSX_HEADERS_DIR}/geometry"
		"${PHYSX_HEADERS_DIR}/foundation"
	)
endif()

if(XASH_ANDROID)
	target_link_libraries(${PROJECT_NAME} PRIVATE log)
endif()

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
