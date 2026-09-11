//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
// This defines the client-side SmokeTrail entity. It can also be used without
// an entity, in which case you must pass calls to it and set its position each frame.

#ifndef PARTICLE_SMOKETRAIL_H
#define PARTICLE_SMOKETRAIL_H

#include "reflect_annotations.h"

#include "particlemgr.h"
#include "particle_prototype.h"
#include "particle_util.h"
#include "particles_simple.h"
#include "c_baseentity.h"
#include "baseparticleentity.h"

#include "fx_trail.h"

//
// Smoke Trail
//

class [[= ks::reflect::NetTable{ .name = "DT_SmokeTrail" } ]]
      C_SmokeTrail : public C_BaseParticleEntity, public IPrototypeAppEffect
{
public:
	DECLARE_CLASS( C_SmokeTrail, C_BaseParticleEntity );
	DECLARE_CLIENTCLASS();
	
					C_SmokeTrail();
	virtual			~C_SmokeTrail();

public:

	//For attachments
	void			GetAimEntOrigin( IClientEntity *pAttachedTo, Vector *pAbsOrigin, QAngle *pAbsAngles );

	// Enable/disable emission.
	void			SetEmit(bool bEmit);

	// Change the spawn rate.
	void			SetSpawnRate(float rate);


// C_BaseEntity.
public:
	virtual	void	OnDataChanged(DataUpdateType_t updateType);

	virtual void	CleanupToolRecordingState( KeyValues *msg );

// IPrototypeAppEffect.
public:
	virtual void	Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs);

// IParticleEffect.
public:
	virtual void	Update(float fTimeDelta);
	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );


public:
	// Effect parameters. These will assume default values but you can change them.
	[[= ks::reflect::Net{} ]] float			m_SpawnRate;			// How many particles per second.

	[[= ks::reflect::Net{} ]] Vector			m_StartColor;			// Fade between these colors.
	[[= ks::reflect::Net{} ]] Vector			m_EndColor;
	[[= ks::reflect::Net{} ]] float			m_Opacity;

	[[= ks::reflect::Net{} ]] float			m_ParticleLifetime;		// How long do the particles live?
	[[= ks::reflect::Net{} ]] float			m_StopEmitTime;			// When do I stop emitting particles? (-1 = never)
	
	[[= ks::reflect::Net{} ]] float			m_MinSpeed;				// Speed range.
	[[= ks::reflect::Net{} ]] float			m_MaxSpeed;
	
	[[= ks::reflect::Net{} ]] float			m_MinDirectedSpeed;		// Directed speed range.
	[[= ks::reflect::Net{} ]] float			m_MaxDirectedSpeed;

	[[= ks::reflect::Net{} ]] float			m_StartSize;			// Size ramp.
	[[= ks::reflect::Net{} ]] float			m_EndSize;

	[[= ks::reflect::Net{} ]] float			m_SpawnRadius;

	Vector			m_VelocityOffset;		// Emit the particles in a certain direction.

	[[= ks::reflect::Net{} ]] bool			m_bEmit;				// Keep emitting particles?

	[[= ks::reflect::Net{} ]] int				m_nAttachment;

private:
	C_SmokeTrail( const C_SmokeTrail & );

	PMaterialHandle	m_MaterialHandle[2];
	TimedEvent		m_ParticleSpawn;

	CParticleMgr	*m_pParticleMgr;
	CSmartPtr<CSimpleEmitter> m_pSmokeEmitter;
};

//==================================================
// C_RocketTrail
//==================================================

class [[= ks::reflect::NetTable{ .name = "DT_RocketTrail" } ]]
      C_RocketTrail : public C_BaseParticleEntity, public IPrototypeAppEffect
{
public:
	DECLARE_CLASS( C_RocketTrail, C_BaseParticleEntity );
	DECLARE_CLIENTCLASS();
	
					C_RocketTrail();
	virtual			~C_RocketTrail();

public:

	//For attachments
	void			GetAimEntOrigin( IClientEntity *pAttachedTo, Vector *pAbsOrigin, QAngle *pAbsAngles );

	// Enable/disable emission.
	void			SetEmit(bool bEmit);

	// Change the spawn rate.
	void			SetSpawnRate(float rate);


// C_BaseEntity.
public:
	virtual	void	OnDataChanged(DataUpdateType_t updateType);

// IPrototypeAppEffect.
public:
	virtual void	Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs);

// IParticleEffect.
public:
	virtual void	Update(float fTimeDelta);
	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );


