// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "gamerules.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_basetempentity.h"
#include "IEffects.h"
#include "toolframework_client.h"

//-----------------------------------------------------------------------------
// Purpose: Smoke TE
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TESmoke" } ]]
      C_TESmoke : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TESmoke, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

					C_TESmoke( void );
	virtual			~C_TESmoke( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{} ]] int				m_nModelIndex;
	[[= ks::reflect::Net{} ]] float			m_fScale;
	[[= ks::reflect::Net{} ]] int				m_nFrameRate;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TESmoke::C_TESmoke( void )
{
	m_vecOrigin.Init();
	m_nModelIndex = 0;
	m_fScale = 0;
	m_nFrameRate = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TESmoke::~C_TESmoke( void )
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TESmoke::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "C_TESmoke::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// The number passed down is 10 times smaller...
	g_pEffects->Smoke( m_vecOrigin, m_nModelIndex, m_fScale * 10.0f, m_nFrameRate );
}

void TE_Smoke( IRecipientFilter& filter, float delay,
	const Vector* pos, int modelindex, float scale, int framerate )
{
	// The number passed down is 10 times smaller...
	g_pEffects->Smoke( *pos, modelindex, scale * 10.0f, framerate );
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( C_TESmoke, DT_TESmoke, CTESmoke )
