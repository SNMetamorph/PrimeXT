cmake_minimum_required(VERSION 2.8.12)

project(utils)
add_subdirectory(pxstudiomdl)
add_subdirectory(pxmv)
add_subdirectory(pxbsp)
#add_subdirectory(pxcsg)
#add_subdirectory(pxrad)
#add_subdirectory(pxvis)
#add_subdirectory(pxsv)
#add_subdirectory(pxmakefont)
#add_subdirectory(pxmakewad)
#add_subdirectory(pxmaketex)

# set build output directory
set(DIR_UTILS_OUTPUT 
	${CMAKE_SOURCE_DIR}/build/bin/${CMAKE_BUILD_TYPE}
)
set_target_properties(pxstudiomdl pxmv PROPERTIES 
    ARCHIVE_OUTPUT_DIRECTORY ${DIR_UTILS_OUTPUT}
    LIBRARY_OUTPUT_DIRECTORY ${DIR_UTILS_OUTPUT}
    RUNTIME_OUTPUT_DIRECTORY ${DIR_UTILS_OUTPUT}
)
