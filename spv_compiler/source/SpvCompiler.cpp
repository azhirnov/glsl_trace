// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>

#include "SpvCompiler.h"
#include "ShaderTrace.h"
#include "../source/Common.h"

// glslang includes
#include "glslang/MachineIndependent/localintermediate.h"
#include "glslang/Include/intermediate.h"
#include "glslang/SPIRV/doc.h"
#include "glslang/SPIRV/disassemble.h"
#include "glslang/SPIRV/GlslangToSpv.h"
#include "glslang/SPIRV/GLSL.std.450.h"
#include "StandAlone/ResourceLimits.cpp"


struct CompiledShader
{
	string				log;
	vector<uint32_t>	spirv;
	ShaderTrace			trace;
};

struct ShaderTraceResult
{
	vector<string>	strings;
};


namespace
{
	using std::vector;
	using std::string;
	using std::string_view;
	using std::unique_ptr;
	using std::unordered_set;
	using std::unordered_map;
	namespace FS = std::filesystem;

	static_assert( SPV_COMP_SHADER_TYPE_VERTEX          == EShLangVertex		 &&
				   SPV_COMP_SHADER_TYPE_TESS_CONTROL    == EShLangTessControl	 &&
				   SPV_COMP_SHADER_TYPE_TESS_EVALUATION == EShLangTessEvaluation &&
				   SPV_COMP_SHADER_TYPE_GEOMETRY        == EShLangGeometry		 &&
				   SPV_COMP_SHADER_TYPE_FRAGMENT        == EShLangFragment		 &&
				   SPV_COMP_SHADER_TYPE_COMPUTE         == EShLangCompute		 &&
				   SPV_COMP_SHADER_TYPE_RAY_GEN         == EShLangRayGen		 &&
				   SPV_COMP_SHADER_TYPE_RAY_INTERSECT   == EShLangIntersect		 &&
				   SPV_COMP_SHADER_TYPE_RAY_ANY_HIT     == EShLangAnyHit		 &&
				   SPV_COMP_SHADER_TYPE_RAY_CLOSEST_HIT == EShLangClosestHit	 &&
				   SPV_COMP_SHADER_TYPE_RAY_MISS        == EShLangMiss			 &&
				   SPV_COMP_SHADER_TYPE_RAY_CALLABLE    == EShLangCallable		 &&
				   SPV_COMP_SHADER_TYPE_MESH_TASK       == EShLangTaskNV		 &&
				   SPV_COMP_SHADER_TYPE_MESH            == EShLangMeshNV,
				   "" );
	
	//
	// Shader Includer
	//
	class ShaderIncluder final : public glslang::TShader::Includer
	{
	// types
	private:
		struct IncludeResultImpl final : IncludeResult
		{
			const string	_data;

			IncludeResultImpl (string &&data, const string& headerName, void* userData = nullptr) :
				IncludeResult{headerName, nullptr, 0, userData}, _data{std::move(data)}
			{
				const_cast<const char*&>(headerData) = _data.c_str();
				const_cast<size_t&>(headerLength)    = _data.length();
			}

			ND_ string_view	GetSource () const	{ return _data; }
		};

		using IncludeResultPtr_t	= unique_ptr< IncludeResultImpl >;
		using IncludeResults_t		= vector< IncludeResultPtr_t >;
		using IncludedFiles_t		= unordered_map< string, IncludeResultImpl* >;


	// variables
	private:
		IncludeResults_t		_results;
		IncludedFiles_t			_includedFiles;
		vector<string> const&	_directories;


	// methods
	public:
		explicit ShaderIncluder (const vector<string> &dirs) : _directories{dirs} {}
		~ShaderIncluder () override {}

		ND_ IncludedFiles_t const&  GetIncludedFiles () const	{ return _includedFiles; }

		// TShader::Includer //
		IncludeResult*  includeSystem (const char* headerName, const char* includerName, size_t inclusionDepth) override;
		IncludeResult*  includeLocal (const char* headerName, const char* includerName, size_t inclusionDepth) override;

