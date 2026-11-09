// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IBrushSurface class to CBrushSurface. Included by gl_rsurf.cpp.

void IBrushSurface::ComputeTextureCoordinate( Vector const& worldPos, Vector2D& texCoord )
{ static_cast<CBrushSurface*>(this)->ComputeTextureCoordinate( worldPos, texCoord ); }
void IBrushSurface::ComputeLightmapCoordinate( Vector const& worldPos, Vector2D& lightmapCoord )
{ static_cast<CBrushSurface*>(this)->ComputeLightmapCoordinate( worldPos, lightmapCoord ); }
int IBrushSurface::GetVertexCount() const
{ return static_cast<const CBrushSurface*>(this)->GetVertexCount(); }
void IBrushSurface::GetVertexData( BrushVertex_t* pVerts )
{ static_cast<CBrushSurface*>(this)->GetVertexData( pVerts ); }
IMaterial* IBrushSurface::GetMaterial()
{ return static_cast<CBrushSurface*>(this)->GetMaterial(); }

