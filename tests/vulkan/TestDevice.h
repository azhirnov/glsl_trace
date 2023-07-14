// Copyright (c) Zhirnov Andrey. For more information see 'LICENSE'

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


class TestDevice final : public VulkanDeviceFn
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
	void *					readBackPtr			= null;
	uint					debugOutputSize		= 128 << 20;

	bool					hasMeshShader		= false;
	bool					hasRayTracing		= false;
	bool					hasShaderClock		= false;
	
	VkPhysicalDeviceProperties							deviceProps;
	VkPhysicalDeviceFeatures							deviceFeat;
	VkPhysicalDeviceMeshShaderFeaturesEXT				meshShaderFeats;
	VkPhysicalDeviceMeshShaderPropertiesEXT				meshShaderProps;
	VkPhysicalDeviceShaderClockFeaturesKHR				shaderClockFeats;
	VkPhysicalDeviceAccelerationStructureFeaturesKHR	accelerationStructureFeats;
	VkPhysicalDeviceAccelerationStructurePropertiesKHR	accelerationStructureProps;
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR		rayTracingPipelineFeats;
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR		rayTracingPipelineProps;

private:
	VkPhysicalDeviceMemoryProperties	_deviceMemoryProperties;
	VkDebugUtilsMessengerEXT			_debugUtilsMessenger	= VK_NULL_HANDLE;
	
	using Debuggable_t	= HashMap< VkShaderModule, ShaderTrace* >;
	Array<uint>				_tempBuf;
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
	using TempHandles_t = Array<Pair<EHandleType, uint64_t>>;
	TempHandles_t		tempHandles;


public:
	ND_ bool  Create ();
		void  Destroy ();
	
		static bool  CheckErrors (VkResult errCode, const char *vkcall, const char *func, const char *file, int line);

	ND_ bool  GetMemoryTypeIndex (uint memoryTypeBits, VkMemoryPropertyFlags flags, OUT uint &memoryTypeIndex) const;

	ND_ bool  CreateDebugDescriptorSet (VkShaderStageFlags stages,
										OUT VkDescriptorSetLayout &dsLayout, OUT VkDescriptorSet &descSet);

	ND_ bool  CreateRenderTarget (VkFormat colorFormat, uint width, uint height, VkImageUsageFlags imageUsage,
								  OUT VkRenderPass &outRenderPass, OUT VkImage &outImage,
								  OUT VkFramebuffer &outFramebuffer);

	ND_ bool  CreateStorageImage (VkFormat format, uint width, uint height, VkImageUsageFlags imageUsage,
								  OUT VkImage &outImage, OUT VkImageView &outView);

	ND_ bool  CreateGraphicsPipelineVar1 (VkShaderModule vertShader, VkShaderModule fragShader,
										  VkDescriptorSetLayout dsLayout, VkRenderPass renderPass,
										  OUT VkPipelineLayout &outPipelineLayout, OUT VkPipeline &outPipeline);

	ND_ bool  CreateGraphicsPipelineVar2 (VkShaderModule vertShader, VkShaderModule tessContShader, VkShaderModule tessEvalShader,
										  VkShaderModule fragShader, VkDescriptorSetLayout dsLayout, VkRenderPass renderPass, uint patchSize,
										  OUT VkPipelineLayout &outPipelineLayout, OUT VkPipeline &outPipeline);

	ND_ bool  CreateMeshPipelineVar1 (VkShaderModule meshShader, VkShaderModule fragShader,
									  VkDescriptorSetLayout dsLayout, VkRenderPass renderPass,
									  OUT VkPipelineLayout &outPipelineLayout, OUT VkPipeline &outPipeline);

	struct RTData
	{
		VkDeviceSize				shaderGroupSize		= 0;
		VkDeviceSize				shaderGroupAlign	= 0;
		VkBuffer					shaderBindingTable	= Default;
		VkDeviceAddress				sbtAddress			= Default;
		VkAccelerationStructureKHR	topLevelAS			= Default;
		VkAccelerationStructureKHR	bottomLevelAS		= Default;
	};
	ND_ bool  CreateRayTracingScene (VkPipeline rtPipeline, uint numGroups, OUT RTData &outRTData);
	
	ND_ bool  Compile (OUT VkShaderModule&		shaderModule,
					   Array<const char *>		source,
					   EShLanguage				shaderType,
					   ETraceMode				mode				= ETraceMode::None,
					   uint						dbgBufferSetIndex	= UMax);
	
	ND_ bool  TestDebugTraceOutput (Array<VkShaderModule> modules, String referenceFile);

	ND_ bool  TestPerformanceOutput (Array<VkShaderModule> modules, Array<String> fnNames);

	ND_ bool  CheckTimeMap (Array<VkShaderModule> modules, float emptyPxFactor = 1.0f);

		void  FreeTempHandles ();

	ND_	VkDevice	GetVkDevice ()		const	{ return device; }
	ND_ VkQueue		GetVkQueue ()		const	{ return queue; }
	ND_ uint		GetQueueFamily ()	const	{ return queueFamily; }
	
	ND_ VkPhysicalDeviceProperties const&						GetDeviceProps ()		const	{ return deviceProps; }
	ND_ VkPhysicalDeviceFeatures const&							GetDeviceFeats ()		const	{ return deviceFeat; }
	ND_ VkPhysicalDeviceMeshShaderFeaturesEXT const&			GetMeshShaderFeats ()	const	{ return meshShaderFeats; }
	ND_ VkPhysicalDeviceMeshShaderPropertiesEXT const&			GetMeshShaderProps ()	const	{ return meshShaderProps; }
	ND_ VkPhysicalDeviceShaderClockFeaturesKHR const&			GetShaderClockFeats ()	const	{ return shaderClockFeats; }
	ND_ VkPhysicalDeviceRayTracingPipelineFeaturesKHR const&	GetRayTracingFeats ()	const	{ return rayTracingPipelineFeats; }
	ND_ VkPhysicalDeviceRayTracingPipelinePropertiesKHR const&	GetRayTracingProps ()	const	{ return rayTracingPipelineProps; }


