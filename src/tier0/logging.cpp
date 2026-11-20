// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: Logging system definitions.

import ks.core.color;
import ks.core.dbg;
import ks.core.logging;
import ks.core.platform;
import ks.core.types;
import ks.core.wchartypes;
import ks.platform;
import ks.simd;
import ks.tier1.icvar;

// First include standard libraries
import std;


//////////////////////////////////////////////////////////////////////////
// Define commonly used channels here
//////////////////////////////////////////////////////////////////////////
LoggingChannelID_t LOG_GENERAL = LoggingSystem_RegisterLoggingChannel( "General", nullptr );;

LoggingChannelID_t LOG_ASSERT = LoggingSystem_RegisterLoggingChannel( "Assert", nullptr );;

// Corresponds to ConMsg/ConWarning/etc. with a level <= 1.
// Only errors are spewed by default.
static void Register_LOG_CONSOLE_Tags();
LoggingChannelID_t LOG_CONSOLE = LoggingSystem_RegisterLoggingChannel( "Console", Register_LOG_CONSOLE_Tags, LCF_CONSOLE_ONLY, LS_ERROR );
void Register_LOG_CONSOLE_Tags()
{
LoggingSystem_AddTagToCurrentChannel( "Console" );
};

// Corresponds to DevMsg/DevWarning/etc. with a level <= 1.
// Only errors are spewed by default.
static void Register_LOG_DEVELOPER_Tags();
LoggingChannelID_t LOG_DEVELOPER = LoggingSystem_RegisterLoggingChannel( "Developer", Register_LOG_DEVELOPER_Tags, LCF_CONSOLE_ONLY, LS_ERROR );
void Register_LOG_DEVELOPER_Tags()
{
LoggingSystem_AddTagToCurrentChannel( "Developer" );
};

// Corresponds to ConMsg/ConWarning/etc. with a level >= 2.
// Only errors are spewed by default.
static void Register_LOG_DEVELOPER_CONSOLE_Tags();
LoggingChannelID_t LOG_DEVELOPER_CONSOLE = LoggingSystem_RegisterLoggingChannel( "DeveloperConsole", Register_LOG_DEVELOPER_CONSOLE_Tags, LCF_CONSOLE_ONLY, LS_ERROR );
void Register_LOG_DEVELOPER_CONSOLE_Tags()
{
LoggingSystem_AddTagToCurrentChannel( "DeveloperVerbose" );
LoggingSystem_AddTagToCurrentChannel( "Console" );
};

// Corresponds to DevMsg/DevWarning/etc, with a level >= 2.
// Only errors are spewed by default.
static void Register_LOG_DEVELOPER_VERBOSE_Tags();
LoggingChannelID_t LOG_DEVELOPER_VERBOSE = LoggingSystem_RegisterLoggingChannel( "DeveloperVerbose", Register_LOG_DEVELOPER_VERBOSE_Tags, LCF_CONSOLE_ONLY, LS_ERROR, Color( 192, 128, 192, 255 ) );
void Register_LOG_DEVELOPER_VERBOSE_Tags()
{
LoggingSystem_AddTagToCurrentChannel( "DeveloperVerbose" );
};

//////////////////////////////////////////////////////////////////////////
// Globals
//////////////////////////////////////////////////////////////////////////

// The index of the logging state used by the current thread.  This defaults to 0 across all threads, 
// which indicates that the global listener set should be used (CLoggingSystem::m_nGlobalStateIndex).
//
// NOTE:
// Because our linux TLS implementation does not support embedding a thread local
// integer in a class, the logging system must use a global thread-local integer.
// This means that we can only have one instance of CLoggingSystem, although
// we could support additional instances if we are willing to lose support for
// thread-local spew handling.
// There is no other reason why this class must be a singleton, except
// for the fact that there's no reason to have more than one in existence.
bool g_bEnforceLoggingSystemSingleton = false;

thread_local int g_nThreadLocalStateIndex;

