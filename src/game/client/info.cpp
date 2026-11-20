// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

module;
#include <math.h>
#include <wchar.h>
#include "client_class.h"
#include "declare_class.h"
#include "reflect_annotations.h"
#include "reflect_recvtable.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialproxy.h"
#include "imaterialproxydict.h"
#include "studio.h"
#include "igameevents.h"
#include "cdll_client_int.h"
#include "materialsystem/imaterialvar.h"
#include "edict.h"
#include "materialsystem/itexture.h"
#include "localize/ilocalize.h"
#include "usermessages_schema.h"
#include "hud_element_helper.h"
#include "hud_macros.h"
#include "datacache/imdlcache.h"
module ks.client;

import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :baseentity;
import :util;
import :particlemgr;
import :precache;
import :noopanimcontroller;
import :achievements_and_stats_interface;
import :sharedvar;
import ks.game.mapentities_shared;
import ks.game.shareddefs;
import ks.tier1.keyvalues;
import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.numeric;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.transform;
import ks.math.vector;
import ks.math.vector2d;
import ks.math.vmatrix;
import ks.tier1.convar;
import ks.tier1.exprevaluator;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.registry;
import ks.tier1.strtools;
import ks.game.gamemovement;
import ks.core.random;
import ks.game.sun_shared;
import ks.core.vprof;
import ks.game.entity_cast;
import ks.tier1.interfaces;
import ks.math.ssequaternion;
import ks.tier1.map_ops;

class CBaseEntity;

using namespace ::ks::reflect;

// -------------------------------------------------------------------------------- //
// An entity used to access overlays (and change their texture)
// -------------------------------------------------------------------------------- //
class CInfoOverlayAccessor : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_InfoOverlayAccessor", .base = false } ]]
	[[= ks::reflect::From<"m_iTextureFrameIndex", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

public:

	DECLARE_CLASS( CInfoOverlayAccessor, CBaseEntity );
	DECLARE_CLIENTCLASS();

	CInfoOverlayAccessor();

	virtual void OnDataChanged( DataUpdateType_t updateType );

private:

	[[= ks::reflect::Net{} ]] int		m_iOverlayID;
};

// Expose it to the engine.
IMPLEMENT_CLIENTCLASS(CInfoOverlayAccessor, DT_InfoOverlayAccessor, CInfoOverlayAccessor);

IMPLEMENT_REFLECT_TABLE( CInfoOverlayAccessor, DT_InfoOverlayAccessor );

// -------------------------------------------------------------------------------- //
// Functions.
// -------------------------------------------------------------------------------- //

CInfoOverlayAccessor::CInfoOverlayAccessor()
{
}

void CInfoOverlayAccessor::OnDataChanged( DataUpdateType_t updateType )
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		// Update overlay's bind proxy
		engine->SetOverlayBindProxy( m_iOverlayID, GetClientRenderable() );
	}
}



class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Returns the player health (from 0 to 1)
//-----------------------------------------------------------------------------
class CProxyIsNPC : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	CFloatInput	m_Factor;
};

bool CProxyIsNPC::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_Factor.Init( pMaterial, pKeyValues, "scale", 1 ))
		return false;

	return true;
}

void CProxyIsNPC::OnBind( void *pC_BaseEntity )
{
	if ( !pC_BaseEntity )
		return;

	CBaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	if ( pEntity && pEntity->IsNPC() )
	{
		SetFloatResult( m_Factor.GetFloat() );
	}
	else
	{
		SetFloatResult( 0.0f );
	}
}

EXPOSE_MATERIAL_PROXY( CProxyIsNPC, IsNPC );

class CJiggleBones;

//-----------------------------------------------------------------------------
ConVar JiggleBoneDebug( side_registry(), "cl_jiggle_bone_debug", "0", FCVAR_CHEAT, "Display physics-based 'jiggle bone' debugging information" );
ConVar JiggleBoneDebugYawConstraints( side_registry(), "cl_jiggle_bone_debug_yaw_constraints", "0", FCVAR_CHEAT, "Display physics-based 'jiggle bone' debugging information" );
ConVar JiggleBoneDebugPitchConstraints( side_registry(), "cl_jiggle_bone_debug_pitch_constraints", "0", FCVAR_CHEAT, "Display physics-based 'jiggle bone' debugging information" );
ConVar JiggleBoneInvert( side_registry(), "cl_jiggle_bone_invert", "0", FCVAR_CHEAT );
ConVar JiggleBoneSanity( side_registry(), "cl_jiggle_bone_sanity", "1", 0, "Prevent jiggle bones from pointing directly away from their target in case of numerical instability." );

static int s_id = 2;

char *VarArgs( const char *format, ... );

//-----------------------------------------------------------------------------
JiggleData *CJiggleBones::GetJiggleData( int bone, float currenttime, const Vector &initBasePos, const Vector &initTipPos )
{
	for ( JiggleData &it : m_jiggleBoneState )
	{
		if ( it.bone == bone )
		{
			JiggleData *data = &it;
			if ( !IsFinite( data->lastUpdate ) )
			{
				Warning( "lastUpdate NaN\n" );
			}
			if ( !data->basePos.IsValid() )
			{
				Warning( "basePos NaN\n" );
			}
			if ( !data->baseLastPos.IsValid() )
			{
				Warning( "baseLastPos NaN\n" );
			}
			if ( !data->baseVel.IsValid() )
			{
				Warning( "baseVel NaN\n" );
			}
			if ( !data->baseAccel.IsValid() )
			{
				Warning( "baseAccel NaN\n" );
			}
			if ( !data->tipPos.IsValid() )
			{
				Warning( "tipPos NaN\n" );
			}
			if ( !data->tipVel.IsValid() )
			{
				Warning( "tipVel NaN\n" );
			}
			if ( !data->tipAccel.IsValid() )
			{
				Warning( "tipAccel NaN\n" );
			}
			return data;
		}
	}

	JiggleData data;
	data.Init( bone, currenttime, initBasePos, initTipPos );
	data.id = s_id++;

	return &m_jiggleBoneState.emplace_back( data );
}

//-----------------------------------------------------------------------------
/**
 * Do spring physics calculations and update "jiggle bone" matrix
 * (Michael Booth, Turtle Rock Studios)
 */
