// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <time.h>
#include "const.h"
#include "irecipientfilter.h"
#include "shattersurfacetypes.h"
#include "client_class.h"
#include "declare_class.h"
#include "reflect_annotations.h"
#include "reflect_recvtable.h"
#include "cmodel.h"
#include "vphysics_interface.h"
#include "dlight.h"
#include "edict.h"
#include "datacache/iprecachesystem.h"
#include "networkstringtabledefs.h"
#include "te_effect_dispatch.h"
#include "engine/ivmodelinfo.h"
#include "tier2/resourceprecacher.h"
module ks.client;

import ks.core.basetypes;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.exprevaluator;
import ks.tier1.keyvalues;
import ks.tier1.reflect_wirevar;
import :iinput;
import :props;
import :baseentity;
import :sharedvar;
import :util;
import :particlemgr;
import :precache;
import :achievements_and_stats_interface;
import :baseanimatedtextureproxy;
import ks.game.mapentities_shared;
import ks.game.shareddefs;
import ks.core.vprof;
import :noopanimcontroller;
import ks.game.tempentity;
import ks.core.dbg;
import ks.core.color;
import ks.game.gamemovement;
import ks.math.mathlib;
import ks.tier1.intrusive_ptr;
import ks.core.platform;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.map_ops;
import std;
import ks.core.wchartypes;
import ks.tier1.convar;
import ks.tier1.registry;

class CBaseEntity;
class CRecipientFilter;

using namespace ::ks::reflect;

// External definitions
void TE_ArmorRicochet( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* dir );
void TE_BeamEntPoint( IRecipientFilter& filter, float delay,
	int	nStartEntity, const Vector *start, int nEndEntity, const Vector* end,
	int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, float endWidth, int fadeLength, float amplitude, 
	int r, int g, int b, int a, int speed );
void TE_BeamEnts( IRecipientFilter& filter, float delay,
	int	start, int end, int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, float endWidth, int fadeLength, float amplitude, 
	int r, int g, int b, int a, int speed );
void TE_BeamFollow( IRecipientFilter& filter, float delay,
	int iEntIndex, int modelIndex, int haloIndex, float life, float width, float endWidth, 
	float fadeLength,float r, float g, float b, float a );
void TE_BeamPoints( IRecipientFilter& filter, float delay,
	const Vector* start, const Vector* end, int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, float endWidth, int fadeLength, float amplitude, 
	int r, int g, int b, int a, int speed );
void TE_BeamLaser( IRecipientFilter& filter, float delay,
	int	start, int end, int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, float endWidth, int fadeLength, float amplitude, int r, int g, int b, int a, int speed );
void TE_BeamRing( IRecipientFilter& filter, float delay,
	int	start, int end, int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, int spread, float amplitude, int r, int g, int b, int a, int speed, int flags = 0 );
void TE_BeamRingPoint( IRecipientFilter& filter, float delay,
	const Vector& center, float start_radius, float end_radius, int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, int spread, float amplitude, int r, int g, int b, int a, int speed, int flags = 0 );
void TE_BeamSpline( IRecipientFilter& filter, float delay,
	int points, Vector* rgPoints );
void TE_BloodStream( IRecipientFilter& filter, float delay,
	const Vector* org, const Vector* dir, int r, int g, int b, int a, int amount );
void TE_BloodStream( IRecipientFilter& filter, float delay, KeyValues *pKeyValues );
void TE_BloodSprite( IRecipientFilter& filter, float delay,
	const Vector* org, const Vector *dir, int r, int g, int b, int a, int size );
void TE_BloodSprite( IRecipientFilter& filter, float delay, KeyValues *pKeyValues );
void TE_BreakModel( IRecipientFilter& filter, float delay,
	const Vector& pos, const QAngle &angles, const Vector& size, const Vector& vel, 
	int modelindex, int randomization, int count, float time, int flags );
void TE_BreakModel( IRecipientFilter& filter, float delay, KeyValues *pKeyValues );
void TE_BSPDecal( IRecipientFilter& filter, float delay,
	const Vector* pos, int entity, int index );
void TE_ProjectDecal( IRecipientFilter& filter, float delay,
	const Vector* pos, const QAngle *angles, float distance, int index );
void TE_ProjectDecal( IRecipientFilter& filter, float delay, KeyValues *pKeyValues );
void TE_Bubbles( IRecipientFilter& filter, float delay,
	const Vector* mins, const Vector* maxs, float height, int modelindex, int count, float speed );
void TE_BubbleTrail( IRecipientFilter& filter, float delay,
	const Vector* mins, const Vector* maxs, float height, int modelindex, int count, float speed );
void TE_Decal( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* start, int entity, int hitbox, int index );
void TE_Decal( IRecipientFilter& filter, float delay, KeyValues *pKeyValues );
void TE_DynamicLight( IRecipientFilter& filter, float delay,
	const Vector* org, int r, int g, int b, int exponent, float radius, float time, float decay, int nLightIndex = LIGHT_INDEX_TE_DYNAMIC );
void TE_DynamicLight( IRecipientFilter& filter, float delay, KeyValues *pKeyValues );
void TE_Explosion( IRecipientFilter& filter, float delay,
	const Vector& pos, int modelindex, float scale, int framerate, int flags, int radius, int magnitude, 
	const Vector& normal, unsigned char materialType = 'C', bool bShouldAffectRagdolls = true );
void TE_Explosion( IRecipientFilter& filter, float delay, KeyValues *pKeyValues );
void TE_ShatterSurface( IRecipientFilter& filter, float delay,
	const Vector* pos, const QAngle* angle, const Vector* vForce, const Vector* vForcePos, 
	float width, float height, float shardsize, ShatterSurface_t surfacetype,
	int front_r, int front_g, int front_b, int back_r, int back_g, int back_b);
void TE_ShatterSurface( IRecipientFilter& filter, float delay, KeyValues *pKeyValues );
void TE_GlowSprite( IRecipientFilter& filter, float delay,
	const Vector* pos, int modelindex, float life, float size, int brightness );
void TE_GlowSprite( IRecipientFilter& filter, float delay, KeyValues *pKeyValues );
void TE_FootprintDecal( IRecipientFilter& filter, float delay, const Vector* origin, const Vector* right, 
	int entity, int index, unsigned char materialType );
void TE_Fizz( IRecipientFilter& filter, float delay,
	const CBaseEntity *ed, int modelindex, int density, int current );
void TE_KillPlayerAttachments( IRecipientFilter& filter, float delay,
	int player );
void TE_LargeFunnel( IRecipientFilter& filter, float delay,
	const Vector* pos, int modelindex, int reversed );
void TE_MetalSparks( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* dir );
void TE_EnergySplash( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* dir, bool bExplosive );
void TE_PlayerDecal( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* start, const Vector* right, int player, int entity, int hitbox, int nAdditionalDecalFlags );
void TE_ShowLine( IRecipientFilter& filter, float delay,
	const Vector* start, const Vector* end );
void TE_Smoke( IRecipientFilter& filter, float delay,
	const Vector* pos, int modelindex, float scale, int framerate );
void TE_Sparks( IRecipientFilter& filter, float delay,
	const Vector* pos, int nMagnitude, int nTrailLength, const Vector *pDir );
void TE_Sprite( IRecipientFilter& filter, float delay,
	const Vector* pos, int modelindex, float size, int brightness );
void TE_Sprite( IRecipientFilter& filter, float delay, KeyValues *pKeyValues );
void TE_SpriteSpray( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* dir, int modelindex, int speed, float noise, int count );
void TE_SpriteSpray( IRecipientFilter& filter, float delay, KeyValues *pKeyValues );
void TE_WorldDecal( IRecipientFilter& filter, float delay,
	const Vector* pos, int index );
void TE_WorldDecal( IRecipientFilter& filter, float delay, KeyValues *pKeyValues );
void TE_MuzzleFlash( IRecipientFilter& filter, float delay,
	const Vector &start, const QAngle &angles, float scale, int type );
void TE_Dust( IRecipientFilter& filter, float delay,
			 const Vector &pos, const Vector &dir, float size, float speed );
void TE_GaussExplosion( IRecipientFilter& filter, float delayt,
			 const Vector &pos, const Vector &dir, int type );
void TE_PhysicsProp( IRecipientFilter& filter, float delay,
	int modelindex, int skin, const Vector& pos, const QAngle &angles, const Vector& vel, int flags, int effects, color24 renderColor );
void TE_PhysicsProp( IRecipientFilter& filter, float delay, KeyValues *pKeyValues );
void TE_ConcussiveExplosion( IRecipientFilter& filter, float delay, KeyValues *pKeyValues );
void TE_ClientProjectile( IRecipientFilter& filter, float delay,
	 const Vector* vecOrigin, const Vector* vecVelocity, int modelindex, int lifetime, CBaseEntity *pOwner );

class CTempEntsSystem : public ITempEntsSystem
{
private:
	//-----------------------------------------------------------------------------
	// Purpose: Returning true means don't even call TE func
	// Input  : filter - 
	//			*suppress_host - 
	// Output : static bool
	//-----------------------------------------------------------------------------
	bool SuppressTE( IRecipientFilter& filter )
	{
		if ( !CanPredict() )
			return true;

		CRecipientFilter& _filter = (( CRecipientFilter & )filter);

		if ( !_filter.GetRecipientCount() )
		{
			// Suppress it
			return true;
		}

		// There's at least one recipient
		return false;
	}
public:

