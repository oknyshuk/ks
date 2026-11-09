// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
#if defined(CLIENT_DLL)
#include "string_t.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "npcevent.h"
#include "debugoverlay_shared.h"
#elif defined(GAME_DLL)
#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "soundflags.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "player.h"
#endif
#include "achievementmgr.h"
#include "filesystem.h"
#include "usermessages.h"
#ifdef CLIENT_DLL
#include "c_playerresource.h"
#include "c_cs_player.h"
#else
#include "enginecallback.h"
#endif // CLIENT_DLL

#include "cs_gamerules.h"


#include "matchmaking/imatchframework.h"
#include "cs_weapon_parse.h"
#include "achievements_cs.h"

ConVar	cc_achievement_debug("achievement_debug", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "Turn on achievement debug msgs." );

// We want to debug getting achievements in various development scenarios (including with cheats enabled). This convar allows bypassing
// the normal check against the cheats when launched with -dev. We need to be very careful not to ship with this enabled on any platforms.
// #if defined(_CERT)
// #else
// #define ALLOW_ACHIEVEMENTS_WITH_CHEATS 1
// #endif

extern ConVar developer;

#if defined( XBX_GetPrimaryUserId )
#undef XBX_GetPrimaryUserId
#endif

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

#ifdef _DEBUG
	// There can be only one achievement manager instance; no one else should be registered
	IAchievementMgr *pAchievementMgr = engine->GetAchievementMgr();
	contract_assert( nullptr == pAchievementMgr );
#endif // _DEBUG

	// register ourselves
	engine->SetAchievementMgr( GetInstanceInterface() );

	// register for events
#ifdef GAME_DLL
	ListenForGameEvent( "entity_killed" );
	ListenForGameEvent( "game_init" );
#else
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
#endif // CLIENT_DLL


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

#ifdef CLIENT_DLL
extern const ConVar *sv_cheats;
#endif

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

#	ifdef GAME_DLL
		// For single-player games, achievement mgr must live on the server.  (Only the server has detailed knowledge of game state.)
		contract_assert( !GameRules()->IsMultiplayer() );
#	else
		// For multiplayer games, achievement mgr must live on the client.  (Only the client can read/write player state.)
		contract_assert( GameRules()->IsMultiplayer() );
#	endif

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
#ifdef CLIENT_DLL	
	V_FileBase( engine->GetLevelName(), m_szMap, std::size( m_szMap ) );
#else
	V_strncpy( m_szMap, gpGlobals->mapname.ToCStr(), std::size( m_szMap ) );
#endif // CLIENT_DLL

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
#ifdef CLIENT_DLL
	m_bUserSlotActive[SINGLE_PLAYER_SLOT] = true;
#endif // CLIENT_DLL
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

#ifdef CLIENT_DLL
	C_BasePlayer *pPlayerLocal = C_BasePlayer::GetLocalPlayer();
	C_CSPlayer* pPlayer = ToCSPlayer(pPlayerLocal);

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
#endif // CLIENT_DLL
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

ConVar	cc_achievement_disable("achievement_disable", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "Turn off achievements." );

//-----------------------------------------------------------------------------
// Purpose: checks if achievements are enabled
//-----------------------------------------------------------------------------
bool CAchievementMgr::CheckAchievementsEnabled( )
{
	if ( cc_achievement_disable.GetBool() )
		return false;

	//No achievements in demo version.


#ifdef CLIENT_DLL
	// achievements disabled if playing demo (Playback demo) or watching HLTV
	if ( engine->IsHLTV() )
	{
		Msg( "Achievements disabled: demo playing.\n" );
		return false;
	}
#endif // CLIENT_DLL

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

#ifdef CLIENT_DLL

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

#endif // CLIENT_DLL


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
#ifdef CLIENT_DLL
	for ( const auto &[nID, pEntry] : m_mapAchievement[SINGLE_PLAYER_SLOT] )
	{
		CBaseAchievement *pAchievement = pEntry.get();
		ResetAchievement_Internal( pAchievement );
	}
	SaveGlobalState();
#endif // CLIENT_DLL
}

void CAchievementMgr::ResetAchievement( int iAchievementID )
{
#ifdef CLIENT_DLL
	CBaseAchievement *pAchievement = GetAchievementByID( iAchievementID, SINGLE_PLAYER_SLOT );
	contract_assert( pAchievement );
	if ( pAchievement )
	{
		ResetAchievement_Internal( pAchievement );
		SaveGlobalState();
	}
#endif // CLIENT_DLL
}

//-----------------------------------------------------------------------------
// Purpose: Resets all achievements.  For debugging purposes only
//-----------------------------------------------------------------------------
void CAchievementMgr::PrintAchievementStatus()
{
#if defined( CLIENT_DLL )
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
#endif
}

