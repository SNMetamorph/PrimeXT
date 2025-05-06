cmake_minimum_required(VERSION 3.5)

project(profiler)

add_library(${PROJECT_NAME} SHARED 
	"${CMAKE_CURRENT_SOURCE_DIR}/profiler.cpp"
	"${CMAKE_SOURCE_DIR}/external/tracy/public/TracyClient.cpp"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
	-DTRACY_ON_DEMAND
	-DTRACY_EXPORTS
)

target_compile_definitions(${PROJECT_NAME} PUBLIC 
	-DTRACY_ENABLE
	-DTRACY_IMPORTS
)

target_include_directories(${PROJECT_NAME} PRIVATE
	"."
	"../../common"
	"../../public"
)

# Tracy includes
target_include_directories(${PROJECT_NAME} PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_SOURCE_DIR}/external/tracy/public"
)
