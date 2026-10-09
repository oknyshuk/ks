// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core;
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "gamerules.h"
#include "c_basetempentity.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"

constexpr auto MAX_SPLINE_POINTS = 16;
//-----------------------------------------------------------------------------
// Purpose: BeamSpline TE
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEBeamSpline", .base = false } ]]
      C_TEBeamSpline : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEBeamSpline, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

					C_TEBeamSpline( void );
	virtual			~C_TEBeamSpline( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{ .varlen = true } ]] Vector			m_vecPoints[ MAX_SPLINE_POINTS ];
	[[= ks::reflect::Net{} ]] int				m_nPoints;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEBeamSpline::C_TEBeamSpline( void )
{
	int i;
	for ( i = 0; i < MAX_SPLINE_POINTS; i++ )
	{
		m_vecPoints[ i ].Init();
	}
	m_nPoints = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEBeamSpline::~C_TEBeamSpline( void )
{
}

void TE_BeamSpline( IRecipientFilter& filter, float delay,
	int points, Vector* rgPoints )
{
	DevMsg( 1, "Beam spline with %i points invoked\n", points );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void C_TEBeamSpline::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "C_TEBeamSpline::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	DevMsg( 1, "Beam spline with %i points received\n", m_nPoints );
}

// Expose the TE to the engine.
IMPLEMENT_CLIENTCLASS_EVENT( C_TEBeamSpline, DT_TEBeamSpline, CTEBeamSpline );

IMPLEMENT_REFLECT_TABLE( C_TEBeamSpline, DT_TEBeamSpline );
