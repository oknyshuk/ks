// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1:buffer_strider;

import std;
import ks.math;
export import :strtools;

export extern "C++" {

class CBufferStrider
{
public:
	CBufferStrider( void *pBuffer );

	template <typename T> T *Stride( int nCount = 1 );
	template <typename T> T StrideUnaligned( );
	template <typename T> void StrideUnaligned( T* pOut, int nCount = 1 );
	char* StrideString( );
	char* StrideString( const void *pEnd );
	template <typename T> T Peek( ){ return *(T*)Get(); }
	uint8 *Get(){ return m_pBuffer; }
	const uint8 *Get() const { return m_pBuffer; }
	void Set( void *p ){ m_pBuffer = ( uint8 *)p; }
	void Align( uint nAlignment ){ m_pBuffer = ( uint8* )( ( uintp( m_pBuffer ) + nAlignment - 1 ) & ~uintp( nAlignment - 1 ) ); }
protected:
	uint8 *m_pBuffer;
};

template <typename T>
inline T *CBufferStrider::Stride( int nCount )
{
	uint nAlignMask = __alignof( T ) - 1; (void)nAlignMask;
	contract_assert( !( uintp( m_pBuffer ) & nAlignMask ) );
	T* p = ( T* ) m_pBuffer;
	m_pBuffer += sizeof( T ) * nCount;
	return p;
}

template <typename T>
inline T CBufferStrider::StrideUnaligned( )
{
	T out;
	memcpy( &out, m_pBuffer, sizeof( T ) );
	m_pBuffer += sizeof( T );
	return out;
}

template <typename T>
inline void CBufferStrider::StrideUnaligned( T* pOut, int nCount )
{
	memcpy( pOut, m_pBuffer, sizeof( T ) * nCount );
	m_pBuffer += sizeof( T ) * nCount;
}

inline char* CBufferStrider::StrideString( const void *pEnd )
{
	uint8 *pNextItem = m_pBuffer;
	do
	{
		if ( pNextItem >= ( uint8* ) pEnd )
		{
			return nullptr; // we steppend past end of buffer
		}
	}
	while ( *( pNextItem++ ) );
	// found end of string
	char *pString = ( char* ) m_pBuffer;
	m_pBuffer = pNextItem;
	return pString;
}

inline char* CBufferStrider::StrideString( )
{
	uint8 *pNextItem = m_pBuffer;
	while ( *( pNextItem++ ) ) // this will crash in case of malformed (no terminating NULL) buffer
	{
		continue;
	}
	// found end of string
	char *pString = ( char* ) m_pBuffer;
	m_pBuffer = pNextItem;
	return pString;
}

inline
CBufferStrider::CBufferStrider( void *pBuffer )
: m_pBuffer( ( uint8* ) pBuffer )
{
}

class CMultiBufferCounter
{
public:
	CMultiBufferCounter( )
		:m_nByteCount( 0 )
		, m_nMaxAlignment( 0 )
		, m_nRunAlignment( 0xFFFFFFFF )
	{}
	template <typename T> 
	T* operator()( T *& pDummy, uint nCount )
	{
		const uint nAlignMask = __alignof( T ) - 1;
		contract_assert( m_nRunAlignment >= nAlignMask );
		m_nRunAlignment &= nAlignMask;
		m_nMaxAlignment |= nAlignMask;
		m_nByteCount += sizeof( T ) * nCount;
		return nullptr; // we didn't allocate anything yet!
	}
	template< typename T, typename V > requires requires ( const V &v ) { std::data( v ); std::size( v ); }
	T* operator()( T*& pDummy, const V &arr )
	{
		( *this )( pDummy, std::ssize( arr ) );
		return nullptr; // we didn't allocate anything yet
	}
	char *String( const char *& pDummy, const char *pSourceString )
	{
		if ( pSourceString )
		{
			( *this )( pDummy, uint( V_strlen( pSourceString ) + 1 ) );
		}
		return nullptr; // we didn't allocate anything yet
	}
	uint GetByteCount( )const { return m_nByteCount; }
protected:
	uint m_nByteCount;
	uint m_nMaxAlignment;
	uint m_nRunAlignment;
};

class CMultiBufferStrider: public CBufferStrider
{
public:
	CMultiBufferStrider( void *pBuffer ): CBufferStrider( pBuffer ){}
	template <typename T>
	T* operator() ( T*& refPtr, uint nCount )
	{
		refPtr = nCount ? Stride< T >( nCount ) : nullptr;
		return refPtr;
	}
	template < typename T, typename V > requires requires ( const V &v ) { std::data( v ); std::size( v ); }
	T* operator()( T *&refPtr, const V &arr )
	{
		if ( arr.empty() )
		{
			refPtr = nullptr;
		}
		else
		{
			refPtr = Stride< T >( std::ssize( arr ) );
			V_memcpy( refPtr, arr.data(), sizeof( T ) * std::ssize( arr ) );
		}
		return refPtr;
	}

	char *String( const char *& refPtr, const char *pSourceString )
	{
		if ( pSourceString )
		{
			uint nBufLen = V_strlen( pSourceString ) + 1;
			char *pOut = const_cast< char* >( ( *this )( refPtr, nBufLen ) );
			V_memcpy( pOut, pSourceString, nBufLen );
			contract_assert( refPtr == pOut );
			return pOut;
		}
		else
		{
			return nullptr; // we didn't allocate anything
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// Derive from this buffer and override function Enum( T& ) that should
// enumerate all the pointers and sizes of buffers to be allocated
//
template <typename Derived>
class CMultiBufferHelper
{
public:
	CMultiBufferHelper( )
	{
		m_pMultiBuffer = nullptr;
		m_nMultiBufferSize = 0;
	}
	~CMultiBufferHelper( )
	{
		if ( m_pMultiBuffer )
		{
			MemAlloc_FreeAligned( m_pMultiBuffer );
			m_pMultiBuffer = nullptr;
			m_nMultiBufferSize = 0;
		}
	}
	uint8 *GetBuffer( ) { return m_pMultiBuffer; }
	uint GetBufferSize( ) { return m_nMultiBufferSize; }
	uint8 *TakeBuffer( )
	{
		// forget the buffer
		uint8 *pBuffer = m_pMultiBuffer;
		m_pMultiBuffer = nullptr;
		m_nMultiBufferSize = 0;
		return pBuffer;
	}

protected:

	template <typename Ctx >
	void ReallocateMultiBuffer( Ctx &context )
	{
		if ( m_pMultiBuffer )
		{
			MemAlloc_FreeAligned( m_pMultiBuffer );
		}

		CMultiBufferCounter byteCounter;
		static_cast< Derived* >( this )->OnAllocateMultiBuffer( byteCounter, context );
		m_nMultiBufferSize = byteCounter.GetByteCount( );
		m_pMultiBuffer = ( uint8* )MemAlloc_AllocAlignedUnattributed( m_nMultiBufferSize, 16 );
		CMultiBufferStrider bufferStrider( m_pMultiBuffer );
		static_cast< Derived* >( this )->OnAllocateMultiBuffer( bufferStrider, context );
		contract_assert( m_pMultiBuffer + m_nMultiBufferSize == bufferStrider.Get( ) );
	}

protected:
	uint8 *m_pMultiBuffer;
	uint m_nMultiBufferSize;
};

}
