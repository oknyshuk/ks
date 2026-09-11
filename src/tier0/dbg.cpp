//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "tier0/platform.h"


#include "tier0/minidump.h"
#include "tier0/stacktools.h"
#include "tier0/etwprof.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "color.h"
#include "tier0/dbg.h"
#include "tier0/threadtools.h"
#include "tier0/icommandline.h"
#include "tier0/vprof.h"
#include <math.h>


#ifndef STEAM
#define PvRealloc realloc
#define PvAlloc malloc
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( ENABLE_RUNTIME_STACK_TRANSLATION )
#pragma optimize( "g", off ) //variable argument functions seem to screw up stack walking unless this optimization is disabled
// Disable this warning: dbg.cpp(479): warning C4748: /GS can not protect parameters and local variables from local buffer overrun because optimizations are disabled in function
#pragma warning( disable : 4748 )
#endif

DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_LOADING, "LOADING" );

//-----------------------------------------------------------------------------
// Stack attachment management
//-----------------------------------------------------------------------------
#if defined( ENABLE_RUNTIME_STACK_TRANSLATION )

static bool s_bCallStacksWithAllWarnings = false; //if true, attach a call stack to every SPEW_WARNING message. Warning()/DevWarning()/...
static int s_iWarningMaxCallStackLength = 5;
#define AutomaticWarningCallStackLength() (s_bCallStacksWithAllWarnings ? s_iWarningMaxCallStackLength : 0)

void _Warning_AlwaysSpewCallStack_Enable( bool bEnable )
{
	s_bCallStacksWithAllWarnings = bEnable;
}

void _Warning_AlwaysSpewCallStack_Length( int iMaxCallStackLength )
{
	s_iWarningMaxCallStackLength = iMaxCallStackLength;
}

static bool s_bCallStacksWithAllErrors = false; //if true, attach a call stack to every SPEW_ERROR message. Mostly just Error()
static int s_iErrorMaxCallStackLength = 20; //default to higher output with an error since we're quitting anyways
#define AutomaticErrorCallStackLength() (s_bCallStacksWithAllErrors ? s_iErrorMaxCallStackLength : 0)

void _Error_AlwaysSpewCallStack_Enable( bool bEnable )
{
	s_bCallStacksWithAllErrors = bEnable;
}

void _Error_AlwaysSpewCallStack_Length( int iMaxCallStackLength )
{
	s_iErrorMaxCallStackLength = iMaxCallStackLength;
}

#else //#if defined( ENABLE_RUNTIME_STACK_TRANSLATION )

#define AutomaticWarningCallStackLength() 0
#define AutomaticErrorCallStackLength() 0

void _Warning_AlwaysSpewCallStack_Enable( bool bEnable )
{
}

void _Warning_AlwaysSpewCallStack_Length( int iMaxCallStackLength )
{
}

void _Error_AlwaysSpewCallStack_Enable( bool bEnable )
{
}

void _Error_AlwaysSpewCallStack_Length( int iMaxCallStackLength )
{
}

#endif //#if defined( ENABLE_RUNTIME_STACK_TRANSLATION )

// Skip forward past the directory
static const char *SkipToFname( const tchar* pFile )
{
	if ( pFile == NULL )
		return "unknown";
	const tchar* pSlash = _tcsrchr( pFile, '\\' );
	const tchar* pSlash2 = _tcsrchr( pFile, '/' );
	if (pSlash < pSlash2) pSlash = pSlash2;
	return pSlash ? pSlash + 1: pFile;
}

void _ExitOnFatalAssert( const tchar* pFile, int line )
{
	Log_Msg( LOG_ASSERT, _T("Fatal assert failed: %s, line %d.  Application exiting.\n"), pFile, line );

	// only write out minidumps if we're not in the debugger
	if ( !Plat_IsInDebugSession() )
	{
		WriteMiniDump();
	}

	Log_Msg( LOG_DEVELOPER, _T("_ExitOnFatalAssert\n") );
	Plat_ExitProcess( EXIT_FAILURE );
}


