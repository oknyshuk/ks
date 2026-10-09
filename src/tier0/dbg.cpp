// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core;

import std;


LoggingChannelID_t LOG_LOADING = LoggingSystem_RegisterLoggingChannel( "LOADING", nullptr );;

//-----------------------------------------------------------------------------
// Stack attachment management
//-----------------------------------------------------------------------------


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

// Skip forward past the directory
static const char *SkipToFname( const tchar* pFile )
{
	if ( pFile == nullptr )
		return "unknown";
	const tchar* pSlash = std::strrchr( pFile, '\\' );
	const tchar* pSlash2 = std::strrchr( pFile, '/' );
	if (pSlash < pSlash2) pSlash = pSlash2;
	return pSlash ? pSlash + 1: pFile;
}

void _ExitOnFatalAssert( const tchar* pFile, int line )
{
	Log_Msg( LOG_ASSERT, "Fatal assert failed: %s, line %d.  Application exiting.\n", pFile, line );

	// only write out minidumps if we're not in the debugger
	if ( !Plat_IsInDebugSession() )
	{
		WriteMiniDump();
	}

	Log_Msg( LOG_DEVELOPER, "_ExitOnFatalAssert\n" );
	Plat_ExitProcess( 1 );
}


//-----------------------------------------------------------------------------
// Templates to assist in validating pointers:
//-----------------------------------------------------------------------------
void _AssertValidReadPtr( void* ptr, int count/* = 1*/ )
{
	contract_assert( !count || ptr );
}

void _AssertValidWritePtr( void* ptr, int count/* = 1*/ )
{
	contract_assert( !count || ptr );
}

void _AssertValidReadWritePtr( void* ptr, int count/* = 1*/ )
{
	contract_assert( !count || ptr );
}

void _AssertValidStringPtr( const tchar* ptr, int maxchar/* = 0xFFFFFF */ )
{
	contract_assert( ptr );
}

void AssertValidWStringPtr( const wchar_t* ptr, int maxchar/* = 0xFFFFFF */ )
{
	contract_assert( ptr );
}

void AppendCallStackToLogMessage( tchar *formattedMessage, int iMessageLength, int iAppendCallStackLength )
{
}

// Forward declare for internal use only.
CLoggingSystem *GetGlobalLoggingSystem();

static void LogLegacy( LoggingChannelID_t channel, LoggingSeverity_t severity, const Color *pColor, int nCallStackLength, const tchar *pMsgFormat, std::va_list args )
{
	CLoggingSystem *pLoggingSystem = GetGlobalLoggingSystem();
	if ( !pLoggingSystem->IsChannelEnabled( channel, severity ) )
		return;

	tchar formattedMessage[MAX_LOGGING_MESSAGE_LENGTH];
	std::vsnprintf( formattedMessage, MAX_LOGGING_MESSAGE_LENGTH, pMsgFormat, args );
	AppendCallStackToLogMessage( formattedMessage, MAX_LOGGING_MESSAGE_LENGTH, nCallStackLength );
	pLoggingSystem->LogDirect( channel, severity, pColor ? *pColor : pLoggingSystem->GetChannelColor( channel ), formattedMessage );
}

void Msg( const tchar* pMsgFormat, ... )
{
	std::va_list args;
	__builtin_va_start( args, pMsgFormat );
	LogLegacy( LOG_GENERAL, LS_MESSAGE, nullptr, 0, pMsgFormat, args );
	__builtin_va_end( args );
}

void Warning( const tchar *pMsgFormat, ... )
{
	std::va_list args;
	__builtin_va_start( args, pMsgFormat );
	LogLegacy( LOG_GENERAL, LS_WARNING, nullptr, 0, pMsgFormat, args );
	__builtin_va_end( args );
}

void Warning_SpewCallStack( int iMaxCallStackLength, const tchar *pMsgFormat, ... )
{
	std::va_list args;
	__builtin_va_start( args, pMsgFormat );
	LogLegacy( LOG_GENERAL, LS_WARNING, nullptr, iMaxCallStackLength, pMsgFormat, args );
	__builtin_va_end( args );
}

void Error( const tchar *pMsgFormat, ... )
{
	std::va_list args;
	__builtin_va_start( args, pMsgFormat );
	LogLegacy( LOG_GENERAL, LS_ERROR, nullptr, 0, pMsgFormat, args );
	__builtin_va_end( args );
	// Many places that call Error assume that execution will not continue afterwards so it
	// is important to exit here. The function prototype promises that this will happen.
	Plat_ExitProcess( 100 );
}

void Error_SpewCallStack( int iMaxCallStackLength, const tchar *pMsgFormat, ... )
{
	std::va_list args;
	__builtin_va_start( args, pMsgFormat );
	LogLegacy( LOG_GENERAL, LS_ERROR, nullptr, iMaxCallStackLength, pMsgFormat, args );
	__builtin_va_end( args );
	// Many places that call Error_SpewCallStack assume that execution will not continue afterwards so it
	// is important to exit here. The function prototype promises that this will happen.
	Plat_ExitProcess( 100 );
}

//-----------------------------------------------------------------------------
// A couple of super-common dynamic spew messages, here for convenience 
// These looked at the "developer" group, print if it's level 1 or higher 
//-----------------------------------------------------------------------------
void DevMsg( int level, const tchar* pMsgFormat, ... )
{
	LoggingChannelID_t channel = level >= 2 ? LOG_DEVELOPER_VERBOSE : LOG_DEVELOPER;
	std::va_list args;
	__builtin_va_start( args, pMsgFormat );
	LogLegacy( channel, LS_MESSAGE, nullptr, 0, pMsgFormat, args );
	__builtin_va_end( args );
}


