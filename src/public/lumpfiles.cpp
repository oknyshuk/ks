//====== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "bspfile.h"
#include "strtools.h"
#include "filesystem.h"
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
	Q_snprintf( lumpfilename, iBufferSize, "%s_l_%d.lmp", lumppre, iIndex );
}

