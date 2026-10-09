// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;
#include "vphysics_interface.h"
#include "const.h"
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
#include "base_transmit_proxy.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "basetempentity.h"
#include "player.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "env_zoom.h"

constexpr auto ENV_ZOOM_OVERRIDE = (1<<0);

class CEnvZoom : public CPointEntity
{
public:
	DECLARE_CLASS( CEnvZoom, CPointEntity );

	[[= ks::reflect::Input{ .name = "Zoom", .type = FIELD_VOID } ]] void	InputZoom( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "UnZoom", .type = FIELD_VOID } ]] void	InputUnZoom( inputdata_t &inputdata );

	int	GetFOV( void ) { return m_nFOV;	}
	float GetSpeed( void ) { return m_flSpeed;	}
private:

	[[= ks::reflect::Key{ .name = "Rate" } ]] float	m_flSpeed;
	[[= ks::reflect::Key{ .name = "FOV" } ]] int		m_nFOV;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( env_zoom, CEnvZoom );

IMPLEMENT_REFLECT_DATAMAP( CEnvZoom )

bool CanOverrideEnvZoomOwner( CBaseEntity *pZoomOwner )
{
	CEnvZoom *pZoom = dynamic_cast<CEnvZoom*>(pZoomOwner );

	if ( pZoom == nullptr || pZoom && pZoom->HasSpawnFlags( ENV_ZOOM_OVERRIDE ) == false )
		 return false;

	return true;
}

float GetZoomOwnerDesiredFOV( CBaseEntity *pZoomOwner )
{
	if ( CanOverrideEnvZoomOwner( pZoomOwner ) )
	{
		CEnvZoom *pZoom = dynamic_cast<CEnvZoom*>( pZoomOwner );

		return pZoom->GetFOV();
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CEnvZoom::InputZoom( inputdata_t &inputdata )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

	if ( pPlayer )
	{


		// If the player's already holding a fov from another env_zoom, we're allowed to overwrite it
		if ( pPlayer->GetFOVOwner() && FClassnameIs( pPlayer->GetFOVOwner(), "env_zoom" ) )
		{
			pPlayer->ClearZoomOwner();
		}

		//Stuff the values
		pPlayer->SetFOV( this, m_nFOV, m_flSpeed );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CEnvZoom::InputUnZoom( inputdata_t &inputdata )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

	if ( pPlayer )
	{
		// Stuff the values
		pPlayer->SetFOV( this, 0 );
	}
}
