#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: CGameClient: represents a player client in a game server

#include "const.h"
#include "net.h"
#include "event_system.h"
#include "protocol.h"
#include <inetmsghandler.h>
#include "baseclient.h"
#include "clientframe.h"
#include <soundinfo.h>
import ks.tier1;
import std;

struct HltvReplayStats_t
{
	enum FailEnum_t
	{
		FAILURE_ALREADY_IN_REPLAY,
		FAILURE_TOO_FREQUENT,
		FAILURE_NO_FRAME,
		FAILURE_NO_FRAME2,
		FAILURE_CANNOT_MATCH_DELAY,
		FAILURE_FRAME_NOT_READY,
		NUM_FAILURES
	};
	uint nClients;
	uint nStartRequests;
	uint nSuccessfulStarts;
	uint nStopRequests;
	uint nAbortStopRequests;
	uint nUserCancels;
	uint nFullReplays;
	uint nNetAbortReplays;
	uint nFailedReplays[NUM_FAILURES];
	HltvReplayStats_t()
	{
		Reset();
	}

	void Reset()
	{
		V_memset( this, 0, sizeof( *this ) );
		nClients = 1;
	}
	void operator += ( const HltvReplayStats_t &other )
	{
		this->nClients			+= other.nClients;
		this->nStartRequests	+= other.nStartRequests;
		this->nSuccessfulStarts	+= other.nSuccessfulStarts;
		this->nStopRequests		+= other.nStopRequests;
		this->nUserCancels		+= other.nUserCancels;
		this->nAbortStopRequests += other.nAbortStopRequests;
		this->nFullReplays		+= other.nFullReplays;
		this->nNetAbortReplays += other.nNetAbortReplays;
		for ( int i = 0; i < NUM_FAILURES; ++i )
			this->nFailedReplays[i] += other.nFailedReplays[i];
	}
	const char *AsString()const;
};

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class	INetChannel;
class	CClientFrame;
class	CFrameSnapshot;
class	CClientMsgHandler;
struct	edict_t;
struct	SoundInfo_t;
class	CHLTVServer;
class	CReplayServer;
class	CPerClientLogoInfo;
class	CCommand;
class	CHLTVClient;

//-----------------------------------------------------------------------------
// CGameClient: represents a player client in a game server
//-----------------------------------------------------------------------------
class CGameClient : public CBaseClient, public CClientFrameManager
{
	typedef CBaseClient BaseClass;

public:
	CGameClient(int slot, CBaseServer *pServer);
	~CGameClient();

	// INetMsgHandler interface
	void ConnectionClosing( const char *reason );
	void ConnectionCrashed(const char *reason);
	
	void PacketStart	(int incoming_sequence, int outgoing_acknowledged);
	void PacketEnd( void );
	
	void FileReceived( const char *fileName, unsigned int transferID, bool bIsReplayDemoFile = false );
	void FileRequested(const char *fileName, unsigned int transferID, bool bIsReplayDemoFile = false );
	void FileDenied( const char *fileName, unsigned int transferID, bool bIsReplayDemoFile = false );
	void FileSent( const char *fileName, unsigned int transferID, bool bIsReplayDemoFile = false );
	
	bool ProcessConnectionlessPacket( netpacket_t *packet );

	// IClient interface
	void	Connect(const char * szName, int nUserID, INetChannel *pNetChannel, bool bFakePlayer, CrossPlayPlatform_t clientPlatform, const ks::net::CMsg_CVars *pVecCvars );
	void	Inactivate( void );
	void	Reconnect( void );
	void	Disconnect( const char *reason );
	bool	CheckConnect( void );
	
	void	SetRate( int nRate, bool bForce );
	void	SetUpdateRate( float fUpdateRate, bool bForce ); // override;
	
	bool	IsHearingClient( int index ) const;
	bool	IsProximityHearingClient( int index ) const;

	void	Clear( void );

	bool	SendNetMsg( INetMessage &msg, bool bForceReliable = false, bool bVoice = false );
	bool	ExecuteStringCommand( const char *s );
	// game event listener (bound to m_GameEventCallback by the constructor)
	void	FireGameEvent( IGameEvent *event );

public: // IClientMessageHandlers
	
	bool CLCMsg_ClientInfo( const ks::net::CCLCMsg_ClientInfo& msg );
	bool CLCMsg_Move( const ks::net::CCLCMsg_Move& msg );
	bool CLCMsg_VoiceData( const ks::net::CCLCMsg_VoiceData& msg );

