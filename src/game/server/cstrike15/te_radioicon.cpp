//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "basetempentity.h"

//-----------------------------------------------------------------------------
// Purpose: Dispatches blood stream tempentity
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TERadioIcon" } ]]
      CTERadioIcon : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTERadioIcon, CBaseTempEntity );

					CTERadioIcon( const char *name );
	virtual			~CTERadioIcon( void );

	void Precache( void );
	
	DECLARE_SERVERCLASS();

public:

	CNetworkVar( int, m_iAttachToClient, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTERadioIcon::CTERadioIcon( const char *name ) :
	CBaseTempEntity( name )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTERadioIcon::~CTERadioIcon( void )
{
}

void CTERadioIcon::Precache( void )
{
	CBaseEntity::PrecacheModel("sprites/radio.vmt");
}

IMPLEMENT_REFLECT_SERVERCLASS( CTERadioIcon, DT_TERadioIcon )


// Singleton to fire StickyBolt objects
static CTERadioIcon g_TERadioIcon( "RadioIcon" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : filter - 
//			delay - 
//			pPlayer - 
//-----------------------------------------------------------------------------
void TE_RadioIcon( IRecipientFilter& filter, float delay, CBaseEntity *pPlayer )
{
	g_TERadioIcon.m_iAttachToClient = pPlayer->entindex();
	
	// Send it over the wire
	g_TERadioIcon.Create( filter, delay );
}
