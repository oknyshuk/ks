#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// fixed point stuff for real-time resampling
inline constexpr auto FIX_BITS = 28;
inline constexpr auto FIX_SCALE = (1 << FIX_BITS);
inline constexpr auto FIX_MASK = ((1 << FIX_BITS)-1);
template <typename T1> [[gnu::always_inline]] inline auto FIX_FLOAT( T1 a ) { return ((int)((a) * FIX_SCALE)); }
template <typename T1> [[gnu::always_inline]] inline auto FIX( T1 a ) { return (((int)(a)) << FIX_BITS); }
template <typename T1> [[gnu::always_inline]] inline auto FIX_INTPART( T1 a ) { return (((int)(a)) >> FIX_BITS); }
#define FIX_FRACTION(a,b)	(FIX(a)/(b))
template <typename T1> [[gnu::always_inline]] inline auto FIX_FRACPART( T1 a ) { return ((a) & FIX_MASK); }
template <typename T1> [[gnu::always_inline]] inline auto FIX_TODOUBLE( T1 a ) { return ((double)(a) / (double)FIX_SCALE); }

typedef unsigned int fixedint;

inline constexpr auto FIX_BITS14 = 14;
inline constexpr auto FIX_SCALE14 = (1 << FIX_BITS14);
inline constexpr auto FIX_MASK14 = ((1 << FIX_BITS14)-1);
template <typename T1> [[gnu::always_inline]] inline auto FIX_FLOAT14( T1 a ) { return ((int)((a) * FIX_SCALE14)); }
template <typename T1> [[gnu::always_inline]] inline auto FIX14( T1 a ) { return (((int)(a)) << FIX_BITS14); }
template <typename T1> [[gnu::always_inline]] inline auto FIX_INTPART14( T1 a ) { return (((int)(a)) >> FIX_BITS14); }
#define FIX_FRACTION14(a,b)	(FIX14(a)/(b))
template <typename T1> [[gnu::always_inline]] inline auto FIX_FRACPART14( T1 a ) { return ((a) & FIX_MASK14); }
template <typename T1> [[gnu::always_inline]] inline auto FIX_14TODOUBLE( T1 a ) { return ((double)(a) / (double)FIX_SCALE14); }

template <typename T1> [[gnu::always_inline]] inline auto FIX_28TO14( T1 a ) { return ( (int)( ((unsigned int)(a)) >> (FIX_BITS - 14) ) ); }
