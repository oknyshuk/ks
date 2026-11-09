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
class [[= ks::reflect::NetTable{ .name = "DT_TEKillPlayerAttachments" } ]]
      CTEKillPlayerAttachments : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEKillPlayerAttachments, CBaseTempEntity );

					CTEKillPlayerAttachments( const char *name );
	virtual			~CTEKillPlayerAttachments( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVar( int, m_nPlayer, [[= ks::reflect::Net{ .bits = 5, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEKillPlayerAttachments::CTEKillPlayerAttachments( const char *name ) :
	CBaseTempEntity( name )
{
	m_nPlayer = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEKillPlayerAttachments::~CTEKillPlayerAttachments( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEKillPlayerAttachments::Test( const Vector& current_origin, const QAngle& current_angles )
{
	m_nPlayer = 1;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}


IMPLEMENT_REFLECT_SERVERCLASS( CTEKillPlayerAttachments, DT_TEKillPlayerAttachments )


// Singleton to fire TEKillPlayerAttachments objects
static CTEKillPlayerAttachments g_TEKillPlayerAttachments( "KillPlayerAttachments" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			player - 
//-----------------------------------------------------------------------------
void TE_KillPlayerAttachments( IRecipientFilter& filter, float delay,
	int player )
{
	g_TEKillPlayerAttachments.m_nPlayer = player;

	// Send it over the wire
	g_TEKillPlayerAttachments.Create( filter, delay );
}
