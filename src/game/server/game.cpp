// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include <string.h>
#include <time.h>
#include "networkvar.h"
#include "edict.h"
#include "usermessages_schema.h"
#include "declare_class.h"
#include "eiface.h"
#include "reflect.h"
#include "responserules/response_types.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "reflect_datamap.h"
#include "util.h"
#include "iregistry.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "tier2/tier2_logging.h"
module ks.server;

import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.cvar;
import ks.tier1.iconvar;
import ks.tier1.icvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.strtools;
import :vscript;
import :fish;
import :sharedvar;
import :player;
import :baseentity;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import :nav_4;
import :entity_2;
import std;
import ks.math.mathlib;
import ks.core.basetypes;
import ks.core.dbg;
import ks.game.in_buttons;
import ks.math.compressed_vector;
import ks.math.ssemath;
import ks.math.vector;
import :entity_dispatch;
import :entity_kind;
import ks.game.mapentities_shared;
import ks.core.cmdline;
import ks.core.cpu;
import ks.core.perfstats;
import ks.core.platform;
import ks.core.types;
import ks.tier1.byte_reader;
import ks.tier1.byte_writer;
import ks.tier1.checksum_crc;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.text_writer;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
import ks.tier1.map_ops;

using namespace ::ks::reflect;

void MapCycleFileChangedCallback( ConVar *var, const char *pOldString, float flOldValue )
{
	if ( V_stricmp( pOldString, mapcyclefile.GetString() ) != 0 )
	{
		if ( GameRules() )
		{
			// For multiplayer games, forces the mapcyclefile to be reloaded
			GameRules()->ResetMapCycleTimeStamp();
		}
	}
}

ConVar	displaysoundlist( side_registry(), "displaysoundlist","0" );
ConVar  mapcyclefile( side_registry(), "mapcyclefile", "mapcycle.txt", FCVAR_NONE, "Name of the .txt file used to cycle the maps on multiplayer servers ", MapCycleFileChangedCallback );
ConVar  loopsingleplayermaps(side_registry(), "loopsingleplayermaps", "0", FCVAR_REPLICATED | FCVAR_CHEAT );

ConVar  servercfgfile( side_registry(), "servercfgfile","server.cfg", FCVAR_RELEASE );
ConVar  lservercfgfile( side_registry(), "lservercfgfile","listenserver.cfg" );

// multiplayer server rules
ConVar	teamplay( side_registry(), "mp_teamplay","0", FCVAR_NOTIFY );
ConVar	falldamage( side_registry(), "mp_falldamage","0", FCVAR_NOTIFY );
ConVar	weaponstay( side_registry(), "mp_weaponstay","0", FCVAR_NOTIFY );
ConVar	forcerespawn( side_registry(), "mp_forcerespawn","1", FCVAR_NOTIFY );
ConVar	footsteps( side_registry(), "mp_footsteps","1", FCVAR_NOTIFY );
ConVar	flashlight( side_registry(), "mp_flashlight","0", FCVAR_NOTIFY );
ConVar	aimcrosshair( side_registry(), "mp_autocrosshair","1", FCVAR_NOTIFY );
ConVar	teamlist( side_registry(), "mp_teamlist","hgrunt;scientist", FCVAR_NOTIFY );
ConVar	teamoverride( side_registry(), "mp_teamoverride","1" );
ConVar	defaultteam( side_registry(), "mp_defaultteam","0" );
ConVar	allowNPCs( side_registry(), "mp_allowNPCs","1", FCVAR_NOTIFY );

// Engine Cvars
const ConVar	*g_pDeveloper = nullptr;

ConVar suitvolume( side_registry(), "suitvolume", "0.25", FCVAR_ARCHIVE );

static bool GameDLL_ConVarAccessor( ConCommandBase *pCommand )
{
	// Remember "unlinked" default value for replicated cvars
	bool replicated = pCommand->IsFlagSet( FCVAR_REPLICATED );
	const char *defvalue = nullptr;
	if ( replicated && !pCommand->IsCommand() )
	{
		defvalue = ( ( ConVar * )pCommand)->GetDefault();
	}

	// Link to engine's list instead
	cvar->RegisterConCommand( pCommand );

	if ( !cvar->GetCommandLineValue( pCommand->GetName() ) )
	{
		// NOTE:  If not overridden at the command line, then if it's a replicated cvar, make sure that it's
		//  value is the server's value.  This solves a problem where think_limit is defined in shared
		//  code but the value is inside and #if defined( _DEBUG ) block and if you have a debug game .dll
		//  and a release client, then the limiit was coming from the client even though the server value 
		//  was the one that was important during debugging.  Now the server trumps the client value for
		//  replicated ConVars by setting the value here after the ConVar has been linked.
		if ( replicated && defvalue && !pCommand->IsCommand() )
		{
			ConVar *var = ( ConVar * )pCommand;
			var->SetValue( defvalue );
		}
	}

	return true;
}

// Register your console variables here
// This gets called one time when the game is initialied
void InitializeCvars( void )
{
	// Register cvars here:
	ConVar_Register( side_registry().convars, FCVAR_GAMEDLL, &GameDLL_ConVarAccessor ); 

	g_pDeveloper	= cvar->FindVar( "developer" );
}

using namespace ::ks::reflect;

CGameTimescale g_GameTimescale;
CGameTimescale* GameTimescale() { return &g_GameTimescale; }

CGameTimescale::CGameTimescale( void ) : CAutoGameSystemPerFrame( "CGameTimescale" )
{
	m_flStartBlendRealtime = 0.0f;
}

CGameTimescale::~CGameTimescale()
{
}

bool CGameTimescale::Init()
{
	ResetTimescale();

	return true;
}

void CGameTimescale::Shutdown()
{
	ResetTimescale();
}

	void CGameTimescale::FrameUpdatePostEntityThink()
	{
		UpdateTimescale();
	}

void CGameTimescale::LevelInitPostEntity()
{
	ResetTimescale();
}

void CGameTimescale::LevelShutdownPostEntity()
{
	ResetTimescale();
}

void CGameTimescale::SetCurrentTimescale( float flTimescale )
{
	if ( m_flCurrentTimescale == flTimescale && m_flCurrentTimescale == engine->GetTimescale() )
		return;

	// No ramp in/out, just set it!
	m_flDesiredTimescale = flTimescale;
	m_flCurrentTimescale = m_flDesiredTimescale;

	m_flDurationRealTimeSeconds = 0.0f;
	m_nInterpolatorType = INTERPOLATOR_LINEAR;

	m_flStartBlendTime = 0.0f;
	m_flStartBlendRealtime = 0.0f;

	engine->SetTimescale( m_flCurrentTimescale );

	// Pass the change info to the client so it can do prediction
	CReliableBroadcastRecipientFilter filter;
	ks::net::CCSUsrMsg_CurrentTimescale msg;
	msg.cur_timescale = m_flCurrentTimescale;
	SendUserMessage( filter, ks::net::CS_UM_CurrentTimescale, msg );
}

void CGameTimescale::SetDesiredTimescaleAtTime( float flDesiredTimescale, float flDurationRealTimeSeconds /*= 0.0f*/, Interpolators_e nInterpolatorType /*= INTERPOLATOR_LINEAR*/, float flStartBlendTime /*= 0.0f*/ )
{
	SetDesiredTimescale( flDesiredTimescale, flDurationRealTimeSeconds, nInterpolatorType, std::max( 0.0f, flStartBlendTime - gpGlobals->curtime ) );
}

void CGameTimescale::SetDesiredTimescale( float flDesiredTimescale, float flDurationRealTimeSeconds /*= 0.0f*/, Interpolators_e nInterpolatorType /*= INTERPOLATOR_LINEAR*/, float flDelayRealtime /*= 0.0f*/ )
{
	if ( m_flDesiredTimescale == flDesiredTimescale )
		return;

	m_flDesiredTimescale = flDesiredTimescale;
	m_flDurationRealTimeSeconds = flDurationRealTimeSeconds;
	m_nInterpolatorType = nInterpolatorType;

	m_flStartBlendTime = gpGlobals->curtime + flDelayRealtime * m_flCurrentTimescale;

	if ( gpGlobals->curtime >= m_flStartBlendTime )
	{
		m_flStartBlendRealtime = gpGlobals->realtime;
	}
	else
	{
		m_flStartBlendRealtime = 0.0f;
	}

	m_flStartTimescale = m_flCurrentTimescale;

	// Pass the change info to the client so it can do prediction
	CReliableBroadcastRecipientFilter filter;
	ks::net::CCSUsrMsg_DesiredTimescale msg;
	msg.desired_timescale = m_flDesiredTimescale;
	msg.duration_realtime_sec = m_flDurationRealTimeSeconds;
	msg.interpolator_type = m_nInterpolatorType;
	msg.start_blend_time = m_flStartBlendTime;
	SendUserMessage( filter, ks::net::CS_UM_DesiredTimescale, msg );
}

void CGameTimescale::UpdateTimescale( void )
{
	if ( engine->IsPaused() )
		return;

	if ( m_flCurrentTimescale != m_flDesiredTimescale )
	{
		if ( gpGlobals->curtime >= m_flStartBlendTime )
		{
			if ( m_flStartBlendRealtime == 0.0f )
			{
				m_flStartBlendRealtime = gpGlobals->realtime;
			}

			float flInterp = 1.0f;

			if ( m_flDurationRealTimeSeconds > 0.0f )
			{
				flInterp = std::min( 1.0f, ( gpGlobals->realtime - m_flStartBlendRealtime ) / m_flDurationRealTimeSeconds );
			}

			switch ( m_nInterpolatorType )
			{
			case INTERPOLATOR_ACCEL:
				flInterp = Bias( flInterp, 0.25f );
				break;

			case INTERPOLATOR_DEACCEL:
				flInterp = Bias( flInterp, 0.75f );
				break;

			case INTERPOLATOR_EASE_IN_OUT:
				flInterp = Gain( flInterp, 0.75f );
				break;
			}

			m_flCurrentTimescale = m_flStartTimescale * ( 1.0f - flInterp ) + m_flDesiredTimescale * flInterp;
		}
	}

	if ( m_flCurrentTimescale != engine->GetTimescale() )
	{
		engine->SetTimescale( m_flCurrentTimescale );
	}
}

void CGameTimescale::ResetTimescale( void )
{
	m_flDesiredTimescale = 1.0f;
	m_flCurrentTimescale = 1.0f;

	m_flDurationRealTimeSeconds = 0.0f;
	m_nInterpolatorType = INTERPOLATOR_LINEAR;

	m_flStartBlendTime = 0.0f;
	m_flStartBlendRealtime = 0.0f;

	engine->SetTimescale( 1.0f );
}

class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Entities that capture the player's UI and move it into game design
//			as outputs.

