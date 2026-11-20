// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.cmdline;
import ks.core.color;
import ks.core.dbg;
import ks.core.scratcharena;
import ks.core.threadtools;
import ks.core.vprof;
import ks.platform;
import ks.simd;
import ks.tier1.byte_reader;
import ks.tier1.convar;
import ks.tier1.cvar;
import ks.tier1.fmtstr;
import ks.tier1.iappsystem;
import ks.tier1.iconvar;
import ks.tier1.icvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.registry;
import ks.tier1.string_ops;
import ks.tier1.strtools;
import ks.tier1.tier1;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
import ks.tier1.vector_ops;
import ks.vstdlib.cvar;
import std;





//-----------------------------------------------------------------------------
// Default linkage rule: any two ConVars may be aliased
//-----------------------------------------------------------------------------
static bool DefaultAreConVarsLinkable( const ConVar *child, const ConVar *parent )
{
	return true;
}

static FnConVarsLinkable_t s_fnLinkable = DefaultAreConVarsLinkable;


//-----------------------------------------------------------------------------
// Factor for CVars 
//-----------------------------------------------------------------------------
static ICvar s_Cvar;
static InterfaceReg s_Reg_ICvar( []() -> void * { return static_cast<ICvar *>( &s_Cvar ); }, CVAR_INTERFACE_VERSION );;


