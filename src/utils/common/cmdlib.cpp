//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// -----------------------
// cmdlib.c
// -----------------------
#include "tier0/platform.h"
#include "cmdlib.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "tier1/strtools.h"
#include "utlvector.h"
#include "filesystem_helpers.h"
#include "utllinkedlist.h"
#include "tier0/icommandline.h"
#include "keyvalues.h"
#include "filesystem_tools.h"

#if defined( MPI )

	#include "vmpi.h"
	#include "vmpi_tools_shared.h"

#endif




// set these before calling CheckParm
int myargc;
char **myargv;

int newdirs = 0;

char		com_token[ 1024 ];

qboolean	archive;
char		archivedir[ 1024 ];



CUtlLinkedList<CleanupFn, unsigned short> g_CleanupFunctions;

bool g_bStopOnExit = false;





/*
===================
ExpandWildcards

Mimic unix command line expansion
===================
*/
#define	MAX_EX_ARGC	1024
int		ex_argc;
char	*ex_argv[ MAX_EX_ARGC ];
void ExpandWildcards( int *argc, char ***argv )
{
}


// only printf if in verbose mode
qboolean verbose = false;
void qprintf( char *format, ... )
{
	if ( !verbose )
		return;

	va_list argptr;
	va_start( argptr, format );

	char str[ 2048 ];
	V_vsprintf_safe( str, format, argptr );

#if defined( CMDLIB_NODBGLIB )
	printf( "%s", str );
#else
	Msg( "%s", str );
#endif

	va_end( argptr );
}


// ---------------------------------------------------------------------------------------------------- //
// Helpers.
// ---------------------------------------------------------------------------------------------------- //

static void CmdLib_getwd( char *out, int outSize )
{
	getwd( out );
	strcat( out, "/" );
	Q_FixSlashes( out );
}

char *ExpandArg( char *path )
{
	static char full[ 1024 ];

	if ( path[ 0 ] != '/' && path[ 0 ] != '\\' && path[ 1 ] != ':' )
	{
		CmdLib_getwd ( full, sizeof( full ) );
		V_strcat_safe( full, path, COPY_ALL_CHARACTERS );
	}
	else
	{
		V_strcpy_safe( full, path );
	}
	return full;
}


char *ExpandPath (char *path)
{
	static char full[ 1024 ];
	if ( path[ 0 ] == '/' || path[ 0 ] == '\\' || path[ 1 ] == ':')
		return path;
	V_sprintf_safe( full, "%s%s", qdir, path );
	return full;
}



char *copystring(const char *s)
{
	char	*b;
	b = ( char * )malloc( strlen( s ) + 1 );
	V_strcpy( b, s );
	return b;
}

void Q_mkdir( char *path )
{
	if ( mkdir( path, 0777 ) != -1)
		return;
//	if (errno != EEXIST)
	Error( "mkdir failed %s\n", path );
}

void CmdLib_InitFileSystem( const char *pFilename, int maxMemoryUsage )
{
	FileSystem_Init( pFilename, maxMemoryUsage );
	if ( !g_pFileSystem )
	{
		Error( "CmdLib_InitFileSystem failed." );
	}
}

void CmdLib_TermFileSystem()
{
	FileSystem_Term();
}

CreateInterfaceFn CmdLib_GetFileSystemFactory()
{
	return FileSystem_GetFactory();
}


/*
============
FileTime

returns -1 if not present
============
*/
int	FileTime( char *path )
{
	struct	stat	buf;
	
	if ( stat( path, &buf ) == -1 )
		return -1;
	
	return buf.st_mtime;
}



/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse( char *data )
{
	return ( char* )ParseFile( data, com_token, nullptr );
}


/*
=============================================================================

						MISC FUNCTIONS

=============================================================================
*/


/*
=================
CheckParm

Checks for the given parameter in the program's command line arguments
Returns the argument number (1 to argc-1) or 0 if not present
=================
*/
int CheckParm( char *check )
{
	int i;

	for ( i = 1; i < myargc; i++ )
	{
		if ( !Q_strcasecmp( check, myargv[ i ] ) )
			return i;
	}

	return 0;
}



/*
================
Q_filelength
================
*/
int Q_filelength( FileHandle_t f )
{
	return g_pFileSystem->Size( f );
}


FileHandle_t SafeOpenWrite( const char *filename )
{
	FileHandle_t f = g_pFileSystem->Open( filename, "wb" );

	if ( !f )
	{
		//Error( "Error opening %s: %s", filename, strerror( errno ) );
		// BUGBUG: No way to get equivalent of errno from IFileSystem!
		Error( "Error opening %s! (Check for write enable)\n", filename );
	}

	return f;
}

