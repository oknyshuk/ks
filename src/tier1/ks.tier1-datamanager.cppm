// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1:datamanager;

import std;
import ks.math;
export import :null_mutex;

export extern "C++" {

struct memhandle_t__;
using memhandle_t = memhandle_t__ *;

inline const memhandle_t INVALID_MEMHANDLE = (memhandle_t)0xffffffff;

class CDataManagerBase
{
public:

	// public API
	// -----------------------------------------------------------------------------
	// memhandle_t			CreateResource( params ) // implemented by derived class
	void					DestroyResource( memhandle_t handle );

	// type-safe implementation in derived class
	//void					*LockResource( memhandle_t handle );
	int						UnlockResource( memhandle_t handle );
	void					TouchResource( memhandle_t handle );
	void					MarkAsStale( memhandle_t handle );		// move to head of LRU

	int						LockCount( memhandle_t handle );
	int						BreakLock( memhandle_t handle );
	int						BreakAllLocks();

	// HACKHACK: For convenience - offers no lock protection 
	// type-safe implementation in derived class
	//void					*GetResource_NoLock( memhandle_t handle );

	unsigned int			TargetSize();
	unsigned int			AvailableSize();
	unsigned int			UsedSize();

	void					NotifySizeChanged( memhandle_t handle, unsigned int oldSize, unsigned int newSize );

	void					SetTargetSize( unsigned int targetSize );

	// NOTE: flush is equivalent to Destroy
	unsigned int			FlushAllUnlocked();
	unsigned int			FlushToTargetSize();
	unsigned int			FlushAll();
	unsigned int			Purge( unsigned int nBytesToPurge );
	unsigned int			EnsureCapacity( unsigned int size );

	// Thread lock
	virtual void			Lock() {}
	virtual bool			TryLock() { return true; }
	virtual void			Unlock() {}
	void					lock() { Lock(); }
	bool					try_lock() { return TryLock(); }
	void					unlock() { Unlock(); }

	// Iteration

	// -----------------------------------------------------------------------------

	void					SetFreeOnDestruct( bool value ) { m_freeOnDestruct = value; }

	// Debugging only!!!!
	void					GetLRUHandleList( std::vector< memhandle_t >& list );
	void					GetLockHandleList( std::vector< memhandle_t >& list );

protected:
	// derived class must call these to implement public API
	unsigned short			CreateHandle( bool bCreateLocked );
	memhandle_t				StoreResourceInHandle( unsigned short memoryIndex, void *pStore, unsigned int realSize );
	void					*GetResource_NoLock( memhandle_t handle );
	void					*GetResource_NoLockNoLRUTouch( memhandle_t handle );
	void					*LockResource( memhandle_t handle );
	void					*LockResourceReturnCount( int *pCount, memhandle_t handle );

	// NOTE: you must call this from the destructor of the derived class! (will assert otherwise)
	void					FreeAllLists()	{ FlushAll(); m_listsAreFreed = true; }

							CDataManagerBase( unsigned int maxSize );
	virtual					~CDataManagerBase();
	
	
	inline unsigned int		MemTotal_Inline() const { return m_targetMemorySize; }
	inline unsigned int		MemAvailable_Inline() const { return m_targetMemorySize - m_memUsed; }
	inline unsigned int		MemUsed_Inline() const { return m_memUsed; }

// Implemented by derived class:
	virtual void			DestroyResourceStorage( void * ) = 0;
	virtual unsigned int	GetRealSize( void * ) = 0;

	memhandle_t				ToHandle( unsigned short index );
	unsigned short			FromHandle( memhandle_t handle );
	
	void					TouchByIndex( unsigned short memoryIndex );
	void *					GetForFreeByIndex( unsigned short memoryIndex );

	// One of these is stored per handle slot. Slots are never released, so an index stays valid for the
	// manager's lifetime; serial is bumped on every free so stale handles stop resolving.
	struct resource_lru_element_t
	{
		unsigned short lockCount = 0;
		unsigned short serial = 1;
		void	*pStore = nullptr;
	};

	// Every slot sits in at most one of these lists; moving between them is O(1) under the lock.
	enum ListId_t : unsigned char { LRU_LIST, LOCK_LIST, FREE_LIST, NUM_LISTS, NO_LIST = 0xFF };

	class CMemoryLists
	{
	public:
		static constexpr unsigned short INVALID_INDEX = 0xFFFF;
		static unsigned short InvalidIndex()			{ return INVALID_INDEX; }

		bool IsValidIndex( unsigned short i ) const		{ return i < m_Slots.size(); }
		resource_lru_element_t &operator[]( unsigned short i )	{ return m_Slots[i].elem; }
		const resource_lru_element_t &operator[]( unsigned short i ) const	{ return m_Slots[i].elem; }
		resource_lru_element_t &Element( unsigned short i )		{ return m_Slots[i].elem; }

		unsigned short Head( ListId_t list ) const		{ return m_Lists[list].head; }
		unsigned short Tail( ListId_t list ) const		{ return m_Lists[list].tail; }
		int Count( ListId_t list ) const				{ return m_Lists[list].count; }
		unsigned short Next( unsigned short i ) const	{ return m_Slots[i].next; }
		unsigned short Previous( unsigned short i ) const { return m_Slots[i].prev; }

		// New slot at the tail of list.
		unsigned short AddToTail( ListId_t list )
		{
			if ( m_Slots.size() >= INVALID_INDEX )
				Error( "CDataManager overflow! (exhausted index range)\n" );
			unsigned short i = (unsigned short)m_Slots.size();
			m_Slots.emplace_back();
			LinkToTail( list, i );
			return i;
		}

		// No-op unless i is in list.
		void Unlink( ListId_t list, unsigned short i )
		{
			Slot &s = m_Slots[i];
			if ( s.list != list )
			{
				contract_assert( s.list == NO_LIST );
				return;
			}
			List &l = m_Lists[list];
			( s.prev != INVALID_INDEX ? m_Slots[s.prev].next : l.head ) = s.next;
			( s.next != INVALID_INDEX ? m_Slots[s.next].prev : l.tail ) = s.prev;
			s.prev = s.next = INVALID_INDEX;
			s.list = NO_LIST;
			--l.count;
		}

		void LinkToTail( ListId_t list, unsigned short i )
		{
			Unlink( list, i );
			Slot &s = m_Slots[i];
			contract_assert( s.list == NO_LIST );
			List &l = m_Lists[list];
			s.prev = l.tail;
			s.next = INVALID_INDEX;
			( l.tail != INVALID_INDEX ? m_Slots[l.tail].next : l.head ) = i;
			l.tail = i;
			s.list = list;
			++l.count;
		}

		void LinkToHead( ListId_t list, unsigned short i )
		{
			Unlink( list, i );
			Slot &s = m_Slots[i];
			contract_assert( s.list == NO_LIST );
			List &l = m_Lists[list];
			s.prev = INVALID_INDEX;
			s.next = l.head;
			( l.head != INVALID_INDEX ? m_Slots[l.head].prev : l.tail ) = i;
			l.head = i;
			s.list = list;
			++l.count;
		}

	private:
		struct Slot
		{
			resource_lru_element_t elem;
			unsigned short prev = INVALID_INDEX;
			unsigned short next = INVALID_INDEX;
			ListId_t list = NO_LIST;
		};
		struct List
		{
			unsigned short head = INVALID_INDEX;
			unsigned short tail = INVALID_INDEX;
			int count = 0;
		};
		std::vector< Slot > m_Slots;
		List m_Lists[NUM_LISTS];
	};

	unsigned int m_targetMemorySize;
	unsigned int m_memUsed;
	
	CMemoryLists m_memoryLists;
	
	static constexpr ListId_t m_lruList = LRU_LIST;
	static constexpr ListId_t m_lockList = LOCK_LIST;
	static constexpr ListId_t m_freeList = FREE_LIST;
	unsigned short m_listsAreFreed : 1;
	unsigned short m_freeOnDestruct : 1;
	unsigned short m_unused : 14;

};

template< class STORAGE_TYPE, class CREATE_PARAMS, class LOCK_TYPE = STORAGE_TYPE *, class MUTEX_TYPE = ks::null_mutex>
class CDataManager : public CDataManagerBase
{
	typedef CDataManagerBase BaseClass;
public:

	CDataManager( unsigned int size = (unsigned)-1 ) : BaseClass(size) {}

	~CDataManager()
	{
		// NOTE: This must be called in all implementations of CDataManager
		if ( m_freeOnDestruct )
		{
			FreeAllLists();
		}
	}

	// Use GetData() to translate pointer to LOCK_TYPE
	LOCK_TYPE LockResource( memhandle_t hMem )
	{
		void *pLock = BaseClass::LockResource( hMem );
		if ( pLock )
		{
			return StoragePointer(pLock)->GetData();
		}

		return 0;
	}

	LOCK_TYPE LockResourceReturnCount( int *pCount, memhandle_t hMem )
	{
		void *pLock = BaseClass::LockResourceReturnCount( pCount, hMem );
		if ( pLock )
		{
			return StoragePointer(pLock)->GetData();
		}

		return nullptr;
	}

	// Use GetData() to translate pointer to LOCK_TYPE
	LOCK_TYPE GetResource_NoLock( memhandle_t hMem )
	{
		void *pLock = const_cast<void *>(BaseClass::GetResource_NoLock( hMem ));
		if ( pLock )
		{
			return StoragePointer(pLock)->GetData();
		}
		return nullptr;
	}

	// Use GetData() to translate pointer to LOCK_TYPE
	// Doesn't touch the memory LRU
	LOCK_TYPE GetResource_NoLockNoLRUTouch( memhandle_t hMem )
	{
		void *pLock = const_cast<void *>(BaseClass::GetResource_NoLockNoLRUTouch( hMem ));
		if ( pLock )
		{
			return StoragePointer(pLock)->GetData();
		}
		return nullptr;
	}

	// Wrapper to match implementation of allocation with typed storage & alloc params.
	memhandle_t CreateResource( const CREATE_PARAMS &createParams, bool bCreateLocked = false )
	{
		BaseClass::EnsureCapacity(STORAGE_TYPE::EstimatedSize(createParams));
		STORAGE_TYPE *pStore = STORAGE_TYPE::CreateResource( createParams );
		std::scoped_lock lock( *this );
		unsigned short memoryIndex = BaseClass::CreateHandle( bCreateLocked );
		return BaseClass::StoreResourceInHandle( memoryIndex, pStore, pStore->Size() );
	}

	// Iteration. Must lock first
	memhandle_t GetFirstUnlocked()
	{
		unsigned node = m_memoryLists.Head(m_lruList);
		if ( node == m_memoryLists.InvalidIndex() )
		{
			return INVALID_MEMHANDLE;
		}
		return ToHandle( node );
	}

	memhandle_t GetFirstLocked()
	{
		unsigned node = m_memoryLists.Head(m_lockList);
		if ( node == m_memoryLists.InvalidIndex() )
		{
			return INVALID_MEMHANDLE;
		}
		return ToHandle( node );
	}

	memhandle_t GetNext( memhandle_t hPrev )
	{
		if ( hPrev == INVALID_MEMHANDLE )
		{
			return INVALID_MEMHANDLE;
		}

		unsigned short iPrev = FromHandle( hPrev );
		if ( iPrev == m_memoryLists.InvalidIndex() )
		{
			return INVALID_MEMHANDLE;
		}

		unsigned short iNext = m_memoryLists.Next( iPrev );
		if ( iNext == m_memoryLists.InvalidIndex() ) 
		{
			return INVALID_MEMHANDLE;
		}

		return ToHandle( iNext );
	}

	MUTEX_TYPE &AccessMutex()	{ return m_mutex; }
	virtual void Lock() { m_mutex.lock(); }
	virtual bool TryLock() { return m_mutex.try_lock(); }
	virtual void Unlock() { m_mutex.unlock(); }

private:
	STORAGE_TYPE *StoragePointer( void *pMem )
	{
		return static_cast<STORAGE_TYPE *>(pMem);
	}

	virtual void DestroyResourceStorage( void *pStore )
	{
		StoragePointer(pStore)->DestroyResource();
	}
	
	virtual unsigned int GetRealSize( void *pStore )
	{
		return StoragePointer(pStore)->Size();
	}

	MUTEX_TYPE m_mutex;
};

//-----------------------------------------------------------------------------

inline unsigned short CDataManagerBase::FromHandle( memhandle_t handle )
{
	unsigned int fullWord = (unsigned int)reinterpret_cast<uintp>( handle );
	unsigned short serial = fullWord>>16;
	unsigned short index = fullWord & 0xFFFF;
	index--;
	if ( m_memoryLists.IsValidIndex(index) && m_memoryLists[index].serial == serial )
		return index;
	return m_memoryLists.InvalidIndex();
}

inline int CDataManagerBase::LockCount( memhandle_t handle )
{
	Lock();
	int result = 0;
	unsigned short memoryIndex = FromHandle(handle);
	if ( memoryIndex != m_memoryLists.InvalidIndex() )
	{
		result = m_memoryLists[memoryIndex].lockCount;
	}
	Unlock();
	return result;
}

}
