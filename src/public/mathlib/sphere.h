#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

import ks.math;
#include "rubikon/param_types.h"
import std;

void CastSphereRay( CShapeCastResult& out, const Vector &m, const Vector& p, const Vector& d, float flRadius );