public:
	// Effect parameters. These will assume default values but you can change them.
	[[= ks::reflect::Net{} ]] float			m_SpawnRate;			// How many particles per second.

	[[= ks::reflect::Net{} ]] Vector			m_StartColor;			// Fade between these colors.
	[[= ks::reflect::Net{} ]] Vector			m_EndColor;
	[[= ks::reflect::Net{} ]] float			m_Opacity;

	[[= ks::reflect::Net{} ]] float			m_ParticleLifetime;		// How long do the particles live?
	[[= ks::reflect::Net{} ]] float			m_StopEmitTime;			// When do I stop emitting particles? (-1 = never)
	
	[[= ks::reflect::Net{} ]] float			m_MinSpeed;				// Speed range.
	[[= ks::reflect::Net{} ]] float			m_MaxSpeed;
	
	[[= ks::reflect::Net{} ]] float			m_StartSize;			// Size ramp.
	[[= ks::reflect::Net{} ]] float			m_EndSize;

	[[= ks::reflect::Net{} ]] float			m_SpawnRadius;

	Vector			m_VelocityOffset;		// Emit the particles in a certain direction.

	[[= ks::reflect::Net{} ]] bool			m_bEmit;				// Keep emitting particles?
	[[= ks::reflect::Net{} ]] bool			m_bDamaged;				// Has been shot down (should be on fire, etc)

	[[= ks::reflect::Net{} ]] int				m_nAttachment;

	Vector			m_vecLastPosition;		// Last known position of the rocket
	[[= ks::reflect::Net{} ]] float			m_flFlareScale;			// Size of the flare

private:
	C_RocketTrail( const C_RocketTrail & );

	PMaterialHandle	m_MaterialHandle[2];
	TimedEvent		m_ParticleSpawn;

	CParticleMgr	*m_pParticleMgr;
	CSmartPtr<CSimpleEmitter> m_pRocketEmitter;
};

class SporeSmokeEffect;


//==================================================
// SporeEffect
//==================================================

class SporeEffect : public CSimpleEmitter
{
public:
	explicit 				SporeEffect( const char *pDebugName );
	static SporeEffect*		Create( const char *pDebugName );

	virtual void			UpdateVelocity( SimpleParticle *pParticle, float timeDelta );
	virtual Vector			UpdateColor( const SimpleParticle *pParticle );
	virtual float			UpdateAlpha( const SimpleParticle *pParticle );

private:
	SporeEffect( const SporeEffect & );
};

//==================================================
// C_SporeExplosion
//==================================================

class [[= ks::reflect::NetTable{ .name = "DT_SporeExplosion" } ]]
      C_SporeExplosion : public C_BaseParticleEntity, public IPrototypeAppEffect
{
public:
	DECLARE_CLASS( C_SporeExplosion, C_BaseParticleEntity );
	DECLARE_CLIENTCLASS();
	
	C_SporeExplosion( void );
	virtual	~C_SporeExplosion( void );

public:

// C_BaseEntity
public:
	virtual	void	OnDataChanged( DataUpdateType_t updateType );

// IPrototypeAppEffect
public:
	virtual void	Start( CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs );

// IParticleEffect
public:
	virtual void	Update( float fTimeDelta );
	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );


public:
	[[= ks::reflect::Net{} ]] float	m_flSpawnRate;
	[[= ks::reflect::Net{} ]] float	m_flParticleLifetime;
	[[= ks::reflect::Net{} ]] float	m_flStartSize;
	[[= ks::reflect::Net{} ]] float	m_flEndSize;
	[[= ks::reflect::Net{} ]] float	m_flSpawnRadius;
	float	m_flPreviousSpawnRate;

	[[= ks::reflect::Net{} ]] bool	m_bEmit;
	[[= ks::reflect::Net{} ]] bool	m_bDontRemove;

private:
	C_SporeExplosion( const C_SporeExplosion & );

	void	AddParticles( void );

	PMaterialHandle		m_hMaterial;
	TimedEvent			m_teParticleSpawn;

	SporeEffect			*m_pSporeEffect;
	CParticleMgr		*m_pParticleMgr;
};

//
// Particle trail
//

class CSmokeParticle;

