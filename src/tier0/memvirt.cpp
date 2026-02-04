//===== Copyright 2010, Valve Corporation, All rights reserved. ======//
//
// Purpose: Virtual memory sections management!
//
// $NoKeywords: $
//===========================================================================//


#include "pch_tier0.h"

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)

//#include <malloc.h>
#include <string.h>
#include "tier0/dbg.h"
#include "tier0/stacktools.h"
#include "tier0/memalloc.h"
#include "tier0/memvirt.h"
#include "tier0/fasttimer.h"
#include "mem_helpers.h"
#ifdef PLATFORM_WINDOWS_PC
#undef WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <crtdbg.h>
#endif
#ifdef OSX
#include <malloc/malloc.h>
#include <stdlib.h>
#endif

#include <map>
#include <set>
#include <limits.h>
#include "tier0/threadtools.h"

// Virtual memory manager is not supported on PC/Linux
IVirtualMemorySection * VirtualMemoryManager_AllocateVirtualMemorySection( size_t numMaxBytes )
{
	return NULL;
}

void VirtualMemoryManager_Shutdown()
{
}

IVirtualMemorySection *GetMemorySectionForAddress( void *pAddress )
{
	return NULL;
}

#endif // !STEAM && !NO_MALLOC_OVERRIDE
