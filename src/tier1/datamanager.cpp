//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "basetypes.h"
#include "datamanager.h"


DECLARE_POINTER_HANDLE( memhandle_t );

CDataManagerBase::CDataManagerBase( unsigned int maxSize )
{
	m_targetMemorySize = maxSize;
	m_memUsed = 0;
	m_listsAreFreed = 0;
	m_freeOnDestruct = 1;
}

CDataManagerBase::~CDataManagerBase() 
{
	Assert( !m_freeOnDestruct || m_listsAreFreed );
}

void CDataManagerBase::NotifySizeChanged( memhandle_t handle, unsigned int oldSize, unsigned int newSize )
{
	lock();
	m_memUsed += (int)newSize - (int)oldSize;
	unlock();
}

void CDataManagerBase::SetTargetSize( unsigned int targetSize )
{
	m_targetMemorySize = targetSize;
}

unsigned int CDataManagerBase::FlushAllUnlocked()
{
	lock();

	int nFlush = m_memoryLists.Count( m_lruList );
	std::vector< void * > destroyList;
	destroyList.reserve( nFlush );

	unsigned nBytesInitial = MemUsed_Inline();

	int node = m_memoryLists.Head(m_lruList);
	while ( node != m_memoryLists.InvalidIndex() )
	{
		int next = m_memoryLists.Next(node);
		m_memoryLists.Unlink( m_lruList, node );
		destroyList.push_back( GetForFreeByIndex( node ) );
		node = next;
	}

	unlock();

	for ( int i = 0; i < nFlush; i++ )
	{
		DestroyResourceStorage( destroyList[i] );
	}

	return ( nBytesInitial - MemUsed_Inline() );
}

unsigned int CDataManagerBase::FlushToTargetSize()
{
	return EnsureCapacity(0);
}

// Frees everything!  The LRU AND the LOCKED items.  This is only used to forcibly free the resources,
// not to make space.

unsigned int CDataManagerBase::FlushAll()
{
	lock();

	int nFlush = m_memoryLists.Count( m_lruList ) + m_memoryLists.Count( m_lockList );
	std::vector< void * > destroyList;
	destroyList.reserve( nFlush );

	unsigned result = MemUsed_Inline();
	int node;
	int nextNode;

	node = m_memoryLists.Head(m_lruList);
	while ( node != m_memoryLists.InvalidIndex() )
	{
		nextNode = m_memoryLists.Next(node);
		m_memoryLists.Unlink( m_lruList, node );
		destroyList.push_back( GetForFreeByIndex( node ) );
		node = nextNode;
	}

	node = m_memoryLists.Head(m_lockList);
	while ( node != m_memoryLists.InvalidIndex() )
	{
		nextNode = m_memoryLists.Next(node);
		m_memoryLists.Unlink( m_lockList, node );
		m_memoryLists[node].lockCount = 0;
		destroyList.push_back( GetForFreeByIndex( node ) );
		node = nextNode;
	}

	m_listsAreFreed = false;
	unlock();

	for ( int i = 0; i < nFlush; i++ )
	{
		DestroyResourceStorage( destroyList[i] );
	}

	return result;
}

unsigned int CDataManagerBase::Purge( unsigned int nBytesToPurge )
{
	unsigned int nTargetSize = MemUsed_Inline() - nBytesToPurge;
	if ( nBytesToPurge > MemUsed_Inline() )
		nTargetSize = 0;
	unsigned int nImpliedCapacity = MemTotal_Inline() - nTargetSize;
	return EnsureCapacity( nImpliedCapacity );
}


void CDataManagerBase::DestroyResource( memhandle_t handle )
{
	lock();
	unsigned short index = FromHandle( handle );
	if ( !m_memoryLists.IsValidIndex(index) )
	{
		unlock();
		return;
	}
	
	Assert( m_memoryLists[index].lockCount == 0  );
	if ( m_memoryLists[index].lockCount )
		BreakLock( handle );
	m_memoryLists.Unlink( m_lruList, index );
	void *p = GetForFreeByIndex( index );
	unlock();

	DestroyResourceStorage( p );
}


void *CDataManagerBase::LockResource( memhandle_t handle )
{
	std::scoped_lock lock( *this );
	unsigned short memoryIndex = FromHandle(handle);
	if ( memoryIndex != m_memoryLists.InvalidIndex() )
	{
		if ( m_memoryLists[memoryIndex].lockCount == 0 )
		{
			m_memoryLists.Unlink( m_lruList, memoryIndex );
			m_memoryLists.LinkToTail( m_lockList, memoryIndex );
		}
		Assert(m_memoryLists[memoryIndex].lockCount != (unsigned short)-1);
		m_memoryLists[memoryIndex].lockCount++;
		return m_memoryLists[memoryIndex].pStore;
	}

	return nullptr;
}

