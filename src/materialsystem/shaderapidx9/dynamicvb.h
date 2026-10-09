#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core;
#include "shaderapidx8_global.h"
#include "locald3dtypes.h"
import std;
#include "shaderapi/ishaderutil.h"
#include "materialsystem/ivballoctracker.h"
#include "materialsystem/imesh.h"

static constexpr auto kTextureGroupDynamicVertexBufferGlobal = ks::Concat( "TexGroup_global_", TEXTURE_GROUP_DYNAMIC_VERTEX_BUFFER );

/////////////////////////////
// D. Sim Dietrich Jr.
// sim.dietrich@nvidia.com
//////////////////////


// Helper function to unbind an vertex buffer
void Unbind( IDirect3DVertexBuffer9 *pVertexBuffer );

inline constexpr auto X360_VERTEX_BUFFER_SIZE_MULTIPLIER = 2.0; //minimum of 1, only affects dynamic buffers
//#define X360_BLOCK_ON_VB_FLUSH //uncomment to block until all data is consumed when a flush is requested. Otherwise we only block when absolutely necessary

//#define SPEW_VERTEX_BUFFER_STALLS //uncomment to allow buffer stall spewing.

#define MB (1024.0f*1024.0f)

class CVertexBuffer
{
public:
	CVertexBuffer( D3DDeviceWrapper * pD3D, VertexFormat_t fmt, DWORD theFVF, int vertexSize,
					int theVertexCount, const char *pTextureBudgetName, bool bSoftwareVertexProcessing, bool dynamic = false );


	~CVertexBuffer();
	
	LPDIRECT3DVERTEXBUFFER GetInterface() const 
	{ 
		// If this buffer still exists, then Late Creation didn't happen. Best case: we'll render the wrong image. Worst case: Crash.
		contract_assert( !m_pSysmemBuffer );
		return m_pVB; 
	}
	
	// Use at beginning of frame to force a flush of VB contents on first draw
	void FlushAtFrameStart() { m_bFlush = true; }
	
	// lock, unlock
	unsigned char* Lock( int numVerts, int& baseVertexIndex );	
	unsigned char* Modify( bool bReadOnly, int firstVertex, int numVerts );	
	void Unlock( int numVerts );

	void HandleLateCreation( );

	// Vertex size
	int VertexSize() const { return m_VertexSize; }

	// Vertex count
	int VertexCount() const { return m_VertexCount; }

	static int BufferCount()
	{
#ifdef _DEBUG
		return s_BufferCount;
#else
		return 0;
#endif
	}

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
			m_pFrameCounter += m_nBufferSize;
		}
	}
	
	// Do we have enough room without discarding?
	bool HasEnoughRoom( int numVertices ) const;

	// Is this dynamic?
	bool IsDynamic() const { return m_bDynamic; }
	bool IsExternal() const { return m_bExternalMemory; }

	// Block until this part of the vertex buffer is free
	void BlockUntilUnused( int nBufferSize );

	// used to alter the characteristics after creation
	// allows one dynamic vb to be shared for multiple formats
	void ChangeConfiguration( int vertexSize, int totalSize ) 
	{
		contract_assert( m_bDynamic && !m_bLocked && vertexSize );
		m_VertexSize = vertexSize;
		m_VertexCount = m_nBufferSize / vertexSize;
	}

	// Compute the next offset for the next lock
	int NextLockOffset( ) const;

	// Returns the allocated size
	int AllocationSize() const;

	// Returns the number of vertices we have enough room for
	int NumVerticesUntilFlush() const
	{
		return (m_nBufferSize - NextLockOffset()) / m_VertexSize;
	}

	// Marks a fence indicating when this buffer was used
	void MarkUsedInRendering()
	{
	}

private:
	void Create( D3DDeviceWrapper *pD3D );
	inline void ReallyUnlock( int unlockBytes )
	{
			unlockBytes; // Unused here
			Dx9Device()->Unlock( m_pVB );
	}

	enum LOCK_FLAGS
	{
		LOCKFLAGS_FLUSH  = D3DLOCK_NOSYSLOCK | D3DLOCK_DISCARD,
		LOCKFLAGS_APPEND = D3DLOCK_NOSYSLOCK | D3DLOCK_NOOVERWRITE
	};

	LPDIRECT3DVERTEXBUFFER m_pVB;
	

	VertexFormat_t	m_VertexBufferFormat;		// yes, Vertex, only used for allocation tracking
	int				m_nBufferSize;
	int				m_Position;
	int				m_VertexCount;
	int				m_VertexSize;
	DWORD			m_TheFVF;
	byte			*m_pSysmemBuffer;
	int				m_nSysmemBufferStartBytes;

	uint			m_nLockCount;
	unsigned char	m_bDynamic : 1;
	unsigned char	m_bLocked : 1;
	unsigned char	m_bFlush : 1;
	unsigned char	m_bExternalMemory : 1;
	unsigned char	m_bSoftwareVertexProcessing : 1;
	unsigned char	m_bLateCreateShouldDiscard : 1;

	int				m_Frame;
	int				*m_pFrameCounter;
	int				*m_pGlobalCounter;

