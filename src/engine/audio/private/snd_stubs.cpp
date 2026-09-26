
#include "audio_pch.h"

#include "matchmaking/imatchframework.h"
#include "tier2/tier2.h"
#include "fmtstr.h"
#include "tier1/byte_writer.h"
#include <cstring>
#include <vector>

#include "audio/public/voice.h"

#if !defined( DEDICATED ) && ( defined( OSX ) || defined( _WIN32 ) || defined( LINUX ) ) && !defined( NO_STEAM )
#include "cl_steamauth.h"
#include "client.h"
#if defined( PS3SDK_INSTALLED ) 
#define PS3_CROSS_PLAY
#endif
extern IVEngineClient *engineClient;
#endif

CEngineVoiceStub *Audio_GetEngineVoiceStub()
{
	static CEngineVoiceStub s_EngineVoiceStub;
	return &s_EngineVoiceStub;
}


#if !defined( DEDICATED ) && ( defined( OSX ) || defined( _WIN32 ) || defined( LINUX ) ) && !defined( NO_STEAM )

ConVar snd_voice_echo( "snd_voice_echo", "0", FCVAR_DEVELOPMENTONLY );

// #define SND_VOICE_LOG_DEBUG
#ifdef SND_VOICE_LOG_DEBUG

ConVar snd_voice_log( "snd_voice_log", "0", FCVAR_DEVELOPMENTONLY ); // 1 = record, 2 = playback
enum SndVoiceLog_t
{
	SND_VOICE_LOG_RECORD_REMOTE = 1,
	SND_VOICE_LOG_TEST_PLAYBACK_LOG = 2,
	SND_VOICE_LOG_RECORD_REMOTE_11025 = 4,
	SND_VOICE_LOG_RECORD_LOCAL = 8,
	SND_VOICE_LOG_RECORD_LOCAL_11025 = 16,
};
ks::byte_writer g_bufSndVoiceLog;
ks::byte_writer g_bufSndVoiceLog11025;

CON_COMMAND( snd_voice_log_commit, "Commit voice log to file" )
{
	if ( ( args.ArgC() <= 1 ) || ( g_bufSndVoiceLog.size() <= 0 ) )
	{
		Msg( "Voice log size: %u/%u\n", (unsigned)g_bufSndVoiceLog.size(), (unsigned)g_bufSndVoiceLog11025.size() );
		return;
	}
	if ( !strcmp( "0", args.Arg( 1 ) ) )
	{
		Msg( "Voice log discarded\n" );
		g_bufSndVoiceLog.clear();
		g_bufSndVoiceLog11025.clear();
		return;
	}
	
	if ( g_pFullFileSystem->WriteFile( args.Arg( 1 ), nullptr, g_bufSndVoiceLog.bytes() ) )
	{
		Msg( "Voice log committed to file '%s', %u bytes\n", args.Arg(1), (unsigned)g_bufSndVoiceLog.size() );
		g_bufSndVoiceLog.clear();
	}
	else
	{
		Warning( "Failed to commit voice log to file '%s', keeping %u bytes\n", args.Arg(1), (unsigned)g_bufSndVoiceLog.size() );
	}
	
	if ( g_pFullFileSystem->WriteFile( CFmtStr( "%s.11025", args.Arg( 1 ) ), nullptr, g_bufSndVoiceLog11025.bytes() ) )
	{
		Msg( "Voice log committed to file '%s.11025', %u bytes\n", args.Arg(1), (unsigned)g_bufSndVoiceLog11025.size() );
		g_bufSndVoiceLog11025.clear();
	}
	else
	{
		Warning( "Failed to commit voice log to file '%s.11025', keeping %u bytes\n", args.Arg(1), (unsigned)g_bufSndVoiceLog11025.size() );
	}
}

