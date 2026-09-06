//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
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
#include "c_te_basebeam.h"
#include "iviewrender_beams.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: BeamEntPoint TE
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEBeamEntPoint" } ]]
      C_TEBeamEntPoint : public C_TEBaseBeam
{
public:
	DECLARE_CLASS( C_TEBeamEntPoint, C_TEBaseBeam );
	DECLARE_CLIENTCLASS();

					C_TEBeamEntPoint( void );
	virtual			~C_TEBeamEntPoint( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] int				m_nStartEntity;
	[[= ks::reflect::Net{} ]] int				m_nEndEntity;
	[[= ks::reflect::Net{} ]] Vector			m_vecStartPoint;
	[[= ks::reflect::Net{} ]] Vector			m_vecEndPoint;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEBeamEntPoint::C_TEBeamEntPoint( void )
{
	m_nStartEntity	= 0;
	m_vecEndPoint.Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEBeamEntPoint::~C_TEBeamEntPoint( void )
{
}

void TE_BeamEntPoint( IRecipientFilter& filter, float delay,
	int	nStartEntity, const Vector *pStart, int nEndEntity, const Vector* pEnd, 
	int modelindex, int haloindex, int startframe, int framerate,
	float life, float width, float endWidth, int fadeLength, float amplitude, 
	int r, int g, int b, int a, int speed )
{
	beams->CreateBeamEntPoint( nStartEntity, pStart, nEndEntity, pEnd, 
		modelindex, haloindex, 0.0f, life,  width, endWidth, fadeLength, amplitude,
		a, 0.1 * (float)speed, startframe, 0.1f * (float)framerate, r, g, b );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void C_TEBeamEntPoint::PostDataUpdate( DataUpdateType_t updateType )
{
	beams->CreateBeamEntPoint( m_nStartEntity, &m_vecStartPoint, m_nEndEntity, &m_vecEndPoint, 
		m_nModelIndex, m_nHaloIndex, 0.0f,
		m_fLife,  m_fWidth, m_fEndWidth, m_nFadeLength, m_fAmplitude, a, 0.1 * m_nSpeed, 
		m_nStartFrame, 0.1 * m_nFrameRate, r, g, b );
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( C_TEBeamEntPoint, DT_TEBeamEntPoint, CTEBeamEntPoint )

