#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "inetmsghandler.h"
#include "protocol.h"
#include "client_class.h"
#include "cdll_int.h"
#include "common.h"
#include "clockdriftmgr.h"
#include "cl_bounded_cvars.h"
#include "netmessages.h"
import std;

#include "matchmaking/imatchasync.h"
#include "netmessages_schema.h"

 // Only send this many requests before timing out.
inline constexpr auto CL_CONNECTION_RETRIES = 4;

// Mininum time gap (in seconds) before a subsequent connection request is sent.
inline constexpr auto CL_MIN_RESEND_TIME = 1.5f;
// Max time.  The cvar cl_resend is bounded by these.
inline constexpr auto CL_MAX_RESEND_TIME = 20.0f;

// In release, send commands at least this many times per second
inline constexpr auto MIN_CMD_RATE = 10.0f;
inline constexpr auto MAX_CMD_RATE = 128.0f;

typedef intp SerializedEntityHandle_t;

extern ConVar cl_name;

class CBaseClientState;

// This represents a server's 
class C_ServerClassInfo
{
public:
				C_ServerClassInfo();
				~C_ServerClassInfo();

public:

	ClientClass	*m_pClientClass;
	char		*m_ClassName;
	char		*m_DatatableName;

	// This is an index into the network string table (GetBaseLocalClient().GetInstanceBaselineTable()).
	int			m_InstanceBaselineIndex; // INVALID_STRING_INDEX if not initialized yet.
};


class CNetworkStringTableContainer;
class PackedEntity;
class INetworkStringTable;
class CEntityReadInfo;	

struct DeferredConnection_t
{
	DeferredConnection_t()
	{
		m_bActive = false;
		m_nChallenge = 0;
		m_nAuthprotocol = 0;
		m_unLobbyID = 0ull;
		m_bRequiresPassword = false;
		m_bDCFriendsReqd = false;
		m_bOfficialValveServer = false;
		m_nEncryptionKey = 0;
		m_nEncryptedSize = 0;
		std::memset( m_chLobbyType, 0, sizeof( m_chLobbyType ) );
	}

	bool	m_bActive;
	bool	m_bRequiresPassword;
	bool	m_bDCFriendsReqd;
	bool	m_bOfficialValveServer;
	char	m_chLobbyType[16];

	int		m_nChallenge;
	int		m_nAuthprotocol;
	uint64	m_unLobbyID;
	ns_address m_adrServerAddress;
	int		m_nEncryptionKey;
	int		m_nEncryptedSize;
};

// 0 == public, 1 == private, 2 == double NAT'd private (for  direct connection)
struct Remote_t
{
	bool Resolve();

	std::string	m_szAlias; // debugging: "public", "private", "direct"
	std::string	m_szRetryAddress;
	// Address for actual packets (may  differ from m_szRetryAddress)
	ns_address	m_adrRemote;
};

class CAddressList
{
public:

	CAddressList() {}

	void RemoveAll();
	bool IsRemoteInList( char const *pchAdrCheck ) const;
	bool IsAddressInList( const ns_address &adr ) const;

	// Only adds if not in list already
	void AddRemote( char const *pchAddress, char const *pchAlias );

	void Describe( std::string &str );

	int	Count() const;
	Remote_t &Get( int index );
	const Remote_t &Get( int index ) const;

private:

	std::vector< Remote_t > m_List;
};

// Classes to help with sending messages to server, with retries and timeouts

// Start by sending the message immediately. Wait for timeout before
// sending message again. 
// If maxAttempts exceeded with no response set state to AOS_FAILED and call callback.
// If a response is received set state to AOS_SUCCEEDED and call callback. 
// Result is only valid if state == AOS_SUCCEEDED
class CServerMsg : public IMatchAsyncOperation
{
public:

	// Methods of IMatchAsyncOperation
	bool IsFinished() { return m_eState > AOS_ABORTING; }
	AsyncOperationState_t GetState() { return m_eState; }
	uint64 GetResult() { return m_result; }
	void Abort() { m_eState = AOS_ABORTED; }

