// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.tier1.filename_table;

import std;
import ks.core.dbg;
import ks.core.platform;
import ks.core.vprof;
import ks.platform;
import ks.simd;
export import ks.tier1.strtools;
export import ks.tier1.map_ops;

export extern "C++" {

// Packs a path index (high 14 bits) and a file index (low 18 bits); null is "no file".
typedef void* FileNameHandle_t;

namespace ks
{
	class filename_table
	{
	public:
		FileNameHandle_t find_or_add( const char *pFileName )
		{
			if ( !pFileName )
				return nullptr;

			char fn[ MAX_PATH ];
			auto [ path, file ] = split( pFileName, fn );
			{
				std::shared_lock lock( m_Mutex );
				if ( unsigned p = m_Paths.find( path ), f = m_Files.find( file ); p && f )
					return pack( p, f );
			}
			std::unique_lock lock( m_Mutex );
			return pack( m_Paths.add( path ), m_Files.add( file ) );
		}

		FileNameHandle_t find( const char *pFileName ) const
		{
			if ( !pFileName )
				return nullptr;

			char fn[ MAX_PATH ];
			auto [ path, file ] = split( pFileName, fn );
			std::shared_lock lock( m_Mutex );
			unsigned p = m_Paths.find( path ), f = m_Files.find( file );
			return p && f ? pack( p, f ) : nullptr;
		}

		// Writes "path/file" into buf; false (and an empty buf) for an unknown handle.
		bool string( FileNameHandle_t handle, char *buf, int buflen ) const
		{
			buf[ 0 ] = 0;
			unsigned value = (unsigned)(std::uintptr_t)handle;
			std::shared_lock lock( m_Mutex );
			const std::string *path = m_Paths.at( value >> cFileBits ), *file = m_Files.at( value & cFileMask );
			if ( !path || !file )
				return false;
			V_strncpy( buf, path->c_str(), buflen );
			V_strncat( buf, file->c_str(), buflen, COPY_ALL_CHARACTERS );
			return true;
		}

		static int path_index( FileNameHandle_t handle )	{ return (int)( (unsigned)(std::uintptr_t)handle >> cFileBits ); }

		void clear()
		{
			std::unique_lock lock( m_Mutex );
			m_Paths.clear();
			m_Files.clear();
		}

	private:
		static constexpr unsigned cFileBits = 18;
		static constexpr unsigned cFileMask = ( 1u << cFileBits ) - 1;
		static constexpr unsigned cMaxPaths = 1u << ( 32 - cFileBits );

		static FileNameHandle_t pack( unsigned path, unsigned file )
		{
			if ( path >= cMaxPaths || file > cFileMask )
				Error( "filename_table: handle space exhausted (%u directories, %u files)\n", path, file );
			return (FileNameHandle_t)(std::uintptr_t)( ( path << cFileBits ) | file );
		}

		// Normalises into fn and splits after the last separator.
		static std::pair< std::string_view, std::string_view > split( const char *pFileName, char ( &fn )[ MAX_PATH ] )
		{
			V_strncpy( fn, pFileName, sizeof( fn ) );
			V_RemoveDotSlashes( fn );
			std::string_view s( fn );
			std::size_t n = s.find_last_of( "\\/" ) + 1;
			return { s.substr( 0, n ), s.substr( n ) };
		}

		// 1-based dense indices; 0 means absent.
		struct pool
		{
			std::deque< std::string > strings;
			std::unordered_map< std::string_view, unsigned, ihash, iequal > index;

			unsigned find( std::string_view s ) const
			{
				auto it = index.find( s );
				return it == index.end() ? 0 : it->second;
			}
			unsigned add( std::string_view s )
			{
				auto it = index.find( s );
				if ( it != index.end() )
					return it->second;
				const unsigned id = (unsigned)strings.size() + 1;
				index.emplace( strings.emplace_back( s ), id );
				return id;
			}
			const std::string *at( unsigned i ) const		{ return i && i <= strings.size() ? &strings[ i - 1 ] : nullptr; }
			void clear()									{ index.clear(); strings.clear(); }
		};

		mutable std::shared_mutex m_Mutex;
		pool m_Paths;
		pool m_Files;
	};
}

}
