// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "gamerules.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_basetempentity.h"
#include "IEffects.h"

//-----------------------------------------------------------------------------
// Purpose: Energy Splash TE
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEEnergySplash", .base = false } ]]
      C_TEEnergySplash : public C_BaseTempEntity
{
public:
	DECLARE_CLIENTCLASS();

					C_TEEnergySplash( void );
	virtual			~C_TEEnergySplash( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

	virtual void	Precache( void );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecPos;
	[[= ks::reflect::Net{} ]] Vector			m_vecDir;
	[[= ks::reflect::Net{} ]] bool			m_bExplosive;

	const struct model_t *m_pModel;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEEnergySplash::C_TEEnergySplash( void )
{
	m_vecPos.Init();
	m_vecDir.Init();
	m_bExplosive = false;
	m_pModel = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEEnergySplash::~C_TEEnergySplash( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TEEnergySplash::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void C_TEEnergySplash::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "C_TEEnergySplash::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	g_pEffects->EnergySplash( m_vecPos, m_vecDir, m_bExplosive );
}

void TE_EnergySplash( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* dir, bool bExplosive )
{
	g_pEffects->EnergySplash( *pos, *dir, bExplosive );
}

// Expose the TE to the engine.
IMPLEMENT_CLIENTCLASS_EVENT( C_TEEnergySplash, DT_TEEnergySplash, CTEEnergySplash );

IMPLEMENT_REFLECT_TABLE( C_TEEnergySplash, DT_TEEnergySplash );
