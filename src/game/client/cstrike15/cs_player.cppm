// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include <string.h>
#include <stdio.h>
#include "engine/ivmodelinfo.h"
#include "declare_class.h"
#include "iachievementmgr.h"
#include "igameevents.h"
#include "client_class.h"
#include "networkvar.h"
#include "reflect_annotations.h"
#include "../predictable_entity.h"
#include "inputsystem/ButtonCode.h"
#include "usermessages_schema.h"
#include "view_shared.h"
#include "matchmaking/cstrike15/imatchext_cstrike15.h"
#include "matchmaking/imatchtitle.h"
#include "matchmaking/iplayer.h"
#include "irecipientfilter.h"
#include "cs_gamerules.h"
#include "const.h"
#include "csgo_limits.h"
#include "bone_setup.h"
#include "edict.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "vphysics_interface.h"
#include "studio.h"
#include "csgo_playeranimstate.h"
#include "materialsystem/imaterial.h"
module ks.client:cstrike15.cs_player;

import std;
import ks.core.basetypes;
import ks.core.platform;
import ks.core.wchartypes;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import ks.tier1.strtools;
import :cstrike15.cs_shareddefs;
import ks.game.shareddefs;
import ks.core.types;
import ks.tier1.keyvalues;
import :particlemgr;
import ks.math.compressed_vector;
import ks.math.vector;
import :baseanimatedtextureproxy;
import ks.core.threadtools;
import ks.game.postprocess_shared;
import :sharedvar;
import :util;
import :baseentity;
import ks.tier1.convar;
import :cstrike15.bot.bot;
import ks.game.iplayeranimstate;
import :iinput;
import ks.game.cstrike15.cs_player_rank_shared;
import ks.math.mathlib;
import ks.tier1.interpolatedvar;
import ks.tier1.tracked_ref;
import ks.tier1.vector_ops;
import ks.game.ai_activity;
import ks.core.color;
import ks.tier1.playerid;
import ks.math.vector2d;

using namespace ::ks::reflect;

// #include "tier1/utlvector.h"

inline constexpr auto CS_NUM_LEVELS = 18;

//=============================================================================
// Helper class for simple manipulation of bit arrays.
// Used for server->client packets containing delta stats
//=============================================================================

template <int BitLength>
class BitArray
{
	enum { ByteLength = (BitLength + 7) / 8 };
public:
	BitArray() { ClearAll(); }

	void SetBit(int n) { m_bytes[n / 8] |= 1 << (n & 7); }
	void ClearBit(int n) { m_bytes[n / 8] &= (~(1 << (n & 7))); }
	bool IsBitSet(int n) const { return (m_bytes[n / 8] & (1 << (n & 7))) != 0;}

	void ClearAll() { V_memset(m_bytes, 0, sizeof(m_bytes)); }
	int NumBits() { return BitLength; }
	int NumBytes() { return ByteLength; }

	byte* RawPointer() { return m_bytes; }

private:
	byte m_bytes[ByteLength];
};

//=============================================================================
//
// CS Game Stats Enums
//
// approprate location.

enum CSStatType_t
{
	CSSTAT_UNDEFINED = -1,
	CSSTAT_SHOTS_HIT,
	CSSTAT_SHOTS_FIRED,
	CSSTAT_KILLS,
	CSSTAT_DEATHS,
	CSSTAT_DAMAGE,
	CSSTAT_NUM_BOMBS_PLANTED,
	CSSTAT_NUM_BOMBS_DEFUSED,
	CSSTAT_TR_NUM_BOMBS_PLANTED,
	CSSTAT_TR_NUM_BOMBS_DEFUSED,
	CSSTAT_PLAYTIME,
	CSSTAT_ROUNDS_WON,
	CSSTAT_T_ROUNDS_WON,
	CSSTAT_CT_ROUNDS_WON,
	CSSTAT_ROUNDS_PLAYED,
	CSSTAT_PISTOLROUNDS_WON,
	CSTAT_GUNGAME_ROUNDS_WON,
	CSTAT_GUNGAME_ROUNDS_PLAYED,
	CSSTAT_MONEY_EARNED,
	CSSTAT_OBJECTIVES_COMPLETED,
	CSSTAT_BOMBS_DEFUSED_WITHKIT,

	CSSTAT_KILLS_DEAGLE,
	CSSTAT_KILLS_USP,
	CSSTAT_KILLS_GLOCK,
	CSSTAT_KILLS_P228,
	CSSTAT_KILLS_ELITE,
	CSSTAT_KILLS_FIVESEVEN,
	CSSTAT_KILLS_AWP,
	CSSTAT_KILLS_AK47,
	CSSTAT_KILLS_M4A1,
	CSSTAT_KILLS_AUG,
	CSSTAT_KILLS_SG552,
	CSSTAT_KILLS_SG550,
	CSSTAT_KILLS_GALIL,
	CSSTAT_KILLS_GALILAR,
	CSSTAT_KILLS_FAMAS,
	CSSTAT_KILLS_SCOUT,
	CSSTAT_KILLS_G3SG1,
	CSSTAT_KILLS_P90,
	CSSTAT_KILLS_MP5NAVY,
	CSSTAT_KILLS_TMP,
	CSSTAT_KILLS_MAC10,
	CSSTAT_KILLS_UMP45,
	CSSTAT_KILLS_M3,
	CSSTAT_KILLS_XM1014,
	CSSTAT_KILLS_M249,
	CSSTAT_KILLS_KNIFE,
	CSSTAT_KILLS_HEGRENADE,
	CSSTAT_KILLS_MOLOTOV,
	CSSTAT_KILLS_DECOY,
	CSSTAT_KILLS_BIZON,
	CSSTAT_KILLS_MAG7,
	CSSTAT_KILLS_NEGEV,
	CSSTAT_KILLS_SAWEDOFF,
	CSSTAT_KILLS_TEC9,
	CSSTAT_KILLS_TASER,
	CSSTAT_KILLS_HKP2000,
	CSSTAT_KILLS_MP7,
	CSSTAT_KILLS_MP9,
	CSSTAT_KILLS_NOVA,
	CSSTAT_KILLS_P250,
	CSSTAT_KILLS_SCAR17,
	CSSTAT_KILLS_SCAR20,
	CSSTAT_KILLS_SG556,
	CSSTAT_KILLS_SSG08,

	CSSTAT_SHOTS_DEAGLE,
	CSSTAT_SHOTS_USP,
	CSSTAT_SHOTS_GLOCK,
	CSSTAT_SHOTS_P228,
	CSSTAT_SHOTS_ELITE,
	CSSTAT_SHOTS_FIVESEVEN,
	CSSTAT_SHOTS_AWP,
	CSSTAT_SHOTS_AK47,
	CSSTAT_SHOTS_M4A1,
	CSSTAT_SHOTS_AUG,
	CSSTAT_SHOTS_SG552,
	CSSTAT_SHOTS_SG550,
	CSSTAT_SHOTS_GALIL,
	CSSTAT_SHOTS_GALILAR,
	CSSTAT_SHOTS_FAMAS,
	CSSTAT_SHOTS_SCOUT,
	CSSTAT_SHOTS_G3SG1,
	CSSTAT_SHOTS_P90,
	CSSTAT_SHOTS_MP5NAVY,
	CSSTAT_SHOTS_TMP,
	CSSTAT_SHOTS_MAC10,
	CSSTAT_SHOTS_UMP45,
	CSSTAT_SHOTS_M3,
	CSSTAT_SHOTS_XM1014,
	CSSTAT_SHOTS_M249,
	CSSTAT_SHOTS_BIZON,
	CSSTAT_SHOTS_MAG7,
	CSSTAT_SHOTS_NEGEV,
	CSSTAT_SHOTS_SAWEDOFF,
	CSSTAT_SHOTS_TEC9,
	CSSTAT_SHOTS_TASER,
	CSSTAT_SHOTS_HKP2000,
	CSSTAT_SHOTS_MP7,
	CSSTAT_SHOTS_MP9,
	CSSTAT_SHOTS_NOVA,
	CSSTAT_SHOTS_P250,
	CSSTAT_SHOTS_SCAR17,
	CSSTAT_SHOTS_SCAR20,
	CSSTAT_SHOTS_SG556,
	CSSTAT_SHOTS_SSG08,

	CSSTAT_HITS_DEAGLE,
	CSSTAT_HITS_USP,
	CSSTAT_HITS_GLOCK,
	CSSTAT_HITS_P228,
	CSSTAT_HITS_ELITE,
	CSSTAT_HITS_FIVESEVEN,
	CSSTAT_HITS_AWP,
	CSSTAT_HITS_AK47,
	CSSTAT_HITS_M4A1,
	CSSTAT_HITS_AUG,
	CSSTAT_HITS_SG552,
	CSSTAT_HITS_SG550,
	CSSTAT_HITS_GALIL,
	CSSTAT_HITS_GALILAR,
	CSSTAT_HITS_FAMAS,
	CSSTAT_HITS_SCOUT,
	CSSTAT_HITS_G3SG1,
	CSSTAT_HITS_P90,
	CSSTAT_HITS_MP5NAVY,
	CSSTAT_HITS_TMP,
	CSSTAT_HITS_MAC10,
	CSSTAT_HITS_UMP45,
	CSSTAT_HITS_M3,
	CSSTAT_HITS_XM1014,
	CSSTAT_HITS_M249,
	CSSTAT_HITS_BIZON,
	CSSTAT_HITS_MAG7,
	CSSTAT_HITS_NEGEV,
	CSSTAT_HITS_SAWEDOFF,
	CSSTAT_HITS_TEC9,
	CSSTAT_HITS_TASER,
	CSSTAT_HITS_HKP2000,
	CSSTAT_HITS_MP7,
	CSSTAT_HITS_MP9,
	CSSTAT_HITS_NOVA,
	CSSTAT_HITS_P250,
	CSSTAT_HITS_SCAR17,
	CSSTAT_HITS_SCAR20,
	CSSTAT_HITS_SG556,
	CSSTAT_HITS_SSG08,

	CSSTAT_DAMAGE_DEAGLE,
	CSSTAT_DAMAGE_USP,
	CSSTAT_DAMAGE_GLOCK,
	CSSTAT_DAMAGE_P228,
	CSSTAT_DAMAGE_ELITE,
	CSSTAT_DAMAGE_FIVESEVEN,
	CSSTAT_DAMAGE_AWP,
	CSSTAT_DAMAGE_AK47,
	CSSTAT_DAMAGE_M4A1,
	CSSTAT_DAMAGE_AUG,
	CSSTAT_DAMAGE_SG552,
	CSSTAT_DAMAGE_SG550,
	CSSTAT_DAMAGE_GALIL,
	CSSTAT_DAMAGE_GALILAR,
	CSSTAT_DAMAGE_FAMAS,
	CSSTAT_DAMAGE_SCOUT,
	CSSTAT_DAMAGE_G3SG1,
	CSSTAT_DAMAGE_P90,
	CSSTAT_DAMAGE_MP5NAVY,
	CSSTAT_DAMAGE_TMP,
	CSSTAT_DAMAGE_MAC10,
	CSSTAT_DAMAGE_UMP45,
	CSSTAT_DAMAGE_M3,
	CSSTAT_DAMAGE_XM1014,
	CSSTAT_DAMAGE_M249,
	CSSTAT_DAMAGE_BIZON,
	CSSTAT_DAMAGE_MAG7,
	CSSTAT_DAMAGE_NEGEV,
	CSSTAT_DAMAGE_SAWEDOFF,
	CSSTAT_DAMAGE_TEC9,
	CSSTAT_DAMAGE_TASER,
	CSSTAT_DAMAGE_HKP2000,
	CSSTAT_DAMAGE_MP7,
	CSSTAT_DAMAGE_MP9,
	CSSTAT_DAMAGE_NOVA,
	CSSTAT_DAMAGE_P250,
	CSSTAT_DAMAGE_SCAR17,
	CSSTAT_DAMAGE_SCAR20,
	CSSTAT_DAMAGE_SG556,
	CSSTAT_DAMAGE_SSG08,

	CSSTAT_KILLS_HEADSHOT,
	CSSTAT_KILLS_ENEMY_BLINDED,
	CSSTAT_KILLS_WHILE_BLINDED,
	CSSTAT_KILLS_WITH_LAST_ROUND,
	CSSTAT_KILLS_ENEMY_WEAPON,
	CSSTAT_KILLS_KNIFE_FIGHT,
	CSSTAT_KILLS_WHILE_DEFENDING_BOMB,
	CSSTAT_KILLS_WITH_STATTRAK_WEAPON,

	CSSTAT_DECAL_SPRAYS,
	CSSTAT_TOTAL_JUMPS,
	CSSTAT_KILLS_WHILE_LAST_PLAYER_ALIVE,
	CSSTAT_KILLS_ENEMY_WOUNDED,
	CSSTAT_FALL_DAMAGE,

	CSSTAT_NUM_HOSTAGES_RESCUED,

	CSSTAT_NUM_BROKEN_WINDOWS,
	CSSTAT_PROPSBROKEN_ALL,
	CSSTAT_PROPSBROKEN_MELON,
	CSSTAT_PROPSBROKEN_OFFICEELECTRONICS,
	CSSTAT_PROPSBROKEN_OFFICERADIO,
	CSSTAT_PROPSBROKEN_OFFICEJUNK,
	CSSTAT_PROPSBROKEN_ITALY_MELON,

	CSSTAT_KILLS_AGAINST_ZOOMED_SNIPER,

	CSSTAT_WEAPONS_DONATED,

	CSSTAT_ITEMS_PURCHASED,
	CSSTAT_MONEY_SPENT,

	CSSTAT_DOMINATIONS,
	CSSTAT_DOMINATION_OVERKILLS,
	CSSTAT_REVENGES,

	CSSTAT_MVPS,
	CSSTAT_CONTRIBUTION_SCORE,
	CSSTAT_GG_PROGRESSIVE_CONTRIBUTION_SCORE,

	CSSTAT_GRENADE_DAMAGE,
	CSSTAT_GRENADE_POSTHUMOUSKILLS,
	CSSTAT_GRENADES_THROWN,

	CSTAT_ITEMS_DROPPED_VALUE,

	//Map win stats
	CSSTAT_MAP_WINS_CS_MILITIA,
	CSSTAT_MAP_WINS_CS_ASSAULT,
	CSSTAT_MAP_WINS_CS_ITALY,
	CSSTAT_MAP_WINS_CS_OFFICE,
	CSSTAT_MAP_WINS_DE_AZTEC,
	CSSTAT_MAP_WINS_DE_CBBLE,  
	CSSTAT_MAP_WINS_DE_DUST2,
	CSSTAT_MAP_WINS_DE_DUST,
	CSSTAT_MAP_WINS_DE_INFERNO,
	CSSTAT_MAP_WINS_DE_NUKE,
	CSSTAT_MAP_WINS_DE_PIRANESI,  
	CSSTAT_MAP_WINS_DE_PRODIGY,  
	CSSTAT_MAP_WINS_DE_LAKE,
	CSSTAT_MAP_WINS_DE_SAFEHOUSE,
	CSSTAT_MAP_WINS_DE_SHORTTRAIN,
	CSSTAT_MAP_WINS_DE_TRAIN,
	CSSTAT_MAP_WINS_DE_SUGARCANE,
	CSSTAT_MAP_WINS_DE_STMARC,
	CSSTAT_MAP_WINS_DE_BANK,
	CSSTAT_MAP_WINS_DE_EMBASSY,
	CSSTAT_MAP_WINS_DE_DEPOT,
	CSSTAT_MAP_WINS_DE_VERTIGO,
	CSSTAT_MAP_WINS_DE_BALKAN,
	CSSTAT_MAP_WINS_AR_MONASTERY,
	CSSTAT_MAP_WINS_AR_SHOOTS,
	CSSTAT_MAP_WINS_AR_BAGGAGE,

	CSSTAT_MAP_ROUNDS_CS_MILITIA,
	CSSTAT_MAP_ROUNDS_CS_ASSAULT,
	CSSTAT_MAP_ROUNDS_CS_ITALY,
	CSSTAT_MAP_ROUNDS_CS_OFFICE,
	CSSTAT_MAP_ROUNDS_DE_AZTEC,
	CSSTAT_MAP_ROUNDS_DE_CBBLE,
	CSSTAT_MAP_ROUNDS_DE_DUST2,
	CSSTAT_MAP_ROUNDS_DE_DUST,
	CSSTAT_MAP_ROUNDS_DE_INFERNO,
	CSSTAT_MAP_ROUNDS_DE_NUKE,
	CSSTAT_MAP_ROUNDS_DE_PIRANESI,
	CSSTAT_MAP_ROUNDS_DE_PRODIGY,
	CSSTAT_MAP_ROUNDS_DE_LAKE,
	CSSTAT_MAP_ROUNDS_DE_SAFEHOUSE,
	CSSTAT_MAP_ROUNDS_DE_SHORTTRAIN,
	CSSTAT_MAP_ROUNDS_DE_TRAIN,
	CSSTAT_MAP_ROUNDS_DE_SUGARCANE,
	CSSTAT_MAP_ROUNDS_DE_STMARC,
	CSSTAT_MAP_ROUNDS_DE_BANK,
	CSSTAT_MAP_ROUNDS_DE_EMBASSY,
	CSSTAT_MAP_ROUNDS_DE_DEPOT,
	CSSTAT_MAP_ROUNDS_DE_VERTIGO,
	CSSTAT_MAP_ROUNDS_DE_BALKAN,
	CSSTAT_MAP_ROUNDS_AR_MONASTERY,
	CSSTAT_MAP_ROUNDS_AR_SHOOTS,
	CSSTAT_MAP_ROUNDS_AR_BAGGAGE,

	CSSTAT_MAP_MATCHES_WON_SHOOTS,
	CSSTAT_MAP_MATCHES_WON_BAGGAGE,
	CSSTAT_MAP_MATCHES_WON_LAKE,
	CSSTAT_MAP_MATCHES_WON_SUGARCANE,
	CSSTAT_MAP_MATCHES_WON_STMARC,
	CSSTAT_MAP_MATCHES_WON_BANK,
	CSSTAT_MAP_MATCHES_WON_EMBASSY,
	CSSTAT_MAP_MATCHES_WON_DEPOT,
	CSSTAT_MAP_MATCHES_WON_SAFEHOUSE,
	CSSTAT_MAP_MATCHES_WON_SHORTTRAIN,
	CSSTAT_MAP_MATCHES_WON_TRAIN,

	CSSTAT_MATCHES_WON,
	CSSTAT_MATCHES_DRAW,
	CSSTAT_MATCHES_PLAYED,
	CSSTAT_GUN_GAME_MATCHES_WON,
	CSSTAT_GUN_GAME_MATCHES_PLAYED,
	CSSTAT_GUN_GAME_PROGRESSIVE_MATCHES_WON,
	CSSTAT_GUN_GAME_SELECT_MATCHES_WON,
	CSSTAT_GUN_GAME_TRBOMB_MATCHES_WON,

	CSSTAT_LASTMATCH_CONTRIBUTION_SCORE,
	CSSTAT_LASTMATCH_GG_PROGRESSIVE_CONTRIBUTION_SCORE,
	CSSTAT_LASTMATCH_T_ROUNDS_WON,
	CSSTAT_LASTMATCH_CT_ROUNDS_WON,
	CSSTAT_LASTMATCH_ROUNDS_WON,
	CSTAT_LASTMATCH_ROUNDS_PLAYED,
	CSSTAT_LASTMATCH_KILLS,
	CSSTAT_LASTMATCH_DEATHS,
	CSSTAT_LASTMATCH_MVPS,
	CSSTAT_LASTMATCH_DAMAGE,
	CSSTAT_LASTMATCH_MONEYSPENT,
	CSSTAT_LASTMATCH_DOMINATIONS,
	CSSTAT_LASTMATCH_REVENGES,
	CSSTAT_LASTMATCH_MAX_PLAYERS,
	CSSTAT_LASTMATCH_FAVWEAPON_ID,
	CSSTAT_LASTMATCH_FAVWEAPON_SHOTS,
	CSSTAT_LASTMATCH_FAVWEAPON_HITS,
	CSSTAT_LASTMATCH_FAVWEAPON_KILLS,

	CSSTAT_MAX	//Must be last entry.
};

inline constexpr auto CSSTAT_FIRST = (CSSTAT_UNDEFINED+1);
inline constexpr auto CSSTAT_LAST = (CSSTAT_MAX-1);

//
// CS Game Stats Flags
//
inline constexpr auto CSSTAT_PRIORITY_MASK = 0x000F;
inline constexpr auto CSSTAT_PRIORITY_NEVER = 0x0000; // not sent to client
inline constexpr auto CSSTAT_PRIORITY_ENDROUND = 0x0001; // sent at end of round
inline constexpr auto CSSTAT_PRIORITY_LOW = 0x0002; // sent every 2500ms
inline constexpr auto CSSTAT_PRIORITY_HIGH = 0x0003; // sent every 250ms

struct CSStatProperty
{
	const char*	szLocalizationToken;	// localization token for the stat
	uint flags;							// priority flags for sending to client
};

extern CSStatProperty CSStatProperty_Table[];

//=============================================================================
//
// CS Player Round Stats
//
struct StatsCollection_t
{
	StatsCollection_t() { Reset(); }

	void Reset()
	{
		for ( int i = 0; i < std::size( m_iValue ); i++ )
		{
			m_iValue[i] = 0;
		}
	}

	int operator[] ( int index ) const
	{
		contract_assert( index >= 0 && index < std::size( m_iValue ) );
		return m_iValue[index];
	}

	int& operator[] ( int index )
	{
		contract_assert( index >= 0 && index < std::size( m_iValue ) );
		return m_iValue[index];
	}

	void Aggregate( const StatsCollection_t& other );

private:
	int m_iValue[CSSTAT_MAX];
};

struct RoundStatsDirectAverage_t
{
	float m_fStat[CSSTAT_MAX];

	RoundStatsDirectAverage_t()
	{
		Reset();
	}

	void Reset()
	{
		for ( int i = 0; i < std::size( m_fStat ); i++ )
		{
			m_fStat[i] = 0;
		}
	}

	RoundStatsDirectAverage_t& operator +=( const StatsCollection_t &other )
	{
		for ( int i = 0; i < std::size( m_fStat ); i++ )
		{
			m_fStat[i] += other[i];
		}
		return *this;
	}

	RoundStatsDirectAverage_t& operator /=( const float &divisor)
	{
		if (divisor > 0)
		{
			for ( int i = 0; i < std::size( m_fStat ); i++ )
			{
				m_fStat[i] /= divisor;
			}
		}
		return *this;
	}

	RoundStatsDirectAverage_t& operator *=( const float &divisor)
	{
		for ( int i = 0; i < std::size( m_fStat ); i++ )
		{
			m_fStat[i] *= divisor;
		}
		return *this;
	}
};

struct RoundStatsRollingAverage_t
{
	float m_fStat[CSSTAT_MAX];
	int m_numberOfDataSets;

	RoundStatsRollingAverage_t()
	{
		Reset();
	}

	void Reset()
	{
		for ( int i = 0; i < std::size( m_fStat ); i++ )
		{
			m_fStat[i] = 0;
		}
		m_numberOfDataSets = 0;
	}

	RoundStatsRollingAverage_t& operator +=( const RoundStatsRollingAverage_t &other )
	{
		for ( int i = 0; i < std::size( m_fStat ); i++ )
		{
			m_fStat[i] += other.m_fStat[i];
		}
		return *this;
	}

    RoundStatsRollingAverage_t& operator +=( const StatsCollection_t &other )
	{
		for ( int i = 0; i < std::size( m_fStat ); i++ )
		{
            m_fStat[i] += other[i];
		}
		return *this;
	}

	RoundStatsRollingAverage_t& operator /=( const float &divisor)
	{
		if (divisor > 0)
		{
			for ( int i = 0; i < std::size( m_fStat ); i++ )
			{
				m_fStat[i] /= divisor;
			}
		}
		return *this;
	}

	void RollDataSetIntoAverage ( const RoundStatsRollingAverage_t &other );
};

enum CSGameStatsVersions_t
{
	CS_GAMESTATS_FILE_VERSION = 006,
	CS_GAMESTATS_MAGIC = 0xDEADBEEF
};

struct CS_Gamestats_Version_t
{
	int m_iMagic;			// always CS_GAMESTATS_MAGIC
	int m_iVersion;
};

struct KillStats_t
{
	KillStats_t() { Reset(); }

	void Reset()
	{
		V_memset( iNumKilled, 0, sizeof( iNumKilled ) );
		V_memset( iNumKilledBy, 0, sizeof( iNumKilledBy ) );
		V_memset( iNumKilledByUnanswered, 0, sizeof( iNumKilledByUnanswered ) );
	}

	int iNumKilled[MAX_PLAYERS+1];					// how many times this player has killed each other player
	int iNumKilledBy[MAX_PLAYERS+1];				// how many times this player has been killed by each other player
	int iNumKilledByUnanswered[MAX_PLAYERS+1];		// how many unanswered kills this player has been dealt by each other player
};

//=============================================================================
//
// CS Player Stats
//
struct PlayerStats_t
{
	PlayerStats_t()
	{
		Reset();
	}

	void Reset()
	{
		statsDelta.Reset();
		statsCurrentRound.Reset();
		statsCurrentMatch.Reset();
		statsKills.Reset();
	}

	PlayerStats_t( const PlayerStats_t &other );

	StatsCollection_t	statsDelta;
	StatsCollection_t	statsCurrentRound;
	StatsCollection_t	statsCurrentMatch;
	KillStats_t		statsKills;
};

struct WeaponName_StatId
{
	CSWeaponID   weaponId;
	CSStatType_t killStatId;
	CSStatType_t shotStatId;
	CSStatType_t hitStatId;
	CSStatType_t damageStatId;
};

struct MapName_MapStatId
{
	const char* szMapName;
	CSStatType_t statWinsId;
	CSStatType_t statRoundsId;
	CSStatType_t matchesWonId;
};

extern const MapName_MapStatId MapName_StatId_Table[];

//A mapping from weapon names to weapon stat IDs
extern const WeaponName_StatId WeaponName_StatId_Table[];

//Used to look up the appropriate entry by the ID of the actual weapon
const WeaponName_StatId& GetWeaponTableEntryFromWeaponId(CSWeaponID id);

class CAchievementMgr;
class CBaseAchievement;

using namespace ::ks::reflect;

#if !defined ( ACHIEVEMENTS_CS_H )
#define ACHIEVEMENTS_CS_H

bool CheckWinNoEnemyCaps( IGameEvent *event, int iRole );
bool IsLocalCSPlayerClass( int iClass );
bool GameRulesAllowsAchievements( void );

//----------------------------------------------------------------------------------------------------------------
// Base class for all CS achievements
class CCSBaseAchievement : public CBaseAchievement
{
	DECLARE_CLASS( CCSBaseAchievement, CBaseAchievement );
public:

	CCSBaseAchievement();

	virtual void GetSettings( KeyValues* pNodeOut );				// serialize
	virtual void ApplySettings( /* const */ KeyValues* pNodeIn );	// unserialize

	// [dwenger] Necessary for sorting achievements by award time
	virtual void OnAchieved();
	bool GetAwardTime( int& year, int& month, int& day, int& hour, int& minute, int& second );

	int64 GetSortKey() const { return GetUnlockTime(); }
};

//----------------------------------------------------------------------------------------------------------------
// Helper class for achievements that check that the player was playing on a game team for the full round
class CCSBaseAchievementFullRound;   // declared only: it was never implemented

//----------------------------------------------------------------------------------------------------------------
// Helper class for achievements based on other achievements
class CAchievement_Meta : public CCSBaseAchievement
{
	DECLARE_CLASS( CAchievement_Meta, CCSBaseAchievement );
public:
	virtual void Init();
	virtual void OnAchievementAwarded();

protected:
	void AddRequirement( int nAchievementId );

private:
	std::vector< int > m_requirements;
};

//Base class for all achievements to kill x players with a given weapon
class CAchievement_StatGoal: public CCSBaseAchievement
{
public:
	void SetStatId(CSStatType_t stat)
	{
		m_StatId = stat;
	}
	CSStatType_t GetStatId(void)
	{
		return m_StatId;
	}
private:
	virtual void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );		
	}	

	void OnPlayerStatsUpdate( int nUserSlot );
	CSStatType_t m_StatId;
};

extern CAchievementMgr g_AchievementMgrCS;	// global achievement mgr for CS

#endif // ACHIEVEMENTS_CS_H

#undef ACHIEVEMENTS_CS_H

using namespace ::ks::reflect;

class CBaseCSGrenadeProjectile : public CBaseGrenade
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseCSGrenadeProjectile" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CBaseCSGrenadeProjectile, CBaseGrenade );
	DECLARE_NETWORKCLASS(); 

	virtual void Spawn();
	//virtual bool	IsGrenadeProjectile( void ) { return true; };

