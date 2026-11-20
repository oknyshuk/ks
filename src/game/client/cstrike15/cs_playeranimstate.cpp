// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <string.h>
#include <stddef.h>
#include <math.h>
#include <stdlib.h>
#include "datacache/imdlcache.h"
#include "declare_class.h"
#include "edict.h"
#include "studio.h"
#include "../ai_activity.h"
#include "cdll_int.h"
#include "client_class.h"
#include "eiface.h"
#include "localize/ilocalize.h"
#include "../reflect_recvtable.h"
#include "iprediction.h"
#include "engine/ivmodelinfo.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "tier2/resourceprecacher.h"
#include "view_shared.h"
#include "../cdll_client_int.h"
#include "datamap.h"
#include "csgo_playeranimstate.h"
#include "../shareddefs.h"
module ks.client;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.types;
import ks.core.vprof;
import ks.game.iplayeranimstate;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.map_ops;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.strtools;
import ks.game.ai_activity;
import :baseentity;
import :iinput;
import :props;
import :baseanimatedtextureproxy;
import :sharedvar;
import :util;
import :cstrike15.cs_player;
import :cstrike15.cs_shareddefs;
import :particlemgr;
import :precache;
import ks.game.shareddefs;
import :achievements_and_stats_interface;
import :cstrike15.cs_player_shared;
import ks.game.gamemovement;
import :noopanimcontroller;
import ks.tier1.interfaces;
import ks.vstdlib.vstrtools;
import :cstrike15.cs_team;
import :econ.econ_item_schema_minimal;
import ks.tier1.keyvalues;
import ks.core.color;
import ks.core.wchartypes;
import :cstrike15.cs_view_scene;
import ks.game.mapentities_shared;
import ks.core.random;
import ks.tier1.checksum_crc;
import ks.math.ssemath;
import ks.tier1.text_writer;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;

class CAnimationLayer;
class CBaseAnimatingOverlay;
class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CCSPlayer;
class CWeaponCSBase;

using namespace ::ks::reflect;

static constexpr auto ANIM_TOPSPEED_WALK = 100;
static constexpr auto ANIM_TOPSPEED_RUN = 250;
static constexpr auto ANIM_TOPSPEED_RUN_CROUCH = 85;

inline constexpr const char *DEFAULT_IDLE_NAME = "idle_upper_";
inline constexpr const char *DEFAULT_CROUCH_IDLE_NAME = "crouch_idle_upper_";
inline constexpr const char *DEFAULT_CROUCH_WALK_NAME = "crouch_walk_upper_";
inline constexpr const char *DEFAULT_WALK_NAME = "walk_upper_";
inline constexpr const char *DEFAULT_RUN_NAME = "run_upper_";

inline constexpr const char *DEFAULT_FIRE_IDLE_NAME = "idle_shoot_";
inline constexpr const char *DEFAULT_FIRE_CROUCH_NAME = "crouch_idle_shoot_";
inline constexpr const char *DEFAULT_FIRE_CROUCH_WALK_NAME = "crouch_walk_shoot_";
inline constexpr const char *DEFAULT_FIRE_WALK_NAME = "walk_shoot_";
inline constexpr const char *DEFAULT_FIRE_RUN_NAME = "run_shoot_";

inline constexpr const char *DEFAULT_SILENCER_ATTACH_NAME = "silencer_attach_%s";
inline constexpr const char *DEFAULT_SILENCER_DETACH_NAME = "silencer_detach_%s";

static constexpr auto ANIM_ACT_DURATION_BEFORE_RETURN_TO_IDLE = 0.5f;
static constexpr auto ANIM_ACT_DURATION_BEFORE_RETURN_TO_WALK = 0.7f;

enum CSPlayerAnimStateLayer_t
{
	FIRESEQUENCE_LAYER = ( AIMSEQUENCE_LAYER + NUM_AIMSEQUENCE_LAYERS ),
	FIRESEQUENCE2_LAYER,
	DEPLOYSEQUENCE_LAYER,
	RELOADSEQUENCE_LAYER,
	SILENCERSEQUENCE_LAYER,
	GRENADESEQUENCE_LAYER,
	FLASHEDSEQUENCE_LAYER,
	FLINCHSEQUENCE_LAYER,
	TAUNTSEQUENCE_LAYER,
	FOOTPLANTSEQUENCE_LAYER,

	NUM_LAYERS_WANTED
};

ConVar post_jump_crouch( side_registry(), "post_jump_crouch", "0.2f", FCVAR_REPLICATED | FCVAR_CHEAT, "This determines how long the third person player character will crouch for after landing a jump.  This only affects the third person animation visuals." );

ConVar hostage_feetyawrate( side_registry(), "hostage_feetyawrate", "720", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "How many degrees per second that hostages can turn their feet or upper body." );

// ------------------------------------------------------------------------------------------------ //
// CCSPlayerAnimState declaration.
// ------------------------------------------------------------------------------------------------ //

class CCSPlayerAnimState : public CBasePlayerAnimState
{
public:
	DECLARE_CLASS( CCSPlayerAnimState, CBasePlayerAnimState );
	friend IPlayerAnimState* CreatePlayerAnimState( CBaseAnimatingOverlay *pEntity, ICSPlayerAnimStateHelpers *pHelpers, LegAnimType_t legAnimType, bool bUseAimSequences );

	CCSPlayerAnimState();

	virtual void DoAnimationEvent( PlayerAnimEvent_t animEvent, int nData );
	virtual bool IsThrowingGrenade();
	virtual bool ShouldHideGrenadeDuringThrow();
	virtual int CalcAimLayerSequence( float *flCycle, float *flAimSequenceWeight, bool bForceIdle );
	virtual void ClearAnimationState();
	virtual bool CanThePlayerMove();
	virtual float GetCurrentMaxGroundSpeed();
	virtual Activity CalcMainActivity();
	virtual void DebugShowAnimState( int iStartLine );
	virtual void ComputeSequences( CStudioHdr *pStudioHdr );
	virtual void ClearAnimationLayers();
	virtual int SelectWeightedSequence( Activity activity );
	// Calculate the playback rate for movement layer
	virtual float CalcMovementPlaybackRate( bool *bIsMoving );
	
	virtual void Update( float eyeYaw, float eyePitch );

	virtual void ModifyTauntDuration( float flTimingChange );

	void InitCS( CBaseAnimatingOverlay *pPlayer, ICSPlayerAnimStateHelpers *pHelpers, LegAnimType_t legAnimType, bool bUseAimSequences );
	
protected:

	int CalcFireLayerSequence(PlayerAnimEvent_t animEvent);
	void ComputeFireSequence( CStudioHdr *pStudioHdr );

	void ComputeDeploySequence( CStudioHdr *pStudioHdr );
	int CalcDeployLayerSequence( void );

	void ComputeReloadSequence( CStudioHdr *pStudioHdr );
	int CalcReloadLayerSequence( PlayerAnimEvent_t animEvent );

	void ComputeSilencerChangeSequence( CStudioHdr *pStudioHdr );
	int CalcSilencerChangeLayerSequence( PlayerAnimEvent_t animEvent );

	int CalcFlashedLayerSequence( CBaseCombatCharacter *pBaseCombatCharacter );
	void ComputeFlashedSequence( CStudioHdr *pStudioHdr );

	int CalcFlinchLayerSequence( CBaseCombatCharacter *pBaseCombatCharacter );
	void ComputeFlinchSequence( CStudioHdr *pStudioHdr );

	int CalcTauntLayerSequence( CBaseCombatCharacter *pBaseCombatCharacter );
	void ComputeTauntSequence( CStudioHdr *pStudioHdr );

	void ComputeFootPlantSequence(CStudioHdr *pStudioHdr);

	bool IsOuterGrenadePrimed();
	void ComputeGrenadeSequence( CStudioHdr *pStudioHdr );
	int CalcGrenadePrimeSequence();
	int CalcGrenadeThrowSequence( bool bUnderhand = false );
	int GetOuterGrenadeThrowCounter();

	const char* GetWeaponSuffix();
	bool HandleJumping();

	void UpdateLayerSequenceGeneric( CStudioHdr *pStudioHdr, int iLayer, bool &bEnabled, float &flCurCycle, int &iSequence, bool bWaitAtEnd, float flWeight = 1.0f );

	virtual int CalcSequenceIndex( const char *pBaseName, ... );

	bool ActiveWeaponIsDeployed();

	float GetTimeSinceLastActChange();
	void UpdateTimeSinceLastActChange();

private:

	// Current state variables.
	bool m_bJumping;			// Set on a jump event.
	float m_flJumpStartTime;
	bool m_bFirstJumpFrame;
	
	float m_flPostLandCrouchEndTime;
	
	// Aim sequence plays reload while this is on.
	bool m_bReloading;
	float m_flReloadCycle;
	int m_iReloadSequence;
	float m_flReloadHoldEndTime;	// Intermediate shotgun reloads get held a fraction of a second

	// Sequence for silencer changing
	bool m_bSilencerChanging;
	float m_flSilencerChangeCycle;
	int m_iSilencerChangeSequence;

	// Sequence for flashbang reaction
	float m_flFlashedAmountDelayed;
	float m_flFlashedAmount;
	float m_flLastFlashDuration;
	int m_iFlashedSequence;

	// Sequence for flinches
	float m_flFlinchStartTime;
	float m_flFlinchLength;
	int m_nFlinchSequence;

	// Sequence for taunts
	float m_flTauntStartTime;
	float m_flTauntLength;
	int m_nTauntSequence;

	// Deploy sequence
	bool m_bDeploying;
	int m_iDeploySequence;
	float m_flDeployCycle;

	int m_iCurrentAimSequence;
	float m_flTargetMaxSpeed;
	float m_flCurrentMaxSpeed;

	int m_iIdleFireSequence;

	// This is set to true if ANY animation is being played in the fire layer.
	bool m_bFiring;						// If this is on, then it'll continue the fire animation in the fire layer
										// until it completes.
	int m_iFireSequence;				// (For any sequences in the fire layer, including grenade throw).
	float m_flFireCycle;
	PlayerAnimEvent_t m_delayedFire;	// if we fire while reloading, delay the fire by one frame so we can cancel the reload first
	PlayerAnimEvent_t m_activeFireEvent;// The current or last set fire event.

	// These control grenade animations.
	bool m_bThrowingGrenade;
	bool m_bPrimingGrenade;
	float m_flGrenadeCycle;
	int m_iGrenadeSequence;
	int m_iLastThrowGrenadeCounter;	// used to detect when the guy threw the grenade.

	bool m_bTryingToRunAfterJump;		// State information that allows us to skip the transition animations to running right after a jump.
	Activity m_CurrentActivity;

	CCSPlayer *m_pPlayer;

	// m_iDeployedWeaponID is the active weapon used for animations.  It does not necessarily line up with what the player has active
	// since the animation system uses events that have higher latency than networked variables.
	// Having our own weapon id will make sure we play animations with the correct weapon.
	CSWeaponID m_iDeployedWeaponID;
	float m_flWeaponSwitchTime;

	ICSPlayerAnimStateHelpers *m_pHelpers;

	void CheckCachedSequenceValidity( void );
	void ClearAnimationLayer( int layer );

	int m_sequenceCache[ ACT_CROUCHIDLE+1 ];	// Cache the first N sequences, since we don't have weights.
	int m_cachedModelIndex;						// Model index for which the sequence cache is valid.

	float m_flLastActChangeTime;
	Activity m_LastActivity;
};

IPlayerAnimState* CreatePlayerAnimState( CBaseAnimatingOverlay *pEntity, ICSPlayerAnimStateHelpers *pHelpers, LegAnimType_t legAnimType, bool bUseAimSequences )
{
	CCSPlayerAnimState *pRet = new CCSPlayerAnimState;
	pRet->InitCS( pEntity, pHelpers, legAnimType, bUseAimSequences );
	return pRet;
}

//----------------------------------------------------------------------------------------------
/**
 * Hostage animation mechanism
 */
class CCSHostageAnimState : public CCSPlayerAnimState
{
public:
	DECLARE_CLASS( CCSHostageAnimState, CCSPlayerAnimState );

	CCSHostageAnimState();

	virtual Activity CalcMainActivity();
	virtual float GetFeetYawRate( void )
	{
		return hostage_feetyawrate.GetFloat();
	}

private:
	Activity mCurrentActivity;
	// No need to cache sequences, and we *do* have multiple sequences per activity
	//virtual int SelectWeightedSequence( Activity activity ) { return GetOuter()->SelectWeightedSequence( activity ); }
};

//----------------------------------------------------------------------------------------------
IPlayerAnimState* CreateHostageAnimState( CBaseAnimatingOverlay *pEntity, ICSPlayerAnimStateHelpers *pHelpers, LegAnimType_t legAnimType, bool bUseAimSequences )
{
	CCSHostageAnimState *anim = new CCSHostageAnimState;
	anim->InitCS( pEntity, pHelpers, legAnimType, bUseAimSequences );
	return anim;
}

//----------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------
CCSHostageAnimState::CCSHostageAnimState()
{
	mCurrentActivity = ACT_IDLE;
}

//----------------------------------------------------------------------------------------------
/**
 * Set hostage animation state
 */
Activity CCSHostageAnimState::CalcMainActivity()
{
	float flOuterSpeed = GetOuterXYSpeed();

	if ( HandleJumping() )
	{
		return ACT_JUMP;
	}
	else
	{
		contract_assert( dynamic_cast<CCHostage*>( m_pOuter ) );
		CCHostage *me = (CCHostage*)m_pOuter;
		
		// If we have no leader, hang out.
		Activity targetIdle = me->GetLeader() ? ACT_IDLE : ACT_BUSY_QUEUE;
		
		// [msmith] We don't want to jitter the activities based on subtle speed changes.
		// The code below only chagnes the activity if the speed has changed by at least SPEED_CHANGE_BUFFER amount.
		const float SPEED_CHANGE_BUFFER = (ARBITRARY_RUN_SPEED - MOVING_MINIMUM_SPEED) / 4.0f;

		if ( m_pOuter->GetFlags() & FL_DUCKING )
		{
			if ( flOuterSpeed > MOVING_MINIMUM_SPEED + SPEED_CHANGE_BUFFER )
			{
				mCurrentActivity = ACT_RUN_CROUCH;
			}
			else if ( flOuterSpeed < MOVING_MINIMUM_SPEED )
			{
				mCurrentActivity = ACT_COVER_LOW;
			}
		}
		else
		{
			if ( flOuterSpeed > ARBITRARY_RUN_SPEED )
			{
				mCurrentActivity = ACT_RUN;
			}
			else if ( flOuterSpeed < ARBITRARY_RUN_SPEED - SPEED_CHANGE_BUFFER && flOuterSpeed > MOVING_MINIMUM_SPEED + SPEED_CHANGE_BUFFER )
			{
				mCurrentActivity = ACT_WALK;
			}
			else if ( flOuterSpeed < MOVING_MINIMUM_SPEED )
			{
				mCurrentActivity = targetIdle;
			}
		}

		return mCurrentActivity;
	}
}

// ------------------------------------------------------------------------------------------------ //
// CCSPlayerAnimState implementation.
// ------------------------------------------------------------------------------------------------ //

CCSPlayerAnimState::CCSPlayerAnimState()
{
	m_pOuter = nullptr;

	m_bJumping = false;
	m_flJumpStartTime = 0.0f;
	m_bFirstJumpFrame = false;

	m_flPostLandCrouchEndTime = 0.0f;

	m_bReloading = false;
	m_flReloadCycle = 0.0f;
	m_iReloadSequence = -1;
	m_flReloadHoldEndTime = 0.0f;

	m_bSilencerChanging = false;
	m_flSilencerChangeCycle = 0.0f;
	m_iSilencerChangeSequence = -1;

	m_flFlashedAmountDelayed = 0;
	m_flFlashedAmount = 0;
	m_flLastFlashDuration = 0;
	m_iFlashedSequence = 0;

	m_flFlinchStartTime = -1.0f;
	m_flFlinchLength = 0.0f;
	m_nFlinchSequence = 0;

	m_flTauntStartTime = -1.0f;
	m_flTauntLength = 0.0f;
	m_nTauntSequence = 0;

	m_bDeploying = false;
	m_iDeploySequence = -1;
	m_flDeployCycle = 0.0f;

	m_iCurrentAimSequence = 0;
	m_flTargetMaxSpeed = 0.0f;
	m_flCurrentMaxSpeed = 0.0f;

	m_bFiring = false;
	m_iFireSequence = -1;
	m_flFireCycle = 0.0f;
	m_delayedFire = PLAYERANIMEVENT_COUNT;
	m_activeFireEvent = PLAYERANIMEVENT_COUNT;
	m_iIdleFireSequence = -1;

	m_bThrowingGrenade = false;
	m_bPrimingGrenade = false;
	m_flGrenadeCycle = 0.0f;
	m_iGrenadeSequence = -1;
	m_iLastThrowGrenadeCounter = 0;
	m_cachedModelIndex = -1;

	m_bTryingToRunAfterJump = false;
	m_CurrentActivity = ACT_IDLE;

	m_pPlayer = nullptr;
	m_iDeployedWeaponID = WEAPON_NONE;
	m_flWeaponSwitchTime = 0.0f;

	m_pHelpers = nullptr;

	m_flLastActChangeTime = gpGlobals->curtime;
	m_LastActivity = ACT_IDLE;
}

void CCSPlayerAnimState::UpdateTimeSinceLastActChange()
{
	if ( m_CurrentActivity != m_LastActivity )
	{
		m_flLastActChangeTime = gpGlobals->curtime;
		m_LastActivity = m_CurrentActivity;
	}
}

float CCSPlayerAnimState::GetTimeSinceLastActChange()
{
	return ( gpGlobals->curtime - m_flLastActChangeTime );
}

void CCSPlayerAnimState::InitCS( CBaseAnimatingOverlay *pEntity, ICSPlayerAnimStateHelpers *pHelpers, LegAnimType_t legAnimType, bool bUseAimSequences )
{
	CModAnimConfig config;
	config.m_flMaxBodyYawDegrees = 60;
	
	//When correcting the player's yaw, m_flMaxBodyYawDegreesCorrectionAmount is applied instead of the max. 
	//Since this value is intended to be more than m_flMaxBodyYawDegrees, it will set the new yaw PAST the midpoint.
	//This looks a little more lifelike when the player is rapidly turning, and if the player halts, it makes the facefront that follows more obvious.
	config.m_flMaxBodyYawDegreesCorrectionAmount = 90;

	//Disable foot plant tunring if the feet are lagging behind by more than this angle
	config.m_flIdleFootPlantMaxYaw = 110;

	//Turning less than this amount? Don't lift the feet, just shuffle them over.
	config.m_flIdleFootPlantFootLiftDelta = 25;

	config.m_LegAnimType = legAnimType;
	config.m_bUseAimSequences = bUseAimSequences;

	m_pPlayer = ToCSPlayer( pEntity );

	m_pHelpers = pHelpers;

	BaseClass::Init( pEntity, config );
}

