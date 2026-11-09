// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

import ks.tier1;
#include "materialsystem/imaterialsystem.h"
#include "IHardwareConfigInternal.h"
#include "shadersystem.h"
#include "shaderapi/ishaderutil.h"
#include "shaderapi/ishaderapi.h"
#include "materialsystem/imesh.h"
import std;
#include "materialsystem/idebugtextureinfo.h"
#include "materialsystem/ivballoctracker.h"
#include "materialsystem/deformations.h"
#include "../cmatqueuedmesh.h"

//-----------------------------------------------------------------------------
// The empty mesh
//-----------------------------------------------------------------------------
class CEmptyMesh : public IMesh
{
public:
	friend class IVertexBuffer;
	friend class IIndexBuffer;
	friend class IMesh;
	CEmptyMesh( bool bIsDynamic );
	~CEmptyMesh();

	// FIXME: Make this work! Unsupported methods of IIndexBuffer + IVertexBuffer
	bool Lock( int nMaxIndexCount, bool bAppend, IndexDesc_t& desc );
	void Unlock( int nWrittenIndexCount, IndexDesc_t& desc );
	void ModifyBegin( bool bReadOnly, int nFirstIndex, int nIndexCount, IndexDesc_t& desc );
	void ModifyEnd( IndexDesc_t& desc );
	void Spew( int nIndexCount, const IndexDesc_t & desc );
	void ValidateData( int nIndexCount, const IndexDesc_t &desc );
	bool Lock( int nVertexCount, bool bAppend, VertexDesc_t &desc );
	void Unlock( int nVertexCount, VertexDesc_t &desc );
	void Spew( int nVertexCount, const VertexDesc_t &desc );
	void ValidateData( int nVertexCount, const VertexDesc_t & desc );
	bool IsDynamic() const { return m_bIsDynamic; }
	void BeginCastBuffer( VertexFormat_t format ) {}
	void BeginCastBuffer( MaterialIndexFormat_t format ) {}
	void EndCastBuffer( ) {}
	int GetRoomRemaining() const { return 0; }
	MaterialIndexFormat_t IndexFormat() const { return MATERIAL_INDEX_FORMAT_UNKNOWN; }

	void LockMesh( int numVerts, int numIndices, MeshDesc_t& desc, MeshBuffersAllocationSettings_t *pSettings = 0 );
	void UnlockMesh( int numVerts, int numIndices, MeshDesc_t& desc );

	void ModifyBeginEx( bool bReadOnly, int firstVertex, int numVerts, int firstIndex, int numIndices, MeshDesc_t& desc );
	void ModifyBegin( int firstVertex, int numVerts, int firstIndex, int numIndices, MeshDesc_t& desc );
	void ModifyEnd( MeshDesc_t& desc );

	// returns the # of vertices (static meshes only)
	int  VertexCount() const;

	// Sets the primitive type
	void SetPrimitiveType( MaterialPrimitiveType_t type );
	 
	// Draws the entire mesh
	void Draw(int firstIndex, int numIndices);
	void DrawModulated( const Vector4D &vecDiffuseModulation, int firstIndex, int numIndices) {}

	void Draw(CPrimList *pPrims, int nPrims);

	// Copy verts and/or indices to a mesh builder. This only works for temp meshes!
	void CopyToMeshBuilder( 
		int iStartVert,		// Which vertices to copy.
		int nVerts, 
		int iStartIndex,	// Which indices to copy.
		int nIndices, 
		int indexOffset,	// This is added to each index.
		CMeshBuilder &builder );

	// Spews the mesh data
	void Spew( int numVerts, int numIndices, const MeshDesc_t & desc );

	void ValidateData( int numVerts, int numIndices, const MeshDesc_t & desc );

	// gets the associated material
	IMaterial* GetMaterial();

	void SetColorMesh( IMesh *pColorMesh, int nVertexOffset )
	{
	}


	int IndexCount() const
	{
		return 0;
	}

	void SetFlexMesh( IMesh *pMesh, int nVertexOffset ) {}

	void DisableFlexMesh() {}

	void MarkAsDrawn() {}

	VertexFormat_t GetVertexFormat() const { return VERTEX_POSITION; }

	IMesh *GetMesh()
	{
		return this;
	}

	unsigned int ComputeMemoryUsed() { return 0; }

	void * AccessRawHardwareDataStream( uint8 nRawStreamIndex, uint32 numBytes, uint32 uiFlags, void *pvContext ) { return nullptr; }

	ICachedPerFrameMeshData *GetCachedPerFrameMeshData() { return nullptr; }
	void ReconstructFromCachedPerFrameMeshData( ICachedPerFrameMeshData *pData ) {}

private:
	enum
	{
		VERTEX_BUFFER_SIZE = 1024 // * 1024
	};

	unsigned char* m_pVertexMemory;
	bool m_bIsDynamic;
};

#include "meshdispatch.inl"


//-----------------------------------------------------------------------------
// The empty shader shadow
//-----------------------------------------------------------------------------
class CShaderShadowEmpty
{
public:
	CShaderShadowEmpty();
	~CShaderShadowEmpty();

	// Sets the default *shadow* state
	void SetDefaultState();

	// Methods related to depth buffering
	void DepthFunc( ShaderDepthFunc_t depthFunc );
	void EnableDepthWrites( bool bEnable );
	void EnableDepthTest( bool bEnable );
	void EnablePolyOffset( PolygonOffsetMode_t nOffsetMode );

	// Suppresses/activates color writing 
	void EnableColorWrites( bool bEnable );
	void EnableAlphaWrites( bool bEnable );

	// Methods related to alpha blending
	void EnableBlending( bool bEnable );
	void EnableBlendingForceOpaque( bool bEnable );
	void BlendFunc( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor );

	// Alpha testing
	void EnableAlphaTest( bool bEnable );
	void AlphaFunc( ShaderAlphaFunc_t alphaFunc, float alphaRef /* [0-1] */ );

	// Wireframe/filled polygons
	void PolyMode( ShaderPolyModeFace_t face, ShaderPolyMode_t polyMode );

	// Back face culling
	void EnableCulling( bool bEnable );
	
	// Indicates the vertex format for use with a vertex shader
	// The flags to pass in here come from the VertexFormatFlags_t enum
	// If pTexCoordDimensions is *not* specified, we assume all coordinates
	// are 2-dimensional
	void VertexShaderVertexFormat( unsigned int nFlags, 
		int nTexCoordCount, int* pTexCoordDimensions, int nUserDataSize );
	
	// Per texture unit stuff
	void EnableTexture( Sampler_t stage, bool bEnable ) {};
	void EnableVertexTexture( VertexTextureSampler_t stage, bool bEnable ) {};

	// Separate alpha blending
	void EnableBlendingSeparateAlpha( bool bEnable );
	void BlendFuncSeparateAlpha( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor );

	// Sets the vertex and pixel shaders
	void SetVertexShader( const char *pFileName, int vshIndex );
	void SetPixelShader( const char *pFileName, int pshIndex );

	// Convert from linear to gamma color space on writes to frame buffer.
	void EnableSRGBWrite( bool bEnable )
	{
	}

	void EnableSRGBRead( Sampler_t stage, bool bEnable )
	{
	}

	void FogMode( ShaderFogMode_t fogMode, bool bVertexFog )
	{
	}

	void DisableFogGammaCorrection( bool bDisable )
	{
	}

	void ExecuteCommandBuffer( uint8 *pBuf ) 
	{
	}
	
	// Alpha to coverage
	void EnableAlphaToCoverage( bool bEnable );
	
	void SetShadowDepthFiltering( Sampler_t stage )
	{
	}

	void BlendOp( ShaderBlendOp_t blendOp )
	{
	}

	void BlendOpSeparateAlpha( ShaderBlendOp_t blendOp )
	{
	}
	float GetLightMapScaleFactor( void ) const
	{
		return 1.0;
	}


	bool m_IsTranslucent;
	bool m_ForceOpaque;
	bool m_IsAlphaTested;
	bool m_bIsDepthWriteEnabled;
	bool m_bUsesVertexAndPixelShaders;
};


//-----------------------------------------------------------------------------
// The DX8 implementation of the shader device
//-----------------------------------------------------------------------------
class CShaderDeviceEmpty
{
public:
	CShaderDeviceEmpty() : m_DynamicMesh( true ), m_Mesh( false ) {}

	// Methods of IShaderDevice
	int GetCurrentAdapter() const { return 0; }
	bool IsUsingGraphics() const { return false; }
	void SpewDriverInfo() const;
	ImageFormat GetBackBufferFormat() const { return IMAGE_FORMAT_RGB888; }
	void GetBackBufferDimensions( int& width, int& height ) const;
	const AspectRatioInfo_t &GetAspectRatioInfo( void ) const
	{
		static AspectRatioInfo_t dummy;
		return dummy;
	}
	int  StencilBufferBits() const { return 0; }
	bool IsAAEnabled() const { return false; }
	void Present( ) {}
	void GetWindowSize( int &width, int &height ) const;
	bool AddView( void* hwnd );
	void RemoveView( void* hwnd );
	void SetView( void* hwnd );
	void ReleaseResources( bool bReleaseManagedResources = true );
	void ReacquireResources();
	IMesh* CreateStaticMesh( VertexFormat_t fmt, const char *pTextureBudgetGroup, IMaterial * pMaterial = nullptr, VertexStreamSpec_t *pStreamSpec = nullptr );
	void DestroyStaticMesh( IMesh* mesh );
	IShaderBuffer* CompileShader( const char *pProgram, std::size_t nBufLen, const char *pShaderVersion ) { return nullptr; }
	VertexShaderHandle_t CreateVertexShader( IShaderBuffer* pShaderBuffer ) { return VERTEX_SHADER_HANDLE_INVALID; }
	void DestroyVertexShader( VertexShaderHandle_t hShader ) {}
	GeometryShaderHandle_t CreateGeometryShader( IShaderBuffer* pShaderBuffer ) { return GEOMETRY_SHADER_HANDLE_INVALID; }
	void DestroyGeometryShader( GeometryShaderHandle_t hShader ) {}
	PixelShaderHandle_t CreatePixelShader( IShaderBuffer* pShaderBuffer ) { return PIXEL_SHADER_HANDLE_INVALID; }
	void DestroyPixelShader( PixelShaderHandle_t hShader ) {}
	IVertexBuffer *CreateVertexBuffer( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroup );
	void DestroyVertexBuffer( IVertexBuffer *pVertexBuffer );
	IIndexBuffer *CreateIndexBuffer( ShaderBufferType_t bufferType, MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroup );
	void DestroyIndexBuffer( IIndexBuffer *pIndexBuffer );
	IVertexBuffer *GetDynamicVertexBuffer( int streamID, VertexFormat_t vertexFormat, bool bBuffered );
	IIndexBuffer *GetDynamicIndexBuffer();
	void SetHardwareGammaRamp( float fGamma, float fGammaTVRangeMin, float fGammaTVRangeMax, float fGammaTVExponent, bool bTVEnabled ) {}
	void EnableNonInteractiveMode( MaterialNonInteractiveMode_t mode, ShaderNonInteractiveInfo_t *pInfo ) {}
	void RefreshFrontBufferNonInteractive( ) {}
	void HandleThreadEvent( uint32 threadEvent ) {}
	void OnDebugEvent( const char *pEvent = "" ) {}


private:
	CEmptyMesh m_Mesh;
	CEmptyMesh m_DynamicMesh;
};

