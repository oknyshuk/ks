// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IRender class to CRender. Included by gl_rmain.cpp.

void IRender::FrameBegin( void )
{ static_cast<CRender*>(this)->FrameBegin(); }
void IRender::FrameEnd( void )
{ static_cast<CRender*>(this)->FrameEnd(); }
void IRender::ViewSetupVis( bool novis, int numorigins, const Vector origin[] )
{ static_cast<CRender*>(this)->ViewSetupVis( novis, numorigins, origin ); }
void IRender::ViewDrawFade( byte *color, IMaterial* pFadeMaterial, bool mapFullTextureToScreen )
{ static_cast<CRender*>(this)->ViewDrawFade( color, pFadeMaterial, mapFullTextureToScreen ); }
void IRender::DrawSceneBegin( void )
{ static_cast<CRender*>(this)->DrawSceneBegin(); }
void IRender::DrawSceneEnd( void )
{ static_cast<CRender*>(this)->DrawSceneEnd(); }
IWorldRenderList * IRender::CreateWorldList()
{ return static_cast<CRender*>(this)->CreateWorldList(); }
void IRender::BuildWorldLists_Epilogue( IWorldRenderList *pList, WorldListInfo_t* pInfo, bool bShadowDepth )
{ static_cast<CRender*>(this)->BuildWorldLists_Epilogue( pList, pInfo, bShadowDepth ); }
void IRender::BuildWorldLists( IWorldRenderList *pList, WorldListInfo_t* pInfo, int iForceViewLeaf, const VisOverrideData_t* pVisData, bool bShadowDepth, float *pReflectionWaterHeight )
{ static_cast<CRender*>(this)->BuildWorldLists( pList, pInfo, iForceViewLeaf, pVisData, bShadowDepth, pReflectionWaterHeight ); }
void IRender::DrawWorldLists( IMatRenderContext *pRenderContext, IWorldRenderList *pList, unsigned long flags, float waterZAdjust )
{ static_cast<CRender*>(this)->DrawWorldLists( pRenderContext, pList, flags, waterZAdjust ); }
const Vector& IRender::ViewOrigin( void )
{ return static_cast<CRender*>(this)->ViewOrigin(); }
const QAngle& IRender::ViewAngles( void )
{ return static_cast<CRender*>(this)->ViewAngles(); }
CViewSetup const & IRender::ViewGetCurrent( void )
{ return static_cast<CRender*>(this)->ViewGetCurrent(); }
const VMatrix& IRender::ViewMatrix( void )
{ return static_cast<CRender*>(this)->ViewMatrix(); }
const VMatrix& IRender::WorldToScreenMatrix( void )
{ return static_cast<CRender*>(this)->WorldToScreenMatrix(); }
float IRender::GetFramerate( void )
{ return static_cast<CRender*>(this)->GetFramerate(); }
float IRender::GetZNear( void )
{ return static_cast<CRender*>(this)->GetZNear(); }
float IRender::GetZFar( void )
{ return static_cast<CRender*>(this)->GetZFar(); }
float IRender::GetFov( void )
{ return static_cast<CRender*>(this)->GetFov(); }
float IRender::GetFovY( void )
{ return static_cast<CRender*>(this)->GetFovY(); }
float IRender::GetFovViewmodel( void )
{ return static_cast<CRender*>(this)->GetFovViewmodel(); }
bool IRender::ClipTransform( Vector const& point, Vector* pClip )
{ return static_cast<CRender*>(this)->ClipTransform( point, pClip ); }
bool IRender::ScreenTransform( Vector const& point, Vector* pScreen )
{ return static_cast<CRender*>(this)->ScreenTransform( point, pScreen ); }
void IRender::Push3DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes )
{ static_cast<CRender*>(this)->Push3DView( pRenderContext, view, nFlags, pRenderTarget, frustumPlanes ); }
void IRender::Push3DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes, ITexture* pDepthTexture )
{ static_cast<CRender*>(this)->Push3DView( pRenderContext, view, nFlags, pRenderTarget, frustumPlanes, pDepthTexture ); }
void IRender::Push2DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes )
{ static_cast<CRender*>(this)->Push2DView( pRenderContext, view, nFlags, pRenderTarget, frustumPlanes ); }
void IRender::PopView( IMatRenderContext *pRenderContext, Frustum frustumPlanes )
{ static_cast<CRender*>(this)->PopView( pRenderContext, frustumPlanes ); }
void IRender::SetMainView( const Vector &vecOrigin, const QAngle &angles )
{ static_cast<CRender*>(this)->SetMainView( vecOrigin, angles ); }
void IRender::ViewSetupVisEx( bool novis, int numorigins, const Vector origin[], unsigned int &returnFlags )
{ static_cast<CRender*>(this)->ViewSetupVisEx( novis, numorigins, origin, returnFlags ); }
void IRender::OverrideViewFrustum( Frustum custom )
{ static_cast<CRender*>(this)->OverrideViewFrustum( custom ); }
void IRender::UpdateBrushModelLightmap( model_t *model, IClientRenderable *Renderable )
{ static_cast<CRender*>(this)->UpdateBrushModelLightmap( model, Renderable ); }
void IRender::BeginUpdateLightmaps( void )
{ static_cast<CRender*>(this)->BeginUpdateLightmaps(); }
void IRender::EndUpdateLightmaps()
{ static_cast<CRender*>(this)->EndUpdateLightmaps(); }
bool IRender::InLightmapUpdate( void ) const
{ return static_cast<const CRender*>(this)->InLightmapUpdate(); }

