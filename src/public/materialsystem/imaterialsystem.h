#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

inline constexpr auto OVERBRIGHT = 2.0f;
inline constexpr auto OO_OVERBRIGHT = ( 1.0f / 2.0f );
inline constexpr auto GAMMA = 2.2f;
inline constexpr auto TEXGAMMA = 2.2f;

import ks.tier1;
import std;
#include "bitmap/imageformat.h"
#include "texture_group_names.h"
#include "vtf/vtf.h"
#include "materialsystem/deformations.h"

// HDRFIXME NOTE: must match common_ps_fxc.h
enum HDRType_t
{
	HDR_TYPE_NONE,
	HDR_TYPE_INTEGER,
	HDR_TYPE_FLOAT,
};

#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/IColorCorrection.h"

// NOTE: Disable this for l4d2 in general!!!  It allocates 4mb of rendertargets and causes Release/Reallocation of rendertargets.
// PORTAL2: Turning this off in the portal 2 branch because we don't use it and it appears to leak each mapload.
// We don't currently expect to merge this code back to main so this may not matter, but if we do we'll have to resolve weeding this
// out of portal2 builds.
//#define FEATURE_SUBD_SUPPORT


//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class IMaterial;
class IMesh;
class IVertexBuffer;
class IIndexBuffer;
struct MaterialSystem_Config_t;
class ITexture;
struct MaterialSystemHardwareIdentifier_t;
class IShader;
class IVertexTexture;
class IMorph;
class IMatRenderContext;
class ICallQueue;
struct MorphWeight_t;
class IFileList;
struct VertexStreamSpec_t;
struct ShaderStencilState_t;
struct MeshInstanceData_t;
class CPaintMaterial;
class IPaintmapDataManager;
class IPaintmapTextureManager;
struct GPUMemoryStats;
class ICustomMaterialManager;
class ICompositeTextureGenerator;

//-----------------------------------------------------------------------------
// The vertex format type
//-----------------------------------------------------------------------------
typedef uint64 VertexFormat_t;

//-----------------------------------------------------------------------------
// important enumeration
//-----------------------------------------------------------------------------
enum ShaderParamType_t 
{ 
	SHADER_PARAM_TYPE_TEXTURE, 
	SHADER_PARAM_TYPE_INTEGER,
	SHADER_PARAM_TYPE_COLOR,
	SHADER_PARAM_TYPE_VEC2,
	SHADER_PARAM_TYPE_VEC3,
	SHADER_PARAM_TYPE_VEC4,
	SHADER_PARAM_TYPE_ENVMAP,	// obsolete
	SHADER_PARAM_TYPE_FLOAT,
	SHADER_PARAM_TYPE_BOOL,
	SHADER_PARAM_TYPE_FOURCC,
	SHADER_PARAM_TYPE_MATRIX,
	SHADER_PARAM_TYPE_MATERIAL,
	SHADER_PARAM_TYPE_STRING,
};

enum MaterialMatrixMode_t
{
	MATERIAL_VIEW = 0,
	MATERIAL_PROJECTION,

	MATERIAL_MATRIX_UNUSED0,
	MATERIAL_MATRIX_UNUSED1,
	MATERIAL_MATRIX_UNUSED2,
	MATERIAL_MATRIX_UNUSED3,
	MATERIAL_MATRIX_UNUSED4,
	MATERIAL_MATRIX_UNUSED5,
	MATERIAL_MATRIX_UNUSED6,
	MATERIAL_MATRIX_UNUSED7,

	MATERIAL_MODEL,

	// Total number of matrices
	NUM_MATRIX_MODES = MATERIAL_MODEL+1,
};

// FIXME: How do I specify the actual number of matrix modes?
const int NUM_MODEL_TRANSFORMS = 53;
const int MATERIAL_MODEL_MAX = MATERIAL_MODEL + NUM_MODEL_TRANSFORMS;

enum MaterialPrimitiveType_t 
{ 
	MATERIAL_POINTS			= 0x0,
	MATERIAL_LINES,
	MATERIAL_TRIANGLES,
	MATERIAL_TRIANGLE_STRIP,
	MATERIAL_LINE_STRIP,
	MATERIAL_LINE_LOOP,	// a single line loop
	MATERIAL_POLYGON,	// this is a *single* polygon
	MATERIAL_QUADS,
	MATERIAL_SUBD_QUADS_EXTRA, // Extraordinary sub-d quads
	MATERIAL_SUBD_QUADS_REG,   // Regular sub-d quads
	MATERIAL_INSTANCED_QUADS, // (X360) like MATERIAL_QUADS, but uses vertex instancing

	// This is used for static meshes that contain multiple types of
	// primitive types.	When calling draw, you'll need to specify
	// a primitive type.
	MATERIAL_HETEROGENOUS
};

enum TessellationMode_t
{
	TESSELLATION_MODE_DISABLED = 0,
	TESSELLATION_MODE_ACC_PATCHES_EXTRA,
	TESSELLATION_MODE_ACC_PATCHES_REG
};

enum MaterialPropertyTypes_t
{
	MATERIAL_PROPERTY_NEEDS_LIGHTMAP = 0,					// bool
	MATERIAL_PROPERTY_OPACITY,								// int (enum MaterialPropertyOpacityTypes_t)
	MATERIAL_PROPERTY_REFLECTIVITY,							// vec3_t
	MATERIAL_PROPERTY_NEEDS_BUMPED_LIGHTMAPS				// bool
};

// acceptable property values for MATERIAL_PROPERTY_OPACITY
enum MaterialPropertyOpacityTypes_t
{
	MATERIAL_ALPHATEST = 0,
	MATERIAL_OPAQUE,
	MATERIAL_TRANSLUCENT
};

enum MaterialBufferTypes_t
{
	MATERIAL_FRONT = 0,
	MATERIAL_BACK
};

enum MaterialCullMode_t
{
	MATERIAL_CULLMODE_CCW,	// this culls polygons with counterclockwise winding
	MATERIAL_CULLMODE_CW,	// this culls polygons with clockwise winding
	MATERIAL_CULLMODE_NONE	// cull nothing
};

enum MaterialIndexFormat_t
{
	MATERIAL_INDEX_FORMAT_UNKNOWN = -1,
	MATERIAL_INDEX_FORMAT_16BIT = 0,
	MATERIAL_INDEX_FORMAT_32BIT,
};

enum MaterialFogMode_t
{
	MATERIAL_FOG_NONE,
	MATERIAL_FOG_LINEAR,
	MATERIAL_FOG_LINEAR_BELOW_FOG_Z,
};

enum MaterialHeightClipMode_t
{
	MATERIAL_HEIGHTCLIPMODE_DISABLE,
	MATERIAL_HEIGHTCLIPMODE_RENDER_ABOVE_HEIGHT,
	MATERIAL_HEIGHTCLIPMODE_RENDER_BELOW_HEIGHT
};

enum MaterialNonInteractiveMode_t
{
	MATERIAL_NON_INTERACTIVE_MODE_NONE = -1,
	MATERIAL_NON_INTERACTIVE_MODE_STARTUP = 0,
	MATERIAL_NON_INTERACTIVE_MODE_LEVEL_LOAD,

	MATERIAL_NON_INTERACTIVE_MODE_COUNT,
};


//-----------------------------------------------------------------------------
// Special morph used in decalling pass
//-----------------------------------------------------------------------------
inline IMorph *const MATERIAL_MORPH_DECAL = reinterpret_cast<IMorph *>( 1 );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

enum MaterialThreadMode_t
{
	MATERIAL_SINGLE_THREADED,
	MATERIAL_QUEUED_SINGLE_THREADED,
	MATERIAL_QUEUED_THREADED
};


//-----------------------------------------------------------------------------
// Light structure
//-----------------------------------------------------------------------------

enum
{
	MATERIAL_MAX_LIGHT_COUNT = 4,
};

struct MaterialLightingState_t
{
	Vector			m_vecAmbientCube[6];		// ambient, and lights that aren't in locallight[]
	Vector			m_vecLightingOrigin;		// The position from which lighting state was computed
	int				m_nLocalLightCount;
	LightDesc_t		m_pLocalLightDesc[MATERIAL_MAX_LIGHT_COUNT];

	MaterialLightingState_t &operator=( const MaterialLightingState_t &src )
	{
		std::memcpy( this, &src, sizeof(MaterialLightingState_t) - MATERIAL_MAX_LIGHT_COUNT * sizeof(LightDesc_t) );
		std::size_t byteCount = src.m_nLocalLightCount * sizeof(LightDesc_t);
		contract_assert( byteCount <= sizeof(m_pLocalLightDesc) );
		memcpy( m_pLocalLightDesc, &src.m_pLocalLightDesc, byteCount );
		return *this;
	}
};



inline constexpr auto CREATERENDERTARGETFLAGS_HDR = 0x00000001;
inline constexpr auto CREATERENDERTARGETFLAGS_AUTOMIPMAP = 0x00000002;
inline constexpr auto CREATERENDERTARGETFLAGS_UNFILTERABLE_OK = 0x00000004;
// XBOX ONLY:
inline constexpr auto CREATERENDERTARGETFLAGS_NOEDRAM = 0x00000008; // inhibit allocation in 360 EDRAM
inline constexpr auto CREATERENDERTARGETFLAGS_TEMP = 0x00000010; // only allocates memory upon first resolve, destroyed at level end
inline constexpr auto CREATERENDERTARGETFLAGS_ALIASCOLORANDDEPTHSURFACES = 0x00000020; // force the depth surface to be aliased overtop of the color surface in EDRAM, for the special case of depth-only rendering