void CCSPlayerAnimState::ModifyTauntDuration( float flTimingChange )
{
	float flInterp = ( m_flTauntLength <= 0.0f ) ? ( -1.0f ) : ( ( gpGlobals->curtime - m_flTauntStartTime ) / m_flTauntLength );
	bool bTaunt = ( flInterp >= 0.0f && flInterp <= 1.0f );

	if ( bTaunt )
	{
		// Move to start time to account for how much the timing change
		m_flTauntStartTime -= flTimingChange;
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCSPlayerAnimState::CheckCachedSequenceValidity( void )
{
	if ( m_cachedModelIndex != GetOuter()->GetModelIndex() )
	{
		m_cachedModelIndex = GetOuter()->GetModelIndex();
		for ( int i=0; i<=ACT_CROUCHIDLE; ++i )
		{
			m_sequenceCache[i] = -1;
		}

		// precache the sequences we'll be using for movement
		if ( m_cachedModelIndex > 0 )
		{
			m_sequenceCache[ACT_HOP - 1] = GetOuter()->SelectWeightedSequence( ACT_HOP );
			m_sequenceCache[ACT_LEAP - 1] = GetOuter()->SelectWeightedSequence( ACT_LEAP );
			m_sequenceCache[ACT_JUMP - 1] = GetOuter()->SelectWeightedSequence( ACT_JUMP );
			m_sequenceCache[ACT_IDLE - 1] = GetOuter()->SelectWeightedSequence( ACT_IDLE );
			m_sequenceCache[ACT_RUN_CROUCH - 1] = GetOuter()->SelectWeightedSequence( ACT_RUN_CROUCH );
			m_sequenceCache[ACT_CROUCHIDLE - 1] = GetOuter()->SelectWeightedSequence( ACT_CROUCHIDLE );
			m_sequenceCache[ACT_RUN - 1] = GetOuter()->SelectWeightedSequence( ACT_RUN );
			m_sequenceCache[ACT_WALK - 1] = GetOuter()->SelectWeightedSequence( ACT_WALK );
			m_sequenceCache[ACT_IDLE - 1] = GetOuter()->SelectWeightedSequence( ACT_IDLE );
		}
	}
}

void CCSPlayerAnimState::ClearAnimationLayer( int layer )
{
	// Client obeys Order of CBaseAnimatingOverlay::MAX_OVERLAYS (15), but server trusts only the ANIM_LAYER_ACTIVE flag.
	CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( layer );
	if ( pLayer )
	{
		pLayer->SetWeight( 0.0f );
		pLayer->SetOrder( CBaseAnimatingOverlay::MAX_OVERLAYS );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Cache the sequence numbers for the first ACT_HOP activities, since the CS player doesn't have multiple
 * sequences per activity.
 */
int CCSPlayerAnimState::SelectWeightedSequence( Activity activity )
{
	CVProfScope _( "CCSPlayerAnimState::ComputeMainSequence", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	if ( activity > ACT_CROUCHIDLE || activity < 1 )
	{
		return GetOuter()->SelectWeightedSequence( activity );
	}

	CheckCachedSequenceValidity();

	int sequence = m_sequenceCache[ activity - 1 ];
	if ( sequence < 0 )
	{
		// just in case, look up the sequence if we didn't precache it above
		sequence = m_sequenceCache[ activity - 1 ] = GetOuter()->SelectWeightedSequence( activity );
	}

	return sequence;
}

//--------------------------------------------------------------------------------------------------------------
// sprintf version of LookupSequence() 
//--------------------------------------------------------------------------------------------------------------
int CCSPlayerAnimState::CalcSequenceIndex( const char *pBaseName, ... )
{
	CVProfScope _( "CCSPlayerAnimState::CalcSequenceIndex", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	CheckCachedSequenceValidity();

	char szFullName[512];
	std::va_list marker;
	__builtin_va_start( marker, pBaseName );
	V_vsnprintf( szFullName, sizeof( szFullName ), pBaseName, marker );
	__builtin_va_end( marker );

	// FIXME: this looks up on the player, not their weapon
	int iSequence = GetOuter()->LookupSequence( szFullName );
	
	// Show warnings if we can't find anything here.
	if ( iSequence == -1 )
	{
		static std::set< std::string, ks::iless > warned;
		if ( warned.emplace( szFullName ).second )
		{
			Warning( "CalcSequenceIndex: can't find '%s'.\n", szFullName );
		}

		iSequence = 0;
	}

	return iSequence;
}

void CCSPlayerAnimState::ClearAnimationState()
{
	m_bJumping = false;
	m_bDeploying = false;
	m_bFiring = false;
	m_bReloading = false;
	m_flReloadHoldEndTime = 0.0f;
	m_bSilencerChanging = false;
	m_bThrowingGrenade = m_bPrimingGrenade = false;
	m_iLastThrowGrenadeCounter = GetOuterGrenadeThrowCounter();
	
	m_bTryingToRunAfterJump = false;

	BaseClass::ClearAnimationState();
}

void CCSPlayerAnimState::DoAnimationEvent( PlayerAnimEvent_t animEvent, int nData )
{
	contract_assert( animEvent != PLAYERANIMEVENT_THROW_GRENADE );

	CWeaponCSBase *pActiveWeapon = m_pHelpers->CSAnim_GetActiveWeapon();
	bool currentWeaponIsDeployedWeapon = ActiveWeaponIsDeployed();

	MDLCACHE_CRITICAL_SECTION();
	switch ( animEvent )
	{
	case PLAYERANIMEVENT_FIRE_GUN_PRIMARY:
	case PLAYERANIMEVENT_FIRE_GUN_PRIMARY_OPT:
	case PLAYERANIMEVENT_FIRE_GUN_PRIMARY_SPECIAL1:
	case PLAYERANIMEVENT_FIRE_GUN_PRIMARY_OPT_SPECIAL1:
	case PLAYERANIMEVENT_FIRE_GUN_SECONDARY:
	case PLAYERANIMEVENT_FIRE_GUN_SECONDARY_SPECIAL1:
		if ( currentWeaponIsDeployedWeapon )
		{
			// Regardless of what we're doing in the fire layer, restart it.
			m_activeFireEvent = animEvent;
			m_iFireSequence = CalcFireLayerSequence( animEvent );
			m_bFiring = m_iFireSequence != -1;
			m_flFireCycle = 0;
	
			// If we are interrupting a (shotgun) reload, cancel the reload, and fire next frame.
			if ( m_bFiring && m_bReloading )
			{
				m_bReloading = false;
				m_iReloadSequence = -1;
	
				m_delayedFire = animEvent;
				m_bFiring = false;
				m_iFireSequence = -1;
	
				ClearAnimationLayer( RELOADSEQUENCE_LAYER );
			}

			if ( m_bFiring && !m_bReloading )
			{
				if ( m_pPlayer )
				{
					m_pPlayer->ProcessMuzzleFlashEvent();
				}
			}
		}
		break;

	case PLAYERANIMEVENT_JUMP:
		// Play the jump animation.
		m_bJumping = true;
		m_bFirstJumpFrame = true;
		m_flJumpStartTime = gpGlobals->curtime;
		break;

	case PLAYERANIMEVENT_RELOAD:
		{
			// ignore normal reload events for shotguns - they get sent to trigger sounds etc only
			// [mlowrance] Mag7 special case: it's classified as a Shotgun, but reloads like a conventional gun with a clip.
			if ( pActiveWeapon && ( pActiveWeapon->GetWeaponType() != WEAPONTYPE_SHOTGUN || pActiveWeapon->GetCSWeaponID() == WEAPON_MAG7 ) )
			{
				m_iReloadSequence = CalcReloadLayerSequence( animEvent );
				if ( m_iReloadSequence != -1 )
				{
					m_bReloading = true;
					m_flReloadCycle = 0;
				}
				else
				{
					m_bReloading = false;
				}
			}
		}
		break;

	case PLAYERANIMEVENT_SILENCER_ATTACH:
		{
			m_iSilencerChangeSequence = CalcSilencerChangeLayerSequence( animEvent );
			if ( m_iSilencerChangeSequence != -1 )
			{
				m_bSilencerChanging = true;
				m_flSilencerChangeCycle = 0;
			}
			else
			{
				m_bSilencerChanging = false;
			}
		}
		break;

	case PLAYERANIMEVENT_SILENCER_DETACH:
		{
			m_iSilencerChangeSequence = CalcSilencerChangeLayerSequence( animEvent );
			if ( m_iSilencerChangeSequence != -1 )
			{
				m_bSilencerChanging = true;
				m_flSilencerChangeCycle = 0;
			}
			else
			{
				m_bSilencerChanging = false;
			}
		}
		break;

	case PLAYERANIMEVENT_RELOAD_START:
	case PLAYERANIMEVENT_RELOAD_LOOP:
		// Set the hold time for _start and _loop anims, then fall through to the _end case
		m_flReloadHoldEndTime = gpGlobals->curtime + 0.75f;

	case PLAYERANIMEVENT_RELOAD_END:
		{
			// ignore shotgun reload events for non-shotguns
			// [mlowrance] Mag7 special case: it's classified as a Shotgun, but reloads like a conventional gun with a clip.
			if ( pActiveWeapon && ( pActiveWeapon->GetWeaponType() != WEAPONTYPE_SHOTGUN || pActiveWeapon->GetCSWeaponID() == WEAPON_MAG7 ) )
			{
				m_flReloadHoldEndTime = 0.0f;  // clear this out in case we set it in _START or _LOOP above
			}
			else
			{
				m_iReloadSequence = CalcReloadLayerSequence( animEvent );
				if ( m_iReloadSequence != -1 )
				{
					m_bReloading = true;
					m_flReloadCycle = 0;
				}
				else
				{
					m_bReloading = false;
				}
			}
		}
		break;

	case PLAYERANIMEVENT_CLEAR_FIRING:
		{
			m_iFireSequence = -1;
			m_flFireCycle = 0;
			m_bFiring = false;
			ClearAnimationLayer( FIRESEQUENCE_LAYER );
			ClearAnimationLayer( FIRESEQUENCE2_LAYER );
		}
		break;

	case PLAYERANIMEVENT_DEPLOY:
		{
			// Update the weapon to use for animations.
			if ( pActiveWeapon != nullptr )
			{
				m_iDeployedWeaponID = pActiveWeapon->GetCSWeaponID();
			}
			else
			{
				m_iDeployedWeaponID = WEAPON_NONE;
			}

			// Start the Deploy animation if we have one.
			m_iDeploySequence = CalcDeployLayerSequence();
			if ( m_iDeploySequence != -1 )
			{
				m_bDeploying = true;
				m_flDeployCycle = 0.0f;

			}
			else
			{
				m_bDeploying = false;
				m_iDeploySequence = -1;
				ClearAnimationLayer( DEPLOYSEQUENCE_LAYER );
			}

		}
		break;

	case PLAYERANIMEVENT_GRENADE_PULL_PIN:
		break;

	//default:
	//	Assert( !"CCSPlayerAnimState::DoAnimationEvent" );
	}
}

float g_flThrowGrenadeFraction = 0.25;
bool CCSPlayerAnimState::IsThrowingGrenade()
{
	if ( m_bThrowingGrenade )
	{
		// An animation event would be more appropriate here.
		return m_flGrenadeCycle < g_flThrowGrenadeFraction;
	}
	else
	{
		bool bThrowPending = (m_iLastThrowGrenadeCounter != GetOuterGrenadeThrowCounter());
		return bThrowPending || IsOuterGrenadePrimed();
	}
}

bool CCSPlayerAnimState::ShouldHideGrenadeDuringThrow()
{
	return m_bThrowingGrenade && (m_flGrenadeCycle > 0.1f);
}

int CCSPlayerAnimState::CalcSilencerChangeLayerSequence( PlayerAnimEvent_t animEvent )
{
	if ( m_delayedFire != PLAYERANIMEVENT_COUNT )
		return ACTIVITY_NOT_AVAILABLE;

	const char *weaponSuffix = GetWeaponSuffix();
	if ( !weaponSuffix )
		return ACTIVITY_NOT_AVAILABLE;

	CWeaponCSBase *pWeapon = m_pHelpers->CSAnim_GetActiveWeapon();
	if ( !pWeapon )
		return ACTIVITY_NOT_AVAILABLE;

	char szName[512];
	int iSilencerChangeSequence = -1;

	if ( animEvent == PLAYERANIMEVENT_SILENCER_ATTACH )
	{
		V_snprintf( szName, sizeof( szName ), DEFAULT_SILENCER_ATTACH_NAME, weaponSuffix );
	}
	else if ( animEvent == PLAYERANIMEVENT_SILENCER_DETACH )
	{
		V_snprintf( szName, sizeof( szName ), DEFAULT_SILENCER_DETACH_NAME, weaponSuffix );
	}
	else
	{
		return ACTIVITY_NOT_AVAILABLE;
	}
	
	iSilencerChangeSequence = m_pOuter->LookupSequence( szName );
	if ( iSilencerChangeSequence != -1 )
		return iSilencerChangeSequence;

	// no current fallback
	return ACTIVITY_NOT_AVAILABLE;
}

int CCSPlayerAnimState::CalcReloadLayerSequence( PlayerAnimEvent_t animEvent )
{
	if ( m_delayedFire != PLAYERANIMEVENT_COUNT )
		return -1;

	const char *weaponSuffix = GetWeaponSuffix();
	if ( !weaponSuffix )
		return -1;

	CWeaponCSBase *pWeapon = m_pHelpers->CSAnim_GetActiveWeapon();
	if ( !pWeapon )
		return -1;

	const char *prefix = "";
	switch ( GetCurrentMainSequenceActivity() )
	{
		case ACT_PLAYER_RUN_FIRE:
		case ACT_RUN:
			prefix = "run";
			break;

		case ACT_PLAYER_WALK_FIRE:
		case ACT_WALK:
			prefix = "walk";
			break;

		case ACT_PLAYER_CROUCH_FIRE:
		case ACT_CROUCHIDLE:
			prefix = "crouch_idle";
			break;

		case ACT_PLAYER_CROUCH_WALK_FIRE:
		case ACT_RUN_CROUCH:
			prefix = "crouch_walk";
			break;

		default:
		case ACT_PLAYER_IDLE_FIRE:
			prefix = "idle";
			break;
	}

	const char *reloadSuffix = "";
	switch ( animEvent )
	{
	case PLAYERANIMEVENT_RELOAD_START:
		reloadSuffix = "_start";
		break;

	case PLAYERANIMEVENT_RELOAD_LOOP:
		reloadSuffix = "_loop";
		break;

	case PLAYERANIMEVENT_RELOAD_END:
		reloadSuffix = "_end";
		break;
	}

	// First, look for <prefix>_reload_<weapon name><_start|_loop|_end>.
	char szName[512];
	int iReloadSequence = -1;

// Avoid doing this look up since we don't need it for CSGO

	// Next, look for reload_<weapon name><_start|_loop|_end>.
	V_snprintf( szName, sizeof( szName ), "reload_%s%s", weaponSuffix, reloadSuffix );
	iReloadSequence = m_pOuter->LookupSequence( szName );
	if ( iReloadSequence != -1 )
		return iReloadSequence;

	// Ok, look for generic categories.. pistol, shotgun, rifle, etc.
	if ( pWeapon->GetWeaponType() == WEAPONTYPE_PISTOL )
	{
		V_snprintf( szName, sizeof( szName ), "reload_pistol" );
		iReloadSequence = m_pOuter->LookupSequence( szName );
		if ( iReloadSequence != -1 )
			return iReloadSequence;
	}
			
	// Fall back to reload_m4.
	iReloadSequence = CalcSequenceIndex( "reload_m4" );
	if ( iReloadSequence > 0 )
		return iReloadSequence;

	return -1;
}

void CCSPlayerAnimState::UpdateLayerSequenceGeneric( CStudioHdr *pStudioHdr, int iLayer, bool &bEnabled, float &flCurCycle, int &iSequence, bool bWaitAtEnd, float flWeight )
{
	if ( !bEnabled || iSequence < 0 )
		return;

	CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iLayer );
	pLayer->SetSequence( iSequence );

	// find cycle rate for the layer
	float flSequenceCycleRate = m_pPlayer->GetLayerSequenceCycleRate( pLayer, iSequence );

	// weapon vs self
	flCurCycle += flSequenceCycleRate * gpGlobals->frametime;

	if ( flCurCycle > 1 )
	{
		if ( bWaitAtEnd )
		{
			flCurCycle = 1;
		}
		else
		{
			// Not firing anymore.
			bEnabled = false;
			iSequence = 0;
			return;
		}
	}

	// Now dump the state into its animation layer.
	pLayer->SetCycle( flCurCycle );

	pLayer->SetPlaybackRate( 1.0f );
	pLayer->SetWeight( flWeight );
	pLayer->SetOrder( iLayer );
}

bool CCSPlayerAnimState::IsOuterGrenadePrimed()
{
	CBaseCombatCharacter *pChar = m_pOuter->MyCombatCharacterPointer();
	if ( pChar )
	{
		CBaseCSGrenade *pGren = dynamic_cast<CBaseCSGrenade*>( pChar->GetActiveWeapon() );
		return pGren && pGren->IsPinPulled();
	}
	else
	{
		return false;
	}
}

void CCSPlayerAnimState::ComputeGrenadeSequence( CStudioHdr *pStudioHdr )
{
	CVProfScope _( "CCSPlayerAnimState::ComputeGrenadeSequence", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	if ( m_bThrowingGrenade )
	{
		UpdateLayerSequenceGeneric( pStudioHdr, GRENADESEQUENCE_LAYER, m_bThrowingGrenade, m_flGrenadeCycle, m_iGrenadeSequence, false );
	}
	else
	{

		bool bThrowingUnderhand = false;

		if ( m_pPlayer )
		{
			CBaseCombatWeapon *pWeapon = m_pPlayer->GetActiveWeapon();
			CBaseCSGrenade *pGren = dynamic_cast<CBaseCSGrenade*>( pWeapon );
			if ( !pGren )
			{
				// The player no longer has a grenade equipped. Bail.
				m_iLastThrowGrenadeCounter = GetOuterGrenadeThrowCounter();
				return;
			}
			else
			{
				bThrowingUnderhand = pGren->IsThrownUnderhand();
			}
		}

		// Priming the grenade isn't an event.. we just watch the player for it.
		// Also play the prime animation first if he wants to throw the grenade.
		bool bThrowPending = (m_iLastThrowGrenadeCounter != GetOuterGrenadeThrowCounter());
		if ( IsOuterGrenadePrimed() || bThrowPending )
		{
			if ( !m_bPrimingGrenade )
			{
				// If this guy just popped into our PVS, and he's got his grenade primed, then
				// let's assume that it's all the way primed rather than playing the prime
				// animation from the start.
				if ( TimeSinceLastAnimationStateClear() < 0.4f )
				{
					m_flGrenadeCycle = 1;
				}
				else
				{
					m_flGrenadeCycle = 0;
				}
					
				m_bPrimingGrenade = true;
			}

			// [MLowrance] Moved this from the IF statement above.  This allows the sequences to switch while maintaining the last frame of the prime anim.
			// So priming the grenade at Idle, then going to a Run while holding the last frame doesn't break the character.
			m_iGrenadeSequence = CalcGrenadePrimeSequence();

			UpdateLayerSequenceGeneric( pStudioHdr, GRENADESEQUENCE_LAYER, m_bPrimingGrenade, m_flGrenadeCycle, m_iGrenadeSequence, true );
			
			// If we're waiting to throw and we're done playing the prime animation...
			// [mlowrance] due to timing and gameplay issues, if we're pending to throw... don't wait, do it
			if ( bThrowPending ) //&& m_flGrenadeCycle == 1 )
			{
				m_iLastThrowGrenadeCounter = GetOuterGrenadeThrowCounter();

				// Now play the throw animation.
				m_iGrenadeSequence = CalcGrenadeThrowSequence( bThrowingUnderhand );
				if ( m_iGrenadeSequence != -1 )
				{
					// Configure to start playing 
					m_bThrowingGrenade = true;
					m_bPrimingGrenade = false;
					m_flGrenadeCycle = 0;
				}
			}
		}
		else
		{
			m_bPrimingGrenade = false;
		}
	}
}

int CCSPlayerAnimState::CalcGrenadePrimeSequence()
{
	const char *prefix = "";
	switch ( GetCurrentMainSequenceActivity() )
	{
		case ACT_PLAYER_RUN_FIRE:
		case ACT_RUN:
			prefix = "run";
			break;

		case ACT_PLAYER_WALK_FIRE:
		case ACT_WALK:
			prefix = "walk";
			break;

		case ACT_PLAYER_CROUCH_FIRE:
		case ACT_CROUCHIDLE:
			prefix = "crouch_idle";
			break;

		case ACT_PLAYER_CROUCH_WALK_FIRE:
		case ACT_RUN_CROUCH:
			prefix = "crouch_walk";
			break;

		default:
		case ACT_PLAYER_IDLE_FIRE:
			prefix = "idle";
			break;
	}

	return CalcSequenceIndex( "%s_shoot_gren1", prefix );
}

int CCSPlayerAnimState::CalcGrenadeThrowSequence( bool bUnderhand )
{
	const char *prefix = "";
	switch ( GetCurrentMainSequenceActivity() )
	{
		case ACT_PLAYER_RUN_FIRE:
		case ACT_RUN:
			prefix = "run";
			break;

		case ACT_PLAYER_WALK_FIRE:
		case ACT_WALK:
			prefix = "walk";
			break;

		case ACT_PLAYER_CROUCH_FIRE:
		case ACT_CROUCHIDLE:
			prefix = "crouch_idle";
			break;

		case ACT_PLAYER_CROUCH_WALK_FIRE:
		case ACT_RUN_CROUCH:
			prefix = "crouch_walk";
			break;

		default:
		case ACT_PLAYER_IDLE_FIRE:
			prefix = "idle";
			break;
	}
	if ( bUnderhand )
	{
		return CalcSequenceIndex( "%s_shoot_gren3", prefix );
	}
	else
	{
		return CalcSequenceIndex( "%s_shoot_gren2", prefix );
	}
}

int CCSPlayerAnimState::GetOuterGrenadeThrowCounter()
{
	if ( m_pPlayer )
		return m_pPlayer->m_iThrowGrenadeCounter;
	else
		return 0;
}

void CCSPlayerAnimState::ComputeSilencerChangeSequence( CStudioHdr *pStudioHdr )
{
	CVProfScope _( "CCSPlayerAnimState::ComputeSilencerChangeSequence", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	UpdateLayerSequenceGeneric( pStudioHdr, SILENCERSEQUENCE_LAYER, m_bSilencerChanging, m_flSilencerChangeCycle, m_iSilencerChangeSequence, false );
}

void CCSPlayerAnimState::ComputeReloadSequence( CStudioHdr *pStudioHdr )
{
	CVProfScope _( "CCSPlayerAnimState::ComputeReloadSequence", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	bool hold = m_flReloadHoldEndTime > gpGlobals->curtime;
	UpdateLayerSequenceGeneric( pStudioHdr, RELOADSEQUENCE_LAYER, m_bReloading, m_flReloadCycle, m_iReloadSequence, hold );
	if ( !m_bReloading )
	{
		m_flReloadHoldEndTime = 0.0f;
	}
}

int CCSPlayerAnimState::CalcFlinchLayerSequence( CBaseCombatCharacter *pBaseCombatCharacter )
{
	if ( !pBaseCombatCharacter )
		return ACTIVITY_NOT_AVAILABLE;

	int nSequence = ACTIVITY_NOT_AVAILABLE;

	float flTimeSinceLastInjury = pBaseCombatCharacter->GetTimeSinceLastInjury();
	float flTimeSinceEndOfLastFlinch = gpGlobals->curtime - ( m_flFlinchStartTime + m_flFlinchLength );

	// If there were hurt after the end of the last flinch, time to flinch again!
	bool bFlinch = pBaseCombatCharacter->HasEverBeenInjured() && ( flTimeSinceLastInjury <= flTimeSinceEndOfLastFlinch );

	if ( bFlinch )
	{
		Activity flinchActivity;

		if ( m_pOuter->GetFlags() & FL_DUCKING )
		{
			// Crouching flinches
			if ( pBaseCombatCharacter->LastHitGroup() != HITGROUP_HEAD )
			{
				switch ( pBaseCombatCharacter->GetLastInjuryRelativeDirection() )
				{
				case DAMAGED_DIR_BACK:
					flinchActivity = ACT_FLINCH_CROUCH_FRONT;
					break;

				case DAMAGED_DIR_LEFT:
					flinchActivity = ACT_FLINCH_CROUCH_LEFT;
					break;

				case DAMAGED_DIR_RIGHT:
					flinchActivity = ACT_FLINCH_CROUCH_RIGHT;
					break;

				case DAMAGED_DIR_FRONT:
				case DAMAGED_DIR_NONE:
				default:
					flinchActivity = ACT_FLINCH_CROUCH_FRONT;
					break;
				}
			}
			else
			{
				switch ( pBaseCombatCharacter->GetLastInjuryRelativeDirection() )
				{
				case DAMAGED_DIR_BACK:
					flinchActivity = ACT_FLINCH_HEAD_BACK;
					break;

				case DAMAGED_DIR_LEFT:
					flinchActivity = ACT_FLINCH_HEAD_LEFT;
					break;

				case DAMAGED_DIR_RIGHT:
					flinchActivity = ACT_FLINCH_HEAD_RIGHT;
					break;

				case DAMAGED_DIR_FRONT:
				case DAMAGED_DIR_NONE:
				default:
					flinchActivity = ACT_FLINCH_HEAD;
					break;
				}
			}

			// do we have a sequence for the ideal activity?
			if ( SelectWeightedSequence( flinchActivity ) == ACTIVITY_NOT_AVAILABLE )
			{
				flinchActivity = ACT_FLINCH_CROUCH_FRONT;
			}
		}
		else
		{
			// Standing flinches
			int nLastHitGroup = pBaseCombatCharacter->LastHitGroup();

			switch ( nLastHitGroup )
			{
				// pick a region-specific flinch
			case HITGROUP_HEAD:
				{
					switch ( pBaseCombatCharacter->GetLastInjuryRelativeDirection() )
					{
					case DAMAGED_DIR_BACK:
						flinchActivity = ACT_FLINCH_HEAD_BACK;
						break;

					case DAMAGED_DIR_LEFT:
						flinchActivity = ACT_FLINCH_HEAD_LEFT;
						break;

					case DAMAGED_DIR_RIGHT:
						flinchActivity = ACT_FLINCH_HEAD_RIGHT;
						break;

					case DAMAGED_DIR_FRONT:
					case DAMAGED_DIR_NONE:
					default:
						flinchActivity = ACT_FLINCH_HEAD;
						break;
					}			 
				}
				break;
			case HITGROUP_STOMACH:
				flinchActivity = ( pBaseCombatCharacter->GetLastInjuryRelativeDirection() == DAMAGED_DIR_BACK ) ? ACT_FLINCH_STOMACH_BACK : ACT_FLINCH_STOMACH;
				break;
			case HITGROUP_LEFTARM:
				flinchActivity = ACT_FLINCH_LEFTARM;
				break;
			case HITGROUP_RIGHTARM:
				flinchActivity = ACT_FLINCH_RIGHTARM;
				break;
			case HITGROUP_LEFTLEG:
				flinchActivity = ACT_FLINCH_LEFTLEG;
				break;
			case HITGROUP_RIGHTLEG:
				flinchActivity = ACT_FLINCH_RIGHTLEG;
				break;
			case HITGROUP_CHEST:
				flinchActivity = ( pBaseCombatCharacter->GetLastInjuryRelativeDirection() == DAMAGED_DIR_BACK ) ? ACT_FLINCH_CHEST_BACK : ACT_FLINCH_CHEST;
				break;
			case HITGROUP_GEAR:
			case HITGROUP_GENERIC:
			default:
				// just get a generic flinch.
				flinchActivity = flinchActivity = ( pBaseCombatCharacter->GetLastInjuryRelativeDirection() == DAMAGED_DIR_BACK ) ? ACT_FLINCH_STOMACH_BACK : ACT_FLINCH_PHYSICS;
				break;
			}
		}

		// Get the sequence for this activity
		nSequence = SelectWeightedSequence( flinchActivity );

		// Do we have a sequence for the ideal activity?
		if ( nSequence == ACTIVITY_NOT_AVAILABLE )
		{
			// Fall back to a basic physics flinch
			nSequence = SelectWeightedSequence( ACT_FLINCH_PHYSICS );
		}
	}

	return nSequence;
}

void CCSPlayerAnimState::ComputeFootPlantSequence( CStudioHdr *pStudioHdr )
{
	if ( !m_bInFootPlantIdleTurn )
		return;

	CVProfScope _( "CCSPlayerAnimState::ComputeFootPlantSequence", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	CBaseCombatCharacter *pBaseCombatCharacter = dynamic_cast<CBaseCombatCharacter*>(m_pOuter);
	if (!pBaseCombatCharacter)
		return;

	int nSequence = SelectWeightedSequence( m_bFootPlantIdleNeedToLiftFeet ? ACT_TURN : ACT_STEP_FORE );
	UpdateLayerSequenceGeneric( pStudioHdr, FOOTPLANTSEQUENCE_LAYER, m_bInFootPlantIdleTurn, m_flFootPlantIdleTurnCycle, nSequence, false );
}

void CCSPlayerAnimState::ComputeFlinchSequence( CStudioHdr *pStudioHdr )
{
	CVProfScope _( "CCSPlayerAnimState::ComputeFlinchSequence", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	CBaseCombatCharacter *pBaseCombatCharacter = dynamic_cast< CBaseCombatCharacter* >( m_pOuter );
	if ( !pBaseCombatCharacter )
		return;

	float flInterp = ( m_flFlinchLength <= 0.0f ) ? ( -1.0f ) : ( ( gpGlobals->curtime - m_flFlinchStartTime ) / m_flFlinchLength );
	bool bFlinch = ( flInterp >= 0.0f && flInterp <= 1.0f );

	if ( bFlinch && 
		 ( pBaseCombatCharacter->LastHitGroup() == HITGROUP_HEAD ) && 
		 m_nFlinchSequence != SelectWeightedSequence( ACT_FLINCH_HEAD ) &&
		 m_nFlinchSequence != SelectWeightedSequence( ACT_FLINCH_HEAD_BACK ) &&
		 m_nFlinchSequence != SelectWeightedSequence( ACT_FLINCH_HEAD_LEFT ) &&
		 m_nFlinchSequence != SelectWeightedSequence( ACT_FLINCH_HEAD_RIGHT ) )
	{
		// They were hit in the head while doing a non-head flinch
		// Override it and force a new head flinch
		bFlinch = false;
		m_flFlinchStartTime = -1.0f;
		m_flFlinchLength = 0.0f;
	}

	if ( !bFlinch )
	{
		// Calculate a new flinch if we have one
		int nSequence = CalcFlinchLayerSequence( pBaseCombatCharacter );
		if ( nSequence != ACTIVITY_NOT_AVAILABLE )
		{
			flInterp = 0.0f;
			bFlinch = true;
			m_nFlinchSequence = nSequence;
			m_flFlinchStartTime = gpGlobals->curtime;
			m_flFlinchLength = m_pOuter->SequenceDuration( nSequence );
		}
	}

	// Update with the current flinch state
	UpdateLayerSequenceGeneric( pStudioHdr, FLINCHSEQUENCE_LAYER, bFlinch, flInterp, m_nFlinchSequence, false );
}

int CCSPlayerAnimState::CalcFlashedLayerSequence( CBaseCombatCharacter *pBaseCombatCharacter )
{
	if ( !pBaseCombatCharacter )
		return ACTIVITY_NOT_AVAILABLE;

	return SelectWeightedSequence( ACT_GESTURE_BIG_FLINCH );
}

void CCSPlayerAnimState::ComputeFlashedSequence( CStudioHdr *pStudioHdr )
{
	CVProfScope _( "CCSPlayerAnimState::ComputeFlashedSequence", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	CBaseCombatCharacter *pBaseCombatCharacter = dynamic_cast< CBaseCombatCharacter* >( m_pOuter );
	if ( !pBaseCombatCharacter )
		return;

	CCSPlayer *pPlayer = ToCSPlayer( pBaseCombatCharacter );
	if ( !pPlayer )
		return;
	
	float flCurrentFlashDuration = pPlayer->m_flFlashDuration.Get();
	if ( flCurrentFlashDuration != m_flLastFlashDuration )
		m_flFlashedAmount = std::max( 0.0f, flCurrentFlashDuration - 1.25f );

	bool bFlashed = m_flFlashedAmount > 0;

	if ( bFlashed )
	{
		int nSequence = CalcFlashedLayerSequence( pBaseCombatCharacter );
		if ( nSequence != ACTIVITY_NOT_AVAILABLE )
		{
			m_iFlashedSequence = nSequence;

			m_flFlashedAmountDelayed = Approach( m_flFlashedAmount, m_flFlashedAmountDelayed, gpGlobals->frametime * 5.0f );

			CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( FLASHEDSEQUENCE_LAYER );
			pLayer->SetSequence( m_iFlashedSequence );
			pLayer->SetCycle( clamp( m_flFlashedAmountDelayed, 0, 1 ) );
			pLayer->SetPlaybackRate( 1.0f );
			pLayer->SetWeight( 1.0f );
			pLayer->SetOrder( FLASHEDSEQUENCE_LAYER );

		}
	}

	if ( bFlashed )
	{
		m_flFlashedAmount -= gpGlobals->frametime;
	}
	else
	{
		m_flFlashedAmount = 0.0f;
	}
	m_flLastFlashDuration = pPlayer->m_flFlashDuration.Get();

}

int CCSPlayerAnimState::CalcTauntLayerSequence( CBaseCombatCharacter *pBaseCombatCharacter )
{
	return ACTIVITY_NOT_AVAILABLE;
	/*
	if ( !pBaseCombatCharacter )
		return ACTIVITY_NOT_AVAILABLE;

	if ( !m_pPlayer || !m_pPlayer->IsTaunting() )
		return ACTIVITY_NOT_AVAILABLE;

	CWeaponCSBase *pActiveWeapon = m_pPlayer->GetActiveCSWeapon();
	if ( !pActiveWeapon )
		return ACTIVITY_NOT_AVAILABLE;

	CEconItemView *pItemView = pActiveWeapon->GetAttributeContainer()->GetItem();
	if ( !pItemView->IsValid() )
		return ACTIVITY_NOT_AVAILABLE;

	const CEconTauntDefinition *pTauntDef = GetItemSchema()->GetTauntDefinition( pItemView->GetTauntID() );

	const char *pchTauntSequenceName = "";

	if ( pTauntDef )
	{
		pchTauntSequenceName = pTauntDef->GetSequenceName();
	}

	return m_pOuter->LookupSequence( pchTauntSequenceName );
	*/
}

void CCSPlayerAnimState::ComputeTauntSequence( CStudioHdr *pStudioHdr )
{
	CVProfScope _( "CCSPlayerAnimState::ComputeFlinchSequence", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	CBaseCombatCharacter *pBaseCombatCharacter = dynamic_cast< CBaseCombatCharacter* >( m_pOuter );
	if ( !pBaseCombatCharacter )
		return;

	float flInterp = ( m_flTauntLength <= 0.0f ) ? ( -1.0f ) : ( ( gpGlobals->curtime - m_flTauntStartTime ) / m_flTauntLength );
	bool bTaunt = ( flInterp >= 0.0f && flInterp <= 1.0f );

	if ( !bTaunt )
	{
		// Calculate a new taunt if we have one
		int nSequence = CalcTauntLayerSequence( pBaseCombatCharacter );
		if ( nSequence != ACTIVITY_NOT_AVAILABLE )
		{
			flInterp = 0.0f;
			bTaunt = true;
			m_nTauntSequence = nSequence;
			m_flTauntStartTime = gpGlobals->curtime;
			m_flTauntLength = m_pOuter->SequenceDuration( nSequence );
		}
	}

	// Update with the current taunt state
	UpdateLayerSequenceGeneric( pStudioHdr, TAUNTSEQUENCE_LAYER, bTaunt, flInterp, m_nTauntSequence, false );
}

int CCSPlayerAnimState::CalcAimLayerSequence( float *flCycle, float *flAimSequenceWeight, bool bForceIdle )
{
	CVProfScope _( "CCSPlayerAnimState::CalcAimLayerSequence", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	const char *pSuffix = GetWeaponSuffix();
	if ( !pSuffix )
		return 0;

	Activity activity = GetCurrentMainSequenceActivity();

	if ( bForceIdle )
	{
		switch ( activity )
		{
			case ACT_CROUCHIDLE:
			case ACT_RUN_CROUCH:
				return CalcSequenceIndex( "%s%s", DEFAULT_CROUCH_IDLE_NAME, pSuffix );

			default:
				return CalcSequenceIndex( "%s%s", DEFAULT_IDLE_NAME, pSuffix );
		}
	}
	else
	{
		switch ( activity )
		{
			case ACT_RUN:
			case ACT_LEAP:
				m_iCurrentAimSequence = CalcSequenceIndex( "%s%s", DEFAULT_RUN_NAME, pSuffix );
				break;

			case ACT_WALK:
			case ACT_RUNTOIDLE:
			case ACT_IDLETORUN:
			case ACT_JUMP:
				m_iCurrentAimSequence = CalcSequenceIndex( "%s%s", DEFAULT_WALK_NAME, pSuffix );
				break;

			case ACT_RUN_CROUCH:
				m_iCurrentAimSequence = CalcSequenceIndex( "%s%s", DEFAULT_CROUCH_WALK_NAME, pSuffix );
				break;
				
			// Since we blend between two transitioners, we always have the lower one be idle (hence the forceIdle flag above).
			// Because the lower on is always idle, we never want to have an idle sequence in the upper transitioner.
			case ACT_CROUCHIDLE:
			case ACT_HOP:
			case ACT_IDLE:
			default:
				break;
		}
		return m_iCurrentAimSequence;
	}
}

const char* CCSPlayerAnimState::GetWeaponSuffix()
{
	CVProfScope _( "CCSPlayerAnimState::GetWeaponSuffix", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Figure out the weapon suffix.
	CWeaponCSBase *pWeapon = m_pHelpers->CSAnim_GetActiveWeapon();
	if ( !pWeapon )
		return nullptr;

	const char *pSuffix = pWeapon->GetPlayerAnimationExtension();

	return pSuffix;
}

int CCSPlayerAnimState::CalcFireLayerSequence(PlayerAnimEvent_t animEvent)
{
	// Figure out the weapon suffix.
	CWeaponCSBase *pWeapon = m_pHelpers->CSAnim_GetActiveWeapon();
	if ( !pWeapon )
		return -1;

	const char *pSuffix = GetWeaponSuffix();
	if ( !pSuffix )
		return -1;

	char tempsuffix[256];
	if ( pWeapon->GetCSWeaponID() == WEAPON_ELITE )
	{
		bool bPrimary = (animEvent == PLAYERANIMEVENT_FIRE_GUN_PRIMARY);
		V_snprintf( tempsuffix, sizeof(tempsuffix), "%s_%c", pSuffix, bPrimary?'r':'l' );
		pSuffix = tempsuffix;
	}
	else if ( pWeapon->GetCSWeaponID() == WEAPON_KNIFE || pWeapon->GetCSWeaponID() == WEAPON_KNIFE_GG )
	{
		const char* newSuffix = tempsuffix;

		if ( animEvent == PLAYERANIMEVENT_FIRE_GUN_PRIMARY )
		{
			V_snprintf( tempsuffix, sizeof(tempsuffix), "%s_light_r", pSuffix );
		}
		else if ( animEvent == PLAYERANIMEVENT_FIRE_GUN_PRIMARY_OPT )
		{
			V_snprintf( tempsuffix, sizeof(tempsuffix), "%s_light_l", pSuffix );
		}
		else if ( animEvent == PLAYERANIMEVENT_FIRE_GUN_PRIMARY_SPECIAL1 )
		{
			V_snprintf( tempsuffix, sizeof(tempsuffix), "%s_light_r_bs", pSuffix );
		}
		else if ( animEvent == PLAYERANIMEVENT_FIRE_GUN_PRIMARY_OPT_SPECIAL1 )
		{
			V_snprintf( tempsuffix, sizeof(tempsuffix), "%s_light_l_bs", pSuffix );
		}
		else if ( animEvent == PLAYERANIMEVENT_FIRE_GUN_SECONDARY )
		{
			V_snprintf( tempsuffix, sizeof(tempsuffix), "%s_heavy", pSuffix );
		}
		else if ( animEvent == PLAYERANIMEVENT_FIRE_GUN_SECONDARY_SPECIAL1 )
		{
			V_snprintf( tempsuffix, sizeof(tempsuffix), "%s_heavy_bs", pSuffix );
		}
		else
		{
			newSuffix = pSuffix;
		}

		pSuffix = newSuffix;
	}

	// Grenades handle their fire events separately
	if ( animEvent == PLAYERANIMEVENT_THROW_GRENADE || pWeapon->IsKindOf(WEAPONTYPE_GRENADE) )
	{
		return -1;
	}

	m_iIdleFireSequence = CalcSequenceIndex( "%s%s", DEFAULT_FIRE_IDLE_NAME, pSuffix );

	switch ( GetCurrentMainSequenceActivity() )
	{
		case ACT_PLAYER_RUN_FIRE:
		case ACT_RUN:
			return CalcSequenceIndex( "%s%s", DEFAULT_FIRE_RUN_NAME, pSuffix );

		case ACT_PLAYER_WALK_FIRE:
		case ACT_WALK:
			return CalcSequenceIndex( "%s%s", DEFAULT_FIRE_WALK_NAME, pSuffix );

		case ACT_PLAYER_CROUCH_FIRE:
		case ACT_CROUCHIDLE:
			return CalcSequenceIndex( "%s%s", DEFAULT_FIRE_CROUCH_NAME, pSuffix );

		case ACT_PLAYER_CROUCH_WALK_FIRE:
		case ACT_RUN_CROUCH:
			return CalcSequenceIndex( "%s%s", DEFAULT_FIRE_CROUCH_WALK_NAME, pSuffix );

		default:
		case ACT_PLAYER_IDLE_FIRE:
			return m_iIdleFireSequence;			
	}
}

bool CCSPlayerAnimState::CanThePlayerMove()
{
	return m_pHelpers->CSAnim_CanMove();
}

float CCSPlayerAnimState::GetCurrentMaxGroundSpeed()
{
	Activity currentActivity = 	m_pOuter->GetSequenceActivity( m_pOuter->GetSequence() );

	if ( currentActivity == ACT_WALK || currentActivity == ACT_JUMP || currentActivity == ACT_IDLE || currentActivity == ACT_HOP || currentActivity == ACT_BUSY_QUEUE || currentActivity == ACT_TURN || currentActivity == ACT_STEP_FORE )
	{
		m_flTargetMaxSpeed = ANIM_TOPSPEED_WALK;
	}
	else if ( currentActivity == ACT_RUN || currentActivity == ACT_LEAP )
	{
		m_flTargetMaxSpeed = ANIM_TOPSPEED_RUN;
		if ( m_pPlayer )
		{
			CBaseCombatWeapon *activeWeapon = m_pPlayer->GetActiveWeapon();
			if ( activeWeapon )
			{
				CWeaponCSBase *csWeapon = dynamic_cast< CWeaponCSBase * >( activeWeapon );
				if ( csWeapon )
				{
					m_flTargetMaxSpeed = csWeapon->GetMaxSpeed();
				}
			}
		}
	}
	else if ( currentActivity == ACT_RUN_CROUCH || currentActivity == ACT_CROUCHIDLE )
	{
		m_flTargetMaxSpeed = ANIM_TOPSPEED_RUN_CROUCH;
	}
	else
	{
		contract_assert( false && "Need to handle the activity %d" );
		DevMsg("Need to handle the activity %d\n", (int)currentActivity);
		m_flTargetMaxSpeed = 0.0f;
	}

	// The current max speed smoothly moves toward the TargetMax speed to avoid pops in animation.
	return m_flCurrentMaxSpeed;
}

bool CCSPlayerAnimState::HandleJumping()
{

	if ( m_bJumping )
	{
		if ( m_bFirstJumpFrame )
		{

			m_bFirstJumpFrame = false;
			RestartMainSequence();	// Reset the animation.
		}

		// Don't check if he's on the ground for a sec.. sometimes the client still has the
		// on-ground flag set right when the message comes in.
		const float GROUND_CHECK_DELAY = 0.2f;
		if ( gpGlobals->curtime - m_flJumpStartTime > GROUND_CHECK_DELAY )
		{
			if ( m_pOuter->GetFlags() & FL_ONGROUND )
			{
				m_flPostLandCrouchEndTime = gpGlobals->curtime + post_jump_crouch.GetFloat();
				m_bJumping = false;
				RestartMainSequence();	// Reset the animation.
			}
		}
	}

	// Are we still jumping? If so, keep playing the jump animation.
	return m_bJumping;
}

Activity CCSPlayerAnimState::CalcMainActivity()
{
	float flOuterSpeed = GetOuterXYSpeed();

	if ( HandleJumping() )
	{
		if ( flOuterSpeed > MOVING_MINIMUM_SPEED )
		{
			if ( flOuterSpeed > ARBITRARY_RUN_SPEED )
				return ACT_LEAP;
			else
				return ACT_JUMP;
		}
		return ACT_HOP;
	}
	else
	{
		int moveState = m_pPlayer->m_iMoveState;
		bool inPostJump = false;

		inPostJump = ( gpGlobals->curtime < m_flPostLandCrouchEndTime );

		if ( m_pOuter->GetFlags() & FL_ANIMDUCKING || inPostJump )
		{
			// If the player is trying to run after jumping, we transition directly to the run animation instead of going through the
			// chain of animations based soley on the player's speed.
			if ( inPostJump && ( moveState == MOVESTATE_RUN ) )
			{
				m_bTryingToRunAfterJump = true;
			}

			if ( flOuterSpeed > MOVING_MINIMUM_SPEED )
			{
				return ACT_RUN_CROUCH;
			}

			return ACT_CROUCHIDLE;
		}
		else
		{

			if ( m_CurrentActivity == ACT_IDLE || m_CurrentActivity == ACT_TURN || m_CurrentActivity == ACT_STEP_FORE )
			{
				//we aren't showing any movement animation at the moment.

				//but we want to walk or run
				if ( moveState != MOVESTATE_IDLE )
				{
					//we can walk right away, pose params will blend us in nicely
					m_CurrentActivity = ACT_WALK;

					//but we need to be going fast enough to go all the way up to a run
					if ( flOuterSpeed > ARBITRARY_RUN_SPEED )
					{
						m_CurrentActivity = ACT_RUN;
					}
				}
			}
			else if ( m_CurrentActivity == ACT_WALK )
			{
				//we're showing a little movement animation

				//but we want to stop
				if ( moveState == MOVESTATE_IDLE )
				{
					//only stop if we're slow enough
					if ( flOuterSpeed < MOVING_MINIMUM_SPEED && GetTimeSinceLastActChange() > ANIM_ACT_DURATION_BEFORE_RETURN_TO_IDLE )
					{
						m_CurrentActivity = ACT_IDLE;
					}
				}

				//but we want to RUN we need to be moving fast enough
				else if ( moveState == MOVESTATE_RUN && flOuterSpeed > ARBITRARY_RUN_SPEED )
				{
					m_CurrentActivity = ACT_RUN;
				}

			}
			else if ( m_CurrentActivity == ACT_RUN )
			{
				//we're playing the run animation

				//if we want to walk, don't do it until we drop into walk speed
				if ( moveState == MOVESTATE_WALK && flOuterSpeed < ARBITRARY_RUN_SPEED && GetTimeSinceLastActChange() > ANIM_ACT_DURATION_BEFORE_RETURN_TO_WALK )
				{
					m_CurrentActivity = ACT_WALK;
				}
				//and if we want to stop, don't do it until we drop to near-stop speed
				else if ( moveState == MOVESTATE_IDLE && flOuterSpeed < MOVING_MINIMUM_SPEED && GetTimeSinceLastActChange() > ANIM_ACT_DURATION_BEFORE_RETURN_TO_IDLE )
				{
					m_CurrentActivity = ACT_IDLE;
				}

			}

			// As soon as they lift up on trying to run, we no longer force the post run animation after a jump.
			if ( moveState != MOVESTATE_RUN )
			{
				m_bTryingToRunAfterJump = false;
			}

			if ( m_bTryingToRunAfterJump )
			{
				m_CurrentActivity = ACT_RUN;
			}

			UpdateTimeSinceLastActChange();

			return m_CurrentActivity;
		}

	}
}

void CCSPlayerAnimState::DebugShowAnimState( int iStartLine )
{
	engine->Con_NPrintf( iStartLine++, "fire  : %s, cycle: %.2f\n", m_bFiring ? GetSequenceName( m_pOuter->GetModelPtr(), m_iFireSequence ) : "[not firing]", m_flFireCycle );
	engine->Con_NPrintf( iStartLine++, "deploy  : %s, cycle: %.2f\n", m_bDeploying ? GetSequenceName( m_pOuter->GetModelPtr(), m_iDeploySequence ) : "[not deploying]", m_flDeployCycle );
	engine->Con_NPrintf( iStartLine++, "reload: %s, cycle: %.2f\n", m_bReloading ? GetSequenceName( m_pOuter->GetModelPtr(), m_iReloadSequence ) : "[not reloading]", m_flReloadCycle );
	BaseClass::DebugShowAnimState( iStartLine );
}

void CCSPlayerAnimState::ComputeSequences( CStudioHdr *pStudioHdr )
{
	BaseClass::ComputeSequences( pStudioHdr );

	CVProfScope _( "CCSPlayerAnimState::ComputeSequences", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	
	// dispatched through weapon
	ComputeDeploySequence( pStudioHdr );
	ComputeFireSequence( pStudioHdr );
	ComputeReloadSequence( pStudioHdr );
	ComputeSilencerChangeSequence( pStudioHdr );
	ComputeGrenadeSequence( pStudioHdr );
	// not dispatched through weapon (normally)
	ComputeFlashedSequence( pStudioHdr );
	ComputeFlinchSequence( pStudioHdr );
	ComputeTauntSequence( pStudioHdr );
	ComputeFootPlantSequence(pStudioHdr);
}

void CCSPlayerAnimState::ClearAnimationLayers()
{
	if ( !m_pOuter )
		return;

	m_pOuter->SetNumAnimOverlays( NUM_LAYERS_WANTED );
	for ( int i=0; i < m_pOuter->GetNumAnimOverlays(); i++ )
	{
		ClearAnimationLayer( i );
	}
}

void CCSPlayerAnimState::ComputeFireSequence( CStudioHdr *pStudioHdr )
{
	CVProfScope _( "CCSPlayerAnimState::ComputeFireSequence", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	if ( m_delayedFire != PLAYERANIMEVENT_COUNT )
	{
		DoAnimationEvent( m_delayedFire, 0 );
		m_delayedFire = PLAYERANIMEVENT_COUNT;
	}

	// firing anims are blended in case the player starts or stops moving during long firing animations

	bool bCrouched = ( m_pOuter->GetFlags() & FL_DUCKING ) ? true : false;

	if (bCrouched)
	{
		UpdateLayerSequenceGeneric( pStudioHdr, FIRESEQUENCE_LAYER, m_bFiring, m_flFireCycle, m_iFireSequence, false );
	}
	else
	{

		float flMaxSpeed = GetCurrentMaxGroundSpeed();
		float flPortionOfMaxSpeed = 0.0f;

		if ( flMaxSpeed > 0 )
		{
			Vector vel;
			GetOuterAbsVelocity( vel );
			flPortionOfMaxSpeed = std::min( vel.Length2D() / flMaxSpeed, 1.0f );
		}	

		if ( flPortionOfMaxSpeed < 1.0f && m_iIdleFireSequence != -1 )
		{
			//if we're at all under top speed, lay down a base layer of the idle (non-moving) firing animation, weighted by that portion of max speed
			UpdateLayerSequenceGeneric( pStudioHdr, FIRESEQUENCE_LAYER, m_bFiring, m_flFireCycle, m_iIdleFireSequence, false, 1.0 - flPortionOfMaxSpeed );
		}

		if ( flPortionOfMaxSpeed > 0 )
		{
			//we are moving and firing, but playing the idle firing animation. Re-evaluate the sequence so we can swap to the walk/crouchwalk/run version.
			if ( m_bFiring && m_iFireSequence == m_iIdleFireSequence )
			{
				m_iFireSequence = CalcFireLayerSequence( m_activeFireEvent );
			}

			// blend in the moving version of this firing animation
			UpdateLayerSequenceGeneric( pStudioHdr, FIRESEQUENCE2_LAYER, m_bFiring, m_flFireCycle, m_iFireSequence, false, flPortionOfMaxSpeed );
		}
	
	}
}

int CCSPlayerAnimState::CalcDeployLayerSequence( void )
{
	// Figure out the weapon suffix.
	const char *pSuffix = GetWeaponSuffix();
	if ( !pSuffix )
		return -1;
	
	//[msmith] NOTE: Once we get all the deploy animations added in, use the line below (CalcSequenceIndex) instead of querying LookupSequence directly.
	//return CalcSequenceIndex( "deploy_%s", pSuffix );
	
	char szName[512];
	V_snprintf( szName, sizeof( szName ), "deploy_%s", pSuffix );
	return m_pOuter->LookupSequence( szName );
}

void CCSPlayerAnimState::ComputeDeploySequence( CStudioHdr *pStudioHdr )
{
	// We need to base the deploy animation on weapon changes instead of an animation event since animation
	// events are delayed and can cause base animations to be out of sync with the new weapons.	

	// There is a tricky bit here since the weapon gets set before the deploy event.
	// To fix this, while the actively deployed weapon differs from the active weapon,
	// clear out all the other layers. 
	if ( !ActiveWeaponIsDeployed() )
	{
		// Clear out the transition history so that we pop to the new weapon instead of blending to it.
		m_LowAimSequenceTransitioner.RemoveAll();
		m_HighAimSequenceTransitioner.RemoveAll();

		// Resets and turns off any Fire anim that might be playing
		m_bFiring = false;
		m_iFireSequence = -1;
		ClearAnimationLayer( FIRESEQUENCE_LAYER );
				
		// Resets and turns off any Reload anim that might be playing
		m_bReloading = false;
		m_iReloadSequence = -1;
		ClearAnimationLayer( RELOADSEQUENCE_LAYER );

		// Resets and turns off any Silencer attach/detach anim that might be playing
		m_bSilencerChanging = false;
		m_iSilencerChangeSequence = -1;
		ClearAnimationLayer( SILENCERSEQUENCE_LAYER );

		// Resets and turns off any Grenade animations that might be playing.
		m_bThrowingGrenade = false;
		m_bPrimingGrenade = false;
		m_iGrenadeSequence = -1;
		ClearAnimationLayer( GRENADESEQUENCE_LAYER );

	}

	if ( m_bDeploying )
	{
		CVProfScope _( "CCSPlayerAnimState::ComputeDeploySequence", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
		UpdateLayerSequenceGeneric( pStudioHdr, DEPLOYSEQUENCE_LAYER, m_bDeploying, m_flDeployCycle, m_iDeploySequence, false );
		
		// Not deploying anymore so clear out the layer.
		if ( !m_bDeploying )
		{
			ClearAnimationLayer( DEPLOYSEQUENCE_LAYER );
		}
	}
}

float CCSPlayerAnimState::CalcMovementPlaybackRate( bool *bIsMoving )
{
	// Determine ideal playback rate
	Vector vel;
	GetOuterAbsVelocity( vel );

	float speed = vel.Length2D();
	bool isMoving = ( speed > MOVING_MINIMUM_SPEED );

	*bIsMoving = false;
	float flReturnValue = 1;

	if ( isMoving && CanThePlayerMove() )
	{
		if ( m_flCurrentMaxSpeed < 0.001f )
		{
			flReturnValue = 0.01;
		}
		else
		{
			// [msmith]
			// The playback rate is used to blend between idle and moving.
			// If we're fully moving, we want it to be 1.0.
			flReturnValue = speed / m_flCurrentMaxSpeed;

			// We cap this to one so we are never more than fully moving.
			flReturnValue = clamp( flReturnValue, 0.0f, 1.0f );
		}
		*bIsMoving = true;
	}
	
	return flReturnValue;
}

void CCSPlayerAnimState::Update( float eyeYaw, float eyePitch )
{

	// Adjust the maxSpeed toward the targetmax speed.  Doesn't need to be perfect here.
	// We just need to avoid some pops from when the speed suddenly changes.
	const float SPEED_BLEND_VALUE = 0.15f;
	m_flCurrentMaxSpeed += ( m_flTargetMaxSpeed - m_flCurrentMaxSpeed ) * SPEED_BLEND_VALUE;

	if ( m_pPlayer && m_pPlayer->IsTaunting() )
	{
		// Get the studio header for the player.
		CStudioHdr *pStudioHdr = m_pPlayer->GetModelPtr();
		if ( !pStudioHdr )
			return;

		Vector vPositionToFace = vec3_origin; // FIXME: orientation lock target goes here!
		bool bInTaunt = m_pPlayer->IsTaunting();
		bool bIsImmobilized = bInTaunt;
		bool bForceAdjust = bInTaunt; // FIXME: Maybe want this at some point
		bool bIsOrientationLockedOnEntity = false;

		if ( !bIsImmobilized )
		{
			// Pose parameter - what direction are the player's legs running in.
			ComputePoseParam_MoveYaw( pStudioHdr );
		}

		if ( bIsOrientationLockedOnEntity )
		{
			// snap body to align with eye angles
			m_bForceAimYaw = true;

			Vector toOther = vPositionToFace - m_pPlayer->GetAbsOrigin();
			QAngle alignedAngles;
			VectorAngles( toOther, alignedAngles );

			m_flEyeYaw = alignedAngles.y;
			m_flEyePitch = alignedAngles.x;
		}
	//	else if ( bForceAdjust )
	//	{
	//		m_bForceAimYaw = true;
	//		m_flEyeYaw = m_pPlayer->GetTauntYaw();
	//	}
		
		if ( bIsOrientationLockedOnEntity || !bIsImmobilized || bForceAdjust )
		{
			eyePitch = m_flEyePitch;
			eyeYaw = m_flEyeYaw;
		}
	}

	BaseClass::Update(eyeYaw, eyePitch);
}

bool CCSPlayerAnimState::ActiveWeaponIsDeployed()
{
	CWeaponCSBase *pActiveWeapon = m_pHelpers->CSAnim_GetActiveWeapon();
	bool currentWeaponIsDeployedWeapon = true;
	if ( pActiveWeapon != nullptr )
	{
		currentWeaponIsDeployedWeapon = ( pActiveWeapon->GetCSWeaponID() == m_iDeployedWeaponID );
		if ( !currentWeaponIsDeployedWeapon )
		{
			// If the player is out of view we don't get animation events about deploy etc.
			// Because of this we use a time out to update the active weapon.
			const float MAX_DEPLOY_DELAY = 1.0f;

			if ( m_flWeaponSwitchTime == 0.0f )
			{
				m_flWeaponSwitchTime = gpGlobals->curtime;
			}
			else if ( m_flWeaponSwitchTime + MAX_DEPLOY_DELAY < gpGlobals->curtime )
			{
				// It has been MAX_DEPLOY_DELAY since the player switched weapons.
				// Go ahead and set the active weapon as the deployed weapon.
				m_iDeployedWeaponID = pActiveWeapon->GetCSWeaponID();
				currentWeaponIsDeployedWeapon = true;
			}
		}
		else
		{
			m_flWeaponSwitchTime = 0.0f;
		}
	}
	return currentWeaponIsDeployedWeapon;	
}

class CEconQuestDefinition;
class CProPlayerData;
class CBasePlayer;

using namespace ::ks::reflect;

// Purpose: CS's custom CPlayerResource

IMPLEMENT_REFLECT_CLIENTCLASS( CCS_PlayerResource, DT_CSPlayerResource, CCSPlayerResource )

ConVar cl_show_playernames_max_chars_console( side_registry(), "cl_show_playernames_max_chars_console", "0", FCVAR_CLIENTDLL | FCVAR_DEVELOPMENTONLY, "Shows all player names (including bots) as 16 W's." );

extern ConVar cl_spec_use_tournament_content_standards;
extern ConVar sv_spec_use_tournament_content_standards;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCS_PlayerResource::CCS_PlayerResource()
{
	m_Colors[TEAM_TERRORIST] = COLOR_RED;
	m_Colors[TEAM_CT] = COLOR_BLUE;
	memset( m_iMVPs, 0, sizeof( m_iMVPs ) );
	memset( m_bHasHelmet, 0, sizeof( m_bHasHelmet ) );
	memset( m_bHasDefuser, 0, sizeof( m_bHasDefuser ) );
	memset( m_iArmor, 0, sizeof( m_iArmor ) );	
	memset( m_iScore, 0, sizeof( m_iScore ) );		
	memset( m_iCompetitiveRanking, 0, sizeof( m_iCompetitiveRanking ) );	
	memset( m_iCompetitiveWins, 0, sizeof( m_iCompetitiveWins ) );	
	memset( m_iCompTeammateColor, 0, sizeof( m_iCompTeammateColor ) );
	memset( m_nEndMatchNextMapVotes, 0, sizeof( m_nEndMatchNextMapVotes ) );
	memset( m_nActiveCoinRank, 0, sizeof( m_nActiveCoinRank ) );
	memset( m_nMusicID, 0, sizeof( m_nMusicID ) );
	memset( m_bIsAssassinationTarget, 0, sizeof( m_bIsAssassinationTarget ) );

	memset( m_nPersonaDataPublicLevel, 0, sizeof( m_nPersonaDataPublicLevel ) );
	memset( m_nPersonaDataPublicCommendsLeader, 0, sizeof( m_nPersonaDataPublicCommendsLeader ) );
	memset( m_nPersonaDataPublicCommendsTeacher, 0, sizeof( m_nPersonaDataPublicCommendsTeacher ) );
	memset( m_nPersonaDataPublicCommendsFriendly, 0, sizeof( m_nPersonaDataPublicCommendsFriendly ) );

	m_bDisableAssassinationTargetNameOverride = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCS_PlayerResource::~CCS_PlayerResource()
{
}

bool CCS_PlayerResource::IsVIP(int iIndex )
{
	return m_iPlayerVIP == iIndex;
}

bool CCS_PlayerResource::HasC4(int iIndex )
{
	return m_iPlayerC4 == iIndex;
}

bool CCS_PlayerResource::IsHostageAlive(int iIndex)
{
	if ( iIndex < 0 || iIndex >= MAX_HOSTAGES )
		return false;

	return m_bHostageAlive[iIndex];
}

bool CCS_PlayerResource::IsHostageFollowingSomeone(int iIndex)
{
	if ( iIndex < 0 || iIndex >= MAX_HOSTAGES )
		return false;

	return m_isHostageFollowingSomeone[iIndex];
}

int CCS_PlayerResource::GetHostageEntityID(int iIndex)
{
	if ( iIndex < 0 || iIndex >= MAX_HOSTAGES )
		return -1;

	return m_iHostageEntityIDs[iIndex];
}

const Vector CCS_PlayerResource::GetBombsiteAPosition()
{
	if ( CSGameRules() && CSGameRules()->IsBombDefuseMap() == false )
		return vec3_origin;

	return m_bombsiteCenterA;
}

const Vector CCS_PlayerResource::GetBombsiteBPosition()
{
	if ( CSGameRules() && CSGameRules()->IsBombDefuseMap() == false )
		return vec3_origin;

	return m_bombsiteCenterB;
}

const Vector CCS_PlayerResource::GetHostageRescuePosition( int iIndex )
{
	if ( iIndex < 0 || iIndex >= MAX_HOSTAGE_RESCUES )
		return vec3_origin;

	Vector ret;

	ret.x = m_hostageRescueX[iIndex];
	ret.y = m_hostageRescueY[iIndex];
	ret.z = m_hostageRescueZ[iIndex];

	return ret;
}

//--------------------------------------------------------------------------------------------------------
int CCS_PlayerResource::GetNumMVPs( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return false;

	return m_iMVPs[iIndex];
} 

//--------------------------------------------------------------------------------------------------------
bool CCS_PlayerResource::HasDefuser( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return false;

	return m_bHasDefuser[iIndex];
}

//--------------------------------------------------------------------------------------------------------
bool CCS_PlayerResource::HasHelmet( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return false;

	return m_bHasHelmet[iIndex];
}

//--------------------------------------------------------------------------------------------------------
int CCS_PlayerResource::GetArmor( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iArmor[iIndex];
}

//--------------------------------------------------------------------------------------------------------
int CCS_PlayerResource::GetScore( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iScore[iIndex];
}

//--------------------------------------------------------------------------------------------------------
int CCS_PlayerResource::GetCompetitiveRanking( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return 0;
	}

	return m_iCompetitiveRanking[iIndex];
}

//--------------------------------------------------------------------------------------------------------
int CCS_PlayerResource::GetCompetitiveWins( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return 0;
	}

	return m_iCompetitiveWins[iIndex];
}

//--------------------------------------------------------------------------------------------------------
int CCS_PlayerResource::GetCompTeammateColor( int iIndex )
{
	if ( !IsConnected( iIndex ) || !CSGameRules( ) )
		return -1;

	if ( !CSGameRules( )->IsPlayingAnyCompetitiveStrictRuleset( ) )
		return -1;

	if ( IsFakePlayer( iIndex ) )
		return -2;

	return m_iCompTeammateColor[iIndex];
}

//--------------------------------------------------------------------------------------------------------
int CCS_PlayerResource::GetTotalCashSpent( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iTotalCashSpent[iIndex];
}

//--------------------------------------------------------------------------------------------------------
int CCS_PlayerResource::GetCashSpentThisRound( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iCashSpentThisRound[iIndex];
}

//--------------------------------------------------------------------------------------------------------
int CCS_PlayerResource::GetEndMatchNextMapVote( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return -1;

	return m_nEndMatchNextMapVotes[iIndex];
}

int CCS_PlayerResource::GetActiveCoinRank( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return 0;
	}

	return m_nActiveCoinRank[iIndex];
}

int CCS_PlayerResource::GetPersonaDataPublicLevel( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return 0;
	}

	return m_nPersonaDataPublicLevel[ iIndex ];
}

int CCS_PlayerResource::GetPersonaDataPublicCommendsLeader( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return 0;
	}

	return m_nPersonaDataPublicCommendsLeader[ iIndex ];
}

int CCS_PlayerResource::GetPersonaDataPublicCommendsTeacher( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return 0;
	}

	return m_nPersonaDataPublicCommendsTeacher[ iIndex ];
}

int CCS_PlayerResource::GetPersonaDataPublicCommendsFriendly( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return 0;
	}

	return m_nPersonaDataPublicCommendsFriendly[ iIndex ];
}

int CCS_PlayerResource::GetMusicID( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return 0;
	}

	return m_nMusicID[ iIndex ];
}

void CCS_PlayerResource::OnDataChanged(DataUpdateType_t updateType)
{
	BaseClass::OnDataChanged( updateType );

}

bool CCS_PlayerResource::EndMatchNextMapAllVoted( void )
{
	return m_bEndMatchNextMapAllVoted;
}

const char *CCS_PlayerResource::GetClanTag( int iIndex )
{
	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
	{
		contract_assert( false );
		return "";
	}

	if ( !IsConnected( iIndex ) )
		return "";

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return "";
	}

	return m_szClan[iIndex];
}

bool CCS_PlayerResource::IsControllingBot( int index )
{
	return m_bControllingBot[ index ];
}

int CCS_PlayerResource::GetControlledPlayer( int index )
{
	return m_iControlledPlayer[ index ];
}

int CCS_PlayerResource::GetControlledByPlayer( int index )
{
	return m_iControlledByPlayer[ index ];
}

bool CCS_PlayerResource::IsAssassinationTarget( int index )
{
	if ( m_bDisableAssassinationTargetNameOverride )
		return false;

	return m_bIsAssassinationTarget[ index ];
}

CCS_PlayerResource * GetCSResources( void )
{
	return ( CCS_PlayerResource* ) g_PR;
}

const char *CCS_PlayerResource::GetPlayerName( int index )
{
	if ( cl_show_playernames_max_chars_console.GetBool() )
		return "WWWWWWWWWWWWWWWW";

	CEconQuestDefinition *pQuestDef = CSGameRules()->GetActiveAssassinationQuest();
	if ( IsAssassinationTarget( index ) && pQuestDef )
	{
		static char szAssassinationTargetName[ MAX_PLAYER_NAME_LENGTH ];
		const char* szLocToken = Helper_GetLocalPlayerAssassinationQuestLocToken( pQuestDef );
		if ( szLocToken )
		{
			V_UnicodeToUTF8( g_pLocalize->Find( szLocToken ), szAssassinationTargetName, MAX_PLAYER_NAME_LENGTH );
			return szAssassinationTargetName;
		}
	}

	return BaseClass::GetPlayerName( index );
}

void CCS_PlayerResource::UpdatePlayerName( int slot )
{
	if ( slot < 1 || slot > MAX_PLAYERS )
	{
		Error( "UpdatePlayerName with bogus slot %d\n", slot );
		return;
	}

	CBasePlayer *pLocalPlayer = CBasePlayer::GetLocalPlayer( );

	char const *pchPlayerName = nullptr;
	player_info_t sPlayerInfo;

	bool bUseTournamentContentStandards = cl_spec_use_tournament_content_standards.GetBool( ) || sv_spec_use_tournament_content_standards.GetBool( );
	// is this spectator insisting to use official pro player names?

	bool bGetProPlayerName = bUseTournamentContentStandards;

	if ( IsConnected( slot ) && ( pLocalPlayer->IsSpectator( ) || pLocalPlayer->IsHLTV( ) ) && bGetProPlayerName )
	{
		CProPlayerData const *pProPlayerData = pProPlayerData = GEconItemSchema( ).GetProPlayerDataByAccountID( GetXuid( index ) );

		if ( pProPlayerData )
		{
			pchPlayerName = pProPlayerData->GetName( );
		}
	}

	// not a pro or we don't care.
	 if ( !pchPlayerName && 
		 IsConnected( slot ) &&
		engine->GetPlayerInfo( slot, &sPlayerInfo ) )
	{
		pchPlayerName = sPlayerInfo.name;

		if ( sPlayerInfo.fakeplayer && pchPlayerName && *pchPlayerName )
		{	// also avoids censorship in pre-localized names
			UpdateAsLocalizedFakePlayerName( slot, pchPlayerName );
			return;
		}
	}

	if ( !pchPlayerName )
		pchPlayerName = PLAYER_UNCONNECTED_NAME;

	if ( !m_szName[ slot ] || V_stricmp( m_szName[ slot ], pchPlayerName ) )
	{
		m_szName[ slot ] = AllocPooledString( pchPlayerName );
	}
}

int CCS_PlayerResource::GetBotDifficulty( int index )
{
	return m_iBotDifficulty[index];
}

const wchar_t* CCS_PlayerResource::GetDecoratedPlayerName( int index, wchar_t* buffer, int buffsize, EDecoratedPlayerNameFlag_t flags )
{
	if ( IsConnected( index ) )
	{
		bool addBotToNameIfControllingBot = !!(flags & k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot);
		bool useNameOfControllingPlayer = !(flags & k_EDecoratedPlayerNameFlag_DontUseNameOfControllingPlayer);
		bool bShowClanName = !(flags & k_EDecoratedPlayerNameFlag_DontShowClanName);
		bool bMakeStringSafe = !(flags & k_EDecoratedPlayerNameFlag_DontMakeStringSafe);
		bool bSkipAssassinationTargetName = !!(flags & k_EDecoratedPlayerNameFlag_DontUseAssassinationTargetName); 

		int nameIndex = index;
		int controlledBy = GetControlledByPlayer( index );
		int nBotControlStringType = 0; // normal name

		if( controlledBy && useNameOfControllingPlayer )
		{
			nBotControlStringType = 1;// BOT ( name )
			nameIndex = controlledBy;

			// HACK: We want to have a fake name for this player if they're our target
			// but we want to show the real player name if they're controlling a bot AND show the fake name in the scoreboard
			// for their dead player entry. 
			contract_assert( m_bDisableAssassinationTargetNameOverride == false );
			m_bDisableAssassinationTargetNameOverride = true;
		}
		else if ( IsFakePlayer( index ) && !cl_show_playernames_max_chars_console.GetBool() && CSGameRules() && !CSGameRules()->IsPlayingCooperativeGametype() )
		{
			nBotControlStringType = 2; // BOT name
		}
		else if ( IsControllingBot( index ) && addBotToNameIfControllingBot )
		{
			nBotControlStringType = 1; // BOT ( name )
			contract_assert( m_bDisableAssassinationTargetNameOverride == false );
			m_bDisableAssassinationTargetNameOverride = true;
		}

		wchar_t wide_name[MAX_PLAYER_NAME_LENGTH];
		wide_name[0] = L'\0';
		char nameBuf[MAX_PLAYER_NAME_LENGTH] = {0};

		if ( nBotControlStringType == 2 && GetBotDifficulty( index ) >= 0 )
		{
			ConVarRef sv_show_bot_difficulty_in_name( "sv_show_bot_difficulty_in_name" );

			if ( sv_show_bot_difficulty_in_name.GetBool() )
			{
				// Show bot difficulty as part of the bot name
				V_snprintf( nameBuf, std::size( nameBuf ) - 1, "%s[%d]", GetPlayerName( nameIndex ), GetBotDifficulty( index ) );
			}
			else
			{
				// Hide bot difficulty
				V_snprintf( nameBuf, std::size( nameBuf ) - 1, "%s", GetPlayerName( nameIndex ) );
			}
		}
		else if ( GetCoachingTeam( index ) != 0 )
		{
			char const *szCoach = nullptr;
			switch( GetCoachingTeam( index ) )
			{
			case TEAM_TERRORIST:
				szCoach = "#SFUI_coach_name_t";
				break;
			case TEAM_CT:
				szCoach = "#SFUI_coach_name_ct";
				break;
			}

			if ( szCoach )
			{
				V_strcpy_safe( nameBuf, g_pLocalize->FindAsUTF8( szCoach ) );
				V_strcat_safe( nameBuf, " " );
			}
			V_strcat_safe( nameBuf, GetPlayerName( nameIndex ) );
		}
		else
		{
			//wchar_t wszClanTag[ MAX_PLAYER_NAME_LENGTH ];
			char szClan[MAX_PLAYER_NAME_LENGTH];
			if ( bShowClanName && V_strlen( GetClanTag( index ) ) > 1 )
			{
				const char* optionalSpace = "";
				if ( GetClanTag( index )[0] == '#' )
				{
					optionalSpace = " ";
				}
				V_snprintf( szClan, sizeof( szClan ), "%s%s ", optionalSpace, GetClanTag( index ) );
			}
			else
			{
				szClan[0] = 0;
			}
			//g_pLocalize->ConvertANSIToUnicode( szClan, wszClanTag, sizeof( wszClanTag ) );

			V_snprintf( nameBuf, std::size( nameBuf ) - 1, "%s%s", szClan, GetPlayerName( nameIndex ) );
		}

		V_UTF8ToUnicode( nameBuf /*GetPlayerName( nameIndex )*/, wide_name, sizeof( wide_name ) );

		wchar_t *pSafeWideName = wide_name;

		if ( !nBotControlStringType ) // normal name
		{
			if ( IsAssassinationTarget( index ) && !bSkipAssassinationTargetName )
			{
				Helper_GetDecoratedAssassinationTargetName( CSGameRules()->GetActiveAssassinationQuest(), buffer, buffsize/sizeof(wchar_t) );
			}
			else
			{
				V_wcsncpy( buffer, pSafeWideName, buffsize );
			}
		}
		else
		{
			const char* translationID = ( nBotControlStringType == 1 ) ? "#SFUI_bot_controlled_by" : "#SFUI_bot_decorated_name";
			g_pLocalize->ConstructString( buffer, buffsize, g_pLocalize->Find( translationID ), 1, pSafeWideName );
		}
	}
	else
	{
		*buffer = L'\0';
	}

	m_bDisableAssassinationTargetNameOverride = false;

	return buffer;
}

class CPrediction;
class CUserCmd;
class CBasePlayer;

using namespace ::ks::reflect;

static CMoveData g_MoveData;
CMoveData *g_pMoveData = &g_MoveData;

class CCSPrediction : public CPrediction
{
DECLARE_CLASS( CCSPrediction, CPrediction );

public:
	virtual void	SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );
	virtual void	FinishMove( CBasePlayer *player, CUserCmd *ucmd, CMoveData *move );
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPrediction::SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, 
	CMoveData *move )
{
	player->AvoidPhysicsProps( ucmd );

	// Call the default SetupMove code.
	BaseClass::SetupMove( player, ucmd, pHelper, move );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPrediction::FinishMove( CBasePlayer *player, CUserCmd *ucmd, CMoveData *move )
{
	// Call the default FinishMove code.
	BaseClass::FinishMove( player, ucmd, move );
}

// Expose interface to engine
// Expose interface to engine
static CCSPrediction g_Prediction;

static InterfaceReg s_Reg_CCSPrediction_IPrediction( side_registry(), []() -> void * { return static_cast<IPrediction *>( &g_Prediction ); }, VCLIENT_PREDICTION_INTERFACE_VERSION );;

CPrediction *prediction = &g_Prediction;

class CEconItemSchema;

// Item schema singleton
static CEconItemSchema s_ItemSchema;
static bool s_bSchemaInitialized = false;

CEconItemSchema* GetItemSchema()
{
	// Initialize on first access
	if ( !s_bSchemaInitialized )
	{
		s_bSchemaInitialized = true;
		if ( !s_ItemSchema.BInit( "scripts/items/items_game.txt" ) )
		{
			Warning( "Failed to load item schema from scripts/items/items_game.txt\n" );
		}
	}
	return &s_ItemSchema;
}

// Item found method strings stub
const char *g_pszItemFoundMethodStrings[NUM_ITEM_FOUND_METHODS] = {
	"#Item_Found",
	"#Item_Crafted",
	"#Item_Traded",
	"#Item_Purchased",
	"#Item_FoundInCrate",
	"#Item_Gifted",
	"#Item_Support",
	"#Item_Promotion",
	"#Item_Earned",
	"#Item_Refunded",
	"#Item_GiftWrapped",
	"#Item_Foreign",
	"#Item_CollectionReward",
	"#Item_PreviewItem",
	"#Item_PreviewItemPurchased",
	"#Item_PeriodicScoreReward",
	"#Item_Recycling",
	"#Item_TournamentDrop",
	"#Item_StockItem",
	"#Item_QuestReward",
	"#Item_LevelUpReward",
	"#Item_Unknown",
};

const float CS_PLAYER_SPEED_RUN				= 260.0f;
const float CS_PLAYER_SPEED_VIP				= 227.0f;
const float CS_PLAYER_SPEED_SHIELD			= 160.0f;
const float CS_PLAYER_SPEED_HAS_HOSTAGE		= 200.0f;
const float CS_PLAYER_SPEED_STOPPED			=   1.0f;
const float CS_PLAYER_SPEED_OBSERVER		= 900.0f;

const float CS_PLAYER_SPEED_DUCK_MODIFIER	= 0.34f;
const float CS_PLAYER_SPEED_WALK_MODIFIER	= 0.52f;
const float CS_PLAYER_SPEED_CLIMB_MODIFIER	= 0.34f;
const float CS_PLAYER_HEAVYARMOR_FLINCH_MODIFIER = 0.5f;

const float CS_PLAYER_DUCK_SPEED_IDEAL = 8.0f;

const char *pszWinPanelCategoryHeaders[] =
{
	"",
	"#winpanel_topdamage",
	"#winpanel_topheadshots",
	"#winpanel_kills"
};

const char *PlayerModelInfo::g_customizationModelCT = "models/player/custom_player/scaffold_ct.mdl";
const char *PlayerModelInfo::g_customizationModelT = "models/player/custom_player/scaffold_t.mdl";

const char *PlayerModelInfo::g_defaultTModel = "tm_phoenix";
const char *PlayerModelInfo::g_defaultCTModel = "ctm_st6";

#define CFG( mdl_substr, skintone, def_glove, sleeve, override_sleeve ) { #mdl_substr, #skintone, #def_glove, #sleeve, #override_sleeve },
static PlayerViewmodelArmConfig s_playerViewmodelArmConfigs[] =
{
#include "viewmodel_arm_config.inc"
/*
	// Old values, leaving for reference
	// character model substr		//skintone index	// default glove model																// associated sleeve													// econ override sleeve ( if present, overrides associated sleeve if glove is econ )
	{ "tm_leet",					"0",				"models/weapons/v_models/arms/glove_fingerless/v_glove_fingerless.mdl",				"",																		"" },
	{ "tm_phoenix",					"0",				"models/weapons/v_models/arms/glove_fullfinger/v_glove_fullfinger.mdl",				"",																		"" },
	{ "tm_separatist",				"0",				"models/weapons/v_models/arms/glove_fullfinger/v_glove_fullfinger.mdl",				"models/weapons/v_models/arms/separatist/v_sleeve_separatist.mdl",		"" },
	{ "tm_balkan",					"0",				"models/weapons/v_models/arms/glove_fullfinger/v_glove_fullfinger.mdl",				"models/weapons/v_models/arms/balkan/v_sleeve_balkan.mdl",				"" },
	{ "tm_professional_var4",		"1",				"models/weapons/v_models/arms/glove_fullfinger/v_glove_fullfinger.mdl",				"models/weapons/v_models/arms/professional/v_sleeve_professional.mdl",	"" },
	{ "tm_professional",			"0",				"models/weapons/v_models/arms/glove_fullfinger/v_glove_fullfinger.mdl",				"models/weapons/v_models/arms/professional/v_sleeve_professional.mdl",	"" },
	{ "tm_anarchist",				"0",				"models/weapons/v_models/arms/anarchist/v_glove_anarchist.mdl",						"",																		"models/weapons/v_models/arms/anarchist/v_sleeve_anarchist.mdl" },
	{ "tm_pirate",					"1",				"models/weapons/v_models/arms/bare/v_bare_hands.mdl",								"models/weapons/v_models/arms/pirate/v_pirate_watch.mdl",				"" },
	{ "tm_heavy",					"0",				"models/weapons/v_models/arms/glove_fingerless/v_glove_fingerless.mdl",				"models/weapons/v_models/arms/balkan/v_sleeve_balkan.mdl",				"" },
	{ "ctm_st6",					"0",				"models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle.mdl",			"models/weapons/v_models/arms/st6/v_sleeve_st6.mdl",					"" },
	{ "ctm_idf",					"0",				"models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle.mdl",			"models/weapons/v_models/arms/idf/v_sleeve_idf.mdl",					"" },
	{ "ctm_gign",					"0",				"models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle_blue.mdl",		"models/weapons/v_models/arms/gign/v_sleeve_gign.mdl",					"" },
	{ "ctm_swat_variantb",			"1",				"models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle_black.mdl",		"models/weapons/v_models/arms/swat/v_sleeve_swat.mdl",					"" },
	{ "ctm_swat",					"0",				"models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle_black.mdl",		"models/weapons/v_models/arms/swat/v_sleeve_swat.mdl",					"" },
	{ "ctm_gsg9",					"0",				"models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle_blue.mdl",		"models/weapons/v_models/arms/gsg9/v_sleeve_gsg9.mdl",					"" },
	{ "ctm_sas",					"0",				"models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle_black.mdl",		"models/weapons/v_models/arms/sas/v_sleeve_sas.mdl",					"" },
	{ "ctm_fbi",					"0",				"models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle_black.mdl",		"models/weapons/v_models/arms/fbi/v_sleeve_fbi.mdl",					"" },
*/
};
#undef CFG

PlayerModelInfo PlayerModelInfo::s_PlayerModelInfo;

PlayerModelInfo::PlayerModelInfo()
{
} 

const PlayerViewmodelArmConfig *GetPlayerViewmodelArmConfigForPlayerModel( const char* szPlayerModel )
{
	if ( szPlayerModel != nullptr )
	{
		for ( int i=0; i<std::size( s_playerViewmodelArmConfigs ); i++ )
		{
			if ( V_stristr( szPlayerModel, s_playerViewmodelArmConfigs[i].szPlayerModelSearchSubStr ) )
				return &s_playerViewmodelArmConfigs[i];
		}
	}

	contract_assert( false && "Could not determine viewmodel config for character model: %s" );
	return &s_playerViewmodelArmConfigs[0];
}

const char* QuestProgress::ReasonString(Reason reason)
{
	switch(reason)
	{
	case QUEST_OK:
		return "ok";
	case QUEST_NOT_ENOUGH_PLAYERS:
		return "not_enough_players";
	case QUEST_WARMUP:
		return "warmup";
	case QUEST_NONOFFICIAL_SERVER:
		return "nonofficial_server";
	case QUEST_NO_ENTITLEMENT:
		return "no_entitlement";
	case QUEST_NO_QUEST:
		return "no_quest";
	case QUEST_PLAYER_IS_BOT:
		return "player_is_bot";
	case QUEST_WRONG_MAP:
		return "wrong_map";
	case QUEST_WRONG_MODE:
		return "wrong_mode";
	case QUEST_NOT_SYNCED_WITH_SERVER:
		return "not_synced_with_server";
	
	case QUEST_NONINITIALIZED: // treat as 'unknown reason'.
	default:
		return "unknown";
	}
}

class CEconQuestDefinition;

using namespace ::ks::reflect;

// Purpose: Client side CCSTeam class

IMPLEMENT_REFLECT_CLIENTCLASS( CCSTeam, DT_CSTeam, CCSTeam )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSTeam::CCSTeam()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSTeam::~CCSTeam()
{
}

const char* Helper_GetLocalPlayerAssassinationQuestLocToken( const CEconQuestDefinition *pQuest )
{
	if ( !pQuest )
		return nullptr;

	KeyValues *pTargetKV = pQuest->GetStringTokens()->FindKey( "target" );
	return pTargetKV ? pTargetKV->GetString() : nullptr;
}

bool Helper_GetDecoratedAssassinationTargetName( const CEconQuestDefinition *pQuest, wchar_t* pszBuffer, size_t nBuffSizeInCharacters )
{
	const char* szToken = Helper_GetLocalPlayerAssassinationQuestLocToken( pQuest );
	if ( wchar_t *wszUndecoratedName = g_pLocalize->Find( szToken ) )
	{
		V_snwprintf( pszBuffer, nBuffSizeInCharacters, L"<font color = '#FF0000'><i>" L"%ls" L"</i></font>", wszUndecoratedName );
		return true;
	}
	return false;
}

class CViewRender;
class CBaseEntity;
class CCSPlayer;

using namespace ::ks::reflect;

// Purpose: Responsible for drawing the scene

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheCSViewScene )
PRECACHE( MATERIAL, "effects/overlaysmoke" )
PRECACHE( MATERIAL, "effects/flashbang" )
PRECACHE( MATERIAL, "effects/flashbang_white" )
PRECACHE( MATERIAL, "effects/nightvision" )
PRECACHE_REGISTER_END()

static CCSViewRender g_ViewRender;
IViewRender *GetViewRenderInstance()
{
	return &g_ViewRender;
}

CCSViewRender::CCSViewRender()
{
	m_pFlashTexture = nullptr;
}

void CCSViewRender::Init( void )
{
	CViewRender::Init();
}

void CCSViewRender::PerformNightVisionEffect( const CViewSetup &view )
{
	CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();

	if ( !pPlayer )
		return;

	if (pPlayer->GetObserverMode() == OBS_MODE_IN_EYE)
	{
		CBaseEntity *target = pPlayer->GetObserverTarget();
		if (target && target->IsPlayer())
		{
			pPlayer = (CCSPlayer *)target;
		}
	}

	if ( pPlayer && pPlayer->m_flNightVisionAlpha > 0 )
	{
		IMaterial *pMaterial = materials->FindMaterial( "effects/nightvision", TEXTURE_GROUP_CLIENT_EFFECTS, true );

		if ( pMaterial )
		{
			int iMaxValue = 255;
			byte overlaycolor[4] = { 0, 255, 0, 255 };
			
			UpdateScreenEffectTexture( 0, view.x, view.y, view.width, view.height );

			if ( pPlayer->m_bNightVisionOn )
			{
				pPlayer->m_flNightVisionAlpha += 15;

				pPlayer->m_flNightVisionAlpha = std::min<std::common_type_t<float, int>>( pPlayer->m_flNightVisionAlpha, iMaxValue );
			}
			else 
			{
				pPlayer->m_flNightVisionAlpha -= 40;

				pPlayer->m_flNightVisionAlpha = std::max<std::common_type_t<float, int>>( pPlayer->m_flNightVisionAlpha, 0 );
			}

			overlaycolor[3] = pPlayer->m_flNightVisionAlpha;
	
			render->ViewDrawFade( overlaycolor, pMaterial );

			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->DrawScreenSpaceQuad( pMaterial );
			render->ViewDrawFade( overlaycolor, pMaterial );
			pRenderContext->DrawScreenSpaceQuad( pMaterial );
		}
	}
}

//Adrian - Super Nifty Flashbang Effect(tm)
// this does the burn in for the flashbang effect.
void CCSViewRender::PerformFlashbangEffect( const CViewSetup &view )
{
	CCSPlayer *pLocalPlayer = CCSPlayer::GetLocalCSPlayer();
	if ( pLocalPlayer == nullptr )
		 return;

	CCSPlayer *pFlashBangPlayer = pLocalPlayer;
	//bool bReduceEffect = false;

	float flAlphaScale = 1.0f;
	if ( pLocalPlayer->GetObserverMode() != OBS_MODE_NONE )
	{
		// If spectating, use values from target
		CBaseEntity *pTarget = pLocalPlayer->GetObserverTarget();
		if ( pTarget )
		{
			CCSPlayer *pPlayerTmp = ToCSPlayer(pTarget);
			if ( pPlayerTmp )
			{
				pFlashBangPlayer = pPlayerTmp;
			}
		}

		// reduce the effect 
		if ( pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE && CanSeeSpectatorOnlyTools() )
		{
			// Reduce alpha. 
			// Note: the logic to reduce the duration of the effect is done when the effect is started (RecvProxy_FlashTime).
			flAlphaScale = 0.6f;
		}
		else if ( pLocalPlayer->GetObserverMode() == OBS_MODE_FIXED || pLocalPlayer->GetObserverMode() == OBS_MODE_ROAMING )
		{
			// Reduce alpha. 
			// Note: the logic to reduce the duration of the effect is done when the effect is started (RecvProxy_FlashTime).
			flAlphaScale = 0.2f;
		}
		else if ( pLocalPlayer->GetObserverMode() != OBS_MODE_IN_EYE )
		{
			// Reduce alpha. 
			// Note: the logic to reduce the duration of the effect is done when the effect is started (RecvProxy_FlashTime).
			flAlphaScale = 0.6f;
		}
	}

	if ( !pFlashBangPlayer->IsFlashBangActive() || CSGameRules()->IsIntermission() )
	{
		if ( !pLocalPlayer->m_bFlashDspHasBeenCleared )
		{
			CLocalPlayerFilter filter;
			enginesound->SetPlayerDSP( filter, 0, true );
			pLocalPlayer->m_bFlashDspHasBeenCleared = true;
		}
		return;
	}

	// bandaid to insure that flashbang dsp effect doesn't continue on indefinitely
	if( pFlashBangPlayer->GetFlashTimeElapsed() > 1.6f && !pLocalPlayer->m_bFlashDspHasBeenCleared )
	{
		CLocalPlayerFilter filter;
		enginesound->SetPlayerDSP( filter, 0, true );
		pLocalPlayer->m_bFlashDspHasBeenCleared = true;
	}

	byte overlaycolor[4] = { 255, 255, 255, 255 };

	// draw the screenshot overlay portion of the flashbang effect
	IMaterial *pMaterial = materials->FindMaterial( "effects/flashbang", TEXTURE_GROUP_CLIENT_EFFECTS, true );
	if ( pMaterial )
	{
		// This is for handling split screen where we could potentially enter this function more than once a frame.
		// Since this bit of code grabs both the left and right viewports of the buffer, it only needs to be done once per frame per flash.
		static float lastTimeGrabbed = 0.0f;
		if ( gpGlobals->curtime == lastTimeGrabbed )
		{
			pLocalPlayer->m_bFlashScreenshotHasBeenGrabbed = true;
			pFlashBangPlayer->m_bFlashScreenshotHasBeenGrabbed = true;
		}

		if ( !pFlashBangPlayer->m_bFlashScreenshotHasBeenGrabbed )
		{
			CMatRenderContextPtr pRenderContext( materials );
			int nScreenWidth, nScreenHeight;
			pRenderContext->GetRenderTargetDimensions( nScreenWidth, nScreenHeight );

			// update m_pFlashTexture
			lastTimeGrabbed = gpGlobals->curtime;
			bool foundVar;

			IMaterialVar* m_BaseTextureVar = pMaterial->FindVar( "$basetexture", &foundVar, false );
			m_pFlashTexture = GetFullFrameFrameBufferTexture( 1 );

			// When grabbing the texture for the super imposed frame, we grab the whole buffer, not just the viewport.
			// We were having issues with trying to grab only the right side of the buffer for the second player in split screen.
			Rect_t srcRect;
			srcRect.x = 0;
			srcRect.y = 0;
			srcRect.width = nScreenWidth;
			srcRect.height = nScreenHeight;
			m_BaseTextureVar->SetTextureValue( m_pFlashTexture );
			pRenderContext->CopyRenderTargetToTextureEx( m_pFlashTexture, 0, &srcRect, nullptr );
			pRenderContext->SetFrameBufferCopyTexture( m_pFlashTexture );

			pFlashBangPlayer->m_bFlashScreenshotHasBeenGrabbed = true;
			pLocalPlayer->m_bFlashScreenshotHasBeenGrabbed = true;
		}

		if ( !CanSeeSpectatorOnlyTools() )
		{
			overlaycolor[0] = overlaycolor[1] = overlaycolor[2] = pFlashBangPlayer->m_flFlashScreenshotAlpha * flAlphaScale;
			if ( m_pFlashTexture != nullptr )
			{
				static int NUM_AFTER_IMAGE_PASSES = 4;
				for ( int pass = 0; pass < NUM_AFTER_IMAGE_PASSES; ++pass )
				{
					render->ViewDrawFade( overlaycolor, pMaterial, false );
				}
			}
		}
	}

	// draw pure white overlay part of the flashbang effect.
	pMaterial = materials->FindMaterial( "effects/flashbang_white", TEXTURE_GROUP_CLIENT_EFFECTS, true );
	if ( pMaterial )
	{
		overlaycolor[0] = overlaycolor[1] = overlaycolor[2] = pFlashBangPlayer->m_flFlashOverlayAlpha * flAlphaScale;
		render->ViewDrawFade( overlaycolor, pMaterial );
	}
}

	#define cl_smoke_origin_height 68.0f
	#define cl_smoke_torus_ring_radius 61.0f
	#define cl_smoke_torus_ring_subradius 88.0f
	#define cl_smoke_edge_feather 21.0f
	#define cl_smoke_lower_speed 4.5f

//If your eyes are less than this distance from the center of the visible smoke torus, we should check exact distance and may need to apply the screen overlay.
inline constexpr auto cl_smoke_must_be_at_least_this_close_to_have_any_effect = (cl_smoke_torus_ring_radius + cl_smoke_torus_ring_subradius);

void DrawCappedTorus( Vector vecOrigin, float flRadiusA, float flRadiusB, Color colWireColor, float flDuration )
{
	// Draw a debug wireframe 'capped' torus. It's a torus with the middle filled in, like a cheese-boule shape that's meant to match a smoke volume

	Vector vecPointA, vecPointB, vecPointC;
	vecPointA.Init();
	vecPointB.Init();
	vecPointC.Init();

	int i = 0;
	int j = 0;
	bool bTri = true;

	for ( int n=0; n<1643; n++ )
	{
		bTri ? i++ : j++;
		bTri = !bTri;

		float phi = i * 6.28318f / 41.0f;
		float theta = j * 6.28318f / 20.0f;
	
		Vector vecTorusNode;
		vecTorusNode.Init(	std::cos(phi) * ( flRadiusA + cos(theta) * flRadiusB ),
							std::sin(phi) * ( flRadiusA + cos(theta) * flRadiusB ),
							sin(theta) * flRadiusB );

		vecTorusNode += vecOrigin;

		vecPointC = vecPointB;
		vecPointB = vecPointA;
		vecPointA = vecTorusNode;

		if ( cos(theta) < 0 )
		{
			vecPointA.x = vecOrigin.x;
			vecPointA.y = vecOrigin.y;
			vecPointA.z = ( vecPointA.z > vecOrigin.z ) ? ( vecOrigin.z + flRadiusB ) : ( vecOrigin.z - flRadiusB );

			if ( vecPointA.x == vecPointB.x && vecPointA.y == vecPointB.y )
				continue;
		}

		if ( n > 1 )
			debugoverlay->AddLineOverlay( vecPointB, vecPointA, colWireColor.r(), colWireColor.g(), colWireColor.b(), false, flDuration );

		//triangulate
		//if ( n > 2 )
		//{
		//	if ( !bTri )
		//	{
		//		debugoverlay->AddTriangleOverlay( vecPointA, vecPointB, vecPointC, 255, 255, 255, 10, false, flDuration );
		//	}
		//	else
		//	{
		//		debugoverlay->AddTriangleOverlay( vecPointC, vecPointB, vecPointA, 255, 255, 255, 10, false, flDuration );
		//	}
		//}

	}

}

//-----------------------------------------------------------------------------
// Purpose: Renders pre-viewmodel smoke screen
//-----------------------------------------------------------------------------
extern std::vector< EHANDLE > g_SmokeGrenadeHandles;
void CCSViewRender::RenderSmokeOverlay( bool bPreViewModel )
{
	if ( bPreViewModel )
	{
		// update the overlay

		//Assume we have no smoke overlay
		float flOptimalSmokeOverlayAlpha = 0;
	
		if ( !g_SmokeGrenadeHandles.empty() )
		{
			Vector vecPlayerEyePos = MainViewOrigin(GET_ACTIVE_SPLITSCREEN_SLOT());

			Vector vecClosestVecToSmoke;

			int iClosestSmokeIndex = -1;
			float flTempSmokeDistance = cl_smoke_must_be_at_least_this_close_to_have_any_effect;

			// we need to find the closest smoke to prevent a latter grenade in the list lifting the opacity of a potentially closer one
			for( int it=0; it < std::ssize( g_SmokeGrenadeHandles ); it++ )
			{
				CBaseCSGrenadeProjectile *pGrenade = static_cast< CBaseCSGrenadeProjectile* >( g_SmokeGrenadeHandles[it].Get() );
				if ( !pGrenade )
					continue;

				Vector toGrenade = ( pGrenade->GetAbsOrigin() + Vector( 0, 0, cl_smoke_origin_height) ) - vecPlayerEyePos;
				if ( toGrenade.Length() < flTempSmokeDistance )
				{
					//save the new closest smoke
					flTempSmokeDistance = toGrenade.Length();
					iClosestSmokeIndex = it;

					//remember its vector
					vecClosestVecToSmoke = toGrenade;
				}
			}

			//only continue if we actually found a close-enough smoke
			if ( iClosestSmokeIndex != -1 )
			{
				Vector vecSmokePos = Vector( 0, 0, 0 );
				CBaseCSGrenadeProjectile *pGrenade = static_cast< CBaseCSGrenadeProjectile* >( g_SmokeGrenadeHandles[iClosestSmokeIndex].Get() );
				if ( pGrenade )
					vecSmokePos = pGrenade->GetAbsOrigin();

				//draw debug visualization

				//linear interpolation between two capped torusoids

				//are we within the Z axis bounds?
				if ( abs( vecClosestVecToSmoke.z ) < cl_smoke_torus_ring_subradius )
				{

					float flvecClosestVecToSmokeLength2D = vecClosestVecToSmoke.Length2D();

					//are we within the cylindrical cap-space on the XY axis?
					if ( flvecClosestVecToSmokeLength2D < cl_smoke_torus_ring_radius )
					{
						//if so we can just use z delta
						flOptimalSmokeOverlayAlpha = ( cl_smoke_torus_ring_subradius - abs( vecClosestVecToSmoke.z ) ) / cl_smoke_edge_feather;

					}
					else if ( flvecClosestVecToSmokeLength2D < cl_smoke_torus_ring_radius + cl_smoke_torus_ring_subradius )
					{
						//are we within the outer possible horizontal range of the torusoid? if so we need the distance to the nearest point on the primary radius
						Vector vecRingPosOnSmokePlane = Vector( vecClosestVecToSmoke.x, vecClosestVecToSmoke.y, 0 ).Normalized() * cl_smoke_torus_ring_radius;

						//and check the distance value
						float flDistanceToClosestRingPoint = ( vecClosestVecToSmoke - vecRingPosOnSmokePlane ).Length();
						if ( flDistanceToClosestRingPoint < cl_smoke_torus_ring_subradius )
						{
							flOptimalSmokeOverlayAlpha = ( cl_smoke_torus_ring_subradius - flDistanceToClosestRingPoint ) / cl_smoke_edge_feather;
						}
					}

					//clamp to 0-1 range
					flOptimalSmokeOverlayAlpha = clamp( flOptimalSmokeOverlayAlpha, 0.0f, 1.0f );
				}

			}
		}

		//alpha can instantly increase, but decreases to the ideal at a constant rate.
		if ( flOptimalSmokeOverlayAlpha < m_flSmokeOverlayAmount )
		{
			flOptimalSmokeOverlayAlpha = Approach( flOptimalSmokeOverlayAlpha, m_flSmokeOverlayAmount, gpGlobals->frametime * cl_smoke_lower_speed );
		}
		m_flSmokeOverlayAmount = flOptimalSmokeOverlayAlpha;

	}

	if ( m_flSmokeOverlayAmount <= 0 )
	{
		return;
	}
	else
	{
		IMaterial *pMaterial = materials->FindMaterial( "effects/overlaysmoke", TEXTURE_GROUP_CLIENT_EFFECTS, true );
		if ( pMaterial )
		{
			byte overlaycolor[4] = { 90, 90, 90, (byte)( m_flSmokeOverlayAmount * ( bPreViewModel ? 255 : 128 ) ) };
			render->ViewDrawFade( overlaycolor, pMaterial );
		}
	}

}

//-----------------------------------------------------------------------------
// Purpose: Renders extra 2D effects in derived classes while the 2D view is on the stack
//-----------------------------------------------------------------------------
void CCSViewRender::Render2DEffectsPreHUD( const CViewSetup &view )
{
	PerformNightVisionEffect( view );	// this needs to come before the HUD is drawn, or it will wash the HUD out
}

//-----------------------------------------------------------------------------
// Purpose: Renders extra 2D effects in derived classes while the 2D view is on the stack
//-----------------------------------------------------------------------------
void CCSViewRender::Render2DEffectsPostHUD( const CViewSetup &view )
{
	PerformFlashbangEffect( view );

	float flFade = g_HltvReplaySystem.GetReplayVideoFadeAmount();

	// replay camera post-process
	if ( g_HltvReplaySystem.WantsReplayEffect() )
	{
		// demo playback uses full-screen quad to fade in/out. Hltv replay uses the pixel shader here.
		CMatRenderContextPtr pRenderContext( materials );		
		int x, y, w, h;
		pRenderContext->GetViewport( x, y, w, h );

		if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() < 95 ) // bail if ps30 is unsupported
			return;

		UpdateScreenEffectTexture( 0, view.x, view.y, view.width, view.height, false );

		// overlay
		IMaterial *pMatReplay = materials->FindMaterial( "dev/replay", TEXTURE_GROUP_OTHER, true );
		if ( pMatReplay )
		{
			IMaterialVar* pVar = pMatReplay->FindVar( "$c0_x", nullptr );
			pVar->SetFloatValue( 1.0f / w );
	
			pVar = pMatReplay->FindVar( "$c0_y", nullptr );
			pVar->SetFloatValue( 1.0f / h );

			pVar = pMatReplay->FindVar( "$c0_z", nullptr );
			pVar->SetFloatValue( gpGlobals->realtime );

			pVar = pMatReplay->FindVar( "$c0_w", nullptr );
			pVar->SetFloatValue( flFade );
			//if( flFade > 0 && g_HltvReplaySystem.GetHltvReplayDelay() ) Msg( "%.2f dev/replay fade %.2f\n", Plat_FloatTime(), flFade );	   // replayfade

			pRenderContext->DrawScreenSpaceRectangle( pMatReplay, x, y, w, h, 0, 0, w-1, h-1, w, h );
		}
	}
	else if ( flFade > 0 )
	{
		//Msg( "%.2f simple fade %.2f\n", Plat_FloatTime(), flFade ); // replayfade
		float flFadeAdj = flFade;// sinf( flFade * ( M_PI * .5f ) );
		CMatRenderContextPtr pRenderContext( materials );
		int x, y, w, h;
		pRenderContext->GetViewport( x, y, w, h );

		//bars
		if ( IMaterial *pMatFade = materials->FindMaterial( "dev/replay_bars", TEXTURE_GROUP_OTHER, true ) )
		{
			if ( IMaterialVar* pVar = pMatFade->FindVar( "$alpha", nullptr ) )
			{
				pVar->SetFloatValue( flFadeAdj );
				pRenderContext->DrawScreenSpaceRectangle( pMatFade, 0, 0, w, h, 0, 0, 0, 0, 1, 1 );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Renders voice feedback and other sprites attached to players
// Input  : none
//-----------------------------------------------------------------------------
void CCSViewRender::RenderPlayerSprites()
{
	CViewRender::RenderPlayerSprites();
	RadioManager()->DrawHeadLabels();
}

#undef cl_smoke_origin_height
#undef cl_smoke_torus_ring_radius
#undef cl_smoke_torus_ring_subradius
#undef cl_smoke_edge_feather
#undef cl_smoke_lower_speed

class CCSWeaponInfo;
class CEconItemDefinition;
class CEconItemView;
class CWeaponCSBase;
class FileWeaponInfo_t;

using namespace ::ks::reflect;

//--------------------------------------------------------------------------------------------------------
struct WeaponTypeInfo
{
	CSWeaponType type;
	const char * name;
};

//--------------------------------------------------------------------------------------------------------
WeaponTypeInfo s_weaponTypeInfo[] =
{
	{ WEAPONTYPE_KNIFE,			"Knife" },
	{ WEAPONTYPE_PISTOL,		"Pistol" },
	{ WEAPONTYPE_SUBMACHINEGUN, "Submachine Gun" },	// First match is printable
	{ WEAPONTYPE_SUBMACHINEGUN, "submachinegun" },
	{ WEAPONTYPE_SUBMACHINEGUN, "smg" },
	{ WEAPONTYPE_RIFLE,			"Rifle" },
	{ WEAPONTYPE_SHOTGUN,		"Shotgun" },
	{ WEAPONTYPE_SNIPER_RIFLE,	"SniperRifle" },
	{ WEAPONTYPE_MACHINEGUN,	"Machine Gun" },		// First match is printable
	{ WEAPONTYPE_MACHINEGUN,	"machinegun" },
	{ WEAPONTYPE_MACHINEGUN,	"mg" },
	{ WEAPONTYPE_C4,			"C4" },
	{ WEAPONTYPE_GRENADE,		"Grenade" },
	{ WEAPONTYPE_STACKABLEITEM,	"StackableItem" },
};

struct WeaponNameInfo
{
	CSWeaponID id;
	const char *name;
};

// note you have to add an item here if it is a new base item that derives from another base item (like the silenced m4)
WeaponNameInfo s_weaponNameInfo[] =
{
	{ WEAPON_DEAGLE,			"weapon_deagle" },
	{ WEAPON_DEAGLE,			"weapon_revolver" },
	{ WEAPON_ELITE,				"weapon_elite" },
	{ WEAPON_FIVESEVEN,			"weapon_fiveseven" },
	{ WEAPON_FIVESEVEN,			"weapon_cz75a" },
	{ WEAPON_GLOCK,				"weapon_glock" },
	{ WEAPON_P228,				"weapon_p228" },
	{ WEAPON_USP,				"weapon_usp" },

	{ WEAPON_AK47,				"weapon_ak47" },
	{ WEAPON_AUG,				"weapon_aug" },
	{ WEAPON_AWP,				"weapon_awp" },
	{ WEAPON_FAMAS,				"weapon_famas" },
	{ WEAPON_G3SG1,				"weapon_g3sg1" },
	{ WEAPON_GALIL,				"weapon_galil" },
	{ WEAPON_GALILAR,			"weapon_galilar" },
	{ WEAPON_M249,				"weapon_m249" },
	{ WEAPON_M3,				"weapon_m3" },
	{ WEAPON_M4A1,				"weapon_m4a1" },
	{ WEAPON_M4A1,				"weapon_m4a1_silencer" },
	{ WEAPON_MAC10,				"weapon_mac10" },
	{ WEAPON_MP5NAVY,			"weapon_mp5navy" },
	{ WEAPON_P90,				"weapon_p90" },
	{ WEAPON_SCOUT,				"weapon_scout" },
	{ WEAPON_SG550,				"weapon_sg550" },
	{ WEAPON_SG552,				"weapon_sg552" },
	{ WEAPON_TMP,				"weapon_tmp" },
	{ WEAPON_UMP45,				"weapon_ump45" },
	{ WEAPON_XM1014,			"weapon_xm1014" },

	{ WEAPON_BIZON,				"weapon_bizon" },
	{ WEAPON_MAG7,				"weapon_mag7" },
	{ WEAPON_NEGEV,				"weapon_negev" },
	{ WEAPON_SAWEDOFF,			"weapon_sawedoff" },
	{ WEAPON_TEC9,				"weapon_tec9" },
	{ WEAPON_TEC9,				"weapon_cz75a" },
	{ WEAPON_TASER,				"weapon_taser" },

	{ WEAPON_HKP2000,			"weapon_hkp2000" },
	{ WEAPON_MP7,				"weapon_mp7" },
	{ WEAPON_MP9,				"weapon_mp9" },
	{ WEAPON_NOVA,				"weapon_nova" },
	{ WEAPON_P250,				"weapon_p250" },
	{ WEAPON_SCAR17,			"weapon_scar17" },
	{ WEAPON_SCAR20,			"weapon_scar20" },
	{ WEAPON_SG556,				"weapon_sg556" },
	{ WEAPON_SSG08,				"weapon_ssg08" },

	{ WEAPON_KNIFE_GG,			"weapon_knifegg" },
	{ WEAPON_KNIFE,				"weapon_knife" },

	{ WEAPON_HEGRENADE,			"weapon_hegrenade" },
	{ WEAPON_SMOKEGRENADE,		"weapon_smokegrenade" },
	{ WEAPON_FLASHBANG,			"weapon_flashbang" },
	{ WEAPON_MOLOTOV,			"weapon_molotov" },
	{ WEAPON_INCGRENADE,		"weapon_incgrenade" },
	{ WEAPON_DECOY,				"weapon_decoy" },
	{ WEAPON_TAGRENADE,			"weapon_tagrenade" },

	{ WEAPON_C4,				"weapon_c4" },

	{ WEAPON_HEALTHSHOT,		"weapon_healthshot" },

	{ ITEM_KEVLAR,				"item_kevlar" },
	{ ITEM_ASSAULTSUIT,			"item_assaultsuit" },
	{ ITEM_HEAVYASSAULTSUIT,			"item_heavyassaultsuit" },	
	{ ITEM_NVG,					"item_nvg" },
	{ ITEM_DEFUSER,				"item_defuser" },
	{ ITEM_CUTTERS,				"item_cutters" },

	{ WEAPON_NONE,				"weapon_none" },
};

//--------------------------------------------------------------------------------------------------------------

struct EquipmentInfo
{
	const char*		szClassName;
	const char*		szPrintName;
	int				iTeam;
	int				iPrice;
};

EquipmentInfo g_EquipmentInfo[] =
{
	{ "item_assaultsuit",	"#SFUI_WPNHUD_ASSAULTSUIT",		TEAM_UNASSIGNED,	ITEM_PRICE_ASSAULTSUIT },
	{ "item_kevlar",		"#SFUI_WPNHUD_KEVLAR",			TEAM_UNASSIGNED,	ITEM_PRICE_KEVLAR },
	{ "item_heavyassaultsuit",	"#SFUI_WPNHUD_HEAVYASSAULTSUIT",	TEAM_UNASSIGNED,		ITEM_PRICE_HEAVYASSAULTSUIT },
	{ "item_defuser",		"#SFUI_WPNHUD_DEFUSER",			TEAM_CT,			ITEM_PRICE_DEFUSEKIT },
	{ "item_cutters",		"#SFUI_WPNHUD_CUTTERS",			TEAM_CT,			ITEM_PRICE_DEFUSEKIT },
};

void LoadEquipmentData()
{
	for ( int i = 0; i < std::size( g_EquipmentInfo ); ++i )
	{
		WEAPON_FILE_INFO_HANDLE handle = g_WeaponDatabase.FindOrCreateWeaponInfo(g_EquipmentInfo[i].szClassName);
		CCSWeaponInfo* pInfo = dynamic_cast<CCSWeaponInfo*>( g_WeaponDatabase.GetFileWeaponInfoFromHandle( handle ) );
		if ( pInfo )
		{
			pInfo->m_weaponId = WeaponIdFromString( pInfo->szClassName );
			V_strncpy( pInfo->szClassName, g_EquipmentInfo[i].szClassName, MAX_WEAPON_STRING );
			V_strncpy( pInfo->szPrintName, g_EquipmentInfo[i].szPrintName, MAX_WEAPON_STRING );
			pInfo->SetUsedByTeam( g_EquipmentInfo[i].iTeam );
			pInfo->SetWeaponPrice( g_EquipmentInfo[i].iPrice );
			pInfo->SetWeaponType( WEAPONTYPE_EQUIPMENT );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
const CCSWeaponInfo* GetWeaponInfo( CSWeaponID weaponID )
{
	if ( weaponID == WEAPON_NONE )
		return nullptr;

	const char *weaponName = WeaponIdAsString( weaponID );
	WEAPON_FILE_INFO_HANDLE	hWpnInfo = g_WeaponDatabase.FindWeaponInfo( weaponName );
	if ( hWpnInfo == GetInvalidWeaponInfoHandle() )
	{
		return nullptr;
	}

	CCSWeaponInfo *pWeaponInfo = dynamic_cast< CCSWeaponInfo* >( g_WeaponDatabase.GetFileWeaponInfoFromHandle( hWpnInfo ) );

	return pWeaponInfo;
}

//--------------------------------------------------------------------------------------------------------
const char* WeaponClassAsString( CSWeaponType weaponType )
{
	for ( int i = 0; i < std::size( s_weaponTypeInfo ); ++i )
	{
		if ( s_weaponTypeInfo[i].type == weaponType )
		{
			return s_weaponTypeInfo[i].name;
		}
	}

	return nullptr;
}

//--------------------------------------------------------------------------------------------------------
CSWeaponType WeaponClassFromString( const char* weaponType )
{
	for ( int i = 0; i < std::size( s_weaponTypeInfo ); ++i )
	{
		if ( !V_stricmp( s_weaponTypeInfo[i].name, weaponType ) )
		{
			return s_weaponTypeInfo[i].type;
		}
	}

	return WEAPONTYPE_UNKNOWN;
}

//--------------------------------------------------------------------------------------------------------
CSWeaponType WeaponClassFromWeaponID( CSWeaponID weaponID )
{
	const char *weaponStr = WeaponIdAsString( weaponID );
	WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot(weaponStr  );
	if ( hWpnInfo != GetInvalidWeaponInfoHandle() )
	{
		CCSWeaponInfo *pWeaponInfo = dynamic_cast< CCSWeaponInfo* >( GetFileWeaponInfoFromHandle( hWpnInfo ) );
		if ( pWeaponInfo )
		{
			return pWeaponInfo->GetWeaponType();
		}
	}

	return WEAPONTYPE_UNKNOWN;
}

//--------------------------------------------------------------------------------------------------------
const char * WeaponIdAsString( CSWeaponID weaponID )
{
	for ( int i = 0; i < std::size( s_weaponNameInfo ); ++i )
	{
		if ( s_weaponNameInfo[i].id == weaponID )
			return s_weaponNameInfo[i].name;
	}

	return nullptr;
}

//--------------------------------------------------------------------------------------------------------
CSWeaponID WeaponIdFromString( const char *szWeaponName )
{
	for ( int i = 0; i < std::size( s_weaponNameInfo ); ++i )
	{
		if ( V_stricmp( s_weaponNameInfo[i].name, szWeaponName ) == 0 )
			return s_weaponNameInfo[i].id;
	}

	return WEAPON_NONE;
}

//--------------------------------------------------------------------------------------------------------
//
// Given a weapon ID, return its alias
//
const char *WeaponIDToAlias( int id )
{
	for ( int i = 0; i < std::size( s_weaponNameInfo ); ++i )
	{
		if ( s_weaponNameInfo[i].id == id )
			return ( std::strchr( s_weaponNameInfo[i].name, '_' ) + 1 );
	}

	return nullptr;
}

//--------------------------------------------------------------------------------------------------------
//
// Given an alias, return the associated weapon ID
//
CSWeaponID AliasToWeaponID( const char *szAlias )
{
	if ( !szAlias )
		return WEAPON_NONE;

	for ( int i = 0; i < std::size( s_weaponNameInfo ); ++i )
	{
		contract_assert( std::strchr( s_weaponNameInfo[i].name, '_' ) );
		if ( V_stricmp( ( std::strchr( s_weaponNameInfo[i].name, '_' ) + 1 ), szAlias ) == 0 )
			return s_weaponNameInfo[i].id;
	}

	return WEAPON_NONE;
}

bool IsGunWeapon( CSWeaponType weaponType )
{
	switch ( weaponType )
	{
	case WEAPONTYPE_PISTOL:
	case WEAPONTYPE_SUBMACHINEGUN:
	case WEAPONTYPE_RIFLE:
	case WEAPONTYPE_SHOTGUN:
	case WEAPONTYPE_SNIPER_RIFLE:
	case WEAPONTYPE_MACHINEGUN:
		return true;

	default:
		return false;
	}
}

//--------------------------------------------------------------------------------------------------------
void ParseVector( KeyValues *keyValues, const char *keyName, Vector& vec )
{
	vec.x = vec.y = vec.z = 0.0f;

	if ( !keyValues || !keyName )
		return;

	const char *vecString = keyValues->GetString( keyName, "0 0 0" );
	if ( vecString && *vecString )
	{
		float x, y, z;
		if ( 3 == std::sscanf( vecString, "%f %f %f", &x, &y, &z ) )
		{
			vec.x = x;
			vec.y = y;
			vec.z = z;
		}
	}
}

struct EnumerationStringValue
{
	const char* szString;
	int iValue;
};

int ParseEnumeration( KeyValues* pKeyValuesData, const char* szKeyName, const EnumerationStringValue enumStringTable[], int iCount, int iDefaultValue )
{
	const char *pTeam = pKeyValuesData->GetString( "Team", nullptr );
	if ( !pTeam )
		return iDefaultValue;

	for ( int i = 0; i < iCount; ++i )
	{
		if ( V_stricmp( pTeam, enumStringTable[i].szString ) == 0 )
			return enumStringTable[i].iValue;
	}
	
	contract_assert( false );
	return iDefaultValue;
}

FileWeaponInfo_t* CreateWeaponInfo()
{
	return new CCSWeaponInfo;
}

template <typename T>
void ZeroObject( T* p )
{
	std::memset( p, 0x0, sizeof(T) );
}

CCSWeaponInfo::CCSWeaponInfo()
{
	m_weaponId = WEAPON_NONE;
	m_flMaxSpeed[0] = m_flMaxSpeed[1] = 1; // This should always be set in the script.
	m_iZoomLevels = 0;
	ZeroObject(m_iZoomFov);
	ZeroObject(m_fZoomTime);
	m_bHideViewModelZoomed = false;
	m_szZoomINSound[0] = '\0';
	m_szZoomOUTSound[0] = '\0';
	m_WeaponType = WEAPONTYPE_UNKNOWN;
	m_bFullAuto = false;
	m_iTeam = TEAM_UNASSIGNED;
	m_flBotAudibleRange = 0.f;
	m_flArmorRatio = 0.f;
	m_iCrosshairMinDistance = 0;
	m_iCrosshairDeltaDistance = 0;
	m_bCanUseWithShield = false;
	m_WrongTeamMsg[0] = '\0';
	m_szAnimExtension[0] = '\0';
	m_szShieldViewModel[0] = '\0';
	m_szAddonModel[0] = '\0';
	m_szAddonLocation[0] = '\0';
	m_szSilencerModel[0] = '\0';

	m_flAddonScale = 1.f;

	m_fFlinchVelocityModifierLarge = 1.f;
	m_fFlinchVelocityModifierSmall = 1.f;

	m_flPenetration = 0;
	m_iDamage = 0;
	m_flRange = 0.f;
	m_flRangeModifier = 0.f;
	m_iBullets = 0;
	m_flCycleTime = 0.f;
	m_flCycleTimeAlt = 0.f;

	ZeroObject(m_fSpread);
	ZeroObject(m_fInaccuracyCrouch);
	ZeroObject(m_fInaccuracyStand);
	ZeroObject(m_fInaccuracyJump);
	ZeroObject(m_fInaccuracyLand);
	ZeroObject(m_fInaccuracyLadder);
	ZeroObject(m_fInaccuracyImpulseFire);
	ZeroObject(m_fInaccuracyMove);
	m_fRecoveryTimeStand = 0.f;
	m_fRecoveryTimeCrouch = 0.f;
	m_fInaccuracyReload = 0.f;
	m_fInaccuracyAltSwitch = 0.f;
	m_fInaccuracyPitchShift = 0.f;
	m_fInaccuracyAltSoundThreshold = 0.f;
	ZeroObject(m_fRecoilAngle);
	ZeroObject(m_fRecoilAngleVariance);
	ZeroObject(m_fRecoilMagnitude);
	ZeroObject(m_fRecoilMagnitudeVariance);
	m_iRecoilSeed = 0;

	m_flTimeToIdleAfterFire = 0.f;
	m_flIdleInterval = 0.f;

	m_fThrowVelocity = 0.0f;

	m_iKillAward = 0;
	m_iWeaponPrice = 0;

	m_flHeatPerShot = 0.f;
	m_szHeatEffectName[0] = '\0';
	m_vSmokeColor.Zero();

	m_szMuzzleFlashEffectName_1stPerson[0] = '\0';
	m_szMuzzleFlashEffectName_3rdPerson[0] = '\0';
	m_szEjectBrassEffectName[0] = '\0';
	m_szTracerEffectName[0] = '\0';
	m_iTracerFequency = 0;

// recoiltable in csweaponinfo is obsolete. remove this once confirmed that the new implementation generates the same result.
	ZeroObject(m_recoilTable);

	if ( !m_bCSWeaponInfoLookupInitialized )
	{
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_bFullAuto ),					FIELD_BOOLEAN,	"is full auto" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_flHeatPerShot ),				FIELD_FLOAT,	"heat per shot" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_flAddonScale ),					FIELD_FLOAT,	"addon scale" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_iTracerFequency ),				FIELD_INTEGER,	"tracer frequency" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_flMaxSpeed[0] ),				FIELD_FLOAT,	"max player speed" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_flMaxSpeed[1] ),				FIELD_FLOAT,	"max player speed alt" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_iWeaponPrice ),					FIELD_INTEGER,	"in game price" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_flArmorRatio ),					FIELD_FLOAT,	"armor ratio" ) );

		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_iKillAward ),					FIELD_INTEGER,	"kill award" ) );

		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_iCrosshairMinDistance ),		FIELD_INTEGER,	"crosshair min distance" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_iCrosshairDeltaDistance ),		FIELD_INTEGER,	"crosshair delta distance" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_flPenetration ),				FIELD_FLOAT,	"penetration" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_iDamage ),						FIELD_INTEGER,	"damage" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_flRange ),						FIELD_FLOAT,	"range" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_flRangeModifier ),				FIELD_FLOAT,	"range modifier" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_iBullets ),						FIELD_INTEGER,	"bullets" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_flCycleTime ),					FIELD_FLOAT,	"cycletime" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_flCycleTimeAlt ),				FIELD_FLOAT,	"cycletime alt" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_flTimeToIdleAfterFire ),		FIELD_FLOAT,	"time to idle" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_flIdleInterval ),				FIELD_FLOAT,	"idle interval" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fFlinchVelocityModifierLarge ),	FIELD_FLOAT,	"flinch velocity modifier large" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fFlinchVelocityModifierSmall ),	FIELD_FLOAT,	"flinch velocity modifier small" ) );

		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fZoomTime[ 0 ] ),				FIELD_FLOAT,	"zoom time 0" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fZoomTime[ 1 ] ),				FIELD_FLOAT,	"zoom time 1" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fZoomTime[ 2 ] ),				FIELD_FLOAT,	"zoom time 2" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_iZoomFov[1] ),					FIELD_INTEGER,	"zoom fov 1" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_iZoomFov[2] ),					FIELD_INTEGER,	"zoom fov 2" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_bHideViewModelZoomed ),			FIELD_BOOLEAN,	"hide view model zoomed" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_iZoomLevels ),					FIELD_INTEGER,	"zoom levels" ) );

		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fSpread[0] ),					FIELD_FLOAT,	"spread" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fInaccuracyCrouch[0] ),			FIELD_FLOAT,	"inaccuracy crouch" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fInaccuracyStand[0] ),			FIELD_FLOAT,	"inaccuracy stand" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fInaccuracyJump[0] ),			FIELD_FLOAT,	"inaccuracy jump" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fInaccuracyLand[0] ),			FIELD_FLOAT,	"inaccuracy land" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fInaccuracyLadder[0] ),			FIELD_FLOAT,	"inaccuracy ladder" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fInaccuracyImpulseFire[0] ),	FIELD_FLOAT,	"inaccuracy fire" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fInaccuracyMove[0] ),			FIELD_FLOAT,	"inaccuracy move" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fInaccuracyReload ),			FIELD_FLOAT,	"inaccuracy reload" ) );

		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fSpread[1] ),					FIELD_FLOAT,	"spread alt" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fInaccuracyCrouch[1] ),			FIELD_FLOAT,	"inaccuracy crouch alt" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fInaccuracyStand[1] ),			FIELD_FLOAT,	"inaccuracy stand alt" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fInaccuracyJump[1] ),			FIELD_FLOAT,	"inaccuracy jump alt" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fInaccuracyLand[1] ),			FIELD_FLOAT,	"inaccuracy land alt" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fInaccuracyLadder[1] ),			FIELD_FLOAT,	"inaccuracy ladder alt" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fInaccuracyImpulseFire[1] ),	FIELD_FLOAT,	"inaccuracy fire alt" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fInaccuracyMove[1] ),			FIELD_FLOAT,	"inaccuracy move alt" ) );

		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fRecoveryTimeCrouch ),			FIELD_FLOAT,	"recovery time crouch" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fRecoveryTimeStand ),			FIELD_FLOAT,	"recovery time stand" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fRecoveryTimeCrouchFinal ),		FIELD_FLOAT,	"recovery time crouch final" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fRecoveryTimeStandFinal ),		FIELD_FLOAT,	"recovery time stand final" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_iRecoilSeed ),					FIELD_INTEGER,	"recoil seed" ) );

		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fRecoilAngle[0] ),				FIELD_FLOAT,	"recoil angle" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fRecoilAngleVariance[0] ),		FIELD_FLOAT,	"recoil angle variance" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fRecoilMagnitude[0] ),			FIELD_FLOAT,	"recoil magnitude" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fRecoilMagnitudeVariance[0] ),	FIELD_FLOAT,	"recoil magnitude variance" ) );

		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fRecoilAngle[1] ),				FIELD_FLOAT,	"recoil angle alt" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fRecoilAngleVariance[1] ),		FIELD_FLOAT,	"recoil angle variance alt" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fRecoilMagnitude[1] ),			FIELD_FLOAT,	"recoil magnitude alt" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( CCSWeaponInfo, m_fRecoilMagnitudeVariance[1] ),	FIELD_FLOAT,	"recoil magnitude variance alt" ) );

		std::ranges::stable_sort( ms_vecWeaponInfoLookup, CWeaponInfoLookupListLess() );
		m_bCSWeaponInfoLookupInitialized = true;
	}

}

