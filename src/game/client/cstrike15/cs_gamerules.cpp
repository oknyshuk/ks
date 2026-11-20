// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include "cdll_int.h"
#include "client_class.h"
#include "edict.h"
#include "game/client/viewport_panel_names.h"
#include "gametypes.h"
#include "igameevents.h"
#include "irecipientfilter.h"
#include "localize/ilocalize.h"
#include "usermessages_schema.h"
#include "vphysics_interface.h"
#include "cs_gamerules.h"
#include "../gamerules_register.h"
#include "../hud_macros.h"
#include "../predictable_entity.h"
#include "../reflect_recvtable.h"
module ks.client;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.platform;
import ks.core.threadtools;
import ks.core.types;
import ks.game.cstrike15.cstrike15_gcconstants;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.playerid;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.strtools;
import :particlemgr;
import :baseentity;
import :iinput;
import :props;
import :baseanimatedtextureproxy;
import :sharedvar;
import :util;
import :cstrike15.cs_player;
import :cstrike15.cs_shareddefs;
import :econ.econ_item_schema_minimal;
import :game_controls.baseviewport;
import :gamevars_shared;
import :precache;
import :noopanimcontroller;
import ks.game.shareddefs;
import :achievements_and_stats_interface;
import :cstrike15.cs_player_shared;
import ks.game.gamemovement;
import ks.game.mapentities_shared;

class CAmmoDef;
class CBaseHudWeaponSelection;
class CCSWeaponInfo;
class CEconItemDefinition;
class CEconMusicDefinition;
class CEconQuestDefinition;
class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CCSPlayer;
class CEconItemView;
class CTeam;
class CWeaponCSBase;

using namespace ::ks::reflect;

// Purpose: The TF Game rules

//
// Crazy compiler problems fix -- deque must be included first before any of the Valve game headers, which introduce
// macro defines that completely break STL compile process. STL include dependencies happen as part of RSA crypto
// library dependencies included at the bottom of this file.
//
// (hegrenade_projectile.h: the client never implemented it)

const float g_flWarmupToFreezetimeDelay = 4.0f;

ConVar sv_server_graphic1( side_registry(), "sv_server_graphic1", "", FCVAR_REPLICATED | FCVAR_RELEASE,	"A 360x60 (<16kb) image file in /csgo/ that will be displayed to spectators." );
ConVar sv_server_graphic2( side_registry(), "sv_server_graphic2", "", FCVAR_REPLICATED | FCVAR_RELEASE,	"A 220x45 (<16kb) image file in /csgo/ that will be displayed to spectators." );