//-----------------------------------------------------------------------------
// Used to describe a material batch
//-----------------------------------------------------------------------------
struct MaterialBatchData_t
{
	IMaterial *				m_pMaterial;
	matrix3x4_t	*			m_pModelToWorld;
	const ITexture *		m_pEnvCubemap;
	int						m_nLightmapPageId;
	MaterialPrimitiveType_t m_nPrimType;
	int						m_nIndexOffset;
	int						m_nIndexCount;
	int						m_nVertexOffsetInBytes;
	const IIndexBuffer *	m_pIndexBuffer;
	const IVertexBuffer	*	m_pVertexBuffer;
};


//-----------------------------------------------------------------------------
// Enumeration for the various fields capable of being morphed
//-----------------------------------------------------------------------------
enum MorphFormatFlags_t
{
	MORPH_POSITION	= 0x0001,	// 3D
	MORPH_NORMAL	= 0x0002,	// 3D
	MORPH_WRINKLE	= 0x0004,	// 1D
	MORPH_SPEED		= 0x0008,	// 1D
	MORPH_SIDE		= 0x0010,	// 1D
};


//-----------------------------------------------------------------------------
// The morph format type
//-----------------------------------------------------------------------------
typedef unsigned int MorphFormat_t;


//-----------------------------------------------------------------------------
// Standard lightmaps
//-----------------------------------------------------------------------------
enum StandardLightmap_t
{
	MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE = -1,
	MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP = -2,
	MATERIAL_SYSTEM_LIGHTMAP_PAGE_USER_DEFINED = -3,
	MATERIAL_SYSTEM_LIGHTMAP_PAGE_INVALID = -4,
};


struct MaterialSystem_SortInfo_t
{
	IMaterial	*material;
	int			lightmapPageID;
};


inline constexpr auto MAX_FB_TEXTURES = 4;

//-----------------------------------------------------------------------------
// Information about each adapter
//-----------------------------------------------------------------------------
enum
{
	MATERIAL_ADAPTER_NAME_LENGTH = 512
};

struct MaterialAdapterInfo_t
{
	char m_pDriverName[MATERIAL_ADAPTER_NAME_LENGTH];
	unsigned int m_VendorID;
	unsigned int m_DeviceID;
	unsigned int m_SubSysID;
	unsigned int m_Revision;
	int m_nDXSupportLevel;			// This is the *preferred* dx support level
	int m_nMinDXSupportLevel;
	int m_nMaxDXSupportLevel;
	unsigned int m_nDriverVersionHigh;
	unsigned int m_nDriverVersionLow;
};


//-----------------------------------------------------------------------------
// Video mode info..
//-----------------------------------------------------------------------------
struct MaterialVideoMode_t
{
	int m_Width;			// if width and height are 0 and you select 
	int m_Height;			// windowed mode, it'll use the window size
	ImageFormat m_Format;	// use ImageFormats (ignored for windowed mode)
	int m_RefreshRate;		// 0 == default (ignored for windowed mode)
};


//--------------------------------------------------------------------------------
// Uberlight parameters
//--------------------------------------------------------------------------------
struct UberlightState_t
{
	UberlightState_t()
	{
		m_fNearEdge 	= 2.0f;
		m_fFarEdge  	= 100.0f;
		m_fCutOn    	= 10.0f;
		m_fCutOff   	= 650.0f;
		m_fShearx   	= 0.0f;
		m_fSheary   	= 0.0f;
		m_fWidth    	= 0.3f;
		m_fWedge    	= 0.05f;
		m_fHeight		= 0.3f;
		m_fHedge		= 0.05f;
		m_fRoundness	= 0.8f;
	}

	float m_fNearEdge;
	float m_fFarEdge;
	float m_fCutOn;
	float m_fCutOff;
	float m_fShearx;
	float m_fSheary;
	float m_fWidth;
	float m_fWedge;
	float m_fHeight;
	float m_fHedge;
	float m_fRoundness;

	public:
	UberlightState_t &operator=( const UberlightState_t &src )
	{
		std::memcpy( this, &src, sizeof( UberlightState_t ) );
		return *this;
	};
};

// fixme: should move this into something else.
struct FlashlightState_t
{
	FlashlightState_t()
	{
		m_bEnableShadows = false;						// Provide reasonable defaults for shadow depth mapping parameters
		m_bDrawShadowFrustum = false;
		m_flShadowMapResolution = 1024.0f;
		m_flShadowFilterSize = 3.0f;
		m_flShadowSlopeScaleDepthBias = 16.0f;
		m_flShadowDepthBias = 0.0005f;
		m_flShadowJitterSeed = 0.0f;
		m_flShadowAtten = 0.0f;
		m_flAmbientOcclusion = 0.0f;
		m_nShadowQuality = 0;
		m_bShadowHighRes = false;

		m_bScissor = false; 
		m_nLeft = -1;
		m_nTop = -1;
		m_nRight = -1;
		m_nBottom = -1;

		m_bUberlight = false;

		m_bVolumetric = false;
		m_flNoiseStrength = 0.8f;
		m_flFlashlightTime = 0.0f;
		m_nNumPlanes = 64;
		m_flPlaneOffset = 0.0f;
		m_flVolumetricIntensity = 1.0f;

		m_bOrtho = false;
		m_fOrthoLeft = -1.0f;
		m_fOrthoRight = 1.0f;
		m_fOrthoTop = -1.0f;
		m_fOrthoBottom = 1.0f;

		m_fBrightnessScale = 1.0f;
		m_pSpotlightTexture = nullptr;
		m_pProjectedMaterial = nullptr;
		m_bShareBetweenSplitscreenPlayers = false;
	}

	Vector m_vecLightOrigin;
	Quaternion m_quatOrientation;
	float m_NearZ;
	float m_FarZ;
	float m_fHorizontalFOVDegrees;
	float m_fVerticalFOVDegrees;
	bool  m_bOrtho;
	float m_fOrthoLeft;
	float m_fOrthoRight;
	float m_fOrthoTop;
	float m_fOrthoBottom;
	float m_fQuadraticAtten;
	float m_fLinearAtten;
	float m_fConstantAtten;
	float m_FarZAtten;
	float m_Color[4];
	float m_fBrightnessScale;
	ITexture *m_pSpotlightTexture;
	IMaterial *m_pProjectedMaterial;
	int m_nSpotlightTextureFrame;

	// Shadow depth mapping parameters
	bool  m_bEnableShadows;
	bool  m_bDrawShadowFrustum;
	float m_flShadowMapResolution;
	float m_flShadowFilterSize;
	float m_flShadowSlopeScaleDepthBias;
	float m_flShadowDepthBias;
	float m_flShadowJitterSeed;
	float m_flShadowAtten;
	float m_flAmbientOcclusion;
	int   m_nShadowQuality;
	bool  m_bShadowHighRes;

	// simple projection
	float m_flProjectionSize;
	float m_flProjectionRotation;

	// Uberlight parameters
	bool m_bUberlight;
	UberlightState_t m_uberlightState;

	bool m_bVolumetric;
	float m_flNoiseStrength;
	float m_flFlashlightTime;
	int m_nNumPlanes;
	float m_flPlaneOffset;
	float m_flVolumetricIntensity;
	bool m_bShareBetweenSplitscreenPlayers;	// When true, this flashlight will render for all splitscreen players

	// Getters for scissor members
	bool DoScissor() const { return m_bScissor; }
	int GetLeft()	 const { return m_nLeft; }
	int GetTop()	 const { return m_nTop; }
	int GetRight()	 const { return m_nRight; }
	int GetBottom()	 const { return m_nBottom; }

private:

	friend class CShadowMgr;

	bool m_bScissor; 
	int m_nLeft;
	int m_nTop;
	int m_nRight;
	int m_nBottom;

	public:
	FlashlightState_t &operator=( const FlashlightState_t &src )
	{
		std::memcpy( this, &src, sizeof( FlashlightState_t ) );
		return *this;
	};
};

inline constexpr auto MAX_CASCADED_SHADOW_MAPPING_CASCADES = (4);

// The number of float4's in the CascadedShadowMappingState_t (starting at m_vLightColor)
inline constexpr auto CASCADED_SHADOW_MAPPING_CONSTANT_BUFFER_SIZE = (26);

// Note: This struct is sent as-is as an array of pixel shader constants (starting at m_vLightColor). If you modify it, be sure to update CASCADED_SHADOW_MAPPING_CONSTANT_BUFFER_SIZE.
struct CascadedShadowMappingState_t
{
	uint m_nNumCascades;
	bool m_bIsRenderingViewModels;
		
	Vector4D m_vLightColor;

	Vector m_vLightDir;
	float m_flPadding1;

	struct
	{
		float m_flInvShadowTextureWidth;
		float m_flInvShadowTextureHeight;
		float m_flHalfInvShadowTextureWidth;
		float m_flHalfInvShadowTextureHeight;
	} m_TexParams;

	struct 
	{
		float m_flShadowTextureWidth;
		float m_flShadowTextureHeight;
		float m_flSplitLerpFactorBase;
		float m_flSplitLerpFactorInvRange;
	} m_TexParams2;

