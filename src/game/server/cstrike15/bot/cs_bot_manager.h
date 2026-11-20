// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#pragma once

#define CCSBOTMANAGER_ITERATE_BOTS( Callback, arg1 ) \
	{ \
		for ( int idx = 1; idx <= gpGlobals->maxClients; ++idx ) \
		{ \
			CBasePlayer *player = UTIL_PlayerByIndex( idx ); \
			if (player == nullptr) continue; \
			if (!player->IsBot()) continue; \
			CCSBot *bot = static_cast< CCSBot * >(player); \
			bot->Callback( arg1 ); \
		} \
	}
