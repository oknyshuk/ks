#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include <const.h>
#include <iclient.h>
#include <protocol.h>
#include <iservernetworkable.h>
#include <bspfile.h>
#include <igameevents.h>
import ks.core.basetypes;
import ks.core.types;
import ks.core.vprof;
import ks.platform;
import ks.simd;
import ks.tier1.bitbuf;
import ks.tier1.checksum_crc;
import ks.tier1.convar;
import ks.tier1.cvar;
import ks.tier1.icvar;
import ks.tier1.interfaces;
import ks.tier1.intrusive_ptr;
import ks.tier1.keyvalues;
import ks.tier1.playerid;
import std;
#include "userid.h"
#include "netmessages.h"

// class CClientFrame;
class CBaseServer;
class CClientFrame;
struct player_info_s;
class CFrameSnapshot;
class CEventInfo;
class CCommand;
struct NetMessageCvar_t;
class CHLTVServer;

struct Spike_t
{
public:
	Spike_t() : 
		m_nBits( 0 )
	{
		m_szDesc[ 0 ] = 0;
	}
	char	m_szDesc[ 64 ];
	int		m_nBits;
};

class CNetworkStatTrace
{
public:
	CNetworkStatTrace() : 
		m_nMinWarningBytes( 0 ), m_nStartBit( 0 ), m_nCurBit( 0 )
	{
	}
	int						m_nMinWarningBytes;
	int						m_nStartBit;
	int						m_nCurBit;
	std::vector< Spike_t >	m_Records;
};


class CBaseClient : public IClient
{
	typedef struct CustomFile_s
	{
		CRC32_t			crc;	//file CRC
		unsigned int	reqID;	// download request ID
	} CustomFile_t;

public:
	enum class client_kind_t : unsigned char { game, hltv };

	explicit CBaseClient( client_kind_t kind );
	~CBaseClient();

	// Deletes the object as the class it really is
	void DeleteThis();

public:

	int			GetPlayerSlot() const { return m_nClientSlot; };
	int			GetUserID() const { return m_UserID; };
	const USERID_t	GetNetworkID() const;
	const char		*GetClientName() const { return m_Name; };
	INetChannel		*GetNetChannel() { return m_NetChannel; };
	IServer			*GetServer() { return (IServer*)m_Server; };
	CHLTVServer		*GetHltvServer();
	CHLTVServer		*GetAnyConnectedHltvServer();
	const char		*GetUserSetting(const char *cvar) const;
	const char		*GetNetworkIDString() const;
	uint64			GetClientXuid() const;
	const char		*GetFriendsName() const { return m_FriendsName; }
	void			UpdateName( const char *pszDefault );
	void Connect( const char * szName, int nUserID, INetChannel *pNetChannel, bool bFakePlayer, CrossPlayPlatform_t clientPlatform, const ks::net::CMsg_CVars *pVecCvars = nullptr );


	void	ConnectBase(const char * szName, int nUserID, INetChannel *pNetChannel, bool bFakePlayer, CrossPlayPlatform_t clientPlatform, const ks::net::CMsg_CVars *pVecCvars = nullptr );
	void Inactivate( void );

	void	InactivateBase( void );
	void Reconnect( void );

	void	ReconnectBase( void );
	void Disconnect( const char *reason );

	void	DisconnectBase( const char *reason );
	bool CheckConnect( void );

	bool	CheckConnectBase( void );
	bool	ChangeSplitscreenUser( int nSplitScreenUserSlot );
	void SetRate( int nRate, bool bForce );


	void	SetRateBase( int nRate, bool bForce );
	int		GetRate( void ) const;
	void SetUpdateRate( float fUpdateRate, bool bForce );

	
	void	SetUpdateRateBase( float fUpdateRate, bool bForce ); // override;
	float	GetUpdateRate( void ) const;
	void Clear( void );
 // override;