	struct 
	{
		float m_flDistLerpFactorBase;
		float m_flDistLerpFactorInvRange;
		float m_flUnused0;
		float m_flUnused1;
	} m_TexParams3;

	VMatrix m_matWorldToShadowTexMatrices[ MAX_CASCADED_SHADOW_MAPPING_CASCADES ];
	Vector4D m_vCascadeAtlasUVOffsets[ MAX_CASCADED_SHADOW_MAPPING_CASCADES ];

	Vector4D m_vCamPosition;
};

//-----------------------------------------------------------------------------
// Flags to be used with the Init call
//-----------------------------------------------------------------------------
enum MaterialInitFlags_t
{
	MATERIAL_INIT_ALLOCATE_FULLSCREEN_TEXTURE = 0x2,
	MATERIAL_INIT_REFERENCE_RASTERIZER	= 0x4,
};

//-----------------------------------------------------------------------------
// Flags to specify type of depth buffer used with RT
//-----------------------------------------------------------------------------

// GR - this is to add RT with no depth buffer bound

enum MaterialRenderTargetDepth_t
{
	MATERIAL_RT_DEPTH_SHARED   = 0x0,
	MATERIAL_RT_DEPTH_SEPARATE = 0x1,
	MATERIAL_RT_DEPTH_NONE     = 0x2,
	MATERIAL_RT_DEPTH_ONLY	   = 0x3,
};

//-----------------------------------------------------------------------------
// A function to be called when we need to release all vertex buffers
// NOTE: The restore function will tell the caller if all the vertex formats
// changed so that it can flush caches, etc. if it needs to (for dxlevel support)
//-----------------------------------------------------------------------------
enum RestoreChangeFlags_t
{
	MATERIAL_RESTORE_VERTEX_FORMAT_CHANGED = 0x1,
	MATERIAL_RESTORE_RELEASE_MANAGED_RESOURCES = 0x2,
};


// NOTE: All size modes will force the render target to be smaller than or equal to
// the size of the framebuffer.
enum RenderTargetSizeMode_t
{
	RT_SIZE_NO_CHANGE=0,			// Only allowed for render targets that don't want a depth buffer
	// (because if they have a depth buffer, the render target must be less than or equal to the size of the framebuffer).
	RT_SIZE_DEFAULT=1,				// Don't play with the specified width and height other than making sure it fits in the framebuffer.
	RT_SIZE_PICMIP=2,				// Apply picmip to the render target's width and height.
	RT_SIZE_HDR=3,					// frame_buffer_width / 4
	RT_SIZE_FULL_FRAME_BUFFER=4,	// Same size as frame buffer, or next lower power of 2 if we can't do that.
	RT_SIZE_OFFSCREEN=5,			// Target of specified size, don't mess with dimensions
	RT_SIZE_FULL_FRAME_BUFFER_ROUNDED_UP=6 // Same size as the frame buffer, rounded up if necessary for systems that can't do non-power of two textures.
};


enum AntiAliasingHintEnum_t
{
	AA_HINT_MESHES,
	AA_HINT_TEXT,
	AA_HINT_DEBUG_TEXT,
	AA_HINT_HEAVY_UI_OVERLAY,
	AA_HINT_ALIASING_PUSH,
	AA_HINT_ALIASING_POP,
	AA_HINT_POSTPROCESS,
	AA_HINT_MOVIE, 
	AA_HINT_MENU
};

typedef void (*MaterialBufferReleaseFunc_t)( int nChangeFlags );	// see RestoreChangeFlags_t
typedef void (*MaterialBufferRestoreFunc_t)( int nChangeFlags );	// see RestoreChangeFlags_t
typedef void (*ModeChangeCallbackFunc_t)( void );
typedef void (*EndFrameCleanupFunc_t)( void );
typedef void (*OnLevelShutdownFunc_t)( void * pUserData );
typedef bool (*EndFramePriorToNextContextFunc_t)( void );

//typedef int VertexBufferHandle_t;
typedef unsigned short MaterialHandle_t;

struct OcclusionQueryObjectHandle_t__ { int unused; };
using OcclusionQueryObjectHandle_t = OcclusionQueryObjectHandle_t__ *;
#define INVALID_OCCLUSION_QUERY_OBJECT_HANDLE	( (OcclusionQueryObjectHandle_t)0 )

class IMaterialProxyFactory;
class ITexture;
class IMaterialSystemHardwareConfig;
class CShadowMgr;

struct MaterialLock_t__ { int unused; };
using MaterialLock_t = MaterialLock_t__ *;

//-----------------------------------------------------------------------------
// Information about a material texture
//-----------------------------------------------------------------------------

struct MaterialTextureInfo_t
{
	// Exclude information:
	//		-1	texture is not subject to exclude-handling
	//		 0	texture is completely excluded
	//		>0	texture is clamped according to exclude-instruction
	int iExcludeInformation;
};

struct ApplicationPerformanceCountersInfo_t
{
	float msMain;
	float msMST;
	float msGPU;
	float msFlip;
	float msTotal;
};

struct ApplicationInstantCountersInfo_t
{
	uint m_nCpuActivityMask;
	uint m_nDeferredWordsAllocated;
};


struct WorldListIndicesInfo_t
{
	uint m_nTotalIndices; // how many indices the world needs to render
	uint m_nMaxBatchIndices; // how many indices there are in the largest batch
};

struct AspectRatioInfo_t
{
	bool m_bIsWidescreen;
	bool m_bIsHidef;
	float m_flFrameBufferAspectRatio; // width / height of framebuffer in pixels
	float m_flPhysicalAspectRatio; // width / height of the physical display in real-life units
	float m_flFrameBuffertoPhysicalScalar; // m_flPhysicalAspectRatio / m_flFrameBufferAspectRatio
	float m_flPhysicalToFrameBufferScalar; // m_flFrameBufferAspectRatio / m_flPhysicalAspectRatio
	bool m_bInitialized;

	AspectRatioInfo_t() : 
		m_bIsWidescreen( false ), 
		m_bIsHidef( false ), 
		m_flFrameBufferAspectRatio( 4.0f / 3.0f ), 
		m_flPhysicalAspectRatio( 4.0f / 3.0f ),
		m_flFrameBuffertoPhysicalScalar( 1.0f ),
		m_flPhysicalToFrameBufferScalar( 1.0f ),
		m_bInitialized( false )
	{
	}
};

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

class IMaterialSystem
{
public:

	// Connection and lifetime (this is an app system of the launcher's group)
	bool Connect( CreateInterfaceFn factory );
	void Disconnect();
	void *QueryInterface( const char *pInterfaceName );
	InitReturnVal_t Init();
	void Shutdown();
	const AppSystemInfo_t *GetDependencies();
	AppSystemTier_t GetTier();
	void Reconnect( CreateInterfaceFn factory, const char *pInterfaceName );

	//---------------------------------------------------------
	// Initialization and shutdown
	//---------------------------------------------------------

	// Call this to initialize the material system
	// returns a method to create interfaces in the shader dll
	CreateInterfaceFn	Init( char const* pShaderAPIDLL, 
		IMaterialProxyFactory *pMaterialProxyFactory,
		CreateInterfaceFn fileSystemFactory,
		CreateInterfaceFn cvarFactory=nullptr );

	// Call this to set an explicit shader version to use 
	// Must be called before Init().
	void				SetShaderAPI( char const *pShaderAPIDLL );

	// Must be called before Init(), if you're going to call it at all...
	void				SetAdapter( int nAdapter, int nFlags );

	// Call this when the mod has been set up, which may occur after init
	// At this point, the game + gamebin paths have been set up
	void				ModInit();
	void				ModShutdown();

	//---------------------------------------------------------
	//
	//---------------------------------------------------------
	void				SetThreadMode( MaterialThreadMode_t mode, int nServiceThread = -1 );
	MaterialThreadMode_t GetThreadMode();
	bool				IsRenderThreadSafe( );
	void				ExecuteQueued();
		
				void            OnDebugEvent( const char * pEvent = "" );

	//---------------------------------------------------------
	// Config management
	//---------------------------------------------------------

	IMaterialSystemHardwareConfig *GetHardwareConfig( const char *pVersion, int *returnCode );


	// Call this before rendering each frame with the current config
	// for the material system.
	// Will do whatever is necessary to get the material system into the correct state
	// upon configuration change. .doesn't much else otherwise.
	bool				UpdateConfig( bool bForceUpdate );

	// Force this to be the config; update all material system convars to match the state
	// return true if lightmaps need to be redownloaded
	bool				OverrideConfig( const MaterialSystem_Config_t &config, bool bForceUpdate );

	// Get the current config for this video card (as last set by UpdateConfig)
	const MaterialSystem_Config_t &GetCurrentConfigForVideoCard() const;

	// Gets *recommended* configuration information associated with the display card, 
	// given a particular dx level to run under. 
	// Use dxlevel 0 to use the recommended dx level.
	// The function returns false if an invalid dxlevel was specified

	// UNDONE: To find out all convars affected by configuration, we'll need to change
	// the dxsupport.pl program to output all column headers into a single keyvalue block
	// and then we would read that in, and send it back to the client
	bool				GetRecommendedConfigurationInfo( int nDXLevel, KeyValues * pKeyValues );


	// -----------------------------------------------------------
	// Device methods
	// -----------------------------------------------------------

