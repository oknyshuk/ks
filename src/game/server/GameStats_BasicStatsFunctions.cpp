//====== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"

#include "GameStats.h"


void BasicGameStatsRecord_t::Clear()
{
	m_nCount = 0;
	m_nSeconds = 0;
	m_nCommentary = 0;
	m_nHDR = 0;
	m_nCaptions = 0;
	m_bSteam = true;
	m_bCyberCafe = false;
	Q_memset( m_nSkill, 0, sizeof( m_nSkill ) );
	m_nDeaths = 0;
}

void BasicGameStatsRecord_t::SaveToBuffer( ks::byte_writer &buf )
{
	buf.write< int >( m_nCount );
	buf.write< int >( m_nSeconds );
	buf.write< int >( m_nCommentary );
	buf.write< int >( m_nHDR );
	buf.write< int >( m_nCaptions );
	for ( int i = 0; i < 3; ++i )
	{
		buf.write< int >( m_nSkill[ i ] );
	}

	buf.write< char >( m_bSteam ? 1 : 0 );
	buf.write< char >( m_bCyberCafe ? 1 : 0 );
	buf.write< int >( m_nDeaths );
}

bool BasicGameStatsRecord_t::ParseFromBuffer( ks::byte_reader &buf, int iBufferStatsVersion )
{
	bool bret = true;
	m_nCount = buf.read< int >();

	if ( m_nCount > 100000 || m_nCount < 0 )
	{
		bret = false;
	}

	m_nSeconds = buf.read< int >();
	// Note, don't put the buf.read< int >() in the macro since it'll get evaluated twice!!!
	m_nSeconds = std::max( m_nSeconds, 0 );

	m_nCommentary = buf.read< int >();
	if ( m_nCommentary < 0 || m_nCommentary > 100000 )
	{
		bret = false;
	}

	m_nHDR = buf.read< int >();
	if ( m_nHDR < 0 || m_nHDR > 100000 )
	{
		bret = false;
	}

	m_nCaptions = buf.read< int >();
	if ( m_nCaptions < 0 || m_nCaptions > 100000 )
	{
		bret = false;
	}

	for ( int i = 0; i < 3; ++i )
	{
		m_nSkill[ i ] = buf.read< int >();
		if ( m_nSkill[ i ] < 0 || m_nSkill[ i ]  > 100000 )
		{
			bret = false;
		}
	}

	if ( iBufferStatsVersion > GAMESTATS_FILE_VERSION_OLD )
	{
		m_bSteam = buf.read< char >() ? true : false;
	}
	if ( iBufferStatsVersion > GAMESTATS_FILE_VERSION_OLD2 )
	{
		m_bCyberCafe = buf.read< char >() ? true : false;
	}
	if ( iBufferStatsVersion > GAMESTATS_FILE_VERSION_OLD5 )
	{
		m_nDeaths = buf.read< int >();
	}

	return bret;
}

void BasicGameStats_t::Clear()
{
	m_nSecondsToCompleteGame = 0;
	m_Summary.Clear();
	m_MapTotals.clear();
}

void BasicGameStats_t::SaveToBuffer( ks::byte_writer &buf )
{
	buf.write< int >( m_nSecondsToCompleteGame );

	m_Summary.SaveToBuffer( buf );

	int c = (int)m_MapTotals.size();
	buf.write< int >( c );
	for ( auto &[name, rec] : m_MapTotals )
	{
		buf.write_cstring( name );
		rec.SaveToBuffer( buf );
	}

	buf.write< char >( 0 );	
	buf.write< char >( m_bSteam ? 1 : 0 );
	buf.write< char >( m_bCyberCafe ? 1 : 0 );
	buf.write< short >( (short)m_nDXLevel );
}

BasicGameStatsRecord_t *BasicGameStats_t::FindOrAddRecordForMap( char const *mapname )
{
	return &m_MapTotals[ mapname ];
}

bool BasicGameStats_t::ParseFromBuffer( ks::byte_reader &buf, int iBufferStatsVersion )
{
	bool bret = true;

	m_nSecondsToCompleteGame = buf.read< int >();
	if ( m_nSecondsToCompleteGame < 0 || m_nSecondsToCompleteGame > 10000000 )
	{
		bret = false;
	}

	m_Summary.ParseFromBuffer( buf, iBufferStatsVersion );
	int c = buf.read< int >();
	if ( c > 1024 || c < 0 )
	{
		bret = false;
	}

	for ( int i = 0; i < c; ++i )
	{
		char mapname[ 256 ];
		buf.read_cstring( mapname, sizeof( mapname ) );

		BasicGameStatsRecord_t *rec = FindOrAddRecordForMap( mapname );
		bool valid= rec->ParseFromBuffer( buf, iBufferStatsVersion );
		if ( !valid )
		{
			bret = false;
		}
	}

	if ( iBufferStatsVersion >= GAMESTATS_FILE_VERSION_OLD2 )
	{
		buf.read< char >();		// Gets the obsolete hl2 unlocked chapter field
		m_bSteam = buf.read< char >() ? true : false;
	}
	if ( iBufferStatsVersion > GAMESTATS_FILE_VERSION_OLD2 )
	{
		m_bCyberCafe = buf.read< char >() ? true : false;
	}
	if ( iBufferStatsVersion > GAMESTATS_FILE_VERSION_OLD3 )
	{
		m_nDXLevel = (int)buf.read< short >();
	}
	return bret;
}