	void	ClearBase( void );
	void	DemoRestart( void ); // called when client started demo recording

	int		GetMaxAckTickCount() const;
	bool ExecuteStringCommand( const char *s );


	bool	ExecuteStringCommandBase( const char *s );
	bool SendNetMsg( INetMessage &msg, bool bForceReliable = false, bool bVoice = false );

	bool	SendNetMsgBase( INetMessage &msg, bool bForceReliable = false, bool bVoice = false );
	
	[[gnu::format(printf, 2, 3)]] void	ClientPrintf ( const char *fmt, ...);
	void	ClientPrintfV ( const char *fmt, std::va_list argptr );

	bool	IsConnected( void ) const { return m_nSignonState >= SIGNONSTATE_CONNECTED; };	
	bool	IsSpawned( void ) const { return m_nSignonState >= SIGNONSTATE_NEW; };	
	bool	IsActive( void ) const { return m_nSignonState == SIGNONSTATE_FULL; };
	bool	IsFakeClient( void ) const { return m_bFakePlayer; };
	bool	IsHLTV( void ) const { return m_bIsHLTV; }
	bool	IsReplay( void ) const { return false; }
	// Is an actual human player or splitscreen player (not a bot and not a HLTV slot)
	bool	IsHumanPlayer() const;
	bool IsHearingClient( int index ) const;

	bool	IsHearingClientBase( int index ) const { return false; };
	bool IsProximityHearingClient( int index ) const;

	bool	IsProximityHearingClientBase( int index ) const { return false; };
	bool	IsLowViolenceClient( void ) const { return m_bLowViolence; }

	void	SetMaxRoutablePayloadSize( int nMaxRoutablePayloadSize );

	bool	IsSplitScreenUser( void ) const { return m_bSplitScreenUser; }
	CrossPlayPlatform_t GetClientPlatform() const { return m_ClientPlatform; }

public: // Message Handlers
	
	bool NETMsg_Tick( const ks::net::CNETMsg_Tick& msg );
	bool NETMsg_StringCmd( const ks::net::CNETMsg_StringCmd& msg );
	bool NETMsg_SignonState( const ks::net::CNETMsg_SignonState& msg );
	bool NETMsg_PlayerAvatarData( const ks::net::CNETMsg_PlayerAvatarData& msg );

	bool NETMsg_PlayerAvatarDataBase( const ks::net::CNETMsg_PlayerAvatarData& msg );
	bool NETMsg_SetConVar( const ks::net::CNETMsg_SetConVar& msg );

	bool NETMsg_SetConVarBase( const ks::net::CNETMsg_SetConVar& msg );
	bool CLCMsg_ClientInfo( const ks::net::CCLCMsg_ClientInfo& msg );


	bool CLCMsg_ClientInfoBase( const ks::net::CCLCMsg_ClientInfo& msg );
	bool CLCMsg_Move( const ks::net::CCLCMsg_Move& msg );

	bool CLCMsg_MoveBase( const ks::net::CCLCMsg_Move& msg ) { contract_assert( 0 ); return true; }
	bool CLCMsg_VoiceData( const ks::net::CCLCMsg_VoiceData& msg );

	bool CLCMsg_VoiceDataBase( const ks::net::CCLCMsg_VoiceData& msg ) { contract_assert( 0 ); return true; }

	bool CLCMsg_BaselineAck( const ks::net::CCLCMsg_BaselineAck& msg );
	bool CLCMsg_ListenEvents( const ks::net::CCLCMsg_ListenEvents& msg );

	bool CLCMsg_ListenEventsBase( const ks::net::CCLCMsg_ListenEvents& msg );
	bool CLCMsg_RespondCvarValue( const ks::net::CCLCMsg_RespondCvarValue& msg );