//-----------------------------------------------------------------------------
// Templates to assist in validating pointers:
//-----------------------------------------------------------------------------
PLATFORM_INTERFACE void _AssertValidReadPtr( void* ptr, int count/* = 1*/ )
{
	Assert( !count || ptr );
}

PLATFORM_INTERFACE void _AssertValidWritePtr( void* ptr, int count/* = 1*/ )
{
	Assert( !count || ptr );
}

PLATFORM_INTERFACE void _AssertValidReadWritePtr( void* ptr, int count/* = 1*/ )
{
	Assert( !count || ptr );
}

PLATFORM_INTERFACE void _AssertValidStringPtr( const tchar* ptr, int maxchar/* = 0xFFFFFF */ )
{
	Assert( ptr );
}

PLATFORM_INTERFACE void AssertValidWStringPtr( const wchar_t* ptr, int maxchar/* = 0xFFFFFF */ )
{
	Assert( ptr );
}

void AppendCallStackToLogMessage( tchar *formattedMessage, int iMessageLength, int iAppendCallStackLength )
{
#if defined( ENABLE_RUNTIME_STACK_TRANSLATION )
#	if defined( TCHAR_IS_CHAR ) //I'm horrible with unicode and I don't plan on testing this with wide characters just yet
		if( iAppendCallStackLength > 0 )
		{
			int iExistingMessageLength = (int)strlen( formattedMessage ); //no V_strlen in tier 0, plus we're only compiling this for windows and 360. Seems safe
			formattedMessage += iExistingMessageLength;
			iMessageLength -= iExistingMessageLength;

			if( iMessageLength <= 32 )
				return; //no room for anything useful

			//append directly to the spew message
			if( (iExistingMessageLength > 0) && (formattedMessage[-1] == '\n') )
			{
				--formattedMessage;
				++iMessageLength;
			}

			//append preface
			int iAppendedLength = _snprintf( formattedMessage, iMessageLength, _T("\nCall Stack:\n\t") );
							
			void **CallStackBuffer = (void **)stackalloc( iAppendCallStackLength * sizeof( void * ) );
			int iCount = GetCallStack( CallStackBuffer, iAppendCallStackLength, 2 );
			if( TranslateStackInfo( CallStackBuffer, iCount, formattedMessage + iAppendedLength, iMessageLength - iAppendedLength, _T("\n\t") ) == 0 )
			{
				//failure
				formattedMessage[0] = '\0'; //this is pointing at where we wrote "\nCall Stack:\n\t"
			}
			else
			{
				iAppendedLength += (int)strlen( formattedMessage + iAppendedLength ); //no V_strlen in tier 0, plus we're only compiling this for windows and 360. Seems safe

				if( iAppendedLength < iMessageLength )
				{
					formattedMessage[iAppendedLength] = '\n'; //Add another newline.
					++iAppendedLength;

					formattedMessage[iAppendedLength] = '\0';
				}
			}
		}
#	else
		AssertMsg( false, "Fixme" );
#	endif
#endif
}

// Forward declare for internal use only.
CLoggingSystem *GetGlobalLoggingSystem();

#define Log_LegacyHelperColor_Stack( Channel, Severity, Color, MessageFormat, AppendCallStackLength ) \
	do \
{ \
	CLoggingSystem *pLoggingSystem = GetGlobalLoggingSystem(); \
	if ( pLoggingSystem->IsChannelEnabled( Channel, Severity ) ) \
{ \
	tchar formattedMessage[MAX_LOGGING_MESSAGE_LENGTH]; \
	va_list args; \
	va_start( args, MessageFormat ); \
	Tier0Internal_vsntprintf( formattedMessage, MAX_LOGGING_MESSAGE_LENGTH, MessageFormat, args ); \
	va_end( args ); \
	AppendCallStackToLogMessage( formattedMessage, MAX_LOGGING_MESSAGE_LENGTH, AppendCallStackLength ); \
	pLoggingSystem->LogDirect( Channel, Severity, Color, formattedMessage ); \
} \
} while( 0 )

#define Log_LegacyHelperColor( Channel, Severity, Color, MessageFormat ) Log_LegacyHelperColor_Stack( Channel, Severity, Color, MessageFormat, 0 )

