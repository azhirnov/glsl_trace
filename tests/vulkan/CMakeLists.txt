cmake_minimum_required( VERSION 3.10 FATAL_ERROR )

file( GLOB_RECURSE SOURCES "*.*" )
add_executable( "Tests.Vulkan" ${SOURCES} )
source_group( TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} )
set_property( TARGET "Tests.Vulkan" PROPERTY FOLDER "" )
target_link_libraries( "Tests.Vulkan" PUBLIC "ShaderTrace" )
target_link_libraries( "Tests.Vulkan" PUBLIC "GLFW-lib" )
