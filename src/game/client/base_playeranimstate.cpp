// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include "edict.h"
#include "filesystem.h"
#include "studio.h"
#include "declare_class.h"
#include "iachievementmgr.h"
#include "igameevents.h"
#include "matchmaking/imatchframework.h"
#include "reflect.h"
#include "cdll_client_int.h"
#include "reflect_datamap.h"
#include "datamap.h"
#include "matchmaking/iplayer.h"
#include "baseachievement.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
module ks.client;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.vprof;
import ks.game.iplayeranimstate;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.math.vector2d;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.keyvalues;
import ks.tier1.map_ops;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.strtools;
import ks.game.ai_activity;
import :baseentity;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :sharedvar;
import :util;
import :cstrike15.cs_shareddefs;
import :particlemgr;
import :precache;
import ks.game.shareddefs;
import :achievements_and_stats_interface;
import :econ.econ_item_schema_minimal;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :noopanimcontroller;
import ks.core.types;
import ks.tier1.interfaces;

class CAnimationLayer;
class CBaseAnimatingOverlay;
class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

	ConVar cl_showanimstate( side_registry(), "cl_showanimstate", "-1", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Show the (client) animation state for the specified entity (-1 for none)." );
	ConVar showanimstate_log( side_registry(), "cl_showanimstate_log", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "1 to output cl_showanimstate to Msg(). 2 to store in AnimStateClient.log. 3 for both." );
	ConVar showanimstate_activities( side_registry(), "cl_showanimstate_activities", "0", FCVAR_CHEAT, "Show activities in the (client) animation state display." );

// Below this many degrees, slow down turning rate linearly
static constexpr auto FADE_TURN_DEGREES = 15.0f;

// After this, need to start turning feet
static constexpr auto MAX_TORSO_ANGLE = 70.0f;

// Below this amount, don't play a turning animation/perform IK
static constexpr auto MIN_TURN_ANGLE_REQUIRING_TURN_ANIMATION = 15.0f;

static constexpr auto POSE_PARAM_DELTA_DAMPEN = 4.0f;

ConVar mp_feetyawrate( 
	side_registry(), "mp_feetyawrate", 
	"400",
	FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, 
	"How many degrees per second that we can turn our feet or upper body." );

ConVar mp_facefronttime( 
	side_registry(), "mp_facefronttime", 
	"2", 
	FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, 
	"After this amount of time of standing in place but aiming to one side, go ahead and move feet to face upper body." );

ConVar mp_ik( side_registry(), "mp_ik", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Use IK on in-place turns." );

// Pose parameters stored for debugging.
float g_flLastBodyPitch, g_flLastBodyYaw, m_flLastMoveYaw;

// ------------------------------------------------------------------------------------------------ //
// CBasePlayerAnimState implementation.
// ------------------------------------------------------------------------------------------------ //

CBasePlayerAnimState::CBasePlayerAnimState()
{
	m_flEyeYaw = 0.0f;
	m_flEyePitch = 0.0f;
	m_bCurrentFeetYawInitialized = false;
	m_flCurrentTorsoYaw = 0.0f;
	m_flCurrentTorsoYaw = TURN_NONE;
	m_flMaxGroundSpeed = 0.0f;
	m_flStoredCycle = 0.0f;

	m_flGaitYaw = 0.0f;
	m_flGoalFeetYaw = 0.0f;
	m_flCurrentFeetYaw = 0.0f;
	m_bForceAimYaw = false;
	m_flLastYaw = 0.0f;
	m_flLastTurnTime = 0.0f;
	m_angRender.Init();
	m_vLastMovePose.Init();
	m_iCurrent8WayIdleSequence = -1;
	m_iCurrent8WayCrouchIdleSequence = -1;

	m_pOuter = nullptr;
	m_eCurrentMainSequenceActivity = ACT_IDLE;
	m_flLastAnimationStateClearTime = 0.0f;
	
	m_bInFootPlantIdleTurn = false;
	m_flFootPlantIdleTurnCycle = 0.0f;
	m_bFootPlantIdleNeedToLiftFeet = false;

	m_flPoseParamTargetDampenedScaleIdeal = 0.0f;
}

CBasePlayerAnimState::~CBasePlayerAnimState()
{
}

void CBasePlayerAnimState::Init( CBaseAnimatingOverlay *pPlayer, const CModAnimConfig &config )
{
	m_pOuter = pPlayer;
	m_AnimConfig = config;
	ClearAnimationState();
}

void CBasePlayerAnimState::Release()
{
	delete this;
}

void CBasePlayerAnimState::ClearAnimationState()
{
	ClearAnimationLayers();
	m_bCurrentFeetYawInitialized = false;
	m_flLastAnimationStateClearTime = gpGlobals->curtime;
}

float CBasePlayerAnimState::TimeSinceLastAnimationStateClear() const
{
	return gpGlobals->curtime - m_flLastAnimationStateClearTime;
}

void CBasePlayerAnimState::Update( float eyeYaw, float eyePitch )
{
	CVProfScope _( "CBasePlayerAnimState::Update", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Clear animation overlays because we're about to completely reconstruct them.
	ClearAnimationLayers();

	// Some mods don't want to update the player's animation state if they're dead and ragdolled.
	if ( !ShouldUpdateAnimState() )
	{
		ClearAnimationState();
		return;
	}
	
	CStudioHdr *pStudioHdr = GetOuter()->GetModelPtr();
	// Store these. All the calculations are based on them.
	m_flEyeYaw = AngleNormalize( eyeYaw );
	m_flEyePitch = AngleNormalize( eyePitch );

	// Compute sequences for all the layers.
	ComputeSequences( pStudioHdr );
	
	// Compute all the pose params.
	ComputePoseParam_BodyPitch( pStudioHdr );	// Look up/down.
	ComputePoseParam_BodyYaw();		// Torso rotation.
	ComputePoseParam_MoveYaw( pStudioHdr );		// What direction his legs are running in.

	ComputePlaybackRate();

	if ( cl_showanimstate.GetInt() == m_pOuter->entindex() )
	{
		DebugShowAnimStateFull( 5 );
	}
	else if ( cl_showanimstate.GetInt() == -2 )
	{
		CBasePlayer *targetPlayer = CBasePlayer::GetLocalPlayer();

		if( targetPlayer && ( targetPlayer->GetObserverMode() == OBS_MODE_IN_EYE || targetPlayer->GetObserverMode() == OBS_MODE_CHASE ) )
		{
			CBaseEntity *target = targetPlayer->GetObserverTarget();

			if( target && target->IsPlayer() )
			{
				targetPlayer = ToBasePlayer( target );
			}
		}

		if ( m_pOuter == targetPlayer )
		{
			DebugShowAnimStateFull( 6 );
		}
	}
}

bool CBasePlayerAnimState::ShouldUpdateAnimState()
{
	// By default, don't update their animation state when they're dead because they're
	// either a ragdoll or they're not drawn.
	return GetOuter()->IsAlive();
}

bool CBasePlayerAnimState::ShouldChangeSequences( void ) const
{
	return true;
}

void CBasePlayerAnimState::SetOuterPoseParameter( int iParam, float flValue )
{
	// Make sure to set all the history values too, otherwise the server can overwrite them.
	GetOuter()->SetPoseParameter( iParam, flValue );
}

void CBasePlayerAnimState::ClearAnimationLayers()
{
	contract_assert( 0 ); // unused?

	CVProfScope _( "CBasePlayerAnimState::ClearAnimationLayers", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	if ( !m_pOuter )
		return;

	m_pOuter->SetNumAnimOverlays( AIMSEQUENCE_LAYER+NUM_AIMSEQUENCE_LAYERS );
	for ( int i=0; i < m_pOuter->GetNumAnimOverlays(); i++ )
	{
		m_pOuter->GetAnimOverlay( i )->SetOrder( CBaseAnimatingOverlay::MAX_OVERLAYS );
	}
}

void CBasePlayerAnimState::RestartMainSequence()
{
	CBaseAnimatingOverlay *pPlayer = GetOuter();

	pPlayer->m_flAnimTime = gpGlobals->curtime;
	pPlayer->SetCycle( 0 );
}

void CBasePlayerAnimState::ComputeSequences( CStudioHdr *pStudioHdr )
{
	CVProfScope _( "CBasePlayerAnimState::ComputeSequences", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	ComputeMainSequence();		// Lower body (walk/run/idle).
	UpdateInterpolators();		// The groundspeed interpolator uses the main sequence info.

	if ( m_AnimConfig.m_bUseAimSequences )
	{
		ComputeAimSequence();		// Upper body, based on weapon type.
	}
}

void CBasePlayerAnimState::ResetGroundSpeed( void )
{
	m_flMaxGroundSpeed = GetCurrentMaxGroundSpeed();
}

void CBasePlayerAnimState::ComputeMainSequence()
{
	CVProfScope _( "CBasePlayerAnimState::ComputeMainSequence", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	CBaseAnimatingOverlay *pPlayer = GetOuter();

	// Have our class or the mod-specific class determine what the current activity is.
	Activity idealActivity = CalcMainActivity();

	Activity oldActivity = m_eCurrentMainSequenceActivity;
	
	// Store our current activity so the aim and fire layers know what to do.
	m_eCurrentMainSequenceActivity = idealActivity;

	// Export to our outer class..
	int animDesired = SelectWeightedSequence( TranslateActivity(idealActivity) );

	ResetCycleAcrossCustomActivityChange( oldActivity, idealActivity );

	if ( animDesired < 0 )
		 animDesired = 0;

	pPlayer->ResetSequence( animDesired );

	if ( ShouldResetGroundSpeed( oldActivity, idealActivity ) )
	{
		ResetGroundSpeed();
	}
}

void CBasePlayerAnimState::ResetCycleAcrossCustomActivityChange( Activity iCurrent, Activity iNew )
{
	if ( !GetOuter() )
		return;

	if ( iCurrent == ACT_CROUCHIDLE && iNew != ACT_CROUCHIDLE )
	{
		GetOuter()->SetCycle(0);
	}
}

bool CBasePlayerAnimState::ShouldResetMainSequence( int iCurrentSequence, int iNewSequence )
{
	if ( !GetOuter() )
		return false;

	return GetOuter()->GetSequenceActivity( iCurrentSequence ) == GetOuter()->GetSequenceActivity( iNewSequence );
}

bool CBasePlayerAnimState::ShouldResetGroundSpeed( Activity oldActivity, Activity idealActivity )
{
	// If we went from idle to walk, reset the interpolation history.
	return ( (oldActivity == ACT_CROUCHIDLE || oldActivity == ACT_IDLE || oldActivity == ACT_TURN || oldActivity == ACT_STEP_FORE ) && 
			 (idealActivity == ACT_WALK || idealActivity == ACT_RUN_CROUCH || idealActivity == ACT_WALK_CROUCH || idealActivity == ACT_RUN ) );
}

void CBasePlayerAnimState::UpdateAimSequenceLayers(
	float flCycle,
	int iFirstLayer,
	bool bForceIdle,
	CSequenceTransitioner *pTransitioner,
	float flWeightScale
	)
{
	float flAimSequenceWeight = 1;
	int iAimSequence = CalcAimLayerSequence( &flCycle, &flAimSequenceWeight, bForceIdle );
	if ( iAimSequence == -1 )
		iAimSequence = 0;

	CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iFirstLayer );

	pLayer->SetSequence( iAimSequence );
	pLayer->SetCycle( flCycle );
	pLayer->SetWeight( clamp( flWeightScale, 0.0f, 1.0f ) );

	pLayer->SetOrder( iFirstLayer );	// should already be set

}

void CBasePlayerAnimState::OptimizeLayerWeights( int iFirstLayer, int nLayers )
{
	int i;

	// Find the total weight of the blended layers, not including the idle layer (iFirstLayer)
	float totalWeight = 0.0f;
	for ( i=1; i < nLayers; i++ )
	{
		CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iFirstLayer+i );
		if ( pLayer->IsActive() && pLayer->GetWeight() > 0.0f )
		{
			totalWeight += pLayer->GetWeight();
		}
	}

	// Set the idle layer's weight to be 1 minus the sum of other layer weights
	CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iFirstLayer );
	if ( pLayer->IsActive() && pLayer->GetWeight() > 0.0f )
	{
		float flWeight = 1.0f - totalWeight;
		flWeight = std::max( flWeight, 0.0f );
		pLayer->SetWeight( flWeight );
	}

	// This part is just an optimization. Since we have the walk/run animations weighted on top of 
	// the idle animations, all this does is disable the idle animations if the walk/runs are at
	// full weighting, which is whenever a guy is at full speed.
	//
	// So it saves us blending a couple animation layers whenever a guy is walking or running full speed.
	int iLastOne = -1;
	for ( i=0; i < nLayers; i++ )
	{
		CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iFirstLayer+i );
		if ( pLayer->IsActive() && pLayer->GetWeight() > 0.99 )
			iLastOne = i;
	}

	if ( iLastOne != -1 )
	{
		for ( int i=iLastOne-1; i >= 0; i-- )
		{
			CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iFirstLayer+i );
			pLayer->SetOrder( CBaseAnimatingOverlay::MAX_OVERLAYS );
		}
	}
}

bool CBasePlayerAnimState::ShouldBlendAimSequenceToIdle()
{
	Activity act = GetCurrentMainSequenceActivity();

	return (act == ACT_RUN || act == ACT_LEAP || act == ACT_WALK || act == ACT_JUMP || act == ACT_RUNTOIDLE || act == ACT_RUN_CROUCH || act == ACT_WALK_CROUCH );
}

void CBasePlayerAnimState::ComputeAimSequence()
{
	CVProfScope _( "CBasePlayerAnimState::ComputeAimSequence", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Synchronize the lower and upper body cycles.
	float flCycle = m_pOuter->GetCycle();

	//[msmith]
	bool bIsMoving = false;
	float flPlaybackRate = 0.0f;

	if ( ShouldBlendAimSequenceToIdle() )
	{
		// See how much the player should weigh in the motion vs. the idle layers.
		flPlaybackRate = CalcMovementPlaybackRate( &bIsMoving );
	}

	// Set the idle layers to full (blend between different idle layers only and do NOT consider movement layers).
	UpdateAimSequenceLayers( flCycle, AIMSEQUENCE_LAYER, true, &m_HighAimSequenceTransitioner, 1.0f );

	if ( bIsMoving )
	{
		// Blend the run / walk layers together.
		UpdateAimSequenceLayers( flCycle, AIMSEQUENCE_LAYER+1, false, &m_LowAimSequenceTransitioner, flPlaybackRate );
	}

	OptimizeLayerWeights( AIMSEQUENCE_LAYER, NUM_AIMSEQUENCE_LAYERS );
}

int CBasePlayerAnimState::CalcSequenceIndex( const char *pBaseName, ... )
{
	char szFullName[512];
	std::va_list marker;
	__builtin_va_start( marker, pBaseName );
	V_vsnprintf( szFullName, sizeof( szFullName ), pBaseName, marker );
	__builtin_va_end( marker );
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

void CBasePlayerAnimState::UpdateInterpolators()
{
	CVProfScope _( "CBasePlayerAnimState::UpdateInterpolators", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// First, figure out their current max speed based on their current activity.
	float flCurMaxSpeed = GetCurrentMaxGroundSpeed();
	m_flMaxGroundSpeed = flCurMaxSpeed;
}

float CBasePlayerAnimState::GetInterpolatedGroundSpeed()
{
	return m_flMaxGroundSpeed;
}

float CBasePlayerAnimState::CalcMovementPlaybackRate( bool *bIsMoving )
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
		float flGroundSpeed = GetInterpolatedGroundSpeed();
		if ( flGroundSpeed < 0.001f )
		{
			flReturnValue = 0.01;
		}
		else
		{
			flReturnValue = speed / flGroundSpeed;
			flReturnValue = clamp( flReturnValue, 0.01, 10 );	// don't go nuts here.
		}
		*bIsMoving = true;
	}
	
	return flReturnValue;
}

bool CBasePlayerAnimState::CanThePlayerMove()
{
	return true;
}

void CBasePlayerAnimState::ComputePlaybackRate()
{
	CVProfScope _( "CBasePlayerAnimState::ComputePlaybackRate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	if ( m_AnimConfig.m_LegAnimType != LEGANIM_9WAY && m_AnimConfig.m_LegAnimType != LEGANIM_8WAY )
	{
		// When using a 9-way blend, playback rate is always 1 and we just scale the pose params
		// to speed up or slow down the animation.
		bool bIsMoving;
		float flRate = CalcMovementPlaybackRate( &bIsMoving );
		if ( bIsMoving )
			GetOuter()->SetPlaybackRate( flRate );
		else
			GetOuter()->SetPlaybackRate( 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBasePlayer
//-----------------------------------------------------------------------------
CBaseAnimatingOverlay *CBasePlayerAnimState::GetOuter() const
{
	return m_pOuter;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
//-----------------------------------------------------------------------------
void CBasePlayerAnimState::EstimateYaw()
{
	Vector est_velocity;
	GetOuterAbsVelocity( est_velocity );

	float flLength = est_velocity.Length2D();
	if ( flLength > MOVING_MINIMUM_SPEED )
	{
		m_flGaitYaw = std::atan2( est_velocity[1], est_velocity[0] );
		m_flGaitYaw = RAD2DEG( m_flGaitYaw );
		m_flGaitYaw = AngleNormalize( m_flGaitYaw );
	}
}

static constexpr auto MOVEMENT_MINIMUM_ANIMATED_SPEED = 10.0f;
static constexpr auto MOVEMENT_MAXIMUM_PLAYBACK_RATE = 1.3f;

//-----------------------------------------------------------------------------
// Purpose: Override for backpeddling
// Input  : dt - 
//-----------------------------------------------------------------------------
void CBasePlayerAnimState::ComputePoseParam_MoveYaw( CStudioHdr *pStudioHdr )
{
	CVProfScope _( "CBasePlayerAnimState::ComputePoseParam_MoveYaw", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	//Matt: Goldsrc style animations need to not rotate the model
	if ( m_AnimConfig.m_LegAnimType == LEGANIM_GOLDSRC )
	{
	}

	// If using goldsrc-style animations where he's moving in the direction that his feet are facing,
	// we don't use move yaw.
	if ( m_AnimConfig.m_LegAnimType != LEGANIM_9WAY && m_AnimConfig.m_LegAnimType != LEGANIM_8WAY )
		return;

	// view direction relative to movement
	float flYaw;	 

	EstimateYaw();

	float ang = m_flEyeYaw;
	if ( ang > 180.0f )
	{
		ang -= 360.0f;
	}
	else if ( ang < -180.0f )
	{
		ang += 360.0f;
	}

	// calc side to side turning
	flYaw = ang - m_flGaitYaw;
	// Invert for mapping into 8way blend
	flYaw = -flYaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;

	if (flYaw < -180)
	{
		flYaw = flYaw + 360;
	}
	else if (flYaw > 180)
	{
		flYaw = flYaw - 360;
	}

	if ( m_AnimConfig.m_LegAnimType == LEGANIM_9WAY )
	{
		//Adrian: Make the model's angle match the legs so the hitboxes match on both sides.
		//[msmith]: Since bounding box code uses the entities local angle, we need the local angle to match
		//			the render angle on both the server AND the client.  The server for hit tests, the client
		//			for proper visibility culling.
		GetOuter()->SetLocalAngles( QAngle( 0, m_flCurrentFeetYaw, 0 ) );

		int iMoveX = GetOuter()->LookupPoseParameter( pStudioHdr, "move_x" );
		int iMoveY = GetOuter()->LookupPoseParameter( pStudioHdr, "move_y" );
		if ( iMoveX < 0 || iMoveY < 0 )
			return;

		bool bIsMoving;
		float flPlaybackRate = CalcMovementPlaybackRate( &bIsMoving );

		Vector vel;
		GetOuterAbsVelocity( vel );
		bIsMoving = ( vel.Length2D() > 0 );

		// Setup the 9-way blend parameters based on our speed and direction.
		Vector2D vCurMovePose( 0, 0 );

		m_flPoseParamTargetDampenedScaleIdeal = Approach( flPlaybackRate, m_flPoseParamTargetDampenedScaleIdeal, gpGlobals->frametime * POSE_PARAM_DELTA_DAMPEN );

		if ( bIsMoving )
		{
			vCurMovePose.x = std::cos( DEG2RAD( flYaw ) );
			vCurMovePose.y = -std::sin( DEG2RAD( flYaw ) );
			// movement pose parameters on the diagonals are encoded at 1 instead of 0.707 (cos 45)
			// scale to the outside of a box instead of the outside of a circle.
			float scale = fabs( vCurMovePose.x );
			float scale2 = fabs( vCurMovePose.y );
			if ( scale2 > scale ) scale = scale2;
			if ( scale > 0.01f )
			{
				scale = 1.0f / scale;
				vCurMovePose.x *= scale;
				vCurMovePose.y *= scale;
			}

			// find the max speed the animation will move in the current direction

			//If these aren't applied here, the legs stutter. Even though they're re-applied in a sec
			GetOuter()->SetPoseParameter( pStudioHdr, iMoveX, vCurMovePose.x );
			GetOuter()->SetPoseParameter( pStudioHdr, iMoveY, vCurMovePose.y );

			Vector vecAnimatedVel;
			GetOuter()->GetBlendedLinearVelocity( &vecAnimatedVel );
			float flAnimatedSpeed = vecAnimatedVel.Length2D();

			// find how to scale the current animation down to the desired speed
			Vector vel;
			GetOuterAbsVelocity( vel );
			float flMovementSpeed = vel.Length2D();

			if ( flAnimatedSpeed > CS_PLAYER_SPEED_RUN )
				flAnimatedSpeed = flMovementSpeed;

			if ( flAnimatedSpeed < MOVEMENT_MINIMUM_ANIMATED_SPEED )
			{
				// we're moving so slowly (either just starting or just stopping) that current playback rate is almost nothing.
				flPlaybackRate = flMovementSpeed / ( MOVEMENT_MINIMUM_ANIMATED_SPEED * 2.0f );
			}
			else
			{
				
				flPlaybackRate = flMovementSpeed / flAnimatedSpeed;
			}

			// player is moving less than what's animated, scale pose parameters back towards 0,0
			if ( flPlaybackRate < 1.0f )
			{
				if ( flPlaybackRate > 0.08f )
				{
					vCurMovePose.x *= flPlaybackRate;
					vCurMovePose.y *= flPlaybackRate;
					GetOuter()->SetPlaybackRate( flPlaybackRate );
				}
				else
				{
					vCurMovePose.x *= 0.08f;
					vCurMovePose.y *= 0.08f;
					GetOuter()->SetPlaybackRate( 1.0f );
				}
			}
			else
			{
				// speed up the animation to match the needed motion, but only so far
				flPlaybackRate = clamp( flPlaybackRate, 1.0f, MOVEMENT_MAXIMUM_PLAYBACK_RATE );
				GetOuter()->SetPlaybackRate( flPlaybackRate );
			}
			vCurMovePose.x *= m_flPoseParamTargetDampenedScaleIdeal;
			vCurMovePose.y *= m_flPoseParamTargetDampenedScaleIdeal;

		}

		GetOuter()->SetPoseParameter( pStudioHdr, iMoveX, vCurMovePose.x );
		GetOuter()->SetPoseParameter( pStudioHdr, iMoveY, vCurMovePose.y );

		m_vLastMovePose = vCurMovePose;
	}
	else
	{
		int iMoveYaw = GetOuter()->LookupPoseParameter( pStudioHdr, "move_yaw" );
		if ( iMoveYaw >= 0 )
		{
			GetOuter()->SetPoseParameter( pStudioHdr, iMoveYaw, flYaw );
			m_flLastMoveYaw = flYaw;

			// Now blend in his idle animation.
			// This makes the 8-way blend act like a 9-way blend by blending to 
			// an idle sequence as he slows down.
			bool bIsMoving;
			CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( MAIN_IDLE_SEQUENCE_LAYER );
			
			pLayer->SetWeight( 1 - CalcMovementPlaybackRate( &bIsMoving ) );
			if ( !bIsMoving )
			{
				pLayer->SetWeight( 1 );
			}

			if ( ShouldChangeSequences() )
			{
				// Whenever this layer stops blending, we can choose a new idle sequence to blend to, so he 
				// doesn't always use the same idle.
				if ( pLayer->GetWeight() < 0.02f || m_iCurrent8WayIdleSequence == -1 )
				{
					m_iCurrent8WayIdleSequence = m_pOuter->SelectWeightedSequence( ACT_IDLE );
					m_iCurrent8WayCrouchIdleSequence = m_pOuter->SelectWeightedSequence( ACT_CROUCHIDLE );
				}

				if ( m_eCurrentMainSequenceActivity == ACT_CROUCHIDLE || m_eCurrentMainSequenceActivity == ACT_RUN_CROUCH )
					pLayer->SetSequence( m_iCurrent8WayCrouchIdleSequence );
				else
					pLayer->SetSequence( m_iCurrent8WayIdleSequence );
			}
			
			pLayer->SetPlaybackRate( 1 );
			pLayer->SetCycle( pLayer->GetCycle() + m_pOuter->GetSequenceCycleRate( pStudioHdr, pLayer->GetSequence() ) * gpGlobals->frametime );
			pLayer->SetCycle( std::fmod( pLayer->GetCycle(), 1 ) );
			pLayer->SetOrder( MAIN_IDLE_SEQUENCE_LAYER );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayerAnimState::ComputePoseParam_BodyPitch( CStudioHdr *pStudioHdr )
{
	CVProfScope _( "CBasePlayerAnimState::ComputePoseParam_BodyPitch", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Get pitch from v_angle
	float flPitch = m_flEyePitch;
	if ( flPitch > 180.0f )
	{
		flPitch -= 360.0f;
	}
	flPitch = clamp( flPitch, -90, 90 );

	// See if we have a blender for pitch
	int pitch = GetOuter()->LookupPoseParameter( pStudioHdr, "body_pitch" );
	if ( pitch < 0 )
		return;

	GetOuter()->SetPoseParameter( pStudioHdr, pitch, flPitch );
	g_flLastBodyPitch = flPitch;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : goal - 
//			maxrate - 
//			dt - 
//			current - 
// Output : int
//-----------------------------------------------------------------------------
int CBasePlayerAnimState::ConvergeAngles( float goal,float maxrate, float maxgap, float dt, float& current )
{
	int direction = TURN_NONE;

	float anglediff = goal - current;
	anglediff = AngleNormalize( anglediff );
	
	float anglediffabs = fabs( anglediff );

	float scale = 1.0f;
	if ( anglediffabs <= FADE_TURN_DEGREES )
	{
		scale = anglediffabs / FADE_TURN_DEGREES;
		// Always do at least a bit of the turn ( 1% )
		scale = clamp( scale, 0.01f, 1.0f );
	}

	float maxmove = maxrate * dt * scale;

	if ( anglediffabs > maxgap )
	{
		// gap is too big, jump
		//maxmove = (anglediffabs - maxgap);
		float flTooFar = std::min( anglediffabs - maxgap, maxmove * 5 );
		if ( anglediff > 0 )
		{
			current += flTooFar;
		}
		else
		{
			current -= flTooFar;
		}
		current = AngleNormalize( current );
		anglediff = goal - current;
		anglediff = AngleNormalize( anglediff );
		anglediffabs = fabs( anglediff );
	}

	if ( anglediffabs < maxmove )
	{
		// we are close enought, just set the final value
		current = goal;
	}
	else
	{
		// adjust value up or down
		if ( anglediff > 0 )
		{
			current += maxmove;
			direction = TURN_LEFT;
		}
		else
		{
			current -= maxmove;
			direction = TURN_RIGHT;
		}
	}

	current = AngleNormalize( current );

	return direction;
}

void CBasePlayerAnimState::ComputePoseParam_BodyYaw()
{
	CVProfScope _( "CBasePlayerAnimState::ComputePoseParam_BodyYaw", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Find out which way he's running (m_flEyeYaw is the way he's looking).
	Vector vel;
	GetOuterAbsVelocity( vel );
	bool bIsMoving = vel.Length2D() > MOVING_MINIMUM_SPEED;

	// If we just initialized this guy (maybe he just came into the PVS), then immediately
	// set his feet in the right direction, otherwise they'll spin around from 0 to the 
	// right direction every time someone switches spectator targets.
	if ( !m_bCurrentFeetYawInitialized )
	{
		m_bCurrentFeetYawInitialized = true;
		m_flGoalFeetYaw = m_flCurrentFeetYaw = m_flEyeYaw;
		m_flLastTurnTime = 0.0f;
		m_bInFootPlantIdleTurn = false;
	}
	else if ( bIsMoving || m_bForceAimYaw )
	{
		// player is moving, feet yaw = aiming yaw
		if ( m_AnimConfig.m_LegAnimType == LEGANIM_9WAY || m_AnimConfig.m_LegAnimType == LEGANIM_8WAY )
		{
			// His feet point in the direction his eyes are, but they can run in any direction.
			m_flGoalFeetYaw = m_flEyeYaw;
		}
		else
		{
			m_flGoalFeetYaw = RAD2DEG( std::atan2( vel.y, vel.x ) );

			// If he's running backwards, flip his feet backwards.
			Vector vEyeYaw( std::cos( DEG2RAD( m_flEyeYaw ) ), std::sin( DEG2RAD( m_flEyeYaw ) ), 0 );
			Vector vFeetYaw( cos( DEG2RAD( m_flGoalFeetYaw ) ), sin( DEG2RAD( m_flGoalFeetYaw ) ), 0 );
			if ( vEyeYaw.Dot( vFeetYaw ) < -0.01 )
			{
				m_flGoalFeetYaw += 180;
			}
		}
		m_bInFootPlantIdleTurn = false;
	}
	else if ( (gpGlobals->curtime - m_flLastTurnTime) > mp_facefronttime.GetFloat() && m_flGoalFeetYaw != m_flEyeYaw )
	{
		// player didn't move & turn for quite some time
		if ( vel.Length2D() <= FOOTPLANT_MINIMUM_SPEED )
		{
			m_bInFootPlantIdleTurn = true;
			if ( m_flFootPlantIdleTurnCycle >= 1 )
				m_flFootPlantIdleTurnCycle = 0;
		}

		float flDiff = AngleNormalize(m_flGoalFeetYaw - m_flEyeYaw);
		m_bFootPlantIdleNeedToLiftFeet = (fabs(flDiff) > m_AnimConfig.m_flIdleFootPlantFootLiftDelta);

		m_flGoalFeetYaw = m_flEyeYaw;
	}
	else
	{
		// If he's rotated his view further than the model can turn, make him face forward.
		float flDiff = AngleNormalize(  m_flGoalFeetYaw - m_flEyeYaw );

		if ( fabs(flDiff) > m_AnimConfig.m_flMaxBodyYawDegrees )
		{
			if ( vel.Length2D() <= FOOTPLANT_MINIMUM_SPEED )
			{
				m_bInFootPlantIdleTurn = true;
				if ( m_flFootPlantIdleTurnCycle >= 1 )
					m_flFootPlantIdleTurnCycle = 0;
			}

			m_bFootPlantIdleNeedToLiftFeet = true;

			if ( flDiff  > 0 )
				m_flGoalFeetYaw -= m_AnimConfig.m_flMaxBodyYawDegreesCorrectionAmount;
			else
				m_flGoalFeetYaw += m_AnimConfig.m_flMaxBodyYawDegreesCorrectionAmount;
		}

		// If current yaw is significantly different from goal, abort idle foot ik to avoid intersecting the legs
		if ( m_bInFootPlantIdleTurn )
		{
			float flDiffYaw = AngleNormalize(m_flCurrentFeetYaw - m_flGoalFeetYaw);
			if ( fabs(flDiffYaw) > m_AnimConfig.m_flIdleFootPlantMaxYaw )
			{
				m_bInFootPlantIdleTurn = false;
				m_flFootPlantIdleTurnCycle = 0;
			}
		}
	}

	m_flGoalFeetYaw = AngleNormalize( m_flGoalFeetYaw );

	if ( m_flCurrentFeetYaw != m_flGoalFeetYaw )
	{
		if ( m_bForceAimYaw )
		{
			m_flCurrentFeetYaw = m_flGoalFeetYaw;
		}
		else
		{
			ConvergeAngles( m_flGoalFeetYaw, GetFeetYawRate(), m_AnimConfig.m_flMaxBodyYawDegrees, gpGlobals->frametime, m_flCurrentFeetYaw );
		}

		m_flLastTurnTime = gpGlobals->curtime;
	}

	// Turn off a force aim yaw - either we have already updated or we don't need to.
	m_bForceAimYaw = false;

	float flCurrentTorsoYaw = AngleNormalize( m_flEyeYaw - m_flCurrentFeetYaw );

	// Rotate entire body into position
	m_angRender[YAW] = m_flCurrentFeetYaw;
	m_angRender[PITCH] = m_angRender[ROLL] = 0;
		
	SetOuterBodyYaw( flCurrentTorsoYaw );
	g_flLastBodyYaw = flCurrentTorsoYaw;
}

float CBasePlayerAnimState::SetOuterBodyYaw( float flValue )
{
	int body_yaw = GetOuter()->LookupPoseParameter( "body_yaw" );
	if ( body_yaw < 0 )
	{
		return 0;
	}

	SetOuterPoseParameter( body_yaw, flValue );
	return flValue;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : activity - 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CBasePlayerAnimState::BodyYawTranslateActivity( Activity activity )
{
	// Not even standing still, sigh
	if ( activity != ACT_IDLE )
		return activity;

	// Not turning
	switch ( m_nTurningInPlace )
	{
	default:
	case TURN_NONE:
		return activity;
	case TURN_RIGHT:
	case TURN_LEFT:
		return mp_ik.GetBool() ? ACT_TURN : activity;
	}

	contract_assert( 0 );
	return activity;
}

const QAngle& CBasePlayerAnimState::GetRenderAngles()
{
	return m_angRender;
}

void CBasePlayerAnimState::SetForceAimYaw( bool bForce )
{
	m_bForceAimYaw = bForce;
}

void CBasePlayerAnimState::GetOuterAbsVelocity( Vector& vel ) const
{
	GetOuter()->EstimateAbsVelocity( vel );
}

float CBasePlayerAnimState::GetOuterXYSpeed() const
{
	Vector vel;
	GetOuterAbsVelocity( vel );
	return vel.Length2D();
}

// -----------------------------------------------------------------------------
void CBasePlayerAnimState::AnimStateLog( const char *pMsg, ... )
{
	// Format the string.
	char str[4096];
	std::va_list marker;
	__builtin_va_start( marker, pMsg );
	V_vsnprintf( str, sizeof( str ), pMsg, marker );
	__builtin_va_end( marker );

	// Log it?	
	if ( showanimstate_log.GetInt() == 1 || showanimstate_log.GetInt() == 3 )
	{
		Msg( "%s", str );
	}

	if ( showanimstate_log.GetInt() > 1 )
	{
		const char *fname = "AnimStateClient.log";
		static FileHandle_t hFile = filesystem->Open( fname, "wt" );
		filesystem->FPrintf( hFile, "%s", str );
		filesystem->Flush( hFile );
	}
}

// -----------------------------------------------------------------------------
void CBasePlayerAnimState::AnimStatePrintf( int iLine, const char *pMsg, ... )
{
	// Format the string.
	char str[4096];
	std::va_list marker;
	__builtin_va_start( marker, pMsg );
	V_vsnprintf( str, sizeof( str ), pMsg, marker );
	__builtin_va_end( marker );

	// Show it with Con_NPrintf.
	engine->Con_NPrintf( iLine, "%s", str );

	// Log it.
	AnimStateLog( "%s\n", str );
}

// -----------------------------------------------------------------------------
void CBasePlayerAnimState::DebugShowAnimState( int iStartLine )
{
	Vector vOuterVel;
	GetOuterAbsVelocity( vOuterVel );

	int iLine = iStartLine;
	AnimStatePrintf( iLine++, "main: %s(%d), cycle: %.2f cyclerate: %.2f playbackrate: %.2f\n", 
		GetSequenceName( m_pOuter->GetModelPtr(), m_pOuter->GetSequence() ), 
		m_pOuter->GetSequence(),
		m_pOuter->GetCycle(), 
		m_pOuter->GetSequenceCycleRate(m_pOuter->GetModelPtr(), m_pOuter->GetSequence()),
		m_pOuter->GetPlaybackRate()
		);

	if ( m_AnimConfig.m_LegAnimType == LEGANIM_8WAY )
	{
		CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( MAIN_IDLE_SEQUENCE_LAYER );

		AnimStatePrintf( iLine++, "idle: %s, weight: %.2f\n",
			GetSequenceName( m_pOuter->GetModelPtr(), pLayer->GetSequence() ), 
			(float)pLayer->GetWeight() );
	}

	for ( int i=0; i < m_pOuter->GetNumAnimOverlays()-1; i++ )
	{
		CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( AIMSEQUENCE_LAYER + i );
		AnimStatePrintf( iLine++, "%s(%d), weight: %.2f, cycle: %.2f, order (%d), aim (%d)", 
			!pLayer->IsActive() ? "-- ": (pLayer->GetSequence() == 0 ? "-- " : (showanimstate_activities.GetBool()) ? GetSequenceActivityName( m_pOuter->GetModelPtr(), pLayer->GetSequence() ) : GetSequenceName( m_pOuter->GetModelPtr(), pLayer->GetSequence() ) ), 
			!pLayer->IsActive() ? 0 : (int)pLayer->GetSequence(), 
			!pLayer->IsActive() ? 0 : (float)pLayer->GetWeight(), 
			!pLayer->IsActive() ? 0 : (float)pLayer->GetCycle(), 
			!pLayer->IsActive() ? 0 : (int)pLayer->GetOrder(),
			i
			);
	}

	AnimStatePrintf( iLine++, "vel: %.2f, time: %.2f, max: %.2f, animspeed: %.2f", 
		vOuterVel.Length2D(), gpGlobals->curtime, GetInterpolatedGroundSpeed(), m_pOuter->GetSequenceGroundSpeed(m_pOuter->GetSequence()) );
	
	if ( m_AnimConfig.m_LegAnimType == LEGANIM_8WAY )
	{
		AnimStatePrintf( iLine++, "ent yaw: %.2f, body_yaw: %.2f, move_yaw: %.2f, gait_yaw: %.2f, body_pitch: %.2f", 
			m_angRender[YAW], g_flLastBodyYaw, m_flLastMoveYaw, m_flGaitYaw, g_flLastBodyPitch );
	}
	else
	{
		AnimStatePrintf( iLine++, "ent yaw: %.2f, body_yaw: %.2f, body_pitch: %.2f, move_x: %.2f, move_y: %.2f", 
			m_angRender[YAW], g_flLastBodyYaw, g_flLastBodyPitch, m_vLastMovePose.x, m_vLastMovePose.y );
	}

	// Draw a red triangle on the ground for the eye yaw.
	float flBaseSize = 10;
	float flHeight = 80;
	Vector vBasePos = GetOuter()->GetAbsOrigin() + Vector( 0, 0, 3 );
	QAngle angles( 0, 0, 0 );
	angles[YAW] = m_flEyeYaw;
	Vector vForward, vRight, vUp;
	AngleVectors( angles, &vForward, &vRight, &vUp );
	debugoverlay->AddTriangleOverlay( vBasePos+vRight*flBaseSize/2, vBasePos-vRight*flBaseSize/2, vBasePos+vForward*flHeight, 255, 0, 0, 255, false, gpGlobals->frametime );

	// Draw a blue triangle on the ground for the body yaw.
	angles[YAW] = m_angRender[YAW];
	AngleVectors( angles, &vForward, &vRight, &vUp );
	debugoverlay->AddTriangleOverlay( vBasePos+vRight*flBaseSize/2, vBasePos-vRight*flBaseSize/2, vBasePos+vForward*flHeight, 0, 0, 255, 255, false, gpGlobals->frametime );

	//left limit
	angles[YAW] = m_flEyeYaw - 60;
	AngleVectors(angles, &vForward, &vRight, &vUp);
	debugoverlay->AddTriangleOverlay(vBasePos + vRight*flBaseSize / 4, vBasePos - vRight*flBaseSize / 4, vBasePos + vForward*flHeight, 0, 255, 0, 100, false, gpGlobals->frametime);

	//right limit
	angles[YAW] = m_flEyeYaw + 60;
	AngleVectors(angles, &vForward, &vRight, &vUp);
	debugoverlay->AddTriangleOverlay(vBasePos + vRight*flBaseSize / 4, vBasePos - vRight*flBaseSize / 4, vBasePos + vForward*flHeight, 0, 255, 0, 100, false, gpGlobals->frametime);

}

// -----------------------------------------------------------------------------
void CBasePlayerAnimState::DebugShowAnimStateFull( int iStartLine )
{
	AnimStateLog( "----------------- frame %d -----------------\n", gpGlobals->framecount );

	DebugShowAnimState( iStartLine );

	AnimStateLog( "--------------------------------------------\n\n" );
}

// -----------------------------------------------------------------------------
int CBasePlayerAnimState::SelectWeightedSequence( Activity activity ) 
{
	return GetOuter()->SelectWeightedSequence( activity ); 
}

float CBasePlayerAnimState::GetFeetYawRate( void )
{
	return mp_feetyawrate.GetFloat();
}

Activity CBasePlayerAnimState::GetCurrentMainSequenceActivity() const
{
	return m_eCurrentMainSequenceActivity;
}

class CBaseAchievement;
class CBaseEntity;

using namespace ::ks::reflect;

CBaseAchievementHelper *CBaseAchievementHelper::s_pFirst = nullptr;
static int g_nAchivementBitchCount = 0;

IMPLEMENT_REFLECT_DATAMAP_NO_BASE( CBaseAchievement )

//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CBaseAchievement::CBaseAchievement()
{
	m_iFlags = 0;
	m_iGoal = 0;
	m_iProgressMsgIncrement = 0;
	m_iProgressMsgMinimum = 0;
	m_iAchievementID = 0;
	m_iPointValue = 0;
	m_bHideUntilAchieved = false;
	m_pVictimClassNameFilter = nullptr;
	m_pAttackerClassNameFilter = nullptr;
	m_pInflictorClassNameFilter = nullptr;
	m_pInflictorEntityNameFilter = nullptr;
	m_pMapNameFilter = nullptr;
	m_pGameDirFilter = nullptr;
	m_pszComponentNames = nullptr;
	m_pszComponentDisplayNames = nullptr;
	m_pszComponentPrefix = nullptr;
	m_iNumComponents = 0;
	m_iComponentPrefixLen = 0;
	m_iComponentBits = 0;
	m_iCount = 0;
	m_iProgressShown = 0;
	m_bAchieved = false;
	m_uUnlockTime = 0;
	m_pAchievementMgr = nullptr;
	m_nUserSlot = 0;
	m_iAssetAwardID = 0;
	m_bShowOnHUD = false;
}

//-----------------------------------------------------------------------------
// Purpose: sets flags
//-----------------------------------------------------------------------------
void CBaseAchievement::SetFlags( int iFlags ) 
{ 
	// must always specify a save method
	contract_assert( iFlags & ( ACH_SAVE_WITH_GAME | ACH_SAVE_GLOBAL ) );

	m_iFlags = iFlags; 
}

//-----------------------------------------------------------------------------
// Purpose: called when a game event being listened for is dispatched
//-----------------------------------------------------------------------------
void CBaseAchievement::FireGameEvent( IGameEvent *event )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nUserSlot );
	//
	// Perform common filtering to make it simpler to write achievements
	//
	if ( !IsActive() )
	{
		return;
	}

	// if the achievement only applies to a specific map, and it's not the current map, skip it
	if ( m_pMapNameFilter && ( 0 != V_strcmp( m_pAchievementMgr->GetMapName(), m_pMapNameFilter ) ) )
		return;

	const char *name = event->GetName();
	if ( 0 == V_strcmp( name, "teamplay_round_win" ) )
	{
		// if this is a round win and the achievement wants full round events only, filter this out
		// if this is not the end of a full round
		if ( ( m_iFlags & ACH_FILTER_FULL_ROUND_ONLY ) && ( false == event->GetBool( "full_round" ) ) )
			return;
	}

	// let the achievement handle the event
	FireGameEvent_Internal( event );
}

//-----------------------------------------------------------------------------
// Purpose: sets victim class to filter with
//-----------------------------------------------------------------------------
void CBaseAchievement::SetVictimFilter( const char *pClassName )
{
	m_pVictimClassNameFilter = pClassName;
}

//-----------------------------------------------------------------------------
// Purpose: sets attacker class to filter with
//-----------------------------------------------------------------------------
void CBaseAchievement::SetAttackerFilter( const char *pClassName )
{
	m_pAttackerClassNameFilter = pClassName;
}

//-----------------------------------------------------------------------------
// Purpose: sets inflictor class to filter with
//-----------------------------------------------------------------------------
void CBaseAchievement::SetInflictorFilter( const char *pClassName )
{
	m_pInflictorClassNameFilter = pClassName;
}

//-----------------------------------------------------------------------------
// Purpose: sets inflictor entity name to filter with
//-----------------------------------------------------------------------------
void CBaseAchievement::SetInflictorEntityNameFilter( const char *pEntityName )
{
	m_pInflictorEntityNameFilter = pEntityName;
}

//-----------------------------------------------------------------------------
// Purpose: sets map name to filter with
//-----------------------------------------------------------------------------
void CBaseAchievement::SetMapNameFilter( const char *pMapName )
{
	m_pMapNameFilter = pMapName;
}

//-----------------------------------------------------------------------------
// Purpose: sets game dir to filter with.  Note: in general, achievements should
//			only be compiled into products they pertain to.  But if there are
//			any game-specific achievements which need to be in a binary shared
//			across products (e.g. Ep1 & Ep2), use the game dir as a runtime
//			filter.
//-----------------------------------------------------------------------------
void CBaseAchievement::SetGameDirFilter( const char *pGameDir )
{
	m_pGameDirFilter = pGameDir;
}

//-----------------------------------------------------------------------------
// Purpose: sets prefix to look for in map event string to identify a component
//			for this achievement
//-----------------------------------------------------------------------------
void CBaseAchievement::SetComponentPrefix( const char *pPrefix )
{
	m_pszComponentPrefix = pPrefix;
	m_iComponentPrefixLen = V_strlen( pPrefix );
}

//-----------------------------------------------------------------------------
// Purpose: called when a kill that passes filter critera occurs.  This
//			is the default implementation, achievements can override to
//			do special handling
//-----------------------------------------------------------------------------
void CBaseAchievement::Event_EntityKilled( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event )
{
	// extra paranoid check: should only get here if registered as a kill event listener
	if ( !( GetFlags() & ACH_LISTEN_KILL_EVENTS ) )
		return;

	// default implementation is just to increase count when filter criteria pass
	IncrementCount();
}

//-----------------------------------------------------------------------------
// Purpose: called when an event that counts toward an achievement occurs
//-----------------------------------------------------------------------------
void CBaseAchievement::IncrementCount( int iOptIncrement )
{
	if ( !IsAchieved() )
	{
		if ( !CheckAchievementsEnabled() )
		{
			if ( g_nAchivementBitchCount++ < 10 )
			{
				DevMsg( "Achievements disabled, ignoring achievement progress for %s\n", GetName() );
			}
			return;
		}

		// on client, where the count is kept, increment count
		if ( iOptIncrement > 0 )
		{
			// user specified that we want to increase by more than one.
			m_iCount += iOptIncrement;
			if ( m_iCount > m_iGoal )
			{
				m_iCount = m_iGoal;
			}
		}
		else
		{
			m_iCount++;
		}

		// if this achievement gets saved w/global state, flag our global state as dirty
		if ( GetFlags() & ACH_SAVE_GLOBAL )
		{
			m_pAchievementMgr->SetDirty( true, m_nUserSlot );
		}

		if ( cc_achievement_debug.GetInt() )
		{
			Msg( "Achievement count increased for %s: %d/%d\n", GetName(), m_iCount, m_iGoal );
		}

		// if we've hit goal, award the achievement
		if ( m_iGoal > 0 )
		{
			if ( m_iCount >= m_iGoal )
			{
				AwardAchievement();
			}
			else
			{	
				HandleProgressUpdate();
			}
		}
	}
}

void CBaseAchievement::SetShowOnHUD( bool bShow )
{
	if ( m_bShowOnHUD != bShow )
	{
		m_pAchievementMgr->SetDirty( true, m_nUserSlot );
	}

	m_bShowOnHUD = bShow;
}

void CBaseAchievement::HandleProgressUpdate()
{
	// if we've hit the right # of progress steps to show a progress notification, show it
	if ( ( m_iProgressMsgIncrement > 0 ) && m_iCount >= m_iProgressMsgMinimum && ( 0 == ( m_iCount % m_iProgressMsgIncrement ) ) )
	{
		// which notification is this
		int iProgress = m_iCount / m_iProgressMsgIncrement;
		// if we haven't already shown this progress step, show it
		if ( iProgress > m_iProgressShown )
		{
			ShowProgressNotification();
			// remember progress step shown so we don't show it again if the player loads an earlier save game
			// and gets past this point again
			m_iProgressShown = iProgress;
			m_pAchievementMgr->SetDirty( true, m_nUserSlot );
		}					
	}
}

//-----------------------------------------------------------------------------
// Purpose: calculates at how many steps we should show a progress notification
//-----------------------------------------------------------------------------
void CBaseAchievement::CalcProgressMsgIncrement()
{
	// by default, show progress at every 25%
	m_iProgressMsgIncrement = m_iGoal / 4;
	// if goal is not evenly divisible by 4, try some other values
	if ( 0 != ( m_iGoal % 4 ) )
	{
		if ( 0 == ( m_iGoal % 3 ) )
		{
			// if evenly divisible by 3, use that
			m_iProgressMsgIncrement = m_iGoal / 3;
		}
		else if ( 0 == ( m_iGoal % 5 ) )
		{
			// if evenly divisible by 5, use that
			m_iProgressMsgIncrement = m_iGoal / 5;
		}
		// otherwise stick with divided by 4, rounded off
	}

	// don't show progress notifications for less than 5 things
	if ( m_iProgressMsgIncrement < 5 )
	{
		m_iProgressMsgIncrement = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseAchievement::SetNextThink( float flThinkTime ) 
{ 
	m_pAchievementMgr->SetAchievementThink( this, flThinkTime ); 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseAchievement::ClearThink( void ) 
{ 
	m_pAchievementMgr->SetAchievementThink( this, THINK_CLEAR ); 
}

//-----------------------------------------------------------------------------
// Purpose: see if we should award an achievement based on what just happened
//-----------------------------------------------------------------------------
void CBaseAchievement::EvaluateNewAchievement()
{
	if ( !IsAchieved() && m_iGoal > 0 && m_iCount >= m_iGoal )
	{
		m_pAchievementMgr->CheckSignInState( false ); // Local user isn't resolvable, but we know we are signed in (our caller checks).
		AwardAchievement();
		m_pAchievementMgr->CheckSignInState( true );

		CheckAssetAwards( m_nUserSlot );	// Since we awarded the achievement we might need to award associated assets as well
	}
}

//-----------------------------------------------------------------------------
// Purpose: determine if we should set this achievement to be achieved based
//			on other state.  Used at init time.
//-----------------------------------------------------------------------------
void CBaseAchievement::EvaluateIsAlreadyAchieved()
{
	EvaluateNewAchievement();
}

//-----------------------------------------------------------------------------
// Purpose: called a map event for this achievement occurs
//-----------------------------------------------------------------------------
void CBaseAchievement::OnMapEvent( const char *pEventName )
{
	contract_assert( m_iFlags & ACH_LISTEN_MAP_EVENTS );

	if ( 0 == V_stricmp( pEventName, GetName() ) )
	{
		IncrementCount();
	}
}

//-----------------------------------------------------------------------------
// Purpose: called when an achievement is awarded
//-----------------------------------------------------------------------------
void CBaseAchievement::AwardAchievement()
{
	if ( IsAchieved() )
		return;

	m_pAchievementMgr->AwardAchievement( m_iAchievementID, m_nUserSlot );
}

//-----------------------------------------------------------------------------
// Purpose: called when a component of a multi-component event is found
//-----------------------------------------------------------------------------
void CBaseAchievement::OnComponentEvent( const char *pchComponentName )
{
	// find the component name in our list
	for ( int i = 0; i < m_iNumComponents; i++ )
	{
		if ( 0 == V_strcmp( pchComponentName, m_pszComponentNames[i] ) )
		{
			EnsureComponentBitSetAndEvaluate( i );
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets the specified component bit # if it is not already.
//			If it does get set, evaluate if this satisfies an achievement
//-----------------------------------------------------------------------------
void CBaseAchievement::EnsureComponentBitSetAndEvaluate( int iBitNumber )
{
	contract_assert( iBitNumber < 64 );	// this is bit #, not a bit mask

	if ( IsAchieved() )
		return;

	// calculate which bit this component corresponds to
	uint64 iBitMask = ( (uint64) 1 ) << iBitNumber;

	// see if we already have gotten this component
	if ( 0 == ( iBitMask & m_iComponentBits ) )
	{				
		if ( !CheckAchievementsEnabled() )
		{
			Msg( "Achievements disabled, ignoring achievement component for %s\n", GetName() );
			return;
		}

		// new component, set the bit and increment the count
		SetComponentBits( m_iComponentBits | iBitMask );

		contract_assert( m_iCount <= m_iGoal );
		if ( m_iCount == m_iGoal )
		{
			// all components found, award the achievement (and save state)
			AwardAchievement();
		}
		else
		{
			// save our state at the next good opportunity
			m_pAchievementMgr->SetDirty( true, m_nUserSlot );		

			if ( cc_achievement_debug.GetInt() )
			{
				Msg( "Component %d for achievement %s found\n", iBitNumber, GetName() );
			}

			ShowProgressNotification();
		}				
	}
	else
	{
		if ( cc_achievement_debug.GetInt() )
		{
			Msg( "Component %d for achievement %s found, but already had that component\n", iBitNumber, GetName() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: displays achievement progress notification in the HUD
//-----------------------------------------------------------------------------
void CBaseAchievement::ShowProgressNotification()
{
	if ( !ShouldShowProgressNotification() )
		return;

	IGameEvent *event = gameeventmanager->CreateEvent( "achievement_event" );
	if ( event )
	{
		event->SetString( "achievement_name", GetName() );
		event->SetInt( "cur_val", m_iCount );
		event->SetInt( "max_val", m_iGoal );
		gameeventmanager->FireEventClientSide( event );
	}	
}

//-----------------------------------------------------------------------------
// Purpose: clears dynamic state for this achievement
//-----------------------------------------------------------------------------
void CBaseAchievement::PreRestoreSavedGame()
{
	// if this achievement gets saved with the game, clear its state
	if ( m_iFlags & ACH_SAVE_WITH_GAME )
	{
		m_iCount = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: called after the data in this achievement has been restored from saved game
//-----------------------------------------------------------------------------
void CBaseAchievement::PostRestoreSavedGame()
{
	EvaluateIsAlreadyAchieved();
}

//-----------------------------------------------------------------------------
// Purpose: sets component bits for this achievement
//-----------------------------------------------------------------------------
void CBaseAchievement::SetComponentBits( uint64 iComponentBits ) 
{ 
	contract_assert( m_iFlags & ACH_HAS_COMPONENTS );
	// set the bit field
	m_iComponentBits = iComponentBits; 
	// count how many bits are set and save that as the count
	int iNumBitsSet = 0;
	while ( iComponentBits > 0 )
	{
		if ( iComponentBits & 1 )
		{
			iNumBitsSet++;
		}
		iComponentBits >>= 1;
	}
	m_iCount = iNumBitsSet;
}

//-----------------------------------------------------------------------------
// Purpose: gets the number of bits that are set in our components
//-----------------------------------------------------------------------------
int CBaseAchievement::GetNumComponentBitsSet( void )
{
	int iNumBitsSet = 0;

	uint64 iComponentBits = m_iComponentBits;

	while ( iComponentBits > 0 )
	{
		if ( iComponentBits & 1 )
		{
			iNumBitsSet++;
		}
		iComponentBits >>= 1;
	}

	return iNumBitsSet;
}

//-----------------------------------------------------------------------------
// Purpose: returns whether we should save this achievement with a save game
//-----------------------------------------------------------------------------
bool CBaseAchievement::ShouldSaveWithGame() 
{ 
	// save if we should get saved with the game, have a non-zero count, and have not
	// been achieved (at which point the achievement state gets saved globally)
	return ( ( m_iFlags & ACH_SAVE_WITH_GAME ) > 0 && ( GetCount() > 0 ) && !IsAchieved() );
}

//-----------------------------------------------------------------------------
// Purpose: returns whether we should save this achievement to the global file
//-----------------------------------------------------------------------------
bool CBaseAchievement::ShouldSaveGlobal() 
{ 
	// save if we should get saved globally and have a non-zero count, or if we have been achieved, or if the player has pinned this achievement to the HUD
	return ( ( ( m_iFlags & ACH_SAVE_GLOBAL ) > 0 && ( GetCount() > 0 ) ) || IsAchieved() || ( m_iProgressShown > 0 ) || ShouldShowOnHUD() );
}

//-----------------------------------------------------------------------------
// Purpose: returns whether this achievement is active
//-----------------------------------------------------------------------------
bool CBaseAchievement::IsActive()
{
	// we're not active if already achieved
	if ( IsAchieved() )	
		return false;
	
	// if there's a map filter and we're not on the specified map, we're not active
	if ( ( m_pMapNameFilter ) && ( 0 != V_strcmp( m_pAchievementMgr->GetMapName(), m_pMapNameFilter ) ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: returns whether this achievement is available.  It can be made
// not available if it requires DLC, etc
//-----------------------------------------------------------------------------
bool CBaseAchievement::IsAvailable()
{
	return true; // by default all achievements are available
}

//-----------------------------------------------------------------------------
// Purpose: Serialize our data to the KeyValues node
//-----------------------------------------------------------------------------
void CBaseAchievement::GetSettings( KeyValues *pNodeOut )
{
	pNodeOut->SetInt( "value", IsAchieved() ? 1 : 0 );

	if ( HasComponents() )
	{
		pNodeOut->SetUint64( "data", m_iComponentBits );
	}
	else
	{
		if ( !IsAchieved() )
		{
			pNodeOut->SetInt( "data", m_iCount );
		}
	}
	pNodeOut->SetInt( "hud", ShouldShowOnHUD() ? 1 : 0 );
	pNodeOut->SetInt( "msg", m_iProgressShown );
}

//-----------------------------------------------------------------------------
// Purpose: Unserialize our data from the KeyValues node
//-----------------------------------------------------------------------------
void CBaseAchievement::ApplySettings( KeyValues *pNodeIn )
{
	// set the count
	if ( pNodeIn->GetInt( "value" ) > 0 )
	{
		m_iCount = m_iGoal;
		m_bAchieved = true;
	}
	else if ( !HasComponents() )
	{
		m_iCount = pNodeIn->GetInt( "data" );
	}

	// if this achievement has components, set the component bits
	if ( HasComponents() )
	{
		int64 iComponentBits = pNodeIn->GetUint64( "data" );
		SetComponentBits( iComponentBits );
	}
	SetShowOnHUD( !!pNodeIn->GetInt( "hud" ) );
	m_iProgressShown = pNodeIn->GetInt( "msg" );
}

//-----------------------------------------------------------------------------
// Purpose: Clears achievement data
//-----------------------------------------------------------------------------
void CBaseAchievement::ClearAchievementData()
{
	SetCount( 0 );
	if ( this->HasComponents() )
	{
		this->SetComponentBits( 0 );
	}
	SetAchieved( false );
}

//-----------------------------------------------------------------------------
// Purpose: get the localizable display string for a specific component
//-----------------------------------------------------------------------------
const char *CBaseAchievement::GetComponentDisplayString( int iComponent )
{
	if ( !( m_iFlags & ACH_HAS_COMPONENTS ) || iComponent < 0 || iComponent > m_iNumComponents )
		return "";

	return m_pszComponentDisplayNames[iComponent];
}

//-----------------------------------------------------------------------------
// Purpose: Translate the asset string into an xbox asset id.
//-----------------------------------------------------------------------------
void CBaseAchievement::SetAssetAward( const char* assetAwardName )
{
	TitleAvatarAwardsDescription_t const *pTitleAssetMap = g_pMatchFramework->GetMatchTitle()->DescribeTitleAvatarAwards();
	while ( pTitleAssetMap && pTitleAssetMap->m_szAvatarAwardName )
	{
		if ( !V_stricmp( pTitleAssetMap->m_szAvatarAwardName, assetAwardName ) )
		{
			SetAssetAwardID( pTitleAssetMap->m_idAvatarAward );
			return;
		}
		else
		{
			++pTitleAssetMap;
		}
	}

	SetAssetAwardID( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Wrapper for achievement manager so we can check enabled per achievement with the gamerules
//-----------------------------------------------------------------------------
bool CBaseAchievement::CheckAchievementsEnabled( void )
{
	if ( g_pGameRules && !g_pGameRules->CheckAchievementsEnabled( GetAchievementID() ) )
		return false;

	return m_pAchievementMgr->CheckAchievementsEnabled();
}

CBaseAchievementHelper::CBaseAchievementHelper( achievementCreateFunc createFunc)
{
	m_pfnCreate = createFunc;
	m_pNext = s_pFirst;
	s_pFirst = this;
}

//-----------------------------------------------------------------------------
// Constructor, destructor: 
//-----------------------------------------------------------------------------

CBaseAnimatedTextureProxy::CBaseAnimatedTextureProxy()
{
	Cleanup();
}

CBaseAnimatedTextureProxy::~CBaseAnimatedTextureProxy()
{
	Cleanup();
}

//-----------------------------------------------------------------------------
// Initialization, shutdown
//-----------------------------------------------------------------------------
bool CBaseAnimatedTextureProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	char const* pAnimatedTextureVarName = pKeyValues->GetString( "animatedTextureVar" );
	if( !pAnimatedTextureVarName )
		return false;

	bool foundVar;
	m_AnimatedTextureVar = pMaterial->FindVar( pAnimatedTextureVarName, &foundVar, false );
	if( !foundVar )
		return false;

	char const* pAnimatedTextureFrameNumVarName = pKeyValues->GetString( "animatedTextureFrameNumVar" );
	if( !pAnimatedTextureFrameNumVarName )
		return false;

	m_AnimatedTextureFrameNumVar = pMaterial->FindVar( pAnimatedTextureFrameNumVarName, &foundVar, false );
	if( !foundVar )
		return false;

	m_FrameRate = pKeyValues->GetFloat( "animatedTextureFrameRate", 15 );
	m_WrapAnimation = !pKeyValues->GetInt( "animationNoWrap", 0 );
	return true;
}

void CBaseAnimatedTextureProxy::Cleanup()
{
	m_AnimatedTextureVar = nullptr;
	m_AnimatedTextureFrameNumVar = nullptr;
}

//-----------------------------------------------------------------------------
// Does the dirty deed
//-----------------------------------------------------------------------------
void CBaseAnimatedTextureProxy::OnBind( void *pEntity )
{
	contract_assert( m_AnimatedTextureVar );

	if( m_AnimatedTextureVar->GetType() != MATERIAL_VAR_TYPE_TEXTURE )
	{
		return;
	}
	ITexture *pTexture;
	pTexture = m_AnimatedTextureVar->GetTextureValue();
	int numFrames = pTexture->GetNumAnimationFrames();

	if ( numFrames <= 0 )
	{
		contract_assert( !"0 frames in material calling animated texture proxy" );
		return;
	}

	// NOTE: Must not use relative time based methods here
	// because the bind proxy can be called many times per frame.
	// Prevent multiple Wrap callbacks to be sent for no wrap mode
	float startTime = GetAnimationStartTime(pEntity);
	float deltaTime = gpGlobals->curtime - startTime;
	float prevTime = deltaTime - gpGlobals->frametime;

	// Clamp..
	if (deltaTime < 0.0f)
		deltaTime = 0.0f;
	if (prevTime < 0.0f)
		prevTime = 0.0f;

	float frame = m_FrameRate * deltaTime;	
	float prevFrame = m_FrameRate * prevTime;

	int intFrame = ((int)frame) % numFrames; 
	int intPrevFrame = ((int)prevFrame) % numFrames;

	// Report wrap situation...
	if (intPrevFrame > intFrame)
	{
		if (m_WrapAnimation)
		{
			AnimationWrapped( pEntity );
		}
		else
		{
			// Only sent the wrapped message once.
			// when we're in non-wrapping mode
			if (prevFrame < numFrames)
				AnimationWrapped( pEntity );
			intFrame = numFrames - 1;
		}
	}

	m_AnimatedTextureFrameNumVar->SetIntValue( intFrame );
}

IMaterial *CBaseAnimatedTextureProxy::GetMaterial()
{
	return m_AnimatedTextureVar->GetOwningMaterial();
}
