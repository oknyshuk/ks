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
#include "te_particlesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Dispatches energy splashes
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEEnergySplash", .base = false } ]]
      CTEEnergySplash : public CBaseTempEntity
{
DECLARE_CLASS( CTEEnergySplash, CBaseTempEntity );

public:
					CTEEnergySplash( const char *name );
	virtual			~CTEEnergySplash( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecPos, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVector( m_vecDir, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVar( bool, m_bExplosive, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEEnergySplash::CTEEnergySplash( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecPos.Init();
	m_vecDir.Init();
	m_bExplosive = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEEnergySplash::~CTEEnergySplash( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEEnergySplash::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_vecPos = current_origin;
	
	AngleVectors( current_angles, &m_vecDir.GetForModify() );
	
	Vector forward;

	m_vecPos.GetForModify()[2] += 24;

	forward = m_vecDir;
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecPos, 100.0, forward, m_vecPos.GetForModify() );

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEEnergySplash, DT_TEEnergySplash )

// Singleton to fire TEEnergySplash objects
static CTEEnergySplash g_TEEnergySplash( "Energy Splash" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*pos - 
//			scale - 
//-----------------------------------------------------------------------------
void TE_EnergySplash( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* dir, bool bExplosive )
{
	g_TEEnergySplash.m_vecPos = *pos;
	g_TEEnergySplash.m_vecDir = *dir;
	g_TEEnergySplash.m_bExplosive = bExplosive;

	// Send it over the wire
	g_TEEnergySplash.Create( filter, delay );
}
