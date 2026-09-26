//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//


#include <malloc.h>
#include "vallocator.h"
#include "basetypes.h"

#include "tier0/memalloc.h"
VStdAllocator g_StdAllocator;

void* VStdAllocator::Alloc(unsigned long size)
{
	if(size)
	{
		void *ret = MemAlloc_Alloc(size);
		return ret;
	}
	else
		return 0;
}

void VStdAllocator::Free(void *ptr)
{
	g_pMemAlloc->Free(ptr);
}

