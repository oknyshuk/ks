// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Base combat character with no AI

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
#include "ai_squadslot.h"
#include "ai_basenpc.h"
#include "stringregistry.h"

//=============================================================================
// Init static variables
//=============================================================================
CAI_GlobalNamespace CAI_BaseNPC::gm_SquadSlotNamespace;

//-----------------------------------------------------------------------------
// Purpose: Given and SquadSlot name, return the SquadSlot ID
//-----------------------------------------------------------------------------
int CAI_BaseNPC::GetSquadSlotID(const char* slotName)
{
	return gm_SquadSlotNamespace.SymbolToId(slotName);
}


//-----------------------------------------------------------------------------
// Purpose: Given and SquadSlot name, return the SquadSlot ID
//-----------------------------------------------------------------------------
const char* CAI_BaseNPC::GetSquadSlotDebugName( int iSquadSlot )
{
	switch( iSquadSlot )
	{
	case SQUAD_SLOT_NONE:				return "None";	
		break;
	case SQUAD_SLOT_ATTACK1:			return "SQUAD_SLOT_ATTACK1";	
		break;
	case SQUAD_SLOT_ATTACK2:			return "SQUAD_SLOT_ATTACK2";	
		break;
	case SQUAD_SLOT_INVESTIGATE_SOUND:	return "SQUAD_SLOT_INVESTIGATE_SOUND";	
		break;
	case SQUAD_SLOT_EXCLUSIVE_HANDSIGN:	return "SQUAD_SLOT_EXCLUSIVE_HANDSIGN";	
		break;
	case SQUAD_SLOT_EXCLUSIVE_RELOAD:	return "SQUAD_SLOT_EXCLUSIVE_RELOAD";	
		break;
	case SQUAD_SLOT_PICKUP_WEAPON1:		return "SQUAD_SLOT_PICKUP_WEAPON1";	
		break;
	case SQUAD_SLOT_PICKUP_WEAPON2:		return "SQUAD_SLOT_PICKUP_WEAPON2";	
		break;
	case SQUAD_SLOT_SPECIAL_ATTACK:		return "SQUAD_SLOT_SPECIAL_ATTACK";	
		break;

	default:
		// If we default, that means that our derived class (which has already been called)
		// didn't provide text for a squad slot that isn't part of the base ai squad slots.
		// OR someone added a squad slot to base AI and didn't update here.
		return "Failed to specify!";
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InitDefaultSquadSlotSR(void)
{
	gm_SquadSlotNamespace.AddSymbol( "SQUAD_SLOT_ATTACK1", AI_RemapToGlobal(SQUAD_SLOT_ATTACK1) );
	gm_SquadSlotNamespace.AddSymbol( "SQUAD_SLOT_ATTACK2", AI_RemapToGlobal(SQUAD_SLOT_ATTACK2) );
	gm_SquadSlotNamespace.AddSymbol( "SQUAD_SLOT_INVESTIGATE_SOUND", AI_RemapToGlobal(SQUAD_SLOT_INVESTIGATE_SOUND) );
	gm_SquadSlotNamespace.AddSymbol( "SQUAD_SLOT_EXCLUSIVE_HANDSIGN", AI_RemapToGlobal(SQUAD_SLOT_EXCLUSIVE_HANDSIGN) );
	gm_SquadSlotNamespace.AddSymbol( "SQUAD_SLOT_EXCLUSIVE_RELOAD", AI_RemapToGlobal(SQUAD_SLOT_EXCLUSIVE_RELOAD) );
	gm_SquadSlotNamespace.AddSymbol( "SQUAD_SLOT_PICKUP_WEAPON1", AI_RemapToGlobal(SQUAD_SLOT_PICKUP_WEAPON1) );
	gm_SquadSlotNamespace.AddSymbol( "SQUAD_SLOT_PICKUP_WEAPON2", AI_RemapToGlobal(SQUAD_SLOT_PICKUP_WEAPON2) );
	gm_SquadSlotNamespace.AddSymbol( "SQUAD_SLOT_SPECIAL_ATTACK", AI_RemapToGlobal(SQUAD_SLOT_SPECIAL_ATTACK) );
}
