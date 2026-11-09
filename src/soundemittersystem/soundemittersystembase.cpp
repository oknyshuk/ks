// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.vstdlib;
#include "filesystem.h"
#include "tier2/interval.h"
#include "engine/IEngineSound.h"
#include "soundemittersystembase.h"
#include "soundchars.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "tier2/tier2.h"
#include "appframework/apptiers.h"
#include "datacache/iresourceaccesscontrol.h"

inline constexpr const char *MANIFEST_FILE = "scripts/game_sounds_manifest.txt";
inline constexpr const char *GAME_SOUNDS_HEADER_BLOCK = "scripts/game_sounds_header.txt";

static void Register_LOG_SOUNDEMITTER_SYSTEM_Tags();
LoggingChannelID_t LOG_SOUNDEMITTER_SYSTEM = LoggingSystem_RegisterLoggingChannel( "SoundEmitterSystem", Register_LOG_SOUNDEMITTER_SYSTEM_Tags, LCF_CONSOLE_ONLY, LS_MESSAGE );
void Register_LOG_SOUNDEMITTER_SYSTEM_Tags()
{
};

//-----------------------------------------------------------------------------

constexpr auto MAX_MEASURED_SOUNDENTRIES = 6124;

// Allocate sound entries in 64k blocks
ks::fixed_pool CSoundEntry::s_Allocator( sizeof(CSoundEntry), 64*1024 / sizeof( CSoundEntry ), ks::fixed_pool::grow::slow, "CSoundEntry pool", alignof(CSoundEntry) );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSoundEmitterSystemBase::CSoundEmitterSystemBase() : 
	m_nInitCount( 0 ),
	m_uManifestPlusScriptChecksum( 0 )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int	
