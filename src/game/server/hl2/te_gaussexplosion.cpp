//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "te_particlesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//=============================================================================
// Gauss explosion
//=============================================================================

class [[= ks::reflect::NetTable{ .name = "DT_TEGaussExplosion" } ]]
      CTEGaussExplosion : public CTEParticleSystem
{
public:
	DECLARE_CLASS( CTEGaussExplosion, CTEParticleSystem );
	DECLARE_SERVERCLASS();

					CTEGaussExplosion( const char *name );
	virtual			~CTEGaussExplosion( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles ) { };

	CNetworkVar( int, m_nType, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVector( m_vecDirection, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
};


CTEGaussExplosion::CTEGaussExplosion( const char *name ) : BaseClass( name )
{
	m_nType = 0;
	m_vecDirection.Init();
}

CTEGaussExplosion::~CTEGaussExplosion( void )
{
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEGaussExplosion, DT_TEGaussExplosion )

static CTEGaussExplosion g_TEGaussExplosion( "GaussExplosion" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &pos - 
//			&angles - 
//-----------------------------------------------------------------------------
void TE_GaussExplosion( IRecipientFilter& filter, float delay,
	const Vector &pos, const Vector &dir, int type )
{
	g_TEGaussExplosion.m_vecOrigin		= pos;
	g_TEGaussExplosion.m_vecDirection	= dir;
	g_TEGaussExplosion.m_nType			= type;

	//Send it
	g_TEGaussExplosion.Create( filter, delay );
}