		void  releaseInclude (IncludeResult *) override {}
	};
	
/*
=================================================
	includeSystem
=================================================
*/
	ShaderIncluder::IncludeResult* ShaderIncluder::includeSystem (const char*, const char *, size_t)
	{
		return nullptr;
	}
	
/*
=================================================
	includeLocal
=================================================
*/
	ShaderIncluder::IncludeResult* ShaderIncluder::includeLocal (const char* headerName, const char *, size_t)
	{
		assert( _directories.size() );

		for (auto& folder : _directories)
		{
			FS::path	fpath = FS::path( folder ) / headerName;

			if ( not FS::exists( fpath ))
				continue;

			const string	filename = fpath.make_preferred().string();

			// prevent recursive include
			if ( _includedFiles.count( filename ))
				return _results.emplace_back(new IncludeResultImpl{ "// skip header\n", headerName }).get();
			
			FILE*	file = nullptr;
			fopen_s( OUT &file, filename.c_str(), "rb" );
			CHECK_ERR( file != nullptr );
			
			const long	curr = ftell( file );
			CHECK( fseek( file, 0, SEEK_END ) == 0 );

			const long	size = ftell( file );
			CHECK( fseek( file, curr, SEEK_SET ) == 0 );

			string	data;
			data.resize( size );
			size_t readn = fread( &data[0], 1, size, file );
			data.resize( readn );
			CHECK_ERR( readn == size );

			auto*	result = _results.emplace_back(new IncludeResultImpl{ std::move(data), headerName }).get();

			_includedFiles.insert_or_assign( filename, result );
			return result;
		}
		return nullptr;
	}
//-----------------------------------------------------------------------------
	
	
	//
	// GLSLang Result
	//
	struct GLSLangResult
	{
		glslang::TProgram				prog;
		unique_ptr< glslang::TShader >	shader;
	};

/*
=================================================
	ConvertShaderType
=================================================
*/
	ND_ static EShLanguage  ConvertShaderType (SPV_COMP_SHADER_TYPE shaderType)
	{
		BEGIN_ENUM_CHECKS();
		switch ( shaderType )
		{
			case SPV_COMP_SHADER_TYPE_VERTEX :			return EShLangVertex;
			case SPV_COMP_SHADER_TYPE_TESS_CONTROL :	return EShLangTessControl;
			case SPV_COMP_SHADER_TYPE_TESS_EVALUATION :	return EShLangTessEvaluation;
			case SPV_COMP_SHADER_TYPE_GEOMETRY :		return EShLangGeometry;
			case SPV_COMP_SHADER_TYPE_FRAGMENT :		return EShLangFragment;
			case SPV_COMP_SHADER_TYPE_COMPUTE :			return EShLangCompute;
			case SPV_COMP_SHADER_TYPE_MESH_TASK :		return EShLangTaskNV;
			case SPV_COMP_SHADER_TYPE_MESH :			return EShLangMeshNV;
			case SPV_COMP_SHADER_TYPE_RAY_GEN :			return EShLangRayGenNV;
			case SPV_COMP_SHADER_TYPE_RAY_ANY_HIT :		return EShLangAnyHitNV;
			case SPV_COMP_SHADER_TYPE_RAY_CLOSEST_HIT :	return EShLangClosestHitNV;
			case SPV_COMP_SHADER_TYPE_RAY_MISS :		return EShLangMissNV;
			case SPV_COMP_SHADER_TYPE_RAY_INTERSECT :	return EShLangIntersectNV;
			case SPV_COMP_SHADER_TYPE_RAY_CALLABLE :	return EShLangCallableNV;
		}
		END_ENUM_CHECKS();
		return EShLangCount;
	}

/*
=================================================
	ParseGLSL
=================================================
*/
	static bool  ParseGLSL (SPV_COMP_SHADER_TYPE shaderType, SPV_COMP_VERSION version, ShaderIncluder &includer,
							const char* const* shaderSources, const int* shaderSourceLengths, unsigned shaderSourcesCount, const char* entryName,
							const TBuiltInResource &builtinResource,
							OUT GLSLangResult &glslangData, OUT string &log)
	{
		using namespace glslang;

		const EShMessages	messages = EShMessages(EShMsgVulkanRules | EShMsgDebugInfo);
		const EShLanguage	stage	 = ConvertShaderType( shaderType );
		auto&				shader	 = glslangData.shader;

		shader.reset( new TShader( stage ));
		shader->setStringsWithLengths( shaderSources, shaderSourceLengths, shaderSourcesCount );
		shader->setEntryPoint( entryName );
		
		switch ( version )
		{
			case SPV_COMP_VERSION_VULKAN_1_0 :
				break;

			case SPV_COMP_VERSION_VULKAN_1_1 :
				shader->setEnvInput( EShSourceGlsl, stage, EShClientVulkan, 110 );
				shader->setEnvClient( EShClientVulkan, EShTargetVulkan_1_1 );
				shader->setEnvTarget( EShTargetSpv, EShTargetSpv_1_3 );
				break;

			case SPV_COMP_VERSION_VULKAN_1_1_SPIRV_1_4 :
				shader->setEnvInput( EShSourceGlsl, stage, EShClientVulkan, 110 );
				shader->setEnvClient( EShClientVulkan, EShTargetVulkan_1_1 );
				shader->setEnvTarget( EShTargetSpv, EShTargetSpv_1_4 );
				break;

			case SPV_COMP_VERSION_VULKAN_1_2 :
				shader->setEnvInput( EShSourceGlsl, stage, EShClientVulkan, 120 );
				shader->setEnvClient( EShClientVulkan, EShTargetVulkan_1_2 );
				shader->setEnvTarget( EShTargetSpv, EShTargetSpv_1_4 );
				break;

			default :
				return false;
		}

		if ( not shader->parse( &builtinResource, 460, ECoreProfile, false, true, messages, includer ))
		{
			log += shader->getInfoLog();
			return false;
		}
		
		glslangData.prog.addShader( shader.get() );

		if ( not glslangData.prog.link( messages ))
		{
			log += glslangData.prog.getInfoLog();
			return false;
		}

		return true;
	}

/*
=================================================
	CompileSPIRV
=================================================
*/
	bool  CompileSPIRV (const GLSLangResult &glslangData, OUT vector<uint32_t> &spirv, INOUT string &log)
	{
		using namespace glslang;

		const TIntermediate* intermediate = glslangData.prog.getIntermediate( glslangData.shader->getStage() );
		CHECK_ERR( intermediate );

		SpvOptions				spv_options;
		spv::SpvBuildLogger		logger;

		spv_options.generateDebugInfo	= false;
		spv_options.disableOptimizer	= true;
		spv_options.optimizeSize		= false;
		
		GlslangToSpv( *intermediate, OUT spirv, &logger, &spv_options );
		log += logger.getAllMessages();

		return true;
	}
	
/*
=================================================
	GenerateResources
=================================================
*/
	static void  GenerateResources (OUT TBuiltInResource& res)
	{
		res = {};

		res.maxLights = 0;
		res.maxClipPlanes = 6;
		res.maxTextureUnits = 32;
		res.maxTextureCoords = 32;
		res.maxVertexAttribs = 16;
		res.maxVertexUniformComponents = 4096;
		res.maxVaryingFloats = 64;
		res.maxVertexTextureImageUnits = 32;
		res.maxCombinedTextureImageUnits = 80;
		res.maxTextureImageUnits = 32;
		res.maxFragmentUniformComponents = 4096;
		res.maxDrawBuffers = 8;
		res.maxVertexUniformVectors = 128;
		res.maxVaryingVectors = 8;
		res.maxFragmentUniformVectors = 16;
		res.maxVertexOutputVectors = 16;
		res.maxFragmentInputVectors = 15;
		res.minProgramTexelOffset = -8;
		res.maxProgramTexelOffset = 7;
		res.maxClipDistances = 8;
		res.maxComputeWorkGroupCountX = 65535;
		res.maxComputeWorkGroupCountY = 65535;
		res.maxComputeWorkGroupCountZ = 65535;
		res.maxComputeWorkGroupSizeX = 1024;
		res.maxComputeWorkGroupSizeY = 1024;
		res.maxComputeWorkGroupSizeZ = 64;
		res.maxComputeUniformComponents = 1024;
		res.maxComputeTextureImageUnits = 16;
		res.maxComputeImageUniforms = 8;
		res.maxComputeAtomicCounters = 0;
		res.maxComputeAtomicCounterBuffers = 0;
		res.maxVaryingComponents = 60;
		res.maxVertexOutputComponents = 64;
		res.maxGeometryInputComponents = 64;
		res.maxGeometryOutputComponents = 128;
		res.maxFragmentInputComponents = 128;
		res.maxImageUnits = 8;
		res.maxCombinedImageUnitsAndFragmentOutputs = 8;
		res.maxImageSamples = 0;
		res.maxVertexImageUniforms = 0;
		res.maxTessControlImageUniforms = 0;
		res.maxTessEvaluationImageUniforms = 0;
		res.maxGeometryImageUniforms = 0;
		res.maxFragmentImageUniforms = 8;
		res.maxCombinedImageUniforms = 8;
		res.maxGeometryTextureImageUnits = 16;
		res.maxGeometryOutputVertices = 256;
		res.maxGeometryTotalOutputComponents = 1024;
		res.maxGeometryUniformComponents = 1024;
		res.maxGeometryVaryingComponents = 64;
		res.maxTessControlInputComponents = 128;
		res.maxTessControlOutputComponents = 128;
		res.maxTessControlTextureImageUnits = 16;
		res.maxTessControlUniformComponents = 1024;
		res.maxTessControlTotalOutputComponents = 4096;
		res.maxTessEvaluationInputComponents = 128;
		res.maxTessEvaluationOutputComponents = 128;
		res.maxTessEvaluationTextureImageUnits = 16;
		res.maxTessEvaluationUniformComponents = 1024;
		res.maxTessPatchComponents = 120;
		res.maxPatchVertices = 32;
		res.maxTessGenLevel = 64;
		res.maxViewports = 8;
		res.maxVertexAtomicCounters = 0;
		res.maxTessControlAtomicCounters = 0;
		res.maxTessEvaluationAtomicCounters = 0;
		res.maxGeometryAtomicCounters = 0;
		res.maxFragmentAtomicCounters = 0;
		res.maxCombinedAtomicCounters = 0;
		res.maxAtomicCounterBindings = 0;
		res.maxVertexAtomicCounterBuffers = 0;
		res.maxTessControlAtomicCounterBuffers = 0;
		res.maxTessEvaluationAtomicCounterBuffers = 0;
		res.maxGeometryAtomicCounterBuffers = 0;
		res.maxFragmentAtomicCounterBuffers = 0;
		res.maxCombinedAtomicCounterBuffers = 0;
		res.maxAtomicCounterBufferSize = 0;
		res.maxTransformFeedbackBuffers = 0;
		res.maxTransformFeedbackInterleavedComponents = 0;
		res.maxCullDistances = 8;
		res.maxCombinedClipAndCullDistances = 8;
		res.maxSamples = 4;
		res.maxMeshOutputVerticesNV = 256;
		res.maxMeshOutputPrimitivesNV = 512;
		res.maxMeshWorkGroupSizeX_NV = 32;
		res.maxMeshWorkGroupSizeY_NV = 1;
		res.maxMeshWorkGroupSizeZ_NV = 1;
		res.maxTaskWorkGroupSizeX_NV = 32;
		res.maxTaskWorkGroupSizeY_NV = 1;
		res.maxTaskWorkGroupSizeZ_NV = 1;
		res.maxMeshViewCountNV = 4;
		res.maxDualSourceDrawBuffersEXT = 0;

		res.limits.nonInductiveForLoops = 1;
		res.limits.whileLoops = 1;
		res.limits.doWhileLoops = 1;
		res.limits.generalUniformIndexing = 1;
		res.limits.generalAttributeMatrixVectorIndexing = 1;
		res.limits.generalVaryingIndexing = 1;
		res.limits.generalSamplerIndexing = 1;
		res.limits.generalVariableIndexing = 1;
		res.limits.generalConstantMatrixVectorIndexing = 1;
	}

