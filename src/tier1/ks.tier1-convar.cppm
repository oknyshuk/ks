// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1.convar;

import std;
import ks.core.basetypes;
import ks.core.color;
import ks.core.types;
import ks.core.vprof;
import ks.platform;
import ks.simd;
export import ks.tier1.utlstring;
export import ks.tier1.icvar;

export extern "C++" {

//-----------------------------------------------------------------------------
// Uncomment me to test for threading issues for material system convars
// NOTE: You want to disable all threading when you do this
// +host_thread_mode 0 +r_threaded_particles 0 +sv_parallel_packentities 0 +sv_disable_querycache 0
//-----------------------------------------------------------------------------
//#define CONVAR_TEST_MATERIAL_THREAD_CONVARS 1

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class ConVar;
class CCommand;
class ConCommand;
class ConCommandBase;
namespace ks { struct registry; }

// This performs a simple, reversible obscuring of the floating point value passed in with
// the obscuring val. This just ensures that we don't see the exact bits of fVal in memory 
// when scanned by CheatEngine, etc.
// Note that this function is actually called StoreValue by the compiler due to the preprocessor
// define above.
[[gnu::always_inline]] inline float StoreValue( float fVal, int nObscuring )
{
	union FloatIntUnion_t
	{
		float fValue;
		int   nValue;
	} a;

	a.fValue = fVal;
	a.nValue ^= nObscuring;

	return a.fValue;
}

// Just masks the value to make it harder to find in memory using Cheat Engine.
// Note that this function is actually called StoreValue by the compiler due to the preprocessor
// define above.
[[gnu::always_inline]] inline int StoreValue( int nVal, int nObscuring )
{
	return nVal ^ nObscuring;
}

//-----------------------------------------------------------------------------
// Sources of console commands
//-----------------------------------------------------------------------------
enum cmd_source_t
{
	// Added to the console buffer by gameplay code.  Generally unrestricted.
	kCommandSrcCode,

	// Sent from code via engine->ClientCmd, which is restricted to commands visible
	// via FCVAR_CLIENTCMD_CAN_EXECUTE.
	kCommandSrcClientCmd,

	// Typed in at the console or via a user key-bind.  Generally unrestricted, although
	// the client will throttle commands sent to the server this way to 16 per second.
	kCommandSrcUserInput,

	// Came in over a net connection as a clc_stringcmd
	// host_client will be valid during this state.
	//
	// Restricted to FCVAR_GAMEDLL commands (but not convars) and special non-ConCommand
	// server commands hardcoded into gameplay code (e.g. "joingame")
	kCommandSrcNetClient,

	// Received from the server as the client
	//
	// Restricted to commands with FCVAR_SERVER_CAN_EXECUTE
	kCommandSrcNetServer,

	// Being played back from a demo file
	//
	// Not currently restricted by convar flag, but some commands manually ignore calls
	// from this source.  FIXME: Should be heavily restricted as demo commands can come
	// from untrusted sources.
	kCommandSrcDemoFile,

	// Invalid value used when cleared
	kCommandSrcInvalid = -1
};

//-----------------------------------------------------------------------------
// Any executable that wants to use ConVars can supply one of these to hook up
// access to console variables: it is called for every ConCommandBase as it is registered.
//-----------------------------------------------------------------------------
typedef bool ( *FnConCommandBaseAccessor_t )( ConCommandBase *pVar );

//-----------------------------------------------------------------------------
// The ConVars and ConCommands one component (the engine, or a game side) defines as statics.
// They are collected here while the statics are constructed and registered with the ICvar by
// ConVar_Register(); every registry has its own DLL identifier, so a component can take its
// commands out again.
//-----------------------------------------------------------------------------
class ConCommandRegistry
{
public:
	constexpr ConCommandRegistry() = default;

	ConCommandBase				*m_pFirst = nullptr;	// constructed, not registered yet
	FnConCommandBaseAccessor_t	m_pAccessor = nullptr;	// set once registered
	int							m_nDLLIdentifier = -1;
	bool						m_bRegistered = false;
};

//-----------------------------------------------------------------------------
// Helper method for console development
//-----------------------------------------------------------------------------
inline void ConVar_PublishToVXConsole() {}

//-----------------------------------------------------------------------------
// Called when a ConCommand needs to execute
//-----------------------------------------------------------------------------
typedef void ( *FnCommandCallbackV1_t )( void );
typedef void ( *FnCommandCallback_t )( const CCommand &command );

inline constexpr int COMMAND_COMPLETION_MAXITEMS = 64;
inline constexpr int COMMAND_COMPLETION_ITEM_LENGTH = 64;

//-----------------------------------------------------------------------------
// Returns 0 to COMMAND_COMPLETION_MAXITEMS worth of completion strings
//-----------------------------------------------------------------------------
typedef int  ( *FnCommandCompletionCallback )( const char *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] );