private:
	bool  _CreateDevice ();
	void  _DestroyDevice ();

	bool  _CreateResources ();
	void  _DestroyResources ();

	void  _ValidateInstanceVersion (INOUT uint &version) const;
	void  _ValidateInstanceLayers (INOUT Array<const char*> &layers) const;
	void  _ValidateInstanceExtensions (INOUT Array<const char*> &ext) const;
	void  _ValidateDeviceExtensions (INOUT Array<const char*> &ext) const;

	bool  _GetDebugOutput (VkShaderModule shaderModule, const void *ptr, VkDeviceSize maxSize, OUT Array<String> &result) const;
	
	bool  _Compile (OUT Array<uint>&		spirvData,
				    OUT ShaderTrace*		dbgInfo,
				    uint					dbgBufferSetIndex,
				    Array<const char *>		source,
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
	ND_ constexpr To  BitCast (const From& src)
	{
		static_assert( sizeof(To) == sizeof(From), "must be same size!" );
		static_assert( alignof(To) == alignof(From), "must be same align!" );
		static_assert( std::is_trivially_copyable<From>::value and std::is_trivial<To>::value, "must be trivial types!" );

		To	dst;
		std::memcpy( OUT &dst, &src, sizeof(To) );
		return dst;
	}
	
	template <typename T>
	ND_ constexpr usize  CountOf (T& value)
	{
		return std::size( value );
	}


#define VK_CHECK( ... ) \
	{ \
		const ::VkResult __vk_err__ = (__VA_ARGS__); \
		::TestDevice::CheckErrors( __vk_err__, #__VA_ARGS__, FUNCTION_NAME, __FILE__, __LINE__ ); \
	}

#define __PRIVATE_VK_CALL_R( _func_, _ret_, ... ) \
	{ \
		const ::VkResult __vk_err__ = (_func_); \
		if ( not ::TestDevice::CheckErrors( __vk_err__, #_func_, FUNCTION_NAME, __FILE__, __LINE__ )) \
			return _ret_; \
	}

#define VK_CHECK_ERR( ... ) \
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
