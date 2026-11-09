// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatcharacter.h"
#include "proxyentity.h"
#include "iclientrenderable.h"

//-----------------------------------------------------------------------------
// Cleanup
//-----------------------------------------------------------------------------
void CEntityMaterialProxy::Release( void )
{ 
	delete this; 
}

//-----------------------------------------------------------------------------
// Helper class to deal with floating point inputs
//-----------------------------------------------------------------------------
void CEntityMaterialProxy::OnBind( void *pRenderable )
{
	if( !pRenderable )
		return;

	IClientRenderable *pRend = ( IClientRenderable* )pRenderable;
	C_BaseEntity *pEnt = EntityCast<C_BaseEntity>( pRend->GetIClientUnknown()->GetBaseEntity() );
	if ( pEnt )
	{
		OnBind( pEnt );
	}
}
