// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2013 Valve Corporation

import std;
import ks.tier1;
#include "matchmaking/imatchframework.h"
#include "edict.h"
#include "igame.h"
#include "host_cmd.h"
#include "status.h"



static char g_szCombinedStatusBuffer[10240] = {0};
static char g_szMinidumpInfoBuffer[4094] = {0}; // 4094 because ETW marks are limited to this.
static char g_szHostStatusBuffer[1024] = {0};
static char g_szMemoryStatusBuffer[1024] = {0};
static char g_szMatchmakingStatusBuffer[4094] = {0}; // 4094 because ETW marks are limited to this.
static char* g_szAudioStatusBuffer; // Returned by audio code
extern CGlobalVars g_ServerGlobalVariables;
extern int  gHostSpawnCount;
extern int g_nMapLoadCount;

#ifdef DEDICATED
PAGED_POOL_INFO_t g_pagedpoolinfo;
char g_minidumpinfo[ 4094 ] = {0};
#else
extern PAGED_POOL_INFO_t g_pagedpoolinfo;
extern char g_minidumpinfo[ 4094 ];
#endif
extern bool g_bUpdateMinidumpComment;
static PAGED_POOL_INFO_t final;

//-----------------------------------------------------------------------------

template < std::size_t N >
static void Status_CopyText( char (&dest)[N], const ks::text_writer &text )
{
	V_strncpy( dest, text.c_str(), N );
}

static void Status_UpdateMemoryStatus( bool bIncludeFullMemoryInfo )
{
	g_szMemoryStatusBuffer[0] = 0;
	ks::text_writer buf;

	if ( bIncludeFullMemoryInfo )
	{
	}

	if ( g_pMemAlloc->MemoryAllocFailed() )
	{
		buf.printf( "*** Memory allocation failed for %d bytes! ***\n", (int)g_pMemAlloc->MemoryAllocFailed() );
	}

	GenericMemoryStat_t *pMemoryStats = nullptr;
	int nMemoryStatCount = g_pMemAlloc->GetGenericMemoryStats( &pMemoryStats );
	if ( nMemoryStatCount > 0 )
	{
		buf.printf( "g_pMemAlloc->GetGenericMemoryStats(): %d\n", nMemoryStatCount );

		for ( int i = 0; i < nMemoryStatCount; i++ )
		{
			buf.printf( "%d. %s : %d \n", i + 1, pMemoryStats[i].name, pMemoryStats[i].value );
		}
	}
	buf.put( '\n' );
	Status_CopyText( g_szMemoryStatusBuffer, buf );
}

static void Status_UpdateMinidumpCommentBuffer()
{
	g_szMinidumpInfoBuffer[0] = 0;
	ks::text_writer minidumpInfoBuffer;

	minidumpInfoBuffer.printf( "Uptime: %f\n", Plat_FloatTime() );
	if ( g_minidumpinfo[0] != 0 )
	{
		minidumpInfoBuffer.printf( "%s", g_minidumpinfo );
	}
	minidumpInfoBuffer.printf( "Active: %s\nSpawnCount %d\nMapLoad Count %d\n", ( game && game->IsActiveApp() ) ? "active" : "inactive", gHostSpawnCount, g_nMapLoadCount );
	Status_CopyText( g_szMinidumpInfoBuffer, minidumpInfoBuffer );
}

static ks::text_writer *s_pStatusBuffer = nullptr;

static void Status_PrintCallback( const char *fmt, ... )
{
	if ( s_pStatusBuffer )
	{
		std::va_list		argptr;
		__builtin_va_start (argptr,fmt);
		s_pStatusBuffer->vprintf( fmt, argptr );
		__builtin_va_end (argptr);
	}
}

static void Status_UpdateHostStatusBuffer()
{
	g_szHostStatusBuffer[0] = 0;
	ks::text_writer hostStatusBuffer;
	s_pStatusBuffer = &hostStatusBuffer;
	Host_PrintStatus( kCommandSrcCode, Status_PrintCallback, false );
	s_pStatusBuffer = nullptr;
	Status_CopyText( g_szHostStatusBuffer, hostStatusBuffer );
}

