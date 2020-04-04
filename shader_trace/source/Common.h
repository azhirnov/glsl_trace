// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "../include/ShaderTrace.h"

#define HIGH_DETAIL_TRACE

#if defined(ENABLE_GLSLANG)
#	include "stl/Common.h"
#else
//#	undef NDEBUG
#	include <assert.h>

#	define OUT
#	define INOUT

# ifdef _MSC_VER
#	define not					!
#	define and					&&
#	define or					||
# endif

#	define __GETARG_0( _0_, ... )		_0_
#	define __GETARG_1( _0_, _1_, ... )	_1_
#	define __CHECK_ERR( _expr_, _ret_ )	{if (not (_expr_)) { assert(!(#_expr_)); return _ret_; } }
#	define CHECK_ERR( ... )				__CHECK_ERR( __GETARG_0( __VA_ARGS__ ), __GETARG_1( __VA_ARGS__, 0 ))
#	define RETURN_ERR( _msg_ )			{ assert(!(#_msg_)); return 0; }
#	define CHECK( _expr_ )				{ assert(_expr_); }
#	define ASSERT( _expr_ )				{ assert(_expr_); }

# ifndef ND_
#	if (defined(_MSC_VER) && (_MSC_VER >= 1917)) //|| (defined(__clang__) && __has_feature( cxx_attributes )) || (defined(__gcc__) && __has_cpp_attribute( nodiscard ))
#		define ND_		[[nodiscard]]
#	else
#		define ND_
#	endif
# endif

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

#endif


// glslang includes
#include "glslang/Include/revision.h"
#include "glslang/MachineIndependent/localintermediate.h"
#include "glslang/Include/intermediate.h"

ND_ std::string  GetFunctionName (glslang::TIntermOperator *op);