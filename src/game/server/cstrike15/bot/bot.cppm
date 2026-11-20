// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include <stdlib.h>
#include "edict.h"
#include "bspfile.h"
#include "mathlib/femodeldesc.h"
#include "declare_class.h"
#include "../../baseentity.h"
module ks.server:cstrike15.bot.bot;

import std;
import ks.core.basetypes;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import :player;
import :baseentity;
import ks.core.vprof;
import ks.core.wchartypes;
import ks.tier1.convar;
import ks.core.platform;
import :cstrike15.cs_playeranimstate;
import ks.core.dbg;
import ks.game.in_buttons;
import ks.math.mathlib;
import ks.math.ssemath;
import :fish;
import :nav_4;
import ks.game.shareddefs;
import :sharedvar;
import ks.tier1.exprevaluator;

// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2003

/// version number is MAJOR.MINOR
inline constexpr auto BOT_VERSION_MAJOR = 1;
inline constexpr auto BOT_VERSION_MINOR = 50;

//--------------------------------------------------------------------------------------------------------
/**
 * Difficulty levels
 */
enum BotDifficultyType
{
	BOT_EASY = 0,
	BOT_NORMAL = 1,
	BOT_HARD = 2,
	BOT_EXPERT = 3,

	NUM_DIFFICULTY_LEVELS
};

inline const char *BotDifficultyName[] =
{
	"EASY", "NORMAL", "HARD", "EXPERT", nullptr
};

namespace BotProfileInputDevice
{
	enum Device
	{
		GAMEPAD				= 0,
		KB_MOUSE			= 1,
		PS3_MOVE			= 2,
		HYDRA				= 3,
		SHARPSHOOTER		= 4,

		COUNT,								// Auto list counter
		FORCE_INT32  = 0x7FFFFFFF			// Force the typedef to be int32
	};
};
typedef BotProfileInputDevice::Device  BotProfileDevice_t;

class CBaseGrenade;
class CBasePlayer;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

extern float g_BotUpkeepInterval;					///< duration between bot upkeeps
extern float g_BotUpdateInterval;					///< duration between bot updates
const int g_BotUpdateSkipCount = 2;					///< number of upkeep periods to skip update

//--------------------------------------------------------------------------------------------------------------

/**
 * An ActiveGrenade is a representation of a grenade in the world
 * NOTE: Currently only used for smoke grenade line-of-sight testing
 * @todo Use system allow bots to avoid HE and Flashbangs
 */
class ActiveGrenade
{
public:
	ActiveGrenade( CBaseGrenade *grenadeEntity );

	void OnEntityGone( void );								///< called when the grenade in the world goes away
	void Update( void );									///< called every frame
	bool IsValid( void ) const	;							///< return true if this grenade is valid
	
	bool IsEntity( CBaseGrenade *grenade ) const		{ return (grenade == m_entity) ? true : false; }
	CBaseGrenade *GetEntity( void ) const				{ return m_entity; }

	const Vector &GetDetonationPosition( void ) const	{ return m_detonationPosition; }
	const Vector &GetPosition( void ) const;
	bool IsSmoke( void ) const							{ return m_isSmoke; }
	bool IsFlashbang( void ) const						{ return m_isFlashbang; }
	bool IsMolotov( void ) const						{ return m_isMolotov; }
	bool IsDecoy( void ) const							{ return m_isDecoy; }
	bool IsSensor( void ) const							{ return m_isSensor; }
	CBaseGrenade *GetGrenade( void ) { return m_entity; }
	float GetRadius( void ) const { return m_radius; }
	void SetRadius( float radius ) { m_radius = radius; }

private:
	CBaseGrenade *m_entity;									///< the entity
	Vector m_detonationPosition;							///< the location where the grenade detonated (smoke)
	float m_dieTimestamp;									///< time this should go away after m_entity is NULL
	bool m_isSmoke;											///< true if this is a smoke grenade
	bool m_isFlashbang;										///< true if this is a flashbang grenade
	bool m_isMolotov;										///< true if this is a molotov grenade
	bool m_isDecoy;											///< true if this is a decoy grenade
	bool m_isSensor;										///< true if this is a sensor grenade
	float m_radius;
};

typedef std::vector<std::unique_ptr<ActiveGrenade>> ActiveGrenadeList;

//--------------------------------------------------------------------------------------------------------------
/**
 * This class manages all active bots, propagating events to them and updating them.
 */
class CBotManager
{
public:
	CBotManager();
	~CBotManager();

	// CCSBotManager (the only bot manager) provides AllocateBotEntity(), ClientDisconnect(), ClientCommand(),
	// ServerActivate(), ServerDeactivate(), ServerCommand(), RestartRound(), StartFrame() and GetPlayerPriority().
	// Use TheCSBots() to reach them.

	static void SetPlayerEdictForNextBot( edict_t *ed )	{ CBasePlayer::s_PlayerEdict = ed; }	///< sets the edict the next bot entity binds to; call before constructing it

	void RestartRoundBase( void );								///< invoked by CCSBotManager::RestartRound()
	void StartFrameBase( void );								///< invoked by CCSBotManager::StartFrame()

	void AddGrenade( CBaseGrenade *grenade );					///< add an active grenade to the bot's awareness
	void RemoveGrenade( CBaseGrenade *grenade );				///< the grenade entity in the world is going away
	void SetGrenadeRadius( CBaseGrenade *grenade, float radius );	///< the radius of the grenade entity (or associated smoke cloud)
	void ValidateActiveGrenades( void );						///< destroy any invalid active grenades
	void DestroyAllGrenades( void );
	bool IsLineBlockedBySmoke( const Vector &from, const Vector &to, float grenadeBloat = 1.0f );	///< return true if line intersects smoke volume, with grenade radius increased by the grenadeBloat factor
	bool IsInsideSmokeCloud( const Vector *pos, float radius );	///< return true if sphere at position overlaps a smoke cloud

	//
	// Invoke functor on all active grenades.
	// If any functor call return false, return false.  Otherwise, return true.
	//
	template < typename T >
	bool ForEachGrenade( T &func )
	{
		for ( auto it = m_activeGrenadeList.begin(); it != m_activeGrenadeList.end(); )
		{
			ActiveGrenade *ag = it->get();

			// lazy validation
			if (!ag->IsValid())
			{
				it = m_activeGrenadeList.erase( it );
				continue;
			}
			else
			{
				if (func( ag ) == false)
				{
					return false;
				}
			}
			++it;
		}

		return true;
	}

	enum { MAX_DBG_MSG_SIZE = 1024 };
	struct DebugMessage
	{
		char m_string[ MAX_DBG_MSG_SIZE ];
		IntervalTimer m_age;
	};

	// debug message history -------------------------------------------------------------------------------
	int GetDebugMessageCount( void ) const;						///< get number of debug messages in history
	const DebugMessage *GetDebugMessage( int which = 0 ) const;	///< return the debug message emitted by the bot (0 = most recent)
	void ClearDebugMessages( void );
	void AddDebugMessage( const char *msg );

	ActiveGrenadeList m_activeGrenadeList;///< the list of active grenades the bots are aware of

private:

	enum { MAX_DBG_MSGS = 6 };
	DebugMessage m_debugMessage[ MAX_DBG_MSGS ];				///< debug message history
	int m_debugMessageCount;
	int m_currentDebugMessage;

	IntervalTimer m_frameTimer;									///< for measuring each frame's duration
};

// global singleton to create and control bots
extern CBotManager *TheBots;

class BotProfile;
class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;


