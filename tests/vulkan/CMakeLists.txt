cmake_minimum_required( VERSION 3.10 FATAL_ERROR )

set( UTILS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../utils" )

file( GLOB_RECURSE SOURCES "*.*" )
file( GLOB_RECURSE UTILS_SRC "${UTILS_DIR}/*.*" )

add_executable( "Tests.Vulkan" ${SOURCES} ${UTILS_SRC} )

target_include_directories( "Tests.Vulkan" PUBLIC "../utils" )

source_group( TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} )
source_group( TREE ${UTILS_DIR} FILES ${UTILS_SRC} )

set_property( TARGET "Tests.Vulkan" PROPERTY FOLDER "" )
target_link_libraries( "Tests.Vulkan" PUBLIC "ShaderTrace" )
target_link_libraries( "Tests.Vulkan" PUBLIC "GLFW-lib" )
target_link_libraries( "Tests.Vulkan" PUBLIC "Vulkan-lib" )

target_compile_definitions( "Tests.Vulkan" PUBLIC "DATA_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/../ref/\"" )
target_compile_definitions( "Tests.Vulkan" PUBLIC VK_NO_PROTOTYPES )

if (NOT CI_BUILD)
	add_test( NAME "Tests.Vulkan" COMMAND "Tests.Vulkan" )
endif ()