#ifdef _DEBUG
	static int		s_BufferCount;
#endif

};



//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
inline CVertexBuffer::CVertexBuffer(D3DDeviceWrapper * pD3D, VertexFormat_t fmt, DWORD theFVF, 
	int vertexSize, int vertexCount, const char *pTextureBudgetName,
	bool bSoftwareVertexProcessing, bool dynamic ) :
		m_pVB(0), 
		m_Position(0),
		m_VertexSize(vertexSize), 
		m_VertexCount(vertexCount),
		m_bFlush(true),
		m_bLocked(false), 
		m_bExternalMemory( false ),
		m_nBufferSize(vertexSize * vertexCount), 
		m_TheFVF( theFVF ),
		m_bSoftwareVertexProcessing( bSoftwareVertexProcessing ),
		m_bDynamic(dynamic),
		m_VertexBufferFormat( fmt ),
		m_bLateCreateShouldDiscard( false )
		,m_Frame( -1 )
{

#ifdef _DEBUG
	++s_BufferCount;
#endif

	if ( !m_bDynamic )
	{
		char name[256];
		V_strcpy_safe( name, "TexGroup_global_" );
		V_strcat_safe( name, pTextureBudgetName, sizeof(name) );
		m_pGlobalCounter = g_VProfCurrentProfile.FindOrCreateCounter( name, COUNTER_GROUP_TEXTURE_GLOBAL );

		V_strcpy_safe( name, "TexGroup_frame_" );
		V_strcat_safe( name, pTextureBudgetName, sizeof(name) );
		m_pFrameCounter = g_VProfCurrentProfile.FindOrCreateCounter( name, COUNTER_GROUP_TEXTURE_PER_FRAME );
	}
	else
	{
		m_pGlobalCounter = g_VProfCurrentProfile.FindOrCreateCounter( kTextureGroupDynamicVertexBufferGlobal.c_str(), COUNTER_GROUP_TEXTURE_GLOBAL );
	}

	if ( !g_pShaderUtil->IsRenderThreadSafe() )
	{
		m_pSysmemBuffer = ( byte * )std::malloc( m_nBufferSize );
		m_nSysmemBufferStartBytes = 0;
	}
	else
	{
		m_pSysmemBuffer = nullptr;
		Create( pD3D );
	}

	if ( !m_bDynamic )
	{
		contract_assert( m_pGlobalCounter );
		*m_pGlobalCounter += m_nBufferSize;
	}
}


void CVertexBuffer::Create( D3DDeviceWrapper *pD3D )
{
	D3DVERTEXBUFFER_DESC desc;
	std::memset( &desc, 0x00, sizeof( desc ) );
	desc.Format = D3DFMT_VERTEXDATA;
	desc.Size = m_nBufferSize;
	desc.Type = D3DRTYPE_VERTEXBUFFER;
	desc.Pool = m_bDynamic ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
	desc.FVF = m_TheFVF;



	desc.Usage = D3DUSAGE_WRITEONLY;
	if ( m_bDynamic )
	{
		desc.Usage |= D3DUSAGE_DYNAMIC;
		// Dynamic meshes should never be compressed (slows down writing to them)
		contract_assert( CompressionType( m_TheFVF ) == VERTEX_COMPRESSION_NONE );
	}
	if ( m_bSoftwareVertexProcessing )
	{
		desc.Usage |= D3DUSAGE_SOFTWAREPROCESSING;
	}


	HRESULT hr = pD3D->CreateVertexBuffer( m_nBufferSize, desc.Usage, desc.FVF, desc.Pool, &m_pVB, nullptr );

	if ( hr == D3DERR_OUTOFVIDEOMEMORY || hr == E_OUTOFMEMORY )
	{
		// Don't have the memory for this.  Try flushing all managed resources
		// out of vid mem and try again.
		// FIXME: need to record this
		pD3D->EvictManagedResources();
		pD3D->CreateVertexBuffer( m_nBufferSize, desc.Usage, desc.FVF, desc.Pool, &m_pVB, nullptr );
	}

#ifdef _DEBUG
	if ( hr != D3D_OK )
	{
		switch ( hr )
		{
		case D3DERR_INVALIDCALL:
			contract_assert( !"D3DERR_INVALIDCALL" );
			break;
		case D3DERR_OUTOFVIDEOMEMORY:
			contract_assert( !"D3DERR_OUTOFVIDEOMEMORY" );
			break;
		case E_OUTOFMEMORY:
			contract_assert( !"E_OUTOFMEMORY" );
			break;
		default:
			contract_assert( 0 );
			break;
		}
	}
#endif

	contract_assert( m_pVB );


	// Track VB allocations
	g_VBAllocTracker->CountVB( m_pVB, m_bDynamic, m_nBufferSize, m_VertexSize, m_VertexBufferFormat );
}


