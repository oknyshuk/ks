// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "host.h"
#include "sysexternal.h"
#include "networkstringtable.h"
#include "netmessages.h"
#include "net.h"
#include "filesystem_engine.h"
#include "baseclient.h"
import std;
#include "checksum_engine.h"
#include "MapReslistGenerator.h"
#include "../utils/common/bsplib.h"
#include "ibsppack.h"
#include "server.h"
#include "eiface.h"
#include "cdll_engine_int.h"
#include "facade/inetworkstringtable.inl"
#include "facade/inetworkstringtablecontainer.inl"

ConVar sv_dumpstringtables( "sv_dumpstringtables", "0", FCVAR_CHEAT );

inline constexpr const char *BSPPACK_STRINGTABLE_DICTIONARY = "stringtable_dictionary.dct";
inline constexpr const char *BSPPACK_STRINGTABLE_DICTIONARY_FALLBACK = "stringtable_dictionary_fallback.dct";
// These are automatically added by vbsp.  Should be removed when adding the real files here
// These must match what is used in utils\vbsp\vbsp.cpp!!!
inline constexpr const char *BSPPACK_STRINGTABLE_DICTIONARY_GAMECONSOLE = "stringtable_dictionary_xbox.dct";
inline constexpr const char *BSPPACK_STRINGTABLE_DICTIONARY_GAMECONSOLE_FALLBACK = "stringtable_dictionary_fallback_xbox.dct";

inline constexpr const char *RESLISTS_FOLDER = "reslists";
inline constexpr const char *RESLISTS_FOLDER_X360 = "reslists_xbox";

constexpr auto SUBSTRING_BITS = 5;
struct StringHistoryEntry
{
	char string[ (1<<SUBSTRING_BITS) ];
};

static int CountSimilarCharacters( char const *str1, char const *str2 )
{
	int c = 0;
	while ( *str1 && *str2 &&
		*str1 == *str2 && c < ((1<<SUBSTRING_BITS) -1 ))
	{
		str1++;
		str2++;
		c++;
	}

	return c;
}

static int GetBestPreviousString( std::vector< StringHistoryEntry >& history, char const *newstring, int& substringsize )
{ 
	int bestindex = -1;
	int bestcount = 0;
	int c = std::ssize( history );
	for ( int i = 0; i < c; i++ )
	{
		char const *prev = history[ i ].string;
		int similar = CountSimilarCharacters( prev, newstring );
		
		if ( similar < 3 )
			continue;

		if ( similar > bestcount )
		{
			bestcount = similar;
			bestindex = i;
		}
	}

	substringsize = bestcount;
	return bestindex;
}


static ConVar stringtable_usedictionaries( "stringtable_usedictionaries", 
										   "1",	// On CS:GO we disable stringtable dictionaries for community servers for maps to be downloadable in code (see: CNetworkStringTable::WriteUpdate)
										   0, "Use dictionaries for string table networking\n" );
static ConVar stringtable_alwaysrebuilddictionaries( "stringtable_alwaysrebuilddictionaries", "0", 0, "Rebuild dictionary file on every level load\n" );
static ConVar stringtable_showsizes( "stringtable_showsizes", "0", 0, "Show sizes of string tables when building for signon\n" );



class CNetworkStringTableDictionaryManager: public INetworkStringTableDictionaryMananger
{
public:

	CNetworkStringTableDictionaryManager();

	// INetworkStringTableDictionaryMananger
	bool OnLevelLoadStart( char const *pchMapName, CRC32_t *pStringTableCRC );
	void OnBSPFullyUnloaded();
	CRC32_t GetCRC() { return m_CRC; }

	// Returns -1 if string can't be found in db
	int	 Find( char const *pchString ) const;
	char const *Lookup( int index ) const;

	int		GetEncodeBits() const;
	bool	ShouldRecreateDictionary( char const *pchMapName );
	bool	IsValid() const;
	void	ProcessBuffer( ks::byte_reader &buf );
	void	SetLoadedFallbacks( bool bLoadedFromFallbacks );

	bool	WriteDictionaryToBSP( char const *pchMapName, std::span< const std::byte > buf, bool bCreatingFor360 );
	void	CacheNewStringTableForWriteToBSPOnLevelShutdown( char const *pchMapName, std::span< const std::byte > buf, bool bCreatingFor360 );

public:

	void LoadMapStrings( char const *pchMapName, bool bServer );
	void Clear();
	std::optional< std::vector< std::byte > > LoadDictionaryFile( char const *pchMapName );

	CRC32_t HashStringCaselessIgnoreSlashes( char const *pString ) const
	{
		if ( !pString )
		{
			pString = "";
		}

		int len = V_strlen( pString ) + 1;
		ks::ScratchArena<> scratch;
		char *name = (char *)scratch.AllocBytes( len );
		V_strncpy( name, pString, len );
		V_FixSlashes( name );
		V_strlower( name );

		return CRC32_ProcessSingleBuffer( (const void *)name, len );
	}

	std::string							m_sCurrentMap;

	// NOTE NOTE:  These need to be 'persistent' objects since our code stores off raw ptrs to these strings when 
	//  precaching, etc.  Can't be constructed on stack buffer or with va( "%s", xxx ), etc.!!!!!
	// Otherwise we could use FileNameHandle_t type stuff.

	// Raw strings in order from the data file
	std::vector< std::string >			m_Strings;
	// Mapping of string back to index in CUtlVector
	std::map< CRC32_t, int >				m_StringHashToIndex;
	CRC32_t								m_CRC;
	int									m_nEncodeBits;
	bool								m_bForceRebuildDictionaries;
	bool								m_bLoadedFallbacks;

	struct CStringTableDictionaryCache
	{
		CStringTableDictionaryCache() 
		{ 
			Reset();
		}

		void Reset()
		{
			m_bActive = false;
			m_sBSPName = "";
			m_bCreatingForX360 = false;
			m_Buffer.clear();
		}

		bool		m_bActive;
		std::string	m_sBSPName;
		std::vector< std::byte >	m_Buffer;
		bool		m_bCreatingForX360;
	};

	CStringTableDictionaryCache		m_BuildStringTableDictionaryCache;
};

#include "facade/inetworkstringdictmgr.inl"


static CNetworkStringTableDictionaryManager g_StringTableDictionary;
// Expose to rest of engine
INetworkStringTableDictionaryMananger *g_pStringTableDictionary = &g_StringTableDictionary;

CNetworkStringTableDictionaryManager::CNetworkStringTableDictionaryManager() : 
	m_nEncodeBits( 1 ),
	m_bForceRebuildDictionaries( false ),
	m_bLoadedFallbacks( false ),
	m_CRC( 0 )
{
}

void CNetworkStringTableDictionaryManager::Clear()
{
	m_StringHashToIndex.clear();
	m_Strings.clear(); m_Strings.shrink_to_fit();
	m_CRC = 0;
	m_nEncodeBits = 1;
}

bool CNetworkStringTableDictionaryManager::IsValid() const
{
	return !m_Strings.empty();
}

int CNetworkStringTableDictionaryManager::GetEncodeBits() const
{
	return m_nEncodeBits;
}

void CNetworkStringTableDictionaryManager::SetLoadedFallbacks( bool bLoadedFromFallbacks )
{
	m_bLoadedFallbacks = bLoadedFromFallbacks;
}

bool CNetworkStringTableDictionaryManager::OnLevelLoadStart( char const *pchMapName, CRC32_t *pStringTableCRC )
{
	m_BuildStringTableDictionaryCache.Reset();

	// INFESTED_DLL	 - disable string dictionaries for Alien Swarm random maps.  TODO: Move a check for this into the game interface
	static char gamedir[MAX_OSPATH];
	V_FileBase( com_gamedir, gamedir, sizeof( gamedir ) );
	if ( !V_stricmp( gamedir, "infested" ) )
	{
		return true;
	}

	m_bForceRebuildDictionaries = stringtable_alwaysrebuilddictionaries.GetBool() || CommandLine()->FindParm( "-stringtables" );

	if ( pchMapName )
		LoadMapStrings( pchMapName, pStringTableCRC == nullptr );
	else
		return true;	// assume that stringtables will match since we will download the map later

	if ( pStringTableCRC )
		return ( *pStringTableCRC == m_CRC );
	return true;
}

