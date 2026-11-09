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
#include "c_te_particlesystem.h"
#include "IEffects.h"
#include "toolframework_client.h"

//-----------------------------------------------------------------------------
// Purpose: Sparks TE
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TESparks" } ]]
      C_TESparks : public C_TEParticleSystem
{
public:
	DECLARE_CLASS( C_TESparks, C_TEParticleSystem );
	DECLARE_CLIENTCLASS();

					C_TESparks( void );
	virtual			~C_TESparks( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );
	virtual void	Precache( void );

	[[= ks::reflect::Net{} ]] int m_nMagnitude;
	[[= ks::reflect::Net{} ]] int m_nTrailLength;
	[[= ks::reflect::Net{} ]] Vector m_vecDir;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TESparks::C_TESparks( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TESparks::~C_TESparks( void )
{
}

void C_TESparks::Precache( void )
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TESparks::PostDataUpdate( DataUpdateType_t updateType )
{
	g_pEffects->Sparks( m_vecOrigin, m_nMagnitude, m_nTrailLength, &m_vecDir );
}

void TE_Sparks( IRecipientFilter& filter, float delay,
	const Vector* pos, int nMagnitude, int nTrailLength, const Vector *pDir )
{
	g_pEffects->Sparks( *pos, nMagnitude, nTrailLength, pDir );
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( C_TESparks, DT_TESparks, CTESparks )
