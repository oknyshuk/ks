// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "gamerules.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_basetempentity.h"
#include "c_te_legacytempents.h"

//-----------------------------------------------------------------------------
// Purpose: Client Projectile TE
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEClientProjectile" } ]]
      C_TEClientProjectile : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEClientProjectile, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	C_TEClientProjectile( void );
	virtual			~C_TEClientProjectile( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector m_vecOrigin;
	[[= ks::reflect::Net{} ]] Vector m_vecVelocity;
	[[= ks::reflect::Net{} ]] int m_nModelIndex;
	[[= ks::reflect::Net{} ]] int m_nLifeTime;
	[[= ks::reflect::Net{} ]] EHANDLE m_hOwner;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEClientProjectile::C_TEClientProjectile( void )
{
	m_vecOrigin.Init();
	m_vecVelocity.Init();
	m_nModelIndex = 0;
	m_nLifeTime = 0;
	m_hOwner = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEClientProjectile::~C_TEClientProjectile( void )
{
}

void TE_ClientProjectile( IRecipientFilter& filter, float delay,
					const Vector* vecOrigin, const Vector* vecVelocity, int modelindex, int lifetime, CBaseEntity *pOwner )
{
	tempents->ClientProjectile( *vecOrigin, *vecVelocity, vec3_origin, modelindex, lifetime, pOwner );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void C_TEClientProjectile::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "C_TEClientProjectile::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	tempents->ClientProjectile( m_vecOrigin, m_vecVelocity, vec3_origin, m_nModelIndex, m_nLifeTime, m_hOwner );
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( C_TEClientProjectile, DT_TEClientProjectile, CTEClientProjectile )
