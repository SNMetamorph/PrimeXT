cmake_minimum_required(VERSION 2.8.12)

project(utils)
include(CompilerRuntime)

option(ENABLE_MAP_COMPILERS_AVX2 "Enable building map compilers with AVX2 support" OFF)

# set build output directory
set(DIR_UTILS_OUTPUT 
	${CMAKE_BINARY_DIR}/$<CONFIG>/${GAMEDIR}/devkit/
)

# add mxtk library (used by pxmv and pxsv)
add_subdirectory("${CMAKE_SOURCE_DIR}/external/mxtk" mxtk)
set_compiler_runtime(mxtk STATIC)

# add mimalloc library
add_subdirectory("${CMAKE_SOURCE_DIR}/external/mimalloc" mimalloc EXCLUDE_FROM_ALL)
set_compiler_runtime(mimalloc-static STATIC)

add_subdirectory(pxstudiomdl)
add_subdirectory(pxmv)
add_subdirectory(pxbsp)
add_subdirectory(pxcsg)
add_subdirectory(pxrad)
add_subdirectory(pxvis)
add_subdirectory(pxsv)
#add_subdirectory(pxmakefont)
#add_subdirectory(pxmakewad)
#add_subdirectory(pxmaketex)

message(STATUS "Utility binaries output directory: ${DIR_UTILS_OUTPUT}")
if(MSVC)
    set_target_properties(pxstudiomdl pxmv pxbsp pxcsg pxrad pxvis pxsv PROPERTIES 
		ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${DIR_UTILS_OUTPUT} 
		ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${DIR_UTILS_OUTPUT}
		LIBRARY_OUTPUT_DIRECTORY_DEBUG ${DIR_UTILS_OUTPUT}
		LIBRARY_OUTPUT_DIRECTORY_RELEASE ${DIR_UTILS_OUTPUT}
		RUNTIME_OUTPUT_DIRECTORY_DEBUG ${DIR_UTILS_OUTPUT}
		RUNTIME_OUTPUT_DIRECTORY_RELEASE ${DIR_UTILS_OUTPUT}
    )
else()
    set_target_properties(pxstudiomdl pxmv pxbsp pxcsg pxrad pxvis pxsv PROPERTIES 
        ARCHIVE_OUTPUT_DIRECTORY ${DIR_UTILS_OUTPUT}
        LIBRARY_OUTPUT_DIRECTORY ${DIR_UTILS_OUTPUT}
        RUNTIME_OUTPUT_DIRECTORY ${DIR_UTILS_OUTPUT}
    )
endif()
