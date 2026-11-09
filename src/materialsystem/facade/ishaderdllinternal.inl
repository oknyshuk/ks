// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IShaderDLLInternal class to the shader DLL singleton.
// Included by ShaderDLL.cpp, which defines ShaderDLLImpl().

bool IShaderDLLInternal::Connect( CreateInterfaceFn factory, bool bIsMaterialSystem )
{ return ShaderDLLImpl().Connect( factory, bIsMaterialSystem ); }
void IShaderDLLInternal::Disconnect( bool bIsMaterialSystem )
{ ShaderDLLImpl().Disconnect( bIsMaterialSystem ); }
int IShaderDLLInternal::ShaderCount() const
{ return ShaderDLLImpl().ShaderCount(); }
IShader * IShaderDLLInternal::GetShader( int nShader )
{ return ShaderDLLImpl().GetShader( nShader ); }
int IShaderDLLInternal::ShaderComboSemanticsCount() const
{ return ShaderDLLImpl().ShaderComboSemanticsCount(); }
const ShaderComboSemantics_t * IShaderDLLInternal::GetComboSemantics( int n )
{ return ShaderDLLImpl().GetComboSemantics( n ); }
