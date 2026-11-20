// SPDX-License-Identifier: MIT
// Copyright 2022 Joshua Ashton and Josh Dowell (Slartibarty)


#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/JobSystemWithBarrier.h>
#include <Jolt/Core/FixedSizeFreeList.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/PhysicsScene.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollidePointResult.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/EmptyShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Ragdoll/Ragdoll.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#include <Jolt/Physics/Constraints/ConeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
import std;
import ks.core.platform;
import ks.core.types;
import ks.platform;
import ks.simd;
import ks.tier1.createinterface;
import ks.tier1.keyvalues;
import ks.tier1.map_ops;
#include "compat/branch_overrides.h"
#include "vcollide_parse.h"

#include "vjolt_keyvalues_schema.h"

#include "vjolt_surfaceprops.h"

//-------------------------------------------------------------------------------------------------

JoltPhysicsSurfaceProps JoltPhysicsSurfaceProps::s_PhysicsSurfaceProps;
static InterfaceReg s_Reg_JoltPhysicsSurfaceProps_IPhysicsSurfaceProps( []() -> void * { return static_cast<IPhysicsSurfaceProps *>( &JoltPhysicsSurfaceProps::GetInstance() ); }, VPHYSICS_SURFACEPROPS_INTERFACE_VERSION );;

//-------------------------------------------------------------------------------------------------

static const JoltKVSchemaProp_t kSurfacePropDescs[] =
{
	// Base Property
	{ "Base",								KVSCHEMA_DESC( JoltSurfaceProp, data ),											FillBaseProp },

	// Physics Properties
	{ "Friction",							KVSCHEMA_DESC( JoltSurfaceProp, data.physics.friction ),						FillFloatProp },
	{ "Elasticity",							KVSCHEMA_DESC( JoltSurfaceProp, data.physics.elasticity ),						FillFloatProp },
	{ "Density",							KVSCHEMA_DESC( JoltSurfaceProp, data.physics.density ),							FillFloatProp },
	{ "Thickness",							KVSCHEMA_DESC( JoltSurfaceProp, data.physics.thickness ),						FillFloatProp },
	{ "Dampening",							KVSCHEMA_DESC( JoltSurfaceProp, data.physics.dampening ),						FillFloatProp },

	// Audio Properties
	{ "AudioReflectivity",					KVSCHEMA_DESC( JoltSurfaceProp, data.audio.reflectivity ),						FillFloatProp },
	{ "AudioHardnessFactor",				KVSCHEMA_DESC( JoltSurfaceProp, data.audio.hardnessFactor ),					FillFloatProp },
	{ "AudioRoughnessFactor",				KVSCHEMA_DESC( JoltSurfaceProp, data.audio.roughnessFactor ),					FillFloatProp },

	{ "ScrapeRoughThreshold",				KVSCHEMA_DESC( JoltSurfaceProp, data.audio.roughThreshold ),					FillFloatProp },
	{ "ImpactHardThreshold",				KVSCHEMA_DESC( JoltSurfaceProp, data.audio.hardThreshold ),						FillFloatProp },
	{ "AudioHardMinVelocity",				KVSCHEMA_DESC( JoltSurfaceProp, data.audio.hardVelocityThreshold ),				FillFloatProp },

#ifdef GAME_CSGO_OR_NEWER
	{ "HighPitchOcclusion",					KVSCHEMA_DESC( JoltSurfaceProp, data.audio.highPitchOcclusion ),				FillFloatProp },
	{ "MidPitchOcclusion",					KVSCHEMA_DESC( JoltSurfaceProp, data.audio.midPitchOcclusion ),					FillFloatProp },
	{ "LowPitchOcclusion",					KVSCHEMA_DESC( JoltSurfaceProp, data.audio.lowPitchOcclusion ),					FillFloatProp },
#endif

	// Sound Properties
#ifdef GAME_ASW_OR_NEWER
	{ "StepLeft",							KVSCHEMA_DESC( JoltSurfaceProp, data.sounds.walkStepLeft ),						FillSoundProp },
	{ "StepLeft",							KVSCHEMA_DESC( JoltSurfaceProp, data.sounds.runStepLeft ),						FillSoundProp },
	{ "StepRight",							KVSCHEMA_DESC( JoltSurfaceProp, data.sounds.walkStepRight ),					FillSoundProp },
	{ "StepRight",							KVSCHEMA_DESC( JoltSurfaceProp, data.sounds.runStepRight ),						FillSoundProp },

	{ "WalkLeft",							KVSCHEMA_DESC( JoltSurfaceProp, data.sounds.walkStepLeft ),						FillSoundProp },
	{ "WalkRight",							KVSCHEMA_DESC( JoltSurfaceProp, data.sounds.walkStepRight ),					FillSoundProp },

	{ "RunLeft",							KVSCHEMA_DESC( JoltSurfaceProp, data.sounds.runStepLeft ),						FillSoundProp },
	{ "RunRight",							KVSCHEMA_DESC( JoltSurfaceProp, data.sounds.runStepRight ),						FillSoundProp },
#else
	{ "StepLeft",							KVSCHEMA_DESC( JoltSurfaceProp, data.sounds.stepleft ),							FillSoundProp },
	{ "StepRight",							KVSCHEMA_DESC( JoltSurfaceProp, data.sounds.stepright ),						FillSoundProp },
#endif

	{ "ImpactSoft",							KVSCHEMA_DESC( JoltSurfaceProp, data.sounds.impactSoft ),						FillSoundProp },
	{ "ImpactHard",							KVSCHEMA_DESC( JoltSurfaceProp, data.sounds.impactHard ),						FillSoundProp },

	{ "ScrapeSmooth",						KVSCHEMA_DESC( JoltSurfaceProp, data.sounds.scrapeSmooth ),						FillSoundProp },
	{ "ScrapeRough",						KVSCHEMA_DESC( JoltSurfaceProp, data.sounds.scrapeRough ),						FillSoundProp },

	{ "BulletImpact",						KVSCHEMA_DESC( JoltSurfaceProp, data.sounds.bulletImpact ),						FillSoundProp },
	{ "Rolling",							KVSCHEMA_DESC( JoltSurfaceProp, data.sounds.rolling ),							FillSoundProp },

	{ "Break",								KVSCHEMA_DESC( JoltSurfaceProp, data.sounds.breakSound ),						FillSoundProp },
	{ "Strain",								KVSCHEMA_DESC( JoltSurfaceProp, data.sounds.strainSound ),						FillSoundProp },

	// Game Properties
	{ "MaxSpeedFactor",						KVSCHEMA_DESC( JoltSurfaceProp, data.game.maxSpeedFactor ),						FillFloatProp },
	{ "JumpFactor",							KVSCHEMA_DESC( JoltSurfaceProp, data.game.jumpFactor ),							FillFloatProp },
#ifdef GAME_CSGO_OR_NEWER
	{ "PenetrationModifier",				KVSCHEMA_DESC( JoltSurfaceProp, data.game.penetrationModifier ),				FillFloatProp },
	{ "DamageModifier",						KVSCHEMA_DESC( JoltSurfaceProp, data.game.damageModifier ),						FillFloatProp },
#endif
	{ "GameMaterial",						KVSCHEMA_DESC( JoltSurfaceProp, data.game.material ),							FillGameMaterialProp },
	{ "Climbable",							KVSCHEMA_DESC( JoltSurfaceProp, data.game.climbable ),							FillUnsignedCharProp },
#ifdef GAME_CSGO_OR_NEWER
	{ "HideTargetID",						KVSCHEMA_DESC( JoltSurfaceProp, data.game.hidetargetid ),						FillBoolProp },
	{ "DamageLossPercentPerPenetration",	KVSCHEMA_DESC( JoltSurfaceProp, data.game.damageLossPercentPerPenetration ),	FillFloatProp },
#endif
};

