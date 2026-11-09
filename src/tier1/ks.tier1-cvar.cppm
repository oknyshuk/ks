// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1:cvar;

import std;
import ks.math;
export import :iappsystem;
export import :icvar;
export import :convar;
export import :byte_writer;
export import :generichash;

export extern "C++" {

// This is a hash table class very similar to the CUtlHashFast, but
// modified specifically so that we can look up ConCommandBases
// by string names without having to actually store those strings in
// the dictionary, and also iterate over all of them. 
// It uses separate chaining: each key hashes to a bucket, each
// bucket is a linked list of hashed commands. We store the hash of 
// the command's string name as well as its pointer, so we can do 
// the linked list march part of the Find() operation more quickly.
class CConCommandHash
{
public:
	// Address of the entry; entries never move while they are in the hash
	typedef intp CCommandHashHandle_t;
	typedef unsigned int HashKey_t;

protected:
	struct HashEntry_t
	{
		HashKey_t m_uiKey;
		ConCommandBase *m_Data;

		HashEntry_t(unsigned int _hash, ConCommandBase * _cmd)
			: m_uiKey(_hash), m_Data(_cmd) {};

		HashEntry_t(){};
	};

	typedef std::list<HashEntry_t> bucket_t;

public:

	// Constructor/Deconstructor.
	CConCommandHash();
	~CConCommandHash();

	// Memory.
	void Purge( bool bReinitialize );

	// Invalid handle.
	static CCommandHashHandle_t InvalidHandle( void )	{ return ( CCommandHashHandle_t )~0; }
	inline bool IsValidHandle( CCommandHashHandle_t hHash ) const;

	/// Initialize.
	void Init( void ); // bucket count is hardcoded in enum below.

	/// Get hash value for a concommand
	static inline HashKey_t Hash( const ConCommandBase *cmd );

	// Insertion.
	CCommandHashHandle_t Insert( ConCommandBase *cmd );
	CCommandHashHandle_t FastInsert( ConCommandBase *cmd );

	// Removal.
	void Remove( CCommandHashHandle_t hHash ) __restrict__;
	void RemoveAll( void );

	// Retrieval.
	inline CCommandHashHandle_t Find( const char *name ) const;
	CCommandHashHandle_t Find( const ConCommandBase *cmd ) const __restrict__;
	// A convenience version of Find that skips the handle part
	// and returns a pointer to a concommand, or NULL if none was found.
	inline ConCommandBase * FindPtr( const char *name ) const;

	inline ConCommandBase * &operator[]( CCommandHashHandle_t hHash );
	inline ConCommandBase *const &operator[]( CCommandHashHandle_t hHash ) const;

	// Iteration
	struct CCommandHashIterator_t
	{
		int bucket;
		bucket_t::const_iterator it;

		CCommandHashIterator_t() : bucket(-1) {}
		CCommandHashIterator_t(int _bucket, bucket_t::const_iterator _it)
			: bucket(_bucket), it(_it) {};
	};
	inline CCommandHashIterator_t First() const;
	inline CCommandHashIterator_t Next( const CCommandHashIterator_t &hHash ) const;
	inline bool IsValidIterator( const CCommandHashIterator_t &iter ) const;
	inline ConCommandBase *operator[]( const CCommandHashIterator_t &iter ) const { return iter.it->m_Data;  }
private:
	// a find func where we've already computed the hash for the string.
	// (hidden private in case we decide to invent a custom string hash func
	//  for this class)
	CCommandHashHandle_t Find( const char *name, HashKey_t hash) const __restrict__;

protected:
	enum 
	{
		kNUM_BUCKETS = 256,
		kBUCKETMASK  = kNUM_BUCKETS - 1,
	};

	static CCommandHashHandle_t ToHandle( const HashEntry_t &entry ) { return (CCommandHashHandle_t)&entry; }
	static HashEntry_t *FromHandle( CCommandHashHandle_t hHash ) { return (HashEntry_t *)hHash; }

	// Each bucket holds its entries newest first
	std::vector< bucket_t >	m_aBuckets;
};

inline bool CConCommandHash::IsValidHandle( CCommandHashHandle_t hHash ) const
{
	return hHash != InvalidHandle();
}


inline CConCommandHash::CCommandHashHandle_t CConCommandHash::Find( const char *name ) const
{
	return Find( name, HashStringCaseless(name) );
}

inline ConCommandBase * &CConCommandHash::operator[]( CCommandHashHandle_t hHash )
{
	return ( FromHandle( hHash )->m_Data );
}

inline ConCommandBase *const &CConCommandHash::operator[]( CCommandHashHandle_t hHash ) const
{
	return ( FromHandle( hHash )->m_Data );
}

//-----------------------------------------------------------------------------
// Purpose: For iterating over the whole hash, return the index of the first element
//-----------------------------------------------------------------------------
inline CConCommandHash::CCommandHashIterator_t CConCommandHash::First() const
{
	// walk through the buckets to find the first one that has some data
	int bucketCount = std::ssize( m_aBuckets );
	for ( int bucket = 0 ; bucket < bucketCount ; ++bucket )
	{
		const bucket_t &list = m_aBuckets[bucket];
		if ( !list.empty() )
			return CCommandHashIterator_t( bucket, list.begin() );
	}

	// if we are down here, the list is empty 
	return CCommandHashIterator_t();
}

//-----------------------------------------------------------------------------
// Purpose: For iterating over the whole hash, return the next element after
// the param one. Or an invalid iterator.
//-----------------------------------------------------------------------------
inline CConCommandHash::CCommandHashIterator_t 
CConCommandHash::Next( const CConCommandHash::CCommandHashIterator_t &iter ) const
{
	// look for the next entry in the current bucket
	bucket_t::const_iterator next = std::next( iter.it );
	if ( next != m_aBuckets[iter.bucket].end() )
	{
		// this bucket still has more elements in it
		return CCommandHashIterator_t(iter.bucket, next);
	}

	// otherwise look for the next bucket with data
	int bucketCount = std::ssize( m_aBuckets );
	for ( int bucket = iter.bucket+1 ; bucket < bucketCount ; ++bucket )
	{
		const bucket_t &list = m_aBuckets[bucket];
		if ( !list.empty() )
			return CCommandHashIterator_t( bucket, list.begin() );
	}

	// if we're here, there's no more data to be had
	return CCommandHashIterator_t();
}

inline bool CConCommandHash::IsValidIterator( const CCommandHashIterator_t &iter ) const
{
	return iter.bucket >= 0;
}

inline CConCommandHash::HashKey_t CConCommandHash::Hash( const ConCommandBase *cmd )
{
	return HashStringCaseless( cmd->GetName() );
}

inline ConCommandBase * CConCommandHash::FindPtr( const char *name ) const
{
	CCommandHashHandle_t handle = Find(name);
	if (handle == InvalidHandle())
	{
		return nullptr;
	}
	else
	{
		return (*this)[handle];
	}
}


//-----------------------------------------------------------------------------
// Purpose: The one console variable / command dictionary.  Lives in the engine image
// (vstdlib/cvar.cpp); g_pCVar points at it.
//-----------------------------------------------------------------------------
class ICvar
{
public:
	ICvar();

	// App system protocol. Not an IAppSystem: CAppSystemGroup drives it by kind (see AppSystemRef).
	bool Connect( CreateInterfaceFn factory );
	void Disconnect();
	void *QueryInterface( const char *pInterfaceName );
	InitReturnVal_t Init();
	void Shutdown();
	const AppSystemInfo_t *GetDependencies() { return nullptr; }
	void Reconnect( CreateInterfaceFn factory, const char *pInterfaceName ) { ReconnectInterface( factory, pInterfaceName ); }

	// Allocate a unique DLL identifier
	CVarDLLIdentifier_t AllocateDLLIdentifier();

	// Register, unregister commands
	void			RegisterConCommand( ConCommandBase *pCommandBase );
	void			UnregisterConCommand( ConCommandBase *pCommandBase );
	void			UnregisterConCommands( CVarDLLIdentifier_t id );

	// If there is a +<varname> <value> on the command line, this returns the value.
	// Otherwise, it returns NULL.
	const char*		GetCommandLineValue( const char *pVariableName );

	// Try to find the cvar pointer by name
	ConCommandBase *FindCommandBase( const char *name );
	const ConCommandBase *FindCommandBase( const char *name ) const;
	ConVar			*FindVar ( const char *var_name );
	const ConVar	*FindVar ( const char *var_name ) const;
	ConCommand		*FindCommand( const char *name );
	const ConCommand *FindCommand( const char *name ) const;

	// Install a global change callback (to be called when any convar changes) 
	void			InstallGlobalChangeCallback( FnChangeCallback_t callback );
	void			RemoveGlobalChangeCallback( FnChangeCallback_t callback );
	void			CallGlobalChangeCallbacks( ConVar *var, const char *pOldString, float flOldValue );

	// Install a console printer; pOwner identifies the registration
	void			InstallConsoleDisplayFunc( const void *pOwner, ConsoleDisplayFn_t fnDisplay );
	void			RemoveConsoleDisplayFunc( const void *pOwner );
	[[gnu::format(printf, 3, 4)]] void			ConsoleColorPrintf( const Color& clr, const char *pFormat, ... ) const;
	[[gnu::format(printf, 2, 3)]] void			ConsolePrintf( const char *pFormat, ... ) const;
	[[gnu::format(printf, 2, 3)]] void			ConsoleDPrintf( const char *pFormat, ... ) const;

	// Reverts cvars which contain a specific flag
	void			RevertFlaggedConVars( int nFlag );

	// Method allowing the engine's linkage rules to take over
	// A little hacky, owing to the fact the engine is loaded
	// well after ICVar, so we can't use the standard connect pattern
	void			InstallCVarQuery( FnConVarsLinkable_t fnLinkable );

	void			SetMaxSplitScreenSlots( int nSlots );
	int				GetMaxSplitScreenSlots() const;

	void			AddSplitScreenConVars();
	void			RemoveSplitScreenConVars( CVarDLLIdentifier_t id );

	// Utilities for convars accessed by the material system thread
	bool			IsMaterialThreadSetAllowed( ) const;
	void			QueueMaterialThreadSetValue( ConVar *pConVar, const char *pValue );
	void			QueueMaterialThreadSetValue( ConVar *pConVar, int nValue );
	void			QueueMaterialThreadSetValue( ConVar *pConVar, float flValue );
	bool			HasQueuedMaterialThreadConVarSets() const;
	int				ProcessQueuedMaterialThreadConVarSets();

	/// Iteration over all cvars. 
	/// (THIS IS A SLOW OPERATION AND YOU SHOULD AVOID IT.)
	/// usage: 
	/// { ICVar::Iterator iter(g_pCVar); 
	///   for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
	///   {  
	///       ConCommandBase *cmd = iter.Get();
	///   } 
	/// }
	class Iterator
	{
	public:
		inline Iterator(ICvar *icvar) : m_pHash( &icvar->m_CommandHash ) {}
		inline void		SetFirst( void ) __restrict__ { m_hashIter = m_pHash->First(); }
		inline void		Next( void ) __restrict__ { m_hashIter = m_pHash->Next( m_hashIter ); }
		inline bool		IsValid( void ) __restrict__ { return m_pHash->IsValidIterator( m_hashIter ); }
		inline ConCommandBase *Get( void ) __restrict__
		{
			contract_assert( m_pHash->IsValidIterator( m_hashIter ) );
			return (*m_pHash)[m_hashIter];
		}
	private:
		const CConCommandHash *m_pHash;
		CConCommandHash::CCommandHashIterator_t m_hashIter;
	};

private:
	enum
	{
		CONSOLE_COLOR_PRINT = 0,
		CONSOLE_PRINT,
		CONSOLE_DPRINT,
	};

	void DisplayQueuedMessages( );

	struct ConsoleDisplay_t
	{
		const void			*m_pOwner;
		ConsoleDisplayFn_t	m_fnDisplay;
	};

	std::vector< FnChangeCallback_t >	m_GlobalChangeCallbacks;
	std::vector< ConsoleDisplay_t >		m_DisplayFuncs;
	int									m_nNextDLLIdentifier;

	ConCommandBase						*m_pConCommandList;
	CConCommandHash						m_CommandHash;

	// temporary console area so we can store prints before console display funs are installed
	mutable ks::byte_writer				m_TempConsoleBuffer;
	int									m_nMaxSplitScreenSlots;

	enum ConVarSetType_t
	{
		CONVAR_SET_STRING = 0,
		CONVAR_SET_INT,
		CONVAR_SET_FLOAT,
	};

	struct QueuedConVarSet_t
	{
		ConVar *m_pConVar;
		ConVarSetType_t m_nType;
		int m_nInt;
		float m_flFloat;
		std::string m_String;
	};

	struct SplitScreenConVar_t
	{
		SplitScreenConVar_t()
		{
			Reset();
		}

		void Reset()
		{
			m_VarName = "";
			m_pVar = nullptr;
		}

		std::string					m_VarName;
		CSplitScreenAddedConVar		*m_pVar;
	};

	struct SplitScreenAddedConVars_t
	{
		SplitScreenAddedConVars_t() 
		{
			for ( int i = 0; i < MAX_SPLITSCREEN_CLIENTS - 1; ++i )
			{
				m_Vars[ i ].Reset();
			}
		}

		// Var names need "static" buffers...
		SplitScreenConVar_t		m_Vars[ MAX_SPLITSCREEN_CLIENTS - 1 ];
	};

	std::map< ConVar *, SplitScreenAddedConVars_t > m_SplitScreenAddedConVarsMap;
	std::vector< QueuedConVarSet_t > m_QueuedConVarSets;
	bool m_bMaterialSystemThreadSetAllowed;

	// Standard console commands -- DO NOT PLACE ANY HIGHER THAN HERE BECAUSE THESE MUST BE THE FIRST TO DESTRUCT
	void Find( const CCommand &args );
	CConCommandMemberAccessor< ICvar > m_Find_register{ this, "find", &ICvar::Find, "Find concommands with the specified string in their name/help text.", 0 };
};

}
