// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
import std;

static volatile bool bSpewAllocations = false; // TODO: Register CMemoryStacks with g_pMemAlloc, so it can spew a summary

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

void PrintStatus( void* p )
{
	CMemoryStack* pMemoryStack = (CMemoryStack*)p;

	pMemoryStack->PrintContents();
}

CMemoryStack::CMemoryStack()
 : 	m_pNextAlloc( nullptr )
	, m_pCommitLimit( nullptr )
	, m_pAllocLimit( nullptr )
	, m_pHighestAllocLimit( nullptr )
	, m_pBase( nullptr )
	, m_bRegisteredAllocation( false )
 	, m_maxSize( 0 )
	, m_alignment( 16 )
{
	AddMemoryInfoCallback( this, std::function_ref< MemoryInfo_t() const >( std::cw< &CMemoryStack::GetMemoryInfo >, this ) );
	m_pszAllocOwner = MemAlloc_StrDup( "CMemoryStack unattributed" );
}
	
//-------------------------------------

CMemoryStack::~CMemoryStack()
{
	if ( m_pBase )
		Term();

	RemoveMemoryInfoCallback( this );
	g_pMemAlloc->Free( m_pszAllocOwner );
}

//-------------------------------------

bool CMemoryStack::Init( const char *pszAllocOwner, unsigned maxSize, unsigned commitIncrement, unsigned initialCommit, unsigned alignment )
{
	contract_assert( !m_pBase );

	m_bPhysical = false;

	m_maxSize = maxSize;
	m_alignment = AlignValue( alignment, 4 );

	contract_assert( m_alignment == alignment );
	contract_assert( m_maxSize > 0 );

	SetAllocOwner( pszAllocOwner );

	m_pBase = (byte*)MemAlloc_AllocAlignedUnattributed( m_maxSize, alignment ? alignment : 1 );
	m_pNextAlloc = m_pBase;
	m_pCommitLimit = m_pBase + m_maxSize;

	m_pHighestAllocLimit = m_pNextAlloc;

	m_pAllocLimit = m_pBase + m_maxSize;

	return ( m_pBase != nullptr );
}

//-------------------------------------

void CMemoryStack::Term()
{
	FreeAll();
	if ( m_pBase )
	{
		MemAlloc_FreeAligned( m_pBase );
		m_pBase = nullptr;
		// Zero these variables to avoid getting misleading mem_dump
		// results when m_pBase is NULL.
		m_pNextAlloc = nullptr;
		m_pCommitLimit = nullptr;
		m_pHighestAllocLimit = nullptr;
		m_maxSize = 0;
		RegisterDeallocation(true);
	}
}

//-------------------------------------

int CMemoryStack::GetSize() const
{ 
	if ( m_bPhysical )
		return m_maxSize;

	return m_maxSize;
}


//-------------------------------------

bool CMemoryStack::CommitTo( byte *pNextAlloc ) __restrict__
{
	return false;
}

// Identify the owner of this memory stack's memory
void CMemoryStack::SetAllocOwner( const char *pszAllocOwner )
{
	if ( !pszAllocOwner || !V_strcmp( m_pszAllocOwner, pszAllocOwner ) )
		return;
	g_pMemAlloc->Free( m_pszAllocOwner );
	m_pszAllocOwner = MemAlloc_StrDup( pszAllocOwner );
}

void CMemoryStack::RegisterAllocation()
{
	if ( GetSize() )
	{
		if ( m_bRegisteredAllocation )
			Warning( "CMemoryStack: ERROR - mismatched RegisterAllocation/RegisterDeallocation!\n" );

		// NOTE: we deliberately don't use MemAlloc_RegisterExternalAllocation. CMemoryStack needs to bypass 'GetActualDbgInfo'
		// due to the way it allocates memory: there's just one representative memory address (m_pBase), it grows at unpredictable
		// times (in CommitTo, not every Alloc call) and it is freed en-masse (instead of freeing each individual allocation).
		MemAlloc_RegisterAllocation( m_pszAllocOwner, 0, GetSize(), GetSize(), 0 );
	}
	m_bRegisteredAllocation = true;

	// Temp memorystack spew: very useful when we crash out of memory
}

void CMemoryStack::RegisterDeallocation( bool bShouldSpewSize )
{
	if ( GetSize() )
	{
		if ( !m_bRegisteredAllocation )
			Warning( "CMemoryStack: ERROR - mismatched RegisterAllocation/RegisterDeallocation!\n" );
		MemAlloc_RegisterDeallocation( m_pszAllocOwner, 0, GetSize(), GetSize(), 0 );
	}
	m_bRegisteredAllocation = false;

	// Temp memorystack spew: very useful when we crash out of memory
}

//-------------------------------------

void CMemoryStack::FreeToAllocPoint( MemoryStackMark_t mark, bool bDecommit )
{
	mark = AlignValue( mark, m_alignment );
	byte *pAllocPoint = m_pBase + mark;

	contract_assert( pAllocPoint >= m_pBase && pAllocPoint <= m_pNextAlloc );
	if ( pAllocPoint >= m_pBase && pAllocPoint <= m_pNextAlloc )
	{
		m_pNextAlloc = pAllocPoint;
	}
}

//-------------------------------------

void CMemoryStack::FreeAll( bool bDecommit )
{
	if ( m_pBase && ( m_pBase < m_pCommitLimit ) )
	{
		FreeToAllocPoint( 0, bDecommit );
	}
}

//-------------------------------------

void CMemoryStack::Access( void **ppRegion, unsigned *pBytes )
{
	*ppRegion = m_pBase;
	*pBytes = ( m_pNextAlloc - m_pBase);
}

MemoryInfo_t CMemoryStack::GetMemoryInfo() const
{
	return MemoryInfo_t{
		.name = m_pszAllocOwner,
		.allocatedBytes = static_cast< std::size_t >( GetUsed() ),
		.committedBytes = static_cast< std::size_t >( GetSize() ),
		.reservedBytes = static_cast< std::size_t >( GetMaxSize() ),
		.highestBytes = static_cast< std::size_t >( m_pHighestAllocLimit - m_pBase ) };
}

//-------------------------------------

void CMemoryStack::PrintContents() const
{
	std::size_t highest = m_pHighestAllocLimit - m_pBase;
	Msg( "CMemoryStack %s\n", m_pszAllocOwner );
	Msg( "    Total used memory:      %d KB\n", GetUsed() / 1024 );
	Msg( "    Total committed memory: %d KB\n", GetSize() / 1024 );
	Msg( "    Max committed memory: %u KB out of %d KB\n", (unsigned)highest / 1024, GetMaxSize() / 1024 );
}
