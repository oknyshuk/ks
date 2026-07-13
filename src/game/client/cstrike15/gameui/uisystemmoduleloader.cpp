//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include <stdio.h>

#include "iengineui.h"
#include "uisystemmoduleloader.h"
#include "IVguiModule.h"
#include "ServerBrowser/IServerBrowser.h"

#include "localize/ilocalize.h"
#include <keyvalues.h>


#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// instance of class
CUISystemModuleLoader g_VModuleLoader;

bool bSteamCommunityFriendsVersion = false;

#include <tier0/dbg.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CUISystemModuleLoader, IUIModuleLoader, UIMODULELOADER_INTERFACE_VERSION, g_VModuleLoader);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUISystemModuleLoader::CUISystemModuleLoader()
{
	m_bModulesInitialized = false;
	m_bPlatformShouldRestartAfterExit = false;
	m_pPlatformModuleData = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CUISystemModuleLoader::~CUISystemModuleLoader()
{
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the module loader has acquired the platform mutex and loaded the modules
//-----------------------------------------------------------------------------
bool CUISystemModuleLoader::IsPlatformReady()
{
	return m_bModulesInitialized;
}

//-----------------------------------------------------------------------------
// Purpose: sets up all the modules for use
//-----------------------------------------------------------------------------
bool CUISystemModuleLoader::InitializeAllModules(CreateInterfaceFn *factorylist, int factorycount)
{
	if ( IsGameConsole() )
	{
		// not valid for 360
		return false;
	}

	bool bSuccess = true;

	// Init vgui in the modules
	int i;
	for ( i = 0; i < m_Modules.Count(); i++ )
	{
		if (!m_Modules[i].moduleInterface->Initialize(factorylist, factorycount))
		{
			bSuccess = false;
			Error("Platform Error: module failed to initialize\n");
		}
	}

	// create a table of all the loaded modules
	CreateInterfaceFn *moduleFactories = (CreateInterfaceFn *)_alloca(sizeof(CreateInterfaceFn) * m_Modules.Count());
	for ( i = 0; i < m_Modules.Count(); i++ )
	{
		moduleFactories[i] = Sys_GetFactory(m_Modules[i].module);
	}

	// give the modules a chance to link themselves together
	for (i = 0; i < m_Modules.Count(); i++)
	{
		if (!m_Modules[i].moduleInterface->PostInitialize(moduleFactories, m_Modules.Count()))
		{
			bSuccess = false;
			Error("Platform Error: module failed to initialize\n");
		}
	}

	m_bModulesInitialized = true;
	return bSuccess;
}

//-----------------------------------------------------------------------------
// Purpose: Loads and initializes all the modules specified in the platform file
//-----------------------------------------------------------------------------
bool CUISystemModuleLoader::LoadPlatformModules(CreateInterfaceFn *factorylist, int factorycount, bool useSteamModules)
{
	// The legacy VGUI "serverbrowser" platform module has been removed
	// (pending an RmlUi port). There are no platform modules to load.
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: gives all platform modules a chance to Shutdown gracefully
//-----------------------------------------------------------------------------
void CUISystemModuleLoader::ShutdownPlatformModules()
{
	if ( IsGameConsole() )
	{
		// not valid for 360
		return;
	}

	// static include guard to prevent recursive calls
	static bool runningFunction = false;
	if (runningFunction)
		return;

	runningFunction = true;

	// deactivate all the modules first
	DeactivatePlatformModules();

	// give all the modules notice of quit
	int i;

	for ( i = 0; i < m_Modules.Count(); i++ )
	{
		m_Modules[i].moduleInterface->Shutdown();
	}

	runningFunction = false;
}

//-----------------------------------------------------------------------------
// Purpose: Deactivates all the modules (puts them into in inactive but recoverable state)
//-----------------------------------------------------------------------------
void CUISystemModuleLoader::DeactivatePlatformModules()
{
	for (int i = 0; i < m_Modules.Count(); i++)
	{
		m_Modules[i].moduleInterface->Deactivate();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reenables all the deactivated platform modules
//-----------------------------------------------------------------------------
void CUISystemModuleLoader::ReactivatePlatformModules()
{
	for (int i = 0; i < m_Modules.Count(); i++)
	{
		m_Modules[i].moduleInterface->Reactivate();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Disables and unloads platform
//-----------------------------------------------------------------------------
void CUISystemModuleLoader::UnloadPlatformModules()
{
	for (int i = 0; i < m_Modules.Count(); i++)
	{
		g_pFullFileSystem->UnloadModule(m_Modules[i].module);
	}

	m_Modules.RemoveAll();

	if (m_pPlatformModuleData)
	{
		m_pPlatformModuleData->deleteThis();
		m_pPlatformModuleData = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame
//-----------------------------------------------------------------------------
void CUISystemModuleLoader::RunFrame()
{
}

//-----------------------------------------------------------------------------
// Purpose: returns number of modules loaded
//-----------------------------------------------------------------------------
int CUISystemModuleLoader::GetModuleCount()
{
	return m_Modules.Count();
}

//-----------------------------------------------------------------------------
// Purpose: returns the string menu name (unlocalized) of a module
//			moduleIndex is of the range [0, GetModuleCount())
//-----------------------------------------------------------------------------
const char *CUISystemModuleLoader::GetModuleLabel(int moduleIndex)
{
	return m_Modules[moduleIndex].data->GetString("MenuName", "< unknown >");
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CUISystemModuleLoader::IsModuleVisible(int moduleIndex)
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: brings the specified module to the foreground
//-----------------------------------------------------------------------------
bool CUISystemModuleLoader::IsModuleHidden(int moduleIndex)
{
	return m_Modules[moduleIndex].data->GetBool("Hidden", false);
}

//-----------------------------------------------------------------------------
// Purpose: brings the specified module to the foreground
//-----------------------------------------------------------------------------
bool CUISystemModuleLoader::ActivateModule(int moduleIndex)
{
	if (!m_Modules.IsValidIndex(moduleIndex))
		return false;

	m_Modules[moduleIndex].moduleInterface->Activate();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: look up a module by name
//-----------------------------------------------------------------------------

int CUISystemModuleLoader::GetModuleIndexFromName( const char* moduleName )
{
	int result = -1;

	for ( int i = 0; i < GetModuleCount(); i++ )
	{
		if ( !stricmp( GetModuleLabel( i ), moduleName ) || !stricmp( m_Modules[i].data->GetName(), moduleName ) )
		{
			result = i;
			break;
		}
	}

	return result;

}


//-----------------------------------------------------------------------------
// Purpose: activates a module by name
//-----------------------------------------------------------------------------
bool CUISystemModuleLoader::ActivateModule( const char *moduleName )
{
	// ActivateModule checks the validity of the index, so it will
	// work correctly even if moduleName is not a valid module
	return ActivateModule( GetModuleIndexFromName( moduleName ) );
}

//-----------------------------------------------------------------------------
// Purpose: returns a modules interface factory
//-----------------------------------------------------------------------------
CreateInterfaceFn CUISystemModuleLoader::GetModuleFactory(int moduleIndex)
{
	return Sys_GetFactory(m_Modules[moduleIndex].module);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUISystemModuleLoader::PostMessageToAllModules(KeyValues *message)
{
	message->deleteThis();
}


//-----------------------------------------------------------------------------
// Purpose: Post a message to a particular module
//-----------------------------------------------------------------------------

// posts a message to a single module
bool CUISystemModuleLoader::PostMessageToModule( int moduleIndex, KeyValues *message )
{
	if ( !m_Modules.IsValidIndex( moduleIndex ) )
		return false;

	return true;
}

bool CUISystemModuleLoader::PostMessageToModule( const char *moduleName, KeyValues *message )
{
	// ActivateModule checks the validity of the index, so it will
	// work correctly even if moduleName is not a valid module
	return PostMessageToModule( GetModuleIndexFromName( moduleName ), message );
}


//-----------------------------------------------------------------------------
// Purpose: sets the the platform should update and restart when it quits
//-----------------------------------------------------------------------------
void CUISystemModuleLoader::SetPlatformToRestart()
{
	m_bPlatformShouldRestartAfterExit = true;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CUISystemModuleLoader::ShouldPlatformRestart()
{
	return m_bPlatformShouldRestartAfterExit;
}
