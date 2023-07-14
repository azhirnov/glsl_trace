cmake_minimum_required( VERSION 3.10 FATAL_ERROR )

message( STATUS "==========================================================================\n" )
message( STATUS "project 'GLSL-Trace' generation started" )

project( "GLSL-Trace" LANGUAGES C CXX )
set_property( GLOBAL PROPERTY USE_FOLDERS ON )
message( STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} (${CMAKE_CXX_COMPILER_VERSION})" )
message( STATUS "target system: ${CMAKE_SYSTEM_NAME} (${CMAKE_SYSTEM_VERSION})" )
message( STATUS "host system: ${CMAKE_HOST_SYSTEM_NAME} (${CMAKE_HOST_SYSTEM_VERSION})" )

set( EXTERNALS_USE_STABLE_VERSIONS ON CACHE BOOL "use last stable version instead of master branch" )
set( ENABLE_TESTS ON CACHE BOOL "tests" )

set( CMAKE_DEBUG_POSTFIX "d" )
set( CMAKE_RELEASE_POSTFIX "" )
set( CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "installation directory" FORCE )
set( MAIN_BINARY_DIR "${CMAKE_BINARY_DIR}/bin" CACHE INTERNAL "" FORCE )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${MAIN_BINARY_DIR}" )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY "${MAIN_BINARY_DIR}" )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY "${MAIN_BINARY_DIR}" )
set( EXTERNALS_PATH "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "Path to external projects sources" )
set( EXTERNALS_INSTALL_PATH "${CMAKE_BINARY_DIR}/external_libs" CACHE PATH "Path to install external projects libraries" )

include( "cmake/compilers_minimal.cmake" )

add_subdirectory( "glslang" )

include( "cmake/project_template.cmake" )
add_subdirectory( "shader_trace" )
add_subdirectory( "spv_compiler" )

if (DEFINED CI_BUILD)
	add_subdirectory( "tests/vulkan" )

elseif (${ENABLE_TESTS})
	enable_testing()
	
	# Vulkan
	add_subdirectory( "glfw" )
	add_subdirectory( "tests/vulkan" )
	
	# OpenGL
	#add_subdirectory( "SPIRV-Cross" )
	#add_subdirectory( "glew/build/cmake" )
	#add_subdirectory( "tests/opengl" )
endif ()

message( STATUS "project 'GLSL-Trace' generation ended" )
message( STATUS "\n==========================================================================" )
