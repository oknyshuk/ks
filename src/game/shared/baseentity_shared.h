#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "shareddefs.h"
import std;

#ifndef CLIENT_DLL
#define SERVER_PLATTIME_RNG true
#else
#define SERVER_PLATTIME_RNG
#endif

extern ConVar hl2_episodic;

// Simple shared header file for common base entities

// How many bits are used to transmit parent attachment indices?
#define NUM_PARENTATTACHMENT_BITS	6

// Maximum number of vphysics objects per entity
inline constexpr auto VPHYSICS_MAX_OBJECT_LIST_COUNT = 1024;

inline constexpr auto DEFAULT_LOOK_AT_USE_ANGLE = 0.8f;

#if defined( CLIENT_DLL )
#include "c_baseentity.h"
#include "c_baseanimating.h"
#else
#include "baseentity.h"
#include "player.h"
#endif

#include "vscript/ivscript.h"
#include "vscript_shared.h"

// CBaseEntity inlines
inline bool CBaseEntity::IsPlayerSimulated( void ) const
{
	return m_bIsPlayerSimulated;
}

inline CBasePlayer *CBaseEntity::GetSimulatingPlayer( void )
{
	return m_hPlayerSimulationOwner.Get();
}

inline MoveType_t CBaseEntity::GetMoveType() const
{
	return (MoveType_t)(unsigned char)m_MoveType;
}

inline MoveCollide_t CBaseEntity::GetMoveCollide() const
{
	return (MoveCollide_t)(unsigned char)m_MoveCollide;
}

//-----------------------------------------------------------------------------
// Collision group accessors
//-----------------------------------------------------------------------------
inline int CBaseEntity::GetCollisionGroup() const
{
	return m_CollisionGroup;
}

inline int	CBaseEntity::GetFlags( void ) const
{
	return m_fFlags;
}

inline bool CBaseEntity::IsAlive( void )const
{
	return m_lifeState == LIFE_ALIVE; 
}

inline CBaseEntity	*CBaseEntity::GetOwnerEntity() const
{
	return m_hOwnerEntity.Get();
}

inline CBaseEntity	*CBaseEntity::GetEffectEntity() const
{
	return m_hEffectEntity.Get();
}

#ifdef CLIENT_DLL
inline int CBaseEntity::GetPredictionRandomSeed( void )
{
	return m_nPredictionRandomSeed;
}
#else
inline int CBaseEntity::GetPredictionRandomSeed( bool bUseUnSyncedServerPlatTime )
{
	return bUseUnSyncedServerPlatTime ? m_nPredictionRandomSeedServer : m_nPredictionRandomSeed;
}
#endif

inline CBasePlayer *CBaseEntity::GetPredictionPlayer( void )
{
	return m_pPredictionPlayer;
}

inline void CBaseEntity::SetPredictionPlayer( CBasePlayer *player )
{
	m_pPredictionPlayer = player;
}


inline bool CBaseEntity::IsSimulatedEveryTick() const
{
	return m_bSimulatedEveryTick;
}

inline bool CBaseEntity::IsAnimatedEveryTick() const
{
	return m_bAnimatedEveryTick;
}

inline void CBaseEntity::SetSimulatedEveryTick( bool sim )
{
	if ( m_bSimulatedEveryTick != sim )
	{
		m_bSimulatedEveryTick = sim;
#ifdef CLIENT_DLL
		Interp_UpdateInterpolationAmounts( GetVarMapping() );
#endif
	}
}

inline void CBaseEntity::SetAnimatedEveryTick( bool anim )
{
	if ( m_bAnimatedEveryTick != anim )
	{
		m_bAnimatedEveryTick = anim;
#ifdef CLIENT_DLL
		Interp_UpdateInterpolationAmounts( GetVarMapping() );
#endif
	}
}

inline float CBaseEntity::GetAnimTime() const
{
	return m_flAnimTime;
}

inline float CBaseEntity::GetSimulationTime() const
{
	return m_flSimulationTime;
}

inline void CBaseEntity::SetAnimTime( float at )
{
	m_flAnimTime = at;
}

inline void CBaseEntity::SetSimulationTime( float st )
{
	m_flSimulationTime = st;
}

inline int CBaseEntity::GetEffects( void ) const
{ 
	return m_fEffects; 
}

inline void CBaseEntity::RemoveEffects( int nEffects ) 
{ 

	m_fEffects &= ~nEffects;
	if ( nEffects & EF_NODRAW )
	{
#ifndef CLIENT_DLL
		NetworkProp()->MarkPVSInformationDirty();
		DispatchUpdateTransmitState();
#else
		UpdateVisibility();
#endif
	}

#ifdef CLIENT_DLL
	if ( nEffects & EF_MARKED_FOR_FAST_REFLECTION )
	{
		OnFastReflectionRenderingChanged();
	}
	OnDisableShadowDepthRenderingChanged();
	OnDisableCSMRenderingChanged();
	OnShadowDepthRenderingCacheableStateChanged();
#endif
}