void CJiggleBones::BuildJiggleTransformations( int boneIndex, float currenttime, const mstudiojigglebone_t *jiggleInfo, const matrix3x4_t &goalMX, matrix3x4_t &boneMX, bool coordSystemIsFlipped )
{
	Vector goalBasePosition;
	MatrixPosition( goalMX, goalBasePosition );

	Vector goalForward, goalUp, goalLeft;
	MatrixGetColumn( goalMX, 0, goalLeft );
	MatrixGetColumn( goalMX, 1, goalUp );
	MatrixGetColumn( goalMX, 2, goalForward );

	// compute goal tip position
	Vector goalTip = goalBasePosition + jiggleInfo->length * goalForward;

	JiggleData *data = GetJiggleData( boneIndex, currenttime, goalBasePosition, goalTip );
	if ( !data )
	{
		return;
	}

	if ( currenttime - data->lastUpdate > 0.5f )
	{
		data->Init( boneIndex, currenttime, goalBasePosition, goalTip );
	}

	if ( JiggleBoneInvert.GetBool() )
	{
		data->basePos = -data->basePos;
		data->baseLastPos = -data->baseLastPos;
		data->baseVel = -data->baseVel;
		data->baseAccel = -data->baseAccel;
		data->tipPos = -data->tipPos;
		data->tipVel = -data->tipVel;
		data->tipAccel = -data->tipAccel;
	}

	if ( data->id == JiggleBoneDebug.GetInt() )
	{
		int line = 20;
		engine->Con_NPrintf( line++, "basePos %f %f %f", data->basePos.x, data->basePos.y, data->basePos.z );
		engine->Con_NPrintf( line++, "baseLastPos %f %f %f", data->baseLastPos.x, data->baseLastPos.y, data->baseLastPos.z );
		engine->Con_NPrintf( line++, "baseVel %f %f %f", data->baseVel.x, data->baseVel.y, data->baseVel.z );
		engine->Con_NPrintf( line++, "baseAccel %f %f %f", data->baseAccel.x, data->baseAccel.y, data->baseAccel.z );
		engine->Con_NPrintf( line++, "tipPos %f %f %f", data->tipPos.x, data->tipPos.y, data->tipPos.z );
		engine->Con_NPrintf( line++, "tipVel %f %f %f", data->tipVel.x, data->tipVel.y, data->tipVel.z );
		engine->Con_NPrintf( line++, "tipAccel %f %f %f", data->tipAccel.x, data->tipAccel.y, data->tipAccel.z );
	}

	if ( JiggleBoneSanity.GetBool() )
	{
		Vector goalDir = goalTip - goalBasePosition;
		goalDir.NormalizeInPlace();
		Vector dataDir = data->tipPos - goalBasePosition;
		dataDir.NormalizeInPlace();

		float dot = goalDir.Dot( dataDir );
		if ( dot < -0.9f ) // if we end up pointing almost completely away, just reset
		{
			data->Init( boneIndex, currenttime, goalBasePosition, goalTip );
		}
	}

	//Vector bodyVel;
	//EstimateAbsVelocity( bodyVel );

	// limit maximum deltaT to avoid simulation blowups
	// if framerate gets very low, jiggle will run in slow motion
	const float thirtyHZ = 0.0333f;
	const float thousandHZ = 0.001f;
	float deltaT = clamp( currenttime - data->lastUpdate, thousandHZ, thirtyHZ );
	data->lastUpdate = currenttime;

	//
	// Bone tip flex
	//
	if (jiggleInfo->flags & (JIGGLE_IS_FLEXIBLE | JIGGLE_IS_RIGID))
	{
		// apply gravity in global space
		data->tipAccel.z -= jiggleInfo->tipMass;

		if (jiggleInfo->flags & JIGGLE_IS_FLEXIBLE)
		{
			// We want to run the simulation for this part of the jiggle with smaller time steps.
			static const float MIN_DELTA_T_PER_ITERATION = 1.0f / 120.0f;
			for (float deltaTLeft = deltaT; deltaTLeft > 0.0f; deltaTLeft -= MIN_DELTA_T_PER_ITERATION )
			{
				float usableDeltaT = deltaTLeft > MIN_DELTA_T_PER_ITERATION ? MIN_DELTA_T_PER_ITERATION : deltaTLeft;
				// decompose into local coordinates
				Vector error = goalTip - data->tipPos;

				Vector localError;
				localError.x = DotProduct( goalLeft, error );
				localError.y = DotProduct( goalUp, error );
				localError.z = DotProduct( goalForward, error );

				Vector localVel;
				localVel.x = DotProduct( goalLeft, data->tipVel );
				localVel.y = DotProduct( goalUp, data->tipVel );
				localVel.z = 0.0f; // TODO: this was uninitialized, but is being used

				// yaw spring
				float yawAccel = jiggleInfo->yawStiffness * localError.x - jiggleInfo->yawDamping * localVel.x;

				// pitch spring
				float pitchAccel = jiggleInfo->pitchStiffness * localError.y - jiggleInfo->pitchDamping * localVel.y;

				if (jiggleInfo->flags & JIGGLE_HAS_LENGTH_CONSTRAINT)
				{
					// drive tip towards goal tip position	
					data->tipAccel += yawAccel * goalLeft + pitchAccel * goalUp;
				}
				else
				{
					// allow flex along length of spring
					localVel.z = DotProduct( goalForward, data->tipVel );

					// along spring
					float alongAccel = jiggleInfo->alongStiffness * localError.z - jiggleInfo->alongDamping * localVel.z;

					// drive tip towards goal tip position	
					data->tipAccel += yawAccel * goalLeft + pitchAccel * goalUp + alongAccel * goalForward;
				}

				// simple euler integration		
				data->tipVel += data->tipAccel * usableDeltaT;
				data->tipPos += data->tipVel * usableDeltaT;
			}

		}

		// clear this timestep's accumulated accelerations
		data->tipAccel = vec3_origin;		

		//
		// Apply optional constraints
		//
		if (jiggleInfo->flags & (JIGGLE_HAS_YAW_CONSTRAINT | JIGGLE_HAS_PITCH_CONSTRAINT))
		{
			// find components of spring vector in local coordinate system
			Vector along = data->tipPos - goalBasePosition;
			Vector localAlong;
			localAlong.x = DotProduct( goalLeft, along );
			localAlong.y = DotProduct( goalUp, along );
			localAlong.z = DotProduct( goalForward, along );

			Vector localVel;
			localVel.x = DotProduct( goalLeft, data->tipVel );
			localVel.y = DotProduct( goalUp, data->tipVel );
			localVel.z = DotProduct( goalForward, data->tipVel );

			if (jiggleInfo->flags & JIGGLE_HAS_YAW_CONSTRAINT)
			{
				// enforce yaw constraints in local XZ plane
				float yawError = std::atan2( localAlong.x, localAlong.z );

				bool isAtLimit = false;
				float yaw = 0.0f;

				if (yawError < jiggleInfo->minYaw)
				{
					// at angular limit
					isAtLimit = true;
					yaw = jiggleInfo->minYaw;
				}
				else if (yawError > jiggleInfo->maxYaw)
				{
					// at angular limit
					isAtLimit = true;
					yaw = jiggleInfo->maxYaw;
				}

				if (isAtLimit)
				{
					float sy, cy;
					SinCos( yaw, &sy, &cy );

					// yaw matrix
					matrix3x4_t yawMatrix;

					yawMatrix[0][0] = cy;
					yawMatrix[1][0] = 0;
					yawMatrix[2][0] = -sy;

					yawMatrix[0][1] = 0;
					yawMatrix[1][1] = 1.0f;
					yawMatrix[2][1] = 0;

					yawMatrix[0][2] = sy;
					yawMatrix[1][2] = 0;
					yawMatrix[2][2] = cy;

					yawMatrix[0][3] = 0;
					yawMatrix[1][3] = 0;
					yawMatrix[2][3] = 0;

					// global coordinates of limit
					matrix3x4_t limitMatrix;					
					ConcatTransforms( goalMX, yawMatrix, limitMatrix );

					Vector limitLeft( limitMatrix.m_flMatVal[0][0], 
						limitMatrix.m_flMatVal[1][0],
						limitMatrix.m_flMatVal[2][0] );

					Vector limitUp( limitMatrix.m_flMatVal[0][1], 
						limitMatrix.m_flMatVal[1][1],
						limitMatrix.m_flMatVal[2][1] );

					Vector limitForward( limitMatrix.m_flMatVal[0][2], 
						limitMatrix.m_flMatVal[1][2],
						limitMatrix.m_flMatVal[2][2] );

					if (JiggleBoneDebugYawConstraints.GetBool())
					{
						float dT = 0.01f;
						const float axisSize = 10.0f;
						debugoverlay->AddLineOverlay( goalBasePosition, goalBasePosition + axisSize * limitLeft, 0, 255, 255, true, dT );
						debugoverlay->AddLineOverlay( goalBasePosition, goalBasePosition + axisSize * limitUp, 255, 255, 0, true, dT );
						debugoverlay->AddLineOverlay( goalBasePosition, goalBasePosition + axisSize * limitForward, 255, 0, 255, true, dT );
					}

					Vector limitAlong( DotProduct( limitLeft, along ), 
						DotProduct( limitUp, along ),
						DotProduct( limitForward, along ) );

					// clip to limit plane
					data->tipPos = goalBasePosition + limitAlong.y * limitUp + limitAlong.z * limitForward;

					// yaw friction - rubbing along limit plane
					Vector limitVel;
					limitVel.x = 0.0f; // TODO: this was uninitialized, and is used when yawBounce is non-zero!
					limitVel.y = DotProduct( limitUp, data->tipVel );
					limitVel.z = DotProduct( limitForward, data->tipVel );

					data->tipAccel -= jiggleInfo->yawFriction * (limitVel.y * limitUp + limitVel.z * limitForward);

					// update velocity reaction to hitting constraint
					data->tipVel = -jiggleInfo->yawBounce * limitVel.x * limitLeft + limitVel.y * limitUp + limitVel.z * limitForward;

					// update along vectors for use by pitch constraint
					along = data->tipPos - goalBasePosition;
					localAlong.x = DotProduct( goalLeft, along );
					localAlong.y = DotProduct( goalUp, along );
					localAlong.z = DotProduct( goalForward, along );

					localVel.x = DotProduct( goalLeft, data->tipVel );
					localVel.y = DotProduct( goalUp, data->tipVel );
					localVel.z = DotProduct( goalForward, data->tipVel );
				}
			}

			if (jiggleInfo->flags & JIGGLE_HAS_PITCH_CONSTRAINT)
			{
				// enforce pitch constraints in local YZ plane
				float pitchError = std::atan2( localAlong.y, localAlong.z );

				bool isAtLimit = false;
				float pitch = 0.0f;

				if (pitchError < jiggleInfo->minPitch)
				{
					// at angular limit
					isAtLimit = true;
					pitch = jiggleInfo->minPitch;
				}
				else if (pitchError > jiggleInfo->maxPitch)
				{
					// at angular limit
					isAtLimit = true;
					pitch = jiggleInfo->maxPitch;
				}

				if (isAtLimit)
				{
					float sp, cp;
					SinCos( pitch, &sp, &cp );

					// pitch matrix
					matrix3x4_t pitchMatrix;

					pitchMatrix[0][0] = 1.0f;
					pitchMatrix[1][0] = 0;
					pitchMatrix[2][0] = 0;

					pitchMatrix[0][1] = 0;
					pitchMatrix[1][1] = cp;
					pitchMatrix[2][1] = -sp;

					pitchMatrix[0][2] = 0;
					pitchMatrix[1][2] = sp;
					pitchMatrix[2][2] = cp;

					pitchMatrix[0][3] = 0;
					pitchMatrix[1][3] = 0;
					pitchMatrix[2][3] = 0;

					// global coordinates of limit
					matrix3x4_t limitMatrix;					
					ConcatTransforms( goalMX, pitchMatrix, limitMatrix );

					Vector limitLeft( limitMatrix.m_flMatVal[0][0], 
						limitMatrix.m_flMatVal[1][0],
						limitMatrix.m_flMatVal[2][0] );

					Vector limitUp( limitMatrix.m_flMatVal[0][1], 
						limitMatrix.m_flMatVal[1][1],
						limitMatrix.m_flMatVal[2][1] );

					Vector limitForward( limitMatrix.m_flMatVal[0][2], 
						limitMatrix.m_flMatVal[1][2],
						limitMatrix.m_flMatVal[2][2] );

					if (JiggleBoneDebugPitchConstraints.GetBool())
					{
						float dT = 0.01f;
						const float axisSize = 10.0f;
						debugoverlay->AddLineOverlay( goalBasePosition, goalBasePosition + axisSize * limitLeft, 0, 255, 255, true, dT );
						debugoverlay->AddLineOverlay( goalBasePosition, goalBasePosition + axisSize * limitUp, 255, 255, 0, true, dT );
						debugoverlay->AddLineOverlay( goalBasePosition, goalBasePosition + axisSize * limitForward, 255, 0, 255, true, dT );
					}

					Vector limitAlong( DotProduct( limitLeft, along ), 
						DotProduct( limitUp, along ),
						DotProduct( limitForward, along ) );

					// clip to limit plane
					data->tipPos = goalBasePosition + limitAlong.x * limitLeft + limitAlong.z * limitForward;

					// pitch friction - rubbing along limit plane
					Vector limitVel;
					limitVel.x = 0.0f; // TODO: this was uninitialized, but is being used
					limitVel.y = DotProduct( limitUp, data->tipVel );
					limitVel.z = DotProduct( limitForward, data->tipVel );

					data->tipAccel -= jiggleInfo->pitchFriction * (limitVel.x * limitLeft + limitVel.z * limitForward);

					// update velocity reaction to hitting constraint
					data->tipVel = limitVel.x * limitLeft - jiggleInfo->pitchBounce * limitVel.y * limitUp + limitVel.z * limitForward;
				}
			}
		}

		// needed for matrix assembly below
		Vector forward = data->tipPos - goalBasePosition;
		forward.NormalizeInPlace();

		if (jiggleInfo->flags & JIGGLE_HAS_ANGLE_CONSTRAINT)
		{
			// enforce max angular error
			Vector error = goalTip - data->tipPos;
			float dot = DotProduct( forward, goalForward );
			float angleBetween = std::acos( dot );
			if (dot < 0.0f)
			{
				angleBetween = 2.0f * std::numbers::pi - angleBetween;
			}

			if (angleBetween > jiggleInfo->angleLimit)
			{
				// at angular limit
				float maxBetween = jiggleInfo->length * std::sin( jiggleInfo->angleLimit );

				Vector delta = goalTip - data->tipPos;
				delta.NormalizeInPlace();

				data->tipPos = goalTip - maxBetween * delta;

				forward = data->tipPos - goalBasePosition;
				forward.NormalizeInPlace();
			}
		}

		if (jiggleInfo->flags & JIGGLE_HAS_LENGTH_CONSTRAINT)
		{
			// enforce spring length
			data->tipPos = goalBasePosition + jiggleInfo->length * forward;

			// zero velocity along forward bone axis
			data->tipVel -= DotProduct( data->tipVel, forward ) * forward;
		}

		//
		// Build bone matrix to align along current tip direction
		//
		Vector left, up;
		if ( coordSystemIsFlipped )
		{
			// If the coordinate system is flipped, use left handed rules.
			left = CrossProduct( forward, goalUp );
			left.NormalizeInPlace();
			up = CrossProduct( left, forward );
		}
		else
		{
			left = CrossProduct( goalUp, forward );
			left.NormalizeInPlace();
			up = CrossProduct( forward, left );
		}

		boneMX[0][0] = left.x;
		boneMX[1][0] = left.y;
		boneMX[2][0] = left.z;
		boneMX[0][1] = up.x;
		boneMX[1][1] = up.y;
		boneMX[2][1] = up.z;
		boneMX[0][2] = forward.x;
		boneMX[1][2] = forward.y;
		boneMX[2][2] = forward.z;

		boneMX[0][3] = goalBasePosition.x;
		boneMX[1][3] = goalBasePosition.y;
		boneMX[2][3] = goalBasePosition.z;
	}

	//
	// Bone base flex
	//
	if (jiggleInfo->flags & JIGGLE_HAS_BASE_SPRING)
	{
		// gravity
		data->baseAccel.z -= jiggleInfo->baseMass;

		// simple spring
		Vector error = goalBasePosition - data->basePos;
		data->baseAccel += jiggleInfo->baseStiffness * error - jiggleInfo->baseDamping * data->baseVel;

		data->baseVel += data->baseAccel * deltaT;
		data->basePos += data->baseVel * deltaT;

		// clear this timestep's accumulated accelerations
		data->baseAccel = vec3_origin;		

		// constrain to limits
		error = data->basePos - goalBasePosition;
		Vector localError;
		localError.x = DotProduct( goalLeft, error );
		localError.y = DotProduct( goalUp, error );
		localError.z = DotProduct( goalForward, error );

		Vector localVel;
		localVel.x = DotProduct( goalLeft, data->baseVel );
		localVel.y = DotProduct( goalUp, data->baseVel );
		localVel.z = DotProduct( goalForward, data->baseVel );

		// horizontal constraint
		if (localError.x < jiggleInfo->baseMinLeft)
		{
			localError.x = jiggleInfo->baseMinLeft;

			// friction
			data->baseAccel -= jiggleInfo->baseLeftFriction * (localVel.y * goalUp + localVel.z * goalForward);
		}
		else if (localError.x > jiggleInfo->baseMaxLeft)
		{
			localError.x = jiggleInfo->baseMaxLeft;

			// friction
			data->baseAccel -= jiggleInfo->baseLeftFriction * (localVel.y * goalUp + localVel.z * goalForward);
		}

		if (localError.y < jiggleInfo->baseMinUp)
		{
			localError.y = jiggleInfo->baseMinUp;

			// friction
			data->baseAccel -= jiggleInfo->baseUpFriction * (localVel.x * goalLeft + localVel.z * goalForward);
		}
		else if (localError.y > jiggleInfo->baseMaxUp)
		{
			localError.y = jiggleInfo->baseMaxUp;

			// friction
			data->baseAccel -= jiggleInfo->baseUpFriction * (localVel.x * goalLeft + localVel.z * goalForward);
		}

		if (localError.z < jiggleInfo->baseMinForward)
		{
			localError.z = jiggleInfo->baseMinForward;

			// friction
			data->baseAccel -= jiggleInfo->baseForwardFriction * (localVel.x * goalLeft + localVel.y * goalUp);
		}
		else if (localError.z > jiggleInfo->baseMaxForward)
		{
			localError.z = jiggleInfo->baseMaxForward;

			// friction
			data->baseAccel -= jiggleInfo->baseForwardFriction * (localVel.x * goalLeft + localVel.y * goalUp);
		}

		data->basePos = goalBasePosition + localError.x * goalLeft + localError.y * goalUp + localError.z * goalForward;

		// fix up velocity
		data->baseVel = (data->basePos - data->baseLastPos) / deltaT;
		data->baseLastPos = data->basePos;

		if (!(jiggleInfo->flags & (JIGGLE_IS_FLEXIBLE | JIGGLE_IS_RIGID)))
		{
			// no tip flex - use bone's goal orientation
			boneMX = goalMX;							
		}

		// update bone position
		MatrixSetColumn( data->basePos, 3, boneMX );
	}
	else if (!(jiggleInfo->flags & (JIGGLE_IS_FLEXIBLE | JIGGLE_IS_RIGID)))
	{
		// no flex at all - just use goal matrix
		boneMX = goalMX;
	}

	// debug display
	if ( JiggleBoneDebug.GetInt() == 1 || JiggleBoneDebug.GetInt() == data->id )
	{
		float dT = 0.01f;
		const float axisSize = 5.0f;
		debugoverlay->AddLineOverlay( goalBasePosition, goalBasePosition + axisSize * goalLeft, 255, 0, 0, true, dT );
		debugoverlay->AddLineOverlay( goalBasePosition, goalBasePosition + axisSize * goalUp, 0, 255, 0, true, dT );
		debugoverlay->AddLineOverlay( goalBasePosition, goalBasePosition + axisSize * goalForward, 0, 0, 255, true, dT );

		debugoverlay->AddTextOverlay( goalBasePosition, dT, "%d", data->id );

		const float sz = 1.0f;

		if (jiggleInfo->flags & (JIGGLE_IS_FLEXIBLE | JIGGLE_IS_RIGID))
		{
			debugoverlay->AddLineOverlay( goalBasePosition, 
				data->tipPos, 255, 255, 0, true, dT );

			debugoverlay->AddLineOverlay( data->tipPos + Vector( -sz, 0, 0 ), 
				data->tipPos + Vector( sz, 0, 0 ), 0, 255, 255, true, dT );
			debugoverlay->AddLineOverlay( data->tipPos + Vector( 0, -sz, 0 ), 
				data->tipPos + Vector( 0, sz, 0 ), 0, 255, 255, true, dT );
			debugoverlay->AddLineOverlay( data->tipPos + Vector( 0, 0, -sz ), 
				data->tipPos + Vector( 0, 0, sz ), 0, 255, 255, true, dT );
		}

		if (jiggleInfo->flags & JIGGLE_HAS_BASE_SPRING)
		{
			debugoverlay->AddLineOverlay( data->basePos + Vector( -sz, 0, 0 ), 
				data->basePos + Vector( sz, 0, 0 ), 255, 0, 255, true, dT );
			debugoverlay->AddLineOverlay( data->basePos + Vector( 0, -sz, 0 ), 
				data->basePos + Vector( 0, sz, 0 ), 255, 0, 255, true, dT );
			debugoverlay->AddLineOverlay( data->basePos + Vector( 0, 0, -sz ), 
				data->basePos + Vector( 0, 0, sz ), 255, 0, 255, true, dT );
		}
	}
}

using namespace ::ks::reflect;

// Purpose: Client handler implementations for instruction players how to play

// Key Value Saver auto game system instantiation
CKeyValueSaver g_KeyValueSaver[ MAX_SPLITSCREEN_PLAYERS ];
CKeyValueSaver &KeyValueSaver()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return g_KeyValueSaver[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}

// CKeyValueSaver
//

bool CKeyValueSaver::Init( void )
{
	// Make sure split slot is up to date
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		if ( &KeyValueSaver() == this )
		{
			SetSlot( i );
			break;
		}
	}

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	ListenForGameEvent( "round_end" );
	ListenForGameEvent( "map_transition" );
	ListenForGameEvent( "game_newmap" );

	return true;
}

void CKeyValueSaver::Shutdown( void )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	WriteAllDirtyKeyValues();

	for ( int i = 0; i < std::ssize( m_KeyValueData ); ++i )
	{
		if ( m_KeyValueData[ i ].pKeyValues )
		{
			m_KeyValueData[ i ].pKeyValues->deleteThis();
			m_KeyValueData[ i ].pKeyValues = nullptr;
		}
	}

	m_KeyValueData.clear();

	// Stop listening for events
	StopListeningForAllEvents();
}

void CKeyValueSaver::Update( float frametime )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

}

void CKeyValueSaver::FireGameEvent( IGameEvent *event )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	const char *name = event->GetName();

	if ( V_strcmp( name, "round_end" ) == 0 || 
		 V_strcmp( name, "map_transition" ) == 0 || 
		 V_strcmp( name, "game_newmap" ) == 0 )
	{
		// Good place to save
		WriteAllDirtyKeyValues();
	}
}

bool CKeyValueSaver::InitKeyValues( const char *pchFileName, KeyValueBuilder funcKeyValueBuilder )
{
	KeyValueSaverData *pKeyValueData = FindKeyValueData( pchFileName );
	if ( pKeyValueData )
	{
		// Already got one by this name
		return false;
	}

	int nNew = std::ssize( m_KeyValueData ); m_KeyValueData.emplace_back();
	pKeyValueData = &(m_KeyValueData[ nNew ]);
	V_strcpy( pKeyValueData->szFileName, pchFileName );
	pKeyValueData->bDirtySaveData = false;
	pKeyValueData->pKeyValues = nullptr;
	pKeyValueData->funcKeyValueBuilder = funcKeyValueBuilder;

	return true;
}

bool CKeyValueSaver::WriteDirtyKeyValues( const char *pchFileName, bool bForceWrite /*= false*/ )
{
	return WriteDirtyKeyValues( FindKeyValueData( pchFileName ), bForceWrite );
}

KeyValues * CKeyValueSaver::GetKeyValues( const char *pchFileName, bool bForceReread /*= false*/ )
{
	KeyValueSaverData *pKeyValueData = FindKeyValueData( pchFileName );

	if ( pKeyValueData )
	{
		if ( !pKeyValueData->pKeyValues  )
		{
			bForceReread = true;
		}

		if ( bForceReread )
		{
			if ( !ReadKeyValues( pKeyValueData ) )
			{
				return nullptr;
			}
		}

		return pKeyValueData->pKeyValues;
	}

	return nullptr;
}

