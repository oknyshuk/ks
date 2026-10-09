// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Implements a moving target that moves along a path of path_tracks
//			and can be shot and killed. When the target it killed it fires an
//			OnDeath output.
//			m_flSpeed is the travel speed
//			m_iHealth is current health
//			m_iMaxHealth is the amount to reset to each time it starts

import std;
import ks.tier1;
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
#include "base_transmit_proxy.h"
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
#include "player.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "entityoutput.h"

constexpr auto FGUNTARGET_START_ON = 0x0001;


class CGunTarget : public CBaseToggle
{
	DECLARE_CLASS( CGunTarget, CBaseToggle );

public:

	virtual void Spawn( void );
	virtual void Activate( void );
	bool CreateVPhysics( void );

	virtual int BloodColor( void ) { return DONT_BLEED; }

	virtual Class_T Classify( void ) { return CLASS_NONE; }
	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	virtual Vector BodyTarget( const Vector &posSrc, bool bNoisy = true ) { return GetAbsOrigin(); }

	// Input handlers
	[[= ks::reflect::Input{ .name = "Start", .type = FIELD_VOID } ]] void InputStart( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Stop", .type = FIELD_VOID } ]] void InputStop( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );

	DECLARE_DATADESC();

protected:

	void Next( void );
	void Start( void );
	void Wait( void );
	void Stop( void );

private:

	bool			m_on;
	EHANDLE			m_hTargetEnt;

	// Outputs
	[[= ks::reflect::Key{ .name = "OnDeath" } ]] COutputEvent	m_OnDeath;
};


LINK_ENTITY_TO_CLASS( func_guntarget, CGunTarget );

IMPLEMENT_REFLECT_DATAMAP( CGunTarget )



void CGunTarget::Spawn( void )
{
	SetSolid( SOLID_BSP );
	SetMoveType( MOVETYPE_PUSH );

	SetModel( STRING( GetModelName() ) );

	if ( m_flSpeed == 0 )
		m_flSpeed = 100;

	// Don't take damage until "on"
	m_takedamage = DAMAGE_NO;
	AddFlag( FL_NPC );

	m_on = false;
	m_iMaxHealth = m_iHealth;

	if ( HasSpawnFlags(FGUNTARGET_START_ON) )
	{
		SetMoveDone( &CGunTarget::Start );
		SetMoveDoneTime( 0.3 );
	}
	CreateVPhysics();
}


bool CGunTarget::CreateVPhysics( void )
{
	VPhysicsInitShadow( false, false );
	return true;
}

void CGunTarget::Activate( void )
{
	BaseClass::Activate();

	CBaseEntity	*pTarg;
	// now find our next target
	pTarg = GetNextTarget();
	if ( pTarg )
	{
		m_hTargetEnt = pTarg;
		Vector nextPos = pTarg->GetAbsOrigin();
		Teleport( &nextPos, nullptr, nullptr );
	}
}


void CGunTarget::Start( void )
{
	m_takedamage = DAMAGE_YES;
	AddFlag( FL_AIMTARGET );
	m_hTargetEnt = GetNextTarget();
	if ( m_hTargetEnt == nullptr )
		return;
	m_iHealth = m_iMaxHealth;
	Next();
}


void CGunTarget::Next( void )
{
	SetThink( nullptr );

	m_hTargetEnt = GetNextTarget();
	CBaseEntity *pTarget = m_hTargetEnt;
	
	if ( !pTarget )
	{
		Stop();
		return;
	}

	SetMoveDone( &CGunTarget::Wait );
	LinearMove( pTarget->GetLocalOrigin(), m_flSpeed );
}


void CGunTarget::Wait( void )
{
	CBaseEntity *pTarget = m_hTargetEnt;
	
	if ( !pTarget )
	{
		Stop();
		return;
	}

	variant_t emptyVariant;
	pTarget->AcceptInput( "InPass", this, this, emptyVariant, 0 );
		
	m_flWait = pTarget->GetDelay();

	m_target = pTarget->m_target;
	SetMoveDone( &CGunTarget::Next );
	if (m_flWait != 0)
	{// -1 wait will wait forever!		
		SetMoveDoneTime( m_flWait );
	}
	else
	{
		Next();// do it RIGHT now!
	}
}


void CGunTarget::Stop( void )
{
	SetAbsVelocity( vec3_origin );
	SetMoveDoneTime( -1 );
	m_takedamage = DAMAGE_NO;
}


int	CGunTarget::OnTakeDamage( const CTakeDamageInfo &info )
{
	if ( m_iHealth > 0 )
	{
		m_iHealth -= info.GetDamage();
		if ( m_iHealth <= 0 )
		{
			m_iHealth = 0;
			Stop();

			m_OnDeath.FireOutput( info.GetInflictor(), this );
		}
	}
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Input handler that starts the target moving.
//-----------------------------------------------------------------------------
void CGunTarget::InputStart( inputdata_t &inputdata )
{
	Start();
}


//-----------------------------------------------------------------------------
// Purpose: Input handler that stops the target from moving.
//-----------------------------------------------------------------------------
void CGunTarget::InputStop( inputdata_t &inputdata )
{
	Stop();
}


//-----------------------------------------------------------------------------
// Purpose: Input handler that toggles the start/stop state of the target.
//-----------------------------------------------------------------------------
void CGunTarget::InputToggle( inputdata_t &inputdata )
{
	if ( m_on )
	{
		Stop();
	}
	else
	{
		Start();
	}
}