bool CCSWeaponInfo::m_bCSWeaponInfoLookupInitialized;

CSWeaponType CCSWeaponInfo::GetWeaponType( const CEconItemView* ) const
{
	return m_WeaponType;
}

const char* CCSWeaponInfo::GetAddonLocation( const CEconItemView* ) const
{
	return m_szAddonLocation;
}

const char* CCSWeaponInfo::GetEjectBrassEffectName( const CEconItemView* ) const
{
	return m_szEjectBrassEffectName;
}

const char* CCSWeaponInfo::GetTracerEffectName( const CEconItemView* ) const
{
	return m_szTracerEffectName;
}

const char* CCSWeaponInfo::GetMuzzleFlashEffectName_1stPerson( const CEconItemView* ) const
{
	return m_szMuzzleFlashEffectName_1stPerson;
}

const char* CCSWeaponInfo::GetMuzzleFlashEffectName_1stPersonAlt( const CEconItemView* ) const
{
	return m_szMuzzleFlashEffectName_1stPerson;
}

const char* CCSWeaponInfo::GetMuzzleFlashEffectName_3rdPerson( const CEconItemView* ) const
{
	return m_szMuzzleFlashEffectName_3rdPerson;
}

const char* CCSWeaponInfo::GetMuzzleFlashEffectName_3rdPersonAlt( const CEconItemView* ) const
{
	return m_szMuzzleFlashEffectName_3rdPerson;
}

