// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'
/*
	docs:
	https://github.com/KhronosGroup/GLSL/blob/master/extensions/ext/GL_EXT_shader_realtime_clock.txt
*/

#include "Common.h"

using namespace glslang;
using VariableID = ShaderTrace::VariableID;

namespace
{
	//
	// Debug Info
	//
	struct DebugInfo
	{
	private:
		using CachedSymbols_t		= unordered_map< TString, TIntermSymbol *>;


	private:
		CachedSymbols_t			_cachedSymbols;
		
		int						_maxSymbolId				= 0;
		bool					_startedUserDefinedSymbols	= false;
		
		TIntermSymbol *			_dbgStorage					= nullptr;

		const string			_entryName;
		const EShLanguage		_shLang;


	public:
		DebugInfo (EShLanguage shLang, const string &entry) :
			_entryName{ entry },
			_shLang{ shLang }
		{}

		ND_ string const&	GetEntryPoint ()			const	{ return _entryName; }

		void			AddSymbol (TIntermSymbol* node, bool isUserDefined);
		ND_ int			GetUniqueSymbolID ();

		void CacheSymbolNode (TIntermSymbol* node, bool isUserDefined)
		{
			_cachedSymbols.insert({ node->getName(), node });
			AddSymbol( node, isUserDefined );
		}

		ND_ TIntermSymbol*  GetCachedSymbolNode (const TString &name)
		{
			auto	iter = _cachedSymbols.find( name );
			return	iter != _cachedSymbols.end() ? iter->second : nullptr;
		}

		ND_ TIntermSymbol*	GetDebugStorage ()						const	{ CHECK( _dbgStorage );  return _dbgStorage; }
			bool			SetDebugStorage (TIntermSymbol* symb);
		ND_ TIntermBinary*  GetDebugStorageField (const char* name)	const;
		
