// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "bspflags.h"
#include "const.h"
#include "vphysics_interface.h"
#include "client_class.h"
#include "declare_class.h"
#include "reflect_annotations.h"
#include "reflect_recvtable.h"
#include "irecipientfilter.h"
#include "particles/particles.h"
#include "string_t.h"
#include "dt_recv.h"
#include "edict.h"
#include "iclientrenderable.h"
#include "cdll_client_int.h"
#include "reflect_predmap.h"
#include "networkvar.h"
#include "predictable_entity.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialproxy.h"
#include "materialsystem/imaterialvar.h"
#include "imaterialproxydict.h"
#include "dlight.h"
#include "iefx.h"
#include "smoke_fog_overlay.h"
#include "datacache/iprecachesystem.h"
#include "te_effect_dispatch.h"
module ks.client;

import ks.core.dbg;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :sharedvar;
import :util;
import :particlemgr;
import :precache;
import :baseentity;
import :achievements_and_stats_interface;
import ks.game.gamemovement;
import std;
import ks.core.random;
import ks.core.vprof;
import ks.tier1.convar;
import :noopanimcontroller;
import ks.game.mapentities_shared;
import ks.game.shareddefs;
import ks.math.vmatrix;
import ks.tier1.createinterface;
import ks.tier1.registry;
import ks.tier1.strtools;
import :cstrike15.cs_player;
import ks.tier1.intrusive_ptr;
import ks.tier1.keyvalues;
import ks.core.basetypes;
import ks.core.platform;
import ks.tier1.exprevaluator;
import ks.tier1.vector_ops;
import :cstrike15.cs_player_shared;
import ks.core.types;
import ks.tier1.fmtstr;
import ks.tier1.interfaces;
import ks.tier1.tracked_ref;

using namespace ::ks::reflect;

// Purpose: Local fast collision system for particles

#define	__DEBUG_PARTICLE_COLLISION_RETEST			1
#define	__DEBUG_PARTICLE_COLLISION_OVERLAY			0
static constexpr auto __DEBUG_PARTICLE_COLLISION_OVERLAY_LIFETIME = 0.1f;

static constexpr auto NUM_DISCREET_STEPS = 8.0f;
static constexpr auto NUM_SIMULATION_SECONDS = 2.0f;

