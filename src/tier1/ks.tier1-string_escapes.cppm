// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.tier1.string_escapes;

import std;
import ks.core.vprof;
import ks.platform;
import ks.simd;

export extern "C++" {

namespace ks
{
	class string_escapes
	{
	public:
		struct entry
		{
			char actual;
			char spelling;
		};

		constexpr string_escapes( char delimiter, char escape, std::span< const entry > entries )
			: m_delimiter( delimiter ), m_escape( escape )
		{
			for ( const entry &e : entries )
			{
				m_spelling_of[ static_cast< unsigned char >( e.actual ) ] = e.spelling;
				m_actual_of[ static_cast< unsigned char >( e.spelling ) ] = e.actual;
			}
		}

		constexpr char delimiter() const					{ return m_delimiter; }
		constexpr char escape_char() const					{ return m_escape; }

		constexpr char spelling_of( char c ) const			{ return m_spelling_of[ static_cast< unsigned char >( c ) ]; }

		constexpr char actual_of( char s ) const			{ return m_actual_of[ static_cast< unsigned char >( s ) ]; }

	private:
		char m_delimiter;
		char m_escape;
		std::array< char, 256 > m_spelling_of{};
		std::array< char, 256 > m_actual_of{};
	};

	inline constexpr std::array< string_escapes::entry, 11 > c_escape_entries{ {
		{ '\n', 'n' }, { '\t', 't' }, { '\v', 'v' }, { '\b', 'b' }, { '\r', 'r' }, { '\f', 'f' },
		{ '\a', 'a' }, { '\\', '\\' }, { '\?', '\?' }, { '\'', '\'' }, { '\"', '\"' },
	} };
	inline constexpr string_escapes c_string_escapes( '"', '\\', c_escape_entries );

	inline constexpr string_escapes no_escapes( '"', '\x7F', {} );
}

}
