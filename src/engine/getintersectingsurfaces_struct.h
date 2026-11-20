#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.basetypes;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;

struct model_t;
class SurfInfo;


// Used by GetIntersectingSurfaces.
class GetIntersectingSurfaces_Struct
{
public:
	model_t			*m_pModel;
	const Vector	*m_pCenter;
	const byte		*m_pCenterPVS;	// PVS for the leaf m_pCenter is in.
	float			m_Radius;
	bool			m_bOnlyVisible;

	SurfInfo		*m_pInfos;
	int				m_nMaxInfos;
	int				m_nSetInfos;
};
