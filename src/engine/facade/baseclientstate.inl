// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// CBaseClientState: calls of the shared implementation to the game / HLTV client state class (dispatch on
// m_state_kind). Included by netdispatch.cpp.

void CBaseClientState::Clear()
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: static_cast<CClientState*>( this )->Clear(); break;
	default: static_cast<CHLTVClientState*>( this )->Clear(); break;
	}
}
void CBaseClientState::ConnectionClosing( const char *reason )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: static_cast<CClientState*>( this )->ConnectionClosing( reason ); break;
	default: static_cast<CHLTVClientState*>( this )->ConnectionClosing( reason ); break;
	}
}
void CBaseClientState::ConnectionCrashed( const char *reason )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: static_cast<CClientState*>( this )->ConnectionCrashed( reason ); break;
	default: static_cast<CHLTVClientState*>( this )->ConnectionCrashed( reason ); break;
	}
}
void CBaseClientState::ConnectionStart( INetChannel *chan )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: static_cast<CClientState*>( this )->ConnectionStart( chan ); break;
	default: ConnectionStartBase( chan ); break;
	}
}
void CBaseClientState::ConnectionStop()
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: static_cast<CClientState*>( this )->ConnectionStop(); break;
	default: ConnectionStopBase(); break;
	}
}
void CBaseClientState::Disconnect( bool bShowMainMenu )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: static_cast<CClientState*>( this )->Disconnect( bShowMainMenu ); break;
	default: static_cast<CHLTVClientState*>( this )->Disconnect( bShowMainMenu ); break;
	}
}
void CBaseClientState::FileDenied( const char *fileName, unsigned int transferID, bool isReplayDemoFile )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: static_cast<CClientState*>( this )->FileDenied( fileName, transferID, isReplayDemoFile ); break;
	default: FileDeniedBase( fileName, transferID, isReplayDemoFile ); break;
	}
}
void CBaseClientState::FileReceived( const char *fileName, unsigned int transferID, bool isReplayDemoFile )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: static_cast<CClientState*>( this )->FileReceived( fileName, transferID, isReplayDemoFile ); break;
	default: FileReceivedBase( fileName, transferID, isReplayDemoFile ); break;
	}
}
void CBaseClientState::FileRequested( const char *fileName, unsigned int transferID, bool isReplayDemoFile )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: static_cast<CClientState*>( this )->FileRequested( fileName, transferID, isReplayDemoFile ); break;
	default: FileRequestedBase( fileName, transferID, isReplayDemoFile ); break;
	}
}
void CBaseClientState::FullConnect( const ns_address &adr, int nEncryptionKey )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: static_cast<CClientState*>( this )->FullConnect( adr, nEncryptionKey ); break;
	default: FullConnectBase( adr, nEncryptionKey ); break;
	}
}
const char * CBaseClientState::GetCDKeyHash()
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->GetCDKeyHash();
	default: return static_cast<CHLTVClientState*>( this )->GetCDKeyHash();
	}
}
const char * CBaseClientState::GetClientName()
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return GetClientNameBase();
	default: return static_cast<CHLTVClientState*>( this )->GetClientName();
	}
}
int CBaseClientState::GetConnectionRetryNumber() const
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return GetConnectionRetryNumberBase();
	default: return static_cast<const CHLTVClientState*>( this )->GetConnectionRetryNumber();
	}
}
bool CBaseClientState::HookClientStringTable( char const *tableName )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->HookClientStringTable( tableName );
	default: return static_cast<CHLTVClientState*>( this )->HookClientStringTable( tableName );
	}
}
void CBaseClientState::InstallStringTableCallback( char const *tableName )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: static_cast<CClientState*>( this )->InstallStringTableCallback( tableName ); break;
	default: static_cast<CHLTVClientState*>( this )->InstallStringTableCallback( tableName ); break;
	}
}
bool CBaseClientState::IsClientStateTv() const
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return IsClientStateTvBase();
	default: return static_cast<const CHLTVClientState*>( this )->IsClientStateTv();
	}
}
bool CBaseClientState::NETMsg_PlayerAvatarData( const ks::net::CNETMsg_PlayerAvatarData& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return NETMsg_PlayerAvatarDataBase( msg );
	default: return static_cast<CHLTVClientState*>( this )->NETMsg_PlayerAvatarData( msg );
	}
}
bool CBaseClientState::NETMsg_SetConVar( const ks::net::CNETMsg_SetConVar& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return NETMsg_SetConVarBase( msg );
	default: return static_cast<CHLTVClientState*>( this )->NETMsg_SetConVar( msg );
	}
}
bool CBaseClientState::NETMsg_StringCmd( const ks::net::CNETMsg_StringCmd& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->NETMsg_StringCmd( msg );
	default: return static_cast<CHLTVClientState*>( this )->NETMsg_StringCmd( msg );
	}
}
bool CBaseClientState::NETMsg_Tick( const ks::net::CNETMsg_Tick& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->NETMsg_Tick( msg );
	default: return NETMsg_TickBase( msg );
	}
}
void CBaseClientState::PacketEnd()
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: static_cast<CClientState*>( this )->PacketEnd(); break;
	default: static_cast<CHLTVClientState*>( this )->PacketEnd(); break;
	}
}
void CBaseClientState::PacketStart( int incoming_sequence, int outgoing_acknowledged )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: static_cast<CClientState*>( this )->PacketStart( incoming_sequence, outgoing_acknowledged ); break;
	default: PacketStartBase( incoming_sequence, outgoing_acknowledged ); break;
	}
}
bool CBaseClientState::ProcessConnectionlessPacket( struct netpacket_s *packet )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->ProcessConnectionlessPacket( packet );
	default: return ProcessConnectionlessPacketBase( packet );
	}
}
void CBaseClientState::ReadPacketEntities( CEntityReadInfo &u )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: static_cast<CClientState*>( this )->ReadPacketEntities( u ); break;
	default: static_cast<CHLTVClientState*>( this )->ReadPacketEntities( u ); break;
	}
}
void CBaseClientState::RunFrame()
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: static_cast<CClientState*>( this )->RunFrame(); break;
	default: static_cast<CHLTVClientState*>( this )->RunFrame(); break;
	}
}
bool CBaseClientState::SVCMsg_BSPDecal( const ks::net::CSVCMsg_BSPDecal& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->SVCMsg_BSPDecal( msg );
	default: return static_cast<CHLTVClientState*>( this )->SVCMsg_BSPDecal( msg );
	}
}
bool CBaseClientState::SVCMsg_ClassInfo( const ks::net::CSVCMsg_ClassInfo& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->SVCMsg_ClassInfo( msg );
	default: return static_cast<CHLTVClientState*>( this )->SVCMsg_ClassInfo( msg );
	}
}
bool CBaseClientState::SVCMsg_CrosshairAngle( const ks::net::CSVCMsg_CrosshairAngle& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->SVCMsg_CrosshairAngle( msg );
	default: return static_cast<CHLTVClientState*>( this )->SVCMsg_CrosshairAngle( msg );
	}
}
bool CBaseClientState::SVCMsg_EncryptedData( const ks::net::CSVCMsg_EncryptedData& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return SVCMsg_EncryptedDataBase( msg );
	default: return static_cast<CHLTVClientState*>( this )->SVCMsg_EncryptedData( msg );
	}
}
bool CBaseClientState::SVCMsg_EntityMsg( const ks::net::CSVCMsg_EntityMsg& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->SVCMsg_EntityMsg( msg );
	default: return static_cast<CHLTVClientState*>( this )->SVCMsg_EntityMsg( msg );
	}
}
bool CBaseClientState::SVCMsg_FixAngle( const ks::net::CSVCMsg_FixAngle& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->SVCMsg_FixAngle( msg );
	default: return static_cast<CHLTVClientState*>( this )->SVCMsg_FixAngle( msg );
	}
}
bool CBaseClientState::SVCMsg_GameEvent( const ks::net::CSVCMsg_GameEvent& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->SVCMsg_GameEvent( msg );
	default: return static_cast<CHLTVClientState*>( this )->SVCMsg_GameEvent( msg );
	}
}
bool CBaseClientState::SVCMsg_GameEventList( const ks::net::CSVCMsg_GameEventList &msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return SVCMsg_GameEventListBase( msg );
	default: return static_cast<CHLTVClientState*>( this )->SVCMsg_GameEventList( msg );
	}
}
bool CBaseClientState::SVCMsg_Menu( const ks::net::CSVCMsg_Menu& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return SVCMsg_MenuBase( msg );
	default: return static_cast<CHLTVClientState*>( this )->SVCMsg_Menu( msg );
	}
}
bool CBaseClientState::SVCMsg_PacketEntities( const ks::net::CSVCMsg_PacketEntities& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->SVCMsg_PacketEntities( msg );
	default: return static_cast<CHLTVClientState*>( this )->SVCMsg_PacketEntities( msg );
	}
}
bool CBaseClientState::SVCMsg_PaintmapData( const ks::net::CSVCMsg_PaintmapData& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->SVCMsg_PaintmapData( msg );
	default: return static_cast<CHLTVClientState*>( this )->SVCMsg_PaintmapData( msg );
	}
}
bool CBaseClientState::SVCMsg_Prefetch( const ks::net::CSVCMsg_Prefetch& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->SVCMsg_Prefetch( msg );
	default: return static_cast<CHLTVClientState*>( this )->SVCMsg_Prefetch( msg );
	}
}
bool CBaseClientState::SVCMsg_ServerInfo( const ks::net::CSVCMsg_ServerInfo& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->SVCMsg_ServerInfo( msg );
	default: return static_cast<CHLTVClientState*>( this )->SVCMsg_ServerInfo( msg );
	}
}
bool CBaseClientState::SVCMsg_SetPause( const ks::net::CSVCMsg_SetPause& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->SVCMsg_SetPause( msg );
	default: return SVCMsg_SetPauseBase( msg );
	}
}
bool CBaseClientState::SVCMsg_SetView( const ks::net::CSVCMsg_SetView& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return SVCMsg_SetViewBase( msg );
	default: return static_cast<CHLTVClientState*>( this )->SVCMsg_SetView( msg );
	}
}
bool CBaseClientState::SVCMsg_Sounds( const ks::net::CSVCMsg_Sounds& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->SVCMsg_Sounds( msg );
	default: return static_cast<CHLTVClientState*>( this )->SVCMsg_Sounds( msg );
	}
}
bool CBaseClientState::SVCMsg_TempEntities( const ks::net::CSVCMsg_TempEntities& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->SVCMsg_TempEntities( msg );
	default: return static_cast<CHLTVClientState*>( this )->SVCMsg_TempEntities( msg );
	}
}
bool CBaseClientState::SVCMsg_UserMessage( const ks::net::CSVCMsg_UserMessage& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->SVCMsg_UserMessage( msg );
	default: return static_cast<CHLTVClientState*>( this )->SVCMsg_UserMessage( msg );
	}
}
bool CBaseClientState::SVCMsg_VoiceData( const ks::net::CSVCMsg_VoiceData& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->SVCMsg_VoiceData( msg );
	default: return static_cast<CHLTVClientState*>( this )->SVCMsg_VoiceData( msg );
	}
}
bool CBaseClientState::SVCMsg_VoiceInit( const ks::net::CSVCMsg_VoiceInit& msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->SVCMsg_VoiceInit( msg );
	default: return static_cast<CHLTVClientState*>( this )->SVCMsg_VoiceInit( msg );
	}
}
bool CBaseClientState::SetSignonState( int state, int count, const ks::net::CNETMsg_SignonState *msg )
{
	switch ( m_state_kind )
	{
	case state_kind_t::game: return static_cast<CClientState*>( this )->SetSignonState( state, count, msg );
	default: return static_cast<CHLTVClientState*>( this )->SetSignonState( state, count, msg );
	}
}

