// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the ITexture class to the texture implementations, selected by the kind
// the texture was created with. Included by ctexture.cpp after the texture classes.

const char * ITexture::GetName() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->GetName();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->GetName();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->GetName();
	}
	std::unreachable();
}
int ITexture::GetMappingWidth() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->GetMappingWidth();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->GetMappingWidth();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->GetMappingWidth();
	}
	std::unreachable();
}
int ITexture::GetMappingHeight() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->GetMappingHeight();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->GetMappingHeight();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->GetMappingHeight();
	}
	std::unreachable();
}
int ITexture::GetActualWidth() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->GetActualWidth();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->GetActualWidth();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->GetActualWidth();
	}
	std::unreachable();
}
int ITexture::GetActualHeight() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->GetActualHeight();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->GetActualHeight();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->GetActualHeight();
	}
	std::unreachable();
}
int ITexture::GetNumAnimationFrames() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->GetNumAnimationFrames();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->GetNumAnimationFrames();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->GetNumAnimationFrames();
	}
	std::unreachable();
}
bool ITexture::IsTranslucent() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->IsTranslucent();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->IsTranslucent();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->IsTranslucent();
	}
	std::unreachable();
}
bool ITexture::IsMipmapped() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->IsMipmapped();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->IsMipmapped();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->IsMipmapped();
	}
	std::unreachable();
}
void ITexture::GetLowResColorSample( float s, float t, float *color ) const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<const CTexture *>( this )->GetLowResColorSample( s, t, color ); return;
	case TextureKind::MultiRenderTarget: static_cast<const CTexture_MultipleRenderTarget *>( this )->GetLowResColorSample( s, t, color ); return;
	case TextureKind::Reference: static_cast<const CReferenceToHandleTexture *>( this )->GetLowResColorSample( s, t, color ); return;
	}
	std::unreachable();
}
void * ITexture::GetResourceData( uint32 eDataType, std::size_t *pNumBytes ) const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->GetResourceData( eDataType, pNumBytes );
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->GetResourceData( eDataType, pNumBytes );
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->GetResourceData( eDataType, pNumBytes );
	}
	std::unreachable();
}
void ITexture::IncrementReferenceCount()
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->IncrementReferenceCount(); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->IncrementReferenceCount(); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->IncrementReferenceCount(); return;
	}
	std::unreachable();
}
void ITexture::DecrementReferenceCount()
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->DecrementReferenceCount(); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->DecrementReferenceCount(); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->DecrementReferenceCount(); return;
	}
	std::unreachable();
}
void ITexture::SetTextureRegenerator( ITextureRegenerator *pTextureRegen, bool releaseExisting )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->SetTextureRegenerator( pTextureRegen, releaseExisting ); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->SetTextureRegenerator( pTextureRegen, releaseExisting ); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->SetTextureRegenerator( pTextureRegen, releaseExisting ); return;
	}
	std::unreachable();
}
void ITexture::Download( Rect_t *pRect, int nAdditionalCreationFlags )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->Download( pRect, nAdditionalCreationFlags ); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->Download( pRect, nAdditionalCreationFlags ); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->Download( pRect, nAdditionalCreationFlags ); return;
	}
	std::unreachable();
}
int ITexture::GetApproximateVidMemBytes() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->GetApproximateVidMemBytes();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->GetApproximateVidMemBytes();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->GetApproximateVidMemBytes();
	}
	std::unreachable();
}
bool ITexture::IsError() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->IsError();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->IsError();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->IsError();
	}
	std::unreachable();
}
bool ITexture::IsVolumeTexture() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->IsVolumeTexture();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->IsVolumeTexture();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->IsVolumeTexture();
	}
	std::unreachable();
}
int ITexture::GetMappingDepth() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->GetMappingDepth();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->GetMappingDepth();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->GetMappingDepth();
	}
	std::unreachable();
}
int ITexture::GetActualDepth() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->GetActualDepth();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->GetActualDepth();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->GetActualDepth();
	}
	std::unreachable();
}
ImageFormat ITexture::GetImageFormat() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->GetImageFormat();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->GetImageFormat();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->GetImageFormat();
	}
	std::unreachable();
}
bool ITexture::IsRenderTarget() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->IsRenderTarget();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->IsRenderTarget();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->IsRenderTarget();
	}
	std::unreachable();
}
bool ITexture::IsCubeMap() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->IsCubeMap();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->IsCubeMap();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->IsCubeMap();
	}
	std::unreachable();
}
bool ITexture::IsNormalMap() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->IsNormalMap();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->IsNormalMap();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->IsNormalMap();
	}
	std::unreachable();
}
bool ITexture::IsProcedural() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->IsProcedural();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->IsProcedural();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->IsProcedural();
	}
	std::unreachable();
}
bool ITexture::IsDefaultPool() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->IsDefaultPool();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->IsDefaultPool();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->IsDefaultPool();
	}
	std::unreachable();
}
void ITexture::DeleteIfUnreferenced()
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->DeleteIfUnreferenced(); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->DeleteIfUnreferenced(); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->DeleteIfUnreferenced(); return;
	}
	std::unreachable();
}
void ITexture::SwapContents( ITexture *pOther )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->SwapContents( pOther ); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->SwapContents( pOther ); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->SwapContents( pOther ); return;
	}
	std::unreachable();
}
unsigned int ITexture::GetFlags() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->GetFlags();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->GetFlags();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->GetFlags();
	}
	std::unreachable();
}
void ITexture::ForceLODOverride( int iNumLodsOverrideUpOrDown )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->ForceLODOverride( iNumLodsOverrideUpOrDown ); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->ForceLODOverride( iNumLodsOverrideUpOrDown ); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->ForceLODOverride( iNumLodsOverrideUpOrDown ); return;
	}
	std::unreachable();
}
void ITexture::ForceExcludeOverride( int iExcludeOverride )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->ForceExcludeOverride( iExcludeOverride ); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->ForceExcludeOverride( iExcludeOverride ); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->ForceExcludeOverride( iExcludeOverride ); return;
	}
	std::unreachable();
}
void ITexture::AddDownsizedSubTarget( const char *szName, int iDownsizePow2, MaterialRenderTargetDepth_t depth )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->AddDownsizedSubTarget( szName, iDownsizePow2, depth ); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->AddDownsizedSubTarget( szName, iDownsizePow2, depth ); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->AddDownsizedSubTarget( szName, iDownsizePow2, depth ); return;
	}
	std::unreachable();
}
void ITexture::SetActiveSubTarget( const char *szName )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: static_cast<CTexture *>( this )->SetActiveSubTarget( szName ); return;
	case TextureKind::MultiRenderTarget: static_cast<CTexture_MultipleRenderTarget *>( this )->SetActiveSubTarget( szName ); return;
	case TextureKind::Reference: static_cast<CReferenceToHandleTexture *>( this )->SetActiveSubTarget( szName ); return;
	}
	std::unreachable();
}
int ITexture::GetReferenceCount() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->GetReferenceCount();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->GetReferenceCount();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->GetReferenceCount();
	}
	std::unreachable();
}
bool ITexture::IsTempExcluded() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->IsTempExcluded();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->IsTempExcluded();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->IsTempExcluded();
	}
	std::unreachable();
}
bool ITexture::CanBeTempExcluded() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->CanBeTempExcluded();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->CanBeTempExcluded();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->CanBeTempExcluded();
	}
	std::unreachable();
}
bool ITexture::FinishAsyncDownload( AsyncTextureContext_t *pContext, void *pData, int nNumReadBytes, bool bAbort, float flMaxTimeMs )
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<CTexture *>( this )->FinishAsyncDownload( pContext, pData, nNumReadBytes, bAbort, flMaxTimeMs );
	case TextureKind::MultiRenderTarget: return static_cast<CTexture_MultipleRenderTarget *>( this )->FinishAsyncDownload( pContext, pData, nNumReadBytes, bAbort, flMaxTimeMs );
	case TextureKind::Reference: return static_cast<CReferenceToHandleTexture *>( this )->FinishAsyncDownload( pContext, pData, nNumReadBytes, bAbort, flMaxTimeMs );
	}
	std::unreachable();
}
bool ITexture::IsForceExcluded() const
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<const CTexture *>( this )->IsForceExcluded();
	case TextureKind::MultiRenderTarget: return static_cast<const CTexture_MultipleRenderTarget *>( this )->IsForceExcluded();
	case TextureKind::Reference: return static_cast<const CReferenceToHandleTexture *>( this )->IsForceExcluded();
	}
	std::unreachable();
}
bool ITexture::ClearForceExclusion()
{
	switch ( static_cast<const ITextureInternal *>( this )->Kind() )
	{
	case TextureKind::Standard: return static_cast<CTexture *>( this )->ClearForceExclusion();
	case TextureKind::MultiRenderTarget: return static_cast<CTexture_MultipleRenderTarget *>( this )->ClearForceExclusion();
	case TextureKind::Reference: return static_cast<CReferenceToHandleTexture *>( this )->ClearForceExclusion();
	}
	std::unreachable();
}
