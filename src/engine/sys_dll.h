#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Expose functions from sys_dll.cpp.

import ks.tier1;
import std;

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IDataCache;
class IPhysics;
class IMDLCache;
class IMatSystemSurface;
class IInputSystem;
class IDedicatedExports;
class ISoundEmitterSystemBase;



//-----------------------------------------------------------------------------
// Class factories
//-----------------------------------------------------------------------------
// This factory gets to many of the major app-single systems,
// including the material system, vgui, vgui surface, the file system.
extern CreateInterfaceFn g_AppSystemFactory;

// this factory connect the AppSystemFactory + client.dll + gameui.dll
extern CreateInterfaceFn g_GameSystemFactory;


//-----------------------------------------------------------------------------
// Singleton interfaces
//-----------------------------------------------------------------------------
extern IPhysics *g_pPhysics;
extern IDedicatedExports *dedicated;

//-----------------------------------------------------------------------------
// Other singletons
//-----------------------------------------------------------------------------


inline bool InEditMode()
{
	return false;
}


struct modinfo_t
{
	char szInfo[ 256 ];
	char szDL  [ 256 ];
	char szHLVersion[ 32 ];
	int version;
	int size;
	bool svonly;
	bool cldll;
};

extern modinfo_t gmodinfo;

void LoadEntityDLLs( const char *szBaseDir, bool bServerOnly );
void UnloadEntityDLLs( void );

// This returns true if someone called Error() or Sys_Error() and we're exiting.
// Since we call exit() from inside those, some destructors need to be safe and not crash.
bool IsInErrorExit();

// error message
bool Sys_MessageBox(const char *title, const char *info, bool bShowOkAndCancel);

bool ServerDLL_Load( bool bServerOnly );
void ServerDLL_Unload();

extern CreateInterfaceFn g_ServerFactory;

const char *Sys_GetVersionString();
const char *Sys_GetProductString();

// SysDlls 3 loading and unloading entry points
bool SysDll3_Load();
void SysDll3_Unload();
extern LoggingChannelID_t LOG_SERVER_LOG;;