	// Timeout in seconds
	explicit CServerMsg( CBaseClientState *pParent, IMatchAsyncOperationCallback *pCallback, 
		const ns_address& serverAdr, int socket, uint32 maxAttempts, double timeout );

	// Per frame update, check for timeout etc if state == AOS_RUNNING
	void Update( void );

	// Call this function to check if it is ok to process a msg
	bool IsValidResponse( const ns_address &from, uint32 token );

	// Called by derived class when response received. Sets state to AOS_SUCCEEDED
	// and calls callback
	void ResponseReceived( uint64 result );

	// Methods derived class must implement. Token is a random number that can be 
	// used to match up a message-response pair. A new token is generated for each
	// SendMsg call; the last token generated can be queried using GetLastToken()
	virtual void SendMsg( const ns_address& serverAdr, int socket, uint32 token ) = 0;

	// See comment for SendMsg()
	uint32 GetLastToken() { return m_lastToken; }

	const ns_address& GetServerAddr() { return m_serverAdr; }

	// base client state parent
	CBaseClientState *m_pParent;

	// Data members
	AsyncOperationState_t m_eState;

	IMatchAsyncOperationCallback *m_pCallback;
	ns_address m_serverAdr;
	int m_socket;

	double m_lastMsgSendTime;
	double m_timeOut;

	uint32 m_maxAttempts;
	uint32 m_numAttempts;

	uint64 m_result;

	uint32 m_lastToken;	// See comment for SendMsg()
};

class CServerMsg_CheckReservation : public CServerMsg
{
public:

	explicit CServerMsg_CheckReservation( CBaseClientState *pParent, IMatchAsyncOperationCallback *pCallback, 
		const ns_address &serverAdr, int socket, uint64 reservationCookie, uint32 uiReservationStage );

	void Release();
	void SendMsg( const ns_address& serverAdr, int socket, uint32 token );
	void ResponseReceived( const ns_address& from, bf_read &msg, int32 hostVersion, uint32 token );

	uint64 m_reservationCookie;
	uint32 m_uiReservationStage;
};

class CServerMsg_Ping : public CServerMsg
{
public:

	explicit CServerMsg_Ping( CBaseClientState *pParent, IMatchAsyncOperationCallback *pCallback, 
		const ns_address &serverAdr, int socket );

	void Release();
	void SendMsg( const ns_address& serverAdrserverAdr, int socket, uint32 token );
	void ResponseReceived( const ns_address& from, bf_read &msg, int32 hostVersion, uint32 token );

	double m_timeLastMsgSent;
};

