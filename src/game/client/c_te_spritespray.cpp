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
// Purpose: Sprite Spray TE
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TESpriteSpray" } ]]
      C_TESpriteSpray : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TESpriteSpray, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

					C_TESpriteSpray( void );
	virtual			~C_TESpriteSpray( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{} ]] Vector			m_vecDirection;
	[[= ks::reflect::Net{} ]] int				m_nModelIndex;
	[[= ks::reflect::Net{} ]] int				m_nSpeed;
	[[= ks::reflect::Net{} ]] float			m_fNoise;
	[[= ks::reflect::Net{} ]] int				m_nCount;
};


//-----------------------------------------------------------------------------
// Networking 
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( C_TESpriteSpray, DT_TESpriteSpray, CTESpriteSpray )


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TESpriteSpray::C_TESpriteSpray( void )
{
	m_vecOrigin.Init();
	m_vecDirection.Init();
	m_nModelIndex = 0;
	m_fNoise = 0;
	m_nSpeed = 0;
	m_nCount = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TESpriteSpray::~C_TESpriteSpray( void )
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TESpriteSpray::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "C_TESpriteSpray::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	tempents->Sprite_Spray( m_vecOrigin, m_vecDirection, m_nModelIndex, m_nCount, m_nSpeed * 0.2, m_fNoise * 100.0 );
}

void TE_SpriteSpray( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* dir, int modelindex, int speed, float noise, int count )
{
	tempents->Sprite_Spray( *pos, *dir, modelindex, count, speed * 0.2, noise * 100.0 );
}

void TE_SpriteSpray( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	Vector vecOrigin, vecDirection;
	vecOrigin.x = pKeyValues->GetFloat( "originx" );
	vecOrigin.y = pKeyValues->GetFloat( "originy" );
	vecOrigin.z = pKeyValues->GetFloat( "originz" );
	vecDirection.x = pKeyValues->GetFloat( "directionx" );
	vecDirection.y = pKeyValues->GetFloat( "directiony" );
	vecDirection.z = pKeyValues->GetFloat( "directionz" );
	const char *pModelName = pKeyValues->GetString( "model" );
	int nModelIndex = pModelName[0] ? modelinfo->GetModelIndex( pModelName ) : 0;
	int nSpeed = pKeyValues->GetInt( "speed" );
	float flNoise = pKeyValues->GetFloat( "noise" );
	int nCount = pKeyValues->GetInt( "count" );

	TE_SpriteSpray( filter, delay, &vecOrigin, &vecDirection, nModelIndex, nSpeed, flNoise, nCount );
}
