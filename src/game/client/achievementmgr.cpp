// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include "edict.h"
#include "iachievementmgr.h"
#include "igameevents.h"
#include "matchmaking/imatchframework.h"
#include "studio.h"
#include "tier2/fileutils.h"
#include "usermessages_schema.h"
#include "cdll_client_int.h"
#include "filesystem.h"
#include "stringregistry.h"
#include "activitylist.h"
#include "iclientrenderable.h"
#include "materialsystem/imaterialproxy.h"
#include "imaterialproxydict.h"
#include "materialsystem/imaterial.h"
#include "bone_setup.h"
#include "ai_activity.h"
module ks.client;

import std;
import ks.core.dbg;
import ks.core.platform;
import ks.core.threadtools;
import ks.core.types;
import ks.core.vprof;
import ks.game.cstrike15.cs_achievementdefs;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.cvar;
import ks.tier1.iconvar;
import ks.tier1.icvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.strtools;
import ks.tier1.vector_ops;
import :particlemgr;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :baseentity;
import :sharedvar;
import :util;
import :cstrike15.cs_player;
import :cstrike15.cs_shareddefs;
import :precache;
import ks.game.shareddefs;
import :achievements_and_stats_interface;
import :cstrike15.cs_player_shared;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :noopanimcontroller;
import ks.tier1.map_ops;
import ks.game.ai_activity;
import ks.game.entity_cast;
import ks.tier1.string_ops;
import ks.core.basetypes;
import ks.core.random;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.math.vmatrix;

class CAchievementMgr;
class CBaseAchievement;
class CBaseAnimating;
class CBaseEntity;
class CBasePlayer;
class CCSPlayer;

using namespace ::ks::reflect;

ConVar	cc_achievement_debug(side_registry(), "achievement_debug", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "Turn on achievement debug msgs." );

// We want to debug getting achievements in various development scenarios (including with cheats enabled). This convar allows bypassing
// the normal check against the cheats when launched with -dev. We need to be very careful not to ship with this enabled on any platforms.
// #if defined(_CERT)
// #else
// #define ALLOW_ACHIEVEMENTS_WITH_CHEATS 1
// #endif

extern ConVar developer;

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

static int AchievementIDCompare( CBaseAchievement * const *ach1, CBaseAchievement * const *ach2 )
{
	return (*ach1)->GetAchievementID() > (*ach2)->GetAchievementID();
}

static int AchievementOrderCompare( CBaseAchievement * const *ach1, CBaseAchievement * const *ach2 )
{
	return (*ach1)->GetDisplayOrder() - (*ach2)->GetDisplayOrder();
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CAchievementMgr::CAchievementMgr() : CAutoGameSystemPerFrame( "CAchievementMgr" )
{
	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		m_flLastClassChangeTime[i] = 0;
		m_flTeamplayStartTime[i] = 0;
		m_iMiniroundsCompleted[i] = 0;
		m_bDirty[i] = false;

		m_AchievementsAwardedDuringCurrentGame[i].clear();
		m_bUserSlotActive[i] = false;
	}

	m_szMap[0] = 0;
	m_bCheatsEverOn = false;

	m_bCheckSigninState = true;
	m_bReadingFromTitleData = false;

}

//#if defined (_X360)
void CAchievementMgr::ResetProfileInfo()
{
	m_UIProfileInfo.m_BotDifficulty = 0;
	m_UIProfileInfo.m_GameMode = 0;
	m_UIProfileInfo.m_IsPublic = true;
	m_UIProfileInfo.m_Map = 0;

	m_UIProfileInfo.m_LeaderboardFilter = 0;
	m_UIProfileInfo.m_LeaderboardMode = 0;
	m_UIProfileInfo.m_LeaderboardType = 0;
}
//#endif

//-----------------------------------------------------------------------------
// Purpose: Initializer
//-----------------------------------------------------------------------------
bool CAchievementMgr::Init()
{
	// We can be created on either client (for multiplayer games) or server
	// (for single player), so register ourselves with the engine so UI has a uniform place 
	// to go get the pointer to us

	// register ourselves
	engine->SetAchievementMgr( GetInstanceInterface() );

	// register for events
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "player_stats_updated" );
	ListenForGameEvent( "achievement_write_failed" );
	ListenForGameEvent( "user_data_downloaded" );
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		m_UMCMsgAchievementEvent.Bind< ks::net::CS_UM_AchievementEvent, ks::net::CCSUsrMsg_AchievementEvent >( MsgFunc_AchievementEvent );
	}
	ListenForGameEvent( "read_game_titledata" );
	ListenForGameEvent( "write_game_titledata" );
	ListenForGameEvent( "reset_game_titledata" ); 
	ListenForGameEvent( "write_profile_data" );

	g_pMatchFramework->GetEventsSubscription()->Subscribe( this, [this]( KeyValues *pEvent ) { OnEvent( pEvent ); } );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Called at init time after all systems are init'd.  We have to
//			do this in PostInit because the game directory is not available earlier.
//-----------------------------------------------------------------------------
void CAchievementMgr::PostInit()
{
	// get current game dir
	const char *pGameDir = COM_GetModDirectory();

	CBaseAchievementHelper *pAchievementHelper = CBaseAchievementHelper::s_pFirst;
	while ( pAchievementHelper )
	{
		for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
		{
			// create and initialize all achievements and insert them in our maps
			CBaseAchievement *pAchievement = pAchievementHelper->m_pfnCreate();
			pAchievement->m_pAchievementMgr = this;
			pAchievement->Init();
			pAchievement->CalcProgressMsgIncrement();
			pAchievement->SetUserSlot( i );

			// only add an achievement if it does not have a game filter (only compiled into the game it
			// applies to, or truly cross-game) or, if it does have a game filter, the filter matches current game.
			// (e.g. EP 1/2/... achievements are in shared binary but are game specific, they have a game filter for runtime check.)
			const char *pGameDirFilter = pAchievement->m_pGameDirFilter;
			if ( !pGameDirFilter || ( 0 == V_strcmp( pGameDir, pGameDirFilter ) ) )
			{
				if ( ( !pAchievement->IsAssetAward() ) )
				{
					// We don't insert achievements with asset awards on the PC.
					// IDs are unique; a duplicate would be dropped (and deleted) here
					auto [it, bInserted] = m_mapAchievement[i].emplace( pAchievement->GetAchievementID(), std::unique_ptr< CBaseAchievement >( pAchievement ) );
					contract_assert( bInserted );
				}
				else
				{
					delete pAchievement;
				}
			}
			else
			{
				// achievement is not for this game, don't use it
				delete pAchievement;
			}
		}

		pAchievementHelper = pAchievementHelper->m_pNext;
	}
	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		// Add each of the achievements to a CUtlVector ordered by achievement ID
		for ( const auto &[nID, pEntry] : m_mapAchievement[i] )
		{
			if ( !pEntry->IsAssetAward() )
			{
				m_vecAchievement[i].push_back( pEntry.get() );
			}
			else
			{
				m_vecAward[i].push_back( pEntry.get() );
			}
		}
		ks::qsort( m_vecAchievement[i], AchievementIDCompare );

		m_vecAchievementInOrder[i].append_range( m_vecAchievement[i] );
		ks::qsort( m_vecAchievementInOrder[i], AchievementOrderCompare );

		m_vecAwardInOrder[i].append_range( m_vecAward[i] );
		ks::qsort( m_vecAwardInOrder[i], AchievementOrderCompare );

		// Clear the progress and achieved data for each splitscreen player
		ClearAchievementData( i );
	}

	UserConnected( SINGLE_PLAYER_SLOT );
}

//-----------------------------------------------------------------------------
// Purpose: Shuts down the achievement manager.
//-----------------------------------------------------------------------------
void CAchievementMgr::Shutdown()
{
	SaveGlobalState();

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		m_mapAchievement[i].clear();
		m_vecAchievement[i].clear();
		m_vecAward[i].clear();
		m_vecAchievementInOrder[i].clear();
		m_vecAwardInOrder[i].clear();
		m_vecKillEventListeners[i].clear();
		m_vecMapEventListeners[i].clear();
		m_vecComponentListeners[i].clear();
		m_AchievementsAwardedDuringCurrentGame[i].clear();
	}

	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
}

//-----------------------------------------------------------------------------
// Purpose: Cleans up all achievements and then re-initializes them
//-----------------------------------------------------------------------------
void CAchievementMgr::InitializeAchievements( )
{
	Shutdown();
	PostInit();
}

extern const ConVar *sv_cheats;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAchievementMgr::SetAchievementThink( CBaseAchievement *pAchievement, float flThinkTime )
{
	// Is the achievement already in the think list?
	int iCount = std::ssize( m_vecThinkListeners );
	for ( int i = 0; i < iCount; i++ )
	{
		if ( m_vecThinkListeners[i].pAchievement == pAchievement )
		{
			if ( flThinkTime == THINK_CLEAR )
			{
				m_vecThinkListeners.erase( m_vecThinkListeners.begin() + i );
				return;
			}

			m_vecThinkListeners[i].m_flThinkTime = gpGlobals->curtime + flThinkTime;
			return;
		}
	}

	if ( flThinkTime == THINK_CLEAR )
		return;

	// Otherwise, add it to the list
	int iIdx = std::ssize( m_vecThinkListeners ); m_vecThinkListeners.emplace_back();
	m_vecThinkListeners[iIdx].pAchievement = pAchievement;
	m_vecThinkListeners[iIdx].m_flThinkTime = gpGlobals->curtime + flThinkTime;
}

//-----------------------------------------------------------------------------
// Purpose: called on level init
//-----------------------------------------------------------------------------
void CAchievementMgr::LevelInitPreEntity()
{
	m_bCheatsEverOn = false;

	// sb: need to make sure we enable achievement manager on the client in split screen??

		// For multiplayer games, achievement mgr must live on the client.  (Only the client can read/write player state.)
		contract_assert( GameRules()->IsMultiplayer() );

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		// clear list of achievements listening for events
		m_vecKillEventListeners[i].clear();
		m_vecMapEventListeners[i].clear();
		m_vecComponentListeners[i].clear();

		m_flLastClassChangeTime[i] = 0;
		m_flTeamplayStartTime[i] = 0;
		m_iMiniroundsCompleted[i] = 0;
	}

	// client and server have map names available in different forms (full path on client, just file base name on server), 
	// cache it in base file name form here so we don't have to have different code paths each time we access it
	V_FileBase( engine->GetLevelName(), m_szMap, std::size( m_szMap ) );

	if ( PLATFORM_EXT[0] )
	{
		// need to remove the .360 extension on the end of the map name
		char *pExt = V_stristr( m_szMap, PLATFORM_EXT );
		if ( pExt )
		{
			*pExt = '\0';
		}
	}

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		// Don't bother listening if a slot is not active
		if ( m_bUserSlotActive[i] )
		{
			// look through all achievements, see which ones we want to have listen for events
			for ( const auto &[nID, pEntry] : m_mapAchievement[i] )
			{
				CBaseAchievement *pAchievement = pEntry.get();

				// if the achievement only applies to a specific map, and it's not the current map, skip it
				const char *pMapNameFilter = pAchievement->m_pMapNameFilter;
				if ( pMapNameFilter && ( 0 != V_strcmp( m_szMap, pMapNameFilter ) ) )
					continue;

				// if the achievement needs kill events, add it as a listener
				if ( pAchievement->GetFlags() & ACH_LISTEN_KILL_EVENTS )
				{
					m_vecKillEventListeners[i].push_back( pAchievement );
				}
				// if the achievement needs map events, add it as a listener
				if ( pAchievement->GetFlags() & ACH_LISTEN_MAP_EVENTS )
				{
					m_vecMapEventListeners[i].push_back( pAchievement );
				}
				// if the achievement needs map events, add it as a listener
				if ( pAchievement->GetFlags() & ACH_LISTEN_COMPONENT_EVENTS )
				{
					m_vecComponentListeners[i].push_back( pAchievement );
				}
				if ( pAchievement->IsActive() )
				{
					pAchievement->ListenForEvents();
				}
			}
			m_flLevelInitTime[i] = gpGlobals->curtime;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: called on level shutdown
//-----------------------------------------------------------------------------
void CAchievementMgr::LevelShutdownPreEntity()
{
	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		// make all achievements stop listening for events 
		for ( const auto &[nID, pEntry] : m_mapAchievement[i] )
		{
			CBaseAchievement *pAchievement = pEntry.get();
			pAchievement->StopListeningForAllEvents();
		}
	}

	// save global state if we have any changes
	SaveGlobalStateIfDirty();
}

