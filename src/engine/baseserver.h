#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include <iserver.h>
import std;
import ks.tier1;
#include "baseclient.h"
#include "netmessages.h"
#include "net.h"
#include "event_system.h"

class CNetworkStringTableContainer;
class PackedEntity;
class ServerClass;
class INetworkStringTable;	
typedef intp SerializedEntityHandle_t;

enum server_state_t
{
	ss_dead = 0,	// Dead
	ss_loading,		// Spawning
	ss_active,		// Running
	ss_paused,		// Running, but paused
};

// MAX_CHALLENGES is made large to prevent a denial
//  of service attack that could cycle all of them
//  out before legitimate users connected
inline constexpr auto MAX_CHALLENGES = 16384;

// time a challenge is valid for, in seconds
inline constexpr auto CHALLENGE_LIFETIME = 60*60.0f;

// MAX_DELTA_TICKS defines the maximum delta difference allowed 
// for delta compression, if clients request on older tick as
// delta baseline, send a full update. 
inline constexpr auto MAX_DELTA_TICKS = 192; // this is about 3 seconds

typedef struct
{
	ns_address  adr;       // Address where challenge value was sent to.
	int			challenge; // To connect, adr IP address must respond with this #
	float		time;      // # is valid for only a short duration.
} challenge_t;


// Owned split-screen connect messages, as read from the connectionless packet.
using SplitPlayerConnects_t = std::vector< std::unique_ptr< CCLCMsg_SplitPlayerConnect_t > >;

class CBaseServer  : public IServer
{
friend class CMaster;

public:
	enum class server_kind_t : unsigned char { game, hltv };

	explicit CBaseServer( server_kind_t kind );
	~CBaseServer();

	server_kind_t m_server_kind;	// the class this object really is (see facade/baseserver.inl)

	bool RestartOnLevelChange() { return m_bRestartOnLevelChange; }

public: // IServer implementation

	int		GetNumClients( void ) const; // returns current number of clients
	int		GetNumProxies( void ) const; // returns number of attached HLTV proxies
	int		GetNumFakeClients() const; // returns number of fake clients/bots
	int		GetMaxClients( void ) const; // returns current client limit
	int		GetUDPPort( void ) const { return NET_GetUDPPort( m_Socket );	}
	IClient	*GetClient( int index ) { return m_Clients[index]; } // returns interface to client 
	int		GetClientCount() const { return std::ssize( m_Clients ); } // for iteration;
	float	GetTime( void ) const;
	int		GetTick( void ) const { return m_nTickCount; }
	float	GetTickInterval( void ) const { return m_flTickInterval; }
	float	GetTimescale( void ) const { return m_flTimescale; }
	const char * GetName( void ) const;

	const char *GetNameBase( void ) const;
	const char *GetMapName( void ) const { return m_szMapname; }
	const char *GetBaseMapName( void ) const { return m_szBaseMapname; }
	const char *GetMapGroupName( void ) const { return m_szMapGroupName; }
	int		GetSpawnCount( void ) const { return m_nSpawnCount; }
	int		GetNumClasses( void ) const { return serverclasses; }
	int		GetClassBits( void ) const { return serverclassbits; }
	void GetNetStats( float &avgIn, float &avgOut );

	void	GetNetStatsBase( float &avgIn, float &avgOut );
	int		GetNumPlayers();
	bool	GetPlayerInfo( int nClientIndex, player_info_t *pinfo );
	float	GetCPUUsage( void ) { return m_fCPUPercent; }
		
	bool	IsActive( void ) const { return m_State >= ss_active; }	
	bool	IsLoading( void ) const { return m_State == ss_loading; }
	bool	IsDedicated( void ) const { return m_bIsDedicated; }
	bool	IsPaused( void ) const { return m_State == ss_paused; }
	bool IsMultiplayer( void ) const;

	bool	IsMultiplayerBase( void ) const { return m_nMaxclients > 1; }
	bool IsPausable( void ) const;

	bool	IsPausableBase( void ) const { return false; }
	bool IsHLTV( void ) const;

	bool	IsHLTVBase( void ) const { return false; }
	bool	IsReplay( void ) const { return false; }