//-----------------------------------------------------------------------------
// Owning callables for commands that need to carry state (see CConCommandMemberAccessor)
//-----------------------------------------------------------------------------
typedef std::move_only_function< void ( const CCommand &command ) > ConCommandCallback_t;
typedef std::move_only_function< int ( const char *pPartial, std::vector< std::string > &commands ) > ConCommandCompletionCallback_t;

//-----------------------------------------------------------------------------
// ConCommandBase is a closed family: a ConCommand, or one of the ConVar flavours.
// The kind tag selects the behaviour of the members they share.
//-----------------------------------------------------------------------------
enum class ConKind : unsigned char
{
	Command,				// ConCommand
	Var,					// plain ConVar
	VarEmpty,				// CEmptyConVar: reads as "", ignores writes
	VarServerBounded,		// ConVar_ServerBounded: the value is bounded by the server
	VarSplitScreenAdded,	// CSplitScreenAddedConVar: "name2".."nameN"
};

//-----------------------------------------------------------------------------
// Purpose: The base console invoked command/cvar interface
//-----------------------------------------------------------------------------
class ConCommandBase
{
	friend class ICvar;
	friend class ConVar;
	friend class ConCommand;
	friend void ConVar_Register( ConCommandRegistry &registry, int nCVarFlag, FnConCommandBaseAccessor_t pAccessor );
	friend void ConVar_PublishToVXConsole();

public:
								// Registers with the engine's registry, or with the given one
								// (a game side passes its own: side_registry()).
								ConCommandBase( void );
	explicit					ConCommandBase( ks::registry &registry );
								ConCommandBase( const char *pName, const char *pHelpString = 0, 
									int flags = 0 );

								~ConCommandBase( void );

	bool						IsCommand( void ) const { return m_Kind == ConKind::Command; }
	ConKind						Kind( void ) const { return m_Kind; }

	// Check flag
	bool						IsFlagSet( int flag ) const;
	// Set flag
	void						AddFlags( int flags );
	// Clear flag
	void						RemoveFlags( int flags );

	int							GetFlags() const;

	// Return name of cvar
	const char					*GetName( void ) const;

	// Return help text for cvar
	const char					*GetHelpText( void ) const;

	// Deal with next pointer
	const ConCommandBase		*GetNext( void ) const;
	ConCommandBase				*GetNext( void );
	
	bool						IsRegistered( void ) const;

	// Returns the DLL identifier
	CVarDLLIdentifier_t			GetDLLIdentifier() const;

protected:
	void						Create( const char *pName, const char *pHelpString = 0, 
									int flags = 0 );

	// Used internally by OneTimeInit to initialize/shutdown
	void						Init();
	void						Shutdown();

	// Internal copy routine ( uses new operator from correct module )
	char						*CopyString( const char *from );

	void						SetKind( ConKind kind ) { m_Kind = kind; }

private:
	// The registry this was constructed into
	ConCommandRegistry			*m_pRegistry;

	// Next ConVar in chain
	// Prior to register, it points to the next convar in the DLL.
	// Once registered, though, m_pNext is reset to point to the next
	// convar in the global list
	ConCommandBase				*m_pNext;

	// Has the cvar been added to the global list?
	bool						m_bRegistered;

	// Which member of the family this is
	ConKind						m_Kind;

	// Static data
	const char 					*m_pszName;
	const char 					*m_pszHelpString;
	
	// ConVar flags
	int							m_nFlags;

};

//-----------------------------------------------------------------------------
// Command tokenizer
//-----------------------------------------------------------------------------
class CCommand
{
public:
	CCommand();
	CCommand( int nArgC, const char **ppArgV, cmd_source_t source = kCommandSrcCode );
	bool Tokenize( const char *pCommand, cmd_source_t source = kCommandSrcCode, characterset_t *pBreakSet = nullptr );
	void Reset();

	int ArgC() const;
	const char **ArgV() const;
	const char *ArgS() const;					// All args that occur after the 0th arg, in string form
	const char *GetCommandString() const;		// The entire command in string form, including the 0th arg
	const char *operator[]( int nIndex ) const;	// Gets at arguments
	const char *Arg( int nIndex ) const;		// Gets at arguments
	cmd_source_t Source() const;				// Find where this command was sent from
	
	// Helper functions to parse arguments to commands.
	const char* FindArg( const char *pName ) const;
	int FindArgInt( const char *pName, int nDefaultVal ) const;

	static int MaxCommandLength();
	static characterset_t* DefaultBreakSet();

private:
	enum
	{
		COMMAND_MAX_ARGC = 64,
		COMMAND_MAX_LENGTH = 512,
	};

