// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Deals with singleton

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
#if defined(CLIENT_DLL)
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "debugoverlay_shared.h"
#elif defined(GAME_DLL)
#endif
#include "igamesystem.h"
#include "datacache/imdlcache.h"

// enable this #define (here or in your vpc) to have 
// each GameSystem accounted for individually in the vprof.
// You must enable VPROF_LEVEL 1 too.
// #define VPROF_ACCOUNT_GAMESYSTEMS

// Pointer to a member method of IGameSystem
typedef void (IGameSystem::*GameSystemFunc_t)();

// Pointer to a member method of IGameSystem
typedef void (IGameSystemPerFrame::*PerFrameGameSystemFunc_t)();

// Used to invoke a method of all added Game systems in order
static void InvokeMethod( GameSystemFunc_t f, char const *timed = 0 );
// Used to invoke a method of all added Game systems in order
static void InvokeMethodTickProgress( GameSystemFunc_t f, char const *timed = 0 );
// Used to invoke a method of all added Game systems in reverse order
static void InvokeMethodReverseOrder( GameSystemFunc_t f );

// Used to invoke a method of all added Game systems in order
static void InvokePerFrameMethod( PerFrameGameSystemFunc_t f, char const *timed = 0 );

static bool s_bSystemsInitted = false; 

// List of all installed Game systems
static std::vector< IGameSystem* > s_GameSystems;
// List of all installed Game systems
static std::vector< IGameSystemPerFrame* > s_GameSystemsPerFrame;

// The map name
static char* s_pMapName = 0;

static CBasePlayer *s_pRunCommandPlayer = nullptr;
static CUserCmd *s_pRunCommandUserCmd = nullptr;

//-----------------------------------------------------------------------------
// Auto-registration of game systems
//-----------------------------------------------------------------------------
static	CAutoGameSystem *s_pSystemList = nullptr;

CAutoGameSystem::CAutoGameSystem( char const *name ) :
	m_pszName( name )
{
	// If s_GameSystems hasn't been initted yet, then add ourselves to the global list
	// because we don't know if the constructor for s_GameSystems has happened yet.
	// Otherwise, we can add ourselves right into that list.
	if ( s_bSystemsInitted )
	{
		Add( this );
	}
	else
	{
		m_pNext = s_pSystemList;
		s_pSystemList = this;
	}
}

static	CAutoGameSystemPerFrame *s_pPerFrameSystemList = nullptr;

//-----------------------------------------------------------------------------
// Purpose: This is a CAutoGameSystem which also cares about the "per frame" hooks
//-----------------------------------------------------------------------------
CAutoGameSystemPerFrame::CAutoGameSystemPerFrame( char const *name ) :
	m_pszName( name )
{
	// If s_GameSystems hasn't been initted yet, then add ourselves to the global list
	// because we don't know if the constructor for s_GameSystems has happened yet.
	// Otherwise, we can add ourselves right into that list.
	if ( s_bSystemsInitted )
	{
		Add( this );
	}
	else
	{
		m_pNext = s_pPerFrameSystemList;
		s_pPerFrameSystemList = this;
	}
}

//-----------------------------------------------------------------------------
// destructor, cleans up automagically....
//-----------------------------------------------------------------------------
IGameSystem::~IGameSystem()
{
	Remove( this );
}

//-----------------------------------------------------------------------------
// destructor, cleans up automagically....
//-----------------------------------------------------------------------------
IGameSystemPerFrame::~IGameSystemPerFrame()
{
	Remove( this );
}


//-----------------------------------------------------------------------------
// Adds a system to the list of systems to run
//-----------------------------------------------------------------------------
void IGameSystem::Add( IGameSystem* pSys )
{
	s_GameSystems.push_back( pSys );
	if ( dynamic_cast< IGameSystemPerFrame * >( pSys ) != nullptr )
	{
		s_GameSystemsPerFrame.push_back( static_cast< IGameSystemPerFrame * >( pSys ) );
	}
}


