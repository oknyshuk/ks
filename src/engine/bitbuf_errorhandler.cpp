// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "bitbuf_errorhandler.h"
import std;

// Thread-safe so bf_write / bf_read stay thread safe; this error case is expected to happen rarely.
static ks::intern_pool_mt g_ErrorNames[BITBUFERROR_NUM_ERRORS];

void EngineBitBufErrorHandler( BitBufErrorType errorType, const char *pDebugName )
{
	if ( !pDebugName )
	{
		pDebugName = "(unknown)";
	}

	// Only print an error a couple times.
	if ( !g_ErrorNames[ errorType ].contains( pDebugName ) )
	{
		g_ErrorNames[ errorType ].intern( pDebugName );

		if ( errorType == BITBUFERROR_VALUE_OUT_OF_RANGE )
		{
			Warning( "Error in bitbuf [%s]: out of range value. Debug in bitbuf_errorhandler.cpp\n", pDebugName );
		}
		else if ( errorType == BITBUFERROR_BUFFER_OVERRUN )
		{
			Warning( "Error in bitbuf [%s]: buffer overrun. Debug in bitbuf_errorhandler.cpp\n", pDebugName );
		}
	}

	contract_assert( 0 );
}


void InstallBitBufErrorHandler()
{
	SetBitBufErrorHandler( EngineBitBufErrorHandler );
}
