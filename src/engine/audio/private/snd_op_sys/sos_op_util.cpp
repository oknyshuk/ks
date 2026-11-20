// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

import std;



import ks.core.color;
import ks.core.logging;
import ks.platform;
import ks.simd;
import ks.tier1.keyvalues;
#include "sound.h"
#include "soundservice.h"
#include "filesystem.h"

#include "snd_channels.h"
#include "snd_audio_source.h"
#include "snd_dev_xaudio.h"

#include "audio/private/posix_stubs.h"

#include "sos_op.h"
#include "sos_op_util.h"

extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;


//-----------------------------------------------------------------------------
// CSosOperatorPrintFloat
// Catchall operator 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorPrintFloat, "util_print_float" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorPrintFloat, m_flInput, SO_SINGLE, "input" );
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorPrintFloat, "util_print_float"  )

void CSosOperatorPrintFloat::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorPrintFloat_t *pStructMem = (CSosOperatorPrintFloat_t *)pVoidMem;
	SOS_INIT_INPUT_VAR( m_flInput, SO_SINGLE, 0.0 )

}
void CSosOperatorPrintFloat::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorPrintFloat_t *pStructMem = (CSosOperatorPrintFloat_t *)pVoidMem;

	Log_Msg( LOG_SND_OPERATORS, OpColor, "SOS PRINT FLOAT: %s: %f\n", pStack->GetOperatorName( nOpIndex ), pStructMem->m_flInput[0] );

}

void CSosOperatorPrintFloat::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
//	CSosOperatorPrintFloat_t *pStructMem = (CSosOperatorPrintFloat_t *)pVoidMem;

}
void CSosOperatorPrintFloat::OpHelp( ) const
{

}
void CSosOperatorPrintFloat::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorPrintFloat_t *pStructMem = (CSosOperatorPrintFloat_t *)pVoidMem;

	KeyValues *pParams = pOpKeys->GetFirstSubKey();
	while ( pParams )
	{
		const char *pParamString = pParams->GetName();
		const char *pValueString = pParams->GetString();
		if ( pParamString && *pParamString )
		{
			if ( pValueString && *pValueString )
			{
				if ( BaseParseKV( pStack, (CSosOperator_t *)pStructMem, pParamString, pValueString ) )
				{

				}
				else
				{
					Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s, unknown sound operator attribute %s\n",  pStack->m_pCurrentOperatorName, pParamString );
				}
			}
		}
		pParams = pParams->GetNextKey();
	}
}
