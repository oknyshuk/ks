#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

import std;
import ks.math;
#include "shaderapi/ishaderdynamic.h"
#include "shaderapi/IShaderDevice.h"
#include "materialsystem/deformations.h"
#include "shaderlib/shadercombosemantics.h"

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class IShaderUtil;
class IFileSystem;
class CPixelWriter;
class CMeshBuilder;
struct MaterialVideoMode_t;
class IMesh;
class IVertexBuffer;
class IIndexBuffer;
struct MeshDesc_t;
enum MaterialCullMode_t;
class IDataCache;   
struct MorphWeight_t;
struct MeshInstanceData_t;
struct ShaderComboInformation_t;

//-----------------------------------------------------------------------------
// This must match the definition in playback.cpp!
//-----------------------------------------------------------------------------
enum ShaderRenderTarget_t
{
	SHADER_RENDERTARGET_BACKBUFFER = -1,
	SHADER_RENDERTARGET_DEPTHBUFFER = -1,
	// GR - no RT, used to disable depth buffer
	SHADER_RENDERTARGET_NONE = -2,
};


//-----------------------------------------------------------------------------
// The state snapshot handle
//-----------------------------------------------------------------------------
typedef short StateSnapshot_t;


//-----------------------------------------------------------------------------
// The state snapshot handle
//-----------------------------------------------------------------------------
typedef int ShaderDLL_t;


//-----------------------------------------------------------------------------
// Texture creation
//-----------------------------------------------------------------------------
enum CreateTextureFlags_t
{
	TEXTURE_CREATE_CUBEMAP		       = 0x00001,
	TEXTURE_CREATE_RENDERTARGET        = 0x00002,
	TEXTURE_CREATE_MANAGED		       = 0x00004,
	TEXTURE_CREATE_DEPTHBUFFER	       = 0x00008,
	TEXTURE_CREATE_DYNAMIC		       = 0x00010,
	TEXTURE_CREATE_AUTOMIPMAP          = 0x00020,
	TEXTURE_CREATE_VERTEXTEXTURE       = 0x00040,
	TEXTURE_CREATE_CACHEABLE           = 0x00080,	// 360:		texture may be subject to streaming
	TEXTURE_CREATE_NOD3DMEMORY         = 0x00100,	// CONSOLE:	real allocation needs to occur later
	TEXTURE_CREATE_REDUCED             = 0x00200,	// CONSOLE:	true dimensions forced smaller (i.e. exclusion)
	TEXTURE_CREATE_EXCLUDED            = 0x00400,	// CONSOLE:	marked as excluded
	TEXTURE_CREATE_DEFAULT_POOL	       = 0x00800,
	TEXTURE_CREATE_UNFILTERABLE_OK     = 0x01000,
	TEXTURE_CREATE_CANCONVERTFORMAT    = 0x02000,	// 360:		allow format conversions at load
	TEXTURE_CREATE_PWLCORRECTED        = 0x04000,	// 360:		texture is pwl corrected
	TEXTURE_CREATE_ERROR               = 0x08000,	// CONSOLE:	texture was forced to checkerboard
	TEXTURE_CREATE_SYSMEM              = 0x10000,
	TEXTURE_CREATE_SRGB                = 0x20000,	// Posix/GL only, for textures which are SRGB-readable
	TEXTURE_CREATE_ANISOTROPIC		   = 0x40000,	// Posix/GL only, for textures which are flagged to use max aniso
	TEXTURE_CREATE_REUSEHANDLES		   = 0x80000,	// hint to re-use supplied texture handles
};

//-----------------------------------------------------------------------------
// Viewport structure
//-----------------------------------------------------------------------------
inline constexpr auto SHADER_VIEWPORT_VERSION = 1;
struct ShaderViewport_t
{
	int m_nVersion;
	int m_nTopLeftX;
	int m_nTopLeftY;
	int m_nWidth;
	int m_nHeight;
	float m_flMinZ;
	float m_flMaxZ;

