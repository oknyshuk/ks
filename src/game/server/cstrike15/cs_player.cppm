// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2011 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include <time.h>
#include "eiface.h"
#include "networkvar.h"
#include "reflect_annotations.h"
#include "server_class.h"
#include "../predictable_entity.h"
#include "bspfile.h"
#include "const.h"
#include "edict.h"
#include "engine/IEngineTrace.h"
#include "igameevents.h"
#include "usermessages_schema.h"
#include "cs_gamerules.h"
#include "declare_class.h"
#include "reflect.h"
#include "csgo_limits.h"
#include "responserules/response_types.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "../ai_criteria.h"
#include "coordsize.h"
#include "string_t.h"
#include "datamap.h"
#include "csgo_playeranimstate.h"
module ks.server:cstrike15.cs_player;

import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import :player;
import :cstrike15.cs_playeranimstate;
import :baseentity;
import ks.game.shareddefs;
import std;
import ks.core.basetypes;
import ks.tier1.createinterface;
import ks.core.wchartypes;
import :vscript;
import ks.core.platform;
import ks.core.types;
import ks.game.cstrike15.cstrike15_gcconstants;
import ks.tier1.convar;
import ks.tier1.keyvalues;
import ks.tier1.playerid;
import ks.tier1.strtools;
import :fish;
import :cstrike15.bot.bot;
import :nav_4;
import ks.game.igamemovement;
import ks.core.threadtools;
import ks.game.cstrike15.cs_player_rank_shared;
import ks.game.iplayeranimstate;
import ks.tier1.icvar;
import ks.tier1.map_ops;
import :sharedvar;
import ks.game.ai_activity;

class CBaseGrenade;
class CCSWeaponInfo;

using namespace ::ks::reflect;

	class CCSWeaponInfo;

class CBaseCSGrenadeProjectile : public CBaseGrenade
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseCSGrenadeProjectile" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CBaseCSGrenadeProjectile, CBaseGrenade );
	DECLARE_NETWORKCLASS(); 

	 void Spawn_Impl();
	//virtual bool	IsGrenadeProjectile( void ) { return true; };

public:

	// This gets sent to the client and placed in the client's interpolation history
	// so the projectile starts out moving right off the bat.
	CNetworkVector( m_vInitialVelocity, [[= ks::reflect::Net{ .bits = 20, .low = -3000, .high = 3000, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( int, m_nBounces, [[= ks::reflect::Net{ .bits = -1 } ]] );

	DECLARE_DATADESC();

	CBaseCSGrenadeProjectile() : m_pWeaponInfo(nullptr) {}
	 void PostConstructor_Impl( const char *className );
	virtual ~CBaseCSGrenadeProjectile();

	 void Precache_Impl();

	 int				UpdateTransmitState_Impl();
	 int				ShouldTransmit_Impl( const CCheckTransmitInfo *pInfo );

	//Constants for all CS Grenades
	static inline float GetGrenadeGravity() { return 0.4f; }
	static inline const float GetGrenadeFriction() { return 0.2f; }
	static inline const float GetGrenadeElasticity() { return 0.45f; }

	//Think function to emit danger sounds for the AI
	void DangerSoundThink( void );

	virtual void OnBounced( void ) {}
	
	virtual float GetShakeAmplitude( void ) { return 0.0f; }
	 void Splash_Impl();

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_EXPLOSIVE; }

	// Specify what velocity we want the grenade to have on the client immediately.
	// Without this, the entity wouldn't have an interpolation history initially, so it would
	// sit still until it had gotten a few updates from the server.
	void SetupInitialTransmittedGrenadeVelocity( const Vector &velocity );

    // [jpaquin] give grenade projectiles a link back to the type
	// of weapon they are
	const CCSWeaponInfo *m_pWeaponInfo;

	EHANDLE m_lastHitPlayer;

	void DetonateOnNextThink( void ) { m_flDetonateTime = 0.0f; }

	 unsigned int PhysicsSolidMaskForEntity_Impl( void ) const;

protected:

	//Set the time to detonate ( now + timer )
	void SetDetonateTimerLength( float timer );

private:	
	
	//Custom collision to allow for constant elasticity on hit surfaces
	public:  void ResolveFlyCollisionCustom_Impl( trace_t &trace, Vector &vecVelocity );
	private:
	
	float m_flDetonateTime;

};

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

// Purpose: Headers and defines for Autobuy and Rebuy

/**
 * Weapon classes as used by the AutoBuy
 * Has to be different that the previous ones because these are bitmasked values as a weapon can be from
 * more than one class.  This also includes all the classes of equipment that a player can buy.
 */
enum AutoBuyClassType
{
	AUTOBUYCLASS_PRIMARY = 1,
	AUTOBUYCLASS_SECONDARY = 2,
	AUTOBUYCLASS_AMMO = 4,
	AUTOBUYCLASS_ARMOR = 8,
	AUTOBUYCLASS_DEFUSER = 16,
	AUTOBUYCLASS_PISTOL = 32,
	AUTOBUYCLASS_SMG = 64,
	AUTOBUYCLASS_RIFLE = 128,
	AUTOBUYCLASS_SNIPERRIFLE = 256,
	AUTOBUYCLASS_SHOTGUN = 512,
	AUTOBUYCLASS_MACHINEGUN = 1024,
	AUTOBUYCLASS_GRENADE = 2048,
	AUTOBUYCLASS_NIGHTVISION = 4096,
	AUTOBUYCLASS_SHIELD = 8192,
};

struct AutoBuyInfoStruct
{
	AutoBuyClassType m_class;
	loadout_positions_t m_LoadoutPosition;
	const char *m_command;
	const char *m_classname;
};

class RebuyStruct
{

private:
	int			m_nPrimaryPos;
	int			m_nSecondaryPos;
	CSWeaponID	m_tertiaryId;				// used just for taser right now
	CSWeaponID	m_grenades[8];
	int			m_armor;					// 0, 1, or 2 (0 = none, 1 = vest, 2 = vest + helmet)
	bool		m_defuser;					// do we want a defuser
	bool		m_nightVision;				// do we want night vision
	bool		m_isNotEmpty;					

public:
	RebuyStruct()
	{
		Clear();
	}

	void Clear()
	{
		std::memset(this, 0, sizeof(RebuyStruct));
	}

	bool isEmpty( void )
	{
		return !m_isNotEmpty;
	}

	void SetPrimary( int nPrimaryPos );

	int GetPrimary( void )
	{
		return m_nPrimaryPos;
	}

	void SetSecondary( int nSecondaryPos );

	int GetSecondary( void )
	{
		return m_nSecondaryPos;
	}

	void SetTertiary( CSWeaponID tertiary );

	CSWeaponID GetTertiary( void )
	{
		return m_tertiaryId;
	}

	void SetGrenade( int index, CSWeaponID grenade );

	CSWeaponID GetGrenade( int index )
	{
		return m_grenades[index];
	}

	void SetArmor( int armor );

	int GetArmor( void )
	{
		return m_armor;
	}

	void SetDefuser( bool defuser );

	bool GetDefuser( void )
	{
		return m_defuser;
	}

	void SetNightVision( bool nv );

	bool GetNightVision( void )
	{
		return m_nightVision;
	}

	int numGrenades( void )
	{
		return std::size( m_grenades );
	}

};

extern AutoBuyInfoStruct g_autoBuyInfo[];

class CCSPlayer;

using namespace ::ks::reflect;


// Purpose: Holds defintion for game ammo types

struct blackmarket_items_t
{
	const char		*pClassname;
	int				iDefaultPrice;
};

extern blackmarket_items_t blackmarket_items[];

void BlackMarketAddWeapon( const char *pWeaponName, CCSPlayer *pBuyer );

class CBlackMarketElement
{
public:

	DECLARE_CLASS_NOBASE( CBlackMarketElement );

	// For CNetworkVars.
	void NetworkStateChanged();
	void NetworkStateChanged( void *pVar );

	CBlackMarketElement()
	{
		m_iPrice = 0;
		m_iTimesBought = 0;
		m_iWeaponID = 0;
	}

	CNetworkVar( int, m_iPrice );
	CNetworkVar( int, m_iWeaponID );

	int  m_iTimesBought;
};

class CBaseEntity;
class CCSPlayer;



void respawn( CBaseEntity *pEdict, bool fCopyCorpse );

void FinishClientPutInServer( CCSPlayer *pPlayer );

class CBaseEntity;
class CCSPlayer;

using namespace ::ks::reflect;


// Purpose: A game system for tracking and updating entity spot state


class CCSEntitySpotting : public CAutoGameSystemPerFrame
{
public:
	enum SpottingRules_T
	{
		SPOT_RULE_ENEMY					= ( 1<<0 ),  // spotted when seen by an enemy
		SPOT_RULE_CT					= ( 1<<1 ),  // spotted when seen by CT
		SPOT_RULE_T						= ( 1<<2 ),  // spotted when seen by T
		SPOT_RULE_ALWAYS_SEEN_BY_FRIEND	= ( 1<<3 ),  // always visible to friend
		SPOT_RULE_ALWAYS_SEEN_BY_CT		= ( 1<<4 ),  // always visible to CT
		SPOT_RULE_ALWAYS_SEEN_BY_T		= ( 1<<5 ),  // always visible to T
	};

	CCSEntitySpotting( const char * szName );
	virtual ~CCSEntitySpotting( void ) {}
	
	virtual bool Init( void );
	virtual char const *Name() { return "CCSEntitySpotting"; }
	virtual void FrameUpdatePostEntityThink( void );

	void UpdateSpottedEntities( void );
	
protected:
	float m_fLastUpdate;
};

extern CCSEntitySpotting * g_EntitySpotting;

//===========================================================
// - GatherNonPVSSpottedEntitiesFunctor -
//
// Given a player, generate a list of spotted entities that
// exist outside of that player's PVS.
// Query GetSpotted for result
//===========================================================
class GatherNonPVSSpottedEntitiesFunctor
{
public:
	GatherNonPVSSpottedEntitiesFunctor( CCSPlayer * pPlayer );

	bool operator()( CBaseEntity * pEntity );

	const std::bitset<MAX_EDICTS> & GetSpotted( void )
	{
		return m_EntitySpotted;
	}

private:
	CCSPlayer          *m_pPlayer;
	int					m_nSourceTeam;
	std::bitset<MAX_EDICTS> m_EntitySpotted;
	byte				m_pSourcePVS[MAX_MAP_LEAFS/8];
	bool				m_bForceSpot;
};

class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CCSFunFactMgr;
class CCSPlayer;
class CCSWeaponInfo;
class CEconItemView;
class CEconQuestDefinition;
class CGameCoopMissionManager;
class CHostage;
class CNavArea;
class CRecipientFilter;
class CTakeDamageInfo;
class CTeam;
class CWeaponCSBase;
struct inputdata_t;

using namespace ::ks::reflect;


// Purpose: The TF Game rules object


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

	extern ConVar mp_autoteambalance;

// forward declare GC message

int ScramblePlayersSort( CCSPlayer* const *p1, CCSPlayer* const *p2 );

class CCSMatch
{		
public:
	CCSMatch();

	void Reset( void );

	void SetPhase( GamePhase phase );
	GamePhase GetPhase( void ) const { return m_phase; }

	//These functions add to both the score and the number of rounds
	void AddTerroristWins( int numWins );
	void AddCTWins( int numWins);
	void IncrementRound( int nNumRounds );

	//These functions only adjust the score (without adding to the number of rounds played)
	void AddTerroristBonusPoints( int numWins );
	void AddCTBonusPoints( int numWins);

	int GetTerroristScore( void ) const 	{ return m_terroristScoreTotal; }
	int GetCTScore( void ) const  			{ return m_ctScoreTotal; }	

	int GetTeamScore( int nTeam ) const		{ switch( nTeam ) { case TEAM_TERRORIST: return GetTerroristScore(); case TEAM_CT: return GetCTScore(); default: contract_assert( !"unexpected value for nTeam" ); return 0; } }
	int GetRoundsPlayed( void ) const		{ return m_actualRoundsPlayed; }	
	
	//Since the teams change in halftime modes and we want to retain their scores, we swap the scores
	//between halves.
	void SwapTeamScores( void );
	
	int GetWinningTeam( void );	

	//These are the internal functions that actually mess with the scores, adjusting the appropriate phase-specific scores as well.
	void AddTerroristScore( int score );
	void AddCTScore( int score );
	void GoToOvertime( int numOvertimesToAdd );

private:
	//This is called anytime the match-internal scores are updated to reflect the changes in the team object (so the scores can be replicated)
	void UpdateTeamScores( void );

	// Called when we wish to change the full all-talk rules, based on entering a specific phase of the match
	void EnableFullAlltalk( bool bEnable );

	//This is the number of rounds that have been played, regardless of the actual score of the match (e.g. Demolition mode can give bonus points)
	short m_actualRoundsPlayed;

	// This is the index of the overtime that is being played, 0 when game has no overtime or is still in regulation time, 1 for first overtime, 2 for second, etc.
	short m_nOvertimePlaying;

	short m_ctScoreFirstHalf;
	short m_ctScoreSecondHalf;
	short m_ctScoreOvertime;
	short m_ctScoreTotal;

	short m_terroristScoreFirstHalf;
	short m_terroristScoreSecondHalf;
	short m_terroristScoreOvertime;
	short m_terroristScoreTotal;

	GamePhase m_phase;
};

class SpawnPoint : public CServerOnlyPointEntity
{
	DECLARE_CLASS( SpawnPoint, CServerOnlyPointEntity );
	DECLARE_DATADESC();
public:
	SpawnPoint();
	void Spawn_Impl( void );
	bool IsEnabled() { return m_bEnabled; }
	[[= ks::reflect::Input{ .name = "SetEnabled", .type = FIELD_VOID } ]] void InputSetEnabled( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetDisabled", .type = FIELD_VOID } ]] void InputSetDisabled( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ToggleEnabled", .type = FIELD_VOID } ]] void InputToggleEnabled( inputdata_t &inputdata );
	void SetSpawnEnabled( bool bEnabled );

	[[= ks::reflect::Key{ .name = "priority" } ]] int		m_iPriority;
	[[= ks::reflect::Key{ .name = "enabled" } ]] bool	m_bEnabled;
	int		m_nType;

	enum Type
	{
		Default = 0,
		Deathmatch = 1,
		ArmsRace = 2,
	};
};

class SpawnPointCoopEnemy : public SpawnPoint
{
	DECLARE_CLASS( SpawnPointCoopEnemy, SpawnPoint );
	DECLARE_DATADESC();
public:	
	SpawnPointCoopEnemy();
	void	Spawn_Impl( void );
	void	Precache_Impl( void );
	const char *GetWeaponsToGive( void ) { return STRING( m_szWeaponsToGive ); }
	const char *GetPlayerModelToUse( void ) { return STRING( m_szPlayerModelToUse ); }
	int			GetArmorToSpawnWith( void ) { return m_nArmorToSpawnWith; }
	bool		ShouldStartAsleep( void ) { return m_bStartAsleep; }
	int			GetBotDifficulty( void ) { return m_nBotDifficulty; }
	bool		IsBotAgressive( void ) { return m_bIsAgressive; }
	CNavArea	*FindNearestArea( void );
	float HideRadius() const { return m_flHideRadius; }
	void HideRadius(bool val) { m_flHideRadius = val; }

	enum BotDefaultBehavior_t
	{
		DEFEND_AREA = 0,
		HUNT,
		CHARGE_ENEMY,
		DEFEND_INVESTIGATE,
	};
	BotDefaultBehavior_t GetDefaultBehavior() const { return m_nDefaultBehavior; }
	void SetDefaultBehavior( BotDefaultBehavior_t val ) { m_nDefaultBehavior = val; }

protected:
	[[= ks::reflect::Key{ .name = "weapons_to_give" } ]] string_t	m_szWeaponsToGive;
	[[= ks::reflect::Key{ .name = "model_to_use" } ]] string_t	m_szPlayerModelToUse;
	[[= ks::reflect::Key{ .name = "armor_to_give" } ]] int			m_nArmorToSpawnWith;
	[[= ks::reflect::Key{ .name = "default_behavior" } ]] BotDefaultBehavior_t m_nDefaultBehavior;
	[[= ks::reflect::Key{ .name = "bot_difficulty" } ]] int			m_nBotDifficulty;
	[[= ks::reflect::Key{ .name = "is_agressive" } ]] bool		m_bIsAgressive;
	[[= ks::reflect::Key{ .name = "start_asleep" } ]] bool		m_bStartAsleep;
	[[= ks::reflect::Key{ .name = "hide_radius" } ]] float		m_flHideRadius;
	CNavArea	*m_pMyArea;
};

namespace DT_CSGameRules { extern SendTable g_SendTable; }
void *SendProxy_CSGameRules( const SendProp *pProp, const void *pStruct,
    const void *pVarData, CSendProxyRecipients *pRecipients,
    int objectID );

class CCSGameRulesProxy : public CGameRulesProxy
{
	[[= ks::reflect::NetTable{ .name = "DT_CSGameRulesProxy" } ]]
	[[= ks::reflect::SubTable<"cs_gamerules_data", &DT_CSGameRules::g_SendTable, SendProxy_CSGameRules, true>{} ]]
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

	void StartWarmup( void );
	void EndWarmup( void );

	virtual bool IsTeamChangeSilent( CBasePlayer *pPlayer, int iTeamNum, bool bAutoTeam, bool bSilent ) { return bSilent || m_bForceTeamChangeSilent; }

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

	int GetCoopWaveNumber( void ) { return m_nGuardianModeWaveNumber; }
	CGameCoopMissionManager *GetCoopMissionManager( void );
	EHANDLE m_coopMissionManager;

	void SetCoopMissionManager( CBaseEntity *pPoint ) { m_coopMissionManager = pPoint; }

	void CoopSetBotQuotaAndRefreshSpawns( int nMaxEnemiesToSpawn );
	void CoopMissionSetNextRespawnIn( float flSeconds, bool bIncrementWaveNumber );
	void CoopMissionSpawnFirstEnemies( int nMaxEnemiesToSpawn );
	void CoopMissionSpawnNextWave( int nMaxEnemiesToSpawn );
	void CoopMissionRespawnDeadPlayers( void );
	void CoopCollectBonusCoin( void );

	virtual bool OnReplayPrompt( CBasePlayer *pVictim, CBasePlayer *pScorer ) override;
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

	bool CheckGotGuardianModeSpecialKill( CWeaponCSBase* pAttackerWeapon );

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

	int m_coopBonusCoinsFound;
	bool m_coopBonusPistolsOnly;
	bool m_coopPlayersInDeploymentZone;

	DECLARE_SERVERCLASS_NOBASE(); // This makes datatables able to access our private vars.
	
	CCSGameRules();
	virtual ~CCSGameRules();

	virtual void RefreshSkillData( bool forceUpdate );

	void DumpTimers( void ) const;	// debugging to help track down a stuck server (rare?)

	CBaseEntity *GetPlayerSpawnSpot( CBasePlayer *pPlayer );

	static void EndRound();

	virtual void PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	virtual void Think();

	void SwitchTeamsAtRoundReset( void );

	void ClearGunGameData( void );	

	void FreezePlayers( void );

	// Called at the end of GameFrame (i.e. after all game logic has run this frame)
	virtual void EndGameFrame( void );

	// Called when game rules are destroyed by CWorld
	virtual void LevelShutdown( void );

	void UpdateTeamClanNames( int nTeam );
	void UpdateTeamPredictions();

	// vscript function
	void	SetPlayerCompletedTraining( bool bCompleted );
	bool	GetPlayerCompletedTraining( void );
	void	SetBestTrainingCourseTime( int nTime );
	int		GetBestTrainingCourseTime( void );
	int		GetValveTrainingCourseTime( void );
	bool	IsLocalPlayerUsingController( void );
	void	TrainingGivePlayerAmmo( void );
	void	TrainingSetRadarHidden( bool bHide );
	void	TrainingSetMiniScoreHidden(  bool bHide );
	void	TrainingHighlightAmmoCounter( void );
	void	TrainingShowFinishMsgBox( void );
	void	TrainingShowExitDoorMsg( void );

	virtual void RegisterScriptFunctions( void );
	virtual bool ClientCommand( CBaseEntity *pEdict, const CCommand &args );
	virtual void PlayerSpawn( CBasePlayer *pPlayer );
			void ShowSpawnPoints( int duration );

	virtual void SpawningLatePlayer( CCSPlayer* pLatePlayer );

	bool IsPistolRound( void );

	void HostageKilled( void ) { m_hostageWasKilled = true; }
	void HostageInjured( void ) { m_hostageWasInjured = true; }

	bool WasHostageKilled( void ) { return m_hostageWasKilled; }
	bool WasHostageInjured( void ) { return m_hostageWasInjured; }

	void PlayerTookDamage( CCSPlayer* player, const CTakeDamageInfo &damageInfo );

	void SendKickBanToGC( CCSPlayer *pPlayer, EMsgGCCStrike15_v2_MatchmakingKickBanReason_t eReason );
	void SendKickBanToGCforAccountId( uint32 uiAccountId, EMsgGCCStrike15_v2_MatchmakingKickBanReason_t eReason );

	virtual bool PlayTextureSounds( void ) { return true; }
	// Let the game rules specify if fall death should fade screen to black
	virtual bool  FlPlayerFallDeathDoesScreenFade( CBasePlayer *pl ) { return false; }

