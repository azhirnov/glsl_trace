// Copyright (c) Zhirnov Andrey. For more information see 'LICENSE'
/*
	docs:
	https://github.com/KhronosGroup/GLSL/blob/master/extensions/ext/GL_EXT_shader_realtime_clock.txt
*/

#include "Common.h"

namespace AE::PipelineCompiler
{
namespace
{
	using namespace glslang;


	//
	// Debug Info
	//
	struct DebugInfo
	{
	private:
		using CachedSymbols_t	= HashMap< TString, TIntermSymbol *>;


	private:
		CachedSymbols_t			_cachedSymbols;

		slong					_maxSymbolId				= 0;
		bool					_startedUserDefinedSymbols	= false;

		TIntermSymbol *			_dbgStorage					= null;

		const String			_entryName;
		const EShLanguage		_shLang;


	public:
		explicit DebugInfo (const TIntermediate &intermediate) :
			_entryName{ intermediate.getEntryPointMangledName() },
			_shLang{ intermediate.getStage() }
		{}

		ND_ StringView	GetEntryPoint ()	const	{ return _entryName; }

			void		AddSymbol (TIntermSymbol* node, bool isUserDefined);
		ND_ slong		GetUniqueSymbolID ();

		void CacheSymbolNode (TIntermSymbol* node, bool isUserDefined)
		{
			_cachedSymbols.emplace( node->getName(), node );
			AddSymbol( node, isUserDefined );
		}

		ND_ TIntermSymbol*  GetCachedSymbolNode (const TString &name)
		{
			auto	iter = _cachedSymbols.find( name );
			return	iter != _cachedSymbols.end() ? iter->second : null;
		}

		ND_ TIntermSymbol*	GetDebugStorage ()						const	{ CHECK( _dbgStorage );  return _dbgStorage; }
			bool			SetDebugStorage (TIntermSymbol* symb);
		ND_ TIntermBinary*  GetDebugStorageField (const char* name)	const;

		ND_ EShLanguage		GetShaderType ()						const	{ return _shLang; }
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
	Unused( isUserDefined );

	_maxSymbolId = Max( _maxSymbolId, node->getId() );
}

/*
=================================================
	GetUniqueSymbolID
=================================================
*/
slong  DebugInfo::GetUniqueSymbolID ()
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

	TPublicType		index_type;		index_type.init( Default );
	index_type.basicType			= TBasicType::EbtInt;
	index_type.qualifier.storage	= TStorageQualifier::EvqConst;

