// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Pieces of the application framework, shared between POSIX systems (Mac OS X, Linux, etc)

import ks.vstdlib;
#include "appframework/AppFramework.h"
import std;
#include "filesystem.h"
#include "appframework/IAppSystemGroup.h"
#include "filesystem_init.h"
#include "togl/rendermechanism.h"

//-----------------------------------------------------------------------------
// Globals...
//-----------------------------------------------------------------------------
HINSTANCE s_HInstance;

static CSimpleLoggingListener s_SimpleLoggingListener;
ILoggingListener *g_pDefaultLoggingListener = &s_SimpleLoggingListener;

//-----------------------------------------------------------------------------
// HACK: Since I don't want to refit vgui yet...
//-----------------------------------------------------------------------------
void *GetAppInstance()
{
	return s_HInstance;
}


//-----------------------------------------------------------------------------
// Sets the application instance
//-----------------------------------------------------------------------------
void SetAppInstance( void* hInstance )
{
	s_HInstance = (HINSTANCE)hInstance;
}


//-----------------------------------------------------------------------------
//
// Default implementation of an application meant to be run using the file system
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CFSApplicationBase::CFSApplicationBase()
{
	m_pFileSystem = nullptr;
}


//-----------------------------------------------------------------------------
// Create necessary interfaces
//-----------------------------------------------------------------------------
bool CFSApplicationBase::Create( )
{
	FileSystem_SetErrorMode( FS_ERRORMODE_NONE );

	// Add in the cvar factory
	AppModule_t cvarModule = LoadModule( VStdLib_GetICVarFactory() );
	AddSystem( cvarModule, CVAR_INTERFACE_VERSION );	

	AppModule_t fileSystemModule = LoadModule( Sys_GetFactoryThis() );
	m_pFileSystem = (IFileSystem*)AddSystem( fileSystemModule, FILESYSTEM_INTERFACE_VERSION );
	if ( !m_pFileSystem )
	{
		Error( "Unable to create the file system" );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// The file system pointer is invalid at this point
//-----------------------------------------------------------------------------
void CFSApplicationBase::Destroy()
{
	m_pFileSystem = nullptr;
}


//-----------------------------------------------------------------------------
// Pre-init, shutdown
//-----------------------------------------------------------------------------
bool CFSApplicationBase::PreInit( )
{
	return true;
}

void CFSApplicationBase::PostShutdown( )
{
}