		ND_ EShLanguage		GetShaderType ()	const						{ return _shLang; }
	};

/*
=================================================
	AddSymbol
=================================================
*/
void  DebugInfo::AddSymbol (TIntermSymbol* node, bool isUserDefined)
{
	ASSERT( node );
	ASSERT( isUserDefined or not _startedUserDefinedSymbols );

	_maxSymbolId = Max( _maxSymbolId, node->getId() );
}

/*
=================================================
	GetUniqueSymbolID
=================================================
*/
int  DebugInfo::GetUniqueSymbolID ()
{
	_startedUserDefinedSymbols = true;
	return ++_maxSymbolId;
}

/*
=================================================
	SetDebugStorage
=================================================
*/
bool  DebugInfo::SetDebugStorage (TIntermSymbol* symb)
{
	if ( _dbgStorage )
		return true;

	CHECK_ERR( symb and symb->getType().isStruct() );

	_dbgStorage = symb;
	return true;
}

/*
=================================================
	GetDebugStorageField
=================================================
*/
TIntermBinary*  DebugInfo::GetDebugStorageField (const char* name) const
{
	CHECK_ERR( _dbgStorage );
		
	TPublicType		index_type;		index_type.init({});
	index_type.basicType			= TBasicType::EbtInt;
	index_type.qualifier.storage	= TStorageQualifier::EvqConst;

	for (auto& field : *_dbgStorage->getType().getStruct())
	{
		if ( field.type->getFieldName() == name )
		{
			const auto				index			= std::distance( _dbgStorage->getType().getStruct()->data(), &field );
			TConstUnionArray		index_Value(1);	index_Value[0].setIConst( int(index) );
			TIntermConstantUnion*	field_index		= new TIntermConstantUnion{ index_Value, TType{index_type} };
			TIntermBinary*			field_access	= new TIntermBinary{ TOperator::EOpIndexDirectStruct };
			field_access->setType( *field.type );
			field_access->setLeft( _dbgStorage );
			field_access->setRight( field_index );
			return field_access;
		}
	}
	return nullptr;
}
//-----------------------------------------------------------------------------



static bool  RecursiveProcessNode (TIntermNode* node, DebugInfo &dbgInfo);
static bool  RecursiveProcessAggregateNode (TIntermAggregate* aggr, DebugInfo &dbgInfo);
static bool  ProcessSymbolNode (TIntermSymbol* node, DebugInfo &dbgInfo);

	
/*
=================================================
	RecursiveProcessAggregateNode
=================================================
*/
static bool  RecursiveProcessAggregateNode (TIntermAggregate* aggr, DebugInfo &dbgInfo)
{
	for (auto& node : aggr->getSequence())
	{
		CHECK_ERR( RecursiveProcessNode( node, dbgInfo ));
	}

	return true;
}

/*
=================================================
	RecursiveProcessNode
=================================================
*/
static bool  RecursiveProcessNode (TIntermNode* node, DebugInfo &dbgInfo)
{
	if ( not node )
		return true;

	if ( auto* aggr = node->getAsAggregate() )
	{
		CHECK_ERR( RecursiveProcessAggregateNode( aggr, dbgInfo ));
		return true;
	}
	
	if ( auto* unary = node->getAsUnaryNode() )
	{
		CHECK_ERR( RecursiveProcessNode( unary->getOperand(), dbgInfo ));
		return true;
	}

	if ( auto* binary = node->getAsBinaryNode() )
	{
		CHECK_ERR( RecursiveProcessNode( binary->getLeft(), dbgInfo ));
		CHECK_ERR( RecursiveProcessNode( binary->getRight(), dbgInfo ));
		return true;
	}

	if ( auto* op = node->getAsOperator() )
	{
		return true;
	}

	if ( auto* branch = node->getAsBranchNode() )
	{
		CHECK_ERR( RecursiveProcessNode( branch->getExpression(), dbgInfo ));
		return true;
	}

	if ( auto* sw = node->getAsSwitchNode() )
	{
		CHECK_ERR( RecursiveProcessNode( sw->getCondition(), dbgInfo ));
		CHECK_ERR( RecursiveProcessNode( sw->getBody(), dbgInfo ));
		return true;
	}

	if ( auto* selection = node->getAsSelectionNode() )
	{
		CHECK_ERR( RecursiveProcessNode( selection->getCondition(), dbgInfo ));
		CHECK_ERR( RecursiveProcessNode( selection->getTrueBlock(), dbgInfo ));
		CHECK_ERR( RecursiveProcessNode( selection->getFalseBlock(), dbgInfo ));
		return true;
	}

	if ( auto* method = node->getAsMethodNode() )
	{
		return true;
	}

	if ( auto* symbol = node->getAsSymbolNode() )
	{
		CHECK_ERR( ProcessSymbolNode( symbol, dbgInfo ));
		return true;
	}
	
	if ( auto* typed = node->getAsTyped() )
	{
		return true;
	}

	if ( auto* loop = node->getAsLoopNode() )
	{
		CHECK_ERR( RecursiveProcessNode( loop->getBody(), dbgInfo ));
		CHECK_ERR( RecursiveProcessNode( loop->getTerminal(), dbgInfo ));
		CHECK_ERR( RecursiveProcessNode( loop->getTest(), dbgInfo ));
		return true;
	}

	return false;
}
	
/*
=================================================
	ProcessSymbolNode
=================================================
*/
static bool  ProcessSymbolNode (TIntermSymbol* node, DebugInfo &dbgInfo)
{
	if ( // fragment shader
		 node->getName() == "gl_FragCoord"		or
		 // compute shader
		 node->getName() == "gl_WorkGroupID"	or
		 // ray tracing shaders
		 node->getName() == RT_LaunchID			)
	{
		dbgInfo.CacheSymbolNode( node, false );
		return true;
	}

	// do nothing
	return true;
}

/*
=================================================
	CreateShaderDebugStorage
=================================================
*/
static void  CreateShaderDebugStorage (uint descSetIndex, DebugInfo &dbgInfo, OUT uint64_t &pixelsOffset)
{
	//  layout(binding=x, std430) buffer dbg_ShaderTraceStorage {
	//      readonly ivec2  tileSize;
	//      readonly int    width;
	//               uvec2  outPixels[];
	//  } dbg_ShaderTrace
	
	TPublicType		type;		type.init({});
	type.basicType				= TBasicType::EbtInt;
	type.vectorSize				= 2;
	type.qualifier.storage		= TStorageQualifier::EvqBuffer;
	type.qualifier.layoutMatrix	= TLayoutMatrix::ElmColumnMajor;
	type.qualifier.layoutPacking= TLayoutPacking::ElpStd430;
	type.qualifier.precision	= TPrecisionQualifier::EpqHigh;
	type.qualifier.layoutOffset	= 0;
		
#ifdef USE_STORAGE_QUALIFIERS
	type.qualifier.readonly		= true;
#endif
	
	TType*			tile_size	= new TType{type};		tile_size->setFieldName( "tileSize" );
	
	type.qualifier.layoutOffset += sizeof(int) * 2;
	type.vectorSize				= 1;

	TType*			width		= new TType{type};		width->setFieldName( "width" );
	
	type.qualifier.layoutOffset += sizeof(int) * 2;
	type.vectorSize				= 2;
	type.basicType				= TBasicType::EbtUint;
	type.arraySizes				= new TArraySizes{};
	type.arraySizes->addInnerSize();

#ifdef USE_STORAGE_QUALIFIERS
	type.qualifier.coherent		= true;
	type.qualifier.readonly		= false;
#endif

	TType*			pixels		= new TType{type};		pixels->setFieldName( "outPixels" );

	TTypeList*		type_list	= new TTypeList{};
	type_list->push_back({ tile_size,	TSourceLoc{} });
	type_list->push_back({ width,		TSourceLoc{} });
	type_list->push_back({ pixels,		TSourceLoc{} });

	TQualifier		block_qual;	block_qual.clear();
	block_qual.storage			= TStorageQualifier::EvqBuffer;
	block_qual.layoutMatrix		= TLayoutMatrix::ElmColumnMajor;
	block_qual.layoutPacking	= TLayoutPacking::ElpStd430;
	block_qual.layoutBinding	= 0;
	block_qual.layoutSet		= descSetIndex;

	TIntermSymbol*	storage_buf	= new TIntermSymbol{ 0x10000001, "dbg_ShaderTrace", TType{type_list, "dbg_ShaderTraceStorage", block_qual} };

	pixelsOffset = pixels->getQualifier().layoutOffset;
	
	dbgInfo.SetDebugStorage( storage_buf );
}

/*
=================================================
	CreateShaderBuiltinSymbols
=================================================
*/
static void  CreateShaderBuiltinSymbols (TIntermNode* root, DebugInfo &dbgInfo)
{
	const auto	shader				= dbgInfo.GetShaderType();
	const bool	is_compute			= (shader == EShLangCompute or shader == EShLangTaskNV or shader == EShLangMeshNV);
	const bool	need_invocation_id	= (shader == EShLangGeometry or shader == EShLangTessControl);
	const bool	need_primitive_id	= (shader == EShLangFragment or shader == EShLangTessControl or shader == EShLangTessEvaluation);
	
	#ifdef USE_NV_RAY_TRACING
	const bool	need_launch_id		= (shader == EShLangRayGenNV or shader == EShLangIntersectNV or shader == EShLangAnyHitNV or
									   shader == EShLangClosestHitNV or shader == EShLangMissNV or shader == EShLangCallableNV);
	#else
	const bool	need_launch_id		= (shader == EShLangRayGen or shader == EShLangIntersect or shader == EShLangAnyHit or
									   shader == EShLangClosestHit or shader == EShLangMiss or shader == EShLangCallable);
	#endif

	TSourceLoc	loc	{};

	if ( shader == EShLangFragment and not dbgInfo.GetCachedSymbolNode( "gl_FragCoord" ))
	{
		TPublicType		vec4_type;	vec4_type.init({});
		vec4_type.basicType			= TBasicType::EbtFloat;
		vec4_type.vectorSize		= 4;
		vec4_type.qualifier.storage	= TStorageQualifier::EvqFragCoord;
		vec4_type.qualifier.builtIn	= TBuiltInVariable::EbvFragCoord;
		
		TIntermSymbol*	symb = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "gl_FragCoord", TType{vec4_type} };
		symb->setLoc( loc );
		dbgInfo.CacheSymbolNode( symb, true );
	}

