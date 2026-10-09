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
// Purpose: Dispatches blood stream tempentity
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEBloodStream" } ]]
      CTEBloodStream : public CTEParticleSystem
{
public:
	DECLARE_CLASS( CTEBloodStream, CTEParticleSystem );

					CTEBloodStream( const char *name );
	virtual			~CTEBloodStream( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecDirection, [[= ks::reflect::Net{ .bits = 11, .low = -10.0, .high = 10.0, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( int, r, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, g, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, b, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, a, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nAmount, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBloodStream::CTEBloodStream( const char *name ) :
	BaseClass( name )
{
	m_vecDirection.Init();
	r = 0;
	g = 0;
	b = 0;
	a = 0;
	m_nAmount = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBloodStream::~CTEBloodStream( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEBloodStream::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	r = 247;
	g = 0;
	b = 0;
	a = 255;
	m_nAmount	= random->RandomInt(50, 150);
	m_vecOrigin = current_origin;
	
	Vector forward;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;	
	VectorNormalize( forward );

	m_vecOrigin += forward * 50;

	m_vecDirection = UTIL_RandomBloodVector();

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEBloodStream, DT_TEBloodStream )

// Singleton to fire TEBloodStream objects
static CTEBloodStream g_TEBloodStream( "Blood Stream" );

//-----------------------------------------------------------------------------
// Purpose: Creates a blood stream
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*org - 
//			*dir - 
//			r - 
//			g - 
//			b - 
//			a - 
//			amount - 
//-----------------------------------------------------------------------------
void TE_BloodStream( IRecipientFilter& filter, float delay,
	const Vector* org, const Vector* dir, int r, int g, int b, int a, int amount )
{
	g_TEBloodStream.m_vecOrigin = *org;
	g_TEBloodStream.m_vecDirection = *dir;	
	g_TEBloodStream.r = r;
	g_TEBloodStream.g = g;
	g_TEBloodStream.b = b;
	g_TEBloodStream.a = a;
	g_TEBloodStream.m_nAmount = amount;

	// Send it over the wire
	g_TEBloodStream.Create( filter, delay );
}
