// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#include "Device.h"
#include <iostream>
#include <functional>
#include <memory>

// glslang includes
#include "glslang/MachineIndependent/localintermediate.h"
#include "glslang/Include/intermediate.h"
#include "glslang/SPIRV/doc.h"
#include "glslang/SPIRV/disassemble.h"
#include "glslang/SPIRV/GlslangToSpv.h"
#include "glslang/SPIRV/GLSL.std.450.h"
#include "StandAlone/ResourceLimits.cpp"
using namespace glslang;

// spirv cross
#ifdef ENABLE_SPIRV_CROSS
#	include "spirv_cross.hpp"
#	include "spirv_glsl.hpp"
#endif

#ifdef __linux__
#   define fopen_s( _outFile_, _name_, _mode_ ) (*(_outFile_) = fopen( (_name_), (_mode_) ))
#endif

using std::unique_ptr;

struct float3
{
	float	x, y, z;
};

struct float4
{
	float	x, y, z, w;
};

template <typename T0, typename T1>
ND_ auto  AlignToLarger (const T0 &value, const T1 &align)
{
	return ((value + align-1) / align) * align;
}

// Warning:
// Before testing on new GPU set 'UpdateReferences' to 'true', run tests,
// using git compare new references with origin, only float values may differ slightly.
// Then set 'UpdateReferences' to 'false' and run tests again.
// All tests must pass.
static const bool	UpdateReferences = true;


/*
=================================================
	Create
=================================================
*/
bool  Device::Create ()
{
	glslang::InitializeProcess();
	_tempBuf.reserve( 1024 );

	CHECK_ERR( _CreateDevice() );
	CHECK_ERR( _CreateResources() );
	return true;
}

/*
=================================================
	Destroy
=================================================
*/
void  Device::Destroy ()
{
	_DestroyResources();
	_DestroyDevice();

	for (auto&[module, trace] : _debuggableShaders) {
		delete trace;
	}
	_debuggableShaders.clear();

	glslang::FinalizeProcess();
}

/*
=================================================
	_CreateDevice
=================================================
*/
bool  Device::_CreateDevice ()
{
	VulkanDeviceFn_Init( &_deviceFnTable );

	// create instance
	{
		uint	version = VK_API_VERSION_1_1;
		
		vector< const char* >	instance_extensions = {
			#ifdef VK_KHR_get_physical_device_properties2
				VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
			#endif
			#ifdef VK_EXT_debug_utils
				VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
			#endif
		};
		vector< const char* >	instance_layers = {
			"VK_LAYER_KHRONOS_validation"
		};
		
		CHECK_ERR( VulkanLoader::Initialize() );

		_ValidateInstanceVersion( INOUT version );
		_ValidateInstanceLayers( INOUT instance_layers );
		_ValidateInstanceExtensions( INOUT instance_extensions );

		VkApplicationInfo		app_info = {};
		app_info.sType				= VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.apiVersion			= version;
		app_info.pApplicationName	= "GLSL-Trace test";
		app_info.applicationVersion	= 0;
		app_info.pEngineName		= "";
		app_info.engineVersion		= 0;
		
		VkInstanceCreateInfo			instance_create_info = {};
		instance_create_info.sType						= VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instance_create_info.pApplicationInfo			= &app_info;
		instance_create_info.enabledExtensionCount		= uint(instance_extensions.size());
		instance_create_info.ppEnabledExtensionNames	= instance_extensions.data();
		instance_create_info.enabledLayerCount			= uint(instance_layers.size());
		instance_create_info.ppEnabledLayerNames		= instance_layers.data();

		VK_CHECK( vkCreateInstance( &instance_create_info, nullptr, OUT &instance ));

		VulkanLoader::LoadInstance( instance );
	}
	
	// choose physical device
	{
		uint						count	= 0;
		vector< VkPhysicalDevice >	devices;
		
		VK_CALL( vkEnumeratePhysicalDevices( instance, OUT &count, nullptr ));
		CHECK_ERR( count > 0 );

		devices.resize( count );
		VK_CALL( vkEnumeratePhysicalDevices( instance, OUT &count, OUT devices.data() ));

		physicalDevice = devices[0];
	}

	// find queue
	{
		vector< VkQueueFamilyProperties >	queue_family_props;
		
		uint	count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties( physicalDevice, OUT &count, nullptr );
		CHECK_ERR( count > 0 );
		
		queue_family_props.resize( count );
		vkGetPhysicalDeviceQueueFamilyProperties( physicalDevice, OUT &count, OUT queue_family_props.data() );

		for (size_t i = 0; i < queue_family_props.size(); ++i)
		{
			auto&	q = queue_family_props[i];

			if ( q.queueFlags & VK_QUEUE_GRAPHICS_BIT )
				queueFamily = uint(i);
		}

		CHECK_ERR( queueFamily < queue_family_props.size() );
	}

	// create logical device
	{
		vector< const char* >	device_extensions = {
			#ifdef VK_KHR_shader_clock
				VK_KHR_SHADER_CLOCK_EXTENSION_NAME,
			#endif
			#ifdef VK_NV_mesh_shader
				VK_NV_MESH_SHADER_EXTENSION_NAME,
			#endif
			#ifdef VK_NV_ray_tracing
				VK_NV_RAY_TRACING_EXTENSION_NAME,
			#endif
		};

		VkDeviceCreateInfo		device_info	= {};
		void **					next_ext	= const_cast<void **>( &device_info.pNext );
		device_info.sType		= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		_ValidateDeviceExtensions( INOUT device_extensions );

		if ( not device_extensions.empty() )
		{
			device_info.enabledExtensionCount	= uint(device_extensions.size());
			device_info.ppEnabledExtensionNames	= device_extensions.data();
		}

		VkDeviceQueueCreateInfo	queue_info		= {};
		float					queue_priority	= 1.0f;

		queue_info.sType			= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_info.queueFamilyIndex	= queueFamily;
		queue_info.queueCount		= 1;
		queue_info.pQueuePriorities	= &queue_priority;

		device_info.queueCreateInfoCount = 1;
		device_info.pQueueCreateInfos	 = &queue_info;
		
		VkPhysicalDeviceFeatures2		feat2		= {};
		void **							next_feat	= &feat2.pNext;
		VkPhysicalDeviceProperties2		props2		= {};
		void **							next_props	= &props2.pNext;

		vkGetPhysicalDeviceFeatures( physicalDevice, OUT &deviceFeat );
		device_info.pEnabledFeatures = &deviceFeat;
		
		// setup features
		feat2.sType		= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		props2.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		
		meshShaderFeat  = {};
		meshShaderProps = {};
		shaderClockFeat = {};
		rayTracingProps = {};

		for (string ext : device_extensions)
		{
			if ( ext == VK_NV_MESH_SHADER_EXTENSION_NAME )
			{
				*next_feat				= &meshShaderFeat;
				next_feat				= &meshShaderFeat.pNext;
				meshShaderFeat.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV;

				*next_props				= &meshShaderProps;
				next_props				= &meshShaderProps.pNext;
				meshShaderProps.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_NV;

				hasMeshShader = true;
			}
			else
			if ( ext == VK_KHR_SHADER_CLOCK_EXTENSION_NAME )
			{
				*next_feat				= &shaderClockFeat;
				next_feat				= &shaderClockFeat.pNext;
				shaderClockFeat.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR;

				hasShaderClock = true;
			}
			else
			if ( ext == VK_NV_RAY_TRACING_EXTENSION_NAME )
			{
				*next_props				= &rayTracingProps;
				next_props				= &rayTracingProps.pNext;
				rayTracingProps.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;

				hasRayTracing = true;
			}
		}
		vkGetPhysicalDeviceFeatures2( physicalDevice, OUT &feat2 );

		VK_CHECK( vkCreateDevice( physicalDevice, &device_info, nullptr, OUT &device ));
		
		VulkanLoader::LoadDevice( device, OUT _deviceFnTable );

		vkGetDeviceQueue( device, queueFamily, 0, OUT &queue );
		
		vkGetPhysicalDeviceProperties( physicalDevice, OUT &deviceProps );
		vkGetPhysicalDeviceMemoryProperties( physicalDevice, OUT &_deviceMemoryProperties );
		vkGetPhysicalDeviceProperties2( physicalDevice, OUT &props2 );
	}

	return true;
}

