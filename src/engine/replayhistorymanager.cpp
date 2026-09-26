//========= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#if defined( REPLAY_ENABLED )
#include "replayhistorymanager.h"
#include "client.h"
#include "net_chan.h"
#include "dmxloader/dmxelement.h"
#include <time.h>
#include <memory>
#include <vector>

//----------------------------------------------------------------------------------------

#define REPLAY_HISTORY_FILE_CLIENT		"client_replay_history.dmx"
#define REPLAY_HISTORY_FILE_SERVER		"server_replay_history.dmx"

//----------------------------------------------------------------------------------------

void CClientReplayHistoryEntryData::BeginDownload()
{
	// Request the .dem file from the server
	GetBaseLocalClient().m_NetChannel->RequestFile( m_szFilename, true );

	m_bTransferring = true;
}

//----------------------------------------------------------------------------------------

BEGIN_DMXELEMENT_UNPACK( CBaseReplayHistoryEntryData )
	DMXELEMENT_UNPACK_FIELD_STRING( "filename", "NONE", m_szFilename )
	DMXELEMENT_UNPACK_FIELD_STRING( "map"     , "NONE", m_szMapName )

	DMXELEMENT_UNPACK_FIELD( "lifespan"   , "0", int      , m_nLifeSpan )
	DMXELEMENT_UNPACK_FIELD( "demo_length", "0", DmeTime_t, m_DemoLength )
	DMXELEMENT_UNPACK_FIELD( "transferred", "0", int      , m_nBytesTransferred )
	DMXELEMENT_UNPACK_FIELD( "size"       , "0", int      , m_nSize )
	DMXELEMENT_UNPACK_FIELD( "transferid" , "0", int      , m_nTransferId )
	DMXELEMENT_UNPACK_FIELD( "complete"   , "0", bool     , m_bTransferComplete )
	DMXELEMENT_UNPACK_FIELD( "downloading", "0", bool     , m_bTransferring )
END_DMXELEMENT_UNPACK( CBaseReplayHistoryEntryData, s_ClientEntryDataUnpack )

//----------------------------------------------------------------------------------------

template< class T >
class CBaseReplayHistoryManager : public IReplayHistoryManager
{
public:
	CBaseReplayHistoryManager()
	:	m_bInit( false )
	{
	}

	virtual void Init()
	{
		// Load all entries from disk
		if ( !LoadEntriesFromDisk() )
		{
			Warning( "Replay history file %s not found.\n", GetCacheFilename() );
		}

		m_bInit = true;
	}

	virtual bool IsInitialized() const		{ return m_bInit; }

	virtual void Shutdown()
	{
		m_bInit = false;
		m_lstEntries.clear();
	}

	virtual int GetNumEntries() const
	{
		return std::ssize( m_lstEntries );
	}

	virtual const CBaseReplayHistoryEntryData *GetEntryAtIndex( int iIndex ) const
	{
		Assert( iIndex >= 0 && iIndex < GetNumEntries() );
		return m_lstEntries[ iIndex ].get();
	}

	virtual CBaseReplayHistoryEntryData *FindEntry( const char *pFilename )
	{
		for ( const auto &pEntry : m_lstEntries )
		{
			if ( !V_stricmp( pFilename, pEntry->m_szFilename ) )
			{
				return pEntry.get();
			}
		}

		return nullptr;
	}

	virtual void FlushEntriesToDisk()
	{
		Assert( m_bInit );

		DECLARE_DMX_CONTEXT();

		CDmxElement* pEntries = CreateDmxElement( "Entries" );
		CDmxElementModifyScope modify( pEntries );

		int const nNumDemos = std::ssize( m_lstEntries );
		pEntries->SetValue( "num_demos", nNumDemos );

		CDmxAttribute* pDemoEntriesAttr = pEntries->AddAttribute( "demos" );
		std::vector< CDmxElement* >& entries = pDemoEntriesAttr->GetArrayForEdit< CDmxElement* >();

		modify.Release();

		for ( const auto &pEntry : m_lstEntries )
		{
			T *pEntryData = pEntry.get();

			CDmxElement* pEntryElement = CreateDmxElement( "demo" );
			entries.AddToTail( pEntryElement );

			CDmxElementModifyScope modifyClass( pEntryElement );
			pEntryElement->AddAttributesFromStructure( pEntryData, s_ClientEntryDataUnpack );

			pEntryElement->SetValue( "record_time", pEntryData->m_nRecordTime );

			RecordAdditionalEntryData( pEntryData, pEntryElement );
		}

		{
			MEM_ALLOC_CREDIT();
			const char *pFilename = GetCacheFilename();
			if ( !SerializeDMX( pFilename, "GAME", false, pEntries ) )
			{
				Warning( "Replay: Failed to write ragdoll cache, %s.\n", pFilename );
				return;
			}
		}

		CleanupDMX( pEntries );
	}

