cmake_minimum_required( VERSION 3.10 FATAL_ERROR )

file( GLOB_RECURSE SOURCES "*.*" )
add_library( "ShaderTrace" STATIC ${SOURCES} )
source_group( TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} )
target_include_directories( "ShaderTrace" PUBLIC "include" )
set_property( TARGET "ShaderTrace" PROPERTY FOLDER "" )
target_link_libraries( "ShaderTrace" PUBLIC "ProjectTemplate" )
target_link_libraries( "ShaderTrace" PUBLIC "GLSLang-lib" )

install( TARGETS "ShaderTrace" ARCHIVE DESTINATION "libs/$<CONFIG>" )
install( DIRECTORY "include" DESTINATION "include/ShaderTrace" FILES_MATCHING PATTERN "*.h*" )
