// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#include "Common.h"

using namespace glslang;
using VariableID = ShaderTrace::VariableID;

namespace {

	//
	// Debug Info
	//
	struct DebugInfo
	{
	public:
		using SrcLoc		= ShaderTrace::SourceLocation;
		using SrcPoint		= ShaderTrace::SourcePoint;
		using ExprInfos_t	= ShaderTrace::ExprInfos_t;
		using VarNames_t	= ShaderTrace::VarNames_t;

		struct StackFrame
		{
			TIntermNode*		node;
			SrcLoc				loc;
		};

		struct VariableInfo
		{
			VariableID			id;
			string				name;
			vector<TSourceLoc>	locations;
		};

		struct FnCallLocation
		{
			string			fnName;
			TSourceLoc		loc;

			ND_ bool operator == (const FnCallLocation &) const;
		};

		struct FnCallLocationHash {
			ND_ size_t operator () (const FnCallLocation &) const;
		};

		struct FieldInfo
		{
			int		baseId		= 0;
			int		fieldIndex	= 0;

			ND_ bool operator == (const FieldInfo &) const;
		};

		struct FieldInfoHash {
			ND_ size_t operator () (const FieldInfo &) const;
		};

		using SymbolLocations_t		= unordered_map< int, vector<TIntermSymbol *> >;
		using RequiredFunctions_t	= unordered_set< TString >;
		using CachedSymbols_t		= unordered_map< TString, TIntermSymbol *>;
		using VariableInfoMap_t		= unordered_map< int, VariableInfo >;
		using FnCallMap_t			= unordered_map< FnCallLocation, VariableInfo, FnCallLocationHash >;
		using StructFieldMap_t		= unordered_map< FieldInfo, VariableInfo, FieldInfoHash >;
		using CallStack_t			= vector< StackFrame >;
		using FileMap_t				= unordered_map< string, uint >;


	public:
		CallStack_t				_callStack;
		TIntermTyped *			_injection			= nullptr;

		RequiredFunctions_t		_requiredFunctions;
		CachedSymbols_t			_cachedSymbols;

		ExprInfos_t &			_exprLocations;
		VariableInfoMap_t		_varInfos;
		FnCallMap_t				_fnCallMap;
		StructFieldMap_t		_fieldMap;

		int						_maxSymbolId		= 0;

		TIntermSymbol *			_dbgStorage			= nullptr;

		FileMap_t const&		_includedFilesMap;

		const string			_entryName;

		const EShLanguage		_shLang;
		bool					_isOpenGL			= false;

		const bool				_hasNVRayTracingExt	= false;	// switch between VN and KHR extensions


	public:
		DebugInfo (const TIntermediate &intermediate, OUT ExprInfos_t &exprLoc, const FileMap_t &includedFiles) :
			_exprLocations{ exprLoc },
			_includedFilesMap{ includedFiles },
			_entryName{ intermediate.getEntryPointMangledName() },
			_shLang{ intermediate.getStage() },
			_hasNVRayTracingExt{ intermediate.getRequestedExtensions().find("GL_NV_ray_tracing") != intermediate.getRequestedExtensions().end() }
		{}

		void Enter (TIntermNode* node);
		void Leave (TIntermNode* node);

		ND_ CallStack_t const&	GetCallStack ()			const					{ return _callStack; }
		ND_ TIntermAggregate*	GetCurrentFunction ();

		ND_ TIntermTyped*		GetInjection ();
		void					InjectNode (TIntermTyped *node);

		ND_ uint				GetSourceLocation (TIntermNode* node, const TSourceLoc &curr);
		ND_ uint				GetCustomSourceLocation (TIntermNode* node, const TSourceLoc &curr);

		ND_ SrcLoc const&		GetCurrentLocation () const						{ return _callStack.back().loc; }
		void					AddLocation (const TSourceLoc &loc);
		void					AddLocation (const SrcLoc &src);

		void							RequestFunc (const TString &fname)		{ _requiredFunctions.insert( fname ); }
		ND_ RequiredFunctions_t const&	GetRequiredFunctions ()		const		{ return _requiredFunctions; }

		void					AddSymbol (TIntermSymbol* node);
		ND_ int					GetUniqueSymbolID ()							{ return ++_maxSymbolId; }

		ND_ string const&		GetEntryPoint ()						const	{ return _entryName; }


		void CacheSymbolNode (TIntermSymbol* node)
		{
			_cachedSymbols.insert({ node->getName(), node });
			AddSymbol( node );
		}

		ND_ TIntermSymbol*  GetCachedSymbolNode (const TString &name)
		{
			auto	iter = _cachedSymbols.find( name );
			return	iter != _cachedSymbols.end() ? iter->second : nullptr;
		}


		ND_ TIntermSymbol*	GetDebugStorage ()						const	{ CHECK( _dbgStorage );  return _dbgStorage; }
			bool			SetDebugStorage (TIntermSymbol* symb);
		ND_ TIntermBinary*  GetDebugStorageField (const char* name)	const;
	

			bool			PostProcess (OUT VarNames_t &);

		ND_ EShLanguage		GetShaderType ()						const	{ return _shLang; }
		ND_ uint			IsNVRT ()								const	{ return uint(_hasNVRayTracingExt); }