//-----------------------------------------------------------------------------
int	 CSoundEmitterSystemBase::First() const
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : i - 
// Output : int
//-----------------------------------------------------------------------------
int CSoundEmitterSystemBase::Next( int i ) const
{
	if ( ++i >= std::ssize( m_Sounds ) )
	{
		return -1;
	}
	return i;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CSoundEmitterSystemBase::InvalidIndex() const
{
	return -1;
}

CUniformRandomStream *randomStream = &GlobalUniformRandomStream();

//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CSoundEmitterSystemBase::Connect( CreateInterfaceFn factory )
{
	if ( !AppSystemTier2Connect( factory ) )
		return false;

	if ( !g_pFullFileSystem )
	{
		Error( "The soundemittersystem system requires the filesystem to run!\n" );
		return false;
	}

	return true;
}


void CSoundEmitterSystemBase::Disconnect()
{
	AppSystemTier2Disconnect();
}


//-----------------------------------------------------------------------------
// Query interface
//-----------------------------------------------------------------------------
void *CSoundEmitterSystemBase::QueryInterface( const char *pInterfaceName )
{
	// Loading the engine DLL mounts *all* soundemitter interfaces
	CreateInterfaceFn factory = Sys_GetFactoryThis();	// This silly construction is necessary
	return factory( pInterfaceName, nullptr );				// to prevent the LTCG compiler from crashing.
}


//-----------------------------------------------------------------------------
// Purpose: Helper for checksuming script files and manifest to determine if soundname caches
//  need to be blown away.
// Input  : *crc - 
//			*filename - 
// Output : static void
//-----------------------------------------------------------------------------
static void AccumulateFileNameAndTimestampIntoChecksum( CRC32_t *crc, char const *filename )
{

	long ft = g_pFullFileSystem->GetFileTime( filename, "GAME" );
	CRC32_ProcessBuffer( crc, &ft, sizeof( ft ) );
	CRC32_ProcessBuffer( crc, filename, V_strlen( filename ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
InitReturnVal_t CSoundEmitterSystemBase::Init()
{
	++m_nInitCount;
	if ( m_nInitCount > 1 )
		return INIT_OK;

	InitReturnVal_t nRetVal = AppSystemTier1Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	bool bLoaded = LoadGameSoundManifest();
	return bLoaded ? INIT_OK : INIT_FAILED;
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSoundEmitterSystemBase::Shutdown()
{
	if ( --m_nInitCount > 0 )
		return;

	ShutdownSounds();
	AppSystemTier1Shutdown();
}


bool CSoundEmitterSystemBase::LoadGameSoundManifest()
{
	/*
	if ( m_SoundKeyValues.Count() > 0 )
	{
		Shutdown();
	}
	*/

	LoadGlobalActors();

	m_uManifestPlusScriptChecksum = 0u;

	CRC32_t crc;
	CRC32_Init( &crc );



	KeyValues *manifest = new KeyValues( MANIFEST_FILE );
	if ( g_pFullFileSystem->LoadKeyValues( *manifest, IFileSystem::TYPE_SOUNDEMITTER, MANIFEST_FILE, "GAME" ) )
	{
		AccumulateFileNameAndTimestampIntoChecksum( &crc, MANIFEST_FILE );

		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != nullptr; sub = sub->GetNextKey() )
		{
			if ( !V_stricmp( sub->GetName(), "precache_file" ) )
			{
				AccumulateFileNameAndTimestampIntoChecksum( &crc, sub->GetString() );

				// Add and always precache
				AddSoundsFromFile( sub->GetString(), false, false );
				continue;
			}
			if ( !V_stricmp( sub->GetName(), "autocache_file" ) )
			{
				AccumulateFileNameAndTimestampIntoChecksum( &crc, sub->GetString() );

				// Add and always precache and autocache
				AddSoundsFromFile( sub->GetString(), false, true );
				continue;
			}
			else if ( !V_stricmp( sub->GetName(), "preload_file" ) )
			{
				AccumulateFileNameAndTimestampIntoChecksum( &crc, sub->GetString() );

				// Add and always precache
				AddSoundsFromFile( sub->GetString(), true, false );
				continue;
			}

			Warning( "CSoundEmitterSystemBase::BaseInit:  Manifest '%s' with bogus file type '%s', expecting 'declare_file' or 'precache_file'\n", 
				MANIFEST_FILE, sub->GetName() );
		}
	}
	else
	{
		{
			Error( "Unable to load manifest file '%s'\n", MANIFEST_FILE );
		}		
	}
	manifest->deleteThis();

	CRC32_Final( &crc );

	m_uManifestPlusScriptChecksum = ( unsigned int )crc;

	// Only print total once, on server
#if !defined( CLIENT_DLL ) && !defined( FACEPOSER )
	DevMsg( 1, "CSoundEmitterSystem:  Registered %i sounds\n", (int)std::ssize( m_Sounds ) );
#endif

	// Helpful code to dump out sound entry lists if we suspect this of being out-of-sync with RTM
	//if ( 0 )
	//{
	//	FileHandle_t hSndDumpFile = NULL;

	//	hSndDumpFile = g_pFullFileSystem->Open( "sound_dump.csv", "w" );
	//	for ( int i = 0; i < m_Sounds.Count(); ++ i )
	//	{
	//		int nHash = HashSoundName( m_Sounds[ i ]->m_Name.data() );
	//		int nSlot = m_HashToSoundIndex.Find( nHash );
	//		nSlot;
	//		Assert( nSlot != m_HashToSoundIndex.InvalidIndex() );
	//		Assert( m_HashToSoundIndex[ nSlot ] == i );
	//		g_pFullFileSystem->FPrintf( hSndDumpFile, "%s,%X\n", m_Sounds[ i ]->m_Name.data(), nHash );
	//	}	

	//	g_pFullFileSystem->Close( hSndDumpFile );
	//}
	

	return true;
}


void CSoundEmitterSystemBase::ShutdownSounds()
{
	int i;
	m_SoundKeyValues.clear();
	for ( i = 0; i < std::ssize( m_Sounds ); ++i )
	{
		delete m_Sounds[ i ];
	}
	m_Sounds.clear(); m_Sounds.shrink_to_fit();

	for ( i = 0; i < std::ssize( m_SavedOverrides ) ; ++i )
	{
		delete m_SavedOverrides[ i ];
	}
	m_SavedOverrides.clear(); m_SavedOverrides.shrink_to_fit();
	m_Waves.clear();
	m_WaveIds.clear();
	m_ActorGenders.clear();
	m_HashToSoundEntry.clear();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pName - 
//-----------------------------------------------------------------------------
int	CSoundEmitterSystemBase::GetSoundIndex( const char *pName ) const
{
	// Use the hash, its faster
	HSOUNDSCRIPTHASH hash = HashSoundName( pName );
	int idx = GetSoundIndexForHash( hash );
	return idx;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSoundEmitterSystemBase::IsValidIndex( int index )
{
	return ks::is_valid_index( m_Sounds, index );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : char const
//-----------------------------------------------------------------------------
const char *CSoundEmitterSystemBase::GetSoundName( int index )
{
	if ( !IsValidIndex( index ) )
		return "";

	return m_Sounds[ index ]->m_Name.data();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CSoundEmitterSystemBase::GetSoundCount( void )
{
	return std::ssize( m_Sounds );
}

void CSoundEmitterSystemBase::EnsureAvailableSlotsForGender( SoundFile *pSoundnames, int c, gender_t gender )
{
	int i;
	if ( c <= 0 )
	{
		return;
	}

	std::vector< int > slots;

	bool needsreset = false;
	for ( i = 0; i < c; i++ )
	{
		if ( pSoundnames[ i ].gender != gender )
			continue;

		// There was at least one match for the gender
		needsreset = true;

		// This sound is unavailable
		if ( !pSoundnames[ i ].available )
			continue;

		slots.push_back( i );
	}

	if ( slots.empty() && needsreset )
	{
		// Reset all slots for the specified gender!!!
		for ( i = 0; i < c; i++ )
		{
			if ( pSoundnames[ i ].gender != gender )
				continue;

			pSoundnames[ i ].available = true;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : gender - 
//			soundnames - 
//-----------------------------------------------------------------------------
int	CSoundEmitterSystemBase::FindBestSoundForGender( SoundFile *pSoundnames, int c, gender_t gender, int &nRandomSeed )
{
	// Check for recycling of random sounds...
	EnsureAvailableSlotsForGender( pSoundnames, c, gender );

	// because this random int / index came across the network as a 6 bit uint
	// we utilize the 0 slot as "undefined", however 0 is a valid index
	// therefore the shift
	int nAdjRandomSeed = nRandomSeed - 1;

	if ( c <= 0 )
	{
		return -1;
	}

	// have we been passed a valid index
	int idx;
	if ( nAdjRandomSeed >= 0 )
	{
		// extract LSB index
		idx = nAdjRandomSeed % c;
	}
	else
	{
		// make a list of possible indices
		std::vector< int > slots;
		for ( int i = 0; i < c; i++ )
		{
			if ( pSoundnames[ i ].gender == gender &&
				( pSoundnames[ i ].available ) )
			{
				slots.push_back( i );
			}
		}

		if ( std::ssize( slots ) >= 1 )
		{

			// TODO: morasky, this should get tested at load time?
			contract_assert( std::ssize( slots ) < MAX_SOUND_SEED_VALUE );

			int nRandomIndex = randomStream->RandomInt( 0, std::ssize( slots ) - 1 );

			idx = slots[ nRandomIndex ];

			// create random MSB for full res seed
			int nMaxBitDiv = MAX_SOUND_SEED_VALUE / c;
			int nRandomMSB = randomStream->RandomInt( 0, nMaxBitDiv );
			int nRandomSum = ( nRandomMSB * c ) + idx;

			// we are using 0 = undefined
			nRandomSum += 1;
			if( nRandomSum > MAX_SOUND_SEED_VALUE )
			{
				nRandomSum -= c;
			}
			nRandomSeed = nRandomSum;
		}
		else
		{
			idx = -1;
			nRandomSeed = 0;
		}
	}

	return idx;


// 	int idx = randomStream->RandomInt( 0, c - 1 );
// 	return idx;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *soundname - 
//			params - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSoundEmitterSystemBase::GetParametersForSound( const char *soundname, CSoundParameters& params, gender_t gender, bool isbeingemitted /*= false*/ )
{
	HSOUNDSCRIPTHASH hash = HashSoundName( soundname );
	int index = GetSoundIndexForHash( hash );

	if ( index == -1 )
	{
		static ks::intern_pool soundWarnings;
		char key[ 256 ];
		V_snprintf( key, sizeof( key ), "%s:%s", soundname, params.soundname );
		if ( !soundWarnings.contains( key ) )
		{
			soundWarnings.intern( key );

			Warning( "CSoundEmitterSystemBase::GetParametersForSound:  No such sound %s\n", soundname );
		}
		return GetParametersForSoundEx( "Error", hash, params, gender, isbeingemitted );
	}

	return GetParametersForSoundEx( soundname, hash, params, gender, isbeingemitted );
}

CSoundParametersInternal *CSoundEmitterSystemBase::InternalGetParametersForSound( int index )
{
	if ( !ks::is_valid_index( m_Sounds, index ) )
	{
		contract_assert( !"CSoundEmitterSystemBase::InternalGetParametersForSound:  Bogus index" );
		return nullptr;
	}

	return &m_Sounds[ index ]->m_SoundParams;
}

static void SplitName( char const *input, int splitchar, int splitlen, char *before, int beforelen, char *after, int afterlen )
{
	char const *in = input;
	char *out = before;

	int c = 0;
	int l = 0;
	int maxl = beforelen;
	while ( *in )
	{
		if ( c == splitchar )
		{
			while ( --splitlen >= 0 )
			{
				in++;
			}

			*out = 0;
			out = after;
			maxl = afterlen;
			c++;
			continue;
		}

		if ( l >= maxl )
		{
			in++;
			c++;
			continue;
		}

		*out++ = *in++;
		l++;
		c++;
	}

	*out = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : params - 
//			*wavename - 
//			gender - 
//-----------------------------------------------------------------------------
void CSoundEmitterSystemBase::AddSoundName( CSoundParametersInternal& params, char const *wavename, gender_t gender )
{
	SoundFile e;
	e.symbol = AddWaveName( wavename );
	e.gender = gender;
	if ( gender != GENDER_NONE )
	{
		params.SetUsesGenderToken( true );
	}
	params.AddSoundName( e );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static const char *FindGenderMacro( const char *wavename, int *duration )
{
	char const *p = V_stristr( wavename, SOUNDGENDER_MACRO );
	if ( p )
	{
		*duration = SOUNDGENDER_MACRO_LENGTH;
	}

	return p;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : params - 
//			*wavename - 
//-----------------------------------------------------------------------------
void CSoundEmitterSystemBase::ExpandSoundNameMacros( CSoundParametersInternal& params, char const *wavename )
{
	int duration = SOUNDGENDER_MACRO_LENGTH;
	const char *p = FindGenderMacro( wavename, &duration );

	if ( !p )
	{
		AddSoundName( params, wavename, GENDER_NONE );
		return;
	}

	int offset = p - wavename;
	contract_assert( offset >= 0 );

	// Create a "male" and "female" version of the sound
	char before[ 256 ], after[ 256 ];
	V_memset( before, 0, sizeof( before ) );
	V_memset( after, 0, sizeof( after ) );

	SplitName( wavename, offset, duration, before, sizeof( before ), after, sizeof( after ) );

	char temp[ 256 ];
	V_snprintf( temp, sizeof( temp ), "%s%s%s", before, "male", after );
	AddSoundName( params, temp, GENDER_MALE );
	V_snprintf( temp, sizeof( temp ), "%s%s%s", before, "female", after );
	AddSoundName( params, temp, GENDER_FEMALE );

	// Add the conversion entry with the gender tags still in it
	SoundFile e;
	e.symbol = AddWaveName( wavename );
	e.gender = GENDER_NONE;
	params.AddConvertedName( e );
}

void CSoundEmitterSystemBase::GenderExpandString( gender_t gender, char const *in, char *out, int maxlen )
{
	// Assume the worst
	V_strncpy( out, in, maxlen );

	int duration = SOUNDGENDER_MACRO_LENGTH;
	const char *p = FindGenderMacro( in, &duration );
	if ( !p )
	{
		return;
	}

	// Look up actor gender
	if ( gender == GENDER_NONE )
	{
		return;
	}

	int offset = p - in;
	contract_assert( offset >= 0 );

	// Create a "male" and "female" version of the sound
	char before[ 256 ], after[ 256 ];
	V_memset( before, 0, sizeof( before ) );
	V_memset( after, 0, sizeof( after ) );

	SplitName( in, offset, duration, before, sizeof( before ), after, sizeof( after ) );

	switch ( gender )
	{
	default:
	case GENDER_NONE:
		{
			contract_assert( !"CSoundEmitterSystemBase::GenderExpandString:  expecting MALE or FEMALE!" );
		}
		break;
	case GENDER_MALE:
		{
			V_snprintf( out, maxlen, "%s%s%s", before, "male", after );
		}
		break;
	case GENDER_FEMALE:
		{
			V_snprintf( out, maxlen, "%s%s%s", before, "female", after );
		}
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actorname - 
//			*in - 
//			*out - 
//			maxlen - 
//-----------------------------------------------------------------------------
void CSoundEmitterSystemBase::GenderExpandString( char const *actormodel, char const *in, char *out, int maxlen )
{
	gender_t gender = GetActorGender( actormodel );
	GenderExpandString( gender, in, out, maxlen );
}

void CSoundEmitterSystemBase::LoadGlobalActors()
{
	// Now load the global actor list from the scripts/globalactors.txt file
	KeyValues *allActors = nullptr;
	
	allActors = new KeyValues( "allactors" );
	if ( allActors->LoadFromFile( g_pFullFileSystem, "scripts/global_actors.txt", nullptr ) )
	{
		KeyValues *pvkActor;
		for ( pvkActor = allActors->GetFirstSubKey(); pvkActor != nullptr; pvkActor = pvkActor->GetNextKey() )
		{
			if ( !m_ActorGenders.contains( pvkActor->GetName() ) )
			{
				// the actor table holds at most 254 entries
				if ( m_ActorGenders.size() + 1 == 0xFF )
				{
					Warning( "Exceeded max number of actors in scripts/global_actors.txt\n" );
					break;
				}

				gender_t gender = GENDER_NONE;
				if ( !V_stricmp( pvkActor->GetString(), "male" ) )
				{
					gender = GENDER_MALE;
				}
				else if (!V_stricmp( pvkActor->GetString(), "female" ) )
				{
					gender = GENDER_FEMALE;
				}
				m_ActorGenders.emplace( pvkActor->GetName(), gender );
			}
		}
	}
	allActors->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actorname - 
// Output : gender_t
//-----------------------------------------------------------------------------
gender_t CSoundEmitterSystemBase::GetActorGender( char const *actormodel )
{
	char actor[ 256 ];
	actor[0] = 0;
	if ( actormodel )
	{
		V_FileBase( actormodel, actor, sizeof( actor ) );
	}

	auto idx = m_ActorGenders.find( actor );
	if ( idx == m_ActorGenders.end() )
		return GENDER_NONE;

	return idx->second;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *soundname - 
//			params - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSoundEmitterSystemBase::InitSoundInternalParameters( const char *soundname, KeyValues *kv, CSoundParametersInternal& params )
{
	// for special case soundentry version error handling
	const char *pSoundEntryVersionValueStr = kv->GetString( "soundentry_version", "1" );
	int nSoundEntryVersion = 1;
	if( pSoundEntryVersionValueStr && pSoundEntryVersionValueStr[0] )
	{
		nSoundEntryVersion = V_atoi( pSoundEntryVersionValueStr );
	}
	bool bEntryNumHasErrored = false;

	KeyValues *pKey = kv->GetFirstSubKey();
	while ( pKey )
	{
		if ( !V_strcasecmp( pKey->GetName(), "channel" ) )
		{
			params.ChannelFromString( pKey->GetString() );
		}
		else if ( !V_strcasecmp( pKey->GetName(), "volume" ) )
		{
			params.VolumeFromString( pKey->GetString() );
		}
		else if ( !V_strcasecmp( pKey->GetName(), "pitch" ) )
		{
			params.PitchFromString( pKey->GetString() );
		}
		else if ( !V_strcasecmp( pKey->GetName(), "wave" ) )
		{
			ExpandSoundNameMacros( params, pKey->GetString() );
		}
		else if ( !V_strcasecmp( pKey->GetName(), "rndwave" ) )
		{
			KeyValues *pWaves = pKey->GetFirstSubKey();
			while ( pWaves )
			{
				if( params.NumSoundNames() >= MAX_SOUND_RNDWAVE_NUM && 
					nSoundEntryVersion > 1 )
				{
					if( !bEntryNumHasErrored )
					{
						contract_assert( params.NumSoundNames() >= MAX_SOUND_RNDWAVE_NUM );
						Log_Warning( LOG_SOUNDEMITTER_SYSTEM, "Error: SoundEmitterSystemBase: %s attempting to load too many rndwave soundfiles!\n", soundname );
					}
					bEntryNumHasErrored	= true;
				}
				else
				{
					ExpandSoundNameMacros( params, pWaves->GetString() );
				}

				pWaves = pWaves->GetNextKey();
			}
		}
		else if ( !V_strcasecmp( pKey->GetName(), "attenuation" ) || !V_strcasecmp( pKey->GetName(), "CompatibilityAttenuation" ) )
		{
			if ( params.GetSoundLevel().start != SNDLVL_NORM || params.GetSoundLevel().range != 0 )
			{
				DevMsg( "CSoundEmitterSystemBase::GetParametersForSound:  sound %s has multiple attenuation, CompatabilityAttenuation, and/or soundlevel entries.\n", soundname );
			}

			if ( !V_strncasecmp( pKey->GetString(), "SNDLVL_", std::strlen( "SNDLVL_" ) ) )
			{
				DevMsg( "CSoundEmitterSystemBase::GetParametersForSound:  sound %s has \"attenuation\" with %s value!\n",
					soundname, pKey->GetString() );
			}

			if ( !V_strncasecmp( pKey->GetString(), "ATTN_", std::strlen( "ATTN_" ) ) )
			{
				params.SetSoundLevel( ATTN_TO_SNDLVL( TranslateAttenuation( pKey->GetString() ) ) );
			}
			else
			{
				interval_t interval;
				interval = ReadInterval( pKey->GetString() );

				// Translate from attenuation to soundlevel
				float start = interval.start;
				float end	= interval.start + interval.range;

				params.SetSoundLevel( ATTN_TO_SNDLVL( start ), ATTN_TO_SNDLVL( end ) - ATTN_TO_SNDLVL( start ) );
			}

			// Goldsrc compatibility mode.. feed the sndlevel value through the sound engine interface in such a way
			// that it can reconstruct the original sndlevel value and flag the sound as using Goldsrc attenuation.
			bool bCompatibilityAttenuation = !V_strcasecmp( pKey->GetName(), "CompatibilityAttenuation" );
			if ( bCompatibilityAttenuation )
			{
				if ( params.GetSoundLevel().range != 0 )
				{
					Warning( "CompatibilityAttenuation for sound %s must have same start and end values.\n", soundname );
				}

				params.SetSoundLevel( SNDLEVEL_TO_COMPATIBILITY_MODE( params.GetSoundLevel().start ) );
			}
		}
		else if ( !V_strcasecmp( pKey->GetName(), "soundlevel" ) || !V_strcasecmp( pKey->GetName(), "CompatibilitySoundlevel" ) )
		{
			if ( params.GetSoundLevel().start != SNDLVL_NORM || params.GetSoundLevel().range != 0 )
			{
				DevMsg( "CSoundEmitterSystemBase::GetParametersForSound:  sound %s has multiple attenuation, CompatabilityAttenuation, and/or soundlevel entries.\n", soundname );
			}

			if ( !V_strncasecmp( pKey->GetString(), "ATTN_", std::strlen( "ATTN_" ) ) )
			{
				DevMsg( "CSoundEmitterSystemBase::GetParametersForSound:  sound %s has \"soundlevel\" with %s value!\n",
					soundname, pKey->GetString() );
			}

			params.SoundLevelFromString( pKey->GetString() );

			// Goldsrc compatibility mode.. feed the sndlevel value through the sound engine interface in such a way
			// that it can reconstruct the original sndlevel value and flag the sound as using Goldsrc attenuation.
			bool bCompatibilityAttenuation = !V_strcasecmp( pKey->GetName(), "CompatibilitySoundlevel" );
			if ( bCompatibilityAttenuation )
			{
				if ( params.GetSoundLevel().range != 0 )
				{
					Warning( "CompatibilitySoundlevel for sound %s must have same start and end values.\n", soundname );
				}

				params.SetSoundLevel( SNDLEVEL_TO_COMPATIBILITY_MODE( params.GetSoundLevel().start ) );
			}
		}
		else if ( !V_strcasecmp( pKey->GetName(), "play_to_owner_only" ) )
		{
			params.SetOnlyPlayToOwner( pKey->GetBool() );
		}
		else if ( !V_strcasecmp( pKey->GetName(), "delay_msec" ) )
		{
			// Don't allow negative delay
			params.SetDelayMsec( std::max( 0, pKey->GetInt() ) );

		}
		else if ( !V_strcasecmp( pKey->GetName(), "soundentry_version" ) )
		{
			params.SetSoundEntryVersion( pKey->GetInt() );
		}
		else if ( !V_strcasecmp( pKey->GetName(), "operator_stacks" ) )
		{
			params.SetOperatorsKV( pKey );
		}
		else if (!V_strcasecmp(pKey->GetName(), "hrtf_follow"))
		{
			params.SetHRTFFollowEntity(pKey->GetBool());
		}
		else if (!V_strcasecmp(pKey->GetName(), "hrtf_bilinear"))
		{
			params.SetHRTFBilinear(pKey->GetBool());
		}


		pKey = pKey->GetNextKey();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *soundname - 
// Output : char const
//-----------------------------------------------------------------------------
const char *CSoundEmitterSystemBase::GetWavFileForSound( const char *soundname, char const *actormodel )
{
	gender_t gender = GetActorGender( actormodel );
	return GetWavFileForSound( soundname, gender );
}

const char *CSoundEmitterSystemBase::GetWavFileForSound( const char *soundname, gender_t gender )
{
	CSoundParameters params;
	if ( !GetParametersForSound( soundname, params, gender ) )
	{
		return soundname;
	}

	if ( !params.soundname[ 0 ] )
	{
		return soundname;
	}

	static char outsound[ 512 ];
	V_strncpy( outsound, params.soundname, sizeof( outsound ) );
	return outsound;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *soundname - 
// Output : soundlevel_t
//-----------------------------------------------------------------------------
soundlevel_t CSoundEmitterSystemBase::LookupSoundLevel( const char *soundname )
{
	CSoundParameters params;
	if ( !GetParametersForSound( soundname, params, GENDER_NONE ) )
	{
		return SNDLVL_NORM;
	}

	return params.soundlevel;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//-----------------------------------------------------------------------------
void CSoundEmitterSystemBase::AddSoundsFromFile( const char *filename, bool bPreload, bool bAutoCache, bool bIsOverride /*=false*/ )
{
	CSoundScriptFile sf;
	sf.hFilename = g_pFullFileSystem->FindOrAddFileName( filename );
	sf.dirty = false;

	int scriptindex = std::ssize( m_SoundKeyValues ); m_SoundKeyValues.push_back( sf );

	int replaceCount = 0;
	int newOverrideCount = 0;
	int duplicatedReplacements = 0;

	// Open the soundscape data file, and abort if we can't
	KeyValues *kv = new KeyValues( "" );
	if ( g_pFullFileSystem->LoadKeyValues( *kv, IFileSystem::TYPE_SOUNDEMITTER, filename, "GAME" ) )
	{
		// parse out all of the top level sections and save their names
		KeyValues *pKeys = kv;
		while ( pKeys )
		{
			if ( pKeys->GetFirstSubKey() )
			{
				if ( std::ssize( m_Sounds ) + 1 == -1 )
				{
					Warning( "Exceeded maximum number of sound emitter entries\n" );
					break;
				}

				CSoundEntry *pEntry;

				{
					pEntry = new CSoundEntry;
				}
			
				const char *pName = pKeys->GetName();
				if ( !V_strlen( pName ) )
				{
					Error( "Syntax Error! Empty named KV block in %s\n", filename );
				}

				pEntry->m_Name = ks::global_symbols().intern( pName );
				pEntry->m_bRemoved			= false;
				pEntry->m_nScriptFileIndex	= scriptindex;
				pEntry->m_bIsOverride		= bIsOverride;

				HSOUNDSCRIPTHASH hash = HashSoundName( pEntry->m_Name.data() );

				if ( bIsOverride )
				{
					++newOverrideCount;
				}

				bool add = true;

				int lookup = GetSoundIndexForHash( hash );
				if ( lookup != -1 )
				{
					add = false;
					if ( bIsOverride )
					{

						// Store off the old sound if it's not already an "override" from another file!!!
						// Otherwise, just whack it again!!!
						if ( !m_Sounds[ lookup ]->m_bIsOverride )
						{
							m_SavedOverrides.push_back( m_Sounds[ lookup ] );
						}
						else
						{
							delete m_Sounds[ lookup ];
							++duplicatedReplacements;
						}

						InitSoundInternalParameters( pKeys->GetName(), pKeys, pEntry->m_SoundParams );
						pEntry->m_SoundParams.SetShouldPreload( bPreload ); // this gets handled by game code after initting.
						pEntry->m_SoundParams.SetShouldAutoCache( bAutoCache ); // this gets handled by game code after initting.

						m_Sounds[ lookup ] = pEntry;

						++replaceCount;
					}
					else
					{
						delete pEntry;
					 //	DevMsg( "CSoundEmitterSystem::AddSoundsFromFile(%s):  Entry %s duplicated, skipping\n", filename, pKeys->GetName() );
					}
				}
				
				if ( add )
				{

					InitSoundInternalParameters( pKeys->GetName(), pKeys, pEntry->m_SoundParams );
					pEntry->m_SoundParams.SetShouldPreload( bPreload ); // this gets handled by game code after initting.
					pEntry->m_SoundParams.SetShouldAutoCache( bAutoCache ); // this gets handled by game code after initting.

					int idx = std::ssize( m_Sounds ); m_Sounds.push_back( pEntry );
					AddHash( pEntry->m_Name.data(), idx );
				}
			}
			pKeys = pKeys->GetNextKey();
		}

		kv->deleteThis();
	}
	else
	{
		if ( !bIsOverride )
		{
			Warning( "CSoundEmitterSystem::AddSoundsFromFile:  No such file %s\n", filename );
		}

		// Discard
		m_SoundKeyValues.erase( m_SoundKeyValues.begin() + scriptindex );

		kv->deleteThis();

		return;
	}

	
	if ( bIsOverride )
	{
		Warning( "SoundEmitter:  adding map sound overrides from %s [%i total, %i replacements, %i duplicated replacements]\n", 
			filename,
			newOverrideCount,
			replaceCount,
			duplicatedReplacements );
	}

	contract_assert( scriptindex >= 0 );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CSoundEmitterSystemBase::CheckForMissingWavFiles( bool verbose )
{
	int missing = 0;

	int c = GetSoundCount();
	int i;
	char testfile[ 512 ];

	for ( i = 0; i < c; i++ )
	{
		CSoundParametersInternal *internal = InternalGetParametersForSound( i );
		if ( !internal )
		{
			contract_assert( 0 );
			continue;
		}

		int waveCount = internal->NumSoundNames();
		for ( int wave = 0; wave < waveCount; wave++ )
		{
			const char *name = GetWaveName( internal->GetSoundNames()[ wave ].symbol );
			if ( !name || !name[ 0 ] )
			{
				contract_assert( 0 );
				continue;
			}

			// Skip ! sentence stuff
			if ( name[0] == CHAR_SENTENCE )
				continue;
			V_snprintf( testfile, sizeof( testfile ), "sound/%s", PSkipSoundChars( name ) );
			if ( g_pFullFileSystem->FileExists( testfile ) )
				continue;

			internal->SetHadMissingWaveFiles( true );

			++missing;

			if ( verbose )
			{
				DevMsg( "Sound %s references missing file %s\n", GetSoundName( i ), name );
			}
		}
	}

	return missing;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *key - 
// Output : float
//-----------------------------------------------------------------------------
float CSoundEmitterSystemBase::TranslateAttenuation( const char *key )
{
	if ( !key )
	{
		contract_assert( 0 );
		return ATTN_NORM;
	}

	if ( !V_strcasecmp( key, "ATTN_NONE" ) )
		return ATTN_NONE;

	if ( !V_strcasecmp( key, "ATTN_NORM" ) )
		return ATTN_NORM;

	if ( !V_strcasecmp( key, "ATTN_IDLE" ) )
		return ATTN_IDLE;

	if ( !V_strcasecmp( key, "ATTN_STATIC" ) )
		return ATTN_STATIC;

	if ( !V_strcasecmp( key, "ATTN_RICOCHET" ) )
		return ATTN_RICOCHET;

	if ( !V_strcasecmp( key, "ATTN_GUNFIRE" ) )
		return ATTN_GUNFIRE;

	DevMsg( "CSoundEmitterSystem:  Unknown attenuation key %s\n", key );

	return ATTN_NORM;
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *key - 
// Output : soundlevel_t
//-----------------------------------------------------------------------------
soundlevel_t CSoundEmitterSystemBase::TranslateSoundLevel( const char *key )
{
	return TextToSoundLevel( key );
}

//-----------------------------------------------------------------------------
// Purpose: Convert "chan_xxx" into integer value for channel
// Input  : *name - 
// Output : static int
//-----------------------------------------------------------------------------
int CSoundEmitterSystemBase::TranslateChannel( const char *name )
{
	return TextToChannel( name );
}

const char *CSoundEmitterSystemBase::GetSourceFileForSound( int index ) const
{
	if ( index < 0 || index >= (int)std::ssize( m_Sounds ) )
	{
		contract_assert( 0 );
		return "";
	}

	CSoundEntry const *entry = m_Sounds[ index ];
	int scriptindex = entry->m_nScriptFileIndex;
	if ( scriptindex < 0 || scriptindex >= std::ssize( m_SoundKeyValues ) )
	{
		contract_assert( 0 );
		return "";
	}
	static char fn[ 512 ];
	if ( g_pFullFileSystem->String( m_SoundKeyValues[ scriptindex ].hFilename, fn, sizeof( fn ) ))
	{
		return fn;
	}
	contract_assert( 0 );
	return "";
}

const char *CSoundEmitterSystemBase::GetWaveName( WaveNameId_t sym )
{
	return sym < m_Waves.size() ? m_Waves[ sym ] : "";
}

int	CSoundEmitterSystemBase::FindSoundScript( const char *name ) const
{
	int i, c;

	FileNameHandle_t hFilename = g_pFullFileSystem->FindFileName( name );
	if ( hFilename )
	{
		// First, make sure it's known
		c = std::ssize( m_SoundKeyValues );
		for ( i = 0; i < c ; i++ )
		{
			if ( m_SoundKeyValues[ i ].hFilename == hFilename )
			{
				return i;
			}
		}
	}

	return -1;
}

bool CSoundEmitterSystemBase::AddSound( const char *soundname, const char *scriptfile, const CSoundParametersInternal& params )
{
	int idx = GetSoundIndex( soundname );


	int i = FindSoundScript( scriptfile );
	if ( i == -1 )
	{
		Warning( "CSoundEmitterSystemBase::AddSound( '%s', '%s', ... ), script file not list in manifest '%s'\n",
			soundname, scriptfile, MANIFEST_FILE );
		return false;
	}


	// More like an update...
	if ( IsValidIndex( idx ) )
	{
		CSoundEntry *entry = m_Sounds[ idx ];

		entry->m_bRemoved				= false;
		entry->m_nScriptFileIndex		= i;
		entry->m_SoundParams.CopyFrom( params );

		m_SoundKeyValues[ i ].dirty = true;

		return true;
	}

	CSoundEntry *pEntry = new CSoundEntry;
	pEntry->m_Name = ks::global_symbols().intern( soundname );
	pEntry->m_bRemoved			= false;
	pEntry->m_nScriptFileIndex	= i;
	pEntry->m_SoundParams.CopyFrom( params );

	idx = std::ssize( m_Sounds ); m_Sounds.push_back( pEntry );
	AddHash( pEntry->m_Name.data(), idx );

	m_SoundKeyValues[ i ].dirty = true;

	return true;
}

void CSoundEmitterSystemBase::RemoveSound( const char *soundname )
{
	int idx = GetSoundIndex( soundname );
	if ( !IsValidIndex( idx ) )
	{
		Warning( "Can't remove %s, no such sound!\n", soundname );
		return;
	}

	m_Sounds[ idx ]->m_bRemoved = true;

	// Mark script as dirty
	int scriptindex = m_Sounds[ idx ]->m_nScriptFileIndex;
	if ( scriptindex < 0 || scriptindex >= std::ssize( m_SoundKeyValues ) )
	{
		contract_assert( 0 );
		return;
	}

	m_SoundKeyValues[ scriptindex ].dirty = true;
}

void CSoundEmitterSystemBase::MoveSound( const char *soundname, const char *newscript )
{
	int idx = GetSoundIndex( soundname );
	if ( !IsValidIndex( idx ) )
	{
		Warning( "Can't move '%s', no such sound!\n", soundname );
		return;
	}

	int oldscriptindex = m_Sounds[ idx ]->m_nScriptFileIndex;
	if ( oldscriptindex < 0 || oldscriptindex >= std::ssize( m_SoundKeyValues ) )
	{
		contract_assert( 0 );
		return;
	}

	int newscriptindex = FindSoundScript( newscript );
	if ( newscriptindex == -1 )
	{
		Warning( "CSoundEmitterSystemBase::MoveSound( '%s', '%s' ), script file not list in manifest '%s'\n",
			soundname, newscript, MANIFEST_FILE );
		return;
	}

	// No actual change
	if ( oldscriptindex == newscriptindex )
	{
		return;
	}

	// Move it
	m_Sounds[ idx ]->m_nScriptFileIndex = newscriptindex;

	// Mark both scripts as dirty
	m_SoundKeyValues[ oldscriptindex ].dirty = true;
	m_SoundKeyValues[ newscriptindex ].dirty = true;
}

int CSoundEmitterSystemBase::GetNumSoundScripts() const
{
	return std::ssize( m_SoundKeyValues );
}

const char *CSoundEmitterSystemBase::GetSoundScriptName( int index ) const
{
	if ( index < 0 || index >= std::ssize( m_SoundKeyValues ) )
		return nullptr;

	static char fn[ 512 ];
	if ( g_pFullFileSystem->String( m_SoundKeyValues[ index ].hFilename, fn, sizeof( fn ) ) )
	{
		return fn;
	}
	return "";
}

bool CSoundEmitterSystemBase::IsSoundScriptDirty( int index ) const
{
	if ( index < 0 || index >= std::ssize( m_SoundKeyValues ) )
		return false;

	return m_SoundKeyValues[ index ].dirty;
}

void CSoundEmitterSystemBase::SaveChangesToSoundScript( int scriptindex )
{
	const char *outfile = GetSoundScriptName( scriptindex );
	if ( !outfile )
	{
		Msg( "CSoundEmitterSystemBase::SaveChangesToSoundScript:  No script file for index %i\n", scriptindex );
		return;
	}

	if ( g_pFullFileSystem->FileExists( outfile ) &&
		 !g_pFullFileSystem->IsFileWritable( outfile ) )
	{
		Warning( "%s is not writable, can't save data to file\n", outfile );
		return;
	}

	ks::text_writer buf;

	// FIXME:  Write sound script header
	if ( g_pFullFileSystem->FileExists( GAME_SOUNDS_HEADER_BLOCK ) )
	{
		FileHandle_t header = g_pFullFileSystem->Open( GAME_SOUNDS_HEADER_BLOCK, "rb", nullptr );
		if ( header != FILESYSTEM_INVALID_HANDLE )
		{
			int len = g_pFullFileSystem->Size( header );
			
			unsigned char *data = new unsigned char[ len + 1 ];
			V_memset( data, 0, len + 1 );
			
			g_pFullFileSystem->Read( data, len, header );
			g_pFullFileSystem->Close( header );

			data[ len ] = 0;

			char *p = (char *)data;
			while ( *p )
			{
				if ( *p != '\r' )
				{
					buf.put( *p );
				}
				++p;
			}

			delete[] data;
		}

		buf.printf( "\n" );
	}


	int c = GetSoundCount();
	for ( int i = 0; i < c; i++ )
	{
		if ( V_stricmp( outfile, GetSourceFileForSound( i ) ) )
			continue;

		// It's marked for deletion, just skip it
		if ( m_Sounds[ i ]->m_bRemoved )
			continue;

		CSoundParametersInternal *p = InternalGetParametersForSound( i );
		if ( !p )
			continue;
		
		buf.printf( "\"%s\"\n{\n", GetSoundName( i ) );

		buf.printf( "\t\"channel\"\t\t\"%s\"\n", p->ChannelToString() );
		buf.printf( "\t\"volume\"\t\t\"%s\"\n", p->VolumeToString() );
		buf.printf( "\t\"pitch\"\t\t\t\"%s\"\n", p->PitchToString() );
		buf.printf( "\n" );
		buf.printf( "\t\"soundlevel\"\t\"%s\"\n", p->SoundLevelToString() );

		if ( p->OnlyPlayToOwner() )
		{
			buf.printf( "\t\"play_to_owner_only\"\t\"1\"\n" );
		}

		if ( p->GetDelayMsec() != 0 )
		{
			buf.printf( "\t\"delay_msec\"\t\"%i\"\n", p->GetDelayMsec() );
		}

		int totalCount = 0;

		int waveCount = p->NumSoundNames();
		int convertedCount = p->NumConvertedNames();

		totalCount = ( waveCount - 2 * convertedCount ) + convertedCount;

		if  ( totalCount > 0 )
		{
			buf.printf( "\n" );

			if ( waveCount == 1 )
			{
				contract_assert( p->GetSoundNames()[ 0 ].gender == GENDER_NONE );
				buf.printf( "\t\"wave\"\t\t\t\"%s\"\n", GetWaveName( p->GetSoundNames()[ 0 ].symbol ) );
			}
			else if ( convertedCount == 1 )
			{
				contract_assert( p->GetConvertedNames()[ 0 ].gender == GENDER_NONE );
				buf.printf( "\t\"wave\"\t\t\t\"%s\"\n", GetWaveName( p->GetConvertedNames()[ 0 ].symbol ) );
			}
			else
			{
				buf.printf( "\t\"rndwave\"\n" );
				buf.printf( "\t{\n" );

				int wave;
				for ( wave = 0; wave < waveCount; wave++ )
				{
					// Skip macro-expanded names
					if ( p->GetSoundNames()[ wave ].gender != GENDER_NONE )
						continue;

					buf.printf( "\t\t\"wave\"\t\"%s\"\n", GetWaveName( p->GetSoundNames()[ wave ].symbol ) );
				}
				for ( wave = 0; wave < convertedCount; wave++ )
				{
					buf.printf( "\t\t\"wave\"\t\"%s\"\n", GetWaveName( p->GetConvertedNames()[ wave ].symbol ) );
				}

				buf.printf( "\t}\n" );
			}

		}

		buf.printf( "}\n" );

		if ( i != c - 1 )
		{
			buf.printf( "\n" );
		}
	}

	// Write it out baby
	FileHandle_t fh = g_pFullFileSystem->Open( outfile, "wt" );
	if (fh)
	{
		g_pFullFileSystem->Write( buf.c_str(), buf.size(), fh );
		g_pFullFileSystem->Close(fh);

		// Changed saved successfully
		m_SoundKeyValues[ scriptindex ].dirty = false;
	}
	else
	{
		Warning( "SceneManager_SaveSoundsToScriptFile:  Unable to write file %s!!!\n", outfile );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : WaveNameId_t
//-----------------------------------------------------------------------------
WaveNameId_t CSoundEmitterSystemBase::AddWaveName( const char *name )
{
	if ( !name )
		return WAVENAME_INVALID;

	if ( auto it = m_WaveIds.find( std::string_view( name ) ); it != m_WaveIds.end() )
		return it->second;

	contract_assert( m_Waves.size() < WAVENAME_INVALID );
	auto it = m_WaveIds.emplace( name, (WaveNameId_t)m_Waves.size() ).first;
	m_Waves.push_back( it->first.c_str() );
	return it->second;
}

void CSoundEmitterSystemBase::RenameSound( const char *soundname, const char *newname )
{
	// Same name?
	if ( !V_stricmp( soundname, newname ) )
	{
		return;
	}

	HSOUNDSCRIPTHASH oldHash = HashSoundName( soundname );
	HSOUNDSCRIPTHASH newHash = HashSoundName( newname );

	int index = GetSoundIndexForHash( oldHash );
	if ( !IsValidIndex( index ) )
	{
		Msg( "Can't rename %s, no such sound\n", soundname );
		return;
	}

	int check = GetSoundIndexForHash( newHash );
	if ( IsValidIndex( check ) )
	{
		Msg( "Can't rename %s to %s, new name already in list\n", soundname, newname );
		return;
	}


	// Copy out old entry
	CSoundEntry *pEntry = m_Sounds[ index ];
	pEntry->m_Name = ks::global_symbols().intern( newname );
	RemoveHash( soundname );
	AddHash( pEntry->m_Name.data(), index );

	// Mark associated script as dirty
	m_SoundKeyValues[ pEntry->m_nScriptFileIndex ].dirty = true;
}

void CSoundEmitterSystemBase::UpdateSoundParameters( const char *soundname, const CSoundParametersInternal& params )
{
	int idx = GetSoundIndex( soundname );
	if ( !IsValidIndex( idx ) )
	{
		Msg( "Can't UpdateSoundParameters %s, no such sound\n", soundname );
		return;
	}

	CSoundEntry *entry = m_Sounds[ idx ];

	if ( entry->m_SoundParams == params )
	{
		// No changes
		return;
	}

	// Update parameters
	entry->m_SoundParams.CopyFrom( params );
	// Set dirty flag
	m_SoundKeyValues[ entry->m_nScriptFileIndex ].dirty = true;
}

bool CSoundEmitterSystemBase::IsUsingGenderToken( char const *soundname )
{
	HSOUNDSCRIPTHASH hash = HashSoundName( soundname );
	int soundindex = GetSoundIndexForHash( hash );

	// Look up the sound level from the soundemitter system
	CSoundParametersInternal *params = InternalGetParametersForSound( soundindex );
	if ( !params )
		return false;

	return params->UsesGenderToken();
}

unsigned int CSoundEmitterSystemBase::GetManifestFileTimeChecksum()
{
	return m_uManifestPlusScriptChecksum;
}

bool CSoundEmitterSystemBase::GetParametersForSoundEx( const char *soundname, HSOUNDSCRIPTHASH& handle, CSoundParameters& params, gender_t gender, bool isbeingemitted /*= false*/ )
{
	if ( g_pResourceAccessControl )
	{
		if ( !g_pResourceAccessControl->IsAccessAllowed( RESOURCE_GAMESOUND, soundname ) )
			return false;
	}

	if ( handle == SOUNDEMITTER_INVALID_HASH )
	{
		handle = HashSoundName( soundname );
	}

	int index = GetSoundIndexForHash( handle );

	CSoundParametersInternal *internal = InternalGetParametersForSound( index );

	if ( !internal )
	{
		contract_assert( 0 );
		Warning( "CSoundEmitterSystemBase::GetParametersForSound:  No such sound %s\n", soundname );

		HSOUNDSCRIPTHASH hash = HashSoundName( "Error" );
		int index = GetSoundIndexForHash( hash );
		internal = InternalGetParametersForSound( index );
		if ( !internal )
			return false;
	}

	int nNumberOfSoundNames = internal->NumSoundNames();

	params.channel = internal->GetChannel();
	params.volume = internal->GetVolume().Random();
	params.pitch = internal->GetPitch().Random();
	params.pitchlow = internal->GetPitch().start;
	params.pitchhigh = params.pitchlow + internal->GetPitch().range;
	params.delay_msec = internal->GetDelayMsec();
	params.count = nNumberOfSoundNames;
	params.soundname[ 0 ] = 0;

	params.m_nSoundEntryVersion = (int)internal->GetSoundEntryVersion();
	params.m_hSoundScriptHash = handle;
	params.m_pOperatorsKV = internal->GetOperatorsKV();

	int bestIndex = FindBestSoundForGender( internal->GetSoundNames(), nNumberOfSoundNames, gender,  params.m_nRandomSeed );

	if ( bestIndex >= 0 )
	{
		V_strncpy( params.soundname, GetWaveName( internal->GetSoundNames()[ bestIndex ].symbol), sizeof( params.soundname ) );

		// If we are actually emitting the sound, mark it as not available...
		if ( isbeingemitted )
		{
			internal->GetSoundNames()[ bestIndex ].available = 0;
		}
	}
	params.soundlevel = (soundlevel_t)(int)internal->GetSoundLevel().Random();
	params.play_to_owner_only = internal->OnlyPlayToOwner();
	params.m_bHRTFBilinear = internal->HasHRTFBilinear();
	params.m_bHRTFFollowEntity = internal->HasHRTFFollowEntity();

	if ( !params.soundname[ 0 ] )
	{
		DevMsg( "CSoundEmitterSystemBase::GetParametersForSound:  sound %s has no wave or rndwave key!\n", soundname );
		return false;
	}

	if ( internal->HadMissingWaveFiles() &&
		params.soundname[ 0 ] != CHAR_SENTENCE )
	{
		char testfile[ 256 ];
		V_snprintf( testfile, sizeof( testfile ), "sound/%s", PSkipSoundChars( params.soundname ) );
		if ( !g_pFullFileSystem->FileExists( testfile ) )
		{
			// Prevent repetitive spew...
			static ks::intern_pool soundWarnings;
			char key[ 256 ];
			V_snprintf( key, sizeof( key ), "%s:%s", soundname, params.soundname );
			if ( !soundWarnings.contains( key ) )
			{
				soundWarnings.intern( key );
			
				DevMsg( "CSoundEmitterSystemBase::GetParametersForSound:  sound '%s' references wave '%s' which doesn't exist on disk!\n", 
					soundname,
					params.soundname );
			}
			return false;
		}
	}

	return true;
}

soundlevel_t CSoundEmitterSystemBase::LookupSoundLevelByHandle( char const *soundname, HSOUNDSCRIPTHASH& handle )
{
	if ( handle == SOUNDEMITTER_INVALID_HASH )
	{
		handle = HashSoundName( soundname );
	}
	int index = GetSoundIndexForHash( handle );

	CSoundParametersInternal *internal = InternalGetParametersForSound( index );
	if ( !internal )
	{
		return SNDLVL_NORM;
	}

	return (soundlevel_t)(int)internal->GetSoundLevel().Random();
}

KeyValues * CSoundEmitterSystemBase::GetOperatorKVByHandle( HSOUNDSCRIPTHASH& handle )
{
	if ( handle == SOUNDEMITTER_INVALID_HASH )
	{
		return nullptr;
	}
	int index = GetSoundIndexForHash( handle );

	CSoundParametersInternal *internal = InternalGetParametersForSound( index );
	if ( !internal )
	{
		return nullptr;
	}

	return internal->GetOperatorsKV();
}

// Called from both client and server (single player) or just one (server only in dedicated server and client only if connected to a remote server)
// Called by LevelInitPreEntity to override sound scripts for the mod with level specific overrides based on custom mapnames, etc.
void CSoundEmitterSystemBase::AddSoundOverrides( char const *scriptfile )
{
	FileNameHandle_t handle = g_pFullFileSystem->FindOrAddFileName( scriptfile );
	if ( ks::find_index( m_OverrideFiles, handle ) != -1 )
		return;

	m_OverrideFiles.push_back( handle );
	// These are overrides and assume bShoudPreload and bShouldAutocache are false
	AddSoundsFromFile( scriptfile, false, false, true );
}

// Called by either client or server in LevelShutdown to clear out custom overrides
void CSoundEmitterSystemBase::ClearSoundOverrides()
{
	for ( int i = 0; i < std::ssize( m_SavedOverrides ); ++i )
	{
		CSoundEntry *entry = m_SavedOverrides[ i ];
		HSOUNDSCRIPTHASH hash = HashSoundName( entry->m_Name.data() );
		int idx = GetSoundIndexForHash( hash );
		if ( IsValidIndex( idx ) )
		{
			delete m_Sounds[ idx ];
			m_Sounds[ idx ] = m_SavedOverrides[i];
		}
	}

	m_SavedOverrides.clear(); m_SavedOverrides.shrink_to_fit();
	m_OverrideFiles.clear(); m_OverrideFiles.shrink_to_fit();
}

void CSoundEmitterSystemBase::AddHash( char const *pchSoundName, int nIndex )
{
	contract_assert( nIndex >= 0 && nIndex < std::ssize( m_Sounds ) );
	CSoundEntry *entry = m_Sounds[ nIndex ];
	HSOUNDSCRIPTHASH hash = HashSoundName( pchSoundName );
	
	// Check for collisions
	auto slot = m_HashToSoundEntry.find( hash );
	if ( slot != m_HashToSoundEntry.end() )
	{
		Error( "Sound name hash collision!  '%s' collides with '%s' %i!", pchSoundName, slot->second.pEntry->m_Name.data(), hash );
		return;
	}

	soundEntryHash_t soundEntryHash =
	{
		nIndex,
		entry
	};
	m_HashToSoundEntry.emplace( hash, soundEntryHash );
}

void CSoundEmitterSystemBase::RemoveHash( char const *pchSoundName )
{
	m_HashToSoundEntry.erase( HashSoundName( pchSoundName ) );
}

char const *CSoundEmitterSystemBase::GetSoundNameForHash( HSOUNDSCRIPTHASH hash ) const
{
	auto slot = m_HashToSoundEntry.find( hash );
	if ( slot == m_HashToSoundEntry.end() )
		return nullptr;
	CSoundEntry *entry = slot->second.pEntry;
	return entry->m_Name.data();
}

int CSoundEmitterSystemBase::GetSoundIndexForHash( HSOUNDSCRIPTHASH hash ) const
{
	auto slot = m_HashToSoundEntry.find( hash );
	if ( slot == m_HashToSoundEntry.end() )
		return -1;
	return slot->second.soundIndex;
}

constexpr auto SOUNDEMITTER_MURMURHASH_SEED = ( ( 'D' << 24 ) | ( 'O' << 16 ) | ( 'T' << 8 ) | 'A' );

HSOUNDSCRIPTHASH CSoundEmitterSystemBase::HashSoundName( char const *pchSndName ) const
{
	HSOUNDSCRIPTHASH hash = MurmurHash2LowerCase( pchSndName, SOUNDEMITTER_MURMURHASH_SEED );
	return hash;
}

bool CSoundEmitterSystemBase::IsValidHash( HSOUNDSCRIPTHASH hash ) const
{
	return m_HashToSoundEntry.contains( hash );
}

void CSoundEmitterSystemBase::DescribeSound( char const *soundname )
{
	HSOUNDSCRIPTHASH hash = HashSoundName( soundname );
	int index = GetSoundIndexForHash( hash );
	if ( index == -1 )
	{
		Msg( "SoundEmitterSystemBase::DescribeSound:  No such sound %s\n", soundname );
		return;
	}

	CSoundParametersInternal *p = InternalGetParametersForSound( index );
	if ( !p )
	{
		Msg( "SoundEmitterSystemBase::DescribeSound:  No such sound %s\n", soundname );
		return;
	}

	Msg( "\"%s\"\n{\n", GetSoundName( index ) );

	Msg( "\t\"channel\"\t\t\"%s\"\n", p->ChannelToString() );
	Msg( "\t\"volume\"\t\t\"%s\"\n", p->VolumeToString() );
	Msg( "\t\"pitch\"\t\t\t\"%s\"\n", p->PitchToString() );
	Msg( "\n" );
	Msg( "\t\"soundlevel\"\t\"%s\"\n", p->SoundLevelToString() );

	if ( p->OnlyPlayToOwner() )
	{
		Msg( "\t\"play_to_owner_only\"\t\"1\"\n" );
	}

	if ( p->GetDelayMsec() != 0 )
	{
		Msg( "\t\"delay_msec\"\t\"%i\"\n", p->GetDelayMsec() );
	}

	int totalCount = 0;

	int waveCount = p->NumSoundNames();
	int convertedCount = p->NumConvertedNames();

	totalCount = ( waveCount - 2 * convertedCount ) + convertedCount;

	if  ( totalCount > 0 )
	{
		Msg( "\n" );

		if ( waveCount == 1 )
		{
			contract_assert( p->GetSoundNames()[ 0 ].gender == GENDER_NONE );
			Msg( "\t\"wave\"\t\t\t\"%s\"\n", GetWaveName( p->GetSoundNames()[ 0 ].symbol ) );
		}
		else if ( convertedCount == 1 )
		{
			contract_assert( p->GetConvertedNames()[ 0 ].gender == GENDER_NONE );
			Msg( "\t\"wave\"\t\t\t\"%s\"\n", GetWaveName( p->GetConvertedNames()[ 0 ].symbol ) );
		}
		else
		{
			Msg( "\t\"rndwave\"\n" );
			Msg( "\t{\n" );

			int wave;
			for ( wave = 0; wave < waveCount; wave++ )
			{
				// Skip macro-expanded names
				if ( p->GetSoundNames()[ wave ].gender != GENDER_NONE )
					continue;

				Msg( "\t\t\"wave\"\t\"%s\"\n", GetWaveName( p->GetSoundNames()[ wave ].symbol ) );
			}
			for ( wave = 0; wave < convertedCount; wave++ )
			{
				Msg( "\t\t\"wave\"\t\"%s\"\n", GetWaveName( p->GetConvertedNames()[ wave ].symbol ) );
			}

			Msg( "\t}\n" );
		}
	}

	Msg( "}\n" );
}

void CSoundEmitterSystemBase::Flush()
{
	ShutdownSounds();
	LoadGameSoundManifest();
}

CSoundEmitterSystemBase g_SoundEmitterSystemBase;
static InterfaceReg s_Reg_CSoundEmitterSystemBase_ISoundEmitterSystemBase( []() -> void * { return static_cast<ISoundEmitterSystemBase *>( &g_SoundEmitterSystemBase ); }, SOUNDEMITTERSYSTEM_INTERFACE_VERSION );;


//-----------------------------------------------------------------------------
// The ISoundEmitterSystemBase members forward to CSoundEmitterSystemBase
//-----------------------------------------------------------------------------
#include "facade/isoundemittersystembase.inl"
