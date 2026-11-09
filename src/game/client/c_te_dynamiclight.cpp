// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "gamerules.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_basetempentity.h"
#include "dlight.h"
#include "iefx.h"
#include "toolframework_client.h"

//-----------------------------------------------------------------------------
// Purpose: Dynamic Light
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEDynamicLight" } ]]
      C_TEDynamicLight : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEDynamicLight, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

					C_TEDynamicLight( void );
	virtual			~C_TEDynamicLight( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{} ]] float			m_fRadius;
	[[= ks::reflect::Net{} ]] int				r;
	[[= ks::reflect::Net{} ]] int				g;
	[[= ks::reflect::Net{} ]] int				b;
	[[= ks::reflect::Net{} ]] int				exponent;
	[[= ks::reflect::Net{} ]] float			m_fTime;
	[[= ks::reflect::Net{} ]] float			m_fDecay;
};


//-----------------------------------------------------------------------------
// Networking 
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( C_TEDynamicLight, DT_TEDynamicLight, CTEDynamicLight )


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEDynamicLight::C_TEDynamicLight( void )
{
	m_vecOrigin.Init();
	r = 0;
	g = 0;
	b = 0;
	exponent = 0;
	m_fRadius = 0.0;
	m_fTime = 0.0;
	m_fDecay = 0.0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEDynamicLight::~C_TEDynamicLight( void )
{
}

void TE_DynamicLight( IRecipientFilter& filter, float delay,
	const Vector* org, int r, int g, int b, int exponent, float radius, float time, float decay, int nLightIndex )
{
	dlight_t *dl = effects->CL_AllocDlight( nLightIndex );
	if ( !dl )
		return;

	dl->origin	= *org;
	dl->radius	= radius;
	dl->color.r	= r;
	dl->color.g	= g;
	dl->color.b	= b;
	dl->color.exponent	= exponent;
	dl->die		= gpGlobals->curtime + time;
	dl->decay	= decay;

}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void C_TEDynamicLight::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "C_TEDynamicLight::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	CBroadcastRecipientFilter filter;
	TE_DynamicLight( filter, 0.0f, &m_vecOrigin, r, g, b, exponent, m_fRadius, m_fTime, m_fDecay, LIGHT_INDEX_TE_DYNAMIC );
}

void TE_DynamicLight( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	Vector vecOrigin;
	vecOrigin.x = pKeyValues->GetFloat( "originx" );
	vecOrigin.y = pKeyValues->GetFloat( "originy" );
	vecOrigin.z = pKeyValues->GetFloat( "originz" );
	float flDuration = pKeyValues->GetFloat( "duration" );
	Color c = pKeyValues->GetColor( "color" );
	int nExponent = pKeyValues->GetInt( "exponent" );
	float flRadius = pKeyValues->GetFloat( "radius" );
	float flDecay = pKeyValues->GetFloat( "decay" );
 	int nLightIndex = pKeyValues->GetInt( "lightindex", LIGHT_INDEX_TE_DYNAMIC );

	TE_DynamicLight( filter, 0.0f, &vecOrigin, c.r(), c.g(), c.b(), nExponent, 
		flRadius, flDuration, flDecay, nLightIndex );
}