public:

	// This gets sent to the client and placed in the client's interpolation history
	// so the projectile starts out moving right off the bat.
	CNetworkVector( m_vInitialVelocity, [[= ks::reflect::Net{ .bits = 20, .low = -3000, .high = 3000, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( int, m_nBounces, [[= ks::reflect::Net{ .bits = -1 } ]] );

	CBaseCSGrenadeProjectile() {}
	CBaseCSGrenadeProjectile( const CBaseCSGrenadeProjectile& ) {}
	virtual ~CBaseCSGrenadeProjectile();
	virtual int DrawModel( int flags, const RenderableInstance_t &instance );
	virtual void PostDataUpdate( DataUpdateType_t type );
	virtual void	ClientThink( void );

	void CreateGrenadeTrail( void );

	float m_flSpawnTime;
	Vector vecLastTrailLinePos;
	float flNextTrailLineTime;
};

class CCSPlayer;
class CColorCorrection;
class CPostProcessController;
class IClientMode;

using namespace ::ks::reflect;


enum PostProcessEffect_t
{
	POST_EFFECT_DEFAULT = 0,
	POST_EFFECT_LOW_HEATH,			// 1st person	event
	POST_EFFECT_VERY_LOW_HEATH,
	POST_EFFECT_IN_BUY_MENU,		// 1st person	event
	POST_EFFECT_DEATH_CAM,				// 1st person	event
	POST_EFFECT_SPECTATING,					// 1st person	state
	POST_EFFECT_IN_FIRE,
	POST_EFFECT_ZOOMED_RIFLE,
	POST_EFFECT_ZOOMED_SNIPER,
	POST_EFFECT_ZOOMED_SNIPER_MOVING,
	POST_EFFECT_UNDER_WATER,
	POST_EFFECT_ROUND_END_VIA_BOMBING,
	POST_EFFECT_SPEC_CAMERA_LERPING,
	POST_EFFECT_MAP_CONTROLLED,
	POST_EFFECT_DEATH_CAM_BODYSHOT,				// 1st person	event
	POST_EFFECT_DEATH_CAM_HEADSHOT,				// 1st person	event
	NUM_POST_EFFECTS
};

enum RoundStatus_t
{
	ROUND_UNKNOWN = 0,
	ROUND_STARTED,
	ROUND_ENDED,
	ROUND_ENDED_VIA_BOMBING
};

struct PostProcessEffectParams_t
{
	float fLocalContrastStrength;
	float fLocalContrastEdgeStrength;
	float fVignetteStart;
	float fVignetteEnd;
	float fVignetteBlurStrength;
	float fFadeToBlackStrength;
};

enum SyncKeyBindingValueDirection_t
{
	KEYBINDING_WRITE_TO_TITLEDATA,
	KEYBINDING_READ_FROM_TITLEDATA
};

class SpecCameraPosition_t
{
public:
	Vector	vecPosition;
	Vector	vecAngles;

	float	flWeight;
};

class ClientModeCSNormal : public ClientModeShared
{
DECLARE_CLASS( ClientModeCSNormal, ClientModeShared );

private:

// IClientMode overrides.
public:

					ClientModeCSNormal();
					~ClientModeCSNormal();

	virtual void	OnEvent( KeyValues *pEvent );

	virtual void	Init();
	virtual void	InitViewport();
	// dgoodenough - fix GCC shortcoming
	// PS3_BUILDFIX
	virtual void	InitViewport( bool bOnlyBaseClass = false );
	virtual void	Update();

	virtual void	LevelShutdown( void );

	virtual int		KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );

	int				GetDeathMessageStartHeight( void );

	void	FireGameEvent( IGameEvent *event );
	virtual void	PostRenderVGui();
	virtual void	PreRender(CViewSetup *pSetup) override;

	virtual bool	ShouldDrawViewModel( void );

	virtual bool	CanRecordDemo( char *errorMsg, int length ) const;

	virtual wchar_t*	GetServerName( void ) { return m_pServerName; }
	virtual void		SetServerName( wchar_t *name );
	virtual wchar_t*	GetMapName( void ) { return m_pMapName; }
	virtual void		SetMapName( wchar_t *name );

	virtual void	SetBlurFade( float scale );
	virtual float	GetBlurFade( void ) { return 0; }
	virtual void	DoPostScreenSpaceEffects( const CViewSetup *pSetup );

	virtual void	UpdateColorCorrectionWeights( void );
	virtual void	OnColorCorrectionWeightsReset( void );
	virtual float	GetColorCorrectionScale( void ) const;

	void			SetupStaticCameras( void );
	bool			GetIdealCameraPosForPlayer( int playerindex );

	void			LoadPostProcessParamsFromFile( const char* pFileName = nullptr );
	void			UpdatePostProcessingEffects();
	void			PostProcessLerpTo( PostProcessEffect_t effectID, float fFadeDuration = 0.75f, const PostProcessParameters_t* pTargetParams = nullptr );
	void			PostProcessLerpTo( PostProcessEffect_t effectID, const CPostProcessController* pPPController );
	void			DoPostProcessParamLerp();
	void			LerpPostProcessParam(	float fAmount, PostProcessParameters_t& result, const PostProcessParameters_t& from,
		const PostProcessParameters_t& to ) const;
	PostProcessEffect_t PostProcessEffectFromName( const char* pName ) const;
	void			GetDefaultPostProcessingParams( CCSPlayer* pPlayer, PostProcessEffectParams_t* pParams );

	void			SyncCurrentKeyBindingsToDeviceTitleData( int iController, int eDevice, const SyncKeyBindingValueDirection_t eOp );

	void			CheckTitleDataStorageConnected( void );

	CUserMessageBinder m_UMCMsgKillCam;
	CUserMessageBinder m_UMCMsgMatchEndConditions;
	CUserMessageBinder m_UMCMsgDisconnectToLobby;
	CUserMessageBinder m_UMCMsgDisplayInventory;
	CUserMessageBinder m_UMCMsgWarmupHasEnded;
	CUserMessageBinder m_UMCMsgServerRankUpdate;
	CUserMessageBinder m_UMCMsgServerRankRevealAll;
	CUserMessageBinder m_UMCMsgScoreLeaderboardData;
	CUserMessageBinder m_UMCMsgGlowPropTurnOff;
	CUserMessageBinder m_UMCMsgXpUpdate;
	CUserMessageBinder m_UMCMsgQuestProgress;
	CUserMessageBinder m_UMCMsgPlayerDecalDigitalSignature;

private:
	//int				CameraSortFunction( SpecCameraPosition_t* entry1, SpecCameraPosition_t* entry2 );

	wchar_t			m_pServerName[256];
	wchar_t			m_pMapName[256];

	CHandle<CColorCorrection>	m_hCurrentColorCorrection;
	//	void	UpdateSpectatorMode( void );

	ClientCCHandle_t	m_CCKillCamReplay;	// handle to death cc effect
	float				m_CCKillCamReplayPercent;
	ClientCCHandle_t	m_CCDeathHandle;	// handle to death cc effect
	float				m_CCDeathPercent;
	ClientCCHandle_t	m_CCFreezePeriodHandle_CT;
	float				m_CCFreezePeriodPercent_CT;
	ClientCCHandle_t	m_CCFreezePeriodHandle_T;
	float				m_CCFreezePeriodPercent_T;
	ClientCCHandle_t	m_CCPlayerFlashedHandle;
	float				m_CCPlayerFlashedPercent;

	RoundStatus_t		m_iRoundStatus;

	float				m_fDelayedCTWinTime;

	static PostProcessParameters_t ms_postProcessParams[];
	static const char* ms_postProcessEffectNames[];
	PostProcessEffect_t m_activePostProcessEffect;
	PostProcessEffect_t m_lastPostProcessEffect;
	const CPostProcessController* m_pActivePostProcessController;
	CountdownTimer m_postProcessEffectCountdown;
	PostProcessParameters_t m_postProcessLerpStartParams;
	PostProcessParameters_t m_postProcessLerpEndParams;
	PostProcessParameters_t m_postProcessCurrentParams;

	std::vector< SpecCameraPosition_t >	m_SpecCameraPositions;
	int m_nRoundMVP;

public:
	struct CQuestUncommittedProgress_t
	{
		uint32 m_numNormalPoints;
		double m_dblNormalPointsProgressTime;
		uint32 m_numNormalPointsProgressBaseline;
		bool m_bIsEventQuest;
	};
	static std::map< uint32, CQuestUncommittedProgress_t > sm_mapQuestProgressUncommitted;
	static ks::net::ScoreLeaderboardData s_ScoreLeaderboardData;
	static uint32 s_numLevelTransitions;
};

extern IClientMode *GetClientModeNormal();
extern ClientModeCSNormal* GetClientModeCSNormal();

class CAmmoDef;

// Purpose: Holds defintion for game ammo types

struct CSAmmoCost
{
	int	buySize;
	int	cost;
};

//=============================================================================
//	>> CCSAmmoDef
//=============================================================================
class CCSAmmoDef : public CAmmoDef
{

public:

	void AddAmmoCost( char const* name, int cost, int buySize );

	CCSAmmoDef(void);
	~CCSAmmoDef( void );

	int GetBuySize( int nAmmoIndex ) const;
	int GetCost( int nAmmoIndex ) const;

private:
	CSAmmoCost	m_csAmmo[MAX_AMMO_TYPES];
};

// Get the global ammodef object. This is usually implemented in each mod's game rules file somewhere,
// so the mod can setup custom ammo types.
CCSAmmoDef* GetCSAmmoDef();

using namespace ::ks::reflect;

struct PlayerStatData_t
{
	wchar_t*    pStatDisplayName;			// Localized display name of the stat
	int         iStatId;					// CSStatType_t enum value of stat
	int32       iStatValue;                 // Value of the stat
};

enum CSSyncStatValueDirection_t
{
	CSSTAT_WRITE_STAT,
	CSSTAT_READ_STAT
};

class CCSClientGameStats : public CAutoGameSystem, public CGameEventListener
{
public:
	CCSClientGameStats();
	virtual void PostInit();
	virtual void LevelShutdownPreEntity();

	int GetStatCount();

	PlayerStatData_t GetStatById(int id, int nUserSlot );

	void		ResetAllStats( int nUserSlot );
	void		ResetMatchStats( void );
	void		UpdateLastMatchStats( void );

	// [jhail] Reset the round stats 
	void		ResetRoundStats( void );

	void		IncrementMatchmakingData( const StatsCollection_t &stats );
	void		UpdateMatchmakingData( void );
	void		ResetMatchmakingData( MatchmakingDataScope mmDataScope );

	const StatsCollection_t&	GetLifetimeStats( int nUserSlot );
	const StatsCollection_t&	GetMatchStats( int nUserSlot );

	// [jhail] Retrieve the per-round stats
	const StatsCollection_t&	GetRoundStats( int nUserSlot );

	bool		MsgFunc_PlayerStatsUpdate( const ks::net::CCSUsrMsg_PlayerStatsUpdate &msg );

	bool ValidateTitleBlockVersion( struct TitleDataFieldsDescription_t const *pFields, class IPlayerLocal *pPlayerLocal, CSSyncStatValueDirection_t eOp, int titleBlockNo );

	bool SyncCSStatsToTitleData( int iController, CSSyncStatValueDirection_t eOp );
	bool SyncCSLoadoutsToTitleData( int iController, CSSyncStatValueDirection_t eOp );
	bool SyncCSMatchmakingDataToTitleData( int iController, CSSyncStatValueDirection_t eOp );
	bool SyncCSRankingDataToTitleData( int iController, CSSyncStatValueDirection_t eOp );

	CUserMessageBinder m_UMCMsgPlayerStatsUpdate;
	CUserMessageBinder m_UMCMsgXRankGet;
	CUserMessageBinder m_UMCMsgXRankUpd;

protected:
public:
	void FireGameEvent( IGameEvent *event );
protected:

	void UpdateStats( const StatsCollection_t &stats, int nUserSlot );
	void CalculateMatchFavoriteWeapons();
	
private:
	StatsCollection_t				m_lifetimeStats[MAX_SPLITSCREEN_PLAYERS];
	StatsCollection_t				m_matchStats[MAX_SPLITSCREEN_PLAYERS];
	StatsCollection_t				m_roundStats[MAX_SPLITSCREEN_PLAYERS];

	int							m_matchMaxPlayerCount;
};

extern CCSClientGameStats g_CSClientGameStats;

// Purpose: Global object for storing gameplay hint strings for the UI to display.

#if !defined CS_GAMEPLAY_HINTS_H_ 
#define CS_GAMEPLAY_HINTS_H_ 

struct CSGameplayHint_t;
class CCSGameplayHints : public CAutoGameSystem
{
public:
	CCSGameplayHints();
	virtual ~CCSGameplayHints();

	virtual void PostInit();
	virtual void Shutdown();
	
	const char* GetRandomLeastPlayedHint( void );
	enum HintRequiredContextFlags
	{
		HINT_CONTEXT_ALWAYS_SHOW		= 0,
		HINT_CONTEXT_BOMB_MAP			= (1<<0), 
		HINT_CONTEXT_HOSTAGE_MAP		= (1<<1), 
		HINT_CONTEXT_GUNGAME			= (1<<2), 
		// HINT_CONTEXT_ELO_BRACKET_SHOWN	= (1<<3),  -- obsolete
	};

private:
	std::vector< std::unique_ptr< CSGameplayHint_t > > m_HintList;
	KeyValues *m_pHintKV;

	void Cleanup( void );
	uint32 GetCurrentContextFlags( void );
};

extern CCSGameplayHints g_CSGameplayHints;

#endif // CS_GAMEPLAY_HINTS_H_ 

#undef CS_GAMEPLAY_HINTS_H_

class CCSWeaponInfo;
class CEconQuestDefinition;
class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CCSPlayer;
class CEconItemView;
class CWeaponCSBase;

using namespace ::ks::reflect;


// Purpose: The TF Game rules object

class CNavArea;

//#define DEBUG_QUESTS_IN_CLIENT

	class CCSPlayer;

inline constexpr auto WINNER_NONE = 0;
inline constexpr auto WINNER_DRAW = 1;
inline constexpr auto WINNER_TER = TEAM_TERRORIST;
inline constexpr auto WINNER_CT = TEAM_CT;

inline constexpr auto CUSTOM_BOT_DIFFICULTY_NOBOTS = 0;
inline constexpr auto CUSTOM_BOT_DIFFICULTY_DUMB = 1;
inline constexpr auto CUSTOM_BOT_DIFFICULTY_EASY = 2;
inline constexpr auto CUSTOM_BOT_DIFFICULTY_MEDIUM = 3;
inline constexpr auto CUSTOM_BOT_DIFFICULTY_HARD = 4;
inline constexpr auto CUSTOM_BOT_DIFFICULTY_EXPERT = 5;

inline constexpr auto CUSTOM_BOT_MIN_DIFFICULTY_FOR_AWARDS_PROGRESS = CUSTOM_BOT_DIFFICULTY_EASY;

inline constexpr auto MAX_WEAPON_NAME_POPUP_RANGE = 128.0;

inline constexpr auto MAX_GIFT_GIVERS_FEATURED_COUNT = 4;

inline constexpr auto MAX_TOURNAMENT_ACTIVE_CASTER_COUNT = 4;

class CCSBot;
class CHostage;

struct quest_data_t;

extern ConVar mp_startmoney;
extern ConVar mp_maxmoney;
extern ConVar mp_afterroundmoney;
extern ConVar mp_playercashawards;
extern ConVar mp_teamcashawards;
extern ConVar mp_tkpunish;
extern ConVar mp_c4timer;
extern ConVar mp_buytime;
extern ConVar mp_freezetime;
extern ConVar mp_playerid;
extern ConVar mp_defuser_allocation;
extern ConVar mp_death_drop_gun;
extern ConVar mp_death_drop_grenade;
extern ConVar mp_death_drop_defuser;
extern ConVar mp_hostages_moveable;
extern ConVar ammo_grenade_limit_total;
extern ConVar sv_competitive_official_5v5;

namespace DefuserAllocation
{
	enum Type
	{
		None = 0,
		Random = 1,
		All = 2,
	};
};

namespace TeamCashAward
{
	enum Type
	{
		NONE = 0,
		TERRORIST_WIN_BOMB,
		ELIMINATION_HOSTAGE_MAP_T,
		ELIMINATION_HOSTAGE_MAP_CT,
		ELIMINATION_BOMB_MAP,
		WIN_BY_TIME_RUNNING_OUT_HOSTAGE,
		WIN_BY_TIME_RUNNING_OUT_BOMB,
		WIN_BY_DEFUSING_BOMB,
		WIN_BY_HOSTAGE_RESCUE,
		LOSER_BONUS,
		LOSER_BONUS_CONSECUTIVE_ROUNDS,
		RESCUED_HOSTAGE,
		HOSTAGE_ALIVE,
		PLANTED_BOMB_BUT_DEFUSED,
		HOSTAGE_INTERACTION,
		LOSER_ZERO,
		SURVIVE_GUARDIAN_WAVE,
		CUSTOM_AWARD,
	};
};

namespace RoundResult
{
	enum Reason
	{
		UNDEFINED = 0,
		CT_WIN_ELIMINATION,
		CT_WIN_RESCUE,
		CT_WIN_DEFUSE,
		CT_WIN_TIME,
		T_WIN_ELIMINATION,
		T_WIN_BOMB,
		T_WIN_TIME,

		COUNT,

		UNKNOWN
	};

};

namespace PlayerCashAward
{
	enum Type
	{
		NONE = 0,
		KILL_TEAMMATE,
		KILLED_ENEMY,
		BOMB_PLANTED,
		BOMB_DEFUSED,
		RESCUED_HOSTAGE,
		INTERACT_WITH_HOSTAGE,
		DAMAGE_HOSTAGE,
		KILL_HOSTAGE,
		RESPAWN,
		GET_KILLED,
	};
};

namespace AutobalanceStatus
{
	enum Type
	{
		NONE = 0,
		NEXT_ROUND,
		THIS_ROUND
	};
};

//--------------------------------------------------------------------------------------------------------------
struct GGWeaponAliasName
{
	CSWeaponID id;
	const char *aliasName;
};

inline constexpr auto GGLIST_PISTOLS_TOTAL = 9;
inline constexpr auto GGLIST_RIFLES_TOTAL = 7;
inline constexpr auto GGLIST_MGS_TOTAL = 2;
inline constexpr auto GGLIST_SGS_TOTAL = 4;
inline constexpr auto GGLIST_SMGS_TOTAL = 6;
inline constexpr auto GGLIST_SNIPERS_TOTAL = 3;

inline constexpr auto GGLIST_PISTOLS_START = 0;
inline constexpr auto GGLIST_PISTOLS_LAST = (GGLIST_PISTOLS_START+GGLIST_PISTOLS_TOTAL-1);
inline constexpr auto GGLIST_RIFLES_START = GGLIST_PISTOLS_LAST+1;
inline constexpr auto GGLIST_RIFLES_LAST = (GGLIST_RIFLES_START+GGLIST_RIFLES_TOTAL-1);
inline constexpr auto GGLIST_MGS_START = GGLIST_RIFLES_LAST+1;
inline constexpr auto GGLIST_MGS_LAST = (GGLIST_MGS_START+GGLIST_MGS_TOTAL-1);
inline constexpr auto GGLIST_SGS_START = GGLIST_MGS_LAST+1;
inline constexpr auto GGLIST_SGS_LAST = (GGLIST_SGS_START+GGLIST_SGS_TOTAL-1);
inline constexpr auto GGLIST_SMGS_START = GGLIST_SGS_LAST+1;
inline constexpr auto GGLIST_SMGS_LAST = (GGLIST_SMGS_START+GGLIST_SMGS_TOTAL-1);
inline constexpr auto GGLIST_SNIPERS_START = GGLIST_SMGS_LAST+1;
inline constexpr auto GGLIST_SNIPERS_LAST = (GGLIST_SNIPERS_START+GGLIST_SNIPERS_TOTAL-1);

//--------------------------------------------------------------------------------------------------------------
// NOTE: Array must be NULL-terminated
inline GGWeaponAliasName ggWeaponAliasNameList[] =
{
	//pistols
	{ WEAPON_DEAGLE, "deagle" },
	{ WEAPON_DEAGLE, "revolver" },
	{ WEAPON_ELITE, "elite" },
	{ WEAPON_FIVESEVEN, "fiveseven" },
	{ WEAPON_GLOCK, "glock" },
	{ WEAPON_TEC9, "tec9" },
	{ WEAPON_HKP2000, "hkp2000" },
	{ WEAPON_HKP2000, "usp_silencer" },
	{ WEAPON_P250, "p250" },

	//rifles
	{ WEAPON_AK47, "ak47" },
	{ WEAPON_AUG, "aug" },
	{ WEAPON_FAMAS, "famas" },
	{ WEAPON_GALILAR, "galilar" },
	{ WEAPON_M4A1, "m4a1" },
	{ WEAPON_M4A1, "m4a1_silencer" },
	{ WEAPON_SG556, "sg556" },

	//mgs
	{ WEAPON_M249, "m249" },
	{ WEAPON_NEGEV, "negev" },

	//shotguns
	{ WEAPON_XM1014, "xm1014" },
	{ WEAPON_MAG7, "mag7" },
	{ WEAPON_SAWEDOFF, "sawedoff" },
	{ WEAPON_NOVA, "nova" },

	//smgs
	{ WEAPON_MAC10, "mac10" },
	{ WEAPON_P90, "p90" },
	{ WEAPON_UMP45, "ump45" },
	{ WEAPON_BIZON, "bizon" },
	{ WEAPON_MP7, "mp7" },
	{ WEAPON_MP9, "mp9" },

	//snipers
	//	{ WEAPON_SSG08, "ssg08" },
	{ WEAPON_SCAR20, "scar20" },
	{ WEAPON_G3SG1, "g3sg1" },
	{ WEAPON_AWP, "awp" },

	{ WEAPON_NONE, "" }
};

namespace DT_CSGameRules { extern RecvTable g_RecvTable; }
void RecvProxy_CSGameRules( const RecvProp *pProp, void **pOut, void *pData,
    int objectID );

class CCSGameRulesProxy : public CGameRulesProxy
{
	[[= ks::reflect::NetTable{ .name = "DT_CSGameRulesProxy" } ]]
	[[= ks::reflect::SubTable<"cs_gamerules_data", &DT_CSGameRules::g_RecvTable, RecvProxy_CSGameRules, true>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CCSGameRulesProxy, CGameRulesProxy );
	DECLARE_NETWORKCLASS();
};

class CCSGameRules : public CTeamplayRules
{
	[[= ks::reflect::NetTable{ .name = "DT_CSGameRules", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CCSGameRules, CTeamplayRules );

	// Stuff that is shared between client and server.
	bool IsFreezePeriod();
	bool IsWarmupPeriod() const;
	float GetWarmupPeriodEndTime() const;	
	bool IsWarmupPeriodPaused();
	void SetWarmupPeriodStartTime( float fl )	{ m_fWarmupPeriodStart = fl; }
	float GetWarmupPeriodStartTime( void )	{ return m_fWarmupPeriodStart; }
	bool AllowTaunts( void );

	bool IsTimeOutActive() const { return ( IsTerroristTimeOutActive() || IsCTTimeOutActive() ); }
	bool IsTerroristTimeOutActive() const { return m_bTerroristTimeOutActive; }
	bool IsCTTimeOutActive() const { return m_bCTTimeOutActive; }

	void StartTerroristTimeOut( void );
	void StartCTTimeOut( void );
	void EndTerroristTimeOut( void );
	void EndCTTimeOut( void );

	float GetCTTimeOutRemaining() const { return m_flCTTimeOutRemaining; }
	float GetTerroristTimeOutRemaining() const { return m_flTerroristTimeOutRemaining; }

	int GetCTTimeOuts( ) const { return m_nCTTimeOuts; }
	int GetTerroristTimeOuts( ) const { return m_nTerroristTimeOuts; }

	virtual bool AllowThirdPersonCamera();
	bool IsGoodDownTime( void ); // this returns true when its a good time to do things like garbage collection
	bool IsLoadoutAllowed( void );
	void MarkClientStopRecordAtRoundEnd( bool bStop );

	bool IsConnectedUserInfoChangeAllowed( CBasePlayer *pPlayer );
				
	virtual bool ShouldCollide( int collisionGroup0, int collisionGroup1 );

	virtual float	GetRoundRestartTime( void ) { return m_flRestartRoundTime; }
	virtual bool	IsGameRestarting( void ) { return m_bGameRestart; }
	float GetMapRemainingTime();	// time till end of map, -1 if timelimit is disabled
	float GetMapElapsedTime();	// How much time has elapsed since the map started.
	float GetRoundRemainingTime() const;	// time till end of round
	float GetRoundStartTime();		// When this round started.
	float GetRoundElapsedTime();	// How much time has elapsed since the round started.
	float GetBuyTimeLength();
	int GetRoundLength() const { return m_iRoundTime; }
	int   SelectDefaultTeam( bool ignoreBots = false );
	int   GetHumanTeam();			// TEAM_UNASSIGNED if no restrictions

	void CalculateMaxGunGameProgressiveWeaponIndex( void );
	int GetMaxGunGameProgressiveWeaponIndex( void ) { return m_iMaxGunGameProgressiveWeaponIndex; }

	AcquireResult::Type IsWeaponAllowed( const CCSWeaponInfo *pWeaponInfo, int nTeamNumber ,CEconItemView *pItem = nullptr  );

	bool IsBombDefuseMap() const;
	bool IsHostageRescueMap() const;
	bool MapHasBuyZone() const;
	bool CanSpendMoneyInMap();
	bool IsIntermission() const;
	bool IsLogoMap() const;
	bool IsSpawnPointValid( CBaseEntity *pSpot, CBasePlayer *pPlayer );
	bool IsSpawnPointHiddenFromOtherPlayers( CBaseEntity *pSpot, CBasePlayer *pPlayer, int nHideFromTeam = 0 );

	bool IsBuyTimeElapsed();
	bool IsMatchWaitingForResume( void );
	void SetMatchWaitingForResume( bool pause )				{ m_bMatchWaitingForResume = pause; };
	
	virtual int	DefaultFOV();

	// Get the view vectors for this mod.
	virtual const CViewVectors* GetViewVectors() const;

	bool IsCSGOBirthday( void );

	int  GetStartMoney( void );
	int	 GetMaxMoney( void );
	int  GetBetweenRoundMoney( void );
	bool PlayerCashAwardsEnabled( void );
	bool TeamCashAwardsEnabled( void );

	void AddHostageRescueTime( void );

	bool IsPlayingCustomGametype( void ) const;
	bool IsPlayingGunGameProgressive( void ) const;
	bool IsPlayingGunGameDeathmatch( void ) const;
	bool IsPlayingGunGameTRBomb( void ) const;
	bool IsPlayingGunGame( void ) const;
	bool IsPlayingClassic( void ) const;
	bool IsPlayingOffline( void ) const;
	bool IsPlayingTraining( void ) const;
	bool IsPlayingCooperativeGametype( void ) const;

	bool IsPlayingClassicCasual( void ) const;
	bool IsPlayingAnyCompetitiveStrictRuleset( void ) const;
	bool IsPlayingCoopGuardian( void ) const;
	bool IsPlayingCoopMission( void ) const;

	bool IsQueuedMatchmaking( void ) const;
	bool IsValveDS( void ) const;
	bool IsQuestEligible( void ) const;
	bool ShouldRecordMatchStats( void ) const;

	virtual bool IgnorePlayerKillCommand( void ) const { return IsQueuedMatchmaking() && !IsWarmupPeriod(); }
	
	bool IsAwardsProgressAllowedForBotDifficulty() const; // returns false if the user is playing offline with trivial bots (no bots, harmless bots)

	bool IsTeammateSolid( void ) const;				// returns true if teammates are solid obstacles in the current game mode
	bool IsArmorFree( void ) const;

	bool HasHalfTime( void ) const;

	bool IsRoundOver() const;

	int GetCustomBotDifficulty( void ) const;

	int GetCurrentGunGameWeapon ( int nCurrentWeaponIndex, int nTeamID );
	int GetNextGunGameWeapon( int nCurrentWeaponIndex, int nTeamID );
	int GetPreviousGunGameWeapon( int nCurrentWeaponIndex, int nTeamID );
	bool IsFinalGunGameProgressiveWeapon( int nCurrentWeaponIndex, int nTeamID );
	int GetGunGameNumKillsRequiredForWeapon( int nCurrentWeaponIndex, int nTeamID );

	void AddGunGameWeapon( const char* pWeaponName, int nNumKillsToUpgrade, int nTeamID );
	int GetNumProgressiveGunGameWeapons( int nTeamID ) const;
	int GetProgressiveGunGameWeapon( int nWeaponIndex, int nTeamID ) const { return nTeamID == TEAM_CT ? m_GGProgressiveWeaponOrderCT[nWeaponIndex] : m_GGProgressiveWeaponOrderT[nWeaponIndex]; }
	int GetProgressiveGunGameWeaponKillRequirement( int nWeaponIndex, int nTeamID ) const { return nTeamID == TEAM_CT ? m_GGProgressiveWeaponKillUpgradeOrderCT[nWeaponIndex] : m_GGProgressiveWeaponKillUpgradeOrderT[nWeaponIndex]; }
	int GetGunGameTRBonusGrenade( CCSPlayer *pPlayer );

	void IncrementGunGameTerroristWeapons( void );
	void IncrementGunGameCTWeapons( void );

	bool IsBotOnlyTeam( int nTeamNumber );
	int TeamCashAwardValue( TeamCashAward::Type reason);
	int PlayerCashAwardValue( PlayerCashAward::Type reason);

	int GetMaxSpectatorSlots( void ) const;

	float GetTimeUntilNextPhaseStarts( void ) 	{ return m_timeUntilNextPhaseStarts; }
	GamePhase GetGamePhase( void ) const		{ return ( GamePhase ) m_gamePhase.Get(); }
	int GetTotalRoundsPlayed( void ) const		{ return m_totalRoundsPlayed; }
	int GetOvertimePlaying( void ) const		{ return m_nOvertimePlaying; }

	int GetNumWinsToClinch( void ) const;
	bool IsLastRoundOfMatch() const;
	bool IsMatchPoint() const;

	// AreTeamsPlayingSwitchedSides() -- will return true when match is in second half, or in the half of overtime period where teams are switched.
	// Overtime logic is as follows: TeamA plays CTs as first half of regulation, then Ts as second half of regulation,
	//				then if tied in regulation continues to play Ts as first half of 1st overtime, then switches to CTs for second half of 1st overtime,
	//				then if still tied after 1st OT they continue to play CTs as first half of 2nd overtime, then switch to Ts for second half of 2nd overtime,
	//				then if still tied after 2nd OT they continue to play Ts as first half of 3rd overtime, then switch to CTs for second half of 3rd overtime,
	//				and so on until the match determines a winner.
	// So AreTeamsPlayingSwitchedSides will return true when TeamA is playing T-side and will return false when TeamA plays CT-side as they started match on CT
	// in scenario outlined above.
	bool AreTeamsPlayingSwitchedSides() const;

	void SetIsWarmupPeriod( bool bIsWarmup )	{ m_bWarmupPeriod = bIsWarmup; }

	bool HasMatchStarted()	{ return m_bHasMatchStarted; }

	int GetWeaponScoreForDeathmatch( int nPos );

	float GetRestartRoundTime( void ) const;

	int GetGuardianRequiredKills( void ) const;
	int GetGuardianKillsRemaining( void ) const;
	int GetGuardianSpecialWeapon( void ) const;

private:
	float GetExplosionDamageAdjustment(Vector & vecSrc, Vector & vecEnd, CBaseEntity *pEntityToIgnore); // returns multiplier between 0.0 and 1.0 that is the percentage of any damage done from vecSrc to vecEnd that actually makes it.
	float GetAmountOfEntityVisible(Vector & src, CBaseEntity *player); // returns a value from 0 to 1 that is the percentage of player visible from src.

	CNetworkVar( bool, m_bFreezePeriod, [[= ks::reflect::Net{} ]] );	 // true at beginning of round, set to false when the period expires
	CNetworkVar( bool, m_bWarmupPeriod, [[= ks::reflect::Net{} ]] );	 // 
	CNetworkVar( float, m_fWarmupPeriodEnd, [[= ks::reflect::Net{ .bits = 32 } ]] ); // OBSOLETE. LEFT IN FOR DEMO COMPATIBILITY.
	CNetworkVar( float, m_fWarmupPeriodStart, [[= ks::reflect::Net{ .bits = 32 } ]] );

	CNetworkVar( bool, m_bTerroristTimeOutActive, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bCTTimeOutActive, [[= ks::reflect::Net{} ]] );
	CNetworkVar( float, m_flTerroristTimeOutRemaining, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkVar( float, m_flCTTimeOutRemaining, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkVar( int, m_nTerroristTimeOuts, [[= ks::reflect::Net{ .bits = -1 } ]] );
	CNetworkVar( int, m_nCTTimeOuts, [[= ks::reflect::Net{ .bits = -1 } ]] );

	CNetworkVar( bool, m_bMatchWaitingForResume, [[= ks::reflect::Net{} ]] ); // When mp_pause_match is called, this state becomes true and will prevent the next freezetime from ending.
	CNetworkVar( int, m_iRoundTime, [[= ks::reflect::Net{ .bits = 16 } ]] );		 // (From mp_roundtime) - How many seconds long this round is.	
	CNetworkVar( float, m_fMatchStartTime, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_NOSCALE } ]] ); // time when match has started
	CNetworkVar( float, m_fRoundStartTime, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_NOSCALE } ]] ); // time round has started		
	CNetworkVar( float, m_flRestartRoundTime, [[= ks::reflect::Net{ .bits = 32 } ]] ); // the global time when the round is supposed to end, if this is not 0
	CNetworkVar( bool, m_bGameRestart, [[= ks::reflect::Net{} ]] ); // True = mp_restartgame is being processed
	CNetworkVar( float, m_flGameStartTime, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_timeUntilNextPhaseStarts, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( int, m_gamePhase, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_totalRoundsPlayed, [[= ks::reflect::Net{ .bits = 16 } ]] );
	CNetworkVar( int, m_nOvertimePlaying, [[= ks::reflect::Net{ .bits = 16 } ]] );
	CNetworkVar( int, m_iHostagesRemaining, [[= ks::reflect::Net{ .bits = 4 } ]] );
	CNetworkVar( bool, m_bAnyHostageReached, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bMapHasBombTarget, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bMapHasRescueZone, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bMapHasBuyZone, [[= ks::reflect::Net{} ]] );	
	CNetworkVar( bool, m_bIsQueuedMatchmaking, [[= ks::reflect::Net{} ]] );	
	CNetworkVar( bool, m_bIsValveDS, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bLogoMap, [[= ks::reflect::Net{} ]] );		 // If there's an info_player_logo entity, then it's a logo map.
	CNetworkVar( int,  m_iNumGunGameProgressiveWeaponsCT, [[= ks::reflect::Net{ .bits = -1 } ]] );	// total number of CT gun game progressive weapons
	CNetworkVar( int,  m_iNumGunGameProgressiveWeaponsT, [[= ks::reflect::Net{ .bits = -1 } ]] );	// total number of T gun game progressive weapons
	CNetworkVar( int,  m_iSpectatorSlotCount, [[= ks::reflect::Net{ .bits = -1 } ]] );				// max spectator slots available				
	CNetworkArray( int, m_GGProgressiveWeaponOrderCT, 60, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED } ]] );				// CT gun game weapon order and # kills per weapon. Size is meant to be larger than the current number of different weapons defined in the CSWeaponID enum
	CNetworkArray( int, m_GGProgressiveWeaponOrderT, 60, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED } ]] );				// T gun game weapon order and # kills per weapon. Size is meant to be larger than the current number of different weapons defined in the CSWeaponID enum
	CNetworkArray( int, m_GGProgressiveWeaponKillUpgradeOrderCT, 60, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED } ]] );	// CT gun game number of kills per weapon. Size is meant to be larger than the current number of different weapons defined in the CSWeaponID enum
	CNetworkArray( int, m_GGProgressiveWeaponKillUpgradeOrderT, 60, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED } ]] );	// T gun game number of kills per weapon. Size is meant to be larger than the current number of different weapons defined in the CSWeaponID enum
	CNetworkVar( int, m_MatchDevice, [[= ks::reflect::Net{ .bits = -1 } ]] );
	CNetworkVar( bool, m_bHasMatchStarted, [[= ks::reflect::Net{} ]] );
	CNetworkVar( float, m_flDMBonusStartTime, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkVar( float, m_flDMBonusTimeLength, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkVar( uint16, m_unDMBonusWeaponLoadoutSlot, [[= ks::reflect::Net{ .bits = -1 } ]] );
	CNetworkVar( bool, m_bDMBonusActive, [[= ks::reflect::Net{} ]] );
	CNetworkVar( int, m_nNextMapInMapgroup, [[= ks::reflect::Net{ .bits = -1 } ]] );
	CNetworkString( m_szTournamentEventName, MAX_PATH, [[= ks::reflect::Net{} ]] );
	CNetworkString( m_szTournamentEventStage, MAX_PATH, [[= ks::reflect::Net{} ]] );
	CNetworkString( m_szMatchStatTxt, MAX_PATH, [[= ks::reflect::Net{} ]] );
	CNetworkString( m_szTournamentPredictionsTxt, MAX_PATH, [[= ks::reflect::Net{} ]] );
	CNetworkVar( int, m_nTournamentPredictionsPct, [[= ks::reflect::Net{ .bits = -1 } ]] );
	CNetworkVar( float, m_flCMMItemDropRevealStartTime, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flCMMItemDropRevealEndTime, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( bool, m_bIsDroppingItems, [[= ks::reflect::Net{} ]] );	 // 
	CNetworkVar( bool, m_bIsQuestEligible, [[= ks::reflect::Net{} ]] );

	CNetworkVar( int, m_nGuardianModeWaveNumber, [[= ks::reflect::Net{ .bits = -1 } ]] );
	CNetworkVar( int, m_nGuardianModeSpecialKillsRemaining, [[= ks::reflect::Net{ .bits = -1 } ]] );
	CNetworkVar( int, m_nGuardianModeSpecialWeaponNeeded, [[= ks::reflect::Net{ .bits = -1 } ]] );	
	int m_nGuardianGrenadesToGiveBots;
public:

	// HACK: Low on time, don't have a better place for this. Hang some global data guardian needs to bookkeep heavy spawns.
	int m_nNumHeaviesToSpawn;
	//
	// Holiday gifts global presence
	//
	CNetworkVar( uint32, m_numGlobalGiftsGiven, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( uint32, m_numGlobalGifters, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( uint32, m_numGlobalGiftsPeriodSeconds, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED } ]] );
	CNetworkArray( uint32, m_arrFeaturedGiftersAccounts, MAX_GIFT_GIVERS_FEATURED_COUNT, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED } ]] );
	CNetworkArray( uint32, m_arrFeaturedGiftersGifts, MAX_GIFT_GIVERS_FEATURED_COUNT, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED } ]] );

	CNetworkArray( uint16, m_arrProhibitedItemIndices, MAX_PROHIBITED_ITEMS, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED } ]] );

	//
	// Tournament Casters
	//
	CNetworkArray( uint32, m_arrTournamentActiveCasterAccounts, MAX_TOURNAMENT_ACTIVE_CASTER_COUNT, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED } ]] );

	// These three are part of a hack to move an expensive network call (ClientPrint) out of 
	// the death code and defer it by a short time. The level of network traffic is causing hitches on ps3.
