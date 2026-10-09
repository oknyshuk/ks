// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.tier1:text_reader;

import std;
import ks.math;
export import :string_escapes;

export extern "C++" {

struct characterset_t;

namespace ks
{
	class text_reader
	{
	public:
		text_reader() = default;
		explicit text_reader( std::string_view text ) : m_text( text ) {}
		text_reader( const void *p, std::size_t n ) : m_text( static_cast< const char * >( p ), n ) {}

		bool ok() const noexcept									{ return !m_failed; }
		explicit operator bool() const noexcept						{ return !m_failed; }
		void fail() noexcept										{ m_failed = true; }

		std::size_t tell() const noexcept							{ return m_pos; }
		std::size_t size() const noexcept							{ return m_text.size(); }
		std::size_t remaining() const noexcept						{ return m_text.size() - m_pos; }
		bool at_end() const noexcept								{ return m_pos >= m_text.size(); }

		// a seek that lands inside the data clears the failed state
		void seek( std::size_t pos ) noexcept
		{
			if ( pos > m_text.size() )
			{
				m_failed = true;
			}
			else
			{
				m_pos = pos;
				m_failed = false;
			}
		}

		std::string_view text() const noexcept						{ return m_text; }
		std::string_view rest() const noexcept						{ return m_text.substr( m_pos ); }

		char peek_char( std::size_t offset = 0 ) const noexcept		{ return m_pos + offset < m_text.size() ? m_text[m_pos + offset] : '\0'; }

		bool starts_with( std::string_view s ) const noexcept		{ return rest().starts_with( s ); }

		char get_char() noexcept;

		template < class T >
			requires std::is_arithmetic_v< T >
		T read( int radix = 10 ) noexcept
		{
			T v{};
			read( v, radix );
			return v;
		}

		template < class T >
			requires std::is_arithmetic_v< T >
		bool read( T &out, int radix = 10 ) noexcept;

		std::string_view read_word() noexcept;

		std::string_view read_line() noexcept;

		void skip_whitespace() noexcept;

		bool skip_cpp_comment() noexcept;

		bool skip_past( std::string_view token ) noexcept;

		std::optional< std::string_view > read_between( std::string_view start, std::string_view end ) noexcept;

		std::optional< std::string_view > read_token( const characterset_t &breaks, bool parse_comments = true ) noexcept;

		std::string read_quoted( const string_escapes &style ) noexcept;

		std::size_t peek_quoted_length( const string_escapes &style, bool actual_size = true ) const noexcept;

		int scanf( const char *fmt, ... ) noexcept __attribute__( ( format( scanf, 2, 3 ) ) );

	private:
		std::size_t peek_whitespace( std::size_t offset ) const noexcept;

		std::string_view m_text;
		std::size_t m_pos = 0;
		bool m_failed = false;
	};

	extern template bool text_reader::read< signed char >( signed char &, int ) noexcept;
	extern template bool text_reader::read< unsigned char >( unsigned char &, int ) noexcept;
	extern template bool text_reader::read< short >( short &, int ) noexcept;
	extern template bool text_reader::read< unsigned short >( unsigned short &, int ) noexcept;
	extern template bool text_reader::read< int >( int &, int ) noexcept;
	extern template bool text_reader::read< unsigned int >( unsigned int &, int ) noexcept;
	extern template bool text_reader::read< long >( long &, int ) noexcept;
	extern template bool text_reader::read< unsigned long >( unsigned long &, int ) noexcept;
	extern template bool text_reader::read< long long >( long long &, int ) noexcept;
	extern template bool text_reader::read< unsigned long long >( unsigned long long &, int ) noexcept;
	extern template bool text_reader::read< float >( float &, int ) noexcept;
	extern template bool text_reader::read< double >( double &, int ) noexcept;
}

}
