#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "imaterialinternal.h"
import ks.core.types;
import ks.core.vprof;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.tier1.convar;
import ks.tier1.filename_table;
import ks.tier1.keyvalues;
import ks.vstdlib.callqueue;

class CMaterial_QueueFriendly : public IMaterialInternal //wraps a CMaterial with queue friendly functions for game/engine code. materialsystem/shaderapi code should use CMaterial directly.
{
	friend class IMaterial;
	friend class IMaterialInternal;
public:
	CMaterial_QueueFriendly() : IMaterialInternal( MaterialKind::QueueFriendly ) {}

	const char *	GetName() const;
	const char *	GetTextureGroupName() const;
	PreviewImageRetVal_t GetPreviewImageProperties( int *width, int *height, ImageFormat *imageFormat, bool* isTranslucent ) const;
	PreviewImageRetVal_t GetPreviewImage( unsigned char *data, int width, int height, ImageFormat imageFormat ) const;

	int				GetMappingWidth( );
	int				GetMappingHeight( );
	int				GetNumAnimationFrames( );
	bool			InMaterialPage( void );
	void			GetMaterialOffset( float *pOffset );
	void			GetMaterialScale( float *pScale );
	IMaterial		*GetMaterialPage( void );
	void			IncrementReferenceCount( void );
	int 			GetEnumerationID( void ) const;
	bool			HasProxy( void ) const;
	void			GetReflectivity( Vector& reflect );
	bool			GetPropertyFlag( MaterialPropertyTypes_t type );
	bool			IsTwoSided();
	int				ShaderParamCount() const;
	bool			IsErrorMaterial() const; //should probably return the realtime error material instead of this wrapper for it
	bool			IsSpriteCard(); //lets just assume nobody changes the shader to spritecard and immediately asks if it's a spritecard



	//TODO: Investigate if these are likely to change at all when setting vars/flags
	bool			IsAlphaTested();
	bool			IsVertexLit();
	VertexFormat_t	GetVertexFormat() const;
	bool			UsesEnvCubemap( void );
	bool			NeedsTangentSpace( void );
	bool			NeedsSoftwareSkinning( void );
	int				GetNumPasses( void );
	int				GetTextureMemoryBytes( void );
	bool			NeedsLightmapBlendAlpha( void );
	bool			NeedsSoftwareLighting( void );

	//TODO: Investigate if this can change over the course of a frame.
	void			GetLowResColorSample( float s, float t, float *color ) const;


	//Functions that need to be queue friendly, the whole reason for this wrapper class.
	IMaterialVar *	FindVar( const char *varName, bool *found, bool complain = true );
	IMaterialVar *	FindVarFast( char const *pVarName, unsigned int *pToken );
	IMaterialVar	**GetShaderParams( void );
	void			DecrementReferenceCount( void );
	void			DeleteIfUnreferenced();	
	void			RecomputeStateSnapshots();
	bool			IsTranslucent();
	bool			IsTranslucentUnderModulation( float fAlphaModulation ) const;
	bool			NeedsPowerOfTwoFrameBufferTexture( bool bCheckSpecificToThisFrame = true );
	bool			NeedsFullFrameBufferTexture( bool bCheckSpecificToThisFrame = true );
	void			AlphaModulate( float alpha );
	void			ColorModulate( float r, float g, float b );
	void			SetMaterialVarFlag( MaterialVarFlags_t flag, bool on );
	bool			GetMaterialVarFlag( MaterialVarFlags_t flag ) const;
	void			SetShader( const char *pShaderName );
	void			SetShaderAndParams( KeyValues *pKeyValues );
	const char *	GetShaderName() const;
	void			Refresh();
	void			RefreshPreservingMaterialVars();
	float			GetAlphaModulation();
	void			GetColorModulation( float *r, float *g, float *b );
	void			CallBindProxy( void *proxyData, ICallQueue *pCallQueue );
	void			PrecacheMappingDimensions( );
	void			FindRepresentativeTexture( void );
	bool			WasReloadedFromWhitelist() { return m_pRealTimeVersion->WasReloadedFromWhitelist(); }



#define QUEUEFRIENDLY_USED_INTERNALLY_ASSERT contract_assert( false && "CMaterial_QueueFriendly used internally within materialsystem. Update the calling code to use a realtime CMaterial." )

