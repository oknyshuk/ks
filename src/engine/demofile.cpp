// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.types;
import ks.platform;
import ks.simd;
import ks.tier1.bitbuf;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.keyvalues;
import ks.tier1.strtools;

#include "demofile.h"
#include "filesystem_engine.h"
#include "demo.h"
#include "demobuffer.h"
#include "host_cmd.h"

#include "cdll_int.h" // For playback parameters

void Host_EndGame (bool bShowMainMenu, const char *message, ...);

// Temporary debug stuff:
ConVar dbg_demofile( "dbg_demofile", "0" );
class CDbgPrint
{
public:
	static int s_nIndent;
	CDbgPrint( const char *pMsg )
	{
		++s_nIndent;
		if ( dbg_demofile.GetInt() )
		{
			for (int i = 0; i < 3*s_nIndent; ++i)
				DevMsg(" ");
			DevMsg( "%s", pMsg );
		}
	}
	~CDbgPrint() { --s_nIndent; }
};
int CDbgPrint::s_nIndent = 0;
#define DemoFileDbg(_txt) CDbgPrint printer( _txt )

ConVar demo_strict_validation( "demo_strict_validation", "0", FCVAR_RELEASE );

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDemoFile::CDemoFile() :
	m_pBuffer( nullptr )
{
}

CDemoFile::~CDemoFile()
{
	if ( IsOpen() )
	{
		Close();
	}
}

