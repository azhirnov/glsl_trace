# Find or download Vulkan headers

include(FetchContent)
set( FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "don't update externals" )
mark_as_advanced( FETCHCONTENT_BASE_DIR FETCHCONTENT_FULLY_DISCONNECTED )
mark_as_advanced( FETCHCONTENT_QUIET FETCHCONTENT_UPDATES_DISCONNECTED )


if (EXISTS "${EXTERNALS_PATH}/vulkan/vulkan_core.h")
	set( Vulkan_INCLUDE_DIRS "${EXTERNALS_PATH}" )
	set( Vulkan_FOUND TRUE )

elseif (EXISTS "${EXTERNALS_PATH}/vulkan/include/vulkan/vulkan_core.h")
	set( Vulkan_INCLUDE_DIRS "${EXTERNALS_PATH}/vulkan/include" )
	set( Vulkan_FOUND TRUE )

elseif (EXISTS "${EXTERNALS_PATH}/Vulkan-Headers/include/vulkan/vulkan_core.h")
	set( Vulkan_INCLUDE_DIRS "${EXTERNALS_PATH}/Vulkan-Headers/include" )
	set( Vulkan_FOUND TRUE )
endif ()


set( VULKAN_HEADERS_TAG "v1.2.162" )

# download
if (NOT Vulkan_FOUND AND NOT CMAKE_VERSION VERSION_LESS 3.11.0)
	FetchContent_Declare( ExternalVulkanHeaders
		GIT_REPOSITORY		https://github.com/KhronosGroup/Vulkan-Headers.git
		GIT_TAG				${VULKAN_HEADERS_TAG}
		SOURCE_DIR			"${EXTERNALS_PATH}/Vulkan-Headers"
	)
	
	FetchContent_GetProperties( ExternalVulkanHeaders )
	if (NOT ExternalVulkanHeaders_POPULATED)
		message( STATUS "downloading Vulkan-Headers" )
		FetchContent_Populate( ExternalVulkanHeaders )
	endif ()

	set( Vulkan_INCLUDE_DIRS "${EXTERNALS_PATH}/Vulkan-Headers/include" )
	set( Vulkan_FOUND TRUE )
endif ()


if (NOT Vulkan_FOUND)
	message( FATAL_ERROR "Vulkan headers is not found! Install VulkanSDK or download from https://github.com/KhronosGroup/Vulkan-Headers" )
endif ()

add_library( "Vulkan-lib" INTERFACE )
target_include_directories( "Vulkan-lib" INTERFACE "${Vulkan_INCLUDE_DIRS}" )
