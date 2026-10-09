// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: work in progress

import std;
import ks.tier1;
#include "shareddefs.h"
#include "util.h"
#include "global_event_log.h"
#include "filesystem.h"



CGlobalEventLog	GlobalEventLog;

static	ks::intern_pool	EventSymbols;
static	ConVar			global_event_log_enabled( "global_event_log_enabled", "0", FCVAR_CHEAT, "Enables the global event log system" );

class CGlobalEventLine
{
public:
	CGlobalEventLine( );
	~CGlobalEventLine( );

	void	Clear( );
	bool	SetStaticText( const char *pszValue );
	bool	SetVaryingText( const char *pszValue );

	bool	IsDirty( ) { return m_bDirty; }

	void	Write( ks::text_writer &buffer );
	void	ClearDirty( );		

private:
	std::string_view	m_ValueSymbol;	// view into EventSymbols
	char		*m_pszValue;
	bool		m_bDirty;
};

CGlobalEventLine::CGlobalEventLine( )
{
	m_pszValue = nullptr;
	m_bDirty = true;
}

CGlobalEventLine::~CGlobalEventLine( )
{
	Clear();
}

void CGlobalEventLine::Clear( )
{
	m_ValueSymbol = {};
	if ( m_pszValue != nullptr )
	{
		MemAlloc_Free( m_pszValue );
		m_pszValue = nullptr;
	}
	m_bDirty = true;
}

bool CGlobalEventLine::SetStaticText( const char *pszValue )
{
	if ( m_ValueSymbol.data() && m_ValueSymbol == pszValue )
	{
		return false;
	}

	Clear();

	m_ValueSymbol = EventSymbols.intern( pszValue );

	return true;
}

bool CGlobalEventLine::SetVaryingText( const char *pszValue )
{
	if ( m_pszValue && strcmpi( m_pszValue, pszValue ) == 0 )
	{	// no change
		return false;
	}

	Clear();

	m_pszValue = ( char * )MemAlloc_Alloc( std::strlen( pszValue ) + 1 );
	std::strcpy( m_pszValue, pszValue );

	return true;
}

void CGlobalEventLine::Write( ks::text_writer &buffer )
{
	const char *pszValue;

	if ( m_pszValue != nullptr )
	{
		pszValue = m_pszValue;
	}
	else
	{
		pszValue = ks::nz( m_ValueSymbol.data() );
	}

	if ( std::strchr( pszValue, ' ' ) != nullptr )
	{
		buffer.printf( "\"%s\"\n", pszValue );
	}
	else
	{
		buffer.printf( "%s\n", pszValue );
	}
}

void CGlobalEventLine::ClearDirty( )
{
	m_bDirty = false;
}



class CGlobalEvent
{
public:
	CGlobalEvent( const char *pszName, unsigned int nID, bool bIsHighLevel, CGlobalEvent *pParent = nullptr );

	bool			AddValue( bool bVarying, const char *pszKey, const char *pszValue );

	unsigned int	GetID( ) { return m_nID; }
	bool			IsDirty( ) { return m_bDirty; }

	void			Write( ks::text_writer &buffer );
	void			ClearDirty( );		

private:
	unsigned int								m_nID;
	std::string_view							m_Name;	// view into EventSymbols
	float										m_flTime;
	bool										m_bIsHighLevel;
	bool										m_bFullUpdate;
	bool										m_bDirty;
	CGlobalEvent								*m_pParent;
	// Keys view EventSymbols; lines are written in key order.
	std::map< std::string_view, std::unique_ptr< CGlobalEventLine >, std::less<> >	m_EventLines;
};


CGlobalEvent::CGlobalEvent( const char *pszName, unsigned int nID, bool bIsHighLevel, CGlobalEvent *pParent )
{
	m_Name = EventSymbols.intern( pszName );
	m_nID = nID;
	m_bIsHighLevel = bIsHighLevel;
	m_pParent = pParent;
	m_bFullUpdate = true;
	m_flTime = gpGlobals->curtime;
}

bool CGlobalEvent::AddValue( bool bVarying, const char *pszKey, const char *pszValue )
{
	std::string_view	KeyID;
	CGlobalEventLine	*pEvent;
	bool				bResult;

	KeyID = EventSymbols.intern( pszKey );
	std::unique_ptr< CGlobalEventLine > &pLine = m_EventLines[ KeyID ];
	if ( !pLine )
	{
		pLine = std::make_unique< CGlobalEventLine >();
	}
	pEvent = pLine.get();

	if ( bVarying )
	{
		bResult = pEvent->SetVaryingText( pszValue );
	}
	else
	{
		bResult = pEvent->SetStaticText( pszValue );
	}

	if ( bResult == true )
	{
		m_bDirty = true;
		m_flTime = gpGlobals->curtime;
	}

	return bResult;
}