	private:
		void		_GetVariableID (TIntermNode* node, OUT VariableID &id, OUT uint &swizzle);
		ND_ uint	_GetSourceId (const TSourceLoc &) const;
	};
//-----------------------------------------------------------------------------



static bool RecursiveProcessNode (TIntermNode* node, DebugInfo &dbgInfo);
static void CreateShaderDebugStorage (uint descSetIndex, DebugInfo &dbgInfo, OUT uint64_t &posOffset, OUT uint64_t &dataOffset);
static void CreateShaderBuiltinSymbols (TIntermNode* root, DebugInfo &dbgInfo);
static bool CreateDebugTraceFunctions (TIntermNode* root, uint initialPosition, DebugInfo &dbgInfo);
static TIntermAggregate*  RecordShaderInfo (const TSourceLoc &loc, DebugInfo &dbgInfo);

/*
=================================================
	CombineHash
=================================================
*/
ND_ inline size_t CombineHash (size_t lhs, size_t rhs)
{
	const size_t	mask	= (sizeof(lhs)*8 - 1);
	size_t			shift	= 4;

	shift &= mask;
	return lhs ^ ((rhs << shift) | (rhs >> ( ~(shift-1) & mask )));
}

/*
=================================================
	Enter
=================================================
*/
void DebugInfo::Enter (TIntermNode* node)
{
	CHECK( node );

	StackFrame	frame;
	frame.node			= node;
	frame.loc.sourceId	= _GetSourceId( node->getLoc() );
	frame.loc.begin		= SrcPoint{ uint(node->getLoc().line), uint(node->getLoc().column) };
	frame.loc.end		= frame.loc.begin;

	_callStack.push_back( frame );
}

/*
=================================================
	Leave
=================================================
*/
void DebugInfo::Leave (TIntermNode* node)
{
	CHECK( _callStack.back().node == node );
	_callStack.pop_back();
}

/*
=================================================
	GetCurrentFunction
=================================================
*/
TIntermAggregate*  DebugInfo::GetCurrentFunction ()
{
	for (auto iter = _callStack.rbegin(), end = _callStack.rend(); iter != end; ++iter)
	{
		TIntermAggregate*	aggr = iter->node->getAsAggregate();

		if ( aggr and aggr->getOp() == TOperator::EOpFunction )
			return aggr;
	}
	return nullptr;
}

/*
=================================================
	GetInjection
=================================================
*/
TIntermTyped*  DebugInfo::GetInjection ()
{
	auto	temp = _injection;
	_injection = nullptr;
	return temp;
}

/*
=================================================
	InjectNode
=================================================
*/
void DebugInfo::InjectNode (TIntermTyped *node)
{
	if ( not node )
		return;

	CHECK( not _injection );
	_injection = node;
}

/*
=================================================
	GetSourceLocation
=================================================
*/
uint  DebugInfo::GetSourceLocation (TIntermNode* node, const TSourceLoc &curr)
{
	VariableID	id;
	uint		swizzle;
	_GetVariableID( node, OUT id, OUT swizzle );

	SrcPoint	point	{ uint(curr.line), uint(curr.column) };
	SrcLoc		range	= _callStack.back().loc;
	uint		src_id	= _GetSourceId( curr );

	ASSERT( range.sourceId != ~0u );
	ASSERT( range.sourceId == src_id );
	
	range.begin.value = Min( range.begin.value,	point.value );
	range.end.value   = Max( range.end.value,   point.value );

	_exprLocations.push_back({ id, swizzle, range, point, {} });
	return uint(_exprLocations.size()-1);
}

/*
=================================================
	GetCustomSourceLocation
=================================================
*/
uint  DebugInfo::GetCustomSourceLocation (TIntermNode* node, const TSourceLoc &curr)
{
	VariableID	id;
	uint		swizzle;
	_GetVariableID( node, OUT id, OUT swizzle );

	SrcLoc	range{ 0, uint(curr.line), uint(curr.column) };
	range.sourceId = _GetSourceId( curr );

	_exprLocations.push_back({ id, swizzle, range, range.begin, {} });
	return uint(_exprLocations.size()-1);
}

/*
=================================================
	AddLocation
=================================================
*/
void DebugInfo::AddLocation (const TSourceLoc &loc)
{
	return AddLocation({ _GetSourceId( loc ), uint(loc.line), uint(loc.column) });
}

void DebugInfo::AddLocation (const SrcLoc &src)
{
	if ( _callStack.empty() )
		return;

	auto&	dst = _callStack.back().loc;

	if ( dst.sourceId == 0 and dst.begin.value == 0 and dst.end.value == 0 )
	{
		dst = src;
		return;
	}

	CHECK( src.sourceId == dst.sourceId );
	dst.begin.value = Min( dst.begin.value,	src.begin.value );
	dst.end.value	= Max( dst.end.value,	src.end.value );
}

/*
=================================================
	SetDebugStorage
=================================================
*/
bool DebugInfo::SetDebugStorage (TIntermSymbol* symb)
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
	
/*
=================================================
	AddSymbol
=================================================
*/
void DebugInfo::AddSymbol (TIntermSymbol* node)
{
	ASSERT( node );
	_maxSymbolId = Max( _maxSymbolId, node->getId() );

	// register symbol
	VariableID	id;
	uint		sw;
	_GetVariableID( node, OUT id, OUT sw );
}

/*
=================================================
	TSourceLoc::operator ==
=================================================
*/
ND_ inline bool operator == (const TSourceLoc &lhs, const TSourceLoc &rhs)
{
	if ( lhs.name != rhs.name )
	{
		if ( lhs.name == nullptr  or
			 rhs.name == nullptr  or
			*lhs.name != *rhs.name )
			return false;
	}

	return	lhs.string	== rhs.string	and
			lhs.line	== rhs.line		and
			lhs.column	== rhs.column;
}

ND_ inline bool operator != (const TSourceLoc &lhs, const TSourceLoc &rhs)
{
	return not (lhs == rhs);
}

ND_ inline bool operator < (const TSourceLoc &lhs, const TSourceLoc &rhs)
{
	if ( lhs.name != rhs.name )
	{
		if ( lhs.name == nullptr  or
			 rhs.name == nullptr )
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
	IsBuiltinFunction
=================================================
*/
ND_ inline bool IsBuiltinFunction (TOperator op)
{
#if 0 //def HIGH_DETAIL_TRACE
	if ( op > TOperator::EOpPreDecrement and op < TOperator::EOpAdd )
		return true;
#endif

	return	(op > TOperator::EOpRadians			and op < TOperator::EOpKill)	or
			(op > TOperator::EOpArrayLength		and op < TOperator::EOpClip);
}

/*
=================================================
	IsDebugFunction
=================================================
*/
ND_ inline bool IsDebugFunction (TIntermOperator* node)
{
	auto*	aggr = node->getAsAggregate();

	if ( not (aggr and aggr->getOp() == TOperator::EOpFunctionCall) )
		return false;

	return aggr->getName().rfind( "dbg_EnableTraceRecording", 0 ) == 0;
}

/*
=================================================
	FnCallLocation::operator ==
=================================================
*/
inline bool  DebugInfo::FnCallLocation::operator == (const FnCallLocation &rhs) const
{
	return loc == rhs.loc and fnName == rhs.fnName;
}

/*
=================================================
	FnCallLocationHash::operator ()
=================================================
*/
inline size_t  DebugInfo::FnCallLocationHash::operator () (const FnCallLocation &value) const
{
	size_t	fn_hash		= std::hash<string>{}( value.fnName );
	size_t	name_hash	= value.loc.name ? std::hash<TString>{}( *value.loc.name ) : 0;

	return	CombineHash( CombineHash( fn_hash, name_hash ),
				CombineHash( CombineHash( size_t(value.loc.string) << 24, size_t(value.loc.line) << 8 ),
							 size_t(value.loc.column) << 18 ));
}

/*
=================================================
	FieldInfo::operator ==
=================================================
*/
inline bool  DebugInfo::FieldInfo::operator == (const FieldInfo &rhs) const
{
	return	baseId		== rhs.baseId	and
			fieldIndex	== rhs.fieldIndex;
}

/*
=================================================
	FieldInfoHash::operator ()
=================================================
*/
inline size_t  DebugInfo::FieldInfoHash::operator () (const FieldInfo &value) const
{
	return	size_t(value.baseId) ^
			(size_t(value.fieldIndex) << (sizeof(size_t)*8-16));
}

/*
=================================================
	GetVectorSwizzleMask
=================================================
*/
ND_ static uint  GetVectorSwizzleMask (TIntermBinary* binary)
{
	vector<uint>			sw_mask;
	vector<TIntermBinary*>	swizzle_op;		swizzle_op.push_back( binary );

	CHECK_ERR( binary and (binary->getOp() == TOperator::EOpVectorSwizzle or binary->getOp() == TOperator::EOpIndexDirect) );

	// extract swizzle mask
	for (TIntermTyped* node = binary->getLeft();
		 node->getAsBinaryNode() and node->getAsBinaryNode()->getOp() == TOperator::EOpVectorSwizzle;)
	{
		swizzle_op.push_back( node->getAsBinaryNode() );

		node = swizzle_op.back()->getLeft();
	}

	binary = swizzle_op.back();

	const auto ProcessUnion = [&sw_mask] (TIntermConstantUnion *cu, const vector<uint> &mask) -> bool
	{
		TConstUnionArray const&	cu_arr = cu->getConstArray();
		CHECK_ERR( cu_arr.size() == 1 and cu->getType().getBasicType() == EbtInt );
		CHECK_ERR( cu_arr[0].getType() == EbtInt and cu_arr[0].getIConst() >= 0 and cu_arr[0].getIConst() < 4 );

		if ( mask.empty() )
			sw_mask.push_back( cu_arr[0].getIConst() );
		else
			sw_mask.push_back( mask[ cu_arr[0].getIConst() ]);
		return true;
	};

	// optimize swizzle
	for (auto iter = swizzle_op.rbegin(); iter != swizzle_op.rend(); ++iter)
	{
		TIntermBinary*		bin		= (*iter);
		const vector<uint>	mask	= sw_mask;

		sw_mask.clear();

		if ( TIntermAggregate* aggr = bin->getRight()->getAsAggregate() )
		{
			CHECK_ERR( aggr->getOp() == TOperator::EOpSequence );

			for (auto& node : aggr->getSequence())
			{
				if ( auto* cu = node->getAsConstantUnion() )
					CHECK_ERR( ProcessUnion( cu, mask ));
			}
		}
		else
		if ( auto* cu = bin->getRight()->getAsConstantUnion() )
		{
			CHECK_ERR( ProcessUnion( cu, mask ));
		}
		else
			RETURN_ERR( "not supported!" );
	}

	uint	result	= 0;
	uint	shift	= 0;
	for (auto& idx : sw_mask)
	{
		result |= ((idx + 1) << shift);
		shift  += 3;
	}
	return result;
}

/*
=================================================
	_GetVariableID
=================================================
*/
void DebugInfo::_GetVariableID (TIntermNode* node, OUT VariableID &id, OUT uint &swizzle)
{
	if ( not node )
	{
		CHECK( node );
		return;
	}

	id = VariableID(~0u);
	swizzle = 0;

	const VariableID	new_id = VariableID(uint(_varInfos.size() + _fnCallMap.size() + _fieldMap.size()));

	if ( TIntermSymbol* symb = node->getAsSymbolNode() )
	{
		auto	iter = _varInfos.find( symb->getId() );
		if ( iter == _varInfos.end() )
			iter = _varInfos.insert({ symb->getId(), VariableInfo{new_id, symb->getName().c_str(), {}} }).first;

		auto&	locations = iter->second.locations;

		if ( locations.empty() or locations.back() != node->getLoc() )
			locations.push_back( node->getLoc() );

		id = iter->second.id;
		return;
	}

	if ( TIntermBinary* binary = node->getAsBinaryNode() )
	{
		// vector swizzle
		if ( binary->getOp() == TOperator::EOpVectorSwizzle or
			(binary->getOp() == TOperator::EOpIndexDirect and not binary->getLeft()->isArray() and
			(binary->getLeft()->isScalar() or binary->getLeft()->isVector())) )
		{
			swizzle = GetVectorSwizzleMask( binary );

			uint	temp;
			return _GetVariableID( binary->getLeft(), OUT id, OUT temp );
		}
		else
		// matrix swizzle
		if ( binary->getOp() == TOperator::EOpIndexDirect and
			 binary->getLeft()->isMatrix() )
		{
			swizzle = GetVectorSwizzleMask( binary );

			uint	temp;
			return _GetVariableID( binary->getLeft(), OUT id, OUT temp );
		}
		else
		// array element
		if ( binary->getOp() == TOperator::EOpIndexDirect and
			 binary->getLeft()->isArray() )
		{
			swizzle = 0;	// TODO

			uint	temp;
			return _GetVariableID( binary->getLeft(), OUT id, OUT temp );
		}
		else
		if ( binary->getOp() == TOperator::EOpIndexIndirect )
		{}
		else
		if ( binary->getOp() == TOperator::EOpIndexDirectStruct )
		{
			// TODO
			/*FieldInfo	field;
			TString		name;
			if ( GetStructFieldInfo( binary, OUT field, OUT name ))
			{
				auto	iter = _fieldMap.find( field );
				if ( iter == _fieldMap.end() )
					iter = _fieldMap.insert({ field, VariableInfo{new_id, name.c_str(), {}} }).first;

				auto&	locations = iter->second.locations;

				if ( locations.empty() or locations.back() != node->getLoc() )
					locations.push_back( node->getLoc() );

				id = iter->second.id;
				return;
			}*/
			return; // temp
		}
		else
			return;	// it hasn't any ID
	}

	if ( TIntermOperator* op = node->getAsOperator() )
	{
		// temporary variable returned by function
		if ( IsBuiltinFunction( op->getOp() ) or op->getOp() == TOperator::EOpFunctionCall )
		{
			auto	name = GetFunctionName( op ) + "()";
			auto	iter = _fnCallMap.find({ name, op->getLoc() });

			if ( iter == _fnCallMap.end() )
				iter = _fnCallMap.insert({ FnCallLocation{name, op->getLoc()}, VariableInfo{new_id, name, {}} }).first;
			
			auto&	locations = iter->second.locations;

			if ( locations.empty() or locations.back() != op->getLoc() )
				locations.push_back( node->getLoc() );
			
			id = iter->second.id;
			return;
		}
		return;	// it hasn't any ID
	}

	if ( node->getAsBranchNode() or node->getAsSelectionNode() )
		return;	// no ID for branches

	CHECK(false);
}

/*
=================================================
	_GetSourceId
=================================================
*/
uint DebugInfo::_GetSourceId (const TSourceLoc &loc) const
{
	if ( loc.name )
	{
		auto	iter = _includedFilesMap.find( loc.name->c_str() );
		CHECK( iter != _includedFilesMap.end() );
		CHECK( loc.string == 0 );

		return iter->second;
	}
	else
		return loc.string;
}

/*
=================================================
	PostProcess
=================================================
*/
bool  DebugInfo::PostProcess (OUT VarNames_t &varNames)
{
	const auto	SearchInExpressions = [&] (VariableInfo &info)
	{
		std::sort(	info.locations.begin(), info.locations.end(),
					[] (auto& lhs, auto& rhs) { return lhs < rhs; });

		bool	is_unused = true;
		
		for (auto& expr : _exprLocations)
		{
			if ( expr.varID == info.id ) {
				is_unused = false;
				continue;
			}

			for (auto& loc : info.locations)
			{
				uint	loc_line	= uint(loc.line);
				uint	loc_column	= uint(loc.column);
				uint	source_id	= _GetSourceId( loc );

				if ( source_id == 0 and loc_line == 0 and loc_column == 0 )
					continue;	// skip undefined location

				// check intersection
				if ( source_id != expr.range.sourceId )
					continue;

				if ( loc_line < expr.range.begin.Line() or loc_line > expr.range.end.Line() )
					continue;

				if ( loc_line == expr.range.begin.Line() and loc_column < expr.range.begin.Column() )
					continue;

				if ( loc_line == expr.range.end.Line() and loc_column > expr.range.end.Column() )
					continue;

				bool	exist = false;
				for (auto& v : expr.vars) {
					exist |= (v == info.id);
				}

				if ( not exist )
					expr.vars.push_back( info.id );

				is_unused = false;
			}
		}

		if ( not is_unused )
			varNames.insert_or_assign( info.id, std::move(info.name) );
	};

	for (auto& info : _varInfos) {
		SearchInExpressions( info.second );
	}
	for (auto& info : _fnCallMap) {
		SearchInExpressions( info.second );
	}
	for (auto& info : _fieldMap) {
		SearchInExpressions( info.second );
	}
	return true;
}
//-----------------------------------------------------------------------------



static bool RecursiveProcessAggregateNode (TIntermAggregate* node, DebugInfo &dbgInfo);
static bool RecursiveProcessBranchNode (TIntermBranch* node, DebugInfo &dbgInfo);
static bool RecursiveProcessSwitchNode (TIntermSwitch* node, DebugInfo &dbgInfo);
static bool RecursiveProcessSelectionNode (TIntermSelection* node, DebugInfo &dbgInfo);
static bool ProcessSymbolNode (TIntermSymbol* node, DebugInfo &dbgInfo);
static bool RecursiveProcessUnaryNode (TIntermUnary* node, DebugInfo &dbgInfo);
static bool RecursiveProcessBinaryNode (TIntermBinary* node, DebugInfo &dbgInfo);
static bool RecursiveProccessLoop (TIntermLoop* node, DebugInfo &dbgInfo);
ND_ static TIntermAggregate*  CreateAppendToTrace (TIntermTyped* exprNode, uint sourceLoc, DebugInfo &dbgInfo);

/*
=================================================
	RecursiveProcessNode
=================================================
*/
static bool RecursiveProcessNode (TIntermNode* node, DebugInfo &dbgInfo)
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
		CHECK_ERR( RecursiveProcessUnaryNode( unary, dbgInfo ));
		return true;
	}

	if ( auto* binary = node->getAsBinaryNode() )
	{
		CHECK_ERR( RecursiveProcessBinaryNode( binary, dbgInfo ));
		return true;
	}

	if ( auto* op = node->getAsOperator() )
	{
		return false;
	}

	if ( auto* branch = node->getAsBranchNode() )
	{
		CHECK_ERR( RecursiveProcessBranchNode( branch, dbgInfo ));
		return true;
	}

	if ( auto* sw = node->getAsSwitchNode() )
	{
		CHECK_ERR( RecursiveProcessSwitchNode( sw, dbgInfo ));
		return true;
	}

	if ( auto* cunion = node->getAsConstantUnion() )
	{
		dbgInfo.AddLocation( node->getLoc() );
		return true;
	}

	if ( auto* selection = node->getAsSelectionNode() )
	{
		CHECK_ERR( RecursiveProcessSelectionNode( selection, dbgInfo ));
		return true;
	}

	if ( auto* method = node->getAsMethodNode() )
	{
		return true;
	}

	if ( auto* symbol = node->getAsSymbolNode() )
	{
		dbgInfo.AddLocation( node->getLoc() );
		CHECK_ERR( ProcessSymbolNode( symbol, dbgInfo ));
		return true;
	}

	if ( auto* typed = node->getAsTyped() )
	{
		dbgInfo.AddLocation( node->getLoc() );
		return true;
	}

	if ( auto* loop = node->getAsLoopNode() )
	{
		CHECK_ERR( RecursiveProccessLoop( loop, dbgInfo ));
		return true;
	}

	return false;
}

/*
=================================================
	CreateGraphicsShaderDebugStorage
=================================================
*/
static void CreateGraphicsShaderDebugStorage (TTypeList* typeList, INOUT TPublicType &type)
{
	type.basicType = TBasicType::EbtInt;

	TType*	fragcoord_x	= new TType{type};		fragcoord_x->setFieldName( "fragCoordX" );
	type.qualifier.layoutOffset += sizeof(int32_t);

	TType*	fragcoord_y	= new TType{type};		fragcoord_y->setFieldName( "fragCoordY" );
	type.qualifier.layoutOffset += sizeof(int32_t);
	
	TType*	padding	= new TType{type};			padding->setFieldName( "padding1" );
	type.qualifier.layoutOffset += sizeof(int32_t);

	typeList->push_back({ fragcoord_x,	TSourceLoc{} });
	typeList->push_back({ fragcoord_y,	TSourceLoc{} });
	typeList->push_back({ padding,		TSourceLoc{} });
}

/*
=================================================
	CreateComputeShaderDebugStorage
=================================================
*/
static void CreateComputeShaderDebugStorage (TTypeList* typeList, INOUT TPublicType &type)
{
	type.basicType = TBasicType::EbtUint;
	
	TType*	thread_id_x	= new TType{type};		thread_id_x->setFieldName( "globalInvocationX" );
	type.qualifier.layoutOffset += sizeof(uint);
	
	TType*	thread_id_y	= new TType{type};		thread_id_y->setFieldName( "globalInvocationY" );
	type.qualifier.layoutOffset += sizeof(uint);
	
	TType*	thread_id_z	= new TType{type};		thread_id_z->setFieldName( "globalInvocationZ" );
	type.qualifier.layoutOffset += sizeof(uint);
	
	typeList->push_back({ thread_id_x,	TSourceLoc{} });
	typeList->push_back({ thread_id_y,	TSourceLoc{} });
	typeList->push_back({ thread_id_z,	TSourceLoc{} });
}

/*
=================================================
	CreateRayTracingShaderDebugStorage
=================================================
*/
static void CreateRayTracingShaderDebugStorage (TTypeList* typeList, INOUT TPublicType &type)
{
	type.basicType = TBasicType::EbtUint;
	
	TType*	thread_id_x	= new TType{type};		thread_id_x->setFieldName( "launchID_x" );
	type.qualifier.layoutOffset += sizeof(uint);
	
	TType*	thread_id_y	= new TType{type};		thread_id_y->setFieldName( "launchID_y" );
	type.qualifier.layoutOffset += sizeof(uint);
	
	TType*	thread_id_z	= new TType{type};		thread_id_z->setFieldName( "launchID_z" );
	type.qualifier.layoutOffset += sizeof(uint);
	
	typeList->push_back({ thread_id_x,	TSourceLoc{} });
	typeList->push_back({ thread_id_y,	TSourceLoc{} });
	typeList->push_back({ thread_id_z,	TSourceLoc{} });
}

/*
=================================================
	CreateShaderDebugStorage
=================================================
*/
static void CreateShaderDebugStorage (uint descSetIndex, DebugInfo &dbgInfo, OUT uint64_t &posOffset, OUT uint64_t &dataOffset)
{
	// "layout(binding=x, std430) buffer dbg_ShaderTraceStorage { ... } dbg_ShaderTrace"
	
	TPublicType		uint_type;			uint_type.init({});
	uint_type.basicType					= TBasicType::EbtUint;
	uint_type.qualifier.storage			= TStorageQualifier::EvqBuffer;
	uint_type.qualifier.layoutMatrix	= TLayoutMatrix::ElmColumnMajor;
	uint_type.qualifier.layoutPacking	= TLayoutPacking::ElpStd430;
	uint_type.qualifier.precision		= TPrecisionQualifier::EpqHigh;
	uint_type.qualifier.layoutOffset	= 0;
	
#ifdef USE_STORAGE_QUALIFIERS
	uint_type.qualifier.coherent		= true;
#endif
	
	TTypeList*		type_list	= new TTypeList{};
	TPublicType		temp		= uint_type;

	BEGIN_ENUM_CHECKS();
	switch ( dbgInfo.GetShaderType() )
	{
		case EShLangVertex :
		case EShLangTessControl :
		case EShLangTessEvaluation :
		case EShLangGeometry :
		case EShLangFragment :
		case EShLangTaskNV :
		case EShLangMeshNV :			CreateGraphicsShaderDebugStorage( type_list, INOUT temp );		break;

		case EShLangCompute :			CreateComputeShaderDebugStorage( type_list, INOUT temp );		break;

		case EShLangRayGen :
		case EShLangIntersect :
		case EShLangAnyHit :
		case EShLangClosestHit :
		case EShLangMiss :
		case EShLangCallable :			CreateRayTracingShaderDebugStorage( type_list, INOUT temp );	break;

		case EShLangCount :
		default :						CHECK(false); return;
	}
	END_ENUM_CHECKS();

	uint_type.qualifier.layoutOffset = temp.qualifier.layoutOffset;

	TType*			position	= new TType{uint_type};		position->setFieldName( "position" );
	
	uint_type.qualifier.layoutOffset += sizeof(uint);
	uint_type.arraySizes			 = new TArraySizes{};
	uint_type.arraySizes->addInnerSize();
	
#ifdef USE_STORAGE_QUALIFIERS
	uint_type.qualifier.coherent	= false;
	uint_type.qualifier.restrict	= true;
	uint_type.qualifier.writeonly	= true;
#endif

	TType*			data_arr	= new TType{uint_type};		data_arr->setFieldName( "outData" );

	type_list->push_back({ position,	TSourceLoc{} });
	type_list->push_back({ data_arr,	TSourceLoc{} });

	TQualifier		block_qual;	block_qual.clear();
	block_qual.storage			= TStorageQualifier::EvqBuffer;
	block_qual.layoutMatrix		= TLayoutMatrix::ElmColumnMajor;
	block_qual.layoutPacking	= TLayoutPacking::ElpStd430;
	block_qual.layoutBinding	= 0;
	block_qual.layoutSet		= descSetIndex;

	TIntermSymbol*	storage_buf	= new TIntermSymbol{ 0x10000001, "dbg_ShaderTrace", TType{type_list, "dbg_ShaderTraceStorage", block_qual} };

	posOffset  = position->getQualifier().layoutOffset;
	dataOffset = data_arr->getQualifier().layoutOffset;
	
	dbgInfo.SetDebugStorage( storage_buf );
}

/*
=================================================
	CreateShaderBuiltinSymbols
=================================================
*/
static void CreateShaderBuiltinSymbols (TIntermNode*, DebugInfo &dbgInfo)
{
	const auto	shader				= dbgInfo.GetShaderType();
	const bool	is_compute			= (shader == EShLangCompute or shader == EShLangTaskNV or shader == EShLangMeshNV);
	const bool	need_invocation_id	= (shader == EShLangGeometry or shader == EShLangTessControl);
	const bool	need_primitive_id	= (shader == EShLangFragment or shader == EShLangTessControl or shader == EShLangTessEvaluation);
	const bool	need_launch_id		= (shader == EShLangRayGen or shader == EShLangIntersect or shader == EShLangAnyHit or
									   shader == EShLangClosestHit or shader == EShLangMiss or shader == EShLangCallable);
	TSourceLoc	loc		{};

	// find default source location
	/*if ( auto* aggr = root->getAsAggregate() )
	{
		for (auto& node : aggr->getSequence())
		{
			auto*	fn = node->getAsAggregate();

			if ( fn and fn->getOp() == TOperator::EOpFunction and fn->getName() == dbgInfo.GetEntryPoint() )
			{
				loc = fn->getLoc();
				break;
			}
		}
	}*/

	dbgInfo._isOpenGL = dbgInfo.GetCachedSymbolNode( "gl_VertexID" ) or
						dbgInfo.GetCachedSymbolNode( "gl_InstanceID" );
	

	if ( shader == EShLangFragment and not dbgInfo.GetCachedSymbolNode( "gl_FragCoord" ))
	{
		TPublicType		vec4_type;	vec4_type.init({});
		vec4_type.basicType			= TBasicType::EbtFloat;
		vec4_type.vectorSize		= 4;
		vec4_type.qualifier.storage	= TStorageQualifier::EvqFragCoord;
		vec4_type.qualifier.builtIn	= TBuiltInVariable::EbvFragCoord;
		
		TIntermSymbol*	symb = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "gl_FragCoord", TType{vec4_type} };
		symb->setLoc( loc );
		dbgInfo.CacheSymbolNode( symb );
	}
	
	// "Any static use of this variable in a fragment shader causes the entire shader to be evaluated per-sample rather than per-fragment."
	// so don't add 'gl_SampleID' and 'gl_SamplePosition' if it doesn't exists.
	
