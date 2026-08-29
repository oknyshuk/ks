//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose:
//
// $NoKeywords: $
//===========================================================================//
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "isys.h"
#include "dedicated.h"
#include "engine_hlds_api.h"
#include "filesystem.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "idedicatedexports.h"
#include "mathlib/expressioncalculator.h"

/*
==============
InitInstance

==============
*/
bool InitInstance( )
{
	return true;
}

/*
==============
ProcessConsoleInput

==============
*/
void ProcessConsoleInput( void )
{
	char *s;

	if ( !engine )
		return;

	do
	{
		s = sys->ConsoleInput();
		if (s)
		{
			char szBuf[ 256 ];
			Q_snprintf( szBuf, sizeof( szBuf ), "%s\n", s );
			engine->AddConsoleText ( szBuf );
		}
	} while (s);
}


class CDedicatedExports : public CBaseAppSystem<IDedicatedExports>
{
public:
	virtual void Sys_Printf( const char *text )
	{
		if ( sys )
		{
			sys->Printf( "%s", text );
		}
	}

	virtual void RunServer( void );
	virtual bool IsGuiDedicatedServer();
};



void PerformCommandLineSubstitutions( int nIndex )
{
	// modify the command line, replacing all occurrences of ## with nIndex
	for( int i = 0; i < CommandLine()->ParmCount(); i++ )
	{
		char newBuf[2048];
		char const *ppParm = CommandLine()->GetParm( i );
		V_strncpy( newBuf, ppParm, sizeof( newBuf ) );
		bool bDidReplace = false;
		bool bWasExpression = false;
		for(;;)
		{
			char *pReplace = V_strstr( newBuf, "##" );
			if (! pReplace )
				break;

			pReplace[0] = '0' + ( nIndex / 10 );
			pReplace[1] = '0' + ( nIndex % 10 );
			bDidReplace = true;
			if ( ( pReplace != newBuf ) &&
				 ( strchr( "+-/*", pReplace[-1] ) ) )		// is this an expression involving "##"?
			{
				bWasExpression = true;
			}

		}
		if ( bDidReplace )
		{
			if ( bWasExpression )
			{
				sprintf( newBuf, "%d", ( int ) ( EvaluateExpression( newBuf, -1 ) ) );
			}
			printf("setparm %d %s\n", i, newBuf );
			CommandLine()->SetParm( i, newBuf );
		}

	}
}

// linux uses the implementation in sys_subproc
void CDedicatedExports::RunServer( void )
{
	// check for forking
	char const *pForkParam = CommandLine()->ParmValue( "-fork" );
	if ( pForkParam )
	{
		int nNumChildInstances = atoi( pForkParam );
		if ( nNumChildInstances >= 1 )
		{
			RunServerSubProcesses( nNumChildInstances );
		}
	}
	else
	{
		::RunServer( false );
	}
}

bool CDedicatedExports::IsGuiDedicatedServer()
{
	return false;
}

EXPOSE_SINGLE_INTERFACE( CDedicatedExports, IDedicatedExports, VENGINE_DEDICATEDEXPORTS_API_VERSION );

int Sys_GetExecutableName( char *out )
{
	extern char g_szEXEName[ 256 ];
	strcpy( out, g_szEXEName );
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Return the directory where this .exe is running from
// Output : char
//-----------------------------------------------------------------------------
const char *UTIL_GetExecutableDir( )
{
	static char	exedir[ MAX_PATH ];

	exedir[ 0 ] = 0;
	if ( !Sys_GetExecutableName(exedir) )
		return NULL;

	char *pSlash;
	char *pSlash2;
	pSlash = strrchr( exedir,'\\' );
	pSlash2 = strrchr( exedir,'/' );
	if ( pSlash2 > pSlash )
	{
		pSlash = pSlash2;
	}
	if (pSlash)
	{
		*pSlash = 0;
	}

	// Return the bin directory as the executable dir if it's not in there
	// because that's really where we're running from...
	int exeLen = strlen(exedir);
	if ( 	exedir[exeLen-4] != CORRECT_PATH_SEPARATOR ||
		exedir[exeLen-3] != 'b' ||
		exedir[exeLen-2] != 'i' ||
		exedir[exeLen-1] != 'n' )
	{
		Q_strncat( exedir, "\\bin", sizeof( exedir ), COPY_ALL_CHARACTERS );
		Q_FixSlashes( exedir );
	}

	return exedir;
}


//-----------------------------------------------------------------------------
// Purpose: Return the directory where this .exe is running from
// Output : char
//-----------------------------------------------------------------------------
const char *UTIL_GetBaseDir( void )
{
	static char	basedir[ MAX_PATH ];

	char const *pOverrideDir = CommandLine()->CheckParm( "-basedir" );
	if ( pOverrideDir )
		return pOverrideDir;

	basedir[ 0 ] = 0;
	const char *pExeDir = UTIL_GetExecutableDir( );
	if ( pExeDir )
	{
		strcpy( basedir, pExeDir );
                int dirlen = strlen( basedir );
                if ( basedir[ dirlen - 3 ] == 'b' &&
                     basedir[ dirlen - 2 ] == 'i' &&
                     basedir[ dirlen - 1 ] == 'n' )
                {
                        basedir[ dirlen - 4 ] = 0;
                }
	}

	return basedir;
}
