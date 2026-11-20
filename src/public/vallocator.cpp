// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
#if defined(CLIENT_DLL)
#endif
import std;
import ks.core.memalloc;
import ks.platform;
import ks.simd;
#if defined(CLIENT_DLL)
#endif
#include "vallocator.h"

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
