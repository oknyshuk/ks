// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation

import ks.core.sysinfo;
import ks.core.types;
import ks.platform;
import ks.simd;

// First include standard libraries
import std;


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
	std::memset( pPPI, 0, sizeof( *pPPI ) );
	return SYSCALL_UNSUPPORTED;
}
