// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IClient class to CBaseClient.
// Included by netdispatch.cpp.

void IClient::Connect( const char * szName, int nUserID, INetChannel *pNetChannel, bool bFakePlayer, CrossPlayPlatform_t clientPlatform, const ks::net::CMsg_CVars *pVecCvars )
{ static_cast<CBaseClient*>(this)->Connect( szName, nUserID, pNetChannel, bFakePlayer, clientPlatform, pVecCvars ); }
void IClient::Inactivate( void )
{ static_cast<CBaseClient*>(this)->Inactivate(); }
void IClient::Reconnect( void )
{ static_cast<CBaseClient*>(this)->Reconnect(); }
void IClient::Disconnect( const char *reason )
{ static_cast<CBaseClient*>(this)->Disconnect( reason ); }
bool IClient::ChangeSplitscreenUser( int nSplitScreenUserSlot )
{ return static_cast<CBaseClient*>(this)->ChangeSplitscreenUser( nSplitScreenUserSlot ); }
int IClient::GetPlayerSlot() const
{ return static_cast<const CBaseClient*>(this)->GetPlayerSlot(); }
int IClient::GetUserID() const
{ return static_cast<const CBaseClient*>(this)->GetUserID(); }
const USERID_t IClient::GetNetworkID() const
{ return static_cast<const CBaseClient*>(this)->GetNetworkID(); }
const char * IClient::GetClientName() const
{ return static_cast<const CBaseClient*>(this)->GetClientName(); }
INetChannel * IClient::GetNetChannel()
{ return static_cast<CBaseClient*>(this)->GetNetChannel(); }
IServer * IClient::GetServer()
{ return static_cast<CBaseClient*>(this)->GetServer(); }
const char * IClient::GetUserSetting( const char *cvar ) const
{ return static_cast<const CBaseClient*>(this)->GetUserSetting( cvar ); }
const char * IClient::GetNetworkIDString() const
{ return static_cast<const CBaseClient*>(this)->GetNetworkIDString(); }
void IClient::SetRate( int nRate, bool bForce )
{ static_cast<CBaseClient*>(this)->SetRate( nRate, bForce ); }
int IClient::GetRate( void ) const
{ return static_cast<const CBaseClient*>(this)->GetRate(); }
void IClient::SetUpdateRate( float fUpdateRate, bool bForce )
{ static_cast<CBaseClient*>(this)->SetUpdateRate( fUpdateRate, bForce ); }
float IClient::GetUpdateRate( void ) const
{ return static_cast<const CBaseClient*>(this)->GetUpdateRate(); }
void IClient::Clear( void )
{ static_cast<CBaseClient*>(this)->Clear(); }
int IClient::GetMaxAckTickCount() const
{ return static_cast<const CBaseClient*>(this)->GetMaxAckTickCount(); }
bool IClient::ExecuteStringCommand( const char *s )
{ return static_cast<CBaseClient*>(this)->ExecuteStringCommand( s ); }
bool IClient::SendNetMsg( INetMessage &msg, bool bForceReliable, bool bVoice )
{ return static_cast<CBaseClient*>(this)->SendNetMsg( msg, bForceReliable, bVoice ); }
void IClient::ClientPrintf( const char *fmt, ... )
{
	std::va_list args;
	__builtin_va_start( args, fmt );
	static_cast<CBaseClient*>(this)->ClientPrintfV( fmt, args );
	__builtin_va_end( args );
}
bool IClient::IsConnected( void ) const
{ return static_cast<const CBaseClient*>(this)->IsConnected(); }
bool IClient::IsSpawned( void ) const
{ return static_cast<const CBaseClient*>(this)->IsSpawned(); }
bool IClient::IsActive( void ) const
{ return static_cast<const CBaseClient*>(this)->IsActive(); }
bool IClient::IsFakeClient( void ) const
{ return static_cast<const CBaseClient*>(this)->IsFakeClient(); }
bool IClient::IsHLTV( void ) const
{ return static_cast<const CBaseClient*>(this)->IsHLTV(); }
bool IClient::IsHearingClient( int index ) const
{ return static_cast<const CBaseClient*>(this)->IsHearingClient( index ); }
bool IClient::IsProximityHearingClient( int index ) const
{ return static_cast<const CBaseClient*>(this)->IsProximityHearingClient( index ); }
void IClient::SetMaxRoutablePayloadSize( int nMaxRoutablePayloadSize )
{ static_cast<CBaseClient*>(this)->SetMaxRoutablePayloadSize( nMaxRoutablePayloadSize ); }
bool IClient::IsSplitScreenUser( void ) const
{ return static_cast<const CBaseClient*>(this)->IsSplitScreenUser(); }
bool IClient::CheckConnect( void )
{ return static_cast<CBaseClient*>(this)->CheckConnect(); }
bool IClient::IsLowViolenceClient( void ) const
{ return static_cast<const CBaseClient*>(this)->IsLowViolenceClient(); }
IClient * IClient::GetSplitScreenOwner()
{ return static_cast<CBaseClient*>(this)->GetSplitScreenOwner(); }
int IClient::GetNumPlayers()
{ return static_cast<CBaseClient*>(this)->GetNumPlayers(); }
bool IClient::IsHumanPlayer() const
{ return static_cast<const CBaseClient*>(this)->IsHumanPlayer(); }
CrossPlayPlatform_t IClient::GetClientPlatform() const
{ return static_cast<const CBaseClient*>(this)->GetClientPlatform(); }
bool IClient::StartHltvReplay( const HltvReplayParams_t &params )
{ return static_cast<CBaseClient*>(this)->StartHltvReplay( params ); }
void IClient::StopHltvReplay()
{ static_cast<CBaseClient*>(this)->StopHltvReplay(); }
int IClient::GetHltvReplayDelay()
{ return static_cast<CBaseClient*>(this)->GetHltvReplayDelay(); }
const char * IClient::GetHltvReplayStatus() const
{ return static_cast<const CBaseClient*>(this)->GetHltvReplayStatus(); }
bool IClient::CanStartHltvReplay()
{ return static_cast<CBaseClient*>(this)->CanStartHltvReplay(); }
void IClient::ResetReplayRequestTime()
{ static_cast<CBaseClient*>(this)->ResetReplayRequestTime(); }

