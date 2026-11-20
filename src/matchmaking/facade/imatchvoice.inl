import ks.core.platform;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMatchVoice class to CMatchVoice. Included by the source that defines CMatchVoice.

bool IMatchVoice::CanPlaybackTalker( XUID xuidTalker )
{ return static_cast<CMatchVoice*>(this)->CanPlaybackTalker( xuidTalker ); }
bool IMatchVoice::IsTalkerMuted( XUID xuidTalker )
{ return static_cast<CMatchVoice*>(this)->IsTalkerMuted( xuidTalker ); }
bool IMatchVoice::IsMachineMuted( XUID xuidPlayer )
{ return static_cast<CMatchVoice*>(this)->IsMachineMuted( xuidPlayer ); }
bool IMatchVoice::IsVoiceRecording()
{ return static_cast<CMatchVoice*>(this)->IsVoiceRecording(); }
void IMatchVoice::SetVoiceRecording( bool bRecordingEnabled )
{ static_cast<CMatchVoice*>(this)->SetVoiceRecording( bRecordingEnabled ); }
void IMatchVoice::MuteTalker( XUID xuidTalker, bool bMute )
{ static_cast<CMatchVoice*>(this)->MuteTalker( xuidTalker, bMute ); }