const char* CCSWeaponInfo::GetHeatEffectName( const CEconItemView* ) const
{
	return m_szHeatEffectName;
}

const char* CCSWeaponInfo::GetPlayerAnimationExtension( const CEconItemView* ) const
{
	return m_szAnimExtension;
}

int CCSWeaponInfo::GetUsedByTeam( const CEconItemView* ) const
{
	return m_iTeam;
}

const char* CCSWeaponInfo::GetAddonModel( const CEconItemView* ) const
{
	return m_szAddonModel;
}

const std::vector< WeaponPaintableMaterial_t >* CCSWeaponInfo::GetPaintData( const CEconItemView* ) const
{
	return nullptr;
}

void CCSWeaponInfo::Parse( KeyValues *pKeyValuesData, const char *szWeaponName )
{
	BaseClass::Parse( pKeyValuesData, szWeaponName );

	m_weaponId = WeaponIdFromString( szWeaponName );

	m_flMaxSpeed[0] = ( float )pKeyValuesData->GetInt( "MaxPlayerSpeed", 1 );
	m_flMaxSpeed[1] = ( float )pKeyValuesData->GetInt( "MaxPlayerSpeedAlt", m_flMaxSpeed[0] );

	m_iZoomLevels = pKeyValuesData->GetInt( "ZoomLevels", 0 );

	// sanity check
	contract_assert( m_iZoomLevels < std::size( m_fZoomTime ) && m_iZoomLevels < std::size( m_iZoomFov ) );
	m_iZoomLevels = std::min<std::common_type_t<int, long unsigned int>>( m_iZoomLevels, std::size( m_iZoomFov ) );

	m_iZoomFov[0] = 0;	// not used -- always default FoV
	m_fZoomTime[0] = pKeyValuesData->GetFloat( "ZoomTime0", 0.0f );
	m_iZoomFov[1] = pKeyValuesData->GetInt( "ZoomFov1", 90 );
	m_fZoomTime[1] = pKeyValuesData->GetFloat( "ZoomTime1", 0.0f );
	m_iZoomFov[2] = pKeyValuesData->GetInt( "ZoomFov2", 90 );
	m_fZoomTime[2] = pKeyValuesData->GetFloat( "ZoomTime2", 0.0f );
	m_bHideViewModelZoomed = pKeyValuesData->GetBool( "HideViewModelZoomed", false );
	V_strncpy( m_szZoomINSound, pKeyValuesData->GetString( "ZoomINSound" ), sizeof( m_szZoomINSound ) );
	V_strncpy( m_szZoomOUTSound, pKeyValuesData->GetString( "ZoomOUTSound" ), sizeof( m_szZoomOUTSound ) );

	m_iKillAward = pKeyValuesData->GetInt( "KillAward", 300 );

	m_iWeaponPrice = pKeyValuesData->GetInt( "WeaponPrice", -1 );
	if ( m_iWeaponPrice == -1 )
	{
		// This weapon should have the price in its script.
		contract_assert( false );
	}

	m_flArmorRatio				= pKeyValuesData->GetFloat( "WeaponArmorRatio", 1 );
	m_iCrosshairMinDistance		= pKeyValuesData->GetInt( "CrosshairMinDistance", 4 );
	m_iCrosshairDeltaDistance	= pKeyValuesData->GetInt( "CrosshairDeltaDistance", 3 );
	m_bCanUseWithShield			= !!pKeyValuesData->GetInt( "CanEquipWithShield", false );
	m_flAddonScale				= pKeyValuesData->GetFloat( "AddonScale", 1.0f );
	m_fFlinchVelocityModifierLarge	= pKeyValuesData->GetFloat( "FlinchVelocityModifierLarge", 1.0f );
	m_fFlinchVelocityModifierSmall	= pKeyValuesData->GetFloat( "FlinchVelocityModifierSmall", 1.0f );

	m_flPenetration		= pKeyValuesData->GetFloat( "Penetration", 1.0 );
	m_iDamage			= pKeyValuesData->GetInt( "Damage", 42 ); // Douglas Adams 1952 - 2001
	m_flRange			= pKeyValuesData->GetFloat( "Range", 8192.0f );
	m_flRangeModifier	= pKeyValuesData->GetFloat( "RangeModifier", 0.98f );
	m_iBullets			= pKeyValuesData->GetInt( "Bullets", 1 );
	m_flCycleTime		= pKeyValuesData->GetFloat( "CycleTime", 0.15 );
	m_flCycleTimeAlt	= pKeyValuesData->GetFloat( "CycleTimeAlt", 0.3 );

	// new accuracy model parameters
	m_fSpread[0]				= pKeyValuesData->GetFloat( "Spread", 0.0f );
	m_fInaccuracyCrouch[0]		= pKeyValuesData->GetFloat( "InaccuracyCrouch", 0.0f );
	m_fInaccuracyStand[0]		= pKeyValuesData->GetFloat( "InaccuracyStand", 0.0f );
	m_fInaccuracyJump[0]		= pKeyValuesData->GetFloat( "InaccuracyJump", 0.0f );
	m_fInaccuracyLand[0]		= pKeyValuesData->GetFloat( "InaccuracyLand", 0.0f );
	m_fInaccuracyLadder[0]		= pKeyValuesData->GetFloat( "InaccuracyLadder", 0.0f );
	m_fInaccuracyImpulseFire[0]	= pKeyValuesData->GetFloat( "InaccuracyFire", 0.0f );
	m_fInaccuracyMove[0]		= pKeyValuesData->GetFloat( "InaccuracyMove", 0.0f );

	m_fSpread[1]				= pKeyValuesData->GetFloat( "SpreadAlt",			m_fSpread[0] );
	m_fInaccuracyCrouch[1]		= pKeyValuesData->GetFloat( "InaccuracyCrouchAlt",	m_fInaccuracyCrouch[0] );
	m_fInaccuracyStand[1]		= pKeyValuesData->GetFloat( "InaccuracyStandAlt",	m_fInaccuracyStand[0] );
	m_fInaccuracyJump[1]		= pKeyValuesData->GetFloat( "InaccuracyJumpAlt",	m_fInaccuracyJump[0] );
	m_fInaccuracyLand[1]		= pKeyValuesData->GetFloat( "InaccuracyLandAlt",	m_fInaccuracyLand[0] );
	m_fInaccuracyLadder[1]		= pKeyValuesData->GetFloat( "InaccuracyLadderAlt",	m_fInaccuracyLadder[0] );
	m_fInaccuracyImpulseFire[1]	= pKeyValuesData->GetFloat( "InaccuracyFireAlt",	m_fInaccuracyImpulseFire[0] );
	m_fInaccuracyMove[1]		= pKeyValuesData->GetFloat( "InaccuracyMoveAlt",	m_fInaccuracyMove[0] );

	m_fRecoilAngle[0]				= pKeyValuesData->GetFloat( "RecoilAngle", 0.0f );
	m_fRecoilAngleVariance[0]		= pKeyValuesData->GetFloat( "RecoilAngleVariance", 0.0f );
	m_fRecoilMagnitude[0]			= pKeyValuesData->GetFloat( "RecoilMagnitude", 0.0f );
	m_fRecoilMagnitudeVariance[0]	= pKeyValuesData->GetFloat( "RecoilMagnitudeVariance", 0.0f );
	m_fRecoilAngle[1]				= pKeyValuesData->GetFloat( "RecoilAngleAlt",				m_fRecoilAngle[0] );
	m_fRecoilAngleVariance[1]		= pKeyValuesData->GetFloat( "RecoilAngleVarianceAlt",		m_fRecoilAngleVariance[0] );
	m_fRecoilMagnitude[1]			= pKeyValuesData->GetFloat( "RecoilMagnitudeAlt",			m_fRecoilMagnitude[0] );
	m_fRecoilMagnitudeVariance[1]	= pKeyValuesData->GetFloat( "RecoilMagnitudeVarianceAlt",	m_fRecoilMagnitudeVariance[0] );
	m_iRecoilSeed					= pKeyValuesData->GetInt( "RecoilSeed", 0 );

	// FIXME[pmf]: temp code - remove when weapon scripts have seed values
	if ( m_iRecoilSeed == 0 && IsGunWeapon(GetWeaponType()) )
	{
		// create a temporary seed value based on a hash of the weapon name
		const char *weaponName = szClassName;

		CRC32_t crc;
		CRC32_Init( &crc );
		CRC32_ProcessBuffer( &crc, (void *)weaponName, V_strlen( weaponName )  );
		CRC32_Final( &crc );

		m_iRecoilSeed = (int)crc & 0xFFFF;

	}

	m_fInaccuracyReload			= pKeyValuesData->GetFloat( "InaccuracyReload", 0.0f );
	m_fInaccuracyAltSwitch		= pKeyValuesData->GetFloat( "InaccuracyAltSwitch", 0.0f );

	m_fInaccuracyPitchShift		= pKeyValuesData->GetFloat( "InaccuracyPitchShift", 0.0f );
	m_fInaccuracyAltSoundThreshold = pKeyValuesData->GetFloat( "InaccuracyAltSoundThreshold", 0.0f );

	m_fRecoveryTimeCrouch		= pKeyValuesData->GetFloat( "RecoveryTimeCrouch", 1.0f );
	m_fRecoveryTimeStand		= pKeyValuesData->GetFloat( "RecoveryTimeStand", 1.0f );

	m_fRecoveryTimeCrouchFinal	= pKeyValuesData->GetFloat( "RecoveryTimeCrouchFinal", m_fRecoveryTimeCrouch );
	m_fRecoveryTimeStandFinal	= pKeyValuesData->GetFloat( "RecoveryTimeStandFinal", m_fRecoveryTimeStand );

	m_flTimeToIdleAfterFire	= pKeyValuesData->GetFloat( "TimeToIdle", 2 );
	m_flIdleInterval	= pKeyValuesData->GetFloat( "IdleInterval", 20 );

	// grenade parameters
	m_fThrowVelocity	= pKeyValuesData->GetFloat( "ThrowVelocity", 0.0f );

	m_flHeatPerShot = pKeyValuesData->GetFloat( "HeatPerShot", 0.25f );

	UTIL_StringToVector( m_vSmokeColor.Base(), pKeyValuesData->GetString( "SmokeColor", "1.0 1.0 1.0" ) );

	V_strncpy( m_szHeatEffectName, pKeyValuesData->GetString( "HeatEffect" ), sizeof( m_szHeatEffectName ) );

	// Figure out what team can have this weapon.
	EnumerationStringValue teamEnums[] =
	{
		{ "CT",	TEAM_CT },
		{ "TERRORIST",	TEAM_TERRORIST },
		{ "ANY", TEAM_UNASSIGNED },
	};
	m_iTeam = ParseEnumeration(pKeyValuesData, "Team", teamEnums, std::size( teamEnums ), TEAM_UNASSIGNED );

	const char *pWrongTeamMsg = pKeyValuesData->GetString( "WrongTeamMsg", "" );
	V_strncpy( m_WrongTeamMsg, pWrongTeamMsg, sizeof( m_WrongTeamMsg ) );

	const char *pShieldViewModel = pKeyValuesData->GetString( "shieldviewmodel", "" );
	V_strncpy( m_szShieldViewModel, pShieldViewModel, sizeof( m_szShieldViewModel ) );
	
	const char *pAnimEx = pKeyValuesData->GetString( "PlayerAnimationExtension", "m4" );
	V_strncpy( m_szAnimExtension, pAnimEx, sizeof( m_szAnimExtension ) );

	// Default is 2000.
	m_flBotAudibleRange = pKeyValuesData->GetFloat( "BotAudibleRange", 2000.0f );
	
	const char *pTypeString = pKeyValuesData->GetString( "WeaponType", "" );
	m_WeaponType = WeaponClassFromString( pTypeString );

	m_bFullAuto = pKeyValuesData->GetBool( "FullAuto", false );

	// Read the addon model.
	V_strncpy( m_szAddonModel, pKeyValuesData->GetString( "AddonModel" ), sizeof( m_szAddonModel ) );

	// Read a special addon attachment location if not the default location
	V_strncpy( m_szAddonLocation, pKeyValuesData->GetString( "AddonLocation" ), sizeof( m_szAddonLocation ) );
	
	// Read the silencer model.
	V_strncpy( m_szSilencerModel, pKeyValuesData->GetString( "SilencerModel" ), sizeof( m_szSilencerModel ) );

    // particle muzzle flash effect to play when fired
	V_strncpy( m_szMuzzleFlashEffectName_1stPerson, pKeyValuesData->GetString( "MuzzleFlashEffect_1stPerson" ), sizeof( m_szMuzzleFlashEffectName_1stPerson ) );
	V_strncpy( m_szMuzzleFlashEffectName_3rdPerson, pKeyValuesData->GetString( "MuzzleFlashEffect_3rdPerson" ), sizeof( m_szMuzzleFlashEffectName_3rdPerson ) );

	// particle effect for the shell casing to eject when we fire bullets
	V_strncpy( m_szEjectBrassEffectName, pKeyValuesData->GetString( "EjectBrassEffect" ), sizeof( m_szEjectBrassEffectName ) );

	// gun tracer effect and frequency
	V_strncpy( m_szTracerEffectName, pKeyValuesData->GetString( "TracerEffect" ), sizeof( m_szTracerEffectName ) );
	m_iTracerFequency		= pKeyValuesData->GetInt( "TracerFrequency", 0 );

// recoiltable in csweaponinfo is obsolete. remove this once confirmed that the new implementation generates the same result.
	// generate the recoil table after everything else has been loaded (since it depends on other script parameters)
	GenerateRecoilTable();
}