	virtual void  RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, CBaseEntity *pEntityIgnore );
	void RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, bool bIgnoreWorld );

	virtual void UpdateClientData( CBasePlayer *pl );
	
	virtual CCSPlayer* CheckAndAwardAssists( CCSPlayer* pCSVictim, CCSPlayer* pKiller );

	// Death notices
	virtual void		DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	IGameEvent * CreateWeaponKillGameEvent( char const *szEventName, const CTakeDamageInfo &info );

	virtual void			InitDefaultAIRelationships( void );

	virtual const char *GetGameDescription( void ) { return "Counter-Strike: Global Offensive"; }  // this is the game name that gets seen in the server browser
	virtual const char *AIClassText(int classType);

	virtual bool FShouldSwitchWeapon( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon );

	virtual const char *SetDefaultPlayerTeam( CBasePlayer *pPlayer );

	// Called before entities are created
	virtual void LevelInitPreEntity();

	// Called after the map has finished loading.
	virtual void LevelInitPostEntity();

	virtual float FlPlayerFallDamage( CBasePlayer *pPlayer );

	virtual bool ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen );
	virtual void ClientDisconnected( edict_t *pClient );

	virtual void ClientCommandKeyValues( edict_t *pEntity, KeyValues *pKeyValues );

	virtual int GetMaxHumanPlayers() const;

	// Recreate all the map entities from the map data (preserving their indices),
	// then remove everything else except the players.
	// Also get rid of all world decals.
	void CleanUpMap();

	void CheckFreezePeriodExpired();
	void CheckRoundTimeExpired();

	// check if the scenario has been won/lost
	// return true if the scenario is over, false if the scenario is still in progress
	bool CheckWinConditions( void );	

	// this is used by entities like X and increments the round and ends it
	void IncrementAndTerminateRound( float tmDelay, int reason );

	void TerminateRound( float tmDelay, int reason );

	void ProcessEndOfRoundAchievements( int iWinnerTeam, int iReason );

	void SaveRoundDataInformation( char const *szFilenameOverride = nullptr );
	void LoadRoundDataInformation( char const *szFilename );

	void ResetMasterSpawnPointsForCoop( void );
	// The following round-related functions are called as follows:
	//
	// At Match Start:
	//		PreRestartRound() -> RestartRound() -> PostRestartRound()
	//
	// During Subsequent Round Gameplay:
	//		RoundWin() is called at the point when the winner of the round has been determined - prior to free-play commencing
	//		PreRestartRound() is called with 1 second remaining prior to the round officially ending (This is after a round
	//						  winner has been chosen and players are allowed to continue playing)
	//		RoundEnd() is then called when the round has completely ended
	//		RestartRound() is then called immediately after RoundEnd()
	//		PostRestartRound() is called immediately after RestartRound() has completed
	void PreRestartRound( void );
	void RestartRound( void );
	void PostRestartRound( void );
	void RoundWin( void );
	void RoundEnd( void );
	int	 GetRoundsPlayed( void ) const		{ return m_match.GetRoundsPlayed(); }	

	void BalanceTeams( void );
	void HandleScrambleTeams( void );
	void HandleSwapTeams( void );
	void MoveHumansToHumanTeam( void );
	bool TeamFull( int team_id );
	int	 MaxNumPlayersOnTerrTeam();
	int  MaxNumPlayersOnCTTeam();

	bool WillTeamHaveRoomForPlayer( CCSPlayer* pPlayer, int newTeam );

	bool TeamStacked( int newTeam_id, int curTeam_id  );
	bool FPlayerCanRespawn( CBasePlayer *pPlayer );
	void UpdateTeamScores();
	void CheckMapConditions();
	void MarkLivingPlayersOnTeamAsNotReceivingMoneyNextRound(int team);

	// Check various conditions to end the map.
	bool CheckGameOver();	
	bool CheckWinLimit();
	bool CheckFragLimit();

	void CheckLevelInitialized();
	void CheckRestartRound();

	virtual bool FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker );
	virtual int IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled );

	bool CanPlayerHearTalker( CBasePlayer* pListener, CBasePlayer *pSpeaker, bool bTeamOnly );
	virtual int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget );
	virtual bool PlayerCanHearChat( CBasePlayer *pListener, CBasePlayer *pSpeaker, bool bTeamOnly );

	void GuardianUpdateBotAccountAndWeapons( CCSPlayer *pBot );
	void GiveGuardianBotGrenades( CCSPlayer *pBot );
	
	// repsawning
	void SetTeamRespawnWaveTime( int iTeam, float flValue ); 
	virtual bool HasPassedMinRespawnTime( CBasePlayer *pPlayer );
	void	CheckRespawnWaves( void );

	// Checks if it still needs players to start a round, or if it has enough players to start rounds.
	// Starts a round and returns true if there are enough players.
	bool NeededPlayersCheck( bool &bNeededPlayers );

	// Setup counts for m_iNumTerrorist, m_iNumCT, m_iNumSpawnableTerrorist, m_iNumSpawnableCT, etc.
	void InitializePlayerCounts(
		int &NumAliveTerrorist,
		int &NumAliveCT,
		int &NumDeadTerrorist,
		int &NumDeadCT
		);

	// Check to see if the round is over for the various game types. Terminates the round
	// and returns true if the round should end.
	bool PrisonRoundEndCheck();
	bool BombRoundEndCheck( bool bNeededPlayers );
	bool HostageRescueRoundEndCheck( bool bNeededPlayers );
	bool BombPlantedRoundEndCheck();
	bool CTsReachedHostageRoundEndCheck();
	bool GuardianAllKillsAchievedCheck();
	bool m_bHasHostageBeenTouched;

	CCSPlayer* CalculateEndOfRoundMVP();

	// Check to see if the teams exterminated each other. Ends the round and returns true if so.
	bool TeamExterminationCheck(
		int NumAliveTerrorist,
		int NumAliveCT,
		int NumDeadTerrorist,
		int NumDeadCT,
		bool bNeededPlayers
		);

	void ReadMultiplayCvars();
	void SwapAllPlayers();

	void OnTeamsSwappedAtRoundReset();

	void ResetForTradeshow( void );		// reset player scores, reset team scores, reset player controls, restart round; used for demos

	void BroadcastSound( const char *sound, int team = -1 );

	// GUN GAME PROGRESSIVE FUNCTION
	bool GunGameProgressiveEndCheck( void );

	// BOMB MAP FUNCTIONS
	void GiveC4ToRandomPlayer();
	void GiveDefuserToRandomPlayer();
	CCSPlayer *IsThereABomber();
	bool IsThereABomb();

	// HOSTAGE MAP FUNCTIONS
	void HostageTouched();

	// Contribution score helpers
	void ScorePlayerKill( CCSPlayer* pPlayer );
	void ScorePlayerAssist( CCSPlayer* pPlayer, CCSPlayer* pCSVictim );
	void ScorePlayerObjectiveKill( CCSPlayer* pPlayer );
	void ScorePlayerSuicide( CCSPlayer* pPlayer );
	void ScorePlayerTeamKill( CCSPlayer* pPlayer );
	void ScorePlayerDamage( CCSPlayer* pPlayer, float fDamage );
	void ScoreBombPlant( CCSPlayer* pPlayer );
	void ScoreBombExploded( CCSPlayer* pPlayer );
	void ScoreBombDefuse( CCSPlayer* pPlayer, bool bMajorEvent );
	void ScoreHostageRescue( CCSPlayer* pPlayer, CHostage* pHostage, bool bMajorEvent );
	void ScoreHostageKilled( CCSPlayer* pPlayer );
	void ScoreHostageDamage( CCSPlayer* pPlayer, float fDamage );
	void ScoreFriendlyFire( CCSPlayer* pPlayer, float fDamage );
	void ScoreBlindEnemy( CCSPlayer* pPlayer );
	void ScoreBlindFriendly( CCSPlayer* pPlayer );
	
	// Sets up g_pPlayerResource.
	virtual void CreateStandardEntities();
	virtual const char *GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer );
	virtual const char *GetChatLocation( bool bTeamOnly, CBasePlayer *pPlayer );
	virtual const char *GetChatFormat( bool bTeamOnly, CBasePlayer *pPlayer );
	void ClientSettingsChanged( CBasePlayer *pPlayer );

	virtual bool CanClientCustomizeOwnIdentity() override;
	
	bool IsCareer( void ) const		{ return false; }		// returns true if this is a CZ "career" game

	virtual bool FAllowNPCs( void );	

	bool CheckSetVoteTime();

	void GoToMatchRestartIntermission();

	CCSMatch* GetMatch( void );
			   
	// Let's the match store recplicated vars in the game rules.
	void SetGamePhase( GamePhase newPhase ) { m_gamePhase = newPhase; }
	void SetTotalRoundsPlayed( int roundsPlayed ) { m_totalRoundsPlayed = roundsPlayed; }
	void SetOvertimePlaying( int nOvertimePlaying ) { m_nOvertimePlaying = nOvertimePlaying; }

	void SetScrambleTeamsOnRestart( bool scramble ) { m_bScrambleTeamsOnRestart = scramble; }
	bool GetScrambleTeamsOnRestart( void ) { return m_bScrambleTeamsOnRestart; }

	void SetSwapTeamsOnRestart( bool swapTeams ) { m_bSwapTeamsOnRestart = swapTeams; }
	bool GetSwapTeamsOnRestart( void ) { return m_bSwapTeamsOnRestart; }

	bool GameModeSupportsHealthBuffer( void );

protected:
	// these functions cover recording and sending item drops for display in game modes where you don't allow drops during the match/round
	void SendPlayerItemDropsToClient( void );
	bool m_bPlayerItemsHaveBeenDisplayed;

	void RewardMatchEndDrops( bool bAbortedMatch );
	virtual void GoToIntermission( bool bAbortedMatch = false );

	void UpdateMatchStats( CCSPlayer* pPlayer, int winnerIndex ) ;

	static void SplitScoreAmongPlayersInZone( int iPoints, int iTeam, CCSPlayer* pExcludePlayer, uint iPlace );
	static void SplitScoreAmongPlayersInRange( int iPoints, int iTeam, CCSPlayer* pExcludePlayer, const Vector& center, float fRangeInner, float fRangeOuter );

	void GotoTRBombModeHalftime( void );
	void EndTRBombModeHalftime( void );

	float CalculateAveragePlayerContributionScore( void );
	float CalculateAverageBotContributionScore( void );

public:
	void ModifyRealtimeBotDifficulty( CCSPlayer* pOnlyBotToProcess = nullptr );

public:

	bool IsFriendlyFireOn() const;

	bool	IsLastRoundBeforeHalfTime( void );

	virtual void	SetAllowWeaponSwitch( bool allow );
	virtual bool	GetAllowWeaponSwitch( void );

	bool			ShouldGunGameSpawnBomb( void );
	void			SetGunGameSpawnBomb( bool allow );

	bool			IsClanTeam( CTeam *pTeam );

	//int				GetEndMatchCurrentMapVotes();

	// VARIABLES FOR ALL TYPES OF MAPS
	bool m_bLevelInitialized;	
	int m_iTotalRoundsPlayed;
	int m_iUnBalancedRounds;	// keeps track of the # of consecutive rounds that have gone by where one team outnumbers the other team by more than 2
	bool m_endMatchOnRoundReset;
	bool m_endMatchOnThink;
	
	float m_flCoopRespawnAndHealTime;

	// GAME TIMES
	int m_iFreezeTime;		// (From mp_freezetime) - How many seconds long the intro round (when players are frozen) is.

	int m_iNumTerrorist;		// The number of terrorists on the team (this is generated at the end of a round)
	int m_iNumCT;				// The number of CTs on the team (this is generated at the end of a round)
	int m_iNumSpawnableTerrorist;
	int m_iNumSpawnableCT;
	std::vector< int > m_arrSelectedHostageSpawnIndices; // The indices of hostage spawn locations selected for the match

	bool m_bFirstConnected;
	bool m_bCompleteReset;		// Set to true to have the scores reset next time round restarts
	bool m_bPickNewTeamsOnReset;
	bool m_bScrambleTeamsOnRestart;
	bool m_bSwapTeamsOnRestart;

	enum EEndMatchMapVoteState_t
	{
		k_EEndMatchMapVoteState_MatchInProgress,
		k_EEndMatchMapVoteState_VoteInProgress,
		k_EEndMatchMapVoteState_VoteTimeEnded,
		k_EEndMatchMapVoteState_AllPlayersVoted,
		k_EEndMatchMapVoteState_SelectingWinner,
		k_EEndMatchMapVoteState_SettingNextLevel,
		k_EEndMatchMapVoteState_VoteAllDone,
	};

	EEndMatchMapVoteState_t m_eEndMatchMapVoteState;
	int m_nEndMatchMapVoteWinner;
	std::vector< int >	m_nEndMatchTiedVotes;
	void CreateEndMatchMapGroupVoteOptions( void );

	enum EQueuedMatchmakingRematchState_t
	{
		k_EQueuedMatchmakingRematchState_MatchInProgress,
		k_EQueuedMatchmakingRematchState_VoteStarting,
		k_EQueuedMatchmakingRematchState_VoteToRematchInProgress,
		k_EQueuedMatchmakingRematchState_VoteToRematchSucceeded,
		k_EQueuedMatchmakingRematchState_VoteToRematchFailed,
		k_EQueuedMatchmakingRematchState_VoteToRematchFailed_Done,
		k_EQueuedMatchmakingRematchState_VoteToRematch_Done,
		k_EQueuedMatchmakingRematchState_VoteToRematch_T_Surrender,
		k_EQueuedMatchmakingRematchState_VoteToRematch_CT_Surrender,
		k_EQueuedMatchmakingRematchState_VoteToRematch_Aborted,
	};
	EQueuedMatchmakingRematchState_t m_eQueuedMatchmakingRematchState;
	bool m_bNeedToAskPlayersForContinueVote;
	uint32 m_numQueuedMatchmakingAccounts;
	char *m_pQueuedMatchmakingReservationString;
	uint32 m_numTotalTournamentDrops;
	uint32 m_numSpectatorsCountMax;
	uint32 m_numSpectatorsCountMaxTV;
	uint32 m_numSpectatorsCountMaxLnk;
	ks::net::CMsgGCCStrike15_v2_MatchmakingServerRoundStats *m_pQueuedMatchmakingReportedRoundStats;
	static ks::net::CMsgGCCStrike15_v2_MatchmakingGC2ServerReserve sm_QueuedServerReservation;
	void ReportRoundEndStatsToGC( ks::net::CMsgGCCStrike15_v2_MatchmakingServerRoundStats **ppAllocateStats = nullptr );

	struct CQMMPlayerData_t
	{
		CQMMPlayerData_t()
		{
			V_memset( this, 0, __builtin_offsetof( CQMMPlayerData_t, m_uiCustomNonPodFields ) );
		}
		void Reset()
		{	// Reset everything starting from m_numKills to the end
			V_memset( ( ( char * ) this ) + __builtin_offsetof( CQMMPlayerData_t, m_numKills ), 0,
				__builtin_offsetof( CQMMPlayerData_t, m_uiCustomNonPodFields ) - __builtin_offsetof( CQMMPlayerData_t, m_numKills ) );
		}

		uint32 m_uiPlayerAccountId;		// QMM player account ID
		int m_iDraftIndex;				// Index of the player in the draft [0-4: team 1; 5-9: team 2]
		uint32 m_msDisconnectionTimestamp; // Timestamp of player's disconnection if not 0
		uint32 m_uiAbandonRecordedReason; // We recorded the fact that the player abandoned the match and this is the reason
		bool m_bAbandonAllowsSurrender;	// This player abandoning the match allows for his teammates to surrender
		char m_chPlayerName[128];		// Last known player name
		bool m_bEverFullyConnected;			// Has this player ever connected to the server
		bool m_bDisconnection1MinWarningPrinted;	// Whether 1 minute warning has been printed

		// RESETTABLE SECTION:
		int m_numKills;					// number of kills
		int m_numAssists;				// number of assists
		int m_numDeaths;				// number of deaths
		int m_numScorePoints;			// number of score points
		int m_numMVPs;					// number of MVP rounds
		int m_cash;						// player's cash
		int m_numTeamKills;				// number of team kills
		int m_numTeamDamagePoints;		// number of team damage points
		int m_numHostageKills;			// number of hostages killed
		int m_numEnemyKills;			// number of enemies killed
		int m_numEnemyKillHeadshots;	// number of enemies killed with headshot
		int m_numEnemy3Ks;				// number of 3Ks
		int m_numEnemy4Ks;				// number of 4Ks
		int m_numEnemy5Ks;				// number of 5Ks
		int m_numEnemyKillsAgg;			// number of enemies killed on aggregate
		int m_numRoundsWon;				// rounds won
		int m_numFirstKills;			// number of times this player got first kill of the round
		int m_numClutchKills;			// number of times this player got a kill with no teammates alive
		int m_numPistolKills;			// number of pistol kills this player got
		int m_numSniperKills;			// number of sniper kills this player got
		int m_numHealthPointsRemovedTotal;	// total number of health points removed (used for coop gameplay)
		int m_numHealthPointsDealtTotal;	// total number of health points dmg dealt to enemies (used for coop gameplay)
		int m_numShotsFiredTotal;			// total number of shots fired (incremented by one every time player pulls trigger, not per each shotgun pellet; used for coop gameplay)
		int m_numShotsOnTargetTotal;		// total number of shots on target (incremented by no moer than one per each time player pulls trigger, not per each shotgun pellet; used for coop gameplay)

		bool m_bReceiveNoMoneyNextRound;	// player is marked to not receive any money next round

		// match stat data for the spectator graphs
		int m_iMatchStats_Kills[MAX_MATCH_STATS_ROUNDS];		
		int m_iMatchStats_Damage[MAX_MATCH_STATS_ROUNDS];	
		int m_iMatchStats_EquipmentValue[MAX_MATCH_STATS_ROUNDS];	
		int m_iMatchStats_MoneySaved[MAX_MATCH_STATS_ROUNDS];	
		int m_iMatchStats_KillReward[MAX_MATCH_STATS_ROUNDS];
		int m_iMatchStats_LiveTime[MAX_MATCH_STATS_ROUNDS];
		int m_iMatchStats_Deaths[ MAX_MATCH_STATS_ROUNDS ];
		int m_iMatchStats_Assists[ MAX_MATCH_STATS_ROUNDS ];
		int m_iMatchStats_HeadShotKills[ MAX_MATCH_STATS_ROUNDS ];
		int m_iMatchStats_Objective[ MAX_MATCH_STATS_ROUNDS ];
		int m_iMatchStats_CashEarned[ MAX_MATCH_STATS_ROUNDS ];
		int m_iMatchStats_UtilityDamage[ MAX_MATCH_STATS_ROUNDS ];
		int m_iMatchStats_EnemiesFlashed[ MAX_MATCH_STATS_ROUNDS ];

		// per weapon kills
		uint32 m_uiCustomNonPodFields;

		// END RESETTABLE SECTION

		std::map< uint32, uint32 > m_mapQuestEventPoints;

		// TODO: If we have more attributes we want to be processed with the timed rewards job, try to generify this
		typedef std::map< itemid_t, attrib_value_t > StattrakMusicKitValues_t;
		StattrakMusicKitValues_t m_mapMusicKitUpdates;

		// once the player purchases an econ item, cache it and rebuy it from the cache.
		typedef std::map< uint16, item_definition_index_t > LoadoutSlotToDefIndexMap_t;
		LoadoutSlotToDefIndexMap_t m_mapLoadoutSlotToItem[ 2 ];

	};
	typedef std::map< uint32, std::unique_ptr< CQMMPlayerData_t > > QueuedMatchmakingPlayersDataMap_t;
	QueuedMatchmakingPlayersDataMap_t m_mapQueuedMatchmakingPlayersData;
	CQMMPlayerData_t * QueuedMatchmakingPlayersDataFind( uint32 uiAccountID ) const
	{
		if ( !uiAccountID ) return nullptr;
		auto it = m_mapQueuedMatchmakingPlayersData.find( uiAccountID );
		return ( it == m_mapQueuedMatchmakingPlayersData.end() ) ? nullptr : it->second.get();
	}
	CQMMPlayerData_t * QueuedMatchmakingPlayersDataFindOrCreate( CCSPlayer *pPlayer );

	bool m_bForceTeamChangeSilent;
	bool m_bLoadingRoundBackupData;

	class ICalculateEndOfRoundMVPHook_t
	{
	public:
		virtual CCSPlayer* CalculateEndOfRoundMVP() = 0;
	};
	ICalculateEndOfRoundMVPHook_t *m_pfnCalculateEndOfRoundMVPHook;

	typedef std::map< int32, uint32 > MapMatchInfoShownCounts;
	MapMatchInfoShownCounts m_mapMatchInfoShownCounts; // tracks how many times each match info piece was shown
	enum MapMatchInfoShownCountsSpecial_t
	{
		k_MapMatchInfoShownCounts_None = 0xF0000,
		k_MapMatchInfoShownCounts_Predictions = 0xF0001,
		k_MapMatchInfoShownCounts_MapDraft = 0xF0002,
	};
	int m_nMatchInfoShowType; // which match info is preferred to be shown
	float m_flMatchInfoDecidedTime; // what curtime moment was when match info was decided

	struct ServerPlayerDecalData_t
	{
		ServerPlayerDecalData_t() { V_memset( this, 0, sizeof(*this) ); }
		bool operator == ( ServerPlayerDecalData_t const &x ) const { return !V_memcmp( this, &x, __builtin_offsetof( ServerPlayerDecalData_t, m_rtGcTime ) ); }

		AccountID_t m_unAccountID;
		int m_nTraceID;

		Vector m_vecOrigin;
		Vector m_vecStart;
		Vector m_vecRight;
		Vector m_vecNormal;
		int m_nEquipSlot;
		int m_nPlayer;
		int m_nEntity;
		int m_nHitbox;
		int m_nTintID;
		float m_flCreationTime;
		uint32 m_rtGcTime;	// not participating in memcmp (set by GC, authoritative)

		void CopyToMsg( ks::net::CCSUsrMsg_PlayerDecalDigitalSignature &msg ) const;
		void InitFromMsg( ks::net::CCSUsrMsg_PlayerDecalDigitalSignature const &msg );
	};
	std::vector< ServerPlayerDecalData_t > m_arrServerPlayerDecalData;

	int m_iAccountTerrorist;
	int m_iAccountCT;

	int m_iNumConsecutiveCTLoses;		//SupraFiend: the number of rounds the CTs have lost in a row.
	int m_iNumConsecutiveTerroristLoses;//SupraFiend: the number of rounds the Terrorists have lost in a row.

	int m_iSpawnPointCount_Terrorist;		// Number of Terrorist spawn points
	int m_iSpawnPointCount_CT;				// Number of CT spawn points

	int m_iMaxNumTerrorists;
	int m_iMaxNumCTs;

	int m_iLoserBonus;			// SupraFiend: the amount of money the losing team gets. This scales up as they lose more rounds in a row
	float m_tmNextPeriodicThink;
	
	bool m_bVoiceWonMatchBragFired;

	float m_fWarmupNextChatNoticeTime;

	CHandle<CCSPlayer> m_pMVP;
	
	// HOSTAGE RESCUE VARIABLES
	int		m_iHostagesRescued;
	int		m_iHostagesTouched;
	float	m_flNextHostageAnnouncement;

	// [tj] Accessor for weapons donation ability
	bool GetCanDonateWeapon( void ) { return m_bCanDonateWeapons; }

	// [tj] flawless and lossless round related flags
	bool m_bNoTerroristsKilled;
	bool m_bNoCTsKilled;
	bool m_bNoTerroristsDamaged;
	bool m_bNoCTsDamaged;
	bool m_bNoEnemiesKilled;

	// [tj] Find out if dropped weapons count as donations
	bool m_bCanDonateWeapons;

	// [tj] Keep track of first kill
	CHandle<CCSPlayer> m_pFirstKill;
	float m_firstKillTime;

	// [menglish] Keep track of first blood
	CHandle<CCSPlayer> m_pFirstBlood;
	float m_firstBloodTime;

	// [dwenger] Rescue-related achievement values
	std::vector< CHandle<CCSPlayer> > m_arrRescuers;

	bool m_hostageWasInjured;
	bool m_hostageWasKilled;

	// [menglish] Fun Fact Manager
	CCSFunFactMgr *m_pFunFactManager;

	// Automatic vote called near the end of a map
	bool	m_bVoteCalled;
	bool	m_bServerVoteOnReset;
	float	m_flVoteCheckThrottle;

	// [tj] To avoid rewriting the same piece of code, we can get all the information
	//		we want from one call that fills in an array of structures.
	struct TeamPlayerCounts
	{
		int totalPlayers;
		int totalAlivePlayers;
		int totalDeadPlayers; //sum of killedPlayers + suicidedPlayers + unenteredPlayers
		int killedPlayers;
		int suicidedPlayers;
		int unenteredPlayers;
	};

	void GetPlayerCounts( TeamPlayerCounts teamCounts[TEAM_MAXCOUNT] );

	bool m_bBuyTimeEnded;

	int m_nLastFreezeEndBeep;

	// PRISON ESCAPE VARIABLES
	int		m_iHaveEscaped;
	bool	m_bMapHasEscapeZone;
	int		m_iNumEscapers;			
	int		m_iNumEscapeRounds;		// keeps track of the # of consecutive rounds of escape played.. Teams will be swapped after 8 rounds

	// BOMB MAP VARIABLES
	bool	m_bTargetBombed;	// whether or not the bomb has been bombed
	bool	m_bBombDefused;	// whether or not the bomb has been defused
	bool	m_bMapHasBombZone;
