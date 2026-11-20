// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.tier1.envelope;

import std;
import ks.core.basetypes;
import ks.platform;
import ks.simd;
export import ks.tier1.small_vector;
export import ks.tier1.string_ops;

export extern "C++" {

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

}
