// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#ifndef BYTE_READER_H
#define BYTE_READER_H

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>
#include <type_traits>

namespace ks
{
	inline std::span< const std::byte > byte_view( const void *p, std::size_t n )
	{
		return { static_cast< const std::byte * >( p ), n };
	}

	inline std::span< const std::byte > byte_view( std::string_view s )
	{
		return byte_view( s.data(), s.size() );
	}

	inline std::string_view text_view( std::span< const std::byte > b )
	{
		return { reinterpret_cast< const char * >( b.data() ), b.size() };
	}

	template < class T >
	concept wire_type = std::is_trivially_copyable_v< T > && !std::is_pointer_v< T >;

	class byte_reader
	{
	public:
		byte_reader() = default;
		explicit byte_reader( std::span< const std::byte > bytes ) : m_bytes( bytes ) {}
		byte_reader( const void *p, std::size_t n ) : m_bytes( byte_view( p, n ) ) {}

		bool ok() const noexcept									{ return !m_failed; }
		explicit operator bool() const noexcept						{ return !m_failed; }
		void fail() noexcept										{ m_failed = true; }

		std::size_t tell() const noexcept							{ return m_pos; }
		std::size_t size() const noexcept							{ return m_bytes.size(); }
		std::size_t remaining() const noexcept						{ return m_bytes.size() - m_pos; }
		bool at_end() const noexcept								{ return m_pos >= m_bytes.size(); }

		// a seek that lands inside the data clears the failed state
		void seek( std::size_t pos ) noexcept
		{
			if ( pos > m_bytes.size() )
			{
				m_failed = true;
			}
			else
			{
				m_pos = pos;
				m_failed = false;
			}
		}

		void skip( std::size_t n ) noexcept
		{
			if ( m_failed )
				return;
			if ( n > remaining() )
				m_failed = true;
			else
				m_pos += n;
		}

		std::span< const std::byte > bytes() const noexcept			{ return m_bytes; }
		std::span< const std::byte > rest() const noexcept			{ return m_bytes.subspan( m_pos ); }

		std::span< const std::byte > peek_span( std::size_t n, std::size_t offset = 0 ) const noexcept
		{
			if ( offset > remaining() || n > remaining() - offset )
				return {};
			return m_bytes.subspan( m_pos + offset, n );
		}

		template < wire_type T >
		T peek( std::size_t offset = 0 ) const noexcept
		{
			T v{};
			const auto s = peek_span( sizeof( T ), offset );
			if ( !s.empty() )
				std::memcpy( &v, s.data(), sizeof( T ) );
			return v;
		}

		template < wire_type T >
		T read() noexcept
		{
			T v{};
			read_bytes( &v, sizeof( T ) );
			return v;
		}

		template < wire_type T >
		bool read( T &out ) noexcept
		{
			out = T{};
			return read_bytes( &out, sizeof( T ) );
		}

		bool read_bytes( void *dst, std::size_t n ) noexcept
		{
			if ( m_failed )
				return false;
			if ( n > remaining() )
			{
				m_failed = true;
				return false;
			}
			if ( n )
				std::memcpy( dst, m_bytes.data() + m_pos, n );
			m_pos += n;
			return true;
		}

		std::span< const std::byte > read_span( std::size_t n ) noexcept
		{
			if ( m_failed )
				return {};
			if ( n > remaining() )
			{
				m_failed = true;
				return {};
			}
			const auto s = m_bytes.subspan( m_pos, n );
			m_pos += n;
			return s;
		}

		std::size_t read_up_to( void *dst, std::size_t n ) noexcept
		{
			if ( m_failed )
				return 0;
			n = std::min( n, remaining() );
			if ( n )
				std::memcpy( dst, m_bytes.data() + m_pos, n );
			m_pos += n;
			return n;
		}

		std::string_view read_cstring() noexcept
		{
			if ( m_failed )
				return {};
			const char *begin = reinterpret_cast< const char * >( m_bytes.data() + m_pos );
			const void *nul = remaining() ? std::memchr( begin, 0, remaining() ) : nullptr;
			if ( !nul )
			{
				const std::string_view s( begin, remaining() );
				m_pos = m_bytes.size();
				m_failed = true;
				return s;
			}
			const std::size_t len = static_cast< const char * >( nul ) - begin;
			m_pos += len + 1;
			return { begin, len };
		}

		void read_cstring( char *dst, std::size_t max_chars ) noexcept
		{
			const std::string_view s = read_cstring();
			if ( !max_chars )
				return;
			const std::size_t n = std::min( s.size(), max_chars - 1 );
			std::memcpy( dst, s.data(), n );
			dst[n] = 0;
		}

	private:
		std::span< const std::byte > m_bytes;
		std::size_t m_pos = 0;
		bool m_failed = false;
	};
}

#endif
