// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include "client_class.h"
#include "eiface.h"
#include "igameevents.h"
#include "igameresources.h"
#include "inputsystem/iinputsystem.h"
#include "platforminputdevice.h"
#include "usermessages_schema.h"
#include "vcollide_parse.h"
#include "view_shared.h"
#include "baseentity.h"
#include "baseplayer.h"
#include "cdll_client_int.h"
#include "predictable_entity.h"
#include "reflect_predmap.h"
#include "reflect_recvtable.h"
#include "shareddefs.h"
#include "edict.h"
#include "engine/ivmodelinfo.h"
#include "iclient.h"
#include "networkvar.h"
#include "reflect_annotations.h"
#include "vphysics/player_controller.h"
#include "vphysics_interface.h"
#include "datacache/imdlcache.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "studio.h"
module ks.client;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.platform;
import ks.core.random;
import ks.core.threadtools;
import ks.core.vprof;
import ks.game.cellcoord;
import ks.game.in_buttons;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.math.vector2d;
import ks.platform;
import ks.simd;
import ks.tier1.bitbuf;
import ks.tier1.convar;
import ks.tier1.cvar;
import ks.tier1.iconvar;
import ks.tier1.icvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.interpolatedvar;
import ks.tier1.keyvalues;
import ks.tier1.playerid;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.strtools;
import :particlemgr;
import :iinput;
import :baseentity;
import :props;
import :baseanimatedtextureproxy;
import :sharedvar;
import :util;
import :cstrike15.cs_player;
import :cstrike15.cs_shareddefs;
import :gamevars_shared;
import :precache;
import :noopanimcontroller;
import ks.game.shareddefs;
import :achievements_and_stats_interface;
import :cstrike15.cs_player_shared;
import :econ.econ_item_schema_minimal;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.core.types;
import ks.core.wchartypes;
import ks.tier1.tracked_ref;
import ks.core.scratcharena;
import ks.math.transform;
import ks.tier1.exprevaluator;
import ks.tier1.vector_ops;
import ks.game.ai_activity;

class CNewParticleEffect;
class CUserCmd;
class CBaseAnimating;
class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CBaseViewModel;
class CBaseWeaponWorldModel;
class CCSPlayer;
class CColorCorrection;
class CPostProcessController;
class FileWeaponInfo_t;
class IRagdoll;

using namespace ::ks::reflect;

// Purpose: Client-side CBasePlayer.
//			- Manages the player's flashlight effect.

// Don't alias here

int g_nKillCamMode = OBS_MODE_NONE;
int g_nKillCamTarget1 = 0;
int g_nKillCamTarget2 = 0;

extern ConVar mp_forcecamera; // in gamevars_shared.h
extern ConVar r_mapextents;
extern ConVar voice_icons_method;
extern ConVar view_recoil_tracking;

static constexpr auto FLASHLIGHT_DISTANCE = 1000;
static constexpr auto MAX_VGUI_INPUT_MODE_SPEED = 30;
inline constexpr auto MAX_VGUI_INPUT_MODE_SPEED_SQ = (MAX_VGUI_INPUT_MODE_SPEED*MAX_VGUI_INPUT_MODE_SPEED);

static Vector WALL_MIN(-WALL_OFFSET,-WALL_OFFSET,-WALL_OFFSET);
static Vector WALL_MAX(WALL_OFFSET,WALL_OFFSET,WALL_OFFSET);

extern ConVar default_fov;
extern ConVar sensitivity;
extern ConVar voice_all_icons;

static CBasePlayer *s_pLocalPlayer[ MAX_SPLITSCREEN_PLAYERS ];

static ConVar	cl_customsounds ( side_registry(), "cl_customsounds", "0", 0, "Enable customized player sound playback" );
static ConVar	spec_track		( side_registry(), "spec_track", "0", 0, "Tracks an entity in spec mode" );
static ConVar	cl_smooth		( side_registry(), "cl_smooth", "1", 0, "Smooth view/eye origin after prediction errors" );
static ConVar	cl_smoothtime	( 
	side_registry(), "cl_smoothtime", 
	"0.1", 
	0, 
	"Smooth client's view after prediction error over this many seconds",
	true, 0.01,	// min/max is 0.01/2.0
	true, 2.0
	 );

ConVar	spec_freeze_time( side_registry(), "spec_freeze_time", "3.0", FCVAR_RELEASE | FCVAR_REPLICATED, "Time spend frozen in observer freeze cam." );
ConVar	spec_freeze_traveltime( side_registry(), "spec_freeze_traveltime", "0.3", FCVAR_RELEASE | FCVAR_REPLICATED, "Time taken to zoom in to frame a target in observer freeze cam.", true, 0.01, false, 0 );
ConVar	spec_freeze_traveltime_long( side_registry(), "spec_freeze_traveltime_long", "0.45", FCVAR_CHEAT | FCVAR_REPLICATED, "Time taken to zoom in to frame a target in observer freeze cam when they are far away.", true, 0.01, false, 0 );
ConVar	spec_freeze_distance_min( side_registry(), "spec_freeze_distance_min", "60", FCVAR_CHEAT, "Minimum random distance from the target to stop when framing them in observer freeze cam." );
ConVar	spec_freeze_distance_max( side_registry(), "spec_freeze_distance_max", "80", FCVAR_CHEAT, "Maximum random distance from the target to stop when framing them in observer freeze cam." );
ConVar  spec_freeze_panel_extended_time( side_registry(), "spec_freeze_panel_extended_time", "0.0", FCVAR_RELEASE | FCVAR_REPLICATED, "Time spent with the freeze panel still up after observer freeze cam is done." );
ConVar	spec_freeze_deathanim_time( side_registry(), "spec_freeze_deathanim_time", "0.8", FCVAR_RELEASE | FCVAR_REPLICATED, "The time that the death cam will spend watching the player's ragdoll before going into the freeze death cam." );
ConVar	spec_freeze_target_fov_long( side_registry(), "spec_freeze_target_fov_long", "90", FCVAR_CHEAT | FCVAR_REPLICATED, "The target FOV that the deathcam should use when the cam zoom far away on the target." );
ConVar	spec_freeze_target_fov( side_registry(), "spec_freeze_target_fov", "42", FCVAR_CHEAT | FCVAR_REPLICATED, "The target FOV that the deathcam should use." );

ConVar	cl_player_fullupdate_predicted_origin_fix( side_registry(), "cl_player_fullupdate_predicted_origin_fix", "1" );

ConVar spec_lock_to_accountid( side_registry(), "spec_lock_to_accountid", "", FCVAR_RELEASE, "As an observer, lock the spectate target to the given accountid." );

bool IsDemoPolishRecording();

// -------------------------------------------------------------------------------- //
// RecvTable for CPlayerState.
// -------------------------------------------------------------------------------- //

IMPLEMENT_REFLECT_TABLE( CPlayerState, DT_PlayerState );

IMPLEMENT_REFLECT_TABLE( CPlayerLocalData, DT_Local );

// -------------------------------------------------------------------------------- //
// This data only gets sent to clients that ARE this player entity.
// -------------------------------------------------------------------------------- //

IMPLEMENT_REFLECT_TABLE_IN( CBasePlayer, DT_LocalPlayerExclusive );

// -------------------------------------------------------------------------------- //
// DT_BasePlayer datatable.
// -------------------------------------------------------------------------------- //
	IMPLEMENT_REFLECT_CLIENTCLASS( CBasePlayer, DT_BasePlayer, CBasePlayer )

IMPLEMENT_REFLECT_PREDMAP_NO_BASE( CPlayerState );

IMPLEMENT_REFLECT_PREDMAP_NO_BASE( CPlayerLocalData );

IMPLEMENT_REFLECT_PREDMAP( CBasePlayer );

LINK_ENTITY_TO_CLASS( player, CBasePlayer );

// -------------------------------------------------------------------------------- //
// Functions.
// -------------------------------------------------------------------------------- //
// The movement core of a player is the player.
static CBasePlayer *Side( const CPlayerMoveCore *p )
{
	return const_cast<CBasePlayer *>( static_cast<const CBasePlayer *>( p ) );
}

CBasePlayer::CBasePlayer() : m_iv_vecViewOffset( "CBasePlayer::m_iv_vecViewOffset" )
{
	// Movement reaches this side through these. They sit in the constructor for its access to the
	// player's members.
	static constexpr PlayerMoveServices s_MoveServices{
		false, // bServer,
		[]( const CPlayerMoveCore *p ) -> CEntityCore * { return Side( p )->GetGroundEntity(); },
		[]( CPlayerMoveCore *p, CEntityCore *pGround ) { Side( p )->SetGroundEntity( static_cast<CBaseEntity *>( pGround ) ); },
		[]( CPlayerMoveCore *p, MoveType_t val ) { Side( p )->SetMoveType( val ); },
		[]( const CPlayerMoveCore *p ) -> const Vector & { return Side( p )->GetViewOffset(); },
		[]( CPlayerMoveCore *p, const Vector &v ) { Side( p )->SetViewOffset( v ); },
		[]( const CPlayerMoveCore *p ) { return Side( p )->IsObserver(); },
		[]( const CPlayerMoveCore *p ) { return Side( p )->GetObserverMode(); },
		[]( const CPlayerMoveCore *p ) -> CEntityCore * { return Side( p )->GetObserverTarget(); },
		[]( const CPlayerMoveCore *p ) { return Side( p )->CurrentCommandNumber(); },
		[]( CPlayerMoveCore *p, Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force ) { Side( p )->PlayStepSound( vecOrigin, psurface, fvol, force ); },
		[]( CPlayerMoveCore *p, surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity ) { Side( p )->UpdateStepSound( psurface, vecOrigin, vecVelocity ); },
		[]( const CPlayerMoveCore *p ) { return Side( p )->GetViewPunchAngle(); },
		[]( CPlayerMoveCore *p, const QAngle &angle ) { Side( p )->SetViewPunchAngle( angle ); },
		[]( const CPlayerMoveCore *p ) -> const Vector & { return Side( p )->m_vecLadderNormal.Get(); },
		[]( CPlayerMoveCore *p, const Vector &v ) { Side( p )->SetLadderNormal( v ); },
		[]( const CPlayerMoveCore *p ) { return (int)Side( p )->m_iHealth; },
		[]( const CPlayerMoveCore *p ) { return (int)Side( p )->m_nTickBase; },
		[]( const CPlayerMoveCore *p ) { return (bool)Side( p )->pl.deadflag; },
		[]( const CPlayerMoveCore *p ) { return Side( p )->GetLaggedMovementValue(); },
		[]( const CPlayerMoveCore *p ) { return Side( p )->IsBot(); },
		[]( CPlayerMoveCore *p ) { return Side( p )->GetPlayerMaxSpeed(); },
	};
	BindMoveCore( this, &m_Local, &s_MoveServices );
	AddVar( &m_vecViewOffset, &m_iv_vecViewOffset, LATCH_SIMULATION_VAR );
	
	m_hViewEntity = nullptr;
	m_bShouldDrawPlayerWhileUsingViewEntity = false;

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; i++ )
	{
		m_bFlashlightEnabled[ i ] = false;
	}

	m_pCurrentVguiScreen = nullptr;
	m_pCurrentCommand = nullptr;

	m_flPredictionErrorTime = -100;
	m_StuckLast = 0;
	m_bWasFrozen = false;

	ResetObserverMode();

	m_bActiveCameraMan = false;
	m_bCameraManXRay = false;
	m_bCameraManOverview = false;
	m_bCameraManScoreBoard = false;
	m_uCameraManGraphs = 0;
	m_bLastActiveCameraManState = false;
	m_bLastCameraManXRayState = false;
	m_bLastCameraManOverviewState = false;
	m_bLastCameraManScoreBoardState = false;
	m_uLastCameraManGraphsState = 0;

	m_iDeathPostEffect = 0;

	m_flTimeLastTouchedGround = 0.0f;
	m_vecPredictionError.Init();
	m_flPredictionErrorTime = 0;

	m_surfaceProps = 0;
	m_pSurfaceData = nullptr;
	m_surfaceFriction = 1.0f;
	m_chTextureType = 0;
	m_nSplitScreenSlot = 0; // When we are building without splitscreen all sorts of SFUI are using local player and might index arrays with negative index otherwise, but we know that only one player exists so just initialize for the correct slot!
	m_bIsLocalPlayer = false;
	m_afButtonForced = 0;
	m_bDisableSimulationFix = true;
	m_flNextAchievementAnnounceTime = 0;

	m_flFreezePanelExtendedStartTime = 0;
	m_bWasFreezePanelExtended = false;
	m_bStartedFreezeFrame = false;
	m_bAbortedFreezeFrame = false;

	m_bFiredWeapon = false;

	m_vecHack_RecvProxy_LocalPlayerOrigin.Init( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );
	m_ignoreLadderJumpTime = 0.0f;

	m_bPlayerIsTalkingOverVOIP = false;

	m_bCanShowFreezeFrameNow = false;
	m_nLastKillerDamageTaken = 0;
	m_nLastKillerHitsTaken = 0;
	m_nLastKillerDamageGiven = 0;
	m_nLastKillerHitsGiven = 0;

	// Init eye and angle offsets - used for TrackIR and motion controllers
	m_vecEyeOffset.Init();
	m_EyeAngleOffset.Init();
	m_AimDirection.Init();

	m_flDuckAmount = 0.0f;
	m_flDuckSpeed = CS_PLAYER_DUCK_SPEED_IDEAL;
	m_vecLastPositionAtFullCrouchSpeed = vec2_origin;

	m_bHasWalkMovedSinceLastJump = false;

	// Initialize physics pointers to NULL - these are created later in PostDataUpdate
	m_pPhysicsController = nullptr;
	m_pShadowStand = nullptr;
	m_pShadowCrouch = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
extern void UntouchAllTriggerSoundOperator( CBaseEntity *pEntity );
CBasePlayer::~CBasePlayer()
{
	if ( this == s_pLocalPlayer[ 0 ] )
	{
		UntouchAllTriggerSoundOperator( this );
	}

	// DeactivateVguiScreen removed (VGUI teardown)
	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		if ( this == s_pLocalPlayer[ i ] )
		{	
			s_pLocalPlayer[ i ] = nullptr;
		}
		else if ( s_pLocalPlayer[ i ] )
		{
			s_pLocalPlayer[ i ]->RemoveSplitScreenPlayer( this );
		}

		if ( m_bFlashlightEnabled[ i ] )
		{
			FlashlightEffectManager( i ).TurnOffFlashlight( true );
			m_bFlashlightEnabled[ i ] = false;
		}
	}
}

bool MsgFunc_SendLastKillerDamageToClient( const ks::net::CCSUsrMsg_SendLastKillerDamageToClient &msg )
{
	int nNumHitsGiven = msg.num_hits_given;
	int nDamageGiven = msg.damage_given;
	int nNumHitsTaken = msg.num_hits_taken;
	int nDamageTaken = msg.damage_taken;

	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		pPlayer->SetLastKillerDamageAndFreezeframe( nDamageTaken, nNumHitsTaken, nDamageGiven, nNumHitsGiven );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayer::Spawn( void )
{
	m_UMCMsg_SendLastKillerDamageToClient.Bind< ks::net::CS_UM_SendLastKillerDamageToClient, ks::net::CCSUsrMsg_SendLastKillerDamageToClient >
		( MsgFunc_SendLastKillerDamageToClient);

	// Clear all flags except for FL_FULLEDICT
	ClearFlags();
	AddFlag( FL_CLIENT );

	int effects = GetEffects() & EF_NOSHADOW;
	SetEffects( effects );

	m_iFOV	= 0;	// init field of view.

	SetModel( GetPlayerModelName() );

	Precache();

	SetThink(nullptr);

	SharedSpawn();

	m_bWasFreezeFraming = false;
	
	m_bFiredWeapon = false;
}

void CBasePlayer::UpdateOnRemove( void )
{

	if ( m_pPhysicsController )
	{
		physenv->DestroyPlayerController( m_pPhysicsController );
		m_pPhysicsController = nullptr;
	}
	PhysRemoveShadow( this );

	VPhysicsSetObject( nullptr );
	if( m_pShadowStand )
	{
		physenv->DestroyObject( m_pShadowStand );
		m_pShadowStand = nullptr;
	}

	if( m_pShadowCrouch )
	{
		physenv->DestroyObject( m_pShadowCrouch );
		m_pShadowCrouch = nullptr;
	}

	if ( m_speechVOIPParticleEffect.IsValid() )
	{
		ParticleProp()->StopEmissionAndDestroyImmediately( m_speechVOIPParticleEffect );
		m_speechVOIPParticleEffect = nullptr;
	}

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBasePlayer::AudioStateIsUnderwater( Vector vecMainViewOrigin )
{
	if ( IsObserver() )
	{
		// Just check the view position
		int cont = enginetrace->GetPointContents_WorldOnly( vecMainViewOrigin, MASK_WATER );
		return (cont & MASK_WATER) ? true : false;
	}

	return ( GetWaterLevel() >= WL_Eyes );
}

bool CBasePlayer::IsBot( void ) const 
{ 
	IGameResources* pGR = GameResources();

	return ( pGR && pGR->IsFakePlayer( entindex() ) );
}

CBaseEntity	*CBasePlayer::GetObserverTarget() const	// returns players targer or NULL
{
	if ( IsHLTV() )
	{
		return HLTVCamera()->GetPrimaryTarget();
	}
	
	return m_hObserverTarget;
}

void UpdateWorldmodelVisibility( CBasePlayer *player )
{
	for ( int i = 0; i < player->WeaponCount(); i++ )
	{
		CBaseCombatWeapon *pWeapon = player->GetWeapon(i);
		if ( pWeapon )
		{
			CBaseWeaponWorldModel *pWeaponWorldModel = pWeapon->GetWeaponWorldModel();
			if ( pWeaponWorldModel )
			{
				pWeaponWorldModel->UpdateVisibility();
			}
		}
	}
}

// Helper method to fix up visiblity across split screen for view models when observer target or mode changes
void UpdateViewmodelVisibility( CBasePlayer *player )
{
	// also update world models
	UpdateWorldmodelVisibility( player );

	// Update view model visibility
	for ( int i = 0; i < MAX_VIEWMODELS; i++ )
	{
		CBaseViewModel *vm = player->GetViewModel( i );
		if ( !vm )
			continue;
		vm->UpdateVisibility();
	}
}

// Called from Recv Proxy, mainly to reset tone map scale
void CBasePlayer::SetObserverTarget( EHANDLE hObserverTarget )
{
	// [msmith] We need to update the view model visibility status of the player we were observing because their view models
	// may no longer be rendering in our splitscreen viewport.
	CBasePlayer* pOldObserverTarget = ToBasePlayer( m_hObserverTarget );

	CBasePlayer *pNewObserverTarget = ToBasePlayer( hObserverTarget );

	// If the observer target is changing to an entity that the client doesn't know about yet,
	// it can resolve to NULL.  If the client didn't have an observer target before, then
	// comparing EHANDLEs directly will see them as equal, since it uses Get(), and compares
	// NULL to NULL.  To combat this, we need to check against GetEntryIndex() and
	// GetSerialNumber().
	if ( hObserverTarget.GetEntryIndex() != m_hObserverTarget.GetEntryIndex() ||
		hObserverTarget.GetSerialNumber() != m_hObserverTarget.GetSerialNumber())
	{
		// Init based on the new handle's entry index and serial number, so that it's Get()
		// has a chance to become non-NULL even if it currently resolves to NULL.
		m_hObserverTarget.Init( hObserverTarget.GetEntryIndex(), hObserverTarget.GetSerialNumber() );

		IGameEvent *event = gameeventmanager->CreateEvent( "spec_target_updated" );
		if ( event )
		{
			event->SetInt("userid", GetUserID() );
			gameeventmanager->FireEventClientSide( event );
		}

		if ( IsLocalPlayer( this ) )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );
			ResetToneMapping( -1.0f ); // This forces the tonemapping scalar to the average of min and max
		}
		UpdateViewmodelVisibility( this );
		UpdateVisibility();	

		if ( pNewObserverTarget )
		{
			pNewObserverTarget->UpdateVisibility();
			UpdateViewmodelVisibility( pNewObserverTarget );
		}
	}

	// [msmith] We need to wait until we've set a new observer target before updating the view model visibility of our
	// old observer target.
	// NOTE: We need to update this even if the observed target did not change because the observer mode may have changed.
	//       If the observer mode switched to third person for example, the view model should NOT be drawn.
	if ( pOldObserverTarget )
	{
		pOldObserverTarget->UpdateVisibility();
		UpdateViewmodelVisibility( pOldObserverTarget );
	}

}

int CBasePlayer::GetObserverMode() const 
{ 
	if ( IsHLTV() )
	{
		return HLTVCamera()->GetMode();
	}

	return m_iObserverMode; 
}

//-----------------------------------------------------------------------------
// Used by prediction, sets the view angles for the player
//-----------------------------------------------------------------------------
void CBasePlayer::SetLocalViewAngles( const QAngle &viewAngles )
{
	pl.v_angle = viewAngles;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ang - 
//-----------------------------------------------------------------------------
void CBasePlayer::SetViewAngles( const QAngle& ang )
{
	SetLocalAngles( ang );
	SetNetworkAngles( ang );
}

surfacedata_t* CBasePlayer::GetGroundSurface()
{
	//
	// Find the name of the material that lies beneath the player.
	//
	Vector start, end;
	VectorCopy( GetAbsOrigin(), start );
	VectorCopy( start, end );

	// Straight down
	end.z -= 64;

	// Fill in default values, just in case.
	
	Ray_t ray;
	ray.Init( start, end, GetPlayerMins(), GetPlayerMaxs() );

	trace_t	trace;
	UTIL_TraceRay( ray, MASK_PLAYERSOLID_BRUSHONLY, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );

	if ( trace.fraction == 1.0f )
		return nullptr;	// no ground
	
	return physprops->GetSurfaceData( trace.surface.surfaceProps );
}

//-----------------------------------------------------------------------------
// returns the player name
//-----------------------------------------------------------------------------
const char * CBasePlayer::GetPlayerName()
{
	return g_PR ? g_PR->GetPlayerName( entindex() ) : "";
}

//-----------------------------------------------------------------------------
// Is the player dead?
//-----------------------------------------------------------------------------
bool CBasePlayer::IsPlayerDead()
{
	return pl.deadflag == true;
}

//-----------------------------------------------------------------------------
// Purpose:


//-----------------------------------------------------------------------------
// Purpose: Store original ammo data to see what has changed
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void CBasePlayer::OnPreDataChanged( DataUpdateType_t updateType )
{
	for (int i = 0; i < MAX_AMMO_TYPES; ++i)
	{
		m_iOldAmmo[i] = GetAmmoCount(i);
	}

	m_bWasFreezeFraming = (GetObserverMode() == OBS_MODE_FREEZECAM);
	m_hOldFogController = m_PlayerFog.m_hCtrl;

	m_iOldObserverMode = GetObserverMode();

	BaseClass::OnPreDataChanged( updateType );
}

void CBasePlayer::PreDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PreDataUpdate( updateType );

	m_ubOldEFNoInterpParity = m_ubEFNoInterpParity;
}

