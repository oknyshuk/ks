// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Weapon data file parsing, shared by game & client dlls.

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
#if defined(CLIENT_DLL)
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
#include "ndebugoverlay.h"
#endif
#include "filesystem.h"
#include "ammodef.h"
#include "util_shared.h"
#include "weapon_parse.h"

// Static default item definition for fallback (never returns NULL for backwards compatibility)
static CEconItemDefinition s_DefaultItemDef;

CEconItemDefinition *CEconItemView::GetItemDefinition() const
{
	return m_pItemDef ? m_pItemDef : &s_DefaultItemDef;
}

CEconItemDefinition *CEconItemView::GetStaticData() const
{
	return m_pItemDef ? m_pItemDef : &s_DefaultItemDef;
}

CWeaponDatabase g_WeaponDatabase;

extern void LoadEquipmentData();

// The sound categories found in the weapon classname.txt files
// This needs to match the WeaponSound_t enum in weapon_parse.h
const char *pWeaponSoundCategories[ NUM_SHOOT_SOUND_TYPES ] = 
{
	"empty",
	"single_shot",
	"single_shot_accurate",
	"single_shot_npc",
	"double_shot",
	"double_shot_npc",
	"burst",
	"reload",
	"reload_npc",
	"melee_miss",
	"melee_hit",
	"melee_hit_world",
	"special1",
	"special2",
	"special3",
	"taunt",
	"nearlyempty",
	"fastreload"
};

int GetWeaponSoundFromString( const char *pszString )
{
	for ( int i = EMPTY; i < NUM_SHOOT_SOUND_TYPES; i++ )
	{
		if ( !V_stricmp(pszString,pWeaponSoundCategories[i]) )
			return (WeaponSound_t)i;
	}
	return -1;
}


// Item flags that we parse out of the file.
typedef struct
{
	const char *m_pFlagName;
	int m_iFlagValue;
} itemFlags_t;
itemFlags_t g_ItemFlags[8] =
{
	{ "ITEM_FLAG_SELECTONEMPTY",	ITEM_FLAG_SELECTONEMPTY },
	{ "ITEM_FLAG_NOAUTORELOAD",		ITEM_FLAG_NOAUTORELOAD },
	{ "ITEM_FLAG_NOAUTOSWITCHEMPTY", ITEM_FLAG_NOAUTOSWITCHEMPTY },
	{ "ITEM_FLAG_LIMITINWORLD",		ITEM_FLAG_LIMITINWORLD },
	{ "ITEM_FLAG_EXHAUSTIBLE",		ITEM_FLAG_EXHAUSTIBLE },
	{ "ITEM_FLAG_DOHITLOCATIONDMG", ITEM_FLAG_DOHITLOCATIONDMG },
	{ "ITEM_FLAG_NOAMMOPICKUPS",	ITEM_FLAG_NOAMMOPICKUPS },
	{ "ITEM_FLAG_NOITEMPICKUP",		ITEM_FLAG_NOITEMPICKUP }
};



#ifdef _DEBUG
// used to track whether or not two weapons have been mistakenly assigned the wrong slot
bool g_bUsedWeaponSlots[MAX_WEAPON_SLOTS][MAX_WEAPON_POSITIONS] = { 0 };

#endif

// Find a weapon slot, assuming the weapon's data has already been loaded.
WEAPON_FILE_INFO_HANDLE LookupWeaponInfoSlot( const char *name )
{
	return g_WeaponDatabase.FindWeaponInfo( name );
}

FileWeaponInfo_t *GetFileWeaponInfoFromHandle( WEAPON_FILE_INFO_HANDLE handle )
{
	return g_WeaponDatabase.GetFileWeaponInfoFromHandle( handle );
}

