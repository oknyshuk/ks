// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// CBaseDemoAction: the demo action types are a closed set; the base class calls the type specific class
// by its DEMOACTION type. Included by cl_demoaction_types.cpp.

void CBaseDemoAction::FireAction()
{
	switch ( GetType() )
	{
	case DEMO_ACTION_SKIPAHEAD: static_cast<CDemoActionSkipAhead*>( this )->FireAction(); break;
	case DEMO_ACTION_STOPPLAYBACK: static_cast<CDemoActionStopPlayback*>( this )->FireAction(); break;
	case DEMO_ACTION_PLAYCOMMANDS: static_cast<CDemoActionPlayCommands*>( this )->FireAction(); break;
	case DEMO_ACTION_SCREENFADE_START: static_cast<CDemoActionScreenFadeStart*>( this )->FireAction(); break;
	case DEMO_ACTION_TEXTMESSAGE_START: static_cast<CDemoActionTextMessageStart*>( this )->FireAction(); break;
	case DEMO_ACTION_PLAYCDTRACK_START: static_cast<CDemoActionCDTrackStart*>( this )->FireAction(); break;
	case DEMO_ACTION_PLAYCDTRACK_STOP: static_cast<CDemoActionCDTrackStop*>( this )->FireAction(); break;
	case DEMO_ACTION_PLAYSOUND_START: static_cast<CDemoActionPlaySoundStart*>( this )->FireAction(); break;
	case DEMO_ACTION_CHANGEPLAYBACKRATE: static_cast<CDemoActionChangePlaybackRate*>( this )->FireAction(); break;
	case DEMO_ACTION_PAUSE: static_cast<CDemoActionPausePlayback*>( this )->FireAction(); break;
	default: static_cast<CDemoActionZoom*>( this )->FireAction(); break;
	}
}
bool CBaseDemoAction::Init( KeyValues *pInitData )
{
	switch ( GetType() )
	{
	case DEMO_ACTION_SKIPAHEAD: return static_cast<CDemoActionSkipAhead*>( this )->Init( pInitData );
	case DEMO_ACTION_STOPPLAYBACK: return InitBase( pInitData );
	case DEMO_ACTION_PLAYCOMMANDS: return static_cast<CDemoActionPlayCommands*>( this )->Init( pInitData );
	case DEMO_ACTION_SCREENFADE_START: return static_cast<CDemoActionScreenFadeStart*>( this )->Init( pInitData );
	case DEMO_ACTION_TEXTMESSAGE_START: return static_cast<CDemoActionTextMessageStart*>( this )->Init( pInitData );
	case DEMO_ACTION_PLAYCDTRACK_START: return static_cast<CDemoActionCDTrackStart*>( this )->Init( pInitData );
	case DEMO_ACTION_PLAYCDTRACK_STOP: return InitBase( pInitData );
	case DEMO_ACTION_PLAYSOUND_START: return static_cast<CDemoActionPlaySoundStart*>( this )->Init( pInitData );
	case DEMO_ACTION_CHANGEPLAYBACKRATE: return static_cast<CDemoActionChangePlaybackRate*>( this )->Init( pInitData );
	case DEMO_ACTION_PAUSE: return static_cast<CDemoActionPausePlayback*>( this )->Init( pInitData );
	default: return static_cast<CDemoActionZoom*>( this )->Init( pInitData );
	}
}
void CBaseDemoAction::OnActionFinished()
{
	switch ( GetType() )
	{
	case DEMO_ACTION_SKIPAHEAD: OnActionFinishedBase(); break;
	case DEMO_ACTION_STOPPLAYBACK: OnActionFinishedBase(); break;
	case DEMO_ACTION_PLAYCOMMANDS: OnActionFinishedBase(); break;
	case DEMO_ACTION_SCREENFADE_START: OnActionFinishedBase(); break;
	case DEMO_ACTION_TEXTMESSAGE_START: OnActionFinishedBase(); break;
	case DEMO_ACTION_PLAYCDTRACK_START: OnActionFinishedBase(); break;
	case DEMO_ACTION_PLAYCDTRACK_STOP: OnActionFinishedBase(); break;
	case DEMO_ACTION_PLAYSOUND_START: OnActionFinishedBase(); break;
	case DEMO_ACTION_CHANGEPLAYBACKRATE: static_cast<CDemoActionChangePlaybackRate*>( this )->OnActionFinished(); break;
	case DEMO_ACTION_PAUSE: OnActionFinishedBase(); break;
	default: OnActionFinishedBase(); break;
	}
}
void CBaseDemoAction::SaveKeysToBuffer( int depth, ks::text_writer& buf )
{
	switch ( GetType() )
	{
	case DEMO_ACTION_SKIPAHEAD: static_cast<CDemoActionSkipAhead*>( this )->SaveKeysToBuffer( depth, buf ); break;
	case DEMO_ACTION_STOPPLAYBACK: SaveKeysToBufferBase( depth, buf ); break;
	case DEMO_ACTION_PLAYCOMMANDS: static_cast<CDemoActionPlayCommands*>( this )->SaveKeysToBuffer( depth, buf ); break;
	case DEMO_ACTION_SCREENFADE_START: static_cast<CDemoActionScreenFadeStart*>( this )->SaveKeysToBuffer( depth, buf ); break;
	case DEMO_ACTION_TEXTMESSAGE_START: static_cast<CDemoActionTextMessageStart*>( this )->SaveKeysToBuffer( depth, buf ); break;
	case DEMO_ACTION_PLAYCDTRACK_START: static_cast<CDemoActionCDTrackStart*>( this )->SaveKeysToBuffer( depth, buf ); break;
	case DEMO_ACTION_PLAYCDTRACK_STOP: SaveKeysToBufferBase( depth, buf ); break;
	case DEMO_ACTION_PLAYSOUND_START: static_cast<CDemoActionPlaySoundStart*>( this )->SaveKeysToBuffer( depth, buf ); break;
	case DEMO_ACTION_CHANGEPLAYBACKRATE: static_cast<CDemoActionChangePlaybackRate*>( this )->SaveKeysToBuffer( depth, buf ); break;
	case DEMO_ACTION_PAUSE: static_cast<CDemoActionPausePlayback*>( this )->SaveKeysToBuffer( depth, buf ); break;
	default: static_cast<CDemoActionZoom*>( this )->SaveKeysToBuffer( depth, buf ); break;
	}
}
bool CBaseDemoAction::Update( const DemoActionTimingContext& tc )
{
	switch ( GetType() )
	{
	case DEMO_ACTION_SKIPAHEAD: return static_cast<CDemoActionSkipAhead*>( this )->Update( tc );
	case DEMO_ACTION_STOPPLAYBACK: return static_cast<CDemoActionStopPlayback*>( this )->Update( tc );
	case DEMO_ACTION_PLAYCOMMANDS: return UpdateBase( tc );
	case DEMO_ACTION_SCREENFADE_START: return UpdateBase( tc );
	case DEMO_ACTION_TEXTMESSAGE_START: return UpdateBase( tc );
	case DEMO_ACTION_PLAYCDTRACK_START: return UpdateBase( tc );
	case DEMO_ACTION_PLAYCDTRACK_STOP: return UpdateBase( tc );
	case DEMO_ACTION_PLAYSOUND_START: return UpdateBase( tc );
	case DEMO_ACTION_CHANGEPLAYBACKRATE: return static_cast<CDemoActionChangePlaybackRate*>( this )->Update( tc );
	case DEMO_ACTION_PAUSE: return static_cast<CDemoActionPausePlayback*>( this )->Update( tc );
	default: return static_cast<CDemoActionZoom*>( this )->Update( tc );
	}
}