	if ( need_primitive_id and not dbgInfo.GetCachedSymbolNode( "gl_PrimitiveID" ))
	{
		TPublicType		int_type;	int_type.init({});
		int_type.basicType			= TBasicType::EbtInt;
		int_type.qualifier.storage	= TStorageQualifier::EvqVaryingIn;
		int_type.qualifier.builtIn	= TBuiltInVariable::EbvPrimitiveId;
		int_type.qualifier.flat		= true;

		TIntermSymbol*	symb = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "gl_PrimitiveID", TType{int_type} };
		symb->setLoc( loc );
		dbgInfo.CacheSymbolNode( symb );
	}

	if ( is_compute and not dbgInfo.GetCachedSymbolNode( "gl_GlobalInvocationID" ))
	{
		TPublicType		uint_type;	uint_type.init({});
		uint_type.basicType			= TBasicType::EbtUint;
		uint_type.vectorSize		= 3;
		uint_type.qualifier.storage	= TStorageQualifier::EvqVaryingIn;
		uint_type.qualifier.builtIn	= TBuiltInVariable::EbvGlobalInvocationId;

		TIntermSymbol*	symb = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "gl_GlobalInvocationID", TType{uint_type} };
		symb->setLoc( loc );
		dbgInfo.CacheSymbolNode( symb );
	}

	if ( is_compute and not dbgInfo.GetCachedSymbolNode( "gl_LocalInvocationID" ))
	{
		TPublicType		uint_type;	uint_type.init({});
		uint_type.basicType			= TBasicType::EbtUint;
		uint_type.vectorSize		= 3;
		uint_type.qualifier.storage	= TStorageQualifier::EvqVaryingIn;
		uint_type.qualifier.builtIn	= TBuiltInVariable::EbvLocalInvocationId;

		TIntermSymbol*	symb = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "gl_LocalInvocationID", TType{uint_type} };
		symb->setLoc( loc );
		dbgInfo.CacheSymbolNode( symb );
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
		dbgInfo.CacheSymbolNode( symb );
	}
	
	if ( need_invocation_id and not dbgInfo.GetCachedSymbolNode( "gl_InvocationID" ))
	{
		TPublicType		int_type;	int_type.init({});
		int_type.basicType			= TBasicType::EbtInt;
		int_type.qualifier.storage	= TStorageQualifier::EvqVaryingIn;
		int_type.qualifier.builtIn	= TBuiltInVariable::EbvInvocationId;

		TIntermSymbol*	symb = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "gl_InvocationID", TType{int_type} };
		symb->setLoc( loc );
		dbgInfo.CacheSymbolNode( symb );
	}
	
	if ( shader == EShLangGeometry and not dbgInfo.GetCachedSymbolNode( "gl_PrimitiveIDIn" ))
	{
		TPublicType		int_type;	int_type.init({});
		int_type.basicType			= TBasicType::EbtInt;
		int_type.qualifier.storage	= TStorageQualifier::EvqVaryingIn;
		int_type.qualifier.builtIn	= TBuiltInVariable::EbvPrimitiveId;

		TIntermSymbol*	symb = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "gl_PrimitiveIDIn", TType{int_type} };
		symb->setLoc( loc );
		dbgInfo.CacheSymbolNode( symb );
	}
	
	if ( shader == EShLangTessEvaluation and not dbgInfo.GetCachedSymbolNode( "gl_TessCoord" ))
	{
		TPublicType		float_type;		float_type.init({});
		float_type.basicType			= TBasicType::EbtFloat;
		float_type.vectorSize			= 3;
		float_type.qualifier.storage	= TStorageQualifier::EvqVaryingIn;
		float_type.qualifier.builtIn	= TBuiltInVariable::EbvTessCoord;

		TIntermSymbol*	symb = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "gl_TessCoord", TType{float_type} };
		symb->setLoc( loc );
		dbgInfo.CacheSymbolNode( symb );
	}
	
	if ( shader == EShLangTessEvaluation and not dbgInfo.GetCachedSymbolNode( "gl_TessLevelInner" ))
	{
		TPublicType		float_type;		float_type.init({});
		float_type.basicType			= TBasicType::EbtFloat;
		float_type.qualifier.storage	= TStorageQualifier::EvqVaryingIn;
		float_type.qualifier.builtIn	= TBuiltInVariable::EbvTessLevelInner;
		float_type.arraySizes			= new TArraySizes{};
		float_type.arraySizes->addInnerSize( 2 );

		TIntermSymbol*	symb = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "gl_TessLevelInner", TType{float_type} };
		symb->setLoc( loc );
		dbgInfo.CacheSymbolNode( symb );
	}
	
	if ( shader == EShLangTessEvaluation and not dbgInfo.GetCachedSymbolNode( "gl_TessLevelOuter" ))
	{
		TPublicType		float_type;		float_type.init({});
		float_type.basicType			= TBasicType::EbtFloat;
		float_type.qualifier.storage	= TStorageQualifier::EvqVaryingIn;
		float_type.qualifier.builtIn	= TBuiltInVariable::EbvTessLevelOuter;
		float_type.arraySizes			= new TArraySizes{};
		float_type.arraySizes->addInnerSize( 4 );

		TIntermSymbol*	symb = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "gl_TessLevelOuter", TType{float_type} };
		symb->setLoc( loc );
		dbgInfo.CacheSymbolNode( symb );
	}

	if ( need_launch_id and not dbgInfo.GetCachedSymbolNode( RT_LaunchID[dbgInfo.IsNVRT()] ))
	{
		TPublicType		uint_type;	uint_type.init({});
		uint_type.basicType			= TBasicType::EbtUint;
		uint_type.vectorSize		= 3;
		uint_type.qualifier.storage	= TStorageQualifier::EvqVaryingIn;
		uint_type.qualifier.builtIn	= TBuiltInVariable::EbvLaunchId;

		TIntermSymbol*	symb = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), RT_LaunchID[dbgInfo.IsNVRT()], TType{uint_type} };
		symb->setLoc( loc );
		dbgInfo.CacheSymbolNode( symb );
	}

	if ( shader == EShLangVertex and not dbgInfo._isOpenGL and not dbgInfo.GetCachedSymbolNode( "gl_VertexIndex" ))
	{
		TPublicType		int_type;	int_type.init({});
		int_type.basicType			= TBasicType::EbtInt;
		int_type.qualifier.storage	= TStorageQualifier::EvqVaryingIn;
		int_type.qualifier.builtIn	= TBuiltInVariable::EbvVertexIndex;

		TIntermSymbol*	symb = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "gl_VertexIndex", TType{int_type} };
		symb->setLoc( loc );
		dbgInfo.CacheSymbolNode( symb );
	}
	
	if ( shader == EShLangVertex and dbgInfo._isOpenGL and not dbgInfo.GetCachedSymbolNode( "gl_VertexID" ))
	{
		TPublicType		int_type;	int_type.init({});
		int_type.basicType			= TBasicType::EbtInt;
		int_type.qualifier.storage	= TStorageQualifier::EvqVaryingIn;
		int_type.qualifier.builtIn	= TBuiltInVariable::EbvVertexId;

		TIntermSymbol*	symb = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "gl_VertexID", TType{int_type} };
		symb->setLoc( loc );
		dbgInfo.CacheSymbolNode( symb );
	}

	if ( shader == EShLangVertex and not dbgInfo._isOpenGL and not dbgInfo.GetCachedSymbolNode( "gl_InstanceIndex" ))
	{
		TPublicType		int_type;	int_type.init({});
		int_type.basicType			= TBasicType::EbtInt;
		int_type.qualifier.storage	= TStorageQualifier::EvqVaryingIn;
		int_type.qualifier.builtIn	= TBuiltInVariable::EbvInstanceIndex;

		TIntermSymbol*	symb = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "gl_InstanceIndex", TType{int_type} };
		symb->setLoc( loc );
		dbgInfo.CacheSymbolNode( symb );
	}

	if ( shader == EShLangVertex and dbgInfo._isOpenGL and not dbgInfo.GetCachedSymbolNode( "gl_InstanceID" ))
	{
		TPublicType		int_type;	int_type.init({});
		int_type.basicType			= TBasicType::EbtInt;
		int_type.qualifier.storage	= TStorageQualifier::EvqVaryingIn;
		int_type.qualifier.builtIn	= TBuiltInVariable::EbvInstanceId;

		TIntermSymbol*	symb = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "gl_InstanceID", TType{int_type} };
		symb->setLoc( loc );
		dbgInfo.CacheSymbolNode( symb );
	}
}

/*
=================================================
	CreateAppendToTraceBody2
----
	also see 'CreateAppendToTrace2()'
=================================================
*/
static TIntermAggregate*  CreateAppendToTraceBody2 (DebugInfo &dbgInfo)
{
	TPublicType		uint_type;	uint_type.init({});
	uint_type.basicType			= TBasicType::EbtUint;
	uint_type.qualifier.storage = TStorageQualifier::EvqConstReadOnly;

	// last_pos, location, size, value
	const uint			dbg_data_size = 3;

	TIntermAggregate*	fn_node		= new TIntermAggregate{ TOperator::EOpFunction };
	TIntermAggregate*	fn_args		= new TIntermAggregate{ TOperator::EOpParameters };
	TIntermAggregate*	fn_body		= new TIntermAggregate{ TOperator::EOpSequence };
	TIntermAggregate*	branch_body = new TIntermAggregate{ TOperator::EOpSequence };

	// build function body
	{
		fn_node->setType( TType{ TBasicType::EbtVoid, TStorageQualifier::EvqGlobal } );
		fn_body->setType( TType{ TBasicType::EbtVoid } );
		fn_node->setName( "dbg_AppendToTrace(u1;" );
		fn_node->getSequence().push_back( fn_args );
		fn_node->getSequence().push_back( fn_body );
	}
	
	// build function argument sequence
	{
		TIntermSymbol*	arg0 = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "sourceLocation", TType{uint_type} };
		fn_args->setType( TType{EbtVoid} );
		fn_args->getSequence().push_back( arg0 );
	}

	// "pos" variable
	uint_type.qualifier.storage = TStorageQualifier::EvqTemporary;
	TIntermSymbol*	var_pos		= new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "pos", TType{uint_type} };
	
	// "uint pos = atomicAdd( dbg_ShaderTrace.position, x );"
	{
		TIntermAggregate*	move_pos	= new TIntermAggregate{ TOperator::EOpSequence };
		TIntermBinary*		assign_op	= new TIntermBinary{ TOperator::EOpAssign };			// pos = ...
		
		branch_body->setType( TType{EbtVoid} );
		branch_body->getSequence().push_back( move_pos );

		move_pos->setType( TType{EbtVoid} );
		move_pos->getSequence().push_back( assign_op );
		
		uint_type.qualifier.storage = TStorageQualifier::EvqConst;
		TConstUnionArray		data_size_value(1);	data_size_value[0].setUConst( dbg_data_size );
		TIntermConstantUnion*	data_size	= new TIntermConstantUnion{ data_size_value, TType{uint_type} };
		
		TIntermAggregate*		add_op	= new TIntermAggregate{ TOperator::EOpAtomicAdd };		// atomicAdd
		uint_type.qualifier.storage = TStorageQualifier::EvqGlobal;
		add_op->setType( TType{uint_type} );
		add_op->setOperationPrecision( TPrecisionQualifier::EpqHigh );
		add_op->getQualifierList().push_back( TStorageQualifier::EvqInOut );
		add_op->getQualifierList().push_back( TStorageQualifier::EvqIn );
		add_op->getSequence().push_back( dbgInfo.GetDebugStorageField("position") );
		add_op->getSequence().push_back( data_size );

		assign_op->setType( TType{uint_type} );
		assign_op->setLeft( var_pos );
		assign_op->setRight( add_op );
	}

	// "dbg_ShaderTrace.outData[pos++] = ..."
	{
		uint_type.qualifier.storage = TStorageQualifier::EvqConst;
		TConstUnionArray		type_value(1);	type_value[0].setUConst( (uint(TBasicType::EbtVoid) & 0xFF) );
		TIntermConstantUnion*	type_id			= new TIntermConstantUnion{ type_value, TType{uint_type} };
		TConstUnionArray		const_value(1);	const_value[0].setUConst( 1 );
		TIntermConstantUnion*	const_one		= new TIntermConstantUnion{ const_value, TType{uint_type} };
		
		uint_type.qualifier.storage = TStorageQualifier::EvqTemporary;
		TIntermBinary*			assign_data0	= new TIntermBinary{ TOperator::EOpAssign };
		TIntermBinary*			assign_data1	= new TIntermBinary{ TOperator::EOpAssign };
		TIntermBinary*			assign_data2	= new TIntermBinary{ TOperator::EOpAssign };
		TIntermBinary*			indexed_access	= new TIntermBinary{ TOperator::EOpIndexIndirect };
		TIntermUnary*			inc_pos			= new TIntermUnary{ TOperator::EOpPostIncrement };
		TIntermSymbol*			last_pos		= dbgInfo.GetCachedSymbolNode( "dbg_LastPosition" );
		TIntermBinary*			new_last_pos	= new TIntermBinary{ TOperator::EOpAssign };
		TIntermBinary*			prev_pos		= new TIntermBinary{ TOperator::EOpSub };

		// "pos++"
		inc_pos->setType( TType{uint_type} );
		inc_pos->setOperand( var_pos );

		// "dbg_ShaderTrace.outData[pos++]"
		indexed_access->setType( TType{uint_type} );
		indexed_access->getWritableType().setFieldName( "outData" );
		indexed_access->setLeft( dbgInfo.GetDebugStorageField("outData") );
		indexed_access->setRight( inc_pos );
		
		// "dbg_ShaderTrace.outData[pos++] = dbg_LastPosition"
		assign_data0->setType( TType{uint_type} );
		assign_data0->setLeft( indexed_access );
		assign_data0->setRight( last_pos );
		branch_body->getSequence().push_back( assign_data0 );
		
		// "pos - 1"
		prev_pos->setType( TType{uint_type} );
		prev_pos->setLeft( var_pos );
		prev_pos->setRight( const_one );

		// "dbg_LastPosition = pos - 1"
		new_last_pos->setType( TType{uint_type} );
		new_last_pos->setLeft( last_pos );
		new_last_pos->setRight( prev_pos );
		branch_body->getSequence().push_back( new_last_pos );

		// "dbg_ShaderTrace.outData[pos++] = sourceLocation"
		assign_data1->setType( TType{uint_type} );
		assign_data1->setLeft( indexed_access );
		assign_data1->setRight( fn_args->getSequence()[0]->getAsTyped() );
		branch_body->getSequence().push_back( assign_data1 );
		
		// "dbg_ShaderTrace.outData[pos++] = typeid"
		assign_data2->setType( TType{uint_type} );
		assign_data2->setLeft( indexed_access );
		assign_data2->setRight( type_id );
		branch_body->getSequence().push_back( assign_data2 );
	}
	
	// "if ( dbg_IsEnabled )"
	{
		TIntermSymbol*		condition	= dbgInfo.GetCachedSymbolNode( "dbg_IsEnabled" );
		TIntermSelection*	selection	= new TIntermSelection{ condition, branch_body, nullptr };
		selection->setType( TType{EbtVoid} );

		fn_body->getSequence().push_back( selection );
	}

	return fn_node;
}