void *CDataManagerBase::LockResourceReturnCount( int *pCount, memhandle_t handle )
{
	std::scoped_lock lock( *this );
	unsigned short memoryIndex = FromHandle(handle);
	if ( memoryIndex != m_memoryLists.InvalidIndex() )
	{
		if ( m_memoryLists[memoryIndex].lockCount == 0 )
		{
			m_memoryLists.Unlink( m_lruList, memoryIndex );
			m_memoryLists.LinkToTail( m_lockList, memoryIndex );
		}
		Assert(m_memoryLists[memoryIndex].lockCount != (unsigned short)-1);
		*pCount = ++m_memoryLists[memoryIndex].lockCount;
		return m_memoryLists[memoryIndex].pStore;
	}

	*pCount = 0;
	return nullptr;
}

int CDataManagerBase::UnlockResource( memhandle_t handle )
{
	std::scoped_lock lock( *this );
	unsigned short memoryIndex = FromHandle(handle);
	if ( memoryIndex != m_memoryLists.InvalidIndex() )
	{
		Assert( m_memoryLists[memoryIndex].lockCount > 0 );
		if ( m_memoryLists[memoryIndex].lockCount > 0 )
		{
			m_memoryLists[memoryIndex].lockCount--;
			if ( m_memoryLists[memoryIndex].lockCount == 0 )
			{
				m_memoryLists.Unlink( m_lockList, memoryIndex );
				m_memoryLists.LinkToTail( m_lruList, memoryIndex );
			}
		}
		return m_memoryLists[memoryIndex].lockCount;
	}

	return 0;
}

void *CDataManagerBase::GetResource_NoLockNoLRUTouch( memhandle_t handle )
{
	std::scoped_lock lock( *this );
	unsigned short memoryIndex = FromHandle(handle);
	if ( memoryIndex != m_memoryLists.InvalidIndex() )
	{
		return m_memoryLists[memoryIndex].pStore;
	}
	return nullptr;
}


void *CDataManagerBase::GetResource_NoLock( memhandle_t handle )
{
	std::scoped_lock lock( *this );
	unsigned short memoryIndex = FromHandle(handle);
	if ( memoryIndex != m_memoryLists.InvalidIndex() )
	{
		TouchByIndex( memoryIndex );
		return m_memoryLists[memoryIndex].pStore;
	}
	return nullptr;
}

void CDataManagerBase::TouchResource( memhandle_t handle )
{
	std::scoped_lock lock( *this );
	TouchByIndex( FromHandle(handle) );
}

void CDataManagerBase::MarkAsStale( memhandle_t handle )
{
	std::scoped_lock lock( *this );
	unsigned short memoryIndex = FromHandle(handle);
	if ( memoryIndex != m_memoryLists.InvalidIndex() )
	{
		if ( m_memoryLists[memoryIndex].lockCount == 0 )
		{
			m_memoryLists.Unlink( m_lruList, memoryIndex );
			m_memoryLists.LinkToHead( m_lruList, memoryIndex );
		}
	}
}

int CDataManagerBase::BreakLock( memhandle_t handle )
{
	std::scoped_lock lock( *this );
	unsigned short memoryIndex = FromHandle(handle);
	if ( memoryIndex != m_memoryLists.InvalidIndex() && m_memoryLists[memoryIndex].lockCount )
	{
		int nBroken = m_memoryLists[memoryIndex].lockCount;
		m_memoryLists[memoryIndex].lockCount = 0;
		m_memoryLists.Unlink( m_lockList, memoryIndex );
		m_memoryLists.LinkToTail( m_lruList, memoryIndex );

		return nBroken;
	}
	return 0;
}

int CDataManagerBase::BreakAllLocks()
{
	std::scoped_lock lock( *this );
	int nBroken = 0;
	int node;
	int nextNode;

	node = m_memoryLists.Head(m_lockList);
	while ( node != m_memoryLists.InvalidIndex() )
	{
		nBroken++;
		nextNode = m_memoryLists.Next(node);
		m_memoryLists[node].lockCount = 0;
		m_memoryLists.Unlink( m_lockList, node );
		m_memoryLists.LinkToTail( m_lruList, node );
		node = nextNode;
	}

	return nBroken;

}

