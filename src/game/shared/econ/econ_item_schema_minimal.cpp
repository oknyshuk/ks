//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Minimal econ item schema implementation
//          Parses items_game.txt for weapon definitions
//
//=============================================================================

#include "cbase.h"
#include "econ_item_schema_minimal.h"
#include "filesystem.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Loadout slot string table
//-----------------------------------------------------------------------------
static const char *s_pszLoadoutSlots[] = {
	"melee",           // 0
	"c4",              // 1
	"secondary0",      // 2
	"secondary1",      // 3
	"secondary2",      // 4
	"secondary3",      // 5
	"secondary4",      // 6
	"smg0",            // 7
	"smg1",            // 8
	"smg2",            // 9
	"smg3",            // 10
	"smg4",            // 11
	"rifle0",          // 12
	"rifle1",          // 13
	"rifle2",          // 14
	"rifle3",          // 15
	"rifle4",          // 16
	"rifle5",          // 17
	"heavy0",          // 18
	"heavy1",          // 19
	"heavy2",          // 20
	"heavy3",          // 21
	"heavy4",          // 22
	"heavy5",          // 23
	"grenade0",        // 24
	"grenade1",        // 25
	"grenade2",        // 26
	"grenade3",        // 27
	"grenade4",        // 28
	"grenade5",        // 29
	"equipment0",      // 30
	"equipment1",      // 31
	"equipment2",      // 32
	"equipment3",      // 33
	"equipment4",      // 34
	"clothing_hands",  // 35
	"musickit",        // 36
	"flair0",          // 37
	"spray0",          // 38
	"secondary",       // maps to secondary0
	"equipment",       // maps to equipment0
	"grenade",         // maps to grenade0
};

//-----------------------------------------------------------------------------
// LoadoutSlotFromString - Convert loadout slot string to enum
//-----------------------------------------------------------------------------
int LoadoutSlotFromString( const char *pszSlot )
{
	if ( !pszSlot || !*pszSlot )
		return LOADOUT_POSITION_INVALID;

	for ( int i = 0; i < ARRAYSIZE( s_pszLoadoutSlots ); i++ )
	{
		if ( V_stricmp( pszSlot, s_pszLoadoutSlots[i] ) == 0 )
		{
			// Handle aliases
			if ( i >= 39 )
			{
				if ( V_stricmp( pszSlot, "secondary" ) == 0 )
					return LOADOUT_POSITION_SECONDARY0;
				if ( V_stricmp( pszSlot, "equipment" ) == 0 )
					return LOADOUT_POSITION_EQUIPMENT0;
				if ( V_stricmp( pszSlot, "grenade" ) == 0 )
					return LOADOUT_POSITION_GRENADE0;
			}
			return i;
		}
	}

	return LOADOUT_POSITION_INVALID;
}

//-----------------------------------------------------------------------------
// TeamFromString - Convert team string to team index
//-----------------------------------------------------------------------------
int TeamFromString( const char *pszTeam )
{
	if ( !pszTeam || !*pszTeam )
		return TEAM_UNASSIGNED;

	if ( V_stricmp( pszTeam, "terrorists" ) == 0 )
		return TEAM_TERRORIST;
	if ( V_stricmp( pszTeam, "counter-terrorists" ) == 0 )
		return TEAM_CT;
	if ( V_stricmp( pszTeam, "noteam" ) == 0 )
		return TEAM_UNASSIGNED;

	return TEAM_UNASSIGNED;
}

//=============================================================================
// CEconItemDefinition
//=============================================================================

CEconItemDefinition::CEconItemDefinition()
{
	m_nDefIndex = 0;
	m_szDefinitionName[0] = '\0';
	m_szItemClass[0] = '\0';
	m_szItemBaseName[0] = '\0';
	m_iDefaultLoadoutSlot = LOADOUT_POSITION_INVALID;
	m_iPrimaryReserveAmmoMax = 0;
	m_iPrimaryClipSize = 0;
	m_bHidden = false;
	m_bBaseItem = false;
	m_bDefaultSlotItem = false;
	m_szModelPlayer[0] = '\0';
	m_szModelWorld[0] = '\0';
	m_szWorldDroppedModel[0] = '\0';
	m_pKVItem = NULL;

	for ( int i = 0; i < LOADOUT_COUNT; i++ )
	{
		m_iLoadoutSlots[i] = LOADOUT_POSITION_INVALID;
		m_bClassUsability[i] = false;
	}
}