void CKeyValueSaver::MarkKeyValuesDirty( const char *pchFileName )
{
	KeyValueSaverData *pKeyValueData = FindKeyValueData( pchFileName );
	if ( !pKeyValueData )
		return;

	pKeyValueData->bDirtySaveData = true;
}

bool CKeyValueSaver::ReadKeyValues( KeyValueSaverData *pKeyValueData )
{

	// Couldn't read from the file
	return false;
}

bool CKeyValueSaver::WriteDirtyKeyValues( KeyValueSaverData *pKeyValueData, bool bForceWrite /*= false*/ )
{
	if ( engine->IsPlayingDemo() )
		return false;

	if ( !pKeyValueData )
		return false;

	if ( !pKeyValueData->bDirtySaveData && !bForceWrite )
		return true;

	// Always mark as clean state to avoid re-entry on
	// subsequent frames when storage device might be
	// in a yet-unmounted state.
	pKeyValueData->bDirtySaveData = false;

	// Build key value data to save
	if ( pKeyValueData->pKeyValues )
	{
		pKeyValueData->pKeyValues->deleteThis();
		pKeyValueData->pKeyValues = nullptr;
	}

	pKeyValueData->pKeyValues = new KeyValues( "KeyValueSaverData" );

	// Build key values
	pKeyValueData->funcKeyValueBuilder( pKeyValueData->pKeyValues );

	// The key values are saved to the title data block in the callback above.
	return true;
}

void CKeyValueSaver::WriteAllDirtyKeyValues( void )
{
	for ( int i = 0; i < std::ssize( m_KeyValueData ); ++i )
	{
		WriteDirtyKeyValues( &(m_KeyValueData[ i ]) );
	}
}

KeyValueSaverData * CKeyValueSaver::FindKeyValueData( const char *pchFileName )
{
	for ( int i = 0; i < std::ssize( m_KeyValueData ); ++i )
	{
		KeyValueSaverData *pKeyValueData = &(m_KeyValueData[ i ]);
		if ( V_strcmp( pKeyValueData->szFileName, pchFileName ) == 0 )
		{
			return pKeyValueData;
		}
	}

	return nullptr;
}

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Used for 'card' beams on lamps, this material fades the sprite out
// as the viewer nears, so that the viewer can't see that the effect is really
// a card.
//-----------------------------------------------------------------------------
class CLampBeamProxy : public CEntityMaterialProxy
{
public:
						CLampBeamProxy( void );
	virtual				~CLampBeamProxy( void );
	virtual bool		Init( IMaterial *pMaterial, KeyValues* pKeyValues );
	virtual void		OnBind( CBaseEntity *pC_BaseEntity );