/*
=================================================
	_DestroyDevice
=================================================
*/
void  Device::_DestroyDevice ()
{
	if ( _debugUtilsMessenger )
	{
		vkDestroyDebugUtilsMessengerEXT( instance, _debugUtilsMessenger, nullptr );
		_debugUtilsMessenger = VK_NULL_HANDLE;
	}

	if ( device )
	{
		vkDestroyDevice( device, nullptr );
		device = VK_NULL_HANDLE;
		VulkanLoader::ResetDevice( OUT _deviceFnTable );
	}

	queue			= VK_NULL_HANDLE;
	queueFamily		= ~0u;
	physicalDevice	= VK_NULL_HANDLE;
	hasMeshShader	= false;
	hasShaderClock	= false;
	hasRayTracing	= false;

	if ( instance )
	{
		vkDestroyInstance( instance, nullptr );
		instance = VK_NULL_HANDLE;
		VulkanLoader::Unload();
	}
}

/*
=================================================
	_CreateResources
=================================================
*/
bool  Device::_CreateResources ()
{
	// create command pool
	{
		VkCommandPoolCreateInfo		info = {};
		info.sType				= VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		info.flags				= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		info.queueFamilyIndex	= queueFamily;

		VK_CHECK( vkCreateCommandPool( device, &info, nullptr, OUT &cmdPool ));

		VkCommandBufferAllocateInfo		alloc = {};
		alloc.sType					= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc.commandPool			= cmdPool;
		alloc.level					= VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc.commandBufferCount	= 1;

		VK_CHECK( vkAllocateCommandBuffers( device, &alloc, OUT &cmdBuffer ));
	}

	// create descriptor pool
	{
		VkDescriptorPoolSize	sizes[] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 },
			{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 100 }
		};
		
		if ( rayTracingProps.shaderGroupHandleSize == 0 )
		{
			// if ray-tracing is not supported then change descriptor type for something else
			sizes[3].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		}

		VkDescriptorPoolCreateInfo		info = {};
		info.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		info.maxSets		= 100;
		info.poolSizeCount	= uint(std::size( sizes ));
		info.pPoolSizes		= sizes;

		VK_CHECK( vkCreateDescriptorPool( device, &info, nullptr, OUT &descPool ));
	}
	
	// debug output buffer
	{
		debugOutputSize = min( debugOutputSize, deviceProps.limits.maxStorageBufferRange );
		std::cout <<  "Shader debug output storage buffer size: " << to_string(debugOutputSize) << std::endl;

		VkBufferCreateInfo	info = {};
		info.sType			= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.flags			= 0;
		info.size			= debugOutputSize;
		info.usage			= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		info.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK( vkCreateBuffer( device, &info, nullptr, OUT &debugOutputBuf ));
		
		VkMemoryRequirements	mem_req;
		vkGetBufferMemoryRequirements( device, debugOutputBuf, OUT &mem_req );
		
		// allocate device local memory
		VkMemoryAllocateInfo	alloc_info = {};
		alloc_info.sType			= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize	= mem_req.size;
		CHECK_ERR( GetMemoryTypeIndex( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, OUT alloc_info.memoryTypeIndex ));

		VK_CHECK( vkAllocateMemory( device, &alloc_info, nullptr, OUT &debugOutputMem ));
		VK_CHECK( vkBindBufferMemory( device, debugOutputBuf, debugOutputMem, 0 ));
	}

	// debug output read back buffer
	{
		VkBufferCreateInfo	info = {};
		info.sType			= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.flags			= 0;
		info.size			= VkDeviceSize(debugOutputSize);
		info.usage			= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		info.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK( vkCreateBuffer( device, &info, nullptr, OUT &readBackBuf ));
		
		VkMemoryRequirements	mem_req;
		vkGetBufferMemoryRequirements( device, readBackBuf, OUT &mem_req );
		
		// allocate host visible memory
		VkMemoryAllocateInfo	alloc_info = {};
		alloc_info.sType			= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize	= mem_req.size;
		CHECK_ERR( GetMemoryTypeIndex( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
									   OUT alloc_info.memoryTypeIndex ));

		VK_CHECK( vkAllocateMemory( device, &alloc_info, nullptr, OUT &readBackMem ));
		VK_CHECK( vkMapMemory( device, readBackMem, 0, info.size, 0, OUT &readBackPtr ));

		VK_CHECK( vkBindBufferMemory( device, readBackBuf, readBackMem, 0 ));
	}
	return true;
}

/*
=================================================
	_DestroyResources
=================================================
*/
void  Device::_DestroyResources ()
{
	if ( cmdPool )
	{
		vkDestroyCommandPool( device, cmdPool, nullptr );
		cmdPool		= VK_NULL_HANDLE;
		cmdBuffer	= VK_NULL_HANDLE;
	}

	if ( descPool )
	{
		vkDestroyDescriptorPool( device, descPool, nullptr );
		descPool = VK_NULL_HANDLE;
	}

	if ( debugOutputBuf )
	{
		vkDestroyBuffer( device, debugOutputBuf, nullptr );
		debugOutputBuf = VK_NULL_HANDLE;
	}

	if ( readBackBuf )
	{
		vkDestroyBuffer( device, readBackBuf, nullptr );
		readBackBuf = VK_NULL_HANDLE;
	}

	if ( debugOutputMem )
	{
		vkFreeMemory( device, debugOutputMem, nullptr );
		debugOutputMem = VK_NULL_HANDLE;
	}

	if ( readBackMem )
	{
		vkFreeMemory( device, readBackMem, nullptr );
		readBackMem = VK_NULL_HANDLE;
	}
}

/*
=================================================
	GetMemoryTypeIndex
=================================================
*/
bool  Device::GetMemoryTypeIndex (uint memoryTypeBits, VkMemoryPropertyFlags flags, OUT uint &memoryTypeIndex) const
{
	memoryTypeIndex = ~0u;

	for (uint i = 0; i < _deviceMemoryProperties.memoryTypeCount; ++i)
	{
		const auto&		mem_type = _deviceMemoryProperties.memoryTypes[i];

		if ( ((memoryTypeBits >> i) & 1) == 1			and
			 (mem_type.propertyFlags & flags) == flags )
		{
			memoryTypeIndex = i;
			return true;
		}
	}
	return false;
}

/*
=================================================
	CheckErrors
=================================================
*/
bool  Device::CheckErrors (VkResult errCode, const char *vkcall, const char *func, const char *file, int line)
{
	if ( errCode == VK_SUCCESS )
		return true;

	#define VK1_CASE_ERR( _code_ ) \
		case _code_ :	msg += #_code_;  break;
		
	string	msg( "Vulkan error: " );

	switch ( errCode )
	{
		VK1_CASE_ERR( VK_NOT_READY )
		VK1_CASE_ERR( VK_TIMEOUT )
		VK1_CASE_ERR( VK_EVENT_SET )
		VK1_CASE_ERR( VK_EVENT_RESET )
		VK1_CASE_ERR( VK_INCOMPLETE )
		VK1_CASE_ERR( VK_ERROR_OUT_OF_HOST_MEMORY )
		VK1_CASE_ERR( VK_ERROR_OUT_OF_DEVICE_MEMORY )
		VK1_CASE_ERR( VK_ERROR_INITIALIZATION_FAILED )
		VK1_CASE_ERR( VK_ERROR_DEVICE_LOST )
		VK1_CASE_ERR( VK_ERROR_MEMORY_MAP_FAILED )
		VK1_CASE_ERR( VK_ERROR_LAYER_NOT_PRESENT )
		VK1_CASE_ERR( VK_ERROR_EXTENSION_NOT_PRESENT )
		VK1_CASE_ERR( VK_ERROR_FEATURE_NOT_PRESENT )
		VK1_CASE_ERR( VK_ERROR_INCOMPATIBLE_DRIVER )
		VK1_CASE_ERR( VK_ERROR_TOO_MANY_OBJECTS )
		VK1_CASE_ERR( VK_ERROR_FORMAT_NOT_SUPPORTED )
		VK1_CASE_ERR( VK_ERROR_FRAGMENTED_POOL )
		VK1_CASE_ERR( VK_ERROR_SURFACE_LOST_KHR )
		VK1_CASE_ERR( VK_ERROR_NATIVE_WINDOW_IN_USE_KHR )
		VK1_CASE_ERR( VK_SUBOPTIMAL_KHR )
		VK1_CASE_ERR( VK_ERROR_OUT_OF_DATE_KHR )
		VK1_CASE_ERR( VK_ERROR_INCOMPATIBLE_DISPLAY_KHR )
		VK1_CASE_ERR( VK_ERROR_VALIDATION_FAILED_EXT )
		VK1_CASE_ERR( VK_ERROR_INVALID_SHADER_NV )
		VK1_CASE_ERR( VK_ERROR_OUT_OF_POOL_MEMORY )
		VK1_CASE_ERR( VK_ERROR_INVALID_EXTERNAL_HANDLE )
		VK1_CASE_ERR( VK_ERROR_FRAGMENTATION_EXT )
		VK1_CASE_ERR( VK_ERROR_NOT_PERMITTED_EXT )
		VK1_CASE_ERR( VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT )
		VK1_CASE_ERR( VK_ERROR_INVALID_DEVICE_ADDRESS_EXT )
		VK1_CASE_ERR( VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT )
		VK1_CASE_ERR( VK_ERROR_UNKNOWN )
		case VK_SUCCESS :
		case VK_RESULT_MAX_ENUM :
		default :	msg = msg + "unknown (" + to_string(int(errCode)) + ')';  break;
	}
	#undef VK1_CASE_ERR
		
	msg = msg + ", in " + vkcall + ", function: " + func;

	std::cout << msg << ", file: " << file << ", line: " << line << std::endl;
	return false;
}

