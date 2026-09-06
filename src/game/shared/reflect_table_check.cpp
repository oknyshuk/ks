// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include "cbase.h"
#include "reflect_table_check.h"

#include <cstdio>
#include <unistd.h>

#include "tier0/memdbgon.h"

namespace ks::reflect
{

static int g_nDiffs = 0;
static CUtlVector< void ( * )() > *g_pVerifiers = NULL;

const char *SafeName( const char *s ) { return s ? s : "(null)"; }

bool SameStr( const char *a, const char *b )
{
	if ( a == b ) return true;
	if ( !a || !b ) return false;
	return V_strcmp( a, b ) == 0;
}

// Straight to stderr. Both DLLInit and the client's equivalent run before the engine's spew
// handlers are usable for failures: Error() there blocks and prints nothing, so a mismatch
// hung until SIGKILL instead of failing.
void ReportDiff( const char *table, const char *prop, const char *field, const char *fmt, ... )
{
	char detail[256];
	va_list ap;
	va_start( ap, fmt );
	V_vsnprintf( detail, sizeof( detail ), fmt, ap );
	va_end( ap );

	++g_nDiffs;
	fprintf( stderr, "reflect table: %s.%s: %s %s\n", table, prop, field, detail );
}

// The legacy tables are built by static initializers, so nothing may compare against them
// until every translation unit's initializers have run.
void RegisterVerify( void ( *fn )() )
{
	if ( !g_pVerifiers ) g_pVerifiers = new CUtlVector< void ( * )() >;
	g_pVerifiers->AddToTail( fn );
}

int DiffCount() { return g_nDiffs; }

void RunAllVerifications()
{
	if ( !g_pVerifiers ) return;
	for ( int i = 0; i < g_pVerifiers->Count(); ++i )
		g_pVerifiers->Element( i )();

	if ( g_nDiffs )
	{
		fprintf( stderr, "reflect table: %d generated tables failed to construct\n",
		         g_nDiffs );
		fflush( stderr );
		_exit( 70 );
	}
	Msg( "reflect table: %d generated tables constructed\n", g_pVerifiers->Count() );
}

} // namespace ks::reflect
