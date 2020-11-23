// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "../source/Common.h"

#ifdef _MSC_VER
#	pragma warning (push, 0)
#	include <vulkan/vulkan.h>
#	pragma warning (pop)
#else
#	include <vulkan/vulkan.h>
#endif


#define VKLOADER_STAGE_DECLFNPOINTER
#	 include "fn_vulkan_lib.h"
#	 include "fn_vulkan_inst.h"
#undef  VKLOADER_STAGE_DECLFNPOINTER

#define VKLOADER_STAGE_INLINEFN
#	 include "fn_vulkan_lib.h"
#	 include "fn_vulkan_inst.h"
#undef  VKLOADER_STAGE_INLINEFN



//
// Vulkan Device Functions Table
//
struct VulkanDeviceFnTable final
{
	friend struct VulkanLoader;
	friend class VulkanDeviceFn;

// variables
private:
#	define VKLOADER_STAGE_FNPOINTER
#	 include "fn_vulkan_dev.h"
#	undef  VKLOADER_STAGE_FNPOINTER


// methods
public:
	VulkanDeviceFnTable () {}

	VulkanDeviceFnTable (const VulkanDeviceFnTable &) = delete;
	VulkanDeviceFnTable (VulkanDeviceFnTable &&) = delete;

	VulkanDeviceFnTable& operator = (const VulkanDeviceFnTable &) = delete;
	VulkanDeviceFnTable& operator = (VulkanDeviceFnTable &&) = delete;
};



//
// Vulkan Device Functions
//
class VulkanDeviceFn
{
// variables
private:
	VulkanDeviceFnTable *		_table;

// methods
public:
	VulkanDeviceFn () : _table{nullptr} {}
	explicit VulkanDeviceFn (VulkanDeviceFnTable *table) : _table{table} {}

	void VulkanDeviceFn_Init (const VulkanDeviceFn &other);
	void VulkanDeviceFn_Init (VulkanDeviceFnTable *table);

#	define VKLOADER_STAGE_INLINEFN
#	 include "fn_vulkan_dev.h"
#	undef  VKLOADER_STAGE_INLINEFN
};



//
// Vulkan Loader
//
struct VulkanLoader final
{
	VulkanLoader () = delete;

	static bool Initialize (std::string libName = {});
	static void LoadInstance (VkInstance instance);
	static void Unload ();
		
	static void LoadDevice (VkDevice device, VulkanDeviceFnTable &table);
	static void ResetDevice (VulkanDeviceFnTable &table);
};

