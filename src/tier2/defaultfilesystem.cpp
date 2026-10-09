// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

// Purpose: A higher level link library for general use in the game and tools.

#include <tier2/tier2.h>
#include <filesystem_init.h>
import ks.core;


static CSysModule *g_pFullFileSystemModule = nullptr;

void* DefaultCreateInterfaceFn(const char *pName, int *pReturnCode)
{
	if ( pReturnCode )
	{
		*pReturnCode = 0;
	}
	return nullptr;
}

void InitDefaultFileSystem( void )
{
	contract_assert( ( !g_pFullFileSystem ) && "Already set up the file system" );

	if ( !Sys_LoadInterface( "filesystem_stdio", FILESYSTEM_INTERFACE_VERSION,
		&g_pFullFileSystemModule, (void**)&g_pFullFileSystem ) )
	{
		std::exit(0);
	}

	if ( !g_pFullFileSystem->Connect( DefaultCreateInterfaceFn ) )
	{
		std::exit(0);
	}

	if ( g_pFullFileSystem->Init() != INIT_OK )
	{
		std::exit(0);
	}

	g_pFullFileSystem->RemoveAllSearchPaths();
	g_pFullFileSystem->AddSearchPath( "", "LOCAL", PATH_ADD_TO_HEAD );
	g_pFullFileSystem->AddSearchPath( "", "DEFAULT_WRITE_PATH", PATH_ADD_TO_HEAD );
}

void ShutdownDefaultFileSystem(void)
{
	contract_assert( g_pFullFileSystem && "File system not set up" );
	g_pFullFileSystem->Shutdown();
	g_pFullFileSystem->Disconnect();
	Sys_UnloadModule( g_pFullFileSystemModule );
}