//$FIXME(hpe) we should probably guard for out of index nUserSlot values in all of these getachievement functions
//-----------------------------------------------------------------------------
// Purpose: returns achievement for specified ID
//-----------------------------------------------------------------------------
CBaseAchievement *CAchievementMgr::GetAchievementByID( int iAchievementID, int nUserSlot )
{
	contract_assert( nUserSlot < MAX_SPLITSCREEN_PLAYERS );
	if( nUserSlot >= MAX_SPLITSCREEN_PLAYERS )
	{
		return nullptr;
	}

	auto it = m_mapAchievement[nUserSlot].find( iAchievementID );
	if ( it != m_mapAchievement[nUserSlot].end() )
	{
		return it->second.get();
	}
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: returns achievement with specified name.  NOTE: this iterates through
//			all achievements to find the name, intended for debugging purposes.
//			Use GetAchievementByID for fast lookup.
//-----------------------------------------------------------------------------
CBaseAchievement *CAchievementMgr::GetAchievementByName( const char *pchName, int nUserSlot )
{
	CVProfScope _( "GetAchievementByName", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	contract_assert( nUserSlot < MAX_SPLITSCREEN_PLAYERS );
	if( nUserSlot >= MAX_SPLITSCREEN_PLAYERS )
	{
		return nullptr;
	}

	for ( const auto &[nID, pEntry] : m_mapAchievement[nUserSlot] )
	{
		CBaseAchievement *pAchievement = pEntry.get();
		if ( pAchievement && 0 == ( V_stricmp( pchName, pAchievement->GetName() ) ) )
			return pAchievement;
	}
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the achievement with the specified name has been achieved
//-----------------------------------------------------------------------------
bool CAchievementMgr::HasAchieved( const char *pchName, int nUserSlot )
{
	CBaseAchievement *pAchievement = GetAchievementByName( pchName, nUserSlot );
	if ( pAchievement )
		return pAchievement->IsAchieved();
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: marks the user slot as disconnected and clears its achievement data
//-----------------------------------------------------------------------------
void CAchievementMgr::UserDisconnected( int nUserSlot )
{
	m_bUserSlotActive[nUserSlot] = false;

	ClearAchievementData( nUserSlot );
}

//-----------------------------------------------------------------------------
// Read our achievement data from the title data.
//-----------------------------------------------------------------------------
void CAchievementMgr::ReadAchievementsFromTitleData( int iController, int iSlot )
{
	IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
	if ( !pPlayer )
		return;

	m_bReadingFromTitleData = true;

	// Set the incremental achievement progress and components
	for ( int i=0; i<std::ssize( m_vecAchievement[iSlot] ); ++i )
	{
		CBaseAchievement *pAchievement = m_vecAchievement[iSlot][i];
		if ( pAchievement )
		{
			pAchievement->ReadProgress( pPlayer );
		}
	}

	for ( int i=0; i<std::ssize( m_vecAward[iSlot] ); ++i )
	{
		CBaseAchievement *pAward = m_vecAward[iSlot][i];
		if ( pAward )
		{
			pAward->ReadProgress( pPlayer );
		}
	}

	m_bReadingFromTitleData = false;

	SaveGlobalStateIfDirty();
}

//-----------------------------------------------------------------------------
// Purpose: marks the user slot as connected
//-----------------------------------------------------------------------------
void CAchievementMgr::UserConnected( int nUserSlot )
{
	m_bUserSlotActive[SINGLE_PLAYER_SLOT] = true;
}

//-----------------------------------------------------------------------------
// Purpose: saves global state to file
//-----------------------------------------------------------------------------
void CAchievementMgr::SaveGlobalState( )
{
	int iController = 0;

	int j = SINGLE_PLAYER_SLOT;
	CVProfScope _( "CAchievementMgr::SaveGlobalState", 0, "Achievements", false, BUDGETFLAG_OTHER );
	{

		IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
		if ( !pPlayer )
			return;

		for ( int i = 0; i < std::ssize( m_vecAchievement[j] ); ++i )
		{
			CBaseAchievement *pAchievement = m_vecAchievement[j][i];
			if ( pAchievement )
			{
				pAchievement->WriteProgress( pPlayer );
			}
		}

		for ( int i = 0; i < std::ssize( m_vecAward[j] ); ++i )
		{
			CBaseAchievement *pAward = m_vecAward[j][i];
			if ( pAward )
			{
				pAward->WriteProgress( pPlayer );
			}
		}

		m_bDirty[iController] = false;
	}

}

void CAchievementMgr::SendResetProfileEvent()
{
	IGameEvent * resetUserProfileEvent = gameeventmanager->CreateEvent( "reset_user_profile" );
	if (resetUserProfileEvent)
		gameeventmanager->FireEventClientSide( resetUserProfileEvent );
}

void CAchievementMgr::SendWriteProfileEvent()
{
	IGameEvent * writeUserProfileEvent = gameeventmanager->CreateEvent( "write_user_profile" );
	if (writeUserProfileEvent)
		gameeventmanager->FireEventClientSide( writeUserProfileEvent );
}

//-----------------------------------------------------------------------------
// Purpose: saves global state to file if there have been any changes
//-----------------------------------------------------------------------------
void CAchievementMgr::SaveGlobalStateIfDirty( )
{
	if ( m_bDirty && !m_bReadingFromTitleData )
	{
		SaveGlobalState( );
	}
}

bool CAchievementMgr::IsAchievementAllowedInGame( int iAchievementID )
{
	// Offline modes with trivial bots disable ALL achievements
	if ( CSGameRules() && !CSGameRules()->IsAwardsProgressAllowedForBotDifficulty() )
		return false;

	bool isGunGameAchievement = false;

	switch( iAchievementID )
	{
		// a list of gun game achievements
		//case CSFastRoundWin:
		case CSGunGameKillKnifer:
		case CSWinEveryGGMap:
		case CSGunGameProgressiveRampage:
		case CSGunGameFirstKill:
		case CSFirstBulletKills:
		case CSGunGameConservationist:
		case CSWinMatchAR_SHOOTS:
		case CSWinMatchAR_BAGGAGE:
		case CSPlantBombsTRLow:
		case CSDefuseBombsTRLow:
		case CSKillEnemyTerrTeamBeforeBombPlant:
		case CSKillEnemyCTTeamBeforeBombPlant:
		case CSWinMatchDE_LAKE:
		case CSWinMatchDE_SAFEHOUSE:   
		case CSWinMatchDE_SUGARCANE:
		case CSWinMatchDE_STMARC:
		case CSWinMatchDE_BANK:
		case CSWinMatchDE_EMBASSY:
		case CSWinMatchDE_DEPOT:
		case CSWinMatchDE_SHORTTRAIN:
		//case CSHipShot:
		case CSBornReady:
		case CSSpawnCamper:
		case CSGunGameKnifeKillKnifer:
		case CSGunGameSMGKillKnifer:
		case CSStillAlive:
		case CSGGWinRoundsLow:
		case CSGGWinRoundsMed:
		case CSGGWinRoundsHigh:
		case CSGGWinRoundsExtreme:
		case CSGGWinRoundsUltimate:
		case CSGGRoundsLow:
		case CSGGRoundsMed:
		case CSGGRoundsHigh:
		case CSPlayEveryGGMap:
		case CSDominationsLow:
		case CSDominationsHigh:
		case CSRevengesLow:
		case CSRevengesHigh:
		case CSDominationOverkillsLow:
		case CSDominationOverkillsHigh:
		case CSDominationOverkillsMatch:
		case CSExtendedDomination:
		case CSConcurrentDominations:
		//case CSAvengeFriend:
			isGunGameAchievement = true;
	};

	// Only unlock gungame achievements in gun game modes
	if( isGunGameAchievement ) 
	{
		return CSGameRules()->IsPlayingGunGame();
	}
	else
	{
		// Other achievements are valid for all game types.
		return true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: awards specified achievement
//-----------------------------------------------------------------------------
void CAchievementMgr::AwardAchievement( int iAchievementID, int nUserSlot )
{
	if( !IsAchievementAllowedInGame( iAchievementID ) )
		return;

	CBasePlayer *pPlayerLocal = CBasePlayer::GetLocalPlayer();
	CCSPlayer* pPlayer = ToCSPlayer(pPlayerLocal);

	if( ( pPlayer && pPlayer->IsControllingBot() ) ) // if we're controlling a bot, no achievements for us!
		return;

	CBaseAchievement *pAchievement = GetAchievementByID( iAchievementID, nUserSlot );
	contract_assert( pAchievement );
	if ( !pAchievement )
		return;

	if ( !CheckAchievementsEnabled() )
	{
		Msg( "Achievements disabled, ignoring achievement unlock for %s\n", pAchievement->GetName() );
		return;
	}

	if ( pAchievement->IsAchieved() )
	{
		if ( cc_achievement_debug.GetInt() > 0 )
		{
			Msg( "Achievement award called but already achieved: %s\n", pAchievement->GetName() );
		}
		return;
	}
	pAchievement->SetAchieved( true );

	pAchievement->OnAchieved();

	IGameEvent * event = gameeventmanager->CreateEvent( "achievement_earned_local" , true );
	if ( event )
	{
		event->SetInt( "achievement", pAchievement->GetAchievementID() );
		event->SetInt( "splitscreenplayer", nUserSlot );
		gameeventmanager->FireEventClientSide( event );
	}

	if ( cc_achievement_debug.GetInt() > 0 )
	{
		Msg( "Achievement awarded: %s\n", pAchievement->GetName() );
	}

	// save state at next good opportunity.  (Don't do it immediately, may hitch at bad time.)
	m_bDirty[nUserSlot] = true;	

	SaveGlobalStateIfDirty();

	// Add this one to the list of achievements earned during current session
	m_AchievementsAwardedDuringCurrentGame[nUserSlot].push_back( iAchievementID );

	// let achievements that depend on other achievements re-evaluate
	for ( const auto &[nID, pEntry] : m_mapAchievement[nUserSlot] )
	{
		if ( !pEntry->IsAchieved() )
		{
			pEntry->OnAchievementAwarded();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: updates specified achievement
//-----------------------------------------------------------------------------
void CAchievementMgr::UpdateAchievement( int iAchievementID, int nData, int nUserSlot )
{
	CBaseAchievement *pAchievement = GetAchievementByID( iAchievementID, nUserSlot );
	contract_assert( pAchievement );
	if ( !pAchievement )
		return;

	if ( !CheckAchievementsEnabled() )
	{
		Msg( "Achievements disabled, ignoring achievement update for %s\n", pAchievement->GetName() );
		return;
	}

	if ( pAchievement->IsAchieved() )
	{
		if ( cc_achievement_debug.GetInt() > 0 )
		{
			Msg( "Achievement update called but already achieved: %s\n", pAchievement->GetName() );
		}
		return;
	}

	pAchievement->UpdateAchievement( nData );
}

//-----------------------------------------------------------------------------
// Purpose: clears state for all achievements
//-----------------------------------------------------------------------------
void CAchievementMgr::PreRestoreSavedGame()
{
	for ( int j = 0; j < MAX_SPLITSCREEN_PLAYERS; ++j )
	{
		for ( const auto &[nID, pEntry] : m_mapAchievement[j] )
		{
			pEntry->PreRestoreSavedGame();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: clears state for all achievements
//-----------------------------------------------------------------------------
void CAchievementMgr::PostRestoreSavedGame()
{
	for ( int j = 0; j < MAX_SPLITSCREEN_PLAYERS; ++j )
	{
		for ( const auto &[nID, pEntry] : m_mapAchievement[j] )
		{
			pEntry->PostRestoreSavedGame();
		}
	}
}

ConVar	cc_achievement_disable(side_registry(), "achievement_disable", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "Turn off achievements." );

//-----------------------------------------------------------------------------
// Purpose: checks if achievements are enabled
//-----------------------------------------------------------------------------
bool CAchievementMgr::CheckAchievementsEnabled( )
{
	if ( cc_achievement_disable.GetBool() )
		return false;

	//No achievements in demo version.

	// achievements disabled if playing demo (Playback demo) or watching HLTV
	if ( engine->IsHLTV() )
	{
		Msg( "Achievements disabled: demo playing.\n" );
		return false;
	}

	// Don't award achievements if cheats are turned on.  
	if ( WereCheatsEverOn() )
	{
		// Cheats get turned on automatically if you run with -dev which many people do internally, so allow cheats if developer is turned on
		if ( developer.GetInt() == 0 )
		{
			Msg( "Achievements disabled: cheats turned on in this app session.\n" );
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the # of teammates of the local player
//-----------------------------------------------------------------------------
int	CalcTeammateCount()
{
	contract_assert( g_pGameRules->IsMultiplayer() );

	// determine local player team
	int iLocalPlayerIndex =  GetLocalPlayerIndex();
	int iLocalPlayerTeam = g_PR->GetTeam( iLocalPlayerIndex );

	int iNumTeammates = 0;
	for( int iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
	{
		// find all players who are on the local player's team
		if( ( iPlayerIndex != iLocalPlayerIndex ) && ( g_PR->IsConnected( iPlayerIndex ) ) && ( g_PR->GetTeam( iPlayerIndex ) == iLocalPlayerTeam ) )
		{
			iNumTeammates++;
		}
	}
	return iNumTeammates;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the # of teammates of the local player
//-----------------------------------------------------------------------------
int	CalcPlayerCount()
{
	int iCount = 0;
	for( int iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
	{
		// find all players who are on the local player's team
		if( g_PR->IsConnected( iPlayerIndex ) )
		{
			iCount++;
		}
	}
	return iCount;
}

////-----------------------------------------------------------------------------
// Purpose: Resets all achievements. Behaves like ResetAchievements but does not automatically save profile
//-----------------------------------------------------------------------------
void CAchievementMgr::ClearAchievements( int nUserSlot )
{
	if ( nUserSlot < 0 || nUserSlot >= MAX_SPLITSCREEN_PLAYERS )
	{
		contract_assert( false && "CAchievementMgr::ClearAchievements invalid nUserSlot\n" );
		return;
	}

	for ( const auto &[nID, pEntry] : m_mapAchievement[nUserSlot] )
	{
		CBaseAchievement *pAchievement = pEntry.get();
		ResetAchievement_Internal( pAchievement );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Resets all achievements.  For debugging purposes only
//-----------------------------------------------------------------------------
void CAchievementMgr::ResetAchievements()
{
	for ( const auto &[nID, pEntry] : m_mapAchievement[SINGLE_PLAYER_SLOT] )
	{
		CBaseAchievement *pAchievement = pEntry.get();
		ResetAchievement_Internal( pAchievement );
	}
	SaveGlobalState();
}

void CAchievementMgr::ResetAchievement( int iAchievementID )
{
	CBaseAchievement *pAchievement = GetAchievementByID( iAchievementID, SINGLE_PLAYER_SLOT );
	contract_assert( pAchievement );
	if ( pAchievement )
	{
		ResetAchievement_Internal( pAchievement );
		SaveGlobalState();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Resets all achievements.  For debugging purposes only
//-----------------------------------------------------------------------------
void CAchievementMgr::PrintAchievementStatus()
{
	Msg( "%42s %-30s %s\n", "Name:", "Status:", "Point value:" );
	int iTotalAchievements = 0, iTotalPoints = 0;
	for ( const auto &[nID, pEntry] : m_mapAchievement[SINGLE_PLAYER_SLOT] )
	{
		CBaseAchievement *pAchievement = pEntry.get();

		Msg( "%42s ", pAchievement->GetName() );	

		if ( pAchievement->IsAchieved() )
		{
			CCSBaseAchievement* pCSAchievement = dynamic_cast<CCSBaseAchievement*>(pAchievement);

			// Assign the award date text
			char dateBuffer[32] = "";
			int year, month, day, hour, minute, second;
			if ( pCSAchievement && pCSAchievement->GetAwardTime(year, month, day, hour, minute, second) )
			{
				V_snprintf( dateBuffer, sizeof(dateBuffer), "ACHIEVED %4d-%02d-%02d %2d:%02d:%02d", year, month, day, hour, minute, second );
				Msg( "%-30s", dateBuffer );
			}
			else
			{
				Msg( "%-30s", "ACHIEVED" );
			}
		}
		else 
		{
			char szBuf[255];
			char szComponents[67] = "";
			int componentsLen = std::size( szComponents );
			
			if ( pAchievement->HasComponents() )
			{
				uint64 bits = pAchievement->GetComponentBits();
				int numComponents = pAchievement->GetNumComponents();

				V_strcat( szComponents, "(", componentsLen );

				for ( int i=numComponents-1; i>=0; i-- )
				{
					V_strcat( szComponents, ( bits & ((uint64)1<<i) ) ? "1" : "0", componentsLen );
				}				

				V_strcat( szComponents, ")", componentsLen );
			}

			V_snprintf( szBuf, std::size( szBuf ), "(%d/%d)%s %s",
				pAchievement->GetCount(), pAchievement->GetGoal(),
				pAchievement->IsActive() ? "" : " (inactive)",
				szComponents );

			Msg( "%-30s", szBuf );
		}
		Msg( " %d   ", pAchievement->GetPointValue() );
		pAchievement->PrintAdditionalStatus();
		Msg( "\n" );
		iTotalAchievements++;
		iTotalPoints += pAchievement->GetPointValue();
	}
	Msg( "Total achievements: %d  Total possible points: %d\n", iTotalAchievements, iTotalPoints );
}

//-----------------------------------------------------------------------------
// Purpose: called when a game event is fired
//-----------------------------------------------------------------------------
void CAchievementMgr::FireGameEvent( IGameEvent *event )
{
	int nSplitScreenPlayer = event->GetInt( "splitscreenplayer" );
	contract_assert( nSplitScreenPlayer < MAX_SPLITSCREEN_PLAYERS );

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSplitScreenPlayer );

	CVProfScope _( "CAchievementMgr::FireGameEvent", 1, "Achievements", false, 0 );
	const char *name = event->GetName();
	if ( 0 == V_strcmp( name, "entity_killed" ) )
	{
	}
	else if ( 0 == V_strcmp( name, "game_init" ) )
	{
	}
	else if ( 0 == V_strcmp( name, "player_death" ) )
	{
		CBaseEntity *pVictim = ClientEntityList().GetEnt( engine->GetPlayerForUserID( event->GetInt("userid") ) );
		CBaseEntity *pAttacker = ClientEntityList().GetEnt( engine->GetPlayerForUserID( event->GetInt("attacker") ) );
		OnKillEvent( pVictim, pAttacker, nullptr, event );
	}
	else if ( 0 == V_strcmp( name, "localplayer_changeclass" ) )
	{
		// keep track of when the player last changed class
		m_flLastClassChangeTime[nSplitScreenPlayer] =  gpGlobals->curtime;
	}
	else if ( 0 == V_strcmp( name, "localplayer_changeteam" ) )
	{
		// keep track of the time of transitions to and from a game team
		CBasePlayer *pLocalPlayer = CBasePlayer::GetLocalPlayer();
		if ( pLocalPlayer )
		{
			int iTeam = pLocalPlayer->GetTeamNumber();
			if ( iTeam > TEAM_SPECTATOR )
			{
				if ( 0 == m_flTeamplayStartTime[nSplitScreenPlayer] )
				{
					// player transitioned from no/spectator team to a game team, mark the time
					m_flTeamplayStartTime[nSplitScreenPlayer] = gpGlobals->curtime;
				}				
			}
			else
			{
				// player transitioned to no/spectator team, clear the teamplay start time
				m_flTeamplayStartTime[nSplitScreenPlayer] = 0;
			}			
		}		
	}
	else if ( 0 == V_strcmp( name, "teamplay_round_start" ) )
	{
		if ( event->GetBool( "full_reset" ) )
		{
			// we're starting a full round, clear miniround count
			m_iMiniroundsCompleted[nSplitScreenPlayer] = 0;
		}
	}
	else if ( 0 == V_strcmp( name, "teamplay_round_win" ) )
	{
		if ( false == event->GetBool( "full_round", true ) )
		{
			// we just finished a miniround but the round is continuing, increment miniround count
			m_iMiniroundsCompleted[nSplitScreenPlayer]++;
		}
	}
	else if ( 0 == V_strcmp( name, "player_stats_updated" ) )
	{
		for ( const auto &[nID, pEntry] : m_mapAchievement[nSplitScreenPlayer] )
		{
			CBaseAchievement *pAchievement = pEntry.get();
			pAchievement->OnPlayerStatsUpdate( nSplitScreenPlayer );
		}
	}
	else if ( 0 == V_strcmp( name, "read_game_titledata" ) )
	{
		SyncAchievementsToTitleData( event->GetInt( "controllerId" ), ACHIEVEMENT_READ_ACHIEVEMENT );
	}
	else if ( 0 == V_strcmp( name, "write_game_titledata" ) )
	{
		SyncAchievementsToTitleData( event->GetInt( "controllerId" ), ACHIEVEMENT_WRITE_ACHIEVEMENT );
	}
	else if ( 0 == V_strcmp( name, "reset_game_titledata" ) )
	{
		ClearAchievements( XBX_GetSlotByUserId( event->GetInt( "controllerId" ) ) );
	}
	else if ( 0 == V_strcmp( name, "write_profile_data" ) )
	{
		for ( int i=0; i<XUSER_MAX_COUNT; ++i )
		{
			char cmdLine[80];
			V_snprintf( cmdLine, 80, "host_writeconfig_ss %d", i );
			engine->ClientCmd_Unrestricted( cmdLine );
		}
	}
	else if ( 0 == V_strcmp( name, "achievement_write_failed" ) )
	{
	}

}

//-----------------------------------------------------------------------------
// Purpose: called when a player or character has been killed
//-----------------------------------------------------------------------------
void CAchievementMgr::OnKillEvent( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event )
{
	int nSplitScreenPlayer = SINGLE_PLAYER_SLOT;
	nSplitScreenPlayer = event->GetInt( "splitscreenplayer" );
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSplitScreenPlayer );

	// can have a NULL victim on client if victim has never entered local player's PVS
	if ( !pVictim )
		return;

	// if single-player game, calculate if the attacker is the local player and if the victim is the player enemy
	bool bAttackerIsPlayer = false;
	bool bVictimIsPlayerEnemy = false;
	CBasePlayer *pLocalPlayer = CBasePlayer::GetLocalPlayer();
	bVictimIsPlayerEnemy = !pLocalPlayer->InSameTeam( pVictim );
	if ( pAttacker == pLocalPlayer )
	{
		bAttackerIsPlayer = true;
	}

	for ( int j = 0; j < MAX_SPLITSCREEN_PLAYERS; ++j )
	{
		// look through all the kill event listeners and notify any achievements whose filters we pass
		for ( CBaseAchievement *pAchievement : m_vecKillEventListeners[j] )
		{
			if ( !pAchievement->IsActive() )
				continue;

			// if this achievement only looks for kills where attacker is player and that is not the case here, skip this achievement
			if ( ( pAchievement->GetFlags() & ACH_FILTER_ATTACKER_IS_PLAYER ) && !bAttackerIsPlayer )
				continue;

			// if this achievement only looks for kills where victim is killer enemy and that is not the case here, skip this achievement
			if ( ( pAchievement->GetFlags() & ACH_FILTER_VICTIM_IS_PLAYER_ENEMY ) && !bVictimIsPlayerEnemy )
				continue;

			// we pass all filters for this achievement, notify the achievement of the kill
			pAchievement->Event_EntityKilled( pVictim, pAttacker, pInflictor, event );
		}
	}
}

void CAchievementMgr::OnAchievementEvent( int iAchievementID, int nUserSlot )
{
	// handle event for specific achievement
	CBaseAchievement *pAchievement = GetAchievementByID( iAchievementID, nUserSlot );
	contract_assert( pAchievement );
	if ( pAchievement )
	{
		if ( !pAchievement->IsAchieved() )
		{
			pAchievement->IncrementCount();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: called when a map-fired achievement event occurs
//-----------------------------------------------------------------------------
void CAchievementMgr::OnMapEvent( const char *pchEventName, int nUserSlot )
{
	contract_assert( pchEventName && *pchEventName );
	if ( !pchEventName || !*pchEventName ) 
		return;

	// see if this event matches the prefix for an achievement component
	for ( CBaseAchievement *pAchievement : m_vecComponentListeners[nUserSlot] )
	{
		contract_assert( pAchievement->m_pszComponentPrefix );
		if ( 0 == V_strncmp( pchEventName, pAchievement->m_pszComponentPrefix, pAchievement->m_iComponentPrefixLen ) )
		{
			// prefix matches, tell the achievement a component was found
			pAchievement->OnComponentEvent( pchEventName );
			return;
		}
	}

	// look through all the map event listeners
	for ( CBaseAchievement *pAchievement : m_vecMapEventListeners[nUserSlot] )
	{
		pAchievement->OnMapEvent( pchEventName );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns an achievement as it's abstract object. This interface is used by gameui.dll for getting achievement info.
// Input  : index - 
// Output : IBaseAchievement*
//-----------------------------------------------------------------------------
IAchievement* CAchievementMgr::GetAchievementByIndex( int index, int nUserSlot )
{
	contract_assert( ks::is_valid_index( m_vecAchievement[nUserSlot], index ) );
	return (IAchievement*)m_vecAchievement[nUserSlot][index];
}

//-----------------------------------------------------------------------------
// Purpose: Returns an achievement as it's abstract object. This interface is used by gameui.dll for getting achievement info.
// Input  : orderIndex - 
// Output : IBaseAchievement*
//-----------------------------------------------------------------------------
IAchievement* CAchievementMgr::GetAchievementByDisplayOrder( int orderIndex, int nUserSlot )
{
	contract_assert( ks::is_valid_index( m_vecAchievementInOrder[nUserSlot], orderIndex ) );
	return (IAchievement*)m_vecAchievementInOrder[nUserSlot][orderIndex];
}

//-----------------------------------------------------------------------------
// Purpose: Returns an asset award achievement as it's abstract object.
// Input  : orderIndex - 
// Output : IBaseAchievement*
//-----------------------------------------------------------------------------
IAchievement* CAchievementMgr::GetAwardByDisplayOrder( int orderIndex, int nUserSlot )
{
	contract_assert( ks::is_valid_index( m_vecAwardInOrder[nUserSlot], orderIndex ) );
	return (IAchievement*)m_vecAwardInOrder[nUserSlot][orderIndex];
}

//-----------------------------------------------------------------------------
// Purpose: Returns total achievement count. This interface is used by gameui.dll for getting achievement info.
// Input  :  - 
// Output : Count of achievements in manager's vector.
//-----------------------------------------------------------------------------
int CAchievementMgr::GetAchievementCount( bool bAssets )
{
	int achCount = 0;
	for ( const auto &[nID, pEntry] : m_mapAchievement[SINGLE_PLAYER_SLOT] )
	{
		CBaseAchievement *pAchievement = pEntry.get();
		if ( (bAssets && pAchievement->IsAssetAward()) || (!bAssets && !pAchievement->IsAssetAward()) )
		{
			achCount++;
		}
	}
	return achCount;
}

//-----------------------------------------------------------------------------
// Purpose: Handles events from the matchmaking framework.
//-----------------------------------------------------------------------------
void CAchievementMgr::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( FStrEq( szEvent, "OnProfileDataLoaded" ) )
	{
		// This event is sent when the title data blocks have been loaded.
		int iController = pEvent->GetInt( "iController" );
		int nSlot = SINGLE_PLAYER_SLOT;
		ReadAchievementsFromTitleData( iController, nSlot );
	}
	else if ( FStrEq( szEvent, "sv_cheats_changed" ) )
	{	// we got a local event that sv_cheats value changed!
		if ( pEvent->GetInt( "value" ) )
			m_bCheatsEverOn = true;
	}
}

bool CAchievementMgr::SyncAchievementsToTitleData( int iController, SyncAchievementValueDirection_t eOp )
{
	return true;
}

void CAchievementMgr::ResetAchievement_Internal( CBaseAchievement *pAchievement )
{
	contract_assert( pAchievement );

	pAchievement->SetAchieved( false );
	pAchievement->SetCount( 0 );	
	if ( pAchievement->HasComponents() )
	{
		pAchievement->SetComponentBits( 0 );
	}
	pAchievement->SetProgressShown( 0 );
	pAchievement->StopListeningForAllEvents();
	if ( pAchievement->IsActive() )
	{
		pAchievement->ListenForEvents();
	}
}

bool MsgFunc_AchievementEvent( const ks::net::CCSUsrMsg_AchievementEvent &msg )
{
	int iAchievementID = (int) msg.achievement;
	CAchievementMgr *pAchievementMgr = CAchievementMgr::GetInstance();
	if ( !pAchievementMgr )
		return true;

	int iCount = (int) msg.count;
	int userID = (int) msg.user_id;

	// Fix unused variable warning
	(void)iCount;

	int userSlot = SINGLE_PLAYER_SLOT;

	(void)userID;

	pAchievementMgr->OnAchievementEvent( iAchievementID, userSlot );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: helper function to get entity model name
//-----------------------------------------------------------------------------
const char *GetModelName( CBaseEntity *pBaseEntity )
{
	CBaseAnimating *pBaseAnimating = dynamic_cast<CBaseAnimating *>( pBaseEntity );
	if ( pBaseAnimating )
	{
		CStudioHdr *pStudioHdr = pBaseAnimating->GetModelPtr();
		if ( pStudioHdr )
		{
			return pStudioHdr->pszName();
		}
	}

	return "";
}

//-----------------------------------------------------------------------------
// Purpose: Gets the list of achievements achieved during the current game
//-----------------------------------------------------------------------------
const std::vector< int >& CAchievementMgr::GetAchievedDuringCurrentGame( int nPlayerSlot )
{
	return m_AchievementsAwardedDuringCurrentGame[nPlayerSlot];
}

//-----------------------------------------------------------------------------
// Purpose: Reset the list of achievements achieved during the current game
//-----------------------------------------------------------------------------
void CAchievementMgr::ResetAchievedDuringCurrentGame( int nPlayerSlot )
{
	m_AchievementsAwardedDuringCurrentGame[nPlayerSlot].clear();
}

//-----------------------------------------------------------------------------
// Purpose: Clears achievement data for the a particular user slot
//-----------------------------------------------------------------------------
void CAchievementMgr::ClearAchievementData( int nUserSlot )
{
	contract_assert( nUserSlot < MAX_SPLITSCREEN_PLAYERS );

	for ( const auto &[nID, pEntry] : m_mapAchievement[nUserSlot] )
	{
		pEntry->ClearAchievementData();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Do per-frame handling
//-----------------------------------------------------------------------------
void CAchievementMgr::Update( float frametime )
{
	if ( !sv_cheats )
	{
		sv_cheats = cvar->FindVar( "sv_cheats" );
	}

	// keep track if cheats have ever been turned on during this level
	if ( !WereCheatsEverOn() )
	{
		if ( sv_cheats && sv_cheats->GetBool() )
		{
			m_bCheatsEverOn = true;
		}
	}

	// [sbodenbender] brought over from orange box
	// Call think functions. Work backwards, because we may remove achievements from the list.
	int iCount = std::ssize( m_vecThinkListeners );
	for ( int i = iCount-1; i >= 0; i-- )
	{
		if ( m_vecThinkListeners[i].m_flThinkTime < gpGlobals->curtime )
		{
			m_vecThinkListeners[i].pAchievement->Think();

			// The think function may have pushed out the think time. If not, remove ourselves from the list.
			if ( m_vecThinkListeners[i].pAchievement->IsAchieved() || m_vecThinkListeners[i].m_flThinkTime < gpGlobals->curtime )
			{
				m_vecThinkListeners.erase( m_vecThinkListeners.begin() + i );
			}
		}
	}

}

bool CAchievementMgr::IsCurrentMap( const char *pszMapName )
{
	if ( !pszMapName )
		return false;

	return StringHasPrefixCaseSensitive( GetMapName(), pszMapName );
}



extern IFileSystem *filesystem;

// NOTE: If CStringRegistry allowed storing arbitrary data, we could just use that.
// in this case we have the "isPrivate" member and the replacement rules 
// (activityIndex can be reused by private activities), so a custom table is necessary
struct activitylist_t
{
	int					activityIndex;
	CStringRegistry::Key	stringKey;
	short				isPrivate;
};

std::vector< activitylist_t > g_ActivityList;

static ks::idict< CActivityRemapCache > m_ActivityRemapDatabase;

// This stores the actual activity names.  Also, the string ID in the registry is simply an index 
// into the g_ActivityList array.
CStringRegistry	g_ActivityStrings;

// this is just here to accelerate adds
static int g_HighestActivity = 0;

int g_nActivityListVersion = 1;

void ActivityList_Init( void )
{
	g_HighestActivity = 0;
}

void ActivityList_Free( void )
{
	g_ActivityStrings.ClearStrings();
	g_ActivityList.clear(); g_ActivityList.shrink_to_fit();
	m_ActivityRemapDatabase.clear();

	// So studiohdrs can reindex activity indices
	++g_nActivityListVersion;
}

// add a new activity to the database
activitylist_t *ActivityList_AddActivityEntry( const char *pName, int iActivityIndex, bool isPrivate )
{
	int index = std::ssize( g_ActivityList ); g_ActivityList.emplace_back();
	activitylist_t *pList = &g_ActivityList[index];
	pList->activityIndex = iActivityIndex;
	pList->stringKey = g_ActivityStrings.AddString( pName, index );
	pList->isPrivate = isPrivate;
	
	// UNDONE: This implies that ALL shared activities are added before ANY custom activities
	// UNDONE: Segment these instead?  It's a 32-bit int, how many activities do we need?
	if ( iActivityIndex > g_HighestActivity )
	{
		g_HighestActivity = iActivityIndex;
	}

	return pList;
}

// get the database entry from a string
static activitylist_t *ListFromString( const char *pString )
{
	// just use the string registry to do this search/map
	int stringID = g_ActivityStrings.GetStringID( pString );
	if ( stringID < 0 )
		return nullptr;

	return &g_ActivityList[stringID];
}

// Get the database entry for an index
static activitylist_t *ListFromActivity( int activityIndex )
{
	// ugly linear search
	for ( int i = 0; i < std::ssize( g_ActivityList ); i++ )
	{
		if ( g_ActivityList[i].activityIndex == activityIndex )
		{
			return &g_ActivityList[i];
		}
	}

	return nullptr;
}

bool ActivityList_RegisterSharedActivity( const char *pszActivityName, int iActivityIndex )
{
	// UNDONE: Do we want to do these checks when not in developer mode? or maybe DEBUG only?
	// They really only matter when you change the list of code controlled activities.  IDs
	// for content controlled activities never collide because they are generated.

	// technically order isn't dependent, but it's too damn easy to forget to add new ACT_'s to all three lists.
	static int lastActivityIndex = -1;
	contract_assert( iActivityIndex < LAST_SHARED_ACTIVITY && (iActivityIndex == lastActivityIndex + 1 || iActivityIndex == 0) );
	lastActivityIndex = iActivityIndex;

	// first, check to make sure the slot we're asking for is free. It must be for 
	// a shared activity.
	activitylist_t *pList = ListFromString( pszActivityName );
	if ( !pList )
	{
		pList = ListFromActivity( iActivityIndex );
	}

	if ( pList )
	{
		Warning( "***\nShared activity collision! %s<->%s\n***\n", pszActivityName, g_ActivityStrings.GetStringForKey( pList->stringKey ) );
		contract_assert( 0 );
		return false;
	}
	// ----------------------------------------------------------------

	ActivityList_AddActivityEntry( pszActivityName, iActivityIndex, false );
	return true;
}

Activity ActivityList_RegisterPrivateActivity( const char *pszActivityName )
{
	activitylist_t *pList = ListFromString( pszActivityName );
	if ( pList )
	{
		// this activity is already in the list. If the activity we collided with is also private, 
		// then the collision is OK. Otherwise, it's a bug.
		if ( pList->isPrivate )
		{
			return (Activity)pList->activityIndex;
		}
		else
		{
			// this private activity collides with a shared activity. That is not allowed.
			Warning( "***\nShared<->Private Activity collision!\n***\n" );
			contract_assert( 0 );
			return ACT_INVALID;
		}
	}

	pList = ActivityList_AddActivityEntry( pszActivityName, g_HighestActivity+1, true );
	return (Activity)pList->activityIndex;
}

// Get the index for a given activity name
// Done at load time for all models
int ActivityList_IndexForName( const char *pszActivityName )
{
	// this is a fast O(lgn) search (actually does 2 O(lgn) searches)
	activitylist_t *pList = ListFromString( pszActivityName );

	if ( pList )
	{
		return pList->activityIndex;
	}

	return -1;
}

// Get the name for a given index
// This should only be used in debug code, it does a linear search
// But at least it only compares integers
const char *ActivityList_NameForIndex( int activityIndex )
{
	activitylist_t *pList = ListFromActivity( activityIndex );
	if ( pList )
	{
		return g_ActivityStrings.GetStringForKey( pList->stringKey );
	}
	return nullptr;
}

void ActivityList_RegisterSharedActivities( void )
{
	REGISTER_SHARED_ACTIVITY( ACT_RESET );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE );
	REGISTER_SHARED_ACTIVITY( ACT_TRANSITION );
	REGISTER_SHARED_ACTIVITY( ACT_COVER );
	REGISTER_SHARED_ACTIVITY( ACT_COVER_MED );
	REGISTER_SHARED_ACTIVITY( ACT_COVER_LOW );
	REGISTER_SHARED_ACTIVITY( ACT_WALK );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_AIM );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_CROUCH );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_CROUCH_AIM );
	REGISTER_SHARED_ACTIVITY( ACT_RUN );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_AIM );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_CROUCH );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_CROUCH_AIM );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_PROTECTED );
	REGISTER_SHARED_ACTIVITY( ACT_SCRIPT_CUSTOM_MOVE );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK1 );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK2 );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK1_LOW );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK2_LOW );
	REGISTER_SHARED_ACTIVITY( ACT_DIESIMPLE );
	REGISTER_SHARED_ACTIVITY( ACT_DIEBACKWARD );
	REGISTER_SHARED_ACTIVITY( ACT_DIEFORWARD );
	REGISTER_SHARED_ACTIVITY( ACT_DIEVIOLENT );
	REGISTER_SHARED_ACTIVITY( ACT_DIERAGDOLL );
	REGISTER_SHARED_ACTIVITY( ACT_FLY );
	REGISTER_SHARED_ACTIVITY( ACT_HOVER );
	REGISTER_SHARED_ACTIVITY( ACT_GLIDE );
	REGISTER_SHARED_ACTIVITY( ACT_SWIM );
	REGISTER_SHARED_ACTIVITY( ACT_JUMP );
	REGISTER_SHARED_ACTIVITY( ACT_HOP );
	REGISTER_SHARED_ACTIVITY( ACT_LEAP );
	REGISTER_SHARED_ACTIVITY( ACT_LAND );
	REGISTER_SHARED_ACTIVITY( ACT_CLIMB_UP );
	REGISTER_SHARED_ACTIVITY( ACT_CLIMB_DOWN );
	REGISTER_SHARED_ACTIVITY( ACT_CLIMB_DISMOUNT );
	REGISTER_SHARED_ACTIVITY( ACT_SHIPLADDER_UP );
	REGISTER_SHARED_ACTIVITY( ACT_SHIPLADDER_DOWN );
	REGISTER_SHARED_ACTIVITY( ACT_STRAFE_LEFT );
	REGISTER_SHARED_ACTIVITY( ACT_STRAFE_RIGHT );
	REGISTER_SHARED_ACTIVITY( ACT_ROLL_LEFT );
	REGISTER_SHARED_ACTIVITY( ACT_ROLL_RIGHT );
	REGISTER_SHARED_ACTIVITY( ACT_TURN_LEFT );
	REGISTER_SHARED_ACTIVITY( ACT_TURN_RIGHT );
	REGISTER_SHARED_ACTIVITY( ACT_CROUCH );
	REGISTER_SHARED_ACTIVITY( ACT_CROUCHIDLE );
	REGISTER_SHARED_ACTIVITY( ACT_STAND );
	REGISTER_SHARED_ACTIVITY( ACT_USE );
	REGISTER_SHARED_ACTIVITY( ACT_ALIEN_BURROW_IDLE );
	REGISTER_SHARED_ACTIVITY( ACT_ALIEN_BURROW_OUT );
	REGISTER_SHARED_ACTIVITY( ACT_SIGNAL1 );
	REGISTER_SHARED_ACTIVITY( ACT_SIGNAL2 );
	REGISTER_SHARED_ACTIVITY( ACT_SIGNAL3 );

	REGISTER_SHARED_ACTIVITY( ACT_SIGNAL_ADVANCE );
	REGISTER_SHARED_ACTIVITY( ACT_SIGNAL_FORWARD );
	REGISTER_SHARED_ACTIVITY( ACT_SIGNAL_GROUP );
	REGISTER_SHARED_ACTIVITY( ACT_SIGNAL_HALT );
	REGISTER_SHARED_ACTIVITY( ACT_SIGNAL_LEFT );
	REGISTER_SHARED_ACTIVITY( ACT_SIGNAL_RIGHT );
	REGISTER_SHARED_ACTIVITY( ACT_SIGNAL_TAKECOVER );

	REGISTER_SHARED_ACTIVITY( ACT_LOOKBACK_RIGHT );
	REGISTER_SHARED_ACTIVITY( ACT_LOOKBACK_LEFT );
	REGISTER_SHARED_ACTIVITY( ACT_COWER );
	REGISTER_SHARED_ACTIVITY( ACT_SMALL_FLINCH );
	REGISTER_SHARED_ACTIVITY( ACT_BIG_FLINCH );
	REGISTER_SHARED_ACTIVITY( ACT_MELEE_ATTACK1 );
	REGISTER_SHARED_ACTIVITY( ACT_MELEE_ATTACK2 );
	REGISTER_SHARED_ACTIVITY( ACT_RELOAD );
	REGISTER_SHARED_ACTIVITY( ACT_RELOAD_START );
	REGISTER_SHARED_ACTIVITY( ACT_RELOAD_FINISH );
	REGISTER_SHARED_ACTIVITY( ACT_RELOAD_LOW );
	REGISTER_SHARED_ACTIVITY( ACT_ARM );
	REGISTER_SHARED_ACTIVITY( ACT_DISARM );
	
	REGISTER_SHARED_ACTIVITY( ACT_DROP_WEAPON );
	REGISTER_SHARED_ACTIVITY( ACT_DROP_WEAPON_SHOTGUN );

	REGISTER_SHARED_ACTIVITY( ACT_PICKUP_GROUND );
	REGISTER_SHARED_ACTIVITY( ACT_PICKUP_RACK );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_ANGRY );

	REGISTER_SHARED_ACTIVITY( ACT_IDLE_RELAXED );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_STIMULATED );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_AGITATED );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_STEALTH );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_HURT );

	REGISTER_SHARED_ACTIVITY( ACT_WALK_RELAXED );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_STIMULATED );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_AGITATED );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_STEALTH );

	REGISTER_SHARED_ACTIVITY( ACT_RUN_RELAXED );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_STIMULATED );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_AGITATED );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_STEALTH );

	REGISTER_SHARED_ACTIVITY( ACT_IDLE_AIM_RELAXED );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_AIM_STIMULATED );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_AIM_AGITATED );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_AIM_STEALTH );

	REGISTER_SHARED_ACTIVITY( ACT_WALK_AIM_RELAXED );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_AIM_STIMULATED );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_AIM_AGITATED );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_AIM_STEALTH );

	REGISTER_SHARED_ACTIVITY( ACT_RUN_AIM_RELAXED );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_AIM_STIMULATED );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_AIM_AGITATED );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_AIM_STEALTH );

	REGISTER_SHARED_ACTIVITY( ACT_CROUCHIDLE_STIMULATED );
	REGISTER_SHARED_ACTIVITY( ACT_CROUCHIDLE_AIM_STIMULATED );
	REGISTER_SHARED_ACTIVITY( ACT_CROUCHIDLE_AGITATED );

	REGISTER_SHARED_ACTIVITY( ACT_WALK_HURT );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_HURT );
	REGISTER_SHARED_ACTIVITY( ACT_SPECIAL_ATTACK1 );
	REGISTER_SHARED_ACTIVITY( ACT_SPECIAL_ATTACK2 );
	REGISTER_SHARED_ACTIVITY( ACT_COMBAT_IDLE );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_SCARED );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_SCARED );
	REGISTER_SHARED_ACTIVITY( ACT_VICTORY_DANCE );
	
	REGISTER_SHARED_ACTIVITY( ACT_DIE_HEADSHOT );
	REGISTER_SHARED_ACTIVITY( ACT_DIE_CHESTSHOT );
	REGISTER_SHARED_ACTIVITY( ACT_DIE_GUTSHOT );
	REGISTER_SHARED_ACTIVITY( ACT_DIE_BACKSHOT );

	REGISTER_SHARED_ACTIVITY( ACT_FLINCH_HEAD );
	REGISTER_SHARED_ACTIVITY( ACT_FLINCH_CHEST );
	REGISTER_SHARED_ACTIVITY( ACT_FLINCH_STOMACH );
	REGISTER_SHARED_ACTIVITY( ACT_FLINCH_LEFTARM );
	REGISTER_SHARED_ACTIVITY( ACT_FLINCH_RIGHTARM );
	REGISTER_SHARED_ACTIVITY( ACT_FLINCH_LEFTLEG );
	REGISTER_SHARED_ACTIVITY( ACT_FLINCH_RIGHTLEG );
	REGISTER_SHARED_ACTIVITY( ACT_FLINCH_PHYSICS );

	REGISTER_SHARED_ACTIVITY( ACT_FLINCH_HEAD_BACK );
	REGISTER_SHARED_ACTIVITY( ACT_FLINCH_HEAD_LEFT );
	REGISTER_SHARED_ACTIVITY( ACT_FLINCH_HEAD_RIGHT );
	REGISTER_SHARED_ACTIVITY( ACT_FLINCH_CHEST_BACK );
	REGISTER_SHARED_ACTIVITY( ACT_FLINCH_STOMACH_BACK );
	REGISTER_SHARED_ACTIVITY( ACT_FLINCH_CROUCH_FRONT );
	REGISTER_SHARED_ACTIVITY( ACT_FLINCH_CROUCH_BACK );
	REGISTER_SHARED_ACTIVITY( ACT_FLINCH_CROUCH_LEFT );
	REGISTER_SHARED_ACTIVITY( ACT_FLINCH_CROUCH_RIGHT );

	REGISTER_SHARED_ACTIVITY( ACT_IDLE_ON_FIRE );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_ON_FIRE );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_ON_FIRE );

	REGISTER_SHARED_ACTIVITY( ACT_RAPPEL_LOOP );

	REGISTER_SHARED_ACTIVITY( ACT_180_LEFT );
	REGISTER_SHARED_ACTIVITY( ACT_180_RIGHT );
	REGISTER_SHARED_ACTIVITY( ACT_90_LEFT );
	REGISTER_SHARED_ACTIVITY( ACT_90_RIGHT );
	REGISTER_SHARED_ACTIVITY( ACT_STEP_LEFT );			
	REGISTER_SHARED_ACTIVITY( ACT_STEP_RIGHT );
	REGISTER_SHARED_ACTIVITY( ACT_STEP_BACK );
	REGISTER_SHARED_ACTIVITY( ACT_STEP_FORE );

	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RANGE_ATTACK1 );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RANGE_ATTACK2 );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_MELEE_ATTACK1 );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_MELEE_ATTACK2 );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RANGE_ATTACK1_LOW );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RANGE_ATTACK2_LOW );
	REGISTER_SHARED_ACTIVITY( ACT_MELEE_ATTACK_SWING_GESTURE );
	
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_SMALL_FLINCH );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_BIG_FLINCH );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_FLINCH_BLAST );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_FLINCH_BLAST_SHOTGUN );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_FLINCH_BLAST_DAMAGED );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_FLINCH_BLAST_DAMAGED_SHOTGUN );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_FLINCH_HEAD );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_FLINCH_CHEST );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_FLINCH_STOMACH );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_FLINCH_LEFTARM );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_FLINCH_RIGHTARM );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_FLINCH_LEFTLEG );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_FLINCH_RIGHTLEG );

	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_TURN_LEFT );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_TURN_RIGHT );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_TURN_LEFT45 );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_TURN_RIGHT45 );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_TURN_LEFT90 );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_TURN_RIGHT90 );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_TURN_LEFT45_FLAT );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_TURN_RIGHT45_FLAT );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_TURN_LEFT90_FLAT );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_TURN_RIGHT90_FLAT );

	// registered for Half-Life HGRUNT compatability
	REGISTER_SHARED_ACTIVITY( ACT_BARNACLE_HIT );
	REGISTER_SHARED_ACTIVITY( ACT_BARNACLE_PULL );
	REGISTER_SHARED_ACTIVITY( ACT_BARNACLE_CHOMP );
	REGISTER_SHARED_ACTIVITY( ACT_BARNACLE_CHEW );

	REGISTER_SHARED_ACTIVITY( ACT_DO_NOT_DISTURB );
	REGISTER_SHARED_ACTIVITY( ACT_SPECIFIC_SEQUENCE );

	// Viewmodel activities may belong elsewhere, but since where is unclear right now,
	// they'll be placed here.
	REGISTER_SHARED_ACTIVITY( ACT_VM_DRAW );
	REGISTER_SHARED_ACTIVITY( ACT_VM_HOLSTER );
	REGISTER_SHARED_ACTIVITY( ACT_VM_IDLE );
	REGISTER_SHARED_ACTIVITY( ACT_VM_FIDGET );
	REGISTER_SHARED_ACTIVITY( ACT_VM_PULLBACK );
	REGISTER_SHARED_ACTIVITY( ACT_VM_PULLBACK_HIGH );
	REGISTER_SHARED_ACTIVITY( ACT_VM_PULLBACK_LOW );
	REGISTER_SHARED_ACTIVITY( ACT_VM_THROW );
	REGISTER_SHARED_ACTIVITY( ACT_VM_PULLPIN );
	REGISTER_SHARED_ACTIVITY( ACT_VM_PRIMARYATTACK );
	REGISTER_SHARED_ACTIVITY( ACT_VM_SECONDARYATTACK );
	REGISTER_SHARED_ACTIVITY( ACT_VM_RELOAD );
	REGISTER_SHARED_ACTIVITY( ACT_VM_DRYFIRE );
	REGISTER_SHARED_ACTIVITY( ACT_VM_HITLEFT );
	REGISTER_SHARED_ACTIVITY( ACT_VM_HITLEFT2 );
	REGISTER_SHARED_ACTIVITY( ACT_VM_HITRIGHT );
	REGISTER_SHARED_ACTIVITY( ACT_VM_HITRIGHT2 );
	REGISTER_SHARED_ACTIVITY( ACT_VM_HITCENTER );
	REGISTER_SHARED_ACTIVITY( ACT_VM_HITCENTER2 );
	REGISTER_SHARED_ACTIVITY( ACT_VM_MISSLEFT );
	REGISTER_SHARED_ACTIVITY( ACT_VM_MISSLEFT2 );
	REGISTER_SHARED_ACTIVITY( ACT_VM_MISSRIGHT );
	REGISTER_SHARED_ACTIVITY( ACT_VM_MISSRIGHT2 );
	REGISTER_SHARED_ACTIVITY( ACT_VM_MISSCENTER );
	REGISTER_SHARED_ACTIVITY( ACT_VM_MISSCENTER2 );
	REGISTER_SHARED_ACTIVITY( ACT_VM_HAULBACK );
	REGISTER_SHARED_ACTIVITY( ACT_VM_SWINGHARD );
	REGISTER_SHARED_ACTIVITY( ACT_VM_SWINGMISS );
	REGISTER_SHARED_ACTIVITY( ACT_VM_SWINGHIT );
	REGISTER_SHARED_ACTIVITY( ACT_VM_IDLE_TO_LOWERED );
	REGISTER_SHARED_ACTIVITY( ACT_VM_IDLE_LOWERED );
	REGISTER_SHARED_ACTIVITY( ACT_VM_LOWERED_TO_IDLE );
	REGISTER_SHARED_ACTIVITY( ACT_VM_RECOIL1 );
	REGISTER_SHARED_ACTIVITY( ACT_VM_RECOIL2 );
	REGISTER_SHARED_ACTIVITY( ACT_VM_RECOIL3 );
	REGISTER_SHARED_ACTIVITY( ACT_VM_PICKUP );
	REGISTER_SHARED_ACTIVITY( ACT_VM_RELEASE );

	REGISTER_SHARED_ACTIVITY ( ACT_VM_ATTACH_SILENCER );
	REGISTER_SHARED_ACTIVITY ( ACT_VM_DETACH_SILENCER );

	REGISTER_SHARED_ACTIVITY ( ACT_VM_EMPTY_FIRE );
	REGISTER_SHARED_ACTIVITY ( ACT_VM_EMPTY_RELOAD );
	REGISTER_SHARED_ACTIVITY ( ACT_VM_EMPTY_DRAW );
	REGISTER_SHARED_ACTIVITY ( ACT_VM_EMPTY_IDLE );

