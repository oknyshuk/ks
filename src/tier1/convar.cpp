// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core.basetypes;
import ks.core.color;
import ks.core.dbg;
import ks.core.numeric;
import ks.core.scratcharena;
import ks.core.types;
import ks.core.vprof;
import ks.platform;
import ks.simd;
import ks.tier1.characterset;
import ks.tier1.commandbuffer;
import ks.tier1.convar;
import ks.tier1.convar_serverbounded;
import ks.tier1.cvar;
import ks.tier1.exprevaluator;
import ks.tier1.iconvar;
import ks.tier1.icvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.registry;
import ks.tier1.strtools;
import ks.tier1.text_reader;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
import ks.tier1.vector_ops;

static bool DefaultAccessor( ConCommandBase *pVar )
{
	// Link to engine's list instead
	g_pCVar->RegisterConCommand( pVar );
	return true;
}

//-----------------------------------------------------------------------------
// Called by the framework to register the ConCommandBases of one registry with the ICVar
//-----------------------------------------------------------------------------
void ConVar_Register( ConCommandRegistry &registry, int nCVarFlag, FnConCommandBaseAccessor_t pAccessor )
{
	if ( !g_pCVar || registry.m_bRegistered )
		return;

	contract_assert( registry.m_nDLLIdentifier < 0 );
	registry.m_bRegistered = true;
	registry.m_nDLLIdentifier = g_pCVar->AllocateDLLIdentifier();

	ConCommandBase *pCur, *pNext;

	registry.m_pAccessor = pAccessor ? pAccessor : &DefaultAccessor;
	pCur = registry.m_pFirst;
	while ( pCur )
	{
		pNext = pCur->m_pNext;
		pCur->AddFlags( nCVarFlag );
		pCur->Init();
		pCur = pNext;
	}

	g_pCVar->AddSplitScreenConVars();
	g_pCVar->ProcessQueuedMaterialThreadConVarSets();

	registry.m_pFirst = nullptr;
}

void ConVar_Unregister( ConCommandRegistry &registry )
{
	if ( !g_pCVar || !registry.m_bRegistered )
		return;

	contract_assert( registry.m_nDLLIdentifier >= 0 );

	// Do this after unregister!!!
	g_pCVar->RemoveSplitScreenConVars( registry.m_nDLLIdentifier );
	g_pCVar->UnregisterConCommands( registry.m_nDLLIdentifier );
	registry.m_nDLLIdentifier = -1;
	registry.m_bRegistered = false;
}

void ConVar_Register( int nCVarFlag, FnConCommandBaseAccessor_t pAccessor )
{
	ConVar_Register( ks::engine_registry().convars, nCVarFlag, pAccessor );
}

void ConVar_Unregister( )
{
	ConVar_Unregister( ks::engine_registry().convars );
}


//-----------------------------------------------------------------------------
// Purpose: Default constructor
//-----------------------------------------------------------------------------
ConCommandBase::ConCommandBase( void )
{
	m_bRegistered   = false;
	m_Kind          = ConKind::Command;
	m_pszName       = nullptr;
	m_pszHelpString = nullptr;

	m_nFlags = 0;
	m_pNext  = nullptr;
	m_pRegistry = &ks::engine_registry().convars;
}

ConCommandBase::ConCommandBase( ks::registry &registry ) : ConCommandBase()
{
	m_pRegistry = &registry.convars;
}

