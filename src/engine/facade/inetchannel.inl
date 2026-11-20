import ks.tier1.bitbuf;
import ks.tier1.ns_address;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the INetChannelInfo / INetChannel classes to CNetChan. Included by net_chan.cpp.

const char * INetChannelInfo::GetName( void ) const
{ return static_cast<const CNetChan*>(this)->GetName(); }
const char * INetChannelInfo::GetAddress( void ) const
{ return static_cast<const CNetChan*>(this)->GetAddress(); }
float INetChannelInfo::GetTime( void ) const
{ return static_cast<const CNetChan*>(this)->GetTime(); }
float INetChannelInfo::GetTimeConnected( void ) const
{ return static_cast<const CNetChan*>(this)->GetTimeConnected(); }
int INetChannelInfo::GetBufferSize( void ) const
{ return static_cast<const CNetChan*>(this)->GetBufferSize(); }
int INetChannelInfo::GetDataRate( void ) const
{ return static_cast<const CNetChan*>(this)->GetDataRate(); }
bool INetChannelInfo::IsLoopback( void ) const
{ return static_cast<const CNetChan*>(this)->IsLoopback(); }
bool INetChannelInfo::IsTimingOut( void ) const
{ return static_cast<const CNetChan*>(this)->IsTimingOut(); }
bool INetChannelInfo::IsPlayback( void ) const
{ return static_cast<const CNetChan*>(this)->IsPlayback(); }
float INetChannelInfo::GetLatency( int flow ) const
{ return static_cast<const CNetChan*>(this)->GetLatency( flow ); }
float INetChannelInfo::GetAvgLatency( int flow ) const
{ return static_cast<const CNetChan*>(this)->GetAvgLatency( flow ); }
float INetChannelInfo::GetAvgLoss( int flow ) const
{ return static_cast<const CNetChan*>(this)->GetAvgLoss( flow ); }
float INetChannelInfo::GetAvgChoke( int flow ) const
{ return static_cast<const CNetChan*>(this)->GetAvgChoke( flow ); }
float INetChannelInfo::GetAvgData( int flow ) const
{ return static_cast<const CNetChan*>(this)->GetAvgData( flow ); }
float INetChannelInfo::GetAvgPackets( int flow ) const
{ return static_cast<const CNetChan*>(this)->GetAvgPackets( flow ); }
int INetChannelInfo::GetTotalData( int flow ) const
{ return static_cast<const CNetChan*>(this)->GetTotalData( flow ); }
int INetChannelInfo::GetTotalPackets( int flow ) const
{ return static_cast<const CNetChan*>(this)->GetTotalPackets( flow ); }
int INetChannelInfo::GetSequenceNr( int flow ) const
{ return static_cast<const CNetChan*>(this)->GetSequenceNr( flow ); }
bool INetChannelInfo::IsValidPacket( int flow, int frame_number ) const
{ return static_cast<const CNetChan*>(this)->IsValidPacket( flow, frame_number ); }
float INetChannelInfo::GetPacketTime( int flow, int frame_number ) const
{ return static_cast<const CNetChan*>(this)->GetPacketTime( flow, frame_number ); }
int INetChannelInfo::GetPacketBytes( int flow, int frame_number, int group ) const
{ return static_cast<const CNetChan*>(this)->GetPacketBytes( flow, frame_number, group ); }
bool INetChannelInfo::GetStreamProgress( int flow, int *received, int *total ) const
{ return static_cast<const CNetChan*>(this)->GetStreamProgress( flow, received, total ); }
float INetChannelInfo::GetTimeSinceLastReceived( void ) const
{ return static_cast<const CNetChan*>(this)->GetTimeSinceLastReceived(); }
float INetChannelInfo::GetCommandInterpolationAmount( int flow, int frame_number ) const
{ return static_cast<const CNetChan*>(this)->GetCommandInterpolationAmount( flow, frame_number ); }
void INetChannelInfo::GetPacketResponseLatency( int flow, int frame_number, int *pnLatencyMsecs, int *pnChoke ) const
{ static_cast<const CNetChan*>(this)->GetPacketResponseLatency( flow, frame_number, pnLatencyMsecs, pnChoke ); }
void INetChannelInfo::GetRemoteFramerate( float *pflFrameTime, float *pflFrameTimeStdDeviation, float *pflFrameStartTimeStdDeviation ) const
{ static_cast<const CNetChan*>(this)->GetRemoteFramerate( pflFrameTime, pflFrameTimeStdDeviation, pflFrameStartTimeStdDeviation ); }
float INetChannelInfo::GetTimeoutSeconds() const
{ return static_cast<const CNetChan*>(this)->GetTimeoutSeconds(); }

