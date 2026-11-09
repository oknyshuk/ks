#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.vstdlib;
import ks.tier1;
#include "imaterialinternal.h"
import std;
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "materialsystem_global.h"
#include "shaderapi/ishaderapi.h"
#include "shadersystem.h"
#include "itextureinternal.h"
#include "cmaterial_queuefriendly.h"

//-----------------------------------------------------------------------------
// Material SubRect implementation
//-----------------------------------------------------------------------------
class CMaterialSubRect : public IMaterialInternal, public ks::pooled< CMaterialSubRect >
{
	friend class IMaterial;
	friend class IMaterialInternal;
public:

	// pVMTKeyValues and pPatchKeyValues should come from LoadVMTFile()
						CMaterialSubRect( char const *pMaterialName, const char *pTextureGroupName, KeyValues *pVMTKeyValues, KeyValues *pPatchKeyValues, bool bAssumeCreateFromFile );
	~CMaterialSubRect();

	// IMaterial Interface
	const char 			*GetName() const;
	const char			*GetTextureGroupName() const;

	int					GetMappingWidth();
	int					GetMappingHeight();

	bool				InMaterialPage( void )									{ return true; }
	void				GetMaterialOffset( float *pOffset );
	void				GetMaterialScale( float *pScale );
	IMaterial			*GetMaterialPage( void )								{ return m_pMaterialPage; }

	void				IncrementReferenceCount( void );
	void				DecrementReferenceCount( void );

	IMaterialVar		*FindVar( char const *varName, bool *found, bool complain = true );
	IMaterialVar		*FindVarFast( char const *pVarName, unsigned int *pToken );

	// Sets new VMT shader parameters for the material
	void		SetShaderAndParams( KeyValues *pKeyValues );

	int 				GetEnumerationID() const;

	// Maybe!
	void				GetReflectivity( Vector& reflect )				{ m_pMaterialPage->GetReflectivity( reflect ); }


	// IMaterialInternal Interface
	int					GetReferenceCount( void ) const;
	void				Precache();
	void				Uncache( bool bPreserveVars = false );
	// If provided, pKeyValues and pPatchKeyValues should come from LoadVMTFile()
	bool				PrecacheVars( KeyValues *pKeyValues = nullptr, KeyValues *pPatchKeyValues = nullptr, std::vector< FileNameHandle_t > *pIncludes = nullptr );
	bool				IsPrecached() const;
	bool				IsPrecachedVars( ) const;
	bool				IsManuallyCreated() const;
	void				SetEnumerationID( int id );
	void				AddMaterialVar( IMaterialVar *pMaterialVar );
	void				MarkAsPreloaded( bool bSet );
	bool				IsPreloaded() const;
	void				ArtificialAddRef();
	void				ArtificialRelease();

	//=============================
	// Chained to the material page.
	//=============================	
	// IMaterial Interface.
	PreviewImageRetVal_t GetPreviewImageProperties( int *width, int *height, ImageFormat *imageFormat, bool* isTranslucent ) const
		{ return m_pMaterialPage->GetPreviewImageProperties( width, height, imageFormat, isTranslucent ); } 
	PreviewImageRetVal_t GetPreviewImage( unsigned char *data, int width, int height, ImageFormat imageFormat ) const
		{ return m_pMaterialPage->GetPreviewImage( data, width, height, imageFormat ); }

	ShaderRenderState_t *GetRenderState()								{ return m_pMaterialPage->GetRenderState(); }
	int					GetNumAnimationFrames()							{ return m_pMaterialPage->GetNumAnimationFrames(); }

	void				GetLowResColorSample( float s, float t, float *color ) const { m_pMaterialPage->GetLowResColorSample( s, t, color ); }
	
	bool				UsesEnvCubemap( void )							{ return m_pMaterialPage->UsesEnvCubemap(); }
	bool				NeedsSoftwareSkinning( void )					{ return m_pMaterialPage->NeedsSoftwareSkinning(); }
	bool				NeedsSoftwareLighting( void )					{ return m_pMaterialPage->NeedsSoftwareLighting(); }
	bool				NeedsTangentSpace( void )						{ return m_pMaterialPage->NeedsTangentSpace(); }
	bool				NeedsPowerOfTwoFrameBufferTexture( bool bCheckSpecificToThisFrame = true )	{ return m_pMaterialPage->NeedsPowerOfTwoFrameBufferTexture( bCheckSpecificToThisFrame ); }
	bool				NeedsFullFrameBufferTexture( bool bCheckSpecificToThisFrame = true )		{ return m_pMaterialPage->NeedsFullFrameBufferTexture( bCheckSpecificToThisFrame ); }
	bool				NeedsLightmapBlendAlpha( void )					{ return m_pMaterialPage->NeedsLightmapBlendAlpha(); }
	
