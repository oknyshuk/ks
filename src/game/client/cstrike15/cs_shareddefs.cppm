// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include "declare_class.h"
module ks.client:cstrike15.cs_shareddefs;

import ks.core.types;
import ks.platform;
import ks.simd;
import :util;
import :econ.econ_item_schema_minimal;
import ks.game.shareddefs;
import std;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.tier1.keyvalues;

class CEconItemDefinition;

// Purpose: Shared CS definitions.

//=============================================================================
// Econ system type stubs (econ system removed)
//=============================================================================
typedef uint16 item_definition_index_t;
inline constexpr auto INVALID_ITEM_DEF_INDEX = ((item_definition_index_t)0);

// Loadout slot positions
enum loadout_positions_t
{
	LOADOUT_POSITION_INVALID = -1,
	LOADOUT_POSITION_MELEE = 0,
	LOADOUT_POSITION_C4,
	LOADOUT_POSITION_SECONDARY0,
	LOADOUT_POSITION_SECONDARY1,
	LOADOUT_POSITION_SECONDARY2,
	LOADOUT_POSITION_SECONDARY3,
	LOADOUT_POSITION_SECONDARY4,
	LOADOUT_POSITION_SMG0,
	LOADOUT_POSITION_SMG1,
	LOADOUT_POSITION_SMG2,
	LOADOUT_POSITION_SMG3,
	LOADOUT_POSITION_SMG4,
	LOADOUT_POSITION_RIFLE0,
	LOADOUT_POSITION_RIFLE1,
	LOADOUT_POSITION_RIFLE2,
	LOADOUT_POSITION_RIFLE3,
	LOADOUT_POSITION_RIFLE4,
	LOADOUT_POSITION_RIFLE5,
	LOADOUT_POSITION_HEAVY0,
	LOADOUT_POSITION_HEAVY1,
	LOADOUT_POSITION_HEAVY2,
	LOADOUT_POSITION_HEAVY3,
	LOADOUT_POSITION_HEAVY4,
	LOADOUT_POSITION_HEAVY5,
	LOADOUT_POSITION_GRENADE0,
	LOADOUT_POSITION_GRENADE1,
	LOADOUT_POSITION_GRENADE2,
	LOADOUT_POSITION_GRENADE3,
	LOADOUT_POSITION_GRENADE4,
	LOADOUT_POSITION_GRENADE5,
	LOADOUT_POSITION_EQUIPMENT0,
	LOADOUT_POSITION_EQUIPMENT1,
	LOADOUT_POSITION_EQUIPMENT2,
	LOADOUT_POSITION_EQUIPMENT3,
	LOADOUT_POSITION_EQUIPMENT4,
	LOADOUT_POSITION_CLOTHING_HANDS,
	LOADOUT_POSITION_MUSICKIT,
	LOADOUT_POSITION_FLAIR0,
	LOADOUT_POSITION_SPRAY0,
	LOADOUT_POSITION_COUNT,

	// Cosmetic aliases (point to clothing_hands for now)
	LOADOUT_POSITION_FIRST_COSMETIC = LOADOUT_POSITION_CLOTHING_HANDS,
	LOADOUT_POSITION_LAST_COSMETIC = LOADOUT_POSITION_SPRAY0
};

// Loadout slot names for debugging
inline const char* g_szLoadoutStrings[] = {
	"melee", "c4", "secondary0", "secondary1", "secondary2", "secondary3", "secondary4",
	"smg0", "smg1", "smg2", "smg3", "smg4",
	"rifle0", "rifle1", "rifle2", "rifle3", "rifle4", "rifle5",
	"heavy0", "heavy1", "heavy2", "heavy3", "heavy4", "heavy5",
	"grenade0", "grenade1", "grenade2", "grenade3", "grenade4", "grenade5",
	"equipment0", "equipment1", "equipment2", "equipment3", "equipment4",
	"clothing_hands", "musickit", "flair0", "spray0"
};

typedef uint64 itemid_t;
typedef uint32 attrib_value_t;
inline constexpr auto INVALID_ITEM_ID = ((itemid_t)0);

// Kill eater event types (StatTrak)
enum kill_eater_event_t
{
	kKillEaterEvent_Kills = 0,
	kKillEaterEvent_PlayerKill = 0,
	kKillEaterEvent_MVPs,
	kKillEaterEvent_COUNT
};

// Forward declarations for item schema stubs (defined in weapon_parse.h)

// CCStrike15ItemDefinition and GameItemDefinition_t are aliases for CEconItemDefinition
typedef CEconItemDefinition CCStrike15ItemDefinition;
typedef CEconItemDefinition GameItemDefinition_t;

