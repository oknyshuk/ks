// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMorphMgr class to its one implementation, CMorphMgr.
// Included by morph.cpp, which defines MorphMgrImpl().

bool IMorphMgr::ShouldAllocateScratchTextures()
{ return MorphMgrImpl().ShouldAllocateScratchTextures(); }
void IMorphMgr::AllocateScratchTextures()
{ MorphMgrImpl().AllocateScratchTextures(); }
void IMorphMgr::FreeScratchTextures()
{ MorphMgrImpl().FreeScratchTextures(); }
void IMorphMgr::AllocateMaterials()
{ MorphMgrImpl().AllocateMaterials(); }
void IMorphMgr::FreeMaterials()
{ MorphMgrImpl().FreeMaterials(); }
ITextureInternal * IMorphMgr::MorphAccumulator()
{ return MorphMgrImpl().MorphAccumulator(); }
ITextureInternal * IMorphMgr::MorphWeights()
{ return MorphMgrImpl().MorphWeights(); }
IMorphInternal * IMorphMgr::CreateMorph()
{ return MorphMgrImpl().CreateMorph(); }
void IMorphMgr::DestroyMorph( IMorphInternal *pMorphData )
{ MorphMgrImpl().DestroyMorph( pMorphData ); }
int IMorphMgr::MaxHWMorphBatchCount() const
{ return MorphMgrImpl().MaxHWMorphBatchCount(); }
void IMorphMgr::BeginMorphAccumulation( IMorphMgrRenderContext *pRenderContext )
{ MorphMgrImpl().BeginMorphAccumulation( pRenderContext ); }
void IMorphMgr::EndMorphAccumulation( IMorphMgrRenderContext *pRenderContext )
{ MorphMgrImpl().EndMorphAccumulation( pRenderContext ); }
void IMorphMgr::AccumulateMorph( IMorphMgrRenderContext *pRenderContext, IMorph* pMorph, int nMorphCount, const MorphWeight_t* pWeights )
{ MorphMgrImpl().AccumulateMorph( pRenderContext, pMorph, nMorphCount, pWeights ); }
void IMorphMgr::AdvanceFrame()
{ MorphMgrImpl().AdvanceFrame(); }
bool IMorphMgr::GetMorphAccumulatorTexCoord( IMorphMgrRenderContext *pRenderContext, Vector2D *pTexCoord, IMorph *pMorph, int nVertex )
{ return MorphMgrImpl().GetMorphAccumulatorTexCoord( pRenderContext, pTexCoord, pMorph, nVertex ); }
IMorphMgrRenderContext * IMorphMgr::AllocateRenderContext()
{ return MorphMgrImpl().AllocateRenderContext(); }
void IMorphMgr::FreeRenderContext( IMorphMgrRenderContext *pRenderContext )
{ MorphMgrImpl().FreeRenderContext( pRenderContext ); }
