//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"

#include "particle_fire.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


IMPLEMENT_REFLECT_SERVERCLASS( CParticleFire, DT_ParticleFire )

LINK_ENTITY_TO_CLASS( env_particlefire, CParticleFire );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------


CParticleFire::CParticleFire()
{
#ifdef _DEBUG
	m_vOrigin.Init();
	m_vDirection.Init();
#endif
}



