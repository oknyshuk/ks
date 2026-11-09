// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "physics.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "plasma.h"

//==================================================
// CPlasma
//==================================================

//Link the entity
LINK_ENTITY_TO_CLASS( _plasma, CPlasma );

//Send datatable
IMPLEMENT_REFLECT_SERVERCLASS( CPlasma, DT_Plasma )

//Data description 

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CPlasma::CPlasma( void ) 
{
	//Client-side
	m_flScale				= 0.0f;
	m_flScaleTime			= 0.0f;
	m_nFlags				= bitsFIRE_NONE;
	m_nPlasmaModelIndex		= PrecacheModel( "sprites/plasma1.vmt" );
	m_nPlasmaModelIndex2	= PrecacheModel( "sprites/plasma1.vmt" );//<<TEMP>>
	m_nGlowModelIndex		= PrecacheModel( "sprites/fire_floor.vmt" );
	//Server-side
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPlasma::~CPlasma( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void CPlasma::EnableSmoke( int state )
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
//-----------------------------------------------------------------------------
void CPlasma::Precache_Impl( void )
{
	PrecacheModel( "sprites/plasma1.vmt" );
	PrecacheModel( "sprites/fire_floor.vmt" );
}
