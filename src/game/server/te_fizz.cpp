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
#include "basetempentity.h"

//-----------------------------------------------------------------------------
// Purpose: Dispatches Fizz tempentity
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEFizz" } ]]
      CTEFizz : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEFizz, CBaseTempEntity );

					CTEFizz( const char *name );
	virtual			~CTEFizz( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );

	virtual void	Precache( void );

	DECLARE_SERVERCLASS();

public:
	CNetworkVar( int, m_nEntity, [[= ks::reflect::Net{ .bits = MAX_EDICT_BITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( int, m_nDensity, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nCurrent, [[= ks::reflect::Net{ .bits = 16 } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEFizz::CTEFizz( const char *name ) :
	CBaseTempEntity( name )
{
	m_nEntity = 0;
	m_nModelIndex = 0;
	m_nDensity = 0;
	m_nCurrent = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEFizz::~CTEFizz( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEFizz::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_nModelIndex = CBaseEntity::PrecacheModel( "sprites/bubble.vmt" );;
	m_nDensity = 200;
	m_nEntity = 1;
	m_nCurrent = 100;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEFizz::Precache( void )
{
	CBaseEntity::PrecacheModel( "sprites/bubble.vmt" );
}


IMPLEMENT_REFLECT_SERVERCLASS( CTEFizz, DT_TEFizz )


// Singleton to fire TEFizz objects
static CTEFizz g_TEFizz( "Fizz" );

void TE_Fizz( IRecipientFilter& filter, float delay,
	const CBaseEntity *entity, int modelindex, int density, int current )
{
	contract_assert( entity );

	g_TEFizz.m_nEntity		= ENTINDEX( (edict_t *)entity->edict() );
	g_TEFizz.m_nModelIndex	= modelindex;	
	g_TEFizz.m_nDensity		= density;
	g_TEFizz.m_nCurrent		= current;

	// Send it over the wire
	g_TEFizz.Create( filter, delay );
}
