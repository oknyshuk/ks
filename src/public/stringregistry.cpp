// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: A registry of strings and associated ints

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
import std;
import ks.platform;
import ks.simd;
import ks.tier1.map_ops;
#if defined(CLIENT_DLL)
#elif defined(GAME_DLL)
#endif
#include "stringregistry.h"

//-----------------------------------------------------------------------------
// Purpose: Add null terminated string to the string registry 
// Input  :
// Output :
//-----------------------------------------------------------------------------
CStringRegistry::Key CStringRegistry::AddString(const char *stringText, int stringID)
{
	return m_pStringList->emplace( stringText, stringID ).first;
}

//-----------------------------------------------------------------------------
// Purpose: Given string text get the string ID
// Input  :	Text of string to find
// Output : Return string id or -1 if no such string exists
//-----------------------------------------------------------------------------
int	CStringRegistry::GetStringID( const char *stringText )
{
	if ( int *pID = ks::find_ptr( *m_pStringList, stringText ) )
	{
		return *pID;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Given a string ID return the string text
// Input  : ID of string to find
// Output : Return string text of NULL of no such ID exists
//-----------------------------------------------------------------------------
char const *CStringRegistry::GetStringText( int stringID )
{
	for ( auto &[name, id] : *m_pStringList )
	{
		if ( id == stringID )
		{
			return name.c_str();
		}
	}

	return nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Given a key return the string text
//-----------------------------------------------------------------------------
char const *CStringRegistry::GetStringForKey( Key key )
{
	if ( key == m_pStringList->end() )
		return nullptr;

	return key->first.c_str();
}

//-----------------------------------------------------------------------------
// Purpose: Given a key return the string text
//-----------------------------------------------------------------------------
int CStringRegistry::GetIDForKey( Key key )
{
	if ( key == m_pStringList->end() )
		return 0;

	return key->second;
}

//-----------------------------------------------------------------------------
// Purpose: Clear all strings from the string registry
//-----------------------------------------------------------------------------
void CStringRegistry::ClearStrings(void)
{
	m_pStringList->clear();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor - delete the list of strings and maps
// Input  :
// Output :
//-----------------------------------------------------------------------------
CStringRegistry::~CStringRegistry(void)
{
	delete m_pStringList;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  :
// Output :
//-----------------------------------------------------------------------------
CStringRegistry::CStringRegistry(void)
{
	m_pStringList = new ks::idict<int>;
}


CStringRegistry::Key CStringRegistry::First() const
{
	return m_pStringList->begin();
}

CStringRegistry::Key CStringRegistry::Next( Key key ) const
{
	return std::next( key );
}

CStringRegistry::Key CStringRegistry::InvalidIndex() const
{
	return m_pStringList->end();
}