void CBaseDemoAction::DeleteThis()
{
	switch ( GetType() )
	{
	case DEMO_ACTION_SKIPAHEAD: delete static_cast<CDemoActionSkipAhead*>( this ); break;
	case DEMO_ACTION_STOPPLAYBACK: delete static_cast<CDemoActionStopPlayback*>( this ); break;
	case DEMO_ACTION_PLAYCOMMANDS: delete static_cast<CDemoActionPlayCommands*>( this ); break;
	case DEMO_ACTION_SCREENFADE_START: delete static_cast<CDemoActionScreenFadeStart*>( this ); break;
	case DEMO_ACTION_TEXTMESSAGE_START: delete static_cast<CDemoActionTextMessageStart*>( this ); break;
	case DEMO_ACTION_PLAYCDTRACK_START: delete static_cast<CDemoActionCDTrackStart*>( this ); break;
	case DEMO_ACTION_PLAYCDTRACK_STOP: delete static_cast<CDemoActionCDTrackStop*>( this ); break;
	case DEMO_ACTION_PLAYSOUND_START: delete static_cast<CDemoActionPlaySoundStart*>( this ); break;
	case DEMO_ACTION_CHANGEPLAYBACKRATE: delete static_cast<CDemoActionChangePlaybackRate*>( this ); break;
	case DEMO_ACTION_PAUSE: delete static_cast<CDemoActionPausePlayback*>( this ); break;
	default: delete static_cast<CDemoActionZoom*>( this ); break;
	}
}
