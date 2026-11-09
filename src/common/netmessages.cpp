// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "netmessages.h"
#include "const.h"
#include "../engine/net_chan.h"
import std;
#include "../engine/audio/public/sound.h"
#include "../engine/event_system.h"
#include "../engine/dt.h"
#include "mathlib/IceKey.H"

static char s_text[1024];


const char *g_MostCommonPathIDs[] =
{
	"GAME",
	"MOD"
};

const char *g_MostCommonPrefixes[] =
{
	"materials",
	"models",
	"sounds",
	"scripts"
};

static int FindCommonPathID( const char *pPathID )
{
	for ( int i = 0; i < std::size( g_MostCommonPathIDs ); i++ )
	{
		if ( V_stricmp( pPathID, g_MostCommonPathIDs[i] ) == 0 )
			return i;
	}
	return -1;
}

static int FindCommonPrefix( const char *pStr )
{
	for ( int i = 0; i < std::size( g_MostCommonPrefixes ); i++ )
	{
		if ( V_stristr( pStr, g_MostCommonPrefixes[i] ) == pStr  )
		{
			int iNextChar = V_strlen( g_MostCommonPrefixes[i] );
			if ( pStr[iNextChar] == '/' || pStr[iNextChar] == '\\' )
				return i;
		}
	}
	return -1;
}

void CCLCMsg_FileCRCCheck_t::SetPath( ks::net::CCLCMsg_FileCRCCheck& msg, const char *path )
{
	int iCode = FindCommonPathID( path );
	if ( iCode == -1 )
	{
		msg.code_path = -1;
		msg.path = path;
	}
	else
	{
		msg.code_path = iCode;
	}
}

const char *CCLCMsg_FileCRCCheck_t::GetPath( const ks::net::CCLCMsg_FileCRCCheck& msg )
{
	int iCode = msg.code_path;
	if( ( iCode >= 0 ) && ( iCode < std::size( g_MostCommonPathIDs ) ) )
	{
		return g_MostCommonPathIDs[ iCode ];
	}

	contract_assert( msg.code_path == -1 );
	return msg.path->c_str();
}

void CCLCMsg_FileCRCCheck_t::SetFileName( ks::net::CCLCMsg_FileCRCCheck& msg, const char *fileName )
{
	int iCode = FindCommonPrefix( fileName );
	if ( iCode == -1 )
	{
		msg.code_filename = -1;
		msg.filename = fileName;
	}
	else
	{
		msg.code_filename = iCode;
		msg.filename = &fileName[ V_strlen( g_MostCommonPrefixes[ iCode ] ) + 1 ];
	}
}

const char *CCLCMsg_FileCRCCheck_t::GetFileName( const ks::net::CCLCMsg_FileCRCCheck& msg )
{
	int iCode = msg.code_filename;
	if( ( iCode >= 0 ) && ( iCode < std::size( g_MostCommonPrefixes ) ) )
	{
		return va( "%s%c%s", g_MostCommonPrefixes[ iCode ], CORRECT_PATH_SEPARATOR, msg.filename->c_str() );
	}

	contract_assert( msg.code_filename == -1 );
	return msg.filename->c_str();
}

void CmdKeyValuesHelper::CLCMsg_SetKeyValues( ks::net::CCLCMsg_CmdKeyValues& msg, const KeyValues *keyValues )
{
	ks::byte_writer bufData;
	keyValues->WriteAsBinary( bufData );
	msg.keyvalues = std::string( ks::text_view( bufData.bytes() ) );
}

KeyValues *CmdKeyValuesHelper::CLCMsg_GetKeyValues ( const ks::net::CCLCMsg_CmdKeyValues& msg )
{
	KeyValues *pKeyValues = new KeyValues( "" );

	const std::string& msgStr = msg.keyvalues;

	ks::byte_reader bufRead( msgStr.data(), msgStr.size() );
	if ( !pKeyValues->ReadAsBinary( bufRead, 90 ) )	// we are expecting very few nest levels of keyvalues here!
	{
		contract_assert( false );
	}

	return pKeyValues;
}

