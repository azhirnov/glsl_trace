// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#ifdef _MSC_VER
#	define DLL_EXPORT	__declspec(dllexport)
#	define DLL_IMPORT	__declspec(dllimport)
#endif

#ifdef SPV_COMPILER_BUILD_DLL
#	define SPVCOMP_CALL		DLL_EXPORT
#else
#	define SPVCOMP_CALL		DLL_IMPORT
#endif

extern "C"
{

enum SPV_COMP_DEBUG_MODE
{
	SPV_COMP_DEBUG_MODE_NONE			= 0,
	SPV_COMP_DEBUG_MODE_TRACE			= 1,
	SPV_COMP_DEBUG_MODE_PROFILE			= 2,
	SPV_COMP_DEBUG_MODE_CLOCK_HEATMAP	= 3,
};

enum SPV_COMP_SHADER_TYPE
{
	SPV_COMP_SHADER_TYPE_VERTEX				= 0,
	SPV_COMP_SHADER_TYPE_TESS_CONTROL		= 1,
	SPV_COMP_SHADER_TYPE_TESS_EVALUATION	= 2,
	SPV_COMP_SHADER_TYPE_GEOMETRY			= 3,
	SPV_COMP_SHADER_TYPE_FRAGMENT			= 4,
	SPV_COMP_SHADER_TYPE_COMPUTE			= 5,
	SPV_COMP_SHADER_TYPE_RAY_GEN			= 6,
	SPV_COMP_SHADER_TYPE_RAY_INTERSECT		= 7,
	SPV_COMP_SHADER_TYPE_RAY_ANY_HIT		= 8,
	SPV_COMP_SHADER_TYPE_RAY_CLOSEST_HIT	= 9,
	SPV_COMP_SHADER_TYPE_RAY_MISS			= 10,
	SPV_COMP_SHADER_TYPE_RAY_CALLABLE		= 11,
	SPV_COMP_SHADER_TYPE_MESH_TASK			= 12,
	SPV_COMP_SHADER_TYPE_MESH				= 13,
};

enum SPV_COMP_VERSION
{
	SPV_COMP_VERSION_VULKAN_1_0				= 100100,
	SPV_COMP_VERSION_VULKAN_1_1				= 100110,
	SPV_COMP_VERSION_VULKAN_1_1_SPIRV_1_4	= 100114,
	SPV_COMP_VERSION_VULKAN_1_2				= 100125,
};

enum SPV_COMP_OPTIMIZATION
{
	SPV_COMP_OPTIMIZATION_NONE		= 0,
	SPV_COMP_OPTIMIZATION_DEBUG		= 1,
	SPV_COMP_OPTIMIZATION_FAST		= 2,
	SPV_COMP_OPTIMIZATION_STRONG	= 3,
};

struct ShaderParams
{
	const char* const*				shaderSources;
	const int*						shaderSourceLengths;
	unsigned						shaderSourcesCount;
	const char*						entryName;					// replaced by 'main' if null
	const char*						defines;					// can be null
	const char* const*				includeDirs;				// can be null
	unsigned						includeDirsCount;
	SPV_COMP_SHADER_TYPE			shaderType;
	SPV_COMP_VERSION				version;
	SPV_COMP_DEBUG_MODE				mode;
	SPV_COMP_OPTIMIZATION			optimization;
	const struct TBuiltInResource*	resources;					// can be null
	unsigned						debugDescriptorSetIndex;
	bool							autoMapBindings;
	bool							autoMapLocations;

#ifdef __cplusplus
	ShaderParams() :
		shaderSources			{nullptr},
		shaderSourceLengths		{nullptr},
		shaderSourcesCount		{0},
		entryName				{nullptr},
		defines					{nullptr},
		includeDirs				{nullptr},
		includeDirsCount		{0},
		shaderType				{SPV_COMP_SHADER_TYPE(-1)},
		version					{SPV_COMP_VERSION_VULKAN_1_0},
		mode					{SPV_COMP_DEBUG_MODE_NONE},
		optimization			{SPV_COMP_OPTIMIZATION_NONE},
		resources				{nullptr},
		debugDescriptorSetIndex	{~0u},
		autoMapBindings			{false},
		autoMapLocations		{false}
	{}
#endif
};

struct CompiledShader;
struct ShaderTraceResult;
struct TBuiltInResource;

struct SpvCompilerFn
{
	int (*Compile) (const struct ShaderParams *params, struct CompiledShader** outShader);
	int (*TrimShader) (struct CompiledShader* shader);
	int (*ReleaseShader) (struct CompiledShader* shader);
	int (*GetShaderLog) (struct CompiledShader* shader, const char** outLog);
	int (*GetShaderBinary) (struct CompiledShader* shader, const unsigned** outSpirv, unsigned* spirvSize);

	int (*ParseShaderTrace) (struct CompiledShader* shader,
							 const void* storageBuffer, unsigned long long storageBufferSize,
							 struct ShaderTraceResult** outResult);
	int (*ReleaseTraceResult) (struct ShaderTraceResult* result);

	int (*GetTraceResultCount)  (struct ShaderTraceResult* traceResult, unsigned* outCount);
	int (*GetTraceResultString) (struct ShaderTraceResult* traceResult, unsigned index, const char** outString);
};

int SPVCOMP_CALL  GetSpvCompilerFn (struct SpvCompilerFn* fn);

} // extern "C"
