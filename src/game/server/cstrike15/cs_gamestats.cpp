// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2006 Valve Corporation

module;
#include <math.h>
#include <time.h>
#include "edict.h"
#include "filesystem.h"
#include "igameevents.h"
#include "networkvar.h"
#include "tier2/tier2_logging.h"
#include "usermessages_schema.h"
#include "../util.h"
#include "bspfile.h"
#include "declare_class.h"
#include "ihltv.h"
#include "ihltvdirector.h"
#include "../shareddefs.h"
#include "eiface.h"
#include "responserules/response_types.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "../ai_criteria.h"
#include "../entity_dispatch.h"
#include "../entity_dispatch_list.h"
#include "../entity_kinds_dump.h"
#include "reflect.h"
module ks.server;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.platform;
import ks.core.types;
import ks.core.wchartypes;
import ks.game.cstrike15.cs_achievement_constants;
import ks.game.cstrike15.cs_achievementdefs;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.checksum_crc;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.map_ops;
import ks.tier1.playerid;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.strtools;
import :vscript;
import :fish;
import :sharedvar;
import :baseentity;
import :player;
import :cstrike15.bot.bot;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player;
import :cstrike15.cs_playeranimstate;
import :entity_kind;
import :nav_4;
import ks.game.shareddefs;
import :cstrike15.cs_player_2;
import :cstrike15.hostage.cs_simple_hostage;
import :econ.econ_item_schema_minimal;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :entity_2;
import ks.core.vprof;
import ks.tier1.byte_reader;
import ks.tier1.byte_writer;
import ks.tier1.kvpacker;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
import ks.core.random;
import :entity_dispatch;
import ks.core.time;
import ks.math.vector2d;

class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CBreakableProp;
class CCSPlayer;
class CCSWeaponInfo;
class CEconItemView;
class CTakeDamageInfo;
class CWeaponCSBase;

using namespace ::ks::reflect;

// Purpose: CS game stats

// Some tricky business here - we don't want to include the precompiled header for the statreader
// and trying to #ifdef it out does funky things like ignoring the #endif. Define our header file
// separately and include it based on the switch

//#define DEBUG_STAT_TRANSMISSION 

extern ConVar mp_roundtime;

float	g_flGameStatsUpdateTime = 0.0f;
short	g_iTerroristVictories[CS_NUM_LEVELS];
short	g_iCounterTVictories[CS_NUM_LEVELS];
short	g_iWeaponPurchases[WEAPON_MAX];

short	g_iAutoBuyPurchases = 0;
short	g_iReBuyPurchases = 0;
short	g_iAutoBuyM4A1Purchases = 0;
short	g_iAutoBuyAK47Purchases = 0;
short	g_iAutoBuyFamasPurchases = 0;
short	g_iAutoBuyGalilPurchases = 0;
short	g_iAutoBuyGalilARPurchases = 0;
short	g_iAutoBuyVestHelmPurchases = 0;
short	g_iAutoBuyVestPurchases = 0;

struct
{
	const char* szPropModelName;
	CSStatType_t statType;
} PropModelStatsTableInit[] =
{
	{ "models/props/cs_office/computer_caseb.mdl", CSSTAT_PROPSBROKEN_OFFICEELECTRONICS },
	{ "models/props/cs_office/computer_monitor.mdl", CSSTAT_PROPSBROKEN_OFFICEELECTRONICS },
	{ "models/props/cs_office/phone.mdl", CSSTAT_PROPSBROKEN_OFFICEELECTRONICS },
	{ "models/props/cs_office/projector.mdl", CSSTAT_PROPSBROKEN_OFFICEELECTRONICS },
	{ "models/props/cs_office/TV_plasma.mdl", CSSTAT_PROPSBROKEN_OFFICEELECTRONICS },
	{ "models/props/cs_office/computer_keyboard.mdl", CSSTAT_PROPSBROKEN_OFFICEELECTRONICS },
	{ "models/props/cs_office/radio.mdl", CSSTAT_PROPSBROKEN_OFFICERADIO },
	{ "models/props/cs_office/trash_can.mdl", CSSTAT_PROPSBROKEN_OFFICEJUNK },
	{ "models/props/cs_office/file_box.mdl", CSSTAT_PROPSBROKEN_OFFICEJUNK },
	{ "models/props_junk/watermelon01.mdl", CSSTAT_PROPSBROKEN_ITALY_MELON },
	// 	models/props/de_inferno/claypot01.mdl
	// 	models/props/de_inferno/claypot02.mdl
	//	models/props/de_dust/grainbasket01c.mdl
	//	models/props_junk/wood_crate001a.mdl 
	//	models/props/cs_office/file_box_p1.mdl 
};

struct
{
	int achievementId;
	int statId;
	int roundRequirement;
	int matchRequirement;
	const char* mapFilter;
	bool disallowGunGameProgressive;

	bool IsMet(int roundStat, int matchStat)
	{
		return roundStat >= roundRequirement && matchStat >= matchRequirement;
	}
} ServerStatBasedAchievements[] =
{
	{ CSBreakWindows,			    CSSTAT_NUM_BROKEN_WINDOWS,		AchievementConsts::BreakWindowsInOfficeRound_Windows,	0,	"cs_office", false },
	//{ CSBreakProps,			        CSSTAT_PROPSBROKEN_ALL,			AchievementConsts::BreakPropsInRound_Props,				0,	NULL },
	{ CSUnstoppableForce,		    CSSTAT_KILLS,					AchievementConsts::UnstoppableForce_Kills,				0,	nullptr, true },
	{ CSHeadshotsInRound,	        CSSTAT_KILLS_HEADSHOT,			AchievementConsts::HeadshotsInRound_Kills,				0,	nullptr, true },
	{ CSDominationOverkillsMatch,	CSSTAT_DOMINATION_OVERKILLS,	0,				                                        10,	nullptr, false },
};

// [Forrest] Allow nemesis/revenge to be turned off for a server
static void SvNoNemesisChangeCallback( ConVar *pConVar, const char *pOldValue, float flOldValue )
{
	ConVarRef var( pConVar );
	if ( var.IsValid() && var.GetBool() )
	{
		// Clear all nemesis relationships.
		for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
		{
			CCSPlayer *pTemp = ToCSPlayer( UTIL_PlayerByIndex( i ) );
			if ( pTemp )
			{
				pTemp->RemoveNemesisRelationships();
			}
		}
	}
}

ConVar sv_nonemesis( side_registry(), "sv_nonemesis", "0", 0, "Disable nemesis and revenge.", SvNoNemesisChangeCallback );
ConVar sv_dumpmatchweaponmetrics( side_registry(), "sv_dumpmatchweaponmetrics", "0", FCVAR_DEVELOPMENTONLY, "Turn on the exporting of weapon metrics at the end of a level.");

int GetCSLevelIndex( const char *pLevelName )
{
	for ( int i = 0; MapName_StatId_Table[i].statWinsId != CSSTAT_UNDEFINED; i ++ )
	{
		if ( V_strcmp( pLevelName, MapName_StatId_Table[i].szMapName ) == 0 )
			return i;
	}

	return -1;
}

