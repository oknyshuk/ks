// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <time.h>
#include "irecipientfilter.h"
#include "networkvar.h"
#include "shattersurfacetypes.h"
#include "reflect_annotations.h"
#include "server_class.h"
#include "reflect_sendtable.h"
#include "eiface.h"
#include "worldsize.h"
#include "declare_class.h"
#include "edict.h"
#include "responserules/response_types.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "util.h"
module ks.server;

import ks.core.basetypes;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.exprevaluator;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.reflect_wirevar;
import :vscript;
import :sharedvar;
import :baseentity;
import :player;
import :fish;
import :nav_4;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.math.mathlib;
import :entity_2;
import std;
import ks.game.tempentity;
import ks.core.wchartypes;
import ks.game.cstrike15.playerdecals_signature;
import ks.tier1.vector_ops;
import :cstrike15.cs_player;
import :entity_dispatch;
import :entity_kind;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_playeranimstate;
import :cstrike15.cs_player_2;
import :cstrike15.hostage.cs_simple_hostage;

class CBaseEntity;
class CBasePlayer;
class CRecipientFilter;

using namespace ::ks::reflect;

// External definitions
void TE_ArmorRicochet( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* dir );
void TE_BeamEntPoint( IRecipientFilter& filter, float delay,
	int	nStartEntity, const Vector *start, int nEndEntity, const Vector* end, int modelindex, int haloindex, int startframe, int framerate,
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
void TE_BloodSprite( IRecipientFilter& filter, float delay,
	const Vector* org, const Vector *dir, int r, int g, int b, int a, int size );
void TE_BreakModel( IRecipientFilter& filter, float delay,
	const Vector& pos, const QAngle &angles, const Vector& size, const Vector& vel, 
	int modelindex, int randomization, int count, float time, int flags );
void TE_BSPDecal( IRecipientFilter& filter, float delay,
	const Vector* pos, int entity, int index );
void TE_ProjectDecal( IRecipientFilter& filer, float delay,
	const Vector* pos, const QAngle *angles, float distance, int index );
void TE_Bubbles( IRecipientFilter& filter, float delay,
	const Vector* mins, const Vector* maxs, float height, int modelindex, int count, float speed );
void TE_BubbleTrail( IRecipientFilter& filter, float delay,
	const Vector* mins, const Vector* maxs, float height, int modelindex, int count, float speed );
void TE_Decal( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* start, int entity, int hitbox, int index );
void TE_DynamicLight( IRecipientFilter& filter, float delay,
	const Vector* org, int r, int g, int b, int exponent, float radius, float time, float decay );
void TE_Explosion( IRecipientFilter& filter, float delay,
	const Vector& pos, int modelindex, float scale, int framerate, int flags, int radius, int magnitude, const Vector& normal, unsigned char materialType = 'C' );
void TE_ShatterSurface( IRecipientFilter& filter, float delay,
	const Vector* pos, const QAngle* angle, const Vector* vForce, const Vector* vForcePos, 
	float width, float height, float shardsize, ShatterSurface_t surfacetype,
	int front_r, int front_g, int front_b, int back_r, int back_g, int back_b);
void TE_GlowSprite( IRecipientFilter& filter, float delay,
	const Vector* pos, int modelindex, float life, float size, int brightness );
void TE_FootprintDecal( IRecipientFilter& filter, float delay, const Vector *origin, const Vector* right, 
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
	const Vector* pos, const Vector* start, const Vector* right, int player, int entity, int hitbox );
void TE_ShowLine( IRecipientFilter& filter, float delay,
	const Vector* start, const Vector* end );
void TE_Smoke( IRecipientFilter& filter, float delay,
	const Vector* pos, int modelindex, float scale, int framerate );
void TE_Sparks( IRecipientFilter& filter, float delay,
	const Vector* pos, int nMagnitude, int nTrailLength, const Vector *pDir );
void TE_Sprite( IRecipientFilter& filter, float delay,
	const Vector* pos, int modelindex, float size, int brightness );
void TE_SpriteSpray( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* dir, int modelindex, int speed, float noise, int count );
void TE_WorldDecal( IRecipientFilter& filter, float delay,
	const Vector* pos, int index );
void TE_MuzzleFlash( IRecipientFilter& filter, float delayt,
	const Vector &start, const QAngle &angles, float scale, int type );
void TE_Dust( IRecipientFilter& filter, float delayt,
			 const Vector &pos, const Vector &dir, float size, float speed );
void TE_PhysicsProp( IRecipientFilter& filter, float delay,
	int modelindex, int skin, const Vector& pos, const QAngle &angles, const Vector& vel, int flags, int effects, color24 color );
void TE_ClientProjectile( IRecipientFilter& filter, float delay,
	 const Vector* vecOrigin, const Vector* vecVelocity, int modelindex, int lifetime, CBaseEntity *pOwner );

class CTempEntsSystem : public ITempEntsSystem
{
	//-----------------------------------------------------------------------------
	// Purpose: Returning true means don't even call TE func
	// Input  : filter - 
	//			*suppress_host - 
	// Output : static bool
	//-----------------------------------------------------------------------------
	bool SuppressTE( IRecipientFilter& filter )
	{
		if ( GetSuppressHost() )
		{
			CRecipientFilter& _filter = (( CRecipientFilter & )filter);

			if ( !_filter.IgnorePredictionCull() )
			{
				_filter.RemoveRecipient( (CBasePlayer *)GetSuppressHost()  );
			}

			if ( !_filter.GetRecipientCount() )
			{
				// Suppress it
				return true;
			}
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
			TE_BeamEntPoint( filter, delay, nStartEntity, pStart, nEndEntity, pEnd, modelindex, haloindex, startframe, framerate,
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
		float life, float width, int spread, float amplitude, int r, int g, int b, int a, int speed, int flags )
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
		float life, float width, int spread, float amplitude, int r, int g, int b, int a, int speed, int flags )
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
		const Vector& pos, const QAngle &angle, const Vector& size, const Vector& vel, 
		int modelindex, int randomization, int count, float time, int flags )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_BreakModel( filter, delay, pos, angle, size, vel, modelindex, randomization, count, time, flags );
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
			TE_Explosion( filter, delay, pos, modelindex, scale, framerate, flags, radius, magnitude, normal, materialType );
		}
	}
	virtual void ShatterSurface( IRecipientFilter& filter, float delay,
		const Vector* pos, const QAngle* angle, const Vector* vForce, const Vector* vForcePos, 
		float width, float height, float shardsize, ShatterSurface_t surfacetype,
		int front_r, int front_g, int front_b, int back_r, int back_g, int back_b)
	{
		if ( !SuppressTE( filter ) )
		{
			TE_ShatterSurface( filter, delay, pos, angle, vForce, vForcePos, width, height, shardsize, surfacetype,
				 front_r, front_g, front_b, back_r, back_g, back_b );
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
	virtual void FootprintDecal( IRecipientFilter& filter, float delay, const Vector *origin, const Vector* right, 
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
		const Vector* pos, const Vector *start, const Vector* right, int player, int entity, int hitbox )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_PlayerDecal( filter, delay,
				pos, start, right, player, entity, hitbox );
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
	}

	virtual void PhysicsProp( IRecipientFilter& filter, float delay, int modelindex, int skin,
		const Vector& pos, const QAngle &angles, const Vector& vel, int flags, int effects, color24 renderColor )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_PhysicsProp( filter, delay, modelindex, skin, pos, angles, vel, flags, effects, renderColor );
		}
	}

	// For playback from external tools
	virtual void TriggerTempEntity( KeyValues *pKeyValues )
	{
		contract_assert( 0 );
	}

	virtual void ClientProjectile( IRecipientFilter& filter, float delay,
		const Vector* vecOrigin, const Vector* vecVelocity, int modelindex, int lifetime, CBaseEntity *pOwner )
	{
		if ( !SuppressTE( filter ) )
		{
			TE_ClientProjectile( filter, delay, vecOrigin, vecVelocity, modelindex, lifetime, pOwner );
		}
	}
};

static CTempEntsSystem g_TESystem;
// Expose to rest of engine
ITempEntsSystem *te = &g_TESystem;

class CBaseTempEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Dispatches sparks
//-----------------------------------------------------------------------------
class CTEMetalSparks : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEMetalSparks", .base = false } ]]
	static constexpr bool ks_class = true;

DECLARE_CLASS( CTEMetalSparks, CBaseTempEntity );

public:
					CTEMetalSparks( const char *name );
	virtual			~CTEMetalSparks( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );

	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecPos, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecDir, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEMetalSparks::CTEMetalSparks( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecPos.Init();
	m_vecDir.Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEMetalSparks::~CTEMetalSparks( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEMetalSparks::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_vecPos = current_origin;
	
	AngleVectors( current_angles, &m_vecDir.GetForModify() );
	
	Vector forward;

	m_vecPos += Vector( 0, 0, 24 );

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecPos, 100.0, forward, m_vecPos.GetForModify() );
	
	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEMetalSparks, DT_TEMetalSparks )

