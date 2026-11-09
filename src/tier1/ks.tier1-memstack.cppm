// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1:memstack;

import std;
import ks.math;
export import :utlvector;

export extern "C++" {

//-----------------------------------------------------------------------------

typedef unsigned MemoryStackMark_t;

class CMemoryStack
{
public:
	CMemoryStack();
	~CMemoryStack();

	bool Init( const char *pszAllocOwner, unsigned maxSize = 0, unsigned commitIncrement = 0, unsigned initialCommit = 0, unsigned alignment = 16 );
	void Term();

	int GetSize() const;
	int GetMaxSize() const ;
	int	GetUsed() const;
	
	void *Alloc( unsigned bytes, bool bClear = false ) __restrict__;

	MemoryStackMark_t GetCurrentAllocPoint() const;
	void FreeToAllocPoint( MemoryStackMark_t mark, bool bDecommit = true );
	void FreeAll( bool bDecommit = true );
	
	void Access( void **ppRegion, unsigned *pBytes );

	void PrintContents() const;

	void *GetBase();
	const void *GetBase() const {  return const_cast<CMemoryStack *>(this)->GetBase(); }

	bool CommitSize( int nBytes );

	void SetAllocOwner( const char *pszAllocOwner );

private:
	bool CommitTo( byte * ) __restrict__;
	void RegisterAllocation();
	void RegisterDeallocation( bool bShouldSpew );

	byte *m_pNextAlloc; // Current alloc point (m_pNextAlloc - m_pBase == allocated bytes)
	byte *m_pCommitLimit; // The current end of the committed memory. On systems without dynamic commit/decommit this is always m_pAllocLimit
	byte *m_pAllocLimit; // The top of the allocated address space (m_pBase + m_maxSize)
	// Track the highest alloc limit seen.
	byte *m_pHighestAllocLimit;

	MemoryInfo_t GetMemoryInfo() const; // Statistics reported to DumpMemoryInfoStats

	byte *m_pBase;
	bool m_bRegisteredAllocation;
	bool m_bPhysical;
	char *m_pszAllocOwner;

	unsigned m_maxSize; // m_maxSize stores how big the stack can grow. It measures the reservation size.
	unsigned m_alignment;

private:
	// Make the assignment operator and copy constructor private and unimplemented.
	CMemoryStack& operator=( const CMemoryStack& );
	CMemoryStack( const CMemoryStack& );
};

//-------------------------------------

[[gnu::always_inline]] inline void *CMemoryStack::Alloc( unsigned bytes, bool bClear ) __restrict__
{
	contract_assert( m_pBase );

	bytes = std::max( bytes, m_alignment );
	bytes = AlignValue( bytes, m_alignment );

	void *pResult = m_pNextAlloc;
	byte *pNextAlloc = m_pNextAlloc + bytes;

	if ( pNextAlloc > m_pCommitLimit )
	{
		if ( !CommitTo( pNextAlloc ) )
		{
			return nullptr;
		}
	}

	if ( bClear )
	{
		std::memset( pResult, 0, bytes );
	}

	m_pNextAlloc = pNextAlloc;
	m_pHighestAllocLimit = Max( m_pNextAlloc, m_pHighestAllocLimit );

	return pResult;
}

//-------------------------------------

inline bool CMemoryStack::CommitSize( int nBytes )
{
	if ( GetSize() != nBytes )
	{
		return CommitTo( m_pBase + nBytes );
	}
	return true;
}

//-------------------------------------

// How big can this memory stack grow? This is equivalent to how many
// bytes are reserved.
inline int CMemoryStack::GetMaxSize() const
{ 
	return m_maxSize;
}

//-------------------------------------

inline int CMemoryStack::GetUsed() const
{ 
	return ( m_pNextAlloc - m_pBase ); 
}

//-------------------------------------

inline void *CMemoryStack::GetBase()
{
	return m_pBase;
}

//-------------------------------------

inline MemoryStackMark_t CMemoryStack::GetCurrentAllocPoint() const
{
	return ( m_pNextAlloc - m_pBase );
}

}
