#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

import ks.core.vprof;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
#include "rubikon/param_types.h"
import std;

void CastSphereRay( CShapeCastResult& out, const Vector &m, const Vector& p, const Vector& d, float flRadius );