/*
=================================================
	_ValidateInstanceVersion
=================================================
*/
void  Device::_ValidateInstanceVersion (INOUT uint &version) const
{
	const uint	min_ver		= VK_API_VERSION_1_0;
	uint		current_ver	= 0;

	VK_CALL( vkEnumerateInstanceVersion( OUT &current_ver ));

	version = min( version, max( min_ver, current_ver ));
}

/*
=================================================
	_ValidateInstanceLayers
=================================================
*/
void  Device::_ValidateInstanceLayers (INOUT vector<const char*> &layers) const
{
	vector<VkLayerProperties> inst_layers;

	// load supported layers
	uint	count = 0;
	VK_CALL( vkEnumerateInstanceLayerProperties( OUT &count, nullptr ));

	if ( count == 0 )
	{
		layers.clear();
		return;
	}

	inst_layers.resize( count );
	VK_CALL( vkEnumerateInstanceLayerProperties( OUT &count, OUT inst_layers.data() ));


	// validate
	for (auto iter = layers.begin(); iter != layers.end();)
	{
		bool	found = false;

		for (auto& prop : inst_layers)
		{
			if ( string(*iter) == prop.layerName ) {
				found = true;
				break;
			}
		}

		if ( not found )
		{
			std::cout << "Vulkan layer \"" << (*iter) << "\" not supported and will be removed" << std::endl;

			iter = layers.erase( iter );
		}
		else
			++iter;
	}
}

/*
=================================================
	_ValidateInstanceExtensions
=================================================
*/
void  Device::_ValidateInstanceExtensions (INOUT vector<const char*> &extensions) const
{
	unordered_set<string>	instance_extensions;


	// load supported extensions
	uint	count = 0;
	VK_CALL( vkEnumerateInstanceExtensionProperties( nullptr, OUT &count, nullptr ));

	if ( count == 0 )
	{
		extensions.clear();
		return;
	}

	vector< VkExtensionProperties >		inst_ext;
	inst_ext.resize( count );

	VK_CALL( vkEnumerateInstanceExtensionProperties( nullptr, OUT &count, OUT inst_ext.data() ));

	for (auto& ext : inst_ext) {
		instance_extensions.insert( string(ext.extensionName) );
	}


	// validate
	for (auto iter = extensions.begin(); iter != extensions.end();)
	{
		if ( instance_extensions.find( string{*iter} ) == instance_extensions.end() )
		{
			std::cout << "Vulkan instance extension \"" << (*iter) << "\" not supported and will be removed" << std::endl;

			iter = extensions.erase( iter );
		}
		else
			++iter;
	}
}
	
/*
=================================================
	_ValidateDeviceExtensions
=================================================
*/
void  Device::_ValidateDeviceExtensions (INOUT vector<const char*> &extensions) const
{
	// load supported device extensions
	uint	count = 0;
	VK_CALL( vkEnumerateDeviceExtensionProperties( physicalDevice, nullptr, OUT &count, nullptr ));

	if ( count == 0 )
	{
		extensions.clear();
		return;
	}

	vector< VkExtensionProperties >	dev_ext;
	dev_ext.resize( count );

	VK_CALL( vkEnumerateDeviceExtensionProperties( physicalDevice, nullptr, OUT &count, OUT dev_ext.data() ));


	// validate
	for (auto iter = extensions.begin(); iter != extensions.end();)
	{
		bool	found = false;

		for (auto& ext : dev_ext)
		{
			if ( string(*iter) == ext.extensionName )
			{
				found = true;
				break;
			}
		}

		if ( not found )
		{
			std::cout << "Vulkan device extension \"" << (*iter) << "\" not supported and will be removed" << std::endl;

			iter = extensions.erase( iter );
		}
		else
			++iter;
	}
}

/*
=================================================
	Compile
=================================================
*/
bool  Device::Compile  (OUT VkShaderModule &		shaderModule,
						vector<const char *>		source,
						EShLanguage					shaderType,
						ETraceMode					mode,
						uint						dbgBufferSetIndex,
						EShTargetLanguageVersion	spvVersion)
{
	vector<const char *>	shader_src;
	const bool				debuggable	= dbgBufferSetIndex != ~0u;
	unique_ptr<ShaderTrace>	debug_info	{ debuggable ? new ShaderTrace{} : nullptr };
	const string			header		= "#version 460 core\n"
										  "#extension GL_ARB_separate_shader_objects : require\n"
										  "#extension GL_ARB_shading_language_420pack : require\n";
	
	shader_src.push_back( header.data() );
	shader_src.insert( shader_src.end(), source.begin(), source.end() );

	if ( not _Compile( OUT _tempBuf, OUT debug_info.get(), dbgBufferSetIndex, shader_src, shaderType, mode, spvVersion ))
		return false;

	VkShaderModuleCreateInfo	info = {};
	info.sType		= VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.flags		= 0;
	info.pCode		= _tempBuf.data();
	info.codeSize	= sizeof(_tempBuf[0]) * _tempBuf.size();

	VK_CHECK( vkCreateShaderModule( device, &info, nullptr, OUT &shaderModule ));
	tempHandles.emplace_back( EHandleType::Shader, uint64_t(shaderModule) );

	if ( debuggable ) {
		_debuggableShaders.insert_or_assign( shaderModule, debug_info.release() );
	}
	return true;
}

/*
=================================================
	_Compile
=================================================
*/
bool  Device::_Compile (OUT vector<uint>&			spirvData,
						OUT ShaderTrace*			dbgInfo,
						uint						dbgBufferSetIndex,
						vector<const char *>		source,
						EShLanguage					shaderType,
						ETraceMode					mode,
						EShTargetLanguageVersion	spvVersion)
{
	EShMessages				messages		= EShMsgDefault;
	TProgram				program;
	TShader					shader			{ shaderType };
	EshTargetClientVersion	client_version	= EShTargetVulkan_1_1;
	TBuiltInResource		builtin_res		= DefaultTBuiltInResource;

	shader.setStrings( source.data(), int(source.size()) );
	shader.setEntryPoint( "main" );
	shader.setEnvInput( EShSourceGlsl, shaderType, EShClientVulkan, 110 );
	shader.setEnvClient( EShClientVulkan, client_version );
	shader.setEnvTarget( EshTargetSpv, spvVersion );

	if ( not shader.parse( &builtin_res, 460, ECoreProfile, false, true, messages ))
	{
		std::cout << shader.getInfoLog() << std::endl;
		return false;
	}
		
	program.addShader( &shader );

	if ( not program.link( messages ) )
	{
		std::cout << program.getInfoLog() << std::endl;
		return false;
	}

	TIntermediate*	intermediate = program.getIntermediate( shader.getStage() );
	if ( not intermediate )
		return false;

	if ( dbgInfo )
	{
		BEGIN_ENUM_CHECKS();
		switch ( mode )
		{
			case ETraceMode::DebugTrace :
				CHECK_ERR( dbgInfo->InsertTraceRecording( INOUT *intermediate, dbgBufferSetIndex ));
				break;

			case ETraceMode::Performance :
				CHECK_ERR( dbgInfo->InsertFunctionProfiler( INOUT *intermediate, dbgBufferSetIndex,
														    shaderClockFeat.shaderSubgroupClock,
														    shaderClockFeat.shaderDeviceClock ));
				break;
				
			case ETraceMode::TimeMap :
				CHECK_ERR( shaderClockFeat.shaderDeviceClock );
				CHECK_ERR( dbgInfo->InsertShaderClockHeatmap( INOUT *intermediate, dbgBufferSetIndex ));
				break;

			case ETraceMode::None :
			default :
				RETURN_ERR( "unknown shader trace mode" );
		}
		END_ENUM_CHECKS();
	
		dbgInfo->SetSource( source.data(), nullptr, source.size() );
	}

	SpvOptions				spv_options;
	spv::SpvBuildLogger		logger;

	spv_options.generateDebugInfo	= false;
	spv_options.disableOptimizer	= true;
	spv_options.optimizeSize		= false;
	spv_options.validate			= false;
		
	spirvData.clear();
	GlslangToSpv( *intermediate, OUT spirvData, &logger, &spv_options );

	CHECK_ERR( spirvData.size() );
	
	// for debugging
	#if 0 //def ENABLE_SPIRV_CROSS
	{
		spirv_cross::CompilerGLSL			compiler {spirvData.data(), spirvData.size()};
		spirv_cross::CompilerGLSL::Options	opt = {};

		opt.version						= 460;
		opt.es							= false;
		opt.vulkan_semantics			= true;
		opt.separate_shader_objects		= true;
		opt.enable_420pack_extension	= true;

		opt.vertex.fixup_clipspace		= false;
		opt.vertex.flip_vert_y			= false;
		opt.vertex.support_nonzero_base_instance = true;

		opt.fragment.default_float_precision	= spirv_cross::CompilerGLSL::Options::Precision::Highp;
		opt.fragment.default_int_precision		= spirv_cross::CompilerGLSL::Options::Precision::Highp;

		compiler.set_common_options(opt);

		std::string	glsl_src = compiler.compile();
		std::cout << glsl_src << std::endl;
	}
	#endif
	return true;
}

