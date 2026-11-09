#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

typedef int TABLEID;

inline constexpr auto INVALID_STRING_TABLE = -1;
const unsigned short INVALID_STRING_INDEX = (unsigned short )-1;

// table index is sent in log2(MAX_TABLES) bits
inline constexpr auto MAX_TABLES = 32; // Table id is 4 bits

inline constexpr const char *INTERFACENAME_NETWORKSTRINGTABLESERVER = "VEngineServerStringTable001";
inline constexpr const char *INTERFACENAME_NETWORKSTRINGTABLECLIENT = "VEngineClientStringTable001";

class INetworkStringTable;

typedef void (*pfnStringChanged)( void *object, INetworkStringTable *stringTable, int stringNumber, char const *newString, void const *newData );

//-----------------------------------------------------------------------------
// Purpose: Game .dll shared string table interfaces
//-----------------------------------------------------------------------------
class INetworkStringTable
{
public:

		
	// Table Info
	const char		*GetTableName( void ) const;
	TABLEID			GetTableId( void ) const;
	int				GetNumStrings( void ) const;
	int				GetMaxStrings( void ) const;
	int				GetEntryBits( void ) const;

	// Networking
	void			SetTick( int tick );
	bool			ChangedSinceTick( int tick ) const;

	// Accessors (length -1 means don't change user data if string already exits)
	int				AddString( bool bIsServer, const char *value, int length = -1, const void *userdata = 0 ); 

	const char		*GetString( int stringNumber ) const;
	void			SetStringUserData( int stringNumber, int length, const void *userdata );
	const void		*GetStringUserData( int stringNumber, int *length ) const;
	int				FindStringIndex( char const *string ); // returns INVALID_STRING_INDEX if not found

	// Callbacks
	void			SetStringChangedCallback( void *object, pfnStringChanged changeFunc );
};

enum ENetworkStringtableFlags
{
	NSF_NONE = 0,
	NSF_DICTIONARY_ENABLED  = (1<<0), // Uses pre-calculated per map dictionaries to reduce bandwidth
};

class INetworkStringTableContainer
{
public:
	
	
	// table creation/destruction
	INetworkStringTable	*CreateStringTable( const char *tableName, int maxentries, int userdatafixedsize = 0, int userdatanetworkbits = 0, int flags = NSF_NONE );
	void				RemoveAllTables( void );
	
	// table infos
	INetworkStringTable	*FindTable( const char *tableName ) const;
	INetworkStringTable	*GetTable( TABLEID stringTable ) const;
	int					GetNumTables( void ) const;

	void				SetAllowClientSideAddString( INetworkStringTable *table, bool bAllowClientSideAddString );

	void				CreateDictionary( char const *pchMapName );
};