	for (auto& field : *_dbgStorage->getType().getStruct())
	{
		if ( field.type->getFieldName() == name )
		{
			const auto				index			= Distance( _dbgStorage->getType().getStruct()->data(), &field );
			TConstUnionArray		index_Value(1);	index_Value[0].setIConst( int(index) );
			TIntermConstantUnion*	field_index		= new TIntermConstantUnion{ index_Value, TType{index_type} };
			TIntermBinary*			field_access	= new TIntermBinary{ TOperator::EOpIndexDirectStruct };
			field_access->setType( *field.type );
			field_access->setLeft( _dbgStorage );
			field_access->setRight( field_index );
			return field_access;
		}
	}
	return null;
}
//-----------------------------------------------------------------------------



ND_ static bool  RecursiveProcessNode (TIntermNode* node, DebugInfo &dbgInfo);
ND_ static bool  RecursiveProcessAggregateNode (TIntermAggregate* aggr, DebugInfo &dbgInfo);
ND_ static bool  ProcessSymbolNode (TIntermSymbol* node, DebugInfo &dbgInfo);


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
		 node->getName() == "gl_FragCoord"			or
		 // compute shader
		 node->getName() == "gl_GlobalInvocationID"	or
		 // ray tracing shaders
		 node->getName() == "gl_LaunchIDEXT" )
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
static void  CreateShaderDebugStorage (uint descSetIndex, DebugInfo &dbgInfo, OUT ulong &pixelsOffset)
{
	//	staticSize: 16, arrayStride: 4
	//  layout(binding=x, std430) buffer dbg_ShaderTraceStorage {
	//      readonly vec2   scale;
	//      readonly ivec2  dimension;
	//      coherent float  outPixels [];
	//  } dbg_ShaderTrace;

	TPublicType		type;		type.init( Default );
	type.basicType				= TBasicType::EbtFloat;
	type.vectorSize				= 2;
	type.qualifier.storage		= TStorageQualifier::EvqBuffer;
	type.qualifier.layoutMatrix	= TLayoutMatrix::ElmColumnMajor;
	type.qualifier.layoutPacking= TLayoutPacking::ElpStd430;
	type.qualifier.precision	= TPrecisionQualifier::EpqHigh;
	type.qualifier.layoutOffset	= 0;

#ifdef USE_STORAGE_QUALIFIERS
	type.qualifier.readonly		= true;
#endif

	TType*			scale		= new TType{type};		scale->setFieldName( "scale" );

	type.basicType				= TBasicType::EbtInt;
	type.qualifier.layoutOffset += 8;

	TType*			dimension	= new TType{type};		dimension->setFieldName( "dimension" );

	type.basicType				= TBasicType::EbtUint;
	type.arraySizes				= new TArraySizes{};
	type.vectorSize				= 1;
	type.arraySizes->addInnerSize();
	type.qualifier.layoutOffset += 8;

#ifdef USE_STORAGE_QUALIFIERS
	type.qualifier.coherent		= true;
	type.qualifier.readonly		= false;
#endif

	TType*			pixels		= new TType{type};		pixels->setFieldName( "outPixels" );

	TTypeList*		type_list	= new TTypeList{};
	type_list->push_back( TTypeLoc{ scale,		TSourceLoc{} });
	type_list->push_back( TTypeLoc{ dimension,	TSourceLoc{} });
	type_list->push_back( TTypeLoc{ pixels,		TSourceLoc{} });

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
static void  CreateShaderBuiltinSymbols (TIntermNode*, DebugInfo &dbgInfo)
{
	const auto	shader				= dbgInfo.GetShaderType();
	const bool	is_compute			= (shader == EShLangCompute or shader == EShLangTask or shader == EShLangMesh);
	const bool	need_launch_id		= (shader == EShLangRayGen or shader == EShLangIntersect or shader == EShLangAnyHit or
									   shader == EShLangClosestHit or shader == EShLangMiss or shader == EShLangCallable);
	TSourceLoc	loc	{};

	if ( shader == EShLangFragment and not dbgInfo.GetCachedSymbolNode( "gl_FragCoord" ))
	{
		TPublicType		vec4_type;	vec4_type.init( Default );
		vec4_type.basicType			= TBasicType::EbtFloat;
		vec4_type.vectorSize		= 4;
		vec4_type.qualifier.storage	= TStorageQualifier::EvqFragCoord;
		vec4_type.qualifier.builtIn	= TBuiltInVariable::EbvFragCoord;

		TIntermSymbol*	symb = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "gl_FragCoord", TType{vec4_type} };
		symb->setLoc( loc );
		dbgInfo.CacheSymbolNode( symb, true );
	}

	if ( is_compute and not dbgInfo.GetCachedSymbolNode( "gl_GlobalInvocationID" ))
	{
		TPublicType		uint_type;	uint_type.init( Default );
		uint_type.basicType			= TBasicType::EbtUint;
		uint_type.vectorSize		= 3;
		uint_type.qualifier.storage	= TStorageQualifier::EvqVaryingIn;
		uint_type.qualifier.builtIn	= TBuiltInVariable::EbvGlobalInvocationId;

		TIntermSymbol*	symb = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "gl_GlobalInvocationID", TType{uint_type} };
		symb->setLoc( loc );
		dbgInfo.CacheSymbolNode( symb, true );
	}

	if ( need_launch_id and not dbgInfo.GetCachedSymbolNode( "gl_LaunchIDEXT" ))
	{
		TPublicType		uint_type;	uint_type.init( Default );
		uint_type.basicType			= TBasicType::EbtUint;
		uint_type.vectorSize		= 3;
		uint_type.qualifier.storage	= TStorageQualifier::EvqVaryingIn;
		uint_type.qualifier.builtIn	= TBuiltInVariable::EbvLaunchId;

		TIntermSymbol*	symb = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "gl_LaunchIDEXT", TType{uint_type} };
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
	TPublicType		type;		type.init( Default );
	type.basicType				= TBasicType::EbtInt;
	type.vectorSize				= 1;
	type.qualifier.precision	= TPrecisionQualifier::EpqHigh;
	type.qualifier.storage		= TStorageQualifier::EvqConst;

	// "gl_FragCoord.xy"
	TConstUnionArray		x_index(1);		x_index[0].setIConst( 0 );
	TIntermConstantUnion*	x_field			= new TIntermConstantUnion{ x_index, TType{type} };
	TConstUnionArray		y_index(1);		y_index[0].setIConst( 1 );
	TIntermConstantUnion*	y_field			= new TIntermConstantUnion{ y_index, TType{type} };
	TIntermSymbol*			fragcoord		= dbgInfo.GetCachedSymbolNode( "gl_FragCoord" );
	TIntermAggregate*		xy_field		= new TIntermAggregate{ TOperator::EOpSequence };
	TIntermBinary*			fragcoord_xy	= new TIntermBinary{ TOperator::EOpVectorSwizzle };
	xy_field->getSequence().push_back( x_field );
	xy_field->getSequence().push_back( y_field );
	type.basicType			= TBasicType::EbtFloat;
	type.vectorSize			= 2;
	type.qualifier.storage	= TStorageQualifier::EvqTemporary;
	fragcoord_xy->setType( TType{type} );
	fragcoord_xy->setLeft( fragcoord );
	fragcoord_xy->setRight( xy_field );

	// "... gl_FragCoord.xy * dbg_ShaderTrace.scale"
	TIntermBinary*		frag_mul_scale	= new TIntermBinary{ TOperator::EOpMul };
	TIntermBinary*		scale			= dbgInfo.GetDebugStorageField( "scale" );
	CHECK_ERR( scale );
	frag_mul_scale->setType( TType{type} );
	frag_mul_scale->setLeft( fragcoord_xy );
	frag_mul_scale->setRight( scale );

	// "... ivec2(gl_FragCoord.xy * dbg_ShaderTrace.scale) ..."
	TIntermUnary*		to_ivec2	= new TIntermUnary{ TOperator::EOpConvFloatToInt };
	type.basicType		= TBasicType::EbtInt;
	to_ivec2->setType( TType{type} );
	to_ivec2->setOperand( frag_mul_scale );

	type.qualifier.storage	= TStorageQualifier::EvqConst;

	// "ivec2(0)"
	TConstUnionArray		zero_value(2);	zero_value[0].setIConst( 0 );  zero_value[1].setIConst( 0 );
	TIntermConstantUnion*	zero_ivec		= new TIntermConstantUnion{ zero_value, TType{type} };

	// "ivec2(1)"
	TConstUnionArray		one_value(2);	one_value[0].setIConst( 1 );  one_value[1].setIConst( 1 );
	TIntermConstantUnion*	one_ivec		= new TIntermConstantUnion{ one_value, TType{type} };

	type.qualifier.storage	= TStorageQualifier::EvqTemporary;


	// "... dimension - ivec2(1) ..."
	TIntermBinary*		dim_sub_one		= new TIntermBinary{ TOperator::EOpSub };
	TIntermBinary*		dimension		= dbgInfo.GetDebugStorageField( "dimension" );
	CHECK_ERR( dimension );
	dim_sub_one->setType( TType{type} );
	dim_sub_one->setLeft( dimension );
	dim_sub_one->setRight( one_ivec );

	// "... clamp( ivec2(gl_FragCoord.xy * dbg_ShaderTrace.scale), ivec2(0), dimension - ivec2(1) ) ..."
	TIntermAggregate*		clamp_op	= new TIntermAggregate{ TOperator::EOpClamp };
	type.vectorSize			= 2;
	type.qualifier.storage	= TStorageQualifier::EvqGlobal;
	clamp_op->setType( TType{type} );
	clamp_op->setOperationPrecision( TPrecisionQualifier::EpqHigh );
	clamp_op->getQualifierList().push_back( TStorageQualifier::EvqIn );
	clamp_op->getQualifierList().push_back( TStorageQualifier::EvqIn );
	clamp_op->getQualifierList().push_back( TStorageQualifier::EvqIn );
	clamp_op->getSequence().push_back( to_ivec2 );
	clamp_op->getSequence().push_back( zero_ivec );
	clamp_op->getSequence().push_back( dim_sub_one );

	// "ivec2  dbg_Coord = clamp( ivec2(gl_FragCoord.xy * dbg_ShaderTrace.scale), ivec2(0), dimension - ivec2(1) )"
	TIntermBinary*		assign_coord	= new TIntermBinary{ TOperator::EOpAssign };
	type.qualifier.storage = TStorageQualifier::EvqTemporary;
	assign_coord->setType( TType{type} );
	assign_coord->setLeft( coord );
	assign_coord->setRight( clamp_op );

	return assign_coord;
}

