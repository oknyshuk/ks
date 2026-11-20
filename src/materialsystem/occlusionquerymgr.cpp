// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

import std;


import ks.core.dbg;
import ks.platform;
import ks.simd;


#include "materialsystem_global.h"
#include "materialsystem/imesh.h"

#include "imaterialinternal.h"
#include "imaterialsysteminternal.h"


#include "occlusionquerymgr.h"
#include "imaterialsysteminternal.h"

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static COcclusionQueryMgr s_OcclusionQueryMgr;
COcclusionQueryMgr *g_pOcclusionQueryMgr = &s_OcclusionQueryMgr;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
COcclusionQueryMgr::COcclusionQueryMgr()
{
	m_nFrameCount = 0;
}


//-----------------------------------------------------------------------------
// Allocate and delete query objects.
//-----------------------------------------------------------------------------
OcclusionQueryObjectHandle_t COcclusionQueryMgr::CreateOcclusionQueryObject( )
{
	m_Mutex.lock();
	OcclusionQueryObject_t *pObject;
	if ( !m_FreeOcclusionQueryObjects.empty() )
	{
		pObject = m_FreeOcclusionQueryObjects.back();
		m_FreeOcclusionQueryObjects.pop_back();
		*pObject = OcclusionQueryObject_t();
	}
	else
	{
		pObject = &m_OcclusionQueryObjects.emplace_back();
	}
	m_Mutex.unlock();
	return (OcclusionQueryObjectHandle_t)pObject;
}

void COcclusionQueryMgr::OnCreateOcclusionQueryObject( OcclusionQueryObjectHandle_t h )
{
	for ( int i = 0; i < COUNT_OCCLUSION_QUERY_STACK; i++)
	{
		FromHandle( h ).m_QueryHandle[i] = g_pShaderAPI->CreateOcclusionQueryObject( );
	}
}

// Flushes an outstanding query
// HEY - Be very careful using this method - it causes a full pipeline flush/stall!
void COcclusionQueryMgr::FlushQuery( OcclusionQueryObjectHandle_t hOcclusionQuery, int nIndex )
{
	// Flush out any previous queries
	OcclusionQueryObject_t &query = FromHandle( hOcclusionQuery );
	if ( query.m_bHasBeenIssued[nIndex] )
	{
		ShaderAPIOcclusionQuery_t hQuery = query.m_QueryHandle[nIndex];
		
		while ( OCCLUSION_QUERY_RESULT_PENDING == g_pShaderAPI->OcclusionQuery_GetNumPixelsRendered( hQuery, true ) )
			continue;
	}
}

void COcclusionQueryMgr::DestroyOcclusionQueryObject( OcclusionQueryObjectHandle_t hOcclusionQuery )
{
	contract_assert( IsValidHandle( hOcclusionQuery ) );
	if ( IsValidHandle( hOcclusionQuery ) )
	{
		OcclusionQueryObject_t &query = FromHandle( hOcclusionQuery );
		for ( int i = 0; i < COUNT_OCCLUSION_QUERY_STACK; i++)
		{
			if ( query.m_QueryHandle[i] != INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE )
			{
				g_pShaderAPI->DestroyOcclusionQueryObject( query.m_QueryHandle[i] );
			}
		}
		m_Mutex.lock();
		query.m_bInUse = false;
		m_FreeOcclusionQueryObjects.push_back( &query );
		m_Mutex.unlock();
	}
}


//-----------------------------------------------------------------------------
// Advance frame
//-----------------------------------------------------------------------------
void COcclusionQueryMgr::AdvanceFrame()
{
	++m_nFrameCount;
}


//-----------------------------------------------------------------------------
// Alt-tab support
// NOTE: This doesn't queue anything up
//-----------------------------------------------------------------------------
void COcclusionQueryMgr::AllocOcclusionQueryObjects( void )
{
	for ( OcclusionQueryObject_t &query : m_OcclusionQueryObjects )
	{
		if ( !query.m_bInUse )
			continue;
		for ( int i = 0; i < COUNT_OCCLUSION_QUERY_STACK; i++)
		{
			query.m_QueryHandle[i] = g_pShaderAPI->CreateOcclusionQueryObject();
			query.m_bHasBeenIssued[i] = false;		// any in-flight queries are never returning
		}
	}
}

