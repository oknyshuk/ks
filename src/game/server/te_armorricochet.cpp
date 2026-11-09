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

//-----------------------------------------------------------------------------
// Purpose: Dispatches sparks
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEMetalSparks", .base = false } ]]
      CTEMetalSparks : public CBaseTempEntity
{
DECLARE_CLASS( CTEMetalSparks, CBaseTempEntity );

public:
					CTEMetalSparks( const char *name );
	virtual			~CTEMetalSparks( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );

	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecPos, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecDir, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEMetalSparks::CTEMetalSparks( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecPos.Init();
	m_vecDir.Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEMetalSparks::~CTEMetalSparks( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEMetalSparks::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_vecPos = current_origin;
	
	AngleVectors( current_angles, &m_vecDir.GetForModify() );
	
	Vector forward;

	m_vecPos += Vector( 0, 0, 24 );

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecPos, 100.0, forward, m_vecPos.GetForModify() );
	
	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEMetalSparks, DT_TEMetalSparks )

// Singleton to fire TEMetalSparks objects
static CTEMetalSparks g_TEMetalSparks( "Metal Sparks" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*pos - 
//			scale - 
//-----------------------------------------------------------------------------
void TE_MetalSparks( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* dir )
{
	g_TEMetalSparks.m_vecPos = *pos;
	g_TEMetalSparks.m_vecDir = *dir;

	contract_assert( dir->Length() < 1.01 ); // make sure it's a normal

	// Send it over the wire
	g_TEMetalSparks.Create( filter, delay );
}

class [[= ks::reflect::NetTable{ .name = "DT_TEArmorRicochet" } ]]
      CTEArmorRicochet : public CTEMetalSparks
{
DECLARE_CLASS( CTEArmorRicochet, CTEMetalSparks );

public:
	CTEArmorRicochet( const char *name ) : CTEMetalSparks(name) {}
	DECLARE_SERVERCLASS();
};

IMPLEMENT_REFLECT_SERVERCLASS( CTEArmorRicochet, DT_TEArmorRicochet )

static CTEArmorRicochet g_TEArmorRicochet( "Armor Ricochet" );
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*pos - 
//			scale - 
//-----------------------------------------------------------------------------
void TE_ArmorRicochet( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* dir )
{
	g_TEArmorRicochet.m_vecPos = *pos;
	g_TEArmorRicochet.m_vecDir = *dir;

	// Send it over the wire
	g_TEArmorRicochet.Create( filter, delay );
}
