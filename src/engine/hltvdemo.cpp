// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include <eiface.h>
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.memalloc;
import ks.core.platform;
import ks.core.random;
import ks.core.wchartypes;
import ks.platform;
import ks.simd;
import ks.tier1.bitbuf;
import ks.tier1.iconvar;
import ks.tier1.strtools;
import std;
#include "hltvdemo.h"
#include "hltvserver.h"
#include "demo.h"
#include "host_cmd.h"
#include "demofile/demoformat.h"
#include "net.h"
#include "networkstringtable.h"
#include "dt_common_eng.h"
#include "host.h"
#include "server.h"


extern CNetworkStringTableContainer *networkStringTableContainerServer;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CHLTVDemoRecorder::CHLTVDemoRecorder(CHLTVServer *pHltvServer) :IDemoRecorder( recorder_kind_t::hltv ), hltv( pHltvServer )
{
	m_bIsRecording = false;
}

CHLTVDemoRecorder::~CHLTVDemoRecorder()
{
	StopRecording();
}

void CHLTVDemoRecorder::StartAutoRecording() 
{
	char fileName[MAX_OSPATH];

	struct tm today;
	Plat_GetLocalTime( &today );

	V_snprintf( fileName, sizeof(fileName), "%sauto%d-%04i%02i%02i-%02i%02i%02i-%u-%s-%s.dem",
		( serverGameDLL && serverGameDLL->IsValveDS() ) ? "replays/" : "",
		hltv->GetInstanceIndex(),
		1900 + today.tm_year, today.tm_mon + 1, today.tm_mday,
		today.tm_hour, today.tm_min, today.tm_sec,
		RandomInt( 1, std::numeric_limits<int>::max() - 1 ),
		hltv->GetMapName(), host_name.GetString() ); 

	// replace any bad characters that might be in the host name
	int nLen = V_strlen( fileName );
	for ( int i = 9; i < nLen; i++ )
	{
		if ( fileName[i] >= 'a' && fileName[i] <= 'z' )
			continue;
		if ( fileName[i] >= 'A' && fileName[i] <= 'Z' )
			continue;
		if ( fileName[i] >= '0' && fileName[i] <= '9' )
			continue;
		if ( fileName[i] == '_' || fileName[i] == '-' || fileName[i] == '.' )
			continue;
		fileName[i] = '_';
	}

	StartRecording( fileName, false );
}


void CHLTVDemoRecorder::StartRecording( const char *filename, bool bContinuously ) 
{
	StopRecording();	// stop if we're already recording
	
	if ( !m_DemoFile.Open( filename, false ) )
	{
		ConMsg ("StartRecording: couldn't open demo file %s.\n", filename );
		return;
	}

	ConMsg ("Recording GOTV demo to %s...\n", filename);

	demoheader_t *dh = &m_DemoFile.m_DemoHeader;

	// open demo header file containing sigondata
	V_memset( dh, 0, sizeof(demoheader_t));

	V_strncpy( dh->demofilestamp, DEMO_HEADER_ID, sizeof(dh->demofilestamp) );
	dh->demoprotocol = DEMO_PROTOCOL;
	dh->networkprotocol = GetHostVersion();

	V_strncpy( dh->mapname, hltv->GetMapName(), sizeof( dh->mapname ) );

	char szGameDir[MAX_OSPATH];
	V_strncpy(szGameDir, com_gamedir, sizeof( szGameDir ) );
	V_FileBase ( szGameDir, dh->gamedirectory, sizeof( dh->gamedirectory ) );

	V_strncpy( dh->servername, host_name.GetString(), sizeof( dh->servername ) );

	V_strncpy( dh->clientname, "GOTV Demo", sizeof( dh->servername ) );

	// Write demo file header info.  This is mostly to allocate space in the output file,
	// the final demo header will be written when we finish recording.
	m_DemoFile.WriteDemoHeader();

	// We keep these identical so that GetRecordingTick() returns 0 until at least the first
	// frame is recorded.
	//
	// -1 here is really a sentinel value so that we can detect when the first tick is written
	// and do some bookkeeping at a safe point there.  See HasRecordingActuallyStarted()
	m_nStartTick = -1;
	m_nLastWrittenTick = -1;

	m_bIsRecording = true;
	m_nFrameCount = 0;
	m_SequenceInfo = 1;
	m_nDeltaTick = -1;
}

bool CHLTVDemoRecorder::IsRecording()
{
	return m_bIsRecording;
}