void PrecacheFileWeaponInfoDatabase()
{
	g_WeaponDatabase.PrecacheAllWeapons();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : WEAPON_FILE_INFO_HANDLE
//-----------------------------------------------------------------------------
WEAPON_FILE_INFO_HANDLE GetInvalidWeaponInfoHandle( void )
{
	return (WEAPON_FILE_INFO_HANDLE)-1;
}


KeyValues* ReadEncryptedKVFile( IFileSystem *filesystem, const char *szFilenameWithoutExtension, const unsigned char *pICEKey, bool bForceReadEncryptedFile )
{
	contract_assert( std::strchr( szFilenameWithoutExtension, '.' ) == nullptr );
	char szFullName[512];

	const char *pSearchPath = "GAME";

	// Open the weapon data file, and abort if we can't
	KeyValues *pKV = new KeyValues( "WeaponDatafile" );
	pKV->UsesEscapeSequences( true );

	V_snprintf(szFullName,sizeof(szFullName), "%s.txt", szFilenameWithoutExtension);

	if ( bForceReadEncryptedFile || !pKV->LoadFromFile( filesystem, szFullName, pSearchPath ) ) // try to load the normal .txt file first
	{
		if ( pICEKey )
		{
			V_snprintf(szFullName,sizeof(szFullName), "%s.ctx", szFilenameWithoutExtension); // fall back to the .ctx file

			FileHandle_t f = filesystem->Open( szFullName, "rb", pSearchPath );

			if (!f)
			{
				pKV->deleteThis();
				return nullptr;
			}
			// load file into a null-terminated buffer
			int fileSize = filesystem->Size(f);
			char *buffer = (char*)MemAllocScratch(fileSize + 1);
		
			contract_assert( buffer );
		
			filesystem->Read(buffer, fileSize, f); // read into local buffer
			buffer[fileSize] = 0; // null terminate file as EOF
			filesystem->Close( f );	// close file after reading

			UTIL_DecodeICE( (unsigned char*)buffer, fileSize, pICEKey );

			bool retOK = pKV->LoadFromBuffer( szFullName, buffer, filesystem );

			MemFreeScratch();

			if ( !retOK )
			{
				pKV->deleteThis();
				return nullptr;
			}
		}
		else
		{
			pKV->deleteThis();
			return nullptr;
		}
	}

	return pKV;
}


std::vector< WeaponInfoLookup* > FileWeaponInfo_t::ms_vecWeaponInfoLookup;

bool							FileWeaponInfo_t::ms_bWeaponInfoLookupInitialized;


WeaponInfoLookup::WeaponInfoLookup( size_t offset, _fieldtypes p_fieldType, const char *szAttribClassName )
{
	m_nWeaponParseDataOffset = offset;
	m_fieldType = p_fieldType;
	m_iszAttribClassName.Set( szAttribClassName );
}

WeaponInfoLookup::WeaponInfoLookup( const WeaponInfoLookup &WepInfoLookup )
{
	m_nWeaponParseDataOffset = WepInfoLookup.m_nWeaponParseDataOffset;
	m_fieldType = WepInfoLookup.m_fieldType;
	m_iszAttribClassName.Set( STRING( WepInfoLookup.m_iszAttribClassName.Get() ) );
}

//-----------------------------------------------------------------------------
// FileWeaponInfo_t implementation.
//-----------------------------------------------------------------------------

FileWeaponInfo_t::FileWeaponInfo_t()
{
	bParsedScript = false;
	bLoadedHudElements = false;
	szClassName[0] = 0;
	szPrintName[0] = 0;

	szViewModel[0] = 0;
	szWorldModel[0] = 0;
	szAnimationPrefix[0] = 0;
	iSlot = 0;
	iPosition = 0;
	iMaxClip1 = 0;
	iMaxClip2 = 0;
	iDefaultClip1 = 0;
	iDefaultClip2 = 0;
	iWeight = 0;
	iRumbleEffect = -1;
	bAutoSwitchTo = false;
	bAutoSwitchFrom = false;
	iFlags = 0;
	szAmmo1[0] = 0;
	szAmmo2[0] = 0;
	szAIAddOn[0] = 0;
	std::memset( aShootSounds, 0, sizeof( aShootSounds ) );
	iAmmoType = 0;
	iAmmo2Type = 0;
	m_bMeleeWeapon = false;
	iSpriteCount = 0;
	iconActive = 0;
	iconInactive = 0;
	iconAmmo = 0;
	iconAmmo2 = 0;
	iconCrosshair = 0;
	iconAutoaim = 0;
	iconZoomedCrosshair = 0;
	iconZoomedAutoaim = 0;
	bShowUsageHint = false;
	m_bAllowFlipping = true;
	m_bBuiltRightHanded = true;

	if ( !ms_bWeaponInfoLookupInitialized )
	{
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( FileWeaponInfo_t, iMaxClip1 ), FIELD_INTEGER, "primary clip size" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( FileWeaponInfo_t, iMaxClip2 ), FIELD_INTEGER, "secondary clip size" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( FileWeaponInfo_t, iDefaultClip1 ), FIELD_INTEGER, "primary default clip size" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( FileWeaponInfo_t, iDefaultClip2 ), FIELD_INTEGER, "secondary default clip size" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( FileWeaponInfo_t, iSlot ), FIELD_INTEGER, "bucket slot" ) );

		std::ranges::stable_sort( ms_vecWeaponInfoLookup, CWeaponInfoLookupListLess() );
		ms_bWeaponInfoLookupInitialized = true;
	}
}

