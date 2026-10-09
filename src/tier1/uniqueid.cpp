// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Unique ID generation


import std;
import ks.tier1;



//-----------------------------------------------------------------------------
// Creates a new unique id
//-----------------------------------------------------------------------------
void CreateUniqueId( UniqueId_t *pDest )
{
	// X360/linux TBD: Need a real UUID Implementation
	V_memset( pDest, 0, sizeof( UniqueId_t ) );
}


//-----------------------------------------------------------------------------
// Creates a new unique id from a string representation of one
//-----------------------------------------------------------------------------
bool UniqueIdFromString( UniqueId_t *pDest, const char *pBuf, int nMaxLen )
{
	ks::ScratchArena<> scratch;
	if ( nMaxLen == 0 )
	{
		nMaxLen = V_strlen( pBuf );
	}

	char *pTemp = (char*)scratch.AllocBytes( nMaxLen + 1 );
	V_strncpy( pTemp, pBuf, nMaxLen + 1 );
	--nMaxLen;
	while( (nMaxLen >= 0) && V_isspace( pTemp[nMaxLen] ) )
	{
		--nMaxLen;
	}
	pTemp[ nMaxLen + 1 ] = 0;

	while( *pTemp && V_isspace( *pTemp ) )
	{
		++pTemp;
	}

	// X360TBD: Need a real UUID Implementation
	// For now, use crc to generate a unique ID from the UUID string.
	V_memset( pDest, 0, sizeof( UniqueId_t ) );
	if ( nMaxLen > 0 )
	{
		CRC32_t crc;
		CRC32_Init( &crc );
		CRC32_ProcessBuffer( &crc, pBuf, nMaxLen );
		CRC32_Final( &crc );
		V_memcpy( pDest, &crc, sizeof( CRC32_t ) );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Sets an object ID to be an invalid state
//-----------------------------------------------------------------------------
void InvalidateUniqueId( UniqueId_t *pDest )
{
	contract_assert( pDest );
	std::memset( pDest, 0, sizeof( UniqueId_t ) );
}

bool IsUniqueIdValid( const UniqueId_t &id )
{
	UniqueId_t invalidId;
	std::memset( &invalidId, 0, sizeof( UniqueId_t ) );
	return !IsUniqueIdEqual( invalidId, id );
}

bool IsUniqueIdEqual( const UniqueId_t &id1, const UniqueId_t &id2 )
{
	return std::memcmp( &id1, &id2, sizeof( UniqueId_t ) ) == 0; 
}

void UniqueIdToString( const UniqueId_t &id, char *pBuf, int nMaxLen )
{
	pBuf[ 0 ] = 0;

// X360TBD: Need a real UUID Implementation
}

void CopyUniqueId( const UniqueId_t &src, UniqueId_t *pDest )
{
	std::memcpy( pDest, &src, sizeof( UniqueId_t ) );
}

bool Serialize( ks::byte_writer &buf, const UniqueId_t &src )
{
// X360TBD: Need a real UUID Implementation
	return false;
}

bool Serialize( ks::text_writer &buf, const UniqueId_t &src )
{
// X360TBD: Need a real UUID Implementation
	return false;
}

bool Unserialize( ks::byte_reader &buf, UniqueId_t &dest )
{
	buf.read_bytes( &dest, sizeof(UniqueId_t) );
	return buf.ok();
}

bool Unserialize( ks::text_reader &buf, UniqueId_t &dest )
{
	const std::string text( buf.read_word() );
	UniqueIdFromString( &dest, text.c_str(), (int)text.size() + 1 );
	return buf.ok();
}
