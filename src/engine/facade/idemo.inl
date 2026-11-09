// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IDemoRecorder / IDemoPlayer classes: dispatch to the demo (cl_demo.h) or the HLTV
// (hltvdemo.h, hltvserver.h) implementation. Included by netdispatch.cpp.

void IDemoRecorder::SetSignonState( SIGNONSTATE state )
{
	switch ( m_recorder_kind )
	{
	case recorder_kind_t::demo: static_cast<CDemoRecorder*>(this)->SetSignonState( state ); break;
	default: static_cast<CHLTVDemoRecorder*>(this)->SetSignonState( state ); break;
	}
}
void IDemoRecorder::RecordServerClasses( ServerClass *pClasses )
{
	switch ( m_recorder_kind )
	{
	case recorder_kind_t::demo: static_cast<CDemoRecorder*>(this)->RecordServerClasses( pClasses ); break;
	default: static_cast<CHLTVDemoRecorder*>(this)->RecordServerClasses( pClasses ); break;
	}
}
void IDemoRecorder::RecordMessages( bf_read &data, int bits )
{
	switch ( m_recorder_kind )
	{
	case recorder_kind_t::demo: static_cast<CDemoRecorder*>(this)->RecordMessages( data, bits ); break;
	default: static_cast<CHLTVDemoRecorder*>(this)->RecordMessages( data, bits ); break;
	}
}
void IDemoRecorder::RecordPacket( void )
{
	switch ( m_recorder_kind )
	{
	case recorder_kind_t::demo: static_cast<CDemoRecorder*>(this)->RecordPacket(); break;
	default: static_cast<CHLTVDemoRecorder*>(this)->RecordPacket(); break;
	}
}
void IDemoRecorder::RecordCommand( const char *cmdstring )
{
	switch ( m_recorder_kind )
	{
	case recorder_kind_t::demo: static_cast<CDemoRecorder*>(this)->RecordCommand( cmdstring ); break;
	default: static_cast<CHLTVDemoRecorder*>(this)->RecordCommand( cmdstring ); break;
	}
}
void IDemoRecorder::RecordUserInput( int cmdnumber )
{
	switch ( m_recorder_kind )
	{
	case recorder_kind_t::demo: static_cast<CDemoRecorder*>(this)->RecordUserInput( cmdnumber ); break;
	default: static_cast<CHLTVDemoRecorder*>(this)->RecordUserInput( cmdnumber ); break;
	}
}
void IDemoRecorder::RecordCustomData( int iCallbackIndex, const void *pData, std::size_t iDataLength )
{
	switch ( m_recorder_kind )
	{
	case recorder_kind_t::demo: static_cast<CDemoRecorder*>(this)->RecordCustomData( iCallbackIndex, pData, iDataLength ); break;
	default: static_cast<CHLTVDemoRecorder*>(this)->RecordCustomData( iCallbackIndex, pData, iDataLength ); break;
	}
}
void IDemoRecorder::ResetDemoInterpolation()
{
	switch ( m_recorder_kind )
	{
	case recorder_kind_t::demo: static_cast<CDemoRecorder*>(this)->ResetDemoInterpolation(); break;
	default: static_cast<CHLTVDemoRecorder*>(this)->ResetDemoInterpolation(); break;
	}
}
void IDemoRecorder::StartRecording( const char *filename, bool bContinuously )
{
	switch ( m_recorder_kind )
	{
	case recorder_kind_t::demo: static_cast<CDemoRecorder*>(this)->StartRecording( filename, bContinuously ); break;
	default: static_cast<CHLTVDemoRecorder*>(this)->StartRecording( filename, bContinuously ); break;
	}
}
void IDemoRecorder::StopRecording( const CGameInfo* pGameInfo )
{
	switch ( m_recorder_kind )
	{
	case recorder_kind_t::demo: static_cast<CDemoRecorder*>(this)->StopRecording( pGameInfo ); break;
	default: static_cast<CHLTVDemoRecorder*>(this)->StopRecording( pGameInfo ); break;
	}
}
bool IDemoRecorder::IsRecording( void )
{
	switch ( m_recorder_kind )
	{
	case recorder_kind_t::demo: return static_cast<CDemoRecorder*>(this)->IsRecording();
	default: return static_cast<CHLTVDemoRecorder*>(this)->IsRecording();
	}
}
int IDemoRecorder::GetRecordingTick( void )
{
	switch ( m_recorder_kind )
	{
	case recorder_kind_t::demo: return static_cast<CDemoRecorder*>(this)->GetRecordingTick();
	default: return static_cast<CHLTVDemoRecorder*>(this)->GetRecordingTick();
	}
}

