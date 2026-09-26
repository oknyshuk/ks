//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "reflect_annotations.h"
#include "reflect_datamap.h"
#include "basetypes.h"
#include "globalstate.h"
#include "igamesystem.h"
#include "tier1/intern_pool.h"

struct globalentity_t
{
	DECLARE_SIMPLE_DATADESC();

	// Views into CGlobalState::m_nameList.
	std::string_view	name;
	std::string_view	levelName;
	GLOBALESTATE	state;
	int				counter;
};


class CGlobalState : public CAutoGameSystem
{
public:
	CGlobalState( char const *name ) : CAutoGameSystem( name ), m_disableStateUpdates(false)
	{
	}

	// IGameSystem
	virtual void LevelShutdownPreEntity() 
	{
		// don't allow state updates during shutdowns
		Assert( !m_disableStateUpdates );
		m_disableStateUpdates = true;
	}
	
	virtual void LevelShutdownPostEntity() 
	{
		Assert( m_disableStateUpdates );
		m_disableStateUpdates = false;
	}

	void EnableStateUpdates( bool bEnable )
	{
		m_disableStateUpdates = !bEnable;
	}

	void SetState( int globalIndex, GLOBALESTATE state )
	{
		if ( m_disableStateUpdates || !ks::is_valid_index( m_list, globalIndex ) )
			return;
		m_list[globalIndex].state = state;
	}

	GLOBALESTATE GetState( int globalIndex )
	{
		if ( !ks::is_valid_index( m_list, globalIndex ) )
			return GLOBAL_OFF;
		return m_list[globalIndex].state;
	}

	void SetCounter( int globalIndex, int counter )
	{
		if ( m_disableStateUpdates || !ks::is_valid_index( m_list, globalIndex ) )
			return;
		m_list[globalIndex].counter = counter;
	}

	int AddToCounter( int globalIndex, int delta )
	{
		if ( m_disableStateUpdates || !ks::is_valid_index( m_list, globalIndex ) )
			return 0;
		return ( m_list[globalIndex].counter += delta );
	}

	int GetCounter( int globalIndex )
	{
		if ( !ks::is_valid_index( m_list, globalIndex ) )
			return 0;
		return m_list[globalIndex].counter;
	}

	void SetMap( int globalIndex, string_t mapname )
	{
		if ( !ks::is_valid_index( m_list, globalIndex ) )
			return;
		m_list[globalIndex].levelName = m_nameList.intern( STRING(mapname) );
	}

	const char *GetMap( int globalIndex )
	{
		if ( !ks::is_valid_index( m_list, globalIndex ) )
			return nullptr;
		return m_list[globalIndex].levelName.data();
	}

	const char *GetName( int globalIndex )
	{
		if ( !ks::is_valid_index( m_list, globalIndex ) )
			return nullptr;
		return m_list[globalIndex].name.data();
	}

	int GetIndex( const char *pGlobalname )
	{
		std::string_view symName = m_nameList.find( pGlobalname );

		if ( symName.data() )
		{
			for ( int i = std::ssize( m_list ) - 1; i >= 0; --i )
			{
				// Same pool, so identity is equality.
				if ( m_list[i].name.data() == symName.data() )
					return i;
			}
		}

		return -1;
	}

	int AddEntity( const char *pGlobalname, const char *pMapName, GLOBALESTATE state )
	{
		globalentity_t entity;
		entity.name = m_nameList.intern( pGlobalname );
		entity.levelName = m_nameList.intern( pMapName );
		entity.state = state;

		int index = GetIndex( entity.name.data() );
		if ( index >= 0 )
			return index;
		m_list.push_back( entity ); return (int)std::ssize( m_list ) - 1;
	}

	int GetNumGlobals( void )
	{
		return std::ssize( m_list );
	}

