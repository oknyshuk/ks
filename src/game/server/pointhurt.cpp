// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Implements hurting point entity

#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "soundflags.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "npcevent.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "entitylist.h"
#include "gamerules.h"
#include "basecombatcharacter.h"
#include "ammodef.h"

const int SF_PHURT_START_ON			= 1;

class CPointHurt : public CPointEntity
{
	DECLARE_CLASS( CPointHurt, CPointEntity );

public:
	void	Spawn_Impl( void );
	void	Precache_Impl( void );
	void	HurtThink( void );

	// Input handlers
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn(inputdata_t &inputdata);
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff(inputdata_t &inputdata);
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle(inputdata_t &inputdata);
	[[= ks::reflect::Input{ .name = "Hurt", .type = FIELD_VOID } ]] void InputHurt(inputdata_t &inputdata);
	
	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "Damage" } ]] int			m_nDamage;
	[[= ks::reflect::Key{ .name = "DamageType" } ]] int			m_bitsDamageType;
	[[= ks::reflect::Key{ .name = "DamageRadius" } ]] float		m_flRadius;
	[[= ks::reflect::Key{ .name = "DamageDelay" } ]] float		m_flDelay;
	[[= ks::reflect::Key{ .name = "DamageTarget" } ]] string_t	m_strTarget;
	EHANDLE		m_pActivator;
};

IMPLEMENT_REFLECT_DATAMAP( CPointHurt )

LINK_ENTITY_TO_CLASS( point_hurt, CPointHurt );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointHurt::Spawn_Impl(void)
{
	SetThink( nullptr );
	SetUse( nullptr );
		
	m_pActivator = nullptr;

	if ( HasSpawnFlags( SF_PHURT_START_ON ) )
	{
		SetThink( &CPointHurt::HurtThink );
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
	
	if ( m_flRadius <= 0.0f )
	{
		m_flRadius = 128.0f;
	}

	if ( m_nDamage <= 0 )
	{
		m_nDamage = 2;
	}

	if ( m_flDelay <= 0 )
	{
		m_flDelay = 0.1f;
	}

	Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointHurt::Precache_Impl( void )
{
	BaseClass::Precache_Impl();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointHurt::HurtThink( void )
{
	if ( m_strTarget != NULL_STRING )
	{
		CBaseEntity	*pEnt = nullptr;
			
		CTakeDamageInfo info( this, m_pActivator, m_nDamage, m_bitsDamageType );
		while ( ( pEnt = gEntList.FindEntityByName( pEnt, m_strTarget, nullptr, m_pActivator ) ) != nullptr )
		{
			GuessDamageForce( &info, (pEnt->GetAbsOrigin() - GetAbsOrigin()), pEnt->GetAbsOrigin() );
			pEnt->TakeDamage( info );
		}
	}
	else
	{
		RadiusDamage( CTakeDamageInfo( this, this, m_nDamage, m_bitsDamageType ), GetAbsOrigin(), m_flRadius, CLASS_NONE, nullptr );
	}

	SetNextThink( gpGlobals->curtime + m_flDelay );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for turning on the point hurt.
//-----------------------------------------------------------------------------
void CPointHurt::InputTurnOn( inputdata_t &data )
{
	SetThink( &CPointHurt::HurtThink );

	SetNextThink( gpGlobals->curtime + 0.1f );

	m_pActivator = data.pActivator;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for turning off the point hurt.
//-----------------------------------------------------------------------------
void CPointHurt::InputTurnOff( inputdata_t &data )
{
	SetThink( nullptr );

	m_pActivator = data.pActivator;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for toggling the on/off state of the point hurt.
//-----------------------------------------------------------------------------
void CPointHurt::InputToggle( inputdata_t &data )
{
	m_pActivator = data.pActivator;

	if ( m_pfnThink == (void (CBaseEntity::*)())&CPointHurt::HurtThink )
	{
		SetThink( nullptr );
	}
	else
	{
		SetThink( &CPointHurt::HurtThink );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for instantaneously hurting whatever is near us.
//-----------------------------------------------------------------------------
void CPointHurt::InputHurt( inputdata_t &data )
{
	m_pActivator = data.pActivator;

	HurtThink();
}
