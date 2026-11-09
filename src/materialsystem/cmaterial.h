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
// Material implementation
//-----------------------------------------------------------------------------
class CMaterial : public IMaterialInternal, public ks::pooled< CMaterial >
{
	friend class IMaterial;
	friend class IMaterialInternal;
public:
	// Members of the IMaterial interface
	const char  *GetName() const;
	IMaterialVar** GetVars() { return m_pShaderParams; }
	const char  *GetTextureGroupName() const;
	inline bool	IsPrecached_Inline( ) const
	{
		return (m_Flags & MATERIAL_IS_PRECACHED) != 0;
	}
	bool	IsPrecached( ) const { return IsPrecached_Inline(); }



	inline bool	IsPrecachedVars_Inline( ) const
	{
		return (m_Flags & MATERIAL_VARS_IS_PRECACHED) != 0;
	}
	bool	IsPrecachedVars( ) const { return IsPrecachedVars_Inline(); }


	PreviewImageRetVal_t GetPreviewImageProperties( int *width, int *height, 
				 				ImageFormat *imageFormat, bool* isTranslucent ) const;
	PreviewImageRetVal_t GetPreviewImage( unsigned char *data, int width, int height,
								ImageFormat imageFormat ) const;

	int			GetMappingWidth( );
	int			GetMappingHeight( );
	int			GetNumAnimationFrames( );

	bool		InMaterialPage( void )						{ return false; }
	void		GetMaterialOffset( float *pOffset );
	void		GetMaterialScale( float *pOffset );
	IMaterial	*GetMaterialPage( void )					{ return nullptr; }

	void		IncrementReferenceCount( );
	void		DecrementReferenceCount( );
	int 		GetEnumerationID( ) const;
	void		GetLowResColorSample( float s, float t, float *color ) const;
	
	IMaterialVar *		FindVar( char const *varName, bool *found, bool complain = true );
	IMaterialVar *		FindVarFast( char const *pVarName, unsigned int *pToken );

	// Sets new VMT shader parameters for the material
	void		SetShaderAndParams( KeyValues *pKeyValues );

	bool				UsesEnvCubemap( void );
	bool				NeedsSoftwareSkinning( void );
	bool		NeedsSoftwareLighting( void );
	bool				NeedsTangentSpace( void );
	bool				NeedsPowerOfTwoFrameBufferTexture( bool bCheckSpecificToThisFrame = true );
	bool				NeedsFullFrameBufferTexture( bool bCheckSpecificToThisFrame = true );
	bool		IsUsingVertexID( ) const;

	// GR - Is lightmap alpha needed?
	bool	NeedsLightmapBlendAlpha( void );
	
	void	AlphaModulate( float alpha );
	void	ColorModulate( float r, float g, float b );
	float	GetAlphaModulation();
	void	GetColorModulation( float *r, float *g, float *b );

	void	SetMaterialVarFlag( MaterialVarFlags_t flag, bool on );
	bool	GetMaterialVarFlag( MaterialVarFlags_t flag ) const;

	bool	IsTranslucent();
	bool	IsTranslucentInternal( float fAlphaModulation ) const; //need to centralize the logic without relying on the *current* alpha modulation being that which is stored in m_pShaderParams[ALPHA].
	bool	IsTranslucentUnderModulation( float fAlphaModulation ) const;
	bool	IsAlphaTested();
	bool	IsVertexLit();
	bool IsSpriteCard();

	void	GetReflectivity( Vector& reflect );
	bool	GetPropertyFlag( MaterialPropertyTypes_t type );

	// Is the material visible from both sides?
	bool	IsTwoSided();

	int		GetNumPasses( void );
	int		GetTextureMemoryBytes( void );

public:
	// stuff that is visible only from within the material system

	// constructor, destructor
	CMaterial( char const* materialName, const char *pTextureGroupName, KeyValues *pVMTKeyValues );
	~CMaterial();

	CBasePerMaterialContextData **GetContextData( int modulationFlags )
	{
		return &(m_ShaderRenderState.m_pSnapshots[modulationFlags].m_pContextData[0]);
	}

