// SPDX-License-Identifier: None
// Copyright (c) 2007 Turtle Rock Studios, Inc.

module;
#include "client_class.h"
#include "declare_class.h"
#include "reflect_annotations.h"
#include "predictable_entity.h"
module ks.client:basetoggle;

import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import :baseentity;
import :particlemgr;
import :util;
import std;
import ks.core.vprof;
import ks.math.mathlib;
import ks.math.ssemath;
import :baseanimatedtextureproxy;
import ks.game.shareddefs;

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;


//--------------------------------------------------------------------------------------------------------

//copy/paste from server subs.cpp
enum togglemovetypes_t
{
	MOVE_TOGGLE_NONE = 0,
	MOVE_TOGGLE_LINEAR = 1,
	MOVE_TOGGLE_ANGULAR = 2,
};

//--------------------------------------------------------------------------------------------------------
class CBaseToggle: public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseToggle" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CBaseToggle, CBaseEntity );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE(); //elevators

	virtual void GetGroundVelocityToApply( Vector &vecGroundVel );
	virtual bool ShouldPredict( void );
	virtual CBasePlayer *GetPredictionOwner( void );
	virtual void PhysicsSimulate( void );
	virtual bool PredictionErrorShouldResetLatchedForAllPredictables( void ) { return false; }
	virtual bool PredictionIsPhysicallySimulated( void );
	virtual void PostDataUpdate( DataUpdateType_t updateType );

	Vector PredictPosition( float fCurTime );

	EHANDLE m_hPredictionOwner;
	Vector m_vLastNetworked;
	float m_fLastNetworkedTime;
	
	[[= ks::reflect::Net{} ]] Vector m_vecFinalDest;
	[[= ks::reflect::Net{} ]] int m_movementType;
	[[= ks::reflect::Net{} ]] float m_flMoveTargetTime;
	bool m_bWasPredictingMotion;
};

//--------------------------------------------------------------------------------------------------------
class CBaseButton: public CBaseToggle
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseButton" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CBaseButton, CBaseToggle );
	DECLARE_CLIENTCLASS();

	CBaseButton()
	{
	}

	virtual bool IsPotentiallyUsable( void );

private:
	[[= ks::reflect::Net{} ]] bool m_usable;
};

//--------------------------------------------------------------------------------------------------------
//
//	CButtonTimed - func_button_timed
//
//--------------------------------------------------------------------------------------------------------
class C_ButtonTimed;   // declared only: it was never implemented

using namespace ::ks::reflect;

//------------------------------------------------------------------------------
// Purpose : Ambient light controller entity with radial falloff
//------------------------------------------------------------------------------
class CEnvAmbientLight : public CSpatialEntityTemplate<Vector>
{
	[[= ks::reflect::NetTable{ .name = "DT_EnvAmbientLight" } ]]
	[[= ks::reflect::From<"m_Value", ks::reflect::Net{ .wire = "m_vecColor" }>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CEnvAmbientLight, CSpatialEntityTemplate<Vector> );
	DECLARE_CLIENTCLASS();

	virtual void ApplyAccumulation( void );
	virtual void ClientThink( void );

	void SetColor( const Vector &vecColor, float flLerpTime = 0 );
	Vector GetTargetColor() { return m_vecTargetColor * 255.0f; }

protected:
	virtual void AddToPersonalSpatialEntityMgr( void );
	virtual void RemoveFromPersonalSpatialEntityMgr( void );

	Vector m_vecStartColor;
	Vector m_vecTargetColor;
	CountdownTimer m_colorTimer;
};

class CBaseCombatCharacter;

using namespace ::ks::reflect;

// SharedFunctorUtils.h
// Useful functors for client and server
// Author: Graham Smallwood, August 2006, Copyright 2006 Turtle Rock Studios, Inc.

//--------------------------------------------------------------------------------------------------------
/**
* NOTE: The functors in this file should ideally be game-independant, 
* and work for any Source based game
*/
//--------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------
/**
 * Finds visible player on given team that we are pointing at.
 * "team" can be TEAM_ANY 
 * Use with ForEachPlayer()
 */
template < class PlayerType >
class TargetScan
{
public:
	TargetScan( PlayerType *me, int team, float aimTolerance = 0.01f, float maxRange = 2000.0f, float closestPointTestDistance = 50.0f, bool debug = false )
	{
		m_me = me;
		AngleVectors( m_me->EyeAngles(), &m_viewForward );
		m_team = team;
		m_closeDot = 1.0f - aimTolerance;
		m_bestDot = m_closeDot;
		m_maxRange = maxRange;
		m_target = nullptr;
		m_closestPointTestDistance = closestPointTestDistance;
		m_debug = debug;
	}

