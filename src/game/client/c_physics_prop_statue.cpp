//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_physics_prop_statue.h"
#include "debugoverlay_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_REFLECT_CLIENTCLASS( C_StatueProp, DT_StatueProp, CStatueProp )

C_StatueProp::C_StatueProp()
{
}

C_StatueProp::~C_StatueProp()
{
}

void C_StatueProp::Spawn( void )
{
	BaseClass::Spawn();

	m_EntClientFlags |= ENTCLIENTFLAG_DONTUSEIK;
}

void C_StatueProp::ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	CBaseAnimating *pBaseAnimating = m_hInitBaseAnimating;

	if ( pBaseAnimating )
	{
		pBaseAnimating->CollisionProp()->WorldSpaceSurroundingBounds( pVecWorldMins, pVecWorldMaxs );
	}
}


void C_StatueProp::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( m_bShatter )
	{
		// Here's the networked data to use with my effect
		//m_nShatterFlags;
		//m_vShatterPosition;
		//m_vShatterForce;

		//FIXME: shatter effects should call a function so derived classes can make their own
		// Do shatter effects here
	}
}