void CHLTVDemoRecorder::StopRecording( const CGameInfo *pGameInfo )
{
	if ( !m_bIsRecording )
		return;

	// Demo playback should read this as an incoming message.
	m_DemoFile.WriteCmdHeader( dem_stop, GetRecordingTick(), 0 );

	// update demo header info
	m_DemoFile.m_DemoHeader.playback_ticks = GetRecordingTick();
	m_DemoFile.m_DemoHeader.playback_time =  host_state.interval_per_tick *	GetRecordingTick();
	m_DemoFile.m_DemoHeader.playback_frames = m_nFrameCount;

	// write updated version
	m_DemoFile.WriteDemoHeader();

	m_DemoFile.Close();

	m_bIsRecording = false;

	// clear writing data buffer
	if ( m_MessageData.GetBasePointer() )
	{
		delete [] m_MessageData.GetBasePointer();
		m_MessageData.StartWriting( nullptr, 0 );
	}
	
	ConMsg("Completed GOTV demo \"%s\", recording time %.1f\n",
		m_DemoFile.m_szFileName,
		m_DemoFile.m_DemoHeader.playback_time );
}

int CHLTVDemoRecorder::GetRecordingTick( void )
{
	return m_nLastWrittenTick - m_nStartTick;
}

void CHLTVDemoRecorder::WriteServerInfo()
{

	byte		buffer[ NET_MAX_PAYLOAD ];
	bf_write	msg( "CHLTVDemoRecorder::WriteServerInfo", buffer, sizeof( buffer ) );

	// on the master demos are using sv object, on relays hltv
	CBaseServer *pServer = hltv->IsMasterProxy()?(CBaseServer*)(&sv):(CBaseServer*)(hltv);

	CSVCMsg_ServerInfo_t serverinfo;
	hltv->FillServerInfo( serverinfo ); // fill rest of info message
	serverinfo.WriteToBuffer( msg );

	// send first tick
	CNETMsg_Tick_t signonTick( m_nStartTick, 0, 0, 0 );
	signonTick.WriteToBuffer( msg );

	// Write replicated ConVars to non-listen server clients only
	CNETMsg_SetConVar_t convars;
	
	// build a list of all replicated convars
	Host_BuildConVarUpdateMessage( &convars.convars.mut(), FCVAR_REPLICATED, true );
	if ( hltv->IsMasterProxy() )
	{
		// for SourceTV server demos write set "tv_transmitall 1" even
		// if it's off for the real broadcast
		convars.AddToTail( "tv_transmitall", "1" );
		hltv->FixupConvars( convars );
	}

	// write convars to demo
	convars.WriteToBuffer( msg );

	// write stringtable baselines
	hltv->m_StringTables->WriteBaselines( pServer->GetMapName(), msg );

	// send signon state
	CNETMsg_SignonState_t signonMsg( SIGNONSTATE_NEW, pServer->GetSpawnCount() );
	signonMsg.WriteToBuffer( msg );

	WriteMessages( dem_signon, msg );
}

void CHLTVDemoRecorder::RecordPlayerAvatar( const CNETMsg_PlayerAvatarData_t* hltvPlayerAvatar )
{
	// If we haven't actually 'signed on' yet, don't record; we'll capture this in our signon data
	if ( !HasRecordingActuallyStarted() )
		return;

	// If we are already recording then append this player's avatar data into the demo
	byte		buffer[NET_MAX_PAYLOAD];
	bf_write	bfWrite( "CHLTVDemo::NETMsg_PlayerAvatarData", buffer, sizeof( buffer ) );
	hltvPlayerAvatar->WriteToBuffer( bfWrite );
	WriteMessages( dem_signon, bfWrite );
}

void CHLTVDemoRecorder::RecordCommand( const char *cmdstring )
{
	if ( !HasRecordingActuallyStarted() )
		return;

	if ( !cmdstring || !cmdstring[0] )
		return;

	m_DemoFile.WriteConsoleCommand( cmdstring, GetRecordingTick(), 0 );
}

void CHLTVDemoRecorder::RecordServerClasses( ServerClass *pClasses )
{
	// We should be in the middle of recording the sign-on state at this point
	// which means we will have a valid nStartTick.
	//
	// This assert is to make sure that this doesn't randomly get called via
	// IDemoRecorder; in that case we should probably just return.

	// to big for stack
	std::vector< std::byte > bigBuff( DEMO_RECORD_BUFFER_SIZE );
	bf_write buf( bigBuff.data(), DEMO_RECORD_BUFFER_SIZE );

	// Send SendTable info.
	DataTable_WriteSendTablesBuffer( pClasses, &buf );

	// Send class descriptions.
	DataTable_WriteClassInfosBuffer( pClasses, &buf );

	if ( buf.GetNumBitsLeft() <= 0 )
	{
		Sys_Error( "unable to record server classes\n" );
	}

	// Now write the buffer into the demo file
	m_DemoFile.WriteNetworkDataTables( &buf, GetRecordingTick() );
}