	// Gets the number of adapters...
	int					GetDisplayAdapterCount() const;

	// Returns the current adapter in use
	int					GetCurrentAdapter() const;

	// Returns info about each adapter
	void				GetDisplayAdapterInfo( int adapter, MaterialAdapterInfo_t& info ) const;

	// Returns the number of modes
	int					GetModeCount( int adapter ) const;

	// Returns mode information..
	void				GetModeInfo( int adapter, int mode, MaterialVideoMode_t& info ) const;

	void				AddModeChangeCallBack( ModeChangeCallbackFunc_t func );

	// Returns the mode info for the current display device
	void				GetDisplayMode( MaterialVideoMode_t& mode ) const;

	// Sets the mode...
	bool				SetMode( void* hwnd, const MaterialSystem_Config_t &config );

	bool				SupportsMSAAMode( int nMSAAMode );

	// FIXME: REMOVE! Get video card identitier
	const MaterialSystemHardwareIdentifier_t &GetVideoCardIdentifier( void ) const;

	// Use this to spew information about the 3D layer 
	void				SpewDriverInfo() const;

	// Get the image format of the back buffer. . useful when creating render targets, etc.
	void				GetBackBufferDimensions( int &width, int &height) const;
	ImageFormat			GetBackBufferFormat() const;
	const AspectRatioInfo_t	&GetAspectRatioInfo() const;

	bool				SupportsHDRMode( HDRType_t nHDRModede );


	// -----------------------------------------------------------
	// Window methods
	// -----------------------------------------------------------

	// Creates/ destroys a child window
	bool				AddView( void* hwnd );
	void				RemoveView( void* hwnd );

	// Sets the view
	void				SetView( void* hwnd );


	// -----------------------------------------------------------
	// Control flow
	// -----------------------------------------------------------

	void				BeginFrame( float frameTime );
	void				EndFrame( );
	void				Flush( bool flushHardware = false );
	uint32				GetCurrentFrameCount();

	/// FIXME: This stuff needs to be cleaned up and abstracted.
	// Stuff that gets exported to the launcher through the engine
	void				SwapBuffers( );

	// Flushes managed textures from the texture cacher
	void				EvictManagedResources();

	void				ReleaseResources(void);
	void				ReacquireResources(void );


	// -----------------------------------------------------------
	// Device loss/restore
	// -----------------------------------------------------------

	// Installs a function to be called when we need to release vertex buffers + textures
	void				AddReleaseFunc( MaterialBufferReleaseFunc_t func );
	void				RemoveReleaseFunc( MaterialBufferReleaseFunc_t func );

	// Installs a function to be called when we need to restore vertex buffers
	void				AddRestoreFunc( MaterialBufferRestoreFunc_t func );
	void				RemoveRestoreFunc( MaterialBufferRestoreFunc_t func );

	// Installs a function to be called when we need to delete objects at the end of the render frame
	void				AddEndFrameCleanupFunc( EndFrameCleanupFunc_t func );
	void				RemoveEndFrameCleanupFunc( EndFrameCleanupFunc_t func );

	// Gets called when the level is shuts down, will call the registered callback
	void				OnLevelShutdown();
	// Installs a function to be called when the level is shuts down
	bool				AddOnLevelShutdownFunc( OnLevelShutdownFunc_t func, void * pUserData );
	bool				RemoveOnLevelShutdownFunc( OnLevelShutdownFunc_t func, void * pUserData );

	void				OnLevelLoadingComplete();

	// Release temporary HW memory...
	void				ResetTempHWMemory( bool bExitingLevel = false );

	// For dealing with device lost in cases where SwapBuffers isn't called all the time (Hammer)
	void				HandleDeviceLost();


	// -----------------------------------------------------------
	// Shaders
	// -----------------------------------------------------------

	// Used to iterate over all shaders for editing purposes
	// GetShaders returns the number of shaders it actually found
	int					ShaderCount() const;
	int					GetShaders( int nFirstShader, int nMaxCount, IShader **ppShaderList ) const;

	// FIXME: Is there a better way of doing this?
	// Returns shader flag names for editors to be able to edit them
	int					ShaderFlagCount() const;
	const char *		ShaderFlagName( int nIndex ) const;

	// Gets the actual shader fallback for a particular shader
	void				GetShaderFallback( const char *pShaderName, char *pFallbackShader, int nFallbackLength );


	// -----------------------------------------------------------
	// Material proxies
	// -----------------------------------------------------------

	IMaterialProxyFactory *GetMaterialProxyFactory();

	// Sets the material proxy factory. Calling this causes all materials to be uncached.
	void				SetMaterialProxyFactory( IMaterialProxyFactory* pFactory );


	// -----------------------------------------------------------
	// Editor mode
	// -----------------------------------------------------------

	// Used to enable editor materials. Must be called before Init.
	void				EnableEditorMaterials();
	void                EnableGBuffers();

	//---------------------------------------------------------
	// Debug support
	//---------------------------------------------------------

	void				DebugPrintUsedMaterials( const char *pSearchSubString, bool bVerbose );
	void				DebugPrintUsedTextures( void );

	void				ToggleSuppressMaterial( char const* pMaterialName );
	void				ToggleDebugMaterial( char const* pMaterialName );


	//---------------------------------------------------------
	// Misc features
	//---------------------------------------------------------
	//returns whether fast clipping is being used or not - needed to be exposed for better per-object clip behavior
	bool				UsingFastClipping( void );

	int					StencilBufferBits( void ); //number of bits per pixel in the stencil buffer


	//---------------------------------------------------------
	// Material and texture management
	//---------------------------------------------------------

	// uncache all materials. .  good for forcing reload of materials.
	void				UncacheAllMaterials( );

	// Remove any materials from memory that aren't in use as determined
	// by the IMaterial's reference count.
	void				UncacheUnusedMaterials( bool bRecomputeStateSnapshots = false );

	// Load any materials into memory that are to be used as determined
	// by the IMaterial's reference count.
	void				CacheUsedMaterials( );

	// Force all textures to be reloaded from disk.
	void				ReloadTextures( );

	// Reloads materials
	void				ReloadMaterials( const char *pSubString = nullptr );

	// Create a procedural material. The keyvalues looks like a VMT file
	IMaterial *			CreateMaterial( const char *pMaterialName, KeyValues *pVMTKeyValues );

	// Find a material by name.
	// The name of a material is a full path to 
	// the vmt file starting from "hl2/materials" (or equivalent) without
	// a file extension.
	// eg. "dev/dev_bumptest" refers to somethign similar to:
	// "d:/hl2/hl2/materials/dev/dev_bumptest.vmt"
	//
	// Most of the texture groups for pTextureGroupName are listed in texture_group_names.h.
	// 
	// Note: if the material can't be found, this returns a checkerboard material. You can 
	// find out if you have that material by calling IMaterial::IsErrorMaterial().
	// (Or use the global IsErrorMaterial function, which checks if it's null too).
	IMaterial *			FindMaterial( char const* pMaterialName, const char *pTextureGroupName, bool complain = true, const char *pComplainPrefix = nullptr );

	bool				LoadKeyValuesFromVMTFile( KeyValues &vmtKeyValues, const char *pMaterialName, bool bUsesUNCFilename  );

	//---------------------------------
	// This is the interface for knowing what materials are available
	// is to use the following functions to get a list of materials.  The
	// material names will have the full path to the material, and that is the 
	// only way that the directory structure of the materials will be seen through this
	// interface.
	// NOTE:  This is mostly for worldcraft to get a list of materials to put
	// in the "texture" browser.in Worldcraft
	MaterialHandle_t	FirstMaterial() const;

	// returns InvalidMaterial if there isn't another material.
	// WARNING: you must call GetNextMaterial until it returns NULL, 
	// otherwise there will be a memory leak.
	MaterialHandle_t	NextMaterial( MaterialHandle_t h ) const;

	// This is the invalid material
	MaterialHandle_t	InvalidMaterial() const;

	// Returns a particular material
	IMaterial*			GetMaterial( MaterialHandle_t h ) const;

	// Get the total number of materials in the system.  These aren't just the used
	// materials, but the complete collection.
	int					GetNumMaterials( ) const;

	//---------------------------------

	ITexture *			FindTexture( char const* pTextureName, const char *pTextureGroupName, bool complain = true, int nAdditionalCreationFlags = 0  );

	// Checks to see if a particular texture is loaded
	bool				IsTextureLoaded( char const* pTextureName ) const;

	// Creates a procedural texture
	ITexture *			CreateProceduralTexture( const char	*pTextureName, 
		const char *pTextureGroupName, 
		int w, 
		int h, 
		ImageFormat fmt, 
		int nFlags );


	//
	// Render targets
	//
	void				BeginRenderTargetAllocation();
	void				EndRenderTargetAllocation(); // Simulate an Alt-Tab in here, which causes a release/restore of all resources

	// Creates a render target
	// If depth == true, a depth buffer is also allocated. If not, then
	// the screen's depth buffer is used.
	// Creates a texture for use as a render target
	ITexture *			CreateRenderTargetTexture( int w, 
		int h, 
		RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
		ImageFormat	format, 
		MaterialRenderTargetDepth_t depth = MATERIAL_RT_DEPTH_SHARED );

