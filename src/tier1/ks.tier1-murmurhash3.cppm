// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1:murmurhash3;

import std;
import ks.math;

export extern "C++" {

uint32 MurmurHash3_32( const void *key, std::size_t len, uint32 seed, bool bCaselessStringVariant = false );

inline uint32 MurmurHash3String( const char *pszKey, std::size_t len )
{
	return MurmurHash3_32( pszKey, len, 1047 /*anything will do for a seed*/, false );
}

inline uint32 MurmurHash3StringCaseless( const char *pszKey, std::size_t len )
{
	return MurmurHash3_32( pszKey, len, 1047 /*anything will do for a seed*/, true );
}

inline uint32 MurmurHash3String( const char *pszKey )
{
	return MurmurHash3String( pszKey, std::strlen( pszKey ) );
}

inline uint32 MurmurHash3StringCaseless( const char *pszKey )
{
	return MurmurHash3StringCaseless( pszKey, std::strlen( pszKey ) );
}

template <typename T>
inline uint32 MurmurHash3Item( const T &item )
{
	return MurmurHash3_32( &item, sizeof(item), 1047 );
}

inline uint32 MurmurHash3Int( uint32 h )
{
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

template <>
inline uint32 MurmurHash3Item( const uint32 &item )
{
	return MurmurHash3Int( item );
}

template <>
inline uint32 MurmurHash3Item( const int32 &item )
{
	return MurmurHash3Int( item );
}

template<typename T>
struct MurmurHash3Functor
{
	typedef uint32 TargetType;
	TargetType operator()(const T &key) const
	{
		return MurmurHash3Item( key );
	}
};

template<>
struct MurmurHash3Functor<char *>
{
	typedef uint32 TargetType;
	TargetType operator()(const char *key) const
	{
		return MurmurHash3String( key );
	}
};

template<>
struct MurmurHash3Functor<const char *>
{
	typedef uint32 TargetType;
	TargetType operator()(const char *key) const
	{
		return MurmurHash3String( key );
	}
};

}
