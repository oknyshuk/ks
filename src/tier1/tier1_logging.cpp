// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: Tier1 logging helpers.

import std;
import ks.core.color;
import ks.core.logging;
import ks.core.types;
import ks.core.wchartypes;
import ks.platform;
import ks.simd;
import ks.tier1.byte_reader;
import ks.tier1.icvar;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.tier1_logging;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;

CBufferedLoggingListener::CBufferedLoggingListener() :
ILoggingListener( [this]( const LoggingContext_t *pContext, const tchar *pMessage ) { Log( pContext, pMessage ); } ),
m_StoredSpew( 512 )
{
}

void CBufferedLoggingListener::Log( const LoggingContext_t *pContext, const tchar *pMessage )
{
	m_StoredSpew.write< int >( pContext->m_ChannelID );
	m_StoredSpew.write< int >( pContext->m_Severity );
	m_StoredSpew.write< char >( pContext->m_Color.r() );
	m_StoredSpew.write< char >( pContext->m_Color.g() );
	m_StoredSpew.write< char >( pContext->m_Color.b() );
	m_StoredSpew.write< char >( pContext->m_Color.a() );
	m_StoredSpew.write_cstring( pMessage );
}

void CBufferedLoggingListener::EmitBufferedSpew()
{
	ks::byte_reader spew = m_StoredSpew.reader();
	while ( spew.remaining() > 0 )
	{
		LoggingChannelID_t channelID = spew.read< int >();
		LoggingSeverity_t severity = ( LoggingSeverity_t )spew.read< int >();
		unsigned char r, g, b, a;
		r = spew.read< char >();
		g = spew.read< char >();
		b = spew.read< char >();
		a = spew.read< char >();
		Color color( r, g, b, a );

		const std::string_view message = spew.read_cstring();
		LoggingSystem_LogDirect( channelID, severity, color, message.data() );
	}

	m_StoredSpew.clear();
}
