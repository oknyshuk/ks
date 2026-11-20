// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

import std;


import ks.core.color;
import ks.core.logging;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.strtools;
import ks.tier1.vector_ops;

#include "sound.h"

#include "snd_device.h"
#include "sound_private.h"
#include "snd_channels.h"
#include "snd_audio_source.h"
#include "snd_dev_xaudio.h"
#include "snd_wave_source.h"
#include "snd_wave_data.h"
#include "snd_wave_mixer_private.h"

#include "audio/private/posix_stubs.h"
#include "tier2/interval.h"

#include "sos_op.h"

extern ConVar snd_sos_show_operator_init;
extern ConVar snd_sos_show_operator_shutdown;

static void Register_LOG_SND_OPERATORS_Tags();
LoggingChannelID_t LOG_SND_OPERATORS = LoggingSystem_RegisterLoggingChannel( "SndOperators", Register_LOG_SND_OPERATORS_Tags, LCF_CONSOLE_ONLY, LS_MESSAGE );
void Register_LOG_SND_OPERATORS_Tags()
{
LoggingSystem_AddTagToCurrentChannel( "SndOperators" );
LoggingSystem_AddTagToCurrentChannel( "SND" );
};


Color OpNameColor( 185, 215, 255, 255 );
Color OpColor( 185, 185, 255, 255 );
Color ConnectColor( 255, 185, 255, 255 );
Color NotExecuteColor( 255, 0, 0, 255 );
Color ResultColor( 255, 185, 185, 255 );
int foo;

void S_GetFloatFromString( float *pFlVector, const char *pString, int nSize = 3 )
{
	char tempString[128];
	V_strncpy( tempString, pString, sizeof(tempString) );

	int i = 0;
	char *token = std::strtok( tempString, "," );
	while( token )
	{
		*pFlVector = std::atof( token );
		token = strtok( nullptr, "," );
		i++;
		if ( i >= nSize )
		{
			break;
		}
		pFlVector++;
	}
}


//-----------------------------------------------------------------------------
// CSosOperator
// Operator base class 
//-----------------------------------------------------------------------------
CSosOperator::CSosOperator( )
{
	SOS_REGISTER_INPUT_FLOAT( CSosOperator, m_flExecute, SO_SINGLE, "input_execute" )
}

void CSosOperator::SetBaseDefaults( void *pVoidMem ) const
{
	CSosOperator_t *pStructMem = (CSosOperator_t *)pVoidMem;

	SOS_INIT_INPUT_VAR( m_flExecute, SO_SINGLE, 1.0 )
	pStructMem->m_bExecuteOnce = false;
	pStructMem->m_bHasExecuted = false;
}