	virtual IMaterial *	GetMaterial();

private:
	IMaterialVar		*m_pFadeValue;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CLampBeamProxy::CLampBeamProxy( void )
{
	m_pFadeValue = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CLampBeamProxy::~CLampBeamProxy( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Get pointer to the color value
// Input  : *pMaterial - 
//-----------------------------------------------------------------------------
bool CLampBeamProxy::Init( IMaterial *pMaterial, KeyValues* pKeyValues )
{
	contract_assert( pMaterial );

	// Need to get the color variable.
	bool found;
	m_pFadeValue = pMaterial->FindVar( "$alpha", &found );
	pMaterial->SetMaterialVarFlag( MATERIAL_VAR_ALPHA_MODIFIED_BY_PROXY, true );
	return found;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pC_BaseEntity - 
//-----------------------------------------------------------------------------
static constexpr auto FADE_DIST = 150;

void CLampBeamProxy::OnBind( CBaseEntity *pEnt )
{
	if ( !m_pFadeValue )
		return;

	Vector vecLocal = pEnt->GetAbsOrigin() - CurrentViewOrigin();
	VectorNormalize( vecLocal );

	float fade = 1.0 - fabs( vecLocal.z );

	m_pFadeValue->SetFloatValue( fade );
}

IMaterial *CLampBeamProxy::GetMaterial()
{
	if ( !m_pFadeValue )
		return nullptr;

	return m_pFadeValue->GetOwningMaterial();
}

EXPOSE_MATERIAL_PROXY( CLampBeamProxy, lampbeam );

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Used for halos on lamps, this material fades the sprite IN
// as the viewer nears.
//-----------------------------------------------------------------------------
class CLampHaloProxy : public CEntityMaterialProxy
{
public:
						CLampHaloProxy( void );
	virtual				~CLampHaloProxy( void );
	virtual bool		Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void		OnBind( CBaseEntity *pC_BaseEntity );

	virtual IMaterial *	GetMaterial();

private:
	IMaterialVar		*m_pFadeValue;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CLampHaloProxy::CLampHaloProxy( void )
{
	m_pFadeValue = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CLampHaloProxy::~CLampHaloProxy( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Get pointer to the color value
// Input  : *pMaterial - 
//-----------------------------------------------------------------------------
bool CLampHaloProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	contract_assert( pMaterial );

	// Get pointers to material vars.

	// Need to get the color variable.
	bool found;
	m_pFadeValue = pMaterial->FindVar( "$alpha", &found );
	pMaterial->SetMaterialVarFlag( MATERIAL_VAR_ALPHA_MODIFIED_BY_PROXY, true );
	return found;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pC_BaseEntity - 
//-----------------------------------------------------------------------------

void CLampHaloProxy::OnBind( CBaseEntity *pEnt )
{
	if ( !m_pFadeValue )
		return;
	
	Vector vecLocal = pEnt->GetAbsOrigin() - CurrentViewOrigin();
	VectorNormalize( vecLocal );

	float fade = fabs( vecLocal.z );

	// I hate these magic numbers here, will have to revise
	// (sjb)
	if( fade < 0.25 )
	{
		fade = 0.0;
	}
	else
	{
		fade = std::min<std::common_type_t<double, float>>( (fade - 0.25) * 1.35, 1.0f );
	}

	m_pFadeValue->SetFloatValue( fade );
}

IMaterial *CLampHaloProxy::GetMaterial()
{
	if ( !m_pFadeValue )
		return nullptr;

	return m_pFadeValue->GetOwningMaterial();
}

EXPOSE_MATERIAL_PROXY( CLampHaloProxy, lamphalo );

class CBaseEntity;

using namespace ::ks::reflect;

class CLightGlowOverlay : public CGlowOverlay
{
public:

	virtual void CalcSpriteColorAndSize( float flDot, CGlowSprite *pSprite, float *flHorzSize, float *flVertSize, Vector *vColor )
	{
		*flHorzSize = pSprite->m_flHorzSize;
		*flVertSize = pSprite->m_flVertSize;
		
		Vector viewDir = ( CurrentViewOrigin() - m_vecOrigin );
		float distToViewer = VectorNormalize( viewDir );

		if ( m_bOneSided )
		{
			if ( DotProduct( viewDir, m_vecDirection ) < 0.0f )
			{
				*vColor = Vector(0,0,0);
				return;
			}
		}

		float fade;

		// See if we're in the outer fade distance range
		if ( m_nOuterMaxDist > m_nMaxDist && distToViewer > m_nMaxDist )
		{
			fade = RemapValClamped( distToViewer, m_nMaxDist, m_nOuterMaxDist, 1.0f, 0.0f );
		}
		else
		{
			fade = RemapValClamped( distToViewer, m_nMinDist, m_nMaxDist, 0.0f, 1.0f );
		}
		
		*vColor = pSprite->m_vColor * fade * m_flGlowObstructionScale;
	}

	void SetOrigin( const Vector &origin ) { m_vecOrigin = origin; }
	
	void SetFadeDistances( int minDist, int maxDist, int outerMaxDist )
	{
		m_nMinDist = minDist;
		m_nMaxDist = maxDist;
		m_nOuterMaxDist = outerMaxDist;
	}

	void SetOneSided( bool state = true ) { m_bOneSided = state; }
	void SetModulateByDot( bool state = true ) { m_bModulateByDot = state; }

	void SetDirection( const Vector &dir ) { m_vecDirection = dir; VectorNormalize( m_vecDirection ); }

protected:

	Vector	m_vecOrigin;
	Vector	m_vecDirection;
	int		m_nMinDist;
	int		m_nMaxDist;
	int		m_nOuterMaxDist;
	bool	m_bOneSided;
	bool	m_bModulateByDot;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
// Defined below, but named by the Bare<> annotation on the class, so it needs a declaration
// above it. TU-local, so it stays static -- c_sun.cpp's counterpart had to be given external
// linkage only because CSun is declared in a header.
static void RecvProxy_HDRColorScale( const CRecvProxyData *pData, void *pStruct, void *pOut );

class CLightGlow : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_LightGlow", .base = false } ]]
	[[= ks::reflect::From<"m_clrRender", ks::reflect::Net{}, RecvProxy_Int32ToColor32>{} ]]
	[[= ks::reflect::From<"m_vecNetworkOrigin", ks::reflect::Net{ .wire = "m_vecOrigin" }>{} ]]
	[[= ks::reflect::From<"m_angNetworkAngles", ks::reflect::Net{ .wire = "m_angRotation" }>{} ]]
	[[= ks::reflect::From<"m_hNetworkMoveParent",
	ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int, .wire = "moveparent" },
	RecvProxy_IntToMoveParent>{} ]]
	[[= ks::reflect::Bare<"HDRColorScale",
	ks::reflect::Net{ .enc = ks::reflect::WireEnc::Float }, RecvProxy_HDRColorScale>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CLightGlow, CBaseEntity );
	DECLARE_CLIENTCLASS();

	CLightGlow();

// CBaseEntity overrides.
public:

	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual bool	Simulate( void );
	virtual void	ClientThink( void );

public:
	
	[[= ks::reflect::Net{} ]] int					m_nHorizontalSize;
	[[= ks::reflect::Net{} ]] int					m_nVerticalSize;
	[[= ks::reflect::Net{} ]] int					m_nMinDist;
	[[= ks::reflect::Net{} ]] int					m_nMaxDist;
	[[= ks::reflect::Net{} ]] int					m_nOuterMaxDist;
	[[= ks::reflect::Net{} ]] int					m_spawnflags;
	CLightGlowOverlay	m_Glow;

	[[= ks::reflect::Net{} ]] float				m_flGlowProxySize;
};

static void RecvProxy_HDRColorScale( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CLightGlow *pLightGlow = ( CLightGlow * )pStruct;

	pLightGlow->m_Glow.m_flHDRColorScale = pData->m_Value.m_Float;
}

IMPLEMENT_REFLECT_CLIENTCLASS( CLightGlow, DT_LightGlow, CLightGlow )

//-----------------------------------------------------------------------------
// Constructor 
//-----------------------------------------------------------------------------
CLightGlow::CLightGlow() :
m_nHorizontalSize( 0 ), m_nVerticalSize( 0 ), m_nMinDist( 0 ), m_nMaxDist( 0 )
{
	m_Glow.m_bDirectional = false;
	m_Glow.m_bInSky = false;
	AddToEntityList(ENTITY_LIST_SIMULATE);
}

bool CLightGlow::Simulate( void )
{
	BaseClass::Simulate();

	m_Glow.m_vPos = GetAbsOrigin();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void CLightGlow::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	m_Glow.m_vPos = GetAbsOrigin();

	if ( updateType == DATA_UPDATE_CREATED )
	{
		// Setup our flare.
		color24 c = GetRenderColor();
		Vector vColor( c.r / 255.0f, c.g / 255.0f, c.b / 255.0f );

		m_Glow.m_nSprites = 1;
		m_Glow.m_Sprites[0].m_flVertSize = (float) m_nVerticalSize;
		m_Glow.m_Sprites[0].m_flHorzSize = (float) m_nHorizontalSize;
		m_Glow.m_Sprites[0].m_vColor = vColor;
		
		m_Glow.SetOrigin( GetAbsOrigin() );
		m_Glow.SetFadeDistances( m_nMinDist, m_nMaxDist, m_nOuterMaxDist );
		m_Glow.m_flProxyRadius = m_flGlowProxySize;

		if ( m_spawnflags & SF_LIGHTGLOW_DIRECTIONAL )
		{
			m_Glow.SetOneSided();
		}

		SetNextClientThink( gpGlobals->curtime + RandomFloat(0,3.0) );
	}
	else if ( updateType == DATA_UPDATE_DATATABLE_CHANGED ) //Right now only color should change.
	{
		// Setup our flare.
		color24 c = GetRenderColor();
		Vector vColor( c.r / 255.0f, c.g / 255.0f, c.b / 255.0f );
		m_Glow.m_Sprites[0].m_vColor = vColor;
	}
	
	Vector forward;
	AngleVectors( GetAbsAngles(), &forward, nullptr, nullptr );
	
	m_Glow.SetDirection( forward );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLightGlow::ClientThink( void )
{
	Vector mins = GetAbsOrigin();
	if ( engine->IsBoxVisible( mins, mins ) )
	{
		m_Glow.Activate();
	}
	else
	{
		m_Glow.Deactivate();
	}

	SetNextClientThink( gpGlobals->curtime + RandomFloat(1.0,3.0) );
}

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Material Modify control entity.

static constexpr auto MATERIAL_MODIFY_STRING_SIZE = 255;
static constexpr auto MATERIAL_MODIFY_ANIMATION_UNSET = -1;

// Must match MaterialModifyControl.cpp
enum MaterialModifyMode_t
{
	MATERIAL_MODIFY_MODE_NONE = 0,
	MATERIAL_MODIFY_MODE_SETVAR = 1,
	MATERIAL_MODIFY_MODE_ANIM_SEQUENCE = 2,
	MATERIAL_MODIFY_MODE_FLOAT_LERP = 3,
};

ConVar debug_materialmodifycontrol_client( side_registry(), "debug_materialmodifycontrol_client", "0" );

struct materialanimcommands_t
{
	int iFrameStart;
	int iFrameEnd;
	bool bWrap;
	float flFrameRate;
};

struct materialfloatlerpcommands_t
{
	int flStartValue;
	int flEndValue;
	float flTransitionTime;
};

//------------------------------------------------------------------------------
// FIXME: This really should inherit from something	more lightweight
//------------------------------------------------------------------------------

class CMaterialModifyControl : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_MaterialModifyControl" } ]]
	static constexpr bool ks_class = true;

public:

	DECLARE_CLASS( CMaterialModifyControl, CBaseEntity );

	CMaterialModifyControl();

	void OnPreDataChanged( DataUpdateType_t updateType );
	void OnDataChanged( DataUpdateType_t updateType );
	bool ShouldDraw();

	IMaterial *GetMaterial( void )					{ return m_pMaterial; }
	const char *GetMaterialVariableName( void )		{ return m_szMaterialVar; }
	const char *GetMaterialVariableValue( void )	{ return m_szMaterialVarValue; }

	DECLARE_CLIENTCLASS();

	// Animated texture and Float Lerp usage
	bool	HasNewAnimationCommands( void )			{ return m_bHasNewAnimationCommands; }
	void	ClearAnimationCommands( void )			{ m_bHasNewAnimationCommands = false; }

	// Animated texture usage
	void	GetAnimationCommands( materialanimcommands_t *pCommands );

	// FloatLerp usage
	void	GetFloatLerpCommands( materialfloatlerpcommands_t *pCommands );

	void	SetAnimationStartTime( float flTime )
	{
		m_flAnimationStartTime = flTime;
	}
	float	GetAnimationStartTime( void ) const
	{
		return m_flAnimationStartTime;
	}

	MaterialModifyMode_t GetModifyMode( void ) const
	{
		return ( MaterialModifyMode_t)m_nModifyMode;
	}
private:

	[[= ks::reflect::Net{} ]] char m_szMaterialName[MATERIAL_MODIFY_STRING_SIZE];
	[[= ks::reflect::Net{} ]] char m_szMaterialVar[MATERIAL_MODIFY_STRING_SIZE];
	[[= ks::reflect::Net{} ]] char m_szMaterialVarValue[MATERIAL_MODIFY_STRING_SIZE];
	IMaterial		*m_pMaterial;

	bool	m_bHasNewAnimationCommands;

	// Animation commands from the server
	[[= ks::reflect::Net{} ]] int		m_iFrameStart;
	[[= ks::reflect::Net{} ]] int		m_iFrameEnd;
	[[= ks::reflect::Net{} ]] bool	m_bWrap;
	[[= ks::reflect::Net{} ]] float	m_flFramerate;
	[[= ks::reflect::Net{} ]] bool	m_bNewAnimCommandsSemaphore;
	bool	m_bOldAnimCommandsSemaphore;

	// Float lerp commands from the server
	[[= ks::reflect::Net{} ]] float	m_flFloatLerpStartValue;
	[[= ks::reflect::Net{} ]] float	m_flFloatLerpEndValue;
	[[= ks::reflect::Net{} ]] float	m_flFloatLerpTransitionTime;
	[[= ks::reflect::Net{} ]] bool	m_bFloatLerpWrap;
	float	m_flAnimationStartTime;

	[[= ks::reflect::Net{} ]] int		m_nModifyMode;
};

IMPLEMENT_REFLECT_CLIENTCLASS( CMaterialModifyControl, DT_MaterialModifyControl, CMaterialModifyControl )

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
CMaterialModifyControl::CMaterialModifyControl()
{
	m_pMaterial = nullptr;
	m_bOldAnimCommandsSemaphore = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaterialModifyControl::OnPreDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnPreDataChanged( updateType );

	m_bOldAnimCommandsSemaphore = m_bNewAnimCommandsSemaphore;
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CMaterialModifyControl::OnDataChanged( DataUpdateType_t updateType )
{
	if( updateType == DATA_UPDATE_CREATED )
	{
		m_pMaterial = materials->FindMaterial( m_szMaterialName, TEXTURE_GROUP_OTHER );

		// Clear out our variables
		m_bHasNewAnimationCommands = true;
	}

	// Detect changes in the anim commands
	if ( m_bNewAnimCommandsSemaphore != m_bOldAnimCommandsSemaphore )
	{
		m_bHasNewAnimationCommands = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaterialModifyControl::GetAnimationCommands( materialanimcommands_t *pCommands )
{
	pCommands->iFrameStart = m_iFrameStart;
	pCommands->iFrameEnd = m_iFrameEnd;
	pCommands->bWrap = m_bWrap;
	pCommands->flFrameRate = m_flFramerate;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaterialModifyControl::GetFloatLerpCommands( materialfloatlerpcommands_t *pCommands )
{
	pCommands->flStartValue = m_flFloatLerpStartValue;
	pCommands->flEndValue = m_flFloatLerpEndValue;
	pCommands->flTransitionTime = m_flFloatLerpTransitionTime;
}

//------------------------------------------------------------------------------
// Purpose: We don't draw.
//------------------------------------------------------------------------------
bool CMaterialModifyControl::ShouldDraw()
{
	return false;
}

//=============================================================================
//
// THE MATERIALMODIFYPROXY ITSELF
//
class CMaterialModifyProxy : public CBaseAnimatedTextureProxy
{
public:
	CMaterialModifyProxy();
	virtual ~CMaterialModifyProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pEntity );
	virtual IMaterial *GetMaterial();

private:
	void OnBindSetVar( CMaterialModifyControl *pControl );
	void OnBindAnimatedTexture( CMaterialModifyControl *pControl );
	void OnBindFloatLerp( CMaterialModifyControl *pControl );
	float GetAnimationStartTime( void* pArg );
	void AnimationWrapped( void* pArg );

	IMaterial	*m_pMaterial;
	
	// texture animation stuff
	int			m_iFrameStart;
	int			m_iFrameEnd;
	bool		m_bReachedEnd;
	bool		m_bCustomWrap;
	float		m_flCustomFramerate;

	// float lerp stuff
	IMaterialVar *m_pMaterialVar;
	int			m_flStartValue;
	int			m_flEndValue;
	float		m_flStartTime;
	float		m_flTransitionTime;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CMaterialModifyProxy::CMaterialModifyProxy()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CMaterialModifyProxy::~CMaterialModifyProxy()
{
}

bool CMaterialModifyProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	// set var stuff
	m_pMaterial = pMaterial;

	// float lerp stuff
	m_flStartValue = MATERIAL_MODIFY_ANIMATION_UNSET;
	m_flEndValue = MATERIAL_MODIFY_ANIMATION_UNSET;

	// animated stuff
//	m_pMaterial = pMaterial;
//	m_iFrameStart = MATERIAL_MODIFY_ANIMATION_UNSET;
//	m_iFrameEnd = MATERIAL_MODIFY_ANIMATION_UNSET;
//	m_bReachedEnd = false;
//	return CBaseAnimatedTextureProxy::Init( pMaterial, pKeyValues );

	return true;
}

void CMaterialModifyProxy::OnBind( void *pEntity )
{
	// Get the modified material vars from the entity input
	IClientRenderable *pRend = (IClientRenderable *)pEntity;
	if ( pRend )
	{
		CBaseEntity *pBaseEntity = EntityCast<CBaseEntity>( pRend->GetIClientUnknown()->GetBaseEntity() );
		
		if ( pBaseEntity )
		{
			if( debug_materialmodifycontrol_client.GetBool() )
			{
//				DevMsg( 1, "%s\n", pBaseEntity->GetDebugName() );
			}
			int numChildren = 0;
			bool gotOne = false;
			for ( CBaseEntity *pChild = pBaseEntity->FirstMoveChild(); pChild; pChild = pChild->NextMovePeer() )
			{
				numChildren++;
				CMaterialModifyControl *pControl = dynamic_cast<CMaterialModifyControl*>( pChild );
				if ( !pControl )
					continue;

				if( debug_materialmodifycontrol_client.GetBool() )
				{
//					DevMsg( 1, "pControl: 0x%p\n", pControl );
				}
				
				switch( pControl->GetModifyMode() )
				{
				case MATERIAL_MODIFY_MODE_NONE:
					break;
				case MATERIAL_MODIFY_MODE_SETVAR:
					gotOne = true;
					OnBindSetVar( pControl );
					break;
				case MATERIAL_MODIFY_MODE_ANIM_SEQUENCE:
					OnBindAnimatedTexture( pControl );
					break;
				case MATERIAL_MODIFY_MODE_FLOAT_LERP:
					OnBindFloatLerp( pControl );
					break;
				default:
					contract_assert( 0 );
					break;
				}
			}
			if( gotOne )
			{
//				DevMsg( 1, "numChildren: %d\n", numChildren );
			}
		}
	}
}

IMaterial *CMaterialModifyProxy::GetMaterial()
{
	return m_pMaterial;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMaterialModifyProxy::OnBindSetVar( CMaterialModifyControl *pControl )
{
	IMaterial *pMaterial = pControl->GetMaterial();
	if( !pMaterial )
	{
		contract_assert( 0 );
		return;
	}

	if ( pMaterial != m_pMaterial )
	{
//		Warning( "\t%s!=%s\n", pMaterial->GetName(), m_pMaterial->GetName() );
		return;
	}

	bool bFound;
	IMaterialVar *pMaterialVar = pMaterial->FindVar( pControl->GetMaterialVariableName(), &bFound, false );
	if ( !bFound )
		return;

	if( V_strcmp( pControl->GetMaterialVariableValue(), "" ) )
	{
//		const char *pMaterialName = m_pMaterial->GetName();
//		const char *pMaterialVarName = pMaterialVar->GetName();
//		const char *pMaterialVarValue = pControl->GetMaterialVariableValue();
//		if( debug_materialmodifycontrol_client.GetBool() 
//			&& V_stristr( m_pMaterial->GetName(), "faceandhair" )
//			&& V_stristr( pMaterialVar->GetName(), "self" )
//			)
//		{
//			static int count = 0;
//			DevMsg( 1, "CMaterialModifyProxy::OnBindSetVar \"%s\" %s=%s %d pControl=0x%p\n", 
//				m_pMaterial->GetName(), pMaterialVar->GetName(), pControl->GetMaterialVariableValue(), count++, pControl );
//		}
		pMaterialVar->SetValueAutodetectType( pControl->GetMaterialVariableValue() );
	}
}

//-----------------------------------------------------------------------------
// Does the dirty deed
//-----------------------------------------------------------------------------
void CMaterialModifyProxy::OnBindAnimatedTexture( CMaterialModifyControl *pControl )
{
	contract_assert( m_AnimatedTextureVar );
	if( m_AnimatedTextureVar->GetType() != MATERIAL_VAR_TYPE_TEXTURE )
		return;

	ITexture *pTexture;
	pTexture = m_AnimatedTextureVar->GetTextureValue();

	if ( !pControl )
		return;

	if ( pControl->HasNewAnimationCommands() )
	{
		// Read the data from the modify entity
		materialanimcommands_t sCommands;
		pControl->GetAnimationCommands( &sCommands );

		m_iFrameStart = sCommands.iFrameStart;
		m_iFrameEnd = sCommands.iFrameEnd;
		m_bCustomWrap = sCommands.bWrap;
		m_flCustomFramerate = sCommands.flFrameRate;
		m_bReachedEnd = false;

		m_flStartTime = gpGlobals->curtime;

		pControl->ClearAnimationCommands();
	}

	// Init all the vars based on whether we're using the base material settings, 
	// or the custom ones from the entity input.
	int numFrames;
	bool bWrapAnimation;
	float flFrameRate;
	int iLastFrame;

	// Do we have a custom frame section from the server?
	if ( m_iFrameStart != MATERIAL_MODIFY_ANIMATION_UNSET )
	{
		if ( m_iFrameEnd == MATERIAL_MODIFY_ANIMATION_UNSET )
		{
			m_iFrameEnd = pTexture->GetNumAnimationFrames();
		}

		numFrames = (m_iFrameEnd - m_iFrameStart) + 1;
		bWrapAnimation = m_bCustomWrap;
		flFrameRate = m_flCustomFramerate;
		iLastFrame = (m_iFrameEnd - 1);
	}
	else
	{
		numFrames = pTexture->GetNumAnimationFrames();
		bWrapAnimation = m_WrapAnimation;
		flFrameRate = m_FrameRate;
		iLastFrame = (numFrames - 1);
	}

	// Have we already reached the end? If so, stay there.
	if ( m_bReachedEnd && !bWrapAnimation )
	{
		m_AnimatedTextureFrameNumVar->SetIntValue( iLastFrame );
		return;
	}

	// NOTE: Must not use relative time based methods here
	// because the bind proxy can be called many times per frame.
	// Prevent multiple Wrap callbacks to be sent for no wrap mode
	float startTime;
	if ( m_iFrameStart != MATERIAL_MODIFY_ANIMATION_UNSET )
	{
		startTime = m_flStartTime;
	}
	else
	{
		startTime = GetAnimationStartTime(pControl);
	}
	float deltaTime = gpGlobals->curtime - startTime;
	float prevTime = deltaTime - gpGlobals->frametime;

	// Clamp..
	if (deltaTime < 0.0f)
		deltaTime = 0.0f;
	if (prevTime < 0.0f)
		prevTime = 0.0f;

	float frame = flFrameRate * deltaTime;	
	float prevFrame = flFrameRate * prevTime;

	int intFrame = ((int)frame) % numFrames; 
	int intPrevFrame = ((int)prevFrame) % numFrames;

	if ( m_iFrameStart != MATERIAL_MODIFY_ANIMATION_UNSET )
	{
		intFrame += m_iFrameStart;
		intPrevFrame += m_iFrameStart;
	}

	// Report wrap situation...
	if (intPrevFrame > intFrame)
	{
		m_bReachedEnd = true;

		if (bWrapAnimation)
		{
			AnimationWrapped( pControl );
		}
		else
		{
			// Only sent the wrapped message once.
			// when we're in non-wrapping mode
			if (prevFrame < numFrames)
				AnimationWrapped( pControl );
			intFrame = numFrames - 1;
		}
	}

	m_AnimatedTextureFrameNumVar->SetIntValue( intFrame );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CMaterialModifyProxy::GetAnimationStartTime( void* pArg )
{
	IClientRenderable *pRend = (IClientRenderable *)pArg;
	if (!pRend)
		return 0.0f;

	CBaseEntity* pEntity = EntityCast<CBaseEntity>( pRend->GetIClientUnknown()->GetBaseEntity() );
	if (pEntity)
	{
		return pEntity->GetTextureAnimationStartTime();
	}
	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaterialModifyProxy::AnimationWrapped( void* pArg )
{
	IClientRenderable *pRend = (IClientRenderable *)pArg;
	if (!pRend)
		return;

	CBaseEntity* pEntity = EntityCast<CBaseEntity>( pRend->GetIClientUnknown()->GetBaseEntity() );
	if (pEntity)
	{
		pEntity->TextureAnimationWrapped();
	}
}

//-----------------------------------------------------------------------------
// Does the dirty deed
//-----------------------------------------------------------------------------
void CMaterialModifyProxy::OnBindFloatLerp( CMaterialModifyControl *pControl )
{
	if ( !pControl )
		return;

	if ( pControl->HasNewAnimationCommands() )
	{
		pControl->SetAnimationStartTime( gpGlobals->curtime );
		pControl->ClearAnimationCommands();
	}

	// Read the data from the modify entity
	materialfloatlerpcommands_t sCommands;
	pControl->GetFloatLerpCommands( &sCommands );

	m_flStartValue = sCommands.flStartValue;
	m_flEndValue = sCommands.flEndValue;
	m_flTransitionTime = sCommands.flTransitionTime;
	m_flStartTime = pControl->GetAnimationStartTime();
	bool bFound;
	m_pMaterialVar = m_pMaterial->FindVar( pControl->GetMaterialVariableName(), &bFound, false );

	if( bFound )
	{
		float currentValue;
		if( m_flTransitionTime > 0.0f )
		{
			currentValue = m_flStartValue + ( m_flEndValue - m_flStartValue ) * clamp( ( ( gpGlobals->curtime - m_flStartTime ) / m_flTransitionTime ), 0.0f, 1.0f );
		}
		else
		{
			currentValue = m_flEndValue;
		}

		if( debug_materialmodifycontrol_client.GetBool() && V_stristr( m_pMaterial->GetName(), "faceandhair" ) && V_stristr( m_pMaterialVar->GetName(), "warp" ) )
		{
			static int count = 0;
			DevMsg( 1, "CMaterialFloatLerpProxy::OnBind \"%s\" %s=%f %d\n", m_pMaterial->GetName(), m_pMaterialVar->GetName(), currentValue, count++ );
		}
		m_pMaterialVar->SetFloatValue( currentValue );
	}
}

//=============================================================================
//
// MATERIALMODIFYANIMATED PROXY 
//
class CMaterialModifyAnimatedProxy : public CBaseAnimatedTextureProxy
{
public:
	CMaterialModifyAnimatedProxy() {};
	virtual ~CMaterialModifyAnimatedProxy() {};
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pEntity );

	virtual float GetAnimationStartTime( void* pBaseEntity );
	virtual void AnimationWrapped( void* pC_BaseEntity );

private:
	IMaterial	*m_pMaterial;
	int			m_iFrameStart;
	int			m_iFrameEnd;
	bool		m_bReachedEnd;
	float		m_flStartTime;
	bool		m_bCustomWrap;
	float		m_flCustomFramerate;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMaterialModifyAnimatedProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	m_pMaterial = pMaterial;
	m_iFrameStart = MATERIAL_MODIFY_ANIMATION_UNSET;
	m_iFrameEnd = MATERIAL_MODIFY_ANIMATION_UNSET;
	m_bReachedEnd = false;
	return CBaseAnimatedTextureProxy::Init( pMaterial, pKeyValues );
}

//-----------------------------------------------------------------------------
// Does the dirty deed
//-----------------------------------------------------------------------------
void CMaterialModifyAnimatedProxy::OnBind( void *pEntity )
{
	contract_assert( m_AnimatedTextureVar );
	if( m_AnimatedTextureVar->GetType() != MATERIAL_VAR_TYPE_TEXTURE )
		return;

	ITexture *pTexture;
	pTexture = m_AnimatedTextureVar->GetTextureValue();

	// Get the modified material vars from the entity input
	IClientRenderable *pRend = (IClientRenderable *)pEntity;
	if ( pRend )
	{
		CBaseEntity *pBaseEntity = EntityCast<CBaseEntity>( pRend->GetIClientUnknown()->GetBaseEntity() );
		if ( pBaseEntity )
		{
			for ( CBaseEntity *pChild = pBaseEntity->FirstMoveChild(); pChild; pChild = pChild->NextMovePeer() )
			{
				CMaterialModifyControl *pControl = dynamic_cast<CMaterialModifyControl*>( pChild );
				if ( !pControl )
					continue;

				if ( !pControl->HasNewAnimationCommands() )
					continue;

				// Read the data from the modify entity
				materialanimcommands_t sCommands;
				pControl->GetAnimationCommands( &sCommands );

				m_iFrameStart = sCommands.iFrameStart;
				m_iFrameEnd = sCommands.iFrameEnd;
				m_bCustomWrap = sCommands.bWrap;
				m_flCustomFramerate = sCommands.flFrameRate;
				m_bReachedEnd = false;

				m_flStartTime = gpGlobals->curtime;

				pControl->ClearAnimationCommands();
			}
		}
	}

	// Init all the vars based on whether we're using the base material settings, 
	// or the custom ones from the entity input.
	int numFrames;
	bool bWrapAnimation;
	float flFrameRate;
	int iLastFrame;

	// Do we have a custom frame section from the server?
	if ( m_iFrameStart != MATERIAL_MODIFY_ANIMATION_UNSET )
	{
		if ( m_iFrameEnd == MATERIAL_MODIFY_ANIMATION_UNSET )
		{
			m_iFrameEnd = pTexture->GetNumAnimationFrames();
		}

		numFrames = (m_iFrameEnd - m_iFrameStart) + 1;
		bWrapAnimation = m_bCustomWrap;
		flFrameRate = m_flCustomFramerate;
		iLastFrame = (m_iFrameEnd - 1);
	}
	else
	{
		numFrames = pTexture->GetNumAnimationFrames();
		bWrapAnimation = m_WrapAnimation;
		flFrameRate = m_FrameRate;
		iLastFrame = (numFrames - 1);
	}

	// Have we already reached the end? If so, stay there.
	if ( m_bReachedEnd && !bWrapAnimation )
	{
		m_AnimatedTextureFrameNumVar->SetIntValue( iLastFrame );
		return;
	}

	// NOTE: Must not use relative time based methods here
	// because the bind proxy can be called many times per frame.
	// Prevent multiple Wrap callbacks to be sent for no wrap mode
	float startTime;
	if ( m_iFrameStart != MATERIAL_MODIFY_ANIMATION_UNSET )
	{
		startTime = m_flStartTime;
	}
	else
	{
		startTime = GetAnimationStartTime(pEntity);
	}
	float deltaTime = gpGlobals->curtime - startTime;
	float prevTime = deltaTime - gpGlobals->frametime;

	// Clamp..
	if (deltaTime < 0.0f)
		deltaTime = 0.0f;
	if (prevTime < 0.0f)
		prevTime = 0.0f;

	float frame = flFrameRate * deltaTime;	
	float prevFrame = flFrameRate * prevTime;

	int intFrame = ((int)frame) % numFrames; 
	int intPrevFrame = ((int)prevFrame) % numFrames;

	if ( m_iFrameStart != MATERIAL_MODIFY_ANIMATION_UNSET )
	{
		intFrame += m_iFrameStart;
		intPrevFrame += m_iFrameStart;
	}

	// Report wrap situation...
	if (intPrevFrame > intFrame)
	{
		m_bReachedEnd = true;

		if (bWrapAnimation)
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

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CMaterialModifyAnimatedProxy::GetAnimationStartTime( void* pArg )
{
	IClientRenderable *pRend = (IClientRenderable *)pArg;
	if (!pRend)
		return 0.0f;

	CBaseEntity* pEntity = EntityCast<CBaseEntity>( pRend->GetIClientUnknown()->GetBaseEntity() );
	if (pEntity)
	{
		return pEntity->GetTextureAnimationStartTime();
	}
	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaterialModifyAnimatedProxy::AnimationWrapped( void* pArg )
{
	IClientRenderable *pRend = (IClientRenderable *)pArg;
	if (!pRend)
		return;

	CBaseEntity* pEntity = EntityCast<CBaseEntity>( pRend->GetIClientUnknown()->GetBaseEntity() );
	if (pEntity)
	{
		pEntity->TextureAnimationWrapped();
	}
}

EXPOSE_MATERIAL_PROXY( CMaterialModifyProxy, MaterialModify );
EXPOSE_MATERIAL_PROXY( CMaterialModifyAnimatedProxy, MaterialModifyAnimated );

class CMaterialProxyDict : public IMaterialProxyDict
{
public:
	IMaterialProxy *	CreateProxy( const char *proxyName );
	void				Add( const char *pMaterialProxyName, MaterialProxyFactory_t *pMaterialProxyFactory );
private:
	ks::idict<MaterialProxyFactory_t *> m_StringToProxyFactoryMap;
};

void CMaterialProxyDict::Add( const char *pMaterialProxyName, MaterialProxyFactory_t *pMaterialProxyFactory )
{
	contract_assert( pMaterialProxyName );
	m_StringToProxyFactoryMap[pMaterialProxyName] = pMaterialProxyFactory;
}

IMaterialProxyDict &GetMaterialProxyDict()
{
	static CMaterialProxyDict g_MaterialProxyDict;
	return g_MaterialProxyDict;
}

IMaterialProxy *CMaterialProxyDict::CreateProxy( const char *pMaterialProxyName )
{
	MaterialProxyFactory_t **ppFactory = ks::find_ptr( m_StringToProxyFactoryMap, pMaterialProxyName );
	if ( !ppFactory )
	{
		return nullptr;
	}
	MaterialProxyFactory_t *pMaterialProxyFactory = *ppFactory;
	contract_assert( pMaterialProxyFactory );
	return (*pMaterialProxyFactory)();
}

using namespace ::ks::reflect;


//-----------------------------------------------------------------------------
// Adds two vars...
//-----------------------------------------------------------------------------

class CAddProxy : public CFunctionProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );
};

bool CAddProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	// Requires 2 args..
	bool ok = CFunctionProxy::Init( pMaterial, pKeyValues );
	ok = ok && m_pSrc2;
	return ok;
}

void CAddProxy::OnBind( void *pC_BaseEntity )
{
	contract_assert( m_pSrc1 && m_pSrc2 && m_pResult );

	MaterialVarType_t resultType;
	int vecSize;
	ComputeResultType( resultType, vecSize );

	switch( resultType )
	{
	case MATERIAL_VAR_TYPE_VECTOR:
		{
			Vector a, b, c;
			m_pSrc1->GetVecValue( a.Base(), vecSize ); 
			m_pSrc2->GetVecValue( b.Base(), vecSize ); 
			VectorAdd( a, b, c );
			m_pResult->SetVecValue( c.Base(), vecSize );
		}
		break;

	case MATERIAL_VAR_TYPE_FLOAT:
		SetFloatResult( m_pSrc1->GetFloatValue() + m_pSrc2->GetFloatValue() );
		break;

	case MATERIAL_VAR_TYPE_INT:
		m_pResult->SetFloatValue( m_pSrc1->GetIntValue() + m_pSrc2->GetIntValue() );
		break;
	}
}

EXPOSE_MATERIAL_PROXY( CAddProxy, Add );

//-----------------------------------------------------------------------------
// modulo
//-----------------------------------------------------------------------------

class CModProxy : public CFunctionProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );
};

bool CModProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	// Requires 2 args..
	bool ok = CFunctionProxy::Init( pMaterial, pKeyValues );
	ok = ok && m_pSrc2;
	return ok;
}

void CModProxy::OnBind( void *pC_BaseEntity )
{
	contract_assert( m_pSrc1 && m_pSrc2 && m_pResult );
	SetFloatResult(  std::fmod( m_pSrc1->GetFloatValue(), m_pSrc2->GetFloatValue() )  );
}

EXPOSE_MATERIAL_PROXY( CModProxy, Modulo );

//-----------------------------------------------------------------------------
// Subtracts two vars...
//-----------------------------------------------------------------------------

class CSubtractProxy : public CFunctionProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );
};

bool CSubtractProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	// Requires 2 args..
	bool ok = CFunctionProxy::Init( pMaterial, pKeyValues );
	ok = ok && m_pSrc2;
	return ok;
}

void CSubtractProxy::OnBind( void *pC_BaseEntity )
{
	contract_assert( m_pSrc1 && m_pSrc2 && m_pResult );

	MaterialVarType_t resultType;
	int vecSize;
	ComputeResultType( resultType, vecSize );

	switch( resultType )
	{
	case MATERIAL_VAR_TYPE_VECTOR:
		{
			Vector a, b, c;
			m_pSrc1->GetVecValue( a.Base(), vecSize ); 
			m_pSrc2->GetVecValue( b.Base(), vecSize ); 
			VectorSubtract( a, b, c );
			m_pResult->SetVecValue( c.Base(), vecSize );
		}
		break;

	case MATERIAL_VAR_TYPE_FLOAT:
		SetFloatResult( m_pSrc1->GetFloatValue() - m_pSrc2->GetFloatValue() );
		break;

	case MATERIAL_VAR_TYPE_INT:
		m_pResult->SetFloatValue( m_pSrc1->GetIntValue() - m_pSrc2->GetIntValue() );
		break;
	}
}

EXPOSE_MATERIAL_PROXY( CSubtractProxy, Subtract );

//-----------------------------------------------------------------------------
// Multiplies two vars...
//-----------------------------------------------------------------------------

class CMultiplyProxy : public CFunctionProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );
};

bool CMultiplyProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	// Requires 2 args..
	bool ok = CFunctionProxy::Init( pMaterial, pKeyValues );
	ok = ok && m_pSrc2;
	return ok;
}

void CMultiplyProxy::OnBind( void *pC_BaseEntity )
{
	contract_assert( m_pSrc1 && m_pSrc2 && m_pResult );

	MaterialVarType_t resultType;
	int vecSize;
	ComputeResultType( resultType, vecSize );

	switch( resultType )
	{
	case MATERIAL_VAR_TYPE_VECTOR:
		{
			Vector a, b, c;
			m_pSrc1->GetVecValue( a.Base(), vecSize ); 
			m_pSrc2->GetVecValue( b.Base(), vecSize ); 
			VectorMultiply( a, b, c );
			m_pResult->SetVecValue( c.Base(), vecSize );
		}
		break;

	case MATERIAL_VAR_TYPE_FLOAT:
		SetFloatResult( m_pSrc1->GetFloatValue() * m_pSrc2->GetFloatValue() );
		break;

	case MATERIAL_VAR_TYPE_INT:
		m_pResult->SetFloatValue( m_pSrc1->GetIntValue() * m_pSrc2->GetIntValue() );
		break;
	}
}

EXPOSE_MATERIAL_PROXY( CMultiplyProxy, Multiply );

//-----------------------------------------------------------------------------
// divides two vars...
//-----------------------------------------------------------------------------

class CDivideProxy : public CFunctionProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );
};

bool CDivideProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	// Requires 2 args..
	bool ok = CFunctionProxy::Init( pMaterial, pKeyValues );
	ok = ok && m_pSrc2;
	return ok;
}

