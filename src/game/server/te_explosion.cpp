//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
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
//===========================================================================//
#include "cbase.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "te_particlesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern int	g_sModelIndexFireball;			// (in combatweapon.cpp) holds the index for the smoke cloud

//-----------------------------------------------------------------------------
// Purpose: Dispatches explosion tempentity
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEExplosion" } ]]
      CTEExplosion : public CTEParticleSystem
{
public:
	DECLARE_CLASS( CTEExplosion, CTEParticleSystem );
	DECLARE_SERVERCLASS();

					CTEExplosion( const char *name );
	virtual			~CTEExplosion( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	

public:
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::ENC_MODELINDEX } ]] );
	CNetworkVar( float, m_fScale, [[= ks::reflect::Net{ .bits = 9, .low = 0.0, .high = 51.2 } ]] );
	CNetworkVar( int, m_nFrameRate, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nFlags, [[= ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVector( m_vecNormal, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVar( unsigned char, m_chMaterialType, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nRadius, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nMagnitude, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEExplosion::CTEExplosion( const char *name ) :
	BaseClass( name )
{
	m_nModelIndex = 0;
	m_fScale = 0;
	m_nFrameRate = 0;
	m_nFlags = 0;
	m_vecNormal.Init();
	m_chMaterialType = 'C';
	m_nRadius = 0;
	m_nMagnitude = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEExplosion::~CTEExplosion( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEExplosion::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_nModelIndex = g_sModelIndexFireball;
	m_fScale = 0.5;
	m_nFrameRate = 15;
	m_nFlags = TE_EXPLFLAG_NONE;
	m_vecOrigin = current_origin;
	
	Vector forward;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	m_vecOrigin += forward * 50;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEExplosion, DT_TEExplosion )

// Singleton to fire TEExplosion objects
static CTEExplosion g_TEExplosion( "Explosion" );

void TE_Explosion( IRecipientFilter& filter, float delay,
	const Vector& pos, int modelindex, float scale, int framerate, int flags, int radius, int magnitude, const Vector& normal, unsigned char materialType )
{
	g_TEExplosion.m_vecOrigin		= pos;
	g_TEExplosion.m_nModelIndex		= modelindex;	
	g_TEExplosion.m_fScale			= scale;
	g_TEExplosion.m_nFrameRate		= framerate;
	g_TEExplosion.m_nFlags			= flags;
	g_TEExplosion.m_nRadius			= radius;
	g_TEExplosion.m_nMagnitude		= magnitude;

	// Make a copy here so we can re-use it below
	g_TEExplosion.m_vecNormal		= normal; // pNormal ? ( *pNormal ) : Vector( 0, 0, 1 );

	g_TEExplosion.m_chMaterialType	= materialType;

	// Send it over the wire
	g_TEExplosion.Create( filter, delay );
}