void CmdKeyValuesHelper::SVCMsg_SetKeyValues( ks::net::CSVCMsg_CmdKeyValues& msg, const KeyValues *keyValues )
{
	ks::byte_writer bufData;
	keyValues->WriteAsBinary( bufData );
	msg.keyvalues = std::string( ks::text_view( bufData.bytes() ) );
}

KeyValues *CmdKeyValuesHelper::SVCMsg_GetKeyValues ( const ks::net::CSVCMsg_CmdKeyValues& msg )
{
	KeyValues *pKeyValues = new KeyValues( "" );

	const std::string& msgStr = msg.keyvalues;

	ks::byte_reader bufRead( msgStr.data(), msgStr.size() );
	if ( !pKeyValues->ReadAsBinary( bufRead ) )
	{
		contract_assert( false );
	}

	return pKeyValues;
}

/*
bool CmdEncryptedDataMessageCodec::SVCMsg_EncryptedData_EncryptMessage( CSVCMsg_EncryptedData_t &msgEncryptedResult, const ::google::protobuf::Message &msgPlaintextInput )
{
	static char const *szEncryptedTag = "[[ENCRYPTED_DATA]]";
	static size_t nEncryptedLen = V_strlen( szEncryptedTag );
	int32 const numBytesWritten = int32( ks::proto::byte_size( msgPlaintextInput ) );
	msgEncryptedResult.encrypted.mut().resize( nEncryptedLen + sizeof( int32 ) + numBytesWritten );

	V_memcpy( &msgEncryptedResult.encrypted.mut().at(0), szEncryptedTag, nEncryptedLen );
	int32 const numBytesWrittenWire = DWordSwapC( numBytesWritten );	// byteswap for the wire
	V_memcpy( &msgEncryptedResult.encrypted.mut().at( nEncryptedLen ), &numBytesWrittenWire, sizeof( numBytesWrittenWire ) );
	return ks::proto::write( msgPlaintextInput,
		( std::byte * ) &msgEncryptedResult.encrypted.mut().at( nEncryptedLen + sizeof( int32 ) ) );
}
*/

bool CmdEncryptedDataMessageCodec::SVCMsg_EncryptedData_EncryptMessage( CSVCMsg_EncryptedData_t &msgEncryptedResult, INetMessage *pMsgPlaintextInput, char const *key )
{
	ks::ScratchArena<> arena;
	// Prepare encryption key
	IceKey iceKey( 2 );
	if ( iceKey.keySize() != V_strlen( key ) )
	{
		Warning( "CmdEncryptedDataMessageCodec key size is %d, but %d is expected!\n", V_strlen( key ), iceKey.keySize() );
		return false; // we cannot encrypt with the supplied key
	}
	iceKey.set( ( const unsigned char * ) key );

	// supporting smaller stack
	net_scratchbuffer_t scratch;
	bf_write msg( "SVCMsg_EncryptedData_EncryptMessage", scratch.GetBuffer(), scratch.Size() );
	if ( !pMsgPlaintextInput->WriteToBuffer( msg ) )
		return false;
	int32 const numBytesWritten = msg.GetNumBytesWritten();
	
	// Generate some random fudge, ICE operates on 64-bit blocks, so make sure our total size is a multiple of 8 bytes
	int numRandomFudgeBytes = RandomInt( 16, 72 );
	int numTotalEncryptedBytes = 1 + numRandomFudgeBytes + sizeof( int32 ) + numBytesWritten;
	numRandomFudgeBytes += iceKey.blockSize() - ( numTotalEncryptedBytes % iceKey.blockSize() );
	numTotalEncryptedBytes = 1 + numRandomFudgeBytes + sizeof( int32 ) + numBytesWritten;

	char *pchRandomFudgeBytes = ( char * ) arena.AllocBytes( numRandomFudgeBytes );
	for ( int k = 0; k < numRandomFudgeBytes; ++ k )
		pchRandomFudgeBytes[k] = RandomInt( 16, 250 );
	
	msgEncryptedResult.encrypted.mut().resize( numTotalEncryptedBytes );

	msgEncryptedResult.encrypted.mut().at(0) = numRandomFudgeBytes;
	V_memcpy( &msgEncryptedResult.encrypted.mut().at(1), pchRandomFudgeBytes, numRandomFudgeBytes );
	
	int32 const numBytesWrittenWire = DWordSwapC( numBytesWritten );	// byteswap for the wire
	V_memcpy( &msgEncryptedResult.encrypted.mut().at( 1 + numRandomFudgeBytes ), &numBytesWrittenWire, sizeof( numBytesWrittenWire ) );
	V_memcpy( &msgEncryptedResult.encrypted.mut().at( 1 + numRandomFudgeBytes + sizeof( int32 ) ), msg.GetBasePointer(), numBytesWritten );

	// Encrypt the message
	unsigned char *pchCryptoBuffer = ( unsigned char * ) arena.AllocBytes( iceKey.blockSize() );
	for ( int k = 0; k < numTotalEncryptedBytes; k += iceKey.blockSize() )
	{
		iceKey.encrypt( ( const unsigned char * ) &msgEncryptedResult.encrypted.mut().at(k), pchCryptoBuffer );
		V_memcpy( &msgEncryptedResult.encrypted.mut().at(k), pchCryptoBuffer, iceKey.blockSize() );
	}
	return true;
}

