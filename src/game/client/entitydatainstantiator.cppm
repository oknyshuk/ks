// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include "client_class.h"
#include "declare_class.h"
#include "reflect_annotations.h"
#include "vphysics_interface.h"
#include "engine/ivmodelinfo.h"
module ks.client:entitydatainstantiator;

import std;
import ks.core.basetypes;
import ks.platform;
import ks.simd;
import ks.tier1.strtools;
import :cstrike15.cs_player;
import :particlemgr;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.tier1.intrusive_ptr;
import ks.tier1.reflect_wirevar;
import :baseentity;
import ks.tier1.tracked_ref;

class CBaseEntity;

// This is the hash key type, but it could just as easily be and int or void *

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class IEntityDataInstantiator
{
public:
	virtual ~IEntityDataInstantiator() {};

	virtual void *GetDataObject( const CBaseEntity *instance ) = 0;
	virtual void *CreateDataObject( const CBaseEntity *instance ) = 0;
	virtual void DestroyDataObject( const CBaseEntity *instance ) = 0;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class T>
class CEntityDataInstantiator : public IEntityDataInstantiator
{
public:
	CEntityDataInstantiator()
	{
		m_HashTable.reserve( 64 );
	}

	virtual void *GetDataObject( const CBaseEntity *instance )
	{
		auto it = m_HashTable.find( instance );
		if ( it != m_HashTable.end() )
		{
			return (void *)it->second.get();
		}

		return nullptr;
	}

	virtual void *CreateDataObject( const CBaseEntity *instance )
	{
		// Create it if not already present
		auto [it, bInserted] = m_HashTable.try_emplace( instance );
		if ( bInserted )
		{
			it->second = std::make_unique<T>();

			// Zero-fill legacy POD data objects; types owning resources construct themselves
			if constexpr ( std::is_trivially_destructible_v<T> )
				V_memset( it->second.get(), 0, sizeof( T ) );
		}

		return (void *)it->second.get();
	}

	virtual void DestroyDataObject( const CBaseEntity *instance )
	{
		m_HashTable.erase( instance );
	}

private:
	std::unordered_map< const CBaseEntity *, std::unique_ptr< T > > m_HashTable;
};

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Entity Dissolve, client-side implementation
//-----------------------------------------------------------------------------
class CEntityDissolve : public CBaseEntity, public IMotionEvent
{
	[[= ks::reflect::NetTable{ .name = "DT_EntityDissolve" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( CEntityDissolve, CBaseEntity );

	CEntityDissolve( void );

	// Inherited from CBaseEntity
	virtual void	GetRenderBounds( Vector& theMins, Vector& theMaxs );
	virtual int		DrawModel( int flags, const RenderableInstance_t &instance );
	virtual bool	ShouldDraw() { return true; }
	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	UpdateOnRemove( void );

	// Inherited from IMotionEvent
	virtual simresult_e	Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );
	
	void			SetupEmitter( void );

	void			ClientThink( void );

	void			SetServerLinkState( bool state ) { m_bLinkedToServerEnt = state; }

	[[= ks::reflect::As{ FIELD_TIME } ]] [[= ks::reflect::Net{} ]] float m_flStartTime;
	[[= ks::reflect::Net{} ]] float	m_flFadeOutStart;
	[[= ks::reflect::Net{} ]] float	m_flFadeOutLength;
	[[= ks::reflect::Net{} ]] float	m_flFadeOutModelStart;
	[[= ks::reflect::Net{} ]] float	m_flFadeOutModelLength;
	[[= ks::reflect::Net{} ]] float	m_flFadeInStart;
	[[= ks::reflect::Net{} ]] float	m_flFadeInLength;
	[[= ks::reflect::Net{} ]] int	m_nDissolveType;
	float   m_flNextSparkTime;

	[[= ks::reflect::Net{} ]] Vector	m_vDissolverOrigin;
	[[= ks::reflect::Net{} ]] int	m_nMagnitude;

	bool	m_bCoreExplode;

protected:

	float GetFadeInPercentage( void );		// Fade in amount (entity fading to black)
	float GetFadeOutPercentage( void );		// Fade out amount (particles fading away)
	float GetModelFadeOutPercentage( void );// Mode fade out amount

	// Compute the bounding box's center, size, and basis
	void ComputeRenderInfo( mstudiobbox_t *pHitBox, const matrix3x4_t &hitboxToWorld, 
								Vector *pVecAbsOrigin, Vector *pXVec, Vector *pYVec );
	void BuildTeslaEffect( mstudiobbox_t *pHitBox, const matrix3x4_t &hitboxToWorld, bool bRandom, float flYawOffset );

	void DoSparks( mstudiohitboxset_t *set, matrix3x4_t *hitboxbones[MAXSTUDIOBONES] );

private:

	ks::intrusive_ptr<CSimpleEmitter>	m_pEmitter;

	bool	m_bLinkedToServerEnt;
	IPhysicsMotionController	*m_pController;
};

class CNewParticleEffect;
class CBaseEntity;

using namespace ::ks::reflect;


//
// Entity flame, client-side implementation
//

class CEntityFlame : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_EntityFlame" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( CEntityFlame, CBaseEntity );

	CEntityFlame( void );
	virtual ~CEntityFlame( void );

	virtual bool	Simulate( void );
	virtual void	UpdateOnRemove( void );
	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	ClientThink( void );

	[[= ks::reflect::Net{} ]] EHANDLE	m_hEntAttached;		// The entity that we are burning (attached to).

private:
	void	CreateEffect( void );
	void	StopEffect( void );

	ks::tracked_ref<CNewParticleEffect> m_hEffect;
	EHANDLE				m_hOldAttached;
	[[= ks::reflect::Net{} ]] bool				m_bCheapEffect;
};

class CBaseEntity;

using namespace ::ks::reflect;

struct EntityFreezingHitboxBlobData_t
{
	std::vector< Vector > m_vPoints;
};

//-----------------------------------------------------------------------------
// Entity Dissolve, client-side implementation
//-----------------------------------------------------------------------------
class CEntityFreezing : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_EntityFreezing" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( CEntityFreezing, CBaseEntity );

	virtual void	GetRenderBounds( Vector& theMins, Vector& theMaxs );
	virtual RenderableTranslucencyType_t ComputeTranslucencyType( );
	virtual int		DrawModel( int flags, const RenderableInstance_t &instance );
	virtual bool	ShouldDraw() { return true; }
	virtual void	OnDataChanged( DataUpdateType_t updateType );

	void			ClientThink( void );

private:
	[[= ks::reflect::Net{} ]] Vector	m_vFreezingOrigin;
	[[= ks::reflect::Net{} ]] float	m_flFrozenPerHitbox[ 50 ];
	[[= ks::reflect::Net{} ]] float	m_flFrozen;
	[[= ks::reflect::Net{} ]] bool	m_bFinishFreezing;

	std::vector< EntityFreezingHitboxBlobData_t > m_HitboxBlobData;
};