	ITexture *			CreateNamedRenderTargetTextureEx(  const char *pRTName,				// Pass in NULL here for an unnamed render target.
		int w, 
		int h, 
		RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
		ImageFormat format, 
		MaterialRenderTargetDepth_t depth = MATERIAL_RT_DEPTH_SHARED, 
		unsigned int textureFlags = TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		unsigned int renderTargetFlags = 0 );

	ITexture *			CreateNamedRenderTargetTexture( const char *pRTName, 
		int w, 
		int h, 
		RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
		ImageFormat format, 
		MaterialRenderTargetDepth_t depth = MATERIAL_RT_DEPTH_SHARED, 
		bool bClampTexCoords = true, 
		bool bAutoMipMap = false );

	// Must be called between the above Begin-End calls!
	ITexture *			CreateNamedRenderTargetTextureEx2( const char *pRTName,				// Pass in NULL here for an unnamed render target.
		int w, 
		int h, 
		RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
		ImageFormat format, 
		MaterialRenderTargetDepth_t depth = MATERIAL_RT_DEPTH_SHARED, 
		unsigned int textureFlags = TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		unsigned int renderTargetFlags = 0 );

	// -----------------------------------------------------------
	// Lightmaps
	// -----------------------------------------------------------

	// To allocate lightmaps, sort the whole world by material twice.
	// The first time through, call AllocateLightmap for every surface.
	// that has a lightmap.
	// The second time through, call AllocateWhiteLightmap for every 
	// surface that expects to use shaders that expect lightmaps.
	void				BeginLightmapAllocation( );
	void				EndLightmapAllocation( );

	// To clean up lightmaps, we exposed this so we can call this function in queueloader before it loads the next map data
	void				CleanupLightmaps();

	// returns the sorting id for this surface
	int 				AllocateLightmap( int width, int height, 
		int offsetIntoLightmapPage[2],
		IMaterial *pMaterial );
	// returns the sorting id for this surface
	int					AllocateWhiteLightmap( IMaterial *pMaterial );

	// lightmaps are in linear color space
	// lightmapPageID is returned by GetLightmapPageIDForSortID
	// lightmapSize and offsetIntoLightmapPage are returned by AllocateLightmap.
	// You should never call UpdateLightmap for a lightmap allocated through
	// AllocateWhiteLightmap.
	void				UpdateLightmap( int lightmapPageID, int lightmapSize[2],
		int offsetIntoLightmapPage[2], 
		float *pFloatImage, float *pFloatImageBump1,
		float *pFloatImageBump2, float *pFloatImageBump3 );

	// fixme: could just be an array of ints for lightmapPageIDs since the material
	// for a surface is already known.
	int					GetNumSortIDs( );
	void				GetSortInfo( MaterialSystem_SortInfo_t *sortInfoArray );

	// Read the page size of an existing lightmap by sort id (returned from AllocateLightmap())
	void				GetLightmapPageSize( int lightmap, int *width, int *height ) const;

	void				ResetMaterialLightmapPageInfo();

	// -----------------------------------------------------------
	// Stereo
	// -----------------------------------------------------------
	bool				IsStereoSupported();
	bool				IsStereoActiveThisFrame() const;
	void				NVStereoUpdate();

	void				ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil = false );

	// -----------------------------------------------------------
	// X360 specifics
	// -----------------------------------------------------------


	void SpinPresent( uint nFrames );

	// -----------------------------------------------------------
	// Access the render contexts
	// -----------------------------------------------------------
	IMatRenderContext *	GetRenderContext();

	void				BeginUpdateLightmaps( void );
	void				EndUpdateLightmaps( void );

	// -----------------------------------------------------------
	// Methods to force the material system into non-threaded, non-queued mode
	// -----------------------------------------------------------
	MaterialLock_t		Lock();
	void				Unlock( MaterialLock_t );

	bool				SupportsCSAAMode( int nNumSamples, int nQualityLevel );

	void				RemoveModeChangeCallBack( ModeChangeCallbackFunc_t func );

	// Finds or create a procedural material.
	IMaterial *			FindProceduralMaterial( const char *pMaterialName, const char *pTextureGroupName, KeyValues *pVMTKeyValues = nullptr );

	void				AddTextureAlias( const char *pAlias, const char *pRealName );
	void				RemoveTextureAlias( const char *pAlias );

	// returns a lightmap page ID for this allocation, -1 if none available
	// frameID is a number that should be changed every frame to prevent locking any textures that are
	// being used to draw in the previous frame
	int					AllocateDynamicLightmap( int lightmapSize[2], int *pOutOffsetIntoPage, int frameID );

	void				SetExcludedTextures( const char *pScriptName, bool bUsingWeaponModelCache );
	void				UpdateExcludedTextures( void );
	void				ClearForceExcludes( void );

	bool				IsInFrame( ) const;

	void				CompactMemory();

	// Get stats on GPU memory usage
	void				GetGPUMemoryStats( GPUMemoryStats &stats );

	// For sv_pure mode. The filesystem figures out which files the client needs to reload to be "pure" ala the server's preferences.
	void ReloadFilesInList( IFileList *pFilesToReload );

	// Get information about the texture for texture management tools
	bool				GetTextureInformation( char const *szTextureName, MaterialTextureInfo_t &info ) const;

	// call this once the render targets are allocated permanently at the beginning of the game
	void FinishRenderTargetAllocation( void );
	
	void ReEnableRenderTargetAllocation_IRealizeIfICallThisAllTexturesWillBeUnloadedAndLoadTimeWillSufferHorribly( void );
	bool				AllowThreading( bool bAllow, int nServiceThread );

	bool				GetRecommendedVideoConfig( KeyValues *pKeyValues );

	bool				CanDownloadTextures() const;
	int					GetNumLightmapPages() const;

	void RegisterPaintmapDataManager( IPaintmapDataManager *pDataManager ); //You supply an interface we can query for bits, it gives back an interface you can use to drive updates
	void BeginUpdatePaintmaps( void );
	void EndUpdatePaintmaps( void );
	void UpdatePaintmap( int paintmap, BYTE* pPaintData, int numRects, Rect_t* pRects );

	// Must be called between the BeginRenderTargetAllocation-EndRenderTargetAllocation calls!
	//Creates a render target capable of having multiple, swappable, textures while using the same name
	ITexture *			CreateNamedMultiRenderTargetTexture( const char *pRTName,				// Pass in NULL here for an unnamed render target.
		int w, 
		int h, 
		RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
		ImageFormat format, 
		MaterialRenderTargetDepth_t depth = MATERIAL_RT_DEPTH_SHARED, 
		unsigned int textureFlags = TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		unsigned int renderTargetFlags = 0 );

	void				RefreshFrontBufferNonInteractive();

	uint32 GetFrameTimestamps( ApplicationPerformanceCountersInfo_t &apci, ApplicationInstantCountersInfo_t & aici );
	

	// Installs a function to be called when we need to perform operation before new rendering context is started
	void				AddEndFramePriorToNextContextFunc( EndFramePriorToNextContextFunc_t func );
	void				RemoveEndFramePriorToNextContextFunc( EndFramePriorToNextContextFunc_t func );

	ICustomMaterialManager *GetCustomMaterialManager();
	ICompositeTextureGenerator *GetCompositeTextureGenerator();
};


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
class IMatRenderContext
{
public:
	int					AddRef();
	int					Release();

	void				BeginRender();
	void				EndRender();

	void				Flush( bool flushHardware = false );

	void				BindLocalCubemap( ITexture *pTexture );

	// pass in an ITexture (that is build with "rendertarget" "1") or
	// pass in NULL for the regular backbuffer.
	void				SetRenderTarget( ITexture *pTexture );
	ITexture *			GetRenderTarget( void );

	void				GetRenderTargetDimensions( int &width, int &height) const;

	// Bind a material is current for rendering.
	void				Bind( IMaterial *material, void *proxyData = 0 );
	// Bind a lightmap page current for rendering.  You only have to 
	// do this for materials that require lightmaps.
	void				BindLightmapPage( int lightmapPageID );

	// inputs are between 0 and 1
	void				DepthRange( float zNear, float zFar );

