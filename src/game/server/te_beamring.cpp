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
#include "te_basebeam.h"

extern int	g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud

//-----------------------------------------------------------------------------
// Purpose: Dispatches a beam ring between two entities
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEBeamRing" } ]]
      CTEBeamRing : public CTEBaseBeam
{
public:
	DECLARE_CLASS( CTEBeamRing, CTEBaseBeam );
	DECLARE_SERVERCLASS();

					CTEBeamRing( const char *name );
	virtual			~CTEBeamRing( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
public:
	CNetworkVar( int, m_nStartEntity, [[= ks::reflect::Net{ .bits = MAX_EDICT_BITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nEndEntity, [[= ks::reflect::Net{ .bits = MAX_EDICT_BITS, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBeamRing::CTEBeamRing( const char *name ) :
	CTEBaseBeam( name )
{
	m_nStartEntity	= 0;
	m_nEndEntity	= 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamRing::~CTEBeamRing( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEBeamRing::Test( const Vector& current_origin, const QAngle& current_angles )
{
	m_nStartEntity	= 1;
	m_nEndEntity	= 0;

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


IMPLEMENT_REFLECT_SERVERCLASS( CTEBeamRing, DT_TEBeamRing )


// Singleton to fire TEBeamRing objects
static CTEBeamRing g_TEBeamRing( "BeamRing" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//				int	start - 
//			end - 
//			modelindex - 
//			startframe - 
//			framerate - 
//			msg_dest - 
//			delay - 
//			origin - 
//			recipient - 
//-----------------------------------------------------------------------------
void TE_BeamRing( IRecipientFilter& filter, float delay,
	int	start, int end, int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, int spread, float amplitude, int r, int g, int b, int a, int speed, int flags )
{
	g_TEBeamRing.m_nStartEntity = (start & 0x0FFF) | ((1 & 0xF)<<12);
	g_TEBeamRing.m_nEndEntity	= (end & 0x0FFF) | ((1 & 0xF)<<12);
	g_TEBeamRing.m_nModelIndex	= modelindex;
	g_TEBeamRing.m_nHaloIndex	= haloindex;
	g_TEBeamRing.m_nStartFrame	= startframe;
	g_TEBeamRing.m_nFrameRate	= framerate;
	g_TEBeamRing.m_fLife		= life;
	g_TEBeamRing.m_fWidth		= width;
	g_TEBeamRing.m_fEndWidth	= width;
	g_TEBeamRing.m_nFadeLength	= 0;
	g_TEBeamRing.m_fAmplitude	= amplitude;
	g_TEBeamRing.m_nSpeed		= speed;
	g_TEBeamRing.r				= r;
	g_TEBeamRing.g				= g;
	g_TEBeamRing.b				= b;
	g_TEBeamRing.a				= a;
	g_TEBeamRing.m_nFlags		= flags;

	// Send it over the wire
	g_TEBeamRing.Create( filter, delay );
}