#define Log_LegacyHelper_Stack( Channel, Severity, MessageFormat, AppendCallStackLength ) Log_LegacyHelperColor_Stack( Channel, Severity, pLoggingSystem->GetChannelColor( Channel ), MessageFormat, AppendCallStackLength )
#define Log_LegacyHelper( Channel, Severity, MessageFormat ) Log_LegacyHelperColor( Channel, Severity, pLoggingSystem->GetChannelColor( Channel ), MessageFormat )

#if !defined( DBGFLAG_STRINGS_STRIP )

void Msg( const tchar* pMsgFormat, ... )
{
	Log_LegacyHelper( LOG_GENERAL, LS_MESSAGE, pMsgFormat );
}

void Warning( const tchar *pMsgFormat, ... )
{
	Log_LegacyHelper_Stack( LOG_GENERAL, LS_WARNING, pMsgFormat, AutomaticWarningCallStackLength() );
}

void Warning_SpewCallStack( int iMaxCallStackLength, const tchar *pMsgFormat, ... )
{
	Log_LegacyHelper_Stack( LOG_GENERAL, LS_WARNING, pMsgFormat, iMaxCallStackLength );
}

#endif // !DBGFLAG_STRINGS_STRIP

void Error( const tchar *pMsgFormat, ... )
{
#if !defined( DBGFLAG_STRINGS_STRIP )
	Log_LegacyHelper_Stack( LOG_GENERAL, LS_ERROR, pMsgFormat, AutomaticErrorCallStackLength() );
	// Many places that call Error assume that execution will not continue afterwards so it
	// is important to exit here. The function prototype promises that this will happen.
	Plat_ExitProcess( 100 );
#endif
}

void Error_SpewCallStack( int iMaxCallStackLength, const tchar *pMsgFormat, ... )
{
#if !defined( DBGFLAG_STRINGS_STRIP )
	Log_LegacyHelper_Stack( LOG_GENERAL, LS_ERROR, pMsgFormat, iMaxCallStackLength );
	// Many places that call Error_SpewCallStack assume that execution will not continue afterwards so it
	// is important to exit here. The function prototype promises that this will happen.
	Plat_ExitProcess( 100 );
#endif
}

#if !defined( DBGFLAG_STRINGS_STRIP )

//-----------------------------------------------------------------------------
// A couple of super-common dynamic spew messages, here for convenience 
// These looked at the "developer" group, print if it's level 1 or higher 
//-----------------------------------------------------------------------------
void DevMsg( int level, const tchar* pMsgFormat, ... )
{
	LoggingChannelID_t channel = level >= 2 ? LOG_DEVELOPER_VERBOSE : LOG_DEVELOPER;
	Log_LegacyHelper( channel, LS_MESSAGE, pMsgFormat );
}


void DevWarning( int level, const tchar *pMsgFormat, ... )
{
	LoggingChannelID_t channel = level >= 2 ? LOG_DEVELOPER_VERBOSE : LOG_DEVELOPER;
	Log_LegacyHelper( channel, LS_WARNING, pMsgFormat );
}

void DevMsg( const tchar *pMsgFormat, ... )
{
	Log_LegacyHelper( LOG_DEVELOPER, LS_MESSAGE, pMsgFormat );
}

void DevWarning( const tchar *pMsgFormat, ... )
{
	Log_LegacyHelper( LOG_DEVELOPER, LS_WARNING, pMsgFormat );
}

void ConColorMsg( const Color& clr, const tchar* pMsgFormat, ... )
{
	Log_LegacyHelperColor( LOG_CONSOLE, LS_MESSAGE, clr, pMsgFormat );
}

void ConMsg( const tchar *pMsgFormat, ... )
{
	Log_LegacyHelper( LOG_CONSOLE, LS_MESSAGE, pMsgFormat );
}

void ConDMsg( const tchar *pMsgFormat, ... )
{
	Log_LegacyHelper( LOG_DEVELOPER_CONSOLE, LS_MESSAGE, pMsgFormat );
}

#endif // !DBGFLAG_STRINGS_STRIP

// If we don't have a function from math.h, then it doesn't link certain floating-point
// functions in and printfs with %f cause runtime errors in the C libraries.
PLATFORM_INTERFACE float CrackSmokingCompiler( float a )
{
	return (float)fabs( a );
}

