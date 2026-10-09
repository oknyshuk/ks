// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.math;
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "shareddefs.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "reflect_sendtable.h"
#include "fire_smoke.h"

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------

//==================================================
// CBaseFire
//==================================================

CBaseFire::CBaseFire( void )
{
	m_flStartScale		= 0.0f;
	m_flScale			= 0.0f;
	m_flScaleTime		= 0.0f;
	m_nFlags			= bitsFIRE_NONE;
}

CBaseFire::~CBaseFire( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Take the current scale of the flame and move it towards a destination
// Input  : size - destination size
//			time - time to scale across
//-----------------------------------------------------------------------------
void CBaseFire::Scale( float size, float time )
{
	//Send to the client
	m_flScale		= size;
	m_flScaleTime	= time;
}

//-----------------------------------------------------------------------------
// Purpose: Overloaded Scale() function to set size
// Input  : start - beginning sizek
//			size - destination size
//			time - time to scale across
//-----------------------------------------------------------------------------
void CBaseFire::Scale( float start, float size, float time )
{
	//Send to the client
	m_flStartScale	= start;
	m_flScale		= size;
	m_flScaleTime	= time;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void CBaseFire::Enable( int state )
{
	if ( state )
	{
		m_nFlags |= bitsFIRE_ACTIVE;
	}
	else
	{
		m_nFlags &= ~bitsFIRE_ACTIVE;
	}
}

//==================================================
// CFireSmoke
//==================================================

//Link the entity
LINK_ENTITY_TO_CLASS( _firesmoke, CFireSmoke );

//Send datatable
IMPLEMENT_REFLECT_SERVERCLASS( CFireSmoke, DT_FireSmoke )

//Data description 

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CFireSmoke::CFireSmoke( void ) 
{
	//Client-side
	m_flScale			= 0.0f;
	m_flScaleTime		= 0.0f;
	m_nFlags			= bitsFIRE_NONE;

	//Server-side
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFireSmoke::~CFireSmoke( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFireSmoke::Precache()
{
	BaseClass::Precache();
}

void CFireSmoke::Spawn()
{
	Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void CFireSmoke::EnableSmoke( int state )
{
	if ( state )
	{
		m_nFlags |= bitsFIRESMOKE_SMOKE;
	}
	else
	{
		m_nFlags &= ~bitsFIRESMOKE_SMOKE;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void CFireSmoke::EnableGlow( int state )
{
	if ( state )
	{
		m_nFlags |= bitsFIRESMOKE_GLOW;
	}
	else
	{
		m_nFlags &= ~bitsFIRESMOKE_GLOW;
	}
}

void CFireSmoke::EnableVisibleFromAbove( int state )
{
	if ( state )
	{
		m_nFlags |= bitsFIRESMOKE_VISIBLE_FROM_ABOVE;
	}
	else
	{
		m_nFlags &= ~bitsFIRESMOKE_VISIBLE_FROM_ABOVE;
	}
}
