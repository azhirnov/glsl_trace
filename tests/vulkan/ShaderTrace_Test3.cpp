// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#include "Device.h"

/*
=================================================
	CompileShaders
=================================================
*/
static bool CompileShaders (Device &vulkan, OUT VkShaderModule &vertShader, OUT VkShaderModule &fragShader)
{
	// create vertex shader
	{
		static const char	vert_shader_source[] = R"#(
const vec2	g_Positions[] = {
	{-1.0f, -1.0f}, {-1.0f, 2.0f}, {2.0f, -1.0f},	// primitive 0 - must hit
};

void main()
{
	gl_Position	= vec4( g_Positions[gl_VertexIndex], float(gl_VertexIndex) * 0.02f, 1.0f );
})#";

		CHECK_ERR( vulkan.Compile( OUT vertShader, {vert_shader_source}, SPV_COMP_SHADER_TYPE_VERTEX ));
	}

	// create fragment shader
	{
		static const char	frag_shader_source[] = R"#(
layout(location = 0) out vec4  out_Color;

void main ()
{
	mat4x4	m1;
	m1[0] = vec4(1.0f);		m1[1] = vec4(2.0f);		m1[2] = vec4(3.0f);		m1[3] = vec4(4.0f);

	mat2x3	m2;
	m2[0] = vec3(11.1f);	m2[1] = vec3(12.2f);

	mat4x4	m3;
	m3[0] = vec4(6.0f);		m3[1] = vec4(7.0f);		m3[2] = vec4(8.0f);		m3[3] = vec4(9.0f);
	
	m3[0][1] = 44.0f;

	mat4x4	m4 = m1 * m3;

	out_Color = (m4 * vec4(8.0f)) + vec4(m2 * vec2(9.0f), 1.0f);
})#";

		CHECK_ERR( vulkan.Compile( OUT fragShader, {frag_shader_source}, SPV_COMP_SHADER_TYPE_FRAGMENT, SPV_COMP_DEBUG_MODE_TRACE, 0 ));
	}
	return true;
}