//-------------------------------------------------------------------------------------------------

JoltPhysicsSurfaceProps::JoltPhysicsSurfaceProps()
{
	JoltSurfaceProp prop = {};
	prop.data.physics.friction		= 0.8f;
	prop.data.physics.elasticity	= 0.25f;
	prop.data.physics.density		= 2000.0f;
	prop.data.physics.thickness		= 0.0f;
	prop.data.physics.dampening		= 0.0f;
	m_SurfaceProps.push_back( prop );
	m_SurfacePropNames.emplace_back( "default" );
	m_SurfacePropIndex.emplace( "default", 0 );
}

//-------------------------------------------------------------------------------------------------

int JoltPhysicsSurfaceProps::ParseSurfaceData( const char *pFilename, const char *pTextfile )
{
	KeyValues::AutoDelete kv( SurfacePropsToKeyValues( pTextfile ) );

	for ( KeyValues* pSurface = kv->GetFirstSubKey(); pSurface != nullptr; pSurface = pSurface->GetNextKey() )
	{
		const char *pSurfaceName = pSurface->GetName();

		JoltSurfaceProp values = {};
		// Try to find it if we already have a material with this name,
		// so we can update the values on it, otherwise take the base material.
		const int *pId = ks::find_ptr( m_SurfacePropIndex, pSurfaceName );
		values = m_SurfaceProps[ pId ? *pId : BaseMaterialIdx ];

		ParseJoltKVSchema( pSurface, kSurfacePropDescs, uint( std::size( kSurfacePropDescs ) ), &values );

		// If we don't have this already, add it,
		// otherwise update the values.
		if ( !pId )
		{
			m_SurfacePropIndex.emplace( pSurfaceName, int( m_SurfaceProps.size() ) );
			m_SurfacePropNames.emplace_back( pSurfaceName );
			m_SurfaceProps.push_back( values );
		}
		else
			m_SurfaceProps[ *pId ] = values;
	}

	return int( m_SurfaceProps.size() );
}

