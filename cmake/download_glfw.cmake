# find or download GLFW

if (TRUE)
	set( EXTERNAL_GLFW_PATH "" CACHE PATH "path to GLFW source" )
	mark_as_advanced( EXTERNAL_GLFW_PATH )
	set( GLFW_INSTALL_DIR "${EXTERNALS_INSTALL_PATH}/glfw" )

	# select version
	if (${EXTERNALS_USE_STABLE_VERSIONS})
		set( GLFW_TAG "3.3" )
	else ()
		set( GLFW_TAG "master" )
	endif ()
	
	# reset to default
	if (NOT EXISTS "${EXTERNAL_GLFW_PATH}/CMakeLists.txt")
		message( STATUS "GLFW is not found in \"${EXTERNAL_GLFW_PATH}\"" )
		set( EXTERNAL_GLFW_PATH "${EXTERNALS_PATH}/GLFW" CACHE PATH "" FORCE )
	else ()
		message( STATUS "GLFW found in \"${EXTERNAL_GLFW_PATH}\"" )
	endif ()

	if (NOT EXISTS "${EXTERNAL_GLFW_PATH}/CMakeLists.txt")
		set( GLFW_REPOSITORY "https://github.com/glfw/glfw.git" )
	else ()
		set( GLFW_REPOSITORY "" )
	endif ()

	ExternalProject_Add( "External.GLFW"
		LIST_SEPARATOR		"${LIST_SEPARATOR}"
		LOG_OUTPUT_ON_FAILURE 1
		# download
		GIT_REPOSITORY		${GLFW_REPOSITORY}
		GIT_TAG				${GLFW_TAG}
		GIT_PROGRESS		1
		# update
		PATCH_COMMAND		""
		UPDATE_DISCONNECTED	1
		# configure
		SOURCE_DIR			"${EXTERNAL_GLFW_PATH}"
		CMAKE_GENERATOR		"${CMAKE_GENERATOR}"
		CMAKE_GENERATOR_PLATFORM "${CMAKE_GENERATOR_PLATFORM}"
		CMAKE_GENERATOR_TOOLSET	"${CMAKE_GENERATOR_TOOLSET}"
		CMAKE_ARGS			"-DCMAKE_CONFIGURATION_TYPES=${EXTERNAL_CONFIGURATION_TYPES}"
							"-DCMAKE_SYSTEM_VERSION=${CMAKE_SYSTEM_VERSION}"
							"-DCMAKE_INSTALL_PREFIX=${GLFW_INSTALL_DIR}"
							${BUILD_TARGET_FLAGS}
		LOG_CONFIGURE 		1
		# build
		BINARY_DIR			"${CMAKE_BINARY_DIR}/build-glfw"
		BUILD_COMMAND		${CMAKE_COMMAND}
							--build .
							--config $<CONFIG>
							--target glfw
		LOG_BUILD 			1
		# install
		INSTALL_DIR 		"${GLFW_INSTALL_DIR}"
		INSTALL_COMMAND		${CMAKE_COMMAND}
							--build .
							--config $<CONFIG>
							--target install
		LOG_INSTALL 		1
		# test
		LOG_TEST			1
	)
	set_property( TARGET "External.GLFW" PROPERTY FOLDER "External" )
	
	add_library( "GLFW-lib" INTERFACE )
	set_property( TARGET "GLFW-lib" PROPERTY INTERFACE_LINK_LIBRARIES "${GLFW_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}glfw3${CMAKE_STATIC_LIBRARY_SUFFIX}" )
	target_include_directories( "GLFW-lib" INTERFACE "${GLFW_INSTALL_DIR}/include" )
	add_dependencies( "GLFW-lib" "External.GLFW" )
endif ()