CON_COMMAND( snd_voice_log_load, "Load voice log file" )
{
	g_bufSndVoiceLog.clear();
	auto data = g_pFullFileSystem->ReadFile( args.Arg( 1 ), nullptr );
	if ( !data )
	{
		Warning( "Failed to read voice log from file '%s'\n", args.Arg( 1 ) );
	}
	else
	{
		g_bufSndVoiceLog.write_bytes( *data );
		Msg( "Loaded voice log from file '%s'\n", args.Arg( 1 ) );
	}
}

CON_COMMAND( snd_voice_log_setsin, "Set voice log sin-wave" )
{
	g_bufSndVoiceLog.clear();

	const int kNumSamples = 500000;

	const auto samples = g_bufSndVoiceLog.claim_span( kNumSamples * sizeof( int16 ) );

	float flSinFreq = 2.0/128.0;
	if ( args.ArgC() > 1 )
	{
		flSinFreq = atof( args.Arg( 1 ) );
		if ( flSinFreq <= 0.00001 )
			flSinFreq = 0.00001;
	}
	
	// PURELY FOR EXAMPLE: a basic sine wave test
	if ( 1 ) {
		for ( int q = 0 ; q < kNumSamples; ++ q )
		{
			float f = sin( flSinFreq * M_PI * q ) * 20000;
			const int16 sample = f;
			std::memcpy( samples.data() + q * sizeof( int16 ), &sample, sizeof( sample ) );
		}
	}
}

#endif // SND_VOICE_LOG_DEBUG

#define SOUND_PC_CELP_FREQ 8000

template < int nSOURCE, int nTARGET >
struct ResampleGeneric_t
{
public:
	ResampleGeneric_t() : m_sampLeftover( 0 ), m_flFractionalSamples( 0.0f ) { }

	uint32 Resample( const int16 *piSamples, uint32 numInSamples, int16 *poSamples )
	{
		if ( !poSamples )
			return ( 1 + ( numInSamples / nSOURCE ) ) * nTARGET;
		if ( !numInSamples )
			return 0;

		int numOutSamples = 0;
		const int16 *piSamplesEnd = piSamples + numInSamples;
		for ( int16 nextSamp = *( piSamples ++ ); ; m_flFractionalSamples += ((float) nSOURCE)/((float) nTARGET) )
		{
			// if we've passed a sample boundary, go onto the next sample
			while ( m_flFractionalSamples >= 1.0f )
			{
				m_flFractionalSamples -= 1.0f;
				m_sampLeftover = nextSamp;
				if ( piSamples < piSamplesEnd )
				{
					nextSamp = *( piSamples ++ );
				}
				else
				{
					return numOutSamples;
				}
			}

			*( poSamples ++ ) = Lerp( m_flFractionalSamples, m_sampLeftover,  nextSamp ); // left
			++ numOutSamples;
		}
	}

private:
	float m_flFractionalSamples;
	int16 m_sampLeftover;
};

typedef ResampleGeneric_t< SOUND_PC_CELP_FREQ, VOICE_OUTPUT_SAMPLE_RATE > Resample_CELP_to_PC_t; // 320 CELP samples = 441 PC samples
typedef ResampleGeneric_t< VOICE_OUTPUT_SAMPLE_RATE, SOUND_PC_CELP_FREQ > Resample_PC_to_CELP_t; // 441 PC samples = 320 PC samples

#ifdef SND_VOICE_LOG_DEBUG

CON_COMMAND( snd_voice_log_resample, "Resample voice log file" )
{
	g_bufSndVoiceLog.clear();
	auto bufRaw = g_pFullFileSystem->ReadFile( args.Arg( 1 ), nullptr );
	if ( !bufRaw )
	{
		Warning( "Failed to read voice log from file '%s'\n", args.Arg( 1 ) );
		return;
	}
	else
	{
		Msg( "Loaded voice log from file '%s'\n", args.Arg( 1 ) );
	}

	Resample_CELP_to_PC_t rcpt;
	std::vector< int16 > in( bufRaw->size() / 2 );
	std::memcpy( in.data(), bufRaw->data(), in.size() * sizeof( int16 ) );
	std::vector< int16 > out( bufRaw->size() + 2 );
	uint32 numResamples = rcpt.Resample( in.data(), in.size(), out.data() );
	g_bufSndVoiceLog.write_bytes( out.data(), numResamples * 2 );
	Msg( "Resampled voice log from %d to %d samples\n", (int)( bufRaw->size() / 2 ), numResamples );
}