// Item found method strings (stub)
inline constexpr auto NUM_ITEM_FOUND_METHODS = 22;
extern const char *g_pszItemFoundMethodStrings[NUM_ITEM_FOUND_METHODS];

// Econ attribute hook macros (stubs - econ system removed)

/*======================*/
//      Menu stuff      //
/*======================*/

// CS-specific viewport panels
inline constexpr const char *PANEL_CHOOSE_CLASS = "choose_class";

// Buy sub menus
// #define MENU_PISTOL					"menu_pistol"
// #define MENU_SHOTGUN				"menu_shotgun"
// #define MENU_RIFLE					"menu_rifle"
// #define MENU_SMG					"menu_smg"
// #define MENU_MACHINEGUN				"menu_mg"
// #define MENU_EQUIPMENT				"menu_equip"

inline constexpr auto CSTRIKE_VISUALS_DATA_BYTES = 96;

inline constexpr auto MAX_HOSTAGES = 12;
inline constexpr auto MAX_HOSTAGE_RESCUES = 4;

inline constexpr auto MAX_MATCH_STATS_ROUNDS = 30;
inline constexpr auto MATCH_STATS_TEAM_SWAP_ROUND = 15;

inline constexpr auto GRENADE_DEFAULT_SIZE = 2.0;

// Brock H. - TR - 03/31/09
// This will allow the player to control CS bots

// demo & press build flags

// Code for subscribing to SO caches for other players is disabled on the client because
// we don't use it, and the optimization to have the server send all inventory data instead
// of the gc has broken it. If we need that in the future for some reason, re-enable and figure out 
// how to network (subscribing is expensive, probably should network from server like dota does).
//#define ENABLE_CLIENT_INVENTORIES_FOR_OTHER_PLAYERS

// E3

//=============================================================================
inline constexpr const char *CSTRIKE_DEFAULT_AVATAR = "avatar_default_64";
inline constexpr const char *CSTRIKE_DEFAULT_T_AVATAR = "avatar_default-t_64";
inline constexpr const char *CSTRIKE_DEFAULT_CT_AVATAR = "avatar_default_64";
//=============================================================================

extern const float CS_PLAYER_SPEED_RUN;
extern const float CS_PLAYER_SPEED_VIP;
extern const float CS_PLAYER_SPEED_SHIELD;
extern const float CS_PLAYER_SPEED_STOPPED;
extern const float CS_PLAYER_SPEED_HAS_HOSTAGE;
extern const float CS_PLAYER_SPEED_OBSERVER;

extern const float CS_PLAYER_SPEED_DUCK_MODIFIER;
extern const float CS_PLAYER_SPEED_WALK_MODIFIER;
extern const float CS_PLAYER_SPEED_CLIMB_MODIFIER;
extern const float CS_PLAYER_HEAVYARMOR_FLINCH_MODIFIER;

extern const float CS_PLAYER_DUCK_SPEED_IDEAL;

inline constexpr auto OLD_CROUCH_PROTOCOL_INDEX = 13546;
inline bool IsPreCrouchUpdateDemo( void ) { return (engine->IsHLTV() || engine->IsPlayingDemo()) && engine->GetConnectionDataProtocol() <= OLD_CROUCH_PROTOCOL_INDEX; }

namespace TeamJoinFailedReason
{
	enum Type
	{
		CHANGED_TOO_OFTEN,
		BOTH_TEAMS_FULL,
		TERRORISTS_FULL,
		CTS_FULL,
		CANT_JOIN_SPECTATOR,
		HUMANS_CAN_ONLY_JOIN_TS,
		HUMANS_CAN_ONLY_JOIN_CTS,
		TOO_MANY_TS,
		TOO_MANY_CTS,
	};
};

inline constexpr auto CS_HOSTAGE_TRANSTIME_PICKUP = 0.1;
inline constexpr auto CS_HOSTAGE_TRANSTIME_DROP = 0.25;
inline constexpr auto CS_HOSTAGE_TRANSTIME_RESCUE = 4.0;

enum EHostageStates_t
{
	k_EHostageStates_Idle = 0,
	k_EHostageStates_BeingUntied,
	k_EHostageStates_GettingPickedUp,
	k_EHostageStates_BeingCarried,
	k_EHostageStates_FollowingPlayer,
	k_EHostageStates_GettingDropped,
	k_EHostageStates_Rescued,
	k_EHostageStates_Dead,
};

inline constexpr auto CONSTANT_UNITS_SMOKEGRENADERADIUS = 166;
inline constexpr auto CONSTANT_UNITS_GENERICGRENADERADIUS = 115;

