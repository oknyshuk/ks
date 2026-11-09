// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Entry point of srceng. The engine, game server and game client are linked into
// this executable, so there is nothing to locate or load: call straight into the launcher.

import ks.tier1;
#include "game_entry.h"

// Both game sides are linked into this executable.
static void *ServerFactory( const char *pName, int *pReturnCode ) { return ks::server_registry().create_interface( pName, pReturnCode ); }
static void *ClientFactory( const char *pName, int *pReturnCode ) { return ks::client_registry().create_interface( pName, pReturnCode ); }
CreateInterfaceFn ServerGameFactory() { return &ServerFactory; }
CreateInterfaceFn ClientGameFactory() { return &ClientFactory; }

extern "C" int LauncherMain( int argc, char **argv );

int main( int argc, char *argv[] )
{
	return LauncherMain( argc, argv );
}
