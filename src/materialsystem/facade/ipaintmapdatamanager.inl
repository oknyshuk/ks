import ks.core.basetypes;
import ks.core.wchartypes;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IPaintmapDataManager class to CPaintmapDataManager, its one implementation.
// Included by engine/paint.cpp after the definition of CPaintmapDataManager.

void IPaintmapDataManager::BeginPaintmapsDataAllocation( int iPaintmapCount )
{ static_cast<CPaintmapDataManager *>( this )->BeginPaintmapsDataAllocation( iPaintmapCount ); }
void IPaintmapDataManager::AllocatePaintmapData( int iPaintmapID, int iCorrespondingLightMapWidth, int iCorrespondingLightMapHeight )
{ static_cast<CPaintmapDataManager *>( this )->AllocatePaintmapData( iPaintmapID, iCorrespondingLightMapWidth, iCorrespondingLightMapHeight ); }
void IPaintmapDataManager::DestroyPaintmapsData()
{ static_cast<CPaintmapDataManager *>( this )->DestroyPaintmapsData(); }
BYTE* IPaintmapDataManager::GetPaintmapData( int paintmap )
{ return static_cast<CPaintmapDataManager *>( this )->GetPaintmapData( paintmap ); }
void IPaintmapDataManager::GetPaintmapSize( int paintmap, int& width, int& height )
{ static_cast<CPaintmapDataManager *>( this )->GetPaintmapSize( paintmap, width, height ); }
void IPaintmapDataManager::OnRestorePaintmaps()
{ static_cast<CPaintmapDataManager *>( this )->OnRestorePaintmaps(); }