	ShaderViewport_t() : m_nVersion( SHADER_VIEWPORT_VERSION ) {}

	void Init()
	{
		std::memset( this, 0, sizeof(ShaderViewport_t) );
		m_nVersion = SHADER_VIEWPORT_VERSION;
	}

	void Init( int x, int y, int nWidth, int nHeight, float flMinZ = 0.0f, float flMaxZ = 1.0f )
	{
		m_nVersion = SHADER_VIEWPORT_VERSION;
		m_nTopLeftX = x; m_nTopLeftY = y; m_nWidth = nWidth; m_nHeight = nHeight;
		m_flMinZ = flMinZ;
		m_flMaxZ = flMaxZ;
	}
};


//-----------------------------------------------------------------------------
// Fill modes
//-----------------------------------------------------------------------------
enum ShaderFillMode_t
{
	SHADER_FILL_SOLID = 0,
	SHADER_FILL_WIREFRAME,
};


//-----------------------------------------------------------------------------
// Rasterization state object
//-----------------------------------------------------------------------------
struct ShaderRasterState_t
{
	ShaderFillMode_t m_FillMode;
	MaterialCullMode_t m_CullMode;
	bool m_bCullEnable : 1;
	bool m_bDepthBias : 1;
	bool m_bScissorEnable : 1;
	bool m_bMultisampleEnable : 1;
};


//-----------------------------------------------------------------------------
// allowed stencil operations. These match the d3d operations
//-----------------------------------------------------------------------------
enum ShaderStencilOp_t 
{
	SHADER_STENCILOP_KEEP = 1,
	SHADER_STENCILOP_ZERO = 2,
	SHADER_STENCILOP_SET_TO_REFERENCE = 3,
	SHADER_STENCILOP_INCREMENT_CLAMP = 4,
	SHADER_STENCILOP_DECREMENT_CLAMP = 5,
	SHADER_STENCILOP_INVERT = 6,
	SHADER_STENCILOP_INCREMENT_WRAP = 7,
	SHADER_STENCILOP_DECREMENT_WRAP = 8,
	SHADER_STENCILOP_FORCE_DWORD = 0x7fffffff
};

enum ShaderStencilFunc_t 
{
	SHADER_STENCILFUNC_NEVER = 1,
	SHADER_STENCILFUNC_LESS = 2,
	SHADER_STENCILFUNC_EQUAL = 3,
	SHADER_STENCILFUNC_LEQUAL = 4,
	SHADER_STENCILFUNC_GREATER = 5,
	SHADER_STENCILFUNC_NOTEQUAL = 6,
	SHADER_STENCILFUNC_GEQUAL = 7,
	SHADER_STENCILFUNC_ALWAYS = 8,

	SHADER_STENCILFUNC_FORCE_DWORD = 0x7fffffff
};


//-----------------------------------------------------------------------------
// Stencil state
//-----------------------------------------------------------------------------
struct ShaderStencilState_t
{
	bool m_bEnable;
	ShaderStencilOp_t m_FailOp;
	ShaderStencilOp_t m_ZFailOp;
	ShaderStencilOp_t m_PassOp;
	ShaderStencilFunc_t m_CompareFunc;
	int m_nReferenceValue;
	uint32 m_nTestMask;
	uint32 m_nWriteMask;


	ShaderStencilState_t()
	{
		m_bEnable = false;
		m_PassOp = m_FailOp = m_ZFailOp = SHADER_STENCILOP_KEEP;
		m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;
		m_nReferenceValue = 0;
		m_nTestMask = m_nWriteMask = 0xFFFFFFFF;

	}
};


//-----------------------------------------------------------------------------
// Used for occlusion queries
//-----------------------------------------------------------------------------
struct ShaderAPIOcclusionQuery_t__ { int unused; };
using ShaderAPIOcclusionQuery_t = ShaderAPIOcclusionQuery_t__ *;
inline constexpr auto INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE = ( (ShaderAPIOcclusionQuery_t)0 );