void CSosOperator::PrintIO( CSosOperator_t *pStructMem, CSosOperatorStack *pStack, int nLevel ) const
{

	int nNumberOfInputs = std::ssize( m_vInputs );
	for(int i = 0; i < nNumberOfInputs; i++ )
	{
		const InputData_t & inputData = m_vInputs[i];

		const char *pInputName = inputData.m_pName;
		Log_Msg( LOG_SND_OPERATORS, OpColor , "%*s%s: ", nLevel, "    ", pInputName );

		int nConnections[8];
		int nNumConnections = 0;

		char *pInputValue = (char *)pStructMem + inputData.m_nValueOffset;
		char *pOffsetOffset  = (char *)pStructMem + inputData.m_nOffsetOffset;
		bool bAnyConnected = false;
		for( int j = 0; j < inputData.m_nCount; j++ )
		{

			int nOffsetValue = *((int *)pOffsetOffset + j );
			bool bIsConnected = nOffsetValue < 0 ? false : true ;
			bAnyConnected |= bIsConnected;
			if( bIsConnected )
			{
				nConnections[ nNumConnections ] = nOffsetValue;
				nNumConnections++;
			}
			else
			{
				nConnections[ nNumConnections ] = -1;
				nNumConnections++;
			}

			Color connectedColor = bIsConnected ? ConnectColor : OpColor;

			switch( inputData.m_Type )
			{
			case SO_FLOAT: 
				Log_Msg( LOG_SND_OPERATORS, connectedColor, "%f ", *((float *)pInputValue+j) );
				break;
			default:
				Log_Msg( LOG_SND_OPERATORS, connectedColor, " UNKNOWN DATA TYPE %i ", inputData.m_Type );
				break;
			}
		}
		if( bAnyConnected )
		{
			int nOpIndex = pStack->FindOperatorViaOffset( nConnections[0] );
			Log_Msg( LOG_SND_OPERATORS, ConnectColor, "%%(connected: %s", pStack->GetOperatorName( nOpIndex ) ); 

			if( nNumConnections > 0 )
			{
				for( int k = 0; k < nNumConnections; k++ )
				{
					Log_Msg( LOG_SND_OPERATORS, ConnectColor, " %i", nConnections[k] );
				}
			}
			Log_Msg( LOG_SND_OPERATORS, ConnectColor, ")");
		}
		Log_Msg( LOG_SND_OPERATORS, ConnectColor, "\n");
		
	}

	int nNumberOfOutputs = std::ssize( m_vOutputs );
	for(int i = 0; i < nNumberOfOutputs; i++ )
	{
		const OutputData_t & outputData = m_vOutputs[i];
		const char *pOutputName = outputData.m_pName;
		Log_Msg( LOG_SND_OPERATORS, OpColor, "%*s%s: ", nLevel, "    ", pOutputName );

		char *pOutput = (char *)pStructMem + outputData.m_nValueOffset;
		for( int j = 0; j < outputData.m_nCount; j++ )
		{
			switch( outputData.m_Type )
			{
			case SO_FLOAT:
				Log_Msg( LOG_SND_OPERATORS, OpColor, "%f ", *((float *)pOutput+j) );
				break;
			default:
				Log_Msg( LOG_SND_OPERATORS, OpColor, " UNKNOWN DATA TYPE %i ", outputData.m_Type );
				break;
			}
		}
		Log_Msg( LOG_SND_OPERATORS, OpColor, "\n" );
	}
}

void CSosOperator::PrintBaseParams( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperator_t *pStructMem = (CSosOperator_t *)pVoidMem;
	const char *pName = pStack->GetOperatorName( nOpIndex );
	Color executeColor = ( ( pStructMem->m_flExecute[0] > 0.0 ) && !( pStructMem->m_bExecuteOnce && pStructMem->m_bHasExecuted ) ) ? OpNameColor : NotExecuteColor;
	Log_Msg( LOG_SND_OPERATORS, executeColor, "\n%*sName: %s\n", nLevel, "    ", pName );
	Log_Msg( LOG_SND_OPERATORS, OpColor, "\n%*sExecute Once: %s\n", nLevel, "    ", pStructMem->m_bExecuteOnce ? "true" : "false" );

	PrintIO( pStructMem, pStack, nLevel );
}

int CSosOperator::ParseValueRef( CSosOperatorStack *pStack, const char *pParamString, const char *pValueString ) const
{
	if ( pValueString[0] == VALUE_REF_CHAR )
	{
		pValueString++;	
		char tempString[64];
		V_strncpy( tempString, pValueString, sizeof(tempString) );

		char *pOpname = std::strtok( tempString, "." );
		char *pOutputName = strtok( nullptr, "");
		int nOutOffset = pStack->GetOperatorOutputOffset( pOpname, pOutputName );
		if ( nOutOffset < 0 )
		{
			return -1;
		}
		int nOutIndex = BaseParseHasArrayIndice( pOutputName );
		if ( nOutIndex > -1 )
		{
			nOutOffset += nOutIndex;
		}

		return nOutOffset;
	}
	return -1;
}
int CSosOperator::BaseParseHasArrayIndice( const char *pParamString ) const
{
	int nInputIndex = -1;
	char tempString[64];
	V_strncpy( tempString, pParamString, sizeof(tempString) );
	//	char *pInputParamString = strtok( tempString, "[" );
	char *pIndexString = std::strtok( nullptr, "]");
	if ( pIndexString && *pIndexString )
	{
		nInputIndex = V_atoi( pIndexString );
	}
	return nInputIndex;

}