ConVar weapon_recoil_suppression_shots( side_registry(), "weapon_recoil_suppression_shots", "4", FCVAR_RELEASE | FCVAR_CHEAT |  FCVAR_REPLICATED, "Number of shots before weapon uses full recoil" );
ConVar weapon_recoil_suppression_factor( side_registry(), "weapon_recoil_suppression_factor", "0.75", FCVAR_RELEASE | FCVAR_CHEAT |  FCVAR_REPLICATED, "Initial recoil suppression factor (first suppressed shot will use this factor * standard recoil, lerping to 1 for later shots" );
ConVar weapon_recoil_variance(side_registry(), "weapon_recoil_variance", "0.55", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Amount of variance per recoil impulse", true, 0.0f, true, 1.0f );

// recoiltable in csweaponinfo is obsolete. remove this once confirmed that the new implementation generates the same result.
void CCSWeaponInfo::GenerateRecoilTable()
{
	const int iSuppressionShots = weapon_recoil_suppression_shots.GetInt();
	const float fBaseSuppressionFactor = weapon_recoil_suppression_factor.GetFloat();
	const float fRecoilVariance = weapon_recoil_variance.GetFloat();
	CUniformRandomStream recoilRandom;

	for ( int iMode = 0; iMode < 2; ++iMode )
	{
		recoilRandom.SetSeed( m_iRecoilSeed );

		float fAngle = 0.0f;
		float fMagnitude = 0.0f;

		for ( int j = 0; j < std::size( m_recoilTable[iMode] ); ++j )
		{
			float fAngleNew = m_fRecoilAngle[iMode] + recoilRandom.RandomFloat(-m_fRecoilAngleVariance[iMode], +m_fRecoilAngleVariance[iMode]);
			float fMagnitudeNew = m_fRecoilMagnitude[iMode] + recoilRandom.RandomFloat(-m_fRecoilMagnitudeVariance[iMode], +m_fRecoilMagnitudeVariance[iMode]);

			if ( m_bFullAuto && j > 0 )
			{
				fAngle = Lerp( fRecoilVariance, fAngle, fAngleNew );
				fMagnitude = Lerp( fRecoilVariance, fMagnitude, fMagnitudeNew );
			}
			else
			{
				fAngle = fAngleNew;
				fMagnitude = fMagnitudeNew;
			}

			if ( m_bFullAuto && j < iSuppressionShots )
			{
				float fSuppressionFactor = Lerp( (float)j / (float)iSuppressionShots, fBaseSuppressionFactor, 1.0f );
				fMagnitude *= fSuppressionFactor;
			}

			m_recoilTable[iMode][j].fAngle = fAngle;
			m_recoilTable[iMode][j].fMagnitude = fMagnitude;
		}
	}
}

// recoiltable in csweaponinfo is obsolete. remove this once confirmed that the new implementation generates the same result.
void CCSWeaponInfo::GetRecoilOffsets( int iMode, int iIndex, float& fAngle, float &fMagnitude ) const
{
	iIndex = iIndex % std::size( m_recoilTable[iMode] );
	fAngle = m_recoilTable[iMode][iIndex].fAngle;
	fMagnitude = m_recoilTable[iMode][iIndex].fMagnitude;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Get an attribute associated with the weapon. It might seem a bit odd that in order to get 
// a weapon's damage you need to ask its deprecated weaponinfo object to extract it from its own econitemview
// but we do this because In CS, there are a bunch of cases where there is no weapon instantiated yet and we need the data. All of this
// legacy code uses weaponinfo objects.

// 
// POSSIBLE RETURNS:
//	-2: total failure. We did not find the attribute in the econ item or in the weaponinfo instance.
//	-1: the attribute was found in the econitemview that was passed in.
//	>=0: the returned int is the index into g_WeaponInfoTable that contains the desired data.
//////////////////////////////////////////////////////////////////////////////////////////////////////

static bool GetAttribute_bool( const CCSWeaponInfo* pWeaponInfo, const char * iszAttrib, CSchemaAttributeDefHandle hAttrib, const CEconItemView *pWepView, float flScale = 1.0f )
{
	uint32 unLocalValue = 0;
	int index = -2;

	if ( pWepView && pWepView->IsValid() )
	{
		if ( pWepView->FindAttribute( hAttrib, &unLocalValue ) )
		{
			return ( unLocalValue != 0 );
		}
	}

	// otherwise use legacy weapon script value
	index = pWeaponInfo->GetIndexofAttribute( AllocPooledString( iszAttrib ) );

	if ( index == -1 ) // we found the attribute in the item.
	{
		return false;
	}
	else if ( index > -1 )
	{
		contract_assert( pWeaponInfo->ms_vecWeaponInfoLookup[ index ]->m_fieldType == FIELD_BOOLEAN );
		if ( pWeaponInfo->ms_vecWeaponInfoLookup[ index ]->m_fieldType != FIELD_BOOLEAN )
			return false;

		const bool* pBool;
		pBool = reinterpret_cast< const bool* >( reinterpret_cast< const char* >( pWeaponInfo ) + pWeaponInfo->ms_vecWeaponInfoLookup[ index ]->m_nWeaponParseDataOffset );

		return *pBool;
	}

	return false;
}

static int GetAttribute_int( const CCSWeaponInfo* pWeaponInfo, const char * iszAttrib, CSchemaAttributeDefHandle hAttrib, const CEconItemView *pWepView, float flScale = 1.0f )
{
	uint32 unLocalValue = 0;
	int index = -2;

	if ( pWepView && pWepView->IsValid() )
	{
		if ( pWepView->FindAttribute( hAttrib, &unLocalValue ) )
		{
			return unLocalValue;
		}

		CEconItemDefinition *pDef = pWepView->GetItemDefinition();
		if ( pDef )
		{
			KeyValues *pRawDef = pDef->GetRawDefinition();
			if ( pRawDef )
			{
				KeyValues *pAttribs = pRawDef->FindKey( "attributes" );
				if ( pAttribs )
				{
					const char *pszValue = pAttribs->GetString( iszAttrib, nullptr );
					if ( pszValue )
					{
						return V_atoi( pszValue );
					}
				}
			}
		}
	}

	// otherwise use legacy weapon script value
	index = pWeaponInfo->GetIndexofAttribute( AllocPooledString( iszAttrib ) );

	if ( index == -1 )
	{
		return 0;
	}
	else if ( index > -1 )
	{
		contract_assert( pWeaponInfo->ms_vecWeaponInfoLookup[ index ]->m_fieldType == FIELD_INTEGER );
		if ( pWeaponInfo->ms_vecWeaponInfoLookup[ index ]->m_fieldType != FIELD_INTEGER )
			return false;

		const int* pInt;
		pInt = reinterpret_cast< const int* >( reinterpret_cast< const char* >( pWeaponInfo ) + pWeaponInfo->ms_vecWeaponInfoLookup[ index ]->m_nWeaponParseDataOffset );

		return *pInt;
	}

	return 0;
}

static float GetAttribute_float( const CCSWeaponInfo* pWeaponInfo, const char * iszAttrib, CSchemaAttributeDefHandle hAttrib, const CEconItemView *pWepView, float flScale = 1.0f )
{
	float flLocalValue = 0.0f;
	int index = -2;

	if ( pWepView && pWepView->IsValid() )
	{
		if ( pWepView->FindAttribute( hAttrib, &flLocalValue ) )
		{
			return flScale * flLocalValue;
		}

		CEconItemDefinition *pDef = pWepView->GetItemDefinition();
		if ( pDef )
		{
			KeyValues *pRawDef = pDef->GetRawDefinition();
			if ( pRawDef )
			{
				KeyValues *pAttribs = pRawDef->FindKey( "attributes" );
				if ( pAttribs )
				{
					const char *pszValue = pAttribs->GetString( iszAttrib, nullptr );
					if ( pszValue )
					{
						return flScale * V_atof( pszValue );
					}
				}
			}
		}
	}

	// otherwise use legacy weapon script value
	index = pWeaponInfo->GetIndexofAttribute( AllocPooledString( iszAttrib ) );

	if ( index == -1 )
	{
		return flLocalValue;
	}
	else if ( index > -1 )
	{
		contract_assert( pWeaponInfo->ms_vecWeaponInfoLookup[ index ]->m_fieldType == FIELD_FLOAT );
		if ( pWeaponInfo->ms_vecWeaponInfoLookup[ index ]->m_fieldType != FIELD_FLOAT )
			return false;

		const float* pFloat;
		pFloat = reinterpret_cast< const float* >( reinterpret_cast< const char* >( pWeaponInfo ) + pWeaponInfo->ms_vecWeaponInfoLookup[ index ]->m_nWeaponParseDataOffset );

		return flScale * *pFloat;
	}

	return 0;
}

// template < class T >
// T GetAttributeFromSchemaOrScript( const CCSWeaponInfo* pWeaponInfo, const char * iszAttrib, CSchemaAttributeDefHandle hAttrib, const CEconItemView *pWepView )
// {
// 	uint32 unLocalValue = 0;
// 	int index = -2;
// 
// 	if ( pWepView && pWepView->IsValid() )
// 	{
// 		if ( pWepView->FindAttribute( hAttrib, &unLocalValue ) )
// 		{
// 			return unLocalValue;
// 		}
// 	}
// 
// 	// otherwise use legacy weapon script value
// 	index = pWeaponInfo->GetIndexofAttribute( AllocPooledString( iszAttrib ) );
// 
// 	if ( index == -1 )
// 	{
// 		return false;
// 	}
// 	else if ( index > -1 )
// 	{
// 		const T* pValue;
// 		pValue = reinterpret_cast< const T* >( reinterpret_cast< const char* >( pWeaponInfo ) + pWeaponInfo->ms_vecWeaponInfoLookup[ index ]->m_nWeaponParseDataOffset );
// 
// 		return *pValue;
// 	}
// 
// 	return false;
// }

// Weapon attribute accessor is macroized and templatized so that we can use static attribute def handles 
// because generating them at every access is costly.
//
#define GET_WEAPON_ATTR_FUNC( type, functionname, attrname )										\
type CCSWeaponInfo::functionname( const CEconItemView* pWepView, int nAlt, float flScale ) const	\
{																									\
	const char* pszAttrib;																			\
	static CSchemaAttributeDefHandle hAttrib( attrname );											\
	static CSchemaAttributeDefHandle hAttribAlt( attrname " alt" );									\
	CSchemaAttributeDefHandle * pAttrib;															\
	if ( nAlt )																						\
	{																								\
		pszAttrib = attrname " alt";																\
		pAttrib = &hAttribAlt;																		\
	}																								\
	else																							\
	{																								\
		pszAttrib = attrname;																		\
		pAttrib = &hAttrib;																			\
	}																								\
	return GetAttribute_##type( this, pszAttrib, *pAttrib, pWepView, flScale );						\
}		

GET_WEAPON_ATTR_FUNC( int,		GetWeaponPrice,					"in game price" )
GET_WEAPON_ATTR_FUNC( bool,		IsFullAuto,						"is full auto" )
GET_WEAPON_ATTR_FUNC( bool,		HasSilencer,					"has silencer" )
GET_WEAPON_ATTR_FUNC( int,		GetBullets,						"bullets" )
GET_WEAPON_ATTR_FUNC( float,	GetCycleTime,					"cycletime" )
GET_WEAPON_ATTR_FUNC( float,	GetHeatPerShot,					"heat per shot" )
GET_WEAPON_ATTR_FUNC( float,	GetRecoveryTimeCrouch,			"recovery time crouch" )
GET_WEAPON_ATTR_FUNC( float,	GetRecoveryTimeStand,			"recovery time stand" )
GET_WEAPON_ATTR_FUNC( float,	GetRecoveryTimeCrouchFinal,		"recovery time crouch final" )
GET_WEAPON_ATTR_FUNC( float,	GetRecoveryTimeStandFinal,		"recovery time stand final" )
GET_WEAPON_ATTR_FUNC( int,		GetRecoveryTransitionStartBullet, "recovery transition start bullet" )
GET_WEAPON_ATTR_FUNC( int,		GetRecoveryTransitionEndBullet, "recovery transition end bullet" )

GET_WEAPON_ATTR_FUNC( int,		GetRecoilSeed,					"recoil seed" )
GET_WEAPON_ATTR_FUNC( float,	GetFlinchVelocityModifierLarge, "flinch velocity modifier large" )
GET_WEAPON_ATTR_FUNC( float,	GetFlinchVelocityModifierSmall, "flinch velocity modifier small" )
GET_WEAPON_ATTR_FUNC( float,	GetTimeToIdleAfterFire,			"time to idle" )
GET_WEAPON_ATTR_FUNC( float,	GetIdleInterval,				"idle interval" )
GET_WEAPON_ATTR_FUNC( float,	GetRange,						"range" )
GET_WEAPON_ATTR_FUNC( float,	GetRangeModifier,				"range modifier" )
GET_WEAPON_ATTR_FUNC( int,		GetDamage,						"damage" )
GET_WEAPON_ATTR_FUNC( float,	GetPenetration,					"penetration" )
GET_WEAPON_ATTR_FUNC( int,		GetCrosshairDeltaDistance,		"crosshair delta distance" )
GET_WEAPON_ATTR_FUNC( int,		GetCrosshairMinDistance,		"crosshair min distance" )

GET_WEAPON_ATTR_FUNC( float,	GetMaxSpeed,					"max player speed" )

GET_WEAPON_ATTR_FUNC( float,	GetSpread,						"spread" )
GET_WEAPON_ATTR_FUNC( float,	GetInaccuracyCrouch,			"inaccuracy crouch" )
GET_WEAPON_ATTR_FUNC( float,	GetInaccuracyStand,				"inaccuracy stand" )
GET_WEAPON_ATTR_FUNC( float,	GetInaccuracyJumpInitial,		"inaccuracy jump initial" )
GET_WEAPON_ATTR_FUNC( float,	GetInaccuracyJump,				"inaccuracy jump" )
GET_WEAPON_ATTR_FUNC( float,	GetInaccuracyLand,				"inaccuracy land" )
GET_WEAPON_ATTR_FUNC( float,	GetInaccuracyLadder,			"inaccuracy ladder" )
GET_WEAPON_ATTR_FUNC( float,	GetInaccuracyFire,				"inaccuracy fire" )
GET_WEAPON_ATTR_FUNC( float,	GetInaccuracyMove,				"inaccuracy move" )

GET_WEAPON_ATTR_FUNC( float,	GetInaccuracyReload,			"inaccuracy reload" )

GET_WEAPON_ATTR_FUNC( float,	GetRecoilAngle,					"recoil angle" )
GET_WEAPON_ATTR_FUNC( float,	GetRecoilAngleVariance,			"recoil angle variance" )
GET_WEAPON_ATTR_FUNC( float,	GetRecoilMagnitude,				"recoil magnitude" )
GET_WEAPON_ATTR_FUNC( float,	GetRecoilMagnitudeVariance,		"recoil magnitude variance" )
GET_WEAPON_ATTR_FUNC( int,		GetTracerFrequency,				"tracer frequency" )

GET_WEAPON_ATTR_FUNC( int,		GetPrimaryClipSize,				"primary clip size" )
GET_WEAPON_ATTR_FUNC( int,		GetSecondaryClipSize,			"secondary clip size" )
GET_WEAPON_ATTR_FUNC( int,		GetDefaultPrimaryClipSize,		"primary default clip size" )
GET_WEAPON_ATTR_FUNC( int,		GetDefaultSecondaryClipSize,	"secondary default clip size" )

GET_WEAPON_ATTR_FUNC( int,		GetKillAward,					"kill award" )
GET_WEAPON_ATTR_FUNC( bool,		HasBurstMode,					"has burst mode" )
//lwss - add some more of these that are missing
GET_WEAPON_ATTR_FUNC( float,    GetBurstModeTimeBetweenShots,   "time between burst shots")
GET_WEAPON_ATTR_FUNC( float,    GetBurstModeCycleTime,          "cycletime when in burst mode")
GET_WEAPON_ATTR_FUNC( float,    GetZoomedCycleTime,                      "cycletime when zoomed")
//lwss end
GET_WEAPON_ATTR_FUNC( bool,		IsRevolver,						"is revolver" )
GET_WEAPON_ATTR_FUNC( bool,		HasAlternateFastSlowReload,		"alternate fastslow reload" )
GET_WEAPON_ATTR_FUNC( float,	GetArmorRatio,					"armor ratio" )
GET_WEAPON_ATTR_FUNC( bool,		HasTraditionalScope,			"traditional scope" )
GET_WEAPON_ATTR_FUNC( bool,		CannotShootUnderwater,			"cannot shoot underwater" )
GET_WEAPON_ATTR_FUNC( bool,		DoesUnzoomAfterShot,			"unzoom after shot" )
GET_WEAPON_ATTR_FUNC( bool,		DoesHideViewModelWhenZoomed,	"hide view model zoomed" )
GET_WEAPON_ATTR_FUNC( int,		GetBucketSlot,					"bucket slot" )
GET_WEAPON_ATTR_FUNC( int,		GetZoomLevels,					"zoom levels" )
GET_WEAPON_ATTR_FUNC( int,		GetZoomFOV1,					"zoom fov 1" )
GET_WEAPON_ATTR_FUNC( int,		GetZoomFOV2,					"zoom fov 2" )
GET_WEAPON_ATTR_FUNC( float,	GetZoomTime0,					"zoom time 0" )
GET_WEAPON_ATTR_FUNC( float,	GetZoomTime1,					"zoom time 1" )
GET_WEAPON_ATTR_FUNC( float,	GetZoomTime2,					"zoom time 2" )

GET_WEAPON_ATTR_FUNC( int,		GetPrimaryReserveAmmoMax,		"primary reserve ammo max" )
GET_WEAPON_ATTR_FUNC( int,		GetSecondaryReserveAmmoMax,		"secondary reserve ammo max" )

WeaponRecoilData::WeaponRecoilData()
{
}

WeaponRecoilData::~WeaponRecoilData()
{
}

static inline float AttrValueAsFloat( attrib_value_t val )
{
	float flValue;
	V_memcpy( &flValue, &val, sizeof( float ) );
	return flValue;
}

void WeaponRecoilData::GenerateRecoilTable( RecoilData *data )
{
	const int iSuppressionShots = weapon_recoil_suppression_shots.GetInt();
	const float fBaseSuppressionFactor = weapon_recoil_suppression_factor.GetFloat();
	const float fRecoilVariance = weapon_recoil_variance.GetFloat();
	CUniformRandomStream recoilRandom;

	if ( !data )
		return;

	int iSeed = 0;
	bool bFullAuto = false;
	float flRecoilAngle[2] = {};
	float flRecoilAngleVariance[2] = {};
	float flRecoilMagnitude[2] = {};
	float flRecoilMagnitudeVariance[2] = {};
	CCSWeaponInfo const *pWeaponInfo = nullptr;

	if ( ( data->iItemDefIndex >= WEAPON_FIRST ) && ( data->iItemDefIndex <= WEAPON_LAST ) )
	{
		pWeaponInfo = GetWeaponInfo( CSWeaponID( data->iItemDefIndex ) );
	}
	if ( pWeaponInfo )
	{
		iSeed = pWeaponInfo->GetRecoilSeed();
		bFullAuto = pWeaponInfo->IsFullAuto();
		for ( int iMode = 0; iMode < 2; ++ iMode )
		{
			flRecoilAngle[iMode] = pWeaponInfo->GetRecoilAngle( nullptr, iMode );
			flRecoilAngleVariance[iMode] = pWeaponInfo->GetRecoilAngleVariance( nullptr, iMode );
			flRecoilMagnitude[iMode] = pWeaponInfo->GetRecoilMagnitude( nullptr, iMode );
			flRecoilMagnitudeVariance[iMode] = pWeaponInfo->GetRecoilMagnitudeVariance( nullptr, iMode );
		}
	}
	else
	{
		return; // No weapon info, cannot generate recoil table
	}

	for ( int iMode = 0; iMode < 2; ++iMode )
	{
		recoilRandom.SetSeed( iSeed );

		float fAngle = 0.0f;
		float fMagnitude = 0.0f;

		for ( int j = 0; j < std::size( data->recoilTable[iMode] ); ++j )
		{
			float fAngleNew = flRecoilAngle[iMode] + recoilRandom.RandomFloat(- flRecoilAngleVariance[iMode], + flRecoilAngleVariance[iMode] );
			float fMagnitudeNew = flRecoilMagnitude[iMode] + recoilRandom.RandomFloat(- flRecoilMagnitudeVariance[iMode], + flRecoilMagnitudeVariance[iMode] );

			if ( bFullAuto && ( j > 0 ) )
			{
				fAngle = Lerp( fRecoilVariance, fAngle, fAngleNew );
				fMagnitude = Lerp( fRecoilVariance, fMagnitude, fMagnitudeNew );
			}
			else
			{
				fAngle = fAngleNew;
				fMagnitude = fMagnitudeNew;
			}

			if ( bFullAuto && ( j < iSuppressionShots ) )
			{
				float fSuppressionFactor = Lerp( (float)j / (float)iSuppressionShots, fBaseSuppressionFactor, 1.0f );
				fMagnitude *= fSuppressionFactor;
			}

			data->recoilTable[iMode][j].fAngle = fAngle;
			data->recoilTable[iMode][j].fMagnitude = fMagnitude;
		}
	}
}

void WeaponRecoilData::GetRecoilOffsets( CWeaponCSBase *pWeapon, int iMode, int iIndex, float& fAngle, float &fMagnitude )
{
	// Recoil offset tables are indexed by a weapon's definition index.
	// Look for the existing table, otherwise generate it.

	item_definition_index_t iDefIndex = (item_definition_index_t)pWeapon->GetCSWeaponID();

	RecoilData *wepData = nullptr;
	auto iMapLocation = m_mapRecoilTables.find( iDefIndex );
	if ( iMapLocation == m_mapRecoilTables.end() )
	{
		contract_assert( !"Generating recoil table too late" ); // failed to find recoil table, need to re-generate!
		wepData = m_mapRecoilTables.insert_or_assign( iDefIndex, std::make_unique< RecoilData >() ).first->second.get();
		wepData->iItemDefIndex = iDefIndex;
		GenerateRecoilTable( wepData );
	}
	else
	{
		wepData = iMapLocation->second.get();
		contract_assert( wepData );
		contract_assert( wepData->iItemDefIndex == iDefIndex );
	}

	iIndex = iIndex % std::size( wepData->recoilTable[iMode] );
	fAngle = wepData->recoilTable[iMode][iIndex].fAngle;
	fMagnitude = wepData->recoilTable[iMode][iIndex].fMagnitude;
}

void WeaponRecoilData::GenerateRecoilPatternForItemDefinition( item_definition_index_t idx )
{
	auto [iMapLocation, bInserted] = m_mapRecoilTables.try_emplace( idx );
	if ( bInserted )
	{
		iMapLocation->second = std::make_unique< RecoilData >();
		RecoilData *wepData = iMapLocation->second.get();
		wepData->iItemDefIndex = idx;
		GenerateRecoilTable( wepData );
	}
}

WeaponRecoilData g_WeaponRecoilData;

void GenerateWeaponRecoilPatternForItemDefinition( item_definition_index_t idx )
{
	g_WeaponRecoilData.GenerateRecoilPatternForItemDefinition( idx );
}

#undef GET_WEAPON_ATTR_FUNC

class CCSGOPlayerAnimState;
class CAnimationLayer;
class CBaseAnimating;
class CBaseAnimatingOverlay;
class CBaseWeaponWorldModel;
class CCSPlayer;
class CWeaponCSBase;

using namespace ::ks::reflect;

float ClampCycle( float flCycleIn )
{
	flCycleIn -= int(flCycleIn);

	if ( flCycleIn < 0 )
	{
		flCycleIn += 1;
	}
	else if ( flCycleIn > 1 )
	{
		flCycleIn -= 1;
	}

	return flCycleIn;
}

CCSGOPlayerAnimState *CreateCSGOPlayerAnimstate( CBaseAnimatingOverlay *pEntity )
{
	CCSPlayer *pPlayer = ToCSPlayer( pEntity );
	contract_assert( pPlayer );

	CCSGOPlayerAnimState *pRet = new CCSGOPlayerAnimState( pPlayer );
	return pRet;
}

static constexpr auto CURRENT_ANIMSTATE_VERSION = 2;
CCSGOPlayerAnimState::CCSGOPlayerAnimState( CCSPlayer *pPlayer )
{
	m_pPlayer = pPlayer;
	contract_assert( m_pPlayer );

	m_cachedModelIndex = -1;
	m_nAnimstateModelVersion = CURRENT_ANIMSTATE_VERSION;
	Reset();
}

static constexpr auto CSGO_ANIM_AIMMATRIX_DEFAULT_YAW_MAX = 58.0f;
static constexpr auto CSGO_ANIM_AIMMATRIX_DEFAULT_YAW_MIN = -58.0f;
static constexpr auto CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MAX = 90.0f;
static constexpr auto CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MIN = -90.0f;

void CCSGOPlayerAnimState::Reset( void )
{
	contract_assert( m_pPlayer );

	ApplyLayerOrderPreset( get_animlayerpreset( Default ), true );

	m_iLastUpdateFrame					= 0;
	m_flStepHeightLeft					= 0;
	m_flStepHeightRight					= 0;

	m_pWeapon							= m_pPlayer->GetActiveCSWeapon();
	m_pWeaponLast = m_pWeapon;

	m_pWeaponLastBoneSetup = m_pWeapon;
	m_flEyePositionSmoothLerp			= 0;
	m_flStrafeChangeWeightSmoothFalloff = 0;
	m_bFirstFootPlantSinceInit			= true;

	m_flLastUpdateTime					= 0;
	m_flLastUpdateIncrement				= 0;

	m_flEyeYaw							= 0;
	m_flEyePitch						= 0;
	m_flFootYaw							= 0;
	m_flFootYawLast						= 0;
	m_flMoveYaw							= 0;
	m_flMoveYawIdeal					= 0;
	m_flMoveYawCurrentToIdeal			= 0;

	m_tStandWalkAim.Init();
	m_tStandWalkAim.m_flHowLongToWaitUntilTransitionCanBlendIn = 0.4f;
	m_tStandWalkAim.m_flHowLongToWaitUntilTransitionCanBlendOut = 0.2f;
	m_tStandRunAim.Init();
	m_tStandRunAim.m_flHowLongToWaitUntilTransitionCanBlendIn = 0.2f;
	m_tStandRunAim.m_flHowLongToWaitUntilTransitionCanBlendOut = 0.4f;
	m_tCrouchWalkAim.Init();
	m_tCrouchWalkAim.m_flHowLongToWaitUntilTransitionCanBlendIn = 0.3f;
	m_tCrouchWalkAim.m_flHowLongToWaitUntilTransitionCanBlendOut = 0.3f;

	m_flPrimaryCycle					= 0;
	m_flMoveWeight						= 0;
	m_flMoveWeightSmoothed				= 0;
	m_flAnimDuckAmount					= 0;
	m_flDuckAdditional					= 0; // for when we duck a bit after landing from a jump
	m_flRecrouchWeight					= 0;

	m_vecPositionCurrent.Init();
	m_vecPositionLast.Init();

	m_vecVelocity.Init();
	m_vecVelocityNormalized.Init();
	m_vecVelocityNormalizedNonZero.Init();
	m_flVelocityLengthXY				= 0;
	m_flVelocityLengthZ					= 0;

	m_flSpeedAsPortionOfRunTopSpeed		= 0;
	m_flSpeedAsPortionOfWalkTopSpeed	= 0;
	m_flSpeedAsPortionOfCrouchTopSpeed	= 0;

	m_flDurationMoving					= 0;
	m_flDurationStill					= 0;

	m_bOnGround							= true;
	m_flLandAnimMultiplier				= 1.0f;
	m_flLeftGroundHeight				= 0;
	m_bLanding							= false;
	m_flJumpToFall						= 0;
	m_flDurationInAir					= 0;

	m_flWalkToRunTransition				= 0;

	m_bLandedOnGroundThisFrame			= false;
	m_bLeftTheGroundThisFrame			= false;
	m_flInAirSmoothValue				= 0;

	m_bOnLadder							= false;
	m_flLadderWeight					= 0;
	m_flLadderSpeed						= 0;

	m_bWalkToRunTransitionState			= ANIM_TRANSITION_WALK_TO_RUN;

	m_bDefuseStarted					= false;
	m_bPlantAnimStarted					= false;
	m_bTwitchAnimStarted				= false;
	m_bAdjustStarted					= false;

	m_flNextTwitchTime					= 0;

	m_flTimeOfLastKnownInjury			= 0;

	m_flLastVelocityTestTime			= 0;
	m_vecVelocityLast.Init();
	m_vecTargetAcceleration.Init();
	m_vecAcceleration.Init();
	m_flAccelerationWeight				= 0;

	m_flAimMatrixTransition				= 0;
	m_flAimMatrixTransitionDelay		= 0;

	m_bFlashed							= 0;

	m_flStrafeChangeWeight				= 0;
	m_flStrafeChangeTargetWeight		= 0;
	m_flStrafeChangeCycle				= 0;
	m_nStrafeSequence					= -1;
	m_bStrafeChanging					= false;
	m_flDurationStrafing				= 0;

	m_flFootLerp						= 0;

	m_bFeetCrossed						= false;

	m_bPlayerIsAccelerating				= false;

	m_flDurationMoveWeightIsTooHigh		= 0;
	m_flStaticApproachSpeed				= 80;

	m_flStutterStep						= 0;
	m_nPreviousMoveState				= 0;

	m_flActionWeightBiasRemainder		= 0;

	m_flAimYawMin						= CSGO_ANIM_AIMMATRIX_DEFAULT_YAW_MIN;
	m_flAimYawMax						= CSGO_ANIM_AIMMATRIX_DEFAULT_YAW_MAX;
	m_flAimPitchMin						= CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MIN;
	m_flAimPitchMax						= CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MAX;

	//m_flMoveWalkWeight					= 0;
	//m_flMoveCrouchWalkWeight			= 0;
	//m_flMoveRunWeight					= 0;

	m_ActivityModifiers.clear(); m_ActivityModifiers.shrink_to_fit();

	m_bFirstRunSinceInit				= true;

	m_flCameraSmoothHeight				= 0;
	m_bSmoothHeightValid				= false;
	m_flLastTimeVelocityOverTen			= 0;

	m_pPlayer->ClearAnimLODflags();
}

#define bonesnapshot_def( _name, _val ) const float _name = _val;
template <typename T1> [[gnu::always_inline]] inline constexpr auto bonesnapshot_get( T1 _name ) { return _name; }

bonesnapshot_def( cl_bonesnapshot_speed_weaponchange,	0.25 )
bonesnapshot_def( cl_bonesnapshot_speed_strafestart,	0.15 )
bonesnapshot_def( cl_bonesnapshot_speed_movebegin,		0.3 )
bonesnapshot_def( cl_bonesnapshot_speed_ladderenter,	0.25 )
bonesnapshot_def( cl_bonesnapshot_speed_ladderexit,		0.1 )

static constexpr auto CSGO_ANIM_DEPLOY_RATELIMIT = 0.15f;

static constexpr auto CSGO_ANIM_DUCK_APPROACH_SPEED_DOWN = 3.1f;
static constexpr auto CSGO_ANIM_DUCK_APPROACH_SPEED_UP = 6.0f;

void CCSGOPlayerAnimState::Update( float eyeYaw, float eyePitch, bool bForce )
{
	if ( !m_pPlayer )
		return;

	if ( !m_pPlayer->IsAlive() )
		return;

	if ( !CacheSequences() )
		return;

	{
		// Apply recoil angle to aim matrix so bullets still come out of the gun straight while spraying
		eyePitch = AngleNormalize( eyePitch + m_pPlayer->m_flThirdpersonRecoil );
	}

	// don't need to update animstate if we already have this curtime
	if ( !bForce && ( m_flLastUpdateTime == gpGlobals->curtime || m_nLastUpdateFrame == gpGlobals->framecount ) )
		return;
	m_flLastUpdateIncrement = Max( 0.0f, gpGlobals->curtime - m_flLastUpdateTime ); // negative values possible when clocks on client and server go out of sync..

	// suspend bonecache invalidation
	CBaseAnimating::EnableInvalidateBoneCache( false );

	m_flEyeYaw = AngleNormalize( eyeYaw );
	m_flEyePitch = AngleNormalize( eyePitch );
	m_vecPositionCurrent = m_pPlayer->GetAbsOrigin();
	m_pWeapon = m_pPlayer->GetActiveCSWeapon();
	
	// purge layer dispatches on weapon change and init
	if ( m_pWeapon != m_pWeaponLast || m_bFirstRunSinceInit )
	{

		// changing weapons will change the pose of leafy bones like fingers. The next time we
		// set up this player's bones, treat it like a clean first setup.
		m_pPlayer->m_nComputedLODframe = 0;

		for ( int i=0; i < ANIMATION_LAYER_COUNT; i++ )
		{
			CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( i, USE_ANIMLAYER_RAW_INDEX );
			if ( pLayer )
			{
				pLayer->m_pDispatchedStudioHdr = nullptr;
				pLayer->m_nDispatchedSrc = ACT_INVALID;
				pLayer->m_nDispatchedDst = ACT_INVALID;
			}
		}
	}

	if ( IsPreCrouchUpdateDemo() )
	{
		// compatibility for old demos using old crouch values
		float flTargetDuck = (m_pPlayer->GetFlags() & ( FL_ANIMDUCKING )) ? 1.0f : m_flDuckAdditional;
		m_flAnimDuckAmount = Approach( flTargetDuck, m_flAnimDuckAmount, m_flLastUpdateIncrement * ( (m_flAnimDuckAmount < flTargetDuck) ? CSGO_ANIM_DUCK_APPROACH_SPEED_DOWN : CSGO_ANIM_DUCK_APPROACH_SPEED_UP )  );
		m_flAnimDuckAmount = clamp( m_flAnimDuckAmount, 0, 1 );
	}
	else
	{
		m_flAnimDuckAmount = clamp( Approach( clamp( m_pPlayer->m_flDuckAmount + m_flDuckAdditional, 0, 1), m_flAnimDuckAmount, m_flLastUpdateIncrement * 6.0f ), 0, 1 );
	}

	// no matter what, we're always playing 'default' underneath
	{
		MDLCACHE_CRITICAL_SECTION();
		m_pPlayer->SetSequence( 0 );
		m_pPlayer->SetPlaybackRate( 0 );
		m_pPlayer->SetCycle( 0 );
	}

	// all the layers get set up here
	SetUpVelocity();			// calculate speed and set up body yaw values
	SetUpAimMatrix();			// aim matrices are full body, so they not only point the weapon down the eye dir, they can crouch the idle player
	SetUpWeaponAction();		// firing, reloading, silencer-swapping, deploying
	SetUpMovement();			// jumping, climbing, ground locomotion, post-weapon crouch-stand
	SetUpAliveloop();			// breathe and fidget
	SetUpWholeBodyAction();		// defusing, planting, whole-body custom events
	SetUpFlashedReaction();		// shield eyes from flashbang
	SetUpFlinch();				// flinch when shot
	SetUpLean();				// lean into acceleration

	// zero-sequences are un-set and should have zero weight on the client
	for ( int i=0; i < ANIMATION_LAYER_COUNT; i++ )
	{
		CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( i, USE_ANIMLAYER_RAW_INDEX );
		if ( pLayer && pLayer->GetSequence() == 0 )
			pLayer->SetWeight(0);
	}

	// force abs angles on client and server to line up hitboxes
	m_pPlayer->SetAbsAngles( QAngle( 0, m_flFootYaw, 0 ) );

	// restore bonecache invalidation
	CBaseAnimating::EnableInvalidateBoneCache( true );

	m_pWeaponLast = m_pWeapon;
	m_vecPositionLast = m_vecPositionCurrent;
	m_bFirstRunSinceInit = false;
	m_flLastUpdateTime = gpGlobals->curtime;
	m_nLastUpdateFrame = gpGlobals->framecount;
}

float CCSGOPlayerAnimState::LerpCrouchWalkRun( float flCrouchVal, float flWalkVal, float flRunVal )
{
	// lerp between three separate magic numbers intended for each state
	return Lerp( m_flAnimDuckAmount, Lerp( m_flWalkToRunTransition, flWalkVal, flRunVal ), flCrouchVal );
}

static constexpr auto HYPEREXTENSION_LIMIT = 34.2; // The length of a CS player's leg (hip to ankle) when nearly fully extended
inline constexpr auto HYPEREXTENSION_LIMIT_SQR = HYPEREXTENSION_LIMIT * HYPEREXTENSION_LIMIT;
static constexpr auto FOOT_SAFE_ZONE_LEFT_SQR = 7 * 7;
static constexpr auto FOOT_SAFE_ZONE_RIGHT_SQR = 8 * 8;
static constexpr auto ANKLE_HEIGHT = 4.5f;
static constexpr auto FOOT_PROXIMITY_LIMIT_SQR = 12 * 12; // don't let feet come to rest this close together
static constexpr auto LATERAL_BLENDOUT_HEIGHT = 1;
static constexpr auto LATERAL_BLENDIN_TIME = 20;
static constexpr auto LATERAL_BLENDOUT_TIME = 6;

static constexpr auto FIRSTPERSON_TO_THIRDPERSON_VERTICAL_TOLERANCE_MIN = 4.0f;
static constexpr auto FIRSTPERSON_TO_THIRDPERSON_VERTICAL_TOLERANCE_MAX = 10.0f;
extern ConVar cl_camera_height_restriction_debug;
void CCSGOPlayerAnimState::ModifyEyePosition( Vector& vecInputEyePos )
{
	if ( !m_pPlayer )
		return;
	
	if ( IsPreCrouchUpdateDemo() )
		return;

	// The local player sets up their third-person bones to locate the position of their head,
	// then this position is used to softly bound the vertical camera position for the client.

	if ( !m_bLanding && m_flAnimDuckAmount == 0 )
	{
		m_bSmoothHeightValid = false;
		return;
	}

	int nHeadBone = m_pPlayer->LookupBone( "head_0" );
	if ( nHeadBone != -1 )
	{
		Vector vecHeadPos;
		QAngle temp;
		m_pPlayer->GetBonePosition( nHeadBone, vecHeadPos, temp );
		vecHeadPos.z += 1.7f;

		if ( cl_camera_height_restriction_debug.GetBool() )
		{
			Vector vecTemp = Vector( vecInputEyePos.x, vecInputEyePos.y, vecHeadPos.z + FIRSTPERSON_TO_THIRDPERSON_VERTICAL_TOLERANCE_MAX );
			debugoverlay->AddLineOverlay( vecTemp, vecTemp + Vector(0,20,0), 255, 0, 255, true, gpGlobals->frametime );
			vecTemp = Vector( vecInputEyePos.x, vecInputEyePos.y, vecHeadPos.z + FIRSTPERSON_TO_THIRDPERSON_VERTICAL_TOLERANCE_MIN );
			debugoverlay->AddLineOverlay( vecTemp, vecTemp + Vector(0,20,0), 0, 0, 255, true, gpGlobals->frametime );
			vecTemp = Vector( vecInputEyePos.x, vecInputEyePos.y, vecInputEyePos.z );
			debugoverlay->AddLineOverlay( vecTemp, vecTemp + Vector(0,20,0), 255, 0, 0, true, gpGlobals->frametime );
			//vecTemp = Vector( vecInputEyePos.x, vecInputEyePos.y, vecHeadPos.z - FIRSTPERSON_TO_THIRDPERSON_VERTICAL_TOLERANCE_MIN );
			//debugoverlay->AddLineOverlay( vecTemp, vecTemp + Vector(0,20,0), 0, 0, 255, true, gpGlobals->frametime );
			//vecTemp = Vector( vecInputEyePos.x, vecInputEyePos.y, vecHeadPos.z - FIRSTPERSON_TO_THIRDPERSON_VERTICAL_TOLERANCE_MAX );
			//debugoverlay->AddLineOverlay( vecTemp, vecTemp + Vector(0,20,0), 255, 0, 255, true, gpGlobals->frametime );
		}

		// Only correct the eye if the camera is ABOVE the head. If the camera is below the head, that's unlikely
		// to be advantageous for the local player.
		if ( vecHeadPos.z < vecInputEyePos.z )
		{
			
			float flLerp = SimpleSplineRemapValClamped( abs( vecInputEyePos.z - vecHeadPos.z ),
				FIRSTPERSON_TO_THIRDPERSON_VERTICAL_TOLERANCE_MIN,
				FIRSTPERSON_TO_THIRDPERSON_VERTICAL_TOLERANCE_MAX,
				0.0f, 1.0f );

			vecInputEyePos.z = Lerp( flLerp, vecInputEyePos.z, vecHeadPos.z );

		}

		if ( cl_camera_height_restriction_debug.GetBool() )
		{
			Vector vecTemp = Vector( vecInputEyePos.x, vecInputEyePos.y, vecInputEyePos.z );
			debugoverlay->AddLineOverlay( vecTemp, vecTemp + Vector(0,20,0), 0, 255, 0, true, gpGlobals->frametime );
		}

		// when fully crouched on the client, floor the camera height under the input z within a vertical range so it doesn't 'bob' continuously
		if ( m_flAnimDuckAmount >= 1 )
		{
			if ( m_bSmoothHeightValid )
			{
				float flHardCamHeight = m_pPlayer->GetAbsOrigin().z + VEC_DUCK_VIEW.z;

				if ( vecInputEyePos.z < flHardCamHeight )
				{
					m_flCameraSmoothHeight = clamp( std::min( m_flCameraSmoothHeight, vecInputEyePos.z ), vecInputEyePos.z - clamp( flHardCamHeight - vecInputEyePos.z, 0.0f, 3.0f ), vecInputEyePos.z );
					vecInputEyePos.z = m_flCameraSmoothHeight;
				}
				else
				{
					m_flCameraSmoothHeight = flHardCamHeight;
				}
		
				if ( cl_camera_height_restriction_debug.GetBool() )
				{
					Vector vecTemp = Vector( vecInputEyePos.x, vecInputEyePos.y, vecInputEyePos.z );
					debugoverlay->AddLineOverlay( vecTemp, vecTemp + Vector(0,30,0), 255, 255, 255, true, gpGlobals->frametime );
				}
		
			}
			else
			{
				m_flCameraSmoothHeight = vecInputEyePos.z;
				m_bSmoothHeightValid = true;
			}
		}
		else
		{
			m_bSmoothHeightValid = false;
		}

	}
}

void CCSGOPlayerAnimState::OnClientWeaponChange( CWeaponCSBase* pCurrentWeapon )
{
	if ( !m_bFirstRunSinceInit && ( pCurrentWeapon != m_pWeaponLastBoneSetup ) &&
		!m_pPlayer->IsAnyBoneSnapshotPending() &&
		m_pPlayer->m_boneSnapshots[ BONESNAPSHOT_UPPER_BODY ].GetCurrentWeight() <= 0 && 
		m_pPlayer->m_boneSnapshots[ BONESNAPSHOT_ENTIRE_BODY ].GetCurrentWeight() <= 0 )
	{
		m_pWeaponLastBoneSetup = pCurrentWeapon;
		if ( m_flSpeedAsPortionOfWalkTopSpeed > 0.25f )
		{
			m_pPlayer->m_boneSnapshots[ BONESNAPSHOT_UPPER_BODY ].SetShouldCapture( bonesnapshot_get( cl_bonesnapshot_speed_weaponchange ) );
		}
		else
		{
			m_pPlayer->m_boneSnapshots[ BONESNAPSHOT_ENTIRE_BODY ].SetShouldCapture( bonesnapshot_get( cl_bonesnapshot_speed_weaponchange ) );
		}
	}
}

bool CCSGOPlayerAnimState::LayerToIndex( const CAnimationLayer* pLayer, int &nIndex )
{
	for ( int i=0; i < ANIMATION_LAYER_COUNT; i++ )
	{
		if ( pLayer == m_pPlayer->GetAnimOverlay( m_pLayerOrderPreset[i], USE_ANIMLAYER_RAW_INDEX ) )
		{
			nIndex = i;
			return true;
		}
	}
	return false;
}

void CCSGOPlayerAnimState::NotifyOnLayerChangeSequence( const CAnimationLayer* pLayer, const int nNewSequence )
{
	int nIndex;
	if ( !LayerToIndex( pLayer, nIndex ) )
		return;
	animstate_layer_t nLayerIndex = (animstate_layer_t)nIndex;
	
	//todo: more hooks for pre-bonesetup sequence changes

	// bone snapshot land/climb transitions
	if ( !m_bFirstRunSinceInit && nLayerIndex == ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB )
	{
		int nLastSequence = pLayer->GetSequence();
		if ( nLastSequence > 0 && nLastSequence != nNewSequence )
		{
			if ( m_pPlayer->GetSequenceActivity( nLastSequence ) == ACT_CSGO_CLIMB_LADDER )
			{
				m_pPlayer->m_boneSnapshots[BONESNAPSHOT_ENTIRE_BODY].SetShouldCapture( bonesnapshot_get( cl_bonesnapshot_speed_ladderexit ) );
			}
			else if ( m_pPlayer->GetSequenceActivity( nNewSequence ) == ACT_CSGO_CLIMB_LADDER )
			{
				m_pPlayer->m_boneSnapshots[BONESNAPSHOT_ENTIRE_BODY].SetShouldCapture( bonesnapshot_get( cl_bonesnapshot_speed_ladderenter ) );
			}
		}
	}

}

void CCSGOPlayerAnimState::NotifyOnLayerChangeWeight( const CAnimationLayer* pLayer, const float flNewWeight )
{
	int nIndex;
	if ( !LayerToIndex( pLayer, nIndex ) )
		return;
	animstate_layer_t nLayerIndex = (animstate_layer_t)nIndex;

	//todo: more hooks for pre-bonesetup sequence changes

	if ( nLayerIndex == ANIMATION_LAYER_MOVEMENT_MOVE )
	{
		m_flMoveWeight = flNewWeight;
	}
}

void CCSGOPlayerAnimState::NotifyOnLayerChangeCycle( const CAnimationLayer* pLayer, const float flNewcycle )
{
	int nIndex;
	if ( !LayerToIndex( pLayer, nIndex ) )
		return;
	animstate_layer_t nLayerIndex = (animstate_layer_t)nIndex;

	//todo: more hooks for pre-bonesetup sequence changes
	
	if ( nLayerIndex == ANIMATION_LAYER_MOVEMENT_MOVE )
	{
		m_flPrimaryCycle = flNewcycle;
	}
}

float CCSGOPlayerAnimState::FootBarrierEq( float flIn, float flMinWidth )
{
	return (( Sqr(flIn) * 0.02f ) + std::min<std::common_type_t<float, int>>( flMinWidth, 3 )) * std::min<std::common_type_t<float, int>>( m_flSpeedAsPortionOfCrouchTopSpeed, 1 );
}

void CCSGOPlayerAnimState::DoProceduralFootPlant( matrix3x4a_t boneToWorld[], mstudioikchain_t *pLeftFootChain, mstudioikchain_t *pRightFootChain, BoneVector pos[] )
{
	if ( !m_pPlayer )
		return;
	
	if ( m_bOnGround && m_flInAirSmoothValue >= 1 && !m_bOnLadder )
	{

		if ( m_pPlayer->GetGroundEntity() && !m_pPlayer->GetGroundEntity()->IsWorld() )
			return;

		int nLeftFootBoneIndex = pLeftFootChain->pLink( 2 )->bone;
		int nRightFootBoneIndex = pRightFootChain->pLink( 2 )->bone;

		int nLeftLockIndex = m_pPlayer->LookupBone( "lfoot_lock" );
		int nRightLockIndex = m_pPlayer->LookupBone( "rfoot_lock" );

		if ( nLeftLockIndex > 0 && nRightLockIndex > 0 )
		{
			m_footLeft.m_vecPosAnim = boneToWorld[nLeftFootBoneIndex].GetOrigin();
			m_footRight.m_vecPosAnim = boneToWorld[nRightFootBoneIndex].GetOrigin();
			
			//debugoverlay->AddBoxOverlay( m_footLeft.m_vecPosAnim, Vector(-5,-5,0), Vector(5,5,0), QAngle(0,0,0), 255, 0, 0, 0, 0 );
			//debugoverlay->AddBoxOverlay( m_footRight.m_vecPosAnim, Vector(-5,-5,0), Vector(5,5,0), QAngle(0,0,0), 255, 0, 0, 0, 0 );

			Vector vecPlayerOrigin = m_pPlayer->GetAbsOrigin();
			
			// reset the procedural locations if this is the first computation or they haven't been computed recently
			if ( m_bFirstFootPlantSinceInit || abs(gpGlobals->framecount - m_iLastUpdateFrame) > 10 )
			{
				m_footLeft.Init( m_footLeft.m_vecPosAnim );
				m_footRight.Init( m_footRight.m_vecPosAnim );

				m_footLeft.m_vecPlantVel = m_vecVelocityNormalizedNonZero;
				m_footRight.m_vecPlantVel = m_vecVelocityNormalizedNonZero;

				m_iLastUpdateFrame = gpGlobals->framecount;
				m_bFirstFootPlantSinceInit = false;
				return;
			}
			
			// use the ik lock driver bones to determine if a new foot plant location has been triggered

			#define FOOT_LOCK_THRESHOLD 0.7f
			
			bool bLeftFootLockWasBelowThreshold = m_footLeft.m_flLockAmount < FOOT_LOCK_THRESHOLD;
			bool bRightFootLockWasBelowThreshold = m_footRight.m_flLockAmount < FOOT_LOCK_THRESHOLD;
			
			m_footLeft.m_flLockAmount = Approach( clamp( abs(pos[nLeftLockIndex].y), 0, 1 ), m_footLeft.m_flLockAmount, gpGlobals->frametime * 10.0f );
			m_footRight.m_flLockAmount = Approach( clamp( abs(pos[nRightLockIndex].y), 0, 1 ), m_footRight.m_flLockAmount, gpGlobals->frametime * 10.0f );
			
			bool bLeftFootLockIsAboveThreshold = m_footLeft.m_flLockAmount >= FOOT_LOCK_THRESHOLD;
			bool bRightFootLockIsAboveThreshold = m_footRight.m_flLockAmount >= FOOT_LOCK_THRESHOLD;
			
			// for a short duration after each foot-plant, the feet are not allowed to deviate laterally from the player velocity at the time of the plant

			float flLeftTimeLerp = RemapValClamped( m_footLeft.m_flLastPlantTime, gpGlobals->curtime, gpGlobals->curtime-0.4f, 1.0f, 0.0f );
			float flRightTimeLerp = RemapValClamped( m_footRight.m_flLastPlantTime, gpGlobals->curtime, gpGlobals->curtime-0.4f, 1.0f, 0.0f );
			
			flLeftTimeLerp = Gain( flLeftTimeLerp, 0.8f );
			flRightTimeLerp = Gain( flRightTimeLerp, 0.8f );
			
			//debugoverlay->AddBoxOverlay( m_footLeft.m_vecPosPlant, Vector(-5,-5,0)*flLeftTimeLerp, Vector(5,5,0)*flLeftTimeLerp, QAngle(0,0,0), 0, 255, 255, 0, 0 );
			//debugoverlay->AddBoxOverlay( m_footRight.m_vecPosPlant, Vector(-5,-5,0)*flRightTimeLerp, Vector(5,5,0)*flRightTimeLerp, QAngle(0,0,0), 255, 255, 0, 0, 0 );
			
			Vector vecLeftPtOnVelLine;
			Vector vecRightPtOnVelLine;

			CalcClosestPointOnLine( m_footLeft.m_vecPosAnim, m_footLeft.m_vecPosPlant, m_footLeft.m_vecPosPlant + m_footLeft.m_vecPlantVel, vecLeftPtOnVelLine );
			CalcClosestPointOnLine( m_footRight.m_vecPosAnim, m_footRight.m_vecPosPlant, m_footRight.m_vecPosPlant + m_footRight.m_vecPlantVel, vecRightPtOnVelLine );
			
			Vector vecLeftTarget = Lerp( flLeftTimeLerp, m_footLeft.m_vecPosAnim, vecLeftPtOnVelLine );
			Vector vecRightTarget = Lerp( flRightTimeLerp, m_footRight.m_vecPosAnim, vecRightPtOnVelLine );
			
			// check for foot plants for next frame

			if ( bLeftFootLockIsAboveThreshold && bLeftFootLockWasBelowThreshold )
			{
				m_footLeft.m_vecPosPlant = vecLeftTarget;
				m_footLeft.m_flLastPlantTime = gpGlobals->curtime;
				m_footLeft.m_vecPlantVel = m_vecVelocityNormalizedNonZero;
			}
			
			if ( bRightFootLockIsAboveThreshold && bRightFootLockWasBelowThreshold )
			{
				m_footRight.m_vecPosPlant = vecRightTarget;
				m_footRight.m_flLastPlantTime = gpGlobals->curtime;
				m_footRight.m_vecPlantVel = m_vecVelocityNormalizedNonZero;
			}

			// always inherit animated z (only foot-stepping below modifies this)
			vecLeftTarget.z = m_footLeft.m_vecPosAnim.z;
			vecRightTarget.z = m_footRight.m_vecPosAnim.z;

			// the feet are not allowed to exceed 3x the instantaneous velocity of the player (prevents pose-popping when mashing keys)

			float flFrametime = std::max( gpGlobals->frametime, 0.0001f );
			float flLeftDeltaVel = std::max( ( m_footLeft.m_vecPosAnimLast - vecLeftTarget ).Length() / flFrametime, 0.0001f );
			float flRightDeltaVel = std::max( ( m_footRight.m_vecPosAnimLast - vecRightTarget ).Length() / flFrametime, 0.0001f );

			float flVelLimitLeft = m_flVelocityLengthXY * 3.0f;
			float flVelLimitRight = m_flVelocityLengthXY * 3.0f;

			if ( m_flVelocityLengthXY > 10.0f )
			{
				m_flLastTimeVelocityOverTen = gpGlobals->curtime;
			}
			float flTimeStill = gpGlobals->curtime - m_flLastTimeVelocityOverTen;

			// when standing mostly still, alternately raise the velocity limit floor, which makes the feet 'shuffle-step'
			if ( m_flVelocityLengthXY <= 10.0f )
			{

				float flEyeFootAngleDiff = abs( AngleDiff(m_flEyeYaw, m_flFootYaw) );
				if ( flEyeFootAngleDiff > 56.0f || flTimeStill < 1.0f ) // when turning rapidly, allow the feet to step faster
				{
					float flFmod = std::fmod( gpGlobals->curtime, 0.33f );
					if ( flFmod < 0.16f )
					{
						flVelLimitLeft = 110.0f;
					}
					else if ( flFmod >= 0.17f )
					{
						flVelLimitRight = 130.0f;
					}
				}
				else
				{
					float flFmod = std::fmod( gpGlobals->curtime, 0.66f );
					if ( flFmod > 0.02 && flFmod < 0.31f )
					{
						flVelLimitLeft = 80.0f;
					}
					else if ( flFmod > 0.35 && flFmod < 0.64f )
					{
						flVelLimitRight = 70.0f;
					}
				}

				// less fudge-factor when crouching, so allow the feet to catch up faster
				if ( m_flAnimDuckAmount > 0.5f )
				{
					flVelLimitLeft *= 2.0f;
					flVelLimitRight *= 2.0f;
				}

				// also lift the feet a bit on their way to their target
				//if ( flVelLimitLeft > 0 )
				//{
				//	float flDistToLeft = RemapValClamped( m_footLeft.m_vecPosAnimLast.DistTo( m_footLeft.m_vecPosAnim ), 0.0f, 30.0f, 0.0f, 1.0f );
				//	vecLeftTarget.z += SmoothCurve( flDistToLeft ) * 14.0f;
				//}
				//if ( flVelLimitRight> 0 )
				//{
				//	float flDistToRight = RemapValClamped( m_footRight.m_vecPosAnimLast.DistTo( m_footRight.m_vecPosAnim ), 0.0f, 30.0f, 0.0f, 1.0f );
				//	vecRightTarget.z += SmoothCurve( flDistToRight ) * 14.0f;
				//}

			}

			// restrict foot velocity

			if ( flLeftDeltaVel > flVelLimitLeft )
				vecLeftTarget = Lerp( flVelLimitLeft / flLeftDeltaVel, m_footLeft.m_vecPosAnimLast, vecLeftTarget );

			if ( flRightDeltaVel > flVelLimitRight )
				vecRightTarget = Lerp( flVelLimitRight / flRightDeltaVel, m_footRight.m_vecPosAnimLast, vecRightTarget );

			// spawn oddities like lowering the player artificially without allowing them to fall can move the player
			// but it doesn't count as a teleport, so their velocity remains zero... long story short this causes the 
			// target z values to catch up instead of reset and it looks weird. I'm clamping their range here:
			vecLeftTarget.z = clamp( vecLeftTarget.z, m_footLeft.m_vecPosAnim.z - 2.0f, m_footLeft.m_vecPosAnim.z + 6.0f );
			vecRightTarget.z = clamp( vecRightTarget.z, m_footRight.m_vecPosAnim.z - 2.0f, m_footRight.m_vecPosAnim.z + 6.0f );

			// sanity-check the result and throw out the positions if they're super weird
			// (we might have been teleported or pvs went nuts or there was a bunch of packet loss - 
			// point being it's easier and more reliable to error check than try to prevent the input cases.

			int nLeftHipBoneIndex = pLeftFootChain->pLink( 0 )->bone;
			int nRightHipBoneIndex = pRightFootChain->pLink( 0 )->bone;
			if ( nLeftHipBoneIndex > 0 && nRightHipBoneIndex > 0 )
			{
				if ( boneToWorld[nLeftHipBoneIndex].GetOrigin().DistToSqr( vecLeftTarget ) > 1400 ||
					 boneToWorld[nRightHipBoneIndex].GetOrigin().DistToSqr( vecRightTarget ) > 1400 )
				{

					//debugoverlay->AddLineOverlay( boneToWorld[nLeftHipBoneIndex].GetOrigin(), vecLeftTarget, 255,0,0, true, 5 );
					//debugoverlay->AddLineOverlay( boneToWorld[nRightHipBoneIndex].GetOrigin(), vecRightTarget, 255,0,0, true, 5 );

					// if either foot is way out of range, bail and reset them both
					m_footLeft.Init( m_footLeft.m_vecPosAnim );
					m_footRight.Init( m_footRight.m_vecPosAnim );

					m_footLeft.m_vecPlantVel = m_vecVelocityNormalizedNonZero;
					m_footRight.m_vecPlantVel = m_vecVelocityNormalizedNonZero;

					m_iLastUpdateFrame = gpGlobals->framecount;
					m_bFirstFootPlantSinceInit = false;
					return;
				}
			}

			if ( vecLeftTarget.DistToSqr( m_footLeft.m_vecPosAnim ) > (12 * 12) )
			{
				vecLeftTarget = m_footLeft.m_vecPosAnim + (( vecLeftTarget - m_footLeft.m_vecPosAnim ).Normalized() * 12.0f);
				//debugoverlay->AddLineOverlay( vecLeftTarget, m_footLeft.m_vecPosAnim, 0, 255, 0, true, 0 );
			}

			if ( vecRightTarget.DistToSqr( m_footRight.m_vecPosAnim ) > (12 * 12) )
			{
				vecRightTarget = m_footRight.m_vecPosAnim + ((vecRightTarget - m_footRight.m_vecPosAnim).Normalized() * 12.0f);
				//debugoverlay->AddLineOverlay( vecRightTarget, m_footRight.m_vecPosAnim, 0, 255, 0, true, 0 );
			}

			// place the foot bones at the newly computed locations (ik will solve the knees)

			boneToWorld[nLeftFootBoneIndex].SetOrigin( vecLeftTarget );
			boneToWorld[nRightFootBoneIndex].SetOrigin( vecRightTarget );
			
			//debugoverlay->AddBoxOverlay( vecLeftTarget, Vector(-5,-5,0), Vector(5,5,0), QAngle(0,0,0), 0, 255, 0, 0, 0 );
			//debugoverlay->AddBoxOverlay(vecRightTarget, Vector(-5,-5,0), Vector(5,5,0), QAngle(0,0,0), 0, 255, 0, 0, 0 );

			m_footLeft.m_vecPosAnimLast = vecLeftTarget;
			m_footRight.m_vecPosAnimLast = vecRightTarget;

		}

		if ( m_iLastUpdateFrame < gpGlobals->framecount )
		{
			m_iLastUpdateFrame = gpGlobals->framecount;
		}
	}	
}

void CCSGOPlayerAnimState::SetUpLean( void )
{
	// lean the body into velocity derivative (acceleration) to simulate maintaining a center of gravity
	float flInterval = gpGlobals->curtime - m_flLastVelocityTestTime;
	if ( flInterval > 0.025f )
	{
		flInterval = std::min( flInterval, 0.1f );
		m_flLastVelocityTestTime = gpGlobals->curtime;

		m_vecTargetAcceleration = ( m_pPlayer->GetLocalVelocity() - m_vecVelocityLast ) / flInterval;
		m_vecTargetAcceleration.z = 0;

		m_vecVelocityLast = m_pPlayer->GetLocalVelocity();
	}

	m_vecAcceleration = Approach( m_vecTargetAcceleration, m_vecAcceleration, m_flLastUpdateIncrement * 800.0f );

//#ifdef CLIENT_DLL
//	debugoverlay->AddLineOverlay( m_vecPositionCurrent, m_vecPositionCurrent + m_vecAcceleration, 255,0,0, 255, 1, m_flLastUpdateIncrement );
//#else
//	debugoverlay->AddLineOverlay( m_vecPositionCurrent, m_vecPositionCurrent + m_vecAcceleration, 0,0,255, 255, 1.5, m_flLastUpdateIncrement );
//#endif

	QAngle temp;
	VectorAngles( m_vecAcceleration, Vector(0,0,1), temp );
	
	m_flAccelerationWeight = clamp( (m_vecAcceleration.Length() / CS_PLAYER_SPEED_RUN) * m_flSpeedAsPortionOfRunTopSpeed, 0, 1 );
	m_flAccelerationWeight *= (1.0f - m_flLadderWeight);

	m_tPoseParamMappings[ PLAYER_POSE_PARAM_LEAN_YAW ].SetValue( m_pPlayer, AngleNormalize( m_flFootYaw - temp[YAW] ) );

	if ( GetLayerSequence( ANIMATION_LAYER_LEAN ) <= 0 )
	{
		MDLCACHE_CRITICAL_SECTION();
		SetLayerSequence( ANIMATION_LAYER_LEAN, m_pPlayer->LookupSequence( "lean" ) );
	}

	SetLayerWeight( ANIMATION_LAYER_LEAN, m_flAccelerationWeight );
}

void CCSGOPlayerAnimState::SetUpFlinch( void )
{

	IncrementLayerCycle( ANIMATION_LAYER_FLINCH, false );
}

void CCSGOPlayerAnimState::SetUpFlashedReaction( void )
{
	animstate_layer_t nLayer = ANIMATION_LAYER_FLASHED;

	if ( GetLayerWeight( nLayer ) > 0 )
	{
		CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayer, USE_ANIMLAYER_RAW_INDEX );
		if ( pLayer && pLayer->GetWeightDeltaRate() < 0 )
			IncrementLayerWeight( nLayer );
	}

}

static constexpr auto CSGO_ANIM_BOMBPLANT_ABORT_SPEED = 12.0f;
static constexpr auto CSGO_ANIM_DEFUSE_ABORT_SPEED = 8.0f;
static constexpr auto CSGO_ANIM_TWITCH_ABORT_SPEED = 6.0f;
static constexpr auto CSGO_ANIM_BOMBPLANT_BLEND_RATE = 1.2f;
void CCSGOPlayerAnimState::SetUpWholeBodyAction( void )
{
	animstate_layer_t nLayer = ANIMATION_LAYER_WHOLE_BODY;

	if ( GetLayerWeight( nLayer ) > 0 )
	{
		IncrementLayerCycle( nLayer, false );
		IncrementLayerWeight( nLayer );
	}

}

void CCSGOPlayerAnimState::SetUpAliveloop( void )
{
	animstate_layer_t nLayer = ANIMATION_LAYER_ALIVELOOP;

	IncrementLayerCycle( nLayer, true );
	//SetLayerWeight( nLayer, 1 );
}

void CCSGOPlayerAnimState::SetUpWeaponAction( void )
{
	animstate_layer_t nLayer = ANIMATION_LAYER_WEAPON_ACTION;
	
	// client

	if ( GetLayerWeight( nLayer ) > 0 )
	{
		IncrementLayerCycle( nLayer, false );
		IncrementLayerWeight( nLayer );
	}

	// set weapon sequence and cycle so dispatched events hit
	CAnimationLayer *pWeaponLayer = m_pPlayer->GetAnimOverlay( nLayer, USE_ANIMLAYER_RAW_INDEX );
	if ( pWeaponLayer && m_pWeapon )
	{
		CBaseWeaponWorldModel *pWeaponWorldModel = m_pWeapon->m_hWeaponWorldModel.Get();
		if ( pWeaponWorldModel )
		{
			MDLCACHE_CRITICAL_SECTION();
			if ( pWeaponLayer->m_nDispatchedDst > 0 && pWeaponLayer->GetWeight() > 0 ) // fixme: does the weight check make 0-frame events fail? Added a check below just in case.
			{
				pWeaponWorldModel->SetSequence( pWeaponLayer->m_nDispatchedDst );
				pWeaponWorldModel->SetCycle( pWeaponLayer->GetCycle() );
				pWeaponWorldModel->SetPlaybackRate( pWeaponLayer->GetPlaybackRate() );
			}
			else
			{
				pWeaponWorldModel->SetSequence( 0 );
				pWeaponWorldModel->SetCycle( 0 );
				pWeaponWorldModel->SetPlaybackRate( 0 );
			}
		}
	}
}

static constexpr auto CSGO_ANIM_WALK_TO_RUN_TRANSITION_SPEED = 2.0f;
static constexpr auto CSGO_ANIM_ONGROUND_FUZZY_APPROACH = 8.0f;
static constexpr auto CSGO_ANIM_ONGROUND_FUZZY_APPROACH_CROUCH = 16.0f;
static constexpr auto CSGO_ANIM_LADDER_CLIMB_COVERAGE = 100.0f;
static constexpr auto CSGO_ANIM_RUN_ANIM_PLAYBACK_MULTIPLIER = 0.85f;
void CCSGOPlayerAnimState::SetUpMovement( void )
{
	MDLCACHE_CRITICAL_SECTION();

	if ( m_flWalkToRunTransition > 0 && m_flWalkToRunTransition < 1 )
	{
		//currently transitioning between walk and run
		if ( m_bWalkToRunTransitionState == ANIM_TRANSITION_WALK_TO_RUN )
		{
			m_flWalkToRunTransition += m_flLastUpdateIncrement * CSGO_ANIM_WALK_TO_RUN_TRANSITION_SPEED;
		}
		else // m_bWalkToRunTransitionState == ANIM_TRANSITION_RUN_TO_WALK
		{
			m_flWalkToRunTransition -= m_flLastUpdateIncrement * CSGO_ANIM_WALK_TO_RUN_TRANSITION_SPEED;
		}
		m_flWalkToRunTransition = clamp( m_flWalkToRunTransition, 0, 1 );
	}

	if ( m_flVelocityLengthXY > (CS_PLAYER_SPEED_RUN*CS_PLAYER_SPEED_WALK_MODIFIER) && m_bWalkToRunTransitionState == ANIM_TRANSITION_RUN_TO_WALK )
	{
		//crossed the walk to run threshold
		m_bWalkToRunTransitionState = ANIM_TRANSITION_WALK_TO_RUN;
		m_flWalkToRunTransition = std::max( 0.01f, m_flWalkToRunTransition );
	}
	else if ( m_flVelocityLengthXY < (CS_PLAYER_SPEED_RUN*CS_PLAYER_SPEED_WALK_MODIFIER) && m_bWalkToRunTransitionState == ANIM_TRANSITION_WALK_TO_RUN )
	{
		//crossed the run to walk threshold
		m_bWalkToRunTransitionState = ANIM_TRANSITION_RUN_TO_WALK;
		m_flWalkToRunTransition = std::min( 0.99f, m_flWalkToRunTransition );
	}

	if ( m_nAnimstateModelVersion < 2 )
	{
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_RUN ].SetValue( m_pPlayer, m_flWalkToRunTransition );
	}
	else
	{
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_MOVE_BLEND_WALK ].SetValue( m_pPlayer, (1.0f - m_flWalkToRunTransition) * (1.0f - m_flAnimDuckAmount) );
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_MOVE_BLEND_RUN ].SetValue( m_pPlayer, (m_flWalkToRunTransition) * (1.0f - m_flAnimDuckAmount) );
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_MOVE_BLEND_CROUCH_WALK ].SetValue( m_pPlayer, m_flAnimDuckAmount );
	}

	char szWeaponMoveSeq[MAX_ANIMSTATE_ANIMNAME_CHARS];
	V_sprintf_safe( szWeaponMoveSeq, "move_%s", GetWeaponPrefix() );

	int nWeaponMoveSeq = m_pPlayer->LookupSequence( szWeaponMoveSeq );
	if ( nWeaponMoveSeq == -1 )
	{
		nWeaponMoveSeq = m_pPlayer->LookupSequence( "move" );
	}
	contract_assert( nWeaponMoveSeq > 0 );

	if ( m_pPlayer->m_iMoveState != m_nPreviousMoveState )
	{
		m_flStutterStep += 10;
	}
	m_nPreviousMoveState = m_pPlayer->m_iMoveState;
	m_flStutterStep = clamp( Approach( 0, m_flStutterStep, m_flLastUpdateIncrement * 40 ), 0, 100 );
	
	// recompute moveweight

	float flTargetMoveWeight = Lerp( m_flAnimDuckAmount, clamp(m_flSpeedAsPortionOfWalkTopSpeed, 0, 1), clamp(m_flSpeedAsPortionOfCrouchTopSpeed, 0, 1) );
	//flTargetMoveWeight *= RemapValClamped( m_flStutterStep, 90, 100, 1, 0 );

	if ( m_flMoveWeight <= flTargetMoveWeight )
	{
		m_flMoveWeight = flTargetMoveWeight;
	}
	else
	{
		m_flMoveWeight = Approach( flTargetMoveWeight, m_flMoveWeight, m_flLastUpdateIncrement * RemapValClamped( m_flStutterStep, 0.0f, 100.0f, 2, 20 ) );
	}

	Vector vecMoveYawDir;
	AngleVectors( QAngle(0, AngleNormalize( m_flFootYaw + m_flMoveYaw + 180 ), 0), &vecMoveYawDir );
	float flYawDeltaAbsDot = abs( DotProduct( m_vecVelocityNormalizedNonZero, vecMoveYawDir ) );
	m_flMoveWeight *= Bias( flYawDeltaAbsDot, 0.2 );

	float flMoveWeightWithAirSmooth = m_flMoveWeight * m_flInAirSmoothValue;

	// dampen move weight for landings
	flMoveWeightWithAirSmooth *= std::max( (1.0f - GetLayerWeight( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB ) ), 0.55f );

	float flMoveCycleRate = 0;
	if ( m_flVelocityLengthXY > 0 )
	{
		flMoveCycleRate = m_pPlayer->GetSequenceCycleRate( m_pPlayer->GetModelPtr(), nWeaponMoveSeq );
		float flSequenceGroundSpeed = std::max( m_pPlayer->GetSequenceMoveDist( m_pPlayer->GetModelPtr(), nWeaponMoveSeq ) / ( 1.0f / flMoveCycleRate ), 0.001f );
		flMoveCycleRate *= m_flVelocityLengthXY / flSequenceGroundSpeed;

		flMoveCycleRate *= Lerp( m_flWalkToRunTransition, 1.0f, CSGO_ANIM_RUN_ANIM_PLAYBACK_MULTIPLIER );
	}

	float flLocalCycleIncrement = (flMoveCycleRate * m_flLastUpdateIncrement);
	m_flPrimaryCycle = ClampCycle( m_flPrimaryCycle + flLocalCycleIncrement );
	
	flMoveWeightWithAirSmooth = clamp( flMoveWeightWithAirSmooth, 0, 1 );
	UpdateAnimLayer( ANIMATION_LAYER_MOVEMENT_MOVE, nWeaponMoveSeq, flLocalCycleIncrement, flMoveWeightWithAirSmooth, m_flPrimaryCycle );

	if ( m_pPlayer->m_bStrafing )
	{
		if ( !m_bStrafeChanging )
		{
			m_flDurationStrafing = 0;

			if ( !m_bFirstRunSinceInit && !m_bStrafeChanging && m_bOnGround && m_pPlayer->m_boneSnapshots[BONESNAPSHOT_UPPER_BODY].GetCurrentWeight() <= 0 )
			{
				m_pPlayer->m_boneSnapshots[BONESNAPSHOT_UPPER_BODY].SetShouldCapture( bonesnapshot_get( cl_bonesnapshot_speed_strafestart ) );
			}
		}

		m_bStrafeChanging = true;

		m_flStrafeChangeWeight = Approach( 1, m_flStrafeChangeWeight, m_flLastUpdateIncrement * 20 );
		m_flStrafeChangeCycle = Approach( 0, m_flStrafeChangeCycle, m_flLastUpdateIncrement * 10 );

		m_tPoseParamMappings[ PLAYER_POSE_PARAM_STRAFE_DIR ].SetValue( m_pPlayer, AngleNormalize( m_flMoveYaw ) );

		//float flCross = m_tPoseParamMappings[ PLAYER_POSE_PARAM_STRAFE_CROSS ].GetValue( m_pPlayer );
		//flCross = clamp( Approach( m_bFeetCrossed ? 1 : 0, flCross, m_flLastUpdateIncrement * 10 ), 0, 1);
		//m_tPoseParamMappings[ PLAYER_POSE_PARAM_STRAFE_CROSS ].SetValue( m_pPlayer, flCross );
		//m_tPoseParamMappings[ PLAYER_POSE_PARAM_STRAFE_CROSS ].SetValue( m_pPlayer, 0 );

	}
	else if ( m_flStrafeChangeWeight > 0 )
	{
		m_flDurationStrafing += m_flLastUpdateIncrement;

		if ( m_flDurationStrafing > 0.08f )
			m_flStrafeChangeWeight = Approach( 0, m_flStrafeChangeWeight, m_flLastUpdateIncrement * 5 );

		m_nStrafeSequence = m_pPlayer->LookupSequence( "strafe" );
		float flRate = m_pPlayer->GetSequenceCycleRate( m_pPlayer->GetModelPtr(), m_nStrafeSequence );
		m_flStrafeChangeCycle = clamp( m_flStrafeChangeCycle + m_flLastUpdateIncrement * flRate, 0, 1 );
	}

	if ( m_flStrafeChangeWeight <= 0 )
	{
		m_bStrafeChanging = false;
	}

	// keep track of if the player is on the ground, and if the player has just touched or left the ground since the last check
	bool bPreviousGroundState = m_bOnGround;
	m_bOnGround = ( m_pPlayer->GetFlags() & FL_ONGROUND );

	m_bLandedOnGroundThisFrame = ( !m_bFirstRunSinceInit && bPreviousGroundState != m_bOnGround && m_bOnGround );
	m_bLeftTheGroundThisFrame = ( bPreviousGroundState != m_bOnGround && !m_bOnGround );

	float flDistanceFell = 0;
	if ( m_bLeftTheGroundThisFrame )
	{
		m_flLeftGroundHeight = m_vecPositionCurrent.z;
	}

	if ( m_bLandedOnGroundThisFrame )
	{
		flDistanceFell = abs( m_flLeftGroundHeight - m_vecPositionCurrent.z );
		float flDistanceFallNormalizedBiasRange = Bias( RemapValClamped( flDistanceFell, 12.0f, 72.0f, 0.0f, 1.0f ), 0.4f );

		//Msg( "Fell %f units, ratio is %f. ", flDistanceFell, flDistanceFallNormalizedBiasRange );
		//Msg( "Fell for %f secs, multiplier is %f\n", m_flDurationInAir, m_flLandAnimMultiplier );

		m_flLandAnimMultiplier = clamp( Bias( m_flDurationInAir, 0.3f ), 0.1f, 1.0f );
		m_flDuckAdditional = std::max( m_flLandAnimMultiplier, flDistanceFallNormalizedBiasRange );

		//Msg( "m_flDuckAdditional is %f\n", m_flDuckAdditional );
	}
	else
	{
		m_flDuckAdditional = Approach( 0, m_flDuckAdditional, m_flLastUpdateIncrement * 2 );
	}

	// the in-air smooth value is a fuzzier representation of if the player is on the ground or not.
	// It will approach 1 when the player is on the ground and 0 when in the air. Useful for blending jump animations.
	m_flInAirSmoothValue = Approach( m_bOnGround ? 1 : 0, m_flInAirSmoothValue, Lerp( m_flAnimDuckAmount, CSGO_ANIM_ONGROUND_FUZZY_APPROACH, CSGO_ANIM_ONGROUND_FUZZY_APPROACH_CROUCH ) * m_flLastUpdateIncrement );
	m_flInAirSmoothValue = clamp( m_flInAirSmoothValue, 0, 1 );

	m_flStrafeChangeWeight *= ( 1.0f - m_flAnimDuckAmount );
	m_flStrafeChangeWeight *= m_flInAirSmoothValue;
	m_flStrafeChangeWeight = clamp( m_flStrafeChangeWeight, 0, 1 );

	//if ( m_flStrafeChangeWeight > 0 && flMoveWeightWithAirSmooth <= 0.01f )
	//{
	//	if ( flStrafePose > 0.5f )
	//	{
	//		m_flPrimaryCycle = m_pPlayer->GetFirstSequenceAnimTag( nWeaponMoveSeq, ANIMTAG_STARTCYCLE_W, 0, 1 );
	//	}
	//	else
	//	{
	//		m_flPrimaryCycle = m_pPlayer->GetFirstSequenceAnimTag( nWeaponMoveSeq, ANIMTAG_STARTCYCLE_E, 0, 1 );
	//	}
	//}

	if ( m_nStrafeSequence != -1 )
		UpdateAnimLayer( ANIMATION_LAYER_MOVEMENT_STRAFECHANGE, m_nStrafeSequence, 0, m_flStrafeChangeWeight, m_flStrafeChangeCycle );

	//ladders
	bool bPreviouslyOnLadder = m_bOnLadder;
	m_bOnLadder = !m_bOnGround && m_pPlayer->GetMoveType() == MOVETYPE_LADDER;
	bool bStartedLadderingThisFrame = ( !bPreviouslyOnLadder && m_bOnLadder );
	bool bStoppedLadderingThisFrame = ( bPreviouslyOnLadder && !m_bOnLadder );
	
	if ( bStartedLadderingThisFrame || bStoppedLadderingThisFrame )
	{
	}
	
	if ( m_flLadderWeight > 0 || m_bOnLadder )
	{

		if ( abs(m_flVelocityLengthZ) > 100 )
		{
			m_flLadderSpeed = Approach( 1, m_flLadderSpeed, m_flLastUpdateIncrement * 10.0f );
		}
		else
		{
			m_flLadderSpeed = Approach( 0, m_flLadderSpeed, m_flLastUpdateIncrement * 10.0f );
		}
		m_flLadderSpeed = clamp( m_flLadderSpeed, 0, 1 );

		if ( m_bOnLadder )
		{
			m_flLadderWeight = Approach( 1, m_flLadderWeight, m_flLastUpdateIncrement * 5.0f );
		}
		else
		{
			m_flLadderWeight = Approach( 0, m_flLadderWeight, m_flLastUpdateIncrement * 10.0f );
		}
		m_flLadderWeight = clamp( m_flLadderWeight, 0, 1 );

		Vector vecLadderNormal = m_pPlayer->GetLadderNormal();
		QAngle angLadder;
		VectorAngles( vecLadderNormal, angLadder );
		float flLadderYaw = AngleDiff( angLadder.y, m_flFootYaw );
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_LADDER_YAW ].SetValue( m_pPlayer, flLadderYaw );

		//float flPlayerZ = m_pPlayer->GetAbsOrigin().z;
		//float flLadderClimbCycle = fmod( abs(flPlayerZ), 80.0f ) / 80.0f;
		//flLadderClimbCycle = ClampCycle( flPlayerZ < 0 ? (1.0f - flLadderClimbCycle) : flLadderClimbCycle );

		float flLadderClimbCycle = GetLayerCycle( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB );
		flLadderClimbCycle += (m_vecPositionCurrent.z - m_vecPositionLast.z) * Lerp( m_flLadderSpeed, 0.010f, 0.004f );

		m_tPoseParamMappings[ PLAYER_POSE_PARAM_LADDER_SPEED ].SetValue( m_pPlayer, m_flLadderSpeed );

		if ( GetLayerActivity( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB ) == ACT_CSGO_CLIMB_LADDER )
		{
			SetLayerWeight( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, m_flLadderWeight );
		}
		
		SetLayerCycle( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, flLadderClimbCycle );

		// fade out jump if we're climbing
		if ( m_bOnLadder )
		{
			float flIdealJumpWeight = 1.0f - m_flLadderWeight;
			if ( GetLayerWeight( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL ) > flIdealJumpWeight )
			{
				SetLayerWeight( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL, flIdealJumpWeight );
			}
		}
	}
	else
	{
		m_flLadderSpeed = 0;
	}

	//jumping
	if ( m_bOnGround )
	{
		if ( !m_bLanding && (m_bLandedOnGroundThisFrame || bStoppedLadderingThisFrame) )
		{
			SetLayerCycle( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, 0 );
			m_bLanding = true;
		}
		m_flDurationInAir = 0;
		
		if ( m_bLanding && GetLayerActivity(ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB) != ACT_CSGO_CLIMB_LADDER )
		{

			IncrementLayerCycle( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, false );
			IncrementLayerCycle( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL, false );

			m_tPoseParamMappings[ PLAYER_POSE_PARAM_JUMP_FALL ].SetValue( m_pPlayer, 0 );

			if ( IsLayerSequenceCompleted( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB ) )
			{
				m_bLanding = false;
				SetLayerWeight( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, 0 );
				//SetLayerRate( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, 1.0f );
				SetLayerWeight( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL, 0 );
				m_flLandAnimMultiplier = 1.0f;
			}
			else
			{

				float flLandWeight = GetLayerIdealWeightFromSeqCycle( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB ) * m_flLandAnimMultiplier;

				// if we hit the ground crouched, reduce the land animation as a function of crouch, since the land animations move the head up a bit ( and this is undesirable )
				flLandWeight *= clamp( (1.0f - m_flAnimDuckAmount), 0.2f, 1.0f );

				SetLayerWeight( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, flLandWeight );
				
				// fade out jump because land is taking over
				float flCurrentJumpFallWeight = GetLayerWeight( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL );
				if ( flCurrentJumpFallWeight > 0 )
				{
					flCurrentJumpFallWeight = Approach( 0, flCurrentJumpFallWeight, m_flLastUpdateIncrement * 10.0f );
					SetLayerWeight( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL, flCurrentJumpFallWeight );
				}
				
			}
		}

	}
	else if ( !m_bOnLadder )
	{
		m_bLanding = false;

		// we're in the air
		if ( m_bLeftTheGroundThisFrame || bStoppedLadderingThisFrame )
		{ 
			m_flDurationInAir = 0;
		}
		
		m_flDurationInAir += m_flLastUpdateIncrement;

//#ifndef CLIENT_DLL
//		Msg( "%f\n", m_flDurationInAir );
//#endif

		IncrementLayerCycle( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL, false );

		// increase jump weight
		float flJumpWeight = GetLayerWeight( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL );
		float flNextJumpWeight = GetLayerIdealWeightFromSeqCycle( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL );
		if ( flNextJumpWeight > flJumpWeight )
		{
			SetLayerWeight( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL, flNextJumpWeight );
		}

		// bash any lingering land weight to zero
		float flLingeringLandWeight = GetLayerWeight( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB );
		if ( flLingeringLandWeight > 0 )
		{
			flLingeringLandWeight *= smoothstep_bounds( 0.2f, 0.0f, m_flDurationInAir );
			SetLayerWeight( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, flLingeringLandWeight );
		}

		// blend jump into fall. This is a no-op if we're playing a fall anim.
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_JUMP_FALL ].SetValue( m_pPlayer, clamp(smoothstep_bounds( 0.72f, 1.52f, m_flDurationInAir ),0,1) );

	}

}