CEconItemDefinition::~CEconItemDefinition()
{
	if ( m_pKVItem )
	{
		m_pKVItem->deleteThis();
		m_pKVItem = NULL;
	}
}

//-----------------------------------------------------------------------------
// BInitFromKV - Initialize item definition from KeyValues
//-----------------------------------------------------------------------------
bool CEconItemDefinition::BInitFromKV( KeyValues *pKV, KeyValues *pPrefabs, CEconItemSchema *pSchema )
{
	if ( !pKV )
		return false;

	// Store a copy of the raw KV for additional lookups
	m_pKVItem = pKV->MakeCopy();

	// Parse def index from key name
	const char *pszDefIndex = pKV->GetName();
	if ( V_stricmp( pszDefIndex, "default" ) == 0 )
	{
		m_nDefIndex = 0;
	}
	else
	{
		m_nDefIndex = (uint16)V_atoi( pszDefIndex );
	}

	// Get name
	const char *pszName = pKV->GetString( "name", "" );
	V_strncpy( m_szDefinitionName, pszName, sizeof( m_szDefinitionName ) );

	// Get item class (entity class name)
	const char *pszItemClass = pKV->GetString( "item_class", "" );
	V_strncpy( m_szItemClass, pszItemClass, sizeof( m_szItemClass ) );

	// Get item base name (display name token)
	const char *pszItemBaseName = pKV->GetString( "item_name", "" );
	V_strncpy( m_szItemBaseName, pszItemBaseName, sizeof( m_szItemBaseName ) );

	// Get loadout slot from item_sub_position (preferred) or item_slot
	const char *pszSubPosition = pKV->GetString( "item_sub_position", NULL );
	const char *pszItemSlot = pKV->GetString( "item_slot", NULL );

	if ( pszSubPosition && *pszSubPosition )
	{
		m_iDefaultLoadoutSlot = LoadoutSlotFromString( pszSubPosition );
	}
	else if ( pszItemSlot && *pszItemSlot )
	{
		m_iDefaultLoadoutSlot = LoadoutSlotFromString( pszItemSlot );
	}

	// Initialize all loadout slots to default
	for ( int i = 0; i < LOADOUT_COUNT; i++ )
	{
		m_iLoadoutSlots[i] = m_iDefaultLoadoutSlot;
	}

	// Parse used_by_classes
	KeyValues *pClasses = pKV->FindKey( "used_by_classes" );
	if ( pClasses )
	{
		for ( KeyValues *pClass = pClasses->GetFirstSubKey(); pClass; pClass = pClass->GetNextKey() )
		{
			const char *pszClassName = pClass->GetName();
			int iTeam = TeamFromString( pszClassName );

			if ( iTeam == TEAM_TERRORIST || iTeam == TEAM_CT )
			{
				m_bClassUsability[iTeam] = true;

				// Check if a specific slot is specified for this team
				const char *pszValue = pClass->GetString();
				if ( pszValue && pszValue[0] != '1' && pszValue[0] != '\0' )
				{
					int iSlot = LoadoutSlotFromString( pszValue );
					if ( iSlot != LOADOUT_POSITION_INVALID )
					{
						m_iLoadoutSlots[iTeam] = iSlot;
					}
				}
			}
			else if ( V_stricmp( pszClassName, "noteam" ) == 0 )
			{
				// "noteam" means usable by all teams
				for ( int i = 0; i < LOADOUT_COUNT; i++ )
				{
					m_bClassUsability[i] = true;
				}
			}
		}
	}
	else
	{
		// If no used_by_classes specified, assume usable by all teams
		// (This matches default behavior for items like grenades)
		for ( int i = 0; i < LOADOUT_COUNT; i++ )
		{
			m_bClassUsability[i] = true;
		}
	}

	// Parse attributes section for ammo values
	KeyValues *pAttribs = pKV->FindKey( "attributes" );
	if ( pAttribs )
	{
		const char *pszReserveAmmo = pAttribs->GetString( "primary reserve ammo max", NULL );
		if ( pszReserveAmmo )
		{
			m_iPrimaryReserveAmmoMax = V_atoi( pszReserveAmmo );
		}

		const char *pszClipSize = pAttribs->GetString( "primary clip size", NULL );
		if ( pszClipSize )
		{
			m_iPrimaryClipSize = V_atoi( pszClipSize );
		}
	}

	// Parse flags
	m_bHidden = pKV->GetBool( "hidden", false );
	m_bBaseItem = pKV->GetBool( "baseitem", false );
	m_bDefaultSlotItem = pKV->GetBool( "default_slot_item", false );

	// Parse models
	const char *pszModelPlayer = pKV->GetString( "model_player", "" );
	V_strncpy( m_szModelPlayer, pszModelPlayer, sizeof( m_szModelPlayer ) );

	const char *pszModelWorld = pKV->GetString( "model_world", "" );
	V_strncpy( m_szModelWorld, pszModelWorld, sizeof( m_szModelWorld ) );

	const char *pszWorldDroppedModel = pKV->GetString( "model_world_dropped", "" );
	V_strncpy( m_szWorldDroppedModel, pszWorldDroppedModel, sizeof( m_szWorldDroppedModel ) );

	return true;
}

