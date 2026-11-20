#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1999-2000 Valve Corporation

// Purpose: Defines a group of app systems that all have the same lifetime
// that need to be connected/initialized, etc. in a well-defined order

import ks.core.platform;
import ks.core.vprof;
import ks.platform;
import ks.simd;
import ks.tier1.cvar;
import ks.tier1.exprevaluator;
import ks.tier1.iappsystem;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.map_ops;
import std;

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class IAppSystem;
class ICvar;
class ILauncherMgr;
class IMaterialSystem;
class CSysModule;
class IFileSystem;
class IDataCache;
class IMDLCache;
class IStudioDataCache;
class IPrecacheSystem;
class IResourceAccessControl;
class IInputSystem;
class IInputStackSystem;
class ILocalize;
class ISoundEmitterSystemBase;
class ISceneFileCache;
class IStudioRender;
class IScriptManager;
class IPhysics;
class IMatchFramework;

//-----------------------------------------------------------------------------
// Handle to a DLL
//-----------------------------------------------------------------------------
typedef int AppModule_t;

enum
{
	APP_MODULE_INVALID = (AppModule_t)~0
};


//-----------------------------------------------------------------------------
// A system owned by a group, held by kind and driven through std::visit. ICvar and
// ILauncherMgr, IMaterialSystem, IFileSystem, IDataCache, ... are concrete app systems without an
// IAppSystem base; any other system is reached through IAppSystem.
//-----------------------------------------------------------------------------
using AppSystemRef = std::variant< IAppSystem *, ICvar *, ILauncherMgr *, IMaterialSystem *, IFileSystem *,
	IDataCache *, IMDLCache *, IStudioDataCache *, IPrecacheSystem *, IResourceAccessControl *,
	IInputSystem *, IInputStackSystem *, ILocalize *, ISoundEmitterSystemBase *, ISceneFileCache *, IStudioRender *, IScriptManager *, IPhysics *, IMatchFramework * >;


//-----------------------------------------------------------------------------
// The application hooks. There is no abstract base: Run() is a template deduced on the
// most-derived group type, so these resolve statically. A group type must provide:
//
//   bool Create();         Tell the group the DLLs and singleton interfaces you want to
//                          instantiate. Return false if there's any problems and the app will abort
//   bool PreInit();        Work after AppSystems are connected but before they are initialized.
//                          Return false if there's any problems and the app will abort
//   int  Main();           Main loop implemented by the application
//   void PostShutdown();   Work after all AppSystems are shut down
//   void Destroy();        Called after all modules are unloaded
//
// and may hide the defaults inherited from CAppSystemGroup:
//
//   bool PostInit();       Work after AppSystems are initialized but before main is run
//   void PreShutdown();    Work before the AppSystems are shut down
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// This class represents a group of app systems that all have the same lifetime
// that need to be connected/initialized, etc. in a well-defined order
//-----------------------------------------------------------------------------
class CAppSystemGroup
{
public:
	// Used to determine where we exited out from the system
	enum AppSystemGroupStage_t
	{
		CREATION = 0,
		DEPENDENCIES,
		CONNECTION,
		PREINITIALIZATION,
		INITIALIZATION,
		POSTINITIALIZATION,
		RUNNING,
		PRESHUTDOWN,
		SHUTDOWN,
		POSTSHUTDOWN,
		DISCONNECTION,
		DESTRUCTION,

		APPSYSTEM_GROUP_STAGE_COUNT,
		NONE,	// This means no error
	};

public:
	// constructor
	CAppSystemGroup( CAppSystemGroup *pParentAppSystem = nullptr );

	// Runs the app system group.
	// First, modules are loaded, next they are connected, followed by initialization
	// Then Main() is run
	// Then modules are shut down, disconnected, and unloaded
	template< class SELF >
	int Run( this SELF &self )
	{
		// The factory now uses this app system group
		self.SetCurrentGroup( &self );

		// Load, connect, init
		int nRetVal = self.OnStartup( self );

		if ( self.m_nCurrentStage == RUNNING )
		{
			// Main loop implemented by the application
			nRetVal = self.Main();
		}

		// Shutdown, disconnect, unload
		self.OnShutdown( self );

		// The factory now uses the parent's app system group
		self.SetCurrentGroup( self.GetParent() );

		return nRetVal;
	}

	// Default implementations...
	bool PostInit() { return true; }
	void PreShutdown() { }

	// Returns the stage at which the app system group ran into an error
	AppSystemGroupStage_t GetCurrentStage() const;
	
	int ReloadModule( const char * pDLLName );

protected:
	// These methods are meant to be called by derived classes of CAppSystemGroup

	// Methods to load + unload DLLs
	AppModule_t LoadModule( const char *pDLLName );
	AppModule_t LoadModule( CreateInterfaceFn factory );

