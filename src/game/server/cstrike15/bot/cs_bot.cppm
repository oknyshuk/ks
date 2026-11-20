// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <time.h>
#include "edict.h"
#include "igameevents.h"
#include "cs_bot_manager.h"
#include "declare_class.h"
#include "responserules/response_types.h"
#include "../../ai_criteria.h"
module ks.server:cstrike15.bot.cs_bot;

import std;
import ks.core.platform;
import ks.core.random;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.reflect_wirevar;
import :vscript;
import :baseentity;
import :fish;
import :cstrike15.bot.bot;
import :cstrike15.cs_player;
import :cstrike15.cs_playeranimstate;
import :sharedvar;
import ks.core.basetypes;
import ks.core.wchartypes;
import ks.tier1.exprevaluator;
import :nav_4;
import :player;
import ks.game.shareddefs;

class CCSBotManager;

class CBaseEntity;
class CBasePlayer;
class CBotManager;
class CCSPlayer;
class CNavArea;
class CWeaponCSBase;
struct Extent;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

extern ConVar mp_friendlyfire;
extern ConVar throttle_expensive_ai;

class CBasePlayerWeapon;

/**
 * Given one team, return the other
 */
inline int OtherTeam( int team )
{
	return (team == TEAM_TERRORIST) ? TEAM_CT : TEAM_TERRORIST;
}

// accessor for CS-specific bots
inline CCSBotManager *TheCSBots( void )
{
	return reinterpret_cast< CCSBotManager * >( TheBots );
}

//--------------------------------------------------------------------------------------------------------------
class BotEventInterface
{
public:
	const char *GetEventName( void ) const		{ return m_eventName; }
	GameEventCallback_t GetCallback( void ) const	{ return m_callback; }

protected:
	BotEventInterface( const char *eventName, GameEventCallback_t callback ) : m_eventName( eventName ), m_callback( callback ) { }

private:
	const char *m_eventName;
	GameEventCallback_t m_callback;		// the event class's FireGameEvent
};

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
/**
 * Macro to set up an OnEventClass() in TheCSBots.
 */