enum ShaderAPIOcclusionQueryResult_t
{
	OCCLUSION_QUERY_RESULT_PENDING	=	-1,
	OCCLUSION_QUERY_RESULT_ERROR	=	-2
};
template <typename T1> [[gnu::always_inline]] inline auto OCCLUSION_QUERY_FINISHED( T1 iQueryResult ) { return ( ( iQueryResult ) != OCCLUSION_QUERY_RESULT_PENDING ); }


//-----------------------------------------------------------------------------
// Used on the 360 to create meshes from data in write-combined memory
//-----------------------------------------------------------------------------
struct ExternalMeshInfo_t
{
	IMaterial *m_pMaterial;
	VertexFormat_t m_VertexFormat;
	bool m_bFlexMesh;
	IMesh* m_pVertexOverride;
	IMesh* m_pIndexOverride;
};

struct ExternalMeshData_t
{
	uint8 *m_pVertexData;
	int m_nVertexCount;
	int m_nVertexSizeInBytes;
	uint16 *m_pIndexData;
	int m_nIndexCount;
};


//-----------------------------------------------------------------------------
// This is what the material system gets to see.
//-----------------------------------------------------------------------------
inline constexpr const char *SHADERAPI_INTERFACE_VERSION = "ShaderApi029";
class IShaderAPI : public IShaderDynamicAPI
{
public:
	//
	// NOTE: These methods have been ported to DX10
	//

	// Viewport methods
	void SetViewports( int nCount, const ShaderViewport_t* pViewports, bool setImmediately = false );
	int GetViewports( ShaderViewport_t* pViewports, int nMax ) const;

