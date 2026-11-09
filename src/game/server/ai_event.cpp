// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Events that are available to all NPCs.

#include "string_t.h"
#include "vphysics_interface.h"
#include "edict.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
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
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "entitylist.h"
#include "physics.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "npcevent.h"
#include "ai_basenpc.h"
#include "eventlist.h"
#include "stringregistry.h"

//=============================================================================
// Init static variables
//=============================================================================
CStringRegistry* CAI_BaseNPC::m_pEventSR	= nullptr;
int				 CAI_BaseNPC::m_iNumEvents	= 0;

//-----------------------------------------------------------------------------
// Purpose: Add an activity to the activity string registry and increment
//			the acitivty counter
//-----------------------------------------------------------------------------
void CAI_BaseNPC::AddEventToSR(const char *eventName, int eventID) 
{
	contract_assert( m_pEventSR );

	m_pEventSR->AddString( eventName, eventID );
	m_iNumEvents++;
}

//-----------------------------------------------------------------------------
// Purpose: Given and activity ID, return the activity name
//-----------------------------------------------------------------------------
const char *CAI_BaseNPC::GetEventName(int eventID) 
{
	const char *name = m_pEventSR->GetStringText( eventID );	
	return name;
}

//-----------------------------------------------------------------------------
// Purpose: Given and activity name, return the activity ID
//-----------------------------------------------------------------------------
int CAI_BaseNPC::GetEventID(const char* eventName) 
{
	return m_pEventSR->GetStringID( eventName );
}
