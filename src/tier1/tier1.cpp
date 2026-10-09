// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

// Purpose: A higher level link library for general use in the game and tools.

import std;
import ks.tier1;

//-----------------------------------------------------------------------------
// Call this to connect to all tier 1 libraries.
// It's up to the caller to check the globals it cares about to see if ones are missing
//-----------------------------------------------------------------------------
void ConnectTier1Libraries( CreateInterfaceFn *pFactoryList, int nFactoryCount )
{
	ConnectInterfaces( pFactoryList, nFactoryCount );
}

void DisconnectTier1Libraries()
{
	DisconnectInterfaces();
}
