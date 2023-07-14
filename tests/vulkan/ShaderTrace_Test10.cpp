// Copyright (c) Zhirnov Andrey. For more information see 'LICENSE'

#include "TestDevice.h"

/*
=================================================
	CompileShaders
=================================================
*/
static bool CompileShaders (TestDevice &vulkan, OUT VkShaderModule &meshShader, OUT VkShaderModule &fragShader)
{
	// create mesh shader
	{
		static const char	mesh_shader_source[] = R"#(
#extension GL_EXT_mesh_shader : require

layout(local_size_x=9, local_size_y=1, local_size_z=1) in;
layout(triangles) out;
layout(max_vertices=9, max_primitives=3) out;

//out uint3 gl_PrimitiveTriangleIndicesEXT [max_primitives];

const vec2	g_Positions[] = {
	{-1.0f, -1.0f}, {-1.0f,  2.0f}, { 2.0f, -1.0f},	// primitive 0 - must hit
	{-1.0f,  2.0f}, { 2.0f, -1.0f}, {-1.0f,  2.0f},	// primitive 1 - miss
	{ 2.0f, -1.0f}, {-1.0f,  2.0f}, {-2.0f,  0.0f}	// primitive 2 - must hit
};

out gl_MeshPerVertexEXT {
	vec4	gl_Position;
} gl_MeshVerticesEXT[]; // [max_vertices]

layout(location = 0) out MeshOutput {
	vec4	color;
} Output[]; // [max_vertices]

void dbg_EnableTraceRecording (bool b) {}

void main ()
{
	const uint I = gl_LocalInvocationID.x;

	dbg_EnableTraceRecording( gl_GlobalInvocationID.x == 0 );

	gl_MeshVerticesEXT[I].gl_Position	= vec4(g_Positions[I], 0.0f, 1.0f);
	Output[I].color						= g_Positions[I].xyxy * 0.5f + 0.5f;

	if ( I == 0 )
	{
		gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0,1,2);
		gl_PrimitiveTriangleIndicesEXT[1] = uvec3(3,4,5);
		gl_PrimitiveTriangleIndicesEXT[2] = uvec3(6,7,8);

		SetMeshOutputsEXT( 9, 3 );
	}
})#";

		CHECK_ERR( vulkan.Compile( OUT meshShader, {mesh_shader_source}, EShLangMesh, ETraceMode::DebugTrace, 0 ));
	}

	// create fragment shader
	{
		static const char	frag_shader_source[] = R"#(
layout(location = 0) out vec4  out_Color;

layout(location = 0) in MeshOutput {
	vec4	color;
} Input;

void main ()
{
	out_Color = Input.color;
})#";

		CHECK_ERR( vulkan.Compile( OUT fragShader, {frag_shader_source}, EShLangFragment ));
	}
	return true;
}

/*
=================================================
	ShaderTrace_Test10
=================================================
*/
extern bool ShaderTrace_Test10 (TestDevice& vulkan)
{
	if ( not vulkan.GetMeshShaderFeats().meshShader )
		return true;

	// create renderpass and framebuffer
	uint			width = 16, height = 16;
	VkRenderPass	render_pass;
	VkImage			color_target;
	VkFramebuffer	framebuffer;
	CHECK_ERR( vulkan.CreateRenderTarget( VK_FORMAT_R8G8B8A8_UNORM, width, height, 0,
										  OUT render_pass, OUT color_target, OUT framebuffer ));


	// create pipeline
	VkShaderModule	mesh_shader, frag_shader;
	CHECK_ERR( CompileShaders( vulkan, OUT mesh_shader, OUT frag_shader ));

	VkDescriptorSetLayout	ds_layout;
	VkDescriptorSet			desc_set;
	CHECK_ERR( vulkan.CreateDebugDescriptorSet( VK_SHADER_STAGE_MESH_BIT_EXT, OUT ds_layout, OUT desc_set ));

	VkPipelineLayout	ppln_layout;
	VkPipeline			pipeline;
	CHECK_ERR( vulkan.CreateMeshPipelineVar1( mesh_shader, frag_shader, ds_layout, render_pass, OUT ppln_layout, OUT pipeline ));


	// build command buffer
	VkCommandBufferBeginInfo	begin = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, null, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, null };
	VK_CHECK_ERR( vulkan.vkBeginCommandBuffer( vulkan.cmdBuffer, &begin ));

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
									 0, null, 0, null, 1, &barrier);
	}

	// setup storage buffer
	{
		vulkan.vkCmdFillBuffer( vulkan.cmdBuffer, vulkan.debugOutputBuf, 0, VK_WHOLE_SIZE, 0 );
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

		vulkan.vkCmdPipelineBarrier( vulkan.cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT, 0,
									 0, null, 1, &barrier, 0, null);
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
	vulkan.vkCmdBindDescriptorSets( vulkan.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ppln_layout, 0, 1, &desc_set, 0, null );

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

	vulkan.vkCmdDrawMeshTasksEXT( vulkan.cmdBuffer, 1, 1, 1 );

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

		vulkan.vkCmdPipelineBarrier( vulkan.cmdBuffer, VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
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

	CHECK_ERR( vulkan.TestDebugTraceOutput( {mesh_shader}, "ShaderTrace_Test10.txt" ));

	vulkan.FreeTempHandles();

	TEST_PASSED();
	return true;
}