static CShaderDeviceEmpty s_ShaderDeviceEmpty;


//-----------------------------------------------------------------------------
// The DX8 implementation of the shader device
//-----------------------------------------------------------------------------
class CShaderDeviceMgrEmpty
{
public:
	// Methods of IAppSystem
	bool Connect( CreateInterfaceFn factory );
	void Disconnect();
	void *QueryInterface( const char *pInterfaceName );
	InitReturnVal_t Init();
	void Shutdown();

public:
	// Methods of IShaderDeviceMgr
	int	 GetAdapterCount() const;
	void GetAdapterInfo( int adapter, MaterialAdapterInfo_t& info ) const;
	bool GetRecommendedVideoConfig( int nAdapter, KeyValues *pKeyValues );
	bool GetRecommendedConfigurationInfo( int nAdapter, int nDXLevel, KeyValues *pKeyValues );
	int	 GetModeCount( int adapter ) const;
	void GetModeInfo( ShaderDisplayMode_t *pInfo, int nAdapter, int mode ) const;
	void GetCurrentModeInfo( ShaderDisplayMode_t* pInfo, int nAdapter ) const;
	bool SetAdapter( int nAdapter, int nFlags );
	CreateInterfaceFn SetMode( void *hWnd, int nAdapter, const ShaderDeviceInfo_t& mode );
	void AddModeChangeCallback( ShaderModeChangeCallbackFunc_t func ) {}
	void RemoveModeChangeCallback( ShaderModeChangeCallbackFunc_t func ) {}
	void AddDeviceDependentObject( IShaderDeviceDependentObject *pObject ) {}
	void RemoveDeviceDependentObject( IShaderDeviceDependentObject *pObject ) {}
};

static CShaderDeviceMgrEmpty s_ShaderDeviceMgrEmpty;


//-----------------------------------------------------------------------------
// The DX8 implementation of the shader API
//-----------------------------------------------------------------------------
class CShaderAPIEmpty
{
	// The facades forward to the (partly private) implementation
	friend class IShaderDynamicAPI;
	friend class IShaderAPI;
	friend class IMaterialSystemHardwareConfig;
	friend class IHardwareConfigInternal;
	friend class IDebugTextureInfo;

public:
	// constructor, destructor
	CShaderAPIEmpty( );
	~CShaderAPIEmpty();

	// IDebugTextureInfo implementation.
public:

	bool IsDebugTextureListFresh( int numFramesAllowed = 1 ) { return false; }
	bool SetDebugTextureRendering( bool bEnable ) { return false; }
	void EnableDebugTextureList( bool bEnable ) {}
	void EnableGetAllTextures( bool bEnable ) {}
	KeyValues* LockDebugTextureList( void ) { return nullptr; }
	void UnlockDebugTextureList( void ) {}
	KeyValues* GetDebugTextureList() { return nullptr; }
	int GetTextureMemoryUsed( IDebugTextureInfo::TextureMemoryType eTextureMemory ) { return 0; }
	void SetVertexShaderViewProj() {}
	void UpdateVertexShaderMatrix( int m ) {}
	void SetVertexShaderModelViewProjAndModelView() {}
	void SetVertexShaderCameraPos() {}
	bool SetSkinningMatrices( const MeshInstanceData_t &instance ) { return false; }
	void BeginGeneratingCSMs() {}
	void EndGeneratingCSMs() {}
	void PerpareForCascadeDraw( int cascade, float fShadowSlopeScaleDepthBias, float fShadowDepthBias ) {}

	// Methods of IShaderDynamicAPI
	void GetBackBufferDimensions( int& width, int& height ) const
	{
		s_ShaderDeviceEmpty.GetBackBufferDimensions( width, height );
	}

	const AspectRatioInfo_t &GetAspectRatioInfo( void ) const
	{
		static AspectRatioInfo_t dummy;
		return dummy;
	}

	// Get the dimensions of the current render target
	void GetCurrentRenderTargetDimensions( int& nWidth, int& nHeight ) const
	{
		s_ShaderDeviceEmpty.GetBackBufferDimensions( nWidth, nHeight );
	}

	// Get the current viewport
	void GetCurrentViewport( int& nX, int& nY, int& nWidth, int& nHeight ) const
	{
		nX = 0;
		nY = 0;
		s_ShaderDeviceEmpty.GetBackBufferDimensions( nWidth, nHeight );
	}

	void GetCurrentColorCorrection( ShaderColorCorrectionInfo_t* pInfo )
	{
		pInfo->m_bIsEnabled = false;
		pInfo->m_nLookupCount = 0;
		pInfo->m_flDefaultWeight = 0.0f;
	}