	int		m_nArgc;
	int		m_nArgv0Size;
	char	m_pArgSBuffer[ COMMAND_MAX_LENGTH ];
	char	m_pArgvBuffer[ COMMAND_MAX_LENGTH ];
	const char*	m_ppArgv[ COMMAND_MAX_ARGC ];
	cmd_source_t m_source;
};

inline int CCommand::MaxCommandLength()
{
	return COMMAND_MAX_LENGTH - 1;
}

inline int CCommand::ArgC() const
{
	return m_nArgc;
}

inline const char **CCommand::ArgV() const
{
	return m_nArgc ? (const char**)m_ppArgv : nullptr;
}

inline const char *CCommand::ArgS() const
{
	return m_nArgv0Size ? &m_pArgSBuffer[m_nArgv0Size] : "";
}

inline const char *CCommand::GetCommandString() const
{
	return m_nArgc ? m_pArgSBuffer : "";
}

inline const char *CCommand::Arg( int nIndex ) const
{
	// FIXME: Many command handlers appear to not be particularly careful
	// about checking for valid argc range. For now, we're going to
	// do the extra check and return an empty string if it's out of range
	if ( nIndex < 0 || nIndex >= m_nArgc )
		return "";
	return m_ppArgv[nIndex];
}

inline const char *CCommand::operator[]( int nIndex ) const
{
	return Arg( nIndex );
}

inline cmd_source_t CCommand::Source() const
{
	return m_source;
}

//-----------------------------------------------------------------------------
// Purpose: The console invoked command
//-----------------------------------------------------------------------------
class ConCommand : public ConCommandBase
{
friend class ICvar;

public:
	typedef ConCommandBase BaseClass;

	ConCommand( const char *pName, FnCommandCallbackV1_t callback, 
		const char *pHelpString = 0, int flags = 0, FnCommandCompletionCallback completionFunc = 0 );
	ConCommand( const char *pName, FnCommandCallback_t callback, 
		const char *pHelpString = 0, int flags = 0, FnCommandCompletionCallback completionFunc = 0 );
	ConCommand( ks::registry &registry, const char *pName, FnCommandCallbackV1_t callback, 
		const char *pHelpString = 0, int flags = 0, FnCommandCompletionCallback completionFunc = 0 );
	ConCommand( ks::registry &registry, const char *pName, FnCommandCallback_t callback, 
		const char *pHelpString = 0, int flags = 0, FnCommandCompletionCallback completionFunc = 0 );
	// For commands that carry state: any callable that cannot be a plain function pointer.
	template < typename F >
		requires ( std::is_invocable_r_v< void, F &, const CCommand & >
			&& !std::is_convertible_v< F, FnCommandCallback_t >
			&& !std::is_convertible_v< F, FnCommandCallbackV1_t > )
	ConCommand( const char *pName, F &&callback, 
		const char *pHelpString = 0, int flags = 0, ConCommandCompletionCallback_t completionCallback = ConCommandCompletionCallback_t() )
		: ConCommand( StatefulTag_t(), pName, ConCommandCallback_t( std::forward< F >( callback ) ), pHelpString, flags, std::move( completionCallback ) )
	{
	}
	template < typename F >
		requires ( std::is_invocable_r_v< void, F &, const CCommand & >
			&& !std::is_convertible_v< F, FnCommandCallback_t >
			&& !std::is_convertible_v< F, FnCommandCallbackV1_t > )
	ConCommand( ks::registry &registry, const char *pName, F &&callback, 
		const char *pHelpString = 0, int flags = 0, ConCommandCompletionCallback_t completionCallback = ConCommandCompletionCallback_t() )
		: ConCommand( StatefulTag_t(), registry, pName, ConCommandCallback_t( std::forward< F >( callback ) ), pHelpString, flags, std::move( completionCallback ) )
	{
	}

	~ConCommand( void );

	int AutoCompleteSuggest( const char *partial, std::vector< std::string > &commands );

	bool CanAutoComplete( void );

	// Invoke the function
	void Dispatch( const CCommand &command );

private:
	struct StatefulTag_t {};
	ConCommand( StatefulTag_t, const char *pName, ConCommandCallback_t callback, 
		const char *pHelpString, int flags, ConCommandCompletionCallback_t completionCallback );
	ConCommand( StatefulTag_t, ks::registry &registry, const char *pName, ConCommandCallback_t callback, 
		const char *pHelpString, int flags, ConCommandCompletionCallback_t completionCallback );

	// Call this function when executing the command
	ConCommandCallback_t m_fnCallback;

	// Empty when the command has no autocompletion
	ConCommandCompletionCallback_t m_fnCompletion;
};

//-----------------------------------------------------------------------------
// Purpose: A console variable
//-----------------------------------------------------------------------------
class ConVar : public ConCommandBase
{
friend class ICvar;
friend class ConVarRef;
friend class SplitScreenConVarRef;

public:
	typedef ConCommandBase BaseClass;

