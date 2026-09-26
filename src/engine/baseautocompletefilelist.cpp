//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "tier0/platform.h"
#include "qlimits.h"
#include "sys.h"
#include "baseautocompletefilelist.h"
#include "tier1/intern_pool.h"

//-----------------------------------------------------------------------------
// Purpose: Fills in a list of commands based on specified subdirectory and extension into the format:
//  commandname subdir/filename.ext
//  commandname subdir/filename2.ext
// Returns number of files in list for autocompletion
//-----------------------------------------------------------------------------
int CBaseAutoCompleteFileList::AutoCompletionFunc( char const *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	char const *cmdname = m_pszCommandName;

	char *substring = (char *)partial;
	if ( Q_strstr( partial, cmdname ) )
	{
		substring = (char *)partial + strlen( cmdname ) + 1;
	}

	// Search the directory structure.
	char searchpath[MAX_QPATH];
	if ( m_pszSubDir && m_pszSubDir[0] && Q_strcasecmp( m_pszSubDir, "NULL" ) )
	{
		Q_snprintf(searchpath,sizeof(searchpath),"%s/*.%s", m_pszSubDir, m_pszExtension );
	}
	else
	{
		Q_snprintf(searchpath,sizeof(searchpath),"*.%s", m_pszExtension );
	}

	ks::intern_pool entries( true );
	std::vector< std::string_view > symbols;

	char const *findfn = Sys_FindFirst( searchpath, nullptr, 0 );
	while ( findfn )
	{
		char sz[ MAX_QPATH ];
		Q_snprintf( sz, sizeof( sz ), "%s", findfn );

		bool add = false;
		// Insert into lookup
		if ( substring[0] )
		{
			if ( !Q_strncasecmp( findfn, substring, strlen( substring ) ) )
			{
				add = true;
			}
		}
		else
		{
			add = true;
		}

		if ( add )
		{
			if ( !entries.contains( findfn ) )
			{
				symbols.push_back( entries.intern( findfn ) );
			}
		}

		findfn = Sys_FindNext( nullptr, 0 );

		// Too many
		if ( std::ssize( symbols ) >= COMMAND_COMPLETION_MAXITEMS )
			break;
	}

	Sys_FindClose();

	for ( int i = 0; i < std::ssize( symbols ); i++ )
	{
		char const *filename = symbols[ i ].data();

		Q_snprintf( commands[ i ], sizeof( commands[ i ] ), "%s %s", cmdname, filename );
		// Remove .dem
		commands[ i ][ strlen( commands[ i ] ) - 4 ] = 0;
	}

	return std::ssize( symbols );
}
