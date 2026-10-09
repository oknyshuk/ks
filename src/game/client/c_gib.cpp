// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "const.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "physics.h"
#include "vcollide_parse.h"
#include "c_gib.h"

//NOTENOTE: This is not yet coupled with the server-side implementation of CGib
//			This is only a client-side version of gibs at the moment

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_Gib::~C_Gib( void )
{
	VPhysicsDestroyObject();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszModelName - 
//			vecOrigin - 
//			vecForceDir - 
//			vecAngularImp - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
C_Gib *C_Gib::CreateClientsideGib( const char *pszModelName, Vector vecOrigin, Vector vecForceDir, AngularImpulse vecAngularImp, float flLifetime )
{
	C_Gib *pGib = new C_Gib;

	if ( pGib == nullptr )
		return nullptr;

	if ( pGib->InitializeGib( pszModelName, vecOrigin, vecForceDir, vecAngularImp, flLifetime ) == false )
		return nullptr;

	return pGib;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszModelName - 
//			vecOrigin - 
//			vecForceDir - 
//			vecAngularImp - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_Gib::InitializeGib( const char *pszModelName, Vector vecOrigin, Vector vecForceDir, AngularImpulse vecAngularImp, float flLifetime )
{
	if ( InitializeAsClientEntity( pszModelName, false ) == false )
	{
		Release();
		return false;
	}

	SetAbsOrigin( vecOrigin );
	SetCollisionGroup( COLLISION_GROUP_DEBRIS );

	solid_t tmpSolid;
	PhysModelParseSolid( tmpSolid, this, GetModelIndex() );
	
	m_pPhysicsObject = VPhysicsInitNormal( SOLID_VPHYSICS, 0, false, &tmpSolid );
	
	if ( m_pPhysicsObject )
	{
		float flForce = m_pPhysicsObject->GetMass();
		vecForceDir *= flForce;	

		m_pPhysicsObject->ApplyForceOffset( vecForceDir, GetAbsOrigin() );
		m_pPhysicsObject->SetCallbackFlags( m_pPhysicsObject->GetCallbackFlags() | CALLBACK_GLOBAL_TOUCH | CALLBACK_GLOBAL_TOUCH_STATIC );
	}
	else
	{
		// failed to create a physics object
		Release();
		return false;
	}

	SetNextClientThink( gpGlobals->curtime + flLifetime );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_Gib::ClientThink( void )
{
	SetRenderMode( kRenderTransAlpha );
	SetRenderFX( kRenderFxFadeFast );

	if ( GetRenderAlpha() == 0 )
	{
		Release();
		return;
	}

	SetNextClientThink( gpGlobals->curtime + 1.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void C_Gib::StartTouch( C_BaseEntity *pOther )
{
	// Limit the amount of times we can bounce
	if ( m_flTouchDelta < gpGlobals->curtime )
	{
		HitSurface( pOther );
		m_flTouchDelta = gpGlobals->curtime + 0.1f;
	}

	BaseClass::StartTouch( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void C_Gib::HitSurface( C_BaseEntity *pOther )
{
	//TODO: Implement splatter or effects in child versions
}
