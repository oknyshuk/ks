// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Create a muzzle flash temp ent

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
// Purpose: Dispatches user tracer stream tempentity
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEMuzzleFlash" } ]]
      CTEMuzzleFlash : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEMuzzleFlash, CBaseTempEntity );

	DECLARE_SERVERCLASS();

	CTEMuzzleFlash( const char *name );
	virtual			~CTEMuzzleFlash( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );

public:

	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkQAngle( m_vecAngles, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( float, m_flScale, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( int, m_nType, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEMuzzleFlash::CTEMuzzleFlash( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_vecAngles.Init();
	
	m_flScale	= 1.0f;
	m_nType		= 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEMuzzleFlash::~CTEMuzzleFlash( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEMuzzleFlash::Test( const Vector& current_origin, const QAngle& current_angles )
{
}


IMPLEMENT_REFLECT_SERVERCLASS( CTEMuzzleFlash, DT_TEMuzzleFlash )

// Singleton to fire TEMuzzleFlash objects
static CTEMuzzleFlash g_TEMuzzleFlash( "MuzzleFlash" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			origin - 
//			*recipient - 
//			*origin - 
//			*dir - 
//			scale - 
//			type - 
//-----------------------------------------------------------------------------
void TE_MuzzleFlash( IRecipientFilter& filter, float delay,
	const Vector &start, const QAngle &angles, float scale, int type )
{
	g_TEMuzzleFlash.m_vecOrigin		= start;
	g_TEMuzzleFlash.m_vecAngles		= angles;
	g_TEMuzzleFlash.m_flScale		= scale;
	g_TEMuzzleFlash.m_nType			= type;

	// Send it over the wire
	g_TEMuzzleFlash.Create( filter, delay );
}