const float SmokeGrenadeRadius = CONSTANT_UNITS_SMOKEGRENADERADIUS;
const float FlashbangGrenadeRadius = CONSTANT_UNITS_GENERICGRENADERADIUS;
const float HEGrenadeRadius = CONSTANT_UNITS_GENERICGRENADERADIUS;
const float MolotovGrenadeRadius = CONSTANT_UNITS_GENERICGRENADERADIUS;
const float DecoyGrenadeRadius = CONSTANT_UNITS_GENERICGRENADERADIUS;

// These go in CCSPlayer::m_iAddonBits and get sent to the client so it can create
// grenade models hanging off players.
// [mlowrance] Molotov purposely left out
inline constexpr auto ADDON_FLASHBANG_1 = 0x0001;
inline constexpr auto ADDON_FLASHBANG_2 = 0x0002;
inline constexpr auto ADDON_HE_GRENADE = 0x0004;
inline constexpr auto ADDON_SMOKE_GRENADE = 0x0008;
inline constexpr auto ADDON_C4 = 0x0010;
inline constexpr auto ADDON_DEFUSEKIT = 0x0020;
inline constexpr auto ADDON_PRIMARY = 0x0040;
inline constexpr auto ADDON_PISTOL = 0x0080;
inline constexpr auto ADDON_PISTOL2 = 0x0100;
inline constexpr auto ADDON_DECOY = 0x0200;
inline constexpr auto ADDON_KNIFE = 0x0400;
inline constexpr auto ADDON_MASK = 0x0800;
inline constexpr auto ADDON_TAGRENADE = 0x1000;
inline constexpr auto NUM_ADDON_BITS = 13;

inline constexpr auto ADDON_CLIENTSIDE_HOLIDAY_HAT = ( 0x1 << NUM_ADDON_BITS );
inline constexpr auto ADDON_CLIENTSIDE_GHOST = ( 0x2 << NUM_ADDON_BITS );
inline constexpr auto ADDON_CLIENTSIDE_ASSASSINATION_TARGET = ( 0x4 << NUM_ADDON_BITS );
inline constexpr auto NUM_CLIENTSIDE_ADDON_BITS = 3;

// Indices of each weapon slot.
inline constexpr auto WEAPON_SLOT_RIFLE = 0; // (primary slot)
inline constexpr auto WEAPON_SLOT_PISTOL = 1; // (secondary slot)
inline constexpr auto WEAPON_SLOT_KNIFE = 2;
inline constexpr auto WEAPON_SLOT_GRENADES = 3;
inline constexpr auto WEAPON_SLOT_C4 = 4;

inline constexpr auto WEAPON_SLOT_FIRST = 0;
inline constexpr auto WEAPON_SLOT_LAST = 4;

// CS Team IDs.
inline constexpr auto TEAM_TERRORIST = 2;
inline constexpr auto TEAM_CT = 3;
inline constexpr auto TEAM_MAXCOUNT = 4; // update this if we ever add teams (unlikely)

inline constexpr auto TEAM_TERRORIST_BASE0 = TEAM_TERRORIST - 2;
inline constexpr auto TEAM_CT_BASE0 = TEAM_CT - 2;

// [menglish] CS specific death animation time now that freeze cam is implemented
//			in order to linger on the players body less 
inline constexpr auto CS_DEATH_ANIMATION_TIME = 0.5;
 
// [tj] The number of times you must kill a given player to be dominating them
// Should always be more than 1
inline constexpr auto CS_KILLS_FOR_DOMINATION = 4;

inline constexpr auto CS_DEATH_DOMINATION = 0x0001; // killer is dominating victim
inline constexpr auto CS_DEATH_REVENGE = 0x0002; // killer got revenge on victim

//--------------
// CSPort Specific damage flags
//--------------
inline constexpr auto DMG_HEADSHOT = (DMG_LASTGENERICFLAG<<1);

struct PlayerViewmodelArmConfig
{
	const char *szPlayerModelSearchSubStr;
	const char *szSkintoneIndex;
	const char *szAssociatedGloveModel;
	const char *szAssociatedSleeveModel;
	const char *szAssociatedSleeveModelEconOverride;
};

// The various states the player can be in during the join game process.
enum CSPlayerState
{
	// Happily running around in the game.
	// You can't move though if CSGameRules()->IsFreezePeriod() returns true.
	// This state can jump to a bunch of other states like STATE_PICKINGCLASS or STATE_DEATH_ANIM.
	STATE_ACTIVE=0,
	
	// This is the state you're in when you first enter the server.
	// It's switching between intro cameras every few seconds, and there's a level info 
	// screen up.
	STATE_WELCOME,			// Show the level intro screen.
	
