//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DT_UTLVECTOR_COMMON_H
#define DT_UTLVECTOR_COMMON_H


#include "utlvector.h"
#include <type_traits>
#include <utility>


// A vector prop is a "lengthproxy" subtable plus one element prop per slot. Neither end touches
// storage past the count: slots past it have a null base (encoder omits them, decoder skips them),
// and the decoder applies lengths before routing element proxies (RecvTable_Decode).

typedef void (*ResizeUtlVectorFn)( void *pStruct, int offsetToUtlVector, int len );
typedef void *(*GetVectorBaseFn)( const void *pStruct, int offsetToUtlVector );
typedef int (*GetVectorCountFn)( const void *pStruct, int offsetToUtlVector );

struct WireVecOps
{
	ResizeUtlVectorFn resize = nullptr;   // new elements value-initialised
	GetVectorBaseFn   base   = nullptr;
	GetVectorCountFn  count  = nullptr;
};


// Customization points. Anything std-style works as is; other containers can overload these.
template < class V >
concept StdStyleVector = requires ( V &v ) { v.data(); v.size(); v.resize( 0 ); };

template < StdStyleVector V > auto *wire_data( V &v )				{ return v.data(); }
template < StdStyleVector V > int   wire_size( const V &v )			{ return (int)v.size(); }
template < StdStyleVector V > void  wire_resize( V &v, int n )		{ v.resize( (size_t)n ); }


// The wire-side view of a vector member; the emitters ask this instead of naming a container.
template < class V >
struct WireVec
{
	using elem_type = std::remove_pointer_t< decltype( wire_data( std::declval< V & >() ) ) >;

	static constexpr WireVecOps ops()
	{
		return { .resize = []( void *s, int o, int n ) { wire_resize( at( s, o ), n ); },
		         .base   = []( const void *s, int o ) -> void * { return wire_data( at( s, o ) ); },
		         .count  = []( const void *s, int o ) { return wire_size( at( s, o ) ); } };
	}

private:
	static V &at( const void *s, int o ) { return *(V *)(void *)( (const char *)s + o ); }
};


// Format and allocate a string.
char* AllocateStringHelper( PRINTF_FORMAT_STRING const char *pFormat, ... );

// Allocates a string for a data table name. Data table names must be unique, so this will
// assert if you try to allocate a duplicate.
char* AllocateUniqueDataTableName( bool bSendTable, PRINTF_FORMAT_STRING const char *pFormat, ... );


#endif // DT_UTLVECTOR_COMMON_H