// 	CCallQueue m_DeferredCallQueue;
// 	float m_flDeferredCallDispatchTime;

	// Tournament best-of-N
	CNetworkVar( int, m_numBestOfMaps, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] );

	// halloween mask seed
	CNetworkVar( int, m_nHalloweenMaskListSeed, [[= ks::reflect::Net{ .bits = -1 } ]] );

	CNetworkVar( bool,  m_bBombDropped, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool,	m_bBombPlanted, [[= ks::reflect::Net{} ]] );
	CNetworkVar( int, m_iRoundWinStatus, [[= ks::reflect::Net{ .bits = -1 } ]] );	// 1 == CT's won last round, 2 == Terrorists did, 3 == Draw, no winner	
	CNetworkVar( int, m_eRoundWinReason, [[= ks::reflect::Net{ .bits = -1 } ]] );	// see: e_RoundEndReason
	CNetworkVar( bool, m_bTCantBuy, [[= ks::reflect::Net{} ]] );			// Who can and can't buy.
	CNetworkVar( bool, m_bCTCantBuy, [[= ks::reflect::Net{} ]] );
	CNetworkVar( float, m_flGuardianBuyUntilTime, [[= ks::reflect::Net{ .bits = 32 } ]] );

	CNetworkArray( int, m_iMatchStats_RoundResults, MAX_MATCH_STATS_ROUNDS, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );	
	CNetworkArray( int, m_iMatchStats_PlayersAlive_CT, MAX_MATCH_STATS_ROUNDS, [[= ks::reflect::Net{ .bits = 6, .flags = SPROP_UNSIGNED } ]] );
	CNetworkArray( int, m_iMatchStats_PlayersAlive_T, MAX_MATCH_STATS_ROUNDS, [[= ks::reflect::Net{ .bits = 6, .flags = SPROP_UNSIGNED } ]] );

	CNetworkArray( float,		m_TeamRespawnWaveTimes, MAX_TEAMS, [[= ks::reflect::Net{ .bits = 32 } ]] );	// Time between each team's respawn wave

	CEconQuestDefinition* GetActiveAssassinationQuest( void ) const;
	int GetActiveServerQuestID( void ) const { return m_iActiveAssassinationTargetMissionID; }
protected:
	CNetworkArray( float,		m_flNextRespawnWave, MAX_TEAMS, [[= ks::reflect::Net{} ]] );		// Minor waste, but cleaner code
	CNetworkVar( int, m_iActiveAssassinationTargetMissionID, [[= ks::reflect::Net{ .bits = -1 } ]] );			// we cannot change the name of this field for networking compatibility, but in coopgametypes this means the server questid
	bool m_bDontIncrementCoopWave;

public:
	float GetCMMItemDropRevealDuration();
	float GetCMMItemDropRevealEndTime() { return m_flCMMItemDropRevealEndTime; }
	bool IsDroppingItems() { return m_bIsDroppingItems; }

	loadout_positions_t GetDMBonusWeaponLoadoutSlot( void ) { return ( loadout_positions_t )m_unDMBonusWeaponLoadoutSlot.Get(); }
	float GetDMBonusStartTime( void ) { return m_flDMBonusStartTime; }
	float GetDMBonusTimeLength( void ) { return m_flDMBonusTimeLength; }
	bool IsDMBonusActive( void ) { return m_bDMBonusActive; }
	void SetNextMapInMapGroup( int nIndex ) { m_nNextMapInMapgroup = nIndex; }
	int GetNextMapInMapGroup( void ) { return m_nNextMapInMapgroup; }

	int GetNumHostagesRemaining( void ) { return m_iHostagesRemaining; }

	virtual CBaseCombatWeapon *GetNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon );

	virtual const unsigned char *GetEncryptionKey( void ) { return (unsigned char *)"d7NSuLq2"; } // both the client and server need this key

	void CreateFriendlyMapNameToken( const char* szShortName, char* szOutBuffer, int nBuffSize  );

	// This is unlocalized and shouldn't be used for display
	const char *GetDefaultTeamName( int nTeam );

	void OpenBuyMenu( int nPlayerIndex );
	void CloseBuyMenu( int nPlayerIndex );

	// respawn
	void			SetNextTeamRespawnWaveDelay( int iTeam, float flDelay );
	virtual float	GetNextRespawnWave( int iTeam, CBasePlayer *pPlayer );
	float			GetMinTimeWhenPlayerMaySpawn( CBasePlayer *pPlayer );
	float			GetRespawnWaveMaxLength( int iTeam, bool bScaleWithNumPlayers = true );
	float			GetRespawnTimeScalar( int iTeam );

	//float			GetTimeUntilEndMatchNextMapVoteEnds( void );
	bool			IsEndMatchVotingForNextMap();
	bool			IsEndMatchVotingForNextMapEnabled();

	// End Match Voting
	CNetworkArray( int, m_nEndMatchMapGroupVoteOptions, MAX_ENDMATCH_VOTE_PANELS, [[= ks::reflect::Net{ .bits = -1 } ]] );				// For mapgroups >10 maps these will be vote options

	// these functions cover recording and sending item drops for display in game modes where you don't allow drops during the match/round
	std::vector< std::unique_ptr< ks::net::CEconItemPreviewDataBlock > > m_ItemsPtrDroppedDuringMatch;
	const std::vector< std::unique_ptr< ks::net::CEconItemPreviewDataBlock > >& GetItemsDroppedDuringMatch( void ) const
	{
		return m_ItemsPtrDroppedDuringMatch;
	}
	void ClearItemsDroppedDuringMatch( void );
	void RecordPlayerItemDrop( const ks::net::CEconItemPreviewDataBlock &iteminfo );

	static int GetMaxPlayers(); // always available

	// COOP
	void CoopResetRoundStartTime( void );
	void CoopGiveC4sToCTs( int nC4sToGive );

	DECLARE_CLIENTCLASS_NOBASE(); // This makes datatables able to access our private vars.
	CCSGameRules();
	~CCSGameRules();

	// helper function - this will eventually support side car info (and will probably move)
	const wchar_t* GetFriendlyMapName( const char* szShortName );
	bool GetFriendlyMapNameToken( const char* szShortName, char* szOutBuffer, int nBuffSize  );

	char const * GetTournamentEventName() const;
	char const * GetTournamentEventStage() const;
	char const * GetTournamentPredictionsTxt() const { return m_szTournamentPredictionsTxt; }
	int GetTournamentPredictionsPct() const { return m_nTournamentPredictionsPct; }
	char const * GetMatchStatTeamsTxt() const { return m_szMatchStatTxt; }

	CUserMessageBinder m_UMCMsgSendPlayerItemDrops;
	CUserMessageBinder m_UMCMsgSendPlayerItemFound;
	bool m_bMarkClientStopRecordAtRoundEnd;

	static void RecvProxy_TournamentActiveCasterAccounts( const CRecvProxyData *pData, void *pStruct, void *pOut );
	void ResetCasterConvars( void );

	// Methods & memeber variables on both client and server DLLs

public:

	virtual bool ForceSplitScreenPlayersOnToSameTeam( void );

	void UpdatePlayerEloBracket( CCSPlayer *pPlayer, int32 bracket );

	bool IsSwitchingTeamsAtRoundReset( void ) { return m_bSwitchingTeamsAtRoundReset; }

	float CheckTotalSmokedLength( float flRadius, Vector vecGrenadePos, Vector from, Vector to );

protected:
	loadout_positions_t PickRandomWeaponForDMBonus( void );

	void AssignStartingMoneyToAllPlayers( void );

	void InitializeGameTypeAndMode( void );

	bool m_bHasTriggeredRoundStartMusic;
	bool m_bHasTriggeredCoopSpawnReset;

public:
	BotProfileDevice_t GetMatchDevice( void ) const { return ( BotProfileDevice_t )( m_MatchDevice.Get() ); }

	void AddDroppedWeaponToList( CWeaponCSBase *pWeapon );
	void RemoveDroppedWeaponFromList( CWeaponCSBase *pWeapon );
	int GetTotalDroppedWeaponsInWorld( void ) { return std::ssize( m_weaponsDroppedInWorld ); }

protected:
	void ProcessAutoBalance( void );

private:
	void UnfreezeAllPlayers( void );

	bool m_bSwitchingTeamsAtRoundReset;
	int m_iMaxGunGameProgressiveWeaponIndex;

	std::vector< CHandle<CWeaponCSBase> > m_weaponsDroppedInWorld;
};

bool EconEntity_OnOwnerKillEaterEvent( CEconItemView *pEconItemView, CCSPlayer *pOwner, CCSPlayer *pVictim, kill_eater_event_t eEventType, int iAmount = 1, uint32 *pNewValue = nullptr );

//-----------------------------------------------------------------------------
// Gets us at the team fortress game rules
//-----------------------------------------------------------------------------

inline CCSGameRules* CSGameRules()
{
	return static_cast<CCSGameRules*>(g_pGameRules);
}

inline constexpr auto IGNORE_SPECTATORS = false;
inline constexpr auto IGNORE_UNASSIGNED = true;
int UTIL_HumansInGame( bool ignoreSpectators = false, bool ignoreUnassigned = false );
int UTIL_SpectatorsInGame( void );

//-----------------------------------------------------------------------------
// Music Selection
//-----------------------------------------------------------------------------

enum CsMusicType_t
{
	CSMUSIC_NONE = 0,
	CSMUSIC_STARTGG,
	CSMUSIC_START,
	CSMUSIC_ACTION,
	CSMUSIC_DEATHCAM,
	CSMUSIC_BOMB,
	CSMUSIC_BOMBTEN,
	CSMUSIC_ROUNDTEN,
	CSMUSIC_WONROUND,
	CSMUSIC_LOSTROUND,
	CSMUSIC_HOSTAGE,
	CSMUSIC_MVP,
	CSMUSIC_SELECTION,
	CSMUSIC_HALFTIME

};
inline constexpr auto CSMUSIC_NOPACK = 0;

void PlayMusicSelection( IRecipientFilter& filter, CsMusicType_t nMusicType , int nPlayerEntIndex  = 0 , float flPreElapsedTime  = 0.0 );

//-----------------------------------------------------------------------------
// Purpose: Useful utility functions
//-----------------------------------------------------------------------------

// Assassination quest helper funcs
bool IsAssassinationQuest( const CEconQuestDefinition *pQuest );
bool IsAssassinationQuest( uint32 questID );
bool IsAssassinationQuestActive( const CEconQuestDefinition *pQuest );

extern const float g_flWarmupToFreezetimeDelay;

class CBaseAnimatingOverlay;
class CWeaponCSBase;

	class CBaseAnimatingOverlay;
	class CWeaponCSBase;

// When moving this fast, he plays run anim.
inline constexpr auto ARBITRARY_RUN_SPEED = 175.0f;

inline constexpr auto HOSTAGE_JUMP_POWER = 200.0f;
inline constexpr const char *HOSTAGE_ANIM_MODEL = "models/hostage/hostage.mdl";

inline constexpr auto MOVESTATE_IDLE = 0;
inline constexpr auto MOVESTATE_WALK = 1;
inline constexpr auto MOVESTATE_RUN = 2;

// This abstracts the differences between CS players and hostages.
class ICSPlayerAnimStateHelpers
{
public:
	virtual CWeaponCSBase* CSAnim_GetActiveWeapon() = 0;
	virtual bool CSAnim_CanMove() = 0;
};

IPlayerAnimState* CreatePlayerAnimState( CBaseAnimatingOverlay *pEntity, ICSPlayerAnimStateHelpers *pHelpers, LegAnimType_t legAnimType, bool bUseAimSequences );
IPlayerAnimState* CreateHostageAnimState( CBaseAnimatingOverlay *pEntity, ICSPlayerAnimStateHelpers *pHelpers, LegAnimType_t legAnimType, bool bUseAimSequences );

// If this is set, then the game code needs to make sure to send player animation events
// to the local player if he's the one being watched.
extern ConVar cl_showanimstate;

using namespace ::ks::reflect;

// Purpose: CS's custom CPlayerResource

inline constexpr auto MAX_DECORATED_PLAYER_NAME_LENGTH = ( ( MAX_NETWORKID_LENGTH * 10 ) + 20 );

enum EDecoratedPlayerNameFlag_t
{
	k_EDecoratedPlayerNameFlag_Simple = 0,
	k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot = ( 1 << 0 ),
	k_EDecoratedPlayerNameFlag_DontUseNameOfControllingPlayer = ( 1 << 1 ),
	k_EDecoratedPlayerNameFlag_DontShowClanName = ( 1 << 2 ),
	k_EDecoratedPlayerNameFlag_DontMakeStringSafe = ( 1 << 3 ),
	k_EDecoratedPlayerNameFlag_DontUseAssassinationTargetName = ( 1 << 4 ),
};

class CCS_PlayerResource : public CPlayerResource
{
	[[= ks::reflect::NetTable{ .name = "DT_CSPlayerResource" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CCS_PlayerResource, CPlayerResource );
public:
	DECLARE_CLIENTCLASS();

					CCS_PlayerResource();
	virtual			~CCS_PlayerResource();

	virtual	void	OnDataChanged(DataUpdateType_t updateType);

	bool			IsVIP( int iIndex );
	bool			HasC4( int iIndex );
	bool			IsHostageAlive(int iIndex);
	bool			IsHostageFollowingSomeone(int iIndex);
	int				GetHostageEntityID(int iIndex);
	const Vector	GetBombsiteAPosition();
	const Vector	GetBombsiteBPosition();
	const Vector	GetHostageRescuePosition( int index );

	int				GetNumMVPs( int iIndex );
	bool			HasDefuser( int iIndex );
	bool			HasHelmet( int iIndex );
	int				GetArmor( int iIndex );	
	int				GetScore( int iIndex );	
	int				GetCompetitiveRanking( int iIndex );	
	int				GetCompetitiveWins( int iIndex );	
	int				GetCompTeammateColor( int iIndex );
	int				GetTotalCashSpent( int iIndex );
	int				GetCashSpentThisRound( int iIndex );
	const char		*GetClanTag( int index );
	int				GetActiveCoinRank( int iIndex );
	int				GetMusicID( int iIndex );

	int GetPersonaDataPublicLevel( int iIndex );
	int GetPersonaDataPublicCommendsLeader( int iIndex );
	int GetPersonaDataPublicCommendsTeacher( int iIndex );
	int GetPersonaDataPublicCommendsFriendly( int iIndex );

	int				GetEndMatchNextMapVote( int iIndex );
	bool			EndMatchNextMapAllVoted( void );

	const wchar_t*		GetDecoratedPlayerName( int name, wchar_t* buffer, int bufflen, EDecoratedPlayerNameFlag_t flags );

	virtual const char *GetPlayerName( int index ) override;
	bool			IsControllingBot( int index );
	int				GetControlledPlayer( int index );
	int				GetControlledByPlayer( int index );
	bool			IsAssassinationTarget( int index );

	int				GetBotDifficulty( int index );

protected:

	virtual void	UpdatePlayerName( int slot ) override;

	bool	m_bDisableAssassinationTargetNameOverride;

	[[= ks::reflect::Net{} ]] int		m_iPlayerC4;	// entity index of C4 carrier or 0
	[[= ks::reflect::Net{} ]] int		m_iPlayerVIP;	// entity index of VIP player or 0
	[[= ks::reflect::Net{} ]] Vector	m_bombsiteCenterA;	
	[[= ks::reflect::Net{} ]] Vector	m_bombsiteCenterB;	

	[[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int } ]] bool	m_bHostageAlive[MAX_HOSTAGES];
	[[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int } ]] bool	m_isHostageFollowingSomeone[MAX_HOSTAGES];
	[[= ks::reflect::Net{} ]] int		m_iHostageEntityIDs[MAX_HOSTAGES];

	[[= ks::reflect::Net{} ]] int		m_hostageRescueX[MAX_HOSTAGE_RESCUES];
	[[= ks::reflect::Net{} ]] int		m_hostageRescueY[MAX_HOSTAGE_RESCUES];
	[[= ks::reflect::Net{} ]] int		m_hostageRescueZ[MAX_HOSTAGE_RESCUES];

	[[= ks::reflect::Net{} ]] int		m_iMVPs[ MAX_PLAYERS + 1 ];
	[[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int } ]] bool	m_bHasDefuser[ MAX_PLAYERS + 1 ];
	[[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int } ]] bool	m_bHasHelmet[ MAX_PLAYERS + 1 ];
	[[= ks::reflect::Net{} ]] int		m_iArmor[ MAX_PLAYERS + 1 ];
	[[= ks::reflect::Net{} ]] int		m_iScore[ MAX_PLAYERS + 1 ];
	[[= ks::reflect::Net{} ]] int		m_iCompetitiveRanking[ MAX_PLAYERS + 1 ];
	[[= ks::reflect::Net{} ]] int		m_iCompetitiveWins[ MAX_PLAYERS + 1 ];
	[[= ks::reflect::Net{} ]] int		m_iCompTeammateColor[MAX_PLAYERS + 1];

	[[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int } ]] bool	m_bControllingBot[ MAX_PLAYERS + 1 ];
	[[= ks::reflect::Net{} ]] int		m_iControlledPlayer[ MAX_PLAYERS + 1 ];
	[[= ks::reflect::Net{} ]] int		m_iControlledByPlayer[ MAX_PLAYERS + 1 ];
	char	m_szPlayerNames[ MAX_PLAYERS + 1 ][ MAX_PLAYER_NAME_LENGTH ];

	[[= ks::reflect::Net{} ]] int		m_iBotDifficulty[ MAX_PLAYERS + 1 ];	// Difficulty level of a bot ( -1 if not applicable )
	[[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::String } ]] char	m_szClan[MAX_PLAYERS+1][MAX_CLAN_TAG_LENGTH];
	[[= ks::reflect::Net{} ]] int		m_iTotalCashSpent[ MAX_PLAYERS + 1 ];
	[[= ks::reflect::Net{} ]] int		m_iCashSpentThisRound[ MAX_PLAYERS + 1 ];
	[[= ks::reflect::Net{} ]] int		m_nEndMatchNextMapVotes[ MAX_PLAYERS + 1 ];
	[[= ks::reflect::Net{} ]] bool	m_bEndMatchNextMapAllVoted;

	[[= ks::reflect::Net{} ]] int		m_nActiveCoinRank[ MAX_PLAYERS + 1 ];
	[[= ks::reflect::Net{} ]] int		m_nMusicID[ MAX_PLAYERS + 1 ];
	bool	m_bIsAssassinationTarget[ MAX_PLAYERS + 1 ];

	[[= ks::reflect::Net{} ]] int m_nPersonaDataPublicLevel[ MAX_PLAYERS + 1 ];
	[[= ks::reflect::Net{} ]] int m_nPersonaDataPublicCommendsLeader[ MAX_PLAYERS + 1 ];
	[[= ks::reflect::Net{} ]] int m_nPersonaDataPublicCommendsTeacher[ MAX_PLAYERS + 1 ];
	[[= ks::reflect::Net{} ]] int m_nPersonaDataPublicCommendsFriendly[ MAX_PLAYERS + 1 ];

};

CCS_PlayerResource *GetCSResources( void );

class CCSPlayer;

class Beam_t;
class CCSGOPlayerAnimState;
class CConfigurationForHighPriorityUseEntity_t;
class CNewParticleEffect;
class CUserCmd;
class CAnimationLayer;
class CBaseAnimating;
class CBaseAnimatingOverlay;
class CBaseCombatWeapon;
class CBaseEntity;
class CBaseParticleEntity;
class CBasePlayer;
class CBaseWeaponWorldModel;
class CEconItemView;
class CWeaponCSBase;
class IRagdoll;

using namespace ::ks::reflect;


inline constexpr const char *SPEECH_VOIP_PARTICLE_EFFECT_NAME = "speech_voice";
inline constexpr const char *RADIO_VOICE_PARTICLE_EFFECT_NAME = "radio_voice";

enum CStrikeScreenSpaceEffect
{
	AR_LEADER_SCREEN_SPACE_EFFECT,

	CSTRIKE_SCREEN_SPACE_EFFECT_COUNT
};

