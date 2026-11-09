// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
#include "const.h"
#include "predictable_entity.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "itempents.h"
#include "vphysics_interface.h"
#include "engine/ivmodelinfo.h"
#include "debugoverlay_shared.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_basetempentity.h"
#include "particle_simple3d.h"
#include "toolframework_client.h"
#include "fx.h"

#define PI 3.14159265359
constexpr auto GLASS_SHARD_MIN_LIFE = 2;
constexpr auto GLASS_SHARD_MAX_LIFE = 5;
constexpr auto GLASS_SHARD_NOISE = 0.3;
constexpr auto GLASS_SHARD_GRAVITY = 500;
constexpr auto GLASS_SHARD_DAMPING = 0.3;
 
#include "precache_register.h"

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheEffectGlassShatter )
PRECACHE( MATERIAL, "effects/fleck_glass1" )
PRECACHE( MATERIAL, "effects/fleck_glass2" )
PRECACHE( MATERIAL, "effects/fleck_tile1" )
PRECACHE( MATERIAL, "effects/fleck_tile2" )
PRECACHE_REGISTER_END()

ConVar fx_glass_velocity_cap("fx_glass_velocity_cap", "0", 0, "Maximum downwards speed of shattered glass particles");

//###################################################
// > C_TEShatterSurface
//###################################################
class [[= ks::reflect::NetTable{ .name = "DT_TEShatterSurface" } ]]
      C_TEShatterSurface : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEShatterSurface, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	C_TEShatterSurface( void );
	~C_TEShatterSurface( void );

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
IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( C_TEShatterSurface, DT_TEShatterSurface, CTEShatterSurface )


//------------------------------------------------------------------------------
// Constructor, destructor
//------------------------------------------------------------------------------
C_TEShatterSurface::C_TEShatterSurface( void )
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

C_TEShatterSurface::~C_TEShatterSurface()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TEShatterSurface::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "C_TEShatterSurface::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	ks::intrusive_ptr<CSimple3DEmitter> pGlassEmitter = CSimple3DEmitter::Create( "C_TEShatterSurface 1" );
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
			pParticle = (Particle3D *) pGlassEmitter->AddParticle( sizeof(Particle3D), hMaterial[random->RandomInt(0,1)], vCurPos );

			Vector vForceVel = Vector(0,0,0);
			if (random->RandomInt(0, 3) != 0)
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
				vForceVel.z = random->RandomFloat(-fx_glass_velocity_cap.GetFloat(), -(fx_glass_velocity_cap.GetFloat() * 0.66f));
			}

			if (pParticle)
			{
				pParticle->m_flLifeRemaining	= random->RandomFloat(GLASS_SHARD_MIN_LIFE,GLASS_SHARD_MAX_LIFE);
				pParticle->m_vecVelocity		= vForceVel;
				pParticle->m_vecVelocity	   += RandomVector(-25,25);
				pParticle->m_uchSize			= m_flShardSize + random->RandomFloat(-0.5*m_flShardSize,0.5*m_flShardSize);
				pParticle->m_vAngles			= m_vecAngles;
				pParticle->m_flAngSpeed			= random->RandomFloat(-400,400);

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
	__g_C_TEShatterSurface.m_vecOrigin = *pos;
	__g_C_TEShatterSurface.m_vecAngles = *angle;
	__g_C_TEShatterSurface.m_vecForce = *vForce;
	__g_C_TEShatterSurface.m_vecForcePos = *vForcePos;
	__g_C_TEShatterSurface.m_flWidth = width;
	__g_C_TEShatterSurface.m_flHeight = height;
	__g_C_TEShatterSurface.m_flShardSize = shardsize;
	__g_C_TEShatterSurface.m_nSurfaceType = surfacetype;
	__g_C_TEShatterSurface.m_uchFrontColor[0] = front_r;
	__g_C_TEShatterSurface.m_uchFrontColor[1] = front_g;
	__g_C_TEShatterSurface.m_uchFrontColor[2] = front_b;
	__g_C_TEShatterSurface.m_uchBackColor[0] = back_r;
	__g_C_TEShatterSurface.m_uchBackColor[1] = back_g;
	__g_C_TEShatterSurface.m_uchBackColor[2] = back_b;

	__g_C_TEShatterSurface.PostDataUpdate( DATA_UPDATE_CREATED );
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
