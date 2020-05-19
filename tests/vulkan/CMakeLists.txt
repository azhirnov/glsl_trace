cmake_minimum_required( VERSION 3.10 FATAL_ERROR )

file( GLOB_RECURSE SOURCES "*.*" )
add_executable( "Tests.Vulkan" ${SOURCES} ${UTILS_SRC} )
source_group( TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} )

set_property( TARGET "Tests.Vulkan" PROPERTY FOLDER "" )
target_link_libraries( "Tests.Vulkan" PUBLIC "ShaderTrace" )
target_link_libraries( "Tests.Vulkan" PUBLIC "Vulkan-lib" )

if (FALSE)
	target_link_libraries( "Tests.Vulkan" PUBLIC "SPIRV-Cross-lib" )
	target_compile_definitions( "Tests.Vulkan" PUBLIC ENABLE_SPIRV_CROSS )
endif ()

target_compile_definitions( "Tests.Vulkan" PUBLIC "DATA_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/../ref/\"" )
target_compile_definitions( "Tests.Vulkan" PUBLIC VK_NO_PROTOTYPES )

if (UNIX)
	target_link_libraries( "Tests.Vulkan" PUBLIC "dl;pthread" )
endif ()

if (NOT CI_BUILD)
	add_test( NAME "Tests.Vulkan" COMMAND "Tests.Vulkan" )
endif ()
