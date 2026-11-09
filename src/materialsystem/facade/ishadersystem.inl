// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IShaderSystem class to the shader system implementation.
// Included by shadersystem.cpp, which defines ShaderSystemImpl().

ShaderAPITextureHandle_t IShaderSystem::GetShaderAPITextureBindHandle( ITexture *pTexture, int nFrameVar, int nTextureChannel )
{ return ShaderSystemImpl().GetShaderAPITextureBindHandle( pTexture, nFrameVar, nTextureChannel ); }
void IShaderSystem::BindTexture( Sampler_t sampler1, TextureBindFlags_t nBindFlags, ITexture *pTexture, int nFrameVar )
{ ShaderSystemImpl().BindTexture( sampler1, nBindFlags, pTexture, nFrameVar ); }
void IShaderSystem::BindTexture( Sampler_t sampler1, Sampler_t sampler2, TextureBindFlags_t nBindFlags, ITexture *pTexture, int nFrameVar )
{ ShaderSystemImpl().BindTexture( sampler1, sampler2, nBindFlags, pTexture, nFrameVar ); }
void IShaderSystem::TakeSnapshot()
{ ShaderSystemImpl().TakeSnapshot(); }
void IShaderSystem::DrawSnapshot( const unsigned char *pInstanceCommandBuffer, bool bMakeActualDrawCall )
{ ShaderSystemImpl().DrawSnapshot( pInstanceCommandBuffer, bMakeActualDrawCall ); }
bool IShaderSystem::IsUsingGraphics() const
{ return ShaderSystemImpl().IsUsingGraphics(); }
bool IShaderSystem::CanUseEditorMaterials() const
{ return ShaderSystemImpl().CanUseEditorMaterials(); }
void IShaderSystem::BindVertexTexture( VertexTextureSampler_t vtSampler, ITexture *pTexture, int nFrameVar )
{ ShaderSystemImpl().BindVertexTexture( vtSampler, pTexture, nFrameVar ); }
void IShaderSystem::AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics )
{ ShaderSystemImpl().AddShaderComboInformation( pSemantics ); }
