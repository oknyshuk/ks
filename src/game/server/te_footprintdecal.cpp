//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
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

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Dispatches footprint decal tempentity
//-----------------------------------------------------------------------------

#define FOOTPRINT_DECAY_TIME 3.0f

class [[= ks::reflect::NetTable{ .name = "DT_TEFootprintDecal" } ]]
      CTEFootprintDecal : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEFootprintDecal, CBaseTempEntity );

					CTEFootprintDecal( const char *name );
	virtual			~CTEFootprintDecal( void );

	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVector( m_vecDirection, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );											
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
	Assert( origin );
	g_TEFootprintDecal.m_vecOrigin = *origin;
	g_TEFootprintDecal.m_vecDirection	= *right;
	g_TEFootprintDecal.m_nEntity		= entity;	
	g_TEFootprintDecal.m_nIndex			= index;
	g_TEFootprintDecal.m_chMaterialType	= materialType;

	VectorNormalize(g_TEFootprintDecal.m_vecDirection.GetForModify());

	// Send it over the wire
	g_TEFootprintDecal.Create( filter, delay );
}