inline CVertexBuffer::~CVertexBuffer()
{
	// Track VB allocations
	if ( m_pVB != nullptr )
	{
		g_VBAllocTracker->UnCountVB( m_pVB );
	}

	if ( !m_bExternalMemory )
	{

		if ( !m_bDynamic )
		{
			contract_assert( m_pGlobalCounter );
			*m_pGlobalCounter -= m_nBufferSize;
		}

#ifdef _DEBUG
		--s_BufferCount;
#endif
	}

	Unlock( 0 );

	if ( m_pSysmemBuffer )
	{
		std::free( m_pSysmemBuffer );
		m_pSysmemBuffer = nullptr;
	}

	if ( m_pVB && !m_bExternalMemory )
	{

		Dx9Device()->Release( m_pVB );
	}
}

//-----------------------------------------------------------------------------
// Compute the next offset for the next lock
//-----------------------------------------------------------------------------
inline int CVertexBuffer::NextLockOffset( ) const
{
	int nNextOffset = ( m_Position + m_VertexSize - 1 ) / m_VertexSize;
	nNextOffset *= m_VertexSize;
	return nNextOffset;
}


//-----------------------------------------------------------------------------
// Do we have enough room without discarding?
//-----------------------------------------------------------------------------
inline bool CVertexBuffer::HasEnoughRoom( int numVertices ) const
{
	return (NextLockOffset() + (numVertices * m_VertexSize)) <= m_nBufferSize;
}

//-----------------------------------------------------------------------------
// Block until this part of the index buffer is free
//-----------------------------------------------------------------------------
inline void CVertexBuffer::BlockUntilUnused( int nBufferSize )
{
	contract_assert( nBufferSize <= m_nBufferSize );

}


//-----------------------------------------------------------------------------
// lock, unlock
//-----------------------------------------------------------------------------
inline unsigned char* CVertexBuffer::Lock( int numVerts, int& baseVertexIndex )
{

	m_nLockCount = numVerts;

	unsigned char* pLockedData = 0;
	baseVertexIndex = 0;
	int nBufferSize = numVerts * m_VertexSize;


	// Ensure there is enough space in the VB for this data
	if ( numVerts > m_VertexCount ) 
	{ 
		contract_assert( 0 );
		return 0; 
	}
	
	if ( !m_pVB && !m_pSysmemBuffer )
		return 0;

	DWORD dwFlags;
	if ( m_bDynamic )
	{		
		dwFlags = LOCKFLAGS_APPEND;

		// If either the user forced us to flush,
		// or there is not enough space for the vertex data,
		// then flush the buffer contents
		if ( !m_Position || m_bFlush || !HasEnoughRoom(numVerts) )
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
		// Since we are a static VB, always lock the beginning of the buffer.
		dwFlags = D3DLOCK_NOSYSLOCK;
		m_Position = 0;
	}


	int nLockOffset = NextLockOffset( );

	// If the caller isn't in the thread that owns the render lock, need to return a system memory pointer--cannot talk to GL from 
	// the non-current thread. 
	if ( !m_pSysmemBuffer && !g_pShaderUtil->IsRenderThreadSafe() )
	{
		m_pSysmemBuffer = ( byte * )std::malloc( m_nBufferSize );
		m_nSysmemBufferStartBytes = nLockOffset;
		contract_assert( ( m_nSysmemBufferStartBytes % m_VertexSize ) == 0 );
	}

	if ( m_pSysmemBuffer != nullptr )
	{
		// Ensure that we're never moving backwards in a buffer--this code would need to be rewritten if so. 
		// We theorize this can happen if you hit the end of a buffer and then wrap before drawing--but
		// this would probably break in other places as well.
		contract_assert( nLockOffset >= m_nSysmemBufferStartBytes );
		pLockedData = m_pSysmemBuffer + nLockOffset;
	}
	else 
	{
		Dx9Device()->Lock( m_pVB, nLockOffset, 
						   nBufferSize, 
						   reinterpret_cast< void** >( &pLockedData ), 
						   dwFlags );
	}

	contract_assert( pLockedData != 0 );
	m_bLocked = true;
	baseVertexIndex = nLockOffset / m_VertexSize;
	return pLockedData;
}

