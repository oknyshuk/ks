// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;

// ------------------------------------------------------------------------------------ //
// InterfaceReg.
// ------------------------------------------------------------------------------------ //

InterfaceReg::InterfaceReg( InstantiateInterfaceFn fn, const char *pName ) :
	m_pName(pName)
{
	ks::registry &registry = ks::current_registry();
	m_CreateFn = fn;
	m_pNext = registry.interfaces;
	registry.interfaces = this;
}

// ------------------------------------------------------------------------------------ //
// Registries.
// ------------------------------------------------------------------------------------ //

namespace ks
{
	namespace
	{
		constinit registry s_EngineRegistry;
		constinit registry s_ServerRegistry;
		constinit registry s_ClientRegistry;
		constinit registry *s_pCurrentRegistry = &s_EngineRegistry;
	}

	registry &engine_registry() { return s_EngineRegistry; }
	registry &server_registry() { return s_ServerRegistry; }
	registry &client_registry() { return s_ClientRegistry; }
	registry &current_registry() { return *s_pCurrentRegistry; }

	bool enter_registry( registry &r )
	{
		s_pCurrentRegistry = &r;
		return true;
	}

	bool leave_registry()
	{
		s_pCurrentRegistry = &s_EngineRegistry;
		return true;
	}

	void *registry::create_interface( const char *pName, int *pReturnCode ) const
	{
		for ( const InterfaceReg *pCur = interfaces; pCur; pCur = pCur->m_pNext )
		{
			if ( std::strcmp( pCur->m_pName, pName ) == 0 )
			{
				if ( pReturnCode )
					*pReturnCode = IFACE_OK;
				return pCur->m_CreateFn();
			}
		}

		if ( pReturnCode )
			*pReturnCode = IFACE_FAILED;
		return nullptr;
	}
}

// ------------------------------------------------------------------------------------ //
// CreateInterface.
// This is the primary exported function by a dll, referenced by name via dynamic binding
// that exposes an opqaue function pointer to the interface.
//
// We have the Internal variant so Sys_GetFactoryThis() returns the correct internal 
// symbol under GCC/Linux/Mac as CreateInterface is DLL_EXPORT so its global so the loaders
// on those OS's pick exactly 1 of the CreateInterface symbols to be the one that is process wide and 
// all Sys_GetFactoryThis() calls find that one, which doesn't work. Using the internal walkthrough here
// makes sure Sys_GetFactoryThis() has the dll specific symbol and GetProcAddress() returns the module specific
// function for CreateInterface again getting the dll specific symbol we need.
// ------------------------------------------------------------------------------------ //
void* CreateInterfaceInternal( const char *pName, int *pReturnCode )
{
	return ks::engine_registry().create_interface( pName, pReturnCode );
}

void* CreateInterface( const char *pName, int *pReturnCode )
{
    return CreateInterfaceInternal( pName, pReturnCode );
}

// ------------------------------------------------------------------------------------ //
// Statically linked modules.
//
// The executable is its own "engine" module, so Sys_LoadModule( "engine" ) needs no loading.
// The game sides are not modules: the engine takes their factories from the executable
// (ServerGameFactory(), ClientGameFactory()). Anything else (plugins, ...) goes to the
// dynamic loader.
// ------------------------------------------------------------------------------------ //
namespace
{
	struct StaticModule_t
	{
		const char *m_pName;			// as the engine asks for it, without directory, "lib" or ".so"
		CreateInterfaceFn m_Factory;
	};

	// The handle Sys_LoadModule() hands out is the address of the entry, so it needs no unloading.
	StaticModule_t s_StaticModules[] =
	{
		{ "engine", &CreateInterfaceInternal },		// the executable's own factory
	};

	// "/path/bin/libclient.so", "client.so", "client" -> the table entry for client
	StaticModule_t *FindStaticModule( const char *pModuleName )
	{
		if ( !pModuleName )
			return nullptr;

		std::string_view name = pModuleName;
		if ( const std::size_t nSlash = name.find_last_of( "/\\" ); nSlash != std::string_view::npos )
			name.remove_prefix( nSlash + 1 );
		if ( name.starts_with( "lib" ) )
			name.remove_prefix( 3 );
		if ( name.ends_with( DLL_EXT_STRING ) )
			name.remove_suffix( std::strlen( DLL_EXT_STRING ) );

		for ( StaticModule_t &module : s_StaticModules )
		{
			if ( name == module.m_pName && module.m_Factory )
				return &module;
		}
		return nullptr;
	}

	bool IsStaticModule( const CSysModule *pModule )
	{
		const auto p = reinterpret_cast<std::uintptr_t>( pModule );
		return p >= reinterpret_cast<std::uintptr_t>( std::begin( s_StaticModules ) ) &&
			p < reinterpret_cast<std::uintptr_t>( std::end( s_StaticModules ) );
	}
}