//-----------------------------------------------------------------------------
// Purpose: The base console invoked command/cvar interface
// Input  : *pName - name of variable/command
//			*pHelpString - help text
//			flags - flags
//-----------------------------------------------------------------------------
ConCommandBase::ConCommandBase( const char *pName, const char *pHelpString /*=0*/, int flags /*= 0*/ )
{
	m_pRegistry = &ks::engine_registry().convars;
	m_Kind = ConKind::Command;
	Create( pName, pHelpString, flags );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ConCommandBase::~ConCommandBase( void )
{
}

//-----------------------------------------------------------------------------
// Returns the DLL identifier
//-----------------------------------------------------------------------------
CVarDLLIdentifier_t ConCommandBase::GetDLLIdentifier() const
{
	return m_pRegistry->m_nDLLIdentifier;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pName - 
//			callback - 
//			*pHelpString - 
//			flags - 
//-----------------------------------------------------------------------------
void ConCommandBase::Create( const char *pName, const char *pHelpString /*= 0*/, int flags /*= 0*/ )
{
	static const char *empty_string = "";

	m_bRegistered = false;

	// Name should be static data
	contract_assert( pName );
	m_pszName = pName;
	m_pszHelpString = pHelpString ? pHelpString : empty_string;

	m_nFlags = flags;

	if ( !( m_nFlags & FCVAR_UNREGISTERED ) )
	{
		m_pNext = m_pRegistry->m_pFirst;
		m_pRegistry->m_pFirst = this;
	}
	else
	{
		// It's unregistered
		m_pNext = nullptr;
	}

	// If the registry is already registered (this ConVar is not a global variable),
	//  register it.
	if ( m_pRegistry->m_pAccessor )
	{
		Init();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Used internally by OneTimeInit to initialize.
//-----------------------------------------------------------------------------
void ConCommandBase::Init()
{
	if ( m_pRegistry->m_pAccessor )
	{
		m_pRegistry->m_pAccessor( this );
	}
}

void ConCommandBase::Shutdown()
{
	if ( g_pCVar )
	{
		g_pCVar->UnregisterConCommand( this );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Return name of the command/var
// Output : const char
//-----------------------------------------------------------------------------
const char *ConCommandBase::GetName( void ) const
{
	if ( m_Kind != ConKind::Command )
		return static_cast< const ConVar * >( this )->ConVar::GetName();

	return m_pszName;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flag - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ConCommandBase::IsFlagSet( int flag ) const
{
	if ( m_Kind != ConKind::Command )
		return static_cast< const ConVar * >( this )->ConVar::IsFlagSet( flag );

	return ( flag & m_nFlags ) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flags - 
//-----------------------------------------------------------------------------
void ConCommandBase::AddFlags( int flags )
{
	if ( m_Kind != ConKind::Command )
	{
		static_cast< ConVar * >( this )->ConVar::AddFlags( flags );
		return;
	}

	m_nFlags |= flags;

}

//-----------------------------------------------------------------------------
// Purpose: removes specified flags
//-----------------------------------------------------------------------------
void ConCommandBase::RemoveFlags( int flags )
{
	m_nFlags &= ~flags;
}

// Returns current flags
int ConCommandBase::GetFlags() const
{
	if ( m_Kind != ConKind::Command )
		return static_cast< const ConVar * >( this )->ConVar::GetFlags();

	return m_nFlags;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const ConCommandBase
//-----------------------------------------------------------------------------
const ConCommandBase *ConCommandBase::GetNext( void ) const
{
	return m_pNext;
}

ConCommandBase *ConCommandBase::GetNext( void )
{
	return m_pNext;
}


//-----------------------------------------------------------------------------
// Purpose: Copies string using local new/delete operators
// Input  : *from - 
// Output : char
//-----------------------------------------------------------------------------
char *ConCommandBase::CopyString( const char *from )
{
	int		len;
	char	*to;

	len = std::strlen( from );
	if ( len <= 0 )
	{
		to = new char[1];
		to[0] = 0;
	}
	else
	{
		to = new char[len+1];
		V_strncpy( to, from, len+1 );
	}
	return to;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *ConCommandBase::GetHelpText( void ) const
{
	if ( m_Kind != ConKind::Command )
		return static_cast< const ConVar * >( this )->ConVar::GetHelpText();

	return m_pszHelpString;
}

//-----------------------------------------------------------------------------
// Purpose: Has this cvar been registered
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ConCommandBase::IsRegistered( void ) const
{
	if ( m_Kind != ConKind::Command )
		return static_cast< const ConVar * >( this )->ConVar::IsRegistered();

	return m_bRegistered;
}


//-----------------------------------------------------------------------------
//
// Con Commands start here
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Global methods
//-----------------------------------------------------------------------------
static characterset_t s_BreakSet;
static bool s_bBuiltBreakSet = false;


//-----------------------------------------------------------------------------
// Tokenizer class
//-----------------------------------------------------------------------------
CCommand::CCommand()
{
	if ( !s_bBuiltBreakSet )
	{
		s_bBuiltBreakSet = true;
		CharacterSetBuild( &s_BreakSet, "{}()':" );
	}

	Reset();
}

CCommand::CCommand( int nArgC, const char **ppArgV, cmd_source_t source )
{
	contract_assert( nArgC > 0 );

	if ( !s_bBuiltBreakSet )
	{
		s_bBuiltBreakSet = true;
		CharacterSetBuild( &s_BreakSet, "{}()':" );
	}

	Reset();

	char *pBuf = m_pArgvBuffer;
	char *pSBuf = m_pArgSBuffer;
	m_nArgc = nArgC;
	for ( int i = 0; i < nArgC; ++i )
	{
		m_ppArgv[i] = pBuf;
		int nLen = V_strlen( ppArgV[i] );
		std::memcpy( pBuf, ppArgV[i], nLen+1 );
		if ( i == 0 )
		{
			m_nArgv0Size = nLen;
		}
		pBuf += nLen+1;

		bool bContainsSpace = std::strchr( ppArgV[i], ' ' ) != nullptr;
		if ( bContainsSpace )
		{
			*pSBuf++ = '\"';
		}
		std::memcpy( pSBuf, ppArgV[i], nLen );
		pSBuf += nLen;
		if ( bContainsSpace )
		{
			*pSBuf++ = '\"';
		}

		if ( i != nArgC - 1 )
		{
			*pSBuf++ = ' ';
		}
	}

	m_source = source;
}

void CCommand::Reset()
{
	m_nArgc = 0;
	m_nArgv0Size = 0;
	m_pArgSBuffer[0] = 0;
	m_source = kCommandSrcInvalid;
}

characterset_t* CCommand::DefaultBreakSet()
{
	return &s_BreakSet;
}

bool CCommand::Tokenize( const char *pCommand, cmd_source_t source, characterset_t *pBreakSet )
{
	Reset();
	m_source = source;

	if ( !pCommand )
		return false;

	// Use default break set
	if ( !pBreakSet )
	{
		pBreakSet = &s_BreakSet;
	}

	// Copy the current command into a temp buffer
	// NOTE: This is here to avoid the pointers returned by DequeueNextCommand
	// to become invalid by calling AddText. Is there a way we can avoid the memcpy?
	int nLen = V_strlen( pCommand );
	if ( nLen >= COMMAND_MAX_LENGTH - 1 )
	{
		Warning( "CCommand::Tokenize: Encountered command which overflows the tokenizer buffer.. Skipping!\n" );
		return false;
	}

	std::memcpy( m_pArgSBuffer, pCommand, nLen + 1 );

	// Parse the current command into the current command buffer
	ks::text_reader bufParse( m_pArgSBuffer, nLen );
	int nArgvBufferSize = 0;
	while ( bufParse.ok() && ( m_nArgc < COMMAND_MAX_ARGC ) )
	{
		char *pArgvBuf = &m_pArgvBuffer[nArgvBufferSize];
		int nMaxLen = COMMAND_MAX_LENGTH - nArgvBufferSize;
		int nStartGet = (int)bufParse.tell();
		const std::optional< std::string_view > token = bufParse.read_token( *pBreakSet );
		if ( !token )
			break;

		int nSize = (int)token->size();

		// Check for overflow condition
		if ( nMaxLen <= nSize )
		{
			Reset();
			return false;
		}

		std::memcpy( pArgvBuf, token->data(), nSize );
		pArgvBuf[nSize] = 0;

		if ( m_nArgc == 1 )
		{
			// Deal with the case where the arguments were quoted
			m_nArgv0Size = (int)bufParse.tell();
			bool bFoundEndQuote = m_pArgSBuffer[m_nArgv0Size-1] == '\"';
			if ( bFoundEndQuote )
			{
				--m_nArgv0Size;
			}
			m_nArgv0Size -= nSize;
			contract_assert( m_nArgv0Size != 0 );

			// The StartGet check is to handle this case: "foo"bar
			// which will parse into 2 different args. ArgS should point to bar.
			bool bFoundStartQuote = ( m_nArgv0Size > nStartGet ) && ( m_pArgSBuffer[m_nArgv0Size-1] == '\"' );
			contract_assert( bFoundEndQuote == bFoundStartQuote );
			if ( bFoundStartQuote )
			{
				--m_nArgv0Size;
			}
		}

		m_ppArgv[ m_nArgc++ ] = pArgvBuf;
		if( m_nArgc >= COMMAND_MAX_ARGC )
		{
			Warning( "CCommand::Tokenize: Encountered command which overflows the argument buffer.. Clamped!\n" );
		}

		nArgvBufferSize += nSize + 1;
		contract_assert( nArgvBufferSize <= COMMAND_MAX_LENGTH );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Helper function to parse arguments to commands.
//-----------------------------------------------------------------------------
const char* CCommand::FindArg( const char *pName ) const
{
	int nArgC = ArgC();
	for ( int i = 1; i < nArgC; i++ )
	{
		if ( !V_stricmp( Arg(i), pName ) )
			return (i+1) < nArgC ? Arg( i+1 ) : "";
	}
	return 0;
}

int CCommand::FindArgInt( const char *pName, int nDefaultVal ) const
{
	const char *pVal = FindArg( pName );
	if ( pVal )
		return std::atoi( pVal );
	else
		return nDefaultVal;
}


//-----------------------------------------------------------------------------
// Purpose: Constructs a console command
//-----------------------------------------------------------------------------
//ConCommand::ConCommand()
//{
//	m_bIsNewConCommand = true;
//}

// The legacy completion callbacks fill in a fixed array of strings
static ConCommandCompletionCallback_t WrapCompletion( FnCommandCompletionCallback completionFunc )
{
	if ( !completionFunc )
		return ConCommandCompletionCallback_t();

	return ConCommandCompletionCallback_t( [completionFunc]( const char *partial, std::vector< std::string > &commands )
	{
		char rgpchCommands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ];
		int iret = completionFunc( partial, rgpchCommands );
		for ( int i = 0 ; i < iret; ++i )
		{
			std::string str = rgpchCommands[ i ];
			commands.push_back( str );
		}
		return iret;
	} );
}

ConCommand::ConCommand( const char *pName, FnCommandCallbackV1_t callback, const char *pHelpString /*= 0*/, int flags /*= 0*/, FnCommandCompletionCallback completionFunc /*= 0*/ )
{
	// Set the callback
	if ( callback )
	{
		m_fnCallback = [callback]( const CCommand & ) { ( *callback )(); };
	}
	m_fnCompletion = WrapCompletion( completionFunc );

	// Setup the rest
	BaseClass::Create( pName, pHelpString, flags );
}

ConCommand::ConCommand( ks::registry &registry, const char *pName, FnCommandCallbackV1_t callback, const char *pHelpString /*= 0*/, int flags /*= 0*/, FnCommandCompletionCallback completionFunc /*= 0*/ )
	: ConCommandBase( registry )
{
	// Set the callback
	if ( callback )
	{
		m_fnCallback = [callback]( const CCommand & ) { ( *callback )(); };
	}
	m_fnCompletion = WrapCompletion( completionFunc );

	// Setup the rest
	BaseClass::Create( pName, pHelpString, flags );
}

ConCommand::ConCommand( const char *pName, FnCommandCallback_t callback, const char *pHelpString /*= 0*/, int flags /*= 0*/, FnCommandCompletionCallback completionFunc /*= 0*/ )
{
	// Set the callback
	if ( callback )
	{
		m_fnCallback = callback;
	}
	m_fnCompletion = WrapCompletion( completionFunc );

	// Setup the rest
	BaseClass::Create( pName, pHelpString, flags );
}

ConCommand::ConCommand( ks::registry &registry, const char *pName, FnCommandCallback_t callback, const char *pHelpString /*= 0*/, int flags /*= 0*/, FnCommandCompletionCallback completionFunc /*= 0*/ )
	: ConCommandBase( registry )
{
	// Set the callback
	if ( callback )
	{
		m_fnCallback = callback;
	}
	m_fnCompletion = WrapCompletion( completionFunc );

	// Setup the rest
	BaseClass::Create( pName, pHelpString, flags );
}

ConCommand::ConCommand( StatefulTag_t, const char *pName, ConCommandCallback_t callback, const char *pHelpString, int flags, ConCommandCompletionCallback_t completionCallback )
	: m_fnCallback( std::move( callback ) ), m_fnCompletion( std::move( completionCallback ) )
{
	// Setup the rest
	BaseClass::Create( pName, pHelpString, flags );
}

ConCommand::ConCommand( StatefulTag_t, ks::registry &registry, const char *pName, ConCommandCallback_t callback, const char *pHelpString, int flags, ConCommandCompletionCallback_t completionCallback )
	: ConCommandBase( registry ), m_fnCallback( std::move( callback ) ), m_fnCompletion( std::move( completionCallback ) )
{
	// Setup the rest
	BaseClass::Create( pName, pHelpString, flags );
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
ConCommand::~ConCommand( void )
{
}


//-----------------------------------------------------------------------------
// Purpose: Invoke the function if there is one
//-----------------------------------------------------------------------------
void ConCommand::Dispatch( const CCommand &command )
{
	if ( m_fnCallback )
	{
		m_fnCallback( command );
		return;
	}

	// Command without callback!!!
	contract_assert( 0 );
}


//-----------------------------------------------------------------------------
// Purpose: Calls the autocompletion method to get autocompletion suggestions
//-----------------------------------------------------------------------------
int	ConCommand::AutoCompleteSuggest( const char *partial, std::vector< std::string > &commands )
{
	if ( !m_fnCompletion )
		return 0;

	return m_fnCompletion( partial, commands );
}


//-----------------------------------------------------------------------------
// Returns true if the console command can autocomplete 
//-----------------------------------------------------------------------------
bool ConCommand::CanAutoComplete( void )
{
	return static_cast< bool >( m_fnCompletion );
}



//-----------------------------------------------------------------------------
//
// Console Variables
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Various constructors
//-----------------------------------------------------------------------------
ConVar::ConVar( const char *pName, const char *pDefaultValue, int flags /* = 0 */ )
{
	Create( pName, pDefaultValue, flags );
}

ConVar::ConVar( const char *pName, const char *pDefaultValue, int flags, const char *pHelpString )
{
	Create( pName, pDefaultValue, flags, pHelpString );
}

ConVar::ConVar( const char *pName, const char *pDefaultValue, int flags, const char *pHelpString, bool bMin, float fMin, bool bMax, float fMax )
{
	Create( pName, pDefaultValue, flags, pHelpString, bMin, fMin, bMax, fMax );
}

ConVar::ConVar( const char *pName, const char *pDefaultValue, int flags, const char *pHelpString, FnChangeCallback_t callback )
{
	Create( pName, pDefaultValue, flags, pHelpString, false, 0.0, false, 0.0, callback );
}

ConVar::ConVar( const char *pName, const char *pDefaultValue, int flags, const char *pHelpString, bool bMin, float fMin, bool bMax, float fMax, FnChangeCallback_t callback )
{
	Create( pName, pDefaultValue, flags, pHelpString, bMin, fMin, bMax, fMax, callback );
}


ConVar::ConVar( ks::registry &registry, const char *pName, const char *pDefaultValue, int flags /* = 0 */ )
	: ConCommandBase( registry )
{
	Create( pName, pDefaultValue, flags );
}

ConVar::ConVar( ks::registry &registry, const char *pName, const char *pDefaultValue, int flags, const char *pHelpString )
	: ConCommandBase( registry )
{
	Create( pName, pDefaultValue, flags, pHelpString );
}

ConVar::ConVar( ks::registry &registry, const char *pName, const char *pDefaultValue, int flags, const char *pHelpString, bool bMin, float fMin, bool bMax, float fMax )
	: ConCommandBase( registry )
{
	Create( pName, pDefaultValue, flags, pHelpString, bMin, fMin, bMax, fMax );
}

ConVar::ConVar( ks::registry &registry, const char *pName, const char *pDefaultValue, int flags, const char *pHelpString, FnChangeCallback_t callback )
	: ConCommandBase( registry )
{
	Create( pName, pDefaultValue, flags, pHelpString, false, 0.0, false, 0.0, callback );
}

ConVar::ConVar( ks::registry &registry, const char *pName, const char *pDefaultValue, int flags, const char *pHelpString, bool bMin, float fMin, bool bMax, float fMax, FnChangeCallback_t callback )
	: ConCommandBase( registry )
{
	Create( pName, pDefaultValue, flags, pHelpString, bMin, fMin, bMax, fMax, callback );
}


//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
ConVar::~ConVar( void )
{
	if ( m_Value.m_pszString )
	{
		delete[] m_Value.m_pszString;
		m_Value.m_pszString = nullptr;
	}
}


//-----------------------------------------------------------------------------
// Install a change callback (there shouldn't already be one....)
//-----------------------------------------------------------------------------
void ConVar::InstallChangeCallback( FnChangeCallback_t callback, bool bInvoke /*=true*/ )
{
	if ( !callback )
	{
		Warning( "InstallChangeCallback called with NULL callback, ignoring!!!\n" );
		return;
	}

	if ( ks::find_index( m_pParent->m_fnChangeCallbacks, callback ) != -1 )
	{
		// Same ptr added twice, sigh...
		Warning( "InstallChangeCallback ignoring duplicate change callback!!!\n" );
		return;
	}

	m_pParent->m_fnChangeCallbacks.push_back( callback );

	// Call it immediately to set the initial value...
	if ( bInvoke )
	{
		callback( this, m_Value.m_pszString, StoreValue( m_Value.m_fValue, ( intp ) this ) );
	}
}

void ConVar::RemoveChangeCallback( FnChangeCallback_t callback )
{
	ks::erase_first( m_pParent->m_fnChangeCallbacks, callback );
}

bool ConVar::IsFlagSet( int flag ) const
{
	if ( Kind() == ConKind::VarEmpty )
		return false;

	return ( flag & m_pParent->m_nFlags ) ? true : false;
}

int ConVar::GetFlags() const
{
	return m_pParent->m_nFlags;
}

const char *ConVar::GetHelpText( void ) const
{
	return m_pParent->m_pszHelpString;
}

void ConVar::AddFlags( int flags )
{
	m_pParent->m_nFlags |= flags;

}

bool ConVar::IsRegistered( void ) const
{
	return m_pParent->m_bRegistered;
}

const char *ConVar::GetName( void ) const
{
	if ( Kind() == ConKind::VarEmpty )
		return "";

	return m_pParent->m_pszName;
}

const char *ConVar::GetBaseName( void ) const
{
	if ( Kind() == ConKind::VarSplitScreenAdded )
		return static_cast< const CSplitScreenAddedConVar * >( this )->CSplitScreenAddedConVar::GetBaseName();

	return m_pParent->m_pszName;
}

int ConVar::GetSplitScreenPlayerSlot( void ) const
{
	if ( Kind() == ConKind::VarSplitScreenAdded )
		return static_cast< const CSplitScreenAddedConVar * >( this )->CSplitScreenAddedConVar::GetSplitScreenPlayerSlot();

	// Default implementation (certain FCVAR_USERINFO derive a new type of convar and set this)
	return 0;
}

//-----------------------------------------------------------------------------
// The value of a ConVar_ServerBounded
//-----------------------------------------------------------------------------
float ConVar::GetBoundedFloat( void ) const
{
	return static_cast< const ConVar_ServerBounded * >( this )->GetBoundedValue();
}

bool ConVar::InternalSetColorFromString( const char *value )
{
	bool bColor = false;

	// Try pulling RGBA color values out of the string
	int nRGBA[4];
	int nParamsRead = std::sscanf( value, "%i %i %i %i", &(nRGBA[0]), &(nRGBA[1]), &(nRGBA[2]), &(nRGBA[3]) );

	if ( nParamsRead >= 3 )
	{
		// This is probably a color!
		if ( nParamsRead == 3 )
		{
			// Assume they wanted full alpha
			nRGBA[3] = 255;
		}

		if ( nRGBA[0] >= 0 && nRGBA[0] <= 255 && 
			 nRGBA[1] >= 0 && nRGBA[1] <= 255 && 
			 nRGBA[2] >= 0 && nRGBA[2] <= 255 && 
			 nRGBA[3] >= 0 && nRGBA[3] <= 255 )
		{
			// This is definitely a color!
			bColor = true;

			// Stuff all the values into each byte of our int
			unsigned char *pColorElement = ((unsigned char*)&m_Value.m_nValue);
			pColorElement[0] = nRGBA[0];
			pColorElement[1] = nRGBA[1];
			pColorElement[2] = nRGBA[2];
			pColorElement[3] = nRGBA[3];

			// Obscure the value.
			m_Value.m_nValue = StoreValue( m_Value.m_nValue, ( intp ) this );

			// Copy that value into a float (even though this has little meaning)
			m_Value.m_fValue = StoreValue( ( float )( m_Value.m_nValue ), ( intp ) this );
		}
	}

	return bColor;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *value - 
//-----------------------------------------------------------------------------
void ConVar::InternalSetValue( const char *value )
{
	if ( IsFlagSet( FCVAR_MATERIAL_THREAD_MASK ) )
	{
		if ( g_pCVar && !g_pCVar->IsMaterialThreadSetAllowed() )
		{
			g_pCVar->QueueMaterialThreadSetValue( this, value );
			return;
		}
	}

	char  tempVal[ 32 ];
	const char  *val;

	contract_assert( m_pParent == this ); // Only valid for root convars.

	float flOldValue = StoreValue( m_Value.m_fValue, ( intp ) this );
	val = value;
	if ( !val )
		val = "";

	if ( !InternalSetColorFromString( value ) )
	{
		// Not a color, do the standard thing
		double dblValue = V_atod( value ); // Use double to avoid 24-bit restriction on integers and allow storing timestamps or dates in convars

		float fNewValue = ( float ) dblValue;
		if ( !IsFinite( fNewValue ) )
		{
			Warning( "Warning:  %s = '%s' is infinite, clamping value.\n", GetName(), value );
			fNewValue = std::numeric_limits<float>::max();
			dblValue = std::numeric_limits<float>::max();
		}

		if ( ClampValue( fNewValue ) )
		{
			dblValue = fNewValue;
			V_snprintf( tempVal,sizeof(tempVal), "%f", fNewValue );
			val = tempVal;
		}

		// Redetermine value
		m_Value.m_fValue = StoreValue( fNewValue, (intp) this );
		m_Value.m_nValue = StoreValue( ( int ) dblValue, (intp) this ); // convert double to int to avoid precision loss
	}

	if ( !( m_nFlags & FCVAR_NEVER_AS_STRING ) )
	{
		ChangeStringValue( val, flOldValue );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *tempVal - 
//-----------------------------------------------------------------------------
void ConVar::ChangeStringValue( const char *tempVal, float flOldValue )
{
	ks::ScratchArena<> scratch;
	contract_assert( !( m_nFlags & FCVAR_NEVER_AS_STRING ) );

 	char* pszOldValue = (char*)scratch.AllocBytes( m_Value.m_StringLength );
	std::memcpy( pszOldValue, m_Value.m_pszString, m_Value.m_StringLength );
	
	int len = V_strlen(tempVal) + 1;

	if ( len > m_Value.m_StringLength)
	{
		if (m_Value.m_pszString)
		{
			delete[] m_Value.m_pszString;
		}

		m_Value.m_pszString	= new char[len];
		m_Value.m_StringLength = len;
	}

	std::memcpy( m_Value.m_pszString, tempVal, len );

	// Invoke any necessary callback function
	for ( int i = 0; i < std::ssize( m_fnChangeCallbacks ); ++i )
	{
		m_fnChangeCallbacks[ i ]( this, pszOldValue, flOldValue );
	}

	if ( g_pCVar )
	{
		g_pCVar->CallGlobalChangeCallbacks( this, pszOldValue, flOldValue );
	}

}

//-----------------------------------------------------------------------------
// Purpose: Check whether to clamp and then perform clamp
// Input  : value - 
// Output : Returns true if value changed
//-----------------------------------------------------------------------------
bool ConVar::ClampValue( float& value )
{
	if ( m_bHasMin && ( value < m_fMinVal ) )
	{
		value = m_fMinVal;
		return true;
	}
	
	if ( m_bHasMax && ( value > m_fMaxVal ) )
	{
		value = m_fMaxVal;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *value - 
//-----------------------------------------------------------------------------
void ConVar::InternalSetFloatValue( float fNewValue )
{
	if ( fNewValue == StoreValue( m_Value.m_fValue, ( intp ) this ) )
		return;

	if ( IsFlagSet( FCVAR_MATERIAL_THREAD_MASK ) )
	{
		if ( g_pCVar && !g_pCVar->IsMaterialThreadSetAllowed() )
		{
			g_pCVar->QueueMaterialThreadSetValue( this, fNewValue );
			return;
		}
	}

	contract_assert( m_pParent == this ); // Only valid for root convars.

	// Check bounds
	ClampValue( fNewValue );

	// Redetermine value
	float flOldValue = StoreValue( m_Value.m_fValue, ( intp ) this );
	m_Value.m_fValue		= StoreValue( fNewValue, ( intp) this );
	m_Value.m_nValue		= StoreValue( ( int )fNewValue, ( intp) this );

	if ( !( m_nFlags & FCVAR_NEVER_AS_STRING ) )
	{
		char tempVal[ 32 ];
		V_snprintf( tempVal, sizeof( tempVal), "%f", fNewValue );
		ChangeStringValue( tempVal, flOldValue );
	}
	else
	{
		contract_assert( std::ssize( m_fnChangeCallbacks ) == 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *value - 
//-----------------------------------------------------------------------------
void ConVar::InternalSetIntValue( int nValue )
{
	if ( nValue == StoreValue( m_Value.m_nValue, (intp) this ) )
		return;

	if ( IsFlagSet( FCVAR_MATERIAL_THREAD_MASK ) )
	{
		if ( g_pCVar && !g_pCVar->IsMaterialThreadSetAllowed() )
		{
			g_pCVar->QueueMaterialThreadSetValue( this, nValue );
			return;
		}
	}

	contract_assert( m_pParent == this ); // Only valid for root convars.

	float fValue = (float)nValue;
	if ( ClampValue( fValue ) )
	{
		nValue = ( int )( fValue );
	}

	// Redetermine value
	float flOldValue = StoreValue( m_Value.m_fValue, ( intp ) this );
	m_Value.m_fValue		= StoreValue( fValue, ( intp) this );
	m_Value.m_nValue		= StoreValue( nValue, ( intp) this );

	if ( !( m_nFlags & FCVAR_NEVER_AS_STRING ) )
	{
		char tempVal[ 32 ];
		V_snprintf( tempVal, sizeof( tempVal ), "%d", nValue );
		ChangeStringValue( tempVal, flOldValue );
	}
	else
	{
		contract_assert( std::ssize( m_fnChangeCallbacks ) == 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *value - 
//-----------------------------------------------------------------------------
void ConVar::InternalSetColorValue( Color value )
{
	// Stuff color values into an int
	int nValue;

	unsigned char *pColorElement = ((unsigned char*)&nValue);
	pColorElement[0] = value[0];
	pColorElement[1] = value[1];
	pColorElement[2] = value[2];
	pColorElement[3] = value[3];

	// Call the int internal set
	InternalSetIntValue( nValue );
}

//-----------------------------------------------------------------------------
// Purpose: Private creation
//-----------------------------------------------------------------------------
void ConVar::Create( const char *pName, const char *pDefaultValue, int flags /*= 0*/,
	const char *pHelpString /*= NULL*/, bool bMin /*= false*/, float fMin /*= 0.0*/,
	bool bMax /*= false*/, float fMax /*= false*/, FnChangeCallback_t callback /*= NULL*/ )
{
	static const char *empty_string = "";

	SetKind( ConKind::Var );

	m_pParent = this;

	// Name should be static data
	m_pszDefaultValue = pDefaultValue ? pDefaultValue : empty_string;
	contract_assert( m_pszDefaultValue );

	m_bHasMin = bMin;
	m_fMinVal = fMin;
	m_bHasMax = bMax;
	m_fMaxVal = fMax;

	if ( callback )
	{
		m_fnChangeCallbacks.push_back( callback );
	}

	m_Value.m_StringLength = std::strlen( m_pszDefaultValue ) + 1;
	m_Value.m_pszString = new char[m_Value.m_StringLength];
	std::memcpy( m_Value.m_pszString, m_pszDefaultValue, m_Value.m_StringLength );

	if ( !InternalSetColorFromString( m_Value.m_pszString ) )
	{
		double dblValue = V_atod( m_Value.m_pszString ); // Use double to avoid 24-bit restriction on integers and allow storing timestamps or dates in convars

		float fValue = ( float ) dblValue;
		if ( !IsFinite( fValue ) )
		{
			Warning( "ConVar(%s) defined with infinite float value (%s)\n", pName, m_Value.m_pszString );
			fValue = std::numeric_limits<float>::max();
			contract_assert( 0 );
		}

		// Bounds Check, should never happen, if it does, no big deal
		if ( m_bHasMin && ( fValue < m_fMinVal ) )
		{
			contract_assert( 0 );
		}

		if ( m_bHasMax && ( fValue > m_fMaxVal ) )
		{
			contract_assert( 0 );
		}

		m_Value.m_fValue = StoreValue( fValue, ( intp ) this );
		m_Value.m_nValue = StoreValue( ( int ) dblValue, ( intp) this );
	}

	BaseClass::Create( pName, pHelpString, flags );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *value - 
//-----------------------------------------------------------------------------
void ConVar::SetValue(const char *value)
{
	if ( Kind() == ConKind::VarEmpty )
		return;

	ConVar *var = ( ConVar * )m_pParent;
	var->InternalSetValue( value );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : value - 
//-----------------------------------------------------------------------------
void ConVar::SetValue( float value )
{
	if ( Kind() == ConKind::VarEmpty )
		return;

	ConVar *var = ( ConVar * )m_pParent;
	var->InternalSetFloatValue( value );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : value - 
//-----------------------------------------------------------------------------
void ConVar::SetValue( int value )
{
	if ( Kind() == ConKind::VarEmpty )
		return;

	ConVar *var = ( ConVar * )m_pParent;
	var->InternalSetIntValue( value );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : value - 
//-----------------------------------------------------------------------------
void ConVar::SetValue( Color value )
{
	ConVar *var = ( ConVar * )m_pParent;
	var->InternalSetColorValue( value );
}

//-----------------------------------------------------------------------------
// Purpose: Reset to default value
//-----------------------------------------------------------------------------
void ConVar::Revert( void )
{
	// Force default value again
	ConVar *var = ( ConVar * )m_pParent;
	var->SetValue( var->m_pszDefaultValue );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : minVal - 
// Output : true if there is a min set
//-----------------------------------------------------------------------------
bool ConVar::GetMin( float& minVal ) const
{
	minVal = m_pParent->m_fMinVal;
	return m_pParent->m_bHasMin;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : maxVal - 
//-----------------------------------------------------------------------------
bool ConVar::GetMax( float& maxVal ) const
{
	maxVal = m_pParent->m_fMaxVal;
	return m_pParent->m_bHasMax;
}

float ConVar::GetMinValue() const
{
	return m_pParent->m_fMinVal;
}

float ConVar::GetMaxValue() const
{
	return m_pParent->m_fMaxVal;;
}

bool ConVar::HasMin() const 
{ 
	return m_pParent->m_bHasMin; 
}

bool ConVar::HasMax() const 
{ 
	return m_pParent->m_bHasMax; 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *ConVar::GetDefault( void ) const
{
	return m_pParent->m_pszDefaultValue;
}

void ConVar::SetDefault( const char *pszDefault ) 
{ 
	static const char *empty_string = "";
	m_pszDefaultValue = pszDefault ? pszDefault : empty_string;
	contract_assert( m_pszDefaultValue );
}

static CEmptyConVar s_EmptyConVar;

ConVarRef::ConVarRef( const char *pName )
{
	Init( pName, false );
}

ConVarRef::ConVarRef( const char *pName, bool bIgnoreMissing )
{
	Init( pName, bIgnoreMissing );
}

void ConVarRef::Init( const char *pName, bool bIgnoreMissing )
{
	m_pConVar = g_pCVar ? g_pCVar->FindVar( pName ) : &s_EmptyConVar;
	if ( !m_pConVar )
	{
		m_pConVar = &s_EmptyConVar;
	}
	m_pConVarState = static_cast< ConVar * >( m_pConVar );
	if( !IsValid() )
	{
		static bool bFirst = true;
		if ( g_pCVar || bFirst )
		{
			if ( !bIgnoreMissing )
			{
				Warning( "ConVarRef %s doesn't point to an existing ConVar\n", pName );
			}
			bFirst = false;
		}
	}
}

ConVarRef::ConVarRef( ConVar *pConVar )
{
	m_pConVar = pConVar ? pConVar : &s_EmptyConVar;
	m_pConVarState = static_cast< ConVar * >( m_pConVar );
}

bool ConVarRef::IsValid() const
{
	return m_pConVar != &s_EmptyConVar;
}

// Helper for splitscreen ConVars
SplitScreenConVarRef::SplitScreenConVarRef( const char *pName )
{
	Init( pName, false );
}

SplitScreenConVarRef::SplitScreenConVarRef( const char *pName, bool bIgnoreMissing )
{
	Init( pName, bIgnoreMissing );
}

void SplitScreenConVarRef::Init( const char *pName, bool bIgnoreMissing )
{
	for ( int i = 0; i < MAX_SPLITSCREEN_CLIENTS; ++i )
	{
		cv_t &info = m_Info[ i ];
		char pchName[ 256 ];
		if ( i != 0 )
		{
			V_snprintf( pchName, sizeof( pchName ), "%s%d", pName, i + 1 );
		}
		else
		{
			V_strncpy( pchName, pName, sizeof( pchName ) );
		}

		info.m_pConVar = g_pCVar ? g_pCVar->FindVar( pchName ) : &s_EmptyConVar;
		if ( !info.m_pConVar )
		{
			info.m_pConVar = &s_EmptyConVar;
			if ( i > 0 )
			{
				// Point at slot zero instead, in case we got in here with a non FCVAR_SS var...
				info.m_pConVar = m_Info[ 0 ].m_pConVar;
			}
		}
		info.m_pConVarState = static_cast< ConVar * >( info.m_pConVar );
	}

	if ( !IsValid() )
	{
		static bool bFirst = true;
		if ( g_pCVar || bFirst )
		{
			if ( !bIgnoreMissing )
			{
				Warning( "ConVarRef %s doesn't point to an existing ConVar\n", pName );
			}
			bFirst = false;
		}
	}
}

SplitScreenConVarRef::SplitScreenConVarRef( ConVar *pConVar )
{
	{
		cv_t &info = m_Info[ 0 ];
		info.m_pConVar = pConVar ? pConVar : &s_EmptyConVar;
		info.m_pConVarState = static_cast< ConVar * >( info.m_pConVar );
	}

	for ( int i = 1; i < MAX_SPLITSCREEN_CLIENTS; ++i )
	{
		cv_t &info = m_Info[ i ];
		char pchName[ 256 ];
		V_snprintf( pchName, sizeof( pchName ), "%s%d", pConVar->GetName(), i + 1 );

		info.m_pConVar = g_pCVar ? g_pCVar->FindVar( pchName ) : &s_EmptyConVar;
		if ( !info.m_pConVar )
		{
			info.m_pConVar = &s_EmptyConVar;
			if ( i > 0 )
			{
				// Point at slot zero instead, in case we got in here with a non FCVAR_SS var...
				info.m_pConVar = m_Info[ 0 ].m_pConVar;
			}
		}
		info.m_pConVarState = static_cast< ConVar * >( info.m_pConVar );
	}
}

bool SplitScreenConVarRef::IsValid() const
{
	return m_Info[ 0 ].m_pConVar != &s_EmptyConVar;
}

struct PrintConVarFlags_t
{
	int flag;
	const char *desc;
};

static PrintConVarFlags_t g_PrintConVarFlags[] =
{
	{ FCVAR_GAMEDLL, "game" },
	{ FCVAR_CLIENTDLL, "client" },
	{ FCVAR_ARCHIVE, "archive" },
	{ FCVAR_NOTIFY, "notify" },
	{ FCVAR_SPONLY, "singleplayer" },
	{ FCVAR_NOT_CONNECTED, "notconnected" },
	{ FCVAR_CHEAT, "cheat" },
	{ FCVAR_REPLICATED, "replicated" },
	{ FCVAR_SERVER_CAN_EXECUTE, "server_can_execute" },
	{ FCVAR_CLIENTCMD_CAN_EXECUTE, "clientcmd_can_execute" },
	{ FCVAR_USERINFO, "user" },
	{ FCVAR_SS, "ss" },
	{ FCVAR_SS_ADDED, "ss_added" },
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ConVar_AppendFlags( const ConCommandBase *var, char *buf, std::size_t bufsize )
{
	for ( int i = 0; i < std::size( g_PrintConVarFlags ) ; ++i )
	{
		const PrintConVarFlags_t &info =  g_PrintConVarFlags[ i ];
		if ( var->IsFlagSet( info.flag ) )
		{
			char append[ 128 ];
			V_snprintf( append, sizeof( append ), " %s", info.desc );
			V_strncat( buf, append, bufsize, COPY_ALL_CHARACTERS );
		}
	}
}

static void AppendPrintf( char *buf, std::size_t bufsize, char const *fmt, ... )
{
	char scratch[ 1024 ];
	std::va_list argptr;
	__builtin_va_start( argptr, fmt );
	std::vsnprintf( scratch, sizeof( scratch ) - 1, fmt, argptr );
	__builtin_va_end( argptr );
	scratch[ sizeof( scratch ) - 1 ] = 0;

	V_strncat( buf, scratch, bufsize, COPY_ALL_CHARACTERS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ConVar_PrintDescription( const ConCommandBase *pVar )
{
	bool bMin, bMax;
	float fMin, fMax;
	const char *pStr;

	contract_assert( pVar );

	Color clr;
	clr.SetColor( 255, 100, 100, 255 );

	char outstr[ 4096 ];
	outstr[ 0 ] = 0;

	if ( !pVar->IsCommand() )
	{
		ConVar *var = ( ConVar * )pVar;
		const ConVar_ServerBounded *pBounded = ( var->Kind() == ConKind::VarServerBounded ) ? static_cast<const ConVar_ServerBounded*>( var ) : nullptr;

		bMin = var->GetMin( fMin );
		bMax = var->GetMax( fMax );

		const char *value = nullptr;
		char tempVal[ 32 ];

		if ( pBounded || var->IsFlagSet( FCVAR_NEVER_AS_STRING ) )
		{
			value = tempVal;
			
			int intVal = pBounded ? pBounded->GetInt() : var->GetInt();
			float floatVal = pBounded ? pBounded->GetFloat() : var->GetFloat();

			if ( std::fabs( (float)intVal - floatVal ) < 0.000001 )
			{
				V_snprintf( tempVal, sizeof( tempVal ), "%d", intVal );
			}
			else
			{
				V_snprintf( tempVal, sizeof( tempVal ), "%f", floatVal );
			}
		}
		else
		{
			value = var->GetString();
		}

		if ( value )
		{
			AppendPrintf( outstr, sizeof( outstr ), "\"%s\" = \"%s\"", var->GetName(), value );

			if ( V_stricmp( value, var->GetDefault() ) )
			{
				AppendPrintf( outstr, sizeof( outstr ), " ( def. \"%s\" )", var->GetDefault() );
			}
		}

		if ( bMin )
		{
			AppendPrintf( outstr, sizeof( outstr ), " min. %f", fMin );
		}
		if ( bMax )
		{
			AppendPrintf( outstr, sizeof( outstr ), " max. %f", fMax );
		}

		// Handle server-bounded cvars.
		if ( pBounded && std::fabs( pBounded->GetFloat() - var->GetFloat() ) > 0.0001f )
		{
			AppendPrintf( outstr, sizeof( outstr ), " [%.3f server clamped to %.3f]",
				var->GetFloat(), pBounded->GetFloat() );
		}
	}
	else
	{
		ConCommand *var = ( ConCommand * )pVar;

		AppendPrintf( outstr, sizeof( outstr ), "\"%s\" ", var->GetName() );
	}

	ConVar_AppendFlags( pVar, outstr, sizeof( outstr ) );

	pStr = pVar->GetHelpText();
	if ( pStr && *pStr )
	{
		ConMsg( "%-80s - %.80s\n", outstr, pStr );
	}
	else
	{
	 	ConMsg( "%-80s\n", outstr );
	}
}