	void				ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil = false );

	// read to a unsigned char rgb image.
	void				ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture = nullptr );
	void				ReadPixelsAsync( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture = nullptr, CThreadEvent *pPixelsReadEvent = nullptr );
	void				ReadPixelsAsyncGetResult( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, CThreadEvent *pGetResultEvent = nullptr );

	// Sets lighting
	void				SetLightingState( const MaterialLightingState_t& state );
	void				SetLights( int nCount, const LightDesc_t *pLights );

	// The faces of the cube are specified in the same order as cubemap textures
	void				SetAmbientLightCube( Vector4D cube[6] );

	// Blit the backbuffer to the framebuffer texture
	void				CopyRenderTargetToTexture( ITexture *pTexture );

	// Set the current texture that is a copy of the framebuffer.
	void				SetFrameBufferCopyTexture( ITexture *pTexture, int textureIndex = 0 );
	ITexture		   *GetFrameBufferCopyTexture( int textureIndex );

	//
	// end vertex array api
	//

	// matrix api
	void				MatrixMode( MaterialMatrixMode_t matrixMode );
	void				PushMatrix( void );
	void				PopMatrix( void );
	void				LoadMatrix( VMatrix const& matrix );
	void				LoadMatrix( matrix3x4_t const& matrix );
	void				MultMatrix( VMatrix const& matrix );
	void				MultMatrix( matrix3x4_t const& matrix );
	void				MultMatrixLocal( VMatrix const& matrix );
	void				MultMatrixLocal( matrix3x4_t const& matrix );
	void				GetMatrix( MaterialMatrixMode_t matrixMode, VMatrix *matrix );
	void				GetMatrix( MaterialMatrixMode_t matrixMode, matrix3x4_t *matrix );
	void				LoadIdentity( void );
	void				Ortho( double left, double top, double right, double bottom, double zNear, double zFar );
	void				PerspectiveX( double fovx, double aspect, double zNear, double zFar );
	void				PickMatrix( int x, int y, int width, int height );
	void				Rotate( float angle, float x, float y, float z );
	void				Translate( float x, float y, float z );
	void				Scale( float x, float y, float z );
	// end matrix api

	// Sets/gets the viewport
	void				Viewport( int x, int y, int width, int height );
	void				GetViewport( int& x, int& y, int& width, int& height ) const;

	// The cull mode
	void				CullMode( MaterialCullMode_t cullMode );
	void				FlipCullMode( void ); //CW->CCW or CCW->CW, intended for mirror support where the view matrix is flipped horizontally

	// Shadow buffer generation
	void				BeginGeneratingCSMs();
	void				EndGeneratingCSMs();
	void				PerpareForCascadeDraw( int cascade, float fShadowSlopeScaleDepthBias, float fShadowDepthBias );

	// end matrix api

	// This could easily be extended to a general user clip plane
	void				SetHeightClipMode( MaterialHeightClipMode_t nHeightClipMode );
	// garymcthack : fog z is always used for heightclipz for now.
	void				SetHeightClipZ( float z );

	// Fog methods...
	void				FogMode( MaterialFogMode_t fogMode );
	void				FogStart( float fStart );
	void				FogEnd( float fEnd );
	void				SetFogZ( float fogZ );
	MaterialFogMode_t	GetFogMode( void );

	void				FogColor3f( float r, float g, float b );
	void				FogColor3fv( float const* rgb );
	void				FogColor3ub( unsigned char r, unsigned char g, unsigned char b );
	void				FogColor3ubv( unsigned char const* rgb );

	void				GetFogColor( unsigned char *rgb );

	// Sets the number of bones for skinning
	void				SetNumBoneWeights( int numBones );

	// Creates/destroys Mesh
	IMesh* CreateStaticMesh( VertexFormat_t fmt, const char *pTextureBudgetGroup, IMaterial * pMaterial = nullptr, VertexStreamSpec_t *pStreamSpec = nullptr );
	void DestroyStaticMesh( IMesh* mesh );

	// Gets the dynamic mesh associated with the currently bound material
	// note that you've got to render the mesh before calling this function 
	// a second time. Clients should *not* call DestroyStaticMesh on the mesh 
	// returned by this call.
	// Use buffered = false if you want to not have the mesh be buffered,
	// but use it instead in the following pattern:
	//		meshBuilder.Begin
	//		meshBuilder.End
	//		Draw partial
	//		Draw partial
	//		Draw partial
	//		meshBuilder.Begin
	//		meshBuilder.End
	//		etc
	// Use Vertex or Index Override to supply a static vertex or index buffer
	// to use in place of the dynamic buffers.
	//
	// If you pass in a material in pAutoBind, it will automatically bind the
	// material. This can be helpful since you must bind the material you're
	// going to use BEFORE calling GetDynamicMesh.
	IMesh* GetDynamicMesh( 
		bool buffered = true, 
		IMesh* pVertexOverride = 0,	
		IMesh* pIndexOverride = 0, 
		IMaterial *pAutoBind = 0 );

	// ------------ New Vertex/Index Buffer interface ----------------------------
	// Do we need support for bForceTempMesh and bSoftwareVertexShader?
	// I don't think we use bSoftwareVertexShader anymore. .need to look into bForceTempMesh.
	IVertexBuffer *CreateStaticVertexBuffer( VertexFormat_t fmt, int nVertexCount, const char *pTextureBudgetGroup );
	IIndexBuffer *CreateStaticIndexBuffer( MaterialIndexFormat_t fmt, int nIndexCount, const char *pTextureBudgetGroup );
	void DestroyVertexBuffer( IVertexBuffer * );
	void DestroyIndexBuffer( IIndexBuffer * );
	// Do we need to specify the stream here in the case of locking multiple dynamic VBs on different streams?
	IVertexBuffer *GetDynamicVertexBuffer( int streamID, VertexFormat_t vertexFormat, bool bBuffered = true );
	IIndexBuffer *GetDynamicIndexBuffer();
	void BindVertexBuffer( int streamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions = 1 );
	void BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes );
	void Draw( MaterialPrimitiveType_t primitiveType, int firstIndex, int numIndices );
	// ------------ End ----------------------------

	// Selection mode methods
	int  SelectionMode( bool selectionMode );
	void SelectionBuffer( unsigned int* pBuffer, int size );
	void ClearSelectionNames( );
	void LoadSelectionName( int name );
	void PushSelectionName( int name );
	void PopSelectionName();

	// Sets the Clear Color for ClearBuffer....
	void		ClearColor3ub( unsigned char r, unsigned char g, unsigned char b );
	void		ClearColor4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a );

	// Allows us to override the depth buffer setting of a material
	void	OverrideDepthEnable( bool bEnable, bool bDepthWriteEnable, bool bDepthTestEnable = true );

	// FIXME: This is a hack required for NVidia/XBox, can they fix in drivers?
	void	DrawScreenSpaceQuad( IMaterial* pMaterial );

	// For debugging and building recording files. This will stuff a token into the recording file,
	// then someone doing a playback can watch for the token.
	void	SyncToken( const char *pToken );

	// FIXME: REMOVE THIS FUNCTION!
	// The only reason why it's not gone is because we're a week from ship when I found the bug in it
	// and everything's tuned to use it.
	// It's returning values which are 2x too big (it's returning sphere diameter x2)
	// Use ComputePixelDiameterOfSphere below in all new code instead.
	float	ComputePixelWidthOfSphere( const Vector& origin, float flRadius );

	//
	// Occlusion query support
	//

	// Allocate and delete query objects.
	OcclusionQueryObjectHandle_t CreateOcclusionQueryObject( void );
	void DestroyOcclusionQueryObject( OcclusionQueryObjectHandle_t );

	// Bracket drawing with begin and end so that we can get counts next frame.
	void BeginOcclusionQueryDrawing( OcclusionQueryObjectHandle_t );
	void EndOcclusionQueryDrawing( OcclusionQueryObjectHandle_t );

	// Get the number of pixels rendered between begin and end on an earlier frame.
	// Calling this in the same frame is a huge perf hit!
	int OcclusionQuery_GetNumPixelsRendered( OcclusionQueryObjectHandle_t );

	void SetFlashlightMode( bool bEnable );

	void SetFlashlightState( const FlashlightState_t &state, const VMatrix &worldToTexture );

	bool IsCascadedShadowMapping() const;
	void SetCascadedShadowMapping( bool bEnable );
	void SetCascadedShadowMappingState( const CascadedShadowMappingState_t &state, ITexture *pDepthTextureAtlas );

	// Gets the current height clip mode
	MaterialHeightClipMode_t GetHeightClipMode( );

	// This returns the diameter of the sphere in pixels based on 
	// the current model, view, + projection matrices and viewport.
	float	ComputePixelDiameterOfSphere( const Vector& vecAbsOrigin, float flRadius );

	// By default, the material system applies the VIEW and PROJECTION matrices	to the user clip
	// planes (which are specified in world space) to generate projection-space user clip planes
	// Occasionally (for the particle system in hl2, for example), we want to override that
	// behavior and explictly specify a ViewProj transform for user clip planes
	void	EnableUserClipTransformOverride( bool bEnable );
	void	UserClipTransform( const VMatrix &worldToView );

	bool GetFlashlightMode() const;
	
	bool IsCullingEnabledForSinglePassFlashlight() const;
	void EnableCullingForSinglePassFlashlight( bool bEnable );

	// Used to make the handle think it's never had a successful query before
	void ResetOcclusionQueryObject( OcclusionQueryObjectHandle_t );

	// Creates/destroys morph data associated w/ a particular material
	IMorph *CreateMorph( MorphFormat_t format, const char *pDebugName );
	void DestroyMorph( IMorph *pMorph );

	// Binds the morph data for use in rendering
	void BindMorph( IMorph *pMorph );

	// Sets flexweights for rendering
	void SetFlexWeights( int nFirstWeight, int nCount, const MorphWeight_t* pWeights );

	// Allocates temp render data. Renderdata goes out of scope at frame end in multicore
	// Renderdata goes out of scope after refcount goes to zero in singlecore.
	// Locking/unlocking increases + decreases refcount
	void *			LockRenderData( int nSizeInBytes );
	void			UnlockRenderData( void *pData );

	// Typed version. If specified, pSrcData is copied into the locked memory.
	template< class E > E*  LockRenderDataTyped( int nCount, const E* pSrcData = nullptr );

	// Temp render data gets immediately freed after it's all unlocked in single core.
	// This prevents it from being freed
	void			AddRefRenderData();	
	void			ReleaseRenderData();

	// Returns whether a pointer is render data. NOTE: passing NULL returns true
	bool			IsRenderData( const void *pData ) const;

	// Read w/ stretch to a host-memory buffer
	void ReadPixelsAndStretch( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *pBuffer, ImageFormat dstFormat, int nDstStride );

	// Gets the window size
	void GetWindowSize( int &width, int &height ) const;

	// This function performs a texture map from one texture map to the render destination, doing
	// all the necessary pixel/texel coordinate fix ups. fractional values can be used for the
	// src_texture coordinates to get linear sampling - integer values should produce 1:1 mappings
	// for non-scaled operations.
	void DrawScreenSpaceRectangle( 
		IMaterial *pMaterial,
		int destx, int desty,
		int width, int height,
		float src_texture_x0, float src_texture_y0,			// which texel you want to appear at
		// destx/y
		float src_texture_x1, float src_texture_y1,			// which texel you want to appear at
		// destx+width-1, desty+height-1
		int src_texture_width, int src_texture_height,		// needed for fixup
		void *pClientRenderable = nullptr,
		int nXDice = 1,
		int nYDice = 1 );

	void LoadBoneMatrix( int boneIndex, const matrix3x4_t& matrix );

	// This version will push the current rendertarget + current viewport onto the stack
	void PushRenderTargetAndViewport( );

	// This version will push a new rendertarget + a maximal viewport for that rendertarget onto the stack
	void PushRenderTargetAndViewport( ITexture *pTexture );

	// This version will push a new rendertarget + a specified viewport onto the stack
	void PushRenderTargetAndViewport( ITexture *pTexture, int nViewX, int nViewY, int nViewW, int nViewH );

	// This version will push a new rendertarget + a specified viewport onto the stack
	void PushRenderTargetAndViewport( ITexture *pTexture, ITexture *pDepthTexture, int nViewX, int nViewY, int nViewW, int nViewH );

	// This will pop a rendertarget + viewport
	void PopRenderTargetAndViewport( void );

	// Binds a particular texture as the current lightmap
	void BindLightmapTexture( ITexture *pLightmapTexture );

	// Blit a subrect of the current render target to another texture
	void CopyRenderTargetToTextureEx( ITexture *pTexture, int nRenderTargetID, Rect_t *pSrcRect, Rect_t *pDstRect = nullptr );
	void CopyTextureToRenderTargetEx( int nRenderTargetID, ITexture *pTexture, Rect_t *pSrcRect, Rect_t *pDstRect = nullptr );

	// Special off-center perspective matrix for DoF, MSAA jitter and poster rendering
	void PerspectiveOffCenterX( double fovx, double aspect, double zNear, double zFar, double bottom, double top, double left, double right );

	// Rendering parameters control special drawing modes withing the material system, shader
	// system, shaders, and engine. renderparm.h has their definitions.
	void SetFloatRenderingParameter(int parm_number, float value);
	void SetIntRenderingParameter(int parm_number, int value);
	void SetVectorRenderingParameter(int parm_number, Vector const &value);

	// stencil buffer operations.
	void SetStencilState( const ShaderStencilState_t &state );
	void ClearStencilBufferRectangle(int xmin, int ymin, int xmax, int ymax,int value);	

	void SetRenderTargetEx( int nRenderTargetID, ITexture *pTexture );

	// rendering clip planes, beware that only the most recently pushed plane will actually be used in a sizeable chunk of hardware configurations
	// and that changes to the clip planes mid-frame while UsingFastClipping() is true will result unresolvable depth inconsistencies
	void PushCustomClipPlane( const float *pPlane );
	void PopCustomClipPlane( void );

	// Returns the number of vertices + indices we can render using the dynamic mesh
	// Passing true in the second parameter will return the max # of vertices + indices
	// we can use before a flush is provoked and may return different values 
	// if called multiple times in succession. 
	// Passing false into the second parameter will return
	// the maximum possible vertices + indices that can be rendered in a single batch
	void GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices );

	// Returns the max possible vertices + indices to render in a single draw call
	int GetMaxVerticesToRender( IMaterial *pMaterial );
	int GetMaxIndicesToRender( );
	void DisableAllLocalLights();
	int CompareMaterialCombos( IMaterial *pMaterial1, IMaterial *pMaterial2, int lightMapID1, int lightMapID2 );

	IMesh *GetFlexMesh();

	void SetFlashlightStateEx( const FlashlightState_t &state, const VMatrix &worldToTexture, ITexture *pFlashlightDepthTexture );

	// Returns the currently bound local cubemap
	ITexture *GetLocalCubemap( );

	// This is a version of clear buffers which will only clear the buffer at pixels which pass the stencil test
	void ClearBuffersObeyStencil( bool bClearColor, bool bClearDepth );

	//enables/disables all entered clipping planes, returns the input from the last time it was called.
	bool EnableClipping( bool bEnable );

	//get fog distances entered with FogStart(), FogEnd(), and SetFogZ()
	void GetFogDistances( float *fStart, float *fEnd, float *fFogZ );

	// Hooks for firing PIX events from outside the Material System...
	void BeginPIXEvent( unsigned long color, const char *szName );
	void EndPIXEvent();
	void SetPIXMarker( unsigned long color, const char *szName );

	// Batch API
	// from changelist 166623:
	// - replaced obtuse material system batch usage with an explicit and easier to thread API
	void BeginBatch( IMesh* pIndices );
	void BindBatch( IMesh* pVertices, IMaterial *pAutoBind = nullptr );
	void DrawBatch( MaterialPrimitiveType_t primType, int firstIndex, int numIndices );
	void EndBatch();

	// Raw access to the call queue, which can be NULL if not in a queued mode
	ICallQueue *GetCallQueue();

	// Returns the world-space camera position
	void GetWorldSpaceCameraPosition( Vector *pCameraPos );
	void GetWorldSpaceCameraVectors( Vector *pVecForward, Vector *pVecRight, Vector *pVecUp );

	// Set a linear vector color scale for all 3D rendering.
	// A value of [1.0f, 1.0f, 1.0f] should match non-tone-mapped rendering.
	void				SetToneMappingScaleLinear( const Vector &scale );
	Vector				GetToneMappingScaleLinear( void );

	void				SetShadowDepthBiasFactors( float fSlopeScaleDepthBias, float fDepthBias );

	// Apply stencil operations to every pixel on the screen without disturbing depth or color buffers
	void				PerformFullScreenStencilOperation( void );

	// Sets lighting origin for the current model (needed to convert directional lights to points)
	void				SetLightingOrigin( Vector vLightingOrigin );

	// Set scissor rect for rendering
	void				PushScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom );
	void				PopScissorRect();

	// Methods used to build the morph accumulator that is read from when HW morphing is enabled.
	void				BeginMorphAccumulation();
	void				EndMorphAccumulation();
	void				AccumulateMorph( IMorph* pMorph, int nMorphCount, const MorphWeight_t* pWeights );

	void				PushDeformation( DeformationBase_t const *Deformation );
	void				PopDeformation( );
	int					GetNumActiveDeformations() const;

	bool				GetMorphAccumulatorTexCoord( Vector2D *pTexCoord, IMorph *pMorph, int nVertex );

	// Version of get dynamic mesh that specifies a specific vertex format
	IMesh*				GetDynamicMeshEx( VertexFormat_t vertexFormat, bool bBuffered = true, 
		IMesh* pVertexOverride = 0,	IMesh* pIndexOverride = 0, IMaterial *pAutoBind = 0 );

	void				FogMaxDensity( float flMaxDensity );



	void                AntiAliasingHint( int nHint );

	IMaterial *GetCurrentMaterial();
	int  GetCurrentNumBones() const;
	void *GetCurrentProxy();

	// Color correction related methods..
	// Client cannot call IColorCorrectionSystem directly because it is not thread-safe
	// FIXME: Make IColorCorrectionSystem threadsafe?
	void EnableColorCorrection( bool bEnable );
	ColorCorrectionHandle_t AddLookup( const char *pName );
	bool RemoveLookup( ColorCorrectionHandle_t handle );
	void LockLookup( ColorCorrectionHandle_t handle );
	void LoadLookup( ColorCorrectionHandle_t handle, const char *pLookupName );
	void UnlockLookup( ColorCorrectionHandle_t handle );
	void SetLookupWeight( ColorCorrectionHandle_t handle, float flWeight );
	void ResetLookupWeights( );
	void SetResetable( ColorCorrectionHandle_t handle, bool bResetable );

	//There are some cases where it's simply not reasonable to update the full screen depth texture (mostly on PC).
	//Use this to mark it as invalid and use a dummy texture for depth reads.
	void SetFullScreenDepthTextureValidityFlag( bool bIsValid );

	// A special path used to tick the front buffer while loading on the 360
	void SetNonInteractiveLogoTexture( ITexture *pTexture, float flNormalizedX, float flNormalizedY, float flNormalizedW, float flNormalizedH );
	void SetNonInteractivePacifierTexture( ITexture *pTexture, float flNormalizedX, float flNormalizedY, float flNormalizedSize );
	void SetNonInteractiveTempFullscreenBuffer( ITexture *pTexture, MaterialNonInteractiveMode_t mode );
	void EnableNonInteractiveMode( MaterialNonInteractiveMode_t mode );
	void RefreshFrontBufferNonInteractive();

	// Flip culling state (swap CCW <-> CW)
	void FlipCulling( bool bFlipCulling );

	void SetTextureRenderingParameter(int parm_number, ITexture *pTexture);

	//only actually sets a bool that can be read in from shaders, doesn't do any of the legwork
	void EnableSinglePassFlashlightMode( bool bEnable );

	// Are we in Single Pass Flashlight Mode?
	bool SinglePassFlashlightModeEnabled() const;
	
	// Draws instances with different meshes
	void DrawInstances( int nInstanceCount, const MeshInstanceData_t *pInstance );
	
	// Allows us to override the color/alpha write settings of a material
	void OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable );
	void OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable );

	void ClearBuffersObeyStencilEx( bool bClearColor, bool bClearAlpha, bool bClearDepth );

	// Subdivision surface interface
	int GetSubDBufferWidth();
	float* LockSubDBuffer( int nNumRows );
	void UnlockSubDBuffer();

	// Update current frame's game time for the shader api.
	void UpdateGameTime( float flTime );

	void			PrintfVA( char *fmt, std::va_list vargs );
	void			Printf( char *fmt, ... );
	float			Knob( char *knobname, float *setvalue = nullptr );

	// RocketUI rendering. The argument is an opaque RmlUi command list produced
	// on the main thread (RocketUI()->RecordHUD/RecordMenu); the callee replays
	// and frees it. nullptr is a safe no-op.
	void RenderRocketHUD(void *cmdList);
	void RenderRocketMenu(void *cmdList);

	void SetRenderingPaint( bool bEnable );

	// Draws batches of different materials using a faster API
	//virtual void DrawBatchedMaterials( int nBatchCount, const MaterialBatchData_t *pBatches ) = 0;

	ColorCorrectionHandle_t FindLookup( const char *pName );
};


