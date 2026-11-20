// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Defines a group of app systems that all have the same lifetime
// that need to be connected/initialized, etc. in a well-defined order

import std;

import ks.core.dbg;
import ks.core.logging;
import ks.core.memalloc;
import ks.core.platwindow;
import ks.core.scratcharena;
import ks.core.threadtools;
import ks.core.types;
import ks.core.vprof;
import ks.math.polyhedron;
import ks.platform;
import ks.simd;
import ks.tier1.cvar;
import ks.tier1.exprevaluator;
import ks.tier1.iappsystem;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.map_ops;
import ks.tier1.strtools;
import ks.tier1.vector_ops;
#include "appframework/ilaunchermgr.h"

#include "appframework/IAppSystemGroup.h"
#include "materialsystem/imaterialsystem.h"
#include "filesystem.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "datacache/iprecachesystem.h"
#include "datacache/iresourceaccesscontrol.h"
#include "istudiorender.h"
#include "inputsystem/iinputsystem.h"
#include "inputsystem/iinputstacksystem.h"
#include "localize/ilocalize.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "scenefilecache/ISceneFileCache.h"
#include "vscript/ivscript.h"
#include "vphysics_interface.h"
#include "matchmaking/imatchframework.h"
#include "filesystem_init.h"


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
extern ILoggingListener *g_pDefaultLoggingListener;


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CAppSystemGroup::CAppSystemGroup( CAppSystemGroup *pAppSystemParent )
{
	m_pParentAppSystem = pAppSystemParent;
	m_nCurrentStage = NONE;
	m_bLoadThroughFileSystem = false;
}


//-----------------------------------------------------------------------------
// Actually loads a DLL
//-----------------------------------------------------------------------------
CSysModule *CAppSystemGroup::LoadModuleDLL( const char *pDLLName )
{
	if ( m_bLoadThroughFileSystem )
		return static_cast<CFSAppSystemGroup *>( this )->LoadModuleDLLViaFileSystem( pDLLName );
	return Sys_LoadModule( pDLLName );
}


//-----------------------------------------------------------------------------
// Methods to load + unload DLLs
//-----------------------------------------------------------------------------
AppModule_t CAppSystemGroup::LoadModule( const char *pDLLName )
{
	ks::ScratchArena<> scratch;
	// Remove the extension when creating the name.
	int nLen = V_strlen( pDLLName ) + 1;
	char *pModuleName = (char*)scratch.AllocBytes( nLen );
	V_StripExtension( pDLLName, pModuleName, nLen );

	// See if we already loaded it...
	for ( int i = std::ssize( m_Modules ); --i >= 0; ) 
	{
		if ( m_Modules[i].m_pModuleName )
		{
			if ( !V_stricmp( pModuleName, m_Modules[i].m_pModuleName ) )
				return i;
		}
	}

	CSysModule *pSysModule = LoadModuleDLL( pDLLName );
	if (!pSysModule)
	{
		Warning("AppFramework : Unable to load module %s!\n", pDLLName );
		return APP_MODULE_INVALID;
	}

	int nIndex = std::ssize( m_Modules ); m_Modules.emplace_back();
	m_Modules[nIndex].m_pModule = pSysModule;
	m_Modules[nIndex].m_Factory = 0;
	m_Modules[nIndex].m_pModuleName = (char*)MemAlloc_Alloc( nLen );
	V_strncpy( m_Modules[nIndex].m_pModuleName, pModuleName, nLen );

	return nIndex;
}