void CDivideProxy::OnBind( void *pC_BaseEntity )
{
	contract_assert( m_pSrc1 && m_pSrc2 && m_pResult );

	MaterialVarType_t resultType;
	int vecSize;
	ComputeResultType( resultType, vecSize );

	switch( resultType )
	{
	case MATERIAL_VAR_TYPE_VECTOR:
		{
			Vector a, b, c;
			m_pSrc1->GetVecValue( a.Base(), vecSize ); 
			m_pSrc2->GetVecValue( b.Base(), vecSize ); 
			VectorDivide( a, b, c );
			m_pResult->SetVecValue( c.Base(), vecSize );
		}
		break;

	case MATERIAL_VAR_TYPE_FLOAT:
		if (m_pSrc2->GetFloatValue() != 0)
		{
			SetFloatResult( m_pSrc1->GetFloatValue() / m_pSrc2->GetFloatValue() );
		}
		else
		{
			SetFloatResult( m_pSrc1->GetFloatValue() );
		}
		break;

	case MATERIAL_VAR_TYPE_INT:
		if (m_pSrc2->GetIntValue() != 0)
		{
			m_pResult->SetFloatValue( m_pSrc1->GetIntValue() / m_pSrc2->GetIntValue() );
		}
		else
		{
			m_pResult->SetFloatValue( m_pSrc1->GetIntValue() );
		}
		break;
	}
}

EXPOSE_MATERIAL_PROXY( CDivideProxy, Divide );

//-----------------------------------------------------------------------------
// clamps a var...
//-----------------------------------------------------------------------------

class CClampProxy : public CFunctionProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	CFloatInput m_Min;
	CFloatInput m_Max;
};

bool CClampProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CFunctionProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_Min.Init( pMaterial, pKeyValues, "min", 0 ))
		return false;

	if (!m_Max.Init( pMaterial, pKeyValues, "max", 1 ))
		return false;

	return true;
}

void CClampProxy::OnBind( void *pC_BaseEntity )
{
	contract_assert( m_pSrc1 && m_pResult );

	MaterialVarType_t resultType;
	int vecSize;
	ComputeResultType( resultType, vecSize );

	float flMin = m_Min.GetFloat();
	float flMax = m_Max.GetFloat();

	if (flMin > flMax)
	{
		float flTemp = flMin;
		flMin = flMax;
		flMax = flTemp;
	}

	switch( resultType )
	{
	case MATERIAL_VAR_TYPE_VECTOR:
		{
			Vector a;
			m_pSrc1->GetVecValue( a.Base(), vecSize );
			for (int i = 0; i < vecSize; ++i)
			{
				if (a[i] < flMin)
					a[i] = flMin;
				else if (a[i] > flMax)
					a[i] = flMax;
			}
			m_pResult->SetVecValue( a.Base(), vecSize );
		}
		break;

	case MATERIAL_VAR_TYPE_FLOAT:
		{
			float src = m_pSrc1->GetFloatValue();
			if (src < flMin)
				src = flMin;
			else if (src > flMax)
				src = flMax;
			SetFloatResult( src );
		}
		break;

	case MATERIAL_VAR_TYPE_INT:
		{
			int src = m_pSrc1->GetIntValue();
			if (src < flMin)
				src = flMin;
			else if (src > flMax)
				src = flMax;
			m_pResult->SetIntValue( src );
		}
		break;
	}
}

EXPOSE_MATERIAL_PROXY( CClampProxy, Clamp );

//-----------------------------------------------------------------------------
// Creates a sinusoid
//-----------------------------------------------------------------------------