//#define CHARACTER_SPAWN_CAM_ZOOM_IN
extern const char *musicTypeStrings[];


extern ConVar cl_disablefreezecam;

struct clientSmokeGrenadeRecord_t
{
	Vector m_vecPosition;
	float m_flInceptionTime;
	int m_iEntityId;
};

struct clientHitVerify_t
{

	clientHitVerify_t();

	clientHitVerify_t( Vector inPos, float flTimestamp, float flExpireTime );

	Vector	vecPosition;
	float	flTimestamp;
	float	flExpireTime;
};

class CBulletHitModel : public CBaseAnimating
{
	DECLARE_CLASS( CBulletHitModel, CBaseAnimating );

public:
	//CBulletHitModel() : m_pPlayerParent(NULL), m_iBoneIndex( 0 ), m_bIsHit( false ), m_flTimeCreated( 0 ){}
	void			AttachToPlayer( CCSPlayer *pTargetPlayer, int nBoneIndex, matrix3x4_t matLocalOffset, bool bIsHit, Vector vecStartPos );
	virtual int		DrawModel( int flags, const RenderableInstance_t &instance );
	bool			UpdatePosition( void );
	CCSPlayer *GetPlayerParent();
private:
	matrix3x4_t m_matLocal;
	int m_iBoneIndex;
	EHANDLE m_hPlayerParent;
	bool m_bIsHit;
	float m_flTimeCreated;
	Vector m_vecStartPos;
};

inline constexpr auto BONESNAPSHOT_ENTIRE_BODY = 0;
inline constexpr auto BONESNAPSHOT_UPPER_BODY = 1;

inline constexpr auto BONESNAPSHOT_DECAYDURATION_MIN = 0;
inline constexpr auto BONESNAPSHOT_DECAYDURATION_MAX = 0.4f;

class CBoneSnapshot
{
public:
	void			Update( CBaseAnimating* pEnt, bool bReadOnly = false );	
	inline void		UpdateReadOnly( void ) { Update( m_pEnt, true ); }

	void			SetShouldCapture( void )					{ m_bCapturePending = true; }

	void SetShouldCapture( float flNewDecayDuration );

	bool			IsCapturePending( void )	{ return (m_bEnabled && m_bCapturePending); }

	void			Enable( void )				{ if ( !m_bEnabled ) { AbandonAnyPending(); } m_bEnabled = true; }
	void			Disable( void )				{ if ( m_bEnabled ) { AbandonAnyPending(); } m_bEnabled = false; }

	float			GetCurrentWeight( void )	{ return m_flWeight; }
	
	void			SetLastBoneSetupTimeIndex( void ) { m_flLastBoneSetupTimeIndex = gpGlobals->curtime; }
	
	void Init( void )
	{
		m_pEnt = nullptr;
		m_flWeight = 0;
		memset( m_Weightlist, 0, std::size( m_Weightlist ) );
		m_bWeightlistInitialized = false;
		m_bEnabled = false;
		m_bCapturePending = false;
		m_vecSubordinateSnapshots.clear();
		m_flDecayIdealDuration = 0.25f;
		m_flDecayStartTime = 0;
		m_flDecayEndTime = 0;
		m_vecWorldCapturePos.Init();
		m_flLastBoneSetupTimeIndex = 0;
		m_nStudioRenderHdrId = -1;
	}

	void Init( CBaseAnimating* pEnt )			{ Init(); m_pEnt = pEnt; }
	CBoneSnapshot() { Init(); }

	void AddSubordinate( CBoneSnapshot* pSnapshot );

	void AbandonAnyPending( void );

	void SetWeightListName( const char* szWeightListName ) { m_szWeightlistName = szWeightListName; }

private:

	CBaseAnimating* m_pEnt;
	float			m_flWeight;
	matrix3x4a_t	m_Cache[ MAXSTUDIOBONES ];
	const char*		m_szWeightlistName;
	float			m_Weightlist[ MAXSTUDIOBONES ];
	bool			m_bWeightlistInitialized;
	bool			m_bEnabled;
	bool			m_bCapturePending;
	float			m_flDecayIdealDuration;
	float			m_flDecayStartTime;
	float			m_flDecayEndTime;
	Vector			m_vecWorldCapturePos;
	float			m_flLastBoneSetupTimeIndex;
	int				m_nStudioRenderHdrId;

	std::vector< CBoneSnapshot* > m_vecSubordinateSnapshots;

	void			CaptureSnapshot( void );
	void			PlaybackSnapshot( void );
	void			InitWeightList( void );
	bool			IsBoneSetupTimeIndexRecent( void ) { return (gpGlobals->curtime - m_flLastBoneSetupTimeIndex) <= 0.25f; }
};

//-----------------------------------------------------------------------------
class CAddonModel
{
public:
	CHandle<CBaseAnimating> m_hEnt;	// The model for the addon.
	int m_iAddon;						// One of the ADDON_ bits telling which model this is.
	int m_iAttachmentPoint;				// Which attachment point on the player model this guy is on.
};

// Class that handles the playback of a sound instance during round start
class RoundStartSoundPlaybackData
{
public:
	RoundStartSoundPlaybackData() { m_pEntityPlayingSound = nullptr; m_SoundName[0] = 0; m_fPlaybackTime = 0.0f; m_bHasBeenPlayed = false; }

	CBaseEntity*	m_pEntityPlayingSound;	// Entity playing the sound event
	char			m_SoundName[64];		// Name of sound event to play
	float			m_fPlaybackTime;		// Length of time for sound event to play
	bool			m_bHasBeenPlayed;		// true = event has been played
};

class CStartOfRoundAudioPlayback
{
public:
	CStartOfRoundAudioPlayback() { m_NextAvailableTime = 1.0f; m_bPlaybackEnabled = false; }

	// Adds a sound event to be played at the next round restart
	void	AddSound( CBaseEntity* pEntityPlayingSound, const char* pName, float fDelayTime );

	// Play all the queued sounds
	void	PlaySounds( void );

	// Clear all the queued sounds
	void	ClearSounds( void );

	// Updates the playback when m_bPlaybackEnabled is valid
	void	Update( void );

	void	RemoveSoundByName( char* pName );

protected:
	// List of sounds to play at start of round
	std::vector< RoundStartSoundPlaybackData >	m_SoundEvents;

	// Time from start of round when the next added sound event can play
	float	m_NextAvailableTime;

	// Allows Update() to be called so sounds can play
	bool	m_bPlaybackEnabled;
};

//-----------------------------------------------------------------------------
namespace DT_CSLocalPlayerExclusive { extern RecvTable g_RecvTable; }
namespace DT_CSNonLocalPlayerExclusive { extern RecvTable g_RecvTable; }

// Receive proxies named by the annotations below; defined in c_cs_player.cpp.
void RecvProxy_HasDefuser( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_NightVision( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_FlashTime( const CRecvProxyData *pData, void *pStruct, void *pOut );

class CCSPlayer : public CBasePlayer, public CGameEventListener, public ICSPlayerAnimStateHelpers
{
	[[= ks::reflect::PredFrom<"m_flCycle", ks::reflect::Pred{ .flags = FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK } >{} ]]
	[[= ks::reflect::NetTable{ .name = "DT_CSLocalPlayerExclusive", .base = false } ]]
	[[= ks::reflect::From<"m_vecNetworkOrigin",
	ks::reflect::Net{ .enc = ks::reflect::WireEnc::VectorXY, .wire = "m_vecOrigin", .table = "DT_CSLocalPlayerExclusive" }>{} ]]
	[[= ks::reflect::From<"m_vecNetworkOrigin",
	ks::reflect::Net{ .wire = "m_vecOrigin", .table = "DT_CSLocalPlayerExclusive", .index = 2 }>{} ]]
	[[= ks::reflect::NetTable{ .name = "DT_CSNonLocalPlayerExclusive", .base = false } ]]
	[[= ks::reflect::From<"m_vecNetworkOrigin",
	ks::reflect::Net{ .enc = ks::reflect::WireEnc::VectorXY, .wire = "m_vecOrigin", .table = "DT_CSNonLocalPlayerExclusive" }>{} ]]
	[[= ks::reflect::From<"m_vecNetworkOrigin",
	ks::reflect::Net{ .wire = "m_vecOrigin", .table = "DT_CSNonLocalPlayerExclusive", .index = 2 }>{} ]]
	[[= ks::reflect::NetTable{ .name = "DT_CSPlayer" } ]]
	[[= ks::reflect::SubTable<"cslocaldata", &DT_CSLocalPlayerExclusive::g_RecvTable,
	nullptr, true>{} ]]
	[[= ks::reflect::SubTable<"csnonlocaldata", &DT_CSNonLocalPlayerExclusive::g_RecvTable,
	nullptr, true>{} ]]
	// Two lanes of m_angEyeAngles, which also sends whole as a QAngles prop; a member cannot
	// carry both shapes at once, so the lanes come through From<>.
	[[= ks::reflect::From<"m_angEyeAngles", ks::reflect::Net{ .index = 0 }>{} ]]
	[[= ks::reflect::From<"m_angEyeAngles", ks::reflect::Net{ .index = 1 }>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CCSPlayer, CBasePlayer );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	CCSPlayer();
	~CCSPlayer();

	float *	GetRenderClipPlane( void );
	float	m_fRenderingClipPlane[4];
	int		m_nLastClipPlaneSetupFrame;
	int		m_nClipPlaneProximityLimitAttachmentIndex;
	Vector	m_vecLastClipCameraPos;
	Vector	m_vecLastClipCameraForward;
	bool	m_bClipHitStaticWorld;
	bool	m_bCachedPlaneIsValid;
	CBaseWeaponWorldModel *m_pClippingWeaponWorldModel;

	virtual void SetSequence(int nSequence) override;

	virtual bool Simulate();
	virtual	void Spawn( void );
	virtual void UpdateOnRemove( void );
	virtual void OnSetDormant( bool bDormant );

	//EHANDLE GetCarriedHostage() const;
	void GiveCarriedHostage( EHANDLE hHostage );
	void RefreshCarriedHostage( bool bForceCreate );
	void RemoveCarriedHostage();

	bool HasDefuser() const;

	void GiveDefuser();
	void RemoveDefuser();

	bool HasNightVision() const;

	static CCSPlayer* GetLocalCSPlayer();
	CSPlayerState State_Get() const;
	CSPlayerState m_previousPlayerState;

	virtual float GetMinFOV() const;
	
	// Get how much $$$ this guy has.
	int GetAccount() const;

	// Returns one of the CS_CLASS_ enums.
	int PlayerClass() const;

	bool IsInBuyZone();
	bool IsInBuyPeriod();
	bool CanBuyDuringImmunity();
	bool CanPlayerBuy( bool display );

	bool CanShowTeamMenu() const;	// Returns true if we're allowed to show the team menu right now.

	// Get the amount of armor the player has.
	int ArmorValue() const;
	bool HasHelmet() const;
	bool HasHeavyArmor() const;

	// create tracers
	void CreateWeaponTracer( Vector vecStart, Vector vecEnd );

	virtual const QAngle& EyeAngles();
	virtual void CalcObserverView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );

	// observer mode
	void				SetSpecWatchingGrenade( CBaseEntity *pGrenade, bool bWatching );
	virtual int			GetObserverMode() const;
	virtual CBaseEntity	*GetObserverTarget() const;
	bool				IsSpecFollowingGrenade( void ) {return m_bIsSpecFollowingGrenade; }
	bool				IsHoldingSpecGrenadeKey( void );
	bool	m_bIsSpecFollowingGrenade;
	EHANDLE			m_hOldGrenadeObserverTarget;

	virtual void			GetRenderBounds( Vector& theMins, Vector& theMaxs );
	virtual void			GetShadowRenderBounds( Vector &mins, Vector &maxs, ShadowType_t shadowType );
	virtual bool			GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const;

	virtual void VPhysicsUpdate( IPhysicsObject *pPhysics );

	// Get the ID target entity index. The ID target is the player that is behind our crosshairs, used to
	// display the player's name.
	int GetIDTarget() const;
	int GetTargetedWeapon( void ) const;

	virtual void NotifyShouldTransmit( ShouldTransmitState_t state );
	virtual void ClientThink();
	virtual void OnTimeJump() override;
	
	virtual void OnDataChanged( DataUpdateType_t type );
	virtual void PostDataUpdate( DataUpdateType_t updateType );
	virtual bool Interpolate( float currentTime );
	virtual void UpdateStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity  );
	virtual surfacedata_t * GetFootstepSurface( const Vector &origin, const char *surfaceName );
	virtual void PlayClientJumpSound( void );
	virtual void ValidateModelIndex( void );

	static bool RenderLocalScreenSpaceEffect( CStrikeScreenSpaceEffect effect, IMatRenderContext *pRenderContext, int x, int y, int w, int h );
	bool AreScreenSpaceVomitParticlesActive( void ) const;
	bool DrawScreenSpaceVomitParticles( IMatRenderContext *pRenderContext );

	int		GetHealth() const		{ return m_iHealth; }
	virtual int	GetMaxHealth() const;

	bool		Weapon_CanSwitchTo(CBaseCombatWeapon *pWeapon);

	virtual void UpdateClientSideAnimation();
	virtual void ProcessMuzzleFlashEvent();
	void HandleTaserAnimation();
	
	virtual const Vector& GetRenderOrigin( void );
		
	bool CreateMove( float flInputSampleTime, CUserCmd *pCmd );

	std::vector< CBaseParticleEntity* > m_SmokeGrenades;

	virtual bool ShouldDraw( void );
	virtual bool UpdateDispatchLayer( CAnimationLayer *pLayer, CStudioHdr *pWeaponStudioHdr, int iSequence ) override;
	virtual void AccumulateLayers( IBoneSetup &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime );
	bool UpdateLayerWeaponDispatch( CAnimationLayer *pLayer, int iSequence );
	virtual float GetLayerSequenceCycleRate( CAnimationLayer *pLayer, int iSequence );
	virtual void BuildTransformations( CStudioHdr *pStudioHdr, BoneVector *pos, BoneQuaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed );

	virtual void DoExtraBoneProcessing( CStudioHdr *pStudioHdr, BoneVector pos[], BoneQuaternion q[], matrix3x4a_t boneToWorld[], CBoneBitList &boneComputed, CIKContext *pIKContext ) override;
	
	virtual bool SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime );

	void ReevauluateAnimLOD( int boneMask = 0 );

	virtual CBaseAnimating * BecomeRagdollOnClient();
	virtual IRagdoll* GetRepresentativeRagdoll() const;
	
	virtual void TeamChange( int iNewTeam );

	void ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName );

	// Have this player play the sounds from his view model's reload animation.
	void PlayReloadEffect( int iActAnimID, const Vector *pOrigin = nullptr );

	virtual void FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options );

	virtual void DropPhysicsMag( const char *options ) override;

	bool		 HasC4( void );
	bool HasWeaponOfType( int nWeaponID ) const;

	virtual bool CreateLightEffects( void ) { return false; }	//no dimlight effects

	// Sometimes the server wants to update the client's cycle to get the two to run in sync (for proper hit detection)
	virtual void SetServerIntendedCycle( float intended ) { m_serverIntendedCycle = intended; }
	virtual float GetServerIntendedCycle( void ) { return m_serverIntendedCycle; }

	virtual bool IsTaunting( void ) const { return m_bIsTaunting; }
	virtual bool IsThirdPersonTaunt( void ) const { return m_bIsThirdPersonTaunt; }
	virtual bool IsHoldingTaunt( void ) const { return m_bIsHoldingTaunt; }

	virtual bool IsLookingAtWeapon( void ) const { return m_bIsLookingAtWeapon; }
	virtual bool IsHoldingLookAtWeapon( void ) const { return m_bIsHoldingLookAtWeapon; }

	virtual float GetTauntYaw( void ) const { return m_flTauntYaw; }

	virtual const char			*GetVOIPParticleEffectName() const { return SPEECH_VOIP_PARTICLE_EFFECT_NAME; }
	virtual const char			*GetRadioHeadParticleEffectName() const { return RADIO_VOICE_PARTICLE_EFFECT_NAME; }
	virtual void				UpdateRadioHeadIcon( bool bVoice );
	virtual bool				ShouldShowRadioHeadIcon() const;
	virtual CNewParticleEffect	*GetRadioHeadParticleEffect( void );
	virtual CNewParticleEffect	*GetVOIPParticleEffect( void );
	virtual Vector				GetParticleHeadLabelOffset( void );
	virtual bool				HasSpeechVOIPParticleIconActive( void ) { return (m_speechVOIPParticleEffect && m_speechVOIPParticleEffect.IsValid()); }

	virtual void AddDecal( const Vector& rayStart, const Vector& rayEnd, const Vector& decalCenter, int hitbox, int decalIndex, bool doTrace, trace_t& tr, int maxLODToDecal );

	virtual void SetRenderAlpha( byte a );
	virtual void SetRenderMode( RenderMode_t nRenderMode, bool bForceUpdate = false );

	virtual int DrawModel( int flags, const RenderableInstance_t &instance );

	virtual bool ShouldReceiveProjectedTextures( int flags )
	{
		return ( this != CBasePlayer::GetLocalPlayer() || IsTaunting() || ShouldDraw() );
	}

	void ClearSoundEvents()
	{
		m_SoundEvents.clear();
	}

	CsMusicType_t GetCurrentMusic() { return m_nCurrentMusic; } const
	void SetCurrentMusic( CsMusicType_t nMusicType );
	float GetCurrentMusicElapsed()
	{
		return  gpGlobals->curtime - m_flCurrentMusicStartTime;
	}

	float GetMusicStartRoundElapsed()
	{
		return  gpGlobals->curtime - m_flMusicRoundStartTime;
	}

	bool IsPlayerDominated( int iPlayerIndex );
	bool IsPlayerDominatingMe( int iPlayerIndex );

	virtual float				GetFOV( void ) const;	
	
	virtual void CalcFreezeCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	virtual void CalcDeathCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	
	void UpdateFreezeCamFlashlightEffect( CBaseEntity *pTarget, float flAmount );
	void CancelFreezeCamFlashlightEffect();
	bool m_bFreezeCamFlashlightActive;
	CTextureReference m_freezeCamSpotLightTexture;

	virtual float GetDeathCamInterpolationTime();
	float GetFreezeFrameInterpolant( void );
	float GetFreezeFrameTilt( void ) { return m_flFreezeFrameTilt; }

	// check autoaim
	bool IsCursorOnAutoAimTarget();

	AcquireResult::Type CanAcquire( CSWeaponID weaponId, AcquireMethod::Type acquireMethod, CEconItemView *pItem = nullptr );
	int					GetCarryLimit( CSWeaponID weaponId );
	int					GetWeaponPrice( CSWeaponID weaponId, const CEconItemView *pWepView = nullptr ) const;
	CWeaponCSBase*		CSWeapon_OwnsThisType( CEconItemView *pItem ) const;

	virtual bool ShouldRegenerateOriginFromCellBits() const;

	bool IsOtherSameTeam( int nTeam );
	bool IsOtherEnemy( CCSPlayer *pPlayer );
	bool IsOtherEnemy( int nEntIndex );
	bool IsOtherEnemyAndPlaying( int nEntIndex );

	virtual void SetModelPointer( const model_t *pModel );

protected:
	ks::tracked_ref< CNewParticleEffect > m_radioHeadIconParticleEffect;

// Called by shared code.
public:

	// ICSPlayerAnimState overrides.
	virtual CWeaponCSBase* CSAnim_GetActiveWeapon();
	virtual bool CSAnim_CanMove();

	void DoAnimationEvent( PlayerAnimEvent_t event, int nData = 0 );

// Implemented in shared code.
public:	
	virtual float GetPlayerMaxSpeed();
	bool IsPrimaryOrSecondaryWeapon( CSWeaponType nType );

	bool GetUseConfigurationForHighPriorityUseEntity( CBaseEntity *pEntity, CConfigurationForHighPriorityUseEntity_t &cfg );
	CBaseEntity *GetUsableHighPriorityEntity( void );

	void GetBulletTypeParameters( 
		int iBulletType, 
		float &fPenetrationPower, 
		float &flPenetrationDistance );

	void FireBullet(
		Vector vecSrc,
		const QAngle &shootAngles,
		float flDistance,
		float flPenetration,
		int nPenetrationCount,
		int iBulletType,
		int iDamage,
		float flRangeModifier,
		CBaseEntity *pevAttacker,
		bool bDoEffects,
		float xSpread, float ySpread );

	bool HandleBulletPenetration( 
		float &flPenetration,
		int &iEnterMaterial,
		bool &hitGrate,
		trace_t &tr,
		Vector &vecDir,
		surfacedata_t *pSurfaceData,
		float flPenetrationModifier,
		float flDamageModifier,
		bool bDoEffects,
		int iDamageType,
		float flPenetrationPower,
		int &nPenetrationCount,
		Vector &vecSrc,
		float flDistance,
		float flCurrentDistance,
		float &fCurrentDamage );

	virtual QAngle	GetAimPunchAngle( void );
	QAngle	GetRawAimPunchAngle( void ) const;

	void KickBack(
		float fAngle,
		float fMagnitude );

	int GetDefaultCrouchedFOV( void ) const;

	void DisplayPenetrationDebug( Vector vecEnter, Vector vecExit, float flDistance, float flInitialDamage, float flDamageLostImpact, float flTotalLostDamage, short nEnterSurf, short nExitSurf );

	// Returns true if the player is allowed to move.
	bool CanMove() const;
	
	// Returns the player mask which includes the solid mask plus the team mask.
	virtual unsigned int PhysicsSolidMaskForEntity( void ) const;

	void OnJump( float fImpulse );
	void OnLand( float fVelocity );

	bool HasC4() const;	// Is this player carrying a C4 bomb?
	bool IsVIP() const;	// Is this player the VIP?

	virtual void SetAnimation( PLAYER_ANIM playerAnim );
	
	virtual bool ShouldInterpolate( void );

public:

	void UpdateIDTarget( void );
	void UpdateTargetedWeapon( void );
	void RemoveAddonModels( void );
	void UpdateMinModels( void );

	void CreateC4Effect( CBaseEntity *pEnt, bool bIsWeaponModel );
	void RemoveC4Effect( bool bIsWeaponModel );

	void SetActivity( Activity eActivity );
	Activity GetActivity( void ) const;

	void DoAnimStateEvent( PlayerAnimEvent_t evt );

	// Global/static methods
	virtual void ThirdPersonSwitch( bool bThirdperson );
	virtual void CalcView( Vector &eyeOrigin, QAngle &eyeAngles, float &zNear, float &zFar, float &fov );