//===========================
// HL2 Specific Activities
//===========================
	// SLAM Specialty Activites
	REGISTER_SHARED_ACTIVITY( ACT_SLAM_STICKWALL_IDLE);
	REGISTER_SHARED_ACTIVITY( ACT_SLAM_STICKWALL_ND_IDLE);
	REGISTER_SHARED_ACTIVITY( ACT_SLAM_STICKWALL_ATTACH);
	REGISTER_SHARED_ACTIVITY( ACT_SLAM_STICKWALL_ATTACH2);
	REGISTER_SHARED_ACTIVITY( ACT_SLAM_STICKWALL_ND_ATTACH);
	REGISTER_SHARED_ACTIVITY( ACT_SLAM_STICKWALL_ND_ATTACH2);
	REGISTER_SHARED_ACTIVITY( ACT_SLAM_STICKWALL_DETONATE);
	REGISTER_SHARED_ACTIVITY( ACT_SLAM_STICKWALL_DETONATOR_HOLSTER);
	REGISTER_SHARED_ACTIVITY( ACT_SLAM_STICKWALL_DRAW); 
  	REGISTER_SHARED_ACTIVITY( ACT_SLAM_STICKWALL_ND_DRAW); 
  	REGISTER_SHARED_ACTIVITY( ACT_SLAM_STICKWALL_TO_THROW); 
  	REGISTER_SHARED_ACTIVITY( ACT_SLAM_STICKWALL_TO_THROW_ND); 
	REGISTER_SHARED_ACTIVITY( ACT_SLAM_STICKWALL_TO_TRIPMINE_ND );
  	REGISTER_SHARED_ACTIVITY( ACT_SLAM_THROW_IDLE);
	REGISTER_SHARED_ACTIVITY( ACT_SLAM_THROW_ND_IDLE);
	REGISTER_SHARED_ACTIVITY( ACT_SLAM_THROW_THROW);
	REGISTER_SHARED_ACTIVITY( ACT_SLAM_THROW_THROW2);
	REGISTER_SHARED_ACTIVITY( ACT_SLAM_THROW_THROW_ND);
	REGISTER_SHARED_ACTIVITY( ACT_SLAM_THROW_THROW_ND2);
	REGISTER_SHARED_ACTIVITY( ACT_SLAM_THROW_DRAW); 
  	REGISTER_SHARED_ACTIVITY( ACT_SLAM_THROW_ND_DRAW); 
  	REGISTER_SHARED_ACTIVITY( ACT_SLAM_THROW_TO_STICKWALL); 
  	REGISTER_SHARED_ACTIVITY( ACT_SLAM_THROW_TO_STICKWALL_ND); 
	REGISTER_SHARED_ACTIVITY( ACT_SLAM_THROW_DETONATE);
	REGISTER_SHARED_ACTIVITY( ACT_SLAM_THROW_DETONATOR_HOLSTER);
	REGISTER_SHARED_ACTIVITY( ACT_SLAM_THROW_TO_TRIPMINE_ND );
  	REGISTER_SHARED_ACTIVITY( ACT_SLAM_TRIPMINE_IDLE);
  	REGISTER_SHARED_ACTIVITY( ACT_SLAM_TRIPMINE_DRAW);
  	REGISTER_SHARED_ACTIVITY( ACT_SLAM_TRIPMINE_ATTACH); 
  	REGISTER_SHARED_ACTIVITY( ACT_SLAM_TRIPMINE_ATTACH2); 
	REGISTER_SHARED_ACTIVITY( ACT_SLAM_TRIPMINE_TO_STICKWALL_ND );
	REGISTER_SHARED_ACTIVITY( ACT_SLAM_TRIPMINE_TO_THROW_ND );
  	REGISTER_SHARED_ACTIVITY( ACT_SLAM_DETONATOR_IDLE); 
  	REGISTER_SHARED_ACTIVITY( ACT_SLAM_DETONATOR_DRAW); 
  	REGISTER_SHARED_ACTIVITY( ACT_SLAM_DETONATOR_DETONATE);
  	REGISTER_SHARED_ACTIVITY( ACT_SLAM_DETONATOR_HOLSTER);
  	REGISTER_SHARED_ACTIVITY( ACT_SLAM_DETONATOR_STICKWALL_DRAW); 
  	REGISTER_SHARED_ACTIVITY( ACT_SLAM_DETONATOR_THROW_DRAW); 

	// SHOTGUN specialty activities
	REGISTER_SHARED_ACTIVITY( ACT_SHOTGUN_RELOAD_START);
	REGISTER_SHARED_ACTIVITY( ACT_SHOTGUN_RELOAD_FINISH);
	REGISTER_SHARED_ACTIVITY( ACT_SHOTGUN_PUMP);

	// SMG2 special activities
	REGISTER_SHARED_ACTIVITY( ACT_SMG2_IDLE2 );
	REGISTER_SHARED_ACTIVITY( ACT_SMG2_FIRE2 );
	REGISTER_SHARED_ACTIVITY( ACT_SMG2_DRAW2 );
	REGISTER_SHARED_ACTIVITY( ACT_SMG2_RELOAD2 );
	REGISTER_SHARED_ACTIVITY( ACT_SMG2_DRYFIRE2 );
	REGISTER_SHARED_ACTIVITY( ACT_SMG2_TOAUTO );
	REGISTER_SHARED_ACTIVITY( ACT_SMG2_TOBURST );

	// Physcannon special activities
	REGISTER_SHARED_ACTIVITY( ACT_PHYSCANNON_UPGRADE );

	// weapon override activities
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK_AR1 );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK_AR2 );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK_AR2_LOW );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK_AR2_GRENADE );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK_HMG1 );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK_ML );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK_SMG1 );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK_SMG1_LOW );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK_SMG2 );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK_SHOTGUN );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK_SHOTGUN_LOW );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK_PISTOL );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK_PISTOL_LOW );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK_SLAM );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK_TRIPWIRE );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK_THROW );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK_SNIPER_RIFLE );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK_RPG );
	REGISTER_SHARED_ACTIVITY( ACT_MELEE_ATTACK_SWING );

	REGISTER_SHARED_ACTIVITY( ACT_RANGE_AIM_LOW );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_AIM_SMG1_LOW );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_AIM_PISTOL_LOW );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_AIM_AR2_LOW );

	REGISTER_SHARED_ACTIVITY( ACT_COVER_PISTOL_LOW );
	REGISTER_SHARED_ACTIVITY( ACT_COVER_SMG1_LOW );

	// weapon override activities
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RANGE_ATTACK_AR1 );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RANGE_ATTACK_AR2 );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RANGE_ATTACK_AR2_GRENADE );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RANGE_ATTACK_HMG1 );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RANGE_ATTACK_ML );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RANGE_ATTACK_SMG1 );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RANGE_ATTACK_SMG1_LOW );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RANGE_ATTACK_SMG2 );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RANGE_ATTACK_SHOTGUN );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RANGE_ATTACK_PISTOL );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RANGE_ATTACK_PISTOL_LOW );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RANGE_ATTACK_SLAM );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RANGE_ATTACK_TRIPWIRE );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RANGE_ATTACK_THROW );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RANGE_ATTACK_SNIPER_RIFLE );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_MELEE_ATTACK_SWING );

	REGISTER_SHARED_ACTIVITY( ACT_IDLE_RIFLE );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_SMG1 );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_ANGRY_SMG1 );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_PISTOL );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_ANGRY_PISTOL );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_ANGRY_SHOTGUN );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_STEALTH_PISTOL );

	REGISTER_SHARED_ACTIVITY( ACT_IDLE_PACKAGE );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_PACKAGE );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_SUITCASE );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_SUITCASE );

	REGISTER_SHARED_ACTIVITY( ACT_IDLE_SMG1_RELAXED );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_SMG1_STIMULATED );

	REGISTER_SHARED_ACTIVITY( ACT_WALK_RIFLE_RELAXED );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_RIFLE_RELAXED );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_RIFLE_STIMULATED );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_RIFLE_STIMULATED );

	REGISTER_SHARED_ACTIVITY( ACT_IDLE_AIM_RIFLE_STIMULATED );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_AIM_RIFLE_STIMULATED );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_AIM_RIFLE_STIMULATED );

	REGISTER_SHARED_ACTIVITY( ACT_IDLE_SHOTGUN_RELAXED );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_SHOTGUN_STIMULATED );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_SHOTGUN_AGITATED );

	// Policing activities
	REGISTER_SHARED_ACTIVITY( ACT_WALK_ANGRY );
	REGISTER_SHARED_ACTIVITY( ACT_POLICE_HARASS1 );
	REGISTER_SHARED_ACTIVITY( ACT_POLICE_HARASS2 );

	// Manned guns
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_MANNEDGUN );

	// Melee weapon activities
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_MELEE );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_ANGRY_MELEE );

	// RPG activities
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_RPG_RELAXED );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_RPG ); 
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_ANGRY_RPG );
	REGISTER_SHARED_ACTIVITY( ACT_COVER_LOW_RPG ); 
	REGISTER_SHARED_ACTIVITY( ACT_WALK_RPG );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_RPG ); 
	REGISTER_SHARED_ACTIVITY( ACT_WALK_CROUCH_RPG ); 
	REGISTER_SHARED_ACTIVITY( ACT_RUN_CROUCH_RPG ); 
	REGISTER_SHARED_ACTIVITY( ACT_WALK_RPG_RELAXED ); 
	REGISTER_SHARED_ACTIVITY( ACT_RUN_RPG_RELAXED );

	REGISTER_SHARED_ACTIVITY( ACT_WALK_RIFLE );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_AIM_RIFLE );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_CROUCH_RIFLE );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_CROUCH_AIM_RIFLE );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_RIFLE );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_AIM_RIFLE );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_CROUCH_RIFLE );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_CROUCH_AIM_RIFLE );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_STEALTH_PISTOL );

	REGISTER_SHARED_ACTIVITY( ACT_WALK_AIM_SHOTGUN );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_AIM_SHOTGUN );

	REGISTER_SHARED_ACTIVITY( ACT_WALK_PISTOL );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_PISTOL );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_AIM_PISTOL );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_AIM_PISTOL );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_STEALTH_PISTOL );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_AIM_STEALTH_PISTOL );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_AIM_STEALTH_PISTOL );

	// Reloads
	REGISTER_SHARED_ACTIVITY( ACT_RELOAD_PISTOL );
	REGISTER_SHARED_ACTIVITY( ACT_RELOAD_PISTOL_LOW );
	REGISTER_SHARED_ACTIVITY( ACT_RELOAD_SMG1 );
	REGISTER_SHARED_ACTIVITY( ACT_RELOAD_SMG1_LOW );
	REGISTER_SHARED_ACTIVITY( ACT_RELOAD_SHOTGUN );
	REGISTER_SHARED_ACTIVITY( ACT_RELOAD_SHOTGUN_LOW );

	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RELOAD );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RELOAD_PISTOL );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RELOAD_SMG1 );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_RELOAD_SHOTGUN );

	// Busy animations
	REGISTER_SHARED_ACTIVITY( ACT_BUSY_LEAN_LEFT );
	REGISTER_SHARED_ACTIVITY( ACT_BUSY_LEAN_LEFT_ENTRY );
	REGISTER_SHARED_ACTIVITY( ACT_BUSY_LEAN_LEFT_EXIT );
	REGISTER_SHARED_ACTIVITY( ACT_BUSY_LEAN_BACK );
	REGISTER_SHARED_ACTIVITY( ACT_BUSY_LEAN_BACK_ENTRY );
	REGISTER_SHARED_ACTIVITY( ACT_BUSY_LEAN_BACK_EXIT );
	REGISTER_SHARED_ACTIVITY( ACT_BUSY_SIT_GROUND );
	REGISTER_SHARED_ACTIVITY( ACT_BUSY_SIT_GROUND_ENTRY );
	REGISTER_SHARED_ACTIVITY( ACT_BUSY_SIT_GROUND_EXIT );
	REGISTER_SHARED_ACTIVITY( ACT_BUSY_SIT_CHAIR );
	REGISTER_SHARED_ACTIVITY( ACT_BUSY_SIT_CHAIR_ENTRY );
	REGISTER_SHARED_ACTIVITY( ACT_BUSY_SIT_CHAIR_EXIT );
	REGISTER_SHARED_ACTIVITY( ACT_BUSY_STAND );
	REGISTER_SHARED_ACTIVITY( ACT_BUSY_QUEUE );
	
	// Dodge animations
	REGISTER_SHARED_ACTIVITY( ACT_DUCK_DODGE );

	REGISTER_SHARED_ACTIVITY( ACT_DIE_BARNACLE_SWALLOW );
	REGISTER_SHARED_ACTIVITY( ACT_GESTURE_BARNACLE_STRANGLE );
	REGISTER_SHARED_ACTIVITY( ACT_PHYSCANNON_DETACH );
	REGISTER_SHARED_ACTIVITY( ACT_PHYSCANNON_ANIMATE );
	REGISTER_SHARED_ACTIVITY( ACT_PHYSCANNON_ANIMATE_PRE );
	REGISTER_SHARED_ACTIVITY( ACT_PHYSCANNON_ANIMATE_POST );

	REGISTER_SHARED_ACTIVITY( ACT_DIE_FRONTSIDE );
	REGISTER_SHARED_ACTIVITY( ACT_DIE_RIGHTSIDE );
	REGISTER_SHARED_ACTIVITY( ACT_DIE_BACKSIDE );
	REGISTER_SHARED_ACTIVITY( ACT_DIE_LEFTSIDE );

	REGISTER_SHARED_ACTIVITY( ACT_DIE_CROUCH_FRONTSIDE );
	REGISTER_SHARED_ACTIVITY( ACT_DIE_CROUCH_RIGHTSIDE );
	REGISTER_SHARED_ACTIVITY( ACT_DIE_CROUCH_BACKSIDE );
	REGISTER_SHARED_ACTIVITY( ACT_DIE_CROUCH_LEFTSIDE );

	REGISTER_SHARED_ACTIVITY( ACT_OPEN_DOOR );

	// Dynamic interactions
	REGISTER_SHARED_ACTIVITY( ACT_DI_ALYX_ZOMBIE_MELEE );
	REGISTER_SHARED_ACTIVITY( ACT_DI_ALYX_ZOMBIE_TORSO_MELEE );
	REGISTER_SHARED_ACTIVITY( ACT_DI_ALYX_HEADCRAB_MELEE );
	REGISTER_SHARED_ACTIVITY( ACT_DI_ALYX_ANTLION );
	REGISTER_SHARED_ACTIVITY( ACT_DI_ALYX_ZOMBIE_SHOTGUN64 );
	REGISTER_SHARED_ACTIVITY( ACT_DI_ALYX_ZOMBIE_SHOTGUN26 );

	//Readiness transitions
	REGISTER_SHARED_ACTIVITY( ACT_READINESS_RELAXED_TO_STIMULATED );
	REGISTER_SHARED_ACTIVITY( ACT_READINESS_RELAXED_TO_STIMULATED_WALK );
	REGISTER_SHARED_ACTIVITY( ACT_READINESS_AGITATED_TO_STIMULATED );
	REGISTER_SHARED_ACTIVITY( ACT_READINESS_STIMULATED_TO_RELAXED );

	REGISTER_SHARED_ACTIVITY( ACT_READINESS_PISTOL_RELAXED_TO_STIMULATED );
	REGISTER_SHARED_ACTIVITY( ACT_READINESS_PISTOL_RELAXED_TO_STIMULATED_WALK );
	REGISTER_SHARED_ACTIVITY( ACT_READINESS_PISTOL_AGITATED_TO_STIMULATED );
	REGISTER_SHARED_ACTIVITY( ACT_READINESS_PISTOL_STIMULATED_TO_RELAXED );

	REGISTER_SHARED_ACTIVITY( ACT_IDLE_CARRY );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_CARRY );

