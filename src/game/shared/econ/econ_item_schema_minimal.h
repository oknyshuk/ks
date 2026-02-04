//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Minimal econ item schema for CS:GO weapon system
//          Provides essential item definitions for weapon creation and lookups
//
//=============================================================================

#ifndef ECON_ITEM_SCHEMA_MINIMAL_H
#define ECON_ITEM_SCHEMA_MINIMAL_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlhashmaplarge.h"
#include "tier1/utldict.h"
#include "tier1/keyvalues.h"

// Forward declarations
class CEconItemSchema;
class CEconItemDefinition;
class KeyValues;

// Include cs_shareddefs.h for loadout_positions_t enum and team constants
// This avoids duplicate definitions
#include "cs_shareddefs.h"

// Number of team slots for loadout arrays (noteam, undefined, terrorists, counter-terrorists)
// This matches the team indexing used in the original econ code
#define LOADOUT_COUNT 4

//-----------------------------------------------------------------------------
// CEconItemDefinition - Minimal item definition
// Contains only the essential fields needed for weapon creation
//-----------------------------------------------------------------------------
class CEconItemDefinition
{
public:
	CEconItemDefinition();
	~CEconItemDefinition();

	// Initialize from KeyValues with prefab inheritance
	bool BInitFromKV( KeyValues *pKV, KeyValues *pPrefabs, CEconItemSchema *pSchema );

	// Accessors
	uint16 GetDefinitionIndex() const { return m_nDefIndex; }
	const char *GetDefinitionName() const { return m_szDefinitionName; }
	const char *GetItemClass() const { return m_szItemClass; }
	const char *GetItemBaseName() const { return m_szItemBaseName; }
	int GetDefaultLoadoutSlot() const { return m_iDefaultLoadoutSlot; }
	int GetLoadoutSlot( int iTeam ) const;
	int GetUsedByTeam() const;
	bool CanBeUsedByTeam( int iTeam ) const;
	int GetPrimaryReserveAmmoMax() const { return m_iPrimaryReserveAmmoMax; }
	int GetPrimaryClipSize() const { return m_iPrimaryClipSize; }
	bool IsHidden() const { return m_bHidden; }
	bool IsBaseItem() const { return m_bBaseItem; }
	bool IsDefaultSlotItem() const { return m_bDefaultSlotItem; }
	KeyValues *GetRawDefinition() const { return m_pKVItem; }

	// Model accessors
	const char *GetBasePlayerDisplayModel() const { return m_szModelPlayer; }
	const char *GetWorldDisplayModel() const { return m_szModelWorld; }
	const char *GetWorldDroppedModel() const { return m_szWorldDroppedModel; }

	// Stub accessors for compatibility (return NULL/defaults)
	const char *GetPrimaryAmmo() const { return NULL; }
	const char *GetEntityOverrideModel() const { return NULL; }
	const char *GetViewOverrideModel() const { return NULL; }
	const char *GetWeaponReplacementSound( int iIndex ) const { return NULL; }
	bool AreSlotsConsideredIdentical( int slot1, int slot2 ) const { return slot1 == slot2; }

private:
	// Core identification
	uint16 m_nDefIndex;
	char m_szDefinitionName[64];    // "weapon_knife_t"
	char m_szItemClass[64];         // "weapon_knife" (entity class)
	char m_szItemBaseName[64];      // Display name token

	// Loadout
	int m_iDefaultLoadoutSlot;      // LOADOUT_POSITION_MELEE etc
	int m_iLoadoutSlots[LOADOUT_COUNT];
	bool m_bClassUsability[LOADOUT_COUNT]; // Which teams can use

	// Weapon stats from attributes
	int m_iPrimaryReserveAmmoMax;
	int m_iPrimaryClipSize;

	// Flags
	bool m_bHidden;
	bool m_bBaseItem;
	bool m_bDefaultSlotItem;

	// Models
	char m_szModelPlayer[260];
	char m_szModelWorld[260];
	char m_szWorldDroppedModel[260];

	// Raw KV for any additional lookups
	KeyValues *m_pKVItem;
};

//-----------------------------------------------------------------------------
// CEconItemSchema - Minimal schema manager
// Parses items_game.txt and provides item definition lookups
//-----------------------------------------------------------------------------
class CEconItemSchema
{
public:
	CEconItemSchema();
	~CEconItemSchema();

	// Initialize the schema from items_game.txt
	bool BInit( const char *pszFilename );

	// Item definition lookups
	CEconItemDefinition *GetItemDefinition( uint16 nDefIndex ) const;
	CEconItemDefinition *GetItemDefinitionByName( const char *pszName ) const;
	CEconItemDefinition *GetItemDefinitionByMapIndex( int index ) const;

	// Iteration
	int GetItemDefinitionCount() const { return m_mapItems.Count(); }

#ifndef CLIENT_DLL
	// Precache all models referenced by item definitions (call during map load)
	void PrecacheItemDefinitionModels() const;
#endif

	// Type alias for compatibility with existing code
	typedef CUtlHashMapLarge<int, CEconItemDefinition*> ItemDefinitionMap_t;
	const ItemDefinitionMap_t& GetItemDefinitionMap() const { return m_mapItems; }

	// Stub accessors for compatibility (return NULL)
	class CEconItemAttributeDefinition *GetAttributeDefinitionByName( const char * ) const { return NULL; }
	class CEconMusicDefinition *GetMusicDefinition( uint32 ) const { return NULL; }
	class CEconQuestDefinition *GetQuestDefinition( uint32 ) const { return NULL; }
	class CEconTauntDefinition *GetTauntDefinition( uint32 ) const { return NULL; }
	class CPaintKit *GetPaintKitDefinition( uint32 ) const { return NULL; }
	class CProPlayerData *GetProPlayerDataByAccountID( uint64 ) const { return NULL; }
	class CStickerKit *GetStickerKitDefinition( uint32 ) const { return NULL; }
	class CEconGraffitiTintDefinition *GetGraffitiTintDefinitionByID( uint8 ) const { return NULL; }

private:
	// Parse prefabs section
	bool BInitPrefabs( KeyValues *pKVPrefabs );

	// Parse items section
	bool BInitItems( KeyValues *pKVItems );

	// Merge prefab values into an item's KeyValues
	KeyValues *MergeWithPrefab( KeyValues *pKVItem, const char *pszPrefab );

	// Helper to resolve nested prefab inheritance
	KeyValues *GetMergedPrefab( const char *pszPrefabName );

	// Storage
	CUtlHashMapLarge<int, CEconItemDefinition*> m_mapItems;        // By def index
	CUtlDict<CEconItemDefinition*> m_dictItemsByName;              // By name
	CUtlDict<KeyValues*> m_dictPrefabs;                            // Prefab definitions
	CUtlDict<KeyValues*> m_dictMergedPrefabs;                      // Cached merged prefabs

	bool m_bInitialized;
};

// Helper to get the loadout slot from a string
int LoadoutSlotFromString( const char *pszSlot );

// Helper to get team index from string
int TeamFromString( const char *pszTeam );

#endif // ECON_ITEM_SCHEMA_MINIMAL_H
