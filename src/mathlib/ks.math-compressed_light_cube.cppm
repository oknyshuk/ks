// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.math.compressed_light_cube;

import std;
import ks.platform;
import ks.simd;
export import ks.math.mathlib;

export extern "C++" {

struct CompressedLightCube
{
	ColorRGBExp32 m_Color[6];
};

}
