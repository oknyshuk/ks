// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include <math.h>
#include "datacache/imdlcache.h"
#include "declare_class.h"
#include "edict.h"
#include "networkvar.h"
#include "studio.h"
#include "../BaseAnimatingOverlay.h"
#include "../ai_activity.h"
#include "collisionutils.h"
#include "gametypes.h"
#include "eiface.h"
#include "responserules/response_types.h"
#include "server_class.h"
#include "vphysics_interface.h"
#include "../ai_criteria.h"
#include "../entity_dispatch.h"
#include "../entity_dispatch_list.h"
#include "../entity_kinds_dump.h"
#include "../reflect_sendtable.h"
#include "../util.h"
#include "datamodel/dmattributetypes.h"
#include "inetchannelinfo.h"
#include "../vote_controller.h"
#include "datamap.h"
module ks.server;

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
import :fish;
import ks.game.ai_activity;
import :vscript;
import :sharedvar;
import :player;
import :cstrike15.cs_player;
import :cstrike15.cs_playeranimstate;
import :cstrike15.hostage.cs_simple_hostage;
import :baseentity;
import :nav_4;
import ks.game.shareddefs;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player_2;
import :entity_2;
import ks.tier1.interfaces;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import ks.core.scratcharena;
import ks.math.ssemath;
import ks.game.mapentities_shared;
import ks.core.random;
import :econ.econ_item_schema_minimal;
import :cstrike15.cs_team;
import :entity_dispatch;
import :entity_kind;
import ks.game.cstrike15.cstrike15_gcconstants;
import ks.tier1.fmtstr;
import ks.tier1.keyvalues;
import ks.core.platform;
import ks.tier1.playerid;
import ks.tier1.checksum_crc;

class CAnimationLayer;
class CBaseAnimatingOverlay;
class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CCSPlayer;
class CHostage;
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
		contract_assert( dynamic_cast<CHostage*>( m_pOuter ) );
		CHostage *me = (CHostage*)m_pOuter;
		
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
		pLayer->m_fFlags = 0;
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
	pLayer->m_fFlags |= ANIM_LAYER_ACTIVE; 
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
			pLayer->m_fFlags |= ANIM_LAYER_ACTIVE; 

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

			// [dwenger] Needed for fun-fact implementation
			CCS_GameStats.IncrementStat( m_pPlayer, CSSTAT_TOTAL_JUMPS, 1 );

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

class CBasePlayer;
class CCSPlayer;
class CPlayerMove;
class CUserCmd;

using namespace ::ks::reflect;

static CMoveData g_MoveData;
CMoveData *g_pMoveData = &g_MoveData;

IPredictionSystem *IPredictionSystem::g_pPredictionSystems = nullptr;

//-----------------------------------------------------------------------------
// Sets up the move data for TF2
//-----------------------------------------------------------------------------
class CCSPlayerMove : public CPlayerMove
{
DECLARE_CLASS( CCSPlayerMove, CPlayerMove );

public:
	virtual void	SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );
	virtual void	FinishMove( CBasePlayer *player, CUserCmd *ucmd, CMoveData *move );
};

// PlayerMove Interface
static CCSPlayerMove g_PlayerMove;

//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
CPlayerMove *PlayerMove()
{
	return &g_PlayerMove;
}

//-----------------------------------------------------------------------------
// Purpose: This is called pre player movement and copies all the data necessary
//          from the player for movement. (Server-side, the client-side version
//          of this code can be found in prediction.cpp.)
//-----------------------------------------------------------------------------
void CCSPlayerMove::SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move )
{
	player->AvoidPhysicsProps( ucmd );

	BaseClass::SetupMove( player, ucmd, pHelper, move );

	
}

//-----------------------------------------------------------------------------
// Purpose: This is called post player movement to copy back all data that
//          movement could have modified and that is necessary for future
//          movement. (Server-side, the client-side version of this code can 
//          be found in prediction.cpp.)
//-----------------------------------------------------------------------------
void CCSPlayerMove::FinishMove( CBasePlayer *player, CUserCmd *ucmd, CMoveData *move )
{
	// Call the default FinishMove code.
	BaseClass::FinishMove( player, ucmd, move );

	

	CCSPlayer *pPlayer = ToCSPlayer( player );

	// Reset these... they get reset each frame.
	pPlayer->m_bInBombZone = false;
	pPlayer->m_bInBombZoneTrigger = false;
	pPlayer->m_bInBuyZone = false;
	pPlayer->m_bInHostageRescueZone = false;
	pPlayer->m_bInNoDefuseArea = false;
}

class CBaseEntity;
class CCSPlayer;

using namespace ::ks::reflect;

// Purpose: Note: This code integrated then adapted from TF:
//			//ValveGames/staging/src/game/server/tf/tf_pushentity.cpp

class CCSPhysicsPushEntities : public CPhysicsPushedEntities
{
public:

	DECLARE_CLASS( CCSPhysicsPushEntities, CPhysicsPushedEntities );

	// Constructor/Destructor.
	CCSPhysicsPushEntities();
	~CCSPhysicsPushEntities();

protected:

	// Speculatively checks to see if all entities in this list can be pushed
	virtual bool SpeculativelyCheckRotPush( const RotatingPushMove_t &rotPushMove, CBaseEntity *pRoot ) override;
	virtual bool SpeculativelyCheckLinearPush( const Vector &vecAbsPush ) override;
	virtual void FinishRotPushedEntity( CBaseEntity *pPushedEntity, const RotatingPushMove_t &rotPushMove ) override;
	
private:

	bool RotationPushCSPlayer( PhysicsPushedInfo_t &info, const Vector &vecAbsPush, const RotatingPushMove_t &rotPushMove, bool bRotationalPush, CBaseEntity *pRoot );
	bool RotationCheckPush( PhysicsPushedInfo_t &info, bool bIgnoreTeammates );
	bool LinearPushCSPlayer( PhysicsPushedInfo_t &info, const Vector &vecAbsPush, bool bRotationalPush );
	bool LinearCheckPush( PhysicsPushedInfo_t &info, bool bIgnoreTeammates );
	void EnsureValidPushWhileRiding( CBaseEntity *pBlocker, CBaseEntity *pPusher );

	bool IsPlayerAABBIntersetingPusherOBB( CBaseEntity *pEntity, CBaseEntity *pRootEntity );

	void MovePlayer( CBaseEntity *pBlocker, PhysicsPushedInfo_t &info, float flMoveScale, bool bPusherIsTrain, bool bIgnoreTeammates );
	void FindNewPushDirection( Vector &vecCurrent, Vector &vecNormal, Vector &vecOutput );

	float m_flPushDist;
	Vector	m_vecPushVector;
};

CCSPhysicsPushEntities s_CSPushedEntities;
CPhysicsPushedEntities *g_pPushedEntities = &s_CSPushedEntities;

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CCSPhysicsPushEntities::CCSPhysicsPushEntities()
{

}

