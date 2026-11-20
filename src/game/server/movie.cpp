// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

module;
#include <stdlib.h>
#include "eiface.h"
#include "networkvar.h"
#include "reflect.h"
#include "responserules/response_types.h"
#include "server_class.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "baseentity.h"
#include "enginecallback.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "util.h"
#include "gametypes.h"
#include "igameevents.h"
#include "gamerules_register.h"
module ks.server;

import ks.core.basetypes;
import ks.core.dbg;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.math.vmatrix;
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import ks.tier1.strtools;
import :fish;
import :sharedvar;
import :vscript;
import :baseentity;
import :player;
import :entity_dispatch;
import :entity_kind;
import :nav_4;
import :entity_2;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import std;
import ks.tier1.createinterface;
import ks.core.cmdline;
import ks.core.platform;
import ks.core.random;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.keyvalues;
import ks.tier1.registry;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player;

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Allows movies to be played as a VGUI screen in the world

class CMovieDisplay : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_MovieDisplay" } ]]
	static constexpr bool ks_class = true;

public:

	DECLARE_CLASS( CMovieDisplay, CBaseEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CMovieDisplay()
		: m_bForcePrecache( false )
	{
	}

	virtual ~CMovieDisplay();

	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );

	int  UpdateTransmitState_Impl();
	void SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways );

	void Spawn_Impl( void );
	void Precache_Impl( void );
	void OnRestore_Impl( void );

	void	ScreenVisible( bool bVisible );

	void	Disable( void );
	void	Enable( void );

	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "SetDisplayText", .type = FIELD_STRING } ]] void	InputSetDisplayText( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TakeOverAsMaster", .type = FIELD_VOID } ]] void	InputTakeOverAsMaster( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "SetMovie", .type = FIELD_STRING } ]] void	InputSetMovie( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "SetUseCustomUVs", .type = FIELD_BOOLEAN } ]] void	InputSetUseCustomUVs( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetUMin", .type = FIELD_FLOAT } ]] void	InputSetUMin( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetVMin", .type = FIELD_FLOAT } ]] void	InputSetVMin( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetUMax", .type = FIELD_FLOAT } ]] void	InputSetUMax( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetVMax", .type = FIELD_FLOAT } ]] void	InputSetVMax( inputdata_t &inputdata );	

private:

	// Control panel
	void GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	void GetControlPanelClassName( int nPanelIndex, const char *&pPanelName );
	void SpawnControlPanels( void );
	void RestoreControlPanels( void );

private:
	CNetworkVar( bool, m_bEnabled, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bLooping, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "looping" } ]] );
	CNetworkVar( bool, m_bStretchToFill, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "stretch" } ]] );
	CNetworkVar( bool, m_bForcedSlave, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "forcedslave" } ]] );
	[[= ks::reflect::Key{ .name = "forceprecache" } ]] bool m_bForcePrecache;

	CNetworkVar( bool, m_bUseCustomUVs, [[= ks::reflect::Net{} ]] );
	CNetworkVar( float, m_flUMin, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkVar( float, m_flUMax, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkVar( float, m_flVMin, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkVar( float, m_flVMax, [[= ks::reflect::Net{ .bits = 32 } ]] );

	CNetworkString( m_szDisplayText, 128 );

	// Filename of the movie to play
	CNetworkString( m_szMovieFilename, 128, [[= ks::reflect::Net{} ]] );
	[[= ks::reflect::Key{ .name = "moviefilename" } ]] string_t	m_strMovieFilename;

	// "Group" name.  Screens of the same group name will play the same movie at the same time
	// Effectively this lets multiple screens tune to the same "channel" in the world
	CNetworkString( m_szGroupName, 128, [[= ks::reflect::Net{} ]] );
	[[= ks::reflect::Key{ .name = "groupname" } ]] string_t	m_strGroupName;

	[[= ks::reflect::Key{ .name = "width" } ]] int			m_iScreenWidth;
	[[= ks::reflect::Key{ .name = "height" } ]] int			m_iScreenHeight;

	bool		m_bDoFullTransmit;
};

LINK_ENTITY_TO_CLASS( vgui_movie_display, CMovieDisplay );

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CMovieDisplay )

IMPLEMENT_REFLECT_SERVERCLASS( CMovieDisplay, DT_MovieDisplay )

CMovieDisplay::~CMovieDisplay()
{
}

//-----------------------------------------------------------------------------
// Read in Hammer data
//-----------------------------------------------------------------------------
bool CMovieDisplay::KeyValue_Impl( const char *szKeyName, const char *szValue ) 
{
	// NOTE: Have to do these separate because they set two values instead of one
	if( FStrEq( szKeyName, "angles" ) )
	{
		QAngle angles;
		UTIL_StringToVector( angles.Base(), szValue );

		// Because the vgui screen basis is strange (z is front, y is up, x is right)
		// we need to rotate the typical basis before applying it
		VMatrix mat, rotation, tmp;
		MatrixFromAngles( angles, mat );
		MatrixBuildRotationAboutAxis( rotation, Vector( 0, 1, 0 ), 90 );
		MatrixMultiply( mat, rotation, tmp );
		MatrixBuildRotateZ( rotation, 90 );
		MatrixMultiply( tmp, rotation, mat );
		MatrixToAngles( mat, angles );
		SetAbsAngles( angles );

		return true;
	}

	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int CMovieDisplay::UpdateTransmitState_Impl()
{
	if ( m_bDoFullTransmit )
	{
		m_bDoFullTransmit = false;
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	return SetTransmitState( FL_EDICT_FULLCHECK );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Are we already marked for transmission?
	if ( (*pInfo->m_pTransmitEdict)[ entindex() ] )
		return;

	BaseClass::SetTransmit_Impl( pInfo, bAlways );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::Spawn_Impl( void )
{
	// Move the strings into a networkable form
	V_strcpy( m_szMovieFilename.GetForModify(), m_strMovieFilename.ToCStr() );
	V_strcpy( m_szGroupName.GetForModify(), m_strGroupName.ToCStr() );

	Precache();

	BaseClass::Spawn_Impl();

	m_bEnabled = false;

	SpawnControlPanels();

	ScreenVisible( m_bEnabled );

	m_bDoFullTransmit = true;

	m_bUseCustomUVs = false;
	m_flUMin = 0;
	m_flUMax = 1;
	m_flVMin = 0;
	m_flVMax = 1;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::Precache_Impl( void )
{
	BaseClass::Precache_Impl();

	if ( m_bForcePrecache )
	{
		DevMsg( "Precaching vgui_movie_display %s with movie %s\n", m_iName->ToCStr(), m_szMovieFilename.Get() );
		PrecacheMovie( m_szMovieFilename );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::OnRestore_Impl( void )
{
	BaseClass::OnRestore_Impl();

	m_bDoFullTransmit = true;

	RestoreControlPanels();

	ScreenVisible( m_bEnabled );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::ScreenVisible( bool bVisible )
{
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::Disable( void )
{
	if ( !m_bEnabled )
		return;

	m_bEnabled = false;

	ScreenVisible( false );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::Enable( void )
{
	if ( m_bEnabled )
		return;

	m_bEnabled = true;

	ScreenVisible( true );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputDisable( inputdata_t &inputdata )
{
	Disable();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputEnable( inputdata_t &inputdata )
{
	Enable();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputSetUseCustomUVs( inputdata_t &inputdata )
{
	m_bUseCustomUVs = inputdata.value.Bool();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputSetUMin( inputdata_t &inputdata )
{
	m_flUMin = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputSetUMax( inputdata_t &inputdata )
{
	m_flUMax = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputSetVMin( inputdata_t &inputdata )
{
	m_flVMin = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputSetVMax( inputdata_t &inputdata )
{
	m_flVMax = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputTakeOverAsMaster( inputdata_t &inputdata )
{
	Enable();

	EntityMessageBegin( this );
		WRITE_BYTE( 0 );
	MessageEnd();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputSetDisplayText( inputdata_t &inputdata )
{
	V_strcpy( m_szDisplayText.GetForModify(), inputdata.value.String() );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputSetMovie( inputdata_t &inputdata )
{
	V_strncpy( m_szMovieFilename.GetForModify(), inputdata.value.String(), 128 );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "movie_display_screen";
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::GetControlPanelClassName( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "vgui_screen";
}

//-----------------------------------------------------------------------------
// This is called by the base object when it's time to spawn the control panels
//-----------------------------------------------------------------------------
void CMovieDisplay::SpawnControlPanels()
{
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::RestoreControlPanels( void )
{
}

class CBaseEntity;

using namespace ::ks::reflect;

inline constexpr const char *MOVIEEXPLOSION_ENTITYNAME = "env_movieexplosion";

IMPLEMENT_REFLECT_SERVERCLASS( MovieExplosion, DT_MovieExplosion )

LINK_ENTITY_TO_CLASS(env_movieexplosion, MovieExplosion);

MovieExplosion* MovieExplosion::CreateMovieExplosion(const Vector &pos)
{
	CBaseEntity *pEnt = CreateEntityByName(MOVIEEXPLOSION_ENTITYNAME);
	if(pEnt)
	{
		MovieExplosion *pEffect = dynamic_cast<MovieExplosion*>(pEnt);
		if(pEffect && pEffect->edict())
		{
			pEffect->SetLocalOrigin( pos );
			pEffect->Activate();
			return pEffect;
		}
		else
		{
			UTIL_Remove(pEnt);
		}
	}

	return nullptr;
}

using namespace ::ks::reflect;

const char *g_pszMPConcepts[] =
{
	"TLK_FIREWEAPON",				// MP_CONCEPT_FIREWEAPON
	"TLK_HURT",						// MP_CONCEPT_HURT
	"TLK_PLAYER_EXPRESSION",		// MP_CONCEPT_PLAYER_EXPRESSION
	"TLK_WINDMINIGUN",				// MP_CONCEPT_WINDMINIGUN
	"TLK_FIREMINIGUN",				// MP_CONCEPT_FIREMINIGUN
	"TLK_PLAYER_MEDIC",				// MP_CONCEPT_PLAYER_MEDIC
	"TLK_DETONATED_OBJECT",			// MP_CONCEPT_DETONATED_OBJECT 
	"TLK_KILLED_PLAYER",			// MP_CONCEPT_KILLED_PLAYER
	"TLK_KILLED_OBJECT",			// MP_CONCEPT_KILLED_OBJECT
	"TLK_PLAYER_PAIN",				// MP_CONCEPT_PLAYER_PAIN
	"TLK_PLAYER_ATTACKER_PAIN",		// MP_CONCEPT_PLAYER_ATTACKER_PAIN
	"TLK_PLAYER_TAUNT",				// MP_CONCEPT_PLAYER_TAUNT
	"TLK_PLAYER_HELP",				// MP_CONCEPT_PLAYER_HELP
	"TLK_PLAYER_GO",				// MP_CONCEPT_PLAYER_GO
	"TLK_PLAYER_MOVEUP",			// MP_CONCEPT_PLAYER_MOVEUP
	"TLK_PLAYER_LEFT",				// MP_CONCEPT_PLAYER_LEFT
	"TLK_PLAYER_RIGHT",				// MP_CONCEPT_PLAYER_RIGHT
	"TLK_PLAYER_YES",				// MP_CONCEPT_PLAYER_YES
	"TLK_PLAYER_NO",				// MP_CONCEPT_PLAYER_NO
	"TLK_PLAYER_INCOMING",			// MP_CONCEPT_PLAYER_INCOMING
	"TLK_PLAYER_CLOAKEDSPY",		// MP_CONCEPT_PLAYER_CLOAKEDSPY
	"TLK_PLAYER_SENTRYAHEAD",		// MP_CONCEPT_PLAYER_SENTRYAHEAD
	"TLK_PLAYER_TELEPORTERHERE",	// MP_CONCEPT_PLAYER_TELEPORTERHERE
	"TLK_PLAYER_DISPENSERHERE",		// MP_CONCEPT_PLAYER_DISPENSERHERE
	"TLK_PLAYER_SENTRYHERE",		// MP_CONCEPT_PLAYER_SENTRYHERE
	"TLK_PLAYER_ACTIVATECHARGE",	// MP_CONCEPT_PLAYER_ACTIVATECHARGE
	"TLK_PLAYER_CHARGEREADY",		// MP_CONCEPT_PLAYER_CHARGEREADY
	"TLK_PLAYER_TAUNTS"	,			// MP_CONCEPT_PLAYER_TAUNTS
	"TLK_PLAYER_BATTLECRY",			// MP_CONCEPT_PLAYER_BATTLECRY
	"TLK_PLAYER_CHEERS",			// MP_CONCEPT_PLAYER_CHEERS
	"TLK_PLAYER_JEERS",				// MP_CONCEPT_PLAYER_JEERS
	"TLK_PLAYER_POSITIVE",			// MP_CONCEPT_PLAYER_POSITIVE
	"TLK_PLAYER_NEGATIVE",			// MP_CONCEPT_PLAYER_NEGATIVE
	"TLK_PLAYER_NICESHOT",			// MP_CONCEPT_PLAYER_NICESHOT
	"TLK_PLAYER_GOODJOB",			// MP_CONCEPT_PLAYER_GOODJOB
	"TLK_MEDIC_STARTEDHEALING",		// MP_CONCEPT_MEDIC_STARTEDHEALING
	"TLK_MEDIC_CHARGEREADY",		// MP_CONCEPT_MEDIC_CHARGEREADY
	"TLK_MEDIC_STOPPEDHEALING",		// MP_CONCEPT_MEDIC_STOPPEDHEALING
	"TLK_MEDIC_CHARGEDEPLOYED",		// MP_CONCEPT_MEDIC_CHARGEDEPLOYED
	"TLK_FLAGPICKUP",				// MP_CONCEPT_FLAGPICKUP
	"TLK_FLAGCAPTURED",				// MP_CONCEPT_FLAGCAPTURED
	"TLK_ROUND_START",				// MP_CONCEPT_ROUND_START
	"TLK_SUDDENDEATH_START",		// MP_CONCEPT_SUDDENDEATH_START
	"TLK_ONFIRE",					// MP_CONCEPT_ONFIRE
	"TLK_STALEMATE",				// MP_CONCEPT_STALEMATE
	"TLK_BUILDING_OBJECT",			// MP_CONCEPT_BUILDING_OBJECT
	"TLK_LOST_OBJECT",				// MP_CONCEPT_LOST_OBJECT
	"TLK_SPY_SAPPER",				// MP_CONCEPT_SPY_SAPPER
	"TLK_TELEPORTED",				// MP_CONCEPT_TELEPORTED
	"TLK_LOST_CONTROL_POINT",		// MP_CONCEPT_LOST_CONTROL_POINT
	"TLK_CAPTURED_POINT",			// MP_CONCEPT_CAPTURED_POINT
	"TLK_CAPTURE_BLOCKED",			// MP_CONCEPT_CAPTURE_BLOCKED
	"TLK_HEALTARGET_STARTEDHEALING",// MP_CONCEPT_HEALTARGET_STARTEDHEALING
	"TLK_HEALTARGET_CHARGEREADY",	// MP_CONCEPT_HEALTARGET_CHARGEREADY
	"TLK_HEALTARGET_STOPPEDHEALING",// MP_CONCEPT_HEALTARGET_STOPPEDHEALING
	"TLK_HEALTARGET_CHARGEDEPLOYED",// MP_CONCEPT_HEALTARGET_CHARGEDEPLOYED
	"TLK_MINIGUN_FIREWEAPON",		// MP_CONCEPT_MINIGUN_FIREWEAPON
	"TLK_DIED",						// MP_CONCEPT_DIED
	"TLK_PLAYER_THANKS",			// MP_CONCEPT_PLAYER_THANKS
	"TLK_CART_MOVING_FORWARD",		// MP_CONCEPT_CART_MOVING_FORWARD
	"TLK_CART_MOVING_BACKWARD",		// MP_CONCEPT_CART_MOVING_BACKWARD
	"TLK_CART_STOP",				// MP_CONCEPT_CART_STOP
	"TLK_ATE_FOOD",					// MP_CONCEPT_ATE_FOOD
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int GetMPConceptIndexFromString( const char *pszConcept )
{
	// Make sure our concept string and enum arrays are the same length
	contract_assert( std::size( g_pszMPConcepts ) == MP_TF_CONCEPT_COUNT );

	for ( int iConcept = 0; iConcept < std::size( g_pszMPConcepts ); ++iConcept )
	{
		if ( !V_stricmp( pszConcept, g_pszMPConcepts[iConcept] ) )
			return iConcept;
	}

	return MP_CONCEPT_NONE;
}

class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBaseMultiplayerPlayer;
class CBasePlayer;
class CItem;
class CRecipientFilter;
class CTakeDamageInfo;

using namespace ::ks::reflect;

// Purpose: Contains the implementation of game rules for multiplayer.

extern ConVar mp_verbose_changelevel_spew;
extern ConVar sv_kick_ban_duration;
extern ConVar mp_autokick;

REGISTER_GAMERULES_CLASS( CMultiplayRules );

ConVar mp_match_restart_delay(
		side_registry(), "mp_match_restart_delay", 
		"15", 
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"Time (in seconds) until a match restarts.",
		true, 1,
		true, 120 );

ConVar mapcycledisabled(
				   side_registry(), "mapcycledisabled",
				   "0",
				   FCVAR_REPLICATED | FCVAR_RELEASE,
				   "repeats the same map after each match instead of using the map cycle");

void MPTimeLimitCallback( ConVar *var, const char *pOldString, float flOldValue )
{
	if ( mp_timelimit.GetInt() < 0 )
	{
		mp_timelimit.SetValue( 0 );
	}

	if ( MultiplayRules() )
	{
		MultiplayRules()->HandleTimeLimitChange();
	}
}

ConVar mp_timelimit( side_registry(), "mp_timelimit", "5", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_RELEASE, "game time per map in minutes"
					, MPTimeLimitCallback 
					);

ConVar fraglimit( side_registry(), "mp_fraglimit","0", FCVAR_NOTIFY|FCVAR_REPLICATED, "The number of kills at which the map ends" );

ConVar tv_delaymapchange( side_registry(), "tv_delaymapchange", "1", FCVAR_RELEASE, "Delays map change until broadcast is complete" );

ConVar mp_restartgame( side_registry(), "mp_restartgame", "0", FCVAR_GAMEDLL | FCVAR_RELEASE, "If non-zero, game will restart in the specified number of seconds" );

void cc_SkipNextMapInCycle()
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( MultiplayRules() )
	{
		MultiplayRules()->SkipNextMapInCycle();
	}
}

ConCommand skip_next_map( side_registry(), "skip_next_map", cc_SkipNextMapInCycle, "Skips the next map in the map rotation for the server." );

ConVar mp_waitingforplayers_time( side_registry(), "mp_waitingforplayers_time", "0", FCVAR_GAMEDLL, "WaitingForPlayers time length in seconds" );

ConVar mp_waitingforplayers_restart( side_registry(), "mp_waitingforplayers_restart", "0", FCVAR_GAMEDLL, "Set to 1 to start or restart the WaitingForPlayers period." );
ConVar mp_waitingforplayers_cancel( side_registry(), "mp_waitingforplayers_cancel", "0", FCVAR_GAMEDLL, "Set to 1 to end the WaitingForPlayers period." );
ConVar mp_clan_readyrestart( side_registry(), "mp_clan_readyrestart", "0", FCVAR_GAMEDLL, "If non-zero, game will restart once someone from each team gives the ready signal" );
ConVar mp_clan_ready_signal( side_registry(), "mp_clan_ready_signal", "ready", FCVAR_GAMEDLL, "Text that team leader from each team must speak for the match to begin" );

ConVar nextmap_print_enabled( side_registry(), "nextmap_print_enabled", "0", FCVAR_GAMEDLL | FCVAR_RELEASE, "When enabled prints next map to clients" );

ConVar nextlevel( side_registry(), "nextlevel", 
	"", 
	FCVAR_NOTIFY | FCVAR_RELEASE | FCVAR_REPLICATED,
	"If set to a valid map name, will trigger a changelevel to the specified map at the end of the round"
				  
	);

int CMultiplayRules::m_nMapCycleTimeStamp = 0;
int CMultiplayRules::m_nMapCycleindex = 0;
std::vector< std::string > CMultiplayRules::m_MapList;

//=========================================================
//=========================================================
bool CMultiplayRules::IsMultiplayer( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CMultiplayRules::Damage_GetTimeBased( void )
{
	int iDamage = ( DMG_PARALYZE | DMG_NERVEGAS | DMG_POISON | DMG_RADIATION | DMG_DROWNRECOVER | DMG_ACID | DMG_SLOWBURN );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CMultiplayRules::Damage_GetShouldGibCorpse( void )
{
	int iDamage = ( DMG_CRUSH | DMG_FALL | DMG_BLAST | DMG_SONIC | DMG_CLUB );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CMultiplayRules::Damage_GetShowOnHud( void )
{
	int iDamage = ( DMG_POISON | DMG_ACID | DMG_DROWN | DMG_BURN | DMG_SLOWBURN | DMG_NERVEGAS | DMG_RADIATION | DMG_SHOCK );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CMultiplayRules::Damage_GetNoPhysicsForce( void )
{
	int iTimeBasedDamage = Damage_GetTimeBased();
	int iDamage = ( DMG_FALL | DMG_BURN | DMG_PLASMA | DMG_DROWN | iTimeBasedDamage | DMG_CRUSH | DMG_PHYSGUN | DMG_PREVENT_PHYSICS_FORCE );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CMultiplayRules::Damage_GetShouldNotBleed( void )
{
	int iDamage = ( DMG_POISON | DMG_ACID );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMultiplayRules::Damage_IsTimeBased( int iDmgType )
{
	// Damage types that are time-based.
	return ( ( iDmgType & ( DMG_PARALYZE | DMG_NERVEGAS | DMG_POISON | DMG_RADIATION | DMG_DROWNRECOVER | DMG_ACID | DMG_SLOWBURN ) ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMultiplayRules::Damage_ShouldGibCorpse( int iDmgType )
{
	// Damage types that gib the corpse.
	return ( ( iDmgType & ( DMG_CRUSH | DMG_FALL | DMG_BLAST | DMG_SONIC | DMG_CLUB ) ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMultiplayRules::Damage_ShowOnHUD( int iDmgType )
{
	// Damage types that have client HUD art.
	return ( ( iDmgType & ( DMG_POISON | DMG_ACID | DMG_DROWN | DMG_BURN | DMG_SLOWBURN | DMG_NERVEGAS | DMG_RADIATION | DMG_SHOCK ) ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMultiplayRules::Damage_NoPhysicsForce( int iDmgType )
{
	// Damage types that don't have to supply a physics force & position.
	int iTimeBasedDamage = Damage_GetTimeBased();
	return ( ( iDmgType & ( DMG_FALL | DMG_BURN | DMG_PLASMA | DMG_DROWN | iTimeBasedDamage | DMG_CRUSH | DMG_PHYSGUN | DMG_PREVENT_PHYSICS_FORCE ) ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMultiplayRules::Damage_ShouldNotBleed( int iDmgType )
{
	// Damage types that don't make the player bleed.
	return ( ( iDmgType & ( DMG_POISON | DMG_ACID ) ) != 0 );
}

//*********************************************************
// Rules for the half-life multiplayer game.
//*********************************************************
CMultiplayRules::CMultiplayRules()
{
	// before we exec ANY cfg files or apply any convars, go through the bspconvar whitelist and set all convars in that list to their default value
	KeyValues::AutoDelete pKV_wl( "convars" );
	if ( pKV_wl->LoadFromFile( g_pFullFileSystem, "bspconvar_whitelist.txt", "GAME" ) )
	{
		for ( KeyValues *pKey = pKV_wl->GetFirstSubKey(); pKey != nullptr; pKey = pKey->GetNextKey() )
		{
			//save the name of this outfit
			const char *szConVarName = pKey->GetName();
			ConVarRef convar( szConVarName );
			if ( convar.IsValid() )
			{
				convar.SetValue( convar.GetDefault() );
				Msg( "%s - %s\n", szConVarName, convar.GetDefault() );
			}
		}
	}

	// 11/8/98
	// Modified by YWB:  Server .cfg file is now a cvar, so that 
	//  server ops can run multiple game servers, with different server .cfg files,
	//  from a single installed directory.
	// Mapcyclefile is already a cvar.

	// 3/31/99
	// Added lservercfg file cvar, since listen and dedicated servers should not
	// share a single config file. (sjb)
	if ( engine->IsDedicatedServer() )
	{
		// dedicated server
		const char *cfgfile = servercfgfile.GetString();

		if ( cfgfile && cfgfile[0] )
		{
			char szCommand[256];

			Msg( "Executing dedicated server config file\n" );
			V_snprintf( szCommand,sizeof(szCommand), "exec %s\n", cfgfile );
			engine->ServerCommand( szCommand );
		}
	}
	else
	{
		// listen server
		const char *cfgfile = lservercfgfile.GetString();

		if ( cfgfile && cfgfile[0] )
		{
			char szCommand[256];

			Msg( "Executing listen server config file\n" );
			V_snprintf( szCommand,sizeof(szCommand), "exec %s\n", cfgfile );
			engine->ServerCommand( szCommand );
		}
	}

	nextlevel.SetValue( "" );

	LoadVoiceCommandScript();
}

	extern bool			g_fGameOver;

	constexpr auto ITEM_RESPAWN_TIME = 30;
	constexpr auto WEAPON_RESPAWN_TIME = 20;
	constexpr auto AMMO_RESPAWN_TIME = 20;

	//=========================================================
	//=========================================================
	void CMultiplayRules::RefreshSkillData( bool forceUpdate )
	{
	// load all default values
		BaseClass::RefreshSkillData( forceUpdate );

	// override some values for multiplay.

		// suitcharger
	}

	ConVar spec_replay_bot( side_registry(), "spec_replay_bot", "0", FCVAR_RELEASE, "Enable Spectator Hltv Replay when killed by bot" );

	//=========================================================
	//
	// WARNING - this function is NOT called in CS:GO 
	//
	//  CCSGameRules (which has CMultiplayRules as a baseclass) ::Think() calls
	//    CGameRules::Think() directly, bypassing CTeamplayRules::Think() and 
	//    CMultiplayRules::Think()
	//
	//  Code placed in here is likely to NEVER be called
	//=========================================================
	void CMultiplayRules::Think ( void )
	{
		BaseClass::Think();
		
		///// Check game rules /////

		if ( g_fGameOver )   // someone else quit the game already
		{
			ChangeLevel(); // intermission is over
			return;
		}

		float flTimeLimit = mp_timelimit.GetFloat() * 60;
		float flFragLimit = fraglimit.GetFloat();
		
		if ( flTimeLimit != 0 && gpGlobals->curtime >= flTimeLimit )
		{
			GoToIntermission();
			return;
		}

		if ( flFragLimit )
		{
			// check if any player is over the frag limit
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

				if ( pPlayer && pPlayer->FragCount() >= flFragLimit )
				{
					GoToIntermission();
					return;
				}
			}
		}
	}

	//=========================================================
	//=========================================================
	bool CMultiplayRules::IsDeathmatch( void )
	{
		return true;
	}

	//=========================================================
	//=========================================================
	bool CMultiplayRules::IsCoOp( void )
	{
		return false;
	}

	//=========================================================
	//=========================================================
	bool CMultiplayRules::FShouldSwitchWeapon( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon )
	{
		if ( !pPlayer->Weapon_CanSwitchTo( pWeapon ) )
		{
			// Can't switch weapons for some reason.
			return false;
		}

		if ( !pPlayer->GetActiveWeapon() )
		{
			// Player doesn't have an active item, might as well switch.
			return true;
		}

		if ( !pWeapon->AllowsAutoSwitchTo() )
		{
			// The given weapon should not be auto switched to from another weapon.
			return false;
		}

		if ( !pPlayer->GetActiveWeapon()->AllowsAutoSwitchFrom() )
		{
			// The active weapon does not allow autoswitching away from it.
			return false;
		}

		if ( pWeapon->GetWeight() > pPlayer->GetActiveWeapon()->GetWeight() )
		{
			return true;
		}

		return false;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Returns the weapon in the player's inventory that would be better than
	//			the given weapon.
	//-----------------------------------------------------------------------------
	CBaseCombatWeapon *CMultiplayRules::GetNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon )
	{
		CBaseCombatWeapon *pCheck;
		CBaseCombatWeapon *pBest;// this will be used in the event that we don't find a weapon in the same category.

		int iCurrentWeight = -1;
		int iBestWeight = -1;// no weapon lower than -1 can be autoswitched to
		pBest = nullptr;

		// If I have a weapon, make sure I'm allowed to holster it
		if ( pCurrentWeapon )
		{
			if ( !pCurrentWeapon->AllowsAutoSwitchFrom() || !pCurrentWeapon->CanHolster() )
			{
				// Either this weapon doesn't allow autoswitching away from it or I
				// can't put this weapon away right now, so I can't switch.
				return nullptr;
			}

			iCurrentWeight = pCurrentWeapon->GetWeight();
		}

		for ( int i = 0 ; i < pPlayer->WeaponCount(); ++i )
		{
			pCheck = pPlayer->GetWeapon( i );
			if ( !pCheck )
				continue;

			// If we have an active weapon and this weapon doesn't allow autoswitching away
			// from another weapon, skip it.
			if ( pCurrentWeapon && !pCheck->AllowsAutoSwitchTo() )
				continue;

			if ( pCheck->GetWeight() > -1 && pCheck->GetWeight() == iCurrentWeight && pCheck != pCurrentWeapon )
			{
				// this weapon is from the same category. 
				if ( pCheck->HasAnyAmmo() )
				{
					if ( pPlayer->Weapon_CanSwitchTo( pCheck ) )
					{
						return pCheck;
					}
				}
			}
			else if ( pCheck->GetWeight() > iBestWeight && pCheck != pCurrentWeapon )// don't reselect the weapon we're trying to get rid of
			{
				//Msg( "Considering %s\n", STRING( pCheck->GetClassname() );
				// we keep updating the 'best' weapon just in case we can't find a weapon of the same weight
				// that the player was using. This will end up leaving the player with his heaviest-weighted 
				// weapon. 
				if ( pCheck->HasAnyAmmo() )
				{
					// if this weapon is useable, flag it as the best
					iBestWeight = pCheck->GetWeight();
					pBest = pCheck;
				}
			}
		}

		// if we make it here, we've checked all the weapons and found no useable 
		// weapon in the same catagory as the current weapon. 
		
		// if pBest is null, we didn't find ANYTHING. Shouldn't be possible- should always 
		// at least get the crowbar, but ya never know.
		return pBest;
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Output : Returns true on success, false on failure.
	//-----------------------------------------------------------------------------
	bool CMultiplayRules::SwitchToNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon )
	{
		CBaseCombatWeapon *pWeapon = GetNextBestWeapon( pPlayer, pCurrentWeapon );

		if ( pWeapon != nullptr )
			return pPlayer->Weapon_Switch( pWeapon );
		
		return false;
	}

	//=========================================================
	//=========================================================
	bool CMultiplayRules::ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen )
	{
		GetVoiceGameMgr()->ClientConnected( pEntity );

		/*
		CBasePlayer *pl = ToBasePlayer( GetContainingEntity( pEntity ) );
		if ( pl && ( engine->IsSplitScreenPlayer( pl->entindex() ) )
		{
			Msg( "%s is a split screen player\n", pszName );
		}
		*/

		return true;
	}

	void CMultiplayRules::InitHUD( CBasePlayer *pl )
	{
	} 

	//=========================================================
	//=========================================================
	void CMultiplayRules::ClientDisconnected( edict_t *pClient )
	{
		if ( pClient )
		{
			CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );

			if ( pPlayer )
			{
				FireTargets( "game_playerleave", pPlayer, pPlayer, USE_TOGGLE, 0 );

				pPlayer->RemoveAllItems( true );// destroy all of the players weapons and items

				// Kill off view model entities
				pPlayer->DestroyViewModels();

				pPlayer->SetConnected( PlayerDisconnected );
			}
		}
	}

	//=========================================================
	//=========================================================
	float CMultiplayRules::FlPlayerFallDamage( CBasePlayer *pPlayer )
	{
		int iFallDamage = (int)falldamage.GetFloat();

		switch ( iFallDamage )
		{
		case 1://progressive
			pPlayer->m_Local.m_flFallVelocity -= PLAYER_MAX_SAFE_FALL_SPEED;
			return pPlayer->m_Local.m_flFallVelocity * DAMAGE_FOR_FALL_SPEED;
			break;
		default:
		case 0:// fixed
			return 10;
			break;
		}
	} 

	//=========================================================
	//=========================================================
	bool CMultiplayRules::AllowDamage( CBaseEntity *pVictim, const CTakeDamageInfo &info )
	{
		return true;
	}

	//=========================================================
	//=========================================================
	bool CMultiplayRules::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
	{
		return true;
	}

	//=========================================================
	//=========================================================
	void CMultiplayRules::PlayerThink( CBasePlayer *pPlayer )
	{
		if ( g_fGameOver )
		{
			// clear attack/use commands from player
			pPlayer->m_afButtonPressed = 0;
			pPlayer->m_nButtons = 0;
			pPlayer->m_afButtonReleased = 0;
		}
	}

	//=========================================================
	//=========================================================
	void CMultiplayRules::PlayerSpawn( CBasePlayer *pPlayer )
	{
		bool		addDefault;
		CBaseEntity	*pWeaponEntity = nullptr;

		pPlayer->EquipSuit();
		
		addDefault = true;

		while ( (pWeaponEntity = gEntList.FindEntityByClassname( pWeaponEntity, "game_player_equip" )) != nullptr)
		{
			pWeaponEntity->Touch( pPlayer );
			addDefault = false;
		}
	}

	//=========================================================
	//=========================================================
	bool CMultiplayRules::FPlayerCanRespawn( CBasePlayer *pPlayer )
	{
		return true;
	}

	//=========================================================
	//=========================================================
	float CMultiplayRules::FlPlayerSpawnTime( CBasePlayer *pPlayer )
	{
		return gpGlobals->curtime;//now!
	}

	bool CMultiplayRules::AllowAutoTargetCrosshair( void )
	{
		return ( aimcrosshair.GetInt() != 0 );
	}

	//=========================================================
	// IPointsForKill - how many points awarded to anyone
	// that kills this player?
	//=========================================================
	int CMultiplayRules::IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
	{
		return 1;
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	CBasePlayer *CMultiplayRules::GetDeathScorer( CBaseEntity *pKiller, CBaseEntity *pInflictor )
	{
		if ( pKiller)
		{
			if ( pKiller->Classify() == CLASS_PLAYER )
				return (CBasePlayer*)pKiller;

			// Killing entity might be specifying a scorer player
			IScorer *pScorerInterface = dynamic_cast<IScorer*>( pKiller );
			if ( pScorerInterface )
			{
				CBasePlayer *pPlayer = pScorerInterface->GetScorer();
				if ( pPlayer )
					return pPlayer;
			}

			// Inflicting entity might be specifying a scoring player
			pScorerInterface = dynamic_cast<IScorer*>( pInflictor );
			if ( pScorerInterface )
			{
				CBasePlayer *pPlayer = pScorerInterface->GetScorer();
				if ( pPlayer )
					return pPlayer;
			}
		}

		return nullptr;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Returns player who should receive credit for kill
	//-----------------------------------------------------------------------------
	CBasePlayer *CMultiplayRules::GetDeathScorer( CBaseEntity *pKiller, CBaseEntity *pInflictor, CBaseEntity *pVictim )
	{
		// if this method not overridden by subclass, just call our default implementation
		return GetDeathScorer( pKiller, pInflictor );
	}

	//=========================================================
	// PlayerKilled - someone/something killed this player
	//=========================================================
	void CMultiplayRules::PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info )
	{

		// Find the killer & the scorer
		CBaseEntity *pInflictor = info.GetInflictor();
		CBaseEntity *pKiller = info.GetAttacker();
		CBasePlayer *pScorer = GetDeathScorer( pKiller, pInflictor, pVictim );
		
		pVictim->IncrementDeathCount( 1 );

		// dvsents2: uncomment when removing all FireTargets
		// variant_t value;
		// g_EventQueue.AddEvent( "game_playerdie", "Use", value, 0, pVictim, pVictim );
		FireTargets( "game_playerdie", pVictim, pVictim, USE_TOGGLE, 0 );

		// Did the player kill himself?
		if ( pVictim == pScorer )  
		{			
			if ( UseSuicidePenalty() )
			{
				// Players lose a frag for killing themselves
				pVictim->IncrementFragCount( -1 );
			}			
		}
		else if ( pScorer )
		{
			// if a player dies in a deathmatch game and the killer is a client, award the killer some points
			int numPointsPerKill = IPointsForKill( pScorer, pVictim );
			int numHeadshots = ( ( numPointsPerKill == 1 ) && ( info.GetDamageType() & /*DMG_HEADSHOT*/(DMG_LASTGENERICFLAG<<1) ) ) ? 1 : 0;
			pScorer->IncrementFragCount( numPointsPerKill, numHeadshots );

			if ( numHeadshots > 0 )
			{
				pVictim->m_iDeathPostEffect = 15; // POST_EFFECT_DEATH_CAM_HEADSHOT
			}
			else
			{
				pVictim->m_iDeathPostEffect = 14; // POST_EFFECT_DEATH_CAM_BODYSHOT
			}

			// Allow the scorer to immediately paint a decal
			pScorer->AllowImmediateDecalPainting();

			// dvsents2: uncomment when removing all FireTargets
			//variant_t value;
			//g_EventQueue.AddEvent( "game_playerkill", "Use", value, 0, pScorer, pScorer );
			FireTargets( "game_playerkill", pScorer, pScorer, USE_TOGGLE, 0 );
		}
		else
		{  
			if ( UseSuicidePenalty() )
			{
				// Players lose a frag for letting the world kill them			
				pVictim->IncrementFragCount( -1 );
			}		
		}

		DeathNotice( pVictim, info );

	}

	//=========================================================
	// Deathnotice. 
	//=========================================================
	void CMultiplayRules::DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info )
	{
		// Work out what killed the player, and send a message to all clients about it
		const char *killer_weapon_name = "world";		// by default, the player is killed by the world
		int killer_ID = 0;

		// Find the killer & the scorer
		CBaseEntity *pInflictor = info.GetInflictor();
		CBaseEntity *pKiller = info.GetAttacker();
		CBasePlayer *pScorer = GetDeathScorer( pKiller, pInflictor, pVictim );

		// Custom damage type?
		if ( info.GetDamageCustom() )
		{
			killer_weapon_name = GetDamageCustomString( info );
			if ( pScorer )
			{
				killer_ID = pScorer->GetUserID();
			}
		}
		else
		{
			// Is the killer a client?
			if ( pScorer )
			{
				killer_ID = pScorer->GetUserID();
				
				if ( pInflictor )
				{
					if ( pInflictor == pScorer )
					{
						// If the inflictor is the killer,  then it must be their current weapon doing the damage
						if ( pScorer->GetActiveWeapon() )
						{
							killer_weapon_name = pScorer->GetActiveWeapon()->GetDeathNoticeName();
						}
					}
					else
					{
						killer_weapon_name = STRING( pInflictor->m_iClassname );  // it's just that easy
					}
				}
			}
			else
			{
				killer_weapon_name = STRING( pInflictor->m_iClassname );
			}

			// strip the NPC_* or weapon_* from the inflictor's classname
			if ( IsWeaponClassname( killer_weapon_name ) )
			{
				killer_weapon_name += WEAPON_CLASSNAME_PREFIX_LENGTH;
			}
			else if ( StringHasPrefixCaseSensitive( killer_weapon_name, "NPC_" ) )
			{
				killer_weapon_name += V_strlen( "NPC_" );
			}
			else if ( StringHasPrefixCaseSensitive( killer_weapon_name, "func_" ) )
			{
				killer_weapon_name += V_strlen( "func_" );
			}
		}

		IGameEvent * event = gameeventmanager->CreateEvent( "player_death" );
		if ( event )
		{
			event->SetInt("userid", pVictim->GetUserID() );
			event->SetInt("attacker", killer_ID );
			event->SetInt("customkill", info.GetDamageCustom() );
			event->SetInt("priority", 7 );	// HLTV event priority, not transmitted
			if( !OnReplayPrompt( pVictim, pScorer ) )
				event->SetBool( "noreplay", true );
			gameeventmanager->FireEvent( event );
		}

	}

	bool CMultiplayRules::OnReplayPrompt( CBasePlayer *pVictim, CBasePlayer *pScorer )
	{
		if( !spec_replay_bot.GetBool() && pScorer && pScorer->IsBot() )
		{
			return false; // don't replay kills by bots if spec_replay_bot == 0 
		}

		if( !engine->HasHltvReplay( ) )
			return false; // cannot replay if the engine doesn't have hltv replay working

		// ok we are ready to replay. Let's replay it to other people, too!
		return true;
	}

	//=========================================================
	// FlWeaponRespawnTime - what is the time in the future
	// at which this weapon may spawn?
	//=========================================================
	float CMultiplayRules::FlWeaponRespawnTime( CBaseCombatWeapon *pWeapon )
	{
		if ( weaponstay.GetInt() > 0 )
		{
			// make sure it's only certain weapons
			if ( !(pWeapon->GetWeaponFlags() & ITEM_FLAG_LIMITINWORLD) )
			{
				return gpGlobals->curtime + 0;		// weapon respawns almost instantly
			}
		}

		return gpGlobals->curtime + WEAPON_RESPAWN_TIME;
	}

	// when we are within this close to running out of entities,  items 
	// marked with the ITEM_FLAG_LIMITINWORLD will delay their respawn
	constexpr auto ENTITY_INTOLERANCE = 100;

	//=========================================================
	// FlWeaponRespawnTime - Returns 0 if the weapon can respawn 
	// now,  otherwise it returns the time at which it can try
	// to spawn again.
	//=========================================================
	float CMultiplayRules::FlWeaponTryRespawn( CBaseCombatWeapon *pWeapon )
	{
		if ( pWeapon && (pWeapon->GetWeaponFlags() & ITEM_FLAG_LIMITINWORLD) )
		{
			if ( gEntList.NumberOfEntities() < (gpGlobals->maxEntities - ENTITY_INTOLERANCE) )
				return 0;

			// we're past the entity tolerance level,  so delay the respawn
			return FlWeaponRespawnTime( pWeapon );
		}

		return 0;
	}

	//=========================================================
	// VecWeaponRespawnSpot - where should this weapon spawn?
	// Some game variations may choose to randomize spawn locations
	//=========================================================
	Vector CMultiplayRules::VecWeaponRespawnSpot( CBaseCombatWeapon *pWeapon )
	{
		return pWeapon->GetAbsOrigin();
	}

	//=========================================================
	// WeaponShouldRespawn - any conditions inhibiting the
	// respawning of this weapon?
	//=========================================================
	int CMultiplayRules::WeaponShouldRespawn( CBaseCombatWeapon *pWeapon )
	{
		if ( pWeapon->HasSpawnFlags( SF_NORESPAWN ) )
		{
			return GR_WEAPON_RESPAWN_NO;
		}

		return GR_WEAPON_RESPAWN_YES;
	}

	//=========================================================
	// CanHaveWeapon - returns false if the player is not allowed
	// to pick up this weapon
	//=========================================================
	bool CMultiplayRules::CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pItem )
	{
		if ( weaponstay.GetInt() > 0 )
		{
			if ( pItem->GetWeaponFlags() & ITEM_FLAG_LIMITINWORLD )
				return BaseClass::CanHavePlayerItem( pPlayer, pItem );

			// check if the player already has this weapon
			for ( int i = 0 ; i < pPlayer->WeaponCount() ; i++ )
			{
				if ( pPlayer->GetWeapon(i) == pItem )
				{
					return false;
				}
			}
		}

		return BaseClass::CanHavePlayerItem( pPlayer, pItem );
	}

	//=========================================================
	//=========================================================
	bool CMultiplayRules::CanHaveItem( CBasePlayer *pPlayer, CItem *pItem )
	{
		return true;
	}

	//=========================================================
	//=========================================================
	void CMultiplayRules::PlayerGotItem( CBasePlayer *pPlayer, CItem *pItem )
	{
	}

	//=========================================================
	//=========================================================
	int CMultiplayRules::ItemShouldRespawn( CItem *pItem )
	{
		if ( pItem->HasSpawnFlags( SF_NORESPAWN ) )
		{
			return GR_ITEM_RESPAWN_NO;
		}

		return GR_ITEM_RESPAWN_YES;
	}

	//=========================================================
	// At what time in the future may this Item respawn?
	//=========================================================
	float CMultiplayRules::FlItemRespawnTime( CItem *pItem )
	{
		return gpGlobals->curtime + ITEM_RESPAWN_TIME;
	}

	//=========================================================
	// Where should this item respawn?
	// Some game variations may choose to randomize spawn locations
	//=========================================================
	Vector CMultiplayRules::VecItemRespawnSpot( CItem *pItem )
	{
		return pItem->GetAbsOrigin();
	}

	//=========================================================
	// What angles should this item use to respawn?
	//=========================================================
	QAngle CMultiplayRules::VecItemRespawnAngles( CItem *pItem )
	{
		return pItem->GetAbsAngles();
	}

	//=========================================================
	//=========================================================
	void CMultiplayRules::PlayerGotAmmo( CBaseCombatCharacter *pPlayer, char *szName, int iCount )
	{
	}

	//=========================================================
	//=========================================================
	bool CMultiplayRules::IsAllowedToSpawn( CBaseEntity *pEntity )
	{
	//	if ( pEntity->GetFlags() & FL_NPC )
	//		return false;

		return true;
	}

	//=========================================================
	//=========================================================
	float CMultiplayRules::FlHealthChargerRechargeTime( void )
	{
		return 60;
	}

	float CMultiplayRules::FlHEVChargerRechargeTime( void )
	{
		return 30;
	}

	//=========================================================
	//=========================================================
	int CMultiplayRules::DeadPlayerWeapons( CBasePlayer *pPlayer )
	{
		return GR_PLR_DROP_GUN_ACTIVE;
	}

	//=========================================================
	//=========================================================
	int CMultiplayRules::DeadPlayerAmmo( CBasePlayer *pPlayer )
	{
		return GR_PLR_DROP_AMMO_ACTIVE;
	}

	CBaseEntity *CMultiplayRules::GetPlayerSpawnSpot( CBasePlayer *pPlayer )
	{
		CBaseEntity *pentSpawnSpot = BaseClass::GetPlayerSpawnSpot( pPlayer );	

	//!! replace this with an Event
	/*
		if ( IsMultiplayer() && pentSpawnSpot->m_target )
		{
			FireTargets( STRING(pentSpawnSpot->m_target), pPlayer, pPlayer, USE_TOGGLE, 0 ); // dvsents2: what is this code supposed to do?
		}
	*/

		return pentSpawnSpot;
	}

	//=========================================================
	//=========================================================
	bool CMultiplayRules::PlayerCanHearChat( CBasePlayer *pListener, CBasePlayer *pSpeaker, bool bTeamOnly )
	{
		return !bTeamOnly || PlayerRelationship( pListener, pSpeaker ) == GR_TEAMMATE;
	}

	int CMultiplayRules::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
	{
		// half life deathmatch has only enemies
		return GR_NOTTEAMMATE;
	}

	bool CMultiplayRules::PlayFootstepSounds( CBasePlayer *pl )
	{
		if ( footsteps.GetInt() == 0 )
			return false;

		if ( pl->IsOnLadder() || pl->GetAbsVelocity().Length2D() > 220 )
			return true;  // only make step sounds in multiplayer if the player is moving fast enough

		return false;
	}

	bool CMultiplayRules::FAllowFlashlight( void ) 
	{ 
		return flashlight.GetInt() != 0; 
	}

	//=========================================================
	//=========================================================
	bool CMultiplayRules::FAllowNPCs( void )
	{
		return true; // E3 hack
		return ( allowNPCs.GetInt() != 0 );
	}

	//=========================================================
	//======== CMultiplayRules private functions ===========

	float CMultiplayRules::GetIntermissionDuration() const
	{
		float flWaitTime = mp_match_restart_delay.GetInt();

		if ( tv_delaymapchange.GetBool() )
		{
			if ( HLTVDirector() && HLTVDirector()->IsActive() )
			{
				CEngineHltvInfo_t engineHltv;
				if ( engine->GetEngineHltvInfo( engineHltv ) &&
					 engineHltv.m_bBroadcastActive && ( engineHltv.m_numClients > 0 ) )
				{
					flWaitTime = std::max( flWaitTime, HLTVDirector()->GetDelay() + 5.0f );
				}
			}
		}

		return flWaitTime;
	}

	void CMultiplayRules::GoToIntermission( void )
	{
		if ( g_fGameOver )
			return;

		g_fGameOver = true;
		m_flIntermissionStartTime = gpGlobals->curtime;

		IGameEvent * event = gameeventmanager->CreateEvent( "cs_intermission" );
		if ( event )
		{
			gameeventmanager->FireEvent( event );
		}

	}

	void StripChar(char *szBuffer, const char cWhiteSpace )
	{

		while ( char *pSpace = std::strchr( szBuffer, cWhiteSpace ) )
		{
			char *pNextChar = pSpace + sizeof(char);
			V_strcpy( pSpace, pNextChar );
		}
	}

	void CMultiplayRules::GetNextLevelName( char *pszNextMap, int bufsize, bool bRandom /* = false */ )
	{
		const char *mapGroupName = nullptr;	

		mapGroupName = STRING( gpGlobals->mapGroupName );

		if ( mp_verbose_changelevel_spew.GetBool() )
		{
			Msg( "CHANGELEVEL: Looking for next level in mapgroup '%s'\n", mapGroupName );
		}

		// If mapcycling is disabled, just return the same map name and bail.
		if ( mapcycledisabled.GetBool() )
		{
			V_strncpy( pszNextMap, STRING( gpGlobals->mapname ), bufsize );
			if ( mp_verbose_changelevel_spew.GetBool() && pszNextMap )
			{
				Msg( "CHANGELEVEL: Map cycle disabled (due to convar '%s') -- not using map group, reloading current map '%s'\n", mapcycledisabled.GetName(), pszNextMap );
			}
			return;
		}

		const char* nextMapName = nullptr;
		if ( bRandom )
		{	
			nextMapName = g_pGameTypes->GetRandomMap( mapGroupName );
			if ( mp_verbose_changelevel_spew.GetBool() && nextMapName )
			{
				Msg( "CHANGELEVEL: Random map request, choosing '%s'\n", nextMapName );
			}
		}
		else
		{
			if ( m_szNextLevelName && m_szNextLevelName[0] )
			{
				nextMapName = m_szNextLevelName;
				contract_assert( 0 ); // Suspect this is a dead code path... remove me if possible
			}
			else
			{
				const char* szPrevMap = STRING( gpGlobals->mapname );
				nextMapName = g_pGameTypes->GetNextMap( mapGroupName, szPrevMap );
				if ( mp_verbose_changelevel_spew.GetBool() )
				{
					if ( nextMapName && szPrevMap )
						Msg( "CHANGELEVEL: Choosing map '%s' (previous was %s)\n", nextMapName, szPrevMap );
					else
						Msg( "CHANGELEVEL: GetNextMap failed for mapgroup '%s', map group invalid or empty\n", mapGroupName);
				}
			}		
		}

		if ( nextMapName )
		{
			// we have a valid map name from the mapgroup info
			V_strncpy( pszNextMap, nextMapName, bufsize );
			return;
		}

		// we were not given a mapgroup name or we were given a mapname that was not in the mapgroup, so we fall back to the old method of cycling maps

		const char *mapcfile = mapcyclefile.GetString();
		contract_assert( mapcfile != nullptr );

		// Check the time of the mapcycle file and re-populate the list of level names if the file has been modified
		const int nMapCycleTimeStamp = filesystem->GetPathTime( mapcfile, "GAME" );

		if ( 0 == nMapCycleTimeStamp )
		{
			// Map cycle file does not exist, make a list containing only the current map
			char *szCurrentMapName = new char[MAX_PATH];
			V_strncpy( szCurrentMapName, STRING(gpGlobals->mapname), MAX_PATH );
			m_MapList.push_back( szCurrentMapName );

			if ( mp_verbose_changelevel_spew.GetBool() && szCurrentMapName )
			{
				Msg( "CHANGELEVEL: No maycycle file, using current map '%s'\n", szCurrentMapName );
			}
		}
		else
		{
			// If map cycle file has changed or this is the first time through ...
			if ( m_nMapCycleTimeStamp != nMapCycleTimeStamp )
			{
				// Reset map index and map cycle timestamp
				m_nMapCycleTimeStamp = nMapCycleTimeStamp;
				m_nMapCycleindex = 0;

				m_MapList.clear();

				// Repopulate map list from mapcycle file
				int nFileLength;
				char *aFileList = (char*)UTIL_LoadFileForMe( mapcfile, &nFileLength );
				if ( aFileList && nFileLength )
				{
					V_SplitString( aFileList, "\n", m_MapList );

					for ( int i = 0; i < std::ssize( m_MapList ); i++ )
					{
						bool bIgnore = false;

						// Strip out the spaces in the name
						StripChar( m_MapList[i].data() , '\r');
						StripChar( m_MapList[i].data() , ' ');
						
						if ( !engine->IsMapValid( m_MapList[i].c_str() ) )
						{
							bIgnore = true;

							// If the engine doesn't consider it a valid map remove it from the lists
							Warning( "Invalid map '%s' included in map cycle file. Ignored.\n", m_MapList[i].c_str() );
						}
						else if ( StringHasPrefixCaseSensitive( m_MapList[i].c_str(), "//" ) )
						{
							bIgnore = true;
						}

						if ( bIgnore )
						{
							m_MapList.erase( m_MapList.begin() + i );
							--i;
						}
					}

					UTIL_FreeFile( (byte *)aFileList );
				}

				// If the current map selection is in the list, set m_nMapCycleindex to the map that follows it.
				for ( int i = 0; i < std::ssize( m_MapList ); i++ )
				{
					if ( V_strcmp( STRING( gpGlobals->mapname ), m_MapList[i].c_str() ) == 0 )
					{
						m_nMapCycleindex = i;
						IncrementMapCycleIndex();
						break;
					}
				}
			}
		}

		// If somehow we have no maps in the list then add the current one
		if ( m_MapList.empty() )
		{
			char *szDefaultMapName = new char[MAX_PATH];
			V_strncpy( szDefaultMapName, STRING(gpGlobals->mapname), MAX_PATH );
			m_MapList.push_back( szDefaultMapName );
			if ( mp_verbose_changelevel_spew.GetBool() && szDefaultMapName )
			{
				Msg( "CHANGELEVEL: Map list empty or failed to parse, using current map '%s'\n", szDefaultMapName );
			}
		}

		if ( bRandom )
		{
			m_nMapCycleindex = RandomInt( 0, std::ssize( m_MapList ) - 1 );
		}

		// Here's the return value
		V_strncpy( pszNextMap, m_MapList[m_nMapCycleindex].c_str(), bufsize);
	}

	void CMultiplayRules::ChangeLevel( void )
	{
		char szNextMap[MAX_PATH];

		if ( nextlevel.GetString() && *nextlevel.GetString() && engine->IsMapValid( nextlevel.GetString() ) )
		{
			V_strncpy( szNextMap, nextlevel.GetString(), sizeof( szNextMap ) );
		}
		else
		{
			GetNextLevelName( szNextMap, sizeof(szNextMap) );
			IncrementMapCycleIndex();
		}

		g_fGameOver = true;
		Msg( "CHANGE LEVEL: %s\n", szNextMap );
		engine->ChangeLevel( szNextMap, nullptr );
	}

	//-----------------------------------------------------------------------------
	// Purpose: Shared script resource of voice menu commands and hud strings
	//-----------------------------------------------------------------------------
	void CMultiplayRules::LoadVoiceCommandScript( void )
	{
		KeyValues *pKV = new KeyValues( "VoiceCommands" );

		if ( pKV->LoadFromFile( filesystem, "scripts/voicecommands.txt", "GAME" ) )
		{
			for ( KeyValues *menu = pKV->GetFirstSubKey(); menu != nullptr; menu = menu->GetNextKey() )
			{
				int iMenuIndex = std::ssize( m_VoiceCommandMenus ); m_VoiceCommandMenus.emplace_back();

				int iNumItems = 0;

				// for each subkey of this menu, add a menu item
				for ( KeyValues *menuitem = menu->GetFirstSubKey(); menuitem != nullptr; menuitem = menuitem->GetNextKey() )
				{
					iNumItems++;

					if ( iNumItems > 9 )
					{
						Warning( "Trying to load more than 9 menu items in voicecommands.txt, extras ignored" );
						continue;
					}

					VoiceCommandMenuItem_t item;

					int iConcept = GetMPConceptIndexFromString( menuitem->GetString( "concept", "" ) );
					if ( iConcept == MP_CONCEPT_NONE )
					{
						Warning( "Voicecommand script attempting to use unknown concept. Need to define new concepts in code. ( %s )\n", menuitem->GetString( "concept", "" ) );
					}
					item.m_iConcept = iConcept;

					item.m_bShowSubtitle = ( menuitem->GetInt( "show_subtitle", 0 ) > 0 );
					item.m_bDistanceBasedSubtitle = ( menuitem->GetInt( "distance_check_subtitle", 0 ) > 0 );

					V_strncpy( item.m_szGestureActivity, menuitem->GetString( "activity", "" ), sizeof( item.m_szGestureActivity ) ); 
					m_VoiceCommandMenus[ iMenuIndex ].push_back( item );
				}
			}
		}

		pKV->deleteThis();
	}

	void CMultiplayRules::SkipNextMapInCycle()
	{
		char szSkippedMap[MAX_PATH];
		char szNextMap[MAX_PATH];

		GetNextLevelName( szSkippedMap, sizeof( szSkippedMap ) );
		IncrementMapCycleIndex();
		GetNextLevelName( szNextMap, sizeof( szNextMap ) );

		Msg( "Skipping: %s\tNext map: %s\n", szSkippedMap, szNextMap );

		if ( nextlevel.GetString() && *nextlevel.GetString() && engine->IsMapValid( nextlevel.GetString() ) )
		{
			Msg( "Warning! \"nextlevel\" is set to \"%s\" and will override the next map to be played.\n", nextlevel.GetString() );
		}
	}

	void CMultiplayRules::IncrementMapCycleIndex()
	{
		// Reset index if we've passed the end of the map list
		if ( ++m_nMapCycleindex >= std::ssize( m_MapList ) )
		{
			m_nMapCycleindex = 0;
		}
	}

	bool CMultiplayRules::ClientCommand( CBaseEntity *pEdict, const CCommand &args )
	{
		CBasePlayer *pPlayer = ToBasePlayer( pEdict );

		const char *pcmd = args[0];
		if ( FStrEq( pcmd, "voicemenu" ) )
		{
			if ( args.ArgC() < 3 )
				return true;

			CBaseMultiplayerPlayer *pMultiPlayerPlayer = dynamic_cast< CBaseMultiplayerPlayer * >( pPlayer );

			if ( pMultiPlayerPlayer )
			{
				int iMenu = std::atoi( args[1] );
				int iItem = atoi( args[2] );

				VoiceCommand( pMultiPlayerPlayer, iMenu, iItem );
			}

			return true;
		}

		return BaseClass::ClientCommand( pEdict, args );

	}

	VoiceCommandMenuItem_t *CMultiplayRules::VoiceCommand( CBaseMultiplayerPlayer *pPlayer, int iMenu, int iItem )
	{
		// have the player speak the concept that is in a particular menu slot
		if ( !pPlayer )
			return nullptr;

		if ( iMenu < 0 || iMenu >= std::ssize( m_VoiceCommandMenus ) )
			return nullptr;

		if ( iItem < 0 || iItem >= std::ssize( m_VoiceCommandMenus[ iMenu ] ) )
			return nullptr;

		VoiceCommandMenuItem_t *pItem = &m_VoiceCommandMenus[ iMenu ][ iItem ];

		contract_assert( pItem );

		char szResponse[AI_Response::MAX_RESPONSE_NAME];

		if ( pPlayer->CanSpeakVoiceCommand() )
		{
			CMultiplayer_Expresser *pExpresser = pPlayer->GetMultiplayerExpresser();
			contract_assert( pExpresser );
			pExpresser->AllowMultipleScenes();

			if ( pPlayer->SpeakConceptIfAllowed( pItem->m_iConcept, nullptr, szResponse, AI_Response::MAX_RESPONSE_NAME ) )
			{
				// show a subtitle if we need to
				if ( pItem->m_bShowSubtitle )
				{
					CRecipientFilter filter;

					if ( pItem->m_bDistanceBasedSubtitle )
					{
						filter.AddRecipientsByPAS( pPlayer->WorldSpaceCenter() );

						// further reduce the range to a certain radius
						int i;
						for ( i = filter.GetRecipientCount()-1; i >= 0; i-- )
						{
							int index = filter.GetRecipientIndex(i);

							CBasePlayer *pListener = UTIL_PlayerByIndex( index );

							if ( pListener && pListener != pPlayer )
							{
								float flDist = ( pListener->WorldSpaceCenter() - pPlayer->WorldSpaceCenter() ).Length2D();

								if ( flDist > VOICE_COMMAND_MAX_SUBTITLE_DIST )
									filter.RemoveRecipientByPlayerIndex( index );
							}
						}
					}
					else
					{
						filter.AddAllPlayers();
					}

					// if we aren't a disguised spy
					if ( !pPlayer->ShouldShowVoiceSubtitleToEnemy() )
					{
						// remove players on other teams
						filter.RemoveRecipientsNotOnTeam( pPlayer->GetTeam() );
					}

					// Register this event in the mod-specific usermessages .cpp file if you hit this assert
					// Gurjeets - Commented this Assert out when converting messages to protobuf. There is no instance in code
					// of user message "VoiceSubtitle" ever being sent
					// Assert( usermessages->LookupUserMessage( "VoiceSubtitle" ) != -1 );
				}

				pPlayer->NoteSpokeVoiceCommand( szResponse );
			}
			else
			{
				pItem = nullptr;
			}

			pExpresser->DisallowMultipleScenes();
			return pItem;
		}

		return nullptr;
	}

	bool CMultiplayRules::IsLoadingBugBaitReport()
	{
		return ( !engine->IsDedicatedServer()&& CommandLine()->CheckParm( "-bugbait" ) && sv_cheats->GetBool() );
	}

	void CMultiplayRules::HaveAllPlayersSpeakConceptIfAllowed( int iConcept )
	{
		CBaseMultiplayerPlayer *pPlayer;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			pPlayer = ToBaseMultiplayerPlayer( UTIL_PlayerByIndex( i ) );

			if ( !pPlayer )
				continue;

			pPlayer->SpeakConceptIfAllowed( iConcept );
		}
	}

	void CMultiplayRules::GetTaggedConVarList( KeyValues *pCvarTagList )
	{
		BaseClass::GetTaggedConVarList( pCvarTagList );

		KeyValues *pGravity = new KeyValues( "sv_gravity" );
		pGravity->SetString( "convar", "sv_gravity" );
		pGravity->SetString( "tag", "gravity" );

		pCvarTagList->AddSubKey( pGravity );
	}