bool CNetworkStringTableDictionaryManager::ShouldRecreateDictionary( char const *pchMapName )
{
	// INFESTED_DLL	 - disable string dictionaries for Alien Swarm random maps.  TODO: Move a check for this into the game interface
	static char gamedir[MAX_OSPATH];
	V_FileBase( com_gamedir, gamedir, sizeof( gamedir ) );
	if ( !V_stricmp( gamedir, "infested" ) )
	{
		return false;
	}

	if ( m_bForceRebuildDictionaries ||	// being forced with -stringtables or stringtable_alwaysrebuilddictionaries
			(
			 MapReslistGenerator().IsEnabled() &&	// true if -makereslists
			 ( m_Strings.empty() ||			// True if we couldn't load the file up before...
			   m_bLoadedFallbacks )					// True if we loaded fallback file instead
			)
		)					
	{
		return m_Strings.empty() || // True if we couldn't load the file up before...
				m_bLoadedFallbacks || // True if we loaded fallback file instead
				MapReslistGenerator().IsEnabled(); // true if -makereslists
	}

	return false;
}

inline char const *GetStringTableDictionaryFileName( bool bIsFallback )
{
	if ( bIsFallback )
	{
		return BSPPACK_STRINGTABLE_DICTIONARY_FALLBACK;
	}

	return BSPPACK_STRINGTABLE_DICTIONARY;
}

std::optional< std::vector< std::byte > > CNetworkStringTableDictionaryManager::LoadDictionaryFile( char const *pchMapName )
{
	m_bLoadedFallbacks = false;

	if ( auto buf = g_pFileSystem->ReadFile( GetStringTableDictionaryFileName( false ), "BSP" ) )
	{
		return buf;
	}

	// Try backup file
	if ( auto buf = g_pFileSystem->ReadFile( GetStringTableDictionaryFileName( true ), "BSP" ) )
	{
		Warning( "#######################################\n" );
		Warning( "Map %s using default stringtable dictionary, don't ship this way!!!\n", pchMapName );
		Warning( "Run with -stringtables on the command line or convar\n" );
		Warning( "stringtable_alwaysrebuilddictionaries enabled to build the string table\n" );
		Warning( "#######################################\n" );
		m_bLoadedFallbacks = true;
		return buf;
	}

	// Try fallback file
	char szFallback[ 256 ];
	V_snprintf( szFallback, sizeof( szFallback ), "reslists/%s.dict", pchMapName );
	if ( auto buf = g_pFileSystem->ReadFile( szFallback, "GAME" ) )
	{
		Warning( "#######################################\n" );
		Warning( "Map %s using fallback stringtable dictionary, don't ship this way!!!\n", pchMapName );
		Warning( "Run with -stringtables on the command line or convar\n" );
		Warning( "stringtable_alwaysrebuilddictionaries enabled to build the string table\n" );
		Warning( "#######################################\n" );
		m_bLoadedFallbacks = true;
		return buf;
	}

	Warning( "#######################################\n" );
	Warning( "Map %s missing stringtable dictionary, don't ship this way!!!\n", pchMapName );
	Warning( "Run with -stringtables on the command line or convar\n" );
	Warning( "stringtable_alwaysrebuilddictionaries enabled to build the string table\n" );
	Warning( "#######################################\n" );
	return std::nullopt;
}

void CNetworkStringTableDictionaryManager::ProcessBuffer( ks::byte_reader &buf )
{
	Clear();

	m_CRC =	CRC32_ProcessSingleBuffer( buf.bytes().data(), buf.size() );
	while ( buf.remaining() > 0 )
	{
		char line[ MAX_PATH ];
		buf.read_cstring( line, sizeof( line ) );

		if ( !line[ 0 ] )
			continue;

		std::string str;
		str = line;
		int index = std::ssize( m_Strings ); m_Strings.push_back( str );

		CRC32_t hash = HashStringCaselessIgnoreSlashes( str.c_str() );
		m_StringHashToIndex.emplace( hash, index );	// a repeated hash keeps its first string
	}

	m_nEncodeBits = Q_log2( std::ssize( m_Strings ) ) + 1;
}

void CNetworkStringTableDictionaryManager::LoadMapStrings( char const *pchMapName, bool bServer )
{
	if ( m_sCurrentMap == pchMapName )
		return;

	m_sCurrentMap = ks::nz( pchMapName );
	Clear();

	// On the client we need to add the bsp to the file search path before loading the dictionary file...
	// It's added on the server in CGameServer::SpawnServer
	char szModelName[MAX_PATH];
	char szNameOnDisk[MAX_PATH];
	V_snprintf( szModelName, sizeof( szModelName ), "maps/%s.bsp", pchMapName );
	GetMapPathNameOnDisk( szNameOnDisk, szModelName, sizeof( szNameOnDisk ) );

	if ( !bServer )
	{
		// Add to file system
		g_pFileSystem->AddSearchPath( szNameOnDisk, "GAME", PATH_ADD_TO_HEAD );

		// Force reload all materials since BSP has changed
		// TODO: modelloader->UnloadUnreferencedModels();
		materials->ReloadMaterials();
	}

	if ( stringtable_usedictionaries.GetBool() )
	{
		// Load the data file
		if ( auto buf = LoadDictionaryFile( pchMapName ) )
		{
			// LZMA decompress it if needed
			CLZMA lzma;
			if ( lzma.IsCompressed( (byte *)buf->data() ) )
			{
				unsigned int decompressedSize = lzma.GetActualSize( (byte *)buf->data() );
				std::vector< std::byte > work( decompressedSize );
				int outputLength = lzma.Uncompress( (byte *)buf->data(), (byte *)work.data() );
				work.resize( outputLength );
				*buf = std::move( work );
			}

			ks::byte_reader reader( *buf );
			ProcessBuffer( reader );
		}
	}
}

int CNetworkStringTableDictionaryManager::Find( char const *pchString ) const
{
	CRC32_t hash = HashStringCaselessIgnoreSlashes( pchString );
	// Note that a string can contain subparts that together both exist as a valid filename, but they may be from separate paths
	auto idx = m_StringHashToIndex.find( hash );
	if ( idx == m_StringHashToIndex.end() )
		return -1;
	return idx->second;
}

char const *CNetworkStringTableDictionaryManager::Lookup( int index ) const
{
	if ( index < 0 || index >= std::ssize( m_Strings ) )
	{
		contract_assert( 0 );
		return "";
	}
	const std::string &string = m_Strings[ index ];
	return string.c_str();
}

void CNetworkStringTableDictionaryManager::OnBSPFullyUnloaded()
{
	if ( !m_BuildStringTableDictionaryCache.m_bActive )
		return;
	WriteDictionaryToBSP( m_BuildStringTableDictionaryCache.m_sBSPName.c_str(), m_BuildStringTableDictionaryCache.m_Buffer, m_BuildStringTableDictionaryCache.m_bCreatingForX360 );
	m_BuildStringTableDictionaryCache.Reset();
}

bool CNetworkStringTableDictionaryManager::WriteDictionaryToBSP( char const *pchMapName, std::span< const std::byte > buf, bool bCreatingFor360 )
{
	char mapPath[ MAX_PATH ];
	V_snprintf( mapPath, sizeof( mapPath ), "maps/%s.bsp", pchMapName );

	// We shouldn't ever fail here since we don't queue this up if it might fail
	// Make sure that the file is writable before building stringtable dictionary.
	if ( !g_pFileSystem->IsFileWritable( mapPath, "GAME" ) )
	{
		return false;
	}

	// load the bsppack dll
	IBSPPack *iBSPPack = nullptr;
	CSysModule *pModule = FileSystem_LoadModule( "bsppack" );
	if ( pModule )
	{
		CreateInterfaceFn factory = Sys_GetFactory( pModule );
		if ( factory )
		{
			iBSPPack = ( IBSPPack * )factory( IBSPPACK_VERSION_STRING, nullptr );
		}
	}

	if( !iBSPPack )
	{
		if ( pModule )
		{
			Sys_UnloadModule( pModule );
		}
		ConMsg( "Can't load bsppack.dll\n" );
		return false;
	}

	iBSPPack->LoadBSPFile( g_pFileSystem, mapPath );

	char const *relative = bCreatingFor360 ? BSPPACK_STRINGTABLE_DICTIONARY_GAMECONSOLE : BSPPACK_STRINGTABLE_DICTIONARY;

	// Remove the fallback that VBSP adds by default
	iBSPPack->RemoveFileFromPack( bCreatingFor360 ? BSPPACK_STRINGTABLE_DICTIONARY_GAMECONSOLE_FALLBACK : BSPPACK_STRINGTABLE_DICTIONARY_FALLBACK );
	// Now add in the up to date information
	iBSPPack->AddBufferToPack( relative, (void *)buf.data(), int( buf.size() ), false );

	iBSPPack->WriteBSPFile( mapPath );
	iBSPPack->ClearPackFile();
	FileSystem_UnloadModule( pModule );

	Msg( "Updated stringtable dictionary saved to %s\n", mapPath );

	return true;
}