	// Buffer clearing
	void ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil, int renderTargetWidth, int renderTargetHeight );
	void ClearColor3ub( unsigned char r, unsigned char g, unsigned char b );
	void ClearColor4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a );

	// Methods related to binding shaders
	void BindVertexShader( VertexShaderHandle_t hVertexShader );
	void BindGeometryShader( GeometryShaderHandle_t hGeometryShader );
	void BindPixelShader( PixelShaderHandle_t hPixelShader );

	// Methods related to state objects
	void SetRasterState( const ShaderRasterState_t& state );

	//
	// NOTE: These methods have not yet been ported to DX10
	//

	// Sets the mode...
	bool SetMode( void* hwnd, int nAdapter, const ShaderDeviceInfo_t &info );

	void ChangeVideoMode( const ShaderDeviceInfo_t &info );

	// Returns the snapshot id for the shader state
	StateSnapshot_t	 TakeSnapshot( );

	void TexMinFilter( ShaderTexFilterMode_t texFilterMode );
	void TexMagFilter( ShaderTexFilterMode_t texFilterMode );
	void TexWrap( ShaderTexCoordComponent_t coord, ShaderTexWrapMode_t wrapMode );

	void CopyRenderTargetToTexture( ShaderAPITextureHandle_t textureHandle );

	// Binds a particular material to render with
	void Bind( IMaterial* pMaterial );

	// Gets the dynamic mesh; note that you've got to render the mesh
	// before calling this function a second time. Clients should *not*
	// call DestroyStaticMesh on the mesh returned by this call.
	IMesh* GetDynamicMesh( IMaterial* pMaterial, int nHWSkinBoneCount, bool bBuffered = true,
		IMesh* pVertexOverride = 0, IMesh* pIndexOverride = 0);
	IMesh* GetDynamicMeshEx( IMaterial* pMaterial, VertexFormat_t vertexFormat, int nHWSkinBoneCount, 
		bool bBuffered = true, IMesh* pVertexOverride = 0, IMesh* pIndexOverride = 0 );

	// Methods to ask about particular state snapshots
	bool IsTranslucent( StateSnapshot_t id ) const;
	bool IsAlphaTested( StateSnapshot_t id ) const;
	bool UsesVertexAndPixelShaders( StateSnapshot_t id ) const;
	bool IsDepthWriteEnabled( StateSnapshot_t id ) const;

	// Gets the vertex format for a set of snapshot ids
	VertexFormat_t ComputeVertexFormat( int numSnapshots, StateSnapshot_t* pIds ) const;

	// What fields in the vertex do we actually use?
	VertexFormat_t ComputeVertexUsage( int numSnapshots, StateSnapshot_t* pIds ) const;

	// Begins a rendering pass
	void BeginPass( StateSnapshot_t snapshot );

	// Renders a single pass of a material
	void RenderPass( const unsigned char *pInstanceCommandBuffer, int nPass, int nPassCount );

	// Set the number of bone weights
	void SetNumBoneWeights( int numBones );

	// Sets the lights
	void SetLights( int nCount, const LightDesc_t *pDesc );

	// Lighting origin for the current model
	void SetLightingOrigin( Vector vLightingOrigin );

	void SetLightingState( const MaterialLightingState_t& state );
	void SetAmbientLightCube( Vector4D cube[6] );

	// The shade mode
	void ShadeMode( ShaderShadeMode_t mode );

	// The cull mode
	void CullMode( MaterialCullMode_t cullMode );
	void FlipCullMode( void ); //CW->CCW or CCW->CW, intended for mirror support where the view matrix is flipped horizontally

	void BeginGeneratingCSMs();
	void EndGeneratingCSMs();
	void PerpareForCascadeDraw( int cascade, float fShadowSlopeScaleDepthBias, float fShadowDepthBias );

	// Force writes only when z matches. . . useful for stenciling things out
	// by rendering the desired Z values ahead of time.
	void ForceDepthFuncEquals( bool bEnable );

	// Forces Z buffering to be on or off
	void OverrideDepthEnable( bool bEnable, bool bDepthWriteEnable, bool bDepthTestEnable = true );

	void SetHeightClipZ( float z ); 
	void SetHeightClipMode( enum MaterialHeightClipMode_t heightClipMode ); 

	void SetClipPlane( int index, const float *pPlane );
	void EnableClipPlane( int index, bool bEnable );
	
	// Returns the nearest supported format
	ImageFormat GetNearestSupportedFormat( ImageFormat fmt, bool bFilteringRequired = true ) const;
	ImageFormat GetNearestRenderTargetFormat( ImageFormat fmt ) const;

	// When AA is enabled, render targets are not AA and require a separate
	// depth buffer.
	bool DoRenderTargetsNeedSeparateDepthBuffer() const;

	// Texture management methods
	// For CreateTexture also see CreateTextures below
	ShaderAPITextureHandle_t CreateTexture( 
		int width, 
		int height,
		int depth,
		ImageFormat dstImageFormat, 
		int numMipLevels, 
		int numCopies, 
		int flags, 
		const char *pDebugName,
		const char *pTextureGroupName );

	void DeleteTexture( ShaderAPITextureHandle_t textureHandle );

	ShaderAPITextureHandle_t CreateDepthTexture( 
		ImageFormat renderTargetFormat, 
		int width, 
		int height, 
		const char *pDebugName,
		bool bTexture,
		bool bAliasDepthSurfaceOverColorX360 = false );

	bool IsTexture( ShaderAPITextureHandle_t textureHandle );
	bool IsTextureResident( ShaderAPITextureHandle_t textureHandle );

	// Indicates we're going to be modifying this texture
	// TexImage2D, TexSubImage2D, TexWrap, TexMinFilter, and TexMagFilter
	// all use the texture specified by this function.
	void ModifyTexture( ShaderAPITextureHandle_t textureHandle );

	void TexImage2D(
		int level,
		int cubeFaceID,
		ImageFormat dstFormat,
		int zOffset,
		int width,
		int height,
		ImageFormat srcFormat, 
		bool bSrcIsTiled,		// NOTE: for X360 only
		void *imageData );

	void TexSubImage2D( 
		int level, 
		int cubeFaceID, 
		int xOffset, 
		int yOffset, 
		int zOffset, 
		int width, 
		int height,
		ImageFormat srcFormat, 
		int srcStride, 
		bool bSrcIsTiled,		// NOTE: for X360 only
		void *imageData );
	
	// An alternate (and faster) way of writing image data
	// (locks the current Modify Texture). Use the pixel writer to write the data
	// after Lock is called
	// Doesn't work for compressed textures 
	bool TexLock( int level, int cubeFaceID, int xOffset, int yOffset, 
		int width, int height, CPixelWriter& writer );
	void TexUnlock( );

	// Copy sysmem surface to default pool surface asynchronously
	void UpdateTexture( int xOffset, int yOffset, int w, int h, ShaderAPITextureHandle_t hDstTexture, ShaderAPITextureHandle_t hSrcTexture );
	void *LockTex( ShaderAPITextureHandle_t hTexture );
	void UnlockTex( ShaderAPITextureHandle_t hTexture );

	// These are bound to the texture
	void TexSetPriority( int priority );

	// Sets the texture state
	void BindTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, ShaderAPITextureHandle_t textureHandle );

	// Set the render target to a texID.
	// Set to SHADER_RENDERTARGET_BACKBUFFER if you want to use the regular framebuffer.
	// Set to SHADER_RENDERTARGET_DEPTHBUFFER if you want to use the regular z buffer.
	void SetRenderTarget( ShaderAPITextureHandle_t colorTextureHandle = SHADER_RENDERTARGET_BACKBUFFER, 
		ShaderAPITextureHandle_t depthTextureHandle = SHADER_RENDERTARGET_DEPTHBUFFER );
	
	// stuff that isn't to be used from within a shader
	void ClearBuffersObeyStencil( bool bClearColor, bool bClearDepth );
	void ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture = nullptr );
	void ReadPixelsAsync( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture = nullptr, CThreadEvent *pPixelsReadEvent = nullptr );
	void ReadPixelsAsyncGetResult( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, CThreadEvent *pGetResultEvent = nullptr );
	void ReadPixels( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *data, ImageFormat dstFormat, int nDstStride );

	void FlushHardware();

	// Use this to begin and end the frame
	void BeginFrame();
	void EndFrame();

	// Selection mode methods
	int  SelectionMode( bool selectionMode );
	void SelectionBuffer( unsigned int* pBuffer, int size );
	void ClearSelectionNames( );
	void LoadSelectionName( int name );
	void PushSelectionName( int name );
	void PopSelectionName();

	// Force the hardware to finish whatever it's doing
	void ForceHardwareSync();

	// Used to clear the transition table when we know it's become invalid.
	void ClearSnapshots();

	void FogStart( float fStart );
	void FogEnd( float fEnd );
	void SetFogZ( float fogZ );	
	// Scene fog state.
	void SceneFogColor3ub( unsigned char r, unsigned char g, unsigned char b );
	void GetSceneFogColor( unsigned char *rgb );
	void SceneFogMode( MaterialFogMode_t fogMode );

	// Can we download textures?
	bool CanDownloadTextures() const;

	void ResetRenderState( bool bFullReset = true );

	// We use smaller dynamic VBs during level transitions, to free up memory
	int  GetCurrentDynamicVBSize( void );
	void DestroyVertexBuffers( bool bExitingLevel = false );

	void EvictManagedResources();

	// Get stats on GPU memory usage
	void GetGPUMemoryStats( GPUMemoryStats &stats );

	// Level of anisotropic filtering
	void SetAnisotropicLevel( int nAnisotropyLevel );

	// For debugging and building recording files. This will stuff a token into the recording file,
	// then someone doing a playback can watch for the token.
	void SyncToken( const char *pToken );

	// Setup standard vertex shader constants (that don't change)
	// This needs to be called anytime that overbright changes.
	void SetStandardVertexShaderConstants( float fOverbright );

	//
	// Occlusion query support
	//
	
	// Allocate and delete query objects.
	ShaderAPIOcclusionQuery_t CreateOcclusionQueryObject( void );
	void DestroyOcclusionQueryObject( ShaderAPIOcclusionQuery_t );

	// Bracket drawing with begin and end so that we can get counts next frame.
	void BeginOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t );
	void EndOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t );

	// OcclusionQuery_GetNumPixelsRendered
	//	Get the number of pixels rendered between begin and end on an earlier frame.
	//	Calling this in the same frame is a huge perf hit!
	// Returns iQueryResult:
	//	iQueryResult >= 0					-	iQueryResult is the number of pixels rendered
	//	OCCLUSION_QUERY_RESULT_PENDING		-	query results are not available yet
	//	OCCLUSION_QUERY_RESULT_ERROR		-	query failed
	// Use OCCLUSION_QUERY_FINISHED( iQueryResult ) to test if query finished.
	int OcclusionQuery_GetNumPixelsRendered( ShaderAPIOcclusionQuery_t hQuery, bool bFlush = false );

	void SetFlashlightState( const FlashlightState_t &state, const VMatrix &worldToTexture );
			
	bool IsCascadedShadowMapping() const;
	void SetCascadedShadowMappingState( const CascadedShadowMappingState_t &state, ITexture *pDepthTextureAtlas );
	const CascadedShadowMappingState_t &GetCascadedShadowMappingState( ITexture **pDepthTextureAtlas, bool bLightMapScale = false ) const;

	void ClearVertexAndPixelShaderRefCounts();
	void PurgeUnusedVertexAndPixelShaders();

	// Called when the dx support level has changed
	void DXSupportLevelChanged( int nDXLevel );

	// By default, the material system applies the VIEW and PROJECTION matrices	to the user clip
	// planes (which are specified in world space) to generate projection-space user clip planes
	// Occasionally (for the particle system in hl2, for example), we want to override that
	// behavior and explictly specify a View transform for user clip planes. The PROJECTION
	// will be mutliplied against this instead of the normal VIEW matrix.
	void EnableUserClipTransformOverride( bool bEnable );
	void UserClipTransform( const VMatrix &worldToView );

	// ----------------------------------------------------------------------------------
	// Everything after this point added after HL2 shipped.
	// ----------------------------------------------------------------------------------

	// Set the render target to a texID.
	// Set to SHADER_RENDERTARGET_BACKBUFFER if you want to use the regular framebuffer.
	// Set to SHADER_RENDERTARGET_DEPTHBUFFER if you want to use the regular z buffer.
	void SetRenderTargetEx( int nRenderTargetID, 
		ShaderAPITextureHandle_t colorTextureHandle = SHADER_RENDERTARGET_BACKBUFFER, 
		ShaderAPITextureHandle_t depthTextureHandle = SHADER_RENDERTARGET_DEPTHBUFFER );

	void CopyRenderTargetToTextureEx( ShaderAPITextureHandle_t textureHandle, int nRenderTargetID, Rect_t *pSrcRect = nullptr, Rect_t *pDstRect = nullptr );
	void CopyTextureToRenderTargetEx( int nRenderTargetID, ShaderAPITextureHandle_t textureHandle, Rect_t *pSrcRect = nullptr, Rect_t *pDstRect = nullptr );

	// For dealing with device lost in cases where SwapBuffers isn't called all the time (Hammer)
	void HandleDeviceLost();

	void EnableLinearColorSpaceFrameBuffer( bool bEnable );

	// Lets the shader know about the full-screen texture so it can 
	void SetFullScreenTextureHandle( ShaderAPITextureHandle_t h );

	// Rendering parameters control special drawing modes withing the material system, shader
	// system, shaders, and engine. renderparm.h has their definitions.
	void SetFloatRenderingParameter(int parm_number, float value);
	void SetIntRenderingParameter(int parm_number, int value);
	void SetVectorRenderingParameter(int parm_number, Vector const &value);

	float GetFloatRenderingParameter(int parm_number) const;
	int GetIntRenderingParameter(int parm_number) const;
	Vector GetVectorRenderingParameter(int parm_number) const;

	void SetFastClipPlane( const float *pPlane );
	void EnableFastClip( bool bEnable );

	// Returns the number of vertices + indices we can render using the dynamic mesh
	// Passing true in the second parameter will return the max # of vertices + indices
	// we can use before a flush is provoked and may return different values 
	// if called multiple times in succession. 
	// Passing false into the second parameter will return
	// the maximum possible vertices + indices that can be rendered in a single batch
	void GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices );

	// Returns the max number of vertices we can render for a given material
	int GetMaxVerticesToRender( IMaterial *pMaterial );
	int GetMaxIndicesToRender( );

	// stencil methods
	void SetStencilState( const ShaderStencilState_t& state );
	void ClearStencilBufferRectangle(int xmin, int ymin, int xmax, int ymax, int value);

	// disables all local lights
	void DisableAllLocalLights();
	int CompareSnapshots( StateSnapshot_t snapshot0, StateSnapshot_t snapshot1 );

	IMesh *GetFlexMesh();

	void SetFlashlightStateEx( const FlashlightState_t &state, const VMatrix &worldToTexture, ITexture *pFlashlightDepthTexture );

	bool SupportsMSAAMode( int nMSAAMode );


	
	void AntiAliasingHint( int nHint );

	bool OwnGPUResources( bool bEnable );

	//get fog distances entered with FogStart(), FogEnd(), and SetFogZ()
	void GetFogDistances( float *fStart, float *fEnd, float *fFogZ );

	// Hooks for firing PIX events from outside the Material System...
	void BeginPIXEvent( unsigned long color, const char *szName );
	void EndPIXEvent();
	void SetPIXMarker( unsigned long color, const char *szName );

	// Enables and disables for Alpha To Coverage
	void EnableAlphaToCoverage();
	void DisableAlphaToCoverage();

	// Computes the vertex buffer pointers 
	void ComputeVertexDescription( unsigned char* pBuffer, VertexFormat_t vertexFormat, MeshDesc_t& desc ) const;
	int VertexFormatSize( VertexFormat_t vertexFormat ) const;

	void SetDisallowAccess( bool );
	void EnableShaderShaderMutex( bool );
	void ShaderLock();
	void ShaderUnlock();

	void SetShadowDepthBiasFactors( float fShadowSlopeScaleDepthBias, float fShadowDepthBias );

