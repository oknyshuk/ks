// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_basetempentity.h"
#include "c_te_legacytempents.h"
#include "tempent.h"
#include "toolframework_client.h"

//-----------------------------------------------------------------------------
// Purpose: Glow Sprite TE
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEGlowSprite" } ]]
      C_TEGlowSprite : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEGlowSprite, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

					C_TEGlowSprite( void );
	virtual			~C_TEGlowSprite( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{} ]] int				m_nModelIndex;
	[[= ks::reflect::Net{} ]] float			m_fScale;
	[[= ks::reflect::Net{} ]] float			m_fLife;
	[[= ks::reflect::Net{} ]] int				m_nBrightness;
};


//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( C_TEGlowSprite, DT_TEGlowSprite, CTEGlowSprite )


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEGlowSprite::C_TEGlowSprite( void )
{
	m_vecOrigin.Init();
	m_nModelIndex = 0;
	m_fScale = 0;
	m_fLife = 0;
	m_nBrightness = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEGlowSprite::~C_TEGlowSprite( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TEGlowSprite::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "C_TEGlowSprite::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	float a = ( 1.0 / 255.0 ) * m_nBrightness;
	C_LocalTempEntity *ent = tempents->TempSprite( m_vecOrigin, vec3_origin, m_fScale, m_nModelIndex, kRenderTransAdd, 0, a, m_fLife, FTENT_SPRANIMATE | FTENT_SPRANIMATELOOP );
	if ( ent )
	{
		ent->bounceFactor = 0.2;
	}
}

void TE_GlowSprite( IRecipientFilter& filter, float delay,
	const Vector* pos, int modelindex, float life, float size, int brightness )
{
	float a = ( 1.0 / 255.0 ) * brightness;
	C_LocalTempEntity *ent = tempents->TempSprite( *pos, vec3_origin, size, modelindex, kRenderTransAdd, 0, a, life, FTENT_SPRANIMATE | FTENT_SPRANIMATELOOP );
	if ( ent )
	{
		ent->bounceFactor = 0.2;
	}
}

void TE_GlowSprite( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	Vector vecOrigin;
	vecOrigin.x = pKeyValues->GetFloat( "originx" );
	vecOrigin.y = pKeyValues->GetFloat( "originy" );
	vecOrigin.z = pKeyValues->GetFloat( "originz" );
	const char *pModelName = pKeyValues->GetString( "model" );
	int nModelIndex = pModelName[0] ? modelinfo->GetModelIndex( pModelName ) : 0;
	float flDuration = pKeyValues->GetFloat( "duration" );
	float flSize = pKeyValues->GetFloat( "size" );
	int nBrightness = pKeyValues->GetFloat( "brightness" );

	TE_GlowSprite( filter, delay, &vecOrigin, nModelIndex, flDuration, flSize, nBrightness );
}
