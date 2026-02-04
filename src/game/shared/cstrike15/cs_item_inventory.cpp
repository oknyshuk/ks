//========= Copyright 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Stub for item inventory (econ system removed)
//
//=============================================================================

#include "cbase.h"
#include "cs_item_inventory.h"
#include "vgui/ILocalize.h"
#include "tier3/tier3.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


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
int	CCSInventoryManager::GetAllUsableItemsForSlot( int iClass, int iSlot, CUtlVector<CScriptCreatedItem*> *pList )
{
	return 0;
}
#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CCSPlayerInventory *CCSInventoryManager::GetBagForPlayer( CSteamID &playerID, inventory_bags_t iBag )
{
	return NULL;
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
		return NULL;
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
