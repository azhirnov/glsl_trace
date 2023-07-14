// Copyright (c) Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include <cassert>
#include "../include/ShaderTrace.h"

#define HIGH_DETAIL_TRACE
//#define USE_STORAGE_QUALIFIERS


#ifdef _DEBUG
# define ASSERT( _expr_ )				{ assert(_expr_); }
#else
# define ASSERT( _expr_ )				{}
#endif
#define __GETARG_0( _0_, ... )			_0_
#define __GETARG_1( _0_, _1_, ... )		_1_
#define __CHECK_ERR( _expr_, _ret_ )	{if (not (_expr_)) { ASSERT(!(#_expr_)); return _ret_; } }
#define CHECK_ERR( ... )				__CHECK_ERR( __GETARG_0( __VA_ARGS__ ), __GETARG_1( __VA_ARGS__, 0 ))
#define CHECK_ERRV( ... )				__CHECK_ERR( (__VA_ARGS__), void() )
#define RETURN_ERR( _msg_ )				{ ASSERT(!(#_msg_)); return 0; }
#define CHECK( _expr_ )					{ ASSERT(_expr_); }
#define DBG_WARNING( _msg_ )			{ ASSERT( false ); }


#ifndef BEGIN_ENUM_CHECKS
# if defined(_MSC_VER)
#	define BEGIN_ENUM_CHECKS() \
		__pragma (warning (push)) \
		__pragma (warning (error: 4061)) /*enumerator 'identifier' in switch of enum 'enumeration' is not explicitly handled by a case label*/ \
		__pragma (warning (error: 4062)) /*enumerator 'identifier' in switch of enum 'enumeration' is not handled*/ \
		__pragma (warning (error: 4063)) /*case 'number' is not a valid value for switch of enum 'type'*/ \

#	define END_ENUM_CHECKS() \
		__pragma (warning (pop)) \

# elif defined(__clang__)
#	define BEGIN_ENUM_CHECKS() \
		 _Pragma( "clang diagnostic error \"-Wswitch\"" )

#	define END_ENUM_CHECKS() \
		 _Pragma( "clang diagnostic ignored \"-Wswitch\"" )

# else
#	define BEGIN_ENUM_CHECKS()
#	define END_ENUM_CHECKS()

# endif
#endif	// BEGIN_ENUM_CHECKS


// glslang includes
#ifdef AE_ENABLE_GLSLANG
# ifdef _MSC_VER
#	pragma warning (push, 0)
#	pragma warning (disable: 4005)
#	pragma warning (disable: 4668)
# endif
#ifdef __clang__
#	pragma clang diagnostic push
#	pragma clang diagnostic ignored "-Wdouble-promotion"
#endif
#ifdef __gcc__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wundef"
#	pragma GCC diagnostic ignored "-Wdouble-promotion"
#endif

# include "glslang/MachineIndependent/localintermediate.h"
# include "glslang/Include/intermediate.h"

# ifdef _MSC_VER
#	pragma warning (pop)
# endif
# ifdef __clang__
#	pragma clang diagnostic pop
# endif
# ifdef __gcc__
#	pragma GCC diagnostic pop
# endif
#endif // AE_ENABLE_GLSLANG


namespace AE::PipelineCompiler
{
	using namespace std::string_literals;

	using VariableID = ShaderTrace::VariableID;
	
/*
=================================================
	operator << (String &, String)
	operator << (String &, StringView)
	operator << (String &, CStyleString)
	operator << (String &, char)
=================================================
*/
	inline String&&  operator << (String &&lhs, const String &rhs)
	{
		return RVRef( RVRef(lhs).append( rhs.data(), rhs.size() ));
	}

	inline String&  operator << (String &lhs, const String &rhs)
	{
		return lhs.append( rhs.data(), rhs.size() );
	}

	inline String&&  operator << (String &&lhs, const StringView &rhs)
	{
		return RVRef( RVRef(lhs).append( rhs.data(), rhs.size() ));
	}

	inline String&  operator << (String &lhs, const StringView &rhs)
	{
		return lhs.append( rhs.data(), rhs.size() );
	}

	inline String&&  operator << (String &&lhs, char const * const rhs)
	{
		return RVRef( RVRef(lhs).append( rhs ));
	}

	inline String&  operator << (String &lhs, char const * const rhs)
	{
		return lhs.append( rhs );
	}

	inline String&&  operator << (String &&lhs, const char rhs)
	{
		return RVRef( RVRef(lhs) += rhs );
	}

	inline String&  operator << (String &lhs, const char rhs)
	{
		return (lhs += rhs);
	}
	

/*
=================================================
	ToString
=================================================
*/
	ND_ inline String  ToString (String value)
	{
		return value;
	}

	ND_ inline String  ToString (const char value[])
	{
		return String{value};
	}

	template <typename T>
	ND_ std::enable_if_t<not std::is_enum_v<T>, String>  ToString (const T &value)
	{
		return std::to_string( value );
	}
	
	ND_ inline String  ToString (const Path &path)
	{
		return path.string();
	}
}
//-----------------------------------------------------------------------------


	
namespace AE::PipelineCompiler
{
# ifdef AE_ENABLE_GLSLANG

	ND_ String  GetFunctionName (glslang::TIntermOperator *op);

	ND_ bool  ValidateInterm (glslang::TIntermediate &intermediate);


/*
=================================================
	TSourceLoc::operator ==
=================================================
*/
	ND_ inline bool  operator == (const glslang::TSourceLoc &lhs, const glslang::TSourceLoc &rhs)
	{
		if ( lhs.name != rhs.name )
		{
			if ( lhs.name == null  or
				 rhs.name == null  or
				*lhs.name != *rhs.name )
				return false;
		}

		return	lhs.string	== rhs.string	and
				lhs.line	== rhs.line		and
				lhs.column	== rhs.column;
	}

	ND_ inline bool  operator != (const glslang::TSourceLoc &lhs, const glslang::TSourceLoc &rhs)
	{
		return not (lhs == rhs);
	}

	ND_ inline bool  operator < (const glslang::TSourceLoc &lhs, const glslang::TSourceLoc &rhs)
	{
		if ( lhs.name != rhs.name )
		{
			if ( lhs.name == null  or
				 rhs.name == null )
				return false;

			if ( *lhs.name != *rhs.name )
				return *lhs.name < *rhs.name;
		}

		return	lhs.string	!= rhs.string	? lhs.string < rhs.string	:
				lhs.line	!= rhs.line		? lhs.line	 < rhs.line		:
											  lhs.column < rhs.column;
	}

/*
=================================================
	SourcePoint
=================================================
*/
	inline ShaderTrace::SourcePoint::SourcePoint (const glslang::TSourceLoc &loc) :
		SourcePoint{ uint(loc.line), uint(loc.column) }
	{}

# endif // AE_ENABLE_GLSLANG


} // AE::PipelineCompiler
