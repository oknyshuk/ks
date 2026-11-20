// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

import ks.core.basetypes;
import ks.core.vprof;
import ks.platform;
import ks.simd;
import ks.tier1.characterset;
import ks.tier1.commandbuffer;
import ks.tier1.keyvalues;
import ks.tier1.string_escapes;
import ks.tier1.text_reader;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;


import std;

namespace ks
{
	namespace
	{
		constexpr bool is_space( char c )
		{
			return c == ' ' || ( c >= '\t' && c <= '\r' );
		}

		constexpr char to_lower( char c )
		{
			return c >= 'A' && c <= 'Z' ? char( c + 32 ) : c;
		}

		std::size_t find_nocase( std::string_view hay, std::string_view needle )
		{
			if ( needle.empty() )
				return 0;
			if ( hay.size() < needle.size() )
				return std::string_view::npos;
			for ( std::size_t i = 0; i + needle.size() <= hay.size(); ++i )
			{
				std::size_t j = 0;
				while ( j < needle.size() && to_lower( hay[i + j] ) == to_lower( needle[j] ) )
					++j;
				if ( j == needle.size() )
					return i;
			}
			return std::string_view::npos;
		}

		template < class T >
		T parse_number( const char *s, char **end, int radix )
		{
			if constexpr ( std::is_floating_point_v< T > )
				return static_cast< T >( std::strtod( s, end ) );
			else if constexpr ( std::is_signed_v< T > )
				return static_cast< T >( std::strtoll( s, end, radix ) );
			else
				return static_cast< T >( std::strtoull( s, end, radix ) );
		}
	}

	char text_reader::get_char() noexcept
	{
		if ( at_end() )
		{
			m_failed = true;
			return 0;
		}
		return m_text[m_pos++];
	}

	template < class T >
		requires std::is_arithmetic_v< T >
	bool text_reader::read( T &out, int radix ) noexcept
	{
		out = T{};
		if ( m_failed || at_end() )
			return false;

		char tmp[129];
		const std::size_t n = std::min< std::size_t >( remaining(), sizeof( tmp ) - 1 );
		std::memcpy( tmp, m_text.data() + m_pos, n );
		tmp[n] = 0;

		char *end = tmp;
		out = parse_number< T >( tmp, &end, radix );
		const std::size_t used = end - tmp;
		if ( !used )
			return false;
		m_pos += used;
		return true;
	}

	template bool text_reader::read< signed char >( signed char &, int ) noexcept;
	template bool text_reader::read< unsigned char >( unsigned char &, int ) noexcept;
	template bool text_reader::read< short >( short &, int ) noexcept;
	template bool text_reader::read< unsigned short >( unsigned short &, int ) noexcept;
	template bool text_reader::read< int >( int &, int ) noexcept;
	template bool text_reader::read< unsigned int >( unsigned int &, int ) noexcept;
	template bool text_reader::read< long >( long &, int ) noexcept;
	template bool text_reader::read< unsigned long >( unsigned long &, int ) noexcept;
	template bool text_reader::read< long long >( long long &, int ) noexcept;
	template bool text_reader::read< unsigned long long >( unsigned long long &, int ) noexcept;
	template bool text_reader::read< float >( float &, int ) noexcept;
	template bool text_reader::read< double >( double &, int ) noexcept;

	std::size_t text_reader::peek_whitespace( std::size_t offset ) const noexcept
	{
		while ( m_pos + offset < m_text.size() && is_space( m_text[m_pos + offset] ) )
			++offset;
		return offset;
	}

	void text_reader::skip_whitespace() noexcept
	{
		if ( m_failed )
			return;
		m_pos += peek_whitespace( 0 );
	}

	bool text_reader::skip_cpp_comment() noexcept
	{
		if ( m_failed || !starts_with( "//" ) )
			return false;
		m_pos += 2;
		while ( ok() && get_char() != '\n' )
		{
		}
		return true;
	}

	std::string_view text_reader::read_word() noexcept
	{
		if ( m_failed )
			return {};
		const std::size_t start = m_pos + peek_whitespace( 0 );
		std::size_t end = start;
		while ( end < m_text.size() && !is_space( m_text[end] ) && m_text[end] != '\0' )
			++end;
		if ( end == start )
		{
			m_pos = start;
			m_failed = true;
			return {};
		}
		m_pos = end;
		return m_text.substr( start, end - start );
	}

	std::string_view text_reader::read_line() noexcept
	{
		if ( m_failed )
			return {};
		if ( at_end() )
		{
			m_failed = true;
			return {};
		}
		std::size_t end = m_pos;
		while ( end < m_text.size() && m_text[end] != '\n' && m_text[end] != '\r' && m_text[end] != '\0' )
			++end;
		if ( end < m_text.size() && m_text[end] != '\0' )
			++end;
		const std::string_view line = m_text.substr( m_pos, end - m_pos );
		m_pos = end;
		return line;
	}

	bool text_reader::skip_past( std::string_view token ) noexcept
	{
		const std::size_t at = find_nocase( rest(), token );
		if ( at == std::string_view::npos )
			return false;
		m_pos += at + token.size();
		return true;
	}

	std::optional< std::string_view > text_reader::read_between( std::string_view start, std::string_view end ) noexcept
	{
		const std::size_t startPos = m_pos;
		const bool wasFailed = m_failed;

		auto bail = [&]() -> std::optional< std::string_view >
		{
			m_pos = startPos;
			m_failed = wasFailed;
			return std::nullopt;
		};

		skip_whitespace();
		for ( const char c : start )
		{
			if ( is_space( c ) )
			{
				skip_whitespace();
			}
			else if ( to_lower( get_char() ) != to_lower( c ) || !ok() )
			{
				return bail();
			}
		}

		skip_whitespace();
		const std::size_t tokenStart = m_pos;
		if ( !skip_past( end ) )
			return bail();

		std::string_view token = m_text.substr( tokenStart, m_pos - end.size() - tokenStart );
		while ( !token.empty() && is_space( token.back() ) )
			token.remove_suffix( 1 );
		return token;
	}

