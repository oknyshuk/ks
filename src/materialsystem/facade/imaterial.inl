import ks.core.types;
import ks.core.vprof;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.tier1.convar;
import ks.tier1.keyvalues;
import ks.vstdlib.callqueue;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMaterial class to the material implementations, selected by the kind
// the material was created with. Included by imaterialdispatch.cpp.

const char * IMaterial::GetName() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->GetName();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->GetName();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->GetName();
	}
	std::unreachable();
}
const char * IMaterial::GetTextureGroupName() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->GetTextureGroupName();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->GetTextureGroupName();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->GetTextureGroupName();
	}
	std::unreachable();
}
PreviewImageRetVal_t IMaterial::GetPreviewImageProperties( int *width, int *height, ImageFormat *imageFormat, bool* isTranslucent ) const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->GetPreviewImageProperties( width, height, imageFormat, isTranslucent );
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->GetPreviewImageProperties( width, height, imageFormat, isTranslucent );
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->GetPreviewImageProperties( width, height, imageFormat, isTranslucent );
	}
	std::unreachable();
}
PreviewImageRetVal_t IMaterial::GetPreviewImage( unsigned char *data, int width, int height, ImageFormat imageFormat ) const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->GetPreviewImage( data, width, height, imageFormat );
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->GetPreviewImage( data, width, height, imageFormat );
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->GetPreviewImage( data, width, height, imageFormat );
	}
	std::unreachable();
}
int IMaterial::GetMappingWidth()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->GetMappingWidth();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->GetMappingWidth();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->GetMappingWidth();
	}
	std::unreachable();
}
int IMaterial::GetMappingHeight()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->GetMappingHeight();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->GetMappingHeight();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->GetMappingHeight();
	}
	std::unreachable();
}
int IMaterial::GetNumAnimationFrames()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->GetNumAnimationFrames();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->GetNumAnimationFrames();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->GetNumAnimationFrames();
	}
	std::unreachable();
}
bool IMaterial::InMaterialPage()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->InMaterialPage();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->InMaterialPage();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->InMaterialPage();
	}
	std::unreachable();
}
void IMaterial::GetMaterialOffset( float *pOffset )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->GetMaterialOffset( pOffset ); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->GetMaterialOffset( pOffset ); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->GetMaterialOffset( pOffset ); return;
	}
	std::unreachable();
}
void IMaterial::GetMaterialScale( float *pScale )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->GetMaterialScale( pScale ); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->GetMaterialScale( pScale ); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->GetMaterialScale( pScale ); return;
	}
	std::unreachable();
}
IMaterial * IMaterial::GetMaterialPage()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->GetMaterialPage();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->GetMaterialPage();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->GetMaterialPage();
	}
	std::unreachable();
}
IMaterialVar * IMaterial::FindVar( const char *varName, bool *found, bool complain )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->FindVar( varName, found, complain );
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->FindVar( varName, found, complain );
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->FindVar( varName, found, complain );
	}
	std::unreachable();
}
void IMaterial::IncrementReferenceCount()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->IncrementReferenceCount(); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->IncrementReferenceCount(); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->IncrementReferenceCount(); return;
	}
	std::unreachable();
}
void IMaterial::DecrementReferenceCount()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->DecrementReferenceCount(); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->DecrementReferenceCount(); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->DecrementReferenceCount(); return;
	}
	std::unreachable();
}
int IMaterial::GetEnumerationID() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->GetEnumerationID();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->GetEnumerationID();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->GetEnumerationID();
	}
	std::unreachable();
}
void IMaterial::GetLowResColorSample( float s, float t, float *color ) const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<const CMaterial *>( this )->GetLowResColorSample( s, t, color ); return;
	case MaterialKind::SubRect: static_cast<const CMaterialSubRect *>( this )->GetLowResColorSample( s, t, color ); return;
	case MaterialKind::QueueFriendly: static_cast<const CMaterial_QueueFriendly *>( this )->GetLowResColorSample( s, t, color ); return;
	}
	std::unreachable();
}
void IMaterial::RecomputeStateSnapshots()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->RecomputeStateSnapshots(); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->RecomputeStateSnapshots(); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->RecomputeStateSnapshots(); return;
	}
	std::unreachable();
}
bool IMaterial::IsTranslucent()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->IsTranslucent();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->IsTranslucent();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->IsTranslucent();
	}
	std::unreachable();
}
bool IMaterial::IsAlphaTested()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->IsAlphaTested();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->IsAlphaTested();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->IsAlphaTested();
	}
	std::unreachable();
}
bool IMaterial::IsVertexLit()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->IsVertexLit();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->IsVertexLit();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->IsVertexLit();
	}
	std::unreachable();
}
VertexFormat_t IMaterial::GetVertexFormat() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->GetVertexFormat();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->GetVertexFormat();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->GetVertexFormat();
	}
	std::unreachable();
}
bool IMaterial::HasProxy() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->HasProxy();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->HasProxy();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->HasProxy();
	}
	std::unreachable();
}
bool IMaterial::UsesEnvCubemap()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->UsesEnvCubemap();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->UsesEnvCubemap();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->UsesEnvCubemap();
	}
	std::unreachable();
}
bool IMaterial::NeedsTangentSpace()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->NeedsTangentSpace();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->NeedsTangentSpace();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->NeedsTangentSpace();
	}
	std::unreachable();
}
bool IMaterial::NeedsPowerOfTwoFrameBufferTexture( bool bCheckSpecificToThisFrame )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->NeedsPowerOfTwoFrameBufferTexture( bCheckSpecificToThisFrame );
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->NeedsPowerOfTwoFrameBufferTexture( bCheckSpecificToThisFrame );
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->NeedsPowerOfTwoFrameBufferTexture( bCheckSpecificToThisFrame );
	}
	std::unreachable();
}
bool IMaterial::NeedsFullFrameBufferTexture( bool bCheckSpecificToThisFrame )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->NeedsFullFrameBufferTexture( bCheckSpecificToThisFrame );
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->NeedsFullFrameBufferTexture( bCheckSpecificToThisFrame );
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->NeedsFullFrameBufferTexture( bCheckSpecificToThisFrame );
	}
	std::unreachable();
}
bool IMaterial::NeedsSoftwareSkinning()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->NeedsSoftwareSkinning();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->NeedsSoftwareSkinning();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->NeedsSoftwareSkinning();
	}
	std::unreachable();
}
void IMaterial::AlphaModulate( float alpha )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->AlphaModulate( alpha ); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->AlphaModulate( alpha ); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->AlphaModulate( alpha ); return;
	}
	std::unreachable();
}
void IMaterial::ColorModulate( float r, float g, float b )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->ColorModulate( r, g, b ); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->ColorModulate( r, g, b ); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->ColorModulate( r, g, b ); return;
	}
	std::unreachable();
}
void IMaterial::SetMaterialVarFlag( MaterialVarFlags_t flag, bool on )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->SetMaterialVarFlag( flag, on ); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->SetMaterialVarFlag( flag, on ); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->SetMaterialVarFlag( flag, on ); return;
	}
	std::unreachable();
}
bool IMaterial::GetMaterialVarFlag( MaterialVarFlags_t flag ) const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->GetMaterialVarFlag( flag );
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->GetMaterialVarFlag( flag );
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->GetMaterialVarFlag( flag );
	}
	std::unreachable();
}
void IMaterial::GetReflectivity( Vector& reflect )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->GetReflectivity( reflect ); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->GetReflectivity( reflect ); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->GetReflectivity( reflect ); return;
	}
	std::unreachable();
}
bool IMaterial::GetPropertyFlag( MaterialPropertyTypes_t type )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->GetPropertyFlag( type );
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->GetPropertyFlag( type );
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->GetPropertyFlag( type );
	}
	std::unreachable();
}
bool IMaterial::IsTwoSided()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->IsTwoSided();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->IsTwoSided();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->IsTwoSided();
	}
	std::unreachable();
}
void IMaterial::SetShader( const char *pShaderName )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->SetShader( pShaderName ); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->SetShader( pShaderName ); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->SetShader( pShaderName ); return;
	}
	std::unreachable();
}
int IMaterial::GetNumPasses()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->GetNumPasses();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->GetNumPasses();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->GetNumPasses();
	}
	std::unreachable();
}
int IMaterial::GetTextureMemoryBytes()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->GetTextureMemoryBytes();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->GetTextureMemoryBytes();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->GetTextureMemoryBytes();
	}
	std::unreachable();
}
void IMaterial::Refresh()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->Refresh(); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->Refresh(); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->Refresh(); return;
	}
	std::unreachable();
}
bool IMaterial::NeedsLightmapBlendAlpha()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->NeedsLightmapBlendAlpha();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->NeedsLightmapBlendAlpha();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->NeedsLightmapBlendAlpha();
	}
	std::unreachable();
}
bool IMaterial::NeedsSoftwareLighting()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->NeedsSoftwareLighting();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->NeedsSoftwareLighting();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->NeedsSoftwareLighting();
	}
	std::unreachable();
}
int IMaterial::ShaderParamCount() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->ShaderParamCount();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->ShaderParamCount();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->ShaderParamCount();
	}
	std::unreachable();
}
IMaterialVar ** IMaterial::GetShaderParams()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->GetShaderParams();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->GetShaderParams();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->GetShaderParams();
	}
	std::unreachable();
}
bool IMaterial::IsErrorMaterial() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->IsErrorMaterial();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->IsErrorMaterial();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->IsErrorMaterial();
	}
	std::unreachable();
}
float IMaterial::GetAlphaModulation()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->GetAlphaModulation();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->GetAlphaModulation();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->GetAlphaModulation();
	}
	std::unreachable();
}
void IMaterial::GetColorModulation( float *r, float *g, float *b )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->GetColorModulation( r, g, b ); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->GetColorModulation( r, g, b ); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->GetColorModulation( r, g, b ); return;
	}
	std::unreachable();
}
bool IMaterial::IsTranslucentUnderModulation( float fAlphaModulation ) const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->IsTranslucentUnderModulation( fAlphaModulation );
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->IsTranslucentUnderModulation( fAlphaModulation );
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->IsTranslucentUnderModulation( fAlphaModulation );
	}
	std::unreachable();
}
IMaterialVar * IMaterial::FindVarFast( char const *pVarName, unsigned int *pToken )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->FindVarFast( pVarName, pToken );
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->FindVarFast( pVarName, pToken );
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->FindVarFast( pVarName, pToken );
	}
	std::unreachable();
}
void IMaterial::SetShaderAndParams( KeyValues *pKeyValues )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->SetShaderAndParams( pKeyValues ); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->SetShaderAndParams( pKeyValues ); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->SetShaderAndParams( pKeyValues ); return;
	}
	std::unreachable();
}
const char * IMaterial::GetShaderName() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->GetShaderName();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->GetShaderName();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->GetShaderName();
	}
	std::unreachable();
}
void IMaterial::DeleteIfUnreferenced()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->DeleteIfUnreferenced(); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->DeleteIfUnreferenced(); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->DeleteIfUnreferenced(); return;
	}
	std::unreachable();
}
bool IMaterial::IsSpriteCard()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->IsSpriteCard();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->IsSpriteCard();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->IsSpriteCard();
	}
	std::unreachable();
}
void IMaterial::CallBindProxy( void *proxyData, ICallQueue *pCallQueue )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->CallBindProxy( proxyData, pCallQueue ); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->CallBindProxy( proxyData, pCallQueue ); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->CallBindProxy( proxyData, pCallQueue ); return;
	}
	std::unreachable();
}
void IMaterial::RefreshPreservingMaterialVars()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: static_cast<CMaterial *>( this )->RefreshPreservingMaterialVars(); return;
	case MaterialKind::SubRect: static_cast<CMaterialSubRect *>( this )->RefreshPreservingMaterialVars(); return;
	case MaterialKind::QueueFriendly: static_cast<CMaterial_QueueFriendly *>( this )->RefreshPreservingMaterialVars(); return;
	}
	std::unreachable();
}
bool IMaterial::WasReloadedFromWhitelist()
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->WasReloadedFromWhitelist();
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->WasReloadedFromWhitelist();
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->WasReloadedFromWhitelist();
	}
	std::unreachable();
}
bool IMaterial::SetTempExcluded( bool bSet, int nExcludedDimensionLimit )
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<CMaterial *>( this )->SetTempExcluded( bSet, nExcludedDimensionLimit );
	case MaterialKind::SubRect: return static_cast<CMaterialSubRect *>( this )->SetTempExcluded( bSet, nExcludedDimensionLimit );
	case MaterialKind::QueueFriendly: return static_cast<CMaterial_QueueFriendly *>( this )->SetTempExcluded( bSet, nExcludedDimensionLimit );
	}
	std::unreachable();
}
int IMaterial::GetReferenceCount() const
{
	switch ( static_cast<const IMaterialInternal *>( this )->Kind() )
	{
	case MaterialKind::Standard: return static_cast<const CMaterial *>( this )->GetReferenceCount();
	case MaterialKind::SubRect: return static_cast<const CMaterialSubRect *>( this )->GetReferenceCount();
	case MaterialKind::QueueFriendly: return static_cast<const CMaterial_QueueFriendly *>( this )->GetReferenceCount();
	}
	std::unreachable();
}