CON_COMMAND( snd_voice_log_resample44, "Resample voice log file all the way up to 44100" )
{
	g_bufSndVoiceLog.clear();
	auto bufRaw = g_pFullFileSystem->ReadFile( args.Arg( 1 ), nullptr );
	if ( !bufRaw )
	{
		Warning( "Failed to read voice log from file '%s'\n", args.Arg( 1 ) );
		return;
	}
	else
	{
		Msg( "Loaded voice log from file '%s'\n", args.Arg( 1 ) );
	}

	Resample_CELP_to_PC_t rcpt;
	std::vector< int16 > in( bufRaw->size() / 2 );
	std::memcpy( in.data(), bufRaw->data(), in.size() * sizeof( int16 ) );
	std::vector< int16 > buf11025( bufRaw->size() + 2 );
	uint32 numResamples = rcpt.Resample( in.data(), in.size(), buf11025.data() );
	Msg( "Resampled voice log from %d to %d @11025 samples\n", (int)( bufRaw->size() / 2 ), numResamples );

	std::vector< int16 > outSamples;
	outSamples.reserve( numResamples * 4 );
	const int16 *pIn = buf11025.data();
	const int16 *pInEnd = pIn + numResamples;
	while ( pIn < pInEnd )
	{
		int16 a = *( pIn++ );
		int16 b = ( pIn < pInEnd ) ? *pIn : a;

		for ( int jj = 0; jj < 4; ++ jj )
		{
			outSamples.push_back( ( double( b ) * ( jj ) + double( a ) * ( 4 - jj ) ) / 4.0 );
		}
	}
	g_bufSndVoiceLog.write_bytes( outSamples.data(), outSamples.size() * sizeof( int16 ) );
}

#endif // SND_VOICE_LOG_DEBUG

class CEngineVoiceSteam : public IEngineVoice
{
public:
	CEngineVoiceSteam();
	~CEngineVoiceSteam();

public:
	virtual bool IsHeadsetPresent( int iController );
	virtual bool IsLocalPlayerTalking( int iController );

	virtual void AddPlayerToVoiceList( XUID xPlayer, int iController, uint64 uiFlags );
	virtual void RemovePlayerFromVoiceList( XUID xPlayer, int iController );

	virtual void GetRemoteTalkers( int *pNumTalkers, XUID *pRemoteTalkers );

	virtual bool VoiceUpdateData( int iController );
	virtual void GetVoiceData( int iController, const byte **ppvVoiceDataBuffer, unsigned int *pnumVoiceDataBytes );
	virtual void VoiceResetLocalData( int iController );

	virtual void SetPlaybackPriority( XUID remoteTalker, int iController, int iAllowPlayback );
	virtual void PlayIncomingVoiceData( XUID xuid, const byte *pbData, unsigned int dwDataSize, const bool *bAudiblePlayers = nullptr );

	virtual void RemoveAllTalkers();

protected:
	void AudioInitializationUpdate();
	void UpdateHUDVoiceStatus();
	bool IsPlayerTalking( XUID xuid );

public:
	bool m_bLocalVoice[ XUSER_MAX_COUNT ];
	struct RemoteTalker_t
	{
		XUID m_xuid;
		uint64 m_uiFlags;
		float m_flLastTalkTimestamp;
	};
	std::vector< RemoteTalker_t > m_arrRemoteVoice;
	bool m_bVoiceForPs3;
	int FindRemoteTalker( XUID xuid )
	{
		for ( int k = 0; k < m_arrRemoteVoice.Count(); ++ k )
			if ( m_arrRemoteVoice[k].m_xuid == xuid )
				return k;
		return m_arrRemoteVoice.InvalidIndex();
	}
	bool m_bInitializedAudio;
	byte m_pbVoiceData[ 18*1024 * XUSER_MAX_COUNT ];
	float m_flLastTalkingTimestamp;

};

