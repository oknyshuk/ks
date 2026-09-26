// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// Fixed-size block pools. Blocks live in blobs that only clear() releases, so addresses stay
// stable (some callers use them as handles). alloc/free are an O(1) intrusive free list; std::pmr's
// pool resources are slower on both (size-class lookup, chunk search on deallocate), have no
// budget (grow::none) or live count, and cannot enumerate live blocks for object_pool::clear.

#ifndef FIXED_POOL_H
#define FIXED_POOL_H

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <vector>

#include "tier0/dbg.h"
#include "tier0/memalloc.h"
#include "tier0/threadtools.h"

namespace ks
{
	class fixed_pool
	{
	public:
		enum class grow
		{
			none,	// one blob; alloc returns nullptr once it is used up
			fast,	// blob n holds n * nBlocksPerBlob blocks
			slow,	// every blob holds nBlocksPerBlob blocks
		};

		fixed_pool( size_t nBlockSize, int nBlocksPerBlob, grow eGrow = grow::fast, const char *pszOwner = nullptr, size_t nAlign = alignof( void * ) );
		~fixed_pool()								{ clear(); }
		fixed_pool( const fixed_pool & ) = delete;
		fixed_pool &operator=( const fixed_pool & ) = delete;

		void *alloc()
		{
			if ( !m_pFree && !add_blob() )
				return nullptr;
			void *p = m_pFree;
			m_pFree = *static_cast< void ** >( p );
			m_nPeak = std::max( m_nPeak, ++m_nLive );
			return p;
		}
		// nullptr when n exceeds the block size (a derived class allocating from its base's pool).
		void *alloc( size_t n )						{ return n <= m_nBlockSize ? alloc() : nullptr; }
		void *alloc_zero()
		{
			void *p = alloc();
			if ( p )
				std::memset( p, 0, m_nBlockSize );
			return p;
		}

		void free( void *p )
		{
			if ( !p )
				return;
#ifdef _DEBUG
			Assert( owns( p ) );
			std::memset( p, 0xDD, m_nBlockSize );
#endif
			*static_cast< void ** >( p ) = m_pFree;
			m_pFree = p;
			--m_nLive;
		}

		// Releases every blob; outstanding blocks become invalid.
		void clear();

		int count() const							{ return m_nLive; }
		int peak_count() const						{ return m_nPeak; }
		size_t block_size() const					{ return m_nBlockSize; }
		size_t size() const;						// bytes held in blobs
		bool owns( const void *p ) const;			// p is the start of a block of this pool

		// Calls f( void * ) for each block that is currently allocated.
		template < class F >
		void for_each_live( F &&f ) const
		{
			std::vector< const void * > free;
			for ( void *p = m_pFree; p; p = *static_cast< void ** >( p ) )
				free.push_back( p );
			std::sort( free.begin(), free.end() );
			for ( size_t i = 0; i < m_Blobs.size(); ++i )
			{
				std::byte *p = m_Blobs[i].get();
				for ( int n = blob_blocks( i ); n--; p += m_nBlockSize )
					if ( !std::binary_search( free.begin(), free.end(), (const void *)p ) )
						f( (void *)p );
			}
		}

	private:
		struct blob_delete
		{
			size_t nAlign;
			void operator()( std::byte *p ) const;
		};

		bool add_blob();
		int blob_blocks( size_t i ) const			{ return m_eGrow == grow::fast ? m_nBlocksPerBlob * int( i + 1 ) : m_nBlocksPerBlob; }

		void *m_pFree = nullptr;
		int m_nLive = 0;
		int m_nPeak = 0;
		size_t m_nBlockSize;
		size_t m_nAlign;
		int m_nBlocksPerBlob;
		grow m_eGrow;
		const char *m_pszOwner;
		std::vector< std::unique_ptr< std::byte[], blob_delete > > m_Blobs;
	};

	struct fast_mutex
	{
		void lock()			{ m.lock(); }
		void unlock()		{ m.unlock(); }
		std::recursive_mutex m;
	};