	bool LoadEntriesFromDisk()
	{ 
		Assert( !m_bInit );

		const char* pFilename = GetCacheFilename();

		DECLARE_DMX_CONTEXT();

		// Attempt to read from disk
		CDmxElement* pDemos = nullptr;
		if ( !UnserializeDMX( pFilename, "GAME", false, &pDemos ) )
			return false;

		std::vector< CDmxElement* > const& demos = pDemos->GetArray< CDmxElement* >( "demos" );
		for ( int i = 0; i < demos.Count(); ++i )
		{
			CDmxElement* pCurDemoInput = demos[ i ];

			// Create a new ragdoll entry and add to list
			T *pNewEntry = m_lstEntries.emplace_back( std::make_unique< T >() ).get();

			// Read
			pCurDemoInput->UnpackIntoStructure( pNewEntry, s_ClientEntryDataUnpack );

			// This should always be false
			pNewEntry->m_bTransferring = false;

			// Load record time
			pNewEntry->m_nRecordTime = pCurDemoInput->GetValue( "record_time", 0 );

			LoadAdditionalEntryData( pNewEntry, pCurDemoInput );
		}

		// Cleanup
		CleanupDMX( pDemos );

		PostLoadEntries();

		return true;
	}

	virtual void StopDownloads()
	{
		for ( const auto &pEntry : m_lstEntries )
		{
			pEntry->m_bTransferring = false;
		}
	}

	virtual const char *GetCacheFilename() const = 0;

protected:
	//
	// Called from FlushEntriesToDisk() for each entry - opportunity to record additional data
	//
	virtual void RecordAdditionalEntryData( const CBaseReplayHistoryEntryData *pEntry, CDmxElement *pElement ) {}

	//
	// Called from LoadEntriesFromDisk() for each entry - opportunity to load additional data
	//
	virtual void LoadAdditionalEntryData( CBaseReplayHistoryEntryData *pEntry, CDmxElement *pElement ) {}

	//
	// Called at the end of LoadEntriesFromDisk()
	//
	virtual void PostLoadEntries() {}

	virtual void Update() {}

	std::vector< std::unique_ptr< T > > m_lstEntries;

private:
	bool m_bInit;
};

//----------------------------------------------------------------------------------------

CON_COMMAND_F( replay_add_test_client_history_entry, "Add a test entry to the replay client history manager", 0 )
{
	// Record in client history
	extern ConVar replay_demolifespan;
	CClientReplayHistoryEntryData *pNewEntry = new CClientReplayHistoryEntryData();
	if ( !pNewEntry )
		return;
	tm now;
	Plat_GetLocalTime( &now );
	time_t now_time_t = mktime( &now );
	pNewEntry->m_nRecordTime = static_cast< int >( now_time_t );
	pNewEntry->m_nLifeSpan = replay_demolifespan.GetInt() * 24 * 3600;
	pNewEntry->m_DemoLength.SetSeconds( 0 );
	V_strcpy( pNewEntry->m_szFilename, "test_filename.dem" );
	V_strcpy( pNewEntry->m_szMapName, "mapname" );
	V_strcpy( pNewEntry->m_szServerAddress, "192.168.0.1" );
	pNewEntry->m_nBytesTransferred = 0;
	pNewEntry->m_bTransferComplete = false;
	pNewEntry->m_nSize = atoi( args[3] );
	pNewEntry->m_bTransferring = false;
	pNewEntry->m_nTransferId = -1;
	if ( !g_pClientReplayHistoryManager->RecordEntry( pNewEntry ) )
	{
		Warning( "Replay: Failed to record entry.\n" );
	}
}

//----------------------------------------------------------------------------------------

class CClientReplayHistoryManager : public CBaseReplayHistoryManager< CClientReplayHistoryEntryData >
{
public:
	virtual const char *GetCacheFilename() const	{ return REPLAY_HISTORY_FILE_CLIENT; }

	virtual void Update()
	{
		if ( !GetBaseLocalClient().m_NetChannel )
			return;

		for ( const auto &pEntry : m_lstEntries )
		{
			if ( !pEntry->m_bTransferComplete )
			{
				GetBaseLocalClient().m_NetChannel->GetStreamProgress( FLOW_INCOMING, &pEntry->m_nBytesTransferred, &pEntry->m_nSize );
			}
		}
	}

	virtual bool RecordEntry( CBaseReplayHistoryEntryData *pNewEntry )
	{
		if ( !IsInitialized() || !pNewEntry )
			return false;

		m_lstEntries.emplace_back( static_cast< CClientReplayHistoryEntryData * >( pNewEntry ) );

		// Write all entries to disk now, just to be safe
		FlushEntriesToDisk();

		return true;
	}