	StateSnapshot_t	GetSnapshotId( int modulation, int renderPass );
	unsigned char* GetInstanceCommandBuffer( int modulation );
	void		DrawMesh( VertexCompressionType_t vertexCompression, bool bIsAlphaModulating, bool bRenderingPreTessPatchMesh );
	void		Uncache( bool bPreserveVars = false );
	void		ReloadTextures( void );
private:
	void		Precache_Internal();
	bool		PrecacheVars_Internal( KeyValues *pKeyValues, KeyValues *pPatchKeyValues, std::vector< FileNameHandle_t > *pIncludes );
public:
	// If provided, pKeyValues and pPatchKeyValues should come from LoadVMTFile()
	inline bool	PrecacheVars_Inline( KeyValues *pKeyValues = nullptr, KeyValues *pPatchKeyValues = nullptr, std::vector< FileNameHandle_t > *pIncludes = nullptr )
	{
		// We should get both parameters or neither
		contract_assert( !!pKeyValues == !!pPatchKeyValues );

		// Don't bother if we're already precached
		if( IsPrecachedVars_Inline() )
			return true;

		return PrecacheVars_Internal( pKeyValues, pPatchKeyValues, pIncludes );
	}
	inline void	Precache_Inline()
	{
		// Don't bother if we're already precached
		if ( IsPrecached_Inline() )
			return;

		// load data from the vmt file
		if ( !PrecacheVars_Inline() )
			return;

		Precache_Internal();
	}
	void	Precache()
	{
		Precache_Inline();
	}

	bool PrecacheVars( KeyValues *pKeyValues = nullptr, KeyValues *pPatchKeyValues = nullptr, std::vector< FileNameHandle_t > *pIncludes = nullptr )
	{
		return PrecacheVars_Inline( pKeyValues, pPatchKeyValues, pIncludes );
	}

	void		SetMinLightmapPageID( int pageID );
	void		SetMaxLightmapPageID( int pageID );
	int			GetMinLightmapPageID( ) const;
	int			GetMaxLightmapPageID( ) const;
	void		SetNeedsWhiteLightmap( bool val );
	bool		GetNeedsWhiteLightmap( ) const;
	IShader *	GetShader() const;
	const char *GetShaderName() const;
	
	void			DeleteIfUnreferenced();

	void		SetEnumerationID( int id );
	void		CallBindProxy( void *proxyData, ICallQueue *pCallQueue );
	bool		HasProxy( void ) const;

	// Sets the shader associated with the material
	void SetShader( const char *pShaderName );

	// Can we override this material in debug?
	bool NoDebugOverride() const;

	// Gets the vertex format
	VertexFormat_t GetVertexFormat() const;
	
	// diffuse bump lightmap?
	bool IsUsingDiffuseBumpedLighting() const;

	// lightmap?
	bool IsUsingLightmap() const;

	// Gets the vertex usage flags
	VertexFormat_t GetVertexUsage() const;

	// Debugs this material
	bool PerformDebugTrace() const;

	// Are we suppressed?
	bool IsSuppressed() const;

	// Do we use fog?
	bool UseFog( void ) const;
	
	// Should we draw?
	void ToggleSuppression();
	void ToggleDebugTrace();
	
	// Refresh material based on current var values
	void Refresh();
	void RefreshPreservingMaterialVars();

	// This computes the state snapshots for this material
	void RecomputeStateSnapshots();

	// Gets at the shader parameters
	int ShaderParamCount() const;
	IMaterialVar **GetShaderParams( void );

	void AddMaterialVar( IMaterialVar *pMaterialVar );

	bool IsErrorMaterial() const;

	// Was this manually created (not read from a file?)
	bool IsManuallyCreated() const;

	bool NeedsFixedFunctionFlashlight() const;

	void MarkAsPreloaded( bool bSet );
	bool IsPreloaded() const;

	void ArtificialAddRef( void );
	void ArtificialRelease( void );

	void ReportVarChanged( IMaterialVar *pVar )
	{	
		m_ChangeID++;
	}
	void ClearContextData( void );

	uint32 GetChangeID() const { return m_ChangeID; }
	uint32 GetChangeTimestamp() const { return m_ChangeID ^ g_nDebugVarsSignature; }

	bool IsRealTimeVersion( void ) const { return true; }
	IMaterialInternal *GetRealTimeVersion( void ) { return this; }
	IMaterialInternal *GetQueueFriendlyVersion( void ) { return &m_QueueFriendlyVersion; }

	void DecideShouldReloadFromWhitelist( IFileList *pFilesToReload );
	void ReloadFromWhitelistIfMarked();
	bool WasReloadedFromWhitelist();

	void CompactMaterialVars();

	// Are any of the proxies attached to this material callable from the queued thread?
	bool HasQueueFriendlyProxies() const;

	bool SetTempExcluded( bool bSet, int nExcludedDimensionLimit );

	int	GetReferenceCount() const;

private:
	// Initializes, cleans up the shader params
	void CleanUpShaderParams();