	bool CLCMsg_RespondCvarValueBase( const ks::net::CCLCMsg_RespondCvarValue& msg ) { contract_assert( 0 ); return true; }
	bool CLCMsg_LoadingProgress( const ks::net::CCLCMsg_LoadingProgress& msg );	
	bool CLCMsg_SplitPlayerConnect( const ks::net::CCLCMsg_SplitPlayerConnect& msg );
	bool CLCMsg_FileCRCCheck( const ks::net::CCLCMsg_FileCRCCheck& msg );

	bool CLCMsg_FileCRCCheckBase( const ks::net::CCLCMsg_FileCRCCheck& msg ) { contract_assert( 0 ); return true; }
	bool CLCMsg_CmdKeyValues( const ks::net::CCLCMsg_CmdKeyValues& msg );

	bool CLCMsg_CmdKeyValuesBase( const ks::net::CCLCMsg_CmdKeyValues& msg );
	bool CLCMsg_HltvReplay( const ks::net::CCLCMsg_HltvReplay &msg );

	bool CLCMsg_HltvReplayBase( const ks::net::CCLCMsg_HltvReplay &msg ) { return false; }
	bool SVCMsg_UserMessage( const ks::net::CSVCMsg_UserMessage &msg );


	bool SVCMsg_UserMessageBase( const ks::net::CSVCMsg_UserMessage &msg ) { return true; }


	enum
	{
		NETMSG_Tick,
		NETMSG_StringCmd,
		NETMSG_SetConVar,
		NETMSG_SignonState,
		NETMSG_ClientInfo,
		NETMSG_Move,
		NETMSG_VoiceData,
		NETMSG_BaselineAck,
		NETMSG_ListenEvents,
		NETMSG_RespondCvarValue,
		NETMSG_SplitPlayerConnect,
		NETMSG_FileCRCCheck,
		NETMSG_LoadingProgress,
		NETMSG_CmdKeyValues,
		NETMSG_PlayerAvatarData,
		NETMSG_HltvReplay,
		NETMSG_UserMessage,
		NETMSG_Max
	};

	std::array< CNetMessageBinder, NETMSG_Max > m_NetMessages;
	void ConnectionClosing( const char *reason );
	void ConnectionCrashed( const char *reason );
	void PacketStart( int incoming_sequence, int outgoing_acknowledged );
	void PacketEnd( void );
	void FileRequested( const char *fileName, unsigned int transferID, bool isReplayDemoFile );
	void FileReceived( const char *fileName, unsigned int transferID, bool isReplayDemoFile );
	void FileDenied( const char *fileName, unsigned int transferID, bool isReplayDemoFile );
	void FileSent( const char *fileName, unsigned int transferID, bool isReplayDemoFile );
	const char * GetHltvReplayStatus(  ) const;

	const char *GetHltvReplayStatusBase() const { return ""; }

	void	ConnectionStart(INetChannel *chan);
	void	ConnectionStop();

public: // game event listener
	void	FireGameEvent( IGameEvent *event ) { SendGameEvent( event, false ); }
	void	SendGameEvent( IGameEvent *event, bool bPassthrough );
	// What the event manager calls for this client; CGameClient rebinds it to its own FireGameEvent.
	GameEventCallback_t	m_GameEventCallback;

public:
	bool UpdateAcknowledgedFramecount( int tick );


	bool	UpdateAcknowledgedFramecountBase(int tick);
	bool ShouldSendMessages( void );

	bool	ShouldSendMessagesBase( void );
	void	UpdateSendState( void );
			void	ForceFullUpdate( void ) { UpdateAcknowledgedFramecount(-1); }
	
	bool	FillUserInfo( player_info_s &userInfo );
	void UpdateUserSettings(  );

	void	UpdateUserSettingsBase();
	void WriteGameSounds( bf_write &buf, int nMaxSounds );

	void	WriteGameSoundsBase(bf_write &buf, int nMaxSounds);
	CClientFrame * GetDeltaFrame( int nTick );

	
	CClientFrame *GetDeltaFrameBase( int nTick );
	bool SendSnapshot( CClientFrame *pFrame );

