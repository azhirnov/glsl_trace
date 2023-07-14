// Copyright (c) Zhirnov Andrey. For more information see 'LICENSE'

// TODO: VS style output: 'file (line): message'

#include "Common.h"

#ifdef AE_ENABLE_GLSLANG
# include "glslang/Include/BaseTypes.h"
#endif

namespace AE::PipelineCompiler
{
namespace
{
	using namespace glslang;

#ifndef AE_ENABLE_GLSLANG
	enum TBasicType : ubyte {
		EbtVoid,
		EbtFloat,
		EbtDouble,
		EbtFloat16,
		EbtInt8,
		EbtUint8,
		EbtInt16,
		EbtUint16,
		EbtInt,
		EbtUint,
		EbtInt64,
		EbtUint64,
		EbtBool,
		EbtAtomicUint,
		EbtSampler,
		EbtStruct,
		EbtBlock,
		EbtAccStruct,
		EbtReference,
		EbtRayQuery,
		EbtHitObjectNV,
		EbtSpirvType,
		EbtString,
		EbtNumTypes
	};
#endif
	STATIC_ASSERT( uint(TBasicType::EbtNumTypes) == 23 );


	//
	// Trace
	//

	struct Trace
	{
	public:
		using ExprInfo			= ShaderTrace::ExprInfo;
		using VarNames_t		= ShaderTrace::VarNames_t;
		using Sources_t			= ShaderTrace::Sources_t;
		using SourceLocation	= ShaderTrace::SourceLocation;
		using ELogFormat		= ShaderTrace::ELogFormat;

		union Value
		{
			StaticArray< int, 4 >		i;
			StaticArray< uint, 4 >		u;
			StaticArray< bool, 4 >		b;
			StaticArray< float, 4 >		f;
			StaticArray< double, 4 >	d;
			StaticArray< slong, 4 >		i64;
			StaticArray< ulong, 4 >		u64;
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
			ulong			subgroup	= 0;
			ulong			device		= 0;
			uint			count		= 0;
		};

		using VarStates_t	= HashMap< ulong, VariableState >;
		using Pending_t		= Array< ulong >;
		using Profiling_t	= HashMap< ExprInfo const*, FnExecutionDuration >;


	public:
		uint				lastPosition	= ~0u;
	private:
		VarStates_t			_states;
		Pending_t			_pending;
		Profiling_t			_profiling;
		SourceLocation		_lastLoc;


	public:
		ND_ bool  AddState (const ExprInfo &expr, TBasicType type, uint rows, uint cols, const uint *data,
							const VarNames_t &varNames, const Sources_t &src, ELogFormat format, INOUT String &result);

		ND_ bool  AddTime (const ExprInfo &expr, uint rows, uint cols, const uint *data);

		ND_ bool  Flush (const VarNames_t &varNames, const Sources_t &src, ELogFormat format, INOUT String &result);


	private:
		ND_ bool  _FlushStates (const VarNames_t &varNames, const Sources_t &src, ELogFormat format, INOUT String &result);
		ND_ bool  _FlushProfiling (const Sources_t &src, ELogFormat format, INOUT String &result);

