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

//-----------------------------------------------------------------------------
// Purpose: Dispatches BSP decal tempentity
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEProjectedDecal" } ]]
      CTEProjectedDecal : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEProjectedDecal, CBaseTempEntity );

					CTEProjectedDecal( const char *name );
	virtual			~CTEProjectedDecal( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( int, m_nIndex, [[= ks::reflect::Net{ .bits = 9, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_flDistance, [[= ks::reflect::Net{ .bits = 10, .low = 0, .high = 1024, .flags = SPROP_ROUNDUP } ]] );
	CNetworkQAngle( m_angRotation, [[= ks::reflect::Net{ .bits = 10, .enc = ks::reflect::WireEnc::QAngles } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEProjectedDecal::CTEProjectedDecal( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_angRotation.Init();
	m_flDistance = 64.0f;
	m_nIndex = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEProjectedDecal::~CTEProjectedDecal( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEProjectedDecal::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_flDistance = 1024.0f;
	m_nIndex = 0;
	m_vecOrigin = current_origin;
	m_angRotation = current_angles;

	Vector vecEnd;
	
	Vector forward;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin, 24.0, forward, m_vecOrigin.GetForModify() );

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEProjectedDecal, DT_TEProjectedDecal )


// Singleton to fire TEBSPDecal objects
static CTEProjectedDecal g_TEProjectedDecal( "Projected Decal" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*pos - 
//			entity - 
//			index - 
//			modelindex - 
//-----------------------------------------------------------------------------
void TE_ProjectDecal( IRecipientFilter& filter, float delay,
	const Vector* pos, const QAngle *angles, float distance, int index )
{
	g_TEProjectedDecal.m_vecOrigin		= *pos;
	g_TEProjectedDecal.m_angRotation	= *angles;
	g_TEProjectedDecal.m_flDistance		= distance;	
	g_TEProjectedDecal.m_nIndex			= index;

	// Send it over the wire
	g_TEProjectedDecal.Create( filter, delay );
}