// sinePeriod: time that it takes to go through whole sine wave in seconds (default: 1.0f)
// sineMax : the max value for the sine wave (default: 1.0f )
// sineMin: the min value for the sine wave  (default: 0.0f )
class CSineProxy : public CResultProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );

private:
	CFloatInput m_SinePeriod;
	CFloatInput m_SineMax;
	CFloatInput m_SineMin;
	CFloatInput m_SineTimeOffset;
};

bool CSineProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_SinePeriod.Init( pMaterial, pKeyValues, "sinePeriod", 1.0f ))
		return false;
	if (!m_SineMax.Init( pMaterial, pKeyValues, "sineMax", 1.0f ))
		return false;
	if (!m_SineMin.Init( pMaterial, pKeyValues, "sineMin", 0.0f ))
		return false;
	if (!m_SineTimeOffset.Init( pMaterial, pKeyValues, "timeOffset", 0.0f ))
		return false;

	return true;
}

void CSineProxy::OnBind( void *pC_BaseEntity )
{
	contract_assert( m_pResult );

	float flValue;
	float flSineTimeOffset = m_SineTimeOffset.GetFloat();
	float flSineMax = m_SineMax.GetFloat();
	float flSineMin = m_SineMin.GetFloat();
	float flSinePeriod = m_SinePeriod.GetFloat();
	if (flSinePeriod == 0)
		flSinePeriod = 1;

	// get a value in [0,1]
	flValue = ( std::sin( 2.0f * std::numbers::pi * (gpGlobals->curtime - flSineTimeOffset) / flSinePeriod ) * 0.5f ) + 0.5f;
	// get a value in [min,max]	
	flValue = ( flSineMax - flSineMin ) * flValue + flSineMin;
	
	SetFloatResult( flValue );
}

EXPOSE_MATERIAL_PROXY( CSineProxy, Sine );

//-----------------------------------------------------------------------------
// copies a var...
//-----------------------------------------------------------------------------

class CEqualsProxy : public CFunctionProxy
{
public:
	void OnBind( void *pC_BaseEntity );
};

void CEqualsProxy::OnBind( void *pC_BaseEntity )
{
	contract_assert( m_pSrc1 && m_pResult );

	MaterialVarType_t resultType;
	int vecSize;
	ComputeResultType( resultType, vecSize );

	switch( resultType )
	{
	case MATERIAL_VAR_TYPE_VECTOR:
		{
			Vector a;
			m_pSrc1->GetVecValue( a.Base(), vecSize );
			m_pResult->SetVecValue( a.Base(), vecSize );
		}
		break;

	case MATERIAL_VAR_TYPE_FLOAT:
		SetFloatResult( m_pSrc1->GetFloatValue() );
		break;

	case MATERIAL_VAR_TYPE_INT:
		m_pResult->SetIntValue( m_pSrc1->GetIntValue() );
		break;
	}
}

EXPOSE_MATERIAL_PROXY( CEqualsProxy, Equals );

//-----------------------------------------------------------------------------
// Get the fractional part of a var
//-----------------------------------------------------------------------------

class CFracProxy : public CFunctionProxy
{
public:
	void OnBind( void *pC_BaseEntity );
};

void CFracProxy::OnBind( void *pC_BaseEntity )
{
	contract_assert( m_pSrc1 && m_pResult );

	MaterialVarType_t resultType;
	int vecSize;
	ComputeResultType( resultType, vecSize );

	switch( resultType )
	{
	case MATERIAL_VAR_TYPE_VECTOR:
		{
			Vector a;
			m_pSrc1->GetVecValue( a.Base(), vecSize );
			a[0] -= ( float )( int )a[0];
			a[1] -= ( float )( int )a[1];
			a[2] -= ( float )( int )a[2];
			m_pResult->SetVecValue( a.Base(), vecSize );
		}
		break;

	case MATERIAL_VAR_TYPE_FLOAT:
		{
			float a = m_pSrc1->GetFloatValue();
			a -= ( int )a;
			SetFloatResult( a );
		}
		break;

	case MATERIAL_VAR_TYPE_INT:
		// don't do anything besides assignment!
		m_pResult->SetIntValue( m_pSrc1->GetIntValue() );
		break;
	}
}

EXPOSE_MATERIAL_PROXY( CFracProxy, Frac );

//-----------------------------------------------------------------------------
// Get the Integer part of a var
//-----------------------------------------------------------------------------

class CIntProxy : public CFunctionProxy
{
public:
	void OnBind( void *pC_BaseEntity );
};

void CIntProxy::OnBind( void *pC_BaseEntity )
{
	contract_assert( m_pSrc1 && m_pResult );

	MaterialVarType_t resultType;
	int vecSize;
	ComputeResultType( resultType, vecSize );

	switch( resultType )
	{
	case MATERIAL_VAR_TYPE_VECTOR:
		{
			Vector a;
			m_pSrc1->GetVecValue( a.Base(), vecSize );
			a[0] = ( float )( int )a[0];
			a[1] = ( float )( int )a[1];
			a[2] = ( float )( int )a[2];
			m_pResult->SetVecValue( a.Base(), vecSize );
		}
		break;

	case MATERIAL_VAR_TYPE_FLOAT:
		{
			float a = m_pSrc1->GetFloatValue();
			a = ( float )( int )a;
			SetFloatResult( a );
		}
		break;

	case MATERIAL_VAR_TYPE_INT:
		// don't do anything besides assignment!
		m_pResult->SetIntValue( m_pSrc1->GetIntValue() );
		break;
	}
}

EXPOSE_MATERIAL_PROXY( CIntProxy, Int );

//-----------------------------------------------------------------------------
// Linear ramp proxy
//-----------------------------------------------------------------------------
class CLinearRampProxy : public CResultProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );

private:
	CFloatInput m_Rate;
	CFloatInput m_InitialValue;
};

bool CLinearRampProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_Rate.Init( pMaterial, pKeyValues, "rate", 1 ))
		return false;

	if (!m_InitialValue.Init( pMaterial, pKeyValues, "initialValue", 0 ))
		return false;

	return true;
}

void CLinearRampProxy::OnBind( void *pC_BaseEntity )
{
	contract_assert( m_pResult );

	float flValue;
	
	// get a value in [0,1]
	flValue = m_Rate.GetFloat() * gpGlobals->curtime + m_InitialValue.GetFloat();	
	SetFloatResult( flValue );
}

EXPOSE_MATERIAL_PROXY( CLinearRampProxy, LinearRamp );

//-----------------------------------------------------------------------------
// Uniform noise proxy
//-----------------------------------------------------------------------------
class CUniformNoiseProxy : public CResultProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );

private:
	CFloatInput	m_flMinVal;
	CFloatInput	m_flMaxVal;
};

bool CUniformNoiseProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_flMinVal.Init( pMaterial, pKeyValues, "minVal", 0 ))
		return false;

	if (!m_flMaxVal.Init( pMaterial, pKeyValues, "maxVal", 1 ))
		return false;

	return true;
}

void CUniformNoiseProxy::OnBind( void *pC_BaseEntity )
{
	SetFloatResult( random_valve->RandomFloat( m_flMinVal.GetFloat(), m_flMaxVal.GetFloat() ) );
}

EXPOSE_MATERIAL_PROXY( CUniformNoiseProxy, UniformNoise );

//-----------------------------------------------------------------------------
// Gaussian noise proxy
//-----------------------------------------------------------------------------
class CGaussianNoiseProxy : public CResultProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );

private:
	CFloatInput m_Mean;
	CFloatInput m_StdDev;
	CFloatInput	m_flMinVal;
	CFloatInput	m_flMaxVal;
};

bool CGaussianNoiseProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_Mean.Init( pMaterial, pKeyValues, "mean", 0.0f ))
		return false;

	if (!m_StdDev.Init( pMaterial, pKeyValues, "halfwidth", 1.0f ))
		return false;

	if (!m_flMinVal.Init( pMaterial, pKeyValues, "minVal", -std::numeric_limits<float>::max() ))
		return false;

	if (!m_flMaxVal.Init( pMaterial, pKeyValues, "maxVal", std::numeric_limits<float>::max() ))
		return false;

	return true;
}

void CGaussianNoiseProxy::OnBind( void *pC_BaseEntity )
{
	float flMean = m_Mean.GetFloat();
	float flStdDev = m_StdDev.GetFloat();
	float flVal = randomgaussian->RandomFloat( flMean, flStdDev );
	float flMaxVal = m_flMaxVal.GetFloat();
	float flMinVal = m_flMinVal.GetFloat();

	if (flMinVal > flMaxVal)
	{
		float flTemp = flMinVal;
		flMinVal = flMaxVal;
		flMaxVal = flTemp;
	}

	// clamp
	if (flVal < flMinVal)
		flVal = flMinVal;
	else if ( flVal > flMaxVal )
		flVal = flMaxVal;

	SetFloatResult( flVal );
}

EXPOSE_MATERIAL_PROXY( CGaussianNoiseProxy, GaussianNoise );

//-----------------------------------------------------------------------------
// Exponential proxy
//-----------------------------------------------------------------------------
class CExponentialProxy : public CFunctionProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );

private:
	CFloatInput	m_Scale;
	CFloatInput	m_Offset;
	CFloatInput	m_flMinVal;
	CFloatInput	m_flMaxVal;
};

bool CExponentialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CFunctionProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_Scale.Init( pMaterial, pKeyValues, "scale", 1.0f ))
		return false;

	if (!m_Offset.Init( pMaterial, pKeyValues, "offset", 0.0f ))
		return false;

	if (!m_flMinVal.Init( pMaterial, pKeyValues, "minVal", -std::numeric_limits<float>::max() ))
		return false;

	if (!m_flMaxVal.Init( pMaterial, pKeyValues, "maxVal", std::numeric_limits<float>::max() ))
		return false;

	return true;
}

void CExponentialProxy::OnBind( void *pC_BaseEntity )
{	
	float flVal = m_Scale.GetFloat() * std::exp(m_pSrc1->GetFloatValue( ) + m_Offset.GetFloat());

	float flMaxVal = m_flMaxVal.GetFloat();
	float flMinVal = m_flMinVal.GetFloat();

	if (flMinVal > flMaxVal)
	{
		float flTemp = flMinVal;
		flMinVal = flMaxVal;
		flMaxVal = flTemp;
	}

	// clamp
	if (flVal < flMinVal)
		flVal = flMinVal;
	else if ( flVal > flMaxVal )
		flVal = flMaxVal;

	SetFloatResult( flVal );
}

EXPOSE_MATERIAL_PROXY( CExponentialProxy, Exponential );

//-----------------------------------------------------------------------------
// Absolute value proxy
//-----------------------------------------------------------------------------
class CAbsProxy : public CFunctionProxy
{
public:
	virtual void OnBind( void *pC_BaseEntity );
};

void CAbsProxy::OnBind( void *pC_BaseEntity )
{	
	SetFloatResult( fabs(m_pSrc1->GetFloatValue( )) );
}

EXPOSE_MATERIAL_PROXY( CAbsProxy, Abs );

//-----------------------------------------------------------------------------
// Empty proxy-- used to comment out large proxy blocks
//-----------------------------------------------------------------------------
class CEmptyProxy : public IMaterialProxy
{
public:
	CEmptyProxy() {}
	virtual ~CEmptyProxy() {}
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues ) { return true; }
	virtual void OnBind( void *pC_BaseEntity ) {}
	virtual void Release( void ) { delete this; }
	virtual IMaterial *GetMaterial() { return nullptr; }
};

EXPOSE_MATERIAL_PROXY( CEmptyProxy, Empty );

//-----------------------------------------------------------------------------
// Comparison proxy
//-----------------------------------------------------------------------------
class CLessOrEqualProxy : public CFunctionProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	IMaterialVar *m_pLessVar;
	IMaterialVar *m_pGreaterVar;
};

bool CLessOrEqualProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	char const* pLessEqualVar = pKeyValues->GetString( "lessEqualVar" );
	if( !pLessEqualVar )
		return false;

	bool foundVar;
	m_pLessVar = pMaterial->FindVar( pLessEqualVar, &foundVar, true );
	if( !foundVar )
		return false;

	char const* pGreaterVar = pKeyValues->GetString( "greaterVar" );
	if( !pGreaterVar )
		return false;

	foundVar;
	m_pGreaterVar = pMaterial->FindVar( pGreaterVar, &foundVar, true );
	if( !foundVar )
		return false;

	// Compare 2 args..
	bool ok = CFunctionProxy::Init( pMaterial, pKeyValues );
	ok = ok && m_pSrc2;
	return ok;
}

void CLessOrEqualProxy::OnBind( void *pC_BaseEntity )
{
	contract_assert( m_pSrc1 && m_pSrc2 && m_pLessVar && m_pGreaterVar && m_pResult );

	IMaterialVar *pSourceVar;
	if (m_pSrc1->GetFloatValue() <= m_pSrc2->GetFloatValue())
	{
		pSourceVar = m_pLessVar;
	}
	else
	{
		pSourceVar = m_pGreaterVar;
	}

	int vecSize = 0;
	MaterialVarType_t resultType = m_pResult->GetType();
	if (resultType == MATERIAL_VAR_TYPE_VECTOR)
	{
		if (m_ResultVecComp >= 0)
			resultType = MATERIAL_VAR_TYPE_FLOAT;
		vecSize = m_pResult->VectorSize();
	}
	else if (resultType == MATERIAL_VAR_TYPE_UNDEFINED)
	{
		resultType = pSourceVar->GetType();
		if (resultType == MATERIAL_VAR_TYPE_VECTOR)
		{
			vecSize = pSourceVar->VectorSize();
		}
	}

	switch( resultType )
	{
	case MATERIAL_VAR_TYPE_VECTOR:
		{
			Vector src;
			pSourceVar->GetVecValue( src.Base(), vecSize ); 
			m_pResult->SetVecValue( src.Base(), vecSize );
		}
		break;

	case MATERIAL_VAR_TYPE_FLOAT:
		SetFloatResult( pSourceVar->GetFloatValue() );
		break;

	case MATERIAL_VAR_TYPE_INT:
		m_pResult->SetFloatValue( pSourceVar->GetIntValue() );
		break;
	}
}

EXPOSE_MATERIAL_PROXY( CLessOrEqualProxy, LessOrEqual );

//-----------------------------------------------------------------------------
// WrapMinMax proxy
//-----------------------------------------------------------------------------
class CWrapMinMaxProxy : public CFunctionProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );

private:
	CFloatInput	m_flMinVal;
	CFloatInput	m_flMaxVal;
};

bool CWrapMinMaxProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CFunctionProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_flMinVal.Init( pMaterial, pKeyValues, "minVal", 0 ))
		return false;

	if (!m_flMaxVal.Init( pMaterial, pKeyValues, "maxVal", 1 ))
		return false;

	return true;
}

void CWrapMinMaxProxy::OnBind( void *pC_BaseEntity )
{
	contract_assert( m_pSrc1 && m_pResult );

	if ( m_flMaxVal.GetFloat() <= m_flMinVal.GetFloat() ) // Bad input, just return the min
	{
		SetFloatResult( m_flMinVal.GetFloat() );
	}
	else
	{
		float flResult = ( m_pSrc1->GetFloatValue() - m_flMinVal.GetFloat() ) / ( m_flMaxVal.GetFloat() - m_flMinVal.GetFloat() );

		if ( flResult >= 0.0f )
		{
			flResult -= ( float )( int )flResult;
		}
		else // Negative
		{
			flResult -= ( float )( ( ( int )flResult ) - 1 );
		}

		flResult *= ( m_flMaxVal.GetFloat() - m_flMinVal.GetFloat() );
		flResult += m_flMinVal.GetFloat();

		SetFloatResult( flResult );
	}
}

