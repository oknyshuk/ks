//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Drops particles where the entity was.
//
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#ifdef CLIENT_DLL
#include "reflect_recvtable.h"
#endif
#include "reflect_annotations.h"
#ifdef GAME_DLL
#include "reflect_sendtable.h"
#endif
#include "entityparticletrail_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Default values
//-----------------------------------------------------------------------------
EntityParticleTrailInfo_t::EntityParticleTrailInfo_t()
{
	m_strMaterialName = NULL_STRING;
	m_flLifetime = 4.0f;
	m_flStartSize = 2.0f;
	m_flEndSize = 3.0f;
}

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------
#ifndef CLIENT_DLL

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( EntityParticleTrailInfo_t )

#endif

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_TABLE( EntityParticleTrailInfo_t, DT_EntityParticleTrailInfo );