//-----------------------------------------------------------------------------
// Purpose: called when a game event is fired
//-----------------------------------------------------------------------------
void CAchievementMgr::FireGameEvent( IGameEvent *event )
{
#ifdef CLIENT_DLL
	int nSplitScreenPlayer = event->GetInt( "splitscreenplayer" );
	contract_assert( nSplitScreenPlayer < MAX_SPLITSCREEN_PLAYERS );

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSplitScreenPlayer );
#endif

	CVProfScope _( "CAchievementMgr::FireGameEvent", 1, "Achievements", false, 0 );
	const char *name = event->GetName();
	if ( 0 == V_strcmp( name, "entity_killed" ) )
	{
#ifdef GAME_DLL
		CBaseEntity *pVictim = UTIL_EntityByIndex( event->GetInt( "entindex_killed", 0 ) );
		CBaseEntity *pAttacker = UTIL_EntityByIndex( event->GetInt( "entindex_attacker", 0 ) );
		CBaseEntity *pInflictor = UTIL_EntityByIndex( event->GetInt( "entindex_inflictor", 0 ) );
		OnKillEvent( pVictim, pAttacker, pInflictor, event );
#endif // GAME_DLL
	}
	else if ( 0 == V_strcmp( name, "game_init" ) )
	{
#ifdef GAME_DLL
		// clear all state as though we were loading a saved game, but without loading the game
		PreRestoreSavedGame();
		PostRestoreSavedGame();
#endif // GAME_DLL
	}
#ifdef CLIENT_DLL
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
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
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

#endif // CLIENT_DLL
}

//-----------------------------------------------------------------------------
// Purpose: called when a player or character has been killed
//-----------------------------------------------------------------------------
void CAchievementMgr::OnKillEvent( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event )
{
#ifdef CLIENT_DLL
	int nSplitScreenPlayer = SINGLE_PLAYER_SLOT;
	nSplitScreenPlayer = event->GetInt( "splitscreenplayer" );
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSplitScreenPlayer );
#endif

	// can have a NULL victim on client if victim has never entered local player's PVS
	if ( !pVictim )
		return;

	// if single-player game, calculate if the attacker is the local player and if the victim is the player enemy
	bool bAttackerIsPlayer = false;
	bool bVictimIsPlayerEnemy = false;
#ifdef GAME_DLL
	if ( !g_pGameRules->IsMultiplayer() )
	{
		CBasePlayer *pLocalPlayer = UTIL_GetLocalPlayer();
		if ( pLocalPlayer )
		{
			if ( pAttacker == pLocalPlayer )
			{
				bAttackerIsPlayer = true;
			}

			CBaseCombatCharacter *pBCC = ToBaseCombatCharacter( pVictim );
			if ( pBCC && ( D_HT == pBCC->IRelationType( pLocalPlayer ) ) )
			{
				bVictimIsPlayerEnemy = true;
			}
		}
	}
#else
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	bVictimIsPlayerEnemy = !pLocalPlayer->InSameTeam( pVictim );
	if ( pAttacker == pLocalPlayer )
	{
		bAttackerIsPlayer = true;
	}
#endif // GAME_DLL

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

#if GAME_DLL
			// if this achievement only looks for a particular victim class name and this victim is a different class, skip this achievement
			const char *pVictimClassNameFilter = pAchievement->m_pVictimClassNameFilter;
			if ( pVictimClassNameFilter && !pVictim->ClassMatches( pVictimClassNameFilter ) )
				continue;

			// if this achievement only looks for a particular inflictor class name and this inflictor is a different class, skip this achievement
			const char *pInflictorClassNameFilter = pAchievement->m_pInflictorClassNameFilter;
			if ( pInflictorClassNameFilter &&  ( ( nullptr == pInflictor ) || !pInflictor->ClassMatches( pInflictorClassNameFilter ) ) )
				continue;

			// if this achievement only looks for a particular attacker class name and this attacker is a different class, skip this achievement
			const char *pAttackerClassNameFilter = pAchievement->m_pAttackerClassNameFilter;
			if ( pAttackerClassNameFilter && ( ( nullptr == pAttacker ) || !pAttacker->ClassMatches( pAttackerClassNameFilter ) ) )
				continue;

			// if this achievement only looks for a particular inflictor entity name and this inflictor has a different name, skip this achievement
			const char *pInflictorEntityNameFilter = pAchievement->m_pInflictorEntityNameFilter;
			if ( pInflictorEntityNameFilter && ( ( nullptr == pInflictor ) || !pInflictor->NameMatches( pInflictorEntityNameFilter ) ) )
				continue;
#endif // GAME_DLL

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
#if defined( CLIENT_DLL )
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
#endif // CLIENT_DLL
}

#ifdef CLIENT_DLL

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

#endif // CLIENT_DLL

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
#ifdef CLIENT_DLL
	if ( !sv_cheats )
	{
		sv_cheats = cvar->FindVar( "sv_cheats" );
	}
#endif

#ifndef _DEBUG
	// keep track if cheats have ever been turned on during this level
	if ( !WereCheatsEverOn() )
	{
		if ( sv_cheats && sv_cheats->GetBool() )
		{
			m_bCheatsEverOn = true;
		}
	}
#endif

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