void* Plat_SimpleLog( const tchar* file, int line )
{
	FILE* f = _tfopen( _T("simple.log"), _T("at+") );
	_ftprintf( f, _T("%s:%i\n"), file, line );
	fclose( f );

	return NULL;
}

#if !defined( DBGFLAG_STRINGS_STRIP )

//-----------------------------------------------------------------------------
// Purpose: For debugging startup times, etc.
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void COM_TimestampedLog( char const *fmt, ... )
{
	static float s_LastStamp = 0.0;
	static bool s_bShouldLog = false;
	static bool s_bShouldLogToConsole = false;
	static bool s_bShouldLogToETW = false;
	static bool s_bChecked = false;
	static bool	s_bFirstWrite = false;

	if ( !s_bChecked )
	{
		s_bShouldLog = ( CommandLine()->CheckParm( "-profile" ) ) ? true : false;
		s_bShouldLogToConsole = ( CommandLine()->ParmValue( "-profile", 0.0f ) != 0.0f ) ? true : false;
		s_bShouldLogToETW = (CommandLine()->CheckParm("-etwprofile")) ? true : false;
		if ( s_bShouldLogToETW )
		{
			s_bShouldLog = true;
		}
		s_bChecked = true;
	}
	if ( !s_bShouldLog )
	{
		return;
	}

	char string[1024];
	va_list argptr;
	va_start( argptr, fmt );
	Tier0Internal_vsnprintf( string, sizeof( string ), fmt, argptr );
	va_end( argptr );

	float curStamp = Plat_FloatTime();


	// If ETW profiling is enabled then do it only.
	/*if (s_bShouldLogToETW)
	{
		ETWMark( string );
	}*/
	if ( !s_bFirstWrite )
	{
		unlink( "timestamped.log" );
		s_bFirstWrite = true;
	}

	FILE* fp = fopen( "timestamped.log", "at+" );
	fprintf( fp, "%8.4f / %8.4f:  %s\n", curStamp, curStamp - s_LastStamp, string );
	fclose( fp );

	if ( s_bShouldLogToConsole )
	{
		Msg( "%8.4f / %8.4f:  %s\n", curStamp, curStamp - s_LastStamp, string );
	}

	s_LastStamp = curStamp;
}

#endif // !DBGFLAG_STRINGS_STRIP

static AssertFailedNotifyFunc_t	s_AssertFailedNotifyFunc = NULL;

//-----------------------------------------------------------------------------
// Sets an assert failed notify handler
//-----------------------------------------------------------------------------
void SetAssertFailedNotifyFunc( AssertFailedNotifyFunc_t func )
{
	s_AssertFailedNotifyFunc = func;
}


//-----------------------------------------------------------------------------
// Calls the assert failed notify handler if one has been set
//-----------------------------------------------------------------------------
void CallAssertFailedNotifyFunc( const char *pchFile, int nLine, const char *pchMessage )
{
	if ( s_AssertFailedNotifyFunc )
		s_AssertFailedNotifyFunc( pchFile, nLine, pchMessage );
}




//-----------------------------------------------------------------------------
// The body the assert macros forward to. See the note in dbg.h for why this is a function rather
// than the macro it used to be.
//-----------------------------------------------------------------------------
void AssertImpl( const tchar *pMsg, bool bFatal, const std::source_location &loc )
{
	LoggingResponse_t ret = Log_Assert( "%s (%d) : %s\n", loc.file_name(), (int)loc.line(),
	                                    static_cast<const char *>( pMsg ) );
	CallAssertFailedNotifyFunc( loc.file_name(), (int)loc.line(), pMsg );

	if ( ret == LR_DEBUGGER )
	{
		if ( ShouldUseNewAssertDialog() )
		{
			if ( DbgFlagMacro_DoNewAssertDialog( loc.file_name(), (int)loc.line(), pMsg ) )
				DebuggerBreak();
		}
		if ( bFatal )
			DbgFlagMacro_ExitOnFatalAssert( loc.file_name(), (int)loc.line() );
	}
}
