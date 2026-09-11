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

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Dispatches decal tempentity
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEDecal" } ]]
      CTEDecal : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEDecal, CBaseTempEntity );

					CTEDecal( const char *name );
	virtual			~CTEDecal( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVector( m_vecStart, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVar( int, m_nEntity, [[= ks::reflect::Net{ .bits = MAX_EDICT_BITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nHitbox, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nIndex, [[= ks::reflect::Net{ .bits = 9, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEDecal::CTEDecal( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_nEntity = 0;
	m_nIndex = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEDecal::~CTEDecal( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEDecal::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_nEntity = 0;
	m_nIndex = 0;
	m_vecOrigin = current_origin;

	Vector vecEnd;
	
	Vector forward;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin, 50.0, forward, m_vecOrigin.GetForModify() );
	VectorMA( m_vecOrigin, 1024.0, forward, vecEnd );

	trace_t tr;

	UTIL_TraceLine( m_vecOrigin, vecEnd, MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr );

	m_vecOrigin = tr.endpos;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}


IMPLEMENT_REFLECT_SERVERCLASS( CTEDecal, DT_TEDecal )


// Singleton to fire TEDecal objects
static CTEDecal g_TEDecal( "Entity Decal" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*pos - 
//			entity - 
//			index - 
//-----------------------------------------------------------------------------
void TE_Decal( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* start, int entity, int hitbox, int index )
{
	Assert( pos && start );
	g_TEDecal.m_vecOrigin	= *pos;
	g_TEDecal.m_vecStart	= *start;
	g_TEDecal.m_nEntity		= entity;	
	g_TEDecal.m_nHitbox		= hitbox;
	g_TEDecal.m_nIndex		= index;

	// Send it over the wire
	g_TEDecal.Create( filter, delay );
}
