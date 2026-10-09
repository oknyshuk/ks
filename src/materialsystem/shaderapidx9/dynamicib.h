#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "locald3dtypes.h"
#include "recording.h"
#include "shaderapidx8_global.h"
#include "shaderapidx8.h"
#include "shaderapi/ishaderutil.h"
#include "materialsystem/ivballoctracker.h"

/////////////////////////////
// D. Sim Dietrich Jr.
// sim.dietrich@nvidia.com
/////////////////////////////

#include "locald3dtypes.h"
import std;
#include "materialsystem/imesh.h"

static constexpr auto kTextureGroupStaticIndexBufferFrame = ks::Concat( "TexGroup_frame_", TEXTURE_GROUP_STATIC_INDEX_BUFFER );
static constexpr auto kTextureGroupStaticIndexBufferGlobal = ks::Concat( "TexGroup_global_", TEXTURE_GROUP_STATIC_INDEX_BUFFER );
static constexpr auto kTextureGroupDynamicIndexBufferD3D = ks::Concat( "D3D: ", TEXTURE_GROUP_DYNAMIC_INDEX_BUFFER );
static constexpr auto kTextureGroupStaticIndexBufferD3D = ks::Concat( "D3D: ", TEXTURE_GROUP_STATIC_INDEX_BUFFER );

// Helper function to unbind an index buffer
void Unbind( IDirect3DIndexBuffer9 *pIndexBuffer );

inline constexpr auto X360_INDEX_BUFFER_SIZE_MULTIPLIER = 2.0; //minimum of 1, only affects dynamic buffers
//#define X360_BLOCK_ON_IB_FLUSH //uncomment to block until all data is consumed when a flush is requested. Otherwise we only block when absolutely necessary

#define SPEW_INDEX_BUFFER_STALLS //uncomment to allow buffer stall spewing.


class CIndexBuffer
{
public:
	CIndexBuffer( D3DDeviceWrapper *pD3D, int count, bool bSoftwareVertexProcessing, bool dynamic = false, MeshBuffersAllocationSettings_t *pSettings = 0 );


	~CIndexBuffer();
	
	LPDIRECT3DINDEXBUFFER GetInterface() const 
	{ 
		// If this buffer still exists, then Late Creation didn't happen. Best case: we'll render the wrong image. Worst case: Crash.
		contract_assert( !m_pSysmemBuffer );
		return m_pIB; 
	}
	
	// Use at beginning of frame to force a flush of VB contents on first draw
	void FlushAtFrameStart() { m_bFlush = true; }
	
	// lock, unlock
	unsigned short *Lock( bool bReadOnly, int numIndices, int &startIndex, int startPosition = -1 );	
	void Unlock( int numIndices );
	void HandleLateCreation( );

	// Index position
	int IndexPosition() const { return m_Position; }
	
	// Index size
	int IndexSize() const { return sizeof(unsigned short); }

	// Index count
	int IndexCount() const { return m_IndexCount; }

	// Do we have enough room without discarding?
	bool HasEnoughRoom( int numIndices ) const;
	
	int GetIndicesLeft() const;

	bool IsDynamic() const { return m_bDynamic; }
	bool IsExternal() const { return m_bExternalMemory; }

	// Block until there's a free portion of the buffer of this size, m_Position will be updated to point at where this section starts
	void BlockUntilUnused( int nAllocationSize );

#ifdef CHECK_INDICES
	void UpdateShadowIndices( unsigned short *pData )
	{
		contract_assert( m_LockedStartIndex + m_LockedNumIndices <= m_NumIndices );
		memcpy( m_pShadowIndices + m_LockedStartIndex, pData, m_LockedNumIndices * IndexSize() );
	}

	unsigned short GetShadowIndex( int i )
	{
		contract_assert( i >= 0 && i < (int)m_NumIndices );
		return m_pShadowIndices[i];
	}
#endif

	// UID
	unsigned int UID() const 
	{ 
		return 0;
	}

	void HandlePerFrameTextureStats( int frame )
	{
		if ( m_Frame != frame && !m_bDynamic )
		{
			m_Frame = frame;
			VProfIncrementCounter<kTextureGroupStaticIndexBufferFrame, COUNTER_GROUP_TEXTURE_PER_FRAME>( IndexCount() * IndexSize() );
		}
	}
	
	static int BufferCount()
	{
#ifdef _DEBUG
		return s_BufferCount;
#else
		return 0;
#endif
	}

	inline int AllocationSize() const;

	inline int AllocationCount() const;

	// Marks a fence indicating when this buffer was used
	void MarkUsedInRendering()
	{
	}

private :
	void Create( D3DDeviceWrapper *pD3D );
	inline void ReallyUnlock( int unlockBytes )
	{
			unlockBytes; // Unused here
			Dx9Device()->Unlock( m_pIB );
	}

