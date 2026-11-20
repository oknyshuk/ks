// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.tier1.text_writer;

import std;
import ks.core.basetypes;
import ks.core.vprof;
import ks.platform;
import ks.simd;
export import ks.tier1.string_escapes;

export extern "C++" {

namespace ks
{
	class text_writer
	{
	public:
		text_writer() = default;
		explicit text_writer( std::string initial ) : m_text( std::move( initial ) ) {}

		const std::string &str() const &noexcept					{ return m_text; }
		std::string release() noexcept
		{
			std::string s = std::move( m_text );
			clear();
			return s;
		}
		std::string_view view() const noexcept						{ return m_text; }
		const char *c_str() const noexcept							{ return m_text.c_str(); }
		std::span< const std::byte > bytes() const noexcept			{ return std::as_bytes( std::span( m_text ) ); }
		std::size_t size() const noexcept							{ return m_text.size(); }
		bool empty() const noexcept									{ return m_text.empty(); }
		void reserve( std::size_t n )								{ m_text.reserve( n ); }
		void clear() noexcept										{ m_text.clear(); m_tabs = 0; }
		void resize( std::size_t n )								{ m_text.resize( n ); }

		void push_tab() noexcept									{ ++m_tabs; }
		void pop_tab() noexcept										{ if ( m_tabs > 0 ) --m_tabs; }
		void enable_tabs( bool enable ) noexcept					{ m_tabs_enabled = enable; }

		void put( char c )
		{
			if ( at_line_start() )
				put_tabs();
			m_text.push_back( c );
		}

		void put( std::string_view s )
		{
			if ( !m_tabs || !m_tabs_enabled )
			{
				m_text.append( s );
				return;
			}
			if ( at_line_start() )
				put_tabs();
			while ( !s.empty() )
			{
				const std::size_t eol = s.find( '\n' );
				if ( eol == std::string_view::npos )
				{
					m_text.append( s );
					return;
				}
				m_text.append( s.substr( 0, eol + 1 ) );
				s.remove_prefix( eol + 1 );
				if ( !s.empty() )
					put_tabs();
			}
		}

		void put( const char *s )									{ if ( s ) put( std::string_view( s ) ); }

		void put_raw( std::string_view s )							{ m_text.append( s ); }

		template < class T >
			requires std::is_arithmetic_v< T > && ( !std::is_same_v< T, char > )
		void put( T v )
		{
			if constexpr ( std::is_same_v< T, bool > )
				put( static_cast< int >( v ) );
			else if constexpr ( std::is_floating_point_v< T > )
				printf( std::is_same_v< T, double > ? "%.15f" : "%f", static_cast< double >( v ) );
			else if constexpr ( std::is_signed_v< T > )
				printf( "%lld", static_cast< long long >( v ) );
			else
				printf( "%llu", static_cast< unsigned long long >( v ) );
		}

		void put_hex( unsigned int v )								{ printf( "%x", v ); }

		void put_quoted( const string_escapes &style, std::string_view s )
		{
			if ( at_line_start() )
				put_tabs();
			m_text.push_back( style.delimiter() );
			for ( const char c : s )
			{
				const char spelling = style.spelling_of( c );
				if ( !spelling )
				{
					put( c );
				}
				else
				{
					put( style.escape_char() );
					m_text.push_back( spelling );
				}
			}
			if ( at_line_start() )
				put_tabs();
			m_text.push_back( style.delimiter() );
		}

		void printf( const char *fmt, ... ) __attribute__( ( format( printf, 2, 3 ) ) )
		{
			std::va_list args;
			__builtin_va_start( args, fmt );
			vprintf( fmt, args );
			__builtin_va_end( args );
		}

		void vprintf( const char *fmt, std::va_list args )
		{
			std::va_list copy;
			__builtin_va_copy( copy, args );
			char small[512];
			const int n = std::vsnprintf( small, sizeof( small ), fmt, copy );
			__builtin_va_end( copy );
			if ( n < 0 )
				return;
			if ( static_cast< std::size_t >( n ) < sizeof( small ) )
			{
				put( std::string_view( small, n ) );
				return;
			}
			std::string big( n, '\0' );
			std::vsnprintf( big.data(), n + 1, fmt, args );
			put( big );
		}

		template < class... Args >
		void format( std::format_string< Args... > fmt, Args &&...args )
		{
			put( std::format( fmt, std::forward< Args >( args )... ) );
		}

	private:
		bool at_line_start() const noexcept							{ return m_tabs && m_tabs_enabled && !m_text.empty() && m_text.back() == '\n'; }
		void put_tabs()
		{
			if ( m_tabs_enabled )
				m_text.append( m_tabs, '\t' );
		}

		std::string m_text;
		int m_tabs = 0;
		bool m_tabs_enabled = true;
	};

	inline text_writer &operator<<( text_writer &w, char v )					{ w.put( v ); return w; }
	inline text_writer &operator<<( text_writer &w, const char *v )				{ w.put( v ); return w; }
	inline text_writer &operator<<( text_writer &w, std::string_view v )		{ w.put( v ); return w; }
	inline text_writer &operator<<( text_writer &w, const std::string &v )		{ w.put( std::string_view( v ) ); return w; }

	template < class T >
		requires std::is_arithmetic_v< T > && ( !std::is_same_v< T, char > )
	text_writer &operator<<( text_writer &w, T v )								{ w.put( v ); return w; }
}

}