#define MAX_CMDLIB_BASE_PATHS 20
static char g_pBasePaths[ MAX_CMDLIB_BASE_PATHS ][ MAX_PATH ];
static int g_NumBasePaths = 0;

void CmdLib_AddBasePath( const char *pPath )			 
{
	//printf( "CmdLib_AddBasePath( \"%s\" )\n", pPath );
	if( g_NumBasePaths < MAX_CMDLIB_BASE_PATHS )
	{
		V_strcpy_safe( g_pBasePaths[ g_NumBasePaths ], pPath );
		Q_FixSlashes( g_pBasePaths[ g_NumBasePaths ] );
		g_NumBasePaths++;
	}
	else
	{
		Assert( 0 );
	}
}


void CmdLib_AddNewSearchPath( const char *pPath )
{
	static int g_nAdditionalDirectoryCount = 0;
	static char s_addedRelativeDirs[ MAX_CMDLIB_BASE_PATHS ][ MAX_PATH ];
   	static int s_originalNumBasePaths = 0;
    static char s_originalBasePaths[ MAX_CMDLIB_BASE_PATHS ][ MAX_PATH ];

	if ( g_nAdditionalDirectoryCount == 0 ) // first call: 
	{
		// remember all original paths
	   s_originalNumBasePaths = g_NumBasePaths;
	   for ( int nOriginalBasePath = 0; nOriginalBasePath < s_originalNumBasePaths; nOriginalBasePath++ )
	   {
			V_strcpy_safe( s_originalBasePaths[nOriginalBasePath], g_pBasePaths[nOriginalBasePath] );
	   }
	}
    
	if( g_nAdditionalDirectoryCount < MAX_CMDLIB_BASE_PATHS )
	{
		V_strcpy_safe( s_addedRelativeDirs[g_nAdditionalDirectoryCount], pPath );
		Q_FixSlashes( s_addedRelativeDirs[g_nAdditionalDirectoryCount] );
		g_nAdditionalDirectoryCount++;
	}
	else
	{
		Assert( 0 );
	}
	
	//update the original base paths with the new number
	// we'll produce a cross-product of the set of original paths and the new relative paths
	g_NumBasePaths = s_originalNumBasePaths + ( s_originalNumBasePaths * g_nAdditionalDirectoryCount );
	if ( g_NumBasePaths > MAX_CMDLIB_BASE_PATHS )
	{
		Error( "You have too many search paths, let SteveK know about this\n");
	}

	//make an array of all the new search directories and copy it back into g_pBasePaths
	int nGeneratedPaths = 0;
	for ( int nOriginalBasePath = 0; nOriginalBasePath <  s_originalNumBasePaths; nOriginalBasePath++ )
	{
		for ( int nAdditionalDir = 0; nAdditionalDir < g_nAdditionalDirectoryCount; nAdditionalDir++ )
		{
			//add in the new search dirs here
			V_strcpy_safe( g_pBasePaths[ nGeneratedPaths ], s_originalBasePaths[ nOriginalBasePath ] ); 
			V_strcat_safe( g_pBasePaths[ nGeneratedPaths ], s_addedRelativeDirs[ nAdditionalDir ], sizeof( s_addedRelativeDirs[ nAdditionalDir ] ) );
			nGeneratedPaths++;
		}
		//we still need the original base path, but insert it after the newly added ones
		V_strcpy_safe( g_pBasePaths[ nGeneratedPaths ], s_originalBasePaths[ nOriginalBasePath ] );
		nGeneratedPaths++;
	}
}


bool CmdLib_HasBasePath( const char *pFileName_, int &pathLength )
{
	char *pFileName = ( char * )stackalloc( strlen( pFileName_ ) + 1 );
	V_strcpy( pFileName, pFileName_ );
	Q_FixSlashes( pFileName );
	pathLength = 0;
	for( int i = 0; i < g_NumBasePaths; i++ )
	{
		// see if we can rip the base off of the filename.
		if( Q_strncasecmp( g_pBasePaths[ i ], pFileName, Q_strlen( g_pBasePaths[ i ] ) ) == 0 )
		{
			pathLength = strlen( g_pBasePaths[ i ] );
			return true;
		}
	}
	return false;
}

int CmdLib_GetNumBasePaths( void )
{
	return g_NumBasePaths;
}

