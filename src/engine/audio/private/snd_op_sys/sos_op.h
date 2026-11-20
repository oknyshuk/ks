#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

import std;
#include "sos_system.h"
#include "snd_channels.h"
#include "sos_op_list.h"
import ks.core.logging;
import ks.core.wchartypes;
import ks.tier1.keyvalues;
import ks.tier1.map_ops;

// Externs the logging channel
extern LoggingChannelID_t LOG_SND_OPERATORS;;



#define THREAD_LOCK_SOUND() std::scoped_lock soundLock( g_SndMutex )




inline constexpr auto VALUE_REF_CHAR = '@';
inline constexpr auto CLOSE_SQ_BRACKET = ']';

#define SOS_INPUT_FLOAT( memberName, memberCount ) \
	float memberName[memberCount]; \
	int memberName##Offset[memberCount];

#define SOS_OUTPUT_FLOAT( memberName, memberCount ) \
	float memberName[memberCount];

#define SOS_INIT_INPUT_VAR( memberName, memberCount, value ) \
	for( int i = 0; i < memberCount; i++ ) \
{ pStructMem->memberName[i] = value; pStructMem->memberName##Offset[i] = -1; }

#define SOS_INIT_OUTPUT_VAR( memberName, memberType, value ) \
	for( int i = 0; i < memberType; i++ ) \
{ pStructMem->memberName[i] = value; }

#define SOS_REGISTER_INPUT( className, memberName, memberCount, memberString, type )	\
	{																					\
		InputData_t inputData;															\
		inputData.m_nValueOffset = __builtin_offsetof( className##_t, memberName );					\
		inputData.m_nOffsetOffset = __builtin_offsetof( className##_t, memberName##Offset );		\
		inputData.m_Type = type;														\
		inputData.m_nCount = memberCount;												\
		inputData.m_pName = memberString;												\
		int nIndex = std::ssize( m_vInputs ); m_vInputs.push_back( inputData );									\
		m_vInputMap.emplace( memberString, nIndex );										\
	}

#define SOS_REGISTER_INPUT_FLOAT( className, memberName, memberCount, memberString ) \
	SOS_REGISTER_INPUT( className, memberName, memberCount, memberString, SO_FLOAT )

#define SOS_REGISTER_OUTPUT( className, memberName, memberCount, memberString, type )	\
	{																					\
		OutputData_t outputData;														\
		outputData.m_nValueOffset = __builtin_offsetof( className##_t, memberName );					\
		outputData.m_Type = type;														\
		outputData.m_nCount = memberCount;												\
		outputData.m_pName = memberString;												\
		int nIndex = std::ssize( m_vOutputs ); m_vOutputs.push_back( outputData );								\
		m_vOutputMap.emplace( memberString, nIndex );									\
	}

#define SOS_REGISTER_OUTPUT_FLOAT( className, memberName, memberCount, memberString ) \
	SOS_REGISTER_OUTPUT( className, memberName, memberCount, memberString, SO_FLOAT )



#define SOS_BEGIN_OPERATOR_CONSTRUCTOR( classname, operatorstring ) \
classname::classname() \
{


#define SOS_END_OPERATOR_CONSTRUCTOR( classname, operatorstring ) \
	m_Kind = SosOperatorKind::classname; \
	g_pSoundOperatorSystem->m_vOperatorCollection.emplace(operatorstring, this ); \
} \
classname g_##classname;


#define SOS_HEADER_DESC( classname ) \
public: \
	classname(); \
	void	SetDefaults( void *pVoidMem ) const ; \
	std::size_t	GetSize() const { return sizeof( classname##_t ); } \
	void	Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const ; \
	void	Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const ; \
	void    OpHelp( ) const ; \
	void	ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pParamKV ) const ; \



//-----------------------------------------------------------------------------
// The CSosOperator class:
//
// Base class for all sound operators, each operator must override "execute"
//
//-----------------------------------------------------------------------------
struct CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flExecute, SO_SINGLE )
	bool m_bExecuteOnce;
	bool m_bHasExecuted;
};


class CSosOperator
{
public:

							CSosOperator();
	int						GetOutputOffset( const char *pOutputName ) const;
	// Proposal to optimize this:
	// For each InputData_t, we store a function pointer to handle the resolution.
	// We would create a different resolve function for the major cases (write a float, write 3 floats, generic case - same as current implementation).
	// When filling the inputs initially, we would store the optimal function pointer.
	// Cost:	4 bytes per InputData_t
	//			One dynamic call instead of one static call (currently ResolveInputValue).
	// But then in most cases, the implementation would more efficient (estimated at 4 to 10 times faster).
	void					ResolveInputValue( void *pInputDefault, int *pOffsetArray, short nDataCount, SODataType_t nDataType, void *pStackMem ) const;
	void					ResolveInputValues( void *pStructMem, void *pStackMem ) const;
	void					OffsetConnections( void *pVoidMem, std::size_t nOffset ) const;
	bool					BaseParseKV( CSosOperatorStack *pStack, CSosOperator_t *pStructMem, const char *pParamString, const char *pValueString ) const; 
	int						ParseValueRef( CSosOperatorStack *pStack, const char *pParamString, const char *pValueString ) const;
	void					PrintBaseParams( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const;
	void					PrintIO( CSosOperator_t *pStructMem, CSosOperatorStack *pStack, int nLevel ) const;
	int						BaseParseHasArrayIndice( const char *pParamString ) const;
	void					SetBaseDefaults( void *pVoidMem ) const;

	// SetDefaults, GetSize, Execute, Print, OpHelp and ParseKV are defined by every operator class
	// (SOS_HEADER_DESC); SosOps calls the one of the operator's kind. StackInit and StackShutdown
	// are the defaults of operators that do not define their own.
	void					StackInit( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex );
	void					StackShutdown( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex );

	// which operator class this is: what SosOps dispatches on (set by the class' constructor)
	SosOperatorKind			m_Kind;

protected:
public:

	struct OutputData_t
	{
		const char *m_pName;	// registered name (a literal)
		std::size_t m_nValueOffset;
		SODataType_t m_Type;
		short m_nCount;
	};

	struct InputData_t
	{
		const char *m_pName;	// registered name (a literal)
		std::size_t m_nValueOffset;
		std::size_t m_nOffsetOffset;
		SODataType_t m_Type;
		short m_nCount;
	};

	ks::idict< int > m_vOutputMap;	// name -> index in m_vOutputs
	std::vector< OutputData_t > m_vOutputs;

	ks::idict< int > m_vInputMap;	// name -> index in m_vInputs
	std::vector< InputData_t > m_vInputs;
};


//-----------------------------------------------------------------------------
// Dispatch over the closed set of sound operators: SosOps::<entry>( pOp, ... ) calls the member of
// that name of the operator's class (sos_op_dispatch.cpp).
//-----------------------------------------------------------------------------
struct SosOps
{
#define KS_SOS_OP_DECLARE_ENTRY( Ret, Name, Const, Params, Args ) \
	static Ret Name( KS_SOS_OP_SIGNATURE( Const, KS_SOS_OP_UNPAREN Params ) );
	KS_SOS_OP_ENTRIES( KS_SOS_OP_DECLARE_ENTRY )
#undef KS_SOS_OP_DECLARE_ENTRY
};
