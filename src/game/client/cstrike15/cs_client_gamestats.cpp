//-------------------------------------------------------------
// File:		cs_client_gamestats.cpp
// Desc: 		Manages client side stat storage, accumulation, and access
// Author: 		Peter Freese <peter@hiddenpath.com>
// Date: 		2009/09/11
// Copyright:	(c) 2009 Hidden Path Entertainment
//
// Keywords: 	
//-------------------------------------------------------------

#include "rocketui/rmlui.h"
import ks.tier1;
import std;
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "cs_client_gamestats.h"
#include "achievementmgr.h"
#include "usermessages.h"
#include "c_cs_player.h"
#include "achievements_cs.h"
#include "localize/ilocalize.h"
#include "c_team.h"
#include "engineinterface.h"
#include "cs_gamerules.h"
#include "cs_player_rank_mgr.h"



CCSClientGameStats g_CSClientGameStats;

bool MsgFunc_PlayerStatsUpdate( const ks::net::CCSUsrMsg_PlayerStatsUpdate &msg )
{
	return g_CSClientGameStats.MsgFunc_PlayerStatsUpdate(msg);
}




//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCSClientGameStats::CCSClientGameStats() 
{
}

//-----------------------------------------------------------------------------
// Purpose: called at init time after all systems are init'd.
//-----------------------------------------------------------------------------
void CCSClientGameStats::PostInit()
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );

	// listen for events
	ListenForGameEvent( "round_end_upload_stats" );
	ListenForGameEvent( "round_end" );
	ListenForGameEvent( "cs_game_disconnected" );
	ListenForGameEvent( "read_game_titledata" );
	ListenForGameEvent( "write_game_titledata" );
	ListenForGameEvent( "reset_game_titledata" );
	ListenForGameEvent( "update_matchmaking_stats" );
	
	// Client info messages
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		m_UMCMsgPlayerStatsUpdate.Bind< ks::net::CS_UM_PlayerStatsUpdate, ks::net::CCSUsrMsg_PlayerStatsUpdate>( ::MsgFunc_PlayerStatsUpdate);
	}
}

//-----------------------------------------------------------------------------
// Purpose: called at level shutdown
//-----------------------------------------------------------------------------
void CCSClientGameStats::LevelShutdownPreEntity()
{
	ResetRoundStats();

	// This is a good opportunity to update our last match stats
	UpdateLastMatchStats();
}

static inline int GetNumPlayers( C_Team *pTeam )
{
	return pTeam ? pTeam->Get_Number_Players() : 0;
}

//-----------------------------------------------------------------------------
// Purpose: called when the stats have changed in-game
//-----------------------------------------------------------------------------
void CCSClientGameStats::FireGameEvent( IGameEvent *event )
{
	const char *pEventName = event->GetName();
	if ( 0 == V_strcmp( pEventName, "read_game_titledata" ) )
	{
		SyncCSStatsToTitleData( event->GetInt( "controllerId" ), CSSTAT_READ_STAT );
		SyncCSLoadoutsToTitleData( event->GetInt( "controllerId" ), CSSTAT_READ_STAT );
		SyncCSMatchmakingDataToTitleData( event->GetInt( "controllerId" ), CSSTAT_READ_STAT );
		SyncCSRankingDataToTitleData( event->GetInt( "controllerId" ), CSSTAT_READ_STAT );
	}
	else if ( 0 == V_strcmp( pEventName, "write_game_titledata" ) )
	{
		SyncCSStatsToTitleData( event->GetInt( "controllerId" ), CSSTAT_WRITE_STAT );
		SyncCSLoadoutsToTitleData( event->GetInt( "controllerId" ), CSSTAT_WRITE_STAT );	
		SyncCSMatchmakingDataToTitleData( event->GetInt( "controllerId" ), CSSTAT_WRITE_STAT );
		SyncCSRankingDataToTitleData( event->GetInt( "controllerId" ), CSSTAT_WRITE_STAT );
	}
	else if ( 0 == V_strcmp( pEventName, "reset_game_titledata" ) )
	{
		// $TODO(hpe) need to get controllerID and use it when stats handle splitscreen and loadouts handle splitscreen
		ResetMatchmakingData( MMDATA_SCOPE_LIFETIME );
		ResetMatchmakingData( MMDATA_SCOPE_ROUND );

		// clear stats
		int userSlot = XBX_GetSlotByUserId( event->GetInt( "controllerId" ) );
		if ( userSlot < 0 || userSlot >= MAX_SPLITSCREEN_PLAYERS )
		{
			contract_assert( false && "CCSClientGameStats::FireGameEvent:: reset_game_titledata  invalid userSlot\n" );
			userSlot = SINGLE_PLAYER_SLOT;
		}
		g_CSClientGameStats.ResetAllStats( userSlot );

	}
	else if ( V_strcmp( pEventName, "update_matchmaking_stats" ) == 0 )
	{
		UpdateMatchmakingData();
	}
	else if ( V_strcmp( pEventName, "round_end_upload_stats" ) == 0 )
	{
		ResetRoundStats();
	}
	else if ( V_strcmp( pEventName, "round_end" ) == 0 )
	{
		int iCurrentPlayerCount = event->GetInt( "player_count", 0 );
		m_matchMaxPlayerCount = Max( m_matchMaxPlayerCount, iCurrentPlayerCount );
	}
	else if ( 0 == V_strcmp( pEventName, "cs_game_disconnected" ) )
	{
		ResetRoundStats();
	}
}

