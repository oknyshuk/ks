//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef PARTICLE_FIRE_H
#define PARTICLE_FIRE_H

#include "reflect_annotations.h"


#include "baseparticleentity.h"


class [[= ks::reflect::NetTable{ .name = "DT_ParticleFire", .base = false } ]]
      CParticleFire : public CBaseParticleEntity
{

public:
	CParticleFire();

	DECLARE_CLASS( CParticleFire, CBaseParticleEntity );

					DECLARE_SERVERCLASS();

	// The client shoots a ray out and starts creating fire where it hits.
	CNetworkVector( m_vOrigin, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVector( m_vDirection, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::ENC_VECTOR } ]] );
};


#endif