CEngineVoiceSteam::CEngineVoiceSteam()
{
	memset( m_bLocalVoice, 0, sizeof( m_bLocalVoice ) );
	memset( m_pbVoiceData, 0, sizeof( m_pbVoiceData ) );
	m_bInitializedAudio = false;
	m_bVoiceForPs3 = false;
	m_flLastTalkingTimestamp = 0.0f;

}

CEngineVoiceSteam::~CEngineVoiceSteam()
{
}

bool CEngineVoiceSteam::IsHeadsetPresent( int iController )
{
	return false;
}

bool CEngineVoiceSteam::IsLocalPlayerTalking( int iController )
{
	EVoiceResult res = Steam3Client().SteamUser()->GetAvailableVoice( nullptr, nullptr, 0 );
	switch ( res )
	{
	case k_EVoiceResultOK:
	case k_EVoiceResultNoData:
		return true;
	default:
		return ( ( Plat_FloatTime() - m_flLastTalkingTimestamp ) <= 0.2f );
	}
}

bool CEngineVoiceSteam::IsPlayerTalking( XUID uid )
{
	if ( !g_pMatchFramework->GetMatchSystem()->GetMatchVoice()->CanPlaybackTalker( uid ) )
		return false;
	
	for ( int k = 0; k < m_arrRemoteVoice.Count(); ++ k )
	{
		if ( m_arrRemoteVoice[k].m_xuid == uid )
		{
			return ( ( Plat_FloatTime() - m_arrRemoteVoice[k].m_flLastTalkTimestamp ) < 0.2 );
		}
	}
	return false;
}

void CEngineVoiceSteam::AddPlayerToVoiceList( XUID xPlayer, int iController, uint64 uiFlags )
{
	if ( !xPlayer && iController >= 0 && iController < XUSER_MAX_COUNT )
	{
		// Add local player
		m_bLocalVoice[ iController ] = true;
		AudioInitializationUpdate();
		
		if ( snd_voice_echo.GetBool() )
		{
			Steam3Client().SteamUser()->StartVoiceRecording();
		}
	}

	if ( xPlayer )
	{
		if ( FindRemoteTalker( xPlayer ) == m_arrRemoteVoice.InvalidIndex() )
		{
			RemoteTalker_t rt = { xPlayer, uiFlags, 0 };
			m_arrRemoteVoice.AddToTail( rt );
			AudioInitializationUpdate();
			// Steam3Client().SteamUser()->StartVoiceRecording();
		}
	}
}

void CEngineVoiceSteam::RemovePlayerFromVoiceList( XUID xPlayer, int iController )
{
	if ( !xPlayer && iController >= 0 && iController < XUSER_MAX_COUNT )
	{
		// Remove local player
		m_bLocalVoice[ iController ] = false;
		AudioInitializationUpdate();
		Steam3Client().SteamUser()->StopVoiceRecording();
	}

	if ( xPlayer )
	{
		int idx = FindRemoteTalker( xPlayer );
		if ( idx != m_arrRemoteVoice.InvalidIndex() )
		{
			m_arrRemoteVoice.FastRemove( idx );
			AudioInitializationUpdate();
		}
	}
}

void CEngineVoiceSteam::GetRemoteTalkers( int *pNumTalkers, XUID *pRemoteTalkers )
{
	if ( pNumTalkers )
		*pNumTalkers = m_arrRemoteVoice.Count();

	if ( pRemoteTalkers )
	{
		for ( int k = 0; k < m_arrRemoteVoice.Count(); ++ k )
			pRemoteTalkers[k] = m_arrRemoteVoice[k].m_xuid;
	}
}