static void Status_UpdateMatchmakingBuffer()
{
	// matchmaking status text
	g_szMatchmakingStatusBuffer[0] = 0;
	if ( g_pMatchFramework )
	{
		if ( IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession() )
		{
			ks::text_writer matchmakingStatusBuffer;
			
			matchmakingStatusBuffer.printf( "match session %p\n", pMatchSession );

			auto writeStatusText = [&matchmakingStatusBuffer]( char const *szText ) { matchmakingStatusBuffer.put( szText ); return true; };
			CKeyValuesDumpContext kvDumpContext( writeStatusText );

			matchmakingStatusBuffer.printf( "session system data:\n" );
			pMatchSession->GetSessionSystemData()->Dump( &kvDumpContext );

			matchmakingStatusBuffer.printf( "session settings:\n" );
			pMatchSession->GetSessionSettings()->Dump( &kvDumpContext );

			Status_CopyText( g_szMatchmakingStatusBuffer, matchmakingStatusBuffer );
		}
	}

	if ( !g_szMatchmakingStatusBuffer[0] )
	{
		V_sprintf_safe( g_szMatchmakingStatusBuffer, "No matchmaking.\n" );
	}
}

static void ConvertNewlinesToTabsinPlace( char *pBuffer )
{
	while( pBuffer && *pBuffer != 0 )
	{
		pBuffer = V_strstr(pBuffer, "\n");
		if ( pBuffer )
		{
			*pBuffer = '\t';
		}
	}
}

static void Status_UpdateBuffers( bool bForETW )
{
	Status_UpdateMinidumpCommentBuffer();
	Status_UpdateMemoryStatus( !bForETW );
	Status_UpdateHostStatusBuffer();
	Status_UpdateMatchmakingBuffer();

#ifndef DEDICATED
	extern char* Status_UpdateAudioBuffer();
	g_szAudioStatusBuffer = Status_UpdateAudioBuffer();
#else
	static char chNoAudio[8] = { 'n', '/', 'a', 0 };
	g_szAudioStatusBuffer = chNoAudio;
#endif

	if ( bForETW )
	{
		// walk through buffers converting \n to \t
		ConvertNewlinesToTabsinPlace( g_szMinidumpInfoBuffer );
		ConvertNewlinesToTabsinPlace( g_szMemoryStatusBuffer );
		ConvertNewlinesToTabsinPlace( g_szHostStatusBuffer );
		ConvertNewlinesToTabsinPlace( g_szMatchmakingStatusBuffer );
		ConvertNewlinesToTabsinPlace( g_szAudioStatusBuffer );
	}
}

static void Status_UpdateCombinedBuffer()
{
	Status_UpdateBuffers( false );

	g_szCombinedStatusBuffer[0] = 0;
	ks::text_writer combinedStatusBuffer;

	combinedStatusBuffer.printf( "\nGame Info:\n%s", g_szMinidumpInfoBuffer );
	combinedStatusBuffer.printf( "\nMemory Info:\n%s", g_szMemoryStatusBuffer );
	combinedStatusBuffer.printf( "\nHost Info:\n%s", g_szHostStatusBuffer );
	combinedStatusBuffer.printf( "\nMatchmaking Info:\n%s", g_szMatchmakingStatusBuffer );
	combinedStatusBuffer.printf( "\nAudio Info:\n%s", g_szAudioStatusBuffer );
	Status_CopyText( g_szCombinedStatusBuffer, combinedStatusBuffer );
}

// public functions

const char *Status_GetBuffer()
{
	return g_szCombinedStatusBuffer;
}

void Status_Update()
{
	Status_UpdateCombinedBuffer();
}

void Status_CheckSendETWMark()
{
	// If ETW (Event Tracing for Windows) is available, 
	// collect status text every 5 seconds
}
