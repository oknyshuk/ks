#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "disp_defs.h"

// Figure out the max number of vertices and indices for a displacement
// of the specified power.
void CalcMaxNumVertsAndIndices( int power, int *nVerts, int *nIndices );