int CAppSystemGroup::ReloadModule( const char * pDLLName )
{
	ks::ScratchArena<> scratch;
	// Remove the extension when creating the name.
	int nLen = V_strlen( pDLLName ) + 1;
	char *pModuleName = (char*)scratch.AllocBytes( nLen );
	V_StripExtension( pDLLName, pModuleName, nLen );

	// See if we already loaded it...
	for ( int i = std::ssize( m_Modules ); --i >= 0; ) 
	{
		Module_t &module = m_Modules[i];
		if ( module.m_pModuleName && !V_stricmp( pModuleName, module.m_pModuleName ) )
		{
			// found the module, reload
			Msg("Unloading module %s, dll %s\n", pModuleName, pDLLName );
			Sys_UnloadModule( m_Modules[i].m_pModule );
			Msg("Module %s unloaded, reloading\n", pModuleName );
			CSysModule *pSysModule = nullptr;
			CreateInterfaceFn fnFactory = nullptr;
			while( !pSysModule )	   
			{
				pSysModule = LoadModuleDLL( pDLLName );
				if( !pSysModule )
				{
					Warning("Cannot load, retrying in 5 seconds..\n");
					ThreadSleep( 5000 );
				}
				fnFactory = Sys_GetFactory( pSysModule ) ;
				if( !fnFactory )
				{
					Error( "Could not get factory from %s\n", pModuleName );
				}
				( *fnFactory )( "Reload Interface", nullptr ); // let the CreateInterface function work and do after-reload stuff
			}
			
			Msg( "Reload complete, module %p->%p, factory %llx->%llx\n", module.m_pModule, pSysModule, (uint64)(uintp)module.m_Factory, (uint64)(uintp)fnFactory );
			module.m_pModule = pSysModule;
			if( module.m_Factory )
			{ // don't reload factory pointer unless it was initialized to non-NULL
				module.m_Factory = fnFactory;
			}
			
			return 0; // no error
		}
	}
	
	Warning( "No such module: '%s' in appsystem @%p. Dumping available modules:\n", pModuleName, this );
	for ( int i = 0; i < std::ssize( m_Modules ); ++i ) 
	{
		Module_t &module = m_Modules[i];
		Msg("%25s %p %p\n", module.m_pModuleName, (void*)module.m_Factory, module.m_pModule );
	}
	
	return m_pParentAppSystem ? m_pParentAppSystem->ReloadModule( pDLLName ) : -1;
}


AppModule_t CAppSystemGroup::LoadModule( CreateInterfaceFn factory )
{
	if (!factory)
	{
		Warning("AppFramework : Unable to load module %p!\n", factory );
		return APP_MODULE_INVALID;
	}

	// See if we already loaded it...
	for ( int i = std::ssize( m_Modules ); --i >= 0; ) 
	{
		if ( m_Modules[i].m_Factory )
		{
			if ( m_Modules[i].m_Factory == factory )
				return i;
		}
	}

	int nIndex = std::ssize( m_Modules ); m_Modules.emplace_back();
	m_Modules[nIndex].m_pModule = nullptr;
	m_Modules[nIndex].m_Factory = factory;
	m_Modules[nIndex].m_pModuleName = nullptr; 
	return nIndex;
}

void CAppSystemGroup::UnloadAllModules()
{
	// NOTE: Iterate in reverse order so they are unloaded in opposite order
	// from loading
	for (int i = std::ssize( m_Modules ); --i >= 0; )
	{
		if ( m_Modules[i].m_pModule )
		{
			Sys_UnloadModule( m_Modules[i].m_pModule );
		}
		if ( m_Modules[i].m_pModuleName )
		{
			g_pMemAlloc->Free( m_Modules[i].m_pModuleName );
		}
	}
	m_Modules.clear();
}


static void *AppSystemPointer( const AppSystemRef &system )
{
	return std::visit( []( auto *p ) -> void * { return p; }, system );
}

