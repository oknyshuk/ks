// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2010 Valve Corporation
// Purpose: - defines the type fltx4 - Avoid cyclic includion.
// Copyright (c) 2026 oknyshuk

export module ks.math:fltx4;

import std;
import ks.core;
import ks.simd;

export extern "C++" {

// The standard replacement for the __m128 below is C++26 std::simd, and it is a drop-in as far as
// the ABI goes: std::simd::vec<float, 4> from <simd> is 16 bytes at 16-byte alignment, and
// bit_cast converts between them for free. (The spelling is the namespace form -- std::simd::vec,
// not std::simd<float, 4>, which is the older Parallelism TS name and does not compile here.)
//
// What holds the migration back is [simd.math]: gcc 16 implements [simd] except the math functions,
// so std::sqrt and std::abs have no simd overloads and __cpp_lib_simd is deliberately undefined --
// the library is not claiming the feature yet. ssemath.h is built on sqrt/rsqrt/abs, so converting
// it now would leave those on intrinsics behind bit_cast, i.e. two representations in the hottest
// code in the engine, and we would pay for it twice. Wait for [simd.math], then: fltx4/i32x4/u32x4
// become std::simd::vec of float/int/unsigned, the Cmp*SIMD results become std::simd::mask<float, 4>
// (which drops the (fltx4) casts that exist only because a mask and a vector are the same type
// today), MaskedAssign becomes std::simd::select, Load/StoreAlignedSIMD become
// unchecked_load/unchecked_store, and the 17 files outside thirdparty that name _mm_* or __m128
// have to follow.

// I thought about defining a class/union for the SIMD packed floats instead of using fltx4,
// but decided against it because (a) the nature of SIMD code which includes comparisons is to blur
// the relationship between packed floats and packed integer types and (b) not sure that the
// compiler would handle generating good code for the intrinsics.

typedef __m128 fltx4;
typedef __m128 i32x4;
typedef __m128 u32x4;
typedef __m128i shortx8;
typedef fltx4 bi32x4;

}
