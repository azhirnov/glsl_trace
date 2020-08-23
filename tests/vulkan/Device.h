// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "VulkanLoader.h"
#include <iostream>

using namespace std::string_literals;

enum class ETraceMode
{
	None,
	DebugTrace,
	Performance,
	TimeMap,
};


class Device final : public VulkanDeviceFn
{
public:
	VkDevice				device				= VK_NULL_HANDLE;
	VkPhysicalDevice		physicalDevice		= VK_NULL_HANDLE;
	VkInstance				instance			= VK_NULL_HANDLE;
	VkQueue					queue				= VK_NULL_HANDLE;
	uint					queueFamily			= ~0u;
	VkCommandPool			cmdPool				= VK_NULL_HANDLE;
	VkCommandBuffer			cmdBuffer			= VK_NULL_HANDLE;
	VkDescriptorPool		descPool			= VK_NULL_HANDLE;
	VkBuffer				debugOutputBuf		= VK_NULL_HANDLE;
	VkBuffer				readBackBuf			= VK_NULL_HANDLE;
	VkDeviceMemory			debugOutputMem		= VK_NULL_HANDLE;
	VkDeviceMemory			readBackMem			= VK_NULL_HANDLE;
	void *					readBackPtr			= nullptr;
	uint					debugOutputSize		= 128 << 20;

	bool					hasMeshShader		= false;
	bool					hasRayTracing		= false;
	bool					hasShaderClock		= false;
	
	VkPhysicalDeviceProperties				deviceProps;
	VkPhysicalDeviceFeatures				deviceFeat;
	VkPhysicalDeviceMeshShaderFeaturesNV	meshShaderFeat;
	VkPhysicalDeviceMeshShaderPropertiesNV	meshShaderProps;
	VkPhysicalDeviceShaderClockFeaturesKHR	shaderClockFeat;
	VkPhysicalDeviceRayTracingPropertiesNV	rayTracingProps;

private:
	VkPhysicalDeviceMemoryProperties	_deviceMemoryProperties;
	VkDebugUtilsMessengerEXT			_debugUtilsMessenger	= VK_NULL_HANDLE;
	
	using Debuggable_t	= unordered_map< VkShaderModule, ShaderTrace* >;
	vector<uint>			_tempBuf;
	Debuggable_t			_debuggableShaders;
	
	VulkanDeviceFnTable		_deviceFnTable;
	
public:
	enum class EHandleType
	{
		Memory,
		Buffer,
		Image,
		ImageView,
		Pipeline,
		PipelineLayout,
		Shader,
		DescriptorSetLayout,
		RenderPass,
		Framebuffer,
		AccStruct,
	};
	using TempHandles_t = vector<std::pair<EHandleType, uint64_t>>;
	TempHandles_t		tempHandles;


public:
	bool  Create ();
	void  Destroy ();

	bool  GetMemoryTypeIndex (uint memoryTypeBits, VkMemoryPropertyFlags flags, OUT uint &memoryTypeIndex) const;
	
	static bool  CheckErrors (VkResult errCode, const char *vkcall, const char *func, const char *file, int line);
	
	bool  CreateDebugDescriptorSet (VkShaderStageFlags stages,
								    OUT VkDescriptorSetLayout &dsLayout, OUT VkDescriptorSet &descSet);

	bool  CreateRenderTarget (VkFormat colorFormat, uint width, uint height, VkImageUsageFlags imageUsage,
							  OUT VkRenderPass &outRenderPass, OUT VkImage &outImage,
							  OUT VkFramebuffer &outFramebuffer);

	bool  CreateGraphicsPipelineVar1 (VkShaderModule vertShader, VkShaderModule fragShader,
									  VkDescriptorSetLayout dsLayout, VkRenderPass renderPass,
									  OUT VkPipelineLayout &outPipelineLayout, OUT VkPipeline &outPipeline);

	bool  CreateGraphicsPipelineVar2 (VkShaderModule vertShader, VkShaderModule tessContShader, VkShaderModule tessEvalShader,
									  VkShaderModule fragShader, VkDescriptorSetLayout dsLayout, VkRenderPass renderPass, uint patchSize,
									  OUT VkPipelineLayout &outPipelineLayout, OUT VkPipeline &outPipeline);