//-----------------------------------------------------------------------------
// Methods to add/remove various global singleton systems 
//-----------------------------------------------------------------------------
void *CAppSystemGroup::AddSystem( AppModule_t module, const char *pInterfaceName )
{
	if (module == APP_MODULE_INVALID)
		return nullptr;

	auto it = m_SystemDict.find( pInterfaceName );
	if ( it != m_SystemDict.end() )
	{
		Warning("AppFramework : Attempted to add two systems with the same interface name %s!\n", pInterfaceName );
		return AppSystemPointer( m_Systems[ it->second ] );
	}

	contract_assert( (module >= 0) && (module < std::ssize( m_Modules )) );
	CreateInterfaceFn pFactory = m_Modules[module].m_pModule ? Sys_GetFactory( m_Modules[module].m_pModule ) : m_Modules[module].m_Factory;

	int retval;
	void *pSystem = pFactory( pInterfaceName, &retval );
	if ((retval != IFACE_OK) || (!pSystem))
	{
		Warning("AppFramework : Unable to create system %s!\n", pInterfaceName );
		return nullptr;
	}

	// The interface name tells which kind of system the factory returned.
	AppSystemRef system;
	if ( !V_strcmp( pInterfaceName, CVAR_INTERFACE_VERSION ) )
		system = static_cast<ICvar*>(pSystem);
	else if ( !V_strcmp( pInterfaceName, SDLMGR_INTERFACE_VERSION ) )
		system = static_cast<ILauncherMgr*>(pSystem);
	else if ( !V_strcmp( pInterfaceName, MATERIAL_SYSTEM_INTERFACE_VERSION ) )
		system = static_cast<IMaterialSystem*>(pSystem);
	else if ( !V_strcmp( pInterfaceName, FILESYSTEM_INTERFACE_VERSION ) )
		system = static_cast<IFileSystem*>(pSystem);
	else if ( !V_strcmp( pInterfaceName, DATACACHE_INTERFACE_VERSION ) )
		system = static_cast<IDataCache*>(pSystem);
	else if ( !V_strcmp( pInterfaceName, MDLCACHE_INTERFACE_VERSION ) )
		system = static_cast<IMDLCache*>(pSystem);
	else if ( !V_strcmp( pInterfaceName, STUDIO_DATA_CACHE_INTERFACE_VERSION ) )
		system = static_cast<IStudioDataCache*>(pSystem);
	else if ( !V_strcmp( pInterfaceName, PRECACHE_SYSTEM_INTERFACE_VERSION ) )
		system = static_cast<IPrecacheSystem*>(pSystem);
	else if ( !V_strcmp( pInterfaceName, RESOURCE_ACCESS_CONTROL_INTERFACE_VERSION ) )
		system = static_cast<IResourceAccessControl*>(pSystem);
	else if ( !V_strcmp( pInterfaceName, INPUTSYSTEM_INTERFACE_VERSION ) )
		system = static_cast<IInputSystem*>(pSystem);
	else if ( !V_strcmp( pInterfaceName, INPUTSTACKSYSTEM_INTERFACE_VERSION ) )
		system = static_cast<IInputStackSystem*>(pSystem);
	else if ( !V_strcmp( pInterfaceName, LOCALIZE_INTERFACE_VERSION ) )
		system = static_cast<ILocalize*>(pSystem);
	else if ( !V_strcmp( pInterfaceName, SOUNDEMITTERSYSTEM_INTERFACE_VERSION ) )
		system = static_cast<ISoundEmitterSystemBase*>(pSystem);
	else if ( !V_strcmp( pInterfaceName, SCENE_FILE_CACHE_INTERFACE_VERSION ) )
		system = static_cast<ISceneFileCache*>(pSystem);
	else if ( !V_strcmp( pInterfaceName, STUDIO_RENDER_INTERFACE_VERSION ) )
		system = static_cast<IStudioRender*>(pSystem);
	else if ( !V_strcmp( pInterfaceName, VSCRIPT_INTERFACE_VERSION ) )
		system = static_cast<IScriptManager*>(pSystem);
	else if ( !V_strcmp( pInterfaceName, VPHYSICS_INTERFACE_VERSION ) )
		system = static_cast<IPhysics*>(pSystem);
	else if ( !V_strcmp( pInterfaceName, IMATCHFRAMEWORK_VERSION_STRING ) )
		system = static_cast<IMatchFramework*>(pSystem);
	else
		system = static_cast<IAppSystem*>(pSystem);

	AppendSystem( system, pInterfaceName );
	return pSystem;
}