EXPOSE_MATERIAL_PROXY( CWrapMinMaxProxy, WrapMinMax );

//-----------------------------------------------------------------------------
// RemapValClamped
//-----------------------------------------------------------------------------

class CRemapValClampedProxy : public CFunctionProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	CFloatInput m_RangeInMin;
	CFloatInput m_RangeInMax;
	CFloatInput m_RangeOutMin;
	CFloatInput m_RangeOutMax;
};

bool CRemapValClampedProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if ( !CFunctionProxy::Init( pMaterial, pKeyValues ) )
		return false;

	if ( !m_RangeInMin.Init( pMaterial, pKeyValues, "range_in_min", 0 ) )
		return false;

	if ( !m_RangeInMax.Init( pMaterial, pKeyValues, "range_in_max", 1 ) )
		return false;

	if ( !m_RangeOutMin.Init( pMaterial, pKeyValues, "range_out_min", 0 ) )
		return false;

	if ( !m_RangeOutMax.Init( pMaterial, pKeyValues, "range_out_max", 1 ) )
		return false;

	return true;
}

void CRemapValClampedProxy::OnBind( void *pC_BaseEntity )
{
	contract_assert( m_pSrc1 && m_pResult );

	MaterialVarType_t resultType;
	int vecSize;
	ComputeResultType( resultType, vecSize );

	float flInMin = m_RangeInMin.GetFloat();
	float flInMax = m_RangeInMax.GetFloat();
	float flOutMin = m_RangeOutMin.GetFloat();
	float flOutMax = m_RangeOutMax.GetFloat();
	
	switch ( resultType )
	{
	case MATERIAL_VAR_TYPE_FLOAT:
		{
			SetFloatResult( RemapValClamped( m_pSrc1->GetFloatValue(), flInMin, flInMax, flOutMin, flOutMax ) );
		}
		break;

	case MATERIAL_VAR_TYPE_INT:
		{
			m_pResult->SetIntValue( (int)RemapValClamped( m_pSrc1->GetIntValue(), flInMin, flInMax, flOutMin, flOutMax ) );
		}
		break;
	}
}

EXPOSE_MATERIAL_PROXY( CRemapValClampedProxy, RemapValClamp );

using namespace ::ks::reflect;


//-----------------------------------------------------------------------------
// Texture transform proxy
//-----------------------------------------------------------------------------
class CTextureTransformProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	IMaterialVar *m_pCenterVar;
	IMaterialVar *m_pScaleVar;
	IMaterialVar *m_pRotateVar;
	IMaterialVar *m_pTranslateVar;
};

bool CTextureTransformProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	// All are optional...
	m_pCenterVar = nullptr;
	m_pScaleVar = nullptr;
	m_pRotateVar = nullptr;
	m_pTranslateVar = nullptr;

	bool bFoundVar;
	char const* pVarName = pKeyValues->GetString( "centerVar" );
	if( pVarName && pVarName[0] )
	{
		m_pCenterVar = pMaterial->FindVar( pVarName, &bFoundVar, false );
	}

	pVarName = pKeyValues->GetString( "scaleVar" );
	if( pVarName && pVarName[0] )
	{
		m_pScaleVar = pMaterial->FindVar( pVarName, &bFoundVar, false );
	}

	pVarName = pKeyValues->GetString( "rotateVar" );
	if( pVarName && pVarName[0] )
	{
		m_pRotateVar = pMaterial->FindVar( pVarName, &bFoundVar, false );
	}

	pVarName = pKeyValues->GetString( "translateVar" );
	if( pVarName && pVarName[0] )
	{
		m_pTranslateVar = pMaterial->FindVar( pVarName, &bFoundVar, false );
	}

	return CResultProxy::Init( pMaterial, pKeyValues );
}

void CTextureTransformProxy::OnBind( void *pC_BaseEntity )
{
	Vector2D center( 0.5, 0.5 );
	Vector2D translation( 0, 0 );

	VMatrix mat, temp;

	if (m_pCenterVar)
	{
		m_pCenterVar->GetVecValue( center.Base(), 2 );
	}
	MatrixBuildTranslation( mat, -center.x, -center.y, 0.0f );

	if (m_pScaleVar)
	{
		Vector2D scale;
		m_pScaleVar->GetVecValue( scale.Base(), 2 );
		MatrixBuildScale( temp, scale.x, scale.y, 1.0f );
		MatrixMultiply( temp, mat, mat );
	}

	if (m_pRotateVar)
	{
		float angle = m_pRotateVar->GetFloatValue( );
		MatrixBuildRotateZ( temp, angle );
		MatrixMultiply( temp, mat, mat );
	}
	MatrixBuildTranslation( temp, center.x, center.y, 0.0f );
	MatrixMultiply( temp, mat, mat );

	if (m_pTranslateVar)
	{
		m_pTranslateVar->GetVecValue( translation.Base(), 2 );
		MatrixBuildTranslation( temp, translation.x, translation.y, 0.0f );
		MatrixMultiply( temp, mat, mat );
	}

	m_pResult->SetMatrixValue( mat );
}

EXPOSE_MATERIAL_PROXY( CTextureTransformProxy, TextureTransform );

//-----------------------------------------------------------------------------
// Rotation proxy
//-----------------------------------------------------------------------------
class CMatrixRotateProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	CFloatInput	m_Angle;
	IMaterialVar *m_pAxisVar;
};

bool CMatrixRotateProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	// All are optional...
	m_pAxisVar = nullptr;

	bool bFoundVar;
	char const* pVarName = pKeyValues->GetString( "axisVar" );
	if( pVarName && pVarName[0] )
	{
		m_pAxisVar = pMaterial->FindVar( pVarName, &bFoundVar, false );
	}

	if (!m_Angle.Init( pMaterial, pKeyValues, "angle", 0 ))
		return false;

	return CResultProxy::Init( pMaterial, pKeyValues );
}

void CMatrixRotateProxy::OnBind( void *pC_BaseEntity )
{
	VMatrix mat;
	Vector axis( 0, 0, 1 );
	if (m_pAxisVar)
	{
		m_pAxisVar->GetVecValue( axis.Base(), 3 );
		if (VectorNormalize( axis ) < 1e-3)
		{
			axis.Init( 0, 0, 1 );
		}
	}

	MatrixBuildRotationAboutAxis( mat, axis, m_Angle.GetFloat() );
	m_pResult->SetMatrixValue( mat );
}

EXPOSE_MATERIAL_PROXY( CMatrixRotateProxy, MatrixRotate );

class CHudElement;

using namespace ::ks::reflect;

//
// menu.cpp
//
// generic menu handler
//

static constexpr auto MAX_MENU_STRING = 512;
wchar_t g_szMenuString[MAX_MENU_STRING];
char g_szPrelocalisedMenuString[MAX_MENU_STRING];

//
//-----------------------------------------------------
//

DECLARE_HUDELEMENT( CHudMenu );
DECLARE_HUD_MESSAGE( CHudMenu, ShowMenu );

