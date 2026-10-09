#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.math;

//#include "mathlib/vector.h"
inline constexpr auto MAX_SURFINFO_VERTS = 16;
class SurfInfo
{
public:
	// Shape of the surface.
	Vector			m_Verts[ MAX_SURFINFO_VERTS ];
	unsigned long	m_nVerts;

	// Plane of the surface.
	VPlane			m_Plane;

	// For engine use only..
	void			*m_pEngineData;
};
