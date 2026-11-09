// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "string_t.h"
#include "eiface.h"
#include "shareddefs.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "soundscape_system.h"
#include "soundscape.h"
#include "filesystem.h"
#include "game.h"

#define SOUNDSCAPE_MANIFEST_FILE				"scripts/soundscapes_manifest.txt"

static void soundscape_flush( const CCommand &args );
static ConCommand soundscape_flush_command( "soundscape_flush", soundscape_flush, "Flushes the server & client side soundscapes" );
static void soundscape_flush( const CCommand &args )
{
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() );
	if ( engine->IsDedicatedServer() )
	{
		// If it's a dedicated server, only the server console can run this.
		if ( pPlayer )
			return;
	}
	else
	{
		// If it's a listen server, only the listen server host can run this.
		if ( !pPlayer || pPlayer != UTIL_GetListenServerHost() )
			return;
	}

	g_SoundscapeSystem.FlushSoundscapes();	// don't bother forgetting about the entities
	g_SoundscapeSystem.Init();

	
	if ( engine->IsDedicatedServer() )
	{
		// If the ds console typed it, send it to everyone.
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer	*pSendToPlayer = UTIL_PlayerByIndex( i );
			if ( pSendToPlayer )
				engine->ClientCommand( pSendToPlayer->edict(), "cl_soundscape_flush\n" );
		}
	}
	else
	{
		engine->ClientCommand( pPlayer->edict(), "cl_soundscape_flush\n" );
	}
}

CSoundscapeSystem g_SoundscapeSystem( "CSoundscapeSystem" );

extern ConVar soundscape_debug;

void CSoundscapeSystem::AddSoundscapeFile( const char *filename )
{
	// Open the soundscape data file, and abort if we can't
	KeyValues *pKeyValuesData = new KeyValues( filename );
	if ( filesystem->LoadKeyValues( *pKeyValuesData, IFileSystem::TYPE_SOUNDSCAPE, filename, "GAME" ) )
	{
		// parse out all of the top level sections and save their names
		KeyValues *pKeys = pKeyValuesData;
		while ( pKeys )
		{
			if ( pKeys->GetFirstSubKey() )
			{
				if ( g_pDeveloper->GetBool() )
				{
					if ( std::strstr( pKeys->GetName(), "{" ) )
					{
						Msg("Error parsing soundscape file %s after %s\n", filename, m_soundscapeCount>0 ?m_soundscapes.GetStringText( m_soundscapeCount-1 ) : "FIRST" );
					}
				}
				m_soundscapes.AddString( pKeys->GetName(), m_soundscapeCount );

				m_soundscapeCount++;
			}
			pKeys = pKeys->GetNextKey();
		}
	}
	pKeyValuesData->deleteThis();
}

static void sv_soundscape_printdebuginfo( const CCommand &args );
static ConCommand sv_soundscape_printdebuginfo_command( "sv_soundscape_printdebuginfo", sv_soundscape_printdebuginfo, "print soundscapes", FCVAR_CHEAT );
static void sv_soundscape_printdebuginfo( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	g_SoundscapeSystem.PrintDebugInfo();
}


void CSoundscapeSystem::PrintDebugInfo()
{
	Msg( "\n------- SERVER SOUNDSCAPES -------\n" );
	for ( auto key=m_soundscapes.First(); key != m_soundscapes.InvalidIndex(); key = m_soundscapes.Next( key ) )
	{
		int id = m_soundscapes.GetIDForKey( key );
		const char *pName = m_soundscapes.GetStringForKey( key );

		Msg( "- %d: %s\n", id, pName );
	}
	Msg( "-------- SOUNDSCAPE ENTITIES -----\n" );
	for( int entityIndex = 0; entityIndex < std::ssize( m_soundscapeEntities ); ++entityIndex )
	{
		CEnvSoundscape *currentSoundscape = m_soundscapeEntities[entityIndex];
		Msg("- %d: %s x:%.4f y:%.4f z:%.4f\n", 
			entityIndex, 
			STRING(currentSoundscape->GetSoundscapeName()), 
			currentSoundscape->GetAbsOrigin().x,
			currentSoundscape->GetAbsOrigin().y,
			currentSoundscape->GetAbsOrigin().z
			);
	}
	Msg( "----------------------------------\n\n" );
}

