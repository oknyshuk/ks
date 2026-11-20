#pragma once


#include "engine/ienginevoice.h"
import ks.core.basetypes;
import ks.core.platform;
import ks.core.types;
import ks.core.wchartypes;

class CEngineVoiceStub : public IEngineVoice
{
public:
	bool IsHeadsetPresent( int iController ) { return false; }
	bool IsLocalPlayerTalking( int iController ) { return false; }

	void AddPlayerToVoiceList( XUID xPlayer, int iController, uint64 uiFlags ) {}
	void RemovePlayerFromVoiceList( XUID xPlayer, int iController ) {}

	void GetRemoteTalkers( int *pNumTalkers, XUID *pRemoteTalkers )
	{
		if ( pNumTalkers )
			*pNumTalkers = 0;
	}

	bool VoiceUpdateData( int iController ) { return false; }
	void GetVoiceData( int iController, const byte **ppvVoiceDataBuffer, unsigned int *pnumVoiceDataBytes )
	{
		if ( ppvVoiceDataBuffer )
			*ppvVoiceDataBuffer = nullptr;
		if ( pnumVoiceDataBytes )
			*pnumVoiceDataBytes = 0;
	}
	void VoiceResetLocalData( int iController ) {}

	void SetPlaybackPriority( XUID remoteTalker, int iController, int iAllowPlayback ) {}
	void PlayIncomingVoiceData( XUID xuid, const byte *pbData, unsigned int dwDataSize, const bool *bAudiblePlayers = nullptr ) {}

	void RemoveAllTalkers() {}
};

CEngineVoiceStub *Audio_GetEngineVoiceStub();

