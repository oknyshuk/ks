// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.vstdlib.ikeyvaluessystem;

import std;
import ks.core.wchartypes;
import ks.platform;
import ks.simd;
import ks.tier1.fixed_pool;
import ks.tier1.memstack;

export extern "C++" {

// handle to a KeyValues key name symbol
typedef int HKeySymbol;
inline constexpr auto INVALID_KEY_SYMBOL = (-1);

//-----------------------------------------------------------------------------
// Purpose: Central storage point for KeyValues memory and symbols (the shared
//			KeyValues symbol table).  There is one instance, see KeyValuesSystem().
//-----------------------------------------------------------------------------
class IKeyValuesSystem
{
public:
	IKeyValuesSystem();
	~IKeyValuesSystem();

	// registers the size of the KeyValues in the specified instance
	// so it can build a properly sized memory pool for the KeyValues objects
	// the sizes will usually never differ but this is for versioning safety
	void RegisterSizeofKeyValues(int size);

	// allocates/frees a KeyValues object from the shared mempool
	void *AllocKeyValuesMemory(int size);
	void FreeKeyValuesMemory(void *pMem);

	// symbol table access (used for key names)
	HKeySymbol GetSymbolForString( const char *name, bool bCreate = true );
	const char *GetStringForSymbol(HKeySymbol symbol);

	// returns the wide version of ansi, also does the lookup on #'d strings
	void GetLocalizedFromANSI( const char *ansi, wchar_t *outBuf, int unicodeBufferSizeInBytes);
	void GetANSIFromLocalized( const wchar_t *wchar, char *outBuf, int ansiBufferSizeInBytes );

	// for debugging, adds KeyValues record into global list so we can track memory leaks
	void AddKeyValuesToMemoryLeakList(void *pMem, HKeySymbol name);
	void RemoveKeyValuesFromMemoryLeakList(void *pMem);

	// set/get a value for keyvalues resolution symbol
	// e.g.: SetKeyValuesExpressionSymbol( "LOWVIOLENCE", true ) - enables [$LOWVIOLENCE]
	void SetKeyValuesExpressionSymbol( const char *name, bool bValue );
	bool GetKeyValuesExpressionSymbol( const char *name );

	// symbol table access from code with case-preserving requirements (used for key names)
	HKeySymbol GetSymbolForStringCaseSensitive( HKeySymbol &hCaseInsensitiveSymbol, const char *name, bool bCreate = true );

private:
	int m_iMaxKeyValuesSize;

	// string hash table
	/*
	Here's the way key values system data structures are laid out:
	hash table with 2047 hash buckets:
	[0] { hash_item_t }
	[1]
	[2]
	...
	each hash_item_t's stringIndex is an offset in m_Strings memory
	at that offset we store the actual null-terminated string followed
	by another 3 bytes for an alternative capitalization.
	These 3 trailing bytes are set to 0 if no alternative capitalization
	variants are present in the dictionary.
	These trailing 3 bytes are interpreted as stringIndex into m_Strings
	memory for the next	alternative capitalization

	Getting a string value by HKeySymbol : constant time access at the
	string memory represented by stringIndex

	Getting a symbol for a string value:
	1)	compute the hash
	2)	start walking the hash-bucket using special version of stricmp
		until a case insensitive match is found
	3a) for case-insensitive lookup return the found stringIndex
	3b) for case-sensitive lookup keep walking the list of alternative
		capitalizations using strcmp until exact case match is found
	*/
	CMemoryStack m_Strings;
	struct hash_item_t
	{
		int stringIndex;
		hash_item_t *next;
	};
	ks::fixed_pool m_HashItemMemPool;
	std::vector< hash_item_t > m_HashTable;
	int CaseInsensitiveHash(const char *string, int iBounds);

	std::map< void *, int > m_KeyValuesTrackingList;	// live block -> name symbol

	std::map< HKeySymbol, bool > m_KvConditionalSymbolTable;

	std::recursive_mutex m_mutex;
};

IKeyValuesSystem *KeyValuesSystem();

// #define KEYVALUESSYSTEM_INTERFACE_VERSION "KeyValuesSystem002"

}