ConVar sv_disable_observer_interpolation( side_registry(), "sv_disable_observer_interpolation", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Disallow interpolating between observer targets on this server." );

ConVar sv_reward_drop_delay( side_registry(), "sv_reward_drop_delay", "3.0", FCVAR_REPLICATED, "Delay between the end match scoreboard being shown and the beginning of item drops." );

// to define which holiday it is
ConVar sv_holiday_mode( side_registry(), "sv_holiday_mode", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "0 = OFF, 1 = Halloween, 2 = Winter" );

ConVar sv_teamid_overhead_always_prohibit( side_registry(), "sv_teamid_overhead_always_prohibit", "0", FCVAR_RELEASE | FCVAR_REPLICATED | FCVAR_NOTIFY, "Determines whether cl_teamid_overhead_always is prohibited." );

ConVar sv_show_team_equipment_prohibit( side_registry(), "sv_show_team_equipment_prohibit", "0", FCVAR_RELEASE | FCVAR_REPLICATED | FCVAR_NOTIFY, "Determines whether +cl_show_team_equipment is prohibited." );

ConVar mp_team_timeout_time( side_registry(), "mp_team_timeout_time", "60", FCVAR_RELEASE | FCVAR_GAMEDLL | FCVAR_REPLICATED, "Duration of each timeout." );
ConVar mp_team_timeout_max( side_registry(), "mp_team_timeout_max", "1", FCVAR_RELEASE | FCVAR_GAMEDLL | FCVAR_REPLICATED, "Number of timeouts each team gets per match." );

void MaxAllowedNetGraphCallback( ConVar *var, const char *pOldValue, float flOldValue );
ConVar sv_max_allowed_net_graph( side_registry(), "sv_max_allowed_net_graph", "1", FCVAR_NOTIFY | FCVAR_RELEASE | FCVAR_REPLICATED, "Determines max allowed net_graph value for clients.", MaxAllowedNetGraphCallback );

// The legacy VGUI net_graph renderer was removed; the cvar is retained here (its only
// remaining consumer is the clamp below) so a future RmlUi rewrite can read it.
ConVar net_graph( side_registry(), "net_graph", "0", FCVAR_ARCHIVE, "Draw the network usage data (renderer pending rewrite)." );

extern ConVar net_graph;
void MaxAllowedNetGraphCallback( ConVar *var, const char *pOldValue, float flOldValue )
{

	if ( net_graph.GetInt() > sv_max_allowed_net_graph.GetInt() )
	{
		net_graph.SetValue( sv_max_allowed_net_graph.GetInt() );
		Msg( "Server does not allow net_graph values above %d\n", sv_max_allowed_net_graph.GetInt() );
	}
}

static constexpr auto SV_QMM_MIN_PLAYERS_FOR_CANCEL_MATCH = 0;

extern ConVar mp_maxrounds;
extern ConVar mp_match_restart_delay;

ConVar sv_disable_show_team_select_menu( side_registry(), "sv_disable_show_team_select_menu", "0", FCVAR_REPLICATED|FCVAR_RELEASE, "Prevent the team select menu from showing." );
ConVar sv_disable_motd( side_registry(), "sv_disable_motd", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT, "Prevent the motd from showing." );

static void print_mapgroup( const CCommand &args );
static ConCommand print_mapgroup_command( side_registry(), "print_mapgroup", print_mapgroup, "Prints the current mapgroup and the contained maps", FCVAR_RELEASE );
static void print_mapgroup( const CCommand &args )
{
	const char* szMapGroup = engine->GetMapGroupName();
	const std::vector< std::string > *pMaps = g_pGameTypes->GetMapGroupMapList( szMapGroup );
	Msg( "Map group: %s\n", szMapGroup );
	if ( pMaps )
	{
		for ( const std::string &sMap : *pMaps )
		{
			const char* szMap = sMap.c_str();
			Msg( "     %s\n", szMap );
		}
	}
	else
	{
		Msg( "No maps in mapgroup map list!\n");
	}
}

static const float cInitialRestartRoundTime = 0.0f;

/**
 * Player hull & eye position for standing, ducking, etc.  This version has a taller
 * player height, but goldsrc-compatible collision bounds.
 */
static CViewVectors g_CSViewVectors(
    Vector( 0, 0, 64 ),		// eye position

    Vector(-16, -16, 0 ),	// hull min
    Vector( 16,  16, 72 ),	// hull max

    Vector(-16, -16, 0 ),	// duck hull min
    Vector( 16,  16, 54 ),	// duck hull max
    Vector( 0, 0, 46 ),		// duck view

    Vector(-10, -10, -10 ),	// observer hull min
    Vector( 10,  10,  10 ),	// observer hull max

    Vector( 0, 0, 14 )		// dead view height
);

	bool __MsgFunc_SendPlayerItemDrops( const ks::net::CCSUsrMsg_SendPlayerItemDrops &msg );
	bool __MsgFunc_SendPlayerItemFound( const ks::net::CCSUsrMsg_SendPlayerItemFound &msg );

REGISTER_GAMERULES_CLASS( CCSGameRules );

IMPLEMENT_REFLECT_TABLE( CCSGameRules, DT_CSGameRules );

IMPLEMENT_NETWORKCLASS_ALIASED( CSGameRulesProxy, DT_CSGameRulesProxy )
LINK_ENTITY_TO_CLASS_ALIASED( cs_gamerules, CSGameRulesProxy );

// COOP
static constexpr auto COOPMISSION_SCORE_MULTIPLIER_TIMELEFT = 40;
static constexpr auto COOPMISSION_SCORE_MULTIPLIER_DAMTAKEN = -10;
static constexpr auto COOPMISSION_SCORE_MULTIPLIER_ROUNDSFAILED = -1000;

static uint32 Helper_ScoreLeaderboardData_FindEntryValue( uint32 nTag, const std::vector< ks::net::ScoreLeaderboardData::Entry > &arr )
{
	for ( int i = 0; i < arr.size(); ++ i )
	{
		if ( arr[ i ].tag == nTag )
			return arr[ i ].val;
	}
	return 0;
}
static uint32 Helper_ScoreLeaderboardData_FindEntryValueSum( uint32 nTag, const std::vector< ks::net::ScoreLeaderboardData::AccountEntries > &arr )
{
	uint32 val = 0;
	for ( int i = 0; i < arr.size(); ++i )
	{
		val += Helper_ScoreLeaderboardData_FindEntryValue( nTag, arr[ i ].entries );
	}
	return val;
}

int32 CoopScoreGetRatingEntryFromLeaderboardData( ks::net::ScoreLeaderboardData &sld, bool bBonus, int nIndex, bool bAsScore )
{	// Note: this function should not be referencing gamerules
	bool bGuardian = false;
	bool bCoopMission = true;
	if ( sld.quest_id )
	{
		const CEconQuestDefinition *pQuest = GetItemSchema()->GetQuestDefinition( sld.quest_id );
		if ( !pQuest )
			return 0.0f;
		if ( !V_stricmp( pQuest->GetGameMode(), "cooperative" ) )
		{
			bGuardian = true;
			bCoopMission = false;
		}
		// otherwise assume coopmission
	}
	else if ( CSGameRules() )
	{	// keeping this around for local compatibility testing with listenservers
		bGuardian = CSGameRules()->IsPlayingCoopGuardian();
		bCoopMission = CSGameRules()->IsPlayingCoopMission();
		// otherwise assume coopmission
	}
	else
		return 0.0f;

	int32 nResult = 0;

	//
	// Base scorechart
	//
	if ( !bBonus )
	{

		if ( bGuardian )
		{
			if ( nIndex == 0 )
			{	// Damage to enemies ratio
				uint32 numDmgInflicted = Helper_ScoreLeaderboardData_FindEntryValueSum( k_EScoreLeaderboardDataEntryTag_HpDmgInflicted, sld.accountentries );
				uint32 numDmgSuffered = Helper_ScoreLeaderboardData_FindEntryValueSum( k_EScoreLeaderboardDataEntryTag_HpDmgSuffered, sld.accountentries );
				if ( !numDmgInflicted )
					nResult = 0;
				else if ( numDmgSuffered )
				{
					nResult = int( 10000.0f * float( numDmgInflicted ) / float( numDmgInflicted + numDmgSuffered ) );
					if ( numDmgInflicted && ( nResult <= 0 ) )
						nResult = 1;
				}
				else
					nResult = 10000;
				//if ( bAsScore )
				//{
				//	nResult = nResult;
				//}
			}
			else if ( nIndex == 3 )
			{	// Rounds failed penalty
				nResult = Helper_ScoreLeaderboardData_FindEntryValue( k_EScoreLeaderboardDataEntryTag_RoundsPlayed, sld.matchentries );
				nResult = ( nResult > 1 ) ? ( nResult - 1 ) : 0;
				if ( bAsScore )
				{
					nResult = COOPMISSION_SCORE_MULTIPLIER_ROUNDSFAILED*nResult;
				}
			}
		}
		else
		{
			if ( nIndex == 0 )
			{	// Time Remaining on the clock
				nResult = Helper_ScoreLeaderboardData_FindEntryValue( k_EScoreLeaderboardDataEntryTag_TimeRemaining, sld.matchentries );
				if ( bAsScore )
				{
					nResult = COOPMISSION_SCORE_MULTIPLIER_TIMELEFT*nResult;
				}
			}
			else if ( nIndex == 3 )
			{	// Total damage taken
				nResult = Helper_ScoreLeaderboardData_FindEntryValueSum( k_EScoreLeaderboardDataEntryTag_HpDmgSuffered, sld.accountentries );
				if ( bAsScore )
				{
					nResult = COOPMISSION_SCORE_MULTIPLIER_DAMTAKEN*nResult;
				}
			}
		}

		if ( bGuardian || bCoopMission )
		{	// Shared categories for Guardian and Coop Mission
			if ( nIndex == 1 )
			{	// Bullets Accuracy
				uint32 numBulletsFired = Helper_ScoreLeaderboardData_FindEntryValueSum( k_EScoreLeaderboardDataEntryTag_ShotsFired, sld.accountentries );
				uint32 numBulletsOnTarget = Helper_ScoreLeaderboardData_FindEntryValueSum( k_EScoreLeaderboardDataEntryTag_ShotsOnTarget, sld.accountentries );
				if ( !numBulletsFired )
					nResult = 0;
				else if ( numBulletsFired > 0 && numBulletsOnTarget < numBulletsFired )
				{
					nResult = int( 10000.0f * float( numBulletsOnTarget ) / float( numBulletsFired ) );
					if ( numBulletsOnTarget && ( nResult <= 0 ) )
						nResult = 1;
				}
				else
					nResult = 10000;
				//if ( bAsScore )
				//{
				//	nResult = nResult;
				//}
			}
			else if ( nIndex == 2 )
			{	// Headshots kill percentage
				uint32 numHeadshots = Helper_ScoreLeaderboardData_FindEntryValueSum( k_EScoreLeaderboardDataEntryTag_Headshots, sld.accountentries );
				uint32 numKills = Helper_ScoreLeaderboardData_FindEntryValueSum( k_EScoreLeaderboardDataEntryTag_Kills, sld.accountentries );
				if ( !numKills )
					nResult = 0;
				else if ( numKills > 0 && numHeadshots < numKills )
				{
					nResult = int( 10000.0f * float( numHeadshots ) / float( numKills ) );
					if ( numHeadshots && ( nResult <= 0 ) )
						nResult = 1;
				}
				else
					nResult = 10000;
				//if ( bAsScore )
				//{
				//	nResult = nResult;
				//}
			}
		}

	}
	//
	// Scorechart for bonuses!
	//
	else
	{

		if ( bGuardian )
		{
			if ( nIndex == 0 )
			{	// Under 3 rounds?
				uint32 numRoundsPlayed = Helper_ScoreLeaderboardData_FindEntryValue( k_EScoreLeaderboardDataEntryTag_RoundsPlayed, sld.matchentries );
				nResult = numRoundsPlayed;
				if ( bAsScore )
				{
					nResult = ( numRoundsPlayed <= 3 ) ? 5000 : 0;
				}
			}
		}

		if ( bCoopMission )
		{
			if ( nIndex == 0 )
			{	// No Deaths?
				uint32 numDeaths = Helper_ScoreLeaderboardData_FindEntryValueSum( k_EScoreLeaderboardDataEntryTag_Deaths, sld.accountentries );
				nResult = numDeaths;
				if ( bAsScore )
				{
					nResult = ( numDeaths == 0 ) ? 5000 : 0;
				}
			}
			else if ( nIndex == 1 )
			{	// All Challenge Coins?
				uint32 numChallenge = Helper_ScoreLeaderboardData_FindEntryValue( k_EScoreLeaderboardDataEntryTag_BonusChallenge, sld.matchentries );
				nResult = numChallenge;
				if ( bAsScore )
				{
					nResult = ( numChallenge ) ? 5000 : 0;
				}
			}
			else if ( nIndex == 2 )
			{	// Pistols Only?
				uint32 numPistolsOnly = Helper_ScoreLeaderboardData_FindEntryValue( k_EScoreLeaderboardDataEntryTag_BonusPistolOnly, sld.matchentries );
				nResult = numPistolsOnly;
				if ( bAsScore )
				{
					nResult = ( numPistolsOnly ) ? 10000 : 0;
				}
			}
			else if ( nIndex == 3 )
			{	// Hard Mode?
				uint32 numHardMode = Helper_ScoreLeaderboardData_FindEntryValue( k_EScoreLeaderboardDataEntryTag_BonusHardMode, sld.matchentries );
				nResult = numHardMode;
				if ( bAsScore )
				{
					nResult = ( numHardMode ) ? 25000 : 0;
				}
			}
		}

	}

	return nResult;
}

    void RecvProxy_CSGameRules( const RecvProp *pProp, void **pOut, void *pData, int objectID )
    {
        CCSGameRules *pRules = CSGameRules();
        contract_assert( pRules );
        *pOut = pRules;
    }

    IMPLEMENT_REFLECT_TABLE( CCSGameRulesProxy, DT_CSGameRulesProxy );

// MOSTLY DECPRECATED.
// All weapons ( except exhaustibles, ITEM_FLAG_EXHAUSTIBLE, such as grenades ) store their reserve ammo on the weapon entity
// and no longer on the player. The reserve amount is specified in schema as attributes "primary reserve ammo max" and "secondary reserve ammo max"

ConVar ammo_50AE_max( side_registry(), "ammo_50AE_max", "35", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_762mm_max( side_registry(), "ammo_762mm_max", "90", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_556mm_max( side_registry(), "ammo_556mm_max", "90", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_556mm_small_max( side_registry(), "ammo_556mm_small_max", "40", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_556mm_box_max( side_registry(), "ammo_556mm_box_max", "200", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_338mag_max( side_registry(), "ammo_338mag_max", "30", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_9mm_max( side_registry(), "ammo_9mm_max", "120", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_buckshot_max( side_registry(), "ammo_buckshot_max", "32", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_45acp_max( side_registry(), "ammo_45acp_max", "100", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_357sig_max( side_registry(), "ammo_357sig_max", "52", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_357sig_p250_max( side_registry(), "ammo_357sig_p250_max", "26", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_357sig_small_max( side_registry(), "ammo_357sig_small_max", "24", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_357sig_min_max( side_registry(), "ammo_357sig_min_max", "12", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_57mm_max( side_registry(), "ammo_57mm_max", "100", FCVAR_REPLICATED | FCVAR_RELEASE );

ConVar ammo_grenade_limit_default( side_registry(), "ammo_grenade_limit_default", "1", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_grenade_limit_flashbang( side_registry(), "ammo_grenade_limit_flashbang", "1", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_grenade_limit_total( side_registry(), "ammo_grenade_limit_total", "3", FCVAR_REPLICATED | FCVAR_RELEASE );

ConVar ammo_item_limit_healthshot( side_registry(), "ammo_item_limit_healthshot", "4", FCVAR_REPLICATED | FCVAR_RELEASE );

ConVar ammo_50AE_impulse( side_registry(), "ammo_50AE_impulse", "2400", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_762mm_impulse( side_registry(), "ammo_762mm_impulse", "2400", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_556mm_impulse( side_registry(), "ammo_556mm_impulse", "2400", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_556mm_box_impulse( side_registry(), "ammo_556mm_box_impulse", "2400", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_338mag_impulse( side_registry(), "ammo_338mag_impulse", "2800", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_9mm_impulse( side_registry(), "ammo_9mm_impulse", "2000", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_buckshot_impulse( side_registry(), "ammo_buckshot_impulse", "600", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_45acp_impulse( side_registry(), "ammo_45acp_impulse", "2100", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_357sig_impulse( side_registry(), "ammo_357sig_impulse", "2000", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_57mm_impulse( side_registry(), "ammo_57mm_impulse", "2000", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );

ConVar ammo_50AE_headshot_mult( side_registry(), "ammo_50AE_headshot_mult", "1.0", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_762mm_headshot_mult( side_registry(), "ammo_762mm_headshot_mult", "1.0", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_556mm_headshot_mult( side_registry(), "ammo_556mm_headshot_mult", "1.0", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_556mm_box_headshot_mult( side_registry(), "ammo_556mm_box_headshot_mult", "1.0", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_338mag_headshot_mult( side_registry(), "ammo_338mag_headshot_mult", "1.0", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_9mm_headshot_mult( side_registry(), "ammo_9mm_headshot_mult", "1.0", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_buckshot_headshot_mult( side_registry(), "ammo_buckshot_headshot_mult", "1.0", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_45acp_headshot_mult( side_registry(), "ammo_45acp_headshot_mult", "1.0", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_357sig_headshot_mult( side_registry(), "ammo_357sig_headshot_mult", "1.0", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_57mm_headshot_mult( side_registry(), "ammo_57mm_headshot_mult", "1.0", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );

//ConVar mp_dynamicpricing( "mp_dynamicpricing", "0", FCVAR_REPLICATED, "Enables or Disables the dynamic weapon prices" );

ConVar mp_spec_swapplayersides( 
	side_registry(), "mp_spec_swapplayersides", 
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Toggle set the player names and team names to the opposite side in which they are are on the spectator panel.",
	true, 0,
	true, 1
	);

ConVar mp_force_assign_teams(
	side_registry(), "mp_force_assign_teams",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Players don't get to choose what team they are on, it is auto assinged.",
	true, 0,
	true, 1
	);

ConVar sv_matchpause_auto_5v5( side_registry(), "sv_matchpause_auto_5v5", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "When enabled will automatically pause the match at next freeze time if less than 5 players are connected on each team." );

extern ConVar sv_stopspeed;
extern ConVar mp_randomspawn;
extern ConVar mp_randomspawn_los;
extern ConVar mp_teammates_are_enemies;
extern ConVar mp_respawnwavetime;
extern ConVar mp_hostages_max;
extern ConVar mp_hostages_spawn_farthest;
extern ConVar mp_hostages_spawn_force_positions;
extern ConVar mp_hostages_spawn_same_every_round;
extern ConVar weapon_max_before_cleanup;

ConVar mp_spectators_max( 
	side_registry(), "mp_spectators_max", 
	"2",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"How many spectators are allowed in a match.",
	true, 0,
	false, 0 );

ConVar mp_buytime( 
    side_registry(), "mp_buytime", 
    "90",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "How many seconds after round start players can buy items for.",
    true, 0,
    false, 0 );

ConVar mp_buy_allow_grenades( 
	side_registry(), "mp_buy_allow_grenades", 
	"1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Whether players can purchase grenades from the buy menu or not.",
	true, 0,
	true, 1 );

ConVar mp_do_warmup_period( 
    side_registry(), "mp_do_warmup_period", 
    "1",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Whether or not to do a warmup period at the start of a match.",
    true, 0,
    true, 1 );

ConVar mp_do_warmup_offine( 
    side_registry(), "mp_do_warmup_offine", 
    "0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Whether or not to do a warmup period at the start of a match in an offline (bot) match.",
    true, 0,
    true, 1 );

ConVar mp_startmoney( 
	side_registry(), "mp_startmoney", 
	"800", 
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"amount of money each player gets when they reset",
	true, 0,
	false, 0 );

ConVar mp_maxmoney(
	side_registry(), "mp_maxmoney",
	"16000",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"maximum amount of money allowed in a player's account",
	true, 0,
	false, 0 );

ConVar mp_afterroundmoney(
	side_registry(), "mp_afterroundmoney",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"amount of money awared to every player after each round" );

ConVar mp_playercashawards(
	side_registry(), "mp_playercashawards",
	"1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Players can earn money by performing in-game actions" );

ConVar mp_teamcashawards(
	side_registry(), "mp_teamcashawards",
	"1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Teams can earn money by performing in-game actions" );

ConVar mp_overtime_enable(
	side_registry(), "mp_overtime_enable",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"If a match ends in a tie, use overtime rules to determine winner" );

ConVar mp_overtime_maxrounds(
	side_registry(), "mp_overtime_maxrounds",
	"6",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"When overtime is enabled play additional rounds to determine winner" );

ConVar mp_overtime_startmoney(
	side_registry(), "mp_overtime_startmoney",
	"10000",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Money assigned to all players at start of every overtime half" );

ConVar mp_hostages_takedamage( 
	side_registry(), "mp_hostages_takedamage", 
	"0", 
	FCVAR_REPLICATED | FCVAR_RELEASE, 
	"Whether or not hostages can be hurt." );

ConVar mp_hostages_moveable(
	side_registry(), "mp_hostages_moveable",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Whether or not hostages can move.");

ConVar mp_hostages_rescuetowin( 
	side_registry(), "mp_hostages_rescuetowin", 
	"1", 
	FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, 
	"0 == all alive, any other number is the number the CT's need to rescue to win the round." );

ConVar mp_hostages_rescuetime( 
	side_registry(), "mp_hostages_rescuetime", 
	"1", 
	FCVAR_REPLICATED | FCVAR_RELEASE, 
	"Additional time added to round time if a hostage is reached by a CT." );

ConVar mp_anyone_can_pickup_c4(
	side_registry(), "mp_anyone_can_pickup_c4",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"If set, everyone can pick up the c4, not just Ts." );

ConVar mp_c4_cannot_be_defused(
	side_registry(), "mp_c4_cannot_be_defused",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"If set, the planted c4 cannot be defused." );

ConVar sv_coaching_enabled(
	side_registry(), "sv_coaching_enabled",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Allows spectating and communicating with a team ( 'coach t' or 'coach ct' )"  );

ConVar sv_allow_thirdperson(
	side_registry(), "sv_allow_thirdperson",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Allows the server set players in third person mode without the client slamming it back (if cheats are on, all clients can set thirdperson without this convar being set)" );

ConVar sv_party_mode(
	side_registry(), "sv_party_mode",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Party!!" );

// ConVar mp_hostages_canpickup( 
// 	"mp_hostages_canpickup", 
// 	"1", 
// 	FCVAR_REPLICATED | FCVAR_RELEASE, 
// 	"1 = hostages are picked up when used, 0 == hostages uyse the old method of following behind players." );

static void mpwarmuptime_f( ConVar *pConVar, const char *pOldString, float flOldValue )
{
	if ( CSGameRules() )
	{ 
		CSGameRules()->SetWarmupPeriodStartTime( gpGlobals->curtime );
	}
}

ConVar mp_verbose_changelevel_spew( side_registry(), "mp_verbose_changelevel_spew", "1", FCVAR_RELEASE | FCVAR_REPLICATED );

ConVar mp_warmuptime( 
    side_registry(), "mp_warmuptime", 
    "30",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "How long the warmup period lasts. Changing this value resets warmup.",
    true, 5,
    false, 0,
	mpwarmuptime_f );

ConVar mp_warmuptime_all_players_connected(
	side_registry(), "mp_warmuptime_all_players_connected",
	"60",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Warmup time to use when all players have connected in official competitive. 0 to disable." );

ConVar mp_warmup_pausetimer( 
	side_registry(), "mp_warmup_pausetimer", 
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Set to 1 to stay in warmup indefinitely. Set to 0 to resume the timer." );

ConVar mp_halftime_pausetimer( 
	side_registry(), "mp_halftime_pausetimer", 
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Set to 1 to stay in halftime indefinitely. Set to 0 to resume the timer." );

ConVar mp_halftime_pausematch(
	side_registry(), "mp_halftime_pausematch",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Set to 1 to pause match after halftime countdown elapses. Match must be resumed by vote or admin." );

ConVar mp_overtime_halftime_pausetimer( 
	side_registry(), "mp_overtime_halftime_pausetimer", 
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"If set to 1 will set mp_halftime_pausetimer to 1 before every half of overtime. Set mp_halftime_pausetimer to 0 to resume the timer." );

ConVar mp_respawn_immunitytime(side_registry(), "mp_respawn_immunitytime", "4.0", FCVAR_RELEASE | FCVAR_REPLICATED, "How many seconds after respawn immunity lasts." );

ConVar mp_playerid(
    side_registry(), "mp_playerid",
    "0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Controls what information player see in the status bar: 0 all names; 1 team names; 2 no names",
    true, 0,
    true, 2 );

ConVar mp_playerid_delay(
    side_registry(), "mp_playerid_delay",
    "0.4",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of seconds to delay showing information in the status bar",
    true, 0,
    true, 1 );

ConVar mp_playerid_hold(
    side_registry(), "mp_playerid_hold",
    "0.2",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of seconds to keep showing old information in the status bar",
    true, 0,
    true, 1 );

ConVar mp_round_restart_delay(
    side_registry(), "mp_round_restart_delay",
    "7.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of seconds to delay before restarting a round after a win",
    true, 0.0f,
	true, 14.0f ); // 10 - 11 seconds is a comfortable minimum to fit the end of round replay

ConVar mp_halftime_duration(
    side_registry(), "mp_halftime_duration",
    "15.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of seconds that halftime lasts",
    true, 0.0f,
    true, 300.0f );

ConVar mp_match_can_clinch(
    side_registry(), "mp_match_can_clinch",
    "1",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Can a team clinch and end the match by being so far ahead that the other team has no way to catching up?" );

ConVar mp_ggtr_end_round_kill_bonus(
    side_registry(), "mp_ggtr_end_round_kill_bonus",
    "1",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of bonus points awarded in Demolition Mode when knife kill ends round",
    true, 0,
    true, 10 );

ConVar mp_ggtr_last_weapon_kill_ends_half(
    side_registry(), "mp_ggtr_last_weapon_kill_ends_half",
    "0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "End the half and give a team round point when a player makes a kill using the final weapon",
    true, 0,
    true, 1 );

ConVar mp_ggprogressive_round_restart_delay(
    side_registry(), "mp_ggprogressive_round_restart_delay",
    "15.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of seconds to delay before restarting a round after a win in gungame progessive",
    true, 0.0f,
    true, 90.0f );

ConVar mp_ggprogressive_use_random_weapons(
	side_registry(), "mp_ggprogressive_use_random_weapons",
	"1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"If set, selects random weapons from set categories for the progression order" );

ConVar mp_ggprogressive_random_weapon_kills_needed(
	side_registry(), "mp_ggprogressive_random_weapon_kills_needed",
	"2",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"If mp_ggprogressive_use_random_weapons is set, this is the number of kills needed with each weapon" );

ConVar mp_ggtr_num_rounds_autoprogress(
	side_registry(), "mp_ggtr_num_rounds_autoprogress",
	"3",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Upgrade the player's weapon after this number of rounds without upgrading" );

ConVar mp_ct_default_melee(
	side_registry(), "mp_ct_default_melee",
	"weapon_knife",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"The default melee weapon that the CTs will spawn with.  Even if this is blank, a knife will be given.  To give a taser, it should look like this: 'weapon_knife weapon_taser'.  Remember to set mp_weapons_allow_zeus to 1 if you want to give a taser!" );

ConVar mp_ct_default_secondary(
	side_registry(), "mp_ct_default_secondary",
	"weapon_hkp2000",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"The default secondary (pistol) weapon that the CTs will spawn with" );

ConVar mp_ct_default_primary(
	side_registry(), "mp_ct_default_primary",
	"",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"The default primary (rifle) weapon that the CTs will spawn with" );

ConVar mp_ct_default_grenades(
	side_registry(), "mp_ct_default_grenades",
	"",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"The default grenades that the CTs will spawn with.  To give multiple grenades, separate each weapon class with a space like this: 'weapon_molotov weapon_hegrenade'" );

ConVar mp_t_default_melee(
	side_registry(), "mp_t_default_melee",
	"weapon_knife",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"The default melee weapon that the Ts will spawn with" );

ConVar mp_t_default_secondary(
	side_registry(), "mp_t_default_secondary",
	"weapon_glock",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"The default secondary (pistol) weapon that the Ts will spawn with" );

ConVar mp_t_default_primary(
	side_registry(), "mp_t_default_primary",
	"",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"The default primary (rifle) weapon that the Ts will spawn with" );

ConVar mp_t_default_grenades(
	side_registry(), "mp_t_default_grenades",
	"",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"The default grenades that the Ts will spawn with.  To give multiple grenades, separate each weapon class with a space like this: 'weapon_molotov weapon_hegrenade'" );

ConVar mp_join_grace_time(
    side_registry(), "mp_join_grace_time",
    "0.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of seconds after round start to allow a player to join a game",
    true, 0.0f,
    true, 30.0f );

ConVar mp_win_panel_display_time(
    side_registry(), "mp_win_panel_display_time",
    "3",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "The amount of time to show the win panel between matches / halfs" );

ConVar mp_ggtr_bomb_pts_for_upgrade(
    side_registry(), "mp_ggtr_bomb_pts_for_upgrade",
    "2.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Kill points required to upgrade a player's weapon",
    true, 1.0,
    true, 10.0 );

ConVar mp_ggtr_bomb_pts_for_he(
    side_registry(), "mp_ggtr_bomb_pts_for_he",
    "3",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Kill points required in a round to get a bonus HE grenade",
    true, 1,
    true, 5);

ConVar mp_ggtr_bomb_pts_for_flash(
    side_registry(), "mp_ggtr_bomb_pts_for_flash",
    "4",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Kill points required in a round to get a bonus flash grenade",
    true, 1,
    true, 5);

ConVar mp_ggtr_bomb_pts_for_molotov(
    side_registry(), "mp_ggtr_bomb_pts_for_molotov",
    "5",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Kill points required in a round to get a bonus molotov cocktail",
    true, 1,
    true, 5);

ConVar mp_molotovusedelay( 
    side_registry(), "mp_molotovusedelay",
    "15.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of seconds to delay before the molotov can be used after acquiring it",
    true, 0.0,
    true, 30.0 );

ConVar mp_ggtr_halftime_delay(
    side_registry(), "mp_ggtr_halftime_delay",
    "0.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of seconds to delay during TR Mode halftime",
    true, 0.0,
    true, 30.0 );

ConVar mp_ggtr_bomb_respawn_delay(
    side_registry(), "mp_ggtr_bomb_respawn_delay",
    "0.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of seconds to delay before making the bomb available to a respawner in gun game",
    true, 0.0,
    true, 30.0 );

ConVar mp_ggtr_bomb_defuse_bonus(
    side_registry(), "mp_ggtr_bomb_defuse_bonus",
    "1.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of bonus upgrades to award the CTs when they defuse a gun game bomb",
    true, 1.0,
    true, 10.0 );

ConVar mp_ggtr_bomb_detonation_bonus(
    side_registry(), "mp_ggtr_bomb_detonation_bonus",
    "1.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of bonus upgrades to award the Ts when they detonate a gun game bomb",
    true, 1.0,
    true, 10.0 );

ConVar mp_dm_bonus_percent(
	side_registry(), "mp_dm_bonus_percent",
	"50",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Percent of points additionally awarded when someone gets a kill with the bonus weapon during the bonus period." );

ConVar mp_display_kill_assists(
	side_registry(), "mp_display_kill_assists",
	"1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Whether to display and score player assists",
	true, 0,
	true, 1 );

ConVar mp_match_end_restart(
    side_registry(), "mp_match_end_restart",
    "0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "At the end of the match, perform a restart instead of loading a new map",
    true, 0,
    true, 1 );

ConVar mp_match_end_changelevel(
	side_registry(), "mp_match_end_changelevel",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"At the end of the match, perform a changelevel even if next map is the same",
	true, 0,
	true, 1 );

ConVar mp_defuser_allocation(
    side_registry(), "mp_defuser_allocation",
    "0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "How to allocate defusers to CTs at start or round: 0=none, 1=random, 2=everyone",
    true, 0,
    true, 2 );

ConVar mp_give_player_c4(
	side_registry(), "mp_give_player_c4",
	"1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Whether this map should spawn a c4 bomb for a player or not.",
	true, 0,
	true, 1 );

ConVar mp_death_drop_gun(
    side_registry(), "mp_death_drop_gun",
    "1",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Which gun to drop on player death: 0=none, 1=best, 2=current or best",
    true, 0,
    true, 2 );

ConVar mp_death_drop_c4(
	side_registry(), "mp_death_drop_c4",
	"1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Whether c4 is droppable" );

ConVar mp_death_drop_grenade(
    side_registry(), "mp_death_drop_grenade",
    "2",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Which grenade to drop on player death: 0=none, 1=best, 2=current or best, 3=all grenades",
    true, 0,
    true, 3 );

ConVar mp_death_drop_defuser(
    side_registry(), "mp_death_drop_defuser",
    "1",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Drop defuser on player death",
    true, 0,
    true, 1 );

ConVar mp_coop_force_join_ct(
	side_registry(), "mp_coop_force_join_ct",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"If set, real players will auto join CT on join." );

ConVar mp_coopmission_mission_number(
	side_registry(), "mp_coopmission_mission_number",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Which mission the map should run after it loads." );

ConVar mp_force_pick_time(
    side_registry(), "mp_force_pick_time",
    "15",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "The amount of time a player has on the team screen to make a selection before being auto-teamed" );

ConVar bot_autodifficulty_threshold_low(
    side_registry(), "bot_autodifficulty_threshold_low",
    "-2.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Lower bound below Average Human Contribution Score that a bot must be below to change its difficulty",
    true, -20.0,
    true, 20.0 );

ConVar bot_autodifficulty_threshold_high(
    side_registry(), "bot_autodifficulty_threshold_high",
    "5.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Upper bound above Average Human Contribution Score that a bot must be above to change its difficulty",
    true, -20.0,
    true, 20.0 );

ConVar mp_weapons_allow_zeus(
	side_registry(), "mp_weapons_allow_zeus", 
	"1", 
	FCVAR_REPLICATED | FCVAR_RELEASE, 
	"Determines how many Zeus purchases a player can make per round (0 to disallow, -1 to have no limit)." );

ConVar mp_weapons_allow_typecount(
	side_registry(), "mp_weapons_allow_typecount",
	"5",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Determines how many purchases of each weapon type allowed per player per round (0 to disallow purchasing, -1 to have no limit)." );

ConVar mp_weapons_allow_map_placed(
	side_registry(), "mp_weapons_allow_map_placed", 
	"0", 
	FCVAR_REPLICATED | FCVAR_RELEASE, 
	"If this convar is set, when a match starts, the game will not delete weapons placed in the map." );

ConVar mp_default_team_winner_no_objective(
	side_registry(), "mp_default_team_winner_no_objective", 
	"-1", 
	FCVAR_REPLICATED | FCVAR_RELEASE, 
	"If the map doesn't define an objective (bomb, hostage, etc), the value of this convar will declare the winner when the time runs out in the round." );

ConVar mp_weapons_glow_on_ground(
	side_registry(), "mp_weapons_glow_on_ground", 
	"0", 
	FCVAR_REPLICATED | FCVAR_RELEASE, 
	"If this convar is set, weapons on the ground will have a glow around them." );

ConVar mp_respawn_on_death_t(
	side_registry(), "mp_respawn_on_death_t", 
	"0", 
	FCVAR_REPLICATED | FCVAR_RELEASE, 
	"When set to 1, terrorists will respawn after dying." );

ConVar mp_respawn_on_death_ct(
	side_registry(), "mp_respawn_on_death_ct", 
	"0", 
	FCVAR_REPLICATED | FCVAR_RELEASE, 
	"When set to 1, counter-terrorists will respawn after dying." );

ConVar mp_use_respawn_waves(
	side_registry(), "mp_use_respawn_waves", 
	"0", 
	FCVAR_REPLICATED | FCVAR_RELEASE, 
	"When set to 1, and that player's team is set to respawn, they will respawn in waves. If set to 2, teams will respawn when the whole team is dead." );

void ProhibitedItemsCallback( ConVar *var, const char *pOldValue, float flOldValue )
{
}

ConVar mp_items_prohibited(
	side_registry(), "mp_items_prohibited",
	"",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Set this convar to a comma-delimited list of definition indices of weapons that should be prohibited from use.",
	ProhibitedItemsCallback );

void RespawnWaveTimeCTCallback( ConVar *var, const char *pOldValue, float flOldValue );
void RespawnWaveTimeTCallback( ConVar *var, const char *pOldValue, float flOldValue );

extern ConVar mp_respawnwavetime_ct;
void RespawnWaveTimeCTCallback( ConVar *var, const char *pOldValue, float flOldValue )
{
	float flTime = mp_respawnwavetime_ct.GetFloat();
	if ( CSGameRules() )
	{
		float flNextRespawn = gpGlobals->curtime + flTime;
		CSGameRules()->SetNextTeamRespawnWaveDelay( TEAM_CT, flNextRespawn );
	}
}

extern ConVar mp_respawnwavetime_t;
void RespawnWaveTimeTCallback( ConVar *var, const char *pOldValue, float flOldValue )
{
	float flTime = mp_respawnwavetime_t.GetFloat();
	if ( CSGameRules() )
	{
		float flNextRespawn = gpGlobals->curtime + flTime;
		CSGameRules()->SetNextTeamRespawnWaveDelay( TEAM_TERRORIST, flNextRespawn );
	}
}

ConVar mp_respawnwavetime_ct( side_registry(), "mp_respawnwavetime_ct", "10.0", FCVAR_REPLICATED | FCVAR_RELEASE, "Time between respawn waves for CTs.", RespawnWaveTimeCTCallback );
ConVar mp_respawnwavetime_t( side_registry(), "mp_respawnwavetime_t", "10.0", FCVAR_REPLICATED | FCVAR_RELEASE, "Time between respawn waves for Terrorists.", RespawnWaveTimeTCallback );

ConVar mp_endmatch_votenextmap( 
		side_registry(), "mp_endmatch_votenextmap", 
		"1", 
		FCVAR_REPLICATED | FCVAR_RELEASE, 
		"Whether or not players vote for the next map at the end of the match when the final scoreboard comes up" );

ConVar mp_endmatch_votenextmap_keepcurrent( 
	side_registry(), "mp_endmatch_votenextmap_keepcurrent", 
	"1", 
	FCVAR_REPLICATED | FCVAR_RELEASE, 
	"If set, keeps the current map in the list of voting options.  If not set, the current map will not appear in the list of voting options." );

ConVar mp_endmatch_votenextleveltime(
	side_registry(), "mp_endmatch_votenextleveltime",
	"20",
	FCVAR_RELEASE | FCVAR_REPLICATED,
	"If mp_endmatch_votenextmap is set, players have this much time to vote on the next map at match end." );
	
// music controls

ConVar snd_music_boost(
    side_registry(), "snd_music_boost",
    "0",
    FCVAR_REPLICATED,
    "Specifies an amount to boost music volume by" );

ConVar snd_music_selection(
    side_registry(), "snd_music_selection", 
    "1", 
    FCVAR_ARCHIVE,
    "Tracking rotating music for players with no music packs equipped.");

extern ConVar cl_borrow_music_from_player_index;

ConVar sv_endmatch_item_drop_interval(
	side_registry(), "sv_endmatch_item_drop_interval",
	"1.0",
	FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY,
	"The time between drops on the end match scoreboard " );

ConVar sv_endmatch_item_drop_interval_rare(
	side_registry(), "sv_endmatch_item_drop_interval_rare",
	"1.0",
	FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY,
	"The time between drops on the end match scoreboard for rare items " );

ConVar sv_endmatch_item_drop_interval_mythical(
	side_registry(), "sv_endmatch_item_drop_interval_mythical",
	"1.25",
	FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY,
	"The time between drops on the end match scoreboard for mythical items " );

ConVar sv_endmatch_item_drop_interval_legendary(
	side_registry(), "sv_endmatch_item_drop_interval_legendary",
	"2.0",
	FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY,
	"The time between drops on the end match scoreboard for legendary items " );

ConVar sv_endmatch_item_drop_interval_ancient(
	side_registry(), "sv_endmatch_item_drop_interval_ancient",
	"3.5",
	FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY,
	"The time between drops on the end match scoreboard for ancient items " );

// bot difficulty tracking per user input device
ConVar sv_compute_per_bot_difficulty(
    side_registry(), "sv_compute_per_bot_difficulty",
    "0",
    FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY,
    "0 = compute all bot difficulties equally, 1 = compute unique bot difficulty for each bot " );

ConVar sv_show_bot_difficulty_in_name(
    side_registry(), "sv_show_bot_difficulty_in_name",
    "0",
    FCVAR_REPLICATED,
    "0 = hide bot difficulty in bot name, 1 = show bot difficulty in bot name" );

ConVar sv_bot_difficulty_kbm(
    side_registry(), "sv_bot_difficulty_kbm",
    "0",
    FCVAR_REPLICATED | FCVAR_HIDDEN,
    "Bot difficulty while playing with Keyboard/Mouse device" );

ConVar sv_bot_difficulty_gamepad(
    side_registry(), "sv_bot_difficulty_gamepad",
    "0",
    FCVAR_REPLICATED | FCVAR_HIDDEN,
    "Bot difficulty while playing with Gamepad device" );

ConVar sv_bot_difficulty_ps3move(
    side_registry(), "sv_bot_difficulty_ps3move",
    "0",
    FCVAR_REPLICATED | FCVAR_HIDDEN,
    "Bot difficulty while playing with PS3Move device" );

ConVar sv_bot_difficulty_hydra(
    side_registry(), "sv_bot_difficulty_hydra",
    "0",
    FCVAR_REPLICATED | FCVAR_HIDDEN,
    "Bot difficulty while playing with Hydra device" );

ConVar sv_bot_difficulty_sharpshooter(
    side_registry(), "sv_bot_difficulty_sharpshooter",
    "0",
    FCVAR_REPLICATED | FCVAR_HIDDEN,
    "Bot difficulty while playing with SharpShooter device" );

ConVar sv_competitive_official_5v5( side_registry(), "sv_competitive_official_5v5",
	"0",
	FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
	"Enable to force the server to show 5v5 scoreboards and allows spectators to see characters through walls." );

ConVar sv_kick_ban_duration( side_registry(), "sv_kick_ban_duration",
	"15",
	FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
	"How long should a kick ban from the server should last (in minutes)" );

ConVar sv_disable_immunity_alpha( side_registry(), "sv_disable_immunity_alpha",
							 "0",
							 FCVAR_REPLICATED | FCVAR_RELEASE,
							 "If set, clients won't slam the player model render settings each frame for immunity [mod authors use this]" );

ConVar cl_bot_difficulty_kbm(
    side_registry(), "cl_bot_difficulty_kbm",
    "0",
    FCVAR_HIDDEN,
    "Bot difficulty while playing with Keyboard/Mouse device" );

ConVar cl_bot_difficulty_gamepad(
    side_registry(), "cl_bot_difficulty_gamepad",
    "0",
    FCVAR_HIDDEN,
    "Bot difficulty while playing with Gamepad device" );

ConVar cl_bot_difficulty_ps3move(
    side_registry(), "cl_bot_difficulty_ps3move",
    "0",
    FCVAR_HIDDEN,
    "Bot difficulty while playing with PS3Move device" );

ConVar cl_bot_difficulty_hydra(
    side_registry(), "cl_bot_difficulty_hydra",
    "0",
    FCVAR_HIDDEN,
    "Bot difficulty while playing with Hydra device" );

ConVar cl_bot_difficulty_sharpshooter(
    side_registry(), "cl_bot_difficulty_sharpshooter",
    "0",
    FCVAR_HIDDEN,
    "Bot difficulty while playing with SharpShooter device" );

// Set game rules to allow all clients to talk to each other.
// Muted players still can't talk to each other.
ConVar sv_alltalk( side_registry(), "sv_alltalk", "0", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE, "Deprecated. Replaced with sv_talk_enemy_dead and sv_talk_enemy_living." );

// [jason] Can the dead speak to the living?
ConVar sv_deadtalk( side_registry(), "sv_deadtalk", "0",	FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE, "Dead players can speak (voice, text) to the living" );

// [jason] Override that removes all chat restrictions, including those for spectators
ConVar sv_full_alltalk( side_registry(), "sv_full_alltalk", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Any player (including Spectator team) can speak to any other player" );

ConVar sv_talk_enemy_dead( side_registry(), "sv_talk_enemy_dead", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Dead players can hear all dead enemy communication (voice, chat)" );
ConVar sv_talk_enemy_living( side_registry(), "sv_talk_enemy_living", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Living players can hear all living enemy communication (voice, chat)" );

ConVar sv_spec_hear( side_registry(), "sv_spec_hear", "1", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE, "Determines who spectators can hear: 0: only spectators; 1: all players; 2: spectated team; 3: self only; 4: nobody" );

ConVar mp_c4timer( 
	side_registry(), "mp_c4timer", 
	"40", 
	FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
	"how long from when the C4 is armed until it blows",
	true, 10,	// min value
	false, 0	// max value
	);

namespace SpecHear
{
    enum Type
    {
        OnlySpectators = 0,
        AllPlayers = 1,
        SpectatedTeam = 2,
		Self = 3,
		Nobody = 4,
    };
}

// NOTE: the indices here must match TEAM_TERRORIST, TEAM_CT, TEAM_SPECTATOR, etc.
const char *sTeamNames[] =
{
	"Unassigned",
	"Spectator",
	"TERRORIST",
	"CT"
};

ConVar cl_autowepswitch(
    side_registry(), "cl_autowepswitch",
    "1",
    FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE| FCVAR_SS | FCVAR_USERINFO,
    "Automatically switch to picked up weapons (if more powerful)" );

ConVar cl_use_opens_buy_menu(
	side_registry(), "cl_use_opens_buy_menu",
	"1",
	FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE| FCVAR_SS | FCVAR_USERINFO,
	"Pressing the +use key will open the buy menu if in a buy zone (just as if you pressed the 'buy' key)." );

ConVar cl_autohelp(
    side_registry(), "cl_autohelp",
    "1",
    FCVAR_ARCHIVE | FCVAR_USERINFO,
    "Auto-help" );

    ConVar cash_team_terrorist_win_bomb(
        side_registry(), "cash_team_terrorist_win_bomb",
        "3500",
		FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_team_elimination_hostage_map_t(
        side_registry(), "cash_team_elimination_hostage_map_t",
        "1000",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

	ConVar cash_team_elimination_hostage_map_ct(
		side_registry(), "cash_team_elimination_hostage_map_ct",
		"2000",
		FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_team_elimination_bomb_map(
        side_registry(), "cash_team_elimination_bomb_map",
        "3250",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

	ConVar cash_team_survive_guardian_wave(
		side_registry(), "cash_team_survive_guardian_wave",
		"1000",
		FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY );

    ConVar cash_team_win_by_time_running_out_hostage(
        side_registry(), "cash_team_win_by_time_running_out_hostage",
        "3250",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

	ConVar cash_team_win_by_time_running_out_bomb(
		side_registry(), "cash_team_win_by_time_running_out_bomb",
		"3250",
		FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_team_win_by_defusing_bomb(
        side_registry(), "cash_team_win_by_defusing_bomb",
        "3250",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_team_win_by_hostage_rescue(
        side_registry(), "cash_team_win_by_hostage_rescue",
        "3500",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_team_loser_bonus(
        side_registry(), "cash_team_loser_bonus",
        "1400",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_team_loser_bonus_consecutive_rounds(
        side_registry(), "cash_team_loser_bonus_consecutive_rounds",
        "500",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_team_rescued_hostage(
        side_registry(), "cash_team_rescued_hostage",
        "0",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_team_hostage_alive(
        side_registry(), "cash_team_hostage_alive",
        "0",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_team_planted_bomb_but_defused(
        side_registry(), "cash_team_planted_bomb_but_defused",
        "800",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_team_hostage_interaction(
        side_registry(), "cash_team_hostage_interaction",
        "500",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_player_killed_teammate(
        side_registry(), "cash_player_killed_teammate",
        "-300",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_player_killed_enemy_factor(
        side_registry(), "cash_player_killed_enemy_factor",
        "1",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_player_killed_enemy_default(
        side_registry(), "cash_player_killed_enemy_default",
        "300",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_player_bomb_planted(
        side_registry(), "cash_player_bomb_planted",
        "300",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_player_bomb_defused(
        side_registry(), "cash_player_bomb_defused",
        "300",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_player_rescued_hostage(
        side_registry(), "cash_player_rescued_hostage",
        "1000",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_player_interact_with_hostage(
        side_registry(), "cash_player_interact_with_hostage",
        "150",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_player_damage_hostage(
        side_registry(), "cash_player_damage_hostage",
        "-30",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_player_killed_hostage(
        side_registry(), "cash_player_killed_hostage",
        "-1000",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

	ConVar cash_player_respawn_amount(
		side_registry(), "cash_player_respawn_amount",
		"0",
		FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

	ConVar cash_player_get_killed(
		side_registry(), "cash_player_get_killed",
		"0",
		FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);
	
int CCSGameRules::TeamCashAwardValue( TeamCashAward::Type reason)
{
    switch( reason )
    {
    case TeamCashAward::TERRORIST_WIN_BOMB:				return cash_team_terrorist_win_bomb.GetInt();
    case TeamCashAward::ELIMINATION_HOSTAGE_MAP_T:		return cash_team_elimination_hostage_map_t.GetInt();
    case TeamCashAward::ELIMINATION_HOSTAGE_MAP_CT:		return cash_team_elimination_hostage_map_ct.GetInt();
    case TeamCashAward::ELIMINATION_BOMB_MAP:			return cash_team_elimination_bomb_map.GetInt();
    case TeamCashAward::WIN_BY_TIME_RUNNING_OUT_HOSTAGE:return cash_team_win_by_time_running_out_hostage.GetInt();
    case TeamCashAward::WIN_BY_TIME_RUNNING_OUT_BOMB:	return cash_team_win_by_time_running_out_bomb.GetInt();
    case TeamCashAward::WIN_BY_DEFUSING_BOMB:			return cash_team_win_by_defusing_bomb.GetInt();
    case TeamCashAward::WIN_BY_HOSTAGE_RESCUE:			return cash_team_win_by_hostage_rescue.GetInt();
    case TeamCashAward::LOSER_BONUS:					return cash_team_loser_bonus.GetInt();
    case TeamCashAward::LOSER_BONUS_CONSECUTIVE_ROUNDS:	return cash_team_loser_bonus_consecutive_rounds.GetInt();
    case TeamCashAward::RESCUED_HOSTAGE:				return cash_team_rescued_hostage.GetInt();
    case TeamCashAward::HOSTAGE_ALIVE:					return cash_team_hostage_alive.GetInt();
    case TeamCashAward::PLANTED_BOMB_BUT_DEFUSED:		return cash_team_planted_bomb_but_defused.GetInt();
    case TeamCashAward::HOSTAGE_INTERACTION:			return cash_team_hostage_interaction.GetInt();
	case TeamCashAward::SURVIVE_GUARDIAN_WAVE:			return cash_team_survive_guardian_wave.GetInt();

    default:
        contract_assert( false && "Unhandled TeamCashAwardReason" );
        return 0;
    };
}

int CCSGameRules::PlayerCashAwardValue( PlayerCashAward::Type reason)
{
    switch( reason )
    {
    case PlayerCashAward::NONE:						return 0;
    case PlayerCashAward::KILL_TEAMMATE:			return cash_player_killed_teammate.GetInt();
    case PlayerCashAward::KILLED_ENEMY:				return cash_player_killed_enemy_default.GetInt();
    case PlayerCashAward::BOMB_PLANTED:				return cash_player_bomb_planted.GetInt();
    case PlayerCashAward::BOMB_DEFUSED:				return cash_player_bomb_defused.GetInt();
    case PlayerCashAward::RESCUED_HOSTAGE:			return cash_player_rescued_hostage.GetInt();
    case PlayerCashAward::INTERACT_WITH_HOSTAGE:	return cash_player_interact_with_hostage.GetInt();
    case PlayerCashAward::DAMAGE_HOSTAGE:			return cash_player_damage_hostage.GetInt();
    case PlayerCashAward::KILL_HOSTAGE:				return cash_player_killed_hostage.GetInt();
	case PlayerCashAward::RESPAWN:					return cash_player_respawn_amount.GetInt();
	case PlayerCashAward::GET_KILLED:				return cash_player_get_killed.GetInt();
    default:
        contract_assert( false && "Unhandled PlayerCashAwardReason" );
        return 0;
    };
}

//////////////////////////////////////////////////////////////////////////
// Guardian mode getter functions for UI
//////////////////////////////////////////////////////////////////////////
int CCSGameRules::GetGuardianRequiredKills() const
{
	static ConVarRef mp_guardian_special_kills_needed( "mp_guardian_special_kills_needed" );

	return mp_guardian_special_kills_needed.GetInt();
}

int CCSGameRules::GetGuardianKillsRemaining() const
{
	return m_nGuardianModeSpecialKillsRemaining;
}

int CCSGameRules::GetGuardianSpecialWeapon() const
{
	return m_nGuardianModeSpecialWeaponNeeded;
}

//////////////////////////////////////////////////////////////////////////
// Gun game getter functions
//////////////////////////////////////////////////////////////////////////

// Determines the highest weapon index of all players in the arms race match
void CCSGameRules::CalculateMaxGunGameProgressiveWeaponIndex( void )
{
    m_iMaxGunGameProgressiveWeaponIndex = 0;

    if ( IsPlayingGunGameProgressive() )
    {
        // Loop through all players and find the max progressive weapon index
        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CCSPlayer* pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
            if ( pPlayer )
            {
                if ( pPlayer->GetPlayerGunGameWeaponIndex() > m_iMaxGunGameProgressiveWeaponIndex )
                {
                    m_iMaxGunGameProgressiveWeaponIndex = pPlayer->GetPlayerGunGameWeaponIndex();
                }
            }
        }
    }
}

int CCSGameRules::GetCurrentGunGameWeapon ( int nCurrentWeaponIndex, int nTeamID )
{
    if ( GetNumProgressiveGunGameWeapons( nTeamID ) == 0 )
    {
        // Don't process if weapon array is empty
        return -1;
    }

    if ( nCurrentWeaponIndex < 0 || nCurrentWeaponIndex >= GetNumProgressiveGunGameWeapons( nTeamID ) )
    {
        // Don't process if out of the array bounds
        return -1;
    }

    return GetProgressiveGunGameWeapon( nCurrentWeaponIndex, nTeamID );
}

int CCSGameRules::GetNextGunGameWeapon( int nCurrentWeaponIndex, int nTeamID )
{
    if ( GetNumProgressiveGunGameWeapons( nTeamID ) == 0 )
    {
        // Don't process if weapon array is empty
        return -1;
    }

    if ( nCurrentWeaponIndex < 0 || nCurrentWeaponIndex >= GetNumProgressiveGunGameWeapons( nTeamID ) - 1 )
    {
        // Don't process if already at last weapon or out of the array bounds
        return -1;
    }

    return GetProgressiveGunGameWeapon( nCurrentWeaponIndex + 1, nTeamID );
}

int CCSGameRules::GetPreviousGunGameWeapon( int nCurrentWeaponIndex, int nTeamID )
{
    if ( GetNumProgressiveGunGameWeapons( nTeamID ) == 0 )
    {
        // Don't process if weapon array is empty
        return -1;
    }

    if ( nCurrentWeaponIndex <= 0 || nCurrentWeaponIndex >= GetNumProgressiveGunGameWeapons( nTeamID ) )
    {
        // Don't process if already at first weapon or out of the array bounds
        return -1;
    }

    return GetProgressiveGunGameWeapon( nCurrentWeaponIndex - 1, nTeamID );
}

bool CCSGameRules::IsFinalGunGameProgressiveWeapon( int nCurrentWeaponIndex, int nTeamID )
{
    // Determine if the current weapon is the last in the list of gun game weapons
    if ( nCurrentWeaponIndex == GetNumProgressiveGunGameWeapons( nTeamID ) - 1 )
    {
        return true;
    }

    return false;
}

int CCSGameRules::GetGunGameNumKillsRequiredForWeapon( int nCurrentWeaponIndex, int nTeamID )
{
    if ( nCurrentWeaponIndex < 0 || nCurrentWeaponIndex > GetNumProgressiveGunGameWeapons( nTeamID ) - 1 )
    {
        // Don't process if out of the array bounds
        return -1;
    }

    return GetProgressiveGunGameWeaponKillRequirement( nCurrentWeaponIndex, nTeamID );
}

CBaseCombatWeapon *CCSGameRules::GetNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon )
{
    CBaseCombatWeapon *bestWeapon = nullptr;

    // search all the weapons looking for the closest next
    for ( int i = 0; i < MAX_WEAPONS; i++ )
    {
        CBaseCombatWeapon *weapon = pPlayer->GetWeapon(i);
        if ( !weapon )
            continue;

        if ( !weapon->CanBeSelected() || weapon == pCurrentWeapon )
            continue;

        if ( bestWeapon )
        {
            if ( weapon->GetSlot() < bestWeapon->GetSlot() )
            {
				int nAmmo = 0;
				if ( weapon->UsesClipsForAmmo1() )
				{
					nAmmo = weapon->Clip1();
				}
				else
				{
					if ( pPlayer )
					{
						nAmmo = weapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );
					}
					else
					{
						// No owner, so return how much primary ammo I have along with me.
						nAmmo =  weapon->GetPrimaryAmmoCount();
					}
				}

				if ( nAmmo > 0 )
				{
					bestWeapon = weapon;
				}
            }
            else if ( weapon->GetSlot() == bestWeapon->GetSlot() && weapon->GetPosition() < bestWeapon->GetPosition() )
            {
                bestWeapon = weapon;
            }
        }
        else
        {
            bestWeapon = weapon;
        }
    }

    return bestWeapon;
}

float CCSGameRules::GetMapRemainingTime()
{
    // if timelimit is disabled, return -1
    if ( mp_timelimit.GetInt() <= 0 )
        return -1;

    // timelimit is in minutes
    float flTimeLeft =  ( m_flGameStartTime + mp_timelimit.GetInt() * 60 ) - gpGlobals->curtime;

    // never return a negative value
    if ( flTimeLeft < 0 )
        flTimeLeft = 0;

    return flTimeLeft;
}

float CCSGameRules::GetMapElapsedTime( void )
{
    return gpGlobals->curtime;
}

float CCSGameRules::GetRoundRemainingTime() const
{
    return (float) (m_fRoundStartTime + m_iRoundTime) - gpGlobals->curtime; 
}

float CCSGameRules::GetRoundStartTime()
{
    return m_fRoundStartTime;
}

float CCSGameRules::GetRoundElapsedTime()
{
    if ( IsPlayingTraining() )
        return 0.0f;

    float remainingTime = m_iRoundTime - GetRoundRemainingTime();
    if ( remainingTime >= 0.0f )
        return remainingTime;
    else
        return 0.0f;
}

bool CCSGameRules::ShouldCollide( int collisionGroup0, int collisionGroup1 )
{
    if ( collisionGroup0 > collisionGroup1 )
    {
        // swap so that lowest is always first
        V_swap(collisionGroup0,collisionGroup1);
    }
    
    //Don't stand on COLLISION_GROUP_WEAPONs
    if( collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT &&
        collisionGroup1 == COLLISION_GROUP_WEAPON )
    {
        return false;
    }

    // TODO: make a CS-SPECIFIC COLLISION GROUP FOR PHYSICS PROPS THAT USE THIS COLLISION BEHAVIOR.

    if ( (collisionGroup0 == COLLISION_GROUP_PLAYER || collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT) &&
        collisionGroup1 == COLLISION_GROUP_PUSHAWAY )
    {
        return false;
    }

    if ( collisionGroup0 == COLLISION_GROUP_DEBRIS && collisionGroup1 == COLLISION_GROUP_PUSHAWAY )
    {
        // let debris and multiplayer objects collide
        return true;
    }

    return BaseClass::ShouldCollide( collisionGroup0, collisionGroup1 ); 
}

bool CCSGameRules::IsFreezePeriod()
{
    return m_bFreezePeriod;
}

bool CCSGameRules::IsWarmupPeriod() const
{
    if ( IsPlayingTraining() )
        return false;

    if ( IsPlayingOffline() && !mp_do_warmup_offine.GetBool() )
        return false;

    return m_bWarmupPeriod;
}

bool CCSGameRules::AllowTaunts( void )
{
	/** Removed for partner depot **/
	return false;
}

bool CCSGameRules::AllowThirdPersonCamera() 
{ 
	return sv_allow_thirdperson.GetBool(); 
}

bool CCSGameRules::IsGoodDownTime( void )
{
	if ( CSGameRules()->IsGameRestarting() )
		return true;

	if ( CSGameRules()->IsIntermission() )
		return true;

	if ( CSGameRules()->InRoundRestart() )
		return true;

	if ( ( ( CSGameRules()->GetRoundRestartTime() - gpGlobals->curtime ) < 0.5f
		&& ( CSGameRules()->GetRoundRestartTime() - gpGlobals->curtime ) > 0.0f ) )
		return true;

	return false;
}

bool CCSGameRules::IsLoadoutAllowed( void )
{
	// main menu
	if ( !engine->IsConnected() )
		return true;
		
	CCSPlayer *pLocalPlayer = CCSPlayer::GetLocalCSPlayer();

	if ( !pLocalPlayer )
		return false;

	// non players
	if ( 	( pLocalPlayer->IsSpectator() )		||
			( pLocalPlayer->IsHLTV() )			||
			( engine->IsPlayingDemo() )	 )
	{
		return true;
	}

	// game mode specific rules
	if ( IsPlayingAnyCompetitiveStrictRuleset() )
	{
		return ( IsWarmupPeriod() );
	}
	else if ( ( IsPlayingGunGameDeathmatch() || IsPlayingCooperativeGametype() ) )
	{
		return true;
	}
	else if ( IsWarmupPeriod() || !pLocalPlayer->IsAlive() )
	{
		return true;
	}

	return false;
}

float CCSGameRules::GetWarmupPeriodEndTime() const
{
    return m_fWarmupPeriodStart + mp_warmuptime.GetFloat();
}

bool CCSGameRules::IsWarmupPeriodPaused()
{
	return mp_warmup_pausetimer.GetBool();
}

bool CCSGameRules::IsConnectedUserInfoChangeAllowed( CBasePlayer *pPlayer )
{
	int iLocalPlayerTeam = GetLocalPlayerTeam();
	if ( ( iLocalPlayerTeam == TEAM_TERRORIST ) || ( iLocalPlayerTeam == TEAM_CT ) )
		return false;
	return true;
}

AcquireResult::Type CCSGameRules::IsWeaponAllowed( const CCSWeaponInfo *pWeaponInfo, int nTeamNumber, CEconItemView *pItem )
{
	CSWeaponID	weaponId = WEAPON_NONE;
	CSWeaponType weaponType = WEAPONTYPE_UNKNOWN;
	if ( pItem && pItem->IsValid() )
	{
		weaponId = WeaponIdFromString( pItem->GetStaticData()->GetItemClass() );
		if ( pWeaponInfo )
			weaponType = pWeaponInfo->GetWeaponType( pItem );
	}
	else if ( pWeaponInfo )
	{
		weaponId = pWeaponInfo->m_weaponId;
		weaponType = pWeaponInfo->GetWeaponType( pItem );
	}

	// prohibited items. currently only supports schema items
	//
	//

	if ( ( CSGameRules()->m_arrProhibitedItemIndices[ 0 ] != 0 ) && pItem && pItem->IsValid() )
	{

		int nPosition = pItem->GetItemDefinition()->GetLoadoutSlot( nTeamNumber );
		const CEconItemView* pBaseItem = CSInventoryManager()->GetBaseItemForTeam( nTeamNumber, nPosition );

		if ( pBaseItem && pBaseItem->IsValid() )
		{
			for ( int j = 0; j < MAX_PROHIBITED_ITEMS; j++ )
			{
				// if the base item is prohibited then the slot is prohibited
				if ( CSGameRules()->m_arrProhibitedItemIndices[ j ] == pBaseItem->GetItemDefinition()->GetDefinitionIndex() )
				{
					return AcquireResult::NotAllowedByProhibition;
				}

				// if the base item is not prohibited then the alternate item might still be prohibited
				if ( CSGameRules()->m_arrProhibitedItemIndices[ j ] == pItem->GetItemDefinition()->GetDefinitionIndex() )
				{
					return AcquireResult::NotAllowedByProhibition;
				}

			}
		}
	}
	///////////////////////////////

	switch ( weaponId )
	{
	case ITEM_DEFUSER:
	case ITEM_CUTTERS:
		if ( nTeamNumber == TEAM_TERRORIST )
		{
			return AcquireResult::NotAllowedByTeam;
		}
		if ( !IsBombDefuseMap() )
		{
			return AcquireResult::NotAllowedByMap;
		}
		break;

	case WEAPON_TASER:
		// special case for limiting taser to classic casual; data drive this if it becomes more complex
		if ( !mp_weapons_allow_zeus.GetBool() )
		{
			return AcquireResult::NotAllowedForPurchase;
		}
		break;

	case WEAPON_C4:
		if ( nTeamNumber != TEAM_TERRORIST && !IsPlayingTraining() && mp_anyone_can_pickup_c4.GetBool() == false )
		{
			return AcquireResult::NotAllowedByTeam;
		}
		break;
	}

	return AcquireResult::Allowed;
}

bool CCSGameRules::IsBombDefuseMap() const
{
    return m_bMapHasBombTarget;
}

bool CCSGameRules::IsHostageRescueMap() const
{
    return m_bMapHasRescueZone;
}

bool CCSGameRules::MapHasBuyZone() const
{
    return m_bMapHasBuyZone;
}

bool CCSGameRules::CanSpendMoneyInMap()
{
	if ( GetMaxMoney() <= 0 )
		return false;

    if ( PlayerCashAwardsEnabled() == false && TeamCashAwardsEnabled() == false )
    	return false;

	if ( IsPlayingCoopMission() )
		return false;

	if ( IsPlayingCoopGuardian() )
	{
		int nTeam = CSGameRules()->IsHostageRescueMap() ? TEAM_TERRORIST : TEAM_CT;
		ConVarRef sv_buy_status_override( "sv_buy_status_override" );
		int iBuyStatus = sv_buy_status_override.GetInt();
		if ( iBuyStatus > 0 && (( nTeam == TEAM_CT && iBuyStatus != 1 ) || ( nTeam == TEAM_TERRORIST && iBuyStatus != 2 )) )
			return false;
	}

    return true;
}

bool CCSGameRules::IsLogoMap() const
{
    return m_bLogoMap;
}

float CCSGameRules::GetBuyTimeLength()
{
	if ( IsWarmupPeriod() )
	{
		if ( IsWarmupPeriodPaused( ) )
			return GetWarmupPeriodEndTime( )-m_fWarmupPeriodStart;

		if ( mp_buytime.GetFloat() < GetWarmupPeriodEndTime() )
			return GetWarmupPeriodEndTime();
	}

    return mp_buytime.GetFloat();
}

bool CCSGameRules::IsBuyTimeElapsed()
{
	if ( IsWarmupPeriod() && IsWarmupPeriodPaused() )
		return false;

    return ( GetRoundElapsedTime() > GetBuyTimeLength() );
}

bool CCSGameRules::IsMatchWaitingForResume()
{
	if ( m_bMatchWaitingForResume )
		return true;

	if ( sv_matchpause_auto_5v5.GetBool() && !IsWarmupPeriod() )
	{
		CTeam
			*arrTeams[2] = { GetGlobalTeam( TEAM_CT ), GetGlobalTeam( TEAM_TERRORIST ) };
		for ( int iTeam = 0; iTeam < std::size( arrTeams ); ++ iTeam )
		{
			if ( !arrTeams[iTeam] ) continue;
			int numPlayers = arrTeams[iTeam]->GetNumPlayers();
			if ( numPlayers < 5 )
			{
				SetMatchWaitingForResume( true ) ;
				return true;
			}
			int numHumans = 0;
			for ( int iPlayer = 0; iPlayer < numPlayers; ++ iPlayer )
			{
				CBasePlayer *pPlayer = arrTeams[iTeam]->GetPlayer( iPlayer );
				if ( !pPlayer ) continue;
				if ( pPlayer->IsBot() ) continue;
				++ numHumans;
			}
			if ( numHumans < 5 )
			{
				SetMatchWaitingForResume( true );
				return true;
			}
		}
	}

	return false;
}

int CCSGameRules::DefaultFOV()
{
    return 90;
}

const CViewVectors* CCSGameRules::GetViewVectors() const
{
    return &g_CSViewVectors;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CCSGameRules::GetRespawnTimeScalar( int iTeam )
{
	if ( mp_use_respawn_waves.GetInt() == 2 )
		return 1;

	// For long respawn times, scale the time as the number of players drops
	int iOptimalPlayers = 8;	// 16 players total, 8 per team

	int iNumPlayers = GetGlobalTeam(iTeam)->GetNumPlayers();

	float flScale = RemapValClamped( iNumPlayers, 1, iOptimalPlayers, 0.25, 1.0 );
	return flScale;
}

bool CCSGameRules::IsEndMatchVotingForNextMapEnabled()
{
	if ( mp_endmatch_votenextmap.GetBool() )
	{
		//int numMaps = 0;
		if ( g_pGameTypes )
		{
			const char* mapGroupName = engine->GetMapGroupName();
			const std::vector< std::string >* mapsInGroup = g_pGameTypes->GetMapGroupMapList( mapGroupName );
			if ( mapsInGroup )
				return (std::ssize( *mapsInGroup ) > 1);
		}
	}

	return false;
}

bool CCSGameRules::IsEndMatchVotingForNextMap()
{
	if ( !IsEndMatchVotingForNextMapEnabled() || GetGamePhase() != GAMEPHASE_MATCH_ENDED )
		return false;

	if ( GetCMMItemDropRevealEndTime() > gpGlobals->curtime )
		return false;

	if ( m_bIsDroppingItems )
		return false;

// 	// TODO: add a check to make sure items aren't dropping
// 	if ( IsEndMatchVotingForNextMapEnabled() && ( GetGamePhase() == GAMEPHASE_MATCH_ENDED ) && 
// 		((gpGlobals->curtime+m_timeUntilNextPhaseStarts) >= gpGlobals->curtime+mp_win_panel_display_time.GetFloat()) && gpGlobals->curtime+GetCMMItemDropRevealEndTime() < gpGlobals->curtime )
// 	{
// 		return true;
// 	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: How long are the respawn waves for this team currently?
//-----------------------------------------------------------------------------
float CCSGameRules::GetRespawnWaveMaxLength( int iTeam, bool bScaleWithNumPlayers /* = true */ )
{
	//Let's just turn off respawn times while players are messing around waiting for the tournament to start
	if ( IsWarmupPeriod() || m_bFreezePeriod )
		return 0.0f;

	if ( mp_use_respawn_waves.GetInt() == 0 )
		return 0.0f;

	float flWaveTime = 0;
	if ( iTeam == TEAM_TERRORIST )
		flWaveTime = mp_respawnwavetime_t.GetFloat();
	else if ( iTeam == TEAM_CT )
		flWaveTime = mp_respawnwavetime_ct.GetFloat();

	//float flTime = ( ( m_TeamRespawnWaveTimes[iTeam] >= 0 ) ? m_TeamRespawnWaveTimes[iTeam] : flWaveTime );
	float flTime = flWaveTime;

	// For long respawn times, scale the time as the number of players drops
	if ( bScaleWithNumPlayers && flTime > 5 )
	{
		flTime = std::max<std::common_type_t<int, float>>( 5, flTime * GetRespawnTimeScalar(iTeam) );
	}

	return flTime;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CCSGameRules::GetMinTimeWhenPlayerMaySpawn( CBasePlayer *pPlayer )
{
	// Min respawn time is the sum of
	//
	// a) the length of one full *unscaled* respawn wave for their team
	//		and
	// b) death anim length + freeze panel length

	float fMinDelay = 2.0f + GetRespawnWaveMaxLength( pPlayer->GetTeamNumber(), false );

	return pPlayer->GetDeathTime() + fMinDelay;
}

void CCSGameRules::SetNextTeamRespawnWaveDelay( int iTeam, float flDelay )
{
	float flNextRespawn = gpGlobals->curtime + flDelay;
	m_flNextRespawnWave.Set( iTeam, flNextRespawn );
}

//-----------------------------------------------------------------------------
// Purpose: don't let us spawn before our freezepanel time would have ended, even if we skip it
//-----------------------------------------------------------------------------
float CCSGameRules::GetNextRespawnWave( int iTeam, CBasePlayer *pPlayer ) 
{ 
	// 	if ( State_Get() == GR_STATE_STALEMATE )
	// 		return 0;

	// If we are purely checking when the next respawn wave is for this team
	if ( pPlayer == nullptr )
	{
		return m_flNextRespawnWave[iTeam];
	}

	// The soonest this player may spawn
	float flMinSpawnTime = GetMinTimeWhenPlayerMaySpawn( pPlayer );

	// the next scheduled respawn wave time
	float flNextRespawnTime = m_flNextRespawnWave[iTeam];

	// the length of one respawn wave. We'll check in increments of this
	float flRespawnWaveMaxLen = GetRespawnWaveMaxLength( iTeam );

	if ( flRespawnWaveMaxLen <= 0 )
	{
		return flNextRespawnTime;
	}

	// Keep adding the length of one respawn until we find a wave that
	// this player will be eligible to spawn in.
	while ( flNextRespawnTime < flMinSpawnTime )
	{
		flNextRespawnTime += flRespawnWaveMaxLen; 
	}

	return flNextRespawnTime; 
}
//-----------------------------------------------------------------------------
// Purpose: Init CS ammo definitions
//-----------------------------------------------------------------------------

// shared ammo definition
// JAY: Trying to make a more physical bullet response
template <typename T1> [[gnu::always_inline]] inline auto BULLET_MASS_GRAINS_TO_LB( T1 grains ) { return (0.002285*(grains)/16.0f); }
#define BULLET_MASS_GRAINS_TO_KG(grains)	lbs2kg(BULLET_MASS_GRAINS_TO_LB(grains))

static CCSAmmoDef ammoDef;
CCSAmmoDef* GetCSAmmoDef()
{
    GetAmmoDef(); // to initialize the ammo info
    return &ammoDef;
}

CAmmoDef* GetAmmoDef()
{
    static bool bInitted = false;

    if ( !bInitted )
    {
        bInitted = true;
        
        ammoDef.AddAmmoType( BULLET_PLAYER_50AE,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_50AE_max",		"ammo_50AE_impulse", 0, 10, 14 );
        ammoDef.AddAmmoType( BULLET_PLAYER_762MM,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_762mm_max",	"ammo_762mm_impulse", 0, 10, 14 );//Sniper ammo has little force since it just goes through the target.
        ammoDef.AddAmmoType( BULLET_PLAYER_556MM,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_556mm_max",	"ammo_556mm_impulse", 0, 10, 14 );
		ammoDef.AddAmmoType( BULLET_PLAYER_556MM_SMALL,	DMG_BULLET, TRACER_LINE, 0, 0, "ammo_556mm_small_max","ammo_556mm_impulse", 0, 10, 14 ); // 15 round clip
        ammoDef.AddAmmoType( BULLET_PLAYER_556MM_BOX,	DMG_BULLET, TRACER_LINE, 0, 0, "ammo_556mm_box_max","ammo_556mm_box_impulse", 0, 10, 14 );
        ammoDef.AddAmmoType( BULLET_PLAYER_338MAG,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_338mag_max",	"ammo_338mag_impulse", 0, 12, 16 );
        ammoDef.AddAmmoType( BULLET_PLAYER_9MM,			DMG_BULLET, TRACER_LINE, 0, 0, "ammo_9mm_max",		"ammo_9mm_impulse", 0, 5, 10 );
        ammoDef.AddAmmoType( BULLET_PLAYER_BUCKSHOT,	DMG_BULLET, TRACER_LINE, 0, 0, "ammo_buckshot_max", "ammo_buckshot_impulse", 0, 3, 6 );
        ammoDef.AddAmmoType( BULLET_PLAYER_45ACP,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_45acp_max",	"ammo_45acp_impulse", 0, 6, 10 );
        ammoDef.AddAmmoType( BULLET_PLAYER_357SIG,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_357sig_max",	"ammo_357sig_impulse", 0, 4, 8 );
        ammoDef.AddAmmoType( BULLET_PLAYER_357SIG_SMALL,DMG_BULLET, TRACER_LINE, 0, 0, "ammo_357sig_small_max",	"ammo_357sig_impulse", 0, 4, 8 );
        ammoDef.AddAmmoType( BULLET_PLAYER_357SIG_MIN,	DMG_BULLET, TRACER_LINE, 0, 0, "ammo_357sig_min_max",	"ammo_357sig_impulse", 0, 4, 8 );
        ammoDef.AddAmmoType( BULLET_PLAYER_57MM,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_57mm_max",		"ammo_57mm_impulse", 0, 4, 8 );
        ammoDef.AddAmmoType( AMMO_TYPE_HEGRENADE,		DMG_BLAST,	TRACER_LINE, 0, 0, "ammo_grenade_limit_default", 0, 0, 0 );
        ammoDef.AddAmmoType( AMMO_TYPE_FLASHBANG,		0,			TRACER_LINE, 0,	0, "ammo_grenade_limit_flashbang", 0, 0, 0 );
        ammoDef.AddAmmoType( AMMO_TYPE_SMOKEGRENADE,	0,			TRACER_LINE, 0, 0, "ammo_grenade_limit_default", 0, 0, 0 );
        ammoDef.AddAmmoType( AMMO_TYPE_MOLOTOV,			DMG_BURN,	TRACER_NONE, 0, 0, "ammo_grenade_limit_default", 0, 0, 0 );
        ammoDef.AddAmmoType( AMMO_TYPE_DECOY,			0,			TRACER_NONE, 0, 0, "ammo_grenade_limit_default", 0, 0, 0 );
        ammoDef.AddAmmoType( AMMO_TYPE_TASERCHARGE,		DMG_SHOCK,	TRACER_BEAM, 0, 0, 0, 0, 0, 0 );
		ammoDef.AddAmmoType( BULLET_PLAYER_357SIG_P250,	DMG_BULLET, TRACER_LINE, 0, 0, "ammo_357sig_p250_max",	"ammo_357sig_impulse", 0, 4, 8 );
		ammoDef.AddAmmoType( AMMO_TYPE_HEALTHSHOT, 0, TRACER_LINE, 0, 0, "ammo_item_limit_healthshot", 0, 0, 0 );
		ammoDef.AddAmmoType( AMMO_TYPE_TAGRENADE,	0,			TRACER_NONE, 0, 0, "ammo_grenade_limit_default", 0, 0, 0 );

        //Adrian: I set all the prices to 0 just so the rest of the buy code works
        //This should be revisited.
        ammoDef.AddAmmoCost( BULLET_PLAYER_50AE, 0, 7 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_762MM, 0, 30 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_556MM, 0, 30 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_556MM_SMALL, 0, 15 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_556MM_BOX, 0, 30 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_338MAG, 0, 10 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_9MM, 0, 30 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_BUCKSHOT, 0, 8 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_45ACP, 0, 25 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_357SIG, 0, 13 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_357SIG_P250, 0, 13 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_357SIG_SMALL, 0, 13 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_357SIG_MIN, 0, 12 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_57MM, 0, 50 );
    }

    return &ammoDef;
}

bool CCSGameRules::IsRoundOver() const
{
    return m_iRoundWinStatus != WINNER_NONE;
}

void CCSGameRules::ClearItemsDroppedDuringMatch( void )
{ 
	m_ItemsPtrDroppedDuringMatch.clear();
}

void CCSGameRules::RecordPlayerItemDrop( const ks::net::CEconItemPreviewDataBlock &iteminfo )
{
	if ( !iteminfo.accountid )
		return;

	for ( int i = 0; i < std::ssize( m_ItemsPtrDroppedDuringMatch ); i++ )
	{
		// if we've recorded this item already, don't record it again
		if ( m_ItemsPtrDroppedDuringMatch[i]->itemid == iteminfo.itemid &&
			m_ItemsPtrDroppedDuringMatch[i]->accountid == iteminfo.accountid )
			return;
	}

	// on the client, we add all local player items to the top of the list
	// server just adds them
	// don't put the local player's at the top anymore - matt wood
// 	CCSPlayer *pLocalPlayer = CCSPlayer::GetLocalCSPlayer();
// 	CCS_PlayerResource *cs_PR = dynamic_cast<CCS_PlayerResource *>( g_PR );
// 	if ( cs_PR && pLocalPlayer )
// 	{
// 		XUID localXuid = cs_PR->GetXuid( pLocalPlayer->entindex() );
// 		if (localXuid == steamOwnerID.ConvertToUint64())
// 		{
// 			m_ItemsPtrDroppedDuringMatch.AddToHead( event );
// 			return;
// 		}		
// 	}

	if ( CDemoPlaybackParameters_t const *pParams = engine->GetDemoPlaybackParameters() )
	{	// When playing back Overwatch with anonymous player identities don't collect drops on client
		if ( pParams->m_bAnonymousPlayerIdentity )
			return;
	}

	// check to see if this player is still connected
	bool bFoundPlayer = false;
	for ( int j = 1; j <= MAX_PLAYERS; j++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( j ) );
		if ( pPlayer )
		{
			CPlayerId playerUID;
			pPlayer->GetPlayerID( &playerUID );
			if ( playerUID.GetAccountID() == iteminfo.accountid )
			{
				bFoundPlayer = true;
				break;
			}
		}
	}
	
	// check to see if this player is still connected
	if ( bFoundPlayer )
		m_ItemsPtrDroppedDuringMatch.push_back( std::make_unique< ks::net::CEconItemPreviewDataBlock >( iteminfo ) );

}

bool CCSGameRules::IsCSGOBirthday( void )
{
	return sv_party_mode.GetBool();
}

int CCSGameRules::GetStartMoney( void )
{
	return IsWarmupPeriod() ? GetMaxMoney() : ( GetOvertimePlaying() ? mp_overtime_startmoney.GetInt() : mp_startmoney.GetInt() );
}

int	 CCSGameRules::GetMaxMoney( void )
{
	return mp_maxmoney.GetInt();
}

int  CCSGameRules::GetBetweenRoundMoney( void )
{
	return mp_afterroundmoney.GetInt();
}

bool CCSGameRules::PlayerCashAwardsEnabled( void )
{
	return mp_playercashawards.GetBool();
}

bool CCSGameRules::TeamCashAwardsEnabled( void )
{
	return mp_teamcashawards.GetBool();
}

bool CCSGameRules::IsPlayingGunGameProgressive( void ) const
{
    return ( IsPlayingGunGame() &&
             g_pGameTypes->GetCurrentGameMode() == CS_GameMode::GunGame_Progressive );
}

bool CCSGameRules::IsPlayingGunGameDeathmatch( void ) const
{
	return ( IsPlayingGunGame() &&
		g_pGameTypes->GetCurrentGameMode() == CS_GameMode::GunGame_Deathmatch );
}

bool CCSGameRules::IsPlayingGunGameTRBomb( void ) const
{
    return ( IsPlayingGunGame() &&
             g_pGameTypes->GetCurrentGameMode() == CS_GameMode::GunGame_Bomb );
}

bool CCSGameRules::IsPlayingClassicCasual( void ) const
{
    return ( IsPlayingClassic() &&
        g_pGameTypes->GetCurrentGameMode() == CS_GameMode::Classic_Casual);
}

bool CCSGameRules::IsPlayingAnyCompetitiveStrictRuleset( void ) const
{
    return ( IsPlayingClassic() && ( (g_pGameTypes->GetCurrentGameMode() == CS_GameMode::Classic_Competitive) ) );
}

bool CCSGameRules::IsPlayingCoopGuardian( void ) const
{
	return ( IsPlayingCooperativeGametype() &&
			 g_pGameTypes->GetCurrentGameMode() == CS_GameMode::Cooperative_Guardian );
}

bool CCSGameRules::IsPlayingCoopMission( void ) const
{
	return ( IsPlayingCooperativeGametype() &&
			 g_pGameTypes->GetCurrentGameMode() == CS_GameMode::Cooperative_Mission );
}

bool CCSGameRules::ShouldRecordMatchStats( void ) const
{
	return (	IsPlayingCoopMission() || (IsPlayingClassic() &&
				( g_pGameTypes->GetCurrentGameMode() == CS_GameMode::Classic_Competitive )) &&
				( !IsWarmupPeriod() ) &&
				( m_nOvertimePlaying == 0 ) &&
				( GetTotalRoundsPlayed() < MAX_MATCH_STATS_ROUNDS ) );
}

bool CCSGameRules::IsQueuedMatchmaking() const
{
	return m_bIsQueuedMatchmaking.Get();
}

bool CCSGameRules::IsValveDS() const
{
	return m_bIsValveDS.Get();
}

bool CCSGameRules::IsQuestEligible() const
{
	return m_bIsQuestEligible.Get();
}

bool CCSGameRules::IsPlayingOffline( void ) const
{
    return !game_online.GetBool();
}

bool CCSGameRules::IsAwardsProgressAllowedForBotDifficulty( void ) const
{
    return ( !IsPlayingOffline() || ( GetCustomBotDifficulty() >= CUSTOM_BOT_MIN_DIFFICULTY_FOR_AWARDS_PROGRESS ) );
}

bool CCSGameRules::IsPlayingCustomGametype( void ) const
{
	return ( g_pGameTypes->GetCurrentGameType() == CS_GameType_Custom );
}

bool CCSGameRules::IsPlayingTraining( void ) const
{
    return ( g_pGameTypes->GetCurrentGameType() == CS_GameType_Training );
}

bool CCSGameRules::IsPlayingClassic( void ) const
{
    return ( g_pGameTypes->GetCurrentGameType() == CS_GameType_Classic );
}

bool CCSGameRules::IsPlayingGunGame( void ) const
{
    return ( g_pGameTypes->GetCurrentGameType() == CS_GameType_GunGame );
}

bool CCSGameRules::IsPlayingCooperativeGametype( void ) const
{
	return ( g_pGameTypes->GetCurrentGameType() == CS_GameType_Cooperative );
}

int	CCSGameRules::GetCustomBotDifficulty( void ) const
{
    return ( g_pGameTypes->GetCustomBotDifficulty() );
}

bool CCSGameRules::ForceSplitScreenPlayersOnToSameTeam( void )
{
    return game_online.GetBool();
}

ConVar mp_solid_teammates(side_registry(), "mp_solid_teammates", "1", FCVAR_REPLICATED | FCVAR_RELEASE, "Determines whether teammates are solid or not." );
ConVar mp_free_armor(side_registry(), "mp_free_armor", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Determines whether armor and helmet are given automatically." );
ConVar mp_halftime(side_registry(), "mp_halftime", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Determines whether the match switches sides in a halftime event.");
ConVar mp_randomspawn(side_registry(), "mp_randomspawn", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Determines whether players are to spawn. 0 = default; 1 = both teams; 2 = Terrorists; 3 = CTs." );
ConVar mp_randomspawn_los(side_registry(), "mp_randomspawn_los", "1", FCVAR_REPLICATED | FCVAR_RELEASE, "If using mp_randomspawn, determines whether to test Line of Sight when spawning." );
ConVar mp_randomspawn_dist( side_registry(), "mp_randomspawn_dist", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "If using mp_randomspawn, determines whether to test distance when selecting this spot." );

// Returns true if teammates are solid obstacles in the current game mode
bool CCSGameRules::IsTeammateSolid( void ) const
{
    return mp_solid_teammates.GetBool();
}

// Returns true if armor is given automatically.
bool CCSGameRules::IsArmorFree( void ) const
{
    return mp_free_armor.GetBool();
}

// Returns true if the game is to be split into two halves.
bool CCSGameRules::HasHalfTime( void ) const
{
	return mp_halftime.GetBool();
}

int CCSGameRules::GetWeaponScoreForDeathmatch( int nPos )
{
	int nScore = 1;

	CSWeaponID wepID = WEAPON_NONE;

	for ( int i = 0; i < GetItemSchema()->GetItemDefinitionCount(); i++ )
	{
		CCStrike15ItemDefinition *pItemDef = ( CCStrike15ItemDefinition * )GetItemSchema()->GetItemDefinitionByMapIndex( i );

		if ( pItemDef->GetDefaultLoadoutSlot() == nPos )
		{
			wepID = WeaponIdFromString( pItemDef->GetItemClass() );
			break;
		}
	}

	if ( wepID == WEAPON_NONE )
		return 0;

	const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( wepID );
	if ( pWeaponInfo )
	{
		// 					float flScore1 = ((pWeaponInfo->m_flCycleTime / pWeaponInfo->m_iDamage)-0.001f) * 10;
		// 					float flScore2 = ((pWeaponInfo->m_iKillAward / std::max( 500, pWeaponInfo->m_iWeaponPrice )) + flScore1) * 100;
		// 					int nScore = std::max( 1, ceil(flScore2) );
		// 					pPlayer->AddContributionScore( nScore );

		if ( wepID == WEAPON_KNIFE )
		{
			nScore = 20;
		}
		else
		{
			int nPrice = std::min( 4500, std::max(100, pWeaponInfo->GetWeaponPrice() - (pWeaponInfo->GetKillAward()/2)) );
			float flScore1 = std::max( 0.05f, ((1 - ((float)nPrice/4500.0f)) * 10)/5);
			float flScore2 = flScore1 + std::min( 2.0f, (((pWeaponInfo->GetCycleTime() / pWeaponInfo->GetDamage() ) *10 ) + ( ( 2.0f-pWeaponInfo->GetArmorRatio() ) /2 ) ) / 4 );
			float flFinal = std::ceil(flScore2-0.5f);//ceil((flScore2/6) * 10 );
			nScore = std::max<std::common_type_t<int, float>>( 0, flFinal ) + 10;
		}
	}

	return nScore;
}

float CCSGameRules::GetRestartRoundTime( void ) const
{
	return m_fRoundStartTime;
}

CCSGameRules::CCSGameRules()
{
	// Reset borrow music convar because player indices scramble after level transition.
	cl_borrow_music_from_player_index.SetValue( cl_borrow_music_from_player_index.GetDefault() );

    InitializeGameTypeAndMode();

	// Set the bestof maps
	m_numBestOfMaps = 0;

	// Set global gifts state
	m_numGlobalGiftsGiven = 0;
	m_numGlobalGifters = 0;
	m_numGlobalGiftsPeriodSeconds = 0;
	for ( int j = 0; j < MAX_GIFT_GIVERS_FEATURED_COUNT; ++ j )
	{
		m_arrFeaturedGiftersAccounts.Set( j, 0 );
		m_arrFeaturedGiftersGifts.Set( j, 0 );
	}

	for ( int j = 0; j < MAX_TOURNAMENT_ACTIVE_CASTER_COUNT; ++ j )
	{
		m_arrTournamentActiveCasterAccounts.Set( j, 0 );
	}

	for ( int j = 0; j < MAX_MATCH_STATS_ROUNDS; ++j )
	{
		m_iMatchStats_PlayersAlive_T.GetForModify( j ) = 0x3F;
		m_iMatchStats_PlayersAlive_CT.GetForModify( j ) = 0x3F;
	}

	ResetCasterConvars();

	m_szTournamentEventName.GetForModify()[0] = 0;
	m_szTournamentEventStage.GetForModify()[0] = 0;
	m_szTournamentPredictionsTxt.GetForModify()[0] = 0;
	m_szMatchStatTxt.GetForModify()[ 0 ] = 0;
	m_nTournamentPredictionsPct = 0;

	HOOK_MESSAGE( SendPlayerItemDrops );
	HOOK_MESSAGE( SendPlayerItemFound );
	m_bMarkClientStopRecordAtRoundEnd = false;

}

CCSGameRules::~CCSGameRules()
{
	ResetCasterConvars();

	for ( int j = 0; j < MAX_TOURNAMENT_ACTIVE_CASTER_COUNT; ++ j )
	{
		m_arrTournamentActiveCasterAccounts.Set( j, 0 );
	}
}

// CLIENT
bool __MsgFunc_SendPlayerItemDrops( const ks::net::CCSUsrMsg_SendPlayerItemDrops &msg )
{
	//IViewPortPanel* panel = GetViewPortInterface()->FindPanelByName( PANEL_SCOREBOARD );
	if ( CSGameRules() )
	{
		CSGameRules()->ClearItemsDroppedDuringMatch();
		for ( int i = 0; i < msg.entity_updates.size(); i ++ )
		{
			const ks::net::CEconItemPreviewDataBlock &update = msg.entity_updates[ i ];
			CSGameRules()->RecordPlayerItemDrop( update );
		}	
	}

	return true;
}

void CCSGameRules::MarkClientStopRecordAtRoundEnd( bool bStop )
{
	m_bMarkClientStopRecordAtRoundEnd = bStop;
}

void CCSGameRules::OpenBuyMenu( int nPlayerID )
{
	// skip buy menu during demo playback
	if ( engine->IsPlayingDemo() || engine->IsPlayingTimeDemo() )
		return;

	GetViewPortInterface()->ShowPanel( PANEL_BUY, true );
	IGameEvent *pEvent = gameeventmanager->CreateEvent( "buymenu_open" );
	if ( pEvent )
	{
		pEvent->SetInt("userid", nPlayerID );
		gameeventmanager->FireEventClientSide( pEvent );
	}
}

void CCSGameRules::CloseBuyMenu( int nPlayerID )
{
	// skip buy menu during demo playback
	if ( engine->IsPlayingDemo() || engine->IsPlayingTimeDemo() )
		return;

	GetViewPortInterface()->ShowPanel( PANEL_BUY, false );
	IGameEvent *pEvent = gameeventmanager->CreateEvent( "buymenu_close" );
	if ( pEvent )
	{
		pEvent->SetInt("userid", nPlayerID );
		gameeventmanager->FireEventClientSide( pEvent );
	}

	// update the inventory
	CCSPlayer *pPlayer = static_cast<CCSPlayer*>( UTIL_PlayerByIndex(engine->GetPlayerForUserID( nPlayerID )) );
	CWeaponCSBase *pWeapon = (pPlayer && pPlayer->IsLocalPlayer()) ? pPlayer->GetActiveCSWeapon() : nullptr ;
	if ( pWeapon )
	{
		CBaseHudWeaponSelection *pHudSelection = GetHudWeaponSelection();
		if ( pHudSelection )
		{
			pHudSelection->OnWeaponSwitch( pWeapon );
		}
	}
}

const wchar_t* CCSGameRules::GetFriendlyMapName( const char* szShortName )
{
    // Look up the nice version of the name.
    char szToken[MAX_MAP_NAME+10+1]; // includes prefix size
    szToken[0] = '\0';
    V_snprintf( szToken, std::size( szToken ), "#SFUI_Map_%s", szShortName );
    wchar_t* szTranslated = g_pLocalize->Find( szToken );
    if ( szTranslated )
        return szTranslated;

    static wchar_t wszMapName[128];
    g_pLocalize->ConvertANSIToUnicode(szShortName, wszMapName, sizeof(wszMapName));
    return wszMapName;
}

bool CCSGameRules::GetFriendlyMapNameToken( const char* szShortName, char* szOutBuffer, int nBuffSize )
{
    // Create the nice version of the name.
    CreateFriendlyMapNameToken( szShortName, szOutBuffer, nBuffSize );

    if ( g_pLocalize->Find( szOutBuffer ) )
        return true;

    return false;
}

char const * CCSGameRules::GetTournamentEventName() const
{
	const char *sz = m_szTournamentEventName.Get();
	if ( sz && *sz && ( sz[0] != '#' ) )
	{
		// Localize it:
		static char const * const arrEvents[] = { ""
,"The 2013 DreamHack SteelSeries CS:GO Championship"
,"The Valve DPR Championship"
,"The 2014 EMS One Katowice CS:GO Championship"
,"ESL One Cologne 2014 CS:GO Championship"
,"The 2014 DreamHack CS:GO Championship"
,"ESL One Katowice 2015 CS:GO Championship"
,"ESL One Cologne 2015 CS:GO Championship"
,"DreamHack Cluj-Napoca 2015 CS:GO Championship"
,"MLG Columbus 2016 CS:GO Championship"
,"ESL One Cologne 2016 CS:GO Championship"
,"ELEAGUE Atlanta 2017 CS:GO Championship"
		};
		for ( int k = std::size( arrEvents ) - 1; k > 0; -- k )
		{
			if ( !V_strcmp( sz, arrEvents[k] ) )
			{
				V_snprintf( const_cast< char * >( sz ), MAX_PATH, "#CSGO_Tournament_Event_Name_%d", k );
				return sz;
			}
		}
	}
	return m_szTournamentEventName;
}

char const * CCSGameRules::GetTournamentEventStage() const
{
	const char *sz = m_szTournamentEventStage.Get();
	if ( sz && *sz && ( sz[ 0 ] != '#' ) )
	{
		// Localize it:
		static char const * const arrEvents[] = { ""
,"Exhibition"
,"Group Stage | First Stage"
,"BYOC"
,"Valve Pre-Event Test"
,"Match 1 of 3 | Quarterfinal"
,"Match 2 of 3 | Quarterfinal"
,"Match 3 of 3 | Quarterfinal"
,"Match 1 of 3 | Semifinal"
,"Match 2 of 3 | Semifinal"
,"Match 3 of 3 | Semifinal"
,"Match 1 of 3 | Grand Final"
,"Match 2 of 3 | Grand Final"
,"Match 3 of 3 | Grand Final"
,"All-Star"
,"Group Stage | Winners Match"
,"Group Stage | Elimination Match"
,"Group Stage | Decider Match"
,"Qualification"
,"Match 2 of 3 | Qualification"
,"Match 3 of 3 | Qualification"
,"Group Stage | Decider Match 1 of 3"
,"Group Stage | Decider Match 2 of 3"
,"Group Stage | Decider Match 3 of 3"
,"Group Stage | Second Stage (Upper Pool)"
,"Group Stage | Second Stage (Lower Pool)"
,"Group Stage | Third Stage"
		};
		for ( int k = std::size( arrEvents ) - 1; k > 0; --k )
		{
			if ( !V_strcmp( sz, arrEvents[ k ] ) )
			{
				V_snprintf( const_cast< char * >( sz ), MAX_PATH, "#CSGO_Tournament_Event_Stage_Display_%d", k );
				return sz;
			}
		}
	}
	return m_szTournamentEventStage;
}

//
// all of this is here to pick the first caster in the active caster list by default
//  later when we have a "pick caster" dialog then this can go away
//
extern ConVar spec_autodirector_cameraman;
extern ConVar spec_cameraman_ui;
extern ConVar spec_cameraman_xray;

void CCSGameRules::RecvProxy_TournamentActiveCasterAccounts( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	RecvProxy_Int32ToInt32( pData, pStruct, pOut );

	// set convars here
	if ( spec_autodirector_cameraman.GetInt() == -1 && CSGameRules() && CSGameRules()->m_arrTournamentActiveCasterAccounts[ 0 ] != 0 )
	{
		int casterID = ( int ) ( CSGameRules()->m_arrTournamentActiveCasterAccounts[ 0 ] );
		spec_autodirector_cameraman.SetValue( casterID );
		spec_cameraman_ui.SetValue( casterID );
		spec_cameraman_xray.SetValue( casterID );
		engine->SetVoiceCasterID( CSGameRules()->m_arrTournamentActiveCasterAccounts[ 0 ] );
	}
}

void CCSGameRules::ResetCasterConvars()
{
		spec_autodirector_cameraman.SetValue( -1 );
		spec_cameraman_ui.SetValue( 0 );
		spec_cameraman_xray.SetValue( 0 );
		engine->SetVoiceCasterID( 0 );
}

CEconQuestDefinition* CCSGameRules::GetActiveAssassinationQuest( void ) const
{
	if ( IsPlayingCooperativeGametype() )
		return nullptr;

	// Skip the map lookup for invlaid quest id
	if ( !m_iActiveAssassinationTargetMissionID )
		return nullptr;
	else
		return GetItemSchema()->GetQuestDefinition( m_iActiveAssassinationTargetMissionID );
}

float CCSGameRules::GetCMMItemDropRevealDuration()
{
	if ( m_flCMMItemDropRevealEndTime == 0 && std::ssize( m_ItemsPtrDroppedDuringMatch ) <= 0 )
		return 2;

	if ( m_flCMMItemDropRevealEndTime == 0 && m_flCMMItemDropRevealStartTime != 0 )
	{
		float flItemDropTime = 2.0f;
		for ( int i = 0; i < std::ssize( m_ItemsPtrDroppedDuringMatch ); i++ )
		{
			switch ( m_ItemsPtrDroppedDuringMatch[i]->rarity )
			{
			case 6:
			case 5:
				{
					flItemDropTime += sv_endmatch_item_drop_interval_ancient.GetFloat();
					break;
				}
			case 4:
				{
					flItemDropTime += sv_endmatch_item_drop_interval_legendary.GetFloat();
					break;
				}						
			case 3:
				{
					flItemDropTime += sv_endmatch_item_drop_interval_mythical.GetFloat();
					break;
				}	
			case 2:
				{
					flItemDropTime += sv_endmatch_item_drop_interval_rare.GetFloat();
					break;
				}
			default:
				{
					flItemDropTime += sv_endmatch_item_drop_interval.GetFloat();
					break;
				}
			}
		}

		m_flCMMItemDropRevealEndTime = (m_flCMMItemDropRevealStartTime + flItemDropTime);
	}

	return std::max<std::common_type_t<int, float>>( 2, (m_flCMMItemDropRevealEndTime - m_flCMMItemDropRevealStartTime) );
}

void CCSGameRules::CreateFriendlyMapNameToken( const char* szShortName, char* szOutBuffer, int nBuffSize )
{
    // must be able to fit the token below
    contract_assert( nBuffSize >= MAX_MAP_NAME+10+1 );

    // Create the nice version of the name.
    // right now we blindly create the token because we can assume (on consoles) that we don't have any maps
    // that we don't know about.  For PC, we'll need to fix this so all checks will happen on the client
    V_snprintf( szOutBuffer, nBuffSize, "#SFUI_Map_%s", szShortName );
}

const char *CCSGameRules::GetDefaultTeamName( int nTeam )
{
	contract_assert( nTeam >= 0 && nTeam < std::size( sTeamNames ) );

	return sTeamNames[nTeam];
}

void CCSGameRules::AddGunGameWeapon( const char* pWeaponName, int nNumKillsToUpgrade, int nTeamID )
{
    if ( !pWeaponName )
    {
        return;
    }

    contract_assert( nNumKillsToUpgrade > 0 );
    if ( nNumKillsToUpgrade <= 0 )
    {
        Warning( "CCSGameRules: Invalid number of kills-to-upgrade (%d) for weapon %s.\n", nNumKillsToUpgrade, pWeaponName );
        nNumKillsToUpgrade = 1;
    }

    char weaponName[MAX_WEAPON_STRING];
    weaponName[0] = '\0';
    V_snprintf( weaponName, sizeof( weaponName ), "weapon_%s", pWeaponName );

	int nWeaponID = 0;

	// Try to get the ID from the item name
	const CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( weaponName );
	if ( pDef )
	{
		nWeaponID = pDef->GetDefinitionIndex();
	}

	if ( nWeaponID == 0 )
	{
		// Fall back to the older weapon id system for things like knifegg
		nWeaponID = WeaponIdFromString( weaponName );
	}

    if ( nWeaponID != WEAPON_NONE )
    {
        if ( nTeamID == TEAM_CT )
        {
            m_GGProgressiveWeaponOrderCT.Set( m_iNumGunGameProgressiveWeaponsCT, nWeaponID );
            m_GGProgressiveWeaponKillUpgradeOrderCT.Set( m_iNumGunGameProgressiveWeaponsCT, nNumKillsToUpgrade );
            m_iNumGunGameProgressiveWeaponsCT++;
        }
        else if ( nTeamID == TEAM_TERRORIST )
        {
            m_GGProgressiveWeaponOrderT.Set( m_iNumGunGameProgressiveWeaponsT, nWeaponID );
            m_GGProgressiveWeaponKillUpgradeOrderT.Set( m_iNumGunGameProgressiveWeaponsT, nNumKillsToUpgrade );
            m_iNumGunGameProgressiveWeaponsT++;
        }

    }
    else
    {
        Warning( "CCSGameRules::AddGunGameWeapon: encountered an unknown weapon \"%s\".\n", pWeaponName );
    }
}

//-----------------------------------------------------------------------------
// Purpose: Set up the convars and data associated with the current game type and mode
//-----------------------------------------------------------------------------
void CCSGameRules::InitializeGameTypeAndMode( void )
{

    // this allows the weapon recoil tables to incorporate changes made to convars before the game launches
    g_WeaponDatabase.RefreshAllWeapons();
}

int CCSGameRules::GetNumProgressiveGunGameWeapons( int nTeamID ) const
{
    if ( nTeamID == TEAM_CT )
    {
        return m_iNumGunGameProgressiveWeaponsCT;
    }
    else
    {
        return m_iNumGunGameProgressiveWeaponsT;
    }
}

int CCSGameRules::GetGunGameTRBonusGrenade( CCSPlayer *pPlayer )
{
    if ( !pPlayer )
        return 0;

    int nNumTRKillPoints = pPlayer->GetNumGunGameTRKillPoints();

    if ( nNumTRKillPoints >= mp_ggtr_bomb_pts_for_molotov.GetInt() )
    {
        if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST )
            return WEAPON_MOLOTOV;
        else
            return WEAPON_INCGRENADE;
    }
    else if ( nNumTRKillPoints >= mp_ggtr_bomb_pts_for_flash.GetInt() )
    {
        return WEAPON_FLASHBANG;
    }
    else if ( nNumTRKillPoints >= mp_ggtr_bomb_pts_for_he.GetInt() )
    {
        return WEAPON_HEGRENADE;
    }

    return 0;
}

bool CCSGameRules::IsIntermission( void ) const
{

    return false;
}

int CCSGameRules::GetMaxSpectatorSlots( void ) const
{
    return m_iSpectatorSlotCount;
}

int CCSGameRules::GetMaxPlayers()
{
	if ( sv_competitive_official_5v5.GetInt() )
		return 10;

	if ( engine->IsPlayingDemo() || !engine->IsConnected() )
	{
		return 0;
	}

	return std::min( g_pGameTypes->GetCurrentServerNumSlots(), 24 );
}

void CCSGameRules::CoopResetRoundStartTime( void )
{
	m_fRoundStartTime.GetForModify() = gpGlobals->curtime;

}

void CCSGameRules::CoopGiveC4sToCTs( int nC4sToGive )
{
}

int CCSGameRules::GetNumWinsToClinch() const
{
	int iNumWinsToClinch = (mp_maxrounds.GetInt() > 0 && mp_match_can_clinch.GetBool()) ? ( mp_maxrounds.GetInt() / 2 ) + 1 + GetOvertimePlaying() * ( mp_overtime_maxrounds.GetInt() / 2 ) : -1;
	return iNumWinsToClinch;
}

bool CCSGameRules::IsLastRoundOfMatch() const
{
	bool bLastRound = mp_maxrounds.GetInt() > 0 ? ( GetTotalRoundsPlayed() == ( mp_maxrounds.GetInt()-1 + GetOvertimePlaying()*mp_overtime_maxrounds.GetInt() ) ) : false;
	return bLastRound;
}

bool CCSGameRules::IsMatchPoint() const
{
	int iNumWinsToClinch = GetNumWinsToClinch();
	bool bMatchPoint = false;
	if ( GetGamePhase() != GAMEPHASE_PLAYING_FIRST_HALF )
	{
		CTeam *pTerrorists = GetGlobalTeam( TEAM_TERRORIST );
		CTeam *pCTs = GetGlobalTeam( TEAM_CT );
		bMatchPoint = ( pCTs && ( pCTs->Get_Score() == iNumWinsToClinch-1 ) ) || ( pTerrorists && ( pTerrorists->Get_Score() == iNumWinsToClinch-1 ) );
	}
	return bMatchPoint;
}

// AreTeamsPlayingSwitchedSides() -- will return true when match is in second half, or in the half of overtime period where teams are switched.
// Overtime logic is as follows: TeamA plays CTs as first half of regulation, then Ts as second half of regulation,
//				then if tied in regulation continues to play Ts as first half of 1st overtime, then switches to CTs for second half of 1st overtime,
//				then if still tied after 1st OT they continue to play CTs as first half of 2nd overtime, then switch to Ts for second half of 2nd overtime,
//				then if still tied after 2nd OT they continue to play Ts as first half of 3rd overtime, then switch to CTs for second half of 3rd overtime,
//				and so on until the match determines a winner.
// So AreTeamsPlayingSwitchedSides will return true when TeamA is playing T-side and will return false when TeamA plays CT-side as they started match on CT
// in scenario outlined above.
bool CCSGameRules::AreTeamsPlayingSwitchedSides() const
{
	if ( !GetOvertimePlaying() )
	{
		switch ( GetGamePhase() )
		{
		case GAMEPHASE_PLAYING_SECOND_HALF:
			return true;
		case GAMEPHASE_MATCH_ENDED:
			return HasHalfTime() && ( GetTotalRoundsPlayed() > ( mp_maxrounds.GetInt() / 2 ) );
		default:
			return false;
		}
	}
	else
	{
		switch ( GetGamePhase() )
		{
		case GAMEPHASE_PLAYING_SECOND_HALF:
			// Playing 2nd half of 2nd half of every even OT, e.g. second OT, will result in switched teams
			return ( GetOvertimePlaying() % 2 ) ? false : true;
		case GAMEPHASE_MATCH_ENDED:
			{
				bool bEndedInSecondHalfOfOvertime = HasHalfTime() &&
					( GetTotalRoundsPlayed() > mp_maxrounds.GetInt() + ( 2*GetOvertimePlaying() - 1 ) * ( mp_overtime_maxrounds.GetInt() / 2 ) );
				if ( GetOvertimePlaying() % 2 )
					bEndedInSecondHalfOfOvertime = !bEndedInSecondHalfOfOvertime;
				return bEndedInSecondHalfOfOvertime;
			}
		case GAMEPHASE_HALFTIME:
			{
				// halftime can also be at the end of regulation or at the end of both OT halves, in this case the overtime number has
				// already been incremented into the next overtime
				bool bSecondHalfOfOvertime = HasHalfTime() &&
					( GetTotalRoundsPlayed() <= ( mp_maxrounds.GetInt() + ( GetOvertimePlaying() - 1 )*mp_overtime_maxrounds.GetInt() ) );
				int nOvertimeInWhichHalftimeIsActuallyReached = GetOvertimePlaying();
				if ( bSecondHalfOfOvertime )
					-- nOvertimeInWhichHalftimeIsActuallyReached;	// this is the case when we already advanced the OT index and wait in intermission
				if ( nOvertimeInWhichHalftimeIsActuallyReached % 2 )
					bSecondHalfOfOvertime = !bSecondHalfOfOvertime;
				return bSecondHalfOfOvertime;
			}
			break;
		default:
			// Playing 1st half, opposite of GAMEPHASE_PLAYING_SECOND_HALF state coded above
			return ( GetOvertimePlaying() % 2 ) ? true : false;
		}
	}
}

// music selection

const char *musicTypeStrings[] =
{
	"NONE",
	"Music.StartRound_GG",
	"Music.StartRound",
	"Music.StartAction",
	"Music.DeathCam",
	"Music.BombPlanted",
	"Music.BombTenSecCount",
	"Music.TenSecCount",
	"Music.WonRound",
	"Music.LostRound",
	"Music.GotHostage",
	"Music.MVPAnthem",
	"Music.Selection",
	"Musix.HalfTime",
};

void PlayMusicSelection( IRecipientFilter& filter, CsMusicType_t nMusicType , int nPlayerEntIndex /* = 0*/ , float flPreElapsedTime  /*= 0.0*/  )
{
	//////////////////////////////////////////////////////////////////////////////////////////
	// test for between rounds and block incoming events until in round
	//
	static bool bBetweenRound = false;
	if( nMusicType == CSMUSIC_LOSTROUND || nMusicType == CSMUSIC_WONROUND || nMusicType == CSMUSIC_MVP || nMusicType == CSMUSIC_HALFTIME )
	{
		bBetweenRound = true;
	}
	else if( nMusicType == CSMUSIC_START || nMusicType == CSMUSIC_ACTION || nMusicType == CSMUSIC_STARTGG )
	{
		bBetweenRound = false;
	}
	if( bBetweenRound && ( nMusicType == CSMUSIC_BOMB || nMusicType == CSMUSIC_BOMBTEN || nMusicType == CSMUSIC_ROUNDTEN ))
	{
		return;
	}

	const char *pEntry = musicTypeStrings[ nMusicType ];

	////////////////////////////////////////////////////////////////////////////////////////////
	// this is to be used in the case that no music pack is equipped, mimicing original csgo behavior
	// music selection switches on team select screen and halftime
	static int nUpdatedMusic = snd_music_selection.GetInt();
	// hack cause halftime music is getting called twice for some reason
	if( nMusicType == CSMUSIC_SELECTION || nMusicType == CSMUSIC_HALFTIME )
	{
		nUpdatedMusic = (nUpdatedMusic == 2 ) ? 1 : 2;		
	}
	if(  nMusicType == CSMUSIC_SELECTION || nMusicType == CSMUSIC_START )
	{
		snd_music_selection.SetValue( nUpdatedMusic );
	}
	char pMusicExtensionBuf[64];
	V_sprintf_safe( pMusicExtensionBuf, "%s_%02i", "valve_csgo", snd_music_selection.GetInt() );
	const char *pMusicExtension = pMusicExtensionBuf;

	/////////////////////////////////////////////////////////////////////////
	// no halftime music in overwatch
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if(pParameters->m_bAnonymousPlayerIdentity && nMusicType == CSMUSIC_HALFTIME )
			return; // or whatever is appropriate to not play the halftime music
	}

	/////////////////////////////////////////////////////////////////////////
	// is there delay from switching spectated player?
	// ********* not currently used ******
// 	engine->SOSSetOpvarFloat( "spectateMusicDelay", flPreElapsedTime );
// 	if( nMusicType == CSMUSIC_START )
// 	{
// 		engine->SOSSetOpvarFloat("csgo_roundstart_time", gpGlobals->curtime );
// 	}

	///////////////////////////////////////////////////////////////////////////////
	// music source priority:
	//	passed in arg (if has music pack)> borrowed music > own music

	int nPlayerIndex = 0;

	// use a specified playerindex if passed in ( MVP )
	if( ( nPlayerEntIndex != 0 ) && g_PR )
	{
		if (  g_PR->IsConnected( nPlayerEntIndex ) )
		{
			if ( CCS_PlayerResource *cs_PR = dynamic_cast< CCS_PlayerResource * >( g_PR ) )
			{
				uint32 unMusicID = cs_PR->GetMusicID( nPlayerEntIndex );
				if( unMusicID > 1 )
					nPlayerIndex = nPlayerEntIndex;
			}
		}
	}

	// if the passed in player didn't give us a music id, check for a borrowed kit.
	if ( !nPlayerIndex )
	{
		int nBorrowPlIndex = cl_borrow_music_from_player_index.GetInt();

		if ( nBorrowPlIndex )
		{	
			if ( g_PR->IsConnected( nBorrowPlIndex ) && !g_PR->IsFakePlayer( nBorrowPlIndex ) )
			{
				nPlayerIndex = nBorrowPlIndex;
			}
			else
			{
				cl_borrow_music_from_player_index.SetValue( cl_borrow_music_from_player_index.GetDefault() );
			}
		}
	}

	// otherwise use our own
	if ( !nPlayerIndex )
		nPlayerIndex = GetLocalPlayerIndex();
			
	{
		uint32 unMusicID = 0;

		if ( CCS_PlayerResource *cs_PR = dynamic_cast< CCS_PlayerResource * >( g_PR ) )
		{
			unMusicID = cs_PR->GetMusicID( nPlayerIndex );
		}
		if( unMusicID > 1 )
		{
			const CEconMusicDefinition *pMusicDef = GetItemSchema()->GetMusicDefinition(unMusicID);
			if(pMusicDef)
				pMusicExtension = pMusicDef->GetName();
		}

	}
	
	if( pEntry )
	{
		char musicSelection[128];
		int nExtLen = V_strlen( pMusicExtension );
		int nStrLen = V_strlen( pEntry );
		V_snprintf( musicSelection, nExtLen + nStrLen+2, "%s.%s", pEntry, pMusicExtension );
		CBaseEntity::EmitSound( filter, -1, musicSelection );
	}
}
void PlayCustomDeathCamSelection( IRecipientFilter& filter, int nEntIndex, const char *pSoundEntry, int nDeathCamIndex )
{
    char musicSelection[128];
    V_snprintf( musicSelection, 128, "%s_%03i", pSoundEntry, nDeathCamIndex );
    CBaseEntity::EmitSound( filter, nEntIndex, musicSelection );

}

float CCSGameRules::CheckTotalSmokedLength( float flSmokeRadiusSq, Vector vecGrenadePos, Vector from, Vector to )
{
	Vector sightDir = to - from;
	float sightLength = sightDir.NormalizeInPlace();

	// the detonation position is the actual position of the smoke grenade, but the smoke volume center is actually some number of units above that
	Vector vecSmokeCenterOffset = Vector( 0, 0, 60 );
	const Vector &smokeOrigin = vecGrenadePos + vecSmokeCenterOffset;

	float flSmokeRadius = std::sqrt(flSmokeRadiusSq);
	// if the start point or the end point is inside the radius of the smoke, then the line goes through the smoke
	if ( (smokeOrigin - from).IsLengthLessThan( flSmokeRadius*0.95f ) || (smokeOrigin - to).IsLengthLessThan( flSmokeRadius ) )
		return -1;

	Vector toGrenade = smokeOrigin - from;

	float alongDist = DotProduct( toGrenade, sightDir );

	// compute closest point to grenade along line of sight ray
	Vector close;

	// constrain closest point to line segment
	if (alongDist < 0.0f)
		close = from;
	else if (alongDist >= sightLength)
		close = to;
	else
		close = from + sightDir * alongDist;

	// if closest point is within smoke radius, the line overlaps the smoke cloud
	Vector toClose = close - smokeOrigin;
	float lengthSq = toClose.LengthSqr();

	//float smokeRadius = (float)sqrt( flSmokeRadiusSq );
	//NDebugOverlay::Sphere( smokeOrigin, smokeRadius, 0, 255, 0, true, 2.0f);
	if (lengthSq < flSmokeRadiusSq)
	{
		// some portion of the ray intersects the cloud
			
		// 'from' and 'to' lie outside of the cloud - the line of sight completely crosses it
		// determine the length of the chord that crosses the cloud
		float smokedLength = 2.0f * (float)sqrt( flSmokeRadiusSq - lengthSq );
		return smokedLength;
	}
	
	return 0;
}