//-----------------------------------------------------------------------------
// GetLoadoutSlot - Get loadout slot for a specific team
//-----------------------------------------------------------------------------
int CEconItemDefinition::GetLoadoutSlot( int iTeam ) const
{
	if ( iTeam < 0 || iTeam >= LOADOUT_COUNT )
		return m_iDefaultLoadoutSlot;

	return m_iLoadoutSlots[iTeam];
}

//-----------------------------------------------------------------------------
// GetUsedByTeam - Get which team(s) can use this item
//-----------------------------------------------------------------------------
int CEconItemDefinition::GetUsedByTeam() const
{
	bool bT = CanBeUsedByTeam( TEAM_TERRORIST );
	bool bCT = CanBeUsedByTeam( TEAM_CT );

	if ( bT && bCT )
		return TEAM_UNASSIGNED; // Both teams

	if ( bT )
		return TEAM_TERRORIST;

	if ( bCT )
		return TEAM_CT;

	return TEAM_UNASSIGNED;
}

//-----------------------------------------------------------------------------
// CanBeUsedByTeam - Check if a team can use this item
//-----------------------------------------------------------------------------
bool CEconItemDefinition::CanBeUsedByTeam( int iTeam ) const
{
	if ( iTeam < 0 || iTeam >= LOADOUT_COUNT )
		return false;

	return m_bClassUsability[iTeam];
}

//=============================================================================
// CEconItemSchema
//=============================================================================

CEconItemSchema::CEconItemSchema()
{
	m_bInitialized = false;
}

CEconItemSchema::~CEconItemSchema()
{
	// Clean up item definitions
	for ( int i = 0; i < m_mapItems.MaxElement(); i++ )
	{
		if ( m_mapItems.IsValidIndex( i ) )
			delete m_mapItems[i];
	}
	m_mapItems.Purge();
	m_dictItemsByName.Purge();

	// Clean up prefabs
	for ( int i = m_dictPrefabs.First(); i != m_dictPrefabs.InvalidIndex(); i = m_dictPrefabs.Next( i ) )
	{
		if ( m_dictPrefabs[i] )
		{
			m_dictPrefabs[i]->deleteThis();
		}
	}
	m_dictPrefabs.Purge();

	// Clean up merged prefabs
	for ( int i = m_dictMergedPrefabs.First(); i != m_dictMergedPrefabs.InvalidIndex(); i = m_dictMergedPrefabs.Next( i ) )
	{
		if ( m_dictMergedPrefabs[i] )
		{
			m_dictMergedPrefabs[i]->deleteThis();
		}
	}
	m_dictMergedPrefabs.Purge();
}

//-----------------------------------------------------------------------------
// BInit - Initialize schema from items_game.txt
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInit( const char *pszFilename )
{
	if ( m_bInitialized )
		return true;

	DevMsg( "CEconItemSchema::BInit - Loading %s\n", pszFilename );

	KeyValues *pKV = new KeyValues( "items_game" );
	if ( !pKV->LoadFromFile( g_pFullFileSystem, pszFilename, "GAME" ) )
	{
		Warning( "CEconItemSchema::BInit - Failed to load %s\n", pszFilename );
		pKV->deleteThis();
		return false;
	}

	// Parse prefabs first
	KeyValues *pPrefabs = pKV->FindKey( "prefabs" );
	if ( pPrefabs )
	{
		BInitPrefabs( pPrefabs );
	}

	// Parse items
	KeyValues *pItems = pKV->FindKey( "items" );
	if ( pItems )
	{
		BInitItems( pItems );
	}

	pKV->deleteThis();
	m_bInitialized = true;
	DevMsg( "CEconItemSchema::BInit - Loaded %d item definitions\n", m_mapItems.Count() );

	return true;
}

