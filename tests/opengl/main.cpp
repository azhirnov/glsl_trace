// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include "source/Common.h"

extern bool  ShaderPerf_Test1 ();
extern bool  ShaderTrace_Test1 ();


void GLAPIENTRY  DbgCallback (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	assert(message);
}


int main ()
{
	glslang::InitializeProcess();

	CHECK_ERR( glfwInit() == GLFW_TRUE );
	CHECK_ERR( glfwVulkanSupported() == GLFW_TRUE );
	glfwWindowHint( GLFW_CLIENT_API, GLFW_OPENGL_API );
	
	glfwWindowHint( GLFW_RED_BITS, 8 );
	glfwWindowHint( GLFW_GREEN_BITS, 8 );
	glfwWindowHint( GLFW_BLUE_BITS, 8 );
	glfwWindowHint( GLFW_DEPTH_BITS, 24 );
	
	GLFWwindow*	wnd = glfwCreateWindow( 800, 600, "GLSL-Trace", nullptr, nullptr );
	CHECK_ERR( wnd );
	
	glfwMakeContextCurrent( wnd );

	CHECK_ERR( glewInit() == GL_NO_ERROR );

	glDebugMessageCallback( DbgCallback, nullptr );
	glDebugMessageControl( GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE );
	glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );

	for (int i = 0;; ++i)
	{
		if ( glfwWindowShouldClose( wnd ))
			break;
		
		glfwPollEvents();

		if ( i > 10 )
		{
			//ShaderPerf_Test1();
			ShaderTrace_Test1();
			break;
		}
	}

	glfwDestroyWindow( wnd );
	glfwTerminate();
	
	glslang::FinalizeProcess();

	return 0;
}