bool CSoundscapeSystem::Init()
{
	m_soundscapeCount = 0;

	const char *mapname = STRING( gpGlobals->mapname );
	const char *mapSoundscapeFilename = nullptr;
	if ( mapname && *mapname )
	{
		mapSoundscapeFilename = UTIL_VarArgs( "scripts/soundscapes_%s.txt", V_GetFileName( mapname ) );
	}

	KeyValues *manifest = new KeyValues( SOUNDSCAPE_MANIFEST_FILE );
	if ( filesystem->LoadKeyValues( *manifest, IFileSystem::TYPE_SOUNDSCAPE, SOUNDSCAPE_MANIFEST_FILE, "GAME" ) )
	{
		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != nullptr; sub = sub->GetNextKey() )
		{
			if ( !V_stricmp( sub->GetName(), "file" ) )
			{
				// Add
				AddSoundscapeFile( sub->GetString() );
				if ( mapSoundscapeFilename && FStrEq( sub->GetString(), mapSoundscapeFilename ) )
				{
					mapSoundscapeFilename = nullptr; // we've already loaded the map's soundscape
				}
				continue;
			}

			Warning( "CSoundscapeSystem::Init:  Manifest '%s' with bogus file type '%s', expecting 'file'\n", 
				SOUNDSCAPE_MANIFEST_FILE, sub->GetName() );
		}

		if ( mapSoundscapeFilename && filesystem->FileExists( mapSoundscapeFilename ) )
		{
			AddSoundscapeFile( mapSoundscapeFilename );
		}
	}
	else
	{
		Error( "Unable to load manifest file '%s'\n", SOUNDSCAPE_MANIFEST_FILE );
	}
	manifest->deleteThis();
	m_activeIndex = 0;

	return true;
}

void CSoundscapeSystem::FlushSoundscapes( void )
{
	m_soundscapeCount = 0;
	m_soundscapes.ClearStrings();
}

void CSoundscapeSystem::Shutdown()
{
	FlushSoundscapes();
	m_soundscapeEntities.clear();
	m_activeIndex = 0;

}

void CSoundscapeSystem::LevelInitPreEntity()
{
	g_SoundscapeSystem.Shutdown();
	g_SoundscapeSystem.Init();
}

void CSoundscapeSystem::LevelInitPostEntity()
{
	std::vector< bbox_t > clusterbounds;
	int clusterCount = engine->GetClusterCount();
	clusterbounds.assign( clusterCount, {} );
	engine->GetAllClusterBounds( clusterbounds.data(), clusterCount );
	m_soundscapesInCluster.assign( clusterCount, {} );
	for ( int i = 0; i < clusterCount; i++ )
	{
		m_soundscapesInCluster[i].soundscapeCount = 0;
		m_soundscapesInCluster[i].firstSoundscape = 0;
	}
	unsigned char myPVS[16 * 1024];
	std::vector< short > clusterIndexList;
	std::vector< short > soundscapeIndexList;

	// find the clusters visible from each soundscape
	// add this soundscape to the list of soundscapes for that cluster, clip cluster bounds to radius
	for ( int i = 0; i < std::ssize( m_soundscapeEntities ); i++ )
	{
		Vector position = m_soundscapeEntities[i]->GetAbsOrigin();
		float radius = m_soundscapeEntities[i]->m_flRadius;
		float radiusSq = radius * radius;
		engine->GetPVSForCluster( engine->GetClusterForOrigin( position ), sizeof( myPVS ), myPVS );
		for ( int j = 0; j < clusterCount; j++ )
		{
			if ( myPVS[ j >> 3 ] & (1<<(j&7)) )
			{
				float distSq = CalcSqrDistanceToAABB( clusterbounds[j].mins, clusterbounds[j].maxs, position );
				if ( distSq < radiusSq )
				{
					m_soundscapesInCluster[j].soundscapeCount++;
					clusterIndexList.push_back( j );
					// UNDONE: Technically you just need a soundscape index and a count for this list.
					soundscapeIndexList.push_back( i );
				}
			}
		}
	}

	// basically this part is like a radix sort
	// this is how many entries we need in the soundscape index list
	m_soundscapeIndexList.assign( std::ssize( soundscapeIndexList ), {} );

	// now compute the starting index of each cluster
	int firstSoundscape = 0;
	for ( int i = 0; i < clusterCount; i++ )
	{
		m_soundscapesInCluster[i].firstSoundscape = firstSoundscape;
		firstSoundscape += m_soundscapesInCluster[i].soundscapeCount;
		m_soundscapesInCluster[i].soundscapeCount = 0;
	}
	// now add each soundscape index to the appropriate cluster's list
	// The resulting list is precomputing all soundscapes that need to be checked for a player
	// in each cluster.  This is used to accelerate the per-frame operations
	for ( int i = 0; i < std::ssize( soundscapeIndexList ); i++ )
	{
		int cluster = clusterIndexList[i];
		int outIndex = m_soundscapesInCluster[cluster].soundscapeCount + m_soundscapesInCluster[cluster].firstSoundscape;
		m_soundscapesInCluster[cluster].soundscapeCount++;
		m_soundscapeIndexList[outIndex] = soundscapeIndexList[i];
	}
}

