// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "include/ShaderTrace.h"
#include "source/Common.h"

#include "GL/glew.h"

#include <string>
#include <vector>


static bool  enableShaderSubgroupClock = false;
static bool  enableShaderDeviceClock   = false;

bool CompileGLSL (OUT std::string&				glslResult,
				  OUT ShaderTrace*				dbgInfo,
				  std::vector<const char *>		source,
				  EShLanguage					shaderType,
				  ETraceMode					mode);

bool CreateShader (OUT GLuint &			shader,
				   OUT ShaderTrace*		dbgInfo,
				   GLenum				shaderType,
				   const char *			source,
				   ETraceMode			mode);