int CCSClientGameStats::GetStatCount()
{
	return CSSTAT_MAX;
}

PlayerStatData_t CCSClientGameStats::GetStatById( int id, int nUserSlot )
{
	contract_assert( id >= 0 && id < CSSTAT_MAX );
	if ( id >= 0 && id < CSSTAT_MAX)
	{
		PlayerStatData_t statData;

		statData.iStatId = id;
		statData.iStatValue = m_lifetimeStats[nUserSlot][statData.iStatId];

		// we can make this more efficient by caching the localized names
		statData.pStatDisplayName = g_pLocalize->Find( CSStatProperty_Table[id].szLocalizationToken );

		return statData;
	}
	else
	{
		PlayerStatData_t dummy;
		dummy.pStatDisplayName = nullptr;
		dummy.iStatId = CSSTAT_UNDEFINED;
		dummy.iStatValue = 0;
		return dummy;
	}
}

const StatsCollection_t& CCSClientGameStats::GetLifetimeStats( int nUserSlot ) 
{ 
	if ( nUserSlot < 0 || nUserSlot >= MAX_SPLITSCREEN_PLAYERS )
	{
		contract_assert( false && "CCSClientGameStats::GetLifetimeStats nUserSlot out of range; using 0\n" );
		return m_lifetimeStats[SINGLE_PLAYER_SLOT]; 
	}
	return m_lifetimeStats[nUserSlot]; 
}

const StatsCollection_t& CCSClientGameStats::GetMatchStats( int nUserSlot ) 
{ 
	if ( nUserSlot < 0 || nUserSlot >= MAX_SPLITSCREEN_PLAYERS )
	{
		contract_assert( false && "CCSClientGameStats::GetMatchStats nUserSlot out of range; using 0\n" );
		return m_matchStats[SINGLE_PLAYER_SLOT]; 
	}
	return m_matchStats[nUserSlot]; 
}

const StatsCollection_t& CCSClientGameStats::GetRoundStats( int nUserSlot ) 
{ 
	if ( nUserSlot < 0 || nUserSlot >= MAX_SPLITSCREEN_PLAYERS )
	{
		contract_assert( false && "CCSClientGameStats::GetRoundStats nUserSlot out of range; using 0\n" );
		return m_roundStats[SINGLE_PLAYER_SLOT]; 
	}
	return m_roundStats[nUserSlot]; 
}

void CCSClientGameStats::UpdateStats( const StatsCollection_t &stats, int nUserSlot )
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	// don't count stats if cheats on, commentary mode, etc
	if ( !g_AchievementMgrCS.CheckAchievementsEnabled() )
		return;

	// Update matchmaking related stats.
	IncrementMatchmakingData( stats );

	// Update the accumulated stats
	// We don't aggregate stats in Offline Games with "Dumb" or No Bots
	if ( CSGameRules() && CSGameRules()->IsAwardsProgressAllowedForBotDifficulty() )
	{
		m_lifetimeStats[nUserSlot].Aggregate(stats);
		m_matchStats[nUserSlot].Aggregate(stats);
		m_roundStats[nUserSlot].Aggregate(stats);
	}
	
	// $TODO: hpe: sb: pass along the userSlot in the player_stats_updated message
	IGameEvent * event = gameeventmanager->CreateEvent( "player_stats_updated" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}
}