int	CSoundscapeSystem::GetSoundscapeIndex( const char *pName )
{
	return m_soundscapes.GetStringID( pName );
}

bool CSoundscapeSystem::IsValidIndex( int index )
{
	if ( index >= 0 && index < m_soundscapeCount )
		return true;
	return false;
}

void CSoundscapeSystem::AddSoundscapeEntity( CEnvSoundscape *pSoundscape )
{
	if ( ks::find_index( m_soundscapeEntities, pSoundscape ) == -1 )
	{
		int index = std::ssize( m_soundscapeEntities ); m_soundscapeEntities.push_back( pSoundscape );
		pSoundscape->m_soundscapeEntityId = index + 1;
	}
}

void CSoundscapeSystem::RemoveSoundscapeEntity( CEnvSoundscape *pSoundscape )
{
	ks::erase_first( m_soundscapeEntities, pSoundscape );
	pSoundscape->m_soundscapeEntityId = -1;
}

void CSoundscapeSystem::FrameUpdatePostEntityThink()
{
	int total = std::ssize( m_soundscapeEntities );
	if ( total > 0 )
	{
		int traceCount = 0;
		int playerCount = 0;
		// budget to do 2 players with 2 traces each tick.  Anything more expensive than that should
		// get spread across multiple ticks
		int maxPlayers = 2;
		int maxTraces = maxPlayers * 2;
		if ( soundscape_debug.GetBool() )
		{
			maxTraces = 9999;
			maxPlayers = MAX_PLAYERS;
		}

		// load balance across server ticks a bit by limiting the numbers of players (get cluster for origin)
		// and traces processed in a single tick.  In single player this will update the player every tick
		// because it always does at least one player's full load of work
		for ( int i = 0; i < gpGlobals->maxClients && traceCount <= maxTraces && playerCount <= maxPlayers; i++ )
		{
			m_activeIndex = (m_activeIndex+1) % gpGlobals->maxClients;
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( m_activeIndex + 1 );
			if ( pPlayer && pPlayer->IsNetClient() )
			{
				// check to see if this is the sound entity that is 
				// currently affecting this player
				audioparams_t &audio = pPlayer->GetAudioParams();

				// if we got this far, we're looking at an entity that is contending
				// for current player sound. the closest entity to player wins.
				CEnvSoundscape *pCurrent = nullptr;
				if ( audio.entIndex > 0 && audio.entIndex <= std::ssize( m_soundscapeEntities ) )
				{
					int ssIndex = audio.entIndex - 1;
					pCurrent = m_soundscapeEntities[ssIndex];
				}
				ss_update_t update;
				update.pPlayer = pPlayer;
				update.pCurrentSoundscape = pCurrent;
				update.playerPosition = pPlayer->EarPosition();
				update.bInRange = false;
				update.currentDistance = 0;
				update.traceCount = 0;
				if ( pCurrent )
				{
					pCurrent->UpdateForPlayer(update);
				}

				int clusterIndex = engine->GetClusterForOrigin( update.playerPosition );
			
				if ( clusterIndex >= 0 && clusterIndex < std::ssize( m_soundscapesInCluster ) )
				{
					// find all soundscapes that could possibly attach to this player and update them
					for ( int j = 0; j < m_soundscapesInCluster[clusterIndex].soundscapeCount; j++ )
					{
						int ssIndex = m_soundscapeIndexList[m_soundscapesInCluster[clusterIndex].firstSoundscape + j];
						if ( m_soundscapeEntities[ssIndex] == update.pCurrentSoundscape )
							continue;
						m_soundscapeEntities[ssIndex]->UpdateForPlayer( update );
					}
				}
				playerCount++;
				traceCount += update.traceCount;
			}
		}
	}
}

void CSoundscapeSystem::AddSoundscapeSounds( KeyValues *pSoundscape, int soundscapeIndex )
{
	return;
}

void CSoundscapeSystem::PrecacheSounds( int soundscapeIndex )
{
	return;
}
