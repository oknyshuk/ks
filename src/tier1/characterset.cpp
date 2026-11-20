// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.platform;
import ks.simd;
import ks.tier1.characterset;

//-----------------------------------------------------------------------------
// Purpose: builds a simple lookup table of a group of important characters
// Input  : *pParseGroup - pointer to the buffer for the group
//			*pGroupString - null terminated list of characters to flag
//-----------------------------------------------------------------------------
void CharacterSetBuild( characterset_t *pSetBuffer, const char *pszSetString )
{
	int i = 0;

	// Test our pointers
	if ( !pSetBuffer || !pszSetString )
		return;

	std::memset( pSetBuffer->set, 0, sizeof(pSetBuffer->set) );

	while ( pszSetString[i] )
	{
		pSetBuffer->set[ pszSetString[i] ] = 1;
		i++;
	}

}