	static TBuiltInResource&  GetBuiltInResources ()
	{
		static TBuiltInResource res;
		return res;
	}
//-----------------------------------------------------------------------------
	


/*
=================================================
	SpvCompileWithShaderTrace
=================================================
*/
	static int SpvCompileWithShaderTrace (const char* const* shaderSources, const int* shaderSourceLengths, unsigned shaderSourcesCount,
										  const char* entryName,
										  const char* const* includeDirs, unsigned includeDirsCount,
										  SPV_COMP_SHADER_TYPE shaderType,
										  SPV_COMP_VERSION version,
										  SPV_COMP_DEBUG_MODE mode,
										  const struct TBuiltInResource* resources,
										  unsigned DebugDescriptorSetIndex,
										  struct CompiledShader** outShader)
	{
		CHECK_ERR( shaderSources != nullptr );
		CHECK_ERR( shaderSourceLengths != nullptr );
		CHECK_ERR( shaderSourcesCount > 0 );
		CHECK_ERR( entryName != nullptr );
		CHECK_ERR( (includeDirs != nullptr) == (includeDirsCount > 0) );
		CHECK_ERR( outShader != nullptr );

		vector<string> dirs;
		dirs.resize( includeDirsCount );
		for (uint i = 0; i < includeDirsCount; ++i)
		{
			CHECK_ERR( includeDirs[i] != nullptr );
			dirs.push_back( includeDirs[i] );
		}