void CHLTVDemoRecorder::RecordCustomData( int iCallbackIndex, const void *pData, std::size_t iDataLength )
{
	if ( !HasRecordingActuallyStarted() )
		return;

	m_DemoFile.WriteCustomData( iCallbackIndex, pData, iDataLength, GetRecordingTick() );
}

void CHLTVDemoRecorder::RecordStringTables()
{

	void *data = MemAlloc_Alloc( DEMO_RECORD_BUFFER_SIZE );
	bf_write buf( data, DEMO_RECORD_BUFFER_SIZE );

	networkStringTableContainerServer->WriteStringTables( buf );

	// Now write the buffer into the demo file
	m_DemoFile.WriteStringTables( &buf, GetRecordingTick() );

	g_pMemAlloc->Free( data );
}

int CHLTVDemoRecorder::WriteSignonData()
{

	int start = m_DemoFile.GetCurPos( false );

	// on the master demos are using sv object, on relays hltv
	CBaseServer *pServer = hltv->IsMasterProxy()?(CBaseServer*)(&sv):(CBaseServer*)(hltv);

	WriteServerInfo();

	RecordServerClasses( serverGameDLL->GetAllServerClasses() );
	RecordStringTables();

	byte		buffer[ NET_MAX_PAYLOAD ];
	bf_write	msg( "CHLTVDemo::WriteSignonData", buffer, sizeof( buffer ) );

	// use your class infos, CRC is correct
	// use your class infos, CRC is correct
	CSVCMsg_ClassInfo_t classmsg;
	classmsg.create_on_client = true;
	classmsg.WriteToBuffer( msg );

	// Write the regular signon now
	msg.WriteBits( hltv->m_Signon.GetData(), hltv->m_Signon.GetNumBitsWritten() );

	// write new state
	CNETMsg_SignonState_t signonMsg1( SIGNONSTATE_PRESPAWN, pServer->GetSpawnCount() ); 
	signonMsg1.WriteToBuffer( msg );

	WriteMessages( dem_signon, msg ); 
	msg.Reset();

	// Dump all accumulated avatar data messages into signon portion of the demo file
	for ( auto &[accountid, pData] : hltv->m_mapPlayerAvatarData )
	{
		CNETMsg_PlayerAvatarData_t &msgPlayerAvatarData = *pData;
		msgPlayerAvatarData.WriteToBuffer( msg );
		WriteMessages( dem_signon, msg );
		msg.Reset();
	}

	// set view entity
	// set view entity
	CSVCMsg_SetView_t viewent;
	viewent.entity_index = hltv->m_nViewEntity;
	viewent.WriteToBuffer( msg );

	// Spawned into server, not fully active, though
	CNETMsg_SignonState_t signonMsg2( SIGNONSTATE_SPAWN, pServer->GetSpawnCount() );
	signonMsg2.WriteToBuffer( msg );

	WriteMessages( dem_signon, msg ); 
	
	return m_DemoFile.GetCurPos( false ) - start;
}