//-----------------------------------------------------------------------------
// Removes a system from the list of systems to update
//-----------------------------------------------------------------------------
void IGameSystem::Remove( IGameSystem* pSys )
{
	ks::erase_first( s_GameSystems, pSys );
	if ( dynamic_cast< IGameSystemPerFrame * >( pSys ) != nullptr )
	{
		ks::erase_first( s_GameSystemsPerFrame, static_cast< IGameSystemPerFrame * >( pSys ) );
	}
}

//-----------------------------------------------------------------------------
// Removes *all* systems from the list of systems to update
//-----------------------------------------------------------------------------
void IGameSystem::RemoveAll(  )
{
	s_GameSystems.clear();
	s_GameSystemsPerFrame.clear();
}


//-----------------------------------------------------------------------------
// Client systems can use this to get at the map name
//-----------------------------------------------------------------------------
char const*	IGameSystem::MapName()
{
	return s_pMapName;
}

#ifndef CLIENT_DLL
CBasePlayer *IGameSystem::RunCommandPlayer()
{
	return s_pRunCommandPlayer;
}

CUserCmd *IGameSystem::RunCommandUserCmd()
{
	return s_pRunCommandUserCmd;
}
#endif

//-----------------------------------------------------------------------------
// Invokes methods on all installed game systems
//-----------------------------------------------------------------------------
bool IGameSystem::InitAllSystems()
{
	int i;

	{
		// first add any auto systems to the end
		CAutoGameSystem *pSystem = s_pSystemList;
		while ( pSystem )
		{
			if ( ks::find_index( s_GameSystems, pSystem ) == -1 )
			{
				Add( pSystem );
			}
			else
			{
				DevWarning( 1, "AutoGameSystem already added to game system list!!!\n" );
			}
			pSystem = pSystem->m_pNext;
		}
		s_pSystemList = nullptr;
	}

	{
		CAutoGameSystemPerFrame *pSystem = s_pPerFrameSystemList;
		while ( pSystem )
		{
			if ( ks::find_index( s_GameSystems, pSystem ) == -1 )
			{
				Add( pSystem );
			}
			else
			{
				DevWarning( 1, "AutoGameSystem already added to game system list!!!\n" );
			}

			pSystem = pSystem->m_pNext;
		}
		s_pSystemList = nullptr;
	}
	// Now remember that we are initted so new CAutoGameSystems will add themselves automatically.
	s_bSystemsInitted = true;

	// PS3: haul the fontlib into memory; some systems (eg vgui) need it.
	for ( i = 0; i < std::ssize( s_GameSystems ); ++i )
	{
		MDLCACHE_COARSE_LOCK();
		MDLCACHE_CRITICAL_SECTION();

		IGameSystem *sys = s_GameSystems[i];

		bool valid = sys->Init();

		if ( !valid )
		{
			DevWarning( 1, "Failed to load %s\n", sys->Name() );
			return false;
		}
	}

	return true;
}

void IGameSystem::PostInitAllSystems( void )
{
	InvokeMethod( &IGameSystem::PostInit, "PostInit" );
}

void IGameSystem::ShutdownAllSystems()
{
	InvokeMethodReverseOrder( &IGameSystem::Shutdown );
}

void IGameSystem::LevelInitPreEntityAllSystems( char const* pMapName )
{
	// Store off the map name
	if ( s_pMapName )
	{
		delete[] s_pMapName;
	}

	int len = V_strlen(pMapName) + 1;
	s_pMapName = new char [ len ];
	V_strncpy( s_pMapName, pMapName, len );
	
	InvokeMethodTickProgress( &IGameSystem::LevelInitPreEntity, "LevelInitPreEntity" );
}

void IGameSystem::LevelInitPostEntityAllSystems( void )
{
	InvokeMethod( &IGameSystem::LevelInitPostEntity, "LevelInitPostEntity" );
}

void IGameSystem::LevelShutdownPreEntityAllSystems()
{
	InvokeMethodReverseOrder( &IGameSystem::LevelShutdownPreEntity );
}

void IGameSystem::LevelShutdownPostEntityAllSystems()
{
	InvokeMethodReverseOrder( &IGameSystem::LevelShutdownPostEntity );

	if ( s_pMapName )
	{
		delete[] s_pMapName;
		s_pMapName = 0;
	}
}

