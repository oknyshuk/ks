//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "pch_tier0.h"

#include "tier0/minidump.h"
#include "tier0/platform.h"

#include "tier0/minidump.h"

PLATFORM_INTERFACE void WriteMiniDump( const char *pszFilenameSuffix )
{
}

PLATFORM_INTERFACE void CatchAndWriteMiniDump( FnWMain pfn, int argc, tchar *argv[] )
{
	pfn( argc, argv );
}