//////////////////////////////////////////////////////////////////////////
// Implementation
//////////////////////////////////////////////////////////////////////////

CLoggingSystem *g_pGlobalLoggingSystem = nullptr;

// This function does not get inlined due to the static variable :(
CLoggingSystem *GetGlobalLoggingSystem_Internal()
{
	static CLoggingSystem globalLoggingSystem;
	g_pGlobalLoggingSystem = &globalLoggingSystem;
	return &globalLoggingSystem;
}

// This function can get inlined
CLoggingSystem *GetGlobalLoggingSystem()
{
	return ( g_pGlobalLoggingSystem == nullptr ) ? GetGlobalLoggingSystem_Internal() : g_pGlobalLoggingSystem;
}

CLoggingSystem::CLoggingSystem() : 
m_nChannelCount( 0 ), 
m_nChannelTagCount( 0 ),
m_nTagNamePoolIndex( 0 ),
m_nGlobalStateIndex( 0 )
{ 
	contract_assert( !g_bEnforceLoggingSystemSingleton );
	g_bEnforceLoggingSystemSingleton = true;
	// Due to uncertain constructor ordering (g_nThreadLocalStateIndex
	// may not be constructed yet so TLS index may not be available yet)
	// we cannot initialize the state index here without risking
	// AppVerifier errors and undefined behavior. Luckily TlsAlloc values
	// are guaranteed to be zero-initialized so we don't need to zero-init,
	// this, and in fact we can't for all threads.

	m_LoggingStates[0].m_nPreviousStackEntry = -1;

	m_LoggingStates[0].m_nListenerCount = 1;
	m_LoggingStates[0].m_RegisteredListeners[0] = &m_DefaultLoggingListener;
	m_LoggingStates[0].m_pLoggingResponse = &m_DefaultLoggingResponse;

	// Mark all other logging state blocks as unused.
	for ( int i = 1; i < MAX_LOGGING_STATE_COUNT; ++ i )
	{
		m_LoggingStates[i].m_nListenerCount = -1;
	}

	m_pStateMutex = nullptr;
}

CLoggingSystem::~CLoggingSystem()
{
	g_bEnforceLoggingSystemSingleton = false;
	delete m_pStateMutex;
}

LoggingChannelID_t CLoggingSystem::RegisterLoggingChannel( const char *pChannelName, RegisterTagsFunc registerTagsFunc, int flags, LoggingSeverity_t severity, Color spewColor )
{
	if ( m_nChannelCount >= MAX_LOGGING_CHANNEL_COUNT )
	{
		// Out of logging channels... catastrophic fail!
		Log_Error( LOG_GENERAL, "Out of logging channels.\n" );
		contract_assert( 0 );
		return INVALID_LOGGING_CHANNEL_ID;
	}
	else
	{
		// Channels can be multiply defined, in which case return the ID of the existing channel.
		for ( int i = 0; i < m_nChannelCount; ++ i )
		{
			if ( V_tier0_stricmp( m_RegisteredChannels[i].m_Name, pChannelName ) == 0 )
			{
				// OK to call the tag registration callback; duplicates will be culled away.
				// This allows multiple people to register a logging channel, and the union of all tags will be registered.
				if ( registerTagsFunc != nullptr )
				{
					registerTagsFunc();
				}

				// If a logging channel is registered multiple times, only one of the registrations should specify flags/severity/color.
				if ( m_RegisteredChannels[i].m_Flags == 0 && m_RegisteredChannels[i].m_MinimumSeverity == LS_MESSAGE && m_RegisteredChannels[i].m_SpewColor == UNSPECIFIED_LOGGING_COLOR )
				{
					m_RegisteredChannels[i].m_Flags = ( LoggingChannelFlags_t )flags;
					m_RegisteredChannels[i].m_MinimumSeverity = severity;
					m_RegisteredChannels[i].m_SpewColor = spewColor;
				}
				else
				{
					contract_assert( ( flags == 0 || flags == m_RegisteredChannels[i].m_Flags ) && "Non-zero or mismatched flags specified in logging channel re-registration!" );
					contract_assert( ( severity == LS_MESSAGE || severity == m_RegisteredChannels[i].m_MinimumSeverity ) && "Non-default or mismatched severity specified in logging channel re-registration!" );
					contract_assert( ( spewColor == UNSPECIFIED_LOGGING_COLOR || spewColor == m_RegisteredChannels[i].m_SpewColor ) && "Non-default or mismatched color specified in logging channel re-registration!" );
				}

				return m_RegisteredChannels[i].m_ID;
			}
		}

		m_RegisteredChannels[m_nChannelCount].m_ID = m_nChannelCount;
		m_RegisteredChannels[m_nChannelCount].m_Flags = ( LoggingChannelFlags_t )flags;
		m_RegisteredChannels[m_nChannelCount].m_MinimumSeverity = severity;
		m_RegisteredChannels[m_nChannelCount].m_SpewColor = spewColor;
		std::strncpy( m_RegisteredChannels[m_nChannelCount].m_Name, pChannelName, MAX_LOGGING_IDENTIFIER_LENGTH );
		
		if ( registerTagsFunc != nullptr ) 
		{
			registerTagsFunc();
		}
		return m_nChannelCount ++;
	}
}

