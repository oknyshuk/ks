//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: A redirection tool that allows the DLLs to reside elsewhere.
//
//=====================================================================================//

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <limits.h>
#include <string.h>
#define MAX_PATH PATH_MAX

#include "tier0/platform.h"
#include "tier0/basetypes.h"

#if defined( VPCGAME )
#define _VPCGAME_STRING_HACK2(x) #x
#define _VPCGAME_STRING_HACK1(x) _VPCGAME_STRING_HACK2(x)
#define VPCGAME_STRING _VPCGAME_STRING_HACK1(VPCGAME)
#endif

typedef int (*LauncherMain_t)( int argc, char **argv );





//-----------------------------------------------------------------------------
// Purpose: Return the directory where this .exe is running from
// Output : char
//-----------------------------------------------------------------------------


int main( int argc, char *argv[] )
{
	const char *pLauncherPath = "bin/libengine" DLL_EXT_STRING;

	void *launcher = dlopen( pLauncherPath, RTLD_NOW );
	
	if ( !launcher )
	{
		printf( "Failed to load the launcher (%s)\n", dlerror() );
		while(1);
		return 0;
	}
	
	LauncherMain_t main = (LauncherMain_t)dlsym( launcher, "LauncherMain" );
	if ( !main )
	{
		printf( "Failed to load the launcher entry proc\n" );
		while(1);
		return 0;
	}

	return main( argc, argv );
}