// CBaseClientState
class CBaseClientState :
	public INetChannelHandler
{
	
public:
	enum class state_kind_t : unsigned char { game, hltv };

	explicit CBaseClientState( state_kind_t kind );
	~CBaseClientState();

	state_kind_t m_state_kind;	// the class this object really is (see facade/baseclientstate.inl)

public:
	bool ProcessConnectionlessPacket( struct netpacket_s *packet );
 // IConnectionlessPacketHandler interface:
		
	bool ProcessConnectionlessPacketBase(struct netpacket_s *packet);

public:
	void ConnectionStart( INetChannel *chan );
 // INetMsgHandler interface:
		
	void ConnectionStartBase(INetChannel *chan);
	void ConnectionStop(  );

	void ConnectionStopBase( );
	void ConnectionClosing( const char *reason );

	void ConnectionClosingBase( const char *reason );
	void ConnectionCrashed( const char *reason );

	void ConnectionCrashedBase(const char *reason);
	void PacketStart( int incoming_sequence, int outgoing_acknowledged );


	void PacketStartBase(int incoming_sequence, int outgoing_acknowledged) {};
	void PacketEnd( void );

	void PacketEndBase( void ) {};
	void FileReceived( const char *fileName, unsigned int transferID, bool isReplayDemoFile );


	void FileReceivedBase( const char *fileName, unsigned int transferID, bool isReplayDemoFile );
	void FileRequested( const char *fileName, unsigned int transferID, bool isReplayDemoFile );

	void FileRequestedBase(const char *fileName, unsigned int transferID, bool isReplayDemoFile );
	void FileDenied( const char *fileName, unsigned int transferID, bool isReplayDemoFile );

	void FileDeniedBase(const char *fileName, unsigned int transferID, bool isReplayDemoFile );
	void FileSent(const char *fileName, unsigned int transferID, bool isReplayDemoFile );

	bool ChangeSplitscreenUser( int nSplitScreenUserSlot ); // interleaved networking used by SS system is changing the SS player slot that the subsequent messages pertain to

public:
	bool NETMsg_Tick( const ks::net::CNETMsg_Tick& msg );
 // IServerMessageHandlers
	
	bool NETMsg_TickBase( const ks::net::CNETMsg_Tick& msg );
	static bool NETMsg_Tick_Delegate( CBaseClientState *pThis, const ks::net::CNETMsg_Tick& msg ) { return pThis->NETMsg_Tick( msg ); }
	bool NETMsg_StringCmd( const ks::net::CNETMsg_StringCmd& msg );

	bool NETMsg_StringCmdBase( const ks::net::CNETMsg_StringCmd& msg );
	bool NETMsg_SignonState( const ks::net::CNETMsg_SignonState& msg );
	bool NETMsg_PlayerAvatarData( const ks::net::CNETMsg_PlayerAvatarData& msg );

	bool NETMsg_PlayerAvatarDataBase( const ks::net::CNETMsg_PlayerAvatarData& msg );
	bool NETMsg_SetConVar( const ks::net::CNETMsg_SetConVar& msg );

	bool NETMsg_SetConVarBase( const ks::net::CNETMsg_SetConVar& msg );

	bool SVCMsg_CmdKeyValues( const ks::net::CSVCMsg_CmdKeyValues& msg);
	bool SVCMsg_EncryptedData( const ks::net::CSVCMsg_EncryptedData& msg );

	bool SVCMsg_EncryptedDataBase( const ks::net::CSVCMsg_EncryptedData& msg );
	bool SVCMsg_SendTable( const ks::net::CSVCMsg_SendTable& msg );
	bool SVCMsg_Print( const ks::net::CSVCMsg_Print& msg );
	bool SVCMsg_ServerInfo( const ks::net::CSVCMsg_ServerInfo& msg );

	bool SVCMsg_ServerInfoBase( const ks::net::CSVCMsg_ServerInfo& msg );
	bool SVCMsg_ClassInfo( const ks::net::CSVCMsg_ClassInfo& msg );

	bool SVCMsg_ClassInfoBase( const ks::net::CSVCMsg_ClassInfo& msg );
	bool SVCMsg_SetPause( const ks::net::CSVCMsg_SetPause& msg );

	bool SVCMsg_SetPauseBase( const ks::net::CSVCMsg_SetPause& msg );
	bool SVCMsg_SetView( const ks::net::CSVCMsg_SetView& msg );

	bool SVCMsg_SetViewBase( const ks::net::CSVCMsg_SetView& msg );
	bool SVCMsg_CreateStringTable( const ks::net::CSVCMsg_CreateStringTable& msg );
	bool SVCMsg_UpdateStringTable( const ks::net::CSVCMsg_UpdateStringTable& msg );
	bool SVCMsg_VoiceInit( const ks::net::CSVCMsg_VoiceInit& msg );
	bool SVCMsg_VoiceData( const ks::net::CSVCMsg_VoiceData& msg );
	bool SVCMsg_FixAngle( const ks::net::CSVCMsg_FixAngle& msg );
	bool SVCMsg_Prefetch( const ks::net::CSVCMsg_Prefetch& msg );
	bool SVCMsg_CrosshairAngle( const ks::net::CSVCMsg_CrosshairAngle& msg );
	bool SVCMsg_BSPDecal( const ks::net::CSVCMsg_BSPDecal& msg );
	bool SVCMsg_SplitScreen( const ks::net::CSVCMsg_SplitScreen& msg );
	bool SVCMsg_GetCvarValue( const ks::net::CSVCMsg_GetCvarValue& msg );
	bool SVCMsg_Menu( const ks::net::CSVCMsg_Menu& msg );

	bool SVCMsg_MenuBase( const ks::net::CSVCMsg_Menu& msg );
	bool SVCMsg_UserMessage( const ks::net::CSVCMsg_UserMessage& msg );
	bool SVCMsg_PaintmapData( const ks::net::CSVCMsg_PaintmapData& msg );
	bool SVCMsg_GameEvent( const ks::net::CSVCMsg_GameEvent& msg );
	bool SVCMsg_GameEventList( const ks::net::CSVCMsg_GameEventList &msg );

	bool SVCMsg_GameEventListBase( const ks::net::CSVCMsg_GameEventList &msg );
	bool SVCMsg_TempEntities( const ks::net::CSVCMsg_TempEntities& msg );
	bool SVCMsg_PacketEntities( const ks::net::CSVCMsg_PacketEntities& msg );

	bool SVCMsg_PacketEntitiesBase( const ks::net::CSVCMsg_PacketEntities& msg );
	bool SVCMsg_Sounds( const ks::net::CSVCMsg_Sounds& msg );
	bool SVCMsg_EntityMsg( const ks::net::CSVCMsg_EntityMsg& msg );
	
	CNetMessageBinder m_NETMsgTick;
	CNetMessageBinder m_NETMsgStringCmd;
	CNetMessageBinder m_NETMsgSignonState;
	CNetMessageBinder m_NETMsgSetConVar;
	CNetMessageBinder m_NETMsgPlayerAvatarData;
	
	CNetMessageBinder m_SVCMsgServerInfo;
	CNetMessageBinder m_SVCMsgCmdKeyValues;
	CNetMessageBinder m_SVCMsg_EncryptedData;
	CNetMessageBinder m_SVCMsgClassInfo;
	CNetMessageBinder m_SVCMsgSendTable;
	CNetMessageBinder m_SVCMsgPrint;
	CNetMessageBinder m_SVCMsgSetPause;
	CNetMessageBinder m_SVCMsgSetView;
	CNetMessageBinder m_SVCMsgCreateStringTable;
	CNetMessageBinder m_SVCMsgUpdateStringTable;
	CNetMessageBinder m_SVCMsgVoiceInit;
	CNetMessageBinder m_SVCMsgVoiceData;
	CNetMessageBinder m_SVCMsgFixAngle;
	CNetMessageBinder m_SVCMsgPrefetch;
	CNetMessageBinder m_SVCMsgCrosshairAngle;
	CNetMessageBinder m_SVCMsgBSPDecal;
	CNetMessageBinder m_SVCMsgSplitScreen;
	CNetMessageBinder m_SVCMsgGetCvarValue;
	CNetMessageBinder m_SVCMsgMenu;
	CNetMessageBinder m_SVCMsgUserMessage;
	CNetMessageBinder m_SVCMsgPaintmapData;
	CNetMessageBinder m_SVCMsgGameEvent;
	CNetMessageBinder m_SVCMsgGameEventList;
	CNetMessageBinder m_SVCMsgTempEntities;
	CNetMessageBinder m_SVCMsgPacketEntities;
	CNetMessageBinder m_SVCMsgSounds;
	CNetMessageBinder m_SVCMsgEntityMsg;

public: // match events
	void OnEvent( KeyValues *pEvent );

public: 
	inline	bool IsActive( void ) const { return m_nSignonState == SIGNONSTATE_FULL; };
	inline	bool IsConnected( void ) const { return m_nSignonState >= SIGNONSTATE_CONNECTED; };
	inline	bool IsConnecting( void ) const { return m_nSignonState >= SIGNONSTATE_NONE; }
	void Clear( void );

	void ClearBase( void );
	void FullConnect( const ns_address &adr, int nEncryptionKey );

	void FullConnectBase( const ns_address &adr, int nEncryptionKey ); // a connection was established
	void Connect( const char *pchPublicAddress, char const *pchPrivateAddress, const char* szJoinType ); // start a connection challenge
	void ConnectSplitScreen( const char *pchPublicAddress, char const *pchPrivateAddress, int numPlayers, const char* szJoinType );
	bool SetSignonState( int state, int count, const ks::net::CNETMsg_SignonState *msg );
 // start a connection challenge
	bool SetSignonStateBase ( int state, int count, const ks::net::CNETMsg_SignonState *msg );
	void Disconnect( bool bShowMainMenu = true );

	void DisconnectBase( bool bShowMainMenu = true );
	void SendConnectPacket ( const ns_address &netAdrRemote, int challengeNr, int authProtocol );
	const char * GetCDKeyHash(  );

	const char *GetCDKeyHashBase() { return "123"; }
	void RunFrame( void );

	void RunFrameBase ( void );
	void CheckForResend ( bool bForceResendNow = false );
	void CheckForReservationResend( void );
	void ResendGameDetailsRequest( const ns_address &adr );
	void InstallStringTableCallback( char const *tableName );

	void InstallStringTableCallbackBase( char const *tableName ) { }
	bool HookClientStringTable( char const *tableName );

	bool HookClientStringTableBase( char const *tableName ) { return false; }
	bool LinkClasses( void );
	int GetConnectionRetryNumber(  ) const;

	int  GetConnectionRetryNumberBase() const { return m_nRetryMax; }
	const char * GetClientName(  );

	const char *GetClientNameBase() { return cl_name.GetString(); }
	void ReserveServer( const ns_address &netAdrPublic, const ns_address &netAdrPrivate, uint64 nServerReservationCookie,
		KeyValues *pKVGameSettings, IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation );

	bool CheckServerReservation( const ns_address &netAdrPublic, uint64 nServerReservationCookie, uint32 uiReservationStage, IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation );
	bool ServerPing( const ns_address &netAdrPublic, IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation );

	struct ReservationResponseReply_t
	{
		ReservationResponseReply_t() { m_uiResponse = 0; m_bValveDS = false; m_numGameSlots = 0; }
		ns_address m_adrFrom;
		uint8 m_uiResponse;
		bool m_bValveDS;
		uint32 m_numGameSlots;
	};
	void HandleReservationResponse( const ReservationResponseReply_t &reply );
	void HandleReserveServerChallengeResponse( int nChallengeNr );

	void SetServerReservationCookie( uint64 nReservationCookie ) { m_nServerReservationCookie = nReservationCookie; }

	static ClientClass* FindClientClass(const char *pClassName);

	CClockDriftMgr& GetClockDriftMgr();
	
	int GetClientTickCount() const;	// Get the client tick count.
	void SetClientTickCount( int tick );

	int GetServerTickCount() const;
	void SetServerTickCount( int tick );

	void SetClientAndServerTickCount( int tick );

	INetworkStringTable *GetStringTable( const char * name ) const;
	
	PackedEntity *GetEntityBaseline( int iBaseline, int nEntityIndex );
	void SetEntityBaseline(int iBaseline, ClientClass *pClientClass, int index, SerializedEntityHandle_t handle);
	void CopyEntityBaseline( int iFrom, int iTo );
	void FreeEntityBaselines();
	bool GetClassBaseline( int iClass, SerializedEntityHandle_t *pHandle );
	void UpdateInstanceBaseline( int nStringNumber );
	ClientClass *GetClientClass( int i );

	void ForceFullUpdate( char const *pchReason );
	void SendStringCmd(const char * command);
	void ReadPacketEntities( CEntityReadInfo &u );

	int				GetViewEntity();
	void			HandleDeferredConnection();
	void			SetConnectionPassword( char const *pchCurrentPW );
	void			ResetConnectionRetries();
	bool IsClientStateTv(  ) const;


	bool IsClientStateTvBase() const { return false; }
public:

	bool InternalProcessStringCmd( const ks::net::CNETMsg_StringCmd& msg );

public:
	void SendReserveServerMsg();	
	void SendReserveServerChallenge();
	void BuildReserveServerPayload( bf_write &msg, int nChallengeNr );

	void SendReservationCheckMsg( const ns_address &netAdrPublic, uint64 nServerReservationCookie );

	int FindSplitPlayerSlot( int nPlayerIndex );

	void ConnectInternal( const char *pchPublicAddress, char const *pchPrivateAddress, int numPlayers, const char* szJoinType );

	void RememberIPAddressForLobby( uint64 unLobbyID, const ns_address &adrRemote );
	bool IsRemoteInList( char const *pchAdrCheck ) const;
	bool ShouldUseDirectConnectAddress( const CAddressList &list ) const;

public:
	// Connection to server.			
	int				m_Socket;		// network socket 
	INetChannel		*m_NetChannel;		// Our sequenced channel to the remote server.
	bool			m_bSplitScreenUser;
	unsigned int	m_nChallengeNr;	// connection challenge number
	double			m_flConnectTime;	// If gap of connect_time to net_time > 3000, then resend connect packet
	int				m_nRetryNumber;	// number of retry connection attempts
	int				m_nRetryMax;	// max # of retry attempts allowed

	// Address for actual packets (may  differ from m_szRetryAddress)
	CAddressList	m_Remote;
	
	struct DirectConnectLobby_t
	{
		DirectConnectLobby_t() : m_flEndTime( -1 ), m_unLobbyID( 0ull ) {}

		float		m_flEndTime;
		ns_address	m_adrRemote;
		uint64		m_unLobbyID;
	};

	DirectConnectLobby_t	m_DirectConnectLobby;


	int				m_nSignonState;    // see SIGNONSTATE_* definitions
	double			m_flNextCmdTime; // When can we send the next command packet?
	int				m_nServerCount;	// server identification for prespawns, must match the svs.spawncount which
									// is incremented on server spawning.  This supercedes svs.spawn_issued, in that
									// we can now spend a fair amount of time sitting connected to the server
									// but downloading models, sounds, etc.  So much time that it is possible that the
									// server might change levels again and, if so, we need to know that.
	int			m_nCurrentSequence;	// this is the sequence number of the current incoming packet	

	CClockDriftMgr m_ClockDriftMgr;

	int			m_nDeltaTick;		//	last valid received snapshot (server) tick
	bool		m_bPaused;			// send over by server
	int			m_nViewEntity;		// player point of view override

	int			m_nPlayerSlot;		// own player entity index-1. skips world. Add 1 to get cl_entitites index;
	int			m_nSplitScreenSlot;

	char		m_szLevelName[ MAX_PATH ];	// for display on solo scoreboard
	char		m_szLevelNameShort[ 40 ]; // removes maps/ and .bsp extension
	char		m_szMapGroupName[ 40 ]; //the name of the map group we are currently playing in
	char		m_szLastLevelNameShort[ 40 ]; // stores the previous value of m_szLevelNameShort when that gets cleared

	int			m_nMaxClients;		// max clients on server
	int			m_nNumPlayersToConnect;	// number of clients to connect to server.

	class CAsyncOperation_ReserveServer : public IMatchAsyncOperation
	{
	public:
		explicit CAsyncOperation_ReserveServer( CBaseClientState *pParent ) : m_eState( AOS_RUNNING ), m_pParent( pParent ) 
		{
			m_numGameSlotsForReservation = 0;
		}

		bool IsFinished() { return m_eState > AOS_ABORTING; }
		AsyncOperationState_t GetState() { return m_eState; }
		uint64 GetResult();
		uint64 GetResultExtraInfo() { return m_numGameSlotsForReservation; }
		void Abort() {}
		void Release()
		{
			if ( m_pParent && m_pParent->m_pServerReservationOperation == this )
			{
				m_pParent->m_pServerReservationOperation = nullptr;
				m_pParent->m_pServerReservationCallback = nullptr;
			}
			delete this;
		}

	public:
		AsyncOperationState_t m_eState;
		ns_address m_adr;
		CBaseClientState *m_pParent;
		uint32 m_numGameSlotsForReservation;
	};
	
	CAsyncOperation_ReserveServer *m_pServerReservationOperation;			// server reservation operation
	IMatchAsyncOperationCallback *m_pServerReservationCallback;		// callback for pending reservation request

	std::vector< CServerMsg_CheckReservation * > m_arrSvReservationCheck;
	std::vector< CServerMsg_Ping * > m_arrSvPing;

	uint64 m_nServerReservationCookie;								// cookie to set during reservation and provide upon connection
	KeyValues *m_pKVGameSettings;									// game settings to request on reserved server
	double m_flReservationMsgSendTime;								// time we last sent reservation msg
	int m_nReservationMsgRetryNumber;								// # of times we've retried sending reservation msg
	CAddressList m_netadrReserveServer;									// netadr of server we're trying to reserve
	bool	m_bEnteredPassword;
	bool	m_bWaitingForPassword;

	DeferredConnection_t			m_DeferredConnection;
	std::map< int32, std::unique_ptr< byte[] > > m_mapGeneratedEncryptionKeys;

	PackedEntity	*m_pEntityBaselines[2][MAX_EDICTS];	// storing entity baselines
		
	// This stuff manages the receiving of data tables and instantiating of client versions
	// of server-side classes.
	C_ServerClassInfo	*m_pServerClasses;
	int					m_nServerClasses;
	int					m_nServerClassBits;
	char				m_szEncryptionKey[AUTH_KEYSIZE];
	unsigned int		m_iEncryptionKeySize;

	CNetworkStringTableContainer *m_StringTableContainer;

	std::map< int, SerializedEntityHandle_t > m_BaselineHandles;

	typedef std::map< uint32, std::unique_ptr< CNETMsg_PlayerAvatarData_t > > PlayerAvatarDataMap_t;
	PlayerAvatarDataMap_t m_mapPlayerAvatarData;
	
	bool m_bRestrictServerCommands;	// If true, then the server is only allowed to execute commands marked with FCVAR_SERVER_CAN_EXECUTE on the client.
	bool m_bRestrictClientCommands;	// If true, then IVEngineClient::ClientCmd is only allowed to execute commands marked with FCVAR_CLIENTCMD_CAN_EXECUTE on the client.

	// tracks valid reception of server info
	bool m_bServerInfoProcessed;
	int m_nServerProtocolVersion;
	int m_nServerInfoMsgProtocol;
	bool m_bServerConnectionRedirect;
};


inline CClockDriftMgr& CBaseClientState::GetClockDriftMgr()
{
	return m_ClockDriftMgr;
}


inline void CBaseClientState::SetClientTickCount( int tick )
{
	m_ClockDriftMgr.m_nClientTick = tick;
}

inline int CBaseClientState::GetClientTickCount() const
{
	return m_ClockDriftMgr.m_nClientTick;
}

inline int CBaseClientState::GetServerTickCount() const
{
	return m_ClockDriftMgr.m_nServerTick;
}

inline void CBaseClientState::SetServerTickCount( int tick )
{
	m_ClockDriftMgr.m_nServerTick = tick;
}

inline void CBaseClientState::SetClientAndServerTickCount( int tick )
{
	m_ClockDriftMgr.m_nServerTick = m_ClockDriftMgr.m_nClientTick = tick;
}