CCSGameStats CCS_GameStats;

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  :  - 
//-----------------------------------------------------------------------------
CCSGameStats::CCSGameStats()
{
	gamestats = this;
	Clear();
	m_fDisseminationTimerLow = m_fDisseminationTimerHigh = 0.0f;

	// create table for mapping prop models to stats
	for ( int i = 0; i < std::size( PropModelStatsTableInit ); ++i)
	{
		m_PropStatTable.emplace(PropModelStatsTableInit[i].szPropModelName, PropModelStatsTableInit[i].statType);
	}

	m_numberOfRoundsForDirectAverages = 0;
	m_numberOfTerroristEntriesForDirectAverages = 0;
	m_numberOfCounterTerroristEntriesForDirectAverages = 0;

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
// Input  :  - 
//-----------------------------------------------------------------------------
CCSGameStats::~CCSGameStats()
{
	Clear();
}

//-----------------------------------------------------------------------------
// Purpose: Clear out game stats
// Input  :  - 
//-----------------------------------------------------------------------------
void CCSGameStats::Clear( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGameStats::Init( void )
{
	ListenForGameEvent( "round_end" );
	ListenForGameEvent( "round_officially_ended" );
	ListenForGameEvent( "break_prop" );
	ListenForGameEvent( "player_decal" );	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_ShotFired( CBasePlayer *pPlayer, CBaseCombatWeapon* pWeapon )
{
	if ( CSGameRules()->IsRoundOver() )
		return;

	contract_assert( pPlayer );
	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );

	// [dwenger] adding tracking for weapon used fun fact
	if ( pCSPlayer && pWeapon )
	{
		// [dwenger] Update the player's tracking of which weapon type they fired
		pCSPlayer->PlayerUsedFirearm( pWeapon );
		if( !pWeapon->HasAnyAmmo() )
			pCSPlayer->PlayerEmptiedAmmoForFirearm( pWeapon );

		IncrementStat( pCSPlayer, CSSTAT_SHOTS_FIRED, 1 );

		CWeaponCSBase* pCSWeapon = dynamic_cast< CWeaponCSBase * >(pWeapon);

		// Increment the individual weapon
	    if( pCSWeapon )
		{
			CSWeaponID weaponId = pCSWeapon->GetCSWeaponID();

			//if ( weaponId == WEAPON_HEGRENADE )
			//	int here = 0;

			for (int i = 0; WeaponName_StatId_Table[i].weaponId != WEAPON_NONE; ++i)
			{
			    if ( WeaponName_StatId_Table[i].weaponId == weaponId && WeaponName_StatId_Table[i].shotStatId != CSSTAT_UNDEFINED )
				{
					IncrementStat( pCSPlayer, WeaponName_StatId_Table[i].shotStatId, 1 );
					break;
				}
			}

			CSWeaponMode weaponMode = pCSWeapon->m_weaponMode;
			++m_weaponStats[weaponId][weaponMode].shots;
		}
	}
}

void CCSGameStats::Event_ShotHit( CBasePlayer *pPlayer, const CTakeDamageInfo &info )
{
	contract_assert( pPlayer );
	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );

	if ( info.GetDamagedOtherPlayers() <=  0 )
	{
		// [dkorus] a 'hit' is a only counted for the first character hit.  Otherwise we can end up with an artificially high hit count and >100% accuracy
		IncrementStat( pCSPlayer, CSSTAT_SHOTS_HIT, 1 );
	}

	CBaseEntity *pInflictor = info.GetInflictor();

	if ( pInflictor )
	{
		if ( pInflictor == pPlayer )
		{			
			if ( pPlayer->GetActiveWeapon() )
			{
				CWeaponCSBase* pCSWeapon = dynamic_cast< CWeaponCSBase * >(pPlayer->GetActiveWeapon());
				if (pCSWeapon)
				{
					CSWeaponID weaponId = pCSWeapon->GetCSWeaponID();
					for (int i = 0; WeaponName_StatId_Table[i].weaponId != WEAPON_NONE; ++i)
					{
						if ( WeaponName_StatId_Table[i].weaponId == weaponId && WeaponName_StatId_Table[i].shotStatId != CSSTAT_UNDEFINED )
						{
							IncrementStat( pCSPlayer, WeaponName_StatId_Table[i].hitStatId, 1 );
							IncrementStat( pCSPlayer, WeaponName_StatId_Table[i].damageStatId, info.GetDamage() );
							break;
						}
					}

					CSWeaponMode weaponMode = pCSWeapon->m_weaponMode;
					++m_weaponStats[weaponId][weaponMode].hits;
					m_weaponStats[weaponId][weaponMode].damage += info.GetDamage();
				}
			}
		}
	}
}
void CCSGameStats::Event_PlayerKilled( CBasePlayer *pPlayer, const CTakeDamageInfo &info )
{
	contract_assert( pPlayer );
	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
	
	IncrementStat( pCSPlayer, CSSTAT_DEATHS, 1 );
}

void CCSGameStats::Event_PlayerSprayedDecal( CCSPlayer* pPlayer )
{
    IncrementStat( pPlayer, CSSTAT_DECAL_SPRAYS, 1 );
}

void CCSGameStats::Event_PlayerKilled_PreWeaponDrop( CBasePlayer *pPlayer, const CTakeDamageInfo &info )
{
	contract_assert( pPlayer );
	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
	CCSPlayer *pAttacker = ToCSPlayer( info.GetAttacker() );
	bool victimZoomed = ( pCSPlayer->GetFOV() != pCSPlayer->GetDefaultFOV() );

	if (victimZoomed)
	{
		IncrementStat(pAttacker, CSSTAT_KILLS_AGAINST_ZOOMED_SNIPER, 1);
	}

	//Check for knife fight achievements
	if (pAttacker && pCSPlayer && pAttacker == info.GetInflictor() && pAttacker->GetTeamNumber() != pCSPlayer->GetTeamNumber())
	{
		CWeaponCSBase* attackerWeapon = pAttacker->GetActiveCSWeapon();
		CWeaponCSBase* victimWeapon = pCSPlayer->GetActiveCSWeapon();

		CSWeaponID victimWeaponID = ( ( victimWeapon ) ? victimWeapon->GetCSWeaponID() : WEAPON_NONE );

		if (attackerWeapon && victimWeapon)
		{
			CSWeaponID attackerWeaponID = attackerWeapon->GetCSWeaponID(); 

			if (attackerWeaponID == WEAPON_KNIFE && victimWeaponID == WEAPON_KNIFE)
			{
				IncrementStat(pAttacker, CSSTAT_KILLS_KNIFE_FIGHT, 1);
			}

			if( CSGameRules( )->IsPlayingGunGame() )
			{
				int nWeapon = CSGameRules()->GetCurrentGunGameWeapon( pAttacker->m_iGunGameProgressiveWeaponIndex, pAttacker->GetTeamNumber() );
				if ( nWeapon == WEAPON_KNIFE && 
					 CSGameRules( )->IsPlayingGunGameTRBomb() )
				{
					// just got a knife kill in a TR game
					if ( pCSPlayer->PlacedBombThisRound() )
						pAttacker->SetKnifeLevelKilledBombPlacer();
				}

				nWeapon = CSGameRules()->GetCurrentGunGameWeapon( pCSPlayer->m_iGunGameProgressiveWeaponIndex, pCSPlayer->GetTeamNumber() );
				if ( nWeapon == WEAPON_KNIFE || nWeapon == WEAPON_KNIFE_GG )
				{
					pAttacker->AwardAchievement( CSGunGameKillKnifer );
					if ( attackerWeaponID == WEAPON_KNIFE || attackerWeaponID == WEAPON_KNIFE_GG )
					{
						pAttacker->AwardAchievement( CSGunGameKnifeKillKnifer );
					}

					if ( attackerWeapon->IsKindOf( WEAPONTYPE_SUBMACHINEGUN ) )
					{
						pAttacker->AwardAchievement( CSGunGameSMGKillKnifer );
					}
				}
			}

		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_BombPlanted( CCSPlayer* pPlayer)
{
    IncrementStat( pPlayer, CSSTAT_NUM_BOMBS_PLANTED, 1 );
	if( CSGameRules( )->IsPlayingGunGameTRBomb() )
	{
		IncrementStat( pPlayer, CSSTAT_TR_NUM_BOMBS_PLANTED, 1 );

	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_BombDefused( CCSPlayer* pPlayer)
{

    IncrementStat( pPlayer, CSSTAT_NUM_BOMBS_DEFUSED, 1 );
	IncrementStat( pPlayer, CSSTAT_OBJECTIVES_COMPLETED, 1 );
	if( pPlayer && pPlayer->HasDefuser() )
	{
		IncrementStat( pPlayer, CSSTAT_BOMBS_DEFUSED_WITHKIT, 1 );
	}

	if( CSGameRules( )->IsPlayingGunGameTRBomb() )
	{
		IncrementStat( pPlayer, CSSTAT_TR_NUM_BOMBS_DEFUSED, 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Increment terrorist team stat
//-----------------------------------------------------------------------------
void CCSGameStats::Event_BombExploded( CCSPlayer* pPlayer )
{
	IncrementStat( pPlayer, CSSTAT_OBJECTIVES_COMPLETED, 1 );
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_HostageRescued( CCSPlayer* pPlayer)
{
    IncrementStat( pPlayer, CSSTAT_NUM_HOSTAGES_RESCUED, 1 );
}

//-----------------------------------------------------------------------------
// Purpose: Increment counter-terrorist team stat
//-----------------------------------------------------------------------------
void CCSGameStats::Event_AllHostagesRescued()
{
	IncrementTeamStat( TEAM_CT, CSSTAT_OBJECTIVES_COMPLETED, 1 );
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_WindowShattered( CBasePlayer *pPlayer)
{
	contract_assert( pPlayer );
	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );

	IncrementStat( pCSPlayer, CSSTAT_NUM_BROKEN_WINDOWS, 1 );
}

void CCSGameStats::Event_BreakProp( CCSPlayer* pPlayer, CBreakableProp *pProp )
{
	if (!pPlayer)
		return;

	DevMsg("Player %s broke a %s (%i)\n", pPlayer->GetPlayerName(), pProp->GetModelName().ToCStr(), pProp->entindex());

	if (CSStatType_t *pStat = ks::find_ptr(m_PropStatTable, pProp->GetModelName().ToCStr()))
	{
		IncrementStat(pPlayer, *pStat, 1);
	}
	IncrementStat(pPlayer, CSSTAT_PROPSBROKEN_ALL, 1);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::UpdatePlayerRoundStats(int winner)
{	
	int mapIndex = GetCSLevelIndex(gpGlobals->mapname.ToCStr());
	CSStatType_t mapStatWinIndex  = CSSTAT_UNDEFINED, mapStatRoundIndex = CSSTAT_UNDEFINED;

	if ( mapIndex != -1 )
	{
		mapStatWinIndex = MapName_StatId_Table[mapIndex].statWinsId;
		mapStatRoundIndex = MapName_StatId_Table[mapIndex].statRoundsId;
	}

	// increment the team specific stats
	IncrementTeamStat( winner, CSSTAT_ROUNDS_WON, 1 );

	if( CSGameRules( )->IsPlayingGunGame() )
	{
		IncrementTeamStat( winner, CSTAT_GUNGAME_ROUNDS_WON, 1 );
		IncrementTeamStat( TEAM_TERRORIST, CSTAT_GUNGAME_ROUNDS_PLAYED, 1 );
		IncrementTeamStat( TEAM_CT, CSTAT_GUNGAME_ROUNDS_PLAYED, 1 );
	}

	if ( mapStatWinIndex != CSSTAT_UNDEFINED )
	{
		IncrementTeamStat( winner, mapStatWinIndex, 1 );
	}
	if ( CSGameRules()->IsPistolRound() )
	{
		IncrementTeamStat( winner, CSSTAT_PISTOLROUNDS_WON, 1 );
	}
	
	IncrementTeamStat( TEAM_TERRORIST, CSSTAT_ROUNDS_PLAYED, 1 );
	IncrementTeamStat( TEAM_CT, CSSTAT_ROUNDS_PLAYED, 1 );

	if( mapStatRoundIndex != CSSTAT_UNDEFINED )
	{
		IncrementTeamStat( TEAM_TERRORIST, mapStatRoundIndex, 1 );
		IncrementTeamStat( TEAM_CT, mapStatRoundIndex, 1 );
	}

	for( int iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iPlayerIndex ) );
		if ( pPlayer && pPlayer->IsConnected() )
		{
			IncrementStat( pPlayer, CSSTAT_ROUNDS_PLAYED, 1, false, true );			
			if ( CSGameRules()->IsPlayingGunGame() )
			{
				IncrementStat( pPlayer, CSTAT_GUNGAME_ROUNDS_PLAYED, 1, false, true );

			}
			if( CSGameRules()->IsPlayingGunGameProgressive() )
			{
				IncrementStat( pPlayer,CSSTAT_GG_PROGRESSIVE_CONTRIBUTION_SCORE, std::max<std::common_type_t<int, float>>( pPlayer->GetRoundContributionScore(), 0.0f), false, true );
			}
			else
			{
				IncrementStat( pPlayer,CSSTAT_CONTRIBUTION_SCORE, std::max<std::common_type_t<int, float>>( pPlayer->GetRoundContributionScore(), 0.0f), false, true );
			}
			pPlayer->ClearRoundContributionScore();
			pPlayer->ClearRoundProximityScore();

			if ( winner == TEAM_CT )
			{
				IncrementStat( pPlayer, CSSTAT_CT_ROUNDS_WON, 1, true, true );
			}
			else if ( winner == TEAM_TERRORIST )
			{
				IncrementStat( pPlayer, CSSTAT_T_ROUNDS_WON, 1, true, true );
			}

			if ( winner == TEAM_CT || winner == TEAM_TERRORIST )
			{
				// Increment the win stats if this player is on the winning team
				if ( pPlayer->GetTeamNumber() == winner )
				{
					IncrementStat( pPlayer, CSSTAT_ROUNDS_WON, 1, true, true );
					if( CSGameRules( )->IsPlayingGunGame() )
					{
						IncrementStat( pPlayer, CSTAT_GUNGAME_ROUNDS_WON, 1, true, true );
					}

					if ( CSGameRules()->IsPistolRound() )
					{
						IncrementStat( pPlayer, CSSTAT_PISTOLROUNDS_WON, 1, true, true );
					}

					if ( mapStatWinIndex != CSSTAT_UNDEFINED )
					{
						IncrementStat( pPlayer, mapStatWinIndex, 1, true, true );
					}
				}

				if ( mapStatWinIndex != CSSTAT_UNDEFINED )
				{
					IncrementStat( pPlayer, mapStatRoundIndex, 1, true, true );
				}

				// set the play time for the round
				IncrementStat( pPlayer, CSSTAT_PLAYTIME, (int)CSGameRules()->GetRoundElapsedTime(), true, true );
			}
		}
	}

	// send a stats update to all players
	for ( int iPlayerIndex = 1; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iPlayerIndex ) );
		if ( pPlayer && pPlayer->IsConnected())
		{
			SendStatsToPlayer(pPlayer, CSSTAT_PRIORITY_ENDROUND);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Log accumulated weapon usage and performance data
//-----------------------------------------------------------------------------
void CCSGameStats::DumpMatchWeaponMetrics()
{
	// generate a filename
	std::time_t t = time( nullptr );
	struct tm *now = std::localtime( &t );
	if ( !now )
		return;

	int year = now->tm_year + 1900;
	int month = now->tm_mon + 1;
	int day = now->tm_mday;
	int hour = now->tm_hour;
	int minute = now->tm_min;
	int second = now->tm_sec;

	char filename[ 128 ];
	V_snprintf( filename, sizeof(filename), "wm_%4d%02d%02d_%02d%02d%02d_%s.csv", 
		year, month, day, hour, minute, second, gpGlobals->mapname.ToCStr());

	FileHandle_t hLogFile = filesystem->Open( filename, "wt" );

	if ( hLogFile == FILESYSTEM_INVALID_HANDLE )
		return;

	filesystem->FPrintf(hLogFile, "%s\n", "WeaponId, Mode, Cost, Bullets, CycleTime, TotalShots, TotalHits, TotalDamage, TotalKills");

	for (int iWeapon = 0; iWeapon < WEAPON_MAX; ++iWeapon)
	{
		const CCSWeaponInfo* pInfo = GetWeaponInfo( (CSWeaponID)iWeapon );
		if ( !pInfo )
			continue;

		const char* pWeaponName = pInfo->szClassName;
		if ( !pWeaponName )
			continue;

		if ( IsWeaponClassname( pWeaponName ) )
		{
			pWeaponName += WEAPON_CLASSNAME_PREFIX_LENGTH;
		}

		for ( int iMode = 0; iMode < WeaponMode_MAX; ++iMode)
		{
			filesystem->FPrintf(hLogFile, "%s, %d, %d, %d, %f, %d, %d, %d, %d\n", 
				pWeaponName,
				iMode,
				pInfo->GetWeaponPrice(),
				pInfo->GetBullets(),
				pInfo->GetCycleTime(),
				m_weaponStats[iWeapon][iMode].shots, 
				m_weaponStats[iWeapon][iMode].hits, 
				m_weaponStats[iWeapon][iMode].damage, 
				m_weaponStats[iWeapon][iMode].kills);
		}
	}

	filesystem->FPrintf(hLogFile, "\n");
	filesystem->FPrintf(hLogFile, "bot_difficulty, %d\n", cv_bot_difficulty.GetInt());

	g_pFullFileSystem->Close(hLogFile);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_PlayerConnected( CBasePlayer *pPlayer )
{
	ResetPlayerStats( ToCSPlayer( pPlayer ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_PlayerDisconnected( CBasePlayer *pPlayer )
{
	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
	if ( !pCSPlayer )
		return;

	ResetPlayerStats( pCSPlayer );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_PlayerKilledOther( CBasePlayer *pAttacker, CBaseEntity *pVictim, const CTakeDamageInfo &info )
{
	// This also gets called when the victim is a building.  That gets tracked separately as building destruction, don't count it here
	if ( !pVictim->IsPlayer() )
		return;

	CCSPlayer *pPlayerAttacker = ToCSPlayer( pAttacker );
	CCSPlayer *pPlayerVictim = ToCSPlayer( pVictim );	

	// keep track of how many times every player kills every other player
	TrackKillStats( pPlayerAttacker, pPlayerVictim );

	// Skip rest of stat reporting for friendly fire
	if ( pPlayerAttacker->GetTeam() == pVictim->GetTeam() )
		return;

	CWeaponCSBase* pCSWeapon = dynamic_cast<CWeaponCSBase*>( info.GetWeapon() );

	// CSN-8452 Gungame modes were counting towards kills with enemy weapons.
	if ( CSGameRules()->IsPlayingClassic() )
	{
		if ( pCSWeapon )
		{
			if ( pCSWeapon->WasOwnedByTeam( pVictim->GetTeamNumber() ) )
			{
				// Stat is incremented if kill is made with a weapon that was once owned by the team that the killed player is on
				IncrementStat(pPlayerAttacker, CSSTAT_KILLS_ENEMY_WEAPON, 1);
			}
		}
	}

	CSWeaponMode weaponMode = Primary_Mode;
	if ( pCSWeapon )
	{
		weaponMode = pCSWeapon->m_weaponMode;
	}

	// CSN-8983 - Grenades aren't the player's weapon upon killing. Get the weapon info from the damage.
	CSWeaponID weaponId = WEAPON_NONE;
	const CCSWeaponInfo* pWeaponInfoFronDamage = CCSPlayer::GetWeaponInfoFromDamageInfo(info);
	if ( pWeaponInfoFronDamage )
	{
		weaponId = pWeaponInfoFronDamage->m_weaponId;
	}

	// update weapon stats
	++m_weaponStats[weaponId][weaponMode].kills;

	for (int i = 0; WeaponName_StatId_Table[i].killStatId != CSSTAT_UNDEFINED; ++i)
	{
		if ( WeaponName_StatId_Table[i].weaponId == weaponId )
		{
			IncrementStat( pPlayerAttacker, WeaponName_StatId_Table[i].killStatId, 1 );
			break;
		}
	}

	if (pPlayerVictim && pPlayerVictim->IsBlind())
	{
		IncrementStat( pPlayerAttacker, CSSTAT_KILLS_ENEMY_BLINDED, 1 );
	}

	if (pPlayerVictim && pPlayerAttacker && pPlayerAttacker->IsBlindForAchievement())
	{
		IncrementStat( pPlayerAttacker, CSSTAT_KILLS_WHILE_BLINDED, 1 );
	}

	// [sbodenbender] check for deaths near planted bomb for funfact
	if (pPlayerVictim && pPlayerAttacker && pPlayerAttacker->GetTeamNumber() == TEAM_TERRORIST && CSGameRules()->m_bBombPlanted)
	{
		if ( pPlayerAttacker->IsCloseToActiveBomb() || pPlayerVictim->IsCloseToActiveBomb() )		
		{
			IncrementStat(pPlayerAttacker, CSSTAT_KILLS_WHILE_DEFENDING_BOMB, 1);
		}
	}

	//Increment stat if this is a headshot.
	if (info.GetDamageType() & DMG_HEADSHOT)
	{
		IncrementStat( pPlayerAttacker, CSSTAT_KILLS_HEADSHOT, 1 );
	}

	IncrementStat( pPlayerAttacker, CSSTAT_KILLS, 1 );

	// we don't have a simple way (yet) to check if the victim actually just achieved The Unstoppable Force, so we
	// award this achievement simply if they've met the requirements and would have received it.  
	PlayerStats_t &victimStats = m_aPlayerStats[pVictim->entindex()];
	if (victimStats.statsCurrentRound[CSSTAT_KILLS] >= AchievementConsts::ImmovableObject_Kills && !CSGameRules()->IsPlayingGunGameProgressive() )
	{
		pPlayerAttacker->AwardAchievement(CSImmovableObject);
	}

	CCSGameRules::TeamPlayerCounts playerCounts[TEAM_MAXCOUNT];

	CSGameRules()->GetPlayerCounts(playerCounts);
	int iAttackerTeamNumber = pPlayerAttacker->GetTeamNumber() ;
	if (playerCounts[iAttackerTeamNumber].totalAlivePlayers == 1 && playerCounts[iAttackerTeamNumber].killedPlayers >= 2)
	{
		IncrementStat(pPlayerAttacker, CSSTAT_KILLS_WHILE_LAST_PLAYER_ALIVE, 1);
	}	

	//if they were damaged by more than one person that must mean that someone else did damage before the killer finished them off.
	if (pPlayerVictim->GetNumEnemyDamagers() > 1)
	{
		IncrementStat(pPlayerAttacker, CSSTAT_KILLS_ENEMY_WOUNDED, 1);
	}	

	// set the number of consecutive kills this scorer has on the victim:
	int nConsecutiveKills = pPlayerAttacker ? std::max( FindPlayerStats( pPlayerVictim ).statsKills.iNumKilledByUnanswered[pPlayerAttacker->entindex()], 1 ) : 0;
	pPlayerVictim->SetLastConcurrentKilled( std::min( nConsecutiveKills, 8 ) );

	// check to see if a player killed another with a StatTrak weapon
	if ( CBaseCombatWeapon *pWeapon = entity_cast<CBaseCombatWeapon>( info.GetWeapon() ) )
	{
		// Does the player own this item?
		CPlayerId HolderPlayerID;
		pAttacker->GetPlayerID( &HolderPlayerID );
		if ( CEconItemView *pItemView = pWeapon->GetEconItemView() )
		{
			// Get the supported killeater types on this weapon
			std::flat_multiset<uint32> killEaterTypes;
			pItemView->GetKillEaterTypes( killEaterTypes );

			if ( !killEaterTypes.empty() )
			{
				if ( pItemView->GetAccountID() == HolderPlayerID.GetAccountID() )
				{
					// Here we could differentiate on StatTrak types (normal, headshot, etc.):
					IncrementStat( pPlayerAttacker, CSSTAT_KILLS_WITH_STATTRAK_WEAPON, 1 );
				}
			}
		}
		
	}
}

void CCSGameStats::CalculateOverkill(CCSPlayer* pAttacker, CCSPlayer* pVictim)
{
	//Count domination overkills - Do this before determining domination
	if (pAttacker->GetTeam() != pVictim->GetTeam())
	{
		if (pAttacker->IsPlayerDominated(pVictim->entindex()))
		{
			IncrementStat( pAttacker, CSSTAT_DOMINATION_OVERKILLS, 1 );
		}
	}
}
	
//-----------------------------------------------------------------------------
// Purpose: Stats event for giving damage to player
//-----------------------------------------------------------------------------
void CCSGameStats::Event_PlayerDamage( CBasePlayer *pBasePlayer, const CTakeDamageInfo &info )
{
	CCSPlayer *pAttacker = ToCSPlayer( info.GetAttacker() );
	if ( pAttacker && pAttacker->GetTeam() != pBasePlayer->GetTeam() )
	{
		IncrementStat( pAttacker, CSSTAT_DAMAGE, info.GetDamage() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Stats event for giving money to player
//-----------------------------------------------------------------------------
void CCSGameStats::Event_MoneyEarned( CCSPlayer* pPlayer, int moneyEarned)
{
	if ( pPlayer && moneyEarned > 0)
	{
		IncrementStat(pPlayer, CSSTAT_MONEY_EARNED, moneyEarned);
	}
}

void CCSGameStats::Event_MoneySpent( CCSPlayer* pPlayer, int moneySpent, const char *pItemName )
{
	if ( pPlayer && moneySpent > 0)
	{
		IncrementStat(pPlayer, CSSTAT_MONEY_SPENT, moneySpent);
	}
}

void CCSGameStats::Event_PlayerDonatedWeapon (CCSPlayer* pPlayer)
{
	if (pPlayer)
	{
		IncrementStat(pPlayer, CSSTAT_WEAPONS_DONATED, 1);
	}
}

void CCSGameStats::Event_MVPEarned( CCSPlayer* pPlayer )
{
	if (pPlayer)
	{
		IncrementStat(pPlayer, CSSTAT_MVPS, 1);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Event handler
//-----------------------------------------------------------------------------
void CCSGameStats::FireGameEvent( IGameEvent *event )
{
	const char *pEventName = event->GetName();

	if ( V_strcmp(pEventName, "round_end") == 0 )
	{
		const int reason = event->GetInt( "reason" );

		if( reason == Game_Commencing )
		{
			ResetPlayerClassMatchStats();
		}
		else
		{
			UpdatePlayerRoundStats(event->GetInt("winner"));
		}
	}
	else if ( V_strcmp( pEventName, "round_officially_ended" ) == 0 )
	{
		CCSPlayer::ResetBulletGroup();
	}
	else if ( V_strcmp(pEventName, "break_prop") == 0 )
	{
		int userid = event->GetInt("userid", 0);
		int entindex = event->GetInt("entindex", 0);
		CBreakableProp* pProp = static_cast<CBreakableProp*>(CBaseEntity::Instance(entindex));
 		Event_BreakProp(ToCSPlayer(UTIL_PlayerByUserId(userid)), pProp);
	}
	else if ( V_strcmp(pEventName, "player_decal") == 0 )
	{
		int userid = event->GetInt("userid", 0);
		Event_PlayerSprayedDecal(ToCSPlayer(UTIL_PlayerByUserId(userid)));
	}	
}

//-----------------------------------------------------------------------------
// Purpose: Return stats for the given player
//-----------------------------------------------------------------------------
const PlayerStats_t& CCSGameStats::FindPlayerStats( CBasePlayer *pPlayer ) const
{
	return m_aPlayerStats[pPlayer->entindex()];
}

//-----------------------------------------------------------------------------
// Purpose: Return stats for the given team
//-----------------------------------------------------------------------------
const StatsCollection_t& CCSGameStats::GetTeamStats( int iTeamIndex ) const
{
	int arrayIndex = iTeamIndex - FIRST_GAME_TEAM;
	contract_assert( arrayIndex >= 0 && arrayIndex < TEAM_MAXCOUNT - FIRST_GAME_TEAM );
	return m_aTeamStats[arrayIndex];
}

//-----------------------------------------------------------------------------
// Purpose: Resets the stats for each team
//-----------------------------------------------------------------------------
void CCSGameStats::ResetAllTeamStats()
{
	for ( int i = 0; i < std::size( m_aTeamStats ); ++i )
	{
		m_aTeamStats[i].Reset();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Resets all stats (including round, match, accumulated and rolling averages
//-----------------------------------------------------------------------------
void CCSGameStats::ResetAllStats()
{
	for ( int i = 0; i < std::size( m_aPlayerStats ); i++ )
	{		
		m_aPlayerStats[i].statsDelta.Reset();
		m_aPlayerStats[i].statsCurrentRound.Reset();
		m_aPlayerStats[i].statsCurrentMatch.Reset();
 		m_aPlayerStats[i].statsKills.Reset();
		m_numberOfRoundsForDirectAverages = 0;
		m_numberOfTerroristEntriesForDirectAverages = 0;
		m_numberOfCounterTerroristEntriesForDirectAverages = 0;
	}
}

void CCSGameStats::ResetWeaponStats()
{
	V_memset(m_weaponStats, 0, sizeof(m_weaponStats));
}

void CCSGameStats::IncrementTeamStat( int iTeamIndex, int iStatIndex, int iAmount )
{
	int arrayIndex = iTeamIndex - TEAM_TERRORIST;
	contract_assert( iStatIndex >= 0 && iStatIndex < CSSTAT_MAX );
	if( arrayIndex >= 0 && arrayIndex < TEAM_MAXCOUNT - TEAM_TERRORIST )
	{
		m_aTeamStats[arrayIndex][iStatIndex] += iAmount;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Resets all stats for this player
//-----------------------------------------------------------------------------
void CCSGameStats::ResetPlayerStats( CBasePlayer* pPlayer )
{
	PlayerStats_t &stats = m_aPlayerStats[pPlayer->entindex()];
	// reset the stats on this player
	stats.Reset();
	// reset the matrix of who killed whom with respect to this player
	ResetKillHistory( pPlayer );
}

//-----------------------------------------------------------------------------
// Purpose: Resets the kill history for this player
//-----------------------------------------------------------------------------
void CCSGameStats::ResetKillHistory( CBasePlayer* pPlayer )
{
	int iPlayerIndex = pPlayer->entindex();

	PlayerStats_t& statsPlayer = m_aPlayerStats[iPlayerIndex];

	// for every other player, set all all the kills with respect to this player to 0
	for ( int i = 0; i < std::size( m_aPlayerStats ); i++ )
	{
		//reset their record of us.
		PlayerStats_t &statsOther = m_aPlayerStats[i];
		statsOther.statsKills.iNumKilled[iPlayerIndex] = 0;
		statsOther.statsKills.iNumKilledBy[iPlayerIndex] = 0;
		statsOther.statsKills.iNumKilledByUnanswered[iPlayerIndex] = 0;

		//reset our record of them
		statsPlayer.statsKills.iNumKilled[i] = 0;
		statsPlayer.statsKills.iNumKilledBy[i] = 0;
		statsPlayer.statsKills.iNumKilledByUnanswered[i] = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Resets per-round stats for all players
//-----------------------------------------------------------------------------
void CCSGameStats::ResetRoundStats()
{
	for ( int i = 0; i < std::size( m_aPlayerStats ); i++ )
	{		
		m_aPlayerStats[i].statsCurrentRound.Reset();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Increments specified stat for specified player by specified amount
//-----------------------------------------------------------------------------
void CCSGameStats::IncrementStat( CCSPlayer* pPlayer, CSStatType_t statId, int iDelta, bool bPlayerOnly /* = false */, bool bIncludeBotController /* = false */ )
{
	// note: don't track stats for players after they switch teams 
	if ( pPlayer && !pPlayer->m_bTeamChanged) 
	{
		// if we're controlling a bot, credit the BOT with our stats
		if( pPlayer->IsControllingBot() )
		{
			CCSPlayer* controlledPlayer = pPlayer->GetControlledBot();

			contract_assert( ( controlledPlayer != pPlayer ) && "Player should never match controlled player: this will cause an infinite loop" );
			if( controlledPlayer )
			{
				IncrementStat( controlledPlayer, statId,iDelta,bPlayerOnly );
			}

			if ( !bIncludeBotController )
				return;
		}

		PlayerStats_t &stats = m_aPlayerStats[pPlayer->entindex()];
	    stats.statsDelta[statId] += iDelta;
	    stats.statsCurrentRound[statId] += iDelta;
	    stats.statsCurrentMatch[statId] += iDelta;

		// increment team stat
		int teamIndex = pPlayer->GetTeamNumber() - FIRST_GAME_TEAM;
		if ( !bPlayerOnly && teamIndex >= 0 && teamIndex < std::size( m_aTeamStats ) )
		{
			m_aTeamStats[teamIndex][statId] += iDelta;
		}

		for (int i = 0; i < std::size( ServerStatBasedAchievements ); ++i)
		{
			if (ServerStatBasedAchievements[i].statId == statId)
			{
				// skip this if there is a map filter and it doesn't match
				if (ServerStatBasedAchievements[i].mapFilter != nullptr && V_strcmp(gpGlobals->mapname.ToCStr(), ServerStatBasedAchievements[i].mapFilter) != 0)
					continue;

				if ( CSGameRules()->IsPlayingGunGameProgressive() && ServerStatBasedAchievements[i].disallowGunGameProgressive )
					continue;

			    bool bWasMet = ServerStatBasedAchievements[i].IsMet(stats.statsCurrentRound[statId] - iDelta, stats.statsCurrentMatch[statId] - iDelta);
			    bool bIsMet = ServerStatBasedAchievements[i].IsMet(stats.statsCurrentRound[statId], stats.statsCurrentMatch[statId]);
				if (!bWasMet && bIsMet)
				{
 					pPlayer->AwardAchievement(ServerStatBasedAchievements[i].achievementId);
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:  Sets the specified stat for specified player to the specified amount
//-----------------------------------------------------------------------------
void CCSGameStats::SetStat( CCSPlayer *pPlayer, CSStatType_t statId, int iValue )
{
	if (pPlayer)
	{
		int oldRoundValue, oldMatchValue;
		PlayerStats_t &stats = m_aPlayerStats[pPlayer->entindex()];

		oldRoundValue = stats.statsCurrentRound[statId];
		oldMatchValue = stats.statsCurrentMatch[statId];

		stats.statsDelta[statId] = iValue;
		stats.statsCurrentRound[statId] = iValue;
		stats.statsCurrentMatch[statId] = iValue;

		for (int i = 0; i < std::size( ServerStatBasedAchievements ); ++i)
		{
			if (ServerStatBasedAchievements[i].statId == statId)
			{
				// skip this if there is a map filter and it doesn't match
				if (ServerStatBasedAchievements[i].mapFilter != nullptr && V_strcmp(gpGlobals->mapname.ToCStr(), ServerStatBasedAchievements[i].mapFilter) != 0)
					continue;

				bool bWasMet = ServerStatBasedAchievements[i].IsMet(oldRoundValue, oldMatchValue);
				bool bIsMet = ServerStatBasedAchievements[i].IsMet(stats.statsCurrentRound[statId], stats.statsCurrentMatch[statId]);
				if (!bWasMet && bIsMet)
				{
					pPlayer->AwardAchievement(ServerStatBasedAchievements[i].achievementId);
				}
			}
		}
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

void CCSGameStats::SendStatsToPlayer( CCSPlayer * pPlayer, int iMinStatPriority )
{
	ASSERT(CSSTAT_MAX < 0xFFFF); // if we add more than 2^16 stats, we'll need to update this protocol
	if ( pPlayer && pPlayer->IsConnected())
	{				
		StatsCollection_t &deltaStats = m_aPlayerStats[pPlayer->entindex()].statsDelta;

		// check to see if we have any stats to actually send
		short  iStatsToSend = 0;
		for ( int iStat = CSSTAT_FIRST; iStat < CSSTAT_MAX; ++iStat )
		{
			int iPriority = CSStatProperty_Table[iStat].flags & CSSTAT_PRIORITY_MASK;
			if (deltaStats[iStat] != 0 && iPriority >= iMinStatPriority)
			{
				++iStatsToSend;
			}
		}

		// nothing changed - bail out
		if ( !iStatsToSend )
			return;

		CSingleUserRecipientFilter filter( pPlayer );
		filter.MakeReliable();

		ks::net::CCSUsrMsg_PlayerStatsUpdate msg;
		
		CRC32_t crc;
		CRC32_Init( &crc );

		// begin the CRC with a trivially hidden key value to discourage packet modification
		const uint32 key = 0x82DA9F4C;	// this key should match the key in cs_client_gamestats.cpp
		CRC32Helper_ProcessUInt32( crc, key );

		// if we make any change to the ordering of the stats or this message format, update this value
		const byte version = 0x03;
		CRC32_ProcessBuffer( &crc, &version, sizeof(version));
		msg.version = version;

		CRC32Helper_ProcessInt16( crc, iStatsToSend );

		for ( short iStat = CSSTAT_FIRST; iStat < CSSTAT_MAX; ++iStat )
		{
			int iPriority = CSStatProperty_Table[iStat].flags & CSSTAT_PRIORITY_MASK;
			if (deltaStats[iStat] != 0 && iPriority >= iMinStatPriority)
			{
				ks::net::CCSUsrMsg_PlayerStatsUpdate::Stat *pStat = &msg.stats.emplace_back();

				CRC32Helper_ProcessInt16( crc, iStat );
				pStat->idx = iStat;

				contract_assert( deltaStats[iStat] <= 0x7FFF && deltaStats[iStat] > 0 );	// make sure we aren't truncating bits

				short delta = deltaStats[iStat];
				CRC32Helper_ProcessInt16( crc, delta );
				pStat->delta = deltaStats[iStat];

				deltaStats[iStat] = 0;
				--iStatsToSend;
			}
		}

		contract_assert( iStatsToSend == 0 );

		int userID = pPlayer->GetUserID();
		msg.user_id = userID;
		CRC32Helper_ProcessInt32( crc, userID );

		CRC32_Final( &crc );
		msg.crc = crc;

		SendUserMessage( filter, ks::net::CS_UM_PlayerStatsUpdate, msg );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sends intermittent stats updates for stats that need to be updated during a round and/or life
//-----------------------------------------------------------------------------
void CCSGameStats::PreClientUpdate()
{
	int iMinStatPriority = -1;
	m_fDisseminationTimerHigh += gpGlobals->frametime;
	m_fDisseminationTimerLow += gpGlobals->frametime;

	if ( m_fDisseminationTimerHigh > cDisseminationTimeHigh)
	{
		iMinStatPriority = CSSTAT_PRIORITY_HIGH;
		m_fDisseminationTimerHigh = 0.0f;

		if ( m_fDisseminationTimerLow > cDisseminationTimeLow)
		{
			iMinStatPriority = CSSTAT_PRIORITY_LOW;
			m_fDisseminationTimerLow = 0.0f;
		}
	}
	else
		return;

	//The proper time has elapsed, now send the update to every player
	for ( int iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex(iPlayerIndex) );
		SendStatsToPlayer(pPlayer, iMinStatPriority);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates the stats of who has killed whom
//-----------------------------------------------------------------------------
void CCSGameStats::TrackKillStats( CCSPlayer *pAttacker, CCSPlayer *pVictim )
{
	int iPlayerIndexAttacker = pAttacker->entindex();
	int iPlayerIndexVictim = pVictim->entindex();

	PlayerStats_t &statsAttacker = m_aPlayerStats[iPlayerIndexAttacker];
	PlayerStats_t &statsVictim = m_aPlayerStats[iPlayerIndexVictim];

	if( !pVictim->IsControllingBot() )
	{
		statsVictim.statsKills.iNumKilledBy[iPlayerIndexAttacker]++;
		statsVictim.statsKills.iNumKilledByUnanswered[iPlayerIndexAttacker]++;
	}

	if( !pAttacker->IsControllingBot() )
	{
		statsAttacker.statsKills.iNumKilled[iPlayerIndexVictim]++;
		statsAttacker.statsKills.iNumKilledByUnanswered[iPlayerIndexVictim] = 0;    
	}
}

//-----------------------------------------------------------------------------
// Purpose: Determines if attacker and victim have gotten domination or revenge
//-----------------------------------------------------------------------------
void CCSGameStats::CalcDominationAndRevenge( CCSPlayer *pAttacker, CCSPlayer *pVictim, int *piDeathFlags )
{
	// [Forrest] Allow nemesis/revenge to be turned off for a server
	if ( sv_nonemesis.GetBool() )
	{
		return;
	}

	// if we aren't playing gungame, we dont do domination or revenge
	if ( !CSGameRules()->IsPlayingGunGame() )
		return;

	//If there is no attacker, there is no domination or revenge
	if( !pAttacker || !pVictim )
	{
		return;
	}
	if (pAttacker->GetTeam() == pVictim->GetTeam())
	{
		return;
	}
	int iPlayerIndexVictim = pVictim->entindex();
	PlayerStats_t &statsVictim = m_aPlayerStats[iPlayerIndexVictim];	
	// calculate # of unanswered kills between killer & victim
	// This is plus 1 as this function gets called before the stat is updated.  That is done so that the domination
	// and revenge will be calculated prior to the death message being sent to the clients
	int attackerEntityIndex = pAttacker->entindex();
	int iKillsUnanswered = statsVictim.statsKills.iNumKilledByUnanswered[attackerEntityIndex] + 1;	

	if ( CS_KILLS_FOR_DOMINATION == iKillsUnanswered )
	{
		// this is the Nth unanswered kill between killer and victim, killer is now dominating victim
		*piDeathFlags |= ( CS_DEATH_DOMINATION );
	}
	else if ( pVictim->IsPlayerDominated( pAttacker->entindex() ) && !pAttacker->IsControllingBot() )
	{
		// the killer killed someone who was dominating him, gains revenge
		*piDeathFlags |= ( CS_DEATH_REVENGE );
	}

	//Check the overkill on 1 player achievement
	if (!pAttacker->IsControllingBot() && iKillsUnanswered == CS_KILLS_FOR_DOMINATION + AchievementConsts::ExtendedDomination_AdditionalKills)
	{
		pAttacker->AwardAchievement(CSExtendedDomination);
	}

	if (!pAttacker->IsControllingBot() && iKillsUnanswered == CS_KILLS_FOR_DOMINATION)
	{			
		//this is the Nth unanswered kill between killer and victim, killer is now dominating victim        
		//set victim to be dominated by killer
		pAttacker->SetPlayerDominated( pVictim, true );

		//Check concurrent dominations achievement
		int numConcurrentDominations = 0;
		for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
		{
			CCSPlayer *pPlayer= ToCSPlayer( UTIL_PlayerByIndex( i ) );
			if (pPlayer && pAttacker->IsPlayerDominated(pPlayer->entindex()))
			{
				numConcurrentDominations++;
			}
		}
		if (numConcurrentDominations >= AchievementConsts::ConcurrentDominations_MinDominations)
		{
			pAttacker->AwardAchievement(CSConcurrentDominations);
		}

		// record stats
		Event_PlayerDominatedOther( pAttacker, pVictim );
	}
	else if ( pVictim->IsPlayerDominated( pAttacker->entindex() ) && !pAttacker->IsControllingBot() )
	{
		// the killer killed someone who was dominating him, gains revenge        
		// set victim to no longer be dominating the killer

		pVictim->SetPlayerDominated( pAttacker, false );
		// record stats
		Event_PlayerRevenge( pAttacker );
	}
}

void CCSGameStats::Event_PlayerDominatedOther( CCSPlayer *pAttacker, CCSPlayer* pVictim )
{ 
	IncrementStat( pAttacker, CSSTAT_DOMINATIONS, 1 );
}

void CCSGameStats::Event_PlayerRevenge( CCSPlayer *pAttacker )
{
	IncrementStat( pAttacker, CSSTAT_REVENGES, 1 );
}

void CCSGameStats::Event_PlayerAvengedTeammate( CCSPlayer* pAttacker, CCSPlayer* pAvengedPlayer )
{
	if (pAttacker && pAvengedPlayer)
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "player_avenged_teammate" );

		if ( event )
		{
			event->SetInt( "avenger_id", pAttacker->GetUserID() );
			event->SetInt( "avenged_player_id", pAvengedPlayer->GetUserID() );
			gameeventmanager->FireEvent( event );
		}
	}
}

void CCSGameStats::Event_LevelInit()
{
	ResetAllTeamStats();
	ResetWeaponStats();
	CBaseGameStats::Event_LevelInit();
}

void CCSGameStats::Event_LevelShutdown( float fElapsed )
{
	if (sv_dumpmatchweaponmetrics.GetBool())
	{
		DumpMatchWeaponMetrics();
	}
	CBaseGameStats::Event_LevelShutdown(fElapsed);

}

// Reset any per match info that resides in the player class
void CCSGameStats::ResetPlayerClassMatchStats()
{
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

		if ( pPlayer )
		{
			pPlayer->SetNumMVPs( 0 );
		}
	}
}

float UTIL_GetEffectiveRange( CCSPlayer* pPlayer )
{
	if ( !pPlayer )
		return 0.0f;

	CWeaponCSBase *weapon = dynamic_cast< CWeaponCSBase * >( pPlayer->GetActiveWeapon() );

	if ( !weapon )
		return 0.0f;

	Vector vecDirShooting, vecRight, vecUp;
	AngleVectors( pPlayer->GetFinalAimAngle(), &vecDirShooting, &vecRight, &vecUp );
	float fInaccuracy = weapon->GetInaccuracy();
	float fSpread = weapon->GetSpread();
	float fFinalInaccuracy = fInaccuracy + fSpread;
	Vector vecInaccFinal = vecDirShooting + fFinalInaccuracy * vecRight + fFinalInaccuracy * vecUp;
	VectorNormalize( vecInaccFinal );
	float flDotInaccFinal = DotProduct( vecDirShooting.Normalized(), vecInaccFinal.Normalized() );
	float flAngleInaccFinal = flDotInaccFinal < 0.0f ? -std::acos( flDotInaccFinal ) : acos( flDotInaccFinal );
	//Msg( "Inaccuracy			: %.2f deg.\n", RAD2DEG( flAngleInaccFinal ) );

	return ( 0.5 * 12 ) / std::tanf( 0.5 * flAngleInaccFinal ); // 12 inch dinner plate 
}

using namespace ::ks::reflect;

// note:  We don't log match stats for any maps with CSSTAT_UNDEFINED for matchID
const MapName_MapStatId MapName_StatId_Table[] =
{
	{"cs_assault",  CSSTAT_MAP_WINS_CS_ASSAULT,		CSSTAT_MAP_ROUNDS_CS_ASSAULT, CSSTAT_UNDEFINED },
	{"cs_italy",    CSSTAT_MAP_WINS_CS_ITALY,		CSSTAT_MAP_ROUNDS_CS_ITALY, CSSTAT_UNDEFINED },
	{"cs_office",   CSSTAT_MAP_WINS_CS_OFFICE,		CSSTAT_MAP_ROUNDS_CS_OFFICE, CSSTAT_UNDEFINED },
	{"cs_militia",  CSSTAT_MAP_WINS_CS_MILITIA,		CSSTAT_MAP_ROUNDS_CS_MILITIA, CSSTAT_UNDEFINED },
	{"de_aztec",    CSSTAT_MAP_WINS_DE_AZTEC,		CSSTAT_MAP_ROUNDS_DE_AZTEC, CSSTAT_UNDEFINED },
	{"de_cbble",    CSSTAT_MAP_WINS_DE_CBBLE,		CSSTAT_MAP_ROUNDS_DE_CBBLE, CSSTAT_UNDEFINED },
	{"de_dust2",    CSSTAT_MAP_WINS_DE_DUST2,		CSSTAT_MAP_ROUNDS_DE_DUST2, CSSTAT_UNDEFINED },
	{"de_dust",     CSSTAT_MAP_WINS_DE_DUST,		CSSTAT_MAP_ROUNDS_DE_DUST, CSSTAT_UNDEFINED },
	{"de_inferno",  CSSTAT_MAP_WINS_DE_INFERNO,		CSSTAT_MAP_ROUNDS_DE_INFERNO, CSSTAT_UNDEFINED },
	{"de_nuke",     CSSTAT_MAP_WINS_DE_NUKE,		CSSTAT_MAP_ROUNDS_DE_NUKE, CSSTAT_UNDEFINED },
	{"de_piranesi", CSSTAT_MAP_WINS_DE_PIRANESI,	CSSTAT_MAP_ROUNDS_DE_PIRANESI, CSSTAT_UNDEFINED },
	{"de_prodigy",  CSSTAT_MAP_WINS_DE_PRODIGY,		CSSTAT_MAP_ROUNDS_DE_PRODIGY, CSSTAT_UNDEFINED },
	{"de_lake",		CSSTAT_MAP_WINS_DE_LAKE,		CSSTAT_MAP_ROUNDS_DE_LAKE, CSSTAT_MAP_MATCHES_WON_LAKE },
	{"de_safehouse",CSSTAT_MAP_WINS_DE_SAFEHOUSE,	CSSTAT_MAP_ROUNDS_DE_SAFEHOUSE, CSSTAT_MAP_MATCHES_WON_SAFEHOUSE},
	{"de_shorttrain",CSSTAT_MAP_WINS_DE_SHORTTRAIN,	CSSTAT_MAP_ROUNDS_DE_SHORTTRAIN, CSSTAT_MAP_MATCHES_WON_SHORTTRAIN }, 
	{"de_sugarcane",CSSTAT_MAP_WINS_DE_SUGARCANE,	CSSTAT_MAP_ROUNDS_DE_SUGARCANE, CSSTAT_MAP_MATCHES_WON_SUGARCANE },
	{"de_stmarc",   CSSTAT_MAP_WINS_DE_STMARC,		CSSTAT_MAP_ROUNDS_DE_STMARC, CSSTAT_MAP_MATCHES_WON_STMARC },
	{"de_bank",     CSSTAT_MAP_WINS_DE_BANK,		CSSTAT_MAP_ROUNDS_DE_BANK, CSSTAT_MAP_MATCHES_WON_BANK },
	{"de_embassy",  CSSTAT_MAP_WINS_DE_EMBASSY,		CSSTAT_MAP_ROUNDS_DE_EMBASSY, CSSTAT_MAP_MATCHES_WON_EMBASSY },
	{"de_depot",    CSSTAT_MAP_WINS_DE_DEPOT,		CSSTAT_MAP_ROUNDS_DE_DEPOT, CSSTAT_MAP_MATCHES_WON_DEPOT },
	{"de_vertigo",  CSSTAT_MAP_WINS_DE_VERTIGO,		CSSTAT_MAP_ROUNDS_DE_VERTIGO, CSSTAT_UNDEFINED},
	{"de_balkan",	CSSTAT_MAP_WINS_DE_BALKAN,		CSSTAT_MAP_ROUNDS_DE_BALKAN, CSSTAT_UNDEFINED},
	{"ar_monastery",CSSTAT_MAP_WINS_AR_MONASTERY,	CSSTAT_MAP_ROUNDS_AR_MONASTERY, CSSTAT_UNDEFINED},
	{"ar_shoots",   CSSTAT_MAP_WINS_AR_SHOOTS,		CSSTAT_MAP_ROUNDS_AR_SHOOTS, CSSTAT_MAP_MATCHES_WON_SHOOTS},
	{"ar_baggage",  CSSTAT_MAP_WINS_AR_BAGGAGE,		CSSTAT_MAP_ROUNDS_AR_BAGGAGE,CSSTAT_MAP_MATCHES_WON_BAGGAGE},
	{"de_train",	CSSTAT_MAP_WINS_DE_TRAIN,		CSSTAT_MAP_ROUNDS_DE_TRAIN,		CSSTAT_MAP_MATCHES_WON_TRAIN }, 
	{"",			CSSTAT_UNDEFINED,				CSSTAT_UNDEFINED, CSSTAT_UNDEFINED },
};

const WeaponName_StatId WeaponName_StatId_Table[] =
{
	{ WEAPON_DEAGLE,		CSSTAT_KILLS_DEAGLE,	CSSTAT_SHOTS_DEAGLE,	CSSTAT_HITS_DEAGLE, 	CSSTAT_DAMAGE_DEAGLE 	},
	{ WEAPON_USP,			CSSTAT_KILLS_USP,		CSSTAT_SHOTS_USP,		CSSTAT_HITS_USP,		CSSTAT_DAMAGE_USP		},
	{ WEAPON_GLOCK,			CSSTAT_KILLS_GLOCK,		CSSTAT_SHOTS_GLOCK,		CSSTAT_HITS_GLOCK,		CSSTAT_DAMAGE_GLOCK		},
	{ WEAPON_P228,			CSSTAT_KILLS_P228,		CSSTAT_SHOTS_P228,		CSSTAT_HITS_P228,		CSSTAT_DAMAGE_P228		},
	{ WEAPON_ELITE,			CSSTAT_KILLS_ELITE,		CSSTAT_SHOTS_ELITE,		CSSTAT_HITS_ELITE,		CSSTAT_DAMAGE_ELITE		},
	{ WEAPON_FIVESEVEN,		CSSTAT_KILLS_FIVESEVEN, CSSTAT_SHOTS_FIVESEVEN, CSSTAT_HITS_FIVESEVEN,	CSSTAT_DAMAGE_FIVESEVEN	},
	{ WEAPON_AWP,			CSSTAT_KILLS_AWP,		CSSTAT_SHOTS_AWP,		CSSTAT_HITS_AWP,		CSSTAT_DAMAGE_AWP		},
	{ WEAPON_AK47,			CSSTAT_KILLS_AK47,		CSSTAT_SHOTS_AK47,		CSSTAT_HITS_AK47,		CSSTAT_DAMAGE_AK47		},
	{ WEAPON_M4A1,			CSSTAT_KILLS_M4A1,		CSSTAT_SHOTS_M4A1,		CSSTAT_HITS_M4A1,		CSSTAT_DAMAGE_M4A1		},
	{ WEAPON_AUG,			CSSTAT_KILLS_AUG,		CSSTAT_SHOTS_AUG,		CSSTAT_HITS_AUG,		CSSTAT_DAMAGE_AUG		},
	{ WEAPON_SG552,			CSSTAT_KILLS_SG552,		CSSTAT_SHOTS_SG552,		CSSTAT_HITS_SG552, 		CSSTAT_DAMAGE_SG552 	},
	{ WEAPON_SG550,			CSSTAT_KILLS_SG550,		CSSTAT_SHOTS_SG550,		CSSTAT_HITS_SG550, 		CSSTAT_DAMAGE_SG550 	},
	{ WEAPON_GALIL,			CSSTAT_KILLS_GALIL,		CSSTAT_SHOTS_GALIL,		CSSTAT_HITS_GALIL, 		CSSTAT_DAMAGE_GALIL 	},
	{ WEAPON_GALILAR,		CSSTAT_KILLS_GALILAR,	CSSTAT_SHOTS_GALILAR,	CSSTAT_HITS_GALILAR, 	CSSTAT_DAMAGE_GALILAR 	},
	{ WEAPON_FAMAS,			CSSTAT_KILLS_FAMAS,		CSSTAT_SHOTS_FAMAS,		CSSTAT_HITS_FAMAS, 		CSSTAT_DAMAGE_FAMAS 	},
	{ WEAPON_SCOUT,			CSSTAT_KILLS_SCOUT,		CSSTAT_SHOTS_SCOUT,		CSSTAT_HITS_SCOUT, 		CSSTAT_DAMAGE_SCOUT 	},
	{ WEAPON_G3SG1,			CSSTAT_KILLS_G3SG1,		CSSTAT_SHOTS_G3SG1,		CSSTAT_HITS_G3SG1, 		CSSTAT_DAMAGE_G3SG1 	},
	{ WEAPON_P90,			CSSTAT_KILLS_P90,		CSSTAT_SHOTS_P90,		CSSTAT_HITS_P90,		CSSTAT_DAMAGE_P90		},
	{ WEAPON_MP5NAVY,		CSSTAT_KILLS_MP5NAVY,	CSSTAT_SHOTS_MP5NAVY,	CSSTAT_HITS_MP5NAVY,	CSSTAT_DAMAGE_MP5NAVY	},
	{ WEAPON_TMP,			CSSTAT_KILLS_TMP,		CSSTAT_SHOTS_TMP,		CSSTAT_HITS_TMP,		CSSTAT_DAMAGE_TMP		},
	{ WEAPON_MAC10,			CSSTAT_KILLS_MAC10,		CSSTAT_SHOTS_MAC10,		CSSTAT_HITS_MAC10,		CSSTAT_DAMAGE_MAC10		},
	{ WEAPON_UMP45,			CSSTAT_KILLS_UMP45,		CSSTAT_SHOTS_UMP45,		CSSTAT_HITS_UMP45,		CSSTAT_DAMAGE_UMP45		},
	{ WEAPON_M3,			CSSTAT_KILLS_M3,		CSSTAT_SHOTS_M3,		CSSTAT_HITS_M3,			CSSTAT_DAMAGE_M3		},
	{ WEAPON_XM1014,		CSSTAT_KILLS_XM1014,	CSSTAT_SHOTS_XM1014,	CSSTAT_HITS_XM1014,		CSSTAT_DAMAGE_XM1014	},
	{ WEAPON_M249,			CSSTAT_KILLS_M249,		CSSTAT_SHOTS_M249,		CSSTAT_HITS_M249,		CSSTAT_DAMAGE_M249		},
	{ WEAPON_KNIFE_GG,		CSSTAT_KILLS_KNIFE,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED		},
	{ WEAPON_KNIFE,			CSSTAT_KILLS_KNIFE,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED		},
	{ WEAPON_HEGRENADE,		CSSTAT_KILLS_HEGRENADE, CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED		},
	{ WEAPON_MOLOTOV,		CSSTAT_KILLS_MOLOTOV,	CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED		},
	{ WEAPON_INCGRENADE,	CSSTAT_KILLS_MOLOTOV,	CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED		},
	{ WEAPON_DECOY,			CSSTAT_KILLS_DECOY,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED		},
	{ WEAPON_BIZON,			CSSTAT_KILLS_BIZON,		CSSTAT_SHOTS_BIZON,		CSSTAT_HITS_BIZON,		CSSTAT_DAMAGE_BIZON		},
	{ WEAPON_MAG7,			CSSTAT_KILLS_MAG7,		CSSTAT_SHOTS_MAG7,		CSSTAT_HITS_MAG7,		CSSTAT_DAMAGE_MAG7		},
	{ WEAPON_NEGEV,			CSSTAT_KILLS_NEGEV,		CSSTAT_SHOTS_NEGEV,		CSSTAT_HITS_NEGEV,		CSSTAT_DAMAGE_NEGEV		},
	{ WEAPON_SAWEDOFF,		CSSTAT_KILLS_SAWEDOFF,	CSSTAT_SHOTS_SAWEDOFF,	CSSTAT_HITS_SAWEDOFF,	CSSTAT_DAMAGE_SAWEDOFF	},
	{ WEAPON_TEC9,			CSSTAT_KILLS_TEC9,		CSSTAT_SHOTS_TEC9,		CSSTAT_HITS_TEC9,		CSSTAT_DAMAGE_TEC9		},
	{ WEAPON_TASER,			CSSTAT_KILLS_TASER,		CSSTAT_SHOTS_TASER,		CSSTAT_HITS_TASER,		CSSTAT_DAMAGE_TASER		},
	{ WEAPON_HKP2000,		CSSTAT_KILLS_HKP2000,	CSSTAT_SHOTS_HKP2000,	CSSTAT_HITS_HKP2000,	CSSTAT_DAMAGE_HKP2000	},
	{ WEAPON_MP7,			CSSTAT_KILLS_MP7,		CSSTAT_SHOTS_MP7,		CSSTAT_HITS_MP7,		CSSTAT_DAMAGE_MP7		},
	{ WEAPON_MP9,			CSSTAT_KILLS_MP9,		CSSTAT_SHOTS_MP9,		CSSTAT_HITS_MP9,		CSSTAT_DAMAGE_MP9		},
	{ WEAPON_NOVA,			CSSTAT_KILLS_NOVA,		CSSTAT_SHOTS_NOVA,		CSSTAT_HITS_NOVA,		CSSTAT_DAMAGE_NOVA		},
	{ WEAPON_P250,			CSSTAT_KILLS_P250,		CSSTAT_SHOTS_P250,		CSSTAT_HITS_P250,		CSSTAT_DAMAGE_P250		},
	{ WEAPON_SCAR17,		CSSTAT_KILLS_SCAR17,	CSSTAT_SHOTS_SCAR17,	CSSTAT_HITS_SCAR17,		CSSTAT_DAMAGE_SCAR17	},
	{ WEAPON_SCAR20,		CSSTAT_KILLS_SCAR20,	CSSTAT_SHOTS_SCAR20,	CSSTAT_HITS_SCAR20,		CSSTAT_DAMAGE_SCAR20	},
	{ WEAPON_SG556,			CSSTAT_KILLS_SG556,		CSSTAT_SHOTS_SG556,		CSSTAT_HITS_SG556,		CSSTAT_DAMAGE_SG556		},
	{ WEAPON_SSG08,			CSSTAT_KILLS_SSG08,		CSSTAT_SHOTS_SSG08,		CSSTAT_HITS_SSG08,		CSSTAT_DAMAGE_SSG08		},
	{ WEAPON_NONE,			CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED		},			// This is a sentinel value so we can loop through all the stats
//	{ WEAPON_SENSORGRENADE, CSSTAT_KILLS_SENSOR,	CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED },

};

struct CSStatProperty_Init
{
	int statId;
	CSStatProperty	statProperty;
};

CSStatProperty_Init CSStatProperty_Table_Init[] =
{
//		StatId									Localization Token						Client Update Priority
	{	CSSTAT_SHOTS_HIT,						"#GAMEUI_Stat_NumHits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_FIRED,						"#GAMEUI_Stat_NumShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_KILLS,							"#GAMEUI_Stat_NumKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_DEATHS,							"#GAMEUI_Stat_NumDeaths",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_DAMAGE,							"#GAMEUI_Stat_DamageDone",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_NUM_BOMBS_PLANTED,				"#GAMEUI_Stat_NumPlantedBombs",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_NUM_BOMBS_DEFUSED,				"#GAMEUI_Stat_NumDefusedBombs",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_TR_NUM_BOMBS_PLANTED,			"#GAMEUI_Stat_TR_NumPlantedBombs",		CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_TR_NUM_BOMBS_DEFUSED,			"#GAMEUI_Stat_TR_NumDefusedBombs",		CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_PLAYTIME,						"#GAMEUI_Stat_TimePlayed",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_ROUNDS_WON,						"#GAMEUI_Stat_TotalWins",				CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_T_ROUNDS_WON,					nullptr,									CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_CT_ROUNDS_WON,					nullptr,									CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_ROUNDS_PLAYED,					"#GAMEUI_Stat_TotalRounds",				CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_PISTOLROUNDS_WON,				"#GAMEUI_Stat_PistolRoundWins",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSTAT_GUNGAME_ROUNDS_WON,				"#GAMEUI_Stat_gun_game_rounds_won",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSTAT_GUNGAME_ROUNDS_PLAYED,			"#GAMEUI_Stat_gun_game_rounds_played",  CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MONEY_EARNED,					"#GAMEUI_Stat_MoneyEarned",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_OBJECTIVES_COMPLETED,			nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_BOMBS_DEFUSED_WITHKIT,			nullptr,									CSSTAT_PRIORITY_NEVER,			},

	{	CSSTAT_KILLS_DEAGLE,					"#GAMEUI_Stat_DeagleKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_USP,						"#GAMEUI_Stat_USPKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_GLOCK,						"#GAMEUI_Stat_GlockKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_P228,						"#GAMEUI_Stat_P228Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_ELITE,						"#GAMEUI_Stat_EliteKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_FIVESEVEN,					"#GAMEUI_Stat_FiveSevenKills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_AWP,						"#GAMEUI_Stat_AWPKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_AK47,						"#GAMEUI_Stat_AK47Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_M4A1,						"#GAMEUI_Stat_M4A1Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_AUG,						"#GAMEUI_Stat_AUGKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_SG552,						"#GAMEUI_Stat_SG552Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_SG550,						"#GAMEUI_Stat_SG550Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_GALIL,						"#GAMEUI_Stat_GALILKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_GALILAR,					"#GAMEUI_Stat_GALILARKills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_FAMAS,						"#GAMEUI_Stat_FAMASKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_SCOUT,						"#GAMEUI_Stat_ScoutKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_G3SG1,						"#GAMEUI_Stat_G3SG1Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_P90,						"#GAMEUI_Stat_P90Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_MP5NAVY,					"#GAMEUI_Stat_MP5NavyKills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_TMP,						"#GAMEUI_Stat_TMPKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_MAC10,						"#GAMEUI_Stat_MAC10Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_UMP45,						"#GAMEUI_Stat_UMP45Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_M3,						"#GAMEUI_Stat_M3Kills",					CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_XM1014,					"#GAMEUI_Stat_XM1014Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_M249,						"#GAMEUI_Stat_M249Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_KNIFE,						"#GAMEUI_Stat_KnifeKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_HEGRENADE,					"#GAMEUI_Stat_HEGrenadeKills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_MOLOTOV,					"#GAMEUI_Stat_MolotovKills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_DECOY,						"#GAMEUI_Stat_DecoyKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_BIZON,						"#GAMEUI_Stat_BIZONKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_MAG7,						"#GAMEUI_Stat_MAG7Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_NEGEV,						"#GAMEUI_Stat_NEGEVKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_SAWEDOFF,					"#GAMEUI_Stat_SAWEDOFFKills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_TEC9,						"#GAMEUI_Stat_TEC9Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_TASER,						"#GAMEUI_Stat_TASERKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_HKP2000,					"#GAMEUI_Stat_HKP2000Kills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_MP7,						"#GAMEUI_Stat_MP7Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_MP9,						"#GAMEUI_Stat_MP9Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_NOVA,						"#GAMEUI_Stat_NovaKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_P250,						"#GAMEUI_Stat_P250Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_SCAR17,					"#GAMEUI_Stat_SCAR17Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_SCAR20,					"#GAMEUI_Stat_SCAR20Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_SG556,						"#GAMEUI_Stat_SG556Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_SSG08,						"#GAMEUI_Stat_SSG08Kills",				CSSTAT_PRIORITY_HIGH,			},

	{	CSSTAT_SHOTS_DEAGLE,					"#GAMEUI_Stat_DeagleShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_USP,						"#GAMEUI_Stat_USPShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_GLOCK,						"#GAMEUI_Stat_GlockShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_P228,						"#GAMEUI_Stat_P228Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_ELITE,						"#GAMEUI_Stat_EliteShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_FIVESEVEN,					"#GAMEUI_Stat_FiveSevenShots",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_AWP,						"#GAMEUI_Stat_AWPShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_AK47,						"#GAMEUI_Stat_AK47Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_M4A1,						"#GAMEUI_Stat_M4A1Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_AUG,						"#GAMEUI_Stat_AUGShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_SG552,						"#GAMEUI_Stat_SG552Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_SG550,						"#GAMEUI_Stat_SG550Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_GALIL,						"#GAMEUI_Stat_GALILShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_GALILAR,					"#GAMEUI_Stat_GALILARShots",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_FAMAS,						"#GAMEUI_Stat_FAMASShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_SCOUT,						"#GAMEUI_Stat_ScoutShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_G3SG1,						"#GAMEUI_Stat_G3SG1Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_P90,						"#GAMEUI_Stat_P90Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_MP5NAVY,					"#GAMEUI_Stat_MP5NavyShots",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_TMP,						"#GAMEUI_Stat_TMPShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_MAC10,						"#GAMEUI_Stat_MAC10Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_UMP45,						"#GAMEUI_Stat_UMP45Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_M3,						"#GAMEUI_Stat_M3Shots",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_XM1014,					"#GAMEUI_Stat_XM1014Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_M249,						"#GAMEUI_Stat_M249Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_BIZON,						"#GAMEUI_Stat_BIZONShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_MAG7,						"#GAMEUI_Stat_MAG7Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_NEGEV,						"#GAMEUI_Stat_NEGEVShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_SAWEDOFF,					"#GAMEUI_Stat_SAWEDOFFShots",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_TEC9,						"#GAMEUI_Stat_TEC9Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_TASER,						"#GAMEUI_Stat_TASERShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_HKP2000,					"#GAMEUI_Stat_HKP2000Shots",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_MP7,						"#GAMEUI_Stat_MP7Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_MP9,						"#GAMEUI_Stat_MP9Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_NOVA,						"#GAMEUI_Stat_NovaShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_P250,						"#GAMEUI_Stat_P250Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_SCAR17,					"#GAMEUI_Stat_SCAR17Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_SCAR20,					"#GAMEUI_Stat_SCAR20Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_SG556,						"#GAMEUI_Stat_SG556Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_SSG08,						"#GAMEUI_Stat_SSG08Shots",				CSSTAT_PRIORITY_LOW,			},

	{	CSSTAT_HITS_DEAGLE,						"#GAMEUI_Stat_DeagleHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_USP,						"#GAMEUI_Stat_USPHits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_GLOCK,						"#GAMEUI_Stat_GlockHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_P228,						"#GAMEUI_Stat_P228Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_ELITE,						"#GAMEUI_Stat_EliteHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_FIVESEVEN,					"#GAMEUI_Stat_FiveSevenHits",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_AWP,						"#GAMEUI_Stat_AWPHits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_AK47,						"#GAMEUI_Stat_AK47Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_M4A1,						"#GAMEUI_Stat_M4A1Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_AUG,						"#GAMEUI_Stat_AUGHits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_SG552,						"#GAMEUI_Stat_SG552Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_SG550,						"#GAMEUI_Stat_SG550Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_GALIL,						"#GAMEUI_Stat_GALILHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_GALILAR,					"#GAMEUI_Stat_GALILARHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_FAMAS,						"#GAMEUI_Stat_FAMASHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_SCOUT,						"#GAMEUI_Stat_ScoutHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_G3SG1,						"#GAMEUI_Stat_G3SG1Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_P90,						"#GAMEUI_Stat_P90Hits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_MP5NAVY,					"#GAMEUI_Stat_MP5NavyHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_TMP,						"#GAMEUI_Stat_TMPHits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_MAC10,						"#GAMEUI_Stat_MAC10Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_UMP45,						"#GAMEUI_Stat_UMP45Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_M3,							"#GAMEUI_Stat_M3Hits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_XM1014,						"#GAMEUI_Stat_XM1014Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_M249,						"#GAMEUI_Stat_M249Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_BIZON,						"#GAMEUI_Stat_BIZONHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_MAG7,						"#GAMEUI_Stat_MAG7Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_NEGEV,						"#GAMEUI_Stat_NEGEVHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_SAWEDOFF,					"#GAMEUI_Stat_SAWEDOFFHits",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_TEC9,						"#GAMEUI_Stat_TEC9Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_TASER,						"#GAMEUI_Stat_TASERHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_HKP2000,					"#GAMEUI_Stat_HKP2000Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_MP7,						"#GAMEUI_Stat_MP7Hits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_MP9,						"#GAMEUI_Stat_MP9Hits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_NOVA,						"#GAMEUI_Stat_NovaHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_P250,						"#GAMEUI_Stat_P250Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_SCAR17,						"#GAMEUI_Stat_SCAR17Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_SCAR20,						"#GAMEUI_Stat_SCAR20Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_SG556,						"#GAMEUI_Stat_SG556Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_SSG08,						"#GAMEUI_Stat_SSG08Hits",				CSSTAT_PRIORITY_LOW,			},

	{	CSSTAT_DAMAGE_DEAGLE,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_USP,						nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_GLOCK,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_P228,						nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_ELITE,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_FIVESEVEN,				nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_AWP,						nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_AK47,						nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_M4A1,						nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_AUG,						nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_SG552,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_SG550,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_GALIL,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_GALILAR,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_FAMAS,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_SCOUT,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_G3SG1,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_P90,						nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_MP5NAVY,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_TMP,						nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_MAC10,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_UMP45,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_M3,						nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_XM1014,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_M249,						nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_BIZON,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_MAG7,						nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_NEGEV,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_SAWEDOFF,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_TEC9,						nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_TASER,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_HKP2000,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_MP7,						nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_MP9,						nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_NOVA,						nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_P250,						nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_SCAR17,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_SCAR20,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_SG556,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_SSG08,					nullptr,									CSSTAT_PRIORITY_NEVER,			},

	{	CSSTAT_KILLS_HEADSHOT,					"#GAMEUI_Stat_HeadshotKills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_ENEMY_BLINDED,				"#GAMEUI_Stat_BlindedEnemyKills",		CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_WHILE_BLINDED,				nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_KILLS_WITH_LAST_ROUND,			nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_KILLS_ENEMY_WEAPON,				"#GAMEUI_Stat_EnemyWeaponKills",		CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_KNIFE_FIGHT,				"#GAMEUI_Stat_KnifeFightKills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_WHILE_DEFENDING_BOMB,		nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_KILLS_WITH_STATTRAK_WEAPON,		nullptr,									CSSTAT_PRIORITY_NEVER,			},

	{	CSSTAT_DECAL_SPRAYS,					"#GAMEUI_Stat_DecalSprays",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_TOTAL_JUMPS,						nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_KILLS_WHILE_LAST_PLAYER_ALIVE,	nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_KILLS_ENEMY_WOUNDED,				nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_FALL_DAMAGE,						nullptr,									CSSTAT_PRIORITY_NEVER,			},

	{	CSSTAT_NUM_HOSTAGES_RESCUED,			"#GAMEUI_Stat_NumRescuedHostages",		CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_NUM_BROKEN_WINDOWS,				"#GAMEUI_Stat_NumBrokenWindows",		CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_PROPSBROKEN_ALL,					nullptr,									CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_PROPSBROKEN_MELON,				nullptr,									CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_PROPSBROKEN_OFFICEELECTRONICS,	nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_PROPSBROKEN_OFFICERADIO,			nullptr,									CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_PROPSBROKEN_OFFICEJUNK,			nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_PROPSBROKEN_ITALY_MELON,			nullptr,									CSSTAT_PRIORITY_NEVER,			},

	{	CSSTAT_KILLS_AGAINST_ZOOMED_SNIPER,		"#GAMEUI_Stat_ZoomedSniperKills",		CSSTAT_PRIORITY_HIGH,			},

	{	CSSTAT_WEAPONS_DONATED,					"#GAMEUI_Stat_WeaponsDonated",			CSSTAT_PRIORITY_HIGH,			},

	{	CSSTAT_ITEMS_PURCHASED,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_MONEY_SPENT,						nullptr,									CSSTAT_PRIORITY_LOW,			},

	{	CSSTAT_DOMINATIONS,						"#GAMEUI_Stat_Dominations",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_DOMINATION_OVERKILLS,			"#GAMEUI_Stat_DominationOverkills",		CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_REVENGES,						"#GAMEUI_Stat_Revenges",				CSSTAT_PRIORITY_HIGH,			},

	{	CSSTAT_MVPS,							"#GAMEUI_Stat_MVPs",					CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_CONTRIBUTION_SCORE,				"#GAMEUI_Contribution_Score",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_GG_PROGRESSIVE_CONTRIBUTION_SCORE,"#GAMEUI_GG_Contribution_Score",	CSSTAT_PRIORITY_HIGH,			},
	
	{	CSSTAT_GRENADE_DAMAGE,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_GRENADE_POSTHUMOUSKILLS,			nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_GRENADES_THROWN,					nullptr,									CSSTAT_PRIORITY_NEVER,			},
	{	CSTAT_ITEMS_DROPPED_VALUE,              nullptr,									CSSTAT_PRIORITY_NEVER,			},

	{	CSSTAT_MAP_WINS_CS_ASSAULT,				"#GAMEUI_Stat_WinsMapCSAssault",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_CS_MILITIA,				"#GAMEUI_Stat_WinsMapCSMilitia",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_CS_ITALY,				"#GAMEUI_Stat_WinsMapCSItaly",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_CS_OFFICE,				"#GAMEUI_Stat_WinsMapCSOffice",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_AZTEC,				"#GAMEUI_Stat_WinsMapDEAztec",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_CBBLE,				"#GAMEUI_Stat_WinsMapDECbble",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_DUST2,				"#GAMEUI_Stat_WinsMapDEDust2",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_DUST,				"#GAMEUI_Stat_WinsMapDEDust",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_INFERNO,				"#GAMEUI_Stat_WinsMapDEInferno",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_NUKE,				"#GAMEUI_Stat_WinsMapDENuke",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_PIRANESI,			"#GAMEUI_Stat_WinsMapDEPiranesi",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_PRODIGY,				"#GAMEUI_Stat_WinsMapDEProdigy",		CSSTAT_PRIORITY_ENDROUND,		},

	{	CSSTAT_MAP_WINS_DE_LAKE,				"#GAMEUI_Stat_WinsMapDELake",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_SAFEHOUSE,			"#GAMEUI_Stat_WinsMapDESafeHouse",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_SHORTTRAIN,			"#GAMEUI_Stat_WinsMapShorttrain",		CSSTAT_PRIORITY_ENDROUND,		},	
	{	CSSTAT_MAP_WINS_DE_TRAIN,				"#GAMEUI_Stat_WinsMapTrain",			CSSTAT_PRIORITY_ENDROUND,		},	
	{	CSSTAT_MAP_WINS_DE_SUGARCANE,			"#GAMEUI_Stat_WinsMapDESugarcane",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_STMARC,				"#GAMEUI_Stat_WinsMapDEStMarc",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_BANK,				"#GAMEUI_Stat_WinsMapDEBank",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_EMBASSY,				"#GAMEUI_Stat_WinsMapDEEmbassy",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_DEPOT,				"#GAMEUI_Stat_WinsMapDEDepot",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_VERTIGO,				"#GAMEUI_Stat_WinsMapDEVertigo",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_BALKAN,				"#GAMEUI_Stat_WinsMapDEBalkan",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_AR_MONASTERY,			"#GAMEUI_Stat_WinsMapARMonastery",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_AR_SHOOTS,				"#GAMEUI_Stat_WinsMapARShoots",	    	CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_AR_BAGGAGE,				"#GAMEUI_Stat_WinsMapARBaggage",		CSSTAT_PRIORITY_ENDROUND,		},

	{	CSSTAT_MAP_ROUNDS_CS_MILITIA,			"#GAMEUI_Stat_RoundsMapCSMilitia",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_CS_ASSAULT,			"#GAMEUI_Stat_RoundsMapCSAssault",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_CS_ITALY,				"#GAMEUI_Stat_RoundsMapCSItaly",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_CS_OFFICE,			"#GAMEUI_Stat_RoundsMapCSOffice",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_AZTEC,				"#GAMEUI_Stat_RoundsMapDEAztec",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_CBBLE,				"#GAMEUI_Stat_RoundsMapDECbble",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_DUST2,				"#GAMEUI_Stat_RoundsMapDEDust2",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_DUST,				"#GAMEUI_Stat_RoundsMapDEDust",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_INFERNO,			"#GAMEUI_Stat_RoundsMapDEInferno",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_NUKE,				"#GAMEUI_Stat_RoundsMapDENuke",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_PIRANESI,			"#GAMEUI_Stat_RoundsMapDEPiranesi",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_PRODIGY,			"#GAMEUI_Stat_RoundsMapDEProdigy",		CSSTAT_PRIORITY_ENDROUND,		},

	{	CSSTAT_MAP_ROUNDS_DE_LAKE,				"#GAMEUI_Stat_RoundsMapDELake",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_SAFEHOUSE,			"#GAMEUI_Stat_RoundsMapDESafeHouse",	CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_SHORTTRAIN,		"#GAMEUI_Stat_RoundsMapShorttrain",		CSSTAT_PRIORITY_ENDROUND,		},	
	{	CSSTAT_MAP_ROUNDS_DE_TRAIN,				"#GAMEUI_Stat_RoundsMapTrain",			CSSTAT_PRIORITY_ENDROUND,		},	
	{	CSSTAT_MAP_ROUNDS_DE_SUGARCANE,			"#GAMEUI_Stat_RoundsMapDESugarcane",	CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_STMARC,			"#GAMEUI_Stat_RoundsMapDEStMarc",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_BANK,				"#GAMEUI_Stat_RoundsMapDEBank",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_EMBASSY,			"#GAMEUI_Stat_RoundsMapDEEmbassy",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_DEPOT,				"#GAMEUI_Stat_RoundsMapDEDepot",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_VERTIGO,			"#GAMEUI_Stat_RoundsMapDEVertigo",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_BALKAN,			"#GAMEUI_Stat_RoundsMapDEBalkan",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_AR_MONASTERY,			"#GAMEUI_Stat_RoundsMapARMonastery",	CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_AR_SHOOTS,			"#GAMEUI_Stat_RoundsMapARShoots",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_AR_BAGGAGE,			"#GAMEUI_Stat_RoundsMapAR_Baggage",		CSSTAT_PRIORITY_ENDROUND,		},
	
	// match stats
	{	CSSTAT_MAP_MATCHES_WON_SHOOTS,			"#GAMEUI_Stat_MatchWinsShoots",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_MATCHES_WON_BAGGAGE,			"#GAMEUI_Stat_MatchWinsBaggage",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_MATCHES_WON_LAKE,			"#GAMEUI_Stat_MatchWinsLake",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_MATCHES_WON_SUGARCANE,		"#GAMEUI_Stat_MatchWinsSugarcane",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_MATCHES_WON_STMARC,			"#GAMEUI_Stat_MatchWinsStMarc",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_MATCHES_WON_BANK,			"#GAMEUI_Stat_MatchWinsBank",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_MATCHES_WON_EMBASSY,			"#GAMEUI_Stat_MatchWinsEmbassy",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_MATCHES_WON_DEPOT,			"#GAMEUI_Stat_MatchWinsDepot",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_MATCHES_WON_SAFEHOUSE,		"#GAMEUI_Stat_MatchWinsSafeHouse",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_MATCHES_WON_SHORTTRAIN,		"#GAMEUI_Stat_MatchWinsShorttrain",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_MATCHES_WON_TRAIN,			"#GAMEUI_Stat_MatchWinsTrain",			CSSTAT_PRIORITY_ENDROUND,		},
	
	{	CSSTAT_MATCHES_WON,						"#GAMEUI_Stat_MatchWins",				CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MATCHES_DRAW,					"#GAMEUI_Stat_MatchDraws",				CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MATCHES_PLAYED,					"#GAMEUI_Stat_MatchesPlayed",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_GUN_GAME_MATCHES_WON,			"#GAMEUI_Stat_MatchWinsGG",				CSSTAT_PRIORITY_ENDROUND,		},
	{   CSSTAT_GUN_GAME_MATCHES_PLAYED,			"GAMEUI_Stat_MatchesPlayedGG",			CSSTAT_PRIORITY_ENDROUND		},
	{	CSSTAT_GUN_GAME_PROGRESSIVE_MATCHES_WON,"#GAMEUI_Stat_MatchWinsProgressive",	CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_GUN_GAME_SELECT_MATCHES_WON,		"#GAMEUI_Stat_MatchWinsSelect",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_GUN_GAME_TRBOMB_MATCHES_WON,		"#GAMEUI_Stat_MatchWinsTRBomb",			CSSTAT_PRIORITY_ENDROUND,		},

	// only client tracks these

	{	CSSTAT_LASTMATCH_CONTRIBUTION_SCORE,	"#GameUI_Stat_LastMatch_Contrib_Score",	CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_GG_PROGRESSIVE_CONTRIBUTION_SCORE,"#GameUI_Stat_LastMatch_gg_Contrib_Score",CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_T_ROUNDS_WON,			"#GameUI_Stat_LastMatch_TWins",			CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_CT_ROUNDS_WON,			"#GameUI_Stat_LastMatch_CTWins",		CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_ROUNDS_WON,			"#GameUI_Stat_LastMatch_RoundsWon",		CSSTAT_PRIORITY_NEVER },
	{	CSTAT_LASTMATCH_ROUNDS_PLAYED,			"#GameUI_Stat_LastMatch_RoundsPlayed",	CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_KILLS,					"#GameUI_Stat_LastMatch_Kills",			CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_DEATHS,				"#GameUI_Stat_LastMatch_Deaths",		CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_MVPS,					"#GameUI_Stat_LastMatch_MVPS",			CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_DAMAGE,				"#GameUI_Stat_LastMatch_Damage",		CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_MONEYSPENT,			"#GameUI_Stat_LastMatch_MoneySpent",	CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_DOMINATIONS,			"#GameUI_Stat_LastMatch_Dominations",	CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_REVENGES,				"#GameUI_Stat_LastMatch_Revenges",		CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_MAX_PLAYERS,			"#GameUI_Stat_LastMatch_MaxPlayers",	CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_FAVWEAPON_ID,			"#GameUI_Stat_LastMatch_FavWeapon",		CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_FAVWEAPON_SHOTS,		"#GameUI_Stat_LastMatch_FavWeaponShots",CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_FAVWEAPON_HITS,		"#GameUI_Stat_LastMatch_FavWeaponHits",	CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_FAVWEAPON_KILLS,		"#GameUI_Stat_LastMatch_FavWeaponKills",CSSTAT_PRIORITY_NEVER },

	{	CSSTAT_UNDEFINED	},	// sentinel
};

CSStatProperty CSStatProperty_Table[CSSTAT_MAX];

class StatPropertyTableInitializer
{
public:
	StatPropertyTableInitializer()
	{
		std::memset( CSStatProperty_Table, 0, sizeof( CSStatProperty_Table ) );
		for ( int i = 0; i < std::size( CSStatProperty_Table_Init ); ++i )
		{
			int statId = CSStatProperty_Table_Init[ i ].statId;
			// The last entry in the table_init array is a sentinel. We were using that as
			// a statId causing a 100% buffer underflow, leading to nasty problems.
			if ( statId >= 0 && statId < std::size( CSStatProperty_Table ) )
				CSStatProperty_Table[ statId ] = CSStatProperty_Table_Init[ i ].statProperty;
		}
	}
};

static StatPropertyTableInitializer statPropertyTableInitializer;

const WeaponName_StatId& GetWeaponTableEntryFromWeaponId( CSWeaponID id )
{
	int i;

	//yes this for loop has no statement block. All we are doing is incrementing i to the appropriate point.
	for (i = 0 ; WeaponName_StatId_Table[i].weaponId != WEAPON_NONE ; ++i)
	{
		if (WeaponName_StatId_Table[i].weaponId == id )
		{
			break;
		}
	}
	return WeaponName_StatId_Table[i];
}

void StatsCollection_t::Aggregate( const StatsCollection_t& other )
{
	for ( int i = 0; i < CSSTAT_MAX; ++i )
	{
		m_iValue[i] += other[i];
	}
}

void StatsCollection_t::Reset()
{
	for ( int i = 0; i < std::size( m_iValue ); i++ )
	{
		m_iValue[i] = 0;
	}
}

void RoundStatsDirectAverage_t::Reset()
{
	for ( int i = 0; i < std::size( m_fStat ); i++ )
	{
		m_fStat[i] = 0;
	}
}

void RoundStatsRollingAverage_t::Reset()
{
	for ( int i = 0; i < std::size( m_fStat ); i++ )
	{
		m_fStat[i] = 0;
	}
	m_numberOfDataSets = 0;
}

void RoundStatsRollingAverage_t::RollDataSetIntoAverage( const RoundStatsRollingAverage_t &other)
{
	for ( int i = 0; i < std::size( m_fStat ); i++ )
	{
		m_fStat[i] *= m_numberOfDataSets;
		m_fStat[i] += other.m_fStat[i];
		m_fStat[i] /= (m_numberOfDataSets + 1);
	}
	m_numberOfDataSets++;
}

void KillStats_t::Reset()
{
	V_memset( iNumKilled, 0, sizeof( iNumKilled ) );
	V_memset( iNumKilledBy, 0, sizeof( iNumKilledBy ) );
	V_memset( iNumKilledByUnanswered, 0, sizeof( iNumKilledByUnanswered ) );
}

void PlayerStats_t::Reset()
{
	statsDelta.Reset();
	statsCurrentRound.Reset();
	statsCurrentMatch.Reset();
	statsKills.Reset();
}

PlayerStats_t::PlayerStats_t( const PlayerStats_t &other)
{
	statsDelta			= other.statsDelta;
	statsCurrentRound	= other.statsCurrentRound;
	statsCurrentMatch	= other.statsCurrentMatch;
}

class CBaseEntity;
class IGameSystem;

using namespace ::ks::reflect;

class CCSHLTVDirector : public CHLTVDirector
{
public:
	DECLARE_CLASS( CCSHLTVDirector, CHLTVDirector );

	const char** GetModEvents();
	void AddHLTVServer( IHLTVServer *hltv );
	void CreateShotFromEvent( const CHLTVGameEvent *event );

};

void CCSHLTVDirector::AddHLTVServer( IHLTVServer *hltv )
{
	BaseClass::AddHLTVServer( hltv );

	// mod specific events the director uses to find interesting shots
	ListenForGameEvent( "hostage_rescued" );
	ListenForGameEvent( "hostage_killed" );
	ListenForGameEvent( "hostage_hurt" );
	ListenForGameEvent( "hostage_follows" );
	ListenForGameEvent( "bomb_pickup" );
	ListenForGameEvent( "bomb_dropped" );
	ListenForGameEvent( "bomb_exploded" );
	ListenForGameEvent( "bomb_defused" );
	ListenForGameEvent( "bomb_planted" );
	ListenForGameEvent( "bomb_begindefuse" );
	ListenForGameEvent( "bomb_beginplant" );
	ListenForGameEvent( "vip_escaped" );
	ListenForGameEvent( "vip_killed" );
}

void CCSHLTVDirector::CreateShotFromEvent( const CHLTVGameEvent *event )
{
	// show event at least for 2 more seconds after it occured
	const char *name = event->m_Event->GetName();
	IGameEvent *shot = nullptr;

	CBaseEntity *player = nullptr;

	if ( !V_strcmp( "hostage_rescued", name ) ||
		 !V_strcmp( "hostage_hurt", name ) ||
		 !V_strcmp( "hostage_follows", name ) ||
		 !V_strcmp( "hostage_killed", name ) )
	{
		player = UTIL_PlayerByUserId( event->m_Event->GetInt("userid") );

		if ( !player )
			return;

		// shot player as primary, hostage as secondary target
		shot = gameeventmanager->CreateEvent( "hltv_chase", true );
		shot->SetInt( "target1", player->entindex() );
		shot->SetInt( "target2", event->m_Event->GetInt("hostage") );
		shot->SetFloat( "distance", 96.0f );
		shot->SetInt( "theta", 40 );
		shot->SetInt( "phi", 20 );

		// shot 2 seconds after event
		m_nNextShotTick = std::min( m_nNextShotTick, (event->m_Tick+TIME_TO_TICKS(2.0)) );
		m_iPVSEntity = player->entindex();
	}

	else if (	!V_strcmp( "bomb_beginplant", name ) ||
				!V_strcmp( "bomb_begindefuse", name ) )
	{

		player = UTIL_PlayerByUserId( event->m_Event->GetInt("userid") );

		if ( !player )
			return;

		// chasecam
		shot = gameeventmanager->CreateEvent( "hltv_chase", true );

		if ( shot )
		{
			shot->SetInt( "target1", player->entindex() );
			shot->SetInt( "target2", 0 );
			shot->SetFloat( "distance", 500.0f );
			shot->SetInt( "theta", 180 );
			shot->SetInt( "phi", 45 );
			shot->SetBool( "ineye", true );

			// shot 3 seconds after pickup
			m_nNextShotTick = std::min( m_nNextShotTick, (event->m_Tick+TIME_TO_TICKS(3.0)) );
			m_iPVSEntity = player->entindex();

			for ( int i = 0; i < std::ssize( m_HltvServers ); ++i )
			{
				m_HltvServers[ i ].m_pHLTVServer->BroadcastEvent( shot );
			}
			gameeventmanager->FreeEvent( shot );
			DevMsg("DrcCmd: %s\n", name );

			return;
		}
	}
	
	// let baseclass create a shot
	BaseClass::CreateShotFromEvent( event );	
}

const char** CCSHLTVDirector::GetModEvents()
{
	// game events relayed to spectator clients
	static const char *s_modevents[] =
	{
		"hltv_status",
		"hltv_chat",
		"player_connect",
		"player_connect_full",
		"player_disconnect",
		"player_team",
		"player_info",
		"server_cvar",
		"player_changename",
		"teamplay_broadcast_audio",
		"player_death",
		"other_death",
		"player_hurt",
		"player_chat",
		"round_start",
		"round_end",
		// additional CS:S events:
		"bomb_planted",	
		"bomb_defused",
		"bomb_beginplant",	
		"bomb_begindefuse",
		"hostage_killed",
		"hostage_hurt",
		"begin_new_match",
		// UI events
		"cs_match_end_restart",
		"cs_game_disconnected",
		"announce_phase_end", 	
		"round_mvp",	
		"server_spawn",
		"player_spawn",
		"hltv_status",
		"cs_win_panel_round",
		"endmatch_cmm_start_reveal_items",
		"game_newmap",
		"hostage_rescued",
		"bomb_exploded",
		"bomb_pickup",
		"bomb_dropped",
		"defuser_pickup",
		"defuser_dropped",
		"decoy_started",
		"decoy_detonate",
		"hegrenade_detonate",
		"flashbang_detonate",
		"smokegrenade_detonate",
		"smokegrenade_expired",
		"inferno_startburn",
		"inferno_expire",
		"bot_takeover",
		"bomb_beep",
		"weapon_fire",
		"weapon_fire_on_empty",
		"weapon_outofammo",
		"weapon_reload",
		"weapon_zoom",
		"player_footstep",
		"player_jump",
		"player_blind",
		"round_freeze_end",
		"cs_win_panel_match",			
		"cs_pre_restart",
		"tournament_reward",
		"item_found",
		"items_gifted",
		"round_announce_warmup",
		"round_announce_last_round_half",
		"round_announce_final",
		"round_announce_match_point",
		"round_poststart",
		"buytime_ended",
		"round_time_warning",
		"dm_bonus_weapon_start",
		"endmatch_mapvote_selecting_map",
		"cs_round_start_beep",
		"cs_round_final_beep",
		"round_announce_match_start",
		"seasoncoin_levelup",
		"player_falldamage",
		"hostage_rescued_all",
		"round_officially_ended",
		"round_prestart",
		nullptr
	};

	return s_modevents;
}

static CCSHLTVDirector s_HLTVDirector;	// singleton

static InterfaceReg s_Reg_CHLTVDirector_IHLTVDirector( side_registry(), []() -> void * { return static_cast<IHLTVDirector *>( &s_HLTVDirector ); }, INTERFACEVERSION_HLTVDIRECTOR );;

CHLTVDirector* HLTVDirector()
{
	return &s_HLTVDirector;
}

IGameSystem* HLTVDirectorSystem()
{
	return &s_HLTVDirector;
}

using namespace ::ks::reflect;

// Purpose: Stub for item inventory (econ system removed)

//-----------------------------------------------------------------------------
CCSInventoryManager g_CSInventoryManager;
CInventoryManager *InventoryManager( void )
{
	return &g_CSInventoryManager;
}
CCSInventoryManager *CSInventoryManager( void )
{
	return &g_CSInventoryManager;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CCSInventoryManager::CCSInventoryManager( void )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSInventoryManager::PostInit( void )
{
	BaseClass::PostInit();
	GenerateBaseItems();
}

//-----------------------------------------------------------------------------
// Purpose: Generate & store the base item details for each class & loadout slot
//-----------------------------------------------------------------------------
void CCSInventoryManager::GenerateBaseItems( void )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CCSPlayerInventory *CCSInventoryManager::GetBagForPlayer( CPlayerId &playerID, inventory_bags_t iBag )
{
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the item data for the base item in the loadout slot for a given class
//-----------------------------------------------------------------------------
CScriptCreatedItem *CCSInventoryManager::GetBaseItemForClass( int iSlot )
{
	if ( iSlot < 0 || iSlot >= LOADOUT_POSITION_COUNT )
		return nullptr;
	return &m_pBaseLoadoutItems[iSlot];
}

// GetBaseItemForTeam, GetItemInLoadoutForClass, GetItemInLoadoutForTeam
// are now inline stubs in cs_item_inventory.h

//=======================================================================================================================
// CS PLAYER INVENTORY
//=======================================================================================================================
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CCSPlayerInventory::CCSPlayerInventory()
{
	m_iBag = BAG_ALL_ITEMS;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSPlayerInventory::InventoryReceived( EItemRequestResult eResult, int iItems )
{
	BaseClass::InventoryReceived( eResult, iItems );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSPlayerInventory::DumpInventoryToConsole( bool bRoot )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSPlayerInventory::ItemShouldBeIncluded( int iItemPosition )
{
	return BaseClass::ItemShouldBeIncluded( iItemPosition );
}

class CNavArea;
class CNavLadder;
class HidingSpot;

using namespace ::ks::reflect;

// nav_area.cpp
// AI Navigation areas
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

//--------------------------------------------------------------------------------------------------------------
// Counter-Strike extensions of CNavArea
//--------------------------------------------------------------------------------------------------------------

void CNavArea::Save( ks::byte_writer &fileBuffer, unsigned int version ) const
{
	SaveBase( fileBuffer, version );

	//
	// Save the approach areas for this area
	//

	// save number of approach areas
	fileBuffer.write< unsigned char >( m_approachCount);

	// save approach area info
	for( int a=0; a<m_approachCount; ++a )
	{
		if (m_approach[a].here.area)
			fileBuffer.write< unsigned int >( m_approach[a].here.area->GetID());
		else
			fileBuffer.write< unsigned int >( 0);

		if (m_approach[a].prev.area)
			fileBuffer.write< unsigned int >( m_approach[a].prev.area->GetID());
		else
			fileBuffer.write< unsigned int >( 0);
		fileBuffer.write< unsigned char >( m_approach[a].prevToHereHow);

		if (m_approach[a].next.area)
			fileBuffer.write< unsigned int >( m_approach[a].next.area->GetID());
		else
			fileBuffer.write< unsigned int >( 0);
		fileBuffer.write< unsigned char >( m_approach[a].hereToNextHow);
	}
}

NavErrorType CNavArea::Load( ks::byte_reader &fileBuffer, unsigned int version, unsigned int subVersion )
{
	if ( version < 15 )
		return LoadLegacy(fileBuffer, version, subVersion);

	// load base class data
	NavErrorType error = LoadBase( fileBuffer, version, subVersion );

	switch ( subVersion )
	{
	case 1:
		//
		// Load number of approach areas
		//
		m_approachCount = fileBuffer.read< unsigned char >();

		// load approach area info (IDs)
		for( int a = 0; a < m_approachCount; ++a )
		{
			m_approach[a].here.id = fileBuffer.read< unsigned int >();

			m_approach[a].prev.id = fileBuffer.read< unsigned int >();
			m_approach[a].prevToHereHow = (NavTraverseType)fileBuffer.read< unsigned char >();

			m_approach[a].next.id = fileBuffer.read< unsigned int >();
			m_approach[a].hereToNextHow = (NavTraverseType)fileBuffer.read< unsigned char >();
		}

		if ( !fileBuffer.ok() )
			error = NAV_INVALID_FILE;

		// fall through

	case 0:
		// legacy version
		break;

	default:
		Warning( "Unknown NavArea sub-version number\n" );
		error = NAV_INVALID_FILE;
	}

	return error;
}

NavErrorType CNavArea::PostLoad( void )
{
	NavErrorType error = PostLoadBase();

	// resolve approach area IDs
	for ( int a = 0; a < m_approachCount; ++a )
	{
		m_approach[a].here.area = TheNavMesh->GetNavAreaByID( m_approach[a].here.id );
		if (m_approach[a].here.id && m_approach[a].here.area == nullptr)
		{
			Msg( "CNavArea::PostLoad: Corrupt navigation data. Missing Approach Area (here).\n" );
			error = NAV_CORRUPT_DATA;
		}

		m_approach[a].prev.area = TheNavMesh->GetNavAreaByID( m_approach[a].prev.id );
		if (m_approach[a].prev.id && m_approach[a].prev.area == nullptr)
		{
			Msg( "CNavArea::PostLoad: Corrupt navigation data. Missing Approach Area (prev).\n" );
			error = NAV_CORRUPT_DATA;
		}

		m_approach[a].next.area = TheNavMesh->GetNavAreaByID( m_approach[a].next.id );
		if (m_approach[a].next.id && m_approach[a].next.area == nullptr)
		{
			Msg( "CNavArea::PostLoad: Corrupt navigation data. Missing Approach Area (next).\n" );
			error = NAV_CORRUPT_DATA;
		}
	}
	return error;
}

void CNavArea::CustomAnalysis( bool isIncremental /*= false */ )
{
	ComputeApproachAreas();
}

//--------------------------------------------------------------------------------------------------------------
// return danger decay rate per second
float CNavArea::GetDangerDecayRate( void ) const
{
	if ( CSGameRules()->IsPlayingGunGameTRBomb() )
	{
		// decay danger faster in this aggressive bomb planting mode
		return 1.0f / 30.0f;
	}

	// one kill == 1.0, which we will forget about in two minutes
	return 1.0f / 120.0f;
}

float CNavArea::GetEarliestOccupyTime( int teamID ) const
{
	if ( CSGameRules()->IsPlayingCoopMission() )
		return 0.0f;

	return GetEarliestOccupyTimeBase( teamID );
}

void CNavArea::UpdateBlocked( bool force /*= false*/, int teamID /*= TEAM_ANY */ )
{
	if ( CSGameRules() && CSGameRules()->IsPlayingCoopMission() )
		UpdateBlockedBase( force, teamID );
}

bool CNavArea::IsBlocked( int teamID, bool ignoreNavBlockers /*= false */ ) const
{
	return ( CSGameRules() && CSGameRules()->IsPlayingCoopMission() ) ? IsBlockedBase( teamID, ignoreNavBlockers ) : false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Load legacy navigation area from the file
 */
NavErrorType CNavArea::LoadLegacy( ks::byte_reader &fileBuffer, unsigned int version, unsigned int subVersion )
{
	// load ID
	m_id = fileBuffer.read< unsigned int >();

	// update nextID to avoid collisions
	if (m_id >= m_nextID)
		m_nextID = m_id+1;

	// load attribute flags
	if ( version <= 8 )
	{
		m_attributeFlags = fileBuffer.read< unsigned char >();
	}
	else if ( version < 13 )
	{
		m_attributeFlags = fileBuffer.read< unsigned short >();
	}
	else
	{
		m_attributeFlags = fileBuffer.read< int >();
	}

	// load extent of area
	fileBuffer.read_bytes( &m_nwCorner, 3*sizeof(float) );
	fileBuffer.read_bytes( &m_seCorner, 3*sizeof(float) );

	if ( ( m_seCorner.x - m_nwCorner.x ) > 0.0f && ( m_seCorner.y - m_nwCorner.y ) > 0.0f )
	{
		m_invDxCorners = 1.0f / ( m_seCorner.x - m_nwCorner.x );
		m_invDyCorners = 1.0f / ( m_seCorner.y - m_nwCorner.y );
	}
	else
	{
		m_invDxCorners = m_invDyCorners = 0;

		DevWarning( "Degenerate Navigation Area #%d at setpos %g %g %g\n", 
			m_id, m_nwCorner.x, m_nwCorner.y, m_nwCorner.z );
	}

	// load heights of implicit corners
	m_neZ = fileBuffer.read< float >();
	m_swZ = fileBuffer.read< float >();

	CheckWaterLevel();

	// load connections (IDs) to adjacent areas
	// in the enum order NORTH, EAST, SOUTH, WEST
	for( int d=0; d<NUM_DIRECTIONS; d++ )
	{
		// load number of connections for this direction
		unsigned int count = fileBuffer.read< unsigned int >();
		contract_assert( fileBuffer.ok() );

		m_connect[d].reserve( count );
		for( unsigned int i=0; i<count; ++i )
		{
			NavConnect connect;
			connect.id = fileBuffer.read< unsigned int >();
			contract_assert( fileBuffer.ok() );

			// don't allow self-referential connections
			if ( connect.id != m_id )
			{
				m_connect[d].push_back( connect );
			}
		}
	}

	//
	// Load hiding spots
	//

	// load number of hiding spots
	unsigned char hidingSpotCount = fileBuffer.read< unsigned char >();

	if (version == 1)
	{
		// load simple vector array
		Vector pos;
		for( int h=0; h<hidingSpotCount; ++h )
		{
			fileBuffer.read_bytes( &pos, 3 * sizeof(float) );

			// create new hiding spot and put on master list
			HidingSpot *spot = TheNavMesh->CreateHidingSpot();
			spot->SetPosition( pos );
			spot->SetFlags( HidingSpot::IN_COVER );
			m_hidingSpots.push_back( spot );
		}
	}
	else
	{
		// load HidingSpot objects for this area
		for( int h=0; h<hidingSpotCount; ++h )
		{
			// create new hiding spot and put on master list
			HidingSpot *spot = TheNavMesh->CreateHidingSpot();

			spot->Load( fileBuffer, version );
			
			m_hidingSpots.push_back( spot );
		}
	}

	if ( version < 15 )
	{
		//
		// Load number of approach areas
		//
		m_approachCount = fileBuffer.read< unsigned char >();

		// load approach area info (IDs)
		for( int a = 0; a < m_approachCount; ++a )
		{
			m_approach[a].here.id = fileBuffer.read< unsigned int >();

			m_approach[a].prev.id = fileBuffer.read< unsigned int >();
			m_approach[a].prevToHereHow = (NavTraverseType)fileBuffer.read< unsigned char >();

			m_approach[a].next.id = fileBuffer.read< unsigned int >();
			m_approach[a].hereToNextHow = (NavTraverseType)fileBuffer.read< unsigned char >();
		}
	}

	//
	// Load encounter paths for this area
	//
	unsigned int count = fileBuffer.read< unsigned int >();

	if (version < 3)
	{
		// old data, read and discard
		for( unsigned int e=0; e<count; ++e )
		{
			SpotEncounter encounter;

			encounter.from.id = fileBuffer.read< unsigned int >();
			encounter.to.id = fileBuffer.read< unsigned int >();

			fileBuffer.read_bytes( &encounter.path.from.x, 3 * sizeof(float) );
			fileBuffer.read_bytes( &encounter.path.to.x, 3 * sizeof(float) );

			// read list of spots along this path
			unsigned char spotCount = fileBuffer.read< unsigned char >();
		
			for( int s=0; s<spotCount; ++s )
			{
				fileBuffer.read< float >();
				fileBuffer.read< float >();
				fileBuffer.read< float >();
				fileBuffer.read< float >();
			}
		}
		return NAV_OK;
	}

	for( unsigned int e=0; e<count; ++e )
	{
		auto encounter = std::make_unique< SpotEncounter >();

		encounter->from.id = fileBuffer.read< unsigned int >();

		unsigned char dir = fileBuffer.read< unsigned char >();
		encounter->fromDir = static_cast<NavDirType>( dir );

		encounter->to.id = fileBuffer.read< unsigned int >();

		dir = fileBuffer.read< unsigned char >();
		encounter->toDir = static_cast<NavDirType>( dir );

		// read list of spots along this path
		unsigned char spotCount = fileBuffer.read< unsigned char >();
	
		SpotOrder order;
		for( int s=0; s<spotCount; ++s )
		{
			order.id = fileBuffer.read< unsigned int >();

			unsigned char t = fileBuffer.read< unsigned char >();

			order.t = (float)t/255.0f;

			encounter->spots.push_back( order );
		}

		m_spotEncounters.push_back( std::move( encounter ) );
	}

	if (version < 5)
		return NAV_OK;

	//
	// Load Place data
	//
	PlaceDirectory::IndexType entry = fileBuffer.read< unsigned short >();

	// convert entry to actual Place
	SetPlace( placeDirectory.IndexToPlace( entry ) );

	if ( version < 7 )
		return NAV_OK;

	// load ladder data
	for ( int dir=0; dir<CNavLadder::NUM_LADDER_DIRECTIONS; ++dir )
	{
		count = fileBuffer.read< unsigned int >();
		for( unsigned int i=0; i<count; ++i )
		{
			NavLadderConnect connect;
			connect.id = fileBuffer.read< unsigned int >();

			bool alreadyConnected = false;
			for ( const auto &ladderConnect : m_ladder[dir] )
			{
				if ( ladderConnect.id == connect.id )
				{
					alreadyConnected = true;
					break;
				}
			}

			if ( !alreadyConnected )
			{
				m_ladder[dir].push_back( connect );
			}
		}
	}

	if ( version < 8 )
		return NAV_OK;

	// load earliest occupy times
	for( int i=0; i<MAX_NAV_TEAMS; ++i )
	{
		// no spot in the map should take longer than this to reach
		m_earliestOccupyTime[i] = fileBuffer.read< float >();
	}

	if ( version < 11 )
		return NAV_OK;

	// load light intensity
	for ( int i=0; i<NUM_CORNERS; ++i )
	{
		m_lightIntensity[i] = fileBuffer.read< float >();
	}

	if ( version < 16 )
		return NAV_OK;

	// load visibility information
	unsigned int visibleAreaCount = fileBuffer.read< unsigned int >();

	m_potentiallyVisibleAreas.reserve( visibleAreaCount );

	for( unsigned int j=0; j<visibleAreaCount; ++j )
	{
		AreaBindInfo info;
		info.id = fileBuffer.read< unsigned int >();
		info.attributes = fileBuffer.read< unsigned char >();

		m_potentiallyVisibleAreas.push_back( info );
	}

	// read area from which we inherit visibility
	m_inheritVisibilityFrom.id = fileBuffer.read< unsigned int >();

	return NAV_OK;
}

class CNavArea;
class CNavLadder;
struct Extent;

using namespace ::ks::reflect;

// nav_generate.cpp
// Auto-generate a Navigation Mesh by sampling the current map
// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

enum { MAX_BLOCKED_AREAS = 256 };
static unsigned int blockedID[ MAX_BLOCKED_AREAS ];
static int blockedIDCount = 0;
static float lastMsgTime = 0.0f;

//ConVar nav_slope_limit( "nav_slope_limit", "0.7", FCVAR_GAMEDLL, "The ground unit normal's Z component must be greater than this for nav areas to be generated." );
ConVar nav_restart_after_analysis( side_registry(), "nav_restart_after_analysis", "1", FCVAR_GAMEDLL, "When nav nav_restart_after_analysis finishes, restart the server.  Turning this off can cause crashes, but is useful for incremental generation." );

//--------------------------------------------------------------------------------------------------------------
/**
 * Shortest path cost, paying attention to "blocked" areas
 */
class ApproachAreaCost
{
public:
	// HPE_TODO[pmf]: check that these new parameters are okay to be ignored
	float operator() ( CNavArea *area, CNavArea *fromArea, const CNavLadder *ladder, const CFuncElevator *elevator, float length )
	{
		// check if this area is "blocked"
		for( int i=0; i<blockedIDCount; ++i )
		{
			if (area->GetID() == blockedID[i])
			{
				return -1.0f;
			}
		}

		if (fromArea == nullptr)
		{
			// first area in path, no cost
			return 0.0f;
		}
		else
		{
			// compute distance traveled along path so far
			float dist;

			if (ladder)
			{
				dist = ladder->m_length;
			}
			else
			{
				dist = (area->GetCenter() - fromArea->GetCenter()).Length();
			}

			float cost = dist + fromArea->GetCostSoFar();

			return cost;
		}
	}
};

/*
 * Determine the set of "approach areas".
 * An approach area is an area representing a place where players 
 * move into/out of our local neighborhood of areas.
 * @todo Optimize by search from eye outward and modifying pathfinder to treat all links as bi-directional
 */
void CNavArea::ComputeApproachAreas( void )
{
	m_approachCount = 0;

	if (nav_quicksave.GetBool())
		return;

	// use the center of the nav area as the "view" point
	Vector eye = GetCenter();
	if (TheNavMesh->GetGroundHeight( eye, &eye.z ) == false)
		return;

	// approximate eye position
	if (GetAttributes() & NAV_MESH_CROUCH)
		eye.z += 0.9f * HalfHumanHeight;
	else
		eye.z += 0.9f * HumanHeight;

	enum { MAX_PATH_LENGTH = 256 };
	CNavArea *path[ MAX_PATH_LENGTH ];
	ApproachAreaCost cost;

	enum SearchType
	{
		FROM_EYE,		///< start search from our eyepoint outward to farArea
		TO_EYE,			///< start search from farArea beack towards our eye
		SEARCH_FINISHED
	};

	//
	// In order to *completely* enumerate all of the approach areas, we
	// need to search from our eyepoint outward, as well as from outwards
	// towards our eyepoint
	//
	for( int searchType = FROM_EYE; searchType != SEARCH_FINISHED; ++searchType )
	{
		//
		// In order to enumerate all of the approach areas, we need to
		// run the algorithm many times, once for each "far away" area
		// and keep the union of the approach area sets
		//
		int it;
		for( it = 0; it < std::ssize( TheNavAreas ); ++it )
		{
			CNavArea *farArea = TheNavAreas[ it ];

			blockedIDCount = 0;

			// skip the small areas
			const float minSize = 200.0f;		// 150
			Extent extent;
			farArea->GetExtent(&extent);
			if (extent.SizeX() < minSize || extent.SizeY() < minSize)
			{
				continue;
			}

			// if we can see 'farArea', try again - the whole point is to go "around the bend", so to speak
			if (farArea->IsVisible( eye ))
			{
				continue;
			}

			//
			// Keep building paths to farArea and blocking them off until we
			// cant path there any more.
			// As areas are blocked off, all exits will be enumerated.
			//
			while( m_approachCount < MAX_APPROACH_AREAS )
			{
				CNavArea *from, *to;

				if (searchType == FROM_EYE)
				{
					// find another path *to* 'farArea'
					// we must pathfind from us in order to pick up one-way paths OUT OF our area
					from = this;
					to = farArea;
				}
				else // TO_EYE
				{
					// find another path *from* 'farArea'
					// we must pathfind to us in order to pick up one-way paths INTO our area
					from = farArea;
					to = this;
				}

				// build the actual path
				if (NavAreaBuildPath( from, to, nullptr, cost ) == false)
				{
					break;
				}

				// find number of areas on path
				int count = 0;
				CNavArea *area;
				for( area = to; area; area = area->GetParent() )
				{
					++count;
				}

				if (count > MAX_PATH_LENGTH)
				{
					count = MAX_PATH_LENGTH;
				}

				// if the path is only two areas long, there can be no approach points
				if (count <= 2)
				{
					break;
				}

				// build path starting from eye
				int i = 0;

				if (searchType == FROM_EYE)
				{
					for( area = to; i < count && area; area = area->GetParent() )
					{
						path[ count-i-1 ] = area;
						++i;
					}
				}
				else // TO_EYE
				{
					for( area = to; i < count && area; area = area->GetParent() )
					{
						path[ i++ ] = area;
					}
				}

				// traverse path to find first area we cannot see (skip the first area)
				for( i=1; i<count; ++i )
				{
					// if we see this area, continue on
					if (path[i]->IsVisible( eye ))
					{
						continue;
					}

					// we can't see this area - mark this area as "blocked" and unusable by subsequent approach paths
					if (blockedIDCount == MAX_BLOCKED_AREAS)
					{
						Msg( "Overflow computing approach areas for area #%d.\n", GetID());
						return;
					}

					// if the area to be blocked is actually farArea, block the one just prior
					// (blocking farArea will cause all subsequent pathfinds to fail)
					int block = (path[i] == farArea) ? i-1 : i;

					// dont block the start area, or all subsequence pathfinds will fail
					if (block == 0)
					{
						continue;
					}

					blockedID[ blockedIDCount++ ] = path[ block ]->GetID();

					// store new approach area if not already in set
					int a;
					for( a=0; a<m_approachCount; ++a )
					{
						if (m_approach[a].here.area == path[block-1])
						{
							break;
						}
					}

					if (a == m_approachCount)
					{
						m_approach[ m_approachCount ].prev.area = (block >= 2) ? path[block-2] : nullptr;

						m_approach[ m_approachCount ].here.area = path[block-1];
						m_approach[ m_approachCount ].prevToHereHow = path[block-1]->GetParentHow();

						m_approach[ m_approachCount ].next.area = path[block];
						m_approach[ m_approachCount ].hereToNextHow = path[block]->GetParentHow();

						++m_approachCount;
					}

					// we are done with this path
					break;
				}
			}
		}
	}
}

class CBaseEntity;
class CCSBotManager;
class CNavArea;
class CNavMesh;

using namespace ::ks::reflect;

// NavMesh.cpp
// Implementation of Navigation Mesh interface
// Author: Michael S. Booth, 2003-2004

extern ConVar showtriggers;
class CFuncBlockDMSpawns : public CFuncBrush
{
	DECLARE_CLASS( CFuncBlockDMSpawns, CFuncBrush );

	public: void Spawn_Impl( void )
	{
		SetSolid( SOLID_BSP );
		AddSolidFlags( FSOLID_NOT_SOLID );

		SetModel( STRING( GetModelName() ) );    // set size and link into world

		if ( !showtriggers.GetInt() )
		{
			AddEffects( EF_NODRAW );
		}
	}
	private:
};

LINK_ENTITY_TO_CLASS( func_block_dm_spawns, CFuncBlockDMSpawns );

extern ConVar mp_randomspawn;
ConVar mp_guardian_target_site( side_registry(), "mp_guardian_target_site", "-1", FCVAR_RELEASE | FCVAR_GAMEDLL, "If set to the index of a bombsite, will cause random spawns to be only created near that site." );

//--------------------------------------------------------------------------------------------------------------
// Counter-Strike extensions of CNavMesh

//--------------------------------------------------------------------------------------------------------------
/**
 * Reset the Navigation Mesh to initial values
 */
void CNavMesh::Reset( void )
{
	m_refreshChickenTimer.Start( 5.0f );
	m_refreshDMSpawnTimer.Start( 1.0f );

	ResetBase();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Zero player counts in all areas
 */
void CNavMesh::ClearPlayerCounts( void )
{
	for ( CNavArea *pNavArea : TheNavAreas )
	{
		CNavArea *area = pNavArea;
		area->ClearPlayerCount();
	}
}

//--------------------------------------------------------------------------------------------------------------
// Keep desired number of chickens alive in map
void CNavMesh::MaintainChickenPopulation( void )
{
	if ( m_desiredChickenCount > 0 && m_refreshChickenTimer.IsElapsed() )
	{
		m_refreshChickenTimer.Start( RandomFloat( 10.0f, 20.0f ) );

		int actualCount = 0;
		for( int i=0; i<std::ssize( m_chickenVector ); ++i )
		{
			if ( m_chickenVector[i] != nullptr )
			{
				++actualCount;
			}
		}

		if ( actualCount < m_desiredChickenCount )
		{
			int need = m_desiredChickenCount - actualCount;

			for( int k=0; k<need; ++k )
			{
				// find a good spot to spawn a chicken
				CBaseEntity *chicken = nullptr;

				for( int attempts=0; attempts<10; ++attempts )
				{
					int which = RandomInt( 0, std::ssize( TheNavAreas )-1 );

					CNavArea *testArea = TheNavAreas[ which ];

					const float tooSmall = 50.0f;

					if ( testArea && testArea->GetSizeX() > tooSmall && testArea->GetSizeY() > tooSmall )
					{
						if ( !UTIL_IsVisibleToTeam( testArea->GetCenter(), TEAM_CT ) &&
							 !UTIL_IsVisibleToTeam( testArea->GetCenter(), TEAM_TERRORIST ) )
						{
							// don't spawn a chicken on top of another chicken
							int n;
							for( n=0; n<std::ssize( m_chickenVector ); ++n )
							{
								if ( m_chickenVector[n] == nullptr )
									continue;

								const float tooClose = 50.0f;
								Vector between = m_chickenVector[n]->GetAbsOrigin() - testArea->GetCenter();
								if ( between.IsLengthLessThan( tooClose ) )
									break;
							}

							if ( n >= std::ssize( m_chickenVector ) )
							{
								// found a good spot - spawn a chicken here
								chicken = CreateEntityByName( "chicken" );
								if ( chicken )
								{
									chicken->SetAbsOrigin( testArea->GetCenter() );

									DispatchSpawn( chicken );
									m_chickenVector.push_back( chicken );
								}

								break;
							}
						}
					}
				}

				if ( !chicken )
				{
					// couldn't spawn a chicken - try again later
					return;
				}
			}
		}
	}
}

NavErrorType CNavMesh::PostLoad( unsigned int version )
{
	if ( CSGameRules()->IsPlayingGunGameDeathmatch() )
		m_desiredChickenCount = 10;
	else
		m_desiredChickenCount = g_pMapInfo ? g_pMapInfo->m_iPetPopulation : 0;
	m_chickenVector.clear();

	if ( CSGameRules() && CSGameRules()->IsPlayingCooperativeGametype() )
	{
		// in Guardian mode, remove all of the mapper placed DM spawn points
		CBaseEntity *pEntity = nullptr;
		while ( ( pEntity = gEntList.FindEntityByClassname( pEntity, "info_deathmatch_spawn" ) ) != nullptr )
		{
			UTIL_Remove( pEntity );
		}
	}

	ResetDMSpawns();

	return PostLoadBase(version);
}

static void dm_reset_spawns( const CCommand &args );
static ConCommand dm_reset_spawns_command( side_registry(), "dm_reset_spawns", dm_reset_spawns, "" );
static void dm_reset_spawns( const CCommand &args )
{
	TheNavMesh->ResetDMSpawns();
	DevMsg( "Manually resetting deathmatch spawns.\n");

}

void CNavMesh::ResetDMSpawns( void )
{
	for ( auto &hDMSpawn : m_DMSpawnVector )
	{
		UTIL_Remove( hDMSpawn );
	}

	m_desiredDMSpawns = 50;
	m_consecutiveFailedAttempts = 0;
	m_DMSpawnVector.clear();
	MaintainDMSpawnPopulation();
}

void CNavMesh::MaintainDMSpawnPopulation( void )
{
	if ( mp_randomspawn.GetInt() == 0 )
		return;

	bool bDisableAutoGeneratedDMSpawns = ( g_pMapInfo ? g_pMapInfo->m_bDisableAutoGeneratedDMSpawns : false );

	// HACK: Force this to run in guardian, even if the map explicitly placed dm spawns
	if ( bDisableAutoGeneratedDMSpawns && !CSGameRules()->IsPlayingCoopGuardian() )
		return;

	if ( m_desiredDMSpawns > 0 && m_refreshDMSpawnTimer.IsElapsed() )
	{
		m_refreshDMSpawnTimer.Start( 1.0f );

		int actualCount = 0;
		for( int i=0; i<std::ssize( m_DMSpawnVector ); ++i )
		{
			if ( m_DMSpawnVector[i] != nullptr )
			{
				++actualCount;
			}
		}

		// If the map isn't large enough to support as many spawns as m_desiredDMSpawns then shrink m_desiredDMSpawns to whatever we can get.
		if ( m_consecutiveFailedAttempts >= 100 )
		{
			m_desiredDMSpawns = actualCount;

			m_consecutiveFailedAttempts = 0;

			Warning( "Giving up attempts to make more random spawn points. Current count: %i.\n", actualCount );

		}

		if ( actualCount < m_desiredDMSpawns )
		{
//			float calctime = Plat_FloatTime(); MEASURE PERF 1/2

			int need = m_desiredDMSpawns - actualCount;

			int spawnsToTry = std::max( 10, need );

			for( int k=0; k < spawnsToTry; k++ )
			{
				// find a good spot to spawn a Deathmatch spawnpoint
				CBaseEntity *DMSpawn = nullptr;

				m_consecutiveFailedAttempts++;

				for( int attempts = 0; attempts < 10; attempts++ )
				{

					CNavArea *testArea = nullptr;
					if ( mp_guardian_target_site.GetInt() >= 0 )
					{
						const CCSBotManager::Zone *zone = TheCSBots()->GetZone( mp_guardian_target_site.GetInt() );
						if ( zone )
						{
							testArea = zone->m_area[ RandomInt( 0, zone->m_areaCount - 1 ) ];
						}
					}
					else
					{
						testArea = TheNavAreas[ RandomInt( 0, std::ssize( TheNavAreas ) - 1 ) ];
					}

					const float tooSmall = ( mp_guardian_target_site.GetInt() >= 0 ) ? 35.0f : 100.0f;

					if ( testArea && testArea->GetSizeX() > tooSmall && testArea->GetSizeY() > tooSmall )
					{
						bool bBadNavArea = false;

						// don't spawn a DMSpawn too close to another DMSpawn
						for( int n = 0; n < std::ssize( m_DMSpawnVector ); n++ )
						{
							if ( m_DMSpawnVector[n] == nullptr )
								continue;

							float tooClose = ( mp_guardian_target_site.GetInt() >= 0 ) ? 50.0f : 300.0f;
							Vector between = m_DMSpawnVector[n]->GetAbsOrigin() - testArea->GetCenter();
							if ( between.IsLengthLessThan( tooClose ) )
							{
								bBadNavArea = true;
								break;
							}
						}

						if ( bBadNavArea )
							continue;

						if ( IsSpawnBlockedByTrigger( testArea->GetCenter() + Vector( 0, 0, 16 ) ) )
							continue;

						// check that we can path from the nav area to a ct spawner to confirm it isn't orphaned.
						CBaseEntity *CTSpawn = gEntList.FindEntityByClassname( nullptr, "info_player_counterterrorist" );

						if ( CTSpawn )
						{
							CNavArea *CTSpawnArea = GetNearestNavArea( CTSpawn->GetAbsOrigin() );

							ShortestPathCost cost;

							bool bNotOrphaned = NavAreaBuildPath( testArea, CTSpawnArea, nullptr, cost );

							if ( !bNotOrphaned )
							{
								const char* szTSpawnEntName = "info_player_terrorist";
								if ( CSGameRules()->IsPlayingCoopMission() )
									szTSpawnEntName = "info_enemy_terrorist_spawn";

								// double check that we can path from the nav area to a t spawner to confirm it isn't orphaned.
								CBaseEntity *TSpawn = gEntList.FindEntityByClassname( nullptr, szTSpawnEntName );
								if ( TSpawn )
								{
									CNavArea *TSpawnArea = GetNearestNavArea( TSpawn->GetAbsOrigin() );
									ShortestPathCost cost2;
									bNotOrphaned = NavAreaBuildPath( testArea, TSpawnArea, nullptr, cost2 );

									if ( !bNotOrphaned )
										continue;
								}							
							}
						}

						// found a good spot - spawn a DMSpawn here
						DMSpawn = CreateEntityByName( "info_deathmatch_spawn" );
						if ( DMSpawn )
						{
							DMSpawn->SetAbsOrigin( testArea->GetCenter() + Vector( 0, 0, 16 ));

							DispatchSpawn( DMSpawn );
							m_DMSpawnVector.push_back( DMSpawn );

							m_consecutiveFailedAttempts = 0;

//							Msg( " %f :CREATED DM SPAWN %i.\n", gpGlobals->curtime, m_DMSpawnVector.Count() );
						}

						break;
					}
				}
			}

//			MEASURE PERF 2/2
// 			calctime = Plat_FloatTime() - calctime;
// 			Msg( "DM SPAWN SEARCH CALC TIME: %f\n", calctime );
		}
	}

}

bool CNavMesh::IsSpawnBlockedByTrigger( Vector pos )
{
	CBaseEntity *list[32];

	Ray_t ray;
	ray.Init( pos, pos + Vector( 1,1,1 ) );

	int nCount = AllEdictsAlongRay( list, 1024, ray, 0 );

	for ( int i = 0; i < nCount; i++ )
	{
		if ( FClassnameIs( list[i], "func_block_dm_spawns" ) )
			return true;
	}

	return false;
}

int CNavMesh::AllEdictsAlongRay( CBaseEntity **pList, int listMax, const Ray_t &ray, int flagMask )
{
	CFlaggedEntitiesEnum rayEnum( pList, listMax, flagMask );

	::partition->EnumerateElementsAlongRay( PARTITION_ENGINE_NON_STATIC_EDICTS, ray, false, &rayEnum );

	return rayEnum.GetCount();
}

class CImprovLocomotor;
class CNavArea;
class CNavLadder;

using namespace ::ks::reflect;

// nav_path.cpp
// Encapsulation of a path through space
// Author: Michael S. Booth (mike@turtlerockstudios.com), November 2003

#define DrawLine( from, to, duration, red, green, blue )		NDebugOverlay::Line( from, to, red, green, blue, true, 0.1f )

//--------------------------------------------------------------------------------------------------------------
/**
 * Determine actual path positions
 */
bool CCSNavPath::ComputePathPositions( void )
{
	if (m_segmentCount == 0)
		return false;

	// start in first area's center
	m_path[0].pos = m_path[0].area->GetCenter();
	m_path[0].ladder = nullptr;
	m_path[0].how = NUM_TRAVERSE_TYPES;

	for( int i=1; i<m_segmentCount; ++i )
	{
		const PathSegment *from = &m_path[ i-1 ];
		PathSegment *to = &m_path[ i ];

		if (to->how <= GO_WEST)		// walk along the floor to the next area
		{
			to->ladder = nullptr;

			// compute next point, keeping path as straight as possible
			from->area->ComputeClosestPointInPortal( to->area, (NavDirType)to->how, from->pos, &to->pos );

			// move goal position into the goal area a bit
			const float stepInDist = 5.0f;		// how far to "step into" an area - must be less than min area size
			AddDirectionVector( &to->pos, (NavDirType)to->how, stepInDist );

			// we need to walk out of "from" area, so keep Z where we can reach it
			to->pos.z = from->area->GetZ( to->pos );

			// if this is a "jump down" connection, we must insert an additional point on the path
			if (to->area->IsConnected( from->area, NUM_DIRECTIONS ) == false)
			{
				// this is a "jump down" link

				// compute direction of path just prior to "jump down"
				Vector2D dir;
				DirectionToVector2D( (NavDirType)to->how, &dir );

				// shift top of "jump down" out a bit to "get over the ledge"
				const float pushDist = 25.0f;
				to->pos.x += pushDist * dir.x;
				to->pos.y += pushDist * dir.y;

				// insert a duplicate node to represent the bottom of the fall
				if (m_segmentCount < MAX_PATH_SEGMENTS-1)
				{
					// copy nodes down
					for( int j=m_segmentCount; j>i; --j )
						m_path[j] = m_path[j-1];

					// path is one node longer
					++m_segmentCount;

					// move index ahead into the new node we just duplicated
					++i;

					m_path[i].pos.x = to->pos.x + pushDist * dir.x;
					m_path[i].pos.y = to->pos.y + pushDist * dir.y;

					// put this one at the bottom of the fall
					m_path[i].pos.z = to->area->GetZ( m_path[i].pos );
				}
			}
		}
		else if (to->how == GO_LADDER_UP)		// to get to next area, must go up a ladder
		{
			// find our ladder
			const NavLadderConnectVector *ladders = from->area->GetLadders( CNavLadder::LADDER_UP );
			int it;
			for( it=0; it<std::ssize( *ladders ); ++it )
			{
				CNavLadder *ladder = (*ladders)[ it ].ladder;

				// can't use "behind" area when ascending...
				if (ladder->m_topForwardArea == to->area ||
					ladder->m_topLeftArea == to->area ||
					ladder->m_topRightArea == to->area)
				{
					to->ladder = ladder;
					to->pos = ladder->m_bottom + ladder->GetNormal() * 2.0f * HalfHumanWidth;
					break;
				}
			}

			if (it == std::ssize( *ladders ))
			{
				//PrintIfWatched( "ERROR: Can't find ladder in path\n" );
				return false;
			}
		}
		else if (to->how == GO_LADDER_DOWN)		// to get to next area, must go down a ladder
		{
			// find our ladder
			const NavLadderConnectVector *ladders = from->area->GetLadders( CNavLadder::LADDER_DOWN );
			int it;
			for( it=0; it<std::ssize( *ladders ); ++it )
			{
				CNavLadder *ladder = (*ladders)[ it ].ladder;

				if (ladder->m_bottomArea == to->area)
				{
					to->ladder = ladder;
					to->pos = ladder->m_top;
					to->pos = ladder->m_top - ladder->GetNormal() * 2.0f * HalfHumanWidth;
					break;
				}
			}

			if (it == std::ssize( *ladders ))
			{
				//PrintIfWatched( "ERROR: Can't find ladder in path\n" );
				return false;
			}
		}
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if position is at the end of the path
 */
bool CCSNavPath::IsAtEnd( const Vector &pos ) const
{
	if (!IsValid())
		return false;

	const float epsilon = 20.0f;
	return (pos - GetEndpoint()).IsLengthLessThan( epsilon );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return length of path from start to finish
 */
float CCSNavPath::GetLength( void ) const
{
	float length = 0.0f;
	for( int i=1; i<GetSegmentCount(); ++i )
	{
		length += (m_path[i].pos - m_path[i-1].pos).Length();
	}

	return length;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return point a given distance along the path - if distance is out of path bounds, point is clamped to start/end
 * @todo Be careful of returning "positions" along one-way drops, ladders, etc.
 */
bool CCSNavPath::GetPointAlongPath( float distAlong, Vector *pointOnPath ) const
{
	if (!IsValid() || pointOnPath == nullptr)
		return false;

	if (distAlong <= 0.0f)
	{
		*pointOnPath = m_path[0].pos;
		return true;
	}

	float lengthSoFar = 0.0f;
	float segmentLength;
	Vector dir;
	for( int i=1; i<GetSegmentCount(); ++i )
	{
		dir = m_path[i].pos - m_path[i-1].pos;
		segmentLength = dir.Length();

		if (segmentLength + lengthSoFar >= distAlong)
		{
			// desired point is on this segment of the path
			float delta = distAlong - lengthSoFar;
			float t = delta / segmentLength;

			*pointOnPath = m_path[i].pos + t * dir;

			return true;
		}

		lengthSoFar += segmentLength;
	}

	*pointOnPath = m_path[ GetSegmentCount()-1 ].pos;
	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the node index closest to the given distance along the path without going over - returns (-1) if error
 */
int CCSNavPath::GetSegmentIndexAlongPath( float distAlong ) const
{
	if (!IsValid())
		return -1;

	if (distAlong <= 0.0f)
	{
		return 0;
	}

	float lengthSoFar = 0.0f;
	Vector dir;
	for( int i=1; i<GetSegmentCount(); ++i )
	{
		lengthSoFar += (m_path[i].pos - m_path[i-1].pos).Length();

		if (lengthSoFar > distAlong)
		{
			return i-1;
		}
	}

	return GetSegmentCount()-1;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute closest point on path to given point
 * NOTE: This does not do line-of-sight tests, so closest point may be thru the floor, etc
 */
bool CCSNavPath::FindClosestPointOnPath( const Vector *worldPos, int startIndex, int endIndex, Vector *close ) const
{
	if (!IsValid() || close == nullptr)
		return false;

	Vector along, toWorldPos;
	Vector pos;
	const Vector *from, *to;
	float length;
	float closeLength;
	float closeDistSq = 9999999999.9;
	float distSq;

	for( int i=startIndex; i<=endIndex; ++i )
	{
		from = &m_path[i-1].pos;
		to = &m_path[i].pos;

		// compute ray along this path segment
		along = *to - *from;

		// make it a unit vector along the path
		length = along.NormalizeInPlace();

		// compute vector from start of segment to our point
		toWorldPos = *worldPos - *from;

		// find distance of closest point on ray
		closeLength = DotProduct( toWorldPos, along );

		// constrain point to be on path segment
		if (closeLength <= 0.0f)
			pos = *from;
		else if (closeLength >= length)
			pos = *to;
		else
			pos = *from + closeLength * along;

		distSq = (pos - *worldPos).LengthSqr();

		// keep the closest point so far
		if (distSq < closeDistSq)
		{
			closeDistSq = distSq;
			*close = pos;
		}
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Build trivial path when start and goal are in the same nav area
 */
bool CCSNavPath::BuildTrivialPath( const Vector &start, const Vector &goal )
{
	m_segmentCount = 0;

	CNavArea *startArea = TheNavMesh->GetNearestNavArea( start );
	if (startArea == nullptr)
		return false;

	CNavArea *goalArea = TheNavMesh->GetNearestNavArea( goal );
	if (goalArea == nullptr)
		return false;

	m_segmentCount = 2;

	m_path[0].area = startArea;
	m_path[0].pos.x = start.x;
	m_path[0].pos.y = start.y;
	m_path[0].pos.z = startArea->GetZ( start );
	m_path[0].ladder = nullptr;
	m_path[0].how = NUM_TRAVERSE_TYPES;

	m_path[1].area = goalArea;
	m_path[1].pos.x = goal.x;
	m_path[1].pos.y = goal.y;
	m_path[1].pos.z = goalArea->GetZ( goal );
	m_path[1].ladder = nullptr;
	m_path[1].how = NUM_TRAVERSE_TYPES;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Draw the path for debugging.
 */
void CCSNavPath::Draw( const Vector &color )
{
	if (!IsValid())
		return;

	for( int i=1; i<m_segmentCount; ++i )
	{
		DrawLine( m_path[i-1].pos + Vector( 0, 0, HalfHumanHeight ), 
				  m_path[i].pos + Vector( 0, 0, HalfHumanHeight ), 2, 255 * color.x, 255 * color.y, 255 * color.z );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Check line of sight from 'anchor' node on path to subsequent nodes until
 * we find a node that can't been seen from 'anchor'.
 */
int CCSNavPath::FindNextOccludedNode( int anchor )
{
	for( int i=anchor+1; i<m_segmentCount; ++i )
	{
		// don't remove ladder nodes
		if (m_path[i].ladder)
			return i;

		if (!IsWalkableTraceLineClear( m_path[ anchor ].pos, m_path[ i ].pos ))
		{
			// cant see this node from anchor node
			return i;
		}

		Vector anchorPlusHalf =  m_path[ anchor ].pos + Vector( 0, 0, HalfHumanHeight );
		Vector iPlusHalf =  m_path[ i ].pos +Vector( 0, 0, HalfHumanHeight );
		if (!IsWalkableTraceLineClear( anchorPlusHalf, iPlusHalf) )
		{
			// cant see this node from anchor node
			return i;
		}

		Vector anchorPlusFull =  m_path[ anchor ].pos + Vector( 0, 0, HumanHeight );
		Vector iPlusFull = m_path[ i ].pos + Vector( 0, 0, HumanHeight );
		if (!IsWalkableTraceLineClear( anchorPlusFull, iPlusFull ))
		{
			// cant see this node from anchor node
			return i;
		}
	}

	return m_segmentCount;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Smooth out path, removing redundant nodes
 */
void CCSNavPath::Optimize( void )
{
// DONT USE THIS: Optimizing the path results in cutting thru obstacles
return;

	if (m_segmentCount < 3)
		return;

	int anchor = 0;

	while( anchor < m_segmentCount )
	{
		int occluded = FindNextOccludedNode( anchor );
		int nextAnchor = occluded-1;

		if (nextAnchor > anchor)
		{
			// remove redundant nodes between anchor and nextAnchor
			int removeCount = nextAnchor - anchor - 1;
			if (removeCount > 0)
			{
				for( int i=nextAnchor; i<m_segmentCount; ++i )
				{
					m_path[i-removeCount] = m_path[i];
				}
				m_segmentCount -= removeCount;
			}
		}

		++anchor;
	}
}

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------

/**
 * Constructor
 */
CNavPathFollower::CNavPathFollower( void )
{
	m_improv = nullptr;
	m_path = nullptr;

	m_segmentIndex = 0;
	m_isLadderStarted = false;

	m_isDebug = false;
}

void CNavPathFollower::Reset( void )
{
	m_segmentIndex = 1;
	m_isLadderStarted = false;

	m_stuckMonitor.Reset();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move improv along path
 */
void CNavPathFollower::Update( float deltaT, bool avoidObstacles )
{
	if (m_path == nullptr || m_path->IsValid() == false)
		return;

	const CCSNavPath::PathSegment *node = (*m_path)[ m_segmentIndex ];

	if (node == nullptr)
	{
		m_improv->OnMoveToFailure( m_path->GetEndpoint(), CImprovLocomotor::FAIL_INVALID_PATH );
		m_path->Invalidate();
		return;
	}

	// handle ladders
	/*
	if (node->ladder)
	{
		const Vector *approachPos = NULL;
		const Vector *departPos = NULL;

		if (m_segmentIndex)
			approachPos = &(*m_path)[ m_segmentIndex-1 ]->pos;

		if (m_segmentIndex < m_path->GetSegmentCount()-1)
			departPos = &(*m_path)[ m_segmentIndex+1 ]->pos;

		if (!m_isLadderStarted)
		{
			// set up ladder movement
			m_improv->StartLadder( node->ladder, node->how, approachPos, departPos );
			m_isLadderStarted = true;
		}

		// move improv along ladder
		if (m_improv->TraverseLadder( node->ladder, node->how, approachPos, departPos, deltaT ))
		{
			// completed ladder
			++m_segmentIndex;
		}
		return;
	}
	*/

	// reset ladder init flag
	m_isLadderStarted = false;

	//
	// Check if we reached the end of the path
	//
	const float closeRange = 20.0f;
	if ((m_improv->GetFeet() - node->pos).IsLengthLessThan( closeRange ))
	{
		++m_segmentIndex;

		if (m_segmentIndex >= m_path->GetSegmentCount())
		{
			m_improv->OnMoveToSuccess( m_path->GetEndpoint() );
			m_path->Invalidate();
			return;
		}
	}

	m_goal = node->pos;

	const float aheadRange = 300.0f;
	m_segmentIndex = FindPathPoint( aheadRange, &m_goal, &m_behindIndex );
	if (m_segmentIndex >= m_path->GetSegmentCount())
		m_segmentIndex = m_path->GetSegmentCount()-1;

	bool isApproachingJumpArea = false;

	//
	// Crouching
	//
	if (!m_improv->IsUsingLadder())
	{
		// because hostage crouching is not really supported by the engine,
		// if we are standing in a crouch area, we must crouch to avoid collisions
		if (m_improv->GetLastKnownArea() && 
			m_improv->GetLastKnownArea()->GetAttributes() & NAV_MESH_CROUCH && 
			!(m_improv->GetLastKnownArea()->GetAttributes() & NAV_MESH_JUMP))
		{
			m_improv->Crouch();
		}

		// if we are approaching a crouch area, crouch
		// if there are no crouch areas coming up, stand
		const float crouchRange = 50.0f;
		bool didCrouch = false;
		for( int i=m_segmentIndex; i<m_path->GetSegmentCount(); ++i )
		{
			const CNavArea *to = (*m_path)[i]->area;

			// if there is a jump area on the way to the crouch area, don't crouch as it messes up the jump
			if (to->GetAttributes() & NAV_MESH_JUMP)
			{
				isApproachingJumpArea = true;
				break;
			}

			Vector close;
			to->GetClosestPointOnArea( m_improv->GetCentroid(), &close );

			if ((close - m_improv->GetFeet()).AsVector2D().IsLengthGreaterThan( crouchRange ))
				break;

			if (to->GetAttributes() & NAV_MESH_CROUCH)
			{
				m_improv->Crouch();
				didCrouch = true;
				break;
			}

		}

		if (!didCrouch && !m_improv->IsJumping())
		{
			// no crouch areas coming up
			m_improv->StandUp();
		}

	}	// end crouching logic

	if (m_isDebug)
	{
		m_path->Draw();
		UTIL_DrawBeamPoints( m_improv->GetCentroid(), m_goal + Vector( 0, 0, StepHeight ), 1, 255, 0, 255 );
		UTIL_DrawBeamPoints( m_goal + Vector( 0, 0, StepHeight ), m_improv->GetCentroid(), 1, 255, 0, 255 );
	}

	// check if improv becomes stuck
	m_stuckMonitor.Update( m_improv );

	// if improv has been stuck for too long, give up
	const float giveUpTime = 2.0f;
	if (m_stuckMonitor.GetDuration() > giveUpTime)
	{
		m_improv->OnMoveToFailure( m_path->GetEndpoint(), CImprovLocomotor::FAIL_STUCK );
		m_path->Invalidate();
		return;
	}

	// if our goal is high above us, we must have fallen
	if (m_goal.z - m_improv->GetFeet().z > JumpCrouchHeight)
	{
		const float closeRange = 75.0f;
		Vector2D to( m_improv->GetFeet().x - m_goal.x, m_improv->GetFeet().y - m_goal.y );
		if (to.IsLengthLessThan( closeRange ))
		{
			// we can't reach the goal position
			// check if we can reach the next node, in case this was a "jump down" situation
			const CCSNavPath::PathSegment *nextNode = (*m_path)[ m_behindIndex+1 ];
			if (m_behindIndex >=0 && nextNode)
			{
				if (nextNode->pos.z - m_improv->GetFeet().z > JumpCrouchHeight)
				{
					// the next node is too high, too - we really did fall of the path
					m_improv->OnMoveToFailure( m_path->GetEndpoint(), CImprovLocomotor::FAIL_FELL_OFF );
					m_path->Invalidate();
					return;
				}
			}
			else
			{
				// fell trying to get to the last node in the path
				m_improv->OnMoveToFailure( m_path->GetEndpoint(), CImprovLocomotor::FAIL_FELL_OFF );
				m_path->Invalidate();
				return;
			}
		}
	}

	// avoid small obstacles
	if (avoidObstacles && !isApproachingJumpArea && !m_improv->IsJumping() && m_segmentIndex < m_path->GetSegmentCount()-1)
	{
		FeelerReflexAdjustment( &m_goal );

		// currently, this is only used for hostages, and their collision physics stinks
		// do more feeler checks to avoid short obstacles
		/*
		const float inc = 0.25f;
		for( float t = 0.5f; t < 1.0f; t += inc )
		{
			FeelerReflexAdjustment( &m_goal, t * StepHeight );
		}
		*/

	}

	// move improv along path
	m_improv->TrackPath( m_goal, deltaT );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the closest point to our current position on our current path
 * If "local" is true, only check the portion of the path surrounding m_pathIndex.
 */
int CNavPathFollower::FindOurPositionOnPath( Vector *close, bool local ) const
{
	if (!m_path->IsValid())
		return -1;

	Vector along, toFeet;
	Vector feet = m_improv->GetFeet();
	Vector eyes = m_improv->GetEyes();
	Vector pos;
	const Vector *from, *to;
	float length;
	float closeLength;
	float closeDistSq = 9999999999.9;
	int closeIndex = -1;
	float distSq;

	int start, end;

	if (local)
	{
		start = m_segmentIndex - 3;
		if (start < 1)
			start = 1;

		end = m_segmentIndex + 3;
		if (end > m_path->GetSegmentCount())
			end = m_path->GetSegmentCount();
	}
	else
	{
		start = 1;
		end = m_path->GetSegmentCount();
	}

	for( int i=start; i<end; ++i )
	{
		from = &(*m_path)[i-1]->pos;
		to = &(*m_path)[i]->pos;

		// compute ray along this path segment
		along = *to - *from;

		// make it a unit vector along the path
		length = along.NormalizeInPlace();

		// compute vector from start of segment to our point
		toFeet = feet - *from;

		// find distance of closest point on ray
		closeLength = DotProduct( toFeet, along );

		// constrain point to be on path segment
		if (closeLength <= 0.0f)
			pos = *from;
		else if (closeLength >= length)
			pos = *to;
		else
			pos = *from + closeLength * along;

		distSq = (pos - feet).LengthSqr();

		// keep the closest point so far
		if (distSq < closeDistSq)
		{
			// don't use points we cant see
			Vector probe = pos + Vector( 0, 0, HalfHumanHeight );
			if (!IsWalkableTraceLineClear( eyes, probe, WALK_THRU_DOORS | WALK_THRU_BREAKABLES ))
				continue;

			// don't use points we cant reach
			//if (!IsStraightLinePathWalkable( &pos ))
			//	continue;

			closeDistSq = distSq;
			if (close)
				*close = pos;
			closeIndex = i-1;
		}
	}

	return closeIndex;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute a point a fixed distance ahead along our path.
 * Returns path index just after point.
 */
int CNavPathFollower::FindPathPoint( float aheadRange, Vector *point, int *prevIndex )
{
	// find path index just past aheadRange
	int afterIndex;

	// finds the closest point on local area of path, and returns the path index just prior to it
	Vector close;
	int startIndex = FindOurPositionOnPath( &close, true );

	if (prevIndex)
		*prevIndex = startIndex;

	if (startIndex <= 0)
	{
		// went off the end of the path
		// or next point in path is unwalkable (ie: jump-down)
		// keep same point
		return m_segmentIndex;
	}

	// if we are crouching, just follow the path exactly
	if (m_improv->IsCrouching())
	{
		// we want to move to the immediately next point along the path from where we are now
		int index = startIndex+1;
		if (index >= m_path->GetSegmentCount())
			index = m_path->GetSegmentCount()-1;

		*point = (*m_path)[ index ]->pos;

		// if we are very close to the next point in the path, skip ahead to the next one to avoid wiggling
		// we must do a 2D check here, in case the goal point is floating in space due to jump down, etc
		const float closeEpsilon = 20.0f;	// 10
		while ((*point - close).AsVector2D().IsLengthLessThan( closeEpsilon ))
		{
			++index;

			if (index >= m_path->GetSegmentCount())
			{
				index = m_path->GetSegmentCount()-1;
				break;
			}

			*point = (*m_path)[ index ]->pos;
		}

		return index;
	}

	// make sure we use a node a minimum distance ahead of us, to avoid wiggling 
	while (startIndex < m_path->GetSegmentCount()-1)
	{
		Vector pos = (*m_path)[ startIndex+1 ]->pos;

		// we must do a 2D check here, in case the goal point is floating in space due to jump down, etc
		const float closeEpsilon = 20.0f;
		if ((pos - close).AsVector2D().IsLengthLessThan( closeEpsilon ))
		{
			++startIndex;
		}
		else
		{
			break;
		}
	}

	// if we hit a ladder or jump area, must stop (dont use ladder behind us)
	if (startIndex > m_segmentIndex && startIndex < m_path->GetSegmentCount() && 
			((*m_path)[ startIndex ]->ladder || (*m_path)[ startIndex ]->area->GetAttributes() & NAV_MESH_JUMP))
	{
		*point = (*m_path)[ startIndex ]->pos;
		return startIndex;
	}

	// we need the point just *ahead* of us
	++startIndex;
	if (startIndex >= m_path->GetSegmentCount())
		startIndex = m_path->GetSegmentCount()-1;

	// if we hit a ladder or jump area, must stop
	if (startIndex < m_path->GetSegmentCount() && 
			((*m_path)[ startIndex ]->ladder || (*m_path)[ startIndex ]->area->GetAttributes() & NAV_MESH_JUMP))
	{
		*point = (*m_path)[ startIndex ]->pos;
		return startIndex;
	}

	// note direction of path segment we are standing on
	Vector initDir = (*m_path)[ startIndex ]->pos - (*m_path)[ startIndex-1 ]->pos;
	initDir.NormalizeInPlace();

	Vector feet = m_improv->GetFeet();
	Vector eyes = m_improv->GetEyes();
	float rangeSoFar = 0;

	// this flag is true if our ahead point is visible
	bool visible = true;

	Vector prevDir = initDir;

	// step along the path until we pass aheadRange
	bool isCorner = false;
	int i;
	for( i=startIndex; i<m_path->GetSegmentCount(); ++i )
	{
		Vector pos = (*m_path)[i]->pos;
		Vector to = pos - (*m_path)[i-1]->pos;
		Vector dir = to;
		dir.NormalizeInPlace();

		// don't allow path to double-back from our starting direction (going upstairs, down curved passages, etc)
		if (DotProduct( dir, initDir ) < 0.0f) // -0.25f
		{
			--i;
			break;
		}

		// if the path turns a corner, we want to move towards the corner, not into the wall/stairs/etc
		if (DotProduct( dir, prevDir ) < 0.5f)
		{
			isCorner = true;
			--i;
			break;
		}
		prevDir = dir;

		// don't use points we cant see
		Vector probe = pos + Vector( 0, 0, HalfHumanHeight );
		if (!IsWalkableTraceLineClear( eyes, probe, WALK_THRU_BREAKABLES ))
		{
			// presumably, the previous point is visible, so we will interpolate
			visible = false;
			break;
		}

		// if we encounter a ladder or jump area, we must stop
		if (i < m_path->GetSegmentCount() && 
				((*m_path)[ i ]->ladder || (*m_path)[ i ]->area->GetAttributes() & NAV_MESH_JUMP))
			break;

		// Check straight-line path from our current position to this position
		// Test for un-jumpable height change, or unrecoverable fall
		//if (!IsStraightLinePathWalkable( &pos ))
		//{
		//	--i;
		//	break;
		//}

		Vector along = (i == startIndex) ? (pos - feet) : (pos - (*m_path)[i-1]->pos);
		rangeSoFar += along.Length2D();

		// stop if we have gone farther than aheadRange
		if (rangeSoFar >= aheadRange)
			break;
	}

	if (i < startIndex)
		afterIndex = startIndex;
	else if (i < m_path->GetSegmentCount())
		afterIndex = i;
	else
		afterIndex = m_path->GetSegmentCount()-1;

	// compute point on the path at aheadRange
	if (afterIndex == 0)
	{
		*point = (*m_path)[0]->pos;
	}
	else
	{
		// interpolate point along path segment
		const Vector *afterPoint = &(*m_path)[ afterIndex ]->pos;
		const Vector *beforePoint = &(*m_path)[ afterIndex-1 ]->pos;

		Vector to = *afterPoint - *beforePoint;
		float length = to.Length2D();

		float t = 1.0f - ((rangeSoFar - aheadRange) / length);

		if (t < 0.0f)
			t = 0.0f;
		else if (t > 1.0f)
			t = 1.0f;

		*point = *beforePoint + t * to;

		// if afterPoint wasn't visible, slide point backwards towards beforePoint until it is
		if (!visible)
		{
			const float sightStepSize = 25.0f;
			float dt = sightStepSize / length;

			Vector probe = *point + Vector( 0, 0, HalfHumanHeight );
			while( t > 0.0f && !IsWalkableTraceLineClear( eyes,  probe, WALK_THRU_BREAKABLES ) )
			{
				t -= dt;
				*point = *beforePoint + t * to;
			}

			if (t <= 0.0f)
				*point = *beforePoint;
		}
	}

	// if position found is too close to us, or behind us, force it farther down the path so we don't stop and wiggle
	if (!isCorner)
	{
		const float epsilon = 50.0f;
		Vector2D toPoint;
		Vector2D centroid( m_improv->GetCentroid().x, m_improv->GetCentroid().y );
		
		toPoint.x = point->x - centroid.x;
		toPoint.y = point->y - centroid.y;

		if (DotProduct2D( toPoint, initDir.AsVector2D() ) < 0.0f || toPoint.IsLengthLessThan( epsilon ))
		{
			int i;
			for( i=startIndex; i<m_path->GetSegmentCount(); ++i )
			{
				toPoint.x = (*m_path)[i]->pos.x - centroid.x;
				toPoint.y = (*m_path)[i]->pos.y - centroid.y;
				if ((*m_path)[i]->ladder || (*m_path)[i]->area->GetAttributes() & NAV_MESH_JUMP || toPoint.IsLengthGreaterThan( epsilon ))
				{
					*point = (*m_path)[i]->pos;
					startIndex = i;
					break;
				}
			}

			if (i == m_path->GetSegmentCount())
			{
				*point = m_path->GetEndpoint();
				startIndex = m_path->GetSegmentCount()-1;
			}
		}
	}

	// m_pathIndex should always be the next point on the path, even if we're not moving directly towards it
	if (startIndex < m_path->GetSegmentCount())
		return startIndex;

	return m_path->GetSegmentCount()-1;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Do reflex avoidance movements if our "feelers" are touched
 * @todo Parameterize feeler spacing
 */
void CNavPathFollower::FeelerReflexAdjustment( Vector *goalPosition, float height )
{
	// if we are in a "precise" area, do not do feeler adjustments
	if (m_improv->GetLastKnownArea() && m_improv->GetLastKnownArea()->GetAttributes() & NAV_MESH_PRECISE)
		return;

	// use the direction towards the goal
	Vector dir = *goalPosition - m_improv->GetFeet();
	dir.z = 0.0f;
	dir.NormalizeInPlace();

	Vector lat( -dir.y, dir.x, 0.0f );

	const float feelerOffset = (m_improv->IsCrouching()) ? 15.0f : 20.0f;	// 15, 20
	const float feelerLengthRun = 50.0f;	// 100 - too long for tight hallways (cs_747)
	const float feelerLengthWalk = 30.0f;

	const float feelerHeight = (height > 0.0f) ? height : StepHeight + 0.1f;	// if obstacle is lower than StepHeight, we'll walk right over it

	float feelerLength = (m_improv->IsRunning()) ? feelerLengthRun : feelerLengthWalk;

	feelerLength = (m_improv->IsCrouching()) ? 20.0f : feelerLength;

	//
	// Feelers must follow floor slope
	//
	float ground;
	Vector normal;
	if (m_improv->GetSimpleGroundHeightWithFloor( m_improv->GetEyes(), &ground, &normal ) == false)
		return;

	// get forward vector along floor
	dir = CrossProduct( lat, normal );

	// correct the sideways vector
	lat = CrossProduct( dir, normal );

	Vector feet = m_improv->GetFeet();
	feet.z += feelerHeight;

	Vector from = feet + feelerOffset * lat;
	Vector to = from + feelerLength * dir;

	bool leftClear = IsWalkableTraceLineClear( from, to, WALK_THRU_DOORS | WALK_THRU_BREAKABLES );

	// draw debug beams
	if (m_isDebug)
	{
		if (leftClear)
			UTIL_DrawBeamPoints( from, to, 1, 0, 255, 0 );
		else
			UTIL_DrawBeamPoints( from, to, 1, 255, 0, 0 );
	}

	from = feet - feelerOffset * lat;
	to = from + feelerLength * dir;

	bool rightClear = IsWalkableTraceLineClear( from, to, WALK_THRU_DOORS | WALK_THRU_BREAKABLES );

	// draw debug beams
	if (m_isDebug)
	{
		if (rightClear)
			UTIL_DrawBeamPoints( from, to, 1, 0, 255, 0 );
		else
			UTIL_DrawBeamPoints( from, to, 1, 255, 0, 0 );
	}

	const float avoidRange = (m_improv->IsCrouching()) ? 150.0f : 300.0f;

	if (!rightClear)
	{
		if (leftClear)
		{
			// right hit, left clear - veer left
			*goalPosition = *goalPosition + avoidRange * lat;
			//*goalPosition = m_improv->GetFeet() + avoidRange * lat;

			//m_improv->StrafeLeft();
		}
	}
	else if (!leftClear)
	{
		// right clear, left hit - veer right
		*goalPosition = *goalPosition - avoidRange * lat;
		//*goalPosition = m_improv->GetFeet() - avoidRange * lat;

		//m_improv->StrafeRight();
	}

}

//--------------------------------------------------------------------------------------------------------------
/**
 * Reset the stuck-checker.
 */
CStuckMonitor::CStuckMonitor( void )
{
	m_isStuck = false;
	m_avgVelIndex = 0;
	m_avgVelCount = 0;
}

/**
 * Reset the stuck-checker.
 */
void CStuckMonitor::Reset( void )
{
	m_isStuck = false;
	m_avgVelIndex = 0;
	m_avgVelCount = 0;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Test if the improv has become stuck
 */
void CStuckMonitor::Update( CImprovLocomotor *improv )
{
	if (m_isStuck)
	{
		// improv is stuck - see if it has moved far enough to be considered unstuck
		const float unstuckRange = 75.0f;
		if ((improv->GetCentroid() - m_stuckSpot).IsLengthGreaterThan( unstuckRange ))
		{
			// no longer stuck
			Reset();
			//PrintIfWatched( "UN-STUCK\n" );
		}
	}
	else
	{
		// check if improv has become stuck

		// compute average velocity over a short period (for stuck check)
		Vector vel = improv->GetCentroid() - m_lastCentroid;

		// if we are jumping, ignore Z
		//if (improv->IsJumping())
		//	vel.z = 0.0f;

		// ignore Z unless we are on a ladder (which is only Z)
		if (!improv->IsUsingLadder())
			vel.z = 0.0f;

		// cannot be Length2D, or will break ladder movement (they are only Z)
		float moveDist = vel.Length();

		float deltaT = gpGlobals->curtime - m_lastTime;
		if (deltaT <= 0.0f)
			return;

		m_lastTime = gpGlobals->curtime;

		// compute current velocity
		m_avgVel[ m_avgVelIndex++ ] = moveDist/deltaT;

		if (m_avgVelIndex == MAX_VEL_SAMPLES)
			m_avgVelIndex = 0;

		if (m_avgVelCount < MAX_VEL_SAMPLES)
		{
			++m_avgVelCount;
		}
		else
		{
			// we have enough samples to know if we're stuck

			float avgVel = 0.0f;
			for( int t=0; t<m_avgVelCount; ++t )
				avgVel += m_avgVel[t];

			avgVel /= m_avgVelCount;

			// cannot make this velocity too high, or actors will get "stuck" when going down ladders
			float stuckVel = (improv->IsUsingLadder()) ? 10.0f : 20.0f;

			if (avgVel < stuckVel)
			{
				// note when and where we initially become stuck
				m_stuckTimer.Start();
				m_stuckSpot = improv->GetCentroid();
				m_isStuck = true;
			}
		}
	}

	// always need to track this
	m_lastCentroid = improv->GetCentroid();
}

#undef DrawLine