//-----------------------------------------------------------------------------
// Returns a CVar dictionary for tool usage
//-----------------------------------------------------------------------------
CreateInterfaceFn VStdLib_GetICVarFactory()
{
	return Sys_GetFactoryThis();
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
ICvar::ICvar() : m_TempConsoleBuffer( 1024 )
{
	m_nNextDLLIdentifier = 0;
	m_pConCommandList = nullptr;
	m_nMaxSplitScreenSlots = 1;
	m_bMaterialSystemThreadSetAllowed = false;
	m_CommandHash.Init();
}

//-----------------------------------------------------------------------------
// App system protocol
//-----------------------------------------------------------------------------
bool ICvar::Connect( CreateInterfaceFn factory )
{
	ConnectTier1Libraries( &factory, 1 );

	ConVar_Register();
	return true;
}

void ICvar::Disconnect()
{
	ConVar_Unregister();
	s_fnLinkable = DefaultAreConVarsLinkable;
	DisconnectTier1Libraries();

	contract_assert( m_SplitScreenAddedConVarsMap.empty() );
}

InitReturnVal_t ICvar::Init()
{
	return INIT_OK;
}

void ICvar::Shutdown()
{
}

void *ICvar::QueryInterface( const char *pInterfaceName )
{
	// We implement the ICvar interface
	if ( !V_strcmp( pInterfaceName, CVAR_INTERFACE_VERSION ) )
		return (ICvar*)this;

	return nullptr;
}


//-----------------------------------------------------------------------------
// Method allowing the engine's linkage rules to take over
//-----------------------------------------------------------------------------
void ICvar::InstallCVarQuery( FnConVarsLinkable_t fnLinkable )
{
	s_fnLinkable = fnLinkable;
}


//-----------------------------------------------------------------------------
// Used by DLLs to be able to unregister all their commands + convars 
//-----------------------------------------------------------------------------
CVarDLLIdentifier_t ICvar::AllocateDLLIdentifier()
{
	return m_nNextDLLIdentifier++;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *variable - 
//-----------------------------------------------------------------------------
void ICvar::RegisterConCommand( ConCommandBase *variable )
{
	// Already registered
	if ( variable->IsRegistered() )
		return;

	variable->m_bRegistered = true;

	const char *pName = variable->GetName();
	if ( !pName || !pName[0] )
	{
		variable->m_pNext = nullptr;
		return;
	}

	// If the variable is already defined, then setup the new variable as a proxy to it.
	const ConCommandBase *pOther = FindCommandBase( variable->GetName() );
	if ( pOther )
	{
		if ( variable->IsCommand() || pOther->IsCommand() )
		{
			{
				Warning( "WARNING: unable to link %s and %s because one or more is a ConCommand.\n", variable->GetName(), pOther->GetName() );
			}
		}
		else
		{
			// This cast is ok because we make sure they're ConVars above.
			ConVar *pChildVar = const_cast< ConVar* >( static_cast< const ConVar* >( variable ) );
			ConVar *pParentVar = const_cast< ConVar* >( static_cast< const ConVar* >( pOther ) );

			// See if it's a valid linkage
			if ( s_fnLinkable( pChildVar, pParentVar ) )
			{
				// Make sure the default values are the same (but only spew about this for FCVAR_REPLICATED)
				if(  pChildVar->m_pszDefaultValue && pParentVar->m_pszDefaultValue &&
					 pChildVar->IsFlagSet( FCVAR_REPLICATED ) && pParentVar->IsFlagSet( FCVAR_REPLICATED ) )
				{
					if( V_stricmp( pChildVar->m_pszDefaultValue, pParentVar->m_pszDefaultValue ) != 0 )
					{
						Warning( "Parent and child ConVars with different default values! %s child: %s parent: %s (parent wins)\n", 
							variable->GetName(), pChildVar->m_pszDefaultValue, pParentVar->m_pszDefaultValue );
					}
				}

				pChildVar->m_pParent = pParentVar->m_pParent;

				// Absorb material thread related convar flags
				pParentVar->m_nFlags |= pChildVar->m_nFlags & ( FCVAR_MATERIAL_THREAD_MASK | FCVAR_ACCESSIBLE_FROM_THREADS );

				// Transfer children's callbacks to parent
				if ( !pChildVar->m_fnChangeCallbacks.empty() )
				{
					for ( int i = 0; i < std::ssize( pChildVar->m_fnChangeCallbacks ); ++i )
					{
						pParentVar->m_fnChangeCallbacks.push_back( pChildVar->m_fnChangeCallbacks[ i ] );
					}
					// Wipe child callbacks
					pChildVar->m_fnChangeCallbacks.clear();
				}

				// make sure we don't have conflicting help strings.
				if ( pChildVar->m_pszHelpString && V_strlen( pChildVar->m_pszHelpString ) != 0 )
				{
					if ( pParentVar->m_pszHelpString && V_strlen( pParentVar->m_pszHelpString ) != 0 )
					{
						if ( V_stricmp( pParentVar->m_pszHelpString, pChildVar->m_pszHelpString ) != 0 )
						{
							Warning( "Convar %s has multiple help strings:\n\tparent (wins): \"%s\"\n\tchild: \"%s\"\n", 
								variable->GetName(), pParentVar->m_pszHelpString, pChildVar->m_pszHelpString );
						}
					}
					else
					{
						pParentVar->m_pszHelpString = pChildVar->m_pszHelpString;
					}
				}

				// make sure we don't have conflicting FCVAR_*** flags.
				static int const nFlags[] =
					{ FCVAR_CHEAT, FCVAR_REPLICATED, FCVAR_DONTRECORD, FCVAR_ARCHIVE, FCVAR_ARCHIVE_GAMECONSOLE };
				static char const * const szFlags[] =
					{ "FCVAR_CHEAT", "FCVAR_REPLICATED", "FCVAR_DONTRECORD", "FCVAR_ARCHIVE", "FCVAR_ARCHIVE_GAMECONSOLE" };

				static_assert( std::size( nFlags ) == std::size( szFlags ) );

				for ( int k = 0; k < std::size( nFlags ); ++ k )
				{
					if ( ( pChildVar->m_nFlags & nFlags[k] ) != ( pParentVar->m_nFlags & nFlags[k] ) )
					{
						Warning( "Convar %s has conflicting %s flags (child: %s%s, parent: %s%s, parent wins)\n", 
							variable->GetName(), szFlags[k],
							( pChildVar->m_nFlags & nFlags[k] ) ? "has " : "no ", szFlags[k],
							( pParentVar->m_nFlags & nFlags[k] ) ? "has " : "no ", szFlags[k] );
					}
				}
			}
		}

		variable->m_pNext = nullptr;
		return;
	}

	// link the variable in
	variable->m_pNext = m_pConCommandList;
	m_pConCommandList = variable;

	contract_assert( ( FindCommandBase(variable->GetName()) == nullptr ) && "Console command %s added twice!" );
	m_CommandHash.Insert(variable);

	if ( !variable->IsCommand() )
	{
		if ( const char *pValue = GetCommandLineValue( pName ) )
		{
			static_cast< ConVar * >( variable )->SetValue( pValue );
		}
	}
}

void ICvar::AddSplitScreenConVars()
{
	if ( m_nMaxSplitScreenSlots == 1 )
		return;

	for( ConCommandBase *pCommand = m_pConCommandList; pCommand; pCommand = pCommand->m_pNext )
	{
		if ( pCommand->IsCommand() )
			continue;

		ConVar *pConVar = static_cast< ConVar * >( pCommand );

		if ( !pConVar->IsFlagSet( FCVAR_SS ) )
			continue;

		// Maps it in if it isn't already
		SplitScreenAddedConVars_t &info = m_SplitScreenAddedConVarsMap[ pConVar ];
		for ( int i = 1 ; i < m_nMaxSplitScreenSlots; ++i )
		{
			// Already registered it
			if ( info.m_Vars[ i - 1 ].m_pVar )
				continue;

			// start at name2, etc.
			info.m_Vars[ i - 1 ].m_VarName = CFmtStr( "%s%d", pConVar->GetName(), i + 1 );

			CSplitScreenAddedConVar *pVar = new CSplitScreenAddedConVar( i, info.m_Vars[ i - 1 ].m_VarName.data(), pConVar );
			info.m_Vars[ i - 1 ].m_pVar = pVar;
			pVar->SetSplitScreenPlayerSlot( i );

			RegisterConCommand( pVar );
		}
	}

	ks::engine_registry().convars.m_pFirst = nullptr;
}

void ICvar::RemoveSplitScreenConVars( CVarDLLIdentifier_t id )

{
	if ( m_nMaxSplitScreenSlots == 1 )
	{
		contract_assert( m_SplitScreenAddedConVarsMap.empty() );
		return;
	}

	for ( auto it = m_SplitScreenAddedConVarsMap.begin(); it != m_SplitScreenAddedConVarsMap.end(); )
	{
		ConVar *key = it->first;

		if ( key->GetDLLIdentifier() != id )
		{
			++it;
			continue;
		}
		
		SplitScreenAddedConVars_t &info = it->second;

 

		for ( int i = 1 ; i < m_nMaxSplitScreenSlots; ++i )
		{

			if ( info.m_Vars[ i - 1 ].m_pVar )
			{
				UnregisterConCommand( info.m_Vars[ i - 1 ].m_pVar );
				delete info.m_Vars[ i - 1 ].m_pVar;
				info.m_Vars[ i - 1 ].m_pVar = nullptr;
			}
		}
		it = m_SplitScreenAddedConVarsMap.erase( it );
	}
}

void ICvar::UnregisterConCommand( ConCommandBase *pCommandToRemove )
{
	// Not registered? Don't bother
	if ( !pCommandToRemove->IsRegistered() )
		return;

	pCommandToRemove->m_bRegistered = false;

	// FIXME: Should we make this a doubly-linked list? Would remove faster
	ConCommandBase *pPrev = nullptr;
	for( ConCommandBase *pCommand = m_pConCommandList; pCommand; pCommand = pCommand->m_pNext )
	{
		if ( pCommand != pCommandToRemove )
		{
			pPrev = pCommand;
			continue;
		}

		if ( pPrev == nullptr )
		{
			m_pConCommandList = pCommand->m_pNext;
		}
		else
		{
			pPrev->m_pNext = pCommand->m_pNext;
		}
		pCommand->m_pNext = nullptr;
		m_CommandHash.Remove(m_CommandHash.Find(pCommand));
		break;
	}
}

void ICvar::UnregisterConCommands( CVarDLLIdentifier_t id )
{
	ConCommandBase	*pNewList;
	ConCommandBase  *pCommand, *pNext;

	pNewList = nullptr;

	m_CommandHash.Purge( true );
	pCommand = m_pConCommandList;
	while ( pCommand )
	{
		pNext = pCommand->m_pNext;
		if ( pCommand->GetDLLIdentifier() != id )
		{
			pCommand->m_pNext = pNewList;
			pNewList = pCommand;

			m_CommandHash.Insert( pCommand );
		}
		else
		{
			// Unlink
			pCommand->m_bRegistered = false;
			pCommand->m_pNext = nullptr;

		}

		pCommand = pNext;
	}

	m_pConCommandList = pNewList;
}


//-----------------------------------------------------------------------------
// Finds base commands 
//-----------------------------------------------------------------------------
const ConCommandBase *ICvar::FindCommandBase( const char *name ) const
{
	VProfIncrementCounter<"ICvar::FindCommandBase">( 1 );
	CVProfScope _( "ICvar::FindCommandBase", 0, VPROF_BUDGETGROUP_CVAR_FIND, false, BUDGETFLAG_OTHER );

	return m_CommandHash.FindPtr( name );
}

ConCommandBase *ICvar::FindCommandBase( const char *name )
{
	VProfIncrementCounter<"ICvar::FindCommandBase">( 1 );
	CVProfScope _( "ICvar::FindCommandBase", 0, VPROF_BUDGETGROUP_CVAR_FIND, false, BUDGETFLAG_OTHER );

	return m_CommandHash.FindPtr( name );
}


//-----------------------------------------------------------------------------
// Purpose Finds ConVars
//-----------------------------------------------------------------------------
const ConVar *ICvar::FindVar( const char *var_name ) const
{
	const ConCommandBase *var = FindCommandBase( var_name );
	if ( !var )
	{
		return nullptr;
	}
	else
	{
		if (var->IsCommand())
		{
			Warning("Tried to look up command %s as if it were a variable.\n", var_name );
			return nullptr;
		}
	}
	
	return static_cast<const ConVar*>(var);
}

ConVar *ICvar::FindVar( const char *var_name )
{
	ConCommandBase *var = FindCommandBase( var_name );
	if ( !var )
	{
		return nullptr;
	}
	else
	{
		if (var->IsCommand())
		{
			Warning("Tried to look up command %s as if it were a variable.\n", var_name );
			return nullptr;
		}
	}
	
	return static_cast<ConVar*>( var );
}


//-----------------------------------------------------------------------------
// Purpose Finds ConCommands
//-----------------------------------------------------------------------------
const ConCommand *ICvar::FindCommand( const char *pCommandName ) const
{
	const ConCommandBase *var = FindCommandBase( pCommandName );
	if ( !var || !var->IsCommand() )
		return nullptr;

	return static_cast<const ConCommand*>(var);
}

ConCommand *ICvar::FindCommand( const char *pCommandName )
{
	ConCommandBase *var = FindCommandBase( pCommandName );
	if ( !var || !var->IsCommand() )
		return nullptr;

	return static_cast<ConCommand*>( var );
}


const char* ICvar::GetCommandLineValue( const char *pVariableName )
{
	ks::ScratchArena<> scratch;
	int nLen = V_strlen(pVariableName);
	char *pSearch = (char*)scratch.AllocBytes( nLen + 2 );
	pSearch[0] = '+';
	std::memcpy( &pSearch[1], pVariableName, nLen + 1 );
	return CommandLine()->ParmValue( pSearch );
}

//-----------------------------------------------------------------------------
// Install, remove global callbacks
//-----------------------------------------------------------------------------
void ICvar::InstallGlobalChangeCallback( FnChangeCallback_t callback )
{
	contract_assert( callback && ks::find_index( m_GlobalChangeCallbacks, callback ) < 0 );
	m_GlobalChangeCallbacks.push_back( callback );
}

void ICvar::RemoveGlobalChangeCallback( FnChangeCallback_t callback )
{
	contract_assert( callback );
	ks::erase_first( m_GlobalChangeCallbacks, callback );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ICvar::CallGlobalChangeCallbacks( ConVar *var, const char *pOldString, float flOldValue )
{
	int nCallbackCount = std::ssize( m_GlobalChangeCallbacks );
	for ( int i = 0; i < nCallbackCount; ++i )
	{
		(*m_GlobalChangeCallbacks[i])( var, pOldString, flOldValue );
	}
}


//-----------------------------------------------------------------------------
// Sets convars containing the flags to their default value
//-----------------------------------------------------------------------------
void ICvar::RevertFlaggedConVars( int nFlag )
{
	for ( CConCommandHash::CCommandHashIterator_t i = m_CommandHash.First() ;
		  m_CommandHash.IsValidIterator( i ) ; 
		  i = m_CommandHash.Next( i ) )
	{
		ConCommandBase *var = m_CommandHash[ i ];
		if ( var->IsCommand() )
			continue;

		ConVar *cvar = ( ConVar * )var;

		if ( !cvar->IsFlagSet( nFlag ) )
			continue;

		// It's == to the default value, don't count
		if ( !V_stricmp( cvar->GetDefault(), cvar->GetString() ) )
			continue;

		cvar->Revert();
		// DevMsg( "%s = \"%s\" (reverted)\n", cvar->GetName(), cvar->GetString() );
	}
}


//-----------------------------------------------------------------------------
// Deal with queued material system convars
//-----------------------------------------------------------------------------
bool ICvar::IsMaterialThreadSetAllowed( ) const
{
	contract_assert( ThreadInMainThread() );
	return m_bMaterialSystemThreadSetAllowed;
}

void ICvar::QueueMaterialThreadSetValue( ConVar *pConVar, const char *pValue )
{
	contract_assert( ThreadInMainThread() );
	int j = std::ssize( m_QueuedConVarSets ); m_QueuedConVarSets.emplace_back();
	m_QueuedConVarSets[j].m_pConVar = pConVar;
	m_QueuedConVarSets[j].m_nType = CONVAR_SET_STRING;
	m_QueuedConVarSets[j].m_String = ks::nz( pValue );
}

void ICvar::QueueMaterialThreadSetValue( ConVar *pConVar, int nValue )
{
	contract_assert( ThreadInMainThread() );
	int j = std::ssize( m_QueuedConVarSets ); m_QueuedConVarSets.emplace_back();
	m_QueuedConVarSets[j].m_pConVar = pConVar;
	m_QueuedConVarSets[j].m_nType = CONVAR_SET_INT;
	m_QueuedConVarSets[j].m_nInt = nValue;
}

void ICvar::QueueMaterialThreadSetValue( ConVar *pConVar, float flValue )
{
	contract_assert( ThreadInMainThread() );
	int j = std::ssize( m_QueuedConVarSets ); m_QueuedConVarSets.emplace_back();
	m_QueuedConVarSets[j].m_pConVar = pConVar;
	m_QueuedConVarSets[j].m_nType = CONVAR_SET_FLOAT;
	m_QueuedConVarSets[j].m_flFloat = flValue;
}

bool ICvar::HasQueuedMaterialThreadConVarSets() const
{
	contract_assert( ThreadInMainThread() );
	return !m_QueuedConVarSets.empty();
}

int ICvar::ProcessQueuedMaterialThreadConVarSets()
{
	contract_assert( ThreadInMainThread() );
	m_bMaterialSystemThreadSetAllowed = true;

	int nUpdateFlags = 0;
	int nCount = std::ssize( m_QueuedConVarSets );
	for ( int i = 0; i < nCount; ++i )
	{
		const QueuedConVarSet_t& set = m_QueuedConVarSets[i];
		switch( set.m_nType )
		{
		case CONVAR_SET_FLOAT:
			set.m_pConVar->SetValue( set.m_flFloat );
			break;
		case CONVAR_SET_INT:
			set.m_pConVar->SetValue( set.m_nInt );
			break;
		case CONVAR_SET_STRING:
			set.m_pConVar->SetValue( set.m_String.c_str() );
			break;
		}

		nUpdateFlags |= set.m_pConVar->GetFlags() & FCVAR_MATERIAL_THREAD_MASK;
	}

	m_QueuedConVarSets.clear(); 
	m_bMaterialSystemThreadSetAllowed = false;
	return nUpdateFlags;
}


//-----------------------------------------------------------------------------
// Display queued messages
//-----------------------------------------------------------------------------
void ICvar::DisplayQueuedMessages( )
{
	// Display any queued up messages
	if ( m_TempConsoleBuffer.size() == 0 )
		return;

	Color clr;
	ks::byte_reader queued = m_TempConsoleBuffer.reader();
	while( queued.remaining() > 0 )
	{
		int nType = queued.read< char >();
		if ( nType == CONSOLE_COLOR_PRINT )
		{
			clr.SetRawColor( queued.read< int >() );
		}

		const std::string_view message = queued.read_cstring();
		const char *pTemp = message.data();

		switch( nType )
		{
		case CONSOLE_COLOR_PRINT:
			ConsoleColorPrintf( clr, "%s", pTemp );
			break;

		case CONSOLE_PRINT:
			ConsolePrintf( "%s", pTemp );
			break;

		case CONSOLE_DPRINT:
			ConsoleDPrintf( "%s", pTemp );
			break;
		}
	}

	m_TempConsoleBuffer.clear();
}


//-----------------------------------------------------------------------------
// Install a console printer
//-----------------------------------------------------------------------------
void ICvar::InstallConsoleDisplayFunc( const void *pOwner, ConsoleDisplayFn_t fnDisplay )
{
	contract_assert( std::ranges::none_of( m_DisplayFuncs, [pOwner]( const ConsoleDisplay_t &d ) { return d.m_pOwner == pOwner; } ) );
	m_DisplayFuncs.push_back( ConsoleDisplay_t{ pOwner, fnDisplay } );
	DisplayQueuedMessages();
}

void ICvar::RemoveConsoleDisplayFunc( const void *pOwner )
{
	auto it = std::ranges::find( m_DisplayFuncs, pOwner, &ConsoleDisplay_t::m_pOwner );
	if ( it != m_DisplayFuncs.end() )
	{
		m_DisplayFuncs.erase( it );
	}
}

void ICvar::ConsoleColorPrintf( const Color& clr, const char *pFormat, ... ) const
{
	char temp[ 8192 ];
	std::va_list argptr;
	__builtin_va_start( argptr, pFormat );
	std::vsnprintf( temp, sizeof( temp ) - 1, pFormat, argptr );
	__builtin_va_end( argptr );
	temp[ sizeof( temp ) - 1 ] = 0;

	int c = std::ssize( m_DisplayFuncs );
	if ( c == 0 )
	{
		m_TempConsoleBuffer.write< char >( CONSOLE_COLOR_PRINT );
		m_TempConsoleBuffer.write< int >( clr.GetRawColor() );
		m_TempConsoleBuffer.write_cstring( temp );
		return;
	}

	for ( int i = 0 ; i < c; ++i )
	{
		m_DisplayFuncs[ i ].m_fnDisplay( ConsoleDisplayOp_t::ColorPrint, clr, temp );
	}
}

void ICvar::ConsolePrintf( const char *pFormat, ... ) const
{
	char temp[ 8192 ];
	std::va_list argptr;
	__builtin_va_start( argptr, pFormat );
	std::vsnprintf( temp, sizeof( temp ) - 1, pFormat, argptr );
	__builtin_va_end( argptr );
	temp[ sizeof( temp ) - 1 ] = 0;

	int c = std::ssize( m_DisplayFuncs );
	if ( c == 0 )
	{
		m_TempConsoleBuffer.write< char >( CONSOLE_PRINT );
		m_TempConsoleBuffer.write_cstring( temp );
		return;
	}

	for ( int i = 0 ; i < c; ++i )
	{
		m_DisplayFuncs[ i ].m_fnDisplay( ConsoleDisplayOp_t::Print, Color(), temp );
	}
}

void ICvar::ConsoleDPrintf( const char *pFormat, ... ) const
{
	char temp[ 8192 ];
	std::va_list argptr;
	__builtin_va_start( argptr, pFormat );
	std::vsnprintf( temp, sizeof( temp ) - 1, pFormat, argptr );
	__builtin_va_end( argptr );
	temp[ sizeof( temp ) - 1 ] = 0;

	int c = std::ssize( m_DisplayFuncs );
	if ( c == 0 )
	{
		m_TempConsoleBuffer.write< char >( CONSOLE_DPRINT );
		m_TempConsoleBuffer.write_cstring( temp );
		return;
	}

	for ( int i = 0 ; i < c; ++i )
	{
		m_DisplayFuncs[ i ].m_fnDisplay( ConsoleDisplayOp_t::DPrint, Color(), temp );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------


struct ConVarSortLess
{
	bool operator()( const ConCommandBase *lhs, const ConCommandBase *rhs ) const
	{
		return V_stricmp( lhs->GetName(), rhs->GetName() ) < 0;
	}
};

//-----------------------------------------------------------------------------
// Console commands
//-----------------------------------------------------------------------------
void ICvar::Find( const CCommand &args )
{
	const char *search = nullptr;
	const char *subSearch = nullptr;

	//lwss: Add support for a 2nd string in the search. I'm used to this feature in source 2, it's useful.
	if ( args.ArgC() != 2 && args.ArgC() != 3 )
	{
		ConMsg( "Usage:  find <string> [<string>...]\n" );
		return;
	}

	// Get substring to find
	search = args[1];
	if( args.ArgC() == 3 )
	    subSearch = args[2];

	std::multiset< ConCommandBase *, ConVarSortLess > sorted;
				 
	// Loop through vars and print out findings
	for ( CConCommandHash::CCommandHashIterator_t i = m_CommandHash.First() ;
		m_CommandHash.IsValidIterator(i) ; 
		i = m_CommandHash.Next(i) )
	{
		ConCommandBase *var = m_CommandHash[ i ];
		if ( var->IsFlagSet(FCVAR_DEVELOPMENTONLY) || var->IsFlagSet(FCVAR_HIDDEN) )
			continue;

		if ( !V_stristr( var->GetName(), search ) && !V_stristr( var->GetHelpText(), search ) )
			continue;

        if( subSearch && ( !V_stristr( var->GetName(), subSearch ) && !V_stristr( var->GetHelpText(), subSearch ) ) )
            continue;

		sorted.insert( var );
	}	

	for ( ConCommandBase *var : sorted )
	{
		ConVar_PrintDescription( var );	
	}
}



void ICvar::SetMaxSplitScreenSlots( int nSlots )
{
	m_nMaxSplitScreenSlots = nSlots;

	AddSplitScreenConVars();
}

int ICvar::GetMaxSplitScreenSlots() const
{
	return m_nMaxSplitScreenSlots;
}



//-----------------------------------------------------------------------------
// Console command hash data structure
//-----------------------------------------------------------------------------
CConCommandHash::CConCommandHash()
{
	Purge( true );
}

CConCommandHash::~CConCommandHash()
{
	Purge( false );
}

void CConCommandHash::Purge( bool bReinitialize )
{
	m_aBuckets.clear(); m_aBuckets.shrink_to_fit();
	if ( bReinitialize )
	{
		Init();
	}
}

// Initialize.
void CConCommandHash::Init( void )
{
	// kNUM_BUCKETS must be a power of two.
	static_assert( (kNUM_BUCKETS & ( kNUM_BUCKETS - 1 )) == 0 );

	// Set the bucket size.
	m_aBuckets.assign( kNUM_BUCKETS, {} );
}

//-----------------------------------------------------------------------------
// Purpose: Insert data into the hash table given its key (unsigned int), 
//			WITH a check to see if the element already exists within the hash.
//-----------------------------------------------------------------------------
CConCommandHash::CCommandHashHandle_t CConCommandHash::Insert( ConCommandBase *cmd )
{
	// Check to see if that key already exists in the buckets (should be unique).
	CCommandHashHandle_t hHash = Find( cmd );
	if( hHash != InvalidHandle() )
		return hHash;

	return FastInsert( cmd );
}
//-----------------------------------------------------------------------------
// Purpose: Insert data into the hash table given its key (unsigned int),
//          WITHOUT a check to see if the element already exists within the hash.
//-----------------------------------------------------------------------------
CConCommandHash::CCommandHashHandle_t CConCommandHash::FastInsert( ConCommandBase *cmd )
{
	HashKey_t key = Hash(cmd);

	// Link element at the head of its bucket.
	int iBucket = key & kBUCKETMASK ; // HashFuncs::Hash( uiKey, m_uiBucketMask );
	return ToHandle( m_aBuckets[iBucket].emplace_front( key, cmd ) );
}

//-----------------------------------------------------------------------------
// Purpose: Remove a given element from the hash.
//-----------------------------------------------------------------------------
void CConCommandHash::Remove( CCommandHashHandle_t hHash ) __restrict__
{
	if ( !IsValidHandle( hHash ) )
		return;

	const HashEntry_t *entry = FromHandle( hHash );
	bucket_t &bucket = m_aBuckets[entry->m_uiKey & kBUCKETMASK];
	for ( auto it = bucket.begin(); it != bucket.end(); ++it )
	{
		if ( &*it == entry )
		{
			bucket.erase( it );
			return;
		}
	}
	contract_assert( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Remove all elements from the hash
//-----------------------------------------------------------------------------
void CConCommandHash::RemoveAll( void )
{
	m_aBuckets.clear();
}

//-----------------------------------------------------------------------------
// Find hash entry corresponding to a string name
//-----------------------------------------------------------------------------
CConCommandHash::CCommandHashHandle_t CConCommandHash::Find( const char *name, HashKey_t hashkey) const __restrict__
{
	// hash the "key" - get the correct hash table "bucket"
	int iBucket = hashkey & kBUCKETMASK;

	for ( const HashEntry_t &element : m_aBuckets[iBucket] )
	{
		if ( element.m_uiKey == hashkey && // if hashes of strings match,
			 V_stricmp( name, element.m_Data->GetName() ) == 0) // then test the actual strings
		{
			return ToHandle( element );
		}
	}

	// found nuffink
	return InvalidHandle();
}

//-----------------------------------------------------------------------------
// Find a command in the hash.
//-----------------------------------------------------------------------------
CConCommandHash::CCommandHashHandle_t CConCommandHash::Find( const ConCommandBase *cmd ) const __restrict__
{
	// Set this #if to 1 if the assert at bottom starts whining --
	// that indicates that a console command is being double-registered,
	// or something similarly nonfatally bad. With this #if 1, we'll search
	// by name instead of by pointer, which is more robust in the face
	// of double registered commands, but obviously slower.
	HashKey_t hashkey = Hash(cmd);
	int iBucket = hashkey & kBUCKETMASK;

	// hunt through all entries in that bucket
	for ( const HashEntry_t &element : m_aBuckets[iBucket] )
	{
		if ( element.m_uiKey == hashkey && // if the hashes match... 
			 element.m_Data  == cmd	) // and the pointers...
		{
			// in debug, test to make sure we don't have commands under the same name
			// or something goofy like that
			contract_assert( ( ToHandle( element ) == Find(cmd->GetName()) ) && "ConCommand %s had two entries in the hash!" );
			
			// return this element
			return ToHandle( element );
		}
	}

	// found nothing.
	return InvalidHandle();
}