	void			Reset( void );
	DECLARE_SIMPLE_DATADESC();

//#ifdef _DEBUG
	void			DumpGlobals( void );
//#endif

public:
	ks::intern_pool	m_nameList;
private:
	bool			m_disableStateUpdates;
	std::vector< globalentity_t > m_list;
};

static CGlobalState gGlobalState( "CGlobalState" );



void GlobalEntity_SetState( int globalIndex, GLOBALESTATE state )
{
	gGlobalState.SetState( globalIndex, state );
}

void GlobalEntity_SetCounter( int globalIndex, int counter )
{
	gGlobalState.SetCounter( globalIndex, counter );
}

int GlobalEntity_AddToCounter( int globalIndex, int delta )
{
	return gGlobalState.AddToCounter( globalIndex, delta );
}

void GlobalEntity_SetFlags( int globalIndex, int flags )
{
	// Flags are stored in counter var
	gGlobalState.SetCounter( globalIndex, flags );
}

void GlobalEntity_AddFlags( int globalIndex, int flags )
{
	// Flags are stored in counter var
	gGlobalState.SetCounter( globalIndex, gGlobalState.GetCounter( globalIndex ) | flags );
}

void GlobalEntity_RemoveFlags( int globalIndex, int flags )
{
	// Flags are stored in counter var
	gGlobalState.SetCounter( globalIndex, gGlobalState.GetCounter( globalIndex ) & ~flags );
}

void GlobalEntity_EnableStateUpdates( bool bEnable )
{
	gGlobalState.EnableStateUpdates( bEnable );
}


void GlobalEntity_SetMap( int globalIndex, string_t mapname )
{
	gGlobalState.SetMap( globalIndex, mapname );
}

int GlobalEntity_Add( const char *pGlobalname, const char *pMapName, GLOBALESTATE state )
{
	return gGlobalState.AddEntity( pGlobalname, pMapName, state );
}

int GlobalEntity_GetIndex( const char *pGlobalname )
{
	return gGlobalState.GetIndex( pGlobalname );
}

GLOBALESTATE GlobalEntity_GetState( int globalIndex )
{
	return gGlobalState.GetState( globalIndex );
}

int GlobalEntity_GetCounter( int globalIndex )
{
	return gGlobalState.GetCounter( globalIndex );
}

int GlobalEntity_GetFlags( int globalIndex )
{
	// Flags are stored in counter var
	return gGlobalState.GetCounter( globalIndex );
}

const char *GlobalEntity_GetMap( int globalIndex )
{
	return gGlobalState.GetMap( globalIndex );
}

const char *GlobalEntity_GetName( int globalIndex )
{
	return gGlobalState.GetName( globalIndex );
}

int GlobalEntity_GetNumGlobals( void )
{
	return gGlobalState.GetNumGlobals();
}

CON_COMMAND(dump_globals, "Dump all global entities/states")
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	gGlobalState.DumpGlobals();
}

// This is available all the time now on impulse 104, remove later
//#ifdef _DEBUG
void CGlobalState::DumpGlobals( void )
{
	static const char *estates[] = { "Off", "On", "Dead" };

	Msg( "-- Globals --\n" );
	for ( int i = 0; i < std::ssize( m_list ); i++ )
	{
		Msg( "%s: %s (%s) = %d\n", m_list[i].name.data(), m_list[i].levelName.data(), estates[m_list[i].state], m_list[i].counter );
	}
}
//#endif


// Global state Savedata 
IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CGlobalState )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( globalentity_t )

void CGlobalState::Reset( void )
{
	m_list.clear(); m_list.shrink_to_fit();
	m_nameList.clear();
}


//-----------------------------------------------------------------------------
// Purpose: This gets called when a level is shut down
//-----------------------------------------------------------------------------

void ResetGlobalState( void )
{
	gGlobalState.Reset();
}


void ShowServerGameTime()
{
	Msg( "Server game time: %f\n", gpGlobals->curtime );
}

CON_COMMAND(server_game_time, "Gives the game time in seconds (server's curtime)")
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	ShowServerGameTime();
}