//
//-----------------------------------------------------
//


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudMenu::CHudMenu( const char *pElementName ) :
	CHudElement( pElementName )
{
	m_nSelectedItem = -1;
	m_flOpenCloseTime = 1.0f;

	SetHiddenBits( HIDEHUD_MISCSTATUS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMenu::Init( void )
{
	HOOK_HUD_MESSAGE( CHudMenu, ShowMenu );

	m_bMenuTakesInput = false;
	m_bMenuDisplayed = false;
	m_bitsValidSlots = 0;
	m_Processed.clear();
	m_nMaxPixels = 0;
	m_nHeight = 0;
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMenu::Reset( void )
{
	g_szPrelocalisedMenuString[0] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHudMenu::IsMenuOpen( void )
{
	return m_bMenuDisplayed && m_bMenuTakesInput;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMenu::VidInit( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMenu::OnThink()
{
	float flSelectionTimeout = MENU_SELECTION_TIMEOUT;

	// If we've been open for a while without input, hide
	if ( m_bMenuDisplayed && ( gpGlobals->curtime - m_flSelectionTime > flSelectionTimeout ) )
	{
		m_bMenuDisplayed = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudMenu::ShouldDraw( void )
{
	bool draw = CHudElement::ShouldDraw() && m_bMenuDisplayed;
	if ( !draw )
		return false;

	// check for if menu is set to disappear
	if ( m_flShutoffTime > 0 && m_flShutoffTime <= gpGlobals->realtime )
	{  
		// times up, shutoff
		m_bMenuDisplayed = false;
		return false;
	}

	return draw;
}

//-----------------------------------------------------------------------------
// Purpose: selects an item from the menu
//-----------------------------------------------------------------------------
void CHudMenu::SelectMenuItem( int menu_item )
{
	// if menu_item is in a valid slot,  send a menuselect command to the server
	if ( (menu_item > 0) && (m_bitsValidSlots & (1 << (menu_item-1))) )
	{
		char szbuf[32];
		V_snprintf( szbuf, sizeof( szbuf ), "menuselect %d\n", menu_item );
		engine->ClientCmd( szbuf );

		m_nSelectedItem = menu_item;
		// Pulse the selection
		GetClientMode()->GetViewportAnimationController()->StartAnimationSequence("MenuPulse");

		// remove the menu quickly
		m_bMenuTakesInput = false;
		m_flShutoffTime = gpGlobals->realtime + m_flOpenCloseTime;
		GetClientMode()->GetViewportAnimationController()->StartAnimationSequence("MenuClose");
	}
}

void CHudMenu::ProcessText( void )
{
	m_Processed.clear();
	m_nMaxPixels = 0;
	m_nHeight = 0;

	int i = 0;
	int startpos = i;
	int menuitem = 0;
	while ( i < MAX_MENU_STRING  )
	{
		wchar_t ch = g_szMenuString[ i ];
		if ( ch == 0 )
			break;

		if ( i == startpos && 
			( ch == L'-' && g_szMenuString[ i + 1 ] == L'>' ) )
		{
			// Special handling for menu item specifiers
			swscanf( &g_szMenuString[ i + 2 ], L"%d", &menuitem );
			i += 2;
			startpos += 2;

			continue;
		}

		// Skip to end of line
		while ( i < MAX_MENU_STRING && g_szMenuString[i] != 0 && g_szMenuString[i] != L'\n' )
		{
			i++;
		}

		// Store off line
		if ( ( i - startpos ) >= 1 )
		{
			ProcessedLine line;
			line.menuitem = menuitem;
			line.startchar = startpos;
			line.length = i - startpos;
			line.pixels = 0;
			line.height = 0;

			m_Processed.push_back( line );
		}

		menuitem = 0;

		// Skip delimiter
		if ( g_szMenuString[i] == '\n' )
		{
			i++;
		}
		startpos = i;
	}

	// Add final block
	if ( i - startpos >= 1 )
	{
		ProcessedLine line;
		line.menuitem = menuitem;
		line.startchar = startpos;
		line.length = i - startpos;
		line.pixels = 0;
		line.height = 0;

		m_Processed.push_back( line );
	}
}
//-----------------------------------------------------------------------------
// Purpose: Local method to hide a menu, mirroring code found in
//          MsgFunc_ShowMenu.
//-----------------------------------------------------------------------------
void CHudMenu::HideMenu( void )
{
	m_bMenuTakesInput = false;
	m_flShutoffTime = gpGlobals->realtime + m_flOpenCloseTime;
	GetClientMode()->GetViewportAnimationController()->StartAnimationSequence("MenuClose");
}

//-----------------------------------------------------------------------------
// Purpose: Local method to bring up a menu, mirroring code found in
//          MsgFunc_ShowMenu.
//
//   takes two values:
//		menuName  : menu name string 
//		validSlots: a bitfield describing the valid keys
//-----------------------------------------------------------------------------
void CHudMenu::ShowMenu( const char * menuName, int validSlots )
{
	m_flShutoffTime = -1;
	m_bitsValidSlots = validSlots;

	V_strncpy( g_szPrelocalisedMenuString, menuName, sizeof( g_szPrelocalisedMenuString ) );

	GetClientMode()->GetViewportAnimationController()->StartAnimationSequence("MenuOpen");
	m_nSelectedItem = -1;

	// we have the whole string, so we can localise it now
	char szMenuString[MAX_MENU_STRING];
    hudtextmessage->LocaliseTextString( g_szPrelocalisedMenuString, szMenuString, sizeof( szMenuString ) );
    ConvertCRtoNL( szMenuString );
	g_pLocalize->ConvertANSIToUnicode( szMenuString, g_szMenuString, sizeof( g_szMenuString ) );
	
	ProcessText();

	m_bMenuDisplayed = true;
	m_bMenuTakesInput = true;

	m_flSelectionTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMenu::ShowMenu_KeyValueItems( KeyValues *pKV )
{
	m_flShutoffTime = -1;
	m_bitsValidSlots = 0;

	GetClientMode()->GetViewportAnimationController()->StartAnimationSequence("MenuOpen");
	m_nSelectedItem = -1;
	
	g_szMenuString[0] = '\0';

	wchar_t wItem[128];

	int i = 0;
	for ( KeyValues *item = pKV->GetFirstSubKey(); item != nullptr; item = item->GetNextKey() )
	{
		// Set this slot valid
		m_bitsValidSlots |= (1<<i);

		const char *pszItem = item->GetName();
		const wchar_t *wLocalizedItem = g_pLocalize->Find( pszItem );

		V_snwprintf( wItem, sizeof( wItem )/ sizeof( wchar_t ), L"%d. %ls\n", i+1, wLocalizedItem );

		V_snwprintf( g_szMenuString, sizeof( g_szMenuString )/ sizeof( wchar_t ), L"%ls%ls", g_szMenuString, wItem );

		i++;
	}

	// put a cancel on the end
	m_bitsValidSlots |= (1<<9);

	V_snwprintf( wItem, sizeof( wItem )/ sizeof( wchar_t ), L"0. %ls", g_pLocalize->Find( "#Cancel" ) );

	V_snwprintf( g_szMenuString, sizeof( g_szMenuString )/ sizeof( wchar_t ), L"%ls\n%ls", g_szMenuString, wItem );

	ProcessText();

	m_bMenuDisplayed = true;
	m_bMenuTakesInput = true;

	m_flSelectionTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Message handler for ShowMenu message
//   takes four values:
//		short: a bitfield of keys that are valid input
//		char : the duration, in seconds, the menu should stay up. -1 means is stays until something is chosen.
//		byte : a boolean, true if there is more string yet to be received before displaying the menu, false if it's the last string
//		string: menu string to display
//  if this message is never received, then scores will simply be the combined totals of the players.
//-----------------------------------------------------------------------------
bool CHudMenu::MsgFunc_ShowMenu( const ks::net::CCSUsrMsg_ShowMenu &msg)
{
	m_bitsValidSlots = msg.bits_valid_slots;
	int DisplayTime = msg.display_time;
	
	if ( DisplayTime > 0 )
	{
		m_flShutoffTime = m_flOpenCloseTime + DisplayTime + gpGlobals->realtime;

	}
	else
	{
		m_flShutoffTime = -1;
	}

	if ( m_bitsValidSlots )
	{
		V_strncpy( g_szPrelocalisedMenuString, msg.menu_string->c_str(), sizeof( g_szPrelocalisedMenuString ) );

		GetClientMode()->GetViewportAnimationController()->StartAnimationSequence("MenuOpen");
		m_nSelectedItem = -1;
			
		// we have the whole string, so we can localise it now
		char szMenuString[MAX_MENU_STRING];
		hudtextmessage->LocaliseTextString( g_szPrelocalisedMenuString, szMenuString, sizeof( szMenuString ) );
		ConvertCRtoNL( szMenuString );
		g_pLocalize->ConvertANSIToUnicode( szMenuString, g_szMenuString, sizeof( g_szMenuString ) );
			
		ProcessText();

		m_bMenuDisplayed = true;
		m_bMenuTakesInput = true;

		m_flSelectionTime = gpGlobals->curtime;
	}
	else
	{
		HideMenu();
	}

	return true;
}

using namespace ::ks::reflect;

struct model_combine_t;
enum combine_result_t
{
	COMBINE_NOT_STARTED,
	COMBINE_IN_PROCESS,
	COMBINE_SUCCEEDED,
	COMBINE_FAILED,
};

static const char *g_pszCombineResults[] =
{
	"not started",		// COMBINE_NOT_STARTED,
	"in process",		// COMBINE_IN_PROCESS,
	"succeeded",		// COMBINE_SUCCEEDED,
	"failed"			// COMBINE_FAILED,
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

// Class that manages combining multiple models into single models
class CModelCombiner : public CAutoGameSystem
{
public:
	CModelCombiner();

	// Request a combined model to be created for the base model and the attached additional models.
	// The requester it must derive from IModelCombinerRequesterInterface. The ModelCombineFinished
	// function will be called with the result of the combine request.
	bool	CombineModel( IModelCombinerRequesterInterface *pRequester, const char *pszBaseModel, std::vector< const char * > *vecAdditionalModels );
	bool	CombineModel( IModelCombinerRequesterInterface *pRequester, const std::vector< SCombinerModelInput_t > &vecModelsToCombine );

	// Call this when you're being destroyed. It'll remove you from any pending callback lists.
	void	AbortCombineModelFor( IModelCombinerRequesterInterface *pRequester );

public:
	model_combine_t	*FindCombineRequestForIndex( int iCombineIndex );
	virtual void	LevelShutdownPostEntity( void );
	void	DumpStats( void );

private:
	void	FillOutCombineRequest( model_combine_t *pRequest, IModelCombinerRequesterInterface *pRequester, const char *pszBaseModel, std::vector< const char * > *vecAdditionalModels );
	void	FillOutCombineRequest( model_combine_t *pRequest, IModelCombinerRequesterInterface *pRequester, const std::vector< SCombinerModelInput_t > &vecModelsToCombine );
	bool	CombineRequestExists( model_combine_t *pRequest, combine_result_t *iResult, int *iIndex );
	void	AddRequesterToCallbackList( int iIndex, IModelCombinerRequesterInterface *pRequester );

private:
	std::vector< std::unique_ptr< model_combine_t > >	m_vecCombines;
	int									m_nNextCombineIndex;
};

CModelCombiner *ModelCombiner( void );

// Struct that contains the state of a single model combine request
struct model_combine_t
{
	model_combine_t& operator=( const model_combine_t& src )
	{
		if ( this == &src )
		{
			return *this;
		}

		nCombinedModelIndex = src.nCombinedModelIndex;
		vecModelsToCombine = src.vecModelsToCombine;
		pRequesterCallbackList = src.pRequesterCallbackList;
		iCombineRequestIndex = src.iCombineRequestIndex;
		iCombineResult = src.iCombineResult;

		return *this;
	}

	bool operator==( const model_combine_t &val ) const 
	{ 
		if ( std::ssize( vecModelsToCombine ) != std::ssize( val.vecModelsToCombine ) )
		{
			return false;
		}
		for ( int i = 0; i < std::ssize( vecModelsToCombine ); i++ )
		{
			if ( ( vecModelsToCombine[ i ].m_iszModelName != val.vecModelsToCombine[i].m_iszModelName ) ||
				 ( vecModelsToCombine[ i ].m_nBodyGroupSubModelSelection != val.vecModelsToCombine[i].m_nBodyGroupSubModelSelection ) )
			{
				return false;
			}
			for ( int j = 0; j < COMBINER_MAX_MATERIALS_PER_INPUT_MODEL; j++ )
			{
				for ( int k = 0; k < COMBINER_MAX_TEXTURES_PER_MATERIAL; k++ )
				{
					if ( ( vecModelsToCombine[ i ].m_textureSubstitutes[ j ][ k ].m_iszMaterialParam != val.vecModelsToCombine[ i ].m_textureSubstitutes[ j ][ k ].m_iszMaterialParam ) ||
						 ( vecModelsToCombine[ i ].m_textureSubstitutes[ j ][ k ].m_pVTFTexture != val.vecModelsToCombine[ i ].m_textureSubstitutes[ j ][ k ].m_pVTFTexture ) )
					{
						return false;
					}
				}
			}
		}

		return true; 
	}

	int														nCombinedModelIndex;
	std::vector< SCombinerModelInput_t >				vecModelsToCombine;
	std::vector< IModelCombinerRequesterInterface* >	pRequesterCallbackList;
	int														iCombineRequestIndex;
	combine_result_t										iCombineResult;
};

CModelCombiner g_ModelCombiner;
CModelCombiner* ModelCombiner() { return &g_ModelCombiner; }

// Request a combined model to be created for the base model and the attached additional models.
// The requester it must derive from IModelCombinerRequesterInterface. The ModelCombineFinished
// function will be called with the result of the combine request.
bool ModelCombiner_CombineModel( IModelCombinerRequesterInterface *pRequester, const char *pszBaseModel, std::vector< const char * > *vecAdditionalModels )
{
	std::vector< SCombinerModelInput_t > vecModelsToCombine;
	string_t iszBaseModel = AllocPooledString( pszBaseModel );

	vecModelsToCombine.push_back( SCombinerModelInput_t( iszBaseModel ) );
	for ( const char *pszAdditionalModel : *vecAdditionalModels )
	{
		string_t iszAdditionalModel = AllocPooledString( pszAdditionalModel );
		vecModelsToCombine.push_back( SCombinerModelInput_t( iszAdditionalModel ) );
	}
	return ModelCombiner()->CombineModel( pRequester, vecModelsToCombine );
}

bool ModelCombiner_CombineModel( IModelCombinerRequesterInterface *pRequester, const std::vector< SCombinerModelInput_t > &vecModelsToCombine )
{
	return ModelCombiner()->CombineModel( pRequester, vecModelsToCombine );
}

// Call this when you're being destroyed. It'll remove you from any pending callback lists.
void ModelCombiner_AbortCombineModelFor( IModelCombinerRequesterInterface *pRequester )
{
	ModelCombiner()->AbortCombineModelFor( pRequester );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CModelCombiner::CModelCombiner()
{
	m_nNextCombineIndex = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelCombiner::FillOutCombineRequest( model_combine_t *pRequest, IModelCombinerRequesterInterface *pRequester, const std::vector< SCombinerModelInput_t > &vecModelsToCombine )
{
	for ( const SCombinerModelInput_t &modelInput : vecModelsToCombine )
	{
		pRequest->vecModelsToCombine.push_back( modelInput );
	}

	pRequest->pRequesterCallbackList.push_back( pRequester );
	pRequest->iCombineRequestIndex = -1;
	pRequest->nCombinedModelIndex = 0;
	pRequest->iCombineResult = COMBINE_NOT_STARTED;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CModelCombiner::CombineRequestExists( model_combine_t *pRequest, combine_result_t *iResult, int *iIndex )
{
	*iResult = COMBINE_NOT_STARTED;
	*iIndex = -1;

	for ( int i = 0; i < std::ssize( m_vecCombines ); i++ )
	{
		if ( *m_vecCombines[i] == *pRequest )
		{
			*iResult = m_vecCombines[i]->iCombineResult;
			*iIndex = i;
			break;
		}
	}

	return (*iIndex != -1);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelCombiner::AddRequesterToCallbackList( int iIndex, IModelCombinerRequesterInterface *pRequester )
{
	for ( IModelCombinerRequesterInterface *pCallbackRequester : m_vecCombines[iIndex]->pRequesterCallbackList )
	{
		if ( pCallbackRequester == pRequester )
			return;
	}

	m_vecCombines[iIndex]->pRequesterCallbackList.push_back( pRequester );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
model_combine_t	*CModelCombiner::FindCombineRequestForIndex( int iCombineIndex )
{
	for ( auto &pCombine : m_vecCombines )
	{
		if ( pCombine->iCombineRequestIndex == iCombineIndex )
			return pCombine.get();
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelCombiner::LevelShutdownPostEntity( void )
{
	// Release all the combined models.
	for ( auto &pCombine : m_vecCombines )
	{
		modelinfo->ReleaseDynamicModel( pCombine->nCombinedModelIndex );
	}

	m_vecCombines.clear();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ModelCombineFinished( void *pUserData, MDLHandle_t OldHandle, MDLHandle_t NewHandle, TCombinedResults &CombinedResults )
{
	int *pCombineIndex = (int *)pUserData;
	if ( !pCombineIndex )
		return;

	model_combine_t	*pCombine = ModelCombiner()->FindCombineRequestForIndex( *pCombineIndex );
	if ( !pCombine )
	{
		Warning("MODELCOMBINER: Received a ModelCombineFinished call with no matching combine request (%d)\n", *pCombineIndex );
		return;
	}

	bool bSucceeded = ( CombinedResults.m_nCombinedResults == COMBINE_RESULT_FLAG_OK );
	if ( bSucceeded )
	{
		DevMsg("MODELCOMBINER: Finishing building combined model for %s.\n", STRING( pCombine->vecModelsToCombine[ 0 ].m_iszModelName ) );
		pCombine->iCombineResult = COMBINE_SUCCEEDED;
		modelinfo->UpdateCombinedDynamicModel( pCombine->nCombinedModelIndex, NewHandle );
	}
	else
	{
		DevMsg("MODELCOMBINER: Failed to build combined model for %s: Error %s (%d)\n", STRING( pCombine->vecModelsToCombine[ 0 ].m_iszModelName ), CombinedResults.m_szErrorMessage, CombinedResults.m_nCombinedResults );
		pCombine->iCombineResult = COMBINE_FAILED;
		modelinfo->ReleaseDynamicModel( pCombine->nCombinedModelIndex );
	}

	// Fire off the callbacks for every requester that wanted to use their combined model
	for ( int i = 0; i < std::ssize( pCombine->pRequesterCallbackList ); i++ )
	{
		pCombine->pRequesterCallbackList[i]->ModelCombineFinished( bSucceeded, pCombine->nCombinedModelIndex );
	}

	pCombine->pRequesterCallbackList.clear(); pCombine->pRequesterCallbackList.shrink_to_fit();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CModelCombiner::CombineModel( IModelCombinerRequesterInterface *pRequester, const std::vector< SCombinerModelInput_t > &vecModelsToCombine )
{
	contract_assert( std::ssize( vecModelsToCombine ) > 1 );

	auto pOwnedCombine = std::make_unique< model_combine_t >();
	model_combine_t	*pCombine = pOwnedCombine.get();
	FillOutCombineRequest( pCombine, pRequester, vecModelsToCombine );

	DevMsg("MODELCOMBINER: Received combine request for:\n");
	for ( const SCombinerModelInput_t &modelInput : vecModelsToCombine )
	{
		DevMsg("   -> %s\n", STRING( modelInput.m_iszModelName ) );
	}
	DevMsg("  %d models.\n", (int)std::ssize( vecModelsToCombine ) );

	// Have we already tried combining this set of models?
	int iExistingIndex;
	combine_result_t iExistingState;
	if ( CombineRequestExists( pCombine, &iExistingState, &iExistingIndex ) )
	{
		DevMsg("  FOUND existing combine in state: %d\n", iExistingState );

		if ( iExistingState == COMBINE_IN_PROCESS )
		{
			// We're still combining this set. Add ourselves to the callback list.
			AddRequesterToCallbackList( iExistingIndex, pRequester );

			DevMsg("  ADDING to the in-progress callback list.\n" );
		}
		else if ( iExistingState == COMBINE_SUCCEEDED )
		{
			// Tell the requester the combined model is already available.
			pRequester->ModelCombineFinished( true, m_vecCombines[iExistingIndex]->nCombinedModelIndex );
			DevMsg("  RE-USING existing combined model (%d).\n", m_vecCombines[iExistingIndex]->nCombinedModelIndex );
		}

		return ( ( iExistingState == COMBINE_SUCCEEDED ) || ( iExistingState == COMBINE_IN_PROCESS ) );
	}

	// New combine. Add it to our list.
	m_vecCombines.push_back( std::move( pOwnedCombine ) );
	pCombine->iCombineRequestIndex = m_nNextCombineIndex++;
	pCombine->iCombineResult = COMBINE_FAILED;

	// Build our unique combined model name.
	char szCombinedModelName[ 256 ];
	V_sprintf_safe( szCombinedModelName, "%s_c_%d", STRING( vecModelsToCombine[ 0 ].m_iszModelName ), pCombine->iCombineRequestIndex );

	pCombine->nCombinedModelIndex = modelinfo->BeginCombinedModel( szCombinedModelName, true );
	if ( pCombine->nCombinedModelIndex == -1 )
	{
		contract_assert( false && "Failed to combine model %s" );
		return false;
	}

	if ( !modelinfo->SetCombineModels( pCombine->nCombinedModelIndex, vecModelsToCombine ) )
	{	
		// we failed - invalid handle?
		contract_assert( "Failed to set combined models for %s!" );
		return false;
	}

	DevMsg("  STARTED Combining: final model index will be %d\n", pCombine->nCombinedModelIndex );

	pCombine->iCombineResult = COMBINE_IN_PROCESS;
	modelinfo->FinishCombinedModel( pCombine->nCombinedModelIndex, &ModelCombineFinished, &pCombine->iCombineRequestIndex );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelCombiner::AbortCombineModelFor( IModelCombinerRequesterInterface *pRequester )
{
	for ( auto &pCombine : m_vecCombines )
	{
		for ( int j = (int)std::ssize( pCombine->pRequesterCallbackList ) - 1; j >= 0; j-- )
		{
			if ( pCombine->pRequesterCallbackList[ j ]  == pRequester )
			{
				pCombine->pRequesterCallbackList.erase( pCombine->pRequesterCallbackList.begin() + j );

				DevMsg("MODELCOMBINER: Removing requester from callback for %s\n", STRING( pCombine->vecModelsToCombine[ 0 ].m_iszModelName ) );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelCombiner::DumpStats( void )
{
	Msg("Model Combiner State\n  %d combined models.\n", (int)std::ssize( m_vecCombines ));
	for ( int i = 0; i < std::ssize( m_vecCombines ); i++ )
	{
		Msg("  %d: Base model %s  (%d attached models)\n", i, STRING( m_vecCombines[ i ]->vecModelsToCombine[ 0 ].m_iszModelName ), (int)std::ssize( m_vecCombines[ i ]->vecModelsToCombine ) - 1 );

		for ( int j = 0; j < std::ssize( m_vecCombines[ i ]->vecModelsToCombine ); j++ )
		{
			if ( j > 0 )
			{
				Msg("       -> %s\n", STRING( m_vecCombines[ i ]->vecModelsToCombine[ j ].m_iszModelName ) );
			}
		}

		Msg("     Combine result:        %s\n", g_pszCombineResults[ m_vecCombines[ i ]->iCombineResult ] );
		Msg("     Combine model index:   %d\n", m_vecCombines[ i ]->nCombinedModelIndex );
		Msg("     Combine request index: %d\n", m_vecCombines[ i ]->iCombineRequestIndex );
		Msg("     Number of requesters:  %d\n", (int)std::ssize( m_vecCombines[ i ]->pRequesterCallbackList ) );
	}
}