void INetChannel::SetDataRate( float rate )
{ static_cast<CNetChan*>(this)->SetDataRate( rate ); }
bool INetChannel::RegisterMessage( INetMessageBinder *msg )
{ return static_cast<CNetChan*>(this)->RegisterMessage( msg ); }
bool INetChannel::UnregisterMessage( INetMessageBinder *msg )
{ return static_cast<CNetChan*>(this)->UnregisterMessage( msg ); }
bool INetChannel::StartStreaming( unsigned int challengeNr )
{ return static_cast<CNetChan*>(this)->StartStreaming( challengeNr ); }
void INetChannel::ResetStreaming( void )
{ static_cast<CNetChan*>(this)->ResetStreaming(); }
void INetChannel::SetTimeout( float seconds, bool bForceExact )
{ static_cast<CNetChan*>(this)->SetTimeout( seconds, bForceExact ); }
void INetChannel::SetDemoRecorder( IDemoRecorder *recorder )
{ static_cast<CNetChan*>(this)->SetDemoRecorder( recorder ); }
void INetChannel::SetChallengeNr( unsigned int chnr )
{ static_cast<CNetChan*>(this)->SetChallengeNr( chnr ); }
void INetChannel::Reset( void )
{ static_cast<CNetChan*>(this)->Reset(); }
void INetChannel::Clear( void )
{ static_cast<CNetChan*>(this)->Clear(); }
void INetChannel::Shutdown( const char *reason )
{ static_cast<CNetChan*>(this)->Shutdown( reason ); }
void INetChannel::ProcessPlayback( void )
{ static_cast<CNetChan*>(this)->ProcessPlayback(); }
bool INetChannel::ProcessStream( void )
{ return static_cast<CNetChan*>(this)->ProcessStream(); }
void INetChannel::ProcessPacket( struct netpacket_s* packet, bool bHasHeader )
{ static_cast<CNetChan*>(this)->ProcessPacket( packet, bHasHeader ); }
bool INetChannel::SendNetMsg( INetMessage &msg, bool bForceReliable, bool bVoice )
{ return static_cast<CNetChan*>(this)->SendNetMsg( msg, bForceReliable, bVoice ); }
bool INetChannel::SendData( bf_write &msg, bool bReliable )
{ return static_cast<CNetChan*>(this)->SendData( msg, bReliable ); }
bool INetChannel::SendFile( const char *filename, unsigned int transferID, bool bIsReplayDemoFile )
{ return static_cast<CNetChan*>(this)->SendFile( filename, transferID, bIsReplayDemoFile ); }
void INetChannel::DenyFile( const char *filename, unsigned int transferID, bool bIsReplayDemoFile )
{ static_cast<CNetChan*>(this)->DenyFile( filename, transferID, bIsReplayDemoFile ); }
void INetChannel::RequestFile_OLD( const char *filename, unsigned int transferID )
{ static_cast<CNetChan*>(this)->RequestFile_OLD( filename, transferID ); }
void INetChannel::SetChoked( void )
{ static_cast<CNetChan*>(this)->SetChoked(); }
int INetChannel::SendDatagram( bf_write *data )
{ return static_cast<CNetChan*>(this)->SendDatagram( data ); }
bool INetChannel::Transmit( bool onlyReliable )
{ return static_cast<CNetChan*>(this)->Transmit( onlyReliable ); }
const ns_address & INetChannel::GetRemoteAddress( void ) const
{ return static_cast<const CNetChan*>(this)->GetRemoteAddress(); }
INetChannelHandler * INetChannel::GetMsgHandler( void ) const
{ return static_cast<const CNetChan*>(this)->GetMsgHandler(); }
int INetChannel::GetDropNumber( void ) const
{ return static_cast<const CNetChan*>(this)->GetDropNumber(); }
int INetChannel::GetSocket( void ) const
{ return static_cast<const CNetChan*>(this)->GetSocket(); }
unsigned int INetChannel::GetChallengeNr( void ) const
{ return static_cast<const CNetChan*>(this)->GetChallengeNr(); }
void INetChannel::GetSequenceData( int &nOutSequenceNr, int &nInSequenceNr, int &nOutSequenceNrAck )
{ static_cast<CNetChan*>(this)->GetSequenceData( nOutSequenceNr, nInSequenceNr, nOutSequenceNrAck ); }
void INetChannel::SetSequenceData( int nOutSequenceNr, int nInSequenceNr, int nOutSequenceNrAck )
{ static_cast<CNetChan*>(this)->SetSequenceData( nOutSequenceNr, nInSequenceNr, nOutSequenceNrAck ); }
void INetChannel::UpdateMessageStats( int msggroup, int bits )
{ static_cast<CNetChan*>(this)->UpdateMessageStats( msggroup, bits ); }
bool INetChannel::CanPacket( void ) const
{ return static_cast<const CNetChan*>(this)->CanPacket(); }
bool INetChannel::IsOverflowed( void ) const
{ return static_cast<const CNetChan*>(this)->IsOverflowed(); }
bool INetChannel::IsTimedOut( void ) const
{ return static_cast<const CNetChan*>(this)->IsTimedOut(); }
bool INetChannel::HasPendingReliableData( void )
{ return static_cast<CNetChan*>(this)->HasPendingReliableData(); }
void INetChannel::SetFileTransmissionMode( bool bBackgroundMode )
{ static_cast<CNetChan*>(this)->SetFileTransmissionMode( bBackgroundMode ); }
void INetChannel::SetCompressionMode( bool bUseCompression )
{ static_cast<CNetChan*>(this)->SetCompressionMode( bUseCompression ); }
unsigned int INetChannel::RequestFile( const char *filename, bool bIsReplayDemoFile )
{ return static_cast<CNetChan*>(this)->RequestFile( filename, bIsReplayDemoFile ); }
float INetChannel::GetTimeSinceLastReceived( void ) const
{ return static_cast<const CNetChan*>(this)->GetTimeSinceLastReceived(); }
void INetChannel::SetMaxBufferSize( bool bReliable, int nBytes, bool bVoice )
{ static_cast<CNetChan*>(this)->SetMaxBufferSize( bReliable, nBytes, bVoice ); }
bool INetChannel::IsNull() const
{ return static_cast<const CNetChan*>(this)->IsNull(); }
int INetChannel::GetNumBitsWritten( bool bReliable )
{ return static_cast<CNetChan*>(this)->GetNumBitsWritten( bReliable ); }
void INetChannel::SetInterpolationAmount( float flInterpolationAmount )
{ static_cast<CNetChan*>(this)->SetInterpolationAmount( flInterpolationAmount ); }
void INetChannel::SetRemoteFramerate( float flFrameTime, float flFrameTimeStdDeviation, float flFrameStartTimeStdDeviation )
{ static_cast<CNetChan*>(this)->SetRemoteFramerate( flFrameTime, flFrameTimeStdDeviation, flFrameStartTimeStdDeviation ); }
void INetChannel::SetMaxRoutablePayloadSize( int nSplitSize )
{ static_cast<CNetChan*>(this)->SetMaxRoutablePayloadSize( nSplitSize ); }
int INetChannel::GetMaxRoutablePayloadSize()
{ return static_cast<CNetChan*>(this)->GetMaxRoutablePayloadSize(); }
bool INetChannel::SetActiveChannel( INetChannel *pNewChannel )
{ return static_cast<CNetChan*>(this)->SetActiveChannel( pNewChannel ); }
void INetChannel::AttachSplitPlayer( int nSplitPlayerSlot, INetChannel *pChannel )
{ static_cast<CNetChan*>(this)->AttachSplitPlayer( nSplitPlayerSlot, pChannel ); }
void INetChannel::DetachSplitPlayer( int nSplitPlayerSlot )
{ static_cast<CNetChan*>(this)->DetachSplitPlayer( nSplitPlayerSlot ); }
bool INetChannel::IsRemoteDisconnected() const
{ return static_cast<const CNetChan*>(this)->IsRemoteDisconnected(); }
bool INetChannel::WasLastMessageReliable() const
{ return static_cast<const CNetChan*>(this)->WasLastMessageReliable(); }
const unsigned char * INetChannel::GetChannelEncryptionKey() const
{ return static_cast<const CNetChan*>(this)->GetChannelEncryptionKey(); }
bool INetChannel::EnqueueVeryLargeAsyncTransfer( INetMessage &msg )
{ return static_cast<CNetChan*>(this)->EnqueueVeryLargeAsyncTransfer( msg ); }

