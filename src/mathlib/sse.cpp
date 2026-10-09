// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: SSE Math primitives.

import std;
import ks.math;
#include "sse.h"

static const uint32 _sincos_masks[]	  = { (uint32)0x0,  (uint32)~0x0 };
static const uint32 _sincos_inv_masks[] = { (uint32)~0x0, (uint32)0x0 };

//-----------------------------------------------------------------------------
// Constants required by some of the SSE assembly:
//-----------------------------------------------------------------------------

const float _ps_am_0[4] [[gnu::aligned(16)]] = { 0.0f, 0.0f, 0.0f, 0.0f };
const float _ps_am_1[4] [[gnu::aligned(16)]] = { 1.0f, 1.0f, 1.0f, 1.0f };
const float _ps_am_m1[4] [[gnu::aligned(16)]] = { -1.0f, -1.0f, -1.0f, -1.0f };
const float _ps_am_0p5[4] [[gnu::aligned(16)]] = { 0.5f, 0.5f, 0.5f, 0.5f };
const float _ps_am_1p5[4] [[gnu::aligned(16)]] = { 1.5f, 1.5f, 1.5f, 1.5f };
const float _ps_am_pi[4] [[gnu::aligned(16)]] = { (float)std::numbers::pi, (float)std::numbers::pi, (float)std::numbers::pi, (float)std::numbers::pi };
const float _ps_am_pi_o_2[4] [[gnu::aligned(16)]] = { (float)(std::numbers::pi / 2.0), (float)(std::numbers::pi / 2.0), (float)(std::numbers::pi / 2.0), (float)(std::numbers::pi / 2.0) };
const float _ps_am_2_o_pi[4] [[gnu::aligned(16)]] = { (float)(2.0 / std::numbers::pi), (float)(2.0 / std::numbers::pi), (float)(2.0 / std::numbers::pi), (float)(2.0 / std::numbers::pi) };
const float _ps_am_pi_o_4[4] [[gnu::aligned(16)]] = { (float)(std::numbers::pi / 4.0), (float)(std::numbers::pi / 4.0), (float)(std::numbers::pi / 4.0), (float)(std::numbers::pi / 4.0) };
const float _ps_am_4_o_pi[4] [[gnu::aligned(16)]] = { (float)(4.0 / std::numbers::pi), (float)(4.0 / std::numbers::pi), (float)(4.0 / std::numbers::pi), (float)(4.0 / std::numbers::pi) };
const int32 _ps_am_sign_mask[4] [[gnu::aligned(16)]] = { (int)0x80000000, (int)0x80000000, (int)0x80000000, (int)0x80000000 };
const int32 _ps_am_inv_sign_mask[4] [[gnu::aligned(16)]] = { ~0x80000000, ~0x80000000, ~0x80000000, ~0x80000000 };
const int32 _ps_am_min_norm_pos[4] [[gnu::aligned(16)]] = { 0x00800000, 0x00800000, 0x00800000, 0x00800000 };
const int32 _ps_am_mant_mask[4] [[gnu::aligned(16)]] = { (int)0x7f800000, (int)0x7f800000, (int)0x7f800000, (int)0x7f800000 };
const int32 _ps_am_inv_mant_mask[4] [[gnu::aligned(16)]] = { ~0x7f800000, ~0x7f800000, ~0x7f800000, ~0x7f800000 };

static const int32 _epi32_1[4] [[gnu::aligned(16)]] = { 1, 1, 1, 1 };
static const int32 _epi32_2[4] [[gnu::aligned(16)]] = { 2, 2, 2, 2 };

static const float _ps_sincos_p0[4] [[gnu::aligned(16)]] = { 0.15707963267948963959e1f, 0.15707963267948963959e1f, 0.15707963267948963959e1f, 0.15707963267948963959e1f };
static const float _ps_sincos_p1[4] [[gnu::aligned(16)]] = { -0.64596409750621907082e0f, -0.64596409750621907082e0f, -0.64596409750621907082e0f, -0.64596409750621907082e0f };
static const float _ps_sincos_p2[4] [[gnu::aligned(16)]] = { 0.7969262624561800806e-1f, 0.7969262624561800806e-1f, 0.7969262624561800806e-1f, 0.7969262624561800806e-1f };
static const float _ps_sincos_p3[4] [[gnu::aligned(16)]] = { -0.468175413106023168e-2f, -0.468175413106023168e-2f, -0.468175413106023168e-2f, -0.468175413106023168e-2f };

//-----------------------------------------------------------------------------
// SSE implementations of optimized routines:
//-----------------------------------------------------------------------------


static const int _ps_sign_mask[4] [[gnu::aligned(16)]] = { (int)0x80000000, (int)0x80000000, (int)0x80000000, (int)0x80000000 };
static const int _ps_inv_sign_mask[4] [[gnu::aligned(16)]] = { ~0x80000000, ~0x80000000, ~0x80000000, ~0x80000000 };