	template < class Mutex >
	class basic_fixed_pool_mt
	{
	public:
		template < class... A >
		explicit basic_fixed_pool_mt( A &&...args ) : m_Pool( std::forward< A >( args )... ) {}

		void *alloc()								{ std::lock_guard l( m_Mutex ); return m_Pool.alloc(); }
		void *alloc( size_t n )						{ std::lock_guard l( m_Mutex ); return m_Pool.alloc( n ); }
		void *alloc_zero()							{ std::lock_guard l( m_Mutex ); return m_Pool.alloc_zero(); }
		void free( void *p )						{ std::lock_guard l( m_Mutex ); m_Pool.free( p ); }
		void clear()								{ std::lock_guard l( m_Mutex ); m_Pool.clear(); }

		int count() const							{ return m_Pool.count(); }
		int peak_count() const						{ return m_Pool.peak_count(); }
		size_t block_size() const					{ return m_Pool.block_size(); }
		size_t size()								{ std::lock_guard l( m_Mutex ); return m_Pool.size(); }

	private:
		Mutex m_Mutex;
		fixed_pool m_Pool;
	};
	using fixed_pool_mt = basic_fixed_pool_mt< fast_mutex >;

	// Typed pool: alloc() default-constructs, free() destroys, clear() destroys the live objects.
	template < class T >
	class object_pool
	{
	public:
		explicit object_pool( int nBlocksPerBlob, fixed_pool::grow eGrow = fixed_pool::grow::fast, size_t nAlign = alignof( T ) )
			: m_Pool( sizeof( T ), nBlocksPerBlob, eGrow, MEM_ALLOC_CLASSNAME( T ), std::max( nAlign, alignof( T ) ) ) {}
		// Live objects are not destroyed here: pools are statics outliving what their objects reference.
		~object_pool() = default;

		T *alloc()
		{
			void *p = m_Pool.alloc();
			return p ? ::new( p ) T : nullptr;
		}
		// Zero-fills before construction, for types relying on zeroed members.
		T *alloc_zero()
		{
			void *p = m_Pool.alloc_zero();
			return p ? ::new( p ) T : nullptr;
		}
		void free( T *p )
		{
			if ( !p )
				return;
			p->~T();
			m_Pool.free( p );
		}
		void clear()
		{
			m_Pool.for_each_live( []( void *p ) { static_cast< T * >( p )->~T(); } );
			m_Pool.clear();
		}

		int count() const							{ return m_Pool.count(); }
		int peak_count() const						{ return m_Pool.peak_count(); }

	private:
		fixed_pool m_Pool;
	};

	// ItemCount blocks in one lazily allocated arena, overflowing to the heap. Thread-safe.
	template < size_t ItemSize, int ItemCount >
	class budget_pool
	{
	public:
		static constexpr size_t item_size = ( ItemSize + 15 ) & ~size_t( 15 );

		budget_pool() = default;
		~budget_pool()								{ clear(); }
		budget_pool( const budget_pool & ) = delete;
		budget_pool &operator=( const budget_pool & ) = delete;

		bool owns( const void *p ) const
		{
			const std::byte *pBase = m_pBase.load( std::memory_order_acquire );
			return pBase && p >= pBase && p < pBase + ItemCount * item_size;
		}

		void *alloc()
		{
			MEM_ALLOC_CREDIT_CLASS();
			{
				std::lock_guard l( m_Mutex );
				if ( !m_pBase.load( std::memory_order_relaxed ) )
				{
					std::byte *pBase = static_cast< std::byte * >( ::operator new( ItemCount * item_size, std::align_val_t( 16 ) ) );
					for ( int i = ItemCount; i--; )
						push( pBase + i * item_size );
					m_pBase.store( pBase, std::memory_order_release );
				}
				if ( void *p = m_pFree )
				{
					m_pFree = *static_cast< void ** >( p );
					--m_nFree;
					return p;
				}
			}
			return ::operator new( item_size, std::align_val_t( 16 ) );
		}

