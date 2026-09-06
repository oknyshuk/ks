//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
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

//-----------------------------------------------------------------------------
// Purpose: create clientside physics prop, as breaks model if needed
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEPhysicsProp" } ]]
      CTEPhysicsProp : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEPhysicsProp, CBaseTempEntity );

					CTEPhysicsProp( const char *name );
	virtual			~CTEPhysicsProp( void );

	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkQAngle( m_angRotation,
	    [[= ks::reflect::Net{ .bits = 13, .enc = ks::reflect::ENC_ANGLE, .index = 0 } ]]
	    [[= ks::reflect::Net{ .bits = 13, .enc = ks::reflect::ENC_ANGLE, .index = 1 } ]]
	    [[= ks::reflect::Net{ .bits = 13, .enc = ks::reflect::ENC_ANGLE, .index = 2 } ]] );
	CNetworkVector( m_vecVelocity, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::ENC_MODELINDEX } ]] );
	CNetworkVar( int, m_nSkin, [[= ks::reflect::Net{ .bits = ANIMATION_SKIN_BITS } ]] );
	CNetworkVar( int, m_nFlags, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nEffects, [[= ks::reflect::Net{ .bits = EF_MAX_BITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkColor32( m_clrRender, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WIRE_SEND>{} ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEPhysicsProp::CTEPhysicsProp( const char *name ) :
	CBaseTempEntity( name )
{
	color32 white = {255, 255, 255, 255};

	m_vecOrigin.Init();
	m_angRotation.Init();
	m_vecVelocity.Init();
	m_nModelIndex		= 0;
	m_nSkin				= 0;
	m_nFlags			= 0;
	m_nEffects			= 0;
	m_clrRender			= white;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEPhysicsProp::~CTEPhysicsProp( void )
{
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEPhysicsProp, DT_TEPhysicsProp )

// Singleton to fire TEBreakModel objects
static CTEPhysicsProp s_TEPhysicsProp( "physicsprop" );

void TE_PhysicsProp( IRecipientFilter& filter, float delay,
	int modelindex, int skin, const Vector& pos, const QAngle &angles, const Vector& vel, int flags, int effects, color24 renderColor )
{
	color32 clrRenderConverted;
	clrRenderConverted.r = renderColor.r;
	clrRenderConverted.g = renderColor.g;
	clrRenderConverted.b = renderColor.b;
	clrRenderConverted.a = 255;

	s_TEPhysicsProp.m_vecOrigin		= pos;
	s_TEPhysicsProp.m_angRotation	= angles;
	s_TEPhysicsProp.m_vecVelocity	= vel;
	s_TEPhysicsProp.m_nModelIndex	= modelindex;	
	s_TEPhysicsProp.m_nSkin			= skin;
	s_TEPhysicsProp.m_nFlags		= flags;
	s_TEPhysicsProp.m_nEffects		= effects;
	s_TEPhysicsProp.m_clrRender		= clrRenderConverted;

	// Send it over the wire
	s_TEPhysicsProp.Create( filter, delay );
}
