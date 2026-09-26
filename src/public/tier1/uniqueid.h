//====== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//
// Utilities for globally unique IDs
//=============================================================================//

#ifndef UNIQUEID_H
#define UNIQUEID_H


#include "tier1/utlvector.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct UniqueId_t;
namespace ks
{
	class byte_reader;
	class byte_writer;
	class text_reader;
	class text_writer;
}


//-----------------------------------------------------------------------------
// Defines a globally unique ID
//-----------------------------------------------------------------------------
struct UniqueId_t
{
	unsigned char m_Value[16];
};


//-----------------------------------------------------------------------------
// Methods related to unique ids
//-----------------------------------------------------------------------------
void CreateUniqueId( UniqueId_t *pDest );
void InvalidateUniqueId( UniqueId_t *pDest );
bool IsUniqueIdValid( const UniqueId_t &id );
bool IsUniqueIdEqual( const UniqueId_t &id1, const UniqueId_t &id2 );
void UniqueIdToString( const UniqueId_t &id, char *pBuf, int nMaxLen );
bool UniqueIdFromString( UniqueId_t *pDest, const char *pBuf, int nMaxLen = 0 );
void CopyUniqueId( const UniqueId_t &src, UniqueId_t *pDest );
bool Serialize( ks::byte_writer &buf, const UniqueId_t &src );
bool Unserialize( ks::byte_reader &buf, UniqueId_t &dest );
bool Serialize( ks::text_writer &buf, const UniqueId_t &src );
bool Unserialize( ks::text_reader &buf, UniqueId_t &dest );

inline bool operator ==( const UniqueId_t& lhs, const UniqueId_t& rhs )
{
	return !Q_memcmp( (void *)&lhs.m_Value[ 0 ], (void *)&rhs.m_Value[ 0 ], sizeof( lhs.m_Value ) );
}


#endif // UNIQUEID_H

