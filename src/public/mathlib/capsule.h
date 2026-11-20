#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

import std;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
#include "rubikon/param_types.h"

void CastCapsuleRay( CShapeCastResult& out, const Vector& vRayStart, const Vector& vRayDelta, const Vector vCenter[], float flRadius );
