// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: Contains a branch-neutral binary packer for KeyValues trees.

import ks.tier1;


import std;
constexpr auto KEYVALUES_TOKEN_SIZE = 1024;

// writes KeyValue as binary data to buffer
bool KVPacker::WriteAsBinary( KeyValues *pNode, ks::byte_writer &buffer )
{
	if ( !buffer.ok() ) // must be valid, no overflows etc
		return false;

	// Write subkeys:

	// loop through all our peers
	for ( KeyValues *dat = pNode; dat != nullptr; dat = dat->GetNextKey() )
	{
		// write type
		switch ( dat->GetDataType() )
		{
		case KeyValues::TYPE_NONE:
			{
				buffer.write< unsigned char >( PACKTYPE_NONE );
				break;
			}
		case KeyValues::TYPE_STRING:
			{
				buffer.write< unsigned char >( PACKTYPE_STRING );
				break;
			}
		case KeyValues::TYPE_WSTRING:
			{
				buffer.write< unsigned char >( PACKTYPE_WSTRING );
				break;
			}

		case KeyValues::TYPE_INT:
			{
				buffer.write< unsigned char >( PACKTYPE_INT );
				break;
			}

		case KeyValues::TYPE_UINT64:
			{
				buffer.write< unsigned char >( PACKTYPE_UINT64 );
				break;
			}

		case KeyValues::TYPE_FLOAT:
			{
				buffer.write< unsigned char >( PACKTYPE_FLOAT );
				break;
			}
		case KeyValues::TYPE_COLOR:
			{
				buffer.write< unsigned char >( PACKTYPE_COLOR );
				break;
			}
		case KeyValues::TYPE_PTR:
			{
				buffer.write< unsigned char >( PACKTYPE_PTR );
				break;
			}

		default:
			break;
		}

		// write name
		buffer.write_cstring( dat->GetName() );

		// write value
		switch ( dat->GetDataType() )
		{
		case KeyValues::TYPE_NONE:
			{
				if( !WriteAsBinary( dat->GetFirstSubKey(), buffer ) )
					return false;
				break;
			}
		case KeyValues::TYPE_STRING:
			{
				if (dat->GetString() && *(dat->GetString()))
				{
					buffer.write_cstring( dat->GetString() );
				}
				else
				{
					buffer.write_cstring( "" );
				}
				break;
			}
		case KeyValues::TYPE_WSTRING:
			{
				int nLength = dat->GetWString() ? V_wcslen( dat->GetWString() ) : 0;
				buffer.write< short >( nLength );
				for( int k = 0; k < nLength; ++ k )
				{
					buffer.write< short >( ( unsigned short ) dat->GetWString()[k] );
				}
				break;
			}

		case KeyValues::TYPE_INT:
			{
				buffer.write< int >( dat->GetInt() );				
				break;
			}

		case KeyValues::TYPE_UINT64:
			{
				buffer.write< int64 >( dat->GetUint64() );
				break;
			}

		case KeyValues::TYPE_FLOAT:
			{
				buffer.write< float >( dat->GetFloat() );
				break;
			}
		case KeyValues::TYPE_COLOR:
			{
				Color color = dat->GetColor();
				buffer.write< unsigned char >( color[0] );
				buffer.write< unsigned char >( color[1] );
				buffer.write< unsigned char >( color[2] );
				buffer.write< unsigned char >( color[3] );
				break;
			}
		case KeyValues::TYPE_PTR:
			{
				buffer.write< uint64 >( reinterpret_cast< std::uintptr_t >( dat->GetPtr() ) );
				break;
			}

		default:
			break;
		}
	}

	// write tail, marks end of peers
	buffer.write< unsigned char >( PACKTYPE_NULLMARKER ); 

	return buffer.ok();
}

// read KeyValues from binary buffer, returns true if parsing was successful
bool KVPacker::ReadAsBinary( KeyValues *pNode, ks::byte_reader &buffer )
{
	if ( !buffer.ok() ) // must be valid, no overflows etc
		return false;

	pNode->Clear();

	char		token[KEYVALUES_TOKEN_SIZE];
	KeyValues	*dat = pNode;
	EPackType		ePackType = (EPackType)buffer.read< unsigned char >();

	// loop through all our peers
	while ( true )
	{
		if ( ePackType == PACKTYPE_NULLMARKER )
			break; // no more peers

		buffer.read_cstring( token, KEYVALUES_TOKEN_SIZE-1 );
		token[KEYVALUES_TOKEN_SIZE-1] = 0;

		dat->SetName( token );

		switch ( ePackType )
		{
		case PACKTYPE_NONE:
			{
				KeyValues *pNewNode = new KeyValues("");
				dat->AddSubKey( pNewNode );
				if( !ReadAsBinary( pNewNode, buffer ) )
					return false;
				break;
			}
		case PACKTYPE_STRING:
			{
				buffer.read_cstring( token, KEYVALUES_TOKEN_SIZE-1 );
				token[KEYVALUES_TOKEN_SIZE-1] = 0;
				dat->SetStringValue( token );
				break;
			}
		case PACKTYPE_WSTRING:
			{
				int nLength = buffer.read< short >();
				if ( nLength >= 0 && nLength*sizeof( uint16 ) <= buffer.remaining() )
				{
					if ( nLength > 0 )
					{
						wchar_t *pTemp = (wchar_t *)MemAlloc_Alloc( sizeof( wchar_t ) * (1 + nLength) );

						for ( int k = 0; k < nLength; ++k )
						{
							pTemp[k] = buffer.read< short >(); // ugly, but preserving existing behavior
						}

						pTemp[nLength] = 0;
						dat->SetWString( nullptr, pTemp );

						g_pMemAlloc->Free( pTemp );
					}
					else
						dat->SetWString( nullptr, L"" );

				}
				break;
			}

		case PACKTYPE_INT:
			{
				dat->SetInt( nullptr, buffer.read< int >() );
				break;
			}

		case PACKTYPE_UINT64:
			{
				dat->SetUint64( nullptr, (uint64)buffer.read< int64 >() );
				break;
			}

		case PACKTYPE_FLOAT:
			{
				dat->SetFloat( nullptr, buffer.read< float >() );
				break;
			}
		case PACKTYPE_COLOR:
			{
				const unsigned char r = buffer.read< unsigned char >();
				const unsigned char g = buffer.read< unsigned char >();
				const unsigned char b = buffer.read< unsigned char >();
				const unsigned char a = buffer.read< unsigned char >();
				dat->SetColor( nullptr, Color( r, g, b, a ) );
				break;
			}
		case PACKTYPE_PTR:
			{
				dat->SetPtr( nullptr, reinterpret_cast< void * >( static_cast< std::uintptr_t >( buffer.read< uint64 >() ) ) );
				break;
			}

		default:
			break;
		}

		if ( !buffer.ok() ) // error occured
			return false;

		ePackType = (EPackType)buffer.read< unsigned char >();

		if ( ePackType == PACKTYPE_NULLMARKER )
			break;

		// new peer follows
		KeyValues *pNewPeer = new KeyValues("");
		dat->SetNextKey( pNewPeer );
		dat = pNewPeer;
	}

	return buffer.ok();
}