/*	bool	m_bBombPlanted;*/

	bool	m_bGunGameRespawnWithBomb;	// Whether or not the next terrorist to spawn should have the bomb
	float	m_fGunGameBombRespawnTimer;	// Time until the bomb can be respawned

	void			AddTeamAccount( int team, TeamCashAward::Type reason );
	void			AddTeamAccount( int team, TeamCashAward::Type reason, int amount, const char* szAwardText = nullptr );

public:
	CBaseEntity* GetNextSpawnpoint( int teamNumber );

	void DoCoopSpawnAndNavInit( void );
	void AddSpawnPointToMasterList( SpawnPoint* pSpawnPoint );
	void GenerateSpawnPointListsFirstTime( void );
	void RefreshCurrentSpawnPointLists( void );

	CCSMatch		m_match;

	Vector	m_vecMainCTSpawnPos;

	void ShuffleSpawnPointLists( void );
	void ShuffleMasterSpawnPointLists( void );
	void SortSpawnPointLists( void );
	void SortMasterSpawnPointLists( void );
	void ShufflePlayerList( std::vector< CCSPlayer* > &playersList );
	
protected:
	std::vector< SpawnPoint* >	m_CTSpawnPointsMasterList;			// The master list of CT spawn points (contains all points whether enabled or disabled)
	std::vector< SpawnPoint* >	m_TerroristSpawnPointsMasterList;	// The master list of Terrorist spawn points (contains all points whether enabled or disabled)

	int m_iNextCTSpawnPoint;						// Used when picking the next CT spawn point to assign
	int m_iNextTerroristSpawnPoint;					// Used when picking the next Terrorist spawn point to assign
	std::vector< SpawnPoint* >	m_CTSpawnPoints;		// List of CT spawn points sorted by their priorities
	std::vector< SpawnPoint* >	m_TerroristSpawnPoints;	// List of Terrorist spawn points sorted by their priorities

private:
	
	float m_fAutobalanceDisplayTime;
	AutobalanceStatus::Type m_AutobalanceStatus;
	CRecipientFilter m_AutoBalanceTraitors;
	CRecipientFilter m_AutoBalanceLoyalists;

	// Don't allow switching weapons while gaining new technologies
	bool			m_bAllowWeaponSwitch;

	bool			m_bRoundTimeWarningTriggered;
	
	float			m_phaseChangeAnnouncementTime;
	float			m_fNextUpdateTeamClanNamesTime;

	float			m_flLastThinkTime;

	CRecipientFilter m_filterTerrorist;
	CRecipientFilter m_filterCT;

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

//-----------------------------------------------------------------------------
// Purpose: Useful utility functions
//-----------------------------------------------------------------------------
	
	class CTFTeam;
	CTFTeam *GetOpposingTeam( CTeam *pTeam );
	bool EntityPlacementTest( CBaseEntity *pMainEnt, const Vector &vOrigin, Vector &outPos, bool bDropToGround, unsigned int mask = MASK_SOLID, ITraceFilter *pFilter = nullptr );

// Assassination quest helper funcs
bool IsAssassinationQuest( const CEconQuestDefinition *pQuest );
bool IsAssassinationQuest( uint32 questID );
bool IsAssassinationQuestActive( const CEconQuestDefinition *pQuest );

extern const float g_flWarmupToFreezetimeDelay;

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

};

CCSInventoryManager *CSInventoryManager( void );

using namespace ::ks::reflect;

// nav.h
// Data structures and constants for the Navigation Mesh system
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

/**
 * Below are several constants used by the navigation system.
 * @todo Move these into TheNavMesh singleton.
 */
const float BotRadius = 10.0f;				///< circular extent that contains bot

class CSNavNode;

using namespace ::ks::reflect;

// nav_area.h
// Navigation areas
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

using namespace ::ks::reflect;

// cs_nav_mesh.h
// The Navigation Mesh interface
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

//
// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003
//
// NOTE: The Navigation code uses Doxygen-style comments. If you run Doxygen over this code, it will 
// auto-generate documentation.  Visit www.doxygen.org to download the system for free.
//

using namespace ::ks::reflect;

// nav_node.h
// Navigation Nodes are used when generating a Navigation Mesh by point sampling the map
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

// If DEBUG_NAV_NODES is true, nav_show_nodes controls drawing node positions, and
// nav_show_node_id allows you to show the IDs of nodes that didn't get used to create areas.
#define DEBUG_NAV_NODES 0

//--------------------------------------------------------------------------------------------------------------
/**
 * Navigation Nodes.
 * These Nodes encapsulate world locations, and ways to get from one location to an adjacent one.
 * Note that these links are not necessarily commutative (falling off of a ledge, for example).
 */
class CSNavNode;   // declared only: it was never implemented

//--------------------------------------------------------------------------------------------------------------
//
// Inlines
//

#undef DEBUG_NAV_NODES

class CImprovLocomotor;
class CNavArea;
class CNavLadder;

using namespace ::ks::reflect;


// nav_path.h
// Navigation Path encapsulation
// Author: Michael S. Booth (mike@turtlerockstudios.com), November 2003


//--------------------------------------------------------------------------------------------------------
/**
 * The CCSNavPath class encapsulates a path through space
 */
class CCSNavPath
{
public:
	CCSNavPath( void )
	{
		m_segmentCount = 0;
	}

	struct PathSegment
	{
		CNavArea *area;											///< the area along the path
		NavTraverseType how;									///< how to enter this area from the previous one
		Vector pos;												///< our movement goal position at this point in the path
		const CNavLadder *ladder;								///< if "how" refers to a ladder, this is it
	};

	const PathSegment * operator[] ( int i ) const		{ return (i >= 0 && i < m_segmentCount) ? &m_path[i] : nullptr; }
	const PathSegment *GetSegment( int i ) const		{ return (i >= 0 && i < m_segmentCount) ? &m_path[i] : nullptr; }
	int GetSegmentCount( void ) const					{ return m_segmentCount; }
	const Vector &GetEndpoint( void ) const				{ return m_path[ m_segmentCount-1 ].pos; }
	bool IsAtEnd( const Vector &pos ) const;					///< return true if position is at the end of the path

	float GetLength( void ) const;								///< return length of path from start to finish
	bool GetPointAlongPath( float distAlong, Vector *pointOnPath ) const;	///< return point a given distance along the path - if distance is out of path bounds, point is clamped to start/end

	/// return the node index closest to the given distance along the path without going over - returns (-1) if error
	int GetSegmentIndexAlongPath( float distAlong ) const;

	bool IsValid( void ) const	{ return (m_segmentCount > 0); }
	void Invalidate( void )		{ m_segmentCount = 0; }

	void Draw( const Vector &color = Vector( 1.0f, 0.3f, 0 ) );	///< draw the path for debugging

	/// compute closest point on path to given point
	bool FindClosestPointOnPath( const Vector *worldPos, int startIndex, int endIndex, Vector *close ) const;

	void Optimize( void );
	
	/**
	 * Compute shortest path from 'start' to 'goal' via A* algorithm.
	 * If returns true, path was build to the goal position.
	 * If returns false, path may either be invalid (use IsValid() to check), or valid but 
	 * doesn't reach all the way to the goal.
	 */
	template< typename CostFunctor >
	bool Compute( const Vector &start, const Vector &goal, CostFunctor &costFunc )
	{
		Invalidate();

		CNavArea *startArea = TheNavMesh->GetNearestNavArea( start + Vector( 0.0f, 0.0f, 1.0f ) );
		if (startArea == nullptr)
		{
			return false;
		}

		CNavArea *goalArea = TheNavMesh->GetNavArea( goal );

		// if we are already in the goal area, build trivial path
		if (startArea == goalArea)
		{
			BuildTrivialPath( start, goal );
			return true;
		}

		// make sure path end position is on the ground
		Vector pathEndPosition = goal;
		if (goalArea)
		{
			pathEndPosition.z = goalArea->GetZ( pathEndPosition );
		}
		else
		{
			TheNavMesh->GetGroundHeight( pathEndPosition, &pathEndPosition.z );
		}

		//
		// Compute shortest path to goal
		//
		CNavArea *closestArea;
		bool pathResult = NavAreaBuildPath( startArea, goalArea, &goal, costFunc, &closestArea );

		//
		// Build path by following parent links
		//

		// get count
		int count = 0;
		CNavArea *area;
		for( area = closestArea; area; area = area->GetParent() )
		{
			++count;
		}

		// save room for endpoint
		if (count > MAX_PATH_SEGMENTS-1)
		{
			count = MAX_PATH_SEGMENTS-1;
		}

		if (count == 0)
		{
			return false;
		}

		if (count == 1)
		{
			BuildTrivialPath( start, goal );
			return true;
		}

		// build path
		m_segmentCount = count;
		for( area = closestArea; count && area; area = area->GetParent() )
		{
			--count;
			m_path[ count ].area = area;
			m_path[ count ].how = area->GetParentHow();
		}

		// compute path positions
		if (ComputePathPositions() == false)
		{
			//PrintIfWatched( "CNavPath::Compute: Error building path\n" );
			Invalidate();
			return false;
		}

		// append path end position
		m_path[ m_segmentCount ].area = closestArea;
		m_path[ m_segmentCount ].pos = pathEndPosition;
		m_path[ m_segmentCount ].ladder = nullptr;
		m_path[ m_segmentCount ].how = NUM_TRAVERSE_TYPES;
		++m_segmentCount;

		return pathResult;
	}

private:
	enum { MAX_PATH_SEGMENTS = 256 };
	PathSegment m_path[ MAX_PATH_SEGMENTS ];
	int m_segmentCount;

	bool ComputePathPositions( void );									///< determine actual path positions 
	bool BuildTrivialPath( const Vector &start, const Vector &goal );	///< utility function for when start and goal are in the same area

	int FindNextOccludedNode( int anchor );								///< used by Optimize()
};

//--------------------------------------------------------------------------------------------------------
/**
 * Monitor improv movement and determine if it becomes stuck
 */
class CStuckMonitor
{
public:
	CStuckMonitor( void );

	void Reset( void );
	void Update( CImprovLocomotor *improv );
	bool IsStuck( void ) const		{ return m_isStuck; }

	float GetDuration( void ) const	{ return (m_isStuck) ? m_stuckTimer.GetElapsedTime() : 0.0f; }

private:
	bool m_isStuck;													///< if true, we are stuck
	Vector m_stuckSpot;												///< the location where we became stuck
	IntervalTimer m_stuckTimer;										///< how long we have been stuck

	enum { MAX_VEL_SAMPLES = 5 };
	float m_avgVel[ MAX_VEL_SAMPLES ];
	int m_avgVelIndex;
	int m_avgVelCount;
	Vector m_lastCentroid;
	float m_lastTime;
};

//--------------------------------------------------------------------------------------------------------
/**
 * The CNavPathFollower class implements path following behavior
 */
class CNavPathFollower
{
public:
	CNavPathFollower( void );

	void SetImprov( CImprovLocomotor *improv ) { m_improv = improv; }
	void SetPath( CCSNavPath *path ) { m_path = path; }

	void Reset( void );

	void Update( float deltaT, bool avoidObstacles = true );		///< move improv along path
	void Debug( bool status )		{ m_isDebug = status; }			///< turn debugging on/off

	bool IsStuck( void ) const		{ return m_stuckMonitor.IsStuck(); }	///< return true if improv is stuck 
	void ResetStuck( void )			{ m_stuckMonitor.Reset(); }
	float GetStuckDuration( void ) const	{ return m_stuckMonitor.GetDuration(); }	///< return how long we've been stuck

	void FeelerReflexAdjustment( Vector *goalPosition, float height = -1.0f );	///< adjust goal position if "feelers" are touched

private:
	CImprovLocomotor *m_improv;										///< who is doing the path following

	CCSNavPath *m_path;												///< the path being followed

	int m_segmentIndex;												///< the point on the path the improv is moving towards
	int m_behindIndex;												///< index of the node on the path just behind us
	Vector m_goal;													///< last computed follow goal

	bool m_isLadderStarted;

	bool m_isDebug;

	int FindOurPositionOnPath( Vector *close, bool local ) const;	///< return the closest point to our current position on current path
	int FindPathPoint( float aheadRange, Vector *point, int *prevIndex );	///< compute a point a fixed distance ahead along our path.

	CStuckMonitor m_stuckMonitor;
};

class CNavArea;

using namespace ::ks::reflect;


// nav_pathfind.h
// Path-finding mechanisms using the Navigation Mesh
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute travel distance along shortest path from startPos to goalPos. 
 * Return -1 if can't reach endPos from goalPos.
 */
template< typename CostFunctor >
float NavAreaTravelDistance( const Vector &startPos, const Vector &goalPos, CostFunctor &costFunc )
{
	CNavArea *startArea = TheNavMesh->GetNearestNavArea( startPos );
	if (startArea == nullptr)
	{
		return -1.0f;
	}

	// compute path between areas using given cost heuristic
	CNavArea *goalArea = nullptr;
	if (NavAreaBuildPath( startArea, nullptr, &goalPos, costFunc, &goalArea ) == false)
	{
		return -1.0f;
	}

	// compute distance along path
	if (goalArea->GetParent() == nullptr)
	{
		// both points are in the same area - return euclidean distance
		return (goalPos - startPos).Length();
	}
	else
	{
		CNavArea *area;
		float distance;

		// goalPos is assumed to be inside goalArea (or very close to it) - skip to next area
		area = goalArea->GetParent();
		distance = (goalPos - area->GetCenter()).Length();

		for( ; area->GetParent(); area = area->GetParent() )
		{
			distance += (area->GetCenter() - area->GetParent()->GetCenter()).Length();
		}

		// add in distance to startPos
		distance += (startPos - area->GetCenter()).Length();

		return distance;
	}
}

class CCSPlayer;

class CAnimationLayer;
class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBaseMultiplayerPlayer;
class CBasePlayer;
class CCSBot;
class CCSGOPlayerAnimState;
class CCSWeaponInfo;
class CConfigurationForHighPriorityUseEntity_t;
class CEconItemView;
class CEconPersonaDataPublic;
class CHintMessageQueue;
class CRecipientFilter;
class CTakeDamageInfo;
class CUserCmd;
class CWeaponCSBase;
struct animevent_t;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Player for HL1.

// the SENDPROP_*_PRIORITY constants the Net annotations below name

class CMenu;

inline constexpr auto MENU_STRING_BUFFER_SIZE = 1024;
inline constexpr auto MENU_MSG_TEXTCHUNK_SIZE = 50;

inline constexpr const char *CS_FORCE_TEAM_THINK_CONTEXT = "CSForceTeamThink";

enum
{
	MIN_NAME_CHANGE_INTERVAL = 0,			// minimum number of seconds between name changes
	NAME_CHANGE_HISTORY_SIZE = 5,			// number of times a player can change names in NAME_CHANGE_HISTORY_INTERVAL
	NAME_CHANGE_HISTORY_INTERVAL = 600,		// no more than NAME_CHANGE_HISTORY_SIZE name changes can be made in this many seconds
};

extern ConVar bot_mimic;

// Function table for each player state.
class CCSPlayerStateInfo
{
public:
	CSPlayerState m_iPlayerState;
	const char *m_pStateName;
	
	void (CCSPlayer::*pfnEnterState)();	// Init and deinit the state.
	void (CCSPlayer::*pfnLeaveState)();

	void (CCSPlayer::*pfnPreThink)();	// Do a PreThink() in this state.
};

//=======================================
//Record of either damage taken or given.
//Contains the player name that we hurt or that hurt us,
//and the total damage
//=======================================
class CDamageRecord
{
public:
	CDamageRecord( CCSPlayer * pPlayerDamager, CCSPlayer * pPlayerRecipient, int iDamage, int iCounter, int iActualHealthRemoved );

	void AddDamage( int iDamage, int iCounter, int iActualHealthRemoved = 0 );

	bool IsDamageRecordStillValidForDamagerAndRecipient( CCSPlayer * pPlayerDamager, CCSPlayer * pPlayerRecipient );
	bool IsDamageRecordValidPlayerToPlayer(void) { return ( this && m_PlayerDamager && m_PlayerRecipient) ? true : false; }

	CCSPlayer* GetPlayerDamagerPtr( void ) { return m_PlayerDamager; }
	CCSPlayer* GetPlayerRecipientPtr( void ) { return m_PlayerRecipient; }

	char *GetPlayerDamagerName( void ) { return m_szPlayerDamagerName; }
	char *GetPlayerRecipientName( void ) { return m_szPlayerRecipientName; }

	int GetDamage( void ) { return m_iDamage; }
	int GetActualHealthRemoved( void ) { return m_iActualHealthRemoved; }
	int GetNumHits( void ) { return m_iNumHits; }

	CCSPlayer* GetPlayerDamagerControlledBotPtr( void ) { return m_PlayerDamagerControlledBot; }
	CCSPlayer* GetPlayerRecipientControlledBotPtr( void ) { return m_PlayerRecipientControlledBot; }

private:
	CHandle<CCSPlayer> m_PlayerDamager;
	CHandle<CCSPlayer> m_PlayerRecipient;

	char m_szPlayerDamagerName[MAX_PLAYER_NAME_LENGTH];
	char m_szPlayerRecipientName[MAX_PLAYER_NAME_LENGTH];
	
	int m_iDamage;		//how much damage was delivered
	int m_iActualHealthRemoved;		//how much damage was actually applied
	int m_iNumHits;		//how many hits
	int	m_iLastBulletUpdate; // update counter

	CHandle<CCSPlayer> m_PlayerDamagerControlledBot;
	CHandle<CCSPlayer> m_PlayerRecipientControlledBot;
};

typedef std::vector< std::unique_ptr< CDamageRecord > > DamageRecordList_t;

// Message display history (CCSPlayer::m_iDisplayHistoryBits)
// These bits are set when hint messages are displayed, and cleared at
// different times, according to the DHM_xxx bitmasks that follow

inline constexpr auto DHF_ROUND_STARTED = ( 1 << 1 );
inline constexpr auto DHF_HOSTAGE_SEEN_FAR = ( 1 << 2 );
inline constexpr auto DHF_HOSTAGE_SEEN_NEAR = ( 1 << 3 );
inline constexpr auto DHF_HOSTAGE_USED = ( 1 << 4 );
inline constexpr auto DHF_HOSTAGE_INJURED = ( 1 << 5 );
inline constexpr auto DHF_HOSTAGE_KILLED = ( 1 << 6 );
inline constexpr auto DHF_FRIEND_SEEN = ( 1 << 7 );
inline constexpr auto DHF_ENEMY_SEEN = ( 1 << 8 );
inline constexpr auto DHF_FRIEND_INJURED = ( 1 << 9 );
inline constexpr auto DHF_FRIEND_KILLED = ( 1 << 10 );
inline constexpr auto DHF_ENEMY_KILLED = ( 1 << 11 );
inline constexpr auto DHF_BOMB_RETRIEVED = ( 1 << 12 );
inline constexpr auto DHF_AMMO_EXHAUSTED = ( 1 << 15 );
inline constexpr auto DHF_IN_TARGET_ZONE = ( 1 << 16 );
inline constexpr auto DHF_IN_RESCUE_ZONE = ( 1 << 17 );
inline constexpr auto DHF_IN_ESCAPE_ZONE = ( 1 << 18 ); // unimplemented
inline constexpr auto DHF_IN_VIPSAFETY_ZONE = ( 1 << 19 ); // unimplemented
inline constexpr auto DHF_NIGHTVISION = ( 1 << 20 );
inline constexpr auto DHF_HOSTAGE_CTMOVE = ( 1 << 21 );
inline constexpr auto DHF_SPEC_DUCK = ( 1 << 22 );

// DHF_xxx bits to clear when the round restarts

inline constexpr auto DHM_ROUND_CLEAR = ( DHF_ROUND_STARTED | DHF_HOSTAGE_KILLED | DHF_FRIEND_KILLED | DHF_BOMB_RETRIEVED );

// DHF_xxx bits to clear when the player is restored

inline constexpr auto DHM_CONNECT_CLEAR = ( DHF_HOSTAGE_SEEN_FAR | DHF_HOSTAGE_SEEN_NEAR | DHF_HOSTAGE_USED | DHF_HOSTAGE_INJURED | DHF_FRIEND_SEEN | DHF_ENEMY_SEEN | DHF_FRIEND_INJURED | DHF_ENEMY_KILLED | DHF_AMMO_EXHAUSTED | DHF_IN_TARGET_ZONE | DHF_IN_RESCUE_ZONE | DHF_IN_ESCAPE_ZONE | DHF_IN_VIPSAFETY_ZONE | DHF_HOSTAGE_CTMOVE | DHF_SPEC_DUCK );

// radio messages (these must be kept in sync with actual radio) -------------------------------------
enum RadioType
{
	RADIO_INVALID = 0,

	RADIO_START_1,							///< radio messages between this and RADIO_START_2 and part of Radio1()

	RADIO_GO_GO_GO,
	RADIO_TEAM_FALL_BACK,
	RADIO_STICK_TOGETHER_TEAM,
	RADIO_HOLD_THIS_POSITION,
	RADIO_FOLLOW_ME,

	RADIO_START_2,							///< radio messages between this and RADIO_START_3 are part of Radio2()

	RADIO_AFFIRMATIVE,
	RADIO_NEGATIVE,
	RADIO_CHEER,
	RADIO_COMPLIMENT,
	RADIO_THANKS,

	RADIO_START_3,							///< radio messages above this are part of Radio3()

	RADIO_ENEMY_SPOTTED,
	RADIO_NEED_BACKUP,
	RADIO_YOU_TAKE_THE_POINT,
	RADIO_SECTOR_CLEAR,
	RADIO_IN_POSITION,

	// not used
	///////////////////////////////////
	RADIO_COVER_ME,
	RADIO_REGROUP_TEAM,
	RADIO_TAKING_FIRE,
	RADIO_REPORT_IN_TEAM,
	RADIO_REPORTING_IN,
	RADIO_GET_OUT_OF_THERE,
	RADIO_ENEMY_DOWN,
	RADIO_STORM_THE_FRONT,

	RADIO_END,

	RADIO_NUM_EVENTS
};

extern const char *RadioEventName[ RADIO_NUM_EVENTS+1 ];

/**
 * Convert name to RadioType
 */
extern RadioType NameToRadioEvent( const char *name );