	//------------------------------------------------------------------------------
	// IMaterialInternal interfaces. Internal systems should not be using this queue
	// wrapper class at all. Switch to the real time pointer in the calling code.
	//------------------------------------------------------------------------------
	int		GetReferenceCount( ) const { return m_pRealTimeVersion->GetReferenceCount(); }
	void	SetEnumerationID( int id ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->SetEnumerationID( id ); }
	void	SetNeedsWhiteLightmap( bool val ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->SetNeedsWhiteLightmap( val ); }
	bool	GetNeedsWhiteLightmap( ) const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->GetNeedsWhiteLightmap(); }
	void	Uncache( bool bPreserveVars = false  ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->Uncache( bPreserveVars ); }
	void	Precache() { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->Precache(); }
	// If provided, pKeyValues and pPatchKeyValues should come from LoadVMTFile()
	bool	PrecacheVars( KeyValues *pKeyValues = nullptr, KeyValues *pPatchKeyValues = nullptr, std::vector< FileNameHandle_t > *pIncludes = nullptr ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->PrecacheVars( pKeyValues, pPatchKeyValues, pIncludes ); }
	void	ReloadTextures() { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->ReloadTextures(); }
	void	SetMinLightmapPageID( int pageID ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->SetMinLightmapPageID( pageID ); }
	void	SetMaxLightmapPageID( int pageID ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->SetMaxLightmapPageID( pageID ); }
	int		GetMinLightmapPageID( ) const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->GetMinLightmapPageID(); }
	int		GetMaxLightmapPageID( ) const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->GetMaxLightmapPageID(); }
	IShader *GetShader() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->GetShader(); }
	bool	IsPrecached( ) const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->IsPrecached(); }
	bool	IsPrecachedVars() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->IsPrecachedVars(); }
	void	DrawMesh( VertexCompressionType_t vertexCompression, bool bIsAlphaModulating, bool bRenderingPreTessPatchMesh ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->DrawMesh( vertexCompression, bIsAlphaModulating, bRenderingPreTessPatchMesh ); }
	VertexFormat_t GetVertexUsage() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->GetVertexUsage(); }
	bool PerformDebugTrace() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->PerformDebugTrace(); }
	bool NoDebugOverride() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->NoDebugOverride(); }
	void ToggleSuppression() { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->ToggleSuppression(); }
	bool IsSuppressed() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->IsSuppressed(); }
	void ToggleDebugTrace() { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->ToggleDebugTrace(); }
	bool UseFog() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->UseFog(); }
	void AddMaterialVar( IMaterialVar *pMaterialVar ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->AddMaterialVar( pMaterialVar ); }
	ShaderRenderState_t *GetRenderState() { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->GetRenderState(); }
	bool IsManuallyCreated() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->IsManuallyCreated(); }
	bool NeedsFixedFunctionFlashlight() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->NeedsFixedFunctionFlashlight(); }
	bool IsUsingVertexID() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->IsUsingVertexID(); }
	void MarkAsPreloaded( bool bSet ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->MarkAsPreloaded( bSet ); }
	bool IsPreloaded() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->IsPreloaded(); }
	void ArtificialAddRef() { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->ArtificialAddRef(); }
	void ArtificialRelease() { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->ArtificialRelease(); }
	void ReportVarChanged( IMaterialVar *pVar ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->ReportVarChanged( pVar ); }
	uint32 GetChangeID() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->GetChangeID(); }
	bool IsTranslucentInternal( float fAlphaModulation ) const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->IsTranslucentInternal( fAlphaModulation ); }

	void DecideShouldReloadFromWhitelist( IFileList *pFileList ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->DecideShouldReloadFromWhitelist( pFileList ); }
	void ReloadFromWhitelistIfMarked() { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->ReloadFromWhitelistIfMarked(); }

	bool IsRealTimeVersion( void ) const;
	IMaterialInternal *GetRealTimeVersion( void );
	IMaterialInternal *GetQueueFriendlyVersion( void );

	void SetRealTimeVersion( IMaterialInternal *pRealTimeVersion ) { m_pRealTimeVersion = pRealTimeVersion; }
	void UpdateToRealTime( void ); //update cached off variables using the real time version as a base.

	void CompactMaterialVars() {}

	// The material system should be asking, and it should be working with the realtime version only.
	bool HasQueueFriendlyProxies() const { contract_assert( !"Unexpected call to HasQueueFriendlyProxies" ); return false; }

	bool SetTempExcluded( bool bSet, int nExcludedDimensionLimit ); 

protected:
	IMaterialInternal *m_pRealTimeVersion; //the material we're wrapping with queued delays

private:	
	//some calls need to know what state the material would be in right now if the queue had completed.
	float m_fAlphaModulationOnQueueCompletion;
	Vector m_vColorModulationOnQueueCompletion;
};