	bool CLCMsg_RespondCvarValue( const ks::net::CCLCMsg_RespondCvarValue& msg );
	bool CLCMsg_FileCRCCheck( const ks::net::CCLCMsg_FileCRCCheck& msg );
	bool CLCMsg_CmdKeyValues( const ks::net::CCLCMsg_CmdKeyValues& msg );

	bool CLCMsg_HltvReplay( const ks::net::CCLCMsg_HltvReplay &msg );

	bool SVCMsg_UserMessage( const ks::net::CSVCMsg_UserMessage &msg );
	
public:

	void	UpdateUserSettings( void );
	bool	UpdateAcknowledgedFramecount(int tick);

	void	WriteGameSounds( bf_write &buf, int nMaxSounds );
	bool	ProcessSignonStateMsg(int state, int spawncount);
	bool	SendSnapshot( CClientFrame *pFrame );
	bool	SendHltvReplaySnapshot( CClientFrame * pFrame );
	bool	ShouldSendMessages( void );
	void	SpawnPlayer( void );
	bool	SendSignonData( void );
	void	ActivatePlayer( void );
	
	void	SetupHltvFrame( int nServerTick );
	void	SetupPackInfo( CFrameSnapshot *pSnapshot );
	void	SetupPrevPackInfo();
	
	void	DownloadCustomizations();
	void	WriteViewAngleUpdate( void );
	CClientFrame *GetDeltaFrame( int nTick );
	CClientFrame *GetSendFrame();
	void	SendSound( SoundInfo_t &sound, bool isReliable );
	void	GetReplayData( int& ticks, int& entity);
	bool	IgnoreTempEntity( CEventInfo *event );
	const CCheckTransmitInfo* GetPrevPackInfo();
	bool StartHltvReplay( const HltvReplayParams_t &params );
	bool CanStartHltvReplay();
	void ResetReplayRequestTime();
	void StepHltvReplayStatus( int nServerTick );
	int GetHltvReplayDelay() { return m_nHltvReplayDelay; }
	const char * GetHltvReplayStatus()const { return m_HltvReplayStats.AsString(); }
	void StopHltvReplay();
	bool	IsHltvReplay() { return m_nHltvReplayDelay > 0; }
	//CHLTVServer *GetCurrentHltvReplayServerSource() { return IsHltvReplay() ? m_pHltvReplayServer : NULL; }
	CBaseClient *GetPropCullClient();
public:
	void	PerformDisconnection( const char *pReason );

public:
	bool	IsEngineClientCommand( const CCommand &args ) const;
	int		FillSoundsMessage( ks::net::CSVCMsg_Sounds &msg, int nMaxSounds );
			
public:

	bool								m_bVoiceLoopback; // if true, client wants own voice loopback
	CPlayerBitVec						m_VoiceStreams;	  // Which other clients does this guy's voice stream go to?
	CPlayerBitVec						m_VoiceProximity; // Should we use proximity for this guy?

	int						m_LastMovementTick;	// for move commands

	int						m_nSoundSequence;	// increases with each reliable sound

	// Identity information.
	edict_t					*edict;				// EDICT_NUM(clientnum+1)
	std::vector< SoundInfo_t >	m_Sounds;			// game sounds
		
	const edict_t			*m_pViewEntity;		// View Entity (camera or the client itself)

	CClientFrame			*m_pCurrentFrame;	// last added frame

	CCheckTransmitInfo		m_PackInfo;

	bool					m_bIsInReplayMode;
	CCheckTransmitInfo		m_PrevPackInfo;		// Used to speed up CheckTransmit.
	std::bitset<MAX_EDICTS>		m_PrevTransmitEdict;
	float					m_flTimeClientBecameFullyConnected;
	double					m_flLastClientCommandQuotaStart;
	int						m_numClientCommandsInQuota;

	int					m_nHltvReplayDelay;
	CHLTVServer			*m_pHltvReplayServer;
	int					m_nHltvReplayStopAt;
	int					m_nHltvReplayStartAt;
	int					m_nHltvReplaySlowdownBeginAt;
	int					m_nHltvReplaySlowdownEndAt;
	float				m_flHltvReplaySlowdownRate;
	int					m_nHltvLastSendTick;	// last send tick, don't send ticks twice
	float				m_flHltvLastReplayRequestTime; // tick at which the last time replay was requested

	std::vector< std::unique_ptr< INetMessage, INetMessageDeleter > > m_HltvQueuedMessages; // it'd probably be more optimal, memory-wise, to make a linked list, but a vector is more readable and debuggable


	HltvReplayStats_t	m_HltvReplayStats;
};
