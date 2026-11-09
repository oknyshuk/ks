// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.tier1:registry;

import std;
export import :interface;
export import :convar;

export extern "C++" {

namespace ks {

//-----------------------------------------------------------------------------
// Everything a program component registers while its statics are constructed. The engine and
// each game side (ks_client, ks_server) own one, so that code which exists once in the
// executable (tier1, tier2, ...) still files the registrations of a component in that
// component's registry.
//-----------------------------------------------------------------------------
struct registry
{
	constexpr registry() = default;

	InterfaceReg			*interfaces = nullptr;			// InterfaceReg chain: CreateInterface()
	ConCommandRegistry		convars;						// ConVar_Register()

	// The interface called pName, or null; *pReturnCode is IFACE_OK or IFACE_FAILED.
	void *create_interface( const char *pName, int *pReturnCode ) const;
};

// The registry of the engine and of everything that does not belong to a game side.
registry &engine_registry();

// The registries of the game sides, ks_server and ks_client (game/ks_side.cmake).
registry &server_registry();
registry &client_registry();

// The registry statics are currently being constructed into: the engine's, except between
// enter_registry() and leave_registry(), which every game translation unit calls around its
// own statics (the build generates the calls, see ks_game_side()).
registry &current_registry();
bool enter_registry( registry &r );
bool leave_registry();

}

}
