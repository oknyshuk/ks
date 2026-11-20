#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation
import ks.core.basetypes;
import ks.core.types;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.math.vmatrix;
import ks.platform;
import ks.simd;

#include "shaderapi/shareddefs.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"

typedef intp ShaderAPITextureHandle_t;
inline constexpr auto INVALID_SHADERAPI_TEXTURE_HANDLE = 0;


//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class CMeshBuilder;
class IMaterialVar;


//-----------------------------------------------------------------------------
// State from ShaderAPI used to select proper vertex and pixel shader combos
//-----------------------------------------------------------------------------
struct LightState_t
{
	int  m_nNumLights;
	bool m_bAmbientLight;
	bool m_bStaticLight;
	bool m_bStaticLightIndirectOnly;  // only valid if m_bStaticLight true
	inline int HasDynamicLight() { return (m_bAmbientLight || (m_nNumLights > 0)) ? 1 : 0; }
};


//-----------------------------------------------------------------------------
// Color correction info
//-----------------------------------------------------------------------------
struct ShaderColorCorrectionInfo_t
{
	bool m_bIsEnabled;
	int m_nLookupCount;
	float m_flDefaultWeight;
	float m_pLookupWeights[4];
};


//-----------------------------------------------------------------------------
// the 3D shader API interface
// This interface is all that shaders see.
//-----------------------------------------------------------------------------
enum StandardTextureId_t
{
	// Lightmaps
	TEXTURE_LIGHTMAP = 0,
	TEXTURE_LIGHTMAP_FULLBRIGHT,
	TEXTURE_LIGHTMAP_BUMPED,
	TEXTURE_LIGHTMAP_BUMPED_FULLBRIGHT,

	// Flat colors
	TEXTURE_WHITE,
	TEXTURE_BLACK,
	TEXTURE_BLACK_ALPHA_ZERO,
	TEXTURE_GREY,
	TEXTURE_GREY_ALPHA_ZERO,

	// Normalmaps
	TEXTURE_NORMALMAP_FLAT,
	TEXTURE_SSBUMP_FLAT,

	// Normalization
	TEXTURE_NORMALIZATION_CUBEMAP,
	TEXTURE_NORMALIZATION_CUBEMAP_SIGNED,

	// Frame-buffer textures
	TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0,
	TEXTURE_FRAME_BUFFER_FULL_TEXTURE_1,

	// Color correction
	TEXTURE_COLOR_CORRECTION_VOLUME_0,
	TEXTURE_COLOR_CORRECTION_VOLUME_1,
	TEXTURE_COLOR_CORRECTION_VOLUME_2,
	TEXTURE_COLOR_CORRECTION_VOLUME_3,

	// An alias to the Back Frame Buffer
	TEXTURE_FRAME_BUFFER_ALIAS,

	// Noise for shadow mapping algorithm
	TEXTURE_SHADOW_NOISE_2D,

	// A texture in which morph data gets accumulated (vs30, fast vertex textures required)
	TEXTURE_MORPH_ACCUMULATOR,

	// A texture which contains morph weights
	TEXTURE_MORPH_WEIGHTS,

	// A snapshot of the frame buffer's depth. Currently only valid on the 360
	TEXTURE_FRAME_BUFFER_FULL_DEPTH,

	// A snapshot of the frame buffer's depth. Currently only valid on the 360
	TEXTURE_IDENTITY_LIGHTWARP,

	// The current local env_cubemap
	TEXTURE_LOCAL_ENV_CUBEMAP,

	// Texture containing subdivision surface patch data
	TEXTURE_SUBDIVISION_PATCHES,

	// Screen-space texture which contains random 3D reflection vectors used in SSAO algorithm
	TEXTURE_SSAO_NOISE_2D,

	TEXTURE_PAINT,

	// Stereo-ized texture that contains eye-dependent stereo constants.
	TEXTURE_STEREO_PARAM_MAP,
	
	TEXTURE_MAX_STD_TEXTURES
};


enum TextureFilterMode_t
{
	TFILTER_MODE_POINTSAMPLED = 1,
};

enum TessellationMode_t;

//-----------------------------------------------------------------------------
// The Shader interface versions
//-----------------------------------------------------------------------------
inline constexpr const char *SHADERDYNAMIC_INTERFACE_VERSION = "ShaderDynamic001";
class IShaderDynamicAPI
{
public:

	void BeginPerfEEvent( const wchar* name ) {}
	void EndPerfEvent() {}
	void SetVertexShaderViewProj();
	void UpdateVertexShaderMatrix( int m );
	void SetVertexShaderModelViewProjAndModelView();
	void SetVertexShaderCameraPos();
	bool SetSkinningMatrices( const MeshInstanceData_t &instance );
	void BindTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, ShaderAPITextureHandle_t textureHandle );

	// returns the current time in seconds....
	double CurrentTime() const;

	// Gets the lightmap dimensions
	void GetLightmapDimensions( int *w, int *h );

	// Scene fog state.
	// This is used by the shaders for picking the proper vertex shader for fogging based on dynamic state.
	MaterialFogMode_t GetSceneFogMode( );
	void GetSceneFogColor( unsigned char *rgb );

	// Sets the constant register for vertex and pixel shaders
	void SetVertexShaderConstant( int var, float const* pVec, int numConst = 1, bool bForce = false );
	void SetPixelShaderConstant( int var, float const* pVec, int numConst = 1, bool bForce = false );

	// Sets the default *dynamic* state
	void SetDefaultState();

	// Get the current camera position in world space.
	void GetWorldSpaceCameraPosition( float* pPos ) const;
	void GetWorldSpaceCameraDirection( float* pDir ) const;

	int GetCurrentNumBones( void ) const;

	MaterialFogMode_t GetCurrentFogType( void ) const;

	// Sets the vertex and pixel shaders
	void SetVertexShaderIndex( int vshIndex = -1 );
	void SetPixelShaderIndex( int pshIndex = 0 );

	// Get the dimensions of the back buffer.
	void GetBackBufferDimensions( int& width, int& height ) const;

	// Get information about the physical and pixel aspect ratios of the backbuffer.
	const AspectRatioInfo_t &GetAspectRatioInfo( void ) const;

	// Get the dimensions of the current render target
	void GetCurrentRenderTargetDimensions( int& nWidth, int& nHeight ) const;

	// Get the current viewport
	void GetCurrentViewport( int& nX, int& nY, int& nWidth, int& nHeight ) const;

	// FIXME: The following 6 methods used to live in IShaderAPI
	// and were moved for stdshader_dx8. Let's try to move them back!

	void SetPixelShaderFogParams( int reg );

	// Use this to get the mesh builder that allows us to modify vertex data
	bool InFlashlightMode() const;
	const FlashlightState_t &GetFlashlightState( VMatrix &worldToTexture ) const;
	bool InEditorMode() const;

	bool IsCascadedShadowMapping() const;
	const CascadedShadowMappingState_t &GetCascadedShadowMappingState( ITexture **pDepthTextureAtlas, bool bLightMapScale = false ) const;

	// Binds a standard texture
	void BindStandardTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, StandardTextureId_t id );

	ITexture *GetRenderTargetEx( int nRenderTargetID ) const;

	void SetToneMappingScaleLinear( const Vector &scale );
	const Vector &GetToneMappingScaleLinear( void ) const;
	void SetFloatRenderingParameter(int parm_number, float value);

	void SetIntRenderingParameter(int parm_number, int value);
	void SetVectorRenderingParameter(int parm_number, Vector const &value);

	float GetFloatRenderingParameter(int parm_number) const;

	int GetIntRenderingParameter(int parm_number) const;

	Vector GetVectorRenderingParameter(int parm_number) const;

	const FlashlightState_t &GetFlashlightStateEx( VMatrix &worldToTexture, ITexture **pFlashlightDepthTexture ) const;

	void GetDX9LightState( LightState_t *state ) const;
	int GetPixelFogCombo( ); //0 is either range fog, or no fog simulated with rigged range fog values. 1 is height fog

	void BindStandardVertexTexture( VertexTextureSampler_t sampler, StandardTextureId_t id );

	// Is hardware morphing enabled?
	bool IsHWMorphingEnabled( ) const;

	void GetStandardTextureDimensions( int *pWidth, int *pHeight, StandardTextureId_t id );

	void SetBooleanVertexShaderConstant( int var, BOOL const* pVec, int numBools = 1, bool bForce = false );
	void SetIntegerVertexShaderConstant( int var, int const* pVec, int numIntVecs = 1, bool bForce = false );
	void SetBooleanPixelShaderConstant( int var, BOOL const* pVec, int numBools = 1, bool bForce = false );
	void SetIntegerPixelShaderConstant( int var, int const* pVec, int numIntVecs = 1, bool bForce = false );

	//Are we in a configuration that needs access to depth data through the alpha channel later?
	bool ShouldWriteDepthToDestAlpha( void ) const;

	// Returns current matrices
	void GetMatrix( MaterialMatrixMode_t matrixMode, float *dst );

	// deformations
	void PushDeformation( DeformationBase_t const *Deformation );
	void PopDeformation( );
	int GetNumActiveDeformations() const;


	// for shaders to set vertex shader constants. returns a packed state which can be used to set
	// the dynamic combo. returns # of active deformations
	int GetPackedDeformationInformation( int nMaskOfUnderstoodDeformations,
		float *pConstantValuesOut,
		int nBufferSize,
		int nMaximumDeformations,
		int *pNumDefsOut ) const;

	// This lets the lower level system that certain vertex fields requested 
	// in the shadow state aren't actually being read given particular state
	// known only at dynamic state time. It's here only to silence warnings.
	void MarkUnusedVertexFields( unsigned int nFlags, int nTexCoordCount, bool *pUnusedTexCoords );


	void ExecuteCommandBuffer( uint8 *pCmdBuffer );

	// Interface for mat system to tell shaderapi about color correction
	void GetCurrentColorCorrection( ShaderColorCorrectionInfo_t* pInfo );

	ITexture *GetTextureRenderingParameter(int parm_number) const;

	void SetScreenSizeForVPOS( int pshReg = 32 );
	void SetVSNearAndFarZ( int vshReg );
	float GetFarZ();

	bool SinglePassFlashlightModeEnabled( void );

	void GetActualProjectionMatrix( float *pMatrix );

	void SetDepthFeatheringShaderConstants( int iConstant, float fDepthBlendScale );

	void GetFlashlightShaderInfo( bool *pShadowsEnabled, bool *pUberLight ) const;

	float GetFlashlightAmbientOcclusion( ) const;

	// allows overriding texture filtering mode on an already bound texture.
	void SetTextureFilterMode( Sampler_t sampler, TextureFilterMode_t nMode );

	TessellationMode_t GetTessellationMode() const;

	float GetSubDHeight();


	bool IsRenderingPaint() const;

	bool IsStereoActiveThisFrame() const;

	bool IsStandardTextureHandleValid( StandardTextureId_t textureId );
};

// end class IShaderDynamicAPI
