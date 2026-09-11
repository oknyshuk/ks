//====== Copyright c 1996-2007, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "pch_tier0.h"


#include "tier0/platform.h"
#include "tier0/systeminformation.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"




//
//	Plat_GetMemPageSize
//		Returns the size of a memory page in bytes.
//
uint32 Plat_GetMemPageSize()
{
	return 4;	// Assume unknown page size is 4 Kb
}

//
//	Plat_GetPagedPoolInfo
//		Fills in the paged pool info structure if successful.
//
SYSTEM_CALL_RESULT_t Plat_GetPagedPoolInfo( PAGED_POOL_INFO_t *pPPI )
{
	memset( pPPI, 0, sizeof( *pPPI ) );
	return SYSCALL_UNSUPPORTED;
}



