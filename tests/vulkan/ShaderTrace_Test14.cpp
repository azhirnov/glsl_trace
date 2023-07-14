// Copyright (c) Zhirnov Andrey. For more information see 'LICENSE'

#include "TestDevice.h"

enum {
	RAYGEN_SHADER,
	HIT_SHADER,
	MISS_SHADER,
	NUM_GROUPS
};

/*
=================================================
	CompileShaders
=================================================
*/
static bool CompileShaders (TestDevice &vulkan, OUT VkShaderModule &rayGenShader, OUT VkShaderModule &rayMissShader, OUT VkShaderModule &rayClosestHitShader)
{
	static const char	rt_shader[] = R"#(
#extension GL_EXT_ray_tracing : require
#define PAYLOAD_LOC 0

void dbg_EnableTraceRecording (bool b) {}
)#";

	// create ray generation shader
	{
		static const char	raygen_shader_source[] = R"#(
layout(binding = 0) uniform accelerationStructureEXT  un_RtScene;
layout(binding = 1, rgba8) writeonly restrict uniform image2D  un_Output;
layout(location = PAYLOAD_LOC) rayPayloadEXT vec4  payload;

void main ()
{
	dbg_EnableTraceRecording( gl_LaunchIDEXT.x == 60 && gl_LaunchIDEXT.y == 40 );

	const vec2 uv		 = vec2(gl_LaunchIDEXT.xy + 0.5) / vec2(gl_LaunchSizeEXT.xy);
	const vec3 origin	 = vec3(uv.x, 1.0f - uv.y, -1.0f);
	const vec3 direction = vec3(0.0f, 0.0f, 1.0f);

	payload = vec4(0.0);

	traceRayEXT( /*topLevel*/un_RtScene, /*rayFlags*/gl_RayFlagsNoneEXT, /*cullMask*/0xFF,
				 /*sbtRecordOffset*/0, /*sbtRecordStride*/0, /*missIndex*/0,
				 /*origin*/origin, /*Tmin*/0.0f,
				 /*direction*/direction, /*Tmax*/10.0f,
				 /*payload*/PAYLOAD_LOC );

	vec4 color = payload;
	imageStore( un_Output, ivec2(gl_LaunchIDEXT), color );
}
)#";
		CHECK_ERR( vulkan.Compile( OUT rayGenShader, {rt_shader, raygen_shader_source}, EShLangRayGen, ETraceMode::DebugTrace, 1 ));
	}

	// create ray miss shader
	{
		static const char	raymiss_shader_source[] = R"#(
layout(location = PAYLOAD_LOC) rayPayloadInEXT vec4  payload;

void main ()
{
	dbg_EnableTraceRecording( gl_LaunchIDEXT.x == 50 && gl_LaunchIDEXT.y == 50 );

	payload = vec4( 0.412f, 0.796f, 1.0f, 1.0f );
}
)#";
		CHECK_ERR( vulkan.Compile( OUT rayMissShader, {rt_shader, raymiss_shader_source}, EShLangMiss, ETraceMode::DebugTrace, 1 ));
	}

	// create ray closest hit shader
	{
		static const char	closesthit_shader_source[] = R"#(
layout(location = PAYLOAD_LOC) rayPayloadInEXT vec4  payload;
hitAttributeEXT vec2  HitAttribs;

void main ()
{
	dbg_EnableTraceRecording( gl_LaunchIDEXT.x == 60 && gl_LaunchIDEXT.y == 40 );

	const vec3 barycentrics = vec3(1.0f - HitAttribs.x - HitAttribs.y, HitAttribs.x, HitAttribs.y);
	payload = vec4(barycentrics, 1.0);
}
)#";
		CHECK_ERR( vulkan.Compile( OUT rayClosestHitShader, {rt_shader, closesthit_shader_source}, EShLangClosestHit, ETraceMode::DebugTrace, 1 ));
	}
	return true;
}

