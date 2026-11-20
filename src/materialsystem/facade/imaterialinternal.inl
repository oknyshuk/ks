import ks.core.types;
import ks.tier1.filename_table;
import ks.tier1.keyvalues;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMaterialInternal class to the material implementations, selected by the kind
// the material was created with. Included by imaterialdispatch.cpp.

int IMaterialInternal::GetReferenceCount() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->GetReferenceCount();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->GetReferenceCount();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->GetReferenceCount();
	}
	std::unreachable();
}
void IMaterialInternal::SetEnumerationID( int id )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->SetEnumerationID( id ); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->SetEnumerationID( id ); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->SetEnumerationID( id ); return;
	}
	std::unreachable();
}
void IMaterialInternal::SetNeedsWhiteLightmap( bool val )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->SetNeedsWhiteLightmap( val ); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->SetNeedsWhiteLightmap( val ); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->SetNeedsWhiteLightmap( val ); return;
	}
	std::unreachable();
}
bool IMaterialInternal::GetNeedsWhiteLightmap() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->GetNeedsWhiteLightmap();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->GetNeedsWhiteLightmap();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->GetNeedsWhiteLightmap();
	}
	std::unreachable();
}
void IMaterialInternal::Uncache( bool bPreserveVars )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->Uncache( bPreserveVars ); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->Uncache( bPreserveVars ); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->Uncache( bPreserveVars ); return;
	}
	std::unreachable();
}
void IMaterialInternal::Precache()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->Precache(); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->Precache(); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->Precache(); return;
	}
	std::unreachable();
}
bool IMaterialInternal::PrecacheVars( KeyValues *pKeyValues, KeyValues *pPatchKeyValues, std::vector< FileNameHandle_t > *pIncludes )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->PrecacheVars( pKeyValues, pPatchKeyValues, pIncludes );
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->PrecacheVars( pKeyValues, pPatchKeyValues, pIncludes );
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->PrecacheVars( pKeyValues, pPatchKeyValues, pIncludes );
	}
	std::unreachable();
}
void IMaterialInternal::ReloadTextures()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->ReloadTextures(); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->ReloadTextures(); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->ReloadTextures(); return;
	}
	std::unreachable();
}
void IMaterialInternal::SetMinLightmapPageID( int pageID )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->SetMinLightmapPageID( pageID ); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->SetMinLightmapPageID( pageID ); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->SetMinLightmapPageID( pageID ); return;
	}
	std::unreachable();
}
void IMaterialInternal::SetMaxLightmapPageID( int pageID )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->SetMaxLightmapPageID( pageID ); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->SetMaxLightmapPageID( pageID ); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->SetMaxLightmapPageID( pageID ); return;
	}
	std::unreachable();
}
int IMaterialInternal::GetMinLightmapPageID() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->GetMinLightmapPageID();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->GetMinLightmapPageID();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->GetMinLightmapPageID();
	}
	std::unreachable();
}
int IMaterialInternal::GetMaxLightmapPageID() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->GetMaxLightmapPageID();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->GetMaxLightmapPageID();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->GetMaxLightmapPageID();
	}
	std::unreachable();
}
IShader * IMaterialInternal::GetShader() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->GetShader();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->GetShader();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->GetShader();
	}
	std::unreachable();
}
IMaterialVar** IMaterialInternal::GetVars()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->GetVars();
	case MaterialKind::SubRect: return nullptr;
	case MaterialKind::QueueFriendly: return nullptr;
	}
	std::unreachable();
}
bool IMaterialInternal::IsPrecached() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->IsPrecached();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->IsPrecached();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->IsPrecached();
	}
	std::unreachable();
}
bool IMaterialInternal::IsPrecachedVars() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->IsPrecachedVars();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->IsPrecachedVars();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->IsPrecachedVars();
	}
	std::unreachable();
}
CBasePerMaterialContextData ** IMaterialInternal::GetContextData( int modulationFlags )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->GetContextData( modulationFlags );
	case MaterialKind::SubRect: return nullptr;
	case MaterialKind::QueueFriendly: return nullptr;
	}
	std::unreachable();
}
StateSnapshot_t IMaterialInternal::GetSnapshotId( int modulation, int renderPass )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->GetSnapshotId( modulation, renderPass );
	case MaterialKind::SubRect: return (StateSnapshot_t)-1;
	case MaterialKind::QueueFriendly: return (StateSnapshot_t)-1;
	}
	std::unreachable();
}
unsigned char* IMaterialInternal::GetInstanceCommandBuffer( int modulation )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->GetInstanceCommandBuffer( modulation );
	case MaterialKind::SubRect: return nullptr;
	case MaterialKind::QueueFriendly: return nullptr;
	}
	std::unreachable();
}
void IMaterialInternal::DrawMesh( VertexCompressionType_t vertexCompression, bool bIsAlphaModulating, bool bUsingPreTessPatches )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->DrawMesh( vertexCompression, bIsAlphaModulating, bUsingPreTessPatches ); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->DrawMesh( vertexCompression, bIsAlphaModulating, bUsingPreTessPatches ); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->DrawMesh( vertexCompression, bIsAlphaModulating, bUsingPreTessPatches ); return;
	}
	std::unreachable();
}
VertexFormat_t IMaterialInternal::GetVertexFormat() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->GetVertexFormat();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->GetVertexFormat();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->GetVertexFormat();
	}
	std::unreachable();
}
VertexFormat_t IMaterialInternal::GetVertexUsage() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->GetVertexUsage();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->GetVertexUsage();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->GetVertexUsage();
	}
	std::unreachable();
}
bool IMaterialInternal::PerformDebugTrace() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->PerformDebugTrace();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->PerformDebugTrace();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->PerformDebugTrace();
	}
	std::unreachable();
}
bool IMaterialInternal::NoDebugOverride() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->NoDebugOverride();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->NoDebugOverride();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->NoDebugOverride();
	}
	std::unreachable();
}
void IMaterialInternal::ToggleSuppression()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->ToggleSuppression(); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->ToggleSuppression(); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->ToggleSuppression(); return;
	}
	std::unreachable();
}
bool IMaterialInternal::IsSuppressed() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->IsSuppressed();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->IsSuppressed();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->IsSuppressed();
	}
	std::unreachable();
}
void IMaterialInternal::ToggleDebugTrace()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->ToggleDebugTrace(); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->ToggleDebugTrace(); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->ToggleDebugTrace(); return;
	}
	std::unreachable();
}
bool IMaterialInternal::UseFog() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->UseFog();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->UseFog();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->UseFog();
	}
	std::unreachable();
}
void IMaterialInternal::AddMaterialVar( IMaterialVar *pMaterialVar )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->AddMaterialVar( pMaterialVar ); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->AddMaterialVar( pMaterialVar ); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->AddMaterialVar( pMaterialVar ); return;
	}
	std::unreachable();
}
ShaderRenderState_t * IMaterialInternal::GetRenderState()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->GetRenderState();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->GetRenderState();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->GetRenderState();
	}
	std::unreachable();
}
bool IMaterialInternal::IsManuallyCreated() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->IsManuallyCreated();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->IsManuallyCreated();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->IsManuallyCreated();
	}
	std::unreachable();
}
bool IMaterialInternal::NeedsFixedFunctionFlashlight() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->NeedsFixedFunctionFlashlight();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->NeedsFixedFunctionFlashlight();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->NeedsFixedFunctionFlashlight();
	}
	std::unreachable();
}
bool IMaterialInternal::IsUsingVertexID() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->IsUsingVertexID();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->IsUsingVertexID();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->IsUsingVertexID();
	}
	std::unreachable();
}
void IMaterialInternal::MarkAsPreloaded( bool bSet )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->MarkAsPreloaded( bSet ); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->MarkAsPreloaded( bSet ); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->MarkAsPreloaded( bSet ); return;
	}
	std::unreachable();
}
bool IMaterialInternal::IsPreloaded() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->IsPreloaded();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->IsPreloaded();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->IsPreloaded();
	}
	std::unreachable();
}
void IMaterialInternal::ArtificialAddRef()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->ArtificialAddRef(); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->ArtificialAddRef(); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->ArtificialAddRef(); return;
	}
	std::unreachable();
}
void IMaterialInternal::ArtificialRelease()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->ArtificialRelease(); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->ArtificialRelease(); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->ArtificialRelease(); return;
	}
	std::unreachable();
}
void IMaterialInternal::ReportVarChanged( IMaterialVar *pVar )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->ReportVarChanged( pVar ); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->ReportVarChanged( pVar ); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->ReportVarChanged( pVar ); return;
	}
	std::unreachable();
}
uint32 IMaterialInternal::GetChangeID() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->GetChangeID();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->GetChangeID();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->GetChangeID();
	}
	std::unreachable();
}
uint32 IMaterialInternal::GetChangeTimestamp() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->GetChangeTimestamp();
	case MaterialKind::SubRect: return 0;
	case MaterialKind::QueueFriendly: return 0;
	}
	std::unreachable();
}
bool IMaterialInternal::IsTranslucentInternal( float fAlphaModulation ) const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->IsTranslucentInternal( fAlphaModulation );
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->IsTranslucentInternal( fAlphaModulation );
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->IsTranslucentInternal( fAlphaModulation );
	}
	std::unreachable();
}
bool IMaterialInternal::IsRealTimeVersion() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->IsRealTimeVersion();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->IsRealTimeVersion();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->IsRealTimeVersion();
	}
	std::unreachable();
}
void IMaterialInternal::ClearContextData()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->ClearContextData(); return;
	case MaterialKind::SubRect: return;
	case MaterialKind::QueueFriendly: return;
	}
	std::unreachable();
}
IMaterialInternal * IMaterialInternal::GetRealTimeVersion()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->GetRealTimeVersion();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->GetRealTimeVersion();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->GetRealTimeVersion();
	}
	std::unreachable();
}
IMaterialInternal * IMaterialInternal::GetQueueFriendlyVersion()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->GetQueueFriendlyVersion();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->GetQueueFriendlyVersion();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->GetQueueFriendlyVersion();
	}
	std::unreachable();
}
void IMaterialInternal::PrecacheMappingDimensions()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->PrecacheMappingDimensions(); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->PrecacheMappingDimensions(); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->PrecacheMappingDimensions(); return;
	}
	std::unreachable();
}
void IMaterialInternal::FindRepresentativeTexture()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->FindRepresentativeTexture(); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->FindRepresentativeTexture(); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->FindRepresentativeTexture(); return;
	}
	std::unreachable();
}
void IMaterialInternal::DecideShouldReloadFromWhitelist( IFileList *pFileList )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->DecideShouldReloadFromWhitelist( pFileList ); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->DecideShouldReloadFromWhitelist( pFileList ); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->DecideShouldReloadFromWhitelist( pFileList ); return;
	}
	std::unreachable();
}
void IMaterialInternal::ReloadFromWhitelistIfMarked()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->ReloadFromWhitelistIfMarked(); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->ReloadFromWhitelistIfMarked(); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->ReloadFromWhitelistIfMarked(); return;
	}
	std::unreachable();
}
void IMaterialInternal::CompactMaterialVars()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->CompactMaterialVars(); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->CompactMaterialVars(); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->CompactMaterialVars(); return;
	}
	std::unreachable();
}
bool IMaterialInternal::HasQueueFriendlyProxies() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->HasQueueFriendlyProxies();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->HasQueueFriendlyProxies();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->HasQueueFriendlyProxies();
	}
	std::unreachable();
}