CDemoFile * IDemoPlayer::GetDemoStream()
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: return static_cast<CDemoPlayer*>(this)->GetDemoStream();
	default: return static_cast<CHLTVServer*>(this)->GetDemoStream();
	}
}
int IDemoPlayer::GetPlaybackStartTick( void )
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: return static_cast<CDemoPlayer*>(this)->GetPlaybackStartTick();
	default: return static_cast<CHLTVServer*>(this)->GetPlaybackStartTick();
	}
}
int IDemoPlayer::GetPlaybackTick( void )
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: return static_cast<CDemoPlayer*>(this)->GetPlaybackTick();
	default: return static_cast<CHLTVServer*>(this)->GetPlaybackTick();
	}
}
bool IDemoPlayer::IsPlayingBack( void ) const
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: return static_cast<const CDemoPlayer*>(this)->IsPlayingBack();
	default: return static_cast<const CHLTVServer*>(this)->IsPlayingBack();
	}
}
bool IDemoPlayer::IsPlaybackPaused( void ) const
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: return static_cast<const CDemoPlayer*>(this)->IsPlaybackPaused();
	default: return static_cast<const CHLTVServer*>(this)->IsPlaybackPaused();
	}
}
bool IDemoPlayer::IsPlayingTimeDemo( void ) const
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: return static_cast<const CDemoPlayer*>(this)->IsPlayingTimeDemo();
	default: return static_cast<const CHLTVServer*>(this)->IsPlayingTimeDemo();
	}
}
bool IDemoPlayer::IsSkipping( void ) const
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: return static_cast<const CDemoPlayer*>(this)->IsSkipping();
	default: return static_cast<const CHLTVServer*>(this)->IsSkipping();
	}
}
bool IDemoPlayer::CanSkipBackwards( void ) const
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: return static_cast<const CDemoPlayer*>(this)->CanSkipBackwards();
	default: return static_cast<const CHLTVServer*>(this)->CanSkipBackwards();
	}
}
void IDemoPlayer::SetPlaybackTimeScale( float timescale )
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: static_cast<CDemoPlayer*>(this)->SetPlaybackTimeScale( timescale ); break;
	default: static_cast<CHLTVServer*>(this)->SetPlaybackTimeScale( timescale ); break;
	}
}
float IDemoPlayer::GetPlaybackTimeScale( void )
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: return static_cast<CDemoPlayer*>(this)->GetPlaybackTimeScale();
	default: return static_cast<CHLTVServer*>(this)->GetPlaybackTimeScale();
	}
}
void IDemoPlayer::PausePlayback( float seconds )
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: static_cast<CDemoPlayer*>(this)->PausePlayback( seconds ); break;
	default: static_cast<CHLTVServer*>(this)->PausePlayback( seconds ); break;
	}
}
void IDemoPlayer::SkipToTick( int tick, bool bRelative, bool bPause )
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: static_cast<CDemoPlayer*>(this)->SkipToTick( tick, bRelative, bPause ); break;
	default: static_cast<CHLTVServer*>(this)->SkipToTick( tick, bRelative, bPause ); break;
	}
}
void IDemoPlayer::SkipToImportantTick( const DemoImportantTick_t *pTick )
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: static_cast<CDemoPlayer*>(this)->SkipToImportantTick( pTick ); break;
	default: static_cast<CHLTVServer*>(this)->SkipToImportantTick( pTick ); break;
	}
}
void IDemoPlayer::ResumePlayback( void )
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: static_cast<CDemoPlayer*>(this)->ResumePlayback(); break;
	default: static_cast<CHLTVServer*>(this)->ResumePlayback(); break;
	}
}
void IDemoPlayer::StopPlayback( void )
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: static_cast<CDemoPlayer*>(this)->StopPlayback(); break;
	default: static_cast<CHLTVServer*>(this)->StopPlayback(); break;
	}
}
void IDemoPlayer::InterpolateViewpoint()
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: static_cast<CDemoPlayer*>(this)->InterpolateViewpoint(); break;
	default: static_cast<CHLTVServer*>(this)->InterpolateViewpoint(); break;
	}
}
netpacket_t * IDemoPlayer::ReadPacket( void )
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: return static_cast<CDemoPlayer*>(this)->ReadPacket();
	default: return static_cast<CHLTVServer*>(this)->ReadPacket();
	}
}
void IDemoPlayer::ResetDemoInterpolation()
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: static_cast<CDemoPlayer*>(this)->ResetDemoInterpolation(); break;
	default: static_cast<CHLTVServer*>(this)->ResetDemoInterpolation(); break;
	}
}
CDemoPlaybackParameters_t const * IDemoPlayer::GetDemoPlaybackParameters()
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: return static_cast<CDemoPlayer*>(this)->GetDemoPlaybackParameters();
	default: return static_cast<CHLTVServer*>(this)->GetDemoPlaybackParameters();
	}
}
void IDemoPlayer::SetPacketReadSuspended( bool bSuspendPacketReading )
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: static_cast<CDemoPlayer*>(this)->SetPacketReadSuspended( bSuspendPacketReading ); break;
	default: static_cast<CHLTVServer*>(this)->SetPacketReadSuspended( bSuspendPacketReading ); break;
	}
}
void IDemoPlayer::SetImportantEventData( const KeyValues *pData )
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: static_cast<CDemoPlayer*>(this)->SetImportantEventData( pData ); break;
	default: static_cast<CHLTVServer*>(this)->SetImportantEventData( pData ); break;
	}
}
int IDemoPlayer::FindNextImportantTick( int nCurrentTick, const char *pEventName )
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: return static_cast<CDemoPlayer*>(this)->FindNextImportantTick( nCurrentTick, pEventName );
	default: return static_cast<CHLTVServer*>(this)->FindNextImportantTick( nCurrentTick, pEventName );
	}
}
int IDemoPlayer::FindPreviousImportantTick( int nCurrentTick, const char *pEventName )
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: return static_cast<CDemoPlayer*>(this)->FindPreviousImportantTick( nCurrentTick, pEventName );
	default: return static_cast<CHLTVServer*>(this)->FindPreviousImportantTick( nCurrentTick, pEventName );
	}
}
const DemoImportantTick_t * IDemoPlayer::GetImportantTick( int nIndex )
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: return static_cast<CDemoPlayer*>(this)->GetImportantTick( nIndex );
	default: return static_cast<CHLTVServer*>(this)->GetImportantTick( nIndex );
	}
}
const DemoImportantGameEvent_t * IDemoPlayer::GetImportantGameEvent( const char *pszEventName )
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: return static_cast<CDemoPlayer*>(this)->GetImportantGameEvent( pszEventName );
	default: return static_cast<CHLTVServer*>(this)->GetImportantGameEvent( pszEventName );
	}
}
void IDemoPlayer::ListImportantTicks( void )
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: static_cast<CDemoPlayer*>(this)->ListImportantTicks(); break;
	default: static_cast<CHLTVServer*>(this)->ListImportantTicks(); break;
	}
}
void IDemoPlayer::ListHighlightData( void )
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: static_cast<CDemoPlayer*>(this)->ListHighlightData(); break;
	default: static_cast<CHLTVServer*>(this)->ListHighlightData(); break;
	}
}
void IDemoPlayer::SetHighlightXuid( uint64 xuid, bool bLowlights )
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: static_cast<CDemoPlayer*>(this)->SetHighlightXuid( xuid, bLowlights ); break;
	default: static_cast<CHLTVServer*>(this)->SetHighlightXuid( xuid, bLowlights ); break;
	}
}
bool IDemoPlayer::ScanDemo( const char *filename, const char* pszMode )
{
	switch ( m_player_kind )
	{
	case player_kind_t::demo: return static_cast<CDemoPlayer*>(this)->ScanDemo( filename, pszMode );
	default: return static_cast<CHLTVServer*>(this)->ScanDemo( filename, pszMode );
	}
}

