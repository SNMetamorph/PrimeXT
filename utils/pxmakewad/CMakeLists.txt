cmake_minimum_required(VERSION 2.8.12)

project(pxmakewad)
include(CompilerRuntime)

# find and add source files
list(APPEND DIR_SOURCES
	"../common/cmdlib.cpp"
	"../common/conprint.cpp"
	"../common/crashhandler.cpp"
	"../common/filesystem.cpp"
	"../common/wadfile.cpp"
	"../common/mathlib.cpp"
	"../common/stringlib.cpp"
	"../common/ddstex.cpp"
	"../common/gamma.cpp"
	"../common/virtualfs.cpp"
	"../common/imagelib.cpp"
	"../common/quantizer.cpp"
	"../common/zone.cpp"
	"${CMAKE_SOURCE_DIR}/public/build_info.cpp"
	"lmptex.cpp"
    "makewad.cpp"
    "miptex.cpp"
)

# add version info
if(MSVC)
	list(APPEND DIR_SOURCES
		"pxmakewad.rc"
	)
endif()

add_executable(${PROJECT_NAME} ${DIR_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
	"."
	"../../public"
	"../../common"
	"../common"
	"${CMAKE_SOURCE_DIR}/external/squish"
	"${CMAKE_SOURCE_DIR}/external/mimalloc/include"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE 
	PXBSP_COMPILING=1 # to avoid header hell because pxmakewad uses another mathlib implementation
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

# add mimalloc library depedency
target_link_libraries(${PROJECT_NAME} PRIVATE
	squish
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
