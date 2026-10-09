// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.vstdlib;
#include "edict.h"
#include "iserverentity.h"
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
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "physics.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "npcevent.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "basecombatweapon.h"
#include "explode.h"
#include "eventqueue.h"
#include "gamerules.h"
#include "ammodef.h"
#include "in_buttons.h"
#include "soundent.h"
#include "ndebugoverlay.h"
#include "engine/IEngineSound.h"
#include "game.h"

#include "player.h"
#include "entitylist.h"

// Spawnflags
constexpr auto SF_MESSAGE_DISABLED = 1;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CMessageEntity : public CPointEntity
{
	DECLARE_CLASS( CMessageEntity, CPointEntity );

public:
	void	Spawn( void );
	void	Activate( void );
	void	Think( void );
	void	DrawOverlays(void);

	virtual void UpdateOnRemove();

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );

	DECLARE_DATADESC();

protected:
	[[= ks::reflect::Key{ .name = "radius" } ]] int				m_radius;
	[[= ks::reflect::Key{ .name = "message" } ]] string_t		m_messageText;
	bool			m_drawText;
	[[= ks::reflect::Key{ .name = "developeronly" } ]] bool			m_bDeveloperOnly;
	bool			m_bEnabled;
};

LINK_ENTITY_TO_CLASS( point_message, CMessageEntity );

IMPLEMENT_REFLECT_DATAMAP( CMessageEntity )

static std::vector< CHandle< CMessageEntity > >	g_MessageEntities;

//-----------------------------------------
// Spawn
//-----------------------------------------
void CMessageEntity::Spawn( void )
{
	SetNextThink( gpGlobals->curtime + 0.1f );
	m_drawText = false;
	m_bDeveloperOnly = false;
	m_bEnabled = !HasSpawnFlags( SF_MESSAGE_DISABLED );
	//m_debugOverlays |= OVERLAY_TEXT_BIT;		// make sure we always show the text
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMessageEntity::Activate( void )
{
	BaseClass::Activate();

	CHandle< CMessageEntity > h;
	h = this;
	g_MessageEntities.push_back( h );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMessageEntity::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();

	CHandle< CMessageEntity > h;
	h = this;
	ks::erase_first( g_MessageEntities, h );

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------
// Think
//-----------------------------------------
void CMessageEntity::Think( void )
{
	SetNextThink( gpGlobals->curtime + 0.1f );

	// check for player distance
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

	if ( !pPlayer || ( pPlayer->GetFlags() & FL_NOTARGET ) )
		return;

	Vector worldTargetPosition = pPlayer->EyePosition();

	// bail if player is too far away
	if ( (worldTargetPosition - GetAbsOrigin()).Length() > m_radius )
	{
		m_drawText = false;
		return;
	}

	// turn on text
	m_drawText = true;
}
	
//-------------------------------------------
//-------------------------------------------
void CMessageEntity::DrawOverlays(void) 
{
	if ( !m_drawText )
		return;

	if ( m_bDeveloperOnly && !g_pDeveloper->GetInt() )
		return;

	if ( !m_bEnabled )
		return;

	// display text if they are within range
	char tempstr[512];
	V_snprintf( tempstr, sizeof(tempstr), "%s", STRING(m_messageText) );
	EntityText( 0, tempstr, 0);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMessageEntity::InputEnable( inputdata_t &inputdata )
{
	m_bEnabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMessageEntity::InputDisable( inputdata_t &inputdata )
{
	m_bEnabled = false;
}

// This is a hack to make point_message stuff appear in developer 0 release builds
//  for now
void DrawMessageEntities()
{
	int c = std::ssize( g_MessageEntities );
	for ( int i = c - 1; i >= 0; i-- )
	{
		CMessageEntity *me = g_MessageEntities[ i ];
		if ( !me )
		{
			g_MessageEntities.erase( g_MessageEntities.begin() + i );
			continue;
		}

		me->DrawOverlays();
	}
}