	// During these states, you can either be a new player waiting to join, or
	// you can be a live player in the game who wants to change teams.
	// Either way, you can't move while choosing team or class (or while any menu is up).
	STATE_PICKINGTEAM,			// Choosing team.
	STATE_PICKINGCLASS,			// Choosing class.
	
	STATE_DEATH_ANIM,			// Playing death anim, waiting for that to finish.
	STATE_DEATH_WAIT_FOR_KEY,	// Done playing death anim. Waiting for keypress to go into observer mode.
	STATE_OBSERVER_MODE,		// Noclipping around, watching players, etc.

	STATE_GUNGAME_RESPAWN,		// Respawning the player in a gun game
	STATE_DORMANT,				// No thinking, client updates, etc

	NUM_PLAYER_STATES
};

enum e_RoundEndReason
{
	/*
	NOTE/WARNING: these enum values are stored in demo files,
	they are explicitly numbered for consistency and editing,
	do not renumber existing elements, always add new elements
	with different numeric values!
	*/
	Invalid_Round_End_Reason			= -1,
	RoundEndReason_StillInProgress		= 0,
	Target_Bombed						= 1,
	VIP_Escaped							= 2,
	VIP_Assassinated					= 3,
	Terrorists_Escaped					= 4,
	CTs_PreventEscape					= 5,
	Escaping_Terrorists_Neutralized		= 6,
	Bomb_Defused						= 7,
	CTs_Win								= 8,
	Terrorists_Win						= 9,
	Round_Draw							= 10,
	All_Hostages_Rescued				= 11,
	Target_Saved						= 12,
	Hostages_Not_Rescued				= 13,
	Terrorists_Not_Escaped				= 14,
	VIP_Not_Escaped						= 15,
	Game_Commencing						= 16,
	Terrorists_Surrender				= 17,
	CTs_Surrender						= 18,
	Terrorists_Planted					= 19,
	CTs_ReachedHostage					= 20,
	RoundEndReason_Count				= 21,
};

enum GamePhase
{
	GAMEPHASE_WARMUP_ROUND,
	GAMEPHASE_PLAYING_STANDARD,	
	GAMEPHASE_PLAYING_FIRST_HALF,
	GAMEPHASE_PLAYING_SECOND_HALF,
	GAMEPHASE_HALFTIME,
	GAMEPHASE_MATCH_ENDED,    
	GAMEPHASE_MAX
};

enum GrenadeType_t
{
	GRENADE_TYPE_EXPLOSIVE,
	GRENADE_TYPE_FLASH,
	GRENADE_TYPE_FIRE,
	GRENADE_TYPE_DECOY,
	GRENADE_TYPE_SMOKE,
	GRENADE_TYPE_SENSOR,

	GRENADE_TYPE_TOTAL,
};

inline constexpr auto PUSHAWAY_THINK_INTERVAL = (1.0f / 20.0f);
inline constexpr auto GLOWUPDATE_DEFAULT_THINK_INTERVAL = 0.2f;

inline constexpr auto CS_MAX_PLAYER_MODELS = 10;
inline constexpr auto MAX_MODEL_STRING_SIZE = 256;

inline constexpr auto CS_CLASS_NONE = 0;

inline constexpr auto MAX_ENDMATCH_VOTE_PANELS = 10;
const PlayerViewmodelArmConfig *GetPlayerViewmodelArmConfigForPlayerModel( const char* szPlayerModel );

class PlayerModelInfo
{
public:
	PlayerModelInfo();

public:
	static const char* g_customizationModelCT;
	static const char* g_customizationModelT;
	static const char *g_defaultTModel;
	static const char *g_defaultCTModel;

	static PlayerModelInfo* GetPtr( void ) { return &s_PlayerModelInfo; }

private:
	static PlayerModelInfo s_PlayerModelInfo;

};

// MVP reasons
enum CSMvpReason_t
{
	CSMVP_UNDEFINED = 0,
	CSMVP_ELIMINATION,
	CSMVP_BOMBPLANT,
	CSMVP_BOMBDEFUSE,
	CSMVP_HOSTAGERESCUE,

	CSMVP_GUNGAMEWINNER,
};

// Keep these in sync with CSClasses.
inline constexpr auto CS_MUZZLEFLASH_NONE = -1;
inline constexpr auto CS_MUZZLEFLASH_NORM = 0;
inline constexpr auto CS_MUZZLEFLASH_X = 1;

extern const char *pszWinPanelCategoryHeaders[];

// Possible results for CSPlayer::CanAcquire
namespace AcquireResult
{
	enum Type
	{
		Allowed,
		InvalidItem,
		AlreadyOwned,
		AlreadyPurchased,
		ReachedGrenadeTypeLimit,
		ReachedGrenadeTotalLimit,
		NotAllowedByTeam,
		NotAllowedByMap,
		NotAllowedByMode,
		NotAllowedForPurchase,
		NotAllowedByProhibition,
	};
}

