// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.core.tslist;

import std;
import ks.platform;
import ks.core.basetypes;
import ks.core.platform;
import ks.core.types;
import ks.core.color;
import ks.core.logging;
import ks.core.dbg;
import ks.core.memalloc;
import ks.core.threadtools;

export extern "C++" {

//-----------------------------------------------------------------------------

typedef __int128_t int128;

inline constexpr auto TSLIST_HEAD_ALIGNMENT = 16;
inline constexpr auto TSLIST_NODE_ALIGNMENT = 16;

inline bool ThreadInterlockedAssignIf128( int128 volatile * pDest, const int128 &value, const int128 &comparand )
{
	// We do not want the original comparand modified by the swap so operate on a local copy.
	int128 local_comparand = comparand;
	return __sync_bool_compare_and_swap( pDest, local_comparand, value );
}

inline bool ThreadInterlockedAssignIf64x128( volatile int128 *pDest, const int128 &value, const int128 &comperand )
{
	return ThreadInterlockedAssignIf128( pDest, value, comperand );
}

//-----------------------------------------------------------------------------

bool RunTSQueueTests( int nListSize = 10000, int nTests = 1 );
bool RunTSListTests( int nListSize = 10000, int nTests = 1 );

//-----------------------------------------------------------------------------
// Lock free list.
//-----------------------------------------------------------------------------
//#define USE_NATIVE_SLIST

struct alignas(TSLIST_NODE_ALIGNMENT) TSLNodeBase_t
{
	TSLNodeBase_t *Next; // name to match Windows
};

union alignas(TSLIST_HEAD_ALIGNMENT) TSLHead_t
{
	struct Value_t
	{
		TSLNodeBase_t *Next;
		// <sergiy> Depth must be in the least significant halfword when atomically loading into register,
		//          to avoid carrying digits from Sequence. Carrying digits from Depth to Sequence is ok,
		//          because Sequence can be pretty much random. We could operate on both of them separately,
		//          but it could perhaps (?) lead to problems with store forwarding. I don't know 'cause I didn't 
		//          performance-test or design original code, I'm just making it work on PowerPC.
		int16   Depth;
		int16	Sequence;
		int32   Padding;
	} value;

	struct Value32_t
	{
		TSLNodeBase_t *Next_do_not_use_me;
		int32   DepthAndSequence;
	} value32;

	int128 value64x128;
};

//-------------------------------------
class alignas(TSLIST_HEAD_ALIGNMENT) CTSListBase
{
public:

	// override new/delete so we can guarantee 8-byte aligned allocs
	static void * operator new(std::size_t size)
	{
		CTSListBase *pNode = (CTSListBase *)MemAlloc_AllocAlignedFileLine( size, TSLIST_HEAD_ALIGNMENT, __FILE__, __LINE__ );
		return pNode;
	}

	static void * operator new(std::size_t size, int nBlockUse, const char *pFileName, int nLine)
	{
		CTSListBase *pNode = (CTSListBase *)MemAlloc_AllocAlignedFileLine( size, TSLIST_HEAD_ALIGNMENT, pFileName, nLine );
		return pNode;
	}

	static void operator delete(void *p)
	{
		MemAlloc_FreeAligned( p );
	}

	static void operator delete(void *p, int nBlockUse, const char *pFileName, int nLine)
	{
		MemAlloc_FreeAligned( p );
	}

private:
	// These ain't gonna work
	static void * operator new[]( std::size_t size );
	static void operator delete[]( void *p );

public:

	CTSListBase()
	{
		if ( ((std::size_t)&m_Head) % TSLIST_HEAD_ALIGNMENT != 0 )
		{
			Error( "CTSListBase: Misaligned list\n" );
			DebuggerBreak();
		}

		m_Head.value64x128 = 0;
	}

	~CTSListBase()
	{
		Detach();
	}

	TSLNodeBase_t *Push( TSLNodeBase_t *pNode )
	{

		TSLHead_t oldHead;
		TSLHead_t newHead;

		newHead.value.Padding = 0;
		for ( ;; )
		{
			// Atomic: the head is written by the 128-bit CAS below, so a plain load of it is UB.
			oldHead.value64x128 = __atomic_load_n( &m_Head.value64x128, __ATOMIC_SEQ_CST );
			pNode->Next = oldHead.value.Next;
			newHead.value.Next = pNode;

			newHead.value32.DepthAndSequence = oldHead.value32.DepthAndSequence + 0x10001;

			if ( ThreadInterlockedAssignIf64x128( &m_Head.value64x128, newHead.value64x128, oldHead.value64x128 ) )
			{
				break;
			}
			ThreadPause();
		};

		return (TSLNodeBase_t *)oldHead.value.Next;
	}

	TSLNodeBase_t *Pop()
	{
		TSLHead_t oldHead;
		TSLHead_t newHead;

		newHead.value.Padding = 0;
		for ( ;; )
		{
			// Atomic: the head is written by the 128-bit CAS below, so a plain load of it is UB.
			oldHead.value64x128 = __atomic_load_n( &m_Head.value64x128, __ATOMIC_SEQ_CST );
			if ( !oldHead.value.Next )
				return nullptr;

			newHead.value.Next = oldHead.value.Next->Next;
			newHead.value32.DepthAndSequence = oldHead.value32.DepthAndSequence - 1;

			if ( ThreadInterlockedAssignIf64x128( &m_Head.value64x128, newHead.value64x128, oldHead.value64x128 ) )
			{
				break;
			}
			ThreadPause();
		};

		return (TSLNodeBase_t *)oldHead.value.Next;
	}

	TSLNodeBase_t *Detach()
	{
		TSLHead_t oldHead;
		TSLHead_t newHead;

		newHead.value.Padding = 0;
		do
		{
			ThreadPause();

			// Atomic: the head is written by the 128-bit CAS below, so a plain load of it is UB.
			oldHead.value64x128 = __atomic_load_n( &m_Head.value64x128, __ATOMIC_SEQ_CST );
			if ( !oldHead.value.Next )
				return nullptr;

			newHead.value.Next = nullptr;
			// <sergiy> the reason for AND'ing it instead of poking a short into memory 
			//          is probably to avoid store forward issues, but I'm not sure because
			//          I didn't construct this code. In any case, leaving it as is on big-endian
			newHead.value32.DepthAndSequence = oldHead.value32.DepthAndSequence & 0xffff0000;

		} while ( !ThreadInterlockedAssignIf64x128( &m_Head.value64x128, newHead.value64x128, oldHead.value64x128 ) );

		return (TSLNodeBase_t *)oldHead.value.Next;
	}

	TSLHead_t *AccessUnprotected()
	{
		return &m_Head;
	}

	int Count() const
	{
		return m_Head.value.Depth;
	}

private:
	TSLHead_t m_Head;
};

//-------------------------------------

template <typename T>
class alignas(TSLIST_HEAD_ALIGNMENT) CTSSimpleList : public CTSListBase
{
public:
	void Push( T *pNode )
	{
		contract_assert( sizeof(T) >= sizeof(TSLNodeBase_t) );
		CTSListBase::Push( (TSLNodeBase_t *)pNode );
	}

	T *Pop()
	{
		return (T *)CTSListBase::Pop();
	}
};

//-------------------------------------
// this is a replacement for CTSList<> and CObjectPool<> that does not
// have a per-item, per-alloc new/delete overhead
// similar to CTSSimpleList except that it allocates it's own pool objects
// and frees them on destruct.  Also it does not overlay the TSNodeBase_t memory
// on T's memory
template< class T >
class alignas(TSLIST_HEAD_ALIGNMENT) CTSPool : public CTSListBase
{
	// packs the node and the item (T) into a single struct and pools those
	struct alignas(TSLIST_NODE_ALIGNMENT) simpleTSPoolStruct_t : public TSLNodeBase_t
	{
		T elem;
	};

public:

	~CTSPool()
	{
		Purge();
	}

	void Purge()
	{
		simpleTSPoolStruct_t *pNode = nullptr;
		while ( 1 )
		{
			pNode = (simpleTSPoolStruct_t *)CTSListBase::Pop();
			if ( !pNode )
				break;
			delete pNode;
		}
	}

	void PutObject( T *pInfo )
	{
		char *pElem = (char *)pInfo;
		pElem -= __builtin_offsetof( simpleTSPoolStruct_t, elem );
		simpleTSPoolStruct_t *pNode = (simpleTSPoolStruct_t *)pElem;

		CTSListBase::Push( pNode );
	}

	T *GetObject()
	{
		simpleTSPoolStruct_t *pNode = (simpleTSPoolStruct_t *)CTSListBase::Pop();
		if ( !pNode )
		{
			pNode = new simpleTSPoolStruct_t;
		}
		return &pNode->elem;
	}

	// omg windows sdk - why do you #define GetObject()?
	[[gnu::always_inline]] inline T *Get()
	{
		return GetObject();
	}
};
//-------------------------------------

template <typename T>
class alignas(TSLIST_HEAD_ALIGNMENT) CTSList : public CTSListBase
{
public:
	struct alignas(TSLIST_NODE_ALIGNMENT) Node_t : public TSLNodeBase_t
	{
		Node_t() {}
		Node_t( const T &init ) : elem( init ) {}
		T elem;

		// override new/delete so we can guarantee 8-byte aligned allocs
		static void * operator new(std::size_t size)
		{
			Node_t *pNode = (Node_t *)MemAlloc_AllocAlignedFileLine( size, TSLIST_NODE_ALIGNMENT, __FILE__, __LINE__ );
			return pNode;
		}

		// override new/delete so we can guarantee 8-byte aligned allocs
		static void * operator new(std::size_t size, int nBlockUse, const char *pFileName, int nLine)
		{
			Node_t *pNode = (Node_t *)MemAlloc_AllocAlignedFileLine( size, TSLIST_NODE_ALIGNMENT, pFileName, nLine );
			return pNode;
		}

		static void operator delete(void *p)
		{
			MemAlloc_FreeAligned( p );
		}
		static void operator delete(void *p, int nBlockUse, const char *pFileName, int nLine)
		{
			MemAlloc_FreeAligned( p );
		}

	};

	~CTSList()
	{
		Purge();
	}

	void Purge()
	{
		Node_t *pCurrent = Detach();
		Node_t *pNext;
		while ( pCurrent )
		{
			pNext = (Node_t *)pCurrent->Next;
			delete pCurrent;
			pCurrent = pNext;
		}
	}

	void RemoveAll()
	{
		Purge();
	}

	Node_t *Push( Node_t *pNode )
	{
		return (Node_t *)CTSListBase::Push( pNode );
	}

	Node_t *Pop()
	{
		return (Node_t *)CTSListBase::Pop();
	}

	void PushItem( const T &init )
	{
		Push( new Node_t( init ) );
	}

	bool PopItem( T *pResult )
	{
		Node_t *pNode = Pop();
		if ( !pNode )
			return false;
		*pResult = pNode->elem;
		delete pNode;
		return true;
	}

	Node_t *Detach()
	{
		return (Node_t *)CTSListBase::Detach();
	}

};

//-------------------------------------

template <typename T>
class alignas(TSLIST_HEAD_ALIGNMENT) CTSListWithFreeList : public CTSListBase
{
public:
	struct alignas(TSLIST_NODE_ALIGNMENT) Node_t : public TSLNodeBase_t
	{
		Node_t() {}
		Node_t( const T &init ) : elem( init ) {}

		T elem;
	};

	~CTSListWithFreeList()
	{
		Purge();
	}

	void Purge()
	{
		Node_t *pCurrent = Detach();
		Node_t *pNext;
		while ( pCurrent )
		{
			pNext = (Node_t *)pCurrent->Next;
			delete pCurrent;
			pCurrent = pNext;
		}
		pCurrent = (Node_t *)m_FreeList.Detach();
		while ( pCurrent )
		{
			pNext = (Node_t *)pCurrent->Next;
			delete pCurrent;
			pCurrent = pNext;
		}
	}

	void RemoveAll()
	{
		Node_t *pCurrent = Detach();
		Node_t *pNext;
		while ( pCurrent )
		{
			pNext = (Node_t *)pCurrent->Next;
			m_FreeList.Push( pCurrent );
			pCurrent = pNext;
		}
	}

	Node_t *Push( Node_t *pNode )
	{
		return (Node_t *)CTSListBase::Push( pNode );
	}

	Node_t *Pop()
	{
		return (Node_t *)CTSListBase::Pop();
	}

	void PushItem( const T &init )
	{
		Node_t *pNode = (Node_t *)m_FreeList.Pop();
		if ( !pNode )
		{
			pNode = new Node_t;
		}
		pNode->elem = init;
		Push( pNode );
	}

	bool PopItem( T *pResult )
	{
		Node_t *pNode = Pop();
		if ( !pNode )
			return false;
		*pResult = pNode->elem;
		m_FreeList.Push( pNode );
		return true;
	}

	Node_t *Detach()
	{
		return (Node_t *)CTSListBase::Detach();
	}

	void FreeNode( Node_t *pNode )
	{
		m_FreeList.Push( pNode );
	}

private:
	CTSListBase m_FreeList;
};

//-----------------------------------------------------------------------------
// Lock free queue
//
// A special consideration: the element type should be simple. This code
// actually dereferences freed nodes as part of pop, but later detects
// that. If the item in the queue is a complex type, only bad things can
// come of that. Also, therefore, if you're using Push/Pop instead of
// push item, be aware that the node memory cannot be freed until
// all threads that might have been popping have completed the pop.
// The PushItem()/PopItem() for handles this by keeping a persistent
// free list. Dont mix Push/PushItem. Note also nodes will be freed at the end, 
// and are expected to have been allocated with operator new.
//-----------------------------------------------------------------------------

// Poison link written into nodes that have been popped, so a stale read is recognisable.
template <typename N> inline N *const TSQUEUE_BAD_NODE_LINK = static_cast<N *>( INT_TO_POINTER( 0xdeadbeef ) );

template <typename T, bool bTestOptimizer = false>
class alignas(TSLIST_NODE_ALIGNMENT) CTSQueue
{
public:

	// override new/delete so we can guarantee 8-byte aligned allocs
	static void * operator new(std::size_t size)
	{
		CTSQueue *pNode = (CTSQueue *)MemAlloc_AllocAlignedFileLine( size, TSLIST_HEAD_ALIGNMENT, __FILE__, __LINE__ );
		return pNode;
	}

	// override new/delete so we can guarantee 8-byte aligned allocs
	static void * operator new(std::size_t size, int nBlockUse, const char *pFileName, int nLine)
	{
		CTSQueue *pNode = (CTSQueue *)MemAlloc_AllocAlignedFileLine( size, TSLIST_HEAD_ALIGNMENT, pFileName, nLine );
		return pNode;
	}

	static void operator delete(void *p)
	{
		MemAlloc_FreeAligned( p );
	}

	static void operator delete(void *p, int nBlockUse, const char *pFileName, int nLine)
	{
		MemAlloc_FreeAligned( p );
	}

private:
	// These ain't gonna work
	static void * operator new[]( std::size_t size ) throw()
	{
		return nullptr;
	}

	static void operator delete []( void *p )
	{
	}

public:

	struct alignas(TSLIST_NODE_ALIGNMENT) Node_t
	{
		// override new/delete so we can guarantee 8-byte aligned allocs
		static void * operator new(std::size_t size)
		{
			Node_t *pNode = (Node_t *)MemAlloc_AllocAlignedFileLine( size, TSLIST_HEAD_ALIGNMENT, __FILE__, __LINE__ );
			return pNode;
		}

		static void * operator new(std::size_t size, int nBlockUse, const char *pFileName, int nLine)
		{
			Node_t *pNode = (Node_t *)MemAlloc_AllocAlignedFileLine( size, TSLIST_HEAD_ALIGNMENT, pFileName, nLine );
			return pNode;
		}

		static void operator delete(void *p)
		{
			MemAlloc_FreeAligned( p );
		}

		static void operator delete(void *p, int nBlockUse, const char *pFileName, int nLine)
		{
			MemAlloc_FreeAligned( p );
		}

		Node_t() {}
		Node_t( const T &init ) : elem( init ) {}

		Node_t *pNext;
		T elem;
	};

	union alignas(TSLIST_HEAD_ALIGNMENT) NodeLink_t
	{
		// override new/delete so we can guarantee 8-byte aligned allocs
		static void * operator new(std::size_t size)
		{
			NodeLink_t *pNode = (NodeLink_t *)MemAlloc_AllocAlignedFileLine( size, TSLIST_HEAD_ALIGNMENT, __FILE__, __LINE__ );
			return pNode;
		}

		static void operator delete(void *p)
		{
			MemAlloc_FreeAligned( p );
		}

		struct Value_t
		{
			Node_t *pNode;
			intp	sequence;
		} value;

		int128 value64x128;
	};

	CTSQueue()
	{
		static_assert( sizeof(Node_t) >= sizeof(TSLNodeBase_t) );
		if ( ((std::size_t)&m_Head) % TSLIST_HEAD_ALIGNMENT != 0 )
		{
			Error( "CTSQueue: Misaligned queue\n" );
			DebuggerBreak();
		}
		if ( ((std::size_t)&m_Tail) % TSLIST_HEAD_ALIGNMENT != 0 )
		{
			Error( "CTSQueue: Misaligned queue\n" );
			DebuggerBreak();
		}
		m_Count = 0;
		m_Head.value.sequence = m_Tail.value.sequence = 0;
		m_Head.value.pNode = m_Tail.value.pNode = new Node_t; // list always contains a dummy node
		m_Head.value.pNode->pNext = End();
	}

	~CTSQueue()
	{
		Purge();
		contract_assert( m_Count == 0 );
		contract_assert( m_Head.value.pNode == m_Tail.value.pNode );
		contract_assert( m_Head.value.pNode->pNext == End() );
		delete m_Head.value.pNode;
	}

	// Note: Purge, RemoveAll, and Validate are *not* threadsafe
	void Purge()
	{
		if ( IsDebug() )
		{
			ValidateQueue();
		}

		Node_t *pNode;
		while ( (pNode = Pop()) != nullptr )
		{
			delete pNode;
		}

		while ( (pNode = (Node_t *)m_FreeNodes.Pop()) != nullptr )
		{
			delete pNode;
		}

		contract_assert( m_Count == 0 );
		contract_assert( m_Head.value.pNode == m_Tail.value.pNode );
		contract_assert( m_Head.value.pNode->pNext == End() );

		m_Head.value.sequence = m_Tail.value.sequence = 0;
	}

	void RemoveAll()
	{
		if ( IsDebug() )
		{
			ValidateQueue();
		}

		Node_t *pNode;
		while ( (pNode = Pop()) != nullptr )
		{
			m_FreeNodes.Push( (TSLNodeBase_t *)pNode );
		}
	}

	bool ValidateQueue()
	{
		if ( IsDebug() )
		{
			bool bResult = true;
			int nNodes = 0;
			if ( m_Tail.value.pNode->pNext != End() )
			{
				DebuggerBreakIfDebugging();
				bResult = false;
			}

			if ( m_Count == 0 )
			{
				if ( m_Head.value.pNode != m_Tail.value.pNode )
				{
					DebuggerBreakIfDebugging();
					bResult = false;
				}
			}

			Node_t *pNode = m_Head.value.pNode;
			while ( pNode != End() )
			{
				nNodes++;
				pNode = pNode->pNext;
			}

			nNodes--;// skip dummy node

			if ( nNodes != m_Count )
			{
				DebuggerBreakIfDebugging();
				bResult = false;
			}

			if ( !bResult )
			{
				Msg( "Corrupt CTSQueueDetected" );
			}

			return bResult;
		}
		else
		{
			return true;
		}
	}

	void FinishPush( Node_t *pNode, const NodeLink_t &oldTail )
	{
		NodeLink_t newTail;

		newTail.value.pNode = pNode;
		newTail.value.sequence = oldTail.value.sequence + 1;

		ThreadMemoryBarrier();

		InterlockedCompareExchangeNodeLink( &m_Tail, newTail, oldTail );
	}

	Node_t *Push( Node_t *pNode )
	{

		NodeLink_t oldTail;

		pNode->pNext = End();

		for ( ;; )
		{
			oldTail.value.sequence = __atomic_load_n( &m_Tail.value.sequence, __ATOMIC_SEQ_CST );
			oldTail.value.pNode = __atomic_load_n( &m_Tail.value.pNode, __ATOMIC_SEQ_CST );
			if ( InterlockedCompareExchangeNode( &(oldTail.value.pNode->pNext), pNode, End() ) == End() )
			{
				break;
			}
			else
			{
				// Another thread is trying to push, help it along
				FinishPush( oldTail.value.pNode->pNext, oldTail );
			}
		}

		FinishPush( pNode, oldTail ); // This can fail if another thread pushed between the sequence and node grabs above. Later pushes or pops corrects

		m_Count++;

		return oldTail.value.pNode;
	}

	Node_t *Pop()
	{
		NodeLink_t * volatile		pHead = &m_Head;
		NodeLink_t * volatile		pTail = &m_Tail;
		Node_t * volatile *			pHeadNode = &m_Head.value.pNode;
		volatile intp * volatile	pHeadSequence = &m_Head.value.sequence;
		Node_t * volatile * 		pTailNode = &pTail->value.pNode;

		NodeLink_t head;
		NodeLink_t newHead;
		Node_t *pNext;
		intp tailSequence;
		T elem;

		for ( ;; )
		{
			head.value.sequence = __atomic_load_n( pHeadSequence, __ATOMIC_SEQ_CST ); // must grab sequence first, which allows condition below to ensure pNext is valid
			ThreadMemoryBarrier(); // need a barrier to prevent reordering of these assignments
			head.value.pNode = __atomic_load_n( pHeadNode, __ATOMIC_SEQ_CST );
			tailSequence = __atomic_load_n( &pTail->value.sequence, __ATOMIC_SEQ_CST );
			pNext = __atomic_load_n( &head.value.pNode->pNext, __ATOMIC_SEQ_CST );

			// Checking pNext only to force optimizer to not reorder the assignment
			// to pNext and the compare of the sequence
			if ( !pNext || head.value.sequence != __atomic_load_n( pHeadSequence, __ATOMIC_SEQ_CST ) )
				continue;

			if ( bTestOptimizer )
			{
				if ( pNext == TSQUEUE_BAD_NODE_LINK<Node_t> )
				{
					Msg( "Bad node link detected\n" );
					continue;
				}
			}

			if ( head.value.pNode == __atomic_load_n( pTailNode, __ATOMIC_SEQ_CST ) )
			{
				if ( pNext == End() )
					return nullptr;

				// Another thread is trying to push, help it along
				NodeLink_t &oldTail = head; // just reuse local memory for head to build old tail
				oldTail.value.sequence = tailSequence; // reuse head pNode
				FinishPush( pNext, oldTail );
				continue;
			}

			if ( pNext != End() )
			{
				elem = pNext->elem; // NOTE: next could be a freed node here, by design
				newHead.value.pNode = pNext;
				newHead.value.sequence = head.value.sequence + 1;
				if ( InterlockedCompareExchangeNodeLink( pHead, newHead, head ) )
				{
					ThreadMemoryBarrier();
					if ( bTestOptimizer )
					{
						head.value.pNode->pNext = TSQUEUE_BAD_NODE_LINK<Node_t>;
					}
					break;
				}
			}
		}

		m_Count--;
		head.value.pNode->elem = elem;
		return head.value.pNode;
	}

	void FreeNode( Node_t *pNode )
	{
		m_FreeNodes.Push( (TSLNodeBase_t *)pNode );
	}

	void PushItem( const T &init )
	{
		Node_t *pNode = (Node_t *)m_FreeNodes.Pop();
		if ( pNode )
		{
			pNode->elem = init;
		}
		else
		{
			pNode = new Node_t( init );
		}
		Push( pNode );
	}

	bool PopItem( T *pResult )
	{
		Node_t *pNode = Pop();
		if ( !pNode )
			return false;

		*pResult = pNode->elem;
		m_FreeNodes.Push( (TSLNodeBase_t *)pNode );
		return true;
	}

	int Count() const
	{
		return m_Count;
	}

private:
	Node_t *End() const { return (Node_t *)this; } // just need a unique signifier

	Node_t *InterlockedCompareExchangeNode( Node_t * volatile *ppNode, Node_t *value, Node_t *comperand )
	{
		return (Node_t *)::ThreadInterlockedCompareExchangePointer( (void **)ppNode, value, comperand );
	}

	bool InterlockedCompareExchangeNodeLink( NodeLink_t volatile *pLink, const NodeLink_t &value, const NodeLink_t &comperand )
	{
		return ThreadInterlockedAssignIf64x128( &pLink->value64x128, value.value64x128, comperand.value64x128 );
	}

	NodeLink_t m_Head;
	NodeLink_t m_Tail;

	std::atomic<int> m_Count;

	CTSListBase m_FreeNodes;
};

}