//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CCSPhysicsPushEntities::~CCSPhysicsPushEntities()
{

}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSPhysicsPushEntities::SpeculativelyCheckRotPush( const RotatingPushMove_t &rotPushMove, CBaseEntity *pRoot )
{

	Vector vecAbsPush( 0.0f, 0.0f, 0.0f );
	m_nBlocker = -1;
	int nMovedCount = std::ssize( m_rgMoved );
	for ( int i = ( nMovedCount - 1 ); i >= 0; --i )
	{
		// Is the entity and CS Player?
		CCSPlayer *pCSPlayer = nullptr;
		bool bPusherIsTrain = false;
		if ( m_rgMoved[i].m_pEntity && m_rgMoved[i].m_pEntity->IsPlayer() )
		{
			pCSPlayer = ToCSPlayer( m_rgMoved[i].m_pEntity );
			CBaseEntity* pPusher = m_rgPusher[ 0 ].m_pEntity->GetRootMoveParent();
			bPusherIsTrain = pPusher && pPusher->IsBaseTrain();
		}

		// Special code to move the player away from the func_train.
		// Only do this if it's a train pushing a player--otherwise use base class. 
		if ( pCSPlayer && bPusherIsTrain )
		{
			// Rotationally push the player!
			ComputeRotationalPushDirection( m_rgMoved[ i ].m_pEntity, rotPushMove, &vecAbsPush, pRoot );
			RotationPushCSPlayer( m_rgMoved[i], vecAbsPush, rotPushMove, true, pRoot );
		}
		else
		{
			// Keep this in sync with BaseClass::SpeculativelyCheckRotPush
			ComputeRotationalPushDirection( m_rgMoved[i].m_pEntity, rotPushMove, &vecAbsPush, pRoot );
			if ( !SpeculativelyCheckPush( m_rgMoved[i], vecAbsPush, true, pRoot ) )
			{
				m_nBlocker = i;
				return false;
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Speculatively checks to see if all entities in this list can be pushed
//-----------------------------------------------------------------------------
bool CCSPhysicsPushEntities::SpeculativelyCheckLinearPush( const Vector &vecAbsPush )
{

	m_nBlocker = -1;
	int nMovedCount = std::ssize( m_rgMoved );
	for ( int i = ( nMovedCount - 1 ); i >= 0; --i )
	{
		// Is the entity and CS Player?
		CCSPlayer *pCSPlayer = nullptr;
		bool bPusherIsTrain = false;
		if ( m_rgMoved[i].m_pEntity && m_rgMoved[i].m_pEntity->IsPlayer() )
		{
			pCSPlayer = ToCSPlayer( m_rgMoved[i].m_pEntity );
			CBaseEntity* pPusher = m_rgPusher[0].m_pEntity->GetRootMoveParent();
 			bPusherIsTrain = pPusher && pPusher->IsBaseTrain();
		}

		// Special code to move the player away from the func_train.
		// Only do this if it's a train pushing a player--otherwise use base class. 
		if ( pCSPlayer && bPusherIsTrain )
		{
			// Linearly push the player!
 			LinearPushCSPlayer( m_rgMoved[i], vecAbsPush, false );
		}
		else
		{
			// Keep this in sync with BaseClass::SpeculativelyCheckLinearPush
			if ( !SpeculativelyCheckPush( m_rgMoved[i], vecAbsPush, false, nullptr ) )
			{
				m_nBlocker = i;
				return false;
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSPhysicsPushEntities::RotationPushCSPlayer( PhysicsPushedInfo_t &info, const Vector &vecAbsPush, const RotatingPushMove_t &rotPushMove, bool bRotationalPush, CBaseEntity* pRoot )
{
	const bool cbIgnoreTeammates = !CSGameRules() || !CSGameRules()->IsTeammateSolid();
	contract_assert( cbIgnoreTeammates ); // This code doesn't behave if teammates are solid.

	// Clear out the collision entity so that if we early out we don't send bogus collision data to the physics system.
	info.m_Trace.m_pEnt = nullptr;

	// Look into doing a full engine->CM_Clear( trace)

	// Get the player.
	CCSPlayer *pPlayer = ToCSPlayer( info.m_pEntity );
	if ( !pPlayer )
		return false;

	info.m_vecStartAbsOrigin = pPlayer->GetAbsOrigin();

	// Get the player collision data.
	CCollisionProperty *pCollisionPlayer = info.m_pEntity->CollisionProp();
	if ( !pCollisionPlayer )
		return false;

	// Find the root object if in hierarchy.
	CBaseEntity *pRootEntity = m_rgPusher[0].m_pEntity->GetRootMoveParent();
	if ( !pRootEntity )
		return false;

	// This code doesn't at all match the code in AvoidPushawayProps, which is a bummer. It'd be
	// great if this just got rolled into that. Unfortunately, doing so would also require 
	// making trains predictive--they are not currently.
	if ( !pPlayer->GetGroundEntity() || pPlayer->GetGroundEntity()->GetRootMoveParent() != pRootEntity )
	{
		Vector vMinPushAway = vecAbsPush;
		m_flPushDist = VectorNormalize( vMinPushAway );
		m_vecPushVector = vMinPushAway;

		contract_assert( !m_vecPushVector.IsZero() ); // Is our push vector legit?
	}
	else
	{
		SpeculativelyCheckPush( info, vecAbsPush, true, pRoot, cbIgnoreTeammates );
		EnsureValidPushWhileRiding( pPlayer, pRootEntity );
	}

	return RotationCheckPush( info, cbIgnoreTeammates );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSPhysicsPushEntities::RotationCheckPush( PhysicsPushedInfo_t &info, bool bIgnoreTeammates )
{
	ks::ScratchArena<> scratch;
	// Get the blocking and pushing entities.
	CBaseEntity *pBlocker = info.m_pEntity;
	CBaseEntity *pRootEntity = m_rgPusher[0].m_pEntity->GetRootMoveParent();
	if ( !pBlocker || !pRootEntity )
		return true;

	int *pPusherHandles = scratch.Alloc<int>( std::ssize( m_rgPusher ) );
	UnlinkPusherList( pPusherHandles );
	for ( int iPushTry = 0; iPushTry < 3; ++iPushTry )
	{
		MovePlayer( pBlocker, info, 0.35f, pRootEntity->IsBaseTrain(), bIgnoreTeammates );
		if ( IsPushedPositionValid( pBlocker, bIgnoreTeammates ) )
			break;
	}
	RelinkPusherList( pPusherHandles );

	// Is the blocked ground the push entity?
	info.m_bPusherIsGround = false;
	if ( pBlocker->GetGroundEntity() && pBlocker->GetGroundEntity()->GetRootMoveParent() == m_rgPusher[0].m_pEntity )
	{
		info.m_bPusherIsGround = true;
	}

	// Check to see if the player is in a good spot and attempt a move again if not - but only if it isn't being ridden on.
	if ( !IsPushedPositionValid( pBlocker, bIgnoreTeammates ) )
	{
		// Try again is the player is still blocked.
		DevMsg( 2, "Pushing rotation hard!\n" );
		UnlinkPusherList( pPusherHandles );
		MovePlayer( pBlocker, info, 1.0f, pRootEntity->IsBaseTrain(), bIgnoreTeammates );
		RelinkPusherList( pPusherHandles );
	}

	// The player will never stop a train from moving in CS.
	info.m_bBlocked = false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSPhysicsPushEntities::LinearPushCSPlayer( PhysicsPushedInfo_t &info, const Vector &vecAbsPush, bool bRotationalPush )
{
	const bool cbIgnoreTeammates = !CSGameRules() || !CSGameRules()->IsTeammateSolid();
	contract_assert( cbIgnoreTeammates ); // This code doesn't behave if teammates are solid.

	// Clear out the collision entity so that if we early out we don't send bogus collision data to the physics system.
	info.m_Trace.m_pEnt = nullptr;

	// Get the player.
	CCSPlayer *pPlayer = ToCSPlayer( info.m_pEntity );
	if ( !pPlayer )
		return false;

	info.m_vecStartAbsOrigin = pPlayer->GetAbsOrigin();

	// Get the player collision data.
	CCollisionProperty *pCollisionPlayer = info.m_pEntity->CollisionProp();
	if ( !pCollisionPlayer )
		return false;

	// Find the root object if in hierarchy.
	CBaseEntity *pRootEntity = m_rgPusher[0].m_pEntity->GetRootMoveParent();
	if ( !pRootEntity )
		return false;

	// Get the pusher collision data.
	CCollisionProperty *pCollisionPusher = pRootEntity->CollisionProp();
	if ( !pCollisionPusher )
		return false;

	if ( !pPlayer->GetGroundEntity() || pPlayer->GetGroundEntity()->GetRootMoveParent() != pRootEntity )
	{
		m_vecPushVector = vecAbsPush;
		m_flPushDist = VectorNormalize( m_vecPushVector );
	}
	else
	{
		// Try to get the base class first. 
		SpeculativelyCheckPush( info, vecAbsPush, false, nullptr, cbIgnoreTeammates );
 		EnsureValidPushWhileRiding( pPlayer, pRootEntity );
	}

	return LinearCheckPush( info, cbIgnoreTeammates );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSPhysicsPushEntities::LinearCheckPush( PhysicsPushedInfo_t &info, bool bIgnoreTeammates )
{
	ks::ScratchArena<> scratch;
	// Get the blocking and pushing entities.
	CBaseEntity *pBlocker = info.m_pEntity;
	CBaseEntity *pRootEntity = m_rgPusher[0].m_pEntity->GetRootMoveParent();
	if ( !pBlocker || !pRootEntity )
		return true;

	// Unlink the pusher from the spatial partition and attempt a player move.
	int *pPusherHandles = scratch.Alloc<int>( std::ssize( m_rgPusher ) );
	UnlinkPusherList( pPusherHandles );
	MovePlayer( pBlocker, info, 1.0f, pRootEntity->IsBaseTrain(), bIgnoreTeammates );
	RelinkPusherList( pPusherHandles );

	// Is the pusher the ground entity the blocker is standing on?
	info.m_bPusherIsGround = false;
	if ( pBlocker->GetGroundEntity() && pBlocker->GetGroundEntity()->GetRootMoveParent() == m_rgPusher[0].m_pEntity )
	{
		info.m_bPusherIsGround = true;
	}

	// Check to see if the player is in a good spot and attempt a move again if not - but only if it isn't being ridden on.
	if ( !info.m_bPusherIsGround && !IsPushedPositionValid( pBlocker, bIgnoreTeammates ) )
	{
		// Try again is the player is still blocked.
		DevMsg( 2, "Pushing linear hard!\n" );
		UnlinkPusherList( pPusherHandles );
		MovePlayer( pBlocker, info, 1.0f, pRootEntity->IsBaseTrain(), bIgnoreTeammates );
		RelinkPusherList( pPusherHandles );
	}

	// The player will never stop a train from moving in CS.
	info.m_bBlocked = false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: When riding atop a vehicle, try to ensure that the final push vector and 
// push distance will land us in a valid location. 
//-----------------------------------------------------------------------------
void CCSPhysicsPushEntities::EnsureValidPushWhileRiding( CBaseEntity *pBlocker, CBaseEntity *pPusher )
{
	const bool cbIgnoreTeammates = !CSGameRules() || !CSGameRules()->IsTeammateSolid();
	contract_assert( cbIgnoreTeammates ); // This code doesn't behave if teammates are solid.
	
	m_vecPushVector.Zero();
	m_flPushDist = 0.0f;

	// Do we still have a collision? 
	if ( IsPushedPositionValid( pBlocker, cbIgnoreTeammates ) )
		return;
	
	const float cMaxDistToLookForPlacement = 72;
	// Try nudging them upwards a bit.
	if ( FindValidLocationUpwards( &m_flPushDist, pBlocker, cMaxDistToLookForPlacement, 1.1 ) )
	{
		m_vecPushVector.z = 1.0f;
	}
	else
	{
		// Try to push the player backwards along the direction of travel of the vehicle (and also up a bit)?
		Vector vBackwardsAndUp( 0, 0, 1 );
		Vector vTraceEndpoint = pPusher->GetAbsVelocity();
		vTraceEndpoint.x = -vTraceEndpoint.x;
		vTraceEndpoint.y = -vTraceEndpoint.y;
		vTraceEndpoint.z = std::sqrt( vTraceEndpoint.x * vTraceEndpoint.x + vTraceEndpoint.y * vTraceEndpoint.y );
		vTraceEndpoint = vTraceEndpoint.Normalized() * cMaxDistToLookForPlacement;
		vTraceEndpoint += pBlocker->GetAbsOrigin();

		Vector vDelta;
		if ( FindValidLocationAlongVector( &vDelta, pBlocker, vTraceEndpoint, 1.1 ) )
		{
			m_vecPushVector = vDelta;
			m_flPushDist = VectorNormalize( m_vecPushVector );
		}
		else
		{
			contract_assert( !"Failed to find a location upwards or backwards for a blocker, sadness!" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSPhysicsPushEntities::IsPlayerAABBIntersetingPusherOBB( CBaseEntity *pEntity, CBaseEntity *pRootEntity )
{
	// Get the player.
	CCSPlayer *pPlayer = ToCSPlayer( pEntity );
	if ( !pPlayer )
		return false;

	// Get the player collision data.
	CCollisionProperty *pCollisionPlayer = pEntity->CollisionProp();
	if ( !pCollisionPlayer )
		return false;

	// Get the pusher collision data.
	CCollisionProperty *pCollisionPusher = pRootEntity->CollisionProp();
	if ( !pCollisionPusher )
		return false;

	// Do we have a collision.
	 return IsOBBIntersectingOBB( pCollisionPlayer->GetCollisionOrigin(), pCollisionPlayer->GetCollisionAngles(), pCollisionPlayer->OBBMins(), pCollisionPlayer->OBBMaxs(), 
		pCollisionPusher->GetCollisionOrigin(), pCollisionPusher->GetCollisionAngles(), pCollisionPusher->OBBMins(), pCollisionPusher->OBBMaxs(), 
		0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPhysicsPushEntities::FindNewPushDirection( Vector &vecCurrent, Vector &vecNormal, Vector &vecOutput )
{
	// Determine how far along plane to slide based on incoming direction.
	float flBackOff = DotProduct( vecCurrent, vecNormal );

	for ( int iAxis = 0; iAxis < 3; ++iAxis )
	{
		float flDelta = vecNormal[iAxis] * flBackOff;
		vecOutput[iAxis] = vecCurrent[iAxis] - flDelta; 
	}

	// iterate once to make sure we aren't still moving through the plane
	float flAdjust = DotProduct( vecOutput, vecNormal );
	if( flAdjust < 0.0f )
	{
		vecOutput -= ( vecNormal * flAdjust );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPhysicsPushEntities::MovePlayer( CBaseEntity *pBlocker, PhysicsPushedInfo_t &info, float flMoveScale, bool bPusherIsTrain, bool bIgnoreTeammates )
{
	// Find out how far we still need to move.
	float flFractionLeft = 1.0f;
	float flNewDist = m_flPushDist *flMoveScale;
	Vector vecPush = m_vecPushVector;

	// Find a new push vector.
	Vector vecStart = pBlocker->GetAbsOrigin();
	Vector logVecStart = vecStart;
	Vector logVecEnd = vecStart;
	int iSteps = 0;

	vecStart.z += 4.0f;
	for ( int iTest = 0; iTest < 4; ++iTest )
	{
		// Clear the trace entity.
		Vector vecEnd = pBlocker->GetAbsOrigin() + ( flNewDist * vecPush );
		TraceBlockerEntity( pBlocker, vecStart, vecEnd, bIgnoreTeammates, &info.m_Trace );

		if ( info.m_Trace.fraction > 0.0f )
		{
			pBlocker->SetAbsOrigin( info.m_Trace.endpos );
			logVecEnd = info.m_Trace.endpos;
			iSteps = iTest + 1;
		}

		if ( info.m_Trace.fraction == 1.0f || !info.m_Trace.Ent<CBaseEntity>() )
			break;

		// New test distance and position.
		flFractionLeft = 1.0f - info.m_Trace.fraction;
		flNewDist = flFractionLeft * flNewDist;
		flNewDist = flNewDist * ( 1.0f + ( 1.0f - fabs( info.m_Trace.plane.normal.Dot( vecPush ) ) ) );

		// Find the new push direction.
		Vector vecTmp;
		FindNewPushDirection( vecPush, info.m_Trace.plane.normal, vecTmp );
		VectorCopy( vecTmp, vecPush );
	}

	Vector finalPushVec = logVecEnd - logVecStart;
	DevMsg( 2, "Pushed player by %.2f over %d steps (push vector: %.2f, %.2f, %.2f)\n", finalPushVec.Length(), iSteps, finalPushVec.x, finalPushVec.y, finalPushVec.z );
}

//-----------------------------------------------------------------------------
// Causes all entities in the list to touch triggers from their prev position
//-----------------------------------------------------------------------------
void CCSPhysicsPushEntities::FinishRotPushedEntity( CBaseEntity *pPushedEntity, const RotatingPushMove_t &rotPushMove )
{
	if ( !pPushedEntity->IsPlayer() )
	{
		QAngle angles = pPushedEntity->GetAbsAngles();

		// only rotate YAW with pushing.  Freely rotateable entities should either use VPHYSICS
		// or be set up as children
		angles.y += rotPushMove.amove.y;
		pPushedEntity->SetAbsAngles( angles );
	}
}

class CEconItemSchema;

using namespace ::ks::reflect;

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
	m_NumTModels = 0;
	m_NumCTModels = 0;
	m_NumModels = 0;
	m_nNextClassT = -1;
	m_nNextClassCT = -1;

	m_mapName[0] = 0;

	for ( int i=0; i<CS_MAX_PLAYER_MODELS; ++i )
	{
		m_ClassModelPaths[i][0] = 0;
		m_ClassNames[i][0] = 0;
	}

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

bool PlayerModelInfo::IsTClass( int i )
{
	// The 0 model is class NONE
	return (i >= GetFirstTClass() && i <= GetLastTClass() );
} 

bool PlayerModelInfo::IsCTClass( int i )
{
	// The 0 model is class NONE
	return (i >= GetFirstCTClass() && i <= GetLastCTClass() );
} 

const char *PlayerModelInfo::GetClassName( int classID )
{
	contract_assert( ( classID >= GetFirstClass() && classID <= GetLastClass() ) && "Invalid class ID for models loaded.\n " );
	return m_ClassNames[classID-1];
}

const char *PlayerModelInfo::GetClassModelPath( int classID )
{
	contract_assert( ( classID >= GetFirstClass() && classID <= GetLastClass() ) && "Invalid class ID for models loaded.\n " );
	return m_ClassModelPaths[classID-1];
}

int PlayerModelInfo::GetNextClassForTeam( int team )
{
	if ( team == TEAM_TERRORIST )
	{
		if ( m_nNextClassT == -1 )
		{
			m_nNextClassT = RandomInt( GetFirstTClass(), GetLastTClass() );
		}

		++m_nNextClassT;

		if ( m_nNextClassT > GetLastTClass() )
		{
			m_nNextClassT = GetFirstTClass();
		}

		return m_nNextClassT;
	}
	else if ( team == TEAM_CT )
	{
		if ( m_nNextClassCT == -1 )
		{
			m_nNextClassCT = RandomInt( GetFirstCTClass(), GetLastCTClass() );	
		}

		++m_nNextClassCT;

		if ( m_nNextClassCT > GetLastCTClass() )
		{
			m_nNextClassCT = GetFirstCTClass();
		}

		return m_nNextClassCT;
	}
	else
	{
		return GetFirstClass();
	}
}

void PlayerModelInfo::InitializeForCurrentMap( void )
{
	const char *mapName = ( gpGlobals ? STRING( gpGlobals->mapname ) : nullptr );
	if ( mapName && 
		 V_stricmp( m_mapName, mapName ) == 0 )
	{
		// We have already cached the model information for this map.
		return;
	}

	if ( mapName )
	{
		V_strcpy( m_mapName, mapName );
	}
	else
	{
		m_mapName[0] = '\0';
	}

	m_NumTModels = 0;
	m_NumCTModels = 0;
	m_NumModels = 0;
	m_nNextClassCT = -1;
	m_nNextClassT = -1;

	bool bUseCosmetics = false;

	// If the custom character system is enabled, we should load invisible skeleton players that will be eventually clothed.
	if ( bUseCosmetics )
	{
		//FIXME: custom characters have no audio persona; that is they don't speak yet (who would they sound like?)

		AddModel( g_customizationModelT );
		m_NumTModels = 1;
		AddModel( g_customizationModelCT );
		m_NumCTModels = 1;

		DevMsg( "Using script-defined character customization definitions\n" );
	}
	else
	{

		// Add the terrorist models.
		// NOTE: Terrorists must be loaded first since it is assumed they are first in the list of models.
		const std::vector< std::string > *pTModelNames = g_pGameTypes->GetTModelsForMap( m_mapName );
		if ( pTModelNames )
		{
			for ( const std::string &tModelName : *pTModelNames )
			{
				const char *modelName = tModelName.c_str();
				if ( modelName )
				{
					AddModel( modelName );
				}
			}

			m_NumTModels = m_NumModels;
		}

		// Add the default terrorist model, if no models were loaded.
		if ( m_NumTModels == 0 )
		{
			Warning( "PlayerModelInfo: missing terrorist models for map %s. Adding the default model %s.\n", m_mapName, g_defaultTModel );
			AddModel( g_defaultTModel );
			m_NumTModels = 1;
		}

		// Add the counter-terrorist models.
		const std::vector< std::string > *pCTModelNames = g_pGameTypes->GetCTModelsForMap( m_mapName );
		if ( pCTModelNames )
		{
			for ( const std::string &ctModelName : *pCTModelNames )
			{
				const char *modelName = ctModelName.c_str();
				if ( modelName )
				{
					AddModel( modelName );
				}
			}

			m_NumCTModels = m_NumModels - m_NumTModels;
		}

		// Add the default counter-terrorist model, if no models were loaded.
		if ( m_NumCTModels == 0 )
		{
			Warning( "PlayerModelInfo: missing counter-terrorist models for map %s. Adding the default model %s.\n", m_mapName, g_defaultCTModel );
			AddModel( g_defaultCTModel );
			m_NumCTModels = 1;
		}

	}
	
}

void PlayerModelInfo::AddModel( const char *modelName )
{
	contract_assert( modelName );
	if ( !modelName ||
		 modelName[0] == '\0' )
	{
		return;
	}

	contract_assert( m_NumModels >= 0 && m_NumModels < CS_MAX_PLAYER_MODELS );
	if ( m_NumModels >= 0 && m_NumModels < CS_MAX_PLAYER_MODELS )
	{
		V_strcpy_safe( m_ClassNames[m_NumModels], modelName );

		if ( V_stristr( modelName, "scaffold" ) )
		{
			V_strcpy_safe( m_ClassModelPaths[m_NumModels], modelName );
		}
		else
		{
			char szCleanedPath[MAX_MODEL_STRING_SIZE];
			V_StrSubst( m_ClassNames[m_NumModels], "models/player", "", szCleanedPath, sizeof(szCleanedPath) );
			char szRedirectedPath[MAX_MODEL_STRING_SIZE];
			V_sprintf_safe( szRedirectedPath, "models/player/custom_player/legacy/%s", szCleanedPath );
			if ( !V_stristr( szRedirectedPath, ".mdl" ) )
				V_strcat_safe( szRedirectedPath, ".mdl" );

			if ( !filesystem->FileExists( szRedirectedPath, "MOD" ) )
			{
				contract_assert( false && "Verify map .kv: Player model doesn't exist: %s." );

				// Get a full path to the mdl file.
				if ( !V_stristr( modelName, "models/player" ) && !V_stristr( modelName, ".mdl" ) )
				{
					V_snprintf( m_ClassModelPaths[m_NumModels], sizeof( m_ClassModelPaths[m_NumModels] ), "models/player/%s.mdl", modelName );
				}
				else
				{
					V_snprintf( m_ClassModelPaths[m_NumModels], sizeof( m_ClassModelPaths[m_NumModels] ), "%s", modelName );
				}
			}
			else
			{
				V_strcpy_safe( m_ClassModelPaths[m_NumModels], szRedirectedPath );
			}
		}

		++m_NumModels;
	}
	else
	{
		Warning( "PlayerModelInfo: model count has exceeded the maximum (%d) for map \"%s\". Ignoring model %s.\n",
			CS_MAX_PLAYER_MODELS, m_mapName, modelName );
	}
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

class CBasePlayer;

using namespace ::ks::reflect;

// Purpose: Team management class. Contains all the details for a specific team

// Datatable
IMPLEMENT_REFLECT_SERVERCLASS( CCSTeam, DT_CSTeam )

LINK_ENTITY_TO_CLASS( cs_team_manager, CCSTeam );

//-----------------------------------------------------------------------------
// Purpose: Get a pointer to the specified TF team manager
//-----------------------------------------------------------------------------
CCSTeam *GetGlobalCSTeam( int iIndex )
{
	return (CCSTeam*)GetGlobalTeam( iIndex );
}

//-----------------------------------------------------------------------------
// Purpose: Needed because this is an entity, but should never be used
//-----------------------------------------------------------------------------
void CCSTeam::Init( const char *pName, int iNumber )
{
	BaseClass::Init( pName, iNumber );

	// Only detect changes every half-second.
	NetworkProp()->SetUpdateInterval( 0.75f );
}

CCSTeam::CCSTeam()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSTeam::~CCSTeam( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSTeam::Precache_Impl( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame
//-----------------------------------------------------------------------------
void CCSTeam::Think_Impl( void )
{
	BaseClass::Think_Impl();
}

void CCSTeam::OnRoundPreStart( void )
{
}

//------------------------------------------------------------------------------------------------------------------
// PLAYERS
//-----------------------------------------------------------------------------
// Purpose: Add the specified player to this team. Remove them from their current team, if any.
//-----------------------------------------------------------------------------
void CCSTeam::AddPlayer( CBasePlayer *pPlayer )
{
	BaseClass::AddPlayer( pPlayer );
}

//-----------------------------------------------------------------------------
// Purpose: Clean up the player's objects when they leave
//-----------------------------------------------------------------------------
void CCSTeam::RemovePlayer( CBasePlayer *pPlayer )
{
	BaseClass::RemovePlayer( pPlayer );
}

//------------------------------------------------------------------------------------------------------------------
// UTILITY FUNCS
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSTeam* CCSTeam::GetEnemyTeam()
{
	int iMyTeam = GetTeamNumber();

	if ( iMyTeam == TEAM_CT )
		return GetGlobalCSTeam( TEAM_TERRORIST );
	else if ( iMyTeam == TEAM_TERRORIST )
		return GetGlobalCSTeam( TEAM_CT );
	else
		return nullptr;
}

class CBaseEntity;
class CBasePlayer;
class CCSPlayer;

using namespace ::ks::reflect;

// Purpose: CS-specific things to vote on

//[tj]removing this to get voting to compile

extern ConVar mp_maxrounds;
extern ConVar mp_winlimit;

//-----------------------------------------------------------------------------
// Purpose: Base CS Issue
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Restart Round Issue
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_restart_game_allowed( side_registry(), "sv_vote_issue_restart_game_allowed", "0", FCVAR_RELEASE, "Can people hold votes to restart the game?" );
ConVar sv_arms_race_vote_to_restart_disallowed_after( side_registry(), "sv_arms_race_vote_to_restart_disallowed_after", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Arms Race gun level after which vote to restart is disallowed" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRestartGameIssue::ExecuteCommand( void )
{
	engine->ServerCommand( CFmtStr( "mp_restartgame 1;" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CRestartGameIssue::IsEnabled( void )
{
	if ( sv_vote_issue_restart_game_allowed.GetBool() )
	{
		// Vote to restart is allowed
		if ( sv_arms_race_vote_to_restart_disallowed_after.GetInt() > 0 )
		{
			// Need to test if a player has surpassed the maximum weapon progression
			if ( sv_arms_race_vote_to_restart_disallowed_after.GetInt() > CSGameRules()->GetMaxGunGameProgressiveWeaponIndex() )
			{
				// No players have surpassed the maximum weapon progression, so enable vote to restart
				return true;
			}
			else
			{
				// A player has surpassed the maximum weapon progression, so disable vote to restart
				return false;
			}
		}

		// No maximum weapon progression value is defined, so enable vote to restart
		return true;
	}

	// Vote to restart is not enabled
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CRestartGameIssue::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CRestartGameIssue::GetDisplayString( void )
{
	return "#SFUI_vote_restart_game";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CRestartGameIssue::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_restart_game";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRestartGameIssue::ListIssueDetails( CBasePlayer *pForWhom )
{
	if( !sv_vote_issue_restart_game_allowed.GetBool() )
		return;

	ListStandardNoArgCommand( pForWhom, GetTypeString() );
}

//-----------------------------------------------------------------------------
// Purpose: Kick Player Issue
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_kick_allowed( side_registry(), "sv_vote_issue_kick_allowed", "1", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE, "Can people hold votes to kick players from the server?" );
ConVar sv_vote_kick_ban_duration( side_registry(), "sv_vote_kick_ban_duration", "15", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE, "How long should a kick vote ban someone from the server? (in minutes)" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CKickIssue::ExecuteCommand( void )
{
	CCSPlayer *subject = nullptr;

	//[tj] Not applicable without TF-specific GC code
	//uint32 unReason = kVoteKickBanPlayerReason_Other;	
	uint32 unReason;

	ExtractDataFromDetails( m_szDetailsString, &subject, &unReason );

	if( subject )
	{
		// Check the cached value of player crashed state
		if ( ( sv_vote_kick_ban_duration.GetInt() > 0 ) && !m_bPlayerCrashed )
		{
			CSGameRules()->SendKickBanToGC( subject, k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_VotedOff );
			// don't roll the kick command into this, it will fail on a lan, where kickid will go through
			engine->ServerCommand( CFmtStr( "banid %d %d;", sv_vote_kick_ban_duration.GetInt(), subject->GetUserID() ) );
		}

		engine->ServerCommand( CFmtStr( "kickid_ex %d %d You have been voted off;", subject->GetUserID(), CSGameRules()->IsPlayingOffline() ? 0 : 1 ) );
	}
	else if ( !m_bPlayerCrashed && m_playerIDtoBan.IsValid() && ( sv_vote_kick_ban_duration.GetInt() > 0 ) )
	{
		// Player has already disconnected, but let's still ban him!
		CSGameRules()->SendKickBanToGCforAccountId( m_playerIDtoBan.GetAccountID(), k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_VotedOff );

		// Also enlist this user's SteamID in the banlist
		engine->ServerCommand( CFmtStr( "banid %d STEAM64BITID_%llu;", sv_vote_kick_ban_duration.GetInt(), m_playerIDtoBan.ConvertToUint64() ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CKickIssue::IsEnabled( void )
{
	return sv_vote_issue_kick_allowed.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CKickIssue::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if ( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if ( CSGameRules() && CSGameRules()->IsPlayingCooperativeGametype() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	if( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	CCSPlayer *pSubject = nullptr;
	ExtractDataFromDetails( pszDetails, &pSubject );
	if ( !pSubject || pSubject->IsBot() )
	{
		nFailCode = VOTE_FAILED_PLAYERNOTFOUND;
		return false;
	}

	if ( pSubject->IsReplay() || pSubject->IsHLTV() )
	{
		nFailCode = VOTE_FAILED_PLAYERNOTFOUND;
		return false;
	}
	
	CBaseEntity *pVoteCaller = UTIL_EntityByIndex( iEntIndex );
	if ( pVoteCaller )
	{
		CCSPlayer *pPlayer = ToCSPlayer( pVoteCaller );
		bool bCanKickVote = false;

		if ( pSubject && pPlayer )
		{
			int voterTeam = pPlayer->GetTeamNumber();
			int nSubjectTeam = pSubject->GetTeamNumber();

			bCanKickVote = ( voterTeam == TEAM_TERRORIST || voterTeam ==  TEAM_CT ) && ( voterTeam == nSubjectTeam || nSubjectTeam == TEAM_SPECTATOR );			
		} 	
		
		if ( pSubject )
		{
			bool bDeny = false;
			if ( !engine->IsDedicatedServer() )
			{
				if ( pSubject->entindex() == 1 )
				{
					bDeny = true;
				}
			}

			// This should only be set if we've successfully authenticated via rcon
			if ( pSubject->IsAutoKickDisabled() )
			{
				bDeny = true;
			}

			if ( bDeny )
			{
				nFailCode = VOTE_FAILED_CANNOT_KICK_ADMIN;
				return false;
			}
		}

		if ( !bCanKickVote )
		{
			// Can't initiate a vote to kick a player on the other team
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CKickIssue::OnVoteFailed( void )
{
	CBaseCSIssue::OnVoteFailed();

	CCSPlayer *subject = nullptr;
	uint32 unReason;// = kVoteKickBanPlayerReason_Other;
	ExtractDataFromDetails( m_szDetailsString, &subject, &unReason );

	//[tj]removing this to get voting to compile
	//NotifyGC( subject, false, unReason );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
 void CKickIssue::OnVoteStarted( void )
 {
	 CCSPlayer *pSubject = nullptr;
	 ExtractDataFromDetails(	m_szDetailsString, &pSubject );

	 // Auto vote 'No' for the person being kicked unless they are idle
	 // NOTE: Subtle. There's a problem with IsAwayFromKeyboard where if a player
	 // has idled and is taken over by a bot, IsAwayFromKeyboard will return false
	 // because the camera controller that takes over when idling will spoof 
	 // input messages, making it impossible to know if the player is moving his
	 // joystick or not. Being on TEAM_SPECTATOR, however, means you're idling,
	 // so we don't want to autovote NO if they are on team spectator
	 if ( m_pVoteController && pSubject && ( pSubject->GetTeamNumber( ) != TEAM_SPECTATOR ) && !pSubject->IsBot( ) )
	 {
		 m_pVoteController->TryCastVote( pSubject->entindex( ), "Option2" );
	 }

	 // Also when the vote starts, figure out if the player should not be banned
	 // if the player is crashed/hung. Need to perform the check here instead of
	 // inside Execute to prevent cheaters quitting before the vote finishes and
	 // not getting banned.
	 m_bPlayerCrashed = false;
	 if ( pSubject )
	 {
		 INetChannelInfo *pNetChanInfo = engine->GetPlayerNetInfo( pSubject->entindex() );
		 if ( !pNetChanInfo || pNetChanInfo->IsTimingOut() )
		 {
			 // don't ban the player
			 DevMsg( "Will not ban kicked player: net channel was idle for %.2f sec.\n", pNetChanInfo ? pNetChanInfo->GetTimeSinceLastReceived() : 0.0f );
			 m_bPlayerCrashed = true;
		 }

		 pSubject->GetPlayerID( &m_playerIDtoBan );
	 }
 }

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CKickIssue::GetDisplayString( void )
{
	uint32 unReason = kVoteKickBanPlayerReason_Other;
	ExtractDataFromDetails( m_szDetailsString, nullptr, &unReason );
	switch ( unReason )
	{
	case kVoteKickBanPlayerReason_Other:	return "#SFUI_vote_kick_player_other";
	case kVoteKickBanPlayerReason_Cheating:	return "#SFUI_vote_kick_player_cheating";
	case kVoteKickBanPlayerReason_Idle:		return "#SFUI_vote_kick_player_idle";
	case kVoteKickBanPlayerReason_Scamming:	return "#SFUI_vote_kick_player_scamming";
	}
	return "#SFUI_vote_kick_player_other";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CKickIssue::GetOtherTeamDisplayString( void )
{
	return "#SFUI_otherteam_vote_kick_player";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CKickIssue::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_kick_player";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CKickIssue::GetDetailsString( void )
{
	int iUserID = 0;
	const char *pReason = std::strstr( m_szDetailsString, " " );
	if ( pReason != nullptr )
	{
		pReason += 1;
		std::string userID;
		userID.assign( m_szDetailsString, pReason - m_szDetailsString );
		iUserID = std::atoi( userID.c_str() );
	}
	else
	{
		iUserID = std::atoi( m_szDetailsString );
	}	

	CBasePlayer *pPlayer = UTIL_PlayerByUserId( iUserID );
	if ( pPlayer )
	{
		return pPlayer->GetPlayerName();
	}
	else
	{
		return "unnamed";
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CKickIssue::ExtractDataFromDetails( const char *pszDetails, CCSPlayer **pSubject, uint32 *pReason )
{
	int iUserID = 0;
	const char *pReasonString = std::strstr( pszDetails, " " );
	if ( pReasonString != nullptr )
	{
		pReasonString += 1;
		std::string userID;
		userID.assign( pszDetails, pReasonString - pszDetails );
		iUserID = std::atoi( userID.c_str() );
		if ( pReason )
		{
			//[tj] Not applicable without TF-specific GC code
			//*pReason = GetKickBanPlayerReason( pReasonString );
		}
	}
	else
	{
		iUserID = std::atoi( pszDetails );
	}

	if ( iUserID >= 0 )
	{
		if( pSubject )
		{
			*pSubject = ToCSPlayer( UTIL_PlayerByUserId( iUserID ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CKickIssue::ListIssueDetails( CBasePlayer *pForWhom )
{
	if( !sv_vote_issue_kick_allowed.GetBool() )
		return;

	char szBuffer[MAX_COMMAND_LENGTH];
	V_snprintf( szBuffer, MAX_COMMAND_LENGTH, "callvote %s <userID>\n", GetTypeString() );
	ClientPrint( pForWhom, HUD_PRINTCONSOLE, szBuffer );
}

//-----------------------------------------------------------------------------
// Purpose: LoadBackup Player Issue
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_loadbackup_allowed( side_registry(), "sv_vote_issue_loadbackup_allowed", "1", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE, "Can people hold votes to load match from backup?" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLoadBackupIssue::ExecuteCommand( void )
{
	CSGameRules()->LoadRoundDataInformation( m_szDetailsString );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CLoadBackupIssue::IsEnabled( void )
{
	return sv_vote_issue_loadbackup_allowed.GetBool( );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CLoadBackupIssue::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if ( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if ( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLoadBackupIssue::OnVoteFailed( void )
{
	CBaseCSIssue::OnVoteFailed( );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CLoadBackupIssue::GetDisplayString( void )
{
	return "#SFUI_vote_loadbackup";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CLoadBackupIssue::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_loadbackup";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CLoadBackupIssue::GetDetailsString( void )
{

	// create human readable name
	if ( !FStrEq( m_szDetailsString, m_szPrevDetailsString ) )
	{
		V_strcpy_safe( m_szNiceName, m_szDetailsString );

		KeyValues *kvSaveFile = new KeyValues( "" );
		KeyValues::AutoDelete autodelete_kvSaveFile( kvSaveFile );
		autodelete_kvSaveFile->UsesEscapeSequences( true );

		if ( kvSaveFile->LoadFromFile( filesystem, m_szDetailsString ) )
		{
			V_sprintf_safe( m_szNiceName, 
				"Score %d:%d",
				kvSaveFile->GetInt( "FirstHalfScore/team1" ) + kvSaveFile->GetInt( "SecondHalfScore/team1" ) + kvSaveFile->GetInt( "OvertimeScore/team1" ),
				kvSaveFile->GetInt( "FirstHalfScore/team2" ) + kvSaveFile->GetInt( "SecondHalfScore/team2" ) + kvSaveFile->GetInt( "OvertimeScore/team2" ) );
		}

		V_strcpy_safe( m_szPrevDetailsString, m_szDetailsString );
	}

	return m_szNiceName;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLoadBackupIssue::ListIssueDetails( CBasePlayer *pForWhom )
{
	if ( !sv_vote_issue_loadbackup_allowed.GetBool( ) )
		return;

	char szBuffer[ MAX_COMMAND_LENGTH ];
	V_snprintf( szBuffer, MAX_COMMAND_LENGTH, "callvote %s <backup filename>\n", GetTypeString( ) );
	ClientPrint( pForWhom, HUD_PRINTCONSOLE, szBuffer );
}

//-----------------------------------------------------------------------------
// Purpose: Changelevel
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_changelevel_allowed( side_registry(), "sv_vote_issue_changelevel_allowed", "1", 0, "Can people hold votes to change levels?" );
ConVar sv_vote_to_changelevel_before_match_point( side_registry(), "sv_vote_to_changelevel_before_match_point", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Restricts vote to change level to rounds prior to match point (default 0, vote is never disallowed)" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChangeLevelIssue::ExecuteCommand( void )
{
	engine->ServerCommand( CFmtStr( "changelevel %s;", m_szDetailsString ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CChangeLevelIssue::CanTeamCallVote( int iTeam ) const
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CChangeLevelIssue::IsEnabled( void )
{
	if ( sv_vote_issue_changelevel_allowed.GetBool() )
	{			 		
		return ( sv_vote_to_changelevel_before_match_point.GetInt() > 0 &&
			 CSGameRules() && 
			 ( CSGameRules()->IsMatchPoint() || CSGameRules()->IsIntermission() ) ) == false;
	}
	// Change Level vote disabled
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CChangeLevelIssue::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	if ( !V_strcmp( pszDetails, "" ) )
	{
		nFailCode = VOTE_FAILED_MAP_NAME_REQUIRED;
		return false;
	}
	else
	{
		if ( !engine->IsMapValid( pszDetails ) )
		{
			nFailCode = VOTE_FAILED_MAP_NOT_FOUND;
			return false;
		}
	}
	
	bool mapIsInGroup = false;
	if ( gpGlobals )
	{	
		const char* groupName = gpGlobals->mapGroupName.ToCStr();
		const std::vector< std::string >* mapsInGroup = g_pGameTypes->GetMapGroupMapList( groupName );
		if ( mapsInGroup )
		{
			int numMaps = std::ssize( *mapsInGroup );

			for( int i = 0 ; i < numMaps ; ++i )
			{
				const char* internalMapName = ( *mapsInGroup )[i].c_str();
				if ( !V_strcmp( pszDetails, internalMapName ) )
				{
					mapIsInGroup = true;
					break;
				}
			}
		}
	}

	return mapIsInGroup;

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CChangeLevelIssue::GetDisplayString( void )
{
	return "#SFUI_vote_changelevel";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CChangeLevelIssue::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_changelevel";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CChangeLevelIssue::GetDetailsString( void )
{
	return m_szDetailsString;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChangeLevelIssue::ListIssueDetails( CBasePlayer *pForWhom )
{
	if( !sv_vote_issue_changelevel_allowed.GetBool() )
		return;

	char szBuffer[MAX_COMMAND_LENGTH];
	V_snprintf( szBuffer, MAX_COMMAND_LENGTH, "callvote %s <mapname>\n", GetTypeString() );
	ClientPrint( pForWhom, HUD_PRINTCONSOLE, szBuffer );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CChangeLevelIssue::IsYesNoVote( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Nextlevel
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_nextlevel_allowed( side_registry(), "sv_vote_issue_nextlevel_allowed", "1", 1, "Can people hold votes to set the next level?" );
ConVar sv_vote_issue_nextlevel_choicesmode( side_registry(), "sv_vote_issue_nextlevel_choicesmode", "1", 0, "Present players with a list of lowest playtime maps to choose from?" );
ConVar sv_vote_issue_nextlevel_allowextend( side_registry(), "sv_vote_issue_nextlevel_allowextend", "1", 0, "Allow players to extend the current map?" );
ConVar sv_vote_issue_nextlevel_prevent_change( side_registry(), "sv_vote_issue_nextlevel_prevent_change", "1", 1, "Not allowed to vote for a nextlevel if one has already been set." );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNextLevelIssue::ExecuteCommand( void )
{
	if ( V_strcmp( m_szDetailsString, "Extend current Map" ) == 0 )
	{
		// Players want to extend the current map, so extend any existing limits
		if ( mp_timelimit.GetInt() > 0 )
		{
			engine->ServerCommand( CFmtStr( "mp_timelimit %d;", mp_timelimit.GetInt() + 20 ) );
		}
		
		if ( mp_maxrounds.GetInt() > 0 )
		{
			engine->ServerCommand( CFmtStr( "mp_maxrounds %d;", mp_maxrounds.GetInt() + 2 ) );
		}
		
		if ( mp_winlimit.GetInt() > 0 )
		{
			engine->ServerCommand( CFmtStr( "mp_winlimit %d;", mp_winlimit.GetInt() + 2 ) );
		}
	}
	else
	{
		engine->ServerCommand( CFmtStr( "nextlevel %s;", m_szDetailsString ) );

		if ( gpGlobals )
		{	
			const char* groupName = gpGlobals->mapGroupName.ToCStr();
			const std::vector< std::string >* mapsInGroup = g_pGameTypes->GetMapGroupMapList( groupName );
			if ( mapsInGroup )
			{
				int numMaps = std::ssize( *mapsInGroup );

				for( int i = 0 ; i < numMaps ; ++i )
				{
					const char* internalMapName = ( *mapsInGroup )[i].c_str();
					if ( !V_strcmp( m_szDetailsString, internalMapName ) )
					{
						CSGameRules()->SetNextMapInMapGroup(i);
						break;
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNextLevelIssue::CanTeamCallVote( int iTeam ) const
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNextLevelIssue::IsEnabled( void )
{
	return sv_vote_issue_nextlevel_allowed.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNextLevelIssue::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	// CSGameRules created vote
	if ( sv_vote_issue_nextlevel_choicesmode.GetBool() && iEntIndex == 99 )
	{
		// Invokes a UI down stream
		if ( V_strcmp( pszDetails, "" ) == 0 )
		{
			return true;
		}

		return false;
	}
	
	if( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	if ( V_strcmp( pszDetails, "" ) == 0 )
	{
		nFailCode = VOTE_FAILED_MAP_NAME_REQUIRED;
		return false;
	}
	else
	{
		if ( !engine->IsMapValid( pszDetails ) )
		{
			nFailCode = VOTE_FAILED_MAP_NOT_FOUND;
			return false;
		}
	}

	if ( sv_vote_issue_nextlevel_prevent_change.GetBool() )
	{
		if ( nextlevel.GetString() && *nextlevel.GetString() && 
			engine->IsMapValid( nextlevel.GetString() ) )
		{
			nFailCode = VOTE_FAILED_NEXTLEVEL_SET;
			return false;
		}
	}
	
	bool mapIsInGroup = false;
	if ( gpGlobals )
	{	
		const char* groupName = gpGlobals->mapGroupName.ToCStr();
		const std::vector< std::string >* mapsInGroup = g_pGameTypes->GetMapGroupMapList( groupName );
		if ( mapsInGroup )
		{
			int numMaps = std::ssize( *mapsInGroup );

			for( int i = 0 ; i < numMaps ; ++i )
			{
				const char* internalMapName = ( *mapsInGroup )[i].c_str();
				if ( !V_strcmp( pszDetails, internalMapName ) )
				{
					mapIsInGroup = true;
					break;
				}
			}
		}
	}

	return mapIsInGroup;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CNextLevelIssue::GetDisplayString( void )
{
	// If we don't have a map passed in already...
	if ( V_strcmp( m_szDetailsString, "" ) == 0 )
	{
		if ( sv_vote_issue_nextlevel_choicesmode.GetBool() )
		{
			return "#SFUI_vote_nextlevel_choices";
		}
	}

	return "#SFUI_vote_nextlevel";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CNextLevelIssue::GetVotePassedString( void )
{
	if ( V_strcmp( m_szDetailsString, STRING( gpGlobals->mapname ) ) == 0 )
	{
		return "#SFUI_vote_passed_nextlevel_extend";
	}
	return "#SFUI_vote_passed_nextlevel";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CNextLevelIssue::GetDetailsString( void )
{
	return m_szDetailsString;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNextLevelIssue::ListIssueDetails( CBasePlayer *pForWhom )
{
	if( !sv_vote_issue_nextlevel_allowed.GetBool() )
		return;

	if ( !sv_vote_issue_nextlevel_choicesmode.GetBool() )
	{
		char szBuffer[MAX_COMMAND_LENGTH];
		V_snprintf( szBuffer, MAX_COMMAND_LENGTH, "callvote %s <mapname>\n", GetTypeString() );
		ClientPrint( pForWhom, HUD_PRINTCONSOLE, szBuffer );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNextLevelIssue::IsYesNoVote( void )
{
	// If we don't have a map name already, this will trigger a list of choices
	if ( V_strcmp( m_szDetailsString, "" ) == 0 )
	{
		if ( sv_vote_issue_nextlevel_choicesmode.GetBool() )
			return false;
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNextLevelIssue::GetNumberVoteOptions( void )
{
	// If we don't have a map name already, this will trigger a list of choices
	if ( V_strcmp( m_szDetailsString, "" ) == 0 )
	{
		if ( sv_vote_issue_nextlevel_choicesmode.GetBool() )
			return MAX_VOTE_OPTIONS;
	}

	// Vote on a specific map - Yes, No
	return 2;
}

//-----------------------------------------------------------------------------
// Purpose: Scramble Teams Issue
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_scramble_teams_allowed( side_registry(), "sv_vote_issue_scramble_teams_allowed", "1", 0, "Can people hold votes to scramble the teams?" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CScrambleTeams::ExecuteCommand( void )
{
	engine->ServerCommand( CFmtStr( "mp_scrambleteams 2;" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CScrambleTeams::IsEnabled( void )
{
	return sv_vote_issue_scramble_teams_allowed.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CScrambleTeams::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	if ( CSGameRules() && CSGameRules()->GetScrambleTeamsOnRestart() )
	{
		nFailCode = VOTE_FAILED_SCRAMBLE_IN_PROGRESS;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CScrambleTeams::GetDisplayString( void )
{
	return "#SFUI_vote_scramble_teams";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CScrambleTeams::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_scramble_teams";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CScrambleTeams::ListIssueDetails( CBasePlayer *pForWhom )
{
	if( !sv_vote_issue_scramble_teams_allowed.GetBool() )
		return;

	ListStandardNoArgCommand( pForWhom, GetTypeString() );
}

//-----------------------------------------------------------------------------
// Purpose: Pause Match
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_pause_match_allowed( side_registry(), "sv_vote_issue_pause_match_allowed", "1", 0, "Can people hold votes to pause/unpause the match?" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPauseMatchIssue::ExecuteCommand( void )
{
	engine->ServerCommand( CFmtStr( "mp_pause_match;" ) );

	CBaseEntity *pVoteCaller = UTIL_EntityByIndex( m_pVoteController->GetCallingEntity( ) );
	if ( !pVoteCaller )
		return;

	CCSPlayer *pPlayer = ToCSPlayer( pVoteCaller );
	if ( !pPlayer )
		return;

	CFmtStr fmtEntName;
	if ( pPlayer )
		fmtEntName.AppendFormat( "#ENTNAME[%d]%s", pPlayer->entindex(), pPlayer->GetPlayerName() );
	UTIL_ClientPrintAll( HUD_PRINTTALK, "#SFUI_vote_passed_pause_match_chat", fmtEntName.Access() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPauseMatchIssue::IsEnabled( void )
{
	return CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() && sv_vote_issue_pause_match_allowed.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPauseMatchIssue::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	if ( !CSGameRules() )
		return false;

	if ( CSGameRules()->IsMatchWaitingForResume() )
	{
		nFailCode = VOTE_FAILED_MATCH_PAUSED;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CPauseMatchIssue::GetDisplayString( void )
{
	return "#SFUI_vote_pause_match";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CPauseMatchIssue::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_pause_match";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPauseMatchIssue::ListIssueDetails( CBasePlayer *pForWhom )
{
	if( !sv_vote_issue_pause_match_allowed.GetBool() )
		return;

	ListStandardNoArgCommand( pForWhom, GetTypeString() );
}

//-----------------------------------------------------------------------------
// Purpose: UnPause Match
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUnpauseMatchIssue::ExecuteCommand( void )
{
	engine->ServerCommand( CFmtStr( "mp_unpause_match;" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CUnpauseMatchIssue::IsEnabled( void )
{
	return CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() && sv_vote_issue_pause_match_allowed.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CUnpauseMatchIssue::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if ( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if ( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	if (!CSGameRules())
		return false;

	if ( !CSGameRules()->IsMatchWaitingForResume() )
	{

		nFailCode = VOTE_FAILED_MATCH_NOT_PAUSED;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CUnpauseMatchIssue::GetDisplayString( void )
{
	return "#SFUI_vote_unpause_match";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CUnpauseMatchIssue::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_unpause_match";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUnpauseMatchIssue::ListIssueDetails( CBasePlayer *pForWhom )
{
	if ( !sv_vote_issue_pause_match_allowed.GetBool() )
		return;

	ListStandardNoArgCommand( pForWhom, GetTypeString() );
}

//-----------------------------------------------------------------------------
// Purpose: TimeOut
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_timeout_allowed( side_registry(), "sv_vote_issue_timeout_allowed", "1", 0, "Can people hold votes to time out?" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CStartTimeOutIssue::ExecuteCommand( void )
{
	CBaseEntity *pVoteHolder = UTIL_EntityByIndex( m_pVoteController->GetCallingEntity( ) );
	if ( !pVoteHolder )
		return;

	if ( pVoteHolder->GetTeamNumber() == TEAM_CT )
	{
		engine->ServerCommand( CFmtStr( "timeout_ct_start;" ) );
	}
	else if ( pVoteHolder->GetTeamNumber() == TEAM_TERRORIST )
	{
		engine->ServerCommand( CFmtStr( "timeout_terrorist_start;" ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CStartTimeOutIssue::IsEnabled( void )
{
	return CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() && sv_vote_issue_timeout_allowed.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CStartTimeOutIssue::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if ( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if ( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	if ( CSGameRules() && CSGameRules()->IsTimeOutActive() )
	{
		nFailCode = VOTE_FAILED_TIMEOUT_ACTIVE;
		return false;
	}

	if ( CSGameRules() && CSGameRules()->IsMatchWaitingForResume() )
	{
		nFailCode = VOTE_FAILED_MATCH_PAUSED;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CStartTimeOutIssue::CanTeamCallVote(int iTeam ) const
{
	if ( !CSGameRules() )
		return false;

	if ( CSGameRules()->IsTimeOutActive() )
		return false;

	if ( iTeam == TEAM_CT )
	{
		return ( CSGameRules()->GetCTTimeOuts() > 0 );
	}
	else if ( iTeam == TEAM_TERRORIST )
	{
		return ( CSGameRules()->GetTerroristTimeOuts() > 0 );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CStartTimeOutIssue::GetDisplayString( void )
{
	return "#SFUI_vote_start_timeout";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CStartTimeOutIssue::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_timeout";
}

vote_create_failed_t CStartTimeOutIssue::MakeVoteFailErrorCodeForClients( vote_create_failed_t eDefaultFailCode )
{
	switch ( eDefaultFailCode )
	{
	case VOTE_FAILED_TEAM_CANT_CALL:
	{
		if ( CSGameRules( ) && CSGameRules( )->IsTimeOutActive( ) )
			return VOTE_FAILED_TIMEOUT_ACTIVE;
		else if ( CSGameRules() && CSGameRules()->IsMatchWaitingForResume() )
			return VOTE_FAILED_MATCH_PAUSED;
		else
			return VOTE_FAILED_TIMEOUT_EXHAUSTED;
	}

	default:
		return eDefaultFailCode;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CStartTimeOutIssue::ListIssueDetails( CBasePlayer *pForWhom )
{
	if ( !sv_vote_issue_pause_match_allowed.GetBool() )
		return;

	ListStandardNoArgCommand( pForWhom, GetTypeString() );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_matchready_allowed( side_registry(), "sv_vote_issue_matchready_allowed", "1", 0, "Can people hold votes to ready/unready the match?" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CReadyForMatchIssue::ExecuteCommand( void )
{
	CSGameRules()->SetWarmupPeriodStartTime( gpGlobals->curtime );

	engine->ServerCommand( CFmtStr( "mp_warmup_pausetimer 0;" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CReadyForMatchIssue::IsEnabled( void )
{
	return CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() && sv_vote_issue_matchready_allowed.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CReadyForMatchIssue::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if ( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if ( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	if ( CSGameRules() && !CSGameRules()->IsWarmupPeriod() )
	{
		nFailCode = VOTE_FAILED_NOT_IN_WARMUP;
		return false;
	}

	if ( UTIL_HumansInGame( true, true ) != 10 )
	{
		nFailCode = VOTE_FAILED_NOT_10_PLAYERS;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CReadyForMatchIssue::GetDisplayString( void )
{
	return "#SFUI_vote_ready_for_match";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CReadyForMatchIssue::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_ready_for_match";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CReadyForMatchIssue::ListIssueDetails( CBasePlayer *pForWhom )
{
	if ( !sv_vote_issue_matchready_allowed.GetBool() )
		return;

	ListStandardNoArgCommand( pForWhom, GetTypeString() );
}

//-----------------------------------------------------------------------------
// Purpose: Someone wants to abort the ready process
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNotReadyForMatchIssue::ExecuteCommand( void )
{
	engine->ServerCommand( CFmtStr( "mp_warmup_pausetimer 1;" ) );

	CBaseEntity *pVoteCaller = UTIL_EntityByIndex( m_pVoteController->GetCallingEntity( ) );
	if ( !pVoteCaller )
		return;

	CCSPlayer *pPlayer = ToCSPlayer( pVoteCaller );
	if ( !pPlayer )
		return;

	CFmtStr fmtEntName;
	if ( pPlayer )
		fmtEntName.AppendFormat( "#ENTNAME[%d]%s", pPlayer->entindex(), pPlayer->GetPlayerName() );
	UTIL_ClientPrintAll( HUD_PRINTTALK, "#SFUI_vote_passed_not_ready_for_match_chat", fmtEntName.Access() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNotReadyForMatchIssue::IsEnabled( void )
{
	return CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() && sv_vote_issue_matchready_allowed.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNotReadyForMatchIssue::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if ( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if ( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	if ( CSGameRules() && !CSGameRules()->IsWarmupPeriod() )
	{
		nFailCode = VOTE_FAILED_NOT_IN_WARMUP;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CNotReadyForMatchIssue::GetDisplayString( void )
{
	return "#SFUI_vote_not_ready_for_match";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CNotReadyForMatchIssue::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_not_ready_for_match";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNotReadyForMatchIssue::ListIssueDetails( CBasePlayer *pForWhom )
{
	if ( !sv_vote_issue_matchready_allowed.GetBool() )
		return;

	ListStandardNoArgCommand( pForWhom, GetTypeString() );
}

//-----------------------------------------------------------------------------
// Purpose: Swap Teams Issue
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_swap_teams_allowed( side_registry(), "sv_vote_issue_swap_teams_allowed", "1", 0, "Can people hold votes to swap the teams?" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSwapTeams::ExecuteCommand( void )
{
	engine->ServerCommand( CFmtStr( "mp_swapteams;" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSwapTeams::IsEnabled( void )
{
	return sv_vote_issue_swap_teams_allowed.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSwapTeams::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if ( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if ( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	if ( CSGameRules() && CSGameRules()->GetSwapTeamsOnRestart() )
	{
		nFailCode = VOTE_FAILED_SWAP_IN_PROGRESS;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CSwapTeams::GetDisplayString( void )
{
	return "#SFUI_vote_swap_teams";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CSwapTeams::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_swap_teams";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSwapTeams::ListIssueDetails( CBasePlayer *pForWhom )
{
	if ( !sv_vote_issue_swap_teams_allowed.GetBool() )
		return;

	ListStandardNoArgCommand( pForWhom, GetTypeString() );
}

//-----------------------------------------------------------------------------
// Purpose: Surrender Issue
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_surrender_allowed( side_registry(), "sv_vote_issue_surrrender_allowed", "1", 0, "Can people hold votes to surrender?" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSurrender::ExecuteCommand( void )
{
	CBaseEntity *pVoteHolder = UTIL_EntityByIndex( m_pVoteController->GetCallingEntity( ) );
	if ( !pVoteHolder )
		return;

	if ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() )
	{
		if ( CSGameRules()->m_iRoundWinStatus != WINNER_NONE || CSGameRules()->GetGamePhase() == GAMEPHASE_HALFTIME )
		{
			// if the vote succeeds at the round end, just cancel it because we don't handle this in the gamerules round logic
			CBasePlayer *pVoteCaller = entity_cast<CBasePlayer>( pVoteHolder );
			if ( pVoteCaller )
				m_pVoteController->SendVoteFailedMessage( VOTE_FAILED_CANT_ROUND_END, pVoteCaller, 0 );
		}
		else if ( ( CSGameRules()->m_eQueuedMatchmakingRematchState == CSGameRules()->k_EQueuedMatchmakingRematchState_MatchInProgress ) &&
			( CSGameRules()->GetGamePhase() != GAMEPHASE_MATCH_ENDED ) )
		{
			if ( pVoteHolder->GetTeamNumber() == TEAM_TERRORIST )
			{
				CSGameRules()->GetMatch()->AddCTWins( 1 );
				CSGameRules()->m_eQueuedMatchmakingRematchState = CSGameRules()->k_EQueuedMatchmakingRematchState_VoteToRematch_T_Surrender;
				CSGameRules()->TerminateRound( 0, Terrorists_Surrender );
			}
			if ( pVoteHolder->GetTeamNumber() == TEAM_CT )
			{
				CSGameRules()->GetMatch()->AddTerroristWins( 1 );
				CSGameRules()->m_eQueuedMatchmakingRematchState = CSGameRules()->k_EQueuedMatchmakingRematchState_VoteToRematch_CT_Surrender;
				CSGameRules()->TerminateRound( 0, CTs_Surrender );
			}
		}
	}
	else
	{
		if ( pVoteHolder->GetTeamNumber() == TEAM_TERRORIST )
		{
			CSGameRules()->TerminateRound( 0, Terrorists_Surrender );
		}
		if ( pVoteHolder->GetTeamNumber() == TEAM_CT )
		{
			CSGameRules()->TerminateRound( 0, CTs_Surrender );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSurrender::IsEnabled( void )
{
	return sv_vote_issue_surrender_allowed.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSurrender::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if ( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() )
	{
		if ( CSGameRules()->IsPlayingCooperativeGametype() )
		{
			nFailCode = VOTE_FAILED_ISSUE_DISABLED;
			return false;
		}

		if ( CSGameRules()->m_eQueuedMatchmakingRematchState != CSGameRules()->k_EQueuedMatchmakingRematchState_MatchInProgress )
		{
			nFailCode = VOTE_FAILED_ISSUE_DISABLED;
			return false;
		}

		if ( CSGameRules()->GetGamePhase() == GAMEPHASE_MATCH_ENDED || CSGameRules()->GetGamePhase() == GAMEPHASE_HALFTIME ||
			 CSGameRules()->IsLastRoundBeforeHalfTime() )
		{
			nFailCode = VOTE_FAILED_ISSUE_DISABLED;
			return false;
		}

		if ( CSGameRules()->GetRoundsPlayed() < 1 )
		{	// Cannot surrender unless at least one round was played
			nFailCode = VOTE_FAILED_WAITINGFORPLAYERS;
			return false;
		}
	}

	if ( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	return true;
}

bool CSurrender::CanTeamCallVote( int iTeam ) const
{
	if ( !CSGameRules() || !CSGameRules()->IsQueuedMatchmaking() )
		return false;

	bool bTeamsArePlayingSwitchedSides = CSGameRules()->AreTeamsPlayingSwitchedSides();
	for ( const auto &[uiAccountID, pQPD] : CSGameRules()->m_mapQueuedMatchmakingPlayersData )
	{
		CCSGameRules::CQMMPlayerData_t const &qpd = *pQPD;
		if ( qpd.m_bAbandonAllowsSurrender )
		{
			bool bSecondTeam = ( qpd.m_iDraftIndex >= 5 );
			int iTeamOfThisQPlayer = ( bSecondTeam == bTeamsArePlayingSwitchedSides ) ? TEAM_CT : TEAM_TERRORIST;
			if ( iTeamOfThisQPlayer == iTeam )
				return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CSurrender::GetDisplayString( void )
{
	return "#SFUI_vote_surrender";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CSurrender::GetOtherTeamDisplayString( void )
{
	return "#SFUI_otherteam_vote_surrender";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CSurrender::GetVotePassedString( void )
{
	return ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() ) ?  "#SFUI_vote_passed_surrender_q" : "#SFUI_vote_passed_surrender";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSurrender::ListIssueDetails( CBasePlayer *pForWhom )
{
	if ( !sv_vote_issue_surrender_allowed.GetBool() )
		return;

	ListStandardNoArgCommand( pForWhom, GetTypeString() );
}

CLoadBackupIssue::CLoadBackupIssue( CVoteController *pVoteController) : CBaseCSIssue( VOTEISSUE_NAME_LOADBACKUP, pVoteController )
{
	m_szPrevDetailsString[ 0 ] = 0;
	m_szNiceName[ 0 ] = 0;
}

class CCSWeaponInfo;
class CEconItemDefinition;
class CEconItemView;
class CWeaponCSBase;

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

		Msg( "RECOIL: No seed found for weapon %s, generated placeholder seed %i\n", weaponName, m_iRecoilSeed );
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

	// Enforce consistency for the weapon here, since that way we don't need to save off the model bounds
	// for all time.
	engine->ForceExactFile( UTIL_VarArgs( "scripts/%s.ctx", szWeaponName ) );

	// Model bounds are rounded to the nearest integer, then extended by 1
	engine->ForceModelBounds( szWorldModel, Vector( -15, -12, -18 ), Vector( 44, 16, 19 ) );
	if ( m_szAddonModel[0] )
	{
		engine->ForceModelBounds( m_szAddonModel, Vector( -5, -5, -6 ), Vector( 13, 5, 7 ) );
	}
	if ( m_szSilencerModel[0] )
	{
		engine->ForceModelBounds( m_szSilencerModel, Vector( -15, -12, -18 ), Vector( 44, 16, 19 ) );
	}
	
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
