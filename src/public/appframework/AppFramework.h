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
// Sets the application instance
//-----------------------------------------------------------------------------
void SetAppInstance( void* hInstance );


//-----------------------------------------------------------------------------
// This class is a helper class used for file system-based applications.
// It loads up the file system in preparation for using it to load other
// required modules.
//-----------------------------------------------------------------------------
class CFSApplicationBase : public CAppSystemGroup
{
public:
	CFSApplicationBase();

	// Hooks of the application group
	bool Create( );
	bool PreInit( );
	void PostShutdown();
	void Destroy();

protected:
	IFileSystem *m_pFileSystem;
};

// The application group that loads the file system, then runs the child group (the real
// application) with the file system as its module loader.
template< class CHILD >
class CFSApplication : public CFSApplicationBase
{
public:
	CFSApplication( CHILD *pAppSystemGroup ) : m_pChildAppSystemGroup( pAppSystemGroup ) {}

	// Run main loop
	int Main( )
	{
		// Now that the file system is loaded, we can load up main libraries through it
		m_pChildAppSystemGroup->Setup( m_pFileSystem, this );
		return m_pChildAppSystemGroup->Run( );
	}

protected:
	CHILD *m_pChildAppSystemGroup;
};
