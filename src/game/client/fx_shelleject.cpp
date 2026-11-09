// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "c_baseplayer.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "fx.h"
#include "c_te_effect_dispatch.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ShellEjectCallback( const CEffectData &data )
{
	// Use the gun angles to orient the shell
	IClientRenderable *pRenderable = data.GetRenderable();
	if ( pRenderable )
	{
		DevWarning( "Unhandled ShellEject effect\n" );
	}
}

DECLARE_CLIENT_EFFECT( ShellEject, ShellEjectCallback );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RifleShellEjectCallback( const CEffectData &data )
{
	// Use the gun angles to orient the shell
	IClientRenderable *pRenderable = data.GetRenderable();
	if ( pRenderable )
	{
		DevWarning( "Unhandled RifleShellEject effect\n" );
	}
}

DECLARE_CLIENT_EFFECT( RifleShellEject, RifleShellEjectCallback );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ShotgunShellEjectCallback( const CEffectData &data )
{
	// Use the gun angles to orient the shell
	IClientRenderable *pRenderable = data.GetRenderable();
	if ( pRenderable )
	{
		DevWarning( "Unhandled ShotgunShellEject effect\n" );
	}
}

DECLARE_CLIENT_EFFECT( ShotgunShellEject, ShotgunShellEjectCallback );