	void	BroadcastMessage( INetMessage &msg, bool onlyActive = false, bool reliable = false );
	void	BroadcastMessage( INetMessage &msg, IRecipientFilter &filter );
	[[gnu::format(printf, 2, 3)]] void	BroadcastPrintf ( const char *fmt, ...);
	const char * GetPassword(  ) const;


	const char * GetPasswordBase() const;
	void SetMaxClients( int number );


	void	SetMaxClientsBase( int number );
	void	SetPaused(bool paused);
	void	SetTimescale( float flTimescale );
	void	SetPassword(const char *password);

	void	DisconnectClient(IClient *client, const char *reason );
	
	void 	WriteDeltaEntities( CBaseClient *client, CClientFrame *to, CClientFrame *from, CSVCMsg_PacketEntities_t &msg );
	void	WriteTempEntities( CBaseClient *client, CFrameSnapshot *to, CFrameSnapshot *from, CSVCMsg_TempEntities_t &msg, int ev_max );
	
public: // IConnectionlessPacketHandler implementation

	bool	ProcessConnectionlessPacket( netpacket_t * packet );
	void Init( bool isDedicated );


	void	InitBase( bool isDedicated );
	void Clear( void );

	void	ClearBase( void );
	void Shutdown( void );

	void	ShutdownBase( void );
	CBaseClient *CreateFakeClient(const char *name);
	void RemoveClientFromGame( CBaseClient *client );

	void 	RemoveClientFromGameBase( CBaseClient *client ) {};
	void SendClientMessages( bool bSendSnapshots );

	void	SendClientMessagesBase ( bool bSendSnapshots );
	void FillServerInfo( ks::net::CSVCMsg_ServerInfo &serverinfo );

	void	FillServerInfoBase(ks::net::CSVCMsg_ServerInfo &serverinfo);
	void UserInfoChanged( int nClientIndex );

	void	UserInfoChangedBase( int nClientIndex );
	bool GetClassBaseline( ServerClass *pClass, SerializedEntityHandle_t *pHandle );


	bool	GetClassBaselineBase( ServerClass *pClass, SerializedEntityHandle_t *pHandle);
	void	RunFrame( void );
	void	ProcessVoice( void );
	void	InactivateClients( void );
	void	ReconnectClients( void );
	void	CheckTimeouts (void);
	void	UpdateUserSettings(void);
	void	SendPendingServerInfo(void);

	INetworkStringTable *GetInstanceBaselineTable( void );
	INetworkStringTable *GetLightStyleTable( void );
	INetworkStringTable *GetUserInfoTable( void );

	[[gnu::format(printf, 3, 4)]] void	RejectConnection(const ns_address &adr, const char *fmt, ... );

	float	GetFinalTickTime( void ) const;

	bool CheckIPRestrictions( const ns_address &adr, int nAuthProtocol );

	void	SetMasterServerRulesDirty();

	void	RecalculateTags( void );
	void	AddTag( const char *pszTag, const char *pszSubTagValue = nullptr );
	void	RemoveTag( const char *pszTag, bool bSubTag = false );

	CBaseClient *CreateSplitClient( const ks::net::CMsg_CVars& vecUserInfo, CBaseClient *pAttachedTo );
	CBaseClient *GetBaseUserForSplitClient( CBaseClient *pSplitUser );

	void QueueSplitScreenDisconnect( CBaseClient *pSplitHost, CBaseClient *pSplitUser );
	void ProcessSplitScreenDisconnects();

	void		UpdateGameType();
	void		UpdateGameData();
	
	char const  *GetGameType() const;
	char const *GetGameData() const;
	int GetGameDataVersion() const;

	bool IsPlayingSoloAgainstBots() const;
	bool ShouldHideServer() const;
	int  GetMaxHumanPlayers() const;
	int  GetNumHumanPlayers() const;

	void GetMasterServerPlayerCounts( int &nHumans, int &nMaxHumanSlots, int &nBots );


