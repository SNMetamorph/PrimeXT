cmake_minimum_required(VERSION 2.8.12)

project(squish)
include(CompilerRuntime)

add_library(${PROJECT_NAME} STATIC
	"alpha.cpp"
	"clusterfit.cpp"
	"colourblock.cpp"
	"colourfit.cpp"
	"colourset.cpp"
	"maths.cpp"
	"rangefit.cpp"
	"singlecolourfit.cpp"
	"squish.cpp"
)

target_include_directories(${PROJECT_NAME} PRIVATE
	"."
)

target_compile_definitions(${PROJECT_NAME} PRIVATE 
	_LIB=1 
)

if(NOT MSVC)
	include(CheckCCompilerFlag)

	target_compile_options(${PROJECT_NAME} PRIVATE -fno-exceptions) # GCC/Clang flag
	target_compile_options(${PROJECT_NAME} PRIVATE -Wno-write-strings) # GCC/Clang flag
	target_compile_options(${PROJECT_NAME} PRIVATE -fvisibility=hidden) # GCC/Clang flag
	CHECK_C_COMPILER_FLAG(-msse2 USE_SSE)

	if( USE_SSE )
		target_compile_options(${PROJECT_NAME} PRIVATE -msse2) # enabled SSE2
		target_compile_definitions(${PROJECT_NAME} PRIVATE SQUISH_USE_SSE=2)
	endif()

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

set_target_properties(${PROJECT_NAME} PROPERTIES
	POSITION_INDEPENDENT_CODE 1)
