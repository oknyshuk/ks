//====== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//
// Unique ID generation
//=============================================================================//

#include "tier0/platform.h"

#include "checksum_crc.h"
#include "tier1/uniqueid.h"
#include "tier1/utlbuffer.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"



//-----------------------------------------------------------------------------
// Creates a new unique id
//-----------------------------------------------------------------------------
void CreateUniqueId( UniqueId_t *pDest )
{
	// X360/linux TBD: Need a real UUID Implementation
	Q_memset( pDest, 0, sizeof( UniqueId_t ) );
}


//-----------------------------------------------------------------------------
// Creates a new unique id from a string representation of one
//-----------------------------------------------------------------------------
bool UniqueIdFromString( UniqueId_t *pDest, const char *pBuf, int nMaxLen )
{
	if ( nMaxLen == 0 )
	{
		nMaxLen = Q_strlen( pBuf );
	}

	char *pTemp = (char*)stackalloc( nMaxLen + 1 );
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
	Q_memset( pDest, 0, sizeof( UniqueId_t ) );
	if ( nMaxLen > 0 )
	{
		CRC32_t crc;
		CRC32_Init( &crc );
		CRC32_ProcessBuffer( &crc, pBuf, nMaxLen );
		CRC32_Final( &crc );
		Q_memcpy( pDest, &crc, sizeof( CRC32_t ) );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Sets an object ID to be an invalid state
//-----------------------------------------------------------------------------
void InvalidateUniqueId( UniqueId_t *pDest )
{
	Assert( pDest );
	memset( pDest, 0, sizeof( UniqueId_t ) );
}

bool IsUniqueIdValid( const UniqueId_t &id )
{
	UniqueId_t invalidId;
	memset( &invalidId, 0, sizeof( UniqueId_t ) );
	return !IsUniqueIdEqual( invalidId, id );
}

bool IsUniqueIdEqual( const UniqueId_t &id1, const UniqueId_t &id2 )
{
	return memcmp( &id1, &id2, sizeof( UniqueId_t ) ) == 0; 
}

void UniqueIdToString( const UniqueId_t &id, char *pBuf, int nMaxLen )
{
	pBuf[ 0 ] = 0;

// X360TBD: Need a real UUID Implementation
}

void CopyUniqueId( const UniqueId_t &src, UniqueId_t *pDest )
{
	memcpy( pDest, &src, sizeof( UniqueId_t ) );
}

bool Serialize( CUtlBuffer &buf, const UniqueId_t &src )
{
// X360TBD: Need a real UUID Implementation
	return false;
}

bool Unserialize( CUtlBuffer &buf, UniqueId_t &dest )
{
	if ( buf.IsText() )
	{
		int nTextLen = buf.PeekStringLength();
		char *pBuf = (char*)stackalloc( nTextLen );
		buf.GetString( pBuf, nTextLen );
		UniqueIdFromString( &dest, pBuf, nTextLen );
	}
	else
	{
		buf.Get( &dest, sizeof(UniqueId_t) );
	}
	return buf.IsValid();
}