		if ( resources == nullptr )
			resources = &GetBuiltInResources();

		GLSLangResult	ast_result; // glslang abstract syntax tree
		ShaderIncluder	includer{ dirs };
		auto			shader = std::make_unique<CompiledShader>();

		if ( not ParseGLSL( shaderType, version, includer, shaderSources, shaderSourceLengths, shaderSourcesCount, entryName, *resources, OUT ast_result, OUT shader->log ))
			return 0;
	
		switch ( mode )
		{
			case SPV_COMP_DEBUG_MODE_TRACE :
			case SPV_COMP_DEBUG_MODE_PROFILE :
			{
				shader->trace.SetSource( shaderSources, shaderSourceLengths, shaderSourcesCount );

				for (auto& file : includer.GetIncludedFiles()) {
					shader->trace.IncludeSource( file.second->headerName.data(), file.second->GetSource().data(), file.second->GetSource().length() );
				}
				break;
			}
		}

		EShLanguage				stage	= ast_result.shader->getStage();
		glslang::TIntermediate&	interm	= *ast_result.prog.getIntermediate( stage );

		switch ( mode )
		{
			case SPV_COMP_DEBUG_MODE_TRACE :
				if ( not shader->trace.InsertTraceRecording( interm, DebugDescriptorSetIndex ))
					return 0;
				break;

			case SPV_COMP_DEBUG_MODE_PROFILE :
				if ( not shader->trace.InsertFunctionProfiler( interm, DebugDescriptorSetIndex, false, true ))
					return 0;
				break;

			case SPV_COMP_DEBUG_MODE_CLOCK_HEATMAP :
				if ( not shader->trace.InsertShaderClockHeatmap( interm, DebugDescriptorSetIndex ))
					return 0;
				break;
		}

