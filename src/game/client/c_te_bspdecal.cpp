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
#include "iefx.h"
#include "fx.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// UNDONE:  Get rid of this?
#define FDECAL_PERMANENT			0x01

//-----------------------------------------------------------------------------
// Purpose: BSP Decal TE
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEBSPDecal" } ]]
      C_TEBSPDecal : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEBSPDecal, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

					C_TEBSPDecal( void );
	virtual			~C_TEBSPDecal( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

	virtual void	Precache( void );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{} ]] int				m_nEntity;
	[[= ks::reflect::Net{} ]] int				m_nIndex;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEBSPDecal::C_TEBSPDecal( void )
{
	m_vecOrigin.Init();
	m_nEntity = 0;
	m_nIndex = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEBSPDecal::~C_TEBSPDecal( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TEBSPDecal::Precache( void )
{
}

void TE_BSPDecal( IRecipientFilter& filter, float delay,
	const Vector* pos, int entity, int index )
{
	C_BaseEntity *ent;
	if ( ( ent = cl_entitylist->GetEnt( entity ) ) == nullptr )
	{
		DevMsg( 1, "Decal: entity = %i", entity );
		return;
	}

	if ( r_decals.GetInt() )
	{
		effects->DecalShoot( index, entity, ent->GetModel(), ent->GetAbsOrigin(), ent->GetAbsAngles(), *pos, 0, FDECAL_PERMANENT );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void C_TEBSPDecal::PostDataUpdate( DataUpdateType_t updateType )
{
	VPROF( "C_TEBSPDecal::PostDataUpdate" );

	C_BaseEntity *ent;
	if ( ( ent = cl_entitylist->GetEnt( m_nEntity ) ) == nullptr )
	{
		DevMsg( 1, "Decal: entity = %i", m_nEntity );
		return;
	}

	if ( r_decals.GetInt() )
	{
		effects->DecalShoot( m_nIndex, m_nEntity, ent->GetModel(), ent->GetAbsOrigin(), ent->GetAbsAngles(), m_vecOrigin, 0, FDECAL_PERMANENT );
	}
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( C_TEBSPDecal, DT_TEBSPDecal, CTEBSPDecal )