static const int _pi32_1[4] [[gnu::aligned(16)]] = { 1, 1, 1, 1 };
static const int _pi32_inv1[4] [[gnu::aligned(16)]] = { ~1, ~1, ~1, ~1 };
static const int _pi32_2[4] [[gnu::aligned(16)]] = { 2, 2, 2, 2 };
static const int _pi32_4[4] [[gnu::aligned(16)]] = { 4, 4, 4, 4 };
static const int _pi32_0x7f[4] [[gnu::aligned(16)]] = { 0x7f, 0x7f, 0x7f, 0x7f };
static const float _ps_1[4] [[gnu::aligned(16)]] = { 1.0f, 1.0f, 1.0f, 1.0f };
static const float _ps_0p5[4] [[gnu::aligned(16)]] = { 0.5f, 0.5f, 0.5f, 0.5f };

static const float _ps_minus_cephes_DP1[4] [[gnu::aligned(16)]] = { -0.78515625, -0.78515625, -0.78515625, -0.78515625 };
static const float _ps_minus_cephes_DP2[4] [[gnu::aligned(16)]] = { -2.4187564849853515625e-4, -2.4187564849853515625e-4, -2.4187564849853515625e-4, -2.4187564849853515625e-4 };
static const float _ps_minus_cephes_DP3[4] [[gnu::aligned(16)]] = { -3.77489497744594108e-8, -3.77489497744594108e-8, -3.77489497744594108e-8, -3.77489497744594108e-8 };
static const float _ps_sincof_p0[4] [[gnu::aligned(16)]] = { -1.9515295891E-4, -1.9515295891E-4, -1.9515295891E-4, -1.9515295891E-4 };
static const float _ps_sincof_p1[4] [[gnu::aligned(16)]] = { 8.3321608736E-3, 8.3321608736E-3, 8.3321608736E-3, 8.3321608736E-3 };
static const float _ps_sincof_p2[4] [[gnu::aligned(16)]] = { -1.6666654611E-1, -1.6666654611E-1, -1.6666654611E-1, -1.6666654611E-1 };
static const float _ps_coscof_p0[4] [[gnu::aligned(16)]] = { 2.443315711809948E-005, 2.443315711809948E-005, 2.443315711809948E-005, 2.443315711809948E-005 };
static const float _ps_coscof_p1[4] [[gnu::aligned(16)]] = { -1.388731625493765E-003, -1.388731625493765E-003, -1.388731625493765E-003, -1.388731625493765E-003 };
static const float _ps_coscof_p2[4] [[gnu::aligned(16)]] = { 4.166664568298827E-002, 4.166664568298827E-002, 4.166664568298827E-002, 4.166664568298827E-002 };
static const float _ps_cephes_FOPI[4] [[gnu::aligned(16)]] = { 1.27323954473516, 1.27323954473516, 1.27323954473516, 1.27323954473516 }; // 4 / M_PI

typedef union xmm_mm_union {
	__m128 xmm;
	__m64 mm[2];
} xmm_mm_union;

typedef __m128 v4sf;  // vector of 4 float (sse1)
typedef __m64 v2si;   // vector of 2 int (mmx)



// SSE Version of VectorTransform
void VectorTransformSSE(const float *in1, const matrix3x4_t& in2, float *out1)
{
	contract_assert( s_bMathlibInitialized );
	contract_assert( in1 != out1 );

	out1[0] = DotProduct(in1, in2[0]) + in2[0][3];
	out1[1] = DotProduct(in1, in2[1]) + in2[1][3];
	out1[2] = DotProduct(in1, in2[2]) + in2[2][3];
}

void VectorRotateSSE( const float *in1, const matrix3x4_t& in2, float *out1 )
{
	contract_assert( s_bMathlibInitialized );
	contract_assert( in1 != out1 );

	out1[0] = DotProduct( in1, in2[0] );
	out1[1] = DotProduct( in1, in2[1] );
	out1[2] = DotProduct( in1, in2[2] );
}




// SSE DotProduct -- it's a smidgen faster than the asm DotProduct...
//   Should be validated too!  :)
//   NJS: (Nov 1 2002) -NOT- faster.  may time a couple cycles faster in a single function like 
//   this, but when inlined, and instruction scheduled, the C version is faster.  
//   Verified this via VTune
/*
vec_t DotProduct (const vec_t *a, const vec_t *c)
{
	vec_t temp;

	__asm
	{
		mov eax, a;
		mov ecx, c;
		mov edx, DWORD PTR [temp]
		movss xmm0, [eax];
		mulss xmm0, [ecx];
		movss xmm1, [eax+4];
		mulss xmm1, [ecx+4];
		movss xmm2, [eax+8];
		mulss xmm2, [ecx+8];
		addss xmm0, xmm1;
		addss xmm0, xmm2;
		movss [edx], xmm0;
		fld DWORD PTR [edx];
		ret
	}
}
*/
