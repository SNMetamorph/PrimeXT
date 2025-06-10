cmake_minimum_required(VERSION 3.19)

project(mxtk)

if(NOT WIN32)
	set(CMAKE_AUTOMOC ON)
	set(CMAKE_AUTORCC ON)
	set(CMAKE_AUTOUIC ON)
endif()

file(GLOB SOURCES_COMMON "src/common/*.cpp")
if(MSVC)
	file(GLOB SOURCES_PLATFORM "src/win32/*.cpp")
else()
	file(GLOB SOURCES_PLATFORM "src/qt/*.cpp")
endif()

list(APPEND SOURCES_COMMON ${SOURCES_PLATFORM})
add_library(${PROJECT_NAME} STATIC ${SOURCES_COMMON})

target_include_directories(${PROJECT_NAME} PRIVATE
	"include"
	"src/qt"
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

if(WIN32)
	target_link_libraries(${PROJECT_NAME} PRIVATE
		"shlwapi"
		"comctl32"
		"comdlg32"
		"glu32"
		"gdi32"
		"opengl32"
		"winmm"
	)
else()
	find_package(Qt5 COMPONENTS OpenGL REQUIRED)
	target_link_libraries(${PROJECT_NAME} PRIVATE
		Qt5::OpenGL
		"GL"
	)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
	POSITION_INDEPENDENT_CODE 1)