bool CEngineVoiceSteam::VoiceUpdateData( int iController )
{
#ifdef SND_VOICE_LOG_DEBUG
	if ( snd_voice_log.GetInt() == SND_VOICE_LOG_TEST_PLAYBACK_LOG )
	{
		PlayIncomingVoiceData( 2, nullptr, 0, nullptr );
		return false;
	}
#endif // SND_VOICE_LOG_DEBUG

	EVoiceResult res = Steam3Client().SteamUser()->GetAvailableVoice( nullptr, nullptr, 0 );
	bool bResult = ( res == k_EVoiceResultOK );
	if ( bResult )
		m_flLastTalkingTimestamp = Plat_FloatTime();
	UpdateHUDVoiceStatus();
	return bResult;
}

void CEngineVoiceSteam::UpdateHUDVoiceStatus( void )
{
	for ( int iClient = 0; iClient < GetBaseLocalClient().m_nMaxClients; iClient++ )
	{
		// Information about local client if it's a local client speaking
		bool bLocalClient = false;
		int iSsSlot = -1;
		int iCtrlr = -1;

		// Detection if it's a local client
		for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
		{
			CClientState &cs = GetLocalClient( k );
			if ( cs.m_nPlayerSlot == iClient )
			{
				bLocalClient = true;
				iSsSlot = k;
				iCtrlr = XBX_GetUserId( iSsSlot );
			}
		}

		// Convert into index and XUID
		int iIndex = iClient + 1;
		XUID xid =  nullptr;
		player_info_t infoClient;
		if ( engineClient->GetPlayerInfo( iIndex, &infoClient ) )
		{
			xid = infoClient.xuid;
		}

		if ( !xid )
			// No XUID means no VOIP
		{
			g_pSoundServices->OnChangeVoiceStatus( iIndex, -1, false );
			if ( bLocalClient )
				g_pSoundServices->OnChangeVoiceStatus( iIndex, iSsSlot, false );
			continue;
		}

		// Determine talking status
		bool bTalking = false;

		if ( bLocalClient )
		{
			//Make sure the player's own "remote" label is not on.
			g_pSoundServices->OnChangeVoiceStatus( iIndex, -1, false );
			iIndex = -1; // issue notification as ent=-1
			bTalking = IsLocalPlayerTalking( iCtrlr );
		}
		else
		{
			bTalking = IsPlayerTalking( xid );
		}

		g_pSoundServices->OnChangeVoiceStatus( iIndex, iSsSlot, bTalking );
	}
}

void CEngineVoiceSteam::GetVoiceData( int iController, const byte **ppvVoiceDataBuffer, unsigned int *pnumVoiceDataBytes )
{
	const int size = ARRAYSIZE( m_pbVoiceData ) / XUSER_MAX_COUNT;
	byte *pbVoiceData = m_pbVoiceData + iController * ARRAYSIZE( m_pbVoiceData ) / XUSER_MAX_COUNT;
	*ppvVoiceDataBuffer = pbVoiceData;
	
	EVoiceResult res = k_EVoiceResultOK;
	if ( !m_bVoiceForPs3 )
	{
		res = Steam3Client().SteamUser()->GetVoice( true, pbVoiceData, size, pnumVoiceDataBytes, false, nullptr, 0, nullptr, 0 );
	}

	// On PC respect user push-to-talk setting and don't transmit voice
	// if push-to-talk key is not held
	static ConVarRef voice_system_enable( "voice_system_enable" ); // voice system is initialized
	static ConVarRef voice_enable( "voice_enable" ); // mute all player voice data, and don't send voice data for this player
	static ConVarRef voice_vox( "voice_vox" ); // open mic
	static ConVarRef voice_ptt( "voice_ptt" ); // mic ptt release time
	if ( !voice_system_enable.GetBool() || !voice_enable.GetBool() )
		goto prevent_voice_comm;
	if ( !voice_vox.GetInt() )
	{
		float flPttReleaseTime = voice_ptt.GetFloat();
		if ( flPttReleaseTime && ( ( Plat_FloatTime() - flPttReleaseTime ) > 1.0f ) )
		{
			// User is in push-to-talk mode and released the talk key over a second ago
			// don't transmit any voice in this case
			goto prevent_voice_comm;
		}
	}
	
	switch ( res )
	{
	case k_EVoiceResultNoData:
	case k_EVoiceResultOK:
		return;
	default:
prevent_voice_comm:
		*pnumVoiceDataBytes = 0;
		*ppvVoiceDataBuffer = nullptr;
		return;
	}
}

