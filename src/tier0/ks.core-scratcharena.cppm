// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.core:scratcharena;

import std;
import ks.platform;
import :prelude;
import :color;
import :logging;
import :dbg;
import :memalloc;
import :threadtools;
import :tslist;
import :cache_hints;

export extern "C++" {

namespace ks
{

template <std::size_t InlineBytes = 4096>
class ScratchArena
{
public:
	static constexpr std::size_t kMinAlignment = 16;

	ScratchArena() noexcept = default;
	~ScratchArena() { Release(); }

	ScratchArena( const ScratchArena & ) = delete;
	ScratchArena &operator=( const ScratchArena & ) = delete;

	[[nodiscard]] void *AllocBytes( std::size_t nBytes, std::size_t nAlignment = kMinAlignment )
	{
		if ( nAlignment < kMinAlignment )
			nAlignment = kMinAlignment;
		if ( nBytes == 0 )
			nBytes = 1;

		std::size_t offset = ( m_Used + nAlignment - 1 ) & ~( nAlignment - 1 );
		if ( offset + nBytes <= sizeof( m_Inline ) )
		{
			m_Used = offset + nBytes;
			return m_Inline + offset;
		}

		const bool overAligned = nAlignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__;
		std::size_t total = sizeof( Block ) + nAlignment - 1 + nBytes;
		Block *block = overAligned
			? static_cast<Block *>( ::operator new( total, std::align_val_t( nAlignment ) ) )
			: static_cast<Block *>( ::operator new( total ) );
		block->next = m_Blocks;
		block->alignment = overAligned ? nAlignment : 0;
		m_Blocks = block;

		std::size_t raw = reinterpret_cast<std::size_t>( block + 1 );
		return reinterpret_cast<void *>( ( raw + nAlignment - 1 ) & ~( nAlignment - 1 ) );
	}

	template <typename T, typename Count>
	[[nodiscard]] T *Alloc( Count nCount )
	{
		static_assert( __has_trivial_destructor( T ), "ScratchArena never runs destructors" );
		return static_cast<T *>( AllocBytes( static_cast<std::size_t>( nCount ) * sizeof( T ), alignof( T ) ) );
	}

private:
	struct Block
	{
		Block *next;
		std::size_t alignment;
	};

	void Release()
	{
		while ( m_Blocks )
		{
			Block *block = m_Blocks;
			m_Blocks = block->next;
			if ( block->alignment )
				::operator delete( block, std::align_val_t( block->alignment ) );
			else
				::operator delete( block );
		}
	}

	alignas( kMinAlignment ) unsigned char m_Inline[InlineBytes];
	std::size_t m_Used = 0;
	Block *m_Blocks = nullptr;
};

} // namespace ks

}
