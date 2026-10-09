#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"
#include "networkvar.h"
#include "string_t.h"
#ifdef CLIENT_DLL
#include "dt_recv.h"
#else
#include "dt_send.h"
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