	virtual void RecordAdditionalEntryData( const CBaseReplayHistoryEntryData *pEntry, CDmxElement *pElement )
	{
		const CClientReplayHistoryEntryData *pClientEntry = static_cast< const CClientReplayHistoryEntryData *>( pEntry );
		pElement->SetValue( "server", pClientEntry->m_szServerAddress );
	}

	virtual void LoadAdditionalEntryData( CBaseReplayHistoryEntryData *pEntry, CDmxElement *pElement )
	{
		CClientReplayHistoryEntryData *pClientEntry = static_cast< CClientReplayHistoryEntryData *>( pEntry );
		V_strcpy( pClientEntry->m_szServerAddress, pElement->GetValueString( "server" ) );
	}
};

//----------------------------------------------------------------------------------------

class CServerReplayHistoryManager : public CBaseReplayHistoryManager< CServerReplayHistoryEntryData >
{
public:
	CServerReplayHistoryManager()
	:	m_flNextScheduledCleanup( 0.0f )
	{
	}

	virtual const char *GetCacheFilename() const	{ return REPLAY_HISTORY_FILE_SERVER; }

	// To be used with UpdateDemoFileEntries()
	enum EUpdateDemoFileEntryFlags
	{
		UPDATE_DELETESTALEFROMDISK	= 0x1,	// Delete stale demos from disk
		UPDATE_PRINTSTATS			= 0x2,	// Print statistics on all files
		UPDATE_SYNC					= 0x4,	// If the file does not exist on disk anymore, remove it from the history file
		UPDATE_REMOVEEXPIREDENTRIES	= 0x8,	// Remove any expired entries and flush to disk
	};

	bool UpdateDemoFileEntries( int nFlags )
	{
		bool bRemovedAny = false;
		bool bFlushToDisk = false;

		time_t now = time( nullptr );

		if ( nFlags & UPDATE_PRINTSTATS )
		{
			Msg( "\nReplay history stats\n" );
			Msg( "----------------------------------------------------\n" );
		}

		for ( auto it = m_lstEntries.begin(); it != m_lstEntries.end(); )
		{
			CServerReplayHistoryEntryData *pEntry = it->get();

			time_t recordtime = static_cast< time_t >( pEntry->m_nRecordTime + pEntry->m_nLifeSpan );
			double delta = difftime( recordtime, now );

			// If the file is no longer on disk and it should be
			if ( ( nFlags & UPDATE_SYNC ) &&
				 ( pEntry->m_nFileStatus == CServerReplayHistoryEntryData::FILESTATUS_EXISTS ) &&
				 !g_pFullFileSystem->FileExists( pEntry->m_szFilename ) )
			{
				pEntry->m_nFileStatus = CServerReplayHistoryEntryData::FILESTATUS_NOTONDISK;
				bFlushToDisk = true;
			}

			// Stale demo file?
			bool bStale = false;
			if ( pEntry->m_nFileStatus == CServerReplayHistoryEntryData::FILESTATUS_EXISTS && delta <= 0 )
			{
				bRemovedAny = true;
				bStale = true;

				// Delete the file from disk
				if ( g_pFullFileSystem->FileExists( pEntry->m_szFilename ) )
				{
					if ( nFlags & UPDATE_DELETESTALEFROMDISK )
					{
						Assert( 0 );	// Just making sure this gets hit...

						Msg( "Replay: Removing stale demo \"%s\" from disk.\n", pEntry->m_szFilename );

						// Remove the file from disk
						g_pFullFileSystem->RemoveFile( pEntry->m_szFilename );

						// Mark as deleted
						pEntry->m_nFileStatus = CServerReplayHistoryEntryData::FILESTATUS_EXPIRED;

						bFlushToDisk = true;
					}
				}
			}

			// Print stats if necessary
			if ( nFlags & UPDATE_PRINTSTATS )
			{
				static const int nSecsPerDay = 86400;
				int nDays  = (int)delta / nSecsPerDay;
				int nHours = (int)delta % nSecsPerDay / 3600;
				int nMins  = (int)delta % 60;
				Msg( "Demo \"%s\" ", pEntry->m_szFilename );
				if ( pEntry->m_nFileStatus == CServerReplayHistoryEntryData::FILESTATUS_EXPIRED )
				{
					Msg( "expired and was removed from disk.\n" );
				}
				else if ( pEntry->m_nFileStatus == CServerReplayHistoryEntryData::FILESTATUS_EXISTS )
				{
					Msg( "expires in %i days, %i hours, %i mins.\n", nDays, nHours, nMins );
				}
				else
				{
					Msg( "not found on disk.\n" );
				}
			}

			// Sync what's in memory with what's actually on disk
			if ( ( nFlags & UPDATE_REMOVEEXPIREDENTRIES ) &&
				 pEntry->m_nFileStatus == CServerReplayHistoryEntryData::FILESTATUS_EXPIRED )
			{
				// Remove the element
				it = m_lstEntries.erase( it );

				bFlushToDisk = true;
			}
			else
			{
				++it;
			}
		}

		// Flush?
		if ( bFlushToDisk )
		{
			FlushEntriesToDisk();
		}

		if ( nFlags & UPDATE_PRINTSTATS )
		{
			Msg( "\n" );
		}

		return bRemovedAny;
	}

