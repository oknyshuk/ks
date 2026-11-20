// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

#include "studiorender.h"
#include "studio.h"
#include "optimize.h"
#include "studiorendercontext.h"
import std;
import ks.tier1.convar;

ConVar g_cv_morph_path("morph_path", "7");
ConVar g_cv_morph_debug("morph_debug", "0");



template
void CCachedRenderData::ComputeFlexedVertex_StreamOffset<mstudiovertanim_t>( studiohdr_t *pStudioHdr, mstudioflex_t *pflex, 
														 mstudiovertanim_t *pvanim, int vertCount, float w1, float w2, float w3, float w4 );
template
void CCachedRenderData::ComputeFlexedVertex_StreamOffset<mstudiovertanim_wrinkle_t>( studiohdr_t *pStudioHdr, mstudioflex_t *pflex, 
														 mstudiovertanim_wrinkle_t *pvanim, int vertCount, float w1, float w2, float w3, float w4 );

// vectorized
void CCachedRenderData::ComputeFlexedVertex_StreamOffset_Optimized( studiohdr_t *pStudioHdr, mstudioflex_t *pflex, mstudiovertanim_t *pvanim, int vertCount, float w1, float w2, float w3, float w4 )
{
	{
		ComputeFlexedVertex_StreamOffset( pStudioHdr, pflex, pvanim, vertCount, w1, w2, w3, w4);
	}
}


void CCachedRenderData::ComputeFlexedVertexWrinkle_StreamOffset_Optimized( studiohdr_t *pStudioHdr, mstudioflex_t *pflex, mstudiovertanim_wrinkle_t *pvanim, int vertCount, float w1, float w2, float w3, float w4)
{

	{
		ComputeFlexedVertex_StreamOffset( pStudioHdr, pflex, pvanim, vertCount, w1, w2, w3, w4);
	}
}
