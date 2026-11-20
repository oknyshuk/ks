#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include <const.h>
import std;
import ks.core.types;
import ks.platform;
import ks.simd;

#include "common.h"
#include "ents_shared.h"

// This is extra spew to the files cltrace.txt + svtrace.txt
// #define DEBUG_NETWORKING 1

#define TRACE_PACKET( text )

inline constexpr auto FLAG_IS_COMPRESSED = (1<<31);


class CSendProxyRecipients;
class SendTable;
class RecvTable;
class ServerClass;
class ClientClass;
class CChangeFrameList;
typedef intp SerializedEntityHandle_t;

// Replaces entity_state_t.
// This is what we send to clients.

class PackedEntity
{
public:

				PackedEntity();
				~PackedEntity();
	
	void		SetNumBits( int nBits );
	int			GetNumBits() const;
	int			GetNumBytes() const;

	// Access the data in the entity.
	const SerializedEntityHandle_t GetPackedData() const;
	void		ReleasePackedData();

	// Copy the data into the PackedEntity's data and make sure the # bytes allocated is
	// an integer multiple of 4.
	void		CopyPackedData( SerializedEntityHandle_t handle );
	// Like copy, but just takes ownership of handle.  Assumes that parent caller is done with entity and will NOT
	// call ReleaseSerializedEntity
	void		SetPackedData( SerializedEntityHandle_t handle ); 

	// These are like Get/Set, except SnagChangeFrameList clears out the
	// PackedEntity's pointer since the usage model in sv_main is to keep
	// the same CChangeFrameList in the most recent PackedEntity for the
	// lifetime of an edict.
	//
	// When the PackedEntity is deleted, it deletes its current CChangeFrameList if it exists.
	void				SetChangeFrameList( CChangeFrameList *pList );
	CChangeFrameList*	GetChangeFrameList();
	const CChangeFrameList*	GetChangeFrameList() const;
	CChangeFrameList*	SnagChangeFrameList();

	// Access the recipients array.
	const CSendProxyRecipients*	GetRecipients() const;
	int							GetNumRecipients() const;

	void				SetRecipients( std::span<const CSendProxyRecipients> recipients );
	bool				CompareRecipients( std::span<const CSendProxyRecipients> recipients ) const;

	void				SetSnapshotCreationTick( int nTick );
	int					GetSnapshotCreationTick() const;

	void				SetShouldCheckCreationTick( bool bState );
	bool				ShouldCheckCreationTick() const;

	void				SetServerAndClientClass( ServerClass *pServerClass, ClientClass *pClientClass );

public:
	
	ServerClass *m_pServerClass;	// Valid on the server
	ClientClass	*m_pClientClass;	// Valid on the client
		
	int			m_nEntityIndex;		// Entity index.
	std::atomic<int>	m_ReferenceCount;	// reference count;

private:

	std::vector< CSendProxyRecipients >	m_Recipients;

	SerializedEntityHandle_t			m_SerializedEntity;
	CChangeFrameList	*m_pChangeFrameList;	// Only the most current 

	// This is the tick this PackedEntity was created on
	unsigned int		m_nSnapshotCreationTick : 31;
	unsigned int		m_nShouldCheckCreationTick : 1;
};

inline const SerializedEntityHandle_t PackedEntity::GetPackedData() const
{
	return m_SerializedEntity;
}

inline void PackedEntity::SetChangeFrameList( CChangeFrameList *pList )
{
	contract_assert( !m_pChangeFrameList );
	m_pChangeFrameList = pList;
}

inline CChangeFrameList* PackedEntity::GetChangeFrameList()
{
	return m_pChangeFrameList;
}

inline const CChangeFrameList* PackedEntity::GetChangeFrameList() const
{
	return m_pChangeFrameList;
}

inline CChangeFrameList* PackedEntity::SnagChangeFrameList()
{
	CChangeFrameList *pRet = m_pChangeFrameList;
	m_pChangeFrameList = nullptr;
	return pRet;
}

inline void PackedEntity::SetSnapshotCreationTick( int nTick )
{
	m_nSnapshotCreationTick = (unsigned int)nTick;
}

inline int PackedEntity::GetSnapshotCreationTick() const
{
	return (int)m_nSnapshotCreationTick;
}

inline void PackedEntity::SetShouldCheckCreationTick( bool bState )
{
	m_nShouldCheckCreationTick = bState ? 1 : 0;
}

inline bool PackedEntity::ShouldCheckCreationTick() const
{
	return m_nShouldCheckCreationTick == 1 ? true : false;
}
