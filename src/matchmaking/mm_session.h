#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

import ks.core;
#include "matchmaking/imatchframework.h"
#include "matchmaking/imatchevents.h"

class IMatchSessionInternal : public IMatchSession, public IMatchEventsSink
{
public:
	// Run a frame update
	virtual void Update() = 0;

	// Destroy the session object
	virtual void Destroy() = 0;

	// Debug print a session object
	virtual void DebugPrint() = 0;

	// Check if another session is joinable
	virtual bool IsAnotherSessionJoinable( char const *pszAnotherSessionInfo ) = 0;
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