bool CSosOperator::BaseParseKV( CSosOperatorStack *pStack, CSosOperator_t *pStructMem, const char *pParamString, const char *pValueString ) const
{
	// if we succeed in finding the specified parameter we return true otw false

	if ( pParamString && *pParamString && pValueString && *pValueString )
	{
		// check for simple global params
		if ( !V_strcasecmp( pParamString, "operator" ) )
		{
			return true;
		}
		else if ( !V_strcasecmp( pParamString, "execute_once" ) )
		{
			if ( !V_strcasecmp( pValueString, "true" ) )
			{
				pStructMem->m_bExecuteOnce = true;
			}
			else
			{
				pStructMem->m_bExecuteOnce = false;
			}
			return true;
		}

		// is this accessing into the array?
		// "parse" it, UGLY, CLEAN IT... move it to "HasIndice"
		//		int nInputIndex = BaseParseHasArrayIndice( pParamString );

		int nInputIndex = -1;
		char tempString[64];
		V_strncpy( tempString, pParamString, sizeof(tempString) );
		char *pInputParamString = std::strtok( tempString, "[" );
		char *pIndexString = strtok( nullptr, "]");
		bool bFillInputArray = false;
		if ( pIndexString && *pIndexString )
		{
			if ( pIndexString[0] == '*' )
			{
				bFillInputArray = true;
			}
			else
			{
				nInputIndex = V_atoi( pIndexString );
			}
		}

		// find the input
		auto itInputParam = m_vInputMap.find( pInputParamString );
		int nInputParamIndex = itInputParam != m_vInputMap.end() ? itInputParam->second : -1;

		if ( ! ks::is_valid_index( m_vInputs, nInputParamIndex ) )
		{
			// Log_Warning( LOG_SND_OPERATORS, "Error: Unable to find input parameter %s", pParamString );
			return false;
		}
		const InputData_t & inputData = m_vInputs[ nInputParamIndex ];
		short nCount = inputData.m_nCount;

		// gotta put some kind of type matching test in here!
		// 		SODataType_t nType = m_vInputType[ nInputParamIndex ];

		int nOffsetOffset = inputData.m_nOffsetOffset;

		int nOutputOffset = ParseValueRef( pStack, pParamString, pValueString );
		if ( nOutputOffset > -1 )
		{
			if ( nInputIndex > -1 )
			{
				if ( nInputIndex < nCount )
				{
					*((int *)((char *)pStructMem + nOffsetOffset) + nInputIndex) = nOutputOffset;
				}
				else
				{
					Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s, illegal array access: %s", pStack->m_pCurrentOperatorName, pParamString );
				}
				return true;
			}
			else
			{
				for ( int i = 0; i < nCount; i++ )
				{
					if ( bFillInputArray )
					{
						*((int *)((char *)pStructMem + nOffsetOffset) + i) = nOutputOffset;
					}
					else
					{
						*((int *)((char *)pStructMem + nOffsetOffset) + i) = ( nOutputOffset + (sizeof(float)*i) );

					}
				}
			}
			return true;
		}
		else
		{
			// if an input is explicit, fill array with single value
			int nInputOffset = inputData.m_nValueOffset;
			float flValue = RandomInterval( ReadInterval( pValueString ) );
			//S_GetFloatFromString( &flValue, pValueString );

			if ( nInputIndex > -1 )
			{
				if ( nInputIndex < nCount )
				{
					*((float *)( (char *)pStructMem + nInputOffset ) + nInputIndex ) = flValue;

					// clear potential previous connections
					*((int *)((char *)pStructMem + nOffsetOffset) + nInputIndex) = -1;

				}
				else
				{
					//error
					return true;
				}
			}
			else
			{
				for ( int i = 0; i < nCount; i++ )
				{
					*((float *)( (char *)pStructMem + nInputOffset ) + i )= flValue;

					// clear potential previous connections
					*((int *)((char *)pStructMem + nOffsetOffset) + i) = nOutputOffset;
				}
			}
			return true;
		}
	}
	return false;
}

