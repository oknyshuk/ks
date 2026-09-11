//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ENTITYPARTICLETRAIL_SHARED_H
#define ENTITYPARTICLETRAIL_SHARED_H

#include "reflect_annotations.h"

#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// For networking this bad boy
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
EXTERN_RECV_TABLE( DT_EntityParticleTrailInfo );
#else
EXTERN_SEND_TABLE( DT_EntityParticleTrailInfo );
#endif


//-----------------------------------------------------------------------------
// Particle trail info
//-----------------------------------------------------------------------------
struct [[= ks::reflect::NetTable{ .name = "DT_EntityParticleTrailInfo", .base = false } ]]
      [[= ks::reflect::From<"m_flLifetime", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_flStartSize", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_flEndSize", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
      EntityParticleTrailInfo_t
{
	EntityParticleTrailInfo_t();

	DECLARE_CLASS_NOBASE( EntityParticleTrailInfo_t );
	DECLARE_SIMPLE_DATADESC();
	DECLARE_EMBEDDED_NETWORKVAR();

	[[= ks::reflect::Key{ .name = "ParticleTrailMaterial" } ]] string_t m_strMaterialName;
	CNetworkVar( float, m_flLifetime, [[= ks::reflect::Key{ .name = "ParticleTrailLifetime" } ]] );
	CNetworkVar( float, m_flStartSize, [[= ks::reflect::Key{ .name = "ParticleTrailStartSize" } ]] );
	CNetworkVar( float, m_flEndSize, [[= ks::reflect::Key{ .name = "ParticleTrailEndSize" } ]] );
};



#endif // ENTITYPARTICLETRAIL_SHARED_H