	virtual void ArmorRicochet( IRecipientFilter& filter, float delay,
		const Vector* pos, const Vector* dir )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_ArmorRicochet( filter, delay, pos, dir );
		}
	}

	virtual void BeamEntPoint( IRecipientFilter& filter, float delay,
		int	nStartEntity, const Vector *pStart, int nEndEntity, const Vector* pEnd,
		int modelindex, int haloindex, int startframe, int framerate,
		float life, float width, float endWidth, int fadeLength, float amplitude, 
		int r, int g, int b, int a, int speed )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_BeamEntPoint( filter, delay, nStartEntity, pStart, nEndEntity, pEnd,
				modelindex, haloindex, startframe, framerate,
				life, width, endWidth, fadeLength, amplitude, r, g, b, a, speed );
		}
	}

	virtual void BeamEnts( IRecipientFilter& filter, float delay,
		int	start, int end, int modelindex, int haloindex, int startframe, int framerate,
		float life, float width, float endWidth, int fadeLength, float amplitude, 
		int r, int g, int b, int a, int speed )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_BeamEnts( filter, delay,
				start, end, modelindex, haloindex, startframe, framerate,
				life, width, endWidth, fadeLength, amplitude, 
				r, g, b, a, speed );
		}
	}

	virtual void BeamFollow( IRecipientFilter& filter, float delay,
		int iEntIndex, int modelIndex, int haloIndex, float life, float width, float endWidth, 
		float fadeLength, float r, float g, float b, float a )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_BeamFollow( filter, delay,
				iEntIndex, modelIndex, haloIndex, life, width, endWidth, fadeLength, 
				r, g, b, a );
		}
	}

	virtual void BeamPoints( IRecipientFilter& filter, float delay,
		const Vector* start, const Vector* end, int modelindex, int haloindex, int startframe, int framerate,
		float life, float width, float endWidth, int fadeLength, float amplitude, 
		int r, int g, int b, int a, int speed )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_BeamPoints( filter, delay,
				start, end, modelindex, haloindex, startframe, framerate,
				life, width, endWidth, fadeLength, amplitude, 
				r, g, b, a, speed );
		}
	}

	virtual void BeamLaser( IRecipientFilter& filter, float delay,
		int	start, int end, int modelindex, int haloindex, int startframe, int framerate,
		float life, float width, float endWidth, int fadeLength, float amplitude, int r, int g, int b, int a, int speed )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_BeamLaser( filter, delay,
				start, end, modelindex, haloindex, startframe, framerate,
				life, width, endWidth, fadeLength, amplitude, r, g, b, a, speed );
		}
	}

	virtual void BeamRing( IRecipientFilter& filter, float delay,
		int	start, int end, int modelindex, int haloindex, int startframe, int framerate,
		float life, float width, int spread, float amplitude, int r, int g, int b, int a, int speed, int flags = 0 )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_BeamRing( filter, delay,
				start, end, modelindex, haloindex, startframe, framerate,
				life, width, spread, amplitude, r, g, b, a, speed, flags );
		}
	}

	virtual void BeamRingPoint( IRecipientFilter& filter, float delay,
		const Vector& center, float start_radius, float end_radius, int modelindex, int haloindex, int startframe, int framerate,
		float life, float width, int spread, float amplitude, int r, int g, int b, int a, int speed, int flags = 0 )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_BeamRingPoint( filter, delay,
				center, start_radius, end_radius, modelindex, haloindex, startframe, framerate,
				life, width, spread, amplitude, r, g, b, a, speed, flags );
		}
	}

	virtual void BeamSpline( IRecipientFilter& filter, float delay,
		int points, Vector* rgPoints )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_BeamSpline( filter, delay, points, rgPoints );
		}
	}

	virtual void BloodStream( IRecipientFilter& filter, float delay,
		const Vector* org, const Vector* dir, int r, int g, int b, int a, int amount )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_BloodStream( filter, delay, org, dir, r, g, b, a, amount );
		}
	}

	virtual void BloodSprite( IRecipientFilter& filter, float delay,
		const Vector* org, const Vector *dir, int r, int g, int b, int a, int size )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_BloodSprite( filter, delay, org, dir, r, g, b, a, size );
		}
	}

	virtual void BreakModel( IRecipientFilter& filter, float delay,
		const Vector& pos, const QAngle &angles, const Vector& size, const Vector& vel, 
		int modelindex, int randomization, int count, float time, int flags )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_BreakModel( filter, delay, pos, angles, size, vel, modelindex, randomization,
				count, time, flags );
		}
	}

	virtual void BSPDecal( IRecipientFilter& filter, float delay,
		const Vector* pos, int entity, int index )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_BSPDecal( filter, delay, pos, entity, index );
		}
	}

	virtual void ProjectDecal( IRecipientFilter& filter, float delay,
		const Vector* pos, const QAngle *angles, float distance, int index )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_ProjectDecal( filter, delay, pos, angles, distance, index );
		}
	}

	virtual void Bubbles( IRecipientFilter& filter, float delay,
		const Vector* mins, const Vector* maxs, float height, int modelindex, int count, float speed )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_Bubbles( filter, delay, mins, maxs, height, modelindex, count, speed );
		}
	}

	virtual void BubbleTrail( IRecipientFilter& filter, float delay,
		const Vector* mins, const Vector* maxs, float flWaterZ, int modelindex, int count, float speed )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_BubbleTrail( filter, delay, mins, maxs, flWaterZ, modelindex, count, speed );
		}
	}

	virtual void Decal( IRecipientFilter& filter, float delay,
		const Vector* pos, const Vector* start, int entity, int hitbox, int index )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_Decal( filter, delay, pos, start, entity, hitbox, index );
		}
	}

	virtual void DynamicLight( IRecipientFilter& filter, float delay,
		const Vector* org, int r, int g, int b, int exponent, float radius, float time, float decay )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_DynamicLight( filter, delay, org, r, g, b, exponent, radius, time, decay );
		}
	}

	virtual void Explosion( IRecipientFilter& filter, float delay,
		const Vector& pos, int modelindex, float scale, int framerate, int flags, int radius, int magnitude, const Vector* pNormal = nullptr, unsigned char materialType = 'C' )
	{
		if ( !SuppressTE( filter ) )
		{
			Vector normal = pNormal ? *pNormal : Vector( 0, 0, 1 );
			TE_Explosion( filter, delay, pos, modelindex, scale, framerate, flags, radius, magnitude, 
				normal, materialType );
		}
	}

	virtual void ShatterSurface( IRecipientFilter& filter, float delay,
		const Vector* pos, const QAngle* angle, const Vector* vForce, const Vector* vForcePos, 
		float width, float height, float shardsize, ShatterSurface_t surfacetype,
		int front_r, int front_g, int front_b, int back_r, int back_g, int back_b)
	{
		if ( !SuppressTE( filter ) )
		{
			TE_ShatterSurface( filter, delay, pos, angle, vForce, vForcePos, 
				width, height, shardsize, surfacetype, front_r, front_g, front_b, back_r, back_g, back_b);
		}
	}
	virtual void GlowSprite( IRecipientFilter& filter, float delay,
		const Vector* pos, int modelindex, float life, float size, int brightness )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_GlowSprite( filter, delay, pos, modelindex, life, size, brightness );
		}
	}
	virtual void FootprintDecal( IRecipientFilter& filter, float delay, const Vector* origin, const Vector* right, 
		int entity, int index, unsigned char materialType )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_FootprintDecal( filter, delay, origin, right, 
				entity, index, materialType );
		}
	}
	virtual void Fizz( IRecipientFilter& filter, float delay,
		const CBaseEntity *ed, int modelindex, int density, int current )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_Fizz( filter, delay,
				ed, modelindex, density, current );
		}
	}
	virtual void KillPlayerAttachments( IRecipientFilter& filter, float delay,
		int player )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_KillPlayerAttachments( filter, delay, player );
		}
	}
	virtual void LargeFunnel( IRecipientFilter& filter, float delay,
		const Vector* pos, int modelindex, int reversed )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_LargeFunnel( filter, delay, pos, modelindex, reversed );
		}
	}
	virtual void MetalSparks( IRecipientFilter& filter, float delay,
		const Vector* pos, const Vector* dir )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_MetalSparks( filter, delay, pos, dir );
		}
	}
	virtual void EnergySplash( IRecipientFilter& filter, float delay,
		const Vector* pos, const Vector* dir, bool bExplosive )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_EnergySplash( filter, delay,
				pos, dir, bExplosive );
		}
	}
	virtual void PlayerDecal( IRecipientFilter& filter, float delay,
		const Vector* pos, const Vector* start, const Vector* right, int player, int entity, int hitbox )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_PlayerDecal( filter, delay,
				pos, start, right, player, entity, hitbox, 0 );
		}
	}
	virtual void ShowLine( IRecipientFilter& filter, float delay,
		const Vector* start, const Vector* end )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_ShowLine( filter, delay,
				start, end );
		}
	}
	virtual void Smoke( IRecipientFilter& filter, float delay,
		const Vector* pos, int modelindex, float scale, int framerate )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_Smoke( filter, delay,
				pos, modelindex, scale, framerate );
		}
	}
	virtual void Sparks( IRecipientFilter& filter, float delay,
		const Vector* pos, int nMagnitude, int nTrailLength, const Vector *pDir )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_Sparks( filter, delay,
				pos, nMagnitude, nTrailLength, pDir );
		}
	}
	virtual void Sprite( IRecipientFilter& filter, float delay,
		const Vector* pos, int modelindex, float size, int brightness )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_Sprite( filter, delay,
				pos, modelindex, size, brightness );
		}
	}
	virtual void SpriteSpray( IRecipientFilter& filter, float delay,
		const Vector* pos, const Vector* dir, int modelindex, int speed, float noise, int count )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_SpriteSpray( filter, delay,
				pos, dir, modelindex, speed, noise, count );
		}
	}
	virtual void WorldDecal( IRecipientFilter& filter, float delay,
		const Vector* pos, int index )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_WorldDecal( filter, delay,
				pos, index );
		}
	}
	virtual void MuzzleFlash( IRecipientFilter& filter, float delay,
		const Vector &start, const QAngle &angles, float scale, int type )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_MuzzleFlash( filter, delay,
				start, angles, scale, type );
		}
	}
	virtual void Dust( IRecipientFilter& filter, float delay,
				 const Vector &pos, const Vector &dir, float size, float speed )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_Dust( filter, delay,
				pos, dir, size, speed );
		}
	}
	virtual void GaussExplosion( IRecipientFilter& filter, float delay,
				const Vector &pos, const Vector &dir, int type )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_GaussExplosion( filter, delay, pos, dir, type );
		}
	}
	virtual void PhysicsProp( IRecipientFilter& filter, float delay, int modelindex, int skin,
		const Vector& pos, const QAngle &angles, const Vector& vel, int flags, int effects, color24 renderColor )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_PhysicsProp( filter, delay, modelindex, skin, pos, angles, vel, flags, effects, renderColor );
		}
	}
	virtual void ClientProjectile( IRecipientFilter& filter, float delay,
		const Vector* vecOrigin, const Vector* vecVelocity, int modelindex, int lifetime, CBaseEntity *pOwner )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_ClientProjectile( filter, delay, vecOrigin, vecVelocity, modelindex, lifetime, pOwner );
		}
	}

	// For playback from external tools
	virtual void TriggerTempEntity( KeyValues *pKeyValues )
	{
		g_pEffects->SuppressEffectsSounds( true );
		SuppressParticleEffects( true );

		CBroadcastRecipientFilter filter;

		TERecordingType_t type = (TERecordingType_t)pKeyValues->GetInt( "te" );
		switch( type )
		{
		case TE_DYNAMIC_LIGHT:
			TE_DynamicLight( filter, 0.0f, pKeyValues );
			break;

		case TE_WORLD_DECAL:
			TE_WorldDecal( filter, 0.0f, pKeyValues );
			break;

		case TE_DISPATCH_EFFECT:
			DispatchEffect( filter, 0.0f, pKeyValues );
			break;

		case TE_MUZZLE_FLASH:
			{
				Vector vecOrigin;
				QAngle angles;
				vecOrigin.x = pKeyValues->GetFloat( "originx" );
				vecOrigin.y = pKeyValues->GetFloat( "originy" );
				vecOrigin.z = pKeyValues->GetFloat( "originz" );
				angles.x = pKeyValues->GetFloat( "anglesx" );
				angles.y = pKeyValues->GetFloat( "anglesy" );
				angles.z = pKeyValues->GetFloat( "anglesz" );
				float flScale = pKeyValues->GetFloat( "scale" );
				int nType = pKeyValues->GetInt( "type" ); 

				TE_MuzzleFlash( filter, 0.0f, vecOrigin, angles, flScale, nType );
			}
			break;

		case TE_ARMOR_RICOCHET:
			{
				Vector vecOrigin, vecDirection;
				vecOrigin.x = pKeyValues->GetFloat( "originx" );
				vecOrigin.y = pKeyValues->GetFloat( "originy" );
				vecOrigin.z = pKeyValues->GetFloat( "originz" );
				vecDirection.x = pKeyValues->GetFloat( "directionx" );
				vecDirection.y = pKeyValues->GetFloat( "directiony" );
				vecDirection.z = pKeyValues->GetFloat( "directionz" );

				TE_ArmorRicochet( filter, 0.0f, &vecOrigin, &vecDirection );
			}
			break;

		case TE_METAL_SPARKS:
			{
				Vector vecOrigin, vecDirection;
				vecOrigin.x = pKeyValues->GetFloat( "originx" );
				vecOrigin.y = pKeyValues->GetFloat( "originy" );
				vecOrigin.z = pKeyValues->GetFloat( "originz" );
				vecDirection.x = pKeyValues->GetFloat( "directionx" );
				vecDirection.y = pKeyValues->GetFloat( "directiony" );
				vecDirection.z = pKeyValues->GetFloat( "directionz" );

				TE_MetalSparks( filter, 0.0f, &vecOrigin, &vecDirection );
			}
			break;

		case TE_SMOKE:
			{
				Vector vecOrigin, vecDirection;
				vecOrigin.x = pKeyValues->GetFloat( "originx" );
				vecOrigin.y = pKeyValues->GetFloat( "originy" );
				vecOrigin.z = pKeyValues->GetFloat( "originz" );
				float flScale = pKeyValues->GetFloat( "scale" );
				int nFrameRate = pKeyValues->GetInt( "framerate" );
				TE_Smoke( filter, 0.0f, &vecOrigin, 0, flScale, nFrameRate );
			}
			break;

		case TE_SPARKS:
			{
				Vector vecOrigin, vecDirection;
				vecOrigin.x = pKeyValues->GetFloat( "originx" );
				vecOrigin.y = pKeyValues->GetFloat( "originy" );
				vecOrigin.z = pKeyValues->GetFloat( "originz" );
				vecDirection.x = pKeyValues->GetFloat( "directionx" );
				vecDirection.y = pKeyValues->GetFloat( "directiony" );
				vecDirection.z = pKeyValues->GetFloat( "directionz" );
				int nMagnitude = pKeyValues->GetInt( "magnitude" );
				int nTrailLength = pKeyValues->GetInt( "traillength" );
				TE_Sparks( filter, 0.0f, &vecOrigin, nMagnitude, nTrailLength, &vecDirection );
			}
			break;

		case TE_BLOOD_STREAM:
			TE_BloodStream( filter, 0.0f, pKeyValues );
			break;

		case TE_BLOOD_SPRITE:
			TE_BloodSprite( filter, 0.0f, pKeyValues );
			break;

		case TE_BREAK_MODEL:
			TE_BreakModel( filter, 0.0f, pKeyValues );
			break;

		case TE_GLOW_SPRITE:
			TE_GlowSprite( filter, 0.0f, pKeyValues );
			break;

		case TE_PHYSICS_PROP:
			TE_PhysicsProp( filter, 0.0f, pKeyValues );
			break;

		case TE_SPRITE_SINGLE:
			TE_Sprite( filter, 0.0f, pKeyValues );
			break;

		case TE_SPRITE_SPRAY:
			TE_SpriteSpray( filter, 0.0f, pKeyValues );
			break;

		case TE_SHATTER_SURFACE:
			TE_ShatterSurface( filter, 0.0f, pKeyValues );
			break;

		case TE_DECAL:
			TE_Decal( filter, 0.0f, pKeyValues );
			break;

		case TE_PROJECT_DECAL:
			TE_ProjectDecal( filter, 0.0f, pKeyValues );
			break;

		case TE_EXPLOSION:
			TE_Explosion( filter, 0.0f, pKeyValues );
			break;

		}

		SuppressParticleEffects( false );
		g_pEffects->SuppressEffectsSounds( false );
	}
};