enum BuyResult_e
{
	BUY_BOUGHT,
	BUY_ALREADY_HAVE,
	BUY_CANT_AFFORD,
	BUY_PLAYER_CANT_BUY,	// not in the buy zone, is the VIP, is past the timelimit, etc
	BUY_NOT_ALLOWED,		// weapon is restricted by VIP mode, team, etc
	BUY_INVALID_ITEM,
};

// [tj] The phases for the "Goose Chase" achievement
enum GooseChaseAchievementStep
{
	GC_NONE,    
	GC_SHOT_DURING_DEFUSE,
	GC_STOPPED_AFTER_GETTING_SHOT
};

// [tj] The phases for the "Defuse Defense" achievement
enum DefuseDefenseAchivementStep
{
	DD_NONE,
	DD_STARTED_DEFUSE,
	DD_KILLED_TERRORIST
};

struct quest_data_t
{
public:
	const char * m_szQuestCalcExpression;
	const char * m_szQuestBonusCalcExpression;
	int m_nQuestID;
	int m_nQuestNormalPoints;
	int m_nQuestBonusPoints;
};

//=============================================================================
// >> CounterStrike player
//=============================================================================
namespace DT_CSLocalPlayerExclusive { extern SendTable g_SendTable; }
namespace DT_CSNonLocalPlayerExclusive { extern SendTable g_SendTable; }

class CCSPlayer : public CBaseMultiplayerPlayer, public ICSPlayerAnimStateHelpers
{
	[[= ks::reflect::NetTable{ .name = "DT_CSLocalPlayerExclusive", .base = false } ]]
	[[= ks::reflect::From<"m_vecOrigin",
	ks::reflect::Net{ .bits = -1, .low = 0.0f, .high = kHighDefault, .flags = SPROP_NOSCALE|SPROP_CHANGES_OFTEN,
	.enc = ks::reflect::WireEnc::VectorXY, .table = "DT_CSLocalPlayerExclusive", .priority = SENDPROP_LOCALPLAYER_ORIGINXY_PRIORITY },
	SendProxy_OriginXY>{} ]]
	[[= ks::reflect::From<"m_vecOrigin",
	ks::reflect::Net{ .bits = -1, .low = 0.0f, .high = kHighDefault, .flags = SPROP_NOSCALE|SPROP_CHANGES_OFTEN,
	.table = "DT_CSLocalPlayerExclusive", .index = 2, .priority = SENDPROP_LOCALPLAYER_ORIGINZ_PRIORITY },
	SendProxy_OriginZ>{} ]]
	[[= ks::reflect::NetTable{ .name = "DT_CSNonLocalPlayerExclusive", .base = false } ]]
	[[= ks::reflect::From<"m_vecOrigin",
	ks::reflect::Net{ .bits = -1, .low = 0.0f, .high = kHighDefault, .flags = SPROP_NOSCALE|SPROP_CHANGES_OFTEN,
	.enc = ks::reflect::WireEnc::VectorXY, .table = "DT_CSNonLocalPlayerExclusive", .priority = SENDPROP_NONLOCALPLAYER_ORIGINXY_PRIORITY },
	SendProxy_OriginXY>{} ]]
	[[= ks::reflect::From<"m_vecOrigin",
	ks::reflect::Net{ .bits = -1, .low = 0.0f, .high = kHighDefault, .flags = SPROP_NOSCALE|SPROP_CHANGES_OFTEN,
	.table = "DT_CSNonLocalPlayerExclusive", .index = 2, .priority = SENDPROP_NONLOCALPLAYER_ORIGINZ_PRIORITY },
	SendProxy_OriginZ>{} ]]
	[[= ks::reflect::NetTable{ .name = "DT_CSPlayer" } ]]
	[[= ks::reflect::From<"m_ArmorValue",
	ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseAnimating", .prop = "m_flPoseParameter" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseAnimating", .prop = "m_flPlaybackRate" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseAnimating", .prop = "m_nSequence" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseAnimating", .prop = "m_nNewSequenceParity" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseAnimating", .prop = "m_nResetEventsParity" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseAnimating", .prop = "m_nMuzzleFlashParity" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseEntity", .prop = "m_angRotation" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseEntity", .prop = "m_vecOrigin" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseEntity", .prop = "m_cellbits" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseEntity", .prop = "m_cellX" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseEntity", .prop = "m_cellY" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseEntity", .prop = "m_cellZ" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_ServerAnimationData", .prop = "m_flCycle" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_AnimTimeMustBeFirst", .prop = "m_flAnimTime" } ]]
	[[= ks::reflect::SubTable<"cslocaldata", &DT_CSLocalPlayerExclusive::g_SendTable, SendProxy_SendLocalDataTable>{} ]]
	[[= ks::reflect::SubTable<"csnonlocaldata", &DT_CSNonLocalPlayerExclusive::g_SendTable, SendProxy_SendNonLocalDataTable>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CCSPlayer, CBaseMultiplayerPlayer );
	DECLARE_SERVERCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CCSPlayer();
	~CCSPlayer();

	static CCSPlayer *CreatePlayer( const char *className, edict_t *ed );
	static CCSPlayer* Instance( int iEnt );

	void		Precache_Impl();
	void		Spawn_Impl();
	virtual void		InitialSpawn( void );
	void		UpdateOnRemove_Impl( void );
	virtual void		PostSpawnPointSelection( void );
	
	void SetCSSpawnLocation( Vector position, QAngle angle );
	
	virtual void		ImpulseCommands() override;
	virtual void		CheatImpulseCommands( int iImpulse );
	virtual void		PlayerRunCommand( CUserCmd *ucmd, IMoveHelper *moveHelper );
	virtual void		PostThink();

	void				SprayPaint( ks::net::CCSUsrMsg_PlayerDecalDigitalSignature const &msg );

	class ITakeDamageListener
	{
	public:
		ITakeDamageListener();
		~ITakeDamageListener();
		virtual void OnTakeDamageListenerCallback( CCSPlayer *pVictim, CTakeDamageInfo &infoTweakable ) = 0;
	};
	int			OnTakeDamage_Impl( const CTakeDamageInfo &inputInfo );
	virtual int			OnTakeDamage_Alive( const CTakeDamageInfo &info );

	void		Event_Killed_Impl( const CTakeDamageInfo &info );
	void		Event_KilledOther_Impl( CBaseEntity *pVictim, const CTakeDamageInfo &info );

	void		TraceAttack_Impl( const CTakeDamageInfo &inputInfo, const Vector &vecDir, trace_t *ptr );

	void FindMatchingWeaponsForTeamLoadout( const char *pchName, int nTeam, bool bMustBeTeamSpecific, std::vector< CEconItemView* > &matchingWeapons );
	virtual CBaseEntity	*GiveNamedItem( const char *pchName, int iSubType = 0, CEconItemView *pScriptItem = nullptr, bool bForce = false ) override;

	virtual bool		IsBeingGivenItem() const { return m_bIsBeingGivenItem; }
	
	virtual CBaseEntity *FindUseEntity( void );
	virtual bool		IsUseableEntity( CBaseEntity *pEntity, unsigned int requiredCaps );
	
	virtual void		CreateViewModel( int viewmodelindex = 0 );
	virtual void		ShowViewPortPanel( const char * name, bool bShow = true, KeyValues *data = nullptr );

	void HandleOutOfAmmoKnifeKills( CCSPlayer* pAttackerPlayer, CWeaponCSBase* pAttackerWeapon );
	// This passes the event to the client's and server's CPlayerAnimState.
	void DoAnimationEvent( PlayerAnimEvent_t event, int nData = 0 );

	// from CBasePlayer
	virtual void		SetupVisibility( CBaseEntity *pViewEntity, unsigned char *pvs, int pvssize );

	virtual bool		ShouldCheckOcclusion( CBasePlayer *pOtherPlayer ) override;

	bool		ShouldCollide_Impl( int collisionGroup, int contentsMask ) const;

	// from CBasePlayer
	virtual bool		IsValidObserverTarget(CBaseEntity * target);
	virtual CBaseEntity* FindNextObserverTarget( bool bReverse );

	virtual int 		GetNextObserverSearchStartPoint( bool bReverse );

	virtual bool UpdateDispatchLayer( CAnimationLayer *pLayer, CStudioHdr *pWeaponStudioHdr, int iSequence ) override;

// In shared code.
public:

	// ICSPlayerAnimState overrides.
	virtual CWeaponCSBase* CSAnim_GetActiveWeapon();
	virtual bool CSAnim_CanMove();

	virtual float GetPlayerMaxSpeed();
	void CheckForWeaponFiredAchievement();

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

	void GetBulletTypeParameters( 
		int iBulletType, 
		float &fPenetrationPower, 
		float &flPenetrationDistance );

	int GetDefaultCrouchedFOV( void ) const;

	void DisplayPenetrationDebug( Vector vecEnter, Vector vecExit, float flDistance, float flInitialDamage, float flDamageLostImpact, float flTotalLostDamage, short nEnterSurf, short nExitSurf );

	// Returns true if the player is allowed to move.
	bool CanMove() const;
	
	// Returns the player mask which includes the solid mask plus the team mask.
	unsigned int PhysicsSolidMaskForEntity_Impl( void ) const;

	void OnJump( float fImpulse );
	void OnLand( float fVelocity );

	bool HasC4() const;	// Is this player carrying a C4 bomb?
	bool IsVIP() const;
	bool IsCloseToActiveBomb();
	bool IsCloseToHostage();
	bool IsObjectiveKill( CCSPlayer* pCSVictim );

	int GetClass( void ) const;

	void MakeVIP( bool isVIP );

	virtual void SetAnimation( PLAYER_ANIM playerAnim );
	void DoAnimStateEvent( PlayerAnimEvent_t evt );

	virtual bool StartReplayMode( float fDelay, float fDuration, int iEntity ) override;
	virtual void StopReplayMode() override;
	virtual void PlayUseDenySound() override;

	bool IsOtherSameTeam( int nTeam );
	bool IsOtherEnemy( CCSPlayer *pPlayer );
	bool IsOtherEnemy( int nEntIndex );
	bool IsOtherEnemyAndPlaying( int nEntIndex ); // Doesn't consider observer to be an enemy

public:

	// Simulates a single frame of movement for a player
	void RunPlayerMove( const QAngle& viewangles, float forwardmove, float sidemove, float upmove, unsigned short buttons, byte impulse, float frametime );
	virtual void HandleAnimEvent( animevent_t *pEvent );

	virtual void UpdateStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity  );
	virtual void PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force );

	// from cbasecombatcharacter
	void InitVCollision( const Vector &vecAbsOrigin, const Vector &vecAbsVelocity );
	void VPhysicsShadowUpdate_Impl( IPhysicsObject *pPhysics );
	
	bool HasWeaponOfType( int nWeaponID ) const;
	bool IsPrimaryOrSecondaryWeapon( CSWeaponType nType );

	virtual bool IsTaunting( void ) const { return m_bIsTaunting; }
	virtual bool IsThirdPersonTaunt( void ) const { return m_bIsThirdPersonTaunt; }
	virtual float GetTauntYaw( void ) const { return m_flTauntYaw; }
	virtual void StopTaunting( void ) { m_bIsTaunting = false; }

	virtual bool IsLookingAtWeapon( void ) const { return m_bIsLookingAtWeapon; }
	virtual bool IsHoldingLookAtWeapon( void ) const { return m_bIsHoldingLookAtWeapon; }
	virtual void StopLookingAtWeapon( void ) { m_bIsLookingAtWeapon = false; m_bIsHoldingLookAtWeapon = false; }
	void ModifyTauntDuration( float flTimingChange );

	CBaseEntity *GetUsableHighPriorityEntity( void );
	bool GetUseConfigurationForHighPriorityUseEntity( CBaseEntity *pEntity, CConfigurationForHighPriorityUseEntity_t &cfg );

	bool HasShield() const;
	bool IsShieldDrawn() const;
	void GiveShield( void );
	void RemoveShield( void );
	bool IsProtectedByShield( void ) const;		// returns true if player has a shield and is currently hidden behind it

	bool HasPrimaryWeapon( void );
	bool HasSecondaryWeapon( void );

	bool IsReloading( void ) const;				// returns true if current weapon is reloading

	void GiveDefaultItems();
	void GiveDefaultWearables();
	void GiveWearableFromSlot( loadout_positions_t position );
	void RemoveAllItems( bool removeSuit );	//overridden to remove the defuser
	void ValidateWearables( void );

	// Reset account, get rid of shield, etc..
	void Reset( bool resetScore );

	void RoundRespawn( void );
	void ObserverRoundRespawn( void );
	void CheckTKPunishment( void );

	virtual void ObserverUse( bool bIsPressed ); // observer pressed use

	// Add money to this player's account.
	void ResetAccount();
	void InitializeAccount( int amount = -1 );
	void AddAccount( int amount, bool bTrackChange = true, bool bItemBought = false, const char *pItemName = nullptr );
	bool AreAccountAwardsEnabled( PlayerCashAward::Type reason ) const;
	void AddAccountAward( PlayerCashAward::Type reason );
	void AddAccountAward( PlayerCashAward::Type reason, int amount, const CWeaponCSBase *pWeapon = nullptr );
	void AddAccountFromTeam( int amount, bool bTrackChange, TeamCashAward::Type reason );

	int AddDeathmatchKillScore( int nScore, CSWeaponID wepID, int iSlot, bool bIsAssist = false, const char* szVictim = nullptr );

	void HintMessage( const char *pMessage, bool bDisplayIfDead, bool bOverrideClientSettings = false ); // Displays a hint message to the player
	CHintMessageQueue *m_pHintMessageQueue;
	unsigned int m_iDisplayHistoryBits;
	bool m_bShowHints;
	float m_flLastAttackedTeammate;
	float m_flNextMouseoverUpdate;
	void UpdateMouseoverHints();

	float m_flDominateEffectDelayTime;
	EHANDLE m_hDominateEffectPlayer;

	// mark this player as not receiving money at the start of the next round.
	void ProcessSuicideAsKillReward();
	void MarkAsNotReceivingMoneyNextRound( bool bAllowMoneyNextRound = false );
	bool DoesPlayerGetRoundStartMoney(); // self-explanitory :)

	virtual bool ShouldPickupItemSilently( CBaseCombatCharacter *pNewOwner );

	void DropC4();	// Get rid of the C4 bomb.
	
	CNetworkHandle( CBaseEntity, m_hCarriedHostage, [[= ks::reflect::Net{} ]] );	// networked entity handle 
	//EHANDLE GetCarriedHostage() const;
	void GiveCarriedHostage( EHANDLE hHostage );
	void RefreshCarriedHostage( bool bForceCreate );
	void RemoveCarriedHostage();
	CNetworkHandle( CBaseEntity, m_hCarriedHostageProp, [[= ks::reflect::Net{} ]] );	// networked entity handle 
	EHANDLE	m_hHostageViewModel;

	bool HasDefuser();		// Is this player carrying a bomb defuser?
	void GiveDefuser( bool bPickedUp = false );		// give the player a defuser
	void RemoveDefuser();	// remove defuser from the player and remove the model attachment

	bool PickedUpDefuser() { return m_bPickedUpDefuser; }
	void SetDefusedWithPickedUpKit(bool bDefusedWithPickedUpKit) { m_bDefusedWithPickedUpKit = bDefusedWithPickedUpKit; }
	bool GetDefusedWithPickedUpKit() { return m_bDefusedWithPickedUpKit; }
	bool AttemptedToDefuseBomb() { return m_bAttemptedDefusal; }

	void SetDefusedBombWithThisTimeRemaining( float flTimeRemaining ) { m_flDefusedBombWithThisTimeRemaining = flTimeRemaining; }
	float GetDefusedBombWithThisTimeRemaining() { return m_flDefusedBombWithThisTimeRemaining; }

	bool IsBlindForAchievement();	// more stringent than IsBlind; more accurately represents when the player can see again

	// [jpaquin] auto refill ammo
	float m_flNextAutoBuyAmmoTime;
	void AutoBuyAmmo( bool bForce = false );
	void GuardianForceFillAmmo( void );

	bool IsBlind( void ) const;		// return true if this player is blind (from a flashbang)
	virtual void Blind( float holdTime, float fadeTime, float startingAlpha = 255 );	// player blinded by a flashbang
	void Unblind( void );	// removes the blind effect from the player
	float m_blindUntilTime;
	float m_blindStartTime;

	void Deafen( float flDistance );		//make the player deaf / apply dsp preset to muffle sound

	void ApplyDeafnessEffect();				// apply the deafness effect for a nearby explosion.

	bool IsAutoFollowAllowed( void ) const;		// return true if this player will allow bots to auto follow
	void InhibitAutoFollow( float duration );	// prevent bots from auto-following for given duration
	void AllowAutoFollow( void );				// allow bots to auto-follow immediately
	float m_allowAutoFollowTime;				// bots can auto-follow after this time

	// Have this guy speak a message into his radio.
	void Radio( const char *szRadioSound, const char *szRadioText = nullptr , bool bTriggeredAutomatically = false );
	void ConstructRadioFilter( CRecipientFilter& filter );
	float m_flGotHostageTalkTimer;
	float m_flDefusingTalkTimer;
	float m_flC4PlantTalkTimer;

	virtual bool CanHearAndReadChatFrom( CBasePlayer *pPlayer );

	void EmitPrivateSound( const char *soundName );		///< emit given sound that only we can hear

	bool Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex = 0 );
	CWeaponCSBase* GetActiveCSWeapon() const;

	int GetNumTriggerPulls() { return m_triggerPulls; }
	void LogTriggerPulls();

	void PreThink();

	// This is the think function for the player when they first join the server and have to select a team
	void JoiningThink();

	virtual bool ClientCommand( const CCommand &args );

	bool AllowTaunts( void );
	void Taunt( void );
	void LookAtHeldWeapon( void );

	void SendJoinTeamFailedMessage( int reason, bool raiseTeamScreen );

	bool HandleCommand_JoinClass( void );
	bool HandleCommand_JoinTeam( int iTeam, bool bQueue = false, int iCoachTeam = 0 ); // if bQueue is true then the team move will not occur until round restart

	BuyResult_e HandleCommand_Buy( const char *item, int nPos = -1, bool bAddToRebuy = true );
	BuyResult_e HandleCommand_Buy_Internal( const char * item, int nPos = -1, bool bAddToRebuy = true  );

	CNetworkVar( bool, m_bIsBuyMenuOpen );
	bool IsBuyMenuOpen( void ) { return m_bIsBuyMenuOpen; } 
	void SetBuyMenuOpen( bool bOpen );

	AcquireResult::Type CanAcquire( CSWeaponID weaponId, AcquireMethod::Type acquireMethod, CEconItemView *pItem = nullptr );
	int					GetCarryLimit( CSWeaponID weaponId );
	int	GetWeaponPrice( CSWeaponID weaponId, const CEconItemView *pWepView = nullptr ) const;
	CWeaponCSBase*		CSWeapon_OwnsThisType( CEconItemView *pItem ) const;

	void HandleMenu_Radio1( int slot );
	void HandleMenu_Radio2( int slot );
	void HandleMenu_Radio3( int slot );

	float m_flRadioTime;	
	int m_iRadioMessages;
	int iRadioMenu;

	void ListPlayers();

	bool m_bIgnoreRadio;

	// Returns one of the CS_CLASS_ enums.
	int PlayerClass() const;

	void MoveToNextIntroCamera();

	// Used to be GETINTOGAME state.
	void GetIntoGame();

	CBaseEntity* EntSelectSpawnPoint();
	
	void SetProgressBarTime( int barTime );
	virtual void PlayerDeathThink();
	virtual void PlayerForceTeamThink();
	virtual void ResetForceTeamThink();

	virtual bool StartObserverMode( int mode ) override;
	virtual bool SetObserverTarget( CBaseEntity *target );
	virtual void ValidateCurrentObserverTarget( void );
	virtual void CheckObserverSettings( void );

	void Weapon_Equip( CBaseCombatWeapon *pWeapon );
	virtual bool BumpWeapon( CBaseCombatWeapon *pWeapon );
	virtual bool Weapon_CanUse( CBaseCombatWeapon *pWeapon );

	void ClearFlashbangScreenFade ( void );
	bool ShouldDoLargeFlinch( const CTakeDamageInfo& info, int nHitGroup );

	void ResetStamina( void );
	bool IsArmored( int nHitGroup );
	void Pain( CCSPlayer* attacker, bool HasArmour, int nDmgTypeBits = 0 );
	
	void DeathSound( const CTakeDamageInfo &info );
	
	bool Weapon_CanSwitchTo( CBaseCombatWeapon *pWeapon );
	virtual void OnSwitchWeapons( CBaseCombatWeapon* pWeapon );
	void ChangeTeam_Impl( int iTeamNum );
	void SwitchTeam( int iTeamNum );	// Changes teams without penalty - used for auto team balancing

	void ModifyOrAppendCriteria_Impl( AI_CriteriaSet& set );
	void ModifyOrAppendPlayerCriteria( AI_CriteriaSet& set );

	virtual void OnDamagedByExplosion( const CTakeDamageInfo &info );

	virtual bool CanKickFromTeam( int kickTeam );

	// Called whenever this player fires a shot.
	void NoteWeaponFired();
	virtual bool WantsLagCompensationOnEntity( const CBaseEntity *pPlayer, const CUserCmd *pCmd, const std::bitset<MAX_EDICTS> *pEntityTransmitBits ) const;

	// training map - used to set hud element visibility via script
	void SetMiniScoreHidden( bool bHidden ) { m_bHud_MiniScoreHidden = bHidden; }
	bool IsMiniScoreHidden( void ) { return m_bHud_MiniScoreHidden; }
	void SetRadarHidden( bool bHidden ) { m_bHud_RadarHidden = bHidden; }
	bool IsRadarHidden( void ) { return m_bHud_RadarHidden; }
	
	void SetLastKillerIndex( int nLastKillerIndex ) { m_nLastKillerIndex = nLastKillerIndex; }
	int GetLastKillerIndex( void ) { return m_nLastKillerIndex; }
	void SetLastConcurrentKilled( int nLastConcurrentKilled ) { m_nLastConcurrentKilled = nLastConcurrentKilled; }
	int GetLastConcurrentKilled( void ) { return m_nLastConcurrentKilled; }
	void SetDeathCamMusicIndex( int nDeathCamMusicIndex ) { m_nDeathCamMusic = nDeathCamMusicIndex; }
	int GetDeathCamMusicIndex( void ) { return m_nDeathCamMusic; }

	void UpdateEquippedCoinFromInventory();
	void SetRank( MedalCategory_t category, MedalRank_t rank );
	MedalRank_t GetRank( MedalCategory_t category ) { return m_rank.Get( category ); }
	void UpdateRankFromKV( KeyValues *pKV );

	void UpdateEquippedMusicFromInventory();
	void SetMusicID( uint16 unMusicID );
	uint32 GetMusicID( ) { return m_unMusicID.Get(); }

	void UpdateEquippedPlayerSprayFromInventory();

	void UpdatePersonaDataFromInventory();
	CEconPersonaDataPublic const * GetPersonaDataPublic() const;

	virtual void AwardAchievement( int iAchievement, int iCount = 1 );

// ------------------------------------------------------------------------------------------------ //
// Player state management.
// ------------------------------------------------------------------------------------------------ //
public:

	void State_Transition( CSPlayerState newState );	// Cleanup the previous state and enter a new state.
	CSPlayerState State_Get() const;				// Get the current state.