	if ( is_compute and not dbgInfo.GetCachedSymbolNode( "gl_WorkGroupID" ))
	{
		TPublicType		uint_type;	uint_type.init({});
		uint_type.basicType			= TBasicType::EbtUint;
		uint_type.vectorSize		= 3;
		uint_type.qualifier.storage	= TStorageQualifier::EvqVaryingIn;
		uint_type.qualifier.builtIn	= TBuiltInVariable::EbvWorkGroupId;

		TIntermSymbol*	symb = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "gl_WorkGroupID", TType{uint_type} };
		symb->setLoc( loc );
		dbgInfo.CacheSymbolNode( symb, true );
	}

	if ( need_launch_id and not dbgInfo.GetCachedSymbolNode( RT_LaunchID ))
	{
		TPublicType		uint_type;	uint_type.init({});
		uint_type.basicType			= TBasicType::EbtUint;
		uint_type.vectorSize		= 3;
		uint_type.qualifier.storage	= TStorageQualifier::EvqVaryingIn;
		uint_type.qualifier.builtIn	= RT_EbvLaunchId;

		TIntermSymbol*	symb = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), RT_LaunchID, TType{uint_type} };
		symb->setLoc( loc );
		dbgInfo.CacheSymbolNode( symb, true );
	}
}

/*
=================================================
	GetFragmentCoord
=================================================
*/
ND_ static TIntermBinary*  GetFragmentCoord (TIntermSymbol* coord, DebugInfo &dbgInfo)
{
	TPublicType		type;		type.init({});
	type.basicType				= TBasicType::EbtInt;
	type.vectorSize				= 2;
	type.qualifier.precision	= TPrecisionQualifier::EpqHigh;
	type.qualifier.storage		= TStorageQualifier::EvqTemporary;

	// "... ivec2(gl_FragCoord.xy) ..."
	TIntermSymbol*		fragcoord	= dbgInfo.GetCachedSymbolNode( "gl_FragCoord" );
	TIntermUnary*		to_ivec2	= new TIntermUnary{ TOperator::EOpConvFloatToInt };
	to_ivec2->setType( TType{type} );
	to_ivec2->setOperand( fragcoord );
				
	// "... ivec2(gl_FragCoord.xy) / dbg_ShaderTrace.tileSize"
	TIntermBinary*		frag_div_tile	= new TIntermBinary{ TOperator::EOpDiv };
	TIntermBinary*		tile_size		= dbgInfo.GetDebugStorageField( "tileSize" );
	frag_div_tile->setType( TType{type} );
	frag_div_tile->setLeft( to_ivec2 );
	frag_div_tile->setRight( tile_size );

	// "ivec2  dbg_Coord = ivec2(gl_FragCoord.xy) / dbg_ShaderTrace.tileSize"
	TIntermBinary*		assign_coord	= new TIntermBinary{ TOperator::EOpAssign };
	assign_coord->setType( TType{type} );
	assign_coord->setLeft( coord );
	assign_coord->setRight( frag_div_tile );

	return assign_coord;
}

