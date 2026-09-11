//====== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "basetempentity.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEClientProjectile" } ]]
      CTEClientProjectile : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEClientProjectile, CBaseTempEntity );

					CTEClientProjectile( const char *name );
	virtual			~CTEClientProjectile( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVector( m_vecVelocity, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::ENC_MODELINDEX } ]] );
	CNetworkVar( int, m_nLifeTime, [[= ks::reflect::Net{ .bits = 6, .flags = SPROP_UNSIGNED } ]] );
	CNetworkHandle( CBaseEntity, m_hOwner, [[= ks::reflect::Net{} ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEClientProjectile::CTEClientProjectile( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_vecVelocity.Init();
	m_nModelIndex = 0;
	m_nLifeTime = 0;
	m_hOwner = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEClientProjectile::~CTEClientProjectile( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEClientProjectile::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_vecOrigin = current_origin;
	
	Vector forward;
	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	m_vecVelocity = forward * 2048;

	m_nLifeTime = 5;
	m_hOwner = nullptr;
	
	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEClientProjectile, DT_TEClientProjectile )


// Singleton to fire TEClientProjectile objects
static CTEClientProjectile g_TEClientProjectile( "Client Projectile" );

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
void TE_ClientProjectile( IRecipientFilter& filter, float delay,
	const Vector* vecOrigin, const Vector* vecVelocity, int modelindex, int lifetime, CBaseEntity *pOwner )
{
	g_TEClientProjectile.m_vecOrigin = *vecOrigin;
	g_TEClientProjectile.m_vecVelocity = *vecVelocity;
	g_TEClientProjectile.m_nModelIndex = modelindex;
	g_TEClientProjectile.m_nLifeTime = lifetime;
	g_TEClientProjectile.m_hOwner = pOwner;

	// Send it over the wire
	g_TEClientProjectile.Create( filter, delay );
}
