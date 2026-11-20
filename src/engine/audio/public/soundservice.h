#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Applicaton-level hooks for clients of the audio subsystem

class CAudioSource;
typedef int SoundSource;
struct SpatializationInfo_t;
typedef void *FileNameHandle_t;
struct StartSoundParams_t;

import ks.platform;
import ks.simd;
import ks.tier1.filename_table;
#include "cdll_int.h"
import std;

//-----------------------------------------------------------------------------
// Purpose: Services required by the audio system to function, this facade
//			defines the bridge between the audio code and higher level
//			systems.
//
//			Note that some of these currently suggest that certain
//			functionality would like to exist at a deeper layer so
//			systems like audio can take advantage of them
//			diectly (toml 05-02-02)
//-----------------------------------------------------------------------------

class ISoundServices
{
public:
	ISoundServices() { m_frameTime = 0; }

	//---------------------------------
	// Allocate a block of memory that will be automatically
	// cleaned up on level change
	//---------------------------------
	void *LevelAlloc( int nBytes, const char *pszTag );

	//---------------------------------
	// Notification that someone called S_ExtraUpdate()
	//---------------------------------
	void OnExtraUpdate();

	//---------------------------------
	// Return false if the entity doesn't exist or is out of the PVS, in which case the sound shouldn't be heard.
	//---------------------------------
	bool GetSoundSpatialization( int entIndex, SpatializationInfo_t& info );

	//---------------------------------
	// This is the client's clock, which follows the servers and thus isn't 100% smooth all the time (it is in single player)
	//---------------------------------
	float GetClientTime();

	//---------------------------------
	// This is the engine's filtered timer, it's pretty smooth all the time
	//---------------------------------
	float GetHostTime();

	//---------------------------------
	//---------------------------------
	int GetViewEntity( int nSlot );

	//---------------------------------
	//---------------------------------
	float GetHostFrametime();
	void SetSoundFrametime( float realDt, float hostDt );

	//---------------------------------
	//---------------------------------
	int GetServerCount();

	//---------------------------------
	//---------------------------------
	bool IsPlayer( SoundSource source );

	// -1 if local player not spectating anyone, otherwise the index of the player being spectated
	int GetSpectatorTarget( ClientDLLObserverMode_t *pObserverMode );

	//---------------------------------
	//---------------------------------
	void OnChangeVoiceStatus( int entity, int iSsSlot, bool status);

	// returns false if the player can't hear the other client due to game rules (eg. the other team)
	bool GetPlayerAudible( int iPlayerIndex );

	// Is the player fully connected (don't do DSP processing if not)
	bool IsConnected();

	// Calls into client .dll with list of close caption tokens to construct a caption out of
	void EmitSentenceCloseCaption( char const *tokenstream );
	// Calls into client .dll with list of close caption tokens to construct a caption out of
	void EmitCloseCaption( char const *captionname, float duration );

	char const *GetGameDir();

	// If the game is paused, certain audio will pause, too (anything with phoneme/sentence data for now)
	bool	IsGamePaused();

	// restarts the sound system externally
	void	RestartSoundSystem();

	void	GetAllSoundFilesReferencedInReslists( std::set< FileNameHandle_t >& list );
	void	GetAllManifestFiles( std::set< FileNameHandle_t >& list );
	void	GetAllSoundFilesInManifest( std::set< FileNameHandle_t >& list, char const *manifestfile );

	void	CacheBuildingStart();
	void	CacheBuildingUpdateProgress( float percent, char const *cachefile );
	void	CacheBuildingFinish();

	// For building sound cache manifests
	int			GetPrecachedSoundCount();
	char const *GetPrecachedSound( int index );

	char const *GetUILanguage();

	bool ShouldSuppressNonUISounds();

private:
	void CacheSoundsFromResFile( bool quiet, std::set< FileNameHandle_t >& list, char const *resfile, bool checkandcleanname = true );

	float m_frameTime;
};

//-------------------------------------

extern ISoundServices *g_pSoundServices;

//=============================================================================