void CNetworkStringTableDictionaryManager::CacheNewStringTableForWriteToBSPOnLevelShutdown( char const *pchMapName, std::span< const std::byte > buf, bool bCreatingFor360 )
{
	m_BuildStringTableDictionaryCache.m_bActive = true;
	m_BuildStringTableDictionaryCache.m_sBSPName = ks::nz( pchMapName );
	m_BuildStringTableDictionaryCache.m_Buffer.assign( buf.begin(), buf.end() );
	m_BuildStringTableDictionaryCache.m_bCreatingForX360 = bCreatingFor360;
}

//-----------------------------------------------------------------------------
// Implementation for general purpose strings
//-----------------------------------------------------------------------------
class CNetworkStringDict : public INetworkStringDict
{
public:
	CNetworkStringDict( bool bUseDictionary ) : 
		m_bUseDictionary( bUseDictionary )
	{
	}

	~CNetworkStringDict() 
	{ 
		Purge();
	}

	unsigned int Count()
	{
		return (unsigned int)m_Items.size();
	}

	void Purge()
	{
		m_Items.clear();
		m_ItemsByHash.clear();
	}

	const char *String( int index )
	{
		return m_Items[index].key.GetName();
	}

	bool IsValidIndex( int index )
	{
		return index >= 0 && index < (int)m_Items.size();
	}

	int Insert( const char *pString )
	{
		int index = (int)m_Items.size();
		CTableItem &item = m_Items.emplace_back().key;
		item.SetName( m_bUseDictionary, pString );
		m_ItemsByHash.emplace( item.GetHash(), index );
		return index;
	}

	int Find( const char *pString )
	{
		CTableItem search;
		search.SetName( false, pString );
		auto it = m_ItemsByHash.find( search.GetHash() );
		return it == m_ItemsByHash.end() ? -1 : it->second;
	}

	CNetworkStringTableItem	&Element( int index )
	{
		return m_Items[index].value;
	}

	const CNetworkStringTableItem &Element( int index ) const
	{
		return m_Items[index].value;
	}

	void UpdateDictionary( int index )
	{
		if ( !m_bUseDictionary )
			return;

		m_Items[index].key.Update();	// refreshes the dictionary slot; the hash (name only) is unchanged
	}

	int DictionaryIndex( int index )
	{
		if ( !m_bUseDictionary )
			return -1;

		return m_Items[index].key.GetDictionaryIndex();
	}

public:
	bool	m_bUseDictionary;

	// We use this type of item to avoid having two copies of the strings in memory --
	//  either we have a dictionary slot and point to that, or we have a m_Name std::string that gets
	//  wiped between levels
	class CTableItem
	{
	public:

		CTableItem() : m_DictionaryIndex( -1 ), m_StringHash( 0u )
		{
		}

		char const *GetName() const
		{
			return m_Name.c_str();
		}

		void Update()
		{
			m_DictionaryIndex = g_StringTableDictionary.Find( m_Name.c_str() );
			ComputeHash();
		}

		int GetDictionaryIndex() const
		{
			return m_DictionaryIndex;
		}

		void SetName( bool bUseDictionary, char const *pString )
		{
			m_Name = ks::nz( pString );
			m_DictionaryIndex = bUseDictionary ? g_StringTableDictionary.Find( pString ) : -1;
			ComputeHash();
		}

		CRC32_t GetHash() const
		{
			return m_StringHash;
		}
		
	public:

		void					ComputeHash()
		{
			char const *pName = GetName();

			int len = V_strlen( pName ) + 1;
			ks::ScratchArena<> scratch;
			char *name = (char *)scratch.AllocBytes( len );
			V_strncpy( name, pName, len );
			V_FixSlashes( name );
			V_strlower( name );

			m_StringHash = CRC32_ProcessSingleBuffer( (const void *)name, len );
		}

		int						m_DictionaryIndex;
		std::string				m_Name;
		CRC32_t					m_StringHash;
	};
	struct Entry
	{
		CTableItem key;
		CNetworkStringTableItem value;
	};
	std::deque< Entry > m_Items;					// index = network string number; deque: items never move
	std::multimap< CRC32_t, int > m_ItemsByHash;
};

#include "facade/inetworkstringdict.inl"