// Singleton to fire TEMetalSparks objects
static CTEMetalSparks g_TEMetalSparks( "Metal Sparks" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*pos - 
//			scale - 
//-----------------------------------------------------------------------------
void TE_MetalSparks( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* dir )
{
	g_TEMetalSparks.m_vecPos = *pos;
	g_TEMetalSparks.m_vecDir = *dir;

	contract_assert( dir->Length() < 1.01 ); // make sure it's a normal

	// Send it over the wire
	g_TEMetalSparks.Create( filter, delay );
}

class CTEArmorRicochet : public CTEMetalSparks
{
	[[= ks::reflect::NetTable{ .name = "DT_TEArmorRicochet" } ]]
	static constexpr bool ks_class = true;

DECLARE_CLASS( CTEArmorRicochet, CTEMetalSparks );

public:
	CTEArmorRicochet( const char *name ) : CTEMetalSparks(name) {}
	DECLARE_SERVERCLASS();
};

IMPLEMENT_REFLECT_SERVERCLASS( CTEArmorRicochet, DT_TEArmorRicochet )

static CTEArmorRicochet g_TEArmorRicochet( "Armor Ricochet" );
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*pos - 
//			scale - 
//-----------------------------------------------------------------------------
void TE_ArmorRicochet( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* dir )
{
	g_TEArmorRicochet.m_vecPos = *pos;
	g_TEArmorRicochet.m_vecDir = *dir;

	// Send it over the wire
	g_TEArmorRicochet.Create( filter, delay );
}

class CBaseTempEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBaseBeam::CTEBaseBeam( const char *name ) :
  CBaseTempEntity( name )
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

IMPLEMENT_REFLECT_SERVERCLASS( CTEBaseBeam, DT_BaseBeam )

using namespace ::ks::reflect;

extern int	g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud

//-----------------------------------------------------------------------------
// Purpose: Dispatches a beam ring between two entities
//-----------------------------------------------------------------------------
class CTEBeamEntPoint : public CTEBaseBeam
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBeamEntPoint" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBeamEntPoint, CTEBaseBeam );
	DECLARE_SERVERCLASS();

					CTEBeamEntPoint( const char *name );
	virtual			~CTEBeamEntPoint( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
public:
	CNetworkVar( int, m_nStartEntity, [[= ks::reflect::Net{ .bits = 24, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVector( m_vecStartPoint, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( int, m_nEndEntity, [[= ks::reflect::Net{ .bits = 24, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVector( m_vecEndPoint, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBeamEntPoint::CTEBeamEntPoint( const char *name ) :
	CTEBaseBeam( name )
{
	m_nStartEntity	= 0;
	m_nEndEntity	= 0;
	m_vecStartPoint.Init();
	m_vecEndPoint.Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamEntPoint::~CTEBeamEntPoint( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEBeamEntPoint::Test( const Vector& current_origin, const QAngle& current_angles )
{
	m_nStartEntity	= 1;

	m_nModelIndex	= g_sModelIndexSmoke;
	m_nStartFrame	= 0;
	m_nFrameRate	= 10;
	m_fLife			= 2.0;
	m_fWidth		= 1.0;
	m_fAmplitude	= 1.0;
	r				= 0;
	g				= 63;
	b				= 127;
	a				= 150;
	m_nSpeed		= 1;

	m_vecEndPoint = current_origin;
	
	Vector forward, right;

	m_vecEndPoint += Vector( 0, 0, 24 );

	AngleVectors( current_angles, &forward, &right, 0 );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecEndPoint, 50.0, forward, m_vecEndPoint.GetForModify() );

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEBeamEntPoint, DT_TEBeamEntPoint )

// Singleton to fire TEBeamEntPoint objects
static CTEBeamEntPoint g_TEBeamEntPoint( "BeamEntPoint" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//				int	start - 
//			*end - 
//			modelindex - 
//			startframe - 
//			framerate - 
//			msg_dest - 
//			delay - 
//			origin - 
//			recipient - 
//-----------------------------------------------------------------------------
void TE_BeamEntPoint( IRecipientFilter& filter, float delay,
	int	nStartEntity, const Vector *start, int nEndEntity, const Vector* end, 
	int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, float endWidth, int fadeLength, float amplitude, int r, int g, int b, int a, int speed )
{
	g_TEBeamEntPoint.m_nStartEntity = (nStartEntity > 0) ? (nStartEntity & 0x0FFF) | ((1 & 0xF)<<12) : 0;
	g_TEBeamEntPoint.m_nEndEntity	= (nEndEntity > 0) ? (nEndEntity & 0x0FFF) | ((1 & 0xF)<<12) : 0;
	g_TEBeamEntPoint.m_vecStartPoint = start ? *start : vec3_origin;
	g_TEBeamEntPoint.m_vecEndPoint	= end ? *end : vec3_origin;
	g_TEBeamEntPoint.m_nModelIndex	= modelindex;
	g_TEBeamEntPoint.m_nHaloIndex	= haloindex;
	g_TEBeamEntPoint.m_nStartFrame	= startframe;
	g_TEBeamEntPoint.m_nFrameRate	= framerate;
	g_TEBeamEntPoint.m_fLife		= life;
	g_TEBeamEntPoint.m_fWidth		= width;
	g_TEBeamEntPoint.m_fEndWidth	= endWidth;
	g_TEBeamEntPoint.m_nFadeLength	= fadeLength;
	g_TEBeamEntPoint.m_fAmplitude	= amplitude;
	g_TEBeamEntPoint.m_nSpeed		= speed;
	g_TEBeamEntPoint.r				= r;
	g_TEBeamEntPoint.g				= g;
	g_TEBeamEntPoint.b				= b;
	g_TEBeamEntPoint.a				= a;

	// Send it over the wire
	g_TEBeamEntPoint.Create( filter, delay );
}

using namespace ::ks::reflect;

extern int	g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud

//-----------------------------------------------------------------------------
// Purpose: Dispatches a beam between two entities
//-----------------------------------------------------------------------------
class CTEBeamEnts : public CTEBaseBeam
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBeamEnts" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBeamEnts, CTEBaseBeam );
	DECLARE_SERVERCLASS();

					CTEBeamEnts( const char *name );
	virtual			~CTEBeamEnts( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );

public:
	CNetworkVar( int, m_nStartEntity, [[= ks::reflect::Net{ .bits = 24, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nEndEntity, [[= ks::reflect::Net{ .bits = 24, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBeamEnts::CTEBeamEnts( const char *name ) :
	CTEBaseBeam( name )
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

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEBeamEnts::Test( const Vector& current_origin, const QAngle& current_angles )
{
	m_nStartEntity	= 1;
	m_nEndEntity	= 0;

	m_nModelIndex	= g_sModelIndexSmoke;
	m_nStartFrame	= 0;
	m_nFrameRate	= 10;
	m_fLife			= 2.0;
	m_fWidth		= 1.0;
	m_fAmplitude	= 1;
	r				= 127;
	g				= 63;
	b				= 0;
	a				= 150;
	m_nSpeed		= 1;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEBeamEnts, DT_TEBeamEnts )

// Singleton to fire TEBeamEnts objects
static CTEBeamEnts g_TEBeamEnts( "BeamEnts" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//				int	start - 
//			end - 
//			modelindex - 
//			startframe - 
//			framerate - 
//			msg_dest - 
//			delay - 
//			origin - 
//			recipient - 
//-----------------------------------------------------------------------------
void TE_BeamEnts( IRecipientFilter& filter, float delay,
	int	start, int end, int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, float endWidth, int fadeLength, float amplitude, int r, int g, int b, int a, int speed )
{
	g_TEBeamEnts.m_nStartEntity = (start & 0x0FFF) | ((1 & 0xF)<<12);
	g_TEBeamEnts.m_nEndEntity	= (end & 0x0FFF) | ((1 & 0xF)<<12);
	g_TEBeamEnts.m_nModelIndex	= modelindex;
	g_TEBeamEnts.m_nHaloIndex	= haloindex;
	g_TEBeamEnts.m_nStartFrame	= startframe;
	g_TEBeamEnts.m_nFrameRate	= framerate;
	g_TEBeamEnts.m_fLife		= life;
	g_TEBeamEnts.m_fWidth		= width;
	g_TEBeamEnts.m_fEndWidth	= endWidth;
	g_TEBeamEnts.m_nFadeLength	= fadeLength;
	g_TEBeamEnts.m_fAmplitude	= amplitude;
	g_TEBeamEnts.m_nSpeed		= speed;
	g_TEBeamEnts.r				= r;
	g_TEBeamEnts.g				= g;
	g_TEBeamEnts.b				= b;
	g_TEBeamEnts.a				= a;

	// Send it over the wire
	g_TEBeamEnts.Create( filter, delay );
}

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Dispatches a beam ring between two entities
//-----------------------------------------------------------------------------
class CTEBeamFollow : public CTEBaseBeam
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBeamFollow" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CTEBeamFollow, CTEBaseBeam );
public:

	DECLARE_SERVERCLASS();

					CTEBeamFollow( const char *name );
	virtual			~CTEBeamFollow( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );

public:

	CNetworkVar( int, m_iEntIndex, [[= ks::reflect::Net{ .bits = 24, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBeamFollow::CTEBeamFollow( const char *name ) :
	CTEBaseBeam( name )
{
	m_iEntIndex = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamFollow::~CTEBeamFollow( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEBeamFollow::Test( const Vector& current_origin, const QAngle& current_angles )
{
	m_iEntIndex	= 1;
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEBeamFollow, DT_TEBeamFollow )

// Singleton to fire TEBeamEntPoint objects
static CTEBeamFollow g_TEBeamFollow( "BeamFollow" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : filter - 
//			delay - 
//			iEntIndex - 
//			modelIndex - 
//			modelindex - 
//			haloIndex - 
//			life - 
//			width - 
//			endWidth - 
//			fadeLength - 
//			r - 
//			g - 
//			b - 
//			a - 
//-----------------------------------------------------------------------------
void TE_BeamFollow( IRecipientFilter& filter, float delay,
	int iEntIndex, int modelIndex, int haloIndex, float life, float width, float endWidth, 
	float fadeLength,float r, float g, float b, float a )
{
	g_TEBeamFollow.m_iEntIndex		= (iEntIndex & 0x0FFF) | ((1 & 0xF)<<12);
	g_TEBeamFollow.m_nModelIndex	= modelIndex;
	g_TEBeamFollow.m_nHaloIndex		= haloIndex;
	g_TEBeamFollow.m_nStartFrame	= 0;
	g_TEBeamFollow.m_nFrameRate		= 0;
	g_TEBeamFollow.m_fLife			= life;
	g_TEBeamFollow.m_fWidth			= width;
	g_TEBeamFollow.m_fEndWidth		= endWidth;
	g_TEBeamFollow.m_nFadeLength	= fadeLength;
	g_TEBeamFollow.r				= r;
	g_TEBeamFollow.g				= g;
	g_TEBeamFollow.b				= b;
	g_TEBeamFollow.a				= a;

	// Send it over the wire
	g_TEBeamFollow.Create( filter, delay );
}

using namespace ::ks::reflect;

// Purpose: Beam used for Laser sights. Fades out when it's perpendicular to the viewpoint.

extern int	g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud

//-----------------------------------------------------------------------------
// Purpose: Beam used for Laser sights. Fades out when it's perpendicular to the viewpoint.
//-----------------------------------------------------------------------------
class CTEBeamLaser : public CTEBaseBeam
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBeamLaser" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CTEBeamLaser, CTEBaseBeam );
public:
	DECLARE_SERVERCLASS();

					CTEBeamLaser( const char *name );
	virtual			~CTEBeamLaser( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
public:
	CNetworkVar( int, m_nStartEntity, [[= ks::reflect::Net{ .bits = 24, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nEndEntity, [[= ks::reflect::Net{ .bits = 24, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBeamLaser::CTEBeamLaser( const char *name ) :
	CTEBaseBeam( name )
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

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEBeamLaser::Test( const Vector& current_origin, const QAngle& current_angles )
{
	m_nStartEntity	= 1;
	m_nEndEntity	= 0;

	m_nModelIndex	= g_sModelIndexSmoke;
	m_nStartFrame	= 0;
	m_nFrameRate	= 10;
	m_fLife			= 2.0;
	m_fWidth		= 1.0;
	m_fAmplitude	= 1.0;
	r				= 127;
	g				= 63;
	b				= 0;
	a				= 150;
	m_nSpeed		= 1;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEBeamLaser, DT_TEBeamLaser )

// Singleton to fire TEBeamLaser objects
static CTEBeamLaser g_TEBeamLaser( "BeamLaser" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*start - 
//			*end - 
//			modelindex - 
//			startframe - 
//			framerate - 
//			msg_dest - 
//			delay - 
//			origin - 
//			recipient - 
//-----------------------------------------------------------------------------
void TE_BeamLaser( IRecipientFilter& filter, float delay,
	int	start, int end, int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, float endWidth, int fadeLength, float amplitude, int r, int g, int b, int a, int speed )
{
	g_TEBeamLaser.m_nStartEntity = (start & 0x0FFF) | ((1 & 0xF)<<12);
	g_TEBeamLaser.m_nEndEntity	= (end & 0x0FFF) | ((1 & 0xF)<<12);
	g_TEBeamLaser.m_nModelIndex	= modelindex;
	g_TEBeamLaser.m_nHaloIndex	= haloindex;
	g_TEBeamLaser.m_nStartFrame	= startframe;
	g_TEBeamLaser.m_nFrameRate	= framerate;
	g_TEBeamLaser.m_fLife		= life;
	g_TEBeamLaser.m_fWidth		= width;
	g_TEBeamLaser.m_fEndWidth	= endWidth;
	g_TEBeamLaser.m_nFadeLength	= fadeLength;
	g_TEBeamLaser.m_fAmplitude	= amplitude;
	g_TEBeamLaser.m_nSpeed		= speed;
	g_TEBeamLaser.r				= r;
	g_TEBeamLaser.g				= g;
	g_TEBeamLaser.b				= b;
	g_TEBeamLaser.a				= a;

	// Send it over the wire
	g_TEBeamLaser.Create( filter, delay );
}

using namespace ::ks::reflect;

extern int	g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud

//-----------------------------------------------------------------------------
// Purpose: Dispatches a beam ring between two entities
//-----------------------------------------------------------------------------
class CTEBeamPoints : public CTEBaseBeam
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBeamPoints" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBeamPoints, CTEBaseBeam );
	DECLARE_SERVERCLASS();

					CTEBeamPoints( const char *name );
	virtual			~CTEBeamPoints( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
public:
	CNetworkVector( m_vecStartPoint, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecEndPoint, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBeamPoints::CTEBeamPoints( const char *name ) :
	CTEBaseBeam( name )
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

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEBeamPoints::Test( const Vector& current_origin, const QAngle& current_angles )
{
	m_nModelIndex	= g_sModelIndexSmoke;
	m_nStartFrame	= 0;
	m_nFrameRate	= 10;
	m_fLife			= 2.0;
	m_fWidth		= 1.0;
	m_fAmplitude	= 1;
	r				= 0;
	g				= 63;
	b				= 127;
	a				= 150;
	m_nSpeed		= 1;
	
	m_vecStartPoint = current_origin;
	
	Vector forward, right;

	m_vecStartPoint += Vector( 0, 0, 30 );

	AngleVectors( current_angles, &forward, &right, 0 );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecStartPoint, 75.0, forward, m_vecStartPoint.GetForModify() );
	VectorMA( m_vecStartPoint, 25.0, right, m_vecEndPoint.GetForModify() );
	VectorMA( m_vecStartPoint, -25.0, right, m_vecStartPoint.GetForModify() );

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEBeamPoints, DT_TEBeamPoints )

// Singleton to fire TEBeamPoints objects
static CTEBeamPoints g_TEBeamPoints( "BeamPoints" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*start - 
//			*end - 
//			modelindex - 
//			startframe - 
//			framerate - 
//			msg_dest - 
//			delay - 
//			origin - 
//			recipient - 
//-----------------------------------------------------------------------------
void TE_BeamPoints( IRecipientFilter& filter, float delay,
	const Vector* start, const Vector* end, int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, float endWidth, int fadeLength, float amplitude, int r, int g, int b, int a, int speed )
{
	g_TEBeamPoints.m_vecStartPoint	= *start;
	g_TEBeamPoints.m_vecEndPoint	= *end;
	g_TEBeamPoints.m_nModelIndex	= modelindex;
	g_TEBeamPoints.m_nHaloIndex		= haloindex;
	g_TEBeamPoints.m_nStartFrame	= startframe;
	g_TEBeamPoints.m_nFrameRate		= framerate;
	g_TEBeamPoints.m_fLife			= life;
	g_TEBeamPoints.m_fWidth			= width;
	g_TEBeamPoints.m_fEndWidth		= endWidth;
	g_TEBeamPoints.m_nFadeLength	= fadeLength;
	g_TEBeamPoints.m_fAmplitude		= amplitude;
	g_TEBeamPoints.m_nSpeed			= speed;
	g_TEBeamPoints.r				= r;
	g_TEBeamPoints.g				= g;
	g_TEBeamPoints.b				= b;
	g_TEBeamPoints.a				= a;

	// Send it over the wire
	g_TEBeamPoints.Create( filter, delay );
}

using namespace ::ks::reflect;

extern int	g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud

//-----------------------------------------------------------------------------
// Purpose: Dispatches a beam ring between two entities
//-----------------------------------------------------------------------------
class CTEBeamRing : public CTEBaseBeam
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBeamRing" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBeamRing, CTEBaseBeam );
	DECLARE_SERVERCLASS();

					CTEBeamRing( const char *name );
	virtual			~CTEBeamRing( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
public:
	CNetworkVar( int, m_nStartEntity, [[= ks::reflect::Net{ .bits = MAX_EDICT_BITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nEndEntity, [[= ks::reflect::Net{ .bits = MAX_EDICT_BITS, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBeamRing::CTEBeamRing( const char *name ) :
	CTEBaseBeam( name )
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

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEBeamRing::Test( const Vector& current_origin, const QAngle& current_angles )
{
	m_nStartEntity	= 1;
	m_nEndEntity	= 0;

	m_nModelIndex	= g_sModelIndexSmoke;
	m_nStartFrame	= 0;
	m_nFrameRate	= 2;
	m_fLife			= 10.0;
	m_fWidth		= 2.0;
	m_fAmplitude	= 1;
	r				= 255;
	g				= 255;
	b				= 0;
	a				= 127;
	m_nSpeed		= 5;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEBeamRing, DT_TEBeamRing )

// Singleton to fire TEBeamRing objects
static CTEBeamRing g_TEBeamRing( "BeamRing" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//				int	start - 
//			end - 
//			modelindex - 
//			startframe - 
//			framerate - 
//			msg_dest - 
//			delay - 
//			origin - 
//			recipient - 
//-----------------------------------------------------------------------------
void TE_BeamRing( IRecipientFilter& filter, float delay,
	int	start, int end, int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, int spread, float amplitude, int r, int g, int b, int a, int speed, int flags )
{
	g_TEBeamRing.m_nStartEntity = (start & 0x0FFF) | ((1 & 0xF)<<12);
	g_TEBeamRing.m_nEndEntity	= (end & 0x0FFF) | ((1 & 0xF)<<12);
	g_TEBeamRing.m_nModelIndex	= modelindex;
	g_TEBeamRing.m_nHaloIndex	= haloindex;
	g_TEBeamRing.m_nStartFrame	= startframe;
	g_TEBeamRing.m_nFrameRate	= framerate;
	g_TEBeamRing.m_fLife		= life;
	g_TEBeamRing.m_fWidth		= width;
	g_TEBeamRing.m_fEndWidth	= width;
	g_TEBeamRing.m_nFadeLength	= 0;
	g_TEBeamRing.m_fAmplitude	= amplitude;
	g_TEBeamRing.m_nSpeed		= speed;
	g_TEBeamRing.r				= r;
	g_TEBeamRing.g				= g;
	g_TEBeamRing.b				= b;
	g_TEBeamRing.a				= a;
	g_TEBeamRing.m_nFlags		= flags;

	// Send it over the wire
	g_TEBeamRing.Create( filter, delay );
}

using namespace ::ks::reflect;

extern int	g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud

//-----------------------------------------------------------------------------
// Purpose: Dispatches a beam ring between two entities
//-----------------------------------------------------------------------------
class CTEBeamRingPoint : public CTEBaseBeam
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBeamRingPoint" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBeamRingPoint, CTEBaseBeam );
	DECLARE_SERVERCLASS();

					CTEBeamRingPoint( const char *name );
	virtual			~CTEBeamRingPoint( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );

public:
	CNetworkVector( m_vecCenter, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( float, m_flStartRadius, [[= ks::reflect::Net{ .bits = 16, .low = 0.0f, .high = 4096.0f, .flags = SPROP_ROUNDUP } ]] );
	CNetworkVar( float, m_flEndRadius, [[= ks::reflect::Net{ .bits = 16, .low = 0.0f, .high = 4096.0f, .flags = SPROP_ROUNDUP } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBeamRingPoint::CTEBeamRingPoint( const char *name ) :
	CTEBaseBeam( name )
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

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEBeamRingPoint::Test( const Vector& current_origin, const QAngle& current_angles )
{
	m_vecCenter		= current_origin;
	m_flEndRadius	= 256.0f;
	m_flStartRadius	= 16.0f;

	m_nModelIndex	= g_sModelIndexSmoke;
	m_nStartFrame	= 0;
	m_nFrameRate	= 2;
	m_fLife			= 10.0;
	m_fWidth		= 2.0;
	m_fAmplitude	= 1;
	r				= 255;
	g				= 255;
	b				= 0;
	a				= 127;
	m_nSpeed		= 5;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEBeamRingPoint, DT_TEBeamRingPoint )

// Singleton to fire TEBeamRingPoint objects
static CTEBeamRingPoint g_TEBeamRingPoint( "BeamRingPoint" );

void TE_BeamRingPoint( IRecipientFilter& filter, float delay,
	const Vector& center, float start_radius, float end_radius, int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, int spread, float amplitude, int r, int g, int b, int a, int speed, int flags )
{
	g_TEBeamRingPoint.m_vecCenter	= center;
	g_TEBeamRingPoint.m_flStartRadius = start_radius;
	g_TEBeamRingPoint.m_flEndRadius	= end_radius;
	g_TEBeamRingPoint.m_nModelIndex	= modelindex;
	g_TEBeamRingPoint.m_nHaloIndex	= haloindex;
	g_TEBeamRingPoint.m_nStartFrame	= startframe;
	g_TEBeamRingPoint.m_nFrameRate	= framerate;
	g_TEBeamRingPoint.m_fLife		= life;
	g_TEBeamRingPoint.m_fWidth		= width;
	g_TEBeamRingPoint.m_fEndWidth	= width;
	g_TEBeamRingPoint.m_nFadeLength	= 0;
	g_TEBeamRingPoint.m_fAmplitude	= amplitude;
	g_TEBeamRingPoint.m_nSpeed		= speed;
	g_TEBeamRingPoint.r				= r;
	g_TEBeamRingPoint.g				= g;
	g_TEBeamRingPoint.b				= b;
	g_TEBeamRingPoint.a				= a;
	g_TEBeamRingPoint.m_nFlags		= flags;

	// Send it over the wire
	g_TEBeamRingPoint.Create( filter, delay );
}

class CBaseTempEntity;

using namespace ::ks::reflect;

static constexpr auto MAX_SPLINE_POINTS = 16;
//-----------------------------------------------------------------------------
// Purpose: Dispatches beam spline tempentity
//-----------------------------------------------------------------------------
class CTEBeamSpline : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBeamSpline", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBeamSpline, CBaseTempEntity );

					CTEBeamSpline( const char *name );
	virtual			~CTEBeamSpline( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkArray( Vector, m_vecPoints, MAX_SPLINE_POINTS,
	               [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .varlen = true } ]] );
	CNetworkVar( int, m_nPoints, [[= ks::reflect::Net{ .bits = 5, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBeamSpline::CTEBeamSpline( const char *name ) :
	CBaseTempEntity( name )
{
	int i;
	for ( i = 0; i < MAX_SPLINE_POINTS; i++ )
	{
		m_vecPoints.GetForModify( i ).Init();
	}
	m_nPoints = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamSpline::~CTEBeamSpline( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEBeamSpline::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_nPoints = 6;
	Vector m_vecStart = current_origin;
	
	Vector forward, right;

	m_vecStart[2] += 24;

	AngleVectors( current_angles, &forward, &right, 0 );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecStart, 100.0, forward, m_vecStart );

	VectorMA( m_vecStart, -128.0, right, m_vecStart );

	for ( int i = 0; i < m_nPoints; i++ )
	{
		m_vecPoints.Set( i, m_vecStart );
		VectorMA( m_vecStart, 128/m_nPoints, right, m_vecStart );
		VectorMA( m_vecStart, 30.0/m_nPoints, forward, m_vecStart );
	}

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEBeamSpline, DT_TEBeamSpline )

// Singleton to fire TEBeamSpline objects
static CTEBeamSpline g_TEBeamSpline( "BeamSpline" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			points - 
//			*points - 
//-----------------------------------------------------------------------------
void TE_BeamSpline( IRecipientFilter& filter, float delay,
	int points, Vector* rgPoints )
{
	int i;
	g_TEBeamSpline.m_nPoints = points;
	for ( i = 0; i < points; i++ )
	{
		g_TEBeamSpline.m_vecPoints.Set( i, rgPoints[ i ] );
	}
	
	for ( ; i < MAX_SPLINE_POINTS; i++ )
	{
		g_TEBeamSpline.m_vecPoints.GetForModify( i ).Init();
	}

	// Send it over the wire
	g_TEBeamSpline.Create( filter, delay );
}

class CBaseTempEntity;

using namespace ::ks::reflect;

extern int		g_sModelIndexBloodDrop;		// (in combatweapon.cpp) holds the sprite index for the initial blood
extern int		g_sModelIndexBloodSpray;	// (in combatweapon.cpp) holds the sprite index for splattered blood

//-----------------------------------------------------------------------------
// Purpose: Display's a blood sprite
//-----------------------------------------------------------------------------
class CTEBloodSprite : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBloodSprite", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBloodSprite, CBaseTempEntity );

					CTEBloodSprite( const char *name );
	virtual			~CTEBloodSprite( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );

	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecDirection, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( int, m_nSprayModel, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( int, m_nDropModel, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( int, r, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, g, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, b, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, a, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nSize, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBloodSprite::CTEBloodSprite( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_nSprayModel = 0;
	m_nDropModel = 0;
	r = 0;
	g = 0;
	b = 0;
	a = 0;
	m_nSize = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBloodSprite::~CTEBloodSprite( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEBloodSprite::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	r = 255;
	g = 255;
	b = 63;
	a = 255;
	m_nSize	= 16;
	m_vecOrigin = current_origin;
	
	m_nSprayModel = g_sModelIndexBloodSpray;
	m_nDropModel = g_sModelIndexBloodDrop;
	
	Vector forward;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin, 50.0, forward, m_vecOrigin.GetForModify() );

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEBloodSprite, DT_TEBloodSprite )

// Singleton
static CTEBloodSprite g_TEBloodSprite( "Blood Sprite" );

//-----------------------------------------------------------------------------
// Purpose: Public interface
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*org - 
//			r - 
//			g - 
//			b - 
//			a - 
//			size - 
//-----------------------------------------------------------------------------
void TE_BloodSprite( IRecipientFilter& filter, float delay,
	const Vector *org, const Vector *dir, int r, int g, int b, int a, int size )
{
	// Set up parameters
	g_TEBloodSprite.m_vecOrigin		= *org;
	g_TEBloodSprite.m_vecDirection	= *dir;
	g_TEBloodSprite.r = r;
	g_TEBloodSprite.g = g;
	g_TEBloodSprite.b = b;
	g_TEBloodSprite.a = a;
	g_TEBloodSprite.m_nSize = size;

	// Implicit
	g_TEBloodSprite.m_nSprayModel = g_sModelIndexBloodSpray;
	g_TEBloodSprite.m_nDropModel = g_sModelIndexBloodDrop;

	// Create it
	g_TEBloodSprite.Create( filter, delay );
}

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Dispatches blood stream tempentity
//-----------------------------------------------------------------------------
class CTEBloodStream : public CTEParticleSystem
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBloodStream" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBloodStream, CTEParticleSystem );

					CTEBloodStream( const char *name );
	virtual			~CTEBloodStream( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecDirection, [[= ks::reflect::Net{ .bits = 11, .low = -10.0, .high = 10.0, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( int, r, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, g, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, b, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, a, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nAmount, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBloodStream::CTEBloodStream( const char *name ) :
	BaseClass( name )
{
	m_vecDirection.Init();
	r = 0;
	g = 0;
	b = 0;
	a = 0;
	m_nAmount = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBloodStream::~CTEBloodStream( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEBloodStream::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	r = 247;
	g = 0;
	b = 0;
	a = 255;
	m_nAmount	= random_valve->RandomInt(50, 150);
	m_vecOrigin = current_origin;
	
	Vector forward;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;	
	VectorNormalize( forward );

	m_vecOrigin += forward * 50;

	m_vecDirection = UTIL_RandomBloodVector();

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEBloodStream, DT_TEBloodStream )

// Singleton to fire TEBloodStream objects
static CTEBloodStream g_TEBloodStream( "Blood Stream" );

//-----------------------------------------------------------------------------
// Purpose: Creates a blood stream
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*org - 
//			*dir - 
//			r - 
//			g - 
//			b - 
//			a - 
//			amount - 
//-----------------------------------------------------------------------------
void TE_BloodStream( IRecipientFilter& filter, float delay,
	const Vector* org, const Vector* dir, int r, int g, int b, int a, int amount )
{
	g_TEBloodStream.m_vecOrigin = *org;
	g_TEBloodStream.m_vecDirection = *dir;	
	g_TEBloodStream.r = r;
	g_TEBloodStream.g = g;
	g_TEBloodStream.b = b;
	g_TEBloodStream.a = a;
	g_TEBloodStream.m_nAmount = amount;

	// Send it over the wire
	g_TEBloodStream.Create( filter, delay );
}

class CBaseTempEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Dispatches model smash pieces
//-----------------------------------------------------------------------------
class CTEBreakModel : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBreakModel" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBreakModel, CBaseTempEntity );

					CTEBreakModel( const char *name );
	virtual			~CTEBreakModel( void );

	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecVelocity, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkQAngle( m_angRotation, [[= ks::reflect::Net{ .bits = 13, .index = 2 } ]]  [[= ks::reflect::Net{ .bits = 13, .index = 1 } ]]  [[= ks::reflect::Net{ .bits = 13, .index = 0 } ]] );
	CNetworkVar( int, m_nRandomization, [[= ks::reflect::Net{ .bits = 9, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( int, m_nCount, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_fTime, [[= ks::reflect::Net{ .bits = 10, .low = 0, .high = 102.4 } ]] );
	CNetworkVar( int, m_nFlags, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBreakModel::CTEBreakModel( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_vecSize.Init();
	m_vecVelocity.Init();
	m_angRotation.Init();
	m_nModelIndex		= 0;
	m_nRandomization	= 0;
	m_nCount			= 0;
	m_fTime				= 0.0;
	m_nFlags			= 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBreakModel::~CTEBreakModel( void )
{
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEBreakModel, DT_TEBreakModel )

// Singleton to fire TEBreakModel objects
static CTEBreakModel g_TEBreakModel( "breakmodel" );

void TE_BreakModel( IRecipientFilter& filter, float delay,
	const Vector& pos, const QAngle& angles, const Vector& size, const Vector& vel, int modelindex, int randomization,
	int count, float time, int flags )
{
	g_TEBreakModel.m_vecOrigin		= pos;
	g_TEBreakModel.m_angRotation	= angles;
	g_TEBreakModel.m_vecSize		= size;
	g_TEBreakModel.m_vecVelocity	= vel;
	g_TEBreakModel.m_nModelIndex	= modelindex;	
	g_TEBreakModel.m_nRandomization	= randomization;
	g_TEBreakModel.m_nCount			= count;
	g_TEBreakModel.m_fTime			= time;
	g_TEBreakModel.m_nFlags			= flags;

	// Send it over the wire
	g_TEBreakModel.Create( filter, delay );
}

class CBaseTempEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Dispatches BSP decal tempentity
//-----------------------------------------------------------------------------
class CTEBSPDecal : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBSPDecal" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBSPDecal, CBaseTempEntity );

					CTEBSPDecal( const char *name );
	virtual			~CTEBSPDecal( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( int, m_nEntity, [[= ks::reflect::Net{ .bits = MAX_EDICT_BITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nIndex, [[= ks::reflect::Net{ .bits = 9, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBSPDecal::CTEBSPDecal( const char *name ) :
	CBaseTempEntity( name )
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
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEBSPDecal::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_nEntity = 0;
	m_nIndex = 0;
	m_vecOrigin = current_origin;

	Vector vecEnd;
	
	Vector forward;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin, 50.0, forward, m_vecOrigin.GetForModify() );
	VectorMA( m_vecOrigin, 1024.0, forward, vecEnd );

	trace_t tr;

	UTIL_TraceLine( m_vecOrigin, vecEnd, MASK_SOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, &tr );

	m_vecOrigin = tr.endpos;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEBSPDecal, DT_TEBSPDecal )

// Singleton to fire TEBSPDecal objects
static CTEBSPDecal g_TEBSPDecal( "BSP Decal" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*pos - 
//			entity - 
//			index - 
//			modelindex - 
//-----------------------------------------------------------------------------
void TE_BSPDecal( IRecipientFilter& filter, float delay,
	const Vector* pos, int entity, int index )
{
	g_TEBSPDecal.m_vecOrigin	= *pos;
	g_TEBSPDecal.m_nEntity		= entity;	
	g_TEBSPDecal.m_nIndex		= index;

	// Send it over the wire
	g_TEBSPDecal.Create( filter, delay );
}

class CBaseTempEntity;

using namespace ::ks::reflect;

extern int	g_sModelIndexBubbles;// holds the index for the bubbles model

//-----------------------------------------------------------------------------
// Purpose: Dispatches bubbles
//-----------------------------------------------------------------------------
class CTEBubbles : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBubbles" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBubbles, CBaseTempEntity );

					CTEBubbles( const char *name );
	virtual			~CTEBubbles( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecMins, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecMaxs, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( float, m_fHeight, [[= ks::reflect::Net{ .bits = 17, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER } ]] );
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( int, m_nCount, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_fSpeed, [[= ks::reflect::Net{ .bits = 17, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBubbles::CTEBubbles( const char *name ) :
	CBaseTempEntity( name )
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

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEBubbles::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_vecMins = current_origin;
	
	Vector forward;

	m_vecMins.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecMins, 100.0, forward, m_vecMins.GetForModify() );

	m_vecMaxs = m_vecMins + Vector( 256, 256, 256 );

	m_fSpeed = 2;
	m_nCount = 50;
	m_fHeight = 256;

	m_nModelIndex = g_sModelIndexBubbles;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEBubbles, DT_TEBubbles )

// Singleton to fire TEBubbles objects
static CTEBubbles g_TEBubbles( "Bubbles" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*mins - 
//			*maxs - 
//			height - 
//			modelindex - 
//			count - 
//			speed - 
//-----------------------------------------------------------------------------
void TE_Bubbles( IRecipientFilter& filter, float delay,
	const Vector* mins, const Vector* maxs, float height, int modelindex, int count, float speed )
{
	g_TEBubbles.m_vecMins = *mins;
	g_TEBubbles.m_vecMaxs = *maxs;
	g_TEBubbles.m_fHeight = height;
	g_TEBubbles.m_nModelIndex = modelindex;
	g_TEBubbles.m_nCount = count;
	g_TEBubbles.m_fSpeed = speed;

	// Send it over the wire
	g_TEBubbles.Create( filter, delay );
}

class CBaseTempEntity;

using namespace ::ks::reflect;

extern int	g_sModelIndexBubbles;// holds the index for the bubbles model

enum
{
	BUBBLE_TRAIL_COUNT_BITS = 8,
	BUBBLE_TRAIL_MAX_COUNT = ( (1 << BUBBLE_TRAIL_COUNT_BITS) - 1 ),
};

//-----------------------------------------------------------------------------
// Purpose: Dispatches bubble trail
//-----------------------------------------------------------------------------
class CTEBubbleTrail : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEBubbleTrail" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBubbleTrail, CBaseTempEntity );

					CTEBubbleTrail( const char *name );
	virtual			~CTEBubbleTrail( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecMins, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecMaxs, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( float, m_flWaterZ, [[= ks::reflect::Net{ .bits = 17, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER } ]] );
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( int, m_nCount, [[= ks::reflect::Net{ .bits = BUBBLE_TRAIL_COUNT_BITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_fSpeed, [[= ks::reflect::Net{ .bits = 17, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBubbleTrail::CTEBubbleTrail( const char *name ) :
	CBaseTempEntity( name )
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

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEBubbleTrail::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_vecMins = current_origin;
	
	Vector forward;

	m_vecMins.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecMins, 100.0, forward, m_vecMins.GetForModify() );

	m_vecMaxs = m_vecMins + Vector( 256, 256, 256 );

	m_fSpeed = 8;
	m_nCount = 20;
	m_flWaterZ = 0;

	m_nModelIndex = g_sModelIndexBubbles;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEBubbleTrail, DT_TEBubbleTrail )

// Singleton to fire TEBubbleTrail objects
static CTEBubbleTrail g_TEBubbleTrail( "Bubble Trail" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*mins - 
//			*maxs - 
//			height - 
//			modelindex - 
//			count - 
//			speed - 
//-----------------------------------------------------------------------------
void TE_BubbleTrail( IRecipientFilter& filter, float delay,
	const Vector* mins, const Vector* maxs, float flWaterZ, int modelindex, int count, float speed )
{
	g_TEBubbleTrail.m_vecMins = *mins;
	g_TEBubbleTrail.m_vecMaxs = *maxs;
	g_TEBubbleTrail.m_flWaterZ = flWaterZ;
	g_TEBubbleTrail.m_nModelIndex = modelindex;
	g_TEBubbleTrail.m_nCount = std::min( count, ( int ) BUBBLE_TRAIL_MAX_COUNT );
	g_TEBubbleTrail.m_fSpeed = speed;

	// Send it over the wire
	g_TEBubbleTrail.Create( filter, delay );
}

class CBaseEntity;
class CBaseTempEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CTEClientProjectile : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEClientProjectile" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEClientProjectile, CBaseTempEntity );

					CTEClientProjectile( const char *name );
	virtual			~CTEClientProjectile( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecVelocity, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( int, m_nLifeTime, [[= ks::reflect::Net{ .bits = 6, .flags = SPROP_UNSIGNED } ]] );
	CNetworkHandle( CBaseEntity, m_hOwner, [[= ks::reflect::Net{} ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEClientProjectile::CTEClientProjectile( const char *name ) :
	CBaseTempEntity( name )
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

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEClientProjectile::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_vecOrigin = current_origin;
	
	Vector forward;
	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	m_vecVelocity = forward * 2048;

	m_nLifeTime = 5;
	m_hOwner = nullptr;
	
	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEClientProjectile, DT_TEClientProjectile )

// Singleton to fire TEClientProjectile objects
static CTEClientProjectile g_TEClientProjectile( "Client Projectile" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*mins - 
//			*maxs - 
//			height - 
//			modelindex - 
//			count - 
//			speed - 
//-----------------------------------------------------------------------------
void TE_ClientProjectile( IRecipientFilter& filter, float delay,
	const Vector* vecOrigin, const Vector* vecVelocity, int modelindex, int lifetime, CBaseEntity *pOwner )
{
	g_TEClientProjectile.m_vecOrigin = *vecOrigin;
	g_TEClientProjectile.m_vecVelocity = *vecVelocity;
	g_TEClientProjectile.m_nModelIndex = modelindex;
	g_TEClientProjectile.m_nLifeTime = lifetime;
	g_TEClientProjectile.m_hOwner = pOwner;

	// Send it over the wire
	g_TEClientProjectile.Create( filter, delay );
}

class CBaseTempEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Dispatches decal tempentity
//-----------------------------------------------------------------------------
class CTEDecal : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEDecal" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEDecal, CBaseTempEntity );

					CTEDecal( const char *name );
	virtual			~CTEDecal( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecStart, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( int, m_nEntity, [[= ks::reflect::Net{ .bits = MAX_EDICT_BITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nHitbox, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nIndex, [[= ks::reflect::Net{ .bits = 9, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEDecal::CTEDecal( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_nEntity = 0;
	m_nIndex = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEDecal::~CTEDecal( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEDecal::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_nEntity = 0;
	m_nIndex = 0;
	m_vecOrigin = current_origin;

	Vector vecEnd;
	
	Vector forward;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin, 50.0, forward, m_vecOrigin.GetForModify() );
	VectorMA( m_vecOrigin, 1024.0, forward, vecEnd );

	trace_t tr;

	UTIL_TraceLine( m_vecOrigin, vecEnd, MASK_SOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, &tr );

	m_vecOrigin = tr.endpos;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEDecal, DT_TEDecal )

// Singleton to fire TEDecal objects
static CTEDecal g_TEDecal( "Entity Decal" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*pos - 
//			entity - 
//			index - 
//-----------------------------------------------------------------------------
void TE_Decal( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* start, int entity, int hitbox, int index )
{
	contract_assert( pos && start );
	g_TEDecal.m_vecOrigin	= *pos;
	g_TEDecal.m_vecStart	= *start;
	g_TEDecal.m_nEntity		= entity;	
	g_TEDecal.m_nHitbox		= hitbox;
	g_TEDecal.m_nIndex		= index;

	// Send it over the wire
	g_TEDecal.Create( filter, delay );
}

class CBaseTempEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Displays a dynamic light
//-----------------------------------------------------------------------------
class CTEDynamicLight : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEDynamicLight" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEDynamicLight, CBaseTempEntity );

					CTEDynamicLight( const char *name );
	virtual			~CTEDynamicLight( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );

	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( float, m_fRadius, [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 2560.0, .flags = SPROP_ROUNDUP } ]] );
	CNetworkVar( int, r, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, g, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, b, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, exponent, [[= ks::reflect::Net{ .bits = 8 } ]] );
	CNetworkVar( float, m_fTime, [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 25.6, .flags = SPROP_ROUNDDOWN } ]] );
	CNetworkVar( float, m_fDecay, [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 2560.0, .flags = SPROP_ROUNDDOWN } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEDynamicLight::CTEDynamicLight( const char *name ) :
	CBaseTempEntity( name )
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

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEDynamicLight::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	r = 255;
	g = 255;
	b = 63;
	m_vecOrigin = current_origin;

	m_fRadius = 200;
	m_fTime = 2.0;
	m_fDecay = 0.0;
	
	Vector forward;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin, 50.0, forward, m_vecOrigin.GetForModify() );

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEDynamicLight, DT_TEDynamicLight )

// Singleton
static CTEDynamicLight g_TEDynamicLight( "Dynamic Light" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*org - 
//			r - 
//			g - 
//			b - 
//			radius - 
//			time - 
//			decay - 
//-----------------------------------------------------------------------------
void TE_DynamicLight( IRecipientFilter& filter, float delay,
	const Vector* org, int r, int g, int b, int exponent, float radius, float time, float decay )
{
	// Set up parameters
	g_TEDynamicLight.m_vecOrigin = *org;
	g_TEDynamicLight.r = r;
	g_TEDynamicLight.g = g;
	g_TEDynamicLight.b = b;
	g_TEDynamicLight.exponent = exponent;
	g_TEDynamicLight.m_fRadius = radius;
	g_TEDynamicLight.m_fTime = time;
	g_TEDynamicLight.m_fDecay = decay;

	// Create it
	g_TEDynamicLight.Create( filter, delay );
}

class CBaseTempEntity;
class CEffectData;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: This TE provides a simple interface to dispatch effects by name using DispatchEffect().
//-----------------------------------------------------------------------------
class CTEEffectDispatch : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEEffectDispatch" } ]]
	[[= ks::reflect::FromTable<"m_EffectData", &REFERENCE_SEND_TABLE( DT_EffectData )>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEEffectDispatch, CBaseTempEntity );

					CTEEffectDispatch( const char *name );
	virtual			~CTEEffectDispatch( void );

	DECLARE_SERVERCLASS();

public:
	CEffectData m_EffectData;
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEEffectDispatch::CTEEffectDispatch( const char *name ) :
	CBaseTempEntity( name )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEEffectDispatch::~CTEEffectDispatch( void )
{
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEEffectDispatch, DT_TEEffectDispatch )

// Singleton to fire TEEffectDispatch objects
static CTEEffectDispatch g_TEEffectDispatch( "EffectDispatch" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchEffect( const char *pName, const CEffectData &data )
{
	//Broadcast effects to all players. The potential for cheating
	//seems minimal on the effects that are sent through this,
	//and the overhead is minimal enough to make it worth sending to
	//all players.
	CReliableBroadcastRecipientFilter filter;
	filter.AddAllPlayers();

	if ( data.m_fFlags & EFFECTDATA_SERVER_IGNOREPREDICTIONCULL )
	{
		// remove prediction cull so it works on listen servers for the host
		filter.SetIgnorePredictionCull( true );
	}

	if ( !te->SuppressTE( filter ) )
	{
		// Copy the supplied effect data.
		g_TEEffectDispatch.m_EffectData = data;

		// Get the entry index in the string table.
		g_TEEffectDispatch.m_EffectData.m_iEffectName = GetEffectIndex( pName );

		// Send it to anyone who can see the effect's origin.
		g_TEEffectDispatch.Create( filter, 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchEffect( IRecipientFilter& filter, float flDelay, const char *pName, const CEffectData &data )
{
	if ( !te->SuppressTE( filter ) )
	{
		// Copy the supplied effect data.
		g_TEEffectDispatch.m_EffectData = data;

		// Get the entry index in the string table.
		g_TEEffectDispatch.m_EffectData.m_iEffectName = GetEffectIndex( pName );

		// Send it to anyone who can see the effect's origin.
		g_TEEffectDispatch.Create( filter, flDelay );
	}
}

class CBaseTempEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Dispatches energy splashes
//-----------------------------------------------------------------------------
class CTEEnergySplash : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEEnergySplash", .base = false } ]]
	static constexpr bool ks_class = true;

DECLARE_CLASS( CTEEnergySplash, CBaseTempEntity );

public:
					CTEEnergySplash( const char *name );
	virtual			~CTEEnergySplash( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecPos, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecDir, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( bool, m_bExplosive, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEEnergySplash::CTEEnergySplash( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecPos.Init();
	m_vecDir.Init();
	m_bExplosive = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEEnergySplash::~CTEEnergySplash( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEEnergySplash::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_vecPos = current_origin;
	
	AngleVectors( current_angles, &m_vecDir.GetForModify() );
	
	Vector forward;

	m_vecPos.GetForModify()[2] += 24;

	forward = m_vecDir;
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecPos, 100.0, forward, m_vecPos.GetForModify() );

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEEnergySplash, DT_TEEnergySplash )

// Singleton to fire TEEnergySplash objects
static CTEEnergySplash g_TEEnergySplash( "Energy Splash" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*pos - 
//			scale - 
//-----------------------------------------------------------------------------
void TE_EnergySplash( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* dir, bool bExplosive )
{
	g_TEEnergySplash.m_vecPos = *pos;
	g_TEEnergySplash.m_vecDir = *dir;
	g_TEEnergySplash.m_bExplosive = bExplosive;

	// Send it over the wire
	g_TEEnergySplash.Create( filter, delay );
}

using namespace ::ks::reflect;

extern int	g_sModelIndexFireball;			// (in combatweapon.cpp) holds the index for the smoke cloud

//-----------------------------------------------------------------------------
// Purpose: Dispatches explosion tempentity
//-----------------------------------------------------------------------------
class CTEExplosion : public CTEParticleSystem
{
	[[= ks::reflect::NetTable{ .name = "DT_TEExplosion" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEExplosion, CTEParticleSystem );
	DECLARE_SERVERCLASS();

					CTEExplosion( const char *name );
	virtual			~CTEExplosion( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
public:
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( float, m_fScale, [[= ks::reflect::Net{ .bits = 9, .low = 0.0, .high = 51.2 } ]] );
	CNetworkVar( int, m_nFrameRate, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nFlags, [[= ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVector( m_vecNormal, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( unsigned char, m_chMaterialType, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nRadius, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nMagnitude, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEExplosion::CTEExplosion( const char *name ) :
	BaseClass( name )
{
	m_nModelIndex = 0;
	m_fScale = 0;
	m_nFrameRate = 0;
	m_nFlags = 0;
	m_vecNormal.Init();
	m_chMaterialType = 'C';
	m_nRadius = 0;
	m_nMagnitude = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEExplosion::~CTEExplosion( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEExplosion::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_nModelIndex = g_sModelIndexFireball;
	m_fScale = 0.5;
	m_nFrameRate = 15;
	m_nFlags = TE_EXPLFLAG_NONE;
	m_vecOrigin = current_origin;
	
	Vector forward;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	m_vecOrigin += forward * 50;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEExplosion, DT_TEExplosion )

// Singleton to fire TEExplosion objects
static CTEExplosion g_TEExplosion( "Explosion" );

void TE_Explosion( IRecipientFilter& filter, float delay,
	const Vector& pos, int modelindex, float scale, int framerate, int flags, int radius, int magnitude, const Vector& normal, unsigned char materialType )
{
	g_TEExplosion.m_vecOrigin		= pos;
	g_TEExplosion.m_nModelIndex		= modelindex;	
	g_TEExplosion.m_fScale			= scale;
	g_TEExplosion.m_nFrameRate		= framerate;
	g_TEExplosion.m_nFlags			= flags;
	g_TEExplosion.m_nRadius			= radius;
	g_TEExplosion.m_nMagnitude		= magnitude;

	// Make a copy here so we can re-use it below
	g_TEExplosion.m_vecNormal		= normal; // pNormal ? ( *pNormal ) : Vector( 0, 0, 1 );

	g_TEExplosion.m_chMaterialType	= materialType;

	// Send it over the wire
	g_TEExplosion.Create( filter, delay );
}

class CBaseEntity;
class CBaseTempEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Dispatches Fizz tempentity
//-----------------------------------------------------------------------------
class CTEFizz : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEFizz" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEFizz, CBaseTempEntity );

					CTEFizz( const char *name );
	virtual			~CTEFizz( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );

	virtual void	Precache( void );

	DECLARE_SERVERCLASS();

public:
	CNetworkVar( int, m_nEntity, [[= ks::reflect::Net{ .bits = MAX_EDICT_BITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( int, m_nDensity, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nCurrent, [[= ks::reflect::Net{ .bits = 16 } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEFizz::CTEFizz( const char *name ) :
	CBaseTempEntity( name )
{
	m_nEntity = 0;
	m_nModelIndex = 0;
	m_nDensity = 0;
	m_nCurrent = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEFizz::~CTEFizz( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEFizz::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_nModelIndex = CBaseEntity::PrecacheModel( "sprites/bubble.vmt" );;
	m_nDensity = 200;
	m_nEntity = 1;
	m_nCurrent = 100;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEFizz::Precache( void )
{
	CBaseEntity::PrecacheModel( "sprites/bubble.vmt" );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEFizz, DT_TEFizz )

// Singleton to fire TEFizz objects
static CTEFizz g_TEFizz( "Fizz" );

void TE_Fizz( IRecipientFilter& filter, float delay,
	const CBaseEntity *entity, int modelindex, int density, int current )
{
	contract_assert( entity );

	g_TEFizz.m_nEntity		= ENTINDEX( (edict_t *)entity->edict() );
	g_TEFizz.m_nModelIndex	= modelindex;	
	g_TEFizz.m_nDensity		= density;
	g_TEFizz.m_nCurrent		= current;

	// Send it over the wire
	g_TEFizz.Create( filter, delay );
}

class CBaseTempEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Dispatches footprint decal tempentity
//-----------------------------------------------------------------------------

static constexpr auto FOOTPRINT_DECAY_TIME = 3.0f;

class CTEFootprintDecal : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEFootprintDecal" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEFootprintDecal, CBaseTempEntity );

					CTEFootprintDecal( const char *name );
	virtual			~CTEFootprintDecal( void );

	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecDirection, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );											
	CNetworkVar( int, m_nEntity, [[= ks::reflect::Net{ .bits = 11, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nIndex, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( unsigned char, m_chMaterialType, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
};

IMPLEMENT_REFLECT_SERVERCLASS( CTEFootprintDecal, DT_TEFootprintDecal )

// Singleton to fire TEFootprintDecal objects
static CTEFootprintDecal g_TEFootprintDecal( "Footprint Decal" );

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------

CTEFootprintDecal::CTEFootprintDecal( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_nEntity = 0;
	m_nIndex = 0;
	m_chMaterialType = 'C';
}

CTEFootprintDecal::~CTEFootprintDecal( void )
{
}

//-----------------------------------------------------------------------------
// places a footprint decal 
//-----------------------------------------------------------------------------
								    
void TE_FootprintDecal( IRecipientFilter& filter, float delay, 
					    const Vector *origin, const Vector *right, int entity, int index, 
					    unsigned char materialType )
{
	contract_assert( origin );
	g_TEFootprintDecal.m_vecOrigin = *origin;
	g_TEFootprintDecal.m_vecDirection	= *right;
	g_TEFootprintDecal.m_nEntity		= entity;	
	g_TEFootprintDecal.m_nIndex			= index;
	g_TEFootprintDecal.m_chMaterialType	= materialType;

	VectorNormalize(g_TEFootprintDecal.m_vecDirection.GetForModify());

	// Send it over the wire
	g_TEFootprintDecal.Create( filter, delay );
}

class CBaseTempEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Dispatches Glass Shatter tempentity
//-----------------------------------------------------------------------------
class CTEShatterSurface : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEShatterSurface" } ]]
	[[= ks::reflect::From<"m_uchFrontColor[0]", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED, .wire = "m_uchFrontColor[0]" }>{} ]]
	[[= ks::reflect::From<"m_uchFrontColor[1]", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED, .wire = "m_uchFrontColor[1]" }>{} ]]
	[[= ks::reflect::From<"m_uchFrontColor[2]", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED, .wire = "m_uchFrontColor[2]" }>{} ]]
	[[= ks::reflect::From<"m_uchBackColor[0]", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED, .wire = "m_uchBackColor[0]" }>{} ]]
	[[= ks::reflect::From<"m_uchBackColor[1]", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED, .wire = "m_uchBackColor[1]" }>{} ]]
	[[= ks::reflect::From<"m_uchBackColor[2]", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED, .wire = "m_uchBackColor[2]" }>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEShatterSurface, CBaseTempEntity );

	CTEShatterSurface( const char *name );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD } ]] );
	CNetworkQAngle( m_vecAngles, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecForce, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD } ]] );
	CNetworkVector( m_vecForcePos, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD } ]] );
	CNetworkVar( float, m_flWidth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flHeight, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flShardSize, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( int, m_nSurfaceType, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] );
	CNetworkArray( byte, m_uchFrontColor, 3 );
	CNetworkArray( byte, m_uchBackColor, 3 );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEShatterSurface::CTEShatterSurface( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_vecAngles.Init();
	m_vecForce.Init();
	m_vecForcePos.Init();
	m_flWidth			= 16.0f;
	m_flHeight			= 16.0f;
	m_flShardSize		= 3.0f;
	m_uchFrontColor.Set( 0, 255 );
	m_uchFrontColor.Set( 1, 255 );
	m_uchFrontColor.Set( 2, 255 );
	m_uchBackColor.Set( 0, 255 );
	m_uchBackColor.Set( 1, 255 );
	m_uchBackColor.Set( 2, 255 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEShatterSurface::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_vecOrigin = current_origin;

	Vector vecEnd;
	
	Vector forward;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin, 50.0, forward, m_vecOrigin.GetForModify() );
	VectorMA( m_vecOrigin, 1024.0, forward, vecEnd );

	trace_t tr;

	UTIL_TraceLine( m_vecOrigin, vecEnd, MASK_SOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, &tr );

	m_vecOrigin = tr.endpos;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEShatterSurface, DT_TEShatterSurface )

// Singleton to fire TEShatterSurface objects
static CTEShatterSurface g_TEShatterSurface( "Surface Shatter" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*pos - 
//			entity - 
//			index - 
//-----------------------------------------------------------------------------
void TE_ShatterSurface( IRecipientFilter& filter, float delay,
	const Vector* pos, const QAngle* angle, const Vector* force, const Vector* forcepos, 
	float width, float height, float shardsize, ShatterSurface_t surfacetype,
	int front_r, int front_g, int front_b, int back_r, int back_g, int back_b)
{
	g_TEShatterSurface.m_vecOrigin			= *pos;
	g_TEShatterSurface.m_vecAngles			= *angle;
	g_TEShatterSurface.m_vecForce			= *force;
	g_TEShatterSurface.m_vecForcePos		= *forcepos;
	g_TEShatterSurface.m_flWidth			= width;
	g_TEShatterSurface.m_flHeight			= height;
	g_TEShatterSurface.m_flShardSize		= shardsize;
	g_TEShatterSurface.m_nSurfaceType		= surfacetype;
	g_TEShatterSurface.m_uchFrontColor.Set( 0, front_r );
	g_TEShatterSurface.m_uchFrontColor.Set( 1, front_g );
	g_TEShatterSurface.m_uchFrontColor.Set( 2, front_b );
	g_TEShatterSurface.m_uchBackColor.Set( 0, back_r );
	g_TEShatterSurface.m_uchBackColor.Set( 1, back_g );
	g_TEShatterSurface.m_uchBackColor.Set( 2, back_b );

	// Send it over the wire
	g_TEShatterSurface.Create( filter, delay );
}

class CBaseTempEntity;

using namespace ::ks::reflect;

extern int	g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud

//-----------------------------------------------------------------------------
// Purpose: Dispatches Sprite tempentity
//-----------------------------------------------------------------------------
class CTEGlowSprite : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEGlowSprite" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEGlowSprite, CBaseTempEntity );

					CTEGlowSprite( const char *name );
	virtual			~CTEGlowSprite( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( float, m_fScale, [[= ks::reflect::Net{ .bits = 8, .low = 0.0, .high = 25.6, .flags = SPROP_ROUNDDOWN } ]] );
	CNetworkVar( float, m_fLife, [[= ks::reflect::Net{ .bits = 8, .low = 0.0, .high = 25.6, .flags = SPROP_ROUNDDOWN } ]] );
	CNetworkVar( int, m_nBrightness, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEGlowSprite::CTEGlowSprite( const char *name ) :
	CBaseTempEntity( name )
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
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEGlowSprite::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_nModelIndex = g_sModelIndexSmoke;
	m_fScale = 0.8;
	m_nBrightness = 200;
	m_fLife = 2.0;
	m_vecOrigin = current_origin;
	
	Vector forward, right;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward, &right, nullptr );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin, 50.0, forward, m_vecOrigin.GetForModify() );
	VectorMA( m_vecOrigin, -25.0, right, m_vecOrigin.GetForModify() );

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEGlowSprite, DT_TEGlowSprite )

// Singleton to fire TEGlowSprite objects
static CTEGlowSprite g_TEGlowSprite( "GlowSprite" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*pos - 
//			modelindex - 
//			life - 
//			size - 
//			brightness - 
//-----------------------------------------------------------------------------
void TE_GlowSprite( IRecipientFilter& filter, float delay,
	const Vector* pos, int modelindex, float life, float size, int brightness )
{
	g_TEGlowSprite.m_vecOrigin		= *pos;
	g_TEGlowSprite.m_nModelIndex	= modelindex;	
	g_TEGlowSprite.m_fLife			= life;
	g_TEGlowSprite.m_fScale			= size;
	g_TEGlowSprite.m_nBrightness	= brightness;

	// Send it over the wire
	g_TEGlowSprite.Create( filter, delay );
}

class CBaseTempEntity;

using namespace ::ks::reflect;

// Purpose: Send generic impact messages to the client for visualization

//-----------------------------------------------------------------------------
// Purpose: Dispatches Gunshot decal tempentity
//-----------------------------------------------------------------------------
class CTEImpact : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEImpact" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEImpact, CBaseTempEntity );

	DECLARE_SERVERCLASS();

	CTEImpact( const char *name );
	virtual	~CTEImpact();

	void Precache( void );
	void Test( const Vector& current_origin, const Vector& current_normal );

public:

	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecNormal, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );	//NOTENOTE: In a multi-play setup we'll probably want non-oriented effects for bandwidth
	CNetworkVar( int, m_iType, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : 
//-----------------------------------------------------------------------------
CTEImpact::CTEImpact( const char *name ) : CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_vecNormal.Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEImpact::~CTEImpact( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEImpact::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEImpact::Test( const Vector& current_origin, const Vector& current_normal )
{
}

//Server class implementation
IMPLEMENT_REFLECT_SERVERCLASS( CTEImpact, DT_TEImpact )

// Singleton to fire TEImpact objects
static CTEImpact g_TEImpact( "Impact" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//-----------------------------------------------------------------------------
void TE_Impact( IRecipientFilter& filter, float delay )
{
	g_TEImpact.Create( filter, delay );
}

class CBaseTempEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Dispatches blood stream tempentity
//-----------------------------------------------------------------------------
class CTEKillPlayerAttachments : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEKillPlayerAttachments" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEKillPlayerAttachments, CBaseTempEntity );

					CTEKillPlayerAttachments( const char *name );
	virtual			~CTEKillPlayerAttachments( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVar( int, m_nPlayer, [[= ks::reflect::Net{ .bits = 5, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEKillPlayerAttachments::CTEKillPlayerAttachments( const char *name ) :
	CBaseTempEntity( name )
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
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEKillPlayerAttachments::Test( const Vector& current_origin, const QAngle& current_angles )
{
	m_nPlayer = 1;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEKillPlayerAttachments, DT_TEKillPlayerAttachments )

// Singleton to fire TEKillPlayerAttachments objects
static CTEKillPlayerAttachments g_TEKillPlayerAttachments( "KillPlayerAttachments" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			player - 
//-----------------------------------------------------------------------------
void TE_KillPlayerAttachments( IRecipientFilter& filter, float delay,
	int player )
{
	g_TEKillPlayerAttachments.m_nPlayer = player;

	// Send it over the wire
	g_TEKillPlayerAttachments.Create( filter, delay );
}

using namespace ::ks::reflect;

extern int	g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud

//-----------------------------------------------------------------------------
// Purpose: Dispatches smoke tempentity
//-----------------------------------------------------------------------------
class CTELargeFunnel : public CTEParticleSystem
{
	[[= ks::reflect::NetTable{ .name = "DT_TELargeFunnel" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTELargeFunnel, CTEParticleSystem );
	DECLARE_SERVERCLASS();

					CTELargeFunnel( const char *name );
	virtual			~CTELargeFunnel( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
public:
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( int, m_nReversed, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTELargeFunnel::CTELargeFunnel( const char *name ) :
	BaseClass( name )
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

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTELargeFunnel::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_nModelIndex = g_sModelIndexSmoke;
	m_nReversed = 0;
	m_vecOrigin = current_origin;
	
	Vector forward, right;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward, &right, nullptr );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin.Get(), 50.0, forward, m_vecOrigin.GetForModify() );
	VectorMA( m_vecOrigin.Get(), 25.0, right, m_vecOrigin.GetForModify() );

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTELargeFunnel, DT_TELargeFunnel )

// Singleton to fire TELargeFunnel objects
static CTELargeFunnel g_TELargeFunnel( "Large Funnel" );

void TE_LargeFunnel( IRecipientFilter& filter, float delay,
	const Vector* pos, int modelindex, int reversed )
{
	g_TELargeFunnel.m_vecOrigin		= *pos;
	g_TELargeFunnel.m_nModelIndex	= modelindex;	
	g_TELargeFunnel.m_nReversed		= reversed;

	// Send it over the wire
	g_TELargeFunnel.Create( filter, delay );
}

class CBaseTempEntity;

using namespace ::ks::reflect;

// Purpose: Create a muzzle flash temp ent

//-----------------------------------------------------------------------------
// Purpose: Dispatches user tracer stream tempentity
//-----------------------------------------------------------------------------
class CTEMuzzleFlash : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEMuzzleFlash" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEMuzzleFlash, CBaseTempEntity );

	DECLARE_SERVERCLASS();

	CTEMuzzleFlash( const char *name );
	virtual			~CTEMuzzleFlash( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );

public:

	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkQAngle( m_vecAngles, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( float, m_flScale, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( int, m_nType, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEMuzzleFlash::CTEMuzzleFlash( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_vecAngles.Init();
	
	m_flScale	= 1.0f;
	m_nType		= 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEMuzzleFlash::~CTEMuzzleFlash( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEMuzzleFlash::Test( const Vector& current_origin, const QAngle& current_angles )
{
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEMuzzleFlash, DT_TEMuzzleFlash )

// Singleton to fire TEMuzzleFlash objects
static CTEMuzzleFlash g_TEMuzzleFlash( "MuzzleFlash" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			origin - 
//			*recipient - 
//			*origin - 
//			*dir - 
//			scale - 
//			type - 
//-----------------------------------------------------------------------------
void TE_MuzzleFlash( IRecipientFilter& filter, float delay,
	const Vector &start, const QAngle &angles, float scale, int type )
{
	g_TEMuzzleFlash.m_vecOrigin		= start;
	g_TEMuzzleFlash.m_vecAngles		= angles;
	g_TEMuzzleFlash.m_flScale		= scale;
	g_TEMuzzleFlash.m_nType			= type;

	// Send it over the wire
	g_TEMuzzleFlash.Create( filter, delay );
}

using namespace ::ks::reflect;

IMPLEMENT_REFLECT_SERVERCLASS( CTEParticleSystem, DT_TEParticleSystem )

class CBaseTempEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: create clientside physics prop, as breaks model if needed
//-----------------------------------------------------------------------------
class CTEPhysicsProp : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEPhysicsProp" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEPhysicsProp, CBaseTempEntity );

					CTEPhysicsProp( const char *name );
	virtual			~CTEPhysicsProp( void );

	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkQAngle( m_angRotation,
	    [[= ks::reflect::Net{ .bits = 13, .enc = ks::reflect::WireEnc::Angle, .index = 0 } ]]
	    [[= ks::reflect::Net{ .bits = 13, .enc = ks::reflect::WireEnc::Angle, .index = 1 } ]]
	    [[= ks::reflect::Net{ .bits = 13, .enc = ks::reflect::WireEnc::Angle, .index = 2 } ]] );
	CNetworkVector( m_vecVelocity, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( int, m_nSkin, [[= ks::reflect::Net{ .bits = ANIMATION_SKIN_BITS } ]] );
	CNetworkVar( int, m_nFlags, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nEffects, [[= ks::reflect::Net{ .bits = EF_MAX_BITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkColor32( m_clrRender, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WireSide::Send>{} ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEPhysicsProp::CTEPhysicsProp( const char *name ) :
	CBaseTempEntity( name )
{
	color32 white = {255, 255, 255, 255};

	m_vecOrigin.Init();
	m_angRotation.Init();
	m_vecVelocity.Init();
	m_nModelIndex		= 0;
	m_nSkin				= 0;
	m_nFlags			= 0;
	m_nEffects			= 0;
	m_clrRender			= white;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEPhysicsProp::~CTEPhysicsProp( void )
{
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEPhysicsProp, DT_TEPhysicsProp )

// Singleton to fire TEBreakModel objects
static CTEPhysicsProp s_TEPhysicsProp( "physicsprop" );

void TE_PhysicsProp( IRecipientFilter& filter, float delay,
	int modelindex, int skin, const Vector& pos, const QAngle &angles, const Vector& vel, int flags, int effects, color24 renderColor )
{
	color32 clrRenderConverted;
	clrRenderConverted.r = renderColor.r;
	clrRenderConverted.g = renderColor.g;
	clrRenderConverted.b = renderColor.b;
	clrRenderConverted.a = 255;

	s_TEPhysicsProp.m_vecOrigin		= pos;
	s_TEPhysicsProp.m_angRotation	= angles;
	s_TEPhysicsProp.m_vecVelocity	= vel;
	s_TEPhysicsProp.m_nModelIndex	= modelindex;	
	s_TEPhysicsProp.m_nSkin			= skin;
	s_TEPhysicsProp.m_nFlags		= flags;
	s_TEPhysicsProp.m_nEffects		= effects;
	s_TEPhysicsProp.m_clrRender		= clrRenderConverted;

	// Send it over the wire
	s_TEPhysicsProp.Create( filter, delay );
}

class CBaseEntity;
class CBaseTempEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Dispatches decal tempentity
//-----------------------------------------------------------------------------
class CTEPlayerDecal : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEPlayerDecal" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEPlayerDecal, CBaseTempEntity );

					CTEPlayerDecal( const char *name );
	virtual			~CTEPlayerDecal( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVar( int, m_nPlayer, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecStart, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecRight, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( int, m_nEntity, [[= ks::reflect::Net{ .bits = MAX_EDICT_BITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nHitbox, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] );
};

class CFEPlayerDecal : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_FEPlayerDecal" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFEPlayerDecal, CBaseEntity );

	CFEPlayerDecal() {
		static int s_nUniqueID = 0;
		m_nUniqueID = ++ s_nUniqueID;
		if ( s_nUniqueID >= ( (1<<16) - 1 ) )
			s_nUniqueID = 1;

		m_unAccountID = 0;
		m_unTraceID = 0;
		m_rtGcTime = 0;

		SetEffects( EF_NOINTERP );
		SetPredictionEligible( false );
		
		m_vecEndPos.Init();
		m_vecStart.Init();
		m_vecRight.Init();
		m_vecNormal.Init();
		m_nPlayer = 0;
		m_nEntity = 0;
		m_nHitbox = 0;
		m_nTintID = 0;

		m_flCreationTime = gpGlobals->curtime;

		m_nVersion = 0;
		for ( int k = 0; k < PLAYERDECALS_SIGNATURE_BYTELEN; ++ k ) m_ubSignature.Set( k, 0 );

		SetThink( &CBaseEntity::SUB_Remove );
		SetNextThink( gpGlobals->curtime + PLAYERDECALS_DURATION_SOLID + PLAYERDECALS_DURATION_FADE2 );

		// Add us to the list
		s_arrFEPlayerDecals.push_back( this );

		// If we have too many then force expire older ones
		while ( std::ssize( s_arrFEPlayerDecals ) >
			PLAYERDECALS_LIMIT_COUNT
			)
		{
			CFEPlayerDecal *pOther = s_arrFEPlayerDecals.front();
			s_arrFEPlayerDecals.erase( s_arrFEPlayerDecals.begin(), s_arrFEPlayerDecals.begin() + 1 );
			pOther->SetNextThink( gpGlobals->curtime );	// must be SUB_Remove
		}
	}
	virtual			~CFEPlayerDecal( void )
	{
		// Remove us from the list
		// Normally we would be first in the list anyways,
		// and even if we are not it's a small number of entries to scan
		( void ) ks::erase_first( s_arrFEPlayerDecals, this );
	}

	// Make sure clients get all the spray entities in the same order
	int UpdateTransmitState_Impl() { return SetTransmitState( FL_EDICT_ALWAYS ); }

	DECLARE_SERVERCLASS();

public:
	CNetworkVar( int, m_nUniqueID, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( uint32, m_unAccountID, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( uint32, m_unTraceID, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( uint32, m_rtGcTime, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVector( m_vecEndPos, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecStart, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecRight, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecNormal, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( int, m_nPlayer, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nEntity, [[= ks::reflect::Net{ .bits = MAX_EDICT_BITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nHitbox, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_flCreationTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( int, m_nTintID, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( uint8, m_nVersion, [[= ks::reflect::Net{ .bits = 3, .flags = SPROP_UNSIGNED } ]] );
	CNetworkArray( uint8, m_ubSignature, PLAYERDECALS_SIGNATURE_BYTELEN, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );

private:
	static std::vector< CFEPlayerDecal * > s_arrFEPlayerDecals;
};
std::vector< CFEPlayerDecal * > CFEPlayerDecal::s_arrFEPlayerDecals;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEPlayerDecal::CTEPlayerDecal( const char *name ) :
	CBaseTempEntity( name )
{
	m_nPlayer = 0;
	m_vecOrigin.Init();
	m_vecStart.Init();
	m_vecRight.Init();
	m_nEntity = 0;
	m_nHitbox = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEPlayerDecal::~CTEPlayerDecal( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEPlayerDecal::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_nPlayer = 1;
	m_nEntity = 0;
	m_vecOrigin = current_origin;

	Vector vecEnd;
	
	Vector forward;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin, 50.0, forward, m_vecOrigin.GetForModify() );
	VectorMA( m_vecOrigin, 1024.0, forward, vecEnd );

	trace_t tr;

	UTIL_TraceLine( m_vecOrigin, vecEnd, MASK_SOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, &tr );

	m_vecOrigin = tr.endpos;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEPlayerDecal, DT_TEPlayerDecal )

IMPLEMENT_REFLECT_SERVERCLASS( CFEPlayerDecal, DT_FEPlayerDecal )
LINK_ENTITY_TO_CLASS( cfe_player_decal, CFEPlayerDecal );

void FE_PlayerDecal( CCSGameRules::ServerPlayerDecalData_t const &data, std::string const &signature )
{
	CFEPlayerDecal *pEnt = ( CFEPlayerDecal * ) CBaseEntity::Create( "cfe_player_decal", data.m_vecOrigin, vec3_angle );
	pEnt->m_unAccountID = data.m_unAccountID;
	pEnt->m_unTraceID = data.m_nTraceID;
	pEnt->m_rtGcTime = data.m_rtGcTime;
	pEnt->m_nPlayer = data.m_nPlayer;
	pEnt->m_vecEndPos = data.m_vecOrigin;
	pEnt->m_vecStart = data.m_vecStart;
	pEnt->m_vecRight = data.m_vecRight;
	pEnt->m_vecNormal = data.m_vecNormal;
	pEnt->m_nEntity = data.m_nEntity;
	pEnt->m_nHitbox = data.m_nHitbox;
	pEnt->m_nTintID = data.m_nTintID;
	pEnt->m_flCreationTime = data.m_flCreationTime;
	pEnt->m_nVersion = PLAYERDECALS_SIGNATURE_VERSION;
	if ( signature.size() == PLAYERDECALS_SIGNATURE_BYTELEN )
	{
		for ( int k = 0; k < PLAYERDECALS_SIGNATURE_BYTELEN; ++ k )
			pEnt->m_ubSignature.Set( k, signature[k] );
	}
}

// Singleton to fire TEPlayerDecal objects
static CTEPlayerDecal g_TEPlayerDecal( "Player Decal" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*pos - 
//			player - 
//			entity - 
//			index - 
//-----------------------------------------------------------------------------
void TE_PlayerDecal( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* start, const Vector* right, int player, int entity, int hitbox )
{
	g_TEPlayerDecal.m_vecOrigin		= *pos;
	g_TEPlayerDecal.m_vecStart		= *start;
	g_TEPlayerDecal.m_vecRight		= *right;
	g_TEPlayerDecal.m_nPlayer		= player;
	g_TEPlayerDecal.m_nEntity		= entity;
	g_TEPlayerDecal.m_nHitbox		= hitbox;

	// Send it over the wire
	g_TEPlayerDecal.Create( filter, delay );
}

class CBaseTempEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Dispatches BSP decal tempentity
//-----------------------------------------------------------------------------
class CTEProjectedDecal : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEProjectedDecal" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEProjectedDecal, CBaseTempEntity );

					CTEProjectedDecal( const char *name );
	virtual			~CTEProjectedDecal( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( int, m_nIndex, [[= ks::reflect::Net{ .bits = 9, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_flDistance, [[= ks::reflect::Net{ .bits = 10, .low = 0, .high = 1024, .flags = SPROP_ROUNDUP } ]] );
	CNetworkQAngle( m_angRotation, [[= ks::reflect::Net{ .bits = 10, .enc = ks::reflect::WireEnc::QAngles } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEProjectedDecal::CTEProjectedDecal( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_angRotation.Init();
	m_flDistance = 64.0f;
	m_nIndex = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEProjectedDecal::~CTEProjectedDecal( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEProjectedDecal::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_flDistance = 1024.0f;
	m_nIndex = 0;
	m_vecOrigin = current_origin;
	m_angRotation = current_angles;

	Vector vecEnd;
	
	Vector forward;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin, 24.0, forward, m_vecOrigin.GetForModify() );

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEProjectedDecal, DT_TEProjectedDecal )

// Singleton to fire TEBSPDecal objects
static CTEProjectedDecal g_TEProjectedDecal( "Projected Decal" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*pos - 
//			entity - 
//			index - 
//			modelindex - 
//-----------------------------------------------------------------------------
void TE_ProjectDecal( IRecipientFilter& filter, float delay,
	const Vector* pos, const QAngle *angles, float distance, int index )
{
	g_TEProjectedDecal.m_vecOrigin		= *pos;
	g_TEProjectedDecal.m_angRotation	= *angles;
	g_TEProjectedDecal.m_flDistance		= distance;	
	g_TEProjectedDecal.m_nIndex			= index;

	// Send it over the wire
	g_TEProjectedDecal.Create( filter, delay );
}

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Dispatches line
//-----------------------------------------------------------------------------
class CTEShowLine : public CTEParticleSystem
{
	[[= ks::reflect::NetTable{ .name = "DT_TEShowLine" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEShowLine, CTEParticleSystem );
	DECLARE_SERVERCLASS();

					CTEShowLine( const char *name );
	virtual			~CTEShowLine( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );

public:
	CNetworkVector( m_vecEnd, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEShowLine::CTEShowLine( const char *name ) :
	BaseClass( name )
{
	m_vecEnd.Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEShowLine::~CTEShowLine( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEShowLine::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_vecOrigin = current_origin;
	
	Vector forward, right;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward, &right, nullptr );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin, 100.0, forward, m_vecEnd.GetForModify() );

	m_vecOrigin = m_vecEnd + right * -128;
	m_vecEnd += right * 128;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEShowLine, DT_TEShowLine )

// Singleton to fire TEShowLine objects
static CTEShowLine g_TEShowLine( "Show Line" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*start - 
//			*end - 
//-----------------------------------------------------------------------------
void TE_ShowLine( IRecipientFilter& filter, float delay,
	const Vector* start, const Vector* end )
{
	g_TEShowLine.m_vecOrigin = *start;
	g_TEShowLine.m_vecEnd = *end;	

	// Send it over the wire
	g_TEShowLine.Create( filter, delay );
}

class CBaseTempEntity;

using namespace ::ks::reflect;

extern int	g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud

//-----------------------------------------------------------------------------
// Purpose: Dispatches smoke tempentity
//-----------------------------------------------------------------------------
class CTESmoke : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TESmoke" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTESmoke, CBaseTempEntity );

					CTESmoke( const char *name );
	virtual			~CTESmoke( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( float, m_fScale, [[= ks::reflect::Net{ .bits = 8, .low = 0.0, .high = 25.6, .flags = SPROP_ROUNDDOWN } ]] );
	CNetworkVar( int, m_nFrameRate, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTESmoke::CTESmoke( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_nModelIndex = 0;
	m_fScale = 0;
	m_nFrameRate = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTESmoke::~CTESmoke( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTESmoke::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_nModelIndex = g_sModelIndexSmoke;
	m_fScale = 5.0;
	m_nFrameRate = 12;
	m_vecOrigin = current_origin;
	
	Vector forward, right;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward, &right, nullptr );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin, 50.0, forward, m_vecOrigin.GetForModify() );
	VectorMA( m_vecOrigin, 25.0, right, m_vecOrigin.GetForModify() );

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTESmoke, DT_TESmoke )

// Singleton to fire TESmoke objects
static CTESmoke g_TESmoke( "Smoke" );

void TE_Smoke( IRecipientFilter& filter, float delay,
	const Vector* pos, int modelindex, float scale, int framerate )
{
	g_TESmoke.m_vecOrigin	= *pos;
	g_TESmoke.m_nModelIndex = modelindex;	
	g_TESmoke.m_fScale		= scale;
	g_TESmoke.m_nFrameRate	= framerate;

	// Send it over the wire
	g_TESmoke.Create( filter, delay );
}

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Dispatches sparks
//-----------------------------------------------------------------------------
class CTESparks : public CTEParticleSystem
{
	[[= ks::reflect::NetTable{ .name = "DT_TESparks" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTESparks, CTEParticleSystem );
	DECLARE_SERVERCLASS();

					CTESparks( const char *name );
	virtual			~CTESparks( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	CNetworkVar( int, m_nMagnitude, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nTrailLength, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVector( m_vecDir, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTESparks::CTESparks( const char *name ) :
	BaseClass( name )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTESparks::~CTESparks( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTESparks::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_vecOrigin = current_origin;
	
	Vector forward;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	m_vecOrigin += forward * 100;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTESparks, DT_TESparks )

// Singleton to fire TESparks objects
static CTESparks g_TESparks( "Sparks" );

void TE_Sparks( IRecipientFilter& filter, float delay,
	const Vector *pos, int nMagnitude, int nTrailLength, const Vector *pDir )
{
	g_TESparks.m_vecOrigin = *pos;
	g_TESparks.m_nMagnitude = nMagnitude;
	g_TESparks.m_nTrailLength = nTrailLength;

	if ( pDir )
	{
		g_TESparks.m_vecDir = *pDir;
	}
	else
	{
		g_TESparks.m_vecDir = vec3_origin;
	}

	// Send it over the wire
	g_TESparks.Create( filter, delay );
}

class CBaseEntity;
class CBaseTempEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Dispatches Sprite tempentity
//-----------------------------------------------------------------------------
class CTESprite : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TESprite" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTESprite, CBaseTempEntity );

					CTESprite( const char *name );
	virtual			~CTESprite( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );

	virtual void	Precache( void );

	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( float, m_fScale, [[= ks::reflect::Net{ .bits = 8, .low = 0.0, .high = 25.6, .flags = SPROP_ROUNDDOWN } ]] );
	CNetworkVar( int, m_nBrightness, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTESprite::CTESprite( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_nModelIndex = 0;
	m_fScale = 0;
	m_nBrightness = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTESprite::~CTESprite( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTESprite::Precache( void )
{
	CBaseEntity::PrecacheModel("sprites/gunsmoke.vmt");
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTESprite::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_nModelIndex = CBaseEntity::PrecacheModel("sprites/gunsmoke.vmt");
	m_fScale = 0.8;
	m_nBrightness = 200;
	m_vecOrigin = current_origin;
	
	Vector forward, right;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward, &right, nullptr );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin, 50.0, forward, m_vecOrigin.GetForModify() );
	VectorMA( m_vecOrigin, -25.0, right, m_vecOrigin.GetForModify() );

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTESprite, DT_TESprite )

// Singleton to fire TESprite objects
static CTESprite g_TESprite( "Sprite" );

void TE_Sprite( IRecipientFilter& filter, float delay,
	const Vector *pos, int modelindex, float size, int brightness )
{
	g_TESprite.m_vecOrigin		= *pos;
	g_TESprite.m_nModelIndex	= modelindex;	
	g_TESprite.m_fScale			= size;
	g_TESprite.m_nBrightness	= brightness;

	// Send it over the wire
	g_TESprite.Create( filter, delay );
}

class CBaseTempEntity;

using namespace ::ks::reflect;

extern int	g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud

//-----------------------------------------------------------------------------
// Purpose: Dispatches Sprite Spray tempentity
//-----------------------------------------------------------------------------
class CTESpriteSpray : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TESpriteSpray" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTESpriteSpray, CBaseTempEntity );

					CTESpriteSpray( const char *name );
	virtual			~CTESpriteSpray( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecDirection, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( int, m_nSpeed, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_fNoise, [[= ks::reflect::Net{ .bits = 8, .low = 0.0, .high = 2.56, .flags = SPROP_ROUNDDOWN } ]] );
	CNetworkVar( int, m_nCount, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTESpriteSpray::CTESpriteSpray( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_vecDirection.Init();
	m_nModelIndex = 0;
	m_fNoise = 0;
	m_nSpeed = 0;
	m_nCount = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTESpriteSpray::~CTESpriteSpray( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTESpriteSpray::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_nModelIndex = g_sModelIndexSmoke;
	m_fNoise = 0.8;
	m_nCount = 5;
	m_nSpeed = 30;
	m_vecOrigin = current_origin;
	
	Vector forward, right;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward, &right, nullptr );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin, 50.0, forward, m_vecOrigin.GetForModify() );
	VectorMA( m_vecOrigin, -25.0, right, m_vecOrigin.GetForModify() );

	m_vecDirection.Init( random_valve->RandomInt( -100, 100 ), random_valve->RandomInt( -100, 100 ), random_valve->RandomInt( 0, 100 ) );

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTESpriteSpray, DT_TESpriteSpray )

// Singleton to fire TESpriteSpray objects
static CTESpriteSpray g_TESpriteSpray( "Sprite Spray" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*pos - 
//			*dir - 
//			modelindex - 
//			speed - 
//			noise - 
//			count - 
//-----------------------------------------------------------------------------
void TE_SpriteSpray( IRecipientFilter& filter, float delay,
	const Vector *pos, const Vector *dir, int modelindex, int speed, float noise, int count )
{
	g_TESpriteSpray.m_vecOrigin		= *pos;
	g_TESpriteSpray.m_vecDirection	= *dir;
	g_TESpriteSpray.m_nModelIndex	= modelindex;	
	g_TESpriteSpray.m_nSpeed		= speed;
	g_TESpriteSpray.m_fNoise		= noise;
	g_TESpriteSpray.m_nCount		= count;

	// Send it over the wire
	g_TESpriteSpray.Create( filter, delay );
}

class CBaseTempEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Dispatches world decal tempentity
//-----------------------------------------------------------------------------
class CTEWorldDecal : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEWorldDecal" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEWorldDecal, CBaseTempEntity );

					CTEWorldDecal( const char *name );
	virtual			~CTEWorldDecal( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( int, m_nIndex, [[= ks::reflect::Net{ .bits = 9, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEWorldDecal::CTEWorldDecal( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_nIndex = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEWorldDecal::~CTEWorldDecal( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEWorldDecal::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_nIndex = 0;
	m_vecOrigin = current_origin;

	Vector vecEnd;
	
	Vector forward;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin, 50.0, forward, m_vecOrigin.GetForModify() );
	VectorMA( m_vecOrigin, 1024.0, forward, vecEnd );

	trace_t tr;

	UTIL_TraceLine( m_vecOrigin, vecEnd, MASK_SOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, &tr );

	m_vecOrigin = tr.endpos;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEWorldDecal, DT_TEWorldDecal )

// Singleton to fire TEWorldDecal objects
static CTEWorldDecal g_TEWorldDecal( "World Decal" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*pos - 
//			index - 
//-----------------------------------------------------------------------------
void TE_WorldDecal( IRecipientFilter& filter, float delay,
	const Vector* pos, int index )
{
	g_TEWorldDecal.m_vecOrigin	= *pos;
	g_TEWorldDecal.m_nIndex		= index;

	// Send it over the wire
	g_TEWorldDecal.Create( filter, delay );
}
