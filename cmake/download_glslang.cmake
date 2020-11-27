# find or download GLSLANG

if (TRUE)
	set( EXTERNAL_GLSLANG_PATH "" CACHE PATH "path to glslang source" )
	mark_as_advanced( EXTERNAL_GLSLANG_PATH )
	
	# reset to default
	if (NOT EXISTS "${EXTERNAL_GLSLANG_PATH}/CMakeLists.txt")
		message( STATUS "glslang is not found in \"${EXTERNAL_GLSLANG_PATH}\"" )
		set( EXTERNAL_GLSLANG_PATH "${EXTERNALS_PATH}/glslang" CACHE PATH "" FORCE )
	else ()
		message( STATUS "glslang found in \"${EXTERNAL_GLSLANG_PATH}\"" )
	endif ()
	
	if (FALSE)
		find_package( Vulkan )
		if (Vulkan_FOUND)
			message( STATUS "used glslang from VulkanSDK" )
			set( EXTERNAL_GLSLANG_PATH "${Vulkan_INCLUDE_DIR}/../glslang" CACHE PATH "" FORCE )
		endif ()
	endif ()

	if (NOT EXISTS "${EXTERNAL_GLSLANG_PATH}/CMakeLists.txt")
		set( GLSLANG_REPOSITORY "https://github.com/KhronosGroup/glslang.git" )
	else ()
		set( GLSLANG_REPOSITORY "" )
	endif ()
	
	if (NOT EXISTS "${EXTERNAL_GLSLANG_PATH}/External/SPIRV-Tools/include")
		set( SPIRVTOOLS_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Tools.git" )
	else ()
		set( SPIRVTOOLS_REPOSITORY "" )
	endif ()
	
	if (NOT EXISTS "${EXTERNAL_GLSLANG_PATH}/External/SPIRV-Tools/external/SPIRV-Headers/include")
		set( SPIRVHEADERS_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Headers.git" )
	else ()
		set( SPIRVHEADERS_REPOSITORY "" )
	endif ()
	
	set( ENABLE_HLSL OFF CACHE BOOL "glslang option" )
	set( ENABLE_OPT OFF CACHE BOOL "glslang option" )
	mark_as_advanced( ENABLE_HLSL ENABLE_OPT )

	# SPIRV-Tools require Python 3 for building
	if (${ENABLE_OPT})
		find_package( PythonInterp 3.7 REQUIRED )
		find_package( PythonLibs 3.7 REQUIRED )
	endif ()

	#if (${EXTERNALS_USE_STABLE_VERSIONS})
	#	set( GLSLANG_TAG "10-11.0.0" )
	#	set( SPIRV_TOOLS_TAG "v2020.4" )
	#	set( SPIRV_HEADERS_TAG "1.5.4" )
	#else ()
		set( GLSLANG_TAG "master" )
		set( SPIRV_TOOLS_TAG "master" )
		set( SPIRV_HEADERS_TAG "master" )
	#endif ()

	set( GLSLANG_DEPS "External.glslang" )

	ExternalProject_Add( "External.glslang"
		LOG_OUTPUT_ON_FAILURE 1
		# download
		GIT_REPOSITORY		${GLSLANG_REPOSITORY}
		GIT_TAG				${GLSLANG_TAG}
		GIT_PROGRESS		1
		# update
		PATCH_COMMAND		""
		UPDATE_DISCONNECTED	1
		LOG_UPDATE			1
		# configure
		SOURCE_DIR			"${EXTERNAL_GLSLANG_PATH}"
		CONFIGURE_COMMAND	""
		# build
		BINARY_DIR			""
		BUILD_COMMAND		""
		INSTALL_COMMAND		""
		TEST_COMMAND		""
	)
	
	if (${ENABLE_OPT})
		ExternalProject_Add( "External.SPIRV-Tools"
			LOG_OUTPUT_ON_FAILURE 1
			DEPENDS				"External.glslang"
			# download
			GIT_REPOSITORY		${SPIRVTOOLS_REPOSITORY}
			GIT_TAG				${SPIRV_TOOLS_TAG}
			GIT_PROGRESS		1
			# update
			PATCH_COMMAND		""
			UPDATE_DISCONNECTED	1
			LOG_UPDATE			1
			# configure
			SOURCE_DIR			"${EXTERNAL_GLSLANG_PATH}/External/SPIRV-Tools"
			CONFIGURE_COMMAND	""
			# build
			BINARY_DIR			""
			BUILD_COMMAND		""
			INSTALL_COMMAND		""
			TEST_COMMAND		""
		)
	
		ExternalProject_Add( "External.SPIRV-Headers"
			LOG_OUTPUT_ON_FAILURE 1
			DEPENDS				"External.glslang"
								"External.SPIRV-Tools"
			# download
			GIT_REPOSITORY		${SPIRVHEADERS_REPOSITORY}
			GIT_TAG				${SPIRV_HEADERS_TAG}
			GIT_PROGRESS		1
			# update
			PATCH_COMMAND		""
			UPDATE_DISCONNECTED	1
			LOG_UPDATE			1
			# configure
			SOURCE_DIR			"${EXTERNAL_GLSLANG_PATH}/External/SPIRV-Tools/external/SPIRV-Headers"
			CONFIGURE_COMMAND	""
			# build
			BINARY_DIR			""
			BUILD_COMMAND		""
			INSTALL_COMMAND		""
			TEST_COMMAND		""
		)

		set( GLSLANG_DEPS "${GLSLANG_DEPS}" "External.SPIRV-Tools" "External.SPIRV-Headers" )
		set_property( TARGET "External.SPIRV-Headers" PROPERTY FOLDER "External" )
		set_property( TARGET "External.SPIRV-Tools" PROPERTY FOLDER "External" )
	endif ()
	
	set( GLSLANG_INSTALL_DIR "${EXTERNALS_INSTALL_PATH}/glslang" )

	ExternalProject_Add( "External.glslang-main"
		LIST_SEPARATOR		"${LIST_SEPARATOR}"
		LOG_OUTPUT_ON_FAILURE 1
		DEPENDS				${GLSLANG_DEPS}
		DOWNLOAD_COMMAND	""
		# configure
		SOURCE_DIR			"${EXTERNAL_GLSLANG_PATH}"
		CMAKE_GENERATOR		"${CMAKE_GENERATOR}"
		CMAKE_GENERATOR_PLATFORM "${CMAKE_GENERATOR_PLATFORM}"
		CMAKE_GENERATOR_TOOLSET	"${CMAKE_GENERATOR_TOOLSET}"
		CMAKE_ARGS			"-DCMAKE_CONFIGURATION_TYPES=${EXTERNAL_CONFIGURATION_TYPES}"
							"-DCMAKE_SYSTEM_VERSION=${CMAKE_SYSTEM_VERSION}"
							"-DCMAKE_INSTALL_PREFIX=${GLSLANG_INSTALL_DIR}"
							"-DENABLE_HLSL=${ENABLE_HLSL}"
							"-DENABLE_OPT=${ENABLE_OPT}"
							"-DENABLE_SPVREMAPPER=ON"
							"-DENABLE_GLSLANG_BINARIES=OFF"
							"-DSKIP_GLSLANG_INSTALL=OFF"
							"-DSKIP_SPIRV_TOOLS_INSTALL=OFF"
							"-DSPIRV_SKIP_EXECUTABLES=OFF"
							"-DSPIRV_SKIP_TESTS=ON"
							"-DBUILD_TESTING=OFF"
							${BUILD_TARGET_FLAGS}
		LOG_CONFIGURE 		1
		# build
		BINARY_DIR			"${CMAKE_BINARY_DIR}/build-glslang"
		BUILD_COMMAND		${CMAKE_COMMAND}
							--build .
							--config $<CONFIG>
							--target glslang
		LOG_BUILD 			1
		# install
		INSTALL_DIR 		"${GLSLANG_INSTALL_DIR}"
		INSTALL_COMMAND		${CMAKE_COMMAND}
							--build .
							--config $<CONFIG>
							--target glslang
							install
							COMMAND ${CMAKE_COMMAND} -E copy_if_different
								"${EXTERNAL_GLSLANG_PATH}/StandAlone/ResourceLimits.h"
								"${GLSLANG_INSTALL_DIR}/include/StandAlone/ResourceLimits.h"
							COMMAND ${CMAKE_COMMAND} -E copy_if_different
								"${EXTERNAL_GLSLANG_PATH}/StandAlone/ResourceLimits.cpp"
								"${GLSLANG_INSTALL_DIR}/include/StandAlone/ResourceLimits.cpp"
		LOG_INSTALL 		1
		# test
		TEST_COMMAND		""
	)

	set_property( TARGET "External.glslang" PROPERTY FOLDER "External" )
	set_property( TARGET "External.glslang-main" PROPERTY FOLDER "External" )

	set( GLSLANG_DEFINITIONS "" )
	
	if (${ENABLE_OPT})
		set( GLSLANG_DEFINITIONS "${GLSLANG_DEFINITIONS}" "ENABLE_OPT" )
	endif ()

	# glslang libraries
	set( GLSLANG_LIBNAMES "SPIRV" "glslang" "OSDependent" "MachineIndependent" "GenericCodeGen" )

	if (${ENABLE_HLSL})
		set( GLSLANG_DEFINITIONS "${GLSLANG_DEFINITIONS}" "ENABLE_HLSL" )
		set( GLSLANG_LIBNAMES "${GLSLANG_LIBNAMES}" "HLSL" )
	endif ()
	
	if (UNIX)
		set( GLSLANG_LIBNAMES "${GLSLANG_LIBNAMES}" "pthread" )
	endif ()

	set( GLSLANG_LIBNAMES "${GLSLANG_LIBNAMES}" "OGLCompiler" )
								
	if (${ENABLE_OPT})
		set( GLSLANG_LIBNAMES "${GLSLANG_LIBNAMES}" "SPIRV-Tools" "SPIRV-Tools-opt" )
	endif ()

	if (MSVC)
		set( DBG_POSTFIX "${CMAKE_DEBUG_POSTFIX}" )
	else ()
		set( DBG_POSTFIX "" )
	endif ()

	set( GLSLANG_LIBRARIES "" )
	foreach ( LIBNAME ${GLSLANG_LIBNAMES} )
		if ( ${LIBNAME} STREQUAL "pthread" )
			set( GLSLANG_LIBRARIES	"${GLSLANG_LIBRARIES}" "${LIBNAME}" )
		else ()
			set( GLSLANG_LIBRARIES	"${GLSLANG_LIBRARIES}"
										"$<$<CONFIG:Debug>:${GLSLANG_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}${LIBNAME}${DBG_POSTFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}>"
										"$<$<CONFIG:Profile>:${GLSLANG_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}${LIBNAME}${CMAKE_STATIC_LIBRARY_SUFFIX}>"
										"$<$<CONFIG:Release>:${GLSLANG_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}${LIBNAME}${CMAKE_STATIC_LIBRARY_SUFFIX}>" )
		endif ()
	endforeach ()
	
	add_library( "GLSLang-lib" INTERFACE )
	set_property( TARGET "GLSLang-lib" PROPERTY INTERFACE_LINK_LIBRARIES "${GLSLANG_LIBRARIES}" )
	target_include_directories( "GLSLang-lib" INTERFACE "${GLSLANG_INSTALL_DIR}/include" )
	target_compile_definitions( "GLSLang-lib" INTERFACE "${GLSLANG_DEFINITIONS}" )
	add_dependencies( "GLSLang-lib" "External.glslang-main" )

endif ()
