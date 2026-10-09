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
#include "te.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "basetempentity.h"
#include "te_basebeam.h"

extern int	g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud

//-----------------------------------------------------------------------------
// Purpose: Dispatches a beam ring between two entities
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEBeamRingPoint" } ]]
      CTEBeamRingPoint : public CTEBaseBeam
{
public:
	DECLARE_CLASS( CTEBeamRingPoint, CTEBaseBeam );
	DECLARE_SERVERCLASS();

					CTEBeamRingPoint( const char *name );
	virtual			~CTEBeamRingPoint( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );

public:
	CNetworkVector( m_vecCenter, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( float, m_flStartRadius, [[= ks::reflect::Net{ .bits = 16, .low = 0.0f, .high = 4096.0f, .flags = SPROP_ROUNDUP } ]] );
	CNetworkVar( float, m_flEndRadius, [[= ks::reflect::Net{ .bits = 16, .low = 0.0f, .high = 4096.0f, .flags = SPROP_ROUNDUP } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBeamRingPoint::CTEBeamRingPoint( const char *name ) :
	CTEBaseBeam( name )
{
	m_vecCenter.Init();
	m_flStartRadius = 0.0f;
	m_flEndRadius = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamRingPoint::~CTEBeamRingPoint( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEBeamRingPoint::Test( const Vector& current_origin, const QAngle& current_angles )
{
	m_vecCenter		= current_origin;
	m_flEndRadius	= 256.0f;
	m_flStartRadius	= 16.0f;

	m_nModelIndex	= g_sModelIndexSmoke;
	m_nStartFrame	= 0;
	m_nFrameRate	= 2;
	m_fLife			= 10.0;
	m_fWidth		= 2.0;
	m_fAmplitude	= 1;
	r				= 255;
	g				= 255;
	b				= 0;
	a				= 127;
	m_nSpeed		= 5;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}


IMPLEMENT_REFLECT_SERVERCLASS( CTEBeamRingPoint, DT_TEBeamRingPoint )


// Singleton to fire TEBeamRingPoint objects
static CTEBeamRingPoint g_TEBeamRingPoint( "BeamRingPoint" );

void TE_BeamRingPoint( IRecipientFilter& filter, float delay,
	const Vector& center, float start_radius, float end_radius, int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, int spread, float amplitude, int r, int g, int b, int a, int speed, int flags )
{
	g_TEBeamRingPoint.m_vecCenter	= center;
	g_TEBeamRingPoint.m_flStartRadius = start_radius;
	g_TEBeamRingPoint.m_flEndRadius	= end_radius;
	g_TEBeamRingPoint.m_nModelIndex	= modelindex;
	g_TEBeamRingPoint.m_nHaloIndex	= haloindex;
	g_TEBeamRingPoint.m_nStartFrame	= startframe;
	g_TEBeamRingPoint.m_nFrameRate	= framerate;
	g_TEBeamRingPoint.m_fLife		= life;
	g_TEBeamRingPoint.m_fWidth		= width;
	g_TEBeamRingPoint.m_fEndWidth	= width;
	g_TEBeamRingPoint.m_nFadeLength	= 0;
	g_TEBeamRingPoint.m_fAmplitude	= amplitude;
	g_TEBeamRingPoint.m_nSpeed		= speed;
	g_TEBeamRingPoint.r				= r;
	g_TEBeamRingPoint.g				= g;
	g_TEBeamRingPoint.b				= b;
	g_TEBeamRingPoint.a				= a;
	g_TEBeamRingPoint.m_nFlags		= flags;

	// Send it over the wire
	g_TEBeamRingPoint.Create( filter, delay );
}