void IGameSystem::OnSaveAllSystems()
{
	InvokeMethod( &IGameSystem::OnSave );
}

void IGameSystem::OnRestoreAllSystems()
{
	InvokeMethod( &IGameSystem::OnRestore );
}

void IGameSystem::SafeRemoveIfDesiredAllSystems()
{
	InvokeMethodReverseOrder( &IGameSystem::SafeRemoveIfDesired );
}

#ifdef CLIENT_DLL

void IGameSystem::PreRenderAllSystems()
{
	CVProfScope _( "IGameSystem::PreRenderAllSystems", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	InvokePerFrameMethod( &IGameSystemPerFrame::PreRender );
}

void IGameSystem::UpdateAllSystems( float frametime )
{
	SafeRemoveIfDesiredAllSystems();

	int i;
	int c = std::ssize( s_GameSystemsPerFrame );
	MDLCACHE_CRITICAL_SECTION();
	for ( i = 0; i < c; ++i )
	{
		IGameSystemPerFrame *sys = s_GameSystemsPerFrame[i];
		sys->Update( frametime );
	}
}

void IGameSystem::PostRenderAllSystems()
{
	InvokePerFrameMethod( &IGameSystemPerFrame::PostRender );
}

#else

void IGameSystem::FrameUpdatePreEntityThinkAllSystems()
{
	InvokePerFrameMethod( &IGameSystemPerFrame::FrameUpdatePreEntityThink );
}

void IGameSystem::FrameUpdatePostEntityThinkAllSystems()
{
	SafeRemoveIfDesiredAllSystems();

	InvokePerFrameMethod( &IGameSystemPerFrame::FrameUpdatePostEntityThink );
}

void IGameSystem::PreClientUpdateAllSystems() 
{
	InvokePerFrameMethod( &IGameSystemPerFrame::PreClientUpdate );
}

#endif


//-----------------------------------------------------------------------------
// Invokes a method on all installed game systems in proper order
//-----------------------------------------------------------------------------
void InvokeMethod( GameSystemFunc_t f, char const *timed /*=0*/ )
{
	(void)timed;

	MDLCACHE_COARSE_LOCK();
	MDLCACHE_CRITICAL_SECTION();
	int i;
	int c = std::ssize( s_GameSystems );
	for ( i = 0; i < c ; ++i )
	{
		IGameSystem *sys = s_GameSystems[i];

		(sys->*f)();
	}
}

//-----------------------------------------------------------------------------
// Invokes a method on all installed game systems in proper order
//-----------------------------------------------------------------------------
void InvokeMethodTickProgress( GameSystemFunc_t f, char const *timed /*=0*/ )
{
	(void)timed;

	int i;
	int c = std::ssize( s_GameSystems );
	for ( i = 0; i < c ; ++i )
	{
		IGameSystem *sys = s_GameSystems[i];

		MDLCACHE_COARSE_LOCK();
		MDLCACHE_CRITICAL_SECTION();
#if defined( CLIENT_DLL )
		engine->TickProgressBar();
#endif
		(sys->*f)();
	}
}
//-----------------------------------------------------------------------------
// Invokes a method on all installed game systems in proper order
//-----------------------------------------------------------------------------
void InvokePerFrameMethod( PerFrameGameSystemFunc_t f, char const *timed /*=0*/ )
{
	(void)timed;

	int i;
	int c = std::ssize( s_GameSystemsPerFrame );
	for ( i = 0; i < c ; ++i )
	{
		IGameSystemPerFrame *sys  = s_GameSystemsPerFrame[i];
		(sys->*f)();
	}
}

//-----------------------------------------------------------------------------
// Invokes a method on all installed game systems in reverse order
//-----------------------------------------------------------------------------
void InvokeMethodReverseOrder( GameSystemFunc_t f )
{
	int i;
	int c = std::ssize( s_GameSystems );
	MDLCACHE_CRITICAL_SECTION();
	for ( i = c; --i >= 0; )
	{
		IGameSystem *sys = s_GameSystems[i];
		(sys->*f)();
	}
}