void CCSClientGameStats::ResetAllStats( int nUSerSlot )
{
	// need to pass along user slot reset into the player_stats_updated message
	int userSlot = nUSerSlot;
	if ( userSlot < 0 || userSlot >= MAX_SPLITSCREEN_PLAYERS )
	{
		contract_assert( false && "CCSClientGameStats::ResetAllStats invalid userSlot\n" );
		userSlot = SINGLE_PLAYER_SLOT;
	}
	m_lifetimeStats[userSlot].Reset();
	m_matchStats[userSlot].Reset();

	m_roundStats[userSlot].Reset();

	IGameEvent * event = gameeventmanager->CreateEvent( "player_stats_updated" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}
}

void CRC32Helper_ProcessInt16( CRC32_t &crc, int16 n )
{
	int16 plat_n = ( n );
	CRC32_ProcessBuffer( &crc, &plat_n, sizeof(plat_n) );
}


void CRC32Helper_ProcessInt32( CRC32_t &crc, int32 n )
{
	int32 plat_n = ( n );
	CRC32_ProcessBuffer( &crc, &plat_n, sizeof(plat_n) );
}


void CRC32Helper_ProcessUInt32( CRC32_t &crc, uint32 n )
{
	uint32 plat_n = ( n );
	CRC32_ProcessBuffer( &crc, &plat_n, sizeof(plat_n) );
}


bool CCSClientGameStats::MsgFunc_PlayerStatsUpdate( const ks::net::CCSUsrMsg_PlayerStatsUpdate &msg )
{
	// Note: if any check fails while decoding this message, bail out and disregard this data to avoid 
	// potentially polluting player stats 

	StatsCollection_t deltaStats;

	CRC32_t crc;
	CRC32_Init( &crc );

	const uint32 key = 0x82DA9F4C;	// this key should match the key in cs_gamestats.cpp
	
	CRC32Helper_ProcessUInt32( crc, key );

	const byte version = 0x03;
	CRC32_ProcessBuffer( &crc, &version, sizeof(version));

	if (msg.version != version)
	{
		Warning("PlayerStatsUpdate message: ignoring unsupported version\n");
		return true;
	}

	short iStatsToRead = msg.stats.size();
	CRC32Helper_ProcessInt16( crc, iStatsToRead );

	for ( int i = 0; i < iStatsToRead; ++i)
	{
		const ks::net::CCSUsrMsg_PlayerStatsUpdate::Stat &stat = msg.stats[ i ];

		short iStat = stat.idx;
		CRC32Helper_ProcessInt16( crc, iStat );

		if (iStat >= CSSTAT_MAX)
		{
			Warning("PlayerStatsUpdate: invalid statId encountered; ignoring stats update\n");
			return true;
		}
		short delta = stat.delta;
		deltaStats[iStat] = delta;
		CRC32Helper_ProcessInt16( crc, delta );
	}

	int userID = msg.user_id;
	CRC32Helper_ProcessInt32( crc, userID );
	
	CRC32_Final( &crc );
	CRC32_t readCRC = msg.crc;

	if ( readCRC != crc )
	{
		Warning("PlayerStatsUpdate message from server is corrupt; ignoring\n");
		return true;
	}

	// do one additional pass for out of band values
	for ( int iStat = CSSTAT_FIRST; iStat < CSSTAT_MAX; ++iStat )
	{
		if (deltaStats[iStat] < 0 || deltaStats[iStat] >= 0x4000)
		{
			Warning("PlayerStatsUpdate message from server has out of band values; ignoring\n");
			return true;
		}
	}

	// everything looks okay at this point; add these stats for the player's round, match, and lifetime stats
	int userSlot = SINGLE_PLAYER_SLOT;


	UpdateStats(deltaStats, userSlot );

	return true;
}

void CCSClientGameStats::ResetMatchStats()
{
	for ( int userSlot = 0; userSlot < MAX_SPLITSCREEN_PLAYERS; ++userSlot)
	{
		m_matchStats[userSlot].Reset();
	}
	m_matchMaxPlayerCount = 0;
}

