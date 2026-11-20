// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

module;
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include "client_class.h"
#include "model_types.h"
#include "reflect_recvtable.h"
#include "declare_class.h"
#include "reflect_annotations.h"
#include "edict.h"
#include "iclientrenderable.h"
#include "utils/common/scriplib.h"
#include "vcollide_parse.h"
#include "iclientunknown.h"
#include "materialsystem/imesh.h"
#include "tier2/resourceprecacher.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "cdll_int.h"
#include "eiface.h"
#include "igameresources.h"
#include "localize/ilocalize.h"
#include "rocketui/rocketui.h"
#include "reflect_predmap.h"
#include "GameUI/IGameUI.h"
#include "igameevents.h"
#include "baseentity.h"
#include "bone_setup.h"
#include "con_nprint.h"
#include "globalvars_base.h"
#include "iclientnetworkable.h"
#include "posedebugger.h"
module ks.client;

import ks.core.basetypes;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.createinterface;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :sharedvar;
import :util;
import :precache;
import :noopanimcontroller;
import ks.game.shareddefs;
import :achievements_and_stats_interface;
import :baseentity;
import ks.game.gamemovement;
import :particlemgr;
import std;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.game.mapentities_shared;
import ks.core.dbg;
import ks.core.platform;
import ks.core.random;
import ks.core.vprof;
import ks.math.mathlib;
import ks.tier1.keyvalues;
import ks.tier1.strtools;
import ks.tier1.interfaces;
import ks.tier1.vector_ops;
import ks.math.ssemath;
import ks.math.vplane;
import ks.core.color;
import ks.tier1.fmtstr;
import ks.tier1.iconvar;
import ks.tier1.map_ops;
import ks.tier1.playerid;
import ks.vstdlib.vstrtools;
import :cstrike15.cs_player;
import :cstrike15.cs_shareddefs;
import ks.core.types;
import ks.core.threadtools;

class CPhysicsProp;
struct ClientModelRenderInfo_t;

using namespace ::ks::reflect;

IMPLEMENT_REFLECT_CLIENTCLASS( CPhysicsProp, DT_PhysicsProp, CPhysicsProp )

// We discard VVD data on consoles to save memory, so we cannot compute static lighting for models at run-time:
ConVar r_PhysPropStaticLighting( side_registry(), "r_PhysPropStaticLighting", "0" ); // Disabled for CS:GO on the PC