public:

	IPlayerAnimState *m_PlayerAnimState;
	CCSGOPlayerAnimState *m_PlayerAnimStateCSGO;

	virtual Vector Weapon_ShootPosition();

	// Used to control animation state.
	Activity m_Activity;

	CNetworkVar( bool, m_bIsScoped, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bIsWalking, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Net{} ]] );
	// Predicted variables.
	CNetworkVar( bool, m_bResumeZoom, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Net{} ]] );
	CNetworkVar( CSPlayerState, m_iPlayerState, [[= ks::reflect::Net{} ]] );	// SupraFiend: this gives the current state in the joining process, the states are listed above
	CNetworkVar( bool, m_bIsDefusing, [[= ks::reflect::Net{} ]] );			// tracks whether this player is currently defusing a bomb
	CNetworkVar( bool, m_bIsGrabbingHostage, [[= ks::reflect::Net{} ]] );			// tracks whether this player is currently defusing a bomb
	CNetworkVar( bool, m_bIsRescuing, [[= ks::reflect::Net{} ]] );			// tracks whether this player is currently rescuing a hostage
	CNetworkVar( float, m_fImmuneToGunGameDamageTime, [[= ks::reflect::Net{} ]] );	// When gun game spawn damage immunity will expire
	float m_fImmuneToGunGameDamageTimeLast;
	CNetworkVar( bool, m_bGunGameImmunity, [[= ks::reflect::Net{} ]] );	// tracks whether this player is currently immune in gun game
	CNetworkVar( bool, m_bHasMovedSinceSpawn, [[= ks::reflect::Net{} ]] ); // Whether player has moved from spawn position
	CNetworkVar( bool, m_bMadeFinalGunGameProgressiveKill, [[= ks::reflect::Net{} ]] );
	CNetworkVar( int, m_iGunGameProgressiveWeaponIndex, [[= ks::reflect::Net{} ]] ); // index of current gun game weapon
	CNetworkVar( int, m_iNumGunGameTRKillPoints, [[= ks::reflect::Net{} ]] );	// number of kill points accumulated so far in TR Gun Game mode (resets to 0 when weapon is upgraded)
	CNetworkVar( int, m_iNumGunGameKillsWithCurrentWeapon, [[= ks::reflect::Net{} ]] );
	CNetworkVar( int, m_iNumRoundKills, [[= ks::reflect::Net{} ]] );	// number of kills a player has gotten in a round
	CNetworkVar( int, m_iNumRoundKillsHeadshots, [[= ks::reflect::Net{} ]] );	// number of headshot kills a player has gotten in a round
	CNetworkVar( float, m_fMolotovUseTime, [[= ks::reflect::Net{} ]] );	// Molotov can be used if current time is after this time
	CNetworkVar( float, m_fMolotovDamageTime, [[= ks::reflect::Net{} ]] );	// Last time when this player was burnt by Molotov damage
	CNetworkVar( bool, m_bInBombZone, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bInBuyZone, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bInNoDefuseArea, [[= ks::reflect::Net{} ]] );
	CNetworkVar( int, m_iThrowGrenadeCounter, [[= ks::reflect::Net{} ]] );	// used to trigger grenade throw animations.
	CNetworkVar( bool, m_bWaitForNoAttack, [[= ks::reflect::Net{} ]] );	// flag to indicate player cannot attack until the attack button is released
	CNetworkVar( bool, m_bIsRespawningForDMBonus, [[= ks::reflect::Net{} ]] ); // flag to indicate player wants to spawn with the deathmatch bonus weapons
	CNetworkVar( float, m_flGuardianTooFarDistFrac, [[= ks::reflect::Net{} ]] );
	CNetworkVar( float, m_flDetectedByEnemySensorTime, [[= ks::reflect::Net{} ]] );
	float m_flNextGuardianTooFarWarning;

	CNetworkVar( bool, m_bKilledByTaser, [[= ks::reflect::Net{} ]] );
	CNetworkVar( int, m_iMoveState, [[= ks::reflect::Net{} ]] );		// Is the player trying to run or walk or idle?  Tells us what the player is "trying" to do.

	CNetworkVar( bool, m_bCanMoveDuringFreezePeriod, [[= ks::reflect::Net{} ]] );

	CNetworkVar( bool, m_isCurrentGunGameLeader, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_isCurrentGunGameTeamLeader, [[= ks::reflect::Net{} ]] );
		
	CNetworkString( m_szArmsModel, MAX_MODEL_STRING_SIZE, [[= ks::reflect::Net{} ]] );		// Which arms we're using for the view model.
	
	const PlayerViewmodelArmConfig *m_pViewmodelArmConfig;

	CNetworkVar( float, m_flLowerBodyYawTarget, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bStrafing, [[= ks::reflect::Net{} ]] );

	bool m_bUseNewAnimstate;

	CBoneSnapshot m_boneSnapshots[2];
	bool IsAnyBoneSnapshotPending( void );

	float m_flLastSpawnTimeIndex;

	virtual void NotifyOnLayerChangeSequence( const CAnimationLayer* pLayer, const int nNewSequence ) override;
	virtual void NotifyOnLayerChangeWeight( const CAnimationLayer* pLayer, const float flNewWeight ) override;
	virtual void NotifyOnLayerChangeCycle( const CAnimationLayer* pLayer, const float flNewCycle ) override;

	bool IsInHostageRescueZone( void );

	// This is a combination of the ADDON_ flags in cs_shareddefs.h.
	CNetworkVar( int, m_iAddonBits, [[= ks::reflect::Net{} ]] );

	// Clients don't know about holstered weapons, so we need to be told about them here
	CNetworkVar( int, m_iPrimaryAddon, [[= ks::reflect::Net{} ]] );
	CNetworkVar( int, m_iSecondaryAddon, [[= ks::reflect::Net{} ]] );

	// How long the progress bar takes to get to the end. If this is 0, then the progress bar
	// should not be drawn.
	CNetworkVar( int, m_iProgressBarDuration, [[= ks::reflect::Net{} ]] );
	
	// When the progress bar should start.
	CNetworkVar( float, m_flProgressBarStartTime, [[= ks::reflect::Net{} ]] );

	CNetworkVar( float, m_flStamina, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = 0.1f } ]]
                                 [[= ks::reflect::Net{ .table = "DT_CSLocalPlayerExclusive" } ]] );
	CNetworkVar( int, m_iDirection, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Net{ .table = "DT_CSLocalPlayerExclusive" } ]] );	// The current lateral kicking direction; 1 = right,  0 = left
	CNetworkVar( int, m_iShotsFired, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Net{ .table = "DT_CSLocalPlayerExclusive" } ]] );	// number of shots fired recently
	CNetworkVar( int, m_nNumFastDucks, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Net{ .table = "DT_CSLocalPlayerExclusive" } ]] ); // UNUSED.  Kept for backwards demo compatibility.  $$$REI TODO: Investigate safely removing variables
	CNetworkVar( bool, m_bDuckOverride, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Net{ .table = "DT_CSLocalPlayerExclusive" } ]] ); // force the player to duck regardless of if they're holding crouch
	
	CNetworkVar( bool, m_bNightVisionOn, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int } ]] [[= ks::reflect::Proxy<RecvProxy_NightVision, ks::reflect::WireSide::Recv>{} ]] );
	CNetworkVar( bool, m_bHasNightVision, [[= ks::reflect::Net{} ]] );
	CNetworkVar( float, m_flVelocityModifier, [[= ks::reflect::Net{ .table = "DT_CSLocalPlayerExclusive" } ]] );
	CNetworkVar( float, m_flGroundAccelLinearFracLastTime, [[= ks::reflect::Net{} ]] );

	CNetworkVar( int, m_iStartAccount, [[= ks::reflect::Net{} ]] );		// how much cash the player started the round with

	CNetworkVar( int, m_totalHitsOnServer, [[= ks::reflect::Net{} ]] );

	CNetworkVar( bool, m_bIsBuyMenuOpen );
	bool IsBuyMenuOpen( void ) { return m_bIsBuyMenuOpen; } 
	void SetBuyMenuOpen( bool bOpen ); 

	[[= ks::reflect::Net{} ]] EHANDLE	m_hRagdoll;

	[[= ks::reflect::Net{} ]] EHANDLE	m_hCarriedHostage;
	[[= ks::reflect::Net{} ]] EHANDLE	m_hCarriedHostageProp;
	bool	m_bPlayingHostageCarrySound;

	CWeaponCSBase* GetActiveCSWeapon() const;
	CWeaponCSBase* GetCSWeapon( CSWeaponID id ) const;

	virtual ShadowType_t		ShadowCastType();

	bool HasShield( void ) { return false; }
	bool IsShieldDrawn( void ) { return false; }
	void SetShieldDrawnState( bool bState ) {}
	
	bool IsMiniScoreHidden( void ) { return m_bHud_MiniScoreHidden; }
	bool IsRadarHidden( void ) { return m_bHud_RadarHidden; }

	virtual int GetLastKillerIndex( void ) { return m_nLastKillerIndex; }
	int GetLastConcurrentKilled( void ) { return m_nLastConcurrentKilled; }
	int GetDeathCamMusicIndex( void ) { return m_nDeathCamMusic; }

	float m_flNightVisionAlpha;

	float m_flFlashBangTime;		// end time
	float m_flFlashScreenshotAlpha;
	float m_flFlashOverlayAlpha;
	bool m_bFlashBuildUp;
	bool m_bFlashDspHasBeenCleared;
	bool m_bFlashScreenshotHasBeenGrabbed;

	bool IsFlashBangActive( void ) { return ( m_flFlashDuration > 0.0f ) && ( gpGlobals->curtime < m_flFlashBangTime ); }
	bool IsFlashBangBuildUpActive( void ) { return m_bFlashBuildUp && IsFlashBangActive(); }
	float GetFlashStartTime( void ) { return (m_flFlashBangTime - m_flFlashDuration); }
	float GetFlashTimeElapsed( void ) { return std::max( gpGlobals->curtime - GetFlashStartTime(), 0.0f ); }

	bool IsBlinded( void ) { return (m_flFlashBangTime - 1.0f) > gpGlobals->curtime; }
	CNetworkVar( float, m_flFlashMaxAlpha, [[= ks::reflect::Net{} ]] );
	CNetworkVar( float, m_flFlashDuration, [[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_FlashTime, ks::reflect::WireSide::Recv>{} ]] );	

	float GetLastFiredWeaponTime( void ) { return m_flLastFiredWeaponTime; }
	float m_flLastFiredWeaponTime;

	// Having the RecvProxy in the player allows us to keep the var private
	static void RecvProxy_CycleLatch( const CRecvProxyData *pData, void *pStruct, void *pOut );

	// Bots and hostages auto-duck during jumps
	bool m_duckUntilOnGround;

	Vector m_lastStandingPos; // used by the gamemovement code for finding ladders

	void SurpressLadderChecks( const Vector& pos, const Vector& normal );
	bool CanGrabLadder( const Vector& pos, const Vector& normal );

	// [tj] checks if this player has another given player as a friend.
	bool HasPlayerAsFriend( CCSPlayer* player );

	bool MadeFinalGunGameProgressiveKill( void ) { return m_bMadeFinalGunGameProgressiveKill; }
	int GetPlayerGunGameWeaponIndex( void ) { return m_iGunGameProgressiveWeaponIndex; }
	int GetNumGunGameTRKillPoints( void ) { return m_iNumGunGameTRKillPoints; }
	int GetNumGunGameKillsWithCurrentWeapon( void ) { return m_iNumGunGameKillsWithCurrentWeapon; }
	
	int GetNumRoundKills( void ) { return m_iNumRoundKills; }
	int GetNumRoundKillsHeadshots( void ) { return m_iNumRoundKillsHeadshots; }

	void DisplayInventory(bool showPistol);
	
	bool CanUseGrenade( CSWeaponID );

	MedalRank_t GetRank( MedalCategory_t category );

	uint32 GetMusicID( void );

	void ToggleRandomWeapons( void );

	bool ShouldShowTeamPlayerColors( int nOtherTeamNum );
	bool ShouldShowTeamPlayerColorLetters( void );

	static void UpdateGlowsForAllPlayers( void );
	CGlowObject m_GlowObject;

	uint16 GetCurrentEquipmentValue ( void ) const { return m_unCurrentEquipmentValue; }
	uint16 GetRoundStartEquipmentValue ( void ) const { return m_unRoundStartEquipmentValue; }
	uint16 GetFreezetimeEndEquipmentValue ( void ) const { return m_unFreezetimeEndEquipmentValue; }

	bool IsAbleToInstantRespawn( void );
	char const * IsAbleToApplySpray( trace_t *ptr, Vector *pvecForward, Vector *pvecRight );

	float GetLastSmokeOverlayAlpha( void ) const { return m_flLastSmokeOverlayAlpha; }

	const char *GetHalloweenMaskModelAddon( CCSPlayer *pPlayer );

	uint32 GetActiveQuestID( void ) const;
	QuestProgress::Reason GetQuestProgressReason( void ) const;

	Vector m_vecLastMuzzleFlashPos;
	QAngle m_angLastMuzzleFlashAngle;
	EHANDLE m_hMuzzleFlashShape;
	int m_iHealthBarRenderMaskIndex;
	float m_flHealthFadeValue;
	float m_flHealthFadeAlpha;

	// Match Stats data
	[[= ks::reflect::Net{} ]] int m_iMatchStats_Kills[ MAX_MATCH_STATS_ROUNDS ];				//kills, per round
	[[= ks::reflect::Net{} ]] int m_iMatchStats_Damage[ MAX_MATCH_STATS_ROUNDS ];				//damage, per round
	[[= ks::reflect::Net{} ]] int m_iMatchStats_EquipmentValue[ MAX_MATCH_STATS_ROUNDS ];				//Equipment value, per round
	[[= ks::reflect::Net{} ]] int m_iMatchStats_MoneySaved[ MAX_MATCH_STATS_ROUNDS ];			//Saved money, per round
	[[= ks::reflect::Net{} ]] int m_iMatchStats_KillReward[ MAX_MATCH_STATS_ROUNDS ];
	[[= ks::reflect::Net{} ]] int m_iMatchStats_LiveTime[ MAX_MATCH_STATS_ROUNDS ];
	[[= ks::reflect::Net{} ]] int m_iMatchStats_Deaths[ MAX_MATCH_STATS_ROUNDS ];
	[[= ks::reflect::Net{} ]] int m_iMatchStats_Assists[ MAX_MATCH_STATS_ROUNDS ];
	[[= ks::reflect::Net{} ]] int m_iMatchStats_HeadShotKills[ MAX_MATCH_STATS_ROUNDS ];
	[[= ks::reflect::Net{} ]] int m_iMatchStats_Objective[ MAX_MATCH_STATS_ROUNDS ];
	[[= ks::reflect::Net{} ]] int m_iMatchStats_CashEarned[ MAX_MATCH_STATS_ROUNDS ];
	[[= ks::reflect::Net{} ]] int m_iMatchStats_UtilityDamage[ MAX_MATCH_STATS_ROUNDS ];
	[[= ks::reflect::Net{} ]] int m_iMatchStats_EnemiesFlashed[ MAX_MATCH_STATS_ROUNDS ];

	ks::tracked_ref<CNewParticleEffect> m_ARScreenGlowEffect;

private:
	void AnimateGlows( void );
	void UpdateGlows( void );
	void UpdateFlashBangEffect( void );

	CountdownTimer m_ladderSurpressionTimer;
	Vector m_lastLadderNormal;
	Vector m_lastLadderPos;

	ClientCCHandle_t	m_CCDeathHandle;	// handle to death cc effect
	float				m_flDeathCCWeight;	// for fading in cc effect	

	bool	m_bFreezeFrameCloseOnKiller;
	int		m_nFreezeFrameShiftSideDist;
	Vector	m_vecFreezeFrameEnd;
	QAngle	m_vecFreezeFrameAnglesStart;
	float	m_flFreezeFrameTilt;	

	bool	m_bOldIsScoped;
	
	CNetworkVar( QuestProgress::Reason, m_nQuestProgressReason, [[= ks::reflect::Net{ .table = "DT_CSLocalPlayerExclusive" } ]] );

	CNetworkVar( uint16, m_unCurrentEquipmentValue, [[= ks::reflect::Net{} ]] );
	CNetworkVar( uint16, m_unRoundStartEquipmentValue, [[= ks::reflect::Net{} ]] );
	CNetworkVar( uint16, m_unFreezetimeEndEquipmentValue, [[= ks::reflect::Net{} ]] );

	CNetworkArray( MedalRank_t, m_rank, MEDAL_CATEGORY_COUNT, [[= ks::reflect::Net{} ]] );

	CNetworkVar( uint16, m_unMusicID, [[= ks::reflect::Net{} ]] );

	void ProcessSpottedEntityUpdate( void );
	void UpdateSoundEvents( void );
//	void HandleTauntSound( void );

	void CreateNewCustomClothingModels( void );
	void UpdateCustomClothingModels( void );

private:
	
	bool ApplyCompositeMaterialsToCustomClothingModel( CBaseAnimating* pTargetEntity, CEconItemView *pItem, int nSlotId, bool bForceRegenerate /*= false */ ) const;

	void CreateAddonModel( int i );
	void UpdateAddonModels( bool bForce = false );
	bool m_bAddonModelsAreOutOfDate;
	void UpdateHostageCarryModels();

	void PushawayThink();

	Vector m_vecThirdPersonViewPositionOverride;
	virtual Vector GetThirdPersonViewPosition( void );

	//=============================================================================
	// HPE_BEGIN:
	// [dwenger] Added for auto-buy functionality
	//=============================================================================
public:
	void FireGameEvent( IGameEvent *event );
private:
	//=============================================================================
	// HPE_END
	//=============================================================================

	[[= ks::reflect::Net{} ]] int		m_iAccount;	
	[[= ks::reflect::Net{} ]] bool	m_bHasHelmet;
	[[= ks::reflect::Net{} ]] bool	m_bHasHeavyArmor;
	[[= ks::reflect::Net{} ]] int		m_iClass;
	[[= ks::reflect::Net{} ]] int		m_ArmorValue;
	[[= ks::reflect::Net{} ]] QAngle	m_angEyeAngles;
	[[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int } ]] [[= ks::reflect::Proxy<RecvProxy_HasDefuser, ks::reflect::WireSide::Recv>{} ]] bool	m_bHasDefuser;
	[[= ks::reflect::Net{} ]] bool	m_bInHostageRescueZone;
	float	m_fNextThinkPushAway;

	// Glow stuff
	float	m_fNextGlowCheckUpdate;
	float	m_fNextGlowCheckInterval;
	float	m_fGlowAlpha;
	float	m_fGlowAlphaTarget;
	float	m_fGlowAlphaUpdateTime;
	float	m_fGlowAlphaTargetTime;

	static bool s_bPlayingFreezeCamSound;
	bool	m_bShouldAutobuyDMWeapons;

	//=============================================================================
	// HPE_BEGIN:
	// [dwenger] Added for auto-buy functionality
	//=============================================================================
	bool    m_bShouldAutobuyNow;
	//=============================================================================
	// HPE_END
	//=============================================================================

	[[= ks::reflect::Net{} ]] bool	m_bHud_MiniScoreHidden;
	[[= ks::reflect::Net{} ]] bool	m_bHud_RadarHidden;

	[[= ks::reflect::Net{} ]] int m_nLastKillerIndex;
	[[= ks::reflect::Net{} ]] int m_nLastConcurrentKilled;
	[[= ks::reflect::Net{} ]] int m_nDeathCamMusic;

	CInterpolatedVar< QAngle >	m_iv_angEyeAngles;

	// ID Target
	int	m_iIDEntIndex;	
	CountdownTimer		m_delayTargetIDTimer;
	
	int					m_iTargetedWeaponEntIndex;

	// Show the ID target after the cursor leaves the entity
	int					m_iOldIDEntIndex;
	CountdownTimer		m_holdTargetIDTimer;	

	void ReleaseFlashlight( void );
	Beam_t	*m_pFlashlightBeam;

	class CCSSoundEvent
	{
	public:
		CCSSoundEvent() : m_bHasSoundOrigin(false) {}
		string_t m_SoundName;
		float m_flEventTime;	// Play the event when gpGlobals->curtime goes past this.
		Vector m_SoundOrigin;
		bool m_bHasSoundOrigin;
	};
	std::vector<CCSSoundEvent> m_SoundEvents;

	// List of sounds to play at start of round
	CStartOfRoundAudioPlayback	m_StartOfRoundSoundEvents;

	// manage per play music
	CsMusicType_t m_nCurrentMusic;
	float m_flCurrentMusicStartTime;
	float m_flMusicRoundStartTime;
	
	// This is the list of addons hanging off the guy (grenades, C4, nightvision, etc).
	std::vector<CAddonModel> m_AddonModels;
	int m_iLastAddonBits;
	int m_iLastPrimaryAddon;
	int m_iLastSecondaryAddon;

	ks::tracked_ref<CNewParticleEffect> m_hC4AddonLED;
	ks::tracked_ref<CNewParticleEffect> m_hC4WeaponLED;

	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<&CCSPlayer::RecvProxy_CycleLatch, ks::reflect::WireSide::Recv>{} ]] int m_cycleLatch;				// server periodically updates this to fix up our anims, here it is a 4 bit fixed point
	float m_serverIntendedCycle;	// server periodically updates this to fix up our anims, here it is the float we want, or -1 for no override

	// [tj] Network variables that track who are dominating and being dominated by
	CNetworkArray( bool, m_bPlayerDominated, MAX_PLAYERS+1, [[= ks::reflect::Net{ .table = "DT_CSLocalPlayerExclusive" } ]] );		// array of state per other player whether player is dominating other players
	CNetworkArray( bool, m_bPlayerDominatingMe, MAX_PLAYERS+1, [[= ks::reflect::Net{ .table = "DT_CSLocalPlayerExclusive" } ]] );	// array of state per other player whether other players are dominating this player

	CNetworkArray( int, m_iWeaponPurchasesThisRound, MAX_WEAPONS, [[= ks::reflect::Net{ .table = "DT_CSLocalPlayerExclusive" } ]] );		// number of times each weapon purchased this round; used to limit repurchases

	CNetworkVar( bool, m_bIsTaunting );
	CNetworkVar( bool, m_bIsThirdPersonTaunt );
	CNetworkVar( bool, m_bIsHoldingTaunt );
	CNetworkVar( float, m_flTauntYaw );

	CNetworkVar( bool, m_bIsLookingAtWeapon, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bIsHoldingLookAtWeapon, [[= ks::reflect::Net{} ]] );

	bool				m_bWasTaunting;
	bool				m_bTauntInterpolating;
	CameraThirdData_t	m_TauntCameraData;
	float				m_flTauntCamCurrentDist;
	float				m_flTauntCamTargetDist;
	QAngle				m_angTauntPredViewAngles;
	QAngle				m_angTauntEngViewAngles;

public:
	[[= ks::reflect::Net{} ]] float m_flThirdpersonRecoil;

	// taser items
	float m_nextTaserShakeTime;
	float m_firstTaserShakeTime;

	float m_flLastSmokeOverlayAlpha;
	
	CCSPlayer( const CCSPlayer & );

	// For interpolating between observer targets 
protected:
	void						UpdateObserverTargetVisibility( void ) const;
	Vector						m_vecObserverInterpolateOffset;			// Offset vec applied to the view which decays over time
	Vector						m_vecObserverInterpStartPos;
	float						m_flObsInterp_PathLength;				// Full path lenght being interpolated
	Quaternion					m_qObsInterp_OrientationStart;
	Quaternion					m_qObsInterp_OrientationTravelDir;
	eObserverInterpState		m_obsInterpState;
	bool						m_bObserverInterpolationNeedsDeferredSetup;

public:
	bool								ShouldInterpolateObserverChanges() const;
	void								StartObserverInterpolation( const QAngle &startAngles );
	virtual eObserverInterpState		GetObserverInterpState( void ) const override;
	virtual bool						IsInObserverInterpolation( void ) const override { return ShouldInterpolateObserverChanges() && m_obsInterpState != OBSERVER_INTERP_NONE; }
	virtual void						SetObserverTarget( EHANDLE hObserverTarget ) override;
	void								InterpolateObserverView( Vector& vOrigin, QAngle& vAngles );
	Vector								GetObserverInterpolatedOffsetVector( void ) { return m_vecObserverInterpolateOffset; }

	bool IsAssassinationTarget( void ) const;

private:
	CNetworkVar( bool, m_bIsAssassinationTarget, [[= ks::reflect::Net{} ]] );	// This player is an assassination target for an active mission

public: 
	bool IsControllingBot()							const { return m_bIsControllingBot; }
	bool CanControlObservedBot()					const { return m_bCanControlObservedBot; }

	int GetControlledBotIndex()						const { return m_iControlledBotEntIndex; }

	virtual bool	GetAttachment( int number, matrix3x4_t &matrix );
	virtual bool	GetAttachment( int number, Vector &origin );
	virtual	bool	GetAttachment( int number, Vector &origin, QAngle &angles );

private:
	[[= ks::reflect::Net{} ]] bool		m_bIsControllingBot;	// Are we controlling a bot? 
	// Note that this can be true even if GetControlledPlayer() returns NULL, IFF we started controlling a bot and then the bot was deleted for some reason. 
	[[= ks::reflect::Net{} ]] bool		m_bCanControlObservedBot;	// True if the player can control the bot s/he is observing
	[[= ks::reflect::Net{} ]] int			m_iControlledBotEntIndex;

	void	UpdateFirstPersonShadowCasters( void );

	float		m_flNextMagDropTime;
	int			m_nLastMagDropAttachmentIndex;
	CNetworkVar( bool, m_bHasControlledBotThisRound, [[= ks::reflect::Net{} ]] );

public: 
	int GetEloBracket( void ) const;
	void SetEloBracket ( int bracket );

public:
	void	ClearAllBulletHitModels( void );
	void	UpdateAllBulletHitModels( void );
	std::vector< CBulletHitModel* > m_vecBulletHitModels;

	// This code allowed us to measure discrepency between client and server bullet hits.
	// It became obsolete when we started using a separate seed for client and server
	// to eliminate 'rage' hacks.
	//
 	std::vector< clientHitVerify_t > m_vecBulletVerifyListClient;
 	std::vector< clientHitVerify_t > m_vecBulletVerifyListServer;
 
 	void	CompareClientServerBulletHits( void );
 
 	CUserMessageBinder m_UMCMsgReportHit;
 
 	uint8 m_ui8ClientServerHitDifference;

	// Ammo count at death OGS data
	struct roundEndAmmoCount_t
	{
		roundEndAmmoCount_t()
		{
			V_memset( this, 0, sizeof( *this ) );
		}

		int		nPrimaryWeaponDefIndex;
		int		nPrimaryWeaponAmmoCount;

		int		nSecondaryWeaponDefIndex;
		int		nSecondaryWeaponAmmoCount;
	
	};

	roundEndAmmoCount_t m_roundEndAmmoCount;

	Vector m_vecLastAliveLocalVelocity;

private:
	void RecordAmmoForRound( void );

	Vector m_vecLastContactShadowTraceOriginLeft;
	Vector m_vecLastContactShadowTraceOriginRight;
	float m_flLastContactShadowGroundHeightLeft;
	float m_flLastContactShadowGroundHeightRight;

public:

	EHANDLE m_hContactShadowLeft;
	EHANDLE m_hContactShadowRight;

	void CreateClientEffectModels( void );
	void RemoveClientEffectModels( void );

};

CCSPlayer* GetLocalOrInEyeCSPlayer( void );
CCSPlayer* GetHudPlayer( void );	// get the player we should show the HUD for (local or observed)

inline CCSPlayer *ToCSPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return nullptr;

	return static_cast<CCSPlayer*>( pEntity );
}

void AddSmokeGrenade( Vector location, int iEntityId );
void AddSmokeGrenadeHandle( EHANDLE hGrenade );
void RemoveSmokeGrenade( Vector location, int iEntityId );
void RemoveSmokeGrenadeHandle( EHANDLE hGrenade );
void RemoveAllSmokeGrenades( void );
void RemoveSmokeGrenadeHandle( EHANDLE hGrenade );
bool LineGoesThroughSmoke( Vector from, Vector to, bool grenadeBloat );
void TruncatePlayerName( wchar_t *pCleanedHTMLName, int destLen, int nTruncateAt, bool bSkipHTML = false );

class CCSRagdoll : public CBaseAnimatingOverlay
{
	[[= ks::reflect::NetTable{ .name = "DT_CSRagdoll", .base = false } ]]
	[[= ks::reflect::From<"m_vecNetworkOrigin", ks::reflect::Net{ .wire = "m_vecOrigin" }>{} ]]
	[[= ks::reflect::From<"m_nModelIndex", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_nForceBone", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_vecForce", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_iTeamNum", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_bClientSideAnimation", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CCSRagdoll, CBaseAnimatingOverlay );
	DECLARE_CLIENTCLASS();
	
	CCSRagdoll();
	~CCSRagdoll();

	virtual void OnDataChanged( DataUpdateType_t type );

	int GetPlayerEntIndex() const;
	IRagdoll* GetIRagdoll() const;
	void GetRagdollInitBoneArrays( matrix3x4a_t *pDeltaBones0, matrix3x4a_t *pDeltaBones1, matrix3x4a_t *pCurrentBones, float boneDt );
	void GetRagdollInitBoneArraysYawMode( matrix3x4a_t *pDeltaBones0, matrix3x4a_t *pDeltaBones1, matrix3x4a_t *pCurrentBones, float boneDt );

	void ApplyRandomTaserForce( void );
	void ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName );
	bool IsInitialized() { return m_bInitialized; }
	virtual void ValidateModelIndex( void );

	void SetRagdollClientSideAddon( uint32 uiAddonMask );
	
	int	BloodColor(){ return BLOOD_COLOR_RED; }

	void ApplySemiRandomDirectionalForce( Vector vecDir, float flStrength );

	void AttachWearableGibsFromPlayer( CCSPlayer *pParentPlayer );
	void DestroyAttachedWearableGibs( void );
	
	int GetDeathPose( void ) { return m_iDeathPose; }

private:
	
	CCSRagdoll( const CCSRagdoll & ) :m_nGlowObjectHandle( -1) { }
	void DestroyGlowObject();

	void Interp_Copy( CBaseAnimatingOverlay *pSourceEntity );

	void CreateLowViolenceRagdoll( void );
	void CreateCSRagdoll( void );

private:

	[[= ks::reflect::Net{} ]] EHANDLE	m_hPlayer;
	CHandle<CBaseAnimating> m_hHolidayHatAddon;
	CHandle<CBaseAnimating> m_hHolidayGhostAddon;
	CHandle<CBaseAnimating> m_hAssassinationTargetAddon;
	CNetworkVector( m_vecRagdollVelocity, [[= ks::reflect::Net{} ]] );
	CNetworkVector( m_vecRagdollOrigin, [[= ks::reflect::Net{} ]] );
	CNetworkVar(int, m_iDeathPose, [[= ks::reflect::Net{} ]] );
	CNetworkVar(int, m_iDeathFrame, [[= ks::reflect::Net{} ]] );
	CNetworkVar(float, m_flDeathYaw, [[= ks::reflect::Net{} ]] );
	CNetworkVar(float, m_flAbsYaw, [[= ks::reflect::Net{} ]] );
	bool m_bInitialized;

	int m_nGlowObjectHandle;
};

class CAchievement_StatGoal;
class CBaseAchievement;

using namespace ::ks::reflect;

// Purpose: Achievements (aka Medals) are divided into categories. Progress towards
// completing a category of medals gives the player a higher 'rank'. Player
// rank is sent to the server for display on the scoreboard.
// Ranks, Medals and medal-relevant gamestats earned during a round are recorded
// here for the purpose of displaying progress towards ranks to the player.

#if !defined CS_PLAYER_RANK_MGR_H
#define CS_PLAYER_RANK_MGR_H


// Structs for recording rank-relevent events that occur during a round
struct RankIncreasedEvent_t
{	
	RankIncreasedEvent_t( CBaseAchievement* pAch, MedalCategory_t cat )
		:m_pAchievement( pAch ), m_category( cat ) {}

	CBaseAchievement *m_pAchievement;
	MedalCategory_t m_category;
};

struct MedalEarnedEvent_t 
{
	MedalEarnedEvent_t( CBaseAchievement* pAch, MedalCategory_t cat )
		:m_pAchievement( pAch ), m_category( cat ) {}
	CBaseAchievement *m_pAchievement;
	MedalCategory_t m_category;
};

struct MedalStatEvent_t
{
	MedalStatEvent_t( CBaseAchievement* pAch, MedalCategory_t category, CSStatType_t type )
		:m_pAchievement( pAch ), m_category( category ), m_StatType( type ) {}
	CBaseAchievement *m_pAchievement;
	MedalCategory_t m_category;
	CSStatType_t m_StatType;
};

class CPlayerRankManager : public CAutoGameSystem, public CGameEventListener
{
public:
	CPlayerRankManager();
	~CPlayerRankManager();

	virtual bool Init();
	virtual void Shutdown();
	virtual void LevelInitPostEntity();

	bool HasBuiltMedalCategories( void ) const;

	// Get rank ups and medals earned during the current round. These lists are reset on round start.
	const std::vector< MedalEarnedEvent_t >& GetMedalsEarnedThisRound( void ) const;
	const std::vector< RankIncreasedEvent_t >& GetRankIncreasesThisRound( void ) const;

	// Builds a list of progress based achievements (goal > 1) who had their dependant gamestat increased this round.
	void GetMedalStatsEarnedThisRound( std::vector< MedalStatEvent_t >& ) const;

	// Number of achievements compeleted by the local player in the given category.
	int CountAchievedInCategory( MedalCategory_t category ) const;
	// Total count of medals in a category
	int GetTotalMedalsInCategory( MedalCategory_t category ) const;
	// Number of medals required to be awarded a given rank
	int GetMinMedalsForRank( MedalCategory_t category, MedalRank_t rank ) const;

	// Blindly sets the rank for the given category
	void SetRankForCategory( MedalCategory_t category, int nRank ) { m_rank[category] = (MedalRank_t)nRank; }
	// Calculates the player's rank for a given category and returns it. Does not set the internal rank variable.
	MedalRank_t CalculateRankForCategory( MedalCategory_t category ) const;

	const char *GetMedalCatagoryName( MedalCategory_t category );
	const char *GetMedalCatagoryRankName( int nRank );

	void NoteEloBracketChanged( int iOldBracket, int iNewBracket );

	// Elo is special: UI display code will explicitly mark when we've shown the
	// 'elo changed' update to the user beacuse we don't want to miss displaying this update.
	int GetEloBracketChange( int &iOutNewEloBracket );
	void ResetRecordedEloBracketChange( void ); 

	// Debug spew.
	void PrintRankProgressThisRound() const;

	// Match events
public:
	void OnEvent( KeyValues *pEvent );

private:
	struct MedalCategoryInfo_t
	{
		int m_totalInCategory;
		int m_minCountForRank[MEDAL_RANK_COUNT];
	};

public:
	void FireGameEvent( IGameEvent *event );

private:
	void SendRankDataToServer( void );
	void BuildMedalCategories( void );
	void RecalculateRanks( void );
	void OnRoundStart();
	void OnAchievementEarned( int achievementID );
	void OnSeasonCoinLeveledUp( int iCategory = MEDAL_CATEGORY_NONE, int iRank = MEDAL_RANK_NONE );
	bool CheckCategoryRankLevelUp( int iCategory, int iRank );
	void CheckForStatProgress();

	// Local player's rank data.
	MedalRank_t m_rank[MEDAL_CATEGORY_COUNT];
	// Info about each medal category.
	MedalCategoryInfo_t m_categoryInfo[MEDAL_CATEGORY_COUNT];
	// List of achievements assigned to each category.
	std::vector< CBaseAchievement* > m_medalCategoryList[MEDAL_CATEGORY_COUNT];

	// Per-round rank relevant events.
	std::vector< RankIncreasedEvent_t > m_RankIncreases;
	std::vector< MedalEarnedEvent_t > m_MedalsEarned;

	// List of stat based achievement
	std::vector< CAchievement_StatGoal* > m_StatBasedAchievements;

	int m_iEloBracketDelta;
	int m_iNewEloBracket;

	bool m_bMedalCategoriesBuilt;
};

extern CPlayerRankManager g_PlayerRankManager;

const char* GetLocTokenForStatId( const CSStatType_t id );

