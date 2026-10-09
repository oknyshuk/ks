// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Engine implementation of services required by the audio subsystem

import ks.tier1;
#include "quakedef.h"
#include "cdll_int.h"
#include "soundservice.h"
#include "zone.h"
#include "cdll_engine_int.h"
#include "gl_model_private.h"
#include "icliententity.h"
#include "icliententitylist.h"
#include "mouthinfo.h"
#include "host.h"
#include "igame.h"
#include "client.h"
#include "server.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "sound.h"
#include "localize/ilocalize.h"
#include "engineui.h"
#include "toolframework/itoolframework.h"
import std;
#include "MapReslistGenerator.h"
#include "cl_main.h"

void Snd_Restart_f();

inline constexpr const char *MAPLIST_FILE = "maplist.txt";

class CEngineSoundServices : public ISoundServices
{
public:
	CEngineSoundServices() { m_frameTime = 0; }

	virtual void *LevelAlloc( int nBytes, const char *pszTag )
	{
		return Hunk_AllocName(nBytes, pszTag);
	}

	virtual void OnExtraUpdate()
	{
		if ( g_ClientDLL && game && game->IsActiveApp() )
		{
			g_ClientDLL->IN_Accumulate();
		}
	}

	virtual bool GetSoundSpatialization( int entIndex, SpatializationInfo_t& info )
	{
		if ( !entitylist )
		{
			return false;
		}

		// Entity has been deleted
		IClientEntity *pClientEntity = entitylist->GetClientEntity( entIndex );
		if ( !pClientEntity )
		{
			// FIXME:  Should this assert?
			return false;
		}
		
		MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );
		bool bResult = pClientEntity->GetSoundSpatialization( info );