/*
=================================================
	CreatePipeline
=================================================
*/
static bool CreatePipeline (TestDevice &vulkan, VkShaderModule rayGenShader, VkShaderModule rayMissShader, VkShaderModule rayClosestHitShader,
							Array<VkDescriptorSetLayout> dsLayouts, OUT VkPipelineLayout &outPipelineLayout, OUT VkPipeline &outPipeline)
{
	// create pipeline layout
	{
		VkPipelineLayoutCreateInfo	info = {};
		info.sType					= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		info.setLayoutCount			= uint(dsLayouts.size());
		info.pSetLayouts			= dsLayouts.data();
		info.pushConstantRangeCount	= 0;
		info.pPushConstantRanges	= null;

		VK_CHECK_ERR( vulkan.vkCreatePipelineLayout( vulkan.GetVkDevice(), &info, null, OUT &outPipelineLayout ));
		vulkan.tempHandles.emplace_back( TestDevice::EHandleType::PipelineLayout, ulong(outPipelineLayout) );
	}


	VkPipelineShaderStageCreateInfo		stages [NUM_GROUPS] = {};

	stages[RAYGEN_SHADER].sType		= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[RAYGEN_SHADER].stage		= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	stages[RAYGEN_SHADER].module	= rayGenShader;
	stages[RAYGEN_SHADER].pName		= "main";

	stages[MISS_SHADER].sType		= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[MISS_SHADER].stage		= VK_SHADER_STAGE_MISS_BIT_KHR;
	stages[MISS_SHADER].module		= rayMissShader;
	stages[MISS_SHADER].pName		= "main";

	stages[HIT_SHADER].sType		= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[HIT_SHADER].stage		= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	stages[HIT_SHADER].module		= rayClosestHitShader;
	stages[HIT_SHADER].pName		= "main";


	VkRayTracingShaderGroupCreateInfoKHR	groups [NUM_GROUPS] = {};

	groups[RAYGEN_SHADER].sType				= VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groups[RAYGEN_SHADER].type				= VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	groups[RAYGEN_SHADER].generalShader		= RAYGEN_SHADER;
	groups[RAYGEN_SHADER].closestHitShader	= VK_SHADER_UNUSED_KHR;
	groups[RAYGEN_SHADER].anyHitShader		= VK_SHADER_UNUSED_KHR;
	groups[RAYGEN_SHADER].intersectionShader= VK_SHADER_UNUSED_KHR;

	groups[HIT_SHADER].sType				= VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groups[HIT_SHADER].type					= VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	groups[HIT_SHADER].generalShader		= VK_SHADER_UNUSED_KHR;
	groups[HIT_SHADER].closestHitShader		= HIT_SHADER;
	groups[HIT_SHADER].anyHitShader			= VK_SHADER_UNUSED_KHR;
	groups[HIT_SHADER].intersectionShader	= VK_SHADER_UNUSED_KHR;

	groups[MISS_SHADER].sType				= VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groups[MISS_SHADER].type				= VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	groups[MISS_SHADER].generalShader		= MISS_SHADER;
	groups[MISS_SHADER].closestHitShader	= VK_SHADER_UNUSED_KHR;
	groups[MISS_SHADER].anyHitShader		= VK_SHADER_UNUSED_KHR;
	groups[MISS_SHADER].intersectionShader	= VK_SHADER_UNUSED_KHR;

	// create pipeline
	VkRayTracingPipelineCreateInfoKHR 	info = {};
	info.sType			= VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	info.flags			= 0;
	info.stageCount		= uint(CountOf( stages ));
	info.pStages		= stages;
	info.pGroups		= groups;
	info.groupCount		= uint(CountOf( groups ));
	info.layout			= outPipelineLayout;
	info.maxPipelineRayRecursionDepth	= 1;

	VK_CHECK_ERR( vulkan.vkCreateRayTracingPipelinesKHR( vulkan.GetVkDevice(), Default, Default, 1, &info, null, OUT &outPipeline ));
	vulkan.tempHandles.emplace_back( TestDevice::EHandleType::Pipeline, ulong(outPipeline) );

	return true;
}

