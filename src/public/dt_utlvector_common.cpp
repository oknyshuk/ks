// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.types;
import ks.platform;
import ks.simd;
import ks.tier1.map_ops;
#include "dt_utlvector_common.h"
import std;

static ks::idict<int> *g_STDict = 0;
static ks::idict<int> *g_RTDict = 0;


char* AllocateStringHelper2( const char *pFormat, std::va_list marker )
{
	char str[512];
	std::vsnprintf( str, sizeof( str ), pFormat, marker );
	str[ sizeof(str) - 1 ] = 0;
	
	int len = std::strlen( str ) + 1;
	char *pRet = new char[len];
	std::memcpy( pRet, str, len );

	return pRet;
}


char* AllocateStringHelper( const char *pFormat, ... )
{
	std::va_list marker;
	__builtin_va_start( marker, pFormat );
	char *pRet = AllocateStringHelper2( pFormat, marker );
	__builtin_va_end( marker );

	return pRet;
}


char* AllocateUniqueDataTableName( bool bSendTable, const char *pFormat, ... )
{
	// Setup the string.
	std::va_list marker;
	__builtin_va_start( marker, pFormat );
	char *pRet = AllocateStringHelper2( pFormat, marker );
	__builtin_va_end( marker );

	// Make sure it's unique.
#ifdef _DEBUG
	// Have to allocate them here because if they're declared as straight global variables,
	// their constructors won't have been called yet by the time we get in here.
	if ( !g_STDict )
	{
		g_STDict = new ks::idict<int>;
		g_RTDict = new ks::idict<int>;
	}

	ks::idict<int> *pDict = bSendTable ? g_STDict : g_RTDict;
	if ( !pDict->emplace( pRet, 0 ).second )
	{
		// If it hits this, then they have 2 utlvectors in different data tables with the same name and the
		// same size limit. The names of 
		contract_assert( false );
	}
#endif

	return pRet;
}
