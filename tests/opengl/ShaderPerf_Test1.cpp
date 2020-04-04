// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#include "ShaderCompiler.h"

extern bool  ShaderPerf_Test1 ()
{
	static const char	vert_shader_source[] = R"#(
const vec2	g_Positions[] = {
	{-1.0f, -1.0f}, {-1.0f, 2.0f}, {2.0f, -1.0f},	// primitive 0 - must hit
	{-1.0f,  2.0f},									// primitive 1 - miss
	{-2.0f,  0.0f}									// primitive 2 - must hit
};

layout(location = 0) out vec4  out_Position;

layout(location=2) out VertOutput {
	vec2	out_Texcoord;
	vec4	out_Color;
};

void main()
{
	out_Position = vec4( g_Positions[gl_VertexID], float(gl_VertexID) * 0.01f, 1.0f );
	gl_Position = out_Position;
	out_Texcoord = g_Positions[gl_VertexID].xy * 0.5f + 0.5f;
	out_Color = mix(vec4(1.0, 0.3, 0.0, 0.8), vec4(0.6, 0.9, 0.1, 1.0), float(gl_VertexID) / float(g_Positions.length()));
})#";
	
	static const char	frag_shader_source[] = R"#(
layout(location = 0) out vec4  out_Color;

layout(location = 0) in vec4  in_Position;

layout(location=2) in VertOutput {
	vec2	in_Texcoord;
	vec4	in_Color;
};

float Fn2 (int i, float x)
{
	return sin( x ) * float(i);
}

float Fn1 (const int i, in vec2 k)
{
	float f = 0.0f;
	for (int j = 0; j < 10; ++j)
	{
		f += cos( 1.5432f * float(j) ) * Fn2( j, k[j&1] );

		if ( j+i == 12 )
			return f + 12.0f;
	}
	return f;
}

void main ()
{
	float c = Fn1( 3, in_Texcoord.xy + in_Position.yx );
	out_Color = vec4(c);
})#";
	
	GLuint		prog = glCreateProgram();
	GLuint		vert, frag;
	ShaderTrace	dbg_info;

	CHECK_ERR( CreateShader( OUT vert, nullptr, GL_VERTEX_SHADER, vert_shader_source, ETraceMode::None ));
	CHECK_ERR( CreateShader( OUT frag, OUT &dbg_info, GL_FRAGMENT_SHADER, frag_shader_source, ETraceMode::Performance ));
	
	glAttachShader( prog, vert );
	glAttachShader( prog, frag );
	glProgramParameteri( prog, GL_PROGRAM_SEPARABLE, GL_TRUE );
	glLinkProgram( prog );

	GLint	status = 0;
	glGetProgramiv( prog, GL_LINK_STATUS, OUT &status );

	if ( status != GL_TRUE )
	{
		GLchar	buf[1024] = {};
		glGetProgramInfoLog( prog, sizeof(buf), nullptr, buf );
		RETURN_ERR( "failed to link program" );
	}

	uint32_t	width = 16, height = 16;

	GLuint		dbg_buffer;
	uint64_t	dbg_buffer_size = 8 << 20;
	glGenBuffers( 1, OUT &dbg_buffer );
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, dbg_buffer );
	glBufferStorage( GL_SHADER_STORAGE_BUFFER, dbg_buffer_size, nullptr, GL_MAP_READ_BIT );
	uint32_t	zero = 0;
	glClearBufferData( GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero );
	uint32_t	data[] = { width/2, height/2 };		// selected pixel
	glBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, sizeof(data), data );
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 );

	GLuint	rt;
	glGenRenderbuffers( 1, OUT &rt );
	glBindRenderbuffer( GL_RENDERBUFFER, rt );
	glRenderbufferStorage( GL_RENDERBUFFER, GL_RGBA8, width, height );
	glBindRenderbuffer( GL_RENDERBUFFER, 0 );

	GLuint	fbo;
	glGenFramebuffers( 1, OUT &fbo );
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, fbo );
	glFramebufferRenderbuffer( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rt );

	glUseProgram( prog );
	glViewport( 0, 0, width, height );
	glDrawArrays( GL_TRIANGLE_STRIP, 0, 5 );

	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );

	glDeleteRenderbuffers( 1, &rt );
	glDeleteFramebuffers( 1, &fbo );
	glDeleteProgram( prog );
	glDeleteShader( vert );
	glDeleteShader( frag );
	
	glFinish();
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, dbg_buffer );
	void* trace = glMapBuffer( GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY );
	CHECK_ERR( trace );

	std::vector<std::string>	result;
	CHECK_ERR( dbg_info.ParseShaderTrace( trace, dbg_buffer_size, OUT result ));
	
	glUnmapBuffer( GL_SHADER_STORAGE_BUFFER );
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 );
	glDeleteBuffers( 1, &dbg_buffer );

	return true;
}