static const char *g_StageLookup[] = 
{
	"CREATION",
	"LOADING DEPENDENCIES",
	"CONNECTION",
	"PREINITIALIZATION",
	"INITIALIZATION",
	"POSTINITIALIZATION",
	"RUNNING",
	"PRESHUTDOWN",
	"SHUTDOWN",
	"POSTSHUTDOWN",
	"DISCONNECTION",
	"DESTRUCTION",
};

void CAppSystemGroup::ReportStartupFailure( int nErrorStage, int nSysIndex )
{
	static_assert( APPSYSTEM_GROUP_STAGE_COUNT == std::size( g_StageLookup ) );

	const char *pszStageDesc = "Unknown";
	if ( nErrorStage >= 0 && nErrorStage < ( int )std::size( g_StageLookup ) )
	{
		pszStageDesc = g_StageLookup[ nErrorStage ];
	}

	const char *pszSystemName = "(Unknown)";
	for ( auto &[name, sysIndex] : m_SystemDict )
	{
		if ( sysIndex != nSysIndex )
			continue;

		pszSystemName = name.c_str();
		break;
	}
		 
	// Walk the dictionary
	Warning( "System (%s) failed during stage %s\n", pszSystemName, pszStageDesc );
}

void CAppSystemGroup::AppendSystem( AppSystemRef system, const char *pInterfaceName )
{
	int sysIndex = std::ssize( m_Systems ); m_Systems.push_back( system );

	// Inserting into the dict will help us do named lookup later
	// Keep the first system of a name, as the module overload does.
	m_SystemDict.emplace( pInterfaceName, sysIndex );
}

void CAppSystemGroup::AddSystem( IAppSystem *pAppSystem, const char *pInterfaceName )
{
	if ( !pAppSystem )
		return;
	AppendSystem( pAppSystem, pInterfaceName );
}

void CAppSystemGroup::AddSystem( ILauncherMgr *pLauncherMgr, const char *pInterfaceName )
{
	if ( !pLauncherMgr )
		return;
	AppendSystem( pLauncherMgr, pInterfaceName );
}

void CAppSystemGroup::AddSystem( IFileSystem *pFileSystem, const char *pInterfaceName )
{
	if ( !pFileSystem )
		return;
	AppendSystem( pFileSystem, pInterfaceName );
}

void CAppSystemGroup::AddSystem( IMatchFramework *pMatchFramework, const char *pInterfaceName )
{
	if ( !pMatchFramework )
		return;
	AppendSystem( pMatchFramework, pInterfaceName );
}

void CAppSystemGroup::RemoveAllSystems()
{
	// NOTE: There's no deallcation here since we don't really know
	// how the allocation has happened. We could add a deallocation method
	// to the code in interface.h; although when the modules are unloaded
	// the deallocation will happen anyways
	m_Systems.clear();
	m_SystemDict.clear();
}


