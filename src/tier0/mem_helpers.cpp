//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "tier0/platform.h"
#include "tier0/icommandline.h"
#include "tier0/dbg.h"
#include "mem_helpers.h"
#include <string.h>
//#include <malloc.h>

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

// Needed for debugging
bool g_bInitMemory = true;

void DoApplyMemoryInitializations( void *pMem, size_t nSize )
{
}

size_t CalcHeapUsed()
{
	return 0;
}

