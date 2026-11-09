#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

import ks.core;
#include "matchmaking/imatchframework.h"
#include "matchmaking/imatchevents.h"

// Non-virtual facade: the only implementation is CMatchSessionOfflineCustom.
class IMatchSessionInternal : public IMatchSession
{
public:
	// Match events
	void OnEvent( KeyValues *pEvent );

	// Run a frame update
	void Update();

	// Destroy the session object
	void Destroy();

	// Debug print a session object
	void DebugPrint();

	// Check if another session is joinable
	bool IsAnotherSessionJoinable( char const *pszAnotherSessionInfo );
};

#include "protocol.h"


#include "mm_session_offline_custom.h"

void MatchSession_BroadcastSessionSettingsUpdate( KeyValues *pUpdateDeletePackage );
void MatchSession_PrepareClientForConnect( KeyValues *pSettings, uint64 uiReservationCookieOverride = 0ull );

uint64 MatchSession_GetMachineFlags();
char const * MatchSession_GetTuInstalledString();

enum MatchSessionMachineFlags_t
{
	MACHINE_PLATFORM_PS3	=	( 1 << 0 ),			// Machine is PS3
};