	// Methods of IShaderAPI
public:
	void SetViewports( int nCount, const ShaderViewport_t* pViewports, bool setImmediately = false );
	int GetViewports( ShaderViewport_t* pViewports, int nMax ) const;
	void ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil, int renderTargetWidth, int renderTargetHeight );
	void ClearBuffersEx( bool bClearRed, bool bClearGreen, bool bClearBlue, bool bClearAlpha,
		unsigned char r, unsigned char g, unsigned char b, unsigned char a );	
	void ClearColor3ub( unsigned char r, unsigned char g, unsigned char b );
	void ClearColor4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a );
	void BindVertexShader( VertexShaderHandle_t hVertexShader ) {}
	void BindGeometryShader( GeometryShaderHandle_t hGeometryShader ) {}
	void BindPixelShader( PixelShaderHandle_t hPixelShader ) {}
	void SetRasterState( const ShaderRasterState_t& state ) {}
	void MarkUnusedVertexFields( unsigned int nFlags, int nTexCoordCount, bool *pUnusedTexCoords ) {}
	bool OwnGPUResources( bool bEnable ) { return false; }
	void OnPresent( void ) {}

	bool DoRenderTargetsNeedSeparateDepthBuffer() const;

	// Used to clear the transition table when we know it's become invalid.
	void ClearSnapshots();

	// Sets the mode...
	bool SetMode( void* hwnd, int nAdapter, const ShaderDeviceInfo_t &info )
	{
		return true;
	}

	void ChangeVideoMode( const ShaderDeviceInfo_t &info )
	{
	}

	// Called when the dx support level has changed
	void DXSupportLevelChanged( int nDXLevel ) {}

	void EnableUserClipTransformOverride( bool bEnable ) {}
	void UserClipTransform( const VMatrix &worldToView ) {}

	// Sets the default *dynamic* state
	void SetDefaultState( );

	// Returns the snapshot id for the shader state
	StateSnapshot_t	 TakeSnapshot( );

	// Returns true if the state snapshot is transparent
	bool IsTranslucent( StateSnapshot_t id ) const;
	bool IsAlphaTested( StateSnapshot_t id ) const;
	bool UsesVertexAndPixelShaders( StateSnapshot_t id ) const;
	bool IsDepthWriteEnabled( StateSnapshot_t id ) const;

	// Gets the vertex format for a set of snapshot ids
	VertexFormat_t ComputeVertexFormat( int numSnapshots, StateSnapshot_t* pIds ) const;

	// Gets the vertex format for a set of snapshot ids
	VertexFormat_t ComputeVertexUsage( int numSnapshots, StateSnapshot_t* pIds ) const;

	// Begins a rendering pass that uses a state snapshot
	void BeginPass( StateSnapshot_t snapshot  );

	// Uses a state snapshot
	void UseSnapshot( StateSnapshot_t snapshot );

	// Use this to get the mesh builder that allows us to modify vertex data
	CMeshBuilder* GetVertexModifyBuilder();

	// Sets the lights
	void SetLightingState( const MaterialLightingState_t &state ) {}
	void SetLights( int nCount, const LightDesc_t *pDesc );
	void SetLightingOrigin( Vector vLightingOrigin );
	void SetAmbientLightCube( Vector4D cube[6] );

	void CopyRenderTargetToTexture( ShaderAPITextureHandle_t texID ){}

	void CopyRenderTargetToTextureEx( ShaderAPITextureHandle_t texID, int nRenderTargetID, Rect_t *pSrcRect, Rect_t *pDstRect ){}

	void CopyTextureToRenderTargetEx( int nRenderTargetID, ShaderAPITextureHandle_t textureHandle, Rect_t *pSrcRect, Rect_t *pDstRect )
	{
	}

	// Set the number of bone weights
	void SetNumBoneWeights( int numBones );
	void EnableHWMorphing( bool bEnable );

	// Gets the dynamic mesh; note that you've got to render the mesh
	// before calling this function a second time. Clients should *not*
	// call DestroyStaticMesh on the mesh returned by this call.
	IMesh* GetDynamicMesh( IMaterial* pMaterial, int nHWSkinBoneCount, bool buffered, IMesh* pVertexOverride, IMesh* pIndexOverride );
	IMesh* GetDynamicMeshEx( IMaterial* pMaterial, VertexFormat_t fmt, int nHWSkinBoneCount, bool buffered, IMesh* pVertexOverride, IMesh* pIndexOverride );

	IMesh* GetFlexMesh();

	// Renders a single pass of a material
	void RenderPass( const unsigned char *pInstanceCommandBuffer, int nPass, int nPassCount );

	// stuff related to matrix stacks
	void MatrixMode( MaterialMatrixMode_t matrixMode );
	void PushMatrix();
	void PopMatrix();
	void LoadMatrix( float *m );
	void LoadBoneMatrix( int boneIndex, const float *m ) {}
	void MultMatrix( float *m );
	void MultMatrixLocal( float *m );
	void GetActualProjectionMatrix( float *pMatrix );
	void GetMatrix( MaterialMatrixMode_t matrixMode, float *dst );
	void LoadIdentity( void );
	void LoadCameraToWorld( void );
	void Ortho( double left, double top, double right, double bottom, double zNear, double zFar );
	void PerspectiveX( double fovx, double aspect, double zNear, double zFar );
	void PerspectiveOffCenterX( double fovx, double aspect, double zNear, double zFar, double bottom, double top, double left, double right );
	void PickMatrix( int x, int y, int width, int height );
	void Rotate( float angle, float x, float y, float z );
	void Translate( float x, float y, float z );
	void Scale( float x, float y, float z );
	void ScaleXY( float x, float y );

	// Fog methods...
	void FogMode( MaterialFogMode_t fogMode );
	void FogStart( float fStart );
	void FogEnd( float fEnd );
	void SetFogZ( float fogZ );
	void FogMaxDensity( float flMaxDensity );
	void GetFogDistances( float *fStart, float *fEnd, float *fFogZ );
	void FogColor3f( float r, float g, float b );
	void FogColor3fv( float const* rgb );
	void FogColor3ub( unsigned char r, unsigned char g, unsigned char b );
	void FogColor3ubv( unsigned char const* rgb );

	void SceneFogColor3ub( unsigned char r, unsigned char g, unsigned char b );
	void SceneFogMode( MaterialFogMode_t fogMode );
	void GetSceneFogColor( unsigned char *rgb );
	MaterialFogMode_t GetSceneFogMode( );
	int GetPixelFogCombo( );

	void SetHeightClipZ( float z ); 
	void SetHeightClipMode( enum MaterialHeightClipMode_t heightClipMode ); 

	void SetClipPlane( int index, const float *pPlane );
	void EnableClipPlane( int index, bool bEnable );

	void SetFastClipPlane( const float *pPlane );
	void EnableFastClip( bool bEnable );
	
	// We use smaller dynamic VBs during level transitions, to free up memory
	int  GetCurrentDynamicVBSize( void );
	void DestroyVertexBuffers( bool bExitingLevel = false );

	// Get stats on GPU memory usage
	void GetGPUMemoryStats( GPUMemoryStats &stats );

	// Sets the vertex and pixel shaders
	void SetVertexShaderIndex( int vshIndex );
	void SetPixelShaderIndex( int pshIndex );

	// Sets the constant register for vertex and pixel shaders
	void SetVertexShaderConstant( int var, float const* pVec, int numConst = 1, bool bForce = false );
	void SetBooleanVertexShaderConstant( int var, BOOL const* pVec, int numConst = 1, bool bForce = false );
	void SetIntegerVertexShaderConstant( int var, int const* pVec, int numConst = 1, bool bForce = false );
	void SetPixelShaderConstant( int var, float const* pVec, int numConst = 1, bool bForce = false );
	void SetBooleanPixelShaderConstant( int var, BOOL const* pVec, int numBools = 1, bool bForce = false );
	void SetIntegerPixelShaderConstant( int var, int const* pVec, int numIntVecs = 1, bool bForce = false );

	void InvalidateDelayedShaderConstants( void );

	// Gamma<->Linear conversions according to the video hardware we're running on
	float GammaToLinear_HardwareSpecific( float fGamma ) const;
	float LinearToGamma_HardwareSpecific( float fLinear ) const;

	//Set's the linear->gamma conversion textures to use for this hardware for both srgb writes enabled and disabled(identity)
	void SetLinearToGammaConversionTextures( ShaderAPITextureHandle_t hSRGBWriteEnabledTexture, ShaderAPITextureHandle_t hIdentityTexture );

	// Cull mode
	void CullMode( MaterialCullMode_t cullMode );
	void FlipCullMode( void );

	// Force writes only when z matches. . . useful for stenciling things out
	// by rendering the desired Z values ahead of time.
	void ForceDepthFuncEquals( bool bEnable );

	// Forces Z buffering on or off
	void OverrideDepthEnable( bool bEnable, bool bDepthWriteEnable, bool bDepthTestEnable = true );
	void OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable );
	void OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable );

	// Sets the shade mode
	void ShadeMode( ShaderShadeMode_t mode );

	// Binds a particular material to render with
	void Bind( IMaterial* pMaterial );

	// Returns the nearest supported format
	ImageFormat GetNearestSupportedFormat( ImageFormat fmt, bool bFilteringRequired = true ) const;
 	ImageFormat GetNearestRenderTargetFormat( ImageFormat fmt ) const;

	// Sets the texture state
	void BindTexture( Sampler_t stage, TextureBindFlags_t nBindFlags, ShaderAPITextureHandle_t textureHandle );

	void BindVertexTexture( VertexTextureSampler_t vtSampler, ShaderAPITextureHandle_t textureHandle );

	void SetRenderTarget( ShaderAPITextureHandle_t colorTextureHandle, ShaderAPITextureHandle_t depthTextureHandle )
	{
	}

	void SetRenderTargetEx( int nRenderTargetID, ShaderAPITextureHandle_t colorTextureHandle, ShaderAPITextureHandle_t depthTextureHandle )
	{
	}

	// Indicates we're going to be modifying this texture
	// TexImage2D, TexSubImage2D, TexWrap, TexMinFilter, and TexMagFilter
	// all use the texture specified by this function.
	void ModifyTexture( ShaderAPITextureHandle_t textureHandle );

	// Texture management methods
	void TexImage2D( int level, int cubeFace, ImageFormat dstFormat, int zOffset, int width, int height, 
							 ImageFormat srcFormat, bool bSrcIsTiled, void *imageData );
	void TexSubImage2D( int level, int cubeFace, int xOffset, int yOffset, int zOffset, int width, int height,
							 ImageFormat srcFormat, int srcStride, bool bSrcIsTiled, void *imageData );

	bool TexLock( int level, int cubeFaceID, int xOffset, int yOffset, 
									int width, int height, CPixelWriter& writer );
	void TexUnlock( );
	
	void UpdateTexture( int xOffset, int yOffset, int w, int h, ShaderAPITextureHandle_t hDstTexture, ShaderAPITextureHandle_t hSrcTexture );

	void *LockTex( ShaderAPITextureHandle_t hTexture );
	void UnlockTex( ShaderAPITextureHandle_t hTexture );

	void* GetD3DTexturePtr( ShaderAPITextureHandle_t hTexture ) { return nullptr; }
	bool IsStandardTextureHandleValid( StandardTextureId_t textureId ) { return false; }

	// These are bound to the texture, not the texture environment
	void TexMinFilter( ShaderTexFilterMode_t texFilterMode );
	void TexMagFilter( ShaderTexFilterMode_t texFilterMode );
	void TexWrap( ShaderTexCoordComponent_t coord, ShaderTexWrapMode_t wrapMode );
	void TexSetPriority( int priority );	

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
	ShaderAPITextureHandle_t CreateDepthTexture( ImageFormat renderFormat, int width, int height, const char *pDebugName, bool bTexture, bool bAliasDepthTextureOverSceneDepthX360 );
	void DeleteTexture( ShaderAPITextureHandle_t textureHandle );
	bool IsTexture( ShaderAPITextureHandle_t textureHandle );
	bool IsTextureResident( ShaderAPITextureHandle_t textureHandle );

	// stuff that isn't to be used from within a shader
	void ClearBuffersObeyStencil( bool bClearColor, bool bClearDepth );
	void ClearBuffersObeyStencilEx( bool bClearColor, bool bClearAlpha, bool bClearDepth );
	void PerformFullScreenStencilOperation( void );
	void ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture = nullptr );
	void ReadPixelsAsync( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture = nullptr, CThreadEvent *pPixelsReadEvent = nullptr );
	void ReadPixelsAsyncGetResult( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, CThreadEvent *pGetResultEvent = nullptr );
	void ReadPixels( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *data, ImageFormat dstFormat, int nDstStride );

	// Selection mode methods
	int SelectionMode( bool selectionMode );
	void SelectionBuffer( unsigned int* pBuffer, int size );
	void ClearSelectionNames( );
	void LoadSelectionName( int name );
	void PushSelectionName( int name );
	void PopSelectionName();

	void FlushHardware();
	void ResetRenderState( bool bFullReset = true );

	void SetScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom, const bool bEnableScissor );

	// Can we download textures?
	bool CanDownloadTextures() const;

	// Board-independent calls, here to unify how shaders set state
	// Implementations should chain back to IShaderUtil->BindTexture(), etc.

	// Use this to begin and end the frame
	void BeginFrame();
	void EndFrame();

	// returns current time
	double CurrentTime() const;

	// Get the current camera position in world space.
	void GetWorldSpaceCameraPosition( float* pPos ) const;
	void GetWorldSpaceCameraDirection( float* pDir ) const;

	// Members of IMaterialSystemHardwareConfig
	bool HasDestAlphaBuffer() const;
	bool HasStencilBuffer() const;
	int  MaxViewports() const;
	void OverrideStreamOffsetSupport( bool bOverrideEnabled, bool bEnableSupport ) {}
	ShadowFilterMode_t GetShadowFilterMode( bool bForceLowQualityShadows, bool bPS30 ) const;
	int  StencilBufferBits() const;
	int	 GetFrameBufferColorDepth() const;
	int  GetSamplerCount() const;
	int  GetVertexSamplerCount() const;
	bool HasSetDeviceGammaRamp() const;
	bool SupportsCompressedTextures() const;
	VertexCompressionType_t SupportsCompressedVertices() const;
	bool SupportsStaticControlFlow() const;
	int  MaximumAnisotropicLevel() const;
	int  MaxTextureWidth() const;
	int  MaxTextureHeight() const;
	int  MaxTextureAspectRatio() const;
	int  GetDXSupportLevel() const;
	int GetMinDXSupportLevel() const { return 90; }
	bool SupportsShadowDepthTextures( void ) const
	{
		return false;
	}

	ImageFormat GetShadowDepthTextureFormat( void ) const
	{
		return IMAGE_FORMAT_RGBA8888;
	}

	ImageFormat GetHighPrecisionShadowDepthTextureFormat( void ) const
	{
		return IMAGE_FORMAT_RGBA8888;
	}

	ImageFormat GetNullTextureFormat( void ) const
	{
		return IMAGE_FORMAT_RGBA8888;
	}

	const char *GetShaderDLLName() const
	{
		return "UNKNOWN";
	}
	int	 TextureMemorySize() const;
	bool SupportsMipmappedCubemaps() const;
	int  GetTextureStageCount() const;
	int	 NumVertexShaderConstants() const;
	int	 NumBooleanVertexShaderConstants() const;
	int	 NumIntegerVertexShaderConstants() const;
	int	 NumPixelShaderConstants() const;
	int	 MaxNumLights() const;
	bool SupportsHardwareLighting() const;
	int	 MaxBlendMatrices() const;
	int	 MaxBlendMatrixIndices() const;
	int	 MaxVertexShaderBlendMatrices() const;
	int	 MaxUserClipPlanes() const;
	bool UseFastClipping() const
	{
		return false;
	}
	bool SpecifiesFogColorInLinearSpace() const;
	bool SupportsSRGB() const;
	bool FakeSRGBWrite() const;
	bool CanDoSRGBReadFromRTs() const;
	bool SupportsGLMixedSizeTargets() const;

	const char *GetHWSpecificShaderDLLName() const;
	bool NeedsAAClamp() const
	{
		return false;
	}
	int MaxHWMorphBatchCount() const { return 0; }

	// This is the max dx support level supported by the card
	int	 GetMaxDXSupportLevel() const;

	bool ReadPixelsFromFrontBuffer() const;
	bool PreferDynamicTextures() const;
	void ForceHardwareSync( void );
	
	int GetCurrentNumBones( void ) const;
	bool IsHWMorphingEnabled( void ) const;
	void GetDX9LightState( LightState_t *state ) const;
	MaterialFogMode_t GetCurrentFogType( void ) const;

	void RecordString( const char *pStr );

	void EvictManagedResources();

	// Gets the lightmap dimensions
	void GetLightmapDimensions( int *w, int *h );

	void SyncToken( const char *pToken );

	// Setup standard vertex shader constants (that don't change)
	// This needs to be called anytime that overbright changes.
	void SetStandardVertexShaderConstants( float fOverbright )
	{
	}
	
	// Level of anisotropic filtering
	void SetAnisotropicLevel( int nAnisotropyLevel );

	bool SupportsHDR() const
	{
		return false;
	}
	HDRType_t GetHDRType() const
	{
		return HDR_TYPE_NONE;
	}
	HDRType_t GetHardwareHDRType() const
	{
		return HDR_TYPE_NONE;
	}
	bool NeedsATICentroidHack() const
	{
		return false;
	}
	bool SupportsColorOnSecondStream() const
	{
		return false;
	}
	bool SupportsStaticPlusDynamicLighting() const
	{
		return false;
	}
	bool SupportsStreamOffset() const
	{
		return false;
	}
	void SetDefaultDynamicState()
	{
	}
	void CommitPixelShaderLighting( int pshReg )
	{
	}

	ShaderAPIOcclusionQuery_t CreateOcclusionQueryObject( void )
	{
		return INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE;
	}

	void DestroyOcclusionQueryObject( ShaderAPIOcclusionQuery_t handle )
	{
	}

	void BeginOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t handle )
	{
	}

	void EndOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t handle )
	{
	}

	int OcclusionQuery_GetNumPixelsRendered( ShaderAPIOcclusionQuery_t handle, bool bFlush )
	{
		return 0;
	}


	void AcquireThreadOwnership() {}
	void ReleaseThreadOwnership() {}

	bool SupportsBorderColor() const { return false; }
	bool SupportsFetch4() const { return false; }
	void EnableBuffer2FramesAhead( bool bEnable ) {}
	float GetShadowDepthBias() const { return 0.0f; }
	float GetShadowSlopeScaleDepthBias() const { return 0.0f; }
	bool PreferZPrepass() const { return false; }
	bool SuppressPixelShaderCentroidHackFixup() const { return true; }
	bool PreferTexturesInHWMemory() const { return false; }
	bool PreferHardwareSync() const { return false; }
	bool IsUnsupported() const { return false; }

	void SetDepthFeatheringShaderConstants( int iConstant, float fDepthBlendScale ) {}

	TessellationMode_t GetTessellationMode() const { return TESSELLATION_MODE_DISABLED; }

	void SetPixelShaderFogParams( int reg )	{}

	bool InFlashlightMode() const
	{
		return false;
	}
	bool IsRenderingPaint() const
	{
		return false;
	}
	bool InEditorMode() const
	{
		return false;
	}

	// Binds a standard texture
	void BindStandardTexture( Sampler_t stage, TextureBindFlags_t nBindFlags, StandardTextureId_t id )
	{
	}

	void BindStandardVertexTexture( VertexTextureSampler_t stage, StandardTextureId_t id )
	{
	}

	void GetStandardTextureDimensions( int *pWidth, int *pHeight, StandardTextureId_t id )
	{
		*pWidth = *pHeight = 0;
	}

	float GetSubDHeight()
	{
		return 0.0f;
	}

	bool IsStereoActiveThisFrame() const
	{
		return false;
	}

	void SetFlashlightState( const FlashlightState_t &state, const VMatrix &worldToTexture )
	{
	}

	void SetFlashlightStateEx( const FlashlightState_t &state, const VMatrix &worldToTexture, ITexture *pFlashlightDepthTexture )
	{
	}
	
	bool IsCascadedShadowMapping() const
	{
		return false;
	}
	
	void SetCascadedShadowMapping( bool bEnable )
	{
		bEnable;
	}

	void SetCascadedShadowMappingState( const CascadedShadowMappingState_t &state, ITexture *pDepthTextureAtlas )
	{
		state, pDepthTextureAtlas;
	}

	const CascadedShadowMappingState_t &GetCascadedShadowMappingState( ITexture **pDepthTextureAtlas, bool bLightMapScale = false ) const
	{
		static CascadedShadowMappingState_t dummyState;
		return dummyState;
	}

	const FlashlightState_t &GetFlashlightState( VMatrix &worldToTexture ) const 
	{
		static FlashlightState_t  blah;
		return blah;
	}

	const FlashlightState_t &GetFlashlightStateEx( VMatrix &worldToTexture, ITexture **pFlashlightDepthTexture ) const 
	{
		static FlashlightState_t  blah;
		return blah;
	}

	void GetFlashlightShaderInfo( bool *pShadowsEnabled, bool *pUberLight ) const
	{
		*pShadowsEnabled = false;
		*pUberLight = false;
	}
	
	float GetFlashlightAmbientOcclusion( ) const { return 1.0f; }

	void ClearVertexAndPixelShaderRefCounts()
	{
	}

	void PurgeUnusedVertexAndPixelShaders()
	{
	}

	void DrawInstances( int nCount, const MeshInstanceData_t *pInstanceData ) {}

	bool IsAAEnabled() const
	{
		return false;
	}

	int GetVertexTextureCount() const
	{
		return 0;
	}

	int GetMaxVertexTextureDimension() const
	{
		return 0;
	}

	int  MaxTextureDepth() const
	{
		return 0;
	}

	// Sets morph target factors
	void SetFlexWeights( int nFirstWeight, int nCount, const MorphWeight_t* pWeights )
	{
	}

	// NOTE: Stuff after this is added after shipping HL2.
	ITexture *GetRenderTargetEx( int nRenderTargetID ) const
	{
		return nullptr;
	}

	void SetToneMappingScaleLinear( const Vector &scale )
	{
	}

	const Vector &GetToneMappingScaleLinear( void ) const
	{
		static Vector dummy;
		return dummy;
	}

	// For dealing with device lost in cases where SwapBuffers isn't called all the time (Hammer)
	void HandleDeviceLost()
	{
	}

	void EnableLinearColorSpaceFrameBuffer( bool bEnable )
	{
	}

	// Lets the shader know about the full-screen texture so it can 
	void SetFullScreenTextureHandle( ShaderAPITextureHandle_t h )
	{
	}

	void SetFloatRenderingParameter(int parm_number, float value)
	{
	}

	void SetIntRenderingParameter(int parm_number, int value)
	{
	}
	void SetTextureRenderingParameter(int parm_number, ITexture *pTexture)
	{
	}
	void SetVectorRenderingParameter(int parm_number, Vector const &value)
	{
	}

	float GetFloatRenderingParameter(int parm_number) const
	{
		return 0;
	}

	int GetIntRenderingParameter(int parm_number) const
	{
		return 0;
	}

	ITexture *GetTextureRenderingParameter(int parm_number) const
	{
		return nullptr;
	}

	Vector GetVectorRenderingParameter(int parm_number) const
	{
		return Vector(0,0,0);
	}

	// Methods related to stencil
	void SetStencilState( const ShaderStencilState_t &state )
	{
	}

	void ClearStencilBufferRectangle( int xmin, int ymin, int xmax, int ymax,int value)
	{
	}

	void GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices )
	{
		*pMaxVerts = 32768;
		*pMaxIndices = 32768;
	}

	// Returns the max possible vertices + indices to render in a single draw call
	int GetMaxVerticesToRender( IMaterial *pMaterial )
	{
		return 32768;
	}

	int GetMaxIndicesToRender( )
	{
		return 32768;
	}
	int CompareSnapshots( StateSnapshot_t snapshot0, StateSnapshot_t snapshot1 ) { return 0; }

	void DisableAllLocalLights() {}

	bool SupportsMSAAMode( int nMSAAMode ) { return false; }

	bool SupportsCSAAMode( int nNumSamples, int nQualityLevel ) { return false; }

	// Hooks for firing PIX events from outside the Material System...
	void BeginPIXEvent( unsigned long color, const char *szName ) {}
	void EndPIXEvent() {}
	void SetPIXMarker( unsigned long color, const char *szName ) {}

	void ComputeVertexDescription( unsigned char* pBuffer, VertexFormat_t vertexFormat, MeshDesc_t& desc ) const {}
	int VertexFormatSize( VertexFormat_t vertexFormat ) const { return 0; }

	bool SupportsShadowDepthTextures() { return false; }

	bool SupportsFetch4() { return false; }

	int NeedsShaderSRGBConversion(void) const { return 0; }
	bool UsesSRGBCorrectBlending() const { return false; }

	bool HasFastVertexTextures() const { return false; }
	bool ActualHasFastVertexTextures() const { return false; }

	void SetShadowDepthBiasFactors( float fShadowSlopeScaleDepthBias, float fShadowDepthBias ) {}

	void SetDisallowAccess( bool ) {}
	void EnableShaderShaderMutex( bool ) {}
	void ShaderLock() {}
	void ShaderUnlock() {}

