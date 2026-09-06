//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "plasma.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

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
void CPlasma::Precache( void )
{
	PrecacheModel( "sprites/plasma1.vmt" );
	PrecacheModel( "sprites/fire_floor.vmt" );
}
