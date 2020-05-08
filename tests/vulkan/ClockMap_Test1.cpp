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
	{ -1.0f,  1.0f },  { -1.0f, -1.0f },
	{  1.0f,  1.0f },  {  1.0f, -1.0f }
};

void main()
{
	gl_Position	= vec4( g_Positions[gl_VertexIndex], 0.0f, 1.0f );
})#";

		CHECK_ERR( vulkan.Compile( OUT vertShader, {vert_shader_source}, EShLangVertex ));
	}

	// create fragment shader
	{
		static const char	frag_shader_source[] = R"#(
layout(location = 0) out vec4  out_Color;

// DHash from https://www.shadertoy.com/view/4djSRW 
// MIT License...
// Copyright (c)2014 David Hoskins.
float DHash13 (const vec3 p)
{
	vec3 p3 = fract( p * 0.1031 );
	p3 += dot( p3, p3.yzx + 19.19 );
	return fract( (p3.x + p3.y) * p3.z );
}

vec3 DHash33 (const vec3 p)
{
	vec3 p3 = fract( p * vec3(0.1031, 0.1030, 0.0973) );
	p3 += dot( p3, p3.yxz + 19.19 );
	return fract( (p3.xxy + p3.yxx) * p3.zyx );
}

float ValueNoise (const vec3 pos)
{
	// from https://www.shadertoy.com/view/4sc3z2
	// license CC BY-NC-SA 3.0
#	define hash( _p_ )	(DHash13( _p_ ) * 2.0 - 1.0)

    vec3 pi = floor(pos);
    vec3 pf = pos - pi;
    
    vec3 w = pf * pf * (3.0 - 2.0 * pf);
    
    return 	mix(
        		mix(
        			mix(hash(pi + vec3(0, 0, 0)), hash(pi + vec3(1, 0, 0)), w.x),
        			mix(hash(pi + vec3(0, 0, 1)), hash(pi + vec3(1, 0, 1)), w.x), 
                    w.z),
        		mix(
                    mix(hash(pi + vec3(0, 1, 0)), hash(pi + vec3(1, 1, 0)), w.x),
        			mix(hash(pi + vec3(0, 1, 1)), hash(pi + vec3(1, 1, 1)), w.x), 
                    w.z),
        		w.y);
#	undef hash
}

vec4 Generator ()
{
	const int max_iter = 128;

	vec3	p = gl_FragCoord.xyz;
	float	accum = 0.0;

	for (uint i = 0; i < max_iter; ++i)
	{
		float	n = ValueNoise( p * 4.0 );
		accum += n;
		p += DHash33( p ) * 0.1;
	}

	return vec4(accum / float(max_iter));
}

void main ()
{
	out_Color = Generator();
})#";

		CHECK_ERR( vulkan.Compile( OUT fragShader, {frag_shader_source}, EShLangFragment, ETraceMode::TimeMap, 0 ));
	}
	return true;
}

/*
=================================================
	ClockMap_Test1
=================================================
*/
extern bool ClockMap_Test1 (Device& vulkan)
{
	if ( not vulkan.shaderClockFeat.shaderDeviceClock )
		return true;	// not supported

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
		const uint	data[] = { BitCast<uint>(1.0f), BitCast<uint>(1.0f),	// scale
							   width, height };								// dimension

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
			
	vulkan.vkCmdDraw( vulkan.cmdBuffer, 4, 1, 0, 0 );
			
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
		region.size			= vulkan.debugOutputSize;

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
	
	CHECK_ERR( vulkan.CheckTimeMap( {frag_shader} ));
	
	vulkan.FreeTempHandles();

	std::cout << "ClockMap_Test1 - passed" << std::endl;
	return true;
}
