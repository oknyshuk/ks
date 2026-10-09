// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
import std;
import ks.core;
#if defined(CLIENT_DLL)
import ks.tier1;
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#elif defined(GAME_DLL)
import ks.math;
#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "base_transmit_proxy.h"
#include "soundflags.h"
#include "networkvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "physics.h"
#include "ndebugoverlay.h"
#endif
#include "death_pose.h"

#ifdef CLIENT_DLL

void GetRagdollCurSequenceWithDeathPose( C_BaseAnimating *entity, matrix3x4a_t *curBones, float flTime, int activity, int frame )
{
	// blow the cached prev bones
	entity->InvalidateBoneCache();

	Vector vPrevOrigin = entity->GetAbsOrigin();

	entity->Interpolate( flTime );
	
	if ( activity != ACT_INVALID )
	{
		Vector vNewOrigin = entity->GetAbsOrigin();
		Vector vDirection = vNewOrigin - vPrevOrigin;

		float flVelocity = VectorNormalize( vDirection );

		Vector vAdjustedOrigin = vNewOrigin + vDirection * ( ( flVelocity * flVelocity ) * gpGlobals->frametime );

		int iTempSequence = entity->GetSequence();
		float flTempCycle = entity->GetCycle();

		entity->SetEffects( EF_NOINTERP );

		entity->SetSequence( activity );

		entity->SetCycle( (float)frame / MAX_DEATHPOSE_FRAMES );

		entity->SetAbsOrigin( vAdjustedOrigin );

		// Now do the current bone setup
		entity->SetupBones( curBones, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, flTime );

		entity->SetAbsOrigin( vNewOrigin );

		// blow the cached prev bones
		entity->InvalidateBoneCache();

		entity->SetSequence( iTempSequence );
		entity->SetCycle( flTempCycle );

		entity->Interpolate( gpGlobals->curtime );

		entity->SetupBones( nullptr, -1, BONE_USED_BY_ANYTHING, gpGlobals->curtime );

		entity->RemoveEffects( EF_NOINTERP );
	}
	else
	{
		entity->SetupBones( curBones, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, flTime );

		// blow the cached prev bones
		entity->InvalidateBoneCache();

		entity->SetupBones( nullptr, -1, BONE_USED_BY_ANYTHING, flTime );
	}
}

#else // !CLIENT_DLL

Activity GetDeathPoseActivity( CBaseAnimating *entity, const CTakeDamageInfo &info )
{
	if ( !entity )
	{
		return ACT_INVALID;
	}

	// Get the entity's current activity.
	Activity aCurrentActivity = entity->GetSequenceActivity( entity->GetSequence() );

	// Determine if the enitiy is crouched or not
	bool bCrouched = false;
	switch ( aCurrentActivity )
	{
		case ACT_CROUCHIDLE:
		case ACT_RUN_CROUCH:
			bCrouched = true;
			break;
	}

	Activity aActivity;

	Vector vForward, vRight;
	entity->GetVectors( &vForward, &vRight, nullptr );

	Vector vDir = -info.GetDamageForce();
	VectorNormalize( vDir );

	float flDotForward	= DotProduct( vForward, vDir );
	float flDotRight	= DotProduct( vRight, vDir );

	bool bNegativeForward = false;
	bool bNegativeRight = false;

	if ( flDotForward < 0.0f )
	{
		bNegativeForward = true;
		flDotForward = flDotForward * -1;
	}

	if ( flDotRight < 0.0f )
	{
		bNegativeRight = true;
		flDotRight = flDotRight * -1;
	}

	if ( flDotRight > flDotForward )
	{
		if ( bNegativeRight == true )
		{
			if ( bCrouched == true )
				aActivity = ACT_DIE_CROUCH_LEFTSIDE;
			else
				aActivity = ACT_DIE_LEFTSIDE;
		}
		else 
		{
			if ( bCrouched == true )
				aActivity = ACT_DIE_CROUCH_RIGHTSIDE;
			else
				aActivity = ACT_DIE_RIGHTSIDE;
		}
	}
	else
	{
		if ( bNegativeForward == true )
		{
			if ( bCrouched == true )
				aActivity = ACT_DIE_CROUCH_BACKSIDE;
			else
				aActivity = ACT_DIE_BACKSIDE;
		}
		else 
		{
			if ( bCrouched == true )
				aActivity = ACT_DIE_CROUCH_FRONTSIDE;
			else
				aActivity = ACT_DIE_FRONTSIDE;
		}
	}

	return aActivity;
}

void SelectDeathPoseActivityAndFrame( CBaseAnimating *entity, const CTakeDamageInfo &info, int hitgroup, Activity& activity, int& frame )
{
	activity = ACT_INVALID;
	frame = 0;

	if ( !entity->GetModelPtr() )
		return;

	activity = GetDeathPoseActivity( entity, info );
	frame = DEATH_FRAME_HEAD;

	switch( hitgroup )
	{
		//Do normal ragdoll stuff if no specific hitgroup was hit.
		case HITGROUP_GENERIC:
		default:
		{
			return;
		}
			
		case HITGROUP_HEAD:
		{
			frame = DEATH_FRAME_HEAD;
			break;
		}
		case HITGROUP_LEFTARM:
			frame = DEATH_FRAME_LEFTARM;
			break;

		case HITGROUP_RIGHTARM:
			frame = DEATH_FRAME_RIGHTARM;
			break;

		case HITGROUP_CHEST:
		case HITGROUP_STOMACH:
			frame = DEATH_FRAME_STOMACH;
			break;
	
		case HITGROUP_LEFTLEG:
			frame = DEATH_FRAME_LEFTLEG;
			break;

		case HITGROUP_RIGHTLEG:
			frame = DEATH_FRAME_RIGHTLEG;
			break;
	}
}

#endif // !CLIENT_DLL