/*
=================================================
	GetComputeCoord
=================================================
*/
ND_ static TIntermBinary*  GetComputeCoord (TIntermSymbol* coord, DebugInfo &dbgInfo)
{
	TPublicType		type;		type.init( Default );
	type.basicType				= TBasicType::EbtFloat;
	type.vectorSize				= 2;
	type.qualifier.precision	= TPrecisionQualifier::EpqHigh;
	type.qualifier.storage		= TStorageQualifier::EvqTemporary;

	// "... vec2(gl_GlobalInvocationID) ..."
	TIntermSymbol*		workgroup	= dbgInfo.GetCachedSymbolNode( "gl_GlobalInvocationID" );
	TIntermUnary*		to_vec2		= new TIntermUnary{ TOperator::EOpConvUintToFloat };
	to_vec2->setType( TType{type} );
	to_vec2->setOperand( workgroup );

	// "... vec2(gl_GlobalInvocationID) * dbg_ShaderTrace.scale ..."
	TIntermBinary*		work_mul_scale	= new TIntermBinary{ TOperator::EOpMul };
	TIntermBinary*		scale			= dbgInfo.GetDebugStorageField( "scale" );
	CHECK_ERR( scale );
	work_mul_scale->setType( TType{type} );
	work_mul_scale->setLeft( to_vec2 );
	work_mul_scale->setRight( scale );

	// "... ivec2(vec2(gl_GlobalInvocationID) * dbg_ShaderTrace.scale) ..."
	TIntermUnary*		to_ivec2	= new TIntermUnary{ TOperator::EOpConvFloatToInt };
	type.basicType		= TBasicType::EbtInt;
	to_ivec2->setType( TType{type} );
	to_ivec2->setOperand( work_mul_scale );

	type.qualifier.storage	= TStorageQualifier::EvqConst;

	// "ivec2(0)"
	TConstUnionArray		zero_value(2);	zero_value[0].setIConst( 0 );  zero_value[1].setIConst( 0 );
	TIntermConstantUnion*	zero_ivec		= new TIntermConstantUnion{ zero_value, TType{type} };

	// "ivec2(1)"
	TConstUnionArray		one_value(2);	one_value[0].setIConst( 1 );  one_value[1].setIConst( 1 );
	TIntermConstantUnion*	one_ivec		= new TIntermConstantUnion{ one_value, TType{type} };

	type.qualifier.storage	= TStorageQualifier::EvqTemporary;

	// "... dimension - ivec2(1) ..."
	TIntermBinary*		dim_sub_one		= new TIntermBinary{ TOperator::EOpSub };
	TIntermBinary*		dimension		= dbgInfo.GetDebugStorageField( "dimension" );
	CHECK_ERR( dimension );
	dim_sub_one->setType( TType{type} );
	dim_sub_one->setLeft( dimension );
	dim_sub_one->setRight( one_ivec );

	// "... clamp( ivec2(vec2(gl_GlobalInvocationID) * dbg_ShaderTrace.scale), ivec2(0), dimension - ivec2(1) ) ..."
	TIntermAggregate*		clamp_op	= new TIntermAggregate{ TOperator::EOpClamp };
	type.vectorSize			= 2;
	type.qualifier.storage	= TStorageQualifier::EvqGlobal;
	clamp_op->setType( TType{type} );
	clamp_op->setOperationPrecision( TPrecisionQualifier::EpqHigh );
	clamp_op->getQualifierList().push_back( TStorageQualifier::EvqIn );
	clamp_op->getQualifierList().push_back( TStorageQualifier::EvqIn );
	clamp_op->getQualifierList().push_back( TStorageQualifier::EvqIn );
	clamp_op->getSequence().push_back( to_ivec2 );
	clamp_op->getSequence().push_back( zero_ivec );
	clamp_op->getSequence().push_back( dim_sub_one );

	// "ivec2  dbg_Coord = clamp( ivec2(vec2(gl_GlobalInvocationID) * dbg_ShaderTrace.scale), ivec2(0), dimension - ivec2(1) )"
	TIntermBinary*		assign_coord	= new TIntermBinary{ TOperator::EOpAssign };
	type.qualifier.storage = TStorageQualifier::EvqTemporary;
	assign_coord->setType( TType{type} );
	assign_coord->setLeft( coord );
	assign_coord->setRight( clamp_op );

	return assign_coord;
}

