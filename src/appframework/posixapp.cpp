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
// Sets the application instance, should only be used if you're not calling AppMain.
//-----------------------------------------------------------------------------
void SetAppInstance( void* hInstance )
{
	s_HInstance = (HINSTANCE)hInstance;
}


//-----------------------------------------------------------------------------
// Version of AppMain used by windows applications
//-----------------------------------------------------------------------------

int AppMain( void* hInstance, void* hPrevInstance, const char* lpCmdLine, int nCmdShow, CAppSystemGroup *pAppSystemGroup )
{
	contract_assert( 0 );
	return -1;
}

static CNonFatalLoggingResponsePolicy s_NonFatalLoggingResponsePolicy;

//-----------------------------------------------------------------------------
// Version of AppMain used by console applications
//-----------------------------------------------------------------------------
int AppMain( int argc, char **argv, CAppSystemGroup *pAppSystemGroup )
{
	contract_assert( pAppSystemGroup );

	LoggingSystem_SetLoggingResponsePolicy( &s_NonFatalLoggingResponsePolicy );
	s_HInstance = nullptr;
	CommandLine()->CreateCmdLine( argc, argv );

	return pAppSystemGroup->Run( );
}



//-----------------------------------------------------------------------------
//
// Default implementation of an application meant to be run using the file system
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CFSApplication::CFSApplication( CFSAppSystemGroup *pAppSystemGroup )
{
	m_pChildAppSystemGroup = pAppSystemGroup;
	m_pFileSystem = nullptr;
}


//-----------------------------------------------------------------------------
// Create necessary interfaces
//-----------------------------------------------------------------------------
bool CFSApplication::Create( )
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

bool CFSApplication::GetFileSystemDLLName( char *pOut, int nMaxBytes )
{
	return FileSystem_GetFileSystemDLLName( pOut, nMaxBytes ) == FS_OK;
}

//-----------------------------------------------------------------------------
// The file system pointer is invalid at this point
//-----------------------------------------------------------------------------
void CFSApplication::Destroy()
{
	m_pFileSystem = nullptr;
}


//-----------------------------------------------------------------------------
// Pre-init, shutdown
//-----------------------------------------------------------------------------
bool CFSApplication::PreInit( )
{
	return true;
}

void CFSApplication::PostShutdown( )
{
}


//-----------------------------------------------------------------------------
// Run main loop
//-----------------------------------------------------------------------------
int CFSApplication::Main( )
{
	// Now that the file system is loaded, we can load up main libraries through it
	m_pChildAppSystemGroup->Setup( m_pFileSystem, this );
	return m_pChildAppSystemGroup->Run( );
}


int CFSApplication::Startup()
{
	int nRetVal = BaseClass::Startup();
	if ( GetCurrentStage() != NONE )
		return nRetVal;
	
	if ( FileSystem_SetBasePaths( m_pFileSystem ) != FS_OK )
		return 0;
	
	// Now that the file system is loaded, we can load up main libraries through it
	m_pChildAppSystemGroup->Setup( m_pFileSystem, this );
	return m_pChildAppSystemGroup->Startup();
}


void CFSApplication::Shutdown()
{
	m_pChildAppSystemGroup->Shutdown();
	BaseClass::Shutdown();
}