// @MULTICORE (toml 9/18/2006): this visualization will need to be implemented elsewhere
ConVar r_visualizeproplightcaching( side_registry(), "r_visualizeproplightcaching", "0" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPhysicsProp::CPhysicsProp( void )
{
	m_pPhysicsObject = nullptr;
	m_takedamage = DAMAGE_YES;

	// default true so static lighting will get recomputed when we go to sleep
	m_bAwakeLastTime = true;

	m_bCanUseStaticLighting = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPhysicsProp::~CPhysicsProp( void )
{
}

void CPhysicsProp::OnPreDataChanged( DataUpdateType_t updateType )
{
}

// Used to indicate if we can use the static lighting baking
//-----------------------------------------------------------------------------
void CPhysicsProp::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	m_bCanUseStaticLighting = modelinfo->UsesStaticLighting( GetModel() );
	if ( m_bCanUseStaticLighting )
	{
		CreateModelInstance();
	}

}

//-----------------------------------------------------------------------------
// Hooks for the fast model rendering path
//-----------------------------------------------------------------------------
IClientModelRenderable*	CPhysicsProp::GetClientModelRenderable()
{
	// Can't do this debug mode through the fast path since it uses 
	// color modulation, which we don't support yet.
	if ( !BaseClass::GetClientModelRenderable() )
		return nullptr;

	if ( ( m_bAwakeLastTime != m_bAwake ) && r_visualizeproplightcaching.GetBool() )
		return nullptr;

	// NOTE: This works because GetClientModelRenderable() is only queried
	// if the prop is known by the viewrender system to be opaque already
	// so we need no other opacity checks here.
	return this;
}

bool CPhysicsProp::GetRenderData( void *pData, ModelDataCategory_t nCategory )
{
	switch ( nCategory )
	{
	case MODEL_DATA_LIGHTING_MODEL:
		{
			// FIXME: Cache bCanBeStaticLit off in OnDataChanged?
			if ( !m_bCanUseStaticLighting || !r_PhysPropStaticLighting.GetBool() )
			{
				*(RenderableLightingModel_t*)pData = LIGHTING_MODEL_STANDARD;
				return true;
			}

			if ( m_bAwakeLastTime && !m_bAwake )
			{
				// transition to sleep, bake lighting now, once
				if ( !modelrender->RecomputeStaticLighting( GetModelInstance() ) )
				{
					// Failed to bake, don't indicate it's asleep yet, use normal model
					*(RenderableLightingModel_t*)pData = LIGHTING_MODEL_STANDARD;
					return true;
				}
			}

			*(RenderableLightingModel_t*)pData = m_bAwake ? LIGHTING_MODEL_STANDARD : LIGHTING_MODEL_PHYSICS_PROP;
			m_bAwakeLastTime = m_bAwake;
		}
		return true;

	default:
		return BaseClass::GetRenderData( pData, nCategory );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Purpose: Draws the object
// Input  : flags - 
//-----------------------------------------------------------------------------
bool CPhysicsProp::OnInternalDrawModel( ClientModelRenderInfo_t *pInfo )
{
	if ( !m_bCanUseStaticLighting || !r_PhysPropStaticLighting.GetBool() )
		return true;

	if ( m_bAwakeLastTime != m_bAwake )
	{
		if ( m_bAwakeLastTime && !m_bAwake )
		{
			// transition to sleep, bake lighting now, once
			if ( !modelrender->RecomputeStaticLighting( GetModelInstance() ) )
			{
				// not valid for drawing
				return false;
			}

			if ( r_visualizeproplightcaching.GetBool() )
			{
				float color[] = { 0.0f, 1.0f, 0.0f, 1.0f };
				render->SetColorModulation( color );
			}
		}
		else if ( r_visualizeproplightcaching.GetBool() )
		{
			float color[] = { 1.0f, 0.0f, 0.0f, 1.0f };
			render->SetColorModulation( color );
		}
	}

	if ( !m_bAwake )
	{
		// going to sleep, have static lighting
		pInfo->flags |= STUDIO_STATIC_LIGHTING;
	}
	
	// track state
	m_bAwakeLastTime = m_bAwake;
	return true;
}

class CBaseAnimating;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPhysMagnet : public CBaseAnimating
{
	[[= ks::reflect::NetTable{ .name = "DT_PhysMagnet" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CPhysMagnet, CBaseAnimating );
public:
	DECLARE_CLIENTCLASS();

	CPhysMagnet();
	virtual	~CPhysMagnet();

	void	PostDataUpdate( DataUpdateType_t updateType );
	bool	GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const;
	
public:
	// Data received from the server
	std::vector< int > m_aAttachedObjectsFromServer;

	// Private list of entities on the magnet
	std::vector< EHANDLE > m_aAttachedObjects;
};

//-----------------------------------------------------------------------------
// Purpose: RecvProxy that converts the Magnet's attached object entindexes to handles
//-----------------------------------------------------------------------------
void RecvProxy_MagnetAttachedObjectList(  const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CPhysMagnet *pMagnet = (CPhysMagnet*)pOut;
	pMagnet->m_aAttachedObjectsFromServer[pData->m_iElement] = pData->m_Value.m_Int;
}

void RecvProxyArrayLength_MagnetAttachedArray( void *pStruct, int objectID, int currentArrayLength )
{
	CPhysMagnet *pMagnet = (CPhysMagnet*)pStruct;
	
	if ( std::ssize( pMagnet->m_aAttachedObjectsFromServer ) != currentArrayLength )
		pMagnet->m_aAttachedObjectsFromServer.assign( currentArrayLength, {} );
}

IMPLEMENT_REFLECT_CLIENTCLASS( CPhysMagnet, DT_PhysMagnet, CPhysMagnet )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPhysMagnet::CPhysMagnet()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPhysMagnet::~CPhysMagnet()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhysMagnet::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

	/*
	// First, detect any entities removed from the magnet and restore their shadows
	int iCount = m_aAttachedObjects.Count();
	int iServerCount = m_aAttachedObjectsFromServer.Count();
	for ( int i = 0; i < iCount; i++ )
	{
		int iEntIndex = m_aAttachedObjects[i]->entindex();
		for ( int j = 0; j < iServerCount; j++ )
		{
			if ( iEntIndex == m_aAttachedObjectsFromServer[j] )
				break;
		}

		if ( j == iServerCount )
		{
			// Ok, a previously attached object is no longer attached
			m_aAttachedObjects[i]->SetShadowUseOtherEntity( NULL );
			m_aAttachedObjects.Remove(i);
		}
	}

	// Make sure newly attached entities have vertical shadows too
	for ( i = 0; i < iServerCount; i++ )
	{
		CBaseEntity *pEntity = cl_entitylist->GetEnt( m_aAttachedObjectsFromServer[i] );
		if ( m_aAttachedObjects.Find( pEntity ) == m_aAttachedObjects.InvalidIndex() )
		{
			pEntity->SetShadowUseOtherEntity( this );
			m_aAttachedObjects.AddToTail( pEntity );
		}
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: Return a per-entity shadow cast direction
//-----------------------------------------------------------------------------
bool CPhysMagnet::GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const
{
	// Magnets shadow is more vertical than others
	//Vector vecDown = g_pClientShadowMgr->GetShadowDirection() - Vector(0,0,1);
	//VectorNormalize( vecDown );
	//*pDirection = vecDown;
	*pDirection = Vector(0,0,-1);
	return true;
}

class CEffectData;
class CBaseAnimating;
class CBaseEntity;
class CClientRagdoll;

using namespace ::ks::reflect;

static constexpr auto FADEOUT_TIME = 1.0f;
ConVar	r_propsmaxdist( side_registry(), "r_propsmaxdist", "1200", 0, "Maximum visible distance" );

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

static int PropBreakablePrecacheAll( int modelIndex )
{
	std::vector< breakmodel_t > list;

	BreakModelList( list, modelIndex, COLLISION_GROUP_NONE, 0 );
	return std::ssize( list );
}

CPhysPropClientside *CPhysPropClientside::CreateNew( bool bForce )
{
	return new CPhysPropClientside();
}

CPhysPropClientside::CPhysPropClientside()
{
	m_fDeathTime = -1;
	m_impactEnergyScale = 1.0f;
	m_iHealth = 0;
	m_iPhysicsMode = PHYSICS_MULTIPLAYER_AUTODETECT;
	m_flTouchDelta = 0;
}

CPhysPropClientside::~CPhysPropClientside()
{
	PhysCleanupFrictionSounds( this );
	VPhysicsDestroyObject();
}

void CPhysPropClientside::SetPhysicsMode(int iMode)
{
	if ( m_iPhysicsMode == PHYSICS_MULTIPLAYER_AUTODETECT )
		m_iPhysicsMode = iMode;
}

//-----------------------------------------------------------------------------
// Should we collide?
//-----------------------------------------------------------------------------
bool CPhysPropClientside::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "physdamagescale"))
	{
		m_impactEnergyScale = atof(szValue);
	}
	else if ( FStrEq(szKeyName, "health") )
	{
		m_iHealth = V_atoi(szValue);
	}
	else if (FStrEq(szKeyName, "spawnflags"))
	{
		m_spawnflags = V_atoi(szValue);
	}
	else if (FStrEq(szKeyName, "model"))
	{
		SetModelName( AllocPooledString( szValue ) );
	}
	else if (FStrEq(szKeyName, "fademaxdist"))
	{
		float flFadeMaxDist = V_atof(szValue);
		SetDistanceFade( GetMinFadeDist(), flFadeMaxDist );
	}
	else if (FStrEq(szKeyName, "fademindist"))
	{
		float flFadeMinDist = V_atof(szValue);
		SetDistanceFade( flFadeMinDist, GetMaxFadeDist() );
	}
	else if (FStrEq(szKeyName, "fadescale"))
	{
		SetGlobalFadeScale( V_atof(szValue) );
	}
	else if (FStrEq(szKeyName, "inertiaScale"))
	{
		m_inertiaScale = V_atof(szValue);
	}
	else if (FStrEq(szKeyName, "skin"))
	{
		SetSkin( V_atoi(szValue) );
	}
	else if (FStrEq(szKeyName, "physicsmode"))
	{
		m_iPhysicsMode = V_atoi(szValue);
	}

	else
	{
		if ( !BaseClass::KeyValue( szKeyName, szValue ) )
		{
			// key hasn't been handled
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CPhysPropClientside::StartTouch( CBaseEntity *pOther )
{
	// Limit the amount of times we can bounce
	if ( m_flTouchDelta < gpGlobals->curtime )
	{
		HitSurface( pOther );
		m_flTouchDelta = gpGlobals->curtime + 0.1f;
	}

	BaseClass::StartTouch( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CPhysPropClientside::HitSurface( CBaseEntity *pOther )
{
	if ( HasInteraction( PROPINTER_WORLD_BLOODSPLAT ) )
	{
		trace_t	tr;
		tr = BaseClass::GetTouchTrace();
		if ( tr.Ent<CBaseEntity>() )
		{
			UTIL_BloodDecalTrace( &tr, BLOOD_COLOR_RED );
		}
	}
}

void CPhysPropClientside::RecreateAll()
{
	DestroyAll();
	ParseAllEntities( engine->GetMapEntitiesString() );
}

void CPhysPropClientside::DestroyAll()
{
	CBaseEntityIterator iterator;
	CBaseEntity *pEnt;
	while ( (pEnt = iterator.Next()) != nullptr )	
	{
		CPhysPropClientside *pProp = dynamic_cast<CPhysPropClientside *>(pEnt);
		if ( pProp )
		{
			pProp->Release();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Parse this prop's data from the model, if it has a keyvalues section.
//			Returns true only if this prop is using a model that has a prop_data section that's invalid.
//-----------------------------------------------------------------------------
int CPhysPropClientside::ParsePropData( void )
{
	KeyValues *pModelKV = modelinfo->GetModelKeyValues( GetModel() );
	if ( !pModelKV )
		return PARSE_FAILED_NO_DATA;

	// Do we have a props section?
	KeyValues *pkvPropData = pModelKV->FindKey("prop_data");
	if ( !pkvPropData )
		return PARSE_FAILED_NO_DATA;

	int iResult = g_PropDataSystem.ParsePropFromKV( this, this, pkvPropData, pModelKV );
	return iResult;
}

bool CPhysPropClientside::Initialize()
{
	if ( InitializeAsClientEntity( STRING(GetModelName()), false ) == false )
	{
		return false;
	}

	const model_t *mod = GetModel();
	if ( mod )
	{
		Vector mins, maxs;
		modelinfo->GetModelBounds( mod, mins, maxs );
		SetCollisionBounds( mins, maxs );
	}

	solid_t tmpSolid;

	// Create the object in the physics system

	if ( !PhysModelParseSolid( tmpSolid, this, GetModelIndex() ) )
	{
		DevMsg("CPhysPropClientside::Initialize: PhysModelParseSolid failed for entity %i.\n", GetModelIndex() );
		return false;
	}
	else
	{
		m_pPhysicsObject = VPhysicsInitNormal( SOLID_VPHYSICS, 0, m_spawnflags & SF_PHYSPROP_START_ASLEEP, &tmpSolid );
	
		if ( !m_pPhysicsObject )
		{
			// failed to create a physics object
		DevMsg(" CPhysPropClientside::Initialize: VPhysicsInitNormal() failed for %s.\n", STRING(GetModelName()) );
			return false;
		}
	}

	// We want touch calls when we hit the world
	unsigned int flags = VPhysicsGetObject()->GetCallbackFlags();
	VPhysicsGetObject()->SetCallbackFlags( flags | CALLBACK_GLOBAL_TOUCH_STATIC );

	if ( m_spawnflags & SF_PHYSPROP_MOTIONDISABLED )
	{
		m_pPhysicsObject->EnableMotion( false );
	}
		
	Spawn(); // loads breakable & prop data

	if ( m_iPhysicsMode == PHYSICS_MULTIPLAYER_AUTODETECT )
	{
		m_iPhysicsMode = GetAutoMultiplayerPhysicsMode( 
			CollisionProp()->OBBSize(), m_pPhysicsObject->GetMass() );
	}

	if 	( m_spawnflags & SF_PHYSPROP_FORCE_SERVER_SIDE )
	{
		// forced to be server-side by map maker
		return false;
	}
		
	if ( m_iPhysicsMode != PHYSICS_MULTIPLAYER_CLIENTSIDE )
	{
		// spawn only clientside entities
		return false;
	}
	else 
	{
		if ( engine->IsInEditMode() )
		{
			// don't spawn in map edit mode
			return false;
		}
	}

	if ( GetMinFadeDist() < 0.0f )
	{
		// start fading out at 75% of r_propsmaxdist
		float flPropsMaxDist = r_propsmaxdist.GetFloat();
		SetDistanceFade( flPropsMaxDist * 0.75f, flPropsMaxDist );
	}

	// player can push it away
	SetCollisionGroup( COLLISION_GROUP_PUSHAWAY );

	UpdatePartitionListEntry();

	CollisionProp()->UpdatePartition();

	SetBlocksLOS( false ); // this should be a small object

	// Set up shadows; do it here so that objects can change shadowcasting state
	CreateShadow();

	UpdateVisibility();

	SetNextClientThink( CLIENT_THINK_NEVER );

	return true;
}

void CPhysPropClientside::Spawn()
{
	// Initialize damage modifiers. Must be done before baseclass spawn.
	m_flDmgModBullet = 1.0;
	m_flDmgModClub = 1.0;
	m_flDmgModExplosive = 1.0;
	m_flDmgModFire = 1.0;

	BaseClass::Spawn();

	// we don't really precache models here, just checking how many we have:
	m_iNumBreakableChunks = PropBreakablePrecacheAll( GetModelIndex() );

	ParsePropData();

	// If we have no custom breakable chunks, see if we're breaking into generic ones
	if ( !m_iNumBreakableChunks )
	{
		if ( GetBreakableModel() != NULL_STRING && GetBreakableCount() )
		{
			m_iNumBreakableChunks = GetBreakableCount();
		}
	}

	// Setup takedamage based upon the health we parsed earlier
	if ( m_iHealth == 0 )
	{
		m_takedamage = DAMAGE_NO;
	}
	else
	{
		m_takedamage = DAMAGE_YES;
	}
}

void CPhysPropClientside::OnTakeDamage( int iDamage ) // very simple version
{
	if ( m_takedamage == DAMAGE_NO )
		return;

	m_iHealth -= iDamage;

	if (m_iHealth <= 0)
	{
		Break();
	}
}

float CPhysPropClientside::GetMass()
{
	if ( VPhysicsGetObject() )
	{
		return VPhysicsGetObject()->GetMass();
	}

	return 0.0f;
}

bool CPhysPropClientside::IsAsleep()
{
	if ( VPhysicsGetObject() )
	{
		return VPhysicsGetObject()->IsAsleep();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhysPropClientside::ClientThink( void )
{
	if ( m_fDeathTime < 0 )
	{
		SetNextClientThink( CLIENT_THINK_NEVER );
		return;
	}

	if ( m_fDeathTime <= gpGlobals->curtime )
	{
		Release(); // Die
		return;
	}

	// fade out 

	float alpha = (m_fDeathTime - gpGlobals->curtime)/FADEOUT_TIME;

	SetRenderMode( kRenderTransTexture );

	SetRenderAlpha( alpha * 256 );

	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

void CPhysPropClientside::StartFadeOut( float fDelay )
{
	m_fDeathTime = gpGlobals->curtime + fDelay + FADEOUT_TIME;

	SetNextClientThink( gpGlobals->curtime + fDelay );
}

void CPhysPropClientside::Break()
{
	m_takedamage = DAMAGE_NO;
	
	IPhysicsObject *pPhysics = VPhysicsGetObject();

	Vector velocity;
	AngularImpulse angVelocity;
	Vector origin;
	QAngle angles;
	AddSolidFlags( FSOLID_NOT_SOLID );

	if ( pPhysics )
	{
		pPhysics->GetVelocity( &velocity, &angVelocity );
		pPhysics->GetPosition( &origin, &angles );
		pPhysics->RecheckCollisionFilter();
	}
	else
	{
		velocity = GetAbsVelocity();
		QAngleToAngularImpulse( GetLocalAngularVelocity(), angVelocity );
		origin = GetAbsOrigin();
		angles = GetAbsAngles();
	}

	breakablepropparams_t params( origin, angles, velocity, angVelocity );
	params.impactEnergyScale = m_impactEnergyScale;
	params.defCollisionGroup = GetCollisionGroup();
	if ( params.defCollisionGroup == COLLISION_GROUP_NONE )
	{
		// don't automatically make anything COLLISION_GROUP_NONE or it will
		// collide with debris being ejected by breaking
		params.defCollisionGroup = COLLISION_GROUP_INTERACTIVE;
	}

	// no damage/damage force? set a burst of 100 for some movement
	params.defBurstScale = 100;

	// spwan break chunks
	PropBreakableCreateAll( GetModelIndex(), pPhysics, params, this, -1, false );

	Release(); // destroy object
}

void CPhysPropClientside::Clone( Vector &velocity )
{
	CPhysPropClientside *pEntity = CPhysPropClientside::CreateNew();

	if ( !pEntity )
		return;

	pEntity->m_spawnflags = m_spawnflags;

	// We never want to be motion disabled
	pEntity->m_spawnflags &= ~SF_PHYSPROP_MOTIONDISABLED;
		
	pEntity->SetDmgModBullet( GetDmgModBullet() );
	pEntity->SetDmgModClub( GetDmgModClub() );
	pEntity->SetDmgModExplosive( GetDmgModExplosive() );
	
	pEntity->SetModelName( GetModelName() );
	pEntity->SetLocalOrigin( GetLocalOrigin() );
	pEntity->SetLocalAngles( GetLocalAngles() );
	pEntity->SetOwnerEntity( this );
	pEntity->SetPhysicsMode( PHYSICS_MULTIPLAYER_CLIENTSIDE );

	if ( !pEntity->Initialize() )
	{
		pEntity->Release();
		return;
	}

	pEntity->SetSkin( GetSkin() );
	pEntity->m_iHealth = m_iHealth;

	if ( pEntity->m_iHealth == 0 )
	{
		// if  no health, don't collide with player anymore, don't take damage
		pEntity->m_takedamage = DAMAGE_NO;
		pEntity->SetCollisionGroup( COLLISION_GROUP_NONE );
	}
	
	IPhysicsObject *pPhysicsObject = pEntity->VPhysicsGetObject();

	if( pPhysicsObject )
	{
		// randomize velocity by 5%
		float rndf = RandomFloat( -0.025, 0.025 );
		Vector rndVel = velocity + rndf*velocity;

		pPhysicsObject->AddVelocity( &rndVel, nullptr );
	}
	else
	{
		// failed to create a physics object
		pEntity->Release();
	}
}

void CPhysPropClientside::ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName )
{
	CVProfScope _( "CPhysPropClientside::ImpactTrace", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();

	if( !pPhysicsObject )
		return;

	Vector dir = pTrace->endpos - pTrace->startpos;
	int iDamage = 0;

	if ( iDamageType & DMG_BLAST )
	{
		iDamage = VectorLength( dir );
		dir *= 500;  // adjust impact strenght

		// apply force at object mass center
		pPhysicsObject->ApplyForceCenter( dir );
	}
	else
	{
		Vector hitpos;  
	
		VectorMA( pTrace->startpos, pTrace->fraction, dir, hitpos );
		VectorNormalize( dir );

		// guess avg damage
		if ( iDamageType == DMG_BULLET )
		{
			iDamage = 30;
		}
		else
		{
			iDamage = 50;
		}
		 
		dir *= 4000;  // adjust impact strenght

		// apply force where we hit it
		pPhysicsObject->ApplyForceOffset( dir, hitpos );	

		// Build the impact data
		CEffectData data;
		data.m_vOrigin = pTrace->endpos;
		data.m_vStart = pTrace->startpos;
		data.m_nSurfaceProp = pTrace->surface.surfaceProps;
		data.m_nDamageType = iDamageType;
		data.m_nHitBox = pTrace->hitbox;
		data.m_hEntity = GetRefEHandle();

		// Send it on its way
		if ( !pCustomImpactName )
		{
			DispatchEffect( "Impact", data );
		}
		else
		{
			DispatchEffect( pCustomImpactName, data );
		}
	}

	// Clone( dir ); // debug code

	OnTakeDamage( iDamage );
}

const char *CPhysPropClientside::ParseEntity( const char *pEntData )
{
	CEntityMapData entData( (char*)pEntData );
	char className[MAPKEY_MAXLENGTH];
	
	MDLCACHE_CRITICAL_SECTION();

	if (!entData.ExtractValue("classname", className))
	{
		Error( "classname missing from entity!\n" );
	}

	if ( !V_strcmp( className, "prop_physics_multiplayer" ) )
	{
		// always force clientside entitis placed in maps
		CPhysPropClientside *pEntity = CPhysPropClientside::CreateNew( true ); 

		if ( pEntity )
		{	// Set up keyvalues.
			pEntity->ParseMapData(&entData);
			
			if ( !pEntity->Initialize() )
				pEntity->Release();
		
			return entData.CurrentBufferPosition();
		}
	}

	if ( !V_strcmp( className, "func_proprrespawnzone" ) )
	{
		DebuggerBreakIfDebugging();
	}

	// Just skip past all the keys.
	char keyName[MAPKEY_MAXLENGTH];
	char value[MAPKEY_MAXLENGTH];
	if ( entData.GetFirstKey(keyName, value) )
	{
		do 
		{
		} 
		while ( entData.GetNextKey(keyName, value) );
	}

	//
	// Return the current parser position in the data block
	//
	return entData.CurrentBufferPosition();
}

//-----------------------------------------------------------------------------
// Purpose: Only called on BSP load. Parses and spawns all the entities in the BSP.
// Input  : pMapData - Pointer to the entity data block to parse.
//-----------------------------------------------------------------------------
void CPhysPropClientside::ParseAllEntities(const char *pMapData)
{
	int nEntities = 0;

	char szTokenBuffer[MAPKEY_MAXLENGTH];

	//
	//  Loop through all entities in the map data, creating each.
	//
	for ( ; true; pMapData = MapEntity_SkipToNextEntity(pMapData, szTokenBuffer) )
	{
		//
		// Parse the opening brace.
		//
		char token[MAPKEY_MAXLENGTH];
		pMapData = MapEntity_ParseToken( pMapData, token );

		//
		// Check to see if we've finished or not.
		//
		if (!pMapData)
			break;

		if (token[0] != '{')
		{
			Error( "MapEntity_ParseAllEntities: found %s when expecting {", token);
			continue;
		}

		//
		// Parse the entity and add it to the spawn list.
		//

		pMapData = ParseEntity( pMapData );

		nEntities++;
	}
}

CBaseAnimating *BreakModelCreate_Ragdoll( CBaseEntity *pOwnerEnt, breakmodel_t *pModel, const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity )
{
	CBaseAnimating *pOwner = dynamic_cast<CBaseAnimating *>( pOwnerEnt );
	if ( !pOwner )
		return nullptr;

	CClientRagdoll *pRagdoll = new CClientRagdoll( false );
	if ( pRagdoll == nullptr )
		return nullptr;

	const char *pModelName = pModel->modelName;
	if ( pRagdoll->InitializeAsClientEntity( pModelName, false ) == false )
	{
		pRagdoll->Release();
		return nullptr;
	}	

	pRagdoll->SetAbsOrigin( position );
	pRagdoll->SetAbsAngles( angles );

	matrix3x4a_t boneDelta0[MAXSTUDIOBONES];
	matrix3x4a_t boneDelta1[MAXSTUDIOBONES];
	matrix3x4a_t currentBones[MAXSTUDIOBONES];
	const float boneDt = 0.1f;

	pRagdoll->SetParent( pOwner );
	pRagdoll->ForceSetupBonesAtTime( boneDelta0, gpGlobals->curtime - boneDt );
	pRagdoll->ForceSetupBonesAtTime( boneDelta1, gpGlobals->curtime );
	pRagdoll->ForceSetupBonesAtTime( currentBones, gpGlobals->curtime );
	pRagdoll->SetParent( nullptr );

	// We need to take these from the entity
	//pRagdoll->SetAbsOrigin( position );
	//pRagdoll->SetAbsAngles( angles );

	pRagdoll->IgniteRagdoll( pOwner );
	pRagdoll->TransferDissolveFrom( pOwner );
	pRagdoll->InitModelEffects();

	if ( pOwner->IsEffectActive( EF_NOSHADOW ) )
	{
		pRagdoll->AddEffects( EF_NOSHADOW );
	}

	pRagdoll->m_bClientSideRagdoll = true;
	pRagdoll->SetRenderMode( pOwner->GetRenderMode() );
	pRagdoll->SetRenderColor( pOwner->GetRenderColor().r, pOwner->GetRenderColor().g, pOwner->GetRenderColor().b );
	pRagdoll->SetRenderAlpha( pOwner->GetRenderAlpha() );
	pRagdoll->SetGlobalFadeScale( pOwner->GetGlobalFadeScale() );

	pRagdoll->SetSkin( pOwner->GetSkin() );
	//pRagdoll->m_vecForce = pOwner->m_vecForce;
	//pRagdoll->m_nForceBone = 0; //pOwner->m_nForceBone;
	pRagdoll->SetNextClientThink( CLIENT_THINK_ALWAYS );

	pRagdoll->SetModelName( AllocPooledString( pModelName ) );
	pRagdoll->ResetSequence( 0 );
	pRagdoll->SetModelScale( pOwner->GetModelScale(), pOwner->GetModelScaleType() );
	pRagdoll->SetCollisionGroup( COLLISION_GROUP_DEBRIS );
	//pRagdoll->m_builtRagdoll = true;

	CStudioHdr *hdr = pRagdoll->GetModelPtr();
	if ( !hdr )
	{
		pRagdoll->Release();
		return nullptr;
	}

	pRagdoll->m_pRagdoll = CreateRagdoll( 
		pRagdoll, 
		hdr, 
		vec3_origin, 
		0, 
		boneDelta0, 
		boneDelta1, 
		currentBones,
 		boneDt );

	IPhysicsObject *pPhysicsObject = pRagdoll->VPhysicsGetObject();
	if ( pPhysicsObject )
	{
		// randomize velocity by 5%
		float rndf = RandomFloat( -0.025, 0.025 );
		Vector rndVel = velocity + rndf*velocity;

		pPhysicsObject->AddVelocity( &rndVel, &angVelocity );
	}
	pRagdoll->ApplyLocalAngularVelocityImpulse( angVelocity );

	if ( pRagdoll->m_pRagdoll )
	{
		pRagdoll->m_bImportant = false;
		s_RagdollLRU.MoveToTopOfLRU( pRagdoll, pRagdoll->m_bImportant, pModel->fadeTime > 0.0f ? gpGlobals->curtime + pModel->fadeTime : 0.0f );
		pRagdoll->m_bFadeOut = true;
	}

	// Cause the entity to recompute its shadow	type and make a
	// version which only updates when physics state changes
	// NOTE: We have to do this after m_pRagdoll is assigned above
	// because that's what ShadowCastType uses to figure out which type of shadow to use.
	pRagdoll->DestroyShadow();
	pRagdoll->CreateShadow();

	pRagdoll->SetAbsOrigin( position );
	pRagdoll->SetAbsAngles( angles );

	pRagdoll->SetPlaybackRate( 0 );
	pRagdoll->UpdatePartitionListEntry();
	pRagdoll->MarkRenderHandleDirty();

	//pRagdoll->InitAsClientRagdoll( boneDelta0, boneDelta1, currentBones, boneDt );

	return pRagdoll;
}

CBaseEntity *BreakModelCreateSingle( CBaseEntity *pOwner, breakmodel_t *pModel, const Vector &position, 
	const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, int nSkin, const breakablepropparams_t &params )
{
	if ( pModel->isRagdoll )
	{
		CBaseEntity *pEntity = BreakModelCreate_Ragdoll( pOwner, pModel, position, angles, velocity, angVelocity );
		return pEntity;
	}

	CPhysPropClientside *pEntity = CPhysPropClientside::CreateNew();

	if ( !pEntity )
		return nullptr;

	// UNDONE: Allow .qc to override spawnflags for child pieces
	CPhysPropClientside *pBreakableOwner = dynamic_cast<CPhysPropClientside *>(pOwner);

	// Inherit the base object's damage modifiers
	if ( pBreakableOwner )
	{
		pEntity->SetEffects( pBreakableOwner->GetEffects() );

		pEntity->m_spawnflags = pBreakableOwner->m_spawnflags;

		// We never want to be motion disabled
		pEntity->m_spawnflags &= ~SF_PHYSPROP_MOTIONDISABLED;
		
		pEntity->SetDmgModBullet( pBreakableOwner->GetDmgModBullet() );
		pEntity->SetDmgModClub( pBreakableOwner->GetDmgModClub() );
		pEntity->SetDmgModExplosive( pBreakableOwner->GetDmgModExplosive() );

		// FIXME: If this was created from a client-side entity which was in the
		// middle of ramping the fade scale, we're screwed.
		pEntity->CopyFadeFrom( pBreakableOwner );
	}
	
	pEntity->SetModelName( AllocPooledString( pModel->modelName ) );
	pEntity->SetLocalOrigin( position );
	pEntity->SetLocalAngles( angles );
	pEntity->SetOwnerEntity( pOwner );
	pEntity->SetPhysicsMode( PHYSICS_MULTIPLAYER_CLIENTSIDE );

	if ( !pEntity->Initialize() )
	{
		pEntity->Release();
		return nullptr;
	}

	pEntity->SetSkin( nSkin );
	pEntity->m_iHealth = pModel->health;

	pEntity->SetCollisionGroup( COLLISION_GROUP_DEBRIS );

	if ( pModel->health == 0 )
	{
		// if  no health, don't collide with player anymore, don't take damage
		pEntity->m_takedamage = DAMAGE_NO;

		if ( pEntity->GetCollisionGroup() == COLLISION_GROUP_PUSHAWAY )
		{
			pEntity->SetCollisionGroup( COLLISION_GROUP_NONE );
		}
	}
	
	if ( pModel->fadeTime > 0 )
	{
		pEntity->StartFadeOut( pModel->fadeTime );
	}

	if ( pModel->fadeMinDist > 0 && pModel->fadeMaxDist >= pModel->fadeMinDist )
	{
		pEntity->SetDistanceFade( pModel->fadeMinDist, pModel->fadeMaxDist );
	}

	IPhysicsObject *pPhysicsObject = pEntity->VPhysicsGetObject();

	if( pPhysicsObject )
	{
		// randomize velocity by 5%
		float rndf = RandomFloat( -0.025, 0.025 );
		Vector rndVel = velocity + rndf*velocity;

		pPhysicsObject->AddVelocity( &rndVel, &angVelocity );
	}
	else
	{
		// failed to create a physics object
		pEntity->Release();
		return nullptr;
	}

	return pEntity;
}

class CBasePlayer;

using namespace ::ks::reflect;

static void PixelvisDrawChanged( ConVar *pPixelvisVar, const char *pOld, float flOldValue );

ConVar r_pixelvisibility_partial( side_registry(), "r_pixelvisibility_partial", "1" );
//#if defined( DX_TO_GL_ABSTRACTION ) 
//ConVar r_dopixelvisibility( "r_dopixelvisibility", "1" );
//#else
ConVar r_dopixelvisibility( side_registry(), "r_dopixelvisibility", "1" );
ConVar r_drawpixelvisibility( side_registry(), "r_drawpixelvisibility", "0", 0, "Show the occlusion proxies", PixelvisDrawChanged );
ConVar r_pixelvisibility_spew( side_registry(), "r_pixelvisibility_spew", "0" );

	// non OSX path
	static bool	HasFastQueries( void )
	{
		return true;
	}

extern ConVar building_cubemaps;

static const float MIN_PROXY_PIXELS = 5.0f;

extern view_id_t CurrentViewID();

struct OcclusionHandleViewIDPair_t
{
	OcclusionQueryObjectHandle_t hOcclusionHandle;
	int iViewID;
	int iLastFrameRendered;
};

struct OcclusionQueryHiddenData_t
{
	COcclusionQuerySet *pOwner;
	std::vector< OcclusionHandleViewIDPair_t > occlusionHandles[MAX_SPLITSCREEN_PLAYERS];
};

static std::vector< OcclusionQueryHiddenData_t > s_OcclusionQueries;
static inline int FindQueryHandlePairIndex( OcclusionQueryHiddenData_t *pData, int iViewID, int iSplitScreenSlot )
{
	int iPairCount = std::ssize( pData->occlusionHandles[iSplitScreenSlot] );
	OcclusionHandleViewIDPair_t *pPairs = pData->occlusionHandles[iSplitScreenSlot].data();
	
	for( int i = 0; i != iPairCount; ++i )
	{
		if( pPairs[i].iViewID == iViewID )
			return i;
	}

	return -1;
}

extern int ScreenTransform( const Vector& point, Vector& screen );
float PixelVisibility_DrawProxy( IMatRenderContext *pRenderContext, OcclusionQueryObjectHandle_t queryHandle, Vector origin, float scale, float proxyAspect, IMaterial *pMaterial, bool screenspace )
{
	Vector point;

	// don't expand this with distance to fit pixels or the sprite will poke through
	// only expand the parts perpendicular to the view
	float forwardScale = scale;
	// draw a pyramid of points touching a sphere of radius "scale" at origin
	float pixelsPerUnit = pRenderContext->ComputePixelDiameterOfSphere( origin, 1.0f );
	pixelsPerUnit = std::max( pixelsPerUnit, 1e-4f );
	if ( screenspace )
	{
		// Force this to be the size of a sphere of diameter "scale" at some reference distance (1.0 unit)
		float pixelsPerUnit2 = pRenderContext->ComputePixelDiameterOfSphere( CurrentViewOrigin() + CurrentViewForward()*1.0f, scale*0.5f );
		// force drawing of "scale" pixels
		scale = pixelsPerUnit2 / pixelsPerUnit;
	}
	else
	{
		float pixels = scale * pixelsPerUnit;
		
		// make the radius larger to ensure a minimum screen space size of the proxy geometry
		if ( pixels < MIN_PROXY_PIXELS )
		{
			scale = MIN_PROXY_PIXELS / pixelsPerUnit;
		}
	}

	// collapses the pyramid to a plane - so this could be a quad instead
	Vector dir = origin - CurrentViewOrigin();
	VectorNormalize(dir);
	origin -= dir * forwardScale;
	forwardScale = 0.0f;
	// 

	Vector verts[5];
	const float sqrt2 = 0.707106781f; // sqrt(2) - keeps all vectors the same length from origin
	scale *= sqrt2;
	float scale45x = scale;
	float scale45y = scale / proxyAspect;
	verts[0] = origin - CurrentViewForward() * forwardScale;					  // the apex of the pyramid
	verts[1] = origin + CurrentViewUp() * scale45y - CurrentViewRight() * scale45x; // these four form the base
	verts[2] = origin + CurrentViewUp() * scale45y + CurrentViewRight() * scale45x; // the pyramid is a sprite with a point that
	verts[3] = origin - CurrentViewUp() * scale45y + CurrentViewRight() * scale45x; // pokes back toward the camera through any nearby 
	verts[4] = origin - CurrentViewUp() * scale45y - CurrentViewRight() * scale45x; // geometry

	// get screen coords of edges
	Vector screen[4];
	for ( int i = 0; i < 4; i++ )
	{
		if ( ScreenTransform( verts[i+1], screen[i] ) )
			return -1;
	}

	// compute area and screen-clipped area
	float w = screen[1].x - screen[0].x;
	float h = screen[0].y - screen[3].y;
	float ws = std::min(1.0f, screen[1].x) - std::max(-1.0f, screen[0].x);
	float hs = std::min(1.0f, screen[0].y) - std::max(-1.0f, screen[3].y);
	float area = w*h; // area can be zero when we ALT-TAB
	float areaClipped = ws*hs;
	float ratio = 0.0f;
	if ( area != 0 )
	{
		// compute the ratio of the area not clipped by the frustum to total area
		ratio = areaClipped / area;
		ratio = clamp(ratio, 0.0f, 1.0f);
	}

	pRenderContext->BeginOcclusionQueryDrawing( queryHandle );
	CMeshBuilder meshBuilder;
	IMesh* pMesh = pRenderContext->GetDynamicMesh( false, nullptr, nullptr, pMaterial );
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, 4 );
	// draw a pyramid
	for ( int i = 0; i < 4; i++ )
	{
		int a = i+1;
		int b = (a%4)+1;
		meshBuilder.Position3fv( verts[0].Base() );
		meshBuilder.AdvanceVertex();
		meshBuilder.Position3fv( verts[a].Base() );
		meshBuilder.AdvanceVertex();
		meshBuilder.Position3fv( verts[b].Base() );
		meshBuilder.AdvanceVertex();
	}
	meshBuilder.End();
	pMesh->Draw();

	// sprite/quad proxy
	pRenderContext->EndOcclusionQueryDrawing( queryHandle );

	// fraction clipped by frustum
	return ratio;
}

class CPixelVisSet
{
public:
	void Init( const pixelvis_queryparams_t &params );
	void MarkActive();
	bool IsActive();
	CPixelVisSet()
	{
		frameIssued = 0;
		serial = 0;
		sizeIsScreenSpace = false;
	}

public:
	float			proxySize;
	float			proxyAspect;
	float			fadeTimeInv;
	std::vector<unsigned short> queryList;	// indices into CPixelVisibilitySystem::m_queryList
	unsigned short	serial;
	bool			sizeIsScreenSpace;
private:
	int				frameIssued;
};

void CPixelVisSet::Init( const pixelvis_queryparams_t &params )
{
	contract_assert( params.bSetup );
	proxySize = params.proxySize;
	proxyAspect = params.proxyAspect;
	if ( params.fadeTime > 0.0f )
	{
		fadeTimeInv = 1.0f / params.fadeTime;
	}
	else
	{
		// fade in over 0.125 seconds
		fadeTimeInv = 1.0f / 0.125f;
	}
	frameIssued = 0;
	sizeIsScreenSpace = params.bSizeInScreenspace;
}

void CPixelVisSet::MarkActive()
{
	frameIssued = gpGlobals->framecount;
}

bool CPixelVisSet::IsActive()
{
	return (gpGlobals->framecount - frameIssued) > 1 ? false : true;
}

class CPixelVisibilityQuery
{
public:
	CPixelVisibilityQuery();
	~CPixelVisibilityQuery();
	bool IsValid();
	bool IsForView( int nPlayerSlot, int viewID );
	bool IsActive();
	float GetFractionVisible( float fadeTimeInv );
	void IssueQuery( IMatRenderContext *pRenderContext, float proxySize, float proxyAspect, IMaterial *pMaterial, bool sizeIsScreenSpace );
	void IssueCountingQuery( IMatRenderContext *pRenderContext, float proxySize, float proxyAspect, IMaterial *pMaterial, bool sizeIsScreenSpace );
	void ResetOcclusionQueries();
	void SetView( int nPlayerSlot, int viewID ) 
	{
		// This is necessary since player slot is stored in 2 bits
		static_assert( MAX_SPLITSCREEN_PLAYERS <= 4 );

		contract_assert( nPlayerSlot >= 0 && nPlayerSlot < MAX_SPLITSCREEN_PLAYERS );
		m_viewID = viewID;	
		m_brightnessTarget = 0.0f;
		m_clipFraction = 1.0f;
		m_frameIssued = -1;
		m_failed = false;
		m_wasQueriedThisFrame = false;
		m_hasValidQueryResults = false;
		m_nPlayerSlot = nPlayerSlot;
	}

public:
	Vector							m_origin;
	int								m_frameIssued;
private:
	float							m_brightnessTarget;
	float							m_clipFraction;
	OcclusionQueryObjectHandle_t	m_queryHandle;
	OcclusionQueryObjectHandle_t	m_queryHandleCount;
	unsigned short					m_wasQueriedThisFrame : 1;
	unsigned short					m_failed : 1;
	unsigned short					m_hasValidQueryResults : 1;
	unsigned short					m_nPlayerSlot : 2;
	unsigned short					m_pad : 11;
	unsigned short					m_viewID;

	friend void PixelVisibility_ShiftVisibilityViews( int nPlayerSlot, int iSourceViewID, int iDestViewID ); //need direct access to private data to make shifting smooth
};

CPixelVisibilityQuery::CPixelVisibilityQuery()
{
	CMatRenderContextPtr pRenderContext( materials );
	SetView( 0, 0xFFFF );
	m_queryHandle = pRenderContext->CreateOcclusionQueryObject();
	m_queryHandleCount = pRenderContext->CreateOcclusionQueryObject();
}

CPixelVisibilityQuery::~CPixelVisibilityQuery()
{
	CMatRenderContextPtr pRenderContext( materials );
	if ( m_queryHandle != INVALID_OCCLUSION_QUERY_OBJECT_HANDLE )
	{
		pRenderContext->DestroyOcclusionQueryObject( m_queryHandle );
	}
	if ( m_queryHandleCount != INVALID_OCCLUSION_QUERY_OBJECT_HANDLE )
	{
		pRenderContext->DestroyOcclusionQueryObject( m_queryHandleCount );
	}
}

void CPixelVisibilityQuery::ResetOcclusionQueries()
{
	// NOTE: Since we're keeping the CPixelVisibilityQuery objects around in a pool
	// and not actually deleting them, this means that our material system occlusion queries are
	// not being deleted either. Which means that if a CPixelVisibilityQuery is 
	// put into the free list and then immediately re-used, then we have an opportunity for
	// a bug: What can happen on the first frame of the material system query
	// is that if the query isn't done yet, it will use the last queried value
	// which will happen to be set to the value of the last query done
	// for the previous CPixelVisSet the CPixelVisibilityQuery happened to be associated with
	// which makes queries have an invalid value for the first frame

	// This will mark the occlusion query objects as not ever having been read from before
	CMatRenderContextPtr pRenderContext( materials );
	if ( m_queryHandle != INVALID_OCCLUSION_QUERY_OBJECT_HANDLE )
	{
		pRenderContext->ResetOcclusionQueryObject( m_queryHandle );
	}
	if ( m_queryHandleCount != INVALID_OCCLUSION_QUERY_OBJECT_HANDLE )
	{
		pRenderContext->ResetOcclusionQueryObject( m_queryHandleCount );
	}
}

bool CPixelVisibilityQuery::IsValid()
{
	return (m_queryHandle != INVALID_OCCLUSION_QUERY_OBJECT_HANDLE) ? true : false;
}

bool CPixelVisibilityQuery::IsForView( int nPlayerSlot, int viewID ) 
{ 
	return ( m_viewID == viewID ) && ( nPlayerSlot == m_nPlayerSlot ); 
}

bool CPixelVisibilityQuery::IsActive()
{
	return ( gpGlobals->framecount - m_frameIssued ) > 1 ? false : true;
}

float CPixelVisibilityQuery::GetFractionVisible( float fadeTimeInv )
{

	if ( !IsValid() )
		return 0.0f;

	if ( !m_wasQueriedThisFrame )
	{
		CMatRenderContextPtr pRenderContext( materials );
		m_wasQueriedThisFrame = true;
		int pixels = -1;
		int pixelsPossible = -1;
		if ( r_pixelvisibility_partial.GetBool() )
		{
			if ( m_frameIssued != -1 )
			{
				pixelsPossible = pRenderContext->OcclusionQuery_GetNumPixelsRendered( m_queryHandleCount );
				pixels = pRenderContext->OcclusionQuery_GetNumPixelsRendered( m_queryHandle );
			}

			if ( r_pixelvisibility_spew.GetBool() && CurrentViewID() == 0 ) 
			{
				DevMsg( 1, "Pixels visible: %d (qh:%p) Pixels possible: %d (qh:%p) (frame:%d)\n", pixels, m_queryHandle, pixelsPossible, m_queryHandleCount, gpGlobals->framecount );
			}

			if ( pixels < 0 || pixelsPossible < 0 )
			{
				m_failed = ( m_frameIssued >= 0 ) ? true : false;
				return m_brightnessTarget * m_clipFraction;
			}
			m_hasValidQueryResults = true;

			if ( pixelsPossible > 0 )
			{
				float target = (float)pixels / (float)pixelsPossible;
				target = (target >= 0.95f) ? 1.0f : (target < 0.0f) ? 0.0f : target;
				float rate = gpGlobals->frametime * fadeTimeInv;
				m_brightnessTarget = Approach( target, m_brightnessTarget, rate ); // fade in / out
			}
			else
			{
				m_brightnessTarget = 0.0f;
			}
		}
		else
		{
			if ( m_frameIssued != -1 )
			{
				pixels = pRenderContext->OcclusionQuery_GetNumPixelsRendered( m_queryHandle );
			}

			if ( r_pixelvisibility_spew.GetBool() && CurrentViewID() == 0 ) 
			{
				DevMsg( 1, "Pixels visible: %d (qh:%p) (frame:%d)\n", pixels, m_queryHandle, gpGlobals->framecount );
			}

			if ( pixels < 0 )
			{
				m_failed = ( m_frameIssued >= 0 ) ? true : false;
				return m_brightnessTarget * m_clipFraction;
			}
			m_hasValidQueryResults = true;
			if ( m_frameIssued == gpGlobals->framecount-1 )
			{
				float rate = gpGlobals->frametime * fadeTimeInv;
				float target = 0.0f;
				if ( pixels > 0 )
				{
					// fade in slower than you fade out
					rate *= 0.5f;
					target = 1.0f;
				}
				m_brightnessTarget = Approach( target, m_brightnessTarget, rate ); // fade in / out
			}
			else
			{
				m_brightnessTarget = 0.0f;
			}
		}
	}

	return m_brightnessTarget * m_clipFraction;
}

void CPixelVisibilityQuery::IssueQuery( IMatRenderContext *pRenderContext, float proxySize, float proxyAspect, IMaterial *pMaterial, bool sizeIsScreenSpace )
{
	if ( !m_failed )
	{

		if ( r_pixelvisibility_spew.GetBool() && CurrentViewID() == 0 ) 
		{
			DevMsg( 1, "Draw Proxy: qh:%p org:<%d,%d,%d> (frame:%d)\n", m_queryHandle, (int)m_origin[0], (int)m_origin[1], (int)m_origin[2], gpGlobals->framecount );
		}

		m_clipFraction = PixelVisibility_DrawProxy( pRenderContext, m_queryHandle, m_origin, proxySize, proxyAspect, pMaterial, sizeIsScreenSpace );
		if ( m_clipFraction < 0 )
		{
			// NOTE: In this case, the proxy wasn't issued cause it was offscreen
			// can't set the m_frameissued[ slot ] field since that would cause it to get marked as failed
			m_clipFraction = 0;
			m_wasQueriedThisFrame = false;
			m_failed = false;
			return;
		}
	}
	// In split screen we can issue these multiple times
	contract_assert( m_frameIssued != gpGlobals->framecount );

	m_frameIssued = gpGlobals->framecount;
	m_wasQueriedThisFrame = false;
	m_failed = false;
}

void CPixelVisibilityQuery::IssueCountingQuery( IMatRenderContext *pRenderContext, float proxySize, float proxyAspect, IMaterial *pMaterial, bool sizeIsScreenSpace )
{
	if ( !m_failed )
	{
		PixelVisibility_DrawProxy( pRenderContext, m_queryHandleCount, m_origin, proxySize, proxyAspect, pMaterial, sizeIsScreenSpace );
	}
}

//Precache the effects
PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheOcclusionProxy )
PRECACHE( MATERIAL, "engine/occlusionproxy" )
PRECACHE( MATERIAL, "engine/occlusionproxy_countdraw" )
PRECACHE_REGISTER_END()

class CPixelVisibilitySystem : public CAutoGameSystem
{
public:
	
	// GameSystem: Level init, shutdown
	virtual void LevelInitPreEntity();
	virtual void LevelShutdownPostEntity();

	// locals
	CPixelVisibilitySystem();
	float GetFractionVisible( const pixelvis_queryparams_t &params, pixelvis_handle_t *queryHandle );
	void EndView();
	void EndScene();
	unsigned short FindQueryForView( CPixelVisSet *pSet, int nPlayerSlot, int viewID );
	unsigned short FindOrCreateQueryForView( CPixelVisSet *pSet, int nPlayerSlot, int viewID );

	void DeleteUnusedQueries( CPixelVisSet *pSet, bool bDeleteAll );
	void DeleteUnusedSets( bool bDeleteAll );
	void ShowQueries( bool show );
	unsigned short AllocQuery();
	unsigned short AllocSet();
	static constexpr unsigned short INVALID_QUERY = 0xFFFF;
	CPixelVisSet *FindOrCreatePixelVisSet( const pixelvis_queryparams_t &params, pixelvis_handle_t *queryHandle );
	bool SupportsOcclusion() { return m_hwCanTestGlows; }
	void DebugInfo()
	{
		Msg("Pixel vis system using %d sets total (%d in free list), %d queries total (%d in free list)\n", 
			(int)m_setList.size(), (int)m_freeSetsList.size(), (int)m_queryList.size(), (int)m_freeQueriesList.size() );
	}

private:
	void FreeActiveSet( size_t activeIndex );

	// Pools never shrink before level shutdown: queries keep their occlusion query objects, sets their serials.
	// Deques so elements never move; an index is a node.
	std::deque< CPixelVisSet >	m_setList;
	std::deque< CPixelVisibilityQuery > m_queryList;
	std::vector< unsigned short > m_freeQueriesList;
	std::vector< unsigned short > m_activeSetsList;
	std::vector< unsigned short > m_freeSetsList;

	IMaterial	*m_pProxyMaterial;
	IMaterial	*m_pDrawMaterial;
	bool		m_hwCanTestGlows;
	bool		m_drawQueries;

	friend void PixelVisibility_ShiftVisibilityViews( int nPlayerSlot, int iSourceViewID, int iDestViewID ); //need direct access to private data to make shifting smooth
};

static CPixelVisibilitySystem g_PixelVisibilitySystem;

CPixelVisibilitySystem::CPixelVisibilitySystem() : CAutoGameSystem( "CPixelVisibilitySystem" )
{
	m_hwCanTestGlows = true;
	m_drawQueries = false;
}
// Level init, shutdown
void CPixelVisibilitySystem::LevelInitPreEntity()
{
	m_hwCanTestGlows = r_dopixelvisibility.GetBool();
	if ( m_hwCanTestGlows )
	{
		CMatRenderContextPtr pRenderContext( materials );

		OcclusionQueryObjectHandle_t query = pRenderContext->CreateOcclusionQueryObject();
		if ( query != INVALID_OCCLUSION_QUERY_OBJECT_HANDLE )
		{
			pRenderContext->DestroyOcclusionQueryObject( query );
		}
		else
		{
			m_hwCanTestGlows = false;
		}
	}

	m_pProxyMaterial = materials->FindMaterial("engine/occlusionproxy", TEXTURE_GROUP_CLIENT_EFFECTS);
	m_pProxyMaterial->IncrementReferenceCount();
	m_pDrawMaterial = materials->FindMaterial("engine/occlusionproxy_countdraw", TEXTURE_GROUP_CLIENT_EFFECTS);
	m_pDrawMaterial->IncrementReferenceCount();
}

void CPixelVisibilitySystem::LevelShutdownPostEntity()
{
	m_pProxyMaterial->DecrementReferenceCount();
	m_pProxyMaterial = nullptr;
	m_pDrawMaterial->DecrementReferenceCount();
	m_pDrawMaterial = nullptr;
	DeleteUnusedSets(true);
	m_setList.clear();
	m_queryList.clear();
	m_freeQueriesList.clear();
	m_activeSetsList.clear();
	m_freeSetsList.clear();
}

float CPixelVisibilitySystem::GetFractionVisible( const pixelvis_queryparams_t &params, pixelvis_handle_t *queryHandle )
{
	if ( !m_hwCanTestGlows || building_cubemaps.GetBool() )
	{
		return GlowSightDistance( params.position, true ) > 0 ? 1.0f : 0.0f;
	}

	if ( CurrentViewID() < 0 )
		return 0.0f;

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nPlayerSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	CPixelVisSet *pSet = FindOrCreatePixelVisSet( params, queryHandle );
	contract_assert( pSet );
	unsigned short node = FindOrCreateQueryForView( pSet, nPlayerSlot, CurrentViewID() );
	m_queryList[node].m_origin = params.position;
	float fraction = m_queryList[node].GetFractionVisible( pSet->fadeTimeInv );
	pSet->MarkActive();
	return fraction;
}

void CPixelVisibilitySystem::EndView()
{
	if ( !PixelVisibility_IsAvailable() && CurrentViewID() >= 0 )
		return;
	
	if ( m_activeSetsList.empty() )
		return;

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nPlayerSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	CMatRenderContextPtr pRenderContext( materials );

	IMaterial *pProxy = m_drawQueries ? m_pDrawMaterial : m_pProxyMaterial;
	pRenderContext->Bind( pProxy );

	// BUGBUG: If you draw both queries, the measure query fails for some reason.
	if ( r_pixelvisibility_partial.GetBool() && !m_drawQueries )
	{
		pRenderContext->DepthRange( 0.0f, 0.01f );
		for ( unsigned short node : m_activeSetsList )
		{
			CPixelVisSet *pSet = &m_setList[node];
			unsigned short queryNode = FindQueryForView( pSet, nPlayerSlot, CurrentViewID() );
			if ( queryNode != INVALID_QUERY )
			{
				m_queryList[queryNode].IssueCountingQuery( pRenderContext, pSet->proxySize, pSet->proxyAspect, pProxy, pSet->sizeIsScreenSpace );
			}
		}
		pRenderContext->DepthRange( 0.0f, 1.0f );
	}

	{
		for ( unsigned short node : m_activeSetsList )
		{
			CPixelVisSet *pSet = &m_setList[node];
			unsigned short queryNode = FindQueryForView( pSet, nPlayerSlot, CurrentViewID() );
			if ( queryNode != INVALID_QUERY )
			{
				m_queryList[queryNode].IssueQuery( pRenderContext, pSet->proxySize, pSet->proxyAspect, pProxy, pSet->sizeIsScreenSpace );
			}
		}
	}
}

void CPixelVisibilitySystem::EndScene()
{
	DeleteUnusedSets(false);
}

unsigned short CPixelVisibilitySystem::FindQueryForView( CPixelVisSet *pSet, int nPlayerSlot, int viewID )
{
	for ( unsigned short node : pSet->queryList )
	{
		if ( m_queryList[node].IsForView( nPlayerSlot, viewID ) )
			return node;
	}
	return INVALID_QUERY;
}
unsigned short CPixelVisibilitySystem::FindOrCreateQueryForView( CPixelVisSet *pSet, int nPlayerSlot, int viewID )
{
	unsigned short node = FindQueryForView( pSet, nPlayerSlot, viewID );
	if ( node != INVALID_QUERY )
		return node;

	node = AllocQuery();
	pSet->queryList.push_back( node );
	m_queryList[node].SetView( nPlayerSlot, viewID );
	return node;
}

void CPixelVisibilitySystem::DeleteUnusedQueries( CPixelVisSet *pSet, bool bDeleteAll )
{
	std::erase_if( pSet->queryList, [&]( unsigned short node )
	{
		if ( !bDeleteAll && m_queryList[node].IsActive() )
			return false;
		m_freeQueriesList.push_back( node );
		return true;
	} );
}
void CPixelVisibilitySystem::DeleteUnusedSets( bool bDeleteAll )
{
	for ( size_t i = 0; i < m_activeSetsList.size(); )
	{
		CPixelVisSet *pSet = &m_setList[m_activeSetsList[i]];
		if ( bDeleteAll || !pSet->IsActive() )
		{
			DeleteUnusedQueries( pSet, true );
		}
		else
		{
			DeleteUnusedQueries( pSet, false );
		}
		if ( pSet->queryList.empty() )
		{
			FreeActiveSet( i );
			continue;
		}
		++i;
	}
}

void CPixelVisibilitySystem::ShowQueries( bool show )
{
	m_drawQueries = show;
}

unsigned short CPixelVisibilitySystem::AllocQuery()
{
	unsigned short node;
	if ( !m_freeQueriesList.empty() )
	{
		node = m_freeQueriesList.back();
		m_freeQueriesList.pop_back();
		m_queryList[node].ResetOcclusionQueries();
	}
	else
	{
		node = (unsigned short)m_queryList.size();
		m_queryList.emplace_back();
	}
	return node;
}

unsigned short CPixelVisibilitySystem::AllocSet()
{
	unsigned short node;
	if ( !m_freeSetsList.empty() )
	{
		node = m_freeSetsList.back();
		m_freeSetsList.pop_back();
	}
	else
	{
		node = (unsigned short)m_setList.size();
		m_setList.emplace_back();
	}
	m_activeSetsList.push_back( node );
	return node;
}

void CPixelVisibilitySystem::FreeActiveSet( size_t activeIndex )
{
	unsigned short node = m_activeSetsList[activeIndex];
	m_activeSetsList.erase( m_activeSetsList.begin() + activeIndex );
	m_freeSetsList.push_back( node );
	m_setList[node].serial++;
}

CPixelVisSet *CPixelVisibilitySystem::FindOrCreatePixelVisSet( const pixelvis_queryparams_t &params, pixelvis_handle_t *queryHandle )
{
	if ( queryHandle[0] )
	{
		unsigned short handle = queryHandle[0] & 0xFFFF;
		handle--;
		unsigned short serial = queryHandle[0] >> 16;
		if ( handle < m_setList.size() && m_setList[handle].serial == serial )
		{
			return &m_setList[handle];
		}
	}

	unsigned short node = AllocSet();
	m_setList[node].Init( params );
	unsigned int out = m_setList[node].serial;
	unsigned short nodeHandle = node + 1;
	out <<= 16;
	out |= nodeHandle;
	queryHandle[0] = out;
	return &m_setList[node];
}

void PixelvisDrawChanged( ConVar *pPixelvisVar, const char *pOld, float flOldValue )
{
	ConVarRef var( pPixelvisVar );
	g_PixelVisibilitySystem.ShowQueries( var.GetBool() );
}

class CTraceFilterGlow : public CTraceFilterSimple
{
public:
	DECLARE_CLASS( CTraceFilterGlow, CTraceFilterSimple );
	
	CTraceFilterGlow( const IHandleEntity *passentity, int collisionGroup ) : CTraceFilterSimple(passentity, collisionGroup) {}
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		IClientUnknown *pUnk = (IClientUnknown*)pHandleEntity;
		ICollideable *pCollide = pUnk->GetCollideable();
		if ( pCollide->GetSolid() != SOLID_VPHYSICS && pCollide->GetSolid() != SOLID_BSP )
			return false;
		return BaseClass::ShouldHitEntity( pHandleEntity, contentsMask );
	}
};
float GlowSightDistance( const Vector &glowOrigin, bool bShouldTrace )
{
	float dist = (glowOrigin - CurrentViewOrigin()).Length();
	CBasePlayer *local = CBasePlayer::GetLocalPlayer();
	if ( local )
	{
		dist *= local->GetFOVDistanceAdjustFactor();
	}

	if ( bShouldTrace )
	{
		Vector end = glowOrigin;
		// HACKHACK: trace 4" from destination in case the glow is inside some parent object
		//			allow a little error...
		if ( dist > 4 )
		{
			end -= CurrentViewForward()*4;
		}
		int traceFlags = MASK_OPAQUE|CONTENTS_MONSTER|CONTENTS_DEBRIS;
		
		CTraceFilterGlow filter(nullptr, COLLISION_GROUP_NONE);
		trace_t tr;
		UTIL_TraceLine( CurrentViewOrigin(), end, traceFlags, &filter, &tr );
		if ( tr.fraction != 1.0f )
			return -1;
	}

	return dist;
}

void PixelVisibility_EndCurrentView()
{
	g_PixelVisibilitySystem.EndView();
}

void PixelVisibility_EndScene()
{
	g_PixelVisibilitySystem.EndScene();
}

float PixelVisibility_FractionVisible( const pixelvis_queryparams_t &params, pixelvis_handle_t *queryHandle )
{
	if ( !queryHandle )
	{
		return GlowSightDistance( params.position, true ) > 0.0f ? 1.0f : 0.0f;
	}
	else 
	{
		return g_PixelVisibilitySystem.GetFractionVisible( params, queryHandle );
	}
}

bool PixelVisibility_IsAvailable()
{
	return r_dopixelvisibility.GetBool() && g_PixelVisibilitySystem.SupportsOcclusion();
}

//this originally called a class function of CPixelVisibiltySystem to keep the work clean, but that function needed friend access to CPixelVisibilityQuery 
//and I didn't want to make the whole class a friend or shift all the functions and class declarations around in this file
void PixelVisibility_ShiftVisibilityViews( int nPlayerSlot, int iSourceViewID, int iDestViewID )
{
	CPixelVisibilitySystem &sys = g_PixelVisibilitySystem;
	for ( size_t i = 0; i < sys.m_activeSetsList.size(); )
	{
		CPixelVisSet *pSet = &sys.m_setList[sys.m_activeSetsList[i]];

		unsigned short iSourceQueryNode = sys.FindQueryForView( pSet, nPlayerSlot, iSourceViewID );
		unsigned short iDestQueryNode = sys.FindQueryForView( pSet, nPlayerSlot, iDestViewID );

		bool bFreed = false;
		if( iDestQueryNode != CPixelVisibilitySystem::INVALID_QUERY )
		{
			//delete the destination if found
			ks::erase_first( pSet->queryList, iDestQueryNode );
			sys.m_freeQueriesList.push_back( iDestQueryNode );

			if ( pSet->queryList.empty() )
			{
				sys.FreeActiveSet( i );
				bFreed = true;
			}
		}

		if( iSourceQueryNode != CPixelVisibilitySystem::INVALID_QUERY )
		{
			//make the source believe it's the destination
			sys.m_queryList[iSourceQueryNode].m_viewID = iDestViewID;
		}		

		if ( !bFreed )
			++i;
	}

	for( int i = 0; i != std::ssize( s_OcclusionQueries ); ++i )
	{
		int iPairCount = std::ssize( s_OcclusionQueries[i].occlusionHandles[nPlayerSlot] );
		OcclusionHandleViewIDPair_t *pPairs = s_OcclusionQueries[i].occlusionHandles[nPlayerSlot].data();
		int iSourceIndex, iDestIndex, iInvalidIndex;
		iDestIndex = iSourceIndex = iInvalidIndex = -1;
		for( int j = 0; j != iPairCount; ++j )
		{
			if( pPairs[j].iViewID == iSourceViewID )
			{
				iSourceIndex = j;
				if( iDestIndex != iInvalidIndex )
					break;
			}

			if( pPairs[j].iViewID == iDestViewID )
			{
				iDestIndex = j;
				if( iSourceIndex != iInvalidIndex )
					break;
			}
		}

		if( iSourceIndex != iInvalidIndex )
		{
			//change view id on source
			pPairs[iSourceIndex].iViewID = iDestViewID;
		}
		
		if( iDestIndex != iInvalidIndex )
		{
			//destroy dest
			materials->GetRenderContext()->DestroyOcclusionQueryObject( pPairs[iDestIndex].hOcclusionHandle );
			ks::swap_remove( s_OcclusionQueries[i].occlusionHandles[nPlayerSlot], iDestIndex );
		}
	}
}

COcclusionQuerySet::COcclusionQuerySet( void )
{	
	OcclusionQueryHiddenData_t &data = s_OcclusionQueries.emplace_back();
	data.pOwner = this;
	m_pManagedData = &data;

	//handle base address shifting
	if( std::ssize( s_OcclusionQueries ) > 1 )
	{
		OcclusionQueryHiddenData_t &baseData = s_OcclusionQueries[0];
		if( baseData.pOwner->m_pManagedData != &baseData )
		{
			for( int i = 0; i != std::ssize( s_OcclusionQueries ); ++i )
			{
				s_OcclusionQueries[i].pOwner->m_pManagedData = &s_OcclusionQueries[i];
			}
		}
	}
}

COcclusionQuerySet::~COcclusionQuerySet( void )
{
	int iIndex;
	for( iIndex = 0; iIndex != std::ssize( s_OcclusionQueries ); ++iIndex )
	{
		if( &s_OcclusionQueries[iIndex] == m_pManagedData )
			break;
	}
	if( iIndex != std::ssize( s_OcclusionQueries ) )
	{
		//destroy query handles
		{
			CMatRenderContextPtr pRenderContext( materials );
			OcclusionQueryHiddenData_t &data = s_OcclusionQueries[iIndex];
			for( int i = 0; i != MAX_SPLITSCREEN_PLAYERS; ++i )
			{
				for( int j = 0; j != std::ssize( data.occlusionHandles[i] ); ++j )
				{
					pRenderContext->DestroyOcclusionQueryObject( data.occlusionHandles[i][ j ].hOcclusionHandle );
				}
			}
		}

		ks::swap_remove( s_OcclusionQueries, iIndex );
		if( !s_OcclusionQueries.empty() )
		{
			s_OcclusionQueries[iIndex].pOwner->m_pManagedData = &s_OcclusionQueries[iIndex];
			//handle base address shifting
			if( std::ssize( s_OcclusionQueries ) > 1 )
			{
				OcclusionQueryHiddenData_t &baseData = s_OcclusionQueries[iIndex == 0 ? 1 : 0];
				if( baseData.pOwner->m_pManagedData != &baseData )
				{
					for( int i = 0; i != std::ssize( s_OcclusionQueries ); ++i )
					{
						s_OcclusionQueries[i].pOwner->m_pManagedData = &s_OcclusionQueries[i];
					}
				}
			}
		}
	}
}

void COcclusionQuerySet::BeginQueryDrawing( int iViewID, int iSplitScreenSlot )
{
	OcclusionQueryHiddenData_t *pData = (OcclusionQueryHiddenData_t *)m_pManagedData;

	int iIndex = FindQueryHandlePairIndex( pData, iViewID, iSplitScreenSlot );
	if( iIndex == -1 )
	{
		//create a new one
		iIndex = std::ssize( pData->occlusionHandles[iSplitScreenSlot] ); pData->occlusionHandles[iSplitScreenSlot].emplace_back();
		OcclusionHandleViewIDPair_t &Entry = pData->occlusionHandles[iSplitScreenSlot][ iIndex ];
		Entry.iViewID = iViewID;
		Entry.hOcclusionHandle = materials->GetRenderContext()->CreateOcclusionQueryObject();	
		materials->GetRenderContext()->ResetOcclusionQueryObject( Entry.hOcclusionHandle );
	}

	materials->GetRenderContext()->BeginOcclusionQueryDrawing( pData->occlusionHandles[iSplitScreenSlot][ iIndex ].hOcclusionHandle );
	pData->occlusionHandles[iSplitScreenSlot][ iIndex ].iLastFrameRendered = gpGlobals->framecount;
}

void COcclusionQuerySet::BeginQueryDrawing( void )
{
	return BeginQueryDrawing( CurrentViewID(), GET_ACTIVE_SPLITSCREEN_SLOT() );
}

void COcclusionQuerySet::EndQueryDrawing( int iViewID, int iSplitScreenSlot )
{
	OcclusionQueryHiddenData_t *pData = (OcclusionQueryHiddenData_t *)m_pManagedData;

	int iIndex = FindQueryHandlePairIndex( pData, iViewID, iSplitScreenSlot );
	if( iIndex != -1 )
	{
		materials->GetRenderContext()->EndOcclusionQueryDrawing( pData->occlusionHandles[iSplitScreenSlot][ iIndex ].hOcclusionHandle );
	}
}

void COcclusionQuerySet::EndQueryDrawing( void )
{
	return EndQueryDrawing( CurrentViewID(), GET_ACTIVE_SPLITSCREEN_SLOT() );
}

int COcclusionQuerySet::QueryNumPixelsRendered( int iViewID, int iSplitScreenSlot )
{
	OcclusionQueryHiddenData_t *pData = (OcclusionQueryHiddenData_t *)m_pManagedData;

	int iIndex = FindQueryHandlePairIndex( pData, iViewID, iSplitScreenSlot );
	if( iIndex != -1 )
	{
		return materials->GetRenderContext()->OcclusionQuery_GetNumPixelsRendered( pData->occlusionHandles[iSplitScreenSlot][ iIndex ].hOcclusionHandle );
	}

	return 0;
}

int COcclusionQuerySet::QueryNumPixelsRendered( void )
{
	return QueryNumPixelsRendered( CurrentViewID(), GET_ACTIVE_SPLITSCREEN_SLOT() );
}

float COcclusionQuerySet::QueryPercentageOfScreenRendered( int iViewID, int iSplitScreenSlot )
{
	OcclusionQueryHiddenData_t *pData = (OcclusionQueryHiddenData_t *)m_pManagedData;

	int iIndex = FindQueryHandlePairIndex( pData, iViewID, iSplitScreenSlot );
	if( iIndex != -1 )
	{
		int iX, iY, iWidth, iHeight;
		materials->GetRenderContext()->GetViewport( iX, iY, iWidth, iHeight );
		return ((float)materials->GetRenderContext()->OcclusionQuery_GetNumPixelsRendered( pData->occlusionHandles[iSplitScreenSlot][ iIndex ].hOcclusionHandle )) / ((float)(iWidth * iHeight));
	}

	return 0.0f;
}

float COcclusionQuerySet::QueryPercentageOfScreenRendered( void )
{
	return QueryPercentageOfScreenRendered( CurrentViewID(), GET_ACTIVE_SPLITSCREEN_SLOT() );
}

int COcclusionQuerySet::QueryNumPixelsRenderedForAllViewsLastFrame( int iSplitScreenSlot )
{
	OcclusionQueryHiddenData_t *pData = (OcclusionQueryHiddenData_t *)m_pManagedData;
	int iMatchFrame = gpGlobals->framecount - 1;
	int iResult = 0;
	CMatRenderContextPtr pRenderContext( materials );
	
	for( int i = 0; i != std::ssize( pData->occlusionHandles[iSplitScreenSlot] ); ++i )
	{
		if( pData->occlusionHandles[iSplitScreenSlot][ i ].iLastFrameRendered == iMatchFrame )
		{
			iResult += pRenderContext->OcclusionQuery_GetNumPixelsRendered( pData->occlusionHandles[iSplitScreenSlot][ i ].hOcclusionHandle );
		}
	}

	return iResult;
}

int COcclusionQuerySet::QueryNumPixelsRenderedForAllViewsLastFrame( void )
{
	return QueryNumPixelsRenderedForAllViewsLastFrame( GET_ACTIVE_SPLITSCREEN_SLOT() );
}

int COcclusionQuerySet::GetLastFrameDrawn( int iViewID, int iSplitScreenSlot )
{
	OcclusionQueryHiddenData_t *pData = (OcclusionQueryHiddenData_t *)m_pManagedData;

	int iIndex = FindQueryHandlePairIndex( pData, iViewID, iSplitScreenSlot );
	if( iIndex != -1 )
	{
		return pData->occlusionHandles[iSplitScreenSlot][ iIndex ].iLastFrameRendered;
	}

	return -1;
}

int COcclusionQuerySet::GetLastFrameDrawn( void )
{
	return GetLastFrameDrawn( CurrentViewID(), GET_ACTIVE_SPLITSCREEN_SLOT() );
}

static void pixelvis_debug( const CCommand &args );
static ConCommand pixelvis_debug_command( side_registry(), "pixelvis_debug", pixelvis_debug, "Dump debug info" );
static void pixelvis_debug( const CCommand &args )
{
	g_PixelVisibilitySystem.DebugInfo();
}

class CBaseEntity;

using namespace ::ks::reflect;

#define	NUM_CHILD_FLAMES	6
static constexpr auto CHILD_SPREAD = 40;

//==================================================
// CPlasma
//==================================================

//NOTENOTE: Mirrored in dlls/fire_smoke.h
#define	bitsFIRESMOKE_NONE				0x00000000
#define	bitsFIRESMOKE_ACTIVE			0x00000001

class CPlasmaSprite : public CSprite
{
	DECLARE_CLASS( CPlasmaSprite, CSprite );

public:
	Vector	m_vecMoveDir;
};

void RecvProxy_PlasmaScale( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_PlasmaScaleTime( const CRecvProxyData *pData, void *pStruct, void *pOut );

class CPlasma : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_Plasma" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( CPlasma, CBaseEntity );

	CPlasma();
	~CPlasma();

	bool	Simulate( void );

protected:
	void	Update( void );
	void	UpdateAnimation( void );
	void	UpdateScale( void );
	void	UpdateFlames( void );
	void	AddFlames( void );
	void	Start( void );
	
	float	GetFlickerScale( void );

//CBaseEntity
public:

	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual bool	ShouldDraw();

//From the server
public:
	[[= ks::reflect::Net{} ]] float	m_flStartScale;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_PlasmaScale, ks::reflect::WireSide::Recv>{} ]] float	m_flScale;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_PlasmaScaleTime, ks::reflect::WireSide::Recv>{} ]] float	m_flScaleTime;
	[[= ks::reflect::Net{} ]] int		m_nFlags;
	[[= ks::reflect::Net{} ]] int		m_nPlasmaModelIndex;
	[[= ks::reflect::Net{} ]] int		m_nPlasmaModelIndex2;
	[[= ks::reflect::Net{} ]] int		m_nGlowModelIndex;

//Client-side only
public:
	float	m_flScaleRegister;
	float	m_flScaleStart;
	float	m_flScaleEnd;
	float	m_flScaleTimeStart;
	float	m_flScaleTimeEnd;

	VPlane	m_planeClip;
	bool	m_bClipTested;

protected:
	CPlasmaSprite		m_entFlames[NUM_CHILD_FLAMES];
	float				m_entFlameScales[NUM_CHILD_FLAMES];

	CSprite			m_entGlow;
	float				m_flGlowScale;

	TimedEvent			m_tParticleSpawn;
	TimedEvent			m_tDecalSpawn;

private:
	CPlasma( const CPlasma & );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pRecvProp - 
//			*pStruct - 
//			*pVarData - 
//			*pIn - 
//			objectID - 
//-----------------------------------------------------------------------------
void RecvProxy_PlasmaScale( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CPlasma	*pPlasmaSmoke	= (CPlasma	*) pStruct;
	float scale				= pData->m_Value.m_Float;

	//If changed, update our internal information
	if ( pPlasmaSmoke->m_flScale != scale )
	{
		pPlasmaSmoke->m_flScaleStart	= pPlasmaSmoke->m_flScaleRegister;
		pPlasmaSmoke->m_flScaleEnd		= scale;			

		pPlasmaSmoke->m_flScale = scale;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pRecvProp - 
//			*pStruct - 
//			*pVarData - 
//			*pIn - 
//			objectID - 
//-----------------------------------------------------------------------------
void RecvProxy_PlasmaScaleTime( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CPlasma	*pPlasmaSmoke	= (CPlasma	*) pStruct;
	float time				= pData->m_Value.m_Float;

	//If changed, update our internal information
	if ( pPlasmaSmoke->m_flScaleTime != time )
	{
		if ( time == -1.0f )
		{
			pPlasmaSmoke->m_flScaleTimeStart	= gpGlobals->curtime-1.0f;
			pPlasmaSmoke->m_flScaleTimeEnd	= pPlasmaSmoke->m_flScaleTimeStart;
		}
		else
		{
			pPlasmaSmoke->m_flScaleTimeStart	= gpGlobals->curtime;
			pPlasmaSmoke->m_flScaleTimeEnd	= gpGlobals->curtime + time;
		}

		pPlasmaSmoke->m_flScaleTime = time;
	}

}

//Receive datatable
IMPLEMENT_REFLECT_CLIENTCLASS( CPlasma, DT_Plasma, CPlasma )

//==================================================
// CPlasma
//==================================================

CPlasma::CPlasma()
{
	//Server-side
	m_flStartScale		= 0.0f;
	m_flScale			= 0.0f;
	m_flScaleTime		= 0.0f;
	m_nFlags			= bitsFIRESMOKE_NONE;
	m_nPlasmaModelIndex	= 0;
	m_nPlasmaModelIndex2	= 0;
	m_nGlowModelIndex	= 0;

	//Client-side
	m_flScaleRegister	= 0.0f;
	m_flScaleStart		= 0.0f;
	m_flScaleEnd		= 0.0f;
	m_flScaleTimeStart	= 0.0f;
	m_flScaleTimeEnd	= 0.0f;
	m_flGlowScale		= 0.0f;
	m_bClipTested		= false;
	
	m_entGlow.Clear();
	
	//Clear all child flames
	for ( int i = 0; i < NUM_CHILD_FLAMES; i++ )
	{
		m_entFlames[i].Clear();
	}
	AddToEntityList(ENTITY_LIST_SIMULATE);
}

CPlasma::~CPlasma()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CPlasma::GetFlickerScale( void )
{
	float	result = 0.0f;

	result = std::sin( gpGlobals->curtime * 10000.0f );
	result += 0.5f * sin( gpGlobals->curtime * 2000.0f );
	result -= 0.5f * std::cos( gpGlobals->curtime * 8000.0f );
	
	return result * 0.1f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPlasma::Simulate( void )
{
	//Only do this if we're active
	if ( ( m_nFlags & bitsFIRESMOKE_ACTIVE ) == false )
		return false;

	Update();
	AddFlames();

	float	dScale = m_flScaleRegister - m_flGlowScale;
	m_flGlowScale = m_flScaleRegister;

	// Note: Sprite renderer assumes scale of 0.0 is 1.0
	m_entGlow.SetScale( std::max( 0.0000001f, (m_flScaleRegister*1.5f) + GetFlickerScale() ) );
	m_entGlow.SetLocalOriginDim( Z_INDEX, m_entGlow.GetLocalOriginDim( Z_INDEX ) + ( dScale * 32.0f ) );
	return true;
}

static constexpr auto FLAME_ALPHA_START = 0.8f;
static constexpr auto FLAME_ALPHA_END = 1.0f;

static constexpr auto FLAME_TRANS_START = 0.75f;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlasma::AddFlames( void )
{
	Vector	viewDir = GetAbsOrigin() - CurrentViewOrigin();
	VectorNormalize(viewDir);
	float	dot		= viewDir.Dot( Vector( 0, 0, 1 ) );	//NOTENOTE: Flames always point up
	float	alpha	= 1.0f;

	dot = fabs( dot );

	if ( dot < FLAME_ALPHA_START )
	{
		alpha = 1.0f;
	}

	for ( int i = 0; i < NUM_CHILD_FLAMES; i++ )
	{
		if ( m_entFlames[i].GetScale() > 0.0f )
		{
			m_entFlames[i].SetRenderColor( ( 255.0f * alpha ), ( 255.0f * alpha ), ( 255.0f * alpha ) );
			m_entFlames[i].SetBrightness( 255.0f * alpha );
		}

		m_entFlames[i].AddToLeafSystem( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void CPlasma::OnDataChanged( DataUpdateType_t updateType )
{
	CBaseEntity::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		Start();
	}

	if ( m_nFlags & bitsFIRESMOKE_ACTIVE )
	{
		AddToEntityList(ENTITY_LIST_SIMULATE);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPlasma::ShouldDraw()
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlasma::Start( void )
{
	//Various setup info
	m_tParticleSpawn.Init( 10.0f );
	m_tDecalSpawn.Init( 20.0f);

	QAngle	offset;
	int		maxFrames;

	// Setup the child flames
	int i;
	for ( i = 0; i < NUM_CHILD_FLAMES; i++ )
	{
		//Setup our offset angles
		offset[0] = 0.0f;
		offset[1] = random_valve->RandomFloat( 0, 360 );
		offset[2] = 0.0f;
	
  		AngleVectors( offset, &m_entFlames[i].m_vecMoveDir );
		
		int	nModelIndex = ( i % 2 ) ? m_nPlasmaModelIndex : m_nPlasmaModelIndex2;

		model_t *pModel	= (model_t *) modelinfo->GetModel( nModelIndex );
		maxFrames	= modelinfo->GetModelFrameCount( pModel );

		// Setup all the information for the client entity
		m_entFlames[i].SetModelByIndex( nModelIndex );
		m_entFlames[i].SetLocalOrigin( GetLocalOrigin() );
		m_entFlames[i].m_flFrame			= random_valve->RandomInt( 0.0f, maxFrames );
		m_entFlames[i].m_flSpriteFramerate	= (float) random_valve->RandomInt( 15, 20 );
		m_entFlames[i].SetScale( m_flStartScale );
		m_entFlames[i].SetRenderMode( kRenderTransAddFrameBlend );
		m_entFlames[i].SetRenderFX( kRenderFxNone );
		m_entFlames[i].SetRenderColor( 255, 255, 255 );
		m_entFlames[i].SetRenderAlpha( 255 );
		m_entFlames[i].SetBrightness( 255 );
		m_entFlames[i].index				= -1;
		
		if ( i == 0 )
		{
			m_entFlameScales[i] = 1.0f;
		}
		else
		{
			//Keep a scale offset
			m_entFlameScales[i] = 1.0f - ( ( (float) i / (float) NUM_CHILD_FLAMES ) );
		}
	}

	// Setup the glow
	m_entGlow.SetModelByIndex( m_nGlowModelIndex );
	m_entGlow.SetLocalOrigin( GetLocalOrigin() );
	m_entGlow.SetScale( m_flStartScale );
	m_entGlow.SetRenderMode( kRenderTransAdd );
	m_entGlow.SetRenderFX( kRenderFxNone );
	m_entGlow.SetRenderColor( 255, 255, 255 );
	m_entGlow.SetRenderAlpha( 255 );
	m_entGlow.SetBrightness( 255 );
	m_entGlow.index				= -1;
	
	m_flGlowScale				= m_flStartScale;

	m_entGlow.AddToLeafSystem( false );

	for( i=0; i < NUM_CHILD_FLAMES; i++ )
	{
		m_entFlames[i].AddToLeafSystem( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlasma::UpdateAnimation( void )
{
	int		numFrames;
	float	frametime	= gpGlobals->frametime;

	for ( int i = 0; i < NUM_CHILD_FLAMES; i++ )
	{
		m_entFlames[i].m_flFrame += m_entFlames[i].m_flSpriteFramerate * frametime;

		numFrames = modelinfo->GetModelFrameCount( m_entFlames[i].GetModel() );

		if ( m_entFlames[i].m_flFrame >= numFrames )
		{
			m_entFlames[i].m_flFrame = m_entFlames[i].m_flFrame - (int)(m_entFlames[i].m_flFrame);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlasma::UpdateFlames( void )
{
	for ( int i = 0; i < NUM_CHILD_FLAMES; i++ )
	{
		float	newScale = m_flScaleRegister * m_entFlameScales[i];
		float	dScale = newScale - m_entFlames[i].GetScale();

		Vector dir;

		dir[2] = 0.0f;
		VectorNormalize( dir );
		dir[2] = 0.0f;

		Vector	offset = GetAbsOrigin();
		offset[2] = m_entFlames[i].GetAbsOrigin()[2];

		// Note: Sprite render assumes 0 scale means 1.0
		m_entFlames[i].SetScale ( std::max<std::common_type_t<double, float>>(0.000001,newScale) );
		
		if ( i != 0 )
		{
			m_entFlames[i].SetLocalOrigin( offset + ( m_entFlames[i].m_vecMoveDir * ((m_entFlames[i].GetScale())*CHILD_SPREAD) ) );
		}

		contract_assert( !m_entFlames[i].GetMoveParent() );
		m_entFlames[i].SetLocalOriginDim( Z_INDEX, m_entFlames[i].GetLocalOriginDim( Z_INDEX ) + ( dScale * 64.0f ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlasma::UpdateScale( void )
{
	float	time = gpGlobals->curtime;

	if ( m_flScaleRegister != m_flScaleEnd )
	{
		//See if we're done scaling
		if ( time > m_flScaleTimeEnd )
		{
			m_flScaleRegister = m_flStartScale = m_flScaleEnd;
		}
		else
		{
			//Lerp the scale and set it
			float	timeFraction = 1.0f - ( m_flScaleTimeEnd - time ) / ( m_flScaleTimeEnd - m_flScaleTimeStart );	
			float	newScale = m_flScaleStart + ( ( m_flScaleEnd - m_flScaleStart ) * timeFraction );

			m_flScaleRegister = m_flStartScale = newScale;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fTimeDelta - 
//-----------------------------------------------------------------------------
void CPlasma::Update( void )
{
	//Update all our parts
	UpdateScale();
	UpdateAnimation();
	UpdateFlames();

	if (m_flScaleRegister > 0.1)
	{
		float tempDelta = gpGlobals->frametime;
		while( m_tDecalSpawn.NextEvent( tempDelta ) )
		{
			// Add decal to floor
			CBaseEntity *ent = cl_entitylist->GetEnt( 0 );
			if ( ent )
			{
				int index = decalsystem->GetDecalIndexForName( "PlasmaGlowFade" );
				if ( index >= 0 )
				{
					effects->DecalShoot( index, 0, ent->GetModel(), ent->GetAbsOrigin(), ent->GetAbsAngles(), GetAbsOrigin(), 0, 0 );
				}
			}
		}
	}
}

#undef NUM_CHILD_FLAMES
#undef bitsFIRESMOKE_NONE
#undef bitsFIRESMOKE_ACTIVE

class CBasePlayer;
class CTeam;

using namespace ::ks::reflect;

// Purpose: Entity that propagates general data needed by clients for every player.

static const float PLAYER_RESOURCE_THINK_INTERVAL = 0.2f;
#define PLAYER_DEBUG_NAME "WWWWWWWWWWWWWWW"

ConVar cl_names_debug( side_registry(), "cl_names_debug", "0", FCVAR_DEVELOPMENTONLY );

void RecvProxy_ChangedTeam( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	// Have the regular proxy store the data.
	RecvProxy_Int32ToInt32( pData, pStruct, pOut );

	if ( g_PR )
	{
		g_PR->TeamChanged();
	}
}

IMPLEMENT_REFLECT_CLIENTCLASS( CPlayerResource, DT_PlayerResource, CPlayerResource )

IMPLEMENT_REFLECT_PREDMAP( CPlayerResource );

CPlayerResource *g_PR;

IGameResources * GameResources( void ) { return g_PR; }

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPlayerResource::CPlayerResource()
{
	for ( int i=0; i<std::size( m_szName ); ++i )
	{
		m_szName[i] = AllocPooledString( "unconnected" );
	}
	memset( m_iPing, 0, sizeof( m_iPing ) );
//	memset( m_iPacketloss, 0, sizeof( m_iPacketloss ) );
	memset( m_iKills, 0, sizeof( m_iKills ) );
	memset( m_iAssists, 0, sizeof( m_iAssists ) );
	memset( m_iDeaths, 0, sizeof( m_iDeaths ) );
	memset( m_bConnected, 0, sizeof( m_bConnected ) );
	memset( m_iTeam, 0, sizeof( m_iTeam ) );
	memset( m_iPendingTeam, 0, sizeof( m_iTeam ) );
	memset( m_bAlive, 0, sizeof( m_bAlive ) );
	memset( m_iHealth, 0, sizeof( m_iHealth ) );
	memset( m_Xuids, 0, sizeof( m_Xuids ) );
	memset( m_iCoachingTeam, 0, sizeof( m_iCoachingTeam) );

	for ( int i=0; i<MAX_TEAMS; i++ )
	{
		m_Colors[i] = COLOR_GREY;
	}

	g_PR = this;
    g_pRocketUI->AddDeviceDependentObject( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPlayerResource::~CPlayerResource()
{
    g_pRocketUI->RemoveDeviceDependentObject( this );
    g_PR = nullptr;
}

void CPlayerResource::OnDataChanged(DataUpdateType_t updateType)
{
	UpdateXuids();

	BaseClass::OnDataChanged( updateType );
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( gpGlobals->curtime + PLAYER_RESOURCE_THINK_INTERVAL );
	}
}

void CPlayerResource::UpdateXuids( void )
{
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		XUID newXuid = INVALID_XUID;
		player_info_t sPlayerInfo;
		if ( m_bConnected[ i ] && engine->GetPlayerInfo( i, &sPlayerInfo ) )
		{
			newXuid = sPlayerInfo.xuid;

			// When running in anonymous mode wipe out all XUIDs
			if ( newXuid != INVALID_XUID )
			{
				if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
				{
					if ( pParameters->m_bAnonymousPlayerIdentity )
					{
// 						CPlayerId steamid( newXuid );
// 						steamid.SetAccountID( ( ~uint32(0) ) -1 - i );
// 						newXuid = steamid.ConvertToUint64();
						newXuid = INVALID_XUID;
					}
				}
			}
		}

		if ( newXuid != m_Xuids[i] )
		{
            bool bAddRefSuccess = true;
            if ( bAddRefSuccess || ( newXuid == INVALID_XUID ) )
			{
				m_Xuids[i] = newXuid;
			}
		}
	}
}

void CPlayerResource::UpdateAsLocalizedFakePlayerName( int slot, char const *pchPlayerName )
{
	static ks::idict< std::string > s_mapLocalizedNames;	// m_szName points into the values
	auto symName = s_mapLocalizedNames.find( pchPlayerName );
	if ( symName == s_mapLocalizedNames.end() )
	{
		// Need to localize it from scratch
		std::vector< std::string > arrLocTokens;
		V_SplitString( pchPlayerName, " ", arrLocTokens );
		CFmtStr1024 strComposite;
		for ( int i = 0; i < std::ssize( arrLocTokens ); i++ )
		{
			if ( i ) strComposite.Append( " " );

			if ( wchar_t const * const kwszLocalizedToken = g_pLocalize->Find( CFmtStr( "#CSGO_FakePlayer_%s", arrLocTokens[ i ].c_str() ) ) )
			{
				char chUtf8token[ MAX_PLAYER_NAME_LENGTH ] = {};
				V_UnicodeToUTF8( kwszLocalizedToken, chUtf8token, sizeof( chUtf8token ) );
				strComposite.Append( chUtf8token );
			}
			else
			{
				strComposite.Append( arrLocTokens[ i ].c_str() );
				Warning( "Failed to localize fake player name '#CSGO_FakePlayer_%s'\n", arrLocTokens[ i ].c_str() );
			}
		}
		symName = s_mapLocalizedNames.emplace( pchPlayerName, strComposite.Access() ).first;
	}

	m_szName[ slot ] = symName->second.c_str();
}

void CPlayerResource::UpdatePlayerName( int slot )
{
	if ( slot < 1 || slot > MAX_PLAYERS )
	{
		Error( "UpdatePlayerName with bogus slot %d\n", slot );
		return;
	}
	player_info_t sPlayerInfo;
	char const *pchPlayerName = PLAYER_UNCONNECTED_NAME;
	if ( IsConnected( slot ) && 
		engine->GetPlayerInfo( slot, &sPlayerInfo ) )
	{
		pchPlayerName = sPlayerInfo.name;

		if ( sPlayerInfo.fakeplayer && *pchPlayerName )
		{
			UpdateAsLocalizedFakePlayerName( slot, pchPlayerName );
			return;
		}
	}

	if ( !m_szName[slot] || V_stricmp( m_szName[slot], pchPlayerName ) )
	{
		m_szName[slot] = AllocPooledString( pchPlayerName );
	}
}

void CPlayerResource::ClientThink()
{
	BaseClass::ClientThink();

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		UpdatePlayerName( i );
	}

	SetNextClientThink( gpGlobals->curtime + PLAYER_RESOURCE_THINK_INTERVAL );
}

#define STATIC_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( s_chStaticArrayVarName, szLocToken, szDefaultText ) \
	static char s_chStaticArrayVarName[MAX_PLAYER_NAME_LENGTH] = {}; \
	if ( !s_chStaticArrayVarName[ 0 ] ) \
	{ \
		wchar_t const * const kwszTheSuspect = g_pLocalize->Find( szLocToken ); \
		contract_assert( kwszTheSuspect ); \
		V_UnicodeToUTF8( kwszTheSuspect, s_chStaticArrayVarName, sizeof( s_chStaticArrayVarName ) ); \
		contract_assert( s_chStaticArrayVarName[ 0 ] ); \
		if ( !s_chStaticArrayVarName[ 0 ] ) \
			V_strcpy_safe( s_chStaticArrayVarName, szDefaultText ); \
	}

class CStaticPlayerNamesSet
{
public:
	CStaticPlayerNamesSet()
	{
		m_bInitialized = false;
		V_memset( m_szNames, 0, sizeof( m_szNames ) );
	}

	char const * GetName( int idx )
	{
		if ( ( idx < 0 ) || ( idx > MAX_PLAYERS ) )
		{
			STATIC_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( s_utf8LocalPlayer, "#SFUI_LocalPlayer", "Player" );
			return s_utf8LocalPlayer;
		}
		else
		{
			if ( !m_bInitialized )
				InitializeNames();

			return m_szNames[idx];
		}
	}

protected:
	bool m_bInitialized;
	char const * m_szNames[MAX_PLAYERS+1];

public:
	void InitializeNames()
	{
		std::vector< const char * > arrNamesOptions;
#define RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( englishName ) { \
	STATIC_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( s_utf8Name##englishName, "#CSGO_FakePlayer_" #englishName, #englishName ); \
	arrNamesOptions.push_back( s_utf8Name##englishName ); }
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Albatross );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Alpha );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Anchor );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Banjo );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Bell );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Beta );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Blackbird );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Bulldog );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Canary );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Cat );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Calf );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Cyclone );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Daisy );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Dalmatian );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Dart );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Delta );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Diamond );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Donkey );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Duck );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Emu );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Eclipse );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Flamingo );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Flute );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Frog );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Goose );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Hatchet );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Heron );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Husky );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Hurricane );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Iceberg );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Iguana );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Kiwi );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Kite );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Lamb );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Lily );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Macaw );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Manatee );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Maple );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Mask );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Nautilus );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Ostrich );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Octopus );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Pelican );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Puffin );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Pyramid );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Rattle );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Robin );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Rose );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Salmon );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Seal );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Shark );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Sheep );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Snake );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Sonar );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Stump );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Sparrow );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Toaster );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Toucan );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Torus );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Violet );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Vortex );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Vulture );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Wagon );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Whale );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Woodpecker );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Zebra );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Zigzag );
#undef RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED
		for ( int k = 0; k < MAX_PLAYERS + 1; ++ k )
		{
			if ( arrNamesOptions.empty() )
				Error( "Insufficient random names pool!\n" );
			int iRandomChoice = RandomInt( 0, std::ssize( arrNamesOptions ) - 1 );
			m_szNames[k] = arrNamesOptions[ iRandomChoice ];
			arrNamesOptions.erase( arrNamesOptions.begin() + iRandomChoice );
		}
		m_bInitialized = true;
	}
} g_staticPlayerNames;
void EnsureStaticPlayerNamesReinitialized()
{
	g_staticPlayerNames.InitializeNames();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CPlayerResource::GetPlayerName( int iIndex )
{
	if ( cl_names_debug.GetInt() )
		return PLAYER_DEBUG_NAME;

	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
	{
		contract_assert( false );
		return PLAYER_ERROR_NAME;
	}
	
	if ( !IsConnected( iIndex ) )
		return PLAYER_UNCONNECTED_NAME;

	// X360TBD: Network - figure out why the name isn't set
	if ( !m_szName[ iIndex ] || !V_stricmp( m_szName[ iIndex ], PLAYER_UNCONNECTED_NAME ) )
	{
		// If you get a full "reset" uncompressed update from server, then you can have NULLNAME show up in the scoreboard
		UpdatePlayerName( iIndex );
	}

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
		{
			player_info_t sPlayerInfo;
			if ( pParameters->m_uiLockFirstPersonAccountID && engine->GetPlayerInfo( iIndex, &sPlayerInfo ) &&
				( CPlayerId( sPlayerInfo.xuid ).GetAccountID() == pParameters->m_uiLockFirstPersonAccountID ) )
			{
				STATIC_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( s_utf8TheSuspect, "#CSGO_Overwatch_TheSuspect", "The Suspect" );
				return s_utf8TheSuspect;
			}
			else
			{
				return g_staticPlayerNames.GetName( iIndex );
			}
		}
	}

	// This gets updated in ClientThink, so it could be up to 1 second out of date, oh well.
	return m_szName[iIndex] ? m_szName[iIndex] : "";
}

bool CPlayerResource::IsAlive(int iIndex )
{
	return m_bAlive[iIndex];
}

int CPlayerResource::GetTeam(int iIndex )
{
	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
	{
		contract_assert( false );
		return 0;
	}
	else
	{
		return m_iTeam[iIndex];
	}
}

int CPlayerResource::GetPendingTeam(int iIndex )
{
	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
	{
		contract_assert( false );
		return 0;
	}
	else
	{
		return m_iPendingTeam[iIndex];
	}
}

const char * CPlayerResource::GetTeamName(int index)
{
	CTeam *team = GetGlobalTeam( index );

	if ( !team )
		return "Unknown";

	return team->Get_Name();
}

int CPlayerResource::GetCoachingTeam(int index)
{
	if ( index < 1 || index > MAX_PLAYERS )
	{
		contract_assert( false );
		return 0;
	}
	else
	{
		return m_iCoachingTeam[index];
	}
}

int CPlayerResource::GetTeamScore(int index)
{
	CTeam *team = GetGlobalTeam( index );

	if ( !team )
		return 0;

	return team->Get_Score();
}

int CPlayerResource::GetFrags(int index )
{
	return 666;
}

bool CPlayerResource::IsLocalPlayer(int index)
{
	CBasePlayer *pPlayer =	CBasePlayer::GetLocalPlayer();

	if ( !pPlayer )
		return false;

	// HLTV replay will not set m_bLocalPlayer flag, in a sense there's no selected local player, we're observing everyone
	if ( g_HltvReplaySystem.GetHltvReplayDelay() )
		return false;

	return ( index == pPlayer->entindex() );
}

bool CPlayerResource::IsHLTV(int index)
{
	if ( !IsConnected( index ) )
		return false;

	// HLTV replay will not set m_bLocalPlayer flag, in a sense there's no selected local player, we're observing everyone
	if ( g_HltvReplaySystem.GetHltvReplayDelay() && CBasePlayer::GetLocalPlayer()->index == index )
		return true;  // local player is always HLTV in HLTV replay mode, even though the hltv property isn't set because we are in the past and replaying everything as it was (including no hltv flag set)

	player_info_t sPlayerInfo;
	
	if ( engine->GetPlayerInfo( index, &sPlayerInfo ) )
	{
		return sPlayerInfo.ishltv;
	}
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPlayerResource::IsFakePlayer( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return false;

	// Yuck, make sure it's up to date
	player_info_t sPlayerInfo;
	if ( engine->GetPlayerInfo( iIndex, &sPlayerInfo ) )
	{
		return sPlayerInfo.fakeplayer;
	}
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CPlayerResource::GetPing( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iPing[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
/*-----------------------------------------------------------------------------
int	CPlayerResource::GetPacketloss( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iPacketloss[iIndex];
}*/

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CPlayerResource::GetKills( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iKills[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CPlayerResource::GetAssists( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iAssists[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CPlayerResource::GetDeaths( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iDeaths[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CPlayerResource::GetHealth( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iHealth[iIndex];
}

const Color &CPlayerResource::GetTeamColor(int index )
{
	if ( index < 0 || index >= MAX_TEAMS )
	{
		contract_assert( false );
		static Color blah;
		return blah;
	}
	else
	{
		return m_Colors[index];
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPlayerResource::IsConnected( int iIndex )
{
	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
		return false;
	else
		return m_bConnected[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
XUID CPlayerResource::GetXuid( int iIndex )
{
	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
		return INVALID_XUID;
	else
		return m_Xuids[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: Fills the given string with the player xuid.
//-----------------------------------------------------------------------------
void CPlayerResource::FillXuidText( int iIndex, char *buf, int bufSize )
{
	contract_assert( buf && bufSize );
	if ( buf && bufSize )
	{
		XUID xuid = GetXuid( iIndex );

		buf[0] = '\0';
		V_snprintf( buf, bufSize, "%llu", xuid );
	}
}

void CPlayerResource::DeviceLost( void )
{
}

void CPlayerResource::DeviceReset( void *pDevice, void *pPresentParameters, void *pHWnd )
{
	UpdateXuids();
}

#undef PLAYER_DEBUG_NAME
#undef STATIC_PLAYER_NAME_UTF8_ARRAY_LOCALIZED

// See interface.h/.cpp for specifics:  basically this ensures that we actually Sys_UnloadModule the dll and that we don't call Sys_LoadModule 
//  over and over again.
static CDllDemandLoader g_GameUI( "gameui" );

void BonusMapChallengeUpdate( const char *pchFileName, const char *pchMapName, const char *pchChallengeName, int iBest )
{
	CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
	if ( gameUIFactory )
	{
		IGameUI *pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, nullptr );
		if ( pGameUI )
		{
			int piNumMedals[ 3 ] = {0};

			IGameEvent *event = gameeventmanager->CreateEvent( "challenge_map_complete" );
			if ( event )
			{
				event->SetInt( "numbronze", piNumMedals[ 0 ] );
				event->SetInt( "numsilver", piNumMedals[ 1 ] );
				event->SetInt( "numgold", piNumMedals[ 2 ] );
				gameeventmanager->FireEvent( event );
			}
		}	
	}
}

void BonusMapChallengeNames( char *pchFileName, char *pchMapName, char *pchChallengeName )
{
}

void BonusMapChallengeObjectives( int &iBronze, int &iSilver, int &iGold )
{
}

class CPointCamera;

using namespace ::ks::reflect;

IMPLEMENT_REFLECT_CLIENTCLASS( CPointCamera, DT_PointCamera, CPointCamera )

CEntityClassList<CPointCamera> g_PointCameraList;
template<> CPointCamera *CEntityClassList<CPointCamera>::m_pClassList = nullptr;

CPointCamera* GetPointCameraList()
{
	return g_PointCameraList.m_pClassList;
}

CPointCamera::CPointCamera()
{
	m_bActive = false;
	m_bFogEnable = false;

	g_PointCameraList.Insert( this );
}

CPointCamera::~CPointCamera()
{
	g_PointCameraList.Remove( this );
}

bool CPointCamera::ShouldDraw()
{
	return false;
}

float CPointCamera::GetFOV()
{
	return m_FOV;
}

float CPointCamera::GetResolution()
{
	return m_Resolution;
}

bool CPointCamera::IsFogEnabled()
{
	return m_bFogEnable;
}

void CPointCamera::GetFogColor( unsigned char &r, unsigned char &g, unsigned char &b )
{
	r = m_FogColor.r;
	g = m_FogColor.g;
	b = m_FogColor.b;
}

float CPointCamera::GetFogStart()
{
	return m_flFogStart;
}

float CPointCamera::GetFogEnd()
{
	return m_flFogEnd;
}

float CPointCamera::GetFogMaxDensity()
{
	return m_flFogMaxDensity;
}

bool CPointCamera::IsActive()
{
	return m_bActive;
}

void CPointCamera::GetToolRecordingState( KeyValues *msg )
{
	BaseClass::GetToolRecordingState( msg );

	unsigned char r, g, b;
	static MonitorRecordingState_t state;
	state.m_bActive = IsActive() && !IsDormant();
	state.m_flFOV = GetFOV();
	state.m_bFogEnabled = IsFogEnabled();
	state.m_flFogStart = GetFogStart();
	state.m_flFogEnd = GetFogEnd();
	GetFogColor( r, g, b );
	state.m_FogColor.SetColor( r, g, b, 255 );
					  
	msg->SetPtr( "monitor", &state );
}

class CBaseAnimating;

using namespace ::ks::reflect;

// Purpose: Controls the pose parameters of a model

//-----------------------------------------------------------------------------
// CLIENT CLASS
//-----------------------------------------------------------------------------

IMPLEMENT_REFLECT_CLIENTCLASS( CPoseController, DT_PoseController, CPoseController )

void CPoseController::Spawn( void )
{
	SetThink( &CPoseController::ClientThink );
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	m_fCurrentFMod = 0.0f;
	m_PoseTransitionValue.Init( 0.0f, 0.0f, 0.0f );

	BaseClass::Spawn();
}

void CPoseController::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		// Start thinking (Baseclass stops it)
		SetNextClientThink( CLIENT_THINK_ALWAYS );

		m_bOldPoseValueParity = m_bPoseValueParity;
		m_fCurrentPoseValue = m_fPoseValue;

		SetCurrentPose( m_fCurrentPoseValue );
	}

	if ( m_bOldPoseValueParity != m_bPoseValueParity )
	{
		// If the pose value was set directly set the actual pose value
		float fClientPoseValue = m_fCurrentPoseValue + m_PoseTransitionValue.Interp( gpGlobals->curtime );

		if ( fClientPoseValue < 0.0f )
			fClientPoseValue += 1.0f;
		else if ( fClientPoseValue > 1.0f )
			fClientPoseValue -= 1.0f;

		float fInterpForward = fClientPoseValue - m_fPoseValue;

		if ( m_bInterpolationWrap )
		{
			float fInterpBackward = ( fClientPoseValue + ( ( fClientPoseValue < 0.5f ) ? ( 1.0f ) : ( -1.0f ) ) ) - m_fPoseValue;

			m_PoseTransitionValue.Init( ( ( std::fabsf( fInterpForward ) < fabsf( fInterpBackward ) ) ? ( fInterpForward ) : ( fInterpBackward ) ), 0.0f, m_fInterpolationTime );
		}
		else
		{
			m_PoseTransitionValue.Init( fInterpForward, 0.0f, m_fInterpolationTime );
		}

		m_bOldPoseValueParity = m_bPoseValueParity;
		m_fCurrentPoseValue = m_fPoseValue;
	}
}

void CPoseController::ClientThink( void )
{
	UpdateModulation();

	UpdatePoseCycle( m_fCycleFrequency + m_fCurrentFMod );
}

void CPoseController::UpdateModulation( void )
{
	switch ( m_nFModType )
	{
	case POSECONTROLLER_FMODTYPE_NONE:
		{
			// No modulation
			m_fCurrentFMod = 0.0f;
			break;
		}

	case POSECONTROLLER_FMODTYPE_SINE:
		{
			float fCycleTime = m_fFModRate * ( gpGlobals->curtime + m_fFModTimeOffset );

			m_fCurrentFMod = m_fFModAmplitude * std::sinf( fCycleTime * ( 2.0f * std::numbers::pi ) );
			break;
		}

	case POSECONTROLLER_FMODTYPE_SQUARE:
		{
			float fCycleTime = std::fabsf( m_fFModRate * 2.0f * ( gpGlobals->curtime + m_fFModTimeOffset ) );

			// Separate the current time into integer and decimal
			int iIntegerPortion = static_cast<int>( fCycleTime );

			// Find if it's going up or down
			if ( ( iIntegerPortion % 2 ) == 0 )
				m_fCurrentFMod = m_fFModAmplitude;
			else
				m_fCurrentFMod = -m_fFModAmplitude;

			break;
		}

	case POSECONTROLLER_FMODTYPE_TRIANGLE:
		{
			float fCycleTime = std::fabsf( m_fFModRate * 4.0f * ( gpGlobals->curtime + m_fFModTimeOffset ) );

			// Separate the current time into integer and decimal
			int iIntegerPortion = static_cast<int>( fCycleTime );
			float fDecimalPortion = fCycleTime - static_cast<float>( iIntegerPortion );

			// Find if it's going up from 0, down from 1, down from 0, or up from -1
			switch ( iIntegerPortion % 4 )
			{
			case 0:
				m_fCurrentFMod = fDecimalPortion * m_fFModAmplitude;
				break;
			case 1:
				m_fCurrentFMod = ( 1.0f - fDecimalPortion ) * m_fFModAmplitude;
				break;
			case 2:
				m_fCurrentFMod = -fDecimalPortion * m_fFModAmplitude;
				break;
			case 3:
				m_fCurrentFMod = ( -1.0f + fDecimalPortion ) * m_fFModAmplitude;
				break;
			}

			break;
		}

	case POSECONTROLLER_FMODTYPE_SAWTOOTH:
		{
			float fCycleTime = std::fabsf( m_fFModRate * 2.0f * ( gpGlobals->curtime + m_fFModTimeOffset ) );

			// Separate the current time into integer and decimal
			int iIntegerPortion = static_cast<int>( fCycleTime );
			float fDecimalPortion = fCycleTime - static_cast<float>( iIntegerPortion );

			// Find if it's going up from 0 or up from -1
			if ( ( iIntegerPortion % 2 ) == 0 )
				m_fCurrentFMod = fDecimalPortion * m_fFModAmplitude;
			else
				m_fCurrentFMod = ( -1.0f + fDecimalPortion ) * m_fFModAmplitude;

			break;
		}

	case POSECONTROLLER_FMODTYPE_NOISE:
		{
			// Randomly increase or decrease by the rate
			if ( RandomInt( 0, 1 ) == 0 )
				m_fCurrentFMod += m_fFModRate * gpGlobals->frametime;
			else
				m_fCurrentFMod -= m_fFModRate * gpGlobals->frametime;

			m_fCurrentFMod = clamp( m_fCurrentFMod, -m_fFModAmplitude, m_fFModAmplitude );

			break;
		}
	}
}

void CPoseController::UpdatePoseCycle( float fCycleAmount )
{
	m_fCurrentPoseValue += fCycleAmount * gpGlobals->frametime;

	float fNewPoseValue = m_fCurrentPoseValue + m_PoseTransitionValue.Interp( gpGlobals->curtime );

	if ( fNewPoseValue < 0.0f )
		fNewPoseValue += 1.0f;
	else if ( fNewPoseValue > 1.0f )
		fNewPoseValue -= 1.0f;

	SetCurrentPose( fNewPoseValue );
}

void CPoseController::SetCurrentPose( float fCurrentPoseValue )
{
	for ( int iPropNum = 0; iPropNum < MAX_POSE_CONTROLLED_PROPS; ++iPropNum )
	{
		// Control each model's pose parameter
		CBaseAnimating *pProp = dynamic_cast<CBaseAnimating*>( m_hProps[ iPropNum ].Get() );

		if ( pProp )
		{
			float fPoseValueMin;
			float fPoseValueMax;

			// Map to the pose parameter's range
			pProp->GetPoseParameterRange( m_chPoseIndex[ iPropNum ], fPoseValueMin, fPoseValueMax );
			pProp->SetPoseParameter( m_chPoseIndex[ iPropNum ], fPoseValueMin + fCurrentPoseValue * ( fPoseValueMax - fPoseValueMin ) );
		}
	}
}

extern IVEngineClient *engine;
extern CGlobalVarsBase *gpGlobals;

static ConVar ui_posedebug_fade_in_time( side_registry(), "ui_posedebug_fade_in_time", "0.2",
										 FCVAR_CHEAT | FCVAR_DONTRECORD,
										 "Time during which a new pose activity layer is shown in green in +posedebug UI" );
static ConVar ui_posedebug_fade_out_time( side_registry(), "ui_posedebug_fade_out_time", "0.8",
										  FCVAR_CHEAT | FCVAR_DONTRECORD,
										  "Time to keep a no longer active pose activity layer in red until removing it from +posedebug UI" );

//////////////////////////////////////////////////////////////////////////
//
// CPoseDebuggerStub : IPoseDebugger
// empty interface implementation
//
//////////////////////////////////////////////////////////////////////////

class CPoseDebuggerStub : public IPoseDebugger
{
public:
	virtual void StartBlending( IClientNetworkable *pEntity, const CStudioHdr *pStudioHdr ) { }
	virtual void AccumulatePose(
		const CStudioHdr *pStudioHdr,
		CIKContext *pIKContext,
		Vector pos[], 
		Quaternion q[], 
		int sequence, 
		float cycle,
		const float poseParameter[],
		int boneMask,
		float flWeight,
		float flTime
		) { }
};

static CPoseDebuggerStub s_PoseDebuggerStub;
IPoseDebugger *g_pPoseDebugger = &s_PoseDebuggerStub;

//////////////////////////////////////////////////////////////////////////
//
// CPoseDebuggerImpl : IPoseDebugger
// Purpose: Main implementation of the pose debugger
// Declaration
//
//////////////////////////////////////////////////////////////////////////

class ModelPoseDebugInfo
{
public:
	ModelPoseDebugInfo() : m_iEntNum( 0 ), m_iCurrentText( 0 ) { }

public:
	// Entity number
	int m_iEntNum;

	// Currently processed text
	int m_iCurrentText;

	// Info Text Flags
	enum InfoTextFlags
	{
		F_SEEN_THIS_FRAME = 1 << 0,
		F_SEEN_LAST_FRAME = 1 << 1,
	};

	struct InfoText
	{
		InfoText() { std::memset( this, 0, sizeof( *this ) ); }

		// Flags
		uint32 m_uiFlags;

		// Time seen
		float m_flTimeToLive, m_flTimeAlive;

		// Activity/label
		int m_iActivity;
		char m_chActivity[100];
		char m_chLabel[100];

		// Text
		char m_chTextLines[4][256];
		enum
		{
			MAX_TEXT_LINES = 4
		};
	};
	
	std::vector< InfoText > m_arrTxt;

public:
	// Add an info text
	void AddInfoText( InfoText *x, ModelPoseDebugInfo *pOld );

	// Lookup an info text
	InfoText *LookupInfoText( InfoText *x );

	// Print pending info text
	void PrintPendingInfoText( int &rnPosPrint );
};

void ModelPoseDebugInfo::AddInfoText( InfoText *x, ModelPoseDebugInfo *pOld )
{
	if ( x )
	{
		// Try to set the proper flags on the info text
		x->m_uiFlags &= ~F_SEEN_LAST_FRAME;
		x->m_uiFlags |= F_SEEN_THIS_FRAME;
	}

	// If we have smth to compare against
	if ( pOld )
	{
		// Search for the same activity/label in the other model pose debug info
		ModelPoseDebugInfo &o = *pOld;
		int k = o.m_iCurrentText;
		if ( x )
		{
			for ( ; k < std::ssize( o.m_arrTxt ); ++ k )
			{
				InfoText &txt = o.m_arrTxt[k];
				if ( ( txt.m_uiFlags & F_SEEN_THIS_FRAME ) &&
					!stricmp( x->m_chActivity, txt.m_chActivity ) &&
					!stricmp( x->m_chLabel, txt.m_chLabel ) &&
					( x->m_iActivity == txt.m_iActivity ) )
				{
					x->m_flTimeAlive = txt.m_flTimeAlive;
					break;
				}
			}
		}
		else
		{
			k = std::ssize( o.m_arrTxt );
		}

		// Range of finished activities
		int iFinishedRange[2] = { o.m_iCurrentText, k };

		// Check whether this is a new message
		if ( k == std::ssize( o.m_arrTxt ) )
		{
			if ( !x )
			{
				o.m_iCurrentText = k;
			}
			else
			{
				// Don't update the current when insertion happens and don't have finished commands
				iFinishedRange[1] = iFinishedRange[0];
			}
		}
		else
		{
			o.m_iCurrentText = k + 1;
			if ( x )
			{
				x->m_uiFlags |= F_SEEN_LAST_FRAME;
				x->m_flTimeAlive += gpGlobals->frametime;
			}
		}

		// Everything before finished
		for ( int iFinished = iFinishedRange[0]; iFinished < iFinishedRange[1]; ++ iFinished )
		{
			InfoText &txtFinished = o.m_arrTxt[ iFinished ];

			if ( txtFinished.m_uiFlags & F_SEEN_THIS_FRAME )
				txtFinished.m_uiFlags |= F_SEEN_LAST_FRAME;

			txtFinished.m_uiFlags &= ~F_SEEN_THIS_FRAME;

			txtFinished.m_flTimeToLive -= gpGlobals->frametime;
			txtFinished.m_flTimeAlive += gpGlobals->frametime;

			if ( txtFinished.m_flTimeToLive >= 0.0f )
				m_arrTxt.push_back( txtFinished );
		}
	}

	if ( x )
	{
		// Now add it to the array
		x->m_flTimeToLive = ui_posedebug_fade_out_time.GetFloat();
		m_arrTxt.push_back( *x );
	}
}

ModelPoseDebugInfo::InfoText * ModelPoseDebugInfo::LookupInfoText( InfoText *x )
{
	int k = m_iCurrentText;
	if ( x )
	{
		for ( ; k < std::ssize( m_arrTxt ); ++ k )
		{
			InfoText &txt = m_arrTxt[k];
			if ( ( txt.m_uiFlags & F_SEEN_THIS_FRAME ) &&
				!stricmp( x->m_chActivity, txt.m_chActivity ) &&
				!stricmp( x->m_chLabel, txt.m_chLabel ) &&
				( x->m_iActivity == txt.m_iActivity ) )
			{
				return &txt;
			}
		}
	}
	return nullptr;
}

void ModelPoseDebugInfo::PrintPendingInfoText( int &rnPosPrint )
{
	con_nprint_s nxPrn = { 0 };
	nxPrn.time_to_live = -1;
	nxPrn.color[0] = 1.0f, nxPrn.color[1] = 1.0f, nxPrn.color[2] = 1.0f;
	nxPrn.fixed_width_font = true;

	float const flFadeInTime = ui_posedebug_fade_in_time.GetFloat();
	float const flFadeOutTime = ui_posedebug_fade_out_time.GetFloat();

	// Now print all the accumulated spew
	for ( int k = m_iCurrentText; k < std::ssize( m_arrTxt ); ++ k )
	{
		InfoText &prntxt = m_arrTxt[k];

		switch( prntxt.m_uiFlags & ( F_SEEN_LAST_FRAME | F_SEEN_THIS_FRAME ) )
		{
		case ( F_SEEN_LAST_FRAME | F_SEEN_THIS_FRAME ) :
			nxPrn.color[0] = 1.f;
			nxPrn.color[1] = 1.f;
			nxPrn.color[2] = 1.f;
			if ( prntxt.m_flTimeAlive > flFadeInTime )
				break;
			else
				nullptr; // Fall-through to keep showing in green
		case F_SEEN_THIS_FRAME :
			if ( flFadeInTime > 0.f )
			{
				nxPrn.color[0] = 1.f * prntxt.m_flTimeAlive / flFadeInTime;
				nxPrn.color[1] = 1.f;
				nxPrn.color[2] = 1.f * prntxt.m_flTimeAlive / flFadeInTime;
			}
			else
			{
				nxPrn.color[0] = ( prntxt.m_flTimeAlive > 0.0f ) ? 1.f : 0.f;
				nxPrn.color[1] = 1.f;
				nxPrn.color[2] = ( prntxt.m_flTimeAlive > 0.0f ) ? 1.f : 0.f;
			}
			break;
		case F_SEEN_LAST_FRAME :
		case 0:
			if ( flFadeOutTime > 0.f )
				nxPrn.color[0] = 1.f * prntxt.m_flTimeToLive / flFadeOutTime;
			else
				nxPrn.color[0] = ( prntxt.m_flTimeToLive > 0.0f ) ? 1.f : 0.f;
			nxPrn.color[1] = 0.f;
			nxPrn.color[2] = 0.f;
			break;
		}

		nxPrn.index = ( rnPosPrint += 1 );
		engine->Con_NXPrintf( &nxPrn, "%s", prntxt.m_chTextLines[0] );

		for ( int iLine = 1; iLine < ModelPoseDebugInfo::InfoText::MAX_TEXT_LINES; ++ iLine)
		{
			if ( !prntxt.m_chTextLines[iLine][0] )
				break;

			nxPrn.index = ( rnPosPrint += 1 );
			engine->Con_NXPrintf( &nxPrn, "%s", prntxt.m_chTextLines[iLine] );
		}
	}

	m_iCurrentText = std::ssize( m_arrTxt );
}

class CPoseDebuggerImpl : public IPoseDebugger
{
public:
	CPoseDebuggerImpl();
	~CPoseDebuggerImpl();

public:
	void ShowAllModels( bool bShow );
	void ShowModel( int iEntNum, bool bShow );
	bool IsModelShown( int iEntNum ) const;

public:
	virtual void StartBlending( IClientNetworkable *pEntity, const CStudioHdr *pStudioHdr );
	virtual void AccumulatePose(
		const CStudioHdr *pStudioHdr,
		CIKContext *pIKContext,
		Vector pos[], 
		Quaternion q[], 
		int sequence, 
		float cycle,
		const float poseParameter[],
		int boneMask,
		float flWeight,
		float flTime
		);

protected:
	typedef std::map< CStudioHdr const *, ModelPoseDebugInfo > MapModel;
	MapModel m_mapModel, m_mapModelOld;
	int m_nPosPrint;

	std::bitset< MAX_EDICTS > m_uiMaskShowModels;

	CStudioHdr const *m_pLastModel;
};

static CPoseDebuggerImpl s_PoseDebuggerImpl;

//////////////////////////////////////////////////////////////////////////
//
// CPoseDebuggerImpl
// Implementation
//
//////////////////////////////////////////////////////////////////////////

CPoseDebuggerImpl::CPoseDebuggerImpl() :
	m_nPosPrint( 0 ),
	m_pLastModel( nullptr )
{
	m_uiMaskShowModels.set();
}

CPoseDebuggerImpl::~CPoseDebuggerImpl()
{
	// g_pPoseDebugger = &s_PoseDebuggerStub;
}

void CPoseDebuggerImpl::ShowAllModels( bool bShow )
{
	bShow ? m_uiMaskShowModels.set() : m_uiMaskShowModels.reset();
}

void CPoseDebuggerImpl::ShowModel( int iEntNum, bool bShow )
{
	contract_assert( iEntNum < MAX_EDICTS );
	if ( iEntNum < MAX_EDICTS )
		m_uiMaskShowModels.set( iEntNum, bShow );
}

bool CPoseDebuggerImpl::IsModelShown( int iEntNum ) const
{
	contract_assert( iEntNum < MAX_EDICTS );
	if ( iEntNum >= 0 && iEntNum < MAX_EDICTS )
		return m_uiMaskShowModels[iEntNum];
	else
		return false;
}

void CPoseDebuggerImpl::StartBlending( IClientNetworkable *pEntity, const CStudioHdr *pStudioHdr )
{
//	virtualmodel_t const *pVMdl = pStudioHdr->GetVirtualModel();
// 	if ( !pVMdl )
// 		return;

	if ( !ThreadInMainThread() )
	{
		ExecuteOnce( [&]{ "Turn of threading when using pose debugger\n"; } );
		return;
	}

	// If we are starting a new model then finalize the previous one
	if ( pStudioHdr != m_pLastModel && m_pLastModel )
	{
		if ( ModelPoseDebugInfo *pMpi = ks::find_ptr( m_mapModel, m_pLastModel ) )
		{
			ModelPoseDebugInfo &mpi = *pMpi;
			ModelPoseDebugInfo *pMpiOld = ks::find_ptr( m_mapModelOld, m_pLastModel );
			mpi.AddInfoText( nullptr, pMpiOld );
			mpi.PrintPendingInfoText( m_nPosPrint );
		}
	}
	m_pLastModel = pStudioHdr;

	// Go ahead with the new model
	studiohdr_t const *pRMdl = pStudioHdr->GetRenderHdr();
	if ( !pRMdl ||
		 !pRMdl->numincludemodels )
		return;

	// Entity number
	int iEntNum = pEntity->entindex();
	if ( !IsModelShown( iEntNum ) )
		return;

	// Check if we saw the model
	if ( m_mapModel.contains( pStudioHdr ) )
	{
		// Initialize the printing position
		m_nPosPrint = 9;

		// Swap the maps
		m_mapModelOld.clear();
		m_mapModelOld.swap( m_mapModel );

		// Zero out the text on the old map
		for ( auto &[pHdr, mpi] : m_mapModelOld )
		{
			mpi.m_iCurrentText = 0;
		}
	}
	else
	{
		// Next model
		m_nPosPrint += 3;
	}

	ModelPoseDebugInfo mpi;
	mpi.m_iEntNum = iEntNum;
	m_mapModel.emplace( pStudioHdr, mpi );

	con_nprint_s nxPrn = { 0 };
	nxPrn.index = m_nPosPrint;
	nxPrn.time_to_live = -1;
	nxPrn.color[0] = 0.9f, nxPrn.color[1] = 1.0f, nxPrn.color[2] = 0.9f;
	nxPrn.fixed_width_font = false;

	engine->Con_NXPrintf( &nxPrn, "[ %2d  ]    Model: %s", iEntNum, pRMdl->pszName() );
	m_nPosPrint += 3;
}

void CPoseDebuggerImpl::AccumulatePose( const CStudioHdr *pStudioHdr, CIKContext *pIKContext,
									    Vector pos[], Quaternion q[], int sequence, float cycle,
										const float poseParameter[], int boneMask,
										float flWeight, float flTime )
{
//	virtualmodel_t const *pVMdl = pStudioHdr->GetVirtualModel();
// 	if ( !pVMdl )
// 		return;

	if ( !ThreadInMainThread() )
	{
		return;
	}

	studiohdr_t const *pRMdl = pStudioHdr->GetRenderHdr();
	if ( !pRMdl ||
		 !pRMdl->numincludemodels )
		return;

	ModelPoseDebugInfo *pMpi = ks::find_ptr( m_mapModel, pStudioHdr );
	if ( !pMpi )
		return;

	ModelPoseDebugInfo &mpi = *pMpi;
	if ( !IsModelShown( mpi.m_iEntNum ) )
		return;

	ModelPoseDebugInfo *pMpiOld = ks::find_ptr( m_mapModelOld, pStudioHdr );

	//
	// Actual processing
	//

	mstudioseqdesc_t	&seqdesc = ((CStudioHdr *)pStudioHdr)->pSeqdesc( sequence );

	if ( sequence >= pStudioHdr->GetNumSeq() )
	{
		sequence = 0;
		seqdesc = ((CStudioHdr *)pStudioHdr)->pSeqdesc( sequence );
	}

	enum
	{
		widthActivity	= 35,
		widthLayer		= 35,
		widthIks		= 60,
		widthPercent	= 6,
	};

	// Prepare the text
	char chBuffer[256];
	ModelPoseDebugInfo::InfoText txt;
	int numLines = 0;
	
	txt.m_iActivity = seqdesc.activity;
	V_snprintf( txt.m_chActivity, std::size( txt.m_chActivity ), "%s", seqdesc.pszActivityName() );
	V_snprintf( txt.m_chLabel, std::size( txt.m_chLabel ), "%s", seqdesc.pszLabel() );

	if ( !txt.m_chActivity[0] )
	{
		// Try to find the last seen activity and re-use it
		for ( int iLast = std::ssize( mpi.m_arrTxt ); iLast --> 0; )
		{
			ModelPoseDebugInfo::InfoText &lastSeenTxt = mpi.m_arrTxt[iLast];
			if ( lastSeenTxt.m_uiFlags & ModelPoseDebugInfo::F_SEEN_THIS_FRAME &&
				 lastSeenTxt.m_chActivity[0] )
			{
				V_snprintf( txt.m_chActivity, std::size( txt.m_chActivity ), "%s", lastSeenTxt.m_chActivity );
				break;
			}
		}
	}

	// The layer information
	ModelPoseDebugInfo::InfoText *pOldTxt = pMpiOld ? pMpiOld->LookupInfoText( &txt ) : nullptr;
	V_snprintf( txt.m_chTextLines[numLines],
		std::size( txt.m_chTextLines[numLines] ),
		"%-*s  %-*s  %*.2f  %*.1f/%-*d  %*.0f%% ",
		widthActivity,
		seqdesc.pszActivityName(),
		widthLayer,
		seqdesc.pszLabel(),
		7,
		pOldTxt ? pOldTxt->m_flTimeAlive : 0.f,
		5,
		cycle * ( ((CStudioHdr *)pStudioHdr)->pAnimdesc( seqdesc.anim( 0, 0 ) ).numframes - 1 ),
		3,
		((CStudioHdr *)pStudioHdr)->pAnimdesc( seqdesc.anim( 0, 0 ) ).numframes,
		widthPercent,
		flWeight * 100.0f
		);
	++ numLines;

	if ( seqdesc.numiklocks )
	{
		V_snprintf( chBuffer,
			std::size( chBuffer ),
			"iklocks : %-2d : ",
			seqdesc.numiklocks );

		for ( int k = 0; k < seqdesc.numiklocks; ++ k )
		{
			mstudioiklock_t *plock = seqdesc.pIKLock( k );
			mstudioikchain_t *pchain = pStudioHdr->pIKChain( plock->chain );

			V_snprintf( chBuffer + std::strlen( chBuffer ), std::size( chBuffer ) - strlen( chBuffer ), "%s ", pchain->pszName() );
			// plock->flPosWeight;
			// plock->flLocalQWeight;
		}

		V_snprintf( txt.m_chTextLines[numLines],
			std::size( txt.m_chTextLines[numLines] ),
			"%-*s",
			widthIks,
			chBuffer
			);
		++ numLines;
	}

	if ( seqdesc.numikrules )
	{
		V_snprintf( chBuffer, std::size( chBuffer ), "ikrules : %-2d",
			seqdesc.numikrules );

		V_snprintf( txt.m_chTextLines[numLines],
			std::size( txt.m_chTextLines[numLines] ),
			"%-*s",
			widthIks,
			chBuffer
			);
		++ numLines;
	}

	// Now add the accumulated text into the container
	mpi.AddInfoText( &txt, pMpiOld );
	mpi.PrintPendingInfoText( m_nPosPrint );
}

class CPostProcessController;

using namespace ::ks::reflect;

// Purpose: stores map postprocess params

IMPLEMENT_REFLECT_CLIENTCLASS( CPostProcessController, DT_PostProcessController, CPostProcessController )

CPostProcessController* CPostProcessController::ms_pMasterController = nullptr;

//-----------------------------------------------------------------------------
CPostProcessController::CPostProcessController( void )
: 	m_bMaster( false )
{
	if ( ms_pMasterController == nullptr )
	{
		ms_pMasterController = this;
	}
}

//-----------------------------------------------------------------------------
CPostProcessController::~CPostProcessController( void )
{
	if ( ms_pMasterController == this )
	{
		ms_pMasterController = nullptr;
	}
}

void CPostProcessController::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

	if ( m_bMaster )
	{
		ms_pMasterController = this;
	}
}
