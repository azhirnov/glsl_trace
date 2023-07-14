// Copyright (c) Zhirnov Andrey. For more information see 'LICENSE'

#include "TestDevice.h"

/*
=================================================
	CompileShaders
=================================================
*/
static bool CompileShaders (TestDevice &vulkan, OUT VkShaderModule &compShader)
{
	// create compute shader
	{
		static const char	comp_shader_source[] = R"#(
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(binding = 0) writeonly uniform image2D  un_OutImage;

void Test1 (out vec4 color)
{
	vec2	point = (vec3(gl_GlobalInvocationID) / vec3(gl_NumWorkGroups * gl_WorkGroupSize - 1)).xy;

	if ( (gl_GlobalInvocationID.x & 1) == 1 )
	{
		color = vec4(1.0f);
		return;
	}

	color = point.xyyx;
	color.x = cos(color.y) * color.z;
}

void main()
{
	vec4	color;
	Test1( color );
	imageStore( un_OutImage, ivec2(gl_GlobalInvocationID.xy), color );
}
)#";
		CHECK_ERR( vulkan.Compile( OUT compShader, {comp_shader_source}, EShLangCompute, ETraceMode::DebugTrace, 1 ));
	}
	return true;
}

/*
=================================================
	CreatePipeline
=================================================
*/
static bool CreatePipeline (TestDevice &vulkan, VkShaderModule compShader, Array<VkDescriptorSetLayout> dsLayouts,
							OUT VkPipelineLayout &outPipelineLayout, OUT VkPipeline &outPipeline)
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

	VkComputePipelineCreateInfo		info = {};
	info.sType			= VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	info.stage.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	info.stage.stage	= VK_SHADER_STAGE_COMPUTE_BIT;
	info.stage.module	= compShader;
	info.stage.pName	= "main";
	info.layout			= outPipelineLayout;

	VK_CHECK_ERR( vulkan.vkCreateComputePipelines( vulkan.GetVkDevice(), Default, 1, &info, null, OUT &outPipeline ));
	vulkan.tempHandles.emplace_back( TestDevice::EHandleType::Pipeline, ulong(outPipeline) );

	return true;
}

/*
=================================================
	ShaderTrace_Test2
=================================================
*/
extern bool ShaderTrace_Test2 (TestDevice& vulkan)
{	
	// create image
	VkImage			image;
	VkImageView		image_view;
	uint			width = 16, height = 16;
	CHECK_ERR( vulkan.CreateStorageImage( VK_FORMAT_R8G8B8A8_UNORM, width, height, 0, OUT image, OUT image_view ));

	// create pipeline
	VkShaderModule	comp_shader;
	CHECK_ERR( CompileShaders( vulkan, OUT comp_shader ));

	VkDescriptorSetLayout	ds1_layout, ds2_layout;
	VkDescriptorSet			desc_set1, desc_set2;
	CHECK_ERR( vulkan.CreateDebugDescriptorSet( VK_SHADER_STAGE_COMPUTE_BIT, OUT ds2_layout, OUT desc_set2 ));

	// create layout
	{
		VkDescriptorSetLayoutBinding	binding = {};
		binding.binding			= 0;
		binding.descriptorType	= VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		binding.descriptorCount	= 1;
		binding.stageFlags		= VK_SHADER_STAGE_COMPUTE_BIT;

		VkDescriptorSetLayoutCreateInfo		info = {};
		info.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		info.bindingCount	= 1;
		info.pBindings		= &binding;

		VK_CHECK_ERR( vulkan.vkCreateDescriptorSetLayout( vulkan.GetVkDevice(), &info, null, OUT &ds1_layout ));
		vulkan.tempHandles.emplace_back( TestDevice::EHandleType::DescriptorSetLayout, ulong(ds1_layout) );
	}

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
		VkDescriptorImageInfo	desc_image	= {};
		desc_image.imageView	= image_view;
		desc_image.imageLayout	= VK_IMAGE_LAYOUT_GENERAL;

		VkWriteDescriptorSet	write	= {};
		write.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet			= desc_set1;
		write.dstBinding		= 0;
		write.descriptorCount	= 1;
		write.descriptorType	= VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		write.pImageInfo		= &desc_image;

		vulkan.vkUpdateDescriptorSets( vulkan.GetVkDevice(), 1, &write, 0, null );
	}

	VkPipelineLayout	ppln_layout;
	VkPipeline			pipeline;
	CHECK_ERR( CreatePipeline( vulkan, comp_shader, {ds1_layout, ds2_layout}, OUT ppln_layout, OUT pipeline ));


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

		vulkan.vkCmdPipelineBarrier( vulkan.cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
									 0, null, 0, null, 1, &barrier);
	}

	// setup storage buffer
	{
		const uint	data[] = { 
			width/2, height/2, 0,		// selected invocation
		};

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

		vulkan.vkCmdPipelineBarrier( vulkan.cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
									 0, null, 1, &barrier, 0, null);
	}

	// dispatch
	{
		vulkan.vkCmdBindPipeline( vulkan.cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline );
		vulkan.vkCmdBindDescriptorSets( vulkan.cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ppln_layout, 0, 1, &desc_set1, 0, null );
		vulkan.vkCmdBindDescriptorSets( vulkan.cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ppln_layout, 1, 1, &desc_set2, 0, null );

		vulkan.vkCmdDispatch( vulkan.cmdBuffer, width, height, 1 );
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

		vulkan.vkCmdPipelineBarrier( vulkan.cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
									 0, null, 1, &barrier, 0, null);
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

	CHECK_ERR( vulkan.TestDebugTraceOutput( {comp_shader}, "ShaderTrace_Test2.txt" ));

	vulkan.FreeTempHandles();

	TEST_PASSED();
	return true;
}
