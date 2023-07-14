cmake_minimum_required( VERSION 3.10 FATAL_ERROR )

message( STATUS "==========================================================================\n" )
message( STATUS "project 'GLSL-Trace' generation started" )

project( "GLSL-Trace" LANGUAGES C CXX )
set_property( GLOBAL PROPERTY USE_FOLDERS ON )
message( STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} (${CMAKE_CXX_COMPILER_VERSION})" )
message( STATUS "target system: ${CMAKE_SYSTEM_NAME} (${CMAKE_SYSTEM_VERSION})" )
message( STATUS "host system: ${CMAKE_HOST_SYSTEM_NAME} (${CMAKE_HOST_SYSTEM_VERSION})" )

set( ENABLE_TESTS ON CACHE BOOL "tests" )

set( CMAKE_DEBUG_POSTFIX "d" )
set( CMAKE_RELEASE_POSTFIX "" )
set( CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "installation directory" FORCE )
set( MAIN_BINARY_DIR "${CMAKE_BINARY_DIR}/bin" CACHE INTERNAL "" FORCE )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${MAIN_BINARY_DIR}" )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY "${MAIN_BINARY_DIR}" )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY "${MAIN_BINARY_DIR}" )

include( "cmake/compilers_minimal.cmake" )

set( ENABLE_CTEST                 OFF  CACHE BOOL "" FORCE )
set( ENABLE_HLSL                  OFF  CACHE BOOL "" FORCE )
set( ENABLE_OPT                    ON  CACHE BOOL "" FORCE )
set( ENABLE_SPVREMAPPER           OFF  CACHE BOOL "" FORCE )
set( ENABLE_GLSLANG_BINARIES      OFF  CACHE BOOL "" FORCE )
set( SKIP_GLSLANG_INSTALL         OFF  CACHE BOOL "" FORCE )
set( SKIP_SPIRV_TOOLS_INSTALL     OFF  CACHE BOOL "" FORCE )
set( SPIRV_SKIP_EXECUTABLES        ON  CACHE BOOL "" FORCE )
set( SPIRV_SKIP_TESTS              ON  CACHE BOOL "" FORCE )
set( BUILD_TESTING                OFF  CACHE BOOL "" FORCE )
set( BUILD_SHARED_LIBS            OFF  CACHE BOOL "" FORCE )
set( BUILD_EXTERNAL                ON  CACHE BOOL "" FORCE )
add_subdirectory( "glslang" )

include( "cmake/project_template.cmake" )
add_subdirectory( "shader_trace" )

if (DEFINED CI_BUILD)
	add_subdirectory( "tests/vulkan" )

elseif (${ENABLE_TESTS})
	enable_testing()
	
	# Vulkan
	set( GLFW_BUILD_DOCS OFF CACHE INTERNAL "" FORCE )
	set( GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "" FORCE )
	set( GLFW_BUILD_TESTS OFF CACHE INTERNAL "" FORCE )
	set( GLFW_INSTALL OFF CACHE INTERNAL "" FORCE )
	add_subdirectory( "glfw" )
	
	add_subdirectory( "tests/vulkan" )
endif ()

message( STATUS "project 'GLSL-Trace' generation ended" )
message( STATUS "\n==========================================================================" )