	std::optional< std::string_view > text_reader::read_token( const characterset_t &breaks, bool parse_comments ) noexcept
	{
		while ( true )
		{
			if ( m_failed )
				return std::nullopt;
			skip_whitespace();
			if ( !parse_comments || !skip_cpp_comment() )
				break;
		}

		const char first = get_char();
		if ( first == 0 )
			return std::nullopt;

		if ( first == '"' )
		{
			const std::size_t start = m_pos;
			while ( ok() )
			{
				const char c = get_char();
				if ( c == '"' || !c )
					return m_text.substr( start, m_pos - 1 - start );
			}
			return m_text.substr( start, m_pos - start );
		}

		if ( IN_CHARACTERSET( breaks, static_cast< unsigned char >( first ) ) )
			return m_text.substr( m_pos - 1, 1 );

		const std::size_t start = m_pos - 1;
		while ( true )
		{
			const char c = get_char();
			if ( !ok() )
				break;
			if ( IN_CHARACTERSET( breaks, static_cast< unsigned char >( c ) ) || c == '"' || static_cast< unsigned char >( c ) <= ' ' )
			{
				--m_pos;
				break;
			}
		}
		return m_text.substr( start, m_pos - start );
	}

	std::string text_reader::read_quoted( const string_escapes &style ) noexcept
	{
		std::string out;
		if ( m_failed )
			return out;

		skip_whitespace();
		if ( peek_char() != style.delimiter() || at_end() )
			return out;
		++m_pos;

		while ( ok() )
		{
			if ( peek_char() == style.delimiter() && !at_end() )
			{
				++m_pos;
				break;
			}
			char c = get_char();
			if ( !ok() )
				break;
			if ( c == style.escape_char() )
			{
				if ( at_end() )
					break;
				const char actual = style.actual_of( peek_char() );
				if ( actual )
					++m_pos;
				c = actual;
			}
			out.push_back( c );
		}
		return out;
	}

	std::size_t text_reader::peek_quoted_length( const string_escapes &style, bool actual_size ) const noexcept
	{
		std::size_t offset = peek_whitespace( 0 );
		if ( m_pos + offset >= m_text.size() || m_text[m_pos + offset] != style.delimiter() )
			return 0;

		const std::size_t actualStart = offset;
		++offset;
		std::size_t len = 1;
		while ( m_pos + offset < m_text.size() )
		{
			const char c = m_text[m_pos + offset];
			if ( c == style.delimiter() )
				break;
			++len;
			++offset;
			if ( c == style.escape_char() && m_pos + offset < m_text.size() && style.actual_of( m_text[m_pos + offset] ) )
				++offset;
		}
		return actual_size ? len : offset - actualStart + 2;
	}

	int text_reader::scanf( const char *fmt, ... ) noexcept
	{
		std::va_list list;
		__builtin_va_start( list, fmt );

		int numScanned = 0;
		if ( m_failed )
		{
			__builtin_va_end( list );
			return 0;
		}

		for ( char c; ( c = *fmt++ ); )
		{
			if ( at_end() )
			{
				m_failed = true;
				break;
			}

			if ( c == ' ' )
			{
				skip_whitespace();
				continue;
			}

			if ( c != '%' )
			{
				if ( peek_char() != c )
					break;
				++m_pos;
				continue;
			}

			const char type = *fmt++;
			if ( !type )
				break;

			bool good = true;
			switch ( type )
			{
			case 'c':
				{
					char *ch = __builtin_va_arg( list, char * );
					*ch = peek_char();
					good = !at_end();
					if ( good )
						++m_pos;
				}
				break;

			case 'h':
				if ( *fmt == 'd' || *fmt == 'i' )
					good = read( *__builtin_va_arg( list, std::int16_t * ) );
				else if ( *fmt == 'u' )
					good = read( *__builtin_va_arg( list, std::uint16_t * ) );
				else
					good = false;
				if ( good )
					++fmt;
				break;

			case 'I':
				if ( *fmt++ != '6' || *fmt++ != '4' )
					good = false;
				else if ( *fmt == 'd' )
					good = read( *__builtin_va_arg( list, std::int64_t * ) );
				else if ( *fmt == 'u' )
					good = read( *__builtin_va_arg( list, std::uint64_t * ) );
				else
					good = false;
				if ( good )
					++fmt;
				break;

			case 'i':
			case 'd':
				good = read( *__builtin_va_arg( list, std::int32_t * ) );
				break;

			case 'x':
				good = read( *__builtin_va_arg( list, std::uint32_t * ), 16 );
				break;

			case 'u':
				good = read( *__builtin_va_arg( list, std::uint32_t * ) );
				break;

			case 'l':
				if ( *fmt == 'f' )
					good = read( *__builtin_va_arg( list, double * ) );
				else if ( *fmt == 'l' && *++fmt == 'd' )
					good = read( *__builtin_va_arg( list, std::int64_t * ) );
				else
					good = false;
				break;

			case 'f':
				good = read( *__builtin_va_arg( list, float * ) );
				break;

			case 's':
				{
					char *s = __builtin_va_arg( list, char * );
					const std::string_view word = read_word();
					const std::size_t n = std::min< std::size_t >( word.size(), 63 );
					std::memcpy( s, word.data(), n );
					s[n] = 0;
				}
				break;

			default:
				good = false;
				break;
			}

			if ( !good )
				break;
			++numScanned;
		}

		__builtin_va_end( list );
		return numScanned;
	}
}
