// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "keyvaluescompiler.h"
#include "filesystem.h"
#include "tier2/tier2.h"
import std;

bool CRunTimeKeyValuesStringTable::ReadStringTable( int numStrings, ks::byte_reader &buf )
{
	contract_assert( std::ssize( m_Strings ) == 0 );

	if ( numStrings < 0 )
	{
		return false;
	}

	std::vector< int > offsets( numStrings );
	buf.read_bytes( offsets.data(), numStrings * sizeof( int ) );

	int stringSize = buf.read< int >();

	if ( !buf.ok() )
	{
		return false;
	}

	// The strings point into the loaded file data
	const char *pStrings = reinterpret_cast< const char * >( buf.rest().data() );

	m_Strings.reserve( numStrings );
	for ( int i = 0 ; i < numStrings; ++i )
	{
		m_Strings.push_back( pStrings + offsets[ i ] );
	}

	buf.skip( stringSize );

	return buf.ok();
}

void CCompiledKeyValuesWriter::BuildKVData_R( KeyValues *kv, int parent )
{
	// Add self
	KVInfo_t info;
	info.key = m_StringTable.AddString( kv->GetName() );
	info.value = m_StringTable.AddString( kv->GetString() );

	info.SetSubTree( kv->GetFirstSubKey() != nullptr ? true : false );
	info.SetParent( parent );

	int newParent = std::ssize( m_Data ); m_Data.push_back( info );

	// Then add children
	for ( KeyValues *sub = kv->GetFirstSubKey(); sub; sub = sub->GetNextKey() )
	{
		BuildKVData_R( sub, newParent );
	}

	// Then add peers
	if ( parent == -1 )
	{
		if ( kv->GetNextKey() )
		{
			BuildKVData_R( kv->GetNextKey(), parent );
		}
	}
}

void CCompiledKeyValuesWriter::Describe( const KVFile_t& file )
{
	Msg( "file( %s )\n", m_StringTable.String( file.filename ) );

	int c = file.numElements;
	for ( int i = 0; i < c; ++i )
	{
		KVInfo_t &info = m_Data[ file.firstElement + i ];
		if ( info.IsSubTree() )
		{
			Msg( "%d:  %s -> subtree at parent %i\n",
				file.firstElement + i,
				m_StringTable.String( info.key ),
				info.GetParent() );
		}
		else
		{
			Msg( "%d:  %s -> %s at parent %i\n",
				file.firstElement + i,
				m_StringTable.String( info.key ),
				m_StringTable.String( info.value ),
				info.GetParent() );
		}
	}
}

void CCompiledKeyValuesWriter::AppendKeyValuesFile( char const *filename )
{
	KVFile_t kvf;
	kvf.filename = m_StringTable.AddString( filename );
	kvf.firstElement = std::ssize( m_Data );

	KeyValues *kv = new KeyValues( filename );
	if ( kv->LoadFromFile( g_pFullFileSystem, filename ) )
	{
		// Add to dictionary
		// do a depth first traversal of the keyvalues
		BuildKVData_R( kv, -1 );
	}
	kv->deleteThis();

	kvf.numElements = std::ssize( m_Data ) - kvf.firstElement;

//	Describe( kvf );

	m_Files.push_back( kvf );
}

void CCompiledKeyValuesWriter::WriteData( ks::byte_writer &buf )
{
	int c = std::ssize( m_Data );
	buf.write< int >( c );
	for ( int i = 0; i < c; ++i )
	{
		KVInfo_t &info = m_Data[ i ];
		buf.write< short >( info.key );
		buf.write< short >( info.value );
		buf.write< short >( info.GetParent() );
		buf.write< char >( info.IsSubTree() ? 1 : 0 );
	}
}

void CCompiledKeyValuesWriter::WriteFiles( ks::byte_writer &buf )
{
	int c = std::ssize( m_Files );
	buf.write< int >( c );
	for ( int i = 0; i < c; ++i )
	{
		KVFile_t &file = m_Files[ i ];
		buf.write< short >( file.filename );
		buf.write< short >( file.firstElement );
		buf.write< short >( file.numElements );
	}
}