/*
=================================================
	_GetDebugOutput
=================================================
*/
bool  Device::_GetDebugOutput (VkShaderModule shaderModule, const void *ptr, VkDeviceSize maxSize, OUT vector<string> &result) const
{
	auto	iter = _debuggableShaders.find( shaderModule );
	CHECK_ERR( iter != _debuggableShaders.end() );

	return iter->second->ParseShaderTrace( ptr, maxSize, OUT result );
}

/*
=================================================
	CreateDebugDescSetLayout
=================================================
*/
bool  Device::CreateDebugDescriptorSet (VkShaderStageFlags stages, OUT VkDescriptorSetLayout &dsLayout, OUT VkDescriptorSet &descSet)
{
	// create layout
	{
		VkDescriptorSetLayoutBinding	binding = {};
		binding.binding			= 0;
		binding.descriptorType	= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		binding.descriptorCount	= 1;
		binding.stageFlags		= stages;

		VkDescriptorSetLayoutCreateInfo		info = {};
		info.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		info.bindingCount	= 1;
		info.pBindings		= &binding;

		VK_CHECK( vkCreateDescriptorSetLayout( device, &info, nullptr, OUT &dsLayout ));
		tempHandles.emplace_back( EHandleType::DescriptorSetLayout, uint64_t(dsLayout) );
	}
	
	// allocate descriptor set
	{
		VkDescriptorSetAllocateInfo		info = {};
		info.sType				= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		info.descriptorPool		= descPool;
		info.descriptorSetCount	= 1;
		info.pSetLayouts		= &dsLayout;

		VK_CHECK( vkAllocateDescriptorSets( device, &info, OUT &descSet ));
	}

	// update descriptor set
	{
		VkDescriptorBufferInfo	buffer	= {};
		buffer.buffer	= debugOutputBuf;
		buffer.range	= VK_WHOLE_SIZE;
		
		VkWriteDescriptorSet	write	= {};
		write.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet			= descSet;
		write.dstBinding		= 0;
		write.descriptorCount	= 1;
		write.descriptorType	= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write.pBufferInfo		= &buffer;
		
		vkUpdateDescriptorSets( device, 1, &write, 0, nullptr );
	}
	return true;
}