// ------------ New Vertex/Index Buffer interface ----------------------------
	void BindVertexBuffer( int streamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions1 )
	{
	}
	void BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes )
	{
	}
	void Draw( MaterialPrimitiveType_t primitiveType, int firstIndex, int numIndices )
	{
	}
// ------------ End ----------------------------

	int  GetVertexBufferCompression( void ) const { return 0; };

	bool ShouldWriteDepthToDestAlpha( void ) const { return false; };
	bool SupportsHDRMode( HDRType_t nMode ) const { return false; }
	bool IsDX10Card() const { return false; };

	void PushDeformation( const DeformationBase_t *pDeformation )
	{
	}

	void PopDeformation( )
	{
	}

	int GetNumActiveDeformations( ) const
	{
		return 0;
	}

	// for shaders to set vertex shader constants. returns a packed state which can be used to set the dynamic combo
	int GetPackedDeformationInformation( int nMaskOfUnderstoodDeformations,
										 float *pConstantValuesOut,
										 int nBufferSize,
										 int nMaximumDeformations,
										 int *pNumDefsOut ) const
	{
		*pNumDefsOut = 0;
		return 0;
	}

	void SetStandardTextureHandle(StandardTextureId_t,ShaderAPITextureHandle_t)
	{
	}

	void ExecuteCommandBuffer( uint8 *pData )
	{
	}
	bool GetHDREnabled( void ) const { return true; }
	void SetHDREnabled( bool bEnable ) {}

	void SetTextureFilterMode( Sampler_t sampler, TextureFilterMode_t nMode )
	{
	}

	void SetScreenSizeForVPOS( int pshReg ) {}
	void SetVSNearAndFarZ( int vshReg ) {}
	float GetFarZ() { return 1000.0f; }
	
	void EnableSinglePassFlashlightMode( bool bEnable ) {}
	bool SinglePassFlashlightModeEnabled( void ) { return false; }
	void FlipCulling( bool bFlipCulling ) {}

	void UpdateGameTime( float flTime ) {}
	
	bool IsStereoSupported() const { return false; }
	void UpdateStereoTexture( ShaderAPITextureHandle_t texHandle, bool *pStereoActiveThisFrame ) {}
	void SetSRGBWrite( bool bState ) {}

	void PrintfVA( char *fmt, std::va_list vargs ) {}
	void Printf( char *fmt, ... ) {}
	float Knob( char *knobname, float *setvalue = nullptr ) { return 0.0f; };
	void AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics ) {}
	
	void SpinPresent( uint nFrames ){}
	void AntiAliasingHint( int nHint ) {}

	bool SupportsCascadedShadowMapping() const { return true; }
	CSMQualityMode_t GetCSMQuality() const { return CSMQUALITY_VERY_LOW; }
	bool SupportsBilinearPCFSampling() const { return true; }
	CSMShaderMode_t GetCSMShaderMode( CSMQualityMode_t nQualityLevel ) const { nQualityLevel; return CSMSHADERMODE_LOW_OR_VERY_LOW; }
	void SetCSMAccurateBlending( bool bEnable ) {}
	bool GetCSMAccurateBlending() const { return true; }

	bool SupportsResolveDepth() const {	return false; }
	bool HasFullResolutionDepthTexture() const { return false; }

