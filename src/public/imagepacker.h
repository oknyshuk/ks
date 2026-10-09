#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;

inline constexpr auto MAX_MAX_LIGHTMAP_WIDTH = 2048;


//-----------------------------------------------------------------------------
// This packs a single lightmap
//-----------------------------------------------------------------------------
class CImagePacker
{
public:
	bool Reset( int nSortId, int maxLightmapWidth, int maxLightmapHeight );
	bool AddBlock( int width, int height, 
		int *returnX, int *returnY );
	void GetMinimumDimensions( int *returnWidth, int *returnHeight, int nMaxAspectRatio = 0 );
	float GetEfficiency( void );
	int GetSortId() const;
	void IncrementSortId();

protected:
	int GetMaxYIndex( int firstX, int width );

	int m_MaxLightmapWidth;
	int m_MaxLightmapHeight;
	int m_pLightmapWavefront[MAX_MAX_LIGHTMAP_WIDTH];
	int m_AreaUsed;
	int m_MinimumHeight;

	// For optimization purposes:
	// These store the width + height of the first image
	// that was unable to be stored in this image
	int m_MaxBlockWidth;
	int m_MaxBlockHeight;
	int m_nSortID;
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline int CImagePacker::GetSortId() const
{
	return m_nSortID;
}

inline void CImagePacker::IncrementSortId()
{
	++m_nSortID;
}
