import ks.core.basetypes;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.math.vector2d;
import ks.math.vmatrix;
import ks.math.volumeculler;
import ks.tier1.exprevaluator;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IVRenderView class to CVRenderView. Included by view.cpp.

void IVRenderView::DrawBrushModel( IClientEntity *baseentity, model_t *model, const Vector& origin, const QAngle& angles, bool bUnused )
{ static_cast<CVRenderView*>(this)->DrawBrushModel( baseentity, model, origin, angles, bUnused ); }
void IVRenderView::DrawIdentityBrushModel( IWorldRenderList *pList, model_t *model )
{ static_cast<CVRenderView*>(this)->DrawIdentityBrushModel( pList, model ); }
void IVRenderView::TouchLight( struct dlight_t *light )
{ static_cast<CVRenderView*>(this)->TouchLight( light ); }
void IVRenderView::Draw3DDebugOverlays( void )
{ static_cast<CVRenderView*>(this)->Draw3DDebugOverlays(); }
void IVRenderView::SetBlend( float blend )
{ static_cast<CVRenderView*>(this)->SetBlend( blend ); }
float IVRenderView::GetBlend( void )
{ return static_cast<CVRenderView*>(this)->GetBlend(); }
void IVRenderView::SetColorModulation( float const* blend )
{ static_cast<CVRenderView*>(this)->SetColorModulation( blend ); }
void IVRenderView::GetColorModulation( float* blend )
{ static_cast<CVRenderView*>(this)->GetColorModulation( blend ); }
void IVRenderView::SceneBegin( void )
{ static_cast<CVRenderView*>(this)->SceneBegin(); }
void IVRenderView::SceneEnd( void )
{ static_cast<CVRenderView*>(this)->SceneEnd(); }
void IVRenderView::GetVisibleFogVolume( const Vector& eyePoint, const VisOverrideData_t *pVisOverrideData, VisibleFogVolumeInfo_t *pInfo )
{ static_cast<CVRenderView*>(this)->GetVisibleFogVolume( eyePoint, pVisOverrideData, pInfo ); }
IWorldRenderList * IVRenderView::CreateWorldList()
{ return static_cast<CVRenderView*>(this)->CreateWorldList(); }
void IVRenderView::BuildWorldLists_Epilogue( IWorldRenderList *pList, WorldListInfo_t* pInfo, bool bShadowDepth )
{ static_cast<CVRenderView*>(this)->BuildWorldLists_Epilogue( pList, pInfo, bShadowDepth ); }
void IVRenderView::BuildWorldLists( IWorldRenderList *pList, WorldListInfo_t* pInfo, int iForceFViewLeaf, const VisOverrideData_t* pVisData, bool bShadowDepth, float *pReflectionWaterHeight )
{ static_cast<CVRenderView*>(this)->BuildWorldLists( pList, pInfo, iForceFViewLeaf, pVisData, bShadowDepth, pReflectionWaterHeight ); }
void IVRenderView::DrawWorldLists( IMatRenderContext *pRenderContext, IWorldRenderList *pList, unsigned long flags, float waterZAdjust )
{ static_cast<CVRenderView*>(this)->DrawWorldLists( pRenderContext, pList, flags, waterZAdjust ); }
void IVRenderView::GetWorldListIndicesInfo( WorldListIndicesInfo_t * pIndicesInfoOut, IWorldRenderList *pList, unsigned long nFlags )
{ static_cast<CVRenderView*>(this)->GetWorldListIndicesInfo( pIndicesInfoOut, pList, nFlags ); }
void IVRenderView::DrawTopView( bool enable )
{ static_cast<CVRenderView*>(this)->DrawTopView( enable ); }
void IVRenderView::TopViewNoBackfaceCulling( bool bDisable )
{ static_cast<CVRenderView*>(this)->TopViewNoBackfaceCulling( bDisable ); }
void IVRenderView::TopViewNoVisCheck( bool bDisable )
{ static_cast<CVRenderView*>(this)->TopViewNoVisCheck( bDisable ); }
void IVRenderView::TopViewBounds( Vector2D const& mins, Vector2D const& maxs )
{ static_cast<CVRenderView*>(this)->TopViewBounds( mins, maxs ); }
void IVRenderView::SetTopViewVolumeCuller( const CVolumeCuller *pVolumeCuller )
{ static_cast<CVRenderView*>(this)->SetTopViewVolumeCuller( pVolumeCuller ); }
void IVRenderView::DrawLights( void )
{ static_cast<CVRenderView*>(this)->DrawLights(); }
void IVRenderView::DrawMaskEntities( void )
{ static_cast<CVRenderView*>(this)->DrawMaskEntities(); }
void IVRenderView::DrawTranslucentSurfaces( IMatRenderContext *pRenderContext, IWorldRenderList *pList, int *pSortList, int sortCount, unsigned long flags )
{ static_cast<CVRenderView*>(this)->DrawTranslucentSurfaces( pRenderContext, pList, pSortList, sortCount, flags ); }
void IVRenderView::DrawLineFile( void )
{ static_cast<CVRenderView*>(this)->DrawLineFile(); }
void IVRenderView::DrawLightmaps( IWorldRenderList *pList, int pageId )
{ static_cast<CVRenderView*>(this)->DrawLightmaps( pList, pageId ); }
void IVRenderView::ViewSetupVis( bool novis, int numorigins, const Vector origin[] )
{ static_cast<CVRenderView*>(this)->ViewSetupVis( novis, numorigins, origin ); }
bool IVRenderView::AreAnyLeavesVisible( int *leafList, int nLeaves )
{ return static_cast<CVRenderView*>(this)->AreAnyLeavesVisible( leafList, nLeaves ); }
void IVRenderView::ViewDrawFade( byte *color, IMaterial *pMaterial, bool mapFullTextureToScreen )
{ static_cast<CVRenderView*>(this)->ViewDrawFade( color, pMaterial, mapFullTextureToScreen ); }
void IVRenderView::OLD_SetProjectionMatrix( float fov, float zNear, float zFar )
{ static_cast<CVRenderView*>(this)->OLD_SetProjectionMatrix( fov, zNear, zFar ); }
colorVec IVRenderView::GetLightAtPoint( Vector& pos )
{ return static_cast<CVRenderView*>(this)->GetLightAtPoint( pos ); }
int IVRenderView::GetViewEntity( void )
{ return static_cast<CVRenderView*>(this)->GetViewEntity(); }
bool IVRenderView::IsViewEntity( int entindex )
{ return static_cast<CVRenderView*>(this)->IsViewEntity( entindex ); }
float IVRenderView::GetFieldOfView( void )
{ return static_cast<CVRenderView*>(this)->GetFieldOfView(); }
unsigned char ** IVRenderView::GetAreaBits( void )
{ return static_cast<CVRenderView*>(this)->GetAreaBits(); }
void IVRenderView::SetFogVolumeState( int nVisibleFogVolume, bool bUseHeightFog )
{ static_cast<CVRenderView*>(this)->SetFogVolumeState( nVisibleFogVolume, bUseHeightFog ); }
void IVRenderView::InstallBrushSurfaceRenderer( IBrushRenderer* pBrushRenderer )
{ static_cast<CVRenderView*>(this)->InstallBrushSurfaceRenderer( pBrushRenderer ); }
void IVRenderView::DrawBrushModelShadow( IClientRenderable *pRenderable )
{ static_cast<CVRenderView*>(this)->DrawBrushModelShadow( pRenderable ); }
bool IVRenderView::LeafContainsTranslucentSurfaces( IWorldRenderList *pList, int sortIndex, unsigned long flags )
{ return static_cast<CVRenderView*>(this)->LeafContainsTranslucentSurfaces( pList, sortIndex, flags ); }
bool IVRenderView::DoesBoxIntersectWaterVolume( const Vector &mins, const Vector &maxs, int leafWaterDataID )
{ return static_cast<CVRenderView*>(this)->DoesBoxIntersectWaterVolume( mins, maxs, leafWaterDataID ); }
void IVRenderView::SetAreaState( unsigned char chAreaBits[MAX_AREA_STATE_BYTES], unsigned char chAreaPortalBits[MAX_AREA_PORTAL_STATE_BYTES] )
{ static_cast<CVRenderView*>(this)->SetAreaState( chAreaBits, chAreaPortalBits ); }
void IVRenderView::Push3DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes )
{ static_cast<CVRenderView*>(this)->Push3DView( pRenderContext, view, nFlags, pRenderTarget, frustumPlanes ); }
void IVRenderView::Push2DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes )
{ static_cast<CVRenderView*>(this)->Push2DView( pRenderContext, view, nFlags, pRenderTarget, frustumPlanes ); }
void IVRenderView::PopView( IMatRenderContext *pRenderContext, Frustum frustumPlanes )
{ static_cast<CVRenderView*>(this)->PopView( pRenderContext, frustumPlanes ); }
void IVRenderView::SetMainView( const Vector &vecOrigin, const QAngle &angles )
{ static_cast<CVRenderView*>(this)->SetMainView( vecOrigin, angles ); }
void IVRenderView::ViewSetupVisEx( bool novis, int numorigins, const Vector origin[], unsigned int &returnFlags )
{ static_cast<CVRenderView*>(this)->ViewSetupVisEx( novis, numorigins, origin, returnFlags ); }
void IVRenderView::OverrideViewFrustum( Frustum custom )
{ static_cast<CVRenderView*>(this)->OverrideViewFrustum( custom ); }
void IVRenderView::DrawBrushModelShadowDepth( IClientEntity *baseentity, model_t *model, const Vector& origin, const QAngle& angles, ERenderDepthMode_t DepthMode )
{ static_cast<CVRenderView*>(this)->DrawBrushModelShadowDepth( baseentity, model, origin, angles, DepthMode ); }
void IVRenderView::UpdateBrushModelLightmap( model_t *model, IClientRenderable *pRenderable )
{ static_cast<CVRenderView*>(this)->UpdateBrushModelLightmap( model, pRenderable ); }
void IVRenderView::BeginUpdateLightmaps( void )
{ static_cast<CVRenderView*>(this)->BeginUpdateLightmaps(); }
void IVRenderView::EndUpdateLightmaps()
{ static_cast<CVRenderView*>(this)->EndUpdateLightmaps(); }
void IVRenderView::OLD_SetOffCenterProjectionMatrix( float fov, float zNear, float zFar, float flAspectRatio, float flBottom, float flTop, float flLeft, float flRight )
{ static_cast<CVRenderView*>(this)->OLD_SetOffCenterProjectionMatrix( fov, zNear, zFar, flAspectRatio, flBottom, flTop, flLeft, flRight ); }
void IVRenderView::OLD_SetProjectionMatrixOrtho( float left, float top, float right, float bottom, float zNear, float zFar )
{ static_cast<CVRenderView*>(this)->OLD_SetProjectionMatrixOrtho( left, top, right, bottom, zNear, zFar ); }
void IVRenderView::Push3DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes, ITexture* pDepthTexture )
{ static_cast<CVRenderView*>(this)->Push3DView( pRenderContext, view, nFlags, pRenderTarget, frustumPlanes, pDepthTexture ); }
void IVRenderView::GetMatricesForView( const CViewSetup &view, VMatrix *pWorldToView, VMatrix *pViewToProjection, VMatrix *pWorldToProjection, VMatrix *pWorldToPixels )
{ static_cast<CVRenderView*>(this)->GetMatricesForView( view, pWorldToView, pViewToProjection, pWorldToProjection, pWorldToPixels ); }
void IVRenderView::DrawBrushModelEx( IClientEntity *baseentity, model_t *model, const Vector& origin, const QAngle& angles, DrawBrushModelMode_t mode )
{ static_cast<CVRenderView*>(this)->DrawBrushModelEx( baseentity, model, origin, angles, mode ); }
bool IVRenderView::DoesBrushModelNeedPowerOf2Framebuffer( const model_t *model )
{ return static_cast<CVRenderView*>(this)->DoesBrushModelNeedPowerOf2Framebuffer( model ); }
void IVRenderView::DrawBrushModelArray( IMatRenderContext* pContext, int nCount, const BrushArrayInstanceData_t *pInstanceData, int nModelTypeFlags )
{ static_cast<CVRenderView*>(this)->DrawBrushModelArray( pContext, nCount, pInstanceData, nModelTypeFlags ); }