/*
=================================================
	CreateRenderTarget
=================================================
*/
bool  Device::CreateRenderTarget (VkFormat colorFormat, uint width, uint height, VkImageUsageFlags imageUsage,
								  OUT VkRenderPass &outRenderPass, OUT VkImage &outImage,
								  OUT VkFramebuffer &outFramebuffer)
{
	// create image
	{
		VkImageCreateInfo	info = {};
		info.sType			= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		info.flags			= 0;
		info.imageType		= VK_IMAGE_TYPE_2D;
		info.format			= colorFormat;
		info.extent			= { width, height, 1 };
		info.mipLevels		= 1;
		info.arrayLayers	= 1;
		info.samples		= VK_SAMPLE_COUNT_1_BIT;
		info.tiling			= VK_IMAGE_TILING_OPTIMAL;
		info.usage			= imageUsage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		info.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;
		info.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK( vkCreateImage( device, &info, nullptr, OUT &outImage ));
		tempHandles.emplace_back( EHandleType::Image, uint64_t(outImage) );

		VkMemoryRequirements	mem_req;
		vkGetImageMemoryRequirements( device, outImage, OUT &mem_req );
		
		// allocate device local memory
		VkMemoryAllocateInfo	alloc_info = {};
		alloc_info.sType			= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize	= mem_req.size;
		CHECK_ERR( GetMemoryTypeIndex( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, OUT alloc_info.memoryTypeIndex ));

		VkDeviceMemory	image_mem;
		VK_CHECK( vkAllocateMemory( device, &alloc_info, nullptr, OUT &image_mem ));
		tempHandles.emplace_back( EHandleType::Memory, uint64_t(image_mem) );

		VK_CHECK( vkBindImageMemory( device, outImage, image_mem, 0 ));
	}

	// create image view
	VkImageView		view = VK_NULL_HANDLE;
	{
		VkImageViewCreateInfo	info = {};
		info.sType				= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		info.flags				= 0;
		info.image				= outImage;
		info.viewType			= VK_IMAGE_VIEW_TYPE_2D;
		info.format				= colorFormat;
		info.components			= { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		info.subresourceRange	= { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		VK_CHECK( vkCreateImageView( device, &info, nullptr, OUT &view ));
		tempHandles.emplace_back( EHandleType::ImageView, uint64_t(view) );
	}

	// create renderpass
	{
		// setup attachment
		VkAttachmentDescription		attachments[1] = {};

		attachments[0].format			= colorFormat;
		attachments[0].samples			= VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout	= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[0].finalLayout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


		// setup subpasses
		VkSubpassDescription	subpasses[1]		= {};
		VkAttachmentReference	attachment_ref[1]	= {};

		attachment_ref[0] = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		subpasses[0].pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpasses[0].colorAttachmentCount	= 1;
		subpasses[0].pColorAttachments		= &attachment_ref[0];


		// setup dependencies
		VkSubpassDependency		dependencies[2] = {};

		dependencies[0].srcSubpass		= VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass		= 0;
		dependencies[0].srcStageMask	= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask	= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask	= VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags	= VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass		= 0;
		dependencies[1].dstSubpass		= VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask	= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask	= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask	= 0;
		dependencies[1].dependencyFlags	= VK_DEPENDENCY_BY_REGION_BIT;


		// setup create info
		VkRenderPassCreateInfo	info = {};
		info.sType				= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		info.flags				= 0;
		info.attachmentCount	= uint(std::size( attachments ));
		info.pAttachments		= attachments;
		info.subpassCount		= uint(std::size( subpasses ));
		info.pSubpasses			= subpasses;
		info.dependencyCount	= uint(std::size( dependencies ));
		info.pDependencies		= dependencies;

		VK_CHECK( vkCreateRenderPass( device, &info, nullptr, OUT &outRenderPass ));
		tempHandles.emplace_back( EHandleType::RenderPass, uint64_t(outRenderPass) );
	}
	
	// create framebuffer
	{
		VkFramebufferCreateInfo		info = {};
		info.sType				= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		info.flags				= 0;
		info.renderPass			= outRenderPass;
		info.attachmentCount	= 1;
		info.pAttachments		= &view;
		info.width				= width;
		info.height				= height;
		info.layers				= 1;

		VK_CHECK( vkCreateFramebuffer( device, &info, nullptr, OUT &outFramebuffer ));
		tempHandles.emplace_back( EHandleType::Framebuffer, uint64_t(outFramebuffer) );
	}
	return true;
}

/*
=================================================
	CreateGraphicsPipelineVar1
=================================================
*/
bool  Device::CreateGraphicsPipelineVar1 (VkShaderModule vertShader, VkShaderModule fragShader,
										  VkDescriptorSetLayout dsLayout, VkRenderPass renderPass,
										  OUT VkPipelineLayout &outPipelineLayout, OUT VkPipeline &outPipeline)
{
	// create pipeline layout
	{
		VkPipelineLayoutCreateInfo	info = {};
		info.sType					= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		info.setLayoutCount			= 1;
		info.pSetLayouts			= &dsLayout;
		info.pushConstantRangeCount	= 0;
		info.pPushConstantRanges	= nullptr;

		VK_CHECK( vkCreatePipelineLayout( device, &info, nullptr, OUT &outPipelineLayout ));
		tempHandles.emplace_back( EHandleType::PipelineLayout, uint64_t(outPipelineLayout) );
	}

	VkPipelineShaderStageCreateInfo			stages[2] = {};
	stages[0].sType		= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage		= VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module	= vertShader;
	stages[0].pName		= "main";
	stages[1].sType		= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage		= VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module	= fragShader;
	stages[1].pName		= "main";

	VkPipelineVertexInputStateCreateInfo	vertex_input = {};
	vertex_input.sType		= VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	
	VkPipelineInputAssemblyStateCreateInfo	input_assembly = {};
	input_assembly.sType	= VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology	= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	VkPipelineViewportStateCreateInfo		viewport = {};
	viewport.sType			= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport.viewportCount	= 1;
	viewport.scissorCount	= 1;

	VkPipelineRasterizationStateCreateInfo	rasterization = {};
	rasterization.sType			= VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization.polygonMode	= VK_POLYGON_MODE_FILL;
	rasterization.cullMode		= VK_CULL_MODE_NONE;
	rasterization.frontFace		= VK_FRONT_FACE_COUNTER_CLOCKWISE;

	VkPipelineMultisampleStateCreateInfo	multisample = {};
	multisample.sType					= VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples	= VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo	depth_stencil = {};
	depth_stencil.sType					= VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable		= VK_FALSE;
	depth_stencil.depthWriteEnable		= VK_FALSE;
	depth_stencil.depthCompareOp		= VK_COMPARE_OP_LESS_OR_EQUAL;
	depth_stencil.depthBoundsTestEnable	= VK_FALSE;
	depth_stencil.stencilTestEnable		= VK_FALSE;

	VkPipelineColorBlendAttachmentState		blend_attachment = {};
	blend_attachment.blendEnable		= VK_FALSE;
	blend_attachment.colorWriteMask		= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo		blend_state = {};
	blend_state.sType				= VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.logicOpEnable		= VK_FALSE;
	blend_state.attachmentCount		= 1;
	blend_state.pAttachments		= &blend_attachment;

	VkDynamicState							dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo		dynamic_state = {};
	dynamic_state.sType				= VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount	= uint(std::size( dynamic_states ));
	dynamic_state.pDynamicStates	= dynamic_states;

	// create pipeline
	VkGraphicsPipelineCreateInfo	info = {};
	info.sType					= VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	info.stageCount				= uint(std::size( stages ));
	info.pStages				= stages;
	info.pViewportState			= &viewport;
	info.pVertexInputState		= &vertex_input;
	info.pInputAssemblyState	= &input_assembly;
	info.pRasterizationState	= &rasterization;
	info.pMultisampleState		= &multisample;
	info.pDepthStencilState		= &depth_stencil;
	info.pColorBlendState		= &blend_state;
	info.pDynamicState			= &dynamic_state;
	info.layout					= outPipelineLayout;
	info.renderPass				= renderPass;
	info.subpass				= 0;

	VK_CHECK( vkCreateGraphicsPipelines( device, VK_NULL_HANDLE, 1, &info, nullptr, OUT &outPipeline ));
	tempHandles.emplace_back( EHandleType::Pipeline, uint64_t(outPipeline) );

	return true;
}

/*
=================================================
	CreateMeshPipelineVar1
=================================================
*/
bool  Device::CreateGraphicsPipelineVar2 (VkShaderModule vertShader, VkShaderModule tessContShader, VkShaderModule tessEvalShader,
										  VkShaderModule fragShader, VkDescriptorSetLayout dsLayout, VkRenderPass renderPass, uint patchSize,
										  OUT VkPipelineLayout &outPipelineLayout, OUT VkPipeline &outPipeline)
{
	// create pipeline layout
	{
		VkPipelineLayoutCreateInfo	info = {};
		info.sType					= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		info.setLayoutCount			= 1;
		info.pSetLayouts			= &dsLayout;
		info.pushConstantRangeCount	= 0;
		info.pPushConstantRanges	= nullptr;

		VK_CHECK( vkCreatePipelineLayout( device, &info, nullptr, OUT &outPipelineLayout ));
		tempHandles.emplace_back( EHandleType::PipelineLayout, uint64_t(outPipelineLayout) );
	}

	VkPipelineShaderStageCreateInfo			stages[4] = {};
	stages[0].sType		= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage		= VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module	= vertShader;
	stages[0].pName		= "main";
	stages[1].sType		= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage		= VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module	= fragShader;
	stages[1].pName		= "main";
	stages[2].sType		= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[2].stage		= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
	stages[2].module	= tessContShader;
	stages[2].pName		= "main";
	stages[3].sType		= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[3].stage		= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
	stages[3].module	= tessEvalShader;
	stages[3].pName		= "main";

	VkPipelineVertexInputStateCreateInfo	vertex_input = {};
	vertex_input.sType		= VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	
	VkPipelineInputAssemblyStateCreateInfo	input_assembly = {};
	input_assembly.sType	= VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology	= VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;

	VkPipelineViewportStateCreateInfo		viewport = {};
	viewport.sType			= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport.viewportCount	= 1;
	viewport.scissorCount	= 1;

	VkPipelineRasterizationStateCreateInfo	rasterization = {};
	rasterization.sType			= VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization.polygonMode	= VK_POLYGON_MODE_FILL;
	rasterization.cullMode		= VK_CULL_MODE_NONE;
	rasterization.frontFace		= VK_FRONT_FACE_COUNTER_CLOCKWISE;

	VkPipelineMultisampleStateCreateInfo	multisample = {};
	multisample.sType					= VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples	= VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo	depth_stencil = {};
	depth_stencil.sType					= VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable		= VK_FALSE;
	depth_stencil.depthWriteEnable		= VK_FALSE;
	depth_stencil.depthCompareOp		= VK_COMPARE_OP_LESS_OR_EQUAL;
	depth_stencil.depthBoundsTestEnable	= VK_FALSE;
	depth_stencil.stencilTestEnable		= VK_FALSE;

	VkPipelineColorBlendAttachmentState		blend_attachment = {};
	blend_attachment.blendEnable		= VK_FALSE;
	blend_attachment.colorWriteMask		= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo		blend_state = {};
	blend_state.sType				= VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.logicOpEnable		= VK_FALSE;
	blend_state.attachmentCount		= 1;
	blend_state.pAttachments		= &blend_attachment;

	VkDynamicState							dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo		dynamic_state = {};
	dynamic_state.sType				= VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount	= uint(std::size( dynamic_states ));
	dynamic_state.pDynamicStates	= dynamic_states;

	VkPipelineTessellationStateCreateInfo	tess_state = {};
	tess_state.sType				= VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	tess_state.patchControlPoints	= patchSize;

	// create pipeline
	VkGraphicsPipelineCreateInfo	info = {};
	info.sType					= VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	info.stageCount				= uint(std::size( stages ));
	info.pStages				= stages;
	info.pViewportState			= &viewport;
	info.pVertexInputState		= &vertex_input;
	info.pInputAssemblyState	= &input_assembly;
	info.pTessellationState		= &tess_state;
	info.pRasterizationState	= &rasterization;
	info.pMultisampleState		= &multisample;
	info.pDepthStencilState		= &depth_stencil;
	info.pColorBlendState		= &blend_state;
	info.pDynamicState			= &dynamic_state;
	info.layout					= outPipelineLayout;
	info.renderPass				= renderPass;
	info.subpass				= 0;

	VK_CHECK( vkCreateGraphicsPipelines( device, VK_NULL_HANDLE, 1, &info, nullptr, OUT &outPipeline ));
	tempHandles.emplace_back( EHandleType::Pipeline, uint64_t(outPipeline) );

	return true;
}

/*
=================================================
	CreateMeshPipelineVar1
=================================================
*/
bool  Device::CreateMeshPipelineVar1 (VkShaderModule meshShader, VkShaderModule fragShader,
									  VkDescriptorSetLayout dsLayout, VkRenderPass renderPass,
									  OUT VkPipelineLayout &outPipelineLayout, OUT VkPipeline &outPipeline)
{
	// create pipeline layout
	{
		VkPipelineLayoutCreateInfo	info = {};
		info.sType					= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		info.setLayoutCount			= 1;
		info.pSetLayouts			= &dsLayout;
		info.pushConstantRangeCount	= 0;
		info.pPushConstantRanges	= nullptr;

		VK_CHECK( vkCreatePipelineLayout( device, &info, nullptr, OUT &outPipelineLayout ));
		tempHandles.emplace_back( EHandleType::PipelineLayout, uint64_t(outPipelineLayout) );
	}

	VkPipelineShaderStageCreateInfo			stages[2] = {};
	stages[0].sType		= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage		= VK_SHADER_STAGE_MESH_BIT_NV;
	stages[0].module	= meshShader;
	stages[0].pName		= "main";
	stages[1].sType		= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage		= VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module	= fragShader;
	stages[1].pName		= "main";

	VkPipelineVertexInputStateCreateInfo	vertex_input = {};
	vertex_input.sType		= VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	
	VkPipelineInputAssemblyStateCreateInfo	input_assembly = {};
	input_assembly.sType	= VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology	= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineViewportStateCreateInfo		viewport = {};
	viewport.sType			= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport.viewportCount	= 1;
	viewport.scissorCount	= 1;

	VkPipelineRasterizationStateCreateInfo	rasterization = {};
	rasterization.sType			= VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization.polygonMode	= VK_POLYGON_MODE_FILL;
	rasterization.cullMode		= VK_CULL_MODE_NONE;
	rasterization.frontFace		= VK_FRONT_FACE_COUNTER_CLOCKWISE;

	VkPipelineMultisampleStateCreateInfo	multisample = {};
	multisample.sType					= VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples	= VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo	depth_stencil = {};
	depth_stencil.sType					= VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable		= VK_FALSE;
	depth_stencil.depthWriteEnable		= VK_FALSE;
	depth_stencil.depthCompareOp		= VK_COMPARE_OP_LESS_OR_EQUAL;
	depth_stencil.depthBoundsTestEnable	= VK_FALSE;
	depth_stencil.stencilTestEnable		= VK_FALSE;

	VkPipelineColorBlendAttachmentState		blend_attachment = {};
	blend_attachment.blendEnable		= VK_FALSE;
	blend_attachment.colorWriteMask		= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo		blend_state = {};
	blend_state.sType				= VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.logicOpEnable		= VK_FALSE;
	blend_state.attachmentCount		= 1;
	blend_state.pAttachments		= &blend_attachment;

	VkDynamicState							dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo		dynamic_state = {};
	dynamic_state.sType				= VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount	= uint(std::size( dynamic_states ));
	dynamic_state.pDynamicStates	= dynamic_states;

	// create pipeline
	VkGraphicsPipelineCreateInfo	info = {};
	info.sType					= VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	info.stageCount				= uint(std::size( stages ));
	info.pStages				= stages;
	info.pViewportState			= &viewport;
	info.pVertexInputState		= &vertex_input;		// ignored in mesh shader
	info.pInputAssemblyState	= &input_assembly;
	info.pRasterizationState	= &rasterization;
	info.pMultisampleState		= &multisample;
	info.pDepthStencilState		= &depth_stencil;
	info.pColorBlendState		= &blend_state;
	info.pDynamicState			= &dynamic_state;
	info.layout					= outPipelineLayout;
	info.renderPass				= renderPass;
	info.subpass				= 0;

	VK_CHECK( vkCreateGraphicsPipelines( device, VK_NULL_HANDLE, 1, &info, nullptr, OUT &outPipeline ));
	tempHandles.emplace_back( EHandleType::Pipeline, uint64_t(outPipeline) );

	return true;
}

/*
=================================================
	CreateRayTracingScene
=================================================
*/
bool  Device::CreateRayTracingScene (VkPipeline rtPipeline, uint numGroups, OUT VkBuffer &shaderBindingTable,
									 OUT VkAccelerationStructureNV &topLevelAS, OUT VkAccelerationStructureNV &bottomLevelAS)
{
	struct VkGeometryInstance
	{
		// 4x3 row-major matrix
		float4		transformRow0;
		float4		transformRow1;
		float4		transformRow2;

		uint		instanceId		: 24;
		uint		mask			: 8;
		uint		instanceOffset	: 24;
		uint		flags			: 8;
		uint64_t	accelerationStructureHandle;
	};

	struct MemInfo
	{
		VkDeviceSize			totalSize		= 0;
		uint					memTypeBits		= 0;
		VkMemoryPropertyFlags	memProperty		= 0;
	};

	struct ResourceInit
	{
		using BindMemCallbacks_t	= vector< std::function<bool (void *)> >;
		using DrawCallbacks_t		= vector< std::function<void (VkCommandBuffer)> >;

		MemInfo					host;
		MemInfo					dev;
		BindMemCallbacks_t		onBind;
		DrawCallbacks_t			onUpdate;
	};

	static const float3		vertices[] = {
		{ 0.25f, 0.25f, 0.0f },
		{ 0.75f, 0.25f, 0.0f },
		{ 0.50f, 0.75f, 0.0f }
	};
	static const uint		indices[] = {
		0, 1, 2
	};

	VkBuffer		vertex_buffer;
	VkBuffer		index_buffer;
	VkBuffer		instance_buffer;
	VkBuffer		scratch_buffer;
	VkDeviceMemory	host_memory;
	uint64_t		bottom_level_as_handle = 0;
	VkDeviceMemory	dev_memory;

	ResourceInit	res;
	res.dev.memProperty = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	res.host.memProperty = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	// create vertex buffer
	{
		VkBufferCreateInfo	info = {};
		info.sType			= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.flags			= 0;
		info.size			= sizeof(vertices);
		info.usage			= VK_BUFFER_USAGE_RAY_TRACING_BIT_NV;
		info.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK( vkCreateBuffer( device, &info, nullptr, OUT &vertex_buffer ));
		
		VkMemoryRequirements	mem_req;
		vkGetBufferMemoryRequirements( device, vertex_buffer, OUT &mem_req );
		
		VkDeviceSize	offset = AlignToLarger( res.host.totalSize, mem_req.alignment );
		res.host.totalSize		 = offset + mem_req.size;
		res.host.memTypeBits	|= mem_req.memoryTypeBits;

		res.onBind.push_back( [this, &host_memory, vertex_buffer, offset] (void *ptr) -> bool
		{
			std::memcpy( ((char*)ptr) + offset, vertices, sizeof(vertices) );
			VK_CHECK( vkBindBufferMemory( device, vertex_buffer, host_memory, offset ));
			return true;
		});
	}

	// create index buffer
	{
		VkBufferCreateInfo	info = {};
		info.sType			= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.flags			= 0;
		info.size			= sizeof(indices);
		info.usage			= VK_BUFFER_USAGE_RAY_TRACING_BIT_NV;
		info.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK( vkCreateBuffer( device, &info, nullptr, OUT &index_buffer ));
		
		VkMemoryRequirements	mem_req;
		vkGetBufferMemoryRequirements( device, index_buffer, OUT &mem_req );
		
		VkDeviceSize	offset = AlignToLarger( res.host.totalSize, mem_req.alignment );
		res.host.totalSize		 = offset + mem_req.size;
		res.host.memTypeBits	|= mem_req.memoryTypeBits;

		res.onBind.push_back( [this, index_buffer, &host_memory, offset] (void *ptr) -> bool
		{
			std::memcpy( ((char*)ptr) + offset, indices, sizeof(indices) );
			VK_CHECK( vkBindBufferMemory( device, index_buffer, host_memory, offset ));
			return true;
		});
	}

	// create bottom level acceleration structure
	{
		VkGeometryNV	geometry[1] = {};
		geometry[0].sType			= VK_STRUCTURE_TYPE_GEOMETRY_NV;
		geometry[0].geometryType	= VK_GEOMETRY_TYPE_TRIANGLES_NV;
		geometry[0].flags			= VK_GEOMETRY_OPAQUE_BIT_NV;
		geometry[0].geometry.aabbs.sType	= VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
		geometry[0].geometry.triangles.sType		= VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
		geometry[0].geometry.triangles.vertexData	= vertex_buffer;
		geometry[0].geometry.triangles.vertexOffset	= 0;
		geometry[0].geometry.triangles.vertexCount	= uint(std::size( vertices ));
		geometry[0].geometry.triangles.vertexStride	= sizeof(vertices[0]);
		geometry[0].geometry.triangles.vertexFormat	= VK_FORMAT_R32G32B32_SFLOAT;
		geometry[0].geometry.triangles.indexData	= index_buffer;
		geometry[0].geometry.triangles.indexOffset	= 0;
		geometry[0].geometry.triangles.indexCount	= uint(std::size( indices ));
		geometry[0].geometry.triangles.indexType	= VK_INDEX_TYPE_UINT32;

		VkAccelerationStructureCreateInfoNV	createinfo = {};
		createinfo.sType				= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
		createinfo.info.sType			= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
		createinfo.info.type			= VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
		createinfo.info.geometryCount	= uint(std::size( geometry ));
		createinfo.info.pGeometries		= geometry;

		VK_CHECK( vkCreateAccelerationStructureNV( device, &createinfo, nullptr, OUT &bottomLevelAS ));
		tempHandles.emplace_back( EHandleType::AccStruct, uint64_t(bottomLevelAS) );
		
		VkAccelerationStructureMemoryRequirementsInfoNV	mem_info = {};
		mem_info.sType					= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
		mem_info.accelerationStructure	= bottomLevelAS;

		VkMemoryRequirements2	mem_req = {};
		vkGetAccelerationStructureMemoryRequirementsNV( device, &mem_info, OUT &mem_req );
		
		VkDeviceSize	offset = AlignToLarger( res.dev.totalSize, mem_req.memoryRequirements.alignment );
		res.dev.totalSize	 = offset + mem_req.memoryRequirements.size;
		res.dev.memTypeBits	|= mem_req.memoryRequirements.memoryTypeBits;
		
		res.onBind.push_back( [this, bottomLevelAS, &bottom_level_as_handle, &dev_memory, offset] (void *) -> bool
		{
			VkBindAccelerationStructureMemoryInfoNV	bind_info = {};
			bind_info.sType					= VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
			bind_info.accelerationStructure	= bottomLevelAS;
			bind_info.memory				= dev_memory;
			bind_info.memoryOffset			= offset;
			VK_CHECK( vkBindAccelerationStructureMemoryNV( device, 1, &bind_info ));

			VK_CHECK( vkGetAccelerationStructureHandleNV( device, bottomLevelAS, sizeof(bottom_level_as_handle), OUT &bottom_level_as_handle ));
			return true;
		});
		
		res.onUpdate.push_back( [this, bottomLevelAS, geometry, &scratch_buffer] (VkCommandBuffer cmd)
		{
			VkAccelerationStructureInfoNV	info = {};
			info.sType			= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
			info.type			= VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
			info.geometryCount	= uint(std::size( geometry ));
			info.pGeometries	= geometry;

			vkCmdBuildAccelerationStructureNV( cmd, &info,
											   VK_NULL_HANDLE, 0,				// instance
											   VK_FALSE,						// update
											   bottomLevelAS, VK_NULL_HANDLE,	// dst, src
											   scratch_buffer, 0
											 );
		});
	}
	
	// create instance buffer
	{
		VkBufferCreateInfo	info = {};
		info.sType			= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.flags			= 0;
		info.size			= sizeof(VkGeometryInstance);
		info.usage			= VK_BUFFER_USAGE_RAY_TRACING_BIT_NV;
		info.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK( vkCreateBuffer( device, &info, nullptr, OUT &instance_buffer ));
		
		VkMemoryRequirements	mem_req;
		vkGetBufferMemoryRequirements( device, instance_buffer, OUT &mem_req );
		
		VkDeviceSize	offset = AlignToLarger( res.host.totalSize, mem_req.alignment );
		res.host.totalSize		 = offset + mem_req.size;
		res.host.memTypeBits	|= mem_req.memoryTypeBits;

		res.onBind.push_back( [this, &host_memory, &bottom_level_as_handle, instance_buffer, offset] (void *ptr) -> bool
		{
			VkGeometryInstance	instance = {};
			instance.transformRow0	= {1.0f, 0.0f, 0.0f, 0.0f};
			instance.transformRow1	= {0.0f, 1.0f, 0.0f, 0.0f};
			instance.transformRow2	= {0.0f, 0.0f, 1.0f, 0.0f};
			instance.instanceId		= 0;
			instance.mask			= 0xFF;
			instance.instanceOffset	= 0;
			instance.flags			= 0;
			instance.accelerationStructureHandle = bottom_level_as_handle;

			std::memcpy( ((char*)ptr) + offset, &instance, sizeof(instance) );

			VK_CHECK( vkBindBufferMemory( device, instance_buffer, host_memory, offset ));
			return true;
		});
	}

	// create top level acceleration structure
	{
		VkAccelerationStructureCreateInfoNV	createinfo = {};
		createinfo.sType				= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
		createinfo.info.sType			= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
		createinfo.info.type			= VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
		createinfo.info.flags			= 0;
		createinfo.info.instanceCount	= 1;

		VK_CHECK( vkCreateAccelerationStructureNV( device, &createinfo, nullptr, OUT &topLevelAS ));
		tempHandles.emplace_back( EHandleType::AccStruct, uint64_t(topLevelAS) );
		
		VkAccelerationStructureMemoryRequirementsInfoNV	mem_info = {};
		mem_info.sType					= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
		mem_info.type					= VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;
		mem_info.accelerationStructure	= topLevelAS;

		VkMemoryRequirements2	mem_req = {};
		vkGetAccelerationStructureMemoryRequirementsNV( device, &mem_info, OUT &mem_req );

		VkDeviceSize	offset = AlignToLarger( res.dev.totalSize, mem_req.memoryRequirements.alignment );
		res.dev.totalSize	 = offset + mem_req.memoryRequirements.size;
		res.dev.memTypeBits	|= mem_req.memoryRequirements.memoryTypeBits;
		
		res.onBind.push_back( [this, &dev_memory, topLevelAS, offset] (void *) -> bool
		{
			VkBindAccelerationStructureMemoryInfoNV	bind_info = {};
			bind_info.sType					= VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
			bind_info.accelerationStructure	= topLevelAS;
			bind_info.memory				= dev_memory;
			bind_info.memoryOffset			= offset;
			VK_CHECK( vkBindAccelerationStructureMemoryNV( device, 1, &bind_info ));
			return true;
		});

		res.onUpdate.push_back( [this, topLevelAS, instance_buffer, &scratch_buffer] (VkCommandBuffer cmd)
		{
			// write-read memory barrier for 'bottomLevelAS'
			// execution barrier for 'scratchBuffer'
			VkMemoryBarrier		barrier = {};
			barrier.sType			= VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			barrier.srcAccessMask	= VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
			barrier.dstAccessMask	= VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
			
			vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
								  0, 1, &barrier, 0, nullptr, 0, nullptr );
			
			VkAccelerationStructureInfoNV	info = {};
			info.sType			= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
			info.type			= VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
			info.flags			= 0;
			info.instanceCount	= 1;

			vkCmdBuildAccelerationStructureNV( cmd, &info,
											   instance_buffer, 0,			// instance
											   VK_FALSE,						// update
											   topLevelAS, VK_NULL_HANDLE,	// dst, src
											   scratch_buffer, 0
											 );
		});
	}
	
	// create scratch buffer
	{
		VkBufferCreateInfo	info = {};
		info.sType			= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.flags			= 0;
		info.usage			= VK_BUFFER_USAGE_RAY_TRACING_BIT_NV;
		info.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;

		// calculate buffer size
		{
			VkMemoryRequirements2								mem_req2	= {};
			VkAccelerationStructureMemoryRequirementsInfoNV		as_info		= {};
			as_info.sType					= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
			as_info.type					= VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;
			as_info.accelerationStructure	= topLevelAS;

			vkGetAccelerationStructureMemoryRequirementsNV( device, &as_info, OUT &mem_req2 );
			info.size = mem_req2.memoryRequirements.size;
		
			as_info.accelerationStructure	= bottomLevelAS;
			vkGetAccelerationStructureMemoryRequirementsNV( device, &as_info, OUT &mem_req2 );
			info.size = Max( info.size, mem_req2.memoryRequirements.size );
		}

		VK_CHECK( vkCreateBuffer( device, &info, nullptr, OUT &scratch_buffer ));
		
		VkMemoryRequirements	mem_req;
		vkGetBufferMemoryRequirements( device, scratch_buffer, OUT &mem_req );
		
		VkDeviceSize	offset = AlignToLarger( res.dev.totalSize, mem_req.alignment );
		res.dev.totalSize	 = offset + mem_req.size;
		res.dev.memTypeBits	|= mem_req.memoryTypeBits;

		res.onBind.push_back( [this, scratch_buffer, &dev_memory, offset] (void *) -> bool
		{
			VK_CHECK( vkBindBufferMemory( device, scratch_buffer, dev_memory, offset ));
			return true;
		});
	}

	// create shader binding table
	{
		const uint	stride		= rayTracingProps.shaderGroupHandleSize;
		const uint	alignment	= std::max( stride, rayTracingProps.shaderGroupBaseAlignment );

		VkBufferCreateInfo	info = {};
		info.sType			= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.flags			= 0;
		info.size			= numGroups * alignment;
		info.usage			= VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_NV;
		info.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK( vkCreateBuffer( device, &info, nullptr, OUT &shaderBindingTable ));
		tempHandles.emplace_back( EHandleType::Buffer, uint64_t(shaderBindingTable) );
		
		VkMemoryRequirements	mem_req;
		vkGetBufferMemoryRequirements( device, shaderBindingTable, OUT &mem_req );
		
		VkDeviceSize	offset = AlignToLarger( res.dev.totalSize, mem_req.alignment );
		res.dev.totalSize	 = offset + mem_req.size;
		res.dev.memTypeBits	|= mem_req.memoryTypeBits;

		res.onBind.push_back( [this, shaderBindingTable, &dev_memory, offset] (void *) -> bool
		{
			VK_CHECK( vkBindBufferMemory( device, shaderBindingTable, dev_memory, offset ));
			return true;
		});

		res.onUpdate.push_back( [this, shaderBindingTable, rtPipeline, numGroups, alignment, size = info.size] (VkCommandBuffer cmd)
		{
			vector<uint8_t>	handles;  handles.resize(size);

			for (uint i = 0; i < numGroups; ++i) {
				VK_CALL( vkGetRayTracingShaderGroupHandlesNV( device, rtPipeline, i, 1, rayTracingProps.shaderGroupHandleSize, OUT handles.data() + alignment * i ));
			}	
			vkCmdUpdateBuffer( cmd, shaderBindingTable, 0, handles.size(), handles.data() );
		});
	}
	
	// allocate device local memory
	{
		VkMemoryAllocateInfo	info = {};
		info.sType				= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		info.allocationSize		= res.dev.totalSize;
		CHECK_ERR( GetMemoryTypeIndex( res.dev.memTypeBits, res.dev.memProperty, OUT info.memoryTypeIndex ));

		VK_CHECK( vkAllocateMemory( device, &info, nullptr, OUT &dev_memory ));
		tempHandles.emplace_back( EHandleType::Memory, uint64_t(dev_memory) );
	}

	// allocate host visible memory
	void* host_ptr = nullptr;
	{
		VkMemoryAllocateInfo	info = {};
		info.sType				= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		info.allocationSize		= res.host.totalSize;
		CHECK_ERR( GetMemoryTypeIndex( res.host.memTypeBits, res.host.memProperty, OUT info.memoryTypeIndex ));

		VK_CHECK( vkAllocateMemory( device, &info, nullptr, OUT &host_memory ));

		VK_CHECK( vkMapMemory( device, host_memory, 0, res.host.totalSize, 0, &host_ptr ));
	}

	// bind resources
	for (auto& bind : res.onBind) {
		CHECK_ERR( bind( host_ptr ));
	}

	// update resources
	{
		VkCommandBufferBeginInfo	begin_info = {};
		begin_info.sType	= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags	= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CALL( vkBeginCommandBuffer( cmdBuffer, &begin_info ));

		for (auto& cb : res.onUpdate) {
			cb( cmdBuffer );
		}

		VK_CALL( vkEndCommandBuffer( cmdBuffer ));

		VkSubmitInfo		submit_info = {};
		submit_info.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.commandBufferCount	= 1;
		submit_info.pCommandBuffers		= &cmdBuffer;

		VK_CHECK( vkQueueSubmit( queue, 1, &submit_info, VK_NULL_HANDLE ));
	}
	VK_CALL( vkQueueWaitIdle( queue ));
	
	vkDestroyBuffer( device, vertex_buffer, nullptr );
	vkDestroyBuffer( device, index_buffer, nullptr );
	vkDestroyBuffer( device, instance_buffer, nullptr );
	vkDestroyBuffer( device, scratch_buffer, nullptr );
	vkFreeMemory( device, host_memory, nullptr );

	return true;
}

/*
=================================================
	_DebugUtilsCallback
=================================================
*/
VKAPI_ATTR VkBool32 VKAPI_CALL
	Device::_DebugUtilsCallback (VkDebugUtilsMessageSeverityFlagBitsEXT			messageSeverity,
								 VkDebugUtilsMessageTypeFlagsEXT				/*messageTypes*/,
								 const VkDebugUtilsMessengerCallbackDataEXT*	pCallbackData,
								 void*											pUserData)
{
	std::cout << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

/*
=================================================
	TestDebugTraceOutput
=================================================
*/
bool  Device::TestDebugTraceOutput (vector<VkShaderModule> modules, string referenceFile)
{
	CHECK_ERR( referenceFile.size() );
	CHECK_ERR( modules.size() );

	string			merged;
	vector<string>	debug_output;

	for (auto& module : modules)
	{
		vector<string>	temp;
		CHECK_ERR( _GetDebugOutput( module, readBackPtr, debugOutputSize, OUT temp ));
		CHECK_ERR( temp.size() );
		debug_output.insert( debug_output.end(), temp.begin(), temp.end() );
	}

	std::sort( debug_output.begin(), debug_output.end() );

	for (auto& str : debug_output) {
		(merged += str) += "//---------------------------\n\n";
	}

	if ( UpdateReferences )
	{
		FILE*	file = nullptr;
		fopen_s( OUT &file, (std::string{DATA_PATH} + referenceFile).c_str(), "wb" );
		CHECK_ERR( file );
		CHECK_ERR( fwrite( merged.c_str(), sizeof(merged[0]), merged.size(), file ) == merged.size() );
		fclose( file );
		return true;
	}

	string	file_data;
	{
		FILE*	file = nullptr;
		fopen_s( OUT &file, (std::string{DATA_PATH} + referenceFile).c_str(), "rb" );
		CHECK_ERR( file );
		
		CHECK_ERR( fseek( file, 0, SEEK_END ) == 0 );
		const long	size = ftell( file );
		CHECK_ERR( fseek( file, 0, SEEK_SET ) == 0 );

		file_data.resize( size );
		CHECK_ERR( fread( file_data.data(), sizeof(file_data[0]), file_data.size(), file ) == file_data.size() );
		fclose( file );
	}

	CHECK_ERR( file_data == merged );
	return true;
}

/*
=================================================
	TestPerformanceOutput
=================================================
*/
bool  Device::TestPerformanceOutput (vector<VkShaderModule> modules, vector<string> fnNames)
{
	CHECK_ERR( fnNames.size() );
	CHECK_ERR( modules.size() );

	for (auto& module : modules)
	{
		vector<string>	temp;
		CHECK_ERR( _GetDebugOutput( module, readBackPtr, debugOutputSize, OUT temp ));
		CHECK_ERR( temp.size() );

		for (auto& output : temp)
		{
			for (auto& fn : fnNames)
			{
				size_t	pos = output.find( fn );
				CHECK_ERR( pos != string::npos );
			}
		}
	}

	return true;
}

/*
=================================================
	CheckTimeMap
=================================================
*/
bool  Device::CheckTimeMap (vector<VkShaderModule> modules, float emptyPxFactor)
{
	CHECK_ERR( modules.size() );

	for (auto& module : modules)
	{
		auto	iter = _debuggableShaders.find( module );
		CHECK_ERR( iter != _debuggableShaders.end() );
	}

	uint const*			ptr		= static_cast<uint const *>( readBackPtr );
	uint				width	= *(ptr + 2);
	uint				height	= *(ptr + 3);
	uint64_t const*		pixels	= static_cast<uint64_t const *>( readBackPtr ) + 2;
	uint				count	= 0;

	for (uint y = 0; y < height; ++y)
	for (uint x = 0; x < width; ++x)
	{
		uint64_t	dt = *(pixels + (x + y * width));
		count += uint(dt > 0);
	}

	float	f = float(count) / float(width * height);
	CHECK_ERR( f >= emptyPxFactor );

	return true;
}

/*
=================================================
	FreeTempHandles
=================================================
*/
void  Device::FreeTempHandles ()
{
	for (auto&[type, hnd] : tempHandles)
	{
		BEGIN_ENUM_CHECKS();
		switch ( type )
		{
			case EHandleType::Memory :
				vkFreeMemory( device, VkDeviceMemory(hnd), nullptr );
				break;
			case EHandleType::Buffer :
				vkDestroyBuffer( device, VkBuffer(hnd), nullptr );
				break;
			case EHandleType::Image :
				vkDestroyImage( device, VkImage(hnd), nullptr );
				break;
			case EHandleType::ImageView :
				vkDestroyImageView( device, VkImageView(hnd), nullptr );
				break;
			case EHandleType::Pipeline :
				vkDestroyPipeline( device, VkPipeline(hnd), nullptr );
				break;
			case EHandleType::PipelineLayout :
				vkDestroyPipelineLayout( device, VkPipelineLayout(hnd), nullptr );
				break;
			case EHandleType::Shader :
				vkDestroyShaderModule( device, VkShaderModule(hnd), nullptr );
				break;
			case EHandleType::DescriptorSetLayout :
				vkDestroyDescriptorSetLayout( device, VkDescriptorSetLayout(hnd), nullptr );
				break;
			case EHandleType::RenderPass :
				vkDestroyRenderPass( device, VkRenderPass(hnd), nullptr );
				break;
			case EHandleType::Framebuffer :
				vkDestroyFramebuffer( device, VkFramebuffer(hnd), nullptr );
				break;
			case EHandleType::AccStruct :
				vkDestroyAccelerationStructureNV( device, VkAccelerationStructureNV(hnd), nullptr );
				break;
		}
		END_ENUM_CHECKS();
	}
	tempHandles.clear();
}
