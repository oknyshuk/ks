// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "eiface.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "entitylist.h"
#include "physics.h"
#include "ndebugoverlay.h"
#include "game.h"
#include "cplane.h"

//=========================================================
// Plane
//=========================================================
CPlane::CPlane ( void )
{
	m_fInitialized = false;
}

//=========================================================
// InitializePlane - Takes a normal for the plane and a
// point on the plane and 
//=========================================================
void CPlane::InitializePlane ( const Vector &vecNormal, const Vector &vecPoint )
{
	m_vecNormal = vecNormal;
	m_flDist = DotProduct ( m_vecNormal, vecPoint );
	m_fInitialized = true;
}


//=========================================================
// PointInFront - determines whether the given vector is 
// in front of the plane. 
//=========================================================
bool CPlane::PointInFront ( const Vector &vecPoint )
{
	float flFace;

	if ( !m_fInitialized )
	{
		return false;
	}

	flFace = DotProduct ( m_vecNormal, vecPoint ) - m_flDist;

	if ( flFace >= 0 )
	{
		return true;
	}

	return false;
}

//=========================================================
//=========================================================
float CPlane::PointDist ( const Vector &vecPoint )
{
	float flDist;

	if ( !m_fInitialized )
	{
		return false;
	}

	flDist = DotProduct ( m_vecNormal, vecPoint ) - m_flDist;

	return flDist;
}
