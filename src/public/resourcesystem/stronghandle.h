#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "tier2/tier2.h"
#include "resourcesystem/iresourcesystem.h"

//-----------------------------------------------------------------------------
// Resource pointer; 
//-----------------------------------------------------------------------------
template< class T >
class CStrongHandle
{
public:
	typedef const ResourceBinding_t< T > *ResourceHandleTyped_t;

	// Constructors
	CStrongHandle();
	CStrongHandle( ResourceHandleTyped_t hResource );
	CStrongHandle( ResourceId_t nResourceId );
	CStrongHandle( const char *pFileName, const char *pSubResourceName );
	CStrongHandle( const CStrongHandle< T > &src );
	~CStrongHandle();

	// Init, shutdown
	void Init( ResourceHandleTyped_t nResourceId );
	void Init( ResourceId_t nResourceId );
	void Init( const char *pFileName, const char *pSubResourceName );
	void Init( const CStrongHandle< T > &src );
	void Shutdown();

	// Assignment
	CStrongHandle< T >& operator=( ResourceHandleTyped_t hResource );

	// Is the resource actually in memory
	bool IsCached() const;

	// Forces the resource to be brought into or out of memory 
	void CacheResource();
	void UncacheResource();

	// Forces a reload of the resource
	void ReloadResource();

	// Cast operators
	operator const T*() const;
	const T* operator->() const;
	operator ResourceHandleTyped_t() const;
	operator ResourceHandle_t() const;

	const ResourceBinding_t< T > *GetHandle() const;

	// Comparison operators
	bool operator==( ResourceHandle_t hResource ) const;
	bool operator==( ResourceHandleTyped_t hResource ) const;
	bool operator==( const CStrongHandle< T > &hResource ) const;

	bool operator!=( ResourceHandle_t hResource ) const;
	bool operator!=( ResourceHandleTyped_t hResource ) const;
	bool operator!=( const CStrongHandle< T > &hResource ) const;

	// Marks the resource as being used this frame
	void MarkUsed();

private:
	const ResourceBinding_t< T > *m_pBinding;
};


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
template< class T >
[[gnu::always_inline]] inline CStrongHandle< T >::CStrongHandle()
{
	m_pBinding = nullptr;
}

template< class T >
[[gnu::always_inline]] inline CStrongHandle< T >::CStrongHandle( const char *pFileName, const char *pSubResourceName )
{
	m_pBinding = nullptr;
	Init( pFileName, pSubResourceName );
}

template< class T >
[[gnu::always_inline]] inline CStrongHandle< T >::CStrongHandle( ResourceId_t nResourceId )
{
	m_pBinding = nullptr;
	Init( nResourceId );
}

// FIXME: Do I want typed resource handles?
template< class T >
[[gnu::always_inline]] inline CStrongHandle< T >::CStrongHandle( ResourceHandleTyped_t hResource )
{
	m_pBinding = nullptr;
	Init( hResource );
}

template< class T >
CStrongHandle< T >::CStrongHandle( const CStrongHandle< T > &src )
{
	m_pBinding = nullptr;
	Init( src );
}