/*
=================================================
	GetComputeCoord
=================================================
*/
ND_ static TIntermBinary*  GetComputeCoord (TIntermSymbol* coord, DebugInfo &dbgInfo)
{
	TPublicType		type;		type.init({});
	type.basicType				= TBasicType::EbtInt;
	type.vectorSize				= 2;
	type.qualifier.precision	= TPrecisionQualifier::EpqHigh;
	type.qualifier.storage		= TStorageQualifier::EvqTemporary;

	// "... ivec2(gl_WorkGroupID.xy) ..."
	TIntermSymbol*		workgroup	= dbgInfo.GetCachedSymbolNode( "gl_WorkGroupID" );
	TIntermUnary*		to_ivec2	= new TIntermUnary{ TOperator::EOpConvUintToInt };
	to_ivec2->setType( TType{type} );
	to_ivec2->setOperand( workgroup );
				
	// "... ivec2(gl_WorkGroupID.xy) / dbg_ShaderTrace.tileSize"
	TIntermBinary*		frag_div_tile	= new TIntermBinary{ TOperator::EOpDiv };
	TIntermBinary*		tile_size		= dbgInfo.GetDebugStorageField( "tileSize" );
	frag_div_tile->setType( TType{type} );
	frag_div_tile->setLeft( to_ivec2 );
	frag_div_tile->setRight( tile_size );

	// "ivec2  dbg_Coord = ivec2(gl_WorkGroupID.xy) / dbg_ShaderTrace.tileSize"
	TIntermBinary*		assign_coord	= new TIntermBinary{ TOperator::EOpAssign };
	assign_coord->setType( TType{type} );
	assign_coord->setLeft( coord );
	assign_coord->setRight( frag_div_tile );

	return assign_coord;
}

