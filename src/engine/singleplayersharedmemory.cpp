// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.dbg;
import ks.core.types;
import ks.platform;
import ks.simd;
import ks.tier1.string_ops;
import ks.tier1.vector_ops;
#include "singleplayersharedmemory.h"
import std;

CSPSharedMemoryManager s_SinglePlayerSharedMemoryManager;
CSPSharedMemoryManager *g_pSinglePlayerSharedMemoryManager = &s_SinglePlayerSharedMemoryManager;

class CSPSharedMemory : public ISPSharedMemory
{
public:
	CSPSharedMemory( void ) : m_pMemory(nullptr), m_Size(0), m_iRefCount(0) { };
	~CSPSharedMemory( void );

	bool	Init( std::size_t iSize ); //Initial implementation assumes the size is fixed/hardcoded
	uint8 *	Base( void );
	std::size_t	Size( void );

	void	AddRef( void );
	void	Release( void );

	
	std::string m_Name;
	int m_iEntNum;
	uint8 *m_pMemory;
	std::size_t m_Size;
	unsigned int m_iRefCount;
};

#include "facade/ispsharedmemory.inl"


CSPSharedMemory::~CSPSharedMemory( void )
{
	contract_assert( m_iRefCount == 0 );
	
	//if( m_pMemory )
	//	delete []m_pMemory;
}

bool CSPSharedMemory::Init( std::size_t iSize )
{
	contract_assert( iSize != 0 );
	contract_assert( (m_Size == 0) || (m_Size == iSize) );

	if( m_Size == 0 )
	{
		m_pMemory = new uint8 [iSize];
		m_Size = iSize;
		std::memset( m_pMemory, 0, iSize );
		return true;
	}
	return false;
}

uint8 * CSPSharedMemory::Base( void )
{
	contract_assert( m_pMemory != nullptr );
	return m_pMemory;
}

std::size_t CSPSharedMemory::Size( void )
{
	return m_Size;
}

void CSPSharedMemory::AddRef( void )
{
	++m_iRefCount;
}

void CSPSharedMemory::Release( void )
{
	--m_iRefCount;
	
	if( m_iRefCount == 0 )
	{
		for( int i = std::ssize( s_SinglePlayerSharedMemoryManager.m_SharedSpaces ); --i >= 0; )
		{
			if( s_SinglePlayerSharedMemoryManager.m_SharedSpaces[i] == this )
			{
				ks::swap_remove( s_SinglePlayerSharedMemoryManager.m_SharedSpaces, i );
				break;
			}
		}
		//delete this;
	}
}




CSPSharedMemoryManager::~CSPSharedMemoryManager( void )
{
	contract_assert( std::ssize( m_SharedSpaces ) == 0 );

	for( int i = std::ssize( m_SharedSpaces ); --i >= 0; )
	{
		Warning( "Shared memory space %s (%i) still has %i references.\n", m_SharedSpaces[i]->m_Name.c_str(), m_SharedSpaces[i]->m_iEntNum, m_SharedSpaces[i]->m_iRefCount );
		//leak it?
	}
}


ISPSharedMemory *CSPSharedMemoryManager::GetSharedMemory( const char *szName, int ent_num )
{
	for( int i = std::ssize( m_SharedSpaces ); --i >= 0; )
	{
		if( m_SharedSpaces[i]->m_Name == szName && m_SharedSpaces[i]->m_iEntNum == ent_num )
		{
			m_SharedSpaces[i]->AddRef();
			return m_SharedSpaces[i];
		}
	}

	//create a new one
	CSPSharedMemory *pNew = new CSPSharedMemory;
	pNew->m_Name = ks::nz( szName );
	pNew->m_iEntNum = ent_num;
	m_SharedSpaces.push_back( pNew );
	pNew->AddRef();
	return pNew;
}