		return bResult;
	}

	virtual bool GetToolSpatialization( int iUserData, int guid, SpatializationInfo_t& info )
	{

		return toolframework->GetSoundSpatialization( iUserData, guid, info );
	}

	virtual float GetClientTime()
	{
		return GetBaseLocalClient().GetTime();
	}

	// Filtered local time
	virtual float GetHostTime()
	{
		return host_time;
	}

	virtual int GetViewEntity( int nSlot )
	{
		if ( g_ClientDLL != nullptr )
		{
			const int nInEyeEntity = g_ClientDLL->GetInEyeEntity();
			if (nInEyeEntity >= 0)
				return nInEyeEntity;
		}

		return GetLocalClient( nSlot ).GetViewEntity();
	}

	virtual void SetSoundFrametime( float realDt, float hostDt )
	{
		if ( cl_movieinfo.IsRecording() )
		{
			m_frameTime = hostDt;
		}
		else
		{
			m_frameTime = realDt;
		}
	}

	virtual float GetHostFrametime()
	{
		return m_frameTime;
	}

	virtual int GetServerCount()
	{
		return GetBaseLocalClient().m_nServerCount;
	}

	virtual bool IsPlayer( SoundSource source )
	{
		if ( source == GetSpectatorTarget( nullptr ) )
		{
			return true;
		}

		if ( splitscreen->IsLocalPlayerResolvable() )
		{
			return ( source == GetLocalClient().m_nPlayerSlot + 1 );
		}

		FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
		{
			if ( GetLocalClient( i ).m_nPlayerSlot + 1 == source )
				return true;
		}

		return false;
	}

	virtual int GetSpectatorTarget( ClientDLLObserverMode_t *pObserverMode )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		return ClientDLL_GetSpectatorTarget( pObserverMode );
	}

	virtual void OnChangeVoiceStatus( int entity, int iSsSlot, bool status )
	{
		ClientDLL_VoiceStatus( entity, iSsSlot, status );
	}

	virtual bool GetPlayerAudible( int iPlayerIndex )
	{
		return ClientDLL_IsPlayerAudible( iPlayerIndex );
	}

	virtual bool IsConnected() 
	{
		return GetBaseLocalClient().IsConnected();
	}

	// Calls into client .dll with list of close caption tokens to construct a caption out of
	virtual void EmitSentenceCloseCaption( char const *tokenstream )
	{
		if ( g_ClientDLL )
		{
			g_ClientDLL->EmitSentenceCloseCaption( tokenstream );
		}
	}

	virtual void EmitCloseCaption( char const *captionname, float duration )
	{
		if ( g_ClientDLL )
		{
			g_ClientDLL->EmitCloseCaption( captionname, duration );
		}
	}

	virtual char const *GetGameDir() 
	{
		return com_gamedir;
	}

	// If the game is paused, certain audio will pause, too (anything with phoneme/sentence data for now)
	virtual bool IsGamePaused()
	{
		extern IVEngineClient *engineClient;
		if ( !engineClient )
		{
			contract_assert( !"No engineClient, bug???" );
			return false;
		}

		return engineClient->IsPaused();
	}

	virtual void RestartSoundSystem()
	{
		Snd_Restart_f();
	}

	virtual void GetAllManifestFiles( std::set< FileNameHandle_t >& list )
	{
		list.clear();

		// Load them in
		FileHandle_t resfilehandle = g_pFileSystem->Open( MAPLIST_FILE, "rb", "MOD" );
		if ( FILESYSTEM_INVALID_HANDLE != resfilehandle )
		{
			// Read in and parse mapcycle.txt
			int length = g_pFileSystem->Size(resfilehandle);
			if ( length > 0 )
			{
				char *pStart = (char *)new char[ length + 1 ];
				if ( pStart && ( length == g_pFileSystem->Read(pStart, length, resfilehandle) )
				   )
				{
					pStart[ length ] = 0;
					const char *pFileList = pStart;

					while ( 1 )
					{
						pFileList = COM_Parse( pFileList );
						if ( std::strlen( com_token ) <= 0 )
							break;

						char manifest_file[ 512 ];
						V_snprintf( manifest_file, sizeof( manifest_file ), "%s/%s.manifest", AUDIOSOURCE_CACHE_ROOTDIR, com_token );

						if ( g_pFileSystem->FileExists( manifest_file, "MOD" ) )
						{
							FileNameHandle_t handle = g_pFileSystem->FindOrAddFileName( manifest_file );
							list.insert( handle );
						}

						// Any more tokens on this line?
						while ( COM_TokenWaiting( pFileList ) )
						{
							pFileList = COM_Parse( pFileList );
						}
					}
				}
				delete[] pStart;
			}

			g_pFileSystem->Close(resfilehandle);
		}
		else
		{
			Warning( "GetAllManifestFiles:  Unable to load %s\n", MAPLIST_FILE );
		}
	}

	virtual void GetAllSoundFilesInManifest( std::set< FileNameHandle_t >& list, char const *manifestfile )
	{
		list.clear();
		CacheSoundsFromResFile( true, list, manifestfile, false );
	}

	virtual void GetAllSoundFilesReferencedInReslists( std::set< FileNameHandle_t >& list )
	{
		char reslistdir[ MAX_PATH ];
		V_strncpy( reslistdir, MapReslistGenerator().GetResListDirectory(), sizeof( reslistdir ) );
		list.clear();

		// Load them in
		FileHandle_t resfilehandle = g_pFileSystem->Open( MAPLIST_FILE, "rb", "MOD" );
		if ( FILESYSTEM_INVALID_HANDLE != resfilehandle )
		{
			// Read in and parse mapcycle.txt
			int length = g_pFileSystem->Size(resfilehandle);
			if ( length > 0 )
			{
				char *pStart = (char *)new char[ length + 1 ];
				if ( pStart && ( length == g_pFileSystem->Read(pStart, length, resfilehandle) )
				   )
				{
					pStart[ length ] = 0;
					const char *pFileList = pStart;

					while ( 1 )
					{
						char resfile[ 512 ];

						pFileList = COM_Parse( pFileList );
						if ( std::strlen( com_token ) <= 0 )
							break;

						V_snprintf( resfile, sizeof( resfile ), "%s\\%s.lst", reslistdir, com_token );

						CacheSoundsFromResFile( false, list, resfile );

						// Any more tokens on this line?
						while ( COM_TokenWaiting( pFileList ) )
						{
							pFileList = COM_Parse( pFileList );
						}
					}
				}
				delete[] pStart;
			}

			g_pFileSystem->Close(resfilehandle);

			CacheSoundsFromResFile( false, list, CFmtStr( "%s\\engine.lst", reslistdir ) );
			CacheSoundsFromResFile( false, list, CFmtStr( "%s\\all.lst", reslistdir ) );
		}
		else
		{
			Warning( "GetAllSoundFilesReferencedInReslists:  Unable to load file %s\n", MAPLIST_FILE );
		}
	}

	virtual void CacheBuildingStart()
	{

		EngineUI()->ActivateGameUI();
		EngineUI()->StartCustomProgress();
		const wchar_t *str = g_pLocalize->Find( "#Valve_CreatingCache" );
		if ( str )
		{
			EngineUI()->UpdateCustomProgressBar( 0.0f, str );
		}
	}

	virtual void CacheBuildingUpdateProgress( float percent, char const *cachefile )
	{

		const wchar_t *format = g_pLocalize->Find( "Valve_CreatingSpecificSoundCache" );
		if ( format )
		{
			wchar_t constructed[ 1024 ];
			wchar_t file[ 256 ];
			g_pLocalize->ConvertANSIToUnicode( cachefile, file, sizeof( file ) );

			g_pLocalize->ConstructString( 
				constructed, 
				sizeof( constructed ),
				( wchar_t * )format,
				1,
				file );

			EngineUI()->UpdateCustomProgressBar( percent, constructed );
		}
	}

	virtual void CacheBuildingFinish()
	{

		EngineUI()->FinishCustomProgress();
		EngineUI()->HideGameUI();
	}

	virtual int GetPrecachedSoundCount()
	{
		if ( !sv.IsActive() )
			return 0;

		INetworkStringTable *table = sv.GetSoundPrecacheTable();
		if ( !table )
			return 0;

		return table->GetNumStrings();
	}

	virtual char const *GetPrecachedSound( int index )
	{
		contract_assert( sv.IsActive() );

		INetworkStringTable *table = sv.GetSoundPrecacheTable();
		if ( !table )
			return "";

		return table->GetString( index );
	}

	virtual bool ShouldSuppressNonUISounds()
	{
		return EngineUI()->IsGameUIVisible() || IsGamePaused();
	}

	virtual char const *GetUILanguage()
	{
		extern ConVar cl_language;
		return cl_language.GetString();
	}