/*
=================================================
	ShaderTrace_Test3
=================================================
*/
extern bool ShaderTrace_Test3 (Device& vulkan)
{
	// create renderpass and framebuffer
	uint			width = 16, height = 16;
	VkRenderPass	render_pass;
	VkImage			color_target;
	VkFramebuffer	framebuffer;
	CHECK_ERR( vulkan.CreateRenderTarget( VK_FORMAT_R8G8B8A8_UNORM, width, height, 0,
										  OUT render_pass, OUT color_target, OUT framebuffer ));


	// create pipeline
	VkShaderModule	vert_shader, frag_shader;
	CHECK_ERR( CompileShaders( vulkan, OUT vert_shader, OUT frag_shader ));

	VkDescriptorSetLayout	ds_layout;
	VkDescriptorSet			desc_set;
	CHECK_ERR( vulkan.CreateDebugDescriptorSet( VK_SHADER_STAGE_FRAGMENT_BIT, OUT ds_layout, OUT desc_set ));

	VkPipelineLayout	ppln_layout;
	VkPipeline			pipeline;
	CHECK_ERR( vulkan.CreateGraphicsPipelineVar1( vert_shader, frag_shader, ds_layout, render_pass, OUT ppln_layout, OUT pipeline ));


	// build command buffer
	VkCommandBufferBeginInfo	begin = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr };
	VK_CHECK( vulkan.vkBeginCommandBuffer( vulkan.cmdBuffer, &begin ));

	// image layout undefined -> color_attachment
	{
		VkImageMemoryBarrier	barrier = {};
		barrier.sType			= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcAccessMask	= 0;
		barrier.dstAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.oldLayout		= VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier.image			= color_target;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange	= {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

		vulkan.vkCmdPipelineBarrier( vulkan.cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
									 0, nullptr, 0, nullptr, 1, &barrier);
	}
	
	// setup storage buffer
	{
		const uint	data[] = { width/2, height/2 };		// selected pixel

		vulkan.vkCmdFillBuffer( vulkan.cmdBuffer, vulkan.debugOutputBuf, sizeof(data), VK_WHOLE_SIZE, 0 );
		vulkan.vkCmdUpdateBuffer( vulkan.cmdBuffer, vulkan.debugOutputBuf, 0, sizeof(data), data );
	}

	// debug output storage read/write after write
	{
		VkBufferMemoryBarrier	barrier = {};
		barrier.sType			= VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		barrier.srcAccessMask	= VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask	= VK_ACCESS_SHADER_WRITE_BIT;
		barrier.buffer			= vulkan.debugOutputBuf;
		barrier.offset			= 0;
		barrier.size			= VK_WHOLE_SIZE;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		
		vulkan.vkCmdPipelineBarrier( vulkan.cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
									 0, nullptr, 1, &barrier, 0, nullptr);
	}

	// begin render pass
	{
		VkClearValue			clear_value = {{{ 0.0f, 0.0f, 0.0f, 1.0f }}};
		VkRenderPassBeginInfo	begin_rp	= {};
		begin_rp.sType				= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		begin_rp.framebuffer		= framebuffer;
		begin_rp.renderPass			= render_pass;
		begin_rp.renderArea			= { {0,0}, {width, height} };
		begin_rp.clearValueCount	= 1;
		begin_rp.pClearValues		= &clear_value;

		vulkan.vkCmdBeginRenderPass( vulkan.cmdBuffer, &begin_rp, VK_SUBPASS_CONTENTS_INLINE );
	}
			
	vulkan.vkCmdBindPipeline( vulkan.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
	vulkan.vkCmdBindDescriptorSets( vulkan.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ppln_layout, 0, 1, &desc_set, 0, nullptr );
	
	// set dynamic states
	{
		VkViewport	viewport = {};
		viewport.x			= 0.0f;
		viewport.y			= 0.0f;
		viewport.width		= float(width);
		viewport.height		= float(height);
		viewport.minDepth	= 0.0f;
		viewport.maxDepth	= 1.0f;
		vulkan.vkCmdSetViewport( vulkan.cmdBuffer, 0, 1, &viewport );

		VkRect2D	scissor_rect = { {0,0}, {width, height} };
		vulkan.vkCmdSetScissor( vulkan.cmdBuffer, 0, 1, &scissor_rect );
	}
			
	vulkan.vkCmdDraw( vulkan.cmdBuffer, 3, 1, 0, 0 );
			
	vulkan.vkCmdEndRenderPass( vulkan.cmdBuffer );

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
		
		vulkan.vkCmdPipelineBarrier( vulkan.cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
									 0, nullptr, 1, &barrier, 0, nullptr);
	}

	// copy shader debug output into host visible memory
	{
		VkBufferCopy	region = {};
		region.srcOffset	= 0;
		region.dstOffset	= 0;
		region.size			= VkDeviceSize(vulkan.debugOutputSize);

		vulkan.vkCmdCopyBuffer( vulkan.cmdBuffer, vulkan.debugOutputBuf, vulkan.readBackBuf, 1, &region );
	}

	VK_CHECK( vulkan.vkEndCommandBuffer( vulkan.cmdBuffer ));


	// submit commands and wait
	{
		VkSubmitInfo	submit = {};
		submit.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.commandBufferCount	= 1;
		submit.pCommandBuffers		= &vulkan.cmdBuffer;

		VK_CHECK( vulkan.vkQueueSubmit( vulkan.queue, 1, &submit, VK_NULL_HANDLE ));
		VK_CHECK( vulkan.vkQueueWaitIdle( vulkan.queue ));
	}
	
	CHECK_ERR( vulkan.TestDebugTraceOutput( {frag_shader}, "ShaderTrace_Test3.txt" ));
	
	vulkan.FreeTempHandles();
	
	TEST_PASSED();
	return true;
}