LoggingChannelID_t CLoggingSystem::FindChannel( const char *pChannelName ) const
{
	for ( int i = 0; i < m_nChannelCount; ++ i )
	{
		if ( V_tier0_stricmp( m_RegisteredChannels[i].m_Name, pChannelName ) == 0 )
		{
			return i;
		}
	}

	return INVALID_LOGGING_CHANNEL_ID;
}

void CLoggingSystem::AddTagToCurrentChannel( const char *pTagName )
{
	// Add tags at the head of the tag-list of the most recently added channel.
	LoggingChannel_t *pChannel = &m_RegisteredChannels[m_nChannelCount];

	// First check for duplicates
	if ( pChannel->HasTag( pTagName ) )
	{
		return;
	}
	
	LoggingTag_t *pTag = AllocTag( pTagName );
	
	pTag->m_pNextTag = pChannel->m_pFirstTag;
	pChannel->m_pFirstTag = pTag;
}

void CLoggingSystem::SetChannelSpewLevel( LoggingChannelID_t channelID, LoggingSeverity_t minimumSeverity )
{
	GetChannel( channelID )->SetSpewLevel( minimumSeverity );
}

void CLoggingSystem::SetChannelSpewLevelByName( const char *pName, LoggingSeverity_t minimumSeverity )
{
	for ( int i = 0; i < m_nChannelCount; ++ i )
	{
		if ( V_tier0_stricmp( m_RegisteredChannels[i].m_Name, pName ) == 0 )
		{
			m_RegisteredChannels[i].SetSpewLevel( minimumSeverity );
		}
	}
}

void CLoggingSystem::SetChannelSpewLevelByTag( const char *pTag, LoggingSeverity_t minimumSeverity )
{
	for ( int i = 0; i < m_nChannelCount; ++ i )
	{
		if ( m_RegisteredChannels[i].HasTag( pTag ) )
		{
			m_RegisteredChannels[i].SetSpewLevel( minimumSeverity );
		}
	}
}

void CLoggingSystem::SetGlobalSpewLevel( LoggingSeverity_t minimumSeverity )
{
	for ( int i = 0; i < m_nChannelCount; ++ i )
	{
		m_RegisteredChannels[i].SetSpewLevel( minimumSeverity );
	}
}