	enum LOCK_FLAGS
	{
		LOCKFLAGS_FLUSH  = D3DLOCK_NOSYSLOCK | D3DLOCK_DISCARD,
		LOCKFLAGS_APPEND = D3DLOCK_NOSYSLOCK | D3DLOCK_NOOVERWRITE
	};

	LPDIRECT3DINDEXBUFFER m_pIB;

	int			m_IndexCount;
	int			m_Position;
	byte		*m_pSysmemBuffer;
	int			m_nSysmemBufferStartBytes;
	unsigned char	m_bLocked : 1;
	unsigned char	m_bFlush : 1;
	unsigned char	m_bDynamic : 1;
	unsigned char	m_bExternalMemory : 1;
	unsigned char	m_bSoftwareVertexProcessing : 1;
	unsigned char	m_bLateCreateShouldDiscard : 1;

	int				m_Frame;

#ifdef _DEBUG
	static int		s_BufferCount;
#endif

	LockedBufferContext m_LockData;

protected:
#ifdef CHECK_INDICES
	unsigned short *m_pShadowIndices;
	unsigned int m_NumIndices;
#endif
	
	unsigned int m_LockedStartIndex;
	unsigned int m_LockedNumIndices;
};

 

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------

inline CIndexBuffer::CIndexBuffer( D3DDeviceWrapper *pD3D, int count, 
	bool bSoftwareVertexProcessing, bool dynamic, MeshBuffersAllocationSettings_t *pSettings ) :
		m_pIB(0), 
		m_Position(0), 
		m_bFlush(true), 
		m_bLocked(false),
		m_bExternalMemory(false),
		m_bDynamic(dynamic),
		m_bSoftwareVertexProcessing( bSoftwareVertexProcessing ),
		m_bLateCreateShouldDiscard( false )
		,m_Frame( -1 )
{
	// For write-combining, ensure we always have locked memory aligned to 4-byte boundaries
	count = ALIGN_VALUE( count, 2 );
	m_IndexCount = count; 


#ifdef CHECK_INDICES
	m_pShadowIndices = nullptr;
#endif

#ifdef _DEBUG
	++s_BufferCount;
#endif

#ifdef CHECK_INDICES
	m_pShadowIndices = new unsigned short[ m_IndexCount ];
	m_NumIndices = m_IndexCount;
#endif


    if ( !g_pShaderUtil->IsRenderThreadSafe() )
	{
		m_pSysmemBuffer = ( byte * )MemAlloc_Alloc( count * IndexSize() );
		m_nSysmemBufferStartBytes = 0;
	}
	else
	{
		m_pSysmemBuffer = nullptr;
		Create( pD3D );
	}



	if ( !m_bDynamic )
	{
		VProfIncrementCounter<kTextureGroupStaticIndexBufferGlobal, COUNTER_GROUP_TEXTURE_GLOBAL>( IndexCount() * IndexSize() );
	}
}


void CIndexBuffer::Create( D3DDeviceWrapper *pD3D )
{
	D3DINDEXBUFFER_DESC desc;
	std::memset( &desc, 0x00, sizeof( desc ) );
	desc.Format = D3DFMT_INDEX16;
	desc.Size = sizeof(unsigned short) * m_IndexCount;
	desc.Type = D3DRTYPE_INDEXBUFFER;
	desc.Pool = D3DPOOL_DEFAULT;
	desc.Usage = D3DUSAGE_WRITEONLY;
	if ( m_bDynamic )
	{
		desc.Usage |= D3DUSAGE_DYNAMIC;
	}
	if ( m_bSoftwareVertexProcessing )
	{
		desc.Usage |= D3DUSAGE_SOFTWAREPROCESSING;
	}


	HRESULT hr = pD3D->CreateIndexBuffer( 
		m_IndexCount * IndexSize(),
		desc.Usage,
		desc.Format,
		desc.Pool, 
		&m_pIB, 
		nullptr );
	if ( hr != D3D_OK )
	{
		Warning( "CreateIndexBuffer failed!\n" );
	}

	if ( ( hr == D3DERR_OUTOFVIDEOMEMORY ) || ( hr == E_OUTOFMEMORY ) )
	{
		// Don't have the memory for this.  Try flushing all managed resources
		// out of vid mem and try again.
		// FIXME: need to record this
		pD3D->EvictManagedResources();
		hr = pD3D->CreateIndexBuffer( m_IndexCount * IndexSize(),
			desc.Usage, desc.Format, desc.Pool, &m_pIB, nullptr );
	}

	contract_assert( m_pIB );
	contract_assert( hr == D3D_OK );

#if defined( _DEBUG )
	if ( m_pIB && !m_pSysmemBuffer )
	{
		D3DINDEXBUFFER_DESC aDesc;
		m_pIB->GetDesc( &aDesc );
		contract_assert( memcmp( &aDesc, &desc, sizeof( desc ) ) == 0 );
	}
#endif
}