private:
	void State_Enter( CSPlayerState newState );		// Initialize the new state.
	void State_Leave();								// Cleanup the previous state.
	void State_PreThink();							// Update the current state.
	
	// Find the state info for the specified state.
	static CCSPlayerStateInfo* State_LookupInfo( CSPlayerState state );

	// This tells us which state the player is currently in (joining, observer, dying, etc).
	// Each state has a well-defined set of parameters that go with it (ie: observer is movetype_noclip, nonsolid,
	// invisible, etc).
	CNetworkVar( CSPlayerState, m_iPlayerState, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] );

	CCSPlayerStateInfo *m_pCurStateInfo;			// This can be NULL if no state info is defined for m_iPlayerState.

	// tells us whether or not this player gets money at the start of the next round.
	bool m_receivesMoneyNextRound;

	// Specific state handler functions.
	void State_Enter_WELCOME();
	void State_PreThink_WELCOME();

	void State_Enter_PICKINGTEAM();
	void State_Enter_PICKINGCLASS();

	void State_Enter_ACTIVE();
	void State_PreThink_ACTIVE();

	void State_Enter_OBSERVER_MODE();
	void State_Leave_OBSERVER_MODE();
	void State_PreThink_OBSERVER_MODE();

	void State_Enter_GUNGAME_RESPAWN();
	void State_PreThink_GUNGAME_RESPAWN();
	void TryGungameRespawn();

	void State_Enter_DEATH_WAIT_FOR_KEY();
	void State_PreThink_DEATH_WAIT_FOR_KEY();

	void State_Enter_DEATH_ANIM();
	void State_PreThink_DEATH_ANIM();

	int FlashlightIsOn( void );
	virtual bool FlashlightTurnOn( bool playSound = false );
	virtual void FlashlightTurnOff( bool playSound = false );

	void UpdateAddonBits();
	void ProcessSpottedEntityUpdate();
	void AppendSpottedEntityUpdateMessage( int entindex, bool bSpotted,
		ks::net::CCSUsrMsg_ProcessSpottedEntityUpdate::SpottedEntityUpdate *pMsg );
	//void UpdateTeamMoney();
	int GetAccountForScoreboard();

public:
	bool StartHltvReplayEvent( const ClientReplayEventParams_t &params );
	int		GetHealth() const		{ return m_iHealth; }
	void	SetHealth_Impl( int amt );

	void				SetDeathPose( const int &iDeathPose ) { m_iDeathPose = iDeathPose; }
	void				SetDeathPoseFrame( const int &iDeathPoseFrame ) { m_iDeathFrame = iDeathPoseFrame; }
	void				SetDeathPoseYaw( const float &flDeathPoseYaw ) { m_flDeathYaw = flDeathPoseYaw; }
	
	void				SelectDeathPose( const CTakeDamageInfo &info );

	bool				MadeFinalGunGameProgressiveKill( void ) { return m_bMadeFinalGunGameProgressiveKill; }
	bool				JustLeftSpawnImmunity(){return m_fJustLeftImmunityTime > 0.0f;}

	virtual void		IncrementFragCount( int nCount, int nHeadshots = 0 );
	virtual void		IncrementAssistsCount( int nCount );
	virtual void		IncrementDeathCount( int nCount );
	void				IncrementTeamKillsCount( int nCount );
	void				IncrementHostageKillsCount( int nCount );
	void				IncrementTeamDamagePoints( int numDamagePoints );

	void	SetLastKillTime( float time );
	float	GetLastKillTime();
	void	IncrementKillStreak( int nCount );
	void	ResetKillStreak();
	int		GetKillStreak();

	void	ResetNumRoundKills() { m_iNumRoundKills = 0; m_iNumRoundKillsHeadshots = 0; }
	
	int GetNumConcurrentDominations( void );
	void GiveWeaponFromID ( int nWeaponID );

private:
	int		m_iKillStreak;
	float	m_flLastKillTime;

	int	m_iDeathPose;
	int	m_iDeathFrame;
	float m_flDeathYaw;
	bool m_bAbortFreezeCam;
	bool m_bJustBecameSpectator;

	bool m_bRespawning;
	int m_iNumGunGameTRBombTotalPoints;
	bool m_bShouldProgressGunGameTRBombModeWeapon;
	bool m_switchTeamsOnNextRoundReset;
	float m_fJustLeftImmunityTime;
	float m_lowHealthGoalTime;

	CNetworkArray( MedalRank_t, m_rank, MEDAL_CATEGORY_COUNT, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED } ]] );
	bool m_bNeedToUpdateCoinFromInventory;

	CNetworkVar( uint16, m_unMusicID, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] );
	bool m_bNeedToUpdateMusicFromInventory;

	uint16 m_unEquippedPlayerSprayIDs[ /*LOADOUT_POSITION_SPRAY3*/ LOADOUT_POSITION_SPRAY0 + 1 - LOADOUT_POSITION_SPRAY0 ];
	bool m_bNeedToUpdatePlayerSprayFromInventory;

	CEconPersonaDataPublic *m_pPersonaDataPublic;
	bool m_bNeedToUpdatePersonaDataPublicFromInventory;

protected:
	void AttemptToExitFreezeCam( void );

	void GiveCurrentProgressiveGunGameWeapon( void );
	void GiveNextProgressiveGunGameWeapon( void );
	void SubtractProgressiveWeaponIndex( void );

	void SendGunGameWeaponUpgradeAlert( void );

public:

	CNetworkVar( bool, m_bIsScoped, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bIsWalking, [[= ks::reflect::Net{} ]] );
	// Predicted variables.
	CNetworkVar( bool, m_bResumeZoom, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bIsDefusing, [[= ks::reflect::Net{} ]] );			// tracks whether this player is currently defusing a bomb
	bool IsDefusing( void ) { return m_bIsDefusing; }
	CNetworkVar( bool, m_bIsGrabbingHostage, [[= ks::reflect::Net{} ]] );			// tracks whether this player is currently grabbing a hostage
	CNetworkVar( float, m_fImmuneToGunGameDamageTime, [[= ks::reflect::Net{} ]] );	// When gun game spawn damage immunity will expire
	CNetworkVar( bool, m_bGunGameImmunity, [[= ks::reflect::Net{} ]] );	// tracks whether this player is currently immune in gun game
	CNetworkVar( bool, m_bMadeFinalGunGameProgressiveKill, [[= ks::reflect::Net{} ]] );
	CNetworkVar( int,  m_iGunGameProgressiveWeaponIndex, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED | SPROP_CHANGES_OFTEN } ]] );	// index of current gun game weapon
	CNetworkVar( int, m_iNumGunGameTRKillPoints, [[= ks::reflect::Net{} ]] );	// number of kill points accumulated so far in TR Gun Game mode (resets to 0 when weapon is upgraded)
	CNetworkVar( int, m_iNumGunGameKillsWithCurrentWeapon, [[= ks::reflect::Net{} ]] );
	CNetworkVar( int, m_iNumRoundKills, [[= ks::reflect::Net{} ]] );	// number of kills a player has in a single round
	CNetworkVar( int, m_iNumRoundKillsHeadshots, [[= ks::reflect::Net{} ]] ); // number of kills a player got this round with headshots
	int m_iNumRoundTKs;	// number of teammate kills a player has in a single round
	CNetworkVar( float, m_fMolotovUseTime, [[= ks::reflect::Net{} ]] );	// Molotov can be used if current time is after this time
	CNetworkVar( float, m_fMolotovDamageTime, [[= ks::reflect::Net{} ]] );	// Last time when this player was burnt by Molotov damage

	CNetworkVar( bool, m_bHasMovedSinceSpawn, [[= ks::reflect::Net{} ]] );		// Whether player has moved from spawn position

	CNetworkVar( bool, m_bCanMoveDuringFreezePeriod, [[= ks::reflect::Net{} ]] );		

	CNetworkVar( bool, m_isCurrentGunGameLeader, [[= ks::reflect::Net{} ]] );		
	CNetworkVar( bool, m_isCurrentGunGameTeamLeader, [[= ks::reflect::Net{} ]] );	
	
	CNetworkVar( float, m_flGuardianTooFarDistFrac, [[= ks::reflect::Net{} ]] );
	float m_flNextGuardianTooFarHurtTime;

	CNetworkVar( float, m_flDetectedByEnemySensorTime, [[= ks::reflect::Net{} ]] );	

	void GiveHealthAndArmorForGuardianMode( bool bAdditive );
	int GetPlayerGunGameWeaponIndex( void ) { return m_iGunGameProgressiveWeaponIndex; }
	int GetNumGunGameTRKillPoints( void ) { return m_iNumGunGameTRKillPoints; }
	int GetNumRoundKills( void ) { return m_iNumRoundKills; }
	int GetNumRoundKillsHeadshots( void ) { return m_iNumRoundKillsHeadshots; }
	int GetNumGunGameKillsWithCurrentWeapon( void ) { return m_iNumGunGameKillsWithCurrentWeapon; }

	//--------------------------------------------------------------------------------------------------------
	void OnHealthshotUsed( void );
	bool UpdateTeamLeaderPlaySound( int nTeam );
	void UpdateLeader( void );

	void IncrementGunGameProgressiveWeapon( int nNumLevelsToIncrease );

	// [tj] overriding the base suicides to trash CS specific stuff
	virtual void CommitSuicide( bool bExplode = false, bool bForce = false );
	virtual void CommitSuicide( const Vector &vecForce, bool bExplode = false, bool bForce = false );

	void WieldingKnifeAndKilledByGun( bool bState ) { m_bWieldingKnifeAndKilledByGun = bState; }
	bool WasWieldingKnifeAndKilledByGun() { return m_bWieldingKnifeAndKilledByGun; }
	
	void RecordRebuyStructLastRound();

	void HandleEndOfRound();
	bool WasKilledThisRound(){return m_wasKilledThisRound;}
	void SetWasKilledThisRound(bool wasKilled);
	int GetMaxNumRoundsSurvived() {return m_maxNumRoundsSurvived;}
	int GetCurNumRoundsSurvived() {return m_numRoundsSurvived;}
	
	// [dwenger] adding tracking for weapon used fun fact
	void PlayerUsedFirearm( CBaseCombatWeapon* pBaseWeapon );
	void PlayerEmptiedAmmoForFirearm( CBaseCombatWeapon* pBaseWeapon );
	void PlayerOutOfAmmoForFirearm( CBaseCombatWeapon* pBaseWeapon );
	void PlayerHeldFirearm( CBaseCombatWeapon* pBaseWeapon );
	void AddBurnDamageDelt( int entityIndex);
	int GetNumPlayersDamagedWithFire();

	int GetNumFirearmsHeld() { return std::ssize( m_WeaponTypesHeld ); }
	int GetNumFirearmsUsed() { return std::ssize( m_WeaponTypesUsed ); }
	int GetNumFirearmsRanOutOfAmmo() { return std::ssize( m_WeaponTypesRunningOutOfAmmo ); }
	bool DidPlayerEmptyAmmoForWeapon( CBaseCombatWeapon* pBaseWeapon );
	void SetLastWeaponBeforeAutoSwitchToC4( CBaseCombatWeapon * weapon ){m_lastWeaponBeforeC4AutoSwitch = weapon;}
	CBaseCombatWeapon* GetLastWeaponBeforeAutoSwitchToC4( void ){return m_lastWeaponBeforeC4AutoSwitch;}
	void RestoreWeaponOnC4Abort();

	void PlayerUsedGrenade( int nWeaponID );
	void PlayerUsedKnife( void );

	void ImpactTrace_Impl( trace_t *pTrace, int iDamageType, const char *pCustomImpactName );

	bool HasHeavyArmor() const	{return m_bHasHeavyArmor;}
	CNetworkVar( bool, m_bHasHelmet, [[= ks::reflect::Net{} ]] );				// Does the player have helmet armor
	CNetworkVar( bool, m_bHasHeavyArmor, [[= ks::reflect::Net{} ]] );				// Does the player have heavy armor?
	bool m_bEscaped;			// Has this terrorist escaped yet?

	// Other variables.
	bool m_bIsVIP;				// Are we the VIP?
	int m_iNumSpawns;			// Number of times player has spawned this round
	int m_iOldTeam;				// Keep what team they were last on so we can allow joining spec and switching back to their real team
	bool m_bTeamChanged;		// Just allow one team change per round
	int m_iShouldHaveCash;

	bool m_bHasSeenJoinGame;	// Since creation, have we seen a joingame command for this player?

	int m_iLastTeam;			// Last team on which this player was a member
	
	bool m_bJustKilledTeammate;
	bool m_bPunishedForTK;
	int m_iTeamKills;
	float m_flLastAction;
	int m_iNextTimeCheck;		// Next time the player can execute a "timeleft" command

	float m_flNameChangeHistory[NAME_CHANGE_HISTORY_SIZE]; // index 0 = most recent change

	bool CanChangeName( void );	// Checks if the player can change his name
	void ChangeName( const char *pszNewName );

	void SetClanTag( const char *pTag );
	const char *GetClanTag( void ) const;
	void SetClanName( const char *pName );
	const char *GetClanName( void ) const;

	void InitTeammatePreferredColor();
	void SetTeammatePreferredColor( int nColor );
	int GetTeammatePreferredColor( void ) const;

	//RecvPropEHandle( RECVINFO(m_hCarriedHostage) ),

	CNetworkVar( bool, m_bHasDefuser, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );			// Does this player have a defuser kit?
	float m_fLastGivenDefuserTime;				// the last time this player received the defuser
	float m_fLastGivenBombTime;					// the last time this player received the bomb
	CNetworkVar( bool, m_bHasNightVision, [[= ks::reflect::Net{} ]] );		// Does this player have night vision?
	CNetworkVar( bool, m_bNightVisionOn, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );		// Is the NightVision turned on ?

	float m_fNextRadarUpdateTime;
	float m_flLastMoneyUpdateTime;

	// Backup copy of the menu text so the player can change this and the menu knows when to update.
	char	m_MenuStringBuffer[MENU_STRING_BUFFER_SIZE];

	// When the player joins, it cycles their view between trigger_camera entities.
	// This is the current camera, and the time that we'll switch to the next one.
	EHANDLE m_pIntroCamera;
	float m_fIntroCamTime;

	// Set to true each frame while in a bomb zone.
	// Reset after prediction (in PostThink).
	CNetworkVar( bool, m_bInBombZone, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( bool, m_bInBuyZone, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	// See if we need to prevent player from being able to diffuse bomb.
	CNetworkVar( bool, m_bInNoDefuseArea, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( bool, m_bKilledByTaser, [[= ks::reflect::Net{} ]] );
	CNetworkVar( int, m_iMoveState, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_CHANGES_OFTEN } ]] );		// Is the player trying to run?  Used for state transitioning after a player lands from a jump etc.
	
	CNetworkString( m_szArmsModel, MAX_MODEL_STRING_SIZE, [[= ks::reflect::Net{} ]] );		// Which arms we're using for the view model.

	// Match Stats data
	CNetworkArray( int, m_iMatchStats_Kills,			MAX_MATCH_STATS_ROUNDS, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED, .priority = SENDPROP_MATCHSTATS_PRIORITY } ]] );				//kills, per round
	CNetworkArray( int, m_iMatchStats_Damage,			MAX_MATCH_STATS_ROUNDS, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED, .priority = SENDPROP_MATCHSTATS_PRIORITY } ]] );				//damage, per round
	CNetworkArray( int, m_iMatchStats_EquipmentValue,	MAX_MATCH_STATS_ROUNDS, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED, .priority = SENDPROP_MATCHSTATS_PRIORITY } ]] );				//Equipment value, per round
	CNetworkArray( int, m_iMatchStats_MoneySaved,		MAX_MATCH_STATS_ROUNDS, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED, .priority = SENDPROP_MATCHSTATS_PRIORITY } ]] );				//Saved money, per round
	CNetworkArray( int, m_iMatchStats_KillReward,		MAX_MATCH_STATS_ROUNDS, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED, .priority = SENDPROP_MATCHSTATS_PRIORITY } ]] );				//Money earned from kills, per round
	CNetworkArray( int, m_iMatchStats_LiveTime,			MAX_MATCH_STATS_ROUNDS, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED, .priority = SENDPROP_MATCHSTATS_PRIORITY } ]] );				//Time spent alive, per round
	CNetworkArray( int, m_iMatchStats_Deaths,			MAX_MATCH_STATS_ROUNDS, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED, .priority = SENDPROP_MATCHSTATS_PRIORITY } ]] );				//Deaths
	CNetworkArray( int, m_iMatchStats_Assists,			MAX_MATCH_STATS_ROUNDS, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED, .priority = SENDPROP_MATCHSTATS_PRIORITY } ]] );				//Assists
	CNetworkArray( int, m_iMatchStats_HeadShotKills,	MAX_MATCH_STATS_ROUNDS, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED, .priority = SENDPROP_MATCHSTATS_PRIORITY } ]] );				// Head shot kills
	CNetworkArray( int, m_iMatchStats_Objective,		MAX_MATCH_STATS_ROUNDS, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED, .priority = SENDPROP_MATCHSTATS_PRIORITY } ]] );				// successful objectives ( bomb plants, defuses, hostage rescues )
	CNetworkArray( int, m_iMatchStats_CashEarned,		MAX_MATCH_STATS_ROUNDS, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED, .priority = SENDPROP_MATCHSTATS_PRIORITY } ]] );				// Cash awards
	CNetworkArray( int, m_iMatchStats_UtilityDamage,	MAX_MATCH_STATS_ROUNDS, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED, .priority = SENDPROP_MATCHSTATS_PRIORITY } ]] );				// Grenade etc damage
	CNetworkArray( int, m_iMatchStats_EnemiesFlashed,	MAX_MATCH_STATS_ROUNDS, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED, .priority = SENDPROP_MATCHSTATS_PRIORITY } ]] );				// Enemies flashed

	bool m_bUseNewAnimstate;
	void SetModel_Impl( const char *szModelName );

	virtual Vector Weapon_ShootPosition();

	int m_iBombSiteIndex;
	// keep track of when we enter and exit a bombzone
	bool	m_bInBombZoneTrigger;
	bool	m_bWasInBombZoneTrigger;
	bool	m_bWasInHostageRescueZone;
	// keep track of when we enter and exit a buyzone
	bool m_bWasInBuyZone;

	bool IsInBuyZone();
	bool IsInBuyPeriod();
	bool CanBuyDuringImmunity();
	bool CanPlayerBuy( bool display );

	CNetworkVar( bool, m_bInHostageRescueZone, [[= ks::reflect::Net{} ]] );
	[[= ks::reflect::Input{ .name = "OnRescueZoneTouch", .type = FIELD_VOID } ]] void RescueZoneTouch( inputdata_t &inputdata );

	CNetworkVar( float, m_flStamina , [[= ks::reflect::Net{ .bits = 14, .low = 0, .high = 100.0f, .flags = 0, .table = "DT_CSLocalPlayerExclusive" } ]] );
	CNetworkVar( int, m_iDirection , [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED, .table = "DT_CSLocalPlayerExclusive" } ]] );	// The current lateral kicking direction; 1 = right,  0 = left
	CNetworkVar( int, m_iShotsFired , [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED, .table = "DT_CSLocalPlayerExclusive" } ]] );	// number of shots fired recently (seems inconsistent, based on specific weapons incrementing this value)
	CNetworkVar( int, m_nNumFastDucks , [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED, .table = "DT_CSLocalPlayerExclusive" } ]] );  // UNUSED.  Kept for backwards demo compatibility.  $$$REI TODO: Investigate safely removing variables
	CNetworkVar( bool, m_bDuckOverride , [[= ks::reflect::Net{ .table = "DT_CSLocalPlayerExclusive" } ]] ); // force the player to duck regardless of if they're holding crouch

	// Make sure to register changes for armor.
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_ArmorValue );

	float m_flFlinchStack; // we add to this stack everytime we take damage that would "tag" us - decays constantly
	CNetworkVar( float, m_flVelocityModifier , [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 1, .flags = 0, .table = "DT_CSLocalPlayerExclusive" } ]] );
	void SetFlinchVelocityModifier( float fVelocityModifier );
	CNetworkVar( float, m_flGroundAccelLinearFracLastTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_CHANGES_OFTEN } ]] );

	int	m_iHostagesKilled;
	int m_bulletsFiredSinceLastSpawn;

	void SetShieldDrawnState( bool bState );
	void DropShield( void );
	
	char m_szNewName [MAX_PLAYER_NAME_LENGTH]; // not empty if player requested a namechange
	char m_szClanTag[MAX_CLAN_TAG_LENGTH];
	char m_szClanName[MAX_TEAM_NAME_LENGTH];

	Vector m_vecTotalBulletForce;	//Accumulator for bullet force in a single frame
	
	// preferred teammate color that user has set for themselves
	int m_iTeammatePreferredColor;

	CNetworkVar( float, m_flFlashDuration, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flFlashMaxAlpha, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	
	CNetworkVar( float, m_flProgressBarStartTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( int, m_iProgressBarDuration, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_iThrowGrenadeCounter, [[= ks::reflect::Net{ .bits = 3, /* THROWGRENADE_COUNTER_BITS, defined in cs_player.cpp */ .flags = SPROP_UNSIGNED } ]] );	// used to trigger grenade throw animations.
	CNetworkVar( bool, m_bWaitForNoAttack, [[= ks::reflect::Net{} ]] );	// flag to indicate player cannot attack until the attack button is released
	CNetworkVar( bool, m_bIsRespawningForDMBonus, [[= ks::reflect::Net{} ]] ); // flag to indicate player wants to spawn with the deathmatch bonus weapons
	
	CNetworkVar( float, m_flLowerBodyYawTarget, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( bool, m_bStrafing, [[= ks::reflect::Net{} ]] );

	// Tracks our ragdoll entity.
	CNetworkHandle( CBaseEntity, m_hRagdoll, [[= ks::reflect::Net{} ]] );	// networked entity handle 

	// Bots and hostages auto-duck during jumps
	bool m_duckUntilOnGround;

	Vector m_lastStandingPos; // used by the gamemovement code for finding ladders

	void SurpressLadderChecks( const Vector& pos, const Vector& normal );
	bool CanGrabLadder( const Vector& pos, const Vector& normal );

	void ClearGunGameImmunity( void );
	void ClearGunGameProgressiveWeaponIndex( void ) { m_iGunGameProgressiveWeaponIndex = 0; }
	void ResetTRBombModeWeaponProgressFlag( void ) { m_bShouldProgressGunGameTRBombModeWeapon = false; }
	void ResetTRBombModeKillPoints( void ) { m_iNumGunGameTRKillPoints = 0; }
	void SwitchTeamsAtRoundReset( void ) { m_switchTeamsOnNextRoundReset = true; }
	bool WillSwitchTeamsAtRoundReset( void ) { return m_switchTeamsOnNextRoundReset; }
	void ResetTRBombModeData( void );

	void SetPickedUpWeaponThisRound( bool pickedUp ){m_bPickedUpWeapon = pickedUp;}
	bool GetPickedUpWeaponThisRound( void ){return m_bPickedUpWeapon;}

	bool CanUseGrenade( CSWeaponID );

	bool CSWeaponDrop( CBaseCombatWeapon *pWeapon, bool bDropShield = true, bool bThrow = false );
	bool CSWeaponDrop( CBaseCombatWeapon *pWeapon, Vector targetPos, bool bDropShield = true );

	bool HandleDropWeapon( CBaseCombatWeapon *pWeapon = nullptr, bool bSwapping = false );

	void SetViewModelArms( const char *armsModel );

	void DestroyWeapon( CBaseCombatWeapon *pWeapon );

	void DestroyWeapons( bool bDropC4 = true );

	bool IsPlayerSpawning( void ) { return m_bIsSpawning; }
	void SetPlayerSpawning( bool bIsSpawning ) { m_bIsSpawning = bIsSpawning; }

private:
	CountdownTimer m_ladderSurpressionTimer;
	Vector m_lastLadderNormal;
	Vector m_lastLadderPos;

protected:

	void CreateRagdollEntity();

	bool IsHittingShield( const Vector &vecDirection, trace_t *ptr );

	void PhysObjectSleep();
	void PhysObjectWake();

	bool RunMimicCommand( CUserCmd& cmd );

	bool SelectSpawnSpot( const char *pEntClassName, CBaseEntity* &pSpot );

	void SetModelFromClass( void );
	CNetworkVar( int, m_iClass, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] ); // One of the CS_CLASS_ enums.

	void TransferInventory( CCSPlayer* pTargetPlayer );
	bool DropWeaponSlot( int nSlot, bool fromDeath = false );

	void DropWeapons( bool fromDeath, bool killedByEnemy );

	virtual int SpawnArmorValue( void ) const { return ArmorValue(); }

	bool BAttemptToBuyCheckSufficientBalance( int nCostOfPurchaseToCheck, bool bClientPrint = true );
	BuyResult_e AttemptToBuyAmmo( int iAmmoType );
	BuyResult_e AttemptToBuyAmmoSingle( int iAmmoType );
	BuyResult_e AttemptToBuyVest( void );
	BuyResult_e AttemptToBuyAssaultSuit( void );
	BuyResult_e AttemptToBuyHeavyAssaultSuit( void );
	BuyResult_e AttemptToBuyDefuser( void );
	BuyResult_e AttemptToBuyNightVision( void );
	
	BuyResult_e BuyAmmo( int nSlot, bool bBlinkMoney );
	BuyResult_e BuyGunAmmo( CBaseCombatWeapon *pWeapon, bool bBlinkMoney );

	void InternalAutoBuyAmmo( int slot );

	void PushawayThink();

private:

	IPlayerAnimState *m_PlayerAnimState;
	CCSGOPlayerAnimState *m_PlayerAnimStateCSGO;

	// Aiming heuristics code
	float						m_flIdleTime;		//Amount of time we've been motionless
	float						m_flMoveTime;		//Amount of time we've been in motion
	float						m_flLastDamageTime;	//Last time we took damage
	float						m_flTargetFindTime;

	int							m_lastDamageHealth;		// Last damage given to our health
	int							m_lastDamageArmor;		// Last damage given to our armor

	bool						m_bPickedUpWeapon;
	bool						m_bPickedUpDefuser;			// Did player pick up the defuser kit as opposed to buying it?
	bool						m_bDefusedWithPickedUpKit;	// Did player defuse the bomb with a picked-up defuse kit?
	bool						m_bAttemptedDefusal;
	int							m_nPreferredGrenadeDrop;

	float						m_flDefusedBombWithThisTimeRemaining;

	// Last usercmd we shot a bullet on.
	int m_iLastWeaponFireUsercmd;

	// Copyed from EyeAngles() so we can send it to the client.
	CNetworkVectorXYZ( m_angEyeAngles,
	    [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE | SPROP_CHANGES_OFTEN,
	                 .enc = ks::reflect::WireEnc::Angle, .index = 0 } ]]
	    [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE | SPROP_CHANGES_OFTEN,
	                 .enc = ks::reflect::WireEnc::Angle, .index = 1 } ]] );

	bool m_bVCollisionInitted;

	Vector m_storedSpawnPosition;
	QAngle m_storedSpawnAngle;
	bool m_bIsSpawning;

