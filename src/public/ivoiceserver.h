#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: This module defines the IVoiceServer interface, which is used by
//			game code to control which clients are listening to which other
//			clients' voice streams.

import std;
import ks.tier1;

inline constexpr const char *INTERFACEVERSION_VOICESERVER = "VoiceServer002";


class IVoiceServer
{
public:
	virtual			~IVoiceServer()	{}

	// Use these to setup who can hear whose voice.
	// Pass in client indices (which are their ent indices - 1).
	virtual bool	GetClientListening(int iReceiver, int iSender) = 0;
	virtual bool	SetClientListening(int iReceiver, int iSender, bool bListen) = 0;
	virtual bool	SetClientProximity(int iReceiver, int iSender, bool bUseProximity) = 0;
};
