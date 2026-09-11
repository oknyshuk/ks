//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//-----------------------------------------------------------------------------
// This is just a little redirection tool so I can get all the dlls in bin
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#define MAX_PATH PATH_MAX

#include "tier0/basetypes.h"

typedef int (*DedicatedMain_t)( int argc, char *argv[] );


//-----------------------------------------------------------------------------
// Purpose: Return the directory where this .exe is running from
// Output : char
//-----------------------------------------------------------------------------

static char *GetBaseDir( const char *pszBuffer )
{
	static char	basedir[ MAX_PATH ];
	char szBuffer[ MAX_PATH ];
	size_t j;
	char *pBuffer = NULL;

	strcpy( szBuffer, pszBuffer );

	pBuffer = strrchr( szBuffer,'\\' );
	if ( pBuffer )
	{
		*(pBuffer+1) = '\0';
	}

	strcpy( basedir, szBuffer );

	j = strlen( basedir );
	if (j > 0)
	{
		if ( ( basedir[ j-1 ] == '\\' ) || 
			 ( basedir[ j-1 ] == '/' ) )
		{
			basedir[ j-1 ] = 0;
		}
	}

	return basedir;
}

#define stringize(a) #a
#define dedicated_binary(a,b,c) a stringize(b) c 

int main( int argc, char *argv[] )
{
	// Must add 'bin' to the path....
	char* pPath = getenv("LD_LIBRARY_PATH");
	char szBuffer[4096];
	char cwd[ MAX_PATH ];
	if ( !getcwd( cwd, sizeof(cwd)) )
	{
		printf( "getcwd failed (%s)", strerror(errno));
	}
	
	snprintf( szBuffer, sizeof( szBuffer ) - 1, "LD_LIBRARY_PATH=%s/bin:%s", cwd, pPath );
	printf( "%s\n", szBuffer );
	int ret = putenv( szBuffer );
	if ( ret )	
	{
		printf( "%s\n", strerror(errno) );
	}

	const char *pBinaryName = "bin/libengine" DLL_EXT_STRING;

	void *dedicated = dlopen( pBinaryName, RTLD_NOW );
	if ( !dedicated )
	{
		printf( "Failed to open %s (%s)\n", pBinaryName, dlerror());
		return -1;
	}
	DedicatedMain_t main = (DedicatedMain_t)dlsym( dedicated, "DedicatedMain" );
	if ( !main )
	{
		printf( "Failed to find dedicated server entry point (%s)\n", dlerror() );
		return -1;
	}

	return main( argc, argv );
}