template< class E > inline E* IMatRenderContext::LockRenderDataTyped( int nCount, const E* pSrcData )
{
	int nSizeInBytes = nCount * sizeof(E);
	E *pDstData = (E*)LockRenderData( nSizeInBytes );
	if ( pSrcData && pDstData )
	{
		std::memcpy( pDstData, pSrcData, nSizeInBytes );
	}
	return pDstData;
}


//-----------------------------------------------------------------------------
// Utility class for addreffing/releasing render data (prevents freeing on single core)
//-----------------------------------------------------------------------------
class CMatRenderDataReference
{
public:
	CMatRenderDataReference();
	CMatRenderDataReference( IMatRenderContext* pRenderContext );
	~CMatRenderDataReference();
	void Lock( IMatRenderContext *pRenderContext );
	void Release();

private:
	IMatRenderContext *m_pRenderContext;
};


inline CMatRenderDataReference::CMatRenderDataReference()
{
	m_pRenderContext = nullptr;
}

inline CMatRenderDataReference::CMatRenderDataReference( IMatRenderContext* pRenderContext )
{
	m_pRenderContext = nullptr;
	Lock( pRenderContext );
}

inline CMatRenderDataReference::~CMatRenderDataReference()
{
	Release();
}

inline void CMatRenderDataReference::Lock( IMatRenderContext* pRenderContext )
{
	if ( !m_pRenderContext )
	{
		m_pRenderContext = pRenderContext;
		m_pRenderContext->AddRefRenderData( );
	}
}

