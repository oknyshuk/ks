// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Author: Michael S. Booth (mike@turtlerockstudios.com), April 2005

import std;
import ks.math;
#include "shareddefs.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "baseanimating.h"
#include "cs_bot.h"
#include "BasePropDoor.h"
#include "doors.h"
#include "props.h"

//-------------------------------------------------------------------------------------------------
/**
 * Face the door and open it.
 * NOTE: This state assumes we are standing in range of the door to be opened, with no obstructions.
 */
void OpenDoorState::OnEnter( CCSBot *me )
{
	m_isDone = false;
	m_timeout.Start( 0.5f );
}


//-------------------------------------------------------------------------------------------------
void OpenDoorState::SetDoor( CBaseEntity *door )
{
	CBaseDoor *funcDoor = dynamic_cast< CBaseDoor * >(door);
	if ( funcDoor )
	{
		m_funcDoor = funcDoor;
		return;
	}

	CBasePropDoor *propDoor = dynamic_cast< CBasePropDoor * >(door);
	if ( propDoor )
	{
		m_propDoor = propDoor;
		return;
	}
}


//-------------------------------------------------------------------------------------------------
void OpenDoorState::OnUpdate( CCSBot *me )
{
	me->ResetStuckMonitor();

	// look at the door
	Vector pos;
	bool isDoorMoving = false;
	CBaseEntity *door = nullptr;

	if ( m_funcDoor.Get() )
	{
		door = m_funcDoor;
		isDoorMoving = m_funcDoor->m_toggle_state == TS_GOING_UP || m_funcDoor->m_toggle_state == TS_GOING_DOWN;
	}
	else if ( m_propDoor.Get() )
	{
		door = m_propDoor;
		isDoorMoving = m_propDoor->IsDoorOpening() || m_propDoor->IsDoorClosing();

		CPropDoorRotatingBreakable *pPropDoor = dynamic_cast< CPropDoorRotatingBreakable* >( door );
		if ( pPropDoor && pPropDoor->IsDoorLocked() && pPropDoor->IsBreakable() == false )
		{
			m_isDone = true;
			return;
		}
	}

	// wait for door to swing open before leaving state
	if ( isDoorMoving || !door )
	{
		m_isDone = true;
		return;
	}

	me->SetLookAt( "Open door", door->WorldSpaceCenter(), PRIORITY_UNINTERRUPTABLE );

	// if we are looking at the door, "use" it and exit
	if ( me->IsLookingAtPosition( door->WorldSpaceCenter() ) )
	{
		if ( m_timeout.IsElapsed() )
		{
			// possibly stuck - blow the damn door away!
			me->PrimaryAttack();

			if ( door )
			{
				contract_assert( ( door->GetHealth() > 2 ) && "Bot is stuck on a door and is going to destroy it to get free!\n" );

				CTakeDamageInfo damageInfo( me, me, 2.0f, DMG_GENERIC );
				door->TakeDamage( damageInfo );
			}

		}

		// we are looking at it - use it directly to avoid complications
		door->Use( me, me, USE_ON, 0.0f );
	}
}


//-------------------------------------------------------------------------------------------------
void OpenDoorState::OnExit( CCSBot *me )
{
	me->ClearLookAt();
	me->ResetStuckMonitor();
}