void CCompiledKeyValuesWriter::WriteStringTable( ks::byte_writer &buf )
{
	int i;
	std::vector< int >	offsets;

	ks::byte_writer stringBuffer;

	offsets.push_back( stringBuffer.tell() );

	stringBuffer.write_cstring( "" );
	// save all the rest
	int c = m_StringTable.GetNumStrings();
	for ( i = 1; i < c; i++)
	{
		offsets.push_back( stringBuffer.tell() );
		stringBuffer.write_cstring( m_StringTable.String( i ) );
	}

	buf.write_bytes( offsets.data(), std::ssize( offsets ) * sizeof( int ) );

	buf.write< int >( stringBuffer.size() );
	buf.write_bytes( stringBuffer.bytes() );
}

void CCompiledKeyValuesWriter::WriteFile( char const *outfile )
{
	ks::byte_writer buf;

	// Write the data file out
	KVHeader_t header;
	header.fileid		= COMPILED_KEYVALUES_ID;
	header.version		= COMPILED_KEYVALUES_VERSION;
	header.numStrings	= m_StringTable.GetNumStrings();

	buf.write( header );

	WriteStringTable( buf );
	WriteData( buf );
	WriteFiles( buf );

	g_pFullFileSystem->WriteFile( outfile, nullptr, buf.bytes() );
}

CCompiledKeyValuesReader::CCompiledKeyValuesReader()
{
}

int CCompiledKeyValuesReader::First() const
{
	return m_Dict.empty() ? InvalidIndex() : 0;
}

int CCompiledKeyValuesReader::Next( int i ) const
{
	return i + 1 < (int)m_Dict.size() ? i + 1 : InvalidIndex();
}

int CCompiledKeyValuesReader::InvalidIndex() const
{
	return -1;
}

void CCompiledKeyValuesReader::GetFileName( int index, char *buf, std::size_t bufsize )
{
	contract_assert( buf );
	buf[ 0 ] = 0;
	FileNameHandle_t handle = std::next( m_Dict.begin(), index )->hFile;
	g_pFullFileSystem->String( handle, buf, bufsize );
}

bool CCompiledKeyValuesReader::LoadFile( char const *filename )
{
	int i;

	auto data = g_pFullFileSystem->ReadFile( filename, nullptr );
	if ( !data )
	{
		return false;
	}
	m_LoadBuffer = std::move( *data );

	ks::byte_reader reader( m_LoadBuffer );

	KVHeader_t header = reader.read< KVHeader_t >();
	if ( !reader.ok() )
	{
		return false;
	}

	if ( header.fileid != COMPILED_KEYVALUES_ID )
	{
		return false;
	}

	if ( header.version != COMPILED_KEYVALUES_VERSION )
	{
		return false;
	}

	if ( !m_StringTable.ReadStringTable( header.numStrings, reader ) )
	{
		return false;
	}

	// Now parse the data
	int dataCount = reader.read< int >();
	m_Data.reserve( dataCount );
	for ( i = 0; i < dataCount; ++i )
	{
		KVInfo_t info;
		info.key = reader.read< short >();
		info.value = reader.read< short >();
		info.SetParent( reader.read< short >() );
		info.SetSubTree( reader.read< char >() == 1 ? true : false );
		m_Data.push_back( info );
	}

	int fileCount = reader.read< int >();
	for ( i = 0; i < fileCount; ++i )
	{
		FileInfo_t kvf;
		short fileNameString	= reader.read< short >();

		kvf.hFile		= g_pFullFileSystem->FindOrAddFileName( m_StringTable.Lookup( fileNameString ) );
		kvf.nFirstIndex = reader.read< short >();
		kvf.nCount		= reader.read< short >();

		m_Dict.insert( kvf );
	}

	return true;
}