//-----------------------------------------------------------------------------
// RecursiveInheritKeyValues - Copy values from pInstance into out_pValues
// This matches Valve's original implementation
//-----------------------------------------------------------------------------
static void RecursiveInheritKeyValues( KeyValues *out_pValues, KeyValues *pInstance )
{
	FOR_EACH_SUBKEY( pInstance, pSubKey )
	{
		KeyValues::types_t eType = pSubKey->GetDataType();
		switch ( eType )
		{
		case KeyValues::TYPE_STRING:	out_pValues->SetString( pSubKey->GetName(), pSubKey->GetString() );		break;
		case KeyValues::TYPE_INT:		out_pValues->SetInt( pSubKey->GetName(), pSubKey->GetInt() );			break;
		case KeyValues::TYPE_FLOAT:		out_pValues->SetFloat( pSubKey->GetName(), pSubKey->GetFloat() );		break;
		case KeyValues::TYPE_WSTRING:	out_pValues->SetWString( pSubKey->GetName(), pSubKey->GetWString() );	break;
		case KeyValues::TYPE_COLOR:		out_pValues->SetColor( pSubKey->GetName(), pSubKey->GetColor() );		break;
		case KeyValues::TYPE_UINT64:	out_pValues->SetUint64( pSubKey->GetName(), pSubKey->GetUint64() );		break;

		// TYPE_NONE means it's a subsection (KeyValues block)
		case KeyValues::TYPE_NONE:
		{
			KeyValues *pNewChild = out_pValues->FindKey( pSubKey->GetName() );
			if ( !pNewChild )
			{
				pNewChild = out_pValues->CreateNewKey();
				pNewChild->SetName( pSubKey->GetName() );
			}
			RecursiveInheritKeyValues( pNewChild, pSubKey );
			break;
		}

		default:
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// BInitPrefabs - Parse prefabs section
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitPrefabs( KeyValues *pKVPrefabs )
{
	if ( !pKVPrefabs )
		return false;

	for ( KeyValues *pPrefab = pKVPrefabs->GetFirstSubKey(); pPrefab; pPrefab = pPrefab->GetNextKey() )
	{
		const char *pszPrefabName = pPrefab->GetName();
		if ( pszPrefabName && *pszPrefabName )
		{
			// Store a copy of the prefab
			m_dictPrefabs.Insert( pszPrefabName, pPrefab->MakeCopy() );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// GetMergedPrefab - Get a prefab with all inheritance resolved
//-----------------------------------------------------------------------------
KeyValues *CEconItemSchema::GetMergedPrefab( const char *pszPrefabName )
{
	if ( !pszPrefabName || !*pszPrefabName )
		return NULL;

	// Check if we've already merged this prefab
	int iCached = m_dictMergedPrefabs.Find( pszPrefabName );
	if ( iCached != m_dictMergedPrefabs.InvalidIndex() )
	{
		return m_dictMergedPrefabs[iCached];
	}

	// Find the base prefab
	int iBase = m_dictPrefabs.Find( pszPrefabName );
	if ( iBase == m_dictPrefabs.InvalidIndex() )
	{
		return NULL;
	}

	KeyValues *pBasePrefab = m_dictPrefabs[iBase];
	if ( !pBasePrefab )
		return NULL;

	// Check if this prefab inherits from another
	const char *pszParentPrefab = pBasePrefab->GetString( "prefab", NULL );

	KeyValues *pMerged = NULL;
	if ( pszParentPrefab && *pszParentPrefab )
	{
		// Recursively get the parent prefab (already merged)
		KeyValues *pParent = GetMergedPrefab( pszParentPrefab );
		if ( pParent )
		{
			// Start with a copy of the parent
			pMerged = pParent->MakeCopy();
			// Merge this prefab's values on top using proper recursive merge
			RecursiveInheritKeyValues( pMerged, pBasePrefab );
		}
		else
		{
			pMerged = pBasePrefab->MakeCopy();
		}
	}
	else
	{
		pMerged = pBasePrefab->MakeCopy();
	}

	// Cache the merged prefab
	m_dictMergedPrefabs.Insert( pszPrefabName, pMerged );

	return pMerged;
}

//-----------------------------------------------------------------------------
// MergeWithPrefab - Merge item KV with its prefab
// Item values override prefab values (matches Valve's InheritKeyValuesRTLMulti)
//-----------------------------------------------------------------------------
KeyValues *CEconItemSchema::MergeWithPrefab( KeyValues *pKVItem, const char *pszPrefab )
{
	if ( !pKVItem )
		return NULL;

	KeyValues *pMerged = NULL;

	if ( pszPrefab && *pszPrefab )
	{
		KeyValues *pPrefab = GetMergedPrefab( pszPrefab );
		if ( pPrefab )
		{
			// Start with a copy of the prefab
			pMerged = pPrefab->MakeCopy();
			// Set the correct name (item def index)
			pMerged->SetName( pKVItem->GetName() );
			// Recursively inherit item values on top of prefab
			RecursiveInheritKeyValues( pMerged, pKVItem );
		}
		else
		{
			pMerged = pKVItem->MakeCopy();
		}
	}
	else
	{
		pMerged = pKVItem->MakeCopy();
	}

	return pMerged;
}

//-----------------------------------------------------------------------------
// BInitItems - Parse items section
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitItems( KeyValues *pKVItems )
{
	if ( !pKVItems )
		return false;

	for ( KeyValues *pItem = pKVItems->GetFirstSubKey(); pItem; pItem = pItem->GetNextKey() )
	{
		// Get prefab name if any
		const char *pszPrefab = pItem->GetString( "prefab", NULL );

		// Merge with prefab
		KeyValues *pMergedKV = MergeWithPrefab( pItem, pszPrefab );
		if ( !pMergedKV )
			continue;

		// Create the item definition
		CEconItemDefinition *pDef = new CEconItemDefinition();
		if ( !pDef->BInitFromKV( pMergedKV, NULL, this ) )
		{
			delete pDef;
			pMergedKV->deleteThis();
			continue;
		}

		// Store by def index
		int idx = m_mapItems.Find( pDef->GetDefinitionIndex() );
		if ( idx == m_mapItems.InvalidIndex() )
		{
			m_mapItems.Insert( pDef->GetDefinitionIndex(), pDef );
		}
		else
		{
			// Duplicate def index - replace
			delete m_mapItems[idx];
			m_mapItems[idx] = pDef;
		}

		// Store by name
		const char *pszName = pDef->GetDefinitionName();
		if ( pszName && *pszName )
		{
			int iNameIdx = m_dictItemsByName.Find( pszName );
			if ( iNameIdx == m_dictItemsByName.InvalidIndex() )
			{
				m_dictItemsByName.Insert( pszName, pDef );
			}
			else
			{
				// Duplicate name - replace
				m_dictItemsByName[iNameIdx] = pDef;
			}
		}

		// Clean up merged KV (item took a copy)
		pMergedKV->deleteThis();
	}

	return true;
}

//-----------------------------------------------------------------------------
// GetItemDefinition - Get item definition by def index
//-----------------------------------------------------------------------------
CEconItemDefinition *CEconItemSchema::GetItemDefinition( uint16 nDefIndex ) const
{
	int idx = m_mapItems.Find( nDefIndex );
	if ( idx == m_mapItems.InvalidIndex() )
		return NULL;

	return m_mapItems[idx];
}

//-----------------------------------------------------------------------------
// GetItemDefinitionByName - Get item definition by name
//-----------------------------------------------------------------------------
CEconItemDefinition *CEconItemSchema::GetItemDefinitionByName( const char *pszName ) const
{
	if ( !pszName || !*pszName )
		return NULL;

	int idx = m_dictItemsByName.Find( pszName );
	if ( idx == m_dictItemsByName.InvalidIndex() )
		return NULL;

	return m_dictItemsByName[idx];
}

//-----------------------------------------------------------------------------
// GetItemDefinitionByMapIndex - Get item by iteration index
//-----------------------------------------------------------------------------
CEconItemDefinition *CEconItemSchema::GetItemDefinitionByMapIndex( int index ) const
{
	if ( index < 0 || index >= m_mapItems.Count() )
		return NULL;

	return m_mapItems.Element( index );
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// PrecacheItemDefinitionModels - Precache all models from item definitions
//-----------------------------------------------------------------------------
void CEconItemSchema::PrecacheItemDefinitionModels() const
{
	FOR_EACH_MAP_FAST( m_mapItems, i )
	{
		CEconItemDefinition *pDef = m_mapItems[i];
		if ( !pDef )
			continue;

		const char *pszModel = pDef->GetBasePlayerDisplayModel();
		if ( pszModel && pszModel[0] )
			CBaseEntity::PrecacheModel( pszModel );

		pszModel = pDef->GetWorldDisplayModel();
		if ( pszModel && pszModel[0] )
			CBaseEntity::PrecacheModel( pszModel );

		pszModel = pDef->GetWorldDroppedModel();
		if ( pszModel && pszModel[0] )
			CBaseEntity::PrecacheModel( pszModel );
	}
}
#endif
