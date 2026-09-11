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

extern int	g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud

//-----------------------------------------------------------------------------
// Purpose: Dispatches Sprite Spray tempentity
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TESpriteSpray" } ]]
      CTESpriteSpray : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTESpriteSpray, CBaseTempEntity );

					CTESpriteSpray( const char *name );
	virtual			~CTESpriteSpray( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVector( m_vecDirection, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::ENC_MODELINDEX } ]] );
	CNetworkVar( int, m_nSpeed, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_fNoise, [[= ks::reflect::Net{ .bits = 8, .low = 0.0, .high = 2.56, .flags = SPROP_ROUNDDOWN } ]] );
	CNetworkVar( int, m_nCount, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTESpriteSpray::CTESpriteSpray( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_vecDirection.Init();
	m_nModelIndex = 0;
	m_fNoise = 0;
	m_nSpeed = 0;
	m_nCount = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTESpriteSpray::~CTESpriteSpray( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTESpriteSpray::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_nModelIndex = g_sModelIndexSmoke;
	m_fNoise = 0.8;
	m_nCount = 5;
	m_nSpeed = 30;
	m_vecOrigin = current_origin;
	
	Vector forward, right;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward, &right, nullptr );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin, 50.0, forward, m_vecOrigin.GetForModify() );
	VectorMA( m_vecOrigin, -25.0, right, m_vecOrigin.GetForModify() );

	m_vecDirection.Init( random->RandomInt( -100, 100 ), random->RandomInt( -100, 100 ), random->RandomInt( 0, 100 ) );

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_REFLECT_SERVERCLASS( CTESpriteSpray, DT_TESpriteSpray )


// Singleton to fire TESpriteSpray objects
static CTESpriteSpray g_TESpriteSpray( "Sprite Spray" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*pos - 
//			*dir - 
//			modelindex - 
//			speed - 
//			noise - 
//			count - 
//-----------------------------------------------------------------------------
void TE_SpriteSpray( IRecipientFilter& filter, float delay,
	const Vector *pos, const Vector *dir, int modelindex, int speed, float noise, int count )
{
	g_TESpriteSpray.m_vecOrigin		= *pos;
	g_TESpriteSpray.m_vecDirection	= *dir;
	g_TESpriteSpray.m_nModelIndex	= modelindex;	
	g_TESpriteSpray.m_nSpeed		= speed;
	g_TESpriteSpray.m_fNoise		= noise;
	g_TESpriteSpray.m_nCount		= count;

	// Send it over the wire
	g_TESpriteSpray.Create( filter, delay );
}
