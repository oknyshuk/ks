//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#if !defined( GC )
#include "igamesystem.h"
#endif

#include "gamestringpool.h"
#include "tier1/map_ops.h"

#include <algorithm>
#include <string>
#include <unordered_set>

#include "tier0/memalloc.h"
//-----------------------------------------------------------------------------
// Purpose: The actual storage for pooled per-level strings
//-----------------------------------------------------------------------------
#if !defined( GC )
class CGameStringPool : public CBaseGameSystem
#else
class CGameStringPool
#endif
{
	virtual char const *Name() { return "CGameStringPool"; }

	virtual void LevelShutdownPostEntity() 
	{
		FreeAll();
		PurgeDeferredDeleteList();
		CGameString::IncrementSerialNumber();
	}

public:
	// Case-insensitive; keeps the first spelling. string_t holds the returned pointer, which a node set keeps stable.
	const char *Allocate( const char *pszValue )
	{
		auto it = m_Strings.find( std::string_view( pszValue ) );
		if ( it == m_Strings.end() )
			it = m_Strings.emplace( pszValue ).first;
		return it->c_str();
	}

	const char *Find( const char *pszValue )
	{
		if ( !pszValue )
			return nullptr;
		auto it = m_Strings.find( std::string_view( pszValue ) );
		return it == m_Strings.end() ? nullptr : it->c_str();
	}

	void FreeAll()
	{
		m_Strings.clear();
	}

	void PurgeDeferredDeleteList()
	{
		m_DeferredDeleteList.clear(); m_DeferredDeleteList.shrink_to_fit();
	}

	void Dump( void )
	{
		std::vector< const char * > sorted;
		for ( const std::string &s : m_Strings )
			sorted.push_back( s.c_str() );
		std::sort( sorted.begin(), sorted.end(), ks::cstr_iless() );
		for ( int i = 0; i < std::ssize( sorted ); i++ )
		{
			DevMsg( "  %d (0x%p) : %s\n", i, sorted[i], sorted[i] );
		}
		DevMsg( "\n" );
		DevMsg( "Size:  %d items\n", (int)m_Strings.size() );
	}

	// Existing string_t's may still point at the string, so it stays allocated until level shutdown.
	void Remove( const char *pszValue )
	{
		if ( !pszValue )
			return;
		auto it = m_Strings.find( std::string_view( pszValue ) );
		if ( it != m_Strings.end() )
		{
			m_DeferredDeleteList.push_back( m_Strings.extract( it ) );
		}
	}

private:
	using StringSet_t = std::unordered_set< std::string, ks::ihash, ks::iequal >;
	StringSet_t m_Strings;
	std::vector< StringSet_t::node_type > m_DeferredDeleteList;
};

static CGameStringPool g_GameStringPool;


//-----------------------------------------------------------------------------
// String system accessor
//-----------------------------------------------------------------------------
#if !defined( GC )
IGameSystem *GameStringSystem()
{
	return &g_GameStringPool;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: The public accessor for the level-global pooled strings
//-----------------------------------------------------------------------------
string_t AllocPooledString( const char * pszValue )
{
	if (pszValue && *pszValue)
		return MAKE_STRING( g_GameStringPool.Allocate( pszValue ) );
	return NULL_STRING;
}

string_t FindPooledString( const char *pszValue )
{
	return MAKE_STRING( g_GameStringPool.Find( pszValue ) );
}

void RemovePooledString( const char *pszValue )
{
	g_GameStringPool.Remove( pszValue );
}

int CGameString::gm_iSerialNumber = 1;

#if !defined(CLIENT_DLL) && !defined( GC )
//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
void CC_DumpGameStringTable( void )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	g_GameStringPool.Dump();
}
static ConCommand dumpgamestringtable("dumpgamestringtable", CC_DumpGameStringTable, "Dump the contents of the game string table to the console.", FCVAR_CHEAT);
#endif
