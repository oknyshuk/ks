// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
#if defined(CLIENT_DLL)
#include "const.h"
#include "string_t.h"
#include "predictableid.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "c_recipientfilter.h"
#include "cdll_client_int.h"
#include "debugoverlay_shared.h"
#elif defined(GAME_DLL)
#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "soundflags.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "entitylist.h"
#include "physics.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#endif
#include "reflect_annotations.h"
#include "reflect_datamap.h"
#include "sharedInterface.h"
#include "soundenvelope.h"
#include "engine/IEngineSound.h"
#include "IEffects.h"
#include "gamestringpool.h"
#include "igamesystem.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"

static ConVar soundpatch_captionlength( "soundpatch_captionlength", "2.0", FCVAR_REPLICATED, "How long looping soundpatch captions should display for." );

// Envelope
// This is a class that controls a ramp for a sound (pitch / volume / etc)
class CSoundEnvelope
{
public:
	DECLARE_SIMPLE_DATADESC();

	CSoundEnvelope()
	{
		m_current = 0.0f;
		m_target = 0.0f;
		m_rate = 0.0f;
		m_forceupdate = false;
	}

	void SetTarget( float target, float deltaTime );
	void SetValue( float value );
	bool ShouldUpdate( void );
	void Update( float time );
	inline float	Value( void ) { return m_current; }

private:
	float	m_current;
	float	m_target;
	float	m_rate;
	bool	m_forceupdate;
};


IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CSoundEnvelope )