bool CmdEncryptedDataMessageCodec::SVCMsg_EncryptedData_Process( ks::net::CSVCMsg_EncryptedData const &msgEncryptedInput, INetChannel *pProcessingChannelInterface, char const *key )
{
	ks::ScratchArena<> arena;
	CNetChan *pProcessingChannel = ( CNetChan * ) pProcessingChannelInterface;
	if ( !pProcessingChannel )
		return false;

	if ( !msgEncryptedInput.encrypted.has_value() )
		return true;

	if ( !key || !*key )
		return true; // key is not supplied, so ignore the message

	// Decrypt the message
	IceKey iceKey( 2 );
	if ( iceKey.keySize() != V_strlen( key ) )
		return true; // we cannot decrypt with the supplied key
	iceKey.set( ( const unsigned char * ) key );
	if ( msgEncryptedInput.encrypted->size() % iceKey.blockSize() )
		return true; // message malformed, cannot decrypt
	if ( msgEncryptedInput.encrypted->size() > NET_MAX_PAYLOAD )
		return true; // size too large, cannot decrypt

	net_scratchbuffer_t scratch;
	byte *buffer = scratch.GetBuffer();
	unsigned char *pchCryptoBuffer = ( unsigned char * ) arena.AllocBytes( iceKey.blockSize() );
	for ( int k = 0; k < ( int ) msgEncryptedInput.encrypted->size(); k += iceKey.blockSize() )
	{
		iceKey.decrypt( ( const unsigned char * ) &msgEncryptedInput.encrypted->at(k), pchCryptoBuffer );
		V_memcpy( &buffer[k], pchCryptoBuffer, iceKey.blockSize() );
	}

	// Check how much random fudge we have
	int numRandomFudgeBytes = *buffer;
	if ( ( numRandomFudgeBytes > 0 ) && ( numRandomFudgeBytes + 1 + sizeof( int32 ) < msgEncryptedInput.encrypted->size() ) )
	{
		// Fetch the size of the encrypted message
		int32 numBytesWrittenWire = 0;
		V_memcpy( &numBytesWrittenWire, &buffer[ 1 + numRandomFudgeBytes ], sizeof( int32 ) );
		int32 const numBytesWritten = DWordSwapC( numBytesWrittenWire );	// byteswap from the wire
		
		// Make sure the total size of the message matches the expectations
		if ( numRandomFudgeBytes + 1 + sizeof( int32 ) + numBytesWritten == msgEncryptedInput.encrypted->size() )
		{
			bf_read bufRead( &buffer[ 1 + numRandomFudgeBytes + sizeof( int32 ) ], numBytesWritten );
			unsigned char cmd = bufRead.ReadVarInt32();

			// See if the netchan has the required binder registered
			int iMsgHandler = 0;
			INetMessageBinder *pMsgBind = pProcessingChannel->FindMessageBinder( cmd, iMsgHandler++ );
			if ( pMsgBind )
			{
				int startbit = bufRead.GetNumBitsRead();

				INetMessage *pEmbeddedMessage = pMsgBind->CreateFromBuffer( bufRead );
				if ( !pEmbeddedMessage )
				{
					Msg( "CmdEncryptedDataMessageCodec failed to parse embedded message" );
					contract_assert( 0 );
					return false;
				}
				pEmbeddedMessage->SetReliable( pProcessingChannel->WasLastMessageReliable() );

				pProcessingChannel->UpdateMessageStats( pEmbeddedMessage->GetGroup(), bufRead.GetNumBitsRead() - startbit );

				do 
				{
					bool bRet = pMsgBind->Process( *pEmbeddedMessage );
					if ( !bRet )
					{
						ConDMsg( "CmdEncryptedDataMessageCodec: netchannel failed processing embedded message %s.\n", pEmbeddedMessage->GetName() );
						contract_assert( 0 );
						pEmbeddedMessage->DeleteThis();
						return false;
					}

					// Move to another binder
					pMsgBind = pProcessingChannel->FindMessageBinder( cmd, iMsgHandler++ );
				} while ( pMsgBind );

				pEmbeddedMessage->DeleteThis();
			}
		}
	}

	return true;
}

