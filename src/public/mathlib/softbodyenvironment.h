#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: A small subset of CRnWorld
// Note: mathlib is tentative place for this code. We will probably move it to a separate lib or dll or vphysics.dll

import ks.math;
#include "rubikon/param_types.h"
import std;
#include "mathlib/softbody.h"
#include "rubikon/intersection.h"
#include "mathlib/dynamictree.h"

class CDynamicTree;

class CSoftbodyCollisionSphere;
class CSoftbodyCollisionCapsule;


class CSoftbodyCollisionFilter
{
public:
	CSoftbodyCollisionFilter();

	void InitGroup( int nGroup, CollisionGroupPairFlags defaultFlags = 0 );
	uint16 TestSimulation( const RnCollisionAttr_t &left, const RnCollisionAttr_t &right )const;


public:
	enum ConstEnum_t{ MAX_GROUPS = COLLISION_GROUPS_MAX_ALLOWED };
	CollisionGroupPairFlags m_GroupPairs[ MAX_GROUPS ][ MAX_GROUPS ];
};


class CSoftbodyCollisionShape
{
public:
	CSoftbodyCollisionShape( PhysicsShapeType_t type ) : m_nType( type ), m_nProxyId( -1 ) {}
// 	CSoftbodyCollisionSphere *IsSphere();
// 	CSoftbodyCollisionCapsule *IsCapsule();

	// Shape type
	PhysicsShapeType_t GetType( void ) const { return m_nType; }
	const RnCollisionAttr_t &GetCollisionAttributes( void ) const { return m_CollisionAttr; }
	RnCollisionAttr_t &GetCollisionAttributes( void ) { return m_CollisionAttr; }
	int32 GetProxyId() const { return m_nProxyId; }
	void SetProxyId( int32 nProxyId ){ m_nProxyId = nProxyId; }

	AABB_t GetBbox()const;
protected:
	RnCollisionAttr_t m_CollisionAttr;
	PhysicsShapeType_t m_nType;	 // not really necessary..

	int32 m_nProxyId;
};

class CSoftbodyCollisionSphere: public CSoftbodyCollisionShape
{
public:
	CSoftbodyCollisionSphere() : CSoftbodyCollisionShape( SHAPE_SPHERE ){}
	void SetRadius( float flRadius ) { m_flRadius = flRadius; }
	float GetRadius() const { return m_flRadius; }

	void SetCenter( const Vector &vCenter ){ m_vCenter = vCenter; }
	const Vector &GetCenter()const { return m_vCenter; }
	AABB_t GetBbox()const;
protected:
	Vector m_vCenter;
	float m_flRadius;
};

class CSoftbodyCollisionCapsule : public CSoftbodyCollisionShape
{
public:
	CSoftbodyCollisionCapsule() : CSoftbodyCollisionShape( SHAPE_CAPSULE ) {}
	void SetRadius( float flRadius ) { m_flRadius = flRadius; }
	float GetRadius() const { return m_flRadius; }

	void SetCenter( int nIndex, const Vector &vCenter ){ m_vCenter[ nIndex ] = vCenter; }
	const Vector &GetCenter( int nIndex )const { return m_vCenter[ nIndex ]; }
	AABB_t GetBbox()const;
protected:
	Vector m_vCenter[2];
	float m_flRadius;
};

class CSoftbodyEnvironment
{
public:
	CSoftbodyEnvironment();

	uint GetSoftbodySimulationFlags()const { return 0; }
	uint GetSoftbodyIterations() const { return m_nIterations; }
	void SetSoftbodyIterations( int nIterations ) { m_nIterations = nIterations; }
	CDynamicTree *GetBroadphaseTree() { return &m_BroadphaseTree; }

	const VectorAligned &GetGravity()const { return m_vGravity; }
	CDebugHighlightCone& GetDebugHighlightCone() { return m_DebugHighlightCone; }
	
