//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======//
//
//  Purpose: Plays a movie and reports on finish
//
//===========================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CLogicPlayMovie : public CLogicalEntity
{
public:
	DECLARE_CLASS( CLogicPlayMovie, CLogicalEntity );
	DECLARE_DATADESC();

	CLogicPlayMovie( void ) { }
	~CLogicPlayMovie( void ) { }

	virtual void Precache( void );
	virtual void Spawn( void );
	
private:

	[[= ks::reflect::Input{ .name = "PlayMovie", .type = FIELD_VOID } ]] void		InputPlayMovie( inputdata_t &data );
	[[= ks::reflect::Input{ .name = "PlayMovieForAllPlayers", .type = FIELD_VOID } ]] void		InputPlayMovieForAllPlayers( inputdata_t &data );
	[[= ks::reflect::Input{ .name = "PlayLevelTransitionMovie", .type = FIELD_VOID } ]] void		InputPlayLevelTransitionMovie( inputdata_t &data );
	[[= ks::reflect::Input{ .name = "FadeAllMovies", .type = FIELD_VOID } ]] void		InputFadeAllMovies( inputdata_t &data );
	[[= ks::reflect::Input{ .name = "__MovieFinished", .type = FIELD_VOID } ]] void		InputMovieFinished( inputdata_t &data );

	[[= ks::reflect::Key{ .name = "MovieFilename" } ]] string_t	m_strMovieFilename;
	[[= ks::reflect::Key{ .name = "allowskip" } ]] bool		m_bAllowUserSkip;
	[[= ks::reflect::Key{ .name = "loopvideo" } ]] bool		m_bLoopVideo;
	[[= ks::reflect::Key{ .name = "fadeintime" } ]] float		m_bFadeInTime;

	[[= ks::reflect::Key{ .name = "OnPlaybackFinished" } ]] COutputEvent	m_OnPlaybackFinished;
};

LINK_ENTITY_TO_CLASS( logic_playmovie, CLogicPlayMovie );

IMPLEMENT_REFLECT_DATAMAP( CLogicPlayMovie )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicPlayMovie::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicPlayMovie::Spawn( void )
{
}

static bool Helper_BIsValidMovieFileString( char const *szFileString )
{
	if ( !szFileString || !*szFileString )
		return false;
	char const *szFileStringStart = szFileString;
	while ( *szFileString )
	{
		if ( !V_isalnum( *szFileString ) ) switch( *szFileString )
		{
		case '-':
		case '_':
		case '/':
		case '.':
			++ szFileString;
			continue;
		default:
			DevWarning( "logic_playmovie has invalid movie filename char '%c' in \"%s\"\n", *szFileString, szFileStringStart );
			return false;
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicPlayMovie::InputPlayMovie( inputdata_t &data )
{
	if ( !Helper_BIsValidMovieFileString( STRING(m_strMovieFilename) ) )
		return;

	const char *szVideoCommand = ( m_bAllowUserSkip ) ? "playvideo_exitcommand" : "playvideo_exitcommand_nointerrupt";
	// Build the hacked string

	char szClientCmd[256];
	Q_snprintf( szClientCmd, sizeof(szClientCmd), 
		"%s %s end_movie %s\n", 
		szVideoCommand,
		STRING(m_strMovieFilename), 
		GetEntityNameAsCStr() );

	// Send it on
	engine->ServerCommand( szClientCmd );
}

//-----------------------------------------------------------------------------
// Purpose: Plays a bink movie for all connected players
//-----------------------------------------------------------------------------
void CLogicPlayMovie::InputPlayMovieForAllPlayers( inputdata_t &data )
{
	if ( !Helper_BIsValidMovieFileString( STRING( m_strMovieFilename ) ) )
		return;

	const char *szVideoCommand = ( m_bAllowUserSkip ) ? "playvideo_exitcommand" : "playvideo_exitcommand_nointerrupt";
	// Build the hacked string

	char szClientCmd[256];
	Q_snprintf( szClientCmd, sizeof(szClientCmd), 
		"%s %s end_movie %s\n", 
		szVideoCommand,
		STRING(m_strMovieFilename), 
		GetEntityNameAsCStr() );

	bool bSplit = false;

	// Find out if we are in splitscreen first
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pToPlayer = UTIL_PlayerByIndex( i );
		if ( pToPlayer )
		{
			if ( engine->IsSplitScreenPlayer( pToPlayer->entindex() ) )
			{
				bSplit = true;
				break;
			}
		}
	}

	// Send it on
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pToPlayer = UTIL_PlayerByIndex( i );
		if ( pToPlayer )
		{
			engine->ClientCommand( pToPlayer->edict(), "%s", szClientCmd );
			// if we are in split screen, just play one movie
			if ( bSplit )
				return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicPlayMovie::InputPlayLevelTransitionMovie( inputdata_t &data )
{
	if ( !Helper_BIsValidMovieFileString( STRING( m_strMovieFilename ) ) )
		return;

	const char *szVideoCommand = "playvideo_end_level_transition";

	m_bFadeInTime = MAX( 0.1f, m_bFadeInTime );

	char szClientCmd[256];
	Q_snprintf( szClientCmd, sizeof(szClientCmd), 
		"%s %s %f\n", 
		szVideoCommand,
		STRING(m_strMovieFilename), 
		m_bFadeInTime );

	// Send it on
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pToPlayer = UTIL_PlayerByIndex( i );
		if ( pToPlayer )
		{
			engine->ClientCommand( pToPlayer->edict(), "%s", szClientCmd );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicPlayMovie::InputFadeAllMovies( inputdata_t &data )
{
	// TODO: HAX FOR PAX
	const char *szVideoCommand = "stopvideos_fadeout";

	char szClientCmd[256];
	Q_snprintf( szClientCmd, sizeof(szClientCmd), 
		"%s %f\n", 
		szVideoCommand,
		1.0f );

	// Send it on
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pToPlayer = UTIL_PlayerByIndex( i );
		if ( pToPlayer )
		{
			engine->ClientCommand( pToPlayer->edict(), "%s", szClientCmd );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicPlayMovie::InputMovieFinished( inputdata_t &data )
{
	// Simply fire our output
	m_OnPlaybackFinished.FireOutput( this, this );
}