//--------------------------------------------------------------------------------------------------------------
enum PriorityType
{
	PRIORITY_LOW, PRIORITY_MEDIUM, PRIORITY_HIGH, PRIORITY_UNINTERRUPTABLE
};

extern ConVar cv_bot_traceview;
extern ConVar cv_bot_stop;
extern ConVar cv_bot_show_nav;
extern ConVar cv_bot_walk;
extern ConVar cv_bot_difficulty;
extern ConVar cv_bot_debug;
extern ConVar cv_bot_debug_target;
extern ConVar cv_bot_quota;
extern ConVar cv_bot_quota_mode;
extern ConVar cv_bot_prefix;
extern ConVar cv_bot_allow_rogues;
extern ConVar cv_bot_allow_pistols;
extern ConVar cv_bot_allow_shotguns;
extern ConVar cv_bot_allow_sub_machine_guns;
extern ConVar cv_bot_allow_rifles;
extern ConVar cv_bot_allow_machine_guns;
extern ConVar cv_bot_allow_grenades;
extern ConVar cv_bot_allow_snipers;
extern ConVar cv_bot_allow_shield;
extern ConVar cv_bot_join_team;
extern ConVar cv_bot_join_after_player;
extern ConVar cv_bot_auto_vacate;
extern ConVar cv_bot_zombie;
extern ConVar cv_bot_defer_to_human_goals;
extern ConVar cv_bot_defer_to_human_items;
extern ConVar cv_bot_chatter;
extern ConVar cv_bot_profile_db;
extern ConVar cv_bot_dont_shoot;
extern ConVar cv_bot_eco_limit;
extern ConVar cv_bot_auto_follow;
extern ConVar cv_bot_flipout;
extern ConVar cv_bot_controllable;

template <typename T1> [[gnu::always_inline]] inline auto RAD_TO_DEG( T1 deg ) { return ((deg) * 180.0 / std::numbers::pi); }
template <typename T1> [[gnu::always_inline]] inline auto DEG_TO_RAD( T1 rad ) { return ((rad) * std::numbers::pi / 180.0); }

#define SIGN( num )	      (((num) < 0) ? -1 : 1)
#define ABS( num )        (SIGN(num) * (num))

#define CREATE_FAKE_CLIENT		( *g_engfuncs.pfnCreateFakeClient )
#define GET_USERINFO			( *g_engfuncs.pfnGetInfoKeyBuffer )
#define SET_KEY_VALUE			( *g_engfuncs.pfnSetKeyValue )
#define SET_CLIENT_KEY_VALUE	( *g_engfuncs.pfnSetClientKeyValue )


extern void   BotPrecache( void );
extern int		UTIL_ClientsInGame( void );

extern bool UTIL_IsNameTaken( const char *name, bool ignoreHumans = false );		///< return true if given name is already in use by another player

inline constexpr auto IS_ALIVE = true;
extern int UTIL_HumansOnTeam( int teamID, bool isAlive = false );

extern int		UTIL_BotsInGame( void );
extern bool		UTIL_IsTeamAllBots( int team );
extern void		UTIL_DrawBeamFromEnt( int iIndex, Vector vecEnd, int iLifetime, byte bRed, byte bGreen, byte bBlue );
extern void		UTIL_DrawBeamPoints( Vector vecStart, Vector vecEnd, int iLifetime, byte bRed, byte bGreen, byte bBlue );
extern CBasePlayer *UTIL_GetClosestPlayer( const Vector &pos, float *distance = nullptr );
extern CBasePlayer *UTIL_GetClosestPlayer( const Vector &pos, int team, float *distance = nullptr );
extern bool UTIL_KickBotFromTeam( int kickTeam, bool bQueue = false ); ///< kick a bot from the given team. If no bot exists on the team, return false, if bQueue is true kick will occur at round restart

extern bool UTIL_IsVisibleToTeam( const Vector &spot, int team ); ///< return true if anyone on the given team can see the given spot
extern bool UTIL_IsRandomSpawnFarEnoughAwayFromTeam( const Vector &spot, int team ); ///< return true if this spot is far enough away from everyone on specified team defined via 

/// return true if moving from "start" to "finish" will cross a player's line of fire.
extern bool IsCrossingLineOfFire( const Vector &start, const Vector &finish, CBaseEntity *ignore = nullptr, int ignoreTeam = 0 );

extern void UTIL_ConstructBotNetName(char *name, int nameLength, const BotProfile *bot);	///< constructs a complete name including prefix

extern void InitBotTrig( void );
extern float BotCOS( float angle );
extern float BotSIN( float angle );

extern void HintMessageToAllPlayers( const char *message );

bool WildcardMatch( const char *query, const char *test );	///< Performs a simple case-insensitive string comparison, honoring trailing * wildcards

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if the given entity is valid
 */