public:
	CNetworkVar( float, m_flThirdpersonRecoil, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_NOSCALE } ]] );

// AutoBuy functions.
public:
	void			AutoBuy( const char *autobuyString ); // this should take into account what the player can afford and should buy the best equipment for them.

	bool			IsInAutoBuy( void ) { return m_bIsInAutoBuy; }
	bool			IsInReBuy( void ) { return m_bIsInRebuy; }
	int				GetAccountBalance( );

private:
	bool			ShouldExecuteAutoBuyCommand(const AutoBuyInfoStruct *commandInfo, bool boughtPrimary, bool boughtSecondary);
	void			PostAutoBuyCommandProcessing(const AutoBuyInfoStruct *commandInfo, bool &boughtPrimary, bool &boughtSecondary);
	void			ParseAutoBuyString(const char *string, bool &boughtPrimary, bool &boughtSecondary);
	AutoBuyInfoStruct *GetAutoBuyCommandInfo(const char *command);
	void			PrioritizeAutoBuyString(char *autobuyString, const char *priorityString); // reorders the tokens in autobuyString based on the order of tokens in the priorityString.
	BuyResult_e	CombineBuyResults( BuyResult_e prevResult, BuyResult_e newResult );

	bool			m_bIsInAutoBuy;
	bool			m_bAutoReload;

//ReBuy functions

public:
	void			Rebuy( const char *rebuyString );
	bool			AttemptToBuyDMBonusWeapon( void );
private:
	void			AddToRebuy( CSWeaponID weaponId, int nPos );
	void			AddToGrenadeRebuy( CSWeaponID weaponId );

	BuyResult_e	RebuyPrimaryWeapon();
	BuyResult_e	RebuySecondaryWeapon();
	BuyResult_e	RebuyTaser();
	BuyResult_e	RebuyGrenade( CSWeaponID weaponId );
	BuyResult_e	RebuyDefuser();
	BuyResult_e	RebuyNightVision();
	BuyResult_e	RebuyArmor();

	bool			m_bIsInRebuy;
	RebuyStruct		m_rebuyStruct;
	RebuyStruct		m_rebuyStructLastRound;
	bool			m_bUsingDefaultPistol;

	bool			m_bIsBeingGivenItem;

//BuyRandom functions
public:
	void			BuyRandom();

	CNetworkVar( bool, m_bHud_MiniScoreHidden, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bHud_RadarHidden, [[= ks::reflect::Net{} ]] );

	CNetworkVar( int, m_nLastKillerIndex, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nLastConcurrentKilled, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nDeathCamMusic, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] ); // this players deathcam music index

	// This is a combination of the ADDON_ flags in cs_shareddefs.h.
	CNetworkVar( int, m_iAddonBits, [[= ks::reflect::Net{ .bits = NUM_ADDON_BITS, .flags = SPROP_UNSIGNED } ]] );

	// Clients don't know about holstered weapons, so we need to tell them the weapon type here
	CNetworkVar( int, m_iPrimaryAddon, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_iSecondaryAddon, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );

	CNetworkVar( int, m_iAccount, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] );	// How much cash this player has.
	int m_iAccountMoneyEarnedForNextRound;	// How much of this player's cash cannot be used during this round (kill rewards, suicide rewards, etc.)
	CNetworkVar( int, m_iStartAccount, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] );	// How much cash this player started the round with

	CNetworkVar( int, m_totalHitsOnServer, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] ); // used to compare against client's hit counts to see how 'wrong' the client was.

//Damage record functions
public:

	static void	StartNewBulletGroup();	// global function
	static uint32 GetBulletGroup();		// global function
	static void ResetBulletGroup();		// global function, called at the beginning of every match

	void RecordDamage( CCSPlayer* damageDealer, CCSPlayer* damageTaker, int iDamageDealt, int iActualHealthRemoved );

	void ResetDamageCounters();	//Reset all lists

	void RemoveSelfFromOthersDamageCounters(); // Additional cleanup to damage counters when not in a round respawn mode.

	void OutputDamageTaken( void );
	void OutputDamageGiven( void );

	void SendLastKillerDamageToClient( CCSPlayer *pLastKiller );

	void StockPlayerAmmo( CBaseCombatWeapon *pNewWeapon = nullptr );

	DamageRecordList_t& GetDamageList() {return m_DamageList;}
	int GetNumAttackersFromDamageList( void );
	int GetMostNumHitsDamageRecordFrom( CCSPlayer *pAttacker );

	int m_nTeamDamageGivenForMatch;
	bool m_bTDGaveProtectionWarning;
	bool m_bTDGaveProtectionWarningThisRound;

private:
	//A unified list of recorded damage that includes giver and taker in each entry
	DamageRecordList_t	m_DamageList;

protected:
	float m_flLastTHWarningTime;
	float m_applyDeafnessTime;
	int m_currentDeafnessFilter;

	bool m_isVIP;

// Command rate limiting.
private:

	bool ShouldRunRateLimitedCommand( const CCommand &args );

	// This lets us rate limit the commands the players can execute so they don't overflow things like reliable buffers.
	ks::idict<float>	m_RateLimitLastCommandTimes;

	CNetworkVar(int, m_cycleLatch, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED | SPROP_CHANGES_OFTEN } ]] );	// Every so often, we are going to transmit our cycle to the client to correct divergence caused by PVS changes
	CountdownTimer m_cycleLatchTimer;

public:
	void ResetRoundBasedAchievementVariables();
	void OnRoundEnd(int winningTeam, int reason);
	void OnPreResetRound();

	void DecrementProgressiveWeaponFromSuicide( void );
	int GetNumEnemyDamagers();
	int GetNumEnemiesDamaged();
	int GetTotalActualHealthRemovedFromEnemies();
	CBaseEntity* GetNearestSurfaceBelow(float maxTrace);

	// Returns the % of the enemies this player killed in the round
	int GetPercentageOfEnemyTeamKilled();

	//List of times of recent kills to check for sprees
	std::vector< float >			m_killTimes; 

	//List of all players killed this round
	std::vector< CHandle<CCSPlayer> >		m_enemyPlayersKilledThisRound;

	//List of weapons we have used to kill players with this round
	std::vector< int >				m_killWeapons; 

	//List of weapons we have used to kill players with this match
	std::vector< int >				m_uniqueKillWeaponsMatch;
	bool						m_bLastKillUsedUniqueWeaponMatch;

	int m_NumEnemiesKilledThisSpawn;
	int m_maxNumEnemiesKillStreak;
	int m_NumEnemiesKilledThisRound;
	int m_NumEnemiesAtRoundStart;
	int m_KillingSpreeStartTime;

	int m_NumChickensKilledThisSpawn;

	bool m_bLastKillUsedUniqueWeapon;

 	int m_iRoundsWon;

	float m_firstKillBlindStartTime; //This is the start time of the blind effect during which we got our most recent kill.
	int m_killsWhileBlind;
	int m_bombCarrierkills;
 
	CNetworkVar( bool, m_bIsRescuing, [[= ks::reflect::Net{} ]] );	// tracks whether a player is currently rescuing a hostage :: Networked to provide access at OGS RoundData recording time
	//bool m_bIsRescuing;			// tracks whether this player is currently rescuing a hostage
	bool m_bInjuredAHostage;	// tracks whether this player injured a hostage
	int  m_iNumFollowers;		// Number of hostages following this player
	bool m_bSurvivedHeadshotDueToHelmet;
	bool m_knifeKillBombPlacer;
	bool m_attemptedBombPlace;
	int m_knifeKillsWhenOutOfAmmo;
	bool m_triggerPulled;
	int m_triggerPulls;

	bool m_bHasUsedDMBonusRespawn;

	// mainly fun-fact/achievement related values below
	int GetKnifeKillsWhenOutOfAmmo() { return m_knifeKillsWhenOutOfAmmo; }
	void IncrKnifeKillsWhenOutOfAmmo() { m_knifeKillsWhenOutOfAmmo++; }
	bool HasAttemptedBombPlace(){ return m_attemptedBombPlace; }
	void SetAttemptedBombPlace(){ m_attemptedBombPlace = true; }
	int GetNumBombCarrierKills( void ) { return m_bombCarrierkills; }
	void IncrementNumFollowers() { m_iNumFollowers++; }
	void DecrementNumFollowers() { m_iNumFollowers--; if (m_iNumFollowers < 0) m_iNumFollowers = 0; }
	int GetNumFollowers() { return m_iNumFollowers; }
	void SetIsRescuing(bool in_bRescuing) { m_bIsRescuing = in_bRescuing; }
	bool IsRescuing() { return m_bIsRescuing; }
	void SetInjuredAHostage(bool in_bInjured) { m_bInjuredAHostage = in_bInjured; }
	bool InjuredAHostage() { return m_bInjuredAHostage; }
	bool PlacedBombThisRound() { return (GetBombPlacedTime() >= 0.0f); }
	float GetBombPickuptime() { return m_bombPickupTime; }
	float GetBombPlacedTime() {return m_bombPlacedTime; }
	float GetBombDroppedTime() {return m_bombDroppedTime; }
	void SetBombPickupTime(float time) { m_bombPickupTime = time; }
	void SetBombPlacedTime( float time) { m_bombPlacedTime = time; }
	void SetBombDroppedTime( float time) { m_bombDroppedTime = time; }
	void SetKnifeLevelKilledBombPlacer( void ) { m_knifeKillBombPlacer = true; }
	bool GetKnifeLevelKilledBombPlacer() {return m_knifeKillBombPlacer; }
	CCSPlayer* GetLastFlashbangAttacker() { return m_lastFlashBangAttacker; }
	void SetLastFlashbangAttacker(CCSPlayer* attacker) { m_lastFlashBangAttacker = attacker; }
	float GetKilledTime( void ) { return m_killedTime; }
	void SetKilledTime( float time ); 
	static const CCSWeaponInfo* GetWeaponInfoFromDamageInfo( const CTakeDamageInfo &info );
	static void ProcessPlayerDeathAchievements( CCSPlayer *pAttacker, CCSPlayer *pVictim, const CTakeDamageInfo &info );
	float GetLongestSurvivalTime( void ) { return m_longestLife; }

	void						OnCanceledDefuse();
	void						OnStartedDefuse();
	GooseChaseAchievementStep	m_gooseChaseStep;
	DefuseDefenseAchivementStep	m_defuseDefenseStep;
	CHandle<CCSPlayer>			m_pGooseChaseDistractingPlayer;

	int							m_lastRoundResult; //save the reason for the last round ending.

	bool						m_bMadeFootstepNoise;

	float						m_bombPickupTime;
	float						m_bombPlacedTime;
	float						m_bombDroppedTime;
	float						m_killedTime;
	float						m_spawnedTime;
	float						m_longestLife;
	
	bool						m_bMadePurchseThisRound;

	int							m_roundsWonWithoutPurchase;

	bool						m_bKilledDefuser;
	bool						m_bKilledRescuer;
	int							m_maxGrenadeKills;

	int							m_grenadeDamageTakenThisRound;
	int							m_firstShotKills;
	bool						m_hasReloaded;
	
	void						SetHasReloaded(){m_hasReloaded = true;}
	bool						HasReloaded(){return m_hasReloaded;}
	void						IncrementFirstShotKills(int amount){m_firstShotKills += amount;}
	void						ResetFirstShotKills(){m_firstShotKills = 0;}
	int							GetFirstShotKills(){return m_firstShotKills;}

	bool						GetKilledDefuser() { return m_bKilledDefuser; } 
	bool						GetKilledRescuer() { return m_bKilledRescuer; } 
	int							GetMaxGrenadeKills() { return m_maxGrenadeKills; }

	void						CheckMaxGrenadeKills(int grenadeKills);

	CHandle<CCSPlayer>			m_lastFlashBangAttacker;

	void	SetPlayerDominated( CCSPlayer *pPlayer, bool bDominated );    
	void	SetPlayerDominatingMe( CCSPlayer *pPlayer, bool bDominated );
	bool	IsPlayerDominated( int iPlayerIndex );
	bool	IsPlayerDominatingMe( int iPlayerIndex );

	bool	m_wasNotKilledNaturally; //Set if the player is dead from a kill command or late login

	bool	WasNotKilledNaturally() { return m_wasNotKilledNaturally; }

	void	SetNumMVPs( int iNumMVP );
	void	IncrementNumMVPs( CSMvpReason_t mvpReason );
	int		GetNumMVPs();

	void	SetEnemyKillTrackInfo( int iEnemyKills, int iEnemyKillHeadshots, int iEnemy3Ks, int iEnemy4Ks, int iEnemy5Ks, int iEnemyKillsAgg ) { m_iEnemyKills = iEnemyKills; m_iEnemyKillHeadshots = iEnemyKillHeadshots; m_iEnemy3Ks = iEnemy3Ks; m_iEnemy4Ks = iEnemy4Ks; m_iEnemy5Ks = iEnemy5Ks; m_iEnemyKillsAgg = iEnemyKillsAgg; }
	void	GetEnemyKillTrackInfo( int &iEnemyKills, int &iEnemyKillHeadshots, int &iEnemy3Ks, int &iEnemy4Ks, int &iEnemy5Ks, int &iEnemyKillsAgg ) { iEnemyKills = m_iEnemyKills; iEnemyKillHeadshots = m_iEnemyKillHeadshots; iEnemy3Ks = m_iEnemy3Ks; iEnemy4Ks = m_iEnemy4Ks; iEnemy5Ks = m_iEnemy5Ks; iEnemyKillsAgg = m_iEnemyKillsAgg; }

	void	SetEnemyFirstKills( int numFirstKills, int numClutchKills ) { m_numFirstKills = numFirstKills; m_numClutchKills = numClutchKills; }
	void	GetEnemyFirstKills( int &numFirstKills, int &numClutchKills ) { numFirstKills = m_numFirstKills; numClutchKills = m_numClutchKills; }
	void	SetEnemyWeaponKills( int numPistolKills, int numSniperKills ) { m_numPistolKills = numPistolKills; m_numSniperKills = numSniperKills; }
	void	GetEnemyWeaponKills( int &numPistolKills, int &numSniperKills ) { numPistolKills = m_numPistolKills; numSniperKills = m_numSniperKills; }

	void    ClearRoundContributionScore( void ) { m_iRoundContributionScore = 0; }
	void    AddRoundContributionScore( int iPoints );
	int		GetRoundContributionScore( void ) { return m_iRoundContributionScore; }

	void	ClearRoundProximityScore( void ){ m_iRoundProximityScore  = 0; }
	void	AddRoundProximityScore( int iPoints );
	int		GetRoundProximityScore( void ) { return m_iRoundProximityScore ; }

	void	ClearScore( void ); 
	void	AddScore( int iPoints );

	int		GetScore() const { return m_iContributionScore; }

	int	GetContributionScore( void ) { return m_iContributionScore; }
	void ClearContributionScore( void ){ m_iContributionScore = 0; pl.score = 0; }
	void AddContributionScore( int iPoints );

	uint32 GetHumanPlayerAccountID() const { return m_uiAccountId; }
	void SetHumanPlayerAccountID( uint32 uiAccountId );
	
	void    RemoveNemesisRelationships();
	void	SetDeathFlags( int iDeathFlags ) { m_iDeathFlags = iDeathFlags; }
	int		GetDeathFlags() { return m_iDeathFlags; }		
	int		GetNumBotsControlled( void ) { return m_botsControlled; }
	int		GetNumFootsteps( void ) { return m_iFootsteps; }
	int		GetMediumHealthKills( void ) { return m_iMediumHealthKills; }
	int		GetTotalCashSpent( void ) { return m_iTotalCashSpent; }
	int		GetCashSpentThisRound( void ) { return m_iCashSpentThisRound; }

	int		GetEndMatchNextMapVote( void ) { return m_nEndMatchNextMapVote; }

	void	ClearTRModeHEGrenade( void ) { m_bGunGameTRModeHasHEGrenade = false; }
	void	ClearTRModeFlashbang( void ) { m_bGunGameTRModeHasFlashbang = false; }
	void	ClearTRModeMolotov( void ) { m_bGunGameTRModeHasMolotov = false; }
	void	ClearTRModeIncendiary( void ) { m_bGunGameTRModeHasIncendiary = false; }

private:
	CNetworkArray( bool, m_bPlayerDominated, MAX_PLAYERS+1 , [[= ks::reflect::Net{ .table = "DT_CSLocalPlayerExclusive" } ]] );		// array of state per other player whether player is dominating other players
	CNetworkArray( bool, m_bPlayerDominatingMe, MAX_PLAYERS+1 , [[= ks::reflect::Net{ .table = "DT_CSLocalPlayerExclusive" } ]] );	// array of state per other player whether other players are dominating this player

	CNetworkArray( int, m_iWeaponPurchasesThisRound, MAX_WEAPONS , [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED, .table = "DT_CSLocalPlayerExclusive" } ]] );	// number of times weapons purchased this round; used to limit repurchases

	CNetworkVar( bool, m_bIsTaunting );
	CNetworkVar( bool, m_bIsThirdPersonTaunt );
	CNetworkVar( bool, m_bIsHoldingTaunt );
	CNetworkVar( float, m_flTauntYaw );

	CNetworkVar( bool, m_bIsLookingAtWeapon, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bIsHoldingLookAtWeapon, [[= ks::reflect::Net{} ]] );

	float m_flTauntEndTime;
	float m_flLookWeaponEndTime;
	bool m_bMustNotMoveDuringTaunt;

	// [menglish] number of rounds this player has caused to be won for their team
	int m_iMVPs;
	uint32 m_uiAccountId;

	// Competitive scorecard tracking information
	int m_iEnemyKills;
	int m_iEnemyKillHeadshots;
	int m_iEnemy3Ks;
	int m_iEnemy4Ks;
	int m_iEnemy5Ks;
	int m_iEnemyKillsAgg;
	int m_numFirstKills;
	int m_numClutchKills;
	int m_numPistolKills;
	int m_numSniperKills;

	// [pfreese] new contribution score system
	int m_iScore;
	int m_iRoundProximityScore;
	int m_iRoundContributionScore;
	int m_iContributionScore;

	// [dwenger] adding tracking for fun fact
	bool m_bWieldingKnifeAndKilledByGun;
	int m_botsControlled;
	int m_iFootsteps;
	int m_iMediumHealthKills;

	// tracking Money spent to calculate Cash Per Kill
	int m_iTotalCashSpent;
	// tracking just the cash spent in this round
	int m_iCashSpentThisRound;

	// this players vote for the next map in the mapgroup at the end of the match
	int m_nEndMatchNextMapVote;

	// [dkorus] achievement tracking
	bool m_wasKilledThisRound;
	int	 m_numRoundsSurvived;
	int  m_maxNumRoundsSurvived;

	// [dwenger] adding tracking for which weapons this player has used in a round
	std::vector< CSWeaponID > m_WeaponTypesUsed; 
	std::vector< CSWeaponID > m_WeaponTypesHeld;
	std::vector< CSWeaponID > m_WeaponTypesRunningOutOfAmmo; 
	std::vector< int >		   m_BurnDamageDeltVec;

	CBaseCombatWeapon* m_lastWeaponBeforeC4AutoSwitch;

	int m_iDeathFlags; // Flags holding revenge and domination info about a death

	// Track last damage type
	int	m_LastDamageType;

	// Used to track whether or not a player in TR gun game mode has earned grenades
	bool m_bGunGameTRModeHasHEGrenade;
	bool m_bGunGameTRModeHasFlashbang;
	bool m_bGunGameTRModeHasMolotov;
	bool m_bGunGameTRModeHasIncendiary;

