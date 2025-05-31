cmake_minimum_required(VERSION 3.19)

project(tracy-client)

add_library(${PROJECT_NAME} SHARED 
	"${CMAKE_CURRENT_SOURCE_DIR}/tracy_client.cpp"
	"${CMAKE_SOURCE_DIR}/external/tracy/public/TracyClient.cpp"
)

# When working with multiple libraries, it is easy to make a mistake and use different sets of feature
# macros between any two compilation jobs. If you do so, Tracy will not be able to work correctly, and
# there will be no error or warning messages about the problem. Henceforth, you must make sure each
# shared object you want to link with, or load uses the same set of macro definitions.

target_compile_definitions(${PROJECT_NAME} PRIVATE
	-DTRACY_EXPORTS
	-DTRACY_ON_DEMAND
	-DTRACY_DELAYED_INIT
)

target_compile_definitions(${PROJECT_NAME} PUBLIC 
	-DTRACY_ENABLE
	-DTRACY_ON_DEMAND
	-DTRACY_DELAYED_INIT
	-DTRACY_PROFILER_ENABLED
)

if(WIN32)
	target_compile_definitions(${PROJECT_NAME} PUBLIC -DTRACY_IMPORTS)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
	"."
)

target_include_directories(${PROJECT_NAME} PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_SOURCE_DIR}/external/tracy/public"
)

set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE 1)

install(TARGETS ${PROJECT_NAME}
	DESTINATION "${CMAKE_INSTALL_PREFIX}"
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
	    GROUP_READ GROUP_EXECUTE
		WORLD_READ WORLD_EXECUTE 
)
