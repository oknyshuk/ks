// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
import ks.tier1;
import std;
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
// Recording
//-----------------------------------------------------------------------------
static inline void RecordSparks( const Vector &start, int nMagnitude, int nTrailLength, const Vector &direction )
{
	if ( !ToolsEnabled() )
		return;

	if ( clienttools->IsInRecordingMode() )
	{
		KeyValues *msg = new KeyValues( "TempEntity" );

 		msg->SetInt( "te", TE_SPARKS );
 		msg->SetString( "name", "TE_Sparks" );
		msg->SetFloat( "time", gpGlobals->curtime );
		msg->SetFloat( "originx", start.x );
		msg->SetFloat( "originy", start.y );
		msg->SetFloat( "originz", start.z );
		msg->SetFloat( "directionx", direction.x );
		msg->SetFloat( "directiony", direction.y );
		msg->SetFloat( "directionz", direction.z );
		msg->SetInt( "magnitude", nMagnitude );
		msg->SetInt( "traillength", nTrailLength );

		ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );
		msg->deleteThis();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TESparks::PostDataUpdate( DataUpdateType_t updateType )
{
	g_pEffects->Sparks( m_vecOrigin, m_nMagnitude, m_nTrailLength, &m_vecDir );
	RecordSparks( m_vecOrigin, m_nMagnitude, m_nTrailLength, m_vecDir );
}

void TE_Sparks( IRecipientFilter& filter, float delay,
	const Vector* pos, int nMagnitude, int nTrailLength, const Vector *pDir )
{
	g_pEffects->Sparks( *pos, nMagnitude, nTrailLength, pDir );
	RecordSparks( *pos, nMagnitude, nTrailLength, *pDir );
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( C_TESparks, DT_TESparks, CTESparks )
