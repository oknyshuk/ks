//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_basetempentity.h"
#include "c_te_legacytempents.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Bubble Trail TE
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEBubbleTrail" } ]]
      C_TEBubbleTrail : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEBubbleTrail, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

					C_TEBubbleTrail( void );
	virtual			~C_TEBubbleTrail( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecMins;
	[[= ks::reflect::Net{} ]] Vector			m_vecMaxs;
	[[= ks::reflect::Net{} ]] float			m_flWaterZ;
	[[= ks::reflect::Net{} ]] int				m_nModelIndex;
	[[= ks::reflect::Net{} ]] int				m_nCount;
	[[= ks::reflect::Net{} ]] float			m_fSpeed;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEBubbleTrail::C_TEBubbleTrail( void )
{
	m_vecMins.Init();
	m_vecMaxs.Init();
	m_flWaterZ = 0.0;
	m_nModelIndex = 0;
	m_nCount = 0;
	m_fSpeed = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEBubbleTrail::~C_TEBubbleTrail( void )
{
}

void TE_BubbleTrail( IRecipientFilter& filter, float delay,
	const Vector* mins, const Vector* maxs, float flWaterZ, int modelindex, int count, float speed )
{
	tempents->BubbleTrail( *mins, *maxs, flWaterZ, modelindex, count, speed );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void C_TEBubbleTrail::PostDataUpdate( DataUpdateType_t updateType )
{
	VPROF( "C_TEBubbleTrail::PostDataUpdate" );

	tempents->BubbleTrail( m_vecMins, m_vecMaxs, m_flWaterZ, m_nModelIndex, m_nCount, m_fSpeed );
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( C_TEBubbleTrail, DT_TEBubbleTrail, CTEBubbleTrail )
