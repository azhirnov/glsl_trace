// Copyright (c) Zhirnov Andrey. For more information see 'LICENSE'

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
	
PFN_vkGetInstanceProcAddr  _var_vkGetInstanceProcAddr = null;

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Dummy_vkGetInstanceProcAddr (VkInstance , const char * ) {  VK_LOG( "used dummy function 'vkGetInstanceProcAddr'" );  return null;  }

/*
=================================================
	VulkanLib
=================================================
*/
namespace {
	struct VulkanLib
	{
		SharedLib_t					module				= null;
		VkInstance					instance			= VK_NULL_HANDLE;
		PFN_vkGetInstanceProcAddr	getInstanceProcAddr	= null;
		int							refCounter			= 0;
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
bool VulkanLoader::Initialize (StringView libName)
{
	VulkanLib&	lib = *VulkanLibSingleton();
		
	if ( lib.module and lib.refCounter > 0 )
	{
		++lib.refCounter;
		return true;
	}
	
	_var_vkGetInstanceProcAddr = &Dummy_vkGetInstanceProcAddr;

#ifdef WIN32
	if ( not libName.empty() )
		lib.module = ::LoadLibraryA( libName.data() );

	if ( lib.module == null )
		lib.module = ::LoadLibraryA( "vulkan-1.dll" );
		
	if ( lib.module == null )
		return false;
		
	// write library path to log
	{
		char	buf[MAX_PATH] = "";
		CHECK( ::GetModuleFileNameA( lib.module, buf, DWORD(std::size(buf)) ) != FALSE );

		std::cout << "Vulkan library path: \"" << buf << '"' << std::endl;
	}

	// all global functions can be loaded using 'vkGetInstanceProcAddr', so we need to import only this function address.
	lib.getInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>( ::GetProcAddress( lib.module, "vkGetInstanceProcAddr" ));
	if ( _var_vkGetInstanceProcAddr == null )
		return false;

#else
	if ( not libName.empty() )
		lib.module = ::dlopen( libName.data(), RTLD_NOW | RTLD_LOCAL );

	if ( lib.module == null )
		lib.module = ::dlopen( "libvulkan.so", RTLD_NOW | RTLD_LOCAL );
		
	if ( lib.module == null )
		lib.module = ::dlopen( "libvulkan.so.1", RTLD_NOW | RTLD_LOCAL );
		
	if ( lib.module == null )
		return false;
		
	// write library path to log
#	ifndef PLATFORM_ANDROID
	{
		char	buf[PATH_MAX] = "";
		CHECK( dlinfo( lib.module, RTLD_DI_ORIGIN, buf ) == 0 );
			
		std::cout << "Vulkan library path: \"" << buf << '"' << std::endl;
	}
#	endif
	
	// all global functions can be loaded using 'vkGetInstanceProcAddr', so we need to import only this function address.
	lib.getInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>( ::dlsym( lib.module, "vkGetInstanceProcAddr" ));
	if ( _var_vkGetInstanceProcAddr == null )
		return false;
#endif
	
	_var_vkGetInstanceProcAddr = lib.getInstanceProcAddr;

	++lib.refCounter;
		
	// it is allowed to use null instance handle in 'vkGetInstanceProcAddr'.
	const auto	Load =	[&lib] (OUT auto& outResult, const char *procName, auto dummy)
						{{
							using FN = decltype(dummy);
							FN	result = reinterpret_cast<FN>( lib.getInstanceProcAddr( null, procName ));
							outResult = result ? result : dummy;
						}};

#	define VKLOADER_STAGE_GETADDRESS
#	 include "fn_vulkan_lib.h"
#	undef  VKLOADER_STAGE_GETADDRESS
	
	CHECK_ERR( _var_vkCreateInstance != &Dummy_vkCreateInstance );
	return true;
}
	
/*
=================================================
	LoadInstance
----
	must be externally synchronized!
	warning: multiple instances are not supported!
=================================================
*/
bool VulkanLoader::LoadInstance (VkInstance instance)
{
	VulkanLib&	lib = *VulkanLibSingleton();

	ASSERT( instance != Default );
	ASSERT( lib.instance == Default or lib.instance == instance );

	if ( lib.getInstanceProcAddr == null )
		return false;

	if ( lib.instance == instance )
		return true;	// functions already loaded for this instance

	lib.instance = instance;

	const auto	Load =	[&lib] (OUT auto& outResult, const char *procName, auto dummy)
						{{
							using FN = decltype(dummy);
							FN	result = reinterpret_cast<FN>( vkGetInstanceProcAddr( lib.instance, procName ));
							outResult = result ? result : dummy;
						}};

	#define VKLOADER_STAGE_GETADDRESS
	#include "fn_vulkan_inst.h"
	#undef  VKLOADER_STAGE_GETADDRESS

	return true;
}

/*
=================================================
	LoadDevice
----
	access to the 'vkGetDeviceProcAddr' must be externally synchronized!
=================================================
*/
bool VulkanLoader::LoadDevice (VkDevice device, OUT VulkanDeviceFnTable &table)
{
	CHECK_ERR( _var_vkGetDeviceProcAddr != &Dummy_vkGetDeviceProcAddr );

	const auto	Load =	[device] (OUT auto& outResult, const char *procName, auto dummy)
						{{
							using FN = decltype(dummy);
							FN	result = reinterpret_cast<FN>( vkGetDeviceProcAddr( device, procName ));
							outResult = result ? result : dummy;
						}};

	#define VKLOADER_STAGE_GETADDRESS
	#include "fn_vulkan_dev.h"
	#undef  VKLOADER_STAGE_GETADDRESS

	return true;
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

	#define VKLOADER_STAGE_GETADDRESS
	#include "fn_vulkan_dev.h"
	#undef  VKLOADER_STAGE_GETADDRESS
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
	VulkanLib&	lib = *VulkanLibSingleton();
		
	ASSERT( lib.refCounter > 0 );

	if ( (--lib.refCounter) != 0 )
		return;
	
#	ifdef WIN32
	if ( lib.module != null )
	{
		::FreeLibrary( lib.module );
	}
#	else
	if ( lib.module != null )
	{
		::dlclose( lib.module );
	}
#	endif

	lib.module				= null;
	lib.instance			= null;
	lib.getInstanceProcAddr	= null;

	const auto	Load =	[] (OUT auto& outResult, const char *, auto dummy)
						{{
							outResult = dummy;
						}};

	#define VKLOADER_STAGE_GETADDRESS
	#include "fn_vulkan_lib.h"
	#include "fn_vulkan_inst.h"
	#undef  VKLOADER_STAGE_GETADDRESS
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
