// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// CBaseServer: calls of the shared implementation to the game / HLTV server class (dispatch on m_server_kind).
// Included by netdispatch.cpp.

void CBaseServer::Clear()
{
	switch ( m_server_kind )
	{
	case server_kind_t::game: static_cast<CGameServer*>( this )->Clear(); break;
	default: static_cast<CHLTVServer*>( this )->Clear(); break;
	}
}
IClient * CBaseServer::ConnectClient( const ns_address &adr, int protocol, int challenge, int authProtocol, const char *name, const char *password, const char *hashedCDkey, int cdKeyLen, const SplitPlayerConnects_t &splitScreenClients, bool isClientLowViolence, CrossPlayPlatform_t clientPlatform, const byte *pbEncryptionKey, int nEncryptionKeyIndex )
{
	switch ( m_server_kind )
	{
	case server_kind_t::game: return ConnectClientBase( adr, protocol, challenge, authProtocol, name, password, hashedCDkey, cdKeyLen, splitScreenClients, isClientLowViolence, clientPlatform, pbEncryptionKey, nEncryptionKeyIndex );
	default: return static_cast<CHLTVServer*>( this )->ConnectClient( adr, protocol, challenge, authProtocol, name, password, hashedCDkey, cdKeyLen, splitScreenClients, isClientLowViolence, clientPlatform, pbEncryptionKey, nEncryptionKeyIndex );
	}
}
CBaseClient * CBaseServer::CreateNewClient( int slot )
{
	switch ( m_server_kind )
	{
	case server_kind_t::game: return static_cast<CGameServer*>( this )->CreateNewClient( slot );
	default: return static_cast<CHLTVServer*>( this )->CreateNewClient( slot );
	}
}
void CBaseServer::FillServerInfo( ks::net::CSVCMsg_ServerInfo &serverinfo )
{
	switch ( m_server_kind )
	{
	case server_kind_t::game: FillServerInfoBase( serverinfo ); break;
	default: static_cast<CHLTVServer*>( this )->FillServerInfo( serverinfo ); break;
	}
}
bool CBaseServer::FinishCertificateCheck( const ns_address &adr, int nAuthProtocol, const char *szRawCertificate )
{
	switch ( m_server_kind )
	{
	case server_kind_t::game: return FinishCertificateCheckBase( adr, nAuthProtocol, szRawCertificate ); // CGameServer::FinishCertificateCheck( netadr_t & ) never overrode it
	default: return FinishCertificateCheckBase( adr, nAuthProtocol, szRawCertificate );
	}
}
int CBaseServer::GetChallengeType( const ns_address &adr )
{
	switch ( m_server_kind )
	{
	case server_kind_t::game: return GetChallengeTypeBase( adr );
	default: return static_cast<CHLTVServer*>( this )->GetChallengeType( adr );
	}
}
bool CBaseServer::GetClassBaseline( ServerClass *pClass, SerializedEntityHandle_t *pHandle )
{
	switch ( m_server_kind )
	{
	case server_kind_t::game: return GetClassBaselineBase( pClass, pHandle );
	default: return static_cast<CHLTVServer*>( this )->GetClassBaseline( pClass, pHandle );
	}
}
const char * CBaseServer::GetName() const
{
	switch ( m_server_kind )
	{
	case server_kind_t::game: return GetNameBase();
	default: return static_cast<const CHLTVServer*>( this )->GetName();
	}
}
void CBaseServer::GetNetStats( float &avgIn, float &avgOut )
{
	switch ( m_server_kind )
	{
	case server_kind_t::game: GetNetStatsBase( avgIn, avgOut ); break;
	default: static_cast<CHLTVServer*>( this )->GetNetStats( avgIn, avgOut ); break;
	}
}
const char * CBaseServer::GetPassword() const
{
	switch ( m_server_kind )
	{
	case server_kind_t::game: return GetPasswordBase();
	default: return static_cast<const CHLTVServer*>( this )->GetPassword();
	}
}
bool CBaseServer::GetRedirectAddressForConnectClient( const ns_address &adr, const SplitPlayerConnects_t &splitScreenClients, ns_address *pNetAdrRedirect )
{
	switch ( m_server_kind )
	{
	case server_kind_t::game: return GetRedirectAddressForConnectClientBase( adr, splitScreenClients, pNetAdrRedirect );
	default: return static_cast<CHLTVServer*>( this )->GetRedirectAddressForConnectClient( adr, splitScreenClients, pNetAdrRedirect );
	}
}
void CBaseServer::Init( bool isDedicated )
{
	switch ( m_server_kind )
	{
	case server_kind_t::game: static_cast<CGameServer*>( this )->Init( isDedicated ); break;
	default: static_cast<CHLTVServer*>( this )->Init( isDedicated ); break;
	}
}
bool CBaseServer::IsHLTV() const
{
	switch ( m_server_kind )
	{
	case server_kind_t::game: return IsHLTVBase();
	default: return static_cast<const CHLTVServer*>( this )->IsHLTV();
	}
}
bool CBaseServer::IsMultiplayer() const
{
	switch ( m_server_kind )
	{
	case server_kind_t::game: return IsMultiplayerBase();
	default: return static_cast<const CHLTVServer*>( this )->IsMultiplayer();
	}
}
bool CBaseServer::IsPausable() const
{
	switch ( m_server_kind )
	{
	case server_kind_t::game: return static_cast<const CGameServer*>( this )->IsPausable();
	default: return IsPausableBase();
	}
}
void CBaseServer::RemoveClientFromGame( CBaseClient *client )
{
	switch ( m_server_kind )
	{
	case server_kind_t::game: static_cast<CGameServer*>( this )->RemoveClientFromGame( client ); break;
	default: RemoveClientFromGameBase( client ); break;
	}
}
void CBaseServer::SendClientMessages( bool bSendSnapshots )
{
	switch ( m_server_kind )
	{
	case server_kind_t::game: static_cast<CGameServer*>( this )->SendClientMessages( bSendSnapshots ); break;
	default: static_cast<CHLTVServer*>( this )->SendClientMessages( bSendSnapshots ); break;
	}
}
void CBaseServer::SetMaxClients( int number )
{
	switch ( m_server_kind )
	{
	case server_kind_t::game: static_cast<CGameServer*>( this )->SetMaxClients( number ); break;
	default: static_cast<CHLTVServer*>( this )->SetMaxClients( number ); break;
	}
}
void CBaseServer::Shutdown()
{
	switch ( m_server_kind )
	{
	case server_kind_t::game: static_cast<CGameServer*>( this )->Shutdown(); break;
	default: static_cast<CHLTVServer*>( this )->Shutdown(); break;
	}
}
void CBaseServer::UserInfoChanged( int nClientIndex )
{
	switch ( m_server_kind )
	{
	case server_kind_t::game: UserInfoChangedBase( nClientIndex ); break;
	default: static_cast<CHLTVServer*>( this )->UserInfoChanged( nClientIndex ); break;
	}
}

