// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core.dbg;
import ks.core.memalloc;
import ks.core.platform;
import ks.core.types;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.fixed_pool;
import ks.tier1.keyvalues;
import ks.tier1.map_ops;
import ks.tier1.strtools;
import ks.vstdlib.ikeyvaluessystem;


//
// Defines platform-endian-specific macros:
// MEM_4BYTES_AS_0_AND_3BYTES :  present a 4 byte uint32 as a memory
//                               layout where first memory byte is zero
//                               and the other 3 bytes represent value
// MEM_4BYTES_FROM_0_AND_3BYTES: unpack from memory with first zero byte
//                               and 3 value bytes the original uint32 value
//
// used for efficiently reading/writing storing 3 byte values into memory
// region immediately following a null-byte-terminated string, essentially
// sharing the null-byte-terminator with the first memory byte
//
// Number in memory has lowest-byte in front, use shifts to make it zero
template <typename T1> [[gnu::always_inline]] inline auto MEM_4BYTES_AS_0_AND_3BYTES( T1 x4bytes ) { return ( ( (uint32) (x4bytes) ) << 8 ); }
template <typename T1> [[gnu::always_inline]] inline auto MEM_4BYTES_FROM_0_AND_3BYTES( T1 x03bytes ) { return ( ( (uint32) (x03bytes) ) >> 8 ); }

//-----------------------------------------------------------------------------
// Instance singleton and expose interface to rest of code
//-----------------------------------------------------------------------------
static IKeyValuesSystem g_KeyValuesSystem;