#endif

#undef CS_PLAYER_RANK_MGR_H

class CBaseAnimating;
class CBaseCombatCharacter;
class CCSPlayer;
class CWeaponCSBase;

using namespace ::ks::reflect;

// Purpose: Client side CHostage class

// for shared code

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class CHostageCarriableProp : public CBaseAnimating
{
	[[= ks::reflect::NetTable{ .name = "DT_HostageCarriableProp" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CHostageCarriableProp, CBaseAnimating );
	DECLARE_CLIENTCLASS();

	CHostageCarriableProp();

	virtual void ClientThink( void );
	void OnDataChanged( DataUpdateType_t updateType );
	virtual bool					ShouldDraw();
	virtual ShadowType_t			ShadowCastType();

	bool	m_bCreatedViewmodel;
	float	m_flFadeInStartTime;
};

//----------------------------------------------------------------------------------------------
/**
 * The client-side implementation of the Hostage
 */
class CCHostage : public CBaseCombatCharacter, public ICSPlayerAnimStateHelpers
{
	[[= ks::reflect::NetTable{ .name = "DT_CHostage" } ]]
	[[= ks::reflect::From<"m_iHealth", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_lifeState", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_fFlags", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CCHostage, CBaseCombatCharacter );
	DECLARE_CLIENTCLASS();

	CCHostage();
	virtual ~CCHostage();

// ICSPlayerAnimState overrides.
public:
	virtual CWeaponCSBase* CSAnim_GetActiveWeapon();
	virtual bool CSAnim_CanMove();
	virtual void EstimateAbsVelocity( Vector &vel );

public:	
	virtual void Spawn( void );
	virtual void UpdateClientSideAnimation();

	const char *GetCustomHostageNameForMap( const char *szMapName );

	void OnPreDataChanged( DataUpdateType_t updateType );
	void OnDataChanged( DataUpdateType_t updateType );

	int GetHostageState( void ) { return m_nHostageState; }

	bool IsRescued( void ) { return m_isRescued; }
	bool WasRecentlyKilledOrRescued( void );

	int GetHealth( void ) const { return m_iHealth; }
	int GetMaxHealth( void ) const { return m_iMaxHealth; }

	virtual void ClientThink( void );

	CCSPlayer *GetLeader( void ) const;			// return who we are following or NULL

	virtual CBaseAnimating * BecomeRagdollOnClient();
	virtual bool ShouldDraw( void );

	void ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName );

	void SetClientSideHolidayHatAddon( bool bEnable );
	static void SetClientSideHoldayHatAddonForAllHostagesAndTheirRagdolls( bool bEnable );

private:
	int  m_OldLifestate;
	[[= ks::reflect::Net{} ]] int  m_iMaxHealth;

	IPlayerAnimState *m_PlayerAnimState;

	// Declared ahead of the members that name them: an attribute-specifier is not a
	// complete-class context, so a Proxy<> annotation can only name a prior declaration.
	static void RecvProxy_Rescued( const CRecvProxyData *pData, void *pStruct, void *pOut );
	static void RecvProxy_Jumped( const CRecvProxyData *pData, void *pStruct, void *pOut );

	CNetworkVar( EHANDLE, m_leader, [[= ks::reflect::Net{} ]] );				// who we are following, or NULL
	
	CNetworkVar( Vector, m_vel, [[= ks::reflect::Net{} ]] );

	CNetworkVar( bool, m_isRescued, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int } ]]
	                    [[= ks::reflect::Proxy<RecvProxy_Rescued, ks::reflect::WireSide::Recv>{} ]] );
	CNetworkVar( bool, m_jumpedThisFrame, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int } ]]
	                    [[= ks::reflect::Proxy<RecvProxy_Jumped, ks::reflect::WireSide::Recv>{} ]] );

	CNetworkVar( int, m_nHostageState, [[= ks::reflect::Net{} ]] );

	CNetworkVar( float, m_flRescueStartTime, [[= ks::reflect::Net{} ]] );
	CNetworkVar( float, m_flGrabSuccessTime, [[= ks::reflect::Net{} ]] );		//What time did the grabbing succeed?
	CNetworkVar( float, m_flDropStartTime, [[= ks::reflect::Net{} ]] );		//What time did the grabbing succeed?

	float m_flDeadOrRescuedTime;

	CountdownTimer m_blinkTimer;

	Vector m_lookAt;		// point in space we are looking at
	void UpdateLookAt( CStudioHdr *pStudioHdr );	// orient head and eyes towards m_lookAt
	void LookAround( void );										// look around at various interesting things
	CountdownTimer m_lookAroundTimer;

	bool m_isInit;
	void Initialize( void );						// set up attachment and pose param indices

	int m_eyeAttachment;
	int m_chestAttachment;

	int m_bodyYawPoseParam;
	float m_bodyYawMin;
	float m_bodyYawMax;

	int m_headYawPoseParam;
	float m_headYawMin;
	float m_headYawMax;
	float m_flCurrentHeadYaw;
	float m_flLastBodyYaw;

	int m_headPitchPoseParam;
	float m_headPitchMin;
	float m_headPitchMax;
	float m_flCurrentHeadPitch;

	bool m_createdLowViolenceRagdoll;

	CHandle<CBaseAnimating> m_hRagdollOnClient;
	CHandle<CBaseAnimating> m_hHolidayHatAddon;
	
private:
	CCHostage( const CCHostage & );				// not defined, not accessible
};

extern std::vector< CCHostage* > g_Hostages;
extern std::vector< CHandle<CBaseAnimating> > g_HostageRagdolls;

class CBaseHudChat;

//--------------------------------------------------------------------------------------------------------------
class CHudChatLine : public CBaseHudChatLine
{
};

//-----------------------------------------------------------------------------
// Purpose: The prompt and text entry area for chat messages
//-----------------------------------------------------------------------------
class CHudChatInputLine : public CBaseHudChatInputLine
{
};

class CHudChat : public CBaseHudChat
{
	typedef CBaseHudChat BaseClass;

public:
	explicit CHudChat( const char *pElementName );

	virtual void	CreateChatInputLine( void );
	virtual void	CreateChatLines( void );

	virtual void	Init( void );
	virtual void	Reset( void );

	[[gnu::format(printf, 4, 5)]] virtual void	ChatPrintf(int iPlayerIndex, int iFilter, const char* fmt, ...);

	bool			MsgFunc_SayText2( const ks::net::CCSUsrMsg_SayText2 &msg );
	bool			MsgFunc_RadioText( const ks::net::CCSUsrMsg_RadioText &msg );
	bool			MsgFunc_RawAudio( const ks::net::CCSUsrMsg_RawAudio &msg );

	int				GetChatInputOffset( void );

	virtual Color	GetTextColorForClient( TextColor colorNum, int clientIndex );
	virtual Color	GetClientColor( int clientIndex );

	virtual int GetFilterForString( const char *pString );
};

class CBaseHudWeaponSelection;
class CBaseCombatWeapon;
class CBasePlayer;


enum
{
	WEPSELECT_PICKUP = 0,
	WEPSELECT_DROP,
	WEPSELECT_SWITCH,
};

//-----------------------------------------------------------------------------
// Purpose: cstrike weapon selection hud element
//-----------------------------------------------------------------------------
class CHudWeaponSelection : public CBaseHudWeaponSelection
{
	DECLARE_CLASS( CHudWeaponSelection, CBaseHudWeaponSelection );

public:
	explicit CHudWeaponSelection(const char *pElementName );

	virtual bool ShouldDraw();
	virtual void OnWeaponPickup( CBaseCombatWeapon *pWeapon );
	virtual void OnWeaponDrop( CBaseCombatWeapon *pWeapon );
	virtual void OnWeaponSwitch( CBaseCombatWeapon *pWeapon );

	virtual void CycleToNextWeapon( void );
	virtual void CycleToPrevWeapon( void );
	virtual void SwitchToLastWeapon( void );

	typedef enum
	{
		WEAPON_SELECTION_NORMAL = 0,
		WEAPON_SELECTION_MELEE,
		WEAPON_SELECTION_GRENADE_AND_BOMB,
		WEAPON_SELECTION_GRENADE_AND_BOMB_AND_MELEE,
		WEAPON_SELECTION_NO_GRENADE_AND_BOMB,
		WEAPON_SELECTION_GRENADE,
		WEAPON_SELECTION_ITEMSLOT,
	} WEAPON_SELECTION_MODE;

	void CycleToNextWeapon(WEAPON_SELECTION_MODE selectionMode);
	virtual void CycleToNextGrenadeOrBomb( void );
	virtual void CycleToNextGrenadeBombOrMelee( void );
	virtual void CycleToNextNonGrenadeOrBomb( void );
	virtual void UserCmd_Slot3( void );
	virtual void UserCmd_Slot4( void );
	virtual void UserCmd_Slot5( void );
	virtual void UserCmd_Slot6( void );
	virtual void UserCmd_Slot7( void );
	virtual void UserCmd_Slot8( void );
	virtual void UserCmd_Slot9( void );
	virtual void UserCmd_Slot10( void );
	virtual void UserCmd_Slot11( void );
	virtual void UserCmd_GamePadSlot1( void );
	virtual void UserCmd_GamePadSlot2( void );
	virtual void UserCmd_GamePadSlot3( void );
	virtual void UserCmd_GamePadSlot4( void );
	virtual void UserCmd_GamePadSlot5( void );
	virtual void UserCmd_GamePadSlot6( void );

	virtual CBaseCombatWeapon *GetWeaponInSlot( int iSlot, int iSlotPos );
	virtual CBaseCombatWeapon *GetWeaponInSlotForTarget( CBasePlayer *player, int iSlot, int iSlotPos );
	virtual void SelectWeaponSlot( int iSlot );
	virtual void SelectWeapon( void );

	virtual CBaseCombatWeapon	*GetSelectedWeapon( void )
	{ 
		return static_cast< CBaseCombatWeapon* >( m_hSelectedWeapon.Get() );
	}

	virtual void OpenSelection( void );
	virtual void HideSelection( void );

	virtual void CancelWeaponSelection( void );

	virtual void LevelInit();

	CBaseCombatWeapon *FindNextWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition, WEAPON_SELECTION_MODE selectionMode=WEAPON_SELECTION_NORMAL);
	//CBaseCombatWeapon *FindNextWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition);
	CBaseCombatWeapon *FindPrevWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition);

protected:
	virtual bool IsWeaponSelectable();

	virtual bool IsHudMenuTakingInput();
	virtual bool IsHudMenuPreventingWeaponSelection();

private:
	void SelectSpecificWeapon( CSWeaponID weaponID );

	virtual	void SetSelectedWeapon( CBaseCombatWeapon *pWeapon );

	// Weapon-switch behavior settings. Defaults match the former panel-animation
	// defaults ("6" / "1").
	int  m_iMaxSlots;
	bool m_bPlaySelectionSounds;
};

class CEconItemView;


// Purpose: Container that allows client & server access to data in player inventories & loadouts

inline constexpr auto LOADOUT_SLOT_USE_BASE_ITEM = 0;

//===============================================================================================================
// POSITION HANDLING
//===============================================================================================================
// Items at this position have been found, but not presented to the player
inline constexpr auto BACKEND_POSITION_FOR_NEW_ITEM = 0;

// We store a bag index in the highbits of the inventory position.
// The lowbit stores the position of the item within the bag.
enum inventory_bags_t
{
	BAG_ALL_ITEMS = 0,
	BAG_BACKPACK = 1,

	BAG_TEAM_BASE = 1000,
};

inline constexpr auto MAX_BACKPACK_SLOTS = 50;

inline uint32 GetBackendPositionFor( int iBag, int iSlot )
{
	uint32 iPos = iSlot | (iBag << 16);
	return iPos;
}
inline int ExtractBagFromBackendPosition( uint32 iBackendPosition )
{
	int iHighbits = iBackendPosition >> 16;
	return iHighbits;
}
inline int ExtractBagSlotFromBackendPosition( uint32 iBackendPosition )
{
	int iLowbits = iBackendPosition & 0xFFFF;
	return iLowbits;
}

/*
inline int GetClassFromLoadoutBag( int iBag )
{
	return (iBag - BAG_CLASS_LOADOUT_SCOUT + 1);
}
inline inventory_bags_t GetLoadoutBagForClass( int iClass )
{
	return (inventory_bags_t)(BAG_CLASS_LOADOUT_SCOUT + iClass - 1);
}
*/

//===============================================================================================================
//-----------------------------------------------------------------------------
// Purpose: A single CS player's inventory. 
//		On the client, the inventory manager contains contains an instance of this for the local player.
//		On the server, each player contains an instance of this.
//-----------------------------------------------------------------------------
class CCSPlayerInventory : public CPlayerInventory
{
	DECLARE_CLASS( CCSPlayerInventory, CPlayerInventory );
public:
	CCSPlayerInventory();

		void				SetBag( int iBag ) { m_iBag = iBag; }
		int					GetBag( void ) { return m_iBag; }

	// Returns true if the specified item is allowed to be stored at the specified position
	// DOES NOT check to make sure the position is clear. It's just a class / slot validity check.
	virtual bool		IsItemAllowedInPosition( CScriptCreatedItem *pItem, int iPosition ) { return true; }

	// Used to reject items on the backend for inclusion into this inventory.
	// Mostly used for division of bags into different in-game inventories.
	virtual bool		ItemShouldBeIncluded( int iItemPosition );

	// Stub for attribute value lookup
	float				FindInventoryItemWithMaxAttributeValue( const char *szItemType, const char *szAttrClass ) { return -1.0f; }

	// Stub for inventory item lookup
	CEconItemView		*GetInventoryItemByItemID( uint64 itemID ) { return nullptr; }

	// Stub for active quest
	uint32				GetActiveQuestID() const { return 0; }

	// Stub for loadout item lookup
	CEconItemView		*GetItemInLoadout( int iTeam, int iSlot ) { return nullptr; }
	CEconItemView		*GetItemInLoadoutFilteredByProhibition( int iTeam, int iSlot ) { return nullptr; }

protected:
	// Item movement functions that are called by the inventory manager
	// Updates the backpack or loadout for the specified item
	// Returns false if the item isn't allowed to be placed in the specified position.
	virtual bool		MoveItemToPosition( globalindex_t iGlobalIndex, int iPosition );

	// Tries to move the specified item into the player's backpack. If this inventory isn't the
	// backpack, it'll find the matching player's backpack and move it. 
	// FAILS if the backpack is full. Returns false in that case.
	virtual bool		MoveItemToBackpack( globalindex_t iGlobalIndex );

	// Inventory updating, called by the Inventory Manager only.
	virtual void		InventoryReceived( EItemRequestResult eResult, int iItems );

	// Debugging
	virtual void		DumpInventoryToConsole( bool bRoot );

protected:
	// The bag position of this inventory, if any (0 == contains all items)
	// If this inventory has a bag position, items inside other bags will be ignored.
	int			m_iBag;

	friend class CCSInventoryManager;
// 	friend class CTFPlayerInventoryLoadout;
};

/*
//-----------------------------------------------------------------------------
// Purpose: Custom TF inventory that manages the player's loadout
//-----------------------------------------------------------------------------
class CTFPlayerInventoryLoadout : public CCSPlayerInventory
{
	DECLARE_CLASS( CTFPlayerInventoryLoadout, CCSPlayerInventory );
public:
	CTFPlayerInventoryLoadout( void );

	// Returns the item in the specified loadout slot for a given class
	CScriptCreatedItem	*GetItemInLoadout( int iSlot );

	// Get the class that this loadout inventory is for
	int					GetLoadoutClass( void ) { return GetClassFromLoadoutBag(m_iBag); }

	// Returns true if the specified item is allowed to be stored at the specified position
	// DOES NOT check to make sure the position is clear. It's just a class / slot validity check.
	bool				IsItemAllowedInPosition( CScriptCreatedItem *pItem, int iPosition );

	// Debugging
	virtual void		DumpInventoryToConsole( bool bRoot );

protected:
	// Item movement functions that are called by the inventory manager
#ifdef CLIENT_DLL
	// Updates the backpack or loadout for the specified item
	// Returns false if the item isn't allowed to be placed in the specified position.
	virtual bool		MoveItemToPosition( globalindex_t iGlobalIndex, int iPosition );

	// Removes any item in a loadout slot. If the slot has a base item,
	// the player essentially returns to using that item.
	// NOTE: This can fail if the player has no backpack space to contain the equipped item.
	bool				ClearLoadoutSlot( int iSlot );
#endif

protected:
	// Inventory updating, called by the Inventory Manager only.
	virtual void		InventoryReceived( EItemRequestResult eResult, int iItems );

	// Derived inventory hooks
	virtual void		ItemHasBeenUpdated( CScriptCreatedItem *pItem );
	virtual void		ItemIsBeingRemoved( CScriptCreatedItem *pItem );

private:
	// Global indices of the items in our inventory in the loadout slots
	globalindex_t		m_LoadoutItems[ LOADOUT_POSITION_COUNT ];

	friend class CCSInventoryManager;
};

//-----------------------------------------------------------------------------
// Purpose: Custom TF inventory that manages the player's loadout
//-----------------------------------------------------------------------------
class CTFPlayerInventoryBackpack : public CCSPlayerInventory
{
	DECLARE_CLASS( CTFPlayerInventoryBackpack, CCSPlayerInventory );
public:
	CTFPlayerInventoryBackpack( void );

#ifdef CLIENT_DLL
	// Tries to move the specified item into the player's backpack. 
	// FAILS if the backpack is full. Returns false in that case.
	virtual bool		MoveItemToBackpack( globalindex_t iGlobalIndex );

protected:
	virtual void		ItemHasBeenUpdated( CScriptCreatedItem *pItem );
	void				CalculateNextEmptySlot( void );
#endif

private:
	int			m_iPredictedEmptySlot;
};
*/

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCSInventoryManager : public CInventoryManager
{
	DECLARE_CLASS( CCSInventoryManager, CInventoryManager );
public:
	CCSInventoryManager();

	virtual void		PostInit( void );

	// Show the player a pickup screen with any items they've collected recently, if any
	bool				ShowItemsPickedUp( void );
	// Force the player to discard an item to make room for a new item, if they have one
	bool				CheckForRoomAndForceDiscard( void );

	// Returns the item data for the base item in the loadout slot for a given class
	CScriptCreatedItem	*GetBaseItemForClass( int iSlot );
	CEconItemView	*GetBaseItemForTeam( int iTeam, int iSlot ) { return nullptr; }
	void				GenerateBaseItems( void );

	// Gets the specified inventory for the player ID
	CCSPlayerInventory	*GetBagForPlayer( CPlayerId &playerID, inventory_bags_t iBag );

	// Returns the item in the specified loadout slot for a given class
	CScriptCreatedItem	*GetItemInLoadoutForClass( int iClass, int iSlot, CPlayerId *pID = nullptr );
	CEconItemView		*GetItemInLoadoutForTeam( int iTeam, int iSlot, CPlayerId *pID = nullptr ) { return nullptr; }

	// Stub for finding or creating reference items
	CEconItemView		*FindOrCreateReferenceEconItem( uint64 itemID ) { return nullptr; }

	// Stub for inventory lookup by player
	CCSPlayerInventory	*GetInventoryForPlayer( const CPlayerId& playerUID ) { return nullptr; }

private:
	// Base items, returned for slots that the player doesn't have anything in
	CScriptCreatedItem m_pBaseLoadoutItems[ LOADOUT_POSITION_COUNT ];

	// On the client, we have a single inventory for the local player. Stored here, instead of in the
	// local player entity, because players need to access it while not being connected to a server.
public:
	virtual void		UpdateLocalInventory( void );
	CPlayerInventory	*GetLocalInventory( void ) { return &m_LocalInventory; }
	CCSPlayerInventory	*GetLocalTFInventory( void );
	CCSPlayerInventory	*GetLocalCSInventory( void ) { return &m_LocalInventory; }

	// Try and equip the specified item in the specified class's loadout slot
	bool				EquipItemInLoadout( int iClass, int iSlot, globalindex_t iGlobalIndex );

	// Fills out pList with all inventory items that could fit into the specified loadout slot for a given class
	int					GetAllUsableItemsForSlot( int iClass, int iSlot, std::vector< CScriptCreatedItem* > *pList );
private:
	CCSPlayerInventory			m_LocalInventory;
/*
	CTFPlayerInventoryBackpack	m_LocalBackpack;
	CTFPlayerInventoryLoadout	m_LoadoutInventories[TF_CLASS_COUNT];
*/
};

CCSInventoryManager *CSInventoryManager( void );

class CCSWeaponInfo;
class CEconItemDefinition;
class CIronSightController;
class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CBaseViewModel;
class CCSPlayer;
class CEconItemView;

using namespace ::ks::reflect;


// extern int  ClassnameToWeaponID( const char *classname );
extern CSWeaponID AliasToWeaponID( const char *alias );
extern const char *WeaponIDToAlias( int id );
extern const char *GetTranslatedWeaponAlias( const char *alias);
extern const char * GetWeaponAliasFromTranslated(const char *translatedAlias);
extern bool	IsPrimaryWeapon( CSWeaponID id );
extern bool IsSecondaryWeapon( CSWeaponID  id );
extern bool IsGrenadeWeapon( CSWeaponID id );
extern bool IsArmor( CSWeaponID id );
extern int GetShellForAmmoType( const char *ammoname );

inline constexpr const char *SHIELD_VIEW_MODEL = "models/weapons/v_shield.mdl";
inline constexpr const char *SHIELD_WORLD_MODEL = "models/weapons/w_shield.mdl";


// These are the names of the ammo types that go in the CAmmoDefs and that the 
// weapon script files reference.
inline constexpr const char *BULLET_PLAYER_50AE = "BULLET_PLAYER_50AE";
inline constexpr const char *BULLET_PLAYER_762MM = "BULLET_PLAYER_762MM";
inline constexpr const char *BULLET_PLAYER_556MM = "BULLET_PLAYER_556MM";
inline constexpr const char *BULLET_PLAYER_556MM_SMALL = "BULLET_PLAYER_556MM_SMALL";
inline constexpr const char *BULLET_PLAYER_556MM_BOX = "BULLET_PLAYER_556MM_BOX";
inline constexpr const char *BULLET_PLAYER_338MAG = "BULLET_PLAYER_338MAG";
inline constexpr const char *BULLET_PLAYER_9MM = "BULLET_PLAYER_9MM";
inline constexpr const char *BULLET_PLAYER_BUCKSHOT = "BULLET_PLAYER_BUCKSHOT";
inline constexpr const char *BULLET_PLAYER_45ACP = "BULLET_PLAYER_45ACP";
inline constexpr const char *BULLET_PLAYER_357SIG = "BULLET_PLAYER_357SIG";
inline constexpr const char *BULLET_PLAYER_357SIG_P250 = "BULLET_PLAYER_357SIG_P250";
inline constexpr const char *BULLET_PLAYER_357SIG_SMALL = "BULLET_PLAYER_357SIG_SMALL";
inline constexpr const char *BULLET_PLAYER_357SIG_MIN = "BULLET_PLAYER_357SIG_MIN";
inline constexpr const char *BULLET_PLAYER_57MM = "BULLET_PLAYER_57MM";
inline constexpr const char *AMMO_TYPE_HEGRENADE = "AMMO_TYPE_HEGRENADE";
inline constexpr const char *AMMO_TYPE_FLASHBANG = "AMMO_TYPE_FLASHBANG";
inline constexpr const char *AMMO_TYPE_SMOKEGRENADE = "AMMO_TYPE_SMOKEGRENADE";
inline constexpr const char *AMMO_TYPE_DECOY = "AMMO_TYPE_DECOY";
inline constexpr const char *AMMO_TYPE_MOLOTOV = "AMMO_TYPE_MOLOTOV";
inline constexpr const char *AMMO_TYPE_TASERCHARGE = "AMMO_TYPE_TASERCHARGE";
inline constexpr const char *AMMO_TYPE_HEALTHSHOT = "AMMO_TYPE_HEALTHSHOT";
inline constexpr const char *AMMO_TYPE_TAGRENADE = "AMMO_TYPE_TAGRENADE";

inline constexpr auto CROSSHAIR_CONTRACT_PIXELS_PER_SECOND = 7.0f;

// Given an ammo type (like from a weapon's GetPrimaryAmmoType()), this compares it
// against the ammo name you specify.
// MIKETODO: this should use indexing instead of searching and strcmp()'ing all the time.
bool IsAmmoType( int iAmmoType, const char *pAmmoName );

enum CSWeaponMode
{
	Primary_Mode = 0,
	Secondary_Mode,
	WeaponMode_MAX
};

// structure to encapsulate state of head bob
struct BobState_t
{
	BobState_t();

	float m_flBobTime;
	float m_flLastBobTime;
	float m_flLastSpeed;
	float m_flVerticalBob;
	float m_flLateralBob;
	float m_flRawVerticalBob;
	float m_flRawLateralBob;
};

float CalcViewModelBobHelper( CBasePlayer *player, BobState_t *pBobState, int nVMIndex = 0 );
void AddViewModelBobHelper( Vector &origin, QAngle &angles, BobState_t *pBobState );

