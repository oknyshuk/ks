// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

import std;
import ks.tier1;
#include "isys.h"
#include "dedicated.h"
#include "engine_hlds_api.h"
#include "idedicatedexports.h"
#include "materialsystem/imaterialsystem.h"
#include "istudiorender.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "vphysics_interface.h"
#include "vscript/ivscript.h"

bool InitInstance( );
void ProcessConsoleInput( void );

#define stringize(a) #a
#define engine_binary(a,b,c) a stringize(b) c


char g_szEXEName[ 256 ];

#include "console/TextConsoleUnix.h"
extern CTextConsoleUnix console;


//-----------------------------------------------------------------------------
// Purpose: Implements OS Specific layer ( loosely )
//-----------------------------------------------------------------------------
class CSys : public ISys
{
public:
	virtual		~CSys();

	virtual bool LoadModules( CDedicatedAppSystemGroup *pAppSystemGroup );

	void		Sleep( int msec );
	bool		GetExecutableName( char *out );
	void		ErrorMessage( int level, const char *msg );

	void		WriteStatusText( char *szText );
	void		UpdateStatus( int force );

	long		LoadLibrary( const char *lib );
	void		FreeLibrary( long library );
	void		*GetProcAddress( long library, const char *name );

	bool		CreateConsoleWindow( void );
	void		DestroyConsoleWindow( void );

	void		ConsoleOutput ( char *string );
	char		*ConsoleInput (void);
	void		Printf(const char *fmt, ...);
};

static CSys g_Sys;
ISys *sys = &g_Sys;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSys::~CSys()
{
	sys = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : msec
// Output : 
//-----------------------------------------------------------------------------
void CSys::Sleep( int msec )
{
    ks::platform::sleep_nanos( (unsigned long long)msec * 1000000ull );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : handle, function name-
// Output : void *
//-----------------------------------------------------------------------------
void *CSys::GetProcAddress( long library, const char *name )
{
	return ks::platform::library_symbol( reinterpret_cast<ks::platform::library *>( library ), name );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *lib -
// Output : long
//-----------------------------------------------------------------------------
long CSys::LoadLibrary( const char *lib )
{
	ks::platform::library *hDll = nullptr;

    char    cwd[1024];
    char    absolute_lib[1024];
    
    if (!ks::platform::current_directory(cwd, sizeof(cwd)))
        ErrorMessage(1, "Sys_LoadLibrary: Couldn't determine current directory.");
        
    if (cwd[std::strlen(cwd)-1] == '/')
        cwd[strlen(cwd)-1] = 0;
        
    V_snprintf(absolute_lib, sizeof( absolute_lib ), "%s/%s", cwd, lib);
    
    hDll = ks::platform::load_library( absolute_lib, nullptr );
    if ( !hDll )
    {
        ErrorMessage( 1, ks::platform::library_error() );
    }
	else
	{
		StackToolsNotify_LoadedLibrary( absolute_lib );
	}
	return (long)hDll;
}

void CSys::FreeLibrary( long library )
{
	if ( !library )
		return;

	ks::platform::close_library( reinterpret_cast<ks::platform::library *>( library ) );
}

bool CSys::GetExecutableName( char *out )
{
	char *name = std::strrchr(g_szEXEName, '/' );
	if ( name )
	{
		std::strcpy( out, name + 1);
		return true;
	}
	else
	{
		return false;
	}
}

/*
==============
ErrorMessage

Engine is erroring out, display error in message box
==============
*/
void CSys::ErrorMessage( int level, const char *msg )
{
	Error( "%s\n", msg );
	std::exit( -1 );
}

void CSys::UpdateStatus( int force )
{
}

/*
================
ConsoleOutput

Print text to the dedicated console
================
*/
void CSys::ConsoleOutput (char *string)
{
	console.Print(string);
}

/*
==============
Printf

Engine is printing to console
==============
*/
void CSys::Printf(const char *fmt, ...)
{
	// Dump text to debugging console.
	std::va_list argptr;
	char szText[1024];

	__builtin_va_start (argptr, fmt);
	V_vsnprintf (szText, sizeof( szText ), fmt, argptr);
	__builtin_va_end (argptr);

	// Get Current text and append it.
	ConsoleOutput( szText );
}

/*
================
ConsoleInput

================
*/
char *CSys::ConsoleInput( void )
{
	return console.GetLine();
}

/*
==============
WriteStatusText

==============
*/
void CSys::WriteStatusText( char *szText )
{
}

/*
==============
CreateConsoleWindow

Create console window ( overridable? )
==============
*/
bool CSys::CreateConsoleWindow( void )
{
	return true;
}

/*
==============
DestroyConsoleWindow

==============
*/
void CSys::DestroyConsoleWindow( void )
{
}

/*
================
GameInit
================
*/
bool CSys::LoadModules( CDedicatedAppSystemGroup *pAppSystemGroup )
{
	
	engine = (IDedicatedServerAPI *)pAppSystemGroup->FindSystem( VENGINE_HLDS_API_VERSION );
															// obsolete i think SetCVarIF( (ICvar*)pAppSystemGroup->FindSystem( VENGINE_CVAR_INTERFACE_VERSION ) );

	IMaterialSystem* pMaterialSystem = (IMaterialSystem*)pAppSystemGroup->FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION );
	pMaterialSystem->SetShaderAPI( "shaderapiempty.so" );


	return true;
}

extern int DedicatedServerMain(int argc, char *argv[]);
extern "C" int DedicatedMain( int argc, char *argv[] );

int DedicatedMain( int argc, char *argv[] )
{
	return DedicatedServerMain(argc,argv);
}
