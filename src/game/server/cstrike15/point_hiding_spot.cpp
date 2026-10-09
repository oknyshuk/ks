// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

import std;
import ks.tier1;
#include "string_t.h"
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
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "base_transmit_proxy.h"
#include "soundflags.h"
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
#include "entitylist.h"
#include "basetempentity.h"
#include "player.h"
#include "reflect_annotations.h"
#include "reflect_datamap.h"
#include "cs_nav_mesh.h"

const char* g_pszPostActivateSetupThink = "PostActivateSetupThink";

class CPointHidingSpot : public CServerOnlyPointEntity
{
	DECLARE_CLASS( CPointHidingSpot, CServerOnlyPointEntity )
public:
	CPointHidingSpot();
	DECLARE_DATADESC();

	virtual void Activate() override;
	virtual void UpdateOnRemove() override;
	void PostActivateSetupThink();
	void DetachFromHidingSpot();
protected:
	CNavArea *m_pNavArea;		// nav our hiding spot is associated with
	HidingSpot *m_pSpot;
};

IMPLEMENT_REFLECT_DATAMAP( CPointHidingSpot )

LINK_ENTITY_TO_CLASS( point_hiding_spot, CPointHidingSpot );



CPointHidingSpot::CPointHidingSpot() : 
m_pNavArea( nullptr ),
m_pSpot( nullptr )
{
}

void CPointHidingSpot::Activate()
{
	BaseClass::Activate();
		
	// Do setup after all activates have run and we're in game simulation. Nav gets loaded/setup during server
	// activate so we can't guarantee this ent will be before or after nav load.
	SetContextThink( &CPointHidingSpot::PostActivateSetupThink, gpGlobals->curtime + 0.1f, g_pszPostActivateSetupThink );
}

void CPointHidingSpot::DetachFromHidingSpot()
{
	m_pSpot = nullptr;
}

void CPointHidingSpot::UpdateOnRemove()
{
	if ( m_pNavArea && m_pSpot )
	{
		m_pNavArea->RemoveHidingSpot( m_pSpot );
		bool bSuccess = DestroyHidingSpot( m_pSpot );
		contract_assert( bSuccess );
		(void)bSuccess;
	}
	BaseClass::UpdateOnRemove();
}

void CPointHidingSpot::PostActivateSetupThink()
{
	if ( !m_pNavArea )
	{
		m_pNavArea = TheNavMesh->GetNearestNavArea( this, 0, MAX_COORD_FLOAT );
		if ( !m_pNavArea )
		{
			Warning( "Warning: point_hiding_spot (%s) couldn't find a nearby nav. Removing.", GetDebugName() );
			UTIL_Remove( this );
			return;
		}
		Vector vecPointOnGround;
		m_pNavArea->GetClosestPointOnArea( GetAbsOrigin(), &vecPointOnGround );
		m_pSpot = TheNavMesh->CreateHidingSpot();
		m_pSpot->SetSaved( false );
		m_pSpot->SetPosition( vecPointOnGround );
		m_pSpot->SetFlags( HidingSpot::IN_COVER | HidingSpot::GOOD_SNIPER_SPOT | HidingSpot::IDEAL_SNIPER_SPOT );
		m_pSpot->SetArea( m_pNavArea );
		m_pNavArea->AddHidingSpot( m_pSpot );

		SetContextThink( nullptr, TICK_NEVER_THINK, g_pszPostActivateSetupThink );
	}
}
