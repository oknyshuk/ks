#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "SoundEmitterSystem/isoundemittersystembase.h"
import std;
#include "soundflags.h"
#include "tier2/interval.h"
#include "tier2/tier2.h"

soundlevel_t TextToSoundLevel( const char *key );

struct CSoundEntry : public ks::pooled< CSoundEntry >
{
	std::string_view			m_Name;		// in ks::global_symbols()
	CSoundParametersInternal	m_SoundParams;
	uint16						m_nScriptFileIndex;
	bool						m_bRemoved : 1;
	bool						m_bIsOverride : 1;

	class CSoundEntryLess
	{
	public:
		bool Less( CSoundEntry * const & lhs, CSoundEntry *const & rhs, void *pCtx )
		{
			return ( V_stricmp( lhs->m_Name.data(), rhs->m_Name.data() ) < 0 ) ? true : false;
		}
	};

	private:
		static ks::fixed_pool s_Allocator;
		friend class ks::pooled< CSoundEntry >;
};

//-----------------------------------------------------------------------------
// Purpose: Base class for sound emitter system handling (can be used by tools)
//-----------------------------------------------------------------------------
class CSoundEmitterSystemBase : public ISoundEmitterSystemBase
{

public:
	CSoundEmitterSystemBase();
	~CSoundEmitterSystemBase() { }

	// App system protocol (a tier 2 system)
	bool Connect( CreateInterfaceFn factory );
	InitReturnVal_t Init();
	void Shutdown();
	void Disconnect();
	void *QueryInterface( const char *pInterfaceName );

public:
	int	GetSoundIndex( const char *pName ) const;
	bool IsValidIndex( int index );
	int GetSoundCount( void );

	const char *GetSoundName( int index );
	bool GetParametersForSound( const char *soundname, CSoundParameters& params, gender_t gender, bool isbeingemitted = false );
	const char *GetWaveName( WaveNameId_t sym );

	WaveNameId_t AddWaveName( const char *name );

	soundlevel_t LookupSoundLevel( const char *soundname );
	const char *GetWavFileForSound( const char *soundname, char const *actormodel );
	const char *GetWavFileForSound( const char *soundname, gender_t gender );

	int		CheckForMissingWavFiles( bool verbose );
	const char *GetSourceFileForSound( int index ) const;
	// Iteration methods
	int		First() const;
	int		Next( int i ) const;
	int		InvalidIndex() const;

	CSoundParametersInternal *InternalGetParametersForSound( int index );

	// The host application is responsible for dealing with dirty sound scripts, etc.
	bool		AddSound( const char *soundname, const char *scriptfile, const CSoundParametersInternal& params );
	void		RemoveSound( const char *soundname );

	void		MoveSound( const char *soundname, const char *newscript );

	void		RenameSound( const char *soundname, const char *newname );
	void		UpdateSoundParameters( const char *soundname, const CSoundParametersInternal& params );

	int			GetNumSoundScripts() const;
	char const	*GetSoundScriptName( int index ) const;
	bool		IsSoundScriptDirty( int index ) const;
	int			FindSoundScript( const char *name ) const;

	void		SaveChangesToSoundScript( int scriptindex );

	void		ExpandSoundNameMacros( CSoundParametersInternal& params, char const *wavename );
	gender_t	GetActorGender( char const *actormodel );
	void		GenderExpandString( char const *actormodel, char const *in, char *out, int maxlen );
	void		GenderExpandString( gender_t gender, char const *in, char *out, int maxlen );
	bool		IsUsingGenderToken( char const *soundname );
	unsigned int GetManifestFileTimeChecksum();

	bool			GetParametersForSoundEx( const char *soundname, HSOUNDSCRIPTHASH& handle, CSoundParameters& params, gender_t gender, bool isbeingemitted = false );
	soundlevel_t	LookupSoundLevelByHandle( char const *soundname, HSOUNDSCRIPTHASH& handle );
	KeyValues		*GetOperatorKVByHandle( HSOUNDSCRIPTHASH& handle );

	char const		*GetSoundNameForHash( unsigned int hash ) const; // Returns NULL if hash not found!!!
	int 			GetSoundIndexForHash( unsigned int hash ) const;
	unsigned int	HashSoundName( char const *pchSndName ) const;
	bool			IsValidHash( unsigned int hash ) const;

	// Called from both client and server (single player) or just one (server only in dedicated server and client only if connected to a remote server)
	// Called by LevelInitPreEntity to override sound scripts for the mod with level specific overrides based on custom mapnames, etc.
	void			AddSoundOverrides( char const *scriptfile );

	// Called by either client or server in LevelShutdown to clear out custom overrides
	void			ClearSoundOverrides();

	void			DescribeSound( char const *soundname );
	void			Flush();

	void			AddSoundsFromFile( const char *filename, bool bPreload, bool bAutoCache, bool bIsOverride = false );

private:
	bool InitSoundInternalParameters( const char *soundname, KeyValues *kv, CSoundParametersInternal& params );
	bool LoadGameSoundManifest();
	void ShutdownSounds();
	void LoadGlobalActors();

	float	TranslateAttenuation( const char *key );
	soundlevel_t	TranslateSoundLevel( const char *key );
	int TranslateChannel( const char *name );

	int		FindBestSoundForGender( SoundFile *pSoundnames, int c, gender_t gender, int &nRandomSeed );
	void	EnsureAvailableSlotsForGender( SoundFile *pSoundnames, int c, gender_t gender );
	void	AddSoundName( CSoundParametersInternal& params, char const *wavename, gender_t gender );

	void	AddHash( char const *pchSoundName, int nIndex );
	void	RemoveHash( char const *pchSoundName );

	ks::idict< gender_t >						m_ActorGenders;
	std::vector< CSoundEntry * >	m_Sounds;

    std::vector< CSoundEntry * >			m_SavedOverrides; 
	std::vector< FileNameHandle_t >				m_OverrideFiles;

	struct CSoundScriptFile
	{
		FileNameHandle_t hFilename;
		bool		dirty;
	};

	std::vector< CSoundScriptFile >	m_SoundKeyValues;
	int					m_nInitCount;
	unsigned int		m_uManifestPlusScriptChecksum;

	// Wave names by WaveNameId_t (dense, in insertion order; case-sensitive), pointing at the keys of m_WaveIds.
	struct CWaveNameHash
	{
		using is_transparent = void;
		std::size_t operator()( std::string_view s ) const { return std::hash< std::string_view >()( s ); }
	};
	std::vector< const char * >	m_Waves;
	std::unordered_map< std::string, WaveNameId_t, CWaveNameHash, std::equal_to<> > m_WaveIds;
	// This is a reverse mapping from crc of the soundname to the sound entry
	struct soundEntryHash_t
	{
		int soundIndex;
		CSoundEntry *pEntry;
	};

	std::map< HSOUNDSCRIPTHASH, soundEntryHash_t >	m_HashToSoundEntry;

};
