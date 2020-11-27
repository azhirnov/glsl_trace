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
#define USE_STORAGE_QUALIFIERS

#include <assert.h>

#ifndef OUT
#	define OUT
#endif

#ifndef INOUT
#	define INOUT
#endif

#ifdef _MSC_VER
# if !defined(and)
#	define not					!
#	define and					&&
#	define or					||
# endif
#endif

#ifndef CHECK_ERR
# ifdef _DEBUG
#	define ASSERT( _expr_ )				{ assert(_expr_); }
# else
#	define ASSERT( _expr_ )				{}
# endif
#	define __GETARG_0( _0_, ... )		_0_
#	define __GETARG_1( _0_, _1_, ... )	_1_
#	define __CHECK_ERR( _expr_, _ret_ )	{if (not (_expr_)) { ASSERT(!(#_expr_)); return _ret_; } }
#	define CHECK_ERR( ... )				__CHECK_ERR( __GETARG_0( __VA_ARGS__ ), __GETARG_1( __VA_ARGS__, 0 ))
#	define RETURN_ERR( _msg_ )			{ ASSERT(!(#_msg_)); return 0; }
#	define CHECK( _expr_ )				{ ASSERT(_expr_); }
#endif

#ifndef ND_
# if defined(_MSC_VER)
#	if _MSC_VER >= 1917
#		define ND_		[[nodiscard]]
#	endif
# elif defined(__clang__)
#	if __has_feature( cxx_attributes )
#		define ND_		[[nodiscard]]
#	endif
# elif defined(__gcc__)
#	if __has_cpp_attribute( nodiscard )
#		define ND_		[[nodiscard]]
#	endif
# endif
# ifndef ND_
#	define ND_
# endif
#endif

#ifdef _MSC_VER
#	define FUNCTION_NAME			__FUNCTION__
#elif defined(__clang__) || defined(__gcc__)
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
#include "glslang/MachineIndependent/localintermediate.h"
#include "glslang/Include/intermediate.h"

ND_ std::string  GetFunctionName (glslang::TIntermOperator *op);

bool ValidateInterm (glslang::TIntermediate &intermediate);

static const char*	RT_LaunchID[]			= { "gl_LaunchID",				"gl_LaunchIDNV" };
static const char*	RT_LaunchSize[]			= { "gl_LaunchSize",			"gl_LaunchSizeNV" };
static const char*	RT_InstanceCustomIndex[]= { "gl_InstanceCustomIndex",	"gl_InstanceCustomIndexNV" };
static const char*	RT_WorldRayOrigin[]		= {	"gl_WorldRayOrigin",		"gl_WorldRayOriginNV" };
static const char*	RT_WorldRayDirection[]	= {	"gl_WorldRayDirection",		"gl_WorldRayDirectionNV" };
static const char*	RT_ObjectRayOrigin[]	= {	"gl_ObjectRayOrigin",		"gl_ObjectRayOriginNV" };
static const char*	RT_ObjectRayDirection[]	= {	"gl_ObjectRayDirection",	"gl_ObjectRayDirectionNV" };
static const char*	RT_RayTmin[]			= {	"gl_RayTmin",				"gl_RayTminNV" };
static const char*	RT_RayTmax[]			= {	"gl_RayTmax",				"gl_RayTmaxNV" };
static const char*	RT_IncomingRayFlags[]	= {	"gl_IncomingRayFlags",		"gl_IncomingRayFlagsNV" };
static const char*	RT_ObjectToWorld[]		= {	"gl_ObjectToWorld",			"gl_ObjectToWorldNV" };
static const char*	RT_WorldToObject[]		= {	"gl_WorldToObject",			"gl_WorldToObjectNV" };
static const char*	RT_HitT[]				= {	"gl_HitT",					"gl_HitTNV" };
static const char*	RT_HitKind[]			= {	"gl_HitKind",				"gl_HitKindNV" };
static const char*	RT_InstanceID[]			= {	"gl_InstanceID",			"gl_InstanceIDNV" };
