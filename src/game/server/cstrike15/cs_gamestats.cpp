// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: CS game stats

// Some tricky business here - we don't want to include the precompiled header for the statreader
// and trying to #ifdef it out does funky things like ignoring the #endif. Define our header file
// separately and include it based on the switch

import std;
import ks.tier1;
#include "string_t.h"
#include "vphysics_interface.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "shareddefs.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "basetempentity.h"
#include "recipientfilter.h"

#include "cs_gamerules.h"
#include "cs_gamestats.h"
#include "weapon_csbase.h"
#include "props.h"
#include "cs_achievement_constants.h"
#include "weapon_c4.h"
#include "cs_bot.h"

#include "filesystem.h"
#include "bot_util.h"

#include "cdll_int.h"

//#define DEBUG_STAT_TRANSMISSION 

extern ConVar game_mode;
extern ConVar game_type;
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
static void SvNoNemesisChangeCallback( IConVar *pConVar, const char *pOldValue, float flOldValue )
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

ConVar sv_nonemesis( "sv_nonemesis", "0", 0, "Disable nemesis and revenge.", SvNoNemesisChangeCallback );
ConVar sv_dumpmatchweaponmetrics( "sv_dumpmatchweaponmetrics", "0", FCVAR_DEVELOPMENTONLY, "Turn on the exporting of weapon metrics at the end of a level.");

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
	if ( CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon *>( info.GetWeapon() ) )
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