int JoltPhysicsSurfaceProps::SurfacePropCount( void ) const
{
	return int ( m_SurfaceProps.size() );
}

//-------------------------------------------------------------------------------------------------

int JoltPhysicsSurfaceProps::GetSurfaceIndex( const char *pSurfacePropName ) const
{
	// TODO(Josh): Something about reserved props for $MATERIAL_INDEX_SHADOW

	if ( const int *pIndex = ks::find_ptr( m_SurfacePropIndex, pSurfacePropName ) )
		return *pIndex;

	return -1;
}

void JoltPhysicsSurfaceProps::GetPhysicsProperties( int surfaceDataIndex, float *density, float *thickness, float *friction, float *elasticity ) const
{
	const int id = surfaceDataIndex >= 0 && surfaceDataIndex < int( m_SurfaceProps.size() )
		? surfaceDataIndex
		: BaseMaterialIdx;

	const JoltSurfaceProp& prop = m_SurfaceProps[ id ];
	if ( density )		*density	= prop.data.physics.density;
	if ( thickness )	*thickness	= prop.data.physics.thickness;
	if ( friction )		*friction	= prop.data.physics.friction;
	if ( elasticity )	*elasticity	= prop.data.physics.elasticity;
}

//-------------------------------------------------------------------------------------------------

surfacedata_t *JoltPhysicsSurfaceProps::GetSurfaceData( int surfaceDataIndex )
{
	const int id = surfaceDataIndex >= 0 && surfaceDataIndex < int( m_SurfaceProps.size() )
		? surfaceDataIndex
		: BaseMaterialIdx;

	JoltSurfaceProp& prop = m_SurfaceProps[ id ];
	return &prop.data;
}

const char *JoltPhysicsSurfaceProps::GetString( unsigned short stringTableIndex ) const
{
	return stringTableIndex < m_SoundStrings.size() ? m_SoundStrings[ stringTableIndex ].c_str() : "";
}

//-------------------------------------------------------------------------------------------------

const char *JoltPhysicsSurfaceProps::GetPropName( int surfaceDataIndex ) const
{
	if ( surfaceDataIndex < 0 || surfaceDataIndex >= int ( m_SurfaceProps.size() ) )
		return nullptr;
	return m_SurfacePropNames[ surfaceDataIndex ].c_str();
}

//-------------------------------------------------------------------------------------------------

void JoltPhysicsSurfaceProps::SetWorldMaterialIndexTable( int *pMapArray, int mapSize )
{
}

//-------------------------------------------------------------------------------------------------

void JoltPhysicsSurfaceProps::GetPhysicsParameters( int surfaceDataIndex, surfacephysicsparams_t *pParamsOut ) const
{
	if ( !pParamsOut )
		return;

	const int id = surfaceDataIndex >= 0 && surfaceDataIndex < int( m_SurfaceProps.size() )
		? surfaceDataIndex
		: BaseMaterialIdx;

	const JoltSurfaceProp& prop = m_SurfaceProps[ id ];
	*pParamsOut = prop.data.physics;
}

//-------------------------------------------------------------------------------------------------

unsigned short JoltPhysicsSurfaceProps::RegisterSound( const char *pName )
{
	if ( !pName )
		return (unsigned short)~0;
	auto [it, bNew] = m_SoundStringIndex.try_emplace( pName, (unsigned short)m_SoundStrings.size() );
	if ( bNew )
		m_SoundStrings.emplace_back( pName );
	return it->second;
}

//-------------------------------------------------------------------------------------------------

KeyValues *JoltPhysicsSurfaceProps::SurfacePropsToKeyValues( const char *pszBuffer )
{
	return HeaderlessKVBufferToKeyValues( pszBuffer, "PhysProps" );
}

//-------------------------------------------------------------------------------------------------

void *JoltPhysicsSurfaceProps::GetIVPMaterial( int nIndex )
{
	return nullptr;
}

int JoltPhysicsSurfaceProps::GetIVPMaterialIndex( const void *pMaterial ) const
{
	return (int)(uintp)( pMaterial );
}

void *JoltPhysicsSurfaceProps::GetIVPManager( void )
{
	return nullptr;
}

int JoltPhysicsSurfaceProps::RemapIVPMaterialIndex( int nIndex ) const
{
	return nIndex;
}

const char *JoltPhysicsSurfaceProps::GetReservedMaterialName( int nMaterialIndex ) const
{
	return "default";
}

//-------------------------------------------------------------------------------------------------
#include "facade/iphysicssurfaceprops.inl"