#ifdef CLIENT_DLL
extern ConVar hud_fastswitch;
#endif

void FileWeaponInfo_t::Parse( KeyValues *pKeyValuesData, const char *szWeaponName )
{
	// Okay, we tried at least once to look this up...
	bParsedScript = true;

	// Classname
	V_strncpy( szClassName, szWeaponName, MAX_WEAPON_STRING );
	// Printable name
	V_strncpy( szPrintName, pKeyValuesData->GetString( "printname", WEAPON_PRINTNAME_MISSING ), MAX_WEAPON_STRING );
	// View model & world model
	V_strncpy( szViewModel, pKeyValuesData->GetString( "viewmodel" ), MAX_WEAPON_STRING );
	V_strncpy( szWorldModel, pKeyValuesData->GetString( "playermodel" ), MAX_WEAPON_STRING );

	V_StripExtension( szWorldModel, szWorldDroppedModel, sizeof( szWorldDroppedModel ) );
	V_strcat_safe( szWorldDroppedModel, "_dropped.mdl" );
	
	V_strncpy( szAnimationPrefix, pKeyValuesData->GetString( "anim_prefix" ), MAX_WEAPON_PREFIX );
	iSlot = pKeyValuesData->GetInt( "bucket", 0 );
	iPosition = pKeyValuesData->GetInt( "bucket_position", 0 );
	
	// Use the console (X360) buckets if hud_fastswitch is set to 2.
#ifdef CLIENT_DLL
	if ( hud_fastswitch.GetInt() == 2 )
	{
		iSlot = pKeyValuesData->GetInt( "bucket_360", iSlot );
		iPosition = pKeyValuesData->GetInt( "bucket_position_360", iPosition );
	}
#endif
	iMaxClip1 = pKeyValuesData->GetInt( "clip_size", WEAPON_NOCLIP );					// Max primary clips gun can hold (assume they don't use clips by default)
	iMaxClip2 = pKeyValuesData->GetInt( "clip2_size", WEAPON_NOCLIP );					// Max secondary clips gun can hold (assume they don't use clips by default)
	iDefaultClip1 = pKeyValuesData->GetInt( "default_clip", iMaxClip1 );		// amount of primary ammo placed in the primary clip when it's picked up
	iDefaultClip2 = pKeyValuesData->GetInt( "default_clip2", iMaxClip2 );		// amount of secondary ammo placed in the secondary clip when it's picked up
	iWeight = pKeyValuesData->GetInt( "weight", 0 );

	iRumbleEffect = pKeyValuesData->GetInt( "rumble", -1 );
	
	// LAME old way to specify item flags.
	// Weapon scripts should use the flag names.
	iFlags = pKeyValuesData->GetInt( "item_flags", ITEM_FLAG_LIMITINWORLD );

	for ( int i=0; i < std::size( g_ItemFlags ); i++ )
	{
		int iVal = pKeyValuesData->GetInt( g_ItemFlags[i].m_pFlagName, -1 );
		if ( iVal == 0 )
		{
			iFlags &= ~g_ItemFlags[i].m_iFlagValue;
		}
		else if ( iVal == 1 )
		{
			iFlags |= g_ItemFlags[i].m_iFlagValue;
		}
	}

	bShowUsageHint = pKeyValuesData->GetBool( "showusagehint", false );
	bAutoSwitchTo = pKeyValuesData->GetBool( "autoswitchto", true );
	bAutoSwitchFrom = pKeyValuesData->GetBool( "autoswitchfrom", true );
	m_bBuiltRightHanded = pKeyValuesData->GetBool( "BuiltRightHanded", true );
	m_bAllowFlipping = pKeyValuesData->GetBool( "AllowFlipping", true );
	m_bMeleeWeapon = pKeyValuesData->GetBool( "MeleeWeapon", false );


	// Primary ammo used
	const char *pAmmo = pKeyValuesData->GetString( "primary_ammo", "None" );
	if ( std::strcmp("None", pAmmo) == 0 )
		V_strncpy( szAmmo1, "", sizeof( szAmmo1 ) );
	else
		V_strncpy( szAmmo1, pAmmo, sizeof( szAmmo1 )  );
	iAmmoType = GetAmmoDef()->Index( szAmmo1 );
	
	// Secondary ammo used
	pAmmo = pKeyValuesData->GetString( "secondary_ammo", "None" );
	if ( std::strcmp("None", pAmmo) == 0)
		V_strncpy( szAmmo2, "", sizeof( szAmmo2 ) );
	else
		V_strncpy( szAmmo2, pAmmo, sizeof( szAmmo2 )  );
	iAmmo2Type = GetAmmoDef()->Index( szAmmo2 );

	// AI AddOn
	const char *pAIAddOn = pKeyValuesData->GetString( "ai_addon", "ai_addon_basecombatweapon" );
	if ( std::strcmp("None", pAIAddOn) == 0)
		V_strncpy( szAIAddOn, "", sizeof( szAIAddOn ) );
	else
		V_strncpy( szAIAddOn, pAIAddOn, sizeof( szAIAddOn )  );

	// Now read the weapon sounds
	std::memset( aShootSounds, 0, sizeof( aShootSounds ) );
	KeyValues *pSoundData = pKeyValuesData->FindKey( "SoundData" );
	if ( pSoundData )
	{
		for ( int i = EMPTY; i < NUM_SHOOT_SOUND_TYPES; i++ )
		{
			const char *soundname = pSoundData->GetString( pWeaponSoundCategories[i] );
			if ( soundname && soundname[0] )
			{
				V_strncpy( aShootSounds[i], soundname, MAX_WEAPON_STRING );
			}
			else
			{
				// FIXME: find a better way to populate defaults
			
				if ( i == NEARLYEMPTY ) // explicit check because it looks like most weapon sound categories don't fall back by default
				{
					V_sprintf_safe( aShootSounds[i], "Default.%s", pWeaponSoundCategories[i] );
				}
			}
		}
	}
}