/*
=================================================
	GetRayTracingCoord
=================================================
*/
ND_ static TIntermBinary*  GetRayTracingCoord (TIntermSymbol* coord, DebugInfo &dbgInfo)
{
	TPublicType		type;		type.init({});
	type.basicType				= TBasicType::EbtInt;
	type.vectorSize				= 2;
	type.qualifier.precision	= TPrecisionQualifier::EpqHigh;
	type.qualifier.storage		= TStorageQualifier::EvqTemporary;

	// "... ivec2(gl_LaunchID.xy) ..."
	TIntermSymbol*		launch_id	= dbgInfo.GetCachedSymbolNode( RT_LaunchID );
	TIntermUnary*		to_ivec2	= new TIntermUnary{ TOperator::EOpConvUintToInt };
	to_ivec2->setType( TType{type} );
	to_ivec2->setOperand( launch_id );
				
	// "... ivec2(gl_LaunchID.xy) / dbg_ShaderTrace.tileSize"
	TIntermBinary*		frag_div_tile	= new TIntermBinary{ TOperator::EOpDiv };
	TIntermBinary*		tile_size		= dbgInfo.GetDebugStorageField( "tileSize" );
	frag_div_tile->setType( TType{type} );
	frag_div_tile->setLeft( to_ivec2 );
	frag_div_tile->setRight( tile_size );

	// "ivec2  dbg_Coord = ivec2(gl_LaunchID.xy) / dbg_ShaderTrace.tileSize"
	TIntermBinary*		assign_coord	= new TIntermBinary{ TOperator::EOpAssign };
	assign_coord->setType( TType{type} );
	assign_coord->setLeft( coord );
	assign_coord->setRight( frag_div_tile );

	return assign_coord;
}

