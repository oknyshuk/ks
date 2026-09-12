//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "baseentity.h"
#include "sendproxy.h"
#include "sun_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define LIGHTGLOW_MAXDIST_BITS		16
#define LIGHTGLOW_MAXDIST_MAX_VALUE	((1 << LIGHTGLOW_MAXDIST_BITS)-1)

#define LIGHTGLOW_OUTERMAXDIST_BITS	16
#define LIGHTGLOW_OUTERMAXDIST_MAX_VALUE	((1 << LIGHTGLOW_OUTERMAXDIST_BITS)-1)

void SendProxy_Angles( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );

class [[= ks::reflect::NetTable{ .name = "DT_LightGlow", .base = false } ]]
      [[= ks::reflect::From<"m_clrRender", ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED }, SendProxy_Color32ToInt32>{} ]]
      [[= ks::reflect::From<"m_spawnflags", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_vecOrigin", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector }>{} ]]
      [[= ks::reflect::From<"m_angRotation", ks::reflect::Net{ .bits = 13, .enc = ks::reflect::WireEnc::QAngles }, SendProxy_Angles>{} ]]
      [[= ks::reflect::From<"m_hMoveParent", ks::reflect::Net{ .wire = "moveparent" }>{} ]]
      CLightGlow : public CBaseEntity
{
public:
	DECLARE_CLASS( CLightGlow, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

					CLightGlow();
					
	virtual void	Spawn( void );
	virtual void	Activate( void );
	virtual int		UpdateTransmitState( void );

	[[= ks::reflect::Input{ .name = "Color", .type = FIELD_COLOR32 } ]] void InputColor(inputdata_t &data);

public:
	CNetworkVar( int, m_nHorizontalSize, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "HorizontalGlowSize" } ]] );
	CNetworkVar( int, m_nVerticalSize, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "VerticalGlowSize" } ]] );
	CNetworkVar( int, m_nMinDist, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "MinDist" } ]] );
	CNetworkVar( int, m_nMaxDist, [[= ks::reflect::Net{ .bits = LIGHTGLOW_MAXDIST_BITS, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "MaxDist" } ]] );
	CNetworkVar( int, m_nOuterMaxDist, [[= ks::reflect::Net{ .bits = LIGHTGLOW_OUTERMAXDIST_BITS, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "OuterMaxDist" } ]] );

	CNetworkVar( float, m_flGlowProxySize, [[= ks::reflect::Net{ .bits = 6, .low = 0.0f, .high = 64.0f, .flags = SPROP_ROUNDUP } ]] [[= ks::reflect::Key{ .name = "GlowProxySize" } ]] );
	CNetworkVar( float, m_flHDRColorScale, [[= ks::reflect::Net{ .bits = 0, .low = 0.0f, .high = 100.0f, .flags = SPROP_NOSCALE, .wire = "HDRColorScale" } ]] [[= ks::reflect::Key{ .name = "HDRColorScale" } ]] );
};

extern void SendProxy_Angles( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );

IMPLEMENT_REFLECT_SERVERCLASS( CLightGlow, DT_LightGlow )

LINK_ENTITY_TO_CLASS( env_lightglow, CLightGlow );

IMPLEMENT_REFLECT_DATAMAP( CLightGlow )

//-----------------------------------------------------------------------------
// Constructor 
//-----------------------------------------------------------------------------
CLightGlow::CLightGlow( void )
{
	m_nHorizontalSize = 0.0f;
	m_nVerticalSize = 0.0f;
	m_nMinDist = 0.0f;
	m_nMaxDist = 0.0f;

	m_flGlowProxySize = 2.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLightGlow::Spawn( void )
{
	BaseClass::Spawn();

	// No model but we still need to force this!
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
}

//-----------------------------------------------------------------------------
// Purpose: Always transmit light glows to clients to avoid spikes as we enter
//			or leave PVS. Done because we often have many glows in an area.
//-----------------------------------------------------------------------------
int CLightGlow::UpdateTransmitState( void )
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLightGlow::Activate()
{
	BaseClass::Activate();

	if ( m_nMaxDist > LIGHTGLOW_MAXDIST_MAX_VALUE )
	{
		Warning( "env_lightglow maxdist too large (%d should be %d).\n", m_nMaxDist.Get(), LIGHTGLOW_MAXDIST_MAX_VALUE );
		m_nMaxDist = LIGHTGLOW_MAXDIST_MAX_VALUE;
	}

	if ( m_nOuterMaxDist > LIGHTGLOW_OUTERMAXDIST_MAX_VALUE )
	{
		Warning( "env_lightglow outermaxdist too large (%d should be %d).\n", m_nOuterMaxDist.Get(), LIGHTGLOW_OUTERMAXDIST_MAX_VALUE );
		m_nOuterMaxDist = LIGHTGLOW_OUTERMAXDIST_MAX_VALUE;
	}
}

void CLightGlow::InputColor(inputdata_t &inputdata)
{
	m_clrRender = inputdata.value.Color32();
}
