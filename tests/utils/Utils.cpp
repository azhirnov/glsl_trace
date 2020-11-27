// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#include "Utils.h"
#include "../source/Common.h"

// Warning:
// Before testing on new GPU set 'UpdateReferences' to 'true', run tests,
// using git compare new references with origin, only float values may differ slightly.
// Then set 'UpdateReferences' to 'false' and run tests again.
// All tests must pass.
static const bool	UpdateReferences = true;

#ifndef _MSC_VER
#   define fopen_s( _outFile_, _name_, _mode_ ) (*_outFile_=fopen(_name_, _mode_))
#endif

/*
=================================================
	TestDebugTraceOutput
=================================================
*/
bool TestDebugTraceOutput (const std::vector<ShaderTrace*>&	shaders,
						   const void*						readBackPtr,
						   const std::string &				referenceFile)
{
	CHECK_ERR( referenceFile.size() );
	CHECK_ERR( shaders.size() );

	std::string					merged;
	std::vector<std::string>	debug_output;

	for (auto& module : shaders)
	{
		std::vector<std::string>	temp;
		CHECK_ERR( module->ParseShaderTrace( readBackPtr, BufferSize, OUT temp ));
		CHECK_ERR( temp.size() );
		debug_output.insert( debug_output.end(), temp.begin(), temp.end() );
	}

	std::sort( debug_output.begin(), debug_output.end() );
	for (auto& str : debug_output) { (merged += str) += "//---------------------------\n\n"; }

	if ( UpdateReferences )
	{
		FILE*	file = nullptr;
		fopen_s( OUT &file, (std::string{DATA_PATH} + referenceFile).c_str(), "wb" );
		CHECK_ERR( file );
		CHECK_ERR( fwrite( merged.c_str(), sizeof(merged[0]), merged.size(), file ) == merged.size() );
		fclose( file );

		return true;
	}

	std::string		file_data;
	{
		FILE*	file = nullptr;
		fopen_s( OUT &file, (std::string{DATA_PATH} + referenceFile).c_str(), "rb" );
		CHECK_ERR( file );
		
		CHECK_ERR( fseek( file, 0, SEEK_END ) == 0 );
		const long	size = ftell( file );
		CHECK_ERR( fseek( file, 0, SEEK_SET ) == 0 );

		file_data.resize( size );
		CHECK_ERR( fread( file_data.data(), sizeof(file_data[0]), file_data.size(), file ) == file_data.size() );
		fclose( file );
	}

	CHECK_ERR( file_data == merged );
	return true;
}

/*
=================================================
	CreateDebugOutputBuffer
=================================================
*/
#ifdef ENABLE_OPENGL
bool CreateDebugOutputBuffer (OUT GLuint &					dbgBuffer,
							  const std::vector<GLuint>&	programs)
{
	glGenBuffers( 1, OUT &dbgBuffer );
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, dbgBuffer );
	glBufferStorage( GL_SHADER_STORAGE_BUFFER, BufferSize, nullptr, GL_MAP_READ_BIT | GL_DYNAMIC_STORAGE_BIT );

	uint	zero = 0;
	glClearBufferData( GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero );

	glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, dbgBuffer );
	
	for (auto& prog : programs)
	{
		GLuint	sb_index = glGetProgramResourceIndex( prog, GL_SHADER_STORAGE_BLOCK, "dbg_ShaderTraceStorage" );

		if ( sb_index == GL_INVALID_INDEX )
			continue;

		glShaderStorageBlockBinding( prog, sb_index, 0 );
	}
	return true;
}
#endif	// ENABLE_OPENGL