/*
=================================================
	InsertShaderTimeMeasurement
=================================================
*/
static bool  InsertShaderTimeMeasurementToEntry (TIntermAggregate* entry, DebugInfo &dbgInfo)
{
	TIntermSymbol*	start_time	= nullptr;

	TPublicType		type;		type.init({});
	type.basicType				= TBasicType::EbtUint;
	type.vectorSize				= 2;
	type.qualifier.storage		= TStorageQualifier::EvqTemporary;
	type.qualifier.precision	= TPrecisionQualifier::EpqHigh;
			
	TPublicType		index_type;	 index_type.init({});
	index_type.basicType		 = TBasicType::EbtInt;
	index_type.qualifier.storage = TStorageQualifier::EvqConst;
	
	// "ivec2  dbg_Coord = ..."
	TIntermSymbol*	coord		= new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "dbg_Coord", TType{type} };
	TIntermBinary*	assign_coord = nullptr;
	{
		BEGIN_ENUM_CHECKS();
		switch ( dbgInfo.GetShaderType() )
		{
			case EShLanguage::EShLangVertex :
			case EShLanguage::EShLangTessControl :
			case EShLanguage::EShLangTessEvaluation :
			case EShLanguage::EShLangGeometry :
				return false;	// not supported yet

			case EShLanguage::EShLangFragment :
				assign_coord = GetFragmentCoord( coord, dbgInfo );
				break;

			case EShLanguage::EShLangCompute :
				assign_coord = GetComputeCoord( coord, dbgInfo );
				break;
				
		#ifdef USE_NV_RAY_TRACING
			case EShLanguage::EShLangRayGenNV :
			case EShLanguage::EShLangIntersectNV :
			case EShLanguage::EShLangAnyHitNV :
			case EShLanguage::EShLangClosestHitNV :
			case EShLanguage::EShLangMissNV :
			case EShLanguage::EShLangCallableNV :
				assign_coord = GetRayTracingCoord( coord, dbgInfo );
				break;
		#else
			case EShLanguage::EShLangRayGen :
			case EShLanguage::EShLangIntersect :
			case EShLanguage::EShLangAnyHit :
			case EShLanguage::EShLangClosestHit :
			case EShLanguage::EShLangMiss :
			case EShLanguage::EShLangCallable :
				assign_coord = GetRayTracingCoord( coord, dbgInfo );
				break;
		#endif

			case EShLanguage::EShLangTaskNV :
			case EShLanguage::EShLangMeshNV :
				return false;	// not supported yet

			case EShLanguage::EShLangCount :
			default :
				return false;	// unknown
		}
		END_ENUM_CHECKS();
	}

	// "uvec2  dbg_StartTime = clockRealtime2x32EXT();"
	{
		type.qualifier.storage	= TStorageQualifier::EvqTemporary;
				
		TIntermAggregate*	time_call	= new TIntermAggregate( TOperator::EOpReadClockDeviceKHR );
		time_call->setType( TType{type} );
				
		start_time = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "dbg_StartTime", TType{type} };

		TIntermBinary*		assign_time	= new TIntermBinary{ TOperator::EOpAssign };
		type.qualifier.storage	= TStorageQualifier::EvqTemporary;
		assign_time->setType( TType{type} );
		assign_time->setLeft( start_time );
		assign_time->setRight( time_call );

		entry->getSequence().insert( entry->getSequence().begin(), assign_time );
	}
	{
		type.qualifier.storage = TStorageQualifier::EvqTemporary;

		// "... clockRealtime2x32EXT() ..."
		TIntermAggregate*	time_call	= new TIntermAggregate( TOperator::EOpReadClockDeviceKHR );
		time_call->setType( TType{type} );

		// "... clockRealtime2x32EXT() - dbg_StartTime"
		TIntermBinary*		time_delta	= new TIntermBinary{ TOperator::EOpSub };
		time_delta->setType( TType{type} );
		time_delta->setLeft( time_call );
		time_delta->setRight( start_time );
				
		// "uvec2  dbg_Diff = clockRealtime2x32EXT() - dbg_StartTime"
		TIntermSymbol*		diff		= new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "dbg_Diff", TType{type} };
		TIntermBinary*		assign_diff	= new TIntermBinary{ TOperator::EOpAssign };
		assign_diff->setType( TType{type} );
		assign_diff->setLeft( diff );
		assign_diff->setRight( time_delta );
				
		// "dbg_Coord.x"
		TConstUnionArray		x_index(1);	x_index[0].setIConst( 0 );
		TIntermConstantUnion*	x_field		= new TIntermConstantUnion{ x_index, TType{index_type} };
		TIntermBinary*			coord_x		= new TIntermBinary{ TOperator::EOpIndexDirect };
		type.vectorSize = 1;
		coord_x->setType( TType{type} );
		coord_x->setLeft( coord );
		coord_x->setRight( x_field );

		// "dbg_Coord.y"
		TConstUnionArray		y_index(1);	y_index[0].setIConst( 1 );
		TIntermConstantUnion*	y_field		= new TIntermConstantUnion{ y_index, TType{index_type} };
		TIntermBinary*			coord_y		= new TIntermBinary{ TOperator::EOpIndexDirect };
		coord_y->setType( TType{type} );
		coord_y->setLeft( coord );
		coord_y->setRight( y_field );

		// "... dbg_Coord.y * dbg_ShaderTrace.width"
		TIntermBinary*		width			= dbgInfo.GetDebugStorageField( "width" );
		TIntermBinary*		cy_mul_width	= new TIntermBinary{ TOperator::EOpMul };
		cy_mul_width->setType( TType{type} );
		cy_mul_width->setLeft( coord_y );
		cy_mul_width->setRight( width );
				
		// "... dbg_Coord.x + dbg_Coord.y * dbg_ShaderTrace.width"
		TIntermBinary*		calc_index	= new TIntermBinary{ TOperator::EOpAdd };
		calc_index->setType( TType{type} );
		calc_index->setLeft( coord_x );
		calc_index->setRight( cy_mul_width );

		// TODO: clamp index
		// "int  dbg_Index = dbg_Coord.x + dbg_Coord.y * dbg_ShaderTrace.width"
		TIntermSymbol*		index			= new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "dbg_Index", TType{type} };
		TIntermBinary*		assign_index	= new TIntermBinary{ TOperator::EOpAssign };
		assign_index->setType( TType{type} );
		assign_index->setLeft( index );
		assign_index->setRight( calc_index );
				
		// "dbg_ShaderTrace.outPixels[dbg_Index]"
		TIntermBinary*		out_pixel		= dbgInfo.GetDebugStorageField( "outPixels" );
		TIntermBinary*		curr_pixel		= new TIntermBinary{ TOperator::EOpIndexIndirect };
		type.basicType	= TBasicType::EbtUint;
		type.vectorSize	= 2;
		curr_pixel->setType( TType{type} );
		curr_pixel->getWritableType().setFieldName( "outPixels" );
		curr_pixel->setLeft( out_pixel );
		curr_pixel->setRight( index );
				
		// "dbg_ShaderTrace.outPixels[dbg_Index].x"
		TIntermBinary*		out_pixel_x		= new TIntermBinary{ TOperator::EOpIndexDirect };
		type.vectorSize	= 1;
		out_pixel_x->setType( TType{type} );
		out_pixel_x->setLeft( curr_pixel );
		out_pixel_x->setRight( x_field );

		// "dbg_Diff.x"
		TIntermBinary*		diff_x			= new TIntermBinary{ TOperator::EOpIndexDirect };
		diff_x->setType( TType{type} );
		diff_x->setLeft( diff );
		diff_x->setRight( x_field );

		// "... atomicAdd( dbg_ShaderTrace.outPixels[dbg_Index].x, dbg_Diff.x )"
		TIntermAggregate*	atomic_add_x	= new TIntermAggregate{ TOperator::EOpAtomicAdd };
		type.qualifier.storage = TStorageQualifier::EvqGlobal;
		atomic_add_x->setType( TType{type} );
		atomic_add_x->setOperationPrecision( TPrecisionQualifier::EpqHigh );
		atomic_add_x->getQualifierList().push_back( TStorageQualifier::EvqInOut );
		atomic_add_x->getQualifierList().push_back( TStorageQualifier::EvqIn );
		atomic_add_x->getSequence().push_back( out_pixel_x );
		atomic_add_x->getSequence().push_back( diff_x );

		// "uint  dbg_OldVal = atomicAdd( dbg_ShaderTrace.outPixels[dbg_Index].x, dbg_Diff.x )"
		type.qualifier.storage = TStorageQualifier::EvqTemporary;
		TIntermSymbol*		old_val			= new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "dbg_OldVal", TType{type} };
		TIntermBinary*		assign_oldval	= new TIntermBinary{ TOperator::EOpAssign };
		assign_oldval->setType( TType{type} );
		assign_oldval->setLeft( old_val );
		assign_oldval->setRight( atomic_add_x );

		// "... dbg_OldVal + dbg_Diff.x ..."
		TIntermBinary*		oldval_add_diffx	= new TIntermBinary{ TOperator::EOpAdd };
		oldval_add_diffx->setType( TType{type} );
		oldval_add_diffx->setLeft( old_val );
		oldval_add_diffx->setRight( diff_x );

		// "... (dbg_OldVal + dbg_Diff.x) < dbg_OldVal ..."
		TIntermBinary*		less_then_oldval	= new TIntermBinary{ TOperator::EOpLessThan };
		type.basicType	= TBasicType::EbtBool;
		type.vectorSize	= 1;
		less_then_oldval->setType( TType{type} );
		less_then_oldval->setLeft( oldval_add_diffx );
		less_then_oldval->setRight( old_val );

		// "... (dbg_OldVal + dbg_Diff.x < dbg_OldVal ? 1 : 0)"
		type.basicType			= TBasicType::EbtUint;
		type.qualifier.storage	= TStorageQualifier::EvqConst;
		TIntermConstantUnion*	zero_val		= new TIntermConstantUnion{ x_index, TType{type} };
		TIntermConstantUnion*	one_val			= new TIntermConstantUnion{ y_index, TType{type} };
		TIntermSelection*		add_one_or_zero	= new TIntermSelection{ less_then_oldval, one_val, zero_val };
		add_one_or_zero->setType( TType{type} );
				
		// "dbg_Diff.y"
		TIntermBinary*		diff_y			= new TIntermBinary{ TOperator::EOpIndexDirect };
		type.qualifier.storage = TStorageQualifier::EvqTemporary;
		diff_y->setType( TType{type} );
		diff_y->setLeft( diff );
		diff_y->setRight( y_field );

		// "... dbg_Diff.y + (dbg_OldVal + dbg_Diff.x < dbg_OldVal ? 1 : 0)"
		TIntermBinary*		diffy_add_sel	= new TIntermBinary{ TOperator::EOpAdd };
		diffy_add_sel->setType( TType{type} );
		diffy_add_sel->setLeft( diff_y );
		diffy_add_sel->setRight( add_one_or_zero );

		// "... dbg_ShaderTrace.outPixels[dbg_Index].y ..."
		TIntermBinary*		out_pixel_y		= new TIntermBinary{ TOperator::EOpIndexDirect };
		out_pixel_y->setType( TType{type} );
		out_pixel_y->setLeft( curr_pixel );
		out_pixel_y->setRight( y_field );

		// "atomicAdd( dbg_ShaderTrace.outPixels[dbg_Index].y, dbg_Diff.y + ((dbg_OldVal + dbg_Diff.x) < dbg_OldVal ? 1 : 0) )"
		TIntermAggregate*	atomic_add_y	= new TIntermAggregate{ TOperator::EOpAtomicAdd };
		type.qualifier.storage = TStorageQualifier::EvqGlobal;
		atomic_add_y->setType( TType{type} );
		atomic_add_y->setOperationPrecision( TPrecisionQualifier::EpqHigh );
		atomic_add_y->getQualifierList().push_back( TStorageQualifier::EvqInOut );
		atomic_add_y->getQualifierList().push_back( TStorageQualifier::EvqIn );
		atomic_add_y->getSequence().push_back( out_pixel_y );
		atomic_add_y->getSequence().push_back( diffy_add_sel );
				
		TIntermAggregate*	sequence	= new TIntermAggregate( TOperator::EOpSequence );
		sequence->getSequence().push_back( assign_diff );
		sequence->getSequence().push_back( assign_coord );
		sequence->getSequence().push_back( assign_index );
		sequence->getSequence().push_back( assign_oldval );
		sequence->getSequence().push_back( atomic_add_y );

		entry->getSequence().push_back( sequence );
	}
	return true;
}

