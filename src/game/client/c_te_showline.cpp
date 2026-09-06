//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
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
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_te_particlesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Show Line TE
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEShowLine" } ]]
      C_TEShowLine : public C_TEParticleSystem
{
public:
	DECLARE_CLASS( C_TEShowLine, C_TEParticleSystem );
	DECLARE_CLIENTCLASS();

					C_TEShowLine( void );
	virtual			~C_TEShowLine( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecEnd;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEShowLine::C_TEShowLine( void )
{
	m_vecEnd.Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEShowLine::~C_TEShowLine( void )
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void C_TEShowLine::PostDataUpdate( DataUpdateType_t updateType )
{
	Vector		vec;
	float		len;
	StandardParticle_t	*p;
	int			dec;
	static int	tracercount;

	VectorSubtract (m_vecEnd, m_vecOrigin, vec);
	len = VectorNormalize (vec);

	dec = 3;

	VectorScale(vec, dec, vec);

	CSmartPtr<CTEParticleRenderer> pRen = CTEParticleRenderer::Create( "TEShowLine", m_vecOrigin );
	if( !pRen )
		return;
	
	while (len > 0)
	{
		len -= dec;

		p = pRen->AddParticle();
		if ( p )
		{
			p->m_Velocity.Init();

			pRen->SetParticleLifetime(p, 30);
			
			p->SetColor(0, 1, 1);
			p->SetAlpha(1);
			pRen->SetParticleType(p, pt_static);

			p->m_Pos = m_vecOrigin;
			
			m_vecOrigin += vec;
		}
	}
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( C_TEShowLine, DT_TEShowLine, CTEShowLine )

void TE_ShowLine( IRecipientFilter& filter, float delay,
	const Vector* start, const Vector* end )
{
	// Major hack to simulate receiving network message
	__g_C_TEShowLine.m_vecOrigin = *start;
	__g_C_TEShowLine.m_vecEnd = *end;

	__g_C_TEShowLine.PostDataUpdate( DATA_UPDATE_CREATED );
}
