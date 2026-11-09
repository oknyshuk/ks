// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
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
// Purpose: Armor Ricochet TE
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEMetalSparks", .base = false } ]]
      C_TEMetalSparks : public C_BaseTempEntity
{
public:
	DECLARE_CLIENTCLASS();

					C_TEMetalSparks( void );
	virtual			~C_TEMetalSparks( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

	virtual void	Precache( void );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecPos;
	[[= ks::reflect::Net{} ]] Vector			m_vecDir;

	const struct model_t *m_pModel;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEMetalSparks::C_TEMetalSparks( void )
{
	m_vecPos.Init();
	m_vecDir.Init();
	m_pModel = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEMetalSparks::~C_TEMetalSparks( void )
{
}

void C_TEMetalSparks::Precache( void )
{
	//m_pModel = engine->LoadModel( "sprites/richo1.vmt" );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void C_TEMetalSparks::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "C_TEMetalSparks::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	g_pEffects->MetalSparks( m_vecPos, m_vecDir );
}

void TE_MetalSparks( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* dir )
{
	g_pEffects->MetalSparks( *pos, *dir );
}

//-----------------------------------------------------------------------------
// Purpose: Armor Ricochet TE
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEArmorRicochet" } ]]
      C_TEArmorRicochet : public C_TEMetalSparks
{
	DECLARE_CLASS( C_TEArmorRicochet, C_TEMetalSparks );
public:
	DECLARE_CLIENTCLASS();
	virtual void	PostDataUpdate( DataUpdateType_t updateType );
};


//-----------------------------------------------------------------------------
// Purpose: Client side version of API
//-----------------------------------------------------------------------------
void TE_ArmorRicochet( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* dir )
{
	g_pEffects->Ricochet( *pos, *dir );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void C_TEArmorRicochet::PostDataUpdate( DataUpdateType_t updateType )
{
	g_pEffects->Ricochet( m_vecPos, m_vecDir );
}

// Expose the TE to the engine.
IMPLEMENT_CLIENTCLASS_EVENT( C_TEMetalSparks, DT_TEMetalSparks, CTEMetalSparks );

IMPLEMENT_REFLECT_TABLE( C_TEMetalSparks, DT_TEMetalSparks );

IMPLEMENT_CLIENTCLASS_EVENT( C_TEArmorRicochet, DT_TEArmorRicochet, CTEArmorRicochet );
IMPLEMENT_REFLECT_TABLE( C_TEArmorRicochet, DT_TEArmorRicochet );