/*
=================================================
	CreateAppendToTraceBody
----
	also see 'CreateAppendToTrace()'
=================================================
*/
static TIntermAggregate*  CreateAppendToTraceBody (const TString &fnName, DebugInfo &dbgInfo)
{
	TPublicType		value_type;	value_type.init({});
	TPublicType		uint_type;	uint_type.init({});
	TPublicType		index_type;	index_type.init({});

	uint_type.basicType			= TBasicType::EbtUint;
	uint_type.qualifier.storage = TStorageQualifier::EvqConstReadOnly;
	
	index_type.basicType		= TBasicType::EbtInt;
	index_type.qualifier.storage= TStorageQualifier::EvqConst;

	// extract type
	int	scale = 1;
	{
		size_t	pos  = fnName.find( '(' );
		size_t	pos1 = fnName.find( ';', pos );
		CHECK_ERR( pos != TString::npos and pos1 != TString::npos and pos < pos1 );
		++pos;

		const bool	is_vector	= (fnName[pos] == 'v');
		const bool	is_matrix	= (fnName[pos] == 'm');

		if ( is_vector or is_matrix )	++pos;

		const bool	is_64	= (pos+2 < fnName.size() and fnName[pos+1] == '6' and fnName[pos+2] == '4');
		//const bool	is_16	= (pos+2 < fnName.size() and fnName[pos+1] == '1' and fnName[pos+2] == '6');

		switch ( fnName[pos] )
		{
			case 'f' :	value_type.basicType = TBasicType::EbtFloat;	break;
			case 'd' :	value_type.basicType = TBasicType::EbtDouble;	scale = 2;	break;
			case 'b' :	value_type.basicType = TBasicType::EbtBool;		break;
			case 'i' :	if ( is_64 ) { value_type.basicType = TBasicType::EbtInt64;  scale = 2; } else value_type.basicType = TBasicType::EbtInt;	break;
			case 'u' :	if ( is_64 ) { value_type.basicType = TBasicType::EbtUint64; scale = 2; } else value_type.basicType = TBasicType::EbtUint;	break;
			default  :	RETURN_ERR( "unknown type" );
		}
		++pos;

		if ( is_matrix ) {
			value_type.setMatrix( fnName[pos] - '0', fnName[pos+1] - '0' );
			CHECK_ERR(	value_type.matrixCols > 0 and value_type.matrixCols <= 4 and
						value_type.matrixRows > 0 and value_type.matrixRows <= 4 );
		}
		else
		if ( is_vector ) {
			value_type.setVector( fnName[pos] - '0' );
			CHECK_ERR( value_type.vectorSize > 0 and value_type.vectorSize <= 4 );
		}
		else {
			// scalar
			value_type.vectorSize = 1;
		}
	}

	// last_pos, location, size, value
	const uint			dbg_data_size = (value_type.matrixCols * value_type.matrixRows + value_type.vectorSize) * scale + 3;

	TIntermAggregate*	fn_node		= new TIntermAggregate{ TOperator::EOpFunction };
	TIntermAggregate*	fn_args		= new TIntermAggregate{ TOperator::EOpParameters };
	TIntermAggregate*	fn_body		= new TIntermAggregate{ TOperator::EOpSequence };
	TIntermAggregate*	branch_body = new TIntermAggregate{ TOperator::EOpSequence };

	// build function body
	{
		value_type.qualifier.storage = TStorageQualifier::EvqGlobal;
		fn_node->setType( TType{value_type} );
		fn_node->setName( fnName );
		fn_node->getSequence().push_back( fn_args );
		fn_node->getSequence().push_back( fn_body );
	}
	
	// build function argument sequence
	{
		value_type.qualifier.storage = TStorageQualifier::EvqConstReadOnly;
		TIntermSymbol*		arg0	 = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "value", TType{value_type} };
		TIntermSymbol*		arg1	 = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "sourceLocation", TType{uint_type} };

		fn_args->setType( TType{EbtVoid} );
		fn_args->getSequence().push_back( arg0 );
		fn_args->getSequence().push_back( arg1 );
	}

	// build function body
	{
		value_type.qualifier.storage = TStorageQualifier::EvqTemporary;
		fn_body->setType( TType{value_type} );
	}

	// "pos" variable
	uint_type.qualifier.storage = TStorageQualifier::EvqTemporary;
	TIntermSymbol*	var_pos		= new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "pos", TType{uint_type} };
	
	// "uint pos = atomicAdd( dbg_ShaderTrace.position, x );"
	{
		TIntermAggregate*	move_pos	= new TIntermAggregate{ TOperator::EOpSequence };
		TIntermBinary*		assign_op	= new TIntermBinary{ TOperator::EOpAssign };			// pos = ...
		
		branch_body->setType( TType{EbtVoid} );
		branch_body->getSequence().push_back( move_pos );

		move_pos->setType( TType{EbtVoid} );
		move_pos->getSequence().push_back( assign_op );
		
		uint_type.qualifier.storage = TStorageQualifier::EvqConst;
		TConstUnionArray		data_size_value(1);	data_size_value[0].setUConst( dbg_data_size );
		TIntermConstantUnion*	data_size	= new TIntermConstantUnion{ data_size_value, TType{uint_type} };
		
		TIntermAggregate*		add_op	= new TIntermAggregate{ TOperator::EOpAtomicAdd };		// atomicAdd
		uint_type.qualifier.storage = TStorageQualifier::EvqGlobal;
		add_op->setType( TType{uint_type} );
		add_op->setOperationPrecision( TPrecisionQualifier::EpqHigh );
		add_op->getQualifierList().push_back( TStorageQualifier::EvqInOut );
		add_op->getQualifierList().push_back( TStorageQualifier::EvqIn );
		add_op->getSequence().push_back( dbgInfo.GetDebugStorageField("position") );
		add_op->getSequence().push_back( data_size );

		assign_op->setType( TType{uint_type} );
		assign_op->setLeft( var_pos );
		assign_op->setRight( add_op );
	}

	// "dbg_ShaderTrace.outData[pos++] = ..."
	{
		uint_type.qualifier.storage = TStorageQualifier::EvqConst;
		TConstUnionArray		type_value(1);	type_value[0].setUConst( (uint(value_type.basicType) & 0xFF) | ((value_type.vectorSize & 0xF) << 8) |
																		 ((value_type.matrixRows & 0xF) << 8) | ((value_type.matrixCols & 0xF) << 12) );
		TIntermConstantUnion*	type_id			= new TIntermConstantUnion{ type_value, TType{uint_type} };
		TConstUnionArray		const_value(1);	const_value[0].setUConst( 1 );
		TIntermConstantUnion*	const_one		= new TIntermConstantUnion{ const_value, TType{uint_type} };
		
		uint_type.qualifier.storage = TStorageQualifier::EvqTemporary;
		TIntermBinary*			assign_data0	= new TIntermBinary{ TOperator::EOpAssign };
		TIntermBinary*			assign_data1	= new TIntermBinary{ TOperator::EOpAssign };
		TIntermBinary*			assign_data2	= new TIntermBinary{ TOperator::EOpAssign };
		TIntermBinary*			indexed_access	= new TIntermBinary{ TOperator::EOpIndexIndirect };
		TIntermUnary*			inc_pos			= new TIntermUnary{ TOperator::EOpPostIncrement };
		TIntermSymbol*			last_pos		= dbgInfo.GetCachedSymbolNode( "dbg_LastPosition" );
		TIntermBinary*			new_last_pos	= new TIntermBinary{ TOperator::EOpAssign };
		TIntermBinary*			prev_pos		= new TIntermBinary{ TOperator::EOpSub };

		// "pos++"
		inc_pos->setType( TType{uint_type} );
		inc_pos->setOperand( var_pos );

		// "dbg_ShaderTrace.outData[pos++]"
		indexed_access->setType( TType{uint_type} );
		indexed_access->getWritableType().setFieldName( "outData" );
		indexed_access->setLeft( dbgInfo.GetDebugStorageField("outData") );
		indexed_access->setRight( inc_pos );
		
		// "dbg_ShaderTrace.outData[pos++] = dbg_LastPosition"
		assign_data0->setType( TType{uint_type} );
		assign_data0->setLeft( indexed_access );
		assign_data0->setRight( last_pos );
		branch_body->getSequence().push_back( assign_data0 );
		
		// "pos - 1"
		prev_pos->setType( TType{uint_type} );
		prev_pos->setLeft( var_pos );
		prev_pos->setRight( const_one );

		// "dbg_LastPosition = pos - 1"
		new_last_pos->setType( TType{uint_type} );
		new_last_pos->setLeft( last_pos );
		new_last_pos->setRight( prev_pos );
		branch_body->getSequence().push_back( new_last_pos );

		// "dbg_ShaderTrace.outData[pos++] = sourceLocation"
		assign_data1->setType( TType{uint_type} );
		assign_data1->setLeft( indexed_access );
		assign_data1->setRight( fn_args->getSequence()[1]->getAsTyped() );
		branch_body->getSequence().push_back( assign_data1 );
		
		// "dbg_ShaderTrace.outData[pos++] = typeid"
		assign_data2->setType( TType{uint_type} );
		assign_data2->setLeft( indexed_access );
		assign_data2->setRight( type_id );
		branch_body->getSequence().push_back( assign_data2 );

		// "ToUint(...)"
		const auto	TypeToUint	= [] (TIntermTyped* operand, int index) -> TIntermTyped*
		{
			TPublicType		utype;	utype.init({});
			utype.basicType			= TBasicType::EbtUint;
			utype.qualifier.storage = TStorageQualifier::EvqGlobal;

			BEGIN_ENUM_CHECKS();
			switch ( operand->getType().getBasicType() )
			{
				case TBasicType::EbtFloat : {
					TIntermUnary*	as_uint = new TIntermUnary{ TOperator::EOpFloatBitsToUint };
					as_uint->setType( TType{utype} );
					as_uint->setOperand( operand );
					as_uint->setOperationPrecision( TPrecisionQualifier::EpqHigh );
					return as_uint;
				}
				case TBasicType::EbtInt : {
					TIntermUnary*	to_uint = new TIntermUnary{ TOperator::EOpConvIntToUint };
					to_uint->setType( TType{utype} );
					to_uint->setOperand( operand );
					return to_uint;
				}
				case TBasicType::EbtUint : {
					return operand;
				}
				case TBasicType::EbtBool : {
					TIntermUnary*	to_uint = new TIntermUnary{ TOperator::EOpConvBoolToUint };
					to_uint->setType( TType{utype} );
					to_uint->setOperand( operand );
					return to_uint;
				}
				case TBasicType::EbtDouble : {
					// "doubleBitsToUint64(value)"
					TIntermUnary*	as_uint64 = new TIntermUnary{ TOperator::EOpDoubleBitsToUint64 };
					utype.basicType = TBasicType::EbtUint64;
					as_uint64->setType( TType{utype} );
					as_uint64->setOperand( operand );
					as_uint64->setOperationPrecision( TPrecisionQualifier::EpqHigh );
					
					// "doubleBitsToUint64(value) >> x"
					utype.qualifier.storage = TStorageQualifier::EvqConst;
					TConstUnionArray		shift_value(1);	shift_value[0].setU64Const( (index&1)*32 );
					TIntermConstantUnion*	const_shift		= new TIntermConstantUnion{ shift_value, TType{utype} };
					TIntermBinary*			shift			= new TIntermBinary{ TOperator::EOpRightShift };
					utype.qualifier.storage	= TStorageQualifier::EvqTemporary;
					shift->setType( TType{utype} );
					shift->setLeft( as_uint64 );
					shift->setRight( const_shift );
					
					// "uint(doubleBitsToUint64(value) >> x)"
					TIntermUnary*			to_uint = new TIntermUnary{ TOperator::EOpConvUint64ToUint };
					utype.basicType			= TBasicType::EbtUint;
					utype.qualifier.storage	= TStorageQualifier::EvqGlobal;
					to_uint->setType( TType{utype} );
					to_uint->setOperand( shift );
					return to_uint;
				}
				case TBasicType::EbtInt64 : {
					// "value >> x"
					utype.basicType			= TBasicType::EbtInt64;
					utype.qualifier.storage = TStorageQualifier::EvqConst;
					TConstUnionArray		shift_value(1);	shift_value[0].setI64Const( (index&1)*32 );
					TIntermConstantUnion*	const_shift		= new TIntermConstantUnion{ shift_value, TType{utype} };
					TIntermBinary*			shift			= new TIntermBinary{ TOperator::EOpRightShift };
					utype.qualifier.storage	= TStorageQualifier::EvqTemporary;
					shift->setType( TType{utype} );
					shift->setLeft( operand );
					shift->setRight( const_shift );
					
					// "uint(value >> x)"
					TIntermUnary*			to_uint = new TIntermUnary{ TOperator::EOpConvInt64ToUint };
					utype.basicType			= TBasicType::EbtUint;
					utype.qualifier.storage	= TStorageQualifier::EvqGlobal;
					to_uint->setType( TType{utype} );
					to_uint->setOperand( shift );
					return to_uint;
				}
				case TBasicType::EbtUint64 : {
					// "value >> x"
					utype.basicType			= TBasicType::EbtUint64;
					utype.qualifier.storage = TStorageQualifier::EvqConst;
					TConstUnionArray		shift_value(1);	shift_value[0].setU64Const( (index&1)*32 );
					TIntermConstantUnion*	const_shift		= new TIntermConstantUnion{ shift_value, TType{utype} };
					TIntermBinary*			shift			= new TIntermBinary{ TOperator::EOpRightShift };
					utype.qualifier.storage	= TStorageQualifier::EvqTemporary;
					shift->setType( TType{utype} );
					shift->setLeft( operand );
					shift->setRight( const_shift );
					
					// "uint(value >> x)"
					TIntermUnary*			to_uint = new TIntermUnary{ TOperator::EOpConvUint64ToUint };
					utype.basicType			= TBasicType::EbtUint;
					utype.qualifier.storage	= TStorageQualifier::EvqGlobal;
					to_uint->setType( TType{utype} );
					to_uint->setOperand( shift );
					return to_uint;
				}
				case TBasicType::EbtVoid :
				case TBasicType::EbtFloat16 :
				case TBasicType::EbtInt8 :
				case TBasicType::EbtUint8 :
				case TBasicType::EbtInt16 :
				case TBasicType::EbtUint16 :
				case TBasicType::EbtAtomicUint :
				case TBasicType::EbtSampler :
				case TBasicType::EbtStruct :
				case TBasicType::EbtBlock :
				case TBasicType::EbtReference :
				case TBasicType::EbtString :
				case TBasicType::EbtNumTypes :
				case TBasicType::EbtAccStruct :
				case TBasicType::EbtRayQuery :
					break;
			}
			END_ENUM_CHECKS();
			RETURN_ERR( "not supported" );
		};

		// "dbg_ShaderTrace.outData[pos++] = ToUint(value)"
		if ( value_type.isScalar() )
		{
			for (int i = 0; i < scale; ++i)
			{
				TIntermBinary*	assign_data3	= new TIntermBinary{ TOperator::EOpAssign };
				TIntermTyped*	scalar			= fn_args->getSequence()[0]->getAsTyped();
			
				assign_data3->setType( TType{uint_type} );
				assign_data3->setLeft( indexed_access );
				assign_data3->setRight( TypeToUint( scalar, i ));
				branch_body->getSequence().push_back( assign_data3 );
			}
		}
		else
		if ( value_type.matrixCols and value_type.matrixRows )
		{
			TIntermTyped*	mat			= fn_args->getSequence()[0]->getAsTyped();
			TPublicType		pub_type;	pub_type.init({});

			pub_type.basicType			= mat->getType().getBasicType();
			pub_type.qualifier.storage	= mat->getType().getQualifier().storage;

			for (int c = 0; c < value_type.matrixCols; ++c)
			{
				TConstUnionArray		col_index(1);	col_index[0].setIConst( c );
				TIntermConstantUnion*	col_field		= new TIntermConstantUnion{ col_index, TType{index_type} };
				TIntermBinary*			col_access		= new TIntermBinary{ TOperator::EOpIndexDirect };
				
				// "matrix[c]"
				pub_type.setVector( value_type.matrixRows );
				col_access->setType( TType{pub_type} );
				col_access->setLeft( mat );
				col_access->setRight( col_field );

				for (int r = 0; r < value_type.matrixRows * scale; ++r)
				{
					TConstUnionArray		row_index(1);	col_index[0].setIConst( r / scale );
					TIntermConstantUnion*	row_field		= new TIntermConstantUnion{ row_index, TType{index_type} };
					TIntermBinary*			row_access		= new TIntermBinary{ TOperator::EOpIndexDirect };
					TIntermBinary*			assign_data3	= new TIntermBinary{ TOperator::EOpAssign };
					
					// "matrix[c][r]"
					pub_type.setVector( 1 );
					row_access->setType( TType{pub_type} );
					row_access->setLeft( col_access );
					row_access->setRight( row_field );
					
					// "dbg_ShaderTrace.outData[pos++] = ToUint(value.x)"
					assign_data3->setType( TType{uint_type} );
					assign_data3->setLeft( indexed_access );
					assign_data3->setRight( TypeToUint( row_access, r ));
					branch_body->getSequence().push_back( assign_data3 );
				}
			}
		}
		else
		for (int i = 0; i < value_type.vectorSize * scale; ++i)
		{
			TIntermBinary*			assign_data3	= new TIntermBinary{ TOperator::EOpAssign };
			TConstUnionArray		field_index(1);	field_index[0].setIConst( i / scale );
			TIntermConstantUnion*	vec_field		= new TIntermConstantUnion{ field_index, TType{index_type} };
			TIntermBinary*			field_access	= new TIntermBinary{ TOperator::EOpIndexDirect };
			TIntermTyped*			vec				= fn_args->getSequence()[0]->getAsTyped();
			
			TPublicType		pub_type;	pub_type.init({});
			pub_type.basicType			= vec->getType().getBasicType();
			pub_type.qualifier.storage	= vec->getType().getQualifier().storage;

			// "value.x"
			field_access->setType( TType{pub_type} );
			field_access->setLeft( vec );
			field_access->setRight( vec_field );
			
			// "dbg_ShaderTrace.outData[pos++] = ToUint(value.x)"
			assign_data3->setType( TType{uint_type} );
			assign_data3->setLeft( indexed_access );
			assign_data3->setRight( TypeToUint( field_access, i ));
			branch_body->getSequence().push_back( assign_data3 );
		}
	}
	
	// "if ( dbg_IsEnabled )"
	{
		TIntermSymbol*		condition	= dbgInfo.GetCachedSymbolNode( "dbg_IsEnabled" );
		TIntermSelection*	selection	= new TIntermSelection{ condition, branch_body, nullptr };
		selection->setType( TType{EbtVoid} );

		fn_body->getSequence().push_back( selection );
	}

	// "return value"
	{
		TIntermBranch*		fn_return	= new TIntermBranch{ TOperator::EOpReturn, fn_args->getSequence()[0]->getAsTyped() };
		fn_body->getSequence().push_back( fn_return );
	}

	return fn_node;
}

/*
=================================================
	AppendShaderInputVaryings
=================================================
*/
static bool  AppendShaderInputVaryings (TIntermAggregate* body, DebugInfo &dbgInfo)
{
	CHECK_ERR( dbgInfo.GetCallStack().size() );

	TIntermAggregate*	root		= dbgInfo.GetCallStack().front().node->getAsAggregate();
	TIntermAggregate*	linker_objs	= nullptr;
	CHECK_ERR( root );

	for (auto* node : root->getSequence()) {
		if ( auto* aggr = node->getAsAggregate(); aggr and aggr->getOp() == TOperator::EOpLinkerObjects ) {
			linker_objs = aggr;
			break;
		}
	}
	CHECK_ERR( linker_objs );

	for (auto* node : linker_objs->getSequence())
	{
		TIntermSymbol*	symb = node->getAsSymbolNode();

		if ( not symb or symb->getQualifier().storage != TStorageQualifier::EvqVaryingIn )
			continue;

		if ( symb->isStruct() )
		{
			auto&	struct_fields = *symb->getType().getStruct();
			
			TPublicType		index_type;		index_type.init({});
			index_type.basicType			= TBasicType::EbtInt;
			index_type.qualifier.storage	= TStorageQualifier::EvqConst;

			for (auto& field : struct_fields)
			{
				ASSERT( not field.type->isStruct() );	// TODO

				const size_t			index			= std::distance( struct_fields.data(), &field );
				TConstUnionArray		index_Value(1);	index_Value[0].setIConst( int(index) );
				TIntermConstantUnion*	field_index		= new TIntermConstantUnion{ index_Value, TType{index_type} };
				TIntermBinary*			field_access	= new TIntermBinary{ TOperator::EOpIndexDirectStruct };
				field_access->setType( *field.type );
				field_access->setLeft( symb );
				field_access->setRight( field_index );

				const uint	loc_id = dbgInfo.GetCustomSourceLocation( field_access, symb->getLoc()/*field.loc*/ );

				if ( auto* fncall = CreateAppendToTrace( field_access, loc_id, dbgInfo ))
					body->getSequence().push_back( fncall );
			}
		}
		else
		if ( symb->isScalar() or symb->isVector() or symb->isMatrix() )
		{
			const uint	loc_id = dbgInfo.GetCustomSourceLocation( symb, symb->getLoc() );

			if ( auto* fncall = CreateAppendToTrace( symb, loc_id, dbgInfo ))
				body->getSequence().push_back( fncall );
		}
	}

	return true;
}

/*
=================================================
	RecordVertexShaderInfo
=================================================
*/
static TIntermAggregate*  RecordVertexShaderInfo (const TSourceLoc &loc, DebugInfo &dbgInfo)
{
	TIntermAggregate*	body = new TIntermAggregate{ TOperator::EOpSequence };
	body->setType( TType{EbtVoid} );
	
	if ( dbgInfo._isOpenGL )
	{
		// "dbg_AppendToTrace( gl_VertexID, location )"
		{
			TIntermSymbol*	vert_index	= dbgInfo.GetCachedSymbolNode( "gl_VertexID" );
			const uint		loc_id		= dbgInfo.GetCustomSourceLocation( vert_index, loc );

			body->getSequence().push_back( CreateAppendToTrace( vert_index, loc_id, dbgInfo ));
		}
	
		// "dbg_AppendToTrace( gl_InstanceID, location )"
		{
			TIntermSymbol*	instance	= dbgInfo.GetCachedSymbolNode( "gl_InstanceID" );
			const uint		loc_id		= dbgInfo.GetCustomSourceLocation( instance, loc );

			body->getSequence().push_back( CreateAppendToTrace( instance, loc_id, dbgInfo ));
		}
	}
	else
	{
		// "dbg_AppendToTrace( gl_VertexIndex, location )"
		{
			TIntermSymbol*	vert_index	= dbgInfo.GetCachedSymbolNode( "gl_VertexIndex" );
			const uint		loc_id		= dbgInfo.GetCustomSourceLocation( vert_index, loc );

			body->getSequence().push_back( CreateAppendToTrace( vert_index, loc_id, dbgInfo ));
		}
	
		// "dbg_AppendToTrace( gl_InstanceIndex, location )"
		{
			TIntermSymbol*	instance	= dbgInfo.GetCachedSymbolNode( "gl_InstanceIndex" );
			const uint		loc_id		= dbgInfo.GetCustomSourceLocation( instance, loc );

			body->getSequence().push_back( CreateAppendToTrace( instance, loc_id, dbgInfo ));
		}
	}

	// "dbg_AppendToTrace( gl_DrawID, location )"
	if ( auto* draw_id = dbgInfo.GetCachedSymbolNode( "gl_DrawID" ) )
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( draw_id, loc );
		body->getSequence().push_back( CreateAppendToTrace( draw_id, loc_id, dbgInfo ));
	}

	return body;
}

/*
=================================================
	RecordTessControlShaderInfo
=================================================
*/
static TIntermAggregate*  RecordTessControlShaderInfo (const TSourceLoc &loc, DebugInfo &dbgInfo)
{
	TIntermAggregate*	body = new TIntermAggregate{ TOperator::EOpSequence };
	body->setType( TType{EbtVoid} );
	
	// "dbg_AppendToTrace( gl_InvocationID, location )"
	{
		TIntermSymbol*	invocation	= dbgInfo.GetCachedSymbolNode( "gl_InvocationID" );
		const uint		loc_id		= dbgInfo.GetCustomSourceLocation( invocation, loc );

		body->getSequence().push_back( CreateAppendToTrace( invocation, loc_id, dbgInfo ));
	}
	
	// "dbg_AppendToTrace( gl_PrimitiveID, location )"
	{
		TIntermSymbol*	primitive	= dbgInfo.GetCachedSymbolNode( "gl_PrimitiveID" );
		const uint		loc_id		= dbgInfo.GetCustomSourceLocation( primitive, loc );

		body->getSequence().push_back( CreateAppendToTrace( primitive, loc_id, dbgInfo ));
	}

	return body;
}

