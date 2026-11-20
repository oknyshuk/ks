#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.platform;
import ks.platform;
import ks.simd;
import ks.tier1.bitbuf;
import ks.tier1.checksum_crc;
#include "networkstringtabledefs.h"
#include "networkstringtableitem.h"
import std;
#include "netmessages.h"

class SVC_CreateStringTable;
class CBaseClient;

class INetworkStringDict
{
public:

	unsigned int Count();
	void Purge();
	const char *String( int index );
	bool IsValidIndex( int index );
	int Insert( const char *pString );
	int Find( const char *pString );
	void UpdateDictionary( int index );
	int DictionaryIndex( int index );
	CNetworkStringTableItem	&Element( int index );
	const CNetworkStringTableItem &Element( int index ) const;
};

class INetworkStringTableDictionaryMananger
{
public:
	bool OnLevelLoadStart( char const *pchMapName, CRC32_t *pStringTableCRC );
	// Indicates .bsp file is fully unloaded, safe to overwrite.
	void OnBSPFullyUnloaded();

	CRC32_t GetCRC();
};

extern INetworkStringTableDictionaryMananger *g_pStringTableDictionary;

//-----------------------------------------------------------------------------
// Purpose: Client/Server shared string table definition
//-----------------------------------------------------------------------------
class CNetworkStringTable  : public INetworkStringTable
{
	enum ConstEnum_t {MIRROR_TABLE_MAX_COUNT = 2};

public:
	// Construction
					CNetworkStringTable( TABLEID id, const char *tableName, int maxentries, int userdatafixedsize, int userdatanetworkbits, int flags );
	~CNetworkStringTable( void );

public:
	// INetworkStringTable interface:

	const char		*GetTableName( void ) const;
	TABLEID			GetTableId( void ) const;
	int				GetNumStrings( void ) const;
	int				GetMaxStrings( void ) const;
	int				GetEntryBits( void ) const;

	// Networking
	void			SetTick( int tick );
	bool			ChangedSinceTick( int tick ) const;

	int				AddString( bool bIsServer, const char *value, int length = -1, const void *userdata = nullptr ); 
	const char		*GetString( int stringNumber ) const;

	void			SetStringUserData( int stringNumber, int length, const void *userdata );
	const void		*GetStringUserData( int stringNumber, int *length ) const;
	int				FindStringIndex( char const *string );

	void			SetStringChangedCallback( void *object, pfnStringChanged changeFunc );

// other public apis
	bool			IsUserDataFixedSize() const;
	int				GetUserDataSizeBits() const;
	int				GetUserDataSize() const;

	bool			IsUsingDictionary() const;
	void			UpdateDictionaryString( int stringNumber );

public:
	
	int				WriteUpdate( CBaseClient *client, bf_write &buf, int tick_ack ) const;
	void			ParseUpdate( bf_read &buf, int entries );

	// HLTV change history & rollback
	void			EnableRollback();
	void			RestoreTick(int tick);
	
	// local backdoor tables
	void			SetMirrorTable( uint nIndex, INetworkStringTable *table );
	void			UpdateMirrorTable( int tick_ack  );
	void			CopyStringTable(CNetworkStringTable * table);
	// buffer IO
	void			WriteStringTable( bf_write& buf );
	bool			ReadStringTable( bf_read& buf );

	bool			WriteBaselines( CSVCMsg_CreateStringTable_t &msg );

	void			TriggerCallbacks( int tick_ack  );
	
	CNetworkStringTableItem *GetItem( int i );
	
	// debug ouptput
	void	Dump( void );
	bool	Lock( bool bLock );
	
	void SetAllowClientSideAddString( bool state );
	pfnStringChanged	GetCallback();

public:
	void			DataChanged( int stringNumber, CNetworkStringTableItem *item );

	// Destroy string table
	void			DeleteAllStrings( void );
	void			CheckDictionary( int stringNumber );

	CNetworkStringTable( const CNetworkStringTable & ); // not implemented, not allowed

	TABLEID					m_id;
	char					*m_pszTableName;
	// Must be a power of 2, so encoding can determine # of bits to use based on log2
	int						m_nMaxEntries;
	int						m_nEntryBits;
	int						m_nTickCount;
	int						m_nLastChangedTick;

	bool					m_bChangeHistoryEnabled : 1;
	bool					m_bLocked : 1;
	bool					m_bAllowClientSideAddString : 1;
	bool					m_bUserDataFixedSize : 1;

	int						m_nFlags; // NSF_*

	int						m_nUserDataSize;
	int						m_nUserDataSizeBits;

	// Change function callback
	pfnStringChanged		m_changeFunc;
	// Optional context/object
	void					*m_pObject;

	// pointer to local backdoor table 
	INetworkStringTable		*m_pMirrorTable[ MIRROR_TABLE_MAX_COUNT ];

	INetworkStringDict		*m_pItems;
	INetworkStringDict		*m_pItemsClientSide;	 // For m_bAllowClientSideAddString, these items are non-networked and are referenced by a negative string index!!!
};

//-----------------------------------------------------------------------------
// Purpose: Implements game .dll string table interface
//-----------------------------------------------------------------------------
class CNetworkStringTableContainer : public INetworkStringTableContainer
{
public:
	// Construction
							CNetworkStringTableContainer( void );
							~CNetworkStringTableContainer( void );

public:

	// Implement INetworkStringTableContainer
	INetworkStringTable	*CreateStringTable( const char *tableName, int maxentries, int userdatafixedsize = 0, int userdatanetworkbits = 0, int flags = NSF_NONE );
	void				RemoveAllTables( void );
	
	// table infos
	INetworkStringTable	*FindTable( const char *tableName ) const ;
	INetworkStringTable	*GetTable( TABLEID stringTable ) const;
	int					GetNumTables( void ) const;

	void				SetAllowClientSideAddString( INetworkStringTable *table, bool bAllowClientSideAddString );
	void				CreateDictionary( char const *pchMapName );
	void						UpdateDictionaryStrings();

public:

	// Update a client (called once during packet sending per client)
	void		SetTick( int tick_count);

	// rollback feature
	void		EnableRollback( bool bState );
	void		RestoreTick( int tick );

	// Buffer I/O
	void		WriteStringTables( bf_write& buf );
	bool		ReadStringTables( bf_read& buf );

	void		WriteUpdateMessage( CBaseClient *client, int tick_ack, bf_write &buf );
	void		WriteBaselines( char const *pchMapName, bf_write &buf );
	void		DirectUpdate( int tick_ack );	// fill mirror table directly with updates

	void		TriggerCallbacks( int tick_ack ); // fire callback functions 

	
		
	// Guards so game .dll can't create tables at the wrong time
	void		AllowCreation( bool state );

	
	
	// Print table data to console
	void		Dump( void );
	// Sets the lock and returns the previous lock state
	bool		Lock( bool bLock );

	void		SetAllowClientSideAddString( bool state );

public:
	bool		m_bAllowCreation;	// creat guard Guard
	int			m_nTickCount;		// current tick
	bool		m_bLocked;			// currently locked?
	bool		m_bEnableRollback;	// enables rollback feature

	std::vector< CNetworkStringTable* > m_Tables;	// the string tables
};