void CLoggingSystem::PushLoggingState( bool bThreadLocal, bool bClearState )
{
	if ( !m_pStateMutex )
		m_pStateMutex = new std::recursive_mutex();

	m_pStateMutex->lock();
	
	int nNewState = FindUnusedStateIndex();
	// Ensure we're not out of state blocks.
	contract_assert( nNewState != -1 );

	int nCurrentState = bThreadLocal ? (int)g_nThreadLocalStateIndex : m_nGlobalStateIndex;

	if ( bClearState )
	{
		m_LoggingStates[nNewState].m_nListenerCount = 0;
		m_LoggingStates[nNewState].m_pLoggingResponse = &m_DefaultLoggingResponse;	
	}
	else
	{
		m_LoggingStates[nNewState] = m_LoggingStates[nCurrentState];
	}

	m_LoggingStates[nNewState].m_nPreviousStackEntry = nCurrentState;

	if ( bThreadLocal )
	{
		g_nThreadLocalStateIndex = nNewState;
	}
	else
	{
		m_nGlobalStateIndex = nNewState;
	}

	m_pStateMutex->unlock();
}

void CLoggingSystem::PopLoggingState( bool bThreadLocal )
{
	if ( !m_pStateMutex )
		m_pStateMutex = new std::recursive_mutex();

	m_pStateMutex->lock();

	int nCurrentState = bThreadLocal ? (int)g_nThreadLocalStateIndex : m_nGlobalStateIndex;
	
	// Shouldn't be less than 0 (implies error during Push()) or 0 (implies that Push() was never called)
	contract_assert( nCurrentState > 0 );

	// Mark the current state as unused.
	m_LoggingStates[nCurrentState].m_nListenerCount = -1;

	if ( bThreadLocal )
	{
		g_nThreadLocalStateIndex = m_LoggingStates[nCurrentState].m_nPreviousStackEntry;
	}
	else
	{
		m_nGlobalStateIndex = m_LoggingStates[nCurrentState].m_nPreviousStackEntry;
	}

	m_pStateMutex->unlock();
}

void CLoggingSystem::RegisterLoggingListener( ILoggingListener *pListener )
{
	if ( !m_pStateMutex )
		m_pStateMutex = new std::recursive_mutex();

	m_pStateMutex->lock();
	LoggingState_t *pState = GetCurrentState();
	if ( pState->m_nListenerCount >= std::size( pState->m_RegisteredListeners ) )
	{
		// Out of logging listener slots... catastrophic fail!
		contract_assert( 0 );
	}
	else
	{
		pState->m_RegisteredListeners[pState->m_nListenerCount] = pListener;
		++ pState->m_nListenerCount;
	}
	m_pStateMutex->unlock();
}

bool CLoggingSystem::IsListenerRegistered( ILoggingListener *pListener )
{
	if ( !m_pStateMutex )
		m_pStateMutex = new std::recursive_mutex();

	m_pStateMutex->lock();
	const LoggingState_t *pState = GetCurrentState();
	bool bFound = false;
	for ( int i = 0; i < pState->m_nListenerCount; ++ i )
	{
		if ( pState->m_RegisteredListeners[i] == pListener )
		{
			bFound = true;
			break;
		}
	}
	m_pStateMutex->unlock();
	return bFound;
}

void CLoggingSystem::ResetCurrentLoggingState()
{
	if ( !m_pStateMutex )
		m_pStateMutex = new std::recursive_mutex();

	m_pStateMutex->lock();
	LoggingState_t *pState = GetCurrentState();
	pState->m_nListenerCount = 0;
	pState->m_pLoggingResponse = &m_DefaultLoggingResponse;
	m_pStateMutex->unlock();
}

void CLoggingSystem::SetLoggingResponsePolicy( ILoggingResponsePolicy *pLoggingResponse )
{
	if ( !m_pStateMutex )
		m_pStateMutex = new std::recursive_mutex();

	m_pStateMutex->lock();
	LoggingState_t *pState = GetCurrentState();
	if ( pLoggingResponse == nullptr )
	{
		pState->m_pLoggingResponse = &m_DefaultLoggingResponse;
	}
	else
	{
		pState->m_pLoggingResponse = pLoggingResponse;
	}
	m_pStateMutex->unlock();
}