/*
=================================================
	InsertShaderTimeMeasurement
=================================================
*/
static bool  InsertShaderTimeMeasurement (TIntermNode* root, DebugInfo &dbgInfo)
{
	TIntermAggregate*	aggr = root->getAsAggregate();
	CHECK_ERR( aggr );

	// find shader resources entry
	TIntermAggregate*	linker_objs	= nullptr;
	
	for (auto& entry : aggr->getSequence())
	{
		if ( auto*  aggr2 = entry->getAsAggregate() )
		{
			if ( aggr2->getOp() == TOperator::EOpLinkerObjects )
			{
				linker_objs = aggr2;
				break;
			}
		}
	}
	
	if ( not linker_objs ) {
		linker_objs = new TIntermAggregate{ TOperator::EOpLinkerObjects };
		aggr->getSequence().push_back( linker_objs );
	}

	linker_objs->getSequence().insert( linker_objs->getSequence().begin(), dbgInfo.GetDebugStorage() );
	

	// find entry function
	for (auto& entry : aggr->getSequence())
	{
		auto*	aggr2 = entry->getAsAggregate();
		if ( not (aggr2 and aggr2->getOp() == TOperator::EOpFunction) )
			continue;
			
		if ( aggr2->getName().c_str() == dbgInfo.GetEntryPoint()	and
			 aggr2->getSequence().size() >= 2 )
		{
			auto*	body = aggr2->getSequence()[1]->getAsAggregate();
			CHECK_ERR( body );
			
			CHECK_ERR( InsertShaderTimeMeasurementToEntry( body, dbgInfo ));
			return true;
		}
	}
	return false;
}

}	// namespace
//-----------------------------------------------------------------------------



/*
=================================================
	InsertShaderClockMap
=================================================
*/
bool  ShaderTrace::InsertShaderClockMap (glslang::TIntermediate &intermediate, uint32_t descSetIndex)
{
	intermediate.addRequestedExtension( "GL_EXT_shader_realtime_clock" );

	DebugInfo		dbg_info{ intermediate.getStage(), intermediate.getEntryPointMangledName() };

	TIntermNode*	root = intermediate.getTreeRoot();
	CHECK_ERR( root );
	
	_posOffset = ~0ull;
	CreateShaderDebugStorage( descSetIndex, dbg_info, OUT _dataOffset );
	
	CHECK_ERR( RecursiveProcessNode( root, dbg_info ));

	CreateShaderBuiltinSymbols( root, dbg_info );

	InsertShaderTimeMeasurement( root, dbg_info );

	return true;
}
