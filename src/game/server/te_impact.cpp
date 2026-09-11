//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Send generic impact messages to the client for visualization
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
// Purpose: Dispatches Gunshot decal tempentity
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEImpact" } ]]
      CTEImpact : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEImpact, CBaseTempEntity );

	DECLARE_SERVERCLASS();

	CTEImpact( const char *name );
	virtual	~CTEImpact();

	void Precache( void );
	void Test( const Vector& current_origin, const Vector& current_normal );

public:

	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVector( m_vecNormal, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );	//NOTENOTE: In a multi-play setup we'll probably want non-oriented effects for bandwidth
	CNetworkVar( int, m_iType, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : 
//-----------------------------------------------------------------------------
CTEImpact::CTEImpact( const char *name ) : CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_vecNormal.Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEImpact::~CTEImpact( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEImpact::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEImpact::Test( const Vector& current_origin, const Vector& current_normal )
{
}


//Server class implementation
IMPLEMENT_REFLECT_SERVERCLASS( CTEImpact, DT_TEImpact )

// Singleton to fire TEImpact objects
static CTEImpact g_TEImpact( "Impact" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//-----------------------------------------------------------------------------
void TE_Impact( IRecipientFilter& filter, float delay )
{
	g_TEImpact.Create( filter, delay );
}
