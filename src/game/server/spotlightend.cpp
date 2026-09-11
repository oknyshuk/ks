//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Dynamic light at the end of a spotlight
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "spotlightend.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS(spotlight_end, CSpotlightEnd);

IMPLEMENT_REFLECT_SERVERCLASS( CSpotlightEnd, DT_SpotlightEnd )


//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------


//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CSpotlightEnd::Spawn( void )
{
	Precache();
	m_flLightScale  = 100;
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
	UTIL_SetSize( this, vec3_origin, vec3_origin );
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
}