/*
=================================================
	GetRayTracingCoord
=================================================
*/
ND_ static TIntermBinary*  GetRayTracingCoord (TIntermSymbol* coord, DebugInfo &dbgInfo)
{
	TPublicType		type;		type.init( Default );
	type.basicType				= TBasicType::EbtFloat;
	type.vectorSize				= 2;
	type.qualifier.precision	= TPrecisionQualifier::EpqHigh;
	type.qualifier.storage		= TStorageQualifier::EvqTemporary;

	// "... vec2(gl_LaunchID) ..."
	TIntermSymbol*		launch_id	= dbgInfo.GetCachedSymbolNode( "gl_LaunchIDEXT" );
	TIntermUnary*		to_vec3		= new TIntermUnary{ TOperator::EOpConvUintToFloat };
	type.vectorSize = 3;
	to_vec3->setType( TType{type} );
	to_vec3->setOperand( launch_id );
	TIntermAggregate*	to_vec2		= new TIntermAggregate{ TOperator::EOpConstructVec2 };
	type.vectorSize = 2;
	to_vec2->setType( TType{type} );
	to_vec2->getSequence().push_back( to_vec3 );

	// "... ivec2(vec2(gl_LaunchID) * dbg_ShaderTrace.scale) ..."
	TIntermBinary*		launch_mul_scale	= new TIntermBinary{ TOperator::EOpMul };
	TIntermBinary*		scale				= dbgInfo.GetDebugStorageField( "scale" );
	CHECK_ERR( scale );
	launch_mul_scale->setType( TType{type} );
	launch_mul_scale->setLeft( to_vec2 );
	launch_mul_scale->setRight( scale );

	// "... ivec2(vec2(gl_LaunchID) * dbg_ShaderTrace.scale) ..."
	TIntermUnary*		to_ivec2	= new TIntermUnary{ TOperator::EOpConvFloatToInt };
	type.basicType		= TBasicType::EbtInt;
	to_ivec2->setType( TType{type} );
	to_ivec2->setOperand( launch_mul_scale );

	type.qualifier.storage	= TStorageQualifier::EvqConst;

	// "ivec2(0)"
	TConstUnionArray		zero_value(2);	zero_value[0].setIConst( 0 );  zero_value[1].setIConst( 0 );
	TIntermConstantUnion*	zero_ivec		= new TIntermConstantUnion{ zero_value, TType{type} };

	// "ivec2(1)"
	TConstUnionArray		one_value(2);	one_value[0].setIConst( 1 );  one_value[1].setIConst( 1 );
	TIntermConstantUnion*	one_ivec		= new TIntermConstantUnion{ one_value, TType{type} };

	type.qualifier.storage	= TStorageQualifier::EvqTemporary;

	// "... dimension - ivec2(1) ..."
	TIntermBinary*		dim_sub_one		= new TIntermBinary{ TOperator::EOpSub };
	TIntermBinary*		dimension		= dbgInfo.GetDebugStorageField( "dimension" );
	CHECK_ERR( dimension );
	dim_sub_one->setType( TType{type} );
	dim_sub_one->setLeft( dimension );
	dim_sub_one->setRight( one_ivec );

	// "... clamp( ivec2(vec2(gl_LaunchID) * dbg_ShaderTrace.scale), ivec2(0), dimension - ivec2(1) ) ..."
	TIntermAggregate*		clamp_op	= new TIntermAggregate{ TOperator::EOpClamp };
	type.vectorSize			= 2;
	type.qualifier.storage	= TStorageQualifier::EvqGlobal;
	clamp_op->setType( TType{type} );
	clamp_op->setOperationPrecision( TPrecisionQualifier::EpqHigh );
	clamp_op->getQualifierList().push_back( TStorageQualifier::EvqIn );
	clamp_op->getQualifierList().push_back( TStorageQualifier::EvqIn );
	clamp_op->getQualifierList().push_back( TStorageQualifier::EvqIn );
	clamp_op->getSequence().push_back( to_ivec2 );
	clamp_op->getSequence().push_back( zero_ivec );
	clamp_op->getSequence().push_back( dim_sub_one );

	// "ivec2  dbg_Coord = clamp( ivec2(vec2(gl_LaunchID) * dbg_ShaderTrace.scale), ivec2(0), dimension - ivec2(1) )"
	TIntermBinary*		assign_coord	= new TIntermBinary{ TOperator::EOpAssign };
	type.qualifier.storage = TStorageQualifier::EvqTemporary;
	assign_coord->setType( TType{type} );
	assign_coord->setLeft( coord );
	assign_coord->setRight( clamp_op );

	return assign_coord;
}

