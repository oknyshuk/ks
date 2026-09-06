//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "baseparticleentity.h"
#include "sendproxy.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class [[= ks::reflect::NetTable{ .name = "DT_FuncSmokeVolume" } ]]
      [[= ks::reflect::From<"m_spawnflags", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED }>{} ]]
      CFuncSmokeVolume : public CBaseParticleEntity
{
public:
	DECLARE_CLASS( CFuncSmokeVolume, CBaseParticleEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CFuncSmokeVolume();
	void Spawn();
	void Activate( void );

	// Set the times it fades out at.
	void SetDensity( float density );

private:
	CNetworkVar( color32, m_Color1, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WIRE_SEND>{} ]] [[= ks::reflect::Key{ .name = "Color1" } ]] );
	CNetworkVar( color32, m_Color2, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WIRE_SEND>{} ]] [[= ks::reflect::Key{ .name = "Color2" } ]] );
	CNetworkString( m_MaterialName, 255, [[= ks::reflect::Net{} ]] );
	[[= ks::reflect::Key{ .name = "Material" } ]] string_t m_String_tMaterialName;
	CNetworkVar( float, m_ParticleDrawWidth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "ParticleDrawWidth" } ]] );
	CNetworkVar( float, m_ParticleSpacingDistance, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "ParticleSpacingDistance" } ]] );
	CNetworkVar( float, m_DensityRampSpeed, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "DensityRampSpeed" } ]] );
	CNetworkVar( float, m_RotationSpeed, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "RotationSpeed" } ]] [[= ks::reflect::Key{ .name = "SetRotationSpeed", .input = true } ]] );
	CNetworkVar( float, m_MovementSpeed, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "MovementSpeed" } ]] [[= ks::reflect::Key{ .name = "SetMovementSpeed", .input = true } ]] );
	CNetworkVar( float, m_Density, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "Density" } ]] [[= ks::reflect::Key{ .name = "SetDensity", .input = true } ]] );
	CNetworkVar( float, m_maxDrawDistance, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "MaxDrawDistance" } ]] );
};

IMPLEMENT_REFLECT_DATAMAP( CFuncSmokeVolume )




IMPLEMENT_REFLECT_SERVERCLASS( CFuncSmokeVolume, DT_FuncSmokeVolume )

LINK_ENTITY_TO_CLASS( func_smokevolume, CFuncSmokeVolume );

CFuncSmokeVolume::CFuncSmokeVolume()
{
	m_Density = 1.0f;
	m_maxDrawDistance = 0.0f;
}

void CFuncSmokeVolume::SetDensity( float density )
{
	m_Density = density;
}

void CFuncSmokeVolume::Spawn()
{
	memset( m_MaterialName.GetForModify(), 0, sizeof( m_MaterialName ) );

	// Bind to our bmodel.
	SetModel( STRING( GetModelName() ) );

	BaseClass::Spawn();
}

void CFuncSmokeVolume::Activate( void )
{
	BaseClass::Activate();
	Q_strncpy( m_MaterialName.GetForModify(), STRING( m_String_tMaterialName ), 255 );
}