		ND_ static ulong		HashOf (VariableID id, uint col)	{ return (ulong(id) << 32) | col; }
		ND_ static VariableID	VarFromHash (ulong h)				{ return VariableID(h >> 32); }
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
				std::memcpy( OUT &value.u64[valueIndex], &data[dataIndex], sizeof(ulong) );
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
						   const VarNames_t &varNames, const Sources_t &sources, ELogFormat format,
						   INOUT String &result)
	{
		if ( not (_lastLoc == expr.range) )
			CHECK_ERR( _FlushStates( varNames, sources, format, INOUT result ));

		const auto	AppendID = [this] (ulong newID)
		{{
			for (auto& id : _pending) {
				if ( id == newID )
					return;
			}
			_pending.push_back( newID );
		}};

		for (uint col = 0; col < cols; ++col)
		{
			ulong	id   = HashOf( expr.varID, col );
			auto	iter = _states.find( id );

			if ( iter == _states.end() or expr.varID == VariableID::Unknown )
				iter = _states.insert_or_assign( id, VariableState{ Default, type, rows, false }).first;

			VariableState&	var = iter->second;

			CHECK( expr.varID == VariableID::Unknown or var.type == type );
			var.type	 = type;
			var.modified = true;

			if ( var.type == TBasicType::EbtVoid )
			{
				var.value = Default;
				var.count = 0;
			}
			else
			if ( expr.swizzle )
			{
				for (uint i = 0; i < rows; ++i)
				{
					uint	sw = (expr.swizzle >> (i*3)) & 7;
					ASSERT( sw > 0 and sw <= 4 );
					var.count = Max( 1u, var.count, sw );
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

			var.count = Max( var.count, rows );
			var.count = Min( var.count, 4u );

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

		ulong	subgroup_begin;		std::memcpy( &subgroup_begin, data + 0, sizeof(subgroup_begin) );
		ulong	subgroup_end;		std::memcpy( &subgroup_end,   data + 4, sizeof(subgroup_end) );
		ulong	device_begin;		std::memcpy( &device_begin,   data + 2, sizeof(device_begin) );
		ulong	device_end;			std::memcpy( &device_end,     data + 6, sizeof(device_end) );

		if ( subgroup_end < subgroup_begin )
		{
			DBG_WARNING( "incorrect subgroup time" );
			subgroup_end = subgroup_begin;
		}

		if ( device_end < device_begin )
		{
			DBG_WARNING( "incorrect device time" );
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
	ND_ inline String  TypeToString (bool value)
	{
		return value ? "true" : "false";
	}

	ND_ inline String  TypeToString (float value)
	{
		float	f		 = std::abs(value);
		bool	exp		 = f != 0.0f and (f < 1.0e-4f or f > 1.0e+5f);
		char	buf[128] = {};
		std::snprintf( buf, sizeof(buf), (exp ? "%1.6e" : "%0.6f"), double(value) );
		return buf;
	}

	ND_ inline String  TypeToString (double value)
	{
		double	f		 = std::abs(value);
		bool	exp		 = f != 0.0 and (f < 1.0e-4 or f > 1.0e+5);
		char	buf[128] = {};
		std::snprintf( buf, sizeof(buf), (exp ? "%1.8e" : "%0.8f"), value );
		return buf;
	}

	template <typename T>
	ND_ inline String  TypeToString (T value)
	{
		return ToString( value );
	}

/*
=================================================
	TypeToString
=================================================
*/
	template <typename T>
	ND_ inline String  TypeToString (uint rows, const StaticArray<T,4> &values)
	{
		String	str;

		if ( rows > 1 )
			str << TypeToString( rows );

		str << " {";

		for (uint r = 0; r < rows; ++r)
		{
			str << (r ? ", " : "") + TypeToString( values[r] );
		}

		str << "}\n";
		return str;
	}

/*
=================================================
	AppendSourceRange
=================================================
*/
	ND_ static bool  AppendSourceRange (const Trace::SourceLocation &loc, const Trace::Sources_t &sources, Trace::ELogFormat format, INOUT String &result)
	{
		CHECK_ERR( loc.sourceId < sources.size() );

		const auto&		src			= sources[ loc.sourceId ];
		const uint		start_line	= Max( 1u, loc.begin.Line() ) - 1;	// because first line is 1
		const uint		end_line	= Max( 1u, loc.end  .Line() ) - 1;
		const uint		start_col	= Max( 1u, loc.begin.Column() ) - 1;
		const uint		end_col		= Max( 1u, loc.end  .Column() ) - 1;
		const uint		file_line	= loc.begin.Line() >= src.firstLine ? loc.begin.Line() - src.firstLine : 0;

		CHECK_ERR( start_line < src.lines.size() );
		CHECK_ERR( end_line < src.lines.size() );

		if ( loc.sourceId == 0 and end_line == 0 and end_col == 0 )
		{
			result << "no source\n";
			return true;
		}

		BEGIN_ENUM_CHECKS();
		switch ( format )
		{
			// pattern: 'file (line): ...'
			case Trace::ELogFormat::VS_Console :
				result << src.filename << " (" << ToString(file_line) << "):\n";
				break;

			// pattern: 'url (line)'
			case Trace::ELogFormat::VS :
				result << "file:///" << src.filename << " (" << ToString(file_line) << ")\n";
				break;

			// pattern: 'url#line'
			case Trace::ELogFormat::VSCode :
				result << "file:///" << src.filename << "#" << ToString(file_line) << "\n";
				break;

			case Trace::ELogFormat::Text :
			case Trace::ELogFormat::Unknown :
			case Trace::ELogFormat::_Count :	break;
		}
		END_ENUM_CHECKS();

		for (uint i = start_line; i <= end_line; ++i)
		{
			result << ToString(i+1) + ". ";

			usize	start	= src.lines[i].first;
			usize	end		= src.lines[i].second;

			// TODO
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
				result << src.code.substr( start, end - start );
				result << '\n';
			}
			else
				result << "invalid source location\n";
		}

		return true;
	}

/*
=================================================
	Trace::_FlushStates
=================================================
*/
	bool  Trace::_FlushStates (const VarNames_t &varNames, const Sources_t &sources, ELogFormat format, INOUT String &result)
	{
		const auto	Convert = [this, &varNames, INOUT &result] (ulong varHash) -> bool
		{{
			VariableID	id	 = VarFromHash( varHash );
			auto		iter = _states.find( varHash );

			if ( iter == _states.end() )
				return false;

			auto	name = varNames.find( id );
			if ( name != varNames.end() )
				result << "//" << (iter->second.modified ? "> " : "  ") << name->second << ": ";
			else
				result << "//" << (iter->second.modified ? "> (out): " : "  (temp): ");

			iter->second.modified = false;

			switch ( iter->second.type )
			{
				case TBasicType::EbtVoid : {
					result << "void\n";
					break;
				}
				case TBasicType::EbtFloat : {
					result << "float";
					result << TypeToString( iter->second.count, iter->second.value.f );
					break;
				}
				case TBasicType::EbtDouble : {
					result << "double";
					result << TypeToString( iter->second.count, iter->second.value.d );
					break;
				}
				case TBasicType::EbtInt : {
					result << "int";
					result << TypeToString( iter->second.count, iter->second.value.i );
					break;
				}
				case TBasicType::EbtBool : {
					result << "bool";
					result << TypeToString( iter->second.count, iter->second.value.b );
					break;
				}
				case TBasicType::EbtUint : {
					result << "uint";
					result << TypeToString( iter->second.count, iter->second.value.u );
					break;
				}
				case TBasicType::EbtInt64 : {
					result << "long";
					result << TypeToString( iter->second.count, iter->second.value.i64 );
					break;
				}
				case TBasicType::EbtUint64 : {
					result << "ulong";
					result << TypeToString( iter->second.count, iter->second.value.u64 );
					break;
				}
				default :
					RETURN_ERR( "not supported" );
			}
			return true;
		}};

		if ( _pending.empty() )
			return true;

		for (auto& h : _pending) {
			Convert( h );
		}
		_pending.clear();

		CHECK_ERR( AppendSourceRange( _lastLoc, sources, format, INOUT result ));

		result << '\n';

		return true;
	}

/*
=================================================
	Trace::_FlushProfiling
=================================================
*/
	bool  Trace::_FlushProfiling (const Sources_t &sources, ELogFormat format, INOUT String &result)
	{
		if ( _profiling.empty() )
			return true;

		// sort by device time
		Array< Pair<ExprInfo const* const, FnExecutionDuration>* >	sorted;
		sorted.reserve( _profiling.size() );

		for (auto& item : _profiling) {
			sorted.push_back( &item );
		}

		std::sort( sorted.begin(), sorted.end(), [](auto* lhs, auto *rhs) { return lhs->second.device > rhs->second.device; });

		// print
		const double	max_subgroup_time	= sorted.front()->second.subgroup ? 100.0 / double(sorted.front()->second.subgroup) : 1.0;
		const double	max_device_time		= sorted.front()->second.device ? 100.0 / double(sorted.front()->second.device) : 1.0;

		const auto		DtoStr = [] (double value) -> String
		{{
			char		buf[32] = {};
			const int	len = std::snprintf( buf, sizeof(buf), "%0.2f", value );

			if ( len <= 0 )
				buf[0] = '\0';

			return buf;
		}};

		for (auto* item : sorted)
		{
			auto&	expr	= *item->first;
			auto&	time	= item->second;

			result << "// subgroup total: " << DtoStr( double(time.subgroup) * max_subgroup_time ) << "%,  avr: " <<
						DtoStr( (double(time.subgroup) * max_subgroup_time) / double(time.count) ) << "%,  (" <<
						DtoStr( double(time.subgroup) / double(time.count) ) << ")\n";

			result << "// device   total: " << DtoStr( double(time.device) * max_device_time ) << "%,  avr: " <<
						DtoStr( (double(time.device) * max_device_time) / double(time.count) ) << "%,  (" <<
						DtoStr( double(time.device) / double(time.count) ) + ")\n";

			result << "// invocations:    " << ToString( time.count ) << "\n";

			CHECK_ERR( AppendSourceRange( expr.range, sources, format, INOUT result ));
			result << "\n";
		}

		return true;
	}

/*
=================================================
	Trace::Flush
=================================================
*/
	bool  Trace::Flush (const VarNames_t &varNames, const Sources_t &sources, ELogFormat format, INOUT String &result)
	{
		CHECK_ERR( _FlushStates( varNames, sources, format, INOUT result ));
		CHECK_ERR( _FlushProfiling( sources, format, INOUT result ));

		return true;
	}

/*
=================================================
	GetTypeSizeOf
=================================================
*/
	ND_ inline uint  GetTypeSizeOf (uint type)
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

} // namespace
//-----------------------------------------------------------------------------



/*
=================================================
	ParseShaderTrace
=================================================
*/
	bool  ShaderTrace::ParseShaderTrace (const void *ptr, const Bytes inMaxSize, ELogFormat format, OUT Array<String> &result) const
	{
		if ( format == Default )
			format = ELogFormat::Text;

		result.clear();

		const ulong		count = *(static_cast<uint const*>(ptr) + _posOffset / sizeof(uint));
		if ( count == 0 )
			return true;

		const ulong		max_size	= ulong(inMaxSize);
		uint const*		start_ptr	= static_cast<uint const*>(ptr) + _dataOffset / sizeof(uint);
		uint const*		end_ptr		= start_ptr + Min( count, (max_size - _dataOffset) / sizeof(uint) );
		Array<Trace>	shaders;

		ASSERT( (count * sizeof(uint)) <= max_size );

		for (auto data_ptr = start_ptr; data_ptr < end_ptr;)
		{
			uint		pos			= uint(Distance( start_ptr, data_ptr ));
			uint		prev_pos	= *(data_ptr++);
			uint		expr_id		= *(data_ptr++);
			uint		type		= *(data_ptr++);
			uint		t_basic		= (type & 0xFF);
			uint		row_size	= (type >> 8) & 0xF;					// for scalar, vector and matrix
			uint		col_size	= Max(1u, (type >> 12) & 0xF );			// only for matrix
			uint const*	data		= data_ptr;
			Trace*		trace		= null;

			CHECK_ERR( (t_basic == uint(TBasicType::EbtVoid) and row_size == 0) or (row_size > 0 and row_size <= 4) );
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
			auto&	str  = result[ Distance( shaders.data(), trace )];

			if ( t_basic == ShaderTrace::TBasicType_Clock )
				CHECK_ERR( trace->AddTime( expr, row_size, col_size, data ))
			else
				CHECK_ERR( trace->AddState( expr, TBasicType(t_basic), row_size, col_size, data, _varNames, _sources, format, INOUT str ));

			trace->lastPosition = pos;
		}

		for (usize i = 0; i < shaders.size(); ++i)
		{
			CHECK_ERR( shaders[i].Flush( _varNames, _sources, format, INOUT result[i] ));
		}
		return true;
	}

} // AE::PipelineCompiler
