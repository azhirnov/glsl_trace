// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#include "Device.h"
#include <iostream>

extern bool ShaderTrace_Test1 (Device& vulkan);
extern bool ShaderTrace_Test2 (Device& vulkan);
extern bool ShaderTrace_Test3 (Device& vulkan);
extern bool ShaderTrace_Test4 (Device& vulkan);
extern bool ShaderTrace_Test5 (Device& vulkan);
extern bool ShaderTrace_Test6 (Device& vulkan);
extern bool ShaderTrace_Test7 (Device& vulkan);
extern bool ShaderTrace_Test8 (Device& vulkan);
extern bool ShaderTrace_Test9 (Device& vulkan);
extern bool ShaderTrace_Test10 (Device& vulkan);
extern bool ShaderTrace_Test11 (Device& vulkan);
extern bool ShaderTrace_Test12 (Device& vulkan);
extern bool ShaderTrace_Test13 (Device& vulkan);
extern bool ShaderTrace_Test14 (Device& vulkan);

extern bool ShaderPerf_Test1 (Device& vulkan);

extern bool ClockMap_Test1 (Device& vulkan);
extern bool ClockMap_Test2 (Device& vulkan);


int main ()
{
	Device	vulkan;
	CHECK_ERR( vulkan.Create(), 1 );
	
	// run tests
	bool	passed = true;
	{
		passed &= ShaderTrace_Test1( vulkan );
		passed &= ShaderTrace_Test2( vulkan );
		//passed &= ShaderTrace_Test3( vulkan );
		passed &= ShaderTrace_Test4( vulkan );
		passed &= ShaderTrace_Test5( vulkan );
		passed &= ShaderTrace_Test6( vulkan );
		passed &= ShaderTrace_Test7( vulkan );
		passed &= ShaderTrace_Test8( vulkan );
		passed &= ShaderTrace_Test9( vulkan );
		passed &= ShaderTrace_Test10( vulkan );
		passed &= ShaderTrace_Test11( vulkan );
		passed &= ShaderTrace_Test12( vulkan );
		passed &= ShaderTrace_Test13( vulkan );
		passed &= ShaderTrace_Test14( vulkan );
	}
	
# ifdef VK_KHR_shader_clock
	if ( vulkan.hasShaderClock )
	{
		passed &= ShaderPerf_Test1( vulkan );
		passed &= ClockMap_Test1( vulkan );
		passed &= ClockMap_Test2( vulkan );
	}
# endif	// VK_KHR_shader_clock


	vulkan.Destroy();

	return 0;
}