private:

	float m_frameTime;

	void CacheSoundsFromResFile( bool quiet, std::set< FileNameHandle_t >& list, char const *resfile, bool checkandcleanname = true )
	{
		if ( !g_pFileSystem->FileExists( resfile, "MOD" ) )
		{
			Warning( "CacheSoundsFromResFile:  Unable to find '%s'\n", resfile );
			return;
		}

		int oldCount = (int)list.size();

		FileHandle_t resfilehandle = g_pFileSystem->Open( resfile, "rb", "MOD" );
		if ( FILESYSTEM_INVALID_HANDLE != resfilehandle )
		{
			// Read in and parse mapcycle.txt
			int length = g_pFileSystem->Size(resfilehandle);
			if ( length > 0 )
			{
				char *pStart = (char *)new char[ length + 1 ];
				if ( pStart && ( length == g_pFileSystem->Read(pStart, length, resfilehandle) )
				   )
				{
					pStart[ length ] = 0;
					const char *pFileList = pStart;

					while ( 1 )
					{
						pFileList = COM_Parse( pFileList );
						if ( std::strlen( com_token ) <= 0 )
							break;

						if ( checkandcleanname )
						{
							if ( V_stristr( com_token, ".wav" ) ||
								 V_stristr( com_token, ".mp3" ) )
							{
								// skip past the game/mod directory		   "hl2/sound/player/footstep.wav"
								V_FixSlashes(com_token);				// "hl2\sound\player\footstep.wav"
								const char *pName = com_token;
								while (pName[0] && pName[0] != CORRECT_PATH_SEPARATOR)
								{
									pName++;
								}										// "\sound\player\footstep.wav"
								FileNameHandle_t handle = g_pFileSystem->FindOrAddFileName( pName+1 );   // "sound\player\footstep.wav"
								list.insert( handle );
							}
						}
						else
						{
							FileNameHandle_t handle = g_pFileSystem->FindOrAddFileName( com_token );
							list.insert( handle );
						}
					}
				}
				delete[] pStart;
			}

			g_pFileSystem->Close(resfilehandle);
		}

		int newCount = (int)list.size();

		if ( !quiet )
		{
			Msg( "Processing (%i new) from %s\n", newCount - oldCount, resfile );
		}
	}

	virtual void OnSoundStarted( int guid, StartSoundParams_t& params, char const *soundname )
	{
		CVProfScope _( "OnSoundStarted", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

		// Don't send the sound message to the tool framework if active tool 
		// is not recording or the sound originated from the tool.
		if ( !toolframework->IsToolRecording() || params.bToolSound )
			return;

		KeyValues *msg = new KeyValues( "StartSound" );
		msg->SetInt( "guid", guid );
		msg->SetFloat( "time", GetBaseLocalClient().GetTime() );
		msg->SetBool( "staticsound", params.staticsound );
		msg->SetInt( "soundsource", params.soundsource );
		msg->SetInt( "entchannel", params.entchannel );
		msg->SetString( "soundname", soundname );
		msg->SetFloat( "originx", params.origin.x );
		msg->SetFloat( "originy", params.origin.y );
		msg->SetFloat( "originz", params.origin.z );
		msg->SetFloat( "directionx", params.direction.x );
		msg->SetFloat( "directiony", params.direction.y );
		msg->SetFloat( "directionz", params.direction.z );
		msg->SetInt( "updatepositions", params.bUpdatePositions );
		msg->SetFloat( "fvol", params.fvol );
		msg->SetInt( "soundlevel", (int)params.soundlevel );
		msg->SetInt( "flags", params.flags );
		msg->SetInt( "pitch", params.pitch );
		msg->SetBool( "fromserver", params.fromserver );
		msg->SetFloat( "delay", params.delay );
		msg->SetInt( "speakerentity", params.speakerentity );

		toolframework->PostMessage( msg );

		msg->deleteThis();
	}

	virtual void OnSoundStopped( int guid, int soundsource, int channel, char const *soundname )
	{
		// NOTE: At the moment, if we don't receive a StartSound message but we do
		// receive a StopSound message, the StopSound message is ignored. In a perfect
		// world, if the StartSound message was not sent, a StopSound message should not
		// be sent for that guid either. This requires more plumbing, though, and
		// for the moment, it's not necessary to do that plumbing.

		CVProfScope _( "OnSoundStopped", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

		if ( !toolframework->IsToolRecording() )
			return;

		KeyValues *msg = new KeyValues( "StopSound" );
		msg->SetInt( "guid", guid );
		msg->SetFloat( "time", GetBaseLocalClient().GetTime() );
		msg->SetInt( "soundsource", soundsource );
		msg->SetInt( "entchannel", channel );
		msg->SetString( "soundname", soundname );

		toolframework->PostMessage( msg );

		msg->deleteThis();
	}
};


static CEngineSoundServices g_EngineSoundServices;
ISoundServices *g_pSoundServices = &g_EngineSoundServices;
