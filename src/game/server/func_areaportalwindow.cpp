// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.math;
#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "shareddefs.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "player.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "func_areaportalwindow.h"
#include "entitylist.h"

// The server will still send entities through a window even after it opaque 
// to allow for net lag.
constexpr auto FADE_DIST_BUFFER = 10;


LINK_ENTITY_TO_CLASS( func_areaportalwindow, CFuncAreaPortalWindow );


IMPLEMENT_REFLECT_SERVERCLASS( CFuncAreaPortalWindow, DT_FuncAreaPortalWindow )


IMPLEMENT_REFLECT_DATAMAP( CFuncAreaPortalWindow )




CFuncAreaPortalWindow::CFuncAreaPortalWindow()
{
	m_iBackgroundModelIndex = -1;
}


CFuncAreaPortalWindow::~CFuncAreaPortalWindow()
{
}


void CFuncAreaPortalWindow::Spawn()
{
	Precache();

	engine->SetAreaPortalState( m_portalNumber, 1 );
}


void CFuncAreaPortalWindow::Activate()
{
	BaseClass::Activate();
	
	// Find our background model.
	CBaseEntity *pBackground = gEntList.FindEntityByName( nullptr, m_iBackgroundBModelName );
	if( pBackground )
	{
		m_iBackgroundModelIndex  = modelinfo->GetModelIndex( STRING( pBackground->GetModelName() ) );
		pBackground->AddEffects( EF_NODRAW ); // we will draw for it.
	}

	// Find our target and steal its bmodel.
	CBaseEntity *pTarget = gEntList.FindEntityByName( nullptr, m_target );
	if( pTarget )
	{
		SetModel( STRING(pTarget->GetModelName()) );
		SetAbsOrigin( pTarget->GetAbsOrigin() );
		pTarget->AddEffects( EF_NODRAW ); // we will draw for it.
	}
}


bool CFuncAreaPortalWindow::IsWindowOpen( const std::vector< Vector > &vecOrigins, float fovDistanceAdjustFactor )
{
	float flClosestDistance = std::numeric_limits<float>::max();
	for ( int i = 0; i < std::ssize( vecOrigins ); ++i )
	{
		float flDist = CollisionProp()->CalcDistanceFromPoint( vecOrigins[ i ] );
		if ( flDist < flClosestDistance )
		{
			flClosestDistance = flDist;
		}
	}

	if ( flClosestDistance == std::numeric_limits<float>::max() )
		return false;

	flClosestDistance *= fovDistanceAdjustFactor;
	return ( flClosestDistance <= (m_flFadeDist + FADE_DIST_BUFFER) );
}


bool CFuncAreaPortalWindow::UpdateVisibility( const std::vector< Vector > &vecOrigins, float fovDistanceAdjustFactor, bool &bIsOpenOnClient )
{
	if ( IsWindowOpen( vecOrigins, fovDistanceAdjustFactor ) )
	{
		return BaseClass::UpdateVisibility( vecOrigins, fovDistanceAdjustFactor, bIsOpenOnClient );
	}
	else
	{
		bIsOpenOnClient = false;
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Changes the fade start distance 
// Input: float distance in inches
//-----------------------------------------------------------------------------
void CFuncAreaPortalWindow::InputSetFadeStartDistance( inputdata_t &inputdata )
{
	m_flFadeStartDist = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: Changes the fade end distance
// Input: float distance in inches
//-----------------------------------------------------------------------------
void CFuncAreaPortalWindow::InputSetFadeEndDistance( inputdata_t &inputdata )
{
	m_flFadeDist = inputdata.value.Float();
}
