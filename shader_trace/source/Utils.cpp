// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#include "Common.h"

using namespace glslang;

/*
=================================================
	GetFunctionName
=================================================
*/
string  GetFunctionName (TIntermOperator *op)
{
	if ( TIntermAggregate* aggr = op->getAsAggregate(); aggr and aggr->getOp() == TOperator::EOpFunctionCall )
	{
		auto	pos = aggr->getName().find( '(' );
		return pos != TString::npos ? aggr->getName().substr( 0, pos ).c_str() : aggr->getName().c_str();
	}

#ifdef HIGH_DETAIL_TRACE
	string	suffix;
	if ( op->getOp() >= TOperator::EOpConvInt8ToBool and op->getOp() <= TOperator::EOpConvDoubleToFloat )
	{
		TType const&	type = op->getType();

		if ( type.isVector() )
			suffix = "vec" + to_string( type.getVectorSize() );
		else
		if ( type.isMatrix() )
			suffix = "mat" + to_string( type.getMatrixCols() ) + "x" + to_string( type.getMatrixRows() );
	}
#endif

	//BEGIN_ENUM_CHECKS();
	switch ( op->getOp() )
	{
#	if 0 //def HIGH_DETAIL_TRACE
		case TOperator::EOpConvInt8ToBool :
		case TOperator::EOpConvUint8ToBool :
		case TOperator::EOpConvInt16ToBool :
		case TOperator::EOpConvUint16ToBool :
		case TOperator::EOpConvIntToBool :
		case TOperator::EOpConvUintToBool :
		case TOperator::EOpConvInt64ToBool :
		case TOperator::EOpConvUint64ToBool :
		case TOperator::EOpConvFloat16ToBool :
		case TOperator::EOpConvFloatToBool :
		case TOperator::EOpConvDoubleToBool :  return suffix.size() ? "b" + suffix : "bool";

		case TOperator::EOpConvBoolToInt :
		case TOperator::EOpConvInt8ToInt :
		case TOperator::EOpConvUint8ToInt :
		case TOperator::EOpConvInt16ToInt :
		case TOperator::EOpConvUint16ToInt :
		case TOperator::EOpConvUintToInt :
		case TOperator::EOpConvInt64ToInt :
		case TOperator::EOpConvUint64ToInt :
		case TOperator::EOpConvFloat16ToInt :
		case TOperator::EOpConvFloatToInt :
		case TOperator::EOpConvDoubleToInt : return suffix.size() ? "i" + suffix : "int";
			
		case TOperator::EOpConvBoolToInt8 :
		case TOperator::EOpConvUint8ToInt8 :
		case TOperator::EOpConvInt16ToInt8 :
		case TOperator::EOpConvUint16ToInt8 :
		case TOperator::EOpConvIntToInt8 :
		case TOperator::EOpConvUintToInt8 :
		case TOperator::EOpConvInt64ToInt8 :
		case TOperator::EOpConvUint64ToInt8 :
		case TOperator::EOpConvFloat16ToInt8 :
		case TOperator::EOpConvFloatToInt8 :
		case TOperator::EOpConvDoubleToInt8 : return suffix.size() ? "i8" + suffix : "int8_t";

		case TOperator::EOpConvBoolToUint8 :
		case TOperator::EOpConvInt8ToUint8 :
		case TOperator::EOpConvInt16ToUint8 :
		case TOperator::EOpConvUint16ToUint8 :
		case TOperator::EOpConvIntToUint8 :
		case TOperator::EOpConvUintToUint8 :
		case TOperator::EOpConvInt64ToUint8 :
		case TOperator::EOpConvUint64ToUint8 :
		case TOperator::EOpConvFloat16ToUint8 :
		case TOperator::EOpConvFloatToUint8 :
		case TOperator::EOpConvDoubleToUint8 : return suffix.size() ? "u8" + suffix : "uint8_t";
			
		case TOperator::EOpConvBoolToInt16 :
		case TOperator::EOpConvInt8ToInt16 :
		case TOperator::EOpConvUint8ToInt16 :
		case TOperator::EOpConvUint16ToInt16 :
		case TOperator::EOpConvIntToInt16 :
		case TOperator::EOpConvUintToInt16 :
		case TOperator::EOpConvInt64ToInt16 :
		case TOperator::EOpConvUint64ToInt16 :
		case TOperator::EOpConvFloat16ToInt16 :
		case TOperator::EOpConvFloatToInt16 :
		case TOperator::EOpConvDoubleToInt16 : return suffix.size() ? "i16" + suffix : "uint16_t";

		case TOperator::EOpConvBoolToUint16 :
		case TOperator::EOpConvInt8ToUint16 :
		case TOperator::EOpConvUint8ToUint16 :
		case TOperator::EOpConvInt16ToUint16 :
		case TOperator::EOpConvIntToUint16 :
		case TOperator::EOpConvUintToUint16 :
		case TOperator::EOpConvInt64ToUint16 :
		case TOperator::EOpConvUint64ToUint16 :
		case TOperator::EOpConvFloat16ToUint16 :
		case TOperator::EOpConvFloatToUint16 :
		case TOperator::EOpConvDoubleToUint16 : return suffix.size() ? "u16" + suffix : "uint16_t";
			
		case TOperator::EOpConvBoolToUint :
		case TOperator::EOpConvInt8ToUint :
		case TOperator::EOpConvUint8ToUint :
		case TOperator::EOpConvInt16ToUint :
		case TOperator::EOpConvUint16ToUint :
		case TOperator::EOpConvIntToUint :
		case TOperator::EOpConvInt64ToUint :
		case TOperator::EOpConvUint64ToUint :
		case TOperator::EOpConvFloat16ToUint :
		case TOperator::EOpConvFloatToUint :
		case TOperator::EOpConvDoubleToUint : return suffix.size() ? "u" + suffix : "uint";
			
		case TOperator::EOpConvBoolToInt64 :
		case TOperator::EOpConvInt8ToInt64 :
		case TOperator::EOpConvUint8ToInt64 :
		case TOperator::EOpConvInt16ToInt64 :
		case TOperator::EOpConvUint16ToInt64 :
		case TOperator::EOpConvIntToInt64 :
		case TOperator::EOpConvUintToInt64 :
		case TOperator::EOpConvUint64ToInt64 :
		case TOperator::EOpConvFloat16ToInt64 :
		case TOperator::EOpConvFloatToInt64 :
		case TOperator::EOpConvDoubleToInt64 : return suffix.size() ? "i64" + suffix : "int64_t";

		case TOperator::EOpConvBoolToUint64 :
		case TOperator::EOpConvInt8ToUint64 :
		case TOperator::EOpConvUint8ToUint64 :
		case TOperator::EOpConvInt16ToUint64 :
		case TOperator::EOpConvUint16ToUint64 :
		case TOperator::EOpConvIntToUint64 :
		case TOperator::EOpConvUintToUint64 :
		case TOperator::EOpConvInt64ToUint64 :
		case TOperator::EOpConvFloat16ToUint64 :
		case TOperator::EOpConvFloatToUint64 :
		case TOperator::EOpConvDoubleToUint64 : return suffix.size() ? "u64" + suffix : "uint64_t";
			
		case TOperator::EOpConvBoolToFloat16 :
		case TOperator::EOpConvInt8ToFloat16 :
		case TOperator::EOpConvUint8ToFloat16 :
		case TOperator::EOpConvInt16ToFloat16 :
		case TOperator::EOpConvUint16ToFloat16 :
		case TOperator::EOpConvIntToFloat16 :
		case TOperator::EOpConvUintToFloat16 :
		case TOperator::EOpConvInt64ToFloat16 :
		case TOperator::EOpConvUint64ToFloat16 :
		case TOperator::EOpConvFloatToFloat16 :
		case TOperator::EOpConvDoubleToFloat16 : return suffix.size() ? "f16" + suffix : "float16_t";
			
		case TOperator::EOpConvBoolToFloat :
		case TOperator::EOpConvInt8ToFloat :
		case TOperator::EOpConvUint8ToFloat :
		case TOperator::EOpConvInt16ToFloat :
		case TOperator::EOpConvUint16ToFloat :
		case TOperator::EOpConvIntToFloat :
		case TOperator::EOpConvUintToFloat :
		case TOperator::EOpConvInt64ToFloat :
		case TOperator::EOpConvUint64ToFloat :
		case TOperator::EOpConvFloat16ToFloat :
		case TOperator::EOpConvDoubleToFloat : return suffix.size() ? suffix : "float";
			
		case TOperator::EOpConvBoolToDouble :
		case TOperator::EOpConvInt8ToDouble :
		case TOperator::EOpConvUint8ToDouble :
		case TOperator::EOpConvInt16ToDouble :
		case TOperator::EOpConvUint16ToDouble :
		case TOperator::EOpConvIntToDouble :
		case TOperator::EOpConvUintToDouble :
		case TOperator::EOpConvInt64ToDouble :
		case TOperator::EOpConvUint64ToDouble :
		case TOperator::EOpConvFloat16ToDouble :
		case TOperator::EOpConvFloatToDouble : return suffix.size() ? "d" + suffix : "double";
#	endif	// HIGH_DETAIL_TRACE
#	if 0
		case TOperator::EOpAssign : return "=";
		case TOperator::EOpAddAssign : return "+=";
		case TOperator::EOpSubAssign : return "-=";
		case TOperator::EOpMulAssign : return "*=";
		case TOperator::EOpVectorTimesMatrixAssign : return "*=";
		case TOperator::EOpVectorTimesScalarAssign : return "*=";
		case TOperator::EOpMatrixTimesScalarAssign : return "*=";
		case TOperator::EOpMatrixTimesMatrixAssign : return "*=";
		case TOperator::EOpDivAssign : return "/=";
		case TOperator::EOpModAssign : return "%=";
		case TOperator::EOpAndAssign : return "&=";
		case TOperator::EOpInclusiveOrAssign : return "|=";
		case TOperator::EOpExclusiveOrAssign : return "^=";
		case TOperator::EOpLeftShiftAssign : return "<<=";
		case TOperator::EOpRightShiftAssign : return ">>=";
		case TOperator::EOpNegative : return "-";
		case TOperator::EOpLogicalNot : return "!";
		case TOperator::EOpVectorLogicalNot : return "not";
		case TOperator::EOpBitwiseNot : return "~";
		case TOperator::EOpPostIncrement : return "++";
		case TOperator::EOpPostDecrement : return "--";
		case TOperator::EOpPreIncrement : return "++";
		case TOperator::EOpPreDecrement : return "--";
		case TOperator::EOpAdd : return "+";
		case TOperator::EOpSub : return "-";
		case TOperator::EOpMul : return "*";
		case TOperator::EOpDiv : return "/";
		case TOperator::EOpMod : return "%";
		case TOperator::EOpRightShift : return ">>";
		case TOperator::EOpLeftShift : return "<<";
		case TOperator::EOpAnd : return "&";
		case TOperator::EOpInclusiveOr : return "|";
		case TOperator::EOpExclusiveOr : return "^";
		case TOperator::EOpEqual : return "==";
		case TOperator::EOpNotEqual : return "!=";
		case TOperator::EOpVectorTimesScalar : return "*";
		case TOperator::EOpVectorTimesMatrix : return "*";
		case TOperator::EOpMatrixTimesVector : return "*";
		case TOperator::EOpMatrixTimesScalar : return "*";
		case TOperator::EOpLogicalOr : return "||";
		case TOperator::EOpLogicalXor : return "^^";
		case TOperator::EOpLogicalAnd : return "&&";
#	endif
		case TOperator::EOpVectorEqual : return "equal";
		case TOperator::EOpVectorNotEqual : return "notEqual";
		case TOperator::EOpLessThan : return "lessThan";
		case TOperator::EOpGreaterThan : return "greaterThan";
		case TOperator::EOpLessThanEqual : return "lessThanEqual";
		case TOperator::EOpGreaterThanEqual : return "greaterThanEqual";
		case TOperator::EOpRadians : return "radians";
		case TOperator::EOpDegrees : return "degress";
		case TOperator::EOpSin : return "sin";
		case TOperator::EOpCos : return "cos";
		case TOperator::EOpTan : return "tan";
		case TOperator::EOpAsin : return "asin";
		case TOperator::EOpAcos : return "acos";
		case TOperator::EOpAtan : return "atan";
		case TOperator::EOpSinh : return "sinh";
		case TOperator::EOpCosh : return "cosh";
		case TOperator::EOpTanh : return "tanh";
		case TOperator::EOpAsinh : return "asinh";
		case TOperator::EOpAcosh : return "acosh";
		case TOperator::EOpAtanh : return "atanh";
		case TOperator::EOpPow : return "pow";
		case TOperator::EOpExp : return "exp";
		case TOperator::EOpLog : return "log";
		case TOperator::EOpExp2 : return "exp2";
		case TOperator::EOpLog2 : return "log2";
		case TOperator::EOpSqrt : return "sqrt";
		case TOperator::EOpInverseSqrt : return "inversesqrt";
		case TOperator::EOpAbs : return "abs";
		case TOperator::EOpSign : return "sign";
		case TOperator::EOpFloor : return "floor";
		case TOperator::EOpTrunc : return "trunc";
		case TOperator::EOpRound : return "round";
		case TOperator::EOpRoundEven : return "roundEven";
		case TOperator::EOpCeil : return "ceil";
		case TOperator::EOpFract : return "fract";
		case TOperator::EOpModf : return "modf";
		case TOperator::EOpMin : return "min";
		case TOperator::EOpMax : return "max";
		case TOperator::EOpClamp : return "clamp";
		case TOperator::EOpMix : return "mix";
		case TOperator::EOpStep : return "step";
		case TOperator::EOpSmoothStep : return "smoothstep";
		case TOperator::EOpIsNan : return "isnan";
		case TOperator::EOpIsInf : return "isinf";
		case TOperator::EOpFma : return "fma";
		case TOperator::EOpFrexp : return "frexp";
		case TOperator::EOpLdexp : return "ldexp";
		case TOperator::EOpFloatBitsToInt : return "floatBitsToInt";
		case TOperator::EOpFloatBitsToUint : return "floatBitsToUint";
		case TOperator::EOpIntBitsToFloat : return "intBitsToFloat";
		case TOperator::EOpUintBitsToFloat : return "uintBitsToFloat";
		case TOperator::EOpDoubleBitsToInt64 : return "doubleBitsToInt64";
		case TOperator::EOpDoubleBitsToUint64 : return "doubleBitsToUint64";
		case TOperator::EOpInt64BitsToDouble : return "int64BitsToDouble";
		case TOperator::EOpUint64BitsToDouble : return "uint64BitsToDouble";
		case TOperator::EOpFloat16BitsToInt16 : return "float16BitsToInt16";
		case TOperator::EOpFloat16BitsToUint16 : return "float16BitsToUint16";
		case TOperator::EOpInt16BitsToFloat16 : return "int16BitsToFloat16";
		case TOperator::EOpUint16BitsToFloat16 : return "uint16BitsToFloat16";
		case TOperator::EOpPackSnorm2x16 : return "packSnorm2x16";
		case TOperator::EOpUnpackSnorm2x16 : return "unpackSnorm2x16";
		case TOperator::EOpPackUnorm2x16 : return "packUnorm2x16";
		case TOperator::EOpUnpackUnorm2x16 : return "unpackUnorm2x16";
		case TOperator::EOpPackSnorm4x8 : return "packSnorm4x8";
		case TOperator::EOpUnpackSnorm4x8 : return "unpackSnorm4x8";
		case TOperator::EOpPackUnorm4x8 : return "packUnorm4x8";
		case TOperator::EOpUnpackUnorm4x8 : return "unpackUnorm4x8";
		case TOperator::EOpPackHalf2x16 : return "packHalf2x16";
		case TOperator::EOpUnpackHalf2x16 : return "unpackHalf2x16";
		case TOperator::EOpPackDouble2x32 : return "packDouble2x32";
		case TOperator::EOpUnpackDouble2x32 : return "unpackDouble2x32";
		case TOperator::EOpPackInt2x32 : return "packInt2x32";
		case TOperator::EOpUnpackInt2x32 : return "unpackInt2x32";
		case TOperator::EOpPackUint2x32 : return "packUint2x32";
		case TOperator::EOpUnpackUint2x32 : return "unpackUint2x32";
		case TOperator::EOpPackFloat2x16 : return "packFloat2x16";
		case TOperator::EOpUnpackFloat2x16 : return "unpackFloat2x16";
		case TOperator::EOpPackInt2x16 : return "packInt2x16";
		case TOperator::EOpUnpackInt2x16 : return "unpackInt2x16";
		case TOperator::EOpPackUint2x16 : return "packUint2x16";
		case TOperator::EOpUnpackUint2x16 : return "unpackUint2x16";
		case TOperator::EOpPackInt4x16 : return "packInt4x16";
		case TOperator::EOpUnpackInt4x16 : return "unpackInt4x16";
		case TOperator::EOpPackUint4x16 : return "packUint4x16";
		case TOperator::EOpUnpackUint4x16 : return "unpackUint4x16";
		case TOperator::EOpPack16 : return "pack16";
		case TOperator::EOpPack32 : return "pack32";
		case TOperator::EOpPack64 : return "pack64";
		case TOperator::EOpUnpack32 : return "unpack32";
		case TOperator::EOpUnpack16 : return "unpack16";
		case TOperator::EOpUnpack8 : return "unpack8";
		case TOperator::EOpLength : return "length";
		case TOperator::EOpDistance : return "distance";
		case TOperator::EOpDot : return "dot";
		case TOperator::EOpCross : return "cross";
		case TOperator::EOpNormalize : return "normalize";
		case TOperator::EOpFaceForward : return "faceForward";
		case TOperator::EOpReflect : return "reflect";
		case TOperator::EOpRefract : return "refract";
		case TOperator::EOpMin3 : return "min3";
		case TOperator::EOpMax3 : return "max3";
		case TOperator::EOpMid3 : return "mid3";
		case TOperator::EOpDPdx : return "dFdx";
		case TOperator::EOpDPdy : return "dFdy";
		case TOperator::EOpFwidth : return "fwidth";
		case TOperator::EOpDPdxFine : return "dFdxFine";
		case TOperator::EOpDPdyFine : return "dFdyFine";
		case TOperator::EOpFwidthFine : return "fwidthFine";
		case TOperator::EOpDPdxCoarse : return "dFdxCoarse";
		case TOperator::EOpDPdyCoarse : return "dFdyCoarse";
		case TOperator::EOpFwidthCoarse : return "fwidthCoarse";
		case TOperator::EOpInterpolateAtCentroid : return "interpolateAtCentroid";
		case TOperator::EOpInterpolateAtSample : return "interpolateAtSample";
		case TOperator::EOpInterpolateAtOffset : return "interpolateAtOffset";
		case TOperator::EOpInterpolateAtVertex : return "interpolateAtVertexAMD";
		case TOperator::EOpMatrixTimesMatrix : return "matrixCompMult";
		case TOperator::EOpOuterProduct : return "outerProduct";
		case TOperator::EOpDeterminant : return "determinant";
		case TOperator::EOpMatrixInverse : return "inverse";
		case TOperator::EOpTranspose : return "transpose";
		case TOperator::EOpEmitVertex : return "EmitVertex";
		case TOperator::EOpEndPrimitive : return "EndPrimitive";
		case TOperator::EOpEmitStreamVertex : return "EmitStreamVertex";
		case TOperator::EOpEndStreamPrimitive : return "EndStreamPrimitive";
		case TOperator::EOpBarrier : return "barrier";
		case TOperator::EOpMemoryBarrier : return "memoryBarrier";
		case TOperator::EOpMemoryBarrierAtomicCounter : return "memoryBarrierAtomicCounter";
		case TOperator::EOpMemoryBarrierBuffer : return "memoryBarrierBuffer";
		case TOperator::EOpMemoryBarrierImage : return "memoryBarrierImage";
		case TOperator::EOpMemoryBarrierShared : return "memoryBarrierShared";
		case TOperator::EOpGroupMemoryBarrier : return "groupMemoryBarrier";
		case TOperator::EOpBallot : return "ballotARB";
		case TOperator::EOpReadInvocation : return "readInvocationARB";
		case TOperator::EOpReadFirstInvocation : return "readFirstInvocationARB";
		case TOperator::EOpAnyInvocation : return "anyInvocation";
		case TOperator::EOpAllInvocations : return "allInvocations";
		case TOperator::EOpAllInvocationsEqual : return "allInvocationsEqual";
		case TOperator::EOpSubgroupBarrier : return "subgroupBarrier";
		case TOperator::EOpSubgroupMemoryBarrier : return "subgroupMemoryBarrier";
		case TOperator::EOpSubgroupMemoryBarrierBuffer : return "subgroupMemoryBarrierBuffer";
		case TOperator::EOpSubgroupMemoryBarrierImage : return "subgroupMemoryBarrierImage";
		case TOperator::EOpSubgroupMemoryBarrierShared : return "subgroupMemoryBarrierShared";
		case TOperator::EOpSubgroupElect : return "subgroupElect";
		case TOperator::EOpSubgroupAll : return "subgroupAll";
		case TOperator::EOpSubgroupAny : return "subgroupAny";
		case TOperator::EOpSubgroupAllEqual : return "subgroupAllEqual";
		case TOperator::EOpSubgroupBroadcast : return "subgroupBroadcast";
		case TOperator::EOpSubgroupBroadcastFirst : return "subgroupBroadcastFirst";
		case TOperator::EOpSubgroupBallot : return "subgroupBallot";
		case TOperator::EOpSubgroupInverseBallot : return "subgroupInverseBallot";
		case TOperator::EOpSubgroupBallotBitExtract : return "subgroupBallotBitExtract";
		case TOperator::EOpSubgroupBallotBitCount : return "subgroupBallotBitCount";
		case TOperator::EOpSubgroupBallotInclusiveBitCount : return "subgroupBallotInclusiveBitCount";
		case TOperator::EOpSubgroupBallotExclusiveBitCount : return "subgroupBallotExclusiveBitCount";
		case TOperator::EOpSubgroupBallotFindLSB : return "subgroupBallotFindLSB";
		case TOperator::EOpSubgroupBallotFindMSB : return "subgroupBallotFindMSB";
		case TOperator::EOpSubgroupShuffle : return "subgroupShuffle";
		case TOperator::EOpSubgroupShuffleXor : return "subgroupShuffleXor";
		case TOperator::EOpSubgroupShuffleUp : return "subgroupShuffleUp";
		case TOperator::EOpSubgroupShuffleDown : return "subgroupShuffleDown";
		case TOperator::EOpSubgroupAdd : return "subgroupAdd";
		case TOperator::EOpSubgroupMul : return "subgroupMul";
		case TOperator::EOpSubgroupMin : return "subgroupMin";
		case TOperator::EOpSubgroupMax : return "subgroupMax";
		case TOperator::EOpSubgroupAnd : return "subgroupAnd";
		case TOperator::EOpSubgroupOr : return "subgroupOr";
		case TOperator::EOpSubgroupXor : return "subgroupXor";
		case TOperator::EOpSubgroupInclusiveAdd : return "subgroupInclusiveAdd";
		case TOperator::EOpSubgroupInclusiveMul : return "subgroupInclusiveMul";
		case TOperator::EOpSubgroupInclusiveMin : return "subgroupInclusiveMin";
		case TOperator::EOpSubgroupInclusiveMax : return "subgroupInclusiveMax";
		case TOperator::EOpSubgroupInclusiveAnd : return "subgroupInclusiveAnd";
		case TOperator::EOpSubgroupInclusiveOr : return "subgroupInclusiveOr";
		case TOperator::EOpSubgroupInclusiveXor : return "subgroupInclusiveXor";
		case TOperator::EOpSubgroupExclusiveAdd : return "subgroupExclusiveAdd";
		case TOperator::EOpSubgroupExclusiveMul : return "subgroupExclusiveMul";
		case TOperator::EOpSubgroupExclusiveMin : return "subgroupExclusiveMin";
		case TOperator::EOpSubgroupExclusiveMax : return "subgroupExclusiveMax";
		case TOperator::EOpSubgroupExclusiveAnd : return "subgroupExclusiveAnd";
		case TOperator::EOpSubgroupExclusiveOr : return "subgroupExclusiveOr";
		case TOperator::EOpSubgroupExclusiveXor : return "subgroupExclusiveXor";
		case TOperator::EOpSubgroupClusteredAdd : return "subgroupClusteredAdd";
		case TOperator::EOpSubgroupClusteredMul : return "subgroupClusteredMul";
		case TOperator::EOpSubgroupClusteredMin : return "subgroupClusteredMin";
		case TOperator::EOpSubgroupClusteredMax : return "subgroupClusteredMax";
		case TOperator::EOpSubgroupClusteredAnd : return "subgroupClusteredAnd";
		case TOperator::EOpSubgroupClusteredOr : return "subgroupClusteredOr";
		case TOperator::EOpSubgroupClusteredXor : return "subgroupClusteredXor";
		case TOperator::EOpSubgroupQuadBroadcast : return "subgroupQuadBroadcast";
		case TOperator::EOpSubgroupQuadSwapHorizontal : return "subgroupQuadSwapHorizontal";
		case TOperator::EOpSubgroupQuadSwapVertical : return "subgroupQuadSwapVertical";
		case TOperator::EOpSubgroupQuadSwapDiagonal : return "subgroupQuadSwapDiagonal";
		case TOperator::EOpSubgroupPartition : return "subgroupPartition";
		case TOperator::EOpSubgroupPartitionedAdd : return "subgroupPartitionedAdd";
		case TOperator::EOpSubgroupPartitionedMul : return "subgroupPartitionedMul";
		case TOperator::EOpSubgroupPartitionedMin : return "subgroupPartitionedMin";
		case TOperator::EOpSubgroupPartitionedMax : return "subgroupPartitionedMax";
		case TOperator::EOpSubgroupPartitionedAnd : return "subgroupPartitionedAnd";
		case TOperator::EOpSubgroupPartitionedOr : return "subgroupPartitionedOr";
		case TOperator::EOpSubgroupPartitionedXor : return "subgroupPartitionedXor";
		case TOperator::EOpSubgroupPartitionedInclusiveAdd : return "subgroupPartitionedInclusiveAdd";
		case TOperator::EOpSubgroupPartitionedInclusiveMul : return "subgroupPartitionedInclusiveMul";
		case TOperator::EOpSubgroupPartitionedInclusiveMin : return "subgroupPartitionedInclusiveMin";
		case TOperator::EOpSubgroupPartitionedInclusiveMax : return "subgroupPartitionedInclusiveMax";
		case TOperator::EOpSubgroupPartitionedInclusiveAnd : return "subgroupPartitionedInclusiveAnd";
		case TOperator::EOpSubgroupPartitionedInclusiveOr : return "subgroupPartitionedInclusiveOr";
		case TOperator::EOpSubgroupPartitionedInclusiveXor : return "subgroupPartitionedInclusiveXor";
		case TOperator::EOpSubgroupPartitionedExclusiveAdd : return "subgroupPartitionedExclusiveAdd";
		case TOperator::EOpSubgroupPartitionedExclusiveMul : return "subgroupPartitionedExclusiveMul";
		case TOperator::EOpSubgroupPartitionedExclusiveMin : return "subgroupPartitionedExclusiveMin";
		case TOperator::EOpSubgroupPartitionedExclusiveMax : return "subgroupPartitionedExclusiveMax";
		case TOperator::EOpSubgroupPartitionedExclusiveAnd : return "subgroupPartitionedExclusiveAnd";
		case TOperator::EOpSubgroupPartitionedExclusiveOr : return "subgroupPartitionedExclusiveOr";
		case TOperator::EOpSubgroupPartitionedExclusiveXor : return "subgroupPartitionedExclusiveXor";
		case TOperator::EOpMinInvocations : return "minInvocationsAMD";
		case TOperator::EOpMaxInvocations : return "maxInvocationsAMD";
		case TOperator::EOpAddInvocations : return "addInvocationsAMD";
		case TOperator::EOpMinInvocationsNonUniform : return "minInvocationsNonUniformAMD";
		case TOperator::EOpMaxInvocationsNonUniform : return "maxInvocationsNonUniformAMD";
		case TOperator::EOpAddInvocationsNonUniform : return "addInvocationsNonUniformAMD";
		case TOperator::EOpMinInvocationsInclusiveScan : return "minInvocationsInclusiveScanAMD";
		case TOperator::EOpMaxInvocationsInclusiveScan : return "maxInvocationsInclusiveScanAMD";
		case TOperator::EOpAddInvocationsInclusiveScan : return "addInvocationsInclusiveScanAMD";
		case TOperator::EOpMinInvocationsInclusiveScanNonUniform : return "minInvocationsInclusiveScanNonUniformAMD";
		case TOperator::EOpMaxInvocationsInclusiveScanNonUniform : return "maxInvocationsInclusiveScanNonUniformAMD";
		case TOperator::EOpAddInvocationsInclusiveScanNonUniform : return "addInvocationsInclusiveScanNonUniformAMD";
		case TOperator::EOpMinInvocationsExclusiveScan : return "minInvocationsExclusiveScanAMD";
		case TOperator::EOpMaxInvocationsExclusiveScan : return "maxInvocationsExclusiveScanAMD";
		case TOperator::EOpAddInvocationsExclusiveScan : return "addInvocationsExclusiveScanAMD";
		case TOperator::EOpMinInvocationsExclusiveScanNonUniform : return "minInvocationsExclusiveScanNonUniformAMD";
		case TOperator::EOpMaxInvocationsExclusiveScanNonUniform : return "maxInvocationsExclusiveScanNonUniformAMD";
		case TOperator::EOpAddInvocationsExclusiveScanNonUniform : return "addInvocationsExclusiveScanNonUniformAMD";
		case TOperator::EOpSwizzleInvocations : return "swizzleInvocationsAMD";
		case TOperator::EOpSwizzleInvocationsMasked : return "swizzleInvocationsMaskedAMD";
		case TOperator::EOpWriteInvocation : return "writeInvocationAMD";
		case TOperator::EOpMbcnt : return "mbcntAMD";
		case TOperator::EOpCubeFaceIndex : return "cubeFaceIndexAMD";
		case TOperator::EOpCubeFaceCoord : return "cubeFaceCoordAMD";
		case TOperator::EOpTime : return "timeAMD";
		case TOperator::EOpAtomicAdd : return "atomicAdd";
		case TOperator::EOpAtomicMin : return "atomicMin";
		case TOperator::EOpAtomicMax : return "atomicMax";
		case TOperator::EOpAtomicAnd : return "atomicAnd";
		case TOperator::EOpAtomicOr : return "atomicOr";
		case TOperator::EOpAtomicXor : return "atomicXor";
		case TOperator::EOpAtomicExchange : return "atomicExchange";
		case TOperator::EOpAtomicCompSwap : return "atomicCompSwap";
		case TOperator::EOpAtomicLoad : return "atomicLoad";
		case TOperator::EOpAtomicStore : return "atomicStore";
		case TOperator::EOpAny : return "any";
		case TOperator::EOpAll : return "all";
		case TOperator::EOpArrayLength : return ".length";
		case TOperator::EOpImageQuerySize : return "imageSize";
		case TOperator::EOpImageQuerySamples : return "imageSamples";
		case TOperator::EOpImageLoad : return "imageLoad";
		case TOperator::EOpImageStore : return "imageStore";
		case TOperator::EOpImageLoadLod : return "imageLoadLod";
		case TOperator::EOpImageStoreLod : return "imageStoreLod";
		case TOperator::EOpImageAtomicAdd : return "imageAtomicAdd";
		case TOperator::EOpImageAtomicMin : return "imageAtomicMin";
		case TOperator::EOpImageAtomicMax : return "imageAtomicMax";
		case TOperator::EOpImageAtomicAnd : return "imageAtomicAnd";
		case TOperator::EOpImageAtomicOr : return "imageAtomicOr";
		case TOperator::EOpImageAtomicXor : return "imageAtomicXor";
		case TOperator::EOpImageAtomicExchange : return "imageAtomicExchange";
		case TOperator::EOpImageAtomicCompSwap : return "imageAtomicCompSwap";
		case TOperator::EOpImageAtomicLoad : return "imageAtomicLoad";
		case TOperator::EOpImageAtomicStore : return "imageAtomicStore";
		case TOperator::EOpSubpassLoad : return "subpassLoad";
		case TOperator::EOpSubpassLoadMS : return "subpassLoadMS";
		case TOperator::EOpSparseImageLoad : return "sparseImageLoadARB";
		case TOperator::EOpSparseImageLoadLod : return "sparseImageLoadLodAMD";
		case TOperator::EOpTextureQuerySize : return "textureSize";
		case TOperator::EOpTextureQueryLod : return "textureQueryLod";
		case TOperator::EOpTextureQueryLevels : return "textureQueryLevels";
		case TOperator::EOpTextureQuerySamples : return "textureSamples";
		case TOperator::EOpTexture : return "texture";
		case TOperator::EOpTextureProj : return "textureProj";
		case TOperator::EOpTextureLod : return "textureLod";
		case TOperator::EOpTextureOffset : return "textureOffset";
		case TOperator::EOpTextureFetch : return "texelFetch";
		case TOperator::EOpTextureFetchOffset : return "texelFetchOffset";
		case TOperator::EOpTextureProjOffset : return "textureProjOffset";
		case TOperator::EOpTextureLodOffset : return "textureLodOffset";
		case TOperator::EOpTextureProjLod : return "textureProjLod";
		case TOperator::EOpTextureProjLodOffset : return "textureProjLodOffset";
		case TOperator::EOpTextureGrad : return "textureGrad";
		case TOperator::EOpTextureGradOffset : return "textureGradOffset";
		case TOperator::EOpTextureProjGrad : return "textureProjGrad";
		case TOperator::EOpTextureProjGradOffset : return "textureProjGradOffset";
		case TOperator::EOpTextureGather : return "textureGather";
		case TOperator::EOpTextureGatherOffset : return "textureGatherOffset";
		case TOperator::EOpTextureGatherOffsets : return "textureGatherOffsets";
		case TOperator::EOpTextureClamp : return "textureClampARB";
		case TOperator::EOpTextureOffsetClamp : return "textureOffsetClampARB";
		case TOperator::EOpTextureGradClamp : return "textureGradClampARB";
		case TOperator::EOpTextureGradOffsetClamp : return "textureGradOffsetClampARB";
		case TOperator::EOpTextureGatherLod : return "textureGatherLodAMD";
		case TOperator::EOpTextureGatherLodOffset : return "textureGatherLodOffsetAMD";
		case TOperator::EOpTextureGatherLodOffsets : return "textureGatherLodOffsetsAMD";
		case TOperator::EOpFragmentMaskFetch : return "fragmentMaskFetchAMD";
		case TOperator::EOpFragmentFetch : return "fragmentFetchAMD";
		case TOperator::EOpSparseTexture : return "sparseTextureARB";
		case TOperator::EOpSparseTextureLod : return "parseTextureLodARB";
		case TOperator::EOpSparseTextureOffset : return "sparseTextureOffsetARB";
		case TOperator::EOpSparseTextureFetch : return "sparseTextureFetchARB";
		case TOperator::EOpSparseTextureFetchOffset : return "sparseTextureFetchOffsetARB";
		case TOperator::EOpSparseTextureLodOffset : return "sparseTextureLodOffsetARB";
		case TOperator::EOpSparseTextureGrad : return "sparseTextureGradARB";
		case TOperator::EOpSparseTextureGradOffset : return "sparseTextureGradOffsetARB";
		case TOperator::EOpSparseTextureGather : return "sparseTextureGatherARB";
		case TOperator::EOpSparseTextureGatherOffset : return "sparseTextureGatherOffsetAARB";
		case TOperator::EOpSparseTextureGatherOffsets : return "sparseTextureGatherOffsetsARB";
		case TOperator::EOpSparseTexelsResident : return "sparseTexelsResidentARB";
		case TOperator::EOpSparseTextureClamp : return "sparseTextureClampARB";
		case TOperator::EOpSparseTextureOffsetClamp : return "sparseTextureOffsetClampARB";
		case TOperator::EOpSparseTextureGradClamp : return "sparseTextureGradClampARB";
		case TOperator::EOpSparseTextureGradOffsetClamp : return "sparseTextureGradOffsetClampARB";
		case TOperator::EOpSparseTextureGatherLod : return "sparseTextureGatherLodAMD";
		case TOperator::EOpSparseTextureGatherLodOffset : return "sparseTextureGatherLodOffsetAMD";
		case TOperator::EOpSparseTextureGatherLodOffsets : return "sparseTextureGatherLodOffsetsAMD";
		case TOperator::EOpImageSampleFootprintNV : return "textureFootprintNV";
		case TOperator::EOpImageSampleFootprintClampNV : return "textureFootprintClampNV";
		case TOperator::EOpImageSampleFootprintLodNV : return "textureFootprintLodNV";
		case TOperator::EOpImageSampleFootprintGradNV : return "textureFootprintGradNV";
		case TOperator::EOpImageSampleFootprintGradClampNV : return "textureFootprintGradClampNV";
		case TOperator::EOpAddCarry : return "uaddCarry";
		case TOperator::EOpSubBorrow : return "subBorrow";
		case TOperator::EOpUMulExtended : return "umulExtended";
		case TOperator::EOpIMulExtended : return "imulExtended";
		case TOperator::EOpBitfieldExtract : return "bitfieldExtract";
		case TOperator::EOpBitfieldInsert : return "bitfieldInsert";
		case TOperator::EOpBitFieldReverse : return "bitFieldReverse";
		case TOperator::EOpBitCount : return "bitCount";
		case TOperator::EOpFindLSB : return "findLSB";
		case TOperator::EOpFindMSB : return "findMSB";
		case TOperator::EOpTraceKHR : return "traceRays";
		case TOperator::EOpTraceNV : return "traceNV";
		case TOperator::EOpReportIntersection : return "reportIntersection";
		case TOperator::EOpIgnoreIntersectionKHR : return "ignoreIntersection";
		case TOperator::EOpIgnoreIntersectionNV : return "ignoreIntersectionNV";
		case TOperator::EOpTerminateRayKHR : return "terminateRay";
		case TOperator::EOpTerminateRayNV : return "terminateRayNV";
		case TOperator::EOpExecuteCallableKHR : return "executeCallable";
		case TOperator::EOpExecuteCallableNV : return "executeCallableNV";
		case TOperator::EOpWritePackedPrimitiveIndices4x8NV : return "writePackedPrimitiveIndices4x8NV";
		// TODO
		/*EOpRayQueryInitialize,
		EOpRayQueryTerminate,
		EOpRayQueryGenerateIntersection,
		EOpRayQueryConfirmIntersection,
		EOpRayQueryProceed,
		EOpRayQueryGetIntersectionType,
		EOpRayQueryGetRayTMin,
		EOpRayQueryGetRayFlags,
		EOpRayQueryGetIntersectionT,
		EOpRayQueryGetIntersectionInstanceCustomIndex,
		EOpRayQueryGetIntersectionInstanceId,
		EOpRayQueryGetIntersectionInstanceShaderBindingTableRecordOffset,
		EOpRayQueryGetIntersectionGeometryIndex,
		EOpRayQueryGetIntersectionPrimitiveIndex,
		EOpRayQueryGetIntersectionBarycentrics,
		EOpRayQueryGetIntersectionFrontFace,
		EOpRayQueryGetIntersectionCandidateAABBOpaque,
		EOpRayQueryGetIntersectionObjectRayDirection,
		EOpRayQueryGetIntersectionObjectRayOrigin,
		EOpRayQueryGetWorldRayDirection,
		EOpRayQueryGetWorldRayOrigin,
		EOpRayQueryGetIntersectionObjectToWorld,
		EOpRayQueryGetIntersectionWorldToObject,*/
	}
	//END_ENUM_CHECKS();

	CHECK(false);
	return "<unknown>";
}

bool ValidateInterm (TIntermediate &intermediate)
{
	TIntermNode*	root = intermediate.getTreeRoot();
	CHECK_ERR( root );

	TIntermNode*	linker_obj = nullptr;

	if ( auto* aggr = root->getAsAggregate() )
	{
		auto& seq = aggr->getSequence();

		for (size_t i = 0; i < seq.size(); ++i)
		{
			if ( auto* aggr2 = seq[i]->getAsAggregate(); aggr2 and aggr2->getOp() == TOperator::EOpLinkerObjects )
			{
				linker_obj = aggr2;
				seq.erase( seq.begin() + i );
				break;
			}
		}

		if ( linker_obj )
		{
			seq.push_back( linker_obj );
		}
	}

	ASSERT( root->getAsAggregate() );
	ASSERT( root->getAsAggregate()->getSequence().back()->getAsAggregate() );
	ASSERT( root->getAsAggregate()->getSequence().back()->getAsAggregate()->getOp() == TOperator::EOpLinkerObjects );

	return true;
}
