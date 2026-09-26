//========== Copyright (c) 2006, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#ifndef BONETOWORLDARRAY_H
#define BONETOWORLDARRAY_H

#include "tier0/tslist.h"


#include "tier0/memalloc.h"
//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
template <int NUM_ARRAYS>
class CBoneToWorldArrays
{
public:
	enum
	{
		ALIGNMENT = 128,
	};

	CBoneToWorldArrays()
	{
		const int SIZE_ARRAY = AlignValue( sizeof(matrix3x4_t) * MAXSTUDIOBONES, ALIGNMENT );
		m_pBase = (matrix3x4_t *)MemAlloc_AllocAligned( SIZE_ARRAY * NUM_ARRAYS, ALIGNMENT );
		for ( int i = 0; i < NUM_ARRAYS; i++ )
		{
			matrix3x4_t *pArray = (matrix3x4_t *)((byte *)m_pBase + SIZE_ARRAY * i);
			Assert( (size_t)pArray % ALIGNMENT == 0 );
			Free( pArray );
		}
	}

	~CBoneToWorldArrays()
	{
		MemAlloc_FreeAligned( m_pBase );
	}

	int NumArrays()
	{
		return NUM_ARRAYS;
	}

	matrix3x4_t *Alloc( bool bBlock = true )
	{
		TSLNodeBase_t *p;
		while ( ( p = m_Free.Pop() ) == nullptr && bBlock )
		{
			ThreadPause();
		}
		return (matrix3x4_t *)p;
	}

	void Free( matrix3x4_t *p )
	{
		m_Free.Push( (TSLNodeBase_t *) p );
	}

private:
	CTSListBase m_Free;
	matrix3x4_t *m_pBase;
};

#endif // BONETOWORLDARRAY_H
