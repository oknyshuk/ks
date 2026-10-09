// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Muzzle flash temp ent

import std;
import ks.tier1;
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "gamerules.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_basetempentity.h"
#include "IEffects.h"
#include "toolframework_client.h"

//-----------------------------------------------------------------------------
// Purpose: User Tracer TE
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEMuzzleFlash" } ]]
      C_TEMuzzleFlash : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEMuzzleFlash, C_BaseTempEntity );
	
	DECLARE_CLIENTCLASS();

					C_TEMuzzleFlash( void );
	virtual			~C_TEMuzzleFlash( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector		m_vecOrigin;
	[[= ks::reflect::Net{} ]] QAngle		m_vecAngles;
	[[= ks::reflect::Net{} ]] float		m_flScale;
	[[= ks::reflect::Net{} ]] int			m_nType;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEMuzzleFlash::C_TEMuzzleFlash( void )
{
	m_vecOrigin.Init();
	m_vecAngles.Init();
	m_flScale	= 1.0f;
	m_nType		= 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEMuzzleFlash::~C_TEMuzzleFlash( void )
{
}

//-----------------------------------------------------------------------------
// Recording
//-----------------------------------------------------------------------------
static inline void RecordMuzzleFlash( const Vector &start, const QAngle &angles, float scale, int type )
{
	if ( !ToolsEnabled() )
		return;

	if ( clienttools->IsInRecordingMode() )
	{
		KeyValues *msg = new KeyValues( "TempEntity" );

 		msg->SetInt( "te", TE_MUZZLE_FLASH );
 		msg->SetString( "name", "TE_MuzzleFlash" );
		msg->SetFloat( "time", gpGlobals->curtime );
		msg->SetFloat( "originx", start.x );
		msg->SetFloat( "originy", start.y );
		msg->SetFloat( "originz", start.z );
		msg->SetFloat( "anglesx", angles.x );
		msg->SetFloat( "anglesy", angles.y );
		msg->SetFloat( "anglesz", angles.z );
		msg->SetFloat( "scale", scale );
		msg->SetInt( "type", type );

		ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );
		msg->deleteThis();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TEMuzzleFlash::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "C_TEMuzzleFlash::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	//FIXME: Index is incorrect
	g_pEffects->MuzzleFlash( m_vecOrigin, m_vecAngles, m_flScale, m_nType );	
	RecordMuzzleFlash( m_vecOrigin, m_vecAngles, m_flScale, m_nType ); 
}

void TE_MuzzleFlash( IRecipientFilter& filter, float delay,
	const Vector &start, const QAngle &angles, float scale, int type )
{
	g_pEffects->MuzzleFlash( start, angles, scale, 0 );	
	RecordMuzzleFlash( start, angles, scale, 0 ); 
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( C_TEMuzzleFlash, DT_TEMuzzleFlash, CTEMuzzleFlash )
