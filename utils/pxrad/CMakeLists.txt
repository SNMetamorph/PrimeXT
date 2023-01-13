cmake_minimum_required(VERSION 2.8.12)

project(pxrad)
set(COMPATIBILITY_TYPE "p2" CACHE STRING "Compatibility type of pxrad (available values: 'p2', 'p1', 'goldsrc')")
include(CompilerRuntime)

# find and add source files
list(APPEND DIR_SOURCES
	"../common/basefs.cpp"
	"../common/bspfile.cpp"
	"../common/cmdlib.cpp"
	"../common/conprint.cpp"
	"../common/filesystem.cpp"
	"../common/mathlib.cpp"
	"../common/polylib.cpp"
	"../common/scriplib.cpp"
	"../common/stringlib.cpp"
	"../common/threads.cpp"
	"../common/virtualfs.cpp"
	"../common/wadfile.cpp"
	"../common/zone.cpp"
	"../common/crashhandler.cpp"
	"${CMAKE_SOURCE_DIR}/public/crclib.cpp"
	"${CMAKE_SOURCE_DIR}/public/build_info.cpp"
	"alias.cpp"
	"ambientcube.cpp"
	"dirtmap.cpp"
	"facepos.cpp"
	"lerp.cpp"
	"lightmap.cpp"
	"model_lightmaps.cpp"
	"qrad.cpp"
	"raytracer.cpp"
	"studio.cpp"
	"textures.cpp"
	"trace.cpp"
	"vertexlight.cpp"
)

# add version info
if(MSVC)
	list(APPEND DIR_SOURCES
		"pxrad.rc"
	)
endif()

add_executable(${PROJECT_NAME} ${DIR_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
	"."
	"../common"
	"../common/imagelib"
	"${CMAKE_SOURCE_DIR}/public"
	"${CMAKE_SOURCE_DIR}/common"
	"${CMAKE_SOURCE_DIR}/game_shared"
	"${CMAKE_SOURCE_DIR}/external/mimalloc/include"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE 
	IGNORE_SEARCH_IN_WADS=1
	ALLOW_WADS_IN_PACKS=1
	PXBSP_COMPILING=1 # to avoid header hell because of another mathlib implementation
	MIMALLOC_ALLOCATOR=1
)

# pxrad compile-time settings
target_compile_definitions(${PROJECT_NAME} PRIVATE 
	HLRAD_COMPRESS_TRANSFERS=1	# saves 80% memory
	HLRAD_RIGHTROUND=1			# when you go down, when you go down-down!
	HLRAD_TestLine_EDGE_FIX=1	# remove shadow artifacts in some cases
)

if(COMPATIBILITY_TYPE STREQUAL "p2")
	target_compile_definitions(${PROJECT_NAME} PRIVATE 
		HLRAD_PSAVIOR=1
		HLRAD_REFLECTIVITY=1
		HLRAD_DELUXEMAPPING=1			# store deluxe vectors for bump-mapping
		#HLRAD_SHADOWMAPPING=0			# store shadow intensity for bump-mapping
		HLRAD_VERTEXLIGHTING=1 			# vertex lighting for static models
		HLRAD_LIGHTMAPMODELS=1			# calculate lightmaps on the models
		HLRAD_AMBIENTCUBES=1			# leaf ambient lighting
		HLRAD_COMPUTE_VISLIGHTMATRIX=1 	# compute vislight matrix
		HLRAD_EXTERNAL_TEXTURES=1		# allow to loading external textures
		#HLRAD_SHRINK_MEMORY=0			# TESTTEST
		HLRAD_RAYTRACE=1				# TESTTEST
	)
elseif(COMPATIBILITY_TYPE STREQUAL "p1")
	target_compile_definitions(${PROJECT_NAME} PRIVATE 
		HLRAD_PARANOIA=1
		HLRAD_PARANOIA_BUMP=1			# compute Paranoia-style bump (three lightstyles)
		HLRAD_DELUXEMAPPING=1			# store deluxe vectors for bump-mapping
		HLRAD_VERTEXLIGHTING=1			# vertex lighting for static models
	)
elseif(COMPATIBILITY_TYPE STREQUAL "goldsrc")
	target_compile_definitions(${PROJECT_NAME} PRIVATE 
		HLRAD_GOLDSRC=1
		HLRAD_REFLECTIVITY=1
		HLRAD_VERTEXLIGHTING=1			# vertex lighting for static models
		HLRAD_AMBIENTCUBES=1			# leaf ambient lighting
	)
endif()

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
		pthread
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