#define DECLARE_BOTMANAGER_EVENT_LISTENER( BotManagerSingleton, EventClass, EventName ) \
	public: \
	void On##EventClass( IGameEvent *data ); \
	private: \
	class EventClass##Event : public BotEventInterface \
	{ \
		bool m_enabled; \
	public: \
		EventClass##Event( void ) : BotEventInterface( #EventName, GameEventCallback_t( std::cw<&EventClass##Event::FireGameEvent>, this ) ) \
		{ \
			gameeventmanager->AddListener( static_cast< BotEventInterface * >( this ), GetCallback(), #EventName, true ); \
			m_enabled = true; \
		} \
		~EventClass##Event( void ) \
		{ \
			if ( m_enabled ) gameeventmanager->RemoveListener( static_cast< BotEventInterface * >( this ) ); \
		} \
		void Enable( bool enable ) \
		{ \
			m_enabled = enable; \
			if ( enable ) \
				gameeventmanager->AddListener( static_cast< BotEventInterface * >( this ), GetCallback(), #EventName, true ); \
			else \
				gameeventmanager->RemoveListener( static_cast< BotEventInterface * >( this ) ); \
		} \
		bool IsEnabled( void ) const { return m_enabled; } \
		void FireGameEvent( IGameEvent *event ) \
		{ \
			BotManagerSingleton()->On##EventClass( event ); \
		} \
	}; \
	EventClass##Event m_##EventClass##Event;

//--------------------------------------------------------------------------------------------------------------
#define DECLARE_CSBOTMANAGER_EVENT_LISTENER( EventClass, EventName ) DECLARE_BOTMANAGER_EVENT_LISTENER( TheCSBots, EventClass, EventName )

//--------------------------------------------------------------------------------------------------------------
/**
 * Macro to propogate an event from the bot manager to all bots
 */
// CCSBot is the only bot type

//--------------------------------------------------------------------------------------------------------------
//
// The manager for Counter-Strike specific bots
//
class CCSBotManager : public CBotManager
{
public:
	CCSBotManager();

	CBasePlayer *AllocateAndBindBotEntity( edict_t *ed );	///< allocate the appropriate entity for the bot and bind it to the given edict
	CBasePlayer *AllocateBotEntity( void );					///< factory method to allocate the appropriate entity for the bot

	void ClientDisconnect( CBaseEntity *entity );
	bool ClientCommand( CBasePlayer *player, const CCommand &args );

	void ServerActivate( void );
	void ServerDeactivate( void );
	bool ServerCommand( const char *cmd );
	bool IsServerActive( void ) const { return m_serverActive; }

	void RestartRound( void );						///< invoked when a new round begins
	void StartFrame( void );						///< called each frame

	unsigned int GetPlayerPriority( CBasePlayer *player ) const;	///< return priority of player (0 = max pri)
	bool IsImportantPlayer( CCSPlayer *player ) const;				///< return true if player is important to scenario (VIP, bomb carrier, etc)

	void ExtractScenarioData( void );							///< search the map entities to determine the game scenario and define important zones

	// difficulty levels -----------------------------------------------------------------------------------------
	static BotDifficultyType GetDifficultyLevel( void )		
	{
		static ConVarRef sv_mmqueue_reservation( "sv_mmqueue_reservation" );
		if ( sv_mmqueue_reservation.GetString()[0] == 'Q' && CSGameRules() && !CSGameRules()->IsPlayingCooperativeGametype() )
			return BOT_EASY;	// Queue matchmaking always subs people with easy bots
		if (cv_bot_difficulty.GetFloat() < 0.9f)
			return BOT_EASY;
		if (cv_bot_difficulty.GetFloat() < 1.9f)
			return BOT_NORMAL;
		if (cv_bot_difficulty.GetFloat() < 2.9f)
			return BOT_HARD;

		return BOT_EXPERT;
	}

	// the supported game scenarios ------------------------------------------------------------------------------
	enum GameScenarioType
	{
		SCENARIO_DEATHMATCH,
		SCENARIO_DEFUSE_BOMB,
		SCENARIO_RESCUE_HOSTAGES,
		SCENARIO_ESCORT_VIP
	};
	GameScenarioType GetScenario( void ) const		{ return m_gameScenario; }

	// "zones" ---------------------------------------------------------------------------------------------------
	// depending on the game mode, these are bomb zones, rescue zones, etc.

	enum { MAX_ZONES = 4 };										///< max # of zones in a map
	enum { MAX_ZONE_NAV_AREAS = 16 };							///< max # of nav areas in a zone
	struct Zone
	{
		CBaseEntity *m_entity;									///< the map entity
		CNavArea *m_area[ MAX_ZONE_NAV_AREAS ];					///< nav areas that overlap this zone
		int m_areaCount;
		Vector m_center;
		bool m_isLegacy;										///< if true, use pev->origin and 256 unit radius as zone
		int m_index;
		bool m_isBlocked;
		Extent m_extent;
	};

	const Zone *GetZone( int i ) const				{ return ( ( i >= 0 ) && ( i < m_zoneCount ) ) ? ( &m_zone[i] ) : nullptr; }
	const Zone *GetZone( const Vector &pos ) const;				///< return the zone that contains the given position
	const Zone *GetClosestZone( const Vector &pos ) const;		///< return the closest zone to the given position
	const Zone *GetClosestZone( const CBaseEntity *entity ) const;	///< return the closest zone to the given entity
	int GetZoneCount( void ) const					{ return m_zoneCount; }
	void CheckForBlockedZones( void );

	const Vector *GetRandomPositionInZone( const Zone *zone ) const;	///< return a random position inside the given zone
	CNavArea *GetRandomAreaInZone( const Zone *zone ) const;			///< return a random area inside the given zone

	/**
	 * Return the zone closest to the given position, using the given cost heuristic
	 */
	template< typename CostFunctor >
	const Zone *GetClosestZone( CNavArea *startArea, CostFunctor costFunc, float *travelDistance = nullptr ) const
	{
		const Zone *closeZone = nullptr;
		float closeDist = 99999999.9f;

		if (startArea == nullptr)
			return nullptr;

		for( int i=0; i<m_zoneCount; ++i )
		{
			if (m_zone[i].m_areaCount == 0)
				continue;

			if ( m_zone[i].m_isBlocked )
				continue;

			// just use the first overlapping nav area as a reasonable approximation
			float dist = NavAreaTravelDistance( startArea, m_zone[i].m_area[0], costFunc );

			if (dist >= 0.0f && dist < closeDist)
			{
				closeZone = &m_zone[i];
				closeDist = dist;
			}
		}

		if (travelDistance)
			*travelDistance = closeDist;

		return closeZone;
	}

	/// pick a zone at random and return it
	const Zone *GetRandomZone( void ) const
	{
		if (m_zoneCount == 0)
			return nullptr;

		int i;
		std::vector< const Zone * > unblockedZones;
		for ( i=0; i<m_zoneCount; ++i )
		{
			if ( m_zone[i].m_isBlocked )
				continue;

			unblockedZones.push_back( &(m_zone[i]) );
		}

		if ( unblockedZones.empty() )
			return nullptr;

		return unblockedZones[ RandomInt( 0, std::ssize( unblockedZones )-1 ) ];
	}

	/// returns a random spawn point for the given team (no arg means use both team spawnpoints)
	CBaseEntity *GetRandomSpawn( int team = TEAM_MAXCOUNT ) const;

	bool IsBombPlanted( void ) const			{ return m_isBombPlanted; }			///< returns true if bomb has been planted
	float GetBombPlantTimestamp( void ) const	{ return m_bombPlantTimestamp; }	///< return time bomb was planted
	bool IsTimeToPlantBomb( void ) const;											///< return true if it's ok to try to plant bomb
	CCSPlayer *GetBombDefuser( void ) const		{ return m_bombDefuser; }			///< return the player currently defusing the bomb, or NULL
	float GetBombTimeLeft( void ) const;											///< get the time remaining before the planted bomb explodes
	CBaseEntity *GetLooseBomb( void )			{ return m_looseBomb; }				///< return the bomb if it is loose on the ground
	CNavArea *GetLooseBombArea( void ) const	{ return m_looseBombArea; }			///< return area that bomb is in/near
	void SetLooseBomb( CBaseEntity *bomb );

	enum ETStrat
	{
		k_ETStrat_Rush,			// Hit the target site asap
		k_ETStrat_Slow,			// Spend time at initial engagement spots, move in if time is low or if team gets a kill
		k_ETStrat_Fake,			// One player goes to the site not being hit

		k_ETStrat_Count,
	};
	ETStrat GetTStrat( void ) const { return m_eTStrat; }
	int GetTerroristTargetSite( void ) const { return m_iTerroristTargetSite; }

	enum ECTStrat
	{
		k_ECTStrat_GuardRandomSite,		// Used for testing
		k_ECTStrat_212,					// 2 each site, one aggressive mid
		k_ECTStrat_StackSite,			// 4 on priority site, 1 alternate site

		k_ECTStrat_Count,
	};
	ECTStrat GetCTStrat( void ) const { return m_eCTStrat; }
	int GetCTPrioritySite( void ) const { return m_iCTPrioritySite; }

	float GetRadioMessageTimestamp( RadioType event, int teamID ) const;			///< return the last time the given radio message was sent for given team
	float GetRadioMessageInterval( RadioType event, int teamID ) const;				///< return the interval since the last time this message was sent
	void SetRadioMessageTimestamp( RadioType event, int teamID );
	void ResetRadioMessageTimestamps( void );

	float GetLastSeenEnemyTimestamp( void ) const	{ return m_lastSeenEnemyTimestamp; }	///< return the last time anyone has seen an enemy
	void SetLastSeenEnemyTimestamp( void ) 			{ m_lastSeenEnemyTimestamp = gpGlobals->curtime; }

	float GetRoundStartTime( void ) const			{ return m_roundStartTimestamp; }
	float GetElapsedRoundTime( void ) const			{ return gpGlobals->curtime - m_roundStartTimestamp; }	///< return the elapsed time since the current round began

	bool AllowRogues( void ) const					{ return cv_bot_allow_rogues.GetBool(); }
	bool AllowPistols( void ) const					{ return cv_bot_allow_pistols.GetBool(); }
	bool AllowShotguns( void ) const				{ return cv_bot_allow_shotguns.GetBool(); }
	bool AllowSubMachineGuns( void ) const			{ return cv_bot_allow_sub_machine_guns.GetBool(); }
	bool AllowRifles( void ) const					{ return cv_bot_allow_rifles.GetBool(); }
	bool AllowMachineGuns( void ) const				{ return cv_bot_allow_machine_guns.GetBool(); }
	bool AllowGrenades( void ) const				{ return cv_bot_allow_grenades.GetBool(); }
	bool AllowSnipers( void ) const					{ return cv_bot_allow_snipers.GetBool(); }
	bool AllowTacticalShield( void ) const			{ return false; }

	bool AllowFriendlyFireDamage( void ) const		{ return mp_friendlyfire.GetBool(); }

	bool IsWeaponUseable( const CWeaponCSBase *weapon ) const;	///< return true if the bot can use this weapon

	bool IsDefenseRushing( void ) const				{ return m_isDefenseRushing; }		///< returns true if defense team has "decided" to rush this round
	bool IsOnDefense( const CCSPlayer *player ) const;		///< return true if this player is on "defense"
	bool IsOnOffense( const CCSPlayer *player ) const;		///< return true if this player is on "offense"

	bool IsRoundOver( void ) const					{ return m_isRoundOver; }		///< return true if the round has ended

	bool BotAddCommand( int team, bool isFromConsole = false, const char *profileName = nullptr, CSWeaponType weaponType = WEAPONTYPE_UNKNOWN, BotDifficultyType difficulty = NUM_DIFFICULTY_LEVELS );	///< process the "bot_add" console command
	
	bool BotPlaceCommand( uint nTeamMask = 0xFFFFFFFF ); //Moves a bot at the location under the cursor.  For perf and lighting testing.

	// Called to mark that an expensive operation has happened this frame, used for budgeting/throttling AI
	void OnExpensiveBotOperation() { m_nNumExpensiveOperationsThisFrame ++; }
	// Query to see if we have exceeded our per-frame budget for "expensive" AI operations
	bool AllowedToDoExpensiveBotOperationThisFrame() { return !throttle_expensive_ai.GetBool() || m_nNumExpensiveOperationsThisFrame < 1; }

	void ForceMaintainBotQuota( void ) { MaintainBotQuota(); }

private:
	enum SkillType { LOW, AVERAGE, HIGH, RANDOM };

	void MaintainBotQuota( void );

	static bool m_isMapDataLoaded;							///< true if we've attempted to load map data
	bool m_serverActive;									///< true between ServerActivate() and ServerDeactivate()

	GameScenarioType m_gameScenario;						///< what kind of game are we playing

	Zone m_zone[ MAX_ZONES ];							
	int m_zoneCount;

	bool m_isBombPlanted;									///< true if bomb has been planted
	float m_bombPlantTimestamp;								///< time bomb was planted
	float m_earliestBombPlantTimestamp;						///< don't allow planting until after this time has elapsed
	CHandle<CCSPlayer> m_bombDefuser;						///< the player currently defusing a bomb
	EHANDLE m_looseBomb;									///< will be non-NULL if bomb is loose on the ground
	CNavArea *m_looseBombArea;								///< area that bomb is is/near

	bool m_isRoundOver;										///< true if the round has ended

	CountdownTimer m_checkTransientAreasTimer;				///< when elapsed, all transient nav areas should be checked for blockage

	float m_radioMsgTimestamp[ RADIO_END - RADIO_START_1 ][ 2 ];

	float m_lastSeenEnemyTimestamp;
	float m_roundStartTimestamp;							///< the time when the current round began

	bool m_isDefenseRushing;								///< whether defensive team is rushing this round or not

	int m_nNumExpensiveOperationsThisFrame;

	// Cooperative mode vars
	ETStrat m_eTStrat;										// Current plan for T
	int m_iTerroristTargetSite;								// Bomb site Ts are planing to plant at

	ECTStrat m_eCTStrat;									// Current plan for CT
	int m_iCTPrioritySite;									// Guess at which site will be attacked

	// Event Handlers --------------------------------------------------------------------------------------------
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( PlayerFootstep,		player_footstep )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( PlayerRadio,			player_radio )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( PlayerDeath,			player_death )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( PlayerFallDamage,		player_falldamage )

	DECLARE_CSBOTMANAGER_EVENT_LISTENER( BombPickedUp,			bomb_pickup )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( BombPlanted,			bomb_planted )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( BombBeep,				bomb_beep )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( BombDefuseBegin,		bomb_begindefuse )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( BombDefused,			bomb_defused )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( BombDefuseAbort,		bomb_abortdefuse )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( BombExploded,			bomb_exploded )

	DECLARE_CSBOTMANAGER_EVENT_LISTENER( RoundEnd,				round_end )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( RoundStart,			round_start )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( RoundFreezeEnd,		round_freeze_end )

	DECLARE_CSBOTMANAGER_EVENT_LISTENER( DoorMoving,			door_moving )

	DECLARE_CSBOTMANAGER_EVENT_LISTENER( BreakProp,				break_prop )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( BreakBreakable,		break_breakable )

	DECLARE_CSBOTMANAGER_EVENT_LISTENER( HostageFollows,		hostage_follows )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( HostageRescuedAll,		hostage_rescued_all )

	DECLARE_CSBOTMANAGER_EVENT_LISTENER( WeaponFire,			weapon_fire )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( WeaponFireOnEmpty,		weapon_fire_on_empty )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( WeaponReload,			weapon_reload )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( WeaponZoom,			weapon_zoom )

	DECLARE_CSBOTMANAGER_EVENT_LISTENER( BulletImpact,			bullet_impact )

	DECLARE_CSBOTMANAGER_EVENT_LISTENER( HEGrenadeDetonate,		hegrenade_detonate )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( FlashbangDetonate,		flashbang_detonate )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( SmokeGrenadeDetonate,	smokegrenade_detonate )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( MolotovDetonate,		molotov_detonate )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( DecoyDetonate,			decoy_detonate )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( DecoyFiring,			decoy_firing )
	DECLARE_CSBOTMANAGER_EVENT_LISTENER( GrenadeBounce,			grenade_bounce )

	DECLARE_CSBOTMANAGER_EVENT_LISTENER( NavBlocked,			nav_blocked )

	DECLARE_CSBOTMANAGER_EVENT_LISTENER( ServerShutdown,		server_shutdown )

	std::vector< BotEventInterface * > m_commonEventListeners;	// These event listeners fire often, and can be disabled for performance gains when no bots are present.
	bool m_eventListenersEnabled;
	void EnableEventListeners( bool enable );
};

#undef DECLARE_BOTMANAGER_EVENT_LISTENER
#undef DECLARE_CSBOTMANAGER_EVENT_LISTENER

class CBaseEntity;
class CCSBot;
class CCSBotManager;
class CHostage;

using namespace ::ks::reflect;


// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003


/**
 * This class represents the game state as known by a particular bot
 */
class CSGameState
{
public:
	CSGameState( CCSBot *owner );

	void Reset( void );

	// Event handling
	void OnHostageRescuedAll( IGameEvent *event );
	void OnRoundEnd( IGameEvent *event );
	void OnRoundStart( IGameEvent *event );
	void OnBombPlanted( IGameEvent *event );
	void OnBombDefused( IGameEvent *event );
	void OnBombExploded( IGameEvent *event );

	bool IsRoundOver( void ) const;								///< true if round has been won or lost (but not yet reset)

	// bomb defuse scenario -----------------------------------------------------------------------------

	enum BombState
	{
		MOVING,					///< being carried by a Terrorist
		LOOSE,					///< loose on the ground somewhere
		PLANTED,				///< planted and ticking
		DEFUSED,				///< the bomb has been defused
		EXPLODED				///< the bomb has exploded
	};

	bool IsBombMoving( void ) const						{ return (m_bombState == MOVING); }
	bool IsBombLoose( void ) const						{ return (m_bombState == LOOSE); }
	bool IsBombPlanted( void ) const					{ return (m_bombState == PLANTED); }
	bool IsBombDefused( void ) const					{ return (m_bombState == DEFUSED); }
	bool IsBombExploded( void ) const					{ return (m_bombState == EXPLODED); }

	void UpdateLooseBomb( const Vector &pos );					///< we see the loose bomb
	float TimeSinceLastSawLooseBomb( void ) const;				///< how long has is been since we saw the loose bomb
	bool IsLooseBombLocationKnown( void ) const;				///< do we know where the loose bomb is

	void UpdateBomber( const Vector &pos );						///< we see the bomber
	float TimeSinceLastSawBomber( void ) const;					///< how long has is been since we saw the bomber

	void UpdatePlantedBomb( const Vector &pos );				///< we see the planted bomb
	bool IsPlantedBombLocationKnown( void ) const;				///< do we know where the bomb was planted
	void MarkBombsiteAsPlanted( int zoneIndex );				///< mark bombsite as the location of the planted bomb

	enum { UNKNOWN = -1 };
	int GetPlantedBombsite( void ) const;						///< return the zone index of the planted bombsite, or UNKNOWN
	bool IsAtPlantedBombsite( void ) const;						///< return true if we are currently in the bombsite where the bomb is planted

	int GetNextBombsiteToSearch( void );						///< return the zone index of the next bombsite to search
	bool IsBombsiteClear( int zoneIndex ) const;				///< return true if given bombsite has been cleared
	void ClearBombsite( int zoneIndex );						///< mark bombsite as clear

	const Vector *GetBombPosition( void ) const;				///< return where we think the bomb is, or NULL if we don't know
	
	// hostage rescue scenario ------------------------------------------------------------------------
	CHostage *GetNearestFreeHostage( Vector *knowPos = nullptr ) const;	///< return the closest free hostage, and where we think it is (knowPos)
	const Vector *GetRandomFreeHostagePosition( void ) const;
	bool AreAllHostagesBeingRescued( void ) const;				///< return true if there are no free hostages
	bool AreAllHostagesGone( void ) const;						///< all hostages have been rescued or are dead
	void AllHostagesGone( void );								///< someone told us all the hostages are gone
	bool HaveSomeHostagesBeenTaken( void ) const				///< return true if one or more hostages have been moved by the CT's
	{
		return m_haveSomeHostagesBeenTaken;
	}
	void HostageWasTaken( void )								///< someone told us a CT is talking to a hostage
	{
		m_haveSomeHostagesBeenTaken = true;
	}

	CHostage *GetNearestVisibleFreeHostage( void ) const;

	enum ValidateStatusType
	{ 
		NO_CHANGE = 0x00,
		HOSTAGE_DIED = 0x01,
		HOSTAGE_GONE = 0x02,
		HOSTAGES_ALL_GONE = 0x04
	};
	unsigned char ValidateHostagePositions( void );				///< update our knowledge with what we currently see - returns bitflag events

	BombState GetBombState( void ) const			{ return m_bombState; }
private:
	CCSBot *m_owner;											///< who owns this gamestate

	bool m_isRoundOver;											///< true if round is over, but no yet reset

	// bomb defuse scenario ---------------------------------------------------------------------------
	void SetBombState( BombState state );

	BombState m_bombState;										///< what we think the bomb is doing

	IntervalTimer m_lastSawBomber;
	Vector m_bomberPos;

	IntervalTimer m_lastSawLooseBomb;
	Vector m_looseBombPos;

	bool m_isBombsiteClear[ CCSBotManager::MAX_ZONES ];			///< corresponds to zone indices in CCSBotManager
	int m_bombsiteSearchOrder[ CCSBotManager::MAX_ZONES ];		///< randomized order of bombsites to search
	int m_bombsiteCount;
	int m_bombsiteSearchIndex;									///< the next step in the search

	int m_plantedBombsite;										///< zone index of the bombsite where the planted bomb is
	
	bool m_isPlantedBombPosKnown;								///< if true, we know the exact location of the bomb
	Vector m_plantedBombPos;

	// hostage rescue scenario ------------------------------------------------------------------------
	struct HostageInfo
	{
		CHandle<CHostage> hostage;
		Vector knownPos;
		bool isValid;
		bool isAlive;
		bool isFree;		///< not being escorted by a CT
	}
	m_hostage[ MAX_HOSTAGES ];
	int m_hostageCount;											///< number of hostages left in map
	CountdownTimer m_validateInterval;

	CBaseEntity *GetNearestHostage( void ) const;				///< return the closest live hostage
	void InitializeHostageInfo( void );							///< initialize our knowledge of the number and location of hostages

	bool m_allHostagesRescued;
	bool m_haveSomeHostagesBeenTaken;							///< true if a hostage has been moved by a CT (and we've seen it)
};

class CCSBot;

class BotChatterInterface;
class BotProfile;
class CBaseCombatWeapon;
class CBaseDoor;
class CBaseEntity;
class CBasePlayer;
class CBasePropDoor;
class CCSBotManager;
class CCSPlayer;
class CNavArea;
class CNavLadder;
class CPushAwayEnumerator;
class CTakeDamageInfo;
class CUserCmd;
class CWeaponCSBase;
class HidingSpot;

using namespace ::ks::reflect;

//
// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003
//
// NOTE: The CS Bot code uses Doxygen-style comments. If you run Doxygen over this code, it will 
// auto-generate documentation.  Visit www.doxygen.org to download the system for free.
//

//--------------------------------------------------------------------------------------------------------------
/**
 * For use with player->m_rgpPlayerItems[]
 */
enum InventorySlotType
{
	PRIMARY_WEAPON_SLOT = 1,
	PISTOL_SLOT,
	KNIFE_SLOT,
	GRENADE_SLOT,
	C4_SLOT
};

//--------------------------------------------------------------------------------------------------------------
/**
 * The definition of a bot's behavior state.  One or more finite state machines 
 * using these states implement a bot's behaviors.
 */
class BotState
{
public:
	/// the behavior states (a CCSBot owns exactly one instance of each)
	enum class Kind
	{
		Idle,
		Hunt,
		Attack,
		InvestigateNoise,
		Buy,
		MoveTo,
		FetchBomb,
		PlantBomb,
		DefuseBomb,
		PickupHostage,
		Hide,
		EscapeFromBomb,
		Follow,
		UseEntity,
		OpenDoor,
		EscapeFromFlames,
	};

	Kind GetKind( void ) const						{ return m_kind; }

	// forward to the concrete state according to the Kind (see cs_bot_statemachine.cpp)
	void OnEnter( CCSBot *bot );						///< when state is entered
	void OnUpdate( CCSBot *bot );						///< state behavior
	void OnExit( CCSBot *bot );							///< when state exited
	const char *GetName( void ) const;					///< return state name

protected:
	explicit BotState( Kind kind ) : m_kind( kind ) { }

private:
	Kind m_kind;
};

//--------------------------------------------------------------------------------------------------------------
/**
 * The state is invoked when a bot has nothing to do, or has finished what it was doing.
 * A bot never stays in this state - it is the main action selection mechanism.
 */
class IdleState : public BotState
{
public:
	IdleState( void ) : BotState( Kind::Idle ) { }

	void OnEnter( CCSBot *bot );
	void OnUpdate( CCSBot *bot );
	const char *GetName( void ) const		{ return "Idle"; }

	void UpdateCoop( CCSBot *bot );
};

//--------------------------------------------------------------------------------------------------------------
/**
 * When a bot is actively searching for an enemy.
 */
class HuntState : public BotState
{
public:
	HuntState( void ) : BotState( Kind::Hunt ) { }

	void OnEnter( CCSBot *bot );
	void OnUpdate( CCSBot *bot );
	void OnExit( CCSBot *bot );
	const char *GetName( void ) const		{ return "Hunt"; }

	void ClearHuntArea( void )						{ m_huntArea = nullptr; }

private:
	CNavArea *m_huntArea;										///< "far away" area we are moving to
};

//--------------------------------------------------------------------------------------------------------------
/**
 * When a bot has an enemy and is attempting to kill it
 */
class AttackState : public BotState
{
public:
	AttackState( void ) : BotState( Kind::Attack ) { }

	void OnEnter( CCSBot *bot );
	void OnUpdate( CCSBot *bot );
	void OnExit( CCSBot *bot );
	const char *GetName( void ) const		{ return "Attack"; }
	
	void SetCrouchAndHold( bool crouch )			{ m_crouchAndHold = crouch; }

protected:
	enum DodgeStateType
	{
		STEADY_ON,
		SLIDE_LEFT,
		SLIDE_RIGHT,
		JUMP,

		NUM_ATTACK_STATES
	};
	DodgeStateType m_dodgeState;
	float m_nextDodgeStateTimestamp;

	CountdownTimer m_repathTimer;
	float m_scopeTimestamp;

	bool m_haveSeenEnemy;										///< false if we haven't yet seen the enemy since we started this attack (told by a friend, etc)
	bool m_isEnemyHidden;										///< true we if we have lost line-of-sight to our enemy
	float m_reacquireTimestamp;									///< time when we can fire again, after losing enemy behind cover
	float m_shieldToggleTimestamp;								///< time to toggle shield deploy state
	bool m_shieldForceOpen;										///< if true, open up and shoot even if in danger

	float m_pinnedDownTimestamp;								///< time when we'll consider ourselves "pinned down" by the enemy

	bool m_crouchAndHold;
	bool m_didAmbushCheck;
	bool m_shouldDodge;
	bool m_firstDodge;

	bool m_isCoward;											///< if true, we'll retreat if outnumbered during this fight
	CountdownTimer m_retreatTimer;

	void StopAttacking( CCSBot *bot );
	void Dodge( CCSBot *bot );									///< do dodge behavior
};

//--------------------------------------------------------------------------------------------------------------
/**
 * When a bot has heard an enemy noise and is moving to find out what it was.
 */
class InvestigateNoiseState : public BotState
{
public:
	InvestigateNoiseState( void ) : BotState( Kind::InvestigateNoise ) { }

	void OnEnter( CCSBot *bot );
	void OnUpdate( CCSBot *bot );
	void OnExit( CCSBot *bot );
	const char *GetName( void ) const		{ return "InvestigateNoise"; }

private:
	void AttendCurrentNoise( CCSBot *bot );						///< move towards currently heard noise
	Vector m_checkNoisePosition;								///< the position of the noise we're investigating
	CountdownTimer m_minTimer;									///< minimum time we will investigate our current noise
};

//--------------------------------------------------------------------------------------------------------------
/**
 * When a bot is buying equipment at the start of a round.
 */
class BuyState : public BotState
{
public:
	BuyState( void ) : BotState( Kind::Buy ) { }

	void OnEnter( CCSBot *bot );
	void OnUpdate( CCSBot *bot );
	void OnExit( CCSBot *bot );
	const char *GetName( void ) const		{ return "Buy"; }

private:
	bool m_isInitialDelay;
	int m_prefRetries;											///< for retrying buying preferred weapon at current index
	int m_prefIndex;											///< where are we in our list of preferred weapons

	int m_retries;
	bool m_doneBuying;
	bool m_buyDefuseKit;
	bool m_buyGrenade;
	bool m_buyShield;
	bool m_buyPistol;
};

//--------------------------------------------------------------------------------------------------------------
/**
 * When a bot is moving to a potentially far away position in the world.
 */
class MoveToState : public BotState
{
public:
	MoveToState( void ) : BotState( Kind::MoveTo ) { }

	void OnEnter( CCSBot *bot );
	void OnUpdate( CCSBot *bot );
	void OnExit( CCSBot *bot );
	const char *GetName( void ) const		{ return "MoveTo"; }
	void SetGoalPosition( const Vector &pos )		{ m_goalPosition = pos; }
	void SetRouteType( RouteType route )			{ m_routeType = route; }

private:
	Vector m_goalPosition;										///< goal position of move
	RouteType m_routeType;										///< the kind of route to build
	bool m_radioedPlan;
	bool m_askedForCover;
};

//--------------------------------------------------------------------------------------------------------------
/**
 * When a Terrorist bot is moving to pick up a dropped bomb.
 */
class FetchBombState : public BotState
{
public:
	FetchBombState( void ) : BotState( Kind::FetchBomb ) { }

	void OnEnter( CCSBot *bot );
	void OnUpdate( CCSBot *bot );
	const char *GetName( void ) const	{ return "FetchBomb"; }
};

//--------------------------------------------------------------------------------------------------------------
/**
 * When a Terrorist bot is actually planting the bomb.
 */
class PlantBombState : public BotState
{
public:
	PlantBombState( void ) : BotState( Kind::PlantBomb ) { }

	void OnEnter( CCSBot *bot );
	void OnUpdate( CCSBot *bot );
	void OnExit( CCSBot *bot );
	const char *GetName( void ) const	{ return "PlantBomb"; }
};

//--------------------------------------------------------------------------------------------------------------
/**
 * When a CT bot is actually defusing a live bomb.
 */
class DefuseBombState : public BotState
{
public:
	DefuseBombState( void ) : BotState( Kind::DefuseBomb ) { }

	void OnEnter( CCSBot *bot );
	void OnUpdate( CCSBot *bot );
	void OnExit( CCSBot *bot );
	const char *GetName( void ) const	{ return "DefuseBomb"; }
};

//--------------------------------------------------------------------------------------------------------------
/**
 * When a CT bot is picking up a hostage
 */
class PickupHostageState : public BotState
{
public:
	PickupHostageState( void ) : BotState( Kind::PickupHostage ) { }

	void OnEnter( CCSBot *bot );
	void OnUpdate( CCSBot *bot );
	void OnExit( CCSBot *bot );
	const char *GetName( void ) const	{ return "PickupHostage"; }

	void SetEntity( CBaseEntity *entity )				{ m_entity = entity; }

private:
	EHANDLE m_entity;	
};

//--------------------------------------------------------------------------------------------------------------
/**
 * When a bot is hiding in a corner.
 * NOTE: This state also includes MOVING TO that hiding spot, which may be all the way
 * across the map!
 */
class HideState : public BotState
{
public:
	HideState( void ) : BotState( Kind::Hide ) { }

	void OnEnter( CCSBot *bot );
	void OnUpdate( CCSBot *bot );
	void OnExit( CCSBot *bot );
	const char *GetName( void ) const	{ return "Hide"; }

	void SetHidingSpot( const Vector &pos )		{ m_hidingSpot = pos; }
	const Vector &GetHidingSpot( void ) const	{ return m_hidingSpot; }

	void SetSearchArea( CNavArea *area )		{ m_searchFromArea = area; }
	void SetSearchRange( float range )			{ m_range = range; }
	void SetDuration( float time )				{ m_duration = time; }
	void SetHoldPosition( bool hold )			{ m_isHoldingPosition = hold; }

	bool IsAtSpot( void ) const					{ return m_isAtSpot; }

	float GetHideTime( void ) const;

private:
	CNavArea *m_searchFromArea;
	float m_range;

	Vector m_hidingSpot;
	bool m_isLookingOutward;
	bool m_isAtSpot;
	float m_duration;
	CountdownTimer m_hideTimer;								///< how long to hide

	bool m_isHoldingPosition;
	float m_holdPositionTime;								///< how long to hold our position after we hear nearby enemy noise

	bool m_heardEnemy;										///< set to true when we first hear an enemy
	float m_firstHeardEnemyTime;							///< when we first heard the enemy

	int m_retry;											///< counter for retrying hiding spot

	Vector m_leaderAnchorPos;								///< the position of our follow leader when we decided to hide

	bool m_isPaused;										///< if true, we have paused in our retreat for a moment
	CountdownTimer m_pauseTimer;							///< for stoppping and starting our pauses while we retreat
};

//--------------------------------------------------------------------------------------------------------------
/**
 * When a bot is attempting to flee from a bomb that is about to explode.
 */
class EscapeFromBombState : public BotState
{
public:
	EscapeFromBombState( void ) : BotState( Kind::EscapeFromBomb ) { }

	void OnEnter( CCSBot *bot );
	void OnUpdate( CCSBot *bot );
	void OnExit( CCSBot *bot );
	const char *GetName( void ) const		{ return "EscapeFromBomb"; }
};

//--------------------------------------------------------------------------------------------------------------
/**
 * When a bot is following another player.
 */
class FollowState : public BotState
{
public:
	FollowState( void ) : BotState( Kind::Follow ) { }

	void OnEnter( CCSBot *bot );
	void OnUpdate( CCSBot *bot );
	void OnExit( CCSBot *bot );
	const char *GetName( void ) const		{ return "Follow"; }

	void SetLeader( CCSPlayer *player )				{ m_leader = player; }

private:
	CHandle< CCSPlayer > m_leader;								///< the player we are following
	Vector m_lastLeaderPos;										///< where the leader was when we computed our follow path
	bool m_isStopped;
	float m_stoppedTimestamp;

	enum LeaderMotionStateType
	{
		INVALID,
		STOPPED,
		WALKING,
		RUNNING
	};
	LeaderMotionStateType m_leaderMotionState;
	IntervalTimer m_leaderMotionStateTime;

	bool m_isSneaking;
	float m_lastSawLeaderTime;
	CountdownTimer m_repathInterval;

	IntervalTimer m_walkTime;
	bool m_isAtWalkSpeed;

	float m_waitTime;
	CountdownTimer m_idleTimer;

	void ComputeLeaderMotionState( float leaderSpeed );
};

//--------------------------------------------------------------------------------------------------------------
/**
 * When a bot is actually using another entity (ie: facing towards it and pressing the use key)
 */
class UseEntityState : public BotState
{
public:
	UseEntityState( void ) : BotState( Kind::UseEntity ) { }

	void OnEnter( CCSBot *bot );
	void OnUpdate( CCSBot *bot );
	void OnExit( CCSBot *bot );
	const char *GetName( void ) const			{ return "UseEntity"; }

	void SetEntity( CBaseEntity *entity )				{ m_entity = entity; }

private:
	EHANDLE m_entity;											///< the entity we will use
};

//--------------------------------------------------------------------------------------------------------------
/**
 * When a bot is opening a door
 */
class OpenDoorState : public BotState
{
public:
	OpenDoorState( void ) : BotState( Kind::OpenDoor ) { }

	void OnEnter( CCSBot *bot );
	void OnUpdate( CCSBot *bot );
	void OnExit( CCSBot *bot );
	const char *GetName( void ) const		{ return "OpenDoor"; }

	void SetDoor( CBaseEntity *door );

	bool IsDone( void ) const						{ return m_isDone; }	///< return true if behavior is done

private:
	CHandle< CBaseDoor > m_funcDoor;									///< the func_door we are opening
	CHandle< CBasePropDoor > m_propDoor;								///< the prop_door we are opening
	bool m_isDone;
	CountdownTimer m_timeout;
};

//--------------------------------------------------------------------------------------------------------------
/**
 * When a bot is attempting to escape from a field of flames (probably from a Molotov)
 */
class EscapeFromFlamesState : public BotState
{
public:
	EscapeFromFlamesState( void ) : BotState( Kind::EscapeFromFlames ) { }

	void OnEnter( CCSBot *bot );
	void OnUpdate( CCSBot *bot );
	void OnExit( CCSBot *bot );
	const char *GetName( void ) const		{ return "EscapeFromFlames"; }

private:
	CNavArea *FindNearestNonDamagingArea( CCSBot *bot ) const;
	CountdownTimer m_searchTimer;
	CNavArea *m_safeArea;
};

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
/**
 * The Counter-strike Bot
 */
class CCSBot : public CBot< CCSPlayer >
{
public:
	DECLARE_CLASS( CCSBot, CBot< CCSPlayer > );

	CCSBot( void );												///< constructor initializes all values to zero
	~CCSBot();
	bool Initialize( const BotProfile *profile, int team );		///< prepare bot for action
	void CoopInitialize( void ); // initialize for Coop missions

	void Spawn_Impl( void );									///< (EXTEND) spawn the bot into the game
	void Touch_Impl( CBaseEntity *other );					///< (EXTEND) when touched by another entity

	void Upkeep( void );								///< lightweight maintenance, invoked frequently
	void Update( void );								///< heavyweight algorithms, invoked less often
	void BuildUserCmd( CUserCmd& cmd, const QAngle& viewangles, float forwardmove, float sidemove, float upmove, int buttons, byte impulse );
	void AvoidPlayers( CUserCmd *pCmd );				///< some game types allow players to pass through each other, this method pushes them apart
	float GetMoveSpeed( void );							///< returns current movement speed (for walk/run)

	void Walk( void );
	bool Jump( bool mustJump = false );					///< returns true if jump was started

	//- behavior properties ------------------------------------------------------------------------------------------
	float GetCombatRange( void ) const;
	bool IsRogue( void ) const;									///< return true if we dont listen to teammates or pursue scenario goals
	void SetRogue( bool rogue );
	bool IsHurrying( void ) const;								///< return true if we are in a hurry 
	void Hurry( float duration );								///< force bot to hurry
	bool IsSafe( void ) const;									///< return true if we are in a safe region
	bool IsWellPastSafe( void ) const;							///< return true if it is well past the early, "safe", part of the round
	bool IsEndOfSafeTime( void ) const;							///< return true if we were in the safe time last update, but not now
	float GetSafeTimeRemaining( void ) const;					///< return the amount of "safe time" we have left
	float GetSafeTime( void ) const;							///< return what we think the total "safe time" for this map is
	virtual void Blind( float holdTime, float fadeTime, float startingAlpha = 255 );	// player blinded by a flashbang
	bool IsUnhealthy( void ) const;								///< returns true if bot is low on health
	
	bool IsAlert( void ) const;									///< return true if bot is in heightened "alert" mode
	void BecomeAlert( void );									///< bot becomes "alert" for immediately nearby enemies

	bool IsSneaking( void ) const;								///< return true if bot is sneaking
	void Sneak( float duration );								///< sneak for given duration

	//- behaviors ---------------------------------------------------------------------------------------------------
	void Idle( void );
	bool IsIdling( void ) const;

	void Hide( CNavArea *searchFromArea = nullptr, float duration = -1.0f, float hideRange = 750.0f, bool holdPosition = false );	///< DEPRECATED: Use TryToHide() instead
	bool TryToHide( CNavArea *searchFromArea = nullptr, float duration = -1.0f, float hideRange = 750.0f, bool holdPosition = false, bool useNearest = false, const Vector *pStartPosOverride = nullptr );	///< try to hide nearby, return false if cannot
	void Hide( const Vector &hidingSpot, float duration = -1.0f, bool holdPosition = false );	///< move to the given hiding place
	bool IsHiding( void ) const;								///< returns true if bot is currently hiding
	bool IsAtHidingSpot( void ) const;							///< return true if we are hiding and at our hiding spot
	float GetHidingTime( void ) const;							///< return number of seconds we have been at our current hiding spot

	bool MoveToInitialEncounter( void );						///< move to a hiding spot and wait for initial encounter with enemy team (return false if no spots are available)

	bool TryToRetreat( float maxRange = 1000.0f, float duration = -1.0f );	///< retreat to a nearby hiding spot, away from enemies

	void Hunt( void );
	bool IsHunting( void ) const;								///< returns true if bot is currently hunting

	void Attack( CCSPlayer *victim );
	void FireWeaponAtEnemy( void );								///< fire our active weapon towards our current enemy
	void StopAttacking( void );
	bool IsAttacking( void ) const;								///< returns true if bot is currently engaging a target

	void MoveTo( const Vector &pos, RouteType route = SAFEST_ROUTE );	///< move to potentially distant position
	bool IsMovingTo( void ) const;								///< return true if we are in the MoveTo state

	void PlantBomb( void );

	void FetchBomb( void );										///< bomb has been dropped - go get it
	bool NoticeLooseBomb( void ) const;							///< return true if we noticed the bomb on the ground or on radar
	bool CanSeeLooseBomb( void ) const;							///< return true if we directly see the loose bomb

	void DefuseBomb( void );

	void PickupHostage( CBaseEntity *entity );
	bool IsPickingupHostage( void ) const;

	bool IsDefusingBomb( void ) const;							///< returns true if bot is currently defusing the bomb
	bool CanSeePlantedBomb( void ) const;						///< return true if we directly see the planted bomb

	void EscapeFromBomb( void );
	bool IsEscapingFromBomb( void ) const;						///< return true if we are escaping from the bomb

	void EscapeFromFlames( void );
	bool IsEscapingFromFlames( void ) const;					///< return true if we are escaping from a field of flames

	void RescueHostages( void );								///< begin process of rescuing hostages

	void UseEntity( CBaseEntity *entity );						///< use the entity

	void OpenDoor( CBaseEntity *door );							///< open the door (assumes we are right in front of it)
	bool IsOpeningDoor( void ) const;							///< return true if we are in the process of opening a door

	void Buy( void );											///< enter the buy state
	bool IsBuying( void ) const;

	void Panic( void );											///< look around in panic
	bool IsPanicking( void ) const;								///< return true if bot is panicked
	void StopPanicking( void );									///< end our panic
	void UpdatePanicLookAround( void );							///< do panic behavior

	void TryToJoinTeam( int team );								///< try to join the given team

	void Follow( CCSPlayer *player );							///< begin following given Player
	void ContinueFollowing( void );								///< continue following our leader after finishing what we were doing
	void StopFollowing( void );									///< stop following
	bool IsFollowing( void ) const;								///< return true if we are following someone (not necessarily in the follow state)
	CCSPlayer *GetFollowLeader( void ) const;					///< return the leader we are following
	float GetFollowDuration( void ) const;						///< return how long we've been following our leader
	bool CanAutoFollow( void ) const;							///< return true if we can auto-follow

	bool IsNotMoving( float minDuration = 0.0f ) const;			///< return true if we are currently standing still and have been for minDuration

	void AimAtEnemy( void );									///< point our weapon towards our enemy
	void StopAiming( void );									///< stop aiming at enemy
	bool IsAimingAtEnemy( void ) const;							///< returns true if we are trying to aim at an enemy

	float GetStateTimestamp( void ) const;						///< get time current state was entered

	bool IsDoingScenario( void ) const;							///< return true if we will do scenario-related tasks

	//- scenario / gamestate -----------------------------------------------------------------------------------------
	CSGameState *GetGameState( void );							///< return an interface to this bot's gamestate
	const CSGameState *GetGameState( void ) const;				///< return an interface to this bot's gamestate

	bool IsAtBombsite( void );									///< return true if we are in a bomb planting zone
	bool GuardRandomZone( float range = 500.0f );				///< pick a random zone and hide near it

	bool IsBusy( void ) const;									///< return true if we are busy doing something important

	//- high-level tasks ---------------------------------------------------------------------------------------------
	enum TaskType
	{
		SEEK_AND_DESTROY,
		PLANT_BOMB,
		FIND_TICKING_BOMB,
		DEFUSE_BOMB,
		GUARD_TICKING_BOMB,
		GUARD_BOMB_DEFUSER,
		GUARD_LOOSE_BOMB,
		GUARD_BOMB_ZONE,
		GUARD_INITIAL_ENCOUNTER,
		ESCAPE_FROM_BOMB,
		HOLD_POSITION,
		FOLLOW,
		VIP_ESCAPE,
		GUARD_VIP_ESCAPE_ZONE,
		COLLECT_HOSTAGES,
		RESCUE_HOSTAGES,
		GUARD_HOSTAGES,
		GUARD_HOSTAGE_RESCUE_ZONE,
		MOVE_TO_LAST_KNOWN_ENEMY_POSITION,
		MOVE_TO_SNIPER_SPOT,
		SNIPING,
		ESCAPE_FROM_FLAMES,

		NUM_TASKS
	};
	void SetTask( TaskType task, CBaseEntity *entity = nullptr );	///< set our current "task"
	TaskType GetTask( void ) const;
	CBaseEntity *GetTaskEntity( void );
	const char *GetTaskName( void ) const;						///< return string describing current task

	// You probably never want to call these on CS bots. 
	CBaseEntity		*GetEnemy_Impl( void )  { contract_assert( 0 ); return nullptr; }
	CBaseEntity		*GetEnemy_Impl( void ) const  { contract_assert( 0 ); return nullptr; }

	//- behavior modifiers ------------------------------------------------------------------------------------------
	enum DispositionType
	{
		ENGAGE_AND_INVESTIGATE,								///< engage enemies on sight and investigate enemy noises
		OPPORTUNITY_FIRE,									///< engage enemies on sight, but only look towards enemy noises, dont investigate
		SELF_DEFENSE,										///< only engage if fired on, or very close to enemy
		IGNORE_ENEMIES,										///< ignore all enemies - useful for ducking around corners, running away, etc

		NUM_DISPOSITIONS
	};
	void SetDisposition( DispositionType disposition );		///< define how we react to enemies
	DispositionType GetDisposition( void ) const;
	const char *GetDispositionName( void ) const;			///< return string describing current disposition

	void IgnoreEnemies( float duration );					///< ignore enemies for a short duration

	enum MoraleType
	{
		TERRIBLE = -3,
		BAD = -2,
		NEGATIVE = -1,
		NEUTRAL = 0,
		POSITIVE = 1,
		GOOD = 2,
		EXCELLENT = 3,
	};
	MoraleType GetMorale( void ) const;
	const char *GetMoraleName( void ) const;				///< return string describing current morale
	void IncreaseMorale( void );
	void DecreaseMorale( void );

	void Surprise( float duration );						///< become "surprised" - can't attack
	bool IsSurprised( void ) const;							///< return true if we are "surprised"

	//- listening for noises ----------------------------------------------------------------------------------------
	bool IsNoiseHeard( void ) const;							///< return true if we have heard a noise
	bool HeardInterestingNoise( void );							///< return true if we heard an enemy noise worth checking in to

	float GetNoiseInvestigateChance( void ) const;

	void InvestigateNoise( void );								///< investigate recent enemy noise
	bool IsInvestigatingNoise( void ) const;					///< return true if we are investigating a noise
	const Vector *GetNoisePosition( void ) const;				///< return position of last heard noise, or NULL if none heard
	CNavArea *GetNoiseArea( void ) const;						///< return area where noise was heard
	void ForgetNoise( void );									///< clear the last heard noise
	bool CanSeeNoisePosition( void ) const;						///< return true if we directly see where we think the noise came from
	float GetNoiseRange( void ) const;							///< return approximate distance to last noise heard

	bool CanHearNearbyEnemyGunfire( float range = -1.0f ) const;///< return true if we hear nearby threatening enemy gunfire within given range (-1 == infinite)
	PriorityType GetNoisePriority( void ) const;				///< return priority of last heard noise

	//- radio and chatter--------------------------------------------------------------------------------------------
	void SendRadioMessage( RadioType event );					///< send a radio message
	void SpeakAudio( const char *voiceFilename, float duration, int pitch );	///< send voice chatter
	bool SpeakAudioResponseRules( const char *pConcept, AI_CriteriaSet *criteria, float duration );	///< send voice chatter through response rules system
	BotChatterInterface *GetChatter( void );					///< return an interface to this bot's chatter system
	bool RespondToHelpRequest( CCSPlayer *player, Place place, float maxRange = -1.0f );	///< decide if we should move to help the player, return true if we will
	bool IsUsingVoice() const;									///< new-style "voice" chatter gets voice feedback

	//- enemies ------------------------------------------------------------------------------------------------------
	// BOTPORT: GetEnemy() collides with GetEnemy() in CBaseEntity - need to use different nomenclature
	void SetBotEnemy( CCSPlayer *enemy );						///< set given player as our current enemy
	CCSPlayer *GetBotEnemy( void ) const;
	int GetNearbyEnemyCount( void ) const;						///< return max number of nearby enemies we've seen recently
	unsigned int GetEnemyPlace( void ) const;					///< return location where we see the majority of our enemies
	bool CanSeeBomber( void ) const;							///< return true if we can see the bomb carrier
	CCSPlayer *GetBomber( void ) const;

	int GetNearbyFriendCount( void ) const;						///< return number of nearby teammates
	CCSPlayer *GetClosestVisibleFriend( void ) const;			///< return the closest friend that we can see
	CCSPlayer *GetClosestVisibleHumanFriend( void ) const;		///< return the closest human friend that we can see

	bool IsOutnumbered( void ) const;							///< return true if we are outnumbered by enemies
	int OutnumberedCount( void ) const;							///< return number of enemies we are outnumbered by

	CCSPlayer *GetImportantEnemy( bool checkVisibility = false ) const;	///< return the closest "important" enemy for the given scenario (bomb carrier, VIP, hostage escorter)

	void UpdateReactionQueue( void );							///< update our reaction time queue
	CCSPlayer *GetRecognizedEnemy( void );						///< return the most dangerous threat we are "conscious" of
	bool IsRecognizedEnemyReloading( void );					///< return true if the enemy we are "conscious" of is reloading
	bool IsRecognizedEnemyProtectedByShield( void );			///< return true if the enemy we are "conscious" of is hiding behind a shield
	float GetRangeToNearestRecognizedEnemy( void );				///< return distance to closest enemy we are "conscious" of

	CCSPlayer *GetAttacker( void ) const;						///< return last enemy that hurt us
	float GetTimeSinceAttacked( void ) const;					///< return duration since we were last injured by an attacker
	float GetFirstSawEnemyTimestamp( void ) const;				///< time since we saw any enemies
	float GetLastSawEnemyTimestamp( void ) const;
	float GetTimeSinceLastSawEnemy( void ) const;
	float GetTimeSinceAcquiredCurrentEnemy( void ) const;
	float GetTimeSinceBurnedByFlames( void ) const;
	bool HasNotSeenEnemyForLongTime( void ) const;				///< return true if we haven't seen an enemy for "a long time"
	const Vector &GetLastKnownEnemyPosition( void ) const;
	bool IsEnemyVisible( void ) const;							///< is our current enemy visible
	float GetEnemyDeathTimestamp( void ) const;
	bool IsFriendInLineOfFire( void );							///< return true if a friend is in our weapon's way
	bool IsAwareOfEnemyDeath( void ) const;						///< return true if we *noticed* that our enemy died
	int GetLastVictimID( void ) const;							///< return the ID (entindex) of the last victim we killed, or zero

	bool CanSeeSniper( void ) const;							///< return true if we can see an enemy sniper
	bool HasSeenSniperRecently( void ) const;					///< return true if we have seen a sniper recently

	float GetTravelDistanceToPlayer( CCSPlayer *player ) const;	///< return shortest path travel distance to this player	
	bool DidPlayerJustFireWeapon( const CCSPlayer *player ) const;	///< return true if the given player just fired their weapon

	//- navigation --------------------------------------------------------------------------------------------------
	bool HasPath( void ) const;
	void DestroyPath( void );

	float GetFeetZ( void ) const;								///< return Z of bottom of feet

	enum PathResult
	{
		PROGRESSING,		///< we are moving along the path
		END_OF_PATH,		///< we reached the end of the path
		PATH_FAILURE		///< we failed to reach the end of the path
	};
	PathResult UpdatePathMovement( bool allowSpeedChange = true );	///< move along our computed path - if allowSpeedChange is true, bot will walk when near goal to ensure accuracy

	//bool AStarSearch( CNavArea *startArea, CNavArea *goalArea );	///< find shortest path from startArea to goalArea - don't actually buid the path
	bool ComputePath( const Vector &goal, RouteType route = SAFEST_ROUTE );	///< compute path to goal position
	bool StayOnNavMesh( void );
	const Vector &GetPathEndpoint( void ) const;					///< return final position of our current path
	float GetPathDistanceRemaining( void ) const;					///< return estimated distance left to travel along path
	void ResetStuckMonitor( void );
	bool IsAreaVisible( const CNavArea *area ) const;				///< is any portion of the area visible to this bot
	const Vector &GetPathPosition( int index ) const;
	bool GetSimpleGroundHeightWithFloor( const Vector &pos, float *height, Vector *normal = nullptr );	///< find "simple" ground height, treating current nav area as part of the floor
	void BreakablesCheck( void );
	void DoorCheck( void );											///< Check for any doors along our path that need opening

	virtual void PushawayTouch( CBaseEntity *pOther );

	Place GetPlace( void ) const;									///< get our current radio chatter place

	bool IsUsingLadder( void ) const;								///< returns true if we are in the process of negotiating a ladder
	void GetOffLadder( void );										///< immediately jump off of our ladder, if we're on one

	void SetGoalEntity( CBaseEntity *entity );
	CBaseEntity *GetGoalEntity( void );

	bool IsNearJump( void ) const;									///< return true if nearing a jump in the path
	float GetApproximateFallDamage( float height ) const;			///< return how much damage will will take from the given fall height

	void ForceRun( float duration );								///< force the bot to run if it moves for the given duration
	virtual bool IsRunning( void ) const;

	void Wait( float duration );									///< wait where we are for the given duration
	bool IsWaiting( void ) const;									///< return true if we are waiting
	void StopWaiting( void );										///< stop waiting

	void Wiggle( void );											///< random movement, for getting un-stuck

	bool IsFriendInTheWay( const Vector &goalPos );					///< return true if a friend is between us and the given position
	void FeelerReflexAdjustment( Vector *goalPosition );			///< do reflex avoidance movements if our "feelers" are touched

	bool HasVisitedEnemySpawn( void ) const;						///< return true if we have visited enemy spawn at least once
	bool IsAtEnemySpawn( void ) const;								///< return true if we are at the/an enemy spawn right now

	//- looking around ----------------------------------------------------------------------------------------------

	// BOTPORT: EVIL VILE HACK - why is EyePosition() not const?!?!?
	const Vector &EyePositionConst( void ) const;
	
	void SetLookAngles( float yaw, float pitch );					///< set our desired look angles
	void UpdateLookAngles( void );									///< move actual view angles towards desired ones
	void UpdateLookAround( bool updateNow = false );				///< update "looking around" mechanism
	void InhibitLookAround( float duration );						///< block all "look at" and "looking around" behavior for given duration - just look ahead

	/// @todo Clean up notion of "forward angle" and "look ahead angle"
	void SetForwardAngle( float angle );							///< define our forward facing
	void SetLookAheadAngle( float angle );							///< define default look ahead angle

	/// look at the given point in space for the given duration (-1 means forever)
	void SetLookAt( const char *desc, const Vector &pos, PriorityType pri, float duration = -1.0f, bool clearIfClose = false, float angleTolerance = 5.0f, bool attack = false );
	void ClearLookAt( void );										///< stop looking at a point in space and just look ahead
	bool IsLookingAtSpot( PriorityType pri = PRIORITY_LOW ) const;	///< return true if we are looking at spot with equal or higher priority
	bool IsViewMoving( float angleVelThreshold = 1.0f ) const;		///< returns true if bot's view angles are rotating (not still)
	bool HasViewBeenSteady( float duration ) const;					///< how long has our view been "steady" (ie: not moving) for given duration

	bool HasLookAtTarget( void ) const;								///< return true if we are in the process of looking at a target

	enum VisiblePartType
	{
		NONE		= 0x00,
		GUT			= 0x01,
		HEAD		= 0x02,
		LEFT_SIDE	= 0x04,			///< the left side of the object from our point of view (not their left side)
		RIGHT_SIDE	= 0x08,			///< the right side of the object from our point of view (not their right side)
		FEET		= 0x10
	};

	bool IsVisible( const Vector &pos, bool testFOV = false, const CBaseEntity *ignore = nullptr ) const;	///< return true if we can see the point
	bool IsVisible( CCSPlayer *player, bool testFOV = false, unsigned char *visParts = nullptr ) const;	///< return true if we can see any part of the player

	bool IsBeyondBotMaxVisionDistance( const Vector &vecTargetPosition ) const;

	bool IsNoticable( const CCSPlayer *player, unsigned char visibleParts ) const;	///< return true if we "notice" given player 

	bool IsEnemyPartVisible( VisiblePartType part ) const;			///< if enemy is visible, return the part we see for our current enemy
	const Vector &GetPartPosition( CCSPlayer *player, VisiblePartType part ) const;	///< return world space position of given part on player

	float ComputeWeaponSightRange( void );							///< return line-of-sight distance to obstacle along weapon fire ray

	bool IsAnyVisibleEnemyLookingAtMe( bool testFOV = false ) const;///< return true if any enemy I have LOS to is looking directly at me

	bool IsSignificantlyCloser( const CCSPlayer *testPlayer, const CCSPlayer *referencePlayer ) const;	///< return true if testPlayer is significantly closer than referencePlayer

	//- approach points ---------------------------------------------------------------------------------------------
	void ComputeApproachPoints( void );								///< determine the set of "approach points" representing where the enemy can enter this region
	void UpdateApproachPoints( void );								///< recompute the approach point set if we have moved far enough to invalidate the current ones
	void ClearApproachPoints( void );
	void DrawApproachPoints( void ) const;							///< for debugging
	float GetHidingSpotCheckTimestamp( HidingSpot *spot ) const;	///< return time when given spot was last checked
	void SetHidingSpotCheckTimestamp( HidingSpot *spot );			///< set the timestamp of the given spot to now

	const CNavArea *GetInitialEncounterArea( void ) const;			///< return area where we think we will first meet the enemy
	void SetInitialEncounterArea( const CNavArea *area );

	//- weapon query and equip --------------------------------------------------------------------------------------
	void EquipBestWeapon( bool mustEquip = false );					///< equip the best weapon we are carrying that has ammo
	void EquipPistol( void );										///< equip our pistol
	void EquipKnife( void );										///< equip the knife

	bool EquipGrenade( bool noSmoke = false );						///< equip a grenade, return false if we cant

	bool IsUsingKnife( void ) const;								///< returns true if we have knife equipped
	bool IsUsingPistol( void ) const;								///< returns true if we have pistol equipped
	bool IsUsingGrenade( void ) const;								///< returns true if we have grenade equipped
	bool IsUsingSniperRifle( void ) const;							///< returns true if using a "sniper" rifle
	bool IsUsing( CSWeaponID weapon ) const;						///< returns true if using the specific weapon
	bool IsSniper( void ) const;									///< return true if we have a sniper rifle in our inventory
	bool IsSniping( void ) const;									///< return true if we are actively sniping (moving to sniper spot or settled in)
	bool IsUsingShotgun( void ) const;								///< returns true if using a shotgun
	bool IsUsingMachinegun( void ) const;							///< returns true if using the big 'ol machinegun
	void ThrowGrenade( const Vector &target );						///< begin the process of throwing the grenade
	bool IsThrowingGrenade( void ) const;							///< return true if we are in the process of throwing a grenade
	bool HasGrenade( void ) const;									///< return true if we have a grenade in our inventory
	void AvoidEnemyGrenades( void );								///< react to enemy grenades we see
	bool IsAvoidingGrenade( void ) const;							///< return true if we are in the act of avoiding a grenade
	bool DoesActiveWeaponHaveRemoveableSilencer( void ) const;		///< returns true if we are using a weapon with a removable silencer
	bool CanActiveWeaponFire( void ) const;							///< returns true if our current weapon can attack
	CWeaponCSBase *GetActiveCSWeapon( void ) const;					///< get our current Counter-Strike weapon

	void GiveWeapon( const char *weaponAlias );						///< Debug command to give a named weapon

	void PrimaryAttack( void );								///< presses the fire button, unless we're holding a pistol that can't fire yet (so we can just always call PrimaryAttack())

	enum ZoomType { NO_ZOOM, LOW_ZOOM, HIGH_ZOOM };
	ZoomType GetZoomLevel( void );									///< return the current zoom level of our weapon

	bool AdjustZoom( float range );									///< change our zoom level to be appropriate for the given range
	bool IsWaitingForZoom( void ) const;							///< return true if we are reacquiring after our zoom

	bool IsPrimaryWeaponEmpty( void ) const;						///< return true if primary weapon doesn't exist or is totally out of ammo
	bool IsPistolEmpty( void ) const;								///< return true if pistol doesn't exist or is totally out of ammo

	int GetHostageEscortCount( void ) const;						///< return the number of hostages following me
	void IncreaseHostageEscortCount( void );
	float GetRangeToFarthestEscortedHostage( void ) const;			///< return euclidean distance to farthest escorted hostage
	void ResetWaitForHostagePatience( void );

	//------------------------------------------------------------------------------------
	// Event hooks
	//

	/// invoked when injured by something (EXTEND) - returns the amount of damage inflicted
	int OnTakeDamage_Impl( const CTakeDamageInfo &info );

	/// invoked when killed (EXTEND)
	void Event_Killed_Impl( const CTakeDamageInfo &info );

	virtual bool BumpWeapon( CBaseCombatWeapon *pWeapon );		///< invoked when in contact with a CWeaponBox

	/// invoked when event occurs in the game (some events have NULL entity)
	void OnPlayerFootstep( IGameEvent *event );
	void OnPlayerRadio( IGameEvent *event );
	void OnPlayerDeath( IGameEvent *event );
	void OnPlayerFallDamage( IGameEvent *event );

	void OnBombPickedUp( IGameEvent *event );
	void OnBombPlanted( IGameEvent *event );
	void OnBombBeep( IGameEvent *event );
	void OnBombDefuseBegin( IGameEvent *event );
	void OnBombDefused( IGameEvent *event );
	void OnBombDefuseAbort( IGameEvent *event );
	void OnBombExploded( IGameEvent *event );

	void OnRoundEnd( IGameEvent *event );
	void OnRoundStart( IGameEvent *event );

	void OnDoorMoving( IGameEvent *event );

	void OnBreakProp( IGameEvent *event );
	void OnBreakBreakable( IGameEvent *event );

	void OnHostageFollows( IGameEvent *event );
	void OnHostageRescuedAll( IGameEvent *event );

	void OnWeaponFire( IGameEvent *event );
	void OnWeaponFireOnEmpty( IGameEvent *event );
	void OnWeaponReload( IGameEvent *event );
	void OnWeaponZoom( IGameEvent *event );

	void OnBulletImpact( IGameEvent *event );

	void OnHEGrenadeDetonate( IGameEvent *event );
	void OnFlashbangDetonate( IGameEvent *event );
	void OnSmokeGrenadeDetonate( IGameEvent *event );
	void OnMolotovDetonate( IGameEvent *event );
	void OnDecoyDetonate( IGameEvent *event );
	void OnDecoyFiring( IGameEvent *event );
	void OnGrenadeBounce( IGameEvent *event );

	void OnNavBlocked( IGameEvent *event );

	void OnEnteredNavArea( CNavArea *newArea );						///< invoked when bot enters a nav area

	void OnAudibleEvent( IGameEvent *event, CBasePlayer *player, float range, PriorityType priority, bool isHostile, bool isFootstep = false, const Vector *actualOrigin = nullptr );	///< Checks if the bot can hear the event

	void SetLastCoopSpawnPoint( SpawnPointCoopEnemy *spawn );
	SpawnPointCoopEnemy *GetLastCoopSpawnPoint( void ) const;
	CHandle< SpawnPointCoopEnemy > m_lastCoopSpawnPoint;				/// the last spawn point entity that we spawned from

protected:

	float SlowNoise( float fTau ) const;

	BotProfile *m_pLocalProfile;										///< local profile of the bot - can change over time due to dynamic bot difficulty changes

private:
	friend class CCSBotManager;

	/// @todo Get rid of these
	friend class AttackState;
	friend class BuyState;

	// BOTPORT: Remove this vile hack
	Vector m_eyePosition;

	void ResetValues( void );										///< reset internal data to initial state
	void BotDeathThink( void );

	char m_name[64];												///< copied from STRING(pev->netname) for debugging
	void DebugDisplay( void ) const;								///< render bot debug info

	//- behavior properties ------------------------------------------------------------------------------------------
	float m_combatRange;											///< desired distance between us and them during gunplay
	mutable bool m_isRogue;											///< if true, the bot is a "rogue" and listens to no-one
	mutable CountdownTimer m_rogueTimer;
	MoraleType m_morale;											///< our current morale, based on our win/loss history
	bool m_diedLastRound;											///< true if we died last round
	float m_safeTime;												///< duration at the beginning of the round where we feel "safe"
	bool m_wasSafe;													///< true if we were in the safe time last update
	void AdjustSafeTime( void );									///< called when enemy seen to adjust safe time for this round
	NavRelativeDirType m_blindMoveDir;								///< which way to move when we're blind
	bool m_blindFire;												///< if true, fire weapon while blinded
	CountdownTimer m_surpriseTimer;									///< when we were surprised

	bool m_isFollowing;												///< true if we are following someone
	CHandle< CCSPlayer > m_leader;									///< the ID of who we are following
	float m_followTimestamp;										///< when we started following
	float m_allowAutoFollowTime;									///< time when we can auto follow

	CountdownTimer m_hurryTimer;									///< if valid, bot is in a hurry
	CountdownTimer m_alertTimer;									///< if valid, bot is alert
	CountdownTimer m_sneakTimer;									///< if valid, bot is sneaking
	CountdownTimer m_panicTimer;									///< if valid, bot is panicking

	// instances of each possible behavior state, to avoid dynamic memory allocation during runtime
	IdleState				m_idleState;
	HuntState				m_huntState;
	AttackState				m_attackState;
	InvestigateNoiseState	m_investigateNoiseState;
	BuyState				m_buyState;
	MoveToState				m_moveToState;
	FetchBombState			m_fetchBombState;
	PlantBombState			m_plantBombState;
	DefuseBombState			m_defuseBombState;
	PickupHostageState		m_pickupHostageState;
	HideState				m_hideState;
	EscapeFromBombState		m_escapeFromBombState;
	FollowState				m_followState;
	UseEntityState			m_useEntityState;
	OpenDoorState			m_openDoorState;
	EscapeFromFlamesState	m_escapeFromFlamesState;

	/// @todo Allow multiple simultaneous state machines (look around, etc)	
	void SetState( BotState *state );								///< set the current behavior state
	BotState *m_state;												///< current behavior state
	float m_stateTimestamp;											///< time state was entered
	bool m_isAttacking;												///< if true, special Attack state is overriding the state machine
	bool m_isOpeningDoor;											///< if true, special OpenDoor state is overriding the state machine

	TaskType m_task;												///< our current task
	EHANDLE m_taskEntity;											///< an entity used for our task

	//- navigation ---------------------------------------------------------------------------------------------------
	Vector m_goalPosition;
	EHANDLE m_goalEntity;
	void MoveTowardsPosition( const Vector &pos );					///< move towards position, independant of view angle
	void MoveAwayFromPosition( const Vector &pos );					///< move away from position, independant of view angle
	void StrafeAwayFromPosition( const Vector &pos );				///< strafe (sidestep) away from position, independant of view angle
	void StuckCheck( void );										///< check if we have become stuck
	CNavArea *m_currentArea;										///< the nav area we are standing on
	CNavArea *m_lastKnownArea;										///< the last area we were in
	EHANDLE m_avoid;												///< higher priority player we need to make way for
	float m_avoidTimestamp;
	bool m_isStopping;												///< true if we're trying to stop because we entered a 'stop' nav area
	bool m_hasVisitedEnemySpawn;									///< true if we have been at the enemy spawn
	IntervalTimer m_stillTimer;										///< how long we have been not moving
	void CoopUpdateChecks( void );									///< checks needed for coop

	//- path navigation data ----------------------------------------------------------------------------------------
	enum { MAX_PATH_LENGTH = 256 };
	struct ConnectInfo
	{
		CNavArea *area;												///< the area along the path
		NavTraverseType how;										///< how to enter this area from the previous one
		Vector pos;													///< our movement goal position at this point in the path
		const CNavLadder *ladder;									///< if "how" refers to a ladder, this is it
	}
	m_path[ MAX_PATH_LENGTH ];
	int m_pathLength;
	int m_pathIndex;												///< index of next area on path
	float m_areaEnteredTimestamp;
	void BuildTrivialPath( const Vector &goal );					///< build trivial path to goal, assuming we are already in the same area

	CountdownTimer m_repathTimer;									///< must have elapsed before bot can pathfind again

	bool ComputePathPositions( void );								///< determine actual path positions bot will move between along the path
	void SetupLadderMovement( void );
	void SetPathIndex( int index );									///< set the current index along the path
	void DrawPath( void );
	int FindOurPositionOnPath( Vector *close, bool local = false ) const;	///< compute the closest point to our current position on our path
	int FindPathPoint( float aheadRange, Vector *point, int *prevIndex = nullptr );	///< compute a point a fixed distance ahead along our path.
	bool FindClosestPointOnPath( const Vector &pos, int startIndex, int endIndex, Vector *close ) const;	///< compute closest point on path to given point
	bool IsStraightLinePathWalkable( const Vector &goal ) const;	///< test for un-jumpable height change, or unrecoverable fall
	void ComputeLadderAngles( float *yaw, float *pitch );			///< computes ideal yaw/pitch for traversing the current ladder on our path

	mutable CountdownTimer m_avoidFriendTimer;						///< used to throttle how often we check for friends in our path
	mutable bool m_isFriendInTheWay;								///< true if a friend is blocking our path
	CountdownTimer m_politeTimer;									///< we'll wait for friend to move until this runs out
	bool m_isWaitingBehindFriend;									///< true if we are waiting for a friend to move

	bool DiscontinuityJump( float ground, bool onlyJumpDown = false, bool mustJump = false ); ///< check if we need to jump due to height change

	enum LadderNavState
	{
		APPROACH_ASCENDING_LADDER,									///< prepare to scale a ladder
		APPROACH_DESCENDING_LADDER,									///< prepare to go down ladder 
		FACE_ASCENDING_LADDER,
		FACE_DESCENDING_LADDER,
		MOUNT_ASCENDING_LADDER,										///< move toward ladder until "on" it
		MOUNT_DESCENDING_LADDER,									///< move toward ladder until "on" it
		ASCEND_LADDER,												///< go up the ladder
		DESCEND_LADDER,												///< go down the ladder
		DISMOUNT_ASCENDING_LADDER,									///< get off of the ladder
		DISMOUNT_DESCENDING_LADDER,									///< get off of the ladder
		MOVE_TO_DESTINATION,										///< dismount ladder and move to destination area
	}
	m_pathLadderState;
	bool m_pathLadderFaceIn;										///< if true, face towards ladder, otherwise face away
	const CNavLadder *m_pathLadder;									///< the ladder we need to use to reach the next area
	bool UpdateLadderMovement( void );								///< called by UpdatePathMovement()
	NavRelativeDirType m_pathLadderDismountDir;						///< which way to dismount
	float m_pathLadderDismountTimestamp;							///< time when dismount started
	float m_pathLadderEnd;											///< if ascending, z of top, if descending z of bottom
	void ComputeLadderEndpoint( bool ascending );
	float m_pathLadderTimestamp;									///< time when we started using ladder - for timeout check

	CountdownTimer m_mustRunTimer;									///< if nonzero, bot cannot walk
	CountdownTimer m_waitTimer;										///< if nonzero, we are waiting where we are

	void UpdateTravelDistanceToAllPlayers( void );					///< periodically compute shortest path distance to each player
	CountdownTimer m_updateTravelDistanceTimer;						///< for throttling travel distance computations
	float m_playerTravelDistance[ MAX_PLAYERS ];					///< current distance from this bot to each player
	unsigned char m_travelDistancePhase;							///< a counter for optimizing when to compute travel distance

	//- game scenario mechanisms -------------------------------------------------------------------------------------
	CSGameState m_gameState;										///< our current knowledge about the state of the scenario

	byte m_hostageEscortCount;										///< the number of hostages we're currently escorting
	void UpdateHostageEscortCount( void );							///< periodic check of hostage count in case we lost some
	float m_hostageEscortCountTimestamp;

	int m_desiredTeam;												///< the team we want to be on
	bool m_hasJoined;												///< true if bot has actually joined the game

	bool m_isWaitingForHostage;
	CountdownTimer m_inhibitWaitingForHostageTimer;					///< if active, inhibits us waiting for lagging hostages
	CountdownTimer m_waitForHostageTimer;							///< stops us waiting too long

	//- listening mechanism ------------------------------------------------------------------------------------------
	Vector m_noisePosition;											///< position we last heard non-friendly noise
	float m_noiseTravelDistance;									///< the travel distance to the noise
	float m_noiseTimestamp;											///< when we heard it (can get zeroed)
	CNavArea *m_noiseArea;											///< the nav area containing the noise
	PriorityType m_noisePriority;									///< priority of currently heard noise
	bool UpdateLookAtNoise( void );									///< return true if we decided to look towards the most recent noise source
	CountdownTimer m_noiseBendTimer;								///< for throttling how often we bend our line of sight to the noise location
	Vector m_bentNoisePosition;										///< the last computed bent line of sight
	bool m_bendNoisePositionValid;

	//- "looking around" mechanism -----------------------------------------------------------------------------------
	float m_lookAroundStateTimestamp;								///< time of next state change
	float m_lookAheadAngle;											///< our desired forward look angle
	float m_forwardAngle;											///< our current forward facing direction
	float m_inhibitLookAroundTimestamp;								///< time when we can look around again

	enum LookAtSpotState
	{
		NOT_LOOKING_AT_SPOT,			///< not currently looking at a point in space
		LOOK_TOWARDS_SPOT,				///< in the process of aiming at m_lookAtSpot
		LOOK_AT_SPOT,					///< looking at m_lookAtSpot
		NUM_LOOK_AT_SPOT_STATES
	}
	m_lookAtSpotState;
	Vector m_lookAtSpot;											///< the spot we're currently looking at
	PriorityType m_lookAtSpotPriority;
	float m_lookAtSpotDuration;										///< how long we need to look at the spot
	float m_lookAtSpotTimestamp;									///< when we actually began looking at the spot
	float m_lookAtSpotAngleTolerance;								///< how exactly we must look at the spot
	bool m_lookAtSpotClearIfClose;									///< if true, the look at spot is cleared if it gets close to us
	bool m_lookAtSpotAttack;										///< if true, the look at spot should be attacked
	const char *m_lookAtDesc;										///< for debugging
	void UpdateLookAt( void );
	void UpdatePeripheralVision();									///< update enounter spot timestamps, etc
	float m_peripheralTimestamp;

	enum { MAX_APPROACH_POINTS = 16 };
	struct ApproachPoint
	{
		Vector m_pos;
		CNavArea *m_area;
	};

	ApproachPoint m_approachPoint[ MAX_APPROACH_POINTS ];
	unsigned char m_approachPointCount;
	Vector m_approachPointViewPosition;								///< the position used when computing current approachPoint set

	CBaseEntity * FindEntitiesOnPath( float distance, CPushAwayEnumerator *enumerator, bool checkStuck );

	IntervalTimer m_viewSteadyTimer;								///< how long has our view been "steady" (ie: not moving)

	bool BendLineOfSight( const Vector &eye, const Vector &target, Vector *bend, float angleLimit = 135.0f ) const;		///< "bend" our line of sight until we can see the target point. Return bend point, false if cant bend.
	bool FindApproachPointNearestPath( Vector *pos );				///< find the approach point that is nearest to our current path, ahead of us
	bool FindGrenadeTossPathTarget( Vector *pos );					///< find spot to throw grenade ahead of us and "around the corner" along our path
	enum GrenadeTossState
	{
		NOT_THROWING,				///< not yet throwing
		START_THROW,				///< lining up throw
		THROW_LINED_UP,				///< pause for a moment when on-line
		FINISH_THROW,				///< throwing
	};
	GrenadeTossState m_grenadeTossState;
	CountdownTimer m_tossGrenadeTimer;								///< timeout timer for grenade tossing
	const CNavArea *m_initialEncounterArea;							///< area where we think we will initially encounter the enemy
	void LookForGrenadeTargets( void );								///< look for grenade throw targets and throw our grenade at them
	void UpdateGrenadeThrow( void );								///< process grenade throwing
	CountdownTimer m_isAvoidingGrenade;								///< if nonzero we are in the act of avoiding a grenade

	SpotEncounter *m_spotEncounter;									///< the spots we will encounter as we move thru our current area
	float m_spotCheckTimestamp;										///< when to check next encounter spot

	/// @todo Add timestamp for each possible client to hiding spots
	enum { MAX_CHECKED_SPOTS = 64 };
	struct HidingSpotCheckInfo
	{
		HidingSpot *spot;
		float timestamp;
	}
	m_checkedHidingSpot[ MAX_CHECKED_SPOTS ];
	int m_checkedHidingSpotCount;

	//- view angle mechanism -----------------------------------------------------------------------------------------
	float m_lookPitch;												///< our desired look pitch angle
	float m_lookPitchVel;
	float m_lookYaw;												///< our desired look yaw angle
	float m_lookYawVel;

	//- aim angle mechanism -----------------------------------------------------------------------------------------
	void PickNewAimSpot();											///< set the current aim offset
	void UpdateAimPrediction( void );								///< wiggle aim based on aim error term
	Vector m_targetSpot;											///< the spot we currently wish to fire at
	Vector m_targetSpotVelocity;									///< the spot we currently wish to fire at
	Vector m_targetSpotPredicted;									///< the spot we currently wish to fire at
	QAngle m_aimError;
	QAngle m_aimGoal;
	float m_targetSpotTime;
	float m_aimFocus;												///< radius of aim focus
	float m_aimFocusInterval;										///< time interval of current offset adjustment (can be random)
	float m_aimFocusNextUpdate;										///< time of next offset adjustment

	struct PartInfo
	{
		Vector m_headPos;											///< current head position
		Vector m_gutPos;											///< current gut position
		Vector m_feetPos;											///< current feet position
		Vector m_leftSidePos;										///< current left side position
		Vector m_rightSidePos;										///< current right side position
		int m_validFrame;											///< frame of last computation (for lazy evaluation)
	};
	static PartInfo m_partInfo[ MAX_PLAYERS ];						///< part positions for each player
	void ComputePartPositions( CCSPlayer *player );					///< compute part positions from bone location

	//- attack state data --------------------------------------------------------------------------------------------
	DispositionType m_disposition;									///< how we will react to enemies
	CountdownTimer m_ignoreEnemiesTimer;							///< how long will we ignore enemies
	mutable CHandle< CCSPlayer > m_enemy;							///< our current enemy
	bool m_isEnemyVisible;											///< result of last visibility test on enemy
	unsigned char m_visibleEnemyParts;								///< which parts of the visible enemy do we see
	Vector m_lastEnemyPosition;										///< last place we saw the enemy
	float m_lastSawEnemyTimestamp;
	float m_firstSawEnemyTimestamp;
	float m_currentEnemyAcquireTimestamp;
	float m_enemyDeathTimestamp;									///< if m_enemy is dead, this is when he died
	float m_friendDeathTimestamp;									///< time since we saw a friend die
	bool m_isLastEnemyDead;											///< true if we killed or saw our last enemy die
	int m_nearbyEnemyCount;											///< max number of enemies we've seen recently
	unsigned int m_enemyPlace;										///< the location where we saw most of our enemies

	struct WatchInfo
	{
		float timestamp;											///< time we last saw this player, zero if never seen
		bool isEnemy;
	}
	m_watchInfo[ MAX_PLAYERS ];
	mutable CHandle< CCSPlayer > m_bomber;							///< points to bomber if we can see him

	int m_nearbyFriendCount;										///< number of nearby teammates
	mutable CHandle< CCSPlayer > m_closestVisibleFriend;			///< the closest friend we can see
	mutable CHandle< CCSPlayer > m_closestVisibleHumanFriend;		///< the closest human friend we can see

	IntervalTimer m_attentionInterval;								///< time between attention checks

	mutable CHandle< CCSPlayer > m_attacker;						///< last enemy that hurt us (may not be same as m_enemy)
	float m_attackedTimestamp;										///< when we were hurt by the m_attacker
	IntervalTimer m_burnedByFlamesTimer;

	int m_lastVictimID;												///< the entindex of the last victim we killed, or zero
	bool m_isAimingAtEnemy;											///< if true, we are trying to aim at our enemy
	bool m_isRapidFiring;											///< if true, RunUpkeep() will toggle our primary attack as fast as it can
	IntervalTimer m_equipTimer;										///< how long have we had our current weapon equipped
	CountdownTimer m_zoomTimer;										///< for delaying firing immediately after zoom
	bool DoEquip( CWeaponCSBase *gun );								///< equip the given item

	void ReloadCheck( void );										///< reload our weapon if we must
	void SilencerCheck( void );										///< use silencer

	float m_fireWeaponTimestamp;

	// sleeping
	bool m_bIsSleeping;

	bool m_isEnemySniperVisible;									///< do we see an enemy sniper right now
	CountdownTimer m_sawEnemySniperTimer;							///< tracking time since saw enemy sniper
	
	//- reaction time system -----------------------------------------------------------------------------------------
	enum { MAX_ENEMY_QUEUE = 20 };
	struct ReactionState
	{
		// NOTE: player position & orientation is not currently stored separately
		CHandle<CCSPlayer> player;
		bool isReloading;
		bool isProtectedByShield;
	}
	m_enemyQueue[ MAX_ENEMY_QUEUE ];								///< round-robin queue for simulating reaction times
	byte m_enemyQueueIndex;
	byte m_enemyQueueCount;
	byte m_enemyQueueAttendIndex;									///< index of the timeframe we are "conscious" of

	CCSPlayer *FindMostDangerousThreat( void );						///< return most dangerous threat in my field of view (feeds into reaction time queue)

	//- stuck detection ---------------------------------------------------------------------------------------------
	bool m_isStuck;
	float m_stuckTimestamp;											///< time when we got stuck
	Vector m_stuckSpot;												///< the location where we became stuck
	NavRelativeDirType m_wiggleDirection;
	CountdownTimer m_wiggleTimer;
	CountdownTimer m_stuckJumpTimer;								///< time for next jump when stuck
	float m_nextCleanupCheckTimestamp;

	enum { MAX_VEL_SAMPLES = 10 };	
	float m_avgVel[ MAX_VEL_SAMPLES ];
	int m_avgVelIndex;
	int m_avgVelCount;
	Vector m_lastOrigin;

	//- radio --------------------------------------------------------------------------------------------------------
	RadioType m_lastRadioCommand;									///< last radio command we recieved
	float m_lastRadioRecievedTimestamp;								///< time we recieved a radio message
	float m_lastRadioSentTimestamp;									///< time when we send a radio message
	CHandle< CCSPlayer > m_radioSubject;							///< who issued the radio message
	Vector m_radioPosition;											///< position referred to in radio message
	void RespondToRadioCommands( void );
	bool IsRadioCommand( RadioType event ) const;					///< returns true if the radio message is an order to do something

	/// new-style "voice" chatter gets voice feedback
	float m_voiceEndTimestamp;

	BotChatterInterface *m_pChatter;									///< chatter mechanism

	public: int ObjectCaps_Impl( void ) { return ( BaseClass::ObjectCaps_Impl() | FCAP_IMPULSE_USE ); }
	private: //allow +use

};

//
// Inlines
//

	

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if the given weapon is a sniper rifle
 */
inline bool IsSniperRifle( CWeaponCSBase *weapon )
{
	if (weapon == nullptr)
		return false;

	return weapon->IsKindOf(WEAPONTYPE_SNIPER_RIFLE);
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Functor used with NavAreaBuildPath()
 */
class PathCost
{
public:
	PathCost( CCSBot *bot, RouteType route = SAFEST_ROUTE );

	// HPE_TODO[pmf]: check that these new parameters are okay to be ignored
	float operator() ( CNavArea *area, CNavArea *fromArea, const CNavLadder *ladder, const CFuncElevator *elevator, float length )
	{
        float dangerFactor = m_dangerFactor;

		if (fromArea == nullptr)
		{
			if (m_route == FASTEST_ROUTE)
				return 0.0f;

			// first area in path, cost is just danger
			return dangerFactor * area->GetDanger( m_bot->GetTeamNumber() );
		}
		else if ((fromArea->GetAttributes() & NAV_MESH_JUMP) && (area->GetAttributes() & NAV_MESH_JUMP))
		{
			// cannot actually walk in jump areas - disallow moving from jump area to jump area
			return -1.0f;
		}
		if ( area->GetAttributes() & NAV_MESH_NO_HOSTAGES && m_bot->GetHostageEscortCount() )
		{
			// if we're leading hostages, don't try to go where they can't
			return -1.0f;
		}
		else
		{
			// compute distance from previous area to this area
			float dist;
			if (ladder)
			{
				// ladders are slow to use
				const float ladderPenalty = 1.0f; // 3.0f;
				dist = ladderPenalty * ladder->m_length;

				// if we are currently escorting hostages, avoid ladders (hostages are confused by them)
				//if (m_bot->GetHostageEscortCount())
				//	dist *= 100.0f;
			}
			else
			{
				dist = (area->GetCenter() - fromArea->GetCenter()).Length();
			}

			// compute distance travelled along path so far
			float cost = dist + fromArea->GetCostSoFar();

			// add cost of "jump down" pain unless we're jumping into water
			if (!area->IsUnderwater() && area->IsConnected( fromArea, NUM_DIRECTIONS ) == false)
			{
				// this is a "jump down" (one way drop) transition - estimate damage we will take to traverse it
				float fallDistance = -fromArea->ComputeGroundHeightChange( area );

				// if it's a drop-down ladder, estimate height from the bottom of the ladder to the lower area
				if ( ladder && ladder->m_bottom.z < fromArea->GetCenter().z && ladder->m_bottom.z > area->GetCenter().z )
				{
					fallDistance = ladder->m_bottom.z - area->GetCenter().z;
				}

				float fallDamage = m_bot->GetApproximateFallDamage( fallDistance );

				if (fallDamage > 0.0f)
				{
					// if the fall would kill us, don't use it
					const float deathFallMargin = 10.0f;
					if (fallDamage + deathFallMargin >= m_bot->GetHealth())
						return -1.0f;

					// if we need to get there in a hurry, ignore minor pain
					const float painTolerance = 15.0f * m_bot->GetProfile()->GetAggression() + 10.0f;
					if (m_route != FASTEST_ROUTE || fallDamage > painTolerance)
					{
						// cost is proportional to how much it hurts when we fall
						// 10 points - not a big deal, 50 points - ouch!
						cost += 100.0f * fallDamage * fallDamage;
					}
				}
			}

			// if this is a "crouch" or "walk" area, add penalty
			if (area->GetAttributes() & (NAV_MESH_CROUCH | NAV_MESH_WALK))
			{
				// these areas are very slow to move through
				float penalty = (m_route == FASTEST_ROUTE) ? 20.0f : 5.0f;

				// avoid crouch areas if we are rescuing hostages 
				if ((area->GetAttributes() & NAV_MESH_CROUCH) && m_bot->GetHostageEscortCount())
				{
					penalty *= 3.0f;
				}

				cost += penalty * dist;
			}

			// if this is a "jump" area, add penalty
			if (area->GetAttributes() & NAV_MESH_JUMP)
			{
				// jumping can slow you down
				//const float jumpPenalty = (m_route == FASTEST_ROUTE) ? 100.0f : 0.5f;
				const float jumpPenalty = 1.0f;
				cost += jumpPenalty * dist;
			}

			// if this is an area to avoid, add penalty
			if ( area->IsDamaging() )
			{
				const float damagingPenalty = 100.0f;
				cost += damagingPenalty * dist;
			}

			// if this is an area to avoid, add penalty
			if (area->GetAttributes() & NAV_MESH_AVOID)
			{
				const float avoidPenalty = 20.0f;
				cost += avoidPenalty * dist;
			}

			if (m_route == SAFEST_ROUTE)
			{
				// add in the danger of this path - danger is per unit length traveled
				cost += dist + ( dist * dangerFactor * area->GetDanger( m_bot->GetTeamNumber() ) );
			}

			// this term causes the same bot to choose different routes over time,
			// but keep the same route for a period in case of repaths
			int timeMod = (int)( gpGlobals->curtime / 10.0f ) + 1;
			
			int uniqueID = ((std::size_t)area) >> 7; // areas are 128-byte aligned, so shift address over
			// We just need a unique number approximately between 1 and 300, so take the mod 293 because it's prime
			unsigned int nRandomCost = ( unsigned int )( m_bot->entindex() * uniqueID * timeMod ) % 293;
			cost += 1.0f + (float)nRandomCost;

			if (!m_bot->IsAttacking())
			{
				// add in cost of teammates in the way

				// approximate density of teammates based on area
				float size = (area->GetSizeX() + area->GetSizeY())/2.0f;

				// degenerate check
				if (size >= 1.0f)
				{
					// cost is proportional to the density of teammates in this area
					const float costPerFriendPerUnit = 50000.0f;
					cost += costPerFriendPerUnit * (float)area->GetPlayerCount( m_bot->GetTeamNumber() ) / size;
				}
			}

			return cost;
		}
	}

private:
	CCSBot *	m_bot;
	RouteType	m_route;
	float		m_dangerFactor;
};

inline CCSBot *ToCSBot( CBaseEntity *pEntity )
{
	// CCSBot is the only bot class: every fake client is a CCSBot, except the GOTV/replay proxies (plain CCSPlayers)
	if ( !pEntity || !pEntity->IsPlayer() )
		return nullptr;

	CBasePlayer *pPlayer = static_cast<CBasePlayer*>( pEntity );
	if ( !pPlayer->IsBot() || pPlayer->IsHLTV() || pPlayer->IsReplay() )
		return nullptr;

	return static_cast<CCSBot*>( pPlayer );
}

//--------------------------------------------------------------------------------------------------------------
//
// Prototypes
//
extern int GetBotFollowCount( CCSPlayer *leader );
extern const Vector *FindNearbyRetreatSpot( CCSBot *me, float maxRange = 250.0f );
extern const HidingSpot *FindInitialEncounterSpot( CBaseEntity *me, const Vector &searchOrigin, float enemyArriveTime, float maxRange, bool isSniper );

class BotChatterInterface;

class CBaseEntity;
class CBasePlayer;
class CCSBot;

using namespace ::ks::reflect;

// Purpose: Bot radio chatter system

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

inline constexpr auto MAX_PLACES_PER_MAP = 64;

typedef unsigned int PlaceCriteria;

typedef unsigned int CountCriteria;
inline constexpr auto UNDEFINED_COUNT = 0xFFFF;
inline constexpr auto COUNT_CURRENT_ENEMIES = 0xFF; // use the number of enemies we see right when we speak
inline constexpr auto COUNT_MANY = 4; // equal to or greater than this is "many"

inline constexpr auto UNDEFINED_SUBJECT = (-1);

/// @todo Make Place a class with member fuctions for this
bool GetRandomSpotAtPlace( Place place, Vector *pPos );

//----------------------------------------------------------------------------------------------------
/**
 * A meme is a unit information that bots use to 
 * transmit information to each other via the radio
 */
class BotMeme
{
public:
	/// the concrete meme types
	enum class Kind
	{
		Help,
		BombsiteStatus,
		BombStatus,
		Follow,
		DefendHere,
		WhereBomb,
		RequestReport,
		AllHostagesGone,
		HostageBeingTaken,
		HeardNoise,
		WarnSniper,
	};

	void Transmit( CCSBot *sender ) const;									///< transmit meme to other bots
	void Interpret( CCSBot *sender, CCSBot *receiver ) const;				///< cause the given bot to act on this meme
	static void Destroy( BotMeme *meme );									///< delete a meme through its concrete type

protected:
	explicit BotMeme( Kind kind ) : m_kind( kind ) { }
	~BotMeme() = default;

private:
	Kind m_kind;
};

//----------------------------------------------------------------------------------------------------
class BotHelpMeme : public BotMeme
{
public:
	BotHelpMeme( Place place = UNDEFINED_PLACE ) : BotMeme( Kind::Help )
	{
		m_place = place;
	}
	void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme

private:
	Place m_place;			///< where the help is needed
};

//----------------------------------------------------------------------------------------------------
class BotBombsiteStatusMeme : public BotMeme
{
public:
	enum StatusType { CLEAR, PLANTED };

	BotBombsiteStatusMeme( int zoneIndex, StatusType status );
	void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme

private:
	int m_zoneIndex;			///< the bombsite
	StatusType m_status;		///< whether it is cleared or the bomb is there (planted)
};

//----------------------------------------------------------------------------------------------------
class BotBombStatusMeme : public BotMeme
{
public:
	BotBombStatusMeme( CSGameState::BombState state, const Vector &pos );
	void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme

private:
	CSGameState::BombState m_state;
	Vector m_pos;
};

//----------------------------------------------------------------------------------------------------
class BotFollowMeme : public BotMeme
{
public:
	BotFollowMeme( void ) : BotMeme( Kind::Follow ) { }
	void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme
};

//----------------------------------------------------------------------------------------------------
class BotDefendHereMeme : public BotMeme
{
public:
	BotDefendHereMeme( const Vector &pos ) : BotMeme( Kind::DefendHere )
	{
		m_pos = pos;
	}
	void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme

private:
	Vector m_pos;
};

//----------------------------------------------------------------------------------------------------
class BotWhereBombMeme : public BotMeme
{
public:
	BotWhereBombMeme( void ) : BotMeme( Kind::WhereBomb ) { }
	void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme
};

//----------------------------------------------------------------------------------------------------
class BotRequestReportMeme : public BotMeme
{
public:
	BotRequestReportMeme( void ) : BotMeme( Kind::RequestReport ) { }
	void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme
};

//----------------------------------------------------------------------------------------------------
class BotAllHostagesGoneMeme : public BotMeme
{
public:
	BotAllHostagesGoneMeme( void ) : BotMeme( Kind::AllHostagesGone ) { }
	void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme
};

//----------------------------------------------------------------------------------------------------
class BotHostageBeingTakenMeme : public BotMeme
{
public:
	BotHostageBeingTakenMeme( void ) : BotMeme( Kind::HostageBeingTaken ) { }
	void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme
};

//----------------------------------------------------------------------------------------------------
class BotHeardNoiseMeme : public BotMeme
{
public:
	BotHeardNoiseMeme( void ) : BotMeme( Kind::HeardNoise ) { }
	void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme
};

//----------------------------------------------------------------------------------------------------
class BotWarnSniperMeme : public BotMeme
{
public:
	BotWarnSniperMeme( void ) : BotMeme( Kind::WarnSniper ) { }
	void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme
};

//----------------------------------------------------------------------------------------------------
enum BotStatementType
{
	REPORT_VISIBLE_ENEMIES,
	REPORT_ENEMY_ACTION,
	REPORT_MY_CURRENT_TASK,
	REPORT_MY_INTENTION,
	REPORT_CRITICAL_EVENT,
	REPORT_REQUEST_HELP,
	REPORT_REQUEST_INFORMATION,
	REPORT_ROUND_END,
	REPORT_MY_PLAN,
	REPORT_INFORMATION,
	REPORT_EMOTE,
	REPORT_ACKNOWLEDGE,			///< affirmative or negative
	REPORT_ENEMIES_REMAINING,
	REPORT_FRIENDLY_FIRE,
	REPORT_KILLED_FRIEND,
	REPORT_ENEMY_LOST,

	NUM_BOT_STATEMENT_TYPES
};

//----------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------
/**
 * BotSpeakables are the smallest unit of bot chatter.
 * They represent a specific wav file of a phrase, and the criteria for which it is useful
 */
class BotSpeakable
{
public:
	BotSpeakable();
	~BotSpeakable();
	char *m_phrase;
	float m_duration;
	PlaceCriteria m_place;
	CountCriteria m_count;
};
typedef std::vector< BotSpeakable * > BotSpeakableVector;
typedef std::vector< BotSpeakableVector * > BotVoiceBankVector;

//----------------------------------------------------------------------------------------------------
/**
 * The BotPhrase class is a collection of Speakables associated with a name, ID, and criteria
 */
class BotPhrase
{
public:
	char *GetSpeakable( int bankIndex, float *duration = nullptr ) const;		///< return a random speakable and its duration in seconds that meets the current criteria

	// NOTE: Criteria must be set just before the GetSpeakable() call, since they are shared among all bots
	void ClearCriteria( void ) const;
	void SetPlaceCriteria( PlaceCriteria place ) const;	///< all returned phrases must have this place criteria
	void SetCountCriteria( CountCriteria count ) const;	///< all returned phrases must have this count criteria
	void SetCriteriaSet( AI_CriteriaSet &criteria ) const;

	const char *GetName( void ) const					{ return m_name; }
	const unsigned int GetPlace( void ) const		{ return m_place; }
	RadioType GetRadioEquivalent( void ) const	{ return m_radioEvent; }			///< return equivalent "standard radio" event
	bool IsImportant( void ) const						{ return m_isImportant; }	///< return true if this phrase is part of an important statement

	bool IsPlace( void ) const								{ return m_isPlace; }

	void Randomize( void );									///< randomly shuffle the speakable order

	PlaceCriteria GetPlaceCriteria( void ) const { return m_placeCriteria; }
	CountCriteria GetCountCriteria( void ) const { return m_countCriteria; }
	AI_CriteriaSet &GetCriteriaSet( void ) const { return m_contexts; }
private:
	friend class BotPhraseManager;
	BotPhrase( bool isPlace );
	~BotPhrase();

	char *m_name;
	Place m_place;
	bool m_isPlace;											///< true if this is a Place phrase
	RadioType m_radioEvent;									///< equivalent radio event
	bool m_isImportant;										///< mission-critical statement

	mutable BotVoiceBankVector m_voiceBank;					///< array of voice banks (arrays of speakables)
	std::vector< int > m_count;								///< number of speakables
	mutable std::vector< int > m_index;						///< index of next speakable to return
	int m_numVoiceBanks;									///< number of voice banks that have been initialized
	void InitVoiceBank( int bankIndex );					///< sets up the vector of voice banks for the first bankIndex voice banks

	mutable PlaceCriteria m_placeCriteria;
	mutable CountCriteria m_countCriteria;
	mutable AI_CriteriaSet m_contexts;
};
typedef std::vector< BotPhrase * > BotPhraseList;

enum BotChatterOutputType
{
	BOT_CHATTER_RADIO,
	BOT_CHATTER_VOICE
};
typedef std::vector< BotChatterOutputType > BotOutputList;

//----------------------------------------------------------------------------------------------------
/**
 * The BotPhraseManager is a singleton that provides an interface to all BotPhrase collections
 */
class BotPhraseManager
{
public:
	BotPhraseManager( void );
	~BotPhraseManager();

	bool Initialize( const char *filename, int bankIndex );	///< initialize phrase system from database file for a specific voice bank (0 is the default voice bank)

	void OnRoundRestart( void );												///< invoked when round resets
	void OnMapChange( void );														///< invoked when map changes
	void Reset( void );

	const BotPhrase *GetPhrase( const char *name ) const;		///< given a name, return the associated phrase collection
	const BotPhrase *GetPainPhrase( void ) const { return m_painPhrase; }					///< optimization, replaces a static pointer to the phrase
	const BotPhrase *GetAgreeWithPlanPhrase( void ) const { return m_agreeWithPlanPhrase; }	///< optimization, replaces a static pointer to the phrase

	const BotPhrase *GetPlace( const char *name ) const;		///< given a name, return the associated Place phrase collection
	const BotPhrase *GetPlace( unsigned int id ) const;			///< given an id, return the associated Place phrase collection

	const BotPhraseList *GetPlaceList( void ) const	{ return &m_placeList; }

	float GetPlaceStatementInterval( Place where ) const;	///< return time last statement of given type was emitted by a teammate for the given place
	void ResetPlaceStatementInterval( Place where );			///< set time of last statement of given type was emitted by a teammate for the given place

	BotChatterOutputType GetOutputType( int voiceBank ) const;

private:
	BotPhraseList m_list;																	///< master list of all phrase collections
	BotPhraseList m_placeList;														///< master list of all Place phrases

	BotOutputList m_output;

	const BotPhrase *m_painPhrase;
	const BotPhrase *m_agreeWithPlanPhrase;

	struct PlaceTimeInfo
	{
		Place placeID;
		IntervalTimer timer;
	};
	mutable PlaceTimeInfo m_placeStatementHistory[ MAX_PLACES_PER_MAP ];
	mutable int m_placeCount;
	int FindPlaceIndex( Place where ) const;
};

/**
 * Return time last statement of given type was emitted by a teammate for the given place
 */

/**
 * Set time of last statement of given type was emitted by a teammate for the given place
 */

extern BotPhraseManager *TheBotPhrases;

//----------------------------------------------------------------------------------------------------
/**
 * Statements are meaningful collections of phrases
 */
class BotStatement
{
public:
	BotStatement( BotChatterInterface *chatter, BotStatementType type, float expireDuration );
	~BotStatement();

	BotChatterInterface *GetChatter( void ) const	{ return m_chatter; }
	CCSBot *GetOwner( void ) const;

	BotStatementType GetType( void ) const	{ return m_type; }	///< return the type of statement this is
	bool IsImportant( void ) const;								///< return true if this statement is "important" and not personality chatter

	bool HasSubject( void ) const	{ return (m_subject == UNDEFINED_SUBJECT) ? false : true; }
	void SetSubject( int playerID )	{ m_subject = playerID; }	///< who this statement is about
	int GetSubject( void ) const	{ return m_subject; }		///< who this statement is about

	bool HasPlace( void ) const		{ return (GetPlace()) ? true : false; }
	Place GetPlace( void ) const;								///< if this statement refers to a specific place, return that place
	void SetPlace( Place where )	{ m_place = where; }		///< explicitly set place

	bool HasCount( void ) const;								///< return true if this statement has an associated count

	bool IsRedundant( const BotStatement *say ) const;			///< return true if this statement is the same as the given one
	bool IsObsolete( void ) const;								///< return true if this statement is no longer appropriate to say
	void Convert( const BotStatement *say );					///< possibly change what were going to say base on what teammate is saying
	
	void AppendPhrase( const BotPhrase *phrase );

	void SetStartTime( float timestamp )	{ m_startTime = timestamp; }	///< define the earliest time this statement can be spoken
	float GetStartTime( void ) const		{ return m_startTime; }

	enum ConditionType
	{
		IS_IN_COMBAT,
		RADIO_SILENCE,
		ENEMIES_REMAINING,

		NUM_CONDITIONS
	};

	void AddCondition( ConditionType condition );				///< conditions must be true for the statement to be spoken
	bool IsValid( void ) const;									///< verify all attached conditions 

	enum ContextType
	{
		CURRENT_ENEMY_COUNT,
		REMAINING_ENEMY_COUNT,
		SHORT_DELAY,
		LONG_DELAY,
		ACCUMULATE_ENEMIES_DELAY
	};
	void AppendPhrase( ContextType contextPhrase );				///< special phrases that depend on the context

	bool Update( void );											///< emit statement over time, return false if statement is done
	bool IsSpeaking( void ) const		{ return m_isSpeaking; }	///< return true if this statement is currently being spoken
	float GetTimestamp( void ) const	{ return m_timestamp; }		///< get time statement was created (but not necessarily started talking)

	void AttachMeme( BotMeme *meme );							///< attach a meme to this statement, to be transmitted to other friendly bots when spoken

private:
	friend class BotChatterInterface;

	BotChatterInterface *m_chatter;								///< the chatter system this statement is part of

	BotStatement *m_next, *m_prev;								///< linked list hooks

	BotStatementType m_type;									///< what kind of statement this is
	int m_subject;												///< who this subject is about
	Place m_place;												///< explicit place - note some phrases have implicit places as well
	BotMeme *m_meme;											///< a statement can only have a single meme for now

	float m_timestamp;											///< time when message was created
	float m_startTime;											///< the earliest time this statement can be spoken
	float m_expireTime;											///< time when this statement is no longer valid
	float m_speakTimestamp;										///< time when message began being spoken
	bool m_isSpeaking;											///< true if this statement is current being spoken

	float m_nextTime;											///< time for next phrase to begin

	enum { MAX_BOT_PHRASES = 4 };
	struct
	{
		bool isPhrase;
		union
		{
			const BotPhrase *phrase;
			ContextType context;
		};
	}
	m_statement[ MAX_BOT_PHRASES ];

	enum { MAX_BOT_CONDITIONS = 4 };
	ConditionType m_condition[ MAX_BOT_CONDITIONS ];			///< conditions that must be true for the statement to be said
	int m_conditionCount;

	int m_index;												///< m_index refers to the phrase currently being spoken, or -1 if we havent started yet
	int m_count;
};

//----------------------------------------------------------------------------------------------------
/**
 * This class defines the interface to the bot radio chatter system
 */
class BotChatterInterface
{
public:
	/// chatter implementations
	enum class Kind
	{
		Standard,
		Coop,			///< BotChatterCoop
	};

	explicit BotChatterInterface( CCSBot *me, Kind kind = Kind::Standard );
	~BotChatterInterface();

	void Reset( void );											///< reset to initial state
	void Update( void );										///< process ongoing chatter

	/// invoked when event occurs in the game (some events have NULL entities)
	void OnDeath( void );										///< invoked when we die

	enum VerbosityType
	{
		NORMAL,				///< full chatter
		MINIMAL,			///< only scenario-critical events
		RADIO,				///< use the standard radio instead
		OFF						///< no chatter at all
	};
	VerbosityType GetVerbosity( void ) const;					///< return our current level of verbosity

	CCSBot *GetOwner( void ) const							{ return m_me; }

	bool IsTalking( void ) const;								///< return true if we are currently talking
	float GetRadioSilenceDuration( void );						///< return time since any teammate said anything
	void ResetRadioSilenceDuration( void );

	enum { MUST_ADD = 1 };
	void AddStatement( BotStatement *statement, bool mustAdd = false );			///< register a statement for speaking
	void RemoveStatement( BotStatement *statement );			///< remove a statement

	BotStatement *GetActiveStatement( void );					///< returns the statement that is being spoken, or is next to be spoken if no-one is speaking now
	BotStatement *GetStatement( void ) const;					///< returns our current statement, or NULL if we aren't speaking

	int GetPitch( void ) const			{ return m_pitch; }

	//-- things the bots can say ---------------------------------------------------------------------
	void Say( const char *phraseName, float lifetime = 3.0f, float delay = 0.0f );

	void AnnouncePlan( const char *phraseName, Place where );
	void Affirmative( void );
	void Negative( void );

	// BotChatterCoop's versions of these four are used for Kind::Coop
	void EnemySpotted( void );											///< report enemy sightings
	void KilledMyEnemy( int victimID );
	void EnemiesRemaining( void );

	void SpottedSniper( void );
	void FriendSpottedSniper( void );

	void Clear( Place where );

	void ReportIn( void );										///< ask for current situation
	void ReportingIn( void );									///< report current situation

	bool NeedBackup( void );
	void PinnedDown( void );
	void Scared( void );
	void HeardNoise( const Vector &pos );
	void FriendHeardNoise( void );

	void TheyPickedUpTheBomb( void );
	void GoingToPlantTheBomb( Place where );
	void BombsiteClear( int zoneIndex );
	void FoundPlantedBomb( int zoneIndex );
	void PlantingTheBomb( Place where );
	void SpottedBomber( CBasePlayer *bomber );
	void SpottedLooseBomb( CBaseEntity *bomb );
	void GuardingLooseBomb( CBaseEntity *bomb );
	void RequestBombLocation( void );

	void GuardingHostages( Place where, bool isPlan = false );
	void GuardingHostageEscapeZone( bool isPlan = false );
	void HostagesBeingTaken( void );
	void HostagesTaken( void );
	void TalkingToHostages( void );
	void EscortingHostages( void );
	void HostageDown( void );
	void GuardingBombsite( Place where );

	void CelebrateWin( void );

	void Encourage( const char *phraseName, float repeatInterval = 10.0f, float lifetime = 3.0f );	///< "encourage" the player to do the scenario

	void KilledFriend( void );
	void FriendlyFire( const char *pDmgType );
	void DoPhoenixHeavyWakeTaunt( void );

	bool SeesAtLeastOneEnemy( void ) const { return m_seeAtLeastOneEnemy; }

protected:
	// the default implementations (BotChatterCoop falls back to them)
	void EnemySpottedDefault( void );
	void KilledMyEnemyDefault( int victimID );
	void EnemiesRemainingDefault( void );
	void CelebrateWinDefault( void );

private:
	Kind m_kind;

	BotStatement *m_statementList;										///< list of all active/pending messages for this bot

	void ReportEnemies( void );											///< track nearby enemy count and generate enemy activity statements
	bool ShouldSpeak( void ) const;										///< return true if we speaking makes sense now

	CCSBot *m_me;														///< the bot this chatter is for

	bool m_seeAtLeastOneEnemy;
	float m_timeWhenSawFirstEnemy;
	bool m_reportedEnemies;
	bool m_requestedBombLocation;										///< true if we already asked where the bomb has been planted

	int m_pitch;

	static IntervalTimer m_radioSilenceInterval[ 2 ];					///< one timer for each team

	IntervalTimer m_needBackupInterval;
	IntervalTimer m_spottedBomberInterval;
	IntervalTimer m_scaredInterval;
	IntervalTimer m_planInterval;
	CountdownTimer m_spottedLooseBombTimer;
	CountdownTimer m_heardNoiseTimer;
	CountdownTimer m_escortingHostageTimer;
	CountdownTimer m_warnSniperTimer;
	static CountdownTimer m_encourageTimer;								///< timer to know when we can "encourage" the human player again - shared by all bots
	CountdownTimer m_heavyTauntTimer;
};

// In player vs bot game modes, have the bots chatter about what they're doing to the players
class BotChatterCoop : public BotChatterInterface
{
	typedef BotChatterInterface BaseClass;
public:
	BotChatterCoop( CCSBot *me );
	void KilledMyEnemy( int nVictimID );
	void EnemiesRemaining( void );
	void CelebrateWin( void );
	void EnemySpotted( void );
};
