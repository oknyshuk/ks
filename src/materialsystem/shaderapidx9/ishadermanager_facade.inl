// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IShaderManager facade class to the one shader manager.
// Included by vertexshaderdx8.cpp, which defines ShaderManagerImpl().

// IShaderManager
void IShaderManager::Init()
{ ShaderManagerImpl().Init(); }
void IShaderManager::Shutdown()
{ ShaderManagerImpl().Shutdown(); }
IShaderBuffer * IShaderManager::CompileShader( const char *pProgram, std::size_t nBufLen, const char *pShaderVersion )
{ return ShaderManagerImpl().CompileShader( pProgram, nBufLen, pShaderVersion ); }
VertexShaderHandle_t IShaderManager::CreateVertexShader( IShaderBuffer* pShaderBuffer )
{ return ShaderManagerImpl().CreateVertexShader( pShaderBuffer ); }
void IShaderManager::DestroyVertexShader( VertexShaderHandle_t hShader )
{ ShaderManagerImpl().DestroyVertexShader( hShader ); }
PixelShaderHandle_t IShaderManager::CreatePixelShader( IShaderBuffer* pShaderBuffer )
{ return ShaderManagerImpl().CreatePixelShader( pShaderBuffer ); }
void IShaderManager::DestroyPixelShader( PixelShaderHandle_t hShader )
{ ShaderManagerImpl().DestroyPixelShader( hShader ); }
VertexShader_t IShaderManager::CreateVertexShader( const char *pVertexShaderFile, int nStaticVshIndex, char *debugLabel )
{ return ShaderManagerImpl().CreateVertexShader( pVertexShaderFile, nStaticVshIndex, debugLabel ); }
PixelShader_t IShaderManager::CreatePixelShader( const char *pPixelShaderFile, int nStaticPshIndex, char *debugLabel )
{ return ShaderManagerImpl().CreatePixelShader( pPixelShaderFile, nStaticPshIndex, debugLabel ); }
void IShaderManager::SetVertexShader( VertexShader_t shader )
{ ShaderManagerImpl().SetVertexShader( shader ); }
void IShaderManager::SetPixelShader( PixelShader_t shader )
{ ShaderManagerImpl().SetPixelShader( shader ); }
void IShaderManager::SetPixelShaderState_Internal( HardwareShader_t shader, DataCacheHandle_t hCachedShader )
{ ShaderManagerImpl().SetPixelShaderState_Internal( shader, hCachedShader ); }
void IShaderManager::SetVertexShaderState_Internal( HardwareShader_t shader, DataCacheHandle_t hCachedShader )
{ ShaderManagerImpl().SetVertexShaderState_Internal( shader, hCachedShader ); }
void IShaderManager::ResetShaderState()
{ ShaderManagerImpl().ResetShaderState(); }
void IShaderManager::FlushShaders()
{ ShaderManagerImpl().FlushShaders(); }
void * IShaderManager::GetCurrentVertexShader()
{ return ShaderManagerImpl().GetCurrentVertexShader(); }
void * IShaderManager::GetCurrentPixelShader()
{ return ShaderManagerImpl().GetCurrentPixelShader(); }
void IShaderManager::ClearVertexAndPixelShaderRefCounts()
{ ShaderManagerImpl().ClearVertexAndPixelShaderRefCounts(); }
void IShaderManager::PurgeUnusedVertexAndPixelShaders()
{ ShaderManagerImpl().PurgeUnusedVertexAndPixelShaders(); }
void IShaderManager::BindVertexShader( VertexShaderHandle_t shader )
{ ShaderManagerImpl().BindVertexShader( shader ); }
void IShaderManager::BindPixelShader( PixelShaderHandle_t shader )
{ ShaderManagerImpl().BindPixelShader( shader ); }
void IShaderManager::AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics )
{ ShaderManagerImpl().AddShaderComboInformation( pSemantics ); }
HardwareShader_t IShaderManager::GetVertexShader( VertexShader_t vs, int dynIdx )
{ return ShaderManagerImpl().GetVertexShader( vs, dynIdx ); }
HardwareShader_t IShaderManager::GetPixelShader( PixelShader_t ps, int dynIdx )
{ return ShaderManagerImpl().GetPixelShader( ps, dynIdx ); }


void IShaderManager::SetVertexShaderIndex( int vshIndex )
{ ShaderManagerImpl().SetVertexShaderIndex( vshIndex ); }
void IShaderManager::SetPixelShaderIndex( int pshIndex )
{ ShaderManagerImpl().SetPixelShaderIndex( pshIndex ); }
