#pragma once
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// The Connect/Disconnect/Init/Shutdown steps the CTier1/2/3AppSystem helpers used to add around a
// system's own code. Systems that are no longer IAppSystems (see AppSystemRef) call them explicitly,
// in the same order as the helpers did:
//   Connect:    tier 1 libraries, tier 2 libraries, tier 3 libraries   (as many as the system's tier)
//   Disconnect: tier 3, tier 2, tier 1
//   Init:       register the cvars (tier 1 and above)
//   Shutdown:   unregister the cvars

import std;
import ks.tier1;
#include "tier2/tier2.h"
#include "tier3/tier3.h"

inline bool AppSystemTier1Connect( CreateInterfaceFn factory )
{
	ConnectTier1Libraries( &factory, 1 );
	return true;
}

inline bool AppSystemTier2Connect( CreateInterfaceFn factory )
{
	AppSystemTier1Connect( factory );
	ConnectTier2Libraries( &factory, 1 );
	return true;
}

inline bool AppSystemTier3Connect( CreateInterfaceFn factory )
{
	AppSystemTier2Connect( factory );
	ConnectTier3Libraries( &factory, 1 );
	return true;
}

inline void AppSystemTier1Disconnect()
{
	DisconnectTier1Libraries();
}

inline void AppSystemTier2Disconnect()
{
	DisconnectTier2Libraries();
	AppSystemTier1Disconnect();
}

inline void AppSystemTier3Disconnect()
{
	DisconnectTier3Libraries();
	AppSystemTier2Disconnect();
}

inline InitReturnVal_t AppSystemTier1Init( int nConVarFlag = 0 )
{
	if ( g_pCVar )
	{
		ConVar_Register( nConVarFlag );
	}
	return INIT_OK;
}

inline void AppSystemTier1Shutdown()
{
	if ( g_pCVar )
	{
		ConVar_Unregister( );
	}
}