class CWeaponCSBase : public CBaseCombatWeapon
{
	[[= ks::reflect::NetTable{ .name = "DT_WeaponCSBase" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_AnimTimeMustBeFirst", .prop = "m_flAnimTime" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseAnimating", .prop = "m_nSequence" } ]]
	[[= ks::reflect::PredFrom<"m_flTimeWeaponIdle", ks::reflect::Pred{ .flags = FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK }>{} ]]
	[[= ks::reflect::PredFrom<"m_flNextPrimaryAttack", ks::reflect::Pred{ .flags = FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK }>{} ]]
	[[= ks::reflect::PredFrom<"m_flNextSecondaryAttack", ks::reflect::Pred{ .flags = FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK }>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CWeaponCSBase, CBaseCombatWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CWeaponCSBase();
	virtual ~CWeaponCSBase();

	virtual int GetShotgunReloadState( void ) { return 0; } // only shotguns use this for multi-stage reloads

	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void	AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles );
	virtual	float	CalcViewmodelBob( void );
	BobState_t		*GetBobState();
	// All predicted weapons need to implement and return true
	virtual bool	IsPredicted() const;

 	bool			IsPistol() const;

	virtual int		GetWeaponPrice() const { return GetCSWpnData().										GetWeaponPrice( GetEconItemView() ); }
	virtual bool	IsFullAuto() const { return GetCSWpnData().											IsFullAuto( GetEconItemView() ); }
	virtual int		GetDamage() const { return GetCSWpnData().											GetDamage( GetEconItemView() ); }
	virtual int		GetKillAward() const { return GetCSWpnData().										GetKillAward( GetEconItemView() ); }
	virtual float	GetCycleTime( int mode = Primary_Mode ) const { return GetCSWpnData().				GetCycleTime( GetEconItemView(), mode ); }
	virtual float	GetArmorRatio() const { return GetCSWpnData().										GetArmorRatio( GetEconItemView() ); }
	virtual bool	HasTraditionalScope() const { return GetCSWpnData().								HasTraditionalScope( GetEconItemView() ); }
	virtual float	GetInaccuracyStand( int mode = Primary_Mode ) const { return GetCSWpnData().		GetInaccuracyStand( GetEconItemView(), mode ); }
	virtual float	GetInaccuracyCrouch( int mode = Primary_Mode ) const { return GetCSWpnData().		GetInaccuracyCrouch( GetEconItemView(), mode ); }

	virtual bool	CannotShootUnderwater() const { return GetCSWpnData().								CannotShootUnderwater( GetEconItemView() ); }

	virtual int		GetRecoilMagnitude( int mode ) const { return GetCSWpnData().						GetRecoilMagnitude( GetEconItemView(), mode ); }
	virtual int		GetRecoilMagnitudeVariance( int mode ) const { return GetCSWpnData().				GetRecoilMagnitudeVariance( GetEconItemView(), mode ); }
	virtual int		GetRecoilAngle( int mode ) const { return GetCSWpnData().							GetRecoilAngle( GetEconItemView(), mode ); }
	virtual int		GetRecoilAngleVariance( int mode ) const { return GetCSWpnData().					GetRecoilAngleVariance( GetEconItemView(), mode ); }

	virtual float	GetMaxSpeed() const { return GetCSWpnData().										GetMaxSpeed( GetEconItemView(), m_weaponMode ); }	// What's the player's max speed while holding this weapon.

	virtual int		GetZoomLevels() const { return GetCSWpnData().										GetZoomLevels( GetEconItemView() ); }

	virtual bool		CanBeUsedWithShield() const	{ return GetCSWpnData().							CanBeUsedWithShield(); }
	virtual const char*	GetZoomInSound() const	{ return GetCSWpnData().								GetZoomInSound(); }
	virtual const char*	GetZoomOutSound() const	{ return GetCSWpnData().								GetZoomOutSound(); }
	virtual float		GetBotAudibleRange() const { return GetCSWpnData().								GetBotAudibleRange(); }
	virtual const char* GetWrongTeamMsg() const { return GetCSWpnData().								GetWrongTeamMsg(); }
	virtual const char* GetAnimExtension() const { return GetCSWpnData().								GetAnimExtension(); }
	virtual const char* GetShieldViewModel() const { return GetCSWpnData().								GetShieldViewModel(); }
	virtual const char* GetSilencerModel() const { return GetCSWpnData().								GetSilencerModel(); }
	virtual float		GetAddonScale() const { return GetCSWpnData().									GetAddonScale(); }
	virtual float		GetThrowVelocity() const { return GetCSWpnData().								GetThrowVelocity(); }

	virtual int		GetZoomFOV( int nZoomLevel ) const;
	virtual float	GetZoomTime( int nZoomLevel ) const;

	virtual CSWeaponType GetWeaponType( void ) const { return GetCSWpnData().GetWeaponType( GetEconItemView() ); }
	virtual const char	*GetDefinitionName( void ) const { return GetEconItemView()->GetStaticData()->GetDefinitionName(); }

	virtual const CEconItemView* GetEconItemView( void ) const override { return &m_EconItemView; }
	virtual CEconItemView* GetEconItemView( void ) override { return &m_EconItemView; }
	void InitializeEconItemView( const char *pszItemName );
	void SetEconItemDefinition( CEconItemDefinition *pDef );

	CCSPlayer* GetPlayerOwner() const;
	CBaseEntity *GetWeaponForEffect();
	void UpdateOutlineGlow( void );
	CGlowObject m_GlowObject;

	virtual int GetRecoilSeed( void ) const;

	// Get CS-specific weapon data.
	virtual CCSWeaponInfo const	&GetCSWpnData() const;
	virtual int GetCSZoomLevel() { return 0; }

	// Get specific CS weapon ID (ie: WEAPON_AK47, etc)
	virtual CSWeaponID GetCSWeaponID( void ) const { return m_nWeaponID; }

	// return true if this weapon is an instance of the given weapon type (ie: "IsA" WEAPON_GLOCK)
	bool IsA( CSWeaponID id ) const						{ return GetCSWeaponID() == id; }

	// return true if this weapon is a kinf of the given weapon type (ie: "IsKindOf" WEAPONTYPE_RIFLE )
	bool IsKindOf( CSWeaponType type ) const			{ return GetCSWpnData().GetWeaponType() == type; }

	const char		*GetTracerType( void ) { return GetCSWpnData().GetTracerEffectName( GetEconItemView() ); }

	loadout_positions_t GetDefaultLoadoutSlot( void ) { return (loadout_positions_t)( GetEconItemView() ? GetEconItemView()->GetItemDefinition()->GetLoadoutSlot( 0 ) : 0 ); }

	// return true if this weapon has a silencer equipped
	virtual bool IsSilenced( void ) const				{ return m_bSilencerOn; }
	// return true is this weapon is capable of being silenced
	// TODO: allow weapons to define this in the weapon script file and have it live in WpnData
	virtual bool HasSilencer( void ) const				
	{ 
		return GetCSWpnData().HasSilencer( GetEconItemView() ); 
	}

	virtual void SetWeaponModelIndex( const char *pName );
	virtual void OnPickedUp( CBaseCombatCharacter *pNewOwner );

	virtual void OnJump( float fImpulse );
	virtual void OnLand( float fVelocity );

	float GetRecoveryTime( void );

	virtual bool	HasZoom() { return false; }
	void			CallSecondaryAttack();

public:

		virtual void	ProcessMuzzleFlashEvent();
		virtual bool	OnFireEvent( CBaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options );
		virtual bool	ShouldPredict();
		virtual void	DrawCrosshair();
		virtual void	OnDataChanged( DataUpdateType_t type );

		virtual int		GetMuzzleAttachmentIndex_1stPerson( CBaseViewModel *pViewModel );
		virtual int		GetMuzzleAttachmentIndex_3rdPerson( void );
		virtual int		GetEjectBrassAttachmentIndex_1stPerson( CBaseViewModel *pViewModel );
		virtual int		GetEjectBrassAttachmentIndex_3rdPerson( void );

		virtual bool	DoesHideViewModelWhenZoomed( void ) { return GetCSWpnData().					DoesHideViewModelWhenZoomed( GetEconItemView() ); }

		float			m_flCrosshairDistance;
		int				m_iAmmoLastCheck;
		int				m_iAlpha;
		int				m_iScopeTextureID;
		int				m_iCrosshairTextureID; // for white additive texture
		float			m_flGunAccuracyPosition;

		virtual const char* GetMuzzleFlashEffectName_1stPerson( void );
		virtual const char* GetMuzzleFlashEffectName_3rdPerson( void );
		virtual const char* GetEjectBrassEffectName( void );
		virtual const char* GetHeatEffectName( void );

		int GetReticleWeaponSpread( void );
		int GetReticleCrosshairGap( void );
		bool WantReticleShown( void );

	bool IsUseable();
	virtual bool	CanDeploy( void );
	virtual void	UpdateShieldState( void );
	virtual bool	SendWeaponAnim( int iActivity );
	virtual void	SendViewModelAnim( int nSequence );
	virtual void	SecondaryAttack( void );
	virtual bool	CanPrimaryAttack( void ) {return true;}
	virtual void	Precache( void );
	virtual bool	CanBeSelected( void );
	virtual Activity GetDeployActivity( void );
	virtual bool	DefaultDeploy( char *szViewModel, char *szWeaponModel, int iActivity, char *szAnimExt );
	virtual void 	DefaultTouch( CBaseEntity *pOther );	// default weapon touch
	virtual bool	DefaultPistolReload();

	virtual bool	Deploy();
	virtual void	Drop( const Vector &vecVelocity );
	bool PlayEmptySound();
	virtual const char *GetShootSound( int iIndex ) const;
	virtual const char *GetPlayerAnimationExtension( void ) const;
	virtual const char *GetAddonModel( void ) const;
	virtual void	ItemPostFrame();
	virtual void	ItemBusyFrame();
	virtual const char *GetViewModel( int viewmodelindex = 0 ) const;
	virtual void WeaponReset( void ) {}
	virtual bool	WeaponHasBurst() const { return false; }
    virtual bool	IsInBurstMode() { return m_bBurstMode; }
    //lwss - added some more that were missing
    virtual float 	GetBurstModeTimeBetweenShots() { return 0.0f; }
    virtual float 	GetBurstModeCycleTime() { return 0.0f; }
    virtual float 	GetZoomedCycleTime() { return 0.0f; }
    //lwss end
	virtual bool	IsRevolver() const { return false; }

	void			ItemPostFrame_ProcessPrimaryAttack( CCSPlayer *pPlayer );
	bool			ItemPostFrame_ProcessZoomAction( CCSPlayer *pPlayer );
	bool			ItemPostFrame_ProcessSecondaryAttack( CCSPlayer *pPlayer );
	void			ItemPostFrame_ProcessReloadAction( CCSPlayer *pPlayer );
	void			ItemPostFrame_ProcessIdleNoAction( CCSPlayer *pPlayer );

	void			ItemPostFrame_RevolverResetHaulback();

	CNetworkVar( CSWeaponMode, m_weaponMode, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );

	virtual float GetInaccuracy() const;
	virtual float GetSpread() const { return GetCSWpnData().GetSpread( GetEconItemView(), m_weaponMode.Get() ); }

	virtual void UpdateAccuracyPenalty();

	CNetworkVar( float, m_fAccuracyPenalty, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_CHANGES_OFTEN } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = TD_MSECTOLERANCE } ]] );
	float m_fAccuracySmoothedForZoom;
	float m_fScopeZoomEndTime;
	CNetworkVar( int, m_iRecoilIndex, [[= ks::reflect::Net{ .side = ks::reflect::WireSide::Recv } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );	// DEPRECATED. Kept for old demo compatibility.
	CNetworkVar( float, m_flRecoilIndex, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( bool, m_bBurstMode, [[= ks::reflect::Net{} ]] );

	CNetworkVar( float, m_flPostponeFireReadyTime, [[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK } ]] );
	void ResetPostponeFireReadyTime( void ) { m_flPostponeFireReadyTime = std::numeric_limits<float>::max(); }
	void SetPostponeFireReadyTime( float flFutureTime ) { m_flPostponeFireReadyTime = flFutureTime; }
	bool IsPostponFireReadyTimeElapsed( void ) { return (m_flPostponeFireReadyTime < gpGlobals->curtime); }

	virtual bool IsReloadVisuallyComplete() {return m_bReloadVisuallyComplete; }
	CNetworkVar( bool, m_bReloadVisuallyComplete, [[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );

	CNetworkVar( bool, m_bSilencerOn, [[= ks::reflect::Net{} ]] );
	CNetworkVar( float, m_flDoneSwitchingSilencer, [[= ks::reflect::Net{} ]] );	// soonest time switching the silencer will be complete
	CNetworkVar( float, m_flDroppedAtTime ); // when was this weapon last dropped

	bool IsSwitchingSilencer( void ) { return ( m_flDoneSwitchingSilencer >= gpGlobals->curtime ); }

	void SetExtraAmmoCount( int count ) { m_iExtraPrimaryAmmo = count; }
	int GetExtraAmmoCount( void ) const { return m_iExtraPrimaryAmmo; }

	void SetPreviousOwner( CCSPlayer* player ) { m_hPrevOwner = ( CBasePlayer * )player; }
	CCSPlayer* GetPreviousOwner() const { return ( CCSPlayer* )m_hPrevOwner.Get(); }

    // [tj] Accessors for the donor system
	void SetDonor( CCSPlayer* player ) { m_donor.Set( player ); }
    CCSPlayer* GetDonor() const { return m_donor.Get(); }
    void SetDonated(bool donated) { m_donated = true;}
    bool GetDonated() const { return m_donated; }

    //[dwenger] Accessors for the prior owner list
	void AddToPriorOwnerList( CCSPlayer* pPlayer );
	bool IsAPriorOwner( CCSPlayer* pPlayer ) const;

	CCSPlayer * GetOriginalOwner();

	// Xuid methods stub (econ removed)
	virtual uint64 GetOriginalOwnerXuid() const override { return 0; }
	virtual void SetOriginalOwnerXuid( uint32, uint32 ) override {}

	CNetworkVar( int, m_iOriginalTeamNumber, [[= ks::reflect::Net{ .bits = -1 } ]] );

	int GetOriginalTeamNumber()	{ return m_iOriginalTeamNumber; }

	void AddPriorOwner( CCSPlayer* pPlayer );
	bool WasOwnedByTeam( int teamNumber );
	bool CanBePickedUp( void ) { return m_bCanBePickedUp; }

	virtual void SaveCustomMaterialsTextures( void );

protected:

	float CalculateNextAttackTime( float flCycleTime );
	void Recoil( CSWeaponMode weaponMode );

	[[= ks::reflect::Key{ .name = "CanBePickedUp" } ]] bool m_bCanBePickedUp;

private:
	CEconItemView m_EconItemView;
	CNetworkVar( int, m_nEconItemDefIndex, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] );

	CWeaponCSBase( const CWeaponCSBase & );

	virtual int GetWeaponID( void ) const		{ return GetCSWeaponID(); }
	void UpdateCustomMaterial( void );
	void CheckCustomMaterial( void );
	bool m_bVisualsDataSet;
	bool m_bOldFirstPersonSpectatedState;

	CSWeaponID	m_nWeaponID;

	int		m_iExtraPrimaryAmmo;
	
	float	m_nextOwnerTouchTime;
	float	m_nextPrevOwnerTouchTime;

	CNetworkHandle( CBasePlayer, m_hPrevOwner, [[= ks::reflect::Net{} ]] );

	int m_iDefaultExtraAmmo;

	// [dwenger] track all prior owners of this weapon
	std::vector< CCSPlayer* > m_PriorOwners;

	// [tj] To keep track of people who drop weapons for teammates during the buy round
	CHandle<CCSPlayer> m_donor;
	bool m_donated;

	void ResetGunHeat( void );
	void UpdateGunHeat( float heat, int iAttachmentIndex );

	CNetworkVar( float, m_fLastShotTime, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );

	bool m_bWasOwnedByCT;
	bool m_bWasOwnedByTerrorist;

	int  GetViewModelSplitScreenSlot( CBaseViewModel *pViewModel );

	// Smoke effect variables.
	float m_gunHeat;
	unsigned int m_smokeAttachments;
	float m_lastSmokeTime;

public:

	float m_flLastClientFireBulletTime;

	CIronSightController *GetIronSightController( void );
	void				 UpdateIronSightController( void );
	CIronSightController *m_IronSightController;
	CNetworkVar( int, m_iIronSightMode, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );

};

extern ConVar weapon_recoil_decay1;
extern ConVar weapon_recoil_decay2_exp;
extern ConVar weapon_recoil_decay2_lin;
extern ConVar weapon_recoil_vel_decay;
extern ConVar weapon_recoil_extra;
extern ConVar weapon_recoil_scale;
extern ConVar weapon_recoil_scale_motion_controller;

class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CWeaponCSBase;

using namespace ::ks::reflect;

inline constexpr auto GRENADE_UNDERHAND_THRESHOLD = 0.33f;

class CBaseCSGrenade : public CWeaponCSBase
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseCSGrenade" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CBaseCSGrenade, CWeaponCSBase );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CBaseCSGrenade();

	virtual void	Precache();

	bool			Deploy();
	bool			Holster( CBaseCombatWeapon *pSwitchingTo );

	void			PrimaryAttack();
	void			SecondaryAttack();

	bool			Reload();

	virtual void	ItemPostFrame();
	virtual void	ItemPreFrame();
	
	virtual void OnPickedUp( CBaseCombatCharacter *pNewOwner );

	void			DecrementAmmo( CBaseCombatCharacter *pOwner );
	virtual void	StartGrenadeThrow();
	virtual void	ThrowGrenade();
	virtual void	DropGrenade();

	bool IsPinPulled() const;
	bool IsBeingThrown() const { return m_fThrowTime > 0; }

	bool IsLoopingSoundPlaying( void ) { return m_bLoopingSoundPlaying; }
	void SetLoopingSoundPlaying( bool bPlaying ) { m_bLoopingSoundPlaying = bPlaying; }

	// true if grenade has been thrown or dropped
	bool			GetIsThrown( void ) { return !m_bIsHeldByPlayer; }

	virtual int		DrawModel( int flags, const RenderableInstance_t &instance ) override;

	bool			IsThrownUnderhand( void ) { return (m_flThrowStrength <= GRENADE_UNDERHAND_THRESHOLD); }
	float			GetThrownStrength( void ) { return m_flThrowStrength; }
	float			ApproachThrownStrength( void );

protected:
	CNetworkVar( bool, m_bRedraw, [[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );	// Draw the weapon again after throwing a grenade
	CNetworkVar( bool, m_bIsHeldByPlayer, [[= ks::reflect::Net{} ]] );	// is true when held by player, false when it's been thrown or dropped
	CNetworkVar( bool, m_bPinPulled, [[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );	// Set to true when the pin has been pulled but the grenade hasn't been thrown yet.
	CNetworkVar( float, m_fThrowTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] ); // the time at which the grenade will be thrown.  If this value is 0 then the time hasn't been set yet.

	CNetworkVar( bool, m_bLoopingSoundPlaying, [[= ks::reflect::Net{} ]] );	// Set to true when the grenade is playing a looping sound
	CNetworkVar( float, m_flThrowStrength, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
private:
	CBaseCSGrenade( const CBaseCSGrenade & ) {}

	float m_flThrowStrengthClientSmooth;
};

class CBaseCombatWeapon;
class CCSPlayer;
class CWeaponCSBase;

using namespace ::ks::reflect;

// Purpose: base class for belt items, eg pills and adrenaline

class CWeaponBaseItem : public CWeaponCSBase
{
	[[= ks::reflect::NetTable{ .name = "DT_WeaponBaseItem" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CWeaponBaseItem, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CWeaponBaseItem();

	virtual bool HasPrimaryAmmo( void );				// Returns true if weapon has ammo
	virtual bool CanBeSelected( void );

	virtual void Spawn( void );

	virtual void PrimaryAttack( void );					// do "+ATTACK"
	virtual void SecondaryAttack( void );				// do "+ATTACK2"
//	virtual bool OnHit( trace_t &trace, const Vector &swingVector, bool firstTime );	// deals damage and plays hit effects.  returns true if this hit stops the swing.

	bool			Reload();

	virtual void ItemPostFrame( void );					// called each frame by the player PostThink
//	virtual bool CanExtendHelpingHand( void ) const;
	virtual bool CanFidget( void );

	virtual bool Deploy( void );
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo = nullptr );

	virtual void WeaponIdle( void );					// called when no buttons pressed

	virtual bool SendWeaponAnim( int iActivity );
//	virtual Activity GetWeaponDeployActivity( PlayerAnimEvent_t animEvent, Activity mainActivity ) { return ACT_DEPLOY_GREN; }

	virtual bool CanUseOnSelf( CCSPlayer *pPlayer )	{ return true; }
	virtual void OnStartUse( CCSPlayer *pPlayer ) {}

	virtual float GetUseTimerDuration( void );

private:
	CWeaponBaseItem( const CWeaponBaseItem & ) {}

	CNetworkVarEmbedded( CountdownTimer, m_UseTimer );
	CNetworkVar( bool, m_bRedraw, [[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );	// Draw the weapon again after throwing a grenade

};

class CNewParticleEffect;
class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseViewModel;
class CWeaponCSBase;

using namespace ::ks::reflect;


inline constexpr auto NUM_BEEPS = 7;

inline constexpr const char *WEAPON_C4_CLASSNAME = "weapon_c4";
inline constexpr const char *PLANTED_C4_CLASSNAME = "planted_c4";
inline constexpr const char *PLANTED_C4TRAINING_CLASSNAME = "planted_c4_training";

class CC4 : public CWeaponCSBase
{
	[[= ks::reflect::NetTable{ .name = "DT_WeaponC4" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CC4, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CC4();
	virtual ~CC4();
	
	virtual void Spawn();

	void ItemPostFrame();
	virtual void WeaponReset( void );
	virtual void PrimaryAttack();
	virtual void WeaponIdle();
	virtual void UpdateShieldState( void );
	virtual float GetMaxSpeed() const;

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_C4; }

	virtual bool			Deploy( void );								// returns true is deploy was successful
	virtual bool			Holster( CBaseCombatWeapon *pSwitchingTo = nullptr );

		void ClientThink( void );
		virtual void	OnDataChanged( DataUpdateType_t type );
		virtual void	UpdateOnRemove( void );
		virtual bool OnFireEvent( CBaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options );
		char *GetScreenText( void );
		char m_szScreenText[32];

		ks::tracked_ref<CNewParticleEffect> m_hC4LED;
		void CreateLEDEffect( void );
		void RemoveLEDEffect( void );
		EHANDLE		   m_hParticleEffectOwner;

		virtual Vector GetGlowColor( void ) { return Vector( (240.0f/255.0f), (225.0f/255.0f), (90.0f/255.0f) ); }

	void AbortBombPlant();

	void PlayArmingBeeps( void );
	void PlayPlantInitSound( void );
	virtual void	OnPickedUp( CBaseCombatCharacter *pNewOwner );
	virtual void	Drop( const Vector &vecVelocity );

	CNetworkVar( bool, m_bStartedArming, [[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( float, m_fArmedTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( bool, m_bBombPlacedAnimation, [[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( bool, m_bShowC4LED, [[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( bool, m_bIsPlantingViaUse, [[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );

	virtual bool IsRemoveable( void ) { return false; }

private:	
	bool m_bPlayedArmingBeeps[NUM_BEEPS];
	bool m_bBombPlanted;

	// [tj] we want to store if this bomb was dropped because the original owner was killed
	bool m_bDroppedFromDeath;

private:
	
	CC4( const CC4 & );
};

class CCSWeaponInfo;
class CBaseCombatWeapon;
class CWeaponCSBase;

using namespace ::ks::reflect;


// This is the base class for pistols and rifles.

class CWeaponCSBaseGun : public CWeaponCSBase
{
	[[= ks::reflect::NetTable{ .name = "DT_WeaponCSBaseGun" } ]]
	static constexpr bool ks_class = true;

public:
	
	DECLARE_CLASS( CWeaponCSBaseGun, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponCSBaseGun();

	virtual void Spawn();
	virtual void Precache();

	virtual void PrimaryAttack();
	virtual void SecondaryAttack();
	virtual bool Reload();
	virtual void WeaponIdle();
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void Drop( const Vector &vecVelocity );
	virtual bool Deploy();

	// Derived classes call this to fire a bullet.
	bool CSBaseGunFire( float flCycleTime, CSWeaponMode weaponMode );

	void BurstFireRemaining( void );

	// Usually plays the shot sound. Guns with silencers can play different sounds.
	virtual void DoFireEffects();
	virtual void ItemPostFrame();
	virtual void ItemBusyFrame( void );

	float GetFOVForAccuracy( void );

	void SetWeaponInfo( const CCSWeaponInfo* pWeaponInfo )	{ m_pWeaponInfo = pWeaponInfo; }

	// Get CS-specific weapon data.
	virtual CCSWeaponInfo const	&GetCSWpnData() const;
	virtual int GetCSZoomLevel() {return m_zoomLevel; }

	CNetworkVar( int, m_zoomLevel, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );

	virtual bool HasZoom( void );
	virtual bool IsZoomed( void ) const;

	virtual bool WeaponHasBurst( void ) const { return GetCSWpnData().HasBurstMode( GetEconItemView() ); }
	virtual bool IsInBurstMode() const;
    //lwss - added some more that were missing
    virtual float GetBurstModeTimeBetweenShots() override  { return GetCSWpnData().GetBurstModeTimeBetweenShots( GetEconItemView() ); }
    virtual float GetBurstModeCycleTime() override { return GetCSWpnData().GetBurstModeCycleTime( GetEconItemView() ); }
    virtual float GetZoomedCycleTime() override { return GetCSWpnData().GetZoomedCycleTime( GetEconItemView() ); }
    //lwss end

	virtual bool IsFullAuto() const;

	virtual bool IsRevolver( void ) const { return GetCSWpnData().IsRevolver( GetEconItemView() ); }
	virtual bool DoesUnzoomAfterShot( void ) const { return GetCSWpnData().DoesUnzoomAfterShot( GetEconItemView() ); }

	virtual bool SendWeaponAnim( int iActivity );

	CNetworkVar( int, m_iBurstShotsRemaining, [[= ks::reflect::Net{ .bits = -1 } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );	
	[[= ks::reflect::Pred{ .flags = 0 } ]] float	m_fNextBurstShot;			// time to shoot the next bullet in burst fire mode

	virtual Activity GetDeployActivity( void );

	virtual const char		*GetMuzzleFlashEffectName_1stPerson( void );
	virtual const char		*GetMuzzleFlashEffectName_3rdPerson( void );
	int m_iSilencerBodygroup;

	virtual const char		*GetWorldModel( void ) const;

private:

	const CCSWeaponInfo*	m_pWeaponInfo;

	CWeaponCSBaseGun( const CWeaponCSBaseGun & );

	int m_silencedModelIndex;
	bool m_inPrecache;

};

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Decoy grenades
//-----------------------------------------------------------------------------
class CDecoyGrenade : public CBaseCSGrenade
{
	[[= ks::reflect::NetTable{ .name = "DT_DecoyGrenade" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CDecoyGrenade, CBaseCSGrenade );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CDecoyGrenade() {}

	virtual CSWeaponID GetCSWeaponID( void ) const { return WEAPON_DECOY; }

private:
	CDecoyGrenade( const CDecoyGrenade& );
};

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Fragmentation grenades
//-----------------------------------------------------------------------------
class CFlashbang : public CBaseCSGrenade
{
	[[= ks::reflect::NetTable{ .name = "DT_Flashbang" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFlashbang, CBaseCSGrenade );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CFlashbang() {}

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_FLASHBANG; }

	CFlashbang( const CFlashbang & ) {}
};

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Fragmentation grenades
//-----------------------------------------------------------------------------
class CHEGrenade : public CBaseCSGrenade
{
	[[= ks::reflect::NetTable{ .name = "DT_HEGrenade" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CHEGrenade, CBaseCSGrenade );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CHEGrenade() {}

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_HEGRENADE; }

	CHEGrenade( const CHEGrenade & ) {}
};

class CWeaponCSBase;

// Purpose: Class to control 'aim-down-sights' aka "IronSight" weapon functionality


	inline constexpr auto IRONSIGHT_ANGLE_AVERAGE_SIZE = 8;
	inline constexpr auto IRONSIGHT_ANGLE_AVERAGE_DIVIDE = ( 1.0f / IRONSIGHT_ANGLE_AVERAGE_SIZE );
	inline constexpr auto IRONSIGHT_HIDE_CROSSHAIR_THRESHOLD = 0.5f;

enum CSIronSightMode
{
	IronSight_should_approach_unsighted = 0,
	IronSight_should_approach_sighted,
	IronSight_viewmodel_is_deploying,
	IronSight_weapon_is_dropped
};

// class to keep track of and update iron sight values and state information
class CIronSightController
{

public:

	CIronSightController();

	bool				Init( CWeaponCSBase *pWeaponToMonitor );
	inline bool			IsInitializedAndAvailable( void )		{ return m_bIronSightAvailable; }

	void				UpdateIronSightAmount( void );

	float				GetIronSightFOV( float flDefaultFOV, bool bUseBiasedValue = false );

	inline float		GetIronSightAmount( void )				{ return m_flIronSightAmount; }
	inline float		GetIronSightAmountGained( void )		{ return m_flIronSightAmountGained; }
	inline float		GetIronSightAmountBiased( void )		{ return m_flIronSightAmountBiased; }

	inline float		GetIronSightIdealFOV( void )			{ return m_flIronSightFOV; }
	inline float		GetIronSightPullUpDuration( void )		{ return (m_flIronSightPullUpSpeed > 0) ? (1.0f / m_flIronSightPullUpSpeed) : 0.0f; } // pull up duration is how long the pull up would take in seconds, not the speed
	inline float		GetIronSightPutDownDuration( void )		{ return (m_flIronSightPutDownSpeed > 0) ? (1.0f / m_flIronSightPutDownSpeed) : 0.0f; } // put down duration is how long the put down would take in seconds, not the speed

	inline bool			IsApproachingSighted( void ); //still true when weapon is 100% ironsighted
	inline bool			IsApproachingUnSighted( void ); //still true when weapon is 0% ironsighted
	inline bool			IsDeploying( void ); //is still playing deploy animation
	inline bool			IsDropped( void ); //is dropped loose in the world with no player owner

	bool				IsInIronSight( void ); //true if the weapon is ironsighted any positive amount

	void				SetState( CSIronSightMode newState );

	void				ApplyIronSightPositioning( Vector &vecCleanEyePosition, QAngle &angCleanEyeAngle, const Vector &vecBobbedEyePosition, const QAngle &angBobbedEyeAngle );

	void				IncreaseDotBlur(float flAmount);
	float				GetDotBlur(void);
	float				GetDotWidth(void);
	Vector2D			GetDotCoords(void);
	const char			*GetDotMaterial(void);

	bool				ShouldHideCrossHair( void );

	bool				PrepareScopeEffect( int x, int y, int w, int h, CViewSetup *pViewSetup );
	void				RenderScopeEffect( int x, int y, int w, int h, CViewSetup *pViewSetup );

private:

	bool		m_bIronSightAvailable;				//only true if members are initialized, valid, and ready to use

	QAngle		m_angPivotAngle;
	Vector		m_vecEyePos;
	float		m_flIronSightAmount;				//0.0 (not ironsighted at all) to 1.0 (fully looking down the sights)
	float		m_flIronSightAmountGained;
	float		m_flIronSightAmountBiased;
	float		m_flIronSightPullUpSpeed;			//speed to pull the weapon up to the eye
	float		m_flIronSightPutDownSpeed;			//speed to put the weapon back down into normal viewmodel position
	float		m_flIronSightPivotForward;
	float		m_flIronSightFOV;
	float		m_flIronSightLooseness;				//how rigidly the viewmodel reacts to changes in m_vecViewDeltaVelocity
	
	CWeaponCSBase	*m_pAttachedWeapon;

	void		AddToAngleAverage( QAngle newAngle );
	QAngle		GetAngleAverage( void );
	QAngle		m_angDeltaAverage[IRONSIGHT_ANGLE_AVERAGE_SIZE];
	QAngle		QAngleDiff( QAngle &angTarget, QAngle &angSrc );
	QAngle		m_angViewLast;

	Vector2D	m_vecDotCoords;
	float		m_flDotBlur;
	float		m_flSpeedRatio;

};

class CWeaponCSBase;

using namespace ::ks::reflect;

// ----------------------------------------------------------------------------- //
// CKnife class definition.
// ----------------------------------------------------------------------------- //

class CKnife : public CWeaponCSBase
{
	[[= ks::reflect::NetTable{ .name = "DT_WeaponKnife" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CKnife, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CKnife();

	// We say yes to this so the weapon system lets us switch to it.
	virtual bool HasPrimaryAmmo();
	virtual bool CanBeSelected();
	
	virtual void Precache();

	void Spawn();
	bool SwingOrStab( CSWeaponMode weaponMode );
	void PrimaryAttack();
	void SecondaryAttack();
	void WeaponAnimation( int iAnimation );

	virtual bool Deploy();

	bool CanDrop();

	void WeaponIdle();

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_KNIFE; }

private:
	CKnife( const CKnife & ) {}

};

class CKnifeGG : public CKnife
{
	[[= ks::reflect::NetTable{ .name = "DT_WeaponKnifeGG" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CKnifeGG, CKnife );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_KNIFE_GG; }
};

class CNewParticleEffect;

using namespace ::ks::reflect;


//-----------------------------------------------------------------------------
// Molotov grenades
//-----------------------------------------------------------------------------
class CMolotovGrenade : public CBaseCSGrenade
{
	[[= ks::reflect::NetTable{ .name = "DT_MolotovGrenade" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CMolotovGrenade, CBaseCSGrenade );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CMolotovGrenade() {}

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_MOLOTOV; }

	virtual bool	Simulate( void );
	virtual void	UpdateParticles( void );
	virtual void	OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect );

	virtual void	Drop( const Vector& vecVelocity );

private:
	CMolotovGrenade( const CMolotovGrenade& );

private:
	ks::tracked_ref<CNewParticleEffect> m_molotovParticleEffect;
};

//-----------------------------------------------------------------------------
// Incendiary grenades
//-----------------------------------------------------------------------------
class CIncendiaryGrenade : public CMolotovGrenade
{
	[[= ks::reflect::NetTable{ .name = "DT_IncendiaryGrenade" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CIncendiaryGrenade, CMolotovGrenade );
	DECLARE_NETWORKCLASS();
	//DECLARE_PREDICTABLE();

	CIncendiaryGrenade() {}

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_INCGRENADE; }

	//virtual bool	Simulate( void );
	//virtual void	UpdateParticles( void );
	//virtual void	OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect );

private:
	CIncendiaryGrenade( const CIncendiaryGrenade& );

	/*
private:
#if defined( CLIENT_DLL )
	ks::tracked_ref<CNewParticleEffect> m_molotovParticleEffect;
#endif
	*/
};

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// SensorGrenade grenades
//-----------------------------------------------------------------------------
class CSensorGrenade : public CBaseCSGrenade
{
	[[= ks::reflect::NetTable{ .name = "DT_SensorGrenade" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CSensorGrenade, CBaseCSGrenade );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CSensorGrenade() {}

	virtual CSWeaponID GetCSWeaponID( void ) const { return WEAPON_TAGRENADE; }

private:
	CSensorGrenade( const CSensorGrenade& );
};

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Smoke grenades
//-----------------------------------------------------------------------------
class CSmokeGrenade : public CBaseCSGrenade
{
	[[= ks::reflect::NetTable{ .name = "DT_SmokeGrenade" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CSmokeGrenade, CBaseCSGrenade );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CSmokeGrenade() {}

	virtual CSWeaponID GetCSWeaponID( void ) const { return WEAPON_SMOKEGRENADE; }

	CSmokeGrenade( const CSmokeGrenade & ) {}
};

class CAnimationLayer;
class CBaseAnimatingOverlay;
class CCSPlayer;
class CWeaponCSBase;



enum animstate_layer_t
{
	ANIMATION_LAYER_AIMMATRIX = 0,
	ANIMATION_LAYER_WEAPON_ACTION,
	ANIMATION_LAYER_WEAPON_ACTION_RECROUCH,
	ANIMATION_LAYER_ADJUST,
	ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL,
	ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB,
	ANIMATION_LAYER_MOVEMENT_MOVE,
	ANIMATION_LAYER_MOVEMENT_STRAFECHANGE,
	ANIMATION_LAYER_WHOLE_BODY,
	ANIMATION_LAYER_FLASHED,
	ANIMATION_LAYER_FLINCH,
	ANIMATION_LAYER_ALIVELOOP,
	ANIMATION_LAYER_LEAN,
	ANIMATION_LAYER_COUNT,
};

inline constexpr auto USE_ANIMLAYER_RAW_INDEX = false;
typedef const int* animlayerpreset;
#define REGISTER_ANIMLAYER_ORDER( _n ) inline constexpr int s_animLayerOrder ## _n [ANIMATION_LAYER_COUNT]

REGISTER_ANIMLAYER_ORDER( Default ) = {	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };

// animations can trigger the player to re-order their layers according to hardcoded presets, e.g.:
REGISTER_ANIMLAYER_ORDER( WeaponPost ) = {
	ANIMATION_LAYER_AIMMATRIX,
	ANIMATION_LAYER_WEAPON_ACTION_RECROUCH,
	ANIMATION_LAYER_ADJUST,
	ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL,
	ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB,
	ANIMATION_LAYER_MOVEMENT_MOVE,
	ANIMATION_LAYER_MOVEMENT_STRAFECHANGE,
	ANIMATION_LAYER_WEAPON_ACTION,
	ANIMATION_LAYER_WHOLE_BODY,
	ANIMATION_LAYER_FLASHED,
	ANIMATION_LAYER_FLINCH,
	ANIMATION_LAYER_ALIVELOOP,
	ANIMATION_LAYER_LEAN,
};

enum animstate_pose_param_idx_t
{
	PLAYER_POSE_PARAM_FIRST = 0,
	PLAYER_POSE_PARAM_LEAN_YAW = PLAYER_POSE_PARAM_FIRST,
	PLAYER_POSE_PARAM_SPEED,
	PLAYER_POSE_PARAM_LADDER_SPEED,
	PLAYER_POSE_PARAM_LADDER_YAW,
	PLAYER_POSE_PARAM_MOVE_YAW,
	PLAYER_POSE_PARAM_RUN,
	PLAYER_POSE_PARAM_BODY_YAW,
	PLAYER_POSE_PARAM_BODY_PITCH,
	PLAYER_POSE_PARAM_DEATH_YAW,
	PLAYER_POSE_PARAM_STAND,
	PLAYER_POSE_PARAM_JUMP_FALL,
	PLAYER_POSE_PARAM_AIM_BLEND_STAND_IDLE,
	PLAYER_POSE_PARAM_AIM_BLEND_CROUCH_IDLE,
	PLAYER_POSE_PARAM_STRAFE_DIR,
	PLAYER_POSE_PARAM_AIM_BLEND_STAND_WALK,
	PLAYER_POSE_PARAM_AIM_BLEND_STAND_RUN,
	PLAYER_POSE_PARAM_AIM_BLEND_CROUCH_WALK,
	PLAYER_POSE_PARAM_MOVE_BLEND_WALK,
	PLAYER_POSE_PARAM_MOVE_BLEND_RUN,
	PLAYER_POSE_PARAM_MOVE_BLEND_CROUCH_WALK,
	//PLAYER_POSE_PARAM_STRAFE_CROSS,
	PLAYER_POSE_PARAM_COUNT,
};

struct animstate_pose_param_cache_t
{
	bool		m_bInitialized;
	int			m_nIndex;
	const char* m_szName;

	animstate_pose_param_cache_t();

	int		GetIndex( void );
	float	GetValue( CCSPlayer* pPlayer );
	void	SetValue( CCSPlayer* pPlayer, float flValue );
	bool	Init( CCSPlayer* pPlayer, const char* szPoseParamName );
};

inline constexpr auto MAX_ANIMSTATE_ANIMNAME_CHARS = 64;

inline constexpr auto ANIM_TRANSITION_WALK_TO_RUN = 0;
inline constexpr auto ANIM_TRANSITION_RUN_TO_WALK = 1;

//these correlate to the CSWeaponType enum
const char* const g_szWeaponPrefixLookupTable[] = {
	"knife",
	"pistol",
	"smg",
	"rifle",
	"shotgun",
	"sniper",
	"heavy",
	"c4",
	"grenade",
	"knife"
};

struct procedural_foot_t
{
	Vector m_vecPosAnim;
	Vector m_vecPosAnimLast;
	Vector m_vecPosPlant;
	Vector m_vecPlantVel;
	float m_flLockAmount;
	float m_flLastPlantTime;

	procedural_foot_t();

	void Init( Vector vecNew )
	{
		m_vecPosAnim = vecNew;
		m_vecPosAnimLast = vecNew;
		m_vecPosPlant = vecNew;
		m_vecPlantVel.Init();
		m_flLockAmount = 0;
		m_flLastPlantTime = gpGlobals->curtime;
	}
};

struct aimmatrix_transition_t
{
	float	m_flDurationStateHasBeenValid;
	float	m_flDurationStateHasBeenInValid;
	float	m_flHowLongToWaitUntilTransitionCanBlendIn;
	float	m_flHowLongToWaitUntilTransitionCanBlendOut;
	float	m_flBlendValue;

	void UpdateTransitionState( bool bStateShouldBeValid, float flTimeInterval, float flSpeed );

	void Init( void )
	{
		m_flDurationStateHasBeenValid = 0;
		m_flDurationStateHasBeenInValid = 0;
		m_flHowLongToWaitUntilTransitionCanBlendIn = 0.3f;
		m_flHowLongToWaitUntilTransitionCanBlendOut = 0.3f;
		m_flBlendValue = 0;
	}

	aimmatrix_transition_t()
	{
		Init();
	}
};

class CCSGOPlayerAnimState
{

public:

	DECLARE_CLASS_NOBASE( CCSGOPlayerAnimState );
	CCSGOPlayerAnimState( CCSPlayer *pPlayer );

	void					Reset( void );
	void					Release( void ) { delete this; }
	void					Update( float eyeYaw, float eyePitch, bool bForce = false );

	float					GetPrimaryCycle( void ) { return m_flPrimaryCycle; }

	void					SetUpVelocity( void );
	void					SetUpAimMatrix( void );
	void					SetUpWeaponAction( void );
	void					SetUpMovement( void );
	void					SetUpAliveloop( void );
	void					SetUpWholeBodyAction( void );
	void					SetUpFlashedReaction( void );
	void					SetUpFlinch( void );
	void					SetUpLean( void );

	void					UpdateAnimLayer( animstate_layer_t nLayerIndex, int nSequence, float flPlaybackRate, float flWeight, float flCycle = 0 );
	void					IncrementLayerCycleWeightRateGeneric( animstate_layer_t nLayerIndex );
	void					IncrementLayerCycle( animstate_layer_t nLayerIndex, bool bAllowLoop = false );
	void					IncrementLayerWeight( animstate_layer_t nLayerIndex );
	void					SetLayerRate( animstate_layer_t nLayerIndex, float flRate );
	void					SetLayerCycle( animstate_layer_t nLayerIndex, float flCycle );
	void					SetLayerWeight( animstate_layer_t nLayerIndex, float flWeight );
	void					SetLayerWeightRate( animstate_layer_t nLayerIndex, float flPrevious );
	void					SetLayerSequence( animstate_layer_t nLayerIndex, int nSequence );
	float					GetLayerWeight( animstate_layer_t nLayerIndex );
	float					GetLayerIdealWeightFromSeqCycle( animstate_layer_t nLayerIndex );
	float					GetLayerRate( animstate_layer_t nLayerIndex );
	float					GetLayerCycle( animstate_layer_t nLayerIndex );
	int						GetLayerSequence( animstate_layer_t nLayerIndex );
	Activity				GetLayerActivity( animstate_layer_t nLayerIndex );
	bool					IsLayerSequenceCompleted( animstate_layer_t nLayerIndex );
	bool					LayerSequenceHasActMod( animstate_layer_t nLayerIndex, const char* szActMod );

	void					WriteAnimstateDebugBuffer( char *pDest );

	void					ApplyLayerOrderPreset( animlayerpreset nNewPreset, bool bForce = false );
	void					UpdateLayerOrderPreset( animstate_layer_t nLayerIndex, int nSequence );
	animlayerpreset			m_pLayerOrderPreset;

	bool					m_bFirstRunSinceInit;

	void					ModifyEyePosition( Vector& vecInputEyePos );

	bool					m_bFirstFootPlantSinceInit;
	void					DoProceduralFootPlant( matrix3x4a_t boneToWorld[], mstudioikchain_t *pLeftFootChain, mstudioikchain_t *pRightFootChain, BoneVector pos[] );
	int						m_iLastUpdateFrame;
	
	float					m_flEyePositionSmoothLerp;
	void					OnClientWeaponChange( CWeaponCSBase* pCurrentWeapon );

	float					m_flStrafeChangeWeightSmoothFalloff;

	void					NotifyOnLayerChangeSequence( const CAnimationLayer* pLayer, const int nNewSequence );
	void					NotifyOnLayerChangeWeight( const CAnimationLayer* pLayer, const float flNewWeight );
	void					NotifyOnLayerChangeCycle( const CAnimationLayer* pLayer, const float flNewcycle );

private:

	aimmatrix_transition_t	m_tStandWalkAim;
	aimmatrix_transition_t	m_tStandRunAim;
	aimmatrix_transition_t	m_tCrouchWalkAim;

	bool					LayerToIndex( const CAnimationLayer* pLayer, int &nIndex );

	float					LerpCrouchWalkRun( float flCrouchVal, float flWalkVal, float flRunVal );

	bool					CacheSequences( void );
	const char*				GetWeaponPrefix( void );
	int						m_cachedModelIndex;

	float					FootBarrierEq( float flIn, float flMinWidth );
	float					m_flStepHeightLeft;
	float					m_flStepHeightRight;

	CWeaponCSBase*			m_pWeaponLastBoneSetup;

	CCSPlayer*				m_pPlayer;
	CWeaponCSBase*			m_pWeapon;
	CWeaponCSBase*			m_pWeaponLast;

	float					m_flLastUpdateTime;
	int						m_nLastUpdateFrame;
	float					m_flLastUpdateIncrement;

	float					m_flEyeYaw;
	float					m_flEyePitch;
	float					m_flFootYaw;
	float					m_flFootYawLast;
	float					m_flMoveYaw;
	float					m_flMoveYawIdeal;
	float					m_flMoveYawCurrentToIdeal;
	float					m_flTimeToAlignLowerBody;

	float					m_flPrimaryCycle;
	float					m_flMoveWeight;
	float					m_flMoveWeightSmoothed;
	float					m_flAnimDuckAmount;
	float					m_flDuckAdditional;
	float					m_flRecrouchWeight;

	Vector					m_vecPositionCurrent;
	Vector					m_vecPositionLast;

	Vector					m_vecVelocity;
	Vector					m_vecVelocityNormalized;
	Vector					m_vecVelocityNormalizedNonZero;
	float					m_flVelocityLengthXY;
	float					m_flVelocityLengthZ;

	float					m_flSpeedAsPortionOfRunTopSpeed;
	float					m_flSpeedAsPortionOfWalkTopSpeed;
	float					m_flSpeedAsPortionOfCrouchTopSpeed;

	float					m_flDurationMoving;
	float					m_flDurationStill;

	bool					m_bOnGround;
	bool					m_bLanding;
	float					m_flJumpToFall;
	float					m_flDurationInAir;
	float					m_flLeftGroundHeight;
	float					m_flLandAnimMultiplier;

	float					m_flWalkToRunTransition;

	bool					m_bLandedOnGroundThisFrame;
	bool					m_bLeftTheGroundThisFrame;
	float					m_flInAirSmoothValue;

	bool					m_bOnLadder;
	float					m_flLadderWeight;
	float					m_flLadderSpeed;

	bool					m_bWalkToRunTransitionState;

	bool					m_bDefuseStarted;
	bool					m_bPlantAnimStarted;
	bool					m_bTwitchAnimStarted;
	bool					m_bAdjustStarted;

	std::vector< ActivityModifierSym_t >	m_ActivityModifiers;

	float					m_flNextTwitchTime;

	float					m_flTimeOfLastKnownInjury;

	float					m_flLastVelocityTestTime;
	Vector					m_vecVelocityLast;
	Vector					m_vecTargetAcceleration;
	Vector					m_vecAcceleration;
	float					m_flAccelerationWeight;

	float					m_flAimMatrixTransition;
	float					m_flAimMatrixTransitionDelay;

	bool					m_bFlashed;

	float					m_flStrafeChangeWeight;
	float					m_flStrafeChangeTargetWeight;
	float					m_flStrafeChangeCycle;
	int						m_nStrafeSequence;
	bool					m_bStrafeChanging;
	float					m_flDurationStrafing;

	float					m_flFootLerp;

	bool					m_bFeetCrossed;

	bool					m_bPlayerIsAccelerating;

	animstate_pose_param_cache_t m_tPoseParamMappings[ PLAYER_POSE_PARAM_COUNT ];

	float					m_flDurationMoveWeightIsTooHigh;
	float					m_flStaticApproachSpeed;

	int						m_nPreviousMoveState;
	float					m_flStutterStep;

	float					m_flActionWeightBiasRemainder;

	procedural_foot_t	m_footLeft;
	procedural_foot_t	m_footRight;

	float					m_flCameraSmoothHeight;
	bool					m_bSmoothHeightValid;
	float					m_flLastTimeVelocityOverTen;

	float					m_flAimYawMin;
	float					m_flAimYawMax;
	float					m_flAimPitchMin;
	float					m_flAimPitchMax;

	//float					m_flMoveWalkWeight;
	//float					m_flMoveCrouchWalkWeight;
	//float					m_flMoveRunWeight;

	int						m_nAnimstateModelVersion;
};

CCSGOPlayerAnimState *CreateCSGOPlayerAnimstate( CBaseAnimatingOverlay *pEntity );

#undef REGISTER_ANIMLAYER_ORDER

class CNewParticleEffect;

using namespace ::ks::reflect;


class CDecoyProjectile : public CBaseCSGrenadeProjectile
{
	[[= ks::reflect::NetTable{ .name = "DT_DecoyProjectile" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CDecoyProjectile, CBaseCSGrenadeProjectile );
	DECLARE_NETWORKCLASS();

	virtual bool Simulate( void );

	virtual void OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect );
	virtual void OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect );

private:
	ks::tracked_ref<CNewParticleEffect> m_decoyParticleEffect;

};

void FX_CS_BloodSpray( 
	const Vector &origin,
	const Vector &normal,
	float flDamage );

using namespace ::ks::reflect;

// This runs on both the client and the server.
// On the server, it only does the damage calculations.
// On the client, it does all the effects.
void FX_FireBullets( 
	int	iPlayer,
	uint16 nItemDefIndex,
	const Vector &vOrigin,
	const QAngle &vAngles,
	CSWeaponID iWeaponID,
	int	iMode,
	int iSeed,
	float fInaccuracy,
	float fSpread,
	float fAccuracyFishtail,
	float flSoundTime/* = 0.0f*/,
	WeaponSound_t sound_type/* = SINGLE*/,
	float flRecoilIndex/* = 0.0f*/
	);

// This runs on both the client and the server.
// On the server, it dispatches a TE_PlantBomb to visible clients.
// On the client, it plays the planting animation.
enum PlantBombOption_t
{
	PLANTBOMB_PLANT, // play the planting animation
	PLANTBOMB_ABORT, // abort the planting animation
	// NOTE: If you add additional items to this enum then m_option in CTEPlantBomb will need to have its SendPropInt setting changed to have more than one bit.
};
void FX_PlantBomb( int iPlayer, const Vector &vOrigin, PlantBombOption_t option );

class CCSPlayer;

using namespace ::ks::reflect;


// Purpose: Healthshot, it makes you go

//-----------------------------------------------------------------------------------------------------------
/**
* Healthshot. When used, give the player speed boost for a short amount of time
*/
class CItem_Healthshot : public CWeaponBaseItem
{
	[[= ks::reflect::NetTable{ .name = "DT_Item_Healthshot" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CItem_Healthshot, CWeaponBaseItem );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

// #ifndef CLIENT_DLL
// 	DECLARE_DATADESC();
// #endif

	CItem_Healthshot() 	{}

	virtual void	Precache( void );

	virtual bool	CanPrimaryAttack( void );

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_HEALTHSHOT; }

	void DropHealthshot( void );
	// CBaseBeltItem
	virtual bool CanUseOnSelf( CCSPlayer *pPlayer );
	virtual void OnStartUse( CCSPlayer *pPlayer );
	virtual float GetUseTimerDuration( void );
	virtual void WeaponIdle();

private:
	CItem_Healthshot( const CItem_Healthshot & ) {}
};

class CNewParticleEffect;

using namespace ::ks::reflect;


class CMolotovProjectile : public CBaseCSGrenadeProjectile
{
	[[= ks::reflect::NetTable{ .name = "DT_MolotovProjectile" } ]]
	[[= ks::reflect::From<"m_bIsIncGrenade", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CMolotovProjectile, CBaseCSGrenadeProjectile );
	DECLARE_NETWORKCLASS();

	virtual bool Simulate( void );

	virtual void OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect );
	virtual void OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect );

protected:
	CNetworkVar( bool, m_bIsIncGrenade );

private:
	ks::tracked_ref<CNewParticleEffect> m_molotovParticleEffect;

};

class CBaseAnimating;
class CCSPlayer;
class CRopeKeyframe;

using namespace ::ks::reflect;


// ------------------------------------------------------------------------------------------ //
// CPlantedC4 class.
// For now to show the planted c4 on the radar - client proxy to remove the CBaseAnimating 
// network vars?
// ------------------------------------------------------------------------------------------ //

class CPlantedC4 : public CBaseAnimating
{
	[[= ks::reflect::NetTable{ .name = "DT_PlantedC4" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CPlantedC4, CBaseAnimating );
	DECLARE_CLIENTCLASS();

	CPlantedC4();
	virtual ~CPlantedC4();

	void Explode( void );
	void Spawn( void );
	virtual void SetDormant( bool bDormant );

	void ClientThink( void );

	int GetSecondsRemaining( void ) { return std::ceil( m_flC4Blow - gpGlobals->curtime ); }

	inline bool IsBombActive( void ) { return m_bBombTicking; }
	CNetworkVar( bool, m_bBombTicking, [[= ks::reflect::Net{} ]] );

	float m_flNextGlow;
	float m_flNextBeep;

	[[= ks::reflect::Net{} ]] float m_flC4Blow;
	[[= ks::reflect::Net{} ]] float m_flTimerLength;
	float m_bTenSecWarning;
	float m_bTriggerWarning;
	float m_bExplodeWarning;

	CNetworkVar( float, m_flDefuseLength, [[= ks::reflect::Net{} ]] );	
	CNetworkVar( float, m_flDefuseCountDown, [[= ks::reflect::Net{} ]] ); 
	CNetworkVar( bool, m_bBombDefused, [[= ks::reflect::Net{} ]] ); 
	CNetworkVar( CHandle<CCSPlayer>, m_hBombDefuser, [[= ks::reflect::Net{} ]] );

	CGlowObject m_GlowObject;

	std::vector< CHandle<CRopeKeyframe> > m_hDefuserRopes;
	CHandle<CBaseAnimating> m_hDefuserMultimeter;

	float GetDefuseProgress( void );

	// returns progress towards detonation in percent (detonation at 0%)
	float GetDetonationProgress( void );

	float	m_flNextRadarFlashTime;	// next time to change flash state
	bool	m_bRadarFlash;			// is the flash on or off

	virtual void UpdateOnRemove( void ) override;

private:
	bool CreateDefuserRopes( void );
	void DestroyDefuserRopes( void );
	CHandle<CCSPlayer> m_hLocalDefusingPlayerHandle;

};

extern std::vector< CPlantedC4* > g_PlantedC4s;

class CBaseEntity;
class CBasePlayer;

// Purpose: APIs for player pickup of physics objects


// Reasons behind a pickup
enum PhysGunPickup_t
{
	PICKED_UP_BY_CANNON,
	PUNTED_BY_CANNON,
	PICKED_UP_BY_PLAYER, // Picked up by +USE, not physgun.
};

// Reasons behind a drop
enum PhysGunDrop_t
{
	DROPPED_BY_PLAYER,
	THROWN_BY_PLAYER,
	DROPPED_BY_CANNON,
	LAUNCHED_BY_CANNON,
};

enum PhysGunForce_t
{
	PHYSGUN_FORCE_DROPPED,	// Dropped by +USE
	PHYSGUN_FORCE_THROWN,	// Thrown from +USE
	PHYSGUN_FORCE_PUNTED,	// Punted by cannon
	PHYSGUN_FORCE_LAUNCHED,	// Launched by cannon
};

void PlayerPickupObject( CBasePlayer *pPlayer, CBaseEntity *pObject );
void Pickup_ForcePlayerToDropThisObject( CBaseEntity *pTarget );

void Pickup_OnPhysGunDrop( CBaseEntity *pDroppedObject, CBasePlayer *pPlayer, PhysGunDrop_t reason );
void Pickup_OnPhysGunPickup( CBaseEntity *pPickedUpObject, CBasePlayer *pPlayer, PhysGunPickup_t reason = PICKED_UP_BY_CANNON );
bool Pickup_OnAttemptPhysGunPickup( CBaseEntity *pPickedUpObject, CBasePlayer *pPlayer, PhysGunPickup_t reason = PICKED_UP_BY_CANNON );
bool Pickup_GetPreferredCarryAngles( CBaseEntity *pObject, CBasePlayer *pPlayer, matrix3x4_t &localToWorld, QAngle &outputAnglesWorldSpace );
bool Pickup_ForcePhysGunOpen( CBaseEntity *pObject, CBasePlayer *pPlayer );
bool Pickup_ShouldPuntUseLaunchForces( CBaseEntity *pObject, PhysGunForce_t reason );
AngularImpulse Pickup_PhysGunLaunchAngularImpulse( CBaseEntity *pObject, PhysGunForce_t reason );
Vector Pickup_DefaultPhysGunLaunchVelocity( const Vector &vecForward, float flMass );
Vector Pickup_PhysGunLaunchVelocity( CBaseEntity *pObject, const Vector &vecForward, PhysGunForce_t reason );

CBaseEntity	*Pickup_OnFailedPhysGunPickup( CBaseEntity *pPickedUpObject, Vector vPhysgunPos );

class IPlayerPickupVPhysics
{
public:
	// Callbacks for the physgun/cannon picking up an entity
	virtual bool			OnAttemptPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason = PICKED_UP_BY_CANNON ) = 0;
	virtual CBaseEntity		*OnFailedPhysGunPickup( Vector vPhysgunPos ) = 0;
	virtual void			OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason = PICKED_UP_BY_CANNON ) = 0;
	virtual void			OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t Reason ) = 0;
	virtual bool			HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer = nullptr ) = 0;
	virtual QAngle			PreferredCarryAngles( void )  = 0;
	virtual bool			ForcePhysgunOpen( CBasePlayer *pPlayer ) = 0;
	virtual AngularImpulse	PhysGunLaunchAngularImpulse() = 0;
	virtual bool			ShouldPuntUseLaunchForces( PhysGunForce_t reason ) = 0;
	virtual Vector			PhysGunLaunchVelocity( const Vector &vecForward, float flMass ) = 0;
};

class CDefaultPlayerPickupVPhysics : public IPlayerPickupVPhysics
{
public:
	virtual bool			OnAttemptPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason = PICKED_UP_BY_CANNON ) { return true; }
	virtual CBaseEntity		*OnFailedPhysGunPickup( Vector vPhysgunPos ) { return nullptr; }
	virtual void			OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason = PICKED_UP_BY_CANNON ) {}
	virtual void			OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t reason ) {}
	virtual bool			HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer ) { return false; }
	virtual QAngle			PreferredCarryAngles( void ) { return vec3_angle; }
	virtual bool			ForcePhysgunOpen( CBasePlayer *pPlayer ) { return false; }
	virtual AngularImpulse	PhysGunLaunchAngularImpulse() { return RandomAngularImpulse( -600, 600 ); }
	virtual bool			ShouldPuntUseLaunchForces( PhysGunForce_t reason ) { return false; }
	virtual Vector			PhysGunLaunchVelocity( const Vector &vecForward, float flMass )
	{
		return Pickup_DefaultPhysGunLaunchVelocity( vecForward, flMass );
	}
};

class CRadioStatus : public CAutoGameSystem
{
public:
	CRadioStatus();

	virtual bool Init();
	virtual void Shutdown();

	virtual void LevelInitPostEntity();
	virtual void LevelShutdownPreEntity();
	
public:

	// Called when a player uses the radio
	void	UpdateRadioStatus(int entindex, float duration);

	// Called when a player (bot) speaks a wav file
	void	UpdateVoiceStatus(int entindex, float duration);

	// Call from the HUD_CreateEntities function so it can add sprites above player heads.
	void	DrawHeadLabels();

private:

	float		m_radioUntil[MAX_PLAYERS];	// Who is currently talking. Indexed by client index.
	IMaterial	*m_pHeadLabelMaterial;		// For labels above players' heads.

	void		ExpireBotVoice( bool force = false );
	float		m_voiceUntil[MAX_PLAYERS];	// Who is currently talking. Indexed by client index.
};

// Get the (global) voice manager. 
CRadioStatus* RadioManager();

class CNewParticleEffect;

using namespace ::ks::reflect;


class CSensorGrenadeProjectile : public CBaseCSGrenadeProjectile//, public CBreakableProp
{
	[[= ks::reflect::NetTable{ .name = "DT_SensorGrenadeProjectile" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CSensorGrenadeProjectile, CBaseCSGrenadeProjectile );
	DECLARE_NETWORKCLASS();

	virtual bool Simulate( void );

	virtual void OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect );
	virtual void OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect );

private:
	ks::tracked_ref<CNewParticleEffect> m_sensorgrenadeParticleEffect;

};

using namespace ::ks::reflect;

class CSmokeGrenadeProjectile : public CBaseCSGrenadeProjectile
{
	[[= ks::reflect::NetTable{ .name = "DT_SmokeGrenadeProjectile" } ]]
	[[= ks::reflect::From<"m_bDidSmokeEffect", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_nSmokeEffectTickBegin", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

	public:
	DECLARE_CLASS( CSmokeGrenadeProjectile, CBaseCSGrenadeProjectile );
	DECLARE_NETWORKCLASS();

	CSmokeGrenadeProjectile() {}
	virtual ~CSmokeGrenadeProjectile();

	virtual void PostDataUpdate( DataUpdateType_t updateType ) override;
	virtual void OnDataChanged( DataUpdateType_t updateType ) override;

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_SMOKE; }
	void SpawnSmokeEffect();

	CNetworkVar( int, m_nSmokeEffectTickBegin );
	CNetworkVar( bool, m_bDidSmokeEffect );

	bool m_bSmokeEffectSpawned;
};