void CHLTVDemoRecorder::WriteFrame( CHLTVFrame *pFrame, bf_write *additionaldata )
{
	contract_assert( hltv->IsMasterProxy() ); // this works only on the master since we use sv.
	contract_assert( pFrame->tick_count >= 0 );

	byte		buffer[ NET_MAX_PAYLOAD ];
	bf_write	msg( "CHLTVDemo::RecordFrame", buffer, sizeof( buffer ) );

	// Handle first frame of demo recording here
	if ( m_nStartTick == -1 )
	{
		// Synchronize start tick with frame.
		//
		// THIS IS THE POINT WHERE HasRecordingActuallyStarted() STARTS TO RETURN "true"!
		//
		// WriteSignonData() relies on this being the correct value for the server, matching
		// the string tables timestamp in the HLTV server
		m_nStartTick = pFrame->tick_count;

		// Write out the current state of string tables, etc.
		m_DemoFile.m_DemoHeader.signonlength = WriteSignonData();

		// Demo playback should read this as an incoming message.
		// Write the client's realtime value out so we can synchronize the reads.
		m_DemoFile.WriteCmdHeader( dem_synctick, 0, 0 );
	}

	m_nLastWrittenTick = pFrame->tick_count;

 	//first write reliable data
	bf_write *data = &pFrame->m_Messages[HLTV_BUFFER_RELIABLE];
	if ( data->GetNumBitsWritten() )
		msg.WriteBits( data->GetBasePointer(), data->GetNumBitsWritten() );

	//now send snapshot data

	// send tick time
	CNETMsg_Tick_t tickmsg( pFrame->tick_count, host_frameendtime_computationduration, host_frametime_stddeviation, host_framestarttime_stddeviation );
	tickmsg.WriteToBuffer( msg );


	// Update shared client/server string tables. Must be done before sending entities
	hltv->m_StringTables->WriteUpdateMessage( nullptr, std::max( m_nStartTick, m_nDeltaTick ), msg );

	// get delta frame
	CClientFrame *deltaFrame = hltv->GetClientFrame( m_nDeltaTick ); // NULL if delta_tick is not found or -1
	
	// send entity update, delta compressed if deltaFrame != NULL
	CSVCMsg_PacketEntities_t packetmsg;
	sv.WriteDeltaEntities( hltv->m_MasterClient, pFrame, deltaFrame, packetmsg );
	packetmsg.WriteToBuffer( msg );

	// send all unreliable temp ents between last and current frame
	CSVCMsg_TempEntities_t tempentsmsg;
	CFrameSnapshot * fromSnapshot = deltaFrame?deltaFrame->GetSnapshot():nullptr;
	sv.WriteTempEntities( hltv->m_MasterClient, pFrame->GetSnapshot(), fromSnapshot, tempentsmsg, 255 );
	if ( tempentsmsg.num_entries )
	{
		tempentsmsg.WriteToBuffer( msg );
	}

	// write sound data
	data = &pFrame->m_Messages[HLTV_BUFFER_SOUNDS];
	if ( data->GetNumBitsWritten() )
		msg.WriteBits( data->GetBasePointer(), data->GetNumBitsWritten()  );

	// write voice data
	data = &pFrame->m_Messages[HLTV_BUFFER_VOICE];
	if ( data->GetNumBitsWritten() )
		msg.WriteBits( data->GetBasePointer(), data->GetNumBitsWritten()  );

	// last write unreliable data
	data = &pFrame->m_Messages[HLTV_BUFFER_UNRELIABLE];
	if ( data->GetNumBitsWritten() )
		msg.WriteBits( data->GetBasePointer(), data->GetNumBitsWritten()  );

	if ( additionaldata && additionaldata->GetNumBitsWritten() )
	{
		msg.WriteBits( additionaldata->GetBasePointer(), additionaldata->GetNumBitsWritten() );
	}

	// update delta tick just like fakeclients do
	m_nDeltaTick = pFrame->tick_count;

	// write packet to demo file
	WriteMessages( dem_packet, msg ); 
}

void CHLTVDemoRecorder::WriteMessages( unsigned char cmd, bf_write &message )
{

	int len = message.GetNumBytesWritten();

	if (len <= 0)
		return;

	// fill last bits in last byte with NOP if necessary
	int nRemainingBits = message.GetNumBitsWritten() % 8;
	if ( nRemainingBits > 0 &&  nRemainingBits <= (8-NETMSG_TYPE_BITS) )
	{
		CNETMsg_NOP_t nop;
		nop.WriteToBuffer( message );
	}

	contract_assert( len < NET_MAX_MESSAGE );

	// if signondata read as fast as possible, no rewind
	// and wait for packet time
	// byte cmd = (m_pDemoFileHeader != NULL)  ? dem_signon : dem_packet;

	if ( cmd == dem_packet )
	{
		m_nFrameCount++;
	}

	// write command & time
	m_DemoFile.WriteCmdHeader( cmd, GetRecordingTick(), 0 ); 
	
	// write NULL democmdinfo just to keep same format as client demos
	democmdinfo_t info;
	V_memset( &info, 0, sizeof( info ) );
	m_DemoFile.WriteCmdInfo( info );

	// write continously increasing sequence numbers
	m_DemoFile.WriteSequenceInfo( m_SequenceInfo, m_SequenceInfo );
	m_SequenceInfo++;
	
	// Output the buffer.  Skip the network packet stuff.
	m_DemoFile.WriteRawData( (char*)message.GetBasePointer(), len );
	
	if ( tv_debug.GetInt() > 1 )
	{
		Msg( "Writing GOTV demo message %i bytes at file pos %i\n", len, m_DemoFile.GetCurPos( false ) );
	}
}

void CHLTVDemoRecorder::RecordMessages(bf_read &data, int bits)
{
	if ( !HasRecordingActuallyStarted() )
		return;

	// create buffer if not there yet
	if ( m_MessageData.GetBasePointer() == nullptr )
	{
		m_MessageData.StartWriting( new unsigned char[NET_MAX_PAYLOAD], NET_MAX_PAYLOAD );
	}
	
	if ( bits>0 )
	{
		m_MessageData.WriteBitsFromBuffer( &data, bits );
		contract_assert( !m_MessageData.IsOverflowed() );
	}
}

void CHLTVDemoRecorder::RecordPacket()
{
	if ( !HasRecordingActuallyStarted() )
		return;

	if( m_MessageData.GetBasePointer() )
	{
		WriteMessages( dem_packet, m_MessageData );
		m_MessageData.Reset(); // clear message buffer
	}
}