// Possible results for CSPlayer::CanAcquire
namespace AcquireMethod
{
	enum Type
	{
		PickUp,
		Buy,
	};
}

// Results for CSPlayer::CanProgressQuest()
namespace QuestProgress
{
	enum Reason
	{
		// Default value: not initialized yet (game rules or inventory data not yet retrieved)
		QUEST_NONINITIALIZED,

		// Quest can make progression
		QUEST_OK,

		// Quest can make progression if more humans connect to server
		QUEST_NOT_ENOUGH_PLAYERS,

		// Quest can make progression once warmup is ended.
		// This is only verified on client, and could be mismatched between client and server.
		QUEST_WARMUP,

		// Playing on community server, quests only enabled on Valve servers
		QUEST_NONOFFICIAL_SERVER,

		// Don't have the challenge coin 
		QUEST_NO_ENTITLEMENT,

		// User has no active quest
		QUEST_NO_QUEST,

		// User isn't human
		QUEST_PLAYER_IS_BOT,

		// Doesn't match state required by quest
		QUEST_WRONG_MAP,
		QUEST_WRONG_MODE,

		// Client thinks it has quest, but the server doesn't know about it
		QUEST_NOT_SYNCED_WITH_SERVER,

		// Sentinel entry, never used except for maximum size verification
		QUEST_REASON_MAX,
	};

	const int QuestReasonBits = 4;
	static_assert( QUEST_REASON_MAX <= ( 1 << QuestReasonBits ) );

	const char* ReasonString(Reason reason); // for use by UI code, so that values stay consistent if enum changes
}

class CEconItemView;
class CWeaponCSBase;
class FileWeaponInfo_t;


//--------------------------------------------------------------------------------------------------------
enum CSWeaponType
{
	WEAPONTYPE_KNIFE=0,	
	WEAPONTYPE_PISTOL,
	WEAPONTYPE_SUBMACHINEGUN,
	WEAPONTYPE_RIFLE,
	WEAPONTYPE_SHOTGUN,
	WEAPONTYPE_SNIPER_RIFLE,
	WEAPONTYPE_MACHINEGUN,
	WEAPONTYPE_C4,
	WEAPONTYPE_GRENADE,
	WEAPONTYPE_EQUIPMENT,
	WEAPONTYPE_STACKABLEITEM,
	WEAPONTYPE_UNKNOWN
};

/*
	NOTE!!!! Please try not to change the order. If you need to add something, please add it to the end.
	Anytime the order of the weaponID's change, we need to updated a bunch of tables in a couple DB's. Also,
	changing the order can invalidate saved queries and in general makes using the OGS stats difficult.
*/
//--------------------------------------------------------------------------------------------------------
enum CSWeaponID
{
	WEAPON_NONE = 0,

	WEAPON_FIRST,

	WEAPON_DEAGLE = WEAPON_FIRST,
	WEAPON_ELITE,
	WEAPON_FIVESEVEN,
	WEAPON_GLOCK,
	WEAPON_P228,
	WEAPON_USP,

	WEAPON_AK47,
	WEAPON_AUG,
	WEAPON_AWP,
	WEAPON_FAMAS,
	WEAPON_G3SG1,
	WEAPON_GALIL,
	WEAPON_GALILAR,
	WEAPON_M249,
	WEAPON_M3,
	WEAPON_M4A1,
	WEAPON_MAC10,
	WEAPON_MP5NAVY,
	WEAPON_P90,
	WEAPON_SCOUT,
	WEAPON_SG550,
	WEAPON_SG552,
	WEAPON_TMP,
	WEAPON_UMP45,
	WEAPON_XM1014,

	WEAPON_BIZON,
	WEAPON_MAG7,
	WEAPON_NEGEV,
	WEAPON_SAWEDOFF,
	WEAPON_TEC9,
	WEAPON_TASER,

	WEAPON_HKP2000,
	WEAPON_MP7,
	WEAPON_MP9,
	WEAPON_NOVA,
	WEAPON_P250,
	WEAPON_SCAR17,
	WEAPON_SCAR20,
	WEAPON_SG556,
	WEAPON_SSG08,
	WEAPON_LAST = WEAPON_SSG08,

	ITEM_FIRST,
	WEAPON_KNIFE_GG = ITEM_FIRST,
	WEAPON_KNIFE,

	WEAPON_FLASHBANG,
	WEAPON_HEGRENADE,
	WEAPON_SMOKEGRENADE,
	WEAPON_MOLOTOV,
	WEAPON_DECOY,
	WEAPON_INCGRENADE,
	WEAPON_TAGRENADE,
	WEAPON_C4,
	ITEM_MAX = WEAPON_C4,