/*
=================================================
	RecordTessEvaluationShaderInfo
=================================================
*/
static TIntermAggregate*  RecordTessEvaluationShaderInfo (const TSourceLoc &loc, DebugInfo &dbgInfo)
{
	TIntermAggregate*	body = new TIntermAggregate{ TOperator::EOpSequence };
	body->setType( TType{EbtVoid} );
	
	// "dbg_AppendToTrace( gl_PrimitiveID, location )"
	{
		TIntermSymbol*	primitive	= dbgInfo.GetCachedSymbolNode( "gl_PrimitiveID" );
		const uint		loc_id		= dbgInfo.GetCustomSourceLocation( primitive, loc );

		body->getSequence().push_back( CreateAppendToTrace( primitive, loc_id, dbgInfo ));
	}
	
	// "dbg_AppendToTrace( gl_TessCoord, location )"
	{
		TIntermSymbol*	tess_coord	= dbgInfo.GetCachedSymbolNode( "gl_TessCoord" );
		const uint		loc_id		= dbgInfo.GetCustomSourceLocation( tess_coord, loc );

		body->getSequence().push_back( CreateAppendToTrace( tess_coord, loc_id, dbgInfo ));
	}
	
	TPublicType		index_type;		index_type.init({});
	index_type.basicType			= TBasicType::EbtInt;
	index_type.qualifier.storage	= TStorageQualifier::EvqConst;

	// "dbg_AppendToTrace( gl_TessLevelInner, location )"
	{
		TIntermSymbol*		inner_level	= dbgInfo.GetCachedSymbolNode( "gl_TessLevelInner" );
		const uint			loc_id		= dbgInfo.GetCustomSourceLocation( inner_level, loc );
		TIntermAggregate*	vec2_ctor	= new TIntermAggregate{ TOperator::EOpConstructVec2 };
		TPublicType			float_type;	float_type.init({});
		float_type.basicType			= TBasicType::EbtFloat;
		float_type.qualifier.storage	= TStorageQualifier::EvqTemporary;
		float_type.qualifier.builtIn	= TBuiltInVariable::EbvTessLevelInner;
		float_type.qualifier.precision	= TPrecisionQualifier::EpqHigh;

		for (int i = 0; i < 2; ++i) {
			TIntermBinary*			elem_access		= new TIntermBinary{ TOperator::EOpIndexDirect };
			TConstUnionArray		elem_Value(1);	elem_Value[0].setIConst( i );
			TIntermConstantUnion*	elem_index		= new TIntermConstantUnion{ elem_Value, TType{index_type} };
			elem_access->setType( TType{float_type} );
			elem_access->setLeft( inner_level );
			elem_access->setRight( elem_index );
			vec2_ctor->getSequence().push_back( elem_access );
		}

		float_type.vectorSize			= 2;
		float_type.qualifier.builtIn	= TBuiltInVariable::EbvNone;
		float_type.qualifier.precision	= TPrecisionQualifier::EpqNone;

		vec2_ctor->setType( TType{float_type} );
		body->getSequence().push_back( CreateAppendToTrace( vec2_ctor, loc_id, dbgInfo ));
	}
	
	// "dbg_AppendToTrace( gl_TessLevelOuter, location )"
	{
		TIntermSymbol*		outer_level	= dbgInfo.GetCachedSymbolNode( "gl_TessLevelOuter" );
		const uint			loc_id		= dbgInfo.GetCustomSourceLocation( outer_level, loc );
		TIntermAggregate*	vec4_ctor	= new TIntermAggregate{ TOperator::EOpConstructVec4 };
		TPublicType			float_type;	float_type.init({});
		float_type.basicType			= TBasicType::EbtFloat;
		float_type.qualifier.storage	= TStorageQualifier::EvqTemporary;
		float_type.qualifier.builtIn	= TBuiltInVariable::EbvTessLevelOuter;
		float_type.qualifier.precision	= TPrecisionQualifier::EpqHigh;

		for (int i = 0; i < 4; ++i) {
			TIntermBinary*			elem_access		= new TIntermBinary{ TOperator::EOpIndexDirect };
			TConstUnionArray		elem_Value(1);	elem_Value[0].setIConst( i );
			TIntermConstantUnion*	elem_index		= new TIntermConstantUnion{ elem_Value, TType{index_type} };
			elem_access->setType( TType{float_type} );
			elem_access->setLeft( outer_level );
			elem_access->setRight( elem_index );
			vec4_ctor->getSequence().push_back( elem_access );
		}
		
		float_type.vectorSize			= 4;
		float_type.qualifier.builtIn	= TBuiltInVariable::EbvNone;
		float_type.qualifier.precision	= TPrecisionQualifier::EpqNone;

		vec4_ctor->setType( TType{float_type} );
		body->getSequence().push_back( CreateAppendToTrace( vec4_ctor, loc_id, dbgInfo ));
	}

	return body;
}

/*
=================================================
	RecordGeometryShaderInfo
=================================================
*/
static TIntermAggregate*  RecordGeometryShaderInfo (const TSourceLoc &loc, DebugInfo &dbgInfo)
{
	TIntermAggregate*	body = new TIntermAggregate{ TOperator::EOpSequence };
	body->setType( TType{EbtVoid} );
	
	// "dbg_AppendToTrace( gl_InvocationID, location )"
	{
		TIntermSymbol*	invocation	= dbgInfo.GetCachedSymbolNode( "gl_InvocationID" );
		const uint		loc_id		= dbgInfo.GetCustomSourceLocation( invocation, loc );

		body->getSequence().push_back( CreateAppendToTrace( invocation, loc_id, dbgInfo ));
	}
	
	// "dbg_AppendToTrace( gl_PrimitiveIDIn, location )"
	{
		TIntermSymbol*	primitive	= dbgInfo.GetCachedSymbolNode( "gl_PrimitiveIDIn" );
		const uint		loc_id		= dbgInfo.GetCustomSourceLocation( primitive, loc );

		body->getSequence().push_back( CreateAppendToTrace( primitive, loc_id, dbgInfo ));
	}

	return body;
}