	// Method to add various global singleton systems 
	// Returns the system (as the interface pointer the factory gave out), or nullptr.
	void *AddSystem( AppModule_t module, const char *pInterfaceName );
	void AddSystem( IAppSystem *pAppSystem, const char *pInterfaceName );
	void AddSystem( ILauncherMgr *pLauncherMgr, const char *pInterfaceName );
	void AddSystem( IFileSystem *pFileSystem, const char *pInterfaceName );
	void AddSystem( IMatchFramework *pMatchFramework, const char *pInterfaceName );

	// Simpler method of doing the LoadModule/AddSystem thing.
	// Make sure the last AppSystemInfo has a NULL module name
	bool AddSystems( AppSystemInfo_t *pSystems );

	// Adds a factory to the system so other stuff can query it. Triggers a connect systems
	void AddNonAppSystemFactory( CreateInterfaceFn fn );

	// Removes a factory, triggers a disconnect call if it succeeds
	void RemoveNonAppSystemFactory( CreateInterfaceFn fn );

	// Causes the systems to reconnect to an interface
	void ReconnectSystems( const char *pInterfaceName );

	// Method to look up a particular named system...
	void *FindSystem( const char *pInterfaceName );

	// Creates the app window (windowed app only)
	void *CreateAppWindow( void *hInstance, const char *pTitle, bool bWindowed, int w, int h, bool bResizing );
	void SetAppWindowTitle( void* hWnd, const char *pTitle );

	// Gets at a class factory for the topmost appsystem group in an appsystem stack
	static CreateInterfaceFn GetFactory();

private:
	struct Module_t
	{
		CSysModule *m_pModule;
		CreateInterfaceFn m_Factory;
		char *m_pModuleName;
	};

	// Dense in insertion order: entry i describes m_Systems[i].
	struct LibraryDependencies_t
	{
		std::vector< std::string > m_Names;
		std::vector< std::vector< std::string > > m_Deps;	// per library, in insertion order
		ks::idict< int > m_Index;

		// Index of pName, added if new.
		int AddString( const char *pName )
		{
			auto [it, bNew] = m_Index.try_emplace( pName, (int)m_Names.size() );
			if ( bNew )
			{
				m_Names.emplace_back( pName );
				m_Deps.emplace_back();
			}
			return it->second;
		}
		int Find( const char *pName ) const
		{
			auto it = m_Index.find( pName );
			return it == m_Index.end() ? -1 : it->second;
		}
		int Count() const { return (int)m_Names.size(); }

		bool DependsOn( int i, const char *pName ) const
		{
			return std::ranges::find( m_Deps[i], std::string_view( pName ) ) != m_Deps[i].end();
		}
		void AddDependency( int i, const char *pName )
		{
			if ( !DependsOn( i, pName ) )
				m_Deps[i].emplace_back( pName );
		}
	};

	// Startup/shutdown sequences; the hooks are called on the most-derived group (self)
	template< class SELF >
	int OnStartup( SELF &self )
	{
		// The factory now uses this app system group
		SetCurrentGroup( this );

		// Call an installed application creation function
		m_nCurrentStage = CREATION;
		if ( !self.Create() )
			return -1;

		// Load dependent libraries
		m_nCurrentStage = DEPENDENCIES;
		if ( !LoadDependentSystems() )
			return -1;

		// Let all systems know about each other
		m_nCurrentStage = CONNECTION;
		if ( !ConnectSystems() )
			return -1;

		// Allow the application to do some work before init
		m_nCurrentStage = PREINITIALIZATION;
		if ( !self.PreInit() )
			return -1;

		// Call Init on all App Systems
		m_nCurrentStage = INITIALIZATION;
		int nRetVal = InitSystems();
		if ( nRetVal != INIT_OK )
			return -1;

		m_nCurrentStage = POSTINITIALIZATION;
		if ( !self.PostInit() )
			return -1;

		m_nCurrentStage = RUNNING;
		return nRetVal;
	}

	template< class SELF >
	void OnShutdown( SELF &self )
	{
		// The factory now uses this app system group
		SetCurrentGroup( this );

		switch( m_nCurrentStage )
		{
		case RUNNING:
		case POSTINITIALIZATION:
			break;

		case PREINITIALIZATION:
		case INITIALIZATION:
			goto disconnect;

		case CREATION:
		case DEPENDENCIES:
		case CONNECTION:
			goto destroy;

		default:
			break;
		}

		// Allow the application to do some work before shutdown
		m_nCurrentStage = PRESHUTDOWN;
		self.PreShutdown();

		// Cal Shutdown on all App Systems
		m_nCurrentStage = SHUTDOWN;
		ShutdownSystems();

		// Allow the application to do some work after shutdown
		m_nCurrentStage = POSTSHUTDOWN;
		self.PostShutdown();

	disconnect:
		// Systems should disconnect from each other
		m_nCurrentStage = DISCONNECTION;
		DisconnectSystems();

	destroy:
		// Unload all DLLs loaded in the AppCreate block
		m_nCurrentStage = DESTRUCTION;
		UnloadSystemsAndModules();

		// Call an installed application destroy function
		self.Destroy();
	}