/*
=================================================
	InsertShaderTimeMeasurement
=================================================
*/
ND_ static bool  InsertShaderTimeMeasurementToEntry (TIntermAggregate* entry, DebugInfo &dbgInfo)
{
	TIntermSymbol*	start_time	= null;

	TPublicType		type;		type.init( Default );
	type.basicType				= TBasicType::EbtInt;
	type.vectorSize				= 2;
	type.qualifier.storage		= TStorageQualifier::EvqTemporary;
	type.qualifier.precision	= TPrecisionQualifier::EpqHigh;

	TPublicType		index_type;	 index_type.init( Default );
	index_type.basicType		 = TBasicType::EbtInt;
	index_type.qualifier.storage = TStorageQualifier::EvqConst;

	TConstUnionArray		x_index(1);	x_index[0].setIConst( 0 );
	TIntermConstantUnion*	x_field		= new TIntermConstantUnion{ x_index, TType{index_type} };

	TConstUnionArray		y_index(1);	y_index[0].setIConst( 1 );
	TIntermConstantUnion*	y_field		= new TIntermConstantUnion{ y_index, TType{index_type} };

	// "ivec2  dbg_Coord = ..."
	TIntermSymbol*	coord		= new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "dbg_Coord", TType{type} };
	TIntermBinary*	assign_coord = null;
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

			case EShLanguage::EShLangRayGen :
			case EShLanguage::EShLangIntersect :
			case EShLanguage::EShLangAnyHit :
			case EShLanguage::EShLangClosestHit :
			case EShLanguage::EShLangMiss :
			case EShLanguage::EShLangCallable :
				assign_coord = GetRayTracingCoord( coord, dbgInfo );
				break;

			case EShLanguage::EShLangTask :
			case EShLanguage::EShLangMesh :
				return false;	// not supported yet

			case EShLanguage::EShLangCount :
			default :
				return false;	// unknown
		}
		END_ENUM_CHECKS();
		CHECK_ERR( assign_coord );
	}

	type.basicType			= TBasicType::EbtUint;
	type.vectorSize			= 2;
	type.qualifier.storage	= TStorageQualifier::EvqConst;

	// for debugging
	//TConstUnionArray		zero_uvec2(2);	zero_uvec2[0].setUConst( 0 );  zero_uvec2[1].setUConst( 0 );
	//TIntermConstantUnion*	uvec2_const		= new TIntermConstantUnion{ zero_uvec2, TType{type} };

	// "uvec2  dbg_StartTime = clockRealtime2x32EXT();"
	{
		type.basicType			= TBasicType::EbtUint;
		type.vectorSize			= 2;
		type.qualifier.storage	= TStorageQualifier::EvqGlobal;

		TIntermAggregate*	time_call	= new TIntermAggregate( TOperator::EOpReadClockDeviceKHR );
		time_call->setType( TType{type} );

		type.qualifier.storage	= TStorageQualifier::EvqTemporary;
		start_time = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "dbg_StartTime", TType{type} };

		TIntermBinary*		assign_time	= new TIntermBinary{ TOperator::EOpAssign };
		assign_time->setType( TType{type} );
		assign_time->setLeft( start_time );
		assign_time->setRight( time_call );

		entry->getSequence().insert( entry->getSequence().begin(), assign_time );
	}
	{
		TIntermAggregate*	sequence	= new TIntermAggregate( TOperator::EOpSequence );
		TIntermSymbol*		end_time;
		{
			type.basicType			= TBasicType::EbtUint;
			type.vectorSize			= 2;
			type.qualifier.storage	= TStorageQualifier::EvqGlobal;

			// "... clockRealtime2x32EXT()"
			TIntermAggregate*	time_call	= new TIntermAggregate( TOperator::EOpReadClockDeviceKHR );
			time_call->setType( TType{type} );

			// "uvec2 dbg_EndTime"
			type.qualifier.storage	= TStorageQualifier::EvqTemporary;
			end_time = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "dbg_EndTime", TType{type} };

			// "uvec2 dbg_EndTime = clockRealtime2x32EXT();"
			TIntermBinary*		assign_time	= new TIntermBinary{ TOperator::EOpAssign };
			assign_time->setType( TType{type} );
			assign_time->setLeft( end_time );
			assign_time->setRight( time_call );

			sequence->getSequence().push_back( assign_time );
		}

		TIntermSymbol*	time_diff;
		{
			type.basicType			= TBasicType::EbtUint;
			type.vectorSize			= 1;
			type.qualifier.storage	= TStorageQualifier::EvqTemporary;

			// "dbg_EndTime.x"
			TIntermBinary*			endx_access	= new TIntermBinary{ TOperator::EOpIndexDirect };
			endx_access->setType( TType{type} );
			endx_access->setLeft( end_time );
			endx_access->setRight( x_field );

			// "dbg_EndTime.y"
			TIntermBinary*			endy_access	= new TIntermBinary{ TOperator::EOpIndexDirect };
			endy_access->setType( TType{type} );
			endy_access->setLeft( end_time );
			endy_access->setRight( y_field );

			// "dbg_StartTime.x"
			TIntermBinary*			startx_access	= new TIntermBinary{ TOperator::EOpIndexDirect };
			startx_access->setType( TType{type} );
			startx_access->setLeft( start_time );
			startx_access->setRight( x_field );

			// "dbg_StartTime.y"
			TIntermBinary*			starty_access	= new TIntermBinary{ TOperator::EOpIndexDirect };
			starty_access->setType( TType{type} );
			starty_access->setLeft( start_time );
			starty_access->setRight( y_field );

			type.basicType			= TBasicType::EbtDouble;
			type.vectorSize			= 1;
			type.qualifier.storage	= TStorageQualifier::EvqConst;

			// "double(0xFFFFFFFF)"
			TConstUnionArray		umax_value(1);	umax_value[0].setDConst( double(0xFFFFFFFF) );
			TIntermConstantUnion*	umax_const		= new TIntermConstantUnion{ umax_value, TType{type} };

			type.qualifier.storage	= TStorageQualifier::EvqTemporary;

			// "double(dbg_EndTime.x)"
			TIntermUnary*		endx_to_d		= new TIntermUnary{ TOperator::EOpConvUintToDouble };
			endx_to_d->setType( TType{type} );
			endx_to_d->setOperand( endx_access );

			// "double(dbg_EndTime.y)"
			TIntermUnary*		endy_to_d		= new TIntermUnary{ TOperator::EOpConvUintToDouble };
			endy_to_d->setType( TType{type} );
			endy_to_d->setOperand( endy_access );

			// "double(dbg_EndTime.y) * double(0xFFFFFFFF)"
			TIntermBinary*		endy_mul_umax	= new TIntermBinary{ TOperator::EOpMul };
			endy_mul_umax->setType( TType{type} );
			endy_mul_umax->setLeft( endy_to_d );
			endy_mul_umax->setRight( umax_const );

			// "(double(dbg_EndTime.y) * double(0xFFFFFFFF)) + double(dbg_EndTime.x)"
			TIntermBinary*		end_add_parts	= new TIntermBinary{ TOperator::EOpAdd };
			end_add_parts->setType( TType{type} );
			end_add_parts->setLeft( endy_mul_umax );
			end_add_parts->setRight( endx_to_d );

			// "double(dbg_StartTime.x)"
			TIntermUnary*		startx_to_d		= new TIntermUnary{ TOperator::EOpConvUintToDouble };
			startx_to_d->setType( TType{type} );
			startx_to_d->setOperand( startx_access );

			// "double(dbg_StartTime.y)"
			TIntermUnary*		starty_to_d		= new TIntermUnary{ TOperator::EOpConvUintToDouble };
			starty_to_d->setType( TType{type} );
			starty_to_d->setOperand( starty_access );

			// "double(dbg_StartTime.y) * double(0xFFFFFFFF)"
			TIntermBinary*		starty_mul_umax	= new TIntermBinary{ TOperator::EOpMul };
			starty_mul_umax->setType( TType{type} );
			starty_mul_umax->setLeft( starty_to_d );
			starty_mul_umax->setRight( umax_const );

			// "(double(dbg_StartTime.y) * double(0xFFFFFFFF)) + double(dbg_StartTime.y)"
			TIntermBinary*		start_add_parts	= new TIntermBinary{ TOperator::EOpAdd };
			start_add_parts->setType( TType{type} );
			start_add_parts->setLeft( starty_mul_umax );
			start_add_parts->setRight( startx_to_d );

			// "(...) - (...)"
			TIntermBinary*		end_minus_start	= new TIntermBinary{ TOperator::EOpSub };
			end_minus_start->setType( TType{type} );
			end_minus_start->setLeft( end_add_parts );
			end_minus_start->setRight( start_add_parts );

			// "float( ... )"
			type.basicType		= TBasicType::EbtFloat;
			TIntermUnary*		diff_to_float	= new TIntermUnary{ TOperator::EOpConvDoubleToFloat };
			diff_to_float->setType( TType{type} );
			diff_to_float->setOperand( end_minus_start );

			// "float dbg_Diff"
			time_diff = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "dbg_Diff", TType{type} };

			// "float dbg_Diff = float( ... );"
			TIntermBinary*		assign_diff	= new TIntermBinary{ TOperator::EOpAssign };
			assign_diff->setType( TType{type} );
			assign_diff->setLeft( time_diff );
			assign_diff->setRight( diff_to_float );

			sequence->getSequence().push_back( assign_diff );
		}
		sequence->getSequence().push_back( assign_coord );

		TIntermSymbol*	index;
		{
			type.basicType			= TBasicType::EbtInt;
			type.vectorSize			= 1;
			type.qualifier.storage	= TStorageQualifier::EvqTemporary;

			// "dbg_Coord.x"
			TIntermBinary*		coord_x			= new TIntermBinary{ TOperator::EOpIndexDirect };
			type.vectorSize = 1;
			coord_x->setType( TType{type} );
			coord_x->setLeft( coord );
			coord_x->setRight( x_field );

			// "dbg_Coord.y"
			TIntermBinary*		coord_y			= new TIntermBinary{ TOperator::EOpIndexDirect };
			coord_y->setType( TType{type} );
			coord_y->setLeft( coord );
			coord_y->setRight( y_field );

			// "dbg_ShaderTrace.dimension.x"
			TIntermBinary*		dimension		= dbgInfo.GetDebugStorageField( "dimension" );
			TIntermBinary*		dimension_x		= new TIntermBinary{ TOperator::EOpIndexDirect };
			CHECK_ERR( dimension );
			dimension_x->setType( TType{type} );
			dimension_x->setLeft( dimension );
			dimension_x->setRight( x_field );

			// "... dbg_Coord.y * dbg_ShaderTrace.dimension.x"
			TIntermBinary*		cy_mul_width	= new TIntermBinary{ TOperator::EOpMul };
			cy_mul_width->setType( TType{type} );
			cy_mul_width->setLeft( coord_y );
			cy_mul_width->setRight( dimension_x );

			// "... dbg_Coord.x + dbg_Coord.y * dbg_ShaderTrace.dimension.x"
			TIntermBinary*		calc_index		= new TIntermBinary{ TOperator::EOpAdd };
			calc_index->setType( TType{type} );
			calc_index->setLeft( coord_x );
			calc_index->setRight( cy_mul_width );

			// "int  dbg_Index = dbg_Coord.x + dbg_Coord.y * dbg_ShaderTrace.dimension.x"
								index			= new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "dbg_Index", TType{type} };
			TIntermBinary*		assign_index	= new TIntermBinary{ TOperator::EOpAssign };
			assign_index->setType( TType{type} );
			assign_index->setLeft( index );
			assign_index->setRight( calc_index );

			sequence->getSequence().push_back( assign_index );
		}
		{
			type.basicType			= TBasicType::EbtUint;
			type.vectorSize			= 1;
			type.qualifier.storage	= TStorageQualifier::EvqConst;

			// "uint dbg_OldValue = 0;"
			TConstUnionArray		zero_val(1);	zero_val[0].setUConst( 0 );
			TIntermConstantUnion*	zero_const		= new TIntermConstantUnion{ zero_val, TType{type} };

			type.qualifier.storage	= TStorageQualifier::EvqTemporary;

			TIntermSymbol*			old_value		= new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "dbg_OldValue", TType{type} };
			TIntermBinary*			init_old_val	= new TIntermBinary{ TOperator::EOpAssign };
			init_old_val->setType( TType{type} );
			init_old_val->setLeft( old_value );
			init_old_val->setRight( zero_const );

			// new line

			// "uintBitsToFloat( dbg_OldValue )"
			TIntermUnary*		old_to_f		= new TIntermUnary{ TOperator::EOpUintBitsToFloat };
			type.basicType = TBasicType::EbtFloat;
			old_to_f->setType( TType{type} );
			old_to_f->setOperand( old_value );

			// "uintBitsToFloat( dbg_OldValue ) + dbg_Diff"
			TIntermBinary*		oldf_add_diff	= new TIntermBinary{ TOperator::EOpAdd };
			oldf_add_diff->setType( TType{type} );
			oldf_add_diff->setLeft( old_to_f );
			oldf_add_diff->setRight( time_diff );

			// "floatBitsToUint( uintBitsToFloat( dbg_OldValue ) + dbg_Diff )"
			TIntermUnary*		old_diff_to_u	= new TIntermUnary{ TOperator::EOpFloatBitsToUint };
			type.basicType = TBasicType::EbtUint;
			old_diff_to_u->setType( TType{type} );
			old_diff_to_u->setOperand( oldf_add_diff );

			// "uint dbg_NewValue = floatBitsToUint( uintBitsToFloat( dbg_OldValue ) + dbg_Diff );"
			TIntermSymbol*		new_value		= new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "dbg_NewValue", TType{type} };
			TIntermBinary*		assign_new_val	= new TIntermBinary{ TOperator::EOpAssign };
			assign_new_val->setType( TType{type} );
			assign_new_val->setLeft( new_value );
			assign_new_val->setRight( old_diff_to_u );

			// new line

			// "dbg_Expected  = dbg_OldValue;"
			TIntermSymbol*		expected_val	= new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "dbg_Expected", TType{type} };
			TIntermBinary*		assign_exp_val	= new TIntermBinary{ TOperator::EOpAssign };
			assign_exp_val->setType( TType{type} );
			assign_exp_val->setLeft( expected_val );
			assign_exp_val->setRight( old_value );

			// new line

			// "dbg_ShaderTrace.outPixels[dbg_Index]"
			TIntermBinary*		out_pixel		= dbgInfo.GetDebugStorageField( "outPixels" );
			TIntermBinary*		curr_pixel		= new TIntermBinary{ TOperator::EOpIndexIndirect };
			CHECK_ERR( out_pixel );
			curr_pixel->setType( TType{type} );
			curr_pixel->getWritableType().setFieldName( "outPixels" );
			curr_pixel->setLeft( out_pixel );
			curr_pixel->setRight( index );

			// "atomicCompSwap( dbg_ShaderTrace.outPixels[dbg_Index], dbg_Expected, dbg_NewValue )"
			type.qualifier.storage	= TStorageQualifier::EvqGlobal;
			TIntermAggregate*	atomic_cs		= new TIntermAggregate( TOperator::EOpAtomicCompSwap );
			atomic_cs->setType( TType{type} );
			atomic_cs->getSequence().push_back( curr_pixel );
			atomic_cs->getSequence().push_back( expected_val );
			atomic_cs->getSequence().push_back( new_value );
			atomic_cs->getQualifierList().push_back( TStorageQualifier::EvqInOut );
			atomic_cs->getQualifierList().push_back( TStorageQualifier::EvqIn );
			atomic_cs->getQualifierList().push_back( TStorageQualifier::EvqIn );

			type.qualifier.storage	= TStorageQualifier::EvqTemporary;

			// "dbg_OldValue = atomicCompSwap( dbg_ShaderTrace.outPixels[dbg_Index], dbg_Expected, dbg_NewValue );"
			TIntermBinary*		assign_old_val	= new TIntermBinary{ TOperator::EOpAssign };
			assign_old_val->setType( TType{type} );
			assign_old_val->setLeft( old_value );
			assign_old_val->setRight( atomic_cs );

			// new line

			// "while( dbg_OldValue != dbg_Expected )"
			TIntermBinary*		not_eqv			= new TIntermBinary{ TOperator::EOpNotEqual };
			type.basicType = TBasicType::EbtBool;
			not_eqv->setType( TType{type} );
			not_eqv->setLeft( old_value );
			not_eqv->setRight( expected_val );

			// body
			TIntermAggregate*	loop_body		= new TIntermAggregate( TOperator::EOpSequence );
			loop_body->getSequence().push_back( assign_new_val );
			loop_body->getSequence().push_back( assign_exp_val );
			loop_body->getSequence().push_back( assign_old_val );

			TIntermLoop*		loop = new TIntermLoop{ loop_body, not_eqv, null, false };

			sequence->getSequence().push_back( init_old_val );
			sequence->getSequence().push_back( loop );
		}
		entry->getSequence().push_back( sequence );
	}
	return true;
}

