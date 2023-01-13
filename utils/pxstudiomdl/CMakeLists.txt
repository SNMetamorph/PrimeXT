cmake_minimum_required(VERSION 2.8.12)

project(pxstudiomdl)
include(CompilerRuntime)

# find and add source files
list(APPEND DIR_SOURCES
	"../../game_shared/mathlib.cpp"
	"../../game_shared/matrix.cpp"
	"../common/cmdlib.cpp"
	"../common/conprint.cpp"
	"../common/crashhandler.cpp"
	"../common/filesystem.cpp"
	"../common/scriplib.cpp"
	"../common/stringlib.cpp"
	"../common/virtualfs.cpp"
	"../common/zone.cpp"
	"${CMAKE_SOURCE_DIR}/public/crclib.cpp"
	"${CMAKE_SOURCE_DIR}/public/build_info.cpp"
	"image_utils.cpp"
	"optimize.cpp"
    "simpilfy.cpp"
    "skin.cpp"
    "studiomdl.cpp"
    "write.cpp"
)

# add version info
if(MSVC)
	list(APPEND DIR_SOURCES
		"pxstudiomdl.rc"
	)
endif()

add_executable(${PROJECT_NAME} ${DIR_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
	"."
	"${CMAKE_SOURCE_DIR}/game_shared" # should be first because mathlib.h also locates i
	"${CMAKE_SOURCE_DIR}/engine"
	"${CMAKE_SOURCE_DIR}/server"
	"${CMAKE_SOURCE_DIR}/public"
	"${CMAKE_SOURCE_DIR}/common"
	"../common"
	"../common/imagelib"
	"${CMAKE_SOURCE_DIR}/external/mimalloc/include"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE 
	IGNORE_SEARCH_IN_WADS=1
	MIMALLOC_ALLOCATOR=1
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
	target_compile_definitions(${PROJECT_NAME} PRIVATE DBGHELP=1)
	target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS=1 _CRT_NONSTDC_NO_DEPRECATE=1) # disable annoying CRT warnings
endif()

if(MSVC)
	# debug mode compiler flags
	target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:/Od>) # disable optimizing at all
	if(NOT ENABLE_ASAN)
		target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:/ZI>) # enable "Edit and Continue" MSVC feature
		target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:
			/INCREMENTAL 
			/SAFESEH:NO
		>)
	else()
		target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:/Zi>)
	endif()

	# release mode compiler flags
	target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release,RelWithDebInfo>:/O2>) # enable optimizing to maximize speed
	target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release,RelWithDebInfo>:/GL>) # enable whole program optimization
endif()

# set static compiler runtime
if(ENABLE_STATIC_LINKING)
	set_compiler_runtime(${PROJECT_NAME} STATIC)
endif()

# link dependency libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
	imagelib
	mimalloc-static
)

# link platform-specific depedency libraries
if(NOT XASH_WIN32)
	target_link_libraries(${PROJECT_NAME} PRIVATE
		dl
	)
else()
	target_link_libraries(${PROJECT_NAME} PRIVATE
		dbghelp
	)
endif()

# link log library for Android platform
if(XASH_ANDROID)
	target_link_libraries(${PROJECT_NAME} PRIVATE
		log
	)
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
