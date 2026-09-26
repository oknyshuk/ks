// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// Wraps a copy of some data so it can travel across a boundary like a thread or a call queue: the
// arguments of a queued call may not point into memory that will be gone by the time the call runs.
// Constructing an envelope copies the elements out of the caller's memory; the envelope converts back
// to a T * (or void *) that stays valid as long as the envelope does. A single element is stored inline.
// T must be trivially copyable. A null source gives a null pointer back.

#ifndef ENVELOPE_H
#define ENVELOPE_H

#include "tier1/small_vector.h"
#include "tier1/string_ops.h"

#include <string>
#include <type_traits>

namespace ks
{
	template < class T >
	class envelope
	{
		static_assert( std::is_trivially_copyable_v< T >, "envelope copies its data bytewise" );

	public:
		envelope( const T *pData, int nElems = 1 )
		{
			if ( pData && nElems > 0 )
				m_data.assign( pData, pData + nElems );
		}

		operator T *() const		{ return m_data.empty() ? nullptr : const_cast< T * >( m_data.data() ); }
		operator void *() const		{ return static_cast< T * >( *this ); }

	private:
		small_vector< T, 1 > m_data;
	};

	// Strings are copied with their terminator, and the copy is never null.
	template <>
	class envelope< const char * >
	{
	public:
		envelope( const char *pData )					: m_string( ks::nz( pData ) ) {}

		operator char *() const							{ return const_cast< char * >( m_string.c_str() ); }
		operator void *() const							{ return const_cast< char * >( m_string.c_str() ); }

	private:
		std::string m_string;
	};
}

#endif // ENVELOPE_H