	void				AlphaModulate( float alpha )					{ m_pMaterialPage->AlphaModulate( alpha ); }
	void				ColorModulate( float r, float g, float b )		{ m_pMaterialPage->ColorModulate( r, g, b ); }
	float				GetAlphaModulation( )							{ return m_pMaterialPage->GetAlphaModulation( ); }
	void				GetColorModulation( float *r, float *g, float *b )		{ m_pMaterialPage->GetColorModulation( r, g, b ); }

	void				SetMaterialVarFlag( MaterialVarFlags_t flag, bool on )	{ m_pMaterialPage->SetMaterialVarFlag( flag, on ); }
	bool				GetMaterialVarFlag( MaterialVarFlags_t flag ) const		{ return m_pMaterialPage->GetMaterialVarFlag( flag ); }

	bool				IsTranslucent()									{ return m_pMaterialPage->IsTranslucent(); }
	bool				IsTranslucentInternal( float fAlphaModulation ) const { return m_pMaterialPage->IsTranslucentInternal( fAlphaModulation ); }
	bool		IsTranslucentUnderModulation( float fAlphaModulation ) const { return m_pMaterialPage->IsTranslucentUnderModulation( fAlphaModulation ); }

	bool				IsAlphaTested()									{ return m_pMaterialPage->IsAlphaTested(); }
	bool				IsVertexLit()									{ return m_pMaterialPage->IsVertexLit(); }

	bool				GetPropertyFlag( MaterialPropertyTypes_t type ) { return m_pMaterialPage->GetPropertyFlag( type ); }

	bool				IsTwoSided()									{ return m_pMaterialPage->IsTwoSided(); }

	int					GetNumPasses( void )							{ return m_pMaterialPage->GetNumPasses(); }
	int					GetTextureMemoryBytes( void )					{ return m_pMaterialPage->GetTextureMemoryBytes(); }

	// IMaterialInternal Interface.
	void				DrawMesh( VertexCompressionType_t vertexCompression, bool bIsAlphaModulating, bool bRenderingPreTessPatchMesh )	{ m_pMaterialPage->DrawMesh( vertexCompression, bIsAlphaModulating, bRenderingPreTessPatchMesh ); }
	void				ReloadTextures( void )									{ m_pMaterialPage->ReloadTextures(); }
	void				SetMinLightmapPageID( int pageID )				
	{
		m_pMaterialPage->SetMinLightmapPageID( pageID );
	}

	void				SetMaxLightmapPageID( int pageID )				
	{ 
		m_pMaterialPage->SetMaxLightmapPageID( pageID ); 
	}

	int					GetMinLightmapPageID( ) const					{ return m_pMaterialPage->GetMinLightmapPageID(); }
	int					GetMaxLightmapPageID( ) const					{ return m_pMaterialPage->GetMaxLightmapPageID(); }
	
	void				SetNeedsWhiteLightmap( bool val )				
	{ 
		m_pMaterialPage->SetNeedsWhiteLightmap( val ); 
	}

	bool				GetNeedsWhiteLightmap( ) const					{ return m_pMaterialPage->GetNeedsWhiteLightmap(); }
	
	IShader *			GetShader() const								{ return m_pMaterialPage->GetShader(); }
	void				CallBindProxy( void *proxyData, ICallQueue *pCallQueue )	{ m_pMaterialPage->CallBindProxy( proxyData, pCallQueue ); }
	bool				HasProxy( void ) const							{ return m_pMaterialPage->HasProxy(); }

	// Sets the shader associated with the material
	void				SetShader( const char *pShaderName )			{ m_pMaterialPage->SetShader( pShaderName ); }
	const char *		GetShaderName() const 							{ return m_pMaterialPage->GetShaderName(); }

	void		DeleteIfUnreferenced();
	bool		IsSpriteCard()									{ return m_pMaterialPage->IsSpriteCard(); }

	// Can we override this material in debug?
	bool				NoDebugOverride() const							{ return m_pMaterialPage->NoDebugOverride(); }

	// Gets the vertex format
	VertexFormat_t		GetVertexFormat() const							{ return m_pMaterialPage->GetVertexFormat(); }

	// diffuse bump lightmap?
//	bool				IsUsingDiffuseBumpedLighting() const			{ return m_pChainMaterial->IsUsingDiffuseBumpedLighting(); }

	// lightmap?
//	bool				IsUsingLightmap() const							{ return m_pChainMaterial->IsUsingLightmap(); }

	// Gets the vertex usage flags
	VertexFormat_t		GetVertexUsage() const							{ return m_pMaterialPage->GetVertexUsage(); }

	// Debugs this material
	bool				PerformDebugTrace() const						{ return m_pMaterialPage->PerformDebugTrace(); }

