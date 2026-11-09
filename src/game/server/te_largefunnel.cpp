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
#include "te_particlesystem.h"

extern int	g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud

//-----------------------------------------------------------------------------
// Purpose: Dispatches smoke tempentity
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TELargeFunnel" } ]]
      CTELargeFunnel : public CTEParticleSystem
{
public:
	DECLARE_CLASS( CTELargeFunnel, CTEParticleSystem );
	DECLARE_SERVERCLASS();

					CTELargeFunnel( const char *name );
	virtual			~CTELargeFunnel( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
public:
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( int, m_nReversed, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTELargeFunnel::CTELargeFunnel( const char *name ) :
	BaseClass( name )
{
	m_nModelIndex = 0;
	m_nReversed = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTELargeFunnel::~CTELargeFunnel( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTELargeFunnel::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_nModelIndex = g_sModelIndexSmoke;
	m_nReversed = 0;
	m_vecOrigin = current_origin;
	
	Vector forward, right;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward, &right, nullptr );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin.Get(), 50.0, forward, m_vecOrigin.GetForModify() );
	VectorMA( m_vecOrigin.Get(), 25.0, right, m_vecOrigin.GetForModify() );

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTELargeFunnel, DT_TELargeFunnel )


// Singleton to fire TELargeFunnel objects
static CTELargeFunnel g_TELargeFunnel( "Large Funnel" );

void TE_LargeFunnel( IRecipientFilter& filter, float delay,
	const Vector* pos, int modelindex, int reversed )
{
	g_TELargeFunnel.m_vecOrigin		= *pos;
	g_TELargeFunnel.m_nModelIndex	= modelindex;	
	g_TELargeFunnel.m_nReversed		= reversed;

	// Send it over the wire
	g_TELargeFunnel.Create( filter, delay );
}