void CNetworkStringTable::CheckDictionary( int stringNumber )
{
	m_pItems->UpdateDictionary( stringNumber );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : id - 
//			*tableName - 
//			maxentries - 
//-----------------------------------------------------------------------------
CNetworkStringTable::CNetworkStringTable( TABLEID id, const char *tableName, int maxentries, int userdatafixedsize, int userdatanetworkbits, int flags ) :
	m_bAllowClientSideAddString( false ),
	m_pItemsClientSide( nullptr ),
	m_nFlags( flags )
{
	if ( maxentries < 0 || userdatafixedsize < 0 || userdatanetworkbits < 0 )
	{
		Host_Error( "String tables negative constructor unsupported %i %i %i\n",
			maxentries, userdatafixedsize, userdatanetworkbits );
	}

	m_id = id;
	int len = std::strlen( tableName ) + 1;
	m_pszTableName = new char[ len ];
	contract_assert( m_pszTableName );
	contract_assert( tableName );
	V_strncpy( m_pszTableName, tableName, len );

	m_changeFunc = nullptr;
	m_pObject = nullptr;
	m_nTickCount = 0;
	for ( int i = 0; i < MIRROR_TABLE_MAX_COUNT; ++i )
		m_pMirrorTable[ i ] = nullptr;
	m_nLastChangedTick = 0;
	m_bChangeHistoryEnabled = false;
	m_bLocked = false;

	m_nMaxEntries = maxentries;
	m_nEntryBits = Q_log2( m_nMaxEntries );

	m_bUserDataFixedSize = userdatafixedsize != 0;
	m_nUserDataSize = userdatafixedsize;
	m_nUserDataSizeBits = userdatanetworkbits;

	if ( m_nUserDataSizeBits > CNetworkStringTableItem::MAX_USERDATA_BITS )
	{
		Host_Error( "String tables user data bits restricted to %i bits, requested %i is too large\n", 
			CNetworkStringTableItem::MAX_USERDATA_BITS,
			m_nUserDataSizeBits );
	}

	if ( m_nUserDataSize > CNetworkStringTableItem::MAX_USERDATA_SIZE )
	{
		Host_Error( "String tables user data size restricted to %i bytes, requested %i is too large\n", 
			CNetworkStringTableItem::MAX_USERDATA_SIZE,
			m_nUserDataSize );
	}

	// Make sure maxentries is power of 2
	if ( ( 1 << m_nEntryBits ) != maxentries )
	{
		Host_Error( "String tables must be powers of two in size!, %i is not a power of 2 [%s]\n", maxentries, tableName );
	}

	m_pItems = new CNetworkStringDict( m_nFlags & NSF_DICTIONARY_ENABLED );
}

void CNetworkStringTable::SetAllowClientSideAddString( bool state )
{
	if ( state == m_bAllowClientSideAddString )
		return;

	m_bAllowClientSideAddString = state;
	if ( m_pItemsClientSide )
	{
		delete m_pItemsClientSide; 
		m_pItemsClientSide = nullptr;
	}

	if ( m_bAllowClientSideAddString )
	{
		m_pItemsClientSide = new CNetworkStringDict( NSF_NONE );
		m_pItemsClientSide->Insert( "___clientsideitemsplaceholder0___" ); // 0 slot can't be used
		m_pItemsClientSide->Insert( "___clientsideitemsplaceholder1___" ); // -1 can't be used since it looks like the "invalid" index from other string lookups
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNetworkStringTable::IsUserDataFixedSize() const
{
	return m_bUserDataFixedSize;
}

bool CNetworkStringTable::IsUsingDictionary() const
{
	return m_nFlags & NSF_DICTIONARY_ENABLED;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CNetworkStringTable::GetUserDataSize() const
{
	return m_nUserDataSize;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CNetworkStringTable::GetUserDataSizeBits() const
{
	return m_nUserDataSizeBits;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNetworkStringTable::~CNetworkStringTable( void )
{
	delete[] m_pszTableName;
	delete m_pItems;
	delete m_pItemsClientSide;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNetworkStringTable::DeleteAllStrings( void )
{
	delete m_pItems;
	m_pItems = new CNetworkStringDict( m_nFlags & NSF_DICTIONARY_ENABLED );

	if ( m_pItemsClientSide )
	{
		delete m_pItemsClientSide;
		m_pItemsClientSide = new CNetworkStringDict( NSF_NONE );
		m_pItemsClientSide->Insert( "___clientsideitemsplaceholder0___" ); // 0 slot can't be used
		m_pItemsClientSide->Insert( "___clientsideitemsplaceholder1___" ); // -1 can't be used since it looks like the "invalid" index from other string lookups
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : i - 
// Output : CNetworkStringTableItem
//-----------------------------------------------------------------------------
CNetworkStringTableItem *CNetworkStringTable::GetItem( int i )
{
	if ( i >= 0 )
	{
		return &m_pItems->Element( i );		
	}

	contract_assert( m_pItemsClientSide );
	return &m_pItemsClientSide->Element( -i );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the table identifier
// Output : TABLEID
//-----------------------------------------------------------------------------
TABLEID CNetworkStringTable::GetTableId( void ) const
{
	return m_id;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the max size of the table
// Output : int
//-----------------------------------------------------------------------------
int CNetworkStringTable::GetMaxStrings( void ) const
{
	return m_nMaxEntries;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a table, by name
// Output : const char
//-----------------------------------------------------------------------------
const char *CNetworkStringTable::GetTableName( void ) const
{
	return m_pszTableName;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the number of bits needed to encode an entry index
// Output : int
//-----------------------------------------------------------------------------
int CNetworkStringTable::GetEntryBits( void ) const
{
	return m_nEntryBits;
}


void CNetworkStringTable::SetTick(int tick_count)
{
	contract_assert( tick_count >= m_nTickCount );
	m_nTickCount = tick_count;
}

bool CNetworkStringTable::Lock(	bool bLock )
{
	bool bState = m_bLocked;
	m_bLocked = bLock;
	return bState;
}

pfnStringChanged CNetworkStringTable::GetCallback()
{ 
	return m_changeFunc; 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNetworkStringTable::EnableRollback()
{
	// stringtable must be empty 
	contract_assert( m_pItems->Count() == 0 );
	m_bChangeHistoryEnabled = true;
}

void CNetworkStringTable::SetMirrorTable(uint nIndex, INetworkStringTable *table)
{
	contract_assert( nIndex < MIRROR_TABLE_MAX_COUNT );
	m_pMirrorTable[ nIndex ] = table;
}

void CNetworkStringTable::RestoreTick(int tick)
{
	// TODO optimize this, most of the time the tables doens't really change

	m_nLastChangedTick = 0;

	int count = m_pItems->Count();
		
	for ( int i = 0; i < count; i++ )
	{
		// restore tick in all entries
		int tickChanged = m_pItems->Element( i ).RestoreTick( tick );

		if ( tickChanged > m_nLastChangedTick )
			m_nLastChangedTick = tickChanged;
	}
}

//-----------------------------------------------------------------------------
// Purpose: updates the mirror table, if set one
// Output : return true if some entries were updates
//-----------------------------------------------------------------------------
void CNetworkStringTable::UpdateMirrorTable( int tick_ack  )
{
	for ( int nMirrorTableIndex = 0; nMirrorTableIndex < MIRROR_TABLE_MAX_COUNT; ++nMirrorTableIndex )
	{
		if ( !m_pMirrorTable[ nMirrorTableIndex ] )
			continue;

		m_pMirrorTable[ nMirrorTableIndex ]->SetTick( m_nTickCount ); // use same tick

		int count = m_pItems->Count();

		for ( int i = 0; i < count; i++ )
		{
			CNetworkStringTableItem *p = &m_pItems->Element( i );

			// mirror is up to date
			if ( p->GetTickChanged() <= tick_ack )
				continue;

			const void *pUserData = p->GetUserData();

			int nBytes = p->GetUserDataLength();

			if ( !nBytes || !pUserData )
			{
				nBytes = 0;
				pUserData = nullptr;
			}

			// Check if we are updating an old entry or adding a new one
			if ( i < m_pMirrorTable[ nMirrorTableIndex ]->GetNumStrings() )
			{
				m_pMirrorTable[ nMirrorTableIndex ]->SetStringUserData( i, nBytes, pUserData );
			}
			else
			{
				// Grow the table (entryindex must be the next empty slot)
				contract_assert( i == m_pMirrorTable[ nMirrorTableIndex ]->GetNumStrings() );
				char const *pName = m_pItems->String( i );
				m_pMirrorTable[ nMirrorTableIndex ]->AddString( true, pName, nBytes, pUserData );
			}
		}
	}
}

int CNetworkStringTable::WriteUpdate( CBaseClient *client, bf_write &buf, int tick_ack ) const
{
	std::vector< StringHistoryEntry > history;

	int entriesUpdated = 0;
	int lastEntry = -1;
	int lastDictionaryIndex = -1;
	int nDictionaryEncodeBits = g_StringTableDictionary.GetEncodeBits();
	bool bUseDictionaries = IsUsingDictionary();
	bool bEncodeUsingDictionaries = bUseDictionaries && stringtable_usedictionaries.GetBool() && g_StringTableDictionary.IsValid();

	// INFESTED_DLL	 - disable string dictionaries for Alien Swarm random maps.  TODO: Move a check for this into the game interface
	static char gamedir[MAX_OSPATH];
	V_FileBase( com_gamedir, gamedir, sizeof( gamedir ) );
	if ( !V_stricmp( gamedir, "infested" ) )
	{
		bEncodeUsingDictionaries = false;
	}
	// CSGO_DLL - disable string dictionaries for CS:GO community servers to allow for map downloading,
	// keep it enabled on Valve official servers since clients always have all maps or on listen servers
	if ( bEncodeUsingDictionaries && !V_stricmp( gamedir, "csgo" ) )
	{
		bEncodeUsingDictionaries = false;
	}

	int count = m_pItems->Count();
	int nDictionaryCount = 0;

	buf.WriteOneBit( bEncodeUsingDictionaries ? 1 : 0 );

	for ( int i = 0; i < count; i++ )
	{
		CNetworkStringTableItem *p = &m_pItems->Element( i );

		// Client is up to date
		if ( p->GetTickChanged() <= tick_ack )
			continue;

		int nStartBit = buf.GetNumBitsWritten();

		// Write Entry index
		if ( (lastEntry+1) == i )
		{
			buf.WriteOneBit( 1 );
		}
		else
		{
			buf.WriteOneBit( 0 );
			buf.WriteUBitLong( i, m_nEntryBits );
		}

		// check if string can use older string as base eg "models/weapons/gun1" & "models/weapons/gun2"
		char const *pEntry = m_pItems->String( i );

		if ( p->GetTickCreated() >= tick_ack )
		{
			// this item has just been created, send string itself
			buf.WriteOneBit( 1 );
			
			int nCurrentDictionaryIndex = m_pItems->DictionaryIndex( i );

			if ( bEncodeUsingDictionaries &&
				nCurrentDictionaryIndex != -1 )
			{
				++nDictionaryCount;

				buf.WriteOneBit( 1 );
				if ( ( lastDictionaryIndex +1 ) == nCurrentDictionaryIndex )
				{
					buf.WriteOneBit( 1 );
				}
				else
				{
					buf.WriteOneBit( 0 );
					buf.WriteUBitLong( nCurrentDictionaryIndex, nDictionaryEncodeBits );
				}

				lastDictionaryIndex = nCurrentDictionaryIndex;
			}
			else
			{
				if ( bEncodeUsingDictionaries )
				{
					buf.WriteOneBit( 0 );
				}

				int substringsize = 0;
				int bestprevious = GetBestPreviousString( history, pEntry, substringsize );
				if ( bestprevious != -1 )
				{
					buf.WriteOneBit( 1 );
					buf.WriteUBitLong( bestprevious, 5 );	// history never has more than 32 entries
					buf.WriteUBitLong( substringsize, SUBSTRING_BITS );
					buf.WriteString( pEntry + substringsize );
				}
				else
				{
					buf.WriteOneBit( 0 );
					buf.WriteString( pEntry  );
				}
			}
		}
		else
		{
			buf.WriteOneBit( 0 );
		}

		// Write the item's user data.
		int len;
		const void *pUserData = GetStringUserData( i, &len );
		if ( pUserData && len > 0 )
		{
			buf.WriteOneBit( 1 );

			if ( IsUserDataFixedSize() )
			{
				// Don't have to send length, it was sent as part of the table definition
				buf.WriteBits( pUserData, GetUserDataSizeBits() );
			}
			else
			{
				buf.WriteUBitLong( len, CNetworkStringTableItem::MAX_USERDATA_BITS );
				buf.WriteBits( pUserData, len*8 );
			}
		}
		else
		{
			buf.WriteOneBit( 0 );
		}

		// limit string history to 32 entries
		if ( std::ssize( history ) > 31 )
		{
			history.erase( history.begin() + 0 );
		}

		// add string to string history
		StringHistoryEntry she;
		V_strncpy( she.string, pEntry, sizeof( she.string ) );
		history.push_back( she );

		entriesUpdated++;
		lastEntry = i;

		if ( client && client->IsTracing() )
		{
			int nBits = buf.GetNumBitsWritten() - nStartBit;
			client->TraceNetworkMsg( nBits, " [%s] %d:%s ", GetTableName(), i, GetString( i ) );
		}
	}

	// If writing the baseline, and using dictionaries, and less than 90% of strings are in the dictionaries, 
	//  then we need to consider rebuild the dictionaries
	if ( tick_ack == -1 && 
		count > 20 &&
		bEncodeUsingDictionaries && 
		nDictionaryCount < 0.9f * count )
	{
	}

	return entriesUpdated;
}


//-----------------------------------------------------------------------------
// Purpose: Parse string update
//-----------------------------------------------------------------------------
void CNetworkStringTable::ParseUpdate( bf_read &buf, int entries )
{
	int lastEntry = -1;
	int lastDictionaryIndex = -1;
	int nDictionaryEncodeBits = g_StringTableDictionary.GetEncodeBits();
	// bool bUseDictionaries = IsUsingDictionary();

	bool bEncodeUsingDictionaries = buf.ReadOneBit() ? true : false;

	std::vector< StringHistoryEntry > history;

	for (int i=0; i<entries; i++)
	{
		int entryIndex = lastEntry + 1;

		if ( !buf.ReadOneBit() )
		{
			entryIndex = buf.ReadUBitLong( GetEntryBits() );
		}

		lastEntry = entryIndex;
		
		if ( entryIndex < 0 || entryIndex >= GetMaxStrings() )
		{
			Host_Error( "Server sent bogus string index %i for table %s\n", entryIndex, GetTableName() );
		}

		const char *pEntry = nullptr;
		char entry[ 1024 ]; 
		char substr[ 1024 ];

		if ( buf.ReadOneBit() )
		{
			// It's using dictionary
			if ( bEncodeUsingDictionaries && buf.ReadOneBit() )
			{
				// It's sequential, no need to encode full dictionary index
				if ( buf.ReadOneBit() )
				{
					lastDictionaryIndex++;
				}
				else
				{
					lastDictionaryIndex = buf.ReadUBitLong( nDictionaryEncodeBits );
				}

				char const *lookup = g_StringTableDictionary.Lookup( lastDictionaryIndex );
				V_strncpy( entry, lookup, sizeof( entry ) );
			}
			else
			{
				bool substringcheck = buf.ReadOneBit() ? true : false;

				if ( substringcheck )
				{
					int index = buf.ReadUBitLong( 5 );
					int bytestocopy = buf.ReadUBitLong( SUBSTRING_BITS );
					V_strncpy( entry, history[ index ].string, bytestocopy + 1 );
					buf.ReadString( substr, sizeof(substr) );
					V_strncat( entry, substr, sizeof(entry), COPY_ALL_CHARACTERS );
				}
				else
				{
					buf.ReadString( entry, sizeof( entry ) );
				}
			}

			pEntry = entry;
		}
		
		// Read in the user data.
		unsigned char tempbuf[ CNetworkStringTableItem::MAX_USERDATA_SIZE ];
		std::memset( tempbuf, 0, sizeof( tempbuf ) );
		const void *pUserData = nullptr;
		int nBytes = 0;

		if ( buf.ReadOneBit() )
		{
			if ( IsUserDataFixedSize() )
			{
				// Don't need to read length, it's fixed length and the length was networked down already.
				nBytes = GetUserDataSize();
				contract_assert( nBytes > 0 );
				tempbuf[nBytes-1] = 0; // be safe, clear last byte
				buf.ReadBits( tempbuf, GetUserDataSizeBits() );
			}
			else
			{
				nBytes = buf.ReadUBitLong( CNetworkStringTableItem::MAX_USERDATA_BITS );
				if ( !( nBytes <= sizeof( tempbuf ) ) ) Error("CNetworkStringTableClient::ParseUpdate: message too large (%d bytes).", nBytes);

				buf.ReadBytes( tempbuf, nBytes );
			}

			pUserData = tempbuf;
		}

		// Check if we are updating an old entry or adding a new one
		if ( entryIndex < GetNumStrings() )
		{
			SetStringUserData( entryIndex, nBytes, pUserData );
#ifdef _DEBUG
			if ( pEntry )
			{
				contract_assert( !V_strcmp( pEntry, GetString( entryIndex ) ) ); // make sure string didn't change
			}
#endif
			pEntry = GetString( entryIndex ); // string didn't change
		}
		else
		{
			// Grow the table (entryindex must be the next empty slot)
			contract_assert( (entryIndex == GetNumStrings()) && (pEntry != nullptr) );
				
			if ( pEntry == nullptr )
			{
				Msg("CNetworkStringTable::ParseUpdate: NULL pEntry, table %s, index %i\n", GetTableName(), entryIndex );
				pEntry = "";// avoid crash because of NULL strings
			}

			AddString( true, pEntry, nBytes, pUserData );
		}

		if ( std::ssize( history ) > 31 )
		{
			history.erase( history.begin() + 0 );
		}

		StringHistoryEntry she;
		V_strncpy( she.string, pEntry, sizeof( she.string ) );
		history.push_back( she );
	}
}

void CNetworkStringTable::CopyStringTable(CNetworkStringTable * table)
{
	contract_assert( m_pItems->Count() == 0 ); // table must be empty before coping

	for ( unsigned int i = 0; i < table->m_pItems->Count() ; ++i )
	{
		CNetworkStringTableItem	*item = &table->m_pItems->Element( i );

		m_nTickCount = item->m_nTickChanged;

		AddString( true, table->GetString( i ), item->m_nUserDataLength, item->m_pUserData );
	}
}

void CNetworkStringTable::TriggerCallbacks( int tick_ack )
{
	if ( m_changeFunc == nullptr )
		return;

	COM_TimestampedLog( "Change(%s):Start", GetTableName() );

	int count = m_pItems->Count();

	for ( int i = 0; i < count; i++ )
	{
		CNetworkStringTableItem *pItem = &m_pItems->Element( i );

		// mirror is up to date
		if ( pItem->GetTickChanged() <= tick_ack )
			continue;

		int userDataSize;
		const void *pUserData = pItem->GetUserData( &userDataSize );

		// fire the callback function
		( *m_changeFunc )( m_pObject, this, i, GetString( i ), pUserData );
	}

	COM_TimestampedLog( "Change(%s):End", GetTableName() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : changeFunc - 
//-----------------------------------------------------------------------------
void CNetworkStringTable::SetStringChangedCallback( void *object, pfnStringChanged changeFunc )
{
	m_changeFunc = changeFunc;
	m_pObject = object;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *client - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNetworkStringTable::ChangedSinceTick( int tick ) const
{
	return ( m_nLastChangedTick > tick );
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *value - 
// Output : int
//-----------------------------------------------------------------------------
int CNetworkStringTable::AddString( bool bIsServer, const char *string, int length /*= -1*/, const void *userdata /*= NULL*/ )
{
	bool bHasChanged;
	CNetworkStringTableItem *item;
	
	if ( !string )
	{
		contract_assert( string );
		ConMsg( "Warning:  Can't add NULL string to table %s\n", GetTableName() );
		return INVALID_STRING_INDEX;
	}

#ifdef _DEBUG
	if ( m_bLocked )
	{
		DevMsg("Warning! CNetworkStringTable::AddString: adding '%s' while locked.\n", string );
	}
#endif

	int i = m_pItems->Find( string );
	if ( !bIsServer )
	{
		if ( m_pItems->IsValidIndex( i ) && !m_pItemsClientSide )
		{
			bIsServer = true;
		}
		else if ( !m_pItemsClientSide )
		{
			// NOTE:  YWB 9/11/2008
			// This case is probably a bug the since the server "owns" the state of the string table and if the client adds 
			// some extra value in and then the server comes along and uses that slot, then all hell breaks loose (probably).  
			// SetAllowClientSideAddString was added to allow for client side only precaching to be in a separate chunk of the table -- it should be used in this case.
			// TODO:  Make this a Sys_Error?
			DevMsg( "CNetworkStringTable::AddString:  client added string which server didn't put into table (consider SetAllowClientSideAddString?): %s %s\n", GetTableName(), string );
		}
	}

	if ( !bIsServer && m_pItemsClientSide )
	{
		i = m_pItemsClientSide->Find( string );

		if ( !m_pItemsClientSide->IsValidIndex( i ) )
		{
			// not in list yet, create it now
			if ( m_pItemsClientSide->Count() >= (unsigned int)GetMaxStrings() )
			{
				// Too many strings, FIXME: Print warning message
				ConMsg( "Warning:  Table %s is full, can't add %s\n", GetTableName(), string );
				return INVALID_STRING_INDEX;
			}

			// create new item
			{
			i = m_pItemsClientSide->Insert( string );
			}

			item = &m_pItemsClientSide->Element( i );

			// set changed ticks

			item->m_nTickChanged = m_nTickCount;

			item->m_nTickCreated = m_nTickCount;

			if ( m_bChangeHistoryEnabled )
			{
				item->EnableChangeHistory();
			}

			bHasChanged = true;
		}
		else
		{
			item = &m_pItemsClientSide->Element( i ); // item already exists
			bHasChanged = false;  // not changed yet
		}

		if ( length > -1 )
		{
			if ( item->SetUserData( m_nTickCount, length, userdata ) )
			{
				bHasChanged = true;
			}
		}

		if ( bHasChanged && !m_bChangeHistoryEnabled )
		{
			DataChanged( -i, item );
		}

		// Negate i for returning to client
		i = -i;
	}
	else
	{
		// See if it's already there
		if ( !m_pItems->IsValidIndex( i ) )
		{
			// not in list yet, create it now
			if ( m_pItems->Count() >= (unsigned int)GetMaxStrings() )
			{
				// Too many strings, FIXME: Print warning message
				ConMsg( "Warning:  Table %s is full, can't add %s\n", GetTableName(), string );
				return INVALID_STRING_INDEX;
			}

			// create new item
			{
			i = m_pItems->Insert( string );
			}

			item = &m_pItems->Element( i );

			// set changed ticks
			item->m_nTickChanged = m_nTickCount;

			item->m_nTickCreated = m_nTickCount;

			if ( m_bChangeHistoryEnabled )
			{
				item->EnableChangeHistory();
			}

			bHasChanged = true;
		}
		else
		{
			item = &m_pItems->Element( i ); // item already exists
			bHasChanged = false;  // not changed yet
		}

		if ( length > -1 )
		{
			if ( item->SetUserData( m_nTickCount, length, userdata ) )
			{
				bHasChanged = true;
			}
		}

		if ( bHasChanged && !m_bChangeHistoryEnabled )
		{
			DataChanged( i, item );
		}
	}

	return i;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : stringNumber - 
// Output : const char
//-----------------------------------------------------------------------------
const char *CNetworkStringTable::GetString( int stringNumber ) const
{
	INetworkStringDict *dict = m_pItems;
	if ( m_pItemsClientSide && stringNumber < -1 )
	{
		dict = m_pItemsClientSide;
		stringNumber = -stringNumber;
	}

	contract_assert( dict->IsValidIndex( stringNumber ) );

	if ( dict->IsValidIndex( stringNumber ) )
	{
		return dict->String( stringNumber );
	}
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : stringNumber - 
//			length - 
//			*userdata - 
//-----------------------------------------------------------------------------
void CNetworkStringTable::SetStringUserData( int stringNumber, int length /*=0*/, const void *userdata /*= 0*/ )
{
#ifdef _DEBUG
	if ( m_bLocked )
	{
		DevMsg("Warning! CNetworkStringTable::SetStringUserData: changing entry %i while locked.\n", stringNumber );
	}
#endif

	INetworkStringDict *dict = m_pItems;
	int saveStringNumber = stringNumber;
	if ( m_pItemsClientSide && stringNumber < -1 )
	{
		dict = m_pItemsClientSide;
		stringNumber = -stringNumber;
	}

	contract_assert( (length == 0 && userdata == nullptr) || ( length > 0 && userdata != nullptr) );
	contract_assert( dict->IsValidIndex( stringNumber ) );
	CNetworkStringTableItem *p = &dict->Element( stringNumber );
	contract_assert( p );

	if ( p->SetUserData( m_nTickCount, length, userdata ) )
	{
		// Mark changed
		DataChanged( saveStringNumber, p );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *item - 
//-----------------------------------------------------------------------------
void CNetworkStringTable::DataChanged( int stringNumber, CNetworkStringTableItem *item )
{
	contract_assert( item );

	if ( !item )
		return;

	// Mark table as changed
	m_nLastChangedTick = m_nTickCount;
	
	// Invoke callback if one was installed
	
	if ( m_changeFunc != nullptr )
	{
		int userDataSize;
		const void *pUserData = item->GetUserData( &userDataSize );
		( *m_changeFunc )( m_pObject, this, stringNumber, GetString( stringNumber ), pUserData );
	}

}

void CNetworkStringTable::WriteStringTable( bf_write& buf )
{
	int numstrings = m_pItems->Count();
	buf.WriteWord( numstrings );
	for ( int i = 0 ; i < numstrings; i++ )
	{
		buf.WriteString( GetString( i ) );
		int userDataSize;
		const void *pUserData = GetStringUserData( i, &userDataSize );
		if ( userDataSize > 0 )
		{
			buf.WriteOneBit( 1 );
			buf.WriteWord( (short)userDataSize );
			buf.WriteBytes( pUserData, userDataSize );
		}
		else
		{
			buf.WriteOneBit( 0 );
		}
	}

	if ( m_pItemsClientSide )
	{
		buf.WriteOneBit( 1 );

		numstrings = m_pItemsClientSide->Count();
		buf.WriteWord( numstrings );
		for ( int i = 0 ; i < numstrings; i++ )
		{
			buf.WriteString( m_pItemsClientSide->String( i ) );
			int userDataSize;
			const void *pUserData = m_pItemsClientSide->Element( i ).GetUserData( &userDataSize );
			if ( userDataSize > 0 )
			{
				buf.WriteOneBit( 1 );
				buf.WriteWord( (short)userDataSize );
				buf.WriteBytes( pUserData, userDataSize );
			}
			else
			{
				buf.WriteOneBit( 0 );
			}
		}

	}
	else
	{
		buf.WriteOneBit( 0 );
	}
}

bool CNetworkStringTable::ReadStringTable( bf_read& buf )
{
	DeleteAllStrings();

	int numstrings = buf.ReadWord();
	for ( int i = 0 ; i < numstrings; i++ )
	{
		char stringname[4096];
		
		buf.ReadString( stringname, sizeof( stringname ) );

		contract_assert( V_strlen( stringname ) < 100 );

		if ( buf.ReadOneBit() == 1 )
		{
			int userDataSize = (int)buf.ReadWord();
			contract_assert( userDataSize > 0 );
			byte *data = new byte[ userDataSize + 4 ];
			contract_assert( data );

			buf.ReadBytes( data, userDataSize );


			AddString( true, stringname, userDataSize, data );

			delete[] data;

			contract_assert( buf.GetNumBytesLeft() > 10 );
			
		}
		else
		{
			AddString( true, stringname );
		}
	}

	// Client side stuff
	if ( buf.ReadOneBit() == 1 )
	{
		int numstrings = buf.ReadWord();
		for ( int i = 0 ; i < numstrings; i++ )
		{
			char stringname[4096];

			buf.ReadString( stringname, sizeof( stringname ) );

			if ( buf.ReadOneBit() == 1 )
			{
				int userDataSize = (int)buf.ReadWord();
				contract_assert( userDataSize > 0 );
				byte *data = new byte[ userDataSize + 4 ];
				contract_assert( data );

				buf.ReadBytes( data, userDataSize );

				if ( i >= 2 )
				{
					AddString( false, stringname, userDataSize, data );
				}

				delete[] data;

			}
			else
			{
				if ( i >= 2 )
				{
					AddString( false, stringname );
				}
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : stringNumber - 
//			length - 
// Output : const void
//-----------------------------------------------------------------------------
const void *CNetworkStringTable::GetStringUserData( int stringNumber, int *length ) const
{
	INetworkStringDict *dict = m_pItems;
	if ( m_pItemsClientSide && stringNumber < -1 )
	{
		dict = m_pItemsClientSide;
		stringNumber = -stringNumber;
	}

	CNetworkStringTableItem *p;

	contract_assert( dict->IsValidIndex( stringNumber ) );
	p = &dict->Element( stringNumber );
	contract_assert( p );
	return p->GetUserData( length );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CNetworkStringTable::GetNumStrings( void ) const
{
	return m_pItems->Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : stringTable - 
//			*string - 
// Output : int
//-----------------------------------------------------------------------------
int CNetworkStringTable::FindStringIndex( char const *string )
{
	int i = m_pItems->Find( string );
	if ( m_pItems->IsValidIndex( i ) )
		return i;

	if ( m_pItemsClientSide )
	{
		i = m_pItemsClientSide->Find( string );
		if ( m_pItemsClientSide->IsValidIndex( i ) )
			return -i;
	}

	return INVALID_STRING_INDEX;
}

void CNetworkStringTable::UpdateDictionaryString( int stringNumber )
{
	if ( !IsUsingDictionary() )
	{
		return;
	}
	// Client side only items don't need to deal with dictionary
	if ( m_pItemsClientSide && stringNumber < -1 )
	{
		return;
	}
	CheckDictionary( stringNumber );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNetworkStringTable::Dump( void )
{
	ConMsg( "Table %s\n", GetTableName() );
	ConMsg( "  %i/%i items\n", GetNumStrings(), GetMaxStrings() );
	for ( int i = 0; i < GetNumStrings() ; i++ )
	{
		if ( IsUsingDictionary() )
		{
			int nCurrentDictionaryIndex = m_pItems->DictionaryIndex( i );
			
			if ( nCurrentDictionaryIndex != -1 )
			{
				ConMsg( "d(%05d) %i : %s\n", nCurrentDictionaryIndex, i, m_pItems->String( i ) );
			}
			else
			{
				ConMsg( "         %i : %s\n", i, m_pItems->String( i ) );
			}
		}
		else
		{
			ConMsg( "   %i : %s\n", i, m_pItems->String( i ) );
		}
		
	}
	if ( m_pItemsClientSide )
	{
		for ( int i = 0; i < (int)m_pItemsClientSide->Count() ; i++ )
		{
			ConMsg( "   (c)%i : %s\n", i, m_pItemsClientSide->String( i ) );
		}
	}
	ConMsg( "\n" );
}

static ConVar sv_temp_baseline_string_table_buffer_size( "sv_temp_baseline_string_table_buffer_size", "131072", 0, "Buffer size for writing string table baselines" );

bool CNetworkStringTable::WriteBaselines( CSVCMsg_CreateStringTable_t &msg )
{
	msg.Clear();

	// allocate the temp buffer for the packet ents
	msg.string_data.mut().resize( sv_temp_baseline_string_table_buffer_size.GetInt() );
	bf_write string_data_buf( &(msg.string_data.mut())[0], msg.string_data->size() );

	msg.flags = m_nFlags;
	msg.name = GetTableName();
	msg.max_entries = GetMaxStrings();
	msg.num_entries = GetNumStrings();
	msg.user_data_fixed_size = IsUserDataFixedSize();
	msg.user_data_size = GetUserDataSize();
	msg.user_data_size_bits = GetUserDataSizeBits();

	// tick = -1 ensures that all entries are updated = baseline
	int entries = WriteUpdate( nullptr, string_data_buf, -1 );

	// resize the buffer to the actual byte size
	msg.string_data.mut().resize( Bits2Bytes( string_data_buf.GetNumBitsWritten() ) );

	return entries == msg.num_entries;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNetworkStringTableContainer::CNetworkStringTableContainer( void )
{
	m_bAllowCreation = false;
	m_bLocked = true;
	m_nTickCount = 0;
	m_bEnableRollback = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNetworkStringTableContainer::~CNetworkStringTableContainer( void )
{
	RemoveAllTables();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNetworkStringTableContainer::AllowCreation( bool state )
{
	m_bAllowCreation = state;
}

bool  CNetworkStringTableContainer::Lock( bool bLock )
{
	bool oldLock = m_bLocked;

	m_bLocked = bLock;

	// Determine if an update is needed
	for ( int i = 0; i < std::ssize( m_Tables ); i++ )
	{
		CNetworkStringTable *table = (CNetworkStringTable*) GetTable( i );

		table->Lock( bLock );
	}
	return oldLock;
}

void CNetworkStringTableContainer::SetAllowClientSideAddString( INetworkStringTable *table, bool bAllowClientSideAddString )
{
	for ( int i = 0; i < std::ssize( m_Tables ); i++ )
	{
		CNetworkStringTable *t = (CNetworkStringTable*) GetTable( i );
		if ( t == table )
		{
			t->SetAllowClientSideAddString( bAllowClientSideAddString );
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *tableName - 
//			maxentries - 
// Output : TABLEID
//-----------------------------------------------------------------------------
INetworkStringTable *CNetworkStringTableContainer::CreateStringTable( const char *tableName, int maxentries, int userdatafixedsize /*= 0*/, int userdatanetworkbits /*= 0*/, int flags /*= NSF_NONE*/ )
{
	if ( !m_bAllowCreation )
	{
		Sys_Error( "Tried to create string table '%s' at wrong time\n", tableName );
		return nullptr;
	}

	CNetworkStringTable *pTable = (CNetworkStringTable*) FindTable( tableName );

	if ( pTable != nullptr )
	{
		Sys_Error( "Tried to create string table '%s' twice\n", tableName );
		return nullptr;
	}

	if ( std::ssize( m_Tables ) >= MAX_TABLES )
	{
		Sys_Error( "Only %i string tables allowed, can't create'%s'", MAX_TABLES, tableName);
		return nullptr;
	}

	TABLEID id = std::ssize( m_Tables );

	pTable = new CNetworkStringTable( id, tableName, maxentries, userdatafixedsize, userdatanetworkbits, flags );

	contract_assert( pTable );

	if ( m_bEnableRollback )
	{
		pTable->EnableRollback();
	}

	pTable->SetTick( m_nTickCount );

	m_Tables.push_back( pTable );

	return pTable;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *tableName - 
//-----------------------------------------------------------------------------
INetworkStringTable *CNetworkStringTableContainer::FindTable( const char *tableName ) const
{
	for ( int i = 0; i < std::ssize( m_Tables ); i++ )
	{
		if ( !V_stricmp( tableName, m_Tables[ i ]->GetTableName() ) )
			return m_Tables[i];
	}

	return nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : stringTable - 
// Output : CNetworkStringTableServer
//-----------------------------------------------------------------------------
INetworkStringTable *CNetworkStringTableContainer::GetTable( TABLEID stringTable ) const
{
	if ( stringTable < 0 || stringTable >= std::ssize( m_Tables ) )
		return nullptr;

	return m_Tables[ stringTable ];
}

int CNetworkStringTableContainer::GetNumTables( void ) const
{
	return std::ssize( m_Tables );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNetworkStringTableContainer::WriteBaselines( char const *pchMapName, bf_write &buf )
{
	if ( g_StringTableDictionary.ShouldRecreateDictionary( pchMapName ) )
	{
		// Creates dictionary, will write it out after level is exited
		CreateDictionary( pchMapName );
	}

	CSVCMsg_CreateStringTable_t msg;

	for ( int i = 0 ; i < std::ssize( m_Tables ) ; i++ )
	{
		CNetworkStringTable *table = (CNetworkStringTable*) GetTable( i );

		int before = buf.GetNumBytesWritten();
		if ( !table->WriteBaselines( msg ) )
		{
			Host_Error( "Index error writing string table baseline %s\n", table->GetTableName() );
		}

		if ( !msg.WriteToBuffer( buf ) )
		{
			Host_Error( "Overflow error writing string table baseline %s\n", table->GetTableName() );
		}
		int after = buf.GetNumBytesWritten();
		if ( sv_dumpstringtables.GetBool() )
		{
			DevMsg( "CNetworkStringTableContainer::WriteBaselines wrote %d bytes for table %s [space remaining %d bytes]\n", after - before, table->GetTableName(), buf.GetNumBytesLeft() );
		}
	}
}

void CNetworkStringTableContainer::WriteStringTables( bf_write& buf )
{
	int numTables = std::ssize( m_Tables );

	buf.WriteByte( numTables );
	for ( int i = 0; i < numTables; i++ )
	{
		CNetworkStringTable *table = m_Tables[ i ];
		buf.WriteString( table->GetTableName() );
		table->WriteStringTable( buf );
	}
}

bool CNetworkStringTableContainer::ReadStringTables( bf_read& buf )
{
	int numTables = buf.ReadByte();
	for ( int i = 0 ; i < numTables; i++ )
	{
		char tablename[ 256 ];
		buf.ReadString( tablename, sizeof( tablename ) );

		// Find this table by name
		CNetworkStringTable *table = (CNetworkStringTable*)FindTable( tablename );
		contract_assert( table );

		// Now read the data for the table
		if ( !table->ReadStringTable( buf ) )
		{
			Host_Error( "Error reading string table %s\n", tablename );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *cl - 
//			*msg - 
//-----------------------------------------------------------------------------
void CNetworkStringTableContainer::WriteUpdateMessage( CBaseClient *client, int tick_ack, bf_write &buf )
{
	CVProfScope _( "CNetworkStringTableContainer::WriteUpdateMessage", 0, VPROF_BUDGETGROUP_OTHER_NETWORKING, false, BUDGETFLAG_OTHER );

	//a working buffer to build our string tables into. Note that this is on the stack, so sanity check that the size doesn't get too large (hence the compile assert below).
	//If it does, a heap allocated solution will be needed
	uint8 StringTableBuff[ NET_MAX_PAYLOAD ];
	static_assert( sizeof( StringTableBuff ) < 300 * 1024 );

	// Determine if an update is needed
	for ( int i = 0; i < std::ssize( m_Tables ); i++ )
	{
		CNetworkStringTable *table = (CNetworkStringTable*) GetTable( i );

		if ( !table )
			continue;

		if ( !table->ChangedSinceTick( tick_ack ) )
			continue;

		CSVCMsg_UpdateStringTable_t msg;

		//setup a writer for the bits that go to our temporary buffer so we can assign it over later
		bf_write string_data_buf( StringTableBuff, sizeof( StringTableBuff ) );

		msg.table_id = table->GetTableId();
		msg.num_changed_entries = table->WriteUpdate( client, string_data_buf, tick_ack );

		//handle the situation where the data may have been truncated
		if( string_data_buf.IsOverflowed() )
			return;

		contract_assert( msg.num_changed_entries > 0 ); // don't send unnecessary empty updates

		//copy over the data we wrote into the actual message
		msg.string_data.mut().assign( StringTableBuff, StringTableBuff + Bits2Bytes( string_data_buf.GetNumBitsWritten() ) );

		if ( !msg.WriteToBuffer( buf ) )
			return;

		if ( client &&
			 client->IsTracing() )
		{
			client->TraceNetworkData( buf, "StringTable %s", table->GetTableName() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *cl - 
//			*msg - 
//-----------------------------------------------------------------------------
void CNetworkStringTableContainer::DirectUpdate( int tick_ack )
{
	CVProfScope _( "CNetworkStringTableContainer::DirectUpdate", 0, VPROF_BUDGETGROUP_OTHER_NETWORKING, false, BUDGETFLAG_OTHER );

	// Determine if an update is needed
	for ( int i = 0; i < std::ssize( m_Tables ); i++ )
	{
		CNetworkStringTable *table = (CNetworkStringTable*) GetTable( i );

		contract_assert( table );
		
		if ( !table->ChangedSinceTick( tick_ack ) )
			continue;

		table->UpdateMirrorTable( tick_ack );
	}
}

void CNetworkStringTableContainer::EnableRollback( bool bState )
{
	// we can't dis/enable rollback if we already created tabled
	contract_assert( std::ssize( m_Tables ) == 0 );

	m_bEnableRollback = bState;
}


void CNetworkStringTableContainer::RestoreTick( int tick )
{
	for ( int i = 0; i < std::ssize( m_Tables ); i++ )
	{
		CNetworkStringTable *table = (CNetworkStringTable*) GetTable( i );

		contract_assert( table );

		table->RestoreTick( tick );
	}
}

void CNetworkStringTableContainer::TriggerCallbacks( int tick_ack )
{
	// Determine if an update is needed
	for ( int i = 0; i < std::ssize( m_Tables ); i++ )
	{
		CNetworkStringTable *table = (CNetworkStringTable*) GetTable( i );

		contract_assert( table );

		if ( !table->ChangedSinceTick( tick_ack ) )
			continue;

		table->TriggerCallbacks( tick_ack );
	}
}

void CNetworkStringTableContainer::SetTick( int tick_count)
{
	// Assert( tick_count > 0 );

	m_nTickCount = tick_count;

	// Determine if an update is needed
	for ( int i = 0; i < std::ssize( m_Tables ); i++ )
	{
		CNetworkStringTable *table = (CNetworkStringTable*) GetTable( i );

		contract_assert( table );

		table->SetTick( tick_count );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNetworkStringTableContainer::RemoveAllTables( void )
{
	while ( !m_Tables.empty() )
	{
		CNetworkStringTable *table = m_Tables[ 0 ];
		m_Tables.erase( m_Tables.begin() + 0 );
		delete table;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNetworkStringTableContainer::Dump( void )
{
	for ( int i = 0; i < std::ssize( m_Tables ); i++ )
	{
		m_Tables[ i ]->Dump();
	}
}

void CNetworkStringTableContainer::CreateDictionary( char const *pchMapName )
{
	// Don't do this on Game Consoles!!!

	char mapPath[ MAX_PATH ];
	V_snprintf( mapPath, sizeof( mapPath ), "maps/%s.bsp", pchMapName );

	// Make sure that the file is writable before building stringtable dictionary.
	if( !g_pFileSystem->IsFileWritable( mapPath, "GAME" ) )
	{
		Warning( "#####################################################################################\n" );
		Warning( "Can't recreate dictionary for %s, file must be writable!!!\n", mapPath );
		Warning( "#####################################################################################\n" );
		return;
	}

	Msg( "Creating dictionary %s\n", pchMapName );

	// Create dictionary
	ks::byte_writer buf;

	for ( int i = 0; i < std::ssize( m_Tables ); ++i )
	{
		CNetworkStringTable *table = m_Tables[ i ];

		if ( !table->IsUsingDictionary() )
			continue;

		int nNumStrings = table->GetNumStrings();
		for ( int j = 0; j < nNumStrings; ++j )
		{
			char const *str = table->GetString( j );
			// Skip empty strings (slot 0 is sometimes encoded as "")
			if ( !*str )
				continue;
			buf.write_cstring( str );
		}
	}

	g_StringTableDictionary.CacheNewStringTableForWriteToBSPOnLevelShutdown( pchMapName, buf.bytes(), false ); 
}

void CNetworkStringTableContainer::UpdateDictionaryStrings()
{
	for ( int i = 0; i < std::ssize( m_Tables ); ++i )
	{
		CNetworkStringTable *table = m_Tables[ i ];

		if ( !table->IsUsingDictionary() )
			continue;

		int nNumStrings = table->GetNumStrings();
		for ( int j = 0; j < nNumStrings; ++j )
		{
			table->UpdateDictionaryString( j );
		}
	}
}
