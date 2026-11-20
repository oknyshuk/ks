// Copyright (c) 2005 Turtle Rock Studios, Inc. - All Rights Reserved

module;
#include "client_class.h"
#include "declare_class.h"
#include "edict.h"
#include "engine/ivmodelinfo.h"
#include "materialsystem/imesh.h"
#include "reflect_annotations.h"
module ks.client:cstrike15.Effects.clientinferno;

import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import ks.tier1.tracked_ref;
import :baseentity;
import :util;
import :particlemgr;

class CNewParticleEffect;
class CBaseEntity;

using namespace ::ks::reflect;


// ClientInferno.cpp
// Render client-side Inferno effects
// Author: Michael Booth, February 2005

//-----------------------------------------------------------------------------------------------
/**
 * The client-side Inferno effect
 */
class CInferno : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_Inferno" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CInferno, CBaseEntity );
	DECLARE_CLIENTCLASS();

	CInferno();
	virtual ~CInferno();

	virtual void Spawn( void );
	virtual void ClientThink();

	virtual int DrawModel( int flags, const RenderableInstance_t &instance );
	virtual bool ShouldDraw()				{ return true; }
	virtual RenderableTranslucencyType_t ComputeTranslucencyType() { return RENDERABLE_IS_TRANSLUCENT; }

	virtual void GetRenderBounds( Vector& mins, Vector& maxs );

	// returns the bounds as an AABB in worldspace
	virtual void GetRenderBoundsWorldspace( Vector& mins, Vector& maxs );

	virtual void OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect );
	virtual void OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect );

	virtual const char* GetParticleEffectName();

private:
	void UpdateParticles( void );
	ks::tracked_ref<CNewParticleEffect> m_burnParticleEffect;

	enum { MAX_INFERNO_FIRES = 100 };
	[[= ks::reflect::Net{} ]] int m_fireXDelta[ MAX_INFERNO_FIRES ];
	[[= ks::reflect::Net{} ]] int m_fireYDelta[ MAX_INFERNO_FIRES ];
	[[= ks::reflect::Net{} ]] int m_fireZDelta[ MAX_INFERNO_FIRES ];
	[[= ks::reflect::Net{} ]] bool m_bFireIsBurning[ MAX_INFERNO_FIRES ];
	Vector m_BurnNormal[ MAX_INFERNO_FIRES ];
	int m_fireUniqueID[ MAX_INFERNO_FIRES ];
	[[= ks::reflect::Net{} ]] int m_fireCount;
	int m_nInfernoType;
	int m_lastFireCount;						///< used to detect changes
	
	// these are the actual visible fires
	enum FireState { STARTING, BURNING, GOING_OUT, FIRE_OUT, UNKNOWN };
	struct Drawable
	{
		Vector m_pos;							///< position of flame
		Vector m_normal;						///< normal of flame surface
		int m_frame;							///< current animation frame
		float m_framerate;						///< rate of animation
		bool m_mirror;							///< if true, flame is mirrored about vertical axis

		int m_dlightIndex;

		FireState m_state;						///< the state of this fire
		float m_stateTimestamp;					///< when the fire entered its current state
		void SetState( FireState state )
		{
			m_state = state;
			m_stateTimestamp = gpGlobals->realtime;
		}

		float m_size;							///< current flame size
		float m_maxSize;						///< maximum size of full-grown flame

		void Draw( void );						///< render this flame
	};
	Drawable m_drawable[ MAX_INFERNO_FIRES ];
	int m_drawableCount;

	void SynchronizeDrawables( void );					///< compare m_fireX etc to m_drawable and update states
	Drawable *GetDrawable( const Vector &pos );	///< given a position, return the fire there

	float m_maxFireHalfWidth;
	float m_maxFireHeight;
	Vector m_minBounds, m_maxBounds;

	void DrawFire( CInferno::Drawable *fire, IMesh *mesh );	///< render an individual flame
	void RecomputeBounds( void );
};

//---------------------------------------------------------
//---------------------------------------------------------
class CFireCrackerBlast: public CInferno
{
	[[= ks::reflect::NetTable{ .name = "DT_FireCrackerBlast" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFireCrackerBlast, CInferno );
	DECLARE_CLIENTCLASS();

	virtual const char* GetParticleEffectName();
};
