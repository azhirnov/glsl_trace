// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#ifdef VKLOADER_STAGE_DECLFNPOINTER
	extern PFN_vkEnumerateInstanceLayerProperties  _var_vkEnumerateInstanceLayerProperties;
	extern PFN_vkGetInstanceProcAddr  _var_vkGetInstanceProcAddr;
	extern PFN_vkDeviceMemoryReportCallbackEXT  _var_vkDeviceMemoryReportCallbackEXT;
	extern PFN_vkCreateInstance  _var_vkCreateInstance;
	extern PFN_vkDebugUtilsMessengerCallbackEXT  _var_vkDebugUtilsMessengerCallbackEXT;
	extern PFN_vkDebugReportCallbackEXT  _var_vkDebugReportCallbackEXT;
	extern PFN_vkEnumerateInstanceVersion  _var_vkEnumerateInstanceVersion;
	extern PFN_vkEnumerateInstanceExtensionProperties  _var_vkEnumerateInstanceExtensionProperties;
#endif // VKLOADER_STAGE_DECLFNPOINTER


#ifdef VKLOADER_STAGE_FNPOINTER
	PFN_vkEnumerateInstanceLayerProperties  _var_vkEnumerateInstanceLayerProperties = nullptr;
	PFN_vkGetInstanceProcAddr  _var_vkGetInstanceProcAddr = nullptr;
	PFN_vkDeviceMemoryReportCallbackEXT  _var_vkDeviceMemoryReportCallbackEXT = nullptr;
	PFN_vkCreateInstance  _var_vkCreateInstance = nullptr;
	PFN_vkDebugUtilsMessengerCallbackEXT  _var_vkDebugUtilsMessengerCallbackEXT = nullptr;
	PFN_vkDebugReportCallbackEXT  _var_vkDebugReportCallbackEXT = nullptr;
	PFN_vkEnumerateInstanceVersion  _var_vkEnumerateInstanceVersion = nullptr;
	PFN_vkEnumerateInstanceExtensionProperties  _var_vkEnumerateInstanceExtensionProperties = nullptr;
#endif // VKLOADER_STAGE_FNPOINTER


#ifdef VKLOADER_STAGE_INLINEFN
	ND_ VKAPI_ATTR inline VkResult vkEnumerateInstanceLayerProperties (uint32_t * pPropertyCount, VkLayerProperties * pProperties)								{ return _var_vkEnumerateInstanceLayerProperties( pPropertyCount, pProperties ); }
	ND_ VKAPI_ATTR inline PFN_vkVoidFunction vkGetInstanceProcAddr (VkInstance instance, const char * pName)								{ return _var_vkGetInstanceProcAddr( instance, pName ); }
		VKAPI_ATTR inline void vkDeviceMemoryReportCallbackEXT (const VkDeviceMemoryReportCallbackDataEXT * pCallbackData, void * pUserData)								{ return _var_vkDeviceMemoryReportCallbackEXT( pCallbackData, pUserData ); }
	ND_ VKAPI_ATTR inline VkResult vkCreateInstance (const VkInstanceCreateInfo * pCreateInfo, const VkAllocationCallbacks * pAllocator, VkInstance * pInstance)								{ return _var_vkCreateInstance( pCreateInfo, pAllocator, pInstance ); }
	ND_ VKAPI_ATTR inline VkBool32 vkDebugUtilsMessengerCallbackEXT (VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData, void * pUserData)								{ return _var_vkDebugUtilsMessengerCallbackEXT( messageSeverity, messageTypes, pCallbackData, pUserData ); }
	ND_ VKAPI_ATTR inline VkBool32 vkDebugReportCallbackEXT (VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char * pLayerPrefix, const char * pMessage, void * pUserData)								{ return _var_vkDebugReportCallbackEXT( flags, objectType, object, location, messageCode, pLayerPrefix, pMessage, pUserData ); }
	ND_ VKAPI_ATTR inline VkResult vkEnumerateInstanceVersion (uint32_t * pApiVersion)								{ return _var_vkEnumerateInstanceVersion( pApiVersion ); }
	ND_ VKAPI_ATTR inline VkResult vkEnumerateInstanceExtensionProperties (const char * pLayerName, uint32_t * pPropertyCount, VkExtensionProperties * pProperties)								{ return _var_vkEnumerateInstanceExtensionProperties( pLayerName, pPropertyCount, pProperties ); }
