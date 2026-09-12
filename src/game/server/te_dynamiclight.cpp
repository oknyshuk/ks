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
// Purpose: Displays a dynamic light
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEDynamicLight" } ]]
      CTEDynamicLight : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEDynamicLight, CBaseTempEntity );

					CTEDynamicLight( const char *name );
	virtual			~CTEDynamicLight( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );

	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( float, m_fRadius, [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 2560.0, .flags = SPROP_ROUNDUP } ]] );
	CNetworkVar( int, r, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, g, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, b, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, exponent, [[= ks::reflect::Net{ .bits = 8 } ]] );
	CNetworkVar( float, m_fTime, [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 25.6, .flags = SPROP_ROUNDDOWN } ]] );
	CNetworkVar( float, m_fDecay, [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 2560.0, .flags = SPROP_ROUNDDOWN } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEDynamicLight::CTEDynamicLight( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	r = 0;
	g = 0;
	b = 0;
	exponent = 0;
	m_fRadius = 0.0;
	m_fTime = 0.0;
	m_fDecay = 0.0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEDynamicLight::~CTEDynamicLight( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEDynamicLight::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	r = 255;
	g = 255;
	b = 63;
	m_vecOrigin = current_origin;

	m_fRadius = 200;
	m_fTime = 2.0;
	m_fDecay = 0.0;
	
	Vector forward;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin, 50.0, forward, m_vecOrigin.GetForModify() );

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEDynamicLight, DT_TEDynamicLight )


// Singleton
static CTEDynamicLight g_TEDynamicLight( "Dynamic Light" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*org - 
//			r - 
//			g - 
//			b - 
//			radius - 
//			time - 
//			decay - 
//-----------------------------------------------------------------------------
void TE_DynamicLight( IRecipientFilter& filter, float delay,
	const Vector* org, int r, int g, int b, int exponent, float radius, float time, float decay )
{
	// Set up parameters
	g_TEDynamicLight.m_vecOrigin = *org;
	g_TEDynamicLight.r = r;
	g_TEDynamicLight.g = g;
	g_TEDynamicLight.b = b;
	g_TEDynamicLight.exponent = exponent;
	g_TEDynamicLight.m_fRadius = radius;
	g_TEDynamicLight.m_fTime = time;
	g_TEDynamicLight.m_fDecay = decay;

	// Create it
	g_TEDynamicLight.Create( filter, delay );
}
