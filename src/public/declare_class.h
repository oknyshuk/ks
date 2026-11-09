#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// The class declaration helpers that do not depend on networkvar.h (which belongs to the game side
// that includes it), so that code built once can use them.

// network vars use memcmp when fields are set.  To ensure proper behavior your
// object's memory should be initialized to zero.  This happens for entities automatically
// use this for other classes.
class CMemZeroOnNew
{
public:
	void *operator new( std::size_t nSize )
	{
		void *pMem = MemAlloc_Alloc( nSize );
		V_memset( pMem, 0, nSize );
		return pMem;
	}

	void* operator new( std::size_t nSize, int nBlockUse, const char *pFileName, int nLine )
	{
		void *pMem = MemAlloc_Alloc( nSize, pFileName, nLine );
		V_memset( pMem, 0, nSize );
		return pMem;
	}

	void operator delete(void *pData)
	{
		if ( pData )
		{
			g_pMemAlloc->Free(pData);
		}
	}

	void operator delete( void* pData, int nBlockUse, const char *pFileName, int nLine )
	{
		if ( pData )
		{
			g_pMemAlloc->Free(pData, pFileName, nLine );
		}
	}
};


inline int InternalCheckDeclareClass( const char *pClassName, const char *pClassNameMatch, void *pTestPtr, void *pBasePtr )
{
	// This makes sure that casting from ThisClass to BaseClass works right. You'll get a compiler error if it doesn't
	// work at all, and you'll get a runtime error if you use multiple inheritance.
	contract_assert( pTestPtr == pBasePtr );
	
	// This is triggered by IMPLEMENT_SERVER_CLASS. It does DLLClassName::CheckDeclareClass( #DLLClassName ).
	// If they didn't do a DECLARE_CLASS in DLLClassName, then it'll be calling its base class's version
	// and the class names won't match.
	contract_assert( (void*)pClassName == (void*)pClassNameMatch );
	return 0;
}


template <typename T> 
inline int CheckDeclareClass_Access( T *, const char *pShouldBe )
{
	return T::CheckDeclareClass( pShouldBe );
}


	#define DECLARE_CLASS( className, baseClassName ) \
		typedef baseClassName BaseClass; \
		typedef className ThisClass;

	#define DECLARE_CLASS_GAMEROOT( className, baseClassName )	DECLARE_CLASS( className, baseClassName )
	#define DECLARE_CLASS_NOFRIEND( className, baseClassName )	DECLARE_CLASS( className, baseClassName )

	#define DECLARE_CLASS_NOBASE( className )					typedef className ThisClass;
