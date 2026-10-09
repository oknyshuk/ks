// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.math:bumpvects;

import std;
import ks.core;
import ks.simd;
export import :mathlib;

export extern "C++" {

inline constexpr auto OO_SQRT_2 = 0.70710676908493042f;
inline constexpr float OO_SQRT_3 = 0.57735025882720947f;
inline constexpr auto OO_SQRT_6 = 0.40824821591377258f;
// sqrt( 2 / 3 )
inline constexpr auto OO_SQRT_2_OVER_3 = 0.81649661064147949f;

inline constexpr auto NUM_BUMP_VECTS = 3;

inline const TableVector g_localBumpBasis[NUM_BUMP_VECTS] = 
{
	{	OO_SQRT_2_OVER_3, 0.0f, OO_SQRT_3 },
	{  -OO_SQRT_6, OO_SQRT_2, OO_SQRT_3 },
	{  -OO_SQRT_6, -OO_SQRT_2, OO_SQRT_3 }
};

void GetBumpNormals( const Vector& sVect, const Vector& tVect, const Vector& flatNormal, 
					 const Vector& phongNormal, Vector bumpNormals[NUM_BUMP_VECTS] );

}