	bool	SendSnapshotBase( CClientFrame *pFrame );
	bool	SendServerInfo( void );
	bool SendSignonData( void );

	bool	SendSignonDataBase( void );
	void SpawnPlayer( void );

	void	SpawnPlayerBase( void );
	void ActivatePlayer( void );

	void	ActivatePlayerBase( void );
	void	SetName( const char * name );
	void	SetUserCVar( const char *cvar, const char *value);
	void	FreeBaselines();
	bool IgnoreTempEntity( CEventInfo *event );

	bool	IgnoreTempEntityBase( CEventInfo *event );
	
	void			SetPlayerID( const CPlayerId &playerUID );

	int				GetSignonState() const { return m_nSignonState; }
	void			SetSignonState( int nState );

	bool			IsTracing() const;
	void			SetTraceThreshold( int nThreshold );
	[[gnu::format(printf, 3, 4)]] void			TraceNetworkData( bf_write &msg, char const *fmt, ... );
	[[gnu::format(printf, 3, 4)]] void			TraceNetworkMsg( int nBits, char const *fmt, ... );

	bool			IsFullyAuthenticated( void ) { return m_bFullyAuthenticated; }
	void			SetFullyAuthenticated( void ) { m_bFullyAuthenticated = true; }

	void			SplitScreenDisconnect( const CCommand &args );

	void			DisconnectSplitScreenUser( CBaseClient *pSplitClient );

	void			ApplyConVars( const ks::net::CMsg_CVars& list, bool bCreateIfNotExisting );
	
	void			FillSignOnFullServerInfo( class CNETMsg_SignonState_t& state );
	bool			IsSplitScreenPartner( const CBaseClient *pOther ) const;
	IClient	*GetSplitScreenOwner() { return m_pAttachedTo; }
	
	int		GetNumPlayers();
	bool StartHltvReplay( const HltvReplayParams_t &params );

	bool	StartHltvReplayBase( const HltvReplayParams_t &params ) { return false; }
	void StopHltvReplay(  );
 // not implemented for most clients
	void	StopHltvReplayBase() { }
	int GetHltvReplayDelay(  );

	int		GetHltvReplayDelayBase() { return 0; }
	bool CanStartHltvReplay(  );

	bool	CanStartHltvReplayBase() { return false; }
	void ResetReplayRequestTime(  );

	void	ResetReplayRequestTimeBase() { }
	CBaseClient * GetPropCullClient(  );

	CBaseClient *GetPropCullClientBase() { return this; }
	void			OverrideSignonStateTransparent( int nState ){ m_nSignonState = nState; }
public:
	bool ProcessSignonStateMsg( int state, int spawncount );

	bool	ProcessSignonStateMsgBase(int state, int spawncount);
	void PerformDisconnection( const char *pReason );

	void	PerformDisconnectionBase( const char *pReason );

	void			OnPlayerAvatarDataChanged();

public:	
	void			OnRequestFullUpdate( char const *pchReason );
	int				GetAvailableSplitScreenSlot() const;
	void			SendFullConnectEvent();

public:

	client_kind_t	m_client_kind;		// the class this object really is (see facade/baseclient.inl)

	// One of the subservient split screen users?
	bool			m_bSplitScreenUser;
	bool			m_bSplitAllowFastDisconnect;
	int				m_nSplitScreenPlayerSlot;
	CBaseClient		*m_SplitScreenUsers[ MAX_SPLITSCREEN_CLIENTS ];
	CBaseClient		*m_pAttachedTo; // If this is a split screen user, this is the "master"
	bool			m_bSplitPlayerDisconnecting;

	// Array index in svs.clients:
	int				m_nClientSlot;	
	// entity index of this client (different from clientSlot+1 in HLTV and Replay mode):
	int				m_nEntityIndex;	
	