static void *Sys_GetProcAddress( const char *pModuleName, const char *pName )
{
	ks::platform::library *lib = ks::platform::load_library( pModuleName, nullptr );
	if ( lib == nullptr )
		return nullptr;
	void *symbol = ks::platform::library_symbol( lib, pName );
	ks::platform::close_library( lib );
	return symbol;
}

static void *Sys_GetProcAddress( HMODULE hModule, const char *pName )
{
	return ks::platform::library_symbol( reinterpret_cast<ks::platform::library *>( hModule ), pName );
}

bool Sys_IsDebuggerPresent()
{
	return Plat_IsInDebugSession();
}

struct ThreadedLoadLibaryContext_t
{
	const char *m_pLibraryName;
	HMODULE m_hLibrary;
	DWORD m_nError;
	ThreadedLoadLibaryContext_t() : m_pLibraryName(nullptr), m_hLibrary(0), m_nError(0) {}
};



// global to propagate a library load error from thread into Sys_LoadModule
static DWORD g_nLoadLibraryError = 0;

static HMODULE Sys_LoadLibraryGuts( const char *pLibraryName )
{
	char str[1024];

	// How to get a string out of a #define on the command line.
	const char *pModuleExtension = DLL_EXT_STRING;	
	const char *pModuleAddition = pModuleExtension;

	V_strncpy( str, pLibraryName, sizeof(str) );
	if ( !V_stristr( str, pModuleExtension ) )
	{
		V_strncat( str, pModuleAddition, sizeof(str) );
	}
	V_FixSlashes( str );

	HMODULE ret = reinterpret_cast<HMODULE>( ks::platform::load_library( str, nullptr ) );
	if ( ! ret )
	{
		const char *pError = ks::platform::library_error();
		if ( pError && ( std::strstr( pError, "No such file" ) == 0 ) && ( std::strstr( pError, "image not found") == 0 ) )
		{
			Msg( " failed to dlopen %s error=%s\n", str, pError );

		}
	}

// 	if( ret )
// 		StackToolsNotify_LoadedLibrary( str );

	return ret;
}

static HMODULE Sys_LoadLibrary( const char *pLibraryName )
{
	// load a library. If a library suffix is set, look for the library first with that name
	const char *pSuffix = nullptr;
	
if ( CommandLine()->FindParm( "-valveinternal" ) )
	{
		pSuffix = "_valveinternal";
	}
	if ( pSuffix )
	{
		char nameBuf[MAX_PATH];
		std::strcpy( nameBuf, pLibraryName );
		char *pDot = std::strchr( nameBuf, '.' );
		if ( pDot )
			*pDot = 0;
		V_strncat( nameBuf, pSuffix, sizeof( nameBuf ), COPY_ALL_CHARACTERS );
		HMODULE hRet = Sys_LoadLibraryGuts( nameBuf );
		if ( hRet )
			return hRet;
	}
	return Sys_LoadLibraryGuts( pLibraryName );
}



//-----------------------------------------------------------------------------
// Purpose: Keeps a flag if the current dll/exe loaded any debug modules
// This flag can also get set if the current process discovers any other debug
// modules loaded by other dlls
//-----------------------------------------------------------------------------
static bool s_bRunningWithDebugModules = false;


//-----------------------------------------------------------------------------
// Purpose: Loads a DLL/component from disk and returns a handle to it
// Input  : *pModuleName - filename of the component
// Output : opaque handle to the module (hides system dependency)
//-----------------------------------------------------------------------------
CSysModule *Sys_LoadModule( const char *pModuleName )
{
	// Either the DLL must be a minimum footprint
	// file in the depot (MFP) or a filesystem GetLocalCopy() call must be made
	// prior to the call to this routine.
	HMODULE hDLL = nullptr;

	// Modules that are linked into this executable need no loading.
	if ( StaticModule_t *pStatic = FindStaticModule( pModuleName ) )
		return reinterpret_cast<CSysModule *>( pStatic );

	if ( !V_IsAbsolutePath( pModuleName ) )
	{
		// full path wasn't passed in, using the current working dir
		char szAbsoluteModuleName[1024];
		char szCwd[1024];
		ks::platform::current_directory( szCwd, sizeof( szCwd ) );
		if (szCwd[std::strlen(szCwd) - 1] == '/' || szCwd[std::strlen(szCwd) - 1] == '\\' )
		{
			szCwd[std::strlen(szCwd) - 1] = 0;
		}

		std::size_t cCwd = std::strlen( szCwd );
		V_snprintf( szAbsoluteModuleName, sizeof(szAbsoluteModuleName), "%s/bin/lib%s", szCwd, pModuleName );
		hDLL = Sys_LoadLibrary( szAbsoluteModuleName );
	}

	if ( !hDLL )
	{
		// full path failed, let LoadLibrary() try to search the PATH now
		hDLL = Sys_LoadLibrary( pModuleName );
	}

	// dlerror() is the only place the loader says *why* -- a missing dependency, an undefined
	// symbol -- and a null handle on its own reads the same for all of them.
	if ( !hDLL )
		Warning( "Module %s failed to load! Error: (%s)\n", pModuleName, ks::platform::library_error() );

	return reinterpret_cast<CSysModule *>(hDLL);
}

