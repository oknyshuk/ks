// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the INetChannelHandler class: dispatch to CBaseClient / CBaseClientState (handler_kind_t).
// Included by netdispatch.cpp.

void INetChannelHandler::ConnectionStart( INetChannel *chan )
{
	switch ( m_handler_kind )
	{
	case handler_kind_t::client: static_cast<CBaseClient*>(this)->ConnectionStart( chan ); break;
	default: static_cast<CBaseClientState*>(this)->ConnectionStart( chan ); break;
	}
}
void INetChannelHandler::ConnectionStop()
{
	switch ( m_handler_kind )
	{
	case handler_kind_t::client: static_cast<CBaseClient*>(this)->ConnectionStop(); break;
	default: static_cast<CBaseClientState*>(this)->ConnectionStop(); break;
	}
}
void INetChannelHandler::ConnectionClosing( const char *reason )
{
	switch ( m_handler_kind )
	{
	case handler_kind_t::client: static_cast<CBaseClient*>(this)->ConnectionClosing( reason ); break;
	default: static_cast<CBaseClientState*>(this)->ConnectionClosing( reason ); break;
	}
}
void INetChannelHandler::ConnectionCrashed( const char *reason )
{
	switch ( m_handler_kind )
	{
	case handler_kind_t::client: static_cast<CBaseClient*>(this)->ConnectionCrashed( reason ); break;
	default: static_cast<CBaseClientState*>(this)->ConnectionCrashed( reason ); break;
	}
}
void INetChannelHandler::PacketStart( int incoming_sequence, int outgoing_acknowledged )
{
	switch ( m_handler_kind )
	{
	case handler_kind_t::client: static_cast<CBaseClient*>(this)->PacketStart( incoming_sequence, outgoing_acknowledged ); break;
	default: static_cast<CBaseClientState*>(this)->PacketStart( incoming_sequence, outgoing_acknowledged ); break;
	}
}
void INetChannelHandler::PacketEnd( void )
{
	switch ( m_handler_kind )
	{
	case handler_kind_t::client: static_cast<CBaseClient*>(this)->PacketEnd(); break;
	default: static_cast<CBaseClientState*>(this)->PacketEnd(); break;
	}
}
void INetChannelHandler::FileRequested( const char *fileName, unsigned int transferID, bool isReplayDemoFile )
{
	switch ( m_handler_kind )
	{
	case handler_kind_t::client: static_cast<CBaseClient*>(this)->FileRequested( fileName, transferID, isReplayDemoFile ); break;
	default: static_cast<CBaseClientState*>(this)->FileRequested( fileName, transferID, isReplayDemoFile ); break;
	}
}
void INetChannelHandler::FileReceived( const char *fileName, unsigned int transferID, bool isReplayDemoFile )
{
	switch ( m_handler_kind )
	{
	case handler_kind_t::client: static_cast<CBaseClient*>(this)->FileReceived( fileName, transferID, isReplayDemoFile ); break;
	default: static_cast<CBaseClientState*>(this)->FileReceived( fileName, transferID, isReplayDemoFile ); break;
	}
}
void INetChannelHandler::FileDenied( const char *fileName, unsigned int transferID, bool isReplayDemoFile )
{
	switch ( m_handler_kind )
	{
	case handler_kind_t::client: static_cast<CBaseClient*>(this)->FileDenied( fileName, transferID, isReplayDemoFile ); break;
	default: static_cast<CBaseClientState*>(this)->FileDenied( fileName, transferID, isReplayDemoFile ); break;
	}
}
void INetChannelHandler::FileSent( const char *fileName, unsigned int transferID, bool isReplayDemoFile )
{
	switch ( m_handler_kind )
	{
	case handler_kind_t::client: static_cast<CBaseClient*>(this)->FileSent( fileName, transferID, isReplayDemoFile ); break;
	default: static_cast<CBaseClientState*>(this)->FileSent( fileName, transferID, isReplayDemoFile ); break;
	}
}
bool INetChannelHandler::ChangeSplitscreenUser( int nSplitScreenUserSlot )
{
	switch ( m_handler_kind )
	{
	case handler_kind_t::client: return static_cast<CBaseClient*>(this)->ChangeSplitscreenUser( nSplitScreenUserSlot );
	default: return static_cast<CBaseClientState*>(this)->ChangeSplitscreenUser( nSplitScreenUserSlot );
	}
}

