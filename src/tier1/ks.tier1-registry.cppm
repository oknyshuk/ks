// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.tier1.registry;

import std;
export import ks.tier1.createinterface;
export import ks.tier1.convar;

export extern "C++" {

namespace ks {

//-----------------------------------------------------------------------------
// Everything a program component registers while its statics are constructed. The engine owns
// one; each game module (ks.server, ks.client) owns its own and names it explicitly where it
// registers something: `ConVar x( side_registry(), ... )`, `InterfaceReg r( side_registry(), ... )`.
//-----------------------------------------------------------------------------
struct registry
{
	constexpr registry() = default;

	InterfaceReg			*interfaces = nullptr;			// InterfaceReg chain: CreateInterface()
	ConCommandRegistry		convars;						// ConVar_Register()

	// The interface called pName, or null; *pReturnCode is IFACE_OK or IFACE_FAILED.
	void *create_interface( const char *pName, int *pReturnCode ) const;
};

// The registry of the engine and of everything that does not belong to a game side; what is
// registered without naming a registry goes there.
registry &engine_registry();

}

}
