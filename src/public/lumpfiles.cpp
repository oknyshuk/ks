// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if defined(CLIENT_DLL)
#endif
import std;
#if defined(CLIENT_DLL)
#endif
import ks.core.platform;
import ks.platform;
import ks.simd;
import ks.tier1.strtools;
#include "lumpfiles.h"

//-----------------------------------------------------------------------------
// Purpose: Generate a lump file name for a given bsp & index
// Input  : *bspfilename - 
//			*lumpfilename - 
//			iIndex - 
//-----------------------------------------------------------------------------
void GenerateLumpFileName( const char *bspfilename, char *lumpfilename, int iBufferSize, int iIndex )
{
	char lumppre[MAX_PATH];
	V_StripExtension( bspfilename, lumppre, MAX_PATH );
	V_snprintf( lumpfilename, iBufferSize, "%s_l_%d.lmp", lumppre, iIndex );
}