void CCSClientGameStats::ResetRoundStats( void )
{
	for ( int userSlot = 0; userSlot < MAX_SPLITSCREEN_PLAYERS; ++userSlot)
	{
		m_roundStats[userSlot].Reset();
	}
}

// note, since we now reset the stats after we update them each time, this can be called multiple times without overwriting match stats
void CCSClientGameStats::UpdateLastMatchStats( void )
{
	for ( int userSlot = 0; userSlot < MAX_SPLITSCREEN_PLAYERS; ++userSlot )
	{
		// only update that last match if we actually have valid data
		if ( m_matchStats[userSlot][CSSTAT_ROUNDS_PLAYED] == 0 )
			return;

		// check to see if the player materially participate; they could have been spectating or joined just in time for the ending.
		int s = 0;
		s += m_matchStats[userSlot][CSSTAT_ROUNDS_WON];
		s += m_matchStats[userSlot][CSSTAT_KILLS];
		s += m_matchStats[userSlot][CSSTAT_DEATHS];
		s += m_matchStats[userSlot][CSSTAT_MVPS];
		s += m_matchStats[userSlot][CSSTAT_DAMAGE];
		s += m_matchStats[userSlot][CSSTAT_MONEY_SPENT];

		if( s != 0 && CSGameRules() && CSGameRules()->IsAwardsProgressAllowedForBotDifficulty() )
		{
			m_lifetimeStats[userSlot][CSSTAT_LASTMATCH_CONTRIBUTION_SCORE]	= m_matchStats[userSlot][CSSTAT_CONTRIBUTION_SCORE];
			m_lifetimeStats[userSlot][CSSTAT_LASTMATCH_GG_PROGRESSIVE_CONTRIBUTION_SCORE] = m_matchStats[userSlot][CSSTAT_GG_PROGRESSIVE_CONTRIBUTION_SCORE];
			m_lifetimeStats[userSlot][CSSTAT_LASTMATCH_T_ROUNDS_WON]	= m_matchStats[userSlot][CSSTAT_T_ROUNDS_WON];
			m_lifetimeStats[userSlot][CSSTAT_LASTMATCH_CT_ROUNDS_WON]	= m_matchStats[userSlot][CSSTAT_CT_ROUNDS_WON];
			m_lifetimeStats[userSlot][CSSTAT_LASTMATCH_ROUNDS_WON]	= m_matchStats[userSlot][CSSTAT_ROUNDS_WON];
			m_lifetimeStats[userSlot][CSTAT_LASTMATCH_ROUNDS_PLAYED]	= m_matchStats[userSlot][CSSTAT_ROUNDS_PLAYED];
			m_lifetimeStats[userSlot][CSSTAT_LASTMATCH_KILLS]			= m_matchStats[userSlot][CSSTAT_KILLS];
			m_lifetimeStats[userSlot][CSSTAT_LASTMATCH_DEATHS]		= m_matchStats[userSlot][CSSTAT_DEATHS];
			m_lifetimeStats[userSlot][CSSTAT_LASTMATCH_MVPS]			= m_matchStats[userSlot][CSSTAT_MVPS];
			m_lifetimeStats[userSlot][CSSTAT_LASTMATCH_DAMAGE]		= m_matchStats[userSlot][CSSTAT_DAMAGE];
			m_lifetimeStats[userSlot][CSSTAT_LASTMATCH_MONEYSPENT]	= m_matchStats[userSlot][CSSTAT_MONEY_SPENT];
			m_lifetimeStats[userSlot][CSSTAT_LASTMATCH_DOMINATIONS]	= m_matchStats[userSlot][CSSTAT_DOMINATIONS];
			m_lifetimeStats[userSlot][CSSTAT_LASTMATCH_REVENGES]		= m_matchStats[userSlot][CSSTAT_REVENGES];
			m_lifetimeStats[userSlot][CSSTAT_LASTMATCH_MAX_PLAYERS]	= m_matchMaxPlayerCount;
			CalculateMatchFavoriteWeapons();
		}
	}
	ResetMatchStats();
}