LoggingResponse_t CLoggingSystem::LogDirect( LoggingChannelID_t channelID, LoggingSeverity_t severity, Color color, const tchar *pMessage )
{
	contract_assert( IsValidChannelID( channelID ) );
	if ( !IsValidChannelID( channelID ) )
		return LR_CONTINUE;

	LoggingContext_t context;
	context.m_ChannelID = channelID;
	context.m_Flags = m_RegisteredChannels[channelID].m_Flags;
	context.m_Severity = severity;
	context.m_Color = ( color == UNSPECIFIED_LOGGING_COLOR ) ? m_RegisteredChannels[channelID].m_SpewColor : color;
	
	// It is assumed that the mutex is reentrant safe on all platforms.
	if ( !m_pStateMutex )
		m_pStateMutex = new std::recursive_mutex();

	m_pStateMutex->lock();

	LoggingState_t *pState = GetCurrentState();
	
	for ( int i = 0; i < pState->m_nListenerCount; ++ i )
	{
		pState->m_RegisteredListeners[i]->Log( &context, pMessage );
	}

	LoggingResponse_t response = pState->m_pLoggingResponse->OnLog( &context );

	if ( false && severity != LS_MESSAGE )
	{
		response = LR_DEBUGGER;
	}

	m_pStateMutex->unlock();

	switch( response )
	{
	case LR_DEBUGGER:
		// Asserts put the debug break in the macro itself so the code breaks at the failure point.
		if ( severity != LS_ASSERT )
		{
			DebuggerBreakIfDebugging();
		}
		break;

	case LR_ABORT:
		Log_Msg( LOG_DEVELOPER_VERBOSE, "Exiting due to logging LR_ABORT request.\n" );
		Plat_ExitProcess( 1 );
		break;
	}

	return response;
}

CLoggingSystem::LoggingChannel_t *CLoggingSystem::GetChannel( LoggingChannelID_t channelID )
{
	contract_assert( IsValidChannelID( channelID ) );
	return &m_RegisteredChannels[channelID];
}

const CLoggingSystem::LoggingChannel_t *CLoggingSystem::GetChannel( LoggingChannelID_t channelID ) const
{
	contract_assert( IsValidChannelID( channelID ) );
	return &m_RegisteredChannels[channelID];
}

CLoggingSystem::LoggingState_t *CLoggingSystem::GetCurrentState()
{
	// Assume the caller grabbed the mutex.
	int nState = g_nThreadLocalStateIndex;
	if ( nState != 0 )
	{
		contract_assert( nState > 0 && nState < MAX_LOGGING_STATE_COUNT );
		return &m_LoggingStates[nState];
	}
	else
	{
		contract_assert( m_nGlobalStateIndex >= 0 && m_nGlobalStateIndex < MAX_LOGGING_STATE_COUNT );
		return &m_LoggingStates[m_nGlobalStateIndex];
	}
}

const CLoggingSystem::LoggingState_t *CLoggingSystem::GetCurrentState() const
{
	// Assume the caller grabbed the mutex.
	int nState = g_nThreadLocalStateIndex;
	if ( nState != 0 )
	{
		contract_assert( nState > 0 && nState < MAX_LOGGING_STATE_COUNT );
		return &m_LoggingStates[nState];
	}
	else
	{
		contract_assert( m_nGlobalStateIndex >= 0 && m_nGlobalStateIndex < MAX_LOGGING_STATE_COUNT );
		return &m_LoggingStates[m_nGlobalStateIndex];
	}
}

int CLoggingSystem::FindUnusedStateIndex()
{
	for ( int i = 0; i < MAX_LOGGING_STATE_COUNT; ++ i )
	{
		if ( m_LoggingStates[i].m_nListenerCount < 0 )
		{
			return i;
		}
	}
	return -1;
}

