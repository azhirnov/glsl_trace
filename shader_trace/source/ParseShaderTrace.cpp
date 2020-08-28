// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#include "Common.h"

#include <array>
#include <assert.h>

#include "glslang/Include/BaseTypes.h"
using namespace glslang;

using VariableID = ShaderTrace::VariableID;

namespace std
{
	inline string  to_string (bool value)
	{
		return value ? "true" : "false";
	}
}	// std


namespace {
/*
=================================================
	Trace
=================================================
*/
struct Trace
{
public:
	using ExprInfo			= ShaderTrace::ExprInfo;
	using VarNames_t		= ShaderTrace::VarNames_t;
	using Sources_t			= ShaderTrace::Sources_t;
	using SourceLocation	= ShaderTrace::SourceLocation;

	union Value
	{
		std::array< int32_t, 4 >	i;
		std::array< uint, 4 >		u;
		std::array< bool, 4 >		b;
		std::array< float, 4 >		f;
		std::array< double, 4 >		d;
		std::array< int64_t, 4 >	i64;
		std::array< uint64_t, 4 >	u64;
	};

	struct VariableState
	{
		Value			value		{};
		TBasicType		type		= TBasicType::EbtVoid;
		uint			count		= 0;
		bool			modified	= false;
	};

	struct FnExecutionDuration
	{
		uint64_t		subgroup	= 0;
		uint64_t		device		= 0;
		uint			count		= 0;
	};

	using VarStates_t	= unordered_map< uint64_t, VariableState >;
	using Pending_t		= vector< uint64_t >;
	using Profiling_t	= unordered_map< ExprInfo const*, FnExecutionDuration >;


public:
	uint				lastPosition	= ~0u;
private:
	VarStates_t			_states;
	Pending_t			_pending;
	Profiling_t			_profiling;
	SourceLocation		_lastLoc;


public:
	bool AddState (const ExprInfo &expr, TBasicType type, uint rows, uint cols, const uint *data,
					const VarNames_t &varNames, const Sources_t &src, INOUT string &result);

	bool AddTime (const ExprInfo &expr, uint rows, uint cols, const uint *data);

	bool Flush (const VarNames_t &varNames, const Sources_t &src, INOUT string &result);

private:
	bool _FlushStates (const VarNames_t &varNames, const Sources_t &src, INOUT string &result);
	bool _FlushProfiling (const Sources_t &src, INOUT string &result);

