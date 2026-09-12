//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
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
// Purpose: Dispatches sparks
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TESparks" } ]]
      CTESparks : public CTEParticleSystem
{
public:
	DECLARE_CLASS( CTESparks, CTEParticleSystem );
	DECLARE_SERVERCLASS();

					CTESparks( const char *name );
	virtual			~CTESparks( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	CNetworkVar( int, m_nMagnitude, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nTrailLength, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVector( m_vecDir, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTESparks::CTESparks( const char *name ) :
	BaseClass( name )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTESparks::~CTESparks( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTESparks::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_vecOrigin = current_origin;
	
	Vector forward;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	m_vecOrigin += forward * 100;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTESparks, DT_TESparks )


// Singleton to fire TESparks objects
static CTESparks g_TESparks( "Sparks" );

void TE_Sparks( IRecipientFilter& filter, float delay,
	const Vector *pos, int nMagnitude, int nTrailLength, const Vector *pDir )
{
	g_TESparks.m_vecOrigin = *pos;
	g_TESparks.m_nMagnitude = nMagnitude;
	g_TESparks.m_nTrailLength = nTrailLength;

	if ( pDir )
	{
		g_TESparks.m_vecDir = *pDir;
	}
	else
	{
		g_TESparks.m_vecDir = vec3_origin;
	}

	// Send it over the wire
	g_TESparks.Create( filter, delay );
}