struct CreateHelper_t
{
	KeyValues	*kv;
	KeyValues	*tail;
};

KeyValues *CCompiledKeyValuesReader::CreateFromData( const FileInfo_t& info )
{
	KeyValues *head = new KeyValues( "" );
	if ( CreateInPlaceFromData( *head, info ) )
	{
		return head;
	}
	else
	{
		head->deleteThis();
		return nullptr;
	}
}

bool CCompiledKeyValuesReader::CreateInPlaceFromData( KeyValues& head, const FileInfo_t& info )
{
	int first = info.nFirstIndex;
	int num = info.nCount;

	KeyValues *root = nullptr;
	KeyValues *tail = nullptr;

	std::map< int, CreateHelper_t >	helper;	// data index -> node

	for ( int i = 0; i < num; ++i )
	{
		int offset = first + i;
		KVInfo_t& info = m_Data[ offset ];

		if ( info.GetParent() != -1 )
		{
			auto idx = helper.find( info.GetParent() );
			if ( idx == helper.end() )
			{
				return false;
			}

			KeyValues *parent = idx->second.kv;
			contract_assert( parent );

			KeyValues *sub = new KeyValues( m_StringTable.Lookup( info.key ) );

			if ( !info.IsSubTree() )
			{
				sub->SetStringValue(m_StringTable.Lookup( info.value ) );
			}

			if ( !parent->GetFirstSubKey() )
			{
				parent->AddSubKey( sub );
			}
			else
			{
				KeyValues *last = idx->second.tail;
				last->SetNextKey( sub );
			}

			idx->second.tail = sub;

			helper.emplace( offset, CreateHelper_t{ sub, nullptr } );
		}
		else
		{
			if ( !root )
			{
				root = &head;
				root->SetName( m_StringTable.Lookup( info.key ) );
				tail = root;

				helper.emplace( offset, CreateHelper_t{ root, nullptr } );
			}
			else
			{
				KeyValues *kv = new KeyValues( m_StringTable.Lookup( info.key ) );
				helper.emplace( offset, CreateHelper_t{ kv, nullptr } );

				tail->SetNextKey( kv );
				tail = kv;
			}
		}
	}
	return true;
}


bool CCompiledKeyValuesReader::InstanceInPlace( KeyValues& head, char const *kvfilename )
{
	char sz[ 512 ];
	V_strncpy( sz, kvfilename, sizeof( sz ) );
	V_FixSlashes( sz );

	FileInfo_t search;
	search.hFile = g_pFullFileSystem->FindOrAddFileName( sz );

	auto idx = m_Dict.find( search );
	if ( idx == m_Dict.end() )
	{
		return false;
	}

	const FileInfo_t& info = *idx;

	return CreateInPlaceFromData( head, info );
}

KeyValues *CCompiledKeyValuesReader::Instance( char const *kvfilename )
{
	char sz[ 512 ];
	V_strncpy( sz, kvfilename, sizeof( sz ) );
	V_FixSlashes( sz );

	FileInfo_t search;
	search.hFile = g_pFullFileSystem->FindOrAddFileName( sz );

	auto idx = m_Dict.find( search );
	if ( idx == m_Dict.end() )
	{
		return nullptr;
	}

	const FileInfo_t& info = *idx;

	return CreateFromData( info );
}

bool CCompiledKeyValuesReader::LookupKeyValuesRootKeyName( char const *kvfilename, char *outbuf, std::size_t bufsize )
{
	char sz[ 512 ];
	V_strncpy( sz, kvfilename, sizeof( sz ) );
	V_FixSlashes( sz );

	FileInfo_t search;
	search.hFile = g_pFullFileSystem->FindOrAddFileName( sz );

	auto idx = m_Dict.find( search );
	if ( idx == m_Dict.end() )
	{
		return false;
	}

	const FileInfo_t& info = *idx;

	V_strncpy( outbuf, m_StringTable.Lookup( m_Data[ info.nFirstIndex ].key ), bufsize );
	return true;
}
