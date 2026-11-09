// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

//-----------------------------------------------------------------------------
// This is just a little redirection tool so I can get all the dlls in bin
//-----------------------------------------------------------------------------



import std;

import ks.core;
import ks.tier1;
#include "game_entry.h"

extern "C" int DedicatedMain( int argc, char *argv[] );


// The game server is linked into this executable, there is no game client.
static void *ServerFactory( const char *pName, int *pReturnCode ) { return ks::server_registry().create_interface( pName, pReturnCode ); }
CreateInterfaceFn ServerGameFactory() { return &ServerFactory; }
CreateInterfaceFn ClientGameFactory() { return nullptr; }

//-----------------------------------------------------------------------------
// Purpose: Return the directory where this .exe is running from
// Output : char
//-----------------------------------------------------------------------------

static char *GetBaseDir( const char *pszBuffer )
{
	static char	basedir[ k_cMaxPath ];
	char szBuffer[ k_cMaxPath ];
	std::size_t j;
	char *pBuffer = nullptr;

	std::strcpy( szBuffer, pszBuffer );

	pBuffer = std::strrchr( szBuffer,'\\' );
	if ( pBuffer )
	{
		*(pBuffer+1) = '\0';
	}

	std::strcpy( basedir, szBuffer );

	j = std::strlen( basedir );
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
	char* pPath = std::getenv("LD_LIBRARY_PATH");
	char szBuffer[4096];
	char cwd[ k_cMaxPath ];
	if ( !ks::platform::current_directory( cwd, sizeof(cwd) ) )
	{
		std::printf( "getcwd failed" );
	}
	
	std::snprintf( szBuffer, sizeof( szBuffer ) - 1, "%s/bin:%s", cwd, pPath );
	std::printf( "LD_LIBRARY_PATH=%s\n", szBuffer );
	int ret = ks::platform::set_environment( "LD_LIBRARY_PATH", szBuffer, true ) ? 0 : -1;
	if ( ret )	
	{
		std::printf( "failed to set LD_LIBRARY_PATH\n" );
	}

	// The engine and the game server are linked into this executable.
	return DedicatedMain( argc, argv );
}