//-----------------------------------------------------------------------------
// Purpose: Set the new target value for this ramp.  Reach this target in deltaTime
//			seconds from now
// Input  : target - new target value
//			deltaTime - time to reach target
//-----------------------------------------------------------------------------
void CSoundEnvelope::SetTarget( float target, float deltaTime )
{
	float deltaValue = target - m_current;

	if ( deltaValue && deltaTime > 0 )
	{
		m_target = target;
		m_rate = std::max<std::common_type_t<double, float>>( 0.1, fabs(deltaValue / deltaTime) );
	}
	else
	{
		if ( target != m_current )
		{
			m_forceupdate = true;
		}

		SetValue( target );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Instantaneously set the value of this ramp
// Input  : value - new value
//-----------------------------------------------------------------------------
void CSoundEnvelope::SetValue( float value )
{
	if ( m_target != value )
	{
		m_forceupdate = true;
	}

	m_current = m_target = value;
	m_rate = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Check to see if I need to update this envelope
// Output : Returns true if this envelope is changing
//-----------------------------------------------------------------------------
bool CSoundEnvelope::ShouldUpdate( void )
{
	if ( m_forceupdate )
	{
		m_forceupdate = false;
		return true;
	}

	if ( m_current != m_target )
	{
		return true;
	}
	
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Update the envelope for the current frame time
// Input  : time - amount of time that has passed
//-----------------------------------------------------------------------------
void CSoundEnvelope::Update( float deltaTime )
{
	m_current = Approach( m_target, m_current, m_rate * deltaTime );
}

class CCopyRecipientFilter : public IRecipientFilter
{
public:
	DECLARE_SIMPLE_DATADESC();

	CCopyRecipientFilter() : m_Flags(0) {}

	void Init( IRecipientFilter *pSrc )
	{
		m_Flags = FLAG_ACTIVE;
		if ( pSrc->IsReliable() )
		{
			m_Flags |= FLAG_RELIABLE;
		}

		if ( pSrc->IsInitMessage() )
		{
			m_Flags |= FLAG_INIT_MESSAGE;
		}

		for ( int i = 0; i < pSrc->GetRecipientCount(); i++ )
		{
			int index = pSrc->GetRecipientIndex( i );
			
			if ( index >= 0 )
				m_Recipients.push_back( index );
		}
	}

	bool IsActive() const 
	{
		return (m_Flags & FLAG_ACTIVE) != 0;
	}

	virtual bool IsReliable( void ) const
	{
		return (m_Flags & FLAG_RELIABLE) != 0;
	}

	virtual int	GetRecipientCount( void ) const
	{
		return std::ssize( m_Recipients );
	}

	virtual int	GetRecipientIndex( int slot ) const
	{
		return m_Recipients[ slot ];
	}

	virtual bool IsInitMessage( void ) const
	{
		return (m_Flags & FLAG_INIT_MESSAGE) != 0;
	}

	virtual bool AddRecipient( CBasePlayer *player )
	{
		contract_assert( player );

		int index = player->entindex();

		if ( index < 0 )
			return false;

		// Already in list
		if ( ks::find_index( m_Recipients, index ) != -1 )
			return false;

		m_Recipients.push_back( index );
		return true;
	}

private:
	enum
	{
		FLAG_ACTIVE = 0x1,
		FLAG_RELIABLE = 0x2,
		FLAG_INIT_MESSAGE = 0x4,
	};

	int m_Flags;
	std::vector< int > m_Recipients;
};

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CCopyRecipientFilter )


// This is the a basic sound controller, a "patch"
// It has envelopes for pitch and volume and can manage state changes to those
class CSoundPatch : public ks::pooled< CSoundPatch >
{
public:
	DECLARE_SIMPLE_DATADESC();

	static int g_SoundPatchCount;
	CSoundPatch()
	{
		g_SoundPatchCount++;
		m_iszSoundName = NULL_STRING;
		m_iszSoundScriptName = NULL_STRING;
		m_flCloseCaptionDuration = soundpatch_captionlength.GetFloat();
		m_soundOrigin.Init();
		m_soundEntityIndex = -1;
		m_guid = -1;

		m_hSoundScriptHash = SOUNDEMITTER_INVALID_HASH;
		m_nSoundEntryVersion = 1;
	}
	~CSoundPatch()
	{
		g_SoundPatchCount--;
	}

	void	Init( IRecipientFilter *pFilter, CBaseEntity *pEnt, int channel, const char *pSoundName, 
				soundlevel_t iSoundLevel, const Vector *pSoundOrigin, float scriptVolume = 1.0f );
	void	ChangePitch( float pitchTarget, float deltaTime );
	void	ChangeVolume( float volumeTarget, float deltaTime );
	void	FadeOut( float deltaTime, bool destroyOnFadeout );
	float	GetPitch( void );
	float	GetVolume( void );
	string_t GetName() { return m_iszSoundName; };
#ifdef CLIENT_DLL
	int		GetGuid() { return m_guid; };
	float	GetElapsedTime( void );
	bool	IsStillPlaying( void );
#endif

	string_t GetScriptName() { return m_iszSoundScriptName; }
	// UNDONE: Don't call this, use the controller to shut down
	void	Shutdown( void );
	bool	Update( float time, float deltaTime );
	void	Reset( void );
	void	StartSound( float flStartTime = 0 );
	void	ResumeSound( void );
	int		IsPlaying( void ) { return m_isPlaying; }
	float	GetShutdownTime( void ) const { return m_shutdownTime; } // TERROR: debugging
	void	AddPlayerPost( CBasePlayer *pPlayer );
	void	SetCloseCaptionDuration( float flDuration ) { m_flCloseCaptionDuration = flDuration; }

	void	SetBaseFlags( int iFlags ) { m_baseFlags = iFlags; }
	
	// Returns the ent index
	int		EntIndex() const;

private:
	// SoundPatches take volumes between 0 & 1, and use that to multiply the sounds.txt specified volume.
	// This function is an internal method of accessing the real volume passed into the engine (i.e. post multiply)
	float	GetVolumeForEngine( void );

private:
	CSoundEnvelope	m_pitch;
	CSoundEnvelope	m_volume;

	int				m_guid;
	soundlevel_t	m_soundlevel;
	float			m_shutdownTime;
	string_t		m_iszSoundName;
	string_t		m_iszSoundScriptName;
	HSOUNDSCRIPTHASH m_hSoundScriptHash;
	int             m_nSoundEntryVersion;
	EHANDLE			m_hEnt;
	int				m_entityChannel;
	int				m_soundEntityIndex;
	Vector			m_soundOrigin;
	int				m_flags;
	int				m_baseFlags;
	int				m_isPlaying;
	float			m_flScriptVolume;	// Volume for this sound in sounds.txt
	CCopyRecipientFilter m_Filter;

	float			m_flCloseCaptionDuration;

#ifdef _DEBUG
	// Used to get the classname of the entity associated with the sound
	string_t		m_iszClassName;
#endif

	private:
		static ks::fixed_pool s_Allocator;
		friend class ks::pooled< CSoundPatch >;
};

int CSoundPatch::g_SoundPatchCount = 0;

#ifdef CLIENT_DLL
static void cl_report_soundpatch( const CCommand &args );
static ConCommand cl_report_soundpatch_command( "cl_report_soundpatch", cl_report_soundpatch, "reports client-side sound patch count" );
static void cl_report_soundpatch( const CCommand &args )
#else
static void report_soundpatch( const CCommand &args );
static ConCommand report_soundpatch_command( "report_soundpatch", report_soundpatch, "reports sound patch count" );
static void report_soundpatch( const CCommand &args )
#endif
{
#ifndef CLIENT_DLL
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;
#endif

	Msg("Current sound patches: %d\n", CSoundPatch::g_SoundPatchCount );
}
ks::fixed_pool CSoundPatch::s_Allocator( sizeof(CSoundPatch), 64, ks::fixed_pool::grow::fast, "CSoundPatch pool", alignof(CSoundPatch) );

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CSoundPatch )



//-----------------------------------------------------------------------------
// Purpose: Setup the patch
// Input  : nEntIndex - index of the edict that owns the sound channel
//			channel - This is a sound channel (CHAN_ITEM, CHAN_STATIC)
//			*pSoundName - sound script string name
//			attenuation - attenuation of this sound (not animated)
//-----------------------------------------------------------------------------
void CSoundPatch::Init( IRecipientFilter *pFilter, CBaseEntity *pEnt, int channel, const char *pSoundName, 
			soundlevel_t soundlevel, const Vector *pSoundOrigin , float scriptVolume)
{
	m_hEnt = pEnt;
	if ( pEnt )
	{
		m_soundEntityIndex = pEnt->entindex();
	}
	m_entityChannel = channel;

	// if not a direct wave reference, crack the script
	CSoundParameters params;
	if ( !V_stristr( pSoundName, ".wav" ) && !V_stristr( pSoundName, ".mp3" ) &&
		CBaseEntity::GetParametersForSound( pSoundName, params, nullptr ) )
	{
		m_flScriptVolume = params.volume;
		// This has to be the actual .wav because rndwave would cause a bunch of new .wavs to play... bad...
		//  e.g., when you pitch shift it would start a different wav instead.

		m_iszSoundScriptName = AllocPooledString( pSoundName );
		m_hSoundScriptHash = params.m_hSoundScriptHash;
		m_nSoundEntryVersion = params.m_nSoundEntryVersion;

		pSoundName = params.soundname;
		m_soundlevel = params.soundlevel;

		// TERROR: if we say we want CHAN_USER_BASE + N, we mean it!
		if ( m_entityChannel < CHAN_USER_BASE )
		{
			m_entityChannel = params.channel;
		}
	}
	else
	{
		m_iszSoundScriptName = AllocPooledString( pSoundName );
		m_flScriptVolume = scriptVolume;
		m_soundlevel = soundlevel;
	}

	m_iszSoundName = AllocPooledString( pSoundName );
	m_volume.SetValue( 0 );
	m_pitch.SetValue( 0 );
	m_isPlaying = false;
	m_shutdownTime = 0;
	m_Filter.Init( pFilter );
	m_baseFlags = 0;
	if( pSoundOrigin )
	{
		m_soundOrigin.x = pSoundOrigin->x;
		m_soundOrigin.y = pSoundOrigin->y;
		m_soundOrigin.z = pSoundOrigin->z;
	}

#ifdef _DEBUG
	if ( pEnt )
	{
		m_iszClassName = AllocPooledString( pEnt->GetClassname() );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Ramps the pitch to a new value
// Input  : pitchTarget - new value
//			deltaTime - seconds to reach the value
//-----------------------------------------------------------------------------
void CSoundPatch::ChangePitch( float pitchTarget, float deltaTime )
{
	m_flags |= SND_CHANGE_PITCH;
	m_pitch.SetTarget( pitchTarget, deltaTime );
}


//-----------------------------------------------------------------------------
// Purpose: Ramps the volume to a new value
// Input  : volumeTarget - new volume
//			deltaTime - seconds to reach the new volume 
//-----------------------------------------------------------------------------
void CSoundPatch::ChangeVolume( float volumeTarget, float deltaTime )
{
	m_flags |= SND_CHANGE_VOL;
	if ( volumeTarget > 1.0 )
		volumeTarget = 1.0;
	m_volume.SetTarget( volumeTarget, deltaTime );
}


//-----------------------------------------------------------------------------
// Purpose: Fade volume to zero AND SHUT DOWN THIS SOUND
// Input  : deltaTime - seconds before done/shutdown
//-----------------------------------------------------------------------------
void CSoundPatch::FadeOut( float deltaTime, bool destroyOnFadeout )
{
	ChangeVolume( 0, deltaTime );
	if ( !destroyOnFadeout )
	{
		m_shutdownTime = g_pEffects->Time() + deltaTime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the sound's current pitch
//-----------------------------------------------------------------------------
float CSoundPatch::GetPitch( void )
{
	return m_pitch.Value();
}

//-----------------------------------------------------------------------------
// Purpose: Get the sound's current volume
//-----------------------------------------------------------------------------
float CSoundPatch::GetVolume( void )
{
	return m_volume.Value();
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: Get the playing status of the sound
// Returns: Sounds playing status from the engine
//-----------------------------------------------------------------------------
bool CSoundPatch::IsStillPlaying( void )
{
	return enginesound->IsSoundStillPlaying(m_guid);
}

//-----------------------------------------------------------------------------
// Purpose: Get the sound's current elapsed time
// Returns: Time in seconds
//-----------------------------------------------------------------------------
float CSoundPatch::GetElapsedTime( void )
{
	// convert to seconds
	return enginesound->GetElapsedTimeByGuid(m_guid) * 0.01;
}
#endif
//-----------------------------------------------------------------------------
// Returns the ent index
//-----------------------------------------------------------------------------
inline int CSoundPatch::EntIndex() const
{
	contract_assert( !m_hEnt.IsValid() || m_hEnt.Get() );
	return m_hEnt.Get() ? m_hEnt->entindex() : -1;
}

//-----------------------------------------------------------------------------
// Purpose: SoundPatches take volumes between 0 & 1, and use that to multiply the sounds.txt specified volume.
// This function is an internal method of accessing the real volume passed into the engine (i.e. post multiply)
// Output : float
//-----------------------------------------------------------------------------
float CSoundPatch::GetVolumeForEngine( void )
{
	return ( m_flScriptVolume * m_volume.Value() );
}

//-----------------------------------------------------------------------------
// Purpose: Stop the sound
//-----------------------------------------------------------------------------
void CSoundPatch::Shutdown( void )
{
//	Msg( "Removing sound %s\n", m_pszSoundName );
	if ( m_isPlaying )
	{
		int entIndex = -1;
		if ( m_hEnt.Get() )
		{
			entIndex = EntIndex();
		}
		else
		{
			// may have deleted the entity after starting the sound, but before stopping the sound, try the saved index
			// this will handle that case so a sound patch doesn't get stuck on
			entIndex = m_soundEntityIndex;
		}
		contract_assert( entIndex >= 0 );
		// BUGBUG: Don't crash in release mode
		if ( entIndex >= 0 )
		{
			if( m_hSoundScriptHash != SOUNDEMITTER_INVALID_HASH )
			{
				CBaseEntity::StopSound( entIndex, STRING( m_iszSoundScriptName ) );
			}
			else
			{
				CBaseEntity::StopSound( entIndex, m_entityChannel, STRING( m_iszSoundName ) );
			}
		}
		m_isPlaying = false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Update all envelopes and send appropriate data to the client
// Input  : time - new global clock
//			deltaTime - amount of time that has passed
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSoundPatch::Update( float time, float deltaTime )
{
	CVProfScope _( "CSoundPatch::Update", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	if ( m_shutdownTime && time > m_shutdownTime )
	{
		Shutdown();
		return false;
	}

	if ( EntIndex() < 0 )
	{
		// FIXME:  The pointer to this soundpatch is probably leaked since no entity is around to clean it up (ywb)
		DevWarning( "CSoundPatch::Update:  Removing CSoundPatch (%s) with NULL EHandle\n", STRING(m_iszSoundName) );
		return false;
	}

	if ( m_pitch.ShouldUpdate() )
	{
		m_pitch.Update( deltaTime );
		m_flags |= SND_CHANGE_PITCH;
	}
	else 
	{
		m_flags &= ~SND_CHANGE_PITCH;
	}

	if ( m_volume.ShouldUpdate() )
	{
		m_volume.Update( deltaTime );
		m_flags |= SND_CHANGE_VOL;
	}
	else 
	{
		m_flags &= ~SND_CHANGE_VOL;
	}

	// if ( m_flags && m_Filter.IsActive() )
	if ( m_flags )
	{
		// SoundPatches take volumes between 0 & 1, and use that to multiply the sounds.txt specified volume.
		// Because of this, we need to always set the SND_CHANGE_VOL flag when we emit sound, or it'll use the scriptfile's instead.
		m_flags |= SND_CHANGE_VOL;

		EmitSound_t ep;
		ep.m_nChannel = m_entityChannel;
		ep.m_pSoundName = STRING(m_iszSoundName);
		ep.m_flVolume = GetVolumeForEngine();
		ep.m_SoundLevel = m_soundlevel;
		ep.m_nFlags = m_flags;
		ep.m_nPitch = (int)m_pitch.Value();
		ep.m_hSoundScriptHash = m_hSoundScriptHash;
		ep.m_nSoundEntryVersion = m_nSoundEntryVersion;


		// only pass the position if it's coming from the world
		if( EntIndex() == 0 )
			ep.m_pOrigin = &m_soundOrigin;

		CBaseEntity::EmitSound( m_Filter, EntIndex(), ep );

		m_flags = 0;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Sound is going to start playing again, clear any shutdown time
//-----------------------------------------------------------------------------
void CSoundPatch::Reset( void )
{
	m_shutdownTime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Start playing the sound - send updates to the client
//-----------------------------------------------------------------------------
void CSoundPatch::StartSound( float flStartTime )
{
//	Msg( "Start sound %s\n", m_pszSoundName );
	m_flags = 0;
	if ( m_Filter.IsActive() )
	{
		EmitSound_t ep;
		ep.m_nChannel = m_entityChannel;
		ep.m_pSoundName = STRING(m_iszSoundName);
		ep.m_flVolume = GetVolumeForEngine();
		ep.m_SoundLevel = m_soundlevel;

		ep.m_hSoundScriptHash = m_hSoundScriptHash;
		ep.m_nSoundEntryVersion = m_nSoundEntryVersion;

		// only pass the position if it's coming from the world
		if( EntIndex() == 0 )
			ep.m_pOrigin = &m_soundOrigin;

		if ( V_stristr( STRING(m_iszSoundName), "music" ) )
		{
			ep.m_nFlags = m_baseFlags;
		}
		else
		{
			ep.m_nFlags = (SND_CHANGE_VOL | m_baseFlags);
		}
		ep.m_nFlags |= SND_GENERATE_GUID;				// We need GUID in all cases, even for threaded sounds
		ep.m_nPitch = (int)m_pitch.Value();
		ep.m_bEmitCloseCaption = false;

		if ( flStartTime )
		{
			ep.m_flSoundTime = flStartTime;
		}

//#ifdef CLIENT_DLL
		{
#ifdef CLIENT_DLL
			m_guid = CBaseEntity::EmitSound( m_Filter, EntIndex(), ep );
#else
			CBaseEntity::EmitSound( m_Filter, EntIndex(), ep );
#endif
		}
		CBaseEntity::EmitCloseCaption( m_Filter, EntIndex(), STRING( m_iszSoundScriptName ), ep.m_UtlVecSoundOrigin, m_flCloseCaptionDuration, true );
	}
	m_isPlaying = true;
}


//-----------------------------------------------------------------------------
// Purpose: resumes playing the sound on restore
//-----------------------------------------------------------------------------
void CSoundPatch::ResumeSound( void )
{
	if ( IsPlaying() && m_Filter.IsActive() )
	{
		if ( EntIndex() >= 0 )
		{
			EmitSound_t ep;
			ep.m_nChannel = m_entityChannel;
			ep.m_pSoundName = STRING(m_iszSoundName);
			ep.m_flVolume = GetVolumeForEngine();
			ep.m_SoundLevel = m_soundlevel;
			ep.m_nFlags = (SND_CHANGE_VOL | SND_CHANGE_PITCH | m_baseFlags);
			ep.m_nPitch = (int)m_pitch.Value();

			ep.m_hSoundScriptHash = m_hSoundScriptHash;
			ep.m_nSoundEntryVersion = m_nSoundEntryVersion;

			// only pass the position if it's coming from the world
			if( EntIndex() == 0 )
				ep.m_pOrigin = &m_soundOrigin;

			CBaseEntity::EmitSound( m_Filter, EntIndex(), ep );
		}
		else
		{
			// FIXME: Lost the entity on restore. It might have been suppressed by the save/restore system.
			// This will probably leak the sound patch since there's no one to delete it, but the next
			// call to CSoundPatch::Update should at least remove it from the list of sound patches.
			DevWarning( "CSoundPatch::ResumeSound: Lost EHAndle on restore - destroy the sound patch in your entity's StopLoopingSounds! (%s)\n", STRING( m_iszSoundName ) );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: A new player's entered the game. See if we need to restart our sound.
//-----------------------------------------------------------------------------
void CSoundPatch::AddPlayerPost( CBasePlayer *pPlayer )
{
	if ( m_Filter.IsActive() && m_Filter.AddRecipient(pPlayer) )
	{
		// Alrighty, he's new. We need to restart our sound just to him.
		// Create a new filter just to him.
		CSingleUserRecipientFilter filter( pPlayer );

		EmitSound_t ep;
		ep.m_nChannel = m_entityChannel;
		ep.m_pSoundName = STRING(m_iszSoundName);
		ep.m_flVolume = GetVolumeForEngine();
		ep.m_SoundLevel = m_soundlevel;
		ep.m_nFlags = (SND_CHANGE_VOL | m_baseFlags);
		ep.m_nPitch = (int)m_pitch.Value();

		ep.m_hSoundScriptHash = m_hSoundScriptHash;
		ep.m_nSoundEntryVersion = m_nSoundEntryVersion;

		// only pass the position if it's coming from the world
		if( EntIndex() == 0 )
			ep.m_pOrigin = &m_soundOrigin;

		CBaseEntity::EmitSound( filter, EntIndex(), ep );
	}
}

// This is an entry in the command queue.  It's used to queue up various pitch and volume changes
// so you can define an envelope without writing timing code in an entity.  Existing queued commands
// can be deleted later if the envelope changes dynamically.
struct SoundCommand_t : public ks::pooled< SoundCommand_t >
{
	SoundCommand_t( void ) { std::memset( this, 0, sizeof(*this) ); }
	SoundCommand_t( CSoundPatch *pSound, float executeTime, soundcommands_t command, float deltaTime, float value ) : m_pPatch(pSound), m_time(executeTime), m_deltaTime(deltaTime), m_command(command), m_value(value) {}

	CSoundPatch		*m_pPatch;
	float			m_time;
	float			m_deltaTime;
	soundcommands_t	m_command;
	float			m_value;
	
	SoundCommand_t	*m_pNext;

	DECLARE_SIMPLE_DATADESC();
	private:
		static ks::fixed_pool s_Allocator;
		friend class ks::pooled< SoundCommand_t >;
};

ks::fixed_pool SoundCommand_t::s_Allocator( sizeof(SoundCommand_t), 32, ks::fixed_pool::grow::fast, "SoundCommand_t pool", alignof(SoundCommand_t) );


IMPLEMENT_REFLECT_DATAMAP_SIMPLE( SoundCommand_t )

typedef std::unique_ptr< SoundCommand_t > SOUNDCOMMANDPTR;

bool SoundCommandLessFunc( const SOUNDCOMMANDPTR &lhs, const SOUNDCOMMANDPTR &rhs )	
{ 
	// NOTE: A greater time means "less" priority
	return ( lhs->m_time > rhs->m_time );	
}


// This implements the sound controller
class CSoundControllerImp : public CSoundEnvelopeController, public CAutoGameSystemPerFrame
{
	//-----------------------------------------------------------------------------
	// internal functions, private to this file
	//-----------------------------------------------------------------------------
public:
	CSoundControllerImp( void ) : CAutoGameSystemPerFrame( "CSoundControllerImp" )
	{
	}

	void ProcessCommand( SoundCommand_t *pCmd );
	void RemoveFromList( CSoundPatch *pSound );

	virtual void	OnRestore();

	//-----------------------------------------------------------------------------
	// external interface functions (from CSoundEnvelopeController)
	//-----------------------------------------------------------------------------
public:

	// Start this sound playing, or reset if already playing with new volume/pitch
	void			Play( CSoundPatch *pSound, float volume, float pitch, float flStartTime = 0 );
	void			CommandAdd( CSoundPatch *pSound, float executeDeltaTime, soundcommands_t command, float commandTime, float commandValue );
	
	void			SystemReset( void );
	void			SystemUpdate( void );
	void			CommandClear( CSoundPatch *pSound );
	void			Shutdown( CSoundPatch *pSound );

	CSoundPatch		*SoundCreate( IRecipientFilter& filter, int nEntIndex, const char *pSoundName );

	CSoundPatch		*SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, const char *pSoundName, 
						float attenuation, float scriptVolume = 1.0f );

	CSoundPatch		*SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, const char *pSoundName, 
						float attenuation, const Vector *pSoundOrigin, float scriptVolume = 1.0f );

	CSoundPatch		*SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, const char *pSoundName, 
						soundlevel_t soundlevel );

	CSoundPatch		*SoundCreate( IRecipientFilter& filter, int nEntIndex, const EmitSound_t &es );

	void			SoundDestroy( CSoundPatch *pSound );
	void			SoundChangePitch( CSoundPatch *pSound, float pitchTarget, float deltaTime );
	void			SoundChangeVolume( CSoundPatch *pSound, float volumeTarget, float deltaTime );
	void			SoundFadeOut( CSoundPatch *pSound, float deltaTime, bool destroyOnFadeout );
	float			SoundGetPitch( CSoundPatch *pSound );
	float			SoundGetVolume( CSoundPatch *pSound );
#ifdef CLIENT_DLL
	float			SoundGetElapsedTime( CSoundPatch *pSound );
	bool			SoundIsStillPlaying( CSoundPatch *pSound );
	int				SoundGetGuid( CSoundPatch *pSound );
#endif

	string_t		SoundGetName( CSoundPatch *pSound ) { return pSound->GetName(); }
	string_t		SoundGetScriptName( CSoundPatch *pSound ) { return pSound->GetScriptName(); }
	void			SoundSetCloseCaptionDuration( CSoundPatch *pSound, float flDuration ) { pSound->SetCloseCaptionDuration(flDuration); }

	float			SoundPlayEnvelope( CSoundPatch *pSound, soundcommands_t soundCommand, envelopePoint_t *points, int numPoints );
	float			SoundPlayEnvelope( CSoundPatch *pSound, soundcommands_t soundCommand, envelopeDescription_t *envelope );

	void			CheckLoopingSoundsForPlayer( CBasePlayer *pPlayer );

	// Inserts the command into the list, sorted by time
	void			CommandInsert( SOUNDCOMMANDPTR pCommand );

#ifdef CLIENT_DLL
	// CAutoClientSystem
	virtual void Update( float frametime ) 
	{
		SystemUpdate();
	}
#else
	virtual void PreClientUpdate()
	{
		SystemUpdate();
	}
#endif

	virtual void LevelShutdownPreEntity() 
	{
		SystemReset();
	}
	
private:
	std::vector< CSoundPatch * >			m_soundList;
	std::vector< SOUNDCOMMANDPTR >		m_commandList;	// heap by SoundCommandLessFunc (ks::heap_*)
	float				m_flLastTime;
};

// Execute a command from the list
// currently only 3 commands 
// UNDONE: Add start command?
void CSoundControllerImp::ProcessCommand( SoundCommand_t *pCmd )
{
	switch( pCmd->m_command )
	{
	case SOUNDCTRL_CHANGE_VOLUME:
		pCmd->m_pPatch->ChangeVolume( pCmd->m_value, pCmd->m_deltaTime );
		break;

	case SOUNDCTRL_CHANGE_PITCH:
		pCmd->m_pPatch->ChangePitch( pCmd->m_value, pCmd->m_deltaTime );
		break;

	case SOUNDCTRL_STOP:
		pCmd->m_pPatch->Shutdown();
		break;

	case SOUNDCTRL_DESTROY:
		RemoveFromList( pCmd->m_pPatch );
		delete pCmd->m_pPatch;
		pCmd->m_pPatch = nullptr;
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Remove this sound from the sound list & shutdown (not in external interface)
// Input  : *pSound - patch to remove
//-----------------------------------------------------------------------------
void CSoundControllerImp::RemoveFromList( CSoundPatch *pSound )
{
	ks::erase_first( m_soundList, pSound );
	pSound->Shutdown();
}


//-----------------------------------------------------------------------------
// Start this sound playing, or reset if already playing with new volume/pitch
//-----------------------------------------------------------------------------
void CSoundControllerImp::Play( CSoundPatch *pSound, float volume, float pitch, float flStartTime )
{
	// reset the vars
	pSound->Reset();

	pSound->ChangeVolume( volume, 0 );
	pSound->ChangePitch( pitch, 0 );

	if ( pSound->IsPlaying() )
	{
		// remove any previous commands in the queue
		CommandClear( pSound );
	}
	else
	{
		m_soundList.push_back( pSound );
		pSound->StartSound( flStartTime );
	}
}


//-----------------------------------------------------------------------------
// Inserts the command into the list, sorted by time
//-----------------------------------------------------------------------------
void CSoundControllerImp::CommandInsert( SOUNDCOMMANDPTR pCommand )
{
	ks::heap_push( m_commandList, std::move( pCommand ), SoundCommandLessFunc );
}


//-----------------------------------------------------------------------------
// Purpose: puts a command into the queue
// Input  : *pSound - patch this command affects
//			executeDeltaTime - relative time to execute this command
//			command - command to execute (SOUNDCTRL_*)
//			commandTime - commands have 2 parameters, a time and a value
//			value - 
// Output : 	void
//-----------------------------------------------------------------------------
void CSoundControllerImp::CommandAdd( CSoundPatch *pSound, float executeDeltaTime, soundcommands_t command, float commandTime, float commandValue )
{
	CommandInsert( std::make_unique< SoundCommand_t >( pSound, g_pEffects->Time() + executeDeltaTime, command, commandTime, commandValue ) );
}

// Reset the whole system (level change, etc.)
void CSoundControllerImp::SystemReset( void )
{
	for ( int i = std::ssize( m_soundList )-1; i >=0; i-- )
	{
		CSoundPatch *pNode = m_soundList[i];
	
		// shutdown all active sounds
		pNode->Shutdown();
	}

	// clear the list
	m_soundList.clear(); m_soundList.shrink_to_fit();

	// clear the command queue
	m_commandList.clear();
}


//-----------------------------------------------------------------------------
// Purpose: Update the active sounds, dequeue any events and move the ramps
//-----------------------------------------------------------------------------
void CSoundControllerImp::SystemUpdate( void )
{
	CVProfScope _( "CSoundControllerImp::SystemUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	float time = g_pEffects->Time();
	float deltaTime = time - m_flLastTime;
	
	// handle clock resets
	if ( deltaTime < 0 )
		deltaTime = 0;

	m_flLastTime = time;

	{
		CVProfScope _( "CSoundControllerImp::SystemUpdate:processcommandlist", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
		while ( !m_commandList.empty() )
		{
			// Commands are sorted by time.
			// process any that should occur by the current time
			if ( time >= m_commandList.front()->m_time )
			{
				SOUNDCOMMANDPTR pCmd = std::move( m_commandList.front() );
				ks::heap_pop( m_commandList, SoundCommandLessFunc );
				ProcessCommand( pCmd.get() );
			}
			else
			{
				break;
			}
		}
	}

	// NOTE: Because this loop goes from the end to the beginning
	// we can fast remove inside it without breaking the indexing
	{
		CVProfScope _( "CSoundControllerImp::SystemUpdate:removesounds", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
		for ( int i = std::ssize( m_soundList )-1; i >=0; i-- )
		{
			CSoundPatch *pNode = m_soundList[i];
			if ( !pNode->Update( time, deltaTime ) )
			{
				pNode->Reset();
				ks::swap_remove( m_soundList, i );
			}
		}
	}
}

// Remove any envelope commands from the list (dynamically changing envelope)
void CSoundControllerImp::CommandClear( CSoundPatch *pSound )
{
	for ( int i = std::ssize( m_commandList )-1; i >= 0; i-- )
	{
		if ( m_commandList[ i ]->m_pPatch == pSound )
		{
			ks::heap_erase( m_commandList, i, SoundCommandLessFunc );
		}
	}
}





//-----------------------------------------------------------------------------
// Purpose: immediately stop playing this sound 
// Input  : *pSound - Patch to shut down
//-----------------------------------------------------------------------------
void CSoundControllerImp::Shutdown( CSoundPatch *pSound )
{
	if ( !pSound )
		return;

	pSound->Shutdown();
	CommandClear( pSound );
	RemoveFromList( pSound );
}

CSoundPatch *CSoundControllerImp::SoundCreate( IRecipientFilter& filter, int nEntIndex, const char *pSoundName )
{
	CSoundPatch *pSound = new CSoundPatch;

	// FIXME: This is done so we don't have to futz with the public interface
	IHandleEntity* pEnt = ( nEntIndex != -1 ) ? g_pEntityList->LookupEntityByNetworkIndex( nEntIndex ) : nullptr;
	pSound->Init( &filter, static_cast<CBaseEntity*>(pEnt), CHAN_AUTO, pSoundName, SNDLVL_NORM, nullptr );

	return pSound;
}

CSoundPatch *CSoundControllerImp::SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, 
			const char *pSoundName, float attenuation, float scriptVolume )
{
	CSoundPatch *pSound = new CSoundPatch;
	IHandleEntity* pEnt = ( nEntIndex != -1 ) ? g_pEntityList->LookupEntityByNetworkIndex( nEntIndex ) : nullptr;
	pSound->Init( &filter, static_cast<CBaseEntity*>(pEnt), channel, pSoundName, ATTN_TO_SNDLVL( attenuation ), nullptr, scriptVolume );

	return pSound;
}

CSoundPatch *CSoundControllerImp::SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, 
			const char *pSoundName, float attenuation, const Vector *pSoundOrigin, float scriptVolume )
{
	CSoundPatch *pSound = new CSoundPatch;
	IHandleEntity* pEnt = ( nEntIndex != -1 ) ? g_pEntityList->LookupEntityByNetworkIndex( nEntIndex ) : nullptr;
	pSound->Init( &filter, static_cast<CBaseEntity*>(pEnt), channel, pSoundName, ATTN_TO_SNDLVL( attenuation ), pSoundOrigin, scriptVolume );

	return pSound;
}
CSoundPatch *CSoundControllerImp::SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, 
			const char *pSoundName, soundlevel_t soundlevel )
{
	CSoundPatch *pSound = new CSoundPatch;
	IHandleEntity* pEnt = ( nEntIndex != -1 ) ? g_pEntityList->LookupEntityByNetworkIndex( nEntIndex ) : nullptr;
	pSound->Init( &filter, static_cast<CBaseEntity*>(pEnt), channel, pSoundName, soundlevel, nullptr );

	return pSound;
}


CSoundPatch *CSoundControllerImp::SoundCreate( IRecipientFilter& filter, int nEntIndex, const EmitSound_t &es )
{
	CSoundPatch *pSound = new CSoundPatch;

	// FIXME: This is done so we don't have to futz with the public interface
	IHandleEntity* pEnt = ( nEntIndex != -1 ) ? g_pEntityList->LookupEntityByNetworkIndex( nEntIndex ) : nullptr;
	pSound->Init( &filter, static_cast<CBaseEntity*>(pEnt), es.m_nChannel, es.m_pSoundName, es.m_SoundLevel, es.m_pOrigin );
	pSound->ChangeVolume( es.m_flVolume, 0 );
	pSound->ChangePitch( es.m_nPitch, 0 );

	if ( es.m_nFlags & SND_SHOULDPAUSE )
	{
		pSound->SetBaseFlags( SND_SHOULDPAUSE );
	}

	return pSound;
}

void CSoundControllerImp::SoundDestroy( CSoundPatch	*pSound )
{
	if ( !pSound )
		return;

	Shutdown( pSound );
	delete pSound;
}

void CSoundControllerImp::SoundChangePitch( CSoundPatch *pSound, float pitchTarget, float deltaTime )
{
	pSound->ChangePitch( pitchTarget, deltaTime );
}


void CSoundControllerImp::SoundChangeVolume( CSoundPatch *pSound, float volumeTarget, float deltaTime )
{
	pSound->ChangeVolume( volumeTarget, deltaTime );
}

#ifdef CLIENT_DLL
int CSoundControllerImp::SoundGetGuid( CSoundPatch *pSound )
{
	return pSound->GetGuid();
}
float CSoundControllerImp::SoundGetElapsedTime( CSoundPatch *pSound )
{
	return pSound->GetElapsedTime();
}
bool CSoundControllerImp::SoundIsStillPlaying( CSoundPatch *pSound )
{
	return pSound->IsStillPlaying();
}
#endif

float CSoundControllerImp::SoundGetPitch( CSoundPatch *pSound )
{
	return pSound->GetPitch();
}

float CSoundControllerImp::SoundGetVolume( CSoundPatch *pSound )
{
	return pSound->GetVolume();
}

void CSoundControllerImp::SoundFadeOut( CSoundPatch *pSound, float deltaTime, bool destroyOnFadeout )
{
	if ( destroyOnFadeout && (deltaTime == 0.0f) )
	{
		SoundDestroy( pSound );
		return;
	}

	pSound->FadeOut( deltaTime, destroyOnFadeout );
	if ( destroyOnFadeout )
	{
		CommandAdd( pSound, deltaTime, SOUNDCTRL_DESTROY, 0.0f, 0.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Queue a list of envelope points into a sound patch's event list
// Input  : *pSound - The sound patch to be operated on
//			soundCommand - Type of operation the envelope describes
//			*points - List of enevelope points
//			numPoints - Number of points provided
// Output : float - Returns the total duration of the envelope
//-----------------------------------------------------------------------------
float CSoundControllerImp::SoundPlayEnvelope( CSoundPatch *pSound, soundcommands_t soundCommand, envelopePoint_t *points, int numPoints )
{
	float	amplitude	= 0.0f;
	float	duration	= 0.0f;
	float	totalDuration	= 0.0f;

	contract_assert( points );

	// Clear out all previously acting commands
	CommandClear( pSound );

	// Evaluate and queue all points
	for ( int i = 0; i < numPoints; i++ )
	{
		// See if we're keeping our last amplitude for this new point
		if ( ( points[i].amplitudeMin != -1.0f ) || ( points[i].amplitudeMax != -1.0f ) )
		{
			amplitude = random->RandomFloat( points[i].amplitudeMin, points[i].amplitudeMax );
		}
		else if ( i == 0 )
		{
			// Can't do this on the first entry
			Msg( "Invalid starting amplitude value in envelope!  (Cannot be -1)\n" );
		}

		// See if we're keeping our last duration for this new point
		if ( ( points[i].durationMin != -1.0f ) || ( points[i].durationMax != -1.0f ) )
		{
			duration = random->RandomFloat( points[i].durationMin, points[i].durationMax );	
			//duration = points[i].durationMin;
		}
		else if ( i == 0 )
		{
			// Can't do this on the first entry
			Msg( "Invalid starting duration value in envelope! (Cannot be -1)\n" );
		}

		// Queue the command
		CommandAdd( pSound, totalDuration, soundCommand, duration, amplitude );

		// Tack this command's duration onto the running duration
		totalDuration += duration;
	}

	return totalDuration;
}

//-----------------------------------------------------------------------------
// Purpose: Queue a list of envelope points into a sound patch's event list
// Input  : *pSound - The sound patch to be operated on
//			soundCommand - Type of operation the envelope describes
//			*envelope - The envelope description to be queued
// Output : float - Returns the total duration of the envelope
//-----------------------------------------------------------------------------
float CSoundControllerImp::SoundPlayEnvelope( CSoundPatch *pSound, soundcommands_t soundCommand, envelopeDescription_t *envelope )
{
	return SoundPlayEnvelope( pSound, soundCommand, envelope->pPoints, envelope->nNumPoints );
}

//-----------------------------------------------------------------------------
// Purpose: Looping sounds are often started in entity spawn/activate functions. 
//			In singleplayer, the player's not ready to receive sounds then, so restart 
//			and SoundPatches that are active and have no receivers.
//-----------------------------------------------------------------------------
void CSoundControllerImp::CheckLoopingSoundsForPlayer( CBasePlayer *pPlayer )
{
	for ( int i = std::ssize( m_soundList )-1; i >=0; i-- )
	{
		CSoundPatch *pNode = m_soundList[i];
		pNode->AddPlayerPost( pPlayer );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Resumes saved soundpatches
//-----------------------------------------------------------------------------
void CSoundControllerImp::OnRestore()
{
	for ( int i = std::ssize( m_soundList )-1; i >=0; i-- )
	{
		CSoundPatch *pNode = m_soundList[i];
		if ( pNode && pNode->IsPlaying() )
		{
			pNode->ResumeSound();
		}
	}
}


//-----------------------------------------------------------------------------
// Singleton accessors
//-----------------------------------------------------------------------------
static CSoundControllerImp g_Controller;
CSoundEnvelopeController &CSoundEnvelopeController::GetController( void )
{
	return g_Controller;
}
