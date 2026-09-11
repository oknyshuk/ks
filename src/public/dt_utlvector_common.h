//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DT_UTLVECTOR_COMMON_H
#define DT_UTLVECTOR_COMMON_H


#include "utlvector.h"


typedef void (*EnsureCapacityFn)( void *pVoid, int offsetToUtlVector, int len );
typedef void (*ResizeUtlVectorFn)( void *pVoid, int offsetToUtlVector, int len );

template< class T >
void UtlVector_InitializeAllocatedElements( T *pBase, int count )
{
	memset( (void *)pBase, 0, count * sizeof( T ) );
}

template< class T, class A >
class UtlVectorTemplate
{
public:
	static void ResizeUtlVector( void *pStruct, int offsetToUtlVector, int len )
	{
		CUtlVector<T,A> *pVec = (CUtlVector<T,A>*)((char*)pStruct + offsetToUtlVector);
		if ( pVec->Count() < len )
			pVec->AddMultipleToTail( len - pVec->Count() );
		else if ( pVec->Count() > len )
			pVec->RemoveMultiple( len, pVec->Count()-len );

		// Ensure capacity
		pVec->EnsureCapacity( len );

		int nNumAllocated = pVec->NumAllocated();

		// This is important to do because EnsureCapacity doesn't actually call the constructors
		// on the elements, but we need them to be initialized, otherwise it'll have out-of-range
		// values which will piss off the datatable encoder.
		UtlVector_InitializeAllocatedElements( pVec->Base() + pVec->Count(), nNumAllocated - pVec->Count() );
	}

	static void EnsureCapacity( void *pStruct, int offsetToUtlVector, int len )
	{
		CUtlVector<T,A> *pVec = (CUtlVector<T,A>*)((char*)pStruct + offsetToUtlVector);

		pVec->EnsureCapacity( len );
		
		int nNumAllocated = pVec->NumAllocated();

		// This is important to do because EnsureCapacity doesn't actually call the constructors
		// on the elements, but we need them to be initialized, otherwise it'll have out-of-range
		// values which will piss off the datatable encoder.
		UtlVector_InitializeAllocatedElements( pVec->Base() + pVec->Count(), nNumAllocated - pVec->Count() );
	}
};

template< class T, class A >
inline ResizeUtlVectorFn GetResizeUtlVectorTemplate( CUtlVector<T,A> &vec )
{
	return &UtlVectorTemplate<T,A>::ResizeUtlVector;
}

template< class T, class A >
inline EnsureCapacityFn GetEnsureCapacityTemplate( CUtlVector<T,A> &vec )
{
	return &UtlVectorTemplate<T,A>::EnsureCapacity;
}


// The wire-side view of a resizable vector member.
//
// Send/RecvPropUtlVector need only an element type and two thunks that reach the vector through
// (this, offset) and know nothing of its layout. So the table emitters must not name CUtlVector:
// they ask this trait instead. Wiring a different vector kind is then one specialization, and
// the reflection layer stops depending on the container layer.
//
// The CUtlVector<T,A> partial specialization keys on the container, not on an instance -- the
// emitters used to default-construct a probe object purely to deduce T and A through the
// Get*Template overloads above, which also required the member type to be default-constructible.
template <class V>
struct WireVec;

template <class T, class A>
struct WireVec< CUtlVector<T,A> >
{
	using elem_type = T;

	static EnsureCapacityFn ensure() { return &UtlVectorTemplate<T,A>::EnsureCapacity; }
	static ResizeUtlVectorFn resize() { return &UtlVectorTemplate<T,A>::ResizeUtlVector; }
};


// Format and allocate a string.
char* AllocateStringHelper( PRINTF_FORMAT_STRING const char *pFormat, ... );

// Allocates a string for a data table name. Data table names must be unique, so this will
// assert if you try to allocate a duplicate.
char* AllocateUniqueDataTableName( bool bSendTable, PRINTF_FORMAT_STRING const char *pFormat, ... );


#endif // DT_UTLVECTOR_COMMON_H
