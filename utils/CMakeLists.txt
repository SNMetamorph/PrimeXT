cmake_minimum_required(VERSION 2.8.12)

project(utils)
include(CompilerRuntime)

option(ENABLE_MAP_COMPILERS_AVX2 "Enable building map compilers with AVX2 support" OFF)
option(BUILD_PXMV "Build PrimeXT Model Viewer" ON)
option(BUILD_PXSV "Build PrimeXT Sprite Viewer" ON)

# set build output directory
set(DIR_UTILS_OUTPUT 
	${CMAKE_BINARY_DIR}/$<CONFIG>/${GAMEDIR}/devkit/
)

# add dependency libraries subprojects
add_subdirectory("common/imagelib" imagelib)
add_subdirectory("${CMAKE_SOURCE_DIR}/external/squish" squish)
add_subdirectory("${CMAKE_SOURCE_DIR}/external/mimalloc" mimalloc EXCLUDE_FROM_ALL)
set_compiler_runtime(mimalloc-static STATIC)

# add mxtk library subproject (used by pxmv and pxsv)
if(BUILD_PXMV OR BUILD_PXSV)
	add_subdirectory("${CMAKE_SOURCE_DIR}/external/mxtk" mxtk)
	set_compiler_runtime(mxtk STATIC)
endif()

add_subdirectory(pxstudiomdl)
add_subdirectory(pxmakewad)
add_subdirectory(pxbsp)
add_subdirectory(pxcsg)
add_subdirectory(pxrad)
add_subdirectory(pxvis)

# append all targets to list
list(APPEND UTILS_TARGETS
	pxstudiomdl
	pxmakewad
	pxbsp
	pxcsg
	pxrad
	pxvis
)

if(BUILD_PXMV)
	add_subdirectory(pxmv)
	list(APPEND UTILS_TARGETS pxmv)
endif()

if(BUILD_PXSV)
	add_subdirectory(pxsv)
	list(APPEND UTILS_TARGETS pxsv)
endif()

message(STATUS "Utility binaries output directory: ${DIR_UTILS_OUTPUT}")
if(MSVC)
    set_target_properties(${UTILS_TARGETS} PROPERTIES 
		ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${DIR_UTILS_OUTPUT} 
		ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${DIR_UTILS_OUTPUT}
		LIBRARY_OUTPUT_DIRECTORY_DEBUG ${DIR_UTILS_OUTPUT}
		LIBRARY_OUTPUT_DIRECTORY_RELEASE ${DIR_UTILS_OUTPUT}
		RUNTIME_OUTPUT_DIRECTORY_DEBUG ${DIR_UTILS_OUTPUT}
		RUNTIME_OUTPUT_DIRECTORY_RELEASE ${DIR_UTILS_OUTPUT}
    )
else()
    set_target_properties(${UTILS_TARGETS} PROPERTIES 
        ARCHIVE_OUTPUT_DIRECTORY ${DIR_UTILS_OUTPUT}
        LIBRARY_OUTPUT_DIRECTORY ${DIR_UTILS_OUTPUT}
        RUNTIME_OUTPUT_DIRECTORY ${DIR_UTILS_OUTPUT}
    )
endif()
