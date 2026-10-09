// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// main precompiled header for client rendering code

import ks.tier1;
#include "materialsystem/imaterialsystemhardwareconfig.h"

import std;
#include "gl_shader.h"
#include "sysexternal.h"
#include "gl_shader.h"
#include "materialproxyfactory.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/idebugtextureinfo.h"


static CMaterialProxyFactory s_MaterialProxyFactory;


//-----------------------------------------------------------------------------
// Connects to other
//-----------------------------------------------------------------------------
static void Shader_ConnectOtherMatSysInterfaces( )
{
	// NOTE: These two interfaces have been connected in the tier2 library
	if ( !g_pMaterialSystemHardwareConfig )
	{
		Shader_Disconnect();
		Sys_Error( "Could not get the material system hardware config interface! (2)" );
	}
	if ( !g_pMaterialSystemDebugTextureInfo )
	{
		Shader_Disconnect();
		Sys_Error( "Could not get the debug texture info interface!" );
	}
}


//-----------------------------------------------------------------------------
// Connect to interfaces we need
//-----------------------------------------------------------------------------
bool Shader_Connect( bool bSetProxyFactory )
{
	if ( !materials )
		return false;

	int nAdapter = CommandLine()->ParmValue( "-adapter", 0 );
	int nModeFlags = MATERIAL_INIT_ALLOCATE_FULLSCREEN_TEXTURE;
	if ( CommandLine()->FindParm( "-ref" ) )
	{
		nModeFlags |= MATERIAL_INIT_REFERENCE_RASTERIZER;
	}

	materials->SetAdapter( nAdapter, nModeFlags );
	if ( bSetProxyFactory )
		materials->SetMaterialProxyFactory( &s_MaterialProxyFactory );

	Shader_ConnectOtherMatSysInterfaces( );
	return g_pMaterialSystemHardwareConfig && g_pMaterialSystemDebugTextureInfo;
}


//-----------------------------------------------------------------------------
// Connect to interfaces we need
//-----------------------------------------------------------------------------
void Shader_Disconnect( void )
{
}



#ifndef DEDICATED

void Shader_SwapBuffers( void )
{
	contract_assert( materials );
	materials->SwapBuffers();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Shader_BeginRendering ( void )
{
	// FIXME:
//	vid.width  = window_rect.width;
//	vid.height = window_rect.height;
}
#endif
