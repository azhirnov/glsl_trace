
#ifdef VKLOADER_STAGE_DECLFNPOINTER
	extern PFN_vkCreateInstance  _var_vkCreateInstance;
	extern PFN_vkEnumerateInstanceExtensionProperties  _var_vkEnumerateInstanceExtensionProperties;
	extern PFN_vkEnumerateInstanceLayerProperties  _var_vkEnumerateInstanceLayerProperties;
	extern PFN_vkEnumerateInstanceVersion  _var_vkEnumerateInstanceVersion;
#endif // VKLOADER_STAGE_DECLFNPOINTER


#ifdef VKLOADER_STAGE_FNPOINTER
	PFN_vkCreateInstance  _var_vkCreateInstance = null;
	PFN_vkEnumerateInstanceExtensionProperties  _var_vkEnumerateInstanceExtensionProperties = null;
	PFN_vkEnumerateInstanceLayerProperties  _var_vkEnumerateInstanceLayerProperties = null;
	PFN_vkEnumerateInstanceVersion  _var_vkEnumerateInstanceVersion = null;
#endif // VKLOADER_STAGE_FNPOINTER


#ifdef VKLOADER_STAGE_INLINEFN
	ND_ VKAPI_ATTR inline VkResult vkCreateInstance (const VkInstanceCreateInfo * pCreateInfo, const VkAllocationCallbacks * pAllocator, VkInstance * pInstance)								{ return _var_vkCreateInstance( pCreateInfo, pAllocator, pInstance ); }
	ND_ VKAPI_ATTR inline VkResult vkEnumerateInstanceExtensionProperties (const char * pLayerName, uint32_t * pPropertyCount, VkExtensionProperties * pProperties)								{ return _var_vkEnumerateInstanceExtensionProperties( pLayerName, pPropertyCount, pProperties ); }
	ND_ VKAPI_ATTR inline VkResult vkEnumerateInstanceLayerProperties (uint32_t * pPropertyCount, VkLayerProperties * pProperties)								{ return _var_vkEnumerateInstanceLayerProperties( pPropertyCount, pProperties ); }
	ND_ VKAPI_ATTR inline VkResult vkEnumerateInstanceVersion (uint32_t * pApiVersion)								{ return _var_vkEnumerateInstanceVersion( pApiVersion ); }
#endif // VKLOADER_STAGE_INLINEFN


#ifdef VKLOADER_STAGE_DUMMYFN
	VKAPI_ATTR VkResult VKAPI_CALL Dummy_vkCreateInstance (const VkInstanceCreateInfo * , const VkAllocationCallbacks * , VkInstance * )			{  VK_LOG( "used dummy function 'vkCreateInstance'" );  return VK_RESULT_MAX_ENUM;  }
	VKAPI_ATTR VkResult VKAPI_CALL Dummy_vkEnumerateInstanceExtensionProperties (const char * , uint32_t * , VkExtensionProperties * )			{  VK_LOG( "used dummy function 'vkEnumerateInstanceExtensionProperties'" );  return VK_RESULT_MAX_ENUM;  }
	VKAPI_ATTR VkResult VKAPI_CALL Dummy_vkEnumerateInstanceLayerProperties (uint32_t * , VkLayerProperties * )			{  VK_LOG( "used dummy function 'vkEnumerateInstanceLayerProperties'" );  return VK_RESULT_MAX_ENUM;  }
	VKAPI_ATTR VkResult VKAPI_CALL Dummy_vkEnumerateInstanceVersion (uint32_t * )			{  VK_LOG( "used dummy function 'vkEnumerateInstanceVersion'" );  return VK_RESULT_MAX_ENUM;  }
#endif // VKLOADER_STAGE_DUMMYFN


#ifdef VKLOADER_STAGE_GETADDRESS
	Load( OUT _var_vkCreateInstance, "vkCreateInstance", Dummy_vkCreateInstance );
	Load( OUT _var_vkEnumerateInstanceExtensionProperties, "vkEnumerateInstanceExtensionProperties", Dummy_vkEnumerateInstanceExtensionProperties );
	Load( OUT _var_vkEnumerateInstanceLayerProperties, "vkEnumerateInstanceLayerProperties", Dummy_vkEnumerateInstanceLayerProperties );
	Load( OUT _var_vkEnumerateInstanceVersion, "vkEnumerateInstanceVersion", Dummy_vkEnumerateInstanceVersion );
#endif // VKLOADER_STAGE_GETADDRESS

