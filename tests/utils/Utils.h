// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include <vector>
#include <iostream>
#include "ShaderTrace.h"

static const size_t  BufferSize = 8 << 20;

bool TestDebugTraceOutput (const std::vector<ShaderTrace*>&	shaders,
						   const void*						readBackPtr,
						   const std::string &				referenceFile);

#ifdef ENABLE_OPENGL
#include "GL/glew.h"
bool CreateDebugOutputBuffer (GLuint &						dbgBuffer,
							  const std::vector<GLuint>&	programs);
#endif


// function name
#ifdef _MSC_VER
#	define FUNCTION_NAME			__FUNCTION__

#elif defined(__clang__) || defined(__gcc__)
#	define FUNCTION_NAME			__func__

#else
#	define FUNCTION_NAME			"unknown function"
#endif

#define TEST_PASSED()	std::cout << FUNCTION_NAME << " - passed" << std::endl;
