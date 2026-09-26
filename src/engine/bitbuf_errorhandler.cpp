//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "bitbuf.h"
#include "bitbuf_errorhandler.h"
#include "tier0/dbg.h"
#include "tier0/threadtools.h"
#include "tier1/intern_pool.h"

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

	Assert( 0 );
}


void InstallBitBufErrorHandler()
{
	SetBitBufErrorHandler( EngineBitBufErrorHandler );
}