//===========================
// TF2 Specific Activities
//===========================
	REGISTER_SHARED_ACTIVITY( ACT_STARTDYING );
	REGISTER_SHARED_ACTIVITY( ACT_DYINGLOOP );
	REGISTER_SHARED_ACTIVITY( ACT_DYINGTODEAD );
	REGISTER_SHARED_ACTIVITY( ACT_RIDE_MANNED_GUN );

	// All viewmodels
	REGISTER_SHARED_ACTIVITY( ACT_VM_SPRINT_ENTER );
	REGISTER_SHARED_ACTIVITY( ACT_VM_SPRINT_IDLE );
	REGISTER_SHARED_ACTIVITY( ACT_VM_SPRINT_LEAVE );

	// Looping weapon firing
	REGISTER_SHARED_ACTIVITY( ACT_FIRE_START );
	REGISTER_SHARED_ACTIVITY( ACT_FIRE_LOOP );
	REGISTER_SHARED_ACTIVITY( ACT_FIRE_END );

	// TF2
	REGISTER_SHARED_ACTIVITY( ACT_CROUCHING_GRENADEIDLE );
	REGISTER_SHARED_ACTIVITY( ACT_CROUCHING_GRENADEREADY );
	REGISTER_SHARED_ACTIVITY( ACT_CROUCHING_PRIMARYATTACK );
	REGISTER_SHARED_ACTIVITY( ACT_OVERLAY_GRENADEIDLE );
	REGISTER_SHARED_ACTIVITY( ACT_OVERLAY_GRENADEREADY );
	REGISTER_SHARED_ACTIVITY( ACT_OVERLAY_PRIMARYATTACK );
	REGISTER_SHARED_ACTIVITY( ACT_OVERLAY_SHIELD_UP );
	REGISTER_SHARED_ACTIVITY( ACT_OVERLAY_SHIELD_DOWN );
	REGISTER_SHARED_ACTIVITY( ACT_OVERLAY_SHIELD_UP_IDLE );
	REGISTER_SHARED_ACTIVITY( ACT_OVERLAY_SHIELD_ATTACK );
	REGISTER_SHARED_ACTIVITY( ACT_OVERLAY_SHIELD_KNOCKBACK );
	REGISTER_SHARED_ACTIVITY( ACT_SHIELD_UP );
	REGISTER_SHARED_ACTIVITY( ACT_SHIELD_DOWN );
	REGISTER_SHARED_ACTIVITY( ACT_SHIELD_UP_IDLE );
	REGISTER_SHARED_ACTIVITY( ACT_SHIELD_ATTACK );
	REGISTER_SHARED_ACTIVITY( ACT_SHIELD_KNOCKBACK );
	REGISTER_SHARED_ACTIVITY( ACT_CROUCHING_SHIELD_UP );
	REGISTER_SHARED_ACTIVITY( ACT_CROUCHING_SHIELD_DOWN );
	REGISTER_SHARED_ACTIVITY( ACT_CROUCHING_SHIELD_UP_IDLE );
	REGISTER_SHARED_ACTIVITY( ACT_CROUCHING_SHIELD_ATTACK );
	REGISTER_SHARED_ACTIVITY( ACT_CROUCHING_SHIELD_KNOCKBACK );

	REGISTER_SHARED_ACTIVITY( ACT_TURNRIGHT45 );
	REGISTER_SHARED_ACTIVITY( ACT_TURNLEFT45 );
	REGISTER_SHARED_ACTIVITY( ACT_TURN );

	REGISTER_SHARED_ACTIVITY( ACT_OBJ_ASSEMBLING );
	REGISTER_SHARED_ACTIVITY( ACT_OBJ_DISMANTLING );
	REGISTER_SHARED_ACTIVITY( ACT_OBJ_STARTUP );
	REGISTER_SHARED_ACTIVITY( ACT_OBJ_RUNNING );
	REGISTER_SHARED_ACTIVITY( ACT_OBJ_IDLE );
	REGISTER_SHARED_ACTIVITY( ACT_OBJ_PLACING );
	REGISTER_SHARED_ACTIVITY( ACT_OBJ_DETERIORATING );
	REGISTER_SHARED_ACTIVITY( ACT_OBJ_UPGRADING );

	// Deploy
	REGISTER_SHARED_ACTIVITY( ACT_DEPLOY );
	REGISTER_SHARED_ACTIVITY( ACT_DEPLOY_IDLE );
	REGISTER_SHARED_ACTIVITY( ACT_UNDEPLOY );

	// Crossbow
	REGISTER_SHARED_ACTIVITY( ACT_CROSSBOW_DRAW_UNLOADED );

	// Gauss
	REGISTER_SHARED_ACTIVITY( ACT_GAUSS_SPINUP );
	REGISTER_SHARED_ACTIVITY( ACT_GAUSS_SPINCYCLE );

