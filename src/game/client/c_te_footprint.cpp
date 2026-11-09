// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_basetempentity.h"
#include "iefx.h"
#include "fx.h"

//-----------------------------------------------------------------------------
// Purpose: Footprint Decal TE
//-----------------------------------------------------------------------------

class [[= ks::reflect::NetTable{ .name = "DT_TEFootprintDecal" } ]]
      C_TEFootprintDecal : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEFootprintDecal, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

					C_TEFootprintDecal( void );
	virtual			~C_TEFootprintDecal( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

	virtual void	Precache( void );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{} ]] Vector			m_vecDirection;
	Vector			m_vecStart;
	[[= ks::reflect::Net{} ]] int				m_nEntity;
	[[= ks::reflect::Net{} ]] int				m_nIndex;
	[[= ks::reflect::Net{} ]] char			m_chMaterialType;
};

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( C_TEFootprintDecal, DT_TEFootprintDecal, CTEFootprintDecal )


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------

C_TEFootprintDecal::C_TEFootprintDecal( void )
{
	m_vecOrigin.Init();
	m_vecStart.Init();
	m_nEntity = 0;
	m_nIndex = 0;
	m_chMaterialType = 'C';
}

C_TEFootprintDecal::~C_TEFootprintDecal( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

void C_TEFootprintDecal::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Do stuff when data changes
//-----------------------------------------------------------------------------

void C_TEFootprintDecal::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "C_TEFootprintDecal::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// FIXME: Make this choose the decal based on material type
	if ( r_decals.GetInt() )
	{
		C_BaseEntity *ent = cl_entitylist->GetEnt( m_nEntity );
		if ( ent )
		{
			effects->DecalShoot( m_nIndex, 
				m_nEntity, ent->GetModel(), ent->GetAbsOrigin(), ent->GetAbsAngles(), m_vecOrigin, &m_vecDirection, 0 );
		}
	}
}

void TE_FootprintDecal( IRecipientFilter& filter, float delay, const Vector *origin, const Vector* right, 
	int entity, int index, unsigned char materialType )
{
	if ( r_decals.GetInt() )
	{
		C_BaseEntity *ent = cl_entitylist->GetEnt( entity );
		if ( ent )
		{
			effects->DecalShoot( index, entity, ent->GetModel(), ent->GetAbsOrigin(), ent->GetAbsAngles(), *origin, right, 0 );
		}
	}
}
