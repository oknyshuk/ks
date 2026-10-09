// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

import std;


import ks.tier1;

#include "sound.h"
#include "soundservice.h"
#include "filesystem.h"

#include "snd_channels.h"
#include "snd_audio_source.h"
#include "snd_dev_xaudio.h"

#include "audio/private/posix_stubs.h"

#include "sos_op.h"
#include "sos_op_platform.h"

extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;


//-----------------------------------------------------------------------------
// CSosOperatorPlatform
// Setting a single, simple scratch pad Expression 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorPlatform, "sys_platform" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorPlatform, m_flOutput, SO_SINGLE, "output" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorPlatform, "sys_platform"  )

void CSosOperatorPlatform::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorPlatform_t *pStructMem = (CSosOperatorPlatform_t *)pVoidMem;

	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 0.0 )
}

void CSosOperatorPlatform::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
//	CSosOperatorPlatform_t *pStructMem = (CSosOperatorPlatform_t *)pVoidMem;

}

void CSosOperatorPlatform::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	//	CSosOperatorPlatform_t *pStructMem = (CSosOperatorPlatform_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
}
void CSosOperatorPlatform::OpHelp( ) const
{
}

void CSosOperatorPlatform::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	extern IVEngineClient *engineClient;
	CSosOperatorPlatform_t *pStructMem = (CSosOperatorPlatform_t *)pVoidMem;
	KeyValues *pParams = pOpKeys->GetFirstSubKey();
	while ( pParams )
	{
		const char *pParamString = pParams->GetName();
		const char *pValueString = pParams->GetString();
		if ( pParamString && *pParamString )
		{
			if ( pValueString && *pValueString )
			{
				if( BaseParseKV( pStack, (CSosOperator_t *)pStructMem, pParamString, pValueString ))
				{

				}
				else if ( !V_strcasecmp( pParamString, "pc" ) )
				{
					pStructMem->m_flOutput[0] = 1.0;
				}
				else if ( !V_strcasecmp( pParamString, "x360" ) )
				{
				}
				else if ( !V_strcasecmp( pParamString, "ps3" ) )
				{
				}
				else if ( !V_strcasecmp( pParamString, "osx" ) )
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