static constexpr auto COLLISION_EPSILON = 0.01f;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseSimpleCollision::CBaseSimpleCollision( void )
{
	ClearActivePlanes();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			radius - 
//-----------------------------------------------------------------------------
void CBaseSimpleCollision::Setup( const Vector &origin, float speed, float gravity )
{
	TestForPlane( origin, Vector(  1,  0,  0 ), speed, gravity );
	TestForPlane( origin, Vector( -1,  0,  0 ), speed, gravity );
	TestForPlane( origin, Vector(  0,  1,  0 ), speed, gravity );
	TestForPlane( origin, Vector(  0, -1,  0 ), speed, gravity );
	TestForPlane( origin, Vector(  0,  0,  1 ), speed, gravity );
	TestForPlane( origin, Vector(  0,  0, -1 ), speed, gravity );
}

//-----------------------------------------------------------------------------
// Purpose: Trace line for super-simplified traces
// Input  : &start - start position
//			&end - end position
//			*pTrace - trace structure to fill
//			coarse - tests again with a real trace unless coarse is set
//-----------------------------------------------------------------------------
void CBaseSimpleCollision::TraceLine( const Vector &start, const Vector &end, trace_t *pTrace, bool coarse )
{
	//Iterate over all active planes
	for ( int i = 0; i < m_nActivePlanes; i++ )
	{
		//Must be a valid plane
		if ( m_collisionPlanes[i].m_Dist == -1.0f )
			continue;

		//Get our information about the relation to this plane
		float dot1 = m_collisionPlanes[i].DistTo(start);
		float dot2 = m_collisionPlanes[i].DistTo(end);

		//Don't consider particles on the backside of planes
		if ( dot1 < -COLLISION_EPSILON )
			continue;

		//Must be crossing the plane's boundary
		if ( ( dot1 > COLLISION_EPSILON ) == ( dot2 > COLLISION_EPSILON ) )
			continue;

		//Find the intersection point
		float	t = dot1 / (dot1 - dot2);
		Vector	vIntersection = start + (end - start) * t;

		//Fake the collision info
		pTrace->endpos			= vIntersection;
		pTrace->fraction		= t	- COLLISION_EPSILON;
		pTrace->plane.normal	= m_collisionPlanes[i].m_Normal;
		pTrace->plane.dist		= m_collisionPlanes[i].m_Dist;
		
		//If we need an exact trace, test again on a successful hit
		if ( ( coarse == false ) && ( pTrace->fraction < 1.0f ) )
		{
			UTIL_TraceLine( start, end, MASK_SOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, pTrace );
		}

		#if	__DEBUG_PARTICLE_COLLISION_OVERLAY
		debugoverlay->AddBoxOverlay( vIntersection, Vector(-1,-1,-1), Vector(1,1,1), QAngle(0,0,0), 0, 255, 0, 16, __DEBUG_PARTICLE_COLLISION_OVERLAY_LIFETIME );
		#endif	//__DEBUG_PARTICLE_COLLISION_OVERLAY
		
		//Done
		return;
	}

	//Fell through, so clear all the fields
	pTrace->plane.normal[0] = 0.0f;
	pTrace->plane.normal[1] = 0.0f;
	pTrace->plane.normal[2] = 0.0f;
	pTrace->plane.dist		= 0.0f;
	pTrace->fraction		= 1.0f;
	pTrace->allsolid		= false;
	pTrace->startsolid		= false;
	pTrace->m_pEnt			= nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Tests the planes against all others for validity
// Input  : *plane - plane to test
//-----------------------------------------------------------------------------
void CBaseSimpleCollision::ConsiderPlane( cplane_t *plane )
{
	//Test against all other active planes
	for ( int i = 0; i < m_nActivePlanes; i++ )
	{
		if ( m_collisionPlanes[i].m_Dist != -1.0f )
		{
			//Test for coplanar
			if ( ( m_collisionPlanes[i].m_Normal == plane->normal ) && ( m_collisionPlanes[i].m_Dist == plane->dist ) )
				return;
		}
	}

	//Don't overrun
	if ( m_nActivePlanes >= MAX_COLLISION_PLANES )
		return;

	//Take it
	m_collisionPlanes[m_nActivePlanes].m_Dist	= plane->dist;
	m_collisionPlanes[m_nActivePlanes].m_Normal	= plane->normal;
	m_nActivePlanes++;
}

//-----------------------------------------------------------------------------
// Purpose: Runs a simulation of the average particle's movement, looking for collisions along the way
// Input  : &start - start of the simulation
//			&dir - direction of travel
//			speed - speed of the particle
//			gravity - gravity being used
//-----------------------------------------------------------------------------
void CBaseSimpleCollision::TestForPlane( const Vector &start, const Vector &dir, float speed, float gravity )
{
	trace_t	tr;
	Vector	testStart, testEnd;

	testStart = start;

	//Setup our step increments
	float	dStepTime = (NUM_SIMULATION_SECONDS/NUM_DISCREET_STEPS);
	Vector	vStepIncr = dir * ( speed * dStepTime );
	float	flGravIncr = gravity*dStepTime;

	//Simulate collsions in discreet steps
	for ( int i = 1; i <= NUM_DISCREET_STEPS; i++ )
	{
		testEnd		= testStart + vStepIncr;
		testEnd[2] -= flGravIncr * (0.5f*(dStepTime*i)*(dStepTime*i) );

		//Trace the line
		UTIL_TraceLine( testStart, testEnd, MASK_SOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, &tr );

		//See if we found one
		if ( tr.fraction != 1.0f )
		{
			#if	__DEBUG_PARTICLE_COLLISION_OVERLAY
			debugoverlay->AddLineOverlay( testStart, tr.endpos, 255, 0, 0, true, __DEBUG_PARTICLE_COLLISION_OVERLAY_LIFETIME );
			
			QAngle angles;

			VectorAngles( tr.plane.normal,angles );
			angles[PITCH] += 90;

			debugoverlay->AddBoxOverlay( tr.endpos, Vector(-64,-64,0), Vector(64,64,0), angles, 255, 0, 0, 16, __DEBUG_PARTICLE_COLLISION_OVERLAY_LIFETIME );
			#endif	//__DEBUG_PARTICLE_COLLISION_OVERLAY
			
			//Test the plane against a set of criteria
			ConsiderPlane( &tr.plane );

			return;
		}

		//We missed
		#if	__DEBUG_PARTICLE_COLLISION_OVERLAY
		debugoverlay->AddLineOverlay( testStart, tr.endpos, 0, 128.0f+(128.0f*((float)i/(float)NUM_DISCREET_STEPS)), 0, true, __DEBUG_PARTICLE_COLLISION_OVERLAY_LIFETIME );
		#endif	//__DEBUG_PARTICLE_COLLISION_OVERLAY
		
		//Save that position for the next round
		testStart = testEnd;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseSimpleCollision::ClearActivePlanes( void )
{ 
	for ( int i = 0; i < MAX_COLLISION_PLANES; i++ )
	{
		m_collisionPlanes[i].m_Dist	= -1.0f;
		m_collisionPlanes[i].m_Normal.Init();
	}

	m_nActivePlanes = 0; 
}

//
// CParticleCollision
//

//-----------------------------------------------------------------------------
// Constructor 
//-----------------------------------------------------------------------------
CParticleCollision::CParticleCollision( void )
{
	m_flGravity					= 800.0f;
	m_flCollisionDampen			= 0.5f;
	m_flAngularCollisionDampen	= 0.25f;

	ClearActivePlanes();
}

//-----------------------------------------------------------------------------
// Purpose: Test for surrounding collision surfaces for quick collision testing for the particle system
// Input  : &origin - starting position
//			*dir - direction of movement (if NULL, will do a point emission test in four directions)
//			angularSpread - looseness of the spread
//			minSpeed - minimum speed
//			maxSpeed - maximum speed
//			gravity - particle gravity for the sytem
//			dampen - dampening amount on collisions
//-----------------------------------------------------------------------------
void CParticleCollision::Setup( const Vector &origin, const Vector *dir, float angularSpread, float minSpeed, float maxSpeed, float gravity, float dampen )
{
	//Take the information for this simulation
	m_flGravity				= gravity;
	m_flCollisionDampen		= dampen;
	m_nActivePlanes			= 0;

	//We take a rough estimation of the spray
	float	speedAvg	= (minSpeed+maxSpeed)*0.5f;

	//Point or directed?
	if ( dir == nullptr )
	{
		//Test all around
		TestForPlane( origin, Vector(  1,  0,  0 ), speedAvg, gravity );
		TestForPlane( origin, Vector( -1,  0,  0 ), speedAvg, gravity );
		TestForPlane( origin, Vector(  0,  1,  0 ), speedAvg, gravity );
		TestForPlane( origin, Vector(  0, -1,  0 ), speedAvg, gravity );
		TestForPlane( origin, Vector(  0,  0,  1 ), speedAvg, gravity );
		TestForPlane( origin, Vector(  0,  0, -1 ), speedAvg, gravity );
	}
	else
	{
		Vector	vSkewDir, vRight;
		QAngle  vAngles;

		//FIXME: Quicker conversion?
		//FIXME: We need to factor in the angular spread instead
		VectorAngles( *dir, vAngles );
		AngleVectors( vAngles, nullptr, &vRight, nullptr );

		//Test straight
		TestForPlane( origin, *dir, speedAvg, gravity );

		vSkewDir = vRight;

		//Test right
		TestForPlane( origin, vSkewDir, speedAvg, gravity );

		vSkewDir *= -1.0f;

		//Test left
		TestForPlane( origin, vSkewDir, speedAvg, gravity );

		#if	__DEBUG_PARTICLE_COLLISION_OVERLAY
		DevMsg( 1, "CParticleCollision: Found %d active plane(s)\n", m_nActivePlanes );
		#endif	//__DEBUG_PARTICLE_COLLISION_OVERLAY
	}
}

//-----------------------------------------------------------------------------
// Purpose: Simulate movement, with collision
// Input  : &origin - position of the particle
//			&velocity - velocity of the particle
//			&rollDelta - roll delta of the particle
//			timeDelta - time step
//-----------------------------------------------------------------------------
bool CParticleCollision::MoveParticle( Vector &origin, Vector &velocity, float *rollDelta, float timeDelta, trace_t *pTrace )
{	
	//Don't bother with non-moving particles
	if ( velocity == vec3_origin )
		return false;

	//Factor in gravity
	velocity[2] -= m_flGravity * timeDelta;

	//Move
	Vector testPosition = ( origin + ( velocity * timeDelta ) );	

	//Only collide if we have active planes
	if ( m_nActivePlanes > 0 )
	{
		//Collide
		TraceLine( origin, testPosition, pTrace );

		//See if we hit something
		if ( pTrace->fraction != 1.0f )
		{
			#if	__DEBUG_PARTICLE_COLLISION_RETEST
			//Retest the collision with a true trace line to avoid errant collisions
			UTIL_TraceLine( origin, testPosition, MASK_SOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, pTrace );
			#endif	//__DEBUG_RETEST_COLLISION

			//Did we hit anything?
			if ( pTrace->fraction != 1.0f )
			{
				//See if we've settled
				if ( ( pTrace->plane.normal[2] >= 0.5f ) && ( fabs( velocity[2] ) <= 48.0f ) )
				{		
					//Leave the particle at the collision point
					origin += velocity * ( (pTrace->fraction-COLLISION_EPSILON) * timeDelta );
					
					//Stop the particle
					velocity	= vec3_origin;
					
					if ( rollDelta != nullptr )
					{
						*rollDelta	= 0.0f;
					}
					
					return false;
				}
				else
				{
					//Move the particle to the collision point
					origin += velocity * ( (pTrace->fraction-COLLISION_EPSILON) * timeDelta );
					
					//Find the reflection vector
					float proj = velocity.Dot( pTrace->plane.normal );
					velocity += pTrace->plane.normal * (-proj*2.0f);
					
					//Apply dampening
					velocity *= random_valve->RandomFloat( (m_flCollisionDampen-0.1f), (m_flCollisionDampen+0.1f) );

					//Dampen the roll of the particles
					if ( rollDelta != nullptr )
					{
						(*rollDelta) *= -0.25f;
					}

					return true;
				}
			}
			else
			{
				#if	__DEBUG_PARTICLE_COLLISION_OVERLAY
				//Display a false hit
				debugoverlay->AddBoxOverlay( pTrace->endpos, Vector(-1,-1,-1), Vector(1,1,1), QAngle(0,0,0), 255, 0, 0, 16, __DEBUG_PARTICLE_COLLISION_OVERLAY_LIFETIME );
				#endif	//__DEBUG_PARTICLE_COLLISION_OVERLAY
			}
		}
	}

	//Simple move, no collision
	origin = testPosition;
	
	return false;
}

#undef __DEBUG_PARTICLE_COLLISION_RETEST
#undef __DEBUG_PARTICLE_COLLISION_OVERLAY

class CParticleMgr;
class CParticleRenderIterator;
class CParticleSimulateIterator;
class CBaseEntity;
class CBaseParticleEntity;
struct Particle;

using namespace ::ks::reflect;

// ------------------------------------------------------------------------- //
// Defines.
// ------------------------------------------------------------------------- //

static constexpr auto MAX_FIRE_EMITTERS = 128;
static constexpr auto FIRE_PARTICLE_LIFETIME = 2;

Vector g_FireSpreadDirection(0,0,1);

class FireRamp
{
public:
			FireRamp(const Vector &s, const Vector &e)
			{
				m_Start=s;
				m_End=e;
			}

	Vector	m_Start;
	Vector	m_End;
};

FireRamp g_FireRamps[] =
{
	FireRamp(Vector(1,0,0), Vector(1,1,0)),
	FireRamp(Vector(0.5,0.5,0), Vector(0,0,0))
};
inline constexpr auto NUM_FIRE_RAMPS = (sizeof(g_FireRamps) / sizeof(g_FireRamps[0]));

static constexpr auto NUM_FIREGRID_OFFSETS = 8;
Vector g_Offsets[NUM_FIREGRID_OFFSETS] =
{
	Vector(-1,-1,-1),
	Vector( 1,-1,-1),
	Vector(-1, 1,-1),
	Vector( 1, 1,-1),

	Vector(-1,-1, 1),
	Vector( 1,-1, 1),
	Vector(-1, 1, 1),
	Vector( 1, 1, 1),
};

// If you follow g_Offset[index], you can follow g_Offsets[GetOppositeOffset(index)] to get back.
inline int GetOppositeOffset(int offset)
{
	return NUM_FIREGRID_OFFSETS - offset - 1;
}

// ------------------------------------------------------------------------- //
// Classes.
// ------------------------------------------------------------------------- //

class CParticleFire : public CBaseParticleEntity, public IPrototypeAppEffect
{
	[[= ks::reflect::NetTable{ .name = "DT_ParticleFire", .base = false } ]]
	[[= ks::reflect::From<"m_vOrigin", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_vDirection", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CParticleFire, CBaseParticleEntity );
	DECLARE_CLIENTCLASS();					

					CParticleFire();
					~CParticleFire();

	class FireEmitter
	{
	public:
		Vector			m_Pos;
		TimedEvent		m_SpawnEvent;
		float			m_Lifetime;			// How long it's been emitting.
		unsigned char	m_DirectionsTested;	// 1 bit for each of g_Offsets.
	};

	class FireParticle : public Particle
	{
	public:
		Vector			m_StartPos;		// The particle moves from m_StartPos to (m_StartPos+m_Direction) over its lifetime.
		Vector			m_Direction;
		
		float			m_Lifetime;
		float			m_SpinAngle;
		unsigned char	m_iRamp;		// Which fire ramp are we using?
	};

// CBaseEntity.
public:

	virtual void	OnDataChanged( DataUpdateType_t updateType );

// IPrototypeAppEffect.
public:
	virtual void	Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs);

// IParticleEffect.
public:
	virtual void	Update(float fTimeDelta);
	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );

public:
	CParticleMgr		*m_pParticleMgr;
	PMaterialHandle	m_MaterialHandle;

	// Controls where the initial fire goes.
	Vector			m_vOrigin;
	Vector			m_vDirection;

	TimedEvent		m_EmitterSpawn;
	FireEmitter		m_Emitters[MAX_FIRE_EMITTERS];
	int				m_nEmitters;

private:
					CParticleFire( const CParticleFire & );
};

// ------------------------------------------------------------------------- //
// Tables.
// ------------------------------------------------------------------------- //

// Expose to the particle app.

// Datatable..
IMPLEMENT_REFLECT_CLIENTCLASS( CParticleFire, DT_ParticleFire, CParticleFire )

// ------------------------------------------------------------------------- //
// CFireSmoke implementation.
// ------------------------------------------------------------------------- //
CParticleFire::CParticleFire()
{
	m_pParticleMgr = nullptr;
	m_MaterialHandle = INVALID_MATERIAL_HANDLE;
}

CParticleFire::~CParticleFire()
{
	if(m_pParticleMgr)
		m_pParticleMgr->RemoveEffect( &m_ParticleEffect );
}

void CParticleFire::OnDataChanged(DataUpdateType_t updateType)
{
	CBaseEntity::OnDataChanged(updateType);

	if(updateType == DATA_UPDATE_CREATED)
	{
		Start(ParticleMgr(), nullptr);
	}
}

void CParticleFire::Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs)
{
	m_pParticleMgr = pParticleMgr;
	m_pParticleMgr->AddEffect( &m_ParticleEffect, this );
	m_MaterialHandle = m_ParticleEffect.FindOrAddMaterial("particle/particle_fire");

	// Start 
	m_nEmitters = 0;
	m_EmitterSpawn.Init(15);
}

static float fireSpreadDist = 15;
static float size = 20;

void CParticleFire::Update(float fTimeDelta)
{
	if(!m_pParticleMgr)
	{
		contract_assert( false );
		return;
	}

	// Add new emitters.
	if(m_nEmitters < MAX_FIRE_EMITTERS)
	{
		float tempDelta = fTimeDelta;
		while(m_EmitterSpawn.NextEvent(tempDelta))
		{
			FireEmitter *pEmitter = nullptr;

			if(m_nEmitters == 0)
			{
				// Make the first emitter.
				trace_t trace;
				UTIL_TraceLine(m_vOrigin, m_vOrigin+m_vDirection*1000, MASK_SOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, &trace);
				if(trace.fraction < 1)
				{
					pEmitter = &m_Emitters[m_nEmitters];
					pEmitter->m_Pos = trace.endpos + trace.plane.normal * (size - 1);
					pEmitter->m_DirectionsTested = 0;
				}
			}
			else
			{
				static int nTries = 50;
				for(int iTry=0; iTry < nTries; iTry++)
				{
					FireEmitter *pSourceEmitter = &m_Emitters[ RandomInt( 0, m_nEmitters-1 )];
					
					int iOffset = RandomInt( 0, NUM_FIREGRID_OFFSETS - 1 );
					if(pSourceEmitter->m_DirectionsTested & (1 << iOffset))
						continue;

					// Test the corners of the new cube. If some points are solid and some are not, then
					// we can put fire here.
					Vector basePos = pSourceEmitter->m_Pos + g_Offsets[iOffset] * fireSpreadDist;
					int nSolidCorners = 0;
					for(int iCorner=0; iCorner < NUM_FIREGRID_OFFSETS; iCorner++)
					{
						Vector vCorner = basePos + g_Offsets[iCorner]*fireSpreadDist;
						if ( enginetrace->GetPointContents(vCorner) & CONTENTS_SOLID )
							++nSolidCorners;
					}

					// Don't test this square again.
					pSourceEmitter->m_DirectionsTested |= 1 << iOffset;

					if(nSolidCorners != 0 && nSolidCorners != NUM_FIREGRID_OFFSETS)
					{
						pEmitter = &m_Emitters[m_nEmitters];
						pEmitter->m_Pos = basePos;
						pEmitter->m_DirectionsTested = 1 << GetOppositeOffset(iOffset);
					}
				}
			}
			
			if(pEmitter)
			{
				pEmitter->m_Lifetime = 0;
				pEmitter->m_SpawnEvent.Init(1);
				++m_nEmitters;
			}
		}
	}

	// Spawn particles out of the emitters.
	for(int i=0; i < m_nEmitters; i++)
	{
		FireEmitter *pEmitter = &m_Emitters[i];

		float tempDelta = fTimeDelta;
		while(pEmitter->m_SpawnEvent.NextEvent(tempDelta))
		{
			FireParticle *pParticle = (FireParticle*)m_ParticleEffect.AddParticle(sizeof(FireParticle), m_MaterialHandle);
			if(pParticle)
			{
				static float particleSpeed = 15;
				pParticle->m_StartPos = pEmitter->m_Pos;
				pParticle->m_Direction = g_FireSpreadDirection * particleSpeed + RandomVector(0, particleSpeed*0.5);
				pParticle->m_iRamp = RandomInt( 0, NUM_FIRE_RAMPS - 1 );
				pParticle->m_Lifetime = 0;
			}
		}
	}
}

void CParticleFire::RenderParticles( CParticleRenderIterator *pIterator )
{
	const FireParticle *pParticle = (const FireParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		float smooth01 = 1 - (std::cos(pParticle->m_Lifetime * 3.14159 / FIRE_PARTICLE_LIFETIME) * 0.5 + 0.5);
		float smooth00 = 1 - (cos(pParticle->m_Lifetime * 3.14159 * 2 / FIRE_PARTICLE_LIFETIME) * 0.5 + 0.5);
		
		FireRamp *pRamp = &g_FireRamps[pParticle->m_iRamp];
		Vector curColor = pRamp->m_Start + (pRamp->m_End - pRamp->m_Start) * smooth01;
		
		// Render.
		Vector tPos;
		TransformParticle(m_pParticleMgr->GetModelView(), pParticle->m_Pos, tPos);
		float sortKey = (int)tPos.z;

		RenderParticle_ColorSize(
			pIterator->GetParticleDraw(),
			tPos,
			curColor,
			smooth00,
			size);

		pParticle = (const FireParticle*)pIterator->GetNext( sortKey );
	}
}

void CParticleFire::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	FireParticle *pParticle = (FireParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		// Should this particle die?
		pParticle->m_Lifetime += pIterator->GetTimeDelta();
		if(pParticle->m_Lifetime > FIRE_PARTICLE_LIFETIME)
		{
			pIterator->RemoveParticle( pParticle );
		}
		else
		{
			float smooth01 = 1 - (std::cos(pParticle->m_Lifetime * 3.14159 / FIRE_PARTICLE_LIFETIME) * 0.5 + 0.5);
			pParticle->m_Pos = pParticle->m_StartPos + pParticle->m_Direction * smooth01;
		}

		pParticle = (FireParticle*)pIterator->GetNext();
	}
}

class CEffectData;
class CBaseEntity;

using namespace ::ks::reflect;

extern void StartParticleEffect( const CEffectData &data, int nSplitScreenPlayerSlot = -1 );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int GetAttachTypeFromString( const char *pszString )
{
	if ( !pszString || !pszString[0] )
		return -1;

	// If you add new attach types, you need to add them to this list
	static const char *pAttachmentNames[MAX_PATTACH_TYPES] =
	{
		"start_at_origin",		// PATTACH_ABSORIGIN = 0,
		"follow_origin",		// PATTACH_ABSORIGIN_FOLLOW,
		"start_at_customorigin",// PATTACH_CUSTOMORIGIN,
		"follow_customorigin",  // PATTACH_CUSTOMORIGIN_FOLLOW,
		"start_at_attachment",	// PATTACH_POINT,
		"follow_attachment",	// PATTACH_POINT_FOLLOW,
		"follow_eyes",			// PATTACH_EYES_FOLLOW,
		"follow_overhead",		// PATTACH_OVERHEAD_FOLLOW
		"world_origin",			// PATTACH_WORLDORIGIN
		"follow_rootbone"		// PATTACH_ROOTBONE_FOLLOW
	};

	for ( int i = 0; i < MAX_PATTACH_TYPES; i++ )
	{
		if ( FStrEq( pAttachmentNames[i], pszString ) )
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ParseParticleEffects( bool bLoadSheets )
{

	g_pParticleSystemMgr->ShouldLoadSheets( bLoadSheets );

	std::vector< std::string > files;
	GetParticleManifest( files );

	int nCount = std::ssize( files );
	for ( int i = 0; i < nCount; ++i )
	{
		g_pParticleSystemMgr->ReadParticleConfigFile( files[i].c_str(), false, false );
	}

	g_pParticleSystemMgr->DecommitTempMemory();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PrecacheStandardParticleSystems( )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchParticleEffect( const char *pszParticleName, ParticleAttachment_t iAttachType, CBaseEntity *pEntity, const char *pszAttachmentName, bool bResetAllParticlesOnEntity, int nSplitScreenPlayerSlot /*= -1*/, IRecipientFilter *filter /*= NULL*/ )
{
	int iAttachment = -1;
	if ( pEntity && pEntity->GetBaseAnimating() )
	{
		// Find the attachment point index
		iAttachment = pEntity->GetBaseAnimating()->LookupAttachment( pszAttachmentName );
		if ( iAttachment == -1 )
		{
			Warning("Model '%s' doesn't have attachment '%s' to attach particle system '%s' to.\n", STRING(pEntity->GetBaseAnimating()->GetModelName()), pszAttachmentName, pszParticleName );
			return;
		}
	}

	DispatchParticleEffect( pszParticleName, iAttachType, pEntity, iAttachment, bResetAllParticlesOnEntity, nSplitScreenPlayerSlot, filter );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchParticleEffect( const char *pszParticleName, ParticleAttachment_t iAttachType, CBaseEntity *pEntity, int iAttachmentPoint, bool bResetAllParticlesOnEntity, int nSplitScreenPlayerSlot /*= -1*/, IRecipientFilter *filter /*= NULL*/, bool bAllowDormantSpawn )
{
	CEffectData	data;

	if ( pEntity )
	{
		data.m_vOrigin = pEntity->GetAbsOrigin();
	}

	data.m_nHitBox = GetParticleSystemIndex( pszParticleName );
	if ( pEntity )
	{
		data.m_hEntity = pEntity;
		data.m_fFlags |= PARTICLE_DISPATCH_FROM_ENTITY;
	}
	data.m_nDamageType = iAttachType;
	data.m_nAttachmentIndex = iAttachmentPoint;

	if ( bResetAllParticlesOnEntity )
	{
		data.m_fFlags |= PARTICLE_DISPATCH_RESET_PARTICLES;
	}
	if ( bAllowDormantSpawn )
	{
		data.m_fFlags |= PARTICLE_DISPATCH_ALLOW_DORMANT;
	}

	// Avoid an unnecessary string search, also behaves better w/ precache checks
	StartParticleEffect( data, nSplitScreenPlayerSlot );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchParticleEffectLink( const char *pszParticleName, ParticleAttachment_t iAttachType, CBaseEntity *pEntity, CBaseEntity *pOtherEntity, int iAttachmentPoint, bool bResetAllParticlesOnEntity, int nSplitScreenPlayerSlot /*= -1*/ )
{
	CEffectData	data;

	if ( pEntity )
	{
		data.m_vOrigin = pEntity->GetAbsOrigin();
	}

	data.m_nHitBox = GetParticleSystemIndex( pszParticleName );
	if ( pEntity && pOtherEntity )
	{
		data.m_hEntity = pEntity;
		data.m_nOtherEntIndex = pOtherEntity->entindex();

		data.m_fFlags |= PARTICLE_DISPATCH_FROM_ENTITY;
	}
	data.m_nDamageType = iAttachType;
	data.m_nAttachmentIndex = iAttachmentPoint;

	if ( bResetAllParticlesOnEntity )
	{
		data.m_fFlags |= PARTICLE_DISPATCH_RESET_PARTICLES;
	}

	// Avoid an unnecessary string search, also behaves better w/ precache checks
	StartParticleEffect( data, nSplitScreenPlayerSlot );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchParticleEffect( int nEffectIndex, const Vector &vecOrigin, const QAngle &vecAngles, ParticleAttachment_t iAttachType, CBaseEntity *pEntity, int nSplitScreenPlayerSlot /*= -1*/ )
{
	CEffectData	data;

	data.m_nHitBox = nEffectIndex;
	data.m_vOrigin = vecOrigin;
	data.m_vAngles = vecAngles;

	if ( pEntity )
	{
		data.m_hEntity = pEntity;
		data.m_fFlags |= PARTICLE_DISPATCH_FROM_ENTITY;
		data.m_nDamageType = iAttachType;
	}
	else
	{
		data.m_hEntity = nullptr;
	}

	// Avoid an unnecessary search, also behaves better w/ precache checks...
	StartParticleEffect( data, nSplitScreenPlayerSlot );
}

void DispatchParticleEffect( const char *pszParticleName, const Vector &vecOrigin, const QAngle &vecAngles, ParticleAttachment_t iAttachType, CBaseEntity *pEntity, int nSplitScreenPlayerSlot /*= -1*/ )
{
	int nEffectIndex = GetParticleSystemIndex( pszParticleName );
	DispatchParticleEffect( nEffectIndex, vecOrigin, vecAngles, iAttachType, pEntity, nSplitScreenPlayerSlot );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchParticleEffect( int iEffectIndex, Vector vecOrigin, Vector vecStart, QAngle vecAngles, CBaseEntity *pEntity, int nSplitScreenPlayerSlot /*= -1*/, IRecipientFilter *filter /*= NULL*/ )
{
	CEffectData	data;

	data.m_nHitBox = iEffectIndex;
	data.m_vOrigin = vecOrigin;
	data.m_vStart = vecStart;
	data.m_vAngles = vecAngles;

	if ( pEntity )
	{
		data.m_hEntity = pEntity;
		data.m_fFlags |= PARTICLE_DISPATCH_FROM_ENTITY;
		data.m_nDamageType = PATTACH_CUSTOMORIGIN;
	}
	else
	{
		data.m_hEntity = nullptr;
	}

	// Avoid an unnecessary search, also behaves better w/ precache checks...
	StartParticleEffect( data, nSplitScreenPlayerSlot );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchParticleEffect( const char *pszParticleName, Vector vecOrigin, QAngle vecAngles, CBaseEntity *pEntity, int nSplitScreenPlayerSlot /*= -1*/, IRecipientFilter *filter /*= NULL*/ )
{
	int iIndex = GetParticleSystemIndex( pszParticleName );
	DispatchParticleEffect( iIndex, vecOrigin, vecOrigin, vecAngles, pEntity, nSplitScreenPlayerSlot, filter );
}

//-----------------------------------------------------------------------------
// Purpose: Yet another overload, lets us supply vecStart
//-----------------------------------------------------------------------------
void DispatchParticleEffect( const char *pszParticleName, Vector vecOrigin, Vector vecStart, QAngle vecAngles, CBaseEntity *pEntity, int nSplitScreenPlayerSlot /*= -1*/, IRecipientFilter *filter /*= NULL*/ )
{
	int iIndex = GetParticleSystemIndex( pszParticleName );
	DispatchParticleEffect( iIndex, vecOrigin, vecStart, vecAngles, pEntity, nSplitScreenPlayerSlot, filter );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void StopParticleEffects( CBaseEntity *pEntity )
{
	CEffectData	data;

	if ( pEntity )
	{
		data.m_hEntity = pEntity;
	}

	DispatchEffect( "ParticleEffectStop", data );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void StopParticleEffect( CBaseEntity *pEntity, const char *pszParticleName )
{
	CEffectData	data;

	if ( pEntity )
	{
		data.m_hEntity = pEntity;
	}

	data.m_nHitBox = GetParticleSystemIndex( pszParticleName );

	DispatchEffect( "ParticleEffectStop", data );
}

class CNewParticleEffect;
class CBaseAnimating;
class CBaseEntity;
class CBasePlayer;
class CBaseViewModel;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Save/load
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Prediction
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_PREDMAP_NO_BASE( CParticleProperty );

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_TABLE( CParticleProperty, DT_ParticleProperty );

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CParticleProperty::CParticleProperty()
{
	Init( nullptr );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CParticleProperty::~CParticleProperty()
{
	// We're being removed. Call StopEmission() on any particle system
	// that has an unlimited number of particles to emit.
	StopEmission( nullptr, false, true );
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
void CParticleProperty::Init( CBaseEntity *pEntity )
{
	m_pOuter = pEntity;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CParticleProperty::GetParticleAttachment( CBaseEntity *pEntity, const char *pszAttachmentName, const char *pszParticleName )
{
	contract_assert( pEntity && pEntity->GetBaseAnimating() );
	if ( !pEntity || !pEntity->GetBaseAnimating() )
		return -1;

	// Find the attachment point index
	int iAttachment = pEntity->GetBaseAnimating()->LookupAttachment( pszAttachmentName );
	if ( iAttachment == -1 )
	{
		Warning("Model '%s' doesn't have attachment '%s' to attach particle system '%s' to.\n", STRING(pEntity->GetBaseAnimating()->GetModelName()), pszAttachmentName, pszParticleName );
	}

	return iAttachment;
}

//-----------------------------------------------------------------------------
// Purpose: Get's a list of all renderables used for this particle property
//-----------------------------------------------------------------------------
int CParticleProperty::GetAllParticleEffectRenderables( IClientRenderable **pOutput, int iMaxOutput )
{
	if( iMaxOutput == 0 )
		return 0;

	int iReturnedRenderables = 0;
	int iParticleEffectListCount = std::ssize( m_ParticleEffects );

	for( int i = 0; i != iParticleEffectListCount; ++i )
	{
		if( m_ParticleEffects[i].pParticleEffect )
		{
			pOutput[iReturnedRenderables++] = m_ParticleEffects[i].pParticleEffect.get();
			if( iReturnedRenderables == iMaxOutput )
				break;
		}
	}

	return iReturnedRenderables;
}

//-----------------------------------------------------------------------------
// Purpose: Create a new particle system and attach it to our owner
//-----------------------------------------------------------------------------
CNewParticleEffect *CParticleProperty::Create( const char *pszParticleName, ParticleAttachment_t iAttachType, const char *pszAttachmentName )
{
	int iAttachment = GetParticleAttachment( GetOuter(), pszAttachmentName, pszParticleName );
	if ( iAttachment == -1 )
		return nullptr;

	// Create the system
	return Create( pszParticleName, iAttachType, iAttachment );
}
	  
//-----------------------------------------------------------------------------
// Purpose: Create a new particle system and attach it to our owner
//-----------------------------------------------------------------------------
static ConVar cl_particle_batch_mode( side_registry(), "cl_particle_batch_mode", "1" );

CNewParticleEffect *CParticleProperty::Create( CParticleSystemDefinition *pDef, ParticleAttachment_t iAttachType, int iAttachmentPoint, Vector vecOriginOffset, matrix3x4_t *matOffset )
{
	int nBatchMode = cl_particle_batch_mode.GetInt();
	bool bRequestedBatch = ( nBatchMode == 2 ) || ( ( nBatchMode == 1 ) && pDef && pDef->ShouldBatch() ); 
	if ( ( iAttachType == PATTACH_CUSTOMORIGIN ) && bRequestedBatch )
	{
		int iIndex = FindEffect( pDef->GetName() );
		if ( iIndex >= 0 )
		{
			CNewParticleEffect *pEffect = m_ParticleEffects[iIndex].pParticleEffect.get();
			pEffect->Restart();
			return pEffect;
		}
	}

	int iIndex = std::ssize( m_ParticleEffects ); m_ParticleEffects.emplace_back();
	ParticleEffectList_t *newEffect = &m_ParticleEffects[iIndex];
	newEffect->pParticleEffect = CNewParticleEffect::Create( m_pOuter, pDef, pDef->GetName() );

	if ( !newEffect->pParticleEffect->IsValid() )
	{
		// Caused by trying to spawn an unregistered particle effect. Remove it.
		ParticleMgr()->RemoveEffect( newEffect->pParticleEffect.get() );
		return nullptr;
	}

	AddControlPoint( iIndex, 0, GetOuter(), iAttachType, iAttachmentPoint, vecOriginOffset, matOffset );

	if ( m_pOuter )
	{
		m_pOuter->OnNewParticleEffect( pDef->GetName(), newEffect->pParticleEffect.get() );
	}

	return newEffect->pParticleEffect.get();
}

CNewParticleEffect *CParticleProperty::CreatePrecached( int nPrecacheIndex, ParticleAttachment_t iAttachType, int iAttachmentPoint, Vector vecOriginOffset, matrix3x4_t *matOffset )
{
	CParticleSystemDefinition *pDef = g_pParticleSystemMgr->FindPrecachedParticleSystem( nPrecacheIndex );
	if ( !pDef )
	{
		contract_assert( false && "Attempting to create unknown particle system" );
		return nullptr;
	}
	return Create( pDef, iAttachType, iAttachmentPoint, vecOriginOffset, matOffset );
}

CNewParticleEffect *CParticleProperty::Create( const char *pszParticleName, ParticleAttachment_t iAttachType, int iAttachmentPoint, Vector vecOriginOffset, matrix3x4_t *matOffset )
{
	CParticleSystemDefinition *pDef = g_pParticleSystemMgr->FindParticleSystem( pszParticleName );
	if ( !pDef )
	{
//		AssertMsg( 0, "Attempting to create unknown particle system" );
		Warning( "Attempting to create unknown particle system '%s' \n", pszParticleName );
		return nullptr;
	}
	return Create( pDef, iAttachType, iAttachmentPoint, vecOriginOffset, matOffset );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleProperty::AddControlPoint( CNewParticleEffect *pEffect, int iPoint, CBaseEntity *pEntity, ParticleAttachment_t iAttachType, const char *pszAttachmentName, Vector vecOriginOffset, matrix3x4_t *matOffset )
{
	contract_assert( pEffect );
	if ( pEffect )
	{
		int iAttachment = -1;
		if ( pszAttachmentName )
		{
			iAttachment = GetParticleAttachment( pEntity, pszAttachmentName, pEffect->GetEffectName() );
		}

		for ( int i = 0; i < std::ssize( m_ParticleEffects ); i++ )
		{
			if ( m_ParticleEffects[i].pParticleEffect == pEffect )
			{
				AddControlPoint( i, iPoint, pEntity, iAttachType, iAttachment, vecOriginOffset, matOffset );
			}
		}
	}
	else
	{
		DevWarning( "Attempted to add control point to NULL particle effect!\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleProperty::AddControlPoint( int iEffectIndex, int iPoint, CBaseEntity *pEntity, ParticleAttachment_t iAttachType, int iAttachmentPoint, Vector vecOriginOffset, matrix3x4_t *matOffset )
{
	contract_assert( iEffectIndex >= 0 && iEffectIndex < std::ssize( m_ParticleEffects ) );
	ParticleEffectList_t *pEffect = &m_ParticleEffects[iEffectIndex];
	contract_assert( std::ssize( pEffect->pControlPoints ) < MAX_PARTICLE_CONTROL_POINTS );

	int iIndex = std::ssize( pEffect->pControlPoints ); pEffect->pControlPoints.emplace_back();
	ParticleControlPoint_t *pNewPoint = &pEffect->pControlPoints[iIndex];
	pNewPoint->iControlPoint = iPoint;
	pNewPoint->hEntity = pEntity;
	pNewPoint->iAttachType = iAttachType;
	pNewPoint->iAttachmentPoint = iAttachmentPoint;
	pNewPoint->vecOriginOffset = vecOriginOffset;
	if ( matOffset )
		pNewPoint->matOffset = *matOffset;
	else
		pNewPoint->matOffset.Init( Vector(1,0,0), Vector(0,1,0), Vector(0,0,1), Vector(0,0,0) );

	UpdateParticleEffect( pEffect, true, iIndex );
}

//-----------------------------------------------------------------------------
// Used to replace a particle effect with a different one; attaches the control point updating to the new one
//-----------------------------------------------------------------------------
void CParticleProperty::ReplaceParticleEffect( CNewParticleEffect *pOldEffect, CNewParticleEffect *pNewEffect )
{
	int nCount = std::ssize( m_ParticleEffects );
	for ( int i = 0; i < nCount; ++i )
	{
		if ( pOldEffect != m_ParticleEffects[i].pParticleEffect.get() )
			continue;

		m_ParticleEffects[i].pParticleEffect = pNewEffect;
		UpdateParticleEffect( &m_ParticleEffects[i], true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the parent of a given control point to the index of some other
//			control point.
//-----------------------------------------------------------------------------
void CParticleProperty::SetControlPointParent( int iEffectIndex, int whichControlPoint, int parentIdx )
{

}

//-----------------------------------------------------------------------------
// Purpose: Stop effects from emitting more particles. If no effect is 
//			specified, all effects attached to this entity are stopped.
//-----------------------------------------------------------------------------
void CParticleProperty::StopEmission( CNewParticleEffect *pEffect, bool bWakeOnStop, bool bDestroyAsleepSystems, bool bForceRemoveInstantly, bool bPlayEndCap )
{
	// If we return from dormancy and are then told to stop emitting,
	// we should have died while dormant. Remove ourselves immediately.
	bool bRemoveInstantly = (m_iDormancyChangedAtFrame == gpGlobals->framecount);

	// force remove particles instantly if caller specified
	bRemoveInstantly |= bForceRemoveInstantly;

	if ( pEffect )
	{
		if ( FindEffect( pEffect ) != -1 )
		{
			pEffect->StopEmission( false, bRemoveInstantly, bWakeOnStop, bPlayEndCap );
		}
	}
	else
	{
		// Stop all effects
		float flNow = g_pParticleSystemMgr->GetLastSimulationTime();
		int nCount = std::ssize( m_ParticleEffects );
		for ( int i = nCount-1; i >= 0; i-- )
		{
			CNewParticleEffect *pTmp = m_ParticleEffects[i].pParticleEffect.get();
			bool bRemoveSystem = bRemoveInstantly || ( bDestroyAsleepSystems && ( flNow >= pTmp->m_flNextSleepTime ) );
			if ( bRemoveSystem )
			{
				m_ParticleEffects.erase( m_ParticleEffects.begin() + i );
				pTmp->SetOwner( nullptr );
			}
			pTmp->StopEmission( false, bRemoveSystem, !bRemoveSystem && bWakeOnStop, bPlayEndCap );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Remove effects immediately, including all current particles. If no
// effect is specified, all effects attached to this entity are removed.
//-----------------------------------------------------------------------------
void CParticleProperty::StopEmissionAndDestroyImmediately( CNewParticleEffect *pEffect )
{
	if ( pEffect )
	{
		int iIndex = FindEffect( pEffect );
		contract_assert( iIndex != -1 );
		if ( iIndex != -1 )
		{
			m_ParticleEffects.erase( m_ParticleEffects.begin() + iIndex );

			// Clear the owner so it doesn't try to call back to us on deletion
			pEffect->SetOwner( nullptr );
			pEffect->StopEmission( false, true );
		}
	}
	else
	{
		// Immediately destroy all effects
		int nCount = std::ssize( m_ParticleEffects );
		for ( int i = nCount-1; i >= 0; i-- )
		{
			CNewParticleEffect *pTmp = m_ParticleEffects[i].pParticleEffect.get();
			m_ParticleEffects.erase( m_ParticleEffects.begin() + i );

			// Clear the owner so it doesn't try to call back to us on deletion
			pTmp->SetOwner( nullptr );
			pTmp->StopEmission( false, true );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Stop all effects that have  a control point associated with the given
//          entity.
//-----------------------------------------------------------------------------
void CParticleProperty::StopParticlesInvolving( CBaseEntity *pEntity, bool bForceRemoveInstantly /* =false */ )
{
	contract_assert( pEntity );

	EHANDLE entHandle = pEntity;

	// If we return from dormancy and are then told to stop emitting,
	// we should have died while dormant. Remove ourselves immediately.
	bool bRemoveInstantly = (m_iDormancyChangedAtFrame == gpGlobals->framecount);
	// force remove particles instantly if caller specified
	bRemoveInstantly |= bForceRemoveInstantly;

	int nCount = std::ssize( m_ParticleEffects );
	for ( int i = 0; i < nCount; ++i )
	{
		// for each effect...
		ParticleEffectList_t &part = m_ParticleEffects[i];
		// look through all the control points to see if any mention the given object
		int cpCount = std::ssize( part.pControlPoints );
		for (int j = 0; j < cpCount ; ++j )
		{
			// if any control points respond to the given handle...
			if (part.pControlPoints[j].hEntity == entHandle)
			{
				part.pParticleEffect->StopEmission( false, bRemoveInstantly );
				break; // break out of the inner loop (to where it says BREAK TO HERE)
			}
		}
		// BREAK TO HERE
	}
}

//-----------------------------------------------------------------------------
// Purpose: Stop all effects that were created using the given definition
//			name.
//-----------------------------------------------------------------------------
void CParticleProperty::StopParticlesNamed( const char *pszEffectName, bool bForceRemoveInstantly /* =false */, int nSplitScreenPlayerSlot /*= -1*/ )
{
	CParticleSystemDefinition *pDef = g_pParticleSystemMgr->FindParticleSystem( pszEffectName );
	contract_assert( pDef && "Could not find particle definition %s" );
	if (!pDef)
		return;

	// If we return from dormancy and are then told to stop emitting,
	// we should have died while dormant. Remove ourselves immediately.
	bool bRemoveInstantly = (m_iDormancyChangedAtFrame == gpGlobals->framecount);
	// force remove particles instantly if caller specified
	bRemoveInstantly |= bForceRemoveInstantly;

	int nCount = std::ssize( m_ParticleEffects );
	for ( int i = 0; i < nCount; ++i )
	{
		// for each effect...
		CNewParticleEffect *pParticleEffect = m_ParticleEffects[i].pParticleEffect.get();
		if (pParticleEffect->m_pDef() == pDef)
		{
			if ( nSplitScreenPlayerSlot != -1 )
			{
				if ( !pParticleEffect->ShouldDrawForSplitScreenUser( nSplitScreenPlayerSlot ) )
					continue;
			}
			pParticleEffect->StopEmission( false, bRemoveInstantly );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleProperty::OnParticleSystemUpdated( CNewParticleEffect *pEffect, float flTimeDelta )
{
	int iIndex = FindEffect( pEffect );
	contract_assert( iIndex != -1 );
	if ( iIndex == -1 )
		return;

	UpdateParticleEffect( &m_ParticleEffects[iIndex] );

	/*
	// Display the bounding box of the particle effect
	Vector vecMins, vecMaxs;
	pEffect->GetRenderBounds( vecMins, vecMaxs );
	debugoverlay->AddBoxOverlay( pEffect->GetRenderOrigin(), vecMins, vecMaxs, QAngle( 0, 0, 0 ), 0, 255, 255, 0, 0 );
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleProperty::OnParticleSystemDeleted( CNewParticleEffect *pEffect )
{
	int iIndex = FindEffect( pEffect );
	if ( iIndex == -1 )
		return;

	if ( m_pOuter )
	{
		m_pOuter->OnParticleEffectDeleted( pEffect );
	}

	m_ParticleEffects[iIndex].pParticleEffect.detach();
	m_ParticleEffects.erase( m_ParticleEffects.begin() + iIndex );
}

//-----------------------------------------------------------------------------
// Purpose: The entity we're attached to has change dormancy state on our client
//-----------------------------------------------------------------------------
void CParticleProperty::OwnerSetDormantTo( bool bDormant )
{
	m_iDormancyChangedAtFrame = gpGlobals->framecount;

	int nCount = std::ssize( m_ParticleEffects );
	for ( int i = 0; i < nCount; i++ )
	{
		//m_ParticleEffects[i].pParticleEffect->SetShouldSimulate( !bDormant );
		m_ParticleEffects[i].pParticleEffect->SetDormant( bDormant );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CParticleProperty::FindEffect( CNewParticleEffect *pEffect )
{
	for ( int i = 0; i < std::ssize( m_ParticleEffects ); i++ )
	{
		if ( m_ParticleEffects[i].pParticleEffect == pEffect )
			return i;
	}

	return -1;
}

int CParticleProperty::FindEffect( const char *pEffectName )
{
	for ( int i = 0; i < std::ssize( m_ParticleEffects ); i++ )
	{
		if ( !V_stricmp( m_ParticleEffects[i].pParticleEffect->GetName(), pEffectName ) )
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleProperty::UpdateParticleEffect( ParticleEffectList_t *pEffect, bool bInitializing, int iOnlyThisControlPoint )
{
	if ( iOnlyThisControlPoint != -1 )
	{
		UpdateControlPoint( pEffect, iOnlyThisControlPoint, bInitializing );
		return;
	}

	// Loop through our control points and update them all
	for ( int i = 0; i < std::ssize( pEffect->pControlPoints ); i++ )
	{
		UpdateControlPoint( pEffect, i, bInitializing );
	}
}

extern void FormatViewModelAttachment( CBasePlayer *pPlayer, Vector &vOrigin, bool bInverse );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleProperty::UpdateControlPoint( ParticleEffectList_t *pEffect, int iPoint, bool bInitializing )
{

	ParticleControlPoint_t *pPoint = &pEffect->pControlPoints[iPoint];

	if ( pEffect->pParticleEffect->m_pDef->IsScreenSpaceEffect() && iPoint == 0 )
	{
		pEffect->pParticleEffect->SetControlPointOrientation( pPoint->iControlPoint, Vector(1,0,0), Vector(0,1,0), Vector(0,0,1) );
		pEffect->pParticleEffect->SetControlPoint( pPoint->iControlPoint, vec3_origin );
		return;
	}

	if ( !pPoint->hEntity.Get() )
	{
		if ( pPoint->iAttachType == PATTACH_WORLDORIGIN && bInitializing )
		{
			pEffect->pParticleEffect->SetControlPointOrientation( pPoint->iControlPoint, Vector(1,0,0), Vector(0,1,0), Vector(0,0,1) );
			pEffect->pParticleEffect->SetControlPoint( pPoint->iControlPoint, pPoint->vecOriginOffset );
			pEffect->pParticleEffect->SetSortOrigin( pPoint->vecOriginOffset );
		}

		pEffect->pParticleEffect->SetControlPointEntity( pPoint->iControlPoint, nullptr );
		return;
	}

	// Only update non-follow particles when we're initializing, 
	if ( !bInitializing && (pPoint->iAttachType == PATTACH_ABSORIGIN || pPoint->iAttachType == PATTACH_POINT ) )
		return;

	if ( pPoint->iAttachType == PATTACH_CUSTOMORIGIN )
		return;

	Vector vecOrigin, vecForward, vecRight, vecUp;

	switch ( pPoint->iAttachType )
	{
	case PATTACH_POINT:
	case PATTACH_POINT_FOLLOW:
		{
			CBaseAnimating *pAnimating = pPoint->hEntity->GetBaseAnimating();

			bool bValid = false;
			contract_assert( pAnimating );
			if ( pAnimating )
			{
				matrix3x4_t attachmentToWorld;

				if ( pAnimating->IsViewModel() )
				{
					CBasePlayer *pPlayer = ToBasePlayer( ((CBaseViewModel *)pAnimating)->GetOwner() );
					ACTIVE_SPLITSCREEN_PLAYER_GUARD( CBasePlayer::GetSplitScreenSlotForPlayer( pPlayer ) );

					if ( pAnimating->GetAttachment( pPoint->iAttachmentPoint, attachmentToWorld ) )
					{
						bValid = true;
						MatrixVectors( attachmentToWorld, &vecForward, &vecRight, &vecUp );
						MatrixPosition( attachmentToWorld, vecOrigin );

						// This is breaking in Portal
						if ( pEffect->pParticleEffect->m_pDef->IsViewModelEffect() )
						{
							FormatViewModelAttachment( pPlayer, vecOrigin, true );
						}
					}
				}
				else
				{
					// HACK_GETLOCALPLAYER_GUARD( "CParticleProperty::UpdateControlPoint" );

					if ( pAnimating->GetAttachment( pPoint->iAttachmentPoint, attachmentToWorld ) )
					{
						bValid = true;
						MatrixVectors( attachmentToWorld, &vecForward, &vecRight, &vecUp );
						MatrixPosition( attachmentToWorld, vecOrigin );

						if ( pEffect->pParticleEffect->m_pDef->IsViewModelEffect() )
						{
							HACK_GETLOCALPLAYER_GUARD( "CParticleProperty::UpdateControlPoint" );

							FormatViewModelAttachment( nullptr, vecOrigin, true );
						}
					}
				}
			}

			if ( !bValid )
			{
				static bool bWarned = false;
				if ( !bWarned )
				{
					bWarned = true;
					DevWarning( "Attempted to attach particle effect %s to an unknown attachment on entity %s\n",
						pEffect->pParticleEffect->m_pDef->GetName(), pAnimating ? pAnimating->GetClassname() : "(null)" );
				}

				// FIXME: what's the fallback? is it ok to kill the effect if we don't know where to attach it?
				pEffect->pParticleEffect->StopEmission();
			}
			if ( !bValid )
			{
				contract_assert( 0 );
				return;
			}
		}
		break;

	case PATTACH_ABSORIGIN:
	case PATTACH_ABSORIGIN_FOLLOW:
	default:
		{
			vecOrigin = pPoint->hEntity->GetAbsOrigin() + pPoint->vecOriginOffset;
			pPoint->hEntity->GetVectors( &vecForward, &vecRight, &vecUp );
		}
		break;

	case PATTACH_EYES_FOLLOW:
		{
			CBaseEntity *pEnt = pPoint->hEntity;

			if ( !pEnt->IsPlayer() )
				return;

			CBasePlayer *pPlayer = static_cast< CBasePlayer* >( pEnt );

			bool bValid = false;
			contract_assert( pPlayer );
			if ( pPlayer )
			{
				bValid = true;
				vecOrigin = pPlayer->EyePosition() + pPoint->vecOriginOffset;
				pPlayer->EyeVectors( &vecForward, &vecRight, &vecUp );
			}
			if ( !bValid )
			{
				contract_assert( 0 );
				return;
			}
		}
		break;

	case PATTACH_OVERHEAD_FOLLOW:
		{
			Vector vecMins, vecMaxs = vec3_origin;
			const model_t *mod = pPoint->hEntity->GetModel();
			if ( mod )
			{
				modelinfo->GetModelBounds( mod, vecMins, vecMaxs );
			}
			vecOrigin = pPoint->hEntity->GetAbsOrigin() + pPoint->vecOriginOffset;
			vecOrigin.z += vecMaxs.z;
			pPoint->hEntity->GetVectors( &vecForward, &vecRight, &vecUp );
		}
		break;
		
	case PATTACH_CUSTOMORIGIN_FOLLOW:
		{
			matrix3x4_t mat;
			MatrixMultiply( pPoint->hEntity->RenderableToWorldTransform(), pPoint->matOffset, mat );
			Vector vecForward, vecRight, vecUp;
			MatrixVectors( mat, &vecForward, &vecRight, &vecUp );
			MatrixPosition( mat, vecOrigin );
			pEffect->pParticleEffect->SetControlPointEntity( pPoint->iControlPoint, pPoint->hEntity );
			pEffect->pParticleEffect->SetControlPoint( pPoint->iControlPoint, vecOrigin );
			pEffect->pParticleEffect->SetSortOrigin( vecOrigin );
			pEffect->pParticleEffect->SetControlPointOrientation( pPoint->iControlPoint, vecForward, vecRight, vecUp );
			return;
		}
		break;
		
	case PATTACH_ROOTBONE_FOLLOW:
		{
			CBaseAnimating *pAnimating = pPoint->hEntity->GetBaseAnimating();

			contract_assert( pAnimating );
			if ( pAnimating )
			{
				matrix3x4_t rootBone;
				if ( pAnimating->GetRootBone( rootBone ) )
				{
					MatrixVectors( rootBone, &vecForward, &vecRight, &vecUp );
					MatrixPosition( rootBone, vecOrigin );
				}
			}
		}
		break;
	}	
	
	pEffect->pParticleEffect->SetControlPointOrientation( pPoint->iControlPoint, vecForward, vecRight, vecUp );
	pEffect->pParticleEffect->SetControlPointEntity( pPoint->iControlPoint, pPoint->hEntity );
	pEffect->pParticleEffect->SetControlPoint( pPoint->iControlPoint, vecOrigin );
	pEffect->pParticleEffect->SetSortOrigin( vecOrigin );
}

//-----------------------------------------------------------------------------
// Purpose: Output all active effects
//-----------------------------------------------------------------------------
void CParticleProperty::DebugPrintEffects( void )
{
	int nCount = std::ssize( m_ParticleEffects );
	for ( int i = 0; i < nCount; ++i )
	{
		// for each effect...
		CNewParticleEffect *pParticleEffect = m_ParticleEffects[i].pParticleEffect.get();

		if ( !pParticleEffect )
			continue;
	
		Msg( "(%d)  EffectName \"%s\"  Dormant? %s  Emission Stopped? %s \n",
			i,
			pParticleEffect->GetEffectName(),
			( pParticleEffect->m_bDormant ) ? "yes" : "no",
			( pParticleEffect->m_bEmissionStopped ) ? "yes" : "no" );
	}
}

bool CParticleProperty::IsValidEffect( const CNewParticleEffect *pEffect )
{
	if( pEffect == nullptr )
		return false;

	int nCount = std::ssize( m_ParticleEffects );
	for ( int i = 0; i < nCount; ++i )
	{
		if( pEffect == m_ParticleEffects[i].pParticleEffect.get() )
			return true;
	}

	return false;
}

ParticleControlPoint_t::ParticleControlPoint_t()
{
	iControlPoint = 0;
	iAttachType = PATTACH_ABSORIGIN_FOLLOW;
	iAttachmentPoint = 0;
	vecOriginOffset = vec3_origin;
	matOffset.Invalidate();
}

class CParticleMgr;

// Purpose: This module implements all the proxies used by the particle systems.

// ------------------------------------------------------------------------ //
// ParticleSphereProxy
// ------------------------------------------------------------------------ //

class ParticleSphereProxy : public IMaterialProxy
{
// IMaterialProxy overrides.
public:
	ParticleSphereProxy()
	{
	}

	virtual		~ParticleSphereProxy() 
	{
	}
	
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues )
	{
		m_pLightPosition = pMaterial->FindVar( "$light_position", nullptr, false );
		m_pLightColor = pMaterial->FindVar( "$light_color", nullptr, false );
		return true;
	}

	virtual void OnBind( void *pvParticleMgr )
	{
		if( !pvParticleMgr )
			return;

		CParticleMgr *pMgr = (CParticleMgr*)pvParticleMgr;
		CParticleLightInfo info;
		pMgr->GetDirectionalLightInfo( info );

		// Transform the light into camera space.
		Vector vTransformedPos = pMgr->GetModelView() * info.m_vPos;
		if ( m_pLightPosition )
			m_pLightPosition->SetVecValue( vTransformedPos.Base(), 3 );

		if ( m_pLightColor )
		{
			Vector vTotalColor = info.m_vColor * info.m_flIntensity;
			m_pLightColor->SetVecValue( vTotalColor.Base(), 3 );
		}
	}

	virtual void	Release( void ) { delete this; }

	virtual IMaterial *GetMaterial()
	{
		IMaterialVar *pVar = m_pLightPosition ? m_pLightPosition : m_pLightColor;
		if ( !pVar )
			return nullptr;
		return pVar->GetOwningMaterial();
	}

private:

	IMaterialVar	*m_pLightPosition;
	IMaterialVar	*m_pLightColor;
};

EXPOSE_MATERIAL_PROXY( ParticleSphereProxy, ParticleSphereProxy );

class CParticleRenderIterator;
class CParticleSimulateIterator;

using namespace ::ks::reflect;

// Defined in pm_math.c

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
ks::intrusive_ptr<CSimple3DEmitter> CSimple3DEmitter::Create( const char *pDebugName )	
{
	CSimple3DEmitter* pSimple3DEmitter = new CSimple3DEmitter( pDebugName );

	// Do in world space
	pSimple3DEmitter->m_ParticleEffect.SetEffectCameraSpace( false );
	return pSimple3DEmitter;
}

void CSimple3DEmitter::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	Particle3D *pParticle = (Particle3D*)pIterator->GetFirst();
	while ( pParticle )
	{
		const float	timeDelta = pIterator->GetTimeDelta();

		//Should this particle die?
		pParticle->m_flLifeRemaining -= timeDelta;

		if ( pParticle->IsDead() )
		{
			pIterator->RemoveParticle( pParticle );
		}
		else
		{
			// Angular rotation
			pParticle->m_vAngles.x += pParticle->m_flAngSpeed * timeDelta;
			pParticle->m_vAngles.y += pParticle->m_flAngSpeed * timeDelta;
			pParticle->m_vAngles.z += pParticle->m_flAngSpeed * timeDelta;

			//Simulate the movement with collision
			trace_t trace;
			m_ParticleCollision.MoveParticle( pParticle->m_Pos, pParticle->m_vecVelocity, &pParticle->m_flAngSpeed, timeDelta, &trace );

			// ---------------------------------------
			// Decay towards flat
			// ---------------------------------------
			if (pParticle->m_flAngSpeed == 0 || trace.fraction != 1.0)
			{
				pParticle->m_vAngles.x = anglemod(pParticle->m_vAngles.x);
				if (pParticle->m_vAngles.x < 180)
				{
					if (fabs(pParticle->m_vAngles.x - 90) > 0.5)
					{
						pParticle->m_vAngles.x = 0.5*pParticle->m_vAngles.x + 46;
					}
				}
				else
				{
					if (fabs(pParticle->m_vAngles.x - 270) > 0.5)
					{
						pParticle->m_vAngles.x = 0.5*pParticle->m_vAngles.x + 135;
					}
				}

				pParticle->m_vAngles.y = anglemod(pParticle->m_vAngles.y);
				if (fabs(pParticle->m_vAngles.y) > 0.5)
				{
					pParticle->m_vAngles.y = 0.5*pParticle->m_vAngles.z;
				}
			}
		}

		pParticle = (Particle3D*)pIterator->GetNext();
	}
}

void CSimple3DEmitter::RenderParticles( CParticleRenderIterator *pIterator )
{
	const Particle3D *pParticle = (const Particle3D *)pIterator->GetFirst();
	while ( pParticle )
	{
		float sortKey = CurrentViewForward().Dot( CurrentViewOrigin() - pParticle->m_Pos );

		// -------------------------------------------------------
		//  Set color based on direction towards camera
		// -------------------------------------------------------
		Vector	color;
		Vector vFaceNorm;
		Vector vCameraToFace = (pParticle->m_Pos - CurrentViewOrigin());
		AngleVectors(pParticle->m_vAngles,&vFaceNorm);
		float flFacing = DotProduct(vCameraToFace,vFaceNorm);

		if (flFacing <= 0)
		{
			color[0] = pParticle->m_uchFrontColor[0] / 255.0f;
			color[1] = pParticle->m_uchFrontColor[1] / 255.0f;
			color[2] = pParticle->m_uchFrontColor[2] / 255.0f;
		}
		else
		{
			color[0] = pParticle->m_uchBackColor[0] / 255.0f;
			color[1] = pParticle->m_uchBackColor[1] / 255.0f;
			color[2] = pParticle->m_uchBackColor[2] / 255.0f;
		}

		//Render it in world space
		RenderParticle_ColorSizeAngles(
			pIterator->GetParticleDraw(),
			pParticle->m_Pos,
			color,
			pParticle->GetFadeFraction(),
			pParticle->m_uchSize,
			pParticle->m_vAngles);

		pParticle = (const Particle3D *)pIterator->GetNext( sortKey );
	}
}

class CParticleMgr;
class CParticleRenderIterator;
class CParticleSimulateIterator;
class CBaseEntity;
class CBaseParticleEntity;
class CCSPlayer;
struct Particle;

using namespace ::ks::reflect;

// ------------------------------------------------------------------------- //
// Definitions
// ------------------------------------------------------------------------- //

static Vector s_FadePlaneDirections[] =
{
	Vector( 1,0,0),
	Vector(-1,0,0),
	Vector(0, 1,0),
	Vector(0,-1,0),
	Vector(0,0, 1),
	Vector(0,0,-1)
};
#define NUM_FADE_PLANES	(sizeof(s_FadePlaneDirections)/sizeof(s_FadePlaneDirections[0]))

// This is used to randomize the direction it chooses to move a particle in.
int g_OffsetLookup[3] = {-1,0,1};

// ------------------------------------------------------------------------- //
// Classes
// ------------------------------------------------------------------------- //
class CParticleSmokeGrenade : public CBaseParticleEntity, public IPrototypeAppEffect
{
	[[= ks::reflect::NetTable{ .name = "DT_ParticleSmokeGrenade" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CParticleSmokeGrenade, CBaseParticleEntity );
	DECLARE_CLIENTCLASS();

					CParticleSmokeGrenade();
					~CParticleSmokeGrenade();

private:
	
	class SmokeGrenadeParticle : public Particle
	{
	public:
		float				m_RotationSpeed;
		float				m_CurRotation;
		float				m_FadeAlpha;		// Set as it moves around.
		unsigned char		m_ColorInterp;		// Amount between min and max colors.
		unsigned char		m_Color[4];
	};

public:

	// Optional call. It will use defaults if you don't call this.
	void			SetParams(
		);

	// Call this to move the source..
	void			SetPos(const Vector &pos);

// CBaseEntity.
public:
	virtual void	OnDataChanged( DataUpdateType_t updateType );

// IPrototypeAppEffect.
public:
	virtual void	Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs);

// IParticleEffect.
public:
	virtual void	Update(float fTimeDelta);
	virtual void	RenderParticles( CParticleRenderIterator *pIterator );
	virtual void	SimulateParticles( CParticleSimulateIterator *pIterator );
	virtual void	NotifyRemove();
	virtual void	GetParticlePosition( Particle *pParticle, Vector& worldpos );
	virtual void	ClientThink();

// Proxies.
public:

	static void		RecvProxy_CurrentStage(  const CRecvProxyData *pData, void *pStruct, void *pOut );

private:

	// The SmokeEmitter represents a grid in 3D space.
	class SmokeParticleInfo
	{
	public:
		SmokeGrenadeParticle	*m_pParticle;
		int						m_TradeIndex;		// -1 if not exchanging yet.
		float					m_TradeClock;		// How long since they started trading.
		float					m_TradeDuration;	// How long the trade will take to finish.
		float					m_FadeAlpha;		// Calculated from nearby world geometry.
		unsigned char			m_Color[4];
	};

	void ApplyDynamicLight( const Vector &vParticlePos, Vector &color );
	void UpdateDynamicLightList( const Vector &vMins, const Vector &vMaxs );

	void UpdateSmokeTrail( float fTimeDelta );
	
	void UpdateParticleAndFindTrade( int iParticle, float fTimeDelta );
	void UpdateParticleDuringTrade( int iParticle, float flTimeDelta );

	inline int					GetSmokeParticleIndex(int x, int y, int z)	{return z*m_xCount*m_yCount+y*m_yCount+x;}
	inline SmokeParticleInfo*	GetSmokeParticleInfo(int x, int y, int z)	{return &m_SmokeParticleInfos[GetSmokeParticleIndex(x,y,z)];}
	inline void					GetParticleInfoXYZ(int index, int &x, int &y, int &z)
	{
		z = index / (m_xCount*m_yCount);
		int zIndex = z*m_xCount*m_yCount;
		y = (index - zIndex) / m_yCount;
		int yIndex = y*m_yCount;
		x = index - zIndex - yIndex;
	}

	inline bool					IsValidXYZCoords(int x, int y, int z)
	{
		return x >= 0 && y >= 0 && z >= 0 && x < m_xCount && y < m_yCount && z < m_zCount;
	}

	inline Vector				GetSmokeParticlePos(int x, int y, int z)	
	{
		return m_SmokeBasePos + 
			Vector( ((float)x / (m_xCount-1)) * m_SpacingRadius * 2 - m_SpacingRadius,
				((float)y / (m_yCount-1)) * m_SpacingRadius * 2 - m_SpacingRadius,
				((float)z / (m_zCount-1)) * m_SpacingRadius * 2 - m_SpacingRadius);
	}

	inline Vector				GetSmokeParticlePosIndex(int index)
	{
		int x, y, z;
		GetParticleInfoXYZ(index, x, y, z);
		return GetSmokeParticlePos(x, y, z);
	}

	inline const Vector&		GetPos()	{ return GetAbsOrigin(); }

	// Start filling the smoke volume (and stop the smoke trail).
	void						FillVolume();

// State variables from server.
public:
	
	[[= ks::reflect::Net{} ]]
	[[= ks::reflect::Proxy<RecvProxy_CurrentStage, ks::reflect::WireSide::Recv>{} ]] unsigned char		m_CurrentStage;
	Vector				m_SmokeBasePos;

	// What time the effect was initially created
	[[= ks::reflect::As{ FIELD_TIME } ]] [[= ks::reflect::Net{} ]] float				m_flSpawnTime;

	// It will fade out during this time.
	[[= ks::reflect::Net{} ]] float				m_FadeStartTime;
	[[= ks::reflect::Net{} ]] float				m_FadeEndTime;
	float				m_FadeAlpha;	// Calculated from the fade start/end times each frame.

	// Color driven by grenade weapon description.
	[[= ks::reflect::Net{} ]] Vector				m_MinColor;
	[[= ks::reflect::Net{} ]] Vector				m_MaxColor;

	// Used during rendering.. active dlights.
	class CActiveLight
	{
	public:
		Vector m_vColor;
		Vector m_vOrigin;
		float m_flRadiusSqr;
	};
	CActiveLight		m_ActiveLights[MAX_DLIGHTS];
	int					m_nActiveLights;

private:
						CParticleSmokeGrenade( const CParticleSmokeGrenade & );

	bool				m_bStarted;
	bool				m_bVolumeFilled;
	PMaterialHandle		m_MaterialHandles[NUM_MATERIAL_HANDLES];

	SmokeParticleInfo	m_SmokeParticleInfos[NUM_PARTICLES_PER_DIMENSION*NUM_PARTICLES_PER_DIMENSION*NUM_PARTICLES_PER_DIMENSION];
	int					m_xCount, m_yCount, m_zCount;
	float				m_SpacingRadius;

	float				m_ExpandTimeCounter;	// How long since we started expanding.	
	float				m_ExpandRadius;			// How large is our radius.

	CSmokeTrail		m_SmokeTrail;
};

// Expose to the particle app.

// Datatable..
IMPLEMENT_REFLECT_CLIENTCLASS( CParticleSmokeGrenade, DT_ParticleSmokeGrenade, ParticleSmokeGrenade )

// ------------------------------------------------------------------------- //
// Helpers.
// ------------------------------------------------------------------------- //

static inline void InterpColor(unsigned char dest[4], unsigned char src1[4], unsigned char src2[4], float percent)
{
	dest[0] = (unsigned char)(src1[0] + (src2[0] - src1[0]) * percent);
	dest[1] = (unsigned char)(src1[1] + (src2[1] - src1[1]) * percent);
	dest[2] = (unsigned char)(src1[2] + (src2[2] - src1[2]) * percent);
}

static inline int GetWorldPointContents(const Vector &vPos)
{
		return enginetrace->GetPointContents( vPos );
}

static inline void WorldTraceLine( const Vector &start, const Vector &end, int contentsMask, trace_t *trace )
{
		UTIL_TraceLine(start, end, contentsMask, nullptr, COLLISION_GROUP_NONE, trace);
}

static inline Vector EngineGetLightForPoint(const Vector &vPos)
{
		return engine->GetLightForPoint(vPos, true);
}

static inline const Vector& EngineGetVecRenderOrigin()
{
		return CurrentViewOrigin();
}

static inline float& EngineGetSmokeFogOverlayAlpha()
{
		return g_SmokeFogOverlayAlpha;
}

static inline void EngineAddSmokeFogOverlayColor( Vector &color )
{
	g_SmokeFogOverlayColor += color;
}

static inline CBaseEntity* ParticleGetEntity(int index)
{
		return cl_entitylist->GetEnt(index);
}

// ------------------------------------------------------------------------- //
// ParticleMovieExplosion
// ------------------------------------------------------------------------- //
CParticleSmokeGrenade::CParticleSmokeGrenade()
{
	memset(m_MaterialHandles, 0, sizeof(m_MaterialHandles));

	m_MinColor.Init(0.5, 0.5, 0.5);
	m_MaxColor.Init(0.6, 0.6, 0.6 );

	m_nActiveLights = 0;
	m_ExpandRadius = 0;
	m_ExpandTimeCounter = 0;
	m_FadeStartTime = 0;
	m_FadeEndTime = 0;
	m_flSpawnTime = 0;
	m_bVolumeFilled = false;
	m_CurrentStage = 0;

	m_bStarted = false;
}

CParticleSmokeGrenade::~CParticleSmokeGrenade()
{
	ParticleMgr()->RemoveEffect( &m_ParticleEffect );
}

void CParticleSmokeGrenade::SetParams(
	)
{
}

void CParticleSmokeGrenade::OnDataChanged( DataUpdateType_t updateType )
{
	CBaseEntity::OnDataChanged(updateType);

	if(updateType == DATA_UPDATE_CREATED )
	{
		Start(ParticleMgr(), nullptr);
	}
}

void CParticleSmokeGrenade::Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs)
{
	if(!pParticleMgr->AddEffect( &m_ParticleEffect, this ))
		return;
	
	m_SmokeTrail.Start(pParticleMgr, pArgs);

	m_SmokeTrail.m_ParticleLifetime = 0.5;
	m_SmokeTrail.SetSpawnRate(40);
	m_SmokeTrail.m_MinSpeed = 0;
	m_SmokeTrail.m_MaxSpeed = 0;
	m_SmokeTrail.m_StartSize = 3;
	m_SmokeTrail.m_EndSize = 10;
	m_SmokeTrail.m_SpawnRadius = 0;

	m_SmokeTrail.SetLocalOrigin( GetAbsOrigin() );

	for(int i=0; i < NUM_MATERIAL_HANDLES; i++)
	{
		char str[256];
		V_snprintf(str, sizeof( str ), "particle/particle_smokegrenade%d", i+1);
		m_MaterialHandles[i] = m_ParticleEffect.FindOrAddMaterial(str);
	}

	if( m_CurrentStage == 2 )
	{
		FillVolume();
	}

	// Go straight into "fill volume" mode if they want.
	if(pArgs)
	{
		if(pArgs->FindArg("-FillVolume"))
		{
			FillVolume();
		}
	}

	m_bStarted = true;
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();

	if ( pPlayer )
	{
		 pPlayer->m_SmokeGrenades.push_back( this );
	}
		 
}

void CParticleSmokeGrenade::ClientThink()
{
	if ( m_CurrentStage == 1 )
	{
		// Add our influence to the global smoke fog alpha.
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
		float testDist = (MainViewOrigin(nSlot) - m_SmokeBasePos).Length();

		float fadeEnd = m_ExpandRadius;

		// The center of the smoke cloud that always gives full fog overlay
		float flCoreDistance = fadeEnd * 0.15;
		
		if(testDist < fadeEnd)
		{		

			float smokeAlpha = 0.0f;
			
			if( testDist < flCoreDistance )
			{
				smokeAlpha = m_FadeAlpha;
			}
			else
			{
				smokeAlpha = (1 - ( testDist - flCoreDistance ) / ( fadeEnd - flCoreDistance ) ) * m_FadeAlpha;
			}

			EngineGetSmokeFogOverlayAlpha() += smokeAlpha;
			
			// The fog overlay color is half the intensity of the maximum color value.  We multiply it by the smoke's fade alpha and
			// later divide the sum by the total alpha value to get the average color weighted by alpha influence.
			// This lets us combine multiple colored smoke effects together should the situation arise.
			Vector color = m_MaxColor * 0.5f * smokeAlpha;
			EngineAddSmokeFogOverlayColor(color);
			
		}	
	}
}

void CParticleSmokeGrenade::UpdateSmokeTrail( float fTimeDelta )
{
	CBaseEntity *pAimEnt = GetFollowedEntity();
	if ( pAimEnt )
	{
		Vector forward, right, up;

		// Update the smoke particle color.
		if(m_CurrentStage == 0)
		{
			m_SmokeTrail.m_StartColor = EngineGetLightForPoint(GetAbsOrigin()) * 0.5f;
			m_SmokeTrail.m_EndColor = m_SmokeTrail.m_StartColor;
		}

		// Spin the smoke trail.
		AngleVectors(pAimEnt->GetAbsAngles(), &forward, &right, &up);
		m_SmokeTrail.m_VelocityOffset = forward * 30 + GetAbsVelocity();

		m_SmokeTrail.SetLocalOrigin( GetAbsOrigin() );
		m_SmokeTrail.Update(fTimeDelta);
	}	
}

void CParticleSmokeGrenade::UpdateParticleDuringTrade( int iParticle, float fTimeDelta )
{
	SmokeParticleInfo *pInfo = &m_SmokeParticleInfos[iParticle];
	SmokeParticleInfo *pOther = &m_SmokeParticleInfos[pInfo->m_TradeIndex];
	contract_assert( pOther->m_TradeIndex == iParticle );
	
	// This makes sure the trade only gets updated once per frame.
	if(pInfo < pOther)
	{
		// Increment the trade clock..
		pInfo->m_TradeClock = (pOther->m_TradeClock += fTimeDelta);
		int x, y, z;
		GetParticleInfoXYZ(iParticle, x, y, z);
		Vector myPos = GetSmokeParticlePos(x, y, z) - m_SmokeBasePos;
		
		int otherX, otherY, otherZ;
		GetParticleInfoXYZ(pInfo->m_TradeIndex, otherX, otherY, otherZ);
		Vector otherPos = GetSmokeParticlePos(otherX, otherY, otherZ) - m_SmokeBasePos;

		// Is the trade finished?
		if(pInfo->m_TradeClock >= pInfo->m_TradeDuration)
		{
			pInfo->m_TradeIndex = pOther->m_TradeIndex = -1;
			
			pInfo->m_pParticle->m_Pos = otherPos;
			pOther->m_pParticle->m_Pos = myPos;

			SmokeGrenadeParticle *temp = pInfo->m_pParticle;
			pInfo->m_pParticle = pOther->m_pParticle;
			pOther->m_pParticle = temp;
		}
		else
		{			
			// Ok, move them closer.
			float percent = (float)std::cos(pInfo->m_TradeClock * 2 * 1.57079632f / pInfo->m_TradeDuration);
			percent = percent * 0.5 + 0.5;
			
			pInfo->m_pParticle->m_FadeAlpha  = pInfo->m_FadeAlpha + (pOther->m_FadeAlpha - pInfo->m_FadeAlpha) * (1 - percent);
			pOther->m_pParticle->m_FadeAlpha = pInfo->m_FadeAlpha + (pOther->m_FadeAlpha - pInfo->m_FadeAlpha) * percent;

			InterpColor(pInfo->m_pParticle->m_Color,  pInfo->m_Color, pOther->m_Color, 1-percent);
			InterpColor(pOther->m_pParticle->m_Color, pInfo->m_Color, pOther->m_Color, percent);

			pInfo->m_pParticle->m_Pos  = myPos + (otherPos - myPos) * (1 - percent);
			pOther->m_pParticle->m_Pos = myPos + (otherPos - myPos) * percent;
		}
	}
}

void CParticleSmokeGrenade::UpdateParticleAndFindTrade( int iParticle, float fTimeDelta )
{
	SmokeParticleInfo *pInfo = &m_SmokeParticleInfos[iParticle];

	pInfo->m_pParticle->m_FadeAlpha = pInfo->m_FadeAlpha;
	pInfo->m_pParticle->m_Color[0] = pInfo->m_Color[0];
	pInfo->m_pParticle->m_Color[1] = pInfo->m_Color[1];
	pInfo->m_pParticle->m_Color[2] = pInfo->m_Color[2];

	// Is there an adjacent one that's not trading?
	int x, y, z;
	GetParticleInfoXYZ(iParticle, x, y, z);

	int xCountOffset = RandomInt( 0, VALVE_RAND_MAX );
	int yCountOffset = RandomInt( 0, VALVE_RAND_MAX );
	int zCountOffset = RandomInt( 0, VALVE_RAND_MAX );

	bool bFound = false;
	for(int xCount=0; xCount < 3 && !bFound; xCount++)
	{
		for(int yCount=0; yCount < 3 && !bFound; yCount++)
		{
			for(int zCount=0; zCount < 3; zCount++)
			{
				int testX = x + g_OffsetLookup[(xCount+xCountOffset) % 3];
				int testY = y + g_OffsetLookup[(yCount+yCountOffset) % 3];
				int testZ = z + g_OffsetLookup[(zCount+zCountOffset) % 3];

				if(testX == x && testY == y && testZ == z)
					continue;

				if(IsValidXYZCoords(testX, testY, testZ))
				{
					SmokeParticleInfo *pOther = GetSmokeParticleInfo(testX, testY, testZ);
					if(pOther->m_pParticle && pOther->m_TradeIndex == -1)
					{
						// Ok, this one is looking to trade also.
						pInfo->m_TradeIndex = GetSmokeParticleIndex(testX, testY, testZ);
						pOther->m_TradeIndex = iParticle;
						pInfo->m_TradeClock = pOther->m_TradeClock = 0;
						pInfo->m_TradeDuration = FRand(TRADE_DURATION_MIN, TRADE_DURATION_MAX);
						
						bFound = true;
						break;
					}
				}
			}
		}
	}
}

void CParticleSmokeGrenade::Update(float fTimeDelta)
{
	float flLifetime = gpGlobals->curtime - m_flSpawnTime;

	// Update the smoke trail.
	UpdateSmokeTrail( fTimeDelta );
	
	// Update our fade alpha.
	if(flLifetime < m_FadeStartTime)
	{
		m_FadeAlpha = 1;
	}
	else if(flLifetime < m_FadeEndTime)
	{
		float fadePercent = (flLifetime - m_FadeStartTime) / (m_FadeEndTime - m_FadeStartTime);
		m_FadeAlpha = std::cos(fadePercent * 3.14159) * 0.5 + 0.5;
	}
	else
	{
		m_FadeAlpha = 0;
	}

	// Scale by the amount the sphere has grown.
	m_FadeAlpha *= m_ExpandRadius / (m_SpacingRadius*2);

	// Update our bbox.
	Vector vMins = m_SmokeBasePos - Vector( m_SpacingRadius, m_SpacingRadius, m_SpacingRadius );
	Vector vMaxs = m_SmokeBasePos + Vector( m_SpacingRadius, m_SpacingRadius, m_SpacingRadius );
	m_ParticleEffect.SetBBox( vMins, vMaxs );

	// Update the current light list.
	UpdateDynamicLightList( vMins, vMaxs );

	if(m_CurrentStage == 1)
	{
		// Update the expanding sphere.
		m_ExpandTimeCounter = flLifetime;
		if(m_ExpandTimeCounter > SMOKESPHERE_EXPAND_TIME)
			m_ExpandTimeCounter = SMOKESPHERE_EXPAND_TIME;

		m_ExpandRadius = (m_SpacingRadius*2) * (float)std::sin(m_ExpandTimeCounter * std::numbers::pi * 0.5 / SMOKESPHERE_EXPAND_TIME);
	
		// Update all the moving traders and establish new ones.
		int nTotal = m_xCount * m_yCount * m_zCount;
		for(int i=0; i < nTotal; i++)
		{
			SmokeParticleInfo *pInfo = &m_SmokeParticleInfos[i];

			if(!pInfo->m_pParticle)
				continue;
		
			if(pInfo->m_TradeIndex == -1)
			{
				UpdateParticleAndFindTrade( i, fTimeDelta );
			}
			else
			{
				UpdateParticleDuringTrade( i, fTimeDelta );
			}
		}
	}

	m_SmokeBasePos = GetPos();
}

void CParticleSmokeGrenade::UpdateDynamicLightList( const Vector &vMins, const Vector &vMaxs )
{
	dlight_t *lights[MAX_DLIGHTS];
	int nLights = effects->CL_GetActiveDLights( lights );
	m_nActiveLights = 0;
	for ( int i=0; i < nLights; i++ )
	{
		dlight_t *pIn = lights[i];
		if ( pIn->origin.x + pIn->radius <= vMins.x || 
			 pIn->origin.y + pIn->radius <= vMins.y || 
			 pIn->origin.z + pIn->radius <= vMins.z || 
			 pIn->origin.x - pIn->radius >= vMaxs.x || 
			 pIn->origin.y - pIn->radius >= vMaxs.y || 
			 pIn->origin.z - pIn->radius >= vMaxs.z )
		{
		}
		else
		{
			CActiveLight *pOut = &m_ActiveLights[m_nActiveLights];
			if ( (pIn->color.r != 0 || pIn->color.g != 0 || pIn->color.b != 0) && pIn->color.exponent != 0 )
			{
				ColorRGBExp32ToVector( pIn->color, pOut->m_vColor );
				pOut->m_vColor /= 255.0f;
				pOut->m_flRadiusSqr = (pIn->radius + SMOKEPARTICLE_SIZE) * (pIn->radius + SMOKEPARTICLE_SIZE);
				pOut->m_vOrigin = pIn->origin;
				++m_nActiveLights;
			}
		}
	}
}

void CParticleSmokeGrenade::ApplyDynamicLight( const Vector &vParticlePos, Vector &color )
{
	if ( m_nActiveLights )
	{
		for ( int i=0; i < m_nActiveLights; i++ )
		{
			CActiveLight *pLight = &m_ActiveLights[i];

			float flDistSqr = (vParticlePos - pLight->m_vOrigin).LengthSqr();
			if ( flDistSqr < pLight->m_flRadiusSqr )
			{
				color += pLight->m_vColor * (1 - flDistSqr / pLight->m_flRadiusSqr) * 0.1f;
			}
		}
	
		// Rescale the color..
		float flMax = std::max( color.x, std::max( color.y, color.z ) );
		if ( flMax > 1 )
		{
			color /= flMax;
		}
	}
}

void CParticleSmokeGrenade::RenderParticles( CParticleRenderIterator *pIterator )
{
	const SmokeGrenadeParticle *pParticle = (const SmokeGrenadeParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		Vector vWorldSpacePos = m_SmokeBasePos + pParticle->m_Pos;

		float sortKey;

		// Draw.
		float len = pParticle->m_Pos.Length();
		if ( len > m_ExpandRadius )
		{
			Vector vTemp;
			TransformParticle(ParticleMgr()->GetModelView(), vWorldSpacePos, vTemp);
			sortKey = vTemp.z;		
		}
		else
		{
			// This smooths out the growing sphere. Rather than having particles appear in one spot as the sphere
			// expands, they stay at the borders.
			Vector renderPos;
			if(len > m_ExpandRadius * 0.5f)
			{
				renderPos = m_SmokeBasePos + (pParticle->m_Pos * (m_ExpandRadius * 0.5f)) / len;
			}
			else
			{
				renderPos = vWorldSpacePos;
			}		

			// Figure out the alpha based on where it is in the sphere.
			float alpha = 1 - len / m_ExpandRadius;
			
			// This changes the ramp to be very solid in the core, then taper off.
			static float testCutoff=0.7;
			if(alpha > testCutoff)
			{
				alpha = 1;
			}
			else
			{
				// at testCutoff it's 1, at 0, it's 0
				alpha = alpha / testCutoff;
			}

			// Fade out globally.
			alpha *= m_FadeAlpha;

			// Apply the precalculated fade alpha from world geometry.
			alpha *= pParticle->m_FadeAlpha;

			// TODO: optimize this whole routine!
			Vector color = m_MinColor + (m_MaxColor - m_MinColor) * (pParticle->m_ColorInterp / 255.1f);
			color.x *= pParticle->m_Color[0] / 255.0f;
			color.y *= pParticle->m_Color[1] / 255.0f;
			color.z *= pParticle->m_Color[2] / 255.0f;

			// Lighting.
			ApplyDynamicLight( renderPos, color );
			
			Vector tRenderPos;
			TransformParticle(ParticleMgr()->GetModelView(), renderPos, tRenderPos);
			sortKey = tRenderPos.z;

			RenderParticle_ColorSizeAngle(
				pIterator->GetParticleDraw(),
				tRenderPos,
				color,
				alpha * GetAlphaDistanceFade(tRenderPos, 100, 200),	// Alpha
				SMOKEPARTICLE_SIZE,
				pParticle->m_CurRotation
				);
		}

		pParticle = (SmokeGrenadeParticle*)pIterator->GetNext( sortKey );
	}
}

void CParticleSmokeGrenade::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	SmokeGrenadeParticle *pParticle = (SmokeGrenadeParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		pParticle->m_CurRotation += pParticle->m_RotationSpeed * pIterator->GetTimeDelta();
		pParticle = (SmokeGrenadeParticle*)pIterator->GetNext();
	}
}

void CParticleSmokeGrenade::NotifyRemove()
{
	m_xCount = m_yCount = m_zCount = 0;

	CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();

	if ( pPlayer )
	{
		 ks::erase_first( pPlayer->m_SmokeGrenades, this );
	}

}

void CParticleSmokeGrenade::GetParticlePosition( Particle *pParticle, Vector& worldpos )
{
	worldpos = pParticle->m_Pos + m_SmokeBasePos;
}

void CParticleSmokeGrenade::RecvProxy_CurrentStage(  const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CParticleSmokeGrenade *pGrenade = (CParticleSmokeGrenade*)pStruct;
	contract_assert( pOut == &pGrenade->m_CurrentStage );

	if ( pGrenade && pGrenade->m_CurrentStage == 0 && pData->m_Value.m_Int == 1 )
	{
		if( pGrenade->m_bStarted )
			pGrenade->FillVolume();
		else
			pGrenade->m_CurrentStage = 2;
	}
}

void CParticleSmokeGrenade::FillVolume()
{
	m_CurrentStage = 1;
	m_SmokeBasePos = GetPos();
	m_SmokeTrail.SetEmit(false);
	m_ExpandTimeCounter = m_ExpandRadius = 0;
	m_bVolumeFilled = true;

	// Spawn all of our particles.
	float overlap = SMOKEPARTICLE_OVERLAP;

	m_SpacingRadius = (SMOKEGRENADE_PARTICLERADIUS - overlap) * NUM_PARTICLES_PER_DIMENSION * 0.5f;
	m_xCount = m_yCount = m_zCount = NUM_PARTICLES_PER_DIMENSION;

	float invNumPerDimX = 1.0f / (m_xCount-1);
	float invNumPerDimY = 1.0f / (m_yCount-1);
	float invNumPerDimZ = 1.0f / (m_zCount-1);

	Vector vPos;
	for(int x=0; x < m_xCount; x++)
	{
		vPos.x = m_SmokeBasePos.x + ((float)x * invNumPerDimX) * m_SpacingRadius * 2 - m_SpacingRadius;

		for(int y=0; y < m_yCount; y++)
		{
			vPos.y = m_SmokeBasePos.y + ((float)y * invNumPerDimY) * m_SpacingRadius * 2 - m_SpacingRadius;
							  
			for(int z=0; z < m_zCount; z++)
			{
				vPos.z = m_SmokeBasePos.z + ((float)z * invNumPerDimZ) * m_SpacingRadius * 2 - m_SpacingRadius;

				// Don't spawn and simulate particles that are inside a wall
				int contents = enginetrace->GetPointContents( vPos );

				if( contents & CONTENTS_SOLID )
				{
					continue;
				}

				if(SmokeParticleInfo *pInfo = GetSmokeParticleInfo(x,y,z))
				{
					// MD 11/10/03: disabled this because we weren't getting coverage near the ground.
					// If we want it back in certain cases, we can make it a flag.
					/*int contents = GetWorldPointContents(vPos);
					if(false && (contents & CONTENTS_SOLID))
					{
						pInfo->m_pParticle = NULL;
					}
					else
					*/
					{
						SmokeGrenadeParticle *pParticle = 
							(SmokeGrenadeParticle*)m_ParticleEffect.AddParticle(sizeof(SmokeGrenadeParticle), m_MaterialHandles[rand() % NUM_MATERIAL_HANDLES]);

						if(pParticle)
						{
							pParticle->m_Pos = vPos - m_SmokeBasePos; // store its position in local space
							pParticle->m_ColorInterp = (unsigned char)( (rand() * 255) / VALVE_RAND_MAX );
							pParticle->m_RotationSpeed = FRand(-ROTATION_SPEED, ROTATION_SPEED); // Rotation speed.
							pParticle->m_CurRotation = FRand(-6, 6);
						}

						Vector vColor = EngineGetLightForPoint(vPos);
						pInfo->m_Color[0] = (unsigned char)(vColor.x * 255.9f);
						pInfo->m_Color[1] = (unsigned char)(vColor.y * 255.9f);
						pInfo->m_Color[2] = (unsigned char)(vColor.z * 255.9f);

						// Cast some rays and if it's too close to anything, fade its alpha down.
						pInfo->m_FadeAlpha = 1;

						/*for(int i=0; i < NUM_FADE_PLANES; i++)
						{
							trace_t trace;
							WorldTraceLine(vPos, vPos + s_FadePlaneDirections[i] * 100, MASK_SOLID_BRUSHONLY, &trace);
							if(trace.fraction < 1.0f)
							{
								float dist = DotProduct(trace.plane.normal, vPos) - trace.plane.dist;
								if(dist < 0)
								{
									pInfo->m_FadeAlpha = 0;
								}
								else if(dist < SMOKEPARTICLE_SIZE)
								{
									float alphaScale = dist / SMOKEPARTICLE_SIZE;
									alphaScale *= alphaScale * alphaScale;
									pInfo->m_FadeAlpha *= alphaScale;
								}
							}
						}*/

						pInfo->m_pParticle = pParticle;
						pInfo->m_TradeIndex = -1;
					}
				}
			}
		}
	}
}

#undef NUM_FADE_PLANES

class CEffectData;
class CNewParticleEffect;
class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: An entity that spawns and controls a particle system
//-----------------------------------------------------------------------------
void RecvProxy_EffectFlags( const CRecvProxyData *pData, void *pStruct, void *pOut );

class CParticleSystem : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_ParticleSystem", .base = false } ]]
	[[= ks::reflect::From<"m_vecNetworkOrigin", ks::reflect::Net{ .wire = "m_vecOrigin" }>{} ]]
	[[= ks::reflect::From<"m_fEffects", ks::reflect::Net{}, RecvProxy_EffectFlags>{} ]]
	[[= ks::reflect::From<"m_hOwnerEntity", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_hNetworkMoveParent", ks::reflect::Net{ .wire = "moveparent" }, RecvProxy_IntToMoveParent>{} ]]
	[[= ks::reflect::From<"m_iParentAttachment", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_angNetworkAngles", ks::reflect::Net{ .enc = ks::reflect::WireEnc::QAngles, .wire = "m_angRotation" }>{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CParticleSystem, CBaseEntity );
public:
	DECLARE_CLIENTCLASS();

	CParticleSystem( void );

	virtual void PreDataUpdate( DataUpdateType_t updateType );
	virtual void PostDataUpdate( DataUpdateType_t updateType );
	virtual void ClientThink( void );

protected:
	~CParticleSystem( void );

	[[= ks::reflect::Net{} ]] int			m_iEffectIndex;
	[[= ks::reflect::Net{} ]] int			m_nStopType;
	[[= ks::reflect::Net{} ]] bool		m_bActive;
	bool		m_bOldActive;
	[[= ks::reflect::Net{} ]] float		m_flStartTime;	// Time at which the effect started
	[[= ks::reflect::Net{} ]] char		m_szSnapshotFileName[ MAX_PATH ];

	//server controlled control points (variables in particle effects instead of literal follow points)
	[[= ks::reflect::Net{} ]] Vector		m_vServerControlPoints[4];
	[[= ks::reflect::Net{} ]] uint8		m_iServerControlPointAssignments[4];

	ks::tracked_ref< CNewParticleEffect > m_pEffect;
	CParticleSnapshot *m_pSnapshot;

	enum { kMAXCONTROLPOINTS = 63 }; ///< actually one less than the total number of cpoints since 0 is assumed to be me

	// stop types
	enum 
	{
		STOP_NORMAL = 0,
		STOP_DESTROY_IMMEDIATELY,
		STOP_PLAY_ENDCAP,
		NUM_STOP_TYPES
	};
	
	[[= ks::reflect::Net{} ]] EHANDLE		m_hControlPointEnts[kMAXCONTROLPOINTS];
	//	SendPropArray3( SENDINFO_ARRAY3(m_iControlPointParents), SendPropInt( SENDINFO_ARRAY(m_iControlPointParents), 3, SPROP_UNSIGNED ) ),
	[[= ks::reflect::Net{} ]] unsigned char m_iControlPointParents[kMAXCONTROLPOINTS];
};

extern void RecvProxy_EffectFlags( const CRecvProxyData *pData, void *pStruct, void *pOut );

IMPLEMENT_CLIENTCLASS(CParticleSystem, DT_ParticleSystem, CParticleSystem);

IMPLEMENT_REFLECT_TABLE( CParticleSystem, DT_ParticleSystem );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CParticleSystem::CParticleSystem( void )
 :	m_pSnapshot( nullptr )
{
	memset( m_szSnapshotFileName, 0, sizeof( m_szSnapshotFileName ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CParticleSystem::~CParticleSystem( void )
{
	if ( m_pSnapshot )
	{
		delete m_pSnapshot;
		m_pSnapshot = nullptr;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleSystem::PreDataUpdate( DataUpdateType_t updateType )
{
	m_bOldActive = m_bActive;

	BaseClass::PreDataUpdate( updateType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleSystem::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

	// Always restart if just created and updated
	// FIXME: Does this play fairly with PVS?
	if ( updateType == DATA_UPDATE_CREATED )
	{
		// TODO: !!HACK HACK HACK!! .PSF files should be loaded/refcounted through the CParticleSystemMgr (ala .PCFs).
		//       The current code will duplicate a given .PSF file in memory for every info_particle_system that uses it!
		if ( m_szSnapshotFileName[0] )
		{
			m_pSnapshot = new CParticleSnapshot();
			if ( !m_pSnapshot->Unserialize( CFmtStr( "particles/%s.psf", m_szSnapshotFileName ) ) )
			{
				delete m_pSnapshot;
				m_pSnapshot = nullptr;
			}
		}

		if ( m_bActive )
		{
			// Delayed here so that we don't get invalid abs queries on level init with active particle systems
			SetNextClientThink( gpGlobals->curtime );
		}
	}
	else
	{
		if ( m_bOldActive != m_bActive )
		{
			if ( m_bActive )
			{
				// Delayed here so that we don't get invalid abs queries on level init with active particle systems
				SetNextClientThink( gpGlobals->curtime );
			}
			else
			{
				switch( m_nStopType )
				{
				case STOP_NORMAL:
					{
						ParticleProp()->StopEmission();
					}
					break;
				case STOP_DESTROY_IMMEDIATELY:
					{
						ParticleProp()->StopEmissionAndDestroyImmediately();
					}
					break;
				case STOP_PLAY_ENDCAP:
					{
						ParticleProp()->StopEmission( nullptr, false, false, false, true);
					}
					break;
				}
			}
		}

		if( m_bActive && ParticleProp()->IsValidEffect( m_pEffect ) )
		{
			//server controlled control points (variables in particle effects instead of literal follow points)
			for( int i = 0; i != std::size( m_iServerControlPointAssignments ); ++i )
			{
				if( m_iServerControlPointAssignments[i] != 255 )
				{
					m_pEffect->SetControlPoint( m_iServerControlPointAssignments[i], m_vServerControlPoints[i] );
				}
				else
				{
					break;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleSystem::ClientThink( void )
{
	if ( m_bActive )
	{
		const char *pszName = GetParticleSystemNameFromIndex( m_iEffectIndex );
		if ( pszName && pszName[0] )
		{
			CNewParticleEffect *pEffect = ParticleProp()->Create( pszName, PATTACH_ABSORIGIN_FOLLOW );
			m_pEffect = pEffect;
	
			if (pEffect)
			{
				for ( int i = 0 ; i < kMAXCONTROLPOINTS ; ++i )
				{
					CBaseEntity *pOnEntity = m_hControlPointEnts[i].Get();
					if ( pOnEntity )
					{
						ParticleProp()->AddControlPoint( pEffect, i + 1, pOnEntity, PATTACH_ABSORIGIN_FOLLOW );
					}

					contract_assert( ( m_iControlPointParents[i] >= 0 && m_iControlPointParents[i] <= kMAXCONTROLPOINTS ) && "Particle system specified bogus control point parent (%d) for point %d." );

					if (m_iControlPointParents[i] != 0)
					{
						pEffect->SetControlPointParent(i+1, m_iControlPointParents[i]);
					}
				}

				//server controlled control points (variables in particle effects instead of literal follow points)
				for( int i = 0; i != std::size( m_iServerControlPointAssignments ); ++i )
				{
					if( m_iServerControlPointAssignments[i] != 255 )
					{
						pEffect->SetControlPoint( m_iServerControlPointAssignments[i], m_vServerControlPoints[i] );
					}
					else
					{
						break;
					}
				}

				// Attach our particle snapshot if we have one
				contract_assert( m_pSnapshot || !m_szSnapshotFileName[0] ); // m_szSnapshotFileName shouldn't change after the create update
				if ( m_pSnapshot )
				{
					pEffect->SetControlPointSnapshot( 0, m_pSnapshot );
				}

				// NOTE: What we really want here is to compare our lifetime and that of our children and see if this delta is
				//		 already past the end of it, denoting that we're finished.  In that case, just destroy us and be done. -- jdw

				// TODO: This can go when the SkipToTime code below goes
				ParticleProp()->OnParticleSystemUpdated( pEffect, 0.0f );

				// Skip the effect ahead if we're restarting it
				float flTimeDelta = gpGlobals->curtime - m_flStartTime;
				if ( flTimeDelta > 0.01f )
				{
					CVProfScope _( "CParticleSystem::ClientThink SkipToTime", 0, "Particle Simulation", false, BUDGETFLAG_OTHER );
					pEffect->SkipToTime( flTimeDelta );
				}
			}
		}
	}
}

//======================================================================================================================
// PARTICLE SYSTEM DISPATCH EFFECT
//======================================================================================================================
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void StartParticleEffect( const CEffectData &data, int nSplitScreenPlayerSlot /*= -1*/ )
{
	// this needs to be before using data.m_nHitBox, 
	// since that may be a serialized value that's past the end of the current particle system string table
	if ( SuppressingParticleEffects() )
		return; 

	// Don't crash if we're passed an invalid particle system
	if ( data.m_nHitBox == 0 )
		return;

	if ( data.m_fFlags & PARTICLE_DISPATCH_FROM_ENTITY )
	{
		if ( EntityFromHandle( data.m_hEntity ) )
		{
			CBaseEntity *pEnt = CBaseEntity::Instance( data.m_hEntity );
			// commented out assert. dormant entities have their particle system spawns stopped.
			//Assert( pEnt && !pEnt->IsDormant() );
			if ( pEnt && ( !pEnt->IsDormant() || ( data.m_fFlags & PARTICLE_DISPATCH_ALLOW_DORMANT ) ) )
			{
				if ( data.m_fFlags & PARTICLE_DISPATCH_RESET_PARTICLES )
				{
					pEnt->ParticleProp()->StopEmission();
				}

				ks::tracked_ref<CNewParticleEffect> pEffect = pEnt->ParticleProp()->CreatePrecached( data.m_nHitBox, (ParticleAttachment_t)data.m_nDamageType, data.m_nAttachmentIndex );

				if ( pEffect.IsValid() && pEffect->IsValid() )
				{
					pEffect->SetDrawOnlyForSplitScreenUser( nSplitScreenPlayerSlot );
					if ( (ParticleAttachment_t)data.m_nDamageType == PATTACH_CUSTOMORIGIN )
					{
						pEffect->SetSortOrigin( data.m_vOrigin );
						pEffect->SetControlPoint( 0, data.m_vOrigin );
						pEffect->SetControlPoint( 1, data.m_vStart );
						Vector vecForward, vecRight, vecUp;
						AngleVectors( data.m_vAngles, &vecForward, &vecRight, &vecUp );
						pEffect->SetControlPointOrientation( 0, vecForward, vecRight, vecUp );
					}
					else if ( data.m_nOtherEntIndex > 0 )
					{
						CBaseEntity *pOtherEnt = ClientEntityList().GetEnt( data.m_nOtherEntIndex );
					
						if ( pOtherEnt )
						{
							pEnt->ParticleProp()->AddControlPoint( pEffect, 1, pOtherEnt, PATTACH_ABSORIGIN_FOLLOW, nullptr, Vector( 0, 0, 50 ) );
						}
					}
				}
			}
		}
	}	
	else
	{
		CParticleSystemDefinition *pDef = g_pParticleSystemMgr->FindPrecachedParticleSystem( data.m_nHitBox );
		if ( pDef )
		{
			ks::tracked_ref<CNewParticleEffect> pEffect = CNewParticleEffect::CreateOrAggregate( nullptr, pDef, data.m_vOrigin, nullptr, nSplitScreenPlayerSlot );
			if ( pEffect.IsValid() && pEffect->IsValid() )
			{
				pEffect->SetSortOrigin( data.m_vOrigin );
				pEffect->SetControlPoint( 0, data.m_vOrigin );
				pEffect->SetControlPoint( 1, data.m_vStart );
				Vector vecForward, vecRight, vecUp;
				AngleVectors( data.m_vAngles, &vecForward, &vecRight, &vecUp );
				pEffect->SetControlPointOrientation( 0, vecForward, vecRight, vecUp );
			}
		}
		else
		{
			Warning( "StartParticleEffect:  Failed to find precached particle system for %d!!\n", data.m_nHitBox );
		}
	}
}

void ParticleEffectCallback( const CEffectData &data )
{
	// NOTE: This is because this effect doesn't need to participate
	// in the precache validation tests. Particle systems used with this
	// effect must already be precached.
	g_pPrecacheSystem->EndLimitedResourceAccess( );

	// From networking always go draw for all local players
	StartParticleEffect( data, -1 );
}

DECLARE_CLIENT_EFFECT( ParticleEffect, ParticleEffectCallback )

//======================================================================================================================
// PARTICLE SYSTEM STOP EFFECT
//======================================================================================================================
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ParticleEffectStopCallback( const CEffectData &data )
{
	if ( EntityFromHandle( data.m_hEntity ) )
	{
		CBaseEntity *pEnt = CBaseEntity::Instance( data.m_hEntity );
		if ( pEnt )
		{
			if ( data.m_nHitBox > 0 )
			{
				if ( pEnt->IsWorld() )
				{
					if ( data.m_nHitBox > 0 ) 
					{
						CNewParticleEffect::RemoveParticleEffect( data.m_nHitBox );
					}
				}
				else
				{
					CParticleSystemDefinition *pDef = g_pParticleSystemMgr->FindPrecachedParticleSystem( data.m_nHitBox );

					if ( pDef )
					{
						pEnt->ParticleProp()->StopParticlesNamed( pDef->GetName(), true );
					}
				}
			}
			else
			{
				pEnt->ParticleProp()->StopEmission( nullptr, true, true, false, true );
			}
		}
	}
}

DECLARE_CLIENT_EFFECT( ParticleEffectStop, ParticleEffectStopCallback );
