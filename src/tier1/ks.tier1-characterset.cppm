// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1:characterset;

import std;
import ks.math;

export extern "C++" {

struct characterset_t
{
	char set[256];
};

// This is essentially a strpbrk() using a precalculated lookup table
//-----------------------------------------------------------------------------
// Purpose: builds a simple lookup table of a group of important characters
// Input  : *pSetBuffer - pointer to the buffer for the group
//			*pSetString - list of characters to flag
//-----------------------------------------------------------------------------
extern void CharacterSetBuild( characterset_t *pSetBuffer, const char *pSetString );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSetBuffer - pre-build group buffer
//			character - character to lookup
// Output : int - 1 if the character was in the set
//-----------------------------------------------------------------------------
template <typename T1, typename T2> [[gnu::always_inline]] inline auto IN_CHARACTERSET( T1 SetBuffer, T2 character ) { return ((SetBuffer).set[(character)]); }

}
