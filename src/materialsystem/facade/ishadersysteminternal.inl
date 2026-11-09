// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IShaderSystemInternal class to the shader system implementation.
// Included by shadersystem.cpp, which defines ShaderSystemImpl().

void IShaderSystemInternal::Init()
{ ShaderSystemImpl().Init(); }
void IShaderSystemInternal::Shutdown()
{ ShaderSystemImpl().Shutdown(); }
void IShaderSystemInternal::ModInit()
{ ShaderSystemImpl().ModInit(); }
void IShaderSystemInternal::ModShutdown()
{ ShaderSystemImpl().ModShutdown(); }
bool IShaderSystemInternal::LoadShaderDLL( const char *pFullPath )
{ return ShaderSystemImpl().LoadShaderDLL( pFullPath ); }
void IShaderSystemInternal::UnloadShaderDLL( const char *pFullPath )
{ ShaderSystemImpl().UnloadShaderDLL( pFullPath ); }
IShader* IShaderSystemInternal::FindShader( char const* pShaderName )
{ return ShaderSystemImpl().FindShader( pShaderName ); }
char const* IShaderSystemInternal::ShaderStateString( int i ) const
{ return ShaderSystemImpl().ShaderStateString( i ); }
int IShaderSystemInternal::ShaderStateCount() const
{ return ShaderSystemImpl().ShaderStateCount(); }
void IShaderSystemInternal::CreateDebugMaterials()
{ ShaderSystemImpl().CreateDebugMaterials(); }
void IShaderSystemInternal::CleanUpDebugMaterials()
{ ShaderSystemImpl().CleanUpDebugMaterials(); }
void IShaderSystemInternal::InitShaderParameters( IShader *pShader, IMaterialVar **params, const char *pMaterialName )
{ ShaderSystemImpl().InitShaderParameters( pShader, params, pMaterialName ); }
void IShaderSystemInternal::InitShaderInstance( IShader *pShader, IMaterialVar **params, const char *pMaterialName, const char *pTextureGroupName )
{ ShaderSystemImpl().InitShaderInstance( pShader, params, pMaterialName, pTextureGroupName ); }
bool IShaderSystemInternal::InitRenderState( IShader *pShader, int numParams, IMaterialVar **params, ShaderRenderState_t* pRenderState, char const* pMaterialName )
{ return ShaderSystemImpl().InitRenderState( pShader, numParams, params, pRenderState, pMaterialName ); }
void IShaderSystemInternal::CleanupRenderState( ShaderRenderState_t* pRenderState )
{ ShaderSystemImpl().CleanupRenderState( pRenderState ); }
void IShaderSystemInternal::DrawElements( IShader *pShader, IMaterialVar **params, ShaderRenderState_t* pShaderState, VertexCompressionType_t vertexCompression, uint32 nMaterialVarTimeStamp, uint32 nModulationFlags, bool bRenderingPreTessPatchMesh )
{ ShaderSystemImpl().DrawElements( pShader, params, pShaderState, vertexCompression, nMaterialVarTimeStamp, nModulationFlags, bRenderingPreTessPatchMesh ); }
int IShaderSystemInternal::ShaderCount() const
{ return ShaderSystemImpl().ShaderCount(); }
int IShaderSystemInternal::GetShaders( int nFirstShader, int nCount, IShader **ppShaderList ) const
{ return ShaderSystemImpl().GetShaders( nFirstShader, nCount, ppShaderList ); }
