// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#include "ShaderCompiler.h"

// glslang includes
#include "glslang/MachineIndependent/localintermediate.h"
#include "glslang/Include/intermediate.h"
#include "glslang/SPIRV/doc.h"
#include "glslang/SPIRV/disassemble.h"
#include "glslang/SPIRV/GlslangToSpv.h"
#include "glslang/SPIRV/GLSL.std.450.h"
#include "StandAlone/ResourceLimits.cpp"
using namespace glslang;

// spirv cross
#include "spirv_cross.hpp"
#include "spirv_glsl.hpp"


bool  enableShaderSubgroupClock	= false;
bool  enableShaderDeviceClock	= false;

/*
=================================================
	CreateShader
=================================================
*/
bool CreateShader (OUT GLuint &			result,
				   OUT ShaderTrace*		dbgInfo,
				   GLenum				shaderType,
				   const char *			inSource,
				   ETraceMode			mode)
{
	EShLanguage		sh_lang;
	std::string		src;
	std::string		header = "#version 450 core\n"
							 "#extension GL_ARB_separate_shader_objects : require\n"
							 "#extension GL_ARB_shading_language_420pack : require\n";
	
	if (enableShaderSubgroupClock)
		header += "#extension GL_ARB_shader_clock : require\n";

	if (enableShaderDeviceClock)
		header += "#extension GL_EXT_shader_realtime_clock : require\n";

	switch ( shaderType )
	{
		case GL_COMPUTE_SHADER :			sh_lang = EShLangCompute;			break;
		case GL_VERTEX_SHADER :				sh_lang = EShLangVertex;			break;
		case GL_TESS_CONTROL_SHADER :		sh_lang = EShLangTessControl;		break;
		case GL_TESS_EVALUATION_SHADER :	sh_lang = EShLangTessEvaluation;	break;
		case GL_GEOMETRY_SHADER :			sh_lang = EShLangGeometry;			break;
		case GL_FRAGMENT_SHADER :			sh_lang = EShLangFragment;			break;
		default :
			RETURN_ERR( "unknown shader type" );
	}
	
	CHECK_ERR( CompileGLSL( OUT src, OUT dbgInfo, {header.c_str(), inSource}, sh_lang, mode ));

	GLuint	shader = glCreateShader( shaderType );

	inSource = src.c_str();
	glShaderSource( shader, 1, &inSource, nullptr );
	glCompileShader( shader );

	GLint	status = 0;
	glGetShaderiv( shader, GL_COMPILE_STATUS, OUT &status );

	if ( status != GL_TRUE )
	{
		GLchar	buf[1024] = {};
		glGetShaderInfoLog( shader, sizeof(buf), nullptr, buf );
		glDeleteShader( shader );

		RETURN_ERR( "shader compilation failed" );
	}

	GLuint	prog = glCreateProgram();
	glAttachShader( prog, shader );

	glProgramParameteri( prog, GL_PROGRAM_SEPARABLE, GL_TRUE );
	glLinkProgram( prog );
	glGetProgramiv( prog, GL_LINK_STATUS, OUT &status );

	if ( status != GL_TRUE )
	{
		GLchar	buf[1024] = {};
		glGetProgramInfoLog( prog, sizeof(buf), nullptr, buf );
		
		glDeleteShader( shader );
		glDeleteProgram( prog );

		RETURN_ERR( "failed to link program" );
	}
	
	glDeleteShader( shader );
	
	result = prog;
	return true;
}

/*
=================================================
	CompileGLSL
=================================================
*/
bool CompileGLSL (OUT std::string&				glslResult,
				  OUT ShaderTrace*				dbgInfo,
				  std::vector<const char *>		source,
				  EShLanguage					shaderType,
				  ETraceMode					mode)
{
	EShMessages				messages		= EShMsgDefault;
	TProgram				program;
	TShader					shader			{ shaderType };
	TBuiltInResource		builtin_res		= DefaultTBuiltInResource;

	shader.setStrings( source.data(), int(source.size()) );
	shader.setEntryPoint( "main" );
	shader.setEnvInput( EShSourceGlsl, shaderType, EShClientOpenGL, 110 );
	shader.setEnvClient( EShClientOpenGL, EShTargetOpenGL_450 );
	shader.setEnvTarget( EshTargetSpv, EShTargetSpv_1_3 );
	
	//shader.setAutoMapLocations( true );

	CHECK_ERR( shader.parse( &builtin_res, 450, ECoreProfile, false, true, messages ));

	program.addShader( &shader );

	CHECK_ERR( program.link( messages ));

	TIntermediate*	intermediate = program.getIntermediate( shader.getStage() );
	CHECK_ERR( intermediate );

	if ( dbgInfo )
	{
		switch ( mode )
		{
			case ETraceMode::DebugTrace :
				CHECK_ERR( dbgInfo->InsertTraceRecording( INOUT *intermediate, 8 ));
				break;

			case ETraceMode::Performance :
				CHECK_ERR( dbgInfo->InsertFunctionProfiler( INOUT *intermediate, 8, enableShaderSubgroupClock, enableShaderDeviceClock ));
				break;

			default :
				RETURN_ERR( "unknown shader trace mode" );
		}
	
		dbgInfo->SetSource( source.data(), nullptr, source.size() );
	}

	SpvOptions				spv_options;
	spv::SpvBuildLogger		logger;

	spv_options.generateDebugInfo	= false;
	spv_options.disableOptimizer	= true;
	spv_options.optimizeSize		= false;
	spv_options.validate			= false;
		
	std::vector<uint>	spirv;
	GlslangToSpv( *intermediate, OUT spirv, &logger, &spv_options );

	CHECK_ERR( spirv.size() );
	
	spirv_cross::CompilerGLSL			compiler {spirv.data(), spirv.size()};
	spirv_cross::CompilerGLSL::Options	opt = {};

	opt.version						= 450;
	opt.es							= false;
	opt.vulkan_semantics			= false;
	opt.separate_shader_objects		= true;
	opt.enable_420pack_extension	= true;

	opt.vertex.fixup_clipspace		= false;
	opt.vertex.flip_vert_y			= false;
	opt.vertex.support_nonzero_base_instance = true;

	opt.fragment.default_float_precision	= spirv_cross::CompilerGLSL::Options::Precision::Highp;
	opt.fragment.default_int_precision		= spirv_cross::CompilerGLSL::Options::Precision::Highp;

	compiler.set_common_options(opt);

	glslResult = compiler.compile();
	return true;
}