/*
=================================================
	RecordFragmentShaderInfo
=================================================
*/
static TIntermAggregate*  RecordFragmentShaderInfo (const TSourceLoc &loc, DebugInfo &dbgInfo)
{
	TIntermAggregate*	body = new TIntermAggregate{ TOperator::EOpSequence };
	body->setType( TType{EbtVoid} );

	// "dbg_AppendToTrace( gl_FragCoord, location )"
	{
		TIntermSymbol*	fragcoord	= dbgInfo.GetCachedSymbolNode( "gl_FragCoord" );
		const uint		loc_id		= dbgInfo.GetCustomSourceLocation( fragcoord, loc );

		body->getSequence().push_back( CreateAppendToTrace( fragcoord, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_SampleID, location )"
	if ( TIntermSymbol*  sample_id = dbgInfo.GetCachedSymbolNode( "gl_SampleID" ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( sample_id, loc );
		body->getSequence().push_back( CreateAppendToTrace( sample_id, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_SamplePosition, location )"
	if ( TIntermSymbol*  sample_pos = dbgInfo.GetCachedSymbolNode( "gl_SamplePosition" ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( sample_pos, loc );
		body->getSequence().push_back( CreateAppendToTrace( sample_pos, loc_id, dbgInfo ));
	}
	
	// "dbg_AppendToTrace( gl_PrimitiveID, location )"
	{
		TIntermSymbol*	primitive_id	= dbgInfo.GetCachedSymbolNode( "gl_PrimitiveID" );
		const uint		loc_id			= dbgInfo.GetCustomSourceLocation( primitive_id, loc );

		body->getSequence().push_back( CreateAppendToTrace( primitive_id, loc_id, dbgInfo ));
	}
	
	// "dbg_AppendToTrace( gl_SamplePosition, location )"
	if ( TIntermSymbol*  layer = dbgInfo.GetCachedSymbolNode( "gl_Layer" ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( layer, loc );
		body->getSequence().push_back( CreateAppendToTrace( layer, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_ViewportIndex, location )"
	if ( TIntermSymbol*  viewport_idx = dbgInfo.GetCachedSymbolNode( "gl_ViewportIndex" ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( viewport_idx, loc );
		body->getSequence().push_back( CreateAppendToTrace( viewport_idx, loc_id, dbgInfo ));
	}
	
	// "dbg_AppendToTrace( gl_SubgroupInvocationID, location )"
	if ( auto* invocation = dbgInfo.GetCachedSymbolNode( "gl_SubgroupInvocationID" ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( invocation, loc );
		body->getSequence().push_back( CreateAppendToTrace( invocation, loc_id, dbgInfo ));
	}

	AppendShaderInputVaryings( body, dbgInfo );

	return body;
}

/*
=================================================
	RecordComputeShaderInfo
=================================================
*/
static TIntermAggregate*  RecordComputeShaderInfo (const TSourceLoc &loc, DebugInfo &dbgInfo)
{
	TIntermAggregate*	body = new TIntermAggregate{ TOperator::EOpSequence };
	body->setType( TType{EbtVoid} );
	
	// "dbg_AppendToTrace( gl_GlobalInvocationID, location )"
	{
		TIntermSymbol*	invocation	= dbgInfo.GetCachedSymbolNode( "gl_GlobalInvocationID" );
		const uint		loc_id		= dbgInfo.GetCustomSourceLocation( invocation, loc );

		body->getSequence().push_back( CreateAppendToTrace( invocation, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_LocalInvocationID, location )"
	{
		TIntermSymbol*	invocation	= dbgInfo.GetCachedSymbolNode( "gl_LocalInvocationID" );
		const uint		loc_id		= dbgInfo.GetCustomSourceLocation( invocation, loc );

		body->getSequence().push_back( CreateAppendToTrace( invocation, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_WorkGroupID, location )"
	{
		TIntermSymbol*	invocation	= dbgInfo.GetCachedSymbolNode( "gl_WorkGroupID" );
		const uint		loc_id		= dbgInfo.GetCustomSourceLocation( invocation, loc );

		body->getSequence().push_back( CreateAppendToTrace( invocation, loc_id, dbgInfo ));
	}
	
	// "dbg_AppendToTrace( gl_SubgroupID, location )"
	if ( auto* invocation = dbgInfo.GetCachedSymbolNode( "gl_SubgroupID" ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( invocation, loc );
		body->getSequence().push_back( CreateAppendToTrace( invocation, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_SubgroupInvocationID, location )"
	if ( auto* invocation = dbgInfo.GetCachedSymbolNode( "gl_SubgroupInvocationID" ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( invocation, loc );
		body->getSequence().push_back( CreateAppendToTrace( invocation, loc_id, dbgInfo ));
	}

	return body;
}

/*
=================================================
	RecordRayGenShaderInfo
=================================================
*/
static TIntermAggregate*  RecordRayGenShaderInfo (const TSourceLoc &loc, DebugInfo &dbgInfo)
{
	TIntermAggregate*	body = new TIntermAggregate{ TOperator::EOpSequence };
	body->setType( TType{EbtVoid} );
	
	// "dbg_AppendToTrace( gl_LaunchID, location )"
	{
		TIntermSymbol*	invocation	= dbgInfo.GetCachedSymbolNode( RT_LaunchID[dbgInfo.IsNVRT()] );
		const uint		loc_id		= dbgInfo.GetCustomSourceLocation( invocation, loc );

		body->getSequence().push_back( CreateAppendToTrace( invocation, loc_id, dbgInfo ));
	}
	return body;
}

/*
=================================================
	RecordHitShaderInfo
=================================================
*/
static TIntermAggregate*  RecordHitShaderInfo (const TSourceLoc &loc, DebugInfo &dbgInfo)
{
	TIntermAggregate*	body = new TIntermAggregate{ TOperator::EOpSequence };
	body->setType( TType{EbtVoid} );
	
	// "dbg_AppendToTrace( gl_LaunchID, location )"
	{
		TIntermSymbol*	invocation	= dbgInfo.GetCachedSymbolNode( RT_LaunchID[dbgInfo.IsNVRT()] );
		const uint		loc_id		= dbgInfo.GetCustomSourceLocation( invocation, loc );

		body->getSequence().push_back( CreateAppendToTrace( invocation, loc_id, dbgInfo ));
	}
	
	// "dbg_AppendToTrace( gl_PrimitiveID, location )"
	if ( auto* primitive = dbgInfo.GetCachedSymbolNode( "gl_PrimitiveID" ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( primitive, loc );
		body->getSequence().push_back( CreateAppendToTrace( primitive, loc_id, dbgInfo ));
	}
	
	// "dbg_AppendToTrace( gl_InstanceID, location )"
	if ( auto* instance = dbgInfo.GetCachedSymbolNode( "gl_InstanceID" ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( instance, loc );
		body->getSequence().push_back( CreateAppendToTrace( instance, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_InstanceCustomIndex, location )"
	if ( auto* instance_index = dbgInfo.GetCachedSymbolNode( RT_InstanceCustomIndex[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( instance_index, loc );
		body->getSequence().push_back( CreateAppendToTrace( instance_index, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_WorldRayOrigin, location )"
	if ( auto* world_ray_origin = dbgInfo.GetCachedSymbolNode( RT_WorldRayOrigin[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( world_ray_origin, loc );
		body->getSequence().push_back( CreateAppendToTrace( world_ray_origin, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_WorldRayDirection, location )"
	if ( auto* world_ray_direction = dbgInfo.GetCachedSymbolNode( RT_WorldRayDirection[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( world_ray_direction, loc );
		body->getSequence().push_back( CreateAppendToTrace( world_ray_direction, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_ObjectRayOrigin, location )"
	if ( auto* object_ray_origin = dbgInfo.GetCachedSymbolNode( RT_ObjectRayOrigin[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( object_ray_origin, loc );
		body->getSequence().push_back( CreateAppendToTrace( object_ray_origin, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_ObjectRayDirection, location )"
	if ( auto* object_ray_direction = dbgInfo.GetCachedSymbolNode( RT_ObjectRayDirection[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( object_ray_direction, loc );
		body->getSequence().push_back( CreateAppendToTrace( object_ray_direction, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_RayTmin, location )"
	if ( auto* ray_tmin = dbgInfo.GetCachedSymbolNode( RT_RayTmin[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( ray_tmin, loc );
		body->getSequence().push_back( CreateAppendToTrace( ray_tmin, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_RayTmax, location )"
	if ( auto* ray_tmax = dbgInfo.GetCachedSymbolNode( RT_RayTmax[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( ray_tmax, loc );
		body->getSequence().push_back( CreateAppendToTrace( ray_tmax, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_IncomingRayFlags, location )"
	if ( auto* incoming_ray_flags = dbgInfo.GetCachedSymbolNode( RT_IncomingRayFlags[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( incoming_ray_flags, loc );
		body->getSequence().push_back( CreateAppendToTrace( incoming_ray_flags, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_HitT, location )"
	if ( auto* hit_t = dbgInfo.GetCachedSymbolNode( RT_HitT[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( hit_t, loc );
		body->getSequence().push_back( CreateAppendToTrace( hit_t, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_HitKind, location )"
	if ( auto* hit_kind = dbgInfo.GetCachedSymbolNode( RT_HitKind[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( hit_kind, loc );
		body->getSequence().push_back( CreateAppendToTrace( hit_kind, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_ObjectToWorld, location )"
	if ( auto* object_to_world = dbgInfo.GetCachedSymbolNode( RT_ObjectToWorld[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( object_to_world, loc );
		body->getSequence().push_back( CreateAppendToTrace( object_to_world, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_WorldToObject, location )"
	if ( auto* world_to_object = dbgInfo.GetCachedSymbolNode( RT_WorldToObject[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( world_to_object, loc );
		body->getSequence().push_back( CreateAppendToTrace( world_to_object, loc_id, dbgInfo ));
	}
	return body;
}

/*
=================================================
	RecordIntersectionShaderInfo
=================================================
*/
static TIntermAggregate*  RecordIntersectionShaderInfo (const TSourceLoc &loc, DebugInfo &dbgInfo)
{
	TIntermAggregate*	body = new TIntermAggregate{ TOperator::EOpSequence };
	body->setType( TType{EbtVoid} );
	
	// "dbg_AppendToTrace( gl_LaunchID, location )"
	{
		TIntermSymbol*	invocation	= dbgInfo.GetCachedSymbolNode( RT_LaunchID[dbgInfo.IsNVRT()] );
		const uint		loc_id		= dbgInfo.GetCustomSourceLocation( invocation, loc );

		body->getSequence().push_back( CreateAppendToTrace( invocation, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_PrimitiveID, location )"
	if ( auto* primitive = dbgInfo.GetCachedSymbolNode( "gl_PrimitiveID" ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( primitive, loc );
		body->getSequence().push_back( CreateAppendToTrace( primitive, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_InstanceID, location )"
	if ( auto* instance = dbgInfo.GetCachedSymbolNode( "gl_InstanceID" ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( instance, loc );
		body->getSequence().push_back( CreateAppendToTrace( instance, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_InstanceCustomIndex, location )"
	if ( auto* instance_index = dbgInfo.GetCachedSymbolNode( RT_InstanceCustomIndex[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( instance_index, loc );
		body->getSequence().push_back( CreateAppendToTrace( instance_index, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_WorldRayOrigin, location )"
	if ( auto* world_ray_origin = dbgInfo.GetCachedSymbolNode( RT_WorldRayOrigin[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( world_ray_origin, loc );
		body->getSequence().push_back( CreateAppendToTrace( world_ray_origin, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_WorldRayDirection, location )"
	if ( auto* world_ray_dir = dbgInfo.GetCachedSymbolNode( RT_WorldRayDirection[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( world_ray_dir, loc );
		body->getSequence().push_back( CreateAppendToTrace( world_ray_dir, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_ObjectRayOrigin, location )"
	if ( auto* object_ray_origin = dbgInfo.GetCachedSymbolNode( RT_ObjectRayOrigin[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( object_ray_origin, loc );
		body->getSequence().push_back( CreateAppendToTrace( object_ray_origin, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_ObjectRayDirection, location )"
	if ( auto* object_ray_dir = dbgInfo.GetCachedSymbolNode( RT_ObjectRayDirection[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( object_ray_dir, loc );
		body->getSequence().push_back( CreateAppendToTrace( object_ray_dir, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_RayTmin, location )"
	if ( auto* ray_tmin = dbgInfo.GetCachedSymbolNode( RT_RayTmin[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( ray_tmin, loc );
		body->getSequence().push_back( CreateAppendToTrace( ray_tmin, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_RayTmax, location )"
	if ( auto* ray_tmax = dbgInfo.GetCachedSymbolNode( RT_RayTmax[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( ray_tmax, loc );
		body->getSequence().push_back( CreateAppendToTrace( ray_tmax, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_IncomingRayFlags, location )"
	if ( auto* incoming_ray_flags = dbgInfo.GetCachedSymbolNode( RT_IncomingRayFlags[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( incoming_ray_flags, loc );
		body->getSequence().push_back( CreateAppendToTrace( incoming_ray_flags, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_ObjectToWorld, location )"
	if ( auto* object_to_world = dbgInfo.GetCachedSymbolNode( RT_ObjectToWorld[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( object_to_world, loc );
		body->getSequence().push_back( CreateAppendToTrace( object_to_world, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_WorldToObject, location )"
	if ( auto* world_to_object = dbgInfo.GetCachedSymbolNode( RT_WorldToObject[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( world_to_object, loc );
		body->getSequence().push_back( CreateAppendToTrace( world_to_object, loc_id, dbgInfo ));
	}
	return body;
}

/*
=================================================
	RecordMissShaderInfo
=================================================
*/
static TIntermAggregate*  RecordMissShaderInfo (const TSourceLoc &loc, DebugInfo &dbgInfo)
{
	TIntermAggregate*	body = new TIntermAggregate{ TOperator::EOpSequence };
	body->setType( TType{EbtVoid} );
	
	// "dbg_AppendToTrace( gl_LaunchID, location )"
	{
		TIntermSymbol*	invocation	= dbgInfo.GetCachedSymbolNode( RT_LaunchID[dbgInfo.IsNVRT()] );
		const uint		loc_id		= dbgInfo.GetCustomSourceLocation( invocation, loc );

		body->getSequence().push_back( CreateAppendToTrace( invocation, loc_id, dbgInfo ));
	}
	
	// "dbg_AppendToTrace( gl_WorldRayOrigin, location )"
	if ( auto* world_ray_origin = dbgInfo.GetCachedSymbolNode( RT_WorldRayOrigin[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( world_ray_origin, loc );
		body->getSequence().push_back( CreateAppendToTrace( world_ray_origin, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_WorldRayDirection, location )"
	if ( auto* world_ray_direction = dbgInfo.GetCachedSymbolNode( RT_WorldRayDirection[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( world_ray_direction, loc );
		body->getSequence().push_back( CreateAppendToTrace( world_ray_direction, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_ObjectRayOrigin, location )"
	if ( auto* obj_ray_origin = dbgInfo.GetCachedSymbolNode( RT_ObjectRayOrigin[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( obj_ray_origin, loc );
		body->getSequence().push_back( CreateAppendToTrace( obj_ray_origin, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_ObjectRayDirection, location )"
	if ( auto* obj_ray_direction = dbgInfo.GetCachedSymbolNode( RT_ObjectRayDirection[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( obj_ray_direction, loc );
		body->getSequence().push_back( CreateAppendToTrace( obj_ray_direction, loc_id, dbgInfo ));
	}
	return body;
}

/*
=================================================
	RecordCallableShaderInfo
=================================================
*/
static TIntermAggregate*  RecordCallableShaderInfo (const TSourceLoc &loc, DebugInfo &dbgInfo)
{
	TIntermAggregate*	body = new TIntermAggregate{ TOperator::EOpSequence };
	body->setType( TType{EbtVoid} );
	
	// "dbg_AppendToTrace( gl_LaunchID, location )"
	{
		TIntermSymbol*	invocation	= dbgInfo.GetCachedSymbolNode( RT_LaunchID[dbgInfo.IsNVRT()] );
		const uint		loc_id		= dbgInfo.GetCustomSourceLocation( invocation, loc );

		body->getSequence().push_back( CreateAppendToTrace( invocation, loc_id, dbgInfo ));
	}

	// "dbg_AppendToTrace( gl_IncomingRayFlags, location )"
	if ( auto* incoming_ray_flags = dbgInfo.GetCachedSymbolNode( RT_IncomingRayFlags[dbgInfo.IsNVRT()] ))
	{
		const uint	loc_id = dbgInfo.GetCustomSourceLocation( incoming_ray_flags, loc );
		body->getSequence().push_back( CreateAppendToTrace( incoming_ray_flags, loc_id, dbgInfo ));
	}
	return body;
}

/*
=================================================
	RecordShaderInfo
=================================================
*/
static TIntermAggregate*  RecordShaderInfo (const TSourceLoc &loc, DebugInfo &dbgInfo)
{
	BEGIN_ENUM_CHECKS();
	switch ( dbgInfo.GetShaderType() )
	{
		case EShLangVertex :			return RecordVertexShaderInfo( loc, dbgInfo );
		case EShLangTessControl :		return RecordTessControlShaderInfo( loc, dbgInfo );
		case EShLangTessEvaluation :	return RecordTessEvaluationShaderInfo( loc, dbgInfo );
		case EShLangGeometry :			return RecordGeometryShaderInfo( loc, dbgInfo );
		case EShLangFragment :			return RecordFragmentShaderInfo( loc, dbgInfo );
		case EShLangTaskNV :
		case EShLangMeshNV :
		case EShLangCompute :			return RecordComputeShaderInfo( loc, dbgInfo );
		case EShLangRayGen :			return RecordRayGenShaderInfo( loc, dbgInfo );
		case EShLangAnyHit :
		case EShLangClosestHit :		return RecordHitShaderInfo( loc, dbgInfo );
		case EShLangIntersect :			return RecordIntersectionShaderInfo( loc, dbgInfo );
		case EShLangMiss :				return RecordMissShaderInfo( loc, dbgInfo );
		case EShLangCallable :			return RecordCallableShaderInfo( loc, dbgInfo );
		case EShLangCount :				break;
	}
	END_ENUM_CHECKS();
	return nullptr;
}

/*
=================================================
	CreateFragmentShaderIsDebugInvocation
=================================================
*/
static TIntermOperator*  CreateFragmentShaderIsDebugInvocation (DebugInfo &dbgInfo)
{
	TPublicType		bool_type;	bool_type.init({});
	bool_type.basicType			= TBasicType::EbtBool;
	bool_type.qualifier.storage	= TStorageQualifier::EvqTemporary;

	TPublicType		int_type;	int_type.init({});
	int_type.basicType			= TBasicType::EbtInt;
	int_type.qualifier.storage	= TStorageQualifier::EvqTemporary;
	
	TPublicType		float_type;	 float_type.init({});
	float_type.basicType		 = TBasicType::EbtFloat;
	float_type.qualifier.storage = TStorageQualifier::EvqTemporary;
	
	TPublicType		index_type;	 index_type.init({});
	index_type.basicType		 = TBasicType::EbtInt;
	index_type.qualifier.storage = TStorageQualifier::EvqConst;
	
	TIntermBinary*	eq1 = new TIntermBinary{ TOperator::EOpEqual };
	{
		// dbg_ShaderTrace.fragCoordX
		TIntermBinary*			fscoord_x	= dbgInfo.GetDebugStorageField( "fragCoordX" );
		CHECK_ERR( fscoord_x );

		// gl_FragCoord.x
		TConstUnionArray		x_index(1);	x_index[0].setIConst( 0 );
		TIntermConstantUnion*	x_field		= new TIntermConstantUnion{ x_index, TType{index_type} };
		TIntermBinary*			frag_x		= new TIntermBinary{ TOperator::EOpIndexDirect };
		frag_x->setType( TType{float_type} );
		frag_x->setLeft( dbgInfo.GetCachedSymbolNode( "gl_FragCoord" ));
		frag_x->setRight( x_field );

		// int(gl_FragCoord.x)
		TIntermUnary*			uint_fc_x	= new TIntermUnary{ TOperator::EOpConvFloatToInt };
		uint_fc_x->setType( TType{int_type} );
		uint_fc_x->setOperand( frag_x );

		// ... == ...
		eq1->setType( TType{bool_type} );
		eq1->setLeft( uint_fc_x );
		eq1->setRight( fscoord_x );
	}
	
	TIntermBinary*	eq2 = new TIntermBinary{ TOperator::EOpEqual };
	{
		// dbg_ShaderTrace.fragCoordY
		TIntermBinary*			fscoord_y	= dbgInfo.GetDebugStorageField( "fragCoordY" );
		CHECK_ERR( fscoord_y );

		// gl_FragCoord.y
		TConstUnionArray		y_index(1);	y_index[0].setIConst( 1 );
		TIntermConstantUnion*	y_field		= new TIntermConstantUnion{ y_index, TType{index_type} };
		TIntermBinary*			frag_y		= new TIntermBinary{ TOperator::EOpIndexDirect };
		frag_y->setType( TType{float_type} );
		frag_y->setLeft( dbgInfo.GetCachedSymbolNode( "gl_FragCoord" ));
		frag_y->setRight( y_field );

		// int(gl_FragCoord.y)
		TIntermUnary*			uint_fc_y	= new TIntermUnary{ TOperator::EOpConvFloatToInt };
		uint_fc_y->setType( TType{int_type} );
		uint_fc_y->setOperand( frag_y );
	
		// ... == ...
		eq2->setType( TType{bool_type} );
		eq2->setLeft( uint_fc_y );
		eq2->setRight( fscoord_y );
	}

	// ... && ...
	TIntermBinary*	cmp1		= new TIntermBinary{ TOperator::EOpLogicalAnd };
	cmp1->setType( TType{bool_type} );
	cmp1->setLeft( eq1 );
	cmp1->setRight( eq2 );

	return cmp1;
}

/*
=================================================
	CreateComputeShaderIsDebugInvocation
=================================================
*/
static TIntermOperator*  CreateComputeShaderIsDebugInvocation (DebugInfo &dbgInfo)
{
	TPublicType		bool_type;	bool_type.init({});
	bool_type.basicType			= TBasicType::EbtBool;
	bool_type.qualifier.storage	= TStorageQualifier::EvqTemporary;

	TPublicType		uint_type;	uint_type.init({});
	uint_type.basicType			= TBasicType::EbtUint;
	uint_type.qualifier.storage	= TStorageQualifier::EvqTemporary;

	TPublicType		index_type;	 index_type.init({});
	index_type.basicType		 = TBasicType::EbtInt;
	index_type.qualifier.storage = TStorageQualifier::EvqConst;

	TIntermBinary*	eq1 = new TIntermBinary{ TOperator::EOpEqual };
	{
		// dbg_ShaderTrace.globalInvocationX
		TIntermBinary*			thread_id_x	= dbgInfo.GetDebugStorageField( "globalInvocationX" );
		CHECK_ERR( thread_id_x );

		// gl_GlobalInvocationID.x
		TConstUnionArray		x_index(1);	x_index[0].setIConst( 0 );
		TIntermConstantUnion*	x_field		= new TIntermConstantUnion{ x_index, TType{index_type} };
		TIntermBinary*			ginvoc_x	= new TIntermBinary{ TOperator::EOpIndexDirect };
		ginvoc_x->setType( TType{uint_type} );
		ginvoc_x->setLeft( dbgInfo.GetCachedSymbolNode( "gl_GlobalInvocationID" ));
		ginvoc_x->setRight( x_field );
	
		// ... == ...
		eq1->setType( TType{bool_type} );
		eq1->setLeft( thread_id_x );
		eq1->setRight( ginvoc_x );
	}
	
	TIntermBinary*	eq2 = new TIntermBinary{ TOperator::EOpEqual };
	{
		// dbg_ShaderTrace.globalInvocationY
		TIntermBinary*			thread_id_y	= dbgInfo.GetDebugStorageField( "globalInvocationY" );
		CHECK_ERR( thread_id_y );

		// gl_GlobalInvocationID.y
		TConstUnionArray		y_index(1);	y_index[0].setIConst( 1 );
		TIntermConstantUnion*	y_field		= new TIntermConstantUnion{ y_index, TType{index_type} };
		TIntermBinary*			ginvoc_y	= new TIntermBinary{ TOperator::EOpIndexDirect };
		ginvoc_y->setType( TType{uint_type} );
		ginvoc_y->setLeft( dbgInfo.GetCachedSymbolNode( "gl_GlobalInvocationID" ));
		ginvoc_y->setRight( y_field );
	
		// ... == ...
		eq2->setType( TType{bool_type} );
		eq2->setLeft( thread_id_y );
		eq2->setRight( ginvoc_y );
	}
	
	TIntermBinary*	eq3 = new TIntermBinary{ TOperator::EOpEqual };
	{
		// dbg_ShaderTrace.globalInvocationZ
		TIntermBinary*			thread_id_z	= dbgInfo.GetDebugStorageField( "globalInvocationZ" );
		CHECK_ERR( thread_id_z );

		// gl_GlobalInvocationID.z
		TConstUnionArray		z_index(1);	z_index[0].setIConst( 2 );
		TIntermConstantUnion*	z_field		= new TIntermConstantUnion{ z_index, TType{index_type} };
		TIntermBinary*			ginvoc_z	= new TIntermBinary{ TOperator::EOpIndexDirect };
		ginvoc_z->setType( TType{uint_type} );
		ginvoc_z->setLeft( dbgInfo.GetCachedSymbolNode( "gl_GlobalInvocationID" ));
		ginvoc_z->setRight( z_field );
	
		// ... == ...
		eq3->setType( TType{bool_type} );
		eq3->setLeft( thread_id_z );
		eq3->setRight( ginvoc_z );
	}
	
	// ... && ...
	TIntermBinary*		cmp1		= new TIntermBinary{ TOperator::EOpLogicalAnd };
	cmp1->setType( TType{bool_type} );
	cmp1->setLeft( eq1 );
	cmp1->setRight( eq2 );
	
	// ... && ...
	TIntermBinary*		cmp2		= new TIntermBinary{ TOperator::EOpLogicalAnd };
	cmp2->setType( TType{bool_type} );
	cmp2->setLeft( cmp1 );
	cmp2->setRight( eq3 );

	return cmp2;
}

/*
=================================================
	CreateRayTracingShaderIsDebugInvocation
=================================================
*/
static TIntermOperator*  CreateRayTracingShaderIsDebugInvocation (DebugInfo &dbgInfo)
{	
	TPublicType		bool_type;	bool_type.init({});
	bool_type.basicType			= TBasicType::EbtBool;
	bool_type.qualifier.storage	= TStorageQualifier::EvqTemporary;

	TPublicType		uint_type;	uint_type.init({});
	uint_type.basicType			= TBasicType::EbtUint;
	uint_type.qualifier.storage	= TStorageQualifier::EvqTemporary;

	TPublicType		index_type;	 index_type.init({});
	index_type.basicType		 = TBasicType::EbtInt;
	index_type.qualifier.storage = TStorageQualifier::EvqConst;

	TIntermBinary*	eq1 = new TIntermBinary{ TOperator::EOpEqual };
	{
		// dbg_ShaderTrace.launchID_x
		TIntermBinary*			thread_id_x	= dbgInfo.GetDebugStorageField( "launchID_x" );
		CHECK_ERR( thread_id_x );

		// gl_LaunchID.x
		TConstUnionArray		x_index(1);	x_index[0].setIConst( 0 );
		TIntermConstantUnion*	x_field		= new TIntermConstantUnion{ x_index, TType{index_type} };
		TIntermBinary*			launch_x	= new TIntermBinary{ TOperator::EOpIndexDirect };
		launch_x->setType( TType{uint_type} );
		launch_x->setLeft( dbgInfo.GetCachedSymbolNode( RT_LaunchID[dbgInfo.IsNVRT()] ));
		launch_x->setRight( x_field );
	
		// ... == ...
		eq1->setType( TType{bool_type} );
		eq1->setLeft( thread_id_x );
		eq1->setRight( launch_x );
	}
	
	TIntermBinary*	eq2 = new TIntermBinary{ TOperator::EOpEqual };
	{
		// dbg_ShaderTrace.launchID_y
		TIntermBinary*			thread_id_y	= dbgInfo.GetDebugStorageField( "launchID_y" );
		CHECK_ERR( thread_id_y );

		// gl_LaunchID.y
		TConstUnionArray		y_index(1);	y_index[0].setIConst( 1 );
		TIntermConstantUnion*	y_field		= new TIntermConstantUnion{ y_index, TType{index_type} };
		TIntermBinary*			launch_y	= new TIntermBinary{ TOperator::EOpIndexDirect };
		launch_y->setType( TType{uint_type} );
		launch_y->setLeft( dbgInfo.GetCachedSymbolNode( RT_LaunchID[dbgInfo.IsNVRT()] ));
		launch_y->setRight( y_field );
	
		// ... == ...
		eq2->setType( TType{bool_type} );
		eq2->setLeft( thread_id_y );
		eq2->setRight( launch_y );
	}
	
	TIntermBinary*	eq3 = new TIntermBinary{ TOperator::EOpEqual };
	{
		// dbg_ShaderTrace.launchID_z
		TIntermBinary*			thread_id_z	= dbgInfo.GetDebugStorageField( "launchID_z" );
		CHECK_ERR( thread_id_z );

		// gl_LaunchID.z
		TConstUnionArray		z_index(1);	z_index[0].setIConst( 2 );
		TIntermConstantUnion*	z_field		= new TIntermConstantUnion{ z_index, TType{index_type} };
		TIntermBinary*			launch_z	= new TIntermBinary{ TOperator::EOpIndexDirect };
		launch_z->setType( TType{uint_type} );
		launch_z->setLeft( dbgInfo.GetCachedSymbolNode( RT_LaunchID[dbgInfo.IsNVRT()] ));
		launch_z->setRight( z_field );
	
		// ... == ...
		eq3->setType( TType{bool_type} );
		eq3->setLeft( thread_id_z );
		eq3->setRight( launch_z );
	}
	
	// ... && ...
	TIntermBinary*		cmp1		= new TIntermBinary{ TOperator::EOpLogicalAnd };
	cmp1->setType( TType{bool_type} );
	cmp1->setLeft( eq1 );
	cmp1->setRight( eq2 );
	
	// ... && ...
	TIntermBinary*		cmp2		= new TIntermBinary{ TOperator::EOpLogicalAnd };
	cmp2->setType( TType{bool_type} );
	cmp2->setLeft( cmp1 );
	cmp2->setRight( eq3 );

	return cmp2;
}

/*
=================================================
	InsertGlobalVariablesAndBuffers
=================================================
*/
static bool InsertGlobalVariablesAndBuffers (TIntermAggregate* linkerObjs, TIntermAggregate* globalVars, uint initialPosition, DebugInfo &dbgInfo)
{
	// "bool dbg_EnableRecording"
	TPublicType		type;	type.init({});
	type.basicType			= TBasicType::EbtBool;
	type.qualifier.storage	= TStorageQualifier::EvqGlobal;

	TIntermSymbol*			is_enable_recording = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "dbg_EnableRecording", TType{type} };
	dbgInfo.CacheSymbolNode( is_enable_recording );
	linkerObjs->getSequence().insert( linkerObjs->getSequence().begin(), is_enable_recording );
	
	// "dbg_EnableRecording = ..."
	TIntermBinary*			init_enable_recording = new TIntermBinary{ TOperator::EOpAssign };
	type.qualifier.storage = TStorageQualifier::EvqTemporary;
	init_enable_recording->setType( TType{type} );
	init_enable_recording->setLeft( is_enable_recording );
	globalVars->getSequence().insert( globalVars->getSequence().begin(), init_enable_recording );
	
	// "dbg_EnableRecording = <check invocation>"
	BEGIN_ENUM_CHECKS();
	switch ( dbgInfo.GetShaderType() )
	{
		case EShLangVertex :
		case EShLangTessControl :
		case EShLangTessEvaluation :
		case EShLangGeometry :
		case EShLangTaskNV :
		case EShLangMeshNV :
		{
			type.qualifier.storage	= TStorageQualifier::EvqConst;
			TConstUnionArray		false_value(1);	false_value[0].setBConst( false );
			TIntermConstantUnion*	const_false		= new TIntermConstantUnion{ false_value, TType{type} };
			
			init_enable_recording->setRight( const_false );
			break;
		}

		case EShLangFragment :		init_enable_recording->setRight( CreateFragmentShaderIsDebugInvocation( dbgInfo ));	break;
		case EShLangCompute :		init_enable_recording->setRight( CreateComputeShaderIsDebugInvocation( dbgInfo ));		break;
		
		case EShLangRayGen :
		case EShLangIntersect :
		case EShLangAnyHit :
		case EShLangClosestHit :
		case EShLangMiss :
		case EShLangCallable :		init_enable_recording->setRight( CreateRayTracingShaderIsDebugInvocation( dbgInfo ));		break;

		case EShLangCount :
		default :					RETURN_ERR( "not supported" );
	}
	END_ENUM_CHECKS();

	
	// "bool dbg_IsEnabled"
	type.qualifier.storage	= TStorageQualifier::EvqGlobal;
	TIntermSymbol*			is_debug_enabled = new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "dbg_IsEnabled", TType{type} };
	dbgInfo.CacheSymbolNode( is_debug_enabled );
	linkerObjs->getSequence().insert( linkerObjs->getSequence().begin(), is_debug_enabled );
	
	// "dbg_IsEnabled = false"
	TIntermBinary*			init_debug_enabled	= new TIntermBinary{ TOperator::EOpAssign };
	type.qualifier.storage = TStorageQualifier::EvqTemporary;
	init_debug_enabled->setType( TType{type} );
	init_debug_enabled->setLeft( is_debug_enabled );
	init_debug_enabled->setRight( is_enable_recording );
	globalVars->getSequence().insert( globalVars->getSequence().begin() + 1, init_debug_enabled );


	// "uint dbg_LastPosition"
	type.basicType			= TBasicType::EbtUint;
	type.qualifier.storage	= TStorageQualifier::EvqGlobal;

	TIntermSymbol*			last_pos		= new TIntermSymbol{ dbgInfo.GetUniqueSymbolID(), "dbg_LastPosition", TType{type} };
	dbgInfo.CacheSymbolNode( last_pos );
	linkerObjs->getSequence().insert( linkerObjs->getSequence().begin(), last_pos );
	
	// "dbg_LastPosition = ~0u"
	type.qualifier.storage = TStorageQualifier::EvqConst;
	TConstUnionArray		init_uvalue(1);	init_uvalue[0].setUConst( initialPosition );
	TIntermConstantUnion*	init_uconst		= new TIntermConstantUnion{ init_uvalue, TType{type} };
	TIntermBinary*			init_pos		= new TIntermBinary{ TOperator::EOpAssign };
		
	type.qualifier.storage = TStorageQualifier::EvqTemporary;
	init_pos->setType( TType{type} );
	init_pos->setLeft( last_pos );
	init_pos->setRight( init_uconst );
	globalVars->getSequence().insert( globalVars->getSequence().begin(), init_pos );
	

	linkerObjs->getSequence().insert( linkerObjs->getSequence().begin(), dbgInfo.GetDebugStorage() );
	return true;
}

/*
=================================================
	CreateEnableIfBody
=================================================
*/
static bool CreateEnableIfBody (TIntermAggregate* fnDecl, DebugInfo &dbgInfo)
{
	CHECK_ERR( fnDecl->getName() == "dbg_EnableTraceRecording(b1;" );
	CHECK_ERR( fnDecl->getSequence().size() >= 1 );
	
	auto*	params = fnDecl->getSequence()[0]->getAsAggregate();
	CHECK_ERR( params and params->getOp() == TOperator::EOpParameters );

	TIntermTyped*		arg		= params->getSequence()[0]->getAsTyped();
	CHECK_ERR( arg );

	TIntermAggregate*	body	= nullptr;

	if ( fnDecl->getSequence().size() == 1 )
	{
		body = new TIntermAggregate{ TOperator::EOpSequence };
		fnDecl->getSequence().push_back( body );
	}
	else
	{
		body = fnDecl->getSequence()[1]->getAsAggregate();
		CHECK_ERR( body );
	}
	
	TIntermSymbol*		is_enable_recording = dbgInfo.GetCachedSymbolNode( "dbg_EnableRecording" );
	TIntermSymbol*		is_debug_enabled	= dbgInfo.GetCachedSymbolNode( "dbg_IsEnabled" );
	TIntermAggregate*	branch_body			= RecordShaderInfo( TSourceLoc{}, dbgInfo );
	CHECK_ERR( is_enable_recording and is_debug_enabled and branch_body );

	TPublicType		type;	type.init({});
	type.basicType			= TBasicType::EbtBool;
	type.qualifier.storage	= TStorageQualifier::EvqTemporary;

	// "not dbg_EnableRecording"
	TIntermUnary*		condition	= new TIntermUnary{ TOperator::EOpLogicalNot };
	condition->setType( TType{type} );
	condition->setOperand( is_enable_recording );

	// "if ( not dbg_EnableRecording ) {...}"
	TIntermSelection*	selection	= new TIntermSelection{ arg, branch_body, nullptr };
	selection->setType( TType{EbtVoid} );
	body->getSequence().push_back( selection );

	// "dbg_EnableRecording = arg"
	TIntermBinary*	assign = new TIntermBinary{ TOperator::EOpAssign };
	assign->setType( TType{type} );
	assign->setLeft( is_enable_recording );
	assign->setRight( arg );
	branch_body->getSequence().insert( branch_body->getSequence().begin(), assign );
	
	// "dbg_IsEnabled = arg"
	assign = new TIntermBinary{ TOperator::EOpAssign };
	assign->setType( TType{type} );
	assign->setLeft( is_debug_enabled );
	assign->setRight( arg );
	branch_body->getSequence().insert( branch_body->getSequence().begin() + 1, assign );

	return true;
}

/*
=================================================
	CreatePauseBody
=================================================
*/
static bool  CreatePauseBody (TIntermAggregate* fnDecl, DebugInfo &dbgInfo)
{
	CHECK_ERR( fnDecl->getName() == "dbg_PauseTraceRecording(b1;" );
	CHECK_ERR( fnDecl->getSequence().size() >= 1 );
	
	auto*	params = fnDecl->getSequence()[0]->getAsAggregate();
	CHECK_ERR( params and params->getOp() == TOperator::EOpParameters );

	TIntermTyped*		arg		= params->getSequence()[0]->getAsTyped();
	CHECK_ERR( arg );

	TIntermAggregate*	body	= nullptr;

	if ( fnDecl->getSequence().size() == 1 )
	{
		body = new TIntermAggregate{ TOperator::EOpSequence };
		fnDecl->getSequence().push_back( body );
	}
	else
	{
		body = fnDecl->getSequence()[1]->getAsAggregate();
		CHECK_ERR( body );
	}
	
	TIntermSymbol*		is_debug_enabled = dbgInfo.GetCachedSymbolNode( "dbg_IsEnabled" );
	CHECK_ERR( is_debug_enabled );

	TPublicType		type;	type.init({});
	type.basicType			= TBasicType::EbtBool;
	type.qualifier.storage	= TStorageQualifier::EvqTemporary;
	
	// "not arg"
	TIntermUnary*		not_arg	= new TIntermUnary{ TOperator::EOpLogicalNot };
	not_arg->setType( TType{type} );
	not_arg->setOperand( arg );

	// "dbg_IsEnabled = not arg"
	TIntermBinary*	assign = new TIntermBinary{ TOperator::EOpAssign };
	assign->setType( TType{type} );
	assign->setLeft( is_debug_enabled );
	assign->setRight( not_arg );
	body->getSequence().push_back( assign );

	return true;
}

/*
=================================================
	CreateDebugTraceFunctions
=================================================
*/
static bool CreateDebugTraceFunctions (TIntermNode* root, uint initialPosition, DebugInfo &dbgInfo)
{
	TIntermAggregate*	aggr = root->getAsAggregate();
	CHECK_ERR( aggr );

	TIntermAggregate*	linker_objs	= nullptr;
	TIntermAggregate*	global_vars	= nullptr;
	
	for (auto& entry : aggr->getSequence())
	{
		if ( auto*  aggr2 = entry->getAsAggregate() )
		{
			if ( aggr2->getOp() == TOperator::EOpLinkerObjects )
				linker_objs = aggr2;

			if ( aggr2->getOp() == TOperator::EOpSequence )
				global_vars = aggr2;
		}
	}
	
	if ( not linker_objs ) {
		linker_objs = new TIntermAggregate{ TOperator::EOpLinkerObjects };
		aggr->getSequence().push_back( linker_objs );
	}

	if ( not global_vars ) {
		global_vars = new TIntermAggregate{ TOperator::EOpSequence };
		aggr->getSequence().push_back( global_vars );
	}

	CHECK_ERR( InsertGlobalVariablesAndBuffers( linker_objs, global_vars, initialPosition, dbgInfo ));

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

			auto*	shader_info = RecordShaderInfo( TSourceLoc{}, dbgInfo );
			CHECK_ERR( shader_info );

			body->getSequence().insert( body->getSequence().begin(), shader_info );
		}
		else
		if ( aggr2->getName().rfind( "dbg_EnableTraceRecording(", 0 ) == 0 )
		{
			CHECK_ERR( CreateEnableIfBody( INOUT aggr2, dbgInfo ));
		}
		else
		if ( aggr2->getName().rfind( "dbg_PauseTraceRecording(", 0 ) == 0 )
		{
			CHECK_ERR( CreatePauseBody( INOUT aggr2, dbgInfo ));
		}
	}

	for (auto& fn : dbgInfo.GetRequiredFunctions())
	{
		if ( fn == "dbg_AppendToTrace(u1;" )
		{
			auto*	body = CreateAppendToTraceBody2( dbgInfo );
			CHECK_ERR( body );

			aggr->getSequence().push_back( body );
		}
		else
		if ( fn.rfind( "dbg_AppendToTrace(", 0 ) == 0 )
		{
			auto*	body = CreateAppendToTraceBody( fn, dbgInfo );
			CHECK_ERR( body );

			aggr->getSequence().push_back( body );
		}
		else
			RETURN_ERR( "unknown function" );
	}
	return true;
}

/*
=================================================
	CreateAppendToTrace2
----
	'sourceLoc' - location index returned by DebugInfo::GetSourceLocation or ::GetCustomSourceLocation.
	also see 'CreateAppendToTraceBody2()'
=================================================
*/
static TIntermAggregate*  CreateAppendToTrace2 (uint sourceLoc, DebugInfo &dbgInfo)
{
	TIntermAggregate*	fcall = new TIntermAggregate( TOperator::EOpFunctionCall );

	fcall->setUserDefined();
	fcall->setName( TString{"dbg_AppendToTrace(u1;"} );
	fcall->setType( TType{ TBasicType::EbtVoid, TStorageQualifier::EvqGlobal });
	fcall->getQualifierList().push_back( TStorageQualifier::EvqConstReadOnly );
	
	TPublicType		uint_type;		uint_type.init({});
	uint_type.basicType				= TBasicType::EbtUint;
	uint_type.qualifier.storage		= TStorageQualifier::EvqConst;

	TConstUnionArray		loc_value(1);	loc_value[0].setUConst( sourceLoc );
	TIntermConstantUnion*	loc_const		= new TIntermConstantUnion{ loc_value, TType{uint_type} };
	
	loc_const->setLoc({});
	fcall->getSequence().push_back( loc_const );

	dbgInfo.RequestFunc( fcall->getName() );

	return fcall;
}

/*
=================================================
	CreateAppendToTrace
----
	returns new node instead of 'exprNode'.
	'exprNode' - any operator.
	'sourceLoc' - location index returned by DebugInfo::GetSourceLocation or ::GetCustomSourceLocation.
	also see 'CreateAppendToTraceBody()'
=================================================
*/
static TIntermAggregate*  CreateAppendToTrace (TIntermTyped* exprNode, uint sourceLoc, DebugInfo &dbgInfo)
{
	TIntermAggregate*	fcall		= new TIntermAggregate( TOperator::EOpFunctionCall );
	string			type_name;
	const TType &		type		= exprNode->getType();

	BEGIN_ENUM_CHECKS();
	switch ( type.getBasicType() )
	{
		case TBasicType::EbtFloat :		type_name = "f";	break;
		case TBasicType::EbtInt :		type_name = "i";	break;
		case TBasicType::EbtUint :		type_name = "u";	break;
		case TBasicType::EbtBool :		type_name = "b";	break;
		case TBasicType::EbtDouble :	type_name = "d";	break;
		case TBasicType::EbtInt64 :		type_name = "i64";	break;
		case TBasicType::EbtUint64 :	type_name = "u64";	break;
			
		case TBasicType::EbtSampler :	return nullptr;
		case TBasicType::EbtStruct :	return nullptr;

		case TBasicType::EbtVoid :
		case TBasicType::EbtFloat16 :
		case TBasicType::EbtInt8 :
		case TBasicType::EbtUint8 :
		case TBasicType::EbtInt16 :
		case TBasicType::EbtUint16 :
		case TBasicType::EbtAtomicUint :
		case TBasicType::EbtBlock :
		case TBasicType::EbtString :
		case TBasicType::EbtReference :
		case TBasicType::EbtNumTypes :
		case TBasicType::EbtAccStruct :
		case TBasicType::EbtRayQuery :
		default :						RETURN_ERR( "not supported" );
	}
	END_ENUM_CHECKS();
	
	if ( type.isArray() )
		return nullptr;	//RETURN_ERR( "arrays is not supported yet" )
	else
	if ( type.isMatrix() )
		type_name = "m" + type_name + to_string(type.getMatrixCols()) + to_string(type.getMatrixRows());
	else
	if ( type.isVector() )
		type_name = "v" + type_name + to_string(type.getVectorSize());
	else
	if ( type.isScalarOrVec1() )
		type_name += "1";
	else
		RETURN_ERR( "unknown type" );


	fcall->setLoc( exprNode->getLoc() );
	fcall->setUserDefined();
	fcall->setName( TString{"dbg_AppendToTrace("} + TString{type_name.c_str()} + ";u1;" );
	fcall->setType( exprNode->getType() );
	fcall->getQualifierList().push_back( TStorageQualifier::EvqConstReadOnly );
	fcall->getQualifierList().push_back( TStorageQualifier::EvqConstReadOnly );
	fcall->getSequence().push_back( exprNode );
	
	TPublicType		uint_type;		uint_type.init( exprNode->getLoc() );
	uint_type.basicType				= TBasicType::EbtUint;
	uint_type.qualifier.storage		= TStorageQualifier::EvqConst;

	TConstUnionArray		loc_value(1);	loc_value[0].setUConst( sourceLoc );
	TIntermConstantUnion*	loc_const		= new TIntermConstantUnion{ loc_value, TType{uint_type} };
	
	loc_const->setLoc( exprNode->getLoc() );
	fcall->getSequence().push_back( loc_const );

	dbgInfo.RequestFunc( fcall->getName() );

	return fcall;
}

/*
=================================================
	ProcessFunctionCall
----
	log out/inout parameters and returned value
=================================================
*/
static void ProcessFunctionCall (TIntermOperator* node, DebugInfo &dbgInfo)
{
	bool	is_builtin		= IsBuiltinFunction( node->getOp() );
	bool	is_user_defined	= (node->getOp() == TOperator::EOpFunctionCall);
	bool	is_debug		= IsDebugFunction( node );

	if ( not (is_builtin or is_user_defined) or is_debug )
		return;
	
	/*bool	has_output = false;
	for (auto& qual : node->getQualifierList()) {
		if ( qual == TStorageQualifier::EvqOut or qual == TStorageQualifier::EvqInOut )
			has_output = true;
	}*/

	// record returned value
	if ( node->getType().getBasicType() == TBasicType::EbtVoid or
		 node->getType().isScalarOrVec1() or node->getType().isVector() or node->getType().isMatrix() )
	{
		// this is location at the end of the function call
		TSourceLoc	loc = node->getLoc();

		// try to find location at the begin of the function call
		if ( dbgInfo.GetCallStack().size() > 1 and
			 node->getType().getBasicType() != TBasicType::EbtVoid )
		{
			TIntermNode*	temp = (dbgInfo.GetCallStack().end()-2)->node;

			if ( temp->getLoc() != TSourceLoc{} and temp->getLoc() < loc )
				loc = temp->getLoc();

			// pattern: "value = FunctionCall(...)"
			// returned value will be recorded later
			if ( TIntermOperator* op = temp->getAsOperator(); op and op->getOp() == TOperator::EOpAssign )
				return;

#		ifdef HIGH_DETAIL_TRACE
			// pattern: "return FunctionCall(...)"
			// returned value will be recorded later
			if ( TIntermBranch* branch = temp->getAsBranchNode(); branch and branch->getFlowOp() == TOperator::EOpReturn )
				return;
#		endif
		}

		// temporary fix
		if ( loc == node->getLoc() )
			loc.column = 0;
		
		const uint	loc_id = dbgInfo.GetSourceLocation( node, loc );

		if ( node->getType().getBasicType() == TBasicType::EbtVoid )
		{
			TIntermAggregate*	temp = new TIntermAggregate{ TOperator::EOpSequence };
			temp->getSequence().push_back( node );
			temp->getSequence().push_back( CreateAppendToTrace2( loc_id, dbgInfo ));
			dbgInfo.InjectNode( temp );
		}
		else
			dbgInfo.InjectNode( CreateAppendToTrace( node, loc_id, dbgInfo ));
	}
}

/*
=================================================
	RecursiveProcessAggregateNode
=================================================
*/
static bool RecursiveProcessAggregateNode (TIntermAggregate* aggr, DebugInfo &dbgInfo)
{
	dbgInfo.Enter( aggr );

	for (auto& node : aggr->getSequence())
	{
		CHECK_ERR( RecursiveProcessNode( node, dbgInfo ));
		
		if ( auto* inj = dbgInfo.GetInjection() )
			node = inj;
	}
	
	ProcessFunctionCall( aggr, dbgInfo );

	if ( aggr->getOp() == TOperator::EOpFunction and
		 aggr->getSequence().size() == 2 )
	{
		TIntermAggregate*	parameters	= aggr->getSequence()[0]->getAsAggregate();
		TIntermAggregate*	body		= aggr->getSequence()[1]->getAsAggregate();
		CHECK_ERR( parameters and body );

		auto		begin_iter	= body->getSequence().begin();
		TSourceLoc	loc			= aggr->getLoc();
		loc.column = 0;	// TODO

		for (auto& arg : parameters->getSequence())
		{
			TIntermSymbol*		symb	= arg->getAsSymbolNode();
			TStorageQualifier	qual	= symb->getQualifier().storage;

			if ( qual == TStorageQualifier::EvqConstReadOnly or
				 qual == TStorageQualifier::EvqIn			 or
				 qual == TStorageQualifier::EvqInOut )
			{
				if ( auto* fncall = CreateAppendToTrace( symb, dbgInfo.GetSourceLocation( symb, loc ), dbgInfo ))
					begin_iter = body->getSequence().insert( begin_iter, fncall );
			}
		}
	}

	const auto	loc = dbgInfo.GetCurrentLocation();
	dbgInfo.Leave( aggr );

	if ( aggr->getOp() >= TOperator::EOpNegative	 or
		 aggr->getOp() == TOperator::EOpFunctionCall or
		 aggr->getOp() == TOperator::EOpParameters	 )
	{
		// propagate source location to root
		dbgInfo.AddLocation( loc );
	}
	return true;
}

/*
=================================================
	RecursiveProcessUnaryNode
=================================================
*/
static bool RecursiveProcessUnaryNode (TIntermUnary* unary, DebugInfo &dbgInfo)
{
	dbgInfo.Enter( unary );

	CHECK_ERR( RecursiveProcessNode( unary->getOperand(), dbgInfo ));
	
	if ( auto* inj = dbgInfo.GetInjection() )
		unary->setOperand( inj );
	
	switch ( unary->getOp() )
	{
		case TOperator::EOpPreIncrement :
		case TOperator::EOpPostIncrement :	// TODO: log as result+1
		case TOperator::EOpPreDecrement :
		case TOperator::EOpPostDecrement :	// TODO: log as result-1
		{
			dbgInfo.InjectNode( CreateAppendToTrace( unary, dbgInfo.GetSourceLocation( unary->getOperand(), unary->getOperand()->getLoc() ), dbgInfo ));
			break;
		}
		default :
			ProcessFunctionCall( unary, dbgInfo );
			break;
	}

	const auto	loc = dbgInfo.GetCurrentLocation();
	dbgInfo.Leave( unary );

	// propagate source location to root
	dbgInfo.AddLocation( loc );
	return true;
}

/*
=================================================
	RecursiveProcessBinaryNode
=================================================
*/
static bool RecursiveProcessBinaryNode (TIntermBinary* binary, DebugInfo &dbgInfo)
{
	dbgInfo.Enter( binary );

	CHECK_ERR( RecursiveProcessNode( binary->getLeft(), dbgInfo ));

	if ( auto* inj = dbgInfo.GetInjection() )
		binary->setLeft( inj );

	CHECK_ERR( RecursiveProcessNode( binary->getRight(), dbgInfo ));

	if ( auto* inj = dbgInfo.GetInjection() )
		binary->setRight( inj );

	switch ( binary->getOp() )
	{
		case TOperator::EOpAssign :
		case TOperator::EOpAddAssign :
		case TOperator::EOpSubAssign :
		case TOperator::EOpMulAssign :
		case TOperator::EOpDivAssign :
		case TOperator::EOpModAssign :
		case TOperator::EOpAndAssign :
		case TOperator::EOpInclusiveOrAssign :
		case TOperator::EOpExclusiveOrAssign :
		case TOperator::EOpLeftShiftAssign :
		case TOperator::EOpRightShiftAssign :
		{
			dbgInfo.InjectNode( CreateAppendToTrace( binary, dbgInfo.GetSourceLocation( binary->getLeft(), binary->getLeft()->getLoc() ), dbgInfo ));
			break;
		}
		default :
			ProcessFunctionCall( binary, dbgInfo );
			break;
	}

	const auto	loc = dbgInfo.GetCurrentLocation();
	dbgInfo.Leave( binary );

	// propagate source location to root
	dbgInfo.AddLocation( loc );
	return true;
}

/*
=================================================
	RecursiveProcessBranchNode
=================================================
*/
static bool RecursiveProcessBranchNode (TIntermBranch* branch, DebugInfo &dbgInfo)
{
	dbgInfo.Enter( branch );

	const TSourceLoc	loc = branch->getLoc();

	CHECK_ERR( RecursiveProcessNode( branch->getExpression(), dbgInfo ));

	if ( auto* inj = dbgInfo.GetInjection() )
	{
		new(branch) TIntermBranch{ branch->getFlowOp(), inj };
		branch->setLoc( loc );
	}
	
#ifdef HIGH_DETAIL_TRACE
	else
	if ( branch->getFlowOp() == TOperator::EOpCase or
		 branch->getFlowOp() == TOperator::EOpDefault )
	{}
	else
	if ( auto* expr = branch->getExpression(); expr and not expr->getAsConstantUnion() )
	{
		if ( auto* fncall = CreateAppendToTrace( expr, dbgInfo.GetSourceLocation( expr, branch->getLoc() ), dbgInfo ))
		{
			new(branch) TIntermBranch{ branch->getFlowOp(), fncall };
			branch->setLoc( loc );
		}
	}
	else
	{
		if ( auto* fncall = CreateAppendToTrace2( dbgInfo.GetSourceLocation( branch, branch->getLoc() ), dbgInfo ))
		{
			TIntermAggregate*	temp = new TIntermAggregate{ TOperator::EOpSequence };
			dbgInfo.InjectNode( temp );
			
			temp->getSequence().push_back( fncall );
			temp->getSequence().push_back( branch );
		}
	}
#endif

	dbgInfo.Leave( branch );
	return true;
}

/*
=================================================
	ReplaceIntermSwitch
=================================================
*/
static void ReplaceIntermSwitch (INOUT TIntermSwitch* sw, TIntermTyped* cond, TIntermAggregate* b)
{
	const bool			is_flatten		= sw->getFlatten();
	const bool			dont_flatten	= sw->getDontFlatten();
	const TSourceLoc	loc				= sw->getLoc();

	new(sw) TIntermSwitch{ cond, b };
	
	sw->setLoc( loc );

	if ( is_flatten )	sw->setFlatten();
	if ( dont_flatten )	sw->setDontFlatten();
}

/*
=================================================
	RecursiveProcessSwitchNode
=================================================
*/
static bool RecursiveProcessSwitchNode (TIntermSwitch* sw, DebugInfo &dbgInfo)
{
	dbgInfo.Enter( sw );

	CHECK_ERR( RecursiveProcessNode( sw->getCondition(), dbgInfo ));
	
	if ( auto* inj = dbgInfo.GetInjection() )
		ReplaceIntermSwitch( INOUT sw, inj, sw->getBody() );
	
#ifdef HIGH_DETAIL_TRACE
	else
	if ( not sw->getCondition()->getAsConstantUnion() and sw->getCondition()->getAsTyped() )
	{
		auto*	cond	= sw->getCondition()->getAsTyped();
		auto*	fncall	= CreateAppendToTrace( cond, dbgInfo.GetSourceLocation( cond, cond->getLoc() ), dbgInfo );

		if ( fncall )
			ReplaceIntermSwitch( INOUT sw, fncall, sw->getBody() );
	}
#endif

	CHECK_ERR( RecursiveProcessNode( sw->getBody(), dbgInfo ));
	
	if ( auto* inj = dbgInfo.GetInjection() )
		ReplaceIntermSwitch( INOUT sw, sw->getCondition()->getAsTyped(), inj->getAsAggregate() );

	dbgInfo.Leave( sw );
	return true;
}

/*
=================================================
	ReplaceIntermSelection
=================================================
*/
static void ReplaceIntermSelection (INOUT TIntermSelection *selection, TIntermTyped* cond, TIntermNode* trueB, TIntermNode* falseB)
{
	const bool			is_flatten			= selection->getFlatten();
	const bool			dont_flatten		= selection->getDontFlatten();
	const bool			is_short_circuit	= selection->getShortCircuit();
	const TSourceLoc	loc					= selection->getLoc();
	TType				type;				type.shallowCopy( selection->getType() );

	new(selection) TIntermSelection{ cond, trueB, falseB, type };

	selection->setLoc( loc );
	
	if ( is_flatten )			selection->setFlatten();
	if ( dont_flatten )			selection->setDontFlatten();
	if ( not is_short_circuit )	selection->setNoShortCircuit();
}

/*
=================================================
	RecursiveProcessSelectionNode
=================================================
*/
static bool RecursiveProcessSelectionNode (TIntermSelection* selection, DebugInfo &dbgInfo)
{
	dbgInfo.Enter( selection );

	CHECK_ERR( RecursiveProcessNode( selection->getCondition(), dbgInfo ));
	
	if ( auto* inj = dbgInfo.GetInjection() )
		ReplaceIntermSelection( INOUT selection, inj, selection->getTrueBlock(), selection->getFalseBlock() );

#ifdef HIGH_DETAIL_TRACE
	else
	if ( not selection->getCondition()->getAsConstantUnion() )
	{
		auto*	cond	= selection->getCondition();
		auto*	fncall	= CreateAppendToTrace( cond, dbgInfo.GetSourceLocation( cond, cond->getLoc() ), dbgInfo );

		if ( fncall )
			ReplaceIntermSelection( INOUT selection, fncall, selection->getTrueBlock(), selection->getFalseBlock() );
	}
#endif

	CHECK_ERR( RecursiveProcessNode( selection->getTrueBlock(), dbgInfo ));
	
	if ( auto* inj = dbgInfo.GetInjection() )
		ReplaceIntermSelection( INOUT selection, selection->getCondition(), inj, selection->getFalseBlock() );

	CHECK_ERR( RecursiveProcessNode( selection->getFalseBlock(), dbgInfo ));
	
	if ( auto* inj = dbgInfo.GetInjection() )
		ReplaceIntermSelection( INOUT selection, selection->getCondition(), selection->getTrueBlock(), inj );
		
	dbgInfo.Leave( selection );
	return true;
}

/*
=================================================
	ReplaceIntermLoop
=================================================
*/
static void ReplaceIntermLoop (INOUT TIntermLoop* loop, TIntermNode* aBody, TIntermTyped* aTest, TIntermTyped* aTerminal)
{
	const bool			test_first	= loop->testFirst();
	const bool			is_unroll	= loop->getUnroll();
	const bool			dont_unroll	= loop->getDontUnroll();
	const int			dependency	= loop->getLoopDependency();
	const TSourceLoc	loc			= loop->getLoc();

	new(loop) TIntermLoop{ aBody, aTest, aTerminal, test_first };

	if ( is_unroll )		loop->setUnroll();
	if ( dont_unroll )		loop->setDontUnroll();

	loop->setLoopDependency( dependency );
	loop->setLoc( loc );
}

/*
=================================================
	RecursiveProccessLoop
=================================================
*/
static bool RecursiveProccessLoop (TIntermLoop* loop, DebugInfo &dbgInfo)
{
	dbgInfo.Enter( loop );

	CHECK_ERR( RecursiveProcessNode( loop->getBody(), dbgInfo ));
	
	if ( auto* inj = dbgInfo.GetInjection() )
		ReplaceIntermLoop( INOUT loop, inj, loop->getTest(), loop->getTerminal() );

	if ( loop->getTerminal() )
	{
		CHECK_ERR( RecursiveProcessNode( loop->getTerminal(), dbgInfo ));
		
		if ( auto* inj = dbgInfo.GetInjection() )
			ReplaceIntermLoop( INOUT loop, loop->getBody(), loop->getTest(), inj );
	}

	if ( loop->getTest() )
	{
		CHECK_ERR( RecursiveProcessNode( loop->getTest(), dbgInfo ));
		
		if ( auto* inj = dbgInfo.GetInjection() )
			ReplaceIntermLoop( INOUT loop, loop->getBody(), inj, loop->getTerminal() );

#ifdef HIGH_DETAIL_TRACE
		else
		{
			auto*	test	= loop->getTest();
			auto*	fncall	= CreateAppendToTrace( test, dbgInfo.GetSourceLocation( test, test->getLoc() ), dbgInfo );

			if ( fncall )
				ReplaceIntermLoop( INOUT loop, loop->getBody(), fncall, loop->getTerminal() );
		}
#endif
	}
	
	dbgInfo.Leave( loop );
	return true;
}

/*
=================================================
	ProcessSymbolNode
=================================================
*/
static bool ProcessSymbolNode (TIntermSymbol* node, DebugInfo &dbgInfo)
{
	dbgInfo.AddSymbol( node );

	if ( // fragment shader
		 node->getName() == "gl_FragCoord"				or
		 node->getName() == "gl_SampleID"				or
		 node->getName() == "gl_PrimitiveID"			or
		 node->getName() == "gl_SamplePosition"			or
		 node->getName() == "gl_Layer"					or
		 node->getName() == "gl_ViewportIndex"			or
		 node->getName() == "gl_FrontFacing"			or
		 node->getName() == "gl_HelperInvocation"		or
		 // vertex shader
		 node->getName() == "gl_VertexIndex"			or
		 node->getName() == "gl_VertexID"				or
		 node->getName() == "gl_InstanceIndex"			or
		 node->getName() == "gl_InstanceID"				or
		 node->getName() == "gl_DrawIDARB"				or		// requires GL_ARB_shader_draw_parameters
		 node->getName() == "gl_DrawID"					or		// requires version 460
		 // geometry shader
		 node->getName() == "gl_InvocationID"			or
		 node->getName() == "gl_PrimitiveIDIn"			or
		 // tessellation
		 //		reuse 'gl_InvocationID'
		//		reuse 'gl_PrimitiveID'
		 node->getName() == "gl_PatchVerticesIn"		or
		 node->getName() == "gl_TessCoord"				or
		 node->getName() == "gl_TessLevelInner"			or
		 node->getName() == "gl_TessLevelOuter"			or
		 // compute shader
		 node->getName() == "gl_GlobalInvocationID"		or
		 node->getName() == "gl_LocalInvocationID"		or
		 node->getName() == "gl_LocalInvocationIndex"	or
		 node->getName() == "gl_WorkGroupID"			or
		 node->getName() == "gl_NumWorkGroups"			or
		 node->getName() == "gl_WorkGroupSize"			or
		 node->getName() == "gl_SubgroupID"				or
		 // task & mesh shader
		 //		reuse 'gl_GlobalInvocationID'
		 //		reuse 'gl_LocalInvocationID'
		 //		reuse 'gl_LocalInvocationIndex'
		 //		reuse 'gl_WorkGroupID'
		 //		reuse 'gl_NumWorkGroups'
		 //		reuse 'gl_WorkGroupSize'
		 node->getName() == "gl_MeshViewCountNV"		or
		 // ray generation shader
		 node->getName() == RT_LaunchID[dbgInfo.IsNVRT()]			or
		 node->getName() == RT_LaunchSize[dbgInfo.IsNVRT()]			or
		 // ray intersection & any-hit & closest-hit & miss shaders
		 //		reuse 'gl_LaunchID'
		 //		reuse 'gl_LaunchSize'
		 //		reuse 'gl_PrimitiveID'
		 node->getName() == RT_InstanceCustomIndex[dbgInfo.IsNVRT()]	or
		 node->getName() == RT_WorldRayOrigin[dbgInfo.IsNVRT()]			or
		 node->getName() == RT_WorldRayDirection[dbgInfo.IsNVRT()]		or
		 node->getName() == RT_ObjectRayOrigin[dbgInfo.IsNVRT()]		or
		 node->getName() == RT_ObjectRayDirection[dbgInfo.IsNVRT()]		or
		 node->getName() == RT_RayTmin[dbgInfo.IsNVRT()]				or
		 node->getName() == RT_RayTmax[dbgInfo.IsNVRT()]				or
		 node->getName() == RT_IncomingRayFlags[dbgInfo.IsNVRT()]		or
		 node->getName() == RT_ObjectToWorld[dbgInfo.IsNVRT()]			or
		 node->getName() == RT_WorldToObject[dbgInfo.IsNVRT()]			or
		 // ray intersection & any-hit & closest-hit shaders
		 node->getName() == RT_HitT[dbgInfo.IsNVRT()]			or
		 node->getName() == RT_HitKind[dbgInfo.IsNVRT()]		or
		 node->getName() == RT_InstanceID[dbgInfo.IsNVRT()]		or
		 // all shaders
		 node->getName() == "gl_SubgroupInvocationID"	)
	{
		dbgInfo.CacheSymbolNode( node );
		return true;
	}

	// do nothing
	return true;
}

}	// namespace
//-----------------------------------------------------------------------------


/*
=================================================
	InsertTraceRecording
=================================================
*/
bool ShaderTrace::InsertTraceRecording (TIntermediate &intermediate, uint descSetIndex)
{
	CHECK_ERR( intermediate.getNumEntryPoints() == 1 );

	DebugInfo		dbg_info{ intermediate, OUT _exprLocations, _fileMap };

	TIntermNode*	root = intermediate.getTreeRoot();
	CHECK_ERR( root );

	_initialPosition = uint(CombineHash( std::hash<void*>{}( this ), descSetIndex+1 ));
	ASSERT( _initialPosition != 0 );
	_initialPosition |= InitialPositionMask;

	CreateShaderDebugStorage( descSetIndex, dbg_info, OUT _posOffset, OUT _dataOffset );

	dbg_info.Enter( root );
	{
		CHECK_ERR( RecursiveProcessNode( root, dbg_info ));
		CHECK( not dbg_info.GetInjection() );

		CreateShaderBuiltinSymbols( root, dbg_info );

		CHECK_ERR( CreateDebugTraceFunctions( root, _initialPosition, dbg_info ));
	}
	dbg_info.Leave( root );
	dbg_info.PostProcess( OUT _varNames );
	
	ValidateInterm( intermediate );
	return true;
}
