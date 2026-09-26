//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		A registry of strings and associated ints
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef STRINGREGISTRY_H
#define STRINGREGISTRY_H
#pragma once

#include "tier1/map_ops.h"

//-----------------------------------------------------------------------------
// Purpose: Just a convenience/legacy wrapper for ks::idict<> .
//-----------------------------------------------------------------------------
class CStringRegistry
{
public:
	// Keys are iterators: valid until ClearStrings.
	using Key = ks::idict<int>::const_iterator;

private:
	ks::idict<int>	*m_pStringList;

public:
	// returns a key for a given string; an existing string keeps its first ID
	Key AddString(const char *stringText, int stringID);

	// This is optimized.  It will do 2 O(logN) searches
	// Only one of the searches needs to compare strings, the other compares symbols (ints)
	// returns -1 if the string is not present in the registry.
	int		GetStringID(const char *stringText);

	// This is unoptimized.  It will linearly search (but only compares ints, not strings)
	const char	*GetStringText(int stringID);

	// This is O(1).  It will not search.  key MUST be a value that was returned by AddString
	const char *GetStringForKey(Key key);
	// This is O(1).  It will not search.  key MUST be a value that was returned by AddString
	int		GetIDForKey(Key key);

	void	ClearStrings(void);


	// Iterate all the keys.
	Key First() const;
	Key Next( Key key ) const;
	Key InvalidIndex() const;

	~CStringRegistry(void);			// Need to free allocated memory
	CStringRegistry(void);
};

#endif // STRINGREGISTRY_H