Activity CCSGOPlayerAnimState::GetLayerActivity( animstate_layer_t nLayerIndex )
{
	MDLCACHE_CRITICAL_SECTION();
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( pLayer )
		return (Activity)GetSequenceActivity( m_pPlayer->GetModelPtr(), pLayer->GetSequence() );
	return ACT_INVALID;
}

bool CCSGOPlayerAnimState::IsLayerSequenceCompleted( animstate_layer_t nLayerIndex )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( pLayer )
		return ( (pLayer->GetCycle() + (m_flLastUpdateIncrement * pLayer->GetPlaybackRate())) >= 1 );
	return false;
}

void CCSGOPlayerAnimState::SetLayerRate( animstate_layer_t nLayerIndex, float flRate )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( pLayer )
		pLayer->SetPlaybackRate( flRate );
}

void CCSGOPlayerAnimState::SetLayerCycle( animstate_layer_t nLayerIndex, float flCycle )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( pLayer )
		pLayer->SetCycle( ClampCycle( flCycle ) );
}

void CCSGOPlayerAnimState::IncrementLayerCycleWeightRateGeneric( animstate_layer_t nLayerIndex )
{
	float flWeightPrevious = GetLayerWeight( nLayerIndex );
	IncrementLayerCycle( nLayerIndex, false );
	SetLayerWeight( nLayerIndex, GetLayerIdealWeightFromSeqCycle( nLayerIndex ) );
	SetLayerWeightRate( nLayerIndex, flWeightPrevious );
}

