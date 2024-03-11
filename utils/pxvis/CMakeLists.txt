cmake_minimum_required(VERSION 3.19)

project(pxvis)
include(CompilerRuntime)

# find and add source files
list(APPEND DIR_SOURCES
	"../common/bspfile.cpp"
	"../common/cmdlib.cpp"
	"../common/conprint.cpp"
	"../common/file_system.cpp"
	"../common/mathlib.cpp"
	"../common/scriplib.cpp"
	"../common/stringlib.cpp"
	"../common/threads.cpp"
	"../common/zone.cpp"
	"../common/crashhandler.cpp"
	"flow.cpp"
	"qvis.cpp"
	"soundpvs.cpp"
	"winding.cpp"
)

# add version info
if(MSVC)
	list(APPEND DIR_SOURCES
		"pxvis.rc"
	)
endif()

add_executable(${PROJECT_NAME} ${DIR_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
	"."
	"../common"
	"../../public"
	"../../common"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE 
	IGNORE_SEARCH_IN_WADS=1
	PXBSP_COMPILING=1 # to avoid header hell because of another mathlib implementation
)

# pxvis compile-time settings
target_compile_definitions(${PROJECT_NAME} PRIVATE 
	HLVIS_SORT_PORTALS=1	# sort portals before flow
	#HLVIS_MERGE_PORTALS=1	# merge visibility between portals (disabled because causes lighting artifacts)
	HLVIS_ENABLE_CHECK_STACK=1 # enables CheckStack() call
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

if(ENABLE_MAP_COMPILERS_AVX2)
	if(MSVC)
		target_compile_options(${PROJECT_NAME} PRIVATE /arch:AVX2)
	else()
		message(WARNING "AVX2 support parameters not implemented for this compiler")
	endif()
endif()

# set static compiler runtime
if(ENABLE_STATIC_CRT_LINKING)
	set_compiler_runtime(${PROJECT_NAME} STATIC)
endif()

# link platform-specific depedency libraries
if(NOT XASH_WIN32)
	target_link_libraries(${PROJECT_NAME} PRIVATE
		pthread
		dl
	)
else()
	target_link_libraries(${PROJECT_NAME} PRIVATE
		dbghelp
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
