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
// Purpose: Dispatches Sprite tempentity
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TESprite" } ]]
      CTESprite : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTESprite, CBaseTempEntity );

					CTESprite( const char *name );
	virtual			~CTESprite( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );

	virtual void	Precache( void );

	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( float, m_fScale, [[= ks::reflect::Net{ .bits = 8, .low = 0.0, .high = 25.6, .flags = SPROP_ROUNDDOWN } ]] );
	CNetworkVar( int, m_nBrightness, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTESprite::CTESprite( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_nModelIndex = 0;
	m_fScale = 0;
	m_nBrightness = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTESprite::~CTESprite( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTESprite::Precache( void )
{
	CBaseEntity::PrecacheModel("sprites/gunsmoke.vmt");
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTESprite::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_nModelIndex = CBaseEntity::PrecacheModel("sprites/gunsmoke.vmt");
	m_fScale = 0.8;
	m_nBrightness = 200;
	m_vecOrigin = current_origin;
	
	Vector forward, right;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward, &right, nullptr );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin, 50.0, forward, m_vecOrigin.GetForModify() );
	VectorMA( m_vecOrigin, -25.0, right, m_vecOrigin.GetForModify() );

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}


IMPLEMENT_REFLECT_SERVERCLASS( CTESprite, DT_TESprite )


// Singleton to fire TESprite objects
static CTESprite g_TESprite( "Sprite" );

void TE_Sprite( IRecipientFilter& filter, float delay,
	const Vector *pos, int modelindex, float size, int brightness )
{
	g_TESprite.m_vecOrigin		= *pos;
	g_TESprite.m_nModelIndex	= modelindex;	
	g_TESprite.m_fScale			= size;
	g_TESprite.m_nBrightness	= brightness;

	// Send it over the wire
	g_TESprite.Create( filter, delay );
}