void CCSGOPlayerAnimState::IncrementLayerCycle( animstate_layer_t nLayerIndex, bool bAllowLoop )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );

	if ( !pLayer )
		return;

	if ( abs(pLayer->GetPlaybackRate()) <= 0 )
		return;

	float flCurrentCycle = pLayer->GetCycle();
	flCurrentCycle += m_flLastUpdateIncrement * pLayer->GetPlaybackRate();

	if ( !bAllowLoop && flCurrentCycle >= 1 )
	{
		flCurrentCycle = 0.999f;
	}

	pLayer->SetCycle( ClampCycle( flCurrentCycle ) );
}

void CCSGOPlayerAnimState::IncrementLayerWeight( animstate_layer_t nLayerIndex )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );

	if ( !pLayer )
		return;

	if ( abs(pLayer->GetWeightDeltaRate()) <= 0 )
		return;

	float flCurrentWeight = pLayer->GetWeight();
	flCurrentWeight += m_flLastUpdateIncrement * pLayer->GetWeightDeltaRate();
	flCurrentWeight = clamp( flCurrentWeight, 0, 1 );

	pLayer->SetWeight( flCurrentWeight );
}

void CCSGOPlayerAnimState::SetLayerSequence( animstate_layer_t nLayerIndex, int nSequence )
{
	contract_assert( nSequence > 1 );
	if ( nSequence > 1 )
	{
		MDLCACHE_CRITICAL_SECTION();

		CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
		
		if ( !pLayer )
			return;

		pLayer->SetSequence( nSequence );
		float flPlaybackRate = m_pPlayer->GetLayerSequenceCycleRate( pLayer, nSequence );
		pLayer->SetPlaybackRate( flPlaybackRate );
		pLayer->SetCycle( 0 );
		pLayer->SetWeight( 0 );
		UpdateLayerOrderPreset( nLayerIndex, nSequence );
	}
}

float CCSGOPlayerAnimState::GetLayerIdealWeightFromSeqCycle( animstate_layer_t nLayerIndex )
{
	MDLCACHE_CRITICAL_SECTION();
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );

	if ( !pLayer )
		return 0;

	mstudioseqdesc_t &seqdesc = m_pPlayer->GetModelPtr()->pSeqdesc( pLayer->GetSequence() );

	float flCycle = pLayer->GetCycle();
	if ( flCycle >= 0.999f )
		flCycle = 1;
	float flEaseIn = seqdesc.fadeintime;
	float flEaseOut = seqdesc.fadeouttime;
	float flIdealWeight = 1;
	
	if ( flEaseIn > 0 && flCycle < flEaseIn )
	{
		flIdealWeight = smoothstep_bounds( 0, flEaseIn, flCycle );
	}
	else if ( flEaseOut < 1 && flCycle > flEaseOut )
	{
		flIdealWeight = smoothstep_bounds( 1.0f, flEaseOut, flCycle );
	}

	if ( flIdealWeight < 0.0015f )
		return 0;

	return (clamp( flIdealWeight, 0, 1));
}

float CCSGOPlayerAnimState::GetLayerCycle( animstate_layer_t nLayerIndex )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( pLayer )
		return pLayer->GetCycle();
	return 0;
}

int CCSGOPlayerAnimState::GetLayerSequence( animstate_layer_t nLayerIndex )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( pLayer )
		return pLayer->GetSequence();
	return 0;
}

float CCSGOPlayerAnimState::GetLayerRate( animstate_layer_t nLayerIndex )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( pLayer )
		return pLayer->GetPlaybackRate();
	return 0;
}

float CCSGOPlayerAnimState::GetLayerWeight( animstate_layer_t nLayerIndex )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( pLayer )
		return pLayer->GetWeight();
	return 0;
}

void CCSGOPlayerAnimState::SetLayerWeight( animstate_layer_t nLayerIndex, float flWeight )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( !pLayer )
		return;
	pLayer->SetWeight( clamp( flWeight, 0, 1) );
}

void CCSGOPlayerAnimState::SetLayerWeightRate( animstate_layer_t nLayerIndex, float flPrevious )
{
	if ( m_flLastUpdateIncrement == 0 )
		return;
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( !pLayer )
		return;
	float flNewRate = ( pLayer->GetWeight() - flPrevious ) / m_flLastUpdateIncrement;
	pLayer->SetWeightDeltaRate( flNewRate );
}

void CCSGOPlayerAnimState::UpdateAnimLayer( animstate_layer_t nLayerIndex, int nSequence, float flPlaybackRate, float flWeight, float flCycle )
{
	contract_assert( flWeight >= 0 && flWeight <= 1 );
	contract_assert( flCycle >= 0 && flCycle <= 1 );
	contract_assert( nSequence > 1 );
	
	contract_assert( nSequence > 1 );
	if ( nSequence > 1 )
	{
		MDLCACHE_CRITICAL_SECTION();	
	
		CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
		if ( !pLayer )
			return;
		pLayer->SetSequence( nSequence );
		pLayer->SetPlaybackRate( flPlaybackRate );
		pLayer->SetCycle( clamp( flCycle, 0, 1) );
		pLayer->SetWeight( clamp( flWeight, 0, 1) );
		UpdateLayerOrderPreset( nLayerIndex, nSequence );
	}
}