// ------------ New Vertex/Index Buffer interface ----------------------------
	void BindVertexBuffer( int nStreamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions = 1 );
	void BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes );
	void Draw( MaterialPrimitiveType_t primitiveType, int nFirstIndex, int nIndexCount );
// ------------ End ----------------------------


	// Apply stencil operations to every pixel on the screen without disturbing depth or color buffers
	void PerformFullScreenStencilOperation( void );

	void SetScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom, const bool bEnableScissor );

	// nVidia CSAA modes, different from SupportsMSAAMode()
	bool SupportsCSAAMode( int nNumSamples, int nQualityLevel );

	//Notifies the shaderapi to invalidate the current set of delayed constants because we just finished a draw pass. Either actual or not.
	void InvalidateDelayedShaderConstants( void ); 

	// Gamma<->Linear conversions according to the video hardware we're running on
	float GammaToLinear_HardwareSpecific( float fGamma ) const;
	float LinearToGamma_HardwareSpecific( float fLinear ) const;

	//Set's the linear->gamma conversion textures to use for this hardware for both srgb writes enabled and disabled(identity)
	void SetLinearToGammaConversionTextures( ShaderAPITextureHandle_t hSRGBWriteEnabledTexture, ShaderAPITextureHandle_t hIdentityTexture );

	void BindVertexTexture( VertexTextureSampler_t nSampler, ShaderAPITextureHandle_t textureHandle );

	// Enables hardware morphing
	void EnableHWMorphing( bool bEnable );

	// Sets flexweights for rendering
	void SetFlexWeights( int nFirstWeight, int nCount, const MorphWeight_t* pWeights );

	void FogMaxDensity( float flMaxDensity );

	void *GetD3DTexturePtr( ShaderAPITextureHandle_t hTexture );
	bool IsStandardTextureHandleValid( StandardTextureId_t textureId );

	// Create a multi-frame texture (equivalent to calling "CreateTexture" multiple times, but more efficient)
	void CreateTextures( 
		ShaderAPITextureHandle_t *pHandles,
		int count,
		int width, 
		int height,
		int depth,
		ImageFormat dstImageFormat, 
		int numMipLevels, 
		int numCopies, 
		int flags, 
		const char *pDebugName,
		const char *pTextureGroupName );

	void AcquireThreadOwnership();
	void ReleaseThreadOwnership();

	// Only does anything on XBox360. This is useful to eliminate stalls
	void EnableBuffer2FramesAhead( bool bEnable );

	void FlipCulling( bool bFlipCulling );

	void SetTextureRenderingParameter(int parm_number, ITexture *pTexture);

	//only actually sets a bool that can be read in from shaders, doesn't do any of the legwork
	void EnableSinglePassFlashlightMode( bool bEnable );

	// stuff related to matrix stacks
	void MatrixMode( MaterialMatrixMode_t matrixMode );
	void PushMatrix();
	void PopMatrix();
	void LoadMatrix( float *m );
	void MultMatrix( float *m );
	void MultMatrixLocal( float *m );
	void LoadIdentity( void );
	void LoadCameraToWorld( void );
	void Ortho( double left, double right, double bottom, double top, double zNear, double zFar );
	void PerspectiveX( double fovx, double aspect, double zNear, double zFar );
	void PickMatrix( int x, int y, int width, int height );
	void Rotate( float angle, float x, float y, float z );
	void Translate( float x, float y, float z );
	void Scale( float x, float y, float z );
	void ScaleXY( float x, float y );
	void PerspectiveOffCenterX( double fovx, double aspect, double zNear, double zFar, double bottom, double top, double left, double right );

	void LoadBoneMatrix( int boneIndex, const float *m );

	// interface for mat system to tell shaderapi about standard texture handles
	void SetStandardTextureHandle( StandardTextureId_t nId, ShaderAPITextureHandle_t nHandle );

	void DrawInstances( int nInstanceCount, const MeshInstanceData_t *pInstance );

	// Allows us to override the color/alpha write settings of a material
	void OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable );
	void OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable );

	//extended clear buffers function with alpha independent from color
	void ClearBuffersObeyStencilEx( bool bClearColor, bool bClearAlpha, bool bClearDepth );

	void OnPresent( void );

	void UpdateGameTime( float flTime );


	bool IsStereoSupported() const;
	void UpdateStereoTexture( ShaderAPITextureHandle_t texHandle, bool *pStereoActiveThisFrame );
	
	void SetSRGBWrite( bool bState );

	// debug logging
	// only implemented in some subclasses
	void PrintfVA( char *fmt, std::va_list vargs );
	float Knob( char *knobname, float *setvalue = nullptr );

	void AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics );

	float GetLightMapScaleFactor() const;

	ShaderAPITextureHandle_t FindTexture( const char *pDebugName );
	void GetTextureDimensions( ShaderAPITextureHandle_t hTexture, int &nWidth, int &nHeight, int &nDepth );

	ShaderAPITextureHandle_t GetStandardTextureHandle(StandardTextureId_t id);
};
