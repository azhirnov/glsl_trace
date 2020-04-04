// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include "source/Common.h"

extern bool  ShaderPerf_Test1 ();
extern bool  ShaderTrace_Test1 ();


void GLAPIENTRY  DbgCallback (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	return;
}


int main ()
{
	glslang::InitializeProcess();

	CHECK_ERR( glfwInit() == GLFW_TRUE );

	glfwWindowHint( GLFW_CLIENT_API, GLFW_OPENGL_API );
	glfwWindowHint( GLFW_DOUBLEBUFFER, 1 );
	glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 4 );
	glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 5 );
	glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, 1 );
	glfwWindowHint( GLFW_OPENGL_DEBUG_CONTEXT, 1 );
	glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
	
	GLFWwindow*	wnd = glfwCreateWindow( 800, 600, "GLSL-Trace", nullptr, nullptr );
	CHECK_ERR( wnd );
	
	glfwMakeContextCurrent( wnd );

	CHECK_ERR( glewInit() == GL_NO_ERROR );

	glDebugMessageCallback( DbgCallback, nullptr );
	glDebugMessageControl( GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE );
	glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
    glEnable( GL_DEBUG_OUTPUT );

	for (int i = 0; i < 10; ++i)
	{
		if ( glfwWindowShouldClose( wnd ))
			break;

		if ( i == 1 )
		{
			//ShaderPerf_Test1();
			ShaderTrace_Test1();
		}
		
		glfwPollEvents();
        glfwSwapBuffers( wnd );
	}

	glfwDestroyWindow( wnd );
	glfwTerminate();
	
	glslang::FinalizeProcess();

	return 0;
}
