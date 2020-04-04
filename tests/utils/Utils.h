// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include <vector>
#include "include/ShaderTrace.h"

static const size_t  BufferSize = 8 << 20;

bool TestDebugTraceOutput (const std::vector<ShaderTrace*>&	shaders,
						   const void*						readBackPtr,
						   const std::string &				referenceFile);

#ifdef ENABLE_OPENGL
#include "GL/glew.h"
bool CreateDebugOutputBuffer (GLuint &						dbgBuffer,
							  const std::vector<GLuint>&	programs);
#endif
