// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2003 Valve Corporation

// Purpose: Stub for item inventory (econ system removed)

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
import std;
import ks.core;
#if defined(CLIENT_DLL)
import ks.tier1;
#include "string_t.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "debugoverlay_shared.h"
#elif defined(GAME_DLL)
import ks.math;
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
#include "physics.h"
#include "ndebugoverlay.h"
#include "npcevent.h"
#endif
#include "cs_item_inventory.h"
#include "tier3/tier3.h"

//-----------------------------------------------------------------------------
CCSInventoryManager g_CSInventoryManager;
CInventoryManager *InventoryManager( void )
{
	return &g_CSInventoryManager;
}
CCSInventoryManager *CSInventoryManager( void )
{
	return &g_CSInventoryManager;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CCSInventoryManager::CCSInventoryManager( void )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSInventoryManager::PostInit( void )
{
	BaseClass::PostInit();
	GenerateBaseItems();
}

//-----------------------------------------------------------------------------
// Purpose: Generate & store the base item details for each class & loadout slot
//-----------------------------------------------------------------------------
void CCSInventoryManager::GenerateBaseItems( void )
{
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSInventoryManager::UpdateLocalInventory( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Fills out pList with all inventory items that could fit into the specified loadout slot for a given class
//-----------------------------------------------------------------------------
int	CCSInventoryManager::GetAllUsableItemsForSlot( int iClass, int iSlot, std::vector< CScriptCreatedItem* > *pList )
{
	return 0;
}
#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CCSPlayerInventory *CCSInventoryManager::GetBagForPlayer( CPlayerId &playerID, inventory_bags_t iBag )
{
	return nullptr;
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSInventoryManager::ShowItemsPickedUp( void )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSInventoryManager::CheckForRoomAndForceDiscard( void )
{
	return false;
}

CCSPlayerInventory *CCSInventoryManager::GetLocalTFInventory( void )
{
	return &m_LocalInventory;
}

#endif

//-----------------------------------------------------------------------------
// Purpose: Returns the item data for the base item in the loadout slot for a given class
//-----------------------------------------------------------------------------
CScriptCreatedItem *CCSInventoryManager::GetBaseItemForClass( int iSlot )
{
	if ( iSlot < 0 || iSlot >= LOADOUT_POSITION_COUNT )
		return nullptr;
	return &m_pBaseLoadoutItems[iSlot];
}

// GetBaseItemForTeam, GetItemInLoadoutForClass, GetItemInLoadoutForTeam
// are now inline stubs in cs_item_inventory.h

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSInventoryManager::EquipItemInLoadout( int iClass, int iSlot, globalindex_t iGlobalIndex )
{
	return false;
}
#endif


//=======================================================================================================================
// CS PLAYER INVENTORY
//=======================================================================================================================
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CCSPlayerInventory::CCSPlayerInventory()
{
	m_iBag = BAG_ALL_ITEMS;
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSPlayerInventory::MoveItemToPosition( globalindex_t iGlobalIndex, int iPosition )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSPlayerInventory::MoveItemToBackpack( globalindex_t iGlobalIndex )
{
	return false;
}

#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSPlayerInventory::InventoryReceived( EItemRequestResult eResult, int iItems )
{
	BaseClass::InventoryReceived( eResult, iItems );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSPlayerInventory::DumpInventoryToConsole( bool bRoot )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSPlayerInventory::ItemShouldBeIncluded( int iItemPosition )
{
	return BaseClass::ItemShouldBeIncluded( iItemPosition );
}