int CSosOperator::GetOutputOffset( const char *pOutputName ) const
{
	auto itOutput = m_vOutputMap.find( pOutputName );
	int nIndex = itOutput != m_vOutputMap.end() ? itOutput->second : -1;
	if ( !ks::is_valid_index( m_vOutputs, nIndex ) )
	{
		Log_Warning( LOG_SND_OPERATORS, "Error: Unable to find referenced sound operator output: %s", pOutputName );
		return -1;
	}
	return m_vOutputs[ nIndex ].m_nValueOffset;
}

void CSosOperator::ResolveInputValue( void *pInputDefault, int *pOffsetArray, short nDataCount, SODataType_t nDataType, void *pStackMem ) const
{
	std::size_t nDataSize = 0;
	switch( nDataType )
	{
	case SO_FLOAT:
		nDataSize = sizeof( float );
		break;
	default:
		break;
	}

	for ( int i = 0; i < nDataCount; i++ )
	{
		int nOffset = *( pOffsetArray + i );
		void *pSrc = nullptr;
		if ( nOffset > -1 )
		{

			// 			pSrc = (float *)( (char *)pStackMem + nOffset );
			pSrc = ( void *)( (char *)pStackMem + nOffset );
			V_memcpy( ( void *) ( (char *) pInputDefault + ( i * nDataSize ) ) , pSrc, nDataSize );
		}
		// 		else
		// 		{
		// 			// copying from itself to itself?
		// 			pSrc = ( void *) ( (char *) pInputDefault + ( i * nDataSize ) );
		// 		}
		// 
		// 
		// 		V_memcpy( pInputDefault[i] , pSrc, nDataSize );
		//	pInputDefault[i] = *pSrc;
	}

}

void CSosOperator::ResolveInputValues( void *pStructMem, void *pStackMem ) const
{
	for ( const InputData_t &inputData : m_vInputs )
	{
		SODataType_t nDataType = inputData.m_Type;
		short nDataCount = inputData.m_nCount;

		std::size_t nOffset = inputData.m_nValueOffset;
		int nOffsetOffset = inputData.m_nOffsetOffset;
		void *pInput = (void *)( (char *)pStructMem + nOffset );
		int *pOffset = (int *)( (char *)pStructMem + nOffsetOffset );

		ResolveInputValue( pInput, pOffset, nDataCount, nDataType, pStackMem );

	}
}

void CSosOperator::OffsetConnections( void *pVoidMem, std::size_t nOffset ) const
{
	// point offsets to a different memory segment
	for ( const InputData_t &inputData : m_vInputs )
	{
//		SODataType_t nDataType = inputData.m_Type;
		short nDataCount = inputData.m_nCount;

		int nOffsetOffset = inputData.m_nOffsetOffset;
		int *pOffset = (int *)( (char *)pVoidMem + nOffsetOffset );

		for( int j = 0;  j < nDataCount; j++ )
		{
			if ( *(pOffset + j) > -1 )
			{
				*(pOffset  + j) += nOffset;
			}
		}
	}
}

void CSosOperator::StackInit( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex )
{

}
void CSosOperator::StackShutdown( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex )
{
	if ( snd_sos_show_operator_shutdown.GetInt() )
	{
		Log_Msg( LOG_SND_OPERATORS, OpColor, "Stack Shutdown: Stack: %s : Operator: %s\n", pStack->GetName(), pStack->GetOperatorName( nOpIndex ) );	
	}
}
