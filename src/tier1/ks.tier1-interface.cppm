// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1:interface;

import std;
import ks.math;

export extern "C++" {

// All interfaces derive from this.
class IBaseInterface
{
public:
	virtual	~IBaseInterface() {}
};

inline constexpr const char *CREATEINTERFACE_PROCNAME = "CreateInterface";

typedef void* (*CreateInterfaceFn)(const char *pName, int *pReturnCode);
typedef void* (*InstantiateInterfaceFn)();

// Used internally to register classes.
class __attribute__(( visibility( "hidden" ) )) InterfaceReg
{
public:
	InterfaceReg(InstantiateInterfaceFn fn, const char *pName);

public:
	InstantiateInterfaceFn	m_CreateFn;
	const char				*m_pName;

	InterfaceReg			*m_pNext; // For the global list.
};

// Use this to expose an interface that can have multiple instances.
// e.g.:
// EXPOSE_INTERFACE( CInterfaceImp, IInterface, "MyInterface001" )
// This will expose a class called CInterfaceImp that implements IInterface (a pure class)
// clients can receive a pointer to this class by calling CreateInterface( "MyInterface001" )
//
// In practice, the shared header file defines the interface (IInterface) and version name ("MyInterface001")
// so that each component can use these names/vtables to communicate
//
// A single class can support multiple interfaces through multiple inheritance
//
// Use this if you want to write the factory function.
// Use this to expose a singleton interface with a global variable you've created.
// Use this to expose a singleton interface. This creates the global variable for you automatically.
// load/unload components
class CSysModule;

// interface return status
enum 
{
	IFACE_OK = 0,
	IFACE_FAILED
};

//-----------------------------------------------------------------------------
// This function is automatically exported and allows you to access any interfaces exposed with the above macros.
// if pReturnCode is set, it will return one of the following values (IFACE_OK, IFACE_FAILED)
// extend this for other error conditions/code
//-----------------------------------------------------------------------------
extern "C" void* CreateInterface(const char *pName, int *pReturnCode);

//-----------------------------------------------------------------------------
// UNDONE: This is obsolete, use the module load/unload/get instead!!!
//-----------------------------------------------------------------------------
extern CreateInterfaceFn	Sys_GetFactory( CSysModule *pModule );
extern CreateInterfaceFn	Sys_GetFactory( const char *pModuleName );
extern CreateInterfaceFn	Sys_GetFactoryThis( void );

//-----------------------------------------------------------------------------
// Load & Unload should be called in exactly one place for each module
// The factory for that module should be passed on to dependent components for
// proper versioning.
//-----------------------------------------------------------------------------
extern CSysModule			*Sys_LoadModule( const char *pModuleName );
extern void					Sys_UnloadModule( CSysModule *pModule );

// Determines if current process is running with any debug modules
extern bool					Sys_RunningWithDebugModules();

// This is a helper function to load a module, get its factory, and get a specific interface.
// You are expected to free all of these things.
// Returns false and cleans up if any of the steps fail.
bool Sys_LoadInterface(
	const char *pModuleName,
	const char *pInterfaceVersionName,
	CSysModule **pOutModule,
	void **pOutInterface );

bool Sys_IsDebuggerPresent();

//-----------------------------------------------------------------------------
// Purpose: Place this as a singleton at module scope (e.g.) and use it to get the factory from the specified module name.  
// 
// When the singleton goes out of scope (.dll unload if at module scope),
//  then it'll call Sys_UnloadModule on the module so that the refcount is decremented 
//  and the .dll actually can unload from memory.
//-----------------------------------------------------------------------------
class CDllDemandLoader
{
public:
						CDllDemandLoader( char const *pchModuleName );
	virtual				~CDllDemandLoader();
	CreateInterfaceFn	GetFactory();
	void				Unload();

private:

	char const	*m_pchModuleName;
	CSysModule	*m_hModule;
	bool		m_bLoadAttempted;
};

}