	bool  CreateMeshPipelineVar1 (VkShaderModule meshShader, VkShaderModule fragShader,
								  VkDescriptorSetLayout dsLayout, VkRenderPass renderPass,
								  OUT VkPipelineLayout &outPipelineLayout, OUT VkPipeline &outPipeline);

	bool  CreateRayTracingScene (VkPipeline rtPipeline, uint numGroups,
								 OUT VkBuffer &shaderBindingTable,
								 OUT VkAccelerationStructureNV &topLevelAS, OUT VkAccelerationStructureNV &bottomLevelAS);
	
	bool  Compile (OUT VkShaderModule&		shaderModule,
				   vector<const char *>		source,
				   EShLanguage				shaderType,
				   ETraceMode				mode				= ETraceMode::None,
				   uint						dbgBufferSetIndex	= ~0u,
				   glslang::EShTargetLanguageVersion	spvVersion	= glslang::EShTargetSpv_1_3);
	
	bool  TestDebugTraceOutput (vector<VkShaderModule> modules, string referenceFile);

	bool  TestPerformanceOutput (vector<VkShaderModule> modules, vector<string> fnNames);
	
	bool  CheckTimeMap (vector<VkShaderModule> modules, float emptyPxFactor = 1.0f);

	void  FreeTempHandles ();


private:
	bool  _CreateDevice ();
	void  _DestroyDevice ();

	bool  _CreateResources ();
	void  _DestroyResources ();

	void  _ValidateInstanceVersion (INOUT uint &version) const;
	void  _ValidateInstanceLayers (INOUT vector<const char*> &layers) const;
	void  _ValidateInstanceExtensions (INOUT vector<const char*> &ext) const;
	void  _ValidateDeviceExtensions (INOUT vector<const char*> &ext) const;

	bool  _GetDebugOutput (VkShaderModule shaderModule, const void *ptr, VkDeviceSize maxSize, OUT vector<string> &result) const;
	
	bool  _Compile (OUT vector<uint>&		spirvData,
				    OUT ShaderTrace*		dbgInfo,
				    uint					dbgBufferSetIndex,
				    vector<const char *>	source,
				    EShLanguage				shaderType,
				    ETraceMode				mode,
				    glslang::EShTargetLanguageVersion	spvVersion);

	VKAPI_ATTR static VkBool32 VKAPI_CALL
		_DebugUtilsCallback (VkDebugUtilsMessageSeverityFlagBitsEXT			messageSeverity,
								VkDebugUtilsMessageTypeFlagsEXT				messageTypes,
								const VkDebugUtilsMessengerCallbackDataEXT*	pCallbackData,
								void*										pUserData);
};


/*
=================================================
	BitCast
=================================================
*/
	template <typename To, typename From>
	ND_ inline constexpr To  BitCast (const From& src)
	{
		static_assert( sizeof(To) == sizeof(From), "must be same size!" );
		static_assert( alignof(To) == alignof(From), "must be same align!" );
		static_assert( std::is_trivially_copyable<From>::value and std::is_trivial<To>::value, "must be trivial types!" );

		To	dst;
		std::memcpy( OUT &dst, &src, sizeof(To) );
		return dst;
	}


#define VK_CALL( ... ) \
	{ \
		const ::VkResult __vk_err__ = (__VA_ARGS__); \
		::Device::CheckErrors( __vk_err__, #__VA_ARGS__, FUNCTION_NAME, __FILE__, __LINE__ ); \
	}

#define __PRIVATE_VK_CALL_R( _func_, _ret_, ... ) \
	{ \
		const ::VkResult __vk_err__ = (_func_); \
		if ( not ::Device::CheckErrors( __vk_err__, #_func_, FUNCTION_NAME, __FILE__, __LINE__ )) \
			return _ret_; \
	}

#define VK_CHECK( ... ) \
	__PRIVATE_VK_CALL_R( __GETARG_0( __VA_ARGS__ ), __GETARG_1( __VA_ARGS__, 0 ))
	
// function name
#ifdef _MSC_VER
#	define FUNCTION_NAME			__FUNCTION__

#elif defined(__clang__) || defined(__gcc__)
#	define FUNCTION_NAME			__func__

#else
#	define FUNCTION_NAME			"unknown function"
#endif

#define TEST_PASSED()	std::cout << FUNCTION_NAME << " - passed" << std::endl;