//-----------------------------------------------------------------------------
// Simpler method of doing the LoadModule/AddSystem thing.
//-----------------------------------------------------------------------------
bool CAppSystemGroup::AddSystems( AppSystemInfo_t *pSystemList )
{
	while ( pSystemList->m_pModuleName[0] )
	{
		AppModule_t module = LoadModule( pSystemList->m_pModuleName );
		void *pSystem = AddSystem( module, pSystemList->m_pInterfaceName );
		if ( !pSystem )
		{
			Warning( "Unable to load interface %s from %s, requested from EXE.\n", pSystemList->m_pInterfaceName, pSystemList->m_pModuleName );
			return false;
		}
		++pSystemList;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Methods to find various global singleton systems 
//-----------------------------------------------------------------------------
void *CAppSystemGroup::FindSystem( const char *pSystemName )
{
	if ( int *pSysIndex = ks::find_ptr( m_SystemDict, pSystemName ) )
		return AppSystemPointer( m_Systems[*pSysIndex] );

	// If it's not an interface we know about, it could be an older
	// version of an interface, or maybe something implemented by
	// one of the instantiated interfaces...

	// QUESTION: What order should we iterate this in?
	// It controls who wins if multiple ones implement the same interface
 	for ( int i = 0; i < std::ssize( m_Systems ); ++i )
	{
		void *pInterface = std::visit( [pSystemName]( auto *p ) { return p->QueryInterface( pSystemName ); }, m_Systems[i] );
		if (pInterface)
			return pInterface;
	}

	int nExternalCount = std::ssize( m_NonAppSystemFactories );
	for ( int i = 0; i < nExternalCount; ++i )
	{
		void *pInterface = m_NonAppSystemFactories[i]( pSystemName, nullptr );
		if (pInterface)
			return pInterface;
	}

	if ( m_pParentAppSystem )
	{
		void* pInterface = m_pParentAppSystem->FindSystem( pSystemName );
		if ( pInterface )
			return pInterface;
	}

	// No dice..
	return nullptr;
}


//-----------------------------------------------------------------------------
// Adds a factory to the system so other stuff can query it. Triggers a connect systems
//-----------------------------------------------------------------------------
void CAppSystemGroup::AddNonAppSystemFactory( CreateInterfaceFn fn )
{
	m_NonAppSystemFactories.push_back( fn );
}


//-----------------------------------------------------------------------------
// Removes a factory, triggers a disconnect call if it succeeds
//-----------------------------------------------------------------------------
void CAppSystemGroup::RemoveNonAppSystemFactory( CreateInterfaceFn fn )
{
	ks::erase_first( m_NonAppSystemFactories, fn );
}


//-----------------------------------------------------------------------------
// Causes the systems to reconnect to an interface
//-----------------------------------------------------------------------------
void CAppSystemGroup::ReconnectSystems( const char *pInterfaceName )
{
	// Let the libraries regrab the specified interface
	for (int i = 0; i < std::ssize( m_Systems ); ++i )
	{
		std::visit( [this, pInterfaceName]( auto *p ) { p->Reconnect( GetFactory(), pInterfaceName ); }, m_Systems[i] );
	}
}



//-----------------------------------------------------------------------------
// Gets at the parent appsystem group
//-----------------------------------------------------------------------------
CAppSystemGroup *CAppSystemGroup::GetParent()
{
	return m_pParentAppSystem;
}

	
//-----------------------------------------------------------------------------
// Deals with sorting dependencies and finding circular dependencies
//-----------------------------------------------------------------------------
void CAppSystemGroup::ComputeDependencies( LibraryDependencies_t &depend )
{
	bool bDone = false;
	while ( !bDone )
	{
		bDone = true;

		// If i depends on j, then i depends on what j depends on
		// Add secondary dependencies to i. We stop when no dependencies are added
		int nCount = depend.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			int nDependentCount = std::ssize( depend.m_Deps[i] );
			for ( int j = 0; j < nDependentCount; ++j )
			{
				int nIndex = depend.Find( depend.m_Deps[i][j].c_str() );
				if ( nIndex < 0 )
					continue;

				int nSecondaryDepCount = std::ssize( depend.m_Deps[nIndex] );
				for ( int k = 0; k < nSecondaryDepCount; ++k )
				{
					// Don't bother if we already contain the secondary dependency
					// Only added to m_Deps[i] when nIndex != i, so this stays valid
					const char *pSecondaryDependency = depend.m_Deps[nIndex][k].c_str();
					if ( depend.DependsOn( i, pSecondaryDependency ) )
						continue;

					// Check for circular dependency
					if ( !V_stricmp( pSecondaryDependency, depend.m_Names[i].c_str() ) )
					{
						Warning( "Encountered a circular dependency with library %s!\n", pSecondaryDependency );
						continue;
					}

					bDone = false;
					depend.AddDependency( i, pSecondaryDependency );
					nDependentCount = std::ssize( depend.m_Deps[i] );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Sorts dependencies
//-----------------------------------------------------------------------------
CAppSystemGroup::LibraryDependencies_t *CAppSystemGroup::sm_pSortDependencies;
bool CAppSystemGroup::SortLessFunc( const int &left, const int &right )
{
	const char *pLeftInterface = sm_pSortDependencies->m_Names[left].c_str();
	bool bRightDependsOnLeft = sm_pSortDependencies->DependsOn( right, pLeftInterface );
	return ( bRightDependsOnLeft );
}

void CAppSystemGroup::SortDependentLibraries( LibraryDependencies_t &depend )
{
	const int nCount = depend.Count();

	// The dependency table is dense in insertion order and entry i describes m_Systems[i], so the
	// two must agree. This is checked for real: contract_assert is compiled out
	// (-fcontract-evaluation-semantic=ignore), and a mismatch would index m_Systems out of range.
	if ( std::ssize( m_Systems ) != nCount )
	{
		Error( "AppFramework : %d dependency entries for %d app systems\n", nCount, static_cast<int>( std::ssize( m_Systems ) ) );
		return;
	}

	std::vector<int> indices( nCount );
	std::iota( indices.begin(), indices.end(), 0 );
	int *pIndices = indices.data();

	// Sort by dependency. Can't use fancy stl algorithms here because the sort func isn't strongly transitive.
	// Using lame bubble sort instead. We could speed this up using a proper depth-first graph walk, but it's not worth the effort.
	sm_pSortDependencies = &depend;
	bool bChanged = true;
	while ( bChanged )
	{
		bChanged = false;
		for ( int i = 1; i < nCount; i++ )
		{
			for ( int j = 0; j < i; j++ )
			{
				if ( SortLessFunc( pIndices[i], pIndices[j] ) )
				{
					int nTmp = pIndices[i];
					pIndices[i] = pIndices[j];
					pIndices[j] = nTmp;
					bChanged = true;
				}
			}
		}
	}
	sm_pSortDependencies = nullptr;


	// This logic will make it so it respects the specified initialization order
	// in the face of no dependencies telling the system otherwise. 
	// Doing this just for safety to reduce the amount of changed code
	bool bDone = false;
	while ( !bDone )
	{
		bDone = true;
		for ( int i = 1; i < nCount; ++i )
		{
			int nLeft = pIndices[i-1];
			int nRight = pIndices[i];
			if ( nRight > nLeft )
				continue;

			const char *pLeftInterface = depend.m_Names[nLeft].c_str();
			bool bRightDependsOnLeft = depend.DependsOn( nRight, pLeftInterface );
			if ( bRightDependsOnLeft )
				continue;
			std::swap( pIndices[i], pIndices[i-1] );
			bDone = false;
		}
	}

	// Reorder appsystem list + dictionary indexing
	const std::vector<AppSystemRef> previous = m_Systems;
	for ( int i = 0; i < nCount; ++i )
	{
		m_Systems[i] = previous[ pIndices[i] ];
	}

	// Remap system indices
	for ( auto &[name, sysIndex] : m_SystemDict )
	{
		int j = 0;
		for ( ; j < nCount; ++j )
		{
			if ( pIndices[j] == sysIndex )
			{
				sysIndex = j;
				break;
			}
		}
		contract_assert( j != nCount );
	}

}


//-----------------------------------------------------------------------------
// Finds appsystem names
//-----------------------------------------------------------------------------
const char *CAppSystemGroup::FindSystemName( int nIndex )
{
	for ( auto &[name, sysIndex] : m_SystemDict )
	{
		if ( sysIndex == nIndex )
			return name.c_str();
	}
	return nullptr;
}


//-----------------------------------------------------------------------------
// Method to load all dependent systems
//-----------------------------------------------------------------------------
bool CAppSystemGroup::LoadDependentSystems()
{
	LibraryDependencies_t dependencies;

	// First, load dependencies.
	for ( int i = 0; i < std::ssize( m_Systems ); ++i )
	{
		const AppSystemRef &system = m_Systems[i];
		const char *pInterfaceName = FindSystemName( i );
		int nDepIndex = dependencies.AddString( pInterfaceName );

		const AppSystemInfo_t *pDependencies = std::visit( []( auto *p ) { return p->GetDependencies(); }, system );
		if ( !pDependencies )
			continue;

		for ( ; pDependencies->m_pInterfaceName && pDependencies->m_pInterfaceName[0]; ++pDependencies )
		{
			dependencies.AddDependency( nDepIndex, pDependencies->m_pInterfaceName );

			CreateInterfaceFn factory = GetFactory();
			if ( factory( pDependencies->m_pInterfaceName, nullptr ) ) 
				continue;

			AppModule_t module = LoadModule( pDependencies->m_pModuleName );
			void *pDependency = AddSystem( module, pDependencies->m_pInterfaceName );
			if ( !pDependency )
			{
				Warning( "Unable to load interface %s from %s (Dependency of %s)\n", pDependencies->m_pInterfaceName, pDependencies->m_pModuleName, pInterfaceName );
				return false;
			}
		}
	}

	ComputeDependencies( dependencies );
	SortDependentLibraries( dependencies );
	return true;
}


//-----------------------------------------------------------------------------
// Method to connect/disconnect all systems
//-----------------------------------------------------------------------------
bool CAppSystemGroup::ConnectSystems()
{
	// Let the libraries grab any other interfaces they may need
	for (int i = 0; i < std::ssize( m_Systems ); ++i )
	{
		if ( !std::visit( [this]( auto *p ) { return p->Connect( GetFactory() ); }, m_Systems[i] ) )
		{
			ReportStartupFailure( CONNECTION, i );
			return false;
		}
	}

	return true;
}

void CAppSystemGroup::DisconnectSystems()
{
	// Disconnect in reverse order of connection
	for (int i = std::ssize( m_Systems ); --i >= 0; )
	{
		std::visit( []( auto *p ) { p->Disconnect(); }, m_Systems[i] );
	}
}


//-----------------------------------------------------------------------------
// Method to initialize/shutdown all systems
//-----------------------------------------------------------------------------
InitReturnVal_t CAppSystemGroup::InitSystems()
{
	for (int nSystemsInitialized = 0; nSystemsInitialized < std::ssize( m_Systems ); ++nSystemsInitialized )
	{
		InitReturnVal_t nRetVal = std::visit( []( auto *p ) { return p->Init(); }, m_Systems[nSystemsInitialized] );
		if ( nRetVal != INIT_OK )
		{
			for( int nSystemsRewind = nSystemsInitialized; nSystemsRewind-->0; )
			{
				std::visit( []( auto *p ) { p->Shutdown(); }, m_Systems[nSystemsRewind] );
			}
		
			ReportStartupFailure( INITIALIZATION, nSystemsInitialized );
			return nRetVal;
		}
	}
	return INIT_OK;
}

void CAppSystemGroup::ShutdownSystems()
{
	// Shutdown in reverse order of initialization
	for (int i = std::ssize( m_Systems ); --i >= 0; )
	{
		std::visit( []( auto *p ) { p->Shutdown(); }, m_Systems[i] );
	}
}


//-----------------------------------------------------------------------------
// Window management
//-----------------------------------------------------------------------------
void* CAppSystemGroup::CreateAppWindow( void *hInstance, const char *pTitle, bool bWindowed, int w, int h, bool bResizing )
{
#ifndef DEDICATED

// PBTODO
// 	extern IGLXMgr *g_pGLXMgr;
// 	g_pGLXMgr->CreateWindow( pTitle, bWindowed, w, h );
	return (void*)Sys_GetFactoryThis();	// Other stuff will query for ICocoaBridge out of this.
#endif
	return nullptr;
}

void CAppSystemGroup::SetAppWindowTitle( void* hWnd, const char *pTitle )
{
	Plat_SetWindowTitle( (PlatWindow_t)hWnd, pTitle );
}


//-----------------------------------------------------------------------------
// Returns the stage at which the app system group ran into an error
//-----------------------------------------------------------------------------
CAppSystemGroup::AppSystemGroupStage_t CAppSystemGroup::GetCurrentStage() const
{
	return m_nCurrentStage;
}


//-----------------------------------------------------------------------------
// Gets at a factory that works just like FindSystem
//-----------------------------------------------------------------------------
// This function is used to make this system appear to the outside world to
// function exactly like the currently existing factory system
CAppSystemGroup *s_pCurrentAppSystem;
void *AppSystemCreateInterfaceFn(const char *pName, int *pReturnCode)
{
	void *pInterface = s_pCurrentAppSystem->FindSystem( pName );
	if ( pReturnCode )
	{
		*pReturnCode = pInterface ? IFACE_OK : IFACE_FAILED;
	}
	return pInterface;
}


//-----------------------------------------------------------------------------
// Gets at a class factory for the topmost appsystem group in an appsystem stack
//-----------------------------------------------------------------------------
CreateInterfaceFn CAppSystemGroup::GetFactory()
{
	return AppSystemCreateInterfaceFn;
}

	
//-----------------------------------------------------------------------------
// Run/OnStartup/OnShutdown are templates in the header; this is their non-template part
//-----------------------------------------------------------------------------
void CAppSystemGroup::SetCurrentGroup( CAppSystemGroup *pGroup )
{
	s_pCurrentAppSystem = pGroup;
}

void CAppSystemGroup::UnloadSystemsAndModules()
{
	RemoveAllSystems();

	// Have to do this because the logging listeners & response policies may live in modules which are being unloaded
	// @TODO: this seems like a bad legacy practice... app systems should unload their spew handlers gracefully.
	LoggingSystem_ResetCurrentLoggingState();
	contract_assert( g_pDefaultLoggingListener != nullptr );
	LoggingSystem_RegisterLoggingListener( g_pDefaultLoggingListener );

	UnloadAllModules();
}


//-----------------------------------------------------------------------------
//
// This class represents a group of app systems that are loaded through the file system
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CFSAppSystemGroup::CFSAppSystemGroup( IFileSystem *pFileSystem, CAppSystemGroup *pAppSystemParent )
{
	m_pFileSystem = pFileSystem;
	m_pGameInfoPath[0] = 0;
	m_bLoadThroughFileSystem = true;
}


//-----------------------------------------------------------------------------
// Used by CFSApplication to set up necessary pointers if we can't do it in the constructor
//-----------------------------------------------------------------------------
void CFSAppSystemGroup::Setup( IFileSystem *pFileSystem, CAppSystemGroup *pParentAppSystem )
{
	m_pFileSystem = pFileSystem;
	m_pParentAppSystem = pParentAppSystem;
}


//-----------------------------------------------------------------------------
// Loads the module
//-----------------------------------------------------------------------------
CSysModule *CFSAppSystemGroup::LoadModuleDLLViaFileSystem( const char *pDLLName )
{
	return m_pFileSystem->LoadModule( pDLLName );
}


//-----------------------------------------------------------------------------
// Returns the game info path
//-----------------------------------------------------------------------------
const char *CFSAppSystemGroup::GetGameInfoPath()	const
{
	return m_pGameInfoPath;
}


//-----------------------------------------------------------------------------
// Sets up the search paths
//-----------------------------------------------------------------------------
bool CFSAppSystemGroup::SetupSearchPaths( const char *pStartingDir, bool bOnlyUseStartingDir, bool bIsTool )
{
	CFSSetupInfo setupInfo;
	setupInfo.m_pDirectoryName = pStartingDir;
	setupInfo.m_bOnlyUseDirectoryName = bOnlyUseStartingDir;
	if ( FileSystem_SetupEnvironment( setupInfo ) != FS_OK )
		return false;

	if ( FileSystem_SetBasePaths( m_pFileSystem ) != FS_OK )
		return false;

	// Finally, load the search paths for the "GAME" path.
	CFSSearchPathsInit searchPathsInit;
	searchPathsInit.m_pDirectoryName = setupInfo.m_GameInfoPath;
	searchPathsInit.m_pFileSystem = m_pFileSystem;
	if ( FileSystem_LoadSearchPaths( searchPathsInit ) != FS_OK )
		return false;

	FileSystem_AddSearchPath_Platform( m_pFileSystem, setupInfo.m_GameInfoPath );
	V_strncpy( m_pGameInfoPath, setupInfo.m_GameInfoPath, sizeof(m_pGameInfoPath) );
	return true;
}
