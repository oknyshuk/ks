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
// Purpose: Dispatches footprint decal tempentity
//-----------------------------------------------------------------------------

constexpr auto FOOTPRINT_DECAY_TIME = 3.0f;

class [[= ks::reflect::NetTable{ .name = "DT_TEFootprintDecal" } ]]
      CTEFootprintDecal : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEFootprintDecal, CBaseTempEntity );

					CTEFootprintDecal( const char *name );
	virtual			~CTEFootprintDecal( void );

	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecDirection, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );											
	CNetworkVar( int, m_nEntity, [[= ks::reflect::Net{ .bits = 11, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nIndex, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( unsigned char, m_chMaterialType, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
};

IMPLEMENT_REFLECT_SERVERCLASS( CTEFootprintDecal, DT_TEFootprintDecal )


// Singleton to fire TEFootprintDecal objects
static CTEFootprintDecal g_TEFootprintDecal( "Footprint Decal" );

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------

CTEFootprintDecal::CTEFootprintDecal( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_nEntity = 0;
	m_nIndex = 0;
	m_chMaterialType = 'C';
}

CTEFootprintDecal::~CTEFootprintDecal( void )
{
}

//-----------------------------------------------------------------------------
// places a footprint decal 
//-----------------------------------------------------------------------------
								    
void TE_FootprintDecal( IRecipientFilter& filter, float delay, 
					    const Vector *origin, const Vector *right, int entity, int index, 
					    unsigned char materialType )
{
	contract_assert( origin );
	g_TEFootprintDecal.m_vecOrigin = *origin;
	g_TEFootprintDecal.m_vecDirection	= *right;
	g_TEFootprintDecal.m_nEntity		= entity;	
	g_TEFootprintDecal.m_nIndex			= index;
	g_TEFootprintDecal.m_chMaterialType	= materialType;

	VectorNormalize(g_TEFootprintDecal.m_vecDirection.GetForModify());

	// Send it over the wire
	g_TEFootprintDecal.Create( filter, delay );
}
