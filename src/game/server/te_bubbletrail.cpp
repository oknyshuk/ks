//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "basetempentity.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern int	g_sModelIndexBubbles;// holds the index for the bubbles model

enum
{
	BUBBLE_TRAIL_COUNT_BITS = 8,
	BUBBLE_TRAIL_MAX_COUNT = ( (1 << BUBBLE_TRAIL_COUNT_BITS) - 1 ),
};


//-----------------------------------------------------------------------------
// Purpose: Dispatches bubble trail
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEBubbleTrail" } ]]
      CTEBubbleTrail : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEBubbleTrail, CBaseTempEntity );

					CTEBubbleTrail( const char *name );
	virtual			~CTEBubbleTrail( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecMins, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVector( m_vecMaxs, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVar( float, m_flWaterZ, [[= ks::reflect::Net{ .bits = 17, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER } ]] );
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::ENC_MODELINDEX } ]] );
	CNetworkVar( int, m_nCount, [[= ks::reflect::Net{ .bits = BUBBLE_TRAIL_COUNT_BITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_fSpeed, [[= ks::reflect::Net{ .bits = 17, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBubbleTrail::CTEBubbleTrail( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecMins.Init();
	m_vecMaxs.Init();
	m_flWaterZ = 0.0;
	m_nModelIndex = 0;
	m_nCount = 0;
	m_fSpeed = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBubbleTrail::~CTEBubbleTrail( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEBubbleTrail::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_vecMins = current_origin;
	
	Vector forward;

	m_vecMins.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecMins, 100.0, forward, m_vecMins.GetForModify() );

	m_vecMaxs = m_vecMins + Vector( 256, 256, 256 );

	m_fSpeed = 8;
	m_nCount = 20;
	m_flWaterZ = 0;

	m_nModelIndex = g_sModelIndexBubbles;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEBubbleTrail, DT_TEBubbleTrail )


// Singleton to fire TEBubbleTrail objects
static CTEBubbleTrail g_TEBubbleTrail( "Bubble Trail" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*mins - 
//			*maxs - 
//			height - 
//			modelindex - 
//			count - 
//			speed - 
//-----------------------------------------------------------------------------
void TE_BubbleTrail( IRecipientFilter& filter, float delay,
	const Vector* mins, const Vector* maxs, float flWaterZ, int modelindex, int count, float speed )
{
	g_TEBubbleTrail.m_vecMins = *mins;
	g_TEBubbleTrail.m_vecMaxs = *maxs;
	g_TEBubbleTrail.m_flWaterZ = flWaterZ;
	g_TEBubbleTrail.m_nModelIndex = modelindex;
	g_TEBubbleTrail.m_nCount = MIN( count, ( int ) BUBBLE_TRAIL_MAX_COUNT );
	g_TEBubbleTrail.m_fSpeed = speed;

	// Send it over the wire
	g_TEBubbleTrail.Create( filter, delay );
}
