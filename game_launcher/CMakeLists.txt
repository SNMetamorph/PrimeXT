cmake_minimum_required(VERSION 2.8.12)

project(game_launcher)
include(CompilerRuntime)

# find and add source files
list(APPEND DIR_SOURCES
	"game.cpp"
)

# add resource file
if(MSVC)
	list(APPEND DIR_SOURCES
		"game.rc"
	)
endif()

add_executable(${PROJECT_NAME} WIN32 ${DIR_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
	"."
	"../common"
	"../public"
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
	target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/Ob1>) # disable force function inlining, to prevent segfault in release mode
	target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/O1>) # enable optimizing to minimize size
endif()

if(MSVC)
	target_link_libraries(${PROJECT_NAME} PRIVATE
		"user32"
		"shell32"
	)
else()
endif()

# set static compiler runtime
set_compiler_runtime(${PROJECT_NAME} STATIC)

# set build output directory
set(DIR_LAUNCHER_OUTPUT 
	${CMAKE_BINARY_DIR}/$<CONFIG>/
)

if(MSVC)
	set_target_properties(game_launcher PROPERTIES 
		ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${DIR_LAUNCHER_OUTPUT}
		ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${DIR_LAUNCHER_OUTPUT}
		LIBRARY_OUTPUT_DIRECTORY_DEBUG ${DIR_LAUNCHER_OUTPUT}
		LIBRARY_OUTPUT_DIRECTORY_RELEASE ${DIR_LAUNCHER_OUTPUT}
		RUNTIME_OUTPUT_DIRECTORY_DEBUG ${DIR_LAUNCHER_OUTPUT}
		RUNTIME_OUTPUT_DIRECTORY_RELEASE ${DIR_LAUNCHER_OUTPUT}
	)
else()
	set_target_properties(game_launcher PROPERTIES 
		ARCHIVE_OUTPUT_DIRECTORY ${DIR_LAUNCHER_OUTPUT}
		LIBRARY_OUTPUT_DIRECTORY ${DIR_LAUNCHER_OUTPUT}
		RUNTIME_OUTPUT_DIRECTORY ${DIR_LAUNCHER_OUTPUT}
	)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
	OUTPUT_NAME "${GAMEDIR}"
	PREFIX "")

set_target_properties(${PROJECT_NAME} PROPERTIES
	POSITION_INDEPENDENT_CODE 1)

install( TARGETS ${PROJECT_NAME}
	DESTINATION "${GAMEDIR}/"
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
	    GROUP_READ GROUP_EXECUTE
		WORLD_READ WORLD_EXECUTE 
)
