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

# prepare for external projects
if (TRUE)
	include( ExternalProject )

	set( LIST_SEPARATOR "|" )
	string( REPLACE ";" "${LIST_SEPARATOR}" EXTERNAL_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" )
	set( BUILD_TARGET_FLAGS 	"-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
								"-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
								"-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}"
								"-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
								"-DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
								"-DCMAKE_STATIC_LINKER_FLAGS=${CMAKE_STATIC_LINKER_FLAGS}"
								"-DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}" )

	foreach ( CONFIG ${CMAKE_CONFIGURATION_TYPES} )
		string( TOUPPER ${CONFIG} OUT_CONFIG )

		set( BUILD_TARGET_FLAGS
			"${BUILD_TARGET_FLAGS}"
			"-DCMAKE_C_FLAGS_${OUT_CONFIG}=${CMAKE_C_FLAGS_${OUT_CONFIG}}"
			"-DCMAKE_CXX_FLAGS_${OUT_CONFIG}=${CMAKE_CXX_FLAGS_${OUT_CONFIG}}"
			"-DCMAKE_EXE_LINKER_FLAGS_${OUT_CONFIG}=${CMAKE_EXE_LINKER_FLAGS_${OUT_CONFIG}}"
			"-DCMAKE_STATIC_LINKER_FLAGS_${OUT_CONFIG}=${CMAKE_STATIC_LINKER_FLAGS_${OUT_CONFIG}}"
			"-DCMAKE_SHARED_LINKER_FLAGS_${OUT_CONFIG}=${CMAKE_SHARED_LINKER_FLAGS_${OUT_CONFIG}}" )
	endforeach ()
endif ()

include( "cmake/download_glslang.cmake" )
include( "cmake/project_template.cmake" )
add_subdirectory( "shader_trace" )

if (DEFINED CI_BUILD)
	include( "cmake/download_vk.cmake" )
	add_subdirectory( "tests/vulkan" )

elseif (${ENABLE_TESTS})
	enable_testing()
	include( "cmake/download_vk.cmake" )
	include( "cmake/download_glfw.cmake" )
	include( "cmake/download_glew.cmake" )
	include( "cmake/download_spirvcross.cmake" )
	add_subdirectory( "tests/opengl" )
	add_subdirectory( "tests/vulkan" )
endif ()

message( STATUS "project 'GLSL-Trace' generation ended" )
message( STATUS "\n==========================================================================" )