private:
	enum
	{
		TRANSLUCENT = 0x1,
		ALPHATESTED = 0x2,
		VERTEX_AND_PIXEL_SHADERS = 0x4,
		DEPTHWRITE = 0x8,
	};

	CEmptyMesh m_Mesh;

	void EnableAlphaToCoverage() {} ;
	void DisableAlphaToCoverage() {} ;

	ImageFormat GetShadowDepthTextureFormat() { return IMAGE_FORMAT_UNKNOWN; };
	ImageFormat GetHighPrecisionShadowDepthTextureFormat() { return IMAGE_FORMAT_UNKNOWN; };
	ImageFormat GetNullTextureFormat() { return IMAGE_FORMAT_UNKNOWN; };

	float GetLightMapScaleFactor() const { return 1.0f; }
	ShaderAPITextureHandle_t FindTexture( const char *pDebugName ) { return INVALID_SHADERAPI_TEXTURE_HANDLE;};
	void GetTextureDimensions( ShaderAPITextureHandle_t hTexture, int &nWidth, int &nHeight, int &nDepth ) {};

	ShaderAPITextureHandle_t GetStandardTextureHandle(StandardTextureId_t id) { return INVALID_SHADERAPI_TEXTURE_HANDLE; }
};


//-----------------------------------------------------------------------------
// Class Factory
//-----------------------------------------------------------------------------

static CShaderAPIEmpty g_ShaderAPIEmpty;
static CShaderShadowEmpty g_ShaderShadow;


//-----------------------------------------------------------------------------
// The main GL Shader util interface
//-----------------------------------------------------------------------------
extern IShaderUtil* g_pShaderUtil;


//-----------------------------------------------------------------------------
// The shader API interfaces are stateless facades that forward to the one implementation
//-----------------------------------------------------------------------------
static IShaderAPI s_ShaderAPIFacade;
static IShaderDevice s_ShaderDeviceFacade;
static IShaderShadow s_ShaderShadowFacade;
static IShaderDeviceMgr s_ShaderDeviceMgrFacade;
static IHardwareConfigInternal s_HardwareConfigFacade;
static IDebugTextureInfo s_DebugTextureInfoFacade;

static CShaderAPIEmpty &ShaderAPIImpl() { return g_ShaderAPIEmpty; }
static CShaderDeviceEmpty &ShaderDeviceImpl() { return s_ShaderDeviceEmpty; }
static CShaderShadowEmpty &ShaderShadowImpl() { return g_ShaderShadow; }
static CShaderDeviceMgrEmpty &ShaderDeviceMgrImpl() { return s_ShaderDeviceMgrEmpty; }
static CShaderAPIEmpty &HardwareConfigImpl() { return g_ShaderAPIEmpty; }
static CShaderAPIEmpty &DebugTextureInfoImpl() { return g_ShaderAPIEmpty; }

#include "../shaderapi_facade/ishaderdynamicapi.inl"
#include "../shaderapi_facade/ishaderapi.inl"
#include "../shaderapi_facade/ishaderdevice.inl"
#include "../shaderapi_facade/ishadershadow.inl"
#include "../shaderapi_facade/ishaderdevicemgr.inl"
#include "../shaderapi_facade/hardwareconfig.inl"
#include "../shaderapi_facade/idebugtextureinfo.inl"

// The empty shader API registers no IVBAllocTracker (callers check for its absence); these definitions
// only satisfy the linker.
void IVBAllocTracker::CountVB( void *, bool, int, int, VertexFormat_t ) {}
void IVBAllocTracker::UnCountVB( void * ) {}
bool IVBAllocTracker::TrackMeshAllocations( const char * ) { return false; }

static InterfaceReg s_Reg_ShaderDevice( []() -> void * { return &s_ShaderDeviceFacade; }, SHADER_DEVICE_INTERFACE_VERSION );
static InterfaceReg s_Reg_ShaderAPI( []() -> void * { return &s_ShaderAPIFacade; }, SHADERAPI_INTERFACE_VERSION );
static InterfaceReg s_Reg_ShaderShadow( []() -> void * { return &s_ShaderShadowFacade; }, SHADERSHADOW_INTERFACE_VERSION );
static InterfaceReg s_Reg_ShaderDeviceMgr( []() -> void * { return &s_ShaderDeviceMgrFacade; }, SHADER_DEVICE_MGR_INTERFACE_VERSION );
static InterfaceReg s_Reg_HardwareConfig( []() -> void * { return static_cast< IMaterialSystemHardwareConfig * >( &s_HardwareConfigFacade ); }, MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION );
static InterfaceReg s_Reg_DebugTextureInfo( []() -> void * { return &s_DebugTextureInfoFacade; }, DEBUG_TEXTURE_INFO_VERSION );


//-----------------------------------------------------------------------------
//
// CShaderDeviceMgrEmpty
//
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrEmpty::Connect( CreateInterfaceFn factory )
{
	// So others can access it
	g_pShaderUtil = (IShaderUtil*)factory( SHADER_UTIL_INTERFACE_VERSION, nullptr );

	return true;
}

void CShaderDeviceMgrEmpty::Disconnect()
{
	g_pShaderUtil = nullptr;
}

void *CShaderDeviceMgrEmpty::QueryInterface( const char *pInterfaceName )
{
	if ( !V_stricmp( pInterfaceName, SHADER_DEVICE_MGR_INTERFACE_VERSION ) ||
		 !V_stricmp( pInterfaceName, MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION ) )
		return Sys_GetFactoryThis()( pInterfaceName, nullptr );
	return nullptr;
}

InitReturnVal_t CShaderDeviceMgrEmpty::Init()
{
	return INIT_OK;
}

void CShaderDeviceMgrEmpty::Shutdown()
{

}

// Sets the adapter
bool CShaderDeviceMgrEmpty::SetAdapter( int nAdapter, int nFlags )
{
	return true;
}

// FIXME: Is this a public interface? Might only need to be private to shaderapi
CreateInterfaceFn CShaderDeviceMgrEmpty::SetMode( void *hWnd, int nAdapter, const ShaderDeviceInfo_t& mode ) 
{
	return Sys_GetFactoryThis();
}

// Gets the number of adapters...
int	 CShaderDeviceMgrEmpty::GetAdapterCount() const
{
	return 0;
}

bool CShaderDeviceMgrEmpty::GetRecommendedVideoConfig( int nAdapter, KeyValues *pKeyValues ) 
{
	return true;
}

bool CShaderDeviceMgrEmpty::GetRecommendedConfigurationInfo( int nAdapter, int nDXLevel, KeyValues *pKeyValues ) 
{
	return true;
}

// Returns info about each adapter
void CShaderDeviceMgrEmpty::GetAdapterInfo( int adapter, MaterialAdapterInfo_t& info ) const
{
	std::memset( &info, 0, sizeof( info ) );
	info.m_nDXSupportLevel = 90;
}

// Returns the number of modes
int	 CShaderDeviceMgrEmpty::GetModeCount( int nAdapter ) const
{
	return 0;
}

// Returns mode information..
void CShaderDeviceMgrEmpty::GetModeInfo( ShaderDisplayMode_t *pInfo, int nAdapter, int nMode ) const
{
}

void CShaderDeviceMgrEmpty::GetCurrentModeInfo( ShaderDisplayMode_t* pInfo, int nAdapter ) const
{
}


//-----------------------------------------------------------------------------
//
// Shader device empty
//
//-----------------------------------------------------------------------------
void CShaderDeviceEmpty::GetWindowSize( int &width, int &height ) const
{
	width = 0;
	height = 0;
}

void CShaderDeviceEmpty::GetBackBufferDimensions( int& width, int& height ) const
{
	width = 1024;
	height = 768;
}

// Use this to spew information about the 3D layer 
void CShaderDeviceEmpty::SpewDriverInfo() const
{
	Warning("Empty shader\n");
}

// Creates/ destroys a child window
bool CShaderDeviceEmpty::AddView( void* hwnd )
{
	return true;
}

void CShaderDeviceEmpty::RemoveView( void* hwnd )
{
}

// Activates a view
void CShaderDeviceEmpty::SetView( void* hwnd )
{
}

void CShaderDeviceEmpty::ReleaseResources( bool bReleaseManagedResources /*= true*/ )
{
}

void CShaderDeviceEmpty::ReacquireResources()
{
}

// Creates/destroys Mesh
IMesh* CShaderDeviceEmpty::CreateStaticMesh( VertexFormat_t fmt, const char *pTextureBudgetGroup, IMaterial * pMaterial, VertexStreamSpec_t *pStreamSpec )
{
	return &m_Mesh;
}

void CShaderDeviceEmpty::DestroyStaticMesh( IMesh* mesh )
{
}

// Creates/destroys static vertex + index buffers
IVertexBuffer *CShaderDeviceEmpty::CreateVertexBuffer( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pTextureBudgetGroup )
{
	return ( type == SHADER_BUFFER_TYPE_STATIC || type == SHADER_BUFFER_TYPE_STATIC_TEMP ) ? &m_Mesh : &m_DynamicMesh;
}

void CShaderDeviceEmpty::DestroyVertexBuffer( IVertexBuffer *pVertexBuffer )
{

}

IIndexBuffer *CShaderDeviceEmpty::CreateIndexBuffer( ShaderBufferType_t bufferType, MaterialIndexFormat_t fmt, int nIndexCount, const char *pTextureBudgetGroup )
{
	switch( bufferType )
	{
	case SHADER_BUFFER_TYPE_STATIC:
	case SHADER_BUFFER_TYPE_STATIC_TEMP:
		return &m_Mesh;
	default:
		contract_assert( 0 );
	case SHADER_BUFFER_TYPE_DYNAMIC:
	case SHADER_BUFFER_TYPE_DYNAMIC_TEMP:
		return &m_DynamicMesh;
	}
}

