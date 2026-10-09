// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: A redirection tool that allows the DLLs to reside elsewhere.

import std;

import ks.core;

typedef int (*LauncherMain_t)( int argc, char **argv );





//-----------------------------------------------------------------------------
// Purpose: Return the directory where this .exe is running from
// Output : char
//-----------------------------------------------------------------------------


int main( int argc, char *argv[] )
{
	const char *pLauncherPath = "bin/libengine.so";

	ks::platform::library *launcher = ks::platform::load_library( pLauncherPath, nullptr );
	
	if ( !launcher )
	{
		std::printf( "Failed to load the launcher (%s)\n", ks::platform::library_error() );
		while(1);
		return 0;
	}
	
	LauncherMain_t main = (LauncherMain_t)ks::platform::library_symbol( launcher, "LauncherMain" );
	if ( !main )
	{
		std::printf( "Failed to load the launcher entry proc\n" );
		while(1);
		return 0;
	}

	return main( argc, argv );
}