inline CIndexBuffer::~CIndexBuffer()
{
#ifdef _DEBUG
	if ( !m_bExternalMemory )
	{
		--s_BufferCount;
	}
#endif

	Unlock(0);

#ifdef CHECK_INDICES
	if ( m_pShadowIndices )
	{
		delete [] m_pShadowIndices;
		m_pShadowIndices = nullptr;
	}
#endif

	if ( m_pSysmemBuffer )
	{
		g_pMemAlloc->Free( m_pSysmemBuffer );
		m_pSysmemBuffer = nullptr;
	}

	if ( m_pIB && !m_bExternalMemory )
	{

		Dx9Device()->Release( m_pIB );
	}

	if ( !m_bExternalMemory )
	{
		if ( !m_bDynamic )
		{
			VProfIncrementCounter<kTextureGroupStaticIndexBufferGlobal, COUNTER_GROUP_TEXTURE_GLOBAL>( - IndexCount() * IndexSize() );
		}
	}
}
	


//-----------------------------------------------------------------------------
// Do we have enough room without discarding?
//-----------------------------------------------------------------------------
inline bool CIndexBuffer::HasEnoughRoom( int numIndices ) const
{
	return ( numIndices + m_Position ) <= m_IndexCount;
}

inline int CIndexBuffer::GetIndicesLeft() const
{
	return m_IndexCount - m_Position ;

}

//-----------------------------------------------------------------------------
// Block until this part of the index buffer is free
//-----------------------------------------------------------------------------
inline void CIndexBuffer::BlockUntilUnused( int nAllocationSize )
{
	contract_assert( nAllocationSize <= m_IndexCount );

}


//-----------------------------------------------------------------------------
// lock, unlock
//-----------------------------------------------------------------------------
inline unsigned short* CIndexBuffer::Lock( bool bReadOnly, int numIndices, int& startIndex, int startPosition )
{
	contract_assert( !m_bLocked );


	unsigned short* pLockedData = nullptr;

	// For write-combining, ensure we always have locked memory aligned to 4-byte boundaries
	if( m_bDynamic )
		numIndices = ALIGN_VALUE( numIndices, 2 );

	// Ensure there is enough space in the IB for this data
	if ( numIndices > m_IndexCount ) 
	{ 
		Error( "too many indices for index buffer. . tell a programmer (%d>%d)\n", ( int )numIndices, ( int )m_IndexCount );
		contract_assert( false ); 
		return 0; 
	}
	
	if ( !m_pIB && !m_pSysmemBuffer )
		return 0;

	DWORD dwFlags;
	
	if ( m_bDynamic )
	{
		// startPosition now can be != -1, when calling in here with a static (staging) buffer.
		dwFlags = LOCKFLAGS_APPEND;
	
		// If either user forced us to flush,
		// or there is not enough space for the vertex data,
		// then flush the buffer contents
		// xbox must not append at position 0 because nooverwrite cannot be guaranteed
		
		if ( !m_Position || m_bFlush || !HasEnoughRoom(numIndices) )
		{
			if ( m_pSysmemBuffer || !g_pShaderUtil->IsRenderThreadSafe() ) 
				m_bLateCreateShouldDiscard = true;

			m_bFlush = false;
			m_Position = 0;

			dwFlags = LOCKFLAGS_FLUSH;
		}
	}
	else
	{
		dwFlags = D3DLOCK_NOSYSLOCK;
	}
	
	if ( bReadOnly )
	{
		dwFlags |= D3DLOCK_READONLY;
	}

	int position = m_Position;
	if( startPosition >= 0 )
	{
		position = startPosition;
	}


	m_LockedStartIndex = position;
	m_LockedNumIndices = numIndices;

	HRESULT hr = D3D_OK;

	// If the caller isn't in the thread that owns the render lock, need to return a system memory pointer--cannot talk to GL from 
	// the non-current thread. 
	if ( !m_pSysmemBuffer && !g_pShaderUtil->IsRenderThreadSafe() )
	{
		m_pSysmemBuffer = ( byte * )MemAlloc_Alloc( m_IndexCount * IndexSize() );
		m_nSysmemBufferStartBytes = position * IndexSize();
	}

	if ( m_pSysmemBuffer != nullptr )
	{
		// Ensure that we're never moving backwards in a buffer--this code would need to be rewritten if so. 
		// We theorize this can happen if you hit the end of a buffer and then wrap before drawing--but
		// this would probably break in other places as well.
// 		if ( position * IndexSize() < m_nSysmemBufferStartBytes ) 
// 		{
// 			while(1) printf("wrapped\n");
// 		}

		pLockedData = ( unsigned short * )( m_pSysmemBuffer + ( position * IndexSize() ) );
	}
	else 
	{
		hr = Dx9Device()->Lock( m_pIB,
			position * IndexSize(),  numIndices * IndexSize(), 
						   reinterpret_cast< void** >( &pLockedData ), dwFlags );
	}

	switch ( hr )
	{
		case D3DERR_INVALIDCALL:
			Msg( "D3DERR_INVALIDCALL - Index Buffer Lock Failed in %s on line %d(offset %d, size %d, flags 0x%x)\n", V_UnqualifiedFileName(__FILE__), __LINE__, position * IndexSize(), numIndices * IndexSize(), dwFlags );
			break;
		case D3DERR_DRIVERINTERNALERROR:
			Msg( "D3DERR_DRIVERINTERNALERROR - Index Buffer Lock Failed in %s on line %d (offset %d, size %d, flags 0x%x)\n", V_UnqualifiedFileName(__FILE__), __LINE__, position * IndexSize(), numIndices * IndexSize(), dwFlags );
			break;
		case D3DERR_OUTOFVIDEOMEMORY:
			Msg( "D3DERR_OUTOFVIDEOMEMORY - Index Buffer Lock Failed in %s on line %d (offset %d, size %d, flags 0x%x)\n", V_UnqualifiedFileName(__FILE__), __LINE__, position * IndexSize(), numIndices * IndexSize(), dwFlags );
			break;
	}

	contract_assert( pLockedData != nullptr );
	   
	startIndex = position;

	contract_assert( m_bLocked == false );
	m_bLocked = true;
	return pLockedData;
}