public:
	bool IsAbleToInstantRespawn( void );
	bool IsAssassinationTarget( void ) const;
	char const * IsAbleToApplySpray( trace_t *ptr, Vector *pvecForward, Vector *pvecRight );

	uint32 GetActiveQuestID( void ) const;
	QuestProgress::Reason GetQuestProgressReason( void ) const;

private:
	CNetworkVar( bool, m_bIsAssassinationTarget );	// This player is an assassination target for an active mission

public: 

	bool CanControlBot( CCSBot *pBot ,bool bSkipTeamCheck = false );
	bool TakeControlOfBot( CCSBot *pBot, bool bSkipTeamCheck = false );
	void ReleaseControlOfBot( void );
	CCSBot* FindNearestControllableBot( bool bMustBeValidObserverTarget );
	bool IsControllingBot( void )							const { return m_bIsControllingBot; }

	bool HasControlledBot( void )						const { return m_hControlledBot.Get() != nullptr; }
	CCSPlayer* GetControlledBot( void )				const { return static_cast<CCSPlayer*>(m_hControlledBot.Get()); }
	void SetControlledBot( CCSPlayer* pOther )	      { m_hControlledBot = pOther; }

	bool HasControlledByPlayer( void )					const { return m_hControlledByPlayer.Get() != nullptr; }
	CCSPlayer* GetControlledByPlayer( void )				const { return static_cast<CCSPlayer*>(m_hControlledByPlayer.Get()); }
	void SetControlledByPlayer( CCSPlayer* pOther )	      { m_hControlledByPlayer = pOther; }

	bool HasBeenControlledThisRound( void ) { return m_bHasBeenControlledByPlayerThisRound; }
	bool HasControlledBotThisRound( void ) {return m_bHasControlledBotThisRound;}

private:
	CNetworkVar( bool, m_bIsControllingBot, [[= ks::reflect::Net{} ]] );	// Are we controlling a bot? 
	// Note that this can be true even if GetControlledPlayer() returns NULL, 
	// IFF we started controlling a bot and then the bot was deleted for some reason. 

	CNetworkVar( bool, m_bCanControlObservedBot, [[= ks::reflect::Net{} ]] );	// set to true if we can take control of the bot we are observing, for client UI feedback. 

	CNetworkVar( int, m_iControlledBotEntIndex, [[= ks::reflect::Net{} ]] );	// Are we controlling a bot? 

	CHandle<CCSPlayer> m_hControlledBot;		// The is the OTHER player that THIS player is controlling
	CHandle<CCSPlayer> m_hControlledByPlayer;	// This is the OTHER player that is controlling THIS player
	bool m_bHasBeenControlledByPlayerThisRound;
	CNetworkVar( bool, m_bHasControlledBotThisRound, [[= ks::reflect::Net{} ]] );

	// Various values from this character before they took control or were controlled
	struct PreControlData
	{
		int m_iClass;		// CS class (such as CS_CLASS_PHOENIX_CONNNECTION or CS_CLASS_SEAL_TEAM_6)
		int m_iAccount;		// money
		int m_iAccountMoneyEarnedForNextRound; // money earned this round
		int m_iFrags;		// kills / score
		int m_iAssists;
		int m_iDeaths;		
	};

	void SavePreControlData();

	PreControlData	m_PreControlData;

public:
	PreControlData	GetBotPreControlData( void ) { return m_PreControlData; }

	void		UpdateInventory( bool bInit );

	// Inventory access
	CCSPlayerInventory	*Inventory( void ) { return &m_Inventory; }
	const CCSPlayerInventory	*Inventory( void ) const { return &m_Inventory; }
	CEconItemView *GetEquippedItemInLoadoutSlot( int iLoadoutSlot ) { return Inventory()->GetInventoryItemByItemID( m_EquippedLoadoutItemIndices[iLoadoutSlot] ); }
	CEconItemView *GetEquippedItemInLoadoutSlotOrBaseItem( int iLoadoutSlot );

	uint32 RecalculateCurrentEquipmentValue( void );
	void UpdateFreezetimeEndEquipmentValue( void );

	//void UpdateAppearanceIndex( void );

private:
	CCSPlayerInventory	m_Inventory;
	// Items that have been equipped on this player instance (the inventory loadout may have changed)
	itemid_t				m_EquippedLoadoutItemIndices[LOADOUT_POSITION_COUNT];

	//uint16 m_unAppearanceIndex;
	CNetworkVar( uint16, m_unCurrentEquipmentValue, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( uint16, m_unRoundStartEquipmentValue, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( uint16, m_unFreezetimeEndEquipmentValue, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] );

private:
	// override for weapon driving animations
	bool UpdateLayerWeaponDispatch( CAnimationLayer *pLayer, int iSequence );
public:
	virtual float	GetLayerSequenceCycleRate( CAnimationLayer *pLayer, int iSequence );

	bool GetBulletHitLocalBoneOffset( const trace_t &tr, int &boneIndexOut, Vector &vecPositionOut, QAngle &angAngleOut );

	// Quest state
private:
	// Can we make progress in our current quest?  If not, why not?
	CNetworkVar( QuestProgress::Reason, m_nQuestProgressReason , [[= ks::reflect::Net{ .bits = QuestProgress::QuestReasonBits, .flags = SPROP_UNSIGNED, .table = "DT_CSLocalPlayerExclusive" } ]] );
};

inline CCSPlayer *ToCSPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return nullptr;

	return dynamic_cast<CCSPlayer*>( pEntity );
}

class CBaseEntity;
class CEconQuestDefinition;

using namespace ::ks::reflect;


// Purpose: CS's custom CPlayerResource

extern Vector g_vecDefuserPosition;
extern CBaseEntity* g_pDefuserEntity;

class CCSPlayerResource : public CPlayerResource
{
	[[= ks::reflect::NetTable{ .name = "DT_CSPlayerResource" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CCSPlayerResource, CPlayerResource );
	
public:
	DECLARE_SERVERCLASS();

	CCSPlayerResource();

	virtual void UpdatePlayerData( void );
	void Spawn_Impl( void );

	const Vector	GetBombsiteAPosition();
	const Vector	GetBombsiteBPosition();

	const Vector	GetHostageRescuePosition( int index );

	bool			EndMatchNextMapAllVoted( void ) { return m_bEndMatchNextMapAllVoted; }

	int				GetCompTeammateColor( int iIndex );
	void			ResetPlayerTeammateColor( int index );
	void			ForcePlayersPickColors( void );
	void			SetPlayerTeammateColor( int index, bool bReset );

	bool			IsAssassinationTarget( int index ) const;
	void			UpdateAssassinationTargets( const CEconQuestDefinition * pQuest );

protected:

	CNetworkVar( int, m_iPlayerC4, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );  // entity index of C4 carrier or 0
	CNetworkVar( int, m_iPlayerVIP, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] ); // entity index of VIP player or 0
	CNetworkArray( bool, m_bHostageAlive, MAX_HOSTAGES, [[= ks::reflect::Net{} ]] );
	CNetworkArray( bool, m_isHostageFollowingSomeone, MAX_HOSTAGES, [[= ks::reflect::Net{} ]] );
	CNetworkArray( int, m_iHostageEntityIDs, MAX_HOSTAGES, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVector( m_bombsiteCenterA, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD } ]] );// Location of bombsite A
	CNetworkVector( m_bombsiteCenterB, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD } ]] );// Location of bombsite B
	CNetworkArray( int, m_hostageRescueX, MAX_HOSTAGE_RESCUES, [[= ks::reflect::Net{ .bits = COORD_INTEGER_BITS+1 } ]] );// Locations of all hostage rescue spots
	CNetworkArray( int, m_hostageRescueY, MAX_HOSTAGE_RESCUES, [[= ks::reflect::Net{ .bits = COORD_INTEGER_BITS+1 } ]] );
	CNetworkArray( int, m_hostageRescueZ, MAX_HOSTAGE_RESCUES, [[= ks::reflect::Net{ .bits = COORD_INTEGER_BITS+1 } ]] );

	CNetworkArray( int, m_iMVPs, MAX_PLAYERS + 1, [[= ks::reflect::Net{ .bits = COORD_INTEGER_BITS+1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkArray( int, m_iArmor, MAX_PLAYERS + 1, [[= ks::reflect::Net{ .bits = COORD_INTEGER_BITS+1, .flags = SPROP_UNSIGNED } ]] );	
	CNetworkArray( bool, m_bHasDefuser, MAX_PLAYERS + 1, [[= ks::reflect::Net{} ]] );
	CNetworkArray( bool, m_bHasHelmet, MAX_PLAYERS + 1, [[= ks::reflect::Net{} ]] );
	CNetworkArray( int, m_iScore, MAX_PLAYERS + 1, [[= ks::reflect::Net{ .bits = 32 } ]] );	
	CNetworkArray( int, m_iCompetitiveRanking, MAX_PLAYERS + 1, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkArray( int, m_iCompetitiveWins, MAX_PLAYERS + 1, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkArray( int, m_iCompTeammateColor, MAX_PLAYERS + 1, [[= ks::reflect::Net{ .bits = 32 } ]] );

	CNetworkArray( int, m_bControllingBot, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkArray( int, m_iControlledPlayer, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkArray( int, m_iControlledByPlayer, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkArray( int, m_iBotDifficulty, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 32 } ]] );	// Difficulty level of a bot ( -1 if not applicable )
	CNetworkArray( string_t, m_szClan, MAX_PLAYERS+1, [[= ks::reflect::Net{} ]] );
	CNetworkArray( int, m_iTotalCashSpent, MAX_PLAYERS + 1, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkArray( int, m_iCashSpentThisRound, MAX_PLAYERS + 1, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkArray( int, m_nEndMatchNextMapVotes, MAX_PLAYERS + 1, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkVar( bool, m_bEndMatchNextMapAllVoted, [[= ks::reflect::Net{} ]] );

	CNetworkArray( int, m_nActiveCoinRank, MAX_PLAYERS + 1, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkArray( int, m_nMusicID, MAX_PLAYERS + 1, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkArray( bool, m_bIsAssassinationTarget, MAX_PLAYERS + 1 );
	
	CNetworkArray( int, m_nPersonaDataPublicLevel, MAX_PLAYERS + 1, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkArray( int, m_nPersonaDataPublicCommendsLeader, MAX_PLAYERS + 1, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkArray( int, m_nPersonaDataPublicCommendsTeacher, MAX_PLAYERS + 1, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkArray( int, m_nPersonaDataPublicCommendsFriendly, MAX_PLAYERS + 1, [[= ks::reflect::Net{ .bits = 32 } ]] );

	bool m_nAttemptedToGetColor[MAX_PLAYERS + 1];

private:
	bool m_foundGoalPositions;
	bool m_bPreferencesAssigned_CT;
	bool m_bPreferencesAssigned_T;
};

inline CCSPlayerResource* CSPlayerResource()
{
	return static_cast<CCSPlayerResource*>(g_pPlayerResource);
}

class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CBaseViewModel;
class CCSPlayer;
class CCSWeaponInfo;
class CEconItemDefinition;
class CEconItemView;
class CIronSightController;
struct animevent_t;

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

		DECLARE_DATADESC();

		virtual void CheckRespawn();
		 CBaseEntity* Respawn_Impl();
		
		virtual const Vector& GetBulletSpread();
		virtual float	GetDefaultAnimSpeed();

		virtual void	BulletWasFired( const Vector &vecStart, const Vector &vecEnd );
		virtual bool	ShouldRemoveOnRoundRestart();
		virtual void    OnRoundRestart();

		virtual bool	DefaultReload( int iClipSize1, int iClipSize2, int iActivity );

		void SendActivityEvents( int nActEvents = ACT_VM_RELOAD );

		void Materialize();
		void AttemptToMaterialize();
		 void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

		virtual bool IsRemoveable();

		virtual void RemoveUnownedWeaponThink();
		
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

	const char		*GetTracerType_Impl( void ) { return GetCSWpnData().GetTracerEffectName( GetEconItemView() ); }

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
		virtual void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
		virtual bool	Reload();
		 void	Spawn_Impl();
		using BaseClass::KeyValue_Impl;
	 bool	KeyValue_Impl( const char *szKeyName, const char *szValue );

		 bool PhysicsSplash_Impl( const Vector &centerPoint, const Vector &normal, float rawSpeed, float scaledSpeed );

	bool IsUseable();
	virtual bool	CanDeploy( void );
	virtual void	UpdateShieldState( void );
	virtual bool	SendWeaponAnim( int iActivity );
	virtual void	SendViewModelAnim( int nSequence );
	virtual void	SecondaryAttack( void );
	virtual bool	CanPrimaryAttack( void ) {return true;}
	 void	Precache_Impl( void );
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

protected:

	float CalculateNextAttackTime( float flCycleTime );
	void Recoil( CSWeaponMode weaponMode );

	[[= ks::reflect::Key{ .name = "CanBePickedUp" } ]] bool m_bCanBePickedUp;

private:
	CEconItemView m_EconItemView;
	CNetworkVar( int, m_nEconItemDefIndex, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] );

	CWeaponCSBase( const CWeaponCSBase & );

	virtual int GetWeaponID( void ) const		{ return GetCSWeaponID(); }

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

	bool m_bFiredOutOfAmmoEvent;
	int m_numRemoveUnownedWeaponThink;

public:

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
class CBasePlayer;
class CCSWeaponInfo;
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

	 void	Precache_Impl();

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

	bool			IsThrownUnderhand( void ) { return (m_flThrowStrength <= GRENADE_UNDERHAND_THRESHOLD); }
	float			GetThrownStrength( void ) { return m_flThrowStrength; }
	float			ApproachThrownStrength( void );

	bool HasEmittedProjectile( void ) const { return m_bHasEmittedProjectile; }
	bool m_bHasEmittedProjectile;

	virtual bool AllowsAutoSwitchFrom( void ) const;

	int		CapabilitiesGet();
	
	// Each derived grenade class implements this.
	virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo );

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

	 void Spawn_Impl( void );

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

	virtual void CompleteUse( CCSPlayer *pPlayer ) {}

private:
	CWeaponBaseItem( const CWeaponBaseItem & ) {}

	CNetworkVarEmbedded( CountdownTimer, m_UseTimer );
	CNetworkVar( bool, m_bRedraw, [[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );	// Draw the weapon again after throwing a grenade

};

class CBaseAnimating;
class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CCSPlayer;
class CWeaponCSBase;
struct inputdata_t;

using namespace ::ks::reflect;


inline constexpr auto NUM_BEEPS = 7;

	// ------------------------------------------------------------------------------------------ //
	// CPlantedC4 class.
	// ------------------------------------------------------------------------------------------ //

	class CPlantedC4 : public CBaseAnimating
{
	[[= ks::reflect::NetTable{ .name = "DT_PlantedC4" } ]]
	static constexpr bool ks_class = true;

	public:
		DECLARE_CLASS( CPlantedC4, CBaseAnimating );
		DECLARE_DATADESC();
		DECLARE_SERVERCLASS();

		DECLARE_PREDICTABLE();

		CPlantedC4();
		virtual ~CPlantedC4();

		 void Spawn_Impl();

		 int  UpdateTransmitState_Impl();
		 void SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways );
		 int  ShouldTransmit_Impl( const CCheckTransmitInfo *pInfo );

		static CPlantedC4* ShootSatchelCharge( CCSPlayer *pevOwner, Vector vecStart, QAngle vecAngles );
		 void Precache_Impl();
		
		// Set these flags so CTs can use the C4 to disarm it.
		 int	ObjectCaps_Impl() { return BaseClass::ObjectCaps_Impl() | (FCAP_CONTINUOUS_USE | FCAP_ONOFF_USE); }

		void SetBombSiteIndex( int iIndex ){ m_iBombSiteIndex = iIndex;	}

		inline bool IsBombActive( void ) { return m_bBombTicking; }

		CCSPlayer* GetPlanter( void ) { return m_pPlanter; }
		void SetPlanter( CCSPlayer* player ) { m_pPlanter = player; }

		CCSPlayer* GetDefuser( void ) { return m_pBombDefuser; }

		void SetPlantedAfterPickup( bool plantedAfterPickup ) { m_bPlantedAfterPickup = plantedAfterPickup; }

	public:

		CNetworkVar( bool, m_bBombTicking, [[= ks::reflect::Net{} ]] );
		CNetworkVar( float, m_flC4Blow, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );

		[[= ks::reflect::Key{ .name = "OnBombDefused" } ]] COutputEvent m_OnBombDefused; 
		[[= ks::reflect::Key{ .name = "OnBombBeginDefuse" } ]] COutputEvent m_OnBombBeginDefuse; 
		[[= ks::reflect::Key{ .name = "OnBombDefuseAborted" } ]] COutputEvent m_OnBombDefuseAborted;

	protected:
		virtual void Init( CCSPlayer *pevOwner, Vector vecStart, QAngle vecAngles, bool	bTrainingPlacedByPlayer );
		// used for the training map where the map spawns the bomb and sets the timer manually
		virtual void ActivateSetTimerLength( float flTimerLength );

		void C4Think();

		bool			m_bTrainingPlacedByPlayer;
		bool			m_bHasExploded;

	private:
		
		// This becomes the think function when the timer has expired and it is about to explode.
		void DetonateThink();
		virtual void Explode( trace_t *pTrace, int bitsDamageType );

		public: void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	private:

		// Replicate timer length to the client for effects
		CNetworkVar( float, m_flTimerLength, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );

		// Info for defusing.
		bool			m_bBeingDefused;
		CHandle<CCSPlayer> m_pBombDefuser;
		float			m_fLastDefuseTime;
		int				m_iBombSiteIndex;

		CNetworkVar( float, m_flDefuseLength, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );		//How long does the defuse take? Depends on if a defuser was used
		CNetworkVar( float, m_flDefuseCountDown, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );	//What time does the defuse complete?
		CNetworkVar( bool, m_bBombDefused, [[= ks::reflect::Net{} ]] ); 
		CNetworkVar( CHandle<CCSPlayer>, m_hBombDefuser, [[= ks::reflect::Net{} ]] );

		// Control panel
		void GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
		void GetControlPanelClassName( int nPanelIndex, const char *&pPanelName );
		void SpawnControlPanels( void );
		void RemoveControlPanels( void );

		int m_iProgressBarTime;
		bool m_bVoiceAlertFired;

		// [tj] We need to store who planted the bomb so we can track who deserves credits for the kills
		CHandle<CCSPlayer>  m_pPlanter;

		// [tj] We need to know if this was planted by a player who recovered the bomb
		bool m_bPlantedAfterPickup;
	};

	// ------------------------------------------------------------------------------------------ //
	// CPlantedC4 class.
	// ------------------------------------------------------------------------------------------ //

	class CPlantedC4Training : public CPlantedC4
	{
	public:
		DECLARE_CLASS( CPlantedC4Training, CPlantedC4 );
		DECLARE_DATADESC();

		//DECLARE_SERVERCLASS();

		DECLARE_PREDICTABLE();

		//CPlantedC4Training();
		//virtual ~CPlantedC4Training();
		[[= ks::reflect::Input{ .name = "ActivateSetTimerLength", .type = FIELD_FLOAT } ]] void	InputActivateSetTimerLength( inputdata_t &inputdata );

		[[= ks::reflect::Key{ .name = "OnBombExploded" } ]] COutputEvent m_OnBombExploded;	//Fired when the bomb explodes

	protected:
		//virtual void Init( CCSPlayer *pevOwner, Vector vecStart, QAngle vecAngles );
		virtual void Explode( trace_t *pTrace, int bitsDamageType );
	};

	extern std::vector< CPlantedC4* > g_PlantedC4s;

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
	
	 void Spawn_Impl();

	void ItemPostFrame();
	virtual void WeaponReset( void );
	virtual void PrimaryAttack();
	virtual void WeaponIdle();
	virtual void UpdateShieldState( void );
	virtual float GetMaxSpeed() const;

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_C4; }

	virtual bool			Deploy( void );								// returns true is deploy was successful
	virtual bool			Holster( CBaseCombatWeapon *pSwitchingTo = nullptr );

		 void Precache_Impl();
		 int  UpdateTransmitState_Impl();
		 int  ShouldTransmit_Impl( const CCheckTransmitInfo *pInfo );
		virtual void GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
		 unsigned int PhysicsSolidMaskForEntity_Impl( void ) const;

		virtual bool ShouldRemoveOnRoundRestart();

        void SetDroppedFromDeath (bool droppedFromDeath) { m_bDroppedFromDeath = droppedFromDeath; }
	
		void Think_Impl( void );
		void ResetToLastValidPlayerHeldPosition();
		 void PhysicsTouchTriggers_Impl(const Vector *pPrevAbsOrigin = nullptr);

private:
		Vector m_vecLastValidPlayerHeldPosition;
public:

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

class CBaseCombatWeapon;
class CCSWeaponInfo;
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

	 void Spawn_Impl();
	 void Precache_Impl();

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

	virtual const char		*GetWorldModel( void ) const;

private:

	const CCSWeaponInfo*	m_pWeaponInfo;

	CWeaponCSBaseGun( const CWeaponCSBaseGun & );

	int m_silencedModelIndex;
	bool m_inPrecache;

};

class CBasePlayer;
class CCSWeaponInfo;

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

	virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo );

private:
	CDecoyGrenade( const CDecoyGrenade& );
};

class CBasePlayer;
class CCSWeaponInfo;

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

		virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo );

	CFlashbang( const CFlashbang & ) {}
};

class CBasePlayer;
class CCSWeaponInfo;

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

	virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo );
	
	CHEGrenade( const CHEGrenade & ) {}
};

class CWeaponCSBase;

// Purpose: Class to control 'aim-down-sights' aka "IronSight" weapon functionality


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
	
	 void Precache_Impl();

	void Spawn_Impl();
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

	bool m_swingLeft;

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

class CBasePlayer;
class CCSWeaponInfo;

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

	virtual void 	EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo );
	 void 	Precache_Impl( void );

	virtual void	Drop( const Vector& vecVelocity );

private:
	CMolotovGrenade( const CMolotovGrenade& );

private:
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

	//DECLARE_DATADESC();

	virtual void	EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo );
	 void 	Precache_Impl( void );

private:
	CIncendiaryGrenade( const CIncendiaryGrenade& );

	/*
private:
#if defined( CLIENT_DLL )
	ks::tracked_ref<CNewParticleEffect> m_molotovParticleEffect;
#endif
	*/
};

class CBasePlayer;
class CCSWeaponInfo;

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

	virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo );
	virtual void ShotDetonate( CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo );

private:
	CSensorGrenade( const CSensorGrenade& );
};

