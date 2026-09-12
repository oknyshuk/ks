//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_FUNC_DUST_H
#define C_FUNC_DUST_H

#include "reflect_annotations.h"
#include "dt_recv.h"


#include "c_baseentity.h"
#include "particles_simple.h"
#include "particle_util.h"
#include "bspflags.h"



// ------------------------------------------------------------------------------------ //
// CDustEffect particle renderer.
// ------------------------------------------------------------------------------------ //

class C_Func_Dust;

class CFuncDustParticle : public Particle
{
public:
	Vector		m_vVelocity;
	float		m_flLifetime;
	float		m_flDieTime;
	float		m_flSize;
	color32		m_Color;
};

class CDustEffect : public CParticleEffect
{
public:
	explicit CDustEffect( const char *pDebugName ) : CParticleEffect( pDebugName ) {}

	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );

	C_Func_Dust		*m_pDust;

private:
	CDustEffect( const CDustEffect & ); // not defined, not accessible
};


// ------------------------------------------------------------------------------------ //
// C_Func_Dust class.
// ------------------------------------------------------------------------------------ //

class [[= ks::reflect::NetTable{ .name = "DT_Func_Dust", .base = false } ]]
      [[= ks::reflect::From<"m_nModelIndex", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_Collision", ks::reflect::Net{}>{} ]]
      C_Func_Dust : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_Func_Dust, C_BaseEntity );
	DECLARE_CLIENTCLASS();

						C_Func_Dust();
	virtual				~C_Func_Dust();
	virtual void		OnDataChanged( DataUpdateType_t updateType );
	virtual void		ClientThink();
	virtual bool		ShouldDraw();


private:

	void				AttemptSpawnNewParticle();



// Vars from server.
public:

	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_Int32ToColor32, ks::reflect::WireSide::Recv>{} ]] color32			m_Color;
	[[= ks::reflect::Net{} ]] int				m_SpawnRate;
	
	[[= ks::reflect::Net{} ]] float			m_flSizeMin;
	[[= ks::reflect::Net{} ]] float			m_flSizeMax;

	[[= ks::reflect::Net{} ]] int				m_SpeedMax;

	[[= ks::reflect::Net{} ]] int				m_LifetimeMin;
	[[= ks::reflect::Net{} ]] int				m_LifetimeMax;

	[[= ks::reflect::Net{} ]] int				m_DistMax;

	[[= ks::reflect::Net{} ]] float			m_FallSpeed;	// extra 'gravity'
	[[= ks::reflect::Net{} ]] bool			m_bAffectedByWind;

public:

	[[= ks::reflect::Net{} ]] int				m_DustFlags;	// Combination of DUSTFLAGS_



public:
	CDustEffect		m_Effect;
	PMaterialHandle		m_hMaterial;
	TimedEvent			m_Spawner;

private:
	C_Func_Dust( const C_Func_Dust & ); // not defined, not accessible
};



#endif // C_FUNC_DUST_H
