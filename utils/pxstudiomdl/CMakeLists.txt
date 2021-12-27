cmake_minimum_required(VERSION 2.8.12)

project(pxstudiomdl)
include(CompilerRuntime)

# find and add source files
list(APPEND DIR_SOURCES
	"../../game_shared/mathlib.cpp"
	"../../game_shared/matrix.cpp"
	"../common/cmdlib.cpp"
	"../common/conprint.cpp"
	"../common/filesystem.cpp"
	"../common/scriplib.cpp"
	"../common/stringlib.cpp"
	"../common/zone.cpp"
	"imagelib.cpp"
	"optimize.cpp"
    "quantizer.cpp"
    "simpilfy.cpp"
    "skin.cpp"
    "studiomdl.cpp"
    "write.cpp"
)

add_executable(${PROJECT_NAME} ${DIR_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
	"."
	"../../game_shared" # should be first because mathlib.h also locates i
	"../../engine"
	"../../server"
	"../../common"
	"../common"
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

if(CMAKE_CONFIGURATION_TYPES MATCHES Debug)
	# debug mode compiler/linker flags
	if(MSVC)
		target_compile_options(${PROJECT_NAME} PRIVATE /Od) # disable optimizing at all
		# enable "Edit and Continue" MSVC feature
		target_compile_options(${PROJECT_NAME} PRIVATE /ZI) 
		target_link_options(${PROJECT_NAME} PRIVATE /INCREMENTAL /SAFESEH:NO)
		#target_link_libraries(${PROJECT_NAME} msvcrtd.lib)
	endif()
else()
	# release mode compiler/linker flags
	if(MSVC)
		target_compile_options(${PROJECT_NAME} PRIVATE /Ob1) # disable force function inlining, to prevent segfault in release mode
		target_compile_options(${PROJECT_NAME} PRIVATE /O1) # enable optimizing to minimize size
		#target_link_libraries(${PROJECT_NAME} msvcrt.lib)
	endif()
endif()

# set static compiler runtime
set_compiler_runtime(${PROJECT_NAME} STATIC)

set_target_properties(${PROJECT_NAME} PROPERTIES
	POSITION_INDEPENDENT_CODE 1)

install( TARGETS ${PROJECT_NAME}
	DESTINATION "${GAMEDIR}/${UTILS_INSTALL_DIR}/"
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
	    GROUP_READ GROUP_EXECUTE
		WORLD_READ WORLD_EXECUTE 
)