	int				m_UserID;			// identifying number on server
	char			m_Name[MAX_PLAYER_NAME_LENGTH];			// for printing to other people
	char			m_GUID[SIGNED_GUID_LEN + 1]; // the clients CD key
	CNETMsg_PlayerAvatarData_t m_msgAvatarData;	// Client avatar

	CPlayerId		m_PlayerUID;			// This is valid when the client is authenticated
	
	uint32			m_nFriendsID;		// client's friends' ID
	char			m_FriendsName[MAX_PLAYER_NAME_LENGTH];

	KeyValues		*m_ConVars;			// stores all client side convars
	bool			m_bConVarsChanged;	// true if convars updated and not changes process yet
	bool			m_bSendServerInfo;	// true if we need to send server info packet to start connect
	CBaseServer		*m_Server;			// pointer to server object
	bool			m_bIsHLTV;			// if this a HLTV proxy ?
	CHLTVServer		*m_pHltvSlaveServer;

	// Client sends this during connection, so we can see if
	//  we need to send sendtable info or if the .dll matches
	CRC32_t			m_nSendtableCRC;

	// a client can have couple of cutomized files distributed to all other players
	CustomFile_t	m_nCustomFiles[MAX_CUSTOM_FILES];
	int				m_nFilesDownloaded;	// counter of how many files we downloaded from this client

	//===== NETWORK ============
	INetChannel		*m_NetChannel;		// The client's net connection.

public:
	int				m_nSignonState;		// connection state

public:
	int				m_nDeltaTick;		// -1 = no compression.  This is where the server is creating the
										// compressed info from.
	int				m_nStringTableAckTick; // Highest tick acked for string tables (usually m_nDeltaTick, except when it's -1)
	int				m_nSignonTick;		// tick the client got his signon data
	ks::intrusive_ptr<CFrameSnapshot> m_pLastSnapshot;	// last send snapshot
	int				m_nLoadingProgress;	// 0..100 progress, only valid during loading

	CFrameSnapshot	*m_pBaseline;			// current entity baselines as a snapshot
	int				m_nBaselineUpdateTick;	// last tick we send client a update baseline signal or -1
	std::bitset<MAX_EDICTS>	m_BaselinesSent;	// baselines sent with last update
	int				m_nBaselineUsed;		// 0/1 toggling flag, singaling client what baseline to use
	
		
	// This is used when we send out a nodelta packet to put the client in a state where we wait 
	// until we get an ack from them on this packet.
	// This is for 3 reasons:
	// 1. A client requesting a nodelta packet means they're screwed so no point in deluging them with data.
	//    Better to send the uncompressed data at a slow rate until we hear back from them (if at all).
	// 2. Since the nodelta packet deletes all client entities, we can't ever delta from a packet previous to it.
	// 3. It can eat up a lot of CPU on the server to keep building nodelta packets while waiting for
	//    a client to get back on its feet.
	int				m_nForceWaitForTick;
	
	bool			m_bFakePlayer;		// JAC: This client is a fake player controlled by the game DLL
	bool			m_bReceivedPacket;	// true, if client received a packet after the last send packet
	bool			m_bLowViolence;		// true if client is in low-violence mode (L4D server needs to know)

	bool			m_bFullyAuthenticated;

	// The datagram is written to after every frame, but only cleared
	// when it is sent out to the client.  overflow is tolerated.

	// Time when we should send next world state update ( datagram )
	double         m_fNextMessageTime;   
	// Default time to wait for next message
	float          m_fSnapshotInterval;

	CrossPlayPlatform_t m_ClientPlatform;

	// Keep these as class members instead of a local variable so that we don't reallocate the
	// strings and their buffers every time.
	CSVCMsg_TempEntities_t m_tempentsmsg;
	CSVCMsg_PacketEntities_t m_packetmsg;

public:
	void				StartTrace( bf_write &msg );
	void				EndTrace( bf_write &msg );


	CNetworkStatTrace	m_Trace;
};
