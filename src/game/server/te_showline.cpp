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

//-----------------------------------------------------------------------------
// Purpose: Dispatches line
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEShowLine" } ]]
      CTEShowLine : public CTEParticleSystem
{
public:
	DECLARE_CLASS( CTEShowLine, CTEParticleSystem );
	DECLARE_SERVERCLASS();

					CTEShowLine( const char *name );
	virtual			~CTEShowLine( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );


public:
	CNetworkVector( m_vecEnd, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEShowLine::CTEShowLine( const char *name ) :
	BaseClass( name )
{
	m_vecEnd.Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEShowLine::~CTEShowLine( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEShowLine::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_vecOrigin = current_origin;
	
	Vector forward, right;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward, &right, nullptr );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin, 100.0, forward, m_vecEnd.GetForModify() );

	m_vecOrigin = m_vecEnd + right * -128;
	m_vecEnd += right * 128;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEShowLine, DT_TEShowLine )


// Singleton to fire TEShowLine objects
static CTEShowLine g_TEShowLine( "Show Line" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*start - 
//			*end - 
//-----------------------------------------------------------------------------
void TE_ShowLine( IRecipientFilter& filter, float delay,
	const Vector* start, const Vector* end )
{
	g_TEShowLine.m_vecOrigin = *start;
	g_TEShowLine.m_vecEnd = *end;	

	// Send it over the wire
	g_TEShowLine.Create( filter, delay );
}