void CCSGOPlayerAnimState::ApplyLayerOrderPreset( animlayerpreset nNewPreset, bool bForce )
{
	if ( !bForce && m_pLayerOrderPreset == nNewPreset )
		return;

	m_pLayerOrderPreset = nNewPreset;

	for ( int i=0; i < ANIMATION_LAYER_COUNT; i++ )
	{
		CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( m_pLayerOrderPreset[i], USE_ANIMLAYER_RAW_INDEX );
		if ( pLayer )
		{
			pLayer->SetOrder( i );

			// purge dispatch info too
			pLayer->m_pDispatchedStudioHdr = nullptr;
			pLayer->m_nDispatchedSrc = ACT_INVALID;
			pLayer->m_nDispatchedDst = ACT_INVALID;
		}
	}
}

void CCSGOPlayerAnimState::UpdateLayerOrderPreset( animstate_layer_t nLayerIndex, int nSequence )
{
	if ( !m_pPlayer || nLayerIndex != ANIMATION_LAYER_WEAPON_ACTION )
		return;
	
	MDLCACHE_CRITICAL_SECTION();

	if ( m_pPlayer->GetAnySequenceAnimTag( nSequence, ANIMTAG_WEAPON_POSTLAYER, 0 ) != 0 )
	{
		ApplyLayerOrderPreset( get_animlayerpreset( WeaponPost ) );
	}
	else
	{
		ApplyLayerOrderPreset( get_animlayerpreset( Default ) );
	}	
}

bool CCSGOPlayerAnimState::LayerSequenceHasActMod( animstate_layer_t nLayerIndex, const char* szActMod )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( !pLayer )
		return false;
	//ActivityModifierSym_t sym = g_ActivityModifiersTable.find( szActMod ).data();
	//if ( sym )
	//{
		mstudioseqdesc_t &seqdesc = m_pPlayer->GetModelPtr()->pSeqdesc( pLayer->GetSequence() );
		for ( int i=0; i<seqdesc.numactivitymodifiers; i++ )
		{
			mstudioactivitymodifier_t *mod = seqdesc.pActivityModifier(i);

			if ( !V_strcmp( mod->pszName(), szActMod ) )
				return true;
		}
	//}
	return false;
}

static constexpr auto CSGO_ANIM_SPEED_TO_CHANGE_AIM_MATRIX = 0.8f;
static constexpr auto CSGO_ANIM_SPEED_TO_CHANGE_AIM_MATRIX_SCOPED = 4.2f;
void CCSGOPlayerAnimState::SetUpAimMatrix( void )
{
	MDLCACHE_CRITICAL_SECTION();
	
	if ( m_flAnimDuckAmount <= 0 || m_flAnimDuckAmount >= 1 ) // only transition aim pose when fully ducked or fully standing
	{
		bool bPlayerIsWalking = ( m_pPlayer && m_pPlayer->m_bIsWalking );
		bool bPlayerIsScoped = ( m_pPlayer && m_pPlayer->m_bIsScoped );

		float flTransitionSpeed = m_flLastUpdateIncrement * ( bPlayerIsScoped ? CSGO_ANIM_SPEED_TO_CHANGE_AIM_MATRIX_SCOPED : CSGO_ANIM_SPEED_TO_CHANGE_AIM_MATRIX );

		if ( bPlayerIsScoped ) // hacky: just tell all the transitions they've been invalid too long so all transitions clear as soon as the player starts scoping
		{
			m_tStandWalkAim.m_flDurationStateHasBeenInValid = m_tStandWalkAim.m_flHowLongToWaitUntilTransitionCanBlendOut;
			m_tStandRunAim.m_flDurationStateHasBeenInValid = m_tStandRunAim.m_flHowLongToWaitUntilTransitionCanBlendOut;
			m_tCrouchWalkAim.m_flDurationStateHasBeenInValid = m_tCrouchWalkAim.m_flHowLongToWaitUntilTransitionCanBlendOut;
		}

		m_tStandWalkAim.UpdateTransitionState( bPlayerIsWalking && !bPlayerIsScoped && m_flSpeedAsPortionOfWalkTopSpeed > 0.7f && m_flSpeedAsPortionOfRunTopSpeed < 0.7,
			m_flLastUpdateIncrement, flTransitionSpeed );

		m_tStandRunAim.UpdateTransitionState( !bPlayerIsScoped && m_flSpeedAsPortionOfRunTopSpeed >= 0.7,
			m_flLastUpdateIncrement, flTransitionSpeed );

		m_tCrouchWalkAim.UpdateTransitionState( !bPlayerIsScoped && m_flSpeedAsPortionOfCrouchTopSpeed >= 0.5,
			m_flLastUpdateIncrement, flTransitionSpeed );
	}

	// Set aims to zero weight if they're underneath aims with 100% weight, for animation perf optimization.
	// Also set aims to full weight if their overlapping aims aren't enough to cover them, because cross-fades don't sum to 100% weight.

	float flStandIdleWeight = 1;
	float flStandWalkWeight = m_tStandWalkAim.m_flBlendValue;
	float flStandRunWeight = m_tStandRunAim.m_flBlendValue;
	float flCrouchIdleWeight = 1;
	float flCrouchWalkWeight = m_tCrouchWalkAim.m_flBlendValue;

	if ( flStandWalkWeight >= 1 )
		flStandIdleWeight = 0;

	if ( flStandRunWeight >= 1 )
	{
		flStandIdleWeight = 0;
		flStandWalkWeight = 0;
	}

	if ( flCrouchWalkWeight >= 1 )
		flCrouchIdleWeight = 0;

	if ( m_flAnimDuckAmount >= 1 )
	{
		flStandIdleWeight = 0;
		flStandWalkWeight = 0;
		flStandRunWeight = 0;
	}
	else if ( m_flAnimDuckAmount <= 0 )
	{
		flCrouchIdleWeight = 0;
		flCrouchWalkWeight = 0;
	}

	float flOneMinusDuckAmount = 1.0f - m_flAnimDuckAmount;

	flCrouchIdleWeight *= m_flAnimDuckAmount;
	flCrouchWalkWeight *= m_flAnimDuckAmount;
	flStandWalkWeight *= flOneMinusDuckAmount;
	flStandRunWeight *= flOneMinusDuckAmount;

	// make sure idle is present underneath cross-fades
	if ( flCrouchIdleWeight < 1 && flCrouchWalkWeight < 1 && flStandWalkWeight < 1 && flStandRunWeight < 1 )
		flStandIdleWeight = 1;

	m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_STAND_IDLE ].SetValue( m_pPlayer, flStandIdleWeight );
	m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_STAND_WALK ].SetValue( m_pPlayer, flStandWalkWeight );
	m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_STAND_RUN ].SetValue( m_pPlayer, flStandRunWeight );
	m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_CROUCH_IDLE ].SetValue( m_pPlayer, flCrouchIdleWeight );
	m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_CROUCH_WALK ].SetValue( m_pPlayer, flCrouchWalkWeight );

	char szTransitionStandAimMatrix[MAX_ANIMSTATE_ANIMNAME_CHARS];
	V_sprintf_safe( szTransitionStandAimMatrix, "%s_aim", GetWeaponPrefix() );
	int nSeqStand = m_pPlayer->LookupSequence( szTransitionStandAimMatrix );

	{
		// use data-driven aim matrix limits

		CAnimationLayer *pAimLayer = m_pPlayer->GetAnimOverlay( ANIMATION_LAYER_AIMMATRIX, USE_ANIMLAYER_RAW_INDEX );
		if ( pAimLayer && m_pWeapon )
		{
			MDLCACHE_CRITICAL_SECTION();

			CBaseAnimating *pAimMatrixHolder = m_pPlayer;
			int nSeq = nSeqStand;

			CBaseWeaponWorldModel *pWeaponWorldModel = m_pWeapon->m_hWeaponWorldModel.Get();
			if ( pWeaponWorldModel && pAimLayer->m_nDispatchedDst != ACT_INVALID )
			{
				pAimMatrixHolder = pWeaponWorldModel;
				nSeq = pAimLayer->m_nDispatchedDst;
			}

			if ( nSeq > 0 )
			{
				float flYawIdleMin = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_YAWMIN_IDLE, CSGO_ANIM_AIMMATRIX_DEFAULT_YAW_MIN );
				float flYawIdleMax = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_YAWMAX_IDLE, CSGO_ANIM_AIMMATRIX_DEFAULT_YAW_MAX );
				float flYawWalkMin = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_YAWMIN_WALK, flYawIdleMin );
				float flYawWalkMax = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_YAWMAX_WALK, flYawIdleMax );
				float flYawRunMin = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_YAWMIN_RUN, flYawWalkMin );
				float flYawRunMax = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_YAWMAX_RUN, flYawWalkMax );
				float flYawCrouchIdleMin = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_YAWMIN_CROUCHIDLE, CSGO_ANIM_AIMMATRIX_DEFAULT_YAW_MIN );
				float flYawCrouchIdleMax = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_YAWMAX_CROUCHIDLE, CSGO_ANIM_AIMMATRIX_DEFAULT_YAW_MAX );
				float flYawCrouchWalkMin = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_YAWMIN_CROUCHWALK, flYawCrouchIdleMin );
				float flYawCrouchWalkMax = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_YAWMAX_CROUCHWALK, flYawCrouchIdleMax );
				
				float flWalkAmt = m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_STAND_WALK ].GetValue( m_pPlayer );
				float flRunAmt = m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_STAND_RUN ].GetValue( m_pPlayer );
				float flCrouchWalkAmt = m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_CROUCH_WALK ].GetValue( m_pPlayer );
			
				m_flAimYawMin = Lerp( m_flAnimDuckAmount, 
										Lerp( flRunAmt, Lerp( flWalkAmt, flYawIdleMin, flYawWalkMin ), flYawRunMin ),
										Lerp( flCrouchWalkAmt, flYawCrouchIdleMin, flYawCrouchWalkMin ) );
				m_flAimYawMax = Lerp( m_flAnimDuckAmount,
										Lerp( flRunAmt, Lerp( flWalkAmt, flYawIdleMax, flYawWalkMax ), flYawRunMax ),
										Lerp( flCrouchWalkAmt, flYawCrouchIdleMax, flYawCrouchWalkMax ) );

				float flPitchIdleMin = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_PITCHMIN_IDLE, CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MIN );
				float flPitchIdleMax = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_PITCHMAX_IDLE, CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MAX );
				float flPitchWalkRunMin = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_PITCHMIN_WALKRUN, flPitchIdleMin );
				float flPitchWalkRunMax = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_PITCHMAX_WALKRUN, flPitchIdleMax );
				float flPitchCrouchMin = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_PITCHMIN_CROUCH, CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MIN );
				float flPitchCrouchMax = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_PITCHMAX_CROUCH, CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MAX );
				float flPitchCrouchWalkMin = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_PITCHMIN_CROUCHWALK, flPitchCrouchMin );
				float flPitchCrouchWalkMax = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_PITCHMAX_CROUCHWALK, flPitchCrouchMax );

				m_flAimPitchMin = Lerp( m_flAnimDuckAmount, Lerp( flWalkAmt, flPitchIdleMin, flPitchWalkRunMin ), Lerp( flCrouchWalkAmt, flPitchCrouchMin, flPitchCrouchWalkMin ) );
				m_flAimPitchMax = Lerp( m_flAnimDuckAmount, Lerp( flWalkAmt, flPitchIdleMax, flPitchWalkRunMax ), Lerp( flCrouchWalkAmt, flPitchCrouchMax, flPitchCrouchWalkMax ) );
			}
		}
	}

	UpdateAnimLayer( ANIMATION_LAYER_AIMMATRIX, nSeqStand, 0, 1, 0 );
}

static constexpr auto CSGO_ANIM_LOWER_CATCHUP_IDLE = 100.0f;
static constexpr auto CSGO_ANIM_AIM_NARROW_WALK = 0.8f;
static constexpr auto CSGO_ANIM_AIM_NARROW_RUN = 0.5f;
static constexpr auto CSGO_ANIM_AIM_NARROW_CROUCHMOVING = 0.5f;
static constexpr auto CSGO_ANIM_LOWER_CATCHUP_WITHIN = 3.0f;
static constexpr auto CSGO_ANIM_LOWER_REALIGN_DELAY = 1.1f;
static constexpr auto CSGO_ANIM_READJUST_THRESHOLD = 120.0f;
static constexpr auto EIGHT_WAY_WIDTH = 22.5f;
void CCSGOPlayerAnimState::SetUpVelocity( void )
{
	MDLCACHE_CRITICAL_SECTION();

	// update the local velocity variables so we don't recalculate them unnecessarily

	Vector vecAbsVelocity = m_vecVelocity;

	if ( engine->IsHLTV() || engine->IsPlayingDemo() )
	{
		// Estimating velocity when playing demos is prone to fail, especially in POVs. Fall back to GetAbsVelocity.
		vecAbsVelocity = m_pPlayer->GetAbsVelocity();
	}
	else
	{
		m_pPlayer->EstimateAbsVelocity( vecAbsVelocity );	// Using this accessor if the client is starved of information, 
															// the player doesn't run on the spot. Note this is unreliable
															// and could fail to populate the value if prediction fails.
	}

	// prevent the client input velocity vector from exceeding a reasonable magnitude
	#define CSGO_ANIM_MAX_VEL_LIMIT 1.2f
	if ( vecAbsVelocity.LengthSqr() > Sqr( CS_PLAYER_SPEED_RUN * CSGO_ANIM_MAX_VEL_LIMIT ) )
		vecAbsVelocity = vecAbsVelocity.Normalized() * (CS_PLAYER_SPEED_RUN * CSGO_ANIM_MAX_VEL_LIMIT);

	// save vertical velocity component
	m_flVelocityLengthZ = vecAbsVelocity.z;

	// discard z component
	vecAbsVelocity.z = 0;
	
	// remember if the player is accelerating.
	m_bPlayerIsAccelerating = ( m_vecVelocityLast.LengthSqr() < vecAbsVelocity.LengthSqr() );

	// rapidly approach ideal velocity instead of instantly adopt it. This helps smooth out instant velocity changes, like
	// when the player runs headlong into a wall and their velocity instantly becomes zero.
	m_vecVelocity = Approach( vecAbsVelocity, m_vecVelocity, m_flLastUpdateIncrement * 2000 );
	m_vecVelocityNormalized = m_vecVelocity.Normalized();

	// save horizontal velocity length
	m_flVelocityLengthXY = std::min( m_vecVelocity.Length(), CS_PLAYER_SPEED_RUN );
	
	if ( m_flVelocityLengthXY > 0 )
	{
		m_vecVelocityNormalizedNonZero = m_vecVelocityNormalized;
	}

	//compute speed in various normalized forms
	float flMaxSpeedRun = m_pWeapon ? std::max( m_pWeapon->GetMaxSpeed(), 0.001f ) : CS_PLAYER_SPEED_RUN;
	contract_assert( flMaxSpeedRun > 0 );

	m_flSpeedAsPortionOfRunTopSpeed = clamp( m_flVelocityLengthXY / flMaxSpeedRun, 0, 1 );
	m_flSpeedAsPortionOfWalkTopSpeed = m_flVelocityLengthXY / (flMaxSpeedRun * CS_PLAYER_SPEED_WALK_MODIFIER);
	m_flSpeedAsPortionOfCrouchTopSpeed = m_flVelocityLengthXY / (flMaxSpeedRun * CS_PLAYER_SPEED_DUCK_MODIFIER);
	
	if ( m_flSpeedAsPortionOfWalkTopSpeed >= 1 )
	{
		m_flStaticApproachSpeed = m_flVelocityLengthXY;
	}
	else if ( m_flSpeedAsPortionOfWalkTopSpeed < 0.5f )
	{
		m_flStaticApproachSpeed = Approach( 80, m_flStaticApproachSpeed, m_flLastUpdateIncrement * 60 );
	}

	bool bStartedMovingThisFrame = false;
	bool bStoppedMovingThisFrame = false;

	if ( m_flVelocityLengthXY > 0 )
	{
		bStartedMovingThisFrame = ( m_flDurationMoving <= 0 );
		m_flDurationStill = 0;
		m_flDurationMoving += m_flLastUpdateIncrement;
	}
	else
	{
		bStoppedMovingThisFrame = ( m_flDurationStill <= 0 );
		m_flDurationMoving = 0;
		m_flDurationStill += m_flLastUpdateIncrement;
	}

	// if the player is looking far enough to either side, turn the feet to keep them within the extent of the aim matrix
	m_flFootYawLast = m_flFootYaw;
	m_flFootYaw = clamp( m_flFootYaw, -360, 360 );
	float flEyeFootDelta = AngleDiff(m_flEyeYaw, m_flFootYaw);

	// narrow the available aim matrix width as speed increases
	float flAimMatrixWidthRange = Lerp( clamp(m_flSpeedAsPortionOfWalkTopSpeed,0,1), 1.0f, Lerp( m_flWalkToRunTransition, CSGO_ANIM_AIM_NARROW_WALK, CSGO_ANIM_AIM_NARROW_RUN ) );

	if ( m_flAnimDuckAmount > 0 )
	{
		flAimMatrixWidthRange = Lerp( m_flAnimDuckAmount * clamp( m_flSpeedAsPortionOfCrouchTopSpeed, 0, 1 ), flAimMatrixWidthRange, CSGO_ANIM_AIM_NARROW_CROUCHMOVING );
	}
	
	float flTempYawMax = m_flAimYawMax * flAimMatrixWidthRange;
	float flTempYawMin = m_flAimYawMin * flAimMatrixWidthRange;

	if ( flEyeFootDelta > flTempYawMax )
	{
		m_flFootYaw = m_flEyeYaw - abs(flTempYawMax);
	}
	else if ( flEyeFootDelta < flTempYawMin )
	{
		m_flFootYaw = m_flEyeYaw + abs(flTempYawMin);
	}
	m_flFootYaw = AngleNormalize( m_flFootYaw );

	// pull the lower body direction towards the eye direction, but only when the player is moving
	if ( m_bOnGround )
	{
		if ( m_flVelocityLengthXY > 0.1f )
		{
			m_flFootYaw = ApproachAngle( m_flEyeYaw, m_flFootYaw, m_flLastUpdateIncrement * (30.0f + 20.0f * m_flWalkToRunTransition) );

		}
		else
		{
			m_flFootYaw = ApproachAngle( m_pPlayer->m_flLowerBodyYawTarget.Get(), m_flFootYaw, m_flLastUpdateIncrement * CSGO_ANIM_LOWER_CATCHUP_IDLE );

		}
	}

	if ( GetLayerWeight( ANIMATION_LAYER_ADJUST ) > 0 )
	{
		IncrementLayerCycle( ANIMATION_LAYER_ADJUST, false );
		IncrementLayerWeight( ANIMATION_LAYER_ADJUST );
	}

	// the final model render yaw is aligned to the foot yaw

	if ( m_flVelocityLengthXY > 0 && m_bOnGround )
	{
		// convert horizontal velocity vec to angular yaw
		float flRawYawIdeal = (std::atan2(-m_vecVelocity[1], -m_vecVelocity[0]) * 180 / std::numbers::pi);
		if (flRawYawIdeal < 0)
			flRawYawIdeal += 360;

		m_flMoveYawIdeal = AngleNormalize( AngleDiff( flRawYawIdeal, m_flFootYaw ) );
	}

	// delta between current yaw and ideal velocity derived target (possibly negative!)
	m_flMoveYawCurrentToIdeal = AngleNormalize( AngleDiff( m_flMoveYawIdeal, m_flMoveYaw ) );

	if ( bStartedMovingThisFrame && m_flMoveWeight <= 0 )
	{
		m_flMoveYaw = m_flMoveYawIdeal;

		// select a special starting cycle that's set by the animator in content
		int nMoveSeq = GetLayerSequence( ANIMATION_LAYER_MOVEMENT_MOVE );
		if ( nMoveSeq != -1 )
		{
			mstudioseqdesc_t &seqdesc = m_pPlayer->GetModelPtr()->pSeqdesc( nMoveSeq );
			if ( seqdesc.numanimtags > 0 )
			{
				if ( abs( AngleDiff( m_flMoveYaw, 180 ) ) <= EIGHT_WAY_WIDTH ) //N
				{
					m_flPrimaryCycle = m_pPlayer->GetFirstSequenceAnimTag( nMoveSeq, ANIMTAG_STARTCYCLE_N, 0, 1 );
				}
				else if ( abs( AngleDiff( m_flMoveYaw, 135 ) ) <= EIGHT_WAY_WIDTH ) //NE
				{
					m_flPrimaryCycle = m_pPlayer->GetFirstSequenceAnimTag( nMoveSeq, ANIMTAG_STARTCYCLE_NE, 0, 1 );
				}
				else if ( abs( AngleDiff( m_flMoveYaw, 90 ) ) <= EIGHT_WAY_WIDTH ) //E
				{
					m_flPrimaryCycle = m_pPlayer->GetFirstSequenceAnimTag( nMoveSeq, ANIMTAG_STARTCYCLE_E, 0, 1 );
				}
				else if ( abs( AngleDiff( m_flMoveYaw, 45 ) ) <= EIGHT_WAY_WIDTH ) //SE
				{
					m_flPrimaryCycle = m_pPlayer->GetFirstSequenceAnimTag( nMoveSeq, ANIMTAG_STARTCYCLE_SE, 0, 1 );
				}
				else if ( abs( AngleDiff( m_flMoveYaw, 0 ) ) <= EIGHT_WAY_WIDTH ) //S
				{
					m_flPrimaryCycle = m_pPlayer->GetFirstSequenceAnimTag( nMoveSeq, ANIMTAG_STARTCYCLE_S, 0, 1 );
				}
				else if ( abs( AngleDiff( m_flMoveYaw, -45 ) ) <= EIGHT_WAY_WIDTH ) //SW
				{
					m_flPrimaryCycle = m_pPlayer->GetFirstSequenceAnimTag( nMoveSeq, ANIMTAG_STARTCYCLE_SW, 0, 1 );
				}
				else if ( abs( AngleDiff( m_flMoveYaw, -90 ) ) <= EIGHT_WAY_WIDTH ) //W
				{
					m_flPrimaryCycle = m_pPlayer->GetFirstSequenceAnimTag( nMoveSeq, ANIMTAG_STARTCYCLE_W, 0, 1 );
				}
				else if ( abs( AngleDiff( m_flMoveYaw, -135 ) ) <= EIGHT_WAY_WIDTH ) //NW
				{
					m_flPrimaryCycle = m_pPlayer->GetFirstSequenceAnimTag( nMoveSeq, ANIMTAG_STARTCYCLE_NW, 0, 1 );
				}
			}
		}

		if ( m_flInAirSmoothValue >= 1 && !m_bFirstRunSinceInit && abs(m_flMoveYawCurrentToIdeal) > 45 && m_bOnGround && m_pPlayer->m_boneSnapshots[BONESNAPSHOT_ENTIRE_BODY].GetCurrentWeight() <= 0 )
		{
			m_pPlayer->m_boneSnapshots[BONESNAPSHOT_ENTIRE_BODY].SetShouldCapture( bonesnapshot_get( cl_bonesnapshot_speed_movebegin ) );
		}

	}
	else
	{
		if ( GetLayerWeight( ANIMATION_LAYER_MOVEMENT_STRAFECHANGE ) >= 1 )
		{
			m_flMoveYaw = m_flMoveYawIdeal;
		}
		else
		{
			if ( m_flInAirSmoothValue >= 1 && !m_bFirstRunSinceInit && abs(m_flMoveYawCurrentToIdeal) > 100 && m_bOnGround && m_pPlayer->m_boneSnapshots[BONESNAPSHOT_ENTIRE_BODY].GetCurrentWeight() <= 0 )
			{
				m_pPlayer->m_boneSnapshots[BONESNAPSHOT_ENTIRE_BODY].SetShouldCapture( bonesnapshot_get( cl_bonesnapshot_speed_movebegin ) );
			}

			float flMoveWeight = Lerp( m_flAnimDuckAmount, clamp(m_flSpeedAsPortionOfWalkTopSpeed, 0, 1), clamp(m_flSpeedAsPortionOfCrouchTopSpeed, 0, 1) );
			float flRatio = Bias( flMoveWeight, 0.18f ) + 0.1f;
			
			m_flMoveYaw = AngleNormalize( m_flMoveYaw + ( m_flMoveYawCurrentToIdeal * flRatio ) );
		}
	}	

	m_tPoseParamMappings[ PLAYER_POSE_PARAM_MOVE_YAW ].SetValue( m_pPlayer, m_flMoveYaw );
	
	float flAimYaw = AngleDiff( m_flEyeYaw, m_flFootYaw );
	if ( flAimYaw >= 0 && m_flAimYawMax != 0 )
	{
		flAimYaw = (flAimYaw / m_flAimYawMax) * 60.0f;
	}
	else if ( m_flAimYawMin != 0 )
	{
		flAimYaw = (flAimYaw / m_flAimYawMin) * -60.0f;
	}

	m_tPoseParamMappings[ PLAYER_POSE_PARAM_BODY_YAW ].SetValue( m_pPlayer, flAimYaw );

	// we need non-symmetrical arbitrary min/max bounds for vertical aim (pitch) too
	float flPitch = AngleDiff( m_flEyePitch, 0 );
	if ( flPitch > 0 )
	{
		flPitch = (flPitch / m_flAimPitchMax) * CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MAX;
	}
	else
	{
		flPitch = (flPitch / m_flAimPitchMin) * CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MIN;
	}

	m_tPoseParamMappings[ PLAYER_POSE_PARAM_BODY_PITCH ].SetValue( m_pPlayer, flPitch );
	m_tPoseParamMappings[ PLAYER_POSE_PARAM_SPEED ].SetValue( m_pPlayer, m_flSpeedAsPortionOfWalkTopSpeed );
	m_tPoseParamMappings[ PLAYER_POSE_PARAM_STAND ].SetValue( m_pPlayer, 1.0f - (m_flAnimDuckAmount*m_flInAirSmoothValue) );
}

const char* CCSGOPlayerAnimState::GetWeaponPrefix( void )
{
	int nWeaponType = 0; // knife

	m_pWeapon = m_pPlayer->GetActiveCSWeapon();
	if ( m_pWeapon )
	{
		nWeaponType = (int)m_pWeapon->GetWeaponType();

		int nWeaponID = m_pWeapon->GetCSWeaponID();
		if ( nWeaponID == WEAPON_MAG7 )
		{
			nWeaponType = WEAPONTYPE_RIFLE;
		}
		else if ( nWeaponID == WEAPON_TASER )
		{
			nWeaponType = WEAPONTYPE_PISTOL;
		}

		if ( nWeaponType == WEAPONTYPE_STACKABLEITEM )
		{
			nWeaponType = WEAPONTYPE_GRENADE; // redirect healthshot, adrenaline, etc to the grenade archetype
		}
	}

	return g_szWeaponPrefixLookupTable[clamp( nWeaponType, 0, 9 )];
}

void CCSGOPlayerAnimState::WriteAnimstateDebugBuffer( char *pDest )
{
}

bool CCSGOPlayerAnimState::CacheSequences( void )
{
	if ( !m_pPlayer )
		return false;

	if ( m_cachedModelIndex != m_pPlayer->GetModelIndex() )
	{
		// read animset version from keyvalues
		m_nAnimstateModelVersion = 0;

		ks::text_writer buf;
		buf.put( "keyvalues {\n" ); // trick the kv loader into thinking this is one big block
		if ( modelinfo->GetModelKeyValue( m_pPlayer->GetModel(), buf ) )
		{
			buf.put( "\n}" ); // end trickery
			KeyValues *modelKeyValues = new KeyValues("");
			KeyValues::AutoDelete autodelete_key( modelKeyValues );
			if ( modelKeyValues->LoadFromBuffer( modelinfo->GetModelName( m_pPlayer->GetModel() ), buf.view() ) )
			{
				for ( KeyValues *pSubKey = modelKeyValues->GetFirstSubKey(); pSubKey != nullptr; pSubKey = pSubKey->GetNextKey() )
				{
					//KeyValuesDumpAsDevMsg( pSubKey );
					if ( pSubKey->FindKey( "animset_version" ) )
					{
						m_nAnimstateModelVersion = pSubKey->GetInt( "animset_version", CURRENT_ANIMSTATE_VERSION );
						break;
					}
				}
			}
		}

		contract_assert( ( m_nAnimstateModelVersion == CURRENT_ANIMSTATE_VERSION ) && "Animset version is out of date!" );

		// cache pose param indices
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_LEAN_YAW				].Init( m_pPlayer, "lean_yaw"				);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_SPEED					].Init( m_pPlayer, "speed"					);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_LADDER_SPEED			].Init( m_pPlayer, "ladder_speed"			);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_LADDER_YAW				].Init( m_pPlayer, "ladder_yaw"				);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_MOVE_YAW				].Init( m_pPlayer, "move_yaw"				);

		if ( m_nAnimstateModelVersion < 2 )
			m_tPoseParamMappings[ PLAYER_POSE_PARAM_RUN					].Init( m_pPlayer, "run"					);

		m_tPoseParamMappings[ PLAYER_POSE_PARAM_BODY_YAW				].Init( m_pPlayer, "body_yaw"				);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_BODY_PITCH				].Init( m_pPlayer, "body_pitch"				);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_DEATH_YAW				].Init( m_pPlayer, "death_yaw"				);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_STAND					].Init( m_pPlayer, "stand"					);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_JUMP_FALL				].Init( m_pPlayer, "jump_fall"				);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_STAND_IDLE	].Init( m_pPlayer, "aim_blend_stand_idle"	);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_CROUCH_IDLE	].Init( m_pPlayer, "aim_blend_crouch_idle"	);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_STRAFE_DIR				].Init( m_pPlayer, "strafe_yaw"				);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_STAND_WALK	].Init( m_pPlayer, "aim_blend_stand_walk"	);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_STAND_RUN		].Init( m_pPlayer, "aim_blend_stand_run"	);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_CROUCH_WALK	].Init( m_pPlayer, "aim_blend_crouch_walk"	);

		if ( m_nAnimstateModelVersion > 0 )
		{
			m_tPoseParamMappings[ PLAYER_POSE_PARAM_MOVE_BLEND_WALK			].Init( m_pPlayer, "move_blend_walk"	);
			m_tPoseParamMappings[ PLAYER_POSE_PARAM_MOVE_BLEND_RUN			].Init( m_pPlayer, "move_blend_run"		);
			m_tPoseParamMappings[ PLAYER_POSE_PARAM_MOVE_BLEND_CROUCH_WALK	].Init( m_pPlayer, "move_blend_crouch"	);
		}

		m_cachedModelIndex = m_pPlayer->GetModelIndex();

		//// precache the sequences we'll be using for movement
		//if ( m_cachedModelIndex > 0 && m_pPlayer->GetModelPtr() )
		//{
		//	bool bSuccess = true;
		//	bSuccess = bSuccess && PopulateSequenceCache( "move",					ANIMCACHE_MOVE );
		//	bSuccess = bSuccess && PopulateSequenceCache( "additive_posebreaker",	ANIMCACHE_ALIVELOOP );
		//	bSuccess = bSuccess && PopulateSequenceCache( "jump",					ANIMCACHE_JUMP );
		//	bSuccess = bSuccess && PopulateSequenceCache( "jump_moving",			ANIMCACHE_JUMP_MOVING );
		//	bSuccess = bSuccess && PopulateSequenceCache( "fall",					ANIMCACHE_FALL );
		//	bSuccess = bSuccess && PopulateSequenceCache( "land_light",				ANIMCACHE_LANDLIGHT );
		//	bSuccess = bSuccess && PopulateSequenceCache( "land_heavy",				ANIMCACHE_LANDHEAVY );
		//	bSuccess = bSuccess && PopulateSequenceCache( "land_light_crouched",	ANIMCACHE_LANDLIGHT_CROUCHED );
		//	bSuccess = bSuccess && PopulateSequenceCache( "land_heavy_crouched",	ANIMCACHE_LANDHEAVY_CROUCHED );
		//	bSuccess = bSuccess && PopulateSequenceCache( "ladder_climb",			ANIMCACHE_LADDERCLIMB );
		//	bSuccess = bSuccess && PopulateSequenceCache( "lean",					ANIMCACHE_LEAN );
		//	return bSuccess;
		//}
	}
	return m_cachedModelIndex > 0;
}

//----------------------------------------------------------------------------------
int animstate_pose_param_cache_t::GetIndex( void )
{
	contract_assert( m_bInitialized );
	return m_nIndex;
}

float animstate_pose_param_cache_t::GetValue( CCSPlayer* pPlayer )
{
	if ( !m_bInitialized )
	{
		Init( pPlayer, m_szName );
	}
	if ( m_bInitialized && pPlayer )
	{
		return pPlayer->GetPoseParameter( m_nIndex );
	}
	return 0;
}

void animstate_pose_param_cache_t::SetValue( CCSPlayer* pPlayer, float flValue )
{
	if ( !m_bInitialized )
	{
		Init( pPlayer, m_szName );
	}
	if ( m_bInitialized && pPlayer )
	{
		MDLCACHE_CRITICAL_SECTION();
		pPlayer->SetPoseParameter( m_nIndex, flValue );
	}
}

bool animstate_pose_param_cache_t::Init( CCSPlayer* pPlayer, const char* szPoseParamName )
{
	MDLCACHE_CRITICAL_SECTION();
	m_szName = szPoseParamName;
	m_nIndex = pPlayer->LookupPoseParameter( szPoseParamName );
	if ( m_nIndex != -1 )
	{
		m_bInitialized = true;
	}
	return m_bInitialized;
}
//----------------------------------------------------------------------------------

animstate_pose_param_cache_t::animstate_pose_param_cache_t()
{
	m_bInitialized = false;
	m_nIndex = -1;
	m_szName = "";
}

procedural_foot_t::procedural_foot_t()
{
	m_vecPosAnim.Init();
	m_vecPosAnimLast.Init();
	m_vecPosPlant.Init();
	m_vecPlantVel.Init();
	m_flLockAmount = 0;
	m_flLastPlantTime = gpGlobals->curtime;
}

void aimmatrix_transition_t::UpdateTransitionState( bool bStateShouldBeValid, float flTimeInterval, float flSpeed)
{
	if ( bStateShouldBeValid )
	{
		m_flDurationStateHasBeenInValid = 0;
		m_flDurationStateHasBeenValid += flTimeInterval;
		if ( m_flDurationStateHasBeenValid >= m_flHowLongToWaitUntilTransitionCanBlendIn )
		{
			m_flBlendValue = Approach( 1, m_flBlendValue, flSpeed );
		}
	}
	else
	{
		m_flDurationStateHasBeenValid = 0;
		m_flDurationStateHasBeenInValid += flTimeInterval;
		if ( m_flDurationStateHasBeenInValid >= m_flHowLongToWaitUntilTransitionCanBlendOut )
		{
			m_flBlendValue = Approach( 0, m_flBlendValue, flSpeed );
		}
	}
}

#undef bonesnapshot_def
#undef FOOT_LOCK_THRESHOLD
#undef CSGO_ANIM_MAX_VEL_LIMIT

class CNewParticleEffect;

using namespace ::ks::reflect;

IMPLEMENT_REFLECT_CLIENTCLASS( CDecoyProjectile, DT_DecoyProjectile, CDecoyProjectile )

//--------------------------------------------------------------------------------------------------------
void CDecoyProjectile::OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect )
{
	if ( FStrEq( pszParticleName, "weapon_decoy_ground_effect" ) )
	{
		m_decoyParticleEffect = pNewParticleEffect;
	}
}

//--------------------------------------------------------------------------------------------------------
void CDecoyProjectile::OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect )
{
	if ( m_decoyParticleEffect == pParticleEffect )
	{
		m_decoyParticleEffect = nullptr;
	}
}

//--------------------------------------------------------------------------------------------------------
bool CDecoyProjectile::Simulate( void )
{
	// we are still moving
	if ( GetAbsVelocity().Length() > 0.1f )
	{
		return true;
	}

	if ( !m_decoyParticleEffect.IsValid() )
	{
		DispatchParticleEffect( "weapon_decoy_ground_effect", PATTACH_POINT_FOLLOW, this, "Wick" );
	}
	else
	{
		m_decoyParticleEffect->SetSortOrigin( GetAbsOrigin() );
		m_decoyParticleEffect->SetNeedsBBoxUpdate( true );
	}

	BaseClass::Simulate();
	return true;
}
