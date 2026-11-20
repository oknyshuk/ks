#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.basetypes;
import ks.core.wchartypes;
import ks.platform;
import ks.simd;


//owned by the client
class IPaintmapDataManager
{
public:
	//called just before the material system initializes the paint textures
	void BeginPaintmapsDataAllocation( int iPaintmapCount );

	void AllocatePaintmapData( int iPaintmapID, int iCorrespondingLightMapWidth, int iCorrespondingLightMapHeight );

	void DestroyPaintmapsData( void ); // clean up old Paintmaps data

	BYTE* GetPaintmapData( int paintmap );

	void GetPaintmapSize( int paintmap, int& width, int& height );

	void OnRestorePaintmaps();
};