	static uint64_t		HashOf (VariableID id, uint col)	{ return (uint64_t(id) << 32) | col; }
	static VariableID	VarFromHash (uint64_t h)				{ return VariableID(h >> 32); }
};

/*
=================================================
	CopyValue
=================================================
*/
inline void  CopyValue (TBasicType type, INOUT Trace::Value &value, uint valueIndex, const uint *data, INOUT uint &dataIndex)
{
	switch ( type )
	{
		case TBasicType::EbtInt :
			std::memcpy( OUT &value.i[valueIndex], &data[dataIndex++], sizeof(int) );
			break;
		
		case TBasicType::EbtBool :
			value.b[valueIndex] = (data[dataIndex++] != 0);
			break;
		
		case TBasicType::EbtUint :
			value.u[valueIndex] = data[dataIndex++];
			break;
		
		case TBasicType::EbtFloat :
			std::memcpy( OUT &value.f[valueIndex], &data[dataIndex++], sizeof(float) );
			break;
		
		case TBasicType::EbtDouble :
			std::memcpy( OUT &value.d[valueIndex], &data[dataIndex], sizeof(double) );
			dataIndex += 2;
			break;
		
		case TBasicType::EbtInt64 :
			std::memcpy( OUT &value.d[valueIndex], &data[dataIndex], sizeof(int64_t) );
			dataIndex += 2;
			break;
		
		case TBasicType::EbtUint64 :
			std::memcpy( OUT &value.u64[valueIndex], &data[dataIndex], sizeof(uint64_t) );
			dataIndex += 2;
			break;
		
		default :
			CHECK( !"not supported" );
			break;
	}
}

/*
=================================================
	Trace::AddState
=================================================
*/
bool  Trace::AddState (const ExprInfo &expr, TBasicType type, uint rows, uint cols, const uint *data,
					   const VarNames_t &varNames, const Sources_t &sources, INOUT string &result)
{
	if ( not (_lastLoc == expr.range) )
		CHECK_ERR( _FlushStates( varNames, sources, INOUT result ));

	const auto	AppendID = [this] (uint64_t newID) {
		for (auto& id : _pending) {
			if ( id == newID )
				return;
		}
		_pending.push_back( newID );
	};

	for (uint col = 0; col < cols; ++col)
	{
		uint64_t	id   = HashOf( expr.varID, col );
		auto		iter = _states.find( id );

		if ( iter == _states.end() or expr.varID == VariableID::Unknown )
			iter = _states.insert_or_assign( id, VariableState{ {}, type, rows, false } ).first;


		VariableState&	var = iter->second;

		CHECK( expr.varID == VariableID::Unknown or var.type == type );
		var.type	 = type;
		var.modified = true;

		if ( var.type == TBasicType::EbtVoid )
		{
			var.value = {};
			var.count = 0;
		}
		else
		if ( expr.swizzle )
		{
			for (uint i = 0; i < rows; ++i)
			{
				uint	sw = (expr.swizzle >> (i*3)) & 7;
				ASSERT( sw > 0 and sw <= 4 );
				var.count = max( 1u, max( var.count, sw ));
			}
			
			for (uint r = 0, j = 0; r < rows; ++r)
			{
				uint	sw = (expr.swizzle >> (r*3)) & 7;
				CopyValue( type, INOUT var.value, sw-1, data, INOUT j );
			}
		}
		else
		{
			for (uint r = 0, j = 0; r < rows; ++r) {
				CopyValue( type, INOUT var.value, r, data, INOUT j );
			}
		}
		
		var.count = max( var.count, rows );
		var.count = min( var.count, 4u );

		AppendID( id );

		for (auto& var_id : expr.vars) {
			AppendID( HashOf( var_id, 0 ));
			AppendID( HashOf( var_id, 1 ));
			AppendID( HashOf( var_id, 2 ));
			AppendID( HashOf( var_id, 3 ));
		}
	}

	_lastLoc = expr.range;
	return true;
}

/*
=================================================
	AddTime
=================================================
*/
bool  Trace::AddTime (const ExprInfo &expr, uint, uint, const uint *data)
{
	auto&	fn = _profiling[ &expr ];

	uint64_t	subgroup_begin;		std::memcpy( &subgroup_begin, data + 0, sizeof(subgroup_begin) );
	uint64_t	subgroup_end;		std::memcpy( &subgroup_end,   data + 4, sizeof(subgroup_end) );
	uint64_t	device_begin;		std::memcpy( &device_begin,   data + 2, sizeof(device_begin) );
	uint64_t	device_end;			std::memcpy( &device_end,     data + 6, sizeof(device_end) );
	
	if ( subgroup_end < subgroup_begin )
	{
		ASSERT( !"incorrect subgroup time" );
		subgroup_end = subgroup_begin;
	}
	
	if ( device_end < device_begin )
	{
		ASSERT( !"incorrect device time" );
		device_end = device_begin;
	}

	fn.count++;
	fn.subgroup += (subgroup_end - subgroup_begin);
	fn.device   += (device_end - device_begin);

	return true;
}

/*
=================================================
	TypeToString
=================================================
*/
inline string  ToString (bool value)
{
	return value ? "true" : "false";
}

inline string  ToString (float value)
{
	float	f		 = std::abs(value);
	bool	exp		 = f != 0.0f and (f < 1.0e-4f or f > 1.0e+5f);
	char	buf[128] = {};
	std::snprintf( buf, sizeof(buf), (exp ? "%1.6e" : "%0.6f"), value );
	return buf;
}

inline string  ToString (double value)
{
	double	f		 = std::abs(value);
	bool	exp		 = f != 0.0 and (f < 1.0e-4 or f > 1.0e+5);
	char	buf[128] = {};
	std::snprintf( buf, sizeof(buf), (exp ? "%1.8e" : "%0.8f"), value );
	return buf;
}

template <typename T>
inline string  ToString (T value)
{
	return to_string( value );
}

/*
=================================================
	TypeToString
=================================================
*/
template <typename T>
inline string  TypeToString (uint rows, const std::array<T,4> &values)
{
	string		str;

	if ( rows > 1 )
		str += ToString( rows );

	str += " {";

	for (uint r = 0; r < rows; ++r)
	{
		str += (r ? ", " : "") + ToString( values[r] );
	}
	
	str += "}\n";
	return str;
}

/*
=================================================
	AppendSourceRange
=================================================
*/
bool  AppendSourceRange (const Trace::SourceLocation &loc, const Trace::Sources_t &sources, INOUT string &result)
{
	CHECK_ERR( loc.sourceId < sources.size() );

	const auto&		src			= sources[ loc.sourceId ];
	const uint		start_line	= max( 1u, loc.begin.Line() ) - 1;	// because first line is 1
	const uint		end_line	= max( 1u, loc.end.Line() ) - 1;
	const uint		start_col	= max( 1u, loc.begin.Column() ) - 1;
	const uint		end_col		= max( 1u, loc.end.Column() ) - 1;

	CHECK_ERR( start_line < src.lines.size() );
	CHECK_ERR( end_line < src.lines.size() );

	if ( loc.sourceId == 0 and end_line == 0 and end_col == 0 )
		result += "no source\n";
	else
	for (uint i = start_line; i <= end_line; ++i)
	{
		result += to_string(i+1) + ". ";

		size_t	start	= src.lines[i].first;
		size_t	end		= src.lines[i].second;
		
		/*if ( i == end_line ) {
			CHECK_ERR( start + end_col < end );
			end = start + end_col;
		}*/
		if ( i == start_line ) {
			//CHECK_ERR( start + start_col < end );
			start += start_col;
		}

		if ( start < end )
		{
			result += src.code.substr( start, end - start );
			result += '\n';
		}
		else
			result += "invalid source location\n";
	}
	return true;
}

/*
=================================================
	Trace::_FlushStates
=================================================
*/
bool  Trace::_FlushStates (const VarNames_t &varNames, const Sources_t &sources, INOUT string &result)
{
	const auto	Convert = [this, &varNames, INOUT &result] (uint64_t varHash) -> bool
	{
		VariableID	id	 = VarFromHash( varHash );
		auto		iter = _states.find( varHash );

		if ( iter == _states.end() )
			return false;

		auto	name = varNames.find( id );
		if ( name != varNames.end() )
			result += string("//") + (iter->second.modified ? "> " : "  ") + name->second + ": ";
		else
			result += string("//") + (iter->second.modified ? "> (out): " : "  (temp): ");

		iter->second.modified = false;

		switch ( iter->second.type )
		{
			case TBasicType::EbtVoid : {
				result += "void\n";
				break;
			}
			case TBasicType::EbtFloat : {
				result += "float";
				result += TypeToString( iter->second.count, iter->second.value.f );
				break;
			}
			case TBasicType::EbtDouble : {
				result += "double";
				result += TypeToString( iter->second.count, iter->second.value.d );
				break;
			}
			case TBasicType::EbtInt : {
				result += "int";
				result += TypeToString( iter->second.count, iter->second.value.i );
				break;
			}
			case TBasicType::EbtBool : {
				result += "bool";
				result += TypeToString( iter->second.count, iter->second.value.b );
				break;
			}
			case TBasicType::EbtUint : {
				result += "uint";
				result += TypeToString( iter->second.count, iter->second.value.u );
				break;
			}
			case TBasicType::EbtInt64 : {
				result += "long";
				result += TypeToString( iter->second.count, iter->second.value.i64 );
				break;
			}
			case TBasicType::EbtUint64 : {
				result += "ulong";
				result += TypeToString( iter->second.count, iter->second.value.u64 );
				break;
			}
			default :
				RETURN_ERR( "not supported" );
		}
		return true;
	};

	if ( _pending.empty() )
		return true;

	for (auto& h : _pending) {
		Convert( h );
	}
	_pending.clear();

	CHECK_ERR( AppendSourceRange( _lastLoc, sources, INOUT result ));

	result += '\n';

	return true;
}

/*
=================================================
	Trace::_FlushProfiling
=================================================
*/
bool  Trace::_FlushProfiling (const Sources_t &sources, INOUT string &result)
{
	if ( _profiling.empty() )
		return true;

	// sort by device time
	vector< std::pair<ExprInfo const* const, FnExecutionDuration>* >	sorted;
	sorted.reserve( _profiling.size() );

	for (auto& item : _profiling) {
		sorted.push_back( &item );
	}

	std::sort( sorted.begin(), sorted.end(), [](auto* lhs, auto *rhs) { return lhs->second.device > rhs->second.device; });

	// print
	const double	max_subgroup_time	= sorted.front()->second.subgroup ? 100.0 / double(sorted.front()->second.subgroup) : 1.0;
	const double	max_device_time		= sorted.front()->second.device ? 100.0 / double(sorted.front()->second.device) : 1.0;

	const auto		DtoStr = [] (double value) -> string
	{
		char		buf[32] = {};
		const int	len = std::snprintf( buf, sizeof(buf), "%0.2f", value );

		if ( len <= 0 )
			buf[0] = '\0';

		return buf;
	};

	for (auto* item : sorted)
	{
		auto&	expr	= *item->first;
		auto&	time	= item->second;

		result += "// subgroup total: " + DtoStr( double(time.subgroup) * max_subgroup_time ) + "%,  avr: " + 
					DtoStr( (double(time.subgroup) * max_subgroup_time) / double(time.count) ) + "%,  (" +
					DtoStr( double(time.subgroup) / double(time.count) ) + ")\n";

		result += "// device   total: " + DtoStr( double(time.device) * max_device_time ) + "%,  avr: " +
					DtoStr( (double(time.device) * max_device_time) / double(time.count) ) + "%,  (" +
					DtoStr( double(time.device) / double(time.count) ) + ")\n";

		result += "// invocations:    " + to_string( time.count ) + "\n";

		CHECK_ERR( AppendSourceRange( expr.range, sources, INOUT result ));
		result += "\n";
	}

	return true;
}

/*
=================================================
	Trace::Flush
=================================================
*/
bool  Trace::Flush (const VarNames_t &varNames, const Sources_t &sources, INOUT string &result)
{
	CHECK_ERR( _FlushStates( varNames, sources, INOUT result ));
	CHECK_ERR( _FlushProfiling( sources, INOUT result ));

	return true;
}

/*
=================================================
	GetTypeSizeOf
=================================================
*/
inline uint  GetTypeSizeOf (TBasicType type)
{
	switch ( type )
	{
		case TBasicType::EbtVoid :
		case TBasicType::EbtFloat :
		case TBasicType::EbtInt :
		case TBasicType::EbtBool :
		case TBasicType::EbtUint :
			return 1;

		case TBasicType::EbtDouble :
		case TBasicType::EbtInt64 :
		case TBasicType::EbtUint64 :
			return 2;

		case ShaderTrace::TBasicType_Clock :
			return 2;
	}
	RETURN_ERR( "not supported" );
}
}	// namespace
//-----------------------------------------------------------------------------