	// Sets up an error shader when we run into problems.
	void SetupErrorShader();

	// Does this material have a UNC-file name?
	bool UsesUNCFileName() const;

	// Prints material flags.
	void PrintMaterialFlags( int flags, int flagsDefined );

	// Parses material flags
	bool ParseMaterialFlag( KeyValues* pParseValue, IMaterialVar* pFlagVar,
		IMaterialVar* pFlagDefinedVar, bool parsingOverrides, int& flagMask, int& overrideMask );

	// Computes the material vars for the shader
	int ParseMaterialVars( IShader* pShader, KeyValues& keyValues, 
		KeyValues* pOverride, bool modelDefault, IMaterialVar** ppVars );

	// Figures out the preview image for worldcraft
	char const*	GetPreviewImageName( );
	char const* GetPreviewImageFileName( void ) const;

	// Hooks up the shader, returns keyvalues of fallback that was used
	KeyValues* InitializeShader( KeyValues &keyValues, KeyValues &patchKeyValues );

	// Finds the flag associated with a particular flag name
	int FindMaterialVarFlag( char const* pFlagName ) const;

	// Initializes, cleans up the state snapshots
	bool InitializeStateSnapshots();
	void CleanUpStateSnapshots();

	// Initializes, cleans up the material proxy
	void InitializeMaterialProxy( KeyValues* pFallbackKeyValues );
	void CleanUpMaterialProxy();

	// Creates, destroys snapshots
	RenderPassList_t *CreateRenderPassList();
	void DestroyRenderPassList( RenderPassList_t *pPassList );

	// Grabs the texture width and height from the var list for faster access
	void PrecacheMappingDimensions( );

	// Gets the renderstate
	ShaderRenderState_t *GetRenderState();

	// Do we have a valid renderstate?
	bool IsValidRenderState() const;

	// Get the material var flags
	int	GetMaterialVarFlags() const;
	void SetMaterialVarFlags( int flags, bool on );
	int	GetMaterialVarFlags2() const;
	void SetMaterialVarFlags2( int flags, bool on );

	// Returns a dummy material variable
	IMaterialVar*	GetDummyVariable();

	IMaterialVar*	GetShaderParam( int id );

	void			FindRepresentativeTexture( void );

	bool ShouldSkipVar( KeyValues *pMaterialVar, bool * pWasConditional );


	// Fixed-size allocator
	private:
		static ks::fixed_pool s_Allocator;
		friend class ks::pooled< CMaterial >;

private:
	enum
	{
		MATERIAL_NEEDS_WHITE_LIGHTMAP = 0x1,
		MATERIAL_IS_PRECACHED = 0x2,
		MATERIAL_VARS_IS_PRECACHED = 0x4,
		MATERIAL_VALID_RENDERSTATE = 0x8,
		MATERIAL_IS_MANUALLY_CREATED = 0x10,
		MATERIAL_USES_UNC_FILENAME = 0x20,
		MATERIAL_IS_PRELOADED = 0x40,
		MATERIAL_ARTIFICIAL_REFCOUNT = 0x80,
	};

	int					m_iEnumerationID;
	
	int					m_minLightmapPageID;
	int					m_maxLightmapPageID;

	unsigned short		m_MappingWidth;
	unsigned short		m_MappingHeight;
	
	IShader				*m_pShader;

	std::string_view	m_Name;					// both in ks::global_symbols()
	// Any textures created for this material go under this texture group.
	std::string_view	m_TextureGroupName;

	std::atomic<int>		m_RefCount;
	unsigned short		m_Flags;

	unsigned char		m_VarCount;
	unsigned char		m_ProxyCount;

	IMaterialVar**		m_pShaderParams;
	IMaterialProxy**	m_ppProxies;
	ShaderRenderState_t m_ShaderRenderState;

	// This remembers filenames of VMTs that we included so we can sv_pure/flush ourselves if any of them need to be reloaded.
	std::vector< FileNameHandle_t > m_VMTIncludes;
	bool m_bShouldReloadFromWhitelist;	// Tells us if the material decided it should be reloaded due to sv_pure whitelist changes.

	ITextureInternal	*m_representativeTexture;
	Vector				m_Reflectivity;
	uint32				m_ChangeID;

	// Used only by procedural materials; it essentially is an in-memory .VMT file
	KeyValues			*m_pVMTKeyValues;

#ifdef _DEBUG
	// Makes it easier to see what's going on
	char				*m_pDebugName;
#endif

protected:
	CMaterial_QueueFriendly m_QueueFriendlyVersion;
};
