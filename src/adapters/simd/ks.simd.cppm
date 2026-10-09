// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

module;

#include <immintrin.h>

export module ks.simd;

export
{
using ::__m64;
using ::__m128;
using ::__m128i;
using ::__m128d;

using ::_mm_add_epi32;
using ::_mm_add_ps;
using ::_mm_andnot_ps;
using ::_mm_and_ps;
using ::_mm_cmpeq_ps;
using ::_mm_cmpge_ps;
using ::_mm_cmpgt_ps;
using ::_mm_cmple_ps;
using ::_mm_cmplt_epi16;
using ::_mm_cmplt_ps;
using ::_mm_comigt_ss;
using ::_mm_cvtepi32_ps;
using ::_mm_cvtps_epi32;
using ::_mm_cvttps_pi32;
using ::_mm_div_ps;
using ::_mm_empty;
using ::_mm_getcsr;
using ::_mm_load_ps;
using ::_mm_load_si128;
using ::_mm_load_ss;
using ::_mm_max_ps;
using ::_mm_min_ps;
using ::_mm_movehl_ps;
using ::_mm_movemask_ps;
using ::_mm_pause;
using ::_mm_mul_ps;
using ::_mm_or_ps;
using ::_mm_packs_epi32;
using ::_mm_rcp_ps;
using ::_mm_rsqrt_ps;
using ::_mm_rsqrt_ss;
using ::_mm_set1_ps;
using ::_mm_setcsr;
using ::_mm_set_ps;
using ::_mm_set_ss;
using ::_mm_setzero_si128;
using ::_mm_sqrt_ps;
using ::_mm_sqrt_ss;
using ::_mm_srai_epi32;
using ::_mm_srl_epi16;
using ::_mm_store_ps;
using ::_mm_store_si128;
using ::_mm_store_ss;
using ::_mm_stream_ps;
using ::_mm_sub_ps;
using ::_mm_unpackhi_epi16;
using ::_mm_unpackhi_epi32;
using ::_mm_unpackhi_ps;
using ::_mm_unpacklo_epi16;
using ::_mm_unpacklo_epi32;
using ::_mm_xor_ps;
}

export namespace ks::simd
{

// The unaligned load/store intrinsics are not re-exported: their bodies depend on the aligned(1)
// typedefs (__m128_u), and GCC drops that attribute when it streams the module interface, so the
// imported versions compile to aligned moves.
[[gnu::always_inline]] inline __m128 load_unaligned( const void *address ) noexcept
{
	__m128 value;
	__builtin_memcpy( &value, address, sizeof value );
	return value;
}

[[gnu::always_inline]] inline void store_unaligned( void *address, __m128 value ) noexcept
{
	__builtin_memcpy( address, &value, sizeof value );
}

[[gnu::always_inline]] inline __m128i load_unaligned_i( const void *address ) noexcept
{
	__m128i value;
	__builtin_memcpy( &value, address, sizeof value );
	return value;
}

[[gnu::always_inline]] inline void store_unaligned_i( void *address, __m128i value ) noexcept
{
	__builtin_memcpy( address, &value, sizeof value );
}

template <int A, int B, int C, int D>
[[gnu::always_inline]] inline __m128 shuffle( __m128 x, __m128 y ) noexcept
{
	return _mm_shuffle_ps( x, y, ( D << 6 ) | ( C << 4 ) | ( B << 2 ) | A );
}

template <int A, int B, int C, int D>
[[gnu::always_inline]] inline __m128 shuffle( __m128 x ) noexcept
{
	return shuffle<A, B, C, D>( x, x );
}

[[gnu::always_inline]] inline void set_flush_to_zero( bool enable ) noexcept
{
	_mm_setcsr( enable ? ( _mm_getcsr() | 0x8040 ) : ( _mm_getcsr() & ~0x8040u ) );
}

[[gnu::always_inline]] inline void prefetch( const void *address ) noexcept
{
	_mm_prefetch( static_cast<const char *>( address ), _MM_HINT_T0 );
}

[[gnu::always_inline]] inline void transpose4( __m128 &row0, __m128 &row1, __m128 &row2, __m128 &row3 ) noexcept
{
	_MM_TRANSPOSE4_PS( row0, row1, row2, row3 );
}

}
