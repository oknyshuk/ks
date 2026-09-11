//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "particle_light.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( env_particlelight, CParticleLight );


//Save/restore
IMPLEMENT_REFLECT_DATAMAP( CParticleLight )



//-----------------------------------------------------------------------------
// Purpose: Called before spawning, after key values have been set.
//-----------------------------------------------------------------------------
CParticleLight::CParticleLight()
{
	m_flIntensity = 5000;
	m_vColor.Init( 1, 0, 0 );
	m_PSName = NULL_STRING;
	m_bDirectional = false;
}