IKeyValuesSystem *KeyValuesSystem()
{
	return &g_KeyValuesSystem;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
IKeyValuesSystem::IKeyValuesSystem() :
	m_HashItemMemPool(sizeof(hash_item_t), 64, ks::fixed_pool::grow::fast, "IKeyValuesSystem::m_HashItemMemPool")
{
	// initialize hash table
	m_HashTable.resize( m_HashTable.size() + 2047 );
	for (int i = 0; i < std::ssize( m_HashTable ); i++)
	{
		m_HashTable[i].stringIndex = 0;
		m_HashTable[i].next = nullptr;
	}

	m_Strings.Init( "IKeyValuesSystem::m_Strings", 4*1024*1024, 64*1024, 0, 4 );
	// Make 0 stringIndex to never be returned, by allocating
	// and wasting minimal number of alignment bytes now:
	char *pszEmpty = ((char *)m_Strings.Alloc(1));
	*pszEmpty = 0;

	m_iMaxKeyValuesSize = sizeof(KeyValues);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
IKeyValuesSystem::~IKeyValuesSystem()
{
}

//-----------------------------------------------------------------------------
// Purpose: registers the size of the KeyValues in the specified instance
//			so it can build a properly sized memory pool for the KeyValues objects
//			the sizes will usually never differ but this is for versioning safety
//-----------------------------------------------------------------------------
void IKeyValuesSystem::RegisterSizeofKeyValues(int size)
{
	if (size > m_iMaxKeyValuesSize)
	{
		m_iMaxKeyValuesSize = size;
	}
}

//-----------------------------------------------------------------------------
// Purpose: allocates a KeyValues object from the shared mempool
//-----------------------------------------------------------------------------
void *IKeyValuesSystem::AllocKeyValuesMemory(int size)
{
	return MemAlloc_Alloc( size );
}

//-----------------------------------------------------------------------------
// Purpose: frees a KeyValues object from the shared mempool
//-----------------------------------------------------------------------------
void IKeyValuesSystem::FreeKeyValuesMemory(void *pMem)
{
	g_pMemAlloc->Free( pMem );
}

//-----------------------------------------------------------------------------
// Purpose: symbol table access (used for key names)
//-----------------------------------------------------------------------------
HKeySymbol IKeyValuesSystem::GetSymbolForString( const char *name, bool bCreate )
{
	if ( !name )
	{
		return (-1);
	}

	std::scoped_lock lock( m_mutex );

	int hash = CaseInsensitiveHash(name, std::ssize( m_HashTable ));
	int i = 0;
	hash_item_t *item = &m_HashTable[hash];
	while (1)
	{
		if (!stricmp(name, (char *)m_Strings.GetBase() + item->stringIndex ))
		{
			return (HKeySymbol)item->stringIndex;
		}

		i++;

		if (item->next == nullptr)
		{
			if ( !bCreate )
			{
				// not found
				return -1;
			}

			// we're not in the table
			if (item->stringIndex != 0)
			{
				// first item is used, an new item
				item->next = (hash_item_t *)m_HashItemMemPool.alloc(sizeof(hash_item_t));
				item = item->next;
			}

			// build up the new item
			item->next = nullptr;
			int numStringBytes = std::strlen(name);
			char *pString = (char *)m_Strings.Alloc( numStringBytes + 1 + 3 );
			if ( !pString )
			{
				Error( "Out of keyvalue string space" );
				return -1;
			}
			item->stringIndex = pString - (char *)m_Strings.GetBase();
			V_memcpy( pString, name, numStringBytes );
			* reinterpret_cast< uint32 * >( pString + numStringBytes ) = 0;	// string null-terminator + 3 alternative spelling bytes
			return (HKeySymbol)item->stringIndex;
		}

		item = item->next;
	}

	// shouldn't be able to get here
	contract_assert( 0 );
	return (-1);
}

//-----------------------------------------------------------------------------
// Purpose: symbol table access (used for key names)
//-----------------------------------------------------------------------------
HKeySymbol IKeyValuesSystem::GetSymbolForStringCaseSensitive( HKeySymbol &hCaseInsensitiveSymbol, const char *name, bool bCreate )
{
	if ( !name )
	{
		return (-1);
	}

	std::scoped_lock lock( m_mutex );

	int hash = CaseInsensitiveHash(name, std::ssize( m_HashTable ));
	int numNameStringBytes = -1;
	int i = 0;
	hash_item_t *item = &m_HashTable[hash];
	while (1)
	{
		char *pCompareString = (char *)m_Strings.GetBase() + item->stringIndex;
		int iResult = _V_stricmp_NegativeForUnequal( name, pCompareString );
		if ( iResult == 0 )
		{
			// strings are exactly equal matching every letter's case
			hCaseInsensitiveSymbol = (HKeySymbol)item->stringIndex;
			return (HKeySymbol)item->stringIndex;
		}
		else if ( iResult > 0 )
		{
			// strings are equal in a case-insensitive compare, but have different case for some letters
			// Need to walk the case-resolving chain
			numNameStringBytes = V_strlen( pCompareString );
			uint32 *pnCaseResolveIndex = reinterpret_cast< uint32 * >( pCompareString + numNameStringBytes );
			hCaseInsensitiveSymbol = (HKeySymbol)item->stringIndex;
			while ( int nAlternativeStringIndex = MEM_4BYTES_FROM_0_AND_3BYTES( *pnCaseResolveIndex ) )
			{
				pCompareString = (char *)m_Strings.GetBase() + nAlternativeStringIndex;
				int iResult = std::strcmp( name, pCompareString );
				if ( !iResult )
				{
					// found an exact match
					return (HKeySymbol)nAlternativeStringIndex;
				}
				// Keep traversing alternative case-resolving chain
				pnCaseResolveIndex = reinterpret_cast< uint32 * >( pCompareString + numNameStringBytes );
			}
			// Reached the end of alternative case-resolving chain, pnCaseResolveIndex is pointing at 0 bytes
			// indicating no further alternative stringIndex
			if ( !bCreate )
			{
				// If we aren't interested in creating the actual string index,
				// then return symbol with default capitalization
				// NOTE: this is not correct value, but it cannot be used to create a new value anyway,
				// only for locating a pre-existing value and lookups are case-insensitive
				return (HKeySymbol)item->stringIndex;
			}
			else
			{
				char *pString = (char *)m_Strings.Alloc( numNameStringBytes + 1 + 3 );
				if ( !pString )
				{
					Error( "Out of keyvalue string space" );
					return -1;
				}
				int nNewAlternativeStringIndex = pString - (char *)m_Strings.GetBase();
				V_memcpy( pString, name, numNameStringBytes );
				* reinterpret_cast< uint32 * >( pString + numNameStringBytes ) = 0;	// string null-terminator + 3 alternative spelling bytes
				*pnCaseResolveIndex = MEM_4BYTES_AS_0_AND_3BYTES( nNewAlternativeStringIndex );	// link previous spelling entry to the new entry
				return (HKeySymbol)nNewAlternativeStringIndex;
			}
		}

		i++;

		if (item->next == nullptr)
		{
			if ( !bCreate )
			{
				// not found
				return -1;
			}

			// we're not in the table
			if (item->stringIndex != 0)
			{
				// first item is used, an new item
				item->next = (hash_item_t *)m_HashItemMemPool.alloc(sizeof(hash_item_t));
				item = item->next;
			}

			// build up the new item
			item->next = nullptr;
			int numStringBytes = std::strlen(name);
			char *pString = (char *)m_Strings.Alloc( numStringBytes + 1 + 3 );
			if ( !pString )
			{
				Error( "Out of keyvalue string space" );
				return -1;
			}
			item->stringIndex = pString - (char *)m_Strings.GetBase();
			V_memcpy( pString, name, numStringBytes );
			* reinterpret_cast< uint32 * >( pString + numStringBytes ) = 0;	// string null-terminator + 3 alternative spelling bytes
			hCaseInsensitiveSymbol = (HKeySymbol)item->stringIndex;
			return (HKeySymbol)item->stringIndex;
		}

		item = item->next;
	}

	// shouldn't be able to get here
	contract_assert( 0 );
	return (-1);
}

//-----------------------------------------------------------------------------
// Purpose: symbol table access
//-----------------------------------------------------------------------------
const char *IKeyValuesSystem::GetStringForSymbol(HKeySymbol symbol)
{
	if ( symbol == -1 )
	{
		return "";
	}
	return ((char *)m_Strings.GetBase() + (std::size_t)symbol);
}

//-----------------------------------------------------------------------------
// Purpose: adds KeyValues record into global list so we can track memory leaks
//-----------------------------------------------------------------------------
void IKeyValuesSystem::AddKeyValuesToMemoryLeakList(void *pMem, HKeySymbol name)
{
}

//-----------------------------------------------------------------------------
// Purpose: used to track memory leaks
//-----------------------------------------------------------------------------
void IKeyValuesSystem::RemoveKeyValuesFromMemoryLeakList(void *pMem)
{
}

//-----------------------------------------------------------------------------
// Purpose: generates a simple hash value for a string
//-----------------------------------------------------------------------------
int IKeyValuesSystem::CaseInsensitiveHash(const char *string, int iBounds)
{
	unsigned int hash = 0;

	for ( ; *string != 0; string++ )
	{
		if (*string >= 'A' && *string <= 'Z')
		{
			hash = (hash << 1) + (*string - 'A' + 'a');
		}
		else
		{
			hash = (hash << 1) + *string;
		}
	}
	  
	return hash % iBounds;
}

//-----------------------------------------------------------------------------
// Purpose: set/get a value for keyvalues resolution symbol
// e.g.: SetKeyValuesExpressionSymbol( "LOWVIOLENCE", true ) - enables [$LOWVIOLENCE]
//-----------------------------------------------------------------------------
void IKeyValuesSystem::SetKeyValuesExpressionSymbol( const char *name, bool bValue )
{
	if ( !name )
		return;

	if ( name[0] == '$' )
		++ name;

	HKeySymbol hSym = GetSymbolForString( name, true );	// find or create symbol
	
	{
		std::scoped_lock lock( m_mutex );
		m_KvConditionalSymbolTable.insert_or_assign( hSym, bValue );
	}
}

bool IKeyValuesSystem::GetKeyValuesExpressionSymbol( const char *name )
{
	if ( !name )
		return false;

	if ( name[0] == '$' )
		++ name;

	HKeySymbol hSym = GetSymbolForString( name, false );	// find or create symbol
	if ( hSym != -1 )
	{
		std::scoped_lock lock( m_mutex );
		if ( bool *pValue = ks::find_ptr( m_KvConditionalSymbolTable, hSym ) )
		{
			// Found the symbol value in conditional symbol table
			return *pValue;
		}
	}

	//
	// Fallback conditionals
	//

	if ( !V_stricmp( name, "INPUTSWAPAB" ) )
	{
		// Registered dynamically by the input system in InitializeXDevices() once
		// controllers are enumerated (true only for Japanese PS3 button layout).
		// KV files can be parsed before input init, so provide a false default
		// here to avoid a bogus "Unknown symbol" warning. If the input system has
		// already registered a value, the symbol-table lookup above wins.
		return false;
	}

	if ( !V_stricmp( name, "GAMECONSOLESPLITSCREEN" ) )
	{
		return false;
	}

	if ( !V_stricmp( name, "GAMECONSOLEGUEST" ) )
	{
		return false;
	}

	if ( !V_stricmp( name, "ENGLISH" ) ||
		 !V_stricmp( name, "JAPANESE" ) ||
		 !V_stricmp( name, "GERMAN" ) ||
		 !V_stricmp( name, "FRENCH" ) ||
		 !V_stricmp( name, "SPANISH" ) ||
		 !V_stricmp( name, "ITALIAN" ) ||
		 !V_stricmp( name, "KOREAN" ) ||
		 !V_stricmp( name, "TCHINESE" ) ||
		 !V_stricmp( name, "PORTUGUESE" ) ||
		 !V_stricmp( name, "SCHINESE" ) ||
		 !V_stricmp( name, "POLISH" ) ||
		 !V_stricmp( name, "RUSSIAN" ) ||
		 !V_stricmp( name, "TURKISH" ) )
	{
		// the language symbols are true if we are in that language
		// english is assumed when no language is present
		const char *pLanguageString;
		static ConVarRef cl_language( "cl_language" );
		pLanguageString = cl_language.GetString();
		if ( !pLanguageString || !pLanguageString[0] )
		{
			pLanguageString = "english";
		}
		if ( !V_stricmp( name, pLanguageString ) )
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	// very expensive, back door for DLC updates
	if ( !V_strnicmp( name, "CVAR_", 5 ) )
	{
		ConVarRef cvRef( name + 5 );
		if ( cvRef.IsValid() )
			return cvRef.GetBool();
	}

	// purposely warn on these to prevent syntax errors
	// need to get these fixed asap, otherwise unintended false behavior
	Warning( "KV Conditional: Unknown symbol %s\n", name );
	return false;
}
