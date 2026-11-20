import ks.core.vprof;
import ks.tier1.bitbuf;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// CBaseClient: calls of the shared implementation to the game / HLTV client class (dispatch on m_client_kind).
// Included by netdispatch.cpp.

void CBaseClient::ActivatePlayer()
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->ActivatePlayer(); break;
	default: ActivatePlayerBase(); break;
	}
}
bool CBaseClient::CLCMsg_ClientInfo( const ks::net::CCLCMsg_ClientInfo& msg )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->CLCMsg_ClientInfo( msg );
	default: return static_cast<CHLTVClient*>( this )->CLCMsg_ClientInfo( msg );
	}
}
bool CBaseClient::CLCMsg_CmdKeyValues( const ks::net::CCLCMsg_CmdKeyValues& msg )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->CLCMsg_CmdKeyValues( msg );
	default: return CLCMsg_CmdKeyValuesBase( msg );
	}
}
bool CBaseClient::CLCMsg_FileCRCCheck( const ks::net::CCLCMsg_FileCRCCheck& msg )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->CLCMsg_FileCRCCheck( msg );
	default: return static_cast<CHLTVClient*>( this )->CLCMsg_FileCRCCheck( msg );
	}
}
bool CBaseClient::CLCMsg_HltvReplay( const ks::net::CCLCMsg_HltvReplay &msg )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->CLCMsg_HltvReplay( msg );
	default: return CLCMsg_HltvReplayBase( msg );
	}
}
bool CBaseClient::CLCMsg_ListenEvents( const ks::net::CCLCMsg_ListenEvents& msg )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return CLCMsg_ListenEventsBase( msg );
	default: return static_cast<CHLTVClient*>( this )->CLCMsg_ListenEvents( msg );
	}
}
bool CBaseClient::CLCMsg_Move( const ks::net::CCLCMsg_Move& msg )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->CLCMsg_Move( msg );
	default: return static_cast<CHLTVClient*>( this )->CLCMsg_Move( msg );
	}
}
bool CBaseClient::CLCMsg_RespondCvarValue( const ks::net::CCLCMsg_RespondCvarValue& msg )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->CLCMsg_RespondCvarValue( msg );
	default: return static_cast<CHLTVClient*>( this )->CLCMsg_RespondCvarValue( msg );
	}
}
bool CBaseClient::CLCMsg_VoiceData( const ks::net::CCLCMsg_VoiceData& msg )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->CLCMsg_VoiceData( msg );
	default: return static_cast<CHLTVClient*>( this )->CLCMsg_VoiceData( msg );
	}
}
bool CBaseClient::CanStartHltvReplay()
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->CanStartHltvReplay();
	default: return CanStartHltvReplayBase();
	}
}
bool CBaseClient::CheckConnect()
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->CheckConnect();
	default: return CheckConnectBase();
	}
}
void CBaseClient::Clear()
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->Clear(); break;
	default: ClearBase(); break;
	}
}
void CBaseClient::Connect( const char * szName, int nUserID, INetChannel *pNetChannel, bool bFakePlayer, CrossPlayPlatform_t clientPlatform, const ks::net::CMsg_CVars *pVecCvars )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->Connect( szName, nUserID, pNetChannel, bFakePlayer, clientPlatform, pVecCvars ); break;
	default: ConnectBase( szName, nUserID, pNetChannel, bFakePlayer, clientPlatform, pVecCvars ); break;
	}
}
void CBaseClient::ConnectionClosing( const char *reason )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->ConnectionClosing( reason ); break;
	default: static_cast<CHLTVClient*>( this )->ConnectionClosing( reason ); break;
	}
}
void CBaseClient::ConnectionCrashed( const char *reason )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->ConnectionCrashed( reason ); break;
	default: static_cast<CHLTVClient*>( this )->ConnectionCrashed( reason ); break;
	}
}
void CBaseClient::Disconnect( const char *reason )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->Disconnect( reason ); break;
	default: DisconnectBase( reason ); break;
	}
}
bool CBaseClient::ExecuteStringCommand( const char *s )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->ExecuteStringCommand( s );
	default: return static_cast<CHLTVClient*>( this )->ExecuteStringCommand( s );
	}
}
void CBaseClient::FileDenied( const char *fileName, unsigned int transferID, bool isReplayDemoFile )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->FileDenied( fileName, transferID, isReplayDemoFile ); break;
	default: static_cast<CHLTVClient*>( this )->FileDenied( fileName, transferID, isReplayDemoFile ); break;
	}
}
void CBaseClient::FileReceived( const char *fileName, unsigned int transferID, bool isReplayDemoFile )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->FileReceived( fileName, transferID, isReplayDemoFile ); break;
	default: static_cast<CHLTVClient*>( this )->FileReceived( fileName, transferID, isReplayDemoFile ); break;
	}
}
void CBaseClient::FileRequested( const char *fileName, unsigned int transferID, bool isReplayDemoFile )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->FileRequested( fileName, transferID, isReplayDemoFile ); break;
	default: static_cast<CHLTVClient*>( this )->FileRequested( fileName, transferID, isReplayDemoFile ); break;
	}
}
void CBaseClient::FileSent( const char *fileName, unsigned int transferID, bool isReplayDemoFile )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->FileSent( fileName, transferID, isReplayDemoFile ); break;
	default: static_cast<CHLTVClient*>( this )->FileSent( fileName, transferID, isReplayDemoFile ); break;
	}
}
CClientFrame * CBaseClient::GetDeltaFrame( int nTick )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->GetDeltaFrame( nTick );
	default: return static_cast<CHLTVClient*>( this )->GetDeltaFrame( nTick );
	}
}
int CBaseClient::GetHltvReplayDelay()
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->GetHltvReplayDelay();
	default: return GetHltvReplayDelayBase();
	}
}
const char * CBaseClient::GetHltvReplayStatus() const
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<const CGameClient*>( this )->GetHltvReplayStatus();
	default: return GetHltvReplayStatusBase();
	}
}
CBaseClient * CBaseClient::GetPropCullClient()
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->GetPropCullClient();
	default: return GetPropCullClientBase();
	}
}
bool CBaseClient::IgnoreTempEntity( CEventInfo *event )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->IgnoreTempEntity( event );
	default: return IgnoreTempEntityBase( event );
	}
}
void CBaseClient::Inactivate()
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->Inactivate(); break;
	default: InactivateBase(); break;
	}
}
bool CBaseClient::IsHearingClient( int index ) const
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<const CGameClient*>( this )->IsHearingClient( index );
	default: return IsHearingClientBase( index );
	}
}
bool CBaseClient::IsProximityHearingClient( int index ) const
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<const CGameClient*>( this )->IsProximityHearingClient( index );
	default: return IsProximityHearingClientBase( index );
	}
}
bool CBaseClient::NETMsg_PlayerAvatarData( const ks::net::CNETMsg_PlayerAvatarData& msg )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return NETMsg_PlayerAvatarDataBase( msg );
	default: return static_cast<CHLTVClient*>( this )->NETMsg_PlayerAvatarData( msg );
	}
}
bool CBaseClient::NETMsg_SetConVar( const ks::net::CNETMsg_SetConVar& msg )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return NETMsg_SetConVarBase( msg );
	default: return static_cast<CHLTVClient*>( this )->NETMsg_SetConVar( msg );
	}
}
void CBaseClient::PacketEnd()
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->PacketEnd(); break;
	default: static_cast<CHLTVClient*>( this )->PacketEnd(); break;
	}
}
void CBaseClient::PacketStart( int incoming_sequence, int outgoing_acknowledged )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->PacketStart( incoming_sequence, outgoing_acknowledged ); break;
	default: static_cast<CHLTVClient*>( this )->PacketStart( incoming_sequence, outgoing_acknowledged ); break;
	}
}
void CBaseClient::PerformDisconnection( const char *pReason )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->PerformDisconnection( pReason ); break;
	default: PerformDisconnectionBase( pReason ); break;
	}
}
bool CBaseClient::ProcessSignonStateMsg( int state, int spawncount )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->ProcessSignonStateMsg( state, spawncount );
	default: return static_cast<CHLTVClient*>( this )->ProcessSignonStateMsg( state, spawncount );
	}
}
void CBaseClient::Reconnect()
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->Reconnect(); break;
	default: ReconnectBase(); break;
	}
}
void CBaseClient::ResetReplayRequestTime()
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->ResetReplayRequestTime(); break;
	default: ResetReplayRequestTimeBase(); break;
	}
}
bool CBaseClient::SVCMsg_UserMessage( const ks::net::CSVCMsg_UserMessage &msg )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->SVCMsg_UserMessage( msg );
	default: return SVCMsg_UserMessageBase( msg );
	}
}
bool CBaseClient::SendNetMsg( INetMessage &msg, bool bForceReliable, bool bVoice )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->SendNetMsg( msg, bForceReliable, bVoice );
	default: return SendNetMsgBase( msg, bForceReliable, bVoice );
	}
}
bool CBaseClient::SendSignonData()
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->SendSignonData();
	default: return static_cast<CHLTVClient*>( this )->SendSignonData();
	}
}
bool CBaseClient::SendSnapshot( CClientFrame *pFrame )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->SendSnapshot( pFrame );
	default: return static_cast<CHLTVClient*>( this )->SendSnapshot( pFrame );
	}
}
void CBaseClient::SetRate( int nRate, bool bForce )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->SetRate( nRate, bForce ); break;
	default: static_cast<CHLTVClient*>( this )->SetRate( nRate, bForce ); break;
	}
}
void CBaseClient::SetUpdateRate( float fUpdateRate, bool bForce )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->SetUpdateRate( fUpdateRate, bForce ); break;
	default: static_cast<CHLTVClient*>( this )->SetUpdateRate( fUpdateRate, bForce ); break;
	}
}
bool CBaseClient::ShouldSendMessages()
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->ShouldSendMessages();
	default: return static_cast<CHLTVClient*>( this )->ShouldSendMessages();
	}
}
void CBaseClient::SpawnPlayer()
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->SpawnPlayer(); break;
	default: static_cast<CHLTVClient*>( this )->SpawnPlayer(); break;
	}
}
bool CBaseClient::StartHltvReplay( const HltvReplayParams_t &params )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->StartHltvReplay( params );
	default: return StartHltvReplayBase( params );
	}
}
void CBaseClient::StopHltvReplay()
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->StopHltvReplay(); break;
	default: StopHltvReplayBase(); break;
	}
}
bool CBaseClient::UpdateAcknowledgedFramecount( int tick )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: return static_cast<CGameClient*>( this )->UpdateAcknowledgedFramecount( tick );
	default: return UpdateAcknowledgedFramecountBase( tick );
	}
}
void CBaseClient::UpdateUserSettings()
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->UpdateUserSettings(); break;
	default: static_cast<CHLTVClient*>( this )->UpdateUserSettings(); break;
	}
}
void CBaseClient::WriteGameSounds( bf_write &buf, int nMaxSounds )
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: static_cast<CGameClient*>( this )->WriteGameSounds( buf, nMaxSounds ); break;
	default: WriteGameSoundsBase( buf, nMaxSounds ); break;
	}
}

void CBaseClient::DeleteThis()
{
	switch ( m_client_kind )
	{
	case client_kind_t::game: delete static_cast<CGameClient*>( this ); break;
	default: delete static_cast<CHLTVClient*>( this ); break;
	}
}