/*
=================================================
	ParseShaderTrace
=================================================
*/
bool  ShaderTrace::ParseShaderTrace (const void *ptr, uint64_t maxSize, OUT vector<string> &result) const
{
	result.clear();

	const uint64_t	count		= *(static_cast<uint const*>(ptr) + _posOffset / sizeof(uint));
	uint const*		start_ptr	= static_cast<uint const*>(ptr) + _dataOffset / sizeof(uint);
	uint const*		end_ptr		= start_ptr + min( count, (maxSize - _dataOffset) / sizeof(uint) );
	vector<Trace>	shaders;

	ASSERT( (count * sizeof(uint)) <= maxSize );
	
	for (auto data_ptr = start_ptr; data_ptr < end_ptr;)
	{
		uint		pos			= uint(std::distance( start_ptr, data_ptr ));
		uint		prev_pos	= *(data_ptr++);
		uint		expr_id		= *(data_ptr++);
		uint		type		= *(data_ptr++);
		TBasicType	t_basic		= TBasicType(type & 0xFF);
		uint		row_size	= (type >> 8) & 0xF;					// for scalar, vector and matrix
		uint		col_size	= max(1u, (type >> 12) & 0xF );			// only for matrix
		uint const*	data		= data_ptr;
		Trace*		trace		= nullptr;

		CHECK_ERR( (t_basic == TBasicType::EbtVoid and row_size == 0) or (row_size > 0 and row_size <= 4) );
		CHECK_ERR( col_size > 0 and col_size <= 4 );

		data_ptr += (row_size * col_size) * GetTypeSizeOf( t_basic );
		ASSERT( data_ptr <= end_ptr );

		for (auto& sh : shaders) {
			if ( sh.lastPosition == prev_pos ) {
				trace = &sh;
				break;
			}
		}
		
		if ( not trace )
		{
			if ( prev_pos == _initialPosition )
			{
				shaders.push_back( Trace{} );
				result.resize( shaders.size() );
				trace = &shaders.back();
			}
			else
			{
				// this entry from another shader, skip it
				continue;
			}
		}
		
		CHECK_ERR( expr_id < _exprLocations.size() );

		auto&	expr = _exprLocations[ expr_id ];
		auto&	str  = result[ std::distance( shaders.data(), trace )];

		if ( t_basic == ShaderTrace::TBasicType_Clock )
			CHECK_ERR( trace->AddTime( expr, row_size, col_size, data ))
		else
			CHECK_ERR( trace->AddState( expr, t_basic, row_size, col_size, data, _varNames, _sources, INOUT str ));

		trace->lastPosition = pos;
	}

	for (size_t i = 0; i < shaders.size(); ++i)
	{
		CHECK_ERR( shaders[i].Flush( _varNames, _sources, INOUT result[i] ));
	}
	return true;
}
