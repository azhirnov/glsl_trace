cmake_minimum_required( VERSION 3.10 FATAL_ERROR )

file( GLOB_RECURSE SOURCES "*.*" )
add_library( "SpvCompiler" SHARED ${SOURCES} ${UTILS_SRC} )
source_group( TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} )
target_include_directories( "SpvCompiler" PUBLIC "include" )
set_property( TARGET "SpvCompiler" PROPERTY FOLDER "" )
target_link_libraries( "SpvCompiler" PUBLIC "ShaderTrace" )
target_compile_definitions( "SpvCompiler" PRIVATE "SPV_COMPILER_BUILD_DLL" )

if (FALSE)
	target_link_libraries( "SpvCompiler" PUBLIC "SPIRV-Cross-lib" )
	target_compile_definitions( "SpvCompiler" PUBLIC ENABLE_SPIRV_CROSS )
endif ()
