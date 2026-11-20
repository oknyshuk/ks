// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

module ks.server;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.createinterface;
import ks.tier1.registry;
import :entity_2;

namespace
{
	// Constant-initialized, so the statics of every unit can register with it.
	constinit ks::registry s_registry;
}

ks::registry &side_registry() { return s_registry; }

void *ks::server::create_interface( const char *pName, int *pReturnCode )
{
	return s_registry.create_interface( pName, pReturnCode );
}