void CShaderDeviceEmpty::DestroyIndexBuffer( IIndexBuffer *pIndexBuffer )
{

}

IVertexBuffer *CShaderDeviceEmpty::GetDynamicVertexBuffer( int streamID, VertexFormat_t vertexFormat, bool bBuffered )
{
	return &m_DynamicMesh;
}

IIndexBuffer *CShaderDeviceEmpty::GetDynamicIndexBuffer()
{
	return &m_Mesh;
}



//-----------------------------------------------------------------------------
//
// The empty mesh...
//
//-----------------------------------------------------------------------------
CEmptyMesh::CEmptyMesh( bool bIsDynamic ) : IMesh( MeshKind::EmptyMesh ), m_bIsDynamic( bIsDynamic )
{
	m_pVertexMemory = new unsigned char[VERTEX_BUFFER_SIZE];
}

CEmptyMesh::~CEmptyMesh()
{
	delete[] m_pVertexMemory;
}

bool CEmptyMesh::Lock( int nMaxIndexCount, bool bAppend, IndexDesc_t& desc )
{
	static int s_BogusIndex;
	desc.m_pIndices = (unsigned short*)&s_BogusIndex;
	desc.m_nIndexSize = 0;
	desc.m_nFirstIndex = 0;
	desc.m_nOffset = 0;
	return true;
}

void CEmptyMesh::Unlock( int nWrittenIndexCount, IndexDesc_t& desc )
{
}

void CEmptyMesh::ModifyBegin( bool bReadOnly, int nFirstIndex, int nIndexCount, IndexDesc_t& desc )
{
	Lock( nIndexCount, false, desc );
}

void CEmptyMesh::ModifyEnd( IndexDesc_t& desc )
{
}

void CEmptyMesh::Spew( int nIndexCount, const IndexDesc_t & desc )
{
}

void CEmptyMesh::ValidateData( int nIndexCount, const IndexDesc_t &desc )
{
}

bool CEmptyMesh::Lock( int nVertexCount, bool bAppend, VertexDesc_t &desc )
{
	// Who cares about the data?
	desc.m_pPosition = (float*)m_pVertexMemory;
	desc.m_pNormal = (float*)m_pVertexMemory;
	desc.m_pColor = m_pVertexMemory;

	int i;
	for ( i = 0; i < VERTEX_MAX_TEXTURE_COORDINATES; ++i)
	{
		desc.m_pTexCoord[i] = (float*)m_pVertexMemory;
	}

	desc.m_pBoneWeight = (float*)m_pVertexMemory;
	desc.m_pBoneMatrixIndex = (unsigned char*)m_pVertexMemory;
	desc.m_pTangentS = (float*)m_pVertexMemory;
	desc.m_pTangentT = (float*)m_pVertexMemory;
	desc.m_pUserData = (float*)m_pVertexMemory;
	desc.m_pSpecular = m_pVertexMemory;
	desc.m_pWrinkle  = (float*)m_pVertexMemory;
	desc.m_NumBoneWeights = 2;

	desc.m_VertexSize_Position = 0;
	desc.m_VertexSize_BoneWeight = 0;
	desc.m_VertexSize_BoneMatrixIndex = 0;
	desc.m_VertexSize_Normal = 0;
	desc.m_VertexSize_Color = 0;
	for( i=0; i < VERTEX_MAX_TEXTURE_COORDINATES; i++ )
	{
		desc.m_VertexSize_TexCoord[i] = 0;
	}
	desc.m_VertexSize_TangentS = 0;
	desc.m_VertexSize_TangentT = 0;
	desc.m_VertexSize_UserData = 0;
	desc.m_VertexSize_Specular = 0;
	desc.m_VertexSize_Wrinkle = 0;
	desc.m_ActualVertexSize = 0;	// Size of the vertices.. Some of the m_VertexSize_ elements above

	desc.m_nFirstVertex = 0;
	desc.m_nOffset = 0;
	return true;
}

void CEmptyMesh::Unlock( int nVertexCount, VertexDesc_t &desc )
{
}

void CEmptyMesh::Spew( int nVertexCount, const VertexDesc_t &desc ) 
{
}

void CEmptyMesh::ValidateData( int nVertexCount, const VertexDesc_t & desc )
{
}

void CEmptyMesh::LockMesh( int numVerts, int numIndices, MeshDesc_t& desc, MeshBuffersAllocationSettings_t *pSettings )
{
	Lock( numVerts, false, *static_cast<VertexDesc_t*>( &desc ) );
	Lock( numIndices, false, *static_cast<IndexDesc_t*>( &desc ) );
}

void CEmptyMesh::UnlockMesh( int numVerts, int numIndices, MeshDesc_t& desc )
{
}

void CEmptyMesh::ModifyBeginEx( bool bReadOnly, int firstVertex, int numVerts, int firstIndex, int numIndices, MeshDesc_t& desc )
{
	Lock( numVerts, false, *static_cast<VertexDesc_t*>( &desc ) );
	Lock( numIndices, false, *static_cast<IndexDesc_t*>( &desc ) );
}

void CEmptyMesh::ModifyBegin( int firstVertex, int numVerts, int firstIndex, int numIndices, MeshDesc_t& desc )
{
	ModifyBeginEx( false, firstVertex, numVerts, firstIndex, numIndices, desc );
}

void CEmptyMesh::ModifyEnd( MeshDesc_t& desc )
{
}

// returns the # of vertices (static meshes only)
int CEmptyMesh::VertexCount() const
{
	return 0;
}

// Sets the primitive type
void CEmptyMesh::SetPrimitiveType( MaterialPrimitiveType_t type )
{
}

// Draws the entire mesh
void CEmptyMesh::Draw( int firstIndex, int numIndices )
{
}

void CEmptyMesh::Draw(CPrimList *pPrims, int nPrims)
{
}

// Copy verts and/or indices to a mesh builder. This only works for temp meshes!
void CEmptyMesh::CopyToMeshBuilder( 
	int iStartVert,		// Which vertices to copy.
	int nVerts, 
	int iStartIndex,	// Which indices to copy.
	int nIndices, 
	int indexOffset,	// This is added to each index.
	CMeshBuilder &builder )
{
}

// Spews the mesh data
void CEmptyMesh::Spew( int numVerts, int numIndices, const MeshDesc_t & desc )
{
}

void CEmptyMesh::ValidateData( int numVerts, int numIndices, const MeshDesc_t & desc )
{
}

// gets the associated material
IMaterial* CEmptyMesh::GetMaterial()
{
	// umm. this don't work none
	contract_assert( 0 );
	return 0;
}

//-----------------------------------------------------------------------------
// The shader shadow interface
//-----------------------------------------------------------------------------
CShaderShadowEmpty::CShaderShadowEmpty()
{
	m_IsTranslucent = false;
	m_ForceOpaque = false;
	m_IsAlphaTested = false;
	m_bIsDepthWriteEnabled = true;
	m_bUsesVertexAndPixelShaders = false;
}

CShaderShadowEmpty::~CShaderShadowEmpty()
{
}

// Sets the default *shadow* state
void CShaderShadowEmpty::SetDefaultState()
{
	m_IsTranslucent = false;
	m_ForceOpaque = false;
	m_IsAlphaTested = false;
	m_bIsDepthWriteEnabled = true;
	m_bUsesVertexAndPixelShaders = false;
}

// Methods related to depth buffering
void CShaderShadowEmpty::DepthFunc( ShaderDepthFunc_t depthFunc )
{
}

void CShaderShadowEmpty::EnableDepthWrites( bool bEnable )
{
	m_bIsDepthWriteEnabled = bEnable;
}

void CShaderShadowEmpty::EnableDepthTest( bool bEnable )
{
}

void CShaderShadowEmpty::EnablePolyOffset( PolygonOffsetMode_t nOffsetMode )
{
}

// Suppresses/activates color writing 
void CShaderShadowEmpty::EnableColorWrites( bool bEnable )
{
}

// Suppresses/activates alpha writing 
void CShaderShadowEmpty::EnableAlphaWrites( bool bEnable )
{
}

// Methods related to alpha blending
void CShaderShadowEmpty::EnableBlending( bool bEnable )
{
	m_IsTranslucent = bEnable;
	m_ForceOpaque = false;
}

void CShaderShadowEmpty::EnableBlendingForceOpaque( bool bEnable )
{
	m_IsTranslucent = bEnable;
	m_ForceOpaque = true;
}

void CShaderShadowEmpty::BlendFunc( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor )
{
}

// Alpha testing
void CShaderShadowEmpty::EnableAlphaTest( bool bEnable )
{
	m_IsAlphaTested = bEnable;
}

void CShaderShadowEmpty::AlphaFunc( ShaderAlphaFunc_t alphaFunc, float alphaRef /* [0-1] */ )
{
}


// Wireframe/filled polygons
void CShaderShadowEmpty::PolyMode( ShaderPolyModeFace_t face, ShaderPolyMode_t polyMode )
{
}


// Back face culling
void CShaderShadowEmpty::EnableCulling( bool bEnable )
{
}


// Alpha to coverage
void CShaderShadowEmpty::EnableAlphaToCoverage( bool bEnable )
{
}

// Indicates the vertex format for use with a vertex shader
// The flags to pass in here come from the VertexFormatFlags_t enum
// If pTexCoordDimensions is *not* specified, we assume all coordinates
// are 2-dimensional
void CShaderShadowEmpty::VertexShaderVertexFormat( unsigned int nFlags, 
												   int nTexCoordCount,
												   int* pTexCoordDimensions,
												   int nUserDataSize )
{
}

// Sets the vertex and pixel shaders
void CShaderShadowEmpty::SetVertexShader( const char *pShaderName, int vshIndex )
{
	m_bUsesVertexAndPixelShaders = ( pShaderName != nullptr );
}

void CShaderShadowEmpty::EnableBlendingSeparateAlpha( bool bEnable )
{
}
void CShaderShadowEmpty::SetPixelShader( const char *pShaderName, int pshIndex )
{
	m_bUsesVertexAndPixelShaders = ( pShaderName != nullptr );
}

void CShaderShadowEmpty::BlendFuncSeparateAlpha( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor )
{
}



//-----------------------------------------------------------------------------
//
// Shader API Empty
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------

CShaderAPIEmpty::CShaderAPIEmpty()  : m_Mesh( false )
{
}

CShaderAPIEmpty::~CShaderAPIEmpty()
{
}


bool CShaderAPIEmpty::DoRenderTargetsNeedSeparateDepthBuffer() const
{
	return false;
}

// Can we download textures?
bool CShaderAPIEmpty::CanDownloadTextures() const
{
	return false;
}

// Used to clear the transition table when we know it's become invalid.
void CShaderAPIEmpty::ClearSnapshots()
{
}

// Members of IMaterialSystemHardwareConfig
bool CShaderAPIEmpty::HasDestAlphaBuffer() const
{
	return false;
}

bool CShaderAPIEmpty::HasStencilBuffer() const
{
	return false;
}

