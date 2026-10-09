#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: An application framework

#include "appframework/IAppSystemGroup.h"
#include "ilaunchabledll.h"

//-----------------------------------------------------------------------------
// Gets the application instance..
//-----------------------------------------------------------------------------
void *GetAppInstance();


//-----------------------------------------------------------------------------
// Sets the application instance, should only be used if you're not calling AppMain.
//-----------------------------------------------------------------------------
void SetAppInstance( void* hInstance );


//-----------------------------------------------------------------------------
// Main entry point for the application
//-----------------------------------------------------------------------------
int AppMain( void* hInstance, void* hPrevInstance, const char* lpCmdLine, int nCmdShow, CAppSystemGroup *pAppSystemGroup );
int AppMain( int argc, char **argv, CAppSystemGroup *pAppSystemGroup );


//-----------------------------------------------------------------------------
// Used to startup/shutdown the application
//-----------------------------------------------------------------------------
int AppStartup( void* hInstance, void* hPrevInstance, const char* lpCmdLine, int nCmdShow, CAppSystemGroup *pAppSystemGroup );
int AppStartup( int argc, char **argv, CAppSystemGroup *pAppSystemGroup );
void AppShutdown( CAppSystemGroup *pAppSystemGroup );


//-----------------------------------------------------------------------------
// This class is a helper class used for file system-based applications.
// It loads up the file system in preparation for using it to load other
// required modules.
//-----------------------------------------------------------------------------
class CFSApplication : public CAppSystemGroup
{
	typedef CAppSystemGroup BaseClass;

public:
	CFSApplication( CFSAppSystemGroup *pAppSystemGroup );

	// Implementation of IAppSystemGroup
	virtual bool Create( );
	virtual bool PreInit( );
	virtual int Main( );
	virtual void PostShutdown();
	virtual void Destroy();

	// Use this version in cases where you can't control the main loop and
	// expect to be ticked
	virtual int Startup();
	virtual void Shutdown();

public:
	// Here's a hook to override the filesystem DLL that it tries to load.
	// By default, it uses FileSystem_GetFileSystemDLLName to figure this out.
	virtual bool GetFileSystemDLLName( char *pOut, int nMaxBytes );

protected:
	IFileSystem *m_pFileSystem;
	CFSAppSystemGroup *m_pChildAppSystemGroup;
};


class CBinLaunchableFSApp : public CFSApplication
{
public:
	CBinLaunchableFSApp( CFSAppSystemGroup *pAppSystemGroup ) : CFSApplication( pAppSystemGroup )
	{
	}

	virtual bool GetFileSystemDLLName( char *pOut, int nMaxChars )
	{
		// Our path should already include game\bin, so just use the filename directly
		// and don't try to figure out an absolute path to it as CFSApplication does.
		V_strncpy( pOut, "filesystem_stdio", nMaxChars );
		return true;
	}
};
