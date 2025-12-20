cmake_minimum_required(VERSION 3.19)

project(client)
include(CheckIncludeFile)
include(CompilerRuntime)
check_include_file("tgmath.h" HAVE_TGMATH_H)

# client source files
list(APPEND CLDLL_SOURCES
	"ammo.cpp"
	"ammo_secondary.cpp"
	"ammohistory.cpp"
	"battery.cpp"
	"cdll_int.cpp"
	"death.cpp"
	"geiger.cpp"
	"health.cpp"
	"entity.cpp"
	"hud.cpp"
	"hud_msg.cpp"
	"hud_redraw.cpp"
	"hud_update.cpp"
	"hud_flashlight.cpp"
	"input.cpp"
	"input_xash3d.cpp"
	"input_mouse.cpp"
	"menu.cpp"
	"message.cpp"
	"motd.cpp"
	"parsemsg.cpp"
	"saytext.cpp"
	"scoreboard.cpp"
	"status_icons.cpp"
	"statusbar.cpp"
	"text_message.cpp"
	"train.cpp"
	"utils.cpp"
	"r_view.cpp"
	"postfx_controller.cpp"
	"postfx_parameters.cpp"
	"client_weapon_layer_impl.cpp"
	"weapon_predicting_context.cpp"
)

# shared source files
list(APPEND CLDLL_SOURCES
	"${CMAKE_SOURCE_DIR}/game_shared/bone_setup.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/common.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/ikcontext.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/jigglebones.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/material.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/mathlib.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/matrix.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/bounding_box.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/frustum.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/meshdesc.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/meshdesc_factory.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/playermove.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/procbones.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/stringlib.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/virtualfs.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/trace.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/weapon_context.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/seeded_random_generator.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/filesystem_utils.cpp"
	"${CMAKE_SOURCE_DIR}/game_shared/filesystem_manager.cpp"
	"${CMAKE_SOURCE_DIR}/public/crclib.cpp"
)

# render source files
file(GLOB RENDER_SOURCES "render/*.cpp")

# entity wrappers source files
file(GLOB ENTITIES_SOURCES "entities/*.cpp")

# game events source files
file(GLOB GAME_EVENTS_SOURCES "events/*.cpp")

# weapon shared code source files
file(GLOB WEAPONS_SHARED_SOURCES "${CMAKE_SOURCE_DIR}/game_shared/weapons/*.cpp")

# ImGui source files
list(APPEND IMGUI_SOURCES
	"ui/vgui_support_int.cpp"
	"ui/imgui_manager.cpp"
	"ui/imgui_window_system.cpp"
	"ui/ui_main.cpp"
	"ui/ui_demo_window.cpp"
	"ui/ui_material_editor.cpp"
	"ui/ui_postfx_menu.cpp"
)

list(APPEND CLDLL_SOURCES ${IMGUI_SOURCES})
list(APPEND CLDLL_SOURCES ${RENDER_SOURCES})
list(APPEND CLDLL_SOURCES ${ENTITIES_SOURCES})
list(APPEND CLDLL_SOURCES ${GAME_EVENTS_SOURCES})
list(APPEND CLDLL_SOURCES ${WEAPONS_SHARED_SOURCES})
add_library(${PROJECT_NAME} SHARED ${CLDLL_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
	"."
	"render/"
	"ui/"
	"entities/"
	"../common"
	"../engine"
	"../game_shared"
	"../public"
)

if(HAVE_TGMATH_H)
	target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_TGMATH_H=1)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE 
	CLIENT_WEAPONS=1 
	CLIENT_DLL=1
)

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
	target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/GL>) # enable whole program optimization

	# enable dynamic debugging feature for release mode
	target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:RelWithDebInfo>:/dynamicdeopt>)
	target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:RelWithDebInfo>:/dynamicdeopt>)
endif()

# set static compiler runtime
if(ENABLE_STATIC_CRT_LINKING)
	set_compiler_runtime(${PROJECT_NAME} STATIC)
endif()

if(ENABLE_TRACY_PROFILER)
	target_link_libraries(${PROJECT_NAME} PRIVATE tracy-client)
endif()

# link dependency libraries
find_package(imgui CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui::imgui)

find_package(fmt CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)

if(WIN32)
	target_link_libraries(${PROJECT_NAME} PRIVATE user32.lib)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS NO
	POSITION_INDEPENDENT_CODE 1
)

# disable library prefix for Windows/iOS/Linux
if(APPLE OR WIN32 OR ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set_target_properties(${PROJECT_NAME} PROPERTIES
		OUTPUT_NAME "client"
		PREFIX "")
endif()

# copy .pdb files to install directory too
if(MSVC)
	install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> DESTINATION "${GAMEDIR}/${CLIENT_INSTALL_DIR}/")
endif()

install(TARGETS ${PROJECT_NAME}
	DESTINATION "${GAMEDIR}/${CLIENT_INSTALL_DIR}/"
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
	    GROUP_READ GROUP_EXECUTE
		WORLD_READ WORLD_EXECUTE 
)