								ConVar( const char *pName, const char *pDefaultValue, int flags = 0);

								ConVar( const char *pName, const char *pDefaultValue, int flags, 
									const char *pHelpString );
								ConVar( const char *pName, const char *pDefaultValue, int flags, 
									const char *pHelpString, bool bMin, float fMin, bool bMax, float fMax );
								ConVar( const char *pName, const char *pDefaultValue, int flags, 
									const char *pHelpString, FnChangeCallback_t callback );
								ConVar( const char *pName, const char *pDefaultValue, int flags, 
									const char *pHelpString, bool bMin, float fMin, bool bMax, float fMax,
									FnChangeCallback_t callback );
								ConVar( ks::registry &registry, const char *pName, const char *pDefaultValue, int flags = 0);

								ConVar( ks::registry &registry, const char *pName, const char *pDefaultValue, int flags, 
									const char *pHelpString );
								ConVar( ks::registry &registry, const char *pName, const char *pDefaultValue, int flags, 
									const char *pHelpString, bool bMin, float fMin, bool bMax, float fMax );
								ConVar( ks::registry &registry, const char *pName, const char *pDefaultValue, int flags, 
									const char *pHelpString, FnChangeCallback_t callback );
								ConVar( ks::registry &registry, const char *pName, const char *pDefaultValue, int flags, 
									const char *pHelpString, bool bMin, float fMin, bool bMax, float fMax,
									FnChangeCallback_t callback );

								~ConVar( void );

	bool						IsFlagSet( int flag ) const;
	const char*					GetHelpText( void ) const;
	bool						IsRegistered( void ) const;
	const char					*GetName( void ) const;
	// Return name of command (usually == GetName(), except in case of FCVAR_SS_ADDED vars
	const char					*GetBaseName( void ) const;
	int							GetSplitScreenPlayerSlot() const;

	void						AddFlags( int flags );
	int							GetFlags() const;

	// Install a change callback (there shouldn't already be one....)
	void InstallChangeCallback( FnChangeCallback_t callback, bool bInvoke = true );
	void RemoveChangeCallback( FnChangeCallback_t callbackToRemove );

	int GetChangeCallbackCount() const { return std::ssize( m_pParent->m_fnChangeCallbacks ); }
	FnChangeCallback_t GetChangeCallback( int slot ) const { return m_pParent->m_fnChangeCallbacks[ slot ]; }

	// Retrieve value
	inline float					GetFloat( void ) const;
	inline int						GetInt( void ) const;
	inline Color			GetColor( void ) const;
	inline bool			GetBool() const {  return !!GetInt(); }
	inline char const	   *GetString( void ) const;

	// Compiler driven selection for template use
	template <typename T> T Get( void ) const;
	template <typename T> T Get( T * ) const;

	// These just call into the accessor to check flags and set the var (which ends up calling InternalSetValue).
	void				SetValue( const char *value );
	void				SetValue( float value );
	void				SetValue( int value );
	void				SetValue( Color value );
	
	// Reset to default value
	void						Revert( void );

	// True if it has a min/max setting
	bool						HasMin() const;
	bool						HasMax() const;

	bool						GetMin( float& minVal ) const;
	bool						GetMax( float& maxVal ) const;

	float						GetMinValue() const;
	float						GetMaxValue() const;

	const char					*GetDefault( void ) const;
	void						SetDefault( const char *pszDefault );

	// Value
	struct CVValue_t
	{
		char						*m_pszString;
		int							m_StringLength;

		// Values
		float						m_fValue;
		int							m_nValue;
	};

	inline CVValue_t &GetRawValue()
	{
		return m_Value;
	}
	inline const CVValue_t &GetRawValue() const
	{
		return m_Value;
	}

private:
	bool						InternalSetColorFromString( const char *value );
	// Called by ICvar when the value of a var is changing.
	void						InternalSetValue(const char *value);
	// For CVARs marked FCVAR_NEVER_AS_STRING
	void						InternalSetFloatValue( float fNewValue );
	void						InternalSetIntValue( int nValue );
	void						InternalSetColorValue( Color value );

	bool						ClampValue( float& value );
	void						ChangeStringValue( const char *tempVal, float flOldValue );

	void						Create( const char *pName, const char *pDefaultValue, int flags = 0,
									const char *pHelpString = 0, bool bMin = false, float fMin = 0.0,
									bool bMax = false, float fMax = false, FnChangeCallback_t callback = 0 );

protected:
	// The value as stored, following the parent link; ConVar_ServerBounded builds its bounded value from it.
	inline float				GetRawFloat( void ) const;

	// The bounded value of a ConVar_ServerBounded (out of line: ConVar_ServerBounded is defined later).
	float						GetBoundedFloat( void ) const;