void DevWarning( int level, const tchar *pMsgFormat, ... )
{
	LoggingChannelID_t channel = level >= 2 ? LOG_DEVELOPER_VERBOSE : LOG_DEVELOPER;
	std::va_list args;
	__builtin_va_start( args, pMsgFormat );
	LogLegacy( channel, LS_WARNING, nullptr, 0, pMsgFormat, args );
	__builtin_va_end( args );
}

void DevMsg( const tchar *pMsgFormat, ... )
{
	std::va_list args;
	__builtin_va_start( args, pMsgFormat );
	LogLegacy( LOG_DEVELOPER, LS_MESSAGE, nullptr, 0, pMsgFormat, args );
	__builtin_va_end( args );
}

void DevWarning( const tchar *pMsgFormat, ... )
{
	std::va_list args;
	__builtin_va_start( args, pMsgFormat );
	LogLegacy( LOG_DEVELOPER, LS_WARNING, nullptr, 0, pMsgFormat, args );
	__builtin_va_end( args );
}

void ConColorMsg( const Color& clr, const tchar* pMsgFormat, ... )
{
	std::va_list args;
	__builtin_va_start( args, pMsgFormat );
	LogLegacy( LOG_CONSOLE, LS_MESSAGE, &clr, 0, pMsgFormat, args );
	__builtin_va_end( args );
}

void ConMsg( const tchar *pMsgFormat, ... )
{
	std::va_list args;
	__builtin_va_start( args, pMsgFormat );
	LogLegacy( LOG_CONSOLE, LS_MESSAGE, nullptr, 0, pMsgFormat, args );
	__builtin_va_end( args );
}

void ConDMsg( const tchar *pMsgFormat, ... )
{
	std::va_list args;
	__builtin_va_start( args, pMsgFormat );
	LogLegacy( LOG_DEVELOPER_CONSOLE, LS_MESSAGE, nullptr, 0, pMsgFormat, args );
	__builtin_va_end( args );
}

// If we don't have a function from math.h, then it doesn't link certain floating-point
// functions in and printfs with %f cause runtime errors in the C libraries.
float CrackSmokingCompiler( float a )
{
	return (float)std::fabs( a );
}

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
	std::va_list argptr;
	__builtin_va_start( argptr, fmt );
	std::vsnprintf( string, sizeof( string ), fmt, argptr );
	__builtin_va_end( argptr );

	float curStamp = Plat_FloatTime();


	// If ETW profiling is enabled then do it only.
	/*if (s_bShouldLogToETW)
	{
		ETWMark( string );
	}*/
	if ( !s_bFirstWrite )
	{
		ks::platform::remove_file( "timestamped.log", nullptr );
		s_bFirstWrite = true;
	}

	std::FILE* fp = std::fopen( "timestamped.log", "at+" );
	std::fprintf( fp, "%8.4f / %8.4f:  %s\n", curStamp, curStamp - s_LastStamp, string );
	std::fclose( fp );

	if ( s_bShouldLogToConsole )
	{
		Msg( "%8.4f / %8.4f:  %s\n", curStamp, curStamp - s_LastStamp, string );
	}

	s_LastStamp = curStamp;
}

static AssertFailedNotifyFunc_t	s_AssertFailedNotifyFunc = nullptr;

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
// Reports one failed assertion: the LOG_ASSERT channel, the notify callback, and the assert
// dialog when a debugger is attached.
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
			if ( DoNewAssertDialog( loc.file_name(), (int)loc.line(), pMsg ) )
				DebuggerBreak();
		}
		if ( bFatal )
			_ExitOnFatalAssert( loc.file_name(), (int)loc.line() );
	}
}

//-----------------------------------------------------------------------------
// The replaceable C++26 contract-violation handler, so every contract_assert, pre and post in the
// tree reports through AssertImpl. It is exported from the engine because client.so and server.so
// call it too, and they must share this one rather than fall back to libstdc++'s default, which
// would print to stderr and bypass the logging system.
//
// A violation is reported once per source location. The old Assert() reported on every failure,
// but it was compiled out in every build; turning checks on under `observe` is the first time
// many of these predicates have run, and one hot assert repeating every frame would bury the
// rest of the log.
//
// Under `enforce` the runtime terminates after this returns. AssertImpl's bFatal path is not used:
// whether a failure stops the program is the evaluation semantic's decision, made per build.
//-----------------------------------------------------------------------------
void handle_contract_violation( const std::contracts::contract_violation &violation )
{
	static constinit std::atomic_flag s_bReporting;
	static std::set<std::pair<const char *, uint32>> s_Reported;

	const std::source_location loc = violation.location();

	// The set is guarded by a spin flag rather than a mutex: this can run from any thread,
	// including during static initialization and teardown, where a mutex is not guaranteed to be
	// usable. A violation raised while another thread holds the flag is dropped, not deferred.
	if ( s_bReporting.test_and_set( std::memory_order_acquire ) )
		return;
	const bool bFirst = s_Reported.emplace( loc.file_name(), loc.line() ).second;
	s_bReporting.clear( std::memory_order_release );
	if ( !bFirst )
		return;

	char szMessage[512];
	std::snprintf( szMessage, sizeof( szMessage ), "Assertion Failed: %s", violation.comment() );
	AssertImpl( szMessage, false, loc );
}