const char* FileWeaponInfo_t::GetWorldModel( const CEconItemView* pWepView, int iTeam ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		const char *pchWorldOverride = pWepView->GetStaticData()->GetEntityOverrideModel();
		if ( pchWorldOverride )
		{
			return pchWorldOverride;
		}

		return pWepView->GetItemDefinition()->GetWorldDisplayModel();
	}
	else
	{
		return szWorldModel;
	}
}

const char* FileWeaponInfo_t::GetViewModel( const CEconItemView* pWepView, int iTeam ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		const char *pchViewOverride = pWepView->GetStaticData()->GetViewOverrideModel();
		if ( pchViewOverride )
		{
			return pchViewOverride;
		}

		return pWepView->GetItemDefinition()->GetBasePlayerDisplayModel();
	}
	else
	{
		return szViewModel;
	}
}

const char* FileWeaponInfo_t::GetWorldDroppedModel( const CEconItemView* pWepView, int iTeam ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		const char *pchWorldDroppedModel = pWepView->GetItemDefinition()->GetWorldDroppedModel();
		if ( pchWorldDroppedModel )
		{
			return pchWorldDroppedModel;
		}
	}
	
	return szWorldDroppedModel;
}

const char* FileWeaponInfo_t::GetPrimaryAmmo( const CEconItemView* pWepView ) const
{

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetPrimaryAmmo();

		if ( pszString )
		{
			return pszString;
		}
	}

	return szAmmo1;
}


