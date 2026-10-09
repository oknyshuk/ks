#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation

// Purpose: a class for performing cube-mapped spherical sample lookups.

import std;
import ks.math;

template<class T, int RES> struct CCubeMap
{
	T m_Samples[6][RES][RES];
	
public:
	[[gnu::always_inline]] inline void GetCoords( Vector const &vecNormalizedDirection, int &nX, int &nY, int &nFace )
	{
		// find largest magnitude component
		int nLargest = 0;
		int nAxis0 = 1;
		int nAxis1 = 2;
		if ( std::fabs( vecNormalizedDirection[1] ) > std::fabs( vecNormalizedDirection[0] ) )
		{
			nLargest = 1;
			nAxis0 = 0;
			nAxis1 = 2;
		}
		if ( std::fabs( vecNormalizedDirection[2] ) > std::fabs( vecNormalizedDirection[nLargest] ) )
		{
			nLargest = 2;
			nAxis0 = 0;
			nAxis1 = 1;
		}
		float flZ = vecNormalizedDirection[nLargest];
		if ( flZ < 0 )
		{
			flZ = - flZ;
			nLargest += 3;
		}
		nFace = nLargest;
		flZ = 1.0 / flZ;
		nX = RemapValClamped( vecNormalizedDirection[nAxis0] * flZ, -1, 1, 0, RES - 1 );
		nY = RemapValClamped( vecNormalizedDirection[nAxis1] * flZ, -1, 1, 0, RES - 1 );
	}

	[[gnu::always_inline]] inline T & GetSample( Vector const &vecNormalizedDirection )
	{
		int nX, nY, nFace;
		GetCoords( vecNormalizedDirection, nX, nY, nFace );
		return m_Samples[nFace][nX][nY];
	}
};