//===========================
// CSPort Specific Activities
//===========================

	REGISTER_SHARED_ACTIVITY ( ACT_VM_PRIMARYATTACK_SILENCED );		// fire
	REGISTER_SHARED_ACTIVITY ( ACT_VM_RELOAD_SILENCED );
	REGISTER_SHARED_ACTIVITY ( ACT_VM_DRYFIRE_SILENCED );				// fire with no ammo loaded.
	REGISTER_SHARED_ACTIVITY ( ACT_VM_IDLE_SILENCED );
	REGISTER_SHARED_ACTIVITY ( ACT_VM_DRAW_SILENCED );

	REGISTER_SHARED_ACTIVITY ( ACT_VM_IDLE_EMPTY_LEFT );
	REGISTER_SHARED_ACTIVITY ( ACT_VM_DRYFIRE_LEFT );
	
	// new for CS2
	REGISTER_SHARED_ACTIVITY ( ACT_VM_IS_DRAW );
	REGISTER_SHARED_ACTIVITY ( ACT_VM_IS_HOLSTER );
	REGISTER_SHARED_ACTIVITY ( ACT_VM_IS_IDLE );
	REGISTER_SHARED_ACTIVITY ( ACT_VM_IS_PRIMARYATTACK );

	REGISTER_SHARED_ACTIVITY ( ACT_PLAYER_IDLE_FIRE );
	REGISTER_SHARED_ACTIVITY ( ACT_PLAYER_CROUCH_FIRE );
	REGISTER_SHARED_ACTIVITY ( ACT_PLAYER_CROUCH_WALK_FIRE );
	REGISTER_SHARED_ACTIVITY ( ACT_PLAYER_WALK_FIRE );
	REGISTER_SHARED_ACTIVITY ( ACT_PLAYER_RUN_FIRE );
	REGISTER_SHARED_ACTIVITY ( ACT_IDLETORUN );
	REGISTER_SHARED_ACTIVITY ( ACT_RUNTOIDLE );

	REGISTER_SHARED_ACTIVITY ( ACT_VM_DRAW_DEPLOYED );

	REGISTER_SHARED_ACTIVITY ( ACT_HL2MP_IDLE_MELEE );
	REGISTER_SHARED_ACTIVITY ( ACT_HL2MP_RUN_MELEE );
	REGISTER_SHARED_ACTIVITY ( ACT_HL2MP_IDLE_CROUCH_MELEE );
	REGISTER_SHARED_ACTIVITY ( ACT_HL2MP_WALK_CROUCH_MELEE );
	REGISTER_SHARED_ACTIVITY ( ACT_HL2MP_GESTURE_RANGE_ATTACK_MELEE );
	REGISTER_SHARED_ACTIVITY ( ACT_HL2MP_GESTURE_RELOAD_MELEE );
	REGISTER_SHARED_ACTIVITY ( ACT_HL2MP_JUMP_MELEE );

	// Portal
	REGISTER_SHARED_ACTIVITY ( ACT_VM_FIZZLE );
	
	// Multiplayer
	REGISTER_SHARED_ACTIVITY ( ACT_MP_STAND_IDLE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_CROUCH_IDLE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_CROUCH_DEPLOYED_IDLE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_CROUCH_DEPLOYED );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_DEPLOYED_IDLE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RUN );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_WALK );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_AIRWALK );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_CROUCHWALK );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_SPRINT );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_START );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_FLOAT );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_LAND );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_IMPACT_N );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_IMPACT_E );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_IMPACT_W );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_IMPACT_S );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_IMPACT_TOP );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_DOUBLEJUMP );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_SWIM );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_DEPLOYED );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_SWIM_DEPLOYED );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_VCD );

	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_STAND_PRIMARYFIRE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_STAND_PRIMARYFIRE_DEPLOYED );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_STAND_SECONDARYFIRE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_STAND_GRENADE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_CROUCH_PRIMARYFIRE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_CROUCH_PRIMARYFIRE_DEPLOYED );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_CROUCH_SECONDARYFIRE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_CROUCH_GRENADE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_SWIM_PRIMARYFIRE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_SWIM_SECONDARYFIRE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_SWIM_GRENADE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_AIRWALK_PRIMARYFIRE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_AIRWALK_SECONDARYFIRE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_AIRWALK_GRENADE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_STAND );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_STAND_LOOP );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_STAND_END );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_CROUCH );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_CROUCH_LOOP );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_CROUCH_END );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_SWIM );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_SWIM_LOOP );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_SWIM_END );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_AIRWALK );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_AIRWALK_LOOP );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_AIRWALK_END );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_STAND_PREFIRE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_STAND_POSTFIRE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_STAND_STARTFIRE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_CROUCH_PREFIRE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_CROUCH_POSTFIRE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_SWIM_PREFIRE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_SWIM_POSTFIRE );

	// Multiplayer - Primary
	REGISTER_SHARED_ACTIVITY ( ACT_MP_STAND_PRIMARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_CROUCH_PRIMARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RUN_PRIMARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_WALK_PRIMARY );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_AIRWALK_PRIMARY );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_CROUCHWALK_PRIMARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_PRIMARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_START_PRIMARY );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_FLOAT_PRIMARY );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_LAND_PRIMARY );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_SWIM_PRIMARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_DEPLOYED_PRIMARY );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_SWIM_DEPLOYED_PRIMARY );	

	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_STAND_PRIMARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_STAND_PRIMARY_DEPLOYED );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_CROUCH_PRIMARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_CROUCH_PRIMARY_DEPLOYED );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_SWIM_PRIMARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_AIRWALK_PRIMARY );	

	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_STAND_PRIMARY );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_STAND_PRIMARY_LOOP );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_STAND_PRIMARY_END );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_CROUCH_PRIMARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_CROUCH_PRIMARY_LOOP );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_CROUCH_PRIMARY_END );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_SWIM_PRIMARY );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_SWIM_PRIMARY_LOOP );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_SWIM_PRIMARY_END );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_AIRWALK_PRIMARY );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_AIRWALK_PRIMARY_LOOP );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_AIRWALK_PRIMARY_END );

	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_STAND_GRENADE_PRIMARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_CROUCH_GRENADE_PRIMARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_SWIM_GRENADE_PRIMARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_AIRWALK_GRENADE_PRIMARY );	

	// Secondary
	REGISTER_SHARED_ACTIVITY ( ACT_MP_STAND_SECONDARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_CROUCH_SECONDARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RUN_SECONDARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_WALK_SECONDARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_AIRWALK_SECONDARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_CROUCHWALK_SECONDARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_SECONDARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_START_SECONDARY );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_FLOAT_SECONDARY );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_LAND_SECONDARY );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_SWIM_SECONDARY );	

	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_STAND_SECONDARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_CROUCH_SECONDARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_SWIM_SECONDARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_AIRWALK_SECONDARY );	

	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_STAND_SECONDARY );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_STAND_SECONDARY_LOOP );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_STAND_SECONDARY_END );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_CROUCH_SECONDARY );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_CROUCH_SECONDARY_LOOP );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_CROUCH_SECONDARY_END );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_SWIM_SECONDARY );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_SWIM_SECONDARY_LOOP );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_SWIM_SECONDARY_END );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_AIRWALK_SECONDARY );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_AIRWALK_SECONDARY_LOOP );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RELOAD_AIRWALK_SECONDARY_END );

	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_STAND_GRENADE_SECONDARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_CROUCH_GRENADE_SECONDARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_SWIM_GRENADE_SECONDARY );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_AIRWALK_GRENADE_SECONDARY );	

	// Melee
	REGISTER_SHARED_ACTIVITY ( ACT_MP_STAND_MELEE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_CROUCH_MELEE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RUN_MELEE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_WALK_MELEE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_AIRWALK_MELEE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_CROUCHWALK_MELEE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_MELEE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_START_MELEE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_FLOAT_MELEE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_LAND_MELEE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_SWIM_MELEE );	

	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_STAND_MELEE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_STAND_MELEE_SECONDARY );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_CROUCH_MELEE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_CROUCH_MELEE_SECONDARY );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_SWIM_MELEE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_AIRWALK_MELEE );	

	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_STAND_GRENADE_MELEE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_CROUCH_GRENADE_MELEE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_SWIM_GRENADE_MELEE );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_AIRWALK_GRENADE_MELEE );	

	// Item1
	REGISTER_SHARED_ACTIVITY( ACT_MP_STAND_ITEM1 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_CROUCH_ITEM1 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_RUN_ITEM1 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_WALK_ITEM1 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_AIRWALK_ITEM1 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_CROUCHWALK_ITEM1 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_JUMP_ITEM1 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_JUMP_START_ITEM1 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_JUMP_FLOAT_ITEM1 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_JUMP_LAND_ITEM1 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_SWIM_ITEM1 );

	REGISTER_SHARED_ACTIVITY( ACT_MP_ATTACK_STAND_ITEM1 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_ATTACK_STAND_ITEM1_SECONDARY );
	REGISTER_SHARED_ACTIVITY( ACT_MP_ATTACK_CROUCH_ITEM1 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_ATTACK_CROUCH_ITEM1_SECONDARY );
	REGISTER_SHARED_ACTIVITY( ACT_MP_ATTACK_SWIM_ITEM1 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_ATTACK_AIRWALK_ITEM1 );

	// Item2
	REGISTER_SHARED_ACTIVITY( ACT_MP_STAND_ITEM2 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_CROUCH_ITEM2 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_RUN_ITEM2 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_WALK_ITEM2 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_AIRWALK_ITEM2 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_CROUCHWALK_ITEM2 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_JUMP_ITEM2 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_JUMP_START_ITEM2 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_JUMP_FLOAT_ITEM2 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_JUMP_LAND_ITEM2 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_SWIM_ITEM2 );

	REGISTER_SHARED_ACTIVITY( ACT_MP_ATTACK_STAND_ITEM2 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_ATTACK_STAND_ITEM2_SECONDARY );
	REGISTER_SHARED_ACTIVITY( ACT_MP_ATTACK_CROUCH_ITEM2 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_ATTACK_CROUCH_ITEM2_SECONDARY );
	REGISTER_SHARED_ACTIVITY( ACT_MP_ATTACK_SWIM_ITEM2 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_ATTACK_AIRWALK_ITEM2 );

	// Flinches
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_FLINCH );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_FLINCH_PRIMARY );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_FLINCH_SECONDARY );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_FLINCH_MELEE );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_FLINCH_ITEM1 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_FLINCH_ITEM2 );

	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_FLINCH_HEAD );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_FLINCH_CHEST );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_FLINCH_STOMACH );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_FLINCH_LEFTARM );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_FLINCH_RIGHTARM );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_FLINCH_LEFTLEG );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_FLINCH_RIGHTLEG );

	// TF Specific
	REGISTER_SHARED_ACTIVITY ( ACT_MP_GRENADE1_DRAW );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_GRENADE1_IDLE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_GRENADE1_ATTACK );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_GRENADE2_DRAW );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_GRENADE2_IDLE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_GRENADE2_ATTACK );

	REGISTER_SHARED_ACTIVITY ( ACT_MP_PRIMARY_GRENADE1_DRAW );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_PRIMARY_GRENADE1_IDLE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_PRIMARY_GRENADE1_ATTACK );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_PRIMARY_GRENADE2_DRAW );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_PRIMARY_GRENADE2_IDLE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_PRIMARY_GRENADE2_ATTACK );

	REGISTER_SHARED_ACTIVITY ( ACT_MP_SECONDARY_GRENADE1_DRAW );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_SECONDARY_GRENADE1_IDLE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_SECONDARY_GRENADE1_ATTACK );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_SECONDARY_GRENADE2_DRAW );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_SECONDARY_GRENADE2_IDLE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_SECONDARY_GRENADE2_ATTACK );

	REGISTER_SHARED_ACTIVITY ( ACT_MP_MELEE_GRENADE1_DRAW );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_MELEE_GRENADE1_IDLE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_MELEE_GRENADE1_ATTACK );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_MELEE_GRENADE2_DRAW );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_MELEE_GRENADE2_IDLE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_MELEE_GRENADE2_ATTACK );

	REGISTER_SHARED_ACTIVITY ( ACT_MP_ITEM1_GRENADE1_DRAW );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ITEM1_GRENADE1_IDLE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ITEM1_GRENADE1_ATTACK );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ITEM1_GRENADE2_DRAW );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ITEM1_GRENADE2_IDLE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ITEM1_GRENADE2_ATTACK );

	REGISTER_SHARED_ACTIVITY ( ACT_MP_ITEM2_GRENADE1_DRAW );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ITEM2_GRENADE1_IDLE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ITEM2_GRENADE1_ATTACK );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ITEM2_GRENADE2_DRAW );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ITEM2_GRENADE2_IDLE );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ITEM2_GRENADE2_ATTACK );

	// Building
	REGISTER_SHARED_ACTIVITY ( ACT_MP_STAND_BUILDING );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_CROUCH_BUILDING );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RUN_BUILDING );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_WALK_BUILDING );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_AIRWALK_BUILDING );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_CROUCHWALK_BUILDING );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_BUILDING );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_START_BUILDING );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_FLOAT_BUILDING );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_LAND_BUILDING );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_SWIM_BUILDING );	

	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_STAND_BUILDING );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_CROUCH_BUILDING );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_SWIM_BUILDING );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_AIRWALK_BUILDING );	

	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_STAND_GRENADE_BUILDING );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_CROUCH_GRENADE_BUILDING );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_SWIM_GRENADE_BUILDING );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_AIRWALK_GRENADE_BUILDING );	

	// PDA
	REGISTER_SHARED_ACTIVITY ( ACT_MP_STAND_PDA );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_CROUCH_PDA );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_RUN_PDA );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_WALK_PDA );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_AIRWALK_PDA );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_CROUCHWALK_PDA );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_PDA );	
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_START_PDA );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_FLOAT_PDA );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_JUMP_LAND_PDA );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_SWIM_PDA );	

	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_STAND_PDA );
	REGISTER_SHARED_ACTIVITY ( ACT_MP_ATTACK_SWIM_PDA );

	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_HANDMOUTH );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_FINGERPOINT );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_FISTPUMP );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_THUMBSUP );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_NODYES );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_NODNO );

	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_HANDMOUTH_PRIMARY );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_FINGERPOINT_PRIMARY );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_FISTPUMP_PRIMARY );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_THUMBSUP_PRIMARY );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_NODYES_PRIMARY );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_NODNO_PRIMARY );

	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_HANDMOUTH_SECONDARY );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_FINGERPOINT_SECONDARY );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_FISTPUMP_SECONDARY );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_THUMBSUP_SECONDARY );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_NODYES_SECONDARY );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_NODNO_SECONDARY );

	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_HANDMOUTH_MELEE );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_FINGERPOINT_MELEE );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_FISTPUMP_MELEE );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_THUMBSUP_MELEE );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_NODYES_MELEE );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_NODNO_MELEE );

	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_HANDMOUTH_ITEM1 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_FINGERPOINT_ITEM1 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_FISTPUMP_ITEM1 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_THUMBSUP_ITEM1 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_NODYES_ITEM1 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_NODNO_ITEM1 );

	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_HANDMOUTH_ITEM2 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_FINGERPOINT_ITEM2 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_FISTPUMP_ITEM2 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_THUMBSUP_ITEM2 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_NODYES_ITEM2 );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_NODNO_ITEM2 );

	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_HANDMOUTH_BUILDING );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_FINGERPOINT_BUILDING );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_FISTPUMP_BUILDING );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_THUMBSUP_BUILDING );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_NODYES_BUILDING );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_NODNO_BUILDING );

	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_HANDMOUTH_PDA );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_FINGERPOINT_PDA );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_FISTPUMP_PDA );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_THUMBSUP_PDA );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_NODYES_PDA );
	REGISTER_SHARED_ACTIVITY( ACT_MP_GESTURE_VC_NODNO_PDA );

	REGISTER_SHARED_ACTIVITY ( ACT_VM_UNUSABLE );
	REGISTER_SHARED_ACTIVITY ( ACT_VM_UNUSABLE_TO_USABLE );
	REGISTER_SHARED_ACTIVITY ( ACT_VM_USABLE_TO_UNUSABLE );

	REGISTER_SHARED_ACTIVITY( ACT_PRIMARY_VM_DRAW );
	REGISTER_SHARED_ACTIVITY( ACT_PRIMARY_VM_HOLSTER );
	REGISTER_SHARED_ACTIVITY( ACT_PRIMARY_VM_IDLE );
	REGISTER_SHARED_ACTIVITY( ACT_PRIMARY_VM_PULLBACK );
	REGISTER_SHARED_ACTIVITY( ACT_PRIMARY_VM_PRIMARYATTACK );
	REGISTER_SHARED_ACTIVITY( ACT_PRIMARY_VM_SECONDARYATTACK );
	REGISTER_SHARED_ACTIVITY( ACT_PRIMARY_VM_RELOAD );			
	REGISTER_SHARED_ACTIVITY( ACT_PRIMARY_VM_DRYFIRE );		
	REGISTER_SHARED_ACTIVITY( ACT_PRIMARY_VM_IDLE_TO_LOWERED );
	REGISTER_SHARED_ACTIVITY( ACT_PRIMARY_VM_IDLE_LOWERED );
	REGISTER_SHARED_ACTIVITY( ACT_PRIMARY_VM_LOWERED_TO_IDLE );

	REGISTER_SHARED_ACTIVITY( ACT_SECONDARY_VM_DRAW );
	REGISTER_SHARED_ACTIVITY( ACT_SECONDARY_VM_HOLSTER );
	REGISTER_SHARED_ACTIVITY( ACT_SECONDARY_VM_IDLE );
	REGISTER_SHARED_ACTIVITY( ACT_SECONDARY_VM_PULLBACK );
	REGISTER_SHARED_ACTIVITY( ACT_SECONDARY_VM_PRIMARYATTACK );
	REGISTER_SHARED_ACTIVITY( ACT_SECONDARY_VM_SECONDARYATTACK );
	REGISTER_SHARED_ACTIVITY( ACT_SECONDARY_VM_RELOAD );			
	REGISTER_SHARED_ACTIVITY( ACT_SECONDARY_VM_DRYFIRE );		
	REGISTER_SHARED_ACTIVITY( ACT_SECONDARY_VM_IDLE_TO_LOWERED );
	REGISTER_SHARED_ACTIVITY( ACT_SECONDARY_VM_IDLE_LOWERED );
	REGISTER_SHARED_ACTIVITY( ACT_SECONDARY_VM_LOWERED_TO_IDLE );

	REGISTER_SHARED_ACTIVITY( ACT_MELEE_VM_DRAW );
	REGISTER_SHARED_ACTIVITY( ACT_MELEE_VM_HOLSTER );
	REGISTER_SHARED_ACTIVITY( ACT_MELEE_VM_IDLE );
	REGISTER_SHARED_ACTIVITY( ACT_MELEE_VM_PULLBACK );
	REGISTER_SHARED_ACTIVITY( ACT_MELEE_VM_PRIMARYATTACK );
	REGISTER_SHARED_ACTIVITY( ACT_MELEE_VM_SECONDARYATTACK );
	REGISTER_SHARED_ACTIVITY( ACT_MELEE_VM_RELOAD );			
	REGISTER_SHARED_ACTIVITY( ACT_MELEE_VM_DRYFIRE );		
	REGISTER_SHARED_ACTIVITY( ACT_MELEE_VM_IDLE_TO_LOWERED );
	REGISTER_SHARED_ACTIVITY( ACT_MELEE_VM_IDLE_LOWERED );
	REGISTER_SHARED_ACTIVITY( ACT_MELEE_VM_LOWERED_TO_IDLE );

	REGISTER_SHARED_ACTIVITY( ACT_PDA_VM_DRAW );
	REGISTER_SHARED_ACTIVITY( ACT_PDA_VM_HOLSTER );
	REGISTER_SHARED_ACTIVITY( ACT_PDA_VM_IDLE );
	REGISTER_SHARED_ACTIVITY( ACT_PDA_VM_PULLBACK );
	REGISTER_SHARED_ACTIVITY( ACT_PDA_VM_PRIMARYATTACK );
	REGISTER_SHARED_ACTIVITY( ACT_PDA_VM_SECONDARYATTACK );
	REGISTER_SHARED_ACTIVITY( ACT_PDA_VM_RELOAD );			
	REGISTER_SHARED_ACTIVITY( ACT_PDA_VM_DRYFIRE );		
	REGISTER_SHARED_ACTIVITY( ACT_PDA_VM_IDLE_TO_LOWERED );
	REGISTER_SHARED_ACTIVITY( ACT_PDA_VM_IDLE_LOWERED );
	REGISTER_SHARED_ACTIVITY( ACT_PDA_VM_LOWERED_TO_IDLE );

	REGISTER_SHARED_ACTIVITY( ACT_ITEM1_VM_DRAW );
	REGISTER_SHARED_ACTIVITY( ACT_ITEM1_VM_HOLSTER );
	REGISTER_SHARED_ACTIVITY( ACT_ITEM1_VM_IDLE );
	REGISTER_SHARED_ACTIVITY( ACT_ITEM1_VM_PULLBACK );
	REGISTER_SHARED_ACTIVITY( ACT_ITEM1_VM_PRIMARYATTACK );
	REGISTER_SHARED_ACTIVITY( ACT_ITEM1_VM_SECONDARYATTACK );
	REGISTER_SHARED_ACTIVITY( ACT_ITEM1_VM_RELOAD );			
	REGISTER_SHARED_ACTIVITY( ACT_ITEM1_VM_DRYFIRE );		
	REGISTER_SHARED_ACTIVITY( ACT_ITEM1_VM_IDLE_TO_LOWERED );
	REGISTER_SHARED_ACTIVITY( ACT_ITEM1_VM_IDLE_LOWERED );
	REGISTER_SHARED_ACTIVITY( ACT_ITEM1_VM_LOWERED_TO_IDLE );

	REGISTER_SHARED_ACTIVITY( ACT_ITEM2_VM_DRAW );
	REGISTER_SHARED_ACTIVITY( ACT_ITEM2_VM_HOLSTER );
	REGISTER_SHARED_ACTIVITY( ACT_ITEM2_VM_IDLE );
	REGISTER_SHARED_ACTIVITY( ACT_ITEM2_VM_PULLBACK );
	REGISTER_SHARED_ACTIVITY( ACT_ITEM2_VM_PRIMARYATTACK );
	REGISTER_SHARED_ACTIVITY( ACT_ITEM2_VM_SECONDARYATTACK );
	REGISTER_SHARED_ACTIVITY( ACT_ITEM2_VM_RELOAD );			
	REGISTER_SHARED_ACTIVITY( ACT_ITEM2_VM_DRYFIRE );		
	REGISTER_SHARED_ACTIVITY( ACT_ITEM2_VM_IDLE_TO_LOWERED );
	REGISTER_SHARED_ACTIVITY( ACT_ITEM2_VM_IDLE_LOWERED );
	REGISTER_SHARED_ACTIVITY( ACT_ITEM2_VM_LOWERED_TO_IDLE );

	// infested
	REGISTER_SHARED_ACTIVITY( ACT_RELOAD_SUCCEED );
	REGISTER_SHARED_ACTIVITY( ACT_RELOAD_FAIL );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_AIM_AUTOGUN );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_AIM_AUTOGUN );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_AUTOGUN );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_AIM_AUTOGUN );
	REGISTER_SHARED_ACTIVITY( ACT_RELOAD_AUTOGUN );
	REGISTER_SHARED_ACTIVITY( ACT_CROUCH_IDLE_AUTOGUN );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK_AUTOGUN );
	REGISTER_SHARED_ACTIVITY( ACT_JUMP_AUTOGUN );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_AIM_PISTOL );
	REGISTER_SHARED_ACTIVITY( ACT_WALK_AIM_DUAL );
	REGISTER_SHARED_ACTIVITY( ACT_RUN_AIM_DUAL );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_DUAL );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_AIM_DUAL );
	REGISTER_SHARED_ACTIVITY( ACT_RELOAD_DUAL );
	REGISTER_SHARED_ACTIVITY( ACT_CROUCH_IDLE_DUAL );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK_DUAL );
	REGISTER_SHARED_ACTIVITY( ACT_JUMP_DUAL );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_SHOTGUN );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_AIM_SHOTGUN );
	REGISTER_SHARED_ACTIVITY( ACT_CROUCH_IDLE_SHOTGUN );
	REGISTER_SHARED_ACTIVITY( ACT_JUMP_SHOTGUN );
	REGISTER_SHARED_ACTIVITY( ACT_IDLE_AIM_RIFLE );
	REGISTER_SHARED_ACTIVITY( ACT_RELOAD_RIFLE );
	REGISTER_SHARED_ACTIVITY( ACT_CROUCH_IDLE_RIFLE );
	REGISTER_SHARED_ACTIVITY( ACT_RANGE_ATTACK_RIFLE );
	REGISTER_SHARED_ACTIVITY( ACT_JUMP_RIFLE );
	// Infested General AI
	REGISTER_SHARED_ACTIVITY( ACT_SLEEP );
	REGISTER_SHARED_ACTIVITY( ACT_WAKE );
	// Shield Bug
	REGISTER_SHARED_ACTIVITY( ACT_FLICK_LEFT );
	REGISTER_SHARED_ACTIVITY( ACT_FLICK_LEFT_MIDDLE );
	REGISTER_SHARED_ACTIVITY( ACT_FLICK_RIGHT_MIDDLE );
	REGISTER_SHARED_ACTIVITY( ACT_FLICK_RIGHT );
	REGISTER_SHARED_ACTIVITY( ACT_SPINAROUND );
	// Mortar Bug
	REGISTER_SHARED_ACTIVITY( ACT_PREP_TO_FIRE );
	REGISTER_SHARED_ACTIVITY( ACT_FIRE );
	REGISTER_SHARED_ACTIVITY( ACT_FIRE_RECOVER );
	// Shaman
	REGISTER_SHARED_ACTIVITY( ACT_SPRAY );
	// Boomer
	REGISTER_SHARED_ACTIVITY( ACT_PREP_EXPLODE );
	REGISTER_SHARED_ACTIVITY( ACT_EXPLODE );

	REGISTER_SHARED_ACTIVITY( ACT_DOTA_IDLE );
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_RUN );
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_ATTACK );
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_ATTACK_EVENT );
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_DIE );
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_FLINCH );
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_DISABLED );
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_CAST_ABILITY_1 );
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_CAST_ABILITY_2 );
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_CAST_ABILITY_3 );
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_CAST_ABILITY_4 );
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_OVERRIDE_ABILITY_1 );
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_OVERRIDE_ABILITY_2	);
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_OVERRIDE_ABILITY_3 );
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_OVERRIDE_ABILITY_4 );
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_CHANNEL_ABILITY_1 );
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_CHANNEL_ABILITY_2 );
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_CHANNEL_ABILITY_3 );
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_CHANNEL_ABILITY_4 );
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_CHANNEL_END_ABILITY_1 );
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_CHANNEL_END_ABILITY_2 );
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_CHANNEL_END_ABILITY_3 );
	REGISTER_SHARED_ACTIVITY( ACT_DOTA_CHANNEL_END_ABILITY_4 );

	// Portal2
	REGISTER_SHARED_ACTIVITY( ACT_MP_RUN_SPEEDPAINT );
	REGISTER_SHARED_ACTIVITY( ACT_MP_LONG_FALL );
	REGISTER_SHARED_ACTIVITY( ACT_MP_TRACTORBEAM_FLOAT );
	REGISTER_SHARED_ACTIVITY( ACT_MP_DEATH_CRUSH );

	REGISTER_SHARED_ACTIVITY( ACT_MP_RUN_SPEEDPAINT_PRIMARY );
	REGISTER_SHARED_ACTIVITY( ACT_MP_DROWNING_PRIMARY );
	REGISTER_SHARED_ACTIVITY( ACT_MP_LONG_FALL_PRIMARY );
	REGISTER_SHARED_ACTIVITY( ACT_MP_TRACTORBEAM_FLOAT_PRIMARY );
	REGISTER_SHARED_ACTIVITY( ACT_MP_DEATH_CRUSH_PRIMARY );

	// csgo death anims that don't require direction (direction is pose-param driven for more granularity)
	REGISTER_SHARED_ACTIVITY( ACT_DIE_STAND );
	REGISTER_SHARED_ACTIVITY( ACT_DIE_STAND_HEADSHOT );
	REGISTER_SHARED_ACTIVITY( ACT_DIE_CROUCH );
	REGISTER_SHARED_ACTIVITY( ACT_DIE_CROUCH_HEADSHOT );

	// CSGO action activities
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_NULL );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_DEFUSE );	
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_DEFUSE_WITH_KIT );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_FLASHBANG_REACTION );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_FIRE_PRIMARY );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_FIRE_PRIMARY_OPT_1 );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_FIRE_PRIMARY_OPT_2 );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_FIRE_SECONDARY );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_FIRE_SECONDARY_OPT_1 );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_FIRE_SECONDARY_OPT_2 );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_RELOAD );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_RELOAD_START );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_RELOAD_LOOP );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_RELOAD_END );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_OPERATE );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_DEPLOY );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_CATCH );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_SILENCER_DETACH );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_SILENCER_ATTACH );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_TWITCH );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_TWITCH_BUYZONE );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_PLANT_BOMB );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_IDLE_TURN_BALANCEADJUST );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_IDLE_ADJUST_STOPPEDMOVING );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_ALIVE_LOOP );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_FLINCH );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_FLINCH_HEAD );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_FLINCH_MOLOTOV );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_JUMP );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_FALL );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_CLIMB_LADDER );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_LAND_LIGHT );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_LAND_HEAVY );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_EXIT_LADDER_TOP );
	REGISTER_SHARED_ACTIVITY( ACT_CSGO_EXIT_LADDER_BOTTOM );

	contract_assert( ( g_HighestActivity == LAST_SHARED_ACTIVITY - 1 ) && "Not all activities from ai_activity.h registered in activitylist.cpp" ); 
} 