template< class T >
CStrongHandle< T >::~CStrongHandle()
{
	Shutdown();
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
template< class T >
void CStrongHandle< T >::Init( ResourceId_t nResourceId )
{
	if ( m_pBinding )
	{
		Shutdown();
	}

	IResourceTypeManager *pMgr = g_pResourceSystem->GetResourceManagerForType< T >();
	m_pBinding = (ResourceHandleTyped_t)pMgr->FindResource( nResourceId );
	contract_assert( m_pBinding );
	if ( m_pBinding )
	{
		++m_pBinding->m_nRefCount;
	}
}

template< class T >
void CStrongHandle< T >::Init( const char *pFileName, const char *pSubResourceName )
{
	if ( m_pBinding )
	{
		Shutdown();
	}

	IResourceTypeManager *pMgr = g_pResourceSystem->GetResourceManagerForType< T >();
	m_pBinding = (ResourceHandleTyped_t)pMgr->FindOrCreateResource( pFileName, pSubResourceName );
	contract_assert( m_pBinding );
	if ( m_pBinding )
	{
		++m_pBinding->m_nRefCount;
	}
}

template< class T >
void CStrongHandle< T >::Init( ResourceHandleTyped_t hResource )
{
	if ( m_pBinding )
	{
		Shutdown();
	}

	m_pBinding = hResource;
	if ( m_pBinding )
	{
		++m_pBinding->m_nRefCount;
	}
}

template< class T >
void CStrongHandle< T >::Init( const CStrongHandle< T > &src )
{
	if ( m_pBinding )
	{
		Shutdown();
	}

	m_pBinding = src.m_pBinding;
	if ( m_pBinding )
	{
		++m_pBinding->m_nRefCount;
	}
}

template< class T >
void CStrongHandle< T >::Shutdown()
{
	if ( m_pBinding )
	{
		--m_pBinding->m_nRefCount;
		m_pBinding = nullptr;
	}
}


//-----------------------------------------------------------------------------
// Assignment
//-----------------------------------------------------------------------------
template< class T >
[[gnu::always_inline]] inline CStrongHandle< T >& CStrongHandle< T >::operator=( ResourceHandleTyped_t hResource )
{
	Init( hResource );
	return *this;
}


//-----------------------------------------------------------------------------
// Cast operators
//-----------------------------------------------------------------------------
template< class T >
[[gnu::always_inline]] inline const ResourceBinding_t< T > *CStrongHandle< T >::GetHandle() const
{
	contract_assert( m_pBinding );
	m_pBinding->m_nLastBindFrame = g_nResourceFrameCount;
	return m_pBinding;
}

template< class T >
[[gnu::always_inline]] inline CStrongHandle< T >::operator const T*() const
{
	contract_assert( m_pBinding );
	m_pBinding->m_nLastBindFrame = g_nResourceFrameCount;
	return ( const T* )m_pBinding->m_pData;
}

template< class T >
[[gnu::always_inline]] inline const T *CStrongHandle< T >::operator->() const
{
	contract_assert( m_pBinding );
	m_pBinding->m_nLastBindFrame = g_nResourceFrameCount;
	return ( const T* )m_pBinding->m_pData;
}

template< class T >
[[gnu::always_inline]] inline CStrongHandle< T >::operator ResourceHandleTyped_t() const
{
	contract_assert( m_pBinding );
	m_pBinding->m_nLastBindFrame = g_nResourceFrameCount;
	return m_pBinding;
}

template< class T >
[[gnu::always_inline]] inline CStrongHandle< T >::operator ResourceHandle_t() const
{
	contract_assert( m_pBinding );
	m_pBinding->m_nLastBindFrame = g_nResourceFrameCount;
	return (ResourceHandle_t)m_pBinding;
}


//-----------------------------------------------------------------------------
// Comparison operators
//-----------------------------------------------------------------------------
template< class T >
inline bool CStrongHandle< T >::operator==( ResourceHandle_t hResource ) const
{
	return m_pBinding == hResource;
}

template< class T >
inline bool CStrongHandle< T >::operator==( ResourceHandleTyped_t hResource ) const
{
	return m_pBinding == hResource;
}

template< class T >
inline bool CStrongHandle< T >::operator==( const CStrongHandle< T > &hResource ) const
{
	return m_pBinding == hResource.m_pBinding;
}


template< class T >
inline bool CStrongHandle< T >::operator!=( ResourceHandle_t hResource ) const
{
	return m_pBinding != hResource;
}

template< class T >
inline bool CStrongHandle< T >::operator!=( ResourceHandleTyped_t hResource ) const
{
	return m_pBinding != hResource;
}

template< class T >
inline bool CStrongHandle< T >::operator!=( const CStrongHandle< T > &hResource ) const
{
	return m_pBinding != hResource.m_pBinding;
}


//-----------------------------------------------------------------------------
// Is the resource actually in memory
//-----------------------------------------------------------------------------
template< class T >
inline bool CStrongHandle< T >::IsCached() const
{
	contract_assert( m_pBinding );
	return ( m_pBinding->m_nFlags & RESOURCE_BINDING_CACHED ) != 0;
}


//-----------------------------------------------------------------------------
// Marks the resource as being used this frame
//-----------------------------------------------------------------------------
template< class T >
inline void CStrongHandle< T >::MarkUsed()
{
	m_pBinding->m_nLastBindFrame = g_nResourceFrameCount;
}


//-----------------------------------------------------------------------------
// Forces the resource to be brought into memory 
//-----------------------------------------------------------------------------
template< class T >
void CStrongHandle< T >::CacheResource()
{
	contract_assert( m_pBinding );
	IResourceTypeManager *pMgr = g_pResourceSystem->GetResourceManagerForType< T >();
	pMgr->CacheResource( (ResourceHandle_t)m_pBinding );
}

template< class T >
void CStrongHandle< T >::UncacheResource()
{
	contract_assert( m_pBinding );
	IResourceTypeManager *pMgr = g_pResourceSystem->GetResourceManagerForType< T >();
	pMgr->UncacheResource( (ResourceHandle_t)m_pBinding );
}


//-----------------------------------------------------------------------------
// Forces a reload of the resource
//-----------------------------------------------------------------------------
template< class T >
void CStrongHandle< T >::ReloadResource()
{
	contract_assert( m_pBinding );
	IResourceTypeManager *pMgr = g_pResourceSystem->GetResourceManagerForType< T >();
	pMgr->UncacheResource( (ResourceHandle_t)m_pBinding );
	pMgr->CacheResource( (ResourceHandle_t)m_pBinding );
}
