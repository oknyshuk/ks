// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the CShaderAPIBase and IShaderAPIDX8 facade classes (the shader API as seen inside
// shaderapidx9) to the one implementation, CShaderAPIDx8.
// Included by shaderapidx8.cpp, which defines ShaderAPIImpl().

// CShaderAPIBase
bool CShaderAPIBase::OnDeviceInit()
{ return ShaderAPIImpl().OnDeviceInit(); }
void CShaderAPIBase::OnDeviceShutdown()
{ ShaderAPIImpl().OnDeviceShutdown(); }
void CShaderAPIBase::BeginPIXEvent( unsigned long color, const char *szName )
{ ShaderAPIImpl().BeginPIXEvent( color, szName ); }
void CShaderAPIBase::EndPIXEvent()
{ ShaderAPIImpl().EndPIXEvent(); }
void CShaderAPIBase::AdvancePIXFrame()
{ ShaderAPIImpl().AdvancePIXFrame(); }
void CShaderAPIBase::ReleaseShaderObjects( bool bReleaseManagedResources )
{ ShaderAPIImpl().ReleaseShaderObjects( bReleaseManagedResources ); }
void CShaderAPIBase::RestoreShaderObjects()
{ ShaderAPIImpl().RestoreShaderObjects(); }
void CShaderAPIBase::ResetRenderState( bool bFullReset )
{ ShaderAPIImpl().ResetRenderState( bFullReset ); }
IDirect3DBaseTexture* CShaderAPIBase::GetD3DTexture( ShaderAPITextureHandle_t hTexture )
{ return ShaderAPIImpl().GetD3DTexture( hTexture ); }
void CShaderAPIBase::QueueResetRenderState()
{ ShaderAPIImpl().QueueResetRenderState(); }

// IShaderAPIDX8
void IShaderAPIDX8::DrawMesh( CMeshBase *pMesh, int nCount, const MeshInstanceData_t *pInstances, VertexCompressionType_t nCompressionType, CompiledLightingState_t* pCompiledState, InstanceInfo_t *pInfo )
{ ShaderAPIImpl().DrawMesh( pMesh, nCount, pInstances, nCompressionType, pCompiledState, pInfo ); }
void IShaderAPIDX8::DrawWithVertexAndIndexBuffers( void )
{ ShaderAPIImpl().DrawWithVertexAndIndexBuffers(); }
void IShaderAPIDX8::GetBufferedState( BufferedState_t &state )
{ ShaderAPIImpl().GetBufferedState( state ); }
D3DCULL IShaderAPIDX8::GetCullMode() const
{ return ShaderAPIImpl().GetCullMode(); }
void IShaderAPIDX8::ComputeFillRate()
{ ShaderAPIImpl().ComputeFillRate(); }
bool IShaderAPIDX8::IsInSelectionMode() const
{ return ShaderAPIImpl().IsInSelectionMode(); }
void IShaderAPIDX8::RegisterSelectionHit( float minz, float maxz )
{ ShaderAPIImpl().RegisterSelectionHit( minz, maxz ); }
IMaterialInternal* IShaderAPIDX8::GetBoundMaterial()
{ return ShaderAPIImpl().GetBoundMaterial(); }
void IShaderAPIDX8::ApplyZBias( const DepthTestState_t &shaderState )
{ ShaderAPIImpl().ApplyZBias( shaderState ); }
void IShaderAPIDX8::ApplyCullEnable( bool bEnable )
{ ShaderAPIImpl().ApplyCullEnable( bEnable ); }
void IShaderAPIDX8::ApplyFogMode( ShaderFogMode_t fogMode, bool bVertexFog, bool bSRGBWritesEnabled, bool bDisableGammaCorrection )
{ ShaderAPIImpl().ApplyFogMode( fogMode, bVertexFog, bSRGBWritesEnabled, bDisableGammaCorrection ); }
int IShaderAPIDX8::GetActualSamplerCount() const
{ return ShaderAPIImpl().GetActualSamplerCount(); }
bool IShaderAPIDX8::IsRenderingMesh() const
{ return ShaderAPIImpl().IsRenderingMesh(); }
void IShaderAPIDX8::EnableFixedFunctionFog( bool bFogEnable )
{ ShaderAPIImpl().EnableFixedFunctionFog( bFogEnable ); }
int IShaderAPIDX8::GetCurrentFrameCounter( void ) const
{ return ShaderAPIImpl().GetCurrentFrameCounter(); }
void IShaderAPIDX8::SetupSelectionModeVisualizationState()
{ ShaderAPIImpl().SetupSelectionModeVisualizationState(); }
bool IShaderAPIDX8::UsingSoftwareVertexProcessing() const
{ return ShaderAPIImpl().UsingSoftwareVertexProcessing(); }
void IShaderAPIDX8::EnabledSRGBWrite( bool bEnabled )
{ ShaderAPIImpl().EnabledSRGBWrite( bEnabled ); }
void IShaderAPIDX8::SetSRGBWrite( bool bState )
{ ShaderAPIImpl().SetSRGBWrite( bState ); }
void IShaderAPIDX8::ApplyAlphaToCoverage( bool bEnable )
{ ShaderAPIImpl().ApplyAlphaToCoverage( bEnable ); }
void IShaderAPIDX8::PrintfVA( char *fmt, std::va_list vargs )
{ ShaderAPIImpl().PrintfVA( fmt, vargs ); }
float IShaderAPIDX8::Knob( char *knobname, float *setvalue )
{ return ShaderAPIImpl().Knob( knobname, setvalue ); }
void IShaderAPIDX8::NotifyShaderConstantsChangedInRenderPass()
{ ShaderAPIImpl().NotifyShaderConstantsChangedInRenderPass(); }
void IShaderAPIDX8::GenerateNonInstanceRenderState( MeshInstanceData_t *pInstance, CompiledLightingState_t** pCompiledState, InstanceInfo_t **pInfo )
{ ShaderAPIImpl().GenerateNonInstanceRenderState( pInstance, pCompiledState, pInfo ); }
void IShaderAPIDX8::ExecuteInstanceCommandBuffer( const unsigned char *pCmdBuf, int nInstanceIndex, bool bForceStateSet )
{ ShaderAPIImpl().ExecuteInstanceCommandBuffer( pCmdBuf, nInstanceIndex, bForceStateSet ); }
void IShaderAPIDX8::SetVertexDecl( VertexFormat_t vertexFormat, bool bHasColorMesh, bool bUsingFlex, bool bUsingMorph, bool bUsingPreTessPatch, VertexStreamSpec_t *pStreamSpec )
{ ShaderAPIImpl().SetVertexDecl( vertexFormat, bHasColorMesh, bUsingFlex, bUsingMorph, bUsingPreTessPatch, pStreamSpec ); }
void IShaderAPIDX8::AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics )
{ ShaderAPIImpl().AddShaderComboInformation( pSemantics ); }
float IShaderAPIDX8::GetLightMapScaleFactor() const
{ return ShaderAPIImpl().GetLightMapScaleFactor(); }