	void ShowTags() const;
	uint64 GetMatchId()const { return m_nMatchId; }
public:
	IClient * ConnectClient( const ns_address &adr, int protocol, int challenge, int authProtocol, const char *name, const char *password, const char *hashedCDkey, int cdKeyLen, const SplitPlayerConnects_t &splitScreenClients, bool isClientLowViolence, CrossPlayPlatform_t clientPlatform, const byte *pbEncryptionKey, int nEncryptionKeyIndex );


	IClient *ConnectClientBase ( const ns_address &adr, int protocol, int challenge, int authProtocol, 
					    const char *name, const char *password, const char *hashedCDkey, int cdKeyLen,
						const SplitPlayerConnects_t &splitScreenClients, bool isClientLowViolence, CrossPlayPlatform_t clientPlatform,
						const byte *pbEncryptionKey, int nEncryptionKeyIndex );
	bool GetRedirectAddressForConnectClient( const ns_address &adr, const SplitPlayerConnects_t &splitScreenClients, ns_address *pNetAdrRedirect );

	
	bool GetRedirectAddressForConnectClientBase( const ns_address &adr, const SplitPlayerConnects_t &splitScreenClients, ns_address *pNetAdrRedirect ) { return false; }
	
	CBaseClient *GetFreeClient( const ns_address &adr );
	CBaseClient * CreateNewClient( int slot );


	CBaseClient *CreateNewClientBase( int slot ) { return nullptr; };
	bool FinishCertificateCheck( const ns_address &adr, int nAuthProtocol, const char *szRawCertificate );
 // must be derived

	
	bool	FinishCertificateCheckBase( const ns_address &adr, int nAuthProtocol, const char *szRawCertificate ) { return true; };
	
	int		GetChallengeNr ( const ns_address &adr );
	int GetChallengeType( const ns_address &adr );

	int		GetChallengeTypeBase ( const ns_address &adr );

	bool	CheckProtocol( const ns_address &adr, int nProtocol );
	bool	CheckChallengeNr( const ns_address &adr, int nChallengeValue );
	bool	CheckChallengeType( CBaseClient *client, int nNewUserID, const ns_address &adr, int nAuthProtocol, const char *pchLogonCookie, int cbCookie );
	bool	CheckPassword( const ns_address &adr, const char *password, const char *name );

	void	ReplyChallenge( const ns_address &adr, bf_read &msg );
	void	ReplyServerChallenge( const ns_address &adr);
	void	ReplyReservationRequest( const ns_address &adr, bf_read &msg );
	void	ReplyReservationCheckRequest( const ns_address &adr, bf_read &msg );

	void	CalculateCPUUsage();

	void SetRestartOnLevelChange(bool state)  { m_bRestartOnLevelChange = state; }

	bool RequireValidChallenge( const ns_address &adr );
	bool ValidChallenge( const ns_address &adr, int challengeNr );

	void ClearBaselineHandles( void );

	// Data
public:

	server_state_t	m_State;		// some actions are only valid during load
	int				m_Socket;		// network socket 
	int				m_nTickCount;	// current server tick
	char			m_szMapname[MAX_PATH];		// map name and path without extension
	char			m_szBaseMapname[MAX_MAP_NAME];		// map name without path or extension
	char			m_szMapGroupName[64];	// map group name
	char			m_szSkyname[64];		// skybox name
	char			m_Password[32];		// server password

	CRC32_t			worldmapCRC;      // For detecting that client has a hacked local copy of map, the client will be dropped if this occurs.
	CRC32_t			clientDllCRC; // The dll that this server is expecting clients to be using.
	CRC32_t			stringTableCRC;
	CNetworkStringTableContainer *m_StringTables;	// newtork string table container

	INetworkStringTable *m_pInstanceBaselineTable; 
	INetworkStringTable *m_pLightStyleTable;
	INetworkStringTable *m_pUserInfoTable;
	INetworkStringTable *m_pServerStartupTable;
	INetworkStringTable *m_pDownloadableFileTable;

	std::map< int, SerializedEntityHandle_t > m_BaselineHandles;

	// This will get set to NET_MAX_PAYLOAD if the server is MP.
	bf_write			m_Signon;
	std::vector< byte >	m_SignonBuffer;

