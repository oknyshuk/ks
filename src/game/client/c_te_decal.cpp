// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "gamerules.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_basetempentity.h"
#include "iefx.h"
#include "engine/IStaticPropMgr.h"
#include "toolframework_client.h"

//-----------------------------------------------------------------------------
// Purpose: Decal TE
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEDecal" } ]]
      C_TEDecal : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEDecal, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

					C_TEDecal( void );
	virtual			~C_TEDecal( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

	virtual void	Precache( void );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{} ]] Vector			m_vecStart;
	[[= ks::reflect::Net{} ]] int				m_nEntity;
	[[= ks::reflect::Net{} ]] int				m_nHitbox;
	[[= ks::reflect::Net{} ]] int				m_nIndex;
};


//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( C_TEDecal, DT_TEDecal, CTEDecal )


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEDecal::C_TEDecal( void )
{
	m_vecOrigin.Init();
	m_vecStart.Init();
	m_nEntity = 0;
	m_nIndex = 0;
	m_nHitbox = 0;

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEDecal::~C_TEDecal( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TEDecal::Precache( void )
{											 
}

//-----------------------------------------------------------------------------
// Tempent 
//-----------------------------------------------------------------------------
void TE_Decal( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* start, int entity, int hitbox, int index )
{

	trace_t tr;

	// Special case for world entity with hitbox:
	if ( (entity == 0) && (hitbox != 0) )
	{
		Ray_t ray;
		ray.Init( *start, *pos );
		staticpropmgr->AddDecalToStaticProp( *start, *pos, hitbox - 1, index, false, tr );
	}
	else
	{
		// Only decal the world + brush models
		// Here we deal with decals on entities.
		C_BaseEntity* ent;
		if ( ( ent = cl_entitylist->GetEnt( entity ) ) == nullptr )
			return;

		ent->AddDecal( *start, *pos, *pos, hitbox, 
			index, false, tr );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TEDecal::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "C_TEDecal::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	CBroadcastRecipientFilter filter;
	TE_Decal( filter, 0.0f, &m_vecOrigin, &m_vecStart, m_nEntity, m_nHitbox, m_nIndex );
}


//-----------------------------------------------------------------------------
// Playback
//-----------------------------------------------------------------------------
void TE_Decal( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	Vector vecOrigin, vecStart;
	vecOrigin.x = pKeyValues->GetFloat( "originx" );
	vecOrigin.y = pKeyValues->GetFloat( "originy" );
	vecOrigin.z = pKeyValues->GetFloat( "originz" );
	vecStart.x = pKeyValues->GetFloat( "startx" );
	vecStart.y = pKeyValues->GetFloat( "starty" );
	vecStart.z = pKeyValues->GetFloat( "startz" );
	int nHitbox = pKeyValues->GetInt( "hitbox" );
	const char *pDecalName = pKeyValues->GetString( "decalname" );

	TE_Decal( filter, 0.0f, &vecOrigin, &vecStart, 0, nHitbox, effects->Draw_DecalIndexFromName( (char*)pDecalName ) );
}
