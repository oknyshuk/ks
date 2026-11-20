#pragma once
import ks.tier1.keyvalues;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// The closed set of sound operators (PLAN.md Decision 2). Every class that registers itself with
// SOS_END_OPERATOR_CONSTRUCTOR is listed here, once; the list generates SosOperatorKind and the
// switches of SosOps (sos_op_dispatch.cpp). A new operator is added to this list and to the
// includes of sos_op_dispatch.cpp, and nowhere else.

#define KS_SOS_OPERATORS( X, ... ) \
	X( CSosOperatorBlockEntry, __VA_ARGS__ ) \
	X( CSosOperatorConvar, __VA_ARGS__ ) \
	X( CSosOperatorSetConvar, __VA_ARGS__ ) \
	X( CSosOperatorDashboard, __VA_ARGS__ ) \
	X( CSosOperatorDelta, __VA_ARGS__ ) \
	X( CSosOperatorDistantDSP, __VA_ARGS__ ) \
	X( CSosOperatorEntityInfo, __VA_ARGS__ ) \
	X( CSosOperatorEntryTime, __VA_ARGS__ ) \
	X( CSosOperatorFalloff, __VA_ARGS__ ) \
	X( CSosOperatorFalloffTail, __VA_ARGS__ ) \
	X( CSosOperatorFloatFilter, __VA_ARGS__ ) \
	X( CSosOperatorIterateAndMergeSpeakers, __VA_ARGS__ ) \
	X( CSosOperatorMapName, __VA_ARGS__ ) \
	X( CSosOperatorFunc1, __VA_ARGS__ ) \
	X( CSosOperatorFloat, __VA_ARGS__ ) \
	X( CSosOperatorFloatAccumulate12, __VA_ARGS__ ) \
	X( CSosOperatorVec3, __VA_ARGS__ ) \
	X( CSosOperatorSpeakers, __VA_ARGS__ ) \
	X( CSosOperatorSourceDistance, __VA_ARGS__ ) \
	X( CSosOperatorRemapValue, __VA_ARGS__ ) \
	X( CSosOperatorCurve4, __VA_ARGS__ ) \
	X( CSosOperatorFacing, __VA_ARGS__ ) \
	X( CSosOperatorRandom, __VA_ARGS__ ) \
	X( CSosOperatorLogicSwitch, __VA_ARGS__ ) \
	X( CSosOperatorMixGroup, __VA_ARGS__ ) \
	X( CSosOperatorMixLayer, __VA_ARGS__ ) \
	X( CSosOperatorOcclusion, __VA_ARGS__ ) \
	X( CSosOperatorSetOpvarFloat, __VA_ARGS__ ) \
	X( CSosOperatorGetOpvarFloat, __VA_ARGS__ ) \
	X( CSosOperatorIncrementOpvarFloat, __VA_ARGS__ ) \
	X( CSosOperatorOutput, __VA_ARGS__ ) \
	X( CSosOperatorPlatform, __VA_ARGS__ ) \
	X( CSosOperatorViewInfo, __VA_ARGS__ ) \
	X( CSosOperatorPosVec8, __VA_ARGS__ ) \
	X( CSosOperatorSourceInfo, __VA_ARGS__ ) \
	X( CSosOperatorSpatializeSpeakers, __VA_ARGS__ ) \
	X( CSosOperatorStartEntry, __VA_ARGS__ ) \
	X( CSosOperatorStopEntry, __VA_ARGS__ ) \
	X( CSosOperatorSysTime, __VA_ARGS__ ) \
	X( CSosOperatorGetTrackSyncPoint, __VA_ARGS__ ) \
	X( CSosOperatorQueueToTrack, __VA_ARGS__ ) \
	X( CSosOperatorPlayOnTrack, __VA_ARGS__ ) \
	X( CSosOperatorStopTrack, __VA_ARGS__ ) \
	X( CSosOperatorPrintFloat, __VA_ARGS__ )

enum class SosOperatorKind : unsigned char
{
#define KS_SOS_OPERATOR_KIND( cls, ... ) cls,
	KS_SOS_OPERATORS( KS_SOS_OPERATOR_KIND, )
#undef KS_SOS_OPERATOR_KIND
};

//-----------------------------------------------------------------------------
// The entry points of an operator (what used to be its virtual functions), as
// E( return type, name, const or nothing, ( parameters ), ( arguments ) ).
// SosOps::<name> takes the operator as its first parameter, pOp.
//-----------------------------------------------------------------------------
#define KS_SOS_OP_UNPAREN( ... ) __VA_ARGS__
#define KS_SOS_OP_SIGNATURE( Const, ... ) Const CSosOperator *pOp __VA_OPT__( , ) __VA_ARGS__

#define KS_SOS_OP_ENTRIES( E ) \
	E( void, SetDefaults, const, ( void *pVoidMem ), ( pVoidMem ) ) \
	E( std::size_t, GetSize, const, ( ), ( ) ) \
	E( void, Execute, const, ( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ), \
		( pVoidMem, pChannel, pScratchPad, pStack, nOpIndex ) ) \
	E( void, Print, const, ( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ), ( pVoidMem, pStack, nOpIndex, nLevel ) ) \
	E( void, OpHelp, const, ( ), ( ) ) \
	E( void, ParseKV, const, ( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pParamKV ), ( pStack, pVoidMem, pParamKV ) ) \
	E( void, StackInit, , ( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex ), ( pVoidMem, pStack, nOpIndex ) ) \
	E( void, StackShutdown, , ( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex ), ( pVoidMem, pStack, nOpIndex ) )