	void Register( CSoftbody *pSoftbody );
	void Unregister( CSoftbody *pSoftbody );
	void Step( float dt, float flSubstepDt = 1.0f / 60.0f, int nMaxSubsteps = 3 );

	int GetSoftbodyCount() const { return std::ssize( m_Softbodies ); }
	CSoftbody* GetSoftbody( int i ) { return m_Softbodies[ i ]; }

	void Add( CSoftbodyCollisionShape * pShape );
	void AddOrUpdate( CSoftbodyCollisionShape * pShape );
	void Update( CSoftbodyCollisionShape * pShape );
	void Remove( CSoftbodyCollisionShape * pShape );
	const Vector4DAligned &GetWindDesc() const { return m_vWindDesc; }
	void SetWind( const Vector & vWind );
	void SetWindDesc( const Vector &vWindDir, float flStrength ) { m_vWindDesc.Init( vWindDir, flStrength ); }
	void SetNoWind() { m_vWindDesc.Init( 1, 0, 0, 0 ); }
public:
	CSoftbodyCollisionFilter m_Filter;
protected:
	Vector4DAligned m_vWindDesc; // normalized direction in x,y,z and strength in w
	VectorAligned m_vGravity;
	CDynamicTree m_BroadphaseTree;
	int m_nIterations;
	CDebugHighlightCone m_DebugHighlightCone;
	std::vector< CSoftbody* > m_Softbodies;	// not owned; each body stores its slot (CWorldIndexPred) for O(1) unregister
	float m_flAccumulatedTimeSlack;
};

inline CSoftbodyEnvironment::CSoftbodyEnvironment()
{
	SetNoWind();
	m_flAccumulatedTimeSlack = 0.0f;
	m_nIterations = 1;
	m_vGravity.Init( 0, 0, -360 );
}

inline void CSoftbodyEnvironment::Register( CSoftbody *pSoftbody )
{
	CSoftbody::CWorldIndexPred::SetIndex( pSoftbody, std::ssize( m_Softbodies ) );
	m_Softbodies.push_back( pSoftbody );
}

// doesn't preserve order: the last body takes the removed one's slot
inline void CSoftbodyEnvironment::Unregister( CSoftbody *pSoftbody )
{
	int nIndex = CSoftbody::CWorldIndexPred::GetIndex( pSoftbody );
	contract_assert( m_Softbodies[ nIndex ] == pSoftbody );
	CSoftbody *pLast = m_Softbodies.back();
	m_Softbodies[ nIndex ] = pLast;
	m_Softbodies.pop_back();
	CSoftbody::CWorldIndexPred::SetIndex( pLast, nIndex );
}


inline void CSoftbodyEnvironment::Step( float dt, float flSubstepDt, int nMaxSubsteps )
{
	m_flAccumulatedTimeSlack += dt;
	if ( m_flAccumulatedTimeSlack < flSubstepDt )
		return;
	float flSubsteps = m_flAccumulatedTimeSlack / flSubstepDt;
	int nSubsteps = int( flSubsteps );
	if ( nSubsteps < nMaxSubsteps )
	{
		m_flAccumulatedTimeSlack = m_flAccumulatedTimeSlack - std::floorf( flSubsteps * flSubstepDt );
	}
	else
	{
		nSubsteps = nMaxSubsteps;
		m_flAccumulatedTimeSlack = 0.0f;
	}
	for ( int i = 0; i < std::ssize( m_Softbodies ); ++i )
	{
		CSoftbody *pSoftbody = m_Softbodies[ i ];
		for ( int j = 0; j < nSubsteps; ++j )
		{
			pSoftbody->Step( flSubstepDt );
		}
	}
}

inline AABB_t CSoftbodyCollisionShape::GetBbox()const
{
	switch ( m_nType )
	{
	default:
		contract_assert( m_nType == SHAPE_SPHERE );
		return static_cast< const CSoftbodyCollisionSphere* >( this )->GetBbox();
	case SHAPE_CAPSULE:
		return static_cast< const CSoftbodyCollisionCapsule* >( this )->GetBbox();
	}
}