/*
=================================================
	ShaderTrace_Test14
=================================================
*/
extern bool ShaderTrace_Test14 (TestDevice& vulkan)
{
	if ( vulkan.GetRayTracingFeats().rayTracingPipeline == VK_FALSE )
		return true;	// not supported

	// create image
	VkImage			image;
	VkImageView		image_view;
	uint			width = 128, height = 128;
	CHECK_ERR( vulkan.CreateStorageImage( VK_FORMAT_R8G8B8A8_UNORM, width, height, 0, OUT image, OUT image_view ));

	// create pipeline
	VkShaderModule	raygen_shader, miss_shader, hit_shader;
	CHECK_ERR( CompileShaders( vulkan, OUT raygen_shader, OUT miss_shader, OUT hit_shader ));

	VkDescriptorSetLayout	ds1_layout, ds2_layout;
	VkDescriptorSet			desc_set1, desc_set2;
	CHECK_ERR( vulkan.CreateDebugDescriptorSet( VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, OUT ds2_layout, OUT desc_set2 ));

	// create layout
	{
		VkDescriptorSetLayoutBinding		binding[2] = {};
		binding[0].binding			= 0;
		binding[0].descriptorType	= VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		binding[0].descriptorCount	= 1;
		binding[0].stageFlags		= VK_SHADER_STAGE_RAYGEN_BIT_KHR;

		binding[1].binding			= 1;
		binding[1].descriptorType	= VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		binding[1].descriptorCount	= 1;
		binding[1].stageFlags		= VK_SHADER_STAGE_RAYGEN_BIT_KHR;

		VkDescriptorSetLayoutCreateInfo		info = {};
		info.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		info.bindingCount	= uint(CountOf( binding ));
		info.pBindings		= binding;

		VK_CHECK_ERR( vulkan.vkCreateDescriptorSetLayout( vulkan.GetVkDevice(), &info, null, OUT &ds1_layout ));
		vulkan.tempHandles.emplace_back( TestDevice::EHandleType::DescriptorSetLayout, ulong(ds1_layout) );
	}

	// create pipeline
	VkPipelineLayout	ppln_layout;
	VkPipeline			pipeline;
	CHECK_ERR( CreatePipeline( vulkan, raygen_shader, miss_shader, hit_shader, {ds1_layout, ds2_layout}, OUT ppln_layout, OUT pipeline ));

	// create BLAS and TLAS
	TestDevice::RTData	rt_data;
	CHECK_ERR( vulkan.CreateRayTracingScene( pipeline, NUM_GROUPS, OUT rt_data ));

	// allocate descriptor set
	{
		VkDescriptorSetAllocateInfo		info = {};
		info.sType				= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		info.descriptorPool		= vulkan.descPool;
		info.descriptorSetCount	= 1;
		info.pSetLayouts		= &ds1_layout;

		VK_CHECK_ERR( vulkan.vkAllocateDescriptorSets( vulkan.GetVkDevice(), &info, OUT &desc_set1 ));
	}

	// update descriptor set
	{
		VkWriteDescriptorSet	writes[2] = {};

		// un_Output
		VkDescriptorImageInfo	desc_image = {};
		desc_image.imageView	= image_view;
		desc_image.imageLayout	= VK_IMAGE_LAYOUT_GENERAL;

		writes[0].sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet			= desc_set1;
		writes[0].dstBinding		= 1;
		writes[0].descriptorCount	= 1;
		writes[0].descriptorType	= VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[0].pImageInfo		= &desc_image;

		// un_RtScene
		VkWriteDescriptorSetAccelerationStructureKHR 	tlas_desc = {};
		tlas_desc.sType							= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		tlas_desc.accelerationStructureCount	= 1;
		tlas_desc.pAccelerationStructures		= &rt_data.topLevelAS;

		writes[1].sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].pNext				= &tlas_desc;
		writes[1].dstSet			= desc_set1;
		writes[1].dstBinding		= 0;
		writes[1].descriptorCount	= 1;
		writes[1].descriptorType	= VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

		vulkan.vkUpdateDescriptorSets( vulkan.GetVkDevice(), uint(CountOf( writes )), writes, 0, null );
	}

	// build command buffer
	VkCommandBufferBeginInfo	begin = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, null, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, null };
	VK_CHECK_ERR( vulkan.vkBeginCommandBuffer( vulkan.cmdBuffer, &begin ));

	// image layout undefined -> general
	{
		VkImageMemoryBarrier	barrier = {};
		barrier.sType			= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcAccessMask	= 0;
		barrier.dstAccessMask	= VK_ACCESS_SHADER_WRITE_BIT;
		barrier.oldLayout		= VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout		= VK_IMAGE_LAYOUT_GENERAL;
		barrier.image			= image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange	= {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

		vulkan.vkCmdPipelineBarrier( vulkan.cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0,
									 0, null, 0, null, 1, &barrier);
	}

	// setup storage buffer
	{
		const uint	data[] = { ~0u, ~0u, ~0u };

		vulkan.vkCmdFillBuffer( vulkan.cmdBuffer, vulkan.debugOutputBuf, sizeof(data), VK_WHOLE_SIZE, 0 );
		vulkan.vkCmdUpdateBuffer( vulkan.cmdBuffer, vulkan.debugOutputBuf, 0, sizeof(data), data );
	}

	// debug output storage read/write after write
	{
		VkBufferMemoryBarrier	barrier = {};
		barrier.sType			= VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		barrier.srcAccessMask	= VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask	= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		barrier.buffer			= vulkan.debugOutputBuf;
		barrier.offset			= 0;
		barrier.size			= VK_WHOLE_SIZE;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		vulkan.vkCmdPipelineBarrier( vulkan.cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0,
									 0, null, 1, &barrier, 0, null);
	}

	// trace rays
	{
		vulkan.vkCmdBindPipeline( vulkan.cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline );
		vulkan.vkCmdBindDescriptorSets( vulkan.cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, ppln_layout, 0, 1, &desc_set1, 0, null );
		vulkan.vkCmdBindDescriptorSets( vulkan.cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, ppln_layout, 1, 1, &desc_set2, 0, null );

		VkStridedDeviceAddressRegionKHR	raygen_sbt	= {};
		VkStridedDeviceAddressRegionKHR	miss_sbt	= {};
		VkStridedDeviceAddressRegionKHR	hit_sbt		= {};
		VkStridedDeviceAddressRegionKHR	call_sbt	= {};

		raygen_sbt.deviceAddress	= rt_data.sbtAddress + RAYGEN_SHADER * rt_data.shaderGroupAlign;
		raygen_sbt.size				= rt_data.shaderGroupSize;
		raygen_sbt.stride			= rt_data.shaderGroupSize;

		miss_sbt.deviceAddress		= rt_data.sbtAddress + MISS_SHADER * rt_data.shaderGroupAlign;
		miss_sbt.size				= rt_data.shaderGroupSize;
		miss_sbt.stride				= rt_data.shaderGroupSize;

		hit_sbt.deviceAddress		= rt_data.sbtAddress + HIT_SHADER * rt_data.shaderGroupAlign;
		hit_sbt.size				= rt_data.shaderGroupSize;
		hit_sbt.stride				= rt_data.shaderGroupSize;

		vulkan.vkCmdTraceRaysKHR( vulkan.cmdBuffer,
								  &raygen_sbt, &miss_sbt, &hit_sbt, &call_sbt,
								  width, height, 1 );
	}

	// debug output storage read after write
	{
		VkBufferMemoryBarrier	barrier = {};
		barrier.sType			= VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		barrier.srcAccessMask	= VK_ACCESS_SHADER_WRITE_BIT;
		barrier.dstAccessMask	= VK_ACCESS_TRANSFER_READ_BIT;
		barrier.buffer			= vulkan.debugOutputBuf;
		barrier.offset			= 0;
		barrier.size			= VK_WHOLE_SIZE;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		vulkan.vkCmdPipelineBarrier( vulkan.cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
									 0, null, 1, &barrier, 0, null );
	}

	// copy shader debug output into host visible memory
	{
		VkBufferCopy	region = {};
		region.srcOffset	= 0;
		region.dstOffset	= 0;
		region.size			= vulkan.debugOutputSize;

		vulkan.vkCmdCopyBuffer( vulkan.cmdBuffer, vulkan.debugOutputBuf, vulkan.readBackBuf, 1, &region );
	}

	VK_CHECK_ERR( vulkan.vkEndCommandBuffer( vulkan.cmdBuffer ));

	// submit commands and wait
	{
		VkSubmitInfo	submit = {};
		submit.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.commandBufferCount	= 1;
		submit.pCommandBuffers		= &vulkan.cmdBuffer;

		VK_CHECK_ERR( vulkan.vkQueueSubmit( vulkan.GetVkQueue(), 1, &submit, Default ));
		VK_CHECK_ERR( vulkan.vkQueueWaitIdle( vulkan.GetVkQueue() ));
	}

	CHECK_ERR( vulkan.TestDebugTraceOutput( {raygen_shader, miss_shader/*, hit_shader*/}, "ShaderTrace_Test14.txt" ));

	vulkan.FreeTempHandles();

	TEST_PASSED();
	return true;
}