	int			serverclasses;		// number of unique server classes
	int			serverclassbits;	// log2 of serverclasses

	bool	IsReserved() const { return m_nReservationCookie != 0; }
	void	Unreserve();	// clear reservation if there is one
	void	UpdateReservedState();

	uint64	GetReservationCookie() const;
	[[gnu::format(printf, 3, 4)]] void	SetReservationCookie( uint64 uiCookie, char const *pchReasonFormat, ... );
	bool ReserveServerForQueuedGame( char const *szReservationPayload );
	
	bool	IsExclusiveToLobbyConnections() const;
	bool	IsSinglePlayerGame() const; // won't allow external connections, no heartbeat to master server, etc.
	int		GetNumGameSlots() const { return m_numGameSlots; }

	enum EReservationStatus_t
	{
		kEReservationStatusRejected = 0,		// Reservation is reject, client should go away
		kEReservationStatusSuccess = 1,			// Reservation is fully completed, client can connect
		kEReservationStatusPending = 2,			// Reservation is pending, client should wait and might get success packet later
	};
	void	SendReservationStatus( EReservationStatus_t kEReservationStatus );
	void	ClearReservationStatus();

public:

	// Gets the next user ID mod SHRT_MAX and unique (not used by any active clients).
	int			GetNextUserID();
	int			m_nUserid;			// increases by one with every new client


	void		ClearTagStrings();
	void		AddTagString( std::string &dest, char const *pchString );

public:
	CBaseClient *GetFreeClientInternal( const ns_address &adr );

	int			m_nMaxclients;         // Current max #
	int			m_nSpawnCount;			// Number of servers spawned since start,
									// used to check late spawns (e.g., when d/l'ing lots of
									// data)
	float		m_flTickInterval;		// time for 1 tick in seconds
	float		m_flTimescale;			// the game time scale (multiplied in conjunction with host_timescale)


	std::vector< CBaseClient* >	m_Clients;		// array of up to [maxclients] client slots.
	
	bool		m_bIsDedicated;

	std::vector< challenge_t > m_ServerQueryChallenges; // prevent spoofed IP's from server queries/connecting

	float		m_fCPUPercent;
	float		m_fStartTime;
	float		m_fLastCPUCheckTime;

	bool		m_bRestartOnLevelChange;
	
	bool		m_bMasterServerRulesDirty;
	struct SplitDisconnect_t
	{
		CBaseClient *m_pUser;
		CBaseClient *m_pSplit;
	};

	std::vector< SplitDisconnect_t >	m_QueuedForDisconnect;

	struct QueueMatchPlayer_t
	{
		uint32 m_uiAccountID;
		ns_address m_adr;
		uint32 m_uiToken;
		uint32 m_uiReservationStage;
	};
	std::vector< QueueMatchPlayer_t > m_arrReservationPlayers;	// Reservation players in queue mode
	uint64		m_nReservationCookie;			// if this server has been reserved, cookie that connecting clients must present
	uint64		*m_pnReservationCookieSession;	// cookie that represents a server session
	float		m_flReservationExpiryTime;		// time at which reservation expires
	float		m_flTimeLastClientLeft;			// time when last client left server
	int			m_numGameSlots;					// number of game slots allocated
	std::string	m_GameType;
	std::vector< char > m_GameData;
	int			m_GameDataVersion;

	float		m_flTimeReservationGraceStarted;	// time when client attempted to connect and was granted a reservation grace period
	netadr_t	m_adrReservationGraceStarted;		// netadr of the client for whom reservation grace has been given
	bool CanAcceptChallengesFrom( const ns_address &adrFrom ) const;

	struct ReservationStatus_t
	{
		ReservationStatus_t() : m_bActive( false ), m_bSuccess( false )
		{
		}

		bool		m_bActive;
		bool		m_bSuccess;
		ns_address	m_Remote;
	};

	ReservationStatus_t m_ReservationStatus;
	uint64 m_nMatchId;
};

extern std::recursive_mutex g_svInstanceBaselineMutex;

const ConVar &GetIndexedConVar( const ConVar &cv, int nIndex );
