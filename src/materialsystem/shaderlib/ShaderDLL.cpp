// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.math;
#include "shaderlib/ShaderDLL.h"
#include "materialsystem/IShader.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/materialsystem_config.h"
#include "materialsystem/ishadersystem.h"
#include "materialsystem/ishaderapi.h"
#include "shaderlib_cvar.h"
import std;
#include "tier2/tier2.h"

//-----------------------------------------------------------------------------
// The standard implementation of CShaderDLL
//-----------------------------------------------------------------------------
class CShaderDLL
{
public:
	CShaderDLL();

	// methods of IShaderDLL
	bool Connect( CreateInterfaceFn factory );
	void Disconnect();

	// methods of IShaderDLLInternal
	int ShaderCount() const;
	IShader *GetShader( int nShader );
	int ShaderComboSemanticsCount() const;
	const ShaderComboSemantics_t *GetComboSemantics( int n );
	bool Connect( CreateInterfaceFn factory, bool bIsMaterialSystem );
	void Disconnect( bool bIsMaterialSystem );
	void InsertShader( IShader *pShader );

	void AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics );

private:
	std::vector< IShader * >	m_ShaderList;
	std::vector< const ShaderComboSemantics_t * > m_ShaderComboSemantics;
};


//-----------------------------------------------------------------------------
// Global interfaces/structures
//-----------------------------------------------------------------------------
IMaterialSystemHardwareConfig* g_pHardwareConfig;
const MaterialSystem_Config_t *g_pConfig;


//-----------------------------------------------------------------------------
// Interfaces/structures local to shaderlib
//-----------------------------------------------------------------------------
IShaderSystem* g_pSLShaderSystem;


// Pattern necessary because shaders register themselves in global constructors
static CShaderDLL *s_pShaderDLL;


//-----------------------------------------------------------------------------
// Global accessor
//-----------------------------------------------------------------------------
// IShaderDLL and IShaderDLLInternal are stateless facades; their members, defined below, forward to the
// one CShaderDLL.
static IShaderDLL s_ShaderDLLFacade;
static IShaderDLLInternal s_ShaderDLLInternalFacade;

static CShaderDLL &ShaderDLLImpl()
{
	// Pattern necessary because shaders register themselves in global constructors
	if ( !s_pShaderDLL )
	{
		s_pShaderDLL = new CShaderDLL;
	}

	return *s_pShaderDLL;
}

IShaderDLL *GetShaderDLL()
{
	return &s_ShaderDLLFacade;
}

IShaderDLLInternal *GetShaderDLLInternal()
{
	return &s_ShaderDLLInternalFacade;
}

#include "facade/ishaderdll.inl"
#include "facade/ishaderdllinternal.inl"

//-----------------------------------------------------------------------------
// Singleton interface
//-----------------------------------------------------------------------------
static InterfaceReg s_Reg__InstantiateInterfaceFn_GetShaderDLLInternal( (InstantiateInterfaceFn)GetShaderDLLInternal, SHADER_DLL_INTERFACE_VERSION );;

//-----------------------------------------------------------------------------
// Connect, disconnect...
//-----------------------------------------------------------------------------
CShaderDLL::CShaderDLL()
{
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );
}


//-----------------------------------------------------------------------------
// Connect, disconnect...
//-----------------------------------------------------------------------------
bool CShaderDLL::Connect( CreateInterfaceFn factory, bool bIsMaterialSystem )
{
	g_pHardwareConfig =  (IMaterialSystemHardwareConfig*)factory( MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION, nullptr );
	g_pConfig = (const MaterialSystem_Config_t*)factory( MATERIALSYSTEM_CONFIG_VERSION, nullptr );
	g_pSLShaderSystem =  (IShaderSystem*)factory( SHADERSYSTEM_INTERFACE_VERSION, nullptr );

	if ( !bIsMaterialSystem )
	{
		ConnectTier1Libraries( &factory, 1 );
  		InitShaderLibCVars( factory );
	}

	return ( g_pConfig != nullptr ) && (g_pHardwareConfig != nullptr) && ( g_pSLShaderSystem != nullptr );
}

void CShaderDLL::Disconnect( bool bIsMaterialSystem )
{
	if ( !bIsMaterialSystem )
	{
		ConVar_Unregister();
		DisconnectTier1Libraries();
	}

	g_pHardwareConfig = nullptr;
	g_pConfig = nullptr;
	g_pSLShaderSystem = nullptr;
}

bool CShaderDLL::Connect( CreateInterfaceFn factory )
{
	return Connect( factory, false );
}

void CShaderDLL::Disconnect()
{
	Disconnect( false );
}


//-----------------------------------------------------------------------------
// Iterates over all shaders
//-----------------------------------------------------------------------------
int CShaderDLL::ShaderCount() const
{
	return std::ssize( m_ShaderList );
}

IShader *CShaderDLL::GetShader( int nShader ) 
{
	if ( ( nShader < 0 ) || ( nShader >= std::ssize( m_ShaderList ) ) )
		return nullptr;

	return m_ShaderList[nShader];
}


//-----------------------------------------------------------------------------
// Adds to the shader lists
//-----------------------------------------------------------------------------
void CShaderDLL::InsertShader( IShader *pShader )
{
	contract_assert( pShader );
	m_ShaderList.push_back( pShader );
}

//-----------------------------------------------------------------------------
// Iterates over all shader semantics
//-----------------------------------------------------------------------------
int CShaderDLL::ShaderComboSemanticsCount() const
{
	return std::ssize( m_ShaderComboSemantics );
}

const ShaderComboSemantics_t *CShaderDLL::GetComboSemantics( int n ) 
{
	if ( ( n < 0 ) || ( n >= std::ssize( m_ShaderComboSemantics ) ) )
		return nullptr;

	return m_ShaderComboSemantics[n];
}

//-----------------------------------------------------------------------------
// Adds to the shader combo semantics list
//-----------------------------------------------------------------------------
void CShaderDLL::AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics )
{
	m_ShaderComboSemantics.push_back( pSemantics );
}