// HACKHACK: Keep backwards compatibility on broken activities temporarily
static constexpr auto ACTIVITY_FILE_TAG = 0x80800000;

void UTIL_LoadActivityRemapFile( const char *filename, const char *section, std::vector< CActivityRemap > &entries )
{
	if ( CActivityRemapCache *actRemap = ks::find_ptr( m_ActivityRemapDatabase, filename ) )
	{
		entries.append_range( actRemap->m_cachedActivityRemaps );
		return;
	}

	KeyValues *pkvFile = new KeyValues( section );

	if ( pkvFile->LoadFromFile( filesystem, filename, nullptr ) )
	{
		KeyValues *pTestKey = pkvFile->GetFirstSubKey();

		CActivityRemapCache actRemap;

		while ( pTestKey )
		{
			Activity ActBase = (Activity)ActivityList_IndexForName( pTestKey->GetName() );

			if ( ActBase != ACT_INVALID )
			{
				KeyValues *pRemapKey = pTestKey->GetFirstSubKey();

				CActivityRemap actMap;
				actMap.activity = ActBase;

				while ( pRemapKey )
				{
					const char *pKeyName = pRemapKey->GetName();
					const char *pKeyValue = pRemapKey->GetString();

					if ( !stricmp( pKeyName, "remapactivity" ) )
					{
						Activity Act = (Activity)ActivityList_IndexForName( pKeyValue );

						if ( Act == ACT_INVALID )
						{
							actMap.mappedActivity = ActivityList_RegisterPrivateActivity( pKeyValue );
						}
						else
						{
							actMap.mappedActivity = Act;
						}
					}
					else if ( !stricmp( pKeyName, "extra" ) )
					{
						actMap.SetExtraKeyValueBlock( pRemapKey->MakeCopy() );
					}

					pRemapKey = pRemapKey->GetNextKey();
				}

				entries.push_back( actMap );
			}

			pTestKey = pTestKey->GetNextKey();
		}

		actRemap.m_cachedActivityRemaps.append_range( entries );
		m_ActivityRemapDatabase.emplace( filename, actRemap );
	}
}

int ActivityList_HighestIndex()
{
	return g_HighestActivity;
}

CActivityRemapCache::CActivityRemapCache( const CActivityRemapCache& src)
{
	int c = std::ssize( src.m_cachedActivityRemaps );
	for ( int i = 0; i < c; i++ )
	{
		m_cachedActivityRemaps.push_back( src.m_cachedActivityRemaps[ i ] );
	}
}

class CAmmoDef;
class CBaseCombatCharacter;

using namespace ::ks::reflect;

// Purpose: Base combat character with no AI