inline bool IsEntityValid( CBaseEntity *entity )
{
	if (entity == nullptr)
		return false;

	if (FNullEnt( entity->edict() ))
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Given two line segments: startA to endA, and startB to endB, return true if they intesect
 * and put the intersection point in "result".
 * Note that this computes the intersection of the 2D (x,y) projection of the line segments.
 */
inline bool IsIntersecting2D( const Vector &startA, const Vector &endA, 
															const Vector &startB, const Vector &endB, 
															Vector *result = nullptr )
{
	float denom = (endA.x - startA.x) * (endB.y - startB.y) - (endA.y - startA.y) * (endB.x - startB.x);
	if (denom == 0.0f)
	{
		// parallel
		return false;
	}

	float numS = (startA.y - startB.y) * (endB.x - startB.x) - (startA.x - startB.x) * (endB.y - startB.y);
	if (numS == 0.0f)
	{
		// coincident
		return true;
	}

	float numT = (startA.y - startB.y) * (endA.x - startA.x) - (startA.x - startB.x) * (endA.y - startA.y);

	float s = numS / denom;
	if (s < 0.0f || s > 1.0f)
	{
		// intersection is not within line segment of startA to endA
		return false;
	}

	float t = numT / denom;
	if (t < 0.0f || t > 1.0f)
	{
		// intersection is not within line segment of startB to endB
		return false;
	}

	// compute intesection point
	if (result)
		*result = startA + s * (endA - startA);

	return true;
}

#undef SIGN
#undef ABS
#undef CREATE_FAKE_CLIENT
#undef GET_USERINFO
#undef SET_KEY_VALUE
#undef SET_CLIENT_KEY_VALUE

class BotProfile;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

enum
{
	FirstCustomSkin = 100,
	NumCustomSkins = 100,
	LastCustomSkin = FirstCustomSkin + NumCustomSkins - 1,
};

//--------------------------------------------------------------------------------------------------------------
/**
 * A BotProfile describes the "personality" of a given bot
 */
class BotProfile
{
public:
	BotProfile( void );

	~BotProfile( void );

	const char *GetName( void ) const { return m_name; }		///< return bot's name
	float GetAggression() const;
	float GetSkill() const;
	float GetTeamwork() const;

	float GetAimFocusInitial() const									{ return m_aimFocusInitial; }
	float GetAimFocusDecay() const										{ return m_aimFocusDecay; }
	float GetAimFocusOffsetScale() const								{ return m_aimFocusOffsetScale; }
	float GetAimFocusInterval() const									{ return m_aimFocusInterval; }

	CSWeaponID GetWeaponPreference(int i ) const;
	const char *GetWeaponPreferenceAsString( int i ) const;
	int GetWeaponPreferenceCount() const;
	bool HasPrimaryPreference() const;				///< return true if this profile has a primary weapon preference
	bool HasPistolPreference() const;				///< return true if this profile has a pistol weapon preference

	int GetCost() const;
	int GetSkin() const;
	int GetRank() const;
	int GetMaxDifficulty() const;					///< return maximum difficulty flag value
	bool IsDifficulty( BotDifficultyType diff ) const;	///< return true if this profile can be used for the given difficulty level
	bool IsMaxDifficulty( BotDifficultyType diff ) const;	///< return true if this profile's highest difficulty matches the incoming difficulty level
	int GetVoicePitch() const;
	float GetReactionTime() const;
	float GetAttackDelay() const;
	int GetVoiceBank() const												{ return m_voiceBank; }
	unsigned char GetDifficultyFlags() const;
	int GetTeams( void ) const												{ return m_teams; }

	float GetLookAngleMaxAccelerationNormal() const;
	float GetLookAngleStiffnessNormal() const;
	float GetLookAngleDampingNormal() const;
	float GetLookAngleMaxAccelerationAttacking() const;
	float GetLookAngleStiffnessAttacking() const;
	float GetLookAngleDampingAttacking() const;

	bool IsValidForTeam( int team ) const;

	bool PrefersSilencer() const								{ return m_prefersSilencer; }

	bool InheritsFrom( const char *name ) const;

	void Clone( const BotProfile* source_profile );
	void SetName( const char* name );
	void SetVoicePitch( int pitch );
	int GetTemplatesCount( void ) const							{ return std::ssize( m_templates ); }
	const BotProfile* GetTemplate( int index ) const;

private:
	friend class BotProfileManager;						///< for loading profiles

	void Inherit( const BotProfile *parent, const BotProfile *baseline );	///< copy values from parent if they differ from baseline

	char *m_name;							// the bot's name
	float m_aggression;						// percentage: 0 = coward, 1 = berserker
	float m_skill;							// percentage: 0 = terrible, 1 = expert
	float m_teamwork;						// percentage: 0 = rogue, 1 = complete obeyance to team, lots of comm

	float m_aimFocusInitial;				// initial minimum aim error on first attack
	float m_aimFocusDecay;					// how quickly our focus error decays (scale/sec)
	float m_aimFocusOffsetScale;			// how much aim focus error we get based on maximum angle distance from our view angle
	float m_aimFocusInterval;				// how frequently we update our focus

	enum { MAX_WEAPON_PREFS = 16 };
	CSWeaponID m_weaponPreference[ MAX_WEAPON_PREFS ];	///< which weapons this bot likes to use, in order of priority
	int m_weaponPreferenceCount;

	int m_cost;								///< reputation point cost for career mode
	int m_skin;								///< "skin" index
	int m_rank;								///< skill rank, as an elo-like rating
	unsigned char m_difficultyFlags;		///< bits set correspond to difficulty levels this is valid for
	int m_voicePitch;						///< the pitch shift for bot chatter (100 = normal)
	float m_reactionTime;					///< our reaction time in seconds
	float m_attackDelay;					///< time in seconds from when we notice an enemy to when we open fire
	int m_teams;							///< teams for which this profile is valid

	bool m_prefersSilencer;					///< does the bot prefer to use silencers?

	int m_voiceBank;						///< Index of the BotChatter.db voice bank this profile uses (0 is the default)

	float m_lookAngleMaxAccelNormal;		// Acceleration of look angle spring under normal conditions
	float m_lookAngleStiffnessNormal;		// Stiffness of look angle spring under normal conditions
	float m_lookAngleDampingNormal;			// Damping of look angle spring under normal conditions

	float m_lookAngleMaxAccelAttacking;		// Acceleration of look angle spring under attack conditions
	float m_lookAngleStiffnessAttacking;	// Stiffness of look angle spring under attack conditions
	float m_lookAngleDampingAttacking;		// Damping of look angle spring under attack conditions

	std::vector< const BotProfile * > m_templates;							///< List of templates we inherit from
};
typedef std::vector<std::unique_ptr<BotProfile>> BotProfileList;

/**
 * Copy in data from parent if it differs from the baseline
 */

//--------------------------------------------------------------------------------------------------------------
/**
 * The BotProfileManager defines the interface to accessing BotProfiles
 */
class BotProfileManager
{
public:
	BotProfileManager( void );
	~BotProfileManager( void );

	void Init( const char *filename, unsigned int *checksum = nullptr );
	void Reset( void );

	/// given a name, return a profile
	const BotProfile *GetProfile( const char *name, int team ) const
	{
		for ( const auto &pProfile : m_profileList )
		{
			const BotProfile *profile = pProfile.get();
	
			if ( !stricmp( name, profile->GetName() ) && profile->IsValidForTeam( team ) )
				return profile;
		}

		return nullptr;
	}

	/// given a template name and difficulty, return a profile
	const BotProfile *GetProfileMatchingTemplate( const char *profileName, int team, BotDifficultyType difficulty, BotProfileDevice_t device, bool bAllowDupeNames = false ) const
	{
		for ( const auto &pProfile : m_profileList )
		{
			const BotProfile *profile = pProfile.get();

			if ( !profile->InheritsFrom( profileName ) )
				continue;

			if ( !profile->IsValidForTeam( team ) )
				continue;

			if ( !profile->IsDifficulty( difficulty ) )
				continue;

			if ( bAllowDupeNames == false && UTIL_IsNameTaken( profile->GetName() ) )
				continue;

			return profile;
		}

		return nullptr;
	}

	const BotProfileList *GetProfileList( void ) const		{ return &m_profileList; }		///< return list of all profiles

	const BotProfile *GetRandomProfile( BotDifficultyType difficulty, int team, CSWeaponType weaponType, bool forceMatchHighestDifficulty = false ) const;			///< return random unused profile that matches the given difficulty level

	const char * GetCustomSkin( int index );				///< Returns custom skin name at a particular index
	const char * GetCustomSkinModelname( int index );		///< Returns custom skin modelname at a particular index
	const char * GetCustomSkinFname( int index );			///< Returns custom skin filename at a particular index
	int GetCustomSkinIndex( const char *name, const char *filename = nullptr );	///< Looks up a custom skin index by name

	typedef std::vector< char * > VoiceBankList;
	const VoiceBankList *GetVoiceBanks( void ) const		{ return &m_voiceBanks; }
	int FindVoiceBankIndex( const char *filename );		///< return index of the (custom) bot phrase db, inserting it if needed

protected:
	void ParseDifficultySetting( unsigned char &difficultyFlags, char* token);
	void ParseWeaponPreference( bool &isFirstWeaponPref, int &weaponPreferenceCount, CSWeaponID* weaponPreference, char* token );

protected:
	BotProfileList m_profileList;							///< the list of all bot profiles
	BotProfileList m_templateList;							///< the list of all bot templates

	VoiceBankList m_voiceBanks;

	char *m_skins[ NumCustomSkins ];						///< Custom skin names
	char *m_skinModelnames[ NumCustomSkins ];				///< Custom skin modelnames
	char *m_skinFilenames[ NumCustomSkins ];				///< Custom skin filenames
	int m_nextSkin;											///< Next custom skin to allocate
};

/// the global singleton for accessing BotProfiles
extern BotProfileManager *TheBotProfiles;

// Purpose: dll-agnostic routines (no dll dependencies here)

// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------
/**
 * Returns the token parsed by SharedParse()
 */
char *SharedGetToken( void );

//--------------------------------------------------------------------------------------------------------
/**
 * Sets the character used to delimit quoted strings.  Default is '\"'.  Be sure to set it back when done.
 */
void SharedSetQuoteChar( char c );

//--------------------------------------------------------------------------------------------------------
/**
 * Parse a token out of a string
 */
const char *SharedParse( const char *data );

//--------------------------------------------------------------------------------------------------------
/**
 * Returns true if additional data is waiting to be processed on this line
 */
bool SharedTokenWaiting( const char *buffer );

//--------------------------------------------------------------------------------------------------------------
/**
 *  snprintf-alike that allows multiple prints into a buffer
 */
char * BufPrintf(char *buf, int& len, const char *fmt, ...);

//--------------------------------------------------------------------------------------------------------------
/**
 *  wide char version of BufPrintf
 */

//--------------------------------------------------------------------------------------------------------------
/**
 *  convenience function that prints an int into a static wchar_t*
 */
// dgoodenough - PS3 needs this guy as well.
// PS3_BUILDFIX
//--------------------------------------------------------------------------------------------------------------
/**
 *  convenience function that prints an int into a static char*
 */
const char * NumAsString( int val );

//--------------------------------------------------------------------------------------------------------------
/**
 *  convenience function that composes a string into a static char*
 */
char * SharedVarArgs(const char *format, ...);

class BotProfile;
class CBaseEntity;
class CBasePlayer;
class CBotManager;
class CCSBot;
class CTakeDamageInfo;
class CUserCmd;
class CWeaponCSBase;

using namespace ::ks::reflect;


//
// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003
//
// NOTE: The CS Bot code uses Doxygen-style comments. If you run Doxygen over this code, it will 
// auto-generate documentation.  Visit www.doxygen.org to download the system for free.
//


//--------------------------------------------------------------------------------------------------------
inline char *CloneString( const char *str )
{
	char *cloneStr = new char [ std::strlen(str)+1 ];
	std::strcpy( cloneStr, str );
	return cloneStr;
}

extern bool AreBotsAllowed();

//--------------------------------------------------------------------------------------------------------
// BOTPORT: Convert everything to assume "origin" means "feet"

//
// Utility function to get "centroid" or center of player or player equivalent
//
inline Vector GetCentroid( const CBaseEntity *player )
{
	Vector centroid = player->GetAbsOrigin();

	const Vector &mins = player->WorldAlignMins();
	const Vector &maxs = player->WorldAlignMaxs();

	centroid.z += (maxs.z - mins.z)/2.0f;

	//centroid.z += HalfHumanHeight;

	return centroid;
}

CBasePlayer* ClientPutInServerOverride_Bot( edict_t *pEdict, const char *playername );

/// @todo Remove this nasty hack - CreateFakeClient() calls CBot::Spawn, which needs the profile
extern const BotProfile *g_botInitProfile;
extern int g_botInitTeam;
extern int g_nClientPutInServerOverrides;

//--------------------------------------------------------------------------------------------------------
template < class T > T * CreateBot( const BotProfile *profile, int team )
{
	if ( !AreBotsAllowed() )
		return nullptr;

	if ( UTIL_ClientsInGame() >= gpGlobals->maxClients )
	{
        Msg( "Unable to create bot: Server is full (%d/%d clients).\n", UTIL_ClientsInGame(), gpGlobals->maxClients );
		return nullptr;
	}

	// set the bot's name
	char botName[64];
	UTIL_ConstructBotNetName( botName, 64, profile );

	// This is a backdoor we use so when the engine calls ClientPutInServer (from CreateFakeClient), 
	// expecting the game to make an entity for the fake client, we can make our special bot class
	// instead of a CCSPlayer.
	g_nClientPutInServerOverrides = 0;
	ClientPutInServerOverride( ClientPutInServerOverride_Bot );
	
	// get an edict for the bot
	// NOTE: This will ultimately invoke CBot::Spawn(), so set the profile now
	g_botInitProfile = profile;
	g_botInitTeam = team;
	edict_t *botEdict = engine->CreateFakeClient( botName );

	ClientPutInServerOverride( nullptr );
	contract_assert( g_nClientPutInServerOverrides == 1 );

	if ( botEdict == nullptr )
	{
        Msg( "Unable to create bot: CreateFakeClient() returned null.\n" );
		return nullptr;
	}

	// create an instance of the bot's class and bind it to the edict
	// the entity is created by ClientPutInServerOverride_Bot() -> CCSBotManager::AllocateBotEntity(), which only makes "cs_bot" entities
	T *bot = static_cast< T * >( CBaseEntity::Instance( botEdict ) );

	if ( bot == nullptr )
	{
		contract_assert( false );
		Error( "Could not allocate and bind entity to bot edict.\n" );
		return nullptr;
	}

	bot->ClearFlags();
	bot->AddFlag( FL_CLIENT | FL_FAKECLIENT );

	return bot;
}

//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------
/**
 * The base bot class from which bots for specific games are derived
 * A template is needed here because the CBot class must be derived from CBasePlayer, 
 * but also may need to be derived from a more specific player class, such as CCSPlayer
 */
class CCSBot;	// the only class derived from CBot

template < class PlayerType >
class CBot : public PlayerType
{
public:
	DECLARE_CLASS( CBot, PlayerType );

	CBot( void );												///< constructor initializes all values to zero
	~CBot();
	bool Initialize( const BotProfile *profile, int team );		///< prepare bot for action

	unsigned int GetID( void ) const		{ return m_id; }	///< return bot's unique ID

	virtual bool IsBot( void ) const { return true; }	
	 bool IsNetClient_Impl( void ) const { return false; }	// Bots should return false for this, they can't receive NET messages

	 void Spawn_Impl( void );									///< (EXTEND) spawn the bot into the game

	// Upkeep() and Update() are provided by CCSBot

	void Run( void );
	void Walk( void );
	virtual bool IsRunning( void ) const		{ return m_isRunning; }
	
	void Crouch( void );
	void StandUp( void );
	bool IsCrouching( void ) const	{ return m_isCrouching; }

	void PushPostureContext( void );							///< push the current posture context onto the top of the stack
	void PopPostureContext( void );								///< restore the posture context to the next context on the stack

	void MoveForward( void );
	void MoveBackward( void );
	void StrafeLeft( void );
	void StrafeRight( void );

	bool Jump( bool mustJump = false );					///< returns true if jump was started
	bool IsJumping( void );										///< returns true if we are in the midst of a jump
	float GetJumpTimestamp( void ) const	{ return m_jumpTimestamp; }	///< return time last jump began

	void ClearMovement( void );						///< zero any MoveForward(), Jump(), etc

	const Vector &GetViewVector( void );						///< return the actual view direction

	//------------------------------------------------------------------------------------
	// Weapon interface
	//
	void UseEnvironment( void );
	void PrimaryAttack( void );
	void ClearPrimaryAttack( void );
	void TogglePrimaryAttack( void );
	void SecondaryAttack( void );
	void Reload( void );

	float GetActiveWeaponAmmoRatio( void ) const;				///< returns ratio of ammo left to max ammo (1 = full clip, 0 = empty)
	bool IsActiveWeaponClipEmpty( void ) const;					///< return true if active weapon has any empty clip
	bool IsActiveWeaponOutOfAmmo( void ) const;					///< return true if active weapon has no ammo at all
	bool IsActiveWeaponRecoilHigh( void ) const;				///< return true if active weapon's bullet spray has become large and inaccurate
	bool IsUsingScope( void );									///< return true if looking thru weapon's scope

	//------------------------------------------------------------------------------------
	// Event hooks
	//

	/// invoked when injured by something (EXTEND) - returns the amount of damage inflicted
	 int OnTakeDamage_Impl( const CTakeDamageInfo &info )
	{
		return PlayerType::OnTakeDamage_Impl( info );
	}

	/// invoked when killed (EXTEND)
	 void Event_Killed_Impl( const CTakeDamageInfo &info )
	{ 
		PlayerType::Event_Killed_Impl( info );
	}

	bool IsEnemy( CBaseEntity *ent ) const;						///< returns true if given entity is our enemy
	int GetEnemiesRemaining( void ) const;						///< return number of enemies left alive
	int GetFriendsRemaining( void ) const;						///< return number of friends left alive

	bool IsPlayerFacingMe( CBasePlayer *enemy ) const;			///< return true if player is facing towards us
	bool IsPlayerLookingAtMe( CBasePlayer *enemy, float cosTolerance = 0.9f ) const;		///< returns true if other player is pointing right at us
	bool IsLookingAtPosition( const Vector &pos, float angleTolerance = 20.0f ) const;	///< returns true if looking (roughly) at given position

	bool IsLocalPlayerWatchingMe( void ) const;					///< return true if local player is observing this bot

	void PrintIfWatched( const char *format, ... ) const;				///< output message to console if we are being watched by the local player

	void UpdatePlayer( void );							///< update player physics, movement, weapon firing commands, etc
	void BuildUserCmd( CUserCmd& cmd, const QAngle& viewangles, float forwardmove, float sidemove, float upmove, int buttons, byte impulse );
	void AvoidPlayers( CUserCmd *pCmd ) { }				///< some game types allow players to pass through each other, this method pushes them apart
	 void SetModel_Impl( const char *modelName );

	 void Think_Impl( void ) { }

	const BotProfile *GetProfile( void ) const		{ return m_profile; }	///< return our personality profile

	virtual bool ClientCommand( const CCommand &args );			///< Do a "client command" - useful for invoking menu choices, etc.
	int Cmd_Argc( void );								///< Returns the number of tokens in the command string
	char *Cmd_Argv( int argc );							///< Retrieves a specified token

private:
	std::vector< char * > m_args;

protected:
	const BotProfile *m_profile;								///< the "personality" profile of this bot
	bool	m_bHasSpawned;

private:
	friend class CBotManager;

	unsigned int m_id;											///< unique bot ID

	CUserCmd m_userCmd;
	bool m_isRunning;											///< run/walk mode
	bool m_isCrouching;											///< true if crouching (ducking)
	float m_forwardSpeed;
	float m_strafeSpeed;
	float m_verticalSpeed;
	int m_buttonFlags;											///< bitfield of movement buttons

	float m_jumpTimestamp;										///< time when we last began a jump

	Vector m_viewForward;										///< forward view direction (only valid when GetViewVector() is used)

	/// the PostureContext represents the current settings of walking and crouching
	struct PostureContext
	{
		bool isRunning;
		bool isCrouching;
	};
	enum { MAX_POSTURE_STACK = 8 };
	PostureContext m_postureStack[ MAX_POSTURE_STACK ];
	int m_postureStackIndex;									///< index of top of stack

	void ResetCommand( void );
	//byte ThrottledMsec( void ) const;

protected:
	float GetMoveSpeed( void );									///< returns current movement speed (for walk/run)
};

//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------
//
// Inlines
//

//--------------------------------------------------------------------------------------------------------------
template < class T >
inline void CBot<T>::SetModel_Impl( const char *modelName )
{
	BaseClass::SetModel_Impl( modelName );
}

//-----------------------------------------------------------------------------------------------------------
template < class T >
inline float CBot<T>::GetMoveSpeed( void )
{
	// dgoodenough - Fix GCC / MSVC difference
	// PS3_BUILDFIX
	// For reasons unknown, GCC requires an explicit this-> to be able to find this function, while MSVC doesn't.
	return this->MaxSpeed();
}

//-----------------------------------------------------------------------------------------------------------
template < class T >
inline void CBot<T>::Run( void )
{
	m_isRunning = true;
}

//-----------------------------------------------------------------------------------------------------------
template < class T >
inline void CBot<T>::Walk( void )
{
	m_isRunning = false;
}

//-----------------------------------------------------------------------------------------------------------
template < class T >
inline bool CBot<T>::IsActiveWeaponRecoilHigh( void ) const
{
	const QAngle &angles = const_cast< CBot<T> * >( this )->GetAimPunchAngle();
	const float highRecoil = -1.5f;
	return (angles.x < highRecoil);
}

//-----------------------------------------------------------------------------------------------------------
template < class T >
inline void CBot<T>::PushPostureContext( void )
{
	if (m_postureStackIndex == MAX_POSTURE_STACK)
	{
		PrintIfWatched( "PushPostureContext() overflow error!\n" );
		return;
	}

	m_postureStack[ m_postureStackIndex ].isRunning = m_isRunning;
	m_postureStack[ m_postureStackIndex ].isCrouching = m_isCrouching;
	++m_postureStackIndex;
}

//-----------------------------------------------------------------------------------------------------------
template < class T >
inline void CBot<T>::PopPostureContext( void )
{
	if (m_postureStackIndex == 0)
	{
		PrintIfWatched( "PopPostureContext() underflow error!\n" );
		m_isRunning = true;
		m_isCrouching = false;
		return;
	}

	--m_postureStackIndex;
	m_isRunning = m_postureStack[ m_postureStackIndex ].isRunning;
	m_isCrouching = m_postureStack[ m_postureStackIndex ].isCrouching;
}

//-----------------------------------------------------------------------------------------------------------
template < class T >
inline bool CBot<T>::IsPlayerFacingMe( CBasePlayer *other ) const
{
	// dgoodenough - Fix GCC / MSVC difference
	// PS3_BUILDFIX
	// For reasons unknown, GCC requires an explicit this-> to be able to find this function, while MSVC doesn't.
	Vector toOther = other->GetAbsOrigin() - this->GetAbsOrigin();

	Vector otherForward;
	AngleVectors( other->EyeAngles() + other->GetViewPunchAngle(), &otherForward );

	if (DotProduct( otherForward, toOther ) < 0.0f)
		return true;

	return false;
}

//-----------------------------------------------------------------------------------------------------------
template < class T >
inline bool CBot<T>::IsPlayerLookingAtMe( CBasePlayer *other, float cosTolerance ) const
{
	// dgoodenough - Fix GCC / MSVC difference
	// PS3_BUILDFIX
	// For reasons unknown, GCC requires an explicit this-> to be able to find this function, while MSVC doesn't.
	Vector toOther = other->GetAbsOrigin() - this->GetAbsOrigin();

	toOther.NormalizeInPlace();

	Vector otherForward;
	AngleVectors( other->EyeAngles() + other->GetViewPunchAngle(), &otherForward );

	// other player must be pointing nearly right at us to be "looking at" us
	if (DotProduct( otherForward, toOther ) < -cosTolerance)
		return true;

	return false;
}

//-----------------------------------------------------------------------------------------------------------
template < class T >
inline const Vector &CBot<T>::GetViewVector( void )
{
	// dgoodenough - Fix GCC / MSVC difference
	// PS3_BUILDFIX
	// For reasons unknown, GCC requires an explicit this-> to be able to find this function, while MSVC doesn't.
	AngleVectors( this->EyeAngles() + this->GetViewPunchAngle(), &m_viewForward );
	return m_viewForward;
}

//-----------------------------------------------------------------------------------------------------------
template < class T >
inline bool CBot<T>::IsLookingAtPosition( const Vector &pos, float angleTolerance ) const
{
	// forced to do this since many methods in CBaseEntity are not const, but should be
	CBot< T > *me = const_cast< CBot< T > * >( this );

	Vector to = pos - me->EyePosition();

	QAngle idealAngles;
	VectorAngles( to, idealAngles );

	QAngle viewAngles = me->EyeAngles();

	float deltaYaw = AngleNormalize( idealAngles.y - viewAngles.y );
	float deltaPitch = AngleNormalize( idealAngles.x - viewAngles.x );

	if (fabs( deltaYaw ) < angleTolerance && abs( deltaPitch ) < angleTolerance)
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
template < class PlayerType >
inline CBot< PlayerType >::CBot( void )
{
	// the profile will be attached after this instance is constructed
	m_profile = nullptr;

	// assign this bot a unique ID
	static unsigned int nextID = 1;

	// wraparound (highly unlikely)
	if (nextID == 0)
		++nextID;

	m_id = nextID;
	++nextID;

	m_postureStackIndex = 0;
}

//--------------------------------------------------------------------------------------------------------------
template < class PlayerType >
inline CBot< PlayerType >::~CBot( void )
{
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Prepare bot for action
 */
template < class PlayerType >
inline bool CBot< PlayerType >::Initialize( const BotProfile *profile, int team )
{
	m_profile = profile;
	return true;
}

//--------------------------------------------------------------------------------------------------------------
template < class PlayerType >
inline void CBot< PlayerType >::Spawn_Impl( void )
{
	// initialize the bot (thus setting its profile)
	if (m_profile == nullptr)
		static_cast< CCSBot * >( this )->Initialize( g_botInitProfile, g_botInitTeam );

	// let the base class set some things up
	PlayerType::Spawn_Impl();

	// Make sure everyone knows we are a bot
	// dgoodenough - Fix GCC / MSVC difference
	// PS3_BUILDFIX
	// For reasons unknown, GCC requires an explicit this-> to be able to find this function, while MSVC doesn't.
	// I probably don't need to have the two separate statements, prepending "this->" *ought* to be harmless and benign.
	// However my paranoia and conservatism got the better of me.
	this->AddFlag( FL_CLIENT | FL_FAKECLIENT );

	// Bots use their own thinking mechanism
	SetThink( nullptr );

	m_isRunning = true;
	m_isCrouching = false;
	m_postureStackIndex = 0;

	m_jumpTimestamp = 0.0f;

	// Command interface variable initialization
	ResetCommand();
}

/*
//--------------------------------------------------------------------------------------------------------------
template < class PlayerType >
inline void CBot< PlayerType >::BotThink( void )
{
float g_flBotFullThinkInterval	= 1.0 / 15.0;	// full AI at lower frequency (was 10 in GoldSrc)
	
	Upkeep();

	if (gpGlobals->curtime >= m_flNextFullBotThink)
	{
		m_flNextFullBotThink = gpGlobals->curtime + g_flBotFullThinkInterval;

		ResetCommand();
		Update();
	}

	UpdatePlayer();
}
*/

//--------------------------------------------------------------------------------------------------------------
template < class PlayerType >
inline void CBot< PlayerType >::MoveForward( void )
{
	m_forwardSpeed = static_cast< CCSBot * >( this )->GetMoveSpeed();
	SETBITS( m_buttonFlags, IN_FORWARD );

	// make mutually exclusive
	CLEARBITS( m_buttonFlags, IN_BACK );
}

//--------------------------------------------------------------------------------------------------------------
template < class PlayerType >
inline void CBot< PlayerType >::MoveBackward( void )
{
	m_forwardSpeed = -static_cast< CCSBot * >( this )->GetMoveSpeed();
	SETBITS( m_buttonFlags, IN_BACK );

	// make mutually exclusive
	CLEARBITS( m_buttonFlags, IN_FORWARD );
}

//--------------------------------------------------------------------------------------------------------------
template < class PlayerType >
inline void CBot< PlayerType >::StrafeLeft( void )
{
	m_strafeSpeed = -static_cast< CCSBot * >( this )->GetMoveSpeed();
	SETBITS( m_buttonFlags, IN_MOVELEFT );

	// make mutually exclusive
	CLEARBITS( m_buttonFlags, IN_MOVERIGHT );
}

//--------------------------------------------------------------------------------------------------------------
template < class PlayerType >
inline void CBot< PlayerType >::StrafeRight( void )
{
	m_strafeSpeed = static_cast< CCSBot * >( this )->GetMoveSpeed();
	SETBITS( m_buttonFlags, IN_MOVERIGHT );

	// make mutually exclusive
	CLEARBITS( m_buttonFlags, IN_MOVELEFT );
}

//--------------------------------------------------------------------------------------------------------------
template < class PlayerType >
inline bool CBot< PlayerType >::Jump( bool mustJump )
{
	if (IsJumping() || IsCrouching())
		return false;

	if (!mustJump)
	{
		const float minJumpInterval = 0.9f; // 1.5f;
		if (gpGlobals->curtime - m_jumpTimestamp < minJumpInterval)
			return false;
	}

	// still need sanity check for jumping frequency
	const float sanityInterval = 0.3f;
	if (gpGlobals->curtime - m_jumpTimestamp < sanityInterval)
		return false;

	// jump
	SETBITS( m_buttonFlags, IN_JUMP );
	m_jumpTimestamp = gpGlobals->curtime;
	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Zero any MoveForward(), Jump(), etc
 */
template < class PlayerType >
void CBot< PlayerType >::ClearMovement( void )
{
	m_forwardSpeed = 0.0;
	m_strafeSpeed = 0.0;
	m_verticalSpeed	= 100.0; // stay at the top of water, so we don't drown.  TODO: swim logic
	m_buttonFlags &= ~(IN_FORWARD | IN_BACK | IN_LEFT | IN_RIGHT | IN_JUMP);
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if we are in the midst of a jump
 */
template < class PlayerType >
inline bool CBot< PlayerType >::IsJumping( void )
{
	// if long time after last jump, we can't be jumping
	if (gpGlobals->curtime - m_jumpTimestamp > 3.0f)
		return false;

	// if we just jumped, we're still jumping
	if (gpGlobals->curtime - m_jumpTimestamp < 0.9f) // 1.0f
		return true;

	// a little after our jump, we're jumping until we hit the ground
	// dgoodenough - Fix GCC / MSVC difference
	// PS3_BUILDFIX
	// For reasons unknown, GCC requires an explicit this-> to be able to find this function, while MSVC doesn't.
	if (FBitSet( this->GetFlags(), FL_ONGROUND ))
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
template < class PlayerType >
inline void CBot< PlayerType >::Crouch( void )
{
	m_isCrouching = true;
}

//--------------------------------------------------------------------------------------------------------------
template < class PlayerType >
inline void CBot< PlayerType >::StandUp( void )
{
	m_isCrouching = false;
}

//--------------------------------------------------------------------------------------------------------------
template < class PlayerType >
inline void CBot< PlayerType >::UseEnvironment( void )
{
	SETBITS( m_buttonFlags, IN_USE );
}

//--------------------------------------------------------------------------------------------------------------
template < class PlayerType >
inline void CBot< PlayerType >::PrimaryAttack( void )
{
	SETBITS( m_buttonFlags, IN_ATTACK );
}

//--------------------------------------------------------------------------------------------------------------
template < class PlayerType >
inline void CBot< PlayerType >::ClearPrimaryAttack( void )
{
	CLEARBITS( m_buttonFlags, IN_ATTACK );
}

//--------------------------------------------------------------------------------------------------------------
template < class PlayerType >
inline void CBot< PlayerType >::TogglePrimaryAttack( void )
{
	if (FBitSet( m_buttonFlags, IN_ATTACK ))
	{
		CLEARBITS( m_buttonFlags, IN_ATTACK );
	}
	else
	{
		SETBITS( m_buttonFlags, IN_ATTACK );
	}
}

//--------------------------------------------------------------------------------------------------------------
template < class PlayerType >
inline void CBot< PlayerType >::SecondaryAttack( void )
{
	SETBITS( m_buttonFlags, IN_ATTACK2 );
}

//--------------------------------------------------------------------------------------------------------------
template < class PlayerType >
inline void CBot< PlayerType >::Reload( void )
{
	SETBITS( m_buttonFlags, IN_RELOAD );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns ratio of ammo left to max ammo (1 = full clip, 0 = empty)
 */
template < class PlayerType >
inline float CBot< PlayerType >::GetActiveWeaponAmmoRatio( void ) const
{
	// dgoodenough - Fix GCC / MSVC difference
	// PS3_BUILDFIX
	// For reasons unknown, GCC requires an explicit this-> to be able to find this function, while MSVC doesn't.
	CWeaponCSBase *weapon = this->GetActiveCSWeapon();

	if (weapon == nullptr)
		return 0.0f;

	// weapons with no ammo are always full
	if (weapon->Clip1() < 0)
		return 1.0f;

	return (float)weapon->Clip1() / (float)weapon->GetMaxClip1();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if active weapon has an empty clip
 */
template < class PlayerType >
inline bool CBot< PlayerType >::IsActiveWeaponClipEmpty( void ) const
{
	// dgoodenough - Fix GCC / MSVC difference
	// PS3_BUILDFIX
	// For reasons unknown, GCC requires an explicit this-> to be able to find this function, while MSVC doesn't.
	CWeaponCSBase *gun = this->GetActiveCSWeapon();

	if (gun && gun->Clip1() == 0)
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if active weapon has no ammo at all
 */
template < class PlayerType >
inline bool CBot< PlayerType >::IsActiveWeaponOutOfAmmo( void ) const
{
	// dgoodenough - Fix GCC / MSVC difference
	// PS3_BUILDFIX
	// For reasons unknown, GCC requires an explicit this-> to be able to find this function, while MSVC doesn't.
	CWeaponCSBase *weapon = this->GetActiveCSWeapon();
	if (weapon == nullptr)
		return true;

	return !weapon->HasAnyAmmo();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if looking thru weapon's scope
 */
template < class PlayerType >
inline bool CBot< PlayerType >::IsUsingScope( void )
{
	// if our field of view is less than 90, we're looking thru a scope (maybe only true for CS...)
	// dgoodenough - Fix GCC / MSVC difference
	// PS3_BUILDFIX
	// For reasons unknown, GCC requires an explicit this-> to be able to find this function, while MSVC doesn't.
	if (this->GetFOV() < this->GetDefaultFOV())
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Fill in a CUserCmd with our data
 */
template < class PlayerType >
inline void CBot< PlayerType >::BuildUserCmd( CUserCmd& cmd, const QAngle& viewangles, float forwardmove, float sidemove, float upmove, int buttons, byte impulse )
{
	cmd.Reset();
	cmd.command_number = gpGlobals->tickcount;
	cmd.forwardmove = forwardmove;
	cmd.sidemove = sidemove;
	cmd.upmove = upmove;
	cmd.buttons = buttons;
	cmd.impulse = impulse;

	VectorCopy( viewangles, cmd.viewangles );
	cmd.random_seed = random_valve->RandomInt( 0, 0x7fffffff );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Update player physics, movement, weapon firing commands, etc
 */
template < class PlayerType >
inline void CBot< PlayerType >::UpdatePlayer( void )
{
	if (m_isCrouching)
	{
		SETBITS( m_buttonFlags, IN_DUCK );
	}
	else if (!m_isRunning)
	{
		SETBITS( m_buttonFlags, IN_SPEED );
	}

	// dgoodenough - Fix GCC / MSVC difference
	// PS3_BUILDFIX
	// For reasons unknown, GCC requires an explicit this-> to be able to find this function, while MSVC doesn't.
	if ( this->IsEFlagSet(EFL_BOT_FROZEN) )
	{
		m_buttonFlags = 0; // Freeze.
		m_forwardSpeed = 0;
		m_strafeSpeed = 0;
		m_verticalSpeed = 0;
	}

	// Fill in a CUserCmd with our data
	// dgoodenough - Fix GCC / MSVC difference
	// PS3_BUILDFIX
	// For reasons unknown, GCC requires an explicit this-> to be able to find this function, while MSVC doesn't.
	static_cast< CCSBot * >( this )->BuildUserCmd( m_userCmd, this->EyeAngles(), m_forwardSpeed, m_strafeSpeed, m_verticalSpeed, m_buttonFlags, 0 );

	static_cast< CCSBot * >( this )->AvoidPlayers( &m_userCmd );

	// Save off the CUserCmd to execute later
	// dgoodenough - Fix GCC / MSVC difference
	// PS3_BUILDFIX
	// For reasons unknown, GCC requires an explicit this-> to be able to find this function, while MSVC doesn't.
	this->ProcessUsercmds( &m_userCmd, 1, 1, 0, false );
}

//--------------------------------------------------------------------------------------------------------------
template < class PlayerType >
inline void CBot< PlayerType >::ResetCommand( void )
{
	m_forwardSpeed = 0.0;
	m_strafeSpeed = 0.0;
	m_verticalSpeed	= 100.0; // stay at the top of water, so we don't drown.  TODO: swim logic
	m_buttonFlags = 0;
}

//--------------------------------------------------------------------------------------------------------------
/*
template < class PlayerType >
inline byte CBot< PlayerType >::ThrottledMsec( void ) const
{
	int iNewMsec;

	// Estimate Msec to use for this command based on time passed from the previous command
	iNewMsec = (int)( (gpGlobals->curtime - m_flPreviousCommandTime) * 1000 );
	if (iNewMsec > 255)		// Doh, bots are going to be slower than they should if this happens.
		iNewMsec = 255;		// Upgrade that CPU or use less bots!

	return (byte)iNewMsec;
}
*/

//--------------------------------------------------------------------------------------------------------------
/**
 * Do a "client command" - useful for invoking menu choices, etc.
 */
template < class PlayerType >
inline bool CBot< PlayerType >::ClientCommand( const CCommand &args )
{
	// Remove old args
	int i;
	for ( i=0; i<std::ssize( m_args ); ++i )
	{
		delete[] m_args[i];
	}
	m_args.clear();

	// parse individual args
	const char *cmd = args.GetCommandString();
	while (1)
	{
		// skip whitespace up to a /n
		while (*cmd && *cmd <= ' ' && *cmd != '\n')
		{
			cmd++;
		}
		
		if (*cmd == '\n')
		{	// a newline seperates commands in the buffer
			cmd++;
			break;
		}

		if (!*cmd)
			break;
	
		cmd = SharedParse (cmd);
		if (!cmd)
			break;

		m_args.push_back( CloneString( SharedGetToken() ) );
	}

	// and pass to the base class
	return PlayerType::ClientCommand( args );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns the number of tokens in the command string
 */
template < class PlayerType >
inline int CBot< PlayerType >::Cmd_Argc()
{
	return std::ssize( m_args );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Retrieves a specified token
 */
template < class PlayerType >
inline char * CBot< PlayerType >::Cmd_Argv( int argc )
{
	if ( argc < 0 || argc >= std::ssize( m_args ) )
		return nullptr;
	return m_args[argc];
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns TRUE if given entity is our enemy
 */
template < class PlayerType >
inline bool CBot< PlayerType >::IsEnemy( CBaseEntity *ent ) const
{
	// only Players (real and AI) can be enemies
	if (!ent->IsPlayer())
		return false;

	// corpses are no threat
	if (!ent->IsAlive())
		return false;	

	CBasePlayer *player = static_cast<CBasePlayer *>( ent );

	// if they are on our team, they are our friends
	// dgoodenough - Fix GCC / MSVC difference
	// PS3_BUILDFIX
	// For reasons unknown, GCC requires an explicit this-> to be able to find this function, while MSVC doesn't.
	if (player->GetTeamNumber() == this->GetTeamNumber())
		return false;

	// yep, we hate 'em
	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return number of enemies left alive
 */
template < class PlayerType >
inline int CBot< PlayerType >::GetEnemiesRemaining( void ) const
{
	int count = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBaseEntity *player = UTIL_PlayerByIndex( i );

		if (player == nullptr)
			continue;

		if (!IsEnemy( player ))
			continue;

		if (!player->IsAlive())
			continue;

		count++;
	}

	return count;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return number of friends left alive
 */
template < class PlayerType >
inline int CBot< PlayerType >::GetFriendsRemaining( void ) const
{
	int count = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBaseEntity *player = UTIL_PlayerByIndex( i );

		if (player == nullptr)
			continue;

		if (IsEnemy( player ))
			continue;

		if (!player->IsAlive())
			continue;

		if (player == static_cast<CBaseEntity *>( const_cast<CBot *>( this ) ))
			continue;

		count++;
	}

	return count;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if the local player is currently in observer mode watching this bot.
 */
template < class PlayerType >
inline bool CBot< PlayerType >::IsLocalPlayerWatchingMe( void ) const
{
	if ( engine->IsDedicatedServer() )
		return false;

	CBasePlayer *player = UTIL_GetListenServerHost();
	if ( player == nullptr )
		return false;

	if ( cv_bot_debug_target.GetInt() > 0 )
	{
		// dgoodenough - Fix GCC / MSVC difference
		// PS3_BUILDFIX
		// For reasons unknown, GCC requires an explicit this-> to be able to find this function, while MSVC doesn't.
		return this->entindex() == cv_bot_debug_target.GetInt();
	}

	if ( player->IsObserver() || !player->IsAlive() )
	{
		if ( const_cast< CBot< PlayerType > * >(this) == player->GetObserverTarget() )
		{
			switch( player->GetObserverMode() )
			{
				case OBS_MODE_IN_EYE:
				case OBS_MODE_CHASE:
					return true;
			}
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Output message to console if we are being watched by the local player
 */
template < class PlayerType >
inline void CBot< PlayerType >::PrintIfWatched( const char *format, ... ) const
{
	if (cv_bot_debug.GetInt() == 0)
	{
		return;
	}

	if ((IsLocalPlayerWatchingMe() && (cv_bot_debug.GetInt() == 1 || cv_bot_debug.GetInt() == 3)) ||
		(cv_bot_debug.GetInt() == 2 || cv_bot_debug.GetInt() == 4))
	{
		std::va_list varg;
		char buffer[ CBotManager::MAX_DBG_MSG_SIZE ];
		const char *name = const_cast< CBot< PlayerType > * >( this )->GetPlayerName();

		__builtin_va_start( varg, format );
		std::vsprintf( buffer, format, varg );
		__builtin_va_end( varg );

		// prefix the console message with the bot's name (this can be NULL if bot was just added)
		if ( !engine->IsDedicatedServer() )
		{
			ClientPrint( UTIL_GetListenServerHost(),
				HUD_PRINTCONSOLE,
				UTIL_VarArgs( "%s: %s",
				( name ) ? name : "(NULL netname)", buffer ) );
		}

		TheBots->AddDebugMessage( buffer );
	}
}

//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------

extern void InstallBotControl( void );
extern void RemoveBotControl( void );
extern void Bot_ServerCommand( void );
extern void Bot_RegisterCvars( void );

extern bool IsSpotOccupied( CBaseEntity *me, const Vector &pos );	// if a player is at the given spot, return true
extern const Vector *FindNearbyHidingSpot( CBaseEntity *me, const Vector &pos, float maxRange = 1000.0f, bool isSniper = false, bool useNearest = false );
extern const Vector *FindRandomHidingSpot( CBaseEntity *me, Place place, bool isSniper = false );
extern const Vector *FindNearbyRetreatSpot( CBaseEntity *me, const Vector &start, float maxRange = 1000.0f, int avoidTeam = 0 );

class CNavArea;
class CNavLadder;

using namespace ::ks::reflect;


// improv_locomotor.h
// Interface for moving Improvs along computed paths
// Author: Michael Booth, July 2004

// TODO: Remove duplicate methods from CImprov, and update CImprov to use this class

/**
 * A locomotor owns the movement of an Improv.
 * Each method forwards to the concrete class (CHostage or CChicken) according to the Kind
 * (see hostage/cs_simple_hostage.cpp).
 */
class CImprovLocomotor
{
public:
	enum class Kind
	{
		Hostage,	///< CHostage
		Chicken,	///< CChicken
	};

	enum MoveToFailureType
	{
		FAIL_INVALID_PATH,
		FAIL_STUCK,
		FAIL_FELL_OFF,
	};

	const Vector & GetCentroid( void ) const;		
	const Vector & GetFeet( void ) const;		///< return position of "feet" - point below centroid of improv at feet level
	const Vector & GetEyes( void ) const;		
	float GetMoveAngle( void ) const;		///< return direction of movement
	CNavArea * GetLastKnownArea( void ) const;		
	bool GetSimpleGroundHeightWithFloor( const Vector &pos, float *height, Vector *normal = nullptr );		///< find "simple" ground height, treating current nav area as part of the floor
	void Crouch( void );		
	void StandUp( void );		///< "un-crouch"
	bool IsCrouching( void ) const;		
	void Jump( void );		///< initiate a jump
	bool IsJumping( void ) const;		
	void Run( void );		///< set movement speed to running
	void Walk( void );		///< set movement speed to walking
	bool IsRunning( void ) const;		
	void StartLadder( const CNavLadder *ladder, NavTraverseType how, const Vector &approachPos, const Vector &departPos );		///< invoked when a ladder is encountered while following a path
	bool TraverseLadder( const CNavLadder *ladder, NavTraverseType how, const Vector &approachPos, const Vector &departPos, float deltaT );		///< traverse given ladder
	bool IsUsingLadder( void ) const;		
	void TrackPath( const Vector &pathGoal, float deltaT );		///< move along path by following "pathGoal"
	void OnMoveToSuccess( const Vector &goal );		///< invoked when an improv reaches its MoveTo goal
	void OnMoveToFailure( const Vector &goal, MoveToFailureType reason );		///< invoked when an improv fails to reach a MoveTo goal

protected:
	explicit CImprovLocomotor( Kind kind ) : m_improvKind( kind ) { }

private:
	Kind m_improvKind;
};