	virtual bool operator() ( PlayerType *them )
	{
		CVProfScope _( "TargetScan()", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
		if ( them != m_me && 
			 them->IsAlive() && 
			 (m_team == TEAM_ANY || them->GetTeamNumber() == m_team) &&
			 IsPotentialTarget( them ) )
		{
			// move the start point out for determining closestPos, to help with close-in checks (healing, etc)
			Vector closestPos;
			Vector start = m_me->EyePosition();
			Vector end = start + m_viewForward * m_closestPointTestDistance;
			Vector testPos;
			CalcClosestPointOnLineSegment( them->WorldSpaceCenter(), start, end, testPos );

			start = them->GetAbsOrigin();
			end = start;
			end.z += them->CollisionProp()->OBBMaxs().z;
			CalcClosestPointOnLineSegment( testPos, start, end, closestPos );
			if ( m_debug )
			{
				NDebugOverlay::Cross3D( closestPos, 1, 255, 255, 255, true, -1.0f );
				NDebugOverlay::Line( end, start, 255, 0, 0, true, -1.0f );
			}

			Vector to = closestPos - m_me->EyePosition();
			to.NormalizeInPlace();

			Vector meRangePoint, themRangePoint;
			m_me->CollisionProp()->CalcNearestPoint( closestPos, &meRangePoint );
			them->CollisionProp()->CalcNearestPoint( meRangePoint, &themRangePoint );
			float range = meRangePoint.DistTo( themRangePoint );

			if ( range > m_maxRange )
			{
				// too far away
				return true;
			}

			float dot = ViewDot( to );
			if ( dot > m_closeDot )
			{
				// target is within angle cone, check visibility
				if ( IsTargetVisible( them ) )
				{
					if ( dot >= m_bestDot )
					{
						m_target = them;
						m_bestDot = dot;
					}

					m_allTargets.AddToTail( them );
				}
			}
		}

		return true;
	}

	PlayerType *GetTarget( void ) const
	{
		return m_target;
	}

	const std::vector< PlayerType * > &GetAllTargets( void ) const
	{
		return m_allTargets;
	}

	float GetTargetDot( void ) const
	{
		return m_bestDot;
	}

protected:
	/**
	 * Is the point in our FOV?
	 */
	virtual float ViewDot( const Vector &dir ) const
	{
		return DotProduct( m_viewForward, dir );
	}

	/**
	 * Is the given actor a visible target?
	 */
	virtual bool IsTargetVisible( PlayerType *them ) const
	{
		// The default check is a straight-up IsAbleToSee
		return m_me->IsAbleToSee( them, CBaseCombatCharacter::DISREGARD_FOV ); // already have a dot product checking FOV
	}

	/**
	 * Is the given player a possible target at all?
	 */
	virtual bool IsPotentialTarget( PlayerType *them ) const
	{
		return true;
	}

	PlayerType *m_me;	
	Vector m_viewForward;
	int m_team;

	float m_closeDot;
	float m_bestDot;
	float m_maxRange;
	float m_closestPointTestDistance;
	bool m_debug;

	PlayerType *m_target;
	std::vector< PlayerType * > m_allTargets;
};

unsigned char g_Script_vscript_client[402] = {
  0x2f, 0x2f, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d,
  0x20, 0x43, 0x6f, 0x70, 0x79, 0x72, 0x69, 0x67, 0x68, 0x74, 0x20, 0xa9,
  0x20, 0x32, 0x30, 0x30, 0x38, 0x2c, 0x20, 0x56, 0x61, 0x6c, 0x76, 0x65,
  0x20, 0x43, 0x6f, 0x72, 0x70, 0x6f, 0x72, 0x61, 0x74, 0x69, 0x6f, 0x6e,
  0x2c, 0x20, 0x41, 0x6c, 0x6c, 0x20, 0x72, 0x69, 0x67, 0x68, 0x74, 0x73,
  0x20, 0x72, 0x65, 0x73, 0x65, 0x72, 0x76, 0x65, 0x64, 0x2e, 0x20, 0x3d,
  0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x0a, 0x2f, 0x2f, 0x0a, 0x2f,
  0x2f, 0x20, 0x50, 0x75, 0x72, 0x70, 0x6f, 0x73, 0x65, 0x3a, 0x0a, 0x2f,
  0x2f, 0x0a, 0x2f, 0x2f, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d,
  0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d,
  0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d,
  0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d,
  0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d,
  0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d,
  0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x0a, 0x0a, 0x66,
  0x75, 0x6e, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x20, 0x55, 0x6e, 0x69, 0x71,
  0x75, 0x65, 0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x28, 0x20, 0x73, 0x74,
  0x72, 0x69, 0x6e, 0x67, 0x20, 0x3d, 0x20, 0x22, 0x22, 0x20, 0x29, 0x0a,
  0x7b, 0x0a, 0x09, 0x72, 0x65, 0x74, 0x75, 0x72, 0x6e, 0x20, 0x44, 0x6f,
  0x55, 0x6e, 0x69, 0x71, 0x75, 0x65, 0x53, 0x74, 0x72, 0x69, 0x6e, 0x67,
  0x28, 0x20, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x2e, 0x74, 0x6f, 0x73,
  0x74, 0x72, 0x69, 0x6e, 0x67, 0x28, 0x29, 0x20, 0x29, 0x3b, 0x0a, 0x7d,
  0x0a, 0x0a, 0x66, 0x75, 0x6e, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x20, 0x49,
  0x6e, 0x63, 0x6c, 0x75, 0x64, 0x65, 0x53, 0x63, 0x72, 0x69, 0x70, 0x74,
  0x28, 0x20, 0x6e, 0x61, 0x6d, 0x65, 0x2c, 0x20, 0x73, 0x63, 0x6f, 0x70,
  0x65, 0x20, 0x3d, 0x20, 0x6e, 0x75, 0x6c, 0x6c, 0x20, 0x29, 0x0a, 0x7b,
  0x0a, 0x09, 0x69, 0x66, 0x20, 0x28, 0x20, 0x73, 0x63, 0x6f, 0x70, 0x65,
  0x20, 0x3d, 0x3d, 0x20, 0x6e, 0x75, 0x6c, 0x6c, 0x20, 0x29, 0x0a, 0x09,
  0x7b, 0x0a, 0x09, 0x09, 0x73, 0x63, 0x6f, 0x70, 0x65, 0x20, 0x3d, 0x20,
  0x74, 0x68, 0x69, 0x73, 0x3b, 0x0a, 0x09, 0x7d, 0x0a, 0x09, 0x72, 0x65,
  0x74, 0x75, 0x72, 0x6e, 0x20, 0x3a, 0x3a, 0x44, 0x6f, 0x49, 0x6e, 0x63,
  0x6c, 0x75, 0x64, 0x65, 0x53, 0x63, 0x72, 0x69, 0x70, 0x74, 0x28, 0x20,
  0x6e, 0x61, 0x6d, 0x65, 0x2c, 0x20, 0x73, 0x63, 0x6f, 0x70, 0x65, 0x20,
  0x29, 0x3b, 0x0a, 0x7d, 0x0a, 0x00
};
