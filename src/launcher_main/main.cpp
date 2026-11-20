// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Entry point of srceng. The engine, game server and game client are linked into
// this executable, so there is nothing to locate or load: call straight into the launcher.

import ks.platform;
import ks.simd;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
#include "game_entry.h"
import ks.server;
import ks.client;

// Both game sides are linked into this executable.
static void *ServerFactory( const char *pName, int *pReturnCode ) { return ks::server::create_interface( pName, pReturnCode ); }
static void *ClientFactory( const char *pName, int *pReturnCode ) { return ks::client::create_interface( pName, pReturnCode ); }
CreateInterfaceFn ServerGameFactory() { return &ServerFactory; }
CreateInterfaceFn ClientGameFactory() { return &ClientFactory; }

extern "C" int LauncherMain( int argc, char **argv );

int main( int argc, char *argv[] )
{
	return LauncherMain( argc, argv );
}
