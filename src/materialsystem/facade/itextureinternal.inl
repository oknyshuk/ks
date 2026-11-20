import ks.core.basetypes;
import ks.math.compressed_vector;
import ks.math.vector;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the ITextureInternal class to the texture implementations, selected by the kind
// the texture was created with. Included by ctexture.cpp after the texture classes.

void ITextureInternal::Bind( Sampler_t sampler, TextureBindFlags_t nBindFlags )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->Bind( sampler, nBindFlags ); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->Bind( sampler, nBindFlags ); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->Bind( sampler, nBindFlags ); return;
	}
	std::unreachable();
}
void ITextureInternal::Bind( Sampler_t sampler1, TextureBindFlags_t nBindFlags, int nFrame, Sampler_t sampler2 )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->Bind( sampler1, nBindFlags, nFrame, sampler2 ); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->Bind( sampler1, nBindFlags, nFrame, sampler2 ); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->Bind( sampler1, nBindFlags, nFrame, sampler2 ); return;
	}
	std::unreachable();
}
void ITextureInternal::GetReflectivity( Vector& reflectivity )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->GetReflectivity( reflectivity ); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->GetReflectivity( reflectivity ); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->GetReflectivity( reflectivity ); return;
	}
	std::unreachable();
}
bool ITextureInternal::SetRenderTarget( int nRenderTargetID )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<CTexture *>( this )->SetRenderTarget( nRenderTargetID );
	case TextureKind::MultiRenderTarget: return static_cast<CTexture_MultipleRenderTarget *>( this )->SetRenderTarget( nRenderTargetID );
	case TextureKind::Reference: return static_cast<CReferenceToHandleTexture *>( this )->SetRenderTarget( nRenderTargetID );
	}
	std::unreachable();
}
void ITextureInternal::Release()
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->Release(); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->Release(); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->Release(); return;
	}
	std::unreachable();
}
void ITextureInternal::OnRestore()
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->OnRestore(); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->OnRestore(); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->OnRestore(); return;
	}
	std::unreachable();
}
void ITextureInternal::SetFilteringAndClampingMode()
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->SetFilteringAndClampingMode(); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->SetFilteringAndClampingMode(); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->SetFilteringAndClampingMode(); return;
	}
	std::unreachable();
}
void ITextureInternal::Precache()
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->Precache(); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->Precache(); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->Precache(); return;
	}
	std::unreachable();
}
void ITextureInternal::CopyFrameBufferToMe( int nRenderTargetID, Rect_t *pSrcRect, Rect_t *pDstRect )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->CopyFrameBufferToMe( nRenderTargetID, pSrcRect, pDstRect ); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->CopyFrameBufferToMe( nRenderTargetID, pSrcRect, pDstRect ); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->CopyFrameBufferToMe( nRenderTargetID, pSrcRect, pDstRect ); return;
	}
	std::unreachable();
}
void ITextureInternal::CopyMeToFrameBuffer( int nRenderTargetID, Rect_t *pSrcRect, Rect_t *pDstRect )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->CopyMeToFrameBuffer( nRenderTargetID, pSrcRect, pDstRect ); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->CopyMeToFrameBuffer( nRenderTargetID, pSrcRect, pDstRect ); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->CopyMeToFrameBuffer( nRenderTargetID, pSrcRect, pDstRect ); return;
	}
	std::unreachable();
}
ITexture * ITextureInternal::GetEmbeddedTexture( int nIndex )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<CTexture *>( this )->GetEmbeddedTexture( nIndex );
	case TextureKind::MultiRenderTarget: return static_cast<CTexture_MultipleRenderTarget *>( this )->GetEmbeddedTexture( nIndex );
	case TextureKind::Reference: return static_cast<CReferenceToHandleTexture *>( this )->GetEmbeddedTexture( nIndex );
	}
	std::unreachable();
}
ShaderAPITextureHandle_t ITextureInternal::GetTextureHandle( int nFrame, int nTextureChannel )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<CTexture *>( this )->GetTextureHandle( nFrame, nTextureChannel );
	case TextureKind::MultiRenderTarget: return static_cast<CTexture_MultipleRenderTarget *>( this )->GetTextureHandle( nFrame, nTextureChannel );
	case TextureKind::Reference: return static_cast<CReferenceToHandleTexture *>( this )->GetTextureHandle( nFrame, nTextureChannel );
	}
	std::unreachable();
}
ImageFormat ITextureInternal::GetImageFormat() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->GetImageFormat();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->GetImageFormat();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->GetImageFormat();
	}
	std::unreachable();
}
bool ITextureInternal::SetRenderTarget( int nRenderTargetID, ITexture* pDepthTexture )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<CTexture *>( this )->SetRenderTarget( nRenderTargetID, pDepthTexture );
	case TextureKind::MultiRenderTarget: return static_cast<CTexture_MultipleRenderTarget *>( this )->SetRenderTarget( nRenderTargetID, pDepthTexture );
	case TextureKind::Reference: return static_cast<CReferenceToHandleTexture *>( this )->SetRenderTarget( nRenderTargetID, pDepthTexture );
	}
	std::unreachable();
}
void ITextureInternal::BindVertexTexture( VertexTextureSampler_t sampler, int frameNum )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->BindVertexTexture( sampler, frameNum ); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->BindVertexTexture( sampler, frameNum ); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->BindVertexTexture( sampler, frameNum ); return;
	}
	std::unreachable();
}
void ITextureInternal::MarkAsPreloaded( bool bSet )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->MarkAsPreloaded( bSet ); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->MarkAsPreloaded( bSet ); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->MarkAsPreloaded( bSet ); return;
	}
	std::unreachable();
}
bool ITextureInternal::IsPreloaded() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->IsPreloaded();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->IsPreloaded();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->IsPreloaded();
	}
	std::unreachable();
}
void ITextureInternal::MarkAsExcluded( bool bSet, int nDimensionsLimit, bool bMarkAsTrumpedExclude )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->MarkAsExcluded( bSet, nDimensionsLimit, bMarkAsTrumpedExclude ); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->MarkAsExcluded( bSet, nDimensionsLimit, bMarkAsTrumpedExclude ); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->MarkAsExcluded( bSet, nDimensionsLimit, bMarkAsTrumpedExclude ); return;
	}
	std::unreachable();
}
bool ITextureInternal::UpdateExcludedState()
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<CTexture *>( this )->UpdateExcludedState();
	case TextureKind::MultiRenderTarget: return static_cast<CTexture_MultipleRenderTarget *>( this )->UpdateExcludedState();
	case TextureKind::Reference: return static_cast<CReferenceToHandleTexture *>( this )->UpdateExcludedState();
	}
	std::unreachable();
}
bool ITextureInternal::IsTempRenderTarget() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->IsTempRenderTarget();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->IsTempRenderTarget();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->IsTempRenderTarget();
	}
	std::unreachable();
}
void ITextureInternal::ReloadFilesInList( IFileList *pFilesToReload )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->ReloadFilesInList( pFilesToReload ); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->ReloadFilesInList( pFilesToReload ); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->ReloadFilesInList( pFilesToReload ); return;
	}
	std::unreachable();
}
bool ITextureInternal::IsMultiRenderTarget()
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<CTexture *>( this )->IsMultiRenderTarget();
	case TextureKind::MultiRenderTarget: return static_cast<CTexture_MultipleRenderTarget *>( this )->IsMultiRenderTarget();
	case TextureKind::Reference: return static_cast<CReferenceToHandleTexture *>( this )->IsMultiRenderTarget();
	}
	std::unreachable();
}
bool ITextureInternal::MarkAsTempExcluded( bool bSet, int nExcludedDimensionLimit )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<CTexture *>( this )->MarkAsTempExcluded( bSet, nExcludedDimensionLimit );
	case TextureKind::MultiRenderTarget: return static_cast<CTexture_MultipleRenderTarget *>( this )->MarkAsTempExcluded( bSet, nExcludedDimensionLimit );
	case TextureKind::Reference: return static_cast<CReferenceToHandleTexture *>( this )->MarkAsTempExcluded( bSet, nExcludedDimensionLimit );
	}
	std::unreachable();
}
bool ITextureInternal::IsForceExcluded() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->IsForceExcluded();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->IsForceExcluded();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->IsForceExcluded();
	}
	std::unreachable();
}
bool ITextureInternal::ClearForceExclusion()
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<CTexture *>( this )->ClearForceExclusion();
	case TextureKind::MultiRenderTarget: return static_cast<CTexture_MultipleRenderTarget *>( this )->ClearForceExclusion();
	case TextureKind::Reference: return static_cast<CReferenceToHandleTexture *>( this )->ClearForceExclusion();
	}
	std::unreachable();
}
bool ITextureInternal::IsAsyncDone() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->IsAsyncDone();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->IsAsyncDone();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->IsAsyncDone();
	}
	std::unreachable();
}