unsigned short CDataManagerBase::CreateHandle( bool bCreateLocked )
{
	std::scoped_lock lock( *this );
	int memoryIndex = m_memoryLists.Head(m_freeList);
	ListId_t list = ( bCreateLocked ) ? m_lockList : m_lruList;
	if ( memoryIndex != m_memoryLists.InvalidIndex() )
	{
		m_memoryLists.Unlink( m_freeList, memoryIndex );
		m_memoryLists.LinkToTail( list, memoryIndex );
	}
	else
	{
		memoryIndex = m_memoryLists.AddToTail( list );
	}

	if ( bCreateLocked )
	{
		m_memoryLists[memoryIndex].lockCount++;
	}

	return memoryIndex;
}

memhandle_t CDataManagerBase::StoreResourceInHandle( unsigned short memoryIndex, void *pStore, unsigned int realSize )
{
	std::scoped_lock lock( *this );
	resource_lru_element_t &mem = m_memoryLists[memoryIndex];
	mem.pStore = pStore;
	m_memUsed += realSize;
	return ToHandle(memoryIndex);
}

void CDataManagerBase::TouchByIndex( unsigned short memoryIndex )
{
	if ( memoryIndex != m_memoryLists.InvalidIndex() )
	{
		if ( m_memoryLists[memoryIndex].lockCount == 0 )
		{
			m_memoryLists.Unlink( m_lruList, memoryIndex );
			m_memoryLists.LinkToTail( m_lruList, memoryIndex );
		}
	}
}

memhandle_t CDataManagerBase::ToHandle( unsigned short index )
{
	unsigned int hiword = m_memoryLists.Element(index).serial;
	hiword <<= 16;
	index++;
	return reinterpret_cast< memhandle_t >( (uintp)( hiword|index ) );
}

unsigned int CDataManagerBase::TargetSize() 
{ 
	return MemTotal_Inline(); 
}

unsigned int CDataManagerBase::AvailableSize()
{ 
	return MemAvailable_Inline(); 
}


unsigned int CDataManagerBase::UsedSize()
{ 
	return MemUsed_Inline(); 
}

// free resources until there is enough space to hold "size"
unsigned int CDataManagerBase::EnsureCapacity( unsigned int size )
{
	unsigned nBytesInitial = MemUsed_Inline();
	while ( MemUsed_Inline() > MemTotal_Inline() || MemAvailable_Inline() < size )
	{
		lock();
		int lruIndex = m_memoryLists.Head( m_lruList );
		if ( lruIndex == m_memoryLists.InvalidIndex() )
		{
			unlock();
			break;
		}
		m_memoryLists.Unlink( m_lruList, lruIndex );
		void *p = GetForFreeByIndex( lruIndex );
		unlock();
		DestroyResourceStorage( p );
	}
	return ( nBytesInitial - MemUsed_Inline() );
}

// free this resource and move the handle to the free list
void *CDataManagerBase::GetForFreeByIndex( unsigned short memoryIndex )
{
	void *p = nullptr;
	if ( memoryIndex != m_memoryLists.InvalidIndex() )
	{
		Assert( m_memoryLists[memoryIndex].lockCount == 0 );

		resource_lru_element_t &mem = m_memoryLists[memoryIndex];
		unsigned size = GetRealSize( mem.pStore );
		if ( size > m_memUsed )
		{
			ExecuteOnce( Warning( "Data manager 'used' memory incorrect\n" ) );
			size = m_memUsed;
		}
		m_memUsed -= size;
		p = mem.pStore;
		mem.pStore = nullptr;
		mem.serial++;
		m_memoryLists.LinkToTail( m_freeList, memoryIndex );
	}
	return p;
}

// get a list of everything in the LRU
void CDataManagerBase::GetLRUHandleList( std::vector< memhandle_t >& list )
{
	for ( int node = m_memoryLists.Tail(m_lruList);
			node != m_memoryLists.InvalidIndex();
			node = m_memoryLists.Previous(node) )
	{
		list.push_back( ToHandle( node ) );
	}
}

// get a list of everything locked
void CDataManagerBase::GetLockHandleList( std::vector< memhandle_t >& list )
{
	for ( int node = m_memoryLists.Head(m_lockList);
			node != m_memoryLists.InvalidIndex();
			node = m_memoryLists.Next(node) )
	{
		list.push_back( ToHandle( node ) );
	}
}