//----------------------------------------------------------------
// Spawn flags
//----------------------------------------------------------------
static constexpr auto SF_GAMEUI_FREEZE_PLAYER = 32;
static constexpr auto SF_GAMEUI_HIDE_WEAPON = 64;
static constexpr auto SF_GAMEUI_USE_DEACTIVATES = 128;
static constexpr auto SF_GAMEUI_JUMP_DEACTIVATES = 256;

class CGameUI : public CBaseEntity
{
public:
	DECLARE_CLASS( CGameUI, CBaseEntity );

	DECLARE_DATADESC();

	// Input handlers
	[[= ks::reflect::Input{ .name = "Deactivate", .type = FIELD_VOID } ]] void InputDeactivate( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Activate", .type = FIELD_STRING } ]] void InputActivate( inputdata_t &inputdata );

	void Think_Impl( void );
	void Deactivate( CBaseEntity *pActivator );

	[[= ks::reflect::Key{ .name = "FieldOfView" } ]] float				m_flFieldOfView;
	CHandle<CBaseCombatWeapon>	m_hSaveWeapon;

	[[= ks::reflect::Key{ .name = "PlayerOn" } ]] COutputEvent		m_playerOn;
	[[= ks::reflect::Key{ .name = "PlayerOff" } ]] COutputEvent		m_playerOff;

	[[= ks::reflect::Key{ .name = "PressedMoveLeft" } ]] COutputEvent		m_pressedMoveLeft;
	[[= ks::reflect::Key{ .name = "PressedMoveRight" } ]] COutputEvent		m_pressedMoveRight;
	[[= ks::reflect::Key{ .name = "PressedForward" } ]] COutputEvent		m_pressedForward;
	[[= ks::reflect::Key{ .name = "PressedBack" } ]] COutputEvent		m_pressedBack;
	[[= ks::reflect::Key{ .name = "PressedAttack" } ]] COutputEvent		m_pressedAttack;
	[[= ks::reflect::Key{ .name = "PressedAttack2" } ]] COutputEvent		m_pressedAttack2;
	
	[[= ks::reflect::Key{ .name = "UnpressedMoveLeft" } ]] COutputEvent		m_unpressedMoveLeft;
	[[= ks::reflect::Key{ .name = "UnpressedMoveRight" } ]] COutputEvent		m_unpressedMoveRight;
	[[= ks::reflect::Key{ .name = "UnpressedForward" } ]] COutputEvent		m_unpressedForward;
	[[= ks::reflect::Key{ .name = "UnpressedBack" } ]] COutputEvent		m_unpressedBack;
	[[= ks::reflect::Key{ .name = "UnpressedAttack" } ]] COutputEvent		m_unpressedAttack;
	[[= ks::reflect::Key{ .name = "UnpressedAttack2" } ]] COutputEvent		m_unpressedAttack2;

	[[= ks::reflect::Key{ .name = "XAxis" } ]] COutputFloat		m_xaxis;
	[[= ks::reflect::Key{ .name = "YAxis" } ]] COutputFloat		m_yaxis;
	[[= ks::reflect::Key{ .name = "AttackAxis" } ]] COutputFloat		m_attackaxis;
	[[= ks::reflect::Key{ .name = "Attack2Axis" } ]] COutputFloat		m_attack2axis;

	bool				m_bForceUpdate;
	int					m_nLastButtonState;

	CHandle<CBasePlayer>	m_player;
};

IMPLEMENT_REFLECT_DATAMAP( CGameUI )

LINK_ENTITY_TO_CLASS( game_ui, CGameUI );
	
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::InputDeactivate( inputdata_t &inputdata )
{
	Deactivate( inputdata.pActivator );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::Deactivate( CBaseEntity *pActivator )
{
	CBasePlayer *pPlayer = m_player;

	contract_assert( pPlayer && "CGameUI deactivated without a player!" );

	if (pPlayer)
	{
		// Re-enable player motion
		if ( FBitSet( m_spawnflags, SF_GAMEUI_FREEZE_PLAYER ) )
		{
			m_player->RemoveFlag( FL_ATCONTROLS );
		}

		// Restore weapons
		if ( FBitSet( m_spawnflags, SF_GAMEUI_HIDE_WEAPON ) )
		{
			// Turn the hud back on
			pPlayer->m_Local.m_iHideHUD &= ~HIDEHUD_WEAPONSELECTION;

			if ( m_hSaveWeapon.Get() )
			{
				m_player->Weapon_Switch( m_hSaveWeapon.Get() );
				m_hSaveWeapon = nullptr;
			}

			if ( pPlayer->GetActiveWeapon() )
			{
				pPlayer->GetActiveWeapon()->Deploy();
			}
		}

		// Announce that the player is no longer controlling through us
		m_playerOff.FireOutput( pPlayer, this, 0 );

		// Clear out the axis controls
		m_xaxis.Set( 0, pPlayer, this );
		m_yaxis.Set( 0, pPlayer, this );
		m_attackaxis.Set( 0, pPlayer, this );
		m_attack2axis.Set( 0, pPlayer, this );
		m_nLastButtonState = 0;
		m_player = nullptr;
	}
	else
	{
		Warning("%s Deactivate(): I have no player when called by %s!\n", GetEntityName().ToCStr(), pActivator->GetEntityName().ToCStr());
	}
	
	// Stop thinking
	SetNextThink( TICK_NEVER_THINK );
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CGameUI::InputActivate( inputdata_t &inputdata )
{
	CBasePlayer *pPlayer;

	// Determine if we're specifying this as an override parameter
	if ( inputdata.value.StringID() != NULL_STRING )
	{
		CBaseEntity *pEntity = gEntList.FindEntityByName( nullptr, inputdata.value.String(), this, inputdata.pActivator, inputdata.pCaller );
		if ( pEntity == nullptr || pEntity->IsPlayer() == false )
		{
			Warning( "%s InputActivate: entity %s not found or is not a player!\n", GetEntityName().ToCStr(), inputdata.value.String() );
			return;
		}

		pPlayer = ToBasePlayer( pEntity );
	}
	else
	{
		// Otherwise try to use the activator
		if ( inputdata.pActivator == nullptr || inputdata.pActivator->IsPlayer() == false )
		{
			Warning( "%s InputActivate (%s): invalid or missing !activator!\n", GetEntityName().ToCStr(), inputdata.value.String() );
			return;
		}

		pPlayer = ToBasePlayer( inputdata.pActivator );
	}

	// If another player is already using these controls3, ignore this activation
	if ( m_player.Get() != nullptr && pPlayer != m_player.Get() )
	{
		// TODO: We could allow this by calling Deactivate() at this point and continuing on -- jdw
		return;
	}

	// Setup our internal data
	m_player = pPlayer;
	m_playerOn.FireOutput( pPlayer, this, 0 );

	// Turn the hud off
	SetNextThink( gpGlobals->curtime );

	// Disable player's motion
	if ( FBitSet( m_spawnflags, SF_GAMEUI_FREEZE_PLAYER ) )
	{
		m_player->AddFlag( FL_ATCONTROLS );
	}

	// Store off and hide the currently held weapon
	if ( FBitSet( m_spawnflags, SF_GAMEUI_HIDE_WEAPON ) )
	{
		m_player->m_Local.m_iHideHUD |= HIDEHUD_WEAPONSELECTION;

		if ( m_player->GetActiveWeapon() )
		{
			m_hSaveWeapon = m_player->GetActiveWeapon();

			m_player->GetActiveWeapon()->Holster();
			m_player->ClearActiveWeapon();
			m_player->HideViewModels();
		}
	}

	// We must update our state
	m_bForceUpdate = true;
}

//------------------------------------------------------------------------------
// Purpose: Samples the player's inputs and fires outputs based on what buttons
//			are currently held down.
//------------------------------------------------------------------------------
void CGameUI::Think_Impl( void )
{
	CBasePlayer *pPlayer = m_player;

	// If player is gone, stop thinking
	if (pPlayer == nullptr)
	{
		SetNextThink( TICK_NEVER_THINK );
		return;
	}

	// If we're forcing an update, state with a clean button state
	if ( m_bForceUpdate )
	{
		m_nLastButtonState = pPlayer->m_nButtons;
	}

	// ------------------------------------------------
	// Check that toucher is facing the UI within
	// the field of view tolerance.  If not disconnect
	// ------------------------------------------------
	if (m_flFieldOfView > -1)
	{
		Vector vPlayerFacing;
		pPlayer->EyeVectors( &vPlayerFacing );
		Vector vPlayerToUI =  GetAbsOrigin() - pPlayer->WorldSpaceCenter();
		VectorNormalize(vPlayerToUI);

		float flDotPr = DotProduct(vPlayerFacing,vPlayerToUI);
		if (flDotPr < m_flFieldOfView)
		{
			Deactivate( pPlayer );
			return;
		}
	}

	pPlayer->AddFlag( FL_ONTRAIN );
	SetNextThink( gpGlobals->curtime );

	// Deactivate if they jump or press +use.
	// FIXME: prevent the use from going through in player.cpp
	if ((( pPlayer->m_afButtonPressed & IN_USE ) && ( m_spawnflags & SF_GAMEUI_USE_DEACTIVATES )) ||
		(( pPlayer->m_afButtonPressed & IN_JUMP ) && ( m_spawnflags & SF_GAMEUI_JUMP_DEACTIVATES )))
	{
		Deactivate( pPlayer );
		return;
	}

	// Determine what's different
	int nButtonsChanged = ( pPlayer->m_nButtons ^ m_nLastButtonState );

	//
	// Handle all our possible input triggers
	//

	if ( nButtonsChanged & IN_MOVERIGHT )
	{
		if ( m_nLastButtonState & IN_MOVERIGHT )
		{
			m_unpressedMoveRight.FireOutput( pPlayer, this, 0 );
		}
		else
		{
			m_pressedMoveRight.FireOutput( pPlayer, this, 0 );
		}
	}

	if ( nButtonsChanged & IN_MOVELEFT )
	{
		if ( m_nLastButtonState & IN_MOVELEFT )
		{
			m_unpressedMoveLeft.FireOutput( pPlayer, this, 0 );
		}
		else
		{
			m_pressedMoveLeft.FireOutput( pPlayer, this, 0 );
		}
	}

	if ( nButtonsChanged & IN_FORWARD )
	{
		if ( m_nLastButtonState & IN_FORWARD )
		{
			m_unpressedForward.FireOutput( pPlayer, this, 0 );
		}
		else
		{
			m_pressedForward.FireOutput( pPlayer, this, 0 );
		}
	}

	if ( nButtonsChanged & IN_BACK )
	{
		if ( m_nLastButtonState & IN_BACK )
		{
			m_unpressedBack.FireOutput( pPlayer, this, 0 );
		}
		else
		{
			m_pressedBack.FireOutput( pPlayer, this, 0 );
		}
	}

	if ( nButtonsChanged & IN_ATTACK )
	{
		if ( m_nLastButtonState & IN_ATTACK )
		{
			m_unpressedAttack.FireOutput( pPlayer, this, 0 );
		}
		else
		{
			m_pressedAttack.FireOutput( pPlayer, this, 0 );
		}
	}

	if ( nButtonsChanged & IN_ATTACK2 )
	{
		if ( m_nLastButtonState & IN_ATTACK2 )
		{
			m_unpressedAttack2.FireOutput( pPlayer, this, 0 );
		}
		else
		{
			m_pressedAttack2.FireOutput( pPlayer, this, 0 );
		}
	}

	// Setup for the next frame
	m_nLastButtonState = pPlayer->m_nButtons;

	float x = 0, y = 0, attack = 0, attack2 = 0;
	if ( pPlayer->m_nButtons & IN_MOVERIGHT )
	{
		x = 1;
	}
	else if ( pPlayer->m_nButtons & IN_MOVELEFT )
	{
		x = -1;
	}

	if ( pPlayer->m_nButtons & IN_FORWARD )
	{
		y = 1;
	}
	else if ( pPlayer->m_nButtons & IN_BACK )
	{
		y = -1;
	}

	if ( pPlayer->m_nButtons & IN_ATTACK )
	{
		attack = 1;
	}

	if ( pPlayer->m_nButtons & IN_ATTACK2 )
	{
		attack2 = 1;
	}

	//
	// Fire the analog outputs if they changed.
	//
	if ( m_bForceUpdate || ( m_xaxis.Get() != x ) )
	{
		m_xaxis.Set( x, pPlayer, this );
	}

	if ( m_bForceUpdate || ( m_yaxis.Get() != y ) )
	{
		m_yaxis.Set( y, pPlayer, this );
	}

	if ( m_bForceUpdate || ( m_attackaxis.Get() != attack ) )
	{
		m_attackaxis.Set( attack, pPlayer, this );
	}

	if ( m_bForceUpdate || ( m_attack2axis.Get() != attack2 ) )
	{
		m_attack2axis.Set( attack2, pPlayer, this );
	}

	m_bForceUpdate = false;
}

class CBaseEntity;
class CBasePlayer;
class CTakeDamageInfo;
class CUserCmd;
struct inputdata_t;

using namespace ::ks::reflect;

#ifndef DEDICATED
#endif

inline constexpr const char *GAMESTATS_LOG_FILE = "gamestats.log";
inline constexpr const char *GAMESTATS_PATHID = "MOD";

/*
static constexpr auto ONE_DAY_IN_SECONDS = 86400;

// Lower threshold in debug for testing...
#if defined( _DEBUG )
#define WALKED_AWAY_FROM_KEYBOARD_SECONDS 15.0f   // 15 seconds of movement == might be paused
#else
#define WALKED_AWAY_FROM_KEYBOARD_SECONDS 300.0f   // 5 minutes of no movement == might be paused
#endif
*/

extern IUploadGameStats *gamestatsuploader;

static char s_szPseudoUniqueID[20] = "";

static inline char const *SafeString( char const *pStr )
{
	return ( pStr ) ? pStr : "?";
}

static CBaseGameStats s_GameStats_Singleton;
CBaseGameStats *gamestats = &s_GameStats_Singleton; //start out pointing at the basic version which does nothing by default
extern ConVar skill;
void OverWriteCharsWeHate( char *pStr );

bool StatsTrackingIsFullyEnabled( void );

class CGamestatsData
{
public:
	CGamestatsData()
	{
		m_pKVData = nullptr;
		m_bHaveData = false;
		AllocData();
	}

	~CGamestatsData()
	{
		FreeData();
	}

	void AllocData()
	{
		FreeData();
		char buffer[ 128 ];
		V_snprintf( buffer, sizeof( buffer ), "gamestats_" );
		UTIL_GetModDir( buffer + 10, sizeof( buffer ) - 10 );
		m_pKVData = new KeyValues( buffer );
	}
	void FreeData()
	{
		if ( m_pKVData != nullptr )
		{
			m_pKVData->deleteThis();
			m_pKVData = nullptr;
		}
	}

	KeyValues *m_pKVData;
	bool m_bHaveData;
};

//used to drive most of the game stat event handlers as well as track basic stats under the hood of CBaseGameStats
class CBaseGameStats_Driver : public CAutoGameSystemPerFrame
{
public:
	CBaseGameStats_Driver( void );
	
	typedef CAutoGameSystemPerFrame BaseClass;

	// IGameSystem overloads
	virtual bool Init();
	virtual void Shutdown();

	// Level init, shutdown
	virtual void LevelInitPreEntity();
	virtual void LevelShutdownPreEntity();
	// Called during game save
	virtual void OnSave();
	// Called during game restore, after the local player has connected and entities have been fully restored
	virtual void OnRestore();

	virtual void FrameUpdatePostEntityThink();

	void PossibleMapChange( void );

	void CollectData( StatSendType_t sendType );
	void SendData();
	void ResetData();
	bool AddBaseDataForSend( KeyValues *pKV, StatSendType_t sendType );

	StatsBufferRecord_t m_StatsBuffer[STATS_WINDOW_SIZE];
	bool m_bBufferFull;
	int m_nWriteIndex;
	double m_flLastRealTime;
	double m_flLastSampleTime;
	float m_flTotalTimeInLevels;
	int m_iNumLevels;
	bool m_bDidVoiceChat;	// Did the player use voice chat at ALL this map?

	// Track how often the CPU is reported as running at full speed (above kCPUMonitoringWarning1),
	// medium speed (above kCPUMonitoringWarning2) or low speed (other).
	int m_nCPUUnthrottledCount;
	int m_nCPUThrottle1Count;
	int m_nCPUThrottle2Count;
	// Track when we last received data.
	double m_lastCPUFrequencyTimestamp;

	// Generate a buffer that will keep trailing observations prior to adding them to the main m_StatsBuffer array
	StatsBufferRecord_t m_TrailingStatsBuffer[STATS_TRAILING_WINDOW_SIZE];
	int m_nTrailingWriteIndex;
	int m_nTrailingSamplesWritten;	
	
	template<class T> T AverageStat( T StatsBufferRecord_t::*field ) const
	{
		T sum = 0;
		int iMax = m_bBufferFull ? std::size( m_StatsBuffer ) : m_nWriteIndex;
		for( int i = 0; i < iMax; i++ )
			sum += m_StatsBuffer[i].*field;
		return sum / iMax;
	}

	template<class T> T MaxStat( T StatsBufferRecord_t::*field ) const
	{
		T maxsofar = -16000000;
		int iMax = m_bBufferFull ? std::size( m_StatsBuffer ) : m_nWriteIndex;
		for( int i = 0; i < iMax; i++ )
			maxsofar = std::max( maxsofar, m_StatsBuffer[i].*field );
		return maxsofar;
	}

	template<class T> T MinStat( T StatsBufferRecord_t::*field ) const
	{
		T minsofar = 16000000;
		int iMax = m_bBufferFull ? std::size( m_StatsBuffer ) : m_nWriteIndex;
		for( int i = 0; i < iMax; i++ )
			minsofar = std::min( minsofar, m_StatsBuffer[i].*field );
		return minsofar;
	}

	template<class T> T StdDevStat( T StatsBufferRecord_t::*field, T average ) const
	{
		T flDeviationsquared = 0;
		int iMax = m_bBufferFull ? std::size( m_StatsBuffer ) : m_nWriteIndex;

		// Compute std deviation
		for (int i = 0; i < iMax; i++)
		{
			T val = m_StatsBuffer[i].*field - average;
			flDeviationsquared += (val * val);
		}

		return ( ( iMax > 1 ) ? std::sqrt( (flDeviationsquared / T(iMax - 1) ) ) : 0 );
	}

	void SortFramerate()
	{
		int iMax = ( ( m_bBufferFull ) ? std::size( m_StatsBuffer ) : m_nWriteIndex ) - 1;

		// Sort members of m_StatsBuffer
		std::sort(&m_StatsBuffer[0],&m_StatsBuffer[iMax]);
	}

	float PercentileFramerate( int nPercent )  const
	{
	    int iMax = ( ( m_bBufferFull ) ? std::size( m_StatsBuffer ) : m_nWriteIndex ) - 1;

		// Return the first or last member if nPercent is out of bounds
		if ( nPercent >= 100 )
			return m_StatsBuffer[iMax].m_flFrameRate;
		if ( nPercent <= 0 )
			return m_StatsBuffer[0].m_flFrameRate;

		// Otherwise determine the desired index and return its field.	
		// NOTE: nPercent = 50 is close to (but is NEVER) the median - special code would be needed to handle odd-item arrays, or averaging two elements for even-item arrays.
		int nIdx = (iMax * nPercent) / 100;
		return m_StatsBuffer[nIdx].m_flFrameRate;
	}

	inline void AdvanceIndex( void )
	{
		m_nWriteIndex++;
		if ( m_nWriteIndex == STATS_WINDOW_SIZE )
		{
			m_nWriteIndex = 0;
			m_bBufferFull = true;
		}
	}

	void UpdatePerfStats( void )
	{
		double flCurTime = Plat_FloatTime();

		if ( ( m_flLastRealTime > 0.0f ) &&
				( flCurTime > m_flLastRealTime ) )
		{				
			float flFps = 1.0 / ( flCurTime - m_flLastRealTime );
			float flCpuWait = g_PerfStats.m_Slots[ PERF_STATS_SLOT_END_FRAME ].m_PrevFrameTime.GetMillisecondsF();
			if ( g_pGameRules ) // are we in the game? if not, it's not worth collecting histogram info
			{
				uint8 nBin = m_FpsHistory.Update( flFps, flCpuWait );
				m_FpsHistogram.Update( nBin );
				for ( int nEvent = 0; nEvent < PERF_STATS_EVENT_COUNT; ++nEvent )
				{
					m_FpsHistogramGame[ nEvent ].Update( nBin );
				}
			}

			m_TrailingStatsBuffer[m_nTrailingWriteIndex].m_flFrameRate = flFps;

			m_nTrailingWriteIndex++;
			if ( m_nTrailingWriteIndex == STATS_TRAILING_WINDOW_SIZE )
				m_nTrailingWriteIndex = 0;
				
			if ( m_nTrailingSamplesWritten < STATS_TRAILING_WINDOW_SIZE )
				m_nTrailingSamplesWritten++;
		}

		if ( ( m_flLastSampleTime >= 0.0f ) && 
			( flCurTime - m_flLastSampleTime >= STATS_RECORD_INTERVAL ) )
		{
			if ( ( m_flLastRealTime > 0 ) && ( flCurTime > m_flLastRealTime ) )
			{
			
				StatsBufferRecord_t &stat = m_StatsBuffer[m_nWriteIndex];

				float flSumFrameRate = 0;
				for ( int i = 0; i < m_nTrailingSamplesWritten; i++ )
				{
					flSumFrameRate += m_TrailingStatsBuffer[i].m_flFrameRate;
				}
				float flFrameRate = flSumFrameRate / m_nTrailingSamplesWritten; 
				stat.m_flFrameRate = flFrameRate;

				m_nTrailingSamplesWritten = 0;
				m_nTrailingWriteIndex = 0;

				stat.m_flMainThreadTimeMS		= g_PerfStats.m_Slots[PERF_STATS_SLOT_MAINTHREAD].m_PrevFrameTime.GetMillisecondsF();
				stat.m_flMainThreadWaitTimeMS	= g_PerfStats.m_Slots[PERF_STATS_SLOT_END_FRAME].m_PrevFrameTime.GetMillisecondsF();
				stat.m_flRenderThreadTimeMS		= g_PerfStats.m_Slots[PERF_STATS_SLOT_RENDERTHREAD].m_PrevFrameTime.GetMillisecondsF();
				stat.m_flRenderThreadWaitTimeMS	= g_PerfStats.m_Slots[PERF_STATS_SLOT_FORCE_HARDWARE_SYNC].m_PrevFrameTime.GetMillisecondsF();

				AdvanceIndex();
				m_flLastSampleTime = flCurTime;
			}
		}
		if ( m_flLastSampleTime == -1 )
		{
			m_flLastSampleTime = flCurTime;
		}
		m_flLastRealTime = flCurTime;

		const CPUFrequencyResults results = GetCPUFrequencyResults( true );
		if ( results.m_timeStamp > m_lastCPUFrequencyTimestamp )
		{
			m_lastCPUFrequencyTimestamp = results.m_timeStamp;
			// Increment the appropriate counter for how much the CPU is throttled (if at all)
			if ( results.m_percentage > kCPUMonitoringWarning1 )
				++m_nCPUUnthrottledCount;
			else if ( results.m_percentage > kCPUMonitoringWarning2 )
				++m_nCPUThrottle1Count;
			else
				++m_nCPUThrottle2Count;
		}
	}

	void FirePerfStatsEvent( PerfStatsEventEnum_t nEvent, int nLookAhead = CFpsHistory::HISTORY_BUF_SIZE, int nLookBack = CFpsHistory::HISTORY_BUF_SIZE )
	{
		m_FpsHistogramGame[ nEvent ].Fire( nLookAhead, nLookBack, m_FpsHistory );
	}

	std::string		m_PrevMapName; //used to track "OnMapChange" events
	int				m_iLoadedVersion;
	char			m_szLoadedUserID[ 17 ];	// GUID

	bool			m_bEnabled; //false if incapable of uploading or the user doesn't want to enable stat tracking
	bool			m_bShuttingDown;
	bool			m_bInLevel;
	bool			m_bFirstLevel;
	std::time_t			m_tLastUpload;

	float			m_flLevelStartTime;

	bool			m_bStationary;
	float			m_flLastMovementTime;
	CUserCmd		m_LastUserCmd;
	bool			m_bGamePaused;
	float			m_flPauseStartTime;

	CGamestatsData	*m_pGamestatsData;

	CFpsHistory m_FpsHistory;
	CFpsHistogram m_FpsHistogram;
	CFpsSelectiveHistogram m_FpsHistogramGame[ PERF_STATS_EVENT_COUNT ];
};
static CBaseGameStats_Driver CBGSDriver;

void UpdatePerfStats( void )
{
	CBGSDriver.UpdatePerfStats();
}

void FirePerfStatsEvent( PerfStatsEventEnum_t nEvent, int nLookAhead, int nLookBack )
{
	CBGSDriver.FirePerfStatsEvent( nEvent, nLookAhead, nLookBack );
}

uint8 CFpsHistory::Update( float flFps, float flCpuWait )
{
	uint8 nBin = 0;
	if ( flFps >= 45 )
	{
		if ( flFps >= 90 )
		{
			if ( flFps >= 120 )
			{
				nBin = FPS_120;
			}
			else
			{
				nBin = FPS_90;
			}
		}
		else
		{
			if ( flFps >= 60 )
			{
				nBin = FPS_60;
			}
			else
			{
				nBin = FPS_45;
			}
		}
	}
	else
	{
		if ( flFps >= 20 )
		{
			if ( flFps >= 30 )
			{
				nBin = FPS_30;
			}
			else
			{
				nBin = FPS_20;
			}
		}
		else
		{
			if ( flFps >= 10 )
			{
				nBin = FPS_10;
			}
			else
			{
				nBin = FPS_LOW;
			}
		}
	}

	if ( flCpuWait > 0.5f )
	{
		nBin |= 0x80;
	}

	return nBin;
}

int64 CFpsHistogram::Encode()const
{
	uint nMaxBin = 0;
	for ( int i = 0; i < FPS_BIN_COUNT; ++i )
	{
		nMaxBin = Max( nMaxBin, m_nFps[ i ] );
	}

	if ( nMaxBin == 0 )
		return 0;

	int64 nEncode = 0;
	for ( int i = 0; i < FPS_BIN_COUNT / 2; ++i )
	{
		nEncode <<= 4;
		if ( uint nDenominator = m_nFps[ i * 2 ] + m_nFps[ i * 2 + 1 ] )
		{
			uint nNormalized = ( 15ll * ( m_nCpuWaits[ i * 2 ] + m_nCpuWaits[ i * 2 + 1 ] ) + nDenominator / 2 ) / nDenominator;
			nEncode |= nNormalized;
		}
	}
	for ( int i = 0; i < FPS_BIN_COUNT; ++i )
	{
		nEncode <<= 6;
		uint nNormalized = ( ( 0x3Fll * m_nFps[ i ] + nMaxBin / 2 ) / nMaxBin ); // round to nearest
		nEncode |= nNormalized;
	}
	return nEncode;
}

CBaseGameStats_Driver::CBaseGameStats_Driver( void ) :
	BaseClass( "CGameStats" ),
	m_iLoadedVersion( -1 ),
	m_bEnabled( false ),
	m_bShuttingDown( false ),
	m_bInLevel( false ),
	m_bFirstLevel( true ),
	m_flLevelStartTime( 0.0f ),
	m_bStationary( false ),
	m_flLastMovementTime( 0.0f ),
	m_bGamePaused( false ),
	m_pGamestatsData( nullptr ),
	m_bBufferFull( false ),
	m_nWriteIndex( 0 ),
	m_flLastRealTime( -1 ),
	m_flLastSampleTime( -1 ),
	m_flTotalTimeInLevels( 0 ),
	m_iNumLevels( 0 ),
	m_bDidVoiceChat( false ),
	m_nTrailingWriteIndex( 0 ),
	m_nTrailingSamplesWritten( 0 )
{
	m_szLoadedUserID[0] = 0;
	m_tLastUpload = 0;
	m_LastUserCmd.Reset();
}

static FileHandle_t g_LogFileHandle = FILESYSTEM_INVALID_HANDLE;

CBaseGameStats::CBaseGameStats() :
	m_bLogging( false ),
	m_bLoggingToFile( false )
{
}

bool CBaseGameStats::StatTrackingAllowed( void )
{
	return CBGSDriver.m_bEnabled;
}

// Don't care about vcr hooks here...
#undef localtime
#undef asctime

void CBaseGameStats::StatsLog( char const *fmt, ... )
{
	if ( !m_bLogging && !m_bLoggingToFile )
		return;

	char buf[ 2048 ];
	std::va_list argptr;
	__builtin_va_start( argptr, fmt );
	V_vsnprintf( buf, sizeof( buf ), fmt, argptr );
	__builtin_va_end( argptr );

	// Prepend timestamp and spew it

	// Prepend the time.
	std::time_t aclock;
	std::time( &aclock );
	struct tm *newtime = std::localtime( &aclock );

	char timeString[ 128 ];
	V_strncpy( timeString, std::asctime( newtime ), sizeof( timeString ) );
	// Get rid of the \n.
	char *pEnd = std::strstr( timeString, "\n" );
	if ( pEnd )
	{
		*pEnd = 0;
	}

	if ( m_bLogging )
	{
		DevMsg( "[GS %s - %7.2f] %s", timeString, gpGlobals->realtime, buf );
	}

	if ( m_bLoggingToFile )
	{
		if ( FILESYSTEM_INVALID_HANDLE == g_LogFileHandle )
		{
			g_LogFileHandle = filesystem->Open( GAMESTATS_LOG_FILE, "a", GAMESTATS_PATHID );
		}

		if ( FILESYSTEM_INVALID_HANDLE != g_LogFileHandle )
		{
			filesystem->FPrintf( g_LogFileHandle, "[GS %s - %7.2f] %s", timeString, gpGlobals->realtime, buf );
			filesystem->Flush( g_LogFileHandle );
		}
	}
}

static char s_szSaveFileName[256] = "";
static char s_szStatUploadRegistryKeyName[256] = "";

const char *CBaseGameStats::GetStatSaveFileName( void )
{
	contract_assert( ( s_szSaveFileName[0] != '\0' ) && "Don't know what file to save stats to." );
	return s_szSaveFileName;
}

const char *CBaseGameStats::GetStatUploadRegistryKeyName( void )
{
	contract_assert( ( s_szStatUploadRegistryKeyName[0] != '\0' ) && "Don't know the registry key to use to mark stats uploads." );
	return s_szStatUploadRegistryKeyName;
}

const char *CBaseGameStats::GetUserPseudoUniqueID( void )
{
	contract_assert( ( s_szPseudoUniqueID[0] != '\0' ) && "Don't have a pseudo unique ID." );
	return s_szPseudoUniqueID;
}

void CBaseGameStats::Event_Init( void )
{
	SetCyberCafeStatistic( gamestatsuploader->IsCyberCafeUser() );
	SetDXLevelStatistic( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() );
	++m_BasicStats.m_Summary.m_nCount;

	StatsLog( "CBaseGameStats::Event_Init [%dth session]\n", m_BasicStats.m_Summary.m_nCount );
}

void CBaseGameStats::Event_Shutdown( void )
{
	StatsLog( "CBaseGameStats::Event_Shutdown [%dth session]\n", m_BasicStats.m_Summary.m_nCount );

	StatsLog( "\n====================================================================\n\n" );
}

void CBaseGameStats::Event_MapChange( const char *szOldMapName, const char *szNewMapName )
{
	StatsLog( "CBaseGameStats::Event_MapChange to [%s]\n", szNewMapName );
}

void CBaseGameStats::Event_LevelInit( void )
{
	StatsLog( "CBaseGameStats::Event_LevelInit [%s]\n", CBGSDriver.m_PrevMapName.c_str() );

	BasicGameStatsRecord_t *map = gamestats->m_BasicStats.FindOrAddRecordForMap( CBGSDriver.m_PrevMapName.c_str() );
	++map->m_nCount;

	// HACK HACK:  Punching this hole through only works in single player!!!
	if ( gpGlobals->maxClients == 1 )
	{
		ConVarRef closecaption( "closecaption" );
		if( closecaption.IsValid() )
			SetCaptionsStatistic( closecaption.GetBool() );

		SetHDRStatistic( gamestatsuploader->IsHDREnabled() );

		SetSkillStatistic( skill.GetInt() );
			SetCyberCafeStatistic( gamestatsuploader->IsCyberCafeUser() );
	}
}

void CBaseGameStats::Event_LevelShutdown( float flElapsed )
{
	BasicGameStatsRecord_t *map = m_BasicStats.FindOrAddRecordForMap( CBGSDriver.m_PrevMapName.c_str() );
	contract_assert( map );
	map->m_nSeconds += (int)flElapsed;
	gamestats->m_BasicStats.m_Summary.m_nSeconds += (int)flElapsed;

	StatsLog( "CBaseGameStats::Event_LevelShutdown [%s] %.2f elapsed %d total\n", CBGSDriver.m_PrevMapName.c_str(), flElapsed, gamestats->m_BasicStats.m_Summary.m_nSeconds );
}

void CBaseGameStats::Event_SaveGame( void )
{
	StatsLog( "CBaseGameStats::Event_SaveGame [%s]\n", CBGSDriver.m_PrevMapName.c_str() );
}

void CBaseGameStats::Event_LoadGame( void )
{
	char const *pchSaveFile = engine->GetMostRecentlyLoadedFileName();
	StatsLog( "CBaseGameStats::Event_LoadGame [%s] from %s\n", CBGSDriver.m_PrevMapName.c_str(), pchSaveFile );
}

void CBaseGameStats::Event_PlayerKilled( CBasePlayer *pPlayer, const CTakeDamageInfo &info )
{
	++m_BasicStats.m_Summary.m_nDeaths;

	if( CBGSDriver.m_bInLevel )
	{
		BasicGameStatsRecord_t *map = m_BasicStats.FindOrAddRecordForMap( CBGSDriver.m_PrevMapName.c_str() );
		++map->m_nDeaths;
		StatsLog( "  Player died %dth time in level [%s]!!!\n", map->m_nDeaths, CBGSDriver.m_PrevMapName.c_str() );
	}
	else
	{
		StatsLog( "  Player died, but not in a level!!!\n" );
		contract_assert( 0 );
	}

	StatsLog( "CBaseGameStats::Event_PlayerKilled [%s] [%dth death]\n", pPlayer->GetPlayerName(), m_BasicStats.m_Summary.m_nDeaths );
}

void CBaseGameStats::Event_Commentary()
{
	if( CBGSDriver.m_bInLevel )
	{
		BasicGameStatsRecord_t *map = m_BasicStats.FindOrAddRecordForMap( CBGSDriver.m_PrevMapName.c_str() );
		++map->m_nCommentary;
	}

	++m_BasicStats.m_Summary.m_nCommentary;

	StatsLog( "CBaseGameStats::Event_Commentary [%d]\n", m_BasicStats.m_Summary.m_nCommentary );
}

void CBaseGameStats::Event_Credits()
{
	StatsLog( "CBaseGameStats::Event_Credits\n" );

	float elapsed = 0.0f;
	if( CBGSDriver.m_bInLevel )
	{
		elapsed = gpGlobals->realtime - CBGSDriver.m_flLevelStartTime;
	}

	if( elapsed < 0.0f )
	{
		contract_assert( 0 );
		Warning( "EVENT_CREDITS with negative elapsed time (rt %f starttime %f)\n", gpGlobals->realtime, CBGSDriver.m_flLevelStartTime );
		elapsed = 0.0f;
	}

	// Only set this one time!!!
	if( gamestats->m_BasicStats.m_nSecondsToCompleteGame == 0 )
	{
		if( gamestats->UserPlayedAllTheMaps() )
		{
			gamestats->m_BasicStats.m_nSecondsToCompleteGame = elapsed + gamestats->m_BasicStats.m_Summary.m_nSeconds;
			gamestats->SaveToFileNOW();
		}
	}
}

void CBaseGameStats::Event_CrateSmashed()
{
	StatsLog( "CBaseGameStats::Event_CrateSmashed\n" );
}

void CBaseGameStats::Event_Punted( CBaseEntity *pObject )
{
	StatsLog( "CBaseGameStats::Event_Punted [%s]\n", pObject->GetClassname() );
}

void CBaseGameStats::Event_PlayerTraveled( CBasePlayer *pBasePlayer, float distanceInInches, bool bInVehicle, bool bSprinting )
{
}



// Called before .sav file is actually loaded (player should still be in previous level, if any)
void CBaseGameStats::Event_PreSaveGameLoaded( char const *pSaveName, bool bInGame )
{
	StatsLog( "CBaseGameStats::Event_PreSaveGameLoaded [%s] %s\n", pSaveName, bInGame ? "in-game" : "at console" );
}

bool CBaseGameStats::SaveToFileNOW( bool bForceSyncWrite /* = false */ )
{
	if ( !StatsTrackingIsFullyEnabled() )
		return false;

	// this code path is only for old format stats.  Products that use new format take a different path.
	if ( !gamestats->UseOldFormat() )
		return false;

	ks::byte_writer buf;
	buf.write< short >( GAMESTATS_FILE_VERSION );
	buf.write_bytes( s_szPseudoUniqueID, 16 );

	if( ShouldTrackStandardStats() )
		m_BasicStats.SaveToBuffer( buf ); 
	else
		buf.write< int >( GAMESTATS_STANDARD_NOT_SAVED );

	gamestats->AppendCustomDataToSaveBuffer( buf );

	char fullpath[ 512 ] = { 0 };
	if ( filesystem->FileExists( GetStatSaveFileName(), GAMESTATS_PATHID ) )
	{
		filesystem->RelativePathToFullPath( GetStatSaveFileName(), GAMESTATS_PATHID, fullpath, sizeof( fullpath ) );
	}
	else
	{
		// filename is local to the game dir, so we need to prepend game dir for regular file save
		char gamePath[256];
		engine->GetGameDir( gamePath, 256 );
		V_StripTrailingSlash( gamePath );
		V_snprintf( fullpath, sizeof( fullpath ), "%s/%s", gamePath, GetStatSaveFileName() );
		V_strlower( fullpath );
		V_FixSlashes( fullpath );
	}

	// StatsLog( "SaveToFileNOW '%s'\n", fullpath );

	if( CBGSDriver.m_bShuttingDown || bForceSyncWrite ) //write synchronously
	{
		filesystem->WriteFile( fullpath, GAMESTATS_PATHID, buf.bytes() );

		StatsLog( "Shut down wrote to '%s'\n", fullpath );
	}
	else
	{
		std::size_t nBufferSize = buf.size();
		void *pMem = new char[nBufferSize];
		memcpy( pMem, buf.data(), nBufferSize );

		// Write data async
		filesystem->AsyncWrite( fullpath, pMem, nBufferSize, true, false );
	}

	return true;
}

void CBaseGameStats::Event_PlayerConnected( CBasePlayer *pBasePlayer )
{
	StatsLog( "CBaseGameStats::Event_PlayerConnected [%s]\n", pBasePlayer->GetPlayerName() );
}

void CBaseGameStats::Event_PlayerDisconnected( CBasePlayer *pBasePlayer )
{
	StatsLog( "CBaseGameStats::Event_PlayerDisconnected [%s]\n", pBasePlayer->GetPlayerName() );
}

void CBaseGameStats::Event_PlayerDamage( CBasePlayer *pBasePlayer, const CTakeDamageInfo &info )
{
	//StatsLog( "CBaseGameStats::Event_PlayerDamage [%s] took %.2f damage\n", pBasePlayer->GetPlayerName(), info.GetDamage() );
}

void CBaseGameStats::Event_PlayerKilledOther( CBasePlayer *pAttacker, CBaseEntity *pVictim, const CTakeDamageInfo &info )
{
	StatsLog( "CBaseGameStats::Event_PlayerKilledOther [%s] killed [%s]\n", pAttacker->GetPlayerName(), pVictim->GetClassname() );
}

void CBaseGameStats::Event_WeaponFired( CBasePlayer *pShooter, bool bPrimary, char const *pchWeaponName )
{
	StatsLog( "CBaseGameStats::Event_WeaponFired [%s] %s weapon [%s]\n", pShooter->GetPlayerName(), bPrimary ? "primary" : "secondary", pchWeaponName );
}

void CBaseGameStats::Event_WeaponHit( CBasePlayer *pShooter, bool bPrimary, char const *pchWeaponName, const CTakeDamageInfo &info )
{
	StatsLog( "CBaseGameStats::Event_WeaponHit [%s] %s weapon [%s] damage [%f]\n", pShooter->GetPlayerName(), bPrimary ? "primary" : "secondary", pchWeaponName, info.GetDamage() );
}

void CBaseGameStats::Event_PlayerEnteredGodMode( CBasePlayer *pBasePlayer )
{
	StatsLog( "CBaseGameStats::Event_PlayerEnteredGodMode [%s] entered GOD mode\n", pBasePlayer->GetPlayerName() );
}

void CBaseGameStats::Event_PlayerEnteredNoClip( CBasePlayer *pBasePlayer )
{
	StatsLog( "CBaseGameStats::Event_PlayerEnteredNoClip [%s] entered NOCLIPe\n", pBasePlayer->GetPlayerName() );
}

void CBaseGameStats::Event_DecrementPlayerEnteredNoClip( CBasePlayer *pBasePlayer )
{
	StatsLog( "CBaseGameStats::Event_DecrementPlayerEnteredNoClip [%s] decrementing NOCLIPe\n", pBasePlayer->GetPlayerName() );
}

void CBaseGameStats::Event_IncrementCountedStatistic( const Vector& vecAbsOrigin, char const *pchStatisticName, float flIncrementAmount )
{
	StatsLog( "Incrementing %s by %f at pos (%d, %d, %d)\n", pchStatisticName, flIncrementAmount, (int)vecAbsOrigin.x, (int)vecAbsOrigin.y, (int)vecAbsOrigin.z );
}

// [dwenger] Needed for CS window-breaking stat
void CBaseGameStats::Event_WindowShattered( CBasePlayer *pPlayer )
{
	StatsLog( "In Event_WindowShattered\n" );
}

bool CBaseGameStats::UploadStatsFileNOW( void )
{
	if( !StatsTrackingIsFullyEnabled() || !HaveValidData() || !gamestats->UseOldFormat() )
		return false;

	if ( !filesystem->FileExists( gamestats->GetStatSaveFileName(), GAMESTATS_PATHID ) )
	{
		return false;
	}

	int curtime = Plat_FloatTime();

	CBGSDriver.m_tLastUpload = curtime;

	// Update the registry
#ifndef DEDICATED
	IRegistry *reg = InstanceRegistry( "Steam" );
	contract_assert( reg );
	reg->WriteInt( GetStatUploadRegistryKeyName(), CBGSDriver.m_tLastUpload );
	ReleaseInstancedRegistry( reg );
#endif

	auto blob = filesystem->ReadFile( GetStatSaveFileName(), GAMESTATS_PATHID );
	unsigned int uBlobSize = blob ? blob->size() : 0;
	if ( uBlobSize == 0 )
	{
		return false;
	}

	const void *pvBlobData = ( const void * )blob->data();

	if( gamestatsuploader )
	{
		return gamestatsuploader->UploadGameStats( "",
												   1,
												   uBlobSize,
												   pvBlobData );
	}

	return false;
}

void CBaseGameStats::LoadingEvent_PlayerIDDifferentThanLoadedStats( void )
{
	StatsLog( "CBaseGameStats::LoadingEvent_PlayerIDDifferentThanLoadedStats\n" );
}

bool CBaseGameStats::LoadFromFile( void )
{
	if ( filesystem->FileExists( gamestats->GetStatSaveFileName(), GAMESTATS_PATHID ) )
	{
		char fullpath[ 512 ];
		filesystem->RelativePathToFullPath( gamestats->GetStatSaveFileName(), GAMESTATS_PATHID, fullpath, sizeof( fullpath ) );
		StatsLog( "Loading stats from '%s'\n", fullpath );
	}
	
	if ( auto fileData = filesystem->ReadFile( gamestats->GetStatSaveFileName(), GAMESTATS_PATHID ) )
	{
		ks::byte_reader buf( *fileData );
		bool bRetVal = true;

		int version = buf.read< short >();
		if ( version > GAMESTATS_FILE_VERSION )
			return false; //file is beyond our comprehension

		// Set global parse version
		CBGSDriver.m_iLoadedVersion = version;

		buf.read_bytes( CBGSDriver.m_szLoadedUserID, 16 );
		CBGSDriver.m_szLoadedUserID[ sizeof( CBGSDriver.m_szLoadedUserID ) - 1 ] = 0;

		if ( s_szPseudoUniqueID[ 0 ] != 0 )
		{			
			if ( V_stricmp( CBGSDriver.m_szLoadedUserID, s_szPseudoUniqueID ) )
			{
				//UserID changed, blow away log!!!
				filesystem->RemoveFile( gamestats->GetStatSaveFileName(), GAMESTATS_PATHID );
				filesystem->RemoveFile( GAMESTATS_LOG_FILE, GAMESTATS_PATHID );
				Warning( "Userid changed, clearing stats file\n" );
				CBGSDriver.m_szLoadedUserID[0] = '\0';
				CBGSDriver.m_iLoadedVersion = -1;
				gamestats->m_BasicStats.Clear();
				gamestats->LoadingEvent_PlayerIDDifferentThanLoadedStats();
				bRetVal = false;
			}
		
			if ( version <= GAMESTATS_FILE_VERSION_OLD5 )
			{
				gamestats->m_BasicStats.Clear();
				bRetVal = false;
			}
			else
			{
				// Peek ahead in buffer to see if we have the "no default stats" secret flag set.
				int iCheckForStandardStatsInFile = buf.peek< int >();
				bool bValid = true;

				if ( iCheckForStandardStatsInFile != GAMESTATS_STANDARD_NOT_SAVED )
				{
					//the GAMESTATS_STANDARD_NOT_SAVED flag coincides with user completion time, rewind so the gamestats parser can grab it
					bValid = gamestats->m_BasicStats.ParseFromBuffer( buf, version );
				}
				else
				{
					// skip over the flag
					buf.read< int >();
				}

				if( !bValid )
				{
					m_BasicStats.Clear();
				}

				if( buf.remaining() != 0 ) //more data left, must be custom data
				{
					gamestats->LoadCustomDataFromBuffer( buf );
				}
			}
		}

		return bRetVal;
	}
	else
	{
		filesystem->RemoveFile( GAMESTATS_LOG_FILE, GAMESTATS_PATHID );
	}

	return false;	
}

bool CBaseGameStats_Driver::Init()
{
	const char *pGameDir = CommandLine()->ParmValue( "-game", "hl2" );

	//standardizing is a good thing
	char szLoweredGameDir[256];
	V_strncpy( szLoweredGameDir, pGameDir, sizeof( szLoweredGameDir ) );
	V_strlower( szLoweredGameDir );

	gamestats = gamestats->OnInit( gamestats, szLoweredGameDir );

	//determine constant strings needed for saving and uploading
	V_strncpy( s_szSaveFileName, szLoweredGameDir, sizeof( s_szSaveFileName ) );
	V_strncat( s_szSaveFileName, "_gamestats.dat", sizeof( s_szSaveFileName ) );

	V_strncpy( s_szStatUploadRegistryKeyName, "GameStatsUpload_", sizeof( s_szStatUploadRegistryKeyName ) );
	V_strncat( s_szStatUploadRegistryKeyName, szLoweredGameDir, sizeof( s_szStatUploadRegistryKeyName ) );

	gamestats->m_bLoggingToFile = CommandLine()->FindParm( "-gamestatsloggingtofile" ) ? true : false;
	gamestats->m_bLogging = CommandLine()->FindParm( "-gamestatslogging" ) ? true : false;

	if ( gamestatsuploader )
	{
		m_bEnabled = gamestatsuploader->IsGameStatsLoggingEnabled();
		if ( m_bEnabled )
		{
			gamestatsuploader->GetPseudoUniqueId( s_szPseudoUniqueID, sizeof( s_szPseudoUniqueID ) );
		}
	}

	ResetData();

	if ( StatsTrackingIsFullyEnabled() )
	{
		// FIXME: Load m_tLastUpload from registry and save it back out, too
#ifndef DEDICATED
		IRegistry *reg = InstanceRegistry( "Steam" );
		contract_assert( reg );
		m_tLastUpload = reg->ReadInt( gamestats->GetStatUploadRegistryKeyName(), 0 );
		ReleaseInstancedRegistry( reg );
#endif
		//load existing stats
		gamestats->LoadFromFile();
	}
		
	if ( s_szPseudoUniqueID[ 0 ] != 0 )
	{			
		gamestats->Event_Init();
		if ( gamestats->UseOldFormat() )
		{
			if( gamestats->AutoSave_OnInit() )
				gamestats->SaveToFileNOW();

			if( gamestats->AutoUpload_OnInit() )
				gamestats->UploadStatsFileNOW();
		}
	}
	else
	{
		m_bEnabled = false; //unable to generate a pseudo-unique ID, disable tracking
	}

	return true;
}

void CBaseGameStats_Driver::Shutdown()
{
	m_bShuttingDown = true;

	gamestats->Event_Shutdown();

	if ( gamestats->UseOldFormat() )
	{
		if( gamestats->AutoSave_OnShutdown() )
			gamestats->SaveToFileNOW();

		if( gamestats->AutoUpload_OnShutdown() )
			gamestats->UploadStatsFileNOW();
	}
	else
	{
		// code path for new format game stats
		if ( gamestats->ShouldSendDataOnAppShutdown() )
		{
			CollectData( STATSEND_APPSHUTDOWN );
			SendData();
		}
	}
	if ( FILESYSTEM_INVALID_HANDLE != g_LogFileHandle )
	{
		filesystem->Close( g_LogFileHandle );
		g_LogFileHandle = FILESYSTEM_INVALID_HANDLE;
	}

	if ( m_pGamestatsData != nullptr )
	{
		delete m_pGamestatsData;
		m_pGamestatsData = nullptr;
	}
}

void CBaseGameStats_Driver::PossibleMapChange( void )
{
	//detect and copy map changes
	if ( V_stricmp( m_PrevMapName.c_str(), STRING( gpGlobals->mapname ) ) )
	{

		std::string PrevMapBackup = m_PrevMapName;

		m_PrevMapName = STRING( gpGlobals->mapname );

		gamestats->Event_MapChange( PrevMapBackup.c_str(), STRING( gpGlobals->mapname ) );

		if ( gamestats->UseOldFormat() )
		{
			if( gamestats->AutoSave_OnMapChange() )
				gamestats->SaveToFileNOW();

			if( gamestats->AutoUpload_OnMapChange() )
				gamestats->UploadStatsFileNOW();
		}
	}
}

void CBaseGameStats_Driver::LevelInitPreEntity()
{
	m_bInLevel = true;
	m_bFirstLevel = false;

	if ( V_stricmp( s_szPseudoUniqueID, "unknown" ) == 0 )
	{
		// "unknown" means this is a dedicated server and we weren't able to generate a unique ID (e.g. Linux server).
		// Change the unique ID to be a hash of IP & port.  We couldn't do this earlier because IP is not known until level
		// init time.
		ConVar *hostip = cvar->FindVar( "hostip" );
		ConVar *hostport = cvar->FindVar( "hostport" );
		if ( hostip && hostport )
		{
			int crcInput[2];
			crcInput[0] = hostip->GetInt();
			crcInput[1] = hostport->GetInt();
			if ( crcInput[0] && crcInput[1] )
			{
				CRC32_t crc = CRC32_ProcessSingleBuffer( crcInput, sizeof( crcInput ) );
				V_snprintf( s_szPseudoUniqueID, std::size( s_szPseudoUniqueID ), "H:%x", ( int ) crc );
			}
		}
	}

	PossibleMapChange();

	m_flPauseStartTime = 0.0f;
	m_flLevelStartTime = gpGlobals->realtime;

	gamestats->Event_LevelInit();

	if ( gamestats->UseOldFormat() )
	{
		if( gamestats->AutoSave_OnLevelInit() )
			gamestats->SaveToFileNOW();

		if( gamestats->AutoUpload_OnLevelInit() )
			gamestats->UploadStatsFileNOW();
	}
}

void CBaseGameStats_Driver::LevelShutdownPreEntity()
{
	float flElapsed = gpGlobals->realtime - m_flLevelStartTime;

	if ( flElapsed < 0.0f )
	{
		contract_assert( 0 );
		Warning( "EVENT_LEVELSHUTDOWN:  with negative elapsed time (rt %f starttime %f)\n", gpGlobals->realtime, m_flLevelStartTime );
		flElapsed = 0.0f;
	}

	//Assert( m_bInLevel ); //so, apparently shutdowns can happen before inits

	if ( m_bInLevel && ( gpGlobals->eLoadType != MapLoad_Background ) )
	{
		m_flTotalTimeInLevels += flElapsed;
		m_iNumLevels ++;

		gamestats->Event_LevelShutdown( flElapsed );

		if ( gamestats->UseOldFormat() )
		{
			if( gamestats->AutoSave_OnLevelShutdown() )
				gamestats->SaveToFileNOW( true );

			if( gamestats->AutoUpload_OnLevelShutdown() )
				gamestats->UploadStatsFileNOW();
		}
		else
		{
			// code path for new format game stats
			CollectData( STATSEND_LEVELSHUTDOWN );
			if ( gamestats->ShouldSendDataOnLevelShutdown() )
			{
				SendData();
			}	
		}
		m_bInLevel = false;	
	}
}

void CBaseGameStats_Driver::OnSave()
{
	gamestats->Event_SaveGame();
}

void CBaseGameStats_Driver::CollectData( StatSendType_t sendType )
{

	// The base stat system holds memory until "APPSHUTDOWN" which on the Xbox isn't good.
	// When on the Xbox, neither server nor clients collect stats, but the base system still does.

	CGamestatsData *pGamestatsData = nullptr;
	// for server, check with the engine to see if there already a gamestats data container registered.  (There will be if there is a client
	// running in the same process.)
	pGamestatsData = static_cast<CGamestatsData *>( engine->GetGamestatsData() );
	if ( pGamestatsData )
	{
		// use the registered gamestats container, so free the one we allocated
		if ( m_pGamestatsData != nullptr )
		{
			delete m_pGamestatsData;
			m_pGamestatsData = nullptr;
		}
	}
	else
	{
		pGamestatsData = m_pGamestatsData;
	}
	contract_assert( pGamestatsData || //valid stats
			(!gpGlobals->IsClient() && (sendType == STATSEND_APPSHUTDOWN) && gamestats->ShouldSendDataOnLevelShutdown()) ); //or already sent off stats during level shutdown (now in app shutdown)
    
    // If a server is told to send stats on STATSEND_APPSHUTDOWN, we need to check to make sure we still have a valid object.
	if ( !pGamestatsData )
		return;

	KeyValues *pKV = pGamestatsData->m_pKVData;

	int iAppID = engine->GetAppID();
	pKV->SetInt( "appid", iAppID );

	switch ( sendType )
	{
	case STATSEND_LEVELSHUTDOWN:
		{
			// make a map node in the KeyValues to use for this level
			char szMap[MAX_PATH+1]="";
			V_strncpy( szMap, gpGlobals->mapname.ToCStr(), std::size( szMap ) );
			if ( !szMap[0] )
			{
				V_FileBase( MapName(), szMap, std::size( szMap ) );
			}

			KeyValues *pKVMap = new KeyValues( "map" );
			pKV->AddSubKey( pKVMap );
			pKVMap->SetString( "mapname", szMap );
			pKV = pKVMap;

		}
		break;
	case STATSEND_APPSHUTDOWN:
		break;
	default:
		contract_assert( false );
		break;
	}

	// add common data
	pGamestatsData->m_bHaveData |= AddBaseDataForSend( pKV, sendType );
    
	// add game-specific data
	pGamestatsData->m_bHaveData |= gamestats->AddDataForSend( pKV, sendType );
}
ConVar gamestats_file_output_directory( side_registry(), "gamestats_file_output_directory", "", FCVAR_NONE, "When -gamestatsfileoutputonly is specified, file will be emitted here instead of to modpath\n" );
void CBaseGameStats_Driver::SendData()
{
	// if we don't own the data container or there's no valid data, nothing to do
	if ( !m_pGamestatsData || !m_pGamestatsData->m_bHaveData )
		return;

	// save the data to a buffer
	ks::text_writer buf;
	m_pGamestatsData->m_pKVData->RecursiveSaveToFile( buf, 0 );

	const bool bOutDirOverriden = gamestats_file_output_directory.GetRawValue().m_StringLength > 1;
	const char *szOverrideDirectory = gamestats_file_output_directory.GetString();
	if ( CommandLine()->FindParm( "-gamestatsfileoutputonly" ) || bOutDirOverriden )
	{
		// write file for debugging
		if ( !bOutDirOverriden ) // if there isn't an override path, put it in the game dir.
		{
			const char szFileName[] = "gamestats.dat";
			filesystem->WriteFile( szFileName, GAMESTATS_PATHID, buf.bytes() );
		}
		else
		{
			char szPathName[MAX_PATH] = {0};
			V_strncat( szPathName, szOverrideDirectory, MAX_PATH );
			V_AppendSlash( szPathName, MAX_PATH );
			int pathLen = V_strlen( szPathName );
			
			char szFileName[MAX_PATH] = {0};
			struct tm timeinfo;
			Plat_GetLocalTime( &timeinfo );
			int fileLen = V_snprintf( szFileName, MAX_PATH, "%s__%2d_%3d_%2d_%2d_%2d.txt", s_szPseudoUniqueID, 
				timeinfo.tm_year % 100, timeinfo.tm_yday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec );

			contract_assert( pathLen + fileLen < MAX_PATH );	fileLen;

			V_strncat( szPathName + pathLen, szFileName, MAX_PATH - pathLen );

			filesystem->WriteFile( szPathName, nullptr, buf.bytes() );
		}
	}
	else
	{
		if ( gamestatsuploader )
			gamestatsuploader->UploadGameStats( "", 1, buf.size(), buf.c_str() );
	}	

	ResetData();
}

extern ConVar closecaption;
bool CBaseGameStats_Driver::AddBaseDataForSend( KeyValues *pKV, StatSendType_t sendType )
{
	switch ( sendType )
	{
	case STATSEND_APPSHUTDOWN:
		break;
	case STATSEND_LEVELSHUTDOWN:
		break;
	}

	return false;
}

void CBaseGameStats_Driver::ResetData()
{
	// on the server, if there is a gamestats data container registered (by a client in the same process), they're in charge of resetting it, nothing for us to do
	if ( engine->GetGamestatsData() != nullptr )
		return;

	if ( m_pGamestatsData != nullptr )
	{
		delete m_pGamestatsData;
		m_pGamestatsData = nullptr;
	}

	m_bBufferFull = false;
	m_nWriteIndex = 0;
	m_bDidVoiceChat = false;

	// Reset the CPU throttling statistics.
	m_nCPUUnthrottledCount = 0;
	m_nCPUThrottle1Count = 0;
	m_nCPUThrottle2Count = 0;
	// Track when we last received data.
	m_lastCPUFrequencyTimestamp = 0.0;

	m_pGamestatsData = new CGamestatsData();
	KeyValues *pKV = m_pGamestatsData->m_pKVData;
	pKV->SetInt( "IsPc", true );
	pKV->SetInt( "version", GAMESTATS_VERSION );
	pKV->SetString( "srcid", s_szPseudoUniqueID );

}

void CBaseGameStats_Driver::OnRestore()
{
	PossibleMapChange();

	gamestats->Event_LoadGame();
}

void CBaseGameStats_Driver::FrameUpdatePostEntityThink()
{
	bool bGamePaused = ( gpGlobals->frametime == 0.0f );

	if ( !m_bInLevel )
	{
		m_flPauseStartTime = 0.0f;
	}
	else if ( m_bGamePaused != bGamePaused )
	{
		if ( bGamePaused )
		{
			m_flPauseStartTime = gpGlobals->realtime;
		}
		else if ( m_flPauseStartTime != 0.0f )
		{
			float flPausedTime = gpGlobals->realtime - m_flPauseStartTime;
			if ( flPausedTime < 0.0f )
			{
				contract_assert( 0 );
				Warning( "Game paused time showing up negative (rt %f pausestart %f)\n", gpGlobals->realtime, m_flPauseStartTime );
				flPausedTime = 0.0f;
			}

			// Remove this from global time counters

			//			Msg( "Pause:  adding %f to level starttime\n", flPausedTime );

			m_flLevelStartTime += flPausedTime;
			m_flPauseStartTime = 0.0f;

			//			Msg( "Paused for %.2f seconds\n", flPausedTime );
		}
		m_bGamePaused = bGamePaused;
	}
}

bool StatsTrackingIsFullyEnabled( void )
{
	return CBGSDriver.m_bEnabled && gamestats->StatTrackingEnabledForMod();
}

void CBaseGameStats::Clear( void )
{
	gamestats->m_BasicStats.Clear();
}

//-----------------------------------------------------------------------------
// Nukes any dangerous characters and replaces w/space char
//-----------------------------------------------------------------------------
void OverWriteCharsWeHate( char *pStr )
{
	while( *pStr )
	{
		switch( *pStr )
		{
		case '\n':
		case '\r':
		case '\\':
		case '\"':
		case '\'':
		case '\032':
		case ';':
			*pStr = ' ';
		}
		pStr++;
	}
}

void CBaseGameStats::SetCyberCafeStatistic( bool bIsCyberCafeUser )
{
	if( CBGSDriver.m_bFirstLevel )
	{
		m_BasicStats.m_Summary.m_bCyberCafe = bIsCyberCafeUser;
	}

	if( CBGSDriver.m_bInLevel )
	{
		BasicGameStatsRecord_t *map = m_BasicStats.FindOrAddRecordForMap( CBGSDriver.m_PrevMapName.c_str() );
		map->m_bCyberCafe = bIsCyberCafeUser;
	}

	m_BasicStats.m_bCyberCafe = bIsCyberCafeUser;
}

void CBaseGameStats::SetHDRStatistic( bool bHDREnabled )
{
	if( bHDREnabled )
	{
		if( CBGSDriver.m_bInLevel )
		{
			BasicGameStatsRecord_t *map = m_BasicStats.FindOrAddRecordForMap( CBGSDriver.m_PrevMapName.c_str() );
			++map->m_nHDR;
		}

		if( CBGSDriver.m_bFirstLevel )
		{
			++m_BasicStats.m_Summary.m_nHDR;
		}
	}
}

void CBaseGameStats::SetCaptionsStatistic( bool bClosedCaptionsEnabled )
{
	if( CBGSDriver.m_bInLevel )
	{
		BasicGameStatsRecord_t *map = m_BasicStats.FindOrAddRecordForMap( CBGSDriver.m_PrevMapName.c_str() );
		++map->m_nCaptions;
	}

	if( CBGSDriver.m_bFirstLevel )
	{
		++m_BasicStats.m_Summary.m_nCaptions;
	}
}

void CBaseGameStats::SetSkillStatistic( int iSkillSetting )
{
	int skill = clamp( iSkillSetting, 1, 3 ) - 1;

	if( CBGSDriver.m_bInLevel )
	{
		BasicGameStatsRecord_t *map = m_BasicStats.FindOrAddRecordForMap( CBGSDriver.m_PrevMapName.c_str() );
		++map->m_nSkill[ skill ];
	}

	if ( CBGSDriver. m_bFirstLevel )
	{
		++m_BasicStats.m_Summary.m_nSkill[ skill ];
	}
}

void CBaseGameStats::SetDXLevelStatistic( int iDXLevel )
{
	m_BasicStats.m_nDXLevel = iDXLevel;
}

static void CC_ResetGameStats( const CCommand &args )
{

	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	gamestats->Clear();
	gamestats->SaveToFileNOW();
	gamestats->StatsLog( "CC_ResetGameStats : Server cleared game stats\n" );
}

static ConCommand resetGameStats(side_registry(), "_resetgamestats", CC_ResetGameStats, "Erases current game stats and writes out a blank stats file", 0 );

class CPointGamestatsCounter : public CPointEntity
{
public:
	DECLARE_CLASS( CPointGamestatsCounter, CPointEntity );
	DECLARE_DATADESC();

	CPointGamestatsCounter();

protected:

	[[= ks::reflect::Input{ .name = "SetName", .type = FIELD_STRING } ]] void InputSetName( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Increment", .type = FIELD_FLOAT } ]] void InputIncrement( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );
private:

	[[= ks::reflect::Key{ .name = "Name" } ]] string_t		m_strStatisticName;
	bool			m_bDisabled;
};

IMPLEMENT_REFLECT_DATAMAP( CPointGamestatsCounter )

LINK_ENTITY_TO_CLASS( point_gamestats_counter, CPointGamestatsCounter )

CPointGamestatsCounter::CPointGamestatsCounter() :
	m_strStatisticName( NULL_STRING ),
	m_bDisabled( false )
{
}

//-----------------------------------------------------------------------------
// Purpose: Changes name of statistic
//-----------------------------------------------------------------------------
void CPointGamestatsCounter::InputSetName( inputdata_t &inputdata )
{
	m_strStatisticName = inputdata.value.StringID();
}

//-----------------------------------------------------------------------------
// Purpose: Changes name of statistic
//-----------------------------------------------------------------------------
void CPointGamestatsCounter::InputIncrement( inputdata_t &inputdata )
{
	if ( m_bDisabled )
		return;

	if ( NULL_STRING == m_strStatisticName )
	{
		DevMsg( 1, "CPointGamestatsCounter::InputIncrement:  No stat name specified for point_gamestats_counter @%f, %f, %f [ent index %d]\n",
			GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z, entindex() );
		return;
	}

	gamestats->Event_IncrementCountedStatistic( GetAbsOrigin(), STRING( m_strStatisticName ), inputdata.value.Float() );
}

void CPointGamestatsCounter::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}

void CPointGamestatsCounter::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &SaveBuffer - 
//			iLump - 
//			iLumpCount - 
//-----------------------------------------------------------------------------
bool CBaseGameStats::AppendLump( int nMaxLumpCount, ks::byte_writer &SaveBuffer, unsigned short iLump, unsigned short iLumpCount, std::size_t nSize, void *pData )
{
	// Verify the lump index.
	contract_assert( ( iLump > 0 ) && ( iLump < nMaxLumpCount ) );

	if ( !( ( iLump > 0 ) && ( iLump < nMaxLumpCount ) ) )
		return false;

	// Check to see if we have any elements to save.
	if ( iLumpCount <= 0 )
		return false;

	// Write the lump id and element count.
	SaveBuffer.write< unsigned short >( iLump );
	SaveBuffer.write< unsigned short >( iLumpCount );

	std::size_t nTotalSize = iLumpCount * nSize;
	SaveBuffer.write_bytes( pData, nTotalSize );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &LoadBuffer - 
//			&iLump - 
//			&iLumpCount - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseGameStats::GetLumpHeader( int nMaxLumpCount, ks::byte_reader &LoadBuffer, unsigned short &iLump, unsigned short &iLumpCount, bool bPermissive /*= false*/ )
{
	// Get the lump id and element count.
	iLump = LoadBuffer.read< unsigned short >();
	if ( !LoadBuffer.ok() )
	{
		// check for EOF
		return false;
	}
	iLumpCount = LoadBuffer.read< unsigned short >();

	if ( bPermissive )
		return true;

	// Verify the lump index.
	contract_assert( ( iLump > 0 ) && ( iLump < nMaxLumpCount ) );
	if ( !( ( iLump > 0 ) && ( iLump < nMaxLumpCount ) ) )
	{
		return false;
	}

	// Check to see if we have any elements to save.
	if ( iLumpCount <= 0 )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Loads 1 or more lumps of raw data
// Input  : &LoadBuffer - buffer to be read from
//			iLumpCount - # of lumps to read
//			nSize - size of each lump
//			pData - where to store the data
//-----------------------------------------------------------------------------
void CBaseGameStats::LoadLump( ks::byte_reader &LoadBuffer, unsigned short iLumpCount, std::size_t nSize, void *pData )
{
	LoadBuffer.read_bytes( pData, iLumpCount * nSize );
}

void CFpsHistogram::Update( uint8 nBin)
{
	m_nFps[ nBin & ( FPS_BIN_COUNT - 1 ) ]++;
	m_nCpuWaits[ nBin & ( FPS_BIN_COUNT - 1 ) ] += ( nBin >> 7 );
}

CFpsSelectiveHistogram::CFpsSelectiveHistogram()
{
	m_nLookAhead = 0;
	m_nLookBack = 0;
}

void CFpsSelectiveHistogram::Fire( int nLookAhead, int nLookBack, const CFpsHistory &history)
{
	// get the prerecorded stats
	if ( m_nLookAhead < nLookAhead )
	{
		m_nLookAhead = nLookAhead;
	}
	int nCatchUp = std::min( nLookBack, m_nLookBack );
	// exhaust back-stats buffer
	for ( int i = 0; i < nCatchUp; ++i )
	{
		CFpsHistogram::Update( history[ i ] );
	}

	m_nLookBack = 0;
}

void CFpsSelectiveHistogram::Update( uint8 nBin)
{
	if ( m_nLookAhead > 0 )
	{
		m_nLookAhead--;
		CFpsHistogram::Update( nBin );
	}
	else
	{
		++m_nLookBack;
	}
}

bool CBaseGameStats::ShouldSendDataOnAppShutdown()
{
	// by default, clients send data at app shutdown and servers don't
	return false;
}

#undef WALKED_AWAY_FROM_KEYBOARD_SECONDS

using namespace ::ks::reflect;

void BasicGameStatsRecord_t::Clear()
{
	m_nCount = 0;
	m_nSeconds = 0;
	m_nCommentary = 0;
	m_nHDR = 0;
	m_nCaptions = 0;
	m_bSteam = true;
	m_bCyberCafe = false;
	V_memset( m_nSkill, 0, sizeof( m_nSkill ) );
	m_nDeaths = 0;
}

void BasicGameStatsRecord_t::SaveToBuffer( ks::byte_writer &buf )
{
	buf.write< int >( m_nCount );
	buf.write< int >( m_nSeconds );
	buf.write< int >( m_nCommentary );
	buf.write< int >( m_nHDR );
	buf.write< int >( m_nCaptions );
	for ( int i = 0; i < 3; ++i )
	{
		buf.write< int >( m_nSkill[ i ] );
	}

	buf.write< char >( m_bSteam ? 1 : 0 );
	buf.write< char >( m_bCyberCafe ? 1 : 0 );
	buf.write< int >( m_nDeaths );
}

bool BasicGameStatsRecord_t::ParseFromBuffer( ks::byte_reader &buf, int iBufferStatsVersion )
{
	bool bret = true;
	m_nCount = buf.read< int >();

	if ( m_nCount > 100000 || m_nCount < 0 )
	{
		bret = false;
	}

	m_nSeconds = buf.read< int >();
	// Note, don't put the buf.read< int >() in the macro since it'll get evaluated twice!!!
	m_nSeconds = std::max( m_nSeconds, 0 );

	m_nCommentary = buf.read< int >();
	if ( m_nCommentary < 0 || m_nCommentary > 100000 )
	{
		bret = false;
	}

	m_nHDR = buf.read< int >();
	if ( m_nHDR < 0 || m_nHDR > 100000 )
	{
		bret = false;
	}

	m_nCaptions = buf.read< int >();
	if ( m_nCaptions < 0 || m_nCaptions > 100000 )
	{
		bret = false;
	}

	for ( int i = 0; i < 3; ++i )
	{
		m_nSkill[ i ] = buf.read< int >();
		if ( m_nSkill[ i ] < 0 || m_nSkill[ i ]  > 100000 )
		{
			bret = false;
		}
	}

	if ( iBufferStatsVersion > GAMESTATS_FILE_VERSION_OLD )
	{
		m_bSteam = buf.read< char >() ? true : false;
	}
	if ( iBufferStatsVersion > GAMESTATS_FILE_VERSION_OLD2 )
	{
		m_bCyberCafe = buf.read< char >() ? true : false;
	}
	if ( iBufferStatsVersion > GAMESTATS_FILE_VERSION_OLD5 )
	{
		m_nDeaths = buf.read< int >();
	}

	return bret;
}

void BasicGameStats_t::Clear()
{
	m_nSecondsToCompleteGame = 0;
	m_Summary.Clear();
	m_MapTotals.clear();
}

void BasicGameStats_t::SaveToBuffer( ks::byte_writer &buf )
{
	buf.write< int >( m_nSecondsToCompleteGame );

	m_Summary.SaveToBuffer( buf );

	int c = (int)m_MapTotals.size();
	buf.write< int >( c );
	for ( auto &[name, rec] : m_MapTotals )
	{
		buf.write_cstring( name );
		rec.SaveToBuffer( buf );
	}

	buf.write< char >( 0 );	
	buf.write< char >( m_bSteam ? 1 : 0 );
	buf.write< char >( m_bCyberCafe ? 1 : 0 );
	buf.write< short >( (short)m_nDXLevel );
}

BasicGameStatsRecord_t *BasicGameStats_t::FindOrAddRecordForMap( char const *mapname )
{
	return &m_MapTotals[ mapname ];
}

bool BasicGameStats_t::ParseFromBuffer( ks::byte_reader &buf, int iBufferStatsVersion )
{
	bool bret = true;

	m_nSecondsToCompleteGame = buf.read< int >();
	if ( m_nSecondsToCompleteGame < 0 || m_nSecondsToCompleteGame > 10000000 )
	{
		bret = false;
	}

	m_Summary.ParseFromBuffer( buf, iBufferStatsVersion );
	int c = buf.read< int >();
	if ( c > 1024 || c < 0 )
	{
		bret = false;
	}

	for ( int i = 0; i < c; ++i )
	{
		char mapname[ 256 ];
		buf.read_cstring( mapname, sizeof( mapname ) );

		BasicGameStatsRecord_t *rec = FindOrAddRecordForMap( mapname );
		bool valid= rec->ParseFromBuffer( buf, iBufferStatsVersion );
		if ( !valid )
		{
			bret = false;
		}
	}

	if ( iBufferStatsVersion >= GAMESTATS_FILE_VERSION_OLD2 )
	{
		buf.read< char >();		// Gets the obsolete hl2 unlocked chapter field
		m_bSteam = buf.read< char >() ? true : false;
	}
	if ( iBufferStatsVersion > GAMESTATS_FILE_VERSION_OLD2 )
	{
		m_bCyberCafe = buf.read< char >() ? true : false;
	}
	if ( iBufferStatsVersion > GAMESTATS_FILE_VERSION_OLD3 )
	{
		m_nDXLevel = (int)buf.read< short >();
	}
	return bret;
}

// Purpose: returns the module handle of the game dll
//			this is in its own file to protect it from tier0 PROTECTED_THINGS

void *GetGameModuleHandle()
{
	contract_assert( 0 );
	return nullptr; // NOT implemented
}
