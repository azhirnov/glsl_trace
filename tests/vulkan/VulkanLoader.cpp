// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#include "VulkanLoader.h"
#include <iostream>

#ifdef WIN32

#	include <Windows.h>

	using SharedLib_t	= HMODULE;

#else
#	include <dlfcn.h>
#   include <linux/limits.h>

	using SharedLib_t	= void*;
#endif

#define VK_LOG( _msg_ )		std::cout << _msg_ << std::endl

#define VKLOADER_STAGE_FNPOINTER
#	include "fn_vulkan_lib.h"
#	include "fn_vulkan_inst.h"
#undef  VKLOADER_STAGE_FNPOINTER

#define VKLOADER_STAGE_DUMMYFN
#	include "fn_vulkan_lib.h"
#	include "fn_vulkan_inst.h"
#	include "fn_vulkan_dev.h"
#undef  VKLOADER_STAGE_DUMMYFN

/*
=================================================
	VulkanLib
=================================================
*/
namespace {
	struct VulkanLib
	{
		SharedLib_t		module		= nullptr;
		VkInstance		instance	= VK_NULL_HANDLE;
		int				refCounter	= 0;
	};

	static VulkanLib*	VulkanLibSingleton ()
	{
		static VulkanLib	inst;
		return &inst;
	}
}
/*
=================================================
	Initialize
----
	must be externally synchronized!
=================================================
*/
bool VulkanLoader::Initialize (string libName)
{
	VulkanLib *		lib = VulkanLibSingleton();
		
	if ( lib->module and lib->refCounter > 0 )
	{
		++lib->refCounter;
		return true;
	}

#ifdef WIN32
	if ( not libName.empty() )
		lib->module = ::LoadLibraryA( libName.c_str() );

	if ( lib->module == nullptr )
		lib->module = ::LoadLibraryA( "vulkan-1.dll" );
		
	if ( lib->module == nullptr )
		return false;
		
	// write library path to log
	{
		char	buf[MAX_PATH] = "";
		CHECK( ::GetModuleFileNameA( lib->module, buf, DWORD(std::size(buf)) ) != FALSE );

		std::cout << "Vulkan library path: \"" << buf << '"' << std::endl;
	}

	const auto	Load =	[module = lib->module] (OUT auto& outResult, const char *procName, auto dummy)
						{
							using FN = decltype(dummy);
							FN	result = reinterpret_cast<FN>( ::GetProcAddress( module, procName ));
							outResult = result ? result : dummy;
						};

#else
	if ( not libName.empty() )
		lib->module = ::dlopen( libName.c_str(), RTLD_NOW | RTLD_LOCAL );

	if ( lib->module == nullptr )
		lib->module = ::dlopen( "libvulkan.so", RTLD_NOW | RTLD_LOCAL );
		
	if ( lib->module == nullptr )
		lib->module = ::dlopen( "libvulkan.so.1", RTLD_NOW | RTLD_LOCAL );
		
	if ( lib->module == nullptr )
		return false;
		
	// write library path to log
#	ifndef PLATFORM_ANDROID
	{
		char	buf[PATH_MAX] = "";
		CHECK( dlinfo( lib->module, RTLD_DI_ORIGIN, buf ) == 0 );
			
		std::cout << "Vulkan library path: \"" << buf << '"' << std::endl;
	}
#	endif

	const auto	Load =	[module = lib->module] (OUT auto& outResult, const char *procName, auto dummy)
						{
							using FN = decltype(dummy);
							FN	result = reinterpret_cast<FN>( ::dlsym( module, procName ));
							outResult = result ? result : dummy;
						};
#endif

	++lib->refCounter;
		

#	define VKLOADER_STAGE_GETADDRESS
#	 include "fn_vulkan_lib.h"
#	undef  VKLOADER_STAGE_GETADDRESS

	ASSERT( _var_vkCreateInstance != &Dummy_vkCreateInstance );
	ASSERT( _var_vkGetInstanceProcAddr != &Dummy_vkGetInstanceProcAddr );

	return true;
}
	
/*
=================================================
	LoadInstance
----
	must be externally synchronized!
=================================================
*/
void VulkanLoader::LoadInstance (VkInstance instance)
{
	VulkanLib *		lib = VulkanLibSingleton();

	ASSERT( instance );
	ASSERT( lib->instance == nullptr or lib->instance == instance );
	ASSERT( _var_vkGetInstanceProcAddr != &Dummy_vkGetInstanceProcAddr );

	if ( lib->instance == instance )
		return;

	lib->instance = instance;

	const auto	Load =	[instance] (OUT auto& outResult, const char *procName, auto dummy)
						{
							using FN = decltype(dummy);
							FN	result = reinterpret_cast<FN>( vkGetInstanceProcAddr( instance, procName ));
							outResult = result ? result : dummy;
						};
		
#	define VKLOADER_STAGE_GETADDRESS
#	 include "fn_vulkan_inst.h"
#	undef  VKLOADER_STAGE_GETADDRESS
}
	
/*
=================================================
	LoadDevice
----
	access to the 'vkGetDeviceProcAddr' must be externally synchronized!
=================================================
*/
void VulkanLoader::LoadDevice (VkDevice device, OUT VulkanDeviceFnTable &table)
{
	ASSERT( _var_vkGetDeviceProcAddr != &Dummy_vkGetDeviceProcAddr );

	const auto	Load =	[device] (OUT auto& outResult, const char *procName, auto dummy)
						{
							using FN = decltype(dummy);
							FN	result = reinterpret_cast<FN>( vkGetDeviceProcAddr( device, procName ));
							outResult = result ? result : dummy;
						};

#	define VKLOADER_STAGE_GETADDRESS
#	 include "fn_vulkan_dev.h"
#	undef  VKLOADER_STAGE_GETADDRESS
}
	
/*
=================================================
	ResetDevice
=================================================
*/
void VulkanLoader::ResetDevice (OUT VulkanDeviceFnTable &table)
{
	const auto	Load =	[] (OUT auto& outResult, const char *, auto dummy) {
							outResult = dummy;
						};

#	define VKLOADER_STAGE_GETADDRESS
#	 include "fn_vulkan_dev.h"
#	undef  VKLOADER_STAGE_GETADDRESS
}

/*
=================================================
	Unload
----
	must be externally synchronized!
=================================================
*/
void VulkanLoader::Unload ()
{
	VulkanLib *		lib = VulkanLibSingleton();
		
	ASSERT( lib->refCounter > 0 );

	if ( (--lib->refCounter) != 0 )
		return;
		
#	ifdef WIN32
	if ( lib->module != nullptr )
	{
		::FreeLibrary( lib->module );
	}
#	else
	if ( lib->module != nullptr )
	{
		::dlclose( lib->module );
	}
#	endif

	lib->instance	= nullptr;
	lib->module		= nullptr;
		
	const auto	Load =	[] (OUT auto& outResult, const char *, auto dummy) {
							outResult = dummy;
						};

#	define VKLOADER_STAGE_GETADDRESS
#	 include "fn_vulkan_lib.h"
#	 include "fn_vulkan_inst.h"
#	undef  VKLOADER_STAGE_GETADDRESS
}
	
/*
=================================================
	VulkanDeviceFn_Init
=================================================
*/
void VulkanDeviceFn::VulkanDeviceFn_Init (const VulkanDeviceFn &other)
{
	_table = other._table;
}
	
/*
=================================================
	VulkanDeviceFn_Init
=================================================
*/
void VulkanDeviceFn::VulkanDeviceFn_Init (VulkanDeviceFnTable *table)
{
	_table = table;
}