const char *CmdLib_GetBasePath( int i )
{
	Assert( i >= 0 && i < g_NumBasePaths );
	return g_pBasePaths[ i ];
}

FileHandle_t SafeOpenRead( const char *filename )
{
	int pathLength;
	FileHandle_t f = 0;
	if( CmdLib_HasBasePath( filename, pathLength ) )
	{
		filename = filename + pathLength;
		char tmp[ MAX_PATH ];
		for( int i = 0; i < g_NumBasePaths; i++ )
		{
			V_strcpy_safe( tmp, g_pBasePaths[ i ] );
			V_strcat_safe( tmp, filename );
			f = g_pFileSystem->Open( tmp, "rb" );
			if( f )
			{
				return f;
			}
		}
	
		Error( "Error opening %s\n",filename );
		return f;
	}
	else
	{
		f = g_pFileSystem->Open( filename, "rb" );
		if ( !f )
		{
			Error( "Error opening %s",filename );
		}

		return f;
	}
}

void SafeRead( FileHandle_t f, void *buffer, int count)
{
	if ( g_pFileSystem->Read( buffer, count, f ) != ( size_t )count )
	{
		Error( "File read failure" );
	}
}


void SafeWrite ( FileHandle_t f, void *buffer, int count)
{
	if ( g_pFileSystem->Write( buffer, count, f ) != ( size_t )count )
	{
		Error( "File write failure" );
	}
}


/*
==============
FileExists
==============
*/
qboolean	FileExists( const char *filename )
{
	FileHandle_t hFile = g_pFileSystem->Open( filename, "rb" );
	if ( hFile == FILESYSTEM_INVALID_HANDLE )
	{
		return false;
	}
	else
	{
		g_pFileSystem->Close( hFile );
		return true;
	}
}

/*
==============
LoadFile
==============
*/
int    LoadFile( const char *filename, void **bufferptr )
{
	int    length = 0;
	void    *buffer;

	FileHandle_t f = SafeOpenRead( filename );
	if ( FILESYSTEM_INVALID_HANDLE != f )
	{
		length = Q_filelength( f );
		buffer = malloc( length + 1 );
		( ( char * )buffer )[ length ] = 0;
		SafeRead( f, buffer, length );
		g_pFileSystem->Close( f );
		*bufferptr = buffer;
	}
	else
	{
		*bufferptr = nullptr;
	}
	return length;
}



/*
==============
SaveFile
==============
*/
void    SaveFile( const char *filename, void *buffer, int count )
{
	FileHandle_t f = SafeOpenWrite( filename );
	SafeWrite( f, buffer, count );
	g_pFileSystem->Close( f );
}

/*
====================
Extract file parts
====================
*/
// FIXME: should include the slash, otherwise
// backing to an empty path will be wrong when appending a slash



/*
==============
ParseNum / ParseHex
==============
*/
int ParseHex( char *hex )
{
	char    *str;
	int    num;

	num = 0;
	str = hex;

	while ( *str )
	{
		num <<= 4;
		if ( *str >= '0' && *str <= '9' )
		{
			num += *str - '0';
		}
		else if ( *str >= 'a' && *str <= 'f' )
		{
			num += 10 + *str - 'a';
		}
		else if ( *str >= 'A' && *str <= 'F' )
		{
			num += 10 + *str - 'A';
		}
		else
		{
			Error( "Bad hex number: %s", hex );
		}
		str++;
	}

	return num;
}


int ParseNum( char *str )
{
	if ( str[ 0 ] == '$' )
		return ParseHex( str + 1 );
	if ( str[ 0 ] == '0' && str[ 1 ] == 'x' )
		return ParseHex( str + 2 );
	return atol( str );
}

/*
============
CreatePath
============
*/
void CreatePath( char *path )
{
	char	*ofs, c;

	// strip the drive
	if ( path[ 1 ] == ':' )
		path += 2;

	for ( ofs = path + 1; *ofs; ofs++)
	{
		c = *ofs;
		if ( c == '/' || c == '\\' )
		{	// create the directory
			*ofs = 0;
			Q_mkdir( path );
			*ofs = c;
		}
	}
}

//-----------------------------------------------------------------------------
// Creates a path, path may already exist
//-----------------------------------------------------------------------------

/*
============
QCopyFile

  Used to archive source files
============
*/
void QCopyFile( char *from, char *to )
{
	void	*buffer;
	int		length;

	length = LoadFile( from, &buffer );
	CreatePath( to );
	SaveFile( to, buffer, length );
	free( buffer );
}



