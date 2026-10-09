// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

//-----------------------------------------------------------------------------
// This is just a little redirection tool so I can get all the dlls in bin
//-----------------------------------------------------------------------------



import std;

import ks.core;

typedef int (*DedicatedMain_t)( int argc, char *argv[] );


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

	const char *pBinaryName = "bin/libengine.so";

	ks::platform::library *dedicated = ks::platform::load_library( pBinaryName, nullptr );
	if ( !dedicated )
	{
		std::printf( "Failed to open %s (%s)\n", pBinaryName, ks::platform::library_error());
		return -1;
	}
	DedicatedMain_t main = (DedicatedMain_t)ks::platform::library_symbol( dedicated, "DedicatedMain" );
	if ( !main )
	{
		std::printf( "Failed to find dedicated server entry point (%s)\n", ks::platform::library_error() );
		return -1;
	}

	return main( argc, argv );
}