//-----------------------------------------------------------------------------
// Purpose: Return a pointer to the Ammo at the Index passed in
//-----------------------------------------------------------------------------
Ammo_t *CAmmoDef::GetAmmoOfIndex(int nAmmoIndex)
{
	if ( nAmmoIndex >= m_nAmmoIndex )
		return nullptr;

	return &m_AmmoType[ nAmmoIndex ];
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CAmmoDef::Index(const char *psz)
{
	int i;

	if (!psz)
		return -1;

	for (i = 1; i < m_nAmmoIndex; i++)
	{
		if (stricmp( psz, m_AmmoType[i].pName ) == 0)
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int	CAmmoDef::PlrDamage(int nAmmoIndex)
{
	if ( nAmmoIndex < 1 || nAmmoIndex >= m_nAmmoIndex )
		return 0;

	if ( m_AmmoType[nAmmoIndex].pPlrDmg == USE_CVAR )
	{
		if ( m_AmmoType[nAmmoIndex].pPlrDmgCVar )
		{
			return m_AmmoType[nAmmoIndex].pPlrDmgCVar->GetFloat();
		}

		return 0;
	}
	else
	{
		return m_AmmoType[nAmmoIndex].pPlrDmg;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int	CAmmoDef::NPCDamage(int nAmmoIndex)
{
	if ( nAmmoIndex < 1 || nAmmoIndex >= m_nAmmoIndex )
		return 0;

	if ( m_AmmoType[nAmmoIndex].pNPCDmg == USE_CVAR )
	{
		if ( m_AmmoType[nAmmoIndex].pNPCDmgCVar )
		{
			return m_AmmoType[nAmmoIndex].pNPCDmgCVar->GetFloat();
		}

		return 0;
	}
	else
	{
		return m_AmmoType[nAmmoIndex].pNPCDmg;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int	CAmmoDef::MaxCarry(int nAmmoIndex, const CBaseCombatCharacter *owner)
{
	if ( nAmmoIndex < 1 || nAmmoIndex >= m_nAmmoIndex )
		return 0;

	if ( m_AmmoType[nAmmoIndex].pMaxCarry == USE_CVAR )
	{
		if ( m_AmmoType[nAmmoIndex].pMaxCarryCVar )
			return m_AmmoType[nAmmoIndex].pMaxCarryCVar->GetInt();

		return 0;
	}
	else
	{
		return m_AmmoType[nAmmoIndex].pMaxCarry;
	}
}

bool CAmmoDef::CanCarryInfiniteAmmo(int nAmmoIndex)
{
	if ( nAmmoIndex < 1 || nAmmoIndex >= m_nAmmoIndex )
		return false;

	int maxCarry = m_AmmoType[nAmmoIndex].pMaxCarry;
	if ( maxCarry == USE_CVAR )
	{
		if ( m_AmmoType[nAmmoIndex].pMaxCarryCVar )
		{
			maxCarry = m_AmmoType[nAmmoIndex].pMaxCarryCVar->GetInt();
		}
	}
	return maxCarry == INFINITE_AMMO ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int	CAmmoDef::DamageType(int nAmmoIndex)
{
	if (nAmmoIndex < 1 || nAmmoIndex >= m_nAmmoIndex)
		return 0;

	return m_AmmoType[nAmmoIndex].nDamageType;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CAmmoDef::Flags(int nAmmoIndex)
{
	if (nAmmoIndex < 1 || nAmmoIndex >= m_nAmmoIndex)
		return 0;

	return m_AmmoType[nAmmoIndex].nFlags;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int	CAmmoDef::MinSplashSize(int nAmmoIndex)
{
	if (nAmmoIndex < 1 || nAmmoIndex >= m_nAmmoIndex)
		return 4;

	return m_AmmoType[nAmmoIndex].nMinSplashSize;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int	CAmmoDef::MaxSplashSize(int nAmmoIndex)
{
	if (nAmmoIndex < 1 || nAmmoIndex >= m_nAmmoIndex)
		return 8;

	return m_AmmoType[nAmmoIndex].nMaxSplashSize;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int	CAmmoDef::TracerType(int nAmmoIndex)
{
	if (nAmmoIndex < 1 || nAmmoIndex >= m_nAmmoIndex)
		return 0;

	return m_AmmoType[nAmmoIndex].eTracerType;
}

//-----------------------------------------------------------------------------
// Purpose: Figure out the damage force for a certian ammo type.
// Input  :
// Output :
//-----------------------------------------------------------------------------
float CAmmoDef::DamageForce(int nAmmoIndex)
{
	const float DEFAULT_IMPULSE = 1.0f;

	if ( nAmmoIndex < 1 || nAmmoIndex >= m_nAmmoIndex )
		return DEFAULT_IMPULSE;

	if ( m_AmmoType[nAmmoIndex].pPhysicsForceImpulse == USE_CVAR )
	{
		if ( m_AmmoType[nAmmoIndex].pPhysicsForceImpulseCVar )
			return m_AmmoType[nAmmoIndex].pPhysicsForceImpulseCVar->GetFloat();

		return DEFAULT_IMPULSE;
	}
	else
	{
		return (float)m_AmmoType[nAmmoIndex].pPhysicsForceImpulse;
	}

}

//-----------------------------------------------------------------------------
// Purpose: Create an Ammo type with the name, decal, and tracer.
// Does not increment m_nAmmoIndex because the functions below do so and 
//  are the only entry point.
//-----------------------------------------------------------------------------
bool CAmmoDef::AddAmmoType(char const* name, int damageType, int tracerType, int nFlags, int minSplashSize, int maxSplashSize )
{
	if (m_nAmmoIndex == MAX_AMMO_TYPES)
		return false;

	int len = std::strlen(name);
	m_AmmoType[m_nAmmoIndex].pName = new char[len+1];
	V_strncpy(m_AmmoType[m_nAmmoIndex].pName, name,len+1);
	m_AmmoType[m_nAmmoIndex].nDamageType	= damageType;
	m_AmmoType[m_nAmmoIndex].eTracerType	= tracerType;
	m_AmmoType[m_nAmmoIndex].nMinSplashSize	= minSplashSize;
	m_AmmoType[m_nAmmoIndex].nMaxSplashSize	= maxSplashSize;
	m_AmmoType[m_nAmmoIndex].nFlags	= nFlags;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Add an ammo type with it's damage & carrying capability specified via cvars
//-----------------------------------------------------------------------------
void CAmmoDef::AddAmmoType(char const* name, int damageType, int tracerType, 
	char const* plr_cvar, char const* npc_cvar, char const* carry_cvar, 
	char const* impulse_cvar, int nFlags, int minSplashSize, int maxSplashSize)
{
	if ( AddAmmoType( name, damageType, tracerType, nFlags, minSplashSize, maxSplashSize ) == false )
		return;

	if (plr_cvar)
	{
		m_AmmoType[m_nAmmoIndex].pPlrDmgCVar	= cvar->FindVar(plr_cvar);
		if (!m_AmmoType[m_nAmmoIndex].pPlrDmgCVar)
		{
			Msg("ERROR: Ammo (%s) found no CVar named (%s)\n",name,plr_cvar);
		}
		m_AmmoType[m_nAmmoIndex].pPlrDmg = USE_CVAR;
	}
	if (npc_cvar)
	{
		m_AmmoType[m_nAmmoIndex].pNPCDmgCVar	= cvar->FindVar(npc_cvar);
		if (!m_AmmoType[m_nAmmoIndex].pNPCDmgCVar)
		{
			Msg("ERROR: Ammo (%s) found no CVar named (%s)\n",name,npc_cvar);
		}
		m_AmmoType[m_nAmmoIndex].pNPCDmg = USE_CVAR;
	}
	if (carry_cvar)
	{
		m_AmmoType[m_nAmmoIndex].pMaxCarryCVar= cvar->FindVar(carry_cvar);
		if (!m_AmmoType[m_nAmmoIndex].pMaxCarryCVar)
		{
			Msg("ERROR: Ammo (%s) found no CVar named (%s)\n",name,carry_cvar);
		}
		m_AmmoType[m_nAmmoIndex].pMaxCarry = USE_CVAR;
	}
	if (impulse_cvar)
	{
		m_AmmoType[m_nAmmoIndex].pPhysicsForceImpulseCVar = cvar->FindVar(impulse_cvar);
		if (!m_AmmoType[m_nAmmoIndex].pPhysicsForceImpulseCVar)
		{
			Msg("ERROR: Ammo (%s) found no CVar named (%s)\n",name,impulse_cvar);
		}
		else
		{
			// By default, use whatever value is in the convar at load time.
			// To enable real-time updating of the ammo impulse values, execute "tweak_ammo_impulse" which will set pPhysicsForceImpulse to USE_CVAR.
			m_AmmoType[m_nAmmoIndex].pPhysicsForceImpulse = m_AmmoType[m_nAmmoIndex].pPhysicsForceImpulseCVar->GetInt();
		}
	}

	m_nAmmoIndex++;
}

//-----------------------------------------------------------------------------
// Purpose: Add an ammo type with it's damage & carrying capability specified via integers
//-----------------------------------------------------------------------------
void CAmmoDef::AddAmmoType(char const* name, int damageType, int tracerType, 
	int plr_dmg, int npc_dmg, int carry, int impulse, 
	int nFlags, int minSplashSize, int maxSplashSize )
{
	if ( AddAmmoType( name, damageType, tracerType, nFlags, minSplashSize, maxSplashSize ) == false )
		return;

	m_AmmoType[m_nAmmoIndex].pPlrDmg = plr_dmg;
	m_AmmoType[m_nAmmoIndex].pNPCDmg = npc_dmg;
	m_AmmoType[m_nAmmoIndex].pMaxCarry = carry;
	m_AmmoType[m_nAmmoIndex].pPhysicsForceImpulse = impulse;

	m_nAmmoIndex++;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  :
// Output :
//-----------------------------------------------------------------------------
CAmmoDef::CAmmoDef(void)
{
	// Start with an index of 1.  Client assumes 0 is an invalid ammo type
	m_nAmmoIndex = 1;
	std::memset( m_AmmoType, 0, sizeof( m_AmmoType ) );
}

CAmmoDef::~CAmmoDef( void )
{
	for ( int i = 1; i < MAX_AMMO_TYPES; i++ )
	{
		delete[] m_AmmoType[ i ].pName;
	}
}

class CBaseEntity;

using namespace ::ks::reflect;

class CAnimatedEntityTextureProxy : public CBaseAnimatedTextureProxy
{
public:
	CAnimatedEntityTextureProxy() {}
	virtual ~CAnimatedEntityTextureProxy() {}

	virtual float GetAnimationStartTime( void* pBaseEntity );
	virtual void AnimationWrapped( void* pC_BaseEntity );

};

EXPOSE_MATERIAL_PROXY( CAnimatedEntityTextureProxy, AnimatedEntityTexture );

float CAnimatedEntityTextureProxy::GetAnimationStartTime( void* pArg )
{
	IClientRenderable *pRend = (IClientRenderable *)pArg;
	if (!pRend)
		return 0.0f;

	CBaseEntity* pEntity = EntityCast<CBaseEntity>( pRend->GetIClientUnknown()->GetBaseEntity() );
	if (pEntity)
	{
		return pEntity->GetTextureAnimationStartTime();
	}
	return 0.0f;
}

void CAnimatedEntityTextureProxy::AnimationWrapped( void* pArg )
{
	IClientRenderable *pRend = (IClientRenderable *)pArg;
	if (!pRend)
		return;

	CBaseEntity* pEntity = EntityCast<CBaseEntity>( pRend->GetIClientUnknown()->GetBaseEntity() );
	if (pEntity)
	{
		pEntity->TextureAnimationWrapped();
	}
}

class CBaseEntity;

using namespace ::ks::reflect;

class CAnimatedOffsetTextureProxy : public CBaseAnimatedTextureProxy
{
public:
	CAnimatedOffsetTextureProxy() : m_flFrameOffset( 0.0f ) {}

	virtual ~CAnimatedOffsetTextureProxy() {}

	virtual float GetAnimationStartTime( void* pBaseEntity );
	virtual void OnBind( void *pBaseEntity );

protected:

	float	m_flFrameOffset;
};

EXPOSE_MATERIAL_PROXY( CAnimatedOffsetTextureProxy, AnimatedOffsetTexture );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pArg - 
// Output : float
//-----------------------------------------------------------------------------
float CAnimatedOffsetTextureProxy::GetAnimationStartTime( void* pArg )
{
	return m_flFrameOffset;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pBaseEntity - 
//-----------------------------------------------------------------------------
void CAnimatedOffsetTextureProxy::OnBind( void *pBaseEntity )
{
	CBaseEntity* pEntity = (CBaseEntity*)pBaseEntity;
	
	if ( pEntity )
	{
		m_flFrameOffset = pEntity->GetTextureAnimationStartTime();
	}

	// Call into the base class
	CBaseAnimatedTextureProxy::OnBind( pBaseEntity );
}

class CAnimatedTextureProxy : public CBaseAnimatedTextureProxy
{
public:
	CAnimatedTextureProxy() {}
	virtual ~CAnimatedTextureProxy() {}
	virtual float GetAnimationStartTime( void* pBaseEntity );
};

EXPOSE_MATERIAL_PROXY( CAnimatedTextureProxy, AnimatedTexture );

float CAnimatedTextureProxy::GetAnimationStartTime( void* pBaseEntity )
{
	return 0;
}

using namespace ::ks::reflect;

// Purpose: Acts exactly like "AnimatedTexture", but ONLY if the texture
//			it's working on matches the desired texture to work on.
//			This assumes that some other proxy will be switching out the textures.

class CAnimateSpecificTexture : public CBaseAnimatedTextureProxy
{
private:
	std::string m_OnlyAnimateOnTexture;
public:
	virtual float GetAnimationStartTime( void* pBaseEntity ) { return 0; }
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
	virtual void Release( void ) { delete this; }
};

bool CAnimateSpecificTexture::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	char const* pszAnimateOnTexture = pKeyValues->GetString( "onlyAnimateOnTexture" );
	if( !pszAnimateOnTexture )
		return false;

	m_OnlyAnimateOnTexture.assign( ks::nz( pszAnimateOnTexture ) );

	return CBaseAnimatedTextureProxy::Init( pMaterial, pKeyValues );
}

void CAnimateSpecificTexture::OnBind( void *pC_BaseEntity )
{
	if( FStrEq( m_AnimatedTextureVar->GetTextureValue()->GetName(), m_OnlyAnimateOnTexture.c_str() ) )
	{
		CBaseAnimatedTextureProxy::OnBind( pC_BaseEntity );
	}
	//else do nothing
}

EXPOSE_MATERIAL_PROXY( CAnimateSpecificTexture, AnimateSpecificTexture );

class CBaseEntity;
struct animevent_t;

using namespace ::ks::reflect;

#define iabs(i) (( (i) >= 0 ) ? (i) : -(i) )

int ExtractBbox( CStudioHdr *pstudiohdr, int sequence, Vector& mins, Vector& maxs )
{
	if (! pstudiohdr)
		return 0;

	if (!pstudiohdr->SequencesAvailable())
		return 0;

	mstudioseqdesc_t	&seqdesc = pstudiohdr->pSeqdesc( sequence );
	
	mins = seqdesc.bbmin;

	maxs = seqdesc.bbmax;

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//
// Input  : *pstudiohdr - 
//			iSequence - 
//
// Output : mstudioseqdesc_t
//-----------------------------------------------------------------------------

extern int g_nActivityListVersion;
extern int g_nEventListVersion;

void SetEventIndexForSequence( mstudioseqdesc_t &seqdesc )
{
	if ( &seqdesc == nullptr )
		 return;

	if ( seqdesc.numevents == 0 )
		 return;

	seqdesc.flags |= STUDIO_EVENT_CLIENT;

	for ( int index = 0; index < (int)seqdesc.numevents; index++ )
	{
		mstudioevent_for_client_server_t *pevent = (mstudioevent_for_client_server_t*)seqdesc.pEvent( index );

		if ( !pevent )
			 continue;

		if ( pevent->type & AE_TYPE_NEWEVENTSYSTEM )
		{
			const char *pEventName = pevent->pszEventName();
			
			int iEventIndex = EventList_IndexForName( pEventName );
				
			if ( iEventIndex == -1 )
			{
				pevent->event_newsystem = EventList_RegisterPrivateEvent( pEventName );
			}
			else
			{
				pevent->event_newsystem = iEventIndex;
				pevent->type |= EventList_GetEventType( iEventIndex );
			}
		}
	}
}

mstudioevent_for_client_server_t *GetEventIndexForSequence( mstudioseqdesc_t &seqdesc )
{
	if ( !(seqdesc.flags & STUDIO_EVENT_CLIENT) )
	{
		SetEventIndexForSequence( seqdesc );
	}

	return (mstudioevent_for_client_server_t*)seqdesc.pEvent( 0 );
}

void BuildAllAnimationEventIndexes( CStudioHdr *pstudiohdr )
{
	if ( !pstudiohdr )
		return;

	if( pstudiohdr->GetEventListVersion() != g_nEventListVersion )
	{
		for ( int i = 0 ; i < pstudiohdr->GetNumSeq() ; i++ )
		{
			SetEventIndexForSequence( pstudiohdr->pSeqdesc( i ) );
		}

		pstudiohdr->SetEventListVersion( g_nEventListVersion );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Ensures that activity / index relationship is recalculated
// Input  :
// Output :
//-----------------------------------------------------------------------------
void ResetEventIndexes( CStudioHdr *pstudiohdr )
{	
	if (! pstudiohdr)
		return;

	pstudiohdr->SetEventListVersion( g_nEventListVersion - 1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

void SetActivityForSequence( CStudioHdr *pstudiohdr, int i )
{
	int iActivityIndex;
	const char *pszActivityName;
	mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( i );

	seqdesc.flags |= STUDIO_ACTIVITY;

	pszActivityName = GetSequenceActivityName( pstudiohdr, i );
	if ( pszActivityName[0] != '\0' )
	{
		iActivityIndex = ActivityList_IndexForName( pszActivityName );
		
		if ( iActivityIndex == -1 )
		{
			// Allow this now.  Animators can create custom activities that are referenced only on the client or by scripts, etc.
			//Warning( "***\nModel %s tried to reference unregistered activity: %s \n***\n", pstudiohdr->name, pszActivityName );
			//Assert(0);
			// HACK: the client and server don't share the private activity list so registering it on the client would hose the server
			seqdesc.flags &= ~STUDIO_ACTIVITY;
		}
		else
		{
			seqdesc.activity = iActivityIndex;
		}
	}
}

//=========================================================
// IndexModelSequences - set activity and event indexes for all model
// sequences that have them.
//=========================================================

void IndexModelSequences( CStudioHdr *pstudiohdr )
{
	CVProfScope _( "IndexModelSequences", 2, IsServerDll() ? VPROF_BUDGETGROUP_SERVER_ANIM : VPROF_BUDGETGROUP_CLIENT_ANIMATION, false, BUDGETFLAG_ALL );
	int i;

	if (! pstudiohdr)
		return;

	if (!pstudiohdr->SequencesAvailable())
		return;

	for ( i = 0 ; i < pstudiohdr->GetNumSeq() ; i++ )
	{
		SetActivityForSequence( pstudiohdr, i );
		SetEventIndexForSequence( pstudiohdr->pSeqdesc( i ) );
	}

	pstudiohdr->SetActivityListVersion( g_nActivityListVersion );
}

//-----------------------------------------------------------------------------
// Purpose: Ensures that activity / index relationship is recalculated
// Input  :
// Output :
//-----------------------------------------------------------------------------
void ResetActivityIndexes( CStudioHdr *pstudiohdr )
{	
	if (! pstudiohdr)
		return;

	pstudiohdr->SetActivityListVersion( g_nActivityListVersion - 1 );
}

void VerifySequenceIndex( CStudioHdr *pstudiohdr )
{
	if ( !pstudiohdr )
	{
		return;
	}

	if( pstudiohdr->GetActivityListVersion( ) < g_nActivityListVersion ) // sometimes the server's numbers can get ahead of the client's if we're sharing memory between the two, so it's only necessary to reindex if a model is lagging the version number.
	{
		// this model's sequences have not yet been indexed by activity
		IndexModelSequences( pstudiohdr );
	}
}

bool IsInPrediction()
{
	return CBaseEntity::GetPredictionPlayer() != nullptr;
}

static int SelectWeightedSequenceFromMapping( CStudioHdr::CActivityToSequenceMapping &mapping, CStudioHdr *pstudiohdr, int activity, int curSequence );

int SelectWeightedSequence( CStudioHdr *pstudiohdr, int activity, int curSequence )
{
	CVProfScope _( "SelectWeightedSequence", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	VProfIncrementCounter<"Client SelectWeightedSequence">( 1 );

	if (! pstudiohdr)
		return 0;

	if (!pstudiohdr->SequencesAvailable())
		return 0;

	VerifySequenceIndex( pstudiohdr );

	int numSeq = pstudiohdr->GetNumSeq();
	if ( numSeq == 1 )
	{
		return ( GetSequenceActivity( pstudiohdr, 0, nullptr ) == activity ) ? 0 : ACTIVITY_NOT_AVAILABLE;
	}

	return SelectWeightedSequenceFromMapping( *pstudiohdr->ActivityToSequence(), pstudiohdr, activity, curSequence );
}

// Pick a sequence for the given activity. If the current sequence is appropriate for the 
// current activity, and its stored weight is negative (whatever that means), always select
// it. Otherwise perform a weighted selection -- imagine a large roulette wheel, with each
// sequence having a number of spaces corresponding to its weight.
static int SelectWeightedSequenceFromMapping( CStudioHdr::CActivityToSequenceMapping &mapping, CStudioHdr *pstudiohdr, int activity, int curSequence )
{
	using SequenceTuple = CStudioHdr::CActivityToSequenceMapping::SequenceTuple;
	using HashValueType = CStudioHdr::CActivityToSequenceMapping::HashValueType;
	// is the current sequence appropriate?
	if (curSequence >= 0)
	{
		mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( curSequence );

		if (seqdesc.activity == activity && seqdesc.actweight < 0)
			return curSequence;
	}

	if ( !pstudiohdr->SequencesAvailable() )
	{
		return ACTIVITY_NOT_AVAILABLE;
	}

	if ( pstudiohdr->GetNumSeq() == 1 )
	{
		contract_assert( false && "Expected single sequence case to be handled in ::SelectWeightedSequence()" );
		return ACTIVITY_NOT_AVAILABLE;
	}

	if (!mapping.ValidateAgainst(pstudiohdr))
	{
		contract_assert( false && "CStudioHdr %s has changed its vmodel pointer without reinitializing its activity mapping! Now performing emergency reinitialization." );
		ExecuteOnce( [&]{ DebuggerBreakIfDebugging(); } );
		mapping.Reinitialize(pstudiohdr);
	}

	// a null m_pSequenceTuples just means that this studio header has no activities.
	if (!mapping.m_pSequenceTuples)
		return ACTIVITY_NOT_AVAILABLE;

	// get the data for the given activity
	const HashValueType * __restrict actData = ks::find_ptr( mapping.m_ActToSeqHash, activity );
	if (!actData)
	{
		return ACTIVITY_NOT_AVAILABLE;
	}

	contract_assert( ( actData->totalWeight > 0 ) && "Activity %d has total weight of %d!" );
	int weighttotal = actData->totalWeight;

	// failsafe if the weight is 0: assume the artist screwed up and that the first sequence
	// for this activity should be returned.
	int randomValue = 0;
	if ( actData->totalWeight <= 0 )
	{
		Warning( "Activity %s has %d sequences with a total weight of %d!", ActivityList_NameForIndex(activity), actData->count, actData->totalWeight );
		return (mapping.m_pSequenceTuples + actData->startingIdx)->seqnum;
	}
	else if ( actData->totalWeight == 1 )
	{
		randomValue = 0;
	}
	else
	{
		// generate a random number from 0 to the total weight
		if ( IsInPrediction() )
		{
			randomValue = SharedRandomInt( "SelectWeightedSequence", 0, weighttotal - 1 );
		}
		else
		{
			randomValue = RandomInt( 0, weighttotal - 1 );
		}
	}

	// chug through the entries in the list (they are sequential therefore cache-coherent)
	// until we run out of random juice
	SequenceTuple * __restrict sequenceInfo = mapping.m_pSequenceTuples + actData->startingIdx;

	const SequenceTuple *const stopHere = sequenceInfo + actData->count; // this is a backup 
		// in case the weights are somehow miscalculated -- we don't read or write through
		// it (because it aliases the restricted pointer above); it's only here for 
		// the comparison.

	while (randomValue >= sequenceInfo->weight && sequenceInfo < stopHere)
	{
		randomValue -= sequenceInfo->weight;
		++sequenceInfo;
	}

	return sequenceInfo->seqnum;

}

int SelectWeightedSequenceFromModifiers( CStudioHdr *pstudiohdr, int activity, const ActivityModifierSym_t *pActivityModifiers, int iModifierCount )
{
	using SequenceTuple = CStudioHdr::CActivityToSequenceMapping::SequenceTuple;
	using HashValueType = CStudioHdr::CActivityToSequenceMapping::HashValueType;
	CStudioHdr::CActivityToSequenceMapping &mapping = *pstudiohdr->ActivityToSequence();
	if ( !pstudiohdr->SequencesAvailable() )
	{
		return ACTIVITY_NOT_AVAILABLE;
	}

	VerifySequenceIndex( pstudiohdr );

	if ( pstudiohdr->GetNumSeq() == 1 )
	{
		return ( ::GetSequenceActivity( pstudiohdr, 0, nullptr ) == activity ) ? 0 : ACTIVITY_NOT_AVAILABLE;
	}

	if (!mapping.ValidateAgainst(pstudiohdr))
	{
		contract_assert( false && "CStudioHdr %s has changed its vmodel pointer without reinitializing its activity mapping! Now performing emergency reinitialization." );
		ExecuteOnce( [&]{ DebuggerBreakIfDebugging(); } );
		mapping.Reinitialize(pstudiohdr);
	}

	// a null m_pSequenceTuples just means that this studio header has no activities.
	if (!mapping.m_pSequenceTuples)
		return ACTIVITY_NOT_AVAILABLE;

	// get the data for the given activity
	const HashValueType * __restrict actData = ks::find_ptr( mapping.m_ActToSeqHash, activity );
	if (!actData)
	{
		return ACTIVITY_NOT_AVAILABLE;
	}
	
	// go through each sequence and give actmod matches preference. LACK of an actmod is a detriment
	int top_score = -1;
	std::vector< int > topScoring; topScoring.reserve( actData->count );
	for ( int i = 0; i < actData->count; i++ )
	{
		SequenceTuple * __restrict sequenceInfo = mapping.m_pSequenceTuples + actData->startingIdx + i;
		int score = 0;

		int num_seq_modifiers = sequenceInfo->iNumActivityModifiers;
		for ( int k = 0; k < num_seq_modifiers; k++ )
		{
			bool bFoundMod = false;

			for ( int m = 0; m < iModifierCount; m++ )
			{
				if ( sequenceInfo->pActivityModifiers[ k ] == pActivityModifiers[ m ] )
				{
					bFoundMod = true;
				}
			}

			if ( bFoundMod )
			{
				score += 2;
			}
			else
			{
				score--;
			}

		}
		
		if ( score >= top_score )
		{
			if ( score > top_score )
			{
				topScoring.clear();
			}
		
			for ( int n=0; n<sequenceInfo->weight; n++ )
			{
				topScoring.push_back( sequenceInfo->seqnum );
			}
		
			top_score = score;
		}
	}

	// randomly pick between the highest scoring sequences ( NOTE: this method of selecting a sequence ignores activity weights )
	//if ( IsInPrediction() )
	//{
	//	return topScoring[ SharedRandomInt( "SelectWeightedSequence", 0, topScoring.Count() - 1 ) ];
	//}
	if ( !topScoring.empty() )
	{
		return topScoring[ RandomInt( 0, std::ssize( topScoring ) - 1 ) ];
	}
	else
	{
		return -1;
	}
}

int SelectHeaviestSequence( CStudioHdr *pstudiohdr, int activity )
{
	if ( !pstudiohdr )
		return 0;

	VerifySequenceIndex( pstudiohdr );

	int maxweight = 0;
	int seq = ACTIVITY_NOT_AVAILABLE;
	int weight = 0;
	for (int i = 0; i < pstudiohdr->GetNumSeq(); i++)
	{
		int curActivity = GetSequenceActivity( pstudiohdr, i, &weight );
		if (curActivity == activity)
		{
			if ( iabs(weight) > maxweight )
			{
				maxweight = iabs(weight);
				seq = i;
			}
		}
	}

	return seq;
}

void GetEyePosition ( CStudioHdr *pstudiohdr, Vector &vecEyePosition )
{
	if ( !pstudiohdr )
	{
		Warning( "GetEyePosition() Can't get pstudiohdr ptr!\n" );
		return;
	}

	vecEyePosition = pstudiohdr->eyeposition();
}

//-----------------------------------------------------------------------------
// Purpose: Looks up an activity by name.
// Input  : label - Name of the activity to look up, ie "ACT_IDLE"
// Output : Activity index or ACT_INVALID if not found.
//-----------------------------------------------------------------------------
int LookupActivity( CStudioHdr *pstudiohdr, const char *label )
{
	CVProfScope _( "LookupActivity", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	if ( !pstudiohdr )
	{
		return 0;
	}

	for ( int i = 0; i < pstudiohdr->GetNumSeq(); i++ )
	{
		mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( i );
		if ( stricmp( seqdesc.pszActivityName(), label ) == 0 )
		{
			return seqdesc.activity;
		}
	}

	return ACT_INVALID;
}

//-----------------------------------------------------------------------------
// Purpose: Looks up a sequence by sequence name first, then by activity name.
// Input  : label - The sequence name or activity name to look up.
// Output : Returns the sequence index of the matching sequence, or ACT_INVALID.
//-----------------------------------------------------------------------------
int LookupSequence( CStudioHdr *pstudiohdr, const char *label )
{
	CVProfScope _( "LookupSequence", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	if (! pstudiohdr)
		return 0;

	if (!pstudiohdr->SequencesAvailable())
		return 0;

	//
	// Look up by sequence name.
	//
	int iSequence = pstudiohdr->LookupSequence( label );
	if ( iSequence != -1 )
		return iSequence;

	//
	// Not found, look up by activity name.
	//
	int nActivity = LookupActivity( pstudiohdr, label );
	if (nActivity != ACT_INVALID )
	{
		return SelectWeightedSequence( pstudiohdr, nActivity );
	}

	return ACT_INVALID;
}

void GetSequenceLinearMotion( CStudioHdr *pstudiohdr, int iSequence, const float poseParameter[], Vector *pVec )
{
	if (! pstudiohdr)
	{
		Msg( "Bad pstudiohdr in GetSequenceLinearMotion()!\n" );
		return;
	}

	if (!pstudiohdr->SequencesAvailable())
		return;

	if( iSequence < 0 || iSequence >= pstudiohdr->GetNumSeq() )
	{
		// Don't spam on bogus model
		if ( pstudiohdr->GetNumSeq() > 0 )
		{
			static int msgCount = 0;
			while ( ++msgCount < 10 )
			{
				DevMsg( "Bad sequence (%i out of %i max) in GetSequenceLinearMotion() for model '%s'!\n", iSequence, pstudiohdr->GetNumSeq(), pstudiohdr->pszName() );
			}
		}
		pVec->Init();
		return;
	}

	QAngle vecAngles;
	Studio_SeqMovement( pstudiohdr, iSequence, 0, 1.0, poseParameter, (*pVec), vecAngles );
}

float GetSequenceLinearMotionAndDuration( CStudioHdr *pstudiohdr, int iSequence, const float poseParameter[], Vector *pVec )
{
	pVec->Init();
	if ( !pstudiohdr )
	{
		Msg( "Bad pstudiohdr in GetSequenceLinearMotion()!\n" );
		return 0.0f;
	}

	if ( !pstudiohdr->SequencesAvailable() )
		return 0.0f;

	if ( iSequence < 0 || iSequence >= pstudiohdr->GetNumSeq() )
	{
		// Don't spam on bogus model
		if ( pstudiohdr->GetNumSeq() > 0 )
		{
			static int msgCount = 0;
			while ( ++msgCount < 10 )
			{
				DevMsg( "Bad sequence (%i out of %i max) in GetSequenceLinearMotion() for model '%s'!\n", iSequence, pstudiohdr->GetNumSeq(), pstudiohdr->pszName() );
			}
		}
		return 0.0f;
	}

	return Studio_SeqMovementAndDuration( pstudiohdr, iSequence, 0, 1.0, poseParameter, (*pVec) );
}

const char *GetSequenceName( CStudioHdr *pstudiohdr, int iSequence )
{
	if( !pstudiohdr || iSequence < 0 || iSequence >= pstudiohdr->GetNumSeq() )
	{
		if ( pstudiohdr )
		{
			DevMsg( "Bad sequence in GetSequenceName() for model '%s'!\n", pstudiohdr->pszName() );
		}
		return "Unknown";
	}

	mstudioseqdesc_t	&seqdesc = pstudiohdr->pSeqdesc( iSequence );
	return seqdesc.pszLabel();
}

const char *GetSequenceActivityName( CStudioHdr *pstudiohdr, int iSequence )
{
	if( !pstudiohdr || iSequence < 0 || iSequence >= pstudiohdr->GetNumSeq() )
	{
		if ( pstudiohdr )
		{
			DevMsg( "Bad sequence in GetSequenceActivityName() for model '%s'!\n", pstudiohdr->pszName() );
		}
		return "Unknown";
	}

	mstudioseqdesc_t	&seqdesc = pstudiohdr->pSeqdesc( iSequence );
	return seqdesc.pszActivityName( );
}

int GetSequenceFlags( CStudioHdr *pstudiohdr, int sequence )
{
	if ( !pstudiohdr || 
		 !pstudiohdr->SequencesAvailable() ||
		sequence < 0 || 
		sequence >= pstudiohdr->GetNumSeq() )
	{
		return 0;
	}

	mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( sequence );

	return seqdesc.flags;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pstudiohdr - 
//			sequence - 
//			type - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool HasAnimationEventOfType( CStudioHdr *pstudiohdr, int sequence, int type )
{
	if ( !pstudiohdr || sequence >= pstudiohdr->GetNumSeq() )
		return false;

	mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( sequence );
	if ( !&seqdesc )
		return false;

	mstudioevent_for_client_server_t *pevent = GetEventIndexForSequence( seqdesc );
	if ( !pevent )
		return false;

	if (seqdesc.numevents == 0 )
		return false;

	int index;
	for ( index = 0; index < (int)seqdesc.numevents; index++ )
	{
		if ( pevent[ index ].Event() == type )
		{
			return true;
		}
	}

	return false;
}

struct animtaglookup_t
{
	int nIndex;
	const char* szName;
};

#define REGISTER_ANIMTAG( _n ) { _n, #_n },
const animtaglookup_t g_AnimTagLookupTable[ANIMTAG_COUNT] = 
{
	REGISTER_ANIMTAG( ANIMTAG_UNINITIALIZED )
	REGISTER_ANIMTAG( ANIMTAG_STARTCYCLE_N )
	REGISTER_ANIMTAG( ANIMTAG_STARTCYCLE_NE )
	REGISTER_ANIMTAG( ANIMTAG_STARTCYCLE_E )
	REGISTER_ANIMTAG( ANIMTAG_STARTCYCLE_SE )
	REGISTER_ANIMTAG( ANIMTAG_STARTCYCLE_S )
	REGISTER_ANIMTAG( ANIMTAG_STARTCYCLE_SW )
	REGISTER_ANIMTAG( ANIMTAG_STARTCYCLE_W )
	REGISTER_ANIMTAG( ANIMTAG_STARTCYCLE_NW )

	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_YAWMIN_IDLE )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_YAWMAX_IDLE )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_YAWMIN_WALK )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_YAWMAX_WALK )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_YAWMIN_RUN )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_YAWMAX_RUN )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_YAWMIN_CROUCHIDLE )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_YAWMAX_CROUCHIDLE )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_YAWMIN_CROUCHWALK )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_YAWMAX_CROUCHWALK )

	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_PITCHMIN_IDLE )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_PITCHMAX_IDLE )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_PITCHMIN_WALKRUN )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_PITCHMAX_WALKRUN )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_PITCHMIN_CROUCH )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_PITCHMAX_CROUCH )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_PITCHMIN_CROUCHWALK )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_PITCHMAX_CROUCHWALK )
	
	REGISTER_ANIMTAG( ANIMTAG_FLASHBANG_PASSABLE )

	REGISTER_ANIMTAG( ANIMTAG_WEAPON_POSTLAYER )
};

int IndexFromAnimTagName( const char* szName )
{
	int i;
	for ( i=1; i<ANIMTAG_COUNT; i++ )
	{
		const animtaglookup_t *pAnimTag = &g_AnimTagLookupTable[i];
		if ( !V_strcmp( szName, pAnimTag->szName ) )
		{
			return pAnimTag->nIndex;
		}
	}
	return ANIMTAG_INVALID;
}

float GetFirstSequenceAnimTag( CStudioHdr *pstudiohdr, int sequence, int nDesiredTag, float flStart, float flEnd )
{
	if ( !pstudiohdr || sequence >= pstudiohdr->GetNumSeq() )
		return flStart;

	mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( sequence );
	if (seqdesc.numanimtags == 0 )
		return flStart;

	mstudioanimtag_t *panimtag = nullptr;

	int index;
	for ( index = 0; index < (int)seqdesc.numanimtags; index++ )
	{
		panimtag = seqdesc.pAnimTag(index);

		if ( panimtag->tag == ANIMTAG_INVALID )
			continue;

		if ( panimtag->tag == ANIMTAG_UNINITIALIZED )
		{
			panimtag->tag = IndexFromAnimTagName( panimtag->pszTagName() );
		}

		if ( panimtag->tag == nDesiredTag && panimtag->cycle >= flStart && panimtag->cycle < flEnd )
		{
			return panimtag->cycle;
		}
	}

	return flStart;
}

float GetAnySequenceAnimTag( CStudioHdr *pstudiohdr, int sequence, int nDesiredTag, float flDefault )
{
	if ( !pstudiohdr || sequence >= pstudiohdr->GetNumSeq() )
		return flDefault;

	mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( sequence );
	if (seqdesc.numanimtags == 0 )
		return flDefault;

	mstudioanimtag_t *panimtag = nullptr;

	int index;
	for ( index = 0; index < (int)seqdesc.numanimtags; index++ )
	{
		panimtag = seqdesc.pAnimTag(index);

		if ( panimtag->tag == ANIMTAG_INVALID )
			continue;

		if ( panimtag->tag == ANIMTAG_UNINITIALIZED )
		{
			panimtag->tag = IndexFromAnimTagName( panimtag->pszTagName() );
		}

		if ( panimtag->tag == nDesiredTag )
		{
			return panimtag->cycle;
		}
	}

	return flDefault;
}

int GetAnimationEvent( CStudioHdr *pstudiohdr, int sequence, animevent_t *pNPCEvent, float flStart, float flEnd, int index )
{
	if ( !pstudiohdr || sequence >= pstudiohdr->GetNumSeq() || !pNPCEvent )
		return 0;

	mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( sequence );
	if (seqdesc.numevents == 0 || index >= (int)seqdesc.numevents )
		return 0;

	// Msg( "flStart %f flEnd %f (%d) %s\n", flStart, flEnd, seqdesc.numevents, seqdesc.label );
	mstudioevent_for_client_server_t *pevent = GetEventIndexForSequence( seqdesc );
	for (; index < (int)seqdesc.numevents; index++)
	{
		// Don't send client-side events to the server AI
		if ( pevent[index].type & AE_TYPE_NEWEVENTSYSTEM )
		{
			if ( !(pevent[index].type & AE_TYPE_SERVER) )
				 continue;
		}
		else if ( pevent[index].Event_OldSystem() >= EVENT_CLIENT ) //Adrian - Support the old event system
			continue;
	
		bool bOverlapEvent = false;

		if (pevent[index].cycle >= flStart && pevent[index].cycle < flEnd)
		{
			bOverlapEvent = true;
		}
		// FIXME: doesn't work with animations being played in reverse
		else if ((seqdesc.flags & STUDIO_LOOPING) && flEnd < flStart)
		{
			if (pevent[index].cycle >= flStart || pevent[index].cycle < flEnd)
			{
				bOverlapEvent = true;
			}
		}

		if (bOverlapEvent)
		{
			pNPCEvent->pSource = nullptr;
			pNPCEvent->cycle = pevent[index].cycle;
			pNPCEvent->eventtime = gpGlobals->curtime;
			pNPCEvent->type	= pevent[index].type;
			pNPCEvent->Event( pevent[index].Event() );
			pNPCEvent->options = pevent[index].pszOptions();
			return index + 1;
		}
	}
	return 0;
}

int FindTransitionSequence( CStudioHdr *pstudiohdr, int iCurrentSequence, int iGoalSequence, int *piDir )
{
	if ( !pstudiohdr )
		return iGoalSequence;

	if ( !pstudiohdr->SequencesAvailable() )
		return iGoalSequence;

	if ( ( iCurrentSequence < 0 ) || ( iCurrentSequence >= pstudiohdr->GetNumSeq() ) )
		return iGoalSequence;

	if ( ( iGoalSequence < 0 ) || ( iGoalSequence >= pstudiohdr->GetNumSeq() ) )
	{
		// asking for a bogus sequence.  Punt.
		contract_assert( 0 );
		return iGoalSequence;
	}

	// bail if we're going to or from a node 0
	if (pstudiohdr->EntryNode( iCurrentSequence ) == 0 || pstudiohdr->EntryNode( iGoalSequence ) == 0)
	{
		*piDir = 1;
		return iGoalSequence;
	}

	int	iEndNode;

	// Msg( "from %d to %d: ", pEndNode->iEndNode, pGoalNode->iStartNode );

	// check to see if we should be going forward or backward through the graph
	if (*piDir > 0)
	{
		iEndNode = pstudiohdr->ExitNode( iCurrentSequence );
	}
	else
	{
		iEndNode = pstudiohdr->EntryNode( iCurrentSequence );
	}

	// if both sequences are on the same node, just go there
	if (iEndNode == pstudiohdr->EntryNode( iGoalSequence ))
	{
		*piDir = 1;
		return iGoalSequence;
	}

	int iInternNode = pstudiohdr->GetTransition( iEndNode, pstudiohdr->EntryNode( iGoalSequence ) );

	// if there is no transitionial node, just go to the goal sequence
	if (iInternNode == 0)
		return iGoalSequence;

	int i;

	// look for someone going from the entry node to next node it should hit
	// this may be the goal sequences node or an intermediate node
	for (i = 0; i < pstudiohdr->GetNumSeq(); i++)
	{
		mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc(i );
		if (pstudiohdr->EntryNode( i ) == iEndNode && pstudiohdr->ExitNode( i ) == iInternNode)
		{
			*piDir = 1;
			return i;
		}
		if (seqdesc.nodeflags)
		{
			if (pstudiohdr->ExitNode( i ) == iEndNode && pstudiohdr->EntryNode( i ) == iInternNode)
			{
				*piDir = -1;
				return i;
			}
		}
	}

	// this means that two parts of the node graph are not connected.
	DevMsg( 2, "error in transition graph: %s to %s\n",  pstudiohdr->pszNodeName( iEndNode ), pstudiohdr->pszNodeName( pstudiohdr->EntryNode( iGoalSequence ) ));
	// Go ahead and jump to the goal sequence
	return iGoalSequence;
}

bool GotoSequence( CStudioHdr *pstudiohdr, int iCurrentSequence, float flCurrentCycle, float flCurrentRate, int iGoalSequence, int &nNextSequence, float &flNextCycle, int &iNextDir )
{
	if ( !pstudiohdr )
		return false;

	if ( !pstudiohdr->SequencesAvailable() )
		return false;

	if ( ( iCurrentSequence < 0 ) || ( iCurrentSequence >= pstudiohdr->GetNumSeq() ) )
		return false;

	if ( ( iGoalSequence < 0 ) || ( iGoalSequence >= pstudiohdr->GetNumSeq() ) )
	{
		// asking for a bogus sequence.  Punt.
		contract_assert( 0 );
		return false;
	}

	// bail if we're going to or from a node 0
	if (pstudiohdr->EntryNode( iCurrentSequence ) == 0 || pstudiohdr->EntryNode( iGoalSequence ) == 0)
	{
		iNextDir = 1;
		flNextCycle = 0.0;
		nNextSequence = iGoalSequence;
		return true;
	}

	int	iEndNode = pstudiohdr->ExitNode( iCurrentSequence );
	// Msg( "from %d to %d: ", pEndNode->iEndNode, pGoalNode->iStartNode );

	// if we're in a transition sequence
	if (pstudiohdr->EntryNode( iCurrentSequence ) != pstudiohdr->ExitNode( iCurrentSequence ))
	{
		// are we done with it?
		if (flCurrentRate > 0.0 && flCurrentCycle >= 0.999)
		{
			iEndNode = pstudiohdr->ExitNode( iCurrentSequence );
		}
		else if (flCurrentRate < 0.0 && flCurrentCycle <= 0.001)
		{
			iEndNode = pstudiohdr->EntryNode( iCurrentSequence );
		}
		else
		{
			// nope, exit
			return false;
		}
	}

	// if both sequences are on the same node, just go there
	if (iEndNode == pstudiohdr->EntryNode( iGoalSequence ))
	{
		iNextDir = 1;
		flNextCycle = 0.0;
		nNextSequence = iGoalSequence;
		return true;
	}

	int iInternNode = pstudiohdr->GetTransition( iEndNode, pstudiohdr->EntryNode( iGoalSequence ) );

	// if there is no transitionial node, just go to the goal sequence
	if (iInternNode == 0)
	{
		iNextDir = 1;
		flNextCycle = 0.0;
		nNextSequence = iGoalSequence;
		return true;
	}

	int i;

	// look for someone going from the entry node to next node it should hit
	// this may be the goal sequences node or an intermediate node
	for (i = 0; i < pstudiohdr->GetNumSeq(); i++)
	{
		mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc(i );
		if (pstudiohdr->EntryNode( i ) == iEndNode && pstudiohdr->ExitNode( i ) == iInternNode)
		{
			iNextDir = 1;
			flNextCycle = 0.0;
			nNextSequence = i;
			return true;
		}
		if (seqdesc.nodeflags)
		{
			if (pstudiohdr->ExitNode( i ) == iEndNode && pstudiohdr->EntryNode( i ) == iInternNode)
			{
				iNextDir = -1;
				flNextCycle = 0.999;	
				nNextSequence = i;
				return true;
			}
		}
	}

	// this means that two parts of the node graph are not connected.
	DevMsg( 2, "error in transition graph: %s to %s\n",  pstudiohdr->pszNodeName( iEndNode ), pstudiohdr->pszNodeName( pstudiohdr->EntryNode( iGoalSequence ) ));
	return false;
}

void SetBodygroupPreset( CStudioHdr *pstudiohdr, int& body, char const *szName )
{
	if (!pstudiohdr)
		return;

	for ( int i=0; i<pstudiohdr->GetNumBodyGroupPresets(); i++ )
	{
		const mstudiobodygrouppreset_t *pBodygroupPreset = pstudiohdr->GetBodyGroupPreset( i );
		if ( !V_strcmp( szName, pBodygroupPreset->pszName() ) )
		{

			for ( int j=0; j<pstudiohdr->numbodyparts(); j++ )
			{
				mstudiobodyparts_t *pbodypart = pstudiohdr->pBodypart( j );

				int iMask = (pBodygroupPreset->iMask / pbodypart->base) % pbodypart->nummodels;
				if ( iMask == 1 )
				{
					int iCurrent = (body / pbodypart->base) % pbodypart->nummodels;
					int iValCurrent = (pBodygroupPreset->iValue / pbodypart->base) % pbodypart->nummodels;

					body = (body - (iCurrent * pbodypart->base) + (iValCurrent * pbodypart->base));
				}

			}
			break;
		}
	}
}

void SetBodygroup( CStudioHdr *pstudiohdr, int& body, int iGroup, int iValue )
{
	if (! pstudiohdr)
		return;

	if (iGroup >= pstudiohdr->numbodyparts())
		return;

	mstudiobodyparts_t *pbodypart = pstudiohdr->pBodypart( iGroup );

	if (iValue >= pbodypart->nummodels)
		return;

	int iCurrent = (body / pbodypart->base) % pbodypart->nummodels;

	body = (body - (iCurrent * pbodypart->base) + (iValue * pbodypart->base));
}

int GetBodygroup( CStudioHdr *pstudiohdr, int body, int iGroup )
{
	if (! pstudiohdr)
		return 0;

	if (iGroup >= pstudiohdr->numbodyparts())
		return 0;

	mstudiobodyparts_t *pbodypart = pstudiohdr->pBodypart( iGroup );

	if (pbodypart->nummodels <= 1)
		return 0;

	int iCurrent = (body / pbodypart->base) % pbodypart->nummodels;

	return iCurrent;
}

const char *GetBodygroupName( CStudioHdr *pstudiohdr, int iGroup )
{
	if ( !pstudiohdr)
		return "";

	if (iGroup >= pstudiohdr->numbodyparts())
		return "";

	mstudiobodyparts_t *pbodypart = pstudiohdr->pBodypart( iGroup );
	return pbodypart->pszName();
}

const char *GetBodygroupPartName( CStudioHdr *pstudiohdr, int iGroup, int iPart )
{
	if ( !pstudiohdr)
		return "";

	if (iGroup >= pstudiohdr->numbodyparts())
		return "";

	mstudiobodyparts_t *pbodypart = pstudiohdr->pBodypart( iGroup );
	if ( iPart < 0 && iPart >= pbodypart->nummodels )
		return "";

	return pbodypart->pModel( iPart )->name;
}

int FindBodygroupByName( CStudioHdr *pstudiohdr, const char *name )
{
	if ( !pstudiohdr )
		return -1;

	int group;
	for ( group = 0; group < pstudiohdr->numbodyparts(); group++ )
	{
		mstudiobodyparts_t *pbodypart = pstudiohdr->pBodypart( group );
		if ( !V_strcasecmp( name, pbodypart->pszName() ) )
		{
			return group;
		}
	}

	return -1;
}

int GetBodygroupCount( CStudioHdr *pstudiohdr, int iGroup )
{
	if ( !pstudiohdr )
		return 0;

	if (iGroup >= pstudiohdr->numbodyparts())
		return 0;

	mstudiobodyparts_t *pbodypart = pstudiohdr->pBodypart( iGroup );
	return pbodypart->nummodels;
}

int GetNumBodyGroups( CStudioHdr *pstudiohdr )
{
	if ( !pstudiohdr )
		return 0;

	return pstudiohdr->numbodyparts();
}

int GetSequenceActivity( CStudioHdr *pstudiohdr, int sequence, int *pweight )
{
	if (!pstudiohdr || !pstudiohdr->SequencesAvailable() )
	{
		if (pweight)
			*pweight = 0;
		return 0;
	}

	contract_assert( sequence >= 0 && sequence < pstudiohdr->GetNumSeq() );
	mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( sequence );

	if (!(seqdesc.flags & STUDIO_ACTIVITY))
	{
		SetActivityForSequence( pstudiohdr, sequence );
	}
	if (pweight)
		*pweight = seqdesc.actweight;
	return seqdesc.activity;
}

void GetAttachmentLocalSpace( CStudioHdr *pstudiohdr, int attachIndex, matrix3x4_t &pLocalToWorld )
{
	if ( attachIndex >= 0 )
	{
		const mstudioattachment_t &pAttachment = pstudiohdr->pAttachment(attachIndex);
		MatrixCopy( pAttachment.local, pLocalToWorld );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pstudiohdr - 
//			*name - 
// Output : int
//-----------------------------------------------------------------------------
int FindHitboxSetByName( CStudioHdr *pstudiohdr, const char *name )
{
	if ( !pstudiohdr )
		return -1;

	for ( int i = 0; i < pstudiohdr->numhitboxsets(); i++ )
	{
		mstudiohitboxset_t *set = pstudiohdr->pHitboxSet( i );
		if ( !set )
			continue;

		if ( !stricmp( set->pszName(), name ) )
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pstudiohdr - 
//			setnumber - 
// Output : char const
//-----------------------------------------------------------------------------
const char *GetHitboxSetName( CStudioHdr *pstudiohdr, int setnumber )
{
	if ( !pstudiohdr )
		return "";

	mstudiohitboxset_t *set = pstudiohdr->pHitboxSet( setnumber );
	if ( !set )
		return "";

	return set->pszName();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pstudiohdr - 
// Output : int
//-----------------------------------------------------------------------------
int GetHitboxSetCount( CStudioHdr *pstudiohdr )
{
	if ( !pstudiohdr )
		return 0;

	return pstudiohdr->numhitboxsets();
}

#undef iabs
#undef REGISTER_ANIMTAG