int CShaderAPIEmpty::MaxViewports() const
{
	return 1;
}

ShadowFilterMode_t CShaderAPIEmpty::GetShadowFilterMode( bool bForceLowQualityShadows, bool bPS30 ) const
{
	bForceLowQualityShadows;
	bPS30;
	return SHADOWFILTERMODE_DEFAULT;
}

int CShaderAPIEmpty::StencilBufferBits() const
{
	return 0;
}

int CShaderAPIEmpty::GetFrameBufferColorDepth() const
{
	return 0;
}

int  CShaderAPIEmpty::GetSamplerCount() const
{
	return 4;
}


int CShaderAPIEmpty::GetVertexSamplerCount() const
{
	return 0;
}


bool CShaderAPIEmpty::HasSetDeviceGammaRamp() const
{
	return false;
}

bool CShaderAPIEmpty::SupportsCompressedTextures() const
{
	return false;
}

bool CShaderAPIEmpty::SupportsStaticControlFlow() const
{
	// Yes, unconditionally unless we're on POSIX
	return !IsOpenGL();
}

VertexCompressionType_t CShaderAPIEmpty::SupportsCompressedVertices() const
{
	return VERTEX_COMPRESSION_NONE;
}

int  CShaderAPIEmpty::MaximumAnisotropicLevel() const
{
	return 0;
}

void CShaderAPIEmpty::SetAnisotropicLevel( int nAnisotropyLevel )
{
}

int  CShaderAPIEmpty::MaxTextureWidth() const
{
	// Should be big enough to cover all cases
	return 16384;
}

int  CShaderAPIEmpty::MaxTextureHeight() const
{
	// Should be big enough to cover all cases
	return 16384;
}

int  CShaderAPIEmpty::MaxTextureAspectRatio() const
{
	// Should be big enough to cover all cases
	return 16384;
}


int	 CShaderAPIEmpty::TextureMemorySize() const
{
	// fake it
	return 1024 * 1024 * 1024;
}

int  CShaderAPIEmpty::GetDXSupportLevel() const 
{ 
	return 95; 
}

bool CShaderAPIEmpty::SupportsMipmappedCubemaps() const
{
	return true;
}

int	 CShaderAPIEmpty::NumVertexShaderConstants() const
{
	return 128;
}

int	 CShaderAPIEmpty::NumBooleanVertexShaderConstants() const
{
	return 0;
}

int	 CShaderAPIEmpty::NumIntegerVertexShaderConstants() const
{
	return 0;
}

int	 CShaderAPIEmpty::NumPixelShaderConstants() const
{
	return 8;
}

int	 CShaderAPIEmpty::MaxNumLights() const
{
	return 4;
}

// This is the max dx support level supported by the card
int	CShaderAPIEmpty::GetMaxDXSupportLevel() const
{
	return 90;
}

int	 CShaderAPIEmpty::MaxVertexShaderBlendMatrices() const
{
	return 0;
}

int	CShaderAPIEmpty::MaxUserClipPlanes() const
{
	return 0;
}

bool CShaderAPIEmpty::SpecifiesFogColorInLinearSpace() const
{
	return false;
}

bool CShaderAPIEmpty::SupportsSRGB() const
{
	return false;
}

bool CShaderAPIEmpty::FakeSRGBWrite() const
{
	return false;
}

bool CShaderAPIEmpty::CanDoSRGBReadFromRTs() const
{
	return true;
}

bool CShaderAPIEmpty::SupportsGLMixedSizeTargets() const
{
	return false;
}

const char *CShaderAPIEmpty::GetHWSpecificShaderDLLName() const
{
	return 0;
}

// Sets the default *dynamic* state
void CShaderAPIEmpty::SetDefaultState()
{
}


// Returns the snapshot id for the shader state
StateSnapshot_t	 CShaderAPIEmpty::TakeSnapshot( )
{
	StateSnapshot_t id = 0;
	if (g_ShaderShadow.m_IsTranslucent && !g_ShaderShadow.m_ForceOpaque)
		id |= TRANSLUCENT;
	if (g_ShaderShadow.m_IsAlphaTested)
		id |= ALPHATESTED;
	if (g_ShaderShadow.m_bUsesVertexAndPixelShaders)
		id |= VERTEX_AND_PIXEL_SHADERS;
	if (g_ShaderShadow.m_bIsDepthWriteEnabled)
		id |= DEPTHWRITE;
	return id;
}

// Returns true if the state snapshot is transparent
bool CShaderAPIEmpty::IsTranslucent( StateSnapshot_t id ) const
{
	return (id & TRANSLUCENT) != 0; 
}

bool CShaderAPIEmpty::IsAlphaTested( StateSnapshot_t id ) const
{
	return (id & ALPHATESTED) != 0; 
}

bool CShaderAPIEmpty::IsDepthWriteEnabled( StateSnapshot_t id ) const
{
	return (id & DEPTHWRITE) != 0; 
}

bool CShaderAPIEmpty::UsesVertexAndPixelShaders( StateSnapshot_t id ) const
{
	return (id & VERTEX_AND_PIXEL_SHADERS) != 0; 
}

// Gets the vertex format for a set of snapshot ids
VertexFormat_t CShaderAPIEmpty::ComputeVertexFormat( int numSnapshots, StateSnapshot_t* pIds ) const
{
	return 0;
}

// Gets the vertex format for a set of snapshot ids
VertexFormat_t CShaderAPIEmpty::ComputeVertexUsage( int numSnapshots, StateSnapshot_t* pIds ) const
{
	return 0;
}

// Uses a state snapshot
void CShaderAPIEmpty::UseSnapshot( StateSnapshot_t snapshot )
{
}

// The shade mode
void CShaderAPIEmpty::ShadeMode( ShaderShadeMode_t mode )
{
}

// Binds a particular material to render with
void CShaderAPIEmpty::Bind( IMaterial* pMaterial )
{
}

// Cull mode
void CShaderAPIEmpty::CullMode( MaterialCullMode_t cullMode )
{
}

void CShaderAPIEmpty::FlipCullMode( void )
{
}

void CShaderAPIEmpty::ForceDepthFuncEquals( bool bEnable )
{
}

// Forces Z buffering on or off
void CShaderAPIEmpty::OverrideDepthEnable( bool bEnable, bool bDepthWriteEnable, bool bDepthTestEnable )
{
}

void CShaderAPIEmpty::OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable )
{
}

void CShaderAPIEmpty::OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable )
{
}

//legacy fast clipping linkage
void CShaderAPIEmpty::SetHeightClipZ( float z )
{
}

void CShaderAPIEmpty::SetHeightClipMode( enum MaterialHeightClipMode_t heightClipMode )
{
}

// Sets the lights
void CShaderAPIEmpty::SetLights( int nCount, const LightDesc_t *pDesc )
{
}

// Sets lighting origin for the current model
void CShaderAPIEmpty::SetLightingOrigin( Vector vLightingOrigin )
{
}

void CShaderAPIEmpty::SetAmbientLightCube( Vector4D cube[6] )
{
}

// Gets the lightmap dimensions
void CShaderAPIEmpty::GetLightmapDimensions( int *w, int *h )
{
	g_pShaderUtil->GetLightmapDimensions( w, h );
}

// Gets the dynamic mesh; note that you've got to render the mesh
// before calling this function a second time. Clients should *not*
// call DestroyStaticMesh on the mesh returned by this call.
IMesh* CShaderAPIEmpty::GetDynamicMesh( IMaterial* pMaterial, int nHWSkinBoneCount, bool buffered, IMesh* pVertexOverride, IMesh* pIndexOverride )
{
	return &m_Mesh;
}

IMesh* CShaderAPIEmpty::GetDynamicMeshEx( IMaterial* pMaterial, VertexFormat_t fmt, int nHWSkinBoneCount, bool buffered, IMesh* pVertexOverride, IMesh* pIndexOverride )
{
	return &m_Mesh;
}

IMesh* CShaderAPIEmpty::GetFlexMesh()
{
	return &m_Mesh;
}

// Begins a rendering pass that uses a state snapshot
void CShaderAPIEmpty::BeginPass( StateSnapshot_t snapshot  )
{
}

// Renders a single pass of a material
void CShaderAPIEmpty::RenderPass( const unsigned char *pInstanceCommandBuffer, int nPass, int nPassCount )
{
}

// stuff related to matrix stacks
void CShaderAPIEmpty::MatrixMode( MaterialMatrixMode_t matrixMode )
{
}

void CShaderAPIEmpty::PushMatrix()
{
}

void CShaderAPIEmpty::PopMatrix()
{
}

void CShaderAPIEmpty::LoadMatrix( float *m )
{
}

void CShaderAPIEmpty::MultMatrix( float *m )
{
}

void CShaderAPIEmpty::MultMatrixLocal( float *m )
{
}

void CShaderAPIEmpty::GetActualProjectionMatrix( float *pMatrix )
{
}

void CShaderAPIEmpty::GetMatrix( MaterialMatrixMode_t matrixMode, float *dst )
{
}

void CShaderAPIEmpty::LoadIdentity( void )
{
}

void CShaderAPIEmpty::LoadCameraToWorld( void )
{
}

void CShaderAPIEmpty::Ortho( double left, double top, double right, double bottom, double zNear, double zFar )
{
}

void CShaderAPIEmpty::PerspectiveX( double fovx, double aspect, double zNear, double zFar )
{
}

void CShaderAPIEmpty::PerspectiveOffCenterX( double fovx, double aspect, double zNear, double zFar, double bottom, double top, double left, double right )
{
}

void CShaderAPIEmpty::PickMatrix( int x, int y, int width, int height )
{
}

void CShaderAPIEmpty::Rotate( float angle, float x, float y, float z )
{
}

void CShaderAPIEmpty::Translate( float x, float y, float z )
{
}

void CShaderAPIEmpty::Scale( float x, float y, float z )
{
}

void CShaderAPIEmpty::ScaleXY( float x, float y )
{
}

// Fog methods...
void CShaderAPIEmpty::FogMode( MaterialFogMode_t fogMode )
{
}

void CShaderAPIEmpty::FogStart( float fStart )
{
}

void CShaderAPIEmpty::FogEnd( float fEnd )
{
}

void CShaderAPIEmpty::SetFogZ( float fogZ )
{
}
	
void CShaderAPIEmpty::FogMaxDensity( float flMaxDensity )
{
}

void CShaderAPIEmpty::GetFogDistances( float *fStart, float *fEnd, float *fFogZ )
{
}


void CShaderAPIEmpty::SceneFogColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
}


void CShaderAPIEmpty::SceneFogMode( MaterialFogMode_t fogMode )
{
}

void CShaderAPIEmpty::GetSceneFogColor( unsigned char *rgb )
{
}

MaterialFogMode_t CShaderAPIEmpty::GetSceneFogMode( )
{
	return MATERIAL_FOG_NONE;
}

int CShaderAPIEmpty::GetPixelFogCombo( )
{
	return 0;
}