//-----------------------------------------------------------------------------
// Purpose: Calculate and store the match favorite weapon for each player
//-----------------------------------------------------------------------------
void CCSClientGameStats::CalculateMatchFavoriteWeapons()
{
	for ( int userSlot = 0; userSlot < MAX_SPLITSCREEN_PLAYERS; ++userSlot )
	{
		int maxKills = 0, maxKillId = -1;

		for( int j = CSSTAT_KILLS_DEAGLE; j <= CSSTAT_KILLS_M249; ++j )
		{
			if ( m_matchStats[userSlot][j] > maxKills )
			{
				maxKills = m_matchStats[userSlot][j];
				maxKillId = j;
			}
		}
		if ( maxKillId == -1 )
		{
			m_lifetimeStats[userSlot][CSSTAT_LASTMATCH_FAVWEAPON_ID] = WEAPON_NONE;
			m_lifetimeStats[userSlot][CSSTAT_LASTMATCH_FAVWEAPON_SHOTS] = 0;
			m_lifetimeStats[userSlot][CSSTAT_LASTMATCH_FAVWEAPON_HITS] = 0;
			m_lifetimeStats[userSlot][CSSTAT_LASTMATCH_FAVWEAPON_KILLS] = 0;
		}
		else
		{
			int statTableID = -1;
			for (int j = 0; WeaponName_StatId_Table[j].killStatId != CSSTAT_UNDEFINED; ++j)
			{
				if ( WeaponName_StatId_Table[j].killStatId == maxKillId )
				{
					statTableID = j;
					break;
				}
			}
			contract_assert( statTableID != -1 );

			m_lifetimeStats[userSlot][CSSTAT_LASTMATCH_FAVWEAPON_ID] = WeaponName_StatId_Table[statTableID].weaponId;
			m_lifetimeStats[userSlot][CSSTAT_LASTMATCH_FAVWEAPON_SHOTS] = m_matchStats[userSlot][WeaponName_StatId_Table[statTableID].shotStatId];
			m_lifetimeStats[userSlot][CSSTAT_LASTMATCH_FAVWEAPON_HITS] = m_matchStats[userSlot][WeaponName_StatId_Table[statTableID].hitStatId];
			m_lifetimeStats[userSlot][CSSTAT_LASTMATCH_FAVWEAPON_KILLS] = m_matchStats[userSlot][WeaponName_StatId_Table[statTableID].killStatId];
		}
	}
}


bool CCSClientGameStats::ValidateTitleBlockVersion( TitleDataFieldsDescription_t const *pFields, IPlayerLocal *pPlayerLocal, CSSyncStatValueDirection_t eOp, int titleBlockNo )
{
	return false;		// no title data for non Xbox systems

}


//-----------------------------------------------------------------------------
// Purpose: Serialize lifetime stats to the user profile title data
//-----------------------------------------------------------------------------
bool CCSClientGameStats::SyncCSStatsToTitleData( int iController, CSSyncStatValueDirection_t eOp )
{

	return true;

}


bool CCSClientGameStats::SyncCSLoadoutsToTitleData( int iController, CSSyncStatValueDirection_t eOp )
{
	// get the local player
	IPlayerLocal *pPlayerLocal = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
	if ( !pPlayerLocal )
		return false;

	return true;
}

bool CCSClientGameStats::SyncCSMatchmakingDataToTitleData( int iController, CSSyncStatValueDirection_t eOp )
{
	// Get the local player.
	IPlayerLocal *pPlayerLocal = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
	if ( !pPlayerLocal )
		return false;

	return true;
}



bool CCSClientGameStats::SyncCSRankingDataToTitleData( int iController, CSSyncStatValueDirection_t eOp )
{
	// Get the local player.
	IPlayerLocal *pPlayerLocal = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
	if ( !pPlayerLocal )
		return false;


	return true;
}

// Increment the current round matchmaking data by the data in the given stats structure.
// This method may be called multiple times per round so we have to deal with rounds played
// differently.  Each call to the method is the delta from the previous call.
void CCSClientGameStats::IncrementMatchmakingData( const StatsCollection_t &stats )
{
}

// Get the matchmaking data for current primary user and compute the new rolling average based
// on the data accumulated so far.
void CCSClientGameStats::UpdateMatchmakingData( void )
{
}

// Reset the matchmaking data for the current primary user for the given scope.
void CCSClientGameStats::ResetMatchmakingData( MatchmakingDataScope mmDataScope )
{
}