	EQUIPMENT_FIRST,
	ITEM_KEVLAR = EQUIPMENT_FIRST,
	ITEM_ASSAULTSUIT,
	ITEM_HEAVYASSAULTSUIT,
	ITEM_NVG,
	ITEM_DEFUSER,
	ITEM_CUTTERS,
	EQUIPMENT_MAX,

	WEAPON_HEALTHSHOT,

	WEAPON_MAX = EQUIPMENT_MAX,		// number of weapons weapon index
};

inline constexpr auto MAX_EQUIPMENT = (EQUIPMENT_MAX - EQUIPMENT_FIRST);

enum
{
	ITEM_PRICE_KEVLAR = 650,
	ITEM_PRICE_HELMET = 350,
	ITEM_PRICE_ASSAULTSUIT = ITEM_PRICE_KEVLAR + ITEM_PRICE_HELMET,
	ITEM_PRICE_HEAVYASSAULTSUIT = 1250,
	ITEM_PRICE_DEFUSEKIT = 400,
	ITEM_PRICE_NVG = 1250,
};

struct WeaponPaintableMaterial_t;

void PrepareEquipmentInfo( void );

class WeaponRecoilData
{
public:

	WeaponRecoilData();
	~WeaponRecoilData();

	void GetRecoilOffsets( CWeaponCSBase *pWeapon, int iMode, int iIndex, float& fAngle, float &fMagnitude );
	void GenerateRecoilPatternForItemDefinition( item_definition_index_t idx );

private:

	struct RecoilOffset
	{
		float	fAngle;
		float	fMagnitude;
	};

	struct RecoilData
	{
		// A constructor of its own: GCC 16 takes the imported array member for incomplete when
		// std::make_unique value-initializes the struct.
		RecoilData() : iItemDefIndex( 0 ), recoilTable{} {}

		item_definition_index_t		iItemDefIndex;
		RecoilOffset				recoilTable[2][64];
	};

	std::map< item_definition_index_t, std::unique_ptr< RecoilData > > m_mapRecoilTables;

	void GenerateRecoilTable( RecoilData *data );

};

//--------------------------------------------------------------------------------------------------------
class CCSWeaponInfo : public FileWeaponInfo_t
{
public:
	DECLARE_CLASS_GAMEROOT( CCSWeaponInfo, FileWeaponInfo_t );
	
	CCSWeaponInfo();
	
	virtual void Parse( ::KeyValues *pKeyValuesData, const char *szWeaponName );

// recoiltable in csweaponinfo is obsolete. remove this once confirmed that the new implementation generates the same result.
	virtual void RefreshDynamicParameters() { GenerateRecoilTable(); }

	const char *GetZoomInSound( void ) const { return m_szZoomINSound; }
	const char *GetZoomOutSound( void ) const { return m_szZoomOUTSound; }

	const Vector& GetSmokeColor() const { return m_vSmokeColor; }

// recoiltable in csweaponinfo is obsolete. remove this once confirmed that the new implementation generates the same result.
	void GetRecoilOffsets( int iMode, int iIndex, float& fAngle, float &fMagnitude ) const;

protected:

	CSWeaponType m_WeaponType;
	int m_iTeam;				// Which team can have this weapon. TEAM_UNASSIGNED if both can have it.

public:

	CSWeaponID	m_weaponId;