		if ( not CompileSPIRV( ast_result, OUT shader->spirv, INOUT shader->log ))
			return 0;

		*outShader = shader.release();
		return 1;
	}

/*
=================================================
	TrimShader
=================================================
*/
	static int  TrimShader (struct CompiledShader* shader)
	{
		if ( shader == nullptr )
			return 0;

		shader->log.clear();
		shader->spirv.clear();
		return 1;
	}

/*
=================================================
	ReleaseShader
=================================================
*/
	static int  ReleaseShader (struct CompiledShader* shader)
	{
		if ( shader == nullptr )
			return 0;

		delete shader;
		return 1;
	}

/*
=================================================
	GetShaderLog
=================================================
*/
	static int  GetShaderLog (struct CompiledShader* shader, const char** outLog)
	{
		if ( shader == nullptr or outLog == nullptr )
			return 0;

		*outLog = shader->log.c_str();
		return 1;
	}

/*
=================================================
	GetShaderBinary
=================================================
*/
	static int  GetShaderBinary (struct CompiledShader* shader, const unsigned** outSpirv, unsigned* spirvSize)
	{
		if ( shader == nullptr or outSpirv == nullptr or spirvSize == nullptr )
			return 0;

		*outSpirv	= shader->spirv.data();
		*spirvSize	= unsigned(shader->spirv.size() * sizeof(shader->spirv[0]));
		return 1;
	}

/*
=================================================
	ParseShaderTrace
=================================================
*/
	static int  ParseShaderTrace (struct CompiledShader* shader,
								  const void* storageBuffer, unsigned long long storageBufferSize,
								  struct ShaderTraceResult** outResult)
	{
		if ( shader == nullptr or storageBuffer == nullptr or storageBufferSize == 0 )
			return 0;