		void free( void *p )
		{
			if ( !p )
				return;
			if ( owns( p ) )
			{
				std::lock_guard l( m_Mutex );
				push( p );
			}
			else
				::operator delete( p, std::align_val_t( 16 ) );
		}

		// Releases the arena; only valid while no arena block is in use.
		void clear()
		{
			std::lock_guard l( m_Mutex );
			if ( std::byte *pBase = m_pBase.exchange( nullptr ) )
				::operator delete( pBase, std::align_val_t( 16 ) );
			m_pFree = nullptr;
			m_nFree = 0;
		}

		// No arena block is in use.
		bool empty()
		{
			std::lock_guard l( m_Mutex );
			return !m_pBase.load( std::memory_order_relaxed ) || m_nFree == ItemCount;
		}
		int free_count()							{ std::lock_guard l( m_Mutex ); return m_nFree; }

	private:
		void push( void *p )
		{
			*static_cast< void ** >( p ) = m_pFree;
			m_pFree = p;
			++m_nFree;
		}

		std::mutex m_Mutex;
		std::atomic< std::byte * > m_pBase = nullptr;
		void *m_pFree = nullptr;
		int m_nFree = 0;
	};

	// Recycles heap objects instead of deleting them; get() hands back a released object as it was
	// left, or a new one. Owns the idle objects. Thread-safe.
	template < class T >
	class reuse_pool
	{
	public:
		T *get()									{ T *p = take(); return p ? p : new T; }
		// An idle object, or nullptr.
		T *take()
		{
			std::lock_guard l( m_Mutex );
			if ( m_Idle.empty() )
				return nullptr;
			T *p = m_Idle.back().release();
			m_Idle.pop_back();
			return p;
		}
		void put( T *p )							{ std::lock_guard l( m_Mutex ); m_Idle.emplace_back( p ); }
		int idle_count()							{ std::lock_guard l( m_Mutex ); return (int)m_Idle.size(); }

	private:
		std::mutex m_Mutex;
		std::vector< std::unique_ptr< T > > m_Idle;
	};
}

//-----------------------------------------------------------------------------
// Make a class allocate from its own pool: DECLARE_FIXEDSIZE_ALLOCATOR in the class,
// DEFINE_FIXEDSIZE_ALLOCATOR( class, blocks per blob, ks::fixed_pool::grow::... ) in a .cpp.
//-----------------------------------------------------------------------------
#define DECLARE_FIXEDSIZE_ALLOCATOR_( _class, _pool )															\
	public:																										\
		inline void* operator new( size_t size ) { MEM_ALLOC_CREDIT_(#_class " pool"); return s_Allocator.alloc(size); }   \
		inline void* operator new( size_t size, int nBlockUse, const char *pFileName, int nLine ) { MEM_ALLOC_CREDIT_(#_class " pool"); return s_Allocator.alloc(size); }   \
		inline void  operator delete( void* p ) { s_Allocator.free(p); }										\
		inline void  operator delete( void* p, int nBlockUse, const char *pFileName, int nLine ) { s_Allocator.free(p); }   \
	private:																									\
		static _pool s_Allocator

#define DECLARE_FIXEDSIZE_ALLOCATOR( _class )		DECLARE_FIXEDSIZE_ALLOCATOR_( _class, ks::fixed_pool )
#define DECLARE_FIXEDSIZE_ALLOCATOR_MT( _class )	DECLARE_FIXEDSIZE_ALLOCATOR_( _class, ks::fixed_pool_mt )

#define DEFINE_FIXEDSIZE_ALLOCATOR( _class, _initsize, _grow )					\
	ks::fixed_pool _class::s_Allocator( sizeof(_class), _initsize, _grow, #_class " pool", alignof(_class) )

#define DEFINE_FIXEDSIZE_ALLOCATOR_MT( _class, _initsize, _grow )				\
	ks::fixed_pool_mt _class::s_Allocator( sizeof(_class), _initsize, _grow, #_class " pool", alignof(_class) )

#endif // FIXED_POOL_H