inline unsigned char* CVertexBuffer::Modify( bool bReadOnly, int firstVertex, int numVerts )
{
	unsigned char* pLockedData = 0;
		
	// D3D still returns a pointer when you call lock with 0 verts, so just in
	// case it's actually doing something, don't even try to lock the buffer with 0 verts.
	if ( numVerts == 0 )
		return nullptr;

	m_nLockCount = numVerts;

	// If this hits, m_pSysmemBuffer logic needs to be added to this code.
	contract_assert( g_pShaderUtil->IsRenderThreadSafe() );
	contract_assert( !m_pSysmemBuffer );		// if this hits, then we need to add code to handle it

	contract_assert( m_pVB && !m_bDynamic );

	if ( firstVertex + numVerts > m_VertexCount ) 
	{ 
		contract_assert( 0 ); 
		return nullptr; 
	}

	DWORD dwFlags = D3DLOCK_NOSYSLOCK;
	if ( bReadOnly )
	{
		dwFlags |= D3DLOCK_READONLY;
	}


	// mmw: for forcing all dynamic...        LOCKFLAGS_FLUSH );
	Dx9Device()->Lock( 
		m_pVB, 
		firstVertex * m_VertexSize, 
		numVerts * m_VertexSize, 
		reinterpret_cast< void** >( &pLockedData ), 
		dwFlags );
	
	m_Position = firstVertex * m_VertexSize;
	contract_assert( pLockedData != 0 );
	m_bLocked = true;

	return pLockedData;
}

inline void CVertexBuffer::Unlock( int numVerts )
{
	if ( !m_bLocked )
		return;

	if ( !m_pVB && !m_pSysmemBuffer )
		return;

	int nLockOffset = NextLockOffset();
	int nBufferSize = numVerts * m_VertexSize;


	if ( m_pSysmemBuffer != nullptr )
	{
	}
	else
	{
			int unlockBytes = 0;

		ReallyUnlock( unlockBytes );
	}
	m_Position = nLockOffset + nBufferSize;

	m_bLocked = false;
}


inline void CVertexBuffer::HandleLateCreation( )
{
	if ( !m_pSysmemBuffer )
	{
		return;
	}

	if( !m_pVB )
	{
		bool bPrior = g_VBAllocTracker->TrackMeshAllocations( "HandleLateCreation" );
		Create( Dx9Device() );
		if ( !bPrior )
		{
			g_VBAllocTracker->TrackMeshAllocations( nullptr );
		}
	}

	void* pWritePtr = nullptr;
	int dataToWriteBytes = m_bDynamic ? ( m_Position - m_nSysmemBufferStartBytes ) : ( m_nLockCount * m_VertexSize );
	DWORD dwFlags = D3DLOCK_NOSYSLOCK;
	if ( m_bDynamic )
	{
		dwFlags |= ( m_bLateCreateShouldDiscard ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE );
	}
	
	// Always clear this.
	m_bLateCreateShouldDiscard = false;
	
	// If we've wrapped might as well transfer the whole VB
	
	if (dataToWriteBytes < 1)
	{
		dataToWriteBytes = m_VertexCount * VertexSize();
		m_nSysmemBufferStartBytes = 0;
	}

	// Don't use the Lock function, it does a bunch of stuff we don't want.
	HRESULT hr = Dx9Device()->Lock( m_pVB, m_nSysmemBufferStartBytes,
		dataToWriteBytes,
		&pWritePtr,
		dwFlags );

	// If this fails we're about to crash. Consider skipping the update and leaving 
	// m_pSysmemBuffer around to try again later. (For example in case of device loss)
	contract_assert( SUCCEEDED( hr ) ); hr; 
	std::memcpy( pWritePtr, m_pSysmemBuffer + m_nSysmemBufferStartBytes, dataToWriteBytes );
	ReallyUnlock( dataToWriteBytes );

	std::free( m_pSysmemBuffer );
	m_pSysmemBuffer = nullptr;
}


// Returns the allocated size
inline int CVertexBuffer::AllocationSize() const
{
	return m_VertexCount * m_VertexSize;
}