void CGlobalEvent::Write( ks::text_writer &buffer )
{
	buffer.printf( "event %u\n", m_nID );
	buffer.printf( "{\n" );

	if ( m_bFullUpdate )
	{
		const char *pszName = m_Name.data();

		if ( std::strchr( pszName, ' ' ) != nullptr )
		{
			buffer.printf( "\tName\t\"%s\"\n", pszName );
		}
		else
		{
			buffer.printf( "\tName\t%s\n", pszName );
		}
		if ( m_pParent != nullptr )
		{
			buffer.printf( "\tParent_ID\t%u\n", m_pParent->GetID() );
		}
		if ( m_bIsHighLevel == true )
		{
			buffer.printf( "\tHighLevel\t1\n" );
		}
	}

	buffer.printf( "\tTime\t%g\n", m_flTime );

	for ( auto &[key, pLine] : m_EventLines )
	{
		if ( pLine->IsDirty() )
		{
			const char *pszKey = key.data();

			if ( std::strchr( pszKey, ' ' ) != nullptr )
			{
				buffer.printf( "\t\"%s\"\t", pszKey );
			}
			else
			{
				buffer.printf( "\t%s\t", pszKey );
			}
			pLine->Write( buffer );
		}
	}
	buffer.printf( "}\n" );
}

void CGlobalEvent::ClearDirty( )
{
	m_bFullUpdate = false;
	m_bDirty = false;
	for ( auto &[key, pLine] : m_EventLines )
	{
		pLine->ClearDirty();
	}
}


CGlobalEventLog::CGlobalEventLog( )
{
	m_nNextID = 1;
}

CGlobalEvent *CGlobalEventLog::GetGlobalEvent( EGlobalEvent GlobalEvent )
{
	return m_pGlobalEvents[ GlobalEvent ];
}

CGlobalEvent *CGlobalEventLog::CreateEvent( const char *pszName, bool bIsHighLevel, CGlobalEvent *pParent )
{
	CGlobalEvent	*pEvent = new CGlobalEvent( pszName, m_nNextID, bIsHighLevel, pParent );

	m_Events.push_back( pEvent );
	m_DirtyEvents.push_back( pEvent );
	m_nNextID++;

	return pEvent;
}

CGlobalEvent *CGlobalEventLog::CreateTempEvent( const char *pszName, CGlobalEvent *pParent )
{
	CGlobalEvent	*pEvent = new CGlobalEvent( pszName, m_nNextID, false, pParent );

	m_TempEvents.push_back( pEvent );
	m_DirtyEvents.push_back( pEvent );
	m_nNextID++;

	return pEvent;
}

void CGlobalEventLog::RemoveEvent( CGlobalEvent *pEvent )
{
	if ( ks::erase_first( m_Events, pEvent ) == true && ks::find_index( m_TempEvents, pEvent ) == -1 )
	{
		m_TempEvents.push_back( pEvent );
	}
}

void CGlobalEventLog::AddKeyValue( CGlobalEvent *pEvent, bool bVarying, const char *pszKey, const char *pszValueFormat, ... )
{
	ks::ScratchArena<> scratch;
	std::va_list		Args;
	int			nLen;
	char		*pszBuffer;
	bool		bResult;

	__builtin_va_start( Args, pszValueFormat );

	nLen = std::vsnprintf( nullptr, 0, pszValueFormat, Args ) + 1;
	pszBuffer = scratch.Alloc<char>( nLen );
	V_vsnprintf( pszBuffer, nLen, pszValueFormat, Args );
	
	bResult = pEvent->AddValue( bVarying, pszKey, pszBuffer );

	if ( bResult == true && ks::find_index( m_DirtyEvents, pEvent ) == -1 )
	{
		m_DirtyEvents.push_back( pEvent );
	}
}

void CGlobalEventLog::SendUpdate( )
{
	if ( m_DirtyEvents.empty() )
	{
		return;
	}

	if ( global_event_log_enabled.GetBool() == true )
	{
		FileHandle_t fh = g_pFullFileSystem->Open( "c:\\o.events", "a" );
		ks::text_writer buffer;

		for( int i = 0; i < std::ssize( m_DirtyEvents ); i++ )
		{
			m_DirtyEvents[ i ]->Write( buffer );

			g_pFullFileSystem->Write( buffer.c_str(), buffer.size(), fh );
			buffer.clear();
		}

		g_pFullFileSystem->Close( fh );
	}

	for( int i = 0; i < std::ssize( m_DirtyEvents ); i++ )
	{
		m_DirtyEvents[ i ]->ClearDirty();
	}
	m_DirtyEvents.clear(); m_DirtyEvents.shrink_to_fit();

	for( int i = 0; i < std::ssize( m_TempEvents ); i++ )
	{
		delete m_TempEvents[ i ];
	}
	m_TempEvents.clear(); m_TempEvents.shrink_to_fit();
}

void CGlobalEventLog::PostInit( )
{
	m_pGlobalEvents[ GLOBAL_EVENT_NPCS ] = CreateEvent( "NPCs", true );
}

void CGlobalEventLog::FrameUpdatePostEntityThink( )
{
	SendUpdate();
}