	// Removes the systems, restores the default logging state and unloads the modules
	void UnloadSystemsAndModules();

	static void SetCurrentGroup( CAppSystemGroup *pGroup );

	void UnloadAllModules( );
	void RemoveAllSystems();

	// Method to load all dependent systems
	bool LoadDependentSystems();

	// Method to connect/disconnect all systems
	bool ConnectSystems( );
	void DisconnectSystems();

	// Method to initialize/shutdown all systems
	InitReturnVal_t InitSystems();
	void ShutdownSystems();
 
	// Gets at the parent appsystem group
	CAppSystemGroup *GetParent();

	// Loads a module the standard way
	CSysModule *LoadModuleDLL( const char *pDLLName );

	void	ComputeDependencies( LibraryDependencies_t &depend );
	void	SortDependentLibraries( LibraryDependencies_t &depend );
	const char *FindSystemName( int nIndex );
	static bool	SortLessFunc( const int &left, const int &right );

	void	ReportStartupFailure( int nErrorStage, int nSysIndex );
	void	AppendSystem( AppSystemRef system, const char *pInterfaceName );

	std::vector< Module_t > m_Modules;
	std::vector< AppSystemRef > m_Systems;
	std::vector< CreateInterfaceFn > m_NonAppSystemFactories;
	std::map<std::string, int> m_SystemDict;
	CAppSystemGroup *m_pParentAppSystem;
	AppSystemGroupStage_t m_nCurrentStage;
	bool m_bLoadThroughFileSystem;	// set by CFSAppSystemGroup: its modules load through the file system

	static LibraryDependencies_t *sm_pSortDependencies;
	friend void *AppSystemCreateInterfaceFn(const char *pName, int *pReturnCode);
	friend class CFSAppSystemGroup;
};


//-----------------------------------------------------------------------------
// This class represents a group of app systems that are loaded through the file system
//-----------------------------------------------------------------------------
class CFSAppSystemGroup : public CAppSystemGroup
{
	friend class CAppSystemGroup;

public:
	CFSAppSystemGroup( IFileSystem *pFileSystem = nullptr, CAppSystemGroup *pParentAppSystem = nullptr );

	// Used by CFSApplication to set up necessary pointers if we can't do it in the constructor
	void Setup( IFileSystem *pFileSystem, CAppSystemGroup *pParentAppSystem );

protected:
	// Sets up the search paths
	bool SetupSearchPaths( const char *pStartingDir, bool bOnlyUseStartingDir, bool bIsTool );

	// Returns the game info path. Only works if you've called SetupSearchPaths first
	const char *GetGameInfoPath() const;

private:
	// Loads a module through the file system (CAppSystemGroup::LoadModuleDLL dispatches here by kind)
	CSysModule *LoadModuleDLLViaFileSystem( const char *pDLLName );

	IFileSystem *m_pFileSystem;
	char m_pGameInfoPath[ MAX_PATH ];
};


//-----------------------------------------------------------------------------
// Special helper for game info directory suggestion
//-----------------------------------------------------------------------------

class CFSSetupInfo;	// Forward declaration

//
// SuggestGameInfoDirFn_t
//		Game info suggestion function.
//		Provided by the application to possibly detect the suggested game info
//		directory and initialize all the game-info-related systems appropriately.
// Parameters:
//		pFsSetupInfo			file system setup information if available.
//		pchPathBuffer			buffer to hold game info directory path on return.
//		nBufferLength			length of the provided buffer to hold game info directory path.
//		pbBubbleDirectories		should contain "true" on return to bubble the directories up searching for game info file.
// Return values:
//		Returns "true" if the game info directory path suggestion is available and
//		was successfully copied into the provided buffer.
//		Returns "false" otherwise, interpreted that no suggestion will be used.
//
typedef bool ( * SuggestGameInfoDirFn_t ) ( CFSSetupInfo const *pFsSetupInfo, char *pchPathBuffer, int nBufferLength, bool *pbBubbleDirectories );

//
// SetSuggestGameInfoDirFn
//		Installs the supplied game info directory suggestion function.
// Parameters:
//		pfnNewFn				the new game info directory suggestion function.
// Returns:
//		The previously installed suggestion function or NULL if none was installed before.
//		This function never fails.
//
SuggestGameInfoDirFn_t SetSuggestGameInfoDirFn( SuggestGameInfoDirFn_t pfnNewFn );