	virtual void Update()
	{
		if ( host_time < m_flNextScheduledCleanup )
			return;
		
		extern ConVar replay_cleanup_time;
		m_flNextScheduledCleanup += replay_cleanup_time.GetInt() * 3600;

		UpdateDemoFileEntries( UPDATE_SYNC | UPDATE_DELETESTALEFROMDISK );
	}

	virtual bool RecordEntry( CBaseReplayHistoryEntryData *pNewEntry )
	{
		if ( !IsInitialized() || !pNewEntry )
			return false;

		m_lstEntries.emplace_back( static_cast< CServerReplayHistoryEntryData * >( pNewEntry ) );

		// Write all entries to disk now, just to be safe
		FlushEntriesToDisk();

		return true;
	}

	virtual void RecordAdditionalEntryData( const CBaseReplayHistoryEntryData *pEntry, CDmxElement *pElement )
	{
		const CServerReplayHistoryEntryData *pServerEntry = static_cast< const CServerReplayHistoryEntryData *>( pEntry );
		pElement->SetValue( "client_steam_id", pServerEntry->m_uClientSteamId );
		pElement->SetValue( "file_status", (int)pServerEntry->m_nFileStatus );
	}

	virtual void LoadAdditionalEntryData( CBaseReplayHistoryEntryData *pEntry, CDmxElement *pElement )
	{
		CServerReplayHistoryEntryData *pServerEntry = static_cast< CServerReplayHistoryEntryData *>( pEntry );
		pServerEntry->m_uClientSteamId = pElement->GetValue( "client_steam_id", (uint64)0 );
		pServerEntry->m_nFileStatus = (CServerReplayHistoryEntryData::EFileStatus)pElement->GetValue< int >( "file_status", (int)CServerReplayHistoryEntryData::FILESTATUS_EXISTS );
	}

private:
	float			m_flNextScheduledCleanup;
};

//----------------------------------------------------------------------------------------

inline CServerReplayHistoryManager *GetServerReplayHistoryManager()
{
	return static_cast< CServerReplayHistoryManager * >( g_pServerReplayHistoryManager );
}

//----------------------------------------------------------------------------------------

CON_COMMAND_F( replay_delete_stale_demos, "Deletes stale replay demo files", FCVAR_GAMEDLL | FCVAR_DONTRECORD )
{
	if ( GetServerReplayHistoryManager() &&
		 !GetServerReplayHistoryManager()->UpdateDemoFileEntries( CServerReplayHistoryManager::UPDATE_DELETESTALEFROMDISK ) )
	{
		Msg( "No demos were deleted.\n" );
	}
}

//----------------------------------------------------------------------------------------

CON_COMMAND_F( replay_print_history_stats, "Deletes stale replay demo files", FCVAR_GAMEDLL | FCVAR_DONTRECORD )
{
	if ( GetServerReplayHistoryManager() )
	{
		GetServerReplayHistoryManager()->UpdateDemoFileEntries( CServerReplayHistoryManager::UPDATE_PRINTSTATS );
	}
}

//----------------------------------------------------------------------------------------

CON_COMMAND_F( replay_remove_expired_entries, "Removes all expired entries from replay history", FCVAR_GAMEDLL | FCVAR_DONTRECORD )
{
	if ( GetServerReplayHistoryManager() )
	{
		GetServerReplayHistoryManager()->UpdateDemoFileEntries( CServerReplayHistoryManager::UPDATE_REMOVEEXPIREDENTRIES );
	}
}

//----------------------------------------------------------------------------------------

IReplayHistoryManager *CreateServerReplayHistoryManager()
{
	return new CServerReplayHistoryManager();
}

//----------------------------------------------------------------------------------------

static CClientReplayHistoryManager s_ClientReplayHistoryManager;
IReplayHistoryManager *g_pClientReplayHistoryManager = &s_ClientReplayHistoryManager;
IReplayHistoryManager *g_pServerReplayHistoryManager = nullptr;

// Expose interface to the client (needed by demo browser) - no need to do this for the server.
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(
	CClientReplayHistoryManager,
	IReplayHistoryManager,
	REPLAYHISTORYMANAGER_INTERFACE_VERSION,
	s_ClientReplayHistoryManager
);

//----------------------------------------------------------------------------------------

#endif
