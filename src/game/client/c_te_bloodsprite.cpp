// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "engine/ivmodelinfo.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_basetempentity.h"
#include "c_te_legacytempents.h"
#include "fx.h"
#include "toolframework_client.h"

extern int		g_sModelIndexBloodDrop;	
extern int		g_sModelIndexBloodSpray;

//-----------------------------------------------------------------------------
// Purpose: Blood sprite
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEBloodSprite", .base = false } ]]
      [[= ks::reflect::From<"r", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"g", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"b", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"a", ks::reflect::Net{}>{} ]]
      C_TEBloodSprite : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEBloodSprite, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

					C_TEBloodSprite( void );
	virtual			~C_TEBloodSprite( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{} ]] Vector			m_vecDirection;
	int				r, g, b, a;
	[[= ks::reflect::Net{} ]] int				m_nDropModel;
	[[= ks::reflect::Net{} ]] int				m_nSprayModel;
	[[= ks::reflect::Net{} ]] int				m_nSize;
};


// Expose it to the engine.
IMPLEMENT_CLIENTCLASS_EVENT( C_TEBloodSprite, DT_TEBloodSprite, CTEBloodSprite );


//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_TABLE( C_TEBloodSprite, DT_TEBloodSprite );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEBloodSprite::C_TEBloodSprite( void )
{
	m_vecOrigin.Init();
	m_vecDirection.Init();

	r = g = b = a = 0;
	m_nSize = 0;
	m_nSprayModel = 0;
	m_nDropModel = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEBloodSprite::~C_TEBloodSprite( void )
{
}


void TE_BloodSprite( IRecipientFilter& filter, float delay,
	const Vector* org, const Vector *dir, int r, int g, int b, int a, int size )
{
	Vector	offset = *org + ( (*dir) * 4.0f );

	tempents->BloodSprite( offset, r, g, b, a, g_sModelIndexBloodSpray, g_sModelIndexBloodDrop, size );	
	FX_Blood( offset, (Vector &)*dir, r, g, b, a );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TEBloodSprite::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "C_TEBloodSprite::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	Vector	offset = m_vecOrigin + ( m_vecDirection * 4.0f );

	tempents->BloodSprite( offset, r, g, b, a, m_nSprayModel, m_nDropModel, m_nSize );	
	FX_Blood( offset, m_vecDirection, r, g, b, a );
}

void TE_BloodSprite( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	Vector vecOrigin, vecDirection;
	vecOrigin.x = pKeyValues->GetFloat( "originx" );
	vecOrigin.y = pKeyValues->GetFloat( "originy" );
	vecOrigin.z = pKeyValues->GetFloat( "originz" );
	vecDirection.x = pKeyValues->GetFloat( "directionx" );
	vecDirection.y = pKeyValues->GetFloat( "directiony" );
	vecDirection.z = pKeyValues->GetFloat( "directionz" );
	Color c = pKeyValues->GetColor( "color" );
//	const char *pSprayModelName = pKeyValues->GetString( "spraymodel" );
//	const char *pDropModelName = pKeyValues->GetString( "dropmodel" );
	int nSize = pKeyValues->GetInt( "size" );

	TE_BloodSprite( filter, 0.0f, &vecOrigin, &vecDirection, c.r(), c.g(), c.b(), c.a(), nSize );
}
