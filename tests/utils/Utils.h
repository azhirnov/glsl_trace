// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include <vector>
#include "include/ShaderTrace.h"

bool TestDebugTraceOutput (const std::vector<ShaderTrace*>&	shaders,
						   const void*						readBackPtr,
						   uint64_t							maxBufferSize,
						   const std::string &				referenceFile);
