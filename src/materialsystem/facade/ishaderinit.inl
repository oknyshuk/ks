// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IShaderInit class to the shader system implementation.
// Included by shadersystem.cpp, which defines ShaderSystemImpl().

void IShaderInit::LoadTexture( IMaterialVar *pTextureVar, const char *pTextureGroupName, int nAdditionalCreationFlags )
{ ShaderSystemImpl().LoadTexture( pTextureVar, pTextureGroupName, nAdditionalCreationFlags ); }
void IShaderInit::LoadBumpMap( IMaterialVar *pTextureVar, const char *pTextureGroupName, int nAdditionalCreationFlags )
{ ShaderSystemImpl().LoadBumpMap( pTextureVar, pTextureGroupName, nAdditionalCreationFlags ); }
void IShaderInit::LoadCubeMap( IMaterialVar **ppParams, IMaterialVar *pTextureVar, int nAdditionalCreationFlags )
{ ShaderSystemImpl().LoadCubeMap( ppParams, pTextureVar, nAdditionalCreationFlags ); }
