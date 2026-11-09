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
#include "toolframework_client.h"

//-----------------------------------------------------------------------------
// Purpose: Breakable Model TE
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEBreakModel" } ]]
      C_TEBreakModel : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEBreakModel, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

					C_TEBreakModel( void );
	virtual			~C_TEBreakModel( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{ .index = 2 } ]] [[= ks::reflect::Net{ .index = 1 } ]] [[= ks::reflect::Net{ .index = 0 } ]] QAngle			m_angRotation;
	[[= ks::reflect::Net{} ]] Vector			m_vecSize;
	[[= ks::reflect::Net{} ]] Vector			m_vecVelocity;
	[[= ks::reflect::Net{} ]] int				m_nRandomization;
	[[= ks::reflect::Net{} ]] int				m_nModelIndex;
	[[= ks::reflect::Net{} ]] int				m_nCount;
	[[= ks::reflect::Net{} ]] float			m_fTime;
	[[= ks::reflect::Net{} ]] int				m_nFlags;
};


//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( C_TEBreakModel, DT_TEBreakModel, CTEBreakModel )


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEBreakModel::C_TEBreakModel( void )
{
	m_vecOrigin.Init();
	m_angRotation.Init();
	m_vecSize.Init();
	m_vecVelocity.Init();
	m_nModelIndex		= 0;
	m_nRandomization	= 0;
	m_nCount			= 0;
	m_fTime				= 0.0;
	m_nFlags			= 0;
}

C_TEBreakModel::~C_TEBreakModel( void )
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TE_BreakModel( IRecipientFilter& filter, float delay,
	const Vector& pos, const QAngle &angles, const Vector& size, const Vector& vel, 
	int modelindex, int randomization, int count, float time, int flags )
{
	tempents->BreakModel( pos, angles, size, vel, randomization, time, count, modelindex, flags );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TEBreakModel::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "C_TEBreakModel::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	tempents->BreakModel( m_vecOrigin, m_angRotation, m_vecSize, m_vecVelocity,
		m_nRandomization, m_fTime, m_nCount, m_nModelIndex, m_nFlags );
}

void TE_BreakModel( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	Vector vecOrigin, vecSize, vecVel;
	QAngle angles;
	vecOrigin.x = pKeyValues->GetFloat( "originx" );
	vecOrigin.y = pKeyValues->GetFloat( "originy" );
	vecOrigin.z = pKeyValues->GetFloat( "originz" );
	angles.x = pKeyValues->GetFloat( "anglesx" );
	angles.y = pKeyValues->GetFloat( "anglesy" );
	angles.z = pKeyValues->GetFloat( "anglesz" );
	vecSize.x = pKeyValues->GetFloat( "sizex" );
	vecSize.y = pKeyValues->GetFloat( "sizey" );
	vecSize.z = pKeyValues->GetFloat( "sizez" );
	vecVel.x = pKeyValues->GetFloat( "velx" );
	vecVel.y = pKeyValues->GetFloat( "vely" );
	vecVel.z = pKeyValues->GetFloat( "velz" );
	Color c = pKeyValues->GetColor( "color" );
	const char *pModelName = pKeyValues->GetString( "model" );
	int nModelIndex = pModelName[0] ? modelinfo->GetModelIndex( pModelName ) : 0;
	int nRandomization = pKeyValues->GetInt( "randomization" );
	int nCount = pKeyValues->GetInt( "count" );
	float flDuration = pKeyValues->GetFloat( "duration" );
	int nFlags = pKeyValues->GetInt( "flags" );
	TE_BreakModel( filter, 0.0f, vecOrigin, angles, vecSize, vecVel,
		nModelIndex, nRandomization, nCount, flDuration, nFlags );
}
