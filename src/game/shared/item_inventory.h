//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Stub for item inventory system (econ system removed)
//
//=============================================================================

#ifndef ITEM_INVENTORY_H
#define ITEM_INVENTORY_H
#ifdef _WIN32
#pragma once
#endif

#include "steam/steam_api.h"

// Type definitions
typedef uint64 globalindex_t;
#define INVALID_GLOBAL_INDEX ((globalindex_t)0)

enum EItemRequestResult
{
	k_EItemRequestResultOK = 0,
	k_EItemRequestResultPending = 1,
	k_EItemRequestResultFailed = 2,
};

//-----------------------------------------------------------------------------
// Forward declaration
//-----------------------------------------------------------------------------
class CEconItemDefinition;

//-----------------------------------------------------------------------------
// Purpose: Stub for script-created items
//-----------------------------------------------------------------------------
class CScriptCreatedItem
{
public:
	CScriptCreatedItem() {}
	virtual ~CScriptCreatedItem() {}

	CEconItemDefinition *GetItemDefinition() const { return NULL; }
};

//-----------------------------------------------------------------------------
// Purpose: Base class for player inventories
//-----------------------------------------------------------------------------
class CPlayerInventory
{
public:
	CPlayerInventory() {}
	virtual ~CPlayerInventory() {}

	virtual bool ItemShouldBeIncluded( int iItemPosition ) { return true; }
	virtual void InventoryReceived( EItemRequestResult eResult, int iItems ) {}
	virtual void DumpInventoryToConsole( bool bRoot ) {}

#ifdef CLIENT_DLL
	virtual bool MoveItemToPosition( globalindex_t iGlobalIndex, int iPosition ) { return true; }
	virtual bool MoveItemToBackpack( globalindex_t iGlobalIndex ) { return true; }
#endif
};

//-----------------------------------------------------------------------------
// Purpose: Base class for inventory managers
//-----------------------------------------------------------------------------
class CInventoryManager
{
public:
	CInventoryManager() {}
	virtual ~CInventoryManager() {}

	virtual void PostInit( void ) {}
};

#endif // ITEM_INVENTORY_H
