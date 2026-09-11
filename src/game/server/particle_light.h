//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PARTICLE_LIGHT_H
#define PARTICLE_LIGHT_H

#include "reflect_annotations.h"


#include "baseentity.h"


//==================================================
// CParticleLight. These are tied to 
//==================================================

#define PARTICLELIGHT_ENTNAME	"env_particlelight"

class CParticleLight : public CServerOnlyPointEntity
{
public:
	DECLARE_CLASS( CParticleLight, CServerOnlyPointEntity );
	DECLARE_DATADESC();

					CParticleLight();


public:
	[[= ks::reflect::Key{ .name = "Intensity" } ]] float			m_flIntensity;
	[[= ks::reflect::Key{ .name = "Color" } ]] Vector			m_vColor;	// 0-255
	[[= ks::reflect::Key{ .name = "PSName" } ]] string_t		m_PSName;	// Name of the particle system entity this light affects.
	[[= ks::reflect::Key{ .name = "Directional" } ]] bool			m_bDirectional;
};


#endif // PARTICLE_LIGHT_H
