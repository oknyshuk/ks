// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IDispInfo class to CDispInfo. Included by disp.cpp.

void IDispInfo::GetIntersectingSurfaces( GetIntersectingSurfaces_Struct *pStruct )
{ static_cast<CDispInfo*>(this)->GetIntersectingSurfaces( pStruct ); }
void IDispInfo::RenderWireframeInLightmapPage( int pageId )
{ static_cast<CDispInfo*>(this)->RenderWireframeInLightmapPage( pageId ); }
void IDispInfo::GetBoundingBox( Vector &bbMin, Vector &bbMax )
{ static_cast<CDispInfo*>(this)->GetBoundingBox( bbMin, bbMax ); }
void IDispInfo::SetParent( SurfaceHandle_t surfID )
{ static_cast<CDispInfo*>(this)->SetParent( surfID ); }
SurfaceHandle_t IDispInfo::GetParent()
{ return static_cast<CDispInfo*>(this)->GetParent(); }
void IDispInfo::AddDynamicLights( struct dlight_t *pLights, unsigned int lightMask )
{ static_cast<CDispInfo*>(this)->AddDynamicLights( pLights, lightMask ); }
unsigned int IDispInfo::ComputeDynamicLightMask( struct dlight_t *pLights )
{ return static_cast<CDispInfo*>(this)->ComputeDynamicLightMask( pLights ); }
DispDecalHandle_t IDispInfo::NotifyAddDecal( decal_t *pDecal, float flSize )
{ return static_cast<CDispInfo*>(this)->NotifyAddDecal( pDecal, flSize ); }
void IDispInfo::NotifyRemoveDecal( DispDecalHandle_t h )
{ static_cast<CDispInfo*>(this)->NotifyRemoveDecal( h ); }
DispShadowHandle_t IDispInfo::AddShadowDecal( ShadowHandle_t shadowHandle )
{ return static_cast<CDispInfo*>(this)->AddShadowDecal( shadowHandle ); }
void IDispInfo::RemoveShadowDecal( DispShadowHandle_t handle )
{ static_cast<CDispInfo*>(this)->RemoveShadowDecal( handle ); }
bool IDispInfo::ComputeShadowFragments( DispShadowHandle_t h, int& vertexCount, int& indexCount )
{ return static_cast<CDispInfo*>(this)->ComputeShadowFragments( h, vertexCount, indexCount ); }
bool IDispInfo::GetTag()
{ return static_cast<CDispInfo*>(this)->GetTag(); }
void IDispInfo::SetTag()
{ static_cast<CDispInfo*>(this)->SetTag(); }
bool IDispInfo::TestRay( Ray_t const& ray, float start, float end, float& dist, Vector2D* lightmapUV, Vector2D* textureUV )
{ return static_cast<CDispInfo*>(this)->TestRay( ray, start, end, dist, lightmapUV, textureUV ); }
void IDispInfo::ComputeLightmapAndTextureCoordinate( RayDispOutput_t const& uv, Vector2D* luv, Vector2D* tuv )
{ static_cast<CDispInfo*>(this)->ComputeLightmapAndTextureCoordinate( uv, luv, tuv ); }

