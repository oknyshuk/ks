import ks.core.threadtools;
import ks.core.types;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IShaderDevice facade class to the shader device implementation.
// Included by the TU that defines the implementation, which must declare ShaderDeviceImpl() returning it.

// IShaderDevice
void IShaderDevice::ReleaseResources( bool bReleaseManagedResources )
{ ShaderDeviceImpl().ReleaseResources( bReleaseManagedResources ); }
void IShaderDevice::ReacquireResources()
{ ShaderDeviceImpl().ReacquireResources(); }
ImageFormat IShaderDevice::GetBackBufferFormat() const
{ return ShaderDeviceImpl().GetBackBufferFormat(); }
void IShaderDevice::GetBackBufferDimensions( int& width, int& height ) const
{ ShaderDeviceImpl().GetBackBufferDimensions( width, height ); }
const AspectRatioInfo_t & IShaderDevice::GetAspectRatioInfo( void ) const
{ return ShaderDeviceImpl().GetAspectRatioInfo(); }
int IShaderDevice::GetCurrentAdapter() const
{ return ShaderDeviceImpl().GetCurrentAdapter(); }
bool IShaderDevice::IsUsingGraphics() const
{ return ShaderDeviceImpl().IsUsingGraphics(); }
void IShaderDevice::SpewDriverInfo() const
{ ShaderDeviceImpl().SpewDriverInfo(); }
int IShaderDevice::StencilBufferBits() const
{ return ShaderDeviceImpl().StencilBufferBits(); }
bool IShaderDevice::IsAAEnabled() const
{ return ShaderDeviceImpl().IsAAEnabled(); }
void IShaderDevice::Present()
{ ShaderDeviceImpl().Present(); }
void IShaderDevice::GetWindowSize( int &nWidth, int &nHeight ) const
{ ShaderDeviceImpl().GetWindowSize( nWidth, nHeight ); }
void IShaderDevice::SetHardwareGammaRamp( float fGamma, float fGammaTVRangeMin, float fGammaTVRangeMax, float fGammaTVExponent, bool bTVEnabled )
{ ShaderDeviceImpl().SetHardwareGammaRamp( fGamma, fGammaTVRangeMin, fGammaTVRangeMax, fGammaTVExponent, bTVEnabled ); }
bool IShaderDevice::AddView( void* hWnd )
{ return ShaderDeviceImpl().AddView( hWnd ); }
void IShaderDevice::RemoveView( void* hWnd )
{ ShaderDeviceImpl().RemoveView( hWnd ); }
void IShaderDevice::SetView( void* hWnd )
{ ShaderDeviceImpl().SetView( hWnd ); }
IShaderBuffer* IShaderDevice::CompileShader( const char *pProgram, std::size_t nBufLen, const char *pShaderVersion )
{ return ShaderDeviceImpl().CompileShader( pProgram, nBufLen, pShaderVersion ); }
VertexShaderHandle_t IShaderDevice::CreateVertexShader( IShaderBuffer* pShaderBuffer )
{ return ShaderDeviceImpl().CreateVertexShader( pShaderBuffer ); }
void IShaderDevice::DestroyVertexShader( VertexShaderHandle_t hShader )
{ ShaderDeviceImpl().DestroyVertexShader( hShader ); }
GeometryShaderHandle_t IShaderDevice::CreateGeometryShader( IShaderBuffer* pShaderBuffer )
{ return ShaderDeviceImpl().CreateGeometryShader( pShaderBuffer ); }
void IShaderDevice::DestroyGeometryShader( GeometryShaderHandle_t hShader )
{ ShaderDeviceImpl().DestroyGeometryShader( hShader ); }
PixelShaderHandle_t IShaderDevice::CreatePixelShader( IShaderBuffer* pShaderBuffer )
{ return ShaderDeviceImpl().CreatePixelShader( pShaderBuffer ); }
void IShaderDevice::DestroyPixelShader( PixelShaderHandle_t hShader )
{ ShaderDeviceImpl().DestroyPixelShader( hShader ); }
IMesh* IShaderDevice::CreateStaticMesh( VertexFormat_t vertexFormat, const char *pTextureBudgetGroup, IMaterial * pMaterial, VertexStreamSpec_t *pStreamSpec )
{ return ShaderDeviceImpl().CreateStaticMesh( vertexFormat, pTextureBudgetGroup, pMaterial, pStreamSpec ); }
void IShaderDevice::DestroyStaticMesh( IMesh* mesh )
{ ShaderDeviceImpl().DestroyStaticMesh( mesh ); }
IVertexBuffer * IShaderDevice::CreateVertexBuffer( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroup )
{ return ShaderDeviceImpl().CreateVertexBuffer( type, fmt, nVertexCount, pBudgetGroup ); }
void IShaderDevice::DestroyVertexBuffer( IVertexBuffer *pVertexBuffer )
{ ShaderDeviceImpl().DestroyVertexBuffer( pVertexBuffer ); }
IIndexBuffer * IShaderDevice::CreateIndexBuffer( ShaderBufferType_t bufferType, MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroup )
{ return ShaderDeviceImpl().CreateIndexBuffer( bufferType, fmt, nIndexCount, pBudgetGroup ); }
void IShaderDevice::DestroyIndexBuffer( IIndexBuffer *pIndexBuffer )
{ ShaderDeviceImpl().DestroyIndexBuffer( pIndexBuffer ); }
IVertexBuffer * IShaderDevice::GetDynamicVertexBuffer( int nStreamID, VertexFormat_t vertexFormat, bool bBuffered )
{ return ShaderDeviceImpl().GetDynamicVertexBuffer( nStreamID, vertexFormat, bBuffered ); }
IIndexBuffer * IShaderDevice::GetDynamicIndexBuffer()
{ return ShaderDeviceImpl().GetDynamicIndexBuffer(); }
void IShaderDevice::EnableNonInteractiveMode( MaterialNonInteractiveMode_t mode, ShaderNonInteractiveInfo_t *pInfo )
{ ShaderDeviceImpl().EnableNonInteractiveMode( mode, pInfo ); }
void IShaderDevice::RefreshFrontBufferNonInteractive()
{ ShaderDeviceImpl().RefreshFrontBufferNonInteractive(); }
void IShaderDevice::HandleThreadEvent( uint32 threadEvent )
{ ShaderDeviceImpl().HandleThreadEvent( threadEvent ); }
void IShaderDevice::OnDebugEvent( const char * pEvent )
{ ShaderDeviceImpl().OnDebugEvent( pEvent ); }