void CBasePlayer::CheckForLocalPlayer( int nSplitScreenSlot )
{
	// Make sure s_pLocalPlayer is correct
	int iLocalPlayerIndex = ( nSplitScreenSlot != -1 ) ? engine->GetLocalPlayer() : 0;

	if ( g_nKillCamMode )
		iLocalPlayerIndex = g_nKillCamTarget1;

	if ( iLocalPlayerIndex == index && !g_HltvReplaySystem.GetHltvReplayDelay() )
	{
		s_pLocalPlayer[ nSplitScreenSlot ] = this;
		m_bIsLocalPlayer = true;

		// Tell host player about the parasitic splitscreen user
		if ( nSplitScreenSlot != 0 )
		{
			contract_assert( s_pLocalPlayer[ 0 ] );
			m_nSplitScreenSlot = nSplitScreenSlot;
			m_hSplitOwner = s_pLocalPlayer[ 0 ];
			if ( s_pLocalPlayer[ 0 ] )
			{
				s_pLocalPlayer[ 0 ]->AddSplitScreenPlayer( this );
			}
		}
		else
		{
			// We're the host, not the parasite...
			m_nSplitScreenSlot = 0;
			m_hSplitOwner = nullptr;
		}

		if ( nSplitScreenSlot == 0 )
		{
			// Reset our sound mixed in case we were in a freeze cam when we
			// changed level, which would cause the snd_soundmixer to be left modified.
			ConVar *pVar = (ConVar *)cvar->FindVar( "snd_soundmixer" );
			pVar->Revert();
		}

		UpdateVisibilityAllEntities();
	}
}

void CBasePlayer::ClientThink()
{
	//////////////////////////////////////////////
	//  spec_lock_to_accountid
	//
	//	Lock observer target to the specified account id
	////////////////////////////////////////////////////////

	static float flNextCheck = Plat_FloatTime( ) + 5;

	if ( Plat_FloatTime( ) >= flNextCheck &&
		spec_lock_to_accountid.GetString( ) &&
		spec_lock_to_accountid.GetString( )[0] &&
		IsObserver( ) && 
		IsLocalPlayer( this ) )
	{
		bool bSwitchTargets = true;

		if ( GetObserverTarget( ) && ( GetObserverMode( ) == OBS_MODE_IN_EYE || GetObserverMode( ) == OBS_MODE_CHASE ) )
		{
			CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( GetObserverTarget( ) );

			if ( pPlayer )
			{
				CPlayerId PlayerID;
				pPlayer->GetPlayerID( &PlayerID );

				if ( PlayerID.GetAccountID( ) == CPlayerId( spec_lock_to_accountid.GetString( ) ).GetAccountID() )
				{
					bSwitchTargets = false;
				}
			}
		}

		if ( bSwitchTargets )
		{
			DevMsg( "spec_lock_to_accountid: Attempting to switch spec target to %s. Clear out the convar spec_lock_to_accountid to stop.\n", spec_lock_to_accountid.GetString( ) );

 			if ( g_bEngineIsHLTV )
			{
				// we can only switch primary spectator targets if PVS isnt locked by auto-director
				if ( !HLTVCamera( )->IsPVSLocked( ) )
				{
					HLTVCamera( )->SpecPlayerByAccountID( spec_lock_to_accountid.GetString( ) );

					HLTVCamera( )->SetMode( OBS_MODE_IN_EYE );
						
					//HLTVCamera()->SetAutoDirector( CHLTVCamera::AUTODIRECTOR_PAUSED );
					HLTVCamera( )->SetAutoDirector( CHLTVCamera::AUTODIRECTOR_OFF );
				}
			}
			else
			{
				char szCmd[ 64 ];
				V_snprintf( szCmd, sizeof( szCmd ), "spec_player_by_accountid %s\n", spec_lock_to_accountid.GetString() );
				engine->ServerCmd( szCmd );

				V_snprintf( szCmd, sizeof( szCmd ), "spec_mode %i\n", OBS_MODE_IN_EYE );
				engine->ServerCmd( szCmd );
			}			
		}

		flNextCheck = Plat_FloatTime( ) + 5;
	}
}

void CBasePlayer::SetAsLocalPlayer()
{
	int nSplitScreenSlot = 0;
	contract_assert( s_pLocalPlayer[ nSplitScreenSlot ] == nullptr );
	s_pLocalPlayer[ nSplitScreenSlot ] = this;
	m_bIsLocalPlayer = true;

	// We're the host
	m_nSplitScreenSlot = 0;
	m_hSplitOwner = nullptr;

	if ( nSplitScreenSlot == 0 && !g_HltvReplaySystem.GetHltvReplayDelay() )
	{
		// Reset our sound mixed in case we were in a freeze cam when we
		// changed level, which would cause the snd_soundmixer to be left modified.
		ConVar *pVar = ( ConVar * )cvar->FindVar( "snd_soundmixer" );
		pVar->Revert();
	}

	UpdateVisibilityAllEntities();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void CBasePlayer::PostDataUpdate( DataUpdateType_t updateType )
{
	// This has to occur here as opposed to OnDataChanged so that EHandles to the player created
	//  on this same frame are not stomped because prediction thinks there
	//  isn't a local player yet!!!

	int nSlot = -1;

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		int nIndex = engine->GetSplitScreenPlayer( i );
		if ( nIndex == index )
		{
			nSlot = i;
			break;
		}
	}
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSlot );

	bool bRecheck = false;
	if ( nSlot != -1 && !s_pLocalPlayer[ nSlot ] )
	{
		bRecheck = true;
	}
	if ( updateType == DATA_UPDATE_CREATED || bRecheck )
	{
		CheckForLocalPlayer( nSlot );
	}

	bool bForceEFNoInterp = ( m_ubOldEFNoInterpParity != m_ubEFNoInterpParity );

	if ( IsLocalPlayer( this ) )
	{
		if ( (updateType == DATA_UPDATE_CREATED) &&
			g_pGameRules->IsMultiplayer() && 
			cl_player_fullupdate_predicted_origin_fix.GetBool() )
		{
			if ( (m_vecHack_RecvProxy_LocalPlayerOrigin.x != std::numeric_limits<float>::max()) && 
				(m_vecHack_RecvProxy_LocalPlayerOrigin.y != std::numeric_limits<float>::max()) && 
				(m_vecHack_RecvProxy_LocalPlayerOrigin.z != std::numeric_limits<float>::max()) )
			{
				SetNetworkOrigin( m_vecHack_RecvProxy_LocalPlayerOrigin );
			}
		}
		m_vecHack_RecvProxy_LocalPlayerOrigin.Init( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );
		SetSimulatedEveryTick( true );
	}
	else
	{
		SetSimulatedEveryTick( false );

		// estimate velocity for non local players
		float flTimeDelta = m_flSimulationTime - m_flOldSimulationTime;

		if (IsParentChanging())
		{
			bForceEFNoInterp = true;
		}

		if ( flTimeDelta > 0  &&  !( IsEffectActive(EF_NOINTERP) || bForceEFNoInterp ) )
		{
			Vector newVelo = (GetNetworkOrigin() - GetOldOrigin()  ) / flTimeDelta;
			// This code used to call SetAbsVelocity, which is a no-no since we are in networking and if
			//  in hieararchy, the parent velocity might not be set up yet.
			// On top of that GetNetworkOrigin and GetOldOrigin are local coordinates
			// So we'll just set the local vel and avoid an Assert here
			SetLocalVelocity( newVelo );
		}
	}

	BaseClass::PostDataUpdate( updateType );

	// Only care about this for local player
	if ( IsLocalPlayer( this ) )
	{
		bool bHideFreezePanel = false;

		QAngle angles;
		engine->GetViewAngles( angles );
		if ( updateType == DATA_UPDATE_CREATED )
		{
			SetLocalViewAngles( angles );
			m_flOldPlayerZ = GetLocalOrigin().z;
		}
		SetLocalAngles( angles );

		if ( m_bCanShowFreezeFrameNow && !m_bWasFreezeFraming && GetObserverMode() == OBS_MODE_FREEZECAM )
		{
			m_vecFreezeFrameStart = MainViewOrigin( GetSplitScreenPlayerSlot() );
			m_flFreezeFrameStartTime = gpGlobals->curtime;
			m_flFreezeFrameDistance = RandomFloat( spec_freeze_distance_min.GetFloat(), spec_freeze_distance_max.GetFloat() );
			m_flFreezeZOffset = RandomFloat( -4, 4 );
			m_bSentFreezeFrame = false;
			m_bStartedFreezeFrame = false;
			m_bAbortedFreezeFrame = false;

			IGameEvent *pEvent = gameeventmanager->CreateEvent( "show_freezepanel" );
			if ( pEvent )
			{
				pEvent->SetInt( "killer", GetObserverTarget() ? GetObserverTarget()->entindex() : GetLastKillerIndex() );
				pEvent->SetInt( "victim", entindex() );
				pEvent->SetInt( "hits_taken", m_nLastKillerHitsTaken );
				pEvent->SetInt( "damage_taken", m_nLastKillerDamageTaken );
				pEvent->SetInt( "hits_given", m_nLastKillerHitsGiven );
				pEvent->SetInt( "damage_given", m_nLastKillerDamageGiven );
				gameeventmanager->FireEventClientSide( pEvent );
			}

			// Force the sound mixer to the freezecam mixer
			ConVar *pVar = (ConVar *)cvar->FindVar( "snd_soundmixer" );
			pVar->SetValue( "FreezeCam_Only" );

			m_bCanShowFreezeFrameNow = false;
		}
		else if ( m_bWasFreezeFraming && GetObserverMode() != OBS_MODE_FREEZECAM )
		{
			if ( spec_freeze_panel_extended_time.GetFloat() > 0 )
			{
				m_flFreezePanelExtendedStartTime = gpGlobals->curtime;
				m_bWasFreezePanelExtended = true;
			}
			else
			{
				bHideFreezePanel = true;
			}

			view->FreezeFrame(0);

			ConVar *pVar = ( ConVar * )cvar->FindVar( "snd_soundmixer" );
			pVar->Revert();
		}
		else if ( m_bWasFreezePanelExtended && gpGlobals->curtime >= m_flFreezePanelExtendedStartTime + spec_freeze_panel_extended_time.GetFloat() )
		{
			bHideFreezePanel = true;
			m_bWasFreezePanelExtended = false;
		}
		else if ( IsAlive() )
		{
		}
		
		if ( bHideFreezePanel && !g_HltvReplaySystem.GetHltvReplayDelay() && !g_HltvReplaySystem.IsDelayedReplayRequestPending() )
		{
			IGameEvent *pEvent = gameeventmanager->CreateEvent( "hide_freezepanel" );
			if ( pEvent )
			{
				gameeventmanager->FireEventClientSide( pEvent );
			}
		}
	}
	// If we are updated while paused, allow the player origin to be snapped by the
	//  server if we receive a packet from the server
	if ( engine->IsPaused() || bForceEFNoInterp )
	{
		ResetLatched();
	}

	m_fLastUpdateServerTime = engine->GetLastTimeStamp();
	m_nLastUpdateTickBase = m_nTickBase;
	m_nLastUpdateServerTickCount = engine->GetServerTick();

	if ( IsActiveCameraMan() != m_bLastActiveCameraManState )
	{
		m_bLastActiveCameraManState = IsActiveCameraMan();

		if ( IsActiveCameraMan() && HLTVCamera()->AutoDirectorState() != CHLTVCamera::AUTODIRECTOR_OFF )
		{
			// if a cameraman becomes active and the autodirector is on, just set it on again and it'll switch to the cameraman if enabled
			HLTVCamera()->SetAutoDirector( CHLTVCamera::AUTODIRECTOR_ON );
		}
	}

	// force voice recording on for casters
	if ( ( GetLocalPlayer() == this ) && HLTVCamera() && ( HLTVCamera()->GetCameraMan() == this ) && !engine->IsVoiceRecording() )
	{
		engine->ForceVoiceRecordOn();
	}

	int eventType = -1;
	int nOptionalParam = 0;
	ConVarRef spec_show_xray( "spec_show_xray" );

	CPlayerId playerUID;
	// if the state changed for any of the cameraman stuff, send to the local player
	if ( IsActiveCameraMan() && GetPlayerID( &playerUID ) )
	{
		if ( spec_show_xray.GetBool() != m_bLastCameraManXRayState || m_bCameraManXRay != m_bLastCameraManXRayState )
		{
			m_bLastCameraManXRayState = m_bCameraManXRay;
			eventType = ( m_bCameraManXRay ) ? HLTV_UI_XRAY_ON : HLTV_UI_XRAY_OFF;
			if ( eventType != -1 )
				GetClientMode()->UpdateCameraManUIState( eventType, nOptionalParam, playerUID.ConvertToUint64() );
		}

		if ( m_bCameraManOverview != m_bLastCameraManOverviewState )
		{
			m_bLastCameraManOverviewState = m_bCameraManOverview;
			eventType = ( m_bCameraManOverview ) ? HLTV_UI_OVERVIEW_ON : HLTV_UI_OVERVIEW_OFF;
			if ( eventType != -1 )
				GetClientMode()->UpdateCameraManUIState( eventType, nOptionalParam, playerUID.ConvertToUint64() );
		}

		if ( m_bCameraManScoreBoard != m_bLastCameraManScoreBoardState )
		{
			m_bLastCameraManScoreBoardState = m_bCameraManScoreBoard;
			eventType = ( m_bCameraManScoreBoard ) ? HLTV_UI_SCOREBOARD_ON : HLTV_UI_SCOREBOARD_OFF;
			if ( eventType != -1 )
				GetClientMode()->UpdateCameraManUIState( eventType, nOptionalParam, playerUID.ConvertToUint64() );
		}

		if ( m_uCameraManGraphs != m_uLastCameraManGraphsState )
		{
			m_uLastCameraManGraphsState = m_uCameraManGraphs;
			eventType = ( m_uCameraManGraphs != 0 ) ? HLTV_UI_GRAPHS_ON : HLTV_UI_GRAPHS_OFF;
			if ( eventType == HLTV_UI_GRAPHS_ON )
			{
				nOptionalParam = m_uCameraManGraphs - 1;
			}

			if ( eventType != -1 )
				GetClientMode()->UpdateCameraManUIState( eventType, nOptionalParam, playerUID.ConvertToUint64() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBasePlayer::CanSetSoundMixer( void )
{
	// Can't set sound mixers when we're in freezecam mode, since it has a code-enforced mixer
	return (GetObserverMode() != OBS_MODE_FREEZECAM);
}

void CBasePlayer::ReceiveMessage( int classID, bf_read &msg )
{
	if ( classID != GetClientClass()->m_ClassID )
	{
		// message is for subclass
		BaseClass::ReceiveMessage( classID, msg );
		return;
	}
}

void CBasePlayer::OnRestore()
{
	BaseClass::OnRestore();

	if ( IsLocalPlayer( this ) )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );

		// debounce the attack key, for if it was used for restore
		input->ClearInputButton( IN_ATTACK | IN_ATTACK2 | IN_ZOOM );
		// GetButtonBits() has to be called for the above to take effect
		input->GetButtonBits( false );
	}

	// For ammo history icons to current value so they don't flash on level transtions
	int ammoTypes = GetAmmoDef()->NumAmmoTypes();
	// ammodef is 1 based, use <=
	for ( int i = 0; i <= ammoTypes; i++ )
	{
		m_iOldAmmo[i] = GetAmmoCount(i);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Process incoming data
//-----------------------------------------------------------------------------
extern bool g_bSpectatingForceCLPredictOff;
void CBasePlayer::OnDataChanged( DataUpdateType_t updateType )
{
	bool isLocalPlayer = IsLocalPlayer( this );
	if ( isLocalPlayer )
	{
		SetPredictionEligible( true );
	}

	BaseClass::OnDataChanged( updateType );

	bool bIsLocalOrHltvObserverPlayer = g_HltvReplaySystem.GetHltvReplayDelay() ? HLTVCamera()->GetCurrentTargetEntindex() == this->index : isLocalPlayer;

	// Only care about this for local player
	if ( bIsLocalOrHltvObserverPlayer )
	{
		int nSlot = GetSplitScreenPlayerSlot();
		// Reset engine areabits pointer, but only for main local player (not piggybacked split screen users)
		if ( nSlot == 0 )
		{
			render->SetAreaState( m_Local.m_chAreaBits, m_Local.m_chAreaPortalBits );
		}

		// Check for Ammo pickups.
		int ammoTypes = GetAmmoDef()->NumAmmoTypes();
		for ( int i = 0; i <= ammoTypes; i++ )
		{
			if ( GetAmmoCount(i) > m_iOldAmmo[i] )
			{
				// Only add this to the correct Hud
				ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSlot );

				// Don't add to ammo pickup if the ammo doesn't do it
				const FileWeaponInfo_t *pWeaponData = gWR.GetWeaponFromAmmo(i);

				if ( !pWeaponData || !( pWeaponData->iFlags & ITEM_FLAG_NOAMMOPICKUPS ) )
				{
					// Ammo pickup history removed (VGUI teardown)
				}
			}
		}

		// Only add this to the correct Hud
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSlot );
			Soundscape_Update( m_Local.m_audio );
		}
	}
	
	if ( isLocalPlayer ) // fog controller is always using local player to update fog; this seems wrong for spectating, because the observer target entity may have different fog parameters, but in practice this has never been a problem. Eventually we may need to address that by changing GetLocalPlayer in viewrender.cpp
	{
		if ( m_hOldFogController != m_PlayerFog.m_hCtrl )
		{
			FogControllerChanged( updateType == DATA_UPDATE_CREATED );
		}
	}

	if( (updateType == DATA_UPDATE_CREATED) && physenv->IsPredicted() )
	{
		if( engine->GetLocalPlayer() == index ) //CBasePlayer::IsLocalPlayer() doesn't work this early when created. This works just as well
		{
			//create physics objects
			Vector vecAbsOrigin = GetAbsOrigin();
			Vector vecAbsVelocity = GetAbsVelocity();

			solid_t solid;
			V_strncpy( solid.surfaceprop, "player", sizeof(solid.surfaceprop) );
			solid.params = g_PhysDefaultObjectParams;
			solid.params.mass = 85.0f;
			solid.params.inertia = 1e24f;
			solid.params.enableCollisions = false;
			//disable drag
			solid.params.dragCoefficient = 0;
			// create standing hull
			m_pShadowStand = PhysModelCreateCustom( this, PhysCreateBbox( VEC_HULL_MIN, VEC_HULL_MAX ), GetLocalOrigin(), GetLocalAngles(), "player_stand", false, &solid );
			m_pShadowStand->SetCallbackFlags( CALLBACK_GLOBAL_COLLISION | CALLBACK_SHADOW_COLLISION );

			// create crouchig hull
			m_pShadowCrouch = PhysModelCreateCustom( this, PhysCreateBbox( VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX ), GetLocalOrigin(), GetLocalAngles(), "player_crouch", false, &solid );
			m_pShadowCrouch->SetCallbackFlags( CALLBACK_GLOBAL_COLLISION | CALLBACK_SHADOW_COLLISION );

			// default to stand
			VPhysicsSetObject( m_pShadowStand );

			// tell physics lists I'm a shadow controller object
			PhysAddShadow( this );	
			m_pPhysicsController = physenv->CreatePlayerController( m_pShadowStand );
			m_pPhysicsController->SetPushMassLimit( 350.0f );
			m_pPhysicsController->SetPushSpeedLimit( 50.0f );

			// Give the controller a valid position so it doesn't do anything rash.
			UpdatePhysicsShadowToPosition( vecAbsOrigin );

			// init state
			if ( GetFlags() & FL_DUCKING )
			{
				SetVCollisionState( vecAbsOrigin, vecAbsVelocity, VPHYS_CROUCH );
			}
			else
			{
				SetVCollisionState( vecAbsOrigin, vecAbsVelocity, VPHYS_WALK );
			}
		}
	}

	if( IsLocalPlayer() )
	{
		IPhysicsObject *pObject = VPhysicsGetObject();
		if ((pObject != nullptr) &&
			(
				((m_vphysicsCollisionState == VPHYS_CROUCH) && (pObject == m_pShadowStand)) ||
				((m_vphysicsCollisionState == VPHYS_WALK) && (pObject == m_pShadowCrouch))
			) )
		{
			//apply networked changes to collision state
			SetVCollisionState( GetAbsOrigin(), GetAbsVelocity(), m_vphysicsCollisionState );
		}

		// [msmith] We want to disable prediction while spectating in first person because the server forcibly sets certain predicted fields on players while
		// they are spectating other players (viewOffset for example).  Keeping prediction enabled modifies these values and causes severe jitter.
		// This is most noticable while playing on a dedicated server and the specated player ducks (causing changes to viewOffset).
		int observationMode = GetObserverMode();
		bool isSpectating = GetObserverTarget() != nullptr && ( observationMode == OBS_MODE_IN_EYE || observationMode == OBS_MODE_CHASE );
		g_bSpectatingForceCLPredictOff = isSpectating;
	}

}

//-----------------------------------------------------------------------------
// Did we just enter a vehicle this frame?


//-----------------------------------------------------------------------------
// Are we in VGUI input mode?.
//-----------------------------------------------------------------------------
bool CBasePlayer::IsInVGuiInputMode() const
{
	return (m_pCurrentVguiScreen.Get() != nullptr);
}

//-----------------------------------------------------------------------------
// Are we inputing to a view model vgui screen
//-----------------------------------------------------------------------------
bool CBasePlayer::IsInViewModelVGuiInputMode() const
{
	return false;
}

//-----------------------------------------------------------------------------
// Check to see if we're in vgui input mode...
//-----------------------------------------------------------------------------
void CBasePlayer::DetermineVguiInputMode( CUserCmd *pCmd )
{
	// In-world VGUI screens removed (VGUI teardown)
}