void COcclusionQueryMgr::FreeOcclusionQueryObjects( void )
{
	for ( OcclusionQueryObject_t &query : m_OcclusionQueryObjects )
	{
		if ( !query.m_bInUse )
			continue;
		for ( int i = 0; i < COUNT_OCCLUSION_QUERY_STACK; i++)
		{
			if ( query.m_QueryHandle[i] != INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE )
			{
				g_pShaderAPI->DestroyOcclusionQueryObject( query.m_QueryHandle[i] );
				query.m_QueryHandle[i] = INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE;
				query.m_bHasBeenIssued[i] = false;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Used to make the handle think it's never had a successful query before
//-----------------------------------------------------------------------------
void COcclusionQueryMgr::ResetOcclusionQueryObject( OcclusionQueryObjectHandle_t hOcclusionQuery )
{
	contract_assert( IsValidHandle( hOcclusionQuery ) );
	if ( IsValidHandle( hOcclusionQuery ) )
	{
		OcclusionQueryObject_t &query = FromHandle( hOcclusionQuery );
		// Forget we've issued any previous queries - there's no need to flush them.
		for ( int i = 0; i < COUNT_OCCLUSION_QUERY_STACK; i++)
		{
			query.m_bHasBeenIssued[i] = false;
		}

		query.m_LastResult = -1;
		query.m_nFrameIssued = -1;
	}
}


//-----------------------------------------------------------------------------
// Bracket drawing with begin and end so that we can get counts next frame.
//-----------------------------------------------------------------------------
void COcclusionQueryMgr::BeginOcclusionQueryDrawing( OcclusionQueryObjectHandle_t hOcclusionQuery )
{
	contract_assert( IsValidHandle( hOcclusionQuery ) );
	if ( IsValidHandle( hOcclusionQuery ) )
	{
		OcclusionQueryObject_t &query = FromHandle( hOcclusionQuery );
		int nCurrent = query.m_nCurrentIssue;
		ShaderAPIOcclusionQuery_t hQuery = query.m_QueryHandle[nCurrent];
		if ( hQuery != INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE )
		{
			// If it's been issued, but we haven't gotten a result when we polled last time,
			// try polling one last time, since we can't poll again after we issue again.
			if ( query.m_bHasBeenIssued[nCurrent] )
			{
				int nPixels = g_pShaderAPI->OcclusionQuery_GetNumPixelsRendered( hQuery, false );
				if ( ( nPixels == OCCLUSION_QUERY_RESULT_PENDING ) && ( query.m_nFrameIssued == m_nFrameCount ) )
				{
					static int s_nWarnCount = 0;
					if ( s_nWarnCount++ < 5 )
					{
						DevWarning( "blocking issue in occlusion queries! Grab brian!\n" );
					}
				}
				while( !OCCLUSION_QUERY_FINISHED( nPixels ) ) 
				{
					// We're going to reuse this query, so issue a flush to force the query results to come back.
					nPixels = g_pShaderAPI->OcclusionQuery_GetNumPixelsRendered( hQuery, true );
				}
				if ( nPixels >= 0 )
				{
					query.m_LastResult = nPixels;
				}
				query.m_bHasBeenIssued[nCurrent] = false;
			}
			g_pShaderAPI->BeginOcclusionQueryDrawing( hQuery );
		}
	}
}


void COcclusionQueryMgr::EndOcclusionQueryDrawing( OcclusionQueryObjectHandle_t hOcclusionQuery )
{
	contract_assert( IsValidHandle( hOcclusionQuery ) );
	if ( IsValidHandle( hOcclusionQuery ) )
	{
		OcclusionQueryObject_t &query = FromHandle( hOcclusionQuery );
		int nCurrent = query.m_nCurrentIssue;
		ShaderAPIOcclusionQuery_t hQuery = query.m_QueryHandle[nCurrent];
		if ( hQuery != INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE )
		{
			g_pShaderAPI->EndOcclusionQueryDrawing( hQuery );

			query.m_bHasBeenIssued[nCurrent] = true;
			query.m_nFrameIssued = m_nFrameCount;

			nCurrent = (nCurrent+1) % COUNT_OCCLUSION_QUERY_STACK;
			query.m_nCurrentIssue = nCurrent;
		}
	}
}


//-----------------------------------------------------------------------------
// Get the number of pixels rendered between begin and end on an earlier frame.
// Calling this in the same frame is a huge perf hit!
//-----------------------------------------------------------------------------
void COcclusionQueryMgr::OcclusionQuery_IssueNumPixelsRenderedQuery( OcclusionQueryObjectHandle_t hOcclusionQuery )
{
	contract_assert( IsValidHandle( hOcclusionQuery ) );
	if ( IsValidHandle( hOcclusionQuery ) )
	{
		OcclusionQueryObject_t &query = FromHandle( hOcclusionQuery );
		for( int i = 0; i < COUNT_OCCLUSION_QUERY_STACK; i++ )
		{
			int nIndex = ( query.m_nCurrentIssue + i ) % COUNT_OCCLUSION_QUERY_STACK;
			ShaderAPIOcclusionQuery_t hQuery = query.m_QueryHandle[nIndex];
			if ( hQuery != INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE && query.m_bHasBeenIssued[nIndex] )
			{
				int nPixels = g_pShaderAPI->OcclusionQuery_GetNumPixelsRendered( hQuery );
				if ( nPixels == OCCLUSION_QUERY_RESULT_ERROR )
				{
					// In GL mode, it's possible for queries to fail (say when mat_queue_mode is toggled). In this case, just clear m_bHasBeenIssued and forget we ever issued this query.
					query.m_bHasBeenIssued[nIndex] = false;
				}
				else if ( nPixels >= 0 )
				{
					query.m_LastResult = nPixels;
					query.m_bHasBeenIssued[nIndex] = false;
				}

			}
		}
	}
}

int COcclusionQueryMgr::OcclusionQuery_GetNumPixelsRendered( OcclusionQueryObjectHandle_t h, bool bDoQuery )
{
	if ( bDoQuery )
	{
		OcclusionQuery_IssueNumPixelsRenderedQuery( h );
	}

	int nPixels = FromHandle( h ).m_LastResult;
	return nPixels;
}