void CEngineVoiceSteam::VoiceResetLocalData( int iController )
{
	const int size = ARRAYSIZE( m_pbVoiceData ) / XUSER_MAX_COUNT;
	byte *pbVoiceData = m_pbVoiceData + iController * ARRAYSIZE( m_pbVoiceData ) / XUSER_MAX_COUNT;
	memset( pbVoiceData, 0, size );
}

void CEngineVoiceSteam::SetPlaybackPriority( XUID remoteTalker, int iController, int iAllowPlayback )
{
	;
}

void CEngineVoiceSteam::PlayIncomingVoiceData( XUID xuid, const byte *pbData, unsigned int dwDataSize, const bool *bAudiblePlayers /* = NULL */ )
{
	int idxRemoteTalker = 0;
	for ( DWORD dwSlot = 0; dwSlot < XBX_GetNumGameUsers(); ++ dwSlot )
	{
		int iCtrlr = dwSlot;
		IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iCtrlr );
		if ( pPlayer && pPlayer->GetXUID() == xuid )
		{
			//Hack: Don't play stuff that comes from ourselves.
			if ( snd_voice_echo.GetBool() )
			{
				if ( !m_arrRemoteVoice.Count() )
				{
					RemoteTalker_t rt = { 0, m_bVoiceForPs3 ? ENGINE_VOICE_FLAG_PS3 : 0, 0 };
					m_arrRemoteVoice.AddToTail( rt );
				}
				goto playvoice;
			}
			return;
		}
	}

#ifdef SND_VOICE_LOG_DEBUG
	if ( snd_voice_log.GetInt() == SND_VOICE_LOG_TEST_PLAYBACK_LOG )
	{
		if ( !m_arrRemoteVoice.Count() )
		{
			RemoteTalker_t rt = { 0, m_bVoiceForPs3 ? ENGINE_VOICE_FLAG_PS3 : 0, 0 };
			m_arrRemoteVoice.AddToTail( rt );
		}
	}
	else
#endif
	{
		// Make sure voice playback is allowed for the specified user
		if ( !g_pMatchFramework->GetMatchSystem()->GetMatchVoice()->CanPlaybackTalker( xuid ) )
			return;

		// Find registered remote talker
		idxRemoteTalker = FindRemoteTalker( xuid );
		if ( idxRemoteTalker == m_arrRemoteVoice.InvalidIndex() )
			return;
	}

playvoice:
	// Uncompress the voice data
	char pbUncompressedVoice[ 11025 * 2 ];
	uint32 numUncompressedBytes = 0;

