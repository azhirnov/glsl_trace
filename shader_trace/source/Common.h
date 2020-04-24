// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "../include/ShaderTrace.h"
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

using std::vector;
using std::string;
using std::unordered_map;
using std::unordered_set;
using std::min;
using std::max;
using std::to_string;

using uint = uint32_t;

#define HIGH_DETAIL_TRACE
#define USE_NV_RAY_TRACING

//#undef NDEBUG
#include <assert.h>

#ifndef OUT
#	define OUT
#endif

#ifndef INOUT
#	define INOUT
#endif

#if defined(_MSC_VER) && !defined(and)
#	define not					!
#	define and					&&
#	define or					||
#endif

#ifndef CHECK_ERR
#	define __GETARG_0( _0_, ... )		_0_
#	define __GETARG_1( _0_, _1_, ... )	_1_
#	define __CHECK_ERR( _expr_, _ret_ )	{if (not (_expr_)) { assert(!(#_expr_)); return _ret_; } }
#	define CHECK_ERR( ... )				__CHECK_ERR( __GETARG_0( __VA_ARGS__ ), __GETARG_1( __VA_ARGS__, 0 ))
#	define RETURN_ERR( _msg_ )			{ assert(!(#_msg_)); return 0; }
#	define CHECK( _expr_ )				{ assert(_expr_); }
#	define ASSERT( _expr_ )				{ assert(_expr_); }
#endif

#ifndef ND_
#	if (defined(_MSC_VER) && (_MSC_VER >= 1917)) //|| (defined(__clang__) && __has_feature( cxx_attributes )) || (defined(__gcc__) && __has_cpp_attribute( nodiscard ))
#		define ND_		[[nodiscard]]
#	else
#		define ND_
#	endif
#endif

#ifdef _MSC_VER
#	define FUNCTION_NAME			__FUNCTION__
#elif defined(__clang__) or defined(__gcc__)
#	define FUNCTION_NAME			__func__
#else
#	define FUNCTION_NAME			"unknown function"
#endif

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
#include "glslang/Include/revision.h"
#include "glslang/MachineIndependent/localintermediate.h"
#include "glslang/Include/intermediate.h"

ND_ std::string  GetFunctionName (glslang::TIntermOperator *op);

#ifdef USE_NV_RAY_TRACING
	static const char	RT_LaunchID[]			= "gl_LaunchIDNV";
	static const char	RT_LaunchSize[]			= "gl_LaunchSizeNV";
	static const char	RT_InstanceCustomIndex[]= "gl_InstanceCustomIndexNV";
	static const char	RT_WorldRayOrigin[]		= "gl_WorldRayOriginNV";
	static const char	RT_WorldRayDirection[]	= "gl_WorldRayDirectionNV";
	static const char	RT_ObjectRayOrigin[]	= "gl_ObjectRayOriginNV";
	static const char	RT_ObjectRayDirection[]	= "gl_ObjectRayDirectionNV";
	static const char	RT_RayTmin[]			= "gl_RayTminNV";
	static const char	RT_RayTmax[]			= "gl_RayTmaxNV";
	static const char	RT_IncomingRayFlags[]	= "gl_IncomingRayFlagsNV";
	static const char	RT_ObjectToWorld[]		= "gl_ObjectToWorldNV";
	static const char	RT_WorldToObject[]		= "gl_WorldToObjectNV";
	static const char	RT_HitT[]				= "gl_HitTNV";
	static const char	RT_HitKind[]			= "gl_HitKindNV";
	static const char	RT_InstanceID[]			= "gl_InstanceIDNV";
	static const auto	RT_EbvLaunchId			= glslang::TBuiltInVariable::EbvLaunchIdNV;
#else
	static const char	RT_LaunchID[]			= "gl_LaunchID";
	static const char	RT_LaunchSize[]			= "gl_LaunchSize";
	static const char	RT_InstanceCustomIndex[]= "gl_InstanceCustomIndex";
	static const char	RT_WorldRayOrigin[]		= "gl_WorldRayOrigin";
	static const char	RT_WorldRayDirection[]	= "gl_WorldRayDirection";
	static const char	RT_ObjectRayOrigin[]	= "gl_ObjectRayOrigin";
	static const char	RT_ObjectRayDirection[]	= "gl_ObjectRayDirection";
	static const char	RT_RayTmin[]			= "gl_RayTmin";
	static const char	RT_RayTmax[]			= "gl_RayTmax";
	static const char	RT_IncomingRayFlags[]	= "gl_IncomingRayFlags";
	static const char	RT_ObjectToWorld[]		= "gl_ObjectToWorld";
	static const char	RT_WorldToObject[]		= "gl_WorldToObject";
	static const char	RT_HitT[]				= "gl_HitT";
	static const char	RT_HitKind[]			= "gl_HitKind";
	static const char	RT_InstanceID[]			= "gl_InstanceID";
	static const auto	RT_EbvLaunchId			= glslang::TBuiltInVariable::EbvLaunchId;
#endif