int FileWeaponInfo_t::GetPrimaryAmmoType( const CEconItemView* pWepView ) const
{

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = GetPrimaryAmmo( pWepView );

		if ( pszString )
		{
			return GetAmmoDef()->Index( pszString );
		}
	}

	return iAmmoType;
}


static int WeaponInfoLookupCompare( WeaponInfoLookup * const * src1, WeaponInfoLookup * const * src2 )
{
	if ( ( *src1 )->m_iszAttribClassName.Get() == ( *src2 )->m_iszAttribClassName.Get() )
		return 0;

	if ( ( *src1 )->m_iszAttribClassName.Get() < ( *src2 )->m_iszAttribClassName.Get() )
		return -1;

	return 1;
}

// Convert a schema attribute classname into an index of g_WeaponInfoTable
int FileWeaponInfo_t::GetIndexofAttribute( string_t iszAttribClassName ) const
{
	if ( ms_vecWeaponInfoLookup.empty() )
		return -1;

	if ( ms_vecWeaponInfoLookup[ 0 ]->m_iszAttribClassName.IsSerialNumberOutOfDate() )
	{
		// If the strings are out of date, rebuild and resort them
		for ( WeaponInfoLookup *pWeaponInfoLookup : ms_vecWeaponInfoLookup )
		{
			pWeaponInfoLookup->m_iszAttribClassName.Get();
		}

		ks::qsort( ms_vecWeaponInfoLookup, &WeaponInfoLookupCompare );
	}

	WeaponInfoLookup searchKey;
	searchKey.m_iszAttribClassName.SetFastNoCopy( iszAttribClassName );
	auto it = std::ranges::lower_bound( ms_vecWeaponInfoLookup, &searchKey, CWeaponInfoLookupListLess() );
	if ( it == ms_vecWeaponInfoLookup.end() || CWeaponInfoLookupListLess()( &searchKey, *it ) )
		return -1;
	return (int)( it - ms_vecWeaponInfoLookup.begin() );
}


#if defined( GAME_DLL )
static void weapon_reload_database( const CCommand &args );
static ConCommand weapon_reload_database_command( "weapon_reload_database", weapon_reload_database, "Reload the weapon database", FCVAR_CHEAT | FCVAR_GAMEDLL| FCVAR_SERVER_CAN_EXECUTE );
static void weapon_reload_database( const CCommand &args )
{
	g_WeaponDatabase.LoadManifest();
	IGameEvent *event = gameeventmanager->CreateEvent( "weapon_reload_database" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}
}
#endif


CWeaponDatabase::CWeaponDatabase() : m_bPreCached( false )
{
}

bool CWeaponDatabase::Init()
{
	ListenForGameEvent( "weapon_reload_database" );
	return LoadManifest();
}

bool CWeaponDatabase::LoadManifest()
{
	// FIXME[pmf]: this is defined as a virtual in g_pGameRules, but it isn't construction yet...
	const unsigned char szEncryptionKey[] = "d7NSuLq2";

	// KeyValues::AutoDelete manifest = ReadEncryptedKVFile( filesystem, "scripts/weapon_manifest.txt", szEncryptionKey );
	KeyValues::AutoDelete manifest( "weaponscripts" );
	manifest->UsesEscapeSequences( true );
	if ( manifest->LoadFromFile( filesystem, "scripts/weapon_manifest.txt", "GAME" ) )
	{
		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != nullptr ; sub = sub->GetNextKey() )
		{
			if ( !V_stricmp( sub->GetName(), "file" ) )
			{
				char fileBase[512];
				V_FileBase( sub->GetString(), fileBase, sizeof(fileBase) );

				LoadWeaponDataFromFile( filesystem, fileBase, szEncryptionKey );
			}
			else
			{
				Error( "Expecting 'file', got %s\n", sub->GetName() );
			}
		}
	}

	LoadEquipmentData();

	return true;
}

