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

//-----------------------------------------------------------------------------
// Purpose: Dispatches a beam ring between two entities
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEBeamFollow" } ]]
      CTEBeamFollow : public CTEBaseBeam
{
	DECLARE_CLASS( CTEBeamFollow, CTEBaseBeam );
public:

	DECLARE_SERVERCLASS();

					CTEBeamFollow( const char *name );
	virtual			~CTEBeamFollow( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );

public:

	CNetworkVar( int, m_iEntIndex, [[= ks::reflect::Net{ .bits = 24, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBeamFollow::CTEBeamFollow( const char *name ) :
	CTEBaseBeam( name )
{
	m_iEntIndex = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBeamFollow::~CTEBeamFollow( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEBeamFollow::Test( const Vector& current_origin, const QAngle& current_angles )
{
	m_iEntIndex	= 1;
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEBeamFollow, DT_TEBeamFollow )


// Singleton to fire TEBeamEntPoint objects
static CTEBeamFollow g_TEBeamFollow( "BeamFollow" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : filter - 
//			delay - 
//			iEntIndex - 
//			modelIndex - 
//			modelindex - 
//			haloIndex - 
//			life - 
//			width - 
//			endWidth - 
//			fadeLength - 
//			r - 
//			g - 
//			b - 
//			a - 
//-----------------------------------------------------------------------------
void TE_BeamFollow( IRecipientFilter& filter, float delay,
	int iEntIndex, int modelIndex, int haloIndex, float life, float width, float endWidth, 
	float fadeLength,float r, float g, float b, float a )
{
	g_TEBeamFollow.m_iEntIndex		= (iEntIndex & 0x0FFF) | ((1 & 0xF)<<12);
	g_TEBeamFollow.m_nModelIndex	= modelIndex;
	g_TEBeamFollow.m_nHaloIndex		= haloIndex;
	g_TEBeamFollow.m_nStartFrame	= 0;
	g_TEBeamFollow.m_nFrameRate		= 0;
	g_TEBeamFollow.m_fLife			= life;
	g_TEBeamFollow.m_fWidth			= width;
	g_TEBeamFollow.m_fEndWidth		= endWidth;
	g_TEBeamFollow.m_nFadeLength	= fadeLength;
	g_TEBeamFollow.r				= r;
	g_TEBeamFollow.g				= g;
	g_TEBeamFollow.b				= b;
	g_TEBeamFollow.a				= a;

	// Send it over the wire
	g_TEBeamFollow.Create( filter, delay );
}
