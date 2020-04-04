cmake_minimum_required( VERSION 3.10 FATAL_ERROR )

set( UTILS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../utils" )

file( GLOB_RECURSE SOURCES "*.*" )
file( GLOB_RECURSE UTILS_SRC "${UTILS_DIR}/*.*" )

add_executable( "Tests.OpenGL" ${SOURCES} ${UTILS_SRC} )

target_include_directories( "Tests.OpenGL" PUBLIC "../utils" )

source_group( TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} )
source_group( TREE ${UTILS_DIR} FILES ${UTILS_SRC} )

set_property( TARGET "Tests.OpenGL" PROPERTY FOLDER "" )
target_link_libraries( "Tests.OpenGL" PUBLIC "ShaderTrace" )
target_link_libraries( "Tests.OpenGL" PUBLIC "SPIRV-Cross-lib" )
target_link_libraries( "Tests.OpenGL" PUBLIC "GLFW-lib" )
target_link_libraries( "Tests.OpenGL" PUBLIC "GLEW-lib" )
target_link_libraries( "Tests.OpenGL" PUBLIC "opengl32" )

target_compile_definitions( "Tests.OpenGL" PUBLIC "ENABLE_OPENGL" "DATA_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/../ref/\"" )

if (NOT CI_BUILD)
	add_test( NAME "Tests.OpenGL" COMMAND "Tests.OpenGL" )
endif ()
