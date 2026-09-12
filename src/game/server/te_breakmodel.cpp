//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
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
#include "vstdlib/random.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Dispatches model smash pieces
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEBreakModel" } ]]
      CTEBreakModel : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEBreakModel, CBaseTempEntity );

					CTEBreakModel( const char *name );
	virtual			~CTEBreakModel( void );

	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecVelocity, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkQAngle( m_angRotation, [[= ks::reflect::Net{ .bits = 13, .index = 2 } ]]  [[= ks::reflect::Net{ .bits = 13, .index = 1 } ]]  [[= ks::reflect::Net{ .bits = 13, .index = 0 } ]] );
	CNetworkVar( int, m_nRandomization, [[= ks::reflect::Net{ .bits = 9, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( int, m_nCount, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_fTime, [[= ks::reflect::Net{ .bits = 10, .low = 0, .high = 102.4 } ]] );
	CNetworkVar( int, m_nFlags, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBreakModel::CTEBreakModel( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_vecSize.Init();
	m_vecVelocity.Init();
	m_angRotation.Init();
	m_nModelIndex		= 0;
	m_nRandomization	= 0;
	m_nCount			= 0;
	m_fTime				= 0.0;
	m_nFlags			= 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEBreakModel::~CTEBreakModel( void )
{
}


IMPLEMENT_REFLECT_SERVERCLASS( CTEBreakModel, DT_TEBreakModel )

// Singleton to fire TEBreakModel objects
static CTEBreakModel g_TEBreakModel( "breakmodel" );

void TE_BreakModel( IRecipientFilter& filter, float delay,
	const Vector& pos, const QAngle& angles, const Vector& size, const Vector& vel, int modelindex, int randomization,
	int count, float time, int flags )
{
	g_TEBreakModel.m_vecOrigin		= pos;
	g_TEBreakModel.m_angRotation	= angles;
	g_TEBreakModel.m_vecSize		= size;
	g_TEBreakModel.m_vecVelocity	= vel;
	g_TEBreakModel.m_nModelIndex	= modelindex;	
	g_TEBreakModel.m_nRandomization	= randomization;
	g_TEBreakModel.m_nCount			= count;
	g_TEBreakModel.m_fTime			= time;
	g_TEBreakModel.m_nFlags			= flags;

	// Send it over the wire
	g_TEBreakModel.Create( filter, delay );
}
