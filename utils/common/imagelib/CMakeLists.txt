cmake_minimum_required(VERSION 2.8.12)

project(imagelib)
include(CompilerRuntime)

# find and add source files
list(APPEND DIR_SOURCES
	"../cmdlib.cpp"
	"../conprint.cpp"
	"../filesystem.cpp"
	#"../mathlib.cpp"
	#"../scriplib.cpp"
	"../stringlib.cpp"
	"../virtualfs.cpp"
	"../zone.cpp"
	"imagelib.cpp"
	"ddstex.cpp"
	"gamma.cpp"
	"quantizer.cpp"
)

add_library(${PROJECT_NAME} STATIC ${DIR_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
	"."
	"../"
	"${CMAKE_SOURCE_DIR}/public"
	"${CMAKE_SOURCE_DIR}/common"
	"${CMAKE_SOURCE_DIR}/external/miniz"
	"${CMAKE_SOURCE_DIR}/external/squish"
	"${CMAKE_SOURCE_DIR}/external/mimalloc/include"
) 

target_compile_definitions(${PROJECT_NAME} PRIVATE 
	PXBSP_COMPILING=1 # to avoid header hell because of another mathlib implementation
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
	target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS=1 _CRT_NONSTDC_NO_DEPRECATE=1) # disable annoying CRT warnings
	target_compile_definitions(${PROJECT_NAME} PRIVATE DBGHELP=1)
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
if(ENABLE_STATIC_LINKING)
	set_compiler_runtime(${PROJECT_NAME} STATIC)
endif()

# add dependecy libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
	squish
	mimalloc-static
)

set_target_properties(${PROJECT_NAME} PROPERTIES
	POSITION_INDEPENDENT_CODE 1)
