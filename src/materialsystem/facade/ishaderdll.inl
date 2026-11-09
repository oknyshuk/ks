// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IShaderDLL class to the shader DLL singleton.
// Included by ShaderDLL.cpp, which defines ShaderDLLImpl().

void IShaderDLL::InsertShader( IShader *pShader )
{ ShaderDLLImpl().InsertShader( pShader ); }
void IShaderDLL::AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics )
{ ShaderDLLImpl().AddShaderComboInformation( pSemantics ); }
