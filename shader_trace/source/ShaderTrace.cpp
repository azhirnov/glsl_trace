// Copyright (c) Zhirnov Andrey. For more information see 'LICENSE'

#include "Common.h"

namespace AE::PipelineCompiler
{

/*
=================================================
	ExprInfo
=================================================
*/
	bool  ShaderTrace::ExprInfo::operator == (const ExprInfo &rhs) const
	{
		return	(varID		== rhs.varID)	&
				(swizzle	== rhs.swizzle)	&
				(range		== rhs.range)	&
				(point		== rhs.point)	&
				(vars		== rhs.vars);
	}
//-----------------------------------------------------------------------------


/*
=================================================
	SourceInfo
=================================================
*/
	bool  ShaderTrace::SourceInfo::operator == (const SourceInfo &rhs) const
	{
		return	(code		== rhs.code)		&
				(filename	== rhs.filename)	&
				(firstLine	== rhs.firstLine)	&
				(lines		== rhs.lines);
	}
//-----------------------------------------------------------------------------



/*
=================================================
	SourceLocation
=================================================
*/
	ShaderTrace::SourceLocation::SourceLocation (uint sourceId, uint line, uint column) :
		sourceId{sourceId},
		begin{line, column},
		end{line, column}
	{}

	bool  ShaderTrace::SourceLocation::operator == (const SourceLocation &rhs) const
	{
		return	(sourceId	== rhs.sourceId)	&
				(begin		== rhs.begin)		&
				(end		== rhs.end);
	}

	bool  ShaderTrace::SourceLocation::IsNotDefined () const
	{
		return	(sourceId	== 0)	&
				(begin._ul	== 0)	&
				(end._ul	== 0);
	}
//-----------------------------------------------------------------------------

	
/*
=================================================
	FindAndReplace
=================================================
*/
	inline uint  FindAndReplace (INOUT String& str, char oldSymb, char newSymb)
	{
		String::size_type	pos		= 0;
		uint				count	= 0;

		while ( (pos = StringView{str}.find( oldSymb, pos )) != StringView::npos )
		{
			str[pos] = newSymb;
			++pos;
			++count;
		}
		return count;
	}

/*
=================================================
	_AppendSource
=================================================
*/
	void  ShaderTrace::_AppendSource (String filename, uint firstLine, String source)
	{
		SourceInfo	info;
		usize		pos = 0;

		FindAndReplace( INOUT filename, '\\', '/' );

		info.filename	= RVRef(filename);
		info.code		= RVRef(source);
		info.firstLine	= firstLine;
		info.lines.reserve( 64 );

		for (usize j = 0, len = info.code.length(); j < len; ++j)
		{
			const char	c = info.code[j];
			const char	n = (j+1) >= len ? 0 : info.code[j+1];

			// windows style "\r\n"
			if ( c == '\r' and n == '\n' )
			{
				info.lines.emplace_back( pos, j );
				pos = (++j) + 1;
			}
			else
			// linux style "\n" (or mac style "\r")
			if ( c == '\n' or c == '\r' )
			{
				info.lines.emplace_back( pos, j );
				pos = j + 1;
			}
		}

		if ( pos < info.code.length() )
			info.lines.emplace_back( pos, info.code.length() );

		_sources.push_back( RVRef(info) );
	}

/*
=================================================
	AddSource
=================================================
*/
	void  ShaderTrace::AddSource (StringView source)
	{
		CHECK_ERRV( not source.empty() );

		_AppendSource( Default, 0, String{source} );
	}

	void  ShaderTrace::AddSource (StringView filename, uint firstLine, StringView source)
	{
		CHECK_ERRV( not source.empty() );

		_AppendSource( String{filename}, firstLine, String{source} );
	}

	void  ShaderTrace::AddSource (const Path &filename, uint firstLine, StringView source)
	{
		CHECK_ERRV( not source.empty() );

		return _AppendSource( ToString(filename), firstLine, String{source} );
	}

/*
=================================================
	IncludeSource
=================================================
*/
	void  ShaderTrace::IncludeSource (StringView headerName, const Path &fullPath, StringView source)
	{
		CHECK_ERRV( not source.empty() );
		CHECK_ERRV( not headerName.empty() );

		_fileMap.insert_or_assign( String(headerName), uint(_sources.size()) );
		_AppendSource( ToString(fullPath), 0, String{source} );
	}

/*
=================================================
	GetSource
=================================================
*/
	void  ShaderTrace::GetSource (OUT String &result) const
	{
		usize	total_size = _sources.size()*2;

		for (auto& src : _sources) {
			total_size += src.code.length();
		}

		result.clear();
		result.reserve( total_size );

		for (auto& src : _sources) {
			result.append( src.code );
		}
	}

/*
=================================================
	operator ==
=================================================
*/
	bool  ShaderTrace::operator == (const ShaderTrace &rhs) const
	{
		return	_exprLocations		== rhs._exprLocations	and
				_varNames			== rhs._varNames		and
				_sources			== rhs._sources			and
				_posOffset			== rhs._posOffset		and
				_dataOffset			== rhs._dataOffset		and
				_initialPosition	== rhs._initialPosition;
	}

} // AE::PipelineCompiler