class [[= ks::reflect::NetTable{ .name = "DT_FireTrail" } ]]
      [[= ks::reflect::From<"m_nAttachment", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_flLifetime", ks::reflect::Net{}>{} ]]
      C_FireTrail : public C_ParticleTrail
{
public:
	DECLARE_CLASS( C_FireTrail, C_ParticleTrail );
	DECLARE_CLIENTCLASS();

	C_FireTrail( void );
	virtual ~C_FireTrail( void );

	virtual void	Start( CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs );
	virtual void	Update( float fTimeDelta );

private:

	enum
	{
		// Smoke
		FTRAIL_SMOKE1,
		FTRAIL_SMOKE2,

		// Large flame
		FTRAIL_FLAME1,
		FTRAIL_FLAME2,
		FTRAIL_FLAME3,
		FTRAIL_FLAME4,
		FTRAIL_FLAME5,

		NUM_FTRAIL_MATERIALS
	};

	CSmartPtr<CSimpleEmitter>	m_pTrailEmitter;
	CSmartPtr<CSmokeParticle>	m_pSmokeEmitter;

	PMaterialHandle				m_hMaterial[NUM_FTRAIL_MATERIALS];

	Vector						m_vecLastPosition;

	C_FireTrail( const C_FireTrail & );
};





//-----------------------------------------------------------------------------
// Purpose:  High drag, non color changing particle
//-----------------------------------------------------------------------------


class CDustFollower : public CSimpleEmitter
{
public:
	
	explicit CDustFollower( const char *pDebugName ) : CSimpleEmitter( pDebugName ) {}
	
	//Create
	static CDustFollower *Create( const char *pDebugName )
	{
		return new CDustFollower( pDebugName );
	}

	//Alpha
	virtual float UpdateAlpha( const SimpleParticle *pParticle )
	{
		return ( ((float)pParticle->m_uchStartAlpha/255.0f) * sin( M_PI * (pParticle->m_flLifetime / pParticle->m_flDieTime) ) );
	}

	virtual	void	UpdateVelocity( SimpleParticle *pParticle, float timeDelta )
	{
		pParticle->m_vecVelocity = pParticle->m_vecVelocity * ExponentialDecay( 0.3, timeDelta );
	}

	//Roll
	virtual	float UpdateRoll( SimpleParticle *pParticle, float timeDelta )
	{
		pParticle->m_flRoll += pParticle->m_flRollDelta * timeDelta;
		
		pParticle->m_flRollDelta *= ExponentialDecay( 0.5, timeDelta );

		return pParticle->m_flRoll;
	}

private:
	CDustFollower( const CDustFollower & );
};







//==================================================
// C_DustTrail
//==================================================

class [[= ks::reflect::NetTable{ .name = "DT_DustTrail" } ]]
      C_DustTrail : public C_BaseParticleEntity, public IPrototypeAppEffect
{
public:
	DECLARE_CLASS( C_DustTrail, C_BaseParticleEntity );
	DECLARE_CLIENTCLASS();
	
					C_DustTrail();
	virtual			~C_DustTrail();

public:

	// Enable/disable emission.
	void			SetEmit(bool bEmit);

	// Change the spawn rate.
	void			SetSpawnRate(float rate);


// C_BaseEntity.
public:
	virtual	void	OnDataChanged(DataUpdateType_t updateType);

	virtual void	CleanupToolRecordingState( KeyValues *msg );

// IPrototypeAppEffect.
public:
	virtual void	Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs);

// IParticleEffect.
public:
	virtual void	Update(float fTimeDelta);
	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );


public:
	// Effect parameters. These will assume default values but you can change them.
	[[= ks::reflect::Net{} ]] float			m_SpawnRate;			// How many particles per second.

	[[= ks::reflect::Net{} ]] Vector			m_Color;
	[[= ks::reflect::Net{} ]] float			m_Opacity;

	[[= ks::reflect::Net{} ]] float			m_ParticleLifetime;		// How long do the particles live?
	float			m_StartEmitTime;		// When did I start emitting particles?
	[[= ks::reflect::Net{} ]] float			m_StopEmitTime;			// When do I stop emitting particles? (-1 = never)
	
	[[= ks::reflect::Net{} ]] float			m_MinSpeed;				// Speed range.
	[[= ks::reflect::Net{} ]] float			m_MaxSpeed;
	
	[[= ks::reflect::Net{} ]] float			m_MinDirectedSpeed;		// Directed speed range.
	[[= ks::reflect::Net{} ]] float			m_MaxDirectedSpeed;

	[[= ks::reflect::Net{} ]] float			m_StartSize;			// Size ramp.
	[[= ks::reflect::Net{} ]] float			m_EndSize;

	[[= ks::reflect::Net{} ]] float			m_SpawnRadius;

	Vector			m_VelocityOffset;		// Emit the particles in a certain direction.

	[[= ks::reflect::Net{} ]] bool			m_bEmit;				// Keep emitting particles?

private:
	C_DustTrail( const C_DustTrail & );

#define DUSTTRAIL_MATERIALS 16
	PMaterialHandle	m_MaterialHandle[DUSTTRAIL_MATERIALS];
	TimedEvent		m_ParticleSpawn;

	CParticleMgr	*m_pParticleMgr;
	CSmartPtr<CSimpleEmitter> m_pDustEmitter;
};

#endif
