import ks.math.compressed_vector;
import ks.math.vector;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IEngineSound class: kind-based dispatch to CEngineSoundServer / CEngineSoundClient.
// Included by EngineSound.cpp.

bool IEngineSound::PrecacheSound( const char *pSample, bool bPreload, bool bIsUISound )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineSoundServer*>(this)->PrecacheSound( pSample, bPreload, bIsUISound );
	default: return static_cast<CEngineSoundClient*>(this)->PrecacheSound( pSample, bPreload, bIsUISound );
	}
}
bool IEngineSound::IsSoundPrecached( const char *pSample )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineSoundServer*>(this)->IsSoundPrecached( pSample );
	default: return static_cast<CEngineSoundClient*>(this)->IsSoundPrecached( pSample );
	}
}
void IEngineSound::PrefetchSound( const char *pSample )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineSoundServer*>(this)->PrefetchSound( pSample ); break;
	default: static_cast<CEngineSoundClient*>(this)->PrefetchSound( pSample ); break;
	}
}
bool IEngineSound::IsLoopingSound( const char *pSample )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineSoundServer*>(this)->IsLoopingSound( pSample );
	default: return static_cast<CEngineSoundClient*>(this)->IsLoopingSound( pSample );
	}
}
float IEngineSound::GetSoundDuration( const char *pSample )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineSoundServer*>(this)->GetSoundDuration( pSample );
	default: return static_cast<CEngineSoundClient*>(this)->GetSoundDuration( pSample );
	}
}
int IEngineSound::EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH nSoundEntryHash, const char *pSample, float flVolume, float flAttenuation, int nSeed, int iFlags, int iPitch, const Vector *pOrigin, const Vector *pDirection, std::vector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime, int speakerentity )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineSoundServer*>(this)->EmitSound( filter, iEntIndex, iChannel, pSoundEntry, nSoundEntryHash, pSample, flVolume, flAttenuation, nSeed, iFlags, iPitch, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity );
	default: return static_cast<CEngineSoundClient*>(this)->EmitSound( filter, iEntIndex, iChannel, pSoundEntry, nSoundEntryHash, pSample, flVolume, flAttenuation, nSeed, iFlags, iPitch, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity );
	}
}
int IEngineSound::EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH nSoundEntryHash, const char *pSample, float flVolume, soundlevel_t iSoundlevel, int nSeed, int iFlags, int iPitch, const Vector *pOrigin, const Vector *pDirection, std::vector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime, int speakerentity )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineSoundServer*>(this)->EmitSound( filter, iEntIndex, iChannel, pSoundEntry, nSoundEntryHash, pSample, flVolume, iSoundlevel, nSeed, iFlags, iPitch, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity );
	default: return static_cast<CEngineSoundClient*>(this)->EmitSound( filter, iEntIndex, iChannel, pSoundEntry, nSoundEntryHash, pSample, flVolume, iSoundlevel, nSeed, iFlags, iPitch, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity );
	}
}
void IEngineSound::EmitSentenceByIndex( IRecipientFilter& filter, int iEntIndex, int iChannel, int iSentenceIndex, float flVolume, soundlevel_t iSoundlevel, int nSeed, int iFlags, int iPitch, const Vector *pOrigin, const Vector *pDirection, std::vector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime, int speakerentity )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineSoundServer*>(this)->EmitSentenceByIndex( filter, iEntIndex, iChannel, iSentenceIndex, flVolume, iSoundlevel, nSeed, iFlags, iPitch, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity ); break;
	default: static_cast<CEngineSoundClient*>(this)->EmitSentenceByIndex( filter, iEntIndex, iChannel, iSentenceIndex, flVolume, iSoundlevel, nSeed, iFlags, iPitch, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity ); break;
	}
}
void IEngineSound::StopSound( int iEntIndex, int iChannel, const char *pSample, HSOUNDSCRIPTHASH nSoundEntryHash )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineSoundServer*>(this)->StopSound( iEntIndex, iChannel, pSample, nSoundEntryHash ); break;
	default: static_cast<CEngineSoundClient*>(this)->StopSound( iEntIndex, iChannel, pSample, nSoundEntryHash ); break;
	}
}
void IEngineSound::StopAllSounds( bool bClearBuffers )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineSoundServer*>(this)->StopAllSounds( bClearBuffers ); break;
	default: static_cast<CEngineSoundClient*>(this)->StopAllSounds( bClearBuffers ); break;
	}
}
void IEngineSound::SetRoomType( IRecipientFilter& filter, int roomType )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineSoundServer*>(this)->SetRoomType( filter, roomType ); break;
	default: static_cast<CEngineSoundClient*>(this)->SetRoomType( filter, roomType ); break;
	}
}
void IEngineSound::SetPlayerDSP( IRecipientFilter& filter, int dspType, bool fastReset )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineSoundServer*>(this)->SetPlayerDSP( filter, dspType, fastReset ); break;
	default: static_cast<CEngineSoundClient*>(this)->SetPlayerDSP( filter, dspType, fastReset ); break;
	}
}
int IEngineSound::EmitAmbientSound( const char *pSample, float flVolume, int iPitch, int flags, float soundtime )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineSoundServer*>(this)->EmitAmbientSound( pSample, flVolume, iPitch, flags, soundtime );
	default: return static_cast<CEngineSoundClient*>(this)->EmitAmbientSound( pSample, flVolume, iPitch, flags, soundtime );
	}
}
float IEngineSound::GetDistGainFromSoundLevel( soundlevel_t soundlevel, float dist )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineSoundServer*>(this)->GetDistGainFromSoundLevel( soundlevel, dist );
	default: return static_cast<CEngineSoundClient*>(this)->GetDistGainFromSoundLevel( soundlevel, dist );
	}
}
int IEngineSound::GetGuidForLastSoundEmitted()
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineSoundServer*>(this)->GetGuidForLastSoundEmitted();
	default: return static_cast<CEngineSoundClient*>(this)->GetGuidForLastSoundEmitted();
	}
}
bool IEngineSound::IsSoundStillPlaying( int guid )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineSoundServer*>(this)->IsSoundStillPlaying( guid );
	default: return static_cast<CEngineSoundClient*>(this)->IsSoundStillPlaying( guid );
	}
}
void IEngineSound::StopSoundByGuid( int guid, bool bForceSync )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineSoundServer*>(this)->StopSoundByGuid( guid, bForceSync ); break;
	default: static_cast<CEngineSoundClient*>(this)->StopSoundByGuid( guid, bForceSync ); break;
	}
}
void IEngineSound::SetVolumeByGuid( int guid, float fvol )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineSoundServer*>(this)->SetVolumeByGuid( guid, fvol ); break;
	default: static_cast<CEngineSoundClient*>(this)->SetVolumeByGuid( guid, fvol ); break;
	}
}
void IEngineSound::GetActiveSounds( std::vector< SndInfo_t >& sndlist )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineSoundServer*>(this)->GetActiveSounds( sndlist ); break;
	default: static_cast<CEngineSoundClient*>(this)->GetActiveSounds( sndlist ); break;
	}
}
void IEngineSound::PrecacheSentenceGroup( const char *pGroupName )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineSoundServer*>(this)->PrecacheSentenceGroup( pGroupName ); break;
	default: static_cast<CEngineSoundClient*>(this)->PrecacheSentenceGroup( pGroupName ); break;
	}
}
void IEngineSound::NotifyBeginMoviePlayback()
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineSoundServer*>(this)->NotifyBeginMoviePlayback(); break;
	default: static_cast<CEngineSoundClient*>(this)->NotifyBeginMoviePlayback(); break;
	}
}
void IEngineSound::NotifyEndMoviePlayback()
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineSoundServer*>(this)->NotifyEndMoviePlayback(); break;
	default: static_cast<CEngineSoundClient*>(this)->NotifyEndMoviePlayback(); break;
	}
}
bool IEngineSound::IsMoviePlaying()
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineSoundServer*>(this)->IsMoviePlaying();
	default: return static_cast<CEngineSoundClient*>(this)->IsMoviePlaying();
	}
}
bool IEngineSound::GetSoundChannelVolume( const char* sound, float &flVolumeLeft, float &flVolumeRight )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineSoundServer*>(this)->GetSoundChannelVolume( sound, flVolumeLeft, flVolumeRight );
	default: return static_cast<CEngineSoundClient*>(this)->GetSoundChannelVolume( sound, flVolumeLeft, flVolumeRight );
	}
}
float IEngineSound::GetElapsedTimeByGuid( int guid )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineSoundServer*>(this)->GetElapsedTimeByGuid( guid );
	default: return static_cast<CEngineSoundClient*>(this)->GetElapsedTimeByGuid( guid );
	}
}
bool IEngineSound::GetPreventSound( void )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineSoundServer*>(this)->GetPreventSound();
	default: return static_cast<CEngineSoundClient*>(this)->GetPreventSound();
	}
}
void IEngineSound::SetReplaySoundFade( float flReplayVolume )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineSoundServer*>(this)->SetReplaySoundFade( flReplayVolume ); break;
	default: static_cast<CEngineSoundClient*>(this)->SetReplaySoundFade( flReplayVolume ); break;
	}
}
float IEngineSound::GetReplaySoundFade() const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CEngineSoundServer*>(this)->GetReplaySoundFade();
	default: return static_cast<const CEngineSoundClient*>(this)->GetReplaySoundFade();
	}
}

