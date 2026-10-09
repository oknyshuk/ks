// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "changeframelist.h"

//-----------------------------------------------------------------------------
ks::fixed_pool_mt CChangeFrameList::s_Allocator( sizeof(CChangeFrameList), 2048, ks::fixed_pool::grow::fast, "CChangeFrameList pool", alignof(CChangeFrameList) );

//-----------------------------------------------------------------------------

CChangeFrameList::CChangeFrameList( int nProperties, int iCurTick )
{
	//determine how many buckets we need for our properties
	int nNumBuckets = ( nProperties + knBucketSize - 1 ) / knBucketSize;
	m_nNumProps = nProperties;

	m_ChangeTicks.assign( nProperties + nNumBuckets, {} );
	std::ranges::fill( m_ChangeTicks, iCurTick );
}

CChangeFrameList::~CChangeFrameList()
{
}

void CChangeFrameList::Release()
{
	delete this;
}

CChangeFrameList::CChangeFrameList( const CChangeFrameList &rhs )
{
	m_ChangeTicks = rhs.m_ChangeTicks;
	m_nNumProps = rhs.m_nNumProps;
}

CChangeFrameList *CChangeFrameList::Copy()
{
	CChangeFrameList *pRet = new CChangeFrameList( *this );
	return pRet;
}

void CChangeFrameList::SetChangeTick( const int* __restrict__ pProps, int nNumProps, const int iTick )
{
	//avoid loads inside the array by the compiler thinking they could overlap
	int* pBuckets		= m_ChangeTicks.data() + m_nNumProps;
	for ( int i=0; i < nNumProps; i++ )
	{
		//update our tick, and the parent bucket for the tick
		m_ChangeTicks[ pProps[i] ] = iTick;
		pBuckets[ pProps[i] / knBucketSize ] = iTick;		
	}
}