CLoggingSystem::LoggingTag_t *CLoggingSystem::AllocTag( const char *pTagName )
{
	contract_assert( m_nChannelTagCount < MAX_LOGGING_TAG_COUNT );
	LoggingTag_t *pTag = &m_ChannelTags[m_nChannelTagCount ++];
	
	pTag->m_pNextTag = nullptr;
	pTag->m_pTagName = m_TagNamePool + m_nTagNamePoolIndex;
	
	// Copy string into pool.
	std::size_t nTagLength = std::strlen( pTagName );
	contract_assert( m_nTagNamePoolIndex + nTagLength + 1 <= MAX_LOGGING_TAG_CHARACTER_COUNT );
	std::strcpy( m_TagNamePool + m_nTagNamePoolIndex, pTagName );
	m_nTagNamePoolIndex += ( int )nTagLength + 1;

	return pTag;
}

LoggingChannelID_t LoggingSystem_RegisterLoggingChannel( const char *pName, RegisterTagsFunc registerTagsFunc, int flags, LoggingSeverity_t severity, Color color )
{
	return GetGlobalLoggingSystem()->RegisterLoggingChannel( pName, registerTagsFunc, flags, severity, color );
}

void LoggingSystem_ResetCurrentLoggingState()
{
	GetGlobalLoggingSystem()->ResetCurrentLoggingState();
}

void LoggingSystem_RegisterLoggingListener( ILoggingListener *pListener )
{
	GetGlobalLoggingSystem()->RegisterLoggingListener( pListener );
}

void LoggingSystem_UnregisterLoggingListener(ILoggingListener *pListener)
{
}

void LoggingSystem_SetLoggingResponsePolicy( ILoggingResponsePolicy *pResponsePolicy )
{
	GetGlobalLoggingSystem()->SetLoggingResponsePolicy( pResponsePolicy );
}

void LoggingSystem_PushLoggingState( bool bThreadLocal, bool bClearState )
{
	GetGlobalLoggingSystem()->PushLoggingState( bThreadLocal, bClearState );
}

void LoggingSystem_PopLoggingState( bool bThreadLocal )
{
	GetGlobalLoggingSystem()->PopLoggingState( bThreadLocal );
}

void LoggingSystem_AddTagToCurrentChannel( const char *pTagName )
{
	GetGlobalLoggingSystem()->AddTagToCurrentChannel( pTagName );
}

LoggingChannelID_t LoggingSystem_FindChannel( const char *pChannelName )
{
	return GetGlobalLoggingSystem()->FindChannel( pChannelName );
}

int LoggingSystem_GetChannelCount()
{
	return GetGlobalLoggingSystem()->GetChannelCount();
}

LoggingChannelID_t LoggingSystem_GetFirstChannelID()
{
	return ( GetGlobalLoggingSystem()->GetChannelCount() > 0 ) ? 0 : INVALID_LOGGING_CHANNEL_ID;
}

LoggingChannelID_t LoggingSystem_GetNextChannelID( LoggingChannelID_t channelID )
{
	int nChannelCount = GetGlobalLoggingSystem()->GetChannelCount();
	int nNextChannel = channelID + 1;
	return ( nNextChannel < nChannelCount ) ? nNextChannel : INVALID_LOGGING_CHANNEL_ID;
}

const CLoggingSystem::LoggingChannel_t *LoggingSystem_GetChannel( LoggingChannelID_t channelIndex )
{
	return GetGlobalLoggingSystem()->GetChannel( channelIndex );
}

bool LoggingSystem_HasTag( LoggingChannelID_t channelID, const char *pTag )
{
	return GetGlobalLoggingSystem()->HasTag( channelID, pTag );
}

bool LoggingSystem_IsChannelEnabled( LoggingChannelID_t channelID, LoggingSeverity_t severity )
{
	return GetGlobalLoggingSystem()->IsChannelEnabled( channelID, severity );
}

void LoggingSystem_SetChannelSpewLevel( LoggingChannelID_t channelID, LoggingSeverity_t minimumSeverity )
{
	GetGlobalLoggingSystem()->SetChannelSpewLevel( channelID, minimumSeverity );
}

void LoggingSystem_SetChannelSpewLevelByName( const char *pName, LoggingSeverity_t minimumSeverity )
{
	GetGlobalLoggingSystem()->SetChannelSpewLevelByName( pName, minimumSeverity );
}