void CWeaponDatabase::PrecacheAllWeapons()
{
// NOTE[pmf]: removed this precache check for now, since it seems that the caches are getting evicted on map change
// 	if ( m_bPreCached )
// 		return;

	for ( const auto &[name, handle] : m_WeaponInfoDatabase )
	{
		const char* szClassName = name.c_str();

#if defined( CLIENT_DLL )
		// don't try to precache items that don't have entities (e.g. the items don't exist as entities on the client)
		if ( GetClassMap().GetClassSize(szClassName) <= 0 )
// 		if ( V_strncasecmp(szClassName, "item_", 5) == 0)
			continue;
#endif

		UTIL_PrecacheOther( szClassName );
	}
	m_bPreCached = true;
}


void CWeaponDatabase::RefreshAllWeapons()
{
	for ( const auto &[name, handle] : m_WeaponInfoDatabase )
	{
		FileWeaponInfo_t* pWeaponInfo = m_WeaponInfos[handle].get();
		if ( pWeaponInfo )
		{
			pWeaponInfo->RefreshDynamicParameters();
		}
	}
}

WEAPON_FILE_INFO_HANDLE CWeaponDatabase::FindWeaponInfo( const char *name )
{
	auto it = m_WeaponInfoDatabase.find( name );
	return it != m_WeaponInfoDatabase.end() ? it->second : GetInvalidWeaponInfoHandle();
}


WEAPON_FILE_INFO_HANDLE CWeaponDatabase::FindOrCreateWeaponInfo( const char *name )
{
	// Complain about duplicately defined metaclass names...
	auto it = m_WeaponInfoDatabase.find( name );
	if ( it != m_WeaponInfoDatabase.end() )
	{
		return it->second;
	}

	WEAPON_FILE_INFO_HANDLE lookup = (WEAPON_FILE_INFO_HANDLE)m_WeaponInfos.size();
	contract_assert( lookup != GetInvalidWeaponInfoHandle() );
	m_WeaponInfos.emplace_back( CreateWeaponInfo() );
	m_WeaponInfoDatabase.emplace( name, lookup );
	return lookup;
}

static FileWeaponInfo_t * Helper_GetNullWeaponInfo()
{
	static FileWeaponInfo_t gNullWeaponInfo;
	return &gNullWeaponInfo;
}

FileWeaponInfo_t * CWeaponDatabase::GetFileWeaponInfoFromHandle( WEAPON_FILE_INFO_HANDLE handle )
{
	if ( handle >= m_WeaponInfos.size() )
	{
		return Helper_GetNullWeaponInfo();
	}

	return m_WeaponInfos[ handle ].get();
}



//-----------------------------------------------------------------------------
// Purpose: Read data on weapon from script file
// Output:  true  - if data2 successfully read
//			false - if data load fails
//-----------------------------------------------------------------------------
bool CWeaponDatabase::LoadWeaponDataFromFile( IFileSystem* filesystem, const char *szWeaponName, const unsigned char *pICEKey )
{
	char sz[128];
	V_snprintf( sz, sizeof( sz ), "scripts/%s", szWeaponName );
	KeyValues *pKV = ReadEncryptedKVFile( filesystem, sz, pICEKey );
	if ( !pKV )
		return false;

	WEAPON_FILE_INFO_HANDLE handle = FindOrCreateWeaponInfo( szWeaponName );
	FileWeaponInfo_t *pFileInfo = GetFileWeaponInfoFromHandle( handle );
	contract_assert( pFileInfo );

	pFileInfo->Parse( pKV, szWeaponName );

	pKV->deleteThis();

	return true;
}

void CWeaponDatabase::Reset()
{
	m_WeaponInfoDatabase.clear();
	m_WeaponInfos.clear();

#ifdef _DEBUG
	memset(g_bUsedWeaponSlots, 0, sizeof(g_bUsedWeaponSlots));
#endif
	m_bPreCached = false;
}

void CWeaponDatabase::FireGameEvent( IGameEvent *pEvent )
{
#if defined( CLIENT_DLL )
	const char *pEventName = pEvent->GetName();
	if ( 0 == V_strcmp( pEventName, "weapon_reload_database" ) )
	{
		LoadManifest();
	}
#endif
}
