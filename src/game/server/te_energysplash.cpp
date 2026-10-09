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
#include "te_particlesystem.h"

//-----------------------------------------------------------------------------
// Purpose: Dispatches energy splashes
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEEnergySplash", .base = false } ]]
      CTEEnergySplash : public CBaseTempEntity
{
DECLARE_CLASS( CTEEnergySplash, CBaseTempEntity );

public:
					CTEEnergySplash( const char *name );
	virtual			~CTEEnergySplash( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecPos, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecDir, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( bool, m_bExplosive, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEEnergySplash::CTEEnergySplash( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecPos.Init();
	m_vecDir.Init();
	m_bExplosive = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEEnergySplash::~CTEEnergySplash( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEEnergySplash::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_vecPos = current_origin;
	
	AngleVectors( current_angles, &m_vecDir.GetForModify() );
	
	Vector forward;

	m_vecPos.GetForModify()[2] += 24;

	forward = m_vecDir;
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecPos, 100.0, forward, m_vecPos.GetForModify() );

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEEnergySplash, DT_TEEnergySplash )

// Singleton to fire TEEnergySplash objects
static CTEEnergySplash g_TEEnergySplash( "Energy Splash" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*pos - 
//			scale - 
//-----------------------------------------------------------------------------
void TE_EnergySplash( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* dir, bool bExplosive )
{
	g_TEEnergySplash.m_vecPos = *pos;
	g_TEEnergySplash.m_vecDir = *dir;
	g_TEEnergySplash.m_bExplosive = bExplosive;

	// Send it over the wire
	g_TEEnergySplash.Create( filter, delay );
}
