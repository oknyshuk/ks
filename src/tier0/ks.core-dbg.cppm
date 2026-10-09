// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.core:dbg;

import std;
import ks.platform;
import :prelude;
import :color;
import :logging;

export extern "C++" {

class Color;

//-----------------------------------------------------------------------------
// Assertions
//
// Assertions are C++26 contract assertions:
//
//   contract_assert( f == 5 );
//   contract_assert( pEntity && "spawned without an owner" );
//
// What happens when one fails is decided per build by -fcontract-evaluation-semantic (see the
// top-level CMakeLists.txt), not per call site: `ignore` skips the check entirely -- the
// predicate is still compiled, so it cannot rot the way a macro that expanded to ((void)0) did --
// and `observe` evaluates it and reports a failure through handle_contract_violation() in
// dbg.cpp, which goes down the same path the old Assert() did: the LOG_ASSERT channel, the
// assert-failed notify callback, and the assert dialog under a debugger.
//
// A predicate sees the enclosing function's locals, parameters and *this as const, so it can
// only call const member functions on them. That is deliberate in the language -- a check must
// not change what it checks -- and the fix for a predicate that trips it is to make the accessor
// const, not to cast it away.
//
// To break into the debugger unconditionally, use DebuggerBreak() (ks.core).
//-----------------------------------------------------------------------------

void _ExitOnFatalAssert( const tchar* pFile, int line );

bool ShouldUseNewAssertDialog();

bool SetupWin32ConsoleIO();

// Returns true if they want to break in the debugger.
bool DoNewAssertDialog( const tchar *pFile, int line, const tchar *pExpression );

// Allows the assert dialogs to be turned off from code
bool AreAllAssertsDisabled();
void SetAllAssertsDisabled( bool bAssertsEnabled );

bool IsAssertDialogDisabled();
void SetAssertDialogDisabled( bool bAssertDialogDisabled );

// Provides a callback that is called on asserts regardless of spew levels
typedef void (*AssertFailedNotifyFunc_t)( const char *pchFile, int nLine, const char *pchMessage );
void SetAssertFailedNotifyFunc( AssertFailedNotifyFunc_t func );
void CallAssertFailedNotifyFunc( const char *pchFile, int nLine, const char *pchMessage );

// A front end may register a modal prompt for failed assertions. It shows `text` with the given
// button labels and returns the index chosen, or -1 if it could not be shown. tier0 stays unaware
// of how the prompt is drawn.
typedef int (*AssertDialogFunc_t)( const char *pszTitle, const char *pszText, const char *const *pLabels, int nLabels, int nDefault, int nEscape );
void SetAssertDialogHandler( AssertDialogFunc_t func );

// Reports a failed assertion at loc. handle_contract_violation() is the caller that matters; it is
// exported for the few places that detect a failure some other way and want it reported the same.
void AssertImpl( const tchar *pMsg, bool bFatal,
                                    const std::source_location &loc = std::source_location::current() );

// Handy define for inserting clickable messages into the build output.
// Use like this:
// #pragma MESSAGE("Some message")

//////////////////////////////////////////////////////////////////////////
// Legacy Logging System
//////////////////////////////////////////////////////////////////////////

// Channels which map the legacy logging system to the new system.

// Channel for all default Msg/Warning/Error commands.
extern LoggingChannelID_t LOG_GENERAL;

// Channel for all asserts.
extern LoggingChannelID_t LOG_ASSERT;;
// Channel for all ConMsg and ConColorMsg commands.
extern LoggingChannelID_t LOG_CONSOLE;;
// Channel for all DevMsg and DevWarning commands with level < 2.
extern LoggingChannelID_t LOG_DEVELOPER;;
// Channel for ConDMsg commands.
extern LoggingChannelID_t LOG_DEVELOPER_CONSOLE;;
// Channel for all DevMsg and DevWarning commands with level >= 2.
extern LoggingChannelID_t LOG_DEVELOPER_VERBOSE;;

// Legacy logging functions

// These functions do not return.
void Error( const tchar *pMsg, ... ) [[gnu::format(printf, 1, 2)]];
void Error_SpewCallStack( int iMaxCallStackLength, const tchar *pMsg, ... ) [[gnu::format(printf, 2, 3)]];
template <typename... Args>
[[noreturn]] inline void Plat_FatalError( Args &&...args )
{
	Log_Error( LOG_GENERAL, static_cast<Args &&>( args )... );
	Plat_ExitProcess( 1 );
}

void Msg( const tchar* pMsg, ... ) [[gnu::format(printf, 1, 2)]];
void Warning( const tchar *pMsg, ... ) [[gnu::format(printf, 1, 2)]];
void Warning_SpewCallStack( int iMaxCallStackLength, const tchar *pMsg, ... ) [[gnu::format(printf, 2, 3)]];

void DevMsg( int level, const tchar* pMsg, ... ) [[gnu::format(printf, 2, 3)]];
void DevWarning( int level, const tchar *pMsg, ... ) [[gnu::format(printf, 2, 3)]];

void DevMsg( const tchar* pMsg, ... ) [[gnu::format(printf, 1, 2)]];
void DevWarning( const tchar *pMsg, ... ) [[gnu::format(printf, 1, 2)]];

void ConColorMsg( const Color& clr, const tchar* pMsg, ... ) [[gnu::format(printf, 2, 3)]];
void ConMsg( const tchar* pMsg, ... ) [[gnu::format(printf, 1, 2)]];

void ConDMsg( const tchar* pMsg, ... ) [[gnu::format(printf, 1, 2)]];

void COM_TimestampedLog( char const *fmt, ... ) [[gnu::format(printf, 1, 2)]];

// @TODO: these callstack spew functions are currently disabled in the new logging system.  Need to add support for these if desired.
void _Warning_AlwaysSpewCallStack_Enable( bool bEnable );
void _Warning_AlwaysSpewCallStack_Length( int iMaxCallStackLength );

void _Error_AlwaysSpewCallStack_Enable( bool bEnable );
void _Error_AlwaysSpewCallStack_Length( int iMaxCallStackLength );

//-----------------------------------------------------------------------------
// Templates to assist in validating pointers:

// Have to use these stubs so we don't have to include windows.h here.
void _AssertValidReadPtr( void* ptr, int count = 1 );
void _AssertValidWritePtr( void* ptr, int count = 1 );
void _AssertValidReadWritePtr( void* ptr, int count = 1 );
void _AssertValidStringPtr( const tchar* ptr, int maxchar );

inline void AssertValidStringPtr( const tchar* ptr, int maxchar = 0xFFFFFF ) 		 {	}
template<class T> inline void AssertValidReadPtr( T* ptr, int count = 1 )		     {  }
template<class T> inline void AssertValidWritePtr( T* ptr, int count = 1 )		     {  }
template<class T> inline void AssertValidReadWritePtr( T* ptr, int count = 1 )	     {  }

//-----------------------------------------------------------------------------
//
// Purpose: Embed debug info in each file.
//

//-----------------------------------------------------------------------------
//
// Purpose: Wrap around a variable to create a simple place to put a breakpoint
//

}
