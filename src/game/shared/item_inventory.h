#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

// Purpose: Stub for item inventory system (econ system removed)

import std;
import ks.core;

// Type definitions
typedef uint64 globalindex_t;
inline constexpr auto INVALID_GLOBAL_INDEX = ((globalindex_t)0);

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

	CEconItemDefinition *GetItemDefinition() const { return nullptr; }
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
