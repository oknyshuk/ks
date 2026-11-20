// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

#include "mm_netmgr.h"

#include "protocol.h"
import ks.tier1.bitbuf;
import ks.tier1.byte_reader;
import ks.tier1.byte_writer;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.netadr;
import ks.tier1.ns_address;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;

//
// Convars
//

ConVar net_allow_multicast( "net_allow_multicast", "1", FCVAR_RELEASE | FCVAR_ARCHIVE );
ConVar net_allow_syslink( "net_allow_syslink", "1", FCVAR_DEVELOPMENTONLY );


//
// CConnectionlessLanMgr
//

CConnectionlessLanMgr::CConnectionlessLanMgr()
{
	;
}

CConnectionlessLanMgr::~CConnectionlessLanMgr()
{
	;
}

static CConnectionlessLanMgr g_ConnectionlessLanMgr;
CConnectionlessLanMgr *g_pConnectionlessLanMgr = &g_ConnectionlessLanMgr;

//
// Implementation of CConnectionlessLanMgr
//

KeyValues * CConnectionlessLanMgr::UnpackPacket( netpacket_t *packet )
{
	if ( !packet )
		return nullptr;

	if ( !packet->size || !packet->data )
		return nullptr;

	// Try to unpack the data
	if ( packet->message.ReadLong() != 0 )
		return nullptr;
	if ( packet->message.ReadLong() != g_pMatchExtensions->GetINetSupport()->GetEngineBuildNumber() )
		return nullptr;

	int nDataLen = packet->message.ReadLong();

	m_buffer.assign( nDataLen, std::byte{} );
	packet->message.ReadBytes( m_buffer.data(), nDataLen );

	// Unpack key values
	KeyValues *pMsg = new KeyValues( "" );
	ks::byte_reader reader( m_buffer );
	if ( pMsg->ReadAsBinary( reader ) )
	{
		// Read binary data as well
		if ( int nBinarySize = packet->message.ReadLong() )
		{
			m_buffer.assign( nBinarySize, std::byte{} );
			packet->message.ReadBytes( m_buffer.data(), nBinarySize );

			// set the ptr to point to our buffer
			pMsg->SetPtr( "binary/ptr", m_buffer.data() );
		}

		return pMsg;	// "pMsg" is deleted by caller
	}
	
	pMsg->deleteThis();
	return nullptr;
}

bool CConnectionlessLanMgr::ProcessConnectionlessPacket( netpacket_t *packet )
{
	// Unpack key values
	KeyValues *pMsg = UnpackPacket( packet );
	if ( !pMsg )
		return false;
		
	KeyValues *notify = new KeyValues( "OnNetLanConnectionlessPacket" );
	notify->SetString( "from", ns_address_render( packet->from ).String() );
	notify->AddSubKey( pMsg );
	
	g_pMatchEventsSubscription->BroadcastEvent( notify );
	return true;	// "pMsg" is deleted as child of "notify"
}

void CConnectionlessLanMgr::Update()
{
}

void CConnectionlessLanMgr::SendPacket( KeyValues *pMsg, char const *szAddress /*= NULL*/, INetSupport::NetworkSocket_t eSock )
{
	char buf[ INetSupport::NC_MAX_ROUTABLE_PAYLOAD ];
	bf_write msg( buf, sizeof( buf ) );

	msg.WriteLong( CONNECTIONLESS_HEADER );
	msg.WriteLong( 0 );
	msg.WriteLong( g_pMatchExtensions->GetINetSupport()->GetEngineBuildNumber() );

	ks::byte_writer data;
	pMsg->WriteAsBinary( data );

	msg.WriteLong( data.size() );
	msg.WriteBytes( data.data(), data.size() );
	contract_assert( !msg.IsOverflowed() );

	// Special case when encoding binary data
	KeyValues *kvPtr = pMsg->FindKey( "binary/ptr" );
	KeyValues *kvSize = pMsg->FindKey( "binary/size" );
	if ( kvPtr && kvSize )
	{
		void *pvData = kvPtr->GetPtr();
		int nSize = kvSize->GetInt();

		if ( pvData && nSize )
		{
			msg.WriteLong( nSize );
			if ( !msg.WriteBytes( pvData, nSize ) )
			{
				contract_assert( 0 );
				return;
			}
		}
		else
		{
			msg.WriteLong( 0 );
		}
	}
	else
	{
		msg.WriteLong( 0 );
	}
	contract_assert( !msg.IsOverflowed() );

	// Prepare the address
	netadr_t inetAddr;
	if ( szAddress )
	{
		if ( szAddress[0] == '*' && szAddress[1] == ':' )
		{
			inetAddr.SetType( NA_BROADCAST );
			inetAddr.SetPort( std::atoi( szAddress + 2 ) );
		}
		else
		{
			inetAddr.SetFromString( szAddress );
		}
	}
	else
	{
		inetAddr.SetType( NA_BROADCAST );
		inetAddr.SetPort( 0 );
	}

	// Check if broadcasts are allowed
	if ( inetAddr.GetType() == NA_BROADCAST &&
		!net_allow_multicast.GetBool() )
		return;

	// Sending the connectionless packet
	g_pMatchExtensions->GetINetSupport()->SendPacket( nullptr, eSock,
		inetAddr, msg.GetData(), msg.GetNumBytesWritten() );
}