inline void CBaseEntity::ClearEffects( void ) 
{ 

#ifdef CLIENT_DLL
	bool bRendersInFastReflection = ( m_fEffects & EF_MARKED_FOR_FAST_REFLECTION ) != 0;
#endif

	m_fEffects = 0;

#ifndef CLIENT_DLL
	DispatchUpdateTransmitState();
#else
	UpdateVisibility();
	if ( bRendersInFastReflection )
	{
		OnFastReflectionRenderingChanged();
	}
	OnDisableShadowDepthRenderingChanged();
	OnDisableCSMRenderingChanged();
	OnShadowDepthRenderingCacheableStateChanged();
#endif
}

inline bool CBaseEntity::IsEffectActive( int nEffects ) const
{ 
	return (m_fEffects & nEffects) != 0; 
}


inline HSCRIPT ToHScript( CBaseEntity *pEnt )
{
	return ( pEnt ) ? pEnt->GetScriptInstance() : nullptr;
}

template <> ScriptClassDesc_t *GetScriptDesc<CBaseEntity>( CBaseEntity * );
inline CBaseEntity *ToEnt( HSCRIPT hScript )
{

	return ( hScript ) ? (CBaseEntity *)g_pScriptVM->GetInstanceValue( hScript, GetScriptDescForClass(CBaseEntity) ) : nullptr;
}

// convenience functions for fishing out the vectors of this object
// equivalent to GetVectors(), but doesn't need an intermediate stack 
// variable (which might cause an LHS anyway)
inline Vector	CBaseEntity::Forward() const __restrict__  ///< get my forward (+x) vector
{
	const matrix3x4_t &mat = EntityToWorldTransform();
	return Vector( mat[0][0], mat[1][0], mat[2][0] );
}

inline Vector	CBaseEntity::Left() const __restrict__     ///< get my left    (+y) vector
{
	const matrix3x4_t &mat = EntityToWorldTransform();
	return Vector( mat[0][1], mat[1][1], mat[2][1] );
}

inline Vector	CBaseEntity::Up() const  __restrict__      ///< get my up      (+z) vector
{
	const matrix3x4_t &mat = EntityToWorldTransform();
	return Vector( mat[0][2], mat[1][2], mat[2][2] );
}

// Shared EntityMessage between game and client .dlls
inline constexpr auto BASEENTITY_MSG_REMOVE_DECALS = 1;

inline bool IsPushableMoveType( int nMoveType )
{
	if ( nMoveType == MOVETYPE_PUSH || nMoveType == MOVETYPE_NONE || 
		nMoveType == MOVETYPE_VPHYSICS || nMoveType == MOVETYPE_NOCLIP )
		return false;
	return true;
}

extern float k_flMaxEntityPosCoord;
extern float k_flMaxEntityEulerAngle;
extern float k_flMaxEntitySpeed;
extern float k_flMaxEntitySpinRate;

inline bool IsEntityCoordinateReasonable( const vec_t c )
{
	float r = k_flMaxEntityPosCoord;
	return c > -r && c < r;
}

inline bool IsEntityPositionReasonable( const Vector &v )
{
	float r = k_flMaxEntityPosCoord;
	return
		v.x > -r && v.x < r &&
		v.y > -r && v.y < r &&
		v.z > -r && v.z < r;
}

// Returns:
//   -1 - velocity is really, REALLY bad and probably should be rejected.
//   0  - velocity was suspicious and clamped.
//   1  - velocity was OK and not modified
extern int CheckEntityVelocity( Vector &v );

inline bool IsEntityQAngleReasonable( const QAngle &q )
{
	float r = k_flMaxEntityEulerAngle;
	return
		q.x > -r && q.x < r &&
		q.y > -r && q.y < r &&
		q.z > -r && q.z < r;
}

// Angular velocity in exponential map form
inline bool IsEntityAngularVelocityReasonable( const Vector &q )
{
	float r = k_flMaxEntitySpinRate;
	return
		q.x > -r && q.x < r &&
		q.y > -r && q.y < r &&
		q.z > -r && q.z < r;
}

// Angular velocity of each Euler angle.
inline bool IsEntityQAngleVelReasonable( const QAngle &q )
{
	float r = k_flMaxEntitySpinRate;
	return
		q.x > -r && q.x < r &&
		q.y > -r && q.y < r &&
		q.z > -r && q.z < r;
}

// Should we emit physics spew into the log or not?
extern bool CheckEmitReasonablePhysicsSpew();