CTSPool< net_scratchbuffer_t::buffer_t > net_scratchbuffer_t::sm_NetScratchBuffers;


//-----------------------------------------------------------------------------
// INetMessage: the operations that depend on the message class dispatch on its kind
//-----------------------------------------------------------------------------
namespace
{
template< class F >
decltype( auto ) net_message_visit( unsigned char kind, F &&f )
{
	switch ( static_cast< net_message_kind >( kind ) )
	{
#define X( Final, ... ) case net_message_kind::Final: return f( std::type_identity< Final >{} );
	KS_NET_MESSAGE_CLASSES( X )
#undef X
	case net_message_kind::count: break;
	}
	contract_assert( false );
	return f( std::type_identity< CNETMsg_NOP_t >{} );
}
}

bool INetMessage::ReadFromBuffer( bf_read &buffer )
{
	return net_message_visit( m_kind, [&]< class T >( std::type_identity< T > ) { return static_cast< T * >( this )->ReadFromBuffer( buffer ); } );
}

bool INetMessage::WriteToBuffer( bf_write &buffer ) const
{
	return net_message_visit( m_kind, [&]< class T >( std::type_identity< T > ) { return static_cast< const T * >( this )->WriteToBuffer( buffer ); } );
}

const char *INetMessage::GetName() const
{
	return net_message_visit( m_kind, [&]< class T >( std::type_identity< T > ) { return static_cast< const T * >( this )->GetName(); } );
}

const char *INetMessage::ToString() const
{
	return net_message_visit( m_kind, [&]< class T >( std::type_identity< T > ) { return static_cast< const T * >( this )->ToString(); } );
}

std::size_t INetMessage::GetSize() const
{
	return net_message_visit( m_kind, [&]< class T >( std::type_identity< T > ) { return static_cast< const T * >( this )->GetSize(); } );
}

void INetMessage::DeleteThis()
{
	net_message_visit( m_kind, [&]< class T >( std::type_identity< T > ) { delete static_cast< T * >( this ); return 0; } );
}
