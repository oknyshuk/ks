#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

#include "shaderapibase.h"
#include "shaderapi/ishadershadow.h"
#include "materialsystem/IShader.h"
#include "locald3dtypes.h"
import ks.math.vector4d;

//-----------------------------------------------------------------------------
// Vendor-specific defines
//-----------------------------------------------------------------------------
#define ATI_FETCH4_ENABLE		MAKEFOURCC('G','E','T','4')
#define ATI_FETCH4_DISABLE		MAKEFOURCC('G','E','T','1')
#define ATISAMP_FETCH4			D3DSAMP_MIPMAPLODBIAS

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CMeshBase;
class CMeshBuilder;
struct ShadowState_t;
struct DepthTestState_t;
class IMaterialInternal;
struct MeshInstanceData_t;

//#define _X360_GPU_OWN_RESOURCES
[[gnu::always_inline]] inline constexpr auto IsGPUOwnSupported() { return ( false ); }


//-----------------------------------------------------------------------------
// State that matters to buffered meshes... (for debugging only)
//-----------------------------------------------------------------------------
struct BufferedState_t
{
	D3DXMATRIX m_Transform[3];
	D3DVIEWPORT9 m_Viewport;
	int m_BoundTexture[16];
	void *m_VertexShader;
	void *m_PixelShader;
};


//-----------------------------------------------------------------------------
// Compiled lighting state
//-----------------------------------------------------------------------------
struct CompiledLightingState_t
{
	Vector4D	m_AmbientLightCube[6];
	int			m_nLocalLightCount;
	Vector4D	m_PixelShaderLocalLights[6];
	Vector4D	m_VertexShaderLocalLights[20];
	int			m_VertexShaderLocalLightLoopControl[4];
	int			m_VertexShaderLocalLightEnable[VERTEX_SHADER_LIGHT_ENABLE_BOOL_CONST_COUNT];
};

struct InstanceInfo_t
{
	// Have we compiled various bits of lighting state?
	bool		m_bAmbientCubeCompiled : 1;
	bool		m_bPixelShaderLocalLightsCompiled : 1;
	bool		m_bVertexShaderLocalLightsCompiled : 1;

	// Have we set various shader constants?
	bool		m_bSetSkinConstants : 1;
	bool		m_bSetLightVertexShaderConstants : 1;
};


//-----------------------------------------------------------------------------
// The DX8 shader API
//-----------------------------------------------------------------------------
// FIXME: Remove this! Either move them into CShaderAPIBase or CShaderAPIDx8
class IShaderAPIDX8 : public CShaderAPIBase
{
public:
	// Draws the mesh
	void DrawMesh( CMeshBase *pMesh, int nCount, const MeshInstanceData_t *pInstances, VertexCompressionType_t nCompressionType, CompiledLightingState_t* pCompiledState, InstanceInfo_t *pInfo );

	// Draw the mesh with the currently bound vertex and index buffers.
	void DrawWithVertexAndIndexBuffers( void );

	// Gets the current buffered state... (debug only)
	void GetBufferedState( BufferedState_t &state );

	// Gets the current backface cull state....
	D3DCULL GetCullMode() const;

	// Measures fill rate
	void ComputeFillRate();

	// Selection mode methods
	bool IsInSelectionMode() const;

	// We hit somefin in selection mode
	void RegisterSelectionHit( float minz, float maxz );

	// Get the currently bound material
	IMaterialInternal* GetBoundMaterial();

	// These methods are called by the transition table
	// They depend on dynamic state so can't exist inside the transition table
	void ApplyZBias( const DepthTestState_t  &shaderState );
	void ApplyCullEnable( bool bEnable );
	void ApplyFogMode( ShaderFogMode_t fogMode, bool bVertexFog, bool bSRGBWritesEnabled, bool bDisableGammaCorrection );

	int GetActualSamplerCount() const;

	bool IsRenderingMesh() const;

	// Fog methods...
	void EnableFixedFunctionFog( bool bFogEnable );

	int GetCurrentFrameCounter( void ) const;

	// Workaround hack for visualization of selection mode
	void SetupSelectionModeVisualizationState();

	bool UsingSoftwareVertexProcessing() const;

	//notification that the SRGB write state is being changed
	void EnabledSRGBWrite( bool bEnabled );
	void SetSRGBWrite( bool bState );

	// Alpha to coverage
	void ApplyAlphaToCoverage( bool bEnable );

	void PrintfVA( char *fmt, std::va_list vargs );
	float Knob( char *knobname, float *setvalue = nullptr );

	void NotifyShaderConstantsChangedInRenderPass();

	void GenerateNonInstanceRenderState( MeshInstanceData_t *pInstance, CompiledLightingState_t** pCompiledState, InstanceInfo_t **pInfo );

	// Executes the per-instance command buffer
	void ExecuteInstanceCommandBuffer( const unsigned char *pCmdBuf, int nInstanceIndex, bool bForceStateSet );

	// Sets the vertex decl
	void SetVertexDecl( VertexFormat_t vertexFormat, bool bHasColorMesh, bool bUsingFlex, bool bUsingMorph, bool bUsingPreTessPatch, VertexStreamSpec_t *pStreamSpec );

	// Set Tessellation Enable
	void SetTessellationMode( TessellationMode_t mode ) {}

	void AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics );

	float GetLightMapScaleFactor() const;
};