static CTempEntsSystem g_TESystem;
// Expose to rest of engine
ITempEntsSystem *te = &g_TESystem;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Armor Ricochet TE
//-----------------------------------------------------------------------------
class CTEMetalSparks : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEMetalSparks", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLIENTCLASS();

					CTEMetalSparks( void );
	virtual			~CTEMetalSparks( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

	virtual void	Precache( void );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecPos;
	[[= ks::reflect::Net{} ]] Vector			m_vecDir;

	const struct model_t *m_pModel;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEMetalSparks::CTEMetalSparks( void )
{
	m_vecPos.Init();
	m_vecDir.Init();
	m_pModel = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEMetalSparks::~CTEMetalSparks( void )
{
}

void CTEMetalSparks::Precache( void )
{
	//m_pModel = engine->LoadModel( "sprites/richo1.vmt" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEMetalSparks::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEMetalSparks::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	g_pEffects->MetalSparks( m_vecPos, m_vecDir );
}

void TE_MetalSparks( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* dir )
{
	g_pEffects->MetalSparks( *pos, *dir );
}

//-----------------------------------------------------------------------------
// Purpose: Armor Ricochet TE
//-----------------------------------------------------------------------------
class CTEArmorRicochet : public CTEMetalSparks
{
	[[= ks::reflect::NetTable{ .name = "DT_TEArmorRicochet" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CTEArmorRicochet, CTEMetalSparks );
public:
	DECLARE_CLIENTCLASS();
	virtual void	PostDataUpdate( DataUpdateType_t updateType );
};

//-----------------------------------------------------------------------------
// Purpose: Client side version of API
//-----------------------------------------------------------------------------
void TE_ArmorRicochet( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* dir )
{
	g_pEffects->Ricochet( *pos, *dir );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEArmorRicochet::PostDataUpdate( DataUpdateType_t updateType )
{
	g_pEffects->Ricochet( m_vecPos, m_vecDir );
}

// Expose the TE to the engine.
IMPLEMENT_CLIENTCLASS_EVENT( CTEMetalSparks, DT_TEMetalSparks, CTEMetalSparks );

IMPLEMENT_REFLECT_TABLE( CTEMetalSparks, DT_TEMetalSparks );

IMPLEMENT_CLIENTCLASS_EVENT( CTEArmorRicochet, DT_TEArmorRicochet, CTEArmorRicochet );
IMPLEMENT_REFLECT_TABLE( CTEArmorRicochet, DT_TEArmorRicochet );

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Contains common variables for all beam TEs
//-----------------------------------------------------------------------------
CTEBaseBeam::CTEBaseBeam( void )
{
	m_nModelIndex	= 0;
	m_nHaloIndex	= 0;
	m_nStartFrame	= 0;
	m_nFrameRate	= 0;
	m_fLife			= 0.0;
	m_fWidth		= 0;
	m_fEndWidth		= 0;
	m_nFadeLength	= 0;
	m_fAmplitude	= 0;
	r = g = b = a = 0;
	m_nSpeed		= 0;
	m_nFlags		= 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBaseBeam::~CTEBaseBeam( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEBaseBeam::PreDataUpdate( DataUpdateType_t updateType ) 
{ 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEBaseBeam::PostDataUpdate( DataUpdateType_t updateType )
{
	contract_assert( 0 );
}

IMPLEMENT_CLIENTCLASS(CTEBaseBeam, DT_BaseBeam, CTEBaseBeam);

IMPLEMENT_REFLECT_TABLE( CTEBaseBeam, DT_BaseBeam );

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: BeamEntPoint TE
//-----------------------------------------------------------------------------
class CTEBeamEntPoint : public CTEBaseBeam
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBeamEntPoint" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBeamEntPoint, CTEBaseBeam );
	DECLARE_CLIENTCLASS();

					CTEBeamEntPoint( void );
	virtual			~CTEBeamEntPoint( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] int				m_nStartEntity;
	[[= ks::reflect::Net{} ]] int				m_nEndEntity;
	[[= ks::reflect::Net{} ]] Vector			m_vecStartPoint;
	[[= ks::reflect::Net{} ]] Vector			m_vecEndPoint;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamEntPoint::CTEBeamEntPoint( void )
{
	m_nStartEntity	= 0;
	m_vecEndPoint.Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamEntPoint::~CTEBeamEntPoint( void )
{
}

void TE_BeamEntPoint( IRecipientFilter& filter, float delay,
	int	nStartEntity, const Vector *pStart, int nEndEntity, const Vector* pEnd, 
	int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, float endWidth, int fadeLength, float amplitude, 
	int r, int g, int b, int a, int speed )
{
	beams->CreateBeamEntPoint( nStartEntity, pStart, nEndEntity, pEnd, 
		modelindex, haloindex, 0.0f, life,  width, endWidth, fadeLength, amplitude,
		a, 0.1 * (float)speed, startframe, 0.1f * (float)framerate, r, g, b );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEBeamEntPoint::PostDataUpdate( DataUpdateType_t updateType )
{
	beams->CreateBeamEntPoint( m_nStartEntity, &m_vecStartPoint, m_nEndEntity, &m_vecEndPoint, 
		m_nModelIndex, m_nHaloIndex, 0.0f,
		m_fLife,  m_fWidth, m_fEndWidth, m_nFadeLength, m_fAmplitude, a, 0.1 * m_nSpeed, 
		m_nStartFrame, 0.1 * m_nFrameRate, r, g, b );
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEBeamEntPoint, DT_TEBeamEntPoint, CTEBeamEntPoint )

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: BeamEnts TE
//-----------------------------------------------------------------------------
class CTEBeamEnts : public CTEBaseBeam
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBeamEnts" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBeamEnts, CTEBaseBeam );
	DECLARE_CLIENTCLASS();

					CTEBeamEnts( void );
	virtual			~CTEBeamEnts( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] int				m_nStartEntity;
	[[= ks::reflect::Net{} ]] int				m_nEndEntity;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamEnts::CTEBeamEnts( void )
{
	m_nStartEntity	= 0;
	m_nEndEntity	= 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamEnts::~CTEBeamEnts( void )
{
}

void TE_BeamEnts( IRecipientFilter& filter, float delay,
	int	start, int end, int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, float endWidth, int fadeLength, float amplitude, 
	int r, int g, int b, int a, int speed )
{
	beams->CreateBeamEnts( start, end, modelindex, haloindex, 0.0f, 
		life, width, endWidth, fadeLength, amplitude, a, 0.1 * (float)speed, 
		startframe, 0.1 * (float)framerate, r, g, b );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEBeamEnts::PostDataUpdate( DataUpdateType_t updateType )
{
	beams->CreateBeamEnts( m_nStartEntity, m_nEndEntity, m_nModelIndex, m_nHaloIndex, 0.0f, 
		m_fLife, m_fWidth,  m_fEndWidth, m_nFadeLength, m_fAmplitude, a, 0.1 * m_nSpeed, 
		m_nStartFrame, 0.1 * m_nFrameRate, r, g, b );
}

// Expose the TE to the engine.
IMPLEMENT_CLIENTCLASS_EVENT( CTEBeamEnts, DT_TEBeamEnts, CTEBeamEnts );

IMPLEMENT_REFLECT_TABLE( CTEBeamEnts, DT_TEBeamEnts );

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CTEBeamFollow : public CTEBaseBeam
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBeamFollow" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBeamFollow, CTEBaseBeam );
	DECLARE_CLIENTCLASS();

					CTEBeamFollow( void );
	virtual			~CTEBeamFollow( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );
	
public:

	[[= ks::reflect::Net{} ]] int m_iEntIndex;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamFollow::CTEBeamFollow( void )
{
	m_iEntIndex = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamFollow::~CTEBeamFollow( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEBeamFollow::PostDataUpdate( DataUpdateType_t updateType )
{
	beams->CreateBeamFollow( m_iEntIndex, m_nModelIndex, m_nHaloIndex, 0, m_fLife,
		m_fWidth, m_fEndWidth, m_nFadeLength, r, g, b, a );
}

// Expose the TE to the engine.
IMPLEMENT_CLIENTCLASS_EVENT( CTEBeamFollow, DT_TEBeamFollow, CTEBeamFollow );

IMPLEMENT_REFLECT_TABLE( CTEBeamFollow, DT_TEBeamFollow );

void TE_BeamFollow( IRecipientFilter& filter, float delay,
	int iEntIndex, int modelIndex, int haloIndex, float life, float width, float endWidth, 
	float fadeLength,float r, float g, float b, float a )
{
	beams->CreateBeamFollow( iEntIndex, modelIndex, haloIndex, 0, life,
		width, endWidth, fadeLength, r, g, b, a );
}

using namespace ::ks::reflect;

// Purpose: Beam that's used for the sniper's laser sight

//-----------------------------------------------------------------------------
// Purpose: Beam used for Laser sights. Fades out when it's perpendicular to the viewpoint.
//-----------------------------------------------------------------------------
class CTEBeamLaser : public CTEBaseBeam
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBeamLaser" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CTEBeamLaser, CTEBaseBeam );
public:
	DECLARE_CLIENTCLASS();

	CTEBeamLaser( void );
	virtual			~CTEBeamLaser( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] int				m_nStartEntity;
	[[= ks::reflect::Net{} ]] int				m_nEndEntity;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamLaser::CTEBeamLaser( void )
{
	m_nStartEntity	= 0;
	m_nEndEntity	= 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamLaser::~CTEBeamLaser( void )
{
}

void TE_BeamLaser( IRecipientFilter& filter, float delay,
	int	start, int end, int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, float endWidth, int fadeLength, float amplitude, int r, int g, int b, int a, int speed )
{
	beams->CreateBeamEnts( start, end, modelindex, haloindex, 0.0f, 
		life, width, endWidth, fadeLength, amplitude, a, 0.1 * speed, 
		startframe, 0.1 * framerate, r, g, b, TE_BEAMLASER );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEBeamLaser::PostDataUpdate( DataUpdateType_t updateType )
{
	beams->CreateBeamEnts( m_nStartEntity, m_nEndEntity, m_nModelIndex, m_nHaloIndex, 0.0f, 
		m_fLife, m_fWidth,  m_fEndWidth, m_nFadeLength, m_fAmplitude, a, 0.1 * m_nSpeed, 
		m_nStartFrame, 0.1 * m_nFrameRate, r, g, b, TE_BEAMLASER );
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEBeamLaser, DT_TEBeamLaser, CTEBeamLaser )

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: BeamPoints TE
//-----------------------------------------------------------------------------
class CTEBeamPoints : public CTEBaseBeam
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBeamPoints" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBeamPoints, CTEBaseBeam );
	DECLARE_CLIENTCLASS();

					CTEBeamPoints( void );
	virtual			~CTEBeamPoints( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecStartPoint;
	[[= ks::reflect::Net{} ]] Vector			m_vecEndPoint;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamPoints::CTEBeamPoints( void )
{
	m_vecStartPoint.Init();
	m_vecEndPoint.Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamPoints::~CTEBeamPoints( void )
{
}

void TE_BeamPoints( IRecipientFilter& filter, float delay,
	const Vector* start, const Vector* end, int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, float endWidth, int fadeLength, float amplitude, 
	int r, int g, int b, int a, int speed )
{
	beams->CreateBeamPoints( (Vector&)*start, (Vector&)*end, modelindex, haloindex, 0.0f, 
		life, width, endWidth, fadeLength, amplitude, a, 0.1 * speed, 
		startframe, 0.1 * (float)framerate, r, g, b );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEBeamPoints::PostDataUpdate( DataUpdateType_t updateType )
{
	beams->CreateBeamPoints( m_vecStartPoint, m_vecEndPoint, m_nModelIndex, m_nHaloIndex, 0.0f, 
		m_fLife, m_fWidth,  m_fEndWidth, m_nFadeLength, m_fAmplitude, a, 0.1 * m_nSpeed, 
		m_nStartFrame, 0.1 * m_nFrameRate, r, g, b );
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEBeamPoints, DT_TEBeamPoints, CTEBeamPoints )

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: BeamRing TE
//-----------------------------------------------------------------------------
class CTEBeamRing : public CTEBaseBeam
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBeamRing" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBeamRing, CTEBaseBeam );
	DECLARE_CLIENTCLASS();

					CTEBeamRing( void );
	virtual			~CTEBeamRing( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] int				m_nStartEntity;
	[[= ks::reflect::Net{} ]] int				m_nEndEntity;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamRing::CTEBeamRing( void )
{
	m_nStartEntity	= 0;
	m_nEndEntity	= 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamRing::~CTEBeamRing( void )
{
}

void TE_BeamRing( IRecipientFilter& filter, float delay,
	int	start, int end, int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, int spread, float amplitude, int r, int g, int b, int a, int speed, int flags )
{
	beams->CreateBeamRing( start, end, modelindex, haloindex, 0.0f,
		life, width, 0.1 * spread, 0.0f, amplitude, a, 0.1 * speed, 
		startframe, 0.1 * framerate, r, g, b, flags );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEBeamRing::PostDataUpdate( DataUpdateType_t updateType )
{
	beams->CreateBeamRing( m_nStartEntity, m_nEndEntity, m_nModelIndex, m_nHaloIndex, 0.0f,
		m_fLife, m_fWidth, m_fEndWidth, m_nFadeLength, m_fAmplitude, a, 0.1 * m_nSpeed, 
		m_nStartFrame, 0.1 * m_nFrameRate, r, g, b, m_nFlags );
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEBeamRing, DT_TEBeamRing, CTEBeamRing )

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: BeamRingPoint TE
//-----------------------------------------------------------------------------
class CTEBeamRingPoint : public CTEBaseBeam
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBeamRingPoint" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBeamRingPoint, CTEBaseBeam );
	DECLARE_CLIENTCLASS();

					CTEBeamRingPoint( void );
	virtual			~CTEBeamRingPoint( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecCenter;
	[[= ks::reflect::Net{} ]] float			m_flStartRadius;
	[[= ks::reflect::Net{} ]] float			m_flEndRadius;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamRingPoint::CTEBeamRingPoint( void )
{
	m_vecCenter.Init();
	m_flStartRadius = 0.0f;
	m_flEndRadius = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamRingPoint::~CTEBeamRingPoint( void )
{
}

void TE_BeamRingPoint( IRecipientFilter& filter, float delay,
	const Vector& center, float start_radius, float end_radius, int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, int spread, float amplitude, int r, int g, int b, int a, int speed, int flags )
{
	beams->CreateBeamRingPoint( center, start_radius, end_radius, modelindex, haloindex, 0.0f,
		life, width, 0.1 * spread, 0.0f, amplitude, a, 0.1 * speed, 
		startframe, 0.1 * framerate, r, g, b, flags );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEBeamRingPoint::PostDataUpdate( DataUpdateType_t updateType )
{
	beams->CreateBeamRingPoint( m_vecCenter, m_flStartRadius, m_flEndRadius, m_nModelIndex, m_nHaloIndex, 0.0f,
		m_fLife, m_fWidth, m_fEndWidth, m_nFadeLength, m_fAmplitude, a, 0.1 * m_nSpeed, 
		m_nStartFrame, 0.1 * m_nFrameRate, r, g, b, m_nFlags );
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEBeamRingPoint, DT_TEBeamRingPoint, CTEBeamRingPoint )

using namespace ::ks::reflect;

static constexpr auto MAX_SPLINE_POINTS = 16;
//-----------------------------------------------------------------------------
// Purpose: BeamSpline TE
//-----------------------------------------------------------------------------
class CTEBeamSpline : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBeamSpline", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBeamSpline, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTEBeamSpline( void );
	virtual			~CTEBeamSpline( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{ .varlen = true } ]] Vector			m_vecPoints[ MAX_SPLINE_POINTS ];
	[[= ks::reflect::Net{} ]] int				m_nPoints;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamSpline::CTEBeamSpline( void )
{
	int i;
	for ( i = 0; i < MAX_SPLINE_POINTS; i++ )
	{
		m_vecPoints[ i ].Init();
	}
	m_nPoints = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamSpline::~CTEBeamSpline( void )
{
}

void TE_BeamSpline( IRecipientFilter& filter, float delay,
	int points, Vector* rgPoints )
{
	DevMsg( 1, "Beam spline with %i points invoked\n", points );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEBeamSpline::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEBeamSpline::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	DevMsg( 1, "Beam spline with %i points received\n", m_nPoints );
}

// Expose the TE to the engine.
IMPLEMENT_CLIENTCLASS_EVENT( CTEBeamSpline, DT_TEBeamSpline, CTEBeamSpline );

IMPLEMENT_REFLECT_TABLE( CTEBeamSpline, DT_TEBeamSpline );

using namespace ::ks::reflect;

extern int		g_sModelIndexBloodDrop;	
extern int		g_sModelIndexBloodSpray;

//-----------------------------------------------------------------------------
// Purpose: Blood sprite
//-----------------------------------------------------------------------------
class CTEBloodSprite : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBloodSprite", .base = false } ]]
	[[= ks::reflect::From<"r", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"g", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"b", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"a", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBloodSprite, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTEBloodSprite( void );
	virtual			~CTEBloodSprite( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{} ]] Vector			m_vecDirection;
	int				r, g, b, a;
	[[= ks::reflect::Net{} ]] int				m_nDropModel;
	[[= ks::reflect::Net{} ]] int				m_nSprayModel;
	[[= ks::reflect::Net{} ]] int				m_nSize;
};

// Expose it to the engine.
IMPLEMENT_CLIENTCLASS_EVENT( CTEBloodSprite, DT_TEBloodSprite, CTEBloodSprite );

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_TABLE( CTEBloodSprite, DT_TEBloodSprite );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBloodSprite::CTEBloodSprite( void )
{
	m_vecOrigin.Init();
	m_vecDirection.Init();

	r = g = b = a = 0;
	m_nSize = 0;
	m_nSprayModel = 0;
	m_nDropModel = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBloodSprite::~CTEBloodSprite( void )
{
}

void TE_BloodSprite( IRecipientFilter& filter, float delay,
	const Vector* org, const Vector *dir, int r, int g, int b, int a, int size )
{
	Vector	offset = *org + ( (*dir) * 4.0f );

	tempents->BloodSprite( offset, r, g, b, a, g_sModelIndexBloodSpray, g_sModelIndexBloodDrop, size );	
	FX_Blood( offset, (Vector &)*dir, r, g, b, a );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEBloodSprite::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEBloodSprite::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	Vector	offset = m_vecOrigin + ( m_vecDirection * 4.0f );

	tempents->BloodSprite( offset, r, g, b, a, m_nSprayModel, m_nDropModel, m_nSize );	
	FX_Blood( offset, m_vecDirection, r, g, b, a );
}

void TE_BloodSprite( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	Vector vecOrigin, vecDirection;
	vecOrigin.x = pKeyValues->GetFloat( "originx" );
	vecOrigin.y = pKeyValues->GetFloat( "originy" );
	vecOrigin.z = pKeyValues->GetFloat( "originz" );
	vecDirection.x = pKeyValues->GetFloat( "directionx" );
	vecDirection.y = pKeyValues->GetFloat( "directiony" );
	vecDirection.z = pKeyValues->GetFloat( "directionz" );
	Color c = pKeyValues->GetColor( "color" );
//	const char *pSprayModelName = pKeyValues->GetString( "spraymodel" );
//	const char *pDropModelName = pKeyValues->GetString( "dropmodel" );
	int nSize = pKeyValues->GetInt( "size" );

	TE_BloodSprite( filter, 0.0f, &vecOrigin, &vecDirection, c.r(), c.g(), c.b(), c.a(), nSize );
}

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Blood Stream TE
//-----------------------------------------------------------------------------
class CTEBloodStream : public CTEParticleSystem
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBloodStream" } ]]
	[[= ks::reflect::From<"r", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"g", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"b", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"a", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBloodStream, CTEParticleSystem );
	DECLARE_CLIENTCLASS();

					CTEBloodStream( void );
	virtual			~CTEBloodStream( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecDirection;
	int				r, g, b, a;
	[[= ks::reflect::Net{} ]] int				m_nAmount;
};

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEBloodStream, DT_TEBloodStream, CTEBloodStream )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBloodStream::CTEBloodStream( void )
{
	m_vecOrigin.Init();
	m_vecDirection.Init();
	r = g = b = a = 0;
	m_nAmount = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBloodStream::~CTEBloodStream( void )
{
}

void TE_BloodStream( IRecipientFilter& filter, float delay,
	const Vector* org, const Vector* direction, int r, int g, int b, int a, int amount )
{

	ks::intrusive_ptr<CTEParticleRenderer> pRen = CTEParticleRenderer::Create( "TEBloodStream", *org );
	if( !pRen )
		return;

	// Add our particles.
	Vector		dirCopy;
	float		arc = 0.05;
	int			count, count2;
	float		num;
	int			speedCopy = amount;
	
	Vector dir;
	VectorCopy( *direction, dir );
	VectorNormalize( dir );
	
	for (count=0 ; count<100 ; count++)
	{
		StandardParticle_t *p = pRen->AddParticle();
		if(p)
		{
			p->SetColor(r * random_valve->RandomFloat(0.7, 1.0), g, b);
			p->SetAlpha(a);
			p->m_Pos = *org;
			pRen->SetParticleLifetime(p, 2);
			pRen->SetParticleType(p, pt_vox_grav);
			
			VectorCopy (dir, dirCopy);
			
			dirCopy[2] -= arc;
			arc -= 0.005;
			
			VectorScale (dirCopy, speedCopy, p->m_Velocity);
			
			speedCopy -= 0.00001;// so last few will drip
		}
	}
	
	// now a few rogue voxels
	arc = 0.075;
	for (count = 0 ; count < (amount/5); count ++)
	{
		StandardParticle_t *p = pRen->AddParticle();
		if(p)
		{
			pRen->SetParticleLifetime(p, 3);
			p->SetColor(r * random_valve->RandomFloat(0.7, 1.0), g, b);
			p->SetAlpha(a);
			p->m_Pos = *org;
			pRen->SetParticleType(p, pt_vox_slowgrav);
			
			VectorCopy (dir, dirCopy);
			
			dirCopy[2] -= arc;
			arc -= 0.005;
			
			num = random_valve->RandomFloat(0,1);
			speedCopy = amount * num;
			
			num *= 1.7;
			
			VectorScale (dirCopy, num, dirCopy);// randomize a bit
			p->m_Velocity = dirCopy * speedCopy;
			
			// add a few extra voxels directly adjacent to this one to give a 
			// 'chunkier' appearance.
			for (count2 = 0; count2 < 2; count2++)
			{
				StandardParticle_t *p = pRen->AddParticle();
				if(p)
				{
					pRen->SetParticleLifetime(p, 3);
					p->SetColor(random_valve->RandomFloat(0.7, 1.0), g, b);
					p->SetAlpha(a);
					p->m_Pos.Init(
						(*org)[0] + random_valve->RandomFloat(-1,1),
						(*org)[1] + random_valve->RandomFloat(-1,1),
						(*org)[2] + random_valve->RandomFloat(-1,1));
					
					pRen->SetParticleType(p, pt_vox_slowgrav);
					
					VectorCopy (dir, dirCopy);
					
					dirCopy[2] -= arc;
					
					VectorScale (dirCopy, num, dirCopy);// randomize a bit
					
					p->m_Velocity = dirCopy * speedCopy;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEBloodStream::PostDataUpdate( DataUpdateType_t updateType )
{
	CBroadcastRecipientFilter filter;
	TE_BloodStream( filter, 0.0f, &m_vecOrigin, &m_vecDirection, r, g, b, a, m_nAmount );
}

void TE_BloodStream( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	Vector vecOrigin, vecDirection;
	vecOrigin.x = pKeyValues->GetFloat( "originx" );
	vecOrigin.y = pKeyValues->GetFloat( "originy" );
	vecOrigin.z = pKeyValues->GetFloat( "originz" );
	vecDirection.x = pKeyValues->GetFloat( "directionx" );
	vecDirection.y = pKeyValues->GetFloat( "directiony" );
	vecDirection.z = pKeyValues->GetFloat( "directionz" );
	Color c = pKeyValues->GetColor( "color" );
	int nAmount = pKeyValues->GetInt( "amount" );
	TE_BloodStream( filter, 0.0f, &vecOrigin, &vecDirection, c.r(), c.g(), c.b(), c.a(), nAmount );
}

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Breakable Model TE
//-----------------------------------------------------------------------------
class CTEBreakModel : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBreakModel" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBreakModel, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTEBreakModel( void );
	virtual			~CTEBreakModel( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{ .index = 2 } ]] [[= ks::reflect::Net{ .index = 1 } ]] [[= ks::reflect::Net{ .index = 0 } ]] QAngle			m_angRotation;
	[[= ks::reflect::Net{} ]] Vector			m_vecSize;
	[[= ks::reflect::Net{} ]] Vector			m_vecVelocity;
	[[= ks::reflect::Net{} ]] int				m_nRandomization;
	[[= ks::reflect::Net{} ]] int				m_nModelIndex;
	[[= ks::reflect::Net{} ]] int				m_nCount;
	[[= ks::reflect::Net{} ]] float			m_fTime;
	[[= ks::reflect::Net{} ]] int				m_nFlags;
};

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEBreakModel, DT_TEBreakModel, CTEBreakModel )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBreakModel::CTEBreakModel( void )
{
	m_vecOrigin.Init();
	m_angRotation.Init();
	m_vecSize.Init();
	m_vecVelocity.Init();
	m_nModelIndex		= 0;
	m_nRandomization	= 0;
	m_nCount			= 0;
	m_fTime				= 0.0;
	m_nFlags			= 0;
}

CTEBreakModel::~CTEBreakModel( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TE_BreakModel( IRecipientFilter& filter, float delay,
	const Vector& pos, const QAngle &angles, const Vector& size, const Vector& vel, 
	int modelindex, int randomization, int count, float time, int flags )
{
	tempents->BreakModel( pos, angles, size, vel, randomization, time, count, modelindex, flags );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEBreakModel::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEBreakModel::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	tempents->BreakModel( m_vecOrigin, m_angRotation, m_vecSize, m_vecVelocity,
		m_nRandomization, m_fTime, m_nCount, m_nModelIndex, m_nFlags );
}

void TE_BreakModel( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	Vector vecOrigin, vecSize, vecVel;
	QAngle angles;
	vecOrigin.x = pKeyValues->GetFloat( "originx" );
	vecOrigin.y = pKeyValues->GetFloat( "originy" );
	vecOrigin.z = pKeyValues->GetFloat( "originz" );
	angles.x = pKeyValues->GetFloat( "anglesx" );
	angles.y = pKeyValues->GetFloat( "anglesy" );
	angles.z = pKeyValues->GetFloat( "anglesz" );
	vecSize.x = pKeyValues->GetFloat( "sizex" );
	vecSize.y = pKeyValues->GetFloat( "sizey" );
	vecSize.z = pKeyValues->GetFloat( "sizez" );
	vecVel.x = pKeyValues->GetFloat( "velx" );
	vecVel.y = pKeyValues->GetFloat( "vely" );
	vecVel.z = pKeyValues->GetFloat( "velz" );
	Color c = pKeyValues->GetColor( "color" );
	const char *pModelName = pKeyValues->GetString( "model" );
	int nModelIndex = pModelName[0] ? modelinfo->GetModelIndex( pModelName ) : 0;
	int nRandomization = pKeyValues->GetInt( "randomization" );
	int nCount = pKeyValues->GetInt( "count" );
	float flDuration = pKeyValues->GetFloat( "duration" );
	int nFlags = pKeyValues->GetInt( "flags" );
	TE_BreakModel( filter, 0.0f, vecOrigin, angles, vecSize, vecVel,
		nModelIndex, nRandomization, nCount, flDuration, nFlags );
}

class CBaseEntity;

using namespace ::ks::reflect;

// UNDONE:  Get rid of this?
#define FDECAL_PERMANENT			0x01

//-----------------------------------------------------------------------------
// Purpose: BSP Decal TE
//-----------------------------------------------------------------------------
class CTEBSPDecal : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBSPDecal" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBSPDecal, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTEBSPDecal( void );
	virtual			~CTEBSPDecal( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

	virtual void	Precache( void );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{} ]] int				m_nEntity;
	[[= ks::reflect::Net{} ]] int				m_nIndex;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBSPDecal::CTEBSPDecal( void )
{
	m_vecOrigin.Init();
	m_nEntity = 0;
	m_nIndex = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBSPDecal::~CTEBSPDecal( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEBSPDecal::Precache( void )
{
}

void TE_BSPDecal( IRecipientFilter& filter, float delay,
	const Vector* pos, int entity, int index )
{
	CBaseEntity *ent;
	if ( ( ent = cl_entitylist->GetEnt( entity ) ) == nullptr )
	{
		DevMsg( 1, "Decal: entity = %i", entity );
		return;
	}

	if ( r_decals.GetInt() )
	{
		effects->DecalShoot( index, entity, ent->GetModel(), ent->GetAbsOrigin(), ent->GetAbsAngles(), *pos, 0, FDECAL_PERMANENT );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEBSPDecal::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEBSPDecal::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	CBaseEntity *ent;
	if ( ( ent = cl_entitylist->GetEnt( m_nEntity ) ) == nullptr )
	{
		DevMsg( 1, "Decal: entity = %i", m_nEntity );
		return;
	}

	if ( r_decals.GetInt() )
	{
		effects->DecalShoot( m_nIndex, m_nEntity, ent->GetModel(), ent->GetAbsOrigin(), ent->GetAbsAngles(), m_vecOrigin, 0, FDECAL_PERMANENT );
	}
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEBSPDecal, DT_TEBSPDecal, CTEBSPDecal )

#undef FDECAL_PERMANENT

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Bubbles TE
//-----------------------------------------------------------------------------
class CTEBubbles : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBubbles" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBubbles, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTEBubbles( void );
	virtual			~CTEBubbles( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecMins;
	[[= ks::reflect::Net{} ]] Vector			m_vecMaxs;
	[[= ks::reflect::Net{} ]] float			m_fHeight;
	[[= ks::reflect::Net{} ]] int				m_nModelIndex;
	[[= ks::reflect::Net{} ]] int				m_nCount;
	[[= ks::reflect::Net{} ]] float			m_fSpeed;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBubbles::CTEBubbles( void )
{
	m_vecMins.Init();
	m_vecMaxs.Init();
	m_fHeight = 0.0;
	m_nModelIndex = 0;
	m_nCount = 0;
	m_fSpeed = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBubbles::~CTEBubbles( void )
{
}

void TE_Bubbles( IRecipientFilter& filter, float delay,
	const Vector* mins, const Vector* maxs, float height, int modelindex, int count, float speed )
{
	tempents->Bubbles( *mins, *maxs, height, modelindex, count, speed );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEBubbles::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEBubbles::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	tempents->Bubbles( m_vecMins, m_vecMaxs, m_fHeight, m_nModelIndex, m_nCount, m_fSpeed );
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEBubbles, DT_TEBubbles, CTEBubbles )

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Bubble Trail TE
//-----------------------------------------------------------------------------
class CTEBubbleTrail : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBubbleTrail" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBubbleTrail, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTEBubbleTrail( void );
	virtual			~CTEBubbleTrail( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecMins;
	[[= ks::reflect::Net{} ]] Vector			m_vecMaxs;
	[[= ks::reflect::Net{} ]] float			m_flWaterZ;
	[[= ks::reflect::Net{} ]] int				m_nModelIndex;
	[[= ks::reflect::Net{} ]] int				m_nCount;
	[[= ks::reflect::Net{} ]] float			m_fSpeed;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBubbleTrail::CTEBubbleTrail( void )
{
	m_vecMins.Init();
	m_vecMaxs.Init();
	m_flWaterZ = 0.0;
	m_nModelIndex = 0;
	m_nCount = 0;
	m_fSpeed = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBubbleTrail::~CTEBubbleTrail( void )
{
}

void TE_BubbleTrail( IRecipientFilter& filter, float delay,
	const Vector* mins, const Vector* maxs, float flWaterZ, int modelindex, int count, float speed )
{
	tempents->BubbleTrail( *mins, *maxs, flWaterZ, modelindex, count, speed );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEBubbleTrail::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEBubbleTrail::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	tempents->BubbleTrail( m_vecMins, m_vecMaxs, m_flWaterZ, m_nModelIndex, m_nCount, m_fSpeed );
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEBubbleTrail, DT_TEBubbleTrail, CTEBubbleTrail )

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Client Projectile TE
//-----------------------------------------------------------------------------
class CTEClientProjectile : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEClientProjectile" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEClientProjectile, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

	CTEClientProjectile( void );
	virtual			~CTEClientProjectile( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector m_vecOrigin;
	[[= ks::reflect::Net{} ]] Vector m_vecVelocity;
	[[= ks::reflect::Net{} ]] int m_nModelIndex;
	[[= ks::reflect::Net{} ]] int m_nLifeTime;
	[[= ks::reflect::Net{} ]] EHANDLE m_hOwner;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEClientProjectile::CTEClientProjectile( void )
{
	m_vecOrigin.Init();
	m_vecVelocity.Init();
	m_nModelIndex = 0;
	m_nLifeTime = 0;
	m_hOwner = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEClientProjectile::~CTEClientProjectile( void )
{
}

void TE_ClientProjectile( IRecipientFilter& filter, float delay,
					const Vector* vecOrigin, const Vector* vecVelocity, int modelindex, int lifetime, CBaseEntity *pOwner )
{
	tempents->ClientProjectile( *vecOrigin, *vecVelocity, vec3_origin, modelindex, lifetime, pOwner );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEClientProjectile::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEClientProjectile::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	tempents->ClientProjectile( m_vecOrigin, m_vecVelocity, vec3_origin, m_nModelIndex, m_nLifeTime, m_hOwner );
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEClientProjectile, DT_TEClientProjectile, CTEClientProjectile )

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Decal TE
//-----------------------------------------------------------------------------
class CTEDecal : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEDecal" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEDecal, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTEDecal( void );
	virtual			~CTEDecal( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

	virtual void	Precache( void );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{} ]] Vector			m_vecStart;
	[[= ks::reflect::Net{} ]] int				m_nEntity;
	[[= ks::reflect::Net{} ]] int				m_nHitbox;
	[[= ks::reflect::Net{} ]] int				m_nIndex;
};

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEDecal, DT_TEDecal, CTEDecal )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEDecal::CTEDecal( void )
{
	m_vecOrigin.Init();
	m_vecStart.Init();
	m_nEntity = 0;
	m_nIndex = 0;
	m_nHitbox = 0;

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEDecal::~CTEDecal( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEDecal::Precache( void )
{											 
}

//-----------------------------------------------------------------------------
// Tempent 
//-----------------------------------------------------------------------------
void TE_Decal( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* start, int entity, int hitbox, int index )
{

	trace_t tr;

	// Special case for world entity with hitbox:
	if ( (entity == 0) && (hitbox != 0) )
	{
		Ray_t ray;
		ray.Init( *start, *pos );
		staticpropmgr->AddDecalToStaticProp( *start, *pos, hitbox - 1, index, false, tr );
	}
	else
	{
		// Only decal the world + brush models
		// Here we deal with decals on entities.
		CBaseEntity* ent;
		if ( ( ent = cl_entitylist->GetEnt( entity ) ) == nullptr )
			return;

		ent->AddDecal( *start, *pos, *pos, hitbox, 
			index, false, tr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEDecal::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEDecal::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	CBroadcastRecipientFilter filter;
	TE_Decal( filter, 0.0f, &m_vecOrigin, &m_vecStart, m_nEntity, m_nHitbox, m_nIndex );
}

//-----------------------------------------------------------------------------
// Playback
//-----------------------------------------------------------------------------
void TE_Decal( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	Vector vecOrigin, vecStart;
	vecOrigin.x = pKeyValues->GetFloat( "originx" );
	vecOrigin.y = pKeyValues->GetFloat( "originy" );
	vecOrigin.z = pKeyValues->GetFloat( "originz" );
	vecStart.x = pKeyValues->GetFloat( "startx" );
	vecStart.y = pKeyValues->GetFloat( "starty" );
	vecStart.z = pKeyValues->GetFloat( "startz" );
	int nHitbox = pKeyValues->GetInt( "hitbox" );
	const char *pDecalName = pKeyValues->GetString( "decalname" );

	TE_Decal( filter, 0.0f, &vecOrigin, &vecStart, 0, nHitbox, effects->Draw_DecalIndexFromName( (char*)pDecalName ) );
}

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Dynamic Light
//-----------------------------------------------------------------------------
class CTEDynamicLight : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEDynamicLight" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEDynamicLight, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTEDynamicLight( void );
	virtual			~CTEDynamicLight( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{} ]] float			m_fRadius;
	[[= ks::reflect::Net{} ]] int				r;
	[[= ks::reflect::Net{} ]] int				g;
	[[= ks::reflect::Net{} ]] int				b;
	[[= ks::reflect::Net{} ]] int				exponent;
	[[= ks::reflect::Net{} ]] float			m_fTime;
	[[= ks::reflect::Net{} ]] float			m_fDecay;
};

//-----------------------------------------------------------------------------
// Networking 
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEDynamicLight, DT_TEDynamicLight, CTEDynamicLight )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEDynamicLight::CTEDynamicLight( void )
{
	m_vecOrigin.Init();
	r = 0;
	g = 0;
	b = 0;
	exponent = 0;
	m_fRadius = 0.0;
	m_fTime = 0.0;
	m_fDecay = 0.0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEDynamicLight::~CTEDynamicLight( void )
{
}

void TE_DynamicLight( IRecipientFilter& filter, float delay,
	const Vector* org, int r, int g, int b, int exponent, float radius, float time, float decay, int nLightIndex )
{
	dlight_t *dl = effects->CL_AllocDlight( nLightIndex );
	if ( !dl )
		return;

	dl->origin	= *org;
	dl->radius	= radius;
	dl->color.r	= r;
	dl->color.g	= g;
	dl->color.b	= b;
	dl->color.exponent	= exponent;
	dl->die		= gpGlobals->curtime + time;
	dl->decay	= decay;

}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEDynamicLight::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEDynamicLight::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	CBroadcastRecipientFilter filter;
	TE_DynamicLight( filter, 0.0f, &m_vecOrigin, r, g, b, exponent, m_fRadius, m_fTime, m_fDecay, LIGHT_INDEX_TE_DYNAMIC );
}

void TE_DynamicLight( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	Vector vecOrigin;
	vecOrigin.x = pKeyValues->GetFloat( "originx" );
	vecOrigin.y = pKeyValues->GetFloat( "originy" );
	vecOrigin.z = pKeyValues->GetFloat( "originz" );
	float flDuration = pKeyValues->GetFloat( "duration" );
	Color c = pKeyValues->GetColor( "color" );
	int nExponent = pKeyValues->GetInt( "exponent" );
	float flRadius = pKeyValues->GetFloat( "radius" );
	float flDecay = pKeyValues->GetFloat( "decay" );
 	int nLightIndex = pKeyValues->GetInt( "lightindex", LIGHT_INDEX_TE_DYNAMIC );

	TE_DynamicLight( filter, 0.0f, &vecOrigin, c.r(), c.g(), c.b(), nExponent, 
		flRadius, flDuration, flDecay, nLightIndex );
}

class CEffectData;
class CNewParticleEffect;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// CClientEffectRegistration registration
//-----------------------------------------------------------------------------

CClientEffectRegistration *CClientEffectRegistration::s_pHead = nullptr;

CClientEffectRegistration::CClientEffectRegistration( const char *pEffectName, ClientEffectCallback fn )
{
	contract_assert( ( pEffectName[0] != '\"' ) && "Error: Effect %s. "
		"Remove quotes around the effect name in DECLARE_CLIENT_EFFECT.\n" );

	m_pEffectName = pEffectName;
	m_pFunction = fn;
	m_pNext = s_pHead;
	s_pHead = this;
}

//-----------------------------------------------------------------------------
// Purpose: EffectDispatch TE
//-----------------------------------------------------------------------------
class CTEEffectDispatch : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEEffectDispatch" } ]]
	[[= ks::reflect::FromTable<"m_EffectData", &REFERENCE_RECV_TABLE( DT_EffectData )>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEEffectDispatch, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTEEffectDispatch( void );
	virtual			~CTEEffectDispatch( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
CEffectData m_EffectData;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEEffectDispatch::CTEEffectDispatch( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEEffectDispatch::~CTEEffectDispatch( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchEffectToCallback( const char *pEffectName, const CEffectData &m_EffectData )
{
	// Built a faster lookup
	static ks::idict< CClientEffectRegistration* > map;
	static bool bInitializedMap = false;
	if ( !bInitializedMap )
	{
		for ( CClientEffectRegistration *pReg = CClientEffectRegistration::s_pHead; pReg; pReg = pReg->m_pNext )
		{
			// If the name matches, call it
			if ( !map.try_emplace( pReg->m_pEffectName, pReg ).second )
			{
				Warning( "Encountered multiple different effects with the same name \"%s\"!\n", pReg->m_pEffectName );
				continue;
			}
		}
		bInitializedMap = true;
	}

	// Look through all the registered callbacks
	CClientEffectRegistration **ppReg = ks::find_ptr( map, pEffectName );
	if ( !ppReg )
	{
		Warning("DispatchEffect: effect \"%s\" not found on client\n", pEffectName );
		return;
	}

	// NOTE: Here, we want to scope resource access to only be able to use
	// those resources specified as being dependencies of this effect
	g_pPrecacheSystem->LimitResourceAccess( DISPATCH_EFFECT, pEffectName );
	( *ppReg )->m_pFunction( m_EffectData );

	// NOTE: Here, we no longer need to restrict resource access
	g_pPrecacheSystem->EndLimitedResourceAccess( );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEEffectDispatch::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEEffectDispatch::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Find the effect name.
	const char *pEffectName = g_StringTableEffectDispatch->GetString( m_EffectData.GetEffectNameIndex() );
	if ( pEffectName )
	{
		DispatchEffectToCallback( pEffectName, m_EffectData );
	}
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEEffectDispatch, DT_TEEffectDispatch, CTEEffectDispatch )

//-----------------------------------------------------------------------------
// Client version of dispatch effect, for predicted weapons
//-----------------------------------------------------------------------------
void DispatchEffect( IRecipientFilter& filter, float delay, const char *pName, const CEffectData &data )
{
	if ( !te->SuppressTE( filter ) )
	{
		DispatchEffectToCallback( pName, data );
	}
}

void DispatchEffect( const char *pName, const CEffectData &data )
{
	CPASFilter filter( data.m_vOrigin );
	if ( !te->SuppressTE( filter ) )
	{
		DispatchEffectToCallback( pName, data );
	}
}

//-----------------------------------------------------------------------------
// Playback
//-----------------------------------------------------------------------------
void DispatchEffect( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	CEffectData data;
	data.m_nMaterial = 0;
		  
	data.m_vOrigin.x = pKeyValues->GetFloat( "originx" );
	data.m_vOrigin.y = pKeyValues->GetFloat( "originy" );
	data.m_vOrigin.z = pKeyValues->GetFloat( "originz" );
	data.m_vStart.x = pKeyValues->GetFloat( "startx" );
	data.m_vStart.y = pKeyValues->GetFloat( "starty" );
	data.m_vStart.z = pKeyValues->GetFloat( "startz" );
	data.m_vNormal.x = pKeyValues->GetFloat( "normalx" );
	data.m_vNormal.y = pKeyValues->GetFloat( "normaly" );
	data.m_vNormal.z = pKeyValues->GetFloat( "normalz" );
	data.m_vAngles.x = pKeyValues->GetFloat( "anglesx" );
	data.m_vAngles.y = pKeyValues->GetFloat( "anglesy" );
	data.m_vAngles.z = pKeyValues->GetFloat( "anglesz" );
	data.m_fFlags = pKeyValues->GetInt( "flags" );
	data.m_flScale = pKeyValues->GetFloat( "scale" );
	data.m_flMagnitude = pKeyValues->GetFloat( "magnitude" );
	data.m_flRadius = pKeyValues->GetFloat( "radius" );
	const char *pSurfaceProp = pKeyValues->GetString( "surfaceprop" );
	data.m_nSurfaceProp = physprops->GetSurfaceIndex( pSurfaceProp );
	data.m_nDamageType = pKeyValues->GetInt( "damagetype" );
	data.m_nHitBox = pKeyValues->GetInt( "hitbox" );
	data.m_nColor = pKeyValues->GetInt( "color" );
	data.m_nAttachmentIndex = pKeyValues->GetInt( "attachmentindex" );

	// NOTE: Ptrs are our way of indicating it's an entindex
	ClientEntityHandle_t hWorld = ClientEntityList().EntIndexToHandle( 0 );
	data.m_hEntity = ClientEntityHandle_t::UnsafeFromIndex( size_cast< int >( (intp) pKeyValues->GetPtr( "entindex", ( void* )(intp)hWorld.ToInt() ) ) );

	const char *pEffectName = pKeyValues->GetString( "effectname" );

	DispatchEffect( filter, 0.0f, pEffectName, data );
}

//-----------------------------------------------------------------------------
// Purpose: Displays an error effect in case of missing precache
//-----------------------------------------------------------------------------
void ErrorEffectCallback( const CEffectData &data )
{
	ks::intrusive_ptr<CNewParticleEffect> pEffect = CNewParticleEffect::Create( nullptr, "error" );
	if ( pEffect->IsValid() )
	{
		pEffect->SetSortOrigin( data.m_vOrigin );
		pEffect->SetControlPoint( 0, data.m_vOrigin );
		pEffect->SetControlPoint( 1, data.m_vStart );
		Vector vecForward, vecRight, vecUp;
		AngleVectors( data.m_vAngles, &vecForward, &vecRight, &vecUp );
		pEffect->SetControlPointOrientation( 0, vecForward, vecRight, vecUp );
	}
}

DECLARE_CLIENT_EFFECT_BEGIN( Error, ErrorEffectCallback )
	PRECACHE( PARTICLE_SYSTEM, "error" )
DECLARE_CLIENT_EFFECT_END()

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Energy Splash TE
//-----------------------------------------------------------------------------
class CTEEnergySplash : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEEnergySplash", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLIENTCLASS();

					CTEEnergySplash( void );
	virtual			~CTEEnergySplash( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

	virtual void	Precache( void );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecPos;
	[[= ks::reflect::Net{} ]] Vector			m_vecDir;
	[[= ks::reflect::Net{} ]] bool			m_bExplosive;

	const struct model_t *m_pModel;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEEnergySplash::CTEEnergySplash( void )
{
	m_vecPos.Init();
	m_vecDir.Init();
	m_bExplosive = false;
	m_pModel = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEEnergySplash::~CTEEnergySplash( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEEnergySplash::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEEnergySplash::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEEnergySplash::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	g_pEffects->EnergySplash( m_vecPos, m_vecDir, m_bExplosive );
}

void TE_EnergySplash( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* dir, bool bExplosive )
{
	g_pEffects->EnergySplash( *pos, *dir, bExplosive );
}

// Expose the TE to the engine.
IMPLEMENT_CLIENTCLASS_EVENT( CTEEnergySplash, DT_TEEnergySplash, CTEEnergySplash );

IMPLEMENT_REFLECT_TABLE( CTEEnergySplash, DT_TEEnergySplash );

class CParticleMgr;
class CParticleRenderIterator;
class CParticleSimulateIterator;
class CBaseAnimating;
class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Client explosions

static constexpr auto OLD_EXPLOSION = 0;

// Enumator class for ragdolls being affected by explosive forces
CRagdollExplosionEnumerator::CRagdollExplosionEnumerator( Vector origin, float radius, float magnitude )
{
	m_vecOrigin		= origin;
	m_flMagnitude	= magnitude;
	m_flRadius		= radius;
}

// Actual work code
IterationRetval_t CRagdollExplosionEnumerator::EnumElement( IHandleEntity *pHandleEntity )
{
	CBaseEntity *pEnt = ClientEntityList().GetBaseEntityFromHandle( pHandleEntity->GetRefEHandle() );
	
	if ( pEnt == nullptr )
		return ITERATION_CONTINUE;

	CBaseAnimating *pModel = static_cast< CBaseAnimating * >( pEnt );

	// If the ragdoll was created on this tick, then the forces were already applied on the server
	if ( pModel == nullptr || WasRagdollCreatedOnCurrentTick( pEnt ) )
		return ITERATION_CONTINUE;
	
	m_Entities.push_back( pEnt );

	return ITERATION_CONTINUE;
}

CRagdollExplosionEnumerator::~CRagdollExplosionEnumerator()
{
	for (int i = 0; i < std::ssize( m_Entities ); i++ )
	{
		CBaseEntity *pEnt = m_Entities[i];
		CBaseAnimating *pModel = static_cast< CBaseAnimating * >( pEnt );

		Vector	position = pEnt->CollisionProp()->GetCollisionOrigin();

		Vector	dir		= position - m_vecOrigin;
		float	dist	= VectorNormalize( dir );
		float	force	= m_flMagnitude - ( ( m_flMagnitude / m_flRadius ) * dist );

		if ( force <= 1.0f )
			continue;

		trace_t	tr;
		UTIL_TraceLine( m_vecOrigin, position, MASK_SHOT_HULL, nullptr, COLLISION_GROUP_NONE, &tr );

		// debugoverlay->AddLineOverlay( m_vecOrigin, position, 0,255,0, true, 18.0 );

		if ( tr.fraction < 1.0f && tr.Ent<CBaseEntity>() != pModel )
			continue;	

		dir *= force; // scale force

		// tricky, adjust tr.start so end-start->= force
		tr.startpos = tr.endpos - dir;
		// move expolsion center a bit down, so things fly higher 
		tr.startpos.z -= 32.0f;

		pModel->ImpactTrace( &tr, DMG_BLAST, nullptr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Explosion TE
//-----------------------------------------------------------------------------
class CTEExplosion : public CTEParticleSystem
{
	[[= ks::reflect::NetTable{ .name = "DT_TEExplosion" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEExplosion, CTEParticleSystem );
	DECLARE_CLIENTCLASS();

					CTEExplosion( void );
	virtual			~CTEExplosion( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );
	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );

public:
	void			AffectRagdolls( void );

	[[= ks::reflect::Net{} ]] int				m_nModelIndex;
	[[= ks::reflect::Net{} ]] float			m_fScale;
	[[= ks::reflect::Net{} ]] int				m_nFrameRate;
	[[= ks::reflect::Net{} ]] int				m_nFlags;
	[[= ks::reflect::Net{} ]] Vector			m_vecNormal;
	[[= ks::reflect::Net{} ]] char			m_chMaterialType;
	[[= ks::reflect::Net{} ]] int				m_nRadius;
	[[= ks::reflect::Net{} ]] int				m_nMagnitude;

	//CParticleCollision	m_ParticleCollision;
	CParticleMgr		*m_pParticleMgr;
	PMaterialHandle		m_MaterialHandle;
	bool			m_bShouldAffectRagdolls;
};

//-----------------------------------------------------------------------------
// Networking 
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEExplosion, DT_TEExplosion, CTEExplosion )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEExplosion::CTEExplosion( void )
{
	m_bShouldAffectRagdolls = true;
	m_nModelIndex = 0;
	m_fScale = 0;
	m_nFrameRate = 0;
	m_nFlags = 0;
	m_vecNormal.Init();
	m_chMaterialType = 'C';
	m_nRadius = 0;
	m_nMagnitude = 0;

	m_pParticleMgr		= nullptr;
	m_MaterialHandle	= INVALID_MATERIAL_HANDLE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEExplosion::~CTEExplosion( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEExplosion::AffectRagdolls( void )
{
	if ( ( m_nRadius == 0 ) || ( m_nMagnitude == 0 ) || (!m_bShouldAffectRagdolls) )
		return;

	CRagdollExplosionEnumerator	ragdollEnum( m_vecOrigin, m_nRadius, m_nMagnitude );
	::partition->EnumerateElementsInSphere( PARTITION_CLIENT_RESPONSIVE_EDICTS, m_vecOrigin, m_nRadius, false, &ragdollEnum );
}

//
// CExplosionOverlay
//

bool CExplosionOverlay::Update( void )
{
	m_flLifetime += gpGlobals->frametime;
	
	const float flTotalLifetime = 0.1f;

	if ( m_flLifetime < flTotalLifetime )
	{
		float flColorScale = 1.0f - ( m_flLifetime / flTotalLifetime );

		for( int i=0; i < m_nSprites; i++ )
		{
			m_Sprites[i].m_vColor = m_vBaseColors[i] * flColorScale;
			
			m_Sprites[i].m_flHorzSize += 16.0f * gpGlobals->frametime;
			m_Sprites[i].m_flVertSize += 16.0f * gpGlobals->frametime;
		}

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEExplosion::PostDataUpdate( DataUpdateType_t updateType )
{
	AffectRagdolls();

	// Filter out a water explosion
	if ( UTIL_PointContents( m_vecOrigin, MASK_WATER ) & CONTENTS_WATER )
	{
		WaterExplosionEffect().Create( m_vecOrigin, m_nMagnitude, m_fScale, m_nFlags );
		return;
	}

	if ( !( m_nFlags & TE_EXPLFLAG_NOFIREBALL ) )
	{
		if ( CExplosionOverlay *pOverlay = new CExplosionOverlay )
		{
			pOverlay->m_flLifetime	= 0;
			pOverlay->m_vPos		= m_vecOrigin;
			pOverlay->m_nSprites	= 1;
			
			pOverlay->m_vBaseColors[0].Init( 1.0f, 0.9f, 0.7f );

			pOverlay->m_Sprites[0].m_flHorzSize = 0.05f;
			pOverlay->m_Sprites[0].m_flVertSize = pOverlay->m_Sprites[0].m_flHorzSize*0.5f;

			pOverlay->Activate();
		}
	}

	BaseExplosionEffect().Create( m_vecOrigin, m_nMagnitude, m_fScale, m_nFlags );
}

void CTEExplosion::RenderParticles( CParticleRenderIterator *pIterator )
{
}

void CTEExplosion::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	pIterator->RemoveAllParticles();
}

void TE_Explosion( IRecipientFilter& filter, float delay,
	const Vector& pos, int modelindex, float scale, int framerate, int flags, int radius, int magnitude, 
	const Vector& normal, unsigned char materialType, bool bShouldAffectRagdolls )
{
	// Major hack to access singleton object for doing this event (simulate receiving network message)
	__g_CTEExplosion.m_nModelIndex = modelindex;
	__g_CTEExplosion.m_fScale = scale;
	__g_CTEExplosion.m_nFrameRate = framerate;
	__g_CTEExplosion.m_nFlags = flags;
	__g_CTEExplosion.m_vecOrigin = pos;
	__g_CTEExplosion.m_vecNormal = normal;
	__g_CTEExplosion.m_chMaterialType = materialType;
	__g_CTEExplosion.m_nRadius = radius;
	__g_CTEExplosion.m_nMagnitude = magnitude;
	__g_CTEExplosion.m_bShouldAffectRagdolls = bShouldAffectRagdolls;

	__g_CTEExplosion.PostDataUpdate( DATA_UPDATE_CREATED );

}

void TE_Explosion( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	Vector vecOrigin, vecNormal;
	vecOrigin.x = pKeyValues->GetFloat( "originx" );
	vecOrigin.y = pKeyValues->GetFloat( "originy" );
	vecOrigin.z = pKeyValues->GetFloat( "originz" );
	vecNormal.x = pKeyValues->GetFloat( "directionx" );
	vecNormal.y = pKeyValues->GetFloat( "directiony" );
	vecNormal.z = pKeyValues->GetFloat( "directionz" );
	const char *pModelName = pKeyValues->GetString( "model" );
	int nModelIndex = pModelName[0] ? modelinfo->GetModelIndex( pModelName ) : 0;
	float flScale = pKeyValues->GetFloat( "scale" );
	int nFrameRate = pKeyValues->GetInt( "framerate" );
	int nFlags = pKeyValues->GetInt( "flags" );
	int nMaterialType = pKeyValues->GetInt( "materialtype" );
	int nRadius = pKeyValues->GetInt( "radius" );
	int nMagnitude = pKeyValues->GetInt( "magnitude" );
	TE_Explosion( filter, 0.0f, vecOrigin, nModelIndex, flScale, nFrameRate,
		nFlags, nRadius, nMagnitude, vecNormal, (unsigned char)nMaterialType, false );
}

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Fizz TE
//-----------------------------------------------------------------------------
class CTEFizz : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEFizz" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEFizz, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTEFizz( void );
	virtual			~CTEFizz( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] int				m_nEntity;
	[[= ks::reflect::Net{} ]] int				m_nModelIndex;
	[[= ks::reflect::Net{} ]] int				m_nDensity;
	[[= ks::reflect::Net{} ]] int				m_nCurrent;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEFizz::CTEFizz( void )
{
	m_nEntity		= 0;
	m_nModelIndex	= 0;
	m_nDensity		= 0;
	m_nCurrent		= 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEFizz::~CTEFizz( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEFizz::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEFizz::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	CBaseEntity *pEnt = cl_entitylist->GetEnt( m_nEntity );
	if (pEnt != nullptr)
	{
		tempents->FizzEffect(pEnt, m_nModelIndex, m_nDensity, m_nCurrent );
	}
}

void TE_Fizz( IRecipientFilter& filter, float delay,
	const CBaseEntity *ed, int modelindex, int density, int current )
{
	CBaseEntity *pEnt = (CBaseEntity *)ed;
	if (pEnt != nullptr)
	{
		tempents->FizzEffect(pEnt, modelindex, density, current );
	}
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEFizz, DT_TEFizz, CTEFizz )

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Footprint Decal TE
//-----------------------------------------------------------------------------

class CTEFootprintDecal : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEFootprintDecal" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEFootprintDecal, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTEFootprintDecal( void );
	virtual			~CTEFootprintDecal( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

	virtual void	Precache( void );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{} ]] Vector			m_vecDirection;
	Vector			m_vecStart;
	[[= ks::reflect::Net{} ]] int				m_nEntity;
	[[= ks::reflect::Net{} ]] int				m_nIndex;
	[[= ks::reflect::Net{} ]] char			m_chMaterialType;
};

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEFootprintDecal, DT_TEFootprintDecal, CTEFootprintDecal )

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------

CTEFootprintDecal::CTEFootprintDecal( void )
{
	m_vecOrigin.Init();
	m_vecStart.Init();
	m_nEntity = 0;
	m_nIndex = 0;
	m_chMaterialType = 'C';
}

CTEFootprintDecal::~CTEFootprintDecal( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

void CTEFootprintDecal::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Do stuff when data changes
//-----------------------------------------------------------------------------

void CTEFootprintDecal::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEFootprintDecal::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// FIXME: Make this choose the decal based on material type
	if ( r_decals.GetInt() )
	{
		CBaseEntity *ent = cl_entitylist->GetEnt( m_nEntity );
		if ( ent )
		{
			effects->DecalShoot( m_nIndex, 
				m_nEntity, ent->GetModel(), ent->GetAbsOrigin(), ent->GetAbsAngles(), m_vecOrigin, &m_vecDirection, 0 );
		}
	}
}

void TE_FootprintDecal( IRecipientFilter& filter, float delay, const Vector *origin, const Vector* right, 
	int entity, int index, unsigned char materialType )
{
	if ( r_decals.GetInt() )
	{
		CBaseEntity *ent = cl_entitylist->GetEnt( entity );
		if ( ent )
		{
			effects->DecalShoot( index, entity, ent->GetModel(), ent->GetAbsOrigin(), ent->GetAbsAngles(), *origin, right, 0 );
		}
	}
}

using namespace ::ks::reflect;

#define PI 3.14159265359
static constexpr auto GLASS_SHARD_MIN_LIFE = 2;
static constexpr auto GLASS_SHARD_MAX_LIFE = 5;
static constexpr auto GLASS_SHARD_NOISE = 0.3;
static constexpr auto GLASS_SHARD_GRAVITY = 500;
static constexpr auto GLASS_SHARD_DAMPING = 0.3;
 
PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheEffectGlassShatter )
PRECACHE( MATERIAL, "effects/fleck_glass1" )
PRECACHE( MATERIAL, "effects/fleck_glass2" )
PRECACHE( MATERIAL, "effects/fleck_tile1" )
PRECACHE( MATERIAL, "effects/fleck_tile2" )
PRECACHE_REGISTER_END()

ConVar fx_glass_velocity_cap(side_registry(), "fx_glass_velocity_cap", "0", 0, "Maximum downwards speed of shattered glass particles");

//###################################################
// > CTEShatterSurface
//###################################################
class CTEShatterSurface : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEShatterSurface" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEShatterSurface, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

	CTEShatterSurface( void );
	~CTEShatterSurface( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector					m_vecOrigin;
	[[= ks::reflect::Net{} ]] QAngle					m_vecAngles;
	[[= ks::reflect::Net{} ]] Vector					m_vecForce;
	[[= ks::reflect::Net{} ]] Vector					m_vecForcePos;
	[[= ks::reflect::Net{} ]] float					m_flWidth;
	[[= ks::reflect::Net{} ]] float					m_flHeight;
	[[= ks::reflect::Net{} ]] float					m_flShardSize;
	PMaterialHandle			m_pMaterialHandle;
	[[= ks::reflect::Net{} ]] int						m_nSurfaceType;
	[[= ks::reflect::Net{ .index = 2 } ]] [[= ks::reflect::Net{ .index = 1 } ]] [[= ks::reflect::Net{ .index = 0 } ]] byte					m_uchFrontColor[3];
	[[= ks::reflect::Net{ .index = 2 } ]] [[= ks::reflect::Net{ .index = 1 } ]] [[= ks::reflect::Net{ .index = 0 } ]] byte					m_uchBackColor[3];
};

//------------------------------------------------------------------------------
// Networking
//------------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEShatterSurface, DT_TEShatterSurface, CTEShatterSurface )

//------------------------------------------------------------------------------
// Constructor, destructor
//------------------------------------------------------------------------------
CTEShatterSurface::CTEShatterSurface( void )
{
	m_vecOrigin.Init();
	m_vecAngles.Init();
	m_vecForce.Init();
	m_vecForcePos.Init();
	m_flWidth			= 16.0;
	m_flHeight			= 16.0;
	m_flShardSize		= 3;
	m_nSurfaceType		= SHATTERSURFACE_GLASS;
	m_uchFrontColor[0]	= 255;
	m_uchFrontColor[1]	= 255;
	m_uchFrontColor[2]	= 255;
	m_uchBackColor[0]	= 255;
	m_uchBackColor[1]	= 255;
	m_uchBackColor[2]	= 255;
}

CTEShatterSurface::~CTEShatterSurface()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEShatterSurface::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEShatterSurface::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	ks::intrusive_ptr<CSimple3DEmitter> pGlassEmitter = CSimple3DEmitter::Create( "CTEShatterSurface 1" );
	pGlassEmitter->SetSortOrigin( m_vecOrigin );

	Vector vecColor;
	engine->ComputeLighting( m_vecOrigin, nullptr, true, vecColor );

	// HACK: Blend a little toward white to match the materials...
	VectorLerp( vecColor, Vector( 1, 1, 1 ), 0.3, vecColor );

	PMaterialHandle *hMaterial;
	if (m_nSurfaceType == SHATTERSURFACE_GLASS)
	{
		hMaterial = g_Mat_Fleck_Glass;
	}
	else
	{
		hMaterial = g_Mat_Fleck_Tile;
	}

	// ---------------------------------------------------
	// Figure out number of particles required to fill space
	// ---------------------------------------------------
	int nNumWide = m_flWidth  / m_flShardSize;
	int nNumHigh = m_flHeight / m_flShardSize;

	Vector vWidthStep,vHeightStep;
	AngleVectors(m_vecAngles,nullptr,&vWidthStep,&vHeightStep);
	vWidthStep	*= m_flShardSize;
	vHeightStep *= m_flShardSize;

	// ---------------------
	// Create glass shards
	// ----------------------
	Vector vCurPos = m_vecOrigin;
	vCurPos.x += 0.5*m_flShardSize;
	vCurPos.z += 0.5*m_flShardSize;

	float flMinSpeed = 9999999999.0f;
	float flMaxSpeed = 0;

	Particle3D *pParticle = nullptr;

	for (int width=0;width<nNumWide;width++)
	{
		for (int height=0;height<nNumHigh;height++)
		{			
			pParticle = (Particle3D *) pGlassEmitter->AddParticle( sizeof(Particle3D), hMaterial[random_valve->RandomInt(0,1)], vCurPos );

			Vector vForceVel = Vector(0,0,0);
			if (random_valve->RandomInt(0, 3) != 0)
			{
				float flForceDistSqr = (vCurPos - m_vecForcePos).LengthSqr();
				vForceVel = m_vecForce;
				if (flForceDistSqr > 0 )
				{
					vForceVel *= ( 40.0f / flForceDistSqr );
				}
			}

			// cap the Z velocity of the shards
			if (fx_glass_velocity_cap.GetFloat() > 0 && vForceVel.z < -fx_glass_velocity_cap.GetFloat())
			{
				vForceVel.z = random_valve->RandomFloat(-fx_glass_velocity_cap.GetFloat(), -(fx_glass_velocity_cap.GetFloat() * 0.66f));
			}

			if (pParticle)
			{
				pParticle->m_flLifeRemaining	= random_valve->RandomFloat(GLASS_SHARD_MIN_LIFE,GLASS_SHARD_MAX_LIFE);
				pParticle->m_vecVelocity		= vForceVel;
				pParticle->m_vecVelocity	   += RandomVector(-25,25);
				pParticle->m_uchSize			= m_flShardSize + random_valve->RandomFloat(-0.5*m_flShardSize,0.5*m_flShardSize);
				pParticle->m_vAngles			= m_vecAngles;
				pParticle->m_flAngSpeed			= random_valve->RandomFloat(-400,400);

				pParticle->m_uchFrontColor[0]	= (byte)(m_uchFrontColor[0] * vecColor.x );
				pParticle->m_uchFrontColor[1]	= (byte)(m_uchFrontColor[1] * vecColor.y );
				pParticle->m_uchFrontColor[2]	= (byte)(m_uchFrontColor[2] * vecColor.z );
				pParticle->m_uchBackColor[0]	= (byte)(m_uchBackColor[0] * vecColor.x );
				pParticle->m_uchBackColor[1]	= (byte)(m_uchBackColor[1] * vecColor.y );
				pParticle->m_uchBackColor[2]	= (byte)(m_uchBackColor[2] * vecColor.z );
			}

			// Keep track of min and max speed for collision detection
			float  flForceSpeed = vForceVel.Length();
			if (flForceSpeed > flMaxSpeed)
			{
				flMaxSpeed = flForceSpeed;
			}
			if (flForceSpeed < flMinSpeed)
			{
				flMinSpeed = flForceSpeed;
			}

			vCurPos += vHeightStep;
		}
		vCurPos	 -= nNumHigh*vHeightStep;
		vCurPos	 += vWidthStep;
	}

	// --------------------------------------------------
	// Set collision parameters
	// --------------------------------------------------
	Vector vMoveDir = m_vecForce;
	VectorNormalize(vMoveDir);

	pGlassEmitter->m_ParticleCollision.Setup( m_vecOrigin, &vMoveDir, GLASS_SHARD_NOISE, 
												flMinSpeed, flMaxSpeed, GLASS_SHARD_GRAVITY, GLASS_SHARD_DAMPING );
}

void TE_ShatterSurface( IRecipientFilter& filter, float delay,
	const Vector* pos, const QAngle* angle, const Vector* vForce, const Vector* vForcePos, 
	float width, float height, float shardsize, ShatterSurface_t surfacetype,
	int front_r, int front_g, int front_b, int back_r, int back_g, int back_b)
{
	// Major hack to simulate receiving network message
	__g_CTEShatterSurface.m_vecOrigin = *pos;
	__g_CTEShatterSurface.m_vecAngles = *angle;
	__g_CTEShatterSurface.m_vecForce = *vForce;
	__g_CTEShatterSurface.m_vecForcePos = *vForcePos;
	__g_CTEShatterSurface.m_flWidth = width;
	__g_CTEShatterSurface.m_flHeight = height;
	__g_CTEShatterSurface.m_flShardSize = shardsize;
	__g_CTEShatterSurface.m_nSurfaceType = surfacetype;
	__g_CTEShatterSurface.m_uchFrontColor[0] = front_r;
	__g_CTEShatterSurface.m_uchFrontColor[1] = front_g;
	__g_CTEShatterSurface.m_uchFrontColor[2] = front_b;
	__g_CTEShatterSurface.m_uchBackColor[0] = back_r;
	__g_CTEShatterSurface.m_uchBackColor[1] = back_g;
	__g_CTEShatterSurface.m_uchBackColor[2] = back_b;

	__g_CTEShatterSurface.PostDataUpdate( DATA_UPDATE_CREATED );
}

void TE_ShatterSurface( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	Vector vecOrigin, vecForce, vecForcePos;
	QAngle angles;
	vecOrigin.x = pKeyValues->GetFloat( "originx" );
	vecOrigin.y = pKeyValues->GetFloat( "originy" );
	vecOrigin.z = pKeyValues->GetFloat( "originz" );
	angles.x = pKeyValues->GetFloat( "anglesx" );
	angles.y = pKeyValues->GetFloat( "anglesy" );
	angles.z = pKeyValues->GetFloat( "anglesz" );
	vecForce.x = pKeyValues->GetFloat( "forcex" );
	vecForce.y = pKeyValues->GetFloat( "forcey" );
	vecForce.z = pKeyValues->GetFloat( "forcez" );
	vecForcePos.x = pKeyValues->GetFloat( "forceposx" );
	vecForcePos.y = pKeyValues->GetFloat( "forceposy" );
	vecForcePos.z = pKeyValues->GetFloat( "forceposz" );
	Color front = pKeyValues->GetColor( "frontcolor" );
	Color back = pKeyValues->GetColor( "backcolor" );
	float flWidth = pKeyValues->GetFloat( "width" );
	float flHeight = pKeyValues->GetFloat( "height" );
	float flSize = pKeyValues->GetFloat( "size" );
	ShatterSurface_t nSurfaceType = (ShatterSurface_t)pKeyValues->GetInt( "surfacetype" );
	TE_ShatterSurface( filter, 0.0f, &vecOrigin, &angles, &vecForce, &vecForcePos,
		flWidth, flHeight, flSize, nSurfaceType, front.r(), front.g(), front.b(),
		back.r(), back.g(), back.b() );
}

#undef PI

class CLocalTempEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Glow Sprite TE
//-----------------------------------------------------------------------------
class CTEGlowSprite : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEGlowSprite" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEGlowSprite, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTEGlowSprite( void );
	virtual			~CTEGlowSprite( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{} ]] int				m_nModelIndex;
	[[= ks::reflect::Net{} ]] float			m_fScale;
	[[= ks::reflect::Net{} ]] float			m_fLife;
	[[= ks::reflect::Net{} ]] int				m_nBrightness;
};

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEGlowSprite, DT_TEGlowSprite, CTEGlowSprite )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEGlowSprite::CTEGlowSprite( void )
{
	m_vecOrigin.Init();
	m_nModelIndex = 0;
	m_fScale = 0;
	m_fLife = 0;
	m_nBrightness = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEGlowSprite::~CTEGlowSprite( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEGlowSprite::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEGlowSprite::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	float a = ( 1.0 / 255.0 ) * m_nBrightness;
	CLocalTempEntity *ent = tempents->TempSprite( m_vecOrigin, vec3_origin, m_fScale, m_nModelIndex, kRenderTransAdd, 0, a, m_fLife, FTENT_SPRANIMATE | FTENT_SPRANIMATELOOP );
	if ( ent )
	{
		ent->bounceFactor = 0.2;
	}
}

void TE_GlowSprite( IRecipientFilter& filter, float delay,
	const Vector* pos, int modelindex, float life, float size, int brightness )
{
	float a = ( 1.0 / 255.0 ) * brightness;
	CLocalTempEntity *ent = tempents->TempSprite( *pos, vec3_origin, size, modelindex, kRenderTransAdd, 0, a, life, FTENT_SPRANIMATE | FTENT_SPRANIMATELOOP );
	if ( ent )
	{
		ent->bounceFactor = 0.2;
	}
}

void TE_GlowSprite( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	Vector vecOrigin;
	vecOrigin.x = pKeyValues->GetFloat( "originx" );
	vecOrigin.y = pKeyValues->GetFloat( "originy" );
	vecOrigin.z = pKeyValues->GetFloat( "originz" );
	const char *pModelName = pKeyValues->GetString( "model" );
	int nModelIndex = pModelName[0] ? modelinfo->GetModelIndex( pModelName ) : 0;
	float flDuration = pKeyValues->GetFloat( "duration" );
	float flSize = pKeyValues->GetFloat( "size" );
	int nBrightness = pKeyValues->GetFloat( "brightness" );

	TE_GlowSprite( filter, delay, &vecOrigin, nModelIndex, flDuration, flSize, nBrightness );
}

using namespace ::ks::reflect;

extern IPhysicsSurfaceProps *physprops;

class CTEImpact : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEImpact" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEImpact, CBaseTempEntity );
	
	DECLARE_CLIENTCLASS();

	CTEImpact( void );
	virtual	~CTEImpact( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );
	virtual void	Precache( void );

	virtual void	PlayImpactSound( trace_t &tr );
	virtual void	PerformCustomEffects( trace_t &tr, Vector &shotDir );
public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{} ]] Vector			m_vecNormal;
	[[= ks::reflect::Net{} ]] int				m_iType;
	[[= ks::reflect::Net{} ]] byte			m_ucFlags;
};

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
CTEImpact::CTEImpact( void )
{
	m_vecOrigin.Init();
	m_vecNormal.Init();
	
	m_iType		= -1;
	m_ucFlags	= 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
CTEImpact::~CTEImpact( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEImpact::Precache( void )
{
	//TODO: Precache all materials/sounds used by impacts here
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : unused - 
//-----------------------------------------------------------------------------
void CTEImpact::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEImpact::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEImpact::PlayImpactSound( trace_t &tr )
{
}

//-----------------------------------------------------------------------------
// Purpose: Perform custom effects based on the Decal index
//-----------------------------------------------------------------------------
void CTEImpact::PerformCustomEffects( trace_t &tr, Vector &shotDir )
{
}

//Receive data table
IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEImpact, DT_TEImpact, CTEImpact )

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Kills Player Attachments
//-----------------------------------------------------------------------------
class CTEKillPlayerAttachments : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEKillPlayerAttachments" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEKillPlayerAttachments, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTEKillPlayerAttachments( void );
	virtual			~CTEKillPlayerAttachments( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] int				m_nPlayer;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEKillPlayerAttachments::CTEKillPlayerAttachments( void )
{
	m_nPlayer = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEKillPlayerAttachments::~CTEKillPlayerAttachments( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEKillPlayerAttachments::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEKillPlayerAttachments::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	tempents->KillAttachedTents( m_nPlayer );
}

void TE_KillPlayerAttachments( IRecipientFilter& filter, float delay,
	int player )
{
	tempents->KillAttachedTents( player );
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEKillPlayerAttachments, DT_TEKillPlayerAttachments, CTEKillPlayerAttachments )

class SimpleParticle;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Large Funnel TE
//-----------------------------------------------------------------------------
class CTELargeFunnel : public CTEParticleSystem
{
	[[= ks::reflect::NetTable{ .name = "DT_TELargeFunnel" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTELargeFunnel, CTEParticleSystem );
	DECLARE_CLIENTCLASS();

					CTELargeFunnel( void );
	virtual			~CTELargeFunnel( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	void			CreateFunnel( void );

	[[= ks::reflect::Net{} ]] int				m_nModelIndex;
	[[= ks::reflect::Net{} ]] int				m_nReversed;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTELargeFunnel::CTELargeFunnel( void )
{
	m_nModelIndex = 0;
	m_nReversed = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTELargeFunnel::~CTELargeFunnel( void )
{
}

void CTELargeFunnel::CreateFunnel( void )
{
	ks::intrusive_ptr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "TELargeFunnel" );
	pSimple->SetSortOrigin( m_vecOrigin );

	int			i, j;
	SimpleParticle *pParticle;

	Vector		vecDir;
	Vector		vecDest;

	float ratio = 0.25;
	float invratio = 1 / ratio;

	PMaterialHandle hMaterial = pSimple->GetPMaterial( "sprites/flare6" );

	for ( i = -256 ; i <= 256 ; i += 24 )	//24 from 32.. little more dense
	{
		for ( j = -256 ; j <= 256 ; j += 24 )
		{
			pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial, m_vecOrigin );			
			if( pParticle )
			{
				if ( m_nReversed )
				{
					pParticle->m_Pos = m_vecOrigin;

					vecDir[0] = i;
					vecDir[1] = j;
					vecDir[2] = random_valve->RandomFloat(100, 800);

					pParticle->m_uchStartAlpha	= 255;
					pParticle->m_uchEndAlpha	= 0;
				}
				else
				{
					pParticle->m_Pos[0] = m_vecOrigin[0] + i;
					pParticle->m_Pos[1] = m_vecOrigin[1] + j;
					pParticle->m_Pos[2] = m_vecOrigin[2] + random_valve->RandomFloat(100, 800);

					// send particle heading to org at a random speed
					vecDir = m_vecOrigin - pParticle->m_Pos;

					pParticle->m_uchStartAlpha	= 0;
					pParticle->m_uchEndAlpha	= 255;
				}

				vecDir *= ratio;

				pParticle->m_vecVelocity = vecDir;			

				pParticle->m_flLifetime = 0;
				pParticle->m_flDieTime = invratio;	

				if( random_valve->RandomInt( 0, 10 ) < 5 )
				{
					// small green particle
					pParticle->m_uchColor[0] = 0;
					pParticle->m_uchColor[1] = 255;
					pParticle->m_uchColor[2] = 0;
				
					pParticle->m_uchStartSize	= 4.0;
				}
				else
				{
					// large white particle
					pParticle->m_uchColor[0] = 255;
					pParticle->m_uchColor[1] = 255;
					pParticle->m_uchColor[2] = 255;
				
					pParticle->m_uchStartSize	= 15.0;
				}

				pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
				pParticle->m_flRoll			= i;	// pseudorandom
				pParticle->m_flRollDelta	= 0;
				pParticle->m_iFlags = 0;
			}

		}
	}

	return;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTELargeFunnel::PostDataUpdate( DataUpdateType_t updateType )
{
	CreateFunnel();
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTELargeFunnel, DT_TELargeFunnel, CTELargeFunnel )

void TE_LargeFunnel( IRecipientFilter& filter, float delay,
	const Vector* pos, int modelindex, int reversed )
{
	// Major hack to simulate receiving network message
	__g_CTELargeFunnel.m_vecOrigin = *pos;
	__g_CTELargeFunnel.m_nModelIndex = modelindex;
	__g_CTELargeFunnel.m_nReversed = reversed;

	__g_CTELargeFunnel.PostDataUpdate( DATA_UPDATE_CREATED );
}
