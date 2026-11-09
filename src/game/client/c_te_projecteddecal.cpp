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

// UNDONE:  Get rid of this?
#define FDECAL_PERMANENT			0x01

//-----------------------------------------------------------------------------
// Purpose: Projected Decal TE
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEProjectedDecal" } ]]
      C_TEProjectedDecal : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEProjectedDecal, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

					C_TEProjectedDecal( void );
	virtual			~C_TEProjectedDecal( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

	virtual void	Precache( void );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::QAngles } ]] QAngle			m_angRotation;
	[[= ks::reflect::Net{} ]] float			m_flDistance;
	[[= ks::reflect::Net{} ]] int				m_nIndex;
};


//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( C_TEProjectedDecal, DT_TEProjectedDecal, CTEProjectedDecal )


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEProjectedDecal::C_TEProjectedDecal( void )
{
	m_vecOrigin.Init();
	m_angRotation.Init();
	m_flDistance = 0.0f;
	m_nIndex = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEProjectedDecal::~C_TEProjectedDecal( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TEProjectedDecal::Precache( void )
{
}


void TE_ProjectDecal( IRecipientFilter& filter, float delay,
	const Vector* pos, const QAngle *angles, float distance, int index )
{

	trace_t	tr;

	Vector fwd;
	AngleVectors( *angles, &fwd );

	Vector endpos;
	VectorMA( *pos, distance, fwd, endpos );

	CTraceFilterHitAll traceFilter;
	UTIL_TraceLine( *pos, endpos, MASK_ALL, &traceFilter, &tr );

	if ( tr.fraction == 1.0f )
	{
		return;
	}

	C_BaseEntity* ent = tr.Ent<CBaseEntity>();
	contract_assert( ent );

	int hitbox = tr.hitbox;

	if ( tr.hitbox != 0 )
	{
		staticpropmgr->AddDecalToStaticProp( *pos, endpos, hitbox - 1, index, false, tr );
	}
	else
	{
		// Only decal the world + brush models
		ent->AddDecal( *pos, endpos, endpos, hitbox, 
			index, false, tr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TEProjectedDecal::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "C_TEProjectedDecal::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	CBroadcastRecipientFilter filter;
	TE_ProjectDecal( filter, 0.0f, &m_vecOrigin, &m_angRotation, m_flDistance, m_nIndex );
}


//-----------------------------------------------------------------------------
// Playback
//-----------------------------------------------------------------------------
void TE_ProjectDecal( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	Vector vecOrigin;
	QAngle angles;
	vecOrigin.x = pKeyValues->GetFloat( "originx" );
	vecOrigin.y = pKeyValues->GetFloat( "originy" );
	vecOrigin.z = pKeyValues->GetFloat( "originz" );
	angles.x = pKeyValues->GetFloat( "anglesx" );
	angles.y = pKeyValues->GetFloat( "anglesy" );
	angles.z = pKeyValues->GetFloat( "anglesz" );
	float flDistance = pKeyValues->GetFloat( "distance" );
	const char *pDecalName = pKeyValues->GetString( "decalname" );

	TE_ProjectDecal( filter, 0.0f, &vecOrigin, &angles, flDistance, effects->Draw_DecalIndexFromName( (char*)pDecalName ) );
}
