#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Engine voice interface

import std;
import ks.core;

inline constexpr const char *IENGINEVOICE_INTERFACE_VERSION = "IEngineVoice001";

enum EngineVoiceFlags_t
{
	ENGINE_VOICE_FLAG_PS3		=	( 1 << 0 )
};

class IEngineVoice
{
public:
	bool IsHeadsetPresent( int iController );
	bool IsLocalPlayerTalking( int iController );

	void AddPlayerToVoiceList( XUID xPlayer, int iController, uint64 uiFlags );
	void RemovePlayerFromVoiceList( XUID xPlayer, int iController );

	void GetRemoteTalkers( int *pNumTalkers, XUID *pRemoteTalkers );
	
	bool VoiceUpdateData( int iController );
	void GetVoiceData( int iController, const byte **ppvVoiceDataBuffer, unsigned int *pnumVoiceDataBytes );
	void VoiceResetLocalData( int iController );

	void SetPlaybackPriority( XUID remoteTalker, int iController, int iAllowPlayback );
	void PlayIncomingVoiceData( XUID xuid, const byte *pbData, unsigned int dwDataSize, const bool *bAudiblePlayers = nullptr );

	void RemoveAllTalkers();
};
