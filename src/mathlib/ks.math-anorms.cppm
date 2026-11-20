// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.math.anorms;

import std;
import ks.platform;
import ks.simd;
export import ks.math.vector;

export extern "C++" {

inline constexpr auto NUMVERTEXNORMALS = 162;


extern Vector g_anorms[NUMVERTEXNORMALS];

}
