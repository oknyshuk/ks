// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "string_t.h"
#include "vphysics_interface.h"
#include "edict.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "physics.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "npcevent.h"
#include "func_hostage_rescue.h"

//============================================================================
LINK_ENTITY_TO_CLASS( func_hostage_rescue, CHostageRescueZone );



void CHostageRescueZone::Spawn_Impl()
{
	m_hInstructorHint = nullptr;

	InitTrigger();
	SetTouch( &CHostageRescueZone::HostageRescueTouch );

	VisibilityMonitor_AddEntity( this, 1600.0f, nullptr, nullptr );
}

void CHostageRescueZone::ReInitOnRoundStart( void )
{
	if ( m_hInstructorHint.Get() )
	{
		CPointEntity *pEnt = static_cast< CPointEntity* >( m_hInstructorHint.Get() );
		UTIL_Remove( pEnt );
		m_hInstructorHint = nullptr;
	}

	//CCSPlayerResource *pCSPR = CSPlayerResource();
	Vector vecPosCur = Vector( 0, 0, 0 );
	Vector vecPosNew = Vector( 0, 0, 0 );
	Vector vecTrigger = CollisionProp()->WorldSpaceCenter();
	if ( CSPlayerResource() )
	{
		vecPosCur = CSPlayerResource()->GetHostageRescuePosition( 0 );

		for ( int i=0; i < MAX_HOSTAGE_RESCUES; i++ )
		{
			vecPosNew = CSPlayerResource()->GetHostageRescuePosition( std::min( i+1, MAX_HOSTAGE_RESCUES-1 ) );
			if ( vecPosNew != vec3_origin )
			{
				if ( vecTrigger.DistTo( vecPosCur ) > vecTrigger.DistTo( vecPosNew ) )
				{
					vecPosCur = vecPosNew;
				}
			}
		}

		CPointEntity *pEnt = nullptr;
		if ( vecPosCur != vec3_origin )
		{
			pEnt = static_cast< CPointEntity* >( CreateEntityByName( "info_hostage_rescue_zone_hint" ) );
		}

		if ( pEnt )
		{
			pEnt->Spawn();
			trace_t tr;

			UTIL_TraceLine( vecPosCur, vecPosCur + Vector ( 0, 0, -128 ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
			Vector vecHint = tr.endpos +  Vector ( 0, 0, 16 );

			pEnt->SetAbsOrigin( vecHint );
			pEnt->SetOwnerEntity( this );
			pEnt->SetParent( this );

			m_hInstructorHint = pEnt;
		}
	}
}

void CHostageRescueZone::HostageRescueTouch( CBaseEntity *pOther )
{
	if ( m_bDisabled == false )
	{
		variant_t emptyVariant;
		pOther->AcceptInput( "OnRescueZoneTouch", nullptr, nullptr, emptyVariant, 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: A generic target entity that gets replicated to the client for displaying a hint for the hostage rescue zone
//-----------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( info_hostage_rescue_zone_hint, CInfoInstructorHintHostageRescueZone );


void CInfoInstructorHintHostageRescueZone::Spawn_Impl( void )
{
	VisibilityMonitor_AddEntity( this, 5000.0f, nullptr, nullptr );
}