//-----------------------------------------------------------------------------
// Purpose: Input handling
//-----------------------------------------------------------------------------
bool CBasePlayer::CreateMove( float flInputSampleTime, CUserCmd *pCmd )
{
	// Allow the vehicle to clamp the view angles
	{
		if ( joy_autosprint.GetBool() )
		{
			if ( input->KeyState( &in_joyspeed ) != 0.0f )
			{
				pCmd->buttons |= IN_SPEED;
			}
		}

		CBaseCombatWeapon *pWeapon = GetActiveWeapon();
		if ( pWeapon )
		{
			pWeapon->CreateMove( flInputSampleTime, pCmd, m_vecOldViewAngles );
		}
	}

	// If the frozen flag is set, prevent view movement (server prevents the rest of the movement)
	if ( GetFlags() & FL_FROZEN )
	{
		// Don't stomp the first time we get frozen
		if ( m_bWasFrozen )
		{
			// Stomp the new viewangles with old ones
			pCmd->viewangles = m_vecOldViewAngles;
			engine->SetViewAngles( pCmd->viewangles );
		}
		else
		{
			m_bWasFrozen = true;
		}
	}
	else
	{
		m_bWasFrozen = false;
	}

	m_vecOldViewAngles = pCmd->viewangles;
	
	// Check to see if we're in vgui input mode...
	DetermineVguiInputMode( pCmd );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Player has changed to a new team
//-----------------------------------------------------------------------------
void CBasePlayer::TeamChange( int iNewTeam )
{
	// Base class does nothing
}

//-----------------------------------------------------------------------------
// Purpose: Creates, destroys, and updates the flashlight effect as needed.
//-----------------------------------------------------------------------------
void CBasePlayer::UpdateFlashlight()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int iSsPlayer = GET_ACTIVE_SPLITSCREEN_SLOT();

	// TERROR: if we're in-eye spectating, use that player's flashlight
	CBasePlayer *pFlashlightPlayer = this;
	if ( !IsAlive() )
	{
		if ( GetObserverMode() == OBS_MODE_IN_EYE )
		{
			pFlashlightPlayer = ToBasePlayer( GetObserverTarget() );
		}
	}

	if ( pFlashlightPlayer )
	{
		FlashlightEffectManager().SetEntityIndex( pFlashlightPlayer->index );
	}

	// The dim light is the flashlight.
	if ( pFlashlightPlayer && pFlashlightPlayer->IsAlive() && pFlashlightPlayer->IsEffectActive( EF_DIMLIGHT ) && !pFlashlightPlayer->GetViewEntity() )
	{
		// Make sure we're using the proper flashlight texture
		const char *pszTextureName = pFlashlightPlayer->GetFlashlightTextureName();
		if ( !m_bFlashlightEnabled[ iSsPlayer ] )
		{
			// Turned on the headlight; create it.
			if ( pszTextureName )
			{
				FlashlightEffectManager().TurnOnFlashlight( pFlashlightPlayer->index, pszTextureName, pFlashlightPlayer->GetFlashlightFOV(),
					pFlashlightPlayer->GetFlashlightFarZ(), pFlashlightPlayer->GetFlashlightLinearAtten() );
			}
			else
			{
				FlashlightEffectManager().TurnOnFlashlight( pFlashlightPlayer->index );
			}
			m_bFlashlightEnabled[ iSsPlayer ] = true;
		}
	}
	else if ( m_bFlashlightEnabled[ iSsPlayer ] )
	{
		// Turned off the flashlight; delete it.
		FlashlightEffectManager().TurnOffFlashlight();
		m_bFlashlightEnabled[ iSsPlayer ] = false;
	}

	if ( pFlashlightPlayer && m_bFlashlightEnabled[ iSsPlayer ] )
	{
		Vector vecForward, vecRight, vecUp;
		Vector vecPos;
		//Check to see if we have an externally specified flashlight origin, if not, use eye vectors/render origin
		if ( pFlashlightPlayer->m_vecFlashlightOrigin != vec3_origin && pFlashlightPlayer->m_vecFlashlightOrigin.IsValid() )
		{
			vecPos = pFlashlightPlayer->m_vecFlashlightOrigin;
			vecForward = pFlashlightPlayer->m_vecFlashlightForward;
			vecRight = pFlashlightPlayer->m_vecFlashlightRight;
			vecUp = pFlashlightPlayer->m_vecFlashlightUp;
		}
		else
		{
			EyeVectors( &vecForward, &vecRight, &vecUp );
			vecPos = GetRenderOrigin() + m_vecViewOffset;
		}

		// Update the light with the new position and direction.		
		FlashlightEffectManager().UpdateFlashlight( vecPos, vecForward, vecRight, vecUp, pFlashlightPlayer->GetFlashlightFOV(), 
			pFlashlightPlayer->CastsFlashlightShadows(), pFlashlightPlayer->GetFlashlightFarZ(), pFlashlightPlayer->GetFlashlightLinearAtten(),
			pFlashlightPlayer->GetFlashlightTextureName() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Creates player flashlight if it's active
//-----------------------------------------------------------------------------
void CBasePlayer::Flashlight( void )
{
	UpdateFlashlight();
}

//-----------------------------------------------------------------------------
// Purpose: Turns off flashlight if it's active (TERROR)
//-----------------------------------------------------------------------------
void CBasePlayer::TurnOffFlashlight( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	if ( m_bFlashlightEnabled[ nSlot ] )
	{
		FlashlightEffectManager().TurnOffFlashlight();
		m_bFlashlightEnabled[ nSlot ] = false;
	}
}

//-----------------------------------------------------------------------------
// Called when not in tactical mode. Allows view to be overriden for things like driving a tank.
//-----------------------------------------------------------------------------
void CBasePlayer::OverrideView( CViewSetup *pSetup )
{
}

bool CBasePlayer::IsCameraMan() const 
{ 
	return IsActiveCameraMan(); //( HLTVCamera() && ( HLTVCamera()->GetCameraMan() == this ) );
}

bool CBasePlayer::ShouldInterpolate()
{
	// always interpolate myself
	if ( IsLocalPlayer( this ) )
		return true;

	// always interpolate entity if followed by HLTV/Replay
	if ( HLTVCamera()->GetCameraMan() == this )
		return true;

	return BaseClass::ShouldInterpolate();
}

bool CBasePlayer::ShouldDraw()
{
	// $FIXME(hpe) this was returning false in splitscreen mode making 2nd player invisible
	return ( this != GetSplitScreenViewPlayer() || CBasePlayer::ShouldDrawLocalPlayer() || (GetObserverMode() == OBS_MODE_DEATHCAM ) ) &&
		   BaseClass::ShouldDraw();
}

int CBasePlayer::DrawModel( int flags, const RenderableInstance_t &instance )
{
	// if local player is spectating this player in first person mode, don't draw it
	CBasePlayer * player = CBasePlayer::GetLocalPlayer();

	if ( player && player->IsObserver() )
	{
		if ( player->GetObserverMode() == OBS_MODE_IN_EYE &&
			player->GetObserverTarget() == this &&
			!input->CAM_IsThirdPerson() && 
			player->GetObserverInterpState() != OBSERVER_INTERP_TRAVELING )
			return 0;
	}

	return BaseClass::DrawModel( flags, instance );
}

bool CBasePlayer::ShouldSuppressForSplitScreenPlayer( int nSlot )
{
	if ( BaseClass::ShouldSuppressForSplitScreenPlayer( nSlot ) )
		return true;

	PlayerRenderMode_t nMode = GetPlayerRenderMode( nSlot );
	return ( nMode == PLAYER_RENDER_FIRSTPERSON );
}

//-----------------------------------------------------------------------------
// Computes the render mode for this player
//-----------------------------------------------------------------------------
PlayerRenderMode_t CBasePlayer::GetPlayerRenderMode( int nSlot )
{
	// check if local player chases owner of this weapon in first person
	CBasePlayer *pSplitscreenPlayer = GetSplitScreenViewPlayer( nSlot );
	if ( !pSplitscreenPlayer )
		return PLAYER_RENDER_THIRDPERSON;

	if ( pSplitscreenPlayer->IsObserver() )
	{
		if ( pSplitscreenPlayer->GetObserverTarget() != this )
			return PLAYER_RENDER_THIRDPERSON;
		if ( pSplitscreenPlayer->GetObserverMode() != OBS_MODE_IN_EYE )
			return PLAYER_RENDER_THIRDPERSON;
	}
	else
	{
		if ( pSplitscreenPlayer != this )
			return PLAYER_RENDER_THIRDPERSON;
	}

	if ( input->CAM_IsThirdPerson( nSlot ) )
		return PLAYER_RENDER_THIRDPERSON;

	if ( (pSplitscreenPlayer->GetViewEntity() != nullptr) && 
		(pSplitscreenPlayer->GetViewEntity() != pSplitscreenPlayer) &&
		pSplitscreenPlayer->m_bShouldDrawPlayerWhileUsingViewEntity )
		return PLAYER_RENDER_THIRDPERSON;

//	if ( IsInThirdPersonView() )
//		return PLAYER_RENDER_THIRDPERSON;

	return PLAYER_RENDER_FIRSTPERSON;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector CBasePlayer::GetChaseCamViewOffset( CBaseEntity *target )
{
	CBasePlayer *player = ToBasePlayer( target );
	
	if ( player && player->IsAlive() )
	{
		if( player->GetFlags() & FL_DUCKING )
			return VEC_DUCK_VIEW;

		return VEC_VIEW;
	}

	CBaseCSGrenadeProjectile *pGrenade = dynamic_cast< CBaseCSGrenadeProjectile* >( target );
	if ( pGrenade )
		return Vector( 0, 0, 8 );

	// assume it's the players ragdoll
	return VEC_DEAD_VIEWHEIGHT;
}

void CBasePlayer::CalcChaseCamView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov)
{
	CBaseEntity *target = GetObserverTarget();

	if ( !target ) 
	{
		// just copy a save in-map position
		VectorCopy( EyePosition(), eyeOrigin );
		VectorCopy( EyeAngles(), eyeAngles );
		return;
	};

	// If our target isn't visible, we're at a camera point of some kind.
	// Instead of letting the player rotate around an invisible point, treat
	// the point as a fixed camera.
	if ( !target->GetBaseAnimating() && !target->GetModel() )
	{
		CalcRoamingView( eyeOrigin, eyeAngles, fov );
		return;
	}

/*#ifdef CSTRIKE_DLL
	// weapon gun-cam go-pro chase camera
	CCSPlayer *pPlayer = ToCSPlayer( target );
	if ( pPlayer && pPlayer->ShouldDraw() )
	{
		Vector vecSrc = target->GetObserverCamOrigin();
		VectorAdd( vecSrc, GetChaseCamViewOffset( target ), vecSrc );

		Vector vecObsForward, vecObsRight, vecObsUp;
		AngleVectors( EyeAngles(), &vecObsForward, &vecObsRight, &vecObsUp );

		trace_t playerEyeTrace;
		UTIL_TraceLine( vecSrc, vecSrc - vecObsForward * 75.0f, MASK_SOLID_BRUSHONLY, pPlayer, COLLISION_GROUP_NONE, &playerEyeTrace );
		
		float flDistMax = playerEyeTrace.startpos.DistTo( playerEyeTrace.endpos + vecObsForward * 4 );
		m_flObserverChaseApproach = (m_flObserverChaseApproach >= flDistMax) ? flDistMax : Approach( 75.0f, m_flObserverChaseApproach, gpGlobals->frametime * 20 );

		Vector vecIdealCamEyePos = vecSrc - vecObsForward * m_flObserverChaseApproach;
		Vector vecIdealCamTargetPos = vecSrc + vecObsRight * RemapValClamped( m_flObserverChaseApproach, 20, 75, 6, 16 ) * abs(DotProduct(vecObsUp,Vector(0,0,1)));
		VectorAngles( (vecIdealCamTargetPos - vecIdealCamEyePos ).Normalized(), eyeAngles );

		eyeOrigin = vecIdealCamEyePos;
		return;
	}
#endif*/
	
	CBaseCSGrenadeProjectile *pGrenade = dynamic_cast< CBaseCSGrenadeProjectile* >( target );

	// QAngle tmpangles;

	Vector forward, viewpoint;

	// GetObserverCamOrigin() returns ragdoll pos if player is ragdolled
	Vector origin = target->GetObserverCamOrigin();

	VectorAdd( origin, GetChaseCamViewOffset( target ), origin );

	QAngle viewangles;

	if ( GetObserverMode() == OBS_MODE_IN_EYE )
	{
		viewangles = eyeAngles;
	}
	else if ( pGrenade && pGrenade->m_nBounces <= 0  )
	{
		Vector vecVel = pGrenade->GetLocalVelocity();
		VectorAngles( vecVel, viewangles );
	}
	else if ( IsLocalPlayer( this ) )
	{
		engine->GetViewAngles( viewangles );
	}
	else
	{
		viewangles = EyeAngles();
	}

	// [Forrest] Spectating someone who is headshotted by a teammate and then switching to chase cam leaves
	// you with a permanent roll to the camera that doesn't decay and is not reset
	// even when switching to different players or at the start of the next round
	// if you are still a spectator.  (If you spawn as a player, the view is reset.
	// if you switch spectator modes, the view is reset.)
	// The chase camera adopts the yaw and pitch of the previous camera, but the camera
	// should not roll.
	viewangles.z = 0;

	m_flObserverChaseDistance += gpGlobals->frametime*48.0f;

	float flMaxDistance = CHASE_CAM_DISTANCE;
	if ( target && target->IsBaseTrain() )
	{
		// if this is a train, we want to be back a little further so we can see more of it
		flMaxDistance *= 2.5f;
	}
	else if ( pGrenade )
	{
		flMaxDistance = 64.0f;
	}
	m_flObserverChaseDistance = clamp( m_flObserverChaseDistance, 16, flMaxDistance );
	
	AngleVectors( viewangles, &forward );

	VectorNormalize( forward );

	VectorMA(origin, -m_flObserverChaseDistance, forward, viewpoint );

	trace_t trace;
	CTraceFilterNoNPCsOrPlayer filter( target, COLLISION_GROUP_NONE );
	CBaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace

	Vector hullMin = WALL_MIN, hullMax = WALL_MAX;

	if ( target && V_strcmp( target->GetClassname(), "class CPlantedC4" ) == 0 )
	{
		hullMin *= 2.f; hullMax *= 2.f; 
	}

	UTIL_TraceHull( origin, viewpoint, hullMin, hullMax, MASK_SOLID, &filter, &trace );
	CBaseEntity::PopEnableAbsRecomputations();

	if (trace.fraction < 1.0)
	{
		viewpoint = trace.endpos;
		m_flObserverChaseDistance = VectorLength(origin - eyeOrigin);
	}
	else if ( pGrenade )
	{
		m_flObserverChaseDistance = std::max<std::common_type_t<int, float>>( 64, m_flObserverChaseDistance );
	}

	VectorCopy( viewangles, eyeAngles );
	VectorCopy( viewpoint, eyeOrigin );

	fov = GetFOV();
}

void CBasePlayer::CalcRoamingView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov)
{
	// in CS, we can have a spec target, but in roaming, we always want to roam FREE!!!  despite the target
	CBaseEntity *target = this;

	m_flObserverChaseDistance = 0.0;

	eyeOrigin = target->EyePosition();
	eyeAngles = target->EyeAngles();

	if ( spec_track.GetInt() > 0 )
	{
		CBaseEntity *target =  ClientEntityList().GetBaseEntity( spec_track.GetInt() );

		if ( target )
		{
			Vector v = target->GetAbsOrigin(); v.z += 54;
			QAngle a; VectorAngles( v - eyeOrigin, a );

			NormalizeAngles( a );
			eyeAngles = a;
			engine->SetViewAngles( a );
		}
	}

	if ( GetObserverMode() == OBS_MODE_FIXED )
	{
		Vector viewpoint;
		// if we are in a fixed position, do a very simple check to make sure we aren't fixed inside the world below us
		trace_t trace;
		CTraceFilterNoNPCsOrPlayer filter( target, COLLISION_GROUP_NONE );
		CBaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace

		Vector hullMin = Vector( -2, -2, -24 ), hullMax = Vector( 2, 2, 8 );
		UTIL_TraceHull( eyeOrigin + Vector( 0, 0, 42 ), eyeOrigin + Vector( 0, 0, -16 ), hullMin, hullMax, MASK_SOLID, &filter, &trace );
		CBaseEntity::PopEnableAbsRecomputations();

		if (trace.fraction < 1.0)
		{
			viewpoint = trace.endpos;
			VectorCopy( viewpoint, eyeOrigin );
		}
	}

	// Apply a smoothing offset to smooth out prediction errors.
	Vector vSmoothOffset;
	GetPredictionErrorSmoothingVector( vSmoothOffset );
	eyeOrigin += vSmoothOffset;

	fov = GetFOV();
}

//-----------------------------------------------------------------------------
// Purpose: Calculate the view for the player while he's in freeze frame observer mode
//-----------------------------------------------------------------------------
void CBasePlayer::CalcFreezeCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov )
{
	CBaseEntity *pTarget = GetObserverTarget();
	if ( !pTarget )
	{
		CalcDeathCamView( eyeOrigin, eyeAngles, fov );
		return;
	}

	if ( !m_bStartedFreezeFrame )
	{
		m_bStartedFreezeFrame = true;
	}

	// Zoom towards our target
	float flCurTime = (gpGlobals->curtime - m_flFreezeFrameStartTime);
	float flBlendPerc = 0.0f;
	
	if ( !g_HltvReplaySystem.IsDelayedReplayRequestPending() ) // if we auto-start replay, we don't want to layer camera motion on top of fade on top of replay scene cut
	{
		clamp( flCurTime / spec_freeze_traveltime.GetFloat(), 0, 1 );
		//Msg( "Freezecam @%.2f\n", flCurTime / spec_freeze_traveltime.GetFloat() ); // replayfade
		flBlendPerc = SimpleSpline( flBlendPerc );
	}

	Vector vecCamDesired = pTarget->GetObserverCamOrigin();	// Returns ragdoll origin if they're ragdolled
	VectorAdd( vecCamDesired, GetChaseCamViewOffset( pTarget ), vecCamDesired );
	Vector vecCamTarget = vecCamDesired;
	if ( pTarget->IsAlive() )
	{
		// Look at their chest, not their head
		Vector maxs = GameRules()->GetViewVectors()->m_vHullMax;
		vecCamTarget.z -= (maxs.z * 0.5);
	}
	else
	{
		vecCamTarget.z += VEC_DEAD_VIEWHEIGHT.z;	// look over ragdoll, not through
	}

	// Figure out a view position in front of the target
	Vector vecEyeOnPlane = eyeOrigin;
	vecEyeOnPlane.z = vecCamTarget.z;
	Vector vecTargetPos = vecCamTarget;
	Vector vecToTarget = vecTargetPos - vecEyeOnPlane;
	VectorNormalize( vecToTarget );

	// Stop a few units away from the target, and shift up to be at the same height
	vecTargetPos = vecCamTarget - (vecToTarget * m_flFreezeFrameDistance);
	float flEyePosZ = pTarget->EyePosition().z;
	vecTargetPos.z = flEyePosZ + m_flFreezeZOffset;

	// Now trace out from the target, so that we're put in front of any walls
	trace_t trace;
	CBaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
	UTIL_TraceHull( vecCamTarget, vecTargetPos, WALL_MIN, WALL_MAX, MASK_SOLID, pTarget, COLLISION_GROUP_NONE, &trace );
	CBaseEntity::PopEnableAbsRecomputations();
	if (trace.fraction < 1.0)
	{
		// The camera's going to be really close to the target. So we don't end up
		// looking at someone's chest, aim close freezecams at the target's eyes.
		vecTargetPos = trace.endpos;
		vecCamTarget = vecCamDesired;

		// To stop all close in views looking up at character's chins, move the view up.
		vecTargetPos.z += fabs(vecCamTarget.z - vecTargetPos.z) * 0.85;
		CBaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
		UTIL_TraceHull( vecCamTarget, vecTargetPos, WALL_MIN, WALL_MAX, MASK_SOLID, pTarget, COLLISION_GROUP_NONE, &trace );
		CBaseEntity::PopEnableAbsRecomputations();
		vecTargetPos = trace.endpos;
	}

	// Look directly at the target
	vecToTarget = vecCamTarget - vecTargetPos;
	VectorNormalize( vecToTarget );
	VectorAngles( vecToTarget, eyeAngles );
	
	VectorLerp( m_vecFreezeFrameStart, vecTargetPos, flBlendPerc, eyeOrigin );

	if ( flCurTime >= spec_freeze_traveltime.GetFloat() && !m_bSentFreezeFrame )
	{
		IGameEvent *pEvent = gameeventmanager->CreateEvent( "freezecam_started" );
		if ( pEvent )
		{
			gameeventmanager->FireEventClientSide( pEvent );
		}

		m_bSentFreezeFrame = true;
		view->FreezeFrame( spec_freeze_time.GetFloat() );
	}
}

void CBasePlayer::CalcInEyeCamView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov)
{
	CBaseEntity *target = GetObserverTarget();

	if ( !target ) 
	{
		// just copy a save in-map position
		VectorCopy( EyePosition(), eyeOrigin );
		VectorCopy( EyeAngles(), eyeAngles );
		return;
	};

	if ( !target->IsAlive() )
	{
		// if dead, show from 3rd person
		CalcChaseCamView( eyeOrigin, eyeAngles, fov );
		return;
	}

	fov = GetFOV();	// TODO use tragets FOV

	m_flObserverChaseDistance = 0.0;

	eyeAngles = target->EyeAngles();
	eyeOrigin = target->GetAbsOrigin();
		
	CalcViewBob( eyeOrigin );
	CalcViewRoll( eyeAngles );

	CalcAddViewmodelCameraAnimation( eyeOrigin, eyeAngles );

	// Apply punch angle
	VectorAdd( eyeAngles, GetViewPunchAngle(), eyeAngles );

	// Apply aim punch angle
	VectorAdd( eyeAngles, GetAimPunchAngle() * view_recoil_tracking.GetFloat(), eyeAngles );

	if( g_bEngineIsHLTV )
	{
		if ( target->GetFlags() & FL_DUCKING )
		{
			eyeOrigin += VEC_DUCK_VIEW;
		}
		else
		{
			eyeOrigin += VEC_VIEW;
		}
	}
	else
	{
		Vector offset = m_vecViewOffset;
		eyeOrigin += offset; // hack hack
	}

	engine->SetViewAngles( eyeAngles );
}

float CBasePlayer::GetDeathCamInterpolationTime()
{
	return DEATH_ANIMATION_TIME;
}

void CBasePlayer::CalcDeathCamView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov)
{
	CBaseEntity	* pKiller = nullptr; 

	if ( mp_forcecamera.GetInt() == OBS_ALLOW_ALL )
	{
		// if mp_forcecamera is off let user see killer or look around
		pKiller = GetObserverTarget();
		eyeAngles = EyeAngles();
	}

	float interpolation = ( gpGlobals->curtime - m_flDeathTime ) / GetDeathCamInterpolationTime();
	interpolation = clamp( interpolation, 0.0f, 1.0f );

	m_flObserverChaseDistance += gpGlobals->frametime*48.0f;
	m_flObserverChaseDistance = clamp( m_flObserverChaseDistance, 16, CHASE_CAM_DISTANCE );

	QAngle aForward = eyeAngles;
	Vector origin = EyePosition();			

	IRagdoll *pRagdoll = GetRepresentativeRagdoll();
	if ( pRagdoll )
	{
		origin = pRagdoll->GetRagdollOrigin();
		origin.z += VEC_DEAD_VIEWHEIGHT.z; // look over ragdoll, not through
	}
	
	if ( pKiller && pKiller->IsPlayer() && (pKiller != this) ) 
	{
		Vector vKiller = pKiller->EyePosition() - origin;
		QAngle aKiller; VectorAngles( vKiller, aKiller );
		InterpolateAngles( aForward, aKiller, eyeAngles, interpolation );
	};

	Vector vForward; AngleVectors( eyeAngles, &vForward );

	VectorNormalize( vForward );

	VectorMA( origin, -m_flObserverChaseDistance, vForward, eyeOrigin );

	trace_t trace; // clip against world
	CBaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
	UTIL_TraceHull( origin, eyeOrigin, WALL_MIN, WALL_MAX, MASK_SOLID, this, COLLISION_GROUP_NONE, &trace );
	CBaseEntity::PopEnableAbsRecomputations();

	if (trace.fraction < 1.0)
	{
		eyeOrigin = trace.endpos;
		m_flObserverChaseDistance = VectorLength(origin - eyeOrigin);
	}

	fov = GetFOV();
}

//-----------------------------------------------------------------------------
// Purpose: Return the weapon to have open the weapon selection on, based upon our currently active weapon
//			Base class just uses the weapon that's currently active.
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBasePlayer::GetActiveWeaponForSelection( void )
{
	return GetActiveWeapon();
}

CBaseAnimating* CBasePlayer::GetRenderedWeaponModel()
{
	// Attach to either their weapon model or their view model.
	if ( ShouldDrawLocalPlayer() || !IsLocalPlayer( this ) )
	{
		return GetActiveWeapon();
	}
	else
	{
		return GetViewModel();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gets a pointer to the local player, if it exists yet.
// static method
//-----------------------------------------------------------------------------
CBasePlayer *CBasePlayer::GetLocalPlayer( int nSlot /*= -1*/ )
{
	if ( nSlot == -1 )
	{
//		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		return s_pLocalPlayer[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
	}
	return s_pLocalPlayer[ nSlot ];
}

void CBasePlayer::SetRemoteSplitScreenPlayerViewsAreLocalPlayer( bool bSet )
{
	for( int i = 0; i != MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		if( !IsLocalSplitScreenPlayer( i ) )
		{
			s_pLocalPlayer[i] = bSet ? GetSplitScreenViewPlayer( i ) : nullptr;
		}
	}
}

bool CBasePlayer::HasAnyLocalPlayer()
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		if ( s_pLocalPlayer[ i ] )
			return true;
	}
	return false;
}

