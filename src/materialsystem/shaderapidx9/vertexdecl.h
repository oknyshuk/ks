#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "locald3dtypes.h"
#include "materialsystem/imaterial.h"

//
// VertexStreamSpec_t *pStreamSpec
//		is an array of stream specifications terminated with an entry {VERTEX_FORMAT_UNKNOWN, STREAM_DEFAULT}
//		or NULL if all the streams should be mapped in a default manner
//

//-----------------------------------------------------------------------------
// Gets the declspec associated with a vertex format
//-----------------------------------------------------------------------------
IDirect3DVertexDeclaration9 *FindOrCreateVertexDecl( VertexFormat_t fmt, bool bStaticLit, bool bUsingFlex, bool bUsingMorph, bool bUsingPreTessPatch, VertexStreamSpec_t *pStreamSpec );

//-----------------------------------------------------------------------------
// Clears out all declspecs
//-----------------------------------------------------------------------------
void ReleaseAllVertexDecl( );
