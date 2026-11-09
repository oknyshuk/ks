// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IOverlayMgr class to COverlayMgr. Included by Overlay.cpp.

bool IOverlayMgr::LoadOverlays()
{ return static_cast<COverlayMgr*>(this)->LoadOverlays(); }
void IOverlayMgr::UnloadOverlays()
{ static_cast<COverlayMgr*>(this)->UnloadOverlays(); }
void IOverlayMgr::CreateFragments( void )
{ static_cast<COverlayMgr*>(this)->CreateFragments(); }
void IOverlayMgr::ReSortMaterials( void )
{ static_cast<COverlayMgr*>(this)->ReSortMaterials(); }
void IOverlayMgr::ClearRenderLists()
{ static_cast<COverlayMgr*>(this)->ClearRenderLists(); }
void IOverlayMgr::ClearRenderLists( int nSortGroup )
{ static_cast<COverlayMgr*>(this)->ClearRenderLists( nSortGroup ); }
void IOverlayMgr::AddFragmentListToRenderList( int nSortGroup, OverlayFragmentHandle_t iFragment, bool bDisp )
{ static_cast<COverlayMgr*>(this)->AddFragmentListToRenderList( nSortGroup, iFragment, bDisp ); }
void IOverlayMgr::RenderOverlays( IMatRenderContext *pRenderContext, int nSortGroup )
{ static_cast<COverlayMgr*>(this)->RenderOverlays( pRenderContext, nSortGroup ); }
void IOverlayMgr::SetOverlayBindProxy( int iOverlayID, void *pBindProxy )
{ static_cast<COverlayMgr*>(this)->SetOverlayBindProxy( iOverlayID, pBindProxy ); }
void IOverlayMgr::UpdateOverlayRenderLevels( int nCPULevel, int nGPULevel )
{ static_cast<COverlayMgr*>(this)->UpdateOverlayRenderLevels( nCPULevel, nGPULevel ); }
void IOverlayMgr::RenderAllUnlitOverlays( IMatRenderContext *pRenderContext, int nSortGroup )
{ static_cast<COverlayMgr*>(this)->RenderAllUnlitOverlays( pRenderContext, nSortGroup ); }