		auto	result = std::make_unique<ShaderTraceResult>();

		CHECK_ERR( shader->trace.ParseShaderTrace( storageBuffer, storageBufferSize, OUT result->strings ));

		*outResult = result.release();
		return 1;
	}
	
/*
=================================================
	ReleaseTraceResult
=================================================
*/
	static int  ReleaseTraceResult (struct ShaderTraceResult* result)
	{
		if ( result == nullptr )
			return 0;

		delete result;
		return 1;
	}

/*
=================================================
	GetTraceResultCount
=================================================
*/
	static int  GetTraceResultCount (struct ShaderTraceResult* traceResult, unsigned* outCount)
	{
		if ( traceResult == nullptr or outCount == nullptr )
			return 0;

		*outCount = unsigned(traceResult->strings.size());
		return 1;
	}

/*
=================================================
	GetTraceResultString
=================================================
*/
	static int  GetTraceResultString (struct ShaderTraceResult* traceResult, unsigned index, const char** outString)
	{
		if ( traceResult == nullptr or outString == nullptr or index >= traceResult->strings.size() )
			return 0;

		*outString = traceResult->strings[index].c_str();
		return 1;
	}
//-----------------------------------------------------------------------------



	static void Initialize ()
	{
		glslang::InitializeProcess();
		GenerateResources( GetBuiltInResources() );
	}

	static void Deinitialize ()
	{
		glslang::FinalizeProcess();
	}

} // namespace
//-----------------------------------------------------------------------------


/*
=================================================
	GetSpvCompilerFn
=================================================
*/
int SPVCOMP_CALL  GetSpvCompilerFn (struct SpvCompilerFn* fn)
{
	fn->Compile					= &SpvCompileWithShaderTrace;
	fn->TrimShader				= &TrimShader;
	fn->ReleaseShader			= &ReleaseShader;
	fn->GetShaderLog			= &GetShaderLog;
	fn->GetShaderBinary			= &GetShaderBinary;
	fn->ParseShaderTrace		= &ParseShaderTrace;
	fn->ReleaseTraceResult		= &ReleaseTraceResult;
	fn->GetTraceResultCount		= &GetTraceResultCount;
	fn->GetTraceResultString	= &GetTraceResultString;
	return 1;
}


#ifdef WIN32
#include <Windows.h>

/*
=================================================
	DllMain
=================================================
*/
BOOL APIENTRY DllMain (HANDLE hModule,
					   DWORD  ulReason,
					   LPVOID lpReserved)
{
	switch (ulReason)
	{
		case DLL_PROCESS_ATTACH :
			Initialize();
			break;

		case DLL_THREAD_ATTACH :
			break;

		case DLL_THREAD_DETACH :
			break;

		case DLL_PROCESS_DETACH :
			Deinitialize();
			break;
	}

	return TRUE;
}

#endif