//-----------------------------------------------------------------------------
// Purpose: Determine if any debug modules were loaded
//-----------------------------------------------------------------------------
bool Sys_RunningWithDebugModules()
{
	if ( !s_bRunningWithDebugModules )
	{
	}
	return s_bRunningWithDebugModules;
}


//-----------------------------------------------------------------------------
// Purpose: Unloads a DLL/component from
// Input  : *pModuleName - filename of the component
// Output : opaque handle to the module (hides system dependency)
//-----------------------------------------------------------------------------
void Sys_UnloadModule( CSysModule *pModule )
{
	if ( !pModule )
		return;

	if ( IsStaticModule( pModule ) )
		return;		// linked into the executable, nothing to unload

	HMODULE	hDLL = reinterpret_cast<HMODULE>(pModule);

//$$$$$$ mikesart: for testing with valgrind don't unload so...	dlclose((void *)hDLL);
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to a function, given a module
// Input  : module - windows HMODULE from Sys_LoadModule() 
//			*pName - proc name
// Output : factory for this module
//-----------------------------------------------------------------------------
CreateInterfaceFn Sys_GetFactory( CSysModule *pModule )
{
	if ( !pModule )
		return nullptr;

	if ( IsStaticModule( pModule ) )
		return reinterpret_cast<const StaticModule_t *>( pModule )->m_Factory;

	HMODULE	hDLL = reinterpret_cast<HMODULE>(pModule);
	// Linux gives this error:
	//../public/interface.cpp: In function `IBaseInterface *(*Sys_GetFactory
	//(CSysModule *)) (const char *, int *)':
	//../public/interface.cpp:154: ISO C++ forbids casting between
	//pointer-to-function and pointer-to-object
	//
	// so lets get around it :)
	return (CreateInterfaceFn)( Sys_GetProcAddress( hDLL, CREATEINTERFACE_PROCNAME ) );
}

//-----------------------------------------------------------------------------
// Purpose: returns the instance of this module
// Output : interface_instance_t
//-----------------------------------------------------------------------------
CreateInterfaceFn Sys_GetFactoryThis( void )
{
	return &CreateInterfaceInternal;
}

//-----------------------------------------------------------------------------
// Purpose: returns the instance of the named module
// Input  : *pModuleName - name of the module
// Output : interface_instance_t - instance of that module
//-----------------------------------------------------------------------------
CreateInterfaceFn Sys_GetFactory( const char *pModuleName )
{
	if ( const StaticModule_t *pStatic = FindStaticModule( pModuleName ) )
		return pStatic->m_Factory;

	// see Sys_GetFactory( CSysModule *pModule ) for an explanation
	return (CreateInterfaceFn)( Sys_GetProcAddress( pModuleName, CREATEINTERFACE_PROCNAME ) );
}

//-----------------------------------------------------------------------------
// Purpose: get the interface for the specified module and version
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
bool Sys_LoadInterface(
	const char *pModuleName,
	const char *pInterfaceVersionName,
	CSysModule **pOutModule,
	void **pOutInterface )
{
	CSysModule *pMod = Sys_LoadModule( pModuleName );
	if ( !pMod )
		return false;

	CreateInterfaceFn fn = Sys_GetFactory( pMod );
	if ( !fn )
	{
		Sys_UnloadModule( pMod );
		return false;
	}

	*pOutInterface = fn( pInterfaceVersionName, nullptr );
	if ( !( *pOutInterface ) )
	{
		Sys_UnloadModule( pMod );
		return false;
	}

	if ( pOutModule )
		*pOutModule = pMod;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Place this as a singleton at module scope (e.g.) and use it to get the factory from the specified module name.  
// 
// When the singleton goes out of scope (.dll unload if at module scope),
//  then it'll call Sys_UnloadModule on the module so that the refcount is decremented 
//  and the .dll actually can unload from memory.
//-----------------------------------------------------------------------------
CDllDemandLoader::CDllDemandLoader( char const *pchModuleName ) : 
	m_pchModuleName( pchModuleName ), 
	m_hModule( 0 ),
	m_bLoadAttempted( false )
{
}

CDllDemandLoader::~CDllDemandLoader()
{
	Unload();
}

CreateInterfaceFn CDllDemandLoader::GetFactory()
{
	if ( !m_hModule && !m_bLoadAttempted )
	{
		m_bLoadAttempted = true;
		m_hModule = Sys_LoadModule( m_pchModuleName );
	}

	if ( !m_hModule )
	{
		return nullptr;
	}

	return Sys_GetFactory( m_hModule );
}

void CDllDemandLoader::Unload()
{
	if ( m_hModule )
	{
		Sys_UnloadModule( m_hModule );
		m_hModule = 0;
	}
}