	int		GetWeaponPrice					( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
	bool	IsFullAuto						( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	bool	HasSilencer						( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	int		GetBullets						( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetCycleTime					( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetHeatPerShot					( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetRecoveryTimeCrouch			( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetRecoveryTimeStand			( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetRecoveryTimeCrouchFinal		( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
	float	GetRecoveryTimeStandFinal		( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
	int		GetRecoveryTransitionStartBullet( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
	int		GetRecoveryTransitionEndBullet	( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;

	int		GetRecoilSeed					( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetFlinchVelocityModifierLarge	( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetFlinchVelocityModifierSmall	( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetTimeToIdleAfterFire			( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetIdleInterval					( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetRange						( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetRangeModifier				( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
	int		GetDamage						( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetPenetration					( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	int		GetCrosshairDeltaDistance		( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	int		GetCrosshairMinDistance			( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 

	float	GetInaccuracyAltSwitch			( void ) const { return m_fInaccuracyAltSwitch; };

	float	GetInaccuracyPitchShift			( void ) const { return m_fInaccuracyPitchShift;  }
	float	GetInaccuracyAltSoundThreshhold ( void ) const { return m_fInaccuracyAltSoundThreshold; }

	float	GetMaxSpeed						( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 

	float	GetSpread						( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 0.001f ) const;
	float	GetInaccuracyCrouch				( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 0.001f ) const;
	float	GetInaccuracyStand				( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 0.001f ) const;
	float   GetInaccuracyJumpInitial		( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 0.001f ) const;
	float	GetInaccuracyJump				( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 0.001f ) const;
	float	GetInaccuracyLand				( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 0.001f ) const;
	float	GetInaccuracyLadder				( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 0.001f ) const;
	float	GetInaccuracyFire				( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 0.001f ) const;
	float	GetInaccuracyMove				( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 0.001f ) const;

	float	GetInaccuracyReload				( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 0.001f ) const;

	float	GetRecoilAngle					( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetRecoilAngleVariance			( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetRecoilMagnitude				( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetRecoilMagnitudeVariance		( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	int		GetTracerFrequency				( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 

	int		GetPrimaryClipSize				( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	int		GetSecondaryClipSize			( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const; 
	int		GetDefaultPrimaryClipSize		( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
	int		GetDefaultSecondaryClipSize		( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;

	int		GetKillAward					( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
	bool	HasBurstMode					( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
    //lwss - add some more of these that are missing
    float 	GetBurstModeTimeBetweenShots	( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
	float 	GetBurstModeCycleTime	        ( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
    float 	GetZoomedCycleTime	            ( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
    //lwss end
	bool	IsRevolver						( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
	bool	HasAlternateFastSlowReload		( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
	float	GetArmorRatio					( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
	bool	HasTraditionalScope				( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
	bool	CannotShootUnderwater			( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
	bool	DoesUnzoomAfterShot				( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
	bool	DoesHideViewModelWhenZoomed		( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
	int		GetBucketSlot					( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
	int		GetZoomLevels					( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;

	int		GetZoomFOV1						( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
	int		GetZoomFOV2						( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
	float	GetZoomTime0					( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
	float	GetZoomTime1					( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
	float	GetZoomTime2					( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;

	int		GetPrimaryReserveAmmoMax				( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;
	int		GetSecondaryReserveAmmoMax				( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const;

	CSWeaponType GetWeaponType						( const CEconItemView* pWepView = nullptr ) const;
	const char* GetAddonLocation					( const CEconItemView* pWepView = nullptr ) const;
	const char* GetEjectBrassEffectName				( const CEconItemView* pWepView = nullptr ) const;
	const char* GetTracerEffectName					( const CEconItemView* pWepView = nullptr ) const;
	const char* GetMuzzleFlashEffectName_1stPerson	( const CEconItemView* pWepView = nullptr ) const;
	const char* GetMuzzleFlashEffectName_1stPersonAlt	( const CEconItemView* pWepView = nullptr ) const;
	const char* GetMuzzleFlashEffectName_3rdPerson	( const CEconItemView* pWepView = nullptr ) const;
	const char* GetMuzzleFlashEffectName_3rdPersonAlt	( const CEconItemView* pWepView = nullptr ) const;
	const char* GetHeatEffectName					( const CEconItemView* pWepView = nullptr ) const;
	const char* GetPlayerAnimationExtension			( const CEconItemView* pWepView = nullptr ) const;
	void		SetUsedByTeam						( int nTeam )	{ m_iTeam = nTeam; }
	int			GetUsedByTeam						( const CEconItemView* pWepView = nullptr ) const;

	bool		CanBeUsedWithShield() const { return m_bCanUseWithShield; }
	float		GetBotAudibleRange() const { return m_flBotAudibleRange; }
	const char* GetWrongTeamMsg() const { return m_WrongTeamMsg; }
	const char* GetAnimExtension() const { return m_szAnimExtension; }
	const char* GetShieldViewModel() const { return m_szShieldViewModel; }
	const char* GetSilencerModel() const { return m_szSilencerModel; }
	float		GetAddonScale() const { return m_flAddonScale; }
	float		GetThrowVelocity() const { return m_fThrowVelocity; }

	const char* GetAddonModel						( const CEconItemView* pWepView = nullptr ) const;
	const std::vector< WeaponPaintableMaterial_t >* GetPaintData( const CEconItemView* pWepView = nullptr ) const;

// recoiltable in csweaponinfo is obsolete. remove this once confirmed that the new implementation generates the same result.
	void GenerateRecoilTable();

	void SetWeaponPrice( int price )	{ m_iWeaponPrice = price; };
	void SetWeaponType( CSWeaponType type )	{ m_WeaponType = type; };

private:
	static bool m_bCSWeaponInfoLookupInitialized;

	bool	m_bFullAuto;		// is this a fully automatic weapon?
	float	m_flHeatPerShot;
	int		m_iWeaponPrice;
	float	m_flArmorRatio;
	float	m_flMaxSpeed[2];	// How fast the player can run while this is his primary weapon.

	int		m_iCrosshairMinDistance;
	int		m_iCrosshairDeltaDistance;

	// Parameters for FX_FireBullets:
	float	m_flPenetration;
	int		m_iDamage;
	float	m_flRange;
	float	m_flRangeModifier;
	int		m_iBullets;
	float	m_flCycleTime;
	float	m_flCycleTimeAlt;

   	char	m_szHeatEffectName[MAX_WEAPON_STRING];

	Vector	m_vSmokeColor;

   	char	m_szMuzzleFlashEffectName_1stPerson[MAX_WEAPON_STRING];
	char	m_szMuzzleFlashEffectName_3rdPerson[MAX_WEAPON_STRING];
	char	m_szEjectBrassEffectName[MAX_WEAPON_STRING];
	char	m_szTracerEffectName[MAX_WEAPON_STRING];
	int		m_iTracerFequency;

	// variables for new accuracy model
	float m_fSpread[2];
	float m_fInaccuracyCrouch[2];
	float m_fInaccuracyStand[2];
	float m_fInaccuracyJump[2];
	float m_fInaccuracyLand[2];
	float m_fInaccuracyLadder[2];
	float m_fInaccuracyImpulseFire[2];
	float m_fInaccuracyMove[2];
	float m_fRecoveryTimeStand;
	float m_fRecoveryTimeCrouch;
	float m_fRecoveryTimeStandFinal;
	float m_fRecoveryTimeCrouchFinal;
	float m_fInaccuracyReload;
	float m_fInaccuracyAltSwitch;
	float m_fInaccuracyPitchShift;
	float m_fInaccuracyAltSoundThreshold;
	float m_fRecoilAngle[2];
	float m_fRecoilAngleVariance[2];
	float m_fRecoilMagnitude[2];
	float m_fRecoilMagnitudeVariance[2];
	int   m_iRecoilSeed;

	float m_fFlinchVelocityModifierLarge;			// velocity modifier for anyone hit by this weapon
	float m_fFlinchVelocityModifierSmall;			// velocity modifier for anyone hit by this weapon

	// Delay until the next idle animation after shooting.
	float	m_flTimeToIdleAfterFire;
	float	m_flIdleInterval;

// recoiltable in csweaponinfo is obsolete. remove this once confirmed that the new implementation generates the same result.
	struct RecoilOffset
	{
		float	fAngle;
		float	fMagnitude;
	};
	RecoilOffset	m_recoilTable[2][64];

	int		m_iZoomLevels;
	int		m_iZoomFov[ 3 ];
	float	m_fZoomTime[ 3 ];
	bool	m_bHideViewModelZoomed;
	char	m_szZoomINSound[ MAX_WEAPON_STRING ];
	char	m_szZoomOUTSound[ MAX_WEAPON_STRING ];

	float m_flBotAudibleRange;	// How far away a bot can hear this weapon.

	bool  m_bCanUseWithShield;

	char m_WrongTeamMsg[ 32 ];	// Reference to a string describing the error if someone tries to buy
	// this weapon but they're on the wrong team to have it.
	// Zero-length if no specific message for this weapon.

	char m_szAnimExtension[ 16 ];
	char m_szShieldViewModel[ 64 ];

	char m_szAddonModel[ MAX_WEAPON_STRING ];		// If this is set, it is used as the addon model. Otherwise, szWorldModel is used.
	char m_szAddonLocation[ MAX_WEAPON_STRING ];	//If this is set, the weapon will look for an attachment location with this name. Otherwize the default is used based on weapon type.
	char m_szSilencerModel[ MAX_WEAPON_STRING ];	// Alternate model with silencer attached

	float m_flAddonScale;

	// grenade throw parameters
	float	m_fThrowVelocity;

	int		m_iKillAward;

};

//--------------------------------------------------------------------------------------------------------
// Utility conversion functions 
//--------------------------------------------------------------------------------------------------------
const char* WeaponClassAsString( CSWeaponType weaponType );
CSWeaponType WeaponClassFromString( const char * weaponType );
CSWeaponType WeaponClassFromWeaponID( CSWeaponID weaponID );
const char* WeaponIdAsString( CSWeaponID weaponID );
CSWeaponID WeaponIdFromString( const char *szWeaponName );
const char *WeaponIDToAlias( int id );
CSWeaponID AliasToWeaponID( const char *szAlias );
const CCSWeaponInfo* GetWeaponInfo( CSWeaponID weaponID );
bool IsGunWeapon( CSWeaponType weaponType );
