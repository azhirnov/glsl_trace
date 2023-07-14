// Copyright (c) Zhirnov Andrey. For more information see 'LICENSE'

#include "TestDevice.h"
#include <iostream>

extern bool ShaderTrace_Test1 (TestDevice& vulkan);
extern bool ShaderTrace_Test2 (TestDevice& vulkan);
extern bool ShaderTrace_Test3 (TestDevice& vulkan);
extern bool ShaderTrace_Test4 (TestDevice& vulkan);
extern bool ShaderTrace_Test5 (TestDevice& vulkan);
extern bool ShaderTrace_Test6 (TestDevice& vulkan);
extern bool ShaderTrace_Test7 (TestDevice& vulkan);
extern bool ShaderTrace_Test8 (TestDevice& vulkan);
extern bool ShaderTrace_Test9 (TestDevice& vulkan);
extern bool ShaderTrace_Test10 (TestDevice& vulkan);
extern bool ShaderTrace_Test11 (TestDevice& vulkan);
extern bool ShaderTrace_Test12 (TestDevice& vulkan);
extern bool ShaderTrace_Test13 (TestDevice& vulkan);
extern bool ShaderTrace_Test14 (TestDevice& vulkan);

extern bool ShaderPerf_Test1 (TestDevice& vulkan);

extern bool ClockMap_Test1 (TestDevice& vulkan);
extern bool ClockMap_Test2 (TestDevice& vulkan);


int main ()
{
	TestDevice	vulkan;
	CHECK_ERR( vulkan.Create(), 1 );
	
	// run tests
	bool	passed = true;
	{
		passed &= ShaderTrace_Test1( vulkan );		// graphics
		passed &= ShaderTrace_Test2( vulkan );		// compute
		//passed &= ShaderTrace_Test3( vulkan );	// graphics
		passed &= ShaderTrace_Test4( vulkan );		// graphics
		passed &= ShaderTrace_Test5( vulkan );		// graphics
		passed &= ShaderTrace_Test6( vulkan );		// geometry
		passed &= ShaderTrace_Test7( vulkan );		// tessellation
		passed &= ShaderTrace_Test8( vulkan );		// tessellation
		passed &= ShaderTrace_Test9( vulkan );		// ray tracing
		passed &= ShaderTrace_Test10( vulkan );		// mesh
		passed &= ShaderTrace_Test11( vulkan );		// compute
		passed &= ShaderTrace_Test12( vulkan );		// compute
		passed &= ShaderTrace_Test13( vulkan );		// graphics
		passed &= ShaderTrace_Test14( vulkan );		// ray tracing
	}
	
	if ( vulkan.hasShaderClock )
	{
		passed &= ShaderPerf_Test1( vulkan );		// graphics
		passed &= ClockMap_Test1( vulkan );			// graphics
		passed &= ClockMap_Test2( vulkan );			// ray tracing
	}

	CHECK_ERR( passed );

	vulkan.Destroy();

	return 0;
}