	// Are we suppressed?
	bool				IsSuppressed() const							{ return m_pMaterialPage->IsSuppressed(); }

	// Do we use fog?
	bool				UseFog( void ) const							{ return m_pMaterialPage->UseFog(); }
	
	// Should we draw?
	void				ToggleSuppression()								{ m_pMaterialPage->ToggleSuppression(); }
	void				ToggleDebugTrace()								{ m_pMaterialPage->ToggleDebugTrace(); }
	
	// Refresh material based on current var values
	void				Refresh()										{ m_pMaterialPage->Refresh(); }
	void				RefreshPreservingMaterialVars()					{ m_pMaterialPage->RefreshPreservingMaterialVars(); }

	// This computes the state snapshots for this material
	void				RecomputeStateSnapshots()						{ m_pMaterialPage->RecomputeStateSnapshots(); }

	// Gets at the shader parameters
	int					ShaderParamCount() const						{ return m_pMaterialPage->ShaderParamCount(); }
	IMaterialVar		**GetShaderParams( void )						{ return m_pMaterialPage->GetShaderParams(); }

	bool				IsErrorMaterial() const							{ return false; }

	bool				NeedsFixedFunctionFlashlight() const			{ return m_pMaterialPage->NeedsFixedFunctionFlashlight(); }

	void		DecideShouldReloadFromWhitelist( IFileList *pFileList )		{ m_pMaterialPage->DecideShouldReloadFromWhitelist( pFileList ); }
	void		ReloadFromWhitelistIfMarked()								{ return m_pMaterialPage->ReloadFromWhitelistIfMarked(); }
	bool		WasReloadedFromWhitelist()								{ return m_pMaterialPage->WasReloadedFromWhitelist(); }

	bool				IsUsingVertexID( ) const						{ return m_pMaterialPage->IsUsingVertexID(); }

	void ReportVarChanged( IMaterialVar *pVar ) { m_pMaterialPage->ReportVarChanged(pVar); }
	uint32 GetChangeID() const { return m_pMaterialPage->GetChangeID(); }

	bool IsRealTimeVersion( void ) const { return true; }
	IMaterialInternal *GetRealTimeVersion( void ) { return this; }
	IMaterialInternal *GetQueueFriendlyVersion( void ) { return &m_QueueFriendlyVersion; };

	void PrecacheMappingDimensions( void ) { m_pMaterialPage->PrecacheMappingDimensions(); }
	void FindRepresentativeTexture( void ) { m_pMaterialPage->FindRepresentativeTexture(); }

	void CompactMaterialVars()									{ ::CompactMaterialVars( m_aMaterialVars.data(), std::ssize( m_aMaterialVars ) ); }

	bool HasQueueFriendlyProxies() const				{ return m_pMaterialPage->HasQueueFriendlyProxies(); }

	bool SetTempExcluded( bool bSet, int nExcludedDimensionLimit )	{ return false; }

private:

	void				ParseMaterialVars( KeyValues &keyValues );
	void				SetupMaterialVars( void );

	// Do we use a UNC-specified materal name?
	bool				UsesUNCFileName() const;

	IMaterialVar		*GetDummyMaterialVar();

private:

	enum
	{
		MATERIALSUBRECT_IS_PRECACHED = 0x1,
		MATERIALSUBRECT_VARS_IS_PRECACHED = 0x2,
		MATERIALSUBRECT_IS_MANUALLY_CREATED = 0x4,	
		MATERIALSUBRECT_USES_UNC_FILENAME = 0x20,
		MATERIALSUBRECT_IS_PRELOADED = 0x40,
		MATERIALSUBRECT_ARTIFICIAL_REFCOUNT = 0x80,
	};

	// Fixed-size allocator
	private:
		static ks::fixed_pool s_Allocator;
		friend class ks::pooled< CMaterialSubRect >;

	IMaterialInternal			*m_pMaterialPage;
	IMaterialInternal			*m_pModelMaterialPage;

	int							m_iEnumID;

	std::string_view			m_symName;					// both in ks::global_symbols()
	std::string_view			m_symTextureGroupName;

	Vector2D					m_vecOffset;
	Vector2D					m_vecScale;
	Vector2D					m_vecSize;

	short						m_nRefCount;

	unsigned int				m_fLocal;					// Local flags - precached etc...

	std::vector< IMaterialVar* >	m_aMaterialVars;

	// Used only by procedural materials; it essentially is an in-memory .VMT file
	KeyValues					*m_pVMTKeyValues;

#ifdef _DEBUG
	// Makes it easier to see what's going on
	char*						m_pDebugName;
#endif

	CMaterial_QueueFriendly		m_QueueFriendlyVersion;
};