void CShaderAPIEmpty::FogColor3f( float r, float g, float b )
{
}

void CShaderAPIEmpty::FogColor3fv( float const* rgb )
{
}

void CShaderAPIEmpty::FogColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
}

void CShaderAPIEmpty::FogColor3ubv( unsigned char const* rgb )
{
}

void CShaderAPIEmpty::SetViewports( int nCount, const ShaderViewport_t* pViewports, bool setImmediately /* = false*/ )
{
}

int CShaderAPIEmpty::GetViewports( ShaderViewport_t* pViewports, int nMax ) const
{
	return 1;
}

// Sets the vertex and pixel shaders
void CShaderAPIEmpty::SetVertexShaderIndex( int vshIndex )
{
}

void CShaderAPIEmpty::SetPixelShaderIndex( int pshIndex )
{
}

// Sets the constant registers for vertex and pixel shaders
void CShaderAPIEmpty::SetVertexShaderConstant( int var, float const* pVec, int numConst, bool bForce )
{
}

void CShaderAPIEmpty::SetBooleanVertexShaderConstant( int var, BOOL const* pVec, int numConst, bool bForce )
{
}

void CShaderAPIEmpty::SetIntegerVertexShaderConstant( int var, int const* pVec, int numConst, bool bForce )
{
}

void CShaderAPIEmpty::SetPixelShaderConstant( int var, float const* pVec, int numConst, bool bForce )
{
}

void CShaderAPIEmpty::SetBooleanPixelShaderConstant( int var, BOOL const* pVec, int numBools, bool bForce )
{
}

void CShaderAPIEmpty::SetIntegerPixelShaderConstant( int var, int const* pVec, int numIntVecs, bool bForce )
{
}

void CShaderAPIEmpty::InvalidateDelayedShaderConstants( void )
{
}

float CShaderAPIEmpty::GammaToLinear_HardwareSpecific( float fGamma ) const
{
	return 0.0f;
}

float CShaderAPIEmpty::LinearToGamma_HardwareSpecific( float fLinear ) const
{
	return 0.0f;
}

void CShaderAPIEmpty::SetLinearToGammaConversionTextures( ShaderAPITextureHandle_t hSRGBWriteEnabledTexture, ShaderAPITextureHandle_t hIdentityTexture )
{
}


// Returns the nearest supported format
ImageFormat CShaderAPIEmpty::GetNearestSupportedFormat( ImageFormat fmt, bool bFilteringRequired /* = true */ ) const
{
	return fmt;
}

ImageFormat CShaderAPIEmpty::GetNearestRenderTargetFormat( ImageFormat fmt ) const
{
	return fmt;
}

// Sets the texture state
void CShaderAPIEmpty::BindTexture( Sampler_t stage, TextureBindFlags_t nBindFlags, ShaderAPITextureHandle_t textureHandle )
{
}

void CShaderAPIEmpty::BindVertexTexture( VertexTextureSampler_t vtSampler, ShaderAPITextureHandle_t textureHandle )
{
}

void CShaderAPIEmpty::ClearColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
}

void CShaderAPIEmpty::ClearColor4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a )
{
}

// Indicates we're going to be modifying this texture
// TexImage2D, TexSubImage2D, TexWrap, TexMinFilter, and TexMagFilter
// all use the texture specified by this function.
void CShaderAPIEmpty::ModifyTexture( ShaderAPITextureHandle_t textureHandle )
{
}

// Texture management methods
void CShaderAPIEmpty::TexImage2D( int level, int cubeFace, ImageFormat dstFormat, int zOffset, int width, int height, 
						 ImageFormat srcFormat, bool bSrcIsTiled, void *imageData )
{
}

void CShaderAPIEmpty::TexSubImage2D( int level, int cubeFace, int xOffset, int yOffset, int zOffset, int width, int height,
						 ImageFormat srcFormat, int srcStride, bool bSrcIsTiled, void *imageData )
{
}

bool CShaderAPIEmpty::TexLock( int level, int cubeFaceID, int xOffset, int yOffset, 
								int width, int height, CPixelWriter& writer )
{
	return false;
}

void CShaderAPIEmpty::TexUnlock( )
{
}

void CShaderAPIEmpty::UpdateTexture( int xOffset, int yOffset, int w, int h, ShaderAPITextureHandle_t hDstTexture, ShaderAPITextureHandle_t hSrcTexture )
{
}
void *CShaderAPIEmpty::LockTex( ShaderAPITextureHandle_t hTexture )
{
	return nullptr;
}
void CShaderAPIEmpty::UnlockTex( ShaderAPITextureHandle_t hTexture )
{
}

// These are bound to the texture, not the texture environment
void CShaderAPIEmpty::TexMinFilter( ShaderTexFilterMode_t texFilterMode )
{
}

void CShaderAPIEmpty::TexMagFilter( ShaderTexFilterMode_t texFilterMode )
{
}

void CShaderAPIEmpty::TexWrap( ShaderTexCoordComponent_t coord, ShaderTexWrapMode_t wrapMode )
{
}

void CShaderAPIEmpty::TexSetPriority( int priority )
{
}

ShaderAPITextureHandle_t CShaderAPIEmpty::CreateTexture( 
	int width, 
	int height,
	int depth,
	ImageFormat dstImageFormat, 
	int numMipLevels, 
	int numCopies, 
	int flags, 
	const char *pDebugName,
	const char *pTextureGroupName )
{
	return 0;
}

// Create a multi-frame texture (equivalent to calling "CreateTexture" multiple times, but more efficient)
void CShaderAPIEmpty::CreateTextures( 
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
							const char *pTextureGroupName )
{
	for ( int k = 0; k < count; ++ k )
		pHandles[ k ] = 0;
}


ShaderAPITextureHandle_t CShaderAPIEmpty::CreateDepthTexture( ImageFormat renderFormat, int width, int height, const char *pDebugName, bool bTexture, bool bAliasDepthTextureOverSceneDepthX360 )
{
	return 0;
}

void CShaderAPIEmpty::DeleteTexture( ShaderAPITextureHandle_t textureHandle )
{
}

bool CShaderAPIEmpty::IsTexture( ShaderAPITextureHandle_t textureHandle )
{
	return true;
}

bool CShaderAPIEmpty::IsTextureResident( ShaderAPITextureHandle_t textureHandle )
{
	return false;
}

// stuff that isn't to be used from within a shader
void CShaderAPIEmpty::ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil, int renderTargetWidth, int renderTargetHeight )
{
}

void CShaderAPIEmpty::ClearBuffersEx( bool bClearRed, bool bClearGreen, bool bClearBlue, bool bClearAlpha,
									unsigned char r, unsigned char g, unsigned char b, unsigned char a )
{

}

void CShaderAPIEmpty::ClearBuffersObeyStencil( bool bClearColor, bool bClearDepth )
{
}

void CShaderAPIEmpty::ClearBuffersObeyStencilEx( bool bClearColor, bool bClearAlpha, bool bClearDepth )
{
}

void CShaderAPIEmpty::PerformFullScreenStencilOperation( void )
{
}

void CShaderAPIEmpty::SetScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom, const bool bEnableScissor )
{
}

void CShaderAPIEmpty::ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture )
{
}

void CShaderAPIEmpty::ReadPixelsAsync( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture, CThreadEvent *pPixelsReadEvent )
{
}

void CShaderAPIEmpty::ReadPixelsAsyncGetResult( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, CThreadEvent *pGetResultEvent )
{
}

void CShaderAPIEmpty::ReadPixels( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *data, ImageFormat dstFormat, int nDstStride )
{
}

void CShaderAPIEmpty::FlushHardware()
{
}

void CShaderAPIEmpty::ResetRenderState( bool bFullReset )
{
}

// Set the number of bone weights
void CShaderAPIEmpty::SetNumBoneWeights( int numBones )
{
}

void CShaderAPIEmpty::EnableHWMorphing( bool bEnable )
{
}

// Selection mode methods
int CShaderAPIEmpty::SelectionMode( bool selectionMode )
{
	return 0;
}

void CShaderAPIEmpty::SelectionBuffer( unsigned int* pBuffer, int size )
{
}

void CShaderAPIEmpty::ClearSelectionNames( )
{
}

void CShaderAPIEmpty::LoadSelectionName( int name )
{
}

void CShaderAPIEmpty::PushSelectionName( int name )
{
}

void CShaderAPIEmpty::PopSelectionName()
{
}


// Use this to get the mesh builder that allows us to modify vertex data
CMeshBuilder* CShaderAPIEmpty::GetVertexModifyBuilder()
{
	return 0;
}

// Board-independent calls, here to unify how shaders set state
// Implementations should chain back to IShaderUtil->BindTexture(), etc.

// Use this to begin and end the frame
void CShaderAPIEmpty::BeginFrame()
{
}

void CShaderAPIEmpty::EndFrame()
{
}

// returns the current time in seconds....
double CShaderAPIEmpty::CurrentTime() const
{
	return Plat_FloatTime();
}

// Get the current camera position in world space.
void CShaderAPIEmpty::GetWorldSpaceCameraPosition( float* pPos ) const
{
}

void CShaderAPIEmpty::GetWorldSpaceCameraDirection( float* pDir ) const
{
}

void CShaderAPIEmpty::ForceHardwareSync( void )
{
}

void CShaderAPIEmpty::SetClipPlane( int index, const float *pPlane )
{
}

void CShaderAPIEmpty::EnableClipPlane( int index, bool bEnable )
{
}

void CShaderAPIEmpty::SetFastClipPlane( const float *pPlane )
{
}

void CShaderAPIEmpty::EnableFastClip( bool bEnable )
{
}

int CShaderAPIEmpty::GetCurrentNumBones( void ) const
{
	return 0;
}

bool CShaderAPIEmpty::IsHWMorphingEnabled( void ) const
{
	return false;
}

void CShaderAPIEmpty::GetDX9LightState( LightState_t *state ) const
{
	state->m_nNumLights = 0;
	state->m_bAmbientLight = false;
	state->m_bStaticLight = false;
	state->m_bStaticLightIndirectOnly = false;
}

MaterialFogMode_t CShaderAPIEmpty::GetCurrentFogType( void ) const
{
	contract_assert( 0 ); // deprecated
	return MATERIAL_FOG_NONE;
}

void CShaderAPIEmpty::RecordString( const char *pStr )
{
}

bool CShaderAPIEmpty::ReadPixelsFromFrontBuffer() const
{
	return true;
}

bool CShaderAPIEmpty::PreferDynamicTextures() const
{
	return false;
}

int  CShaderAPIEmpty::GetCurrentDynamicVBSize( void )
{
	return 0;
}

void CShaderAPIEmpty::DestroyVertexBuffers( bool bExitingLevel )
{
}

void CShaderAPIEmpty::EvictManagedResources()
{
}

void CShaderAPIEmpty::GetGPUMemoryStats( GPUMemoryStats &stats )
{
}

void CShaderAPIEmpty::SyncToken( const char *pToken )
{
}
