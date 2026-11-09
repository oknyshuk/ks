// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

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
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "basetempentity.h"

//-----------------------------------------------------------------------------
// Purpose: Dispatches blood stream tempentity
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TERadioIcon" } ]]
      CTERadioIcon : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTERadioIcon, CBaseTempEntity );

					CTERadioIcon( const char *name );
	virtual			~CTERadioIcon( void );

	void Precache( void );
	
	DECLARE_SERVERCLASS();

public:

	CNetworkVar( int, m_iAttachToClient, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTERadioIcon::CTERadioIcon( const char *name ) :
	CBaseTempEntity( name )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTERadioIcon::~CTERadioIcon( void )
{
}

void CTERadioIcon::Precache( void )
{
	CBaseEntity::PrecacheModel("sprites/radio.vmt");
}

IMPLEMENT_REFLECT_SERVERCLASS( CTERadioIcon, DT_TERadioIcon )


// Singleton to fire StickyBolt objects
static CTERadioIcon g_TERadioIcon( "RadioIcon" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : filter - 
//			delay - 
//			pPlayer - 
//-----------------------------------------------------------------------------
void TE_RadioIcon( IRecipientFilter& filter, float delay, CBaseEntity *pPlayer )
{
	g_TERadioIcon.m_iAttachToClient = pPlayer->entindex();
	
	// Send it over the wire
	g_TERadioIcon.Create( filter, delay );
}
