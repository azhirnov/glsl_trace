// Copyright (c) Zhirnov Andrey. For more information see 'LICENSE'
/*
	GLSL Trace project.

	old project: https://github.com/azhirnov/glsl_trace
	new project: https://gitflic.ru/project/azhirnov/as-en/file?file=engine%2Ftools%2Fres_pack%2Fshader_trace
*/

#pragma once

#include "Base.h"

namespace glslang {
	class TIntermediate;
	struct TSourceLoc;
}

namespace AE::PipelineCompiler
{

	//
	// Shader Trace
	//

	struct ShaderTrace final
	{
	// types
	public:
		enum class ELogFormat : uint
		{
			Unknown,
			Text,			// as plane text with part of source code 
			VS_Console,		// compatible with VS outpit, allow navigation to code by click
			VS,				// click to file path will open shader source file
			VSCode,			// click to file path will open shader source file in specified line
			_Count
		};

		enum class VariableID : uint { Unknown = ~0u };

		union SourcePoint
		{
			struct {
				uint	column;
				uint	line;
			}		_packed;
			ulong	_ul			= UMax;

			SourcePoint () {}
			SourcePoint (uint line, uint column) : _ul{(ulong(line) << 32) | column } {}
			explicit SourcePoint (const glslang::TSourceLoc &);

			ND_ bool  operator == (const SourcePoint &rhs)	const	{ return _ul == rhs._ul; }
			ND_ bool  operator >  (const SourcePoint &rhs)	const	{ return _ul >  rhs._ul; }

				void  SetMin (const SourcePoint &rhs)				{ _ul = Min( _ul, rhs._ul ); }
				void  SetMax (const SourcePoint &rhs)				{ _ul = Max( _ul, rhs._ul ); }

			ND_ uint  Line ()								const	{ return uint(_ul >> 32); }
			ND_ uint  Column ()								const	{ return uint(_ul & 0xFFFFFFFF); }
		};

		struct SourceLocation
		{
			uint			sourceId	= UMax;
			SourcePoint		begin;
			SourcePoint		end;

			SourceLocation () {}
			SourceLocation (uint sourceId, uint line, uint column);

			ND_ bool  operator == (const SourceLocation &rhs)	const;
			ND_ bool  IsNotDefined ()							const;
		};

		struct ExprInfo
		{
			VariableID			varID	= Default;	// ID of output variable
			uint				swizzle	= 0;
			SourceLocation		range;				// begin and end location of expression
			SourcePoint			point;				// location of operator
			Array<VariableID>	vars;				// all variables IDs in this expression

			ExprInfo () = default;
			ExprInfo (VariableID id, uint sw, const SourceLocation &range, const SourcePoint &pt) : varID{id}, swizzle{sw}, range{range}, point{pt} {} 

			ND_ bool  operator == (const ExprInfo &rhs) const;
		};

		struct SourceInfo
		{
			using LineRange = Pair< usize, usize >;

			String				filename;
			String				code;
			uint				firstLine	= 0;
			Array<LineRange>	lines;				// offset in bytes for each line in 'code'

			ND_ bool  operator == (const SourceInfo &rhs) const;
		};

		using VarNames_t	= HashMap< VariableID, String >;
		using ExprInfos_t	= Array< ExprInfo >;
		using Sources_t		= Array< SourceInfo >;
		using FileMap_t		= HashMap< String, uint >;	// index in '_sources'

		static constexpr uint	TBasicType_Clock	= 0xcc;	// 4x uint64

	private:
		static constexpr uint	InitialPositionMask	= 0x80000000u;
		static constexpr uint	MaxCount			= 1 << 10;


	// variables
	private:
		ExprInfos_t		_exprLocations;
		VarNames_t		_varNames;
		Sources_t		_sources;
		FileMap_t		_fileMap;
		ulong			_posOffset			= 0;
		ulong			_dataOffset			= 0;
		uint			_initialPosition	= 0;


	// methods
	public:
		ShaderTrace () {}

		ShaderTrace (ShaderTrace &&) = delete;
		ShaderTrace (const ShaderTrace &) = delete;
		ShaderTrace& operator = (ShaderTrace &&) = delete;
		ShaderTrace& operator = (const ShaderTrace &) = delete;

		// Log all function results, log all function calls, log some operator results.
		// Use 'ParseShaderTrace' to get trace as string.
		ND_ bool  InsertTraceRecording (glslang::TIntermediate &, uint descSetIndex);

		// Insert time measurement into user-defined functions.
		// Use 'ParseShaderTrace' to get trace as string.
		ND_ bool  InsertFunctionProfiler (glslang::TIntermediate &, uint descSetIndex, bool shaderSubgroupClock, bool shaderDeviceClock);

		// Insert time measurement into entry function, summarize shader invocation times in storage buffer.
		ND_ bool  InsertShaderClockHeatmap (glslang::TIntermediate &, uint descSetIndex);

		// Converts binary trace into string.
		ND_ bool  ParseShaderTrace (const void *ptr, Bytes maxSize, ELogFormat format, OUT Array<String> &result) const;

		// Source code required for 'ParseShaderTrace' function.
		void  AddSource (StringView source);
		void  AddSource (StringView filename, uint firstLine, StringView source);
		void  AddSource (const Path &filename, uint firstLine, StringView source);

		void  IncludeSource (StringView headerName, const Path &fullPath, StringView source);	// if used '#include'

		void  GetSource (OUT String &result) const;

		ND_ bool  operator == (const ShaderTrace &rhs) const;

	private:
		void  _AppendSource (String filename, uint firstLine, String source);
	};


} // AE::PipelineCompiler
