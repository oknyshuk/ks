// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.tier1:byte_writer;

import std;
import ks.math;
export import :byte_reader;

export extern "C++" {

namespace ks
{
	class byte_writer
	{
	public:
		byte_writer() = default;
		explicit byte_writer( std::size_t reserve_bytes )			{ m_owned.reserve( reserve_bytes ); }

		explicit byte_writer( std::span< std::byte > fixed ) : m_fixed( true ), m_ext( fixed ) {}
		byte_writer( void *p, std::size_t n ) : byte_writer( std::span< std::byte >( static_cast< std::byte * >( p ), n ) ) {}

		static byte_writer adopt( std::vector< std::byte > bytes )
		{
			byte_writer w;
			w.m_owned = std::move( bytes );
			w.m_size = w.m_pos = w.m_owned.size();
			return w;
		}

		byte_writer( const byte_writer & ) = default;
		byte_writer &operator=( const byte_writer & ) = default;
		byte_writer( byte_writer &&o ) noexcept					{ *this = std::move( o ); }
		byte_writer &operator=( byte_writer &&o ) noexcept
		{
			m_owned = std::move( o.m_owned );
			m_ext = o.m_ext;
			m_fixed = o.m_fixed;
			m_size = std::exchange( o.m_size, 0 );
			m_pos = std::exchange( o.m_pos, 0 );
			m_failed = std::exchange( o.m_failed, false );
			return *this;
		}

		bool ok() const noexcept									{ return !m_failed; }
		explicit operator bool() const noexcept						{ return !m_failed; }
		void fail() noexcept										{ m_failed = true; }

		std::size_t tell() const noexcept							{ return m_pos; }

		std::size_t size() const noexcept							{ return m_size; }
		bool empty() const noexcept									{ return m_size == 0; }

		void seek( std::size_t pos )
		{
			if ( pos > m_size && !extend( pos ) )
				return;
			m_pos = pos;
		}

		void seek_end() noexcept									{ m_pos = m_size; }

		std::span< const std::byte > bytes() const noexcept			{ return { base(), m_size }; }
		std::span< std::byte > bytes() noexcept						{ return { base(), m_size }; }
		std::byte *data() noexcept									{ return base(); }
		const std::byte *data() const noexcept						{ return base(); }

		byte_reader reader() const noexcept							{ return byte_reader( bytes() ); }

		std::vector< std::byte > release() noexcept
		{
			std::vector< std::byte > v = std::move( m_owned );
			v.resize( m_size );
			clear();
			return v;
		}

		void reserve( std::size_t n )								{ if ( !m_fixed ) m_owned.reserve( n ); }

		void clear() noexcept
		{
			m_owned.clear();
			m_size = m_pos = 0;
			m_failed = false;
		}

		template < wire_type T >
		void write( const T &v )									{ write_bytes( &v, sizeof( T ) ); }

		void write_bytes( const void *p, std::size_t n )
		{
			if ( !n )
				return;
			if ( std::byte *dst = claim( n ) )
				std::memcpy( dst, p, n );
		}

		void write_bytes( std::span< const std::byte > s )			{ write_bytes( s.data(), s.size() ); }

		void write_cstring( std::string_view s )
		{
			if ( std::byte *dst = claim( s.size() + 1 ) )
			{
				if ( !s.empty() )
					std::memcpy( dst, s.data(), s.size() );
				dst[s.size()] = std::byte{ 0 };
			}
		}

		void write_chars( std::string_view s )						{ write_bytes( s.data(), s.size() ); }

		void fill( std::size_t n, std::byte value = std::byte{ 0 } )
		{
			if ( std::byte *dst = claim( n ) )
				std::memset( dst, static_cast< int >( value ), n );
		}

		std::span< std::byte > claim_span( std::size_t n )
		{
			std::byte *dst = claim( n );
			return dst ? std::span< std::byte >( dst, n ) : std::span< std::byte >();
		}

	private:
		std::byte *base() noexcept									{ return m_fixed ? m_ext.data() : m_owned.data(); }
		const std::byte *base() const noexcept						{ return m_fixed ? m_ext.data() : m_owned.data(); }
		std::size_t capacity() const noexcept						{ return m_fixed ? m_ext.size() : m_owned.size(); }

		bool extend( std::size_t n )
		{
			if ( m_fixed )
			{
				if ( n > m_ext.size() )
				{
					m_failed = true;
					return false;
				}
				std::memset( m_ext.data() + m_size, 0, n - m_size );
			}
			else if ( n > m_owned.size() )
			{
				m_owned.resize( std::max( n, m_owned.size() * 2 ) );
			}
			m_size = n;
			return true;
		}

		std::byte *claim( std::size_t n )
		{
			const std::size_t end = m_pos + n;
			if ( end < m_pos )
			{
				m_failed = true;
				return nullptr;
			}
			if ( end > capacity() )
			{
				if ( m_fixed )
				{
					m_failed = true;
					return nullptr;
				}
				m_owned.resize( std::max( end, m_owned.size() * 2 ) );
			}
			std::byte *dst = base() + m_pos;
			m_pos = end;
			if ( end > m_size )
				m_size = end;
			return dst;
		}

		std::vector< std::byte > m_owned;
		std::span< std::byte > m_ext;
		bool m_fixed = false;
		std::size_t m_size = 0;
		std::size_t m_pos = 0;
		bool m_failed = false;
	};
}

}
