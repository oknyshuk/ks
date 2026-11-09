#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2004 Valve Corporation

#include "materialsystem/imorph.h"
#include "shaderapi/shareddefs.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class ITextureInternal;


//-----------------------------------------------------------------------------
// Render context for morphs
//-----------------------------------------------------------------------------
class IMorphMgrRenderContext
{
public:
};


//-----------------------------------------------------------------------------
//
// Morph data class 
//
//-----------------------------------------------------------------------------
class IMorphInternal : public IMorph
{
public:
	void Init( MorphFormat_t format, const char *pDebugName );
	bool Bind( IMorphMgrRenderContext *pRenderContext );
	MorphFormat_t GetMorphFormat() const;
};


//-----------------------------------------------------------------------------
// Morph manager class
//-----------------------------------------------------------------------------
class IMorphMgr
{
public:
	// Allocate,free scratch textures + materials
	bool ShouldAllocateScratchTextures();
	void AllocateScratchTextures();
	void FreeScratchTextures();
	void AllocateMaterials();
	void FreeMaterials();

	// Returns the morph accumulator scratch texture
	ITextureInternal *MorphAccumulator();
	ITextureInternal *MorphWeights();

	// Class factory
	IMorphInternal *CreateMorph();
	void DestroyMorph( IMorphInternal *pMorphData );

	// Max morphs between Begin/End
	int MaxHWMorphBatchCount() const;

	// Begin, end morph accumulation phase
	void BeginMorphAccumulation( IMorphMgrRenderContext *pRenderContext );
	void EndMorphAccumulation( IMorphMgrRenderContext *pRenderContext );

	// Accumulate a morph
	void AccumulateMorph( IMorphMgrRenderContext *pRenderContext, IMorph* pMorph, int nMorphCount, const MorphWeight_t* pWeights );

	// Advances frame count (for debugging)
	void AdvanceFrame();

	// Returns the location of a particular vertex in the morph accumulator
	bool GetMorphAccumulatorTexCoord( IMorphMgrRenderContext *pRenderContext, Vector2D *pTexCoord, IMorph *pMorph, int nVertex );

	// Allocate, free morph mgr render context data.
	IMorphMgrRenderContext *AllocateRenderContext();
	void FreeRenderContext( IMorphMgrRenderContext *pRenderContext );
};

extern IMorphMgr *g_pMorphMgr;
