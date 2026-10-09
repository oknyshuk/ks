// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

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
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "basetempentity.h"

extern int	g_sModelIndexBubbles;// holds the index for the bubbles model

//-----------------------------------------------------------------------------
// Purpose: Dispatches bubbles
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEBubbles" } ]]
      CTEBubbles : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEBubbles, CBaseTempEntity );

					CTEBubbles( const char *name );
	virtual			~CTEBubbles( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecMins, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecMaxs, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( float, m_fHeight, [[= ks::reflect::Net{ .bits = 17, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER } ]] );
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( int, m_nCount, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_fSpeed, [[= ks::reflect::Net{ .bits = 17, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBubbles::CTEBubbles( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecMins.Init();
	m_vecMaxs.Init();
	m_fHeight = 0.0;
	m_nModelIndex = 0;
	m_nCount = 0;
	m_fSpeed = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBubbles::~CTEBubbles( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEBubbles::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_vecMins = current_origin;
	
	Vector forward;

	m_vecMins.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecMins, 100.0, forward, m_vecMins.GetForModify() );

	m_vecMaxs = m_vecMins + Vector( 256, 256, 256 );

	m_fSpeed = 2;
	m_nCount = 50;
	m_fHeight = 256;

	m_nModelIndex = g_sModelIndexBubbles;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEBubbles, DT_TEBubbles )


// Singleton to fire TEBubbles objects
static CTEBubbles g_TEBubbles( "Bubbles" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*mins - 
//			*maxs - 
//			height - 
//			modelindex - 
//			count - 
//			speed - 
//-----------------------------------------------------------------------------
void TE_Bubbles( IRecipientFilter& filter, float delay,
	const Vector* mins, const Vector* maxs, float height, int modelindex, int count, float speed )
{
	g_TEBubbles.m_vecMins = *mins;
	g_TEBubbles.m_vecMaxs = *maxs;
	g_TEBubbles.m_fHeight = height;
	g_TEBubbles.m_nModelIndex = modelindex;
	g_TEBubbles.m_nCount = count;
	g_TEBubbles.m_fSpeed = speed;

	// Send it over the wire
	g_TEBubbles.Create( filter, delay );
}
