//========= Copyright (c) 2009, Valve Corporation, All rights reserved. ============//
#include "tier0/platform.h"
#include "tier1/miniprofiler_hash.h"

#if ENABLE_HARDWARE_PROFILER
#include "tier1/map_ops.h"
#include "tier1/strtools.h"
ks::idict<CLinkedMiniProfiler*> g_mapMiniProfilers;
DLL_IMPORT CLinkedMiniProfiler *g_pOtherMiniProfilers;
CLinkedMiniProfiler *HashMiniProfiler( const char *pString )
{
	auto [it, bInserted] = g_mapMiniProfilers.try_emplace( pString, nullptr );
	if( bInserted )
	{
		// this profiler does not exist yet; it lives as long as the process and names itself by the map key
		it->second = new CLinkedMiniProfiler( it->first.c_str(), &g_pOtherMiniProfilers );
	}
	return it->second;
}
CLinkedMiniProfiler *HashMiniProfilerF( const char *pFormat, ... )
{
	va_list args;
	va_start( args, pFormat );
	char buffer[2048];
	Q_vsnprintf( buffer, sizeof( buffer ), pFormat, args );
	CLinkedMiniProfiler *pProfiler = HashMiniProfiler( buffer );
	va_end( args )   ;
	return pProfiler;
}
#else
CMiniProfiler *HashMiniProfiler( const char *pString )
{
	return nullptr;
}
#endif
