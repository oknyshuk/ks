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
	virtual bool IsHeadsetPresent( int iController ) = 0;
	virtual bool IsLocalPlayerTalking( int iController ) = 0;

	virtual void AddPlayerToVoiceList( XUID xPlayer, int iController, uint64 uiFlags ) = 0;
	virtual void RemovePlayerFromVoiceList( XUID xPlayer, int iController ) = 0;

	virtual void GetRemoteTalkers( int *pNumTalkers, XUID *pRemoteTalkers ) = 0;
	
	virtual bool VoiceUpdateData( int iController ) = 0;
	virtual void GetVoiceData( int iController, const byte **ppvVoiceDataBuffer, unsigned int *pnumVoiceDataBytes ) = 0;
	virtual void VoiceResetLocalData( int iController ) = 0;

	virtual void SetPlaybackPriority( XUID remoteTalker, int iController, int iAllowPlayback ) = 0;
	virtual void PlayIncomingVoiceData( XUID xuid, const byte *pbData, unsigned int dwDataSize, const bool *bAudiblePlayers = nullptr ) = 0;

	virtual void RemoveAllTalkers() = 0;
};