class CBasePlayer;
class CCSWeaponInfo;

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

	void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo );

	CSmokeGrenade( const CSmokeGrenade & ) {}
};

class CBaseEntity;
class CBaseTrigger;

using namespace ::ks::reflect;


// Purpose: No defuse area ent

class CNoDefuseArea : public CBaseTrigger
{
public:
	DECLARE_CLASS( CNoDefuseArea, CBaseTrigger );

	void Spawn_Impl();
	void NoDefuseAreaTouch( CBaseEntity* pOther );
};

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

class CBaseCombatCharacter;
class CCSWeaponInfo;
struct inputdata_t;

using namespace ::ks::reflect;


class CHEGrenadeProjectile : public CBaseCSGrenadeProjectile
{
public:
	DECLARE_CLASS( CHEGrenadeProjectile, CBaseCSGrenadeProjectile );

	DECLARE_DATADESC();

// Overrides.
public:
	CHEGrenadeProjectile() {}

	 void Spawn_Impl();
	 void Precache_Impl();
	virtual void BounceSound( void );
	virtual void Detonate();
	virtual const char *GetParticleSystemName( int pointContents, surfacedata_t *pdata );

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_EXPLOSIVE; }

// Grenade stuff.
public:

	static CHEGrenadeProjectile* Create( 
		const Vector &position, 
		const QAngle &angles, 
		const Vector &velocity, 
		const AngularImpulse &angVelocity, 
		CBaseCombatCharacter *pOwner, 
		const CCSWeaponInfo& weaponInfo,
		float timer );

	void SetTimer( float timer );

	[[= ks::reflect::Input{ .name = "InitializeSpawnFromWorld", .type = FIELD_VOID } ]] void	InitializeSpawnFromWorld( inputdata_t &inputdata );

private:
	float m_flDetonateTime;
};

class CBaseEntity;

using namespace ::ks::reflect;

class CInfoViewParameters : public CBaseEntity
{
public:
	DECLARE_CLASS( CInfoViewParameters, CBaseEntity );
	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "ViewMode" } ]] int m_nViewMode;
};

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

	 void	Precache_Impl( void );

	virtual bool	CanPrimaryAttack( void );

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_HEALTHSHOT; }

	void DropHealthshot( void );
	// CBaseBeltItem
	virtual bool CanUseOnSelf( CCSPlayer *pPlayer );
	virtual void OnStartUse( CCSPlayer *pPlayer );
	virtual float GetUseTimerDuration( void );
	virtual void WeaponIdle();

	virtual void CompleteUse( CCSPlayer *pPlayer );

private:
	CItem_Healthshot( const CItem_Healthshot & ) {}
};

struct inputdata_t;

using namespace ::ks::reflect;

class CMapInfo : public CPointEntity
{
public :

	DECLARE_DATADESC();
	DECLARE_CLASS( CMapInfo, CPointEntity );
	
	CMapInfo();
	virtual ~CMapInfo();

	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	void Spawn_Impl();

	[[= ks::reflect::Input{ .name = "FireWinCondition", .type = FIELD_INTEGER } ]] void InputFireWinCondition( inputdata_t &inputdata );

public:
	int m_iBuyingStatus;
	float m_flBombRadius;
	[[= ks::reflect::Key{ .name = "petpopulation" } ]] int m_iPetPopulation;
	[[= ks::reflect::Key{ .name = "usenormalspawnsfordm" } ]] bool m_bUseNormalSpawnsForDM;
	[[= ks::reflect::Key{ .name = "disableautogenerateddmspawns" } ]] bool m_bDisableAutoGeneratedDMSpawns;
	[[= ks::reflect::Key{ .name = "botmaxvisiondistance" } ]] float m_flBotMaxVisionDistance;
	[[= ks::reflect::Key{ .name = "hostagecount" } ]] int m_iHostageCount;
};

// The info_map_parameters entity in this map (only one is allowed for).
extern CMapInfo *g_pMapInfo;

class CBaseCombatCharacter;
class CBaseEntity;
class CCSWeaponInfo;
struct inputdata_t;

using namespace ::ks::reflect;


class CMolotovProjectile : public CBaseCSGrenadeProjectile
{
	[[= ks::reflect::NetTable{ .name = "DT_MolotovProjectile" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CMolotovProjectile, CBaseCSGrenadeProjectile );
	DECLARE_NETWORKCLASS();
	DECLARE_DATADESC();

// Overrides.
public:
	 void Spawn_Impl( void );
	 void Precache_Impl( void );

	virtual void BounceTouch( CBaseEntity *other );
	virtual void BounceSound( void );

	virtual void Detonate( void );

//	virtual int GetDamageType( void ) const { return DMG_BURN; }
	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_FIRE; }

	void SetIsIncGrenade( bool bIsIncGrenade ) { m_bIsIncGrenade = bIsIncGrenade; }

// Grenade stuff.
public:

	static CMolotovProjectile* Create( 
		const Vector &position, 
		const QAngle &angles, 
		const Vector &velocity, 
		const AngularImpulse &angVelocity, 
		CBaseCombatCharacter *pOwner,
		const CCSWeaponInfo& weaponInfo );

	[[= ks::reflect::Input{ .name = "InitializeSpawnFromWorld", .type = FIELD_VOID } ]] void	InitializeSpawnFromWorld( inputdata_t &inputdata );

protected:
	CNetworkVar( bool, m_bIsIncGrenade, [[= ks::reflect::Net{} ]] );

private:
	void DetonateThink( void );
	void SetTimer( float timer );

	IntervalTimer m_stillTimer;
	IntervalTimer m_throwDetTimer;
	float m_flDetonateTime;
};

class CNavArea;
class HidingSpot;

using namespace ::ks::reflect;


class CPointHidingSpot : public CServerOnlyPointEntity
{
	DECLARE_CLASS( CPointHidingSpot, CServerOnlyPointEntity )
public:
	CPointHidingSpot();
	DECLARE_DATADESC();

	void Activate_Impl();
	void UpdateOnRemove_Impl();
	void PostActivateSetupThink();
	void DetachFromHidingSpot();
protected:
	CNavArea *m_pNavArea;		// nav our hiding spot is associated with
	HidingSpot *m_pSpot;
};

class CBaseCombatCharacter;
class CBaseEntity;
class CCSWeaponInfo;

using namespace ::ks::reflect;


struct SensorGrenadeWeaponProfile;

class CSensorGrenadeProjectile : public CBaseCSGrenadeProjectile//, public CBreakableProp
{
	[[= ks::reflect::NetTable{ .name = "DT_SensorGrenadeProjectile" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CSensorGrenadeProjectile, CBaseCSGrenadeProjectile );
	DECLARE_NETWORKCLASS();

// Overrides.
public:
	 void Spawn_Impl( void );
	 void Precache_Impl( void );
	virtual void Detonate( void );
	virtual void BounceTouch( CBaseEntity *other );
	virtual void BounceSound( void );

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_SENSOR; }

// Grenade stuff.
	static CSensorGrenadeProjectile* Create( 
		const Vector &position, 
		const QAngle &angles, 
		const Vector &velocity, 
		const AngularImpulse &angVelocity, 
		CBaseCombatCharacter *pOwner,
		const CCSWeaponInfo& weaponInfo );	

private:
	void Think_Arm( void );
	void Think_Remove( void );
	void SensorThink( void );
	void SetTimer( float timer );
	void DoDetectWave( void );

	float m_fExpireTime;
	float m_fNextDetectPlayerSound;

	EHANDLE m_hDisplayGrenade;
};

class CBaseCombatCharacter;
class CCSWeaponInfo;

using namespace ::ks::reflect;


class CSmokeGrenadeProjectile : public CBaseCSGrenadeProjectile
{
	[[= ks::reflect::NetTable{ .name = "DT_SmokeGrenadeProjectile" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CSmokeGrenadeProjectile, CBaseCSGrenadeProjectile );
	DECLARE_NETWORKCLASS();

public:
// Overrides.
public:
	 void Spawn_Impl();
	 void Precache_Impl();
	virtual void Detonate();
	virtual void OnBounced( void );
	virtual void BounceSound( void );

	void Think_Detonate();
	void Think_Fade();
	void Think_Remove();

	void SmokeDetonate( void );
	void RemoveGrenadeFromLists( void );

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_SMOKE; }

	 int UpdateTransmitState_Impl() { return SetTransmitState( FL_EDICT_ALWAYS ); }

// Grenade stuff.
public:

	static CSmokeGrenadeProjectile* Create( 
		const Vector &position, 
		const QAngle &angles, 
		const Vector &velocity, 
		const AngularImpulse &angVelocity, 
		CBaseCombatCharacter *pOwner,
		const CCSWeaponInfo& weaponInfo );

	void SetTimer( float timer );

	CNetworkVar( int, m_nSmokeEffectTickBegin, [[= ks::reflect::Net{ .bits = -1 } ]] );
	CNetworkVar( bool, m_bDidSmokeEffect, [[= ks::reflect::Net{} ]] );
	Vector m_vSmokeColor;
	float m_flLastBounce;
};

void TE_FireBullets( 
	int	iPlayerIndex,
	uint16 nItemDefIndex,
	const Vector &vOrigin,
	const QAngle &vAngles,
	int	iWeaponID,
	int	iMode,
	int iSeed,
	float fInaccuracy,
	float fSpread,
	float fAccuracyFishtail,
	int iSoundType,
	float flRecoilIndex
	);

void TE_PlantBomb( int iPlayerIndex, const Vector &vOrigin, PlantBombOption_t option );

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

	void Reset();

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

	void Reset();

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

	void Reset();

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

	void Reset();

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

	void Reset();

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

class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CBreakableProp;
class CCSPlayer;
class CTakeDamageInfo;

using namespace ::ks::reflect;


// Purpose: The CS game stats header

// forward declares

const float cDisseminationTimeHigh = 0.25f;		// Time interval for high priority stats sent to the player
const float cDisseminationTimeLow = 2.5f;	// Time interval for medium priority stats sent to the player

int GetCSLevelIndex( const char *pLevelName );

typedef struct
{
	char szGameName[8];
	byte iVersion;
	char szMapName[32];
	char ipAddr[4];
	short port;
	int serverid;
} gamestats_header_t;

typedef struct
{
	gamestats_header_t	header;
	short	iMinutesPlayed;

	short	iTerroristVictories[CS_NUM_LEVELS];
	short	iCounterTVictories[CS_NUM_LEVELS];
	short	iBlackMarketPurchases[WEAPON_MAX];

	short	iAutoBuyPurchases;
	short	iReBuyPurchases;
	short	iAutoBuyM4A1Purchases;
	short	iAutoBuyAK47Purchases;
	short	iAutoBuyFamasPurchases;
	short	iAutoBuyGalilPurchases;
	short	iAutoBuyGalilARPurchases;
	short	iAutoBuyVestHelmPurchases;
	short	iAutoBuyVestPurchases;

} cs_gamestats_t;

struct WeaponStats
{
	int shots;
	int hits;
	int kills;
	int damage;
};

//=============================================================================
//
// CS Game Stats Class
//
class CCSGameStats : public CBaseGameStats, public CGameEventListener, public CAutoGameSystemPerFrame
{
public:

	// Constructor/Destructor.
	CCSGameStats( void );
	~CCSGameStats( void );

	virtual void Clear( void );
	virtual bool Init();
	virtual void PreClientUpdate();
	
	// Overridden events
	virtual void Event_LevelInit( void );
	virtual void Event_LevelShutdown( float flElapsed );
	virtual void Event_ShotFired( CBasePlayer *pPlayer, CBaseCombatWeapon* pWeapon );
	virtual void Event_ShotHit( CBasePlayer *pPlayer, const CTakeDamageInfo &info );
	virtual void Event_PlayerKilled( CBasePlayer *pPlayer, const CTakeDamageInfo &info );
	virtual void Event_PlayerKilled_PreWeaponDrop( CBasePlayer *pPlayer, const CTakeDamageInfo &info );
	virtual void Event_PlayerConnected( CBasePlayer *pPlayer );
	virtual void Event_PlayerDisconnected( CBasePlayer *pPlayer );
	virtual void Event_WindowShattered( CBasePlayer *pPlayer );
	virtual void Event_PlayerDamage( CBasePlayer *pBasePlayer, const CTakeDamageInfo &info );
	virtual void Event_PlayerKilledOther( CBasePlayer *pAttacker, CBaseEntity *pVictim, const CTakeDamageInfo &info );

	// CSS specific events
    void Event_BombPlanted( CCSPlayer *pPlayer );
    void Event_BombDefused( CCSPlayer *pPlayer );
	void Event_BombExploded( CCSPlayer *pPlayer );
	void Event_MoneyEarned( CCSPlayer *pPlayer, int moneyEarned );
	void Event_MoneySpent( CCSPlayer* pPlayer, int moneySpent, const char *pItemName );
    void Event_HostageRescued( CCSPlayer *pPlayer );
    void Event_PlayerSprayedDecal( CCSPlayer*pPlayer );
	void Event_AllHostagesRescued();
	void Event_BreakProp( CCSPlayer *pPlayer, CBreakableProp *pProp );
	void Event_PlayerDonatedWeapon (CCSPlayer* pPlayer);
	void Event_PlayerDominatedOther( CCSPlayer* pAttacker, CCSPlayer* pVictim);
	void Event_PlayerRevenge( CCSPlayer* pAttacker );
	void Event_PlayerAvengedTeammate( CCSPlayer* pAttacker, CCSPlayer* pAvengedPlayer );
	void Event_MVPEarned( CCSPlayer* pPlayer );	
	void Event_KnifeUse( CCSPlayer* pPlayer, bool bStab, int iDamage );

	void FireGameEvent( IGameEvent *event );

	void UpdatePlayerRoundStats(int winner);
	void DumpMatchWeaponMetrics();

	const PlayerStats_t&		FindPlayerStats( CBasePlayer *pPlayer ) const;
	void						ResetPlayerStats( CBasePlayer *pPlayer );
	void						ResetKillHistory( CBasePlayer *pPlayer );
	void						ResetRoundStats();
	void						ResetPlayerClassMatchStats();

	const StatsCollection_t&	GetTeamStats( int iTeamIndex ) const;
	void						ResetAllTeamStats();
	void						ResetAllStats();
	void						ResetWeaponStats();
 	void						IncrementTeamStat( int iTeamIndex, int iStatIndex, int iAmount );
	void                        CalcDominationAndRevenge( CCSPlayer *pAttacker, CCSPlayer *pVictim, int *piDeathFlags );
    void                        CalculateOverkill(CCSPlayer* pAttacker, CCSPlayer* pVictim);
	void						PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );

	void						IncrementStat( CCSPlayer* pPlayer, CSStatType_t statId, int iValue, bool bPlayerOnly = false, bool bIncludeBotController = false );

	void						SendStatsToPlayer( CCSPlayer * pPlayer, int iMinStatPriority );

protected:	
	void						SetStat( CCSPlayer *pPlayer, CSStatType_t statId, int iValue );
	void						TrackKillStats( CCSPlayer *pAttacker, CCSPlayer *pVictim );

private:
	PlayerStats_t				m_aPlayerStats[MAX_PLAYERS+1];	// List of stats for each player for current life - reset after each death    
	StatsCollection_t			m_aTeamStats[TEAM_MAXCOUNT - FIRST_GAME_TEAM];

	float						m_fDisseminationTimerLow;		// how long since last medium priority stat update
	float						m_fDisseminationTimerHigh;		// how long since last high priority stat update

	int							m_numberOfRoundsForDirectAverages;
	int							m_numberOfTerroristEntriesForDirectAverages;
	int							m_numberOfCounterTerroristEntriesForDirectAverages;

	ks::idict< CSStatType_t >	m_PropStatTable;

	WeaponStats					m_weaponStats[WEAPON_MAX][WeaponMode_MAX];
};

extern CCSGameStats CCS_GameStats;

class CAnimationLayer;
class CBaseAnimatingOverlay;
class CCSPlayer;
class CTakeDamageInfo;
class CWeaponCSBase;

using namespace ::ks::reflect;



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

	void					AddActivityModifier( const char *szName );
	void					UpdateActivityModifiers( void );
	int						SelectSequenceFromActMods( Activity iAct );
	void					DoAnimationEvent( PlayerAnimEvent_t event, int nData = 0 );
	void					SelectDeathPose( const CTakeDamageInfo &info, int hitgroup, Activity& activity, float& yaw );

	float					m_flFlashedAmountEaseOutStart;
	float					m_flFlashedAmountEaseOutEnd;

private:

	aimmatrix_transition_t	m_tStandWalkAim;
	aimmatrix_transition_t	m_tStandRunAim;
	aimmatrix_transition_t	m_tCrouchWalkAim;

	bool					LayerToIndex( const CAnimationLayer* pLayer, int &nIndex );

	float					LerpCrouchWalkRun( float flCrouchVal, float flWalkVal, float flRunVal );

	bool					CacheSequences( void );
	const char*				GetWeaponPrefix( void );
	int						m_cachedModelIndex;

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
	bool					m_bJumping;
	float					m_flLowerBodyRealignTimer;
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

	bool					m_bDeployRateLimiting;

	float					m_flDurationMoveWeightIsTooHigh;
	float					m_flStaticApproachSpeed;

	int						m_nPreviousMoveState;
	float					m_flStutterStep;

	float					m_flActionWeightBiasRemainder;

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

class CBaseCombatCharacter;
class CCSWeaponInfo;

using namespace ::ks::reflect;


struct DecoyWeaponProfile;

class CDecoyProjectile : public CBaseCSGrenadeProjectile
{
	[[= ks::reflect::NetTable{ .name = "DT_DecoyProjectile" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CDecoyProjectile, CBaseCSGrenadeProjectile );
	DECLARE_NETWORKCLASS();

// Overrides.
public:
	 void Spawn_Impl( void );
	 void Precache_Impl( void );
	virtual void Detonate( void );
	virtual void BounceSound( void );

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_DECOY; }

// Grenade stuff.
	static CDecoyProjectile* Create( 
		const Vector &position, 
		const QAngle &angles, 
		const Vector &velocity, 
		const AngularImpulse &angVelocity, 
		CBaseCombatCharacter *pOwner,
		const CCSWeaponInfo& weaponInfo );	

private:
	void Think_Detonate( void );
	void GunfireThink( void );
	void SetTimer( float timer );

	int m_shotsRemaining;
	float m_fExpireTime;
	DecoyWeaponProfile*	m_pProfile;
	CSWeaponID	m_decoyWeaponId;
	item_definition_index_t m_decoyWeaponDefIndex;
	WeaponSound_t m_decoyWeaponSoundType;
};

class CBaseCombatCharacter;
class CCSWeaponInfo;
struct inputdata_t;

using namespace ::ks::reflect;


class CFlashbangProjectile : public CBaseCSGrenadeProjectile
{
public:
	DECLARE_CLASS( CFlashbangProjectile, CBaseCSGrenadeProjectile );

	DECLARE_DATADESC();

// Overrides.
public:
	CFlashbangProjectile();

	 void Spawn_Impl();
	 void Precache_Impl();
	virtual void BounceSound( void );
	virtual void Detonate();
	
	[[= ks::reflect::Input{ .name = "SetTimer", .type = FIELD_FLOAT } ]] void	InputSetTimer( inputdata_t &inputdata );

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_FLASH; }

// Grenade stuff.
	static CFlashbangProjectile* Create( 
		const Vector &position, 
		const QAngle &angles, 
		const Vector &velocity, 
		const AngularImpulse &angVelocity, 
		CBaseCombatCharacter *pOwner,
		const CCSWeaponInfo& weaponInfo );	
	
public:
	float m_flTimeToDetonate;

	// Count of players effected by the flash
	uint8 m_numOpponentsHit; // note: opponents are considered to be anybody not on the flasher's team.
	uint8 m_numTeammatesHit;
};

class CBaseEntity;
class CBaseTrigger;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Bomb Target Area ent

//#include "cs_player_resource.h"

class CBombTarget : public CBaseTrigger
{
public:
	DECLARE_CLASS( CBombTarget, CBaseTrigger );
	DECLARE_DATADESC();

	CBombTarget();

	void Spawn_Impl();
	virtual void ReInitOnRoundStart( void );
	void BombTargetTouch( CBaseEntity* pOther );
	void BombTargetUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	[[= ks::reflect::Input{ .name = "BombExplode", .type = FIELD_VOID } ]] void OnBombExplode( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "BombPlanted", .type = FIELD_VOID } ]] void OnBombPlanted( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "BombDefused", .type = FIELD_VOID } ]] void OnBombDefused( inputdata_t &inputdata );

	bool	IsHeistBombTarget( void ) { return m_bIsHeistBombTarget; }
	const char *GetBombMountTarget( void ){ return STRING( m_szMountTarget ); }

private:
	[[= ks::reflect::Key{ .name = "BombExplode" } ]] COutputEvent m_OnBombExplode;	//Fired when the bomb explodes
	[[= ks::reflect::Key{ .name = "BombPlanted" } ]] COutputEvent m_OnBombPlanted;	//Fired when the bomb is planted
	[[= ks::reflect::Key{ .name = "BombDefused" } ]] COutputEvent m_OnBombDefused;	//Fired when the bomb is defused

	[[= ks::reflect::Key{ .name = "heistbomb" } ]] bool		m_bIsHeistBombTarget;
	bool		m_bBombPlantedHere;
	[[= ks::reflect::Key{ .name = "bomb_mount_target" } ]] string_t	m_szMountTarget;
	EHANDLE		m_hInstructorHint;		// Hint that's used by the instructor system
};

//-----------------------------------------------------------------------------
// Purpose: A generic target entity that gets replicated to the client for displaying a hint for the CS bomb targets
//-----------------------------------------------------------------------------
class CInfoInstructorHintBombTargetA : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoInstructorHintBombTargetA, CPointEntity );

	void Spawn_Impl( void );
	int UpdateTransmitState_Impl( void )	// set transmit filter to transmit always
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

};

//-----------------------------------------------------------------------------
// Purpose: A generic target entity that gets replicated to the client for displaying a hint for the CS bomb targets
//-----------------------------------------------------------------------------
class CInfoInstructorHintBombTargetB : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoInstructorHintBombTargetB, CPointEntity );

	void Spawn_Impl( void );
	int UpdateTransmitState_Impl( void )	// set transmit filter to transmit always
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

};

class CBaseEntity;
class CBaseTrigger;

using namespace ::ks::reflect;

class CHostageRescueZone : public CBaseTrigger
{
public:
	DECLARE_CLASS( CHostageRescueZone, CBaseTrigger );

	void CHostageRescue();
	void Spawn_Impl();
	virtual void ReInitOnRoundStart( void );

	void HostageRescueTouch( CBaseEntity* pOther );

	EHANDLE		m_hInstructorHint;		// Hint that's used by the instructor system
};

//-----------------------------------------------------------------------------
// Purpose: A generic target entity that gets replicated to the client for displaying a hint for the hostage rescue zone
//-----------------------------------------------------------------------------
class CInfoInstructorHintHostageRescueZone : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoInstructorHintHostageRescueZone, CPointEntity );

	void Spawn_Impl( void );
	int UpdateTransmitState_Impl( void )	// set transmit filter to transmit always
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

};