inline void CIndexBuffer::Unlock( int numIndices )
{
	contract_assert( (m_Position + numIndices) <= m_IndexCount );

	if ( !m_bLocked )
		return;

	// For write-combining, ensure we always have locked memory aligned to 4-byte boundaries
	if( m_bDynamic )
		numIndices = ALIGN_VALUE( numIndices, 2 );

	if ( !m_pIB && !m_pSysmemBuffer )
		return;


	if ( m_pSysmemBuffer )
	{
	}
	else 
	{
		int unlockBytes = 0; 
		ReallyUnlock( unlockBytes );
	}

	m_Position += numIndices;
	m_bLocked = false;

#ifdef CHECK_INDICES
	m_LockedStartIndex = 0;
	m_LockedNumIndices = 0;
#endif
}


inline void CIndexBuffer::HandleLateCreation( )
{
	if ( !m_pSysmemBuffer )
	{
		return;
	}

	if( !m_pIB )
	{
		bool bPrior = g_VBAllocTracker->TrackMeshAllocations( "HandleLateCreation" );
		Create( Dx9Device() );
		if ( !bPrior )
		{
			g_VBAllocTracker->TrackMeshAllocations( nullptr );
		}
	}
	
	void* pWritePtr = nullptr;
	int dataToWriteBytes = ( m_Position * IndexSize() ) - m_nSysmemBufferStartBytes;
	DWORD dwFlags = D3DLOCK_NOSYSLOCK;
	if ( m_bDynamic )
		dwFlags |= ( m_bLateCreateShouldDiscard ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE );
	
	// Always clear this.
	m_bLateCreateShouldDiscard = false;

	// Don't use the Lock function, it does a bunch of stuff we don't want.

	HRESULT hr = 0;

	// If we've wrapped might as well transfer the whole IB

	if (dataToWriteBytes < 1)
	{
		dataToWriteBytes = m_IndexCount * IndexSize();
		m_nSysmemBufferStartBytes = 0;
	}


	hr = Dx9Device()->Lock( m_pIB,
			m_nSysmemBufferStartBytes, 
			dataToWriteBytes,
			&pWritePtr,
			dwFlags );

	// If this fails we're about to crash. Consider skipping the update and leaving 
	// m_pSysmemBuffer around to try again later. (For example in case of device loss)
	contract_assert( SUCCEEDED( hr ) ); hr;

	std::memcpy( pWritePtr, m_pSysmemBuffer + m_nSysmemBufferStartBytes, dataToWriteBytes );
	ReallyUnlock( dataToWriteBytes );

	g_pMemAlloc->Free( m_pSysmemBuffer );
	m_pSysmemBuffer = nullptr;
}


// Returns the allocated size
inline int CIndexBuffer::AllocationSize() const
{
	return m_IndexCount * IndexSize();
}

inline int CIndexBuffer::AllocationCount() const
{
	return m_IndexCount;
}