inline void CMatRenderDataReference::Release()
{
	if ( m_pRenderContext )
	{
		m_pRenderContext->ReleaseRenderData( );
		m_pRenderContext = nullptr;
	}
}


//-----------------------------------------------------------------------------
// Utility class for locking/unlocking render data
//-----------------------------------------------------------------------------
template< typename E > 
class CMatRenderData
{
public:
	CMatRenderData( IMatRenderContext* pRenderContext );
	CMatRenderData( IMatRenderContext* pRenderContext, int nCount, const E *pSrcData = nullptr );
	~CMatRenderData();
	E* Lock( int nCount, const E* pSrcData = nullptr ); 
	void Release();
	bool IsValid() const;
	const E* Base() const;
	E* Base();
	const E& operator[]( int i ) const;
	E& operator[]( int i );

private:
	IMatRenderContext* m_pRenderContext;
	E *m_pRenderData;
	int m_nCount;
	bool m_bNeedsUnlock;
};

template< typename E >
inline CMatRenderData<E>::CMatRenderData( IMatRenderContext* pRenderContext )
{
	m_pRenderContext = pRenderContext;
	m_nCount = 0;
	m_pRenderData = 0;
	m_bNeedsUnlock = false;
}

template< typename E >
inline CMatRenderData<E>::CMatRenderData( IMatRenderContext* pRenderContext, int nCount, const E* pSrcData )
{
	m_pRenderContext = pRenderContext;
	m_nCount = 0;
	m_pRenderData = 0;
	m_bNeedsUnlock = false;
	Lock( nCount, pSrcData );
}

template< typename E >
inline CMatRenderData<E>::~CMatRenderData()
{
	Release();
}

template< typename E >
inline bool CMatRenderData<E>::IsValid() const
{
	return m_pRenderData != nullptr;
}

template< typename E >
inline E* CMatRenderData<E>::Lock( int nCount, const E* pSrcData )
{
	m_nCount = nCount;
	if ( pSrcData && m_pRenderContext->IsRenderData( pSrcData ) )
	{
		// Yes, we're const-casting away, but that should be ok since 
		// the src data is render data
		m_pRenderData = const_cast<E*>( pSrcData );
		m_pRenderContext->AddRefRenderData();
		m_bNeedsUnlock = false;
		return m_pRenderData;
	}
	m_pRenderData = m_pRenderContext->LockRenderDataTyped<E>( nCount, pSrcData );
	m_bNeedsUnlock = true;
	return m_pRenderData;
}

template< typename E >
inline void CMatRenderData<E>::Release()
{
	if ( m_pRenderContext && m_pRenderData )
	{
		if ( m_bNeedsUnlock )
		{
			m_pRenderContext->UnlockRenderData( m_pRenderData );
		}
		else
		{
			m_pRenderContext->ReleaseRenderData();
		}
	}
	m_pRenderData = nullptr;
	m_nCount = 0;
	m_bNeedsUnlock = false;
}

template< typename E >
inline E* CMatRenderData<E>::Base()
{
	return m_pRenderData;
}

template< typename E >
inline const E* CMatRenderData<E>::Base() const
{
	return m_pRenderData;
}

template< typename E >
inline E& CMatRenderData<E>::operator[]( int i )
{
	contract_assert( ( i >= 0 ) && ( i < m_nCount ) );
	return m_pRenderData[i];
}

template< typename E >
inline const E& CMatRenderData<E>::operator[]( int i ) const
{
	contract_assert( ( i >= 0 ) && ( i < m_nCount ) );
	return m_pRenderData[i];
}


//-----------------------------------------------------------------------------

class CMatRenderContextPtr
{
public:
	CMatRenderContextPtr()																					{}
	CMatRenderContextPtr( IMatRenderContext *pInit )			: m_pContext( pInit, ks::adopt_ref )		{ if ( m_pContext ) m_pContext->BeginRender(); }
	CMatRenderContextPtr( IMaterialSystem *pFrom )				: m_pContext( pFrom->GetRenderContext(), ks::adopt_ref )	{ if ( m_pContext ) m_pContext->BeginRender(); }
	~CMatRenderContextPtr()																					{ if ( m_pContext ) m_pContext->EndRender(); }

	IMatRenderContext *operator=( IMatRenderContext *p )		{ if ( p ) p->BeginRender(); if ( m_pContext ) m_pContext->EndRender(); m_pContext.reset( p, ks::adopt_ref ); return p; }

	operator IMatRenderContext *() const						{ return m_pContext.get(); }
	IMatRenderContext *operator->() const						{ return m_pContext.get(); }
	IMatRenderContext &operator*() const						{ return *m_pContext; }

	void SafeRelease()											{ if ( m_pContext ) m_pContext->EndRender(); m_pContext.reset(); }
	void AssignAddRef( IMatRenderContext *pFrom )				{ if ( m_pContext ) m_pContext->EndRender(); m_pContext = pFrom; m_pContext->BeginRender(); }

	void GetFrom( IMaterialSystem *pFrom )						{ AssignAddRef( pFrom->GetRenderContext() ); }


private:
	CMatRenderContextPtr( const CMatRenderContextPtr &from );
	void operator=( const CMatRenderContextPtr &from );

	ks::intrusive_ptr<IMatRenderContext> m_pContext;
};

//-----------------------------------------------------------------------------
// Helper class for begin/end of pix event via constructor/destructor 
//-----------------------------------------------------------------------------
#define PIX_VALVE_ORANGE	0xFFF5940F

class PIXEvent
{
public:
	PIXEvent( IMatRenderContext *pRenderContext, const char *szName, unsigned long color = PIX_VALVE_ORANGE )
	{
		m_pRenderContext = pRenderContext;
		contract_assert( m_pRenderContext );
		contract_assert( szName );
		m_pRenderContext->BeginPIXEvent( color, szName );
	}
	~PIXEvent()
	{
		m_pRenderContext->EndPIXEvent();
	}
private:
	IMatRenderContext *m_pRenderContext;
};


// [mhansen] Enable PIX only in debug and profile builds for Xbox 360
#define PIX_ENABLE 0


#if PIX_ENABLE
#	define PIXEVENT PIXEvent _pixEvent
#else
#	define PIXEVENT 
#endif

//-----------------------------------------------------------------------------



//-----------------------------------------------------------------------------

extern IMaterialSystem *materials;;
extern IMaterialSystem *g_pMaterialSystem;;