void CDemoFile::WriteSequenceInfo(int nSeqNrIn, int nSeqNrOut)
{
	DemoFileDbg( "WriteSequenceInfo()\n" );
	contract_assert( m_pBuffer && m_pBuffer->IsInitialized() );
	m_pBuffer->PutInt( nSeqNrIn );
	m_pBuffer->PutInt( nSeqNrOut );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoFile::ReadSequenceInfo(int &nSeqNrIn, int &nSeqNrOut)
{
	contract_assert( m_pBuffer && m_pBuffer->IsInitialized() );
	nSeqNrIn = m_pBuffer->GetInt( );
	nSeqNrOut = m_pBuffer->GetInt( );
}


void CDemoFile::WriteCmdInfo( democmdinfo_t& info )
{
	DemoFileDbg( "WriteCmdInfo()\n" );

	contract_assert( m_pBuffer && m_pBuffer->IsInitialized() );
	m_pBuffer->Put( &info, sizeof(democmdinfo_t) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoFile::ReadCmdInfo( democmdinfo_t& info )
{
	contract_assert( m_pBuffer && m_pBuffer->IsInitialized() );
	m_pBuffer->Get( &info, sizeof(democmdinfo_t) );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : cmd - 
//			*fp - 
//-----------------------------------------------------------------------------
void CDemoFile::WriteCmdHeader( unsigned char cmd, int tick, int nPlayerSlot )
{
	if ( dbg_demofile.GetInt() ) DevMsg( "----------------------------------------\n" );
	contract_assert( cmd >= dem_signon && cmd <= dem_lastcmd );

	contract_assert( m_pBuffer && m_pBuffer->IsInitialized() );
	m_pBuffer->PutUnsignedChar( cmd );
	m_pBuffer->WriteTick( tick );
	contract_assert( nPlayerSlot >= 0 &&
			nPlayerSlot < MAX_SPLITSCREEN_CLIENTS );
	m_pBuffer->PutChar( nPlayerSlot );

	const char *cmdname[] =
	{
		"dem_unknown",
		"dem_signon",
		"dem_packet",
		"dem_synctick",
		"dem_consolecmd",
		"dem_usercmd",
		"dem_datatables",
		"dem_stop",
		"dem_customdata",
		"dem_stringtables"
	};

	DemoFileDbg( "WriteCmdHeader()..." );
	if ( dbg_demofile.GetInt() ) DevMsg( "tick %i, cmd %s \n", tick, cmdname[cmd] );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : cmd - 
//			dt - 
//			frame - 
//-----------------------------------------------------------------------------
void CDemoFile::ReadCmdHeader( unsigned char& cmd, int& tick, int &nPlayerSlot )
{
	contract_assert( m_pBuffer && m_pBuffer->IsInitialized() );
	cmd = m_pBuffer->GetUnsignedChar( );
	if ( !m_pBuffer || !m_pBuffer->IsValid() )
	{
		ConDMsg("Missing end tag in demo file.\n");
		cmd = dem_stop;
		return;
	}

	if ( cmd <= 0 || cmd > dem_lastcmd )
	{
		ConDMsg("Unexepcted command token [%d] in .demo file\n", cmd );
		cmd = dem_stop;
		return;
	}

	tick = m_pBuffer->GetInt( );
	nPlayerSlot = (int)m_pBuffer->GetChar();
}

void CDemoFile::WriteConsoleCommand( const char *cmdstring, int tick, int nPlayerSlot )
{
	DemoFileDbg( "WriteConsoleCommand()\n" );
	if ( !cmdstring || !cmdstring[0] )
		return;

	if ( !m_pBuffer || !m_pBuffer->IsInitialized() )
		return;

	int len = V_strlen( cmdstring ) + 1;
	if ( len >= 1024 )
	{
		DevMsg("CDemoFile::WriteConsoleCommand: command too long (>1024).\n");
		return;
	}

	WriteCmdHeader( dem_consolecmd, tick, nPlayerSlot );

	WriteRawData( cmdstring, len );
}

const char *CDemoFile::ReadConsoleCommand()
{
	static char cmdstring[1024];
	
	ReadRawData( cmdstring, sizeof(cmdstring) );

	return cmdstring;
}

unsigned int CDemoFile::GetCurPos( bool bRead )
{
	if ( !m_pBuffer || !m_pBuffer->IsInitialized() )
		return 0;
	if ( bRead )
		return m_pBuffer->TellGet();
	return m_pBuffer->TellPut();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : buf - 
//-----------------------------------------------------------------------------
void CDemoFile::WriteNetworkDataTables( bf_write *buf, int tick )
{
	DemoFileDbg( "WriteNetworkDataTables()\n" );

	if ( !m_pBuffer || !m_pBuffer->IsInitialized() )
	{
		DevMsg("CDemoFile::WriteNetworkDataTables: Haven't opened file yet!\n" );
		return;
	}

	WriteCmdHeader( dem_datatables, tick, 0 );

	WriteRawData( (char*)buf->GetBasePointer(), buf->GetNumBytesWritten() );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : expected_length - 
//			&demofile - 
//-----------------------------------------------------------------------------
int CDemoFile::ReadNetworkDataTables( bf_read *buf )
{
	if ( buf )
		return ReadRawData( (char*)buf->GetBasePointer(), buf->GetNumBytesLeft() );
	return ReadRawData( nullptr, 0 ); // skip data
}

void CDemoFile::WriteStringTables( bf_write *buf, int tick )
{
	DemoFileDbg( "WriteStringTables()\n" );

	if ( !m_pBuffer || !m_pBuffer->IsInitialized() )
	{
		DevMsg("CDemoFile::WriteStringTables: Haven't opened file yet!\n" );
		return;
	}

	WriteCmdHeader( dem_stringtables, tick, 0 );

	WriteRawData( (char*)buf->GetBasePointer(), buf->GetNumBytesWritten() );
}

int CDemoFile::ReadStringTables( bf_read *buf )
{
	if ( buf )
		return ReadRawData( (char*)buf->GetBasePointer(), buf->GetNumBytesLeft() );
	return ReadRawData( nullptr, 0 ); // skip data
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : cmdnumber - 
//-----------------------------------------------------------------------------
void CDemoFile::WriteUserCmd( int cmdnumber, const char *buffer, unsigned char bytes, int tick, int nPlayerSlot )
{
	DemoFileDbg( "WriteUserCmd()\n" );
	if ( !m_pBuffer || !m_pBuffer->IsInitialized() )
		return;

	WriteCmdHeader( dem_usercmd, tick, nPlayerSlot );

	m_pBuffer->PutInt( cmdnumber );

	WriteRawData( buffer, bytes );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : discard - 
//-----------------------------------------------------------------------------
int CDemoFile::ReadUserCmd( char *buffer, int &size )
{
	int outgoing_sequence;
	
	contract_assert( m_pBuffer && m_pBuffer->IsInitialized() );
	outgoing_sequence = m_pBuffer->GetInt();

	size = ReadRawData( buffer, size );
	return outgoing_sequence;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoFile::WriteCustomData( int iCallbackIndex, const void *pData, std::size_t iDataSize, int tick )
{
	DemoFileDbg( "WriteCustomData()\n" );
	if ( !m_pBuffer || !m_pBuffer->IsInitialized() )
		return;

	WriteCmdHeader( dem_customdata, tick, 0 );

	//write the index of the callback according to the table we saved earlier
	m_pBuffer->PutInt( iCallbackIndex );


	contract_assert( m_pBuffer && m_pBuffer->IsInitialized() );
	m_pBuffer->PutInt( iDataSize );
	m_pBuffer->Put( pData, iDataSize );
}

//-----------------------------------------------------------------------------
// Purpose: Grabs the callback id and data for a custom data chunk
//-----------------------------------------------------------------------------
int CDemoFile::ReadCustomData( int *pCallbackIndex, uint8 **ppDataChunk )
{
	static std::vector< uint8 > s_TempMemoryBuffer;
	int size;
	int iCallbackIndex;

	contract_assert( m_pBuffer && m_pBuffer->IsInitialized() );
	iCallbackIndex = m_pBuffer->GetInt();
	size = m_pBuffer->GetInt();

	if( (pCallbackIndex == nullptr) || (ppDataChunk == nullptr) )
	{
		//skip the chunk
		m_pBuffer->SeekGet( false, size );
		return 0;
	}

	s_TempMemoryBuffer.assign( size, {} );
	*ppDataChunk = s_TempMemoryBuffer.data();
	*pCallbackIndex = iCallbackIndex;

	// read data into buffer
	m_pBuffer->Get( s_TempMemoryBuffer.data(), size );
	if ( !m_pBuffer || !m_pBuffer->IsValid() )
	{
		if ( demo_strict_validation.GetInt() )
			Host_EndGame(true, "Error reading demo message data.\n");
		return -1;
	}

	return size;
}

//-----------------------------------------------------------------------------
// Purpose: Rewind from the current spot by the time stamp, byte code and frame counter offsets
//-----------------------------------------------------------------------------
void CDemoFile::SeekTo( int position, bool bRead )
{
	contract_assert( m_pBuffer && m_pBuffer->IsInitialized() );
	if ( bRead )
	{
		m_pBuffer->SeekGet( true, position );
	}
	else
	{
		m_pBuffer->SeekPut( true, position );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
int CDemoFile::ReadRawData( char *buffer, int length )
{
	int size;
	
	contract_assert( m_pBuffer && m_pBuffer->IsInitialized() );
	size = m_pBuffer->GetInt();

	if ( buffer && (length < size) )
	{
		DevMsg("CDemoFile::ReadRawData: buffer overflow (%i).\n", size );
		return -1;
	}

	if ( !buffer )
	{
		// just skip it
		m_pBuffer->SeekGet( false, size );
		return size;
	}

	if ( length < size )
	{
		// given buffer is too small
		DevMsg("CDemoFile::ReadRawData: buffer overflow (%i).\n", size );
		m_pBuffer->SeekGet( false, size );
		return -1;
	}

	// read data into buffer
	m_pBuffer->Get( buffer, size );
	if ( !m_pBuffer || !m_pBuffer->IsValid() )
	{
		if ( demo_strict_validation.GetInt() )
			Host_EndGame( true, "Error reading demo message data.\n" );
		return -1;
	}

	return size;
}

void CDemoFile::WriteRawData( const char *buffer, int length )
{
	DemoFileDbg( "WriteRawData()\n" );

	contract_assert( m_pBuffer && m_pBuffer->IsInitialized() );
	m_pBuffer->PutInt( length );
	m_pBuffer->Put( buffer, length );
}

void CDemoFile::WriteDemoHeader()
{
	DemoFileDbg( "WriteDemoHeader()\n" );
	contract_assert( m_DemoHeader.networkprotocol == GetHostVersion() );

	DevMsg( "\n" );
	DevMsg( "     demofilestamp: %s\n", m_DemoHeader.demofilestamp );
	DevMsg( "     demoprotocol (should be %i): %i\n", DEMO_PROTOCOL, m_DemoHeader.demoprotocol );
	DevMsg( "     networkprotocol (should be %i): %i\n", GetHostVersion(), m_DemoHeader.networkprotocol );
	DevMsg( "     servername: %s\n", m_DemoHeader.servername );
	DevMsg( "     clientname: %s\n", m_DemoHeader.clientname );
	DevMsg( "     mapname: %s\n", m_DemoHeader.mapname );
	DevMsg( "     gamedirectory: %s\n", m_DemoHeader.gamedirectory );
	DevMsg( "     playback_time: %f\n", m_DemoHeader.playback_time );
	DevMsg( "     playback_ticks: %i\n", m_DemoHeader.playback_ticks );	
	DevMsg( "     playback_frames: %i\n", m_DemoHeader.playback_frames );
	DevMsg( "     signonlength: %i\n", m_DemoHeader.signonlength );
	DevMsg( "\n" );

	// Swaps endianness, goes to file start and writes header
	m_pBuffer->WriteHeader( &m_DemoHeader, sizeof(demoheader_t) );
}

demoheader_t *CDemoFile::ReadDemoHeader( CDemoPlaybackParameters_t const *pPlaybackParameters )
{
	bool bOk;
	V_memset( &m_DemoHeader, 0, sizeof(m_DemoHeader) );

	if ( !m_pBuffer || !m_pBuffer->IsInitialized() )
		return nullptr;
	m_pBuffer->SeekGet( true, pPlaybackParameters ? pPlaybackParameters->m_uiHeaderPrefixLength : 0 );
	m_pBuffer->Get( &m_DemoHeader, sizeof(demoheader_t) );
	bOk = m_pBuffer->IsValid();

	if ( !bOk )
		return nullptr;  // reading failed

	if ( V_strcmp( m_DemoHeader.demofilestamp, DEMO_HEADER_ID ) )
	{
		ConMsg( "%s has invalid demo header ID.\n", m_szFileName );
		return nullptr;
	}

	if ( ( m_DemoHeader.networkprotocol != GetHostVersion() ) &&
		( !pPlaybackParameters || !pPlaybackParameters->m_bAnonymousPlayerIdentity ) )
	{
		ConMsg ("WARNING: demo network protocol %i differs, engine version is %i \n", 
			m_DemoHeader.networkprotocol, GetHostVersion() );
	}

	if ( ( m_DemoHeader.demoprotocol > DEMO_PROTOCOL) ||
		 ( m_DemoHeader.demoprotocol < 2 ) )
	{
		ConMsg ("ERROR: demo file protocol %i outdated, engine version is %i \n", 
			m_DemoHeader.demoprotocol, DEMO_PROTOCOL );

		return nullptr;
	}

	return &m_DemoHeader;
}

void CDemoFile::WriteFileBytes( FileHandle_t fh, int length )
{
	DemoFileDbg( "WriteFileBytes()\n" );
	int   copysize = length;
	char  copybuf[COM_COPY_CHUNK_SIZE];

	while ( copysize > COM_COPY_CHUNK_SIZE )
	{
		g_pFileSystem->Read ( copybuf, COM_COPY_CHUNK_SIZE, fh );
		m_pBuffer->Put( copybuf, COM_COPY_CHUNK_SIZE );
		copysize -= COM_COPY_CHUNK_SIZE;
	}

	g_pFileSystem->Read ( copybuf, copysize, fh );
	m_pBuffer->Put( copybuf, copysize );
	
	g_pFileSystem->Flush ( fh );
}

float CDemoFile::GetTicksPerSecond()
{
	return m_DemoHeader.playback_ticks / m_DemoHeader.playback_time;
}

float CDemoFile::GetTicksPerFrame()
{
	return m_DemoHeader.playback_ticks / m_DemoHeader.playback_frames;
}

int CDemoFile::GetTotalTicks( void )
{
	return m_DemoHeader.playback_ticks;
}

bool CDemoFile::Open( const char *name, bool bReadOnly, bool bMemoryBuffer )
{
	if ( m_pBuffer && m_pBuffer->IsInitialized() )
	{
		ConMsg( "CDemoFile::Open: file already open.\n" );
		return false;
	}

	m_szFileName[0] = 0;  // clear name
	V_memset( &m_DemoHeader, 0, sizeof(m_DemoHeader) ); // and demo header

	if ( bMemoryBuffer )
	{
		contract_assert( !bReadOnly );	// Only read from files
		int const nMaxSize = 1024 * 1024 * 16;
		MemoryDemoBufferInitParams_t params( nMaxSize );
		m_pBuffer = CreateDemoBuffer( true, params );
	}
	else
	{
		StreamDemoBufferInitParams_t params( name, nullptr, bReadOnly, 0 );
		m_pBuffer = CreateDemoBuffer( false, params );
	}

	if ( !m_pBuffer || !m_pBuffer->IsInitialized() )
	{
		ConMsg( "CDemoFile::Open: couldn't open file %s for %s.\n", 
			name, bReadOnly ? "reading" : "writing" );
		Close();
		return false;
	}

	V_strncpy( m_szFileName, name, sizeof(m_szFileName) );
	return true;
}

bool CDemoFile::IsOpen()
{
	return m_pBuffer && m_pBuffer->IsInitialized();
}

void CDemoFile::Close()
{
	DestroyDemoBuffer( m_pBuffer );
	m_pBuffer = nullptr;
}

int CDemoFile::GetSize()
{
	return m_pBuffer->TellMaxPut();
}

void CDemoFile::DumpBufferToFile( char const* pFilename, const demoheader_t &header )
{
	// TODO: Fix this crash - when user tries to download before replay_record has been run.
	m_pBuffer->DumpToFile( pFilename, header );
}

void CDemoFile::UpdateStartTick( int& nStartTick )
{
	if ( m_pBuffer )
	{
		m_pBuffer->UpdateStartTick( nStartTick );
	}
}

void CDemoFile::NotifySignonComplete()
{
	m_pBuffer->NotifySignonComplete();
}

void CDemoFile::NotifyBeginFrame()
{
	m_pBuffer->NotifyBeginFrame();
}

void CDemoFile::NotifyEndFrame()
{
	m_pBuffer->NotifyEndFrame();
}
