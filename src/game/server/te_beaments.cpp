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
// Purpose: Dispatches a beam between two entities
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEBeamEnts" } ]]
      CTEBeamEnts : public CTEBaseBeam
{
public:
	DECLARE_CLASS( CTEBeamEnts, CTEBaseBeam );
	DECLARE_SERVERCLASS();

					CTEBeamEnts( const char *name );
	virtual			~CTEBeamEnts( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );


public:
	CNetworkVar( int, m_nStartEntity, [[= ks::reflect::Net{ .bits = 24, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nEndEntity, [[= ks::reflect::Net{ .bits = 24, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBeamEnts::CTEBeamEnts( const char *name ) :
	CTEBaseBeam( name )
{
	m_nStartEntity	= 0;
	m_nEndEntity	= 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamEnts::~CTEBeamEnts( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEBeamEnts::Test( const Vector& current_origin, const QAngle& current_angles )
{
	m_nStartEntity	= 1;
	m_nEndEntity	= 0;

	m_nModelIndex	= g_sModelIndexSmoke;
	m_nStartFrame	= 0;
	m_nFrameRate	= 10;
	m_fLife			= 2.0;
	m_fWidth		= 1.0;
	m_fAmplitude	= 1;
	r				= 127;
	g				= 63;
	b				= 0;
	a				= 150;
	m_nSpeed		= 1;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEBeamEnts, DT_TEBeamEnts )


// Singleton to fire TEBeamEnts objects
static CTEBeamEnts g_TEBeamEnts( "BeamEnts" );

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
void TE_BeamEnts( IRecipientFilter& filter, float delay,
	int	start, int end, int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, float endWidth, int fadeLength, float amplitude, int r, int g, int b, int a, int speed )
{
	g_TEBeamEnts.m_nStartEntity = (start & 0x0FFF) | ((1 & 0xF)<<12);
	g_TEBeamEnts.m_nEndEntity	= (end & 0x0FFF) | ((1 & 0xF)<<12);
	g_TEBeamEnts.m_nModelIndex	= modelindex;
	g_TEBeamEnts.m_nHaloIndex	= haloindex;
	g_TEBeamEnts.m_nStartFrame	= startframe;
	g_TEBeamEnts.m_nFrameRate	= framerate;
	g_TEBeamEnts.m_fLife		= life;
	g_TEBeamEnts.m_fWidth		= width;
	g_TEBeamEnts.m_fEndWidth	= endWidth;
	g_TEBeamEnts.m_nFadeLength	= fadeLength;
	g_TEBeamEnts.m_fAmplitude	= amplitude;
	g_TEBeamEnts.m_nSpeed		= speed;
	g_TEBeamEnts.r				= r;
	g_TEBeamEnts.g				= g;
	g_TEBeamEnts.b				= b;
	g_TEBeamEnts.a				= a;

	// Send it over the wire
	g_TEBeamEnts.Create( filter, delay );
}
