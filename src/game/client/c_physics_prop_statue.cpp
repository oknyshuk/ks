// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_physics_prop_statue.h"

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
