cmake_minimum_required(VERSION 3.19)

project(pxmv)
include(CompilerRuntime)

# find and add source files
list(APPEND DIR_SOURCES
	"../../game_shared/bone_setup.cpp"
	#"../../game_shared/common.cpp"
	"../../game_shared/ikcontext.cpp"
	"../../game_shared/jigglebones.cpp"
	"../../game_shared/mathlib.cpp"
	"../../game_shared/matrix.cpp"
	"../../game_shared/procbones.cpp"
	"../../game_shared/stringlib.cpp"
	"../common/conprint.cpp"
	"../common/file_system.cpp"
	"../common/cmdlib.cpp"
	"../common/zone.cpp"
	"${CMAKE_SOURCE_DIR}/public/crclib.cpp"
	"ControlPanel.cpp"
	"FileAssociation.cpp"
    "GlWindow.cpp"
	"ViewerSettings.cpp"
    "mdlviewer.cpp"
    "pakviewer.cpp"
    "studio_render.cpp"
    "studio_utils.cpp"
)

# add version info and manifest
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	list(APPEND DIR_SOURCES
		"pxmv.rc"
		"pxmv.manifest"
	)
endif()

add_executable(${PROJECT_NAME} ${DIR_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
	"."
	"${CMAKE_SOURCE_DIR}/external/mxtk/include"
	"${CMAKE_SOURCE_DIR}/external/mxtk/include/mx"
	"../../game_shared" # should be first because mathlib.h also locates in ../common
	"../../engine"
	"../../common"
	"../../public"
	"../common"
	"../common/imagelib"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE 
	IGNORE_SEARCH_IN_WADS=1
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
	target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release,RelWithDebInfo>:/GL>) # enable whole program optimization

	# set proper subsystem to avoid unwanted console window appearing
	target_link_options(${PROJECT_NAME} PRIVATE /SUBSYSTEM:WINDOWS)
endif()

# link dependencies libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
	mxtk
	imagelib
)

find_package(RapidJSON CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE RapidJSON rapidjson)

find_package(fmt CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)

if(MSVC)
	target_link_libraries(${PROJECT_NAME} PRIVATE
		"comctl32"
		"opengl32"
		"winmm"
	)
else()
endif()

if(WIN32)
	# we cannot use static runtime linking on Linux, because of Qt
	if(ENABLE_STATIC_CRT_LINKING)
		set_compiler_runtime(${PROJECT_NAME} STATIC)
	endif()
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
	POSITION_INDEPENDENT_CODE 1)

# copy .pdb files to install directory too
if(MSVC)
	install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> DESTINATION "${GAMEDIR}/${UTILS_INSTALL_DIR}/")
endif()

install(TARGETS ${PROJECT_NAME}
	DESTINATION "${GAMEDIR}/${UTILS_INSTALL_DIR}/"
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
	    GROUP_READ GROUP_EXECUTE
		WORLD_READ WORLD_EXECUTE 
)