#endif // VKLOADER_STAGE_INLINEFN


#ifdef VKLOADER_STAGE_DUMMYFN
	VKAPI_ATTR VkResult VKAPI_CALL Dummy_vkEnumerateInstanceLayerProperties (uint32_t * , VkLayerProperties * )			{  VK_LOG( "used dummy function 'vkEnumerateInstanceLayerProperties'" );  return VK_RESULT_MAX_ENUM;  }
	VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Dummy_vkGetInstanceProcAddr (VkInstance , const char * )			{  VK_LOG( "used dummy function 'vkGetInstanceProcAddr'" );  return nullptr;  }
	VKAPI_ATTR void VKAPI_CALL Dummy_vkDeviceMemoryReportCallbackEXT (const VkDeviceMemoryReportCallbackDataEXT * , void * )			{  VK_LOG( "used dummy function 'vkDeviceMemoryReportCallbackEXT'" );  return;  }
	VKAPI_ATTR VkResult VKAPI_CALL Dummy_vkCreateInstance (const VkInstanceCreateInfo * , const VkAllocationCallbacks * , VkInstance * )			{  VK_LOG( "used dummy function 'vkCreateInstance'" );  return VK_RESULT_MAX_ENUM;  }
	VKAPI_ATTR VkBool32 VKAPI_CALL Dummy_vkDebugUtilsMessengerCallbackEXT (VkDebugUtilsMessageSeverityFlagBitsEXT , VkDebugUtilsMessageTypeFlagsEXT , const VkDebugUtilsMessengerCallbackDataEXT * , void * )			{  VK_LOG( "used dummy function 'vkDebugUtilsMessengerCallbackEXT'" );  return VkBool32(0);  }
	VKAPI_ATTR VkBool32 VKAPI_CALL Dummy_vkDebugReportCallbackEXT (VkDebugReportFlagsEXT , VkDebugReportObjectTypeEXT , uint64_t , size_t , int32_t , const char * , const char * , void * )			{  VK_LOG( "used dummy function 'vkDebugReportCallbackEXT'" );  return VkBool32(0);  }
	VKAPI_ATTR VkResult VKAPI_CALL Dummy_vkEnumerateInstanceVersion (uint32_t * )			{  VK_LOG( "used dummy function 'vkEnumerateInstanceVersion'" );  return VK_RESULT_MAX_ENUM;  }
	VKAPI_ATTR VkResult VKAPI_CALL Dummy_vkEnumerateInstanceExtensionProperties (const char * , uint32_t * , VkExtensionProperties * )			{  VK_LOG( "used dummy function 'vkEnumerateInstanceExtensionProperties'" );  return VK_RESULT_MAX_ENUM;  }
#endif // VKLOADER_STAGE_DUMMYFN


#ifdef VKLOADER_STAGE_GETADDRESS
	Load( OUT _var_vkEnumerateInstanceLayerProperties, "vkEnumerateInstanceLayerProperties", Dummy_vkEnumerateInstanceLayerProperties );
	Load( OUT _var_vkGetInstanceProcAddr, "vkGetInstanceProcAddr", Dummy_vkGetInstanceProcAddr );
	Load( OUT _var_vkDeviceMemoryReportCallbackEXT, "vkDeviceMemoryReportCallbackEXT", Dummy_vkDeviceMemoryReportCallbackEXT );
	Load( OUT _var_vkCreateInstance, "vkCreateInstance", Dummy_vkCreateInstance );
	Load( OUT _var_vkDebugUtilsMessengerCallbackEXT, "vkDebugUtilsMessengerCallbackEXT", Dummy_vkDebugUtilsMessengerCallbackEXT );
	Load( OUT _var_vkDebugReportCallbackEXT, "vkDebugReportCallbackEXT", Dummy_vkDebugReportCallbackEXT );
	Load( OUT _var_vkEnumerateInstanceVersion, "vkEnumerateInstanceVersion", Dummy_vkEnumerateInstanceVersion );
	Load( OUT _var_vkEnumerateInstanceExtensionProperties, "vkEnumerateInstanceExtensionProperties", Dummy_vkEnumerateInstanceExtensionProperties );
#endif // VKLOADER_STAGE_GETADDRESS