int CBasePlayer::GetSplitScreenSlotForPlayer( CBaseEntity *pl )
{
	CBasePlayer *pPlayer = ToBasePlayer( pl );

	if ( !pPlayer )
	{
		contract_assert( 0 );
		return -1;
	}
	return pPlayer->GetSplitScreenPlayerSlot();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bThirdperson - 
//-----------------------------------------------------------------------------
void CBasePlayer::ThirdPersonSwitch( bool bThirdperson )
{
	// We've switch from first to third, or vice versa.
	UpdateVisibility();
}

//-----------------------------------------------------------------------------
// Purpose: single place to decide whether the local player should draw
//-----------------------------------------------------------------------------
bool CBasePlayer::ShouldDrawLocalPlayer()
{
	int nSlot = GetSplitScreenPlayerSlot();

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSlot );

	return ( GetPlayerRenderMode(nSlot) == PLAYER_RENDER_THIRDPERSON ) || input->CAM_IsThirdPerson() || ( ToolsEnabled() && ToolFramework_IsThirdPersonCamera() );
}

//----------------------------------------------------------------------------
// Hooks into the fast path render system
//----------------------------------------------------------------------------
IClientModelRenderable *CBasePlayer::GetClientModelRenderable()
{

	// We can enable mostly opaque models to cause players to be rendered in both the opaque and the translucent fast paths
	// allowing both alpha and non alpha materials to show up.
	// However, since the bounding boxes are different for these "sub models" they have sorting issues when the player
	// is inside of a smoke cloud such that the alpha components sort in front of the smoke cloud.
	// Because of this, we no longer use two passes for players and instead cause all players to NOT use the fast path 
	// rendering by returning NULL here.
	return nullptr;

	// Because of alpha sorting issues with smoke when we have mostlyopaque models.
	// Honor base class eligibility
	if ( !BaseClass::GetClientModelRenderable() )
		return nullptr;

	// No fast path for firstperson local players
	if ( IsLocalPlayer( this ) )
	{
		bool bThirdPerson = input->CAM_IsThirdPerson() || ( ToolsEnabled() && ToolFramework_IsThirdPersonCamera() );
		if ( !bThirdPerson )
		{
			return nullptr;
		}
	}

	// if local player is spectating this player in first person mode, don't use fast path, so we can skip drawing it
	CBasePlayer *localPlayer = CBasePlayer::GetLocalPlayer();
	if ( localPlayer && localPlayer->IsObserver() )
	{
		if ( localPlayer->GetObserverMode() == OBS_MODE_IN_EYE &&
			localPlayer->GetObserverTarget() == this &&
			!input->CAM_IsThirdPerson() )
			return nullptr;
	}

	// Probably for the left 4 dead code.
	// don't use fastpath for teammates (causes extra work for glows)
	if ( localPlayer && localPlayer->GetTeamNumber() == GetTeamNumber() )
	{
		return nullptr;
	}

	return this; 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBasePlayer::IsLocalPlayer( const CBaseEntity *pEntity )
{
	if ( !pEntity || 
		 !pEntity->IsPlayer() )
		return false;

	return static_cast< const CBasePlayer * >( pEntity )->m_bIsLocalPlayer;
}

int	CBasePlayer::GetUserID( void ) const
{
	player_info_t pi;

	if ( !engine->GetPlayerInfo( entindex(), &pi ) )
		return -1;

	return pi.userID;
}

// For weapon prediction
void CBasePlayer::SetAnimation( PLAYER_ANIM playerAnim )
{
	// FIXME
}

void CBasePlayer::UpdateClientData( void )
{
	// Update all the items
	for ( int i = 0; i < WeaponCount(); i++ )
	{
		if ( GetWeapon(i) )  // each item updates it's successors
			GetWeapon(i)->UpdateClientData( this );
	}
}

// Prediction stuff
void CBasePlayer::PreThink( void )
{
	ItemPreFrame();

	UpdateClientData();

	UpdateUnderwaterState();

	// Update the player's fog data if necessary.
	UpdateFogController();

	if (m_lifeState >= LIFE_DYING)
		return;

	//
	// If we're not on the ground, we're falling. Update our falling velocity.
	//
	if ( !( GetFlags() & FL_ONGROUND ) )
	{
		m_Local.m_flFallVelocity = -GetAbsVelocity().z;
	}

	if ( GetGroundEntity() )
	{
		m_flTimeLastTouchedGround = gpGlobals->curtime;
	}
}

void CBasePlayer::PostThink( void )
{
	MDLCACHE_CRITICAL_SECTION();

	if ( IsAlive())
	{
		UpdateCollisionBounds();

		// do weapon stuff
		ItemPostFrame();

		if ( GetFlags() & FL_ONGROUND )
		{		
			m_Local.m_flFallVelocity = 0;
		}

		// Don't allow bogus sequence on player
		if ( GetSequence() == -1 )
		{
			SetSequence( 0 );
		}

		StudioFrameAdvance();
		PostThinkVPhysics();
	}

	// Even if dead simulate entities
	SimulatePlayerSimulatedEntities();
}

//-----------------------------------------------------------------------------
// Purpose: send various tool messages - viewoffset, and base class messages (flex and bones)
//-----------------------------------------------------------------------------
void CBasePlayer::GetToolRecordingState( KeyValues *msg )
{
	if ( !ToolsEnabled() )
		return;

	CVProfScope _( "CBasePlayer::GetToolRecordingState", 0, VPROF_BUDGETGROUP_TOOLS, false, BUDGETFLAG_OTHER );

	BaseClass::GetToolRecordingState( msg );

	msg->SetBool( "baseplayer", true );
	msg->SetBool( "localplayer", IsLocalPlayer( this ) );
	msg->SetString( "playername", GetPlayerName() );

	static CameraRecordingState_t state;
	state.m_flFOV = GetFOV();

	float flZNear = view->GetZNear();
	float flZFar = view->GetZFar();
	CalcView( state.m_vecEyePosition, state.m_vecEyeAngles, flZNear, flZFar, state.m_flFOV );
	state.m_bThirdPerson = !engine->IsPaused() && ::input->CAM_IsThirdPerson();
	state.m_bPlayerEyeIsPortalled = false;

	// this is a straight copy from ClientModeShared::OverrideView,
	// When that method is removed in favor of rolling it into CalcView,
	// then this code can (should!) be removed
	if ( state.m_bThirdPerson )
	{
		Vector cam_ofs;
		::input->CAM_GetCameraOffset( cam_ofs );

		QAngle camAngles;
		camAngles[ PITCH ] = cam_ofs[ PITCH ];
		camAngles[ YAW ] = cam_ofs[ YAW ];
		camAngles[ ROLL ] = 0;

		Vector camForward, camRight, camUp;
		AngleVectors( camAngles, &camForward, &camRight, &camUp );

		VectorMA( state.m_vecEyePosition, -cam_ofs[ ROLL ], camForward, state.m_vecEyePosition );

		// Override angles from third person camera
		VectorCopy( camAngles, state.m_vecEyeAngles );
	}

	msg->SetPtr( "camera", &state );
}

//-----------------------------------------------------------------------------
// Purpose: Simulate the player for this frame
//-----------------------------------------------------------------------------
bool CBasePlayer::Simulate()
{
	//Frame updates
	if ( CBasePlayer::IsLocalPlayer( this ) )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );

		//Update the flashlight
		Flashlight();

		// Update the player's fog data if necessary.
		UpdateFogController();
	}
	else
	{
		// update step sounds for all other players
		Vector vel;
		EstimateAbsVelocity( vel );
		UpdateStepSound( GetGroundSurface(), GetAbsOrigin(), vel );
	}

	BaseClass::Simulate();

	// Server says don't interpolate this frame, so set previous info to new info.
	if ( IsEffectActive( EF_NOINTERP ) || Teleported() )
	{
		ResetLatched();
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBaseViewModel
//-----------------------------------------------------------------------------
CBaseViewModel *CBasePlayer::GetViewModel( int index /*= 0*/ ) const
{
	contract_assert( index >= 0 && index < MAX_VIEWMODELS );

	CBaseViewModel *vm = m_hViewModel[ index ];
	
	if ( GetObserverMode() == OBS_MODE_IN_EYE )
	{
		CBasePlayer *target =  ToBasePlayer( GetObserverTarget() );

		// get the targets viewmodel unless the target is an observer itself
		if ( target && target != this && !target->IsObserver() )
		{
			vm = target->GetViewModel( index );
		}
	}

	return vm;
}

CBaseCombatWeapon	*CBasePlayer::GetActiveWeapon( void ) const
{
	const CBasePlayer *fromPlayer = this;

	// if localplayer is in InEye spectator mode, return weapon on chased player.
	if ( ( CBasePlayer::IsLocalPlayer( const_cast< CBasePlayer * >( fromPlayer) ) ) && ( GetObserverMode() == OBS_MODE_IN_EYE) )
	{
		CBaseEntity *target =  GetObserverTarget();

		if ( target && target->IsPlayer() )
		{
			fromPlayer = ToBasePlayer( target );
		}
	}

	return fromPlayer->CBaseCombatCharacter::GetActiveWeapon();
}

//=========================================================
// Autoaim
// set crosshair position to point to enemey
//=========================================================
Vector CBasePlayer::GetAutoaimVector( float flScale )
{
	// Never autoaim a predicted weapon (for now)
	Vector	forward;
	AngleVectors( GetAbsAngles() + m_Local.m_viewPunchAngle, &forward );
	return	forward;
}

// Stuff for prediction
void CBasePlayer::SetSuitUpdate(const char *name, int fgroup, int iNoRepeat)
{
	// FIXME:  Do something here?
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayer::ResetAutoaim( void )
{
	m_fOnTarget = false;
}

bool CBasePlayer::ShouldPredict( void )
{
	// Do this before calling into baseclass so prediction data block gets allocated
	if ( IsLocalPlayer( this ) )
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Return the player who will predict this entity
//-----------------------------------------------------------------------------
CBasePlayer *CBasePlayer::GetPredictionOwner( void )
{
	return this;
}

//-----------------------------------------------------------------------------
// Purpose: Special processing for player simulation
// NOTE: Don't chain to BaseClass!!!!
//-----------------------------------------------------------------------------
void CBasePlayer::PhysicsSimulate( void )
{
	CVProfScope _( "CBasePlayer::PhysicsSimulate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	// If we've got a moveparent, we must simulate that first.
	CBaseEntity *pMoveParent = GetMoveParent();
	if (pMoveParent)
	{
		pMoveParent->PhysicsSimulate();
	}

	// Make sure not to simulate this guy twice per frame
	if (m_nSimulationTick == gpGlobals->tickcount)
		return;

	m_nSimulationTick = gpGlobals->tickcount;

	if ( !IsLocalPlayer( this ) )
		return;

	CCommandContext *ctx = GetCommandContext();
	contract_assert( ctx );
	contract_assert( ctx->needsprocessing );
	if ( !ctx->needsprocessing )
		return;

	ctx->needsprocessing = false;

	m_bTouchedPhysObject = false;

	// Handle FL_FROZEN.
	if(GetFlags() & FL_FROZEN)
	{
		ctx->cmd.forwardmove = 0;
		ctx->cmd.sidemove = 0;
		ctx->cmd.upmove = 0;
		ctx->cmd.buttons = 0;
		ctx->cmd.impulse = 0;
		//VectorCopy ( pl.v_angle, ctx->cmd.viewangles );
	}

	// Run the next command
	MoveHelper()->SetHost( this );
	prediction->RunCommand( 
		this, 
		&ctx->cmd, 
		MoveHelper() );
	
	UpdateVPhysicsPosition( m_vNewVPhysicsPosition, m_vNewVPhysicsVelocity, TICK_INTERVAL );

	MoveHelper()->SetHost( nullptr );
}

extern void TouchTriggerSoundOperator( CBaseEntity *pEntity );
void CBasePlayer::PhysicsTouchTriggers( const Vector *pPrevAbsOrigin )
{
	CBaseCombatCharacter::PhysicsTouchTriggers( pPrevAbsOrigin );

	if ( this == GetLocalPlayer() )
	{
		TouchTriggerSoundOperator( this );
	}
}

QAngle CBasePlayer::GetViewPunchAngle()
{
	return m_Local.m_viewPunchAngle.Get();
}

void CBasePlayer::SetViewPunchAngle( const QAngle &angle )
{
	m_Local.m_viewPunchAngle = angle;
}

QAngle CBasePlayer::GetAimPunchAngle()
{
	return m_Local.m_aimPunchAngle.Get();
}

void CBasePlayer::SetAimPunchAngle( const QAngle &angle )
{
	m_Local.m_aimPunchAngle = angle;
}

void CBasePlayer::SetAimPunchAngleVelocity( const QAngle &angleVelocity )
{
	m_Local.m_aimPunchAngleVel = angleVelocity;
}

QAngle CBasePlayer::GetFinalAimAngle()
{
	QAngle eyeAngles = EyeAngles();

	if ( PlatformInputDevice::IsInputDeviceAPointer( g_pInputSystem->GetCurrentInputDevice() ) )
	{
		// If we are using a pointing device, our final aim angle is based on where we're pointing and not where we're looking.
		VectorAngles( GetAimDirection(), eyeAngles );
	}

	return eyeAngles + GetAimPunchAngle();
}

float CBasePlayer::GetWaterJumpTime() const
{
	return m_flWaterJumpTime;
}

void CBasePlayer::SetWaterJumpTime( float flWaterJumpTime )
{
	m_flWaterJumpTime = flWaterJumpTime;
}

float CBasePlayer::GetSwimSoundTime() const
{
	return m_flSwimSoundTime;
}

void CBasePlayer::SetSwimSoundTime( float flSwimSoundTime )
{
	m_flSwimSoundTime = flSwimSoundTime;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this object can be +used by the player
//-----------------------------------------------------------------------------
bool CBasePlayer::IsUseableEntity( CBaseEntity *pEntity, unsigned int requiredCaps )
{
	return false;
}

CBaseEntity* CBasePlayer::GetUseEntity( void ) const 
{ 
	return m_hUseEntity;
}

CBaseEntity* CBasePlayer::GetPotentialUseEntity( void ) const 
{ 
	return GetUseEntity();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CBasePlayer::GetFOV( void ) const
{
	if ( GetObserverMode() == OBS_MODE_IN_EYE )
	{
		CBasePlayer *pTargetPlayer = ToBasePlayer( GetObserverTarget() );

		// get fov from observer target. Not if target is observer itself
		if ( pTargetPlayer && !pTargetPlayer->IsObserver() )
		{
			return pTargetPlayer->GetFOV();
		}
	}

	// Allow our vehicle to override our FOV if it's currently at the default FOV.
	float flDefaultFOV;
	flDefaultFOV = GetDefaultFOV();
	
	float fFOV = ( m_iFOV == 0 ) ? flDefaultFOV : m_iFOV;

	// Don't do lerping during prediction. It's only necessary when actually rendering,
	// and it'll cause problems due to prediction timing messiness.
	if ( !prediction->InPrediction() )
	{
		// See if we need to lerp the values for local player
		if ( IsLocalPlayer( this ) && ( fFOV != m_iFOVStart ) && (m_Local.m_flFOVRate > 0.0f ) )
		{
			float deltaTime = (float)( gpGlobals->curtime - m_flFOVTime ) / m_Local.m_flFOVRate;

			if ( GetPredictable() )
			{
				// m_flFOVTime was set to a predicted time in the future, because the FOV change was predicted.
				deltaTime = (float)( GetFinalPredictedTime() - m_flFOVTime );
				deltaTime += ( gpGlobals->interpolation_amount * TICK_INTERVAL );
				deltaTime /= m_Local.m_flFOVRate;
			}

			if ( deltaTime >= 1.0f )
			{
				//If we're past the zoom time, just take the new value and stop lerping
				const_cast<CBasePlayer *>(this)->m_iFOVStart = fFOV;
			}
			else
			{
				fFOV = SimpleSplineRemapValClamped( deltaTime, 0.0f, 1.0f, (float) m_iFOVStart, fFOV );
			}
		}
	}

	return fFOV;
}

void RecvProxy_LocalVelocityX( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBasePlayer *pPlayer = (CBasePlayer *) pStruct;

	contract_assert( pPlayer );

	float flNewVel_x = pData->m_Value.m_Float;

	Vector vecVelocity = pPlayer->GetLocalVelocity();

	if( vecVelocity.x != flNewVel_x )	// Should this use an epsilon check?
	{
		vecVelocity.x = flNewVel_x;
		pPlayer->SetLocalVelocity( vecVelocity );
	}
}

void RecvProxy_LocalVelocityY( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBasePlayer *pPlayer = (CBasePlayer *) pStruct;

	contract_assert( pPlayer );

	float flNewVel_y = pData->m_Value.m_Float;

	Vector vecVelocity = pPlayer->GetLocalVelocity();

	if( vecVelocity.y != flNewVel_y )
	{
		vecVelocity.y = flNewVel_y;
		pPlayer->SetLocalVelocity( vecVelocity );
	}
}

void RecvProxy_LocalVelocityZ( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBasePlayer *pPlayer = (CBasePlayer *) pStruct;
	
	contract_assert( pPlayer );

	float flNewVel_z = pData->m_Value.m_Float;

	Vector vecVelocity = pPlayer->GetLocalVelocity();

	if( vecVelocity.z != flNewVel_z )
	{
		vecVelocity.z = flNewVel_z;
		pPlayer->SetLocalVelocity( vecVelocity );
	}
}

void CBasePlayer::RecvProxy_ObserverMode( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	RecvProxy_Int32ToInt32( pData, pStruct, pOut );

	CBasePlayer *pPlayer = (CBasePlayer *) pStruct;
	contract_assert( pPlayer );
	
	if ( pPlayer )
		pPlayer->OnObserverModeChange( false );
}

void CBasePlayer::OnObserverModeChange( bool bIsObserverTarget )
{
	CBasePlayer *pPlayer = this;

	if ( CBasePlayer::IsLocalPlayer( pPlayer ) || bIsObserverTarget )
	{
		pPlayer->UpdateVisibility();
		UpdateViewmodelVisibility( pPlayer );
	}

	if ( bIsObserverTarget )
		return;

	// [msmith] When the observer mode changes, we also need to update the visibility of the view models for the
	// target we are observing.  This is important when changing between first and third person when in split screen.
	CBasePlayer* observerTarget = ToBasePlayer( pPlayer->GetObserverTarget() );
	if ( nullptr != observerTarget )
	{
		observerTarget->UpdateVisibility();
		UpdateViewmodelVisibility( observerTarget );
	}
}

void CBasePlayer::RecvProxy_ObserverTarget( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBasePlayer *pPlayer = (CBasePlayer *) pStruct;

	contract_assert( pPlayer );

	EHANDLE hTarget;

	RecvProxy_IntToEHandle( pData, pStruct, &hTarget );

	pPlayer->SetObserverTarget( hTarget );
}

void CBasePlayer::RecvProxy_LocalOriginXY( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBasePlayer *player = (CBasePlayer *) pStruct;
	player->m_vecHack_RecvProxy_LocalPlayerOrigin.x = pData->m_Value.m_Vector[0];
	player->m_vecHack_RecvProxy_LocalPlayerOrigin.y = pData->m_Value.m_Vector[1];

	((float*)pOut)[0] = pData->m_Value.m_Vector[0];
	((float*)pOut)[1] = pData->m_Value.m_Vector[1];
}

void CBasePlayer::RecvProxy_LocalOriginZ( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBasePlayer *player = (CBasePlayer *) pStruct;
	player->m_vecHack_RecvProxy_LocalPlayerOrigin.z = pData->m_Value.m_Float;

	*((float*)pOut) = pData->m_Value.m_Float;
}

void CBasePlayer::RecvProxy_NonLocalOriginXY( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	((float*)pOut)[0] = pData->m_Value.m_Vector[0];
	((float*)pOut)[1] = pData->m_Value.m_Vector[1];
}

void CBasePlayer::RecvProxy_NonLocalOriginZ( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	*((float*)pOut) = pData->m_Value.m_Float;
}

void CBasePlayer::RecvProxy_NonLocalCellOriginXY( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBasePlayer *player = (CBasePlayer *) pStruct;

	player->m_vecCellOrigin.x = pData->m_Value.m_Vector[0];
	player->m_vecCellOrigin.y = pData->m_Value.m_Vector[1];

	int const cellwidth = player->m_cellwidth; // Load it into a register
	((float*)pOut)[0] = CoordFromCell( cellwidth, player->m_cellX, pData->m_Value.m_Vector[0] );
	((float*)pOut)[1] = CoordFromCell( cellwidth, player->m_cellY, pData->m_Value.m_Vector[1] );
}

void CBasePlayer::RecvProxy_NonLocalCellOriginZ( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBasePlayer *player = (CBasePlayer *) pStruct;

	player->m_vecCellOrigin.z = pData->m_Value.m_Float;

	int const cellwidth = player->m_cellwidth; // Load it into a register
	*((float*)pOut) = CoordFromCell( cellwidth, player->m_cellZ, pData->m_Value.m_Float );
}

//-----------------------------------------------------------------------------
// Purpose: Remove this player from a vehicle


float CBasePlayer::GetMinFOV()	const
{
	if ( gpGlobals->maxClients == 1 )
	{
		// Let them do whatever they want, more or less, in single player
		return 5;
	}
	else
	{
		return 75;
	}
}

float CBasePlayer::GetFinalPredictedTime() const
{
	return ( m_nFinalPredictedTick * TICK_INTERVAL );
}

float CBasePlayer::PredictedServerTime() const
{
	return m_fLastUpdateServerTime + ((m_nTickBase - m_nLastUpdateTickBase) * TICK_INTERVAL);
}

void CBasePlayer::NotePredictionError( const Vector &vDelta )
{
	// don't worry about prediction errors when dead
	if ( !IsAlive() )
		return;

	Vector vOldDelta;

	GetPredictionErrorSmoothingVector( vOldDelta );

	// sum all errors within smoothing time
	m_vecPredictionError = vDelta + vOldDelta;

	// remember when last error happened
	m_flPredictionErrorTime = gpGlobals->curtime;
 
	ResetLatched(); 
}

// offset curtime and setup bones at that time using fake interpolation
// fake interpolation means we don't have reliable interpolation history (the local player doesn't animate locally)
// so we just modify cycle and origin directly and use that as a fake guess
void CBasePlayer::ForceSetupBonesAtTimeFakeInterpolation( matrix3x4a_t *pBonesOut, float curtimeOffset )
{
	// we don't have any interpolation data, so fake it
	float cycle = m_flCycle;
	Vector origin = GetLocalOrigin();

	// blow the cached prev bones
	InvalidateBoneCache();
	// reset root position to flTime
	Interpolate( gpGlobals->curtime + curtimeOffset );

	// force cycle back by boneDt
	m_flCycle = std::fmod( 10 + cycle + GetPlaybackRate() * curtimeOffset, 1.0f );
	SetLocalOrigin( origin + curtimeOffset * GetLocalVelocity() );
	// Setup bone state to extrapolate physics velocity
	SetupBones( pBonesOut, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, gpGlobals->curtime + curtimeOffset );

	m_flCycle = cycle;
	SetLocalOrigin( origin );
}

void CBasePlayer::GetRagdollInitBoneArrays( matrix3x4a_t *pDeltaBones0, matrix3x4a_t *pDeltaBones1, matrix3x4a_t *pCurrentBones, float boneDt )
{
	if ( !CBasePlayer::IsLocalPlayer( this ) )
	{
		BaseClass::GetRagdollInitBoneArrays(pDeltaBones0, pDeltaBones1, pCurrentBones, boneDt);
		return;
	}
	ForceSetupBonesAtTimeFakeInterpolation( pDeltaBones0, -boneDt );
	ForceSetupBonesAtTimeFakeInterpolation( pDeltaBones1, 0 );
	float ragdollCreateTime = PhysGetSyncCreateTime();
	if ( ragdollCreateTime != gpGlobals->curtime )
	{
		ForceSetupBonesAtTimeFakeInterpolation( pCurrentBones, ragdollCreateTime - gpGlobals->curtime );
	}
	else
	{
		SetupBones( pCurrentBones, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, gpGlobals->curtime );
	}
}

void CBasePlayer::GetPredictionErrorSmoothingVector( Vector &vOffset )
{
	if ( engine->IsPlayingDemo() || !cl_smooth.GetInt() || !cl_predict->GetInt() || engine->IsPaused() )
	{
		vOffset.Init();
		return;
	}

	float errorAmount = ( gpGlobals->curtime - m_flPredictionErrorTime ) / cl_smoothtime.GetFloat();

	if ( errorAmount >= 1.0f )
	{
		vOffset.Init();
		return;
	}
	
	errorAmount = 1.0f - errorAmount;

	vOffset = m_vecPredictionError * errorAmount;
}

IRagdoll* CBasePlayer::GetRepresentativeRagdoll() const
{
	return m_pRagdoll;
}

IMaterial *CBasePlayer::GetHeadLabelMaterial( void )
{
	if ( GetClientVoiceMgr() == nullptr )
		return nullptr;

	return GetClientVoiceMgr()->GetHeadLabelMaterial();
}

void CBasePlayer::UpdateSpeechVOIP( bool bVoice )
{
	m_bPlayerIsTalkingOverVOIP = ( bVoice && ShouldShowVOIPIcon() );

	if ( voice_icons_method.GetInt() == 2 )
		return;

	if ( m_bPlayerIsTalkingOverVOIP )
	{
		if ( !m_speechVOIPParticleEffect.IsValid() )
		{
			MDLCACHE_CRITICAL_SECTION();
			m_speechVOIPParticleEffect = GetVOIPParticleEffect();
		}
	}
	else
	{
		if ( m_speechVOIPParticleEffect.IsValid() )
		{
			ParticleProp()->StopEmissionAndDestroyImmediately( m_speechVOIPParticleEffect );
			m_speechVOIPParticleEffect = nullptr;
		}
	}
}

bool CBasePlayer::ShouldShowVOIPIcon() const
{
	return GameRules() && GameRules()->IsMultiplayer() && ( !IsLocalPlayer( this ) || voice_all_icons.GetBool() );
}

CNewParticleEffect *CBasePlayer::GetVOIPParticleEffect( void )
{
	return ParticleProp()->Create( GetVOIPParticleEffectName(), PATTACH_ABSORIGIN_FOLLOW, -1, ( EyePosition() - GetAbsOrigin() ) + Vector( 0.0f, 0.0f, GetClientVoiceMgr()->GetHeadLabelOffset() ) );
}

bool IsInFreezeCam( void )
{
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( pPlayer && pPlayer->GetObserverMode() == OBS_MODE_FREEZECAM )
		return true;

	return false;
}

void CBasePlayer::SetLastKillerDamageAndFreezeframe( int nLastKillerDamageTaken, int nLastKillerHitsTaken, int nLastKillerDamageGiven, int nLastKillerHitsGiven )
{
	m_nLastKillerDamageTaken = nLastKillerDamageTaken;
	m_nLastKillerHitsTaken = nLastKillerHitsTaken;
	m_nLastKillerDamageGiven = nLastKillerDamageGiven;
	m_nLastKillerHitsGiven = nLastKillerHitsGiven;

	m_bCanShowFreezeFrameNow = true;
	//m_bCheckHltvReplayAutoStart = spec_replay_autostart.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: Set the fog controller data per player.
// Input  : &inputdata -
//-----------------------------------------------------------------------------
void CBasePlayer::FogControllerChanged( bool bSnap )
{
	if ( m_PlayerFog.m_hCtrl )
	{
		fogparams_t	*pFogParams = &(m_PlayerFog.m_hCtrl->m_fog);

		/*
		Msg("FOG %s (%d) Updating Fog Target: (%d,%d,%d) %.0f,%.0f -> (%d,%d,%d) %.0f,%.0f (%.2f seconds)\n", 
			 GetPlayerName(), entindex(),
					m_CurrentFog.colorPrimary.GetR(), m_CurrentFog.colorPrimary.GetB(), m_CurrentFog.colorPrimary.GetG(), 
					m_CurrentFog.start.Get(), m_CurrentFog.end.Get(), 
					pFogParams->colorPrimary.GetR(), pFogParams->colorPrimary.GetB(), pFogParams->colorPrimary.GetG(), 
					pFogParams->start.Get(), pFogParams->end.Get(), pFogParams->duration.Get() );*/
		
		// Setup the fog color transition.
		m_PlayerFog.m_OldColor = m_CurrentFog.colorPrimary;
		m_PlayerFog.m_flOldStart = m_CurrentFog.start;
		m_PlayerFog.m_flOldEnd = m_CurrentFog.end;
		m_PlayerFog.m_flOldMaxDensity = m_CurrentFog.maxdensity;
		m_PlayerFog.m_flOldHDRColorScale = m_CurrentFog.HDRColorScale;
		m_PlayerFog.m_flOldFarZ = m_CurrentFog.farz;

		m_PlayerFog.m_NewColor = pFogParams->colorPrimary;
		m_PlayerFog.m_flNewStart = pFogParams->start;
		m_PlayerFog.m_flNewEnd = pFogParams->end;
		m_PlayerFog.m_flNewMaxDensity = pFogParams->maxdensity;
		m_PlayerFog.m_flNewHDRColorScale = pFogParams->HDRColorScale;
		m_PlayerFog.m_flNewFarZ = pFogParams->farz;

		m_PlayerFog.m_flTransitionTime = bSnap ? -1 : gpGlobals->curtime;

		m_PlayerFog.m_flZoomFogScale = pFogParams->ZoomFogScale;

		m_CurrentFog = *pFogParams;

		// Update the fog player's local fog data with the fog controller's data if need be.
		UpdateFogController();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check to see that the controllers data is up to date.
//-----------------------------------------------------------------------------
void CBasePlayer::UpdateFogController( void )
{
	if ( m_PlayerFog.m_hCtrl )
	{
		// Don't bother copying while we're transitioning, since it'll be stomped in UpdateFogBlend();
		if ( m_PlayerFog.m_flTransitionTime == -1 && (m_hOldFogController == m_PlayerFog.m_hCtrl) )
		{
			fogparams_t	*pFogParams = &(m_PlayerFog.m_hCtrl->m_fog);
			if ( m_CurrentFog != *pFogParams )
			{
				/*
					Msg("FOG %s (%d) FORCING UPDATE: (%d,%d,%d) %.0f,%.0f -> (%d,%d,%d) %.0f,%.0f (%.2f seconds)\n", 
							GetPlayerName(), entindex(),
										m_CurrentFog.colorPrimary.GetR(), m_CurrentFog.colorPrimary.GetB(), m_CurrentFog.colorPrimary.GetG(), 
										m_CurrentFog.start.Get(), m_CurrentFog.end.Get(), 
										pFogParams->colorPrimary.GetR(), pFogParams->colorPrimary.GetB(), pFogParams->colorPrimary.GetG(), 
										pFogParams->start.Get(), pFogParams->end.Get(), pFogParams->duration.Get() );*/
					
				m_CurrentFog = *pFogParams;
			}
		}
	}
	else
	{
		if ( m_CurrentFog.farz != -1 || m_CurrentFog.enable != false )
		{
			// No fog controller in this level. Use default fog parameters.
			m_CurrentFog.farz = -1;
			m_CurrentFog.enable = false;
		}
	}

	// Update the fog blending state - of necessary.
	UpdateFogBlend();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CBasePlayer::UpdateFogBlend( void )
{
	float flNewStart = m_PlayerFog.m_flNewStart;
	float flNewEnd = m_PlayerFog.m_flNewEnd;

	// Transition.
	if ( m_PlayerFog.m_flTransitionTime != -1 )
	{
		float flTimeDelta = gpGlobals->curtime - m_PlayerFog.m_flTransitionTime;
		if ( flTimeDelta < m_CurrentFog.duration )
		{
			float flScale = flTimeDelta / m_CurrentFog.duration;
			m_CurrentFog.colorPrimary.SetR( ( m_PlayerFog.m_NewColor.r * flScale ) + ( m_PlayerFog.m_OldColor.r * ( 1.0f - flScale ) ) );
			m_CurrentFog.colorPrimary.SetG( ( m_PlayerFog.m_NewColor.g * flScale ) + ( m_PlayerFog.m_OldColor.g * ( 1.0f - flScale ) ) );
			m_CurrentFog.colorPrimary.SetB( ( m_PlayerFog.m_NewColor.b * flScale ) + ( m_PlayerFog.m_OldColor.b * ( 1.0f - flScale ) ) );
			m_CurrentFog.start.Set( ( flNewStart * flScale ) + ( ( m_PlayerFog.m_flOldStart * ( 1.0f - flScale ) ) ) );
			m_CurrentFog.end.Set( ( flNewEnd * flScale ) + ( ( m_PlayerFog.m_flOldEnd * ( 1.0f - flScale ) ) ) );
			m_CurrentFog.maxdensity.Set( ( m_PlayerFog.m_flNewMaxDensity * flScale ) + ( ( m_PlayerFog.m_flOldMaxDensity * ( 1.0f - flScale ) ) ) );
			m_CurrentFog.HDRColorScale.Set( ( m_PlayerFog.m_flNewHDRColorScale * flScale ) + ( ( m_PlayerFog.m_flOldHDRColorScale * ( 1.0f - flScale ) ) ) );

			// Lerp to a sane FarZ (default value comes from CViewRender::GetZFar())
			float newFarZ = m_PlayerFog.m_flNewFarZ;
			if ( newFarZ <= 0 )
				newFarZ = r_mapextents.GetFloat() * 1.73205080757f;

			float oldFarZ = m_PlayerFog.m_flOldFarZ;
			if ( oldFarZ <= 0 )
				oldFarZ = r_mapextents.GetFloat() * 1.73205080757f;

			m_CurrentFog.farz.Set( ( newFarZ * flScale ) + ( ( oldFarZ * ( 1.0f - flScale ) ) ) );
		}
		else
		{
			// Slam the final fog values.
			m_CurrentFog.colorPrimary.SetR( m_PlayerFog.m_NewColor.r );
			m_CurrentFog.colorPrimary.SetG( m_PlayerFog.m_NewColor.g );
			m_CurrentFog.colorPrimary.SetB( m_PlayerFog.m_NewColor.b );
			m_CurrentFog.start.Set( flNewStart );
			m_CurrentFog.end.Set( flNewEnd );
			m_CurrentFog.maxdensity.Set( m_PlayerFog.m_flNewMaxDensity );
			m_CurrentFog.HDRColorScale.Set( m_PlayerFog.m_flNewHDRColorScale );
			m_CurrentFog.farz.Set( m_PlayerFog.m_flNewFarZ );
			m_PlayerFog.m_flTransitionTime = -1;

			/*
				Msg("FOG %s (%d) Finished transition to (%d,%d,%d) %.0f,%.0f\n", 
					 GetPlayerName(), entindex(),
								m_CurrentFog.colorPrimary.GetR(), m_CurrentFog.colorPrimary.GetB(), m_CurrentFog.colorPrimary.GetG(), 
								m_CurrentFog.start.Get(), m_CurrentFog.end.Get() );*/
				
		}
	}

	float flFov = GetFOV();
	float flDefaultFov = GetDefaultFOV( );
	if ( flFov < flDefaultFov )
	{
		float frac = (1.0 - std::max<std::common_type_t<double, float>>( 0.1, flFov / flDefaultFov )) * m_PlayerFog.m_flZoomFogScale;
		flNewEnd = flNewEnd + (800 * frac);
		flNewStart = flNewStart + (200 * frac);

		m_CurrentFog.start.Set( flNewStart );
		m_CurrentFog.end.Set( flNewEnd );
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CPostProcessController* CBasePlayer::GetActivePostProcessController() const
{
	return m_hPostProcessCtrl.Get();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CColorCorrection* CBasePlayer::GetActiveColorCorrection() const
{
	return m_hColorCorrectionCtrl.Get();
}

bool CBasePlayer::PreRender( int nSplitScreenPlayerSlot )
{
	if ( !IsVisible() || 
		!GetClientMode()->ShouldDrawLocalPlayer( this ) )
	{
		return true;
	}

	// Add in lighting effects
	return CreateLightEffects();
}

bool CBasePlayer::IsSplitScreenPartner( CBasePlayer *pPlayer )
{
	if ( !pPlayer || pPlayer == this )
		return false;

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		if ( s_pLocalPlayer[i] == pPlayer )
			return true;
	}	

	return false;
}

int CBasePlayer::GetSplitScreenPlayerSlot()
{
	contract_assert( ( m_nSplitScreenSlot >= 0 ) && ( m_nSplitScreenSlot < MAX_SPLITSCREEN_PLAYERS ) );
	return m_nSplitScreenSlot;
}

bool CBasePlayer::IsSplitScreenPlayer() const
{
	return m_nSplitScreenSlot >= 1;
}

CrossPlayPlatform_t CBasePlayer::GetCrossPlayPlatform( void ) const
{
	return CROSSPLAYPLATFORM_THISPLATFORM;
}

bool CBasePlayer::ShouldRegenerateOriginFromCellBits() const
{
	// Don't use cell bits for local players
	if ( 
		 (IsLocalPlayer( this ) ||
		  (!g_pGameRules->IsMultiplayer()) ) ) //SP load fails the IsLocalPlayer() test while creating the player. Resulting in a bad origin until you move
	{
		return false;
	}

	/*if ( g_pGameRules->IsMultiplayer() &&
		(GetCreationTick() == gpGlobals->tickcount) &&
		!CBasePlayer::HasAnyLocalPlayer() &&
		(engine->GetLocalPlayer() == index) )
	{
		return false;
	}*/

	return BaseClass::ShouldRegenerateOriginFromCellBits();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBasePlayer::GetPlayerID( CPlayerId *pID )
{

	return false;
}

void CBasePlayer::OnTimeJumpAllPlayers()
{
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer )
		{
			pPlayer->OnTimeJump();
		}
	}
}

void CBasePlayer::OnTimeJump()
{
}

void CC_DumpClientSoundscapeData( const CCommand& args )
{
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	Msg("Client Soundscape data dump:\n");
	Msg("   Position: %.2f %.2f %.2f\n", pPlayer->GetAbsOrigin().x, pPlayer->GetAbsOrigin().y, pPlayer->GetAbsOrigin().z );
	Msg("   soundscape index: %d\n", pPlayer->m_Local.m_audio.soundscapeIndex.Get() );
	Msg("   entity index: %d\n", pPlayer->m_Local.m_audio.entIndex.Get() );
	bool bFoundOne = false;
	for ( int i = 0; i < NUM_AUDIO_LOCAL_SOUNDS; i++ )
	{
		if ( pPlayer->m_Local.m_audio.localBits & (1<<i) )
		{
			if ( !bFoundOne )
			{
				Msg("   Sound Positions:\n");
				bFoundOne = true;
			}

			Vector vecPos = pPlayer->m_Local.m_audio.localSound[i];
			Msg("   %d: %.2f %.2f %.2f\n", i, vecPos.x,vecPos.y, vecPos.z );
		}
	}

	Msg("End dump.\n");
}
static ConCommand soundscape_dumpclient(side_registry(), "soundscape_dumpclient", CC_DumpClientSoundscapeData, "Dumps the client's soundscape data.\n", FCVAR_CHEAT);

bool CBasePlayer::IsObserver() const 
{ 
	return (GetObserverMode() != OBS_MODE_NONE); 
}

int CBasePlayer::GetImpulse( void ) const 
{ 
	return m_nImpulse; 
}

CCommandContext* CBasePlayer::GetCommandContext()
{
	return &m_CommandContext;
}

int CBasePlayer::CurrentCommandNumber() const
{
	contract_assert( m_pCurrentCommand );
	if ( !m_pCurrentCommand )
		return 0;
	return m_pCurrentCommand->command_number;
}

const CUserCmd *CBasePlayer::GetCurrentUserCommand() const
{
	contract_assert( m_pCurrentCommand );
	return m_pCurrentCommand;
}

bool CBasePlayer::IsHLTV() const
{
	return m_bIsLocalPlayer && g_bEngineIsHLTV;	
}

void CBasePlayer::SetTouchedPhysics( bool bTouch ) 
{ 
	m_bTouchedPhysObject = bTouch; 
}

bool CBasePlayer::TouchedPhysics( void )			
{ 
	return m_bTouchedPhysObject; 
}

CUserCmd const *CBasePlayer::GetLastUserCommand( void )
{
	return &m_LastCmd;
}

class CBaseHudWeaponSelection;
class CUserCmd;
class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CBaseViewModel;
class CCSPlayer;
class CRecipientFilter;

using namespace ::ks::reflect;

// Purpose: Implements shared baseplayer class functionality

//----------------------------------------------------
// Player Physics Shadow
//----------------------------------------------------
static constexpr auto VPHYS_MAX_DISTANCE = 2.0;
static constexpr auto VPHYS_MAX_VEL = 10;
inline constexpr auto VPHYS_MAX_DISTSQR = (VPHYS_MAX_DISTANCE*VPHYS_MAX_DISTANCE);
inline constexpr auto VPHYS_MAX_VELSQR = (VPHYS_MAX_VEL*VPHYS_MAX_VEL);

ConVar sv_infinite_ammo( side_registry(), "sv_infinite_ammo", "0", FCVAR_RELEASE | FCVAR_REPLICATED, "Player's active weapon will never run out of ammo. If set to 2 then player has infinite total ammo but still has to reload the magazine." );

ConVar view_punch_decay( side_registry(), "view_punch_decay", "18", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Decay factor exponent for view punch" );
ConVar view_recoil_tracking( side_registry(), "view_recoil_tracking", "0.45", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "How closely the view tracks with the aim punch from weapon recoil" );

ConVar sv_footstep_sound_frequency( side_registry(), "sv_footstep_sound_frequency", "0.97", FCVAR_CHEAT | FCVAR_REPLICATED, "How frequent to hear the player's step sound or how fast they appear to be running from first person." );

	static ConVar physicsshadowupdate_render( side_registry(), "cl_physicsshadowupdate_render", "0" );
	ConVar cl_shadowupdatespacing( side_registry(), "cl_shadowupdatespacing", "10.0" );
	ConVar cl_predict_motioncontrol( side_registry(), "cl_predict_motioncontrol", "0" );

ConVar mp_usehwmmodels( side_registry(), "mp_usehwmmodels", "0", 0, "Enable the use of the hw morph models. (-1 = never, 1 = always, 0 = based upon GPU)" ); // -1 = never, 0 = if hasfastvertextextures, 1 = always

extern ConVar sv_turbophysics;
extern CMoveData *g_pMoveData;

extern ConVar sv_coaching_enabled;

bool UseHWMorphModels()
{
	if ( mp_usehwmmodels.GetInt() == 0 )
		return g_pMaterialSystemHardwareConfig->HasFastVertexTextures();

	return mp_usehwmmodels.GetInt() > 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CBasePlayer::GetTimeBase( void ) const
{
	return m_nTickBase * TICK_INTERVAL;
}

float CBasePlayer::GetPlayerMaxSpeed()
{
	// player max speed is the lower limit of m_flMaxSpeed and sv_maxspeed
	float fMaxSpeed = sv_maxspeed.GetFloat();
	if ( MaxSpeed() > 0.0f && MaxSpeed() < fMaxSpeed )
		fMaxSpeed = MaxSpeed();

	return fMaxSpeed;
}

//-----------------------------------------------------------------------------
// Purpose: Called every usercmd by the player PreThink
//-----------------------------------------------------------------------------
void CBasePlayer::ItemPreFrame()
{
	// Handle use events
	PlayerUse();

	CBaseCombatWeapon *pActive = GetActiveWeapon();

	// Allow all the holstered weapons to update
	for ( int i = 0; i < WeaponCount(); ++i )
	{
		CBaseCombatWeapon *pWeapon = GetWeapon( i );

		if ( pWeapon == nullptr )
			continue;

		if ( pActive == pWeapon )
			continue;

		pWeapon->ItemHolsterFrame();
	}

    if ( gpGlobals->curtime < m_flNextAttack )
		return;

	if (!pActive)
		return;

	// Not predicting this weapon
	if ( !pActive->IsPredicted() )
		return;

	pActive->ItemPreFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.


//-----------------------------------------------------------------------------
// Purpose: Called every usercmd by the player PostThink
//-----------------------------------------------------------------------------
extern ConVar sv_infinite_ammo;
void CBasePlayer::ItemPostFrame()
{
	CVProfScope _( "CBasePlayer::ItemPostFrame", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Put viewmodels into basically correct place based on new player origin
	CalcViewModelView( EyePosition(), EyeAngles() );

	// Don't process items while in a vehicle.
	

	// check if the player is using something
	if ( m_hUseEntity != nullptr )
	{
		return;
	}

    if ( gpGlobals->curtime < m_flNextAttack )
	{
		if ( GetActiveWeapon() )
		{
			GetActiveWeapon()->ItemBusyFrame();
		}
	}
	else
	{
		if ( GetActiveWeapon() )
		{
			// Not predicting this weapon
			if ( GetActiveWeapon()->IsPredicted() )

			{
				GetActiveWeapon()->ItemPostFrame( );
			}
		}
	}

	// NOTE: If we ever support full impulse commands on the client,
	// remove this line and call ImpulseCommands instead.
	m_nImpulse = 0;

	if( ( sv_infinite_ammo.GetInt() == 1 ) && (GetActiveWeapon() != nullptr) )
	{
		CBaseCombatWeapon *pWeapon = GetActiveWeapon();

		pWeapon->m_iClip1 = pWeapon->GetMaxClip1();
		pWeapon->m_iClip2 = pWeapon->GetMaxClip2();

	}
}

//-----------------------------------------------------------------------------
// Eye angles
//-----------------------------------------------------------------------------
const QAngle &CBasePlayer::EyeAngles( )
{
	// NOTE: Viewangles are measured *relative* to the parent's coordinate system
	CBaseEntity *pMoveParent = const_cast<CBasePlayer*>(this)->GetMoveParent();

	if ( !pMoveParent )
	{
		// if in camera mode, use that
		if ( GetViewEntity() != nullptr )
		{
			return GetViewEntity()->EyeAngles();
		}
		return pl.v_angle;
	}

	// FIXME: Cache off the angles?
	matrix3x4_t eyesToParent, eyesToWorld;
	AngleMatrix( pl.v_angle, eyesToParent );
	ConcatTransforms( pMoveParent->EntityToWorldTransform(), eyesToParent, eyesToWorld );

	static QAngle angEyeWorld;
	MatrixAngles( eyesToWorld, angEyeWorld );
	return angEyeWorld;
}

const QAngle &CBasePlayer::LocalEyeAngles()
{
	return pl.v_angle;
}

//-----------------------------------------------------------------------------
// Actual Eye position + angles
//-----------------------------------------------------------------------------
Vector CBasePlayer::EyePosition( )
{
	{
		if ( IsObserver() )
		{
			if ( m_iObserverMode == OBS_MODE_CHASE )
			{
				if ( IsLocalPlayer( this ) )
				{
					return MainViewOrigin(GetSplitScreenPlayerSlot());
				}
			}
		}
		// if in camera mode, use that
		if ( GetViewEntity() != nullptr )
		{
			return GetViewEntity()->EyePosition();
		}

		if ( !IsLocalPlayer( this ) && IsAlive() )
		{
			if( ( GetFlags() & FL_DUCKING ) || m_Local.m_bDucked )
			{
				return GetAbsOrigin() + VEC_DUCK_VIEW;
			}
			else
			{
				return GetAbsOrigin() + VEC_VIEW;
			}
		}
		else
		{
			return BaseClass::EyePosition();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : const Vector
//-----------------------------------------------------------------------------
const Vector CBasePlayer::GetPlayerMins( void ) const
{
	if ( IsObserver() )
	{
		return VEC_OBS_HULL_MIN;	
	}
	else
	{
		if ( GetFlags() & FL_DUCKING )
		{
			return VEC_DUCK_HULL_MIN;
		}
		else
		{
			return VEC_HULL_MIN;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : const Vector
//-----------------------------------------------------------------------------
const Vector CBasePlayer::GetPlayerMaxs( void ) const
{	
	if ( IsObserver() )
	{
		return VEC_OBS_HULL_MAX;	
	}
	else
	{
		if ( GetFlags() & FL_DUCKING )
		{
			return VEC_DUCK_HULL_MAX;
		}
		else
		{
			return VEC_HULL_MAX;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayer::UpdateCollisionBounds( void )
{
	if ( GetFlags() & FL_DUCKING )
	{
		SetCollisionBounds( VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
	}
	else
	{
		SetCollisionBounds( VEC_HULL_MIN, VEC_HULL_MAX );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update the vehicle view, or simply return the cached position and angles


//-----------------------------------------------------------------------------
// Returns eye vectors
//-----------------------------------------------------------------------------
void CBasePlayer::EyeVectors( Vector *pForward, Vector *pRight, Vector *pUp )
{
	{
		AngleVectors( EyeAngles(), pForward, pRight, pUp );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the eye position and angle vectors.
//-----------------------------------------------------------------------------
void CBasePlayer::EyePositionAndVectors( Vector *pPosition, Vector *pForward,
										 Vector *pRight, Vector *pUp )
{
	// Handle the view in the vehicle
	{
		VectorCopy( EyePosition(), *pPosition );
		AngleVectors( EyeAngles(), pForward, pRight, pUp );
	}
}

surfacedata_t * CBasePlayer::GetFootstepSurface( const Vector &origin, const char *surfaceName )
{
	return physprops->GetSurfaceData( physprops->GetSurfaceIndex( surfaceName ) );
}

surfacedata_t *CBasePlayer::GetLadderSurface( const Vector &origin )
{
	return GetFootstepSurface( origin, "ladder" );
}

void CBasePlayer::UpdateStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity )
{
	bool bWalking;
	float fvol;
	Vector knee;
	Vector feet;
	float height;
	float speed;
	float velrun;
	float velwalk;
	bool fLadder;

	if ( m_flStepSoundTime > 0 )
	{
		m_flStepSoundTime -= 1000.0f * gpGlobals->frametime;
		if ( m_flStepSoundTime < 0 )
		{
			m_flStepSoundTime = 0;
		}
	}

	if ( m_flStepSoundTime > 0 )
		return;

	if ( GetFlags() & (FL_FROZEN|FL_ATCONTROLS))
		return;

	if ( GetMoveType() == MOVETYPE_NOCLIP || GetMoveType() == MOVETYPE_OBSERVER )
		return;

	if ( !sv_footsteps.GetFloat() )
		return;

	speed = VectorLength( vecVelocity );
	float groundspeed = Vector2DLength( vecVelocity.AsVector2D() );

	// determine if we are on a ladder
	fLadder = ( GetMoveType() == MOVETYPE_LADDER );

	GetStepSoundVelocities( &velwalk, &velrun );

	bool onground = ( GetFlags() & FL_ONGROUND );
	bool movingalongground = ( groundspeed > 0.0001f );
	bool moving_fast_enough =  ( speed >= velwalk );

	// To hear step sounds you must be either on a ladder or moving along the ground AND
	// You must be moving fast enough

	if ( !moving_fast_enough || !(fLadder || ( onground && movingalongground )) )
			return;

//	MoveHelper()->PlayerSetAnimation( PLAYER_WALK );

	bWalking = speed < velrun;

	VectorCopy( vecOrigin, knee );
	VectorCopy( vecOrigin, feet );

	height = GetPlayerMaxs()[ 2 ] - GetPlayerMins()[ 2 ];

	knee[2] = vecOrigin[2] + 0.2 * height;

	// find out what we're stepping in or on...
	if ( fLadder )
	{
		psurface = GetLadderSurface(vecOrigin);
		fvol = 0.5;

		SetStepSoundTime( STEPSOUNDTIME_ON_LADDER, bWalking );
	}
	else if ( enginetrace->GetPointContents( knee ) & MASK_WATER )  // we want to use the knee for Cstrike, not the waist
	{
		static int iSkipStep = 0;

		if ( iSkipStep == 0 )
		{
			iSkipStep++;
			return;
		}

		if ( iSkipStep++ == 3 )
		{
			iSkipStep = 0;
		}
		psurface = physprops->GetSurfaceData( physprops->GetSurfaceIndex( "wade" ) );
		fvol = 0.65;
		SetStepSoundTime( STEPSOUNDTIME_WATER_KNEE, bWalking );
	}
	else if ( GetWaterLevel() == WL_Feet )
	{
		psurface = physprops->GetSurfaceData( physprops->GetSurfaceIndex( "water" ) );
		fvol = bWalking ? 0.2 : 0.5;

		SetStepSoundTime( STEPSOUNDTIME_WATER_FOOT, bWalking );
	}
	else
	{
		if ( !psurface )
			return;

		SetStepSoundTime( STEPSOUNDTIME_NORMAL, bWalking );

		switch ( psurface->game.material )
		{
		default:
		case CHAR_TEX_CONCRETE:						
			fvol = bWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_METAL:	
			fvol = bWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_DIRT:
			fvol = bWalking ? 0.25 : 0.55;
			break;

		case CHAR_TEX_VENT:	
			fvol = bWalking ? 0.4 : 0.7;
			break;

		case CHAR_TEX_GRATE:
			fvol = bWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_TILE:	
			fvol = bWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_SLOSH:
			fvol = bWalking ? 0.2 : 0.5;
			break;
		}
	}
	
	// play the sound
	// 65% volume if ducking
	if ( GetFlags() & FL_DUCKING )
	{
		fvol *= 0.65;
	}

	PlayStepSound( feet, psurface, fvol, false );
}

ConVar sv_max_distance_transmit_footsteps( side_registry(), "sv_max_distance_transmit_footsteps", "1250.0", FCVAR_REPLICATED, "Maximum distance to transmit footstep sound effects." );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : step - 
//			fvol - 
//			force - force sound to play
//-----------------------------------------------------------------------------
extern ISoundEmitterSystemBase *soundemitterbase;
void CBasePlayer::PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool bForce )
{
	ks::ScratchArena<> scratch;
	if ( gpGlobals->maxClients > 1 && !sv_footsteps.GetFloat() )
		return;

	// during prediction play footstep sounds only once
	if ( prediction->InPrediction() && !prediction->IsFirstTimePredicted() )
		return;

	if ( !psurface )
		return;

	int nSide = m_Local.m_nStepside;
	unsigned short stepSoundName = nSide ? psurface->sounds.runStepLeft : psurface->sounds.runStepRight;
	if ( !stepSoundName )
		return;

	m_Local.m_nStepside = !nSide;

	CSoundParameters params;

	contract_assert( nSide == 0 || nSide == 1 );

	if ( m_StepSoundCache[ nSide ].m_usSoundNameIndex == stepSoundName )
	{
		params = m_StepSoundCache[ nSide ].m_SoundParameters;
	}
	else
	{
		IPhysicsSurfaceProps *physprops = MoveHelper()->GetSurfaceProps();
		
// footstep sounds
		const char *pRawSoundName = physprops->GetString( stepSoundName );
		const char *pSoundName = nullptr;
		int const nStepCopyLen = V_strlen(pRawSoundName) + 4;
		char *szStep = ( char * ) scratch.AllocBytes( nStepCopyLen );
		if ( GetTeamNumber() == TEAM_CT )
		{
			V_snprintf(szStep, nStepCopyLen, "ct_%s", pRawSoundName);
		}
		else
		{
			V_snprintf(szStep, nStepCopyLen, "t_%s", pRawSoundName);
		}

		pSoundName = szStep;
		if ( !CBaseEntity::GetParametersForSound( pSoundName, params, nullptr ) )
		{
			DevMsg( "Can't find specific footstep sound! (%s) - Using the default instead. (%s)\n", pSoundName, pRawSoundName );
			pSoundName = pRawSoundName;
		}
		if ( !CBaseEntity::GetParametersForSound( pSoundName, params, nullptr ) )
			return;

		// Only cache if there's one option.  Otherwise we'd never here any other sounds
		if ( params.count == 1 )
		{
			m_StepSoundCache[ nSide ].m_usSoundNameIndex = stepSoundName;
			m_StepSoundCache[ nSide ].m_SoundParameters = params;
		}
	}

	CRecipientFilter filter;
	
	// make sure we hear our own jump
	filter.AddRecipient( this );
	if ( prediction->InPrediction() && !bForce )
	{
		// Only use these rules when in prediction.
		filter.UsePredictionRules();
	}

	if( !bForce )
	{
		filter.AddRecipientsByPAS( vecOrigin );
	}

// #if defined( CLIENT_DLL )
// 	Msg( "CLIENT_DLL: (PlayStepSound) filter recipients = %d\n", filter.GetRecipientCount() );
// #else
// 	Msg( "GAME_DLL: (PlayStepSound) filter recipients = %d\n", filter.GetRecipientCount() );
// #endif

	EmitSound_t ep;
	ep.m_nChannel = CHAN_BODY;
	ep.m_pSoundName = params.soundname;
	ep.m_flVolume = fvol;
	ep.m_SoundLevel = params.soundlevel;
	ep.m_nFlags = 0;
	ep.m_nPitch = params.pitch;
	ep.m_pOrigin = &vecOrigin;
	ep.m_hSoundScriptHash = params.m_hSoundScriptHash;
	ep.m_nSoundEntryVersion = params.m_nSoundEntryVersion;

	EmitSound( filter, entindex(), ep );

	// Step Suit
	if (CCSPlayer *pThisCsPlayer = dynamic_cast<CCSPlayer *>(this))
	{
		if (pThisCsPlayer->IsBot() && pThisCsPlayer->HasHeavyArmor())
		{
			static const char * const k_HeavyStepSoundName = "Heavy.Step";
			static HSOUNDSCRIPTHASH const k_HeavyStepSoundHash = soundemitterbase->HashSoundName( k_HeavyStepSoundName );
			ep.m_pSoundName = k_HeavyStepSoundName;
			ep.m_hSoundScriptHash = k_HeavyStepSoundHash;
			EmitSound(filter, entindex(), ep);
		}
	}
	CSoundParameters paramsSuitSound;
	if (!CBaseEntity::GetParametersForSound((GetTeamNumber() == TEAM_CT) ? "CT_Default.Suit" : "T_Default.Suit", paramsSuitSound, nullptr))
		return;

	EmitSound_t epSuitSound;
	epSuitSound.m_nChannel = CHAN_AUTO;
	epSuitSound.m_pSoundName = paramsSuitSound.soundname;
	epSuitSound.m_flVolume = fvol;
	epSuitSound.m_SoundLevel = paramsSuitSound.soundlevel;
	epSuitSound.m_nFlags = 0;
	epSuitSound.m_nPitch = paramsSuitSound.pitch;
	epSuitSound.m_pOrigin = &vecOrigin;
	epSuitSound.m_hSoundScriptHash = paramsSuitSound.m_hSoundScriptHash;
	epSuitSound.m_nSoundEntryVersion = paramsSuitSound.m_nSoundEntryVersion;

	EmitSound(filter, entindex(), epSuitSound);

}

void CBasePlayer::UpdateButtonState( int nUserCmdButtonMask )
{
	// Track button info so we can detect 'pressed' and 'released' buttons next frame
	m_afButtonLast = m_nButtons;

	// Get button states
	m_nButtons = nUserCmdButtonMask;
 	int buttonsChanged = m_afButtonLast ^ m_nButtons;
	
	// Debounced button codes for pressed/released
	// UNDONE: Do we need auto-repeat?
	m_afButtonPressed =  buttonsChanged & m_nButtons;		// The changed ones still down are "pressed"
	m_afButtonReleased = buttonsChanged & (~m_nButtons);	// The ones not down are "released"
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayer::GetStepSoundVelocities( float *velwalk, float *velrun )
{
	// UNDONE: need defined numbers for run, walk, crouch, crouch run velocities!!!!	
	if ( ( GetFlags() & FL_DUCKING) || ( GetMoveType() == MOVETYPE_LADDER ) )
	{
		*velwalk = 60;		// These constants should be based on cl_movespeedkey * cl_forwardspeed somehow
		*velrun = 80;		
	}
	else
	{
		*velwalk = 90;
		*velrun = 220;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayer::SetStepSoundTime( stepsoundtimes_t iStepSoundTime, bool bWalking )
{
	switch ( iStepSoundTime )
	{
	case STEPSOUNDTIME_NORMAL:
	case STEPSOUNDTIME_WATER_FOOT:
		m_flStepSoundTime = bWalking ? 400 : 300;
		break;

	case STEPSOUNDTIME_ON_LADDER:
		m_flStepSoundTime = 200;
		break;

	case STEPSOUNDTIME_WATER_KNEE:
		m_flStepSoundTime = 600;
		break;

	default:
		contract_assert( 0 );
		break;
	}

	m_flStepSoundTime *= sv_footstep_sound_frequency.GetFloat();

	// UNDONE: need defined numbers for run, walk, crouch, crouch run velocities!!!!	
	if ( ( GetFlags() & FL_DUCKING) || ( GetMoveType() == MOVETYPE_LADDER ) )
	{
		m_flStepSoundTime += 100;
	}
}

Vector CBasePlayer::Weapon_ShootPosition( )
{
	return EyePosition();
}

bool CBasePlayer::Weapon_CanUse( CBaseCombatWeapon *pWeapon )
{
	return true;
}

void CBasePlayer::SetAnimationExtension( const char *pExtension )
{
	V_strncpy( m_szAnimExtension, pExtension, sizeof(m_szAnimExtension) );
}

//-----------------------------------------------------------------------------
// Purpose: Set the weapon to switch to when the player uses the 'lastinv' command
//-----------------------------------------------------------------------------
void CBasePlayer::Weapon_SetLast( CBaseCombatWeapon *pWeapon )
{
	m_hLastWeapon = pWeapon;
}

//-----------------------------------------------------------------------------
// Purpose: Override to clear dropped weapon from the hud
//-----------------------------------------------------------------------------
void CBasePlayer::Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget /* = NULL */, const Vector *pVelocity /* = NULL */ )
{
	bool bWasActiveWeapon = false;
	if ( pWeapon == GetActiveWeapon() )
	{
		bWasActiveWeapon = true;
	}

	if ( pWeapon )
	{
		if ( bWasActiveWeapon )
		{
			pWeapon->SendWeaponAnim( ACT_VM_IDLE );
		}
	}

	if ( bWasActiveWeapon )
	{
		if (!SwitchToNextBestWeapon( nullptr ))
		{
			CBaseViewModel *vm = GetViewModel();
			if ( vm )
			{
				vm->AddEffects( EF_NODRAW );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Override base class so player can reset autoaim
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CBasePlayer::Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex /*=0*/ ) 
{
	CBaseCombatWeapon *pLastWeapon = GetActiveWeapon();

	if ( BaseClass::Weapon_Switch( pWeapon, viewmodelindex ))
	{
		if ( pLastWeapon && Weapon_ShouldSetLast( pLastWeapon, GetActiveWeapon() ) )
		{
			Weapon_SetLast( pLastWeapon->GetLastWeapon() );
		}

		CBaseViewModel *pViewModel = GetViewModel( viewmodelindex );
		contract_assert( pViewModel );
		if ( pViewModel )
			pViewModel->RemoveEffects( EF_NODRAW );
		ResetAutoaim( );
		OnSwitchWeapons( pWeapon );
		CBaseHudWeaponSelection *pHudSelection = GetHudWeaponSelection();
		if ( pHudSelection )
		{
			pHudSelection->OnWeaponSwitch( pWeapon );
		}
		return true;
	}
	return false;
}

void CBasePlayer::SelectLastItem(void)
{
	if ( m_hLastWeapon.Get() == nullptr )
		return;

	if ( GetActiveWeapon() && !GetActiveWeapon()->CanHolster() )
		return;

	SelectItem( m_hLastWeapon.Get()->GetClassname(), m_hLastWeapon.Get()->GetSubType() );
}

//-----------------------------------------------------------------------------
// Purpose: Abort any reloads we're in
//-----------------------------------------------------------------------------
void CBasePlayer::AbortReload( void )
{
	if ( GetActiveWeapon() )
	{
		GetActiveWeapon()->AbortReload();
	}
}

void CBasePlayer::AddToPlayerSimulationList( CBaseEntity *other )
{
	CHandle< CBaseEntity > h;
	h = other;
	// Already in list
	if ( ks::find_index( m_SimulatedByThisPlayer, h ) != -1 )
		return;

	contract_assert( other->IsPlayerSimulated() );

	m_SimulatedByThisPlayer.push_back( h );
}

//-----------------------------------------------------------------------------
// Purpose: Fixme, this should occur if the player fails to drive simulation
//  often enough!!!
// Input  : *other - 
//-----------------------------------------------------------------------------
void CBasePlayer::RemoveFromPlayerSimulationList( CBaseEntity *other )
{
	if ( !other )
		return;

	contract_assert( other->IsPlayerSimulated() );
	contract_assert( other->GetSimulatingPlayer() == this );

	CHandle< CBaseEntity > h;
	h = other;

	ks::erase_first( m_SimulatedByThisPlayer, h );
}

void CBasePlayer::SimulatePlayerSimulatedEntities( void )
{
	int c = std::ssize( m_SimulatedByThisPlayer );
	int i;

	for ( i = c - 1; i >= 0; i-- )
	{
		CHandle< CBaseEntity > h;
		
		h = m_SimulatedByThisPlayer[ i ];
		CBaseEntity *e = h;

		if ( !e || !e->IsPlayerSimulated() )
		{
			m_SimulatedByThisPlayer.erase( m_SimulatedByThisPlayer.begin() + i );
			continue;
		}

		contract_assert( e->IsPlayerSimulated() );
		contract_assert( e->GetSimulatingPlayer() == this );

		e->PhysicsSimulate();
	}

	// Loop through all entities again, checking their untouch if flagged to do so
	c = std::ssize( m_SimulatedByThisPlayer );

	for ( i = c - 1; i >= 0; i-- )
	{
		CHandle< CBaseEntity > h;
		
		h = m_SimulatedByThisPlayer[ i ];

		CBaseEntity *e = h;
		if ( !e || !e->IsPlayerSimulated() )
		{
			m_SimulatedByThisPlayer.erase( m_SimulatedByThisPlayer.begin() + i );
			continue;
		}

		contract_assert( e->IsPlayerSimulated() );
		contract_assert( e->GetSimulatingPlayer() == this );

		if ( !e->GetCheckUntouch() )
			continue;

		e->PhysicsCheckForEntityUntouch();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayer::ClearPlayerSimulationList( void )
{
	int c = std::ssize( m_SimulatedByThisPlayer );
	int i;

	for ( i = c - 1; i >= 0; i-- )
	{
		CHandle< CBaseEntity > h;
		
		h = m_SimulatedByThisPlayer[ i ];
		CBaseEntity *e = h;
		if ( e )
		{
			e->UnsetPlayerSimulated();
		}
	}

	m_SimulatedByThisPlayer.clear();
}

//-----------------------------------------------------------------------------
// Purpose: Return true if we should allow selection of the specified item
//-----------------------------------------------------------------------------
bool CBasePlayer::Weapon_ShouldSelectItem( CBaseCombatWeapon *pWeapon )
{
	return ( pWeapon != GetActiveWeapon() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayer::SelectItem( const char *pstr, int iSubType )
{
	if (!pstr)
		return;

	CBaseCombatWeapon *pItem = Weapon_OwnsThisType( pstr, iSubType );

	if (!pItem)
		return;

	if( GetObserverMode() != OBS_MODE_NONE )
		return;// Observers can't select things.

	if ( !Weapon_ShouldSelectItem( pItem ) )
		return;

	// FIX, this needs to queue them up and delay
	// Make sure the current weapon can be holstered
	if ( GetActiveWeapon() )
	{
		if ( !GetActiveWeapon()->CanHolster() )
			return;

		ResetAutoaim( );
	}

	Weapon_Switch( pItem );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ConVar sv_debug_player_use( side_registry(), "sv_debug_player_use", "0", FCVAR_REPLICATED, "Visualizes +use logic. Green cross=trace success, Red cross=trace too far, Green box=radius success" );
float IntervalDistance( float x, float x0, float x1 )
{
	// swap so x0 < x1
	if ( x0 > x1 )
	{
		float tmp = x0;
		x0 = x1;
		x1 = tmp;
	}

	if ( x < x0 )
		return x0-x;
	else if ( x > x1 )
		return x - x1;
	return 0;
}

CBaseEntity *CBasePlayer::FindUseEntity()
{
	Vector forward, up;
	// NOTE: This doesn't handle the case when the player is in a vehicle.
	AngleVectors( GetFinalAimAngle(), &forward, nullptr, &up );

	trace_t tr;
	// Search for objects in a sphere (tests for entities that are not solid, yet still useable)
	Vector searchCenter = EyePosition();

	// NOTE: Some debris objects are useable too, so hit those as well
	// A button, etc. can be made out of clip brushes, make sure it's +useable via a traceline, too.
	int useableContents = MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_PLAYERCLIP;

	useableContents = (MASK_NPCSOLID_BRUSHONLY | MASK_OPAQUE_AND_NPCS) & ~CONTENTS_OPAQUE;

	// UNDONE: Might be faster to just fold this range into the sphere query
	CBaseEntity *pObject = nullptr;

	float nearestDist = std::numeric_limits<float>::max();
	// try the hit entity if there is one, or the ground entity if there isn't.
	CBaseEntity *pNearest = nullptr;

	const int NUM_TANGENTS = 8;

	const int NUM_TRACES = NUM_TANGENTS;
	// trace a box at successive angles down
	//							forward, 45 deg, 30 deg, 20 deg, 15 deg, 10 deg, -10, -15
	const float tangents[NUM_TANGENTS] = { 0, 1, 0.57735026919f, 0.3639702342f, 0.267949192431f, 0.1763269807f, -0.1763269807f, -0.267949192431f };
	for ( int i = 0; i < NUM_TRACES; i++ )
	{
		if ( i == 0 )
		{
			UTIL_TraceLine( searchCenter, searchCenter + forward * 1024, useableContents, this, COLLISION_GROUP_NONE, &tr );
		}
		else
		{
			Vector down = forward - tangents[i]*up;
			VectorNormalize(down);
			UTIL_TraceHull( searchCenter, searchCenter + down * 72, -Vector(16,16,16), Vector(16,16,16), useableContents, this, COLLISION_GROUP_NONE, &tr );
		}
		pObject = tr.Ent<CBaseEntity>();

		bool bUsable = IsUseableEntity(pObject, 0);
		while ( pObject && !bUsable && pObject->GetMoveParent() )
		{
			pObject = pObject->GetMoveParent();
			bUsable = IsUseableEntity(pObject, 0);
		}

		if ( bUsable )
		{
			Vector delta = tr.endpos - tr.startpos;
			float centerZ = CollisionProp()->WorldSpaceCenter().z;
			delta.z = IntervalDistance( tr.endpos.z, centerZ + CollisionProp()->OBBMins().z, centerZ + CollisionProp()->OBBMaxs().z );
			float dist = delta.Length();
			CCSPlayer *pPlayer = dynamic_cast<CCSPlayer*>( pObject );
			if ( (pPlayer && pPlayer->IsBot() && dist < PLAYER_USE_BOT_RADIUS) || dist < PLAYER_USE_RADIUS )
			{
				if ( sv_debug_player_use.GetBool() )
				{
					Msg( "Trace using: %s\n", pObject ? pObject->GetDebugName() : "no usable entity found" );
				}

				pNearest = pObject;
				
				// if this is directly under the cursor just return it now
				if ( i == 0 )
					return pObject;
			}
		}
	}

	// check ground entity first
	// if you've got a useable ground entity, then shrink the cone of this search to 45 degrees
	// otherwise, search out in a 90 degree cone (hemisphere)
	if ( GetGroundEntity() && IsUseableEntity(GetGroundEntity(), FCAP_USE_ONGROUND) )
	{
		pNearest = GetGroundEntity();
	}
	if ( pNearest )
	{
		// estimate nearest object by distance from the view vector
		Vector point;
		pNearest->CollisionProp()->CalcNearestPoint( searchCenter, &point );
		nearestDist = CalcDistanceToLine( point, searchCenter, forward );
		if ( sv_debug_player_use.GetBool() )
		{
			Msg("Trace found %s, dist %.2f\n", pNearest->GetClassname(), nearestDist );
		}
	}

	for ( CEntitySphereQuery sphere( searchCenter, PLAYER_USE_RADIUS ); ( pObject = sphere.GetCurrentEntity() ) != nullptr; sphere.NextEntity() )
	{
		if ( !pObject )
			continue;

		if ( !IsUseableEntity( pObject, FCAP_USE_IN_RADIUS ) )
			continue;

		// see if it's more roughly in front of the player than previous guess
		Vector point;
		pObject->CollisionProp()->CalcNearestPoint( searchCenter, &point );

		float fMinimumDot = pObject->GetUseLookAtAngle();

		Vector dir = point - searchCenter;
		VectorNormalize(dir);
		float dot = DotProduct( dir, forward );

		// Need to be looking at the object more or less
		if ( dot < fMinimumDot )
			continue;

		float dist = CalcDistanceToLine( point, searchCenter, forward );

		if ( sv_debug_player_use.GetBool() )
		{
			Msg("Radius found %s, dist %.2f\n", pObject->GetClassname(), dist );
		}

		if ( dist < nearestDist )
		{
			// Since this has purely been a radius search to this point, we now
			// make sure the object isn't behind glass or a grate.
			trace_t trCheckOccluded;
			UTIL_TraceLine( searchCenter, point, useableContents, this, COLLISION_GROUP_NONE, &trCheckOccluded );

			if ( trCheckOccluded.fraction == 1.0 || trCheckOccluded.Ent<CBaseEntity>() == pObject )
			{
				pNearest = pObject;
				nearestDist = dist;
			}
		}
	}

	if ( sv_debug_player_use.GetBool() )
	{
		Msg( "Radial using: %s\n", pNearest ? pNearest->GetDebugName() : "no usable entity found" );
	}

	return pNearest;
}

//-----------------------------------------------------------------------------
// Purpose: Handles USE keypress
//-----------------------------------------------------------------------------
void CBasePlayer::PlayerUse ( void )
{
}

ConVar	sv_suppress_viewpunch( side_registry(), "sv_suppress_viewpunch", "0", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayer::ViewPunch( const QAngle &angleOffset )
{
	//See if we're suppressing the view punching
	if ( sv_suppress_viewpunch.GetBool() )
		return;

	// We don't allow view kicks in the vehicle
	

	m_Local.m_viewPunchAngle += angleOffset;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayer::ViewPunchReset( float tolerance )
{
	if ( tolerance != 0 )
	{
		tolerance *= tolerance;	// square
		float check = m_Local.m_viewPunchAngle->LengthSqr();
		if ( check > tolerance )
			return;
	}
	m_Local.m_viewPunchAngle = vec3_angle;
}

static ConVar smoothstairs( side_registry(), "smoothstairs", "1", FCVAR_REPLICATED, "Smooth player eye z coordinate when traversing stairs." );

//-----------------------------------------------------------------------------
// Handle view smoothing when going up or down stairs
//-----------------------------------------------------------------------------
void CBasePlayer::SmoothViewOnStairs( Vector& eyeOrigin )
{
	CBaseEntity *pGroundEntity = GetGroundEntity();
	float flCurrentPlayerZ = GetLocalOrigin().z;
	float flCurrentPlayerViewOffsetZ = GetViewOffset().z;

	{
		matrix3x4_t matOriginDisontinuity;
		if( GetOriginInterpolator().GetDiscontinuityTransform( GetEffectiveInterpolationCurTime( gpGlobals->curtime ), matOriginDisontinuity ) )
		{
			//if the origin has a discontinuity, assume that m_flOldPlayerZ is in the new space. Therefore, we need to transform the local origin into the new space for comparisons
			Vector vCurrentSpaceLocalOrigin;
			VectorITransform( GetLocalOrigin(), matOriginDisontinuity, vCurrentSpaceLocalOrigin ); //inverse transform because the matrix goes from new space to old space
			flCurrentPlayerZ = vCurrentSpaceLocalOrigin.z;
		}
	}

	// Smooth out stair step ups
	// NOTE: Don't want to do this when the ground entity is moving the player
	if ( ( pGroundEntity != nullptr && pGroundEntity->GetMoveType() == MOVETYPE_NONE ) && ( flCurrentPlayerZ != m_flOldPlayerZ ) && smoothstairs.GetBool() &&
		 m_flOldPlayerViewOffsetZ == flCurrentPlayerViewOffsetZ )
	{
		int dir = ( flCurrentPlayerZ > m_flOldPlayerZ ) ? 1 : -1;

		float steptime = gpGlobals->frametime;
		if (steptime < 0)
		{
			steptime = 0;
		}

		m_flOldPlayerZ += steptime * 150 * dir;

		const float stepSize = 18.0f;

		if ( dir > 0 )
		{
			if (m_flOldPlayerZ > flCurrentPlayerZ)
			{
				m_flOldPlayerZ = flCurrentPlayerZ;
			}
			if (flCurrentPlayerZ - m_flOldPlayerZ > stepSize)
			{
				m_flOldPlayerZ = flCurrentPlayerZ - stepSize;
			}
		}
		else
		{
			if (m_flOldPlayerZ < flCurrentPlayerZ)
			{
				m_flOldPlayerZ = flCurrentPlayerZ;
			}
			if (flCurrentPlayerZ - m_flOldPlayerZ < -stepSize)
			{
				m_flOldPlayerZ = flCurrentPlayerZ + stepSize;
			}
		}

		eyeOrigin[2] += m_flOldPlayerZ - flCurrentPlayerZ;
	}
	else
	{
		m_flOldPlayerZ = flCurrentPlayerZ;
		m_flOldPlayerViewOffsetZ = flCurrentPlayerViewOffsetZ;
	}
}

static bool IsWaterContents( int contents )
{
	if ( contents & MASK_WATER )
		return true;

//	if ( contents & CONTENTS_TESTFOGVOLUME )
//		return true;

	return false;
}

void CBasePlayer::ResetObserverMode()
{
	m_hObserverTarget.Set( 0 );
	m_iObserverMode = (int)OBS_MODE_NONE;

	UpdateVisibility();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : eyeOrigin - 
//			eyeAngles - 
//			zNear - 
//			zFar - 
//			fov - 
//-----------------------------------------------------------------------------
extern Vector g_cameraFollowPos;
void CBasePlayer::CalcView( Vector &eyeOrigin, QAngle &eyeAngles, float &zNear, float &zFar, float &fov )
{
	if ( IsObserver() )
	{
		CalcObserverView( eyeOrigin, eyeAngles, fov );
	}
	else
	{
		CalcPlayerView( eyeOrigin, eyeAngles, fov );
	}

	// Set the follow bone if necessary
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );

		static ConVarRef cvFollowBoneIndexVar( "cl_camera_follow_bone_index" );

		CStudioHdr const* pHdr = GetModelPtr();

		if ( pHdr &&
			 cvFollowBoneIndexVar.IsValid() && 
			 CBasePlayer::GetLocalPlayer() == this )
		{
			int boneIdx = cvFollowBoneIndexVar.GetInt();
			if ( boneIdx >= -1 && boneIdx <	pHdr->numbones() )
			{
				if ( boneIdx == -1 )
				{
					VectorCopy( GetRenderOrigin(), g_cameraFollowPos );
				}
				else if ( pHdr->pBone( boneIdx )->flags & BONE_USED_BY_ANYTHING )
				{
					MatrixPosition( m_BoneAccessor.GetBone( boneIdx ), g_cameraFollowPos );
				}
			}
		}
	}
}

void CBasePlayer::CalcViewModelView( const Vector& eyeOrigin, const QAngle& eyeAngles)
{
	for ( int i = 0; i < MAX_VIEWMODELS; i++ )
	{
		CBaseViewModel *vm = GetViewModel( i );
		if ( !vm )
			continue;
	
		vm->CalcViewModelView( this, eyeOrigin, eyeAngles );
	}
}

void CBasePlayer::CalcPlayerView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov )
{
	if ( !prediction->InPrediction() )
	{
		// FIXME: Move into prediction
		view->DriftPitch();
	}

	// TrackIR
	if ( IsHeadTrackingEnabled() )
	{
		VectorCopy( EyePosition() + GetEyeOffset(), eyeOrigin );
	}
	else
	{
		VectorCopy( EyePosition(), eyeOrigin );
	}
	
	VectorCopy( EyeAngles(), eyeAngles );

	if ( !prediction->InPrediction() )
	{
		SmoothViewOnStairs( eyeOrigin );
	}

	// Snack off the origin before bob + water offset are applied
	Vector vecBaseEyePosition = eyeOrigin;
	QAngle baseEyeAngles = eyeAngles;

	CalcViewBob( eyeOrigin );
	CalcViewRoll( eyeAngles );

	CalcAddViewmodelCameraAnimation( eyeOrigin, eyeAngles );

	// Apply punch angles
	VectorAdd( eyeAngles, m_Local.m_viewPunchAngle, eyeAngles );

	// TODO[pmf]: apply a scaling factor to this
	VectorAdd( eyeAngles, GetAimPunchAngle() * view_recoil_tracking.GetFloat(), eyeAngles );

	if ( !prediction->InPrediction() )
	{
		// Shake it up baby!
		GetViewEffects()->CalcShake();
		GetViewEffects()->ApplyShake( eyeOrigin, eyeAngles, 1.0 );

		// Tilting handled in CInput::AdjustAngles
	}

	// Apply a smoothing offset to smooth out prediction errors.
	Vector vSmoothOffset;
	GetPredictionErrorSmoothingVector( vSmoothOffset );
	eyeOrigin += vSmoothOffset;
	m_flObserverChaseDistance = 0.0;

	// calc current FOV
	fov = GetFOV();
}

//-----------------------------------------------------------------------------
// Purpose: The main view setup function for vehicles


void CBasePlayer::CalcObserverView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov )
{
	switch ( GetObserverMode() )
	{

		case OBS_MODE_DEATHCAM	:	CalcDeathCamView( eyeOrigin, eyeAngles, fov );
									break;

		case OBS_MODE_ROAMING	:	// just copy current position without view offset
		case OBS_MODE_FIXED		:	CalcRoamingView( eyeOrigin, eyeAngles, fov );
									break;

		case OBS_MODE_IN_EYE	:	CalcInEyeCamView( eyeOrigin, eyeAngles, fov );
									break;

		case OBS_MODE_CHASE		:	CalcChaseCamView( eyeOrigin, eyeAngles, fov  );
									break;

		case OBS_MODE_FREEZECAM	:	CalcFreezeCamView( eyeOrigin, eyeAngles, fov  );
									break;
	}

}

//-----------------------------------------------------------------------------
// Purpose: Compute roll angle for a particular lateral velocity
// Input  : angles - 
//			velocity - 
//			rollangle - 
//			rollspeed - 
// Output : float CViewRender::CalcRoll
//-----------------------------------------------------------------------------
float CBasePlayer::CalcRoll (const QAngle& angles, const Vector& velocity, float rollangle, float rollspeed)
{
    float   sign;
    float   side;
    float   value;
	
	Vector  forward, right, up;
	
    AngleVectors (angles, &forward, &right, &up);
	
	// Get amount of lateral movement
    side = DotProduct( velocity, right );
	// Right or left side?
    sign = side < 0 ? -1 : 1;
    side = fabs(side);
    
	value = rollangle;
	// Hit 100% of rollangle at rollspeed.  Below that get linear approx.
    if ( side < rollspeed )
	{
		side = side * value / rollspeed;
	}
    else
	{
		side = value;
	}

	// Scale by right/left sign
    return side*sign;
}

//-----------------------------------------------------------------------------
// Purpose: Allow the viewmodel to layer in artist-authored additive camera animation (to make some first-person anims 'punchier')
//-----------------------------------------------------------------------------
static constexpr auto CAM_DRIVER_RETURN_TO_NORMAL = 0.25f;
static constexpr auto CAM_DRIVER_RETURN_TO_NORMAL_GAIN = 0.8f;
void CBasePlayer::CalcAddViewmodelCameraAnimation( Vector& eyeOrigin, QAngle& eyeAngles )
{
	CBaseViewModel *vm = GetViewModel();
	if ( vm && vm->GetModelPtr() )
	{
		float flTimeDelta = clamp( (gpGlobals->curtime - vm->m_flCamDriverAppliedTime), 0, CAM_DRIVER_RETURN_TO_NORMAL);
		if ( flTimeDelta < CAM_DRIVER_RETURN_TO_NORMAL )
		{
			vm->m_flCamDriverWeight = clamp( Gain( RemapValClamped( flTimeDelta, 0.0f, CAM_DRIVER_RETURN_TO_NORMAL, 1.0f, 0.0f ), CAM_DRIVER_RETURN_TO_NORMAL_GAIN ), 0, 1 );

			//eyeOrigin += (vm->m_vecCamDriverLastPos * vm->m_flCamDriverWeight);
			eyeAngles += (vm->m_angCamDriverLastAng * vm->m_flCamDriverWeight);
		}
		else
		{
			vm->m_flCamDriverWeight = 0;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Determine view roll, including data kick
//-----------------------------------------------------------------------------
void CBasePlayer::CalcViewRoll( QAngle& eyeAngles )
{
	if ( GetMoveType() == MOVETYPE_NOCLIP )
		return;

	float side = CalcRoll( GetAbsAngles(), GetAbsVelocity(), sv_rollangle.GetFloat(), sv_rollspeed.GetFloat() );
	eyeAngles[ROLL] += side;
}

extern ConVar cl_use_new_headbob;
//ConVar cl_headbob_freq( "cl_headbob_freq", "12", FCVAR_CLIENTDLL );
//ConVar cl_headbob_amp("cl_headbob_amp", "1.5", FCVAR_CLIENTDLL );
ConVar cl_headbob_land_dip_amt(side_registry(), "cl_headbob_land_dip_amt", "4", FCVAR_CLIENTDLL );

void CBasePlayer::CalcViewBob( Vector& eyeOrigin )
{
		if ( cl_use_new_headbob.GetBool() == false )
			return;
		
		Vector vecBaseEyePosition = eyeOrigin;
		
		// if we just landed, dip the player's view
		float flOldFallVel = m_Local.m_flOldFallVelocity;
		float flFallVel = m_Local.m_flFallVelocity;
		//Msg("Fall Velocity: %f\n", flFallVel );

		if ( flFallVel <= 0.1f && flOldFallVel > 10.0f && flOldFallVel <= PLAYER_FATAL_FALL_SPEED && m_Local.m_bInLanding == false )
		{
			m_Local.m_bInLanding = true;
			m_Local.m_flLandingTime = gpGlobals->curtime;
		}

		// don't bob the view right now
		/*
		const float flMaxSpeed = sv_maxspeed.GetFloat();
		float flSpeedFactor;
		*/

		if ( m_Local.m_bInLanding == true )
		{
			float landseconds = std::max(gpGlobals->curtime - m_Local.m_flLandingTime, 0.0f);
			float landFraction = SimpleSpline( landseconds / 0.25f );
			clamp( landFraction, 0.0f, 1.0f );

			float flDipAmount = (1 / flOldFallVel) * 0.1f;

			int dipHighOffset = 64;
			int dipLowOffset = dipHighOffset - cl_headbob_land_dip_amt.GetInt();
			Vector temp = GetViewOffset();
			temp.z = ( ( dipLowOffset - flDipAmount ) * landFraction ) +
				( dipHighOffset * ( 1 - landFraction ) );

			if ( temp.z > dipHighOffset )
			{
				temp.z = dipHighOffset;
				m_Local.m_bInLanding = false;
			}
			eyeOrigin.z -= ( dipHighOffset - temp.z );
			//SetViewOffset( temp );
		}
		else
		{
			// don't bob the view right now
			/*
			flSpeedFactor = GetAbsVelocity().Length() / flMaxSpeed; 
			clamp( flSpeedFactor, 0.0f, 1.0f );
			eyeOrigin.z += flSpeedFactor * (sin(gpGlobals->curtime * cl_headbob_freq.GetFloat() ) * cl_headbob_amp.GetFloat());
			*/
		}

		// stop when our eyes get back to default
		if ( m_Local.m_bInLanding == true && ( (eyeOrigin.z - 0.001f) >= vecBaseEyePosition.z )  )
		{
			m_Local.m_bInLanding = false;
		}

		if ( m_Local.m_bInLanding == false  )
		{	
			// Set the old velocity to the new velocity, we check next frame to see if we hit the ground
			m_Local.m_flOldFallVelocity = m_Local.m_flFallVelocity;
		}
}

void CBasePlayer::DoMuzzleFlash()
{
	for ( int i = 0; i < MAX_VIEWMODELS; i++ )
	{
		CBaseViewModel *vm = GetViewModel( i );
		if ( !vm )
			continue;

		vm->DoMuzzleFlash();
	}

	BaseClass::DoMuzzleFlash();
}

float CBasePlayer::GetFOVDistanceAdjustFactor()
{
	float defaultFOV	= (float)GetDefaultFOV();
	float localFOV		= (float)GetFOV();

	if ( localFOV == defaultFOV || defaultFOV < 0.001f )
	{
		return 1.0f;
	}

	// If FOV is lower, then we're "zoomed" in and this will give a factor < 1 so apparent LOD distances can be
	//  shorted accordingly
	return localFOV / defaultFOV;

}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecTracerSrc - 
//			&tr - 
//			iTracerType - 
//-----------------------------------------------------------------------------
void CBasePlayer::MakeTracer( const Vector &vecTracerSrc, const trace_t &tr, int iTracerType )
{
	if ( GetActiveWeapon() )
	{
		GetActiveWeapon()->MakeTracer( vecTracerSrc, tr, iTracerType );
		return;
	}

	BaseClass::MakeTracer( vecTracerSrc, tr, iTracerType );
}

void CBasePlayer::SharedSpawn()
{
	SetMoveType( MOVETYPE_WALK );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetFriction( 1.0f );

	pl.deadflag	= false;
	m_lifeState	= LIFE_ALIVE;
	m_iHealth = 100;
	m_takedamage		= DAMAGE_YES;

	m_Local.m_bDrawViewmodel = true;
	m_Local.m_flStepSize = sv_stepsize.GetFloat();
	m_Local.m_bAllowAutoMovement = true;

	SetRenderFX( kRenderFxNone );
	m_flNextAttack	= gpGlobals->curtime;
	m_flMaxspeed		= 0.0f;

	MDLCACHE_CRITICAL_SECTION();
	int iIdleSequence = SelectWeightedSequence( ACT_IDLE );
	if( iIdleSequence < 0 )
	{
		iIdleSequence = 0;
	}
	SetSequence( iIdleSequence );

	if ( GetFlags() & FL_DUCKING ) 
		SetCollisionBounds( VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
	else
		SetCollisionBounds( VEC_HULL_MIN, VEC_HULL_MAX );

	// dont let uninitialized value here hurt the player
	m_Local.m_flFallVelocity = 0;

	SetBloodColor( BLOOD_COLOR_RED );

	m_hUseEntity = nullptr;

	m_flDuckAmount = 0;
	m_flDuckSpeed = CS_PLAYER_DUCK_SPEED_IDEAL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CBasePlayer::GetDefaultFOV( void ) const
{
	if ( GetObserverMode() == OBS_MODE_IN_EYE )
	{
		CBasePlayer *pTargetPlayer = ToBasePlayer( GetObserverTarget() );

		if ( pTargetPlayer && !pTargetPlayer->IsObserver() )
		{
			return pTargetPlayer->GetDefaultFOV();
		}
	}

	int iFOV = ( m_iDefaultFOV == 0 ) ? g_pGameRules->DefaultFOV() : m_iDefaultFOV;

	return iFOV;
}

void CBasePlayer::AvoidPhysicsProps( CUserCmd *pCmd )
{
	// Don't avoid if noclipping or in movetype none
	switch ( GetMoveType() )
	{
	case MOVETYPE_NOCLIP:
	case MOVETYPE_NONE:
	case MOVETYPE_OBSERVER:
		return;
	default:
		break;
	}

	if ( GetObserverMode() != OBS_MODE_NONE || !IsAlive() )
		return;

	AvoidPushawayProps( this, pCmd );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CBasePlayer::GetTracerType( void )
{
	if ( GetActiveWeapon() )
	{
		return GetActiveWeapon()->GetTracerType();
	}

	return BaseClass::GetTracerType();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayer::ClearZoomOwner( void )
{
	m_hZoomOwner = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the FOV of the client, doing interpolation between old and new if requested
// Input  : FOV - New FOV
//			zoomRate - Amount of time (in seconds) to move between old and new FOV
//-----------------------------------------------------------------------------
bool CBasePlayer::SetFOV( CBaseEntity *pRequester, int FOV, float zoomRate, int iZoomStart /* = 0 */ )
{
	//NOTENOTE: You MUST specify who is requesting the zoom change
	contract_assert( pRequester != nullptr );
	if ( pRequester == nullptr )
		return false;

	// If we already have an owner, we only allow requests from that owner
	if ( ( m_hZoomOwner.Get() != nullptr ) && ( m_hZoomOwner.Get() != pRequester ) )
	{
			return false;
	}
	else
	{
		//FIXME: Maybe do this is as an accessor instead
		if ( FOV == 0 )
		{
			m_hZoomOwner = nullptr;
		}
		else
		{
			m_hZoomOwner = pRequester;
		}
	}

	// Setup our FOV and our scaling time

	if ( iZoomStart > 0 )
	{
		m_iFOVStart = iZoomStart;
	}
	else
	{
		m_iFOVStart = GetFOV();
	}

	m_flFOVTime = gpGlobals->curtime;
	m_iFOV = FOV;

	m_Local.m_flFOVRate	= zoomRate;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayer::UpdateUnderwaterState( void )
{
	if ( GetWaterLevel() == WL_Eyes )
	{
		if ( IsPlayerUnderwater() == false )
		{
			SetPlayerUnderwater( true );
		}
		return;
	}

	if ( IsPlayerUnderwater() )
	{
		SetPlayerUnderwater( false );
	}

	if ( GetWaterLevel() == WL_NotInWater )
	{
		if ( GetFlags() & FL_INWATER )
		{
			RemoveFlag( FL_INWATER );
		}
	}
	else if ( !(GetFlags() & FL_INWATER) )
	{

		AddFlag( FL_INWATER );
	}
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
// ensure that for every emitsound there is a matching stopsound
//-----------------------------------------------------------------------------
void CBasePlayer::SetPlayerUnderwater( bool state )
{
	if ( m_bPlayerUnderwater != state )
	{
		m_bPlayerUnderwater = state;

		if ( state )
			EmitSound( "Player.AmbientUnderWater" );
		else
			StopSound( "Player.AmbientUnderWater" );		
	}
}

void CBasePlayer::SetPreviouslyPredictedOrigin( const Vector &vecAbsOrigin )
{
	m_vecPreviouslyPredictedOrigin = vecAbsOrigin;
}

const Vector &CBasePlayer::GetPreviouslyPredictedOrigin() const
{
	return m_vecPreviouslyPredictedOrigin;
}

bool fogparams_t::operator !=( const fogparams_t& other ) const
{
	if ( this->enable != other.enable ||
		this->blend != other.blend ||
		!VectorsAreEqual(this->dirPrimary, other.dirPrimary, 0.01f ) || 
		this->colorPrimary.Get() != other.colorPrimary.Get() ||
		this->colorSecondary.Get() != other.colorSecondary.Get() ||
		this->start != other.start ||
		this->end != other.end ||
		this->farz != other.farz ||
		this->maxdensity != other.maxdensity ||
		this->colorPrimaryLerpTo.Get() != other.colorPrimaryLerpTo.Get() ||
		this->colorSecondaryLerpTo.Get() != other.colorSecondaryLerpTo.Get() ||
		this->startLerpTo != other.startLerpTo ||
		this->endLerpTo != other.endLerpTo ||
		this->maxdensityLerpTo != other.maxdensityLerpTo ||
		this->lerptime != other.lerptime ||
		this->duration != other.duration ||
		this->HDRColorScale != other.HDRColorScale ||
		this->ZoomFogScale != other.ZoomFogScale)
		return true;

	return false;
}

void CBasePlayer::IncrementEFNoInterpParity()
{
	// Only matters in multiplayer
	if ( gpGlobals->maxClients == 1 )
		return;
	m_ubEFNoInterpParity = (m_ubEFNoInterpParity + 1) % NOINTERP_PARITY_MAX;
}

int CBasePlayer::GetEFNoInterpParity() const
{
	return (int)m_ubEFNoInterpParity;
}

void CBasePlayer::AddSplitScreenPlayer( CBasePlayer *pOther )
{
	CHandle< CBasePlayer > h;
	h = pOther;
	if ( ks::find_index( m_hSplitScreenPlayers, h ) == -1 )
	{
		m_hSplitScreenPlayers.push_back( h );
	}

	UpdateSplitScreenAndPictureInPicturePlayerList();
}

void CBasePlayer::RemoveSplitScreenPlayer( CBasePlayer *pOther )
{
	CHandle< CBasePlayer > h;
	h = pOther;
	ks::erase_first( m_hSplitScreenPlayers, h );

	UpdateSplitScreenAndPictureInPicturePlayerList();
}

std::vector< CHandle< CBasePlayer > > &CBasePlayer::GetSplitScreenPlayers()
{
	return m_hSplitScreenPlayers;
}

bool CBasePlayer::HasAttachedSplitScreenPlayers() const
{
	return ( !m_hSplitScreenPlayers.empty() );
}

void CBasePlayer::AddPictureInPicturePlayer( CBasePlayer *pOther )
{
	CHandle< CBasePlayer > h;
	h = pOther;
	if ( ks::find_index( m_hPipPlayers, h ) == -1 )
	{
		m_hPipPlayers.push_back( h );
	}

	UpdateSplitScreenAndPictureInPicturePlayerList();
}

void CBasePlayer::RemovePictureInPicturePlayer( CBasePlayer *pOther )
{
	CHandle< CBasePlayer > h;
	h = pOther;
	ks::erase_first( m_hPipPlayers, h );

	UpdateSplitScreenAndPictureInPicturePlayerList();
}

std::vector< CHandle< CBasePlayer > > &CBasePlayer::GetSplitScreenAndPictureInPicturePlayers()
{
	return m_hSplitScreenAndPipPlayers;
}

std::vector< CHandle< CBasePlayer > >& CBasePlayer::GetPictureInPicturePlayers( void )
{
	return m_hPipPlayers;
}

void CBasePlayer::UpdateSplitScreenAndPictureInPicturePlayerList()
{
	// Make m_hSplitScreenAndPipPlayers the union of m_hSplitScreenPlayers and m_hPipPlayers
	m_hSplitScreenAndPipPlayers.clear();
	m_hSplitScreenAndPipPlayers.append_range( m_hSplitScreenPlayers );

	for ( int i = 0; i < std::ssize( m_hPipPlayers ); i++ )
	{
		CHandle< CBasePlayer > h = m_hPipPlayers[i];
		if ( ks::find_index( m_hSplitScreenAndPipPlayers, h ) == -1 )
		{
			m_hSplitScreenAndPipPlayers.push_back( h );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Strips off IN_xxx flags from the player's input
//-----------------------------------------------------------------------------
void CBasePlayer::ForceButtons( int nButtons )
{
	m_afButtonForced |= nButtons;
}

//-----------------------------------------------------------------------------
// Purpose: Re-enables stripped IN_xxx flags to the player's input
//-----------------------------------------------------------------------------
void CBasePlayer::UnforceButtons( int nButtons )
{
	m_afButtonForced &= ~nButtons;
}

CBaseEntity* CBasePlayer::GetSoundscapeListener()
{
	return this;
}

void CBasePlayer::SetUseEntity( CBaseEntity *pUseEntity ) 
{ 
	m_hUseEntity = pUseEntity; 
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBasePlayer::VPhysicsShadowUpdate( IPhysicsObject *pPhysics )
{
	if ( sv_turbophysics.GetBool() )
		return;

	Vector newPosition;

	bool physicsUpdated = m_pPhysicsController->GetShadowPosition( &newPosition, nullptr ) > 0 ? true : false;

	bool bCheckStuck = false;
	if ( m_afPhysicsFlags & PFLAG_GAMEPHYSICS_ROTPUSH )
	{
		bCheckStuck = true;
		m_afPhysicsFlags &= ~PFLAG_GAMEPHYSICS_ROTPUSH;
	}
	uint32 nContactState = m_pPhysicsController->GetContactState( FVPHYSICS_PUSH_PLAYER );
	if ( (nContactState & PLAYER_CONTACT_PHYSICS) || (m_afPhysicsFlags & PFLAG_VPHYSICS_MOTIONCONTROLLER) )
	{
		m_bTouchedPhysObject = true;
	}

	if ( IsFollowingPhysics() )
	{
		m_bTouchedPhysObject = true;
	}

	if( !cl_predict_motioncontrol.GetBool() )
	{
		m_bTouchedPhysObject = false;
	}

	if ( GetMoveType() == MOVETYPE_NOCLIP || pl.deadflag )
	{
		m_oldOrigin = GetAbsOrigin();
		return;
	}

	if ( !physicsUpdated )
		return;

	IPhysicsObject *pPhysGround = GetGroundVPhysics();

	Vector newVelocity;
	pPhysics->GetPosition( &newPosition, 0 );
	m_pPhysicsController->GetShadowVelocity( &newVelocity );
	
	// assume vphysics gave us back a position without penetration
	Vector lastValidPosition = newPosition;

	if ( physicsshadowupdate_render.GetBool() )
	{
		NDebugOverlay::Box( GetAbsOrigin(), WorldAlignMins(), WorldAlignMaxs(), 255, 255, 0, 24, 15.0f );
		NDebugOverlay::Box( newPosition, WorldAlignMins(), WorldAlignMaxs(), 0,255,255, 24, 15.0f);
		//	NDebugOverlay::Box( newPosition, WorldAlignMins(), WorldAlignMaxs(), 0,255,255, 24, .01f);
	}

	Vector tmp = GetAbsOrigin() - newPosition;
	if ( !m_bTouchedPhysObject && !(GetFlags() & FL_ONGROUND) )
	{
		tmp.z *= 0.5f;	// don't care about z delta as much
	}

	float dist = tmp.LengthSqr();
	float deltaV = (newVelocity - GetAbsVelocity()).LengthSqr();

	float maxDistErrorSqr = VPHYS_MAX_DISTSQR;
	float maxVelErrorSqr = VPHYS_MAX_VELSQR;
	if ( IsRideablePhysics(pPhysGround) )
	{
		maxDistErrorSqr *= 0.25;
		maxVelErrorSqr *= 0.25;
	}

	// player's physics was frozen, try moving to the game's simulated position if possible
	if ( m_pPhysicsController->WasFrozen() )
	{
		m_bPhysicsWasFrozen = true;
		// check my position (physics object could have simulated into my position
		// physics is not very far away, check my position
		trace_t trace;
		UTIL_TraceEntity( this, GetAbsOrigin(), GetAbsOrigin(), PhysicsSolidMaskForEntity(), this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
		if ( !trace.startsolid )
			return;

		// The physics shadow position is probably not in solid, try to move from there to the desired position
		UTIL_TraceEntity( this, newPosition, GetAbsOrigin(), PhysicsSolidMaskForEntity(), this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
		if ( !trace.startsolid )
		{
			// found a valid position between the two?  take it.
			SetAbsOrigin( trace.endpos );
			UpdateVPhysicsPosition(trace.endpos, vec3_origin, 0);
			return;
		}

	}
	if ( dist >= maxDistErrorSqr || deltaV >= maxVelErrorSqr || (pPhysGround && !m_bTouchedPhysObject) )
	{
		if ( m_bTouchedPhysObject || pPhysGround )
		{
			// BUGBUG: Rewrite this code using fixed timestep
			if ( deltaV >= maxVelErrorSqr && !m_bPhysicsWasFrozen )
			{
				if ( !IsRideablePhysics(pPhysGround) )
				{
					Vector dir = GetAbsVelocity();
					float len = VectorNormalize(dir);
					float dot = DotProduct( newVelocity, dir );
					if ( dot > len )
					{
						dot = len;
					}
					else if ( dot < -len )
					{
						dot = -len;
					}

					VectorMA( newVelocity, -dot, dir, newVelocity );

					if ( m_afPhysicsFlags & PFLAG_VPHYSICS_MOTIONCONTROLLER )
					{
						float val = Lerp( 0.1f, len, dot );
						VectorMA( newVelocity, val - len, dir, newVelocity );
					}

					if ( !(m_afPhysicsFlags & PFLAG_VPHYSICS_MOTIONCONTROLLER ) && IsSimulatingOnAlternateTicks() )
					{
						newVelocity *= 0.5f;
					}
	
					ApplyAbsVelocityImpulse( newVelocity );
				}
				else
				{
					Vector forward, right, up;

					AngleVectors (g_pMoveData->m_vecViewAngles, &forward, &right, &up);  // Determine movement angles

					// Copy movement amounts
					float fmove = g_pMoveData->m_flForwardMove;
					float smove = g_pMoveData->m_flSideMove;

					// Keep movement vectors in our plane of movement
					forward -= Vector(0,0,-1) * DotProduct( forward, Vector(0,0,-1) );
					VectorNormalize( forward );

					right -= Vector(0,0,-1) * DotProduct( right, Vector(0,0,-1) );
					VectorNormalize( right );

					// Determine velocity
					Vector wishvel = fmove * forward + smove * right;

					// HACK!:  This code let's the player walk around with a little more control when on a func_physbox.
					//		   They still slide around more than physical objects, but at least you can actually
					//		   control yourself.
					// Trying to walk around
					if( wishvel.Length2DSqr() )
					{
						Vector vVel = -newVelocity;
						vVel += GetAbsVelocity() + wishvel;

						// Clamp or else this can get wildy huge and I don't know why
						for( int i=0; i<3; ++i )
						{
							vVel[i] = clamp( vVel[i], newVelocity[i] - 180, newVelocity[i] + 180 );
						}

						SetAbsVelocity( vVel );
					}

				}
			}

			trace_t trace;
			UTIL_TraceEntity( this, newPosition, newPosition, PhysicsSolidMaskForEntity(), this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
			if ( !trace.allsolid && !trace.startsolid )
			{
				SetAbsOrigin( newPosition );
			}
		}
		else
		{
			{
				bCheckStuck = true;
			}
		}
	}
	else
	{
		if ( m_bTouchedPhysObject || (nContactState & PLAYER_CONTACT_GAMEOBJECT) )
		{
			// check my position (physics object could have simulated into my position
			// physics is not very far away, check my position
			trace_t trace;
			UTIL_TraceEntity( this, GetAbsOrigin(), GetAbsOrigin(),
				PhysicsSolidMaskForEntity(), this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );

			// is current position ok?
			if ( trace.allsolid || trace.startsolid )
			{
				// no use the final stuck check to move back to old if this stuck fix didn't work
				bCheckStuck = true;
				lastValidPosition = m_oldOrigin;
				SetAbsOrigin( newPosition );
			}
		}
	}

	if ( bCheckStuck )
	{
		trace_t trace;
		UTIL_TraceEntity( this, GetAbsOrigin(), GetAbsOrigin(), PhysicsSolidMaskForEntity(), this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );

		// current position is not ok, fixup
		if ( trace.allsolid || trace.startsolid )
		{
			// STUCK!?!?!
			//Warning( "Checkstuck failed.  Stuck on %s!!\n", trace.Ent<CBaseEntity>()->GetClassname() );
			SetAbsOrigin( lastValidPosition );
		}
	}
	m_oldOrigin = GetAbsOrigin();
	m_bPhysicsWasFrozen = false;

}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBasePlayer::PostThinkVPhysics( void )
{
	// Check to see if things are initialized!
	if ( !m_pPhysicsController )
	{
		if( !physenv->IsPredicted() )
		{
			if ( GetMoveType() == MOVETYPE_NOCLIP || GetMoveType() == MOVETYPE_OBSERVER )
			{
				m_vphysicsCollisionState = VPHYS_NOCLIP;
			}
			else if ( GetFlags() & FL_DUCKING )
			{
				m_vphysicsCollisionState = VPHYS_CROUCH;
			}
			else
			{
				m_vphysicsCollisionState = VPHYS_WALK;
			}
		}
		return;
	}

	Vector newPosition = GetAbsOrigin();
	float frametime = gpGlobals->frametime;
	if ( frametime <= 0 || frametime > 0.1f )
		frametime = 0.1f;

	IPhysicsObject *pPhysGround = GetGroundVPhysics();

	if ( !pPhysGround && m_bTouchedPhysObject && g_pMoveData->m_outStepHeight <= 0.f && (GetFlags() & FL_ONGROUND) )
	{
		newPosition = m_oldOrigin + frametime * g_pMoveData->m_outWishVel;
		newPosition = (GetAbsOrigin() * 0.5f) + (newPosition * 0.5f);
	}

	int collisionState = VPHYS_WALK;
	if ( GetMoveType() == MOVETYPE_NOCLIP || GetMoveType() == MOVETYPE_OBSERVER )
	{
		collisionState = VPHYS_NOCLIP;
	}
	else if ( GetFlags() & FL_DUCKING )
	{
		collisionState = VPHYS_CROUCH;
	}

	if ( collisionState != m_vphysicsCollisionState )
	{
		SetVCollisionState( GetAbsOrigin(), GetAbsVelocity(), collisionState );
	}

	if ( !(TouchedPhysics() || pPhysGround) )
	{
		float maxSpeed = m_flMaxspeed > 0.0f ? m_flMaxspeed : sv_maxspeed.GetFloat();
		g_pMoveData->m_outWishVel.Init( maxSpeed, maxSpeed, maxSpeed );
	}

	// teleport the physics object up by stepheight (game code does this - reflect in the physics)
	if ( g_pMoveData->m_outStepHeight > 0.1f )
	{
		if ( g_pMoveData->m_outStepHeight > 4.0f )
		{
			VPhysicsGetObject()->SetPosition( GetAbsOrigin(), vec3_angle, true );
		}
		else
		{
			// don't ever teleport into solid
			Vector position, end;
			VPhysicsGetObject()->GetPosition( &position, nullptr );
			end = position;
			end.z += g_pMoveData->m_outStepHeight;
			trace_t trace;
			UTIL_TraceEntity( this, position, end, PhysicsSolidMaskForEntity(), this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
			if ( trace.DidHit() )
			{
				g_pMoveData->m_outStepHeight = trace.endpos.z - position.z;
			}
			m_pPhysicsController->StepUp( g_pMoveData->m_outStepHeight );
		}
		m_pPhysicsController->Jump();
	}
	g_pMoveData->m_outStepHeight = 0.0f;

	// Store these off because after running the usercmds, it'll pass them
	// to UpdateVPhysicsPosition.	
	m_vNewVPhysicsPosition = newPosition;
	m_vNewVPhysicsVelocity = g_pMoveData->m_outWishVel;

	m_oldOrigin = GetAbsOrigin();
}

IPhysicsObject *CBasePlayer::GetGroundVPhysics()
{
	CBaseEntity *pGroundEntity = GetGroundEntity();
	if ( pGroundEntity && pGroundEntity->GetMoveType() == MOVETYPE_VPHYSICS )
	{
		IPhysicsObject *pPhysGround = pGroundEntity->VPhysicsGetObject();
		if ( pPhysGround && pPhysGround->IsMoveable() )
			return pPhysGround;
	}
	return nullptr;
}

// UNDONE: Look and see if the ground entity is in hierarchy with a MOVETYPE_VPHYSICS?
// Behavior in that case is not as good currently when the parent is rideable
bool CBasePlayer::IsRideablePhysics( IPhysicsObject *pPhysics )
{
	if ( pPhysics )
	{
		if ( pPhysics->GetMass() > (VPhysicsGetObject()->GetMass()*2) )
			return true;
	}

	return false;
}

void CBasePlayer::UpdateVPhysicsPosition( const Vector &position, const Vector &velocity, float secondsToArrival )
{
	if( !m_pPhysicsController )
		return;

	bool onground = (GetFlags() & FL_ONGROUND) ? true : false;
	IPhysicsObject *pPhysGround = GetGroundVPhysics();

	// if the object is much heavier than the player, treat it as a local coordinate system
	// the player controller will solve movement differently in this case.
	if ( !IsRideablePhysics(pPhysGround) )
	{
		pPhysGround = nullptr;
	}

	m_pPhysicsController->Update( position, velocity, secondsToArrival, onground, pPhysGround );
}

// used by the physics gun and game physics... is there a better interface?
void CBasePlayer::SetPhysicsFlag( int nFlag, bool bSet )
{
	if (bSet)
		m_afPhysicsFlags |= nFlag;
	else
		m_afPhysicsFlags &= ~nFlag;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBasePlayer::SetVCollisionState( const Vector &vecAbsOrigin, const Vector &vecAbsVelocity, int collisionState )
{
	m_vphysicsCollisionState = collisionState;
	if( m_pShadowStand == nullptr )
		return;

	switch( collisionState )
	{
	case VPHYS_WALK:
		m_pShadowStand->SetPosition( vecAbsOrigin, vec3_angle, true );
		m_pShadowStand->SetVelocity( &vecAbsVelocity, nullptr );
		m_pShadowCrouch->EnableCollisions( false );
		m_pPhysicsController->SetObject( m_pShadowStand );
		VPhysicsSwapObject( m_pShadowStand );
		m_pShadowStand->EnableCollisions( true );
		break;

	case VPHYS_CROUCH:
		m_pShadowCrouch->SetPosition( vecAbsOrigin, vec3_angle, true );
		m_pShadowCrouch->SetVelocity( &vecAbsVelocity, nullptr );
		m_pShadowStand->EnableCollisions( false );
		m_pPhysicsController->SetObject( m_pShadowCrouch );
		VPhysicsSwapObject( m_pShadowCrouch );
		m_pShadowCrouch->EnableCollisions( true );
		break;

	case VPHYS_NOCLIP:
		m_pShadowCrouch->EnableCollisions( false );
		m_pShadowStand->EnableCollisions( false );
		break;
	}
}

void CBasePlayer::UpdatePhysicsShadowToCurrentPosition()
{
	UpdateVPhysicsPosition( GetAbsOrigin(), vec3_origin, gpGlobals->frametime );
}

void CBasePlayer::UpdatePhysicsShadowToPosition( const Vector &vecAbsOrigin )
{
	UpdateVPhysicsPosition( vecAbsOrigin, vec3_origin, gpGlobals->frametime );
}

const char *CBasePlayer::GetPlayerModelName( void )
{
	return "models/player.mdl";
}

float CBasePlayer::GetAirTime( void )
{
	return m_flTimeLastTouchedGround == 0.0f ? 0.0f : gpGlobals->curtime - m_flTimeLastTouchedGround;
}

const Vector& CBasePlayer::GetEyeOffset() const 
{ 
	return m_vecEyeOffset; 
}

void CBasePlayer::SetEyeOffset( const Vector& v ) 
{ 
	m_vecEyeOffset = v; 
}

const QAngle & CBasePlayer::GetEyeAngleOffset() const 
{ 
	return m_EyeAngleOffset; 
}

void CBasePlayer::SetEyeAngleOffset( const QAngle& qa ) 
{ 
	m_EyeAngleOffset = qa; 
}

const Vector & CBasePlayer::GetAimDirection() const
{
	return m_AimDirection;
}

void CBasePlayer::SetAimDirection( const Vector& v )
{
	m_AimDirection = v; 
}

bool CBasePlayer::IsCoach( void ) const
{
	return ( GetCoachingTeam() != 0 );
}

int CBasePlayer::GetCoachingTeam( void ) const
{
	if ( sv_coaching_enabled.GetBool() && ( GetTeamNumber() == TEAM_SPECTATOR ) )
	{
		return m_iCoachingTeam;
	}
	else
	{ 
		return 0;
	}
}

// returns the player's team or, if coach, coaching team.
int CBasePlayer::GetAssociatedTeamNumber( void ) const
{
	return ( IsCoach() ? GetCoachingTeam() : GetTeamNumber() ); 
}

// is a true spectator, i.e. NOT a team coach
bool CBasePlayer::IsSpectator( void ) const
{
	return ( GetTeamNumber() == TEAM_SPECTATOR && !IsCoach() );
}

autoaim_params_t::autoaim_params_t()
{
	m_fScale = 0;
	m_fMaxDist = 0;
	m_fMaxDeflection = -1.0f;
	m_bOnTargetQueryOnly = false;
}