void LoggingSystem_SetChannelSpewLevelByTag( const char *pTag, LoggingSeverity_t minimumSeverity )
{
	GetGlobalLoggingSystem()->SetChannelSpewLevelByTag( pTag, minimumSeverity );
}

void LoggingSystem_SetGlobalSpewLevel( LoggingSeverity_t minimumSeverity )
{
	GetGlobalLoggingSystem()->SetGlobalSpewLevel( minimumSeverity );
}

int32 LoggingSystem_GetChannelColor( LoggingChannelID_t channelID )
{
	return GetGlobalLoggingSystem()->GetChannelColor( channelID ).GetRawColor();
}

void LoggingSystem_SetChannelColor( LoggingChannelID_t channelID, int color )
{
	Color c;
	c.SetRawColor( color );
	GetGlobalLoggingSystem()->SetChannelColor( channelID, c );
}

LoggingChannelFlags_t LoggingSystem_GetChannelFlags( LoggingChannelID_t channelID )
{
	return GetGlobalLoggingSystem()->GetChannelFlags( channelID );
}

void LoggingSystem_SetChannelFlags( LoggingChannelID_t channelID, LoggingChannelFlags_t flags )
{
	GetGlobalLoggingSystem()->SetChannelFlags( channelID, flags );
}

LoggingResponse_t LoggingSystem_Log( LoggingChannelID_t channelID, LoggingSeverity_t severity, const char *pMessageFormat, ... )
{
	if ( !GetGlobalLoggingSystem()->IsChannelEnabled( channelID, severity ) )
		return LR_CONTINUE;

	tchar formattedMessage[MAX_LOGGING_MESSAGE_LENGTH];

	std::va_list args;
	__builtin_va_start( args, pMessageFormat );
	std::vsnprintf( formattedMessage, MAX_LOGGING_MESSAGE_LENGTH, pMessageFormat, args );
	__builtin_va_end( args );

	return GetGlobalLoggingSystem()->LogDirect( channelID, severity, UNSPECIFIED_LOGGING_COLOR, formattedMessage );
}

LoggingResponse_t LoggingSystem_Log( LoggingChannelID_t channelID, LoggingSeverity_t severity, Color spewColor, const char *pMessageFormat, ... ) 
{
	if ( !GetGlobalLoggingSystem()->IsChannelEnabled( channelID, severity ) )
		return LR_CONTINUE;

	tchar formattedMessage[MAX_LOGGING_MESSAGE_LENGTH];

	std::va_list args;
	__builtin_va_start( args, pMessageFormat );
	std::vsnprintf( formattedMessage, MAX_LOGGING_MESSAGE_LENGTH, pMessageFormat, args );
	__builtin_va_end( args );

	return GetGlobalLoggingSystem()->LogDirect( channelID, severity, spewColor, formattedMessage );
}

LoggingResponse_t LoggingSystem_LogDirect( LoggingChannelID_t channelID, LoggingSeverity_t severity, Color spewColor, const char *pMessage )
{
	if ( !GetGlobalLoggingSystem()->IsChannelEnabled( channelID, severity ) )
		return LR_CONTINUE;
	return GetGlobalLoggingSystem()->LogDirect( channelID, severity, spewColor, pMessage );
}

LoggingResponse_t LoggingSystem_LogAssert( const char *pMessageFormat, ... ) 
{
	if ( !GetGlobalLoggingSystem()->IsChannelEnabled( LOG_ASSERT, LS_ASSERT ) )
		return LR_CONTINUE;

	tchar formattedMessage[MAX_LOGGING_MESSAGE_LENGTH];

	std::va_list args;
	__builtin_va_start( args, pMessageFormat );
	std::vsnprintf( formattedMessage, MAX_LOGGING_MESSAGE_LENGTH, pMessageFormat, args );
	__builtin_va_end( args );

	return GetGlobalLoggingSystem()->LogDirect( LOG_ASSERT, LS_ASSERT, UNSPECIFIED_LOGGING_COLOR, formattedMessage );
}
