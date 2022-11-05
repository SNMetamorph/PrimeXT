cmake_minimum_required(VERSION 2.8.12)

project(server)
include(CheckIncludeFile)
include(CompilerRuntime)
check_include_file("tgmath.h" HAVE_TGMATH_H)

# add source files
set(SVDLL_SOURCES
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
	"${CMAKE_SOURCE_DIR}/game_shared/crc.cpp"
)

list(APPEND SVDLL_SOURCES
	"monsters/aflock.cpp"
	"monsters/agrunt.cpp"
	"airtank.cpp"
	"monsters/animating.cpp"
	"monsters/animation.cpp"
	"monsters/apache.cpp"
	"monsters/barnacle.cpp"
	"monsters/barney.cpp"
	"monsters/bigmomma.cpp"
	"monsters/bloater.cpp"
	"bmodels.cpp"
	"monsters/bullsquid.cpp"
	"buttons.cpp"
	"cbase.cpp"
	"ropes/CElectrifiedWire.cpp"
	"client.cpp"
	"combat.cpp"
	"monsters/controller.cpp"
	"ropes/CRope.cpp"
	"ropes/CRopeSegment.cpp"
	"crossbow.cpp"
	"crowbar.cpp"
	"monsters/defaultai.cpp"
	"dll_int.cpp"
	"doors.cpp"
	"effects.cpp"
	"egon.cpp"
	"explode.cpp"
	"monsters/flyingmonster.cpp"
	"func_break.cpp"
	"func_monitor.cpp"
	"func_tank.cpp"
	"game.cpp"
	"gamerules.cpp"
	"monsters/gargantua.cpp"
	"gauss.cpp"
	"monsters/genericmonster.cpp"
	"ggrenade.cpp"
	"globals.cpp"
	"glock.cpp"
	"monsters/gman.cpp"
	"monsters/h_ai.cpp"
	"h_battery.cpp"
	"h_cycler.cpp"
	"handgrenade.cpp"
	"monsters/hassassin.cpp"
	"monsters/headcrab.cpp"
	"healthkit.cpp"
	"monsters/hgrunt.cpp"
	"monsters/hornet.cpp"
	"hornetgun.cpp"
	"monsters/houndeye.cpp"
	"monsters/ichthyosaur.cpp"
	"monsters/islave.cpp"
	"items.cpp"
	"monsters/leech.cpp"
	"lights.cpp"
	"mapents.cpp"
	"maprules.cpp"
	"monsters/monstermaker.cpp"
	"monsters/monsters.cpp"
	"monsters/monsterstate.cpp"
	"mortar.cpp"
	"mp5.cpp"
	"multiplay_gamerules.cpp"
	"monsters/nihilanth.cpp"
	"monsters/nodes.cpp"
	"novodex.cpp"
	"physics/NxUserStream.cpp"
	"monsters/osprey.cpp"
	"pathcorner.cpp"
	"physents.cpp"
	"physic.cpp"
	"plats.cpp"
	"player.cpp"
	"monsters/playermonster.cpp"
	"python.cpp"
	"monsters/rat.cpp"
	"monsters/roach.cpp"
	"rpg.cpp"
	"satchel.cpp"
	"saverestore.cpp"
	"monsters/schedule.cpp"
	"monsters/scientist.cpp"
	"monsters/scripted.cpp"
	"shotgun.cpp"
	"singleplay_gamerules.cpp"
	"skill.cpp"
	"sound.cpp"
	"monsters/soundent.cpp"
	"spectator.cpp"
	"monsters/spritemonster.cpp"
	"monsters/squadmonster.cpp"
	"squeakgrenade.cpp"
	"stats.cpp"
	"strings.cpp"
	"subs.cpp"
	"sv_materials.cpp"
	"monsters/talkmonster.cpp"
	"teamplay_gamerules.cpp"
	"monsters/tempmonster.cpp"
	"monsters/tentacle.cpp"
	"triggers.cpp"
	"tripmine.cpp"
	"monsters/turret.cpp"
	"util.cpp"
	"weapons.cpp"
	"world.cpp"
	"monsters/xen.cpp"
	"monsters/zombie.cpp"
)

# add .def file to sources
if(MSVC)
list(APPEND SVDLL_SOURCES
	"server.def"
)
endif()

add_library (${PROJECT_NAME} SHARED ${SVDLL_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
	"."
	"monsters"
	"physics"
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
	target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/Ob1>) # disable force function inlining, to prevent segfault in release mode
	target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/O1>) # enable optimizing to minimize size
endif()

# set static compiler runtime
if(ENABLE_STATIC_LINKING)
	set_compiler_runtime(${PROJECT_NAME} STATIC)
endif()

set_target_properties (${PROJECT_NAME} PROPERTIES
	POSITION_INDEPENDENT_CODE 1)

# disable library prefix for Windows/iOS/Linux
if(APPLE OR WIN32 OR ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set_target_properties(${PROJECT_NAME} PROPERTIES
		OUTPUT_NAME ${SERVER_LIBRARY_NAME}
		PREFIX "")
endif()

install( TARGETS ${PROJECT_NAME}
	DESTINATION "${GAMEDIR}/${SERVER_INSTALL_DIR}/"
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
	    GROUP_READ GROUP_EXECUTE
		WORLD_READ WORLD_EXECUTE
)