/*
=================================================
	InsertShaderTimeMeasurement
=================================================
*/
ND_ static bool  InsertShaderTimeMeasurement (TIntermNode* root, DebugInfo &dbgInfo)
{
	TIntermAggregate*	aggr = root->getAsAggregate();
	CHECK_ERR( aggr );

	// find shader resources entry
	TIntermAggregate*	linker_objs	= null;

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

} // namespace
//-----------------------------------------------------------------------------



/*
=================================================
	InsertShaderClockHeatmap
=================================================
*/
	bool  ShaderTrace::InsertShaderClockHeatmap (glslang::TIntermediate &intermediate, uint descSetIndex)
	{
		intermediate.addRequestedExtension( "GL_EXT_shader_realtime_clock" );
		intermediate.addRequestedExtension( "GL_ARB_gpu_shader_fp64" );

		DebugInfo		dbg_info{ intermediate };

		TIntermNode*	root = intermediate.getTreeRoot();
		CHECK_ERR( root );

		_posOffset = ~0ull;
		CreateShaderDebugStorage( descSetIndex, dbg_info, OUT _dataOffset );

		CHECK_ERR( RecursiveProcessNode( root, dbg_info ));

		CreateShaderBuiltinSymbols( root, dbg_info );

		CHECK_ERR( InsertShaderTimeMeasurement( root, dbg_info ));

		CHECK_ERR( ValidateInterm( intermediate ));
		return true;
	}

} // AE::PipelineCompiler