#ifdef SND_VOICE_LOG_DEBUG
	if ( snd_voice_log.GetInt() == SND_VOICE_LOG_TEST_PLAYBACK_LOG )
	{
		const int nVoiceLogFreq = 11025;
		const int nVoiceLogSamples = 441;

		static float s_flLastTime = 0;
		static int s_nBufferPosition = 0;
		if ( (int)g_bufSndVoiceLog.size() <= nVoiceLogSamples )
			return;

		if ( !s_flLastTime )
		{
			s_flLastTime = Plat_FloatTime();
			return;
		}

		// See how many bytes we can send, assuming 16,000 bytes/sec (8000Hz), rounding to nearest 320 bytes
		float flCurTime = Plat_FloatTime();
		if ( flCurTime - s_flLastTime > 1.0f )
		{
			// drop frames
			s_flLastTime = Plat_FloatTime();
			return;
		}
		int numSoundFrames = nVoiceLogFreq * ( flCurTime - s_flLastTime ) / nVoiceLogSamples;
		if ( numSoundFrames <= 0 )
			return;

		// Advance time pointer
		s_flLastTime += numSoundFrames * nVoiceLogSamples /  float( nVoiceLogFreq );

		int16 *piWritePos = ( int16 * ) pbUncompressedVoice;
		while ( numSoundFrames --> 0 )
		{
			// See if we need to reset buffer position
			if ( s_nBufferPosition + nVoiceLogSamples > (int)g_bufSndVoiceLog.size() )
				s_nBufferPosition = 0;

			// uint32 numOutSamples = m_resampleCelp2Pc.Resample( ( ( const int16 * ) g_bufSndVoiceLog.data() ) + ( s_nBufferPosition / 2 ), 160, piWritePos );
					uint32 numOutSamples = nVoiceLogSamples;
					V_memcpy( piWritePos, ( ( const int16 * ) g_bufSndVoiceLog.data() ) + ( s_nBufferPosition / sizeof( int16 ) ), nVoiceLogSamples* sizeof( int16 ) );
			piWritePos += numOutSamples;
			numUncompressedBytes += numOutSamples * 2;
			s_nBufferPosition += nVoiceLogSamples * sizeof( int16 );
		}
		if ( !numUncompressedBytes )
			return;
	}
	else
#endif // SND_VOICE_LOG_DEBUG
	if ( !(m_arrRemoteVoice[idxRemoteTalker].m_uiFlags & ENGINE_VOICE_FLAG_PS3) )
	{
		EVoiceResult res = Steam3Client().SteamUser()->DecompressVoice( const_cast< byte * >( pbData ), dwDataSize,
			pbUncompressedVoice, sizeof( pbUncompressedVoice ), &numUncompressedBytes, 11025 );

		if ( res != k_EVoiceResultOK )
			return;
	}

	// Voice channel index
	int idxVoiceChan = idxRemoteTalker;

	int nChannel = Voice_GetChannel( idxVoiceChan );
	if ( nChannel == VOICE_CHANNEL_ERROR )
	{
		// Create a channel in the voice engine and a channel in the sound engine for this guy.
		nChannel = Voice_AssignChannel( idxVoiceChan, false, 0.0f );
	}

	// Give the voice engine the data (it in turn gives it to the mixer for the sound engine).
	if ( nChannel != VOICE_CHANNEL_ERROR )
	{
		Voice_AddIncomingData( nChannel, reinterpret_cast<const char*>(pbData), dwDataSize, 0, 0, 0, VoiceFormat_Steam );
		m_arrRemoteVoice[idxRemoteTalker].m_flLastTalkTimestamp = Plat_FloatTime();
	}
}

void CEngineVoiceSteam::RemoveAllTalkers()
{
	memset( m_bLocalVoice, 0, sizeof( m_bLocalVoice ) );
	m_arrRemoteVoice.RemoveAll();
	AudioInitializationUpdate();
}

void CEngineVoiceSteam::AudioInitializationUpdate()
{
	bool bHasTalkers = ( m_arrRemoteVoice.Count() > 0 );
	for ( int k = 0; k < ARRAYSIZE( m_bLocalVoice ); ++ k )
	{
		if ( m_bLocalVoice[k] )
		{
			bHasTalkers = true;
			break;
		}
	}

	// Initialized already
	if ( bHasTalkers == m_bInitializedAudio )
		return;

	// Clear out voice buffers
	memset( m_pbVoiceData, 0, sizeof( m_pbVoiceData ) );

	// Init or deinit voice system
	if ( bHasTalkers )
	{
		Voice_ForceInit();
	}
	else
	{
		Voice_Deinit();
	}

	m_bInitializedAudio = bHasTalkers;
}


IEngineVoice *Audio_GetEngineVoiceSteam()
{
	static CEngineVoiceSteam s_EngineVoiceSteam;
	return &s_EngineVoiceSteam;
}

#else

IEngineVoice *Audio_GetEngineVoiceSteam()
{
	return Audio_GetEngineVoiceStub();
}

#endif
