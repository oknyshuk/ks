#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

// Purpose: Defines a group of app systems that all have the same lifetime
// that need to be connected/initialized, etc. in a well-defined order

#include "appframework/tier3app.h"

//-----------------------------------------------------------------------------
// Forward declarations 
//-----------------------------------------------------------------------------
class IDedicatedServerAPI;


//-----------------------------------------------------------------------------
// Singleton interfaces 
//-----------------------------------------------------------------------------
extern IDedicatedServerAPI *engine;


//-----------------------------------------------------------------------------
// Inner loop: initialize, shutdown main systems, load the file system
//-----------------------------------------------------------------------------
class CDedicatedAppSystemGroup : public CTier2FSApp
{
	typedef CTier2FSApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create( );
	virtual bool PreInit( );
	virtual int Main( );
	virtual void PostShutdown();
	virtual void Destroy();

	// Used to chain to base class
	AppModule_t LoadModule( CreateInterfaceFn factory )
	{
		return CFSAppSystemGroup::LoadModule( factory );
	}

	// Method to add various global singleton systems 
	bool AddSystems( AppSystemInfo_t *pSystems )
	{
		return CFSAppSystemGroup::AddSystems( pSystems );
	}

	void *FindSystem( const char *pInterfaceName )
	{
		return CFSAppSystemGroup::FindSystem( pInterfaceName );
	}
};


// main server entry point after init
void RunServer( bool bSupressStdIOBecauseWeAreAForkedChild );

void RunServerSubProcesses( int nNumChildInstances );

// modify the command line, replacing all occurrences of ## with nIndex
void PerformCommandLineSubstitutions( int nIndex );


// forked subprocess index. 0 for master or not forked mode
extern int g_nSubProcessId;
