#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

// Purpose: Forwards game events to remote IP address. This is to facilitate the recording
// of the play tests and to be able to index into the recorded video based on events.

import ks.tier1;
#include "igameevents.h"
import std;

class CGameEventTransmitter
{
public:
	CGameEventTransmitter();
	~CGameEventTransmitter();
	
	bool	Init();
	void	TransmitGameEvent( IGameEvent *event );
	bool	SetIPAndPort( const char *address );
	
private:

	netadr_t m_Adr;

};

extern CGameEventTransmitter &g_GameEventTransmitter;