	// This either points to "this" or it points to the original declaration of a ConVar.
	// This allows ConVars to exist in separate modules, and they all use the first one to be declared.
	// m_pParent->m_pParent must equal m_pParent (ie: m_pParent must be the root, or original, ConVar).
	ConVar						*m_pParent;

	// Static data
	const char					*m_pszDefaultValue;
	
	CVValue_t					m_Value;

	// Min/Max values
	bool						m_bHasMin;
	float						m_fMinVal;
	bool						m_bHasMax;
	float						m_fMaxVal;
	
	// Call this function when ConVar changes
	std::vector< FnChangeCallback_t > m_fnChangeCallbacks;
};

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a float
// Output : float
//-----------------------------------------------------------------------------
inline float ConVar::GetFloat( void ) const
{
	if ( Kind() == ConKind::VarServerBounded )
		return GetBoundedFloat();

	return GetRawFloat();
}

inline float ConVar::GetRawFloat( void ) const
{
	if ( m_pParent == this )
		return StoreValue( m_Value.m_fValue, ( intp ) this );
	else
		return m_pParent->GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as an int
// Output : int
//-----------------------------------------------------------------------------
inline int ConVar::GetInt( void ) const 
{
	if ( Kind() == ConKind::VarServerBounded )
		return (int)GetBoundedFloat();

	// dgoodenough - Better NULL pointer protection
	contract_assert( m_pParent );
	if ( m_pParent == this )
		return StoreValue( m_Value.m_nValue, ( intp ) this );
	else
		return m_pParent->GetInt();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a color
// Output : Color
//-----------------------------------------------------------------------------
inline Color ConVar::GetColor( void ) const 
{
	contract_assert( m_pParent );
	if ( m_pParent == this )
	{
		int nColorValue = StoreValue( m_Value.m_nValue, ( intp ) this );
		unsigned char *pColorElement = ( unsigned char * ) &nColorValue;
		return Color( pColorElement[ 0 ], pColorElement[ 1 ], pColorElement[ 2 ], pColorElement[ 3 ] );
	}
	else
		return m_pParent->GetColor();
}

//-----------------------------------------------------------------------------

template <> inline float			ConVar::Get<float>( void ) const		{ return GetFloat(); }
template <> inline int			ConVar::Get<int>( void ) const			{ return GetInt(); }
template <> inline bool			ConVar::Get<bool>( void ) const			{ return GetBool(); }
template <> inline const char *	ConVar::Get<const char *>( void ) const	{ return GetString(); }
template <> inline float			ConVar::Get<float>( float *p ) const				{ return ( *p = GetFloat() ); }
template <> inline int			ConVar::Get<int>( int *p ) const					{ return ( *p = GetInt() ); }
template <> inline bool			ConVar::Get<bool>( bool *p ) const					{ return ( *p = GetBool() ); }
template <> inline const char *	ConVar::Get<const char *>( char const **p ) const	{ return ( *p = GetString() ); }

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a string, return "" for bogus string pointer, etc.
// Output : const char *
//-----------------------------------------------------------------------------
inline const char *ConVar::GetString( void ) const 
{
	if ( m_nFlags & FCVAR_NEVER_AS_STRING )
		return "FCVAR_NEVER_AS_STRING";
	
	char const *str = m_pParent->m_Value.m_pszString;
	return str ? str : "";
}

class CSplitScreenAddedConVar : public ConVar
{
	typedef ConVar BaseClass;
public:
	CSplitScreenAddedConVar( int nSplitScreenSlot, const char *pName, const ConVar *pBaseVar ) :
		BaseClass
		( 
			pName, 
			pBaseVar->GetDefault(), 
			// Keep basevar flags, except remove _SS and add _SS_ADDED instead
			( pBaseVar->GetFlags() & ~FCVAR_SS ) | FCVAR_SS_ADDED, 
			pBaseVar->GetHelpText(), 
			pBaseVar->HasMin(),
			pBaseVar->GetMinValue(),
			pBaseVar->HasMax(),
			pBaseVar->GetMaxValue()
		),
		m_pBaseVar( pBaseVar ),
		m_nSplitScreenSlot( nSplitScreenSlot )
	{
		SetKind( ConKind::VarSplitScreenAdded );
		for ( int i = 0; i < pBaseVar->GetChangeCallbackCount(); ++i )
		{
			InstallChangeCallback( pBaseVar->GetChangeCallback( i ), false );
		}
		contract_assert( nSplitScreenSlot >= 1 );
		contract_assert( nSplitScreenSlot < MAX_SPLITSCREEN_CLIENTS );
		contract_assert( m_pBaseVar );
		contract_assert( IsFlagSet( FCVAR_SS_ADDED ) );
		contract_assert( !IsFlagSet( FCVAR_SS ) );
	}

	const ConVar *GetBaseVar() const;
	const char *GetBaseName() const;
	void SetSplitScreenPlayerSlot( int nSlot );
	int GetSplitScreenPlayerSlot() const;

protected:

	const ConVar	*m_pBaseVar;
	int		m_nSplitScreenSlot;
};

inline const ConVar *CSplitScreenAddedConVar::GetBaseVar() const 
{ 
	contract_assert( m_pBaseVar );
	return m_pBaseVar; 
}

inline const char *CSplitScreenAddedConVar::GetBaseName() const 
{ 
	contract_assert( m_pBaseVar );
	return m_pBaseVar->GetName(); 
}

inline void CSplitScreenAddedConVar::SetSplitScreenPlayerSlot( int nSlot ) 
{ 
	m_nSplitScreenSlot = nSlot; 
}

inline int CSplitScreenAddedConVar::GetSplitScreenPlayerSlot() const 
{ 
	return m_nSplitScreenSlot; 
}

//-----------------------------------------------------------------------------
// This version is simply used to make reading convars simpler.
// Writing convars isn't allowed in this mode
//-----------------------------------------------------------------------------
class CEmptyConVar : public ConVar
{
public:
	CEmptyConVar() : ConVar( "", "0" ) { SetKind( ConKind::VarEmpty ); }
};

//-----------------------------------------------------------------------------
// Used to read/write convars that already exist (replaces the FindVar method)
//-----------------------------------------------------------------------------
class ConVarRef
{
public:
	ConVarRef( const char *pName );
	ConVarRef( const char *pName, bool bIgnoreMissing );
	ConVarRef( ConVar *pConVar );

	void Init( const char *pName, bool bIgnoreMissing );
	bool IsValid() const;
	bool IsFlagSet( int nFlags ) const;
	ConVar *GetLinkedConVar();

	// Get/Set value
	float GetFloat( void ) const;
	int GetInt( void ) const;
	Color GetColor( void ) const;
	bool GetBool() const { return !!GetInt(); }
	const char *GetString( void ) const;

	void SetValue( const char *pValue );
	void SetValue( float flValue );
	void SetValue( int nValue );
	void SetValue( Color value );
	void SetValue( bool bValue );

	const char *GetName() const;

	const char *GetDefault() const;

	const char *GetBaseName() const;

	int	GetSplitScreenPlayerSlot() const;

	//=============================================================================
	// HPE_BEGIN:
	// [dwenger] - Convenience function for retrieving the max value of the convar
	//=============================================================================
	float GetMax() const;

	// [jbright] - Convenience function for retrieving the min value of the convar
	float GetMin() const;
	//=============================================================================
	// HPE_END
	//=============================================================================

private:
	// High-speed method to read convar data
	ConVar *m_pConVar;
	ConVar *m_pConVarState;
};

//-----------------------------------------------------------------------------
// Did we find an existing convar of that name?
//-----------------------------------------------------------------------------
inline bool ConVarRef::IsFlagSet( int nFlags ) const
{
	return ( m_pConVar->IsFlagSet( nFlags ) != 0 );
}

inline ConVar *ConVarRef::GetLinkedConVar()
{
	return m_pConVar;
}

inline const char *ConVarRef::GetName() const
{
	return m_pConVar->GetName();
}

inline const char *ConVarRef::GetBaseName() const
{
	return m_pConVar->GetBaseName();
}

inline int ConVarRef::GetSplitScreenPlayerSlot() const
{
	return m_pConVar->GetSplitScreenPlayerSlot();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a float
//-----------------------------------------------------------------------------
inline float ConVarRef::GetFloat( void ) const
{
	return m_pConVarState->GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as an int
//-----------------------------------------------------------------------------
inline int ConVarRef::GetInt( void ) const 
{
	return m_pConVarState->GetInt();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a color
//-----------------------------------------------------------------------------
inline Color ConVarRef::GetColor( void ) const 
{
	return m_pConVarState->GetColor();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a string, return "" for bogus string pointer, etc.
//-----------------------------------------------------------------------------
inline const char *ConVarRef::GetString( void ) const 
{
	contract_assert( !IsFlagSet( FCVAR_NEVER_AS_STRING ) );
	return m_pConVarState->m_Value.m_pszString;
}

inline void ConVarRef::SetValue( const char *pValue )
{
	m_pConVar->SetValue( pValue );
}

inline void ConVarRef::SetValue( float flValue )
{
	m_pConVar->SetValue( flValue );
}

inline void ConVarRef::SetValue( int nValue )
{
	m_pConVar->SetValue( nValue );
}

inline void ConVarRef::SetValue( Color value )
{
	m_pConVar->SetValue( value );
}

inline void ConVarRef::SetValue( bool bValue )
{
	m_pConVar->SetValue( bValue ? 1 : 0 );
}

inline const char *ConVarRef::GetDefault() const
{
	return m_pConVarState->m_pszDefaultValue;
}

//=============================================================================
// HPE_BEGIN:
// [dwenger] - Convenience function for retrieving the max value of the convar
//=============================================================================
inline  float ConVarRef::GetMax() const
{
	return m_pConVarState->m_fMaxVal;
}

// [jbright] - Convenience function for retrieving the min value of the convar
inline  float ConVarRef::GetMin() const
{
	return m_pConVarState->m_fMinVal;
}

//=============================================================================
// HPE_END
//=============================================================================

//-----------------------------------------------------------------------------
// Helper for referencing splitscreen convars (i.e., "name" and "name2")
//-----------------------------------------------------------------------------
class SplitScreenConVarRef
{
public:
	SplitScreenConVarRef( const char *pName );
	SplitScreenConVarRef( const char *pName, bool bIgnoreMissing );
	SplitScreenConVarRef( ConVar *pConVar );

	void Init( const char *pName, bool bIgnoreMissing );
	bool IsValid() const;
	bool IsFlagSet( int nFlags ) const;

	float GetMax( void ) const;
	float GetMin( void ) const;

	// Get/Set value
	float GetFloat( int nSlot ) const;
	int GetInt( int nSlot ) const;
	Color GetColor( int nSlot ) const;
	bool GetBool( int nSlot ) const { return !!GetInt( nSlot ); }
	const char *GetString( int nSlot  ) const;

	void SetValue( int nSlot, const char *pValue );
	void SetValue( int nSlot, float flValue );
	void SetValue( int nSlot, int nValue );
	void SetValue( int nSlot, Color value );
	void SetValue( int nSlot, bool bValue );

	const char *GetName( int nSlot ) const;

	const char *GetDefault() const;

	const char *GetBaseName() const;

private:
	struct cv_t
	{
		ConVar *m_pConVar;
		ConVar *m_pConVarState;
	};

	cv_t	m_Info[ MAX_SPLITSCREEN_CLIENTS ];
};

//-----------------------------------------------------------------------------
// Did we find an existing convar of that name?
//-----------------------------------------------------------------------------
inline bool SplitScreenConVarRef::IsFlagSet( int nFlags ) const
{
	return ( m_Info[ 0 ].m_pConVar->IsFlagSet( nFlags ) != 0 );
}

inline const char *SplitScreenConVarRef::GetName( int nSlot ) const
{
	return m_Info[ nSlot ].m_pConVar->GetName();
}

inline const char *SplitScreenConVarRef::GetBaseName() const
{
	return m_Info[ 0 ].m_pConVar->GetBaseName();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a float
//-----------------------------------------------------------------------------
inline float SplitScreenConVarRef::GetFloat( int nSlot ) const
{
	return m_Info[ nSlot ].m_pConVarState->GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as an int
//-----------------------------------------------------------------------------
inline int SplitScreenConVarRef::GetInt( int nSlot ) const 
{
	return m_Info[ nSlot ].m_pConVarState->GetInt();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as an int
//-----------------------------------------------------------------------------
inline Color SplitScreenConVarRef::GetColor( int nSlot ) const 
{
	return m_Info[ nSlot ].m_pConVarState->GetColor();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a string, return "" for bogus string pointer, etc.
//-----------------------------------------------------------------------------
inline const char *SplitScreenConVarRef::GetString( int nSlot ) const 
{
	contract_assert( !IsFlagSet( FCVAR_NEVER_AS_STRING ) );
	return m_Info[ nSlot ].m_pConVarState->m_Value.m_pszString;
}

inline void SplitScreenConVarRef::SetValue( int nSlot, const char *pValue )
{
	m_Info[ nSlot ].m_pConVar->SetValue( pValue );
}

inline void SplitScreenConVarRef::SetValue( int nSlot, float flValue )
{
	m_Info[ nSlot ].m_pConVar->SetValue( flValue );
}

inline void SplitScreenConVarRef::SetValue( int nSlot, int nValue )
{
	m_Info[ nSlot ].m_pConVar->SetValue( nValue );
}

inline void SplitScreenConVarRef::SetValue( int nSlot, Color value )
{
	m_Info[ nSlot ].m_pConVar->SetValue( value );
}

inline void SplitScreenConVarRef::SetValue( int nSlot, bool bValue )
{
	m_Info[ nSlot ].m_pConVar->SetValue( bValue ? 1 : 0 );
}

inline const char *SplitScreenConVarRef::GetDefault() const
{
	return m_Info[ 0 ].m_pConVarState->m_pszDefaultValue;
}

inline float SplitScreenConVarRef::GetMin( void ) const
{
	return m_Info[ 0 ].m_pConVarState->m_fMinVal;
}

inline float SplitScreenConVarRef::GetMax( void ) const
{
	return m_Info[ 0 ].m_pConVarState->m_fMaxVal;
}

//-----------------------------------------------------------------------------
// a frequently more convenient wrapper around SplitScreenConvarRef
//-----------------------------------------------------------------------------

class SplitScreenSlottedConVarRef : public SplitScreenConVarRef
{
public:
	SplitScreenSlottedConVarRef( int slot, const char *pName ) : SplitScreenConVarRef( pName) 
	{ 
		m_iSlot = slot; 
	}

	SplitScreenSlottedConVarRef( int slot, const char *pName, bool bIgnoreMissing ) : SplitScreenConVarRef( pName, bIgnoreMissing )
	{ 
		m_iSlot = slot; 
	}

	SplitScreenSlottedConVarRef( int slot, ConVar *pConVar ) : SplitScreenConVarRef( pConVar )
	{ 
		m_iSlot = slot; 
	}

	// Get/Set value
	float GetFloat( void ) const { return SplitScreenConVarRef::GetFloat( m_iSlot ); }
	int GetInt( void ) const { return SplitScreenConVarRef::GetInt( m_iSlot ); }
	Color GetColor( void ) const { return SplitScreenConVarRef::GetColor( m_iSlot ); }
	bool GetBool( void ) const { return SplitScreenConVarRef::GetBool( m_iSlot ); }
	const char *GetString( void ) const { return SplitScreenConVarRef::GetString( m_iSlot ); }

	void SetValue( const char *pValue ) { SplitScreenConVarRef::SetValue( m_iSlot, pValue );}
	void SetValue( float flValue ) { SplitScreenConVarRef::SetValue( m_iSlot, flValue );};
	void SetValue( int nValue ) { SplitScreenConVarRef::SetValue( m_iSlot, nValue );};
	void SetValue( Color value ) { SplitScreenConVarRef::SetValue( m_iSlot, value );};
	void SetValue( bool bValue ) { SplitScreenConVarRef::SetValue( m_iSlot, bValue );};

	const char *GetName( void ) const { return SplitScreenConVarRef::GetName( m_iSlot ); }

private:
	int m_iSlot;
};

//-----------------------------------------------------------------------------
// Called by the framework to register ConCommands with the ICVar
//-----------------------------------------------------------------------------
void ConVar_Register( ConCommandRegistry &registry, int nCVarFlag = 0, FnConCommandBaseAccessor_t pAccessor = nullptr );
void ConVar_Unregister( ConCommandRegistry &registry );

// The same for the engine's registry, which holds everything that is not part of a game side.
void ConVar_Register( int nCVarFlag = 0, FnConCommandBaseAccessor_t pAccessor = nullptr );
void ConVar_Unregister( );

//-----------------------------------------------------------------------------
// Utility methods 
//-----------------------------------------------------------------------------
void ConVar_PrintDescription( const ConCommandBase *pVar );

//-----------------------------------------------------------------------------
// Purpose: Utility class to quickly allow ConCommands to call member methods
//-----------------------------------------------------------------------------

template< class T >
class CConCommandMemberAccessor : public ConCommand
{
	typedef ConCommand BaseClass;
	typedef void ( T::*FnMemberCommandCallback_t )( const CCommand &command );
	typedef int  ( T::*FnMemberCommandCompletionCallback_t )( const char *pPartial, std::vector< std::string > &commands );

public:
	CConCommandMemberAccessor( T* pOwner, const char *pName, FnMemberCommandCallback_t callback, const char *pHelpString = 0,
		int flags = 0, FnMemberCommandCompletionCallback_t completionFunc = 0 ) :
		BaseClass( pName,
			ConCommandCallback_t( [this]( const CCommand &command ) { Callback( command ); } ),
			pHelpString, flags,
			( completionFunc != 0 )
				? ConCommandCompletionCallback_t( [this]( const char *pPartial, std::vector< std::string > &commands ) { return Completion( pPartial, commands ); } )
				: ConCommandCompletionCallback_t() )
	{
		m_pOwner = pOwner;
		m_Func = callback;
		m_CompletionFunc = completionFunc;
	}

	~CConCommandMemberAccessor()
	{
		Shutdown();
	}

	void SetOwner( T* pOwner )
	{
		m_pOwner = pOwner;
	}

private:
	void Callback( const CCommand &command )
	{
		contract_assert( m_pOwner && m_Func );
		(m_pOwner->*m_Func)( command );
	}

	int Completion( const char *pPartial, std::vector< std::string > &commands )
	{
		contract_assert( m_pOwner && m_CompletionFunc );
		return (m_pOwner->*m_CompletionFunc)( pPartial, commands );
	}

	T* m_pOwner;
	FnMemberCommandCallback_t m_Func;
	FnMemberCommandCompletionCallback_t m_CompletionFunc;
};

}
