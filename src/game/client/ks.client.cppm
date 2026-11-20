// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.client;

import ks.tier1.convar;
import ks.tier1.createinterface;
import ks.tier1.registry;

export namespace ks::client
{
	// The interfaces the client exposes to the engine (its InterfaceRegs, see side_registry()).
	void *create_interface( const char *pName, int *pReturnCode );
}
