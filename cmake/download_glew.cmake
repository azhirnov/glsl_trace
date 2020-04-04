# find or download GLEW

if (TRUE)
	set( EXTERNAL_GLEW_PATH "" CACHE PATH "path to GLEW source" )
	mark_as_advanced( EXTERNAL_GLEW_PATH )
	set( GLEW_INSTALL_DIR "${EXTERNALS_INSTALL_PATH}/glew" )

	# select version
	if (${EXTERNALS_USE_STABLE_VERSIONS})
		set( GLEW_TAG "glew-2.1.0" )
	else ()
		set( GLEW_TAG "master" )
	endif ()
	
	# reset to default
	if (NOT EXISTS "${EXTERNAL_GLEW_PATH}/build/cmake/CMakeLists.txt")
		message( STATUS "GLEW is not found in \"${EXTERNAL_GLEW_PATH}\"" )
		set( EXTERNAL_GLEW_PATH "${EXTERNALS_PATH}/GLEW" CACHE PATH "" FORCE )
	else ()
		message( STATUS "GLEW found in \"${EXTERNAL_GLEW_PATH}\"" )
	endif ()

	if (NOT EXISTS "${EXTERNAL_GLEW_PATH}/CMakeLists.txt")
		set( GLEW_URL "https://sourceforge.net/projects/glew/files/glew/2.1.0/glew-2.1.0.zip/download" )
	else ()
		set( GLEW_URL "" )
	endif ()

	ExternalProject_Add( "External.GLEW"
		LIST_SEPARATOR		"${LIST_SEPARATOR}"
		LOG_OUTPUT_ON_FAILURE 1
		# download
		URL					"${GLEW_URL}"
		DOWNLOAD_DIR		"${EXTERNAL_GLEW_PATH}"
		LOG_DOWNLOAD		1
		# update
		PATCH_COMMAND		""
		UPDATE_DISCONNECTED	1
		# configure
		SOURCE_DIR			"${EXTERNAL_GLEW_PATH}"
		SOURCE_SUBDIR		"build/cmake"
		CMAKE_GENERATOR		"${CMAKE_GENERATOR}"
		CMAKE_GENERATOR_PLATFORM "${CMAKE_GENERATOR_PLATFORM}"
		CMAKE_GENERATOR_TOOLSET	"${CMAKE_GENERATOR_TOOLSET}"
		CMAKE_ARGS			"-DCMAKE_CONFIGURATION_TYPES=${EXTERNAL_CONFIGURATION_TYPES}"
							"-DCMAKE_SYSTEM_VERSION=${CMAKE_SYSTEM_VERSION}"
							"-DCMAKE_INSTALL_PREFIX=${GLEW_INSTALL_DIR}"
							${BUILD_TARGET_FLAGS}
		LOG_CONFIGURE 		1
		# build
		BINARY_DIR			"${CMAKE_BINARY_DIR}/build-glew"
		BUILD_COMMAND		${CMAKE_COMMAND}
							--build .
							--config $<CONFIG>
							--target ALL_BUILD
		LOG_BUILD 			1
		# install
		INSTALL_DIR 		"${GLEW_INSTALL_DIR}"
		INSTALL_COMMAND		${CMAKE_COMMAND}
							--build .
							--config $<CONFIG>
							--target install
		LOG_INSTALL 		1
		# test
		LOG_TEST			1
	)
	set_property( TARGET "External.GLEW" PROPERTY FOLDER "External" )
	
	add_library( "GLEW-lib" INTERFACE )

	set_property( TARGET "GLEW-lib" PROPERTY INTERFACE_LINK_LIBRARIES
		"$<$<CONFIG:Debug>:${GLEW_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}libglew32d${CMAKE_STATIC_LIBRARY_SUFFIX}>"
		"$<$<CONFIG:Profile>:${GLEW_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}libglew32${CMAKE_STATIC_LIBRARY_SUFFIX}>"
		"$<$<CONFIG:Release>:${GLEW_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}libglew32${CMAKE_STATIC_LIBRARY_SUFFIX}>" )
		
	target_compile_definitions( "GLEW-lib" INTERFACE GLEW_STATIC )
	target_include_directories( "GLEW-lib" INTERFACE "${GLEW_INSTALL_DIR}/include" )
	add_dependencies( "GLEW-lib" "External.GLEW" )
endif ()